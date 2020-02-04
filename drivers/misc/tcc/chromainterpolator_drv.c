/*
 * linux/drivers/char/tcc_chromainterp.c
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
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <linux/fs.h> 
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/miscdevice.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_chromainterpolator.h>
#include <video/tcc/chromainterp_drv.h>
#include <video/tcc/vioc_global.h>

#define TCC_CP_DEBUG	0
#define dprintk(msg...)	if(TCC_CP_DEBUG) { printk("[DBG][CHROMA_I] " msg); }

struct chromainterp_drv_type {
	unsigned int  			dev_opened;
	struct miscdevice		*misc;
};

static long chromainterp_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	switch(cmd) 
	{
		case TCC_CITPL_SET:
			{
				struct VIOC_CITPL_SET *vioc_citpl;
				volatile void __iomem *Cl_reg;

				vioc_citpl = kmalloc(sizeof(struct VIOC_CITPL_SET), GFP_KERNEL);

				if(vioc_citpl == NULL)
					return -ENOMEM;
		
				if(copy_from_user((void *)vioc_citpl, (const void *)arg, sizeof(struct VIOC_CITPL_SET)))				{
					kfree(vioc_citpl);
					return -EFAULT;
				}

				Cl_reg = VIOC_ChromaInterpol_GetAddress(vioc_citpl->citpl_n);
				if(Cl_reg != NULL)
				{
					dprintk("SET  chroma_n:%x , mode:%d (r2y:%d mode:%d) (y2r:%d mode:%d) \n", 
						vioc_citpl->citpl_n, vioc_citpl->mode, 
						vioc_citpl->r2y_en, vioc_citpl->r2y_mode, vioc_citpl->y2r_en, vioc_citpl->y2r_mode);
					VIOC_ChromaInterpol_ctrl(Cl_reg, vioc_citpl->mode, 
						vioc_citpl->r2y_en, vioc_citpl->r2y_mode, vioc_citpl->y2r_en, vioc_citpl->y2r_mode);

				}
				kfree(vioc_citpl);
			}
			break;


		case TCC_CITPL_PLUG:
			{
				struct VIOC_CITPL_PLUG_SET citpl_plug;
				int ret;
				if(copy_from_user((void *)&citpl_plug, (const void *)arg, sizeof(struct VIOC_CITPL_PLUG_SET)))	{
					return -EFAULT;
				}

				dprintk("mPlug  onoff:%d  chroma_n:%x , rdma_n:%x  ret:%d \n",
					citpl_plug.onoff , citpl_plug.citpl_n, citpl_plug.rdma_n, ret);

				if(citpl_plug.onoff)
					ret = VIOC_CONFIG_PlugIn(citpl_plug.citpl_n, citpl_plug.rdma_n);
				else
					ret = VIOC_CONFIG_PlugOut(citpl_plug.citpl_n);
				
			}
			break;

		default:
			pr_err("[ERR][CHROMA_I] not supported LUT IOCTL(0x%x). \n", cmd);
			break;			
	}

	return 0;
}



static int chromainterp_drv_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	
	struct chromainterp_drv_type	*chromaI = dev_get_drvdata(misc->parent);

	int ret = 0;
	
	dprintk("%s():  In -open(%d) \n", __func__, chromaI->dev_opened);

	chromaI->dev_opened++;
	return ret;
}



static int chromainterp_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct chromainterp_drv_type	*chromainterp = dev_get_drvdata(misc->parent);

	dprintk("%s(): open(%d). \n", __func__, chromainterp->dev_opened);

	return 0;
}


static struct file_operations chromainterp_drv_fops = {
	.owner				= THIS_MODULE,
	.unlocked_ioctl		= chromainterp_drv_ioctl,
	.open				= chromainterp_drv_open,
	.release				= chromainterp_drv_release,
};

static int chromainterp_drv_probe(struct platform_device *pdev)
{
	struct chromainterp_drv_type *chromainterp;
	int ret = -ENODEV;

	chromainterp = kzalloc(sizeof(struct chromainterp_drv_type), GFP_KERNEL);
	if (!chromainterp)
		return -ENOMEM;

	chromainterp->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (chromainterp->misc == 0)
		goto err_misc_alloc;
	
 	/* register scaler discdevice */
	chromainterp->misc->minor = MISC_DYNAMIC_MINOR;
	chromainterp->misc->fops = &chromainterp_drv_fops;
	chromainterp->misc->name = "chromainterpol_drv";
	chromainterp->misc->parent = &pdev->dev;
	ret = misc_register(chromainterp->misc);
	if (ret)
		goto err_misc_register;
	
	platform_set_drvdata(pdev, chromainterp);

	pr_info("[INF][CHROMA_I] %s: :%s, Driver Initialized  \n",__func__, pdev->name);

	return 0;


err_misc_register:
	misc_deregister(chromainterp->misc);
	
err_misc_alloc:
	kfree(chromainterp->misc);
	kfree(chromainterp);
	pr_err("[ERR][CHROMA_I] %s: %s: err ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int chromainterp_drv_remove(struct platform_device *pdev)
{
	struct chromainterp_drv_type *chromainterp = (struct chromainterp_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(chromainterp->misc);
	kfree(chromainterp);
	return 0;
}

static int chromainterp_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int chromainterp_drv_resume(struct platform_device *pdev)
{
	return 0;
}


static struct of_device_id chromainterpolator_of_match[] = {
	{ .compatible = "telechips,chromainterpolator_drv" },
	{}
};
MODULE_DEVICE_TABLE(of, chromainterpolator_of_match);

static struct platform_driver chromainterp_driver = {
	.probe		= chromainterp_drv_probe,
	.remove		= chromainterp_drv_remove,
	.suspend		= chromainterp_drv_suspend,
	.resume		= chromainterp_drv_resume,
	.driver 	= {
		.name	= "chromainterpolator_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(chromainterpolator_of_match),
#endif
	},
};

static int __init chromainterp_drv_init(void)
{
	return platform_driver_register(&chromainterp_driver);
}

static void __exit chromainterp_drv_exit(void)
{
	platform_driver_unregister(&chromainterp_driver);
}

module_init(chromainterp_drv_init);
module_exit(chromainterp_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips look up table Driver");
MODULE_LICENSE("GPL");



