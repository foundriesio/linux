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

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#define MAX_ESM_DEVICES 16
#define	USE_RESERVED_MEMORY

#if defined(CONFIG_ARCH_TCC899X)
#define OPTEE_BASE_HDCP
#endif

#define HDCP_HOST_DRV_VERSION "4.14_1.0.4"

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

	/** Device node */
	struct device *parent_dev;

	/** Device Tree Information */
	char *device_name;

	/** Misc Device */
	struct miscdevice *misc;

	/** Device Open Count */
	int open_cs;

	/** Device list **/
	struct list_head devlist;

	int hdcp_suspend;
} esm_device;

#ifndef OPTEE_BASE_HDCP
static esm_device esm_devices[MAX_ESM_DEVICES];
static struct device *g_esm_dev;

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

/**
 * @short List of the devices
 * Linked list that contains the installed devices
 */
static LIST_HEAD(devlist_global);

extern void * alloc_mem(char *info, size_t size, struct mem_alloc *allocated);
extern void free_all_mem(void);

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
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &esm->dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &esm->dev->status)) {
			esm->dev->hdmi_tx_ctrl.hdcp_on = 1;
			hdcp1p4Configure(esm->dev, esm->params);
			hdcp1p4SwitchSet(esm->dev);
		}
	}

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
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &esm->dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &esm->dev->status)) {
			mc_hdcp_clock_enable(esm->dev, 0);
			dwc_hdmi_set_hdcp_keepout(esm->dev);
			hdcp1p4Start(esm->dev, esm->params);
		}
	}
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP1_STOP implementation */
static long hdcp1Stop(esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &esm->dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &esm->dev->status)) {
			esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
			hdcp1p4Stop(esm->dev);
			mc_hdcp_clock_enable(esm->dev, 1);
			dwc_hdmi_set_hdcp_keepout(esm->dev);
		}
	}
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
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &esm->dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &esm->dev->status)) {
			hdcpStatus = hdcpAuthGetStatus(esm->dev);
		}
	}
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
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &esm->dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &esm->dev->status)) {
			esm->dev->hdmi_tx_ctrl.hdcp_on = 2;
			hdcp2p2SwitchSet(esm->dev);
			mc_hdcp_clock_enable(esm->dev, 0);
			dwc_hdmi_set_hdcp_keepout(esm->dev);
			_HDCP22RegMute(esm->dev, 0x00);
			_HDCP22RegMask(esm->dev, 0x00);
		}
	}
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

/* HDCP2_STOP implementation */
static long hdcp2Stop(esm_device *esm, void __user *arg)
{
	if (!esm->dev)
		return -ENOSPC;

	mutex_lock(&esm->dev->mutex);
	if(!test_bit(HDMI_TX_STATUS_SUSPEND_L1, &esm->dev->status)) {
		if(test_bit(HDMI_TX_STATUS_POWER_ON, &esm->dev->status)) {
			pr_info("%s[%d] \n",__func__, __LINE__);
			esm->dev->hdmi_tx_ctrl.hdcp_on = 0;
			mc_hdcp_clock_enable(esm->dev, 1);
			dwc_hdmi_set_hdcp_keepout(esm->dev);
			hdcp2p2Stop(esm->dev);
		}
	}
	mutex_unlock(&esm->dev->mutex);

	return 0;
}

#ifdef CONFIG_PM
/* HDCP2_STOP implementation */
static long hdcpBlank(esm_device *esm, void __user *arg)
{
	struct hdcp_ioc_data data;
	int ret = 0;
	struct device *pdev_hdcp;

	if (!esm->dev)
		return -ENOSPC;

	pdev_hdcp = esm->parent_dev;

	if (copy_from_user(&data, arg, sizeof data) != 0) {
		pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
		return -EFAULT;
	}

	pr_info("%s : blank(mode=%d)\n",__func__, data.status);

	if (pdev_hdcp !=  NULL) {
		switch(data.status)
		{
			case FB_BLANK_POWERDOWN:
			case FB_BLANK_NORMAL:
				pr_info("%s[%d] : blank(mode=%d)\n",__func__, __LINE__, data.status);
				pm_runtime_put_sync(pdev_hdcp);
				break;
			case FB_BLANK_UNBLANK:
				pr_info("%s[%d] : blank(mode=%d)\n",__func__, __LINE__, data.status);
				pm_runtime_get_sync(pdev_hdcp);
				break;
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_VSYNC_SUSPEND:
			default:
				pr_info("%s[%d] : blank(mode=%d)\n",__func__, __LINE__, data.status);
				ret = -EINVAL;
		}
	}

	return ret;

}
#endif

#ifndef OPTEE_BASE_HDCP
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

#ifndef OPTEE_BASE_HDCP
	esm = alloc_esm_slot(&info);
#else
	esm = f->private_data;
#endif
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
	esm->hdcp_suspend = 0;

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

#if defined(CONFIG_PM)
int tcc_hdcp_suspend(struct device *dev)
{
	esm_device *hdcp_dev = (esm_device *)dev_get_drvdata(dev);
	printk("[%s] \n", __func__);

	hdcp_dev->hdcp_suspend = 1;

	printk("[%s]: finish \n", __func__);

	return 0;
}

int tcc_hdcp_resume(struct device *dev)
{
	esm_device *hdcp_dev = (esm_device *)dev_get_drvdata(dev);
	printk("[%s] \n", __func__);

	hdcp_dev->hdcp_suspend = 0;

	printk("[%s]: finish \n", __func__);

	return 0;
}

int tcc_hdcp_runtime_suspend(struct device *dev)
{
	esm_device *hdcp_dev = (esm_device *)dev_get_drvdata(dev);
	printk("[%s] \n", __func__);

	hdcp_dev->hdcp_suspend = 1;

	printk("[%s]: finish \n", __func__);

	return 0;
}

int tcc_hdcp_runtime_resume(struct device *dev)
{
	esm_device *hdcp_dev = (esm_device *)dev_get_drvdata(dev);

	printk("[%s] \n", __func__);

	hdcp_dev->hdcp_suspend = 0;

	printk("[%s]: finish \n", __func__);

	return 0;
}

static const struct dev_pm_ops tcc_hdcp_pm_ops = {
	.suspend = tcc_hdcp_suspend,
	.resume = tcc_hdcp_resume,
	.runtime_suspend = tcc_hdcp_runtime_suspend,
	.runtime_resume = tcc_hdcp_runtime_resume,
};
#endif // CONFIG_PM

static long tcc_hdcp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	esm_device *esm = f->private_data;
	void __user *data = (void __user *)arg;

	if (cmd == ESM_IOC_INIT) {
		return init(f, data);
	} else if (!esm) {
		return -EAGAIN;
	}

	if (esm->hdcp_suspend == 1) {
		printk("[hdcp driver] suspend mode is set. \n");
		return -EBUSY;
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
	case HDCP_IOC_BLANK:
		return hdcpBlank(esm, data);
	}

	return -ENOTTY;
}

static int
tcc_hdcp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = (struct miscdevice *)file->private_data;
	esm_device *dev = dev_get_drvdata(misc->parent);

	file->private_data = dev;


	dev->open_cs++;

	return 0;
}

static int
tcc_hdcp_release(struct inode *inode, struct file *file)
{
	esm_device *dev = (esm_device *)file->private_data;

	dev->open_cs--;

	return 0;
}

static ssize_t
tcc_hdcp_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static ssize_t
tcc_hdcp_write(struct file *file, const char *buf, size_t count, loff_t *f_pos)
{
	return count;
}

static unsigned int tcc_hdcp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	return mask;
}


static const struct file_operations tcc_hdcp_fops =
{
	.owner			= THIS_MODULE,
	.open			= tcc_hdcp_open,
	.release			= tcc_hdcp_release,
	.read			= tcc_hdcp_read,
	.write			= tcc_hdcp_write,
	.unlocked_ioctl	= tcc_hdcp_ioctl,
	.poll				= tcc_hdcp_poll,
};


/**
 * @short misc register routine
 * @param[in] dev pointer to the esm_device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int tcc_hdcp_misc_register(esm_device *dev)
{
	int ret = 0;

	dev->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if(dev->misc == NULL) {
		ret = -1;
	}else {
		dev->misc->minor = MISC_DYNAMIC_MINOR;
		dev->misc->name = "esm";
		dev->misc->fops = &tcc_hdcp_fops;
		dev->misc->parent = dev->parent_dev;
		ret = misc_register(dev->misc);
	}

	if(ret < 0) {
		goto end_process;
	}

	dev_set_drvdata(dev->parent_dev, dev);

end_process:

	return ret;
}

/**
 * @short misc deregister routine
 * @param[in] dev pointer to the esm_device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int tcc_hdcp_misc_deregister(esm_device *dev)
{
	if(dev->misc) {
		misc_deregister(dev->misc);
		kfree(dev->misc);
		dev->misc = 0;
	}

	return 0;
}

static int tcc_hdcp_probe(struct platform_device *pdev)
{
	int ret = 0;
	esm_device *dev = NULL;

	printk("%s:Device registration\n", __func__);
	dev = alloc_mem("HDCP Device", sizeof(esm_device), NULL);
	if(!dev){
		pr_err("%s:Could not allocated hdcp device driver\n", __func__);
		return -ENOMEM;
	}

	// Zero the device
	memset(dev, 0, sizeof(esm_device));

	// Update the device node
	dev->parent_dev = &pdev->dev;

	dev->device_name = "TCC_HDCP";

	printk("%s:Driver's name '%s' \n", __func__, dev->device_name);

	tcc_hdcp_misc_register(dev);

	dev->hdcp_suspend = 0;

	#ifdef CONFIG_PM
	pm_runtime_set_active(dev->parent_dev);
	pm_runtime_enable(dev->parent_dev);
	pm_runtime_get_noresume(dev->parent_dev);  //increase usage_count
	#endif

	// Now that everything is fine, let's add it to device list
	list_add_tail(&dev->devlist, &devlist_global);

	return ret;
}

/**
 * @short Exit routine - Exit point of the driver
 * @param[in] pdev pointer to the platform device structure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
static int tcc_hdcp_remove(struct platform_device *pdev)
{
	esm_device *dev;
	struct list_head *list;

	while(!list_empty(&devlist_global)){
		list = devlist_global.next;
		list_del(list);
		dev = list_entry(list, esm_device , devlist);

		if(dev == NULL) {
			continue;
		}

		#if defined(CONFIG_PM)
		pm_runtime_disable(dev->parent_dev);
		#endif

		tcc_hdcp_misc_deregister(dev);

		free_all_mem();
	}

	return 0;
}

/**
 * @short of_device_id structure
 */
static const struct of_device_id tcc_hdcp[] = {
	{ .compatible =	"telechips,esm" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_hdcp);


/**
 * @short Platform driver structure
 */
static struct platform_driver __refdata tcc_hdcp_pdrv = {
	.remove = tcc_hdcp_remove,
	.probe = tcc_hdcp_probe,
	.driver = {
		.name = "telechips,esm",
		.owner = THIS_MODULE,
		.of_match_table = tcc_hdcp,
		#if defined(CONFIG_PM)
		.pm = &tcc_hdcp_pm_ops,
		#endif
	},
};

static int __init hld_init(void)
{
	printk(KERN_INFO " HDCP Host Drv version: %s\n", HDCP_HOST_DRV_VERSION);

	return platform_driver_register(&tcc_hdcp_pdrv);
}

static void __exit hld_exit(void)
{
	int i;
	printk(KERN_INFO " %s\n", __func__);

	platform_driver_unregister(&tcc_hdcp_pdrv);

#ifndef OPTEE_BASE_HDCP
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		free_esm_slot(&esm_devices[i]);
	}
#endif
}

module_init(hld_init);
module_exit(hld_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("ESM Linux Host Library Driver");
MODULE_VERSION("1.0");
