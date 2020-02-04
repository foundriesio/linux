/****************************************************************************
linux/drivers/video/tcc/viqe/tcc_vioc_viqe.c
Description: TCC VIOC h/w block 

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>

#include <video/tcc/vioc_viqe.h>

#include "tcc_vioc_viqe.h"
#include "tcc_vioc_viqe_interface.h"

#if 0
#define dprintk(msg...)	 { printk("[DBG][VIQE] " msg); }
#else
#define dprintk(msg...)	 
#endif

static long tcc_viqe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	VIQE_DI_TYPE viqe_arg;

	dprintk("%s  (0x%x)  \n", __func__, cmd);

	switch (cmd) {
		case IOCTL_VIQE_INITIALIZE:
		case IOCTL_VIQE_INITIALIZE_KERNEL:
			if(cmd == IOCTL_VIQE_INITIALIZE_KERNEL) {
				memcpy(&viqe_arg, (VIQE_DI_TYPE *)arg, sizeof(VIQE_DI_TYPE));
			} else {
				if(copy_from_user(&viqe_arg, (VIQE_DI_TYPE *)arg, sizeof(VIQE_DI_TYPE))){
					printk(KERN_ALERT "%s():  Error copy_from_user(%d). \n", __func__, cmd);
					return -EINVAL;
				}
			}

			dprintk("[VIQE Initiaize] LCDC %d, Scaler %d, OnTheFly %d, W:%d, H:%d\n",
	               	viqe_arg.lcdCtrlNo, viqe_arg.scalerCh, viqe_arg.onTheFly, viqe_arg.srcWidth, viqe_arg.srcHeight);

			TCC_VIQE_DI_Init(&viqe_arg);
			break;

		case IOCTL_VIQE_EXCUTE:
		case IOCTL_VIQE_EXCUTE_KERNEL:
			if(cmd == IOCTL_VIQE_EXCUTE_KERNEL) {
				memcpy(&viqe_arg, (VIQE_DI_TYPE *)arg, sizeof(VIQE_DI_TYPE));
			} else {
				if(copy_from_user(&viqe_arg, (VIQE_DI_TYPE *)arg, sizeof(VIQE_DI_TYPE))){
					printk(KERN_ALERT "%s():  Error copy_from_user(%d). \n", __func__, cmd);
					return -EINVAL;
				}
			}

			dprintk("[VIQE_EXCUTE]scalerCH %d, Address %x %x %x %x %x %x, srcWidth %d, srcHeight %d\nlcdcCH %d, dstAddr %x, M2MMode %d, OddFirst %d, DI %d\n",
                       viqe_arg.scalerCh, viqe_arg.address[0], viqe_arg.address[1], viqe_arg.address[2], viqe_arg.address[3], viqe_arg.address[4], viqe_arg.address[5],
                       viqe_arg.srcWidth, viqe_arg.srcHeight, viqe_arg.lcdCtrlNo, viqe_arg.dstAddr, viqe_arg.deinterlaceM2M, viqe_arg.OddFirst, viqe_arg.DI_use);

			TCC_VIQE_DI_Run(&viqe_arg);
			break;

		case IOCTL_VIQE_DEINITIALIZE:
		case IOCTL_VIQE_DEINITIALIZE_KERNEL:
			if(cmd == IOCTL_VIQE_DEINITIALIZE_KERNEL) {
				memcpy(&viqe_arg, (VIQE_DI_TYPE *)arg, sizeof(VIQE_DI_TYPE));
			} else {
				if(copy_from_user(&viqe_arg, (VIQE_DI_TYPE *)arg, sizeof(VIQE_DI_TYPE))){
					printk(KERN_ALERT "%s():  Error copy_from_user(%d). \n", __func__, cmd);
					return -EINVAL;
				}
			}
			TCC_VIQE_DI_DeInit(&viqe_arg);
			break;

		default:
			pr_warn("[WAN][VIQE] unrecognized ioctl (0x%x)\n", cmd); 
			ret = -EINVAL;
			break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long tcc_viqe_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return tcc_viqe_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int tcc_viqe_release(struct inode *inode, struct file *filp)
{
	pr_info("[INF][VIQE] %s\n", __func__);
	return 0;
}

static int tcc_viqe_open(struct inode *inode, struct file *filp)
{
	pr_info("[INF][VIQE] %s\n", __func__);
	return 0;
}


struct file_operations tcc_viqe_fops =
{
	.owner		= THIS_MODULE,
	.open		= tcc_viqe_open,
	.release	= tcc_viqe_release,
	.unlocked_ioctl = tcc_viqe_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tcc_viqe_compat_ioctl,
#endif
};

static struct class *viqe_class;

static int __init tcc_viqe_init(void)
{
	int res;

	res = register_chrdev(VIQE_DEV_MAJOR, VIQE_DEV_NAME, &tcc_viqe_fops);
	if (res < 0)
		return res;
	viqe_class = class_create(THIS_MODULE, VIQE_DEV_NAME);
	device_create(viqe_class, NULL, MKDEV(VIQE_DEV_MAJOR, VIQE_DEV_MINOR), NULL, VIQE_DEV_NAME);

	pr_info("[INF][VIQE] %s\n", __func__);
	return 0;
}

static void __exit tcc_viqe_exit(void)
{
	unregister_chrdev(VIQE_DEV_MAJOR, VIQE_DEV_NAME);
	pr_info("[INF][VIQE] %s\n", __func__);
}

module_init(tcc_viqe_init);
module_exit(tcc_viqe_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCCxxx viqe driver");
MODULE_LICENSE("GPL");
