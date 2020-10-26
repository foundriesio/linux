/*
 * linux/drivers/char/tcc_hsm.c
 *
 * Author:  <linux@telechips.com>
 * Created: March 18, 2010
 * Description: TCC Cipher driver
 *
 * Copyright (C) 20010-2011 Telechips
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
//#define NDEBUG
#define TLOG_LEVEL (TLOG_DEBUG)
#include "tcc_hsm_log.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/timer.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/div64.h>

#include <linux/tcc_hsm.h>
#include <linux/uaccess.h>
#include "tcc_hsm_sp_cmd.h"

/****************************************************************************
  DEFINITiON
 ****************************************************************************/
#define TCC_HSM_DMA_BUF_SIZE    4096

/****************************************************************************
DEFINITION OF LOCAL VARIABLES
****************************************************************************/
static DEFINE_MUTEX(tcc_hsm_mutex);

static struct tcc_hsm_dma_buf
{
	struct device *dev;
	dma_addr_t  srcPhy;
	uint8_t     *srcVir;
	dma_addr_t  dstPhy;
	uint8_t     *dstVir;
} *dma_buf;

/****************************************************************************
DEFINITION OF LOCAL FUNCTIONS
****************************************************************************/
static long tcc_hsm_ioctl_get_version(unsigned long arg)
{
    struct tcc_hsm_ioctl_version_param param;
    long ret = -EFAULT;

	ret = tcc_hsm_sp_cmd_get_version(MBOX_DEV_M4, &param.major, &param.minor);
	if (ret != 0) {
		ELOG("failed to get version from SP\n");
		return ret;
	}

	if(copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
    }

    return ret;
}

static long tcc_hsm_ioctl_set_mode(unsigned long arg)
{
    struct tcc_hsm_ioctl_set_mode_param param;
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_set_mdoe_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

	ret = tcc_hsm_sp_cmd_set_mode(
		MBOX_DEV_M4, param.keyIndex, param.algorithm, param.opMode, param.residual, param.sMsg);

	return ret;
}

static long tcc_hsm_ioctl_set_key(unsigned long arg)
{
    struct tcc_hsm_ioctl_set_key_param param;
    uint8_t key[32] = {0, };
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_set_key_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

    if(param.key == NULL) {
		ELOG("param.key is null\n");
		return ret;
    }

    if(copy_from_user(key, (const uint8_t *)param.key, param.keySize)) {
		ELOG("copy_from_user failed(%d)\n", param.keySize);
		return ret;
    }

	ret = tcc_hsm_sp_cmd_set_key(
		MBOX_DEV_M4, param.keyIndex, param.keyType, param.keyMode, key, param.keySize);

	return ret;
}

static long tcc_hsm_ioctl_set_iv(unsigned long arg)
{
    struct tcc_hsm_ioctl_set_iv_param param;
    uint8_t iv[32] = {0, };
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_set_iv_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

    if(param.iv == NULL) {
		ELOG("param.iv is null\n");
		return ret;
    }

    if(copy_from_user(iv, (const uint8_t *)param.iv, param.ivSize)) {
		ELOG("copy_from_user failed(%d)\n", param.ivSize);
		return ret;
    }

	ret = tcc_hsm_sp_cmd_set_iv(MBOX_DEV_M4, param.keyIndex, iv, param.ivSize);

	return ret;
}

static long tcc_hsm_ioctl_set_kldata(unsigned long arg)
{
    struct tcc_hsm_ioctl_set_kldata_param param;
    struct tcc_hsm_kldata klData;
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_set_kldata_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

    if(param.klData == NULL) {
		ELOG("invalid klData\n");
		return ret;
    }

    if(copy_from_user(&klData, (const struct tcc_hsm_kldata *)param.klData, sizeof(klData))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

	ret = tcc_hsm_sp_cmd_set_kldata(
		MBOX_DEV_M4, param.keyIndex, (uintptr_t *)&klData, sizeof(klData));

	return ret;
}

static long tcc_hsm_ioctl_run_cipher(unsigned long arg)
{
    struct tcc_hsm_ioctl_run_cipher_param param;
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_iotcl_run_cipher_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

    if(copy_from_user(dma_buf->srcVir, (const uint8_t *)param.srcAddr, param.srcSize)) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

	ret = tcc_hsm_sp_cmd_run_cipher_by_dma(
		MBOX_DEV_M4, param.keyIndex, (uint32_t)dma_buf->srcPhy, (uint32_t)dma_buf->dstPhy,
		param.srcSize, param.enc, param.cwSel, param.klIndex, param.keyMode);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.srcSize, DMA_FROM_DEVICE);

	if (copy_to_user(param.dstAddr, (const uint8_t *)dma_buf->dstVir, param.srcSize)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

    return ret;
}

static long tcc_hsm_ioctl_run_cipher_by_dma(unsigned long arg)
{
    struct tcc_hsm_ioctl_run_cipher_param param;
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_run_cipher_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

	ret = tcc_hsm_sp_cmd_run_cipher_by_dma(
		MBOX_DEV_M4, param.keyIndex, (long)param.srcAddr, (long)param.dstAddr, param.srcSize,
		param.enc, param.cwSel, param.klIndex, param.keyMode);

	return ret;
}

static long tcc_hsm_ioctl_write_otp(unsigned long arg)
{
    struct tcc_hsm_ioctl_otp_param param;
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_otp_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

    if(copy_from_user(dma_buf->srcVir, (const uint8_t *)param.buf, param.size)) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

	ret = tcc_hsm_sp_cmd_write_otp(MBOX_DEV_M4, param.addr, dma_buf->srcVir, param.size);

	return ret;
}

static long tcc_hsm_ioctl_get_rng(unsigned long arg)
{
    struct tcc_hsm_ioctl_rng_param param;
    long ret = -EFAULT;

    if(copy_from_user(&param, (const struct tcc_hsm_ioctl_rng_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
    }

    if(param.rng == NULL || param.size > TCCHSM_RNG_MAX) {
		ELOG(" invalid param(%p, %d)\n", param.rng, param.size);
		return ret;
    }

	ret = tcc_hsm_sp_cmd_get_rand(MBOX_DEV_M4, dma_buf->dstVir, param.size);

	if (copy_to_user(param.rng, (const uint8_t *)dma_buf->dstVir, param.size)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_hsm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = -EFAULT;

	// DLOG("cmd=%d\n", cmd);

	mutex_lock(&tcc_hsm_mutex);

    switch(cmd)
    {
        case TCCHSM_IOCTL_GET_VERSION:
            ret = tcc_hsm_ioctl_get_version(arg);
            break;

        case TCCHSM_IOCTL_SET_MODE:
            ret = tcc_hsm_ioctl_set_mode(arg);
           break;

        case TCCHSM_IOCTL_SET_KEY:
            ret = tcc_hsm_ioctl_set_key(arg);
            break;

        case TCCHSM_IOCTL_SET_IV:
            ret = tcc_hsm_ioctl_set_iv(arg);
            break;

        case TCCHSM_IOCTL_SET_KLDATA:
            ret = tcc_hsm_ioctl_set_kldata(arg);
            break;

        case TCCHSM_IOCTL_RUN_CIPHER:
            ret = tcc_hsm_ioctl_run_cipher(arg);
            break;

        case TCCHSM_IOCTL_RUN_CIPHER_BY_DMA:
            ret = tcc_hsm_ioctl_run_cipher_by_dma(arg);
            break;

        case TCCHSM_IOCTL_WRITE_OTP:
            ret = tcc_hsm_ioctl_write_otp(arg);
            break;

        case TCCHSM_IOCTL_GET_RNG:
            ret = tcc_hsm_ioctl_get_rng(arg);
            break;

        default:
			ELOG("unknown command(%d)\n", cmd);
			break;
    }

    mutex_unlock(&tcc_hsm_mutex);

    return ret;
}

int tcc_hsm_open(struct inode *inode, struct file *filp)
{
	DLOG("\n");

	return 0;
}

int tcc_hsm_release(struct inode *inode, struct file *file)
{
	DLOG("\n");

	return 0;
}

static const struct file_operations tcc_hsm_fops =
{
    .owner		= THIS_MODULE,
    .unlocked_ioctl = tcc_hsm_ioctl,
    .compat_ioctl = tcc_hsm_ioctl,
    .open		= tcc_hsm_open,
    .release	= tcc_hsm_release,
};

static struct miscdevice tcc_hsm_miscdevice =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = TCCHSM_DEVICE_NAME,
    .fops = &tcc_hsm_fops,
};

static int tcc_hsm_probe(struct platform_device *pdev)
{
	DLOG("\n");

	dma_buf = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hsm_dma_buf), GFP_KERNEL);
	if( dma_buf == NULL ) {
		ELOG("failed to allocate dma_buf\n");
		return -ENOMEM;
	}

	dma_buf->dev = &pdev->dev;

	dma_buf->srcVir = \
        dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, &dma_buf->srcPhy, GFP_KERNEL);
	if(dma_buf->srcVir == NULL) {
		ELOG("failed to allocate dma_buf->srcVir\n");
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	dma_buf->dstVir = \
        dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, &dma_buf->dstPhy, GFP_KERNEL);
	if(dma_buf->dstVir == NULL) {
		ELOG("failed to allocate dma_buf->dstVir\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
    	devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

    if(misc_register(&tcc_hsm_miscdevice)) {
		ELOG("register device err\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
	    dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->dstVir, dma_buf->dstPhy);
    	devm_kfree(&pdev->dev, dma_buf);
        return -EBUSY;
    }

    return 0;
}

static int tcc_hsm_remove(struct platform_device *pdev)
{
	DLOG("\n");

	misc_deregister(&tcc_hsm_miscdevice);

	dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
    dma_buf->srcVir = NULL;

	dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->dstVir, dma_buf->dstPhy);
    dma_buf->dstVir = NULL;

	devm_kfree(&pdev->dev, dma_buf);

    return 0;
}

#ifdef CONFIG_PM
static int tcc_hsm_suspend(struct platform_device *pdev, pm_message_t state)
{
	DLOG("\n");
	return 0;
}

static int tcc_hsm_resume(struct platform_device *pdev)
{
	DLOG("\n");
	return 0;
}
#else
#define tcc_hsm_suspend NULL
#define tcc_hsm_resume NULL
#endif

#ifdef CONFIG_OF
static struct of_device_id hsm_of_match[] = {
    { .compatible = "telechips,tcc-hsm" },
    { "", "", "", NULL },
};
MODULE_DEVICE_TABLE(of, hsm_of_match);
#endif

static struct platform_driver tcc_hsm_driver = {
    .driver	= {
        .name	= "tcc_hsm",
        .owner 	= THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(hsm_of_match)
#endif
    },
    .probe	= tcc_hsm_probe,
    .remove	= tcc_hsm_remove,
#ifdef CONFIG_PM
    .suspend= tcc_hsm_suspend,
    .resume	= tcc_hsm_resume,
#endif
};

static int __init tcc_hsm_init(void)
{
    int ret =0;

	DLOG("\n");

	ret = platform_driver_register(&tcc_hsm_driver);
    if (ret) {
		ELOG("platform_driver_register err(%d)\n", ret);
		return 0;
    }

    return ret;
}

static void __exit tcc_hsm_exit(void)
{
	DLOG("\n");

	platform_driver_unregister(&tcc_hsm_driver);
}

module_init(tcc_hsm_init);
module_exit(tcc_hsm_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC HSM driver");
MODULE_LICENSE("GPL");

