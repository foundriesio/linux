/*
 * linux/drivers/char/tcc_thsm.c
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
#include <linux/mailbox/tcc_multi_ipc.h>
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

#include <linux/tcc_thsm.h>
#include <linux/uaccess.h>
#include "tcc_thsm_cmd.h"

/****************************************************************************
  DEFINITiON
 ****************************************************************************/
#define TCC_THSM_DMA_BUF_SIZE 4096

#define DEBUG_TCC_THSM
#ifdef DEBUG_TCC_THSM
#undef dprintk
#define dprintk(msg...) printk(KERN_DEBUG "[DEBUG][TCCTHSM] " msg);
#undef eprintk
#define eprintk(msg...) printk(KERN_ERR "[ERROR][TCCTHSM] " msg);
#else
#undef dprintk
#define dprintk(msg...)
#undef eprintk
#define eprintk(msg...) // printk(KERN_ERR "[ERROR][TCCTHSM] " msg);
#endif

/****************************************************************************
DEFINITION OF LOCAL VARIABLES
****************************************************************************/
static DEFINE_MUTEX(tcc_thsm_mutex);

static struct tcc_thsm_dma_buf
{
	struct device *dev;
	dma_addr_t srcPhy;
	uint8_t *srcVir;
	dma_addr_t dstPhy;
	uint8_t *dstVir;
} * dma_buf;

/****************************************************************************
DEFINITION OF LOCAL FUNCTIONS
****************************************************************************/
static long tcc_thsm_ioctl_get_version(unsigned long arg)
{
	struct tcc_thsm_ioctl_version_param param;
	long ret = -EFAULT;

	ret = tcc_thsm_cmd_get_version(MBOX_DEV_A53, &param.major, &param.minor);
	if (ret != 0) {
		eprintk("[%s:%d] failed to get version from multi device\n", __func__, __LINE__);
		return ret;
	}

	if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
		eprintk("[%s:%d] copy_to_user failed\n", __func__, __LINE__);
		return -EFAULT;
	}

	return ret;
}

static long tcc_thsm_ioctl_set_mode(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_mode_param param;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_mdoe_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	ret = tcc_thsm_cmd_set_mode(
		MBOX_DEV_A53, param.keyIndex, param.algorithm, param.opMode, param.residual, param.sMsg);

	return ret;
}

static long tcc_thsm_ioctl_set_key(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_key_param param;
	uint8_t key[32] = {
		0,
	};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_key_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	if (param.key == NULL) {
		eprintk("[%s:%d] param.key is null\n", __func__, __LINE__);
		return ret;
	}

	if (copy_from_user(key, (const uint8_t *)param.key, param.keySize)) {
		eprintk("[%s:%d] copy_from_user failed(%d)\n", __func__, __LINE__, param.keySize);
		return ret;
	}

	ret = tcc_thsm_cmd_set_key(
		MBOX_DEV_A53, param.keyIndex, param.keyType, param.keyMode, key, param.keySize);

	return ret;
}

static long tcc_thsm_ioctl_set_iv(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_iv_param param;
	uint8_t iv[32] = {
		0,
	};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_iv_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	if (param.iv == NULL) {
		eprintk("[%s:%d] param.iv is null\n", __func__, __LINE__);
		return ret;
	}

	if (copy_from_user(iv, (const uint8_t *)param.iv, param.ivSize)) {
		eprintk("[%s:%d] copy_from_user failed(%d)\n", __func__, __LINE__, param.ivSize);
		return ret;
	}

	ret = tcc_thsm_cmd_set_iv(MBOX_DEV_A53, param.keyIndex, iv, param.ivSize);

	return ret;
}

static long tcc_thsm_ioctl_set_kldata(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_kldata_param param;
	struct tcc_thsm_kldata klData;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_set_kldata_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	if (param.klData == NULL) {
		eprintk("[%s] invalid klData\n", __func__);
		return ret;
	}

	if (copy_from_user(&klData, (const struct tcc_thsm_kldata *)param.klData, sizeof(klData))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	ret =
		tcc_thsm_cmd_set_kldata(MBOX_DEV_A53, param.keyIndex, (uintptr_t *)&klData, sizeof(klData));

	return ret;
}

static long tcc_thsm_ioctl_run_cipher(unsigned long arg)
{
	struct tcc_thsm_ioctl_run_cipher_param param;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_iotcl_run_cipher_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.srcAddr, param.srcSize)) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}
	/*
		dprintk("[%02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02x%02x\n",
			dma_buf->srcVir[0], dma_buf->srcVir[1], dma_buf->srcVir[2], dma_buf->srcVir[3],
			dma_buf->srcVir[4], dma_buf->srcVir[5], dma_buf->srcVir[6], dma_buf->srcVir[7],
			dma_buf->srcVir[8], dma_buf->srcVir[9], dma_buf->srcVir[10], dma_buf->srcVir[11],
			dma_buf->srcVir[12], dma_buf->srcVir[13], dma_buf->srcVir[14], dma_buf->srcVir[15]);
	*/
	ret = tcc_thsm_cmd_run_cipher_by_dma(
		MBOX_DEV_A53, param.keyIndex, (uint32_t)dma_buf->srcPhy, (uint32_t)dma_buf->dstPhy,
		param.srcSize, param.enc, param.cwSel, param.klIndex, param.keyMode);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.srcSize, DMA_FROM_DEVICE);

	if (copy_to_user(param.dstAddr, (const uint8_t *)dma_buf->dstVir, param.srcSize)) {
		eprintk("[%s] copy_to_user failed\n", __func__);
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_run_cipher_by_dma(unsigned long arg)
{
	struct tcc_thsm_ioctl_run_cipher_param param;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_run_cipher_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	ret = tcc_thsm_cmd_run_cipher_by_dma(
		MBOX_DEV_A53, param.keyIndex, (uint32_t)param.srcAddr, (uint32_t)param.dstAddr,
		param.srcSize, param.enc, param.cwSel, param.klIndex, param.keyMode);

	return ret;
}

static long tcc_thsm_ioctl_write_otp(unsigned long arg)
{
	struct tcc_thsm_ioctl_otp_param param;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_otp_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.buf, param.size)) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	ret = tcc_thsm_cmd_write_otp(MBOX_DEV_A53, param.addr, dma_buf->srcVir, param.size);

	return ret;
}

static long tcc_thsm_ioctl_get_rng(unsigned long arg)
{
	struct tcc_thsm_ioctl_rng_param param;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_rng_param *)arg, sizeof(param))) {
		eprintk("[%s:%d] copy_from_user failed\n", __func__, __LINE__);
		return ret;
	}

	if (param.rng == NULL || param.size > TCCTHSM_RNG_MAX) {
		eprintk("[%s:%d] invalid param(%p, %d)\n", __func__, __LINE__, param.rng, param.size);
		return ret;
	}

	ret = tcc_thsm_cmd_get_rand(MBOX_DEV_A53, dma_buf->dstVir, param.size);

	if (copy_to_user(param.rng, (const uint8_t *)dma_buf->dstVir, param.size)) {
		eprintk("[%s] copy_to_user failed\n", __func__);
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EFAULT;

	// dprintk("[%s] cmd=%d\n", __func__, cmd);

	mutex_lock(&tcc_thsm_mutex);

	switch (cmd) {
	case TCCTHSM_IOCTL_GET_VERSION:
		ret = tcc_thsm_ioctl_get_version(arg);
		break;

	case TCCTHSM_IOCTL_SET_MODE:
		ret = tcc_thsm_ioctl_set_mode(arg);
		break;

	case TCCTHSM_IOCTL_SET_KEY:
		ret = tcc_thsm_ioctl_set_key(arg);
		break;

	case TCCTHSM_IOCTL_SET_IV:
		ret = tcc_thsm_ioctl_set_iv(arg);
		break;

	case TCCTHSM_IOCTL_SET_KLDATA:
		ret = tcc_thsm_ioctl_set_kldata(arg);
		break;

	case TCCTHSM_IOCTL_RUN_CIPHER:
		ret = tcc_thsm_ioctl_run_cipher(arg);
		break;

	case TCCTHSM_IOCTL_RUN_CIPHER_BY_DMA:
		ret = tcc_thsm_ioctl_run_cipher_by_dma(arg);
		break;

	case TCCTHSM_IOCTL_WRITE_OTP:
		ret = tcc_thsm_ioctl_write_otp(arg);
		break;

	case TCCTHSM_IOCTL_GET_RNG:
		ret = tcc_thsm_ioctl_get_rng(arg);
		break;

	default:
		eprintk("[%s] unknown command(%d)\n", __func__, cmd);
		break;
	}

	mutex_unlock(&tcc_thsm_mutex);

	return ret;
}

int tcc_thsm_open(struct inode *inode, struct file *filp)
{
	dprintk("%s\n", __func__);

	return 0;
}

int tcc_thsm_release(struct inode *inode, struct file *file)
{
	dprintk("%s\n", __func__);

	return 0;
}

static const struct file_operations tcc_thsm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tcc_thsm_ioctl,
	.compat_ioctl = tcc_thsm_ioctl,
	.open = tcc_thsm_open,
	.release = tcc_thsm_release,
};

static struct miscdevice tcc_thsm_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TCCTHSM_DEVICE_NAME,
	.fops = &tcc_thsm_fops,
};

static int tcc_thsm_probe(struct platform_device *pdev)
{
	dprintk("%s\n", __func__);

	dma_buf = devm_kzalloc(&pdev->dev, sizeof(struct tcc_thsm_dma_buf), GFP_KERNEL);
	if (dma_buf == NULL) {
		printk("%s failed to allocate dma_buf\n", __func__);
		return -ENOMEM;
	}

	dma_buf->dev = &pdev->dev;

	dma_buf->srcVir =
		dma_alloc_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, &dma_buf->srcPhy, GFP_KERNEL);
	if (dma_buf->srcVir == NULL) {
		dprintk("%s failed to allocate dma_buf->srcVir\n", __func__);
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	dma_buf->dstVir =
		dma_alloc_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, &dma_buf->dstPhy, GFP_KERNEL);
	if (dma_buf->dstVir == NULL) {
		dprintk("%s failed to allocate dma_buf->dstVir\n", __func__);
		dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	if (misc_register(&tcc_thsm_miscdevice)) {
		eprintk("[%s] register device err\n", __func__);
		dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
		dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->dstVir, dma_buf->dstPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -EBUSY;
	}

	return 0;
}

static int tcc_thsm_remove(struct platform_device *pdev)
{
	dprintk("%s\n", __func__);

	misc_deregister(&tcc_thsm_miscdevice);

	dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
	dma_buf->srcVir = NULL;

	dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->dstVir, dma_buf->dstPhy);
	dma_buf->dstVir = NULL;

	devm_kfree(&pdev->dev, dma_buf);

	return 0;
}

#ifdef CONFIG_PM
static int tcc_thsm_suspend(struct platform_device *pdev, pm_message_t state)
{
	dprintk("%s\n", __func__);
	return 0;
}

static int tcc_thsm_resume(struct platform_device *pdev)
{
	dprintk("%s\n", __func__);
	return 0;
}
#else
#define tcc_thsm_suspend NULL
#define tcc_thsm_resume NULL
#endif

#ifdef CONFIG_OF
static struct of_device_id thsm_of_match[] = {
	{.compatible = "telechips,tcc-thsm"},
	{"", "", "", NULL},
};
MODULE_DEVICE_TABLE(of, thsm_of_match);
#endif

static struct platform_driver tcc_thsm_driver = {
	.driver = {.name = "tcc_thsm",
			   .owner = THIS_MODULE,
#ifdef CONFIG_OF
			   .of_match_table = of_match_ptr(thsm_of_match)
#endif
	},
	.probe = tcc_thsm_probe,
	.remove = tcc_thsm_remove,
#ifdef CONFIG_PM
	.suspend = tcc_thsm_suspend,
	.resume = tcc_thsm_resume,
#endif
};

static int __init tcc_thsm_init(void)
{
	int ret = 0;

	dprintk("%s\n", __func__);

	ret = platform_driver_register(&tcc_thsm_driver);
	if (ret) {
		eprintk("[%s] platform_driver_register err(%d) \n", __func__, ret);
		return 0;
	}

	return ret;
}

static void __exit tcc_thsm_exit(void)
{
	dprintk("%s\n", __func__);

	platform_driver_unregister(&tcc_thsm_driver);
}

module_init(tcc_thsm_init);
module_exit(tcc_thsm_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC THSM driver");
MODULE_LICENSE("GPL");
