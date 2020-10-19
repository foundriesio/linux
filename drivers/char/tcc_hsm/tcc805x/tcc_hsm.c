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
#define TLOG_LEVEL TLOG_DEBUG
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

#include <linux/uaccess.h>
#include "tcc_hsm.h"
#include "tcc_hsm_cmd.h"

/****************************************************************************
  DEFINITiON
 ****************************************************************************/
#define TCC_HSM_DMA_BUF_SIZE 4096

/****************************************************************************
DEFINITION OF LOCAL VARIABLES
****************************************************************************/
static DEFINE_MUTEX(tcc_hsm_mutex);

static struct tcc_hsm_dma_buf
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
static long tcc_hsm_ioctl_set_key(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_set_key_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_SET_KEY_FROM_OTP_CMD) {
		req = REQ_HSM_SET_KEY_FROM_OTP;
	} else if (cmd == HSM_SET_KEY_FROM_SNOR_CMD) {
		req = REQ_HSM_SET_KEY_FROM_SNOR;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_set_key_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_hsm_cmd_set_key(MBOX_DEV_HSM, req, param.addr, param.data_size, param.key_index);
	if (ret) {
		ELOG("tcc_hsm_cmd_set_key failed\n");
		return -EFAULT;
	}
	return ret;
}

static long tcc_hsm_ioctl_run_aes(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_aes_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_RUN_AES_CMD) {
		req = REQ_HSM_RUN_AES;
	} else if (cmd == HSM_RUN_SM4_CMD) {
		req = REQ_HSM_RUN_SM4;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_aes_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}
	if (param.dma) {
		ret = tcc_hsm_cmd_run_aes(
			MBOX_DEV_HSM, req, param.obj_id, param.key, param.key_size, param.iv, param.iv_size,
			param.tag, param.tag_size, param.aad, param.aad_size, param.src, param.src_size,
			param.dst, param.dst_size);
		if (ret != 0) {
			ELOG("tcc_hsm_cmd_run_aes fail(%d)\n", ret);
		} else {
			if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
				ELOG("copy_to_user failed\n");
				return -EFAULT;
			}
		}
	} else {
		if (copy_from_user((void *)dma_buf->srcVir, (void *)param.src, param.src_size)) {
			ELOG("copy_from_user failed(%d)\n", ret);
			return ret;
		}

		ret = tcc_hsm_cmd_run_aes(
			MBOX_DEV_HSM, req, param.obj_id, param.key, param.key_size, param.iv, param.iv_size,
			param.tag, param.tag_size, param.aad, param.aad_size, (uint32_t)dma_buf->srcPhy,
			param.src_size, (uint32_t)dma_buf->dstPhy, param.dst_size);

		dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.dst_size, DMA_FROM_DEVICE);
		if (ret != 0) {
			ELOG("tcc_hsm_cmd_run_aes fail(%d)\n", ret);
		} else {
			if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
				ELOG("copy_to_user failed\n");
				return -EFAULT;
			}
			if (copy_to_user((void *)param.dst, (void *)dma_buf->dstVir, param.dst_size)) {
				ELOG("copy_to_user failed\n");
				return ret;
			}
		}
	}

	return ret;
}

static long tcc_hsm_ioctl_run_aes_by_kt(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_aes_by_kt_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_RUN_AES_BY_KT_CMD) {
		req = REQ_HSM_RUN_AES_BY_KT;
	} else if (cmd == HSM_RUN_SM4_BY_KT_CMD) {
		req = REQ_HSM_RUN_SM4_BY_KT;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_aes_by_kt_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.dma) {
		ret = tcc_hsm_cmd_run_aes_by_kt(
			MBOX_DEV_HSM, req, param.obj_id, param.key_index, param.iv, param.iv_size, param.tag,
			param.tag_size, param.aad, param.aad_size, param.src, param.src_size, param.dst,
			param.dst_size);
		if (ret != 0) {
			ELOG("tcc_hsm_cmd_run_ecdsa fail(%d)\n", ret);
			return -EFAULT;
		} else {
			if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
				ELOG("copy_to_user failed\n");
				return -EFAULT;
			}
		}
	} else {
		if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.src, param.src_size)) {
			ELOG("copy_from_user failed\n");
			return ret;
		}

		ret = tcc_hsm_cmd_run_aes_by_kt(
			MBOX_DEV_HSM, req, param.obj_id, param.key_index, param.iv, param.iv_size, param.tag,
			param.tag_size, param.aad, param.aad_size, dma_buf->srcPhy, param.src_size,
			dma_buf->dstPhy, param.dst_size);

		dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.dst_size, DMA_FROM_DEVICE);
		if (ret != 0) {
			ELOG("tcc_hsm_ioctl_run_aes_by_kt fail(%d)\n", ret);
			return -EFAULT;
		} else {
			if (copy_to_user((void *)param.dst, (void *)dma_buf->dstVir, param.dst_size)) {
				ELOG("copy_to_user failed\n");
				return ret;
			}
			if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
				ELOG("copy_to_user failed\n");
				return -EFAULT;
			}
		}
	}

	return ret;
}

static long tcc_hsm_ioctl_gen_mac(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_mac_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_GEN_CMAC_VERIFY_CMD) {
		req = REQ_HSM_VERIFY_CMAC;
	} else if (cmd == HSM_GEN_GMAC_CMD) {
		req = REQ_HSM_GEN_GMAC;
	} else if (cmd == HSM_GEN_HMAC_CMD) {
		req = REQ_HSM_GEN_HMAC;
	} else if (cmd == HSM_GEN_SM3_HMAC_CMD) {
		req = REQ_HSM_GEN_SM3_HMAC;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_mac_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.dma) {
		ret = tcc_hsm_cmd_gen_mac(
			MBOX_DEV_HSM, req, param.obj_id, param.key, param.key_size, param.src, param.src_size,
			param.mac, param.mac_size);
	} else {
		if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.src, param.src_size)) {
			ELOG("copy_from_user failed\n");
			return ret;
		}

		ret = tcc_hsm_cmd_gen_mac(
			MBOX_DEV_HSM, req, param.obj_id, param.key, param.key_size, dma_buf->srcPhy,
			param.src_size, param.mac, param.mac_size);
	}

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.mac_size, DMA_FROM_DEVICE);

	if (ret != 0) {
		ELOG("tcc_hsm_ioctl_gen_mac fail(%d)\n", ret);
		return -EFAULT;
	} else {
		if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
			ELOG("copy_to_user failed\n");
			return ret;
		}
	}

	return ret;
}

static long tcc_hsm_ioctl_gen_mac_by_kt(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_mac_by_kt_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_GEN_CMAC_VERIFY_BY_KT_CMD) {
		req = REQ_HSM_VERIFY_CMAC_BY_KT;
	} else if (cmd == HSM_GEN_GMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_GMAC_BY_KT;
	} else if (cmd == HSM_GEN_HMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_HMAC_BY_KT;
	} else if (cmd == HSM_GEN_SM3_HMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_SM3_HMAC_BY_KT;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_mac_by_kt_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.dma) {
		ret = tcc_hsm_cmd_gen_mac_by_kt(
			MBOX_DEV_HSM, req, param.obj_id, param.key_index, param.src, param.src_size, param.mac,
			param.mac_size);
	} else {
		if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.src, param.src_size)) {
			ELOG("copy_from_user failed\n");
			return ret;
		}

		ret = tcc_hsm_cmd_gen_mac_by_kt(
			MBOX_DEV_HSM, req, param.obj_id, param.key_index, (uint32_t)dma_buf->srcPhy,
			param.src_size, param.mac, param.mac_size);
	}

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.mac_size, DMA_FROM_DEVICE);
	if (ret != 0) {
		ELOG("tcc_hsm_ioctl_gen_mac_by_kt fail(%d)\n", ret);
		return -EFAULT;
	} else {
		if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
			ELOG("copy_to_user failed\n");
			return ret;
		}
	}

	return ret;
}

static long tcc_hsm_ioctl_gen_hash(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_hash_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_GEN_SHA_CMD) {
		req = REQ_HSM_GEN_SHA;
	} else if (cmd == HSM_GEN_SM3_CMD) {
		req = REQ_HSM_GEN_SM3;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_hash_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.dma) {
		ret = tcc_hsm_cmd_gen_hash(
			MBOX_DEV_HSM, req, param.obj_id, param.src, param.src_size, (uint8_t *)dma_buf->dstPhy,
			param.digest_size);
	} else {
		if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.src, param.src_size)) {
			ELOG("copy_from_user failed\n");
			return ret;
		}

		ret = tcc_hsm_cmd_gen_hash(
			MBOX_DEV_HSM, req, param.obj_id, dma_buf->srcPhy, param.src_size, param.digest,
			param.digest_size);
	}

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.digest_size, DMA_FROM_DEVICE);
	if (ret != 0) {
		ELOG("tcc_hsm_cmd_run_aes fail(%d)\n", ret);
		return -EFAULT;
	} else {
		if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
			ELOG("copy_to_user failed\n");
			return ret;
		}
	}

	return ret;
}

static long tcc_hsm_ioctl_run_ecdsa(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_ecdsa_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_ecdsa_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	switch (cmd) {
	case HSM_RUN_ECDSA_SIGN_CMD:
		req = REQ_HSM_RUN_ECDSA_SIGN;
		ret = tcc_hsm_cmd_run_ecdsa(
			MBOX_DEV_HSM, req, param.obj_id, param.key, param.key_size, param.digest,
			param.digest_size, param.sig, param.sig_size);
		if (ret != 0) {
			ELOG("tcc_hsm_cmd_run_ecdsa fail(%d)\n", ret);
		} else {
			if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
				ELOG("copy_to_user failed\n");
				return -EFAULT;
			}
		}

		break;

	case HSM_RUN_ECDSA_VERIFY_CMD:
		req = REQ_HSM_RUN_ECDSA_VERIFY;
		ret = tcc_hsm_cmd_run_ecdsa(
			MBOX_DEV_HSM, req, param.obj_id, param.key, param.key_size, param.digest,
			param.digest_size, param.sig, param.sig_size);
		break;

	default:
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (ret) {
		ELOG("tcc_hsm_cmd_run_ecdsa Err(0x%x)\n", ret);
		return ret;
	}

	return ret;
}

static long tcc_hsm_ioctl_run_rsa(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_rsassa_param param = {0};
	uint32_t req = 0;
	int32_t ret = -EFAULT;

	if (cmd == HSM_RUN_RSASSA_PKCS_SIGN_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_SIGN;
	} else if (cmd == HSM_RUN_RSASSA_PKCS_VERIFY_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_VERIFY;
	} else if (cmd == HSM_RUN_RSASSA_PSS_SIGN_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_SIGN;
	} else if (cmd == HSM_RUN_RSASSA_PSS_VERIFY_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_VERIFY;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_rsassa_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	/* To handle key size exceeding 512 byte */
	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.key, param.key_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}
	ret = tcc_hsm_cmd_run_rsa(
		MBOX_DEV_HSM, req, param.obj_id, param.modN, param.modN_size, dma_buf->srcPhy,
		param.key_size, (uint8_t *)param.digest, param.digest_size, (uint8_t *)param.sign,
		param.sign_size);

	if (ret != 0) {
		ELOG("tcc_hsm_cmd_run_rsa fail(%d)\n", ret);
	} else {
		if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
			ELOG("copy_to_user failed\n");
			return -EFAULT;
		}
	}

	return ret;
}

static long tcc_hsm_ioctl_get_rng(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_rng_param param;
	uint32_t req = REQ_HSM_GET_RNG;
	int32_t ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_rng_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_hsm_cmd_get_rand(MBOX_DEV_HSM, req, dma_buf->dstPhy, param.rng_size);
	if (copy_to_user((void *)param.rng, (void *)dma_buf->dstVir, param.rng_size)) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}

	return ret;
}

static long tcc_hsm_ioctl_write(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_write_param param;
	uint32_t req = REQ_HSM_WRITE_OTP;
	int32_t ret = -EFAULT;

	if (cmd == HSM_WRITE_OTP_CMD) {
		req = REQ_HSM_WRITE_OTP;
	} else if (cmd == HSM_WRITE_SNOR_CMD) {
		req = REQ_HSM_WRITE_SNOR;
	} else {
		ELOG("cmd is invalid\n");
		return ret;
	}

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_write_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.data, param.data_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret =
		tcc_hsm_cmd_write(MBOX_DEV_HSM, req, param.addr, dma_buf->srcVir, param.data_size);
	if (ret) {
		ELOG("tcc_hsm_cmd_write failed\n");
		return -EFAULT;
	}
	return ret;
}

static long tcc_hsm_ioctl_get_version(unsigned int cmd, unsigned long arg)
{
	struct tcc_hsm_ioctl_version_param param;
	uint32_t req = REQ_HSM_GET_VER;
	int32_t ret = -EFAULT;

	ret = tcc_hsm_cmd_get_version(MBOX_DEV_HSM, req, &param.x, &param.y, &param.z);
	if (ret != 0) {
		ELOG("failed to get version\n");
		return -EFAULT;
	}
	if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}

	return ret;
}

static long tcc_hsm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int32_t ret = -EFAULT;

	DLOG("cmd=0x%x\n", cmd);

	mutex_lock(&tcc_hsm_mutex);

	switch (cmd) {
	case HSM_SET_KEY_FROM_OTP_CMD:
	case HSM_SET_KEY_FROM_SNOR_CMD:
		ret = tcc_hsm_ioctl_set_key(cmd, arg);
		break;

	case HSM_RUN_AES_CMD:
	case HSM_RUN_SM4_CMD:
		ret = tcc_hsm_ioctl_run_aes(cmd, arg);
		break;

	case HSM_RUN_AES_BY_KT_CMD:
	case HSM_RUN_SM4_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_aes_by_kt(cmd, arg);
		break;

	case HSM_GEN_CMAC_VERIFY_CMD:
	case HSM_GEN_GMAC_CMD:
	case HSM_GEN_HMAC_CMD:
	case HSM_GEN_SM3_HMAC_CMD:
		ret = tcc_hsm_ioctl_gen_mac(cmd, arg);
		break;

	case HSM_GEN_CMAC_VERIFY_BY_KT_CMD:
	case HSM_GEN_GMAC_BY_KT_CMD:
	case HSM_GEN_HMAC_BY_KT_CMD:
	case HSM_GEN_SM3_HMAC_BY_KT_CMD:
		ret = tcc_hsm_ioctl_gen_mac_by_kt(cmd, arg);
		break;

	case HSM_GEN_SHA_CMD:
	case HSM_GEN_SM3_CMD:
		ret = tcc_hsm_ioctl_gen_hash(cmd, arg);
		break;

	case HSM_RUN_ECDSA_SIGN_CMD:
	case HSM_RUN_ECDSA_VERIFY_CMD:
		ret = tcc_hsm_ioctl_run_ecdsa(cmd, arg);
		break;

	case HSM_RUN_RSASSA_PKCS_SIGN_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_CMD:
		ret = tcc_hsm_ioctl_run_rsa(cmd, arg);
		break;

	case HSM_WRITE_OTP_CMD:
	case HSM_WRITE_SNOR_CMD:
		ret = tcc_hsm_ioctl_write(cmd, arg);
		break;

	case HSM_GET_RNG_CMD:
		ret = tcc_hsm_ioctl_get_rng(cmd, arg);
		break;

	case HSM_GET_VER_CMD:
		ret = tcc_hsm_ioctl_get_version(cmd, arg);
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
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

static const struct file_operations tcc_hsm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tcc_hsm_ioctl,
	.compat_ioctl = tcc_hsm_ioctl,
	.open = tcc_hsm_open,
	.release = tcc_hsm_release,
};

static struct miscdevice tcc_hsm_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TCCHSM_DEVICE_NAME,
	.fops = &tcc_hsm_fops,
};

static int tcc_hsm_probe(struct platform_device *pdev)
{
	DLOG("\n");

	dma_buf = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hsm_dma_buf), GFP_KERNEL);
	if (dma_buf == NULL) {
		ELOG("failed to allocate dma_buf\n");
		return -ENOMEM;
	}

	dma_buf->dev = &pdev->dev;

	dma_buf->srcVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, &dma_buf->srcPhy, GFP_KERNEL);
	if (dma_buf->srcVir == NULL) {
		ELOG("failed to allocate dma_buf->srcVir\n");
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	dma_buf->dstVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, &dma_buf->dstPhy, GFP_KERNEL);
	if (dma_buf->dstVir == NULL) {
		ELOG("failed to allocate dma_buf->dstVir\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	if (misc_register(&tcc_hsm_miscdevice)) {
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
static struct of_device_id hsm_of_match[2] = {
	{.compatible = "telechips,tcc-hsm"},
	{"", "", "", NULL},
};
MODULE_DEVICE_TABLE(of, hsm_of_match);
#endif

static struct platform_driver tcc_hsm_driver = {
	.driver = {.name = "tcc_hsm",
			   .owner = THIS_MODULE,
#ifdef CONFIG_OF
			   .of_match_table = of_match_ptr(hsm_of_match)
#endif
	},
	.probe = tcc_hsm_probe,
	.remove = tcc_hsm_remove,
#ifdef CONFIG_PM
	.suspend = tcc_hsm_suspend,
	.resume = tcc_hsm_resume,
#endif
};

static int __init tcc_hsm_init(void)
{
	int ret = 0;

	DLOG("\n");

	ret = platform_driver_register(&tcc_hsm_driver);
	if (ret) {
		ELOG("[%s] platform_driver_register err(%d) \n", __func__, ret);
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
