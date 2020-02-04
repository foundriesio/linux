/*
 * linux/drivers/video/skeletonfb.c -- Skeleton for a frame buffer device
 *
 *  Modified to new api Jan 2001 by James Simmons (jsimmons@transvirtual.com)
 *
 *  Created 28 Dec 1997 by Geert Uytterhoeven
 *
 *
 *  I have started rewriting this driver as a example of the upcoming new API
 *  The primary goal is to remove the console code from fbdev and place it
 *  into fbcon.c. This reduces the code and makes writing a new fbdev driver
 *  easy since the author doesn't need to worry about console internals. It
 *  also allows the ability to run fbdev without a console/tty system on top 
 *  of it. 
 *
 *  First the roles of struct fb_info and struct display have changed. Struct
 *  display will go away. The way the new framebuffer console code will
 *  work is that it will act to translate data about the tty/console in 
 *  struct vc_data to data in a device independent way in struct fb_info. Then
 *  various functions in struct fb_ops will be called to store the device 
 *  dependent state in the par field in struct fb_info and to change the 
 *  hardware to that state. This allows a very clean separation of the fbdev
 *  layer from the console layer. It also allows one to use fbdev on its own
 *  which is a bounus for embedded devices. The reason this approach works is  
 *  for each framebuffer device when used as a tty/console device is allocated
 *  a set of virtual terminals to it. Only one virtual terminal can be active 
 *  per framebuffer device. We already have all the data we need in struct 
 *  vc_data so why store a bunch of colormaps and other fbdev specific data
 *  per virtual terminal. 
 *
 *  As you can see doing this makes the con parameter pretty much useless
 *  for struct fb_ops functions, as it should be. Also having struct  
 *  fb_var_screeninfo and other data in fb_info pretty much eliminates the 
 *  need for get_fix and get_var. Once all drivers use the fix, var, and cmap
 *  fbcon can be written around these fields. This will also eliminate the
 *  need to regenerate struct fb_var_screeninfo, struct fb_fix_screeninfo
 *  struct fb_cmap every time get_var, get_fix, get_cmap functions are called
 *  as many drivers do now. 
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
_int*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/platform_device.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <soc/tcc/pmap.h>

#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_scaler_ioctrl.h>
#include <video/tcc/tcc_attach_ioctrl.h>


#ifdef CONFIG_OF
static struct of_device_id fb_cr_of_match[] = {
	{ .compatible = "telechips,fb_cr" },
	{}
};
MODULE_DEVICE_TABLE(of, fb_cr_of_match);
#endif

static int __init fb_cr_probe (struct platform_device *pdev)
{
	struct device_node *np = NULL;
	unsigned int idx;
	unsigned int temp;
	unsigned int num_comp = 0;
	unsigned int vioc_comp = 0;
	unsigned int vioc_intr_type;
	unsigned int irq_sts; //irq_status
	unsigned int irq_num; //irq_num

	of_property_read_u32_index(pdev->dev.of_node, "telechips,num_comp", 0, &num_comp);
	if(num_comp > 0){
		for(idx = 0 ; idx < num_comp ; idx++){
			of_property_read_u32_index(pdev->dev.of_node, "telechips,vioc_comp", idx , &vioc_comp);
			switch(get_vioc_type(vioc_comp)){
				case get_vioc_type(VIOC_DISP):
					vioc_intr_type = VIOC_INTR_DEV0;
					break;
				case get_vioc_type(VIOC_RDMA):
					vioc_intr_type = VIOC_INTR_RD0;
					break;
				case get_vioc_type(VIOC_WDMA):
					vioc_intr_type = VIOC_INTR_WD0;
					break;
				default:
					pr_err("[ERR][FB_CR] %s : invalid vioc type\n",__func__);
					return -EINVAL;
					break;
			}
			irq_sts = vioc_intr_get_status(vioc_intr_type + get_vioc_index(vioc_comp));
			pr_debug("[DBG][FB_CR] %s : vioc component = %d, irq sts = %x\n",__func__,vioc_intr_type + get_vioc_index(vioc_comp), irq_sts);

			//clear interrupt
			vioc_intr_clear(vioc_intr_type + get_vioc_index(vioc_comp) , irq_sts);
			irq_sts = vioc_intr_get_status(vioc_intr_type + get_vioc_index(vioc_comp));
			pr_debug("[DBG][FB_CR] %s : after cleared. irq sts = %x\n",__func__,irq_sts);
		}
	}else{
		pr_err("[ERR][FB_CR] %s : invalid VIOC component number\n",__func__);
		return -EINVAL;
	}

	return 0;
}

/*
 *  Cleanup
 */
static int fb_cr_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);
	return 0;
}

#ifdef CONFIG_PM
/**
 *	fb_cr_suspend - Optional but recommended function. Suspend the device.
 *	@dev: platform device
 *	@msg: the suspend event code.
 *
 *      See Documentation/power/devices.txt for more information
 */
static int fb_cr_suspend(struct platform_device *dev, pm_message_t msg)
{
	struct fb_info *info = platform_get_drvdata(dev);
	/* suspend here */
	return 0;
}

/**
 *	fb_cr_resume - Optional but recommended function. Resume the device.
 *	@dev: platform device
 *
 *      See Documentation/power/devices.txt for more information
 */
static int fb_cr_resume(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
	/* resume here */
	return 0;
}
#else
#define fb_cr_suspend NULL
#define fb_cr_resume NULL
#endif /* CONFIG_PM */

static struct platform_driver fb_cr_driver = {
	.probe = fb_cr_probe,
	.remove = fb_cr_remove,
	.suspend = fb_cr_suspend, /* optional but recommended */
	.resume = fb_cr_resume,   /* optional but recommended */
	.driver = {
		.name = "fb_cr",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fb_cr_of_match),
#endif
	},
};

static int __init fb_cr_init(void)
{
	int ret;
	ret = platform_driver_register(&fb_cr_driver);
	return ret;
}
fs_initcall(fb_cr_init);

static void __exit fb_cr_exit(void)
{
	platform_driver_unregister(&fb_cr_driver);
}

MODULE_AUTHOR("Inho Hwang <inhowhite@telechips.com>");
MODULE_DESCRIPTION("telechips fb interrupt manager driver");
MODULE_LICENSE("GPL");
