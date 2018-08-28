// ------------------------------------------------------------------------
//
//              (C) COPYRIGHT 2014 - 2015 SYNOPSYS, INC.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// ------------------------------------------------------------------------
//
// Project:
//
// ESM Host Library
//
// Description:
//
// ESM Host Library Driver: Linux kernel module
//
// ------------------------------------------------------------------------

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/of_device.h>
#include "include/host_lib_driver_linux_if.h"
#include "../../hdmi_v2_0/include/hdmi_includes.h"

#define MAX_ESM_DEVICES 16
#define	USE_RESERVED_MEMORY

//#define OPTEE_BASE_HDCP

#define HDCP_HOST_DRV_VERSION "4.4_1.0.2"

static bool randomize_mem = false;
module_param(randomize_mem, bool, 0);
MODULE_PARM_DESC(noverify, "Wipe memory allocations on startup (for debug)");

typedef struct {
	int allocated, initialized;
	int code_loaded;

	int code_is_phys_mem;
	dma_addr_t code_base;
	uint32_t code_size;
	uint8_t *code;

	int data_is_phys_mem;
	dma_addr_t data_base;
	uint32_t data_size;
	uint8_t *data;

	struct resource *hpi_resource;
	uint8_t __iomem *hpi;

	struct hdmi_tx_dev *dev;
	hdcpParams_t *params;
} esm_device;

static esm_device esm_devices[MAX_ESM_DEVICES];
static struct device *g_esm_dev;

#ifndef OPTEE_BASE_HDCP
#ifdef	USE_RESERVED_MEMORY
struct esm_rev_mem {
	dma_addr_t code_base;
	uint32_t code_size;
	uint8_t *code;
	dma_addr_t data_base;
	uint32_t data_size;
	uint8_t *data;
};

static struct esm_rev_mem g_rev_mem;
#endif
#endif

/* ESM_IOC_MEMINFO implementation */
static long get_meminfo(esm_device *esm, void __user *arg)
{
	struct esm_ioc_meminfo info = {
		.hpi_base = esm->hpi_resource->start,
		.code_base = esm->code_base,
		.code_size = esm->code_size,
		.data_base = esm->data_base,
		.data_size = esm->data_size,
	};

	if (copy_to_user(arg, &info, sizeof info) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_LOAD_CODE implementation */
static long load_code(esm_device *esm, struct esm_ioc_code __user *arg)
{
	struct esm_ioc_code head;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (head.len > esm->code_size)
		return -ENOSPC;
#if 0 // remove by yonghee, we need to load/unload according hdmi status
	if (esm->code_loaded)
		return -EBUSY;
#endif
	if (copy_from_user(esm->code, &arg->data, head.len) != 0)
		return -EFAULT;

	esm->code_loaded = 1;
	return 0;
}

/* ESM_IOC_WRITE_DATA implementation */
static long write_data(esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user(esm->data + head.offset, &arg->data, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_READ_DATA implementation */
static long read_data(esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, esm->data + head.offset, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_MEMSET_DATA implementation */
static long set_data(esm_device *esm, void __user *arg)
{
	union {
		struct esm_ioc_data data;
		unsigned char buf[sizeof(struct esm_ioc_data) + 1];
	} u;

	if (copy_from_user(&u.data, arg, sizeof u.buf) != 0)
		return -EFAULT;

	if (esm->data_size < u.data.len)
		return -ENOSPC;
	if (esm->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(esm->data + u.data.offset, u.data.data[0], u.data.len);
	return 0;
}

/* ESM_IOC_READ_HPI implementation */
static long hpi_read(esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(esm->hpi_resource))
		return -EINVAL;

	reg.value = ioread32(esm->hpi + reg.offset);

	if (copy_to_user(arg, &reg, sizeof reg) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_WRITE_HPI implementation */
static long hpi_write(esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(esm->hpi_resource))
		return -EINVAL;

	iowrite32(reg.value, esm->hpi + reg.offset);
	return 0;
}

/* HDCP1_INIT implementation */
static long hdcp1Initialize(esm_device *esm, void __user *arg)
{
	struct dwc_hdmi_hdcp_data hdcpData;

	if (copy_from_user(&hdcpData, arg, sizeof hdcpData) != 0)
		return -EFAULT;

	esm->params = &hdcpData.hdcpParam;
	esm->dev = dwc_hdmi_api_get_dev();

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 1;
	hdcp1p4Configure(esm->dev, esm->params);
	hdcp1p4SwitchSet(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP1_START implementation */
static long hdcp1Start(esm_device *esm, void __user *arg)
{
	struct dwc_hdmi_hdcp_data hdcpData;

	if (copy_from_user(&hdcpData, arg, sizeof hdcpData) != 0)
		return -EFAULT;

	if (!esm->dev)
		return -ENOSPC;

	esm->params = &hdcpData.hdcpParam;
	mutex_lock(&esm->dev->mutex);
	mc_hdcp_clock_enable(esm->dev, 0);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	hdcp1p4Start(esm->dev, esm->params);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP1_STOP implementation */
static long hdcp1Stop(esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
	hdcp1p4Stop(esm->dev);
	mc_hdcp_clock_enable(esm->dev, 1);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP_GET_STATUS implementation */
static long hdcpGetStatus(esm_device *esm, void __user *arg)
{
	struct hdcp_ioc_data data;
	int hdcpStatus;

	if (copy_from_user(&data, arg, sizeof data) != 0)
		return -EFAULT;

	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	hdcpStatus = hdcpAuthGetStatus(esm->dev);
	mutex_unlock(&esm->dev->mutex);
	data.status = hdcpStatus;
	if (copy_to_user(arg, &data, sizeof data) != 0)
		return -EFAULT;

	return 0;
}

/* HDCP2_INIT implementation */
static long hdcp2Initialize(esm_device *esm, void __user *arg)
{
	esm->dev = dwc_hdmi_api_get_dev();

	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 2;
	hdcp2p2SwitchSet(esm->dev);
	mc_hdcp_clock_enable(esm->dev, 0);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	_HDCP22RegMute(esm->dev, 0x00);
	_HDCP22RegMask(esm->dev, 0x00);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP2_STOP implementation */
static long hdcp2Stop(esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
	mc_hdcp_clock_enable(esm->dev, 1);
	dwc_hdmi_set_hdcp_keepout(esm->dev);
	hdcp2p2Stop(esm->dev);
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

static esm_device *alloc_esm_slot(const struct esm_ioc_meminfo *info)
{
	int i;

	/* Check if we have a matching device (same HPI base) */
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		esm_device *slot = &esm_devices[i];
		#ifndef OPTEE_BASE_HDCP
		if (slot->allocated && info->hpi_base == slot->hpi_resource->start)
			return slot;
		#else
		if (slot->allocated)
			return slot;
		#endif
	}

	/* Find unused slot */
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		esm_device *slot = &esm_devices[i];
		if (!slot->allocated) {
			slot->allocated = 1;
			return slot;
		}
	}

	return NULL;
}

#ifndef OPTEE_BASE_HDCP
static void free_dma_areas(esm_device *esm)
{
	if (!esm->code_is_phys_mem && esm->code) {
		dma_free_coherent(NULL, esm->code_size, esm->code, esm->code_base);
		esm->code = NULL;
	}

	if (!esm->data_is_phys_mem && esm->data) {
		dma_free_coherent(NULL, esm->data_size, esm->data, esm->data_base);
		esm->data = NULL;
	}
}

static int alloc_dma_areas(esm_device *esm, const struct esm_ioc_meminfo *info)
{
	esm->code_size = info->code_size;
	esm->code_is_phys_mem = (info->code_base != 0);

	if (esm->code_is_phys_mem) {
		/* TODO: support highmem */
		esm->code_base = info->code_base;
		esm->code = phys_to_virt(esm->code_base);
	} else {
#ifdef	USE_RESERVED_MEMORY
		if(esm->code_size > g_rev_mem.code_size) {
			printk(KERN_ERR " %s - no code memory : %d : %d !!!\n", __func__, esm->code_size, g_rev_mem.code_size);
			return -ENOMEM;
		}
		esm->code = g_rev_mem.code;
		esm->code_base = g_rev_mem.code_base;
#else
		esm->code = dma_alloc_coherent(NULL, esm->code_size, &esm->code_base, GFP_KERNEL);
		if (!esm->code) {
			return -ENOMEM;
		}
#endif
	}

	esm->data_size = info->data_size;
	esm->data_is_phys_mem = (info->data_base != 0);

	if (esm->data_is_phys_mem) {
		esm->data_base = info->data_base;
		esm->data = phys_to_virt(esm->data_base);
	} else {
#ifdef	USE_RESERVED_MEMORY
		if(esm->data_size > g_rev_mem.data_size) {
			printk(KERN_ERR " %s - no data memory : %d : %d !!!\n", __func__, esm->data_size, g_rev_mem.data_size);
			return -ENOMEM;
		}
		esm->data = g_rev_mem.data;
		esm->data_base = g_rev_mem.data_base;
#else
		esm->data = dma_alloc_coherent(NULL, esm->data_size, &esm->data_base, GFP_KERNEL);
		if (!esm->data) {
			free_dma_areas(esm);
			return -ENOMEM;
		}
#endif
	}

	if (randomize_mem) {
		prandom_bytes(esm->code, esm->code_size);
		prandom_bytes(esm->data, esm->data_size);
	}

	return 0;
}
#endif

/* ESM_IOC_INIT implementation */
static long init(struct file *f, void __user *arg)
{
#ifndef OPTEE_BASE_HDCP
	struct resource *hpi_mem;
	int rc;
#endif
	struct esm_ioc_meminfo info;
	esm_device *esm;

	if (copy_from_user(&info, arg, sizeof info) != 0)
		return -EFAULT;

	esm = alloc_esm_slot(&info);
	if (!esm)
		return -EMFILE;

	if (!esm->initialized) {
#ifndef OPTEE_BASE_HDCP
		rc = alloc_dma_areas(esm, &info);
		if (rc < 0)
			goto err_free;

		hpi_mem = request_mem_region(info.hpi_base, 128, "esm-hpi");
		if (!hpi_mem) {
			rc = -EADDRNOTAVAIL;
			goto err_free;
		}

		esm->hpi = ioremap_nocache(hpi_mem->start, resource_size(hpi_mem));
		if (!esm->hpi) {
			rc = -ENOMEM;
			goto err_release_region;
		}
		esm->hpi_resource = hpi_mem;
#endif
		esm->initialized = 1;
	}

	f->private_data = esm;

	return 0;

#ifndef OPTEE_BASE_HDCP
err_release_region:
	release_resource(hpi_mem);
err_free:
	free_dma_areas(esm);
	esm->initialized = 0;
	esm->allocated = 0;

	return rc;
#endif
}

static void free_esm_slot(esm_device *slot)
{
	if (!slot->allocated)
		return;

#ifndef OPTEE_BASE_HDCP
	if (slot->initialized) {
		iounmap(slot->hpi);
		release_resource(slot->hpi_resource);
		free_dma_areas(slot);
	}
#endif

	slot->initialized = 0;
	slot->allocated = 0;
}

static long hld_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	esm_device *esm = f->private_data;
	void __user *data = (void __user *)arg;

	if (cmd == ESM_IOC_INIT) {
		return init(f, data);
	} else if (!esm) {
		return -EAGAIN;
	}

	switch (cmd) {
	case ESM_IOC_INIT:
		return init(f, data);
	case ESM_IOC_MEMINFO:
		return get_meminfo(esm, data);
	case ESM_IOC_READ_HPI:
		return hpi_read(esm, data);
	case ESM_IOC_WRITE_HPI:
		return hpi_write(esm, data);
	case ESM_IOC_LOAD_CODE:
		return load_code(esm, data);
	case ESM_IOC_WRITE_DATA:
		return write_data(esm, data);
	case ESM_IOC_READ_DATA:
		return read_data(esm, data);
	case ESM_IOC_MEMSET_DATA:
		return set_data(esm, data);
	case HDCP1_INIT:
		return hdcp1Initialize(esm, data);
	case HDCP1_START:
		return hdcp1Start(esm, data);
	case HDCP1_STOP:
		return hdcp1Stop(esm, data);
	case HDCP_GET_STATUS:
		return hdcpGetStatus(esm, data);
	case HDCP2_INIT:
		return hdcp2Initialize(esm, data);
	case HDCP2_STOP:
		return hdcp2Stop(esm, data);
	}

	return -ENOTTY;
}

static const struct file_operations hld_file_operations = {
	.unlocked_ioctl = hld_ioctl,
	.owner = THIS_MODULE,
};

static struct miscdevice hld_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "esm",
	.fops = &hld_file_operations,
};

static int __init hld_init(void)
{
	int ret = 0;
	printk(KERN_INFO " HDCP Host Drv version: %s\n", HDCP_HOST_DRV_VERSION);
	ret = misc_register(&hld_device);
	if (unlikely(ret)) {
		printk(KERN_ERR " %s - failed to register !!!\n", __func__);
		return ret;
	}
	g_esm_dev = hld_device.this_device;
	of_dma_configure(g_esm_dev, NULL);
	_dev_info(g_esm_dev, "registered.\n");
#ifndef OPTEE_BASE_HDCP
#ifdef USE_RESERVED_MEMORY
	g_rev_mem.code_size = 0x30000;
	g_rev_mem.data_size = 0x20000;
	g_rev_mem.code =
		dma_alloc_coherent(g_esm_dev, g_rev_mem.code_size, &g_rev_mem.code_base, GFP_KERNEL);
	if (!g_rev_mem.code) {
		printk(
			KERN_ERR " %s[%d] - no code memory : %d !!!\n", __func__, __LINE__,
			g_rev_mem.code_size);
		return -ENOMEM;
	}

	g_rev_mem.data =
		dma_alloc_coherent(g_esm_dev, g_rev_mem.data_size, &g_rev_mem.data_base, GFP_KERNEL);
	if (!g_rev_mem.data) {
		printk(
			KERN_ERR " %s[%d] - no data memory : %d !!!\n", __func__, __LINE__,
			g_rev_mem.data_size);
		return -ENOMEM;
	}
#endif
#endif
	return ret;
}
module_init(hld_init);

static void __exit hld_exit(void)
{
	int i;
	printk(KERN_INFO " %s\n", __func__);
#ifndef OPTEE_BASE_HDCP
#ifdef	USE_RESERVED_MEMORY
	if (!g_rev_mem.code) {
		dma_free_coherent(g_esm_dev, g_rev_mem.code_size, g_rev_mem.code, g_rev_mem.code_base);
		g_rev_mem.code = NULL;
	}
	if (!g_rev_mem.data) {
		dma_free_coherent(g_esm_dev, g_rev_mem.data_size, g_rev_mem.data, g_rev_mem.data_base);
		g_rev_mem.data = NULL;
	}
#endif
#endif
	misc_deregister(&hld_device);

	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		free_esm_slot(&esm_devices[i]);
	}
}
module_exit(hld_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("ESM Linux Host Library Driver");
