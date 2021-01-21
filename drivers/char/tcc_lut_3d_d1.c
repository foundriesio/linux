/*
 * linux/drivers/char/tcc_lut.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut_3d.h>
#include <video/tcc/tcc_lut_3d_ioctl.h>

#define LUT_VERSION "v1.0"

struct lut_3d_drv_type {
	struct miscdevice *misc;

	/* proc fs */
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_debug;
};

struct lut_3d_drv_type *lut_3d_api_d1;


static long lut_3d_drv_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	int ret = -EFAULT;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_3d_drv_type *lut_3d = dev_get_drvdata(misc->parent);

	switch (cmd) {
	case TCC_LUT_3D_SET_TABLE:
		{
			struct VIOC_LUT_3D_SET_TABLE  *lut_cmd;

			lut_cmd =
			    kmalloc(sizeof(struct VIOC_LUT_3D_SET_TABLE),
				    GFP_KERNEL);

			if (copy_from_user
			    ((void *)lut_cmd, (const void *)arg,
			     sizeof(struct VIOC_LUT_3D_SET_TABLE))) {
				kfree(lut_cmd);
				break;
			}

			vioc_lut_3d_set_table(LUT_3D_DISP1,
					  lut_cmd->table);
			kfree(lut_cmd);
			ret = 0;
		}
		break;

	case TCC_LUT_3D_ONOFF:
		{
			struct VIOC_LUT_3D_ONOFF lut_cmd;

			if (copy_from_user
			    ((void *)&lut_cmd, (const void *)arg,
			     sizeof(struct VIOC_LUT_3D_ONOFF))) {
				break;
			}
			ret =
			    vioc_lut_3d_bypass(LUT_3D_DISP1, lut_cmd.lut_3d_onoff);
		}
		break;

	default:
		pr_err(
		       "[ERR]\x1b[1;38m[LUT]\x1b[0m  not supported LUT IOCTL(0x%x).\n",
		       cmd);
		break;
	}

	return ret;
}

static int lut_3d_drv_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_3d_drv_type *lut_3d = dev_get_drvdata(misc->parent);

	return ret;
}

static int lut_3d_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_3d_drv_type *lut_3d = dev_get_drvdata(misc->parent);

	return 0;
}

static const struct file_operations lut_3d_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lut_3d_drv_ioctl,
	.open = lut_3d_drv_open,
	.release = lut_3d_drv_release,
};

static int lut_3d_drv_probe(struct platform_device *pdev)
{
	struct lut_3d_drv_type *lut_3d;
	int ret = -ENODEV;

	lut_3d = kzalloc(sizeof(struct lut_3d_drv_type), GFP_KERNEL);
	if (lut_3d == NULL)
		return -ENOMEM;

	lut_3d->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (lut_3d->misc == NULL)
		goto err_misc_alloc;

	/* register lut 3d misc device */
	lut_3d->misc->minor = MISC_DYNAMIC_MINOR;
	lut_3d->misc->fops = &lut_3d_drv_fops;
	lut_3d->misc->name = "tcc_lut_3d_d1";
	lut_3d->misc->parent = &pdev->dev;
	ret = misc_register(lut_3d->misc);
	if (ret)
		goto err_misc_register;

	//lut_drv_fill_mapping_table();

	platform_set_drvdata(pdev, lut_3d);

	/* Copy lut to lut_3d_api_d1 to support external APIs */
	lut_3d_api_d1 = lut_3d;

	return 0;

err_misc_register:
	misc_deregister(lut_3d->misc);

err_misc_alloc:
	kfree(lut_3d->misc);
	kfree(lut_3d);
	pr_err("[ERR]\x1b[1;38m[3D LUT]\x1b[0m %s: %s: err ret:%d\n",
	       __func__, pdev->name, ret);
	return ret;
}

static int lut_3d_drv_remove(struct platform_device *pdev)
{
	struct lut_3d_drv_type *lut_3d =
	    (struct lut_3d_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(lut_3d->misc);
	kfree(lut_3d);

	lut_3d_api_d1 = NULL;
	return 0;
}

static int lut_3d_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int lut_3d_drv_resume(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id lut_3d_d1_of_match[] = {
	{.compatible = "telechips,vioc_lut_3d_d1"},
	{}
};

MODULE_DEVICE_TABLE(of, lut_3d_d1_of_match);

static struct platform_driver lut_3d_driver_d1 = {
	.probe = lut_3d_drv_probe,
	.remove = lut_3d_drv_remove,
//	.suspend = lut_drv_3d_suspend,
//	.resume = lut_drv_3d_resume,
	.driver = {
		   .name = "tcc_lut_3d_d1",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(lut_3d_d1_of_match),
#endif
		   },
};

static int __init lut_3d_drv_init(void)
{
	return platform_driver_register(&lut_3d_driver_d1);
}

static void __exit lut_3d_drv_exit(void)
{
	platform_driver_unregister(&lut_3d_driver_d1);
}

module_init(lut_3d_drv_init);
module_exit(lut_3d_drv_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips 3D look up table Driver 1");
MODULE_LICENSE("GPL");
