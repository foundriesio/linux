// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

#include <linux/io.h>
//#include <asm/io.h>
#include <asm/div64.h>

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_mem.h"
#include "vpu_mgr.h"

#if DEFINED_CONFIG_VENC_CNT_12345
#include "vpu_enc.h"
#endif
#if DEFINED_CONFIG_VDEC_CNT_12345
#include "vpu_dec.h"
#endif

#ifdef CONFIG_OF
static const struct of_device_id vpu_mgr_of_match[] = {
	{.compatible = "telechips,vpu_dev_mgr"},	//MGR_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vpu_mgr_of_match);
#endif

static struct platform_driver vmgr_driver = {
	.probe = vmgr_probe,
	.remove = vmgr_remove,
#if defined(CONFIG_PM)
	.suspend = vmgr_suspend,
	.resume = vmgr_resume,
#endif
	.driver = {
		   .name = MGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_mgr_of_match),
#endif
	},
};

static struct platform_device vmem_device = {
	.name = MEM_NAME,
	.dev = {},
	.id = 0,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vmem_of_match[] = {
	{.compatible = "telechips,vpu_vmem"},	//MEM_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vmem_of_match);
#endif

static struct platform_driver vmem_driver = {
	.probe = vmem_probe,
	.remove = vmem_remove,
	.driver = {
		   .name = MEM_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vmem_of_match),
#endif
	},
};

#if DEFINED_CONFIG_VDEC_CNT_12345
static struct platform_device vdec_device = {
	.name = DEC_NAME,
	.dev = {},
	.id = VPU_DEC,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_of_match[] = {
	{.compatible = "telechips,vpu_vdec"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_of_match);
#endif

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VDEC_CNT_2345
static struct platform_device vdec_ext_device = {
	.name = DEC_EXT_NAME,
	.dev = {},
	.id = VPU_DEC_EXT,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext_of_match);
#endif

static struct platform_driver vdec_ext_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
static struct platform_device vdec_ext2_device = {
	.name = DEC_EXT2_NAME,
	.dev = {},
	.id = VPU_DEC_EXT2,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext2_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext2"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext2_of_match);
#endif

static struct platform_driver vdec_ext2_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT2_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext2_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VDEC_CNT_45
static struct platform_device vdec_ext3_device = {
	.name = DEC_EXT3_NAME,
	.dev = {},
	.id = VPU_DEC_EXT3,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext3_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext3"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext3_of_match);
#endif

static struct platform_driver vdec_ext3_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT3_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext3_of_match),
#endif
	},
};
#endif

#if defined(CONFIG_VDEC_CNT_5)
static struct platform_device vdec_ext4_device = {
	.name = DEC_EXT4_NAME,
	.dev = {},
	.id = VPU_DEC_EXT4,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext4_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext4"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext4_of_match);
#endif

static struct platform_driver vdec_ext4_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT4_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext4_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_12345
static struct platform_device venc_device = {
	.name = ENC_NAME,
	.dev = {},
	.id = VPU_ENC,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_of_match[] = {
	{.compatible = "telechips,vpu_venc"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_of_match);
#endif

static struct platform_driver venc_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = ENC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_2345
static struct platform_device venc_ext_device = {
	.name = ENC_EXT_NAME,
	.dev = {},
	.id = VPU_ENC_EXT,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext_of_match);
#endif

static struct platform_driver venc_ext_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = ENC_EXT_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_345
static struct platform_device venc_ext2_device = {
	.name = ENC_EXT2_NAME,
	.dev = {},
	.id = VPU_ENC_EXT2,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext2_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext2"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext2_of_match);
#endif

static struct platform_driver venc_ext2_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = ENC_EXT2_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext2_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_45
static struct platform_device venc_ext3_device = {
	.name = ENC_EXT3_NAME,
	.dev = {},
	.id = VPU_ENC_EXT3,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext3_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext3"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext3_of_match);
#endif

static struct platform_driver venc_ext3_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = ENC_EXT3_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext3_of_match),
#endif
	},
};
#endif

#if defined(CONFIG_VENC_CNT_5)
static struct platform_device venc_ext4_device = {
	.name = ENC_EXT4_NAME,
	.dev = {},
	.id = VPU_ENC_EXT4,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext4_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext4"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext4_of_match);
#endif

static struct platform_driver venc_ext4_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = ENC_EXT4_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext4_of_match),
#endif
	},
};
#endif

static void __exit vdev_cleanup(void)
{
#if defined(CONFIG_VENC_CNT_5)
	platform_driver_unregister(&venc_ext4_driver);
	platform_device_unregister(&venc_ext4_device);
#endif
#if DEFINED_CONFIG_VENC_CNT_45
	platform_driver_unregister(&venc_ext3_driver);
	platform_device_unregister(&venc_ext3_device);
#endif
#if DEFINED_CONFIG_VENC_CNT_345
	platform_driver_unregister(&venc_ext2_driver);
	platform_device_unregister(&venc_ext2_device);
#endif
#if DEFINED_CONFIG_VENC_CNT_2345
	platform_driver_unregister(&venc_ext_driver);
	platform_device_unregister(&venc_ext_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_12345
	platform_driver_unregister(&venc_driver);
	platform_device_unregister(&venc_device);
#endif

#if defined(CONFIG_VDEC_CNT_5)
	platform_driver_unregister(&vdec_ext4_driver);
	platform_device_unregister(&vdec_ext4_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_45
	platform_driver_unregister(&vdec_ext3_driver);
	platform_device_unregister(&vdec_ext3_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	platform_driver_unregister(&vdec_ext2_driver);
	platform_device_unregister(&vdec_ext2_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_2345
	platform_driver_unregister(&vdec_ext_driver);
	platform_device_unregister(&vdec_ext_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_12345
	platform_driver_unregister(&vdec_driver);
	platform_device_unregister(&vdec_device);
#endif

	platform_driver_unregister(&vmgr_driver);
}

static int vdev_init(void)
{
	pr_info("=======> VPU Devices drivers initializing!!  Start ---\n");

	platform_driver_register(&vmgr_driver);

	platform_device_register(&vmem_device);
	platform_driver_register(&vmem_driver);

#if DEFINED_CONFIG_VDEC_CNT_12345
	// register decoder driver...
	platform_device_register(&vdec_device);
	platform_driver_register(&vdec_driver);
#endif

#if DEFINED_CONFIG_VDEC_CNT_2345
	// register decoder_ext driver...
	platform_device_register(&vdec_ext_device);
	platform_driver_register(&vdec_ext_driver);
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	// register decoder_ext2 driver...
	platform_device_register(&vdec_ext2_device);
	platform_driver_register(&vdec_ext2_driver);
#endif

#if DEFINED_CONFIG_VDEC_CNT_45
	// register decoder_ext3 driver...
	platform_device_register(&vdec_ext3_device);
	platform_driver_register(&vdec_ext3_driver);
#endif

#if defined(CONFIG_VDEC_CNT_5)
	// register decoder_ext4 driver...
	platform_device_register(&vdec_ext4_device);
	platform_driver_register(&vdec_ext4_driver);
#endif

#if DEFINED_CONFIG_VENC_CNT_12345
	// register encoder driver...
	platform_device_register(&venc_device);
	platform_driver_register(&venc_driver);
#endif

#if DEFINED_CONFIG_VENC_CNT_2345
	// register encoder_ext driver...
	platform_device_register(&venc_ext_device);
	platform_driver_register(&venc_ext_driver);
#endif

#if DEFINED_CONFIG_VENC_CNT_345
	// register encoder_ext2 driver...
	platform_device_register(&venc_ext2_device);
	platform_driver_register(&venc_ext2_driver);
#endif

#if DEFINED_CONFIG_VENC_CNT_45
	// register encoder_ext3 driver...
	platform_device_register(&venc_ext3_device);
	platform_driver_register(&venc_ext3_driver);
#endif

#if defined(CONFIG_VENC_CNT_5)
	// register encoder_ext4 driver...
	platform_device_register(&venc_ext4_device);
	platform_driver_register(&venc_ext4_driver);
#endif

	pr_info("Done!!\n");
	return 0;
}

module_init(vdev_init);
module_exit(vdev_cleanup);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu devices driver");
MODULE_LICENSE("GPL");
