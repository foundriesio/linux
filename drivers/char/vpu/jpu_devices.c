// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
/*
 *   jpu_devices.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
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

#include <asm/io.h>
#include <asm/div64.h>

#ifdef CONFIG_SUPPORT_TCC_JPU

#include "vpu_comm.h"
#include "vpu_devices.h"

extern int jmgr_is_loadable(void);

extern int jmgr_probe(struct platform_device *pdev);
extern int jmgr_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
extern int jmgr_suspend(struct platform_device *pdev, pm_message_t state);
extern int jmgr_resume(struct platform_device *pdev);
#endif


#if 0 // ZzaU :: device-tree
static struct platform_device jmgr_device = {
	.name	= JMGR_NAME,
	.dev	= {},
	.id	= 0,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id jmgr_of_match[] = {
        { .compatible = "telechips,jpu_dev_mgr" },//JMGR_NAME
        {}
};
MODULE_DEVICE_TABLE(of, jmgr_of_match);
#endif

static struct platform_driver jmgr_driver = {
	.probe          = jmgr_probe,
	.remove         = jmgr_remove,
#if defined(CONFIG_PM)
	.suspend        = jmgr_suspend,
	.resume         = jmgr_resume,
#endif
	.driver         = {
	     .name   	= JMGR_NAME,
	     .owner  	= THIS_MODULE,
#ifdef CONFIG_OF
	     .of_match_table = of_match_ptr(jmgr_of_match),
#endif
	},
};


static void __exit jmgr_cleanup(void)
{
	platform_driver_unregister(&jmgr_driver);
#if 0 // ZzaU :: device-tree	
	//platform_device_unregister(&jmgr_device);
#endif
}

static int jmgr_init(void)
{
// register manager driver...
	if(jmgr_is_loadable() > 0)
		return -1;

	printk("============> JPU Devices drivers initializing!!  Start ------- ");

#if 0 // ZzaU :: device-tree
	platform_device_register(&jmgr_device);
#endif
	platform_driver_register(&jmgr_driver);

	printk("Done!! \n");
	return 0;
}

module_init(jmgr_init);
module_exit(jmgr_cleanup);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu devices driver");
MODULE_LICENSE("GPL");
