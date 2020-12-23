// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC

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
#include "vpu_hevc_enc_mgr.h"


#ifdef CONFIG_OF
//VPU_HEVC_ENC_MGR_NAME
static const struct of_device_id vmgr_hevc_enc_of_match[] = {
	{.compatible = "telechips,vpu_hevc_enc_dev_mgr"},
	{}
};
MODULE_DEVICE_TABLE(of, vmgr_hevc_enc_of_match);
#endif

static struct platform_driver vmgr_hevc_enc_driver = {
	.probe = vmgr_hevc_enc_probe,
	.remove = vmgr_hevc_enc_remove,
#if defined(CONFIG_PM)
	.suspend = vmgr_hevc_enc_suspend,
	.resume = vmgr_hevc_enc_resume,
#endif
	.driver = {
		 .name = VPU_HEVC_ENC_MGR_NAME,
		 .owner = THIS_MODULE,
#ifdef CONFIG_OF
		 .of_match_table = of_match_ptr(vmgr_hevc_enc_of_match),
#endif
	},
};

static void __exit vpu_hevc_enc_dev_cleanup(void)
{
	platform_driver_unregister(&vmgr_hevc_enc_driver);
}

static int vpu_hevc_enc_dev_init(void)
{
	pr_info("============> VPU HEVC ENC device drivers initializing!!  Start -------\n");

	/*
	 * Register an encoder manager driver
	 * Device is in device tree.
	 */
	platform_driver_register(&vmgr_hevc_enc_driver);

	pr_info("Done!!\n");
	return 0;
}

module_init(vpu_hevc_enc_dev_init);
module_exit(vpu_hevc_enc_dev_cleanup);
#endif /*CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC*/

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu hevc enc device driver");
MODULE_LICENSE("GPL");
