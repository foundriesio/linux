/* linux/drivers/char/tcc_thsm.h
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

#ifndef _TCC_THSM_H_
#define _TCC_THSM_H_

#define TCCTHSM_DEVICE_NAME "tcc_thsm"

enum tcc_thsm_ioctl_cmd
{
	TCCTHSM_IOCTL_GET_VERSION,
	TCCTHSM_IOCTL_START,
	TCCTHSM_IOCTL_STOP,
	TCCTHSM_IOCTL_SET_MODE,
	TCCTHSM_IOCTL_SET_KEY,
	TCCTHSM_IOCTL_SET_IV,
	TCCTHSM_IOCTL_SET_KLDATA,
	TCCTHSM_IOCTL_RUN_CIPHER,
	TCCTHSM_IOCTL_RUN_CIPHER_BY_DMA,
	TCCTHSM_IOCTL_WRITE_OTP,
	TCCTHSM_IOCTL_GET_RNG,
	TCCTHSM_IOCTL_MAX
};

struct tcc_thsm_ioctl_version_param
{
	uint32_t major;
	uint32_t minor;
};

enum tcc_thsm_ioctl_cipher_algo
{
	NONE = 0,
	DVB_CSA2 = 1,
	DVB_CSA3 = 2,
	AES_128 = 3,
	DES = 4,
	TDES_128 = 5,
	Multi2 = 6,
};

enum tcc_thsm_ioctl_cipher_op_mode
{
	ECB = 0,
	CBC = 1,
	CTR_128 = 4,
	CTR_64 = 5,
};

struct tcc_thsm_ioctl_set_mode_param
{
	uint32_t keyIndex;
	uint32_t algorithm;
	uint32_t opMode;
	uint32_t residual;
	uint32_t sMsg;
};

enum tcc_thsm_ioctl_cipher_key_type
{
	CORE_Key = 0,
	Multi2_System_Key = 1,
	CMAC_Key = 2,
};

enum tcc_thsm_ioctl_cipher_key_mode
{
	TS_NOT_SCRAMBLED = 0,
	TS_RESERVED,
	TS_SCRAMBLED_WITH_EVENKEY,
	TS_SCRAMBLED_WITH_ODDKEY
};

struct tcc_thsm_ioctl_set_key_param
{
	uint32_t keyIndex;
	uint32_t keyType;
	uint32_t keyMode;
	uint32_t keySize;
	uint8_t *key;
};

struct tcc_thsm_ioctl_set_iv_param
{
	uint32_t keyIndex;
	uint32_t ivSize;
	uint8_t *iv;
};

struct tcc_thsm_kldata
{
	uint32_t klIndex;
	uint32_t nonceUsed;
	uint8_t Din1[16];
	uint8_t Din2[16];
	uint8_t Din3[16];
	uint8_t Din4[16];
	uint8_t Din5[16];
	uint8_t Din6[16];
	uint8_t Din7[16];
	uint8_t Din8[16];
	uint8_t nonce[16];
	uint8_t nonceResp[16];
};

struct tcc_thsm_ioctl_set_kldata_param
{
	uint32_t keyIndex;
	struct tcc_thsm_kldata *klData;
};

enum tcc_thsm_ioctl_cipher_enc
{
	DECRYPTION = 0,
	ENCRYPTION = 1,
};

enum tcc_thsm_ioctl_cipher_cw_sel
{
	TCKL = 0,
	CPU_Key = 1,
};

struct tcc_thsm_ioctl_run_cipher_param
{
	uint32_t keyIndex;
	uint8_t *srcAddr;
	uint8_t *dstAddr;
	uint32_t srcSize;
	uint32_t enc;
	uint32_t cwSel;
	uint32_t klIndex;
	uint32_t keyMode;
};

struct tcc_thsm_ioctl_otp_param
{
	uint32_t addr;
	uint8_t *buf;
	uint32_t size;
};

#define TCCTHSM_RNG_MAX 16
struct tcc_thsm_ioctl_rng_param
{
	uint8_t *rng;
	uint32_t size;
};

#endif /*_TCC_THSM_H_*/
