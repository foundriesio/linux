/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * (C) COPYRIGHT 2014 - 2017 SYNOPSYS, INC.
 * Copyright (C) Telechips Inc.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include <linux/platform_device.h>
#include <linux/of_address.h>


#define HL_DRIVER_ALLOCATE_DYNAMIC_MEM 0xffffffff


//#include "HostLibDriver.h"
//#include "HostLibDriverErrors.h"
#include "host_lib_driver_linux_if.h"

/**
 * \file
 * \ingroup HL_Driver_Kernel
 * \brief Sample Linux Host Library Driver
 * \copydoc HL_Driver_Kernel
 */

/**
 * \defgroup HL_Driver_Linux Sample Linux Host Library Driver
 * \ingroup HL_Driver
 * \brief Sample code for the Linux Host Library Driver.
 * The Linux Host Library Driver is composed of 2 parts:
 * 1. A kernel driver.
 * 2. A file access instance.
 *
 * The kernel driver is the kernel executable code enabling the firmware to
 * execute.
 * It provides the access to the hardware register to interact with the
 * firmware.
 *
 * The file access instance initializes the #hl_driver_t structure for the
 * host library access.  The Host Library references the file access to request
 * the kernel operations.
 */

/**
 * \defgroup HL_Driver_Kernel Sample Linux Kernel Host Library Driver
 * \ingroup HL_Driver_Linux
 * \brief Example code for the Linux Kernel Host Library Driver.
 *
 * The Sample Linux Kernel Driver operates on the linux kernel.
 * To install (requires root access):
 * \code
 insmod bin/linux_hld_module.ko verbose=0
 * \endcode
 *
 * To remove (requires root access):
 * \code
 rmmod linux_hld_module
 * \endcode
 *
 * Example Linux Host Library Code:
 * \code
 */

#define MAX_HL_DEVICES		16

static bool randomize_mem;
module_param(randomize_mem, bool, 0755);
MODULE_PARM_DESC(noverify, "Wipe memory allocations on startup (for debug)");

//
// HL Device
//
struct hl_device {
	int32_t allocated, initialized;
	int32_t code_loaded;

	uint32_t code_is_phys_mem;
	dma_addr_t code_base;
	uint32_t code_size;
	uint8_t *code;
	uint32_t data_is_phys_mem;
	dma_addr_t data_base;
	uint32_t data_size;
	uint8_t *data;

	struct resource *hpi_resource;
	uint8_t __iomem *hpi;
};

static struct hl_device hl_devices[MAX_HL_DEVICES];

/* HL_DRV_IOC_MEMINFO implementation */
static long get_meminfo(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_meminfo info;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->hpi_resource == 0)) {
		return -EFAULT;
	}

	info.hpi_base  = hl_dev->hpi_resource->start;
	info.code_base = hl_dev->code_base;
	info.code_size = hl_dev->code_size;
	info.data_base = hl_dev->data_base;
	info.data_size = hl_dev->data_size;

	if (copy_to_user(arg, &info, sizeof(info)) != 0) {
		return -EFAULT;
	}

	return 0;
}

/* HL_DRV_IOC_LOAD_CODE implementation */
static long load_code(struct hl_device *hl_dev,
		      struct hl_drv_ioc_code __user *arg)
{
	struct hl_drv_ioc_code head;

	if ((hl_dev == 0) || (arg == 0) ||
	    (hl_dev->code == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (head.len > hl_dev->code_size)
		return -ENOSPC;

	if (hl_dev->code_loaded)
		return -EBUSY;

	if (randomize_mem) {
		prandom_bytes(hl_dev->code, hl_dev->code_size);
		prandom_bytes(hl_dev->data, hl_dev->data_size);
	}

	if (copy_from_user(hl_dev->code, &arg->data, head.len) != 0)
		return -EFAULT;

	hl_dev->code_loaded = 1;
	return 0;
}

/* HL_DRV_IOC_WRITE_DATA implementation */
static long write_data(struct hl_device *hl_dev,
	    struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;
	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user(hl_dev->data + head.offset, &arg->data, head.len)
			 != 0)
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_READ_DATA implementation */
static long read_data(struct hl_device *hl_dev,
		      struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;
	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, hl_dev->data + head.offset, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_MEMSET_DATA implementation */
static long set_data(struct hl_device *hl_dev, void __user *arg)
{
	union {
		struct hl_drv_ioc_data data;
		unsigned char buf[sizeof(struct hl_drv_ioc_data) + 1];
	} u;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&u.data, arg, sizeof(u.buf)) != 0)
		return -EFAULT;

	if (hl_dev->data_size < u.data.len)
		return -ENOSPC;
	if (hl_dev->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(hl_dev->data + u.data.offset, u.data.data[0], u.data.len);
	return 0;
}

/* HL_DRV_IOC_READ_HPI implementation */
static long hpi_read(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->hpi_resource == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) ||
	    reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	reg.value = ioread32(hl_dev->hpi + reg.offset);

	if (copy_to_user(arg, &reg, sizeof(reg)) != 0)
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_WRITE_HPI implementation */
static long hpi_write(struct hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if ((hl_dev == 0) || (arg == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) ||
	    reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	iowrite32(reg.value, hl_dev->hpi + reg.offset);

	#ifdef TROOT_GRIFFIN
	//  If Kill command
	//  (HL_GET_CMD_EVENT(krequest.data) == TROOT_CMD_SYSTEM_ON_EXIT_REQ))
	//
	if ((reg.offset == 0x38) && ((reg.value & 0x000000ff) == 0x08)) {
		hl_dev->code_loaded = 0;
	}
	#endif
	return 0;
}

static struct hl_device *alloc_hl_dev_slot(
				const struct hl_drv_ioc_meminfo *info)
{
	int32_t i;

	if (info == 0) {
		return 0;
	}

	/* Check if we have a matching device (same HPI base) */
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		struct hl_device *slot = &hl_devices[i];

		if (slot->allocated &&
		    (info->hpi_base == slot->hpi_resource->start))
			return slot;
	}

	/* Find unused slot */
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		struct hl_device *slot = &hl_devices[i];

		if (!slot->allocated) {
			slot->allocated = 1;
			return slot;
		}
	}

	return 0;
}

static void free_dma_areas(struct hl_device *hl_dev)
{
	if (hl_dev == 0) {
		return;
	}

	if (!hl_dev->code_is_phys_mem && hl_dev->code) {
		dma_free_coherent(0, hl_dev->code_size, hl_dev->code,
				  hl_dev->code_base);
		hl_dev->code = 0;
	}

	if (!hl_dev->data_is_phys_mem && hl_dev->data) {
		dma_free_coherent(0, hl_dev->data_size, hl_dev->data,
				  hl_dev->data_base);
		hl_dev->data = 0;
	}
}

static int alloc_dma_areas(struct hl_device *hl_dev,
			   const struct hl_drv_ioc_meminfo *info)
{
	if ((hl_dev == 0) || (info == 0)) {
		return -EFAULT;
	}

	hl_dev->code_size = info->code_size;
	hl_dev->code_is_phys_mem =
			(info->code_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if (hl_dev->code_is_phys_mem && (hl_dev->code == 0)) {
		/* TODO: support highmem */
		hl_dev->code_base = info->code_base;
		hl_dev->code = phys_to_virt(hl_dev->code_base);
	} else {
		hl_dev->code = dma_alloc_coherent(0, hl_dev->code_size,
						&hl_dev->code_base, GFP_KERNEL);
		if (!hl_dev->code) {
			return -ENOMEM;
		}
	}

	hl_dev->data_size = info->data_size;
	hl_dev->data_is_phys_mem =
			(info->data_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if (hl_dev->data_is_phys_mem && (hl_dev->data == 0)) {
		hl_dev->data_base = info->data_base;
		hl_dev->data = phys_to_virt(hl_dev->data_base);
	} else {
		hl_dev->data = dma_alloc_coherent(0, hl_dev->data_size,
						&hl_dev->data_base, GFP_KERNEL);
		if (!hl_dev->data) {
			free_dma_areas(hl_dev);
			return -ENOMEM;
		}
	}

	return 0;
}

/* HL_DRV_IOC_INIT implementation */
static long init(struct file *f, void __user *arg)
{
	struct resource *hpi_mem;
	struct hl_drv_ioc_meminfo info;
	struct hl_device *hl_dev;
	int rc;

	if ((f == 0) || (arg == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&info, arg, sizeof(info)) != 0)
		return -EFAULT;

	hl_dev = alloc_hl_dev_slot(&info);
	if (!hl_dev)
		return -EMFILE;

	if (!hl_dev->initialized) {
		rc = alloc_dma_areas(hl_dev, &info);
		if (rc < 0)
			goto err_free;

		hpi_mem = request_mem_region(info.hpi_base, 128, "hl_dev-hpi");
		if (!hpi_mem) {
			rc = -EADDRNOTAVAIL;
			goto err_free;
		}

		hl_dev->hpi = ioremap_nocache(hpi_mem->start,
					      resource_size(hpi_mem));
		if (!hl_dev->hpi) {
			rc = -ENOMEM;
			goto err_release_region;
		}
		hl_dev->hpi_resource = hpi_mem;
		hl_dev->initialized = 1;
	}

	f->private_data = hl_dev;
	return 0;

err_release_region:
	release_resource(hpi_mem);
err_free:
	free_dma_areas(hl_dev);
	hl_dev->initialized   = 0;
	hl_dev->allocated     = 0;
	hl_dev->hpi_resource  = 0;
	hl_dev->hpi           = 0;

	return rc;
}

static void free_hl_dev_slot(struct hl_device *slot)
{
	if (slot == 0) {
		return;
	}

	if (!slot->allocated)
		return;

	if (slot->initialized) {
		if (slot->hpi) {
			iounmap(slot->hpi);
			slot->hpi = 0;
		}

		if (slot->hpi_resource) {
			release_mem_region(slot->hpi_resource->start, 128);
			slot->hpi_resource = 0;
		}

		free_dma_areas(slot);
	}

	slot->initialized  = 0;
	slot->allocated    = 0;
}

static long hld_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct hl_device *hl_dev;
	void __user *data;

	if (f == 0) {
		return -EFAULT;
	}

	hl_dev = f->private_data;
	data = (void __user *)arg;

	if (cmd == HL_DRV_IOC_INIT) {
		return init(f, data);
	} else if (!hl_dev) {
		return -EAGAIN;
	}

	switch (cmd) {
	case HL_DRV_IOC_INIT:
		return init(f, data);
	case HL_DRV_IOC_MEMINFO:
		return get_meminfo(hl_dev, data);
	case HL_DRV_IOC_READ_HPI:
		return hpi_read(hl_dev, data);
	case HL_DRV_IOC_WRITE_HPI:
		return hpi_write(hl_dev, data);
	case HL_DRV_IOC_LOAD_CODE:
		return load_code(hl_dev, data);
	case HL_DRV_IOC_WRITE_DATA:
		return write_data(hl_dev, data);
	case HL_DRV_IOC_READ_DATA:
		return read_data(hl_dev, data);
	case HL_DRV_IOC_MEMSET_DATA:
		return set_data(hl_dev, data);
	}

	return -ENOTTY;
}

static const struct file_operations hld_file_operations = {
	.unlocked_ioctl = hld_ioctl,
	.owner = THIS_MODULE,
};

static struct miscdevice hld_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hl_dev",
	.fops = &hld_file_operations,
};

static int hld_probe(struct platform_device *pdev)
{
	int32_t i;

	for (i = 0; i < MAX_HL_DEVICES; i++) {
		hl_devices[i].allocated    = 0;
		hl_devices[i].initialized  = 0;
		hl_devices[i].code_loaded  = 0;
		hl_devices[i].code         = 0;
		hl_devices[i].data         = 0;
		hl_devices[i].hpi_resource = 0;
		hl_devices[i].hpi          = 0;
	}
	return misc_register(&hld_device);
}

static int hld_remove(struct platform_device *pdev)
{
	int32_t i;

	for (i = 0; i < MAX_HL_DEVICES; i++) {
		free_hl_dev_slot(&hl_devices[i]);
	}

	misc_deregister(&hld_device);

	return 0;
}

static const struct of_device_id hld_of_match[] = {
	{ .compatible =	"dwc,hld" },
	{ }
};
MODULE_DEVICE_TABLE(of, hld_of_match);

static struct platform_driver __refdata dwc_hld_driver = {
	.driver		= {
		.name	= "dwc,hld",
		.of_match_table = hld_of_match,
	},
	.probe		= hld_probe,
	.remove		= hld_remove,
};
module_platform_driver(dwc_hld_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Synopsys, Inc.");
MODULE_DESCRIPTION("Linux Host Library Driver");
