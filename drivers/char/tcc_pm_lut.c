/*
 * linux/drivers/char/tcc_pixelmapper.c
 * Author:  <linux@telechips.com>
 * Created: Sep. 02, 2019
 * Description: TCC VIOC h/w block 
 *
 * Copyright (C) 2019 Telechips
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
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <linux/fs.h> 
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/miscdevice.h>
#include <soc/tcc/pmap.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tcc_pm_lut_ioctl.h>
#include <video/tcc/vioc_pixel_mapper.h>
#include <video/tcc/vioc_global.h>

#define TCC_PM_LUT_DEBUG	0
#define dprintk(msg...)		if(TCC_PM_LUT_DEBUG) { printk("\x1b[1;38m TCC PM_LUT: \x1b[0m " msg); }
struct VIOC_PM_LUT_VALUE_SET backup_pm_lut_cmd;

struct pm_lut_drv_type {
	unsigned int  			dev_opened;
	struct miscdevice		*misc;
};

extern int VIOC_AUTOPWR_Enalbe(unsigned int component, unsigned int onoff);

void pm_lut_drv_set_last_frame(void)
{
	int pixelmapper_plug;
	unsigned int last_frame_pm;
	pixelmapper_plug = CheckPixelMapPathSelection(VIOC_RDMA01);

	if( pixelmapper_plug >= 0 ){
		int ret = 0;
		if(backup_pm_lut_cmd.pm_lut_dev_num == 0)
			last_frame_pm = 1;
		else
			last_frame_pm = 0;
		vioc_pm_set_lut_table(last_frame_pm, (unsigned int *)&backup_pm_lut_cmd.table);
		ret = VIOC_CONFIG_PlugIn(VIOC_PIXELMAP+last_frame_pm, VIOC_RDMA01);
		if (ret < 0)
			pr_err("%s pixel_mapper_%d plug in fail\n", __func__, get_vioc_index(VIOC_PIXELMAP0));
		dprintk("pixel mapper-%d plug in to RDMA :0x%x \n", last_frame_pm, VIOC_RDMA01);
	}
}
EXPORT_SYMBOL(pm_lut_drv_set_last_frame);

void pm_lut_drv_last_frame_plugout(void)
{
	VIOC_PlugInOutCheck VIOC_lastframe_PlugIn;
	unsigned int component = 0;
	int ret =0;

	if(VIOC_CONFIG_Device_PlugState(VIOC_PIXELMAP0, &VIOC_lastframe_PlugIn) == VIOC_DEVICE_CONNECTED){
		if(VIOC_lastframe_PlugIn.connect_device == 1 && (VIOC_lastframe_PlugIn.connect_statue==VIOC_PATH_CONNECTED))
			component = VIOC_PIXELMAP0;
	}

	if(VIOC_CONFIG_Device_PlugState(VIOC_PIXELMAP1, &VIOC_lastframe_PlugIn) == VIOC_DEVICE_CONNECTED){
		if(VIOC_lastframe_PlugIn.connect_device == 1 && (VIOC_lastframe_PlugIn.connect_statue==VIOC_PATH_CONNECTED))
			component = VIOC_PIXELMAP1;
	}

	if(component) {
	ret = VIOC_CONFIG_PlugOut(component);
	if (ret < 0)
		pr_err("%s pixel_mapper_%d plug out fail\n", __func__, get_vioc_index(VIOC_PIXELMAP0));
	}
}
EXPORT_SYMBOL(pm_lut_drv_last_frame_plugout);

static long pm_lut_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
		case TCC_PM_LUT_SET:
			{
				struct VIOC_PM_LUT_VALUE_SET *pm_lut_cmd;
				VIOC_PlugInOutCheck VIOC_PlugIn;
				int next_set_pm = VIOC_DEVICE_INVALID;
				unsigned long value = 0;

				pm_lut_cmd = kmalloc(sizeof(struct VIOC_PM_LUT_VALUE_SET), GFP_KERNEL);

				if(copy_from_user((void *)pm_lut_cmd, (const void *)arg, sizeof(struct VIOC_PM_LUT_VALUE_SET))){
					kfree(pm_lut_cmd);
					printk("%s copy from user fail\n", __func__);
					return -EFAULT;
				}
				
				if(VIOC_CONFIG_Device_PlugState(VIOC_PIXELMAP0, &VIOC_PlugIn) == VIOC_DEVICE_CONNECTED){
					if(VIOC_PlugIn.enable && (VIOC_PlugIn.connect_statue==VIOC_PATH_CONNECTED))
						next_set_pm= 1;
				}
				if(VIOC_CONFIG_Device_PlugState(VIOC_PIXELMAP1, &VIOC_PlugIn) == VIOC_DEVICE_CONNECTED){
					if(VIOC_PlugIn.enable&& (VIOC_PlugIn.connect_statue==VIOC_PATH_CONNECTED))
						next_set_pm= 0;
				}
				if(next_set_pm == VIOC_DEVICE_INVALID){
					vioc_pm_set_lut_table(0, pm_lut_cmd->table);
					vioc_pm_set_lut_table(1, pm_lut_cmd->table);
					pm_lut_cmd->pm_lut_dev_num = 0;					
					dprintk("%s pixel-mapper-0/1, table:0x%p\n", __func__, pm_lut_cmd->table);
				}
				else{
					vioc_pm_set_lut_table(next_set_pm, pm_lut_cmd->table);
					VIOC_CONFIG_PM_PlugChange();
					pm_lut_cmd->pm_lut_dev_num = next_set_pm;
					dprintk("%s pixel-mapper-%d, table:0x%p\n", __func__, next_set_pm, pm_lut_cmd->table);
				}
				
				memcpy(&backup_pm_lut_cmd, pm_lut_cmd, sizeof(struct VIOC_PM_LUT_VALUE_SET));
				
				kfree(pm_lut_cmd);
			}
			return 0;

		case TCC_PM_LUT_ONOFF:
			{
				struct VIOC_PM_LUT_ONOFF_SET pm_lut_onoff;

				if(copy_from_user((void *)&pm_lut_onoff, (const void *)arg, sizeof(struct VIOC_PM_LUT_ONOFF_SET))) {
					return -EFAULT;
				}
				dprintk("%s pm_lut_dev_num:%d pm_lut_onoff:%d \n", __func__, pm_lut_onoff.pm_lut_dev_num, pm_lut_onoff.pm_lut_onoff);
				vioc_pm_bypass(0, !(pm_lut_onoff.pm_lut_onoff));
				vioc_pm_bypass(1, !(pm_lut_onoff.pm_lut_onoff));
			}
			return 0;			
			
		case TCC_PM_LUT_PLUG_IN:
			{
				struct VIOC_PM_LUT_PLUG_IN_SET pm_lut_cmd;

				if(copy_from_user((void *)&pm_lut_cmd, (const void *)arg, sizeof(struct VIOC_PM_LUT_PLUG_IN_SET))) {
					return -EFAULT;
				}
				dprintk("%s enable:%d pm_lut dev_num:%d pm_lut_plug_in_ch:%d \n", __func__, pm_lut_cmd.enable, pm_lut_cmd.pm_lut_dev_num, pm_lut_cmd.pm_lut_plug_in_ch);

				if(!pm_lut_cmd.enable)
				{
					VIOC_CONFIG_PlugOut(VIOC_PIXELMAP+pm_lut_cmd.pm_lut_dev_num);
				}
				else
				{
					int support_plug = CheckPixelMapPathSelection(pm_lut_cmd.pm_lut_plug_in_ch);
					dprintk("pixel mapper 0  plug in to RDMA :0x%x \n", pm_lut_cmd.pm_lut_plug_in_ch);

					if( support_plug >= 0 ){
						int ret = VIOC_CONFIG_PlugIn(VIOC_PIXELMAP+pm_lut_cmd.pm_lut_dev_num, pm_lut_cmd.pm_lut_plug_in_ch);
						if (ret < 0)
							pr_err("%s pixelmapper plug in fail\n", __func__);
					}
				}
			}
			return 0;

		default:
			printk(KERN_ALERT "not supported PM_LUT IOCTL(0x%x). \n", cmd);
			break;
	}

	return 0;
}

static int pm_lut_drv_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct pm_lut_drv_type	*pm_lut = dev_get_drvdata(misc->parent);

	int ret = 0;
	
	dprintk("%s():  In -open(%d) \n", __func__, pm_lut->dev_opened);

	pm_lut->dev_opened++;
	return ret;
}

static int pm_lut_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct pm_lut_drv_type	*pm_lut = dev_get_drvdata(misc->parent);

	dprintk("%s(): open(%d). \n", __func__, pm_lut->dev_opened);

	return 0;
}

static struct file_operations pm_lut_drv_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= pm_lut_drv_ioctl,
	.open			= pm_lut_drv_open,
	.release			= pm_lut_drv_release,
};

static int pm_lut_drv_probe(struct platform_device *pdev)
{
	struct pm_lut_drv_type *pm_lut;
	int ret = -ENODEV;

	pm_lut = kzalloc(sizeof(struct pm_lut_drv_type), GFP_KERNEL);
	if (!pm_lut)
		return -ENOMEM;

	pm_lut->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (pm_lut->misc == 0)
		goto err_misc_alloc;
	
 	/* register scaler discdevice */
	pm_lut->misc->minor = MISC_DYNAMIC_MINOR;
	pm_lut->misc->fops = &pm_lut_drv_fops;
	pm_lut->misc->name = "tcc_pm_lut";
	pm_lut->misc->parent = &pdev->dev;
	ret = misc_register(pm_lut->misc);
	if (ret)
		goto err_misc_register;
	
	platform_set_drvdata(pdev, pm_lut);
	VIOC_AUTOPWR_Enalbe(VIOC_PIXELMAP0, 0);
	VIOC_AUTOPWR_Enalbe(VIOC_PIXELMAP1, 0);
	vioc_pm_initialize_set(get_vioc_index(VIOC_PIXELMAP0));
	vioc_pm_initialize_set(get_vioc_index(VIOC_PIXELMAP1));
	vioc_pm_cal_lut_reg();
	pr_info("%s: :%s, Driver Initialized  pm_lut set num :0x%x\n",__func__, pdev->name, TCC_PM_LUT_SET);
	return 0;

err_misc_register:
	misc_deregister(pm_lut->misc);
	
err_misc_alloc:
	kfree(pm_lut->misc);
	kfree(pm_lut);
	printk("%s: %s: err ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int pm_lut_drv_remove(struct platform_device *pdev)
{
	struct pm_lut_drv_type *pm_lut = (struct pm_lut_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(pm_lut->misc);
	kfree(pm_lut);
	return 0;
}

static int pm_lut_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int pm_lut_drv_resume(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id pm_lut_of_match[] = {
	{ .compatible = "telechips,vioc_pixel_mapper" },
	{}
};
MODULE_DEVICE_TABLE(of, pm_lut_of_match);

static struct platform_driver pm_lut_driver = {
	.probe		= pm_lut_drv_probe,
	.remove		= pm_lut_drv_remove,
	.suspend		= pm_lut_drv_suspend,
	.resume		= pm_lut_drv_resume,
	.driver 	= {
		.name	= "tcc_pm_lut",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(pm_lut_of_match),
#endif
	},
};

static int __init pm_lut_drv_init(void)
{
	return platform_driver_register(&pm_lut_driver);
}

static void __exit pm_lut_drv_exit(void)
{
	platform_driver_unregister(&pm_lut_driver);
}

module_init(pm_lut_drv_init);
module_exit(pm_lut_drv_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips look up table Driver");
MODULE_LICENSE("GPL");
