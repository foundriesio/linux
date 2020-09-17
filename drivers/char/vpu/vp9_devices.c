// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/moduleparam.h>
#include <linux/device.h>
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9

#include <asm/io.h>
#include <asm/div64.h>

#include "vpu_comm.h"
#include "vpu_devices.h"

extern int vp9mgr_is_loadable(void);

extern int vp9mgr_probe(struct platform_device* pdev);
extern int vp9mgr_remove(struct platform_device* pdev);

#if defined(CONFIG_PM)
extern int vp9mgr_suspend(struct platform_device* pdev, pm_message_t state);
extern int vp9mgr_resume(struct platform_device* pdev);
#endif

#ifdef CONFIG_OF
static struct of_device_id vp9mgr_of_match[] =
{
		{ .compatible = "telechips,vp9_dev_mgr" }, //VP9MGR_NAME
		{}
};
MODULE_DEVICE_TABLE(of, vp9mgr_of_match);
#endif

static struct platform_driver vp9mgr_driver =
{
	.probe          = vp9mgr_probe,
	.remove         = vp9mgr_remove,
#if defined(CONFIG_PM)
	.suspend        = vp9mgr_suspend,
	.resume         = vp9mgr_resume,
#endif
	.driver         =
	{
		 .name   	= VP9MGR_NAME,
		 .owner  	= THIS_MODULE,
#ifdef CONFIG_OF
		 .of_match_table = of_match_ptr(vp9mgr_of_match),
#endif
	},
};

static void __exit vp9mgr_cleanup(void)
{
	platform_driver_unregister(&vp9mgr_driver);
}

static int vp9mgr_init(void)
{
	if (vp9mgr_is_loadable() > 0)
		return -1;

	printk("============> VP9 Devices drivers initializing!!  Start ------- ");

	platform_driver_register(&vp9mgr_driver);

	printk("Done!! \n");
	return 0;
}

module_init(vp9mgr_init);
module_exit(vp9mgr_cleanup);
#endif /*CONFIG_SUPPORT_TCC_G2V2_VP9*/

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vp9 devices driver");
MODULE_LICENSE("GPL");
