/* linux/drivers/char/tcc_thsm_cmd.h
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

#ifndef _TCC_THSM_CMD_H_
#define _TCC_THSM_CMD_H_

// clang-format off
/* tccTHSM command range: 0x4000 ~ 0x4FFF */
#define MAGIC_NUM                       (4)  // for THSM
#define TCCTHSM_EVT_START                THSM_EVENT(MAGIC_NUM, 0x001)
#define TCCTHSM_EVT_STOP                 THSM_EVENT(MAGIC_NUM, 0x002)
#define TCCTHSM_EVT_GET_VERSION          THSM_EVENT(MAGIC_NUM, 0x003)
#define TCCTHSM_EVT_SET_MODE             THSM_EVENT(MAGIC_NUM, 0x011)
#define TCCTHSM_EVT_SET_KEY              THSM_EVENT(MAGIC_NUM, 0x012)
#define TCCTHSM_EVT_SET_KEY_FROM_OTP     THSM_EVENT(MAGIC_NUM, 0x013)
#define TCCTHSM_EVT_SET_IV               THSM_EVENT(MAGIC_NUM, 0x014)
#define TCCTHSM_EVT_SET_KLDATA           THSM_EVENT(MAGIC_NUM, 0x017)
#define TCCTHSM_EVT_RUN_CIPHER           THSM_EVENT(MAGIC_NUM, 0x018)
#define TCCTHSM_EVT_RUN_CIPHER_BY_DMA    THSM_EVENT(MAGIC_NUM, 0x019)
#define TCCTHSM_EVT_WRITE_OTP            THSM_EVENT(MAGIC_NUM, 0x032)
#define TCCTHSM_EVT_GET_RNG              THSM_EVENT(MAGIC_NUM, 0x041)
// clang-format on

#define TCCTHSM_CIPHER_KEYSIZE_FOR_64 8
#define TCCTHSM_CIPHER_KEYSIZE_FOR_128 16
#define TCCTHSM_CIPHER_KEYSIZE_FOR_192 24
#define TCCTHSM_CIPHER_KEYSIZE_FOR_256 32

int32_t tcc_thsm_cmd_start(void);
int32_t tcc_thsm_cmd_stop(void);
int32_t tcc_thsm_cmd_get_version(uint32_t device_id, uint32_t *major, uint32_t *minor);
int32_t tcc_thsm_cmd_set_mode(
	uint32_t device_id, uint32_t keyIndex, uint32_t algorithm, uint32_t opMode, uint32_t residual,
	uint32_t sMsg);
int32_t tcc_thsm_cmd_set_key(
	uint32_t device_id, uint32_t keyIndex, uint32_t keyType, uint32_t keyMode, uint8_t *key,
	uint32_t keySize);
int32_t tcc_thsm_cmd_set_iv(uint32_t device_id, uint32_t keyIndex, uint8_t *iv, uint32_t ivSize);
int32_t tcc_thsm_cmd_set_kldata(
	uint32_t device_id, uint32_t keyIndex, uintptr_t *klData, uint32_t klDataSize);
int32_t tcc_thsm_cmd_run_cipher_by_dma(
	uint32_t device_id, uint32_t keyIndex, uint32_t srcAddr, uint32_t dstAddr, uint32_t srcSize,
	uint32_t enc, uint32_t swSel, uint32_t klIndex, uint32_t keyMode);
int32_t
tcc_thsm_cmd_write_otp(uint32_t device_id, uint32_t otpAddr, uint8_t *otpBuf, uint32_t otpSize);
int32_t tcc_thsm_cmd_get_rand(uint32_t device_id, uint8_t *rng, int32_t rngSize);

#endif /*_TCC_THSM_CMD_H_*/
