/* linux/drivers/char/tcc_hsm_cmd.h
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

#ifndef _TCC_HSM_CMD_H_
#define _TCC_HSM_CMD_H_

// clang-format off
#define REQ_HSM_SET_KEY_FROM_OTP            0x10000001
#define REQ_HSM_SET_KEY_FROM_SNOR           0x10000002

#define REQ_HSM_RUN_AES                     0x10010000
#define REQ_HSM_RUN_AES_BY_KT               0x10010001
#define REQ_HSM_RUN_SM4                     0x10020000
#define REQ_HSM_RUN_SM4_BY_KT               0x10020002

#define REQ_HSM_GEN_CMAC                    0x10110000
#define REQ_HSM_GEN_CMAC_BY_KT              0x10110001
#define REQ_HSM_GEN_GMAC                    0x10120000
#define REQ_HSM_GEN_GMAC_BY_KT              0x10120001
#define REQ_HSM_GEN_HMAC                    0x10130000
#define REQ_HSM_GEN_HMAC_BY_KT              0x10130001
#define REQ_HSM_GEN_SM3_HMAC                0x10140000
#define REQ_HSM_GEN_SM3_HMAC_BY_KT          0x10140001

#define REQ_HSM_GEN_SHA                     0x10210000
#define REQ_HSM_GEN_SM3                     0x10220000

#define REQ_HSM_RUN_ECDSA_SIGN              0x10310000
#define REQ_HSM_RUN_ECDSA_VERIFY            0x10320000

#define REQ_HSM_RUN_RSASSA_PKCS_SIGN        0x10550000
#define REQ_HSM_RUN_RSASSA_PKCS_VERIFY      0x10560000
#define REQ_HSM_RUN_RSASSA_PSS_SIGN         0x10570000
#define REQ_HSM_RUN_RSASSA_PSS_VERIFY       0x10580000

#define REQ_HSM_GET_RNG                     0x10610000

#define REQ_HSM_WRITE_OTP                   0x10710000
#define REQ_HSM_WRITE_SNOR                  0x10710001

#define REQ_HSM_GET_VER                     0x20010000

typedef enum _dma_type {
	HSM_NONE_DMA = 0,
	HSM_DMA
} dma_type;

// clang-format on

int32_t tcc_hsm_cmd_set_key(
	uint32_t device_id, uint32_t req, uint32_t otpAddr, uint32_t otpSize, uint32_t keyIndex);

int32_t tcc_hsm_cmd_run_aes(
	uint32_t device_id, uint32_t req, uint32_t objID, uint8_t *key, uint32_t keySize, uint8_t *iv,
	uint32_t ivSize, uint32_t src, uint32_t srcSize, uint32_t dst, uint32_t dstSize);
int32_t tcc_hsm_cmd_run_aes_by_kt(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint32_t key_index, uint8_t *iv,
	uint32_t iv_size, uint32_t src, uint32_t src_size, uint32_t dst, uint32_t dst_size);

int32_t tcc_hsm_cmd_gen_mac(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint8_t *key, uint32_t key_size,
	uint32_t src, uint32_t src_size, uint8_t *mac, uint32_t mac_size);
int32_t tcc_hsm_cmd_gen_mac_by_kt(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint32_t key_index, uint32_t src,
	uint32_t src_size, uint8_t *mac, uint32_t mac_size);

int32_t tcc_hsm_cmd_gen_hash(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint32_t src, uint32_t src_size,
	uint8_t *dig, uint32_t dig_size);

int32_t tcc_hsm_cmd_run_ecdsa(
	uint32_t device_id, uint32_t req, uint32_t objID, uint8_t *key, uint32_t keySize,
	uint8_t *digest, uint32_t digesteSize, uint8_t *sign, uint32_t signSize);

int32_t tcc_hsm_cmd_run_rsa(
	uint32_t device_id, uint32_t req, uint32_t obj_id, uint8_t *modN, uint32_t modN_size,
	uint32_t key, uint32_t key_size, uint8_t *digest, uint32_t digest_size, uint8_t *sig,
	uint32_t sig_size);

int32_t tcc_hsm_cmd_get_rand(uint32_t device_id, uint32_t req, uint32_t rng, int32_t rng_size);

int32_t tcc_hsm_cmd_write_otp(
	uint32_t device_id, uint32_t req, uint32_t otp_addr, uint32_t otpBuf, uint32_t otp_size);
int32_t tcc_hsm_cmd_write_snor(
	uint32_t device_id, uint32_t req, uint32_t snorAddr, uint8_t *snorBuf, uint32_t snorSize);

int32_t
tcc_hsm_cmd_get_version(uint32_t device_id, uint32_t req, uint32_t *x, uint32_t *y, uint32_t *z);

#endif /*_TCC_HSM_CMD_H_*/
