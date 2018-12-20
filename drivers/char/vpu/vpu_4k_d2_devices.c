/*
 *   hevc_devices.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
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
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/div64.h>

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2

#include "vpu_comm.h"
#include "vpu_devices.h"

extern int vmgr_4k_d2_is_loadable(void);

extern int vmgr_4k_d2_probe(struct platform_device *pdev);
extern int vmgr_4k_d2_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
extern int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state);
extern int vmgr_4k_d2_resume(struct platform_device *pdev);
#endif

#if 0 // ZzaU :: device-tree
static struct platform_device vpu_4k_d2_device = {
    .name   = VPU_4K_D2_MGR_NAME,
    .dev    = {},
    .id = 0,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id vpu_4k_d2_of_match[] = {
        { .compatible = "telechips,vpu_4k_d2_dev_mgr" },//VPU_4K_D2_MGR_NAME
        {}
};
MODULE_DEVICE_TABLE(of, vpu_4k_d2_of_match);
#endif

static struct platform_driver vpu_4k_d2_driver = {
    .probe          = vmgr_4k_d2_probe,
    .remove         = vmgr_4k_d2_remove,
#if defined(CONFIG_PM)
    .suspend        = vmgr_4k_d2_suspend,
    .resume         = vmgr_4k_d2_resume,
#endif
    .driver         = {
         .name      = VPU_4K_D2_MGR_NAME,
         .owner     = THIS_MODULE,
#ifdef CONFIG_OF
         .of_match_table = of_match_ptr(vpu_4k_d2_of_match),
#endif
    },
};


static void __exit vpu_4k_d2_cleanup(void)
{
    platform_driver_unregister(&vpu_4k_d2_driver);
#if 0 // ZzaU :: device-tree
    //platform_device_unregister(&vpu_4k_d2_device);
#endif
}

static int vpu_4k_d2_init(void)
{
    printk("============> 4K-D2 VP9/HEVC Devices drivers initializing!!  Start ------- ");

#if 0 // ZzaU :: device-tree
    platform_device_register(&vpu_4k_d2_device);
#endif
    platform_driver_register(&vpu_4k_d2_driver);

    printk("Done!! \n");
    return 0;
}

module_init(vpu_4k_d2_init);
module_exit(vpu_4k_d2_cleanup);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC 4K-D2 VP9/HEVC devices driver");
MODULE_LICENSE("GPL");
