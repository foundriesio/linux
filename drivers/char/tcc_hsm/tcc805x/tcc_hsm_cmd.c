/*
 * linux/drivers/char/tcc_hsm_cmd.c
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#include <linux/mailbox/tcc_sp_ioctl.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include "tcc_hsm_cmd.h"

#define DMA_MAX_RSIZE (1 * 1024 * 1024)
#define MBOX_LOCATION_DATA 0x0400
#define MBOX_LOCATION_CMD 0x0000

int32_t tcc_hsm_cmd_set_key(
	uint32_t device_id, uint32_t req, uint32_t otp_addr, uint32_t otp_size, uint32_t key_index)
{
	uint32_t data[128] = {0};
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = -1;

	data[0] = otp_addr;
	data[1] = otp_size;
	data[2] = key_index;

	data_size = (sizeof(uint32_t) * 3);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_run_aes(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint8_t *key, uint32_t key_size, uint8_t *iv,
	uint32_t iv_size, uint32_t src, uint32_t src_size, uint32_t dst, uint32_t dst_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = -1;

	data[index++] = obj_id;
	data[index++] = HSM_DMA;
	data[index++] = key_size;
	if (key != NULL && key_size > 0) {
		memcpy(&data[index], (uint32_t *)key, key_size);
		index += (key_size + 3) / sizeof(uint32_t);
	}

	data[index++] = iv_size;
	if (iv != NULL && iv_size > 0) {
		memcpy(&data[index], (uint32_t *)iv, iv_size);
		index += (iv_size + 3) / sizeof(uint32_t);
	}

	if (req == REQ_HSM_RUN_AES) {
		data[index++] = 0; // tag size
		data[index++] = 0; // add size
	}
	data[index++] = src_size;
	data[index++] = (uint32_t)src;
	data[index++] = dst_size;
	data[index++] = dst;

	data_size = (sizeof(uint32_t) * index);
	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_run_aes_by_kt(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint32_t key_index, uint8_t *iv,
	uint32_t iv_size, uint32_t src, uint32_t src_size, uint32_t dst, uint32_t dst_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = -1;

	data[index++] = obj_id;
	data[index++] = HSM_DMA;
	data[index++] = key_index;

	data[index++] = iv_size;
	if (iv != NULL && iv_size > 0) {
		memcpy(&data[index], iv, iv_size);
	}
	index += (iv_size + 3) / sizeof(uint32_t);

	data[index++] = 0; // tag size, Not used
	data[index++] = 0; // add size, Not used
	data[index++] = src_size;
	data[index++] = src;
	data[index++] = dst_size;
	data[index++] = dst;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_gen_mac(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint8_t *key, uint32_t key_size,
	uint32_t src, uint32_t src_size, uint8_t *mac, uint32_t mac_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	if ((req == REQ_HSM_GEN_HMAC) || (req == REQ_HSM_GEN_SM3_HMAC)) {
		data[index++] = obj_id;
	}
	data[index++] = HSM_DMA;
	data[index++] = key_size;
	if (key != NULL && key_size > 0) {
		memcpy(&data[index], key, key_size);
	}
	index += (key_size + 3) / sizeof(uint32_t);

	data[index++] = src_size;
	data[index++] = src;
	data[index++] = mac_size;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if (rdata[1] == mac_size) {
		memcpy(mac, (uint8_t *)&rdata[2], mac_size);
	} else {
		ELOG("wrong mac_size(%d)\n", rdata[1]);
		return -EBADR;
	}

	return result;
}

int32_t tcc_hsm_cmd_gen_mac_by_kt(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint32_t key_index, uint32_t src,
	uint32_t src_size, uint8_t *mac, uint32_t mac_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	if ((req == REQ_HSM_GEN_HMAC) || (req == REQ_HSM_GEN_SM3_HMAC)) {
		data[index++] = obj_id;
	}
	data[index++] = HSM_DMA;
	data[index++] = key_index;
	data[index++] = src_size;
	data[index++] = src;
	data[index++] = mac_size;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if (rdata[1] == mac_size) {
		memcpy(mac, (uint8_t *)&rdata[2], mac_size);
	} else {
		ELOG("wrong mac_size(%d)\n", rdata[1]);
		return -EBADR;
	}
	return result;
}

int32_t tcc_hsm_cmd_gen_hash(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint32_t src, uint32_t src_size,
	uint8_t *dig, uint32_t dig_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[index++] = obj_id;
	data[index++] = HSM_DMA;
	data[index++] = src_size;
	data[index++] = src;
	data[index++] = dig_size;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if (rdata[1] == dig_size) {
		memcpy(dig, (uint8_t *)&rdata[2], dig_size);
	} else {
		ELOG("wrong sig_size(%d)\n", rdata[1]);
		return -EBADR;
	}

	return result;
}

int32_t tcc_hsm_cmd_run_ecdsa(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint8_t *key, uint32_t key_size,
	uint8_t *digest, uint32_t digest_size, uint8_t *sig, uint32_t sig_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[index++] = obj_id;
	data[index++] = key_size;
	if (key != NULL && key_size > 0) {
		memcpy(&data[index], key, key_size);
		index += (key_size + 3) / sizeof(uint32_t);
	}
	data[index++] = digest_size;
	memcpy(&data[index], digest, digest_size);
	index += (digest_size + 3) / sizeof(uint32_t);

	data[index++] = sig_size;
	if (req == REQ_HSM_RUN_ECDSA_VERIFY) {
		memcpy(&data[index], sig, sig_size);
		index += (sig_size + 3) / sizeof(uint32_t);
	}

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if (req == REQ_HSM_RUN_ECDSA_VERIFY) {
		return result;
	} else if (req == REQ_HSM_RUN_ECDSA_SIGN) {
		if (rdata[1] == sig_size) {
			memcpy(sig, (uint8_t *)&rdata[2], sig_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_run_rsa(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint8_t *modN, uint32_t modN_size,
	uint32_t key, uint32_t key_size, uint8_t *digest, uint32_t digest_size, uint8_t *sig,
	uint32_t sig_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[index++] = obj_id;
	data[index++] = HSM_DMA;
	data[index++] = modN_size;
	memcpy(&data[index], modN, modN_size);
	index += (modN_size + 3) / sizeof(uint32_t);

	data[index++] = key_size;
	data[index++] = key;

	data[index++] = digest_size;
	memcpy(&data[index], digest, digest_size);
	index += (digest_size + 3) / sizeof(uint32_t);

	data[index++] = sig_size;
	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY) || (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY)) {
		memcpy(&data[index], sig, sig_size);
		index += (sig_size + 3) / sizeof(uint32_t);
	}
	data_size = (sizeof(uint32_t) * index);
	// ELOG("data_size=%d %d %d %d\n", data_size, modN_size, digest_size, sig_size);
	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY) || (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY)) {
		return result;
	} else if ((req == REQ_HSM_RUN_RSASSA_PKCS_SIGN) || (req == REQ_HSM_RUN_RSASSA_PSS_SIGN)) {
		if (rdata[1] == sig_size) {
			memcpy(sig, (uint8_t *)&rdata[2], sig_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_get_rand(uint32_t device_id, uint32_t req, uint32_t rng, int32_t rng_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[index++] = HSM_DMA;
	data[index++] = rng_size;
	data[index++] = rng;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_write_otp(
	uint32_t device_id, uint32_t req, uint32_t otp_addr, uint32_t otpBuf, uint32_t otp_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[index++] = otp_addr;
	data[index++] = otp_size;
	data[index++] = otpBuf;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_write_snor(
	uint32_t device_id, uint32_t req, uint32_t snorAddr, uint8_t *snorBuf, uint32_t snorSize)
{
	int32_t result = -1;
	//	req |= MBOX_LOCATION_DATA;

	return result;
}

int32_t
tcc_hsm_cmd_get_version(uint32_t device_id, uint32_t req, uint32_t *x, uint32_t *y, uint32_t *z)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = -1;

	data_size = (sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if (rdata[1] == sizeof(uint32_t) * 3) {
		*x = rdata[2];
		*y = rdata[3];
		*z = rdata[4];
	} else {
		ELOG("wrong sig_size(%d)\n", rdata[1]);
		return -EBADR;
	}

	return result;
}
