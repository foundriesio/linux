/* linux/drivers/char/tcc_hsm_sp_cmd.h
 *
 * Copyright (C) 2009, 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _TCC_HSM_SP_CMD_H_
#define _TCC_HSM_SP_CMD_H_

// clang-format off
/* tccHSM command range: 0x4000 ~ 0x4FFF */
#define MAGIC_NUM                       (4)  // for HSM
#define TCCHSM_CMD_START                SP_CMD(MAGIC_NUM, 0x001)
#define TCCHSM_CMD_STOP                 SP_CMD(MAGIC_NUM, 0x002)
#define TCCHSM_CMD_GET_VERSION          SP_CMD(MAGIC_NUM, 0x003)
#define TCCHSM_CMD_SET_MODE             SP_CMD(MAGIC_NUM, 0x011)
#define TCCHSM_CMD_SET_KEY              SP_CMD(MAGIC_NUM, 0x012)
#define TCCHSM_CMD_SET_KEY_FROM_OTP     SP_CMD(MAGIC_NUM, 0x013)
#define TCCHSM_CMD_SET_IV               SP_CMD(MAGIC_NUM, 0x014)
#define TCCHSM_CMD_SET_KLDATA           SP_CMD(MAGIC_NUM, 0x017)
#define TCCHSM_CMD_RUN_CIPHER           SP_CMD(MAGIC_NUM, 0x018)
#define TCCHSM_CMD_RUN_CIPHER_BY_DMA    SP_CMD(MAGIC_NUM, 0x019)
#define TCCHSM_CMD_WRITE_OTP            SP_CMD(MAGIC_NUM, 0x032)
#define TCCHSM_CMD_GET_RNG              SP_CMD(MAGIC_NUM, 0x041)
// clang-format on

#define TCCHSM_CIPHER_KEYSIZE_FOR_64    8
#define TCCHSM_CIPHER_KEYSIZE_FOR_128   16
#define TCCHSM_CIPHER_KEYSIZE_FOR_192   24
#define TCCHSM_CIPHER_KEYSIZE_FOR_256   32

int32_t tcc_hsm_sp_cmd_start(void);
int32_t tcc_hsm_sp_cmd_stop(void);
int32_t tcc_hsm_sp_cmd_get_version(uint32_t device_id, uint32_t *major, uint32_t *minor);
int32_t tcc_hsm_sp_cmd_set_mode(
	uint32_t device_id, uint32_t keyIndex, uint32_t algorithm, uint32_t opMode, uint32_t residual,
	uint32_t sMsg);
int32_t tcc_hsm_sp_cmd_set_key(
	uint32_t device_id, uint32_t keyIndex, uint32_t keyType, uint32_t keyMode, uint8_t *key,
	uint32_t keySize);
int32_t tcc_hsm_sp_cmd_set_iv(uint32_t device_id, uint32_t keyIndex, uint8_t *iv, uint32_t ivSize);
int32_t tcc_hsm_sp_cmd_set_kldata(
	uint32_t device_id, uint32_t keyIndex, uintptr_t *klData, uint32_t klDataSize);
int32_t tcc_hsm_sp_cmd_run_cipher_by_dma(
	uint32_t device_id, uint32_t keyIndex, uint32_t srcAddr, uint32_t dstAddr, uint32_t srcSize,
	uint32_t enc, uint32_t swSel, uint32_t klIndex, uint32_t keyMode);
int32_t
tcc_hsm_sp_cmd_write_otp(uint32_t device_id, uint32_t otpAddr, uint8_t *otpBuf, uint32_t otpSize);
int32_t tcc_hsm_sp_cmd_get_rand(uint32_t device_id, uint8_t *rng, int32_t rngSize);

#endif /*_TCC_HSM_SP_CMD_H_*/
