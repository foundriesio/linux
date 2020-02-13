/*
 * linux/drivers/media/platform/tcc-hwdemux/tcc_hwdemux.c
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#include "hwdmx.h"
#include "hwdmx_core.h"
#include "tcc_hwdemux_tsif_rx.h"

#define HWDMX_DEV_NAME "tcc_hwdmx"
#define HWDMX_RD_BUF_SIZE (256 * 1024)

static int majornum;
static struct class *class;
static void *dma_vaddr;
static dma_addr_t dma_paddr;

int hwdmx_start(struct tcc_demux_handle *dmx, unsigned int devid)
{
	dmx->handle = (void *)tcc_hwdmx_tsif_rx_start(devid);
	return 0;
}
EXPORT_SYMBOL(hwdmx_start);

int hwdmx_stop(struct tcc_demux_handle *dmx, unsigned int devid)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_stop(demux, devid);
}
EXPORT_SYMBOL(hwdmx_stop);

int hwdmx_buffer_flush(struct tcc_demux_handle *dmx, unsigned long addr, int len)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_buffer_flush(demux, addr, len);
}
EXPORT_SYMBOL(hwdmx_buffer_flush);

int hwdmx_set_external_tsdemux(
	struct tcc_demux_handle *dmx,
	int (*decoder)(char *p1, int p1_size, char *p2, int p2_size, int devid))
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_set_external_tsdemux(demux, decoder);
}
EXPORT_SYMBOL(hwdmx_set_external_tsdemux);

int hwdmx_add_pid(struct tcc_demux_handle *dmx, struct tcc_tsif_filter *param)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_add_pid(demux, (struct tcc_hwdmx_tsif_rx_filter_param *)param);
}
EXPORT_SYMBOL(hwdmx_add_pid);

int hwdmx_remove_pid(struct tcc_demux_handle *dmx, struct tcc_tsif_filter *param)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_remove_pid(demux, (struct tcc_hwdmx_tsif_rx_filter_param *)param);
}
EXPORT_SYMBOL(hwdmx_remove_pid);

int hwdmx_set_pcr_pid(struct tcc_demux_handle *dmx, unsigned int index, unsigned int pcr_pid)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_set_pcr_pid(demux, index, pcr_pid);
}
EXPORT_SYMBOL(hwdmx_set_pcr_pid);

int hwdmx_get_stc(struct tcc_demux_handle *dmx, unsigned int index, u64 *stc)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_get_stc(demux, index, stc);
}
EXPORT_SYMBOL(hwdmx_get_stc);

int hwdmx_set_cipher_dec_pid(struct tcc_demux_handle *dmx,	unsigned int numOfPids, unsigned int delete_option, unsigned short *pids)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_set_cipher_dec_pid(demux, numOfPids, delete_option, pids);
}
EXPORT_SYMBOL(hwdmx_set_cipher_dec_pid);

int hwdmx_set_cipher_mode(struct tcc_demux_handle *dmx, int algo, int opmode,
	int residual, int smsg, unsigned int numOfPids, unsigned short *pids)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_set_mode(demux, algo, opmode, residual, smsg, numOfPids, pids);
}
EXPORT_SYMBOL(hwdmx_set_cipher_mode);

int hwdmx_set_key(struct tcc_demux_handle *dmx, int keytype, int keymode, int size, void *key)
{
	struct tcc_hwdmx_tsif_rx_handle *demux = (struct tcc_hwdmx_tsif_rx_handle *)dmx->handle;
	return tcc_hwdmx_tsif_rx_set_key(demux, keytype, keymode, size, key);
}
EXPORT_SYMBOL(hwdmx_set_key);

int hwdmx_register(int devid, struct device *dev)
{
	int result = 0;

	if (tcc_hwdmx_tsif_rx_register(devid, dev) != 0) {
		dev_err(
			dev, "[ERROR][HWDMX] %s:%d tcc_hwdmx_tsif_rx_register failed\n", __FUNCTION__,
			__LINE__);
		return -EFAULT;
	}

	if (dev != NULL
		&& device_create(class, dev, MKDEV(majornum, devid), NULL, HWDMX_DEV_NAME "%d", devid)
			== NULL) {
		dev_err(dev, "[ERROR][HWDMX] %s:%d device_create failed\n", __FUNCTION__, __LINE__);
		result = -EFAULT;
		goto device_create_fail;
	}

	return 0;

device_create_fail:
	tcc_hwdmx_tsif_rx_unregister(devid);

	return result;
}
EXPORT_SYMBOL(hwdmx_register);

int hwdmx_unregister(int devid)
{
	device_destroy(class, MKDEV(majornum, devid));
	tcc_hwdmx_tsif_rx_unregister(devid);

	return 0;
}
EXPORT_SYMBOL(hwdmx_unregister);

static int hwdmx_open(struct inode *inode, struct file *filp)
{
	int devid = MINOR(inode->i_rdev);

	filp->private_data = (void*)devid;

	hwdmx_set_interface_cmd(devid, HWDMX_INTERNAL);

	return 0;
}

/**
 * This function is used by CLS 2.0
 */
int hwdmx_input_ts(int devid, uintptr_t mmap_buf, int size)
{
	return hwdmx_input_stream_cmd(devid, mmap_buf, size);
}
EXPORT_SYMBOL(hwdmx_input_ts);

/* deprecated */
int hwdmx_input_stream(uintptr_t mmap_buf, int size)
{
	return hwdmx_input_stream_cmd(0, mmap_buf, size);
}
EXPORT_SYMBOL(hwdmx_input_stream);

static ssize_t hwdmx_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int result, devid;
	static DEFINE_MUTEX(hwdmx_buf_mutex);
	devid = (int)filp->private_data;
	if (dma_vaddr == NULL) {
		return -EFAULT;
	}
	mutex_lock(&hwdmx_buf_mutex); //for writing multiple, it is critical section
	result = copy_from_user(dma_vaddr, buf, count);
	if (result != 0) {
		pr_err("[ERROR][HWDMX] %s:%d copy_from_user fail\n", __func__, __LINE__);
		result = -EFAULT;
		goto out;
	}

	// dmx_id should be 1 at internal mode
	result = hwdmx_input_stream_cmd(devid, dma_paddr, count);
	if (result != 0) {
		result = -EFAULT;
		goto out;
	}

	// pr_info("[INFO][HWDMX] %s:%d\n", __func__, count);
	// HexDump((unsigned char *)gpvVirtAddr, 64);

out:
	mutex_unlock(&hwdmx_buf_mutex); //for writing multiple, it is critical section
	return result;
}

static long hwdmx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int devid = (int)filp->private_data;

	switch (cmd) {
	case IOCTL_HWDMX_SET_INTERFACE:
		hwdmx_set_interface_cmd(devid, arg);
		return 0;
	}
	return -1;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = hwdmx_open,
	.write = hwdmx_write,
	.unlocked_ioctl = hwdmx_ioctl,
};

static int hwdmx_probe(struct platform_device *pdev)
{
	int retval;

	printk("%s\n", __FUNCTION__);

	tcc_hwdmx_tsif_rx_init(&pdev->dev);

	retval = register_chrdev(0, HWDMX_DEV_NAME, &fops);
	if (retval < 0) {
		return retval;
	}

	majornum = retval;
	class = class_create(THIS_MODULE, HWDMX_DEV_NAME);

	dma_vaddr = dma_alloc_writecombine(&pdev->dev, HWDMX_RD_BUF_SIZE, &dma_paddr, GFP_KERNEL);
	if (dma_vaddr == NULL) {
		dev_err(&pdev->dev, "[ERROR][HWDMX] DMA alloc error.\n");
		retval = -ENOMEM;
		goto dma_alloc_fail;
	}

	hwdmx_set_evt_handler();

	return 0;

dma_alloc_fail:
	class_destroy(class);
	unregister_chrdev(majornum, HWDMX_DEV_NAME);
	tcc_hwdmx_tsif_rx_deinit(&pdev->dev);

	return retval;
}

static int hwdmx_remove(struct platform_device *pdev)
{
	printk("%s\n", __FUNCTION__);

	if (dma_vaddr != NULL) {
		dma_free_writecombine(&pdev->dev, HWDMX_RD_BUF_SIZE, dma_vaddr, dma_paddr);
		dma_vaddr = NULL;
	}
	class_destroy(class);
	unregister_chrdev(majornum, HWDMX_DEV_NAME);
	tcc_hwdmx_tsif_rx_deinit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id match[] = {
	{
		.compatible = "telechips,hwdemux",
	},
	{},
};
MODULE_DEVICE_TABLE(of, match);
#endif

static struct platform_driver hwdmx_drv = {
	.probe = hwdmx_probe,
	.remove = hwdmx_remove,
	.driver =
		{
			.name = "tcc_hwdemux",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = match,
#endif
		},
};

module_platform_driver(hwdmx_drv);
MODULE_DESCRIPTION("Hardware demux core");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");
