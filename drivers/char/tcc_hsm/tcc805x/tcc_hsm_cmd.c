/*
 *******************************************************************************
 *
 *   FileName : tcc_hsm_cmd.c
 *
 *   Copyright (c) Telechips Inc.
 *
 *   Description : TCC HSM CMD
 *
 *
 *******************************************************************************
 *
 *   TCC Version 1.0
 *
 *   This source code contains confidential information of Telechips.
 *
 *   Any unauthorized use without a written permission of Telechips including
 *not limited to re-distribution in source or binary form is strictly
 *prohibited.
 *
 *   This source code is provided "AS IS" and nothing contained in this source
 *code shall constitute any express or implied warranty of any kind, including
 *without limitation, any warranty of merchantability, fitness for a
 *particular purpose or non-infringement of any patent, copyright or other
 *third party intellectual property right. No warranty is made, express or
 *implied, regarding the information's accuracy,completeness, or performance.
 *
 *   In no event shall Telechips be liable for any claim, damages or other
 *liability arising from, out of or in connection with this source code or the
 *use in the source code.
 *
 *   This source code is provided subject to the terms of a Mutual
 *Non-Disclosure Agreement between Telechips and Company. This source code is
 *provided "AS IS" and nothing contained in this source code shall constitute
 *any express or implied warranty of any kind, including without limitation, any
 *warranty (of merchantability, fitness for a particular purpose or
 *non-infringement of any patent, copyright or other third party intellectual
 *property right. No warranty is made, express or implied, regarding the
 *information's accuracy, completeness, or performance. In no event shall
 *Telechips be liable for any claim, damages or other liability arising from,
 *out of or in connection with this source code or the use in the source code.
 *This source code is provided subject to the terms of a Mutual Non-Disclosure
 *Agreement between Telechips and Company.
 *
 *******************************************************************************
 */

#if 0
#define NDEBUG
#endif
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

#include "tcc_hsm_cmd.h"

/****************************************************************************
 * DEFINITiON
 ****************************************************************************/
#define DMA_MAX_RSIZE (1U * 1024U * 1024U)
#define MBOX_LOCATION_DATA (0x0400U)
#define MBOX_LOCATION_CMD (0x0000U)

/****************************************************************************
 * DEFINITION OF LOCAL FUNCTIONS
 ****************************************************************************/
int32_t tcc_hsm_cmd_set_key(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_set_key_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		return HSM_INVALID_PARAM;
	}

	data[index] = param->addr;
	index++;
	data[index] = param->data_size;
	index++;
	data[index] = param->key_index;
	index++;

	data_size = (uint32_t)(sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_run_aes(
	uint32_t device_id, uint32_t req, struct tcc_hsm_ioctl_aes_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[12] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data[index] = param->obj_id;
	index++;
	data[index] = HSM_DMA;
	index++;
	data[index] = param->key_size;
	index++;

	if (param->key_size > 0U) {
		memcpy((void *)&data[index], (const void *)param->key,
		       param->key_size);
		index += ((param->key_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data[index] = param->iv_size;
	index++;
	if (param->iv_size > 0U) {
		memcpy((void *)&data[index], (const void *)param->iv,
		       param->iv_size);
		index += ((param->iv_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	if (req == REQ_HSM_RUN_AES) {
		data[index] = param->tag_size;
		index++;
		data[index] = param->aad_size;
		index++;
		memcpy((void *)&data[index], (const void *)param->aad,
		       param->aad_size);
		index += ((param->aad_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data[index] = param->src_size;
	index++;
	data[index] = (uint32_t)param->src;
	index++;
	data[index] = param->dst_size;
	index++;
	data[index] = (uint32_t)param->dst;
	index++;

	data_size = (uint32_t)(sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if ((req == REQ_HSM_RUN_AES) && (param->tag_size > 0U)) {
		if (rdata[1] == (int32_t)param->tag_size) {
			memcpy((void *)param->tag, (const void *)&rdata[2],
			       param->tag_size);
		} else {
			ELOG("wrong tag_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_run_aes_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_aes_by_kt_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[12] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data[index] = param->obj_id;
	index++;
	data[index] = HSM_DMA;
	index++;
	data[index] = param->key_index;
	index++;

	data[index] = param->iv_size;
	index++;
	if (param->iv_size > 0U) {
		memcpy((void *)&data[index], (const void *)param->iv,
		       param->iv_size);
	}
	index += ((param->iv_size + 3U) / (uint32_t)sizeof(uint32_t));

	if (req == REQ_HSM_RUN_AES_BY_KT) {
		data[index] = param->tag_size;
		index++;
		data[index] = param->aad_size;
		index++;
		memcpy((void *)&data[index], (const void *)param->aad,
		       param->aad_size);
		index += ((param->aad_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data[index] = param->src_size;
	index++;
	data[index] = (uint32_t)param->src;
	index++;
	data[index] = param->dst_size;
	index++;
	data[index] = (uint32_t)param->dst;
	index++;

	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if ((req == REQ_HSM_RUN_AES_BY_KT) && (param->tag_size > 0U)) {
		if (rdata[1] == (int32_t)param->tag_size) {
			memcpy((void *)param->tag, (const void *)&rdata[2],
			       param->tag_size);
		} else {
			ELOG("wrong tag_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_gen_mac(
	uint32_t device_id, uint32_t req, struct tcc_hsm_ioctl_mac_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	if ((req == REQ_HSM_GEN_HMAC) || (req == REQ_HSM_GEN_SM3_HMAC)) {
		data[index] = param->obj_id;
		index++;
	}
	data[index] = HSM_DMA;
	index++;
	data[index] = param->key_size;
	index++;
	if (param->key_size > 0U) {
		memcpy((void *)&data[index], (const void *)param->key,
		       param->key_size);
	}
	index += ((param->key_size + 3U) / (uint32_t)sizeof(uint32_t));

	data[index] = param->src_size;
	index++;
	data[index] = (uint32_t)param->src;
	index++;
	data[index] = param->mac_size;
	index++;

	if (req == REQ_HSM_VERIFY_CMAC) {
		memcpy((void *)&data[index], (const void *)param->mac,
		       param->mac_size);
		index += ((param->mac_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data_size = ((uint32_t)sizeof(uint32_t) * index);
	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if (req != REQ_HSM_VERIFY_CMAC) {
		if (rdata[1] == (int32_t)param->mac_size) {
			memcpy((void *)param->mac, (const void *)&rdata[2],
			       param->mac_size);
		} else {
			ELOG("wrong mac_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_gen_mac_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_mac_by_kt_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	if ((req == REQ_HSM_GEN_HMAC_BY_KT)
	    || (req == REQ_HSM_GEN_SM3_HMAC_BY_KT)) {
		data[index] = param->obj_id;
		index++;
	}
	data[index] = HSM_DMA;
	index++;
	data[index] = param->key_index;
	index++;
	data[index] = param->src_size;
	index++;
	data[index] = (uint32_t)param->src;
	index++;
	data[index] = param->mac_size;
	index++;

	if (req == REQ_HSM_VERIFY_CMAC_BY_KT) {
		memcpy((void *)&data[index], (const void *)param->mac,
		       param->mac_size);
		index += ((param->mac_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data_size = ((uint32_t)sizeof(uint32_t) * index);
	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if (req != REQ_HSM_VERIFY_CMAC_BY_KT) {
		if (rdata[1] == (int32_t)param->mac_size) {
			memcpy((void *)param->mac, (const void *)&rdata[2],
			       param->mac_size);
		} else {
			ELOG("wrong mac_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_gen_hash(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_hash_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data[index] = param->obj_id;
	index++;
	data[index] = HSM_DMA;
	index++;
	data[index] = param->src_size;
	index++;
	data[index] = (uint32_t)param->src;
	index++;
	data[index] = param->digest_size;
	index++;

	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if (rdata[1] == (int32_t)param->digest_size) {
		memcpy((void *)param->digest, (const void *)&rdata[2],
		       param->digest_size);
	} else {
		ELOG("wrong sig_size(%d)\n", rdata[1]);
		return -EBADR;
	}

	return result;
}

int32_t tcc_hsm_cmd_run_ecdh_phaseI(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdh_key_param *param)
{
	uint32_t data[4] = {0};
	uint32_t index = 0;
	int32_t rdata[64] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = 0;

	data[index] = param->key_type;
	index++;
	data[index] = param->obj_id;
	index++;
	data[index] = param->prikey_size;
	index++;
	data[index] = param->pubkey_size;
	index++;
	DLOG("key_type=0x%x obj_id=0x%x prikey_size=0x%x pubkey_size=0x%x\n",
	     param->key_type, param->obj_id, param->prikey_size,
	     param->pubkey_size);
	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	index = 0;

	result = rdata[index];
	index++;
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if (rdata[index]
	    == (int32_t)(param->prikey_size + param->pubkey_size)) {
		index++;
		memcpy((void *)param->prikey, (const void *)&rdata[index],
		       param->prikey_size);
		index +=
			((param->prikey_size + 3U)
			 / (uint32_t)sizeof(uint32_t));

		memcpy((void *)param->pubkey, (const void *)&rdata[index],
		       param->pubkey_size);
		index +=
			((param->pubkey_size + 3U)
			 / (uint32_t)sizeof(uint32_t));
	} else {
		ELOG("wrong prikey_size(%d)\n", rdata[index]);
		return -EBADR;
	}

	return result;
}

int32_t tcc_hsm_cmd_run_ecdsa(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdsa_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data[index] = param->obj_id;
	index++;
	data[index] = param->key_size;
	index++;

	if (param->key_size > 0U) {
		memcpy((void *)&data[index], (const void *)param->key,
		       param->key_size);
		index += ((param->key_size + 3U) / (uint32_t)sizeof(uint32_t));
	}
	data[index] = param->digest_size;
	index++;
	memcpy((void *)&data[index], (const void *)param->digest,
	       param->digest_size);
	index += ((param->digest_size + 3U) / (uint32_t)sizeof(uint32_t));

	data[index] = param->sig_size;
	index++;
	if (req == REQ_HSM_RUN_ECDSA_VERIFY) {
		memcpy(&data[index], param->sig, param->sig_size);
		index += ((param->sig_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if (req == REQ_HSM_RUN_ECDSA_SIGN) {
		if (rdata[1] == (int32_t)param->sig_size) {
			memcpy((void *)param->sig, (const void *)&rdata[2],
			       param->sig_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_run_rsa(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_rsassa_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data[index] = param->obj_id;
	index++;
	data[index] = HSM_DMA;
	index++;
	data[index] = param->modN_size;
	index++;
	memcpy((void *)&data[index], (const void *)param->modN,
	       param->modN_size);
	index += ((param->modN_size + 3U) / (uint32_t)sizeof(uint32_t));

	data[index] = param->key_size;
	index++;
	data[index] = (uint32_t)param->key;
	index++;

	data[index] = param->digest_size;
	index++;
	memcpy((void *)&data[index], (const void *)param->digest,
	       param->digest_size);
	index += ((param->digest_size + 3U) / (uint32_t)sizeof(uint32_t));

	data[index] = param->sign_size;
	index++;
	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY)
	    || (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY)) {
		memcpy((void *)&data[index], (const void *)param->sign,
		       param->sign_size);
		index += ((param->sign_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}
	if ((req == REQ_HSM_RUN_RSASSA_PKCS_SIGN)
	    || (req == REQ_HSM_RUN_RSASSA_PSS_SIGN)) {
		if (rdata[1] == (int32_t)param->sign_size) {
			memcpy((void *)param->sign, (const void *)&rdata[2],
			       param->sign_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_hsm_cmd_write(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_write_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data[index] = param->addr;
	index++;
	data[index] = param->data_size;
	index++;

	if (param->data_size > 0U) {
		memcpy((void *)&data[index], (const void *)param->data,
		       param->data_size);
		index += ((param->data_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	data_size = (uint32_t)(sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}

int32_t tcc_hsm_cmd_get_version(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_version_param *param)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata[128] = {0};
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = HSM_GENERAL_FAIL;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		return HSM_INVALID_PARAM;
	}

	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	if ((uint32_t)rdata[1] == ((uint32_t)sizeof(uint32_t) * 3U)) {
		param->x = (uint32_t)rdata[2];
		param->y = (uint32_t)rdata[3];
		param->z = (uint32_t)rdata[4];
	} else {
		ELOG("wrong sig_size(%d)\n", rdata[1]);
		return -EBADR;
	}

	return result;
}

int32_t tcc_hsm_cmd_get_rand(
	uint32_t device_id, uint32_t req, uint32_t rng, uint32_t rng_size)
{
	uint32_t data[128] = {0};
	uint32_t index = 0;
	int32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	int32_t result = 0;

	data[index] = HSM_DMA;
	index++;
	data[index] = rng_size;
	index++;
	data[index] = rng;
	index++;

	data_size = ((uint32_t)sizeof(uint32_t) * index);

	rdata_size = sec_sendrecv_cmd(
		device_id, (req | MBOX_LOCATION_DATA), data, data_size, &rdata,
		DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != HSM_OK) {
		ELOG("Error: 0x%x\n", result);
		return result;
	}

	return result;
}
