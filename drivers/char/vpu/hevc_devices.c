/*
 * hevc_devices.c
 *
 * TCC HEVC DEVICES driver
 *
 * Copyright (C) 2013 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

#include "vpu_comm.h"
#include "vpu_devices.h"

extern int hmgr_is_loadable(void);

extern int hmgr_probe(struct platform_device *pdev);
extern int hmgr_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
extern int hmgr_suspend(struct platform_device *pdev, pm_message_t state);
extern int hmgr_resume(struct platform_device *pdev);
#endif

#if 0 // ZzaU :: device-tree
static struct platform_device hmgr_device = {
	.name	= HMGR_NAME,
	.dev	= {},
	.id	= 0,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id hmgr_of_match[] = {
        { .compatible = "telechips,hevc_dev_mgr" },//HMGR_NAME
        {}
};
MODULE_DEVICE_TABLE(of, hmgr_of_match);
#endif

static struct platform_driver hmgr_driver = {
	.probe          = hmgr_probe,
	.remove         = hmgr_remove,
#if defined(CONFIG_PM)
	.suspend        = hmgr_suspend,
	.resume         = hmgr_resume,
#endif
	.driver         = {
	     .name   	= HMGR_NAME,
	     .owner  	= THIS_MODULE,
#ifdef CONFIG_OF
	     .of_match_table = of_match_ptr(hmgr_of_match),
#endif
	},
};


static void __exit hmgr_cleanup(void)
{
	platform_driver_unregister(&hmgr_driver);
#if 0 // ZzaU :: device-tree	
	//platform_device_unregister(&hmgr_device);
#endif
}

static int hmgr_init(void)
{
// register manager driver...
	if(hmgr_is_loadable() > 0)
		return -1;

	printk("============> HEVC Devices drivers initializing!!  Start ------- ");

#if 0 // ZzaU :: device-tree
	platform_device_register(&hmgr_device);
#endif
	platform_driver_register(&hmgr_driver);

	printk("Done!! \n");
	return 0;
}

module_init(hmgr_init);
module_exit(hmgr_cleanup);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("GPL");
