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

#define NDEBUG
#define TLOG_LEVEL TLOG_DEBUG
#include "tcc_thsm_log.h"

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

#include <linux/tcc_thsm.h>
#include <linux/uaccess.h>
#include "tcc_thsm_cmd.h"

/****************************************************************************
  DEFINITiON
 ****************************************************************************/
#define TCC_THSM_DMA_BUF_SIZE 4096

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
static uint32_t device_id = MBOX_DEV_A53;

/****************************************************************************
DEFINITION OF LOCAL FUNCTIONS
****************************************************************************/
static long tcc_thsm_ioctl_init(unsigned long arg)
{
	long ret = -EFAULT;

	ret = tcc_thsm_cmd_init(device_id);
	if (ret != 0) {
		ELOG("Failed to init\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_finalize(unsigned long arg)
{
	long ret = -EFAULT;

	ret = tcc_thsm_cmd_finalize(device_id);
	if (ret != 0) {
		ELOG("Failed to init\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_get_version(unsigned long arg)
{
	struct tcc_thsm_ioctl_version_param param;
	long ret = -EFAULT;

	ret = tcc_thsm_cmd_get_version(device_id, &param.major, &param.minor);
	if (ret != 0) {
		ELOG("failed to get version from multi device\n");
		return ret;
	}

	if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}

	return ret;
}

static long tcc_thsm_ioctl_set_mode(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_mode_param param;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_mdoe_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_set_mode(
		device_id, param.key_index, param.algorithm, param.op_mode, param.key_size);

	return ret;
}

static long tcc_thsm_ioctl_set_key(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_key_param param;
	uint32_t key1[32] = {0};
	uint32_t key2[32] = {0};
	uint32_t key3[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_key_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.key1 == NULL || param.key1_size < 0) {
		ELOG("param.key is null\n");
		return ret;
	}

	if (copy_from_user(key1, param.key1, param.key1_size)) {
		ELOG("copy_from_user failed(%d)\n", param.key1_size);
		return ret;
	}

	if (param.key2_size > 0) {
		if (copy_from_user(key2, param.key2, param.key2_size)) {
			ELOG("copy_from_user failed(%d)\n", param.key2_size);
			return ret;
		}
	}

	if (param.key3_size > 0) {
		if (copy_from_user(key3, param.key3, param.key3_size)) {
			ELOG("copy_from_user failed(%d)\n", param.key3_size);
			return ret;
		}
	}

	ret = tcc_thsm_cmd_set_key(
		device_id, param.key_index, param.key1, param.key1_size, param.key2, param.key2_size,
		param.key3, param.key3_size);

	return ret;
}

static long tcc_thsm_ioctl_set_key_from_storage(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_key_storage_param param;
	uint8_t obj_id[32] = {
		0,
	};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_set_key_storage_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.obj_id == NULL) {
		ELOG("param.obj_id is null\n");
		return ret;
	}

	if (copy_from_user(obj_id, (const uint8_t *)param.obj_id, param.obj_id_len)) {
		ELOG("copy_from_user failed(%d)\n", param.obj_id_len);
		return ret;
	}

	ret = tcc_thsm_cmd_set_key_from_storage(device_id, param.key_index, obj_id, param.obj_id_len);

	return ret;
}

static long tcc_thsm_ioctl_set_key_from_otp(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_key_otp_param param;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_set_key_otp_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_set_key_from_otp(device_id, param.key_index, param.otp_addr, param.key_size);

	return ret;
}

static long tcc_thsm_ioctl_free_mode(unsigned long arg)
{
	uint32_t key_index = 0;
	long ret = -EFAULT;

	key_index = arg;
	ret = tcc_thsm_cmd_free_mode(device_id, key_index);

	return ret;
}

static long tcc_thsm_ioctl_set_iv_symmetric(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_iv_param param;
	uint32_t iv[16] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_iv_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.iv == NULL) {
		ELOG("param.iv is null\n");
		return ret;
	}

	if (copy_from_user(iv, (const uint8_t *)param.iv, param.iv_size)) {
		ELOG("copy_from_user failed(%d)\n", param.iv_size);
		return ret;
	}

	ret = tcc_thsm_cmd_set_iv_symmetric(device_id, param.key_index, iv, param.iv_size);

	return ret;
}

static long tcc_thsm_ioctl_run_cipher(unsigned long arg)
{
	struct tcc_thsm_ioctl_cipher_param param;
	uint32_t dst_size = 0;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_cipher_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.src_addr, param.src_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_run_cipher_by_dma(
		device_id, param.key_index, (uint32_t)dma_buf->srcPhy, param.src_size,
		(uint32_t)dma_buf->dstPhy, &dst_size, param.flag);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.src_size, DMA_FROM_DEVICE);

	if (copy_to_user(param.dst_addr, (const uint8_t *)dma_buf->dstVir, param.src_size)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	if (copy_to_user(param.dst_size, &dst_size, sizeof(uint32_t))) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_run_cipher_by_dma(unsigned long arg)
{
	struct tcc_thsm_ioctl_cipher_dma_param param;
	uint32_t dst_size = 0;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_cipher_dma_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_run_cipher_by_dma(
		device_id, param.key_index, (uint32_t)param.src_addr, param.src_size,
		(uint32_t)param.dst_addr, &dst_size, param.flag);

	if (copy_to_user(param.dst_size, &dst_size, sizeof(uint32_t))) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_run_digest(unsigned long arg)
{
	struct tcc_thsm_ioctl_run_digest_param param;
	uint32_t hash_len = 0;
	uint8_t hash[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_run_digest_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.chunk, param.chunk_len)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_run_digest_by_dma(
		device_id, param.key_index, (uint32_t)dma_buf->srcPhy, param.chunk_len, hash, &hash_len,
		param.flag);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.chunk_len, DMA_FROM_DEVICE);

	if (copy_to_user(param.hash, hash, hash_len)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	if (copy_to_user(param.hash_len, &hash_len, sizeof(uint32_t))) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_run_digest_by_dma(unsigned long arg)
{
	struct tcc_thsm_ioctl_run_digest_dma_param param;
	uint32_t hash_len = 0;
	uint8_t hash[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_run_digest_dma_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_run_digest_by_dma(
		device_id, param.key_index, (uint32_t)param.chunk_addr, param.chunk_len, hash, &hash_len,
		param.flag);

	if (copy_to_user(param.hash, hash, hash_len)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	if (copy_to_user(param.hash_len, &hash_len, sizeof(uint32_t))) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_set_iv_mac(unsigned long arg)
{
	struct tcc_thsm_ioctl_set_iv_param param;
	uint32_t iv[16] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_set_iv_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.iv == NULL) {
		ELOG("param.iv is null\n");
		return ret;
	}

	if (copy_from_user(iv, (const uint8_t *)param.iv, param.iv_size)) {
		ELOG("copy_from_user failed(%d)\n", param.iv_size);
		return ret;
	}

	ret = tcc_thsm_cmd_set_iv_mac(device_id, param.key_index, iv, param.iv_size);

	return ret;
}

static long tcc_thsm_ioctl_compute_mac(unsigned long arg)
{
	struct tcc_thsm_ioctl_compute_mac_param param;
	uint32_t mac_len = 0;
	uint8_t mac[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_compute_mac_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.message, param.message_len)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_compute_mac_by_dma(
		device_id, param.key_index, (uint32_t)dma_buf->srcPhy, param.message_len, mac, &mac_len,
		param.flag);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.message_len, DMA_FROM_DEVICE);

	if (copy_to_user(param.mac, mac, mac_len)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}
	if (copy_to_user(param.mac_len, &mac_len, sizeof(uint32_t))) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_compute_mac_by_dma(unsigned long arg)
{
	struct tcc_thsm_ioctl_compute_mac_dma_param param;
	uint32_t mac_len = 0;
	uint8_t mac[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_compute_mac_dma_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_compute_mac_by_dma(
		device_id, param.key_index, (uint32_t)param.message_addr, param.message_len, mac, &mac_len,
		param.flag);

	if (copy_to_user(param.mac, mac, mac_len)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}
	if (copy_to_user(param.mac_len, &mac_len, sizeof(uint32_t))) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_compare_mac(unsigned long arg)
{
	struct tcc_thsm_ioctl_compare_mac_param param;
	uint8_t mac[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_compare_mac_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.message, param.message_len)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(mac, (const uint8_t *)param.mac, param.mac_len)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_compare_mac_by_dma(
		device_id, param.key_index, (uint32_t)dma_buf->srcPhy, param.message_len, mac,
		param.mac_len, param.flag);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.message_len, DMA_FROM_DEVICE);

	return ret;
}

static long tcc_thsm_ioctl_compare_mac_by_dma(unsigned long arg)
{
	struct tcc_thsm_ioctl_compare_mac_dma_param param;
	uint8_t mac[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_compare_mac_dma_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(mac, (const uint8_t *)param.mac, param.mac_len)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_compare_mac_by_dma(
		device_id, param.key_index, (uint32_t)param.message_addr, param.message_len, mac,
		param.mac_len, param.flag);

	return ret;
}

static long tcc_thsm_ioctl_get_rand(unsigned long arg)
{
	struct tcc_thsm_ioctl_rng_param param;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_rng_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.rng == NULL || param.size > TCCTHSM_RNG_MAX) {
		ELOG("invalid param(%p, %d)\n", param.rng, param.size);
		return ret;
	}

	ret = tcc_thsm_cmd_get_rand(device_id, (uint32_t *)dma_buf->dstVir, param.size);

	if (copy_to_user(param.rng, (const uint8_t *)dma_buf->dstVir, param.size)) {
		ELOG("copy_to_user failed\n");
		return ret;
	}

	return ret;
}

static long tcc_thsm_ioctl_gen_key_ss(unsigned long arg)
{
	struct tcc_thsm_ioctl_gen_key_param param;
	uint8_t obj_id[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_gen_key_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.obj_id == NULL) {
		ELOG("param. is null\n");
		return ret;
	}

	if (copy_from_user(obj_id, (const uint8_t *)param.obj_id, param.obj_len)) {
		ELOG("copy_from_user failed(%d)\n", param.obj_len);
		return ret;
	}

	ret =
		tcc_thsm_cmd_gen_key_ss(device_id, obj_id, param.obj_len, param.algorithm, param.key_size);

	return ret;
}

static long tcc_thsm_ioctl_del_key_ss(unsigned long arg)
{
	struct tcc_thsm_ioctl_del_key_param param;
	uint8_t obj_id[32] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_del_key_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.obj_id == NULL) {
		ELOG("param. is null\n");
		return ret;
	}

	if (copy_from_user(obj_id, (const uint8_t *)param.obj_id, param.obj_len)) {
		ELOG("copy_from_user failed(%d)\n", param.obj_len);
		return ret;
	}

	ret = tcc_thsm_cmd_del_key_ss(device_id, obj_id, param.obj_len);

	return ret;
}

static long tcc_thsm_ioctl_write_key_ss(unsigned long arg)
{
	struct tcc_thsm_ioctl_write_key_param param;
	uint8_t obj_id[32] = {0};
	uint8_t buffer[128] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_write_key_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (param.obj_id == NULL || param.buffer == NULL) {
		ELOG("param. is null\n");
		return ret;
	}

	if (copy_from_user(obj_id, (const uint8_t *)param.obj_id, param.obj_len)) {
		ELOG("copy_from_user failed(%d)\n", param.obj_len);
		return ret;
	}

	if (copy_from_user(buffer, (const uint8_t *)param.buffer, param.size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret =
		tcc_thsm_cmd_write_key_ss(device_id, obj_id, param.obj_len, (uint8_t *)buffer, param.size);

	return ret;
}
static long tcc_thsm_ioctl_write_otp(unsigned long arg)
{
	struct tcc_thsm_ioctl_otp_param param;
	uint8_t buffer[128] = {0};
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_otp_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(buffer, (const uint8_t *)param.buf, param.size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_write_otp(device_id, param.otp_addr, buffer, param.size);

	return ret;
}

static long tcc_thsm_ioctl_write_otpimage(unsigned long arg)
{
	struct tcc_thsm_ioctl_otpimage_param param;
	long ret = -EFAULT;

	if (copy_from_user(&param, (const struct tcc_thsm_ioctl_otpimage_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_write_otpimage(device_id, param.otp_addr, param.size);

	return ret;
}

static long tcc_thsm_ioctl_asym_enc_dec(unsigned long arg)
{
	struct tcc_thsm_ioctl_asym_enc_dec_param param;
	uint32_t dst_size = 0;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_asym_enc_dec_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dma_buf->srcVir, (const uint8_t *)param.src_addr, param.src_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_asym_enc_dec_by_dma(
		device_id, param.key_index, (uint32_t)dma_buf->srcPhy, param.src_size,
		(uint32_t)dma_buf->dstPhy, &dst_size, param.enc);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.src_size, DMA_FROM_DEVICE);

	if (copy_to_user(param.dst_addr, (const uint8_t *)dma_buf->dstVir, dst_size)) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}
	if (copy_to_user(param.dst_size, &dst_size, sizeof(dst_size))) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}

	return ret;
}

static long tcc_thsm_ioctl_asym_enc_dec_by_dma(unsigned long arg)
{
	struct tcc_thsm_ioctl_asym_enc_dec_dma_param param;
	uint32_t dst_size = 0;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_asym_enc_dec_dma_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_asym_enc_dec_by_dma(
		device_id, param.key_index, (uint32_t)param.src_addr, param.src_size,
		(uint32_t)param.dst_addr, &dst_size, param.enc);

	if (copy_to_user(param.dst_size, &dst_size, sizeof(dst_size))) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}

	return ret;
}

static long tcc_thsm_ioctl_asym_sign_digest(unsigned long arg)
{
	struct tcc_thsm_ioctl_asym_sign_digest_param param;
	uint8_t dig[512] = {0}, sig[128] = {0};
	uint32_t sig_size = 0;
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_asym_sign_digest_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dig, (const uint8_t *)param.dig, param.dig_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_asym_sign_digest(
		device_id, param.key_index, dig, param.dig_size, sig, &sig_size);

	if (copy_to_user(param.sig, sig, sig_size)) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}
	if (copy_to_user(param.sig_size, &sig_size, sizeof(sig_size))) {
		ELOG("copy_to_user failed\n");
		return -EFAULT;
	}

	return ret;
}

static long tcc_thsm_ioctl_asym_verify_digest(unsigned long arg)
{
	struct tcc_thsm_ioctl_asym_verify_digest_param param;
	uint8_t dig[512] = {0}, sig[128] = {0};
	long ret = -EFAULT;

	if (copy_from_user(
			&param, (const struct tcc_thsm_ioctl_asym_verify_digest_param *)arg, sizeof(param))) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(dig, (const uint8_t *)param.dig, param.dig_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	if (copy_from_user(sig, (const uint8_t *)param.sig, param.sig_size)) {
		ELOG("copy_from_user failed\n");
		return ret;
	}

	ret = tcc_thsm_cmd_asym_verify_digest(
		device_id, param.key_index, dig, param.dig_size, sig, param.sig_size);

	dma_sync_single_for_cpu(dma_buf->dev, dma_buf->dstPhy, param.dig_size, DMA_FROM_DEVICE);

	return ret;
}

static long tcc_thsm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EFAULT;

	DLOG("cmd=%d\n", cmd);

	mutex_lock(&tcc_thsm_mutex);

	switch (cmd) {
	case TCCTHSM_IOCTL_INIT:
		ret = tcc_thsm_ioctl_init(arg);
		break;

	case TCCTHSM_IOCTL_FINALIZE:
		ret = tcc_thsm_ioctl_finalize(arg);
		break;

	case TCCTHSM_IOCTL_GET_VERSION:
		ret = tcc_thsm_ioctl_get_version(arg);
		break;

	case TCCTHSM_IOCTL_SET_MODE:
		ret = tcc_thsm_ioctl_set_mode(arg);
		break;

	case TCCTHSM_IOCTL_SET_KEY:
		ret = tcc_thsm_ioctl_set_key(arg);
		break;

	case TCCTHSM_IOCTL_SET_KEY_FROM_STORAGE:
		ret = tcc_thsm_ioctl_set_key_from_storage(arg);
		break;

	case TCCTHSM_IOCTL_SET_KEY_FROM_OTP:
		ret = tcc_thsm_ioctl_set_key_from_otp(arg);
		break;

	case TCCTHSM_IOCTL_FREE_MODE:
		ret = tcc_thsm_ioctl_free_mode(arg);
		break;

	case TCCTHSM_IOCTL_SET_IV_SYMMETRIC:
		ret = tcc_thsm_ioctl_set_iv_symmetric(arg);
		break;

	case TCCTHSM_IOCTL_RUN_CIPHER:
		ret = tcc_thsm_ioctl_run_cipher(arg);
		break;

	case TCCTHSM_IOCTL_RUN_CIPHER_BY_DMA:
		ret = tcc_thsm_ioctl_run_cipher_by_dma(arg);
		break;

	case TCCTHSM_IOCTL_RUN_DIGEST:
		ret = tcc_thsm_ioctl_run_digest(arg);
		break;

	case TCCTHSM_IOCTL_RUN_DIGEST_BY_DMA:
		ret = tcc_thsm_ioctl_run_digest_by_dma(arg);
		break;

	case TCCTHSM_IOCTL_SET_IV_MAC:
		ret = tcc_thsm_ioctl_set_iv_mac(arg);
		break;

	case TCCTHSM_IOCTL_COMPUTE_MAC:
		ret = tcc_thsm_ioctl_compute_mac(arg);
		break;

	case TCCTHSM_IOCTL_COMPUTE_MAC_BY_DMA:
		ret = tcc_thsm_ioctl_compute_mac_by_dma(arg);
		break;

	case TCCTHSM_IOCTL_COMPARE_MAC:
		ret = tcc_thsm_ioctl_compare_mac(arg);
		break;

	case TCCTHSM_IOCTL_COMPARE_MAC_BY_DMA:
		ret = tcc_thsm_ioctl_compare_mac_by_dma(arg);
		break;

	case TCCTHSM_IOCTL_GET_RAND:
		ret = tcc_thsm_ioctl_get_rand(arg);
		break;

	case TCCTHSM_IOCTL_GEN_KEY_SS:
		ret = tcc_thsm_ioctl_gen_key_ss(arg);
		break;

	case TCCTHSM_IOCTL_DEL_KEY_SS:
		ret = tcc_thsm_ioctl_del_key_ss(arg);
		break;

	case TCCTHSM_IOCTL_WRITE_KEY_SS:
		ret = tcc_thsm_ioctl_write_key_ss(arg);
		break;

	case TCCTHSM_IOCTL_WRITE_OTP:
		ret = tcc_thsm_ioctl_write_otp(arg);
		break;

	case TCCTHSM_IOCTL_WRITE_OTP_IMAGE:
		ret = tcc_thsm_ioctl_write_otpimage(arg);
		break;

	case TCCTHSM_IOCTL_ASYMMETRIC_ENC_DEC:
		ret = tcc_thsm_ioctl_asym_enc_dec(arg);
		break;

	case TCCTHSM_IOCTL_ASYMMETRIC_ENC_DEC_BY_DMA:
		ret = tcc_thsm_ioctl_asym_enc_dec_by_dma(arg);
		break;

	case TCCTHSM_IOCTL_ASYMMETRIC_SIGN:
		ret = tcc_thsm_ioctl_asym_sign_digest(arg);
		break;

	case TCCTHSM_IOCTL_ASYMMETRIC_VERIFY:
		ret = tcc_thsm_ioctl_asym_verify_digest(arg);
		break;

	default:
		ELOG("unknown command(%d)\n", cmd);
		break;
	}

	mutex_unlock(&tcc_thsm_mutex);

	return ret;
}

int tcc_thsm_open(struct inode *inode, struct file *filp)
{
	DLOG("\n");

	return 0;
}

int tcc_thsm_release(struct inode *inode, struct file *file)
{
	DLOG("\n");

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
	DLOG("\n");

	dma_buf = devm_kzalloc(&pdev->dev, sizeof(struct tcc_thsm_dma_buf), GFP_KERNEL);
	if (dma_buf == NULL) {
		printk("%s failed to allocate dma_buf\n", __func__);
		return -ENOMEM;
	}

	dma_buf->dev = &pdev->dev;

	dma_buf->srcVir =
		dma_alloc_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, &dma_buf->srcPhy, GFP_KERNEL);
	if (dma_buf->srcVir == NULL) {
		DLOG("failed to allocate dma_buf->srcVir\n");
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	dma_buf->dstVir =
		dma_alloc_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, &dma_buf->dstPhy, GFP_KERNEL);
	if (dma_buf->dstVir == NULL) {
		DLOG("failed to allocate dma_buf->dstVir\n");
		dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -ENOMEM;
	}

	if (misc_register(&tcc_thsm_miscdevice)) {
		ELOG("register device err\n");
		dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->srcVir, dma_buf->srcPhy);
		dma_free_coherent(&pdev->dev, TCC_THSM_DMA_BUF_SIZE, dma_buf->dstVir, dma_buf->dstPhy);
		devm_kfree(&pdev->dev, dma_buf);
		return -EBUSY;
	}

	return 0;
}

static int tcc_thsm_remove(struct platform_device *pdev)
{
	DLOG("\n");

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
	DLOG("\n");
	return 0;
}

static int tcc_thsm_resume(struct platform_device *pdev)
{
	DLOG("\n");
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

	DLOG("\n");

	ret = platform_driver_register(&tcc_thsm_driver);
	if (ret) {
		ELOG("platform_driver_register err(%d) \n", ret);
		return 0;
	}

	return ret;
}

static void __exit tcc_thsm_exit(void)
{
	DLOG("\n");

	platform_driver_unregister(&tcc_thsm_driver);
}

module_init(tcc_thsm_init);
module_exit(tcc_thsm_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC THSM driver");
MODULE_LICENSE("GPL");
