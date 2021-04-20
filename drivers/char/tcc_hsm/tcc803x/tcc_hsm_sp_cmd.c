/*
 * linux/drivers/char/tcc_hsm_sp_cmd.c
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

#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/div64.h>

#include "tcc_hsm_sp_cmd.h"

int32_t
tcc_hsm_sp_cmd_get_version(uint32_t device_id, uint32_t *major,
			   uint32_t *minor)
{
	int32_t mbox_data[2] = {
		0,
	};
	int32_t mbox_result[3] = {
		0,
	};
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_GET_VERSION, mbox_data,
			     sizeof(mbox_data), mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	*major = mbox_result[1];
	*minor = mbox_result[2];

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_set_mode(uint32_t device_id, uint32_t keyIndex,
				uint32_t algorithm, uint32_t opMode,
				uint32_t residual, uint32_t sMsg)
{
	int32_t mbox_data[5] = {
		0,
	};
	int32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = algorithm;
	mbox_data[2] = opMode;
	mbox_data[3] = residual;
	mbox_data[4] = sMsg;

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_MODE, mbox_data,
			     sizeof(mbox_data), &mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_set_key(uint32_t device_id, uint32_t keyIndex,
			       uint32_t keyType, uint32_t keyMode,
			       uint8_t *key, uint32_t keySize)
{
	int32_t mbox_data[12] = {
		0,
	};
	int32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = keyType;
	mbox_data[2] = keyMode;
	mbox_data[3] = keySize;

	if (key != NULL) {
		memcpy(&mbox_data[4], key, keySize);
	}

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_KEY, mbox_data,
			     sizeof(mbox_data), &mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_set_iv(uint32_t device_id, uint32_t keyIndex,
			      uint8_t *iv, uint32_t ivSize)
{
	int32_t mbox_data[11] = {
		0,
	};
	int32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = 1;
	mbox_data[2] = ivSize;

	if (iv != NULL) {
		memcpy(&mbox_data[3], iv, ivSize);
	}

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_IV, mbox_data,
			     sizeof(mbox_data), &mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_set_kldata(uint32_t device_id, uint32_t keyIndex,
				  uintptr_t *klData, uint32_t klDataSize)
{
	int32_t mbox_data[50] = {
		0,
	};
	int32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = klDataSize;

	if (klData != NULL) {
		memcpy(&mbox_data[2], klData, klDataSize);
	}

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_KLDATA, mbox_data,
			     sizeof(mbox_data), &mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_run_cipher_by_dma(uint32_t device_id, uint32_t keyIndex,
					 uint32_t srcAddr, uint32_t dstAddr,
					 uint32_t srcSize, uint32_t enc,
					 uint32_t cwSel, uint32_t klIndex,
					 uint32_t keyMode)
{
	int32_t mbox_data[12] = {
		0,
	};
	int32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = srcAddr;
	mbox_data[2] = dstAddr;
	mbox_data[3] = srcSize;
	mbox_data[4] = 0;
	mbox_data[5] = 0;
	mbox_data[6] = enc;
	mbox_data[7] = cwSel;
	mbox_data[8] = klIndex;
	mbox_data[9] = keyMode;
	mbox_data[10] = 0;
	mbox_data[11] = 0;

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_RUN_CIPHER_BY_DMA, mbox_data,
			     sizeof(mbox_data), &mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_run_cmac(
	uint32_t device_id, uint32_t keyIndex, uint32_t flag, uint8_t *srcAddr,
	uint32_t srcSize, uint8_t *macAddr, uint32_t *macSize)
{
	int32_t mbox_data[20] = {0};
	int32_t mbox_result[12] = {0};
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = flag;
	mbox_data[2] = srcSize;

	memcpy(&mbox_data[3], srcAddr, srcSize);

	mbox_result_size = sec_sendrecv_cmd(
		device_id, TCCHSM_CMD_RUN_CMAC, mbox_data, sizeof(mbox_data),
		mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	*macSize = mbox_result[1];
	memcpy((void *)macAddr, (const void *)&mbox_result[2], *macSize);

out:
	return result;
}

int32_t tcc_hsm_sp_cmd_write_otp(uint32_t device_id, uint32_t otpAddr,
				 uint8_t *otpBuf, uint32_t otpSize)
{
	int32_t mbox_data[32] = {
		0,
	};
	int32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data[0] = otpAddr;
	mbox_data[1] = otpSize;

	memcpy(&mbox_data[2], otpBuf, otpSize);

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_WRITE_OTP, mbox_data,
			     sizeof(mbox_data), &mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}

int32_t
tcc_hsm_sp_cmd_get_rand(uint32_t device_id, uint8_t *rng, int32_t rngSize)
{
	int32_t mbox_data = 0;
	int32_t mbox_result[16] = {
		0,
	};
	int32_t mbox_result_size = 0;
	int32_t result = 0;

	mbox_data = rngSize;

	mbox_result_size =
	    sec_sendrecv_cmd(device_id, TCCHSM_CMD_GET_RNG, &mbox_data,
			     sizeof(mbox_data), mbox_result,
			     sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = -EBADR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	rngSize = mbox_result[1];
	memcpy(rng, &mbox_result[2], rngSize);

out:
	return result;
}
