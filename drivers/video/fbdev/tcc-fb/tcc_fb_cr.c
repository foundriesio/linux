/*
 * Copyright (C) 2008-2019 Telechips Inc.
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
static const struct of_device_id fb_cr_of_match[] = {
	{.compatible = "telechips,fb_cr"},
	{}
};
MODULE_DEVICE_TABLE(of, fb_cr_of_match);
#endif

static int __init fb_cr_probe(struct platform_device *pdev)
{
	//struct device_node *np = NULL;
	unsigned int idx;
	//unsigned int temp;
	unsigned int num_comp = 0;
	unsigned int vioc_comp = 0;
	unsigned int vioc_intr_type;
	unsigned int irq_sts; // irq_status
	//unsigned int irq_num; // irq_num

	of_property_read_u32_index(
		pdev->dev.of_node, "telechips,num_comp", 0, &num_comp);
	if (num_comp > 0) {
		for (idx = 0; idx < num_comp; idx++) {
			of_property_read_u32_index(
				pdev->dev.of_node, "telechips,vioc_comp", idx,
				&vioc_comp);
			switch (get_vioc_type(vioc_comp)) {
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
				pr_err("[ERR][FB_CR] %s : invalid vioc type\n",
				       __func__);
				return -EINVAL;
			}
			irq_sts = vioc_intr_get_status(
				vioc_intr_type + get_vioc_index(vioc_comp));
			pr_debug(
				"[DBG][FB_CR] %s : vioc component = %d, irq sts = %x\n",
				__func__,
				vioc_intr_type + get_vioc_index(vioc_comp),
				irq_sts);

			// clear interrupt
			vioc_intr_clear(
				vioc_intr_type + get_vioc_index(vioc_comp),
				irq_sts);
			irq_sts = vioc_intr_get_status(
				vioc_intr_type + get_vioc_index(vioc_comp));
			pr_debug(
				"[DBG][FB_CR] %s : after cleared. irq sts = %x\n",
				__func__, irq_sts);
		}
	} else {
		pr_err("[ERR][FB_CR] %s : invalid VIOC component number\n",
		       __func__);
		return -EINVAL;
	}

	return 0;
}

/*
 *  Cleanup
 */
static int fb_cr_remove(struct platform_device *pdev)
{
	//struct fb_info *info = platform_get_drvdata(pdev);

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
	//struct fb_info *info = platform_get_drvdata(dev);

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
	//struct fb_info *info = platform_get_drvdata(dev);

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
