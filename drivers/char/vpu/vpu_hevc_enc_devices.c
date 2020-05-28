// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
/*
 *   drivers/char/vpu/vpu_hevc_enc_devices.c
 *   Author: <linux@telechips.com>
 *   Created: Apr 28, 2020
 *   Description: TCC HEVC ENC HW block
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC

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

#include "vpu_comm.h"
#include "vpu_devices.h"

extern int vmgr_hevc_enc_is_loadable(void);

extern int vmgr_hevc_enc_probe(struct platform_device *pdev);
extern int vmgr_hevc_enc_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
extern int vmgr_hevc_enc_suspend(struct platform_device *pdev, pm_message_t state);
extern int vmgr_hevc_enc_resume(struct platform_device *pdev);
#endif

#ifdef CONFIG_OF
static struct of_device_id vmgr_hevc_enc_of_match[] = {
        { .compatible = "telechips,vpu_hevc_enc_dev_mgr" },	//VPU_HEVC_ENC_MGR_NAME
        {}
};
MODULE_DEVICE_TABLE(of, vmgr_hevc_enc_of_match);
#endif

static struct platform_driver vmgr_hevc_enc_driver = {
    .probe          = vmgr_hevc_enc_probe,
    .remove         = vmgr_hevc_enc_remove,
#if defined(CONFIG_PM)
    .suspend        = vmgr_hevc_enc_suspend,
    .resume         = vmgr_hevc_enc_resume,
#endif
    .driver         = {
         .name      = VPU_HEVC_ENC_MGR_NAME,
         .owner     = THIS_MODULE,
#ifdef CONFIG_OF
         .of_match_table = of_match_ptr(vmgr_hevc_enc_of_match),
#endif
    },
};

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || \
	defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)

extern int vpu_hevc_enc_probe(struct platform_device *pdev);
extern int vpu_hevc_enc_remove(struct platform_device *pdev);

static struct platform_device vpu_hevc_enc_device = {
	.name	= VPU_HEVC_ENC_NAME,
	.dev	= {},
	.id 	= VPU_HEVC_ENC,
};

#ifdef CONFIG_OF
static struct of_device_id vpu_hevc_enc_of_match[] = {
		{ .compatible = "telechips,vpu_hevc_enc" },
		{}
};
MODULE_DEVICE_TABLE(of, vpu_hevc_enc_of_match);
#endif

static struct platform_driver vpu_hevc_enc_driver = {
	.probe			= vpu_hevc_enc_probe,
	.remove 		= vpu_hevc_enc_remove,
	.driver 		= {
	.name			= VPU_HEVC_ENC_NAME,
	.owner			= THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = of_match_ptr(vpu_hevc_enc_of_match),
#endif
	},
};
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
static struct platform_device vpu_hevc_enc_ext_device = {
    .name   = VPU_HEVC_ENC_EXT_NAME,
    .dev    = {},
    .id     = VPU_HEVC_ENC_EXT,
};

#ifdef CONFIG_OF
static struct of_device_id vpu_hevc_enc_ext_of_match[] = {
        { .compatible = "telechips,vpu_hevc_enc_ext" },
        {}
};
MODULE_DEVICE_TABLE(of, vpu_hevc_enc_ext_of_match);
#endif

static struct platform_driver vpu_hevc_enc_ext_driver = {
    .probe          = vpu_hevc_enc_probe,
    .remove         = vpu_hevc_enc_remove,
    .driver         = {
    .name           = VPU_HEVC_ENC_EXT_NAME,
    .owner          = THIS_MODULE,
#ifdef CONFIG_OF
    .of_match_table = of_match_ptr(vpu_hevc_enc_ext_of_match),
#endif
    },
};
#endif

#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
static struct platform_device vpu_hevc_enc_ext2_device = {
    .name   = VPU_HEVC_ENC_EXT2_NAME,
    .dev    = {},
    .id     = VPU_HEVC_ENC_EXT2,
};

#ifdef CONFIG_OF
static struct of_device_id vpu_hevc_enc_ext2_of_match[] = {
        { .compatible = "telechips,vpu_hevc_enc_ext2" },
        {}
};
MODULE_DEVICE_TABLE(of, vpu_hevc_enc_ext2_of_match);
#endif

static struct platform_driver vpu_hevc_enc_ext2_driver = {
    .probe          = vpu_hevc_enc_probe,
    .remove         = vpu_hevc_enc_remove,
    .driver         = {
    .name           = VPU_HEVC_ENC_EXT2_NAME,
    .owner          = THIS_MODULE,
#ifdef CONFIG_OF
    .of_match_table = of_match_ptr(vpu_hevc_enc_ext2_of_match),
#endif
    },
};
#endif

#if defined(CONFIG_VENC_CNT_4)
static struct platform_device vpu_hevc_enc_ext3_device = {
    .name   = VPU_HEVC_ENC_EXT3_NAME,
    .dev    = {},
    .id     = VPU_HEVC_ENC_EXT3,
};

#ifdef CONFIG_OF
static struct of_device_id vpu_hevc_enc_ext3_of_match[] = {
        { .compatible = "telechips,vpu_hevc_enc_ext3" },
        {}
};
MODULE_DEVICE_TABLE(of, vpu_hevc_enc_ext3_of_match);
#endif

static struct platform_driver vpu_hevc_enc_ext3_driver = {
    .probe          = vpu_hevc_enc_probe,
    .remove         = vpu_hevc_enc_remove,
    .driver         = {
    .name           = VPU_HEVC_ENC_EXT3_NAME,
    .owner          = THIS_MODULE,
#ifdef CONFIG_OF
    .of_match_table = of_match_ptr(vpu_hevc_enc_ext3_of_match),
#endif
    },
};
#endif

/////////////////////////////////////////////// Jun 20200428
static void __exit vpu_hevc_enc_dev_cleanup(void)
{
#if defined(CONFIG_VENC_CNT_4)
    platform_driver_unregister(&vpu_hevc_enc_ext3_driver);
    platform_device_unregister(&vpu_hevc_enc_ext3_device);
#endif
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    platform_driver_unregister(&vpu_hevc_enc_ext2_driver);
    platform_device_unregister(&vpu_hevc_enc_ext2_device);
#endif
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    platform_driver_unregister(&vpu_hevc_enc_ext_driver);
    platform_device_unregister(&vpu_hevc_enc_ext_device);
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    platform_driver_unregister(&vpu_hevc_enc_driver);
    platform_device_unregister(&vpu_hevc_enc_device);
#endif

    platform_driver_unregister(&vmgr_hevc_enc_driver);
    //platform_device_unregister(&vmgr_hevc_enc_device);
}

static int vpu_hevc_enc_dev_init(void)
{
	// register manager driver...
	if(vmgr_hevc_enc_is_loadable() > 0)
		return -1;

    printk("============> VPU HEVC ENC device drivers enter!! -------\n");

	/*
	 * Register an encoder manager driver
	 * Device is in device tree.
	 */
    //platform_device_register(&vmgr_hevc_enc_device);
    platform_driver_register(&vmgr_hevc_enc_driver);

    //platform_device_register(&vmem_device);
    //platform_driver_register(&vmem_driver);

	/*
	 * Register each encoder device and driver
	 * The maximum number of instances registered are 4.
	 */

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	platform_device_register(&vpu_hevc_enc_device);
	platform_driver_register(&vpu_hevc_enc_driver);
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	platform_device_register(&vpu_hevc_enc_ext_device);
	platform_driver_register(&vpu_hevc_enc_ext_driver);
#endif
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	platform_device_register(&vpu_hevc_enc_ext2_device);
	platform_driver_register(&vpu_hevc_enc_ext2_driver);
#endif
#if defined(CONFIG_VENC_CNT_4)
	platform_device_register(&vpu_hevc_enc_ext3_device);
	platform_driver_register(&vpu_hevc_enc_ext3_driver);
#endif

	printk("============> VPU HEVC ENC device drivers out!! -------\n");
	return 0;
}

module_init(vpu_hevc_enc_dev_init);
module_exit(vpu_hevc_enc_dev_cleanup);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu hevc enc device driver");
MODULE_LICENSE("GPL");
