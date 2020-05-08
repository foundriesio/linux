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
#define TCCTHSM_RNG_MAX 16

enum tcc_thsm_ioctl_cmd
{
	TCCTHSM_IOCTL_INIT,
	TCCTHSM_IOCTL_FINALIZE,
	TCCTHSM_IOCTL_GET_VERSION,
	TCCTHSM_IOCTL_SET_MODE,
	TCCTHSM_IOCTL_SET_KEY,
	TCCTHSM_IOCTL_SET_KEY_FROM_STORAGE,
	TCCTHSM_IOCTL_SET_KEY_FROM_OTP,
	TCCTHSM_IOCTL_FREE_MODE,
	TCCTHSM_IOCTL_SET_IV_SYMMETRIC,
	TCCTHSM_IOCTL_RUN_CIPHER,
	TCCTHSM_IOCTL_RUN_CIPHER_BY_DMA,
	TCCTHSM_IOCTL_RUN_DIGEST,
	TCCTHSM_IOCTL_RUN_DIGEST_BY_DMA,
	TCCTHSM_IOCTL_SET_IV_MAC,
	TCCTHSM_IOCTL_COMPUTE_MAC,
	TCCTHSM_IOCTL_COMPUTE_MAC_BY_DMA,
	TCCTHSM_IOCTL_COMPARE_MAC,
	TCCTHSM_IOCTL_COMPARE_MAC_BY_DMA,
	TCCTHSM_IOCTL_GET_RAND,
	TCCTHSM_IOCTL_GEN_KEY_SS,
	TCCTHSM_IOCTL_DEL_KEY_SS,
	TCCTHSM_IOCTL_WRITE_KEY_SS,
	TCCTHSM_IOCTL_WRITE_OTP,
	TCCTHSM_IOCTL_WRITE_OTP_IMAGE,
	TCCTHSM_IOCTL_ASYMMETRIC_ENC_DEC,
	TCCTHSM_IOCTL_ASYMMETRIC_ENC_DEC_BY_DMA,
	TCCTHSM_IOCTL_ASYMMETRIC_SIGN,
	TCCTHSM_IOCTL_ASYMMETRIC_VERIFY
};

enum tcc_thsm_ioctl_cipher_cw_sel
{
	TCKL = 0,
	CPU_Key = 1,
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

struct tcc_thsm_ioctl_version_param
{
	uint32_t major;
	uint32_t minor;
};

struct tcc_thsm_ioctl_set_mode_param
{
	uint32_t key_index;
	uint32_t algorithm;
	uint32_t op_mode;
	uint32_t key_size;
};

struct tcc_thsm_ioctl_set_key_param
{
	uint32_t key_index;
	uint32_t *key1;
	uint32_t key1_size;
	uint32_t *key2;
	uint32_t key2_size;
	uint32_t *key3;
	uint32_t key3_size;
};

struct tcc_thsm_ioctl_set_key_storage_param
{
	uint32_t key_index;
	uint8_t *obj_id;
	uint32_t obj_id_len;
};

struct tcc_thsm_ioctl_set_key_otp_param
{
	uint32_t key_index;
	uint32_t otp_addr;
	uint32_t key_size;
};

struct tcc_thsm_ioctl_set_iv_param
{
	uint32_t key_index;
	uint32_t *iv;
	uint32_t iv_size;
};

struct tcc_thsm_ioctl_cipher_param
{
	uint32_t key_index;
	uint8_t *src_addr;
	uint32_t src_size;
	uint8_t *dst_addr;
	uint32_t *dst_size;
	uint32_t flag;
};

struct tcc_thsm_ioctl_cipher_dma_param
{
	uint32_t key_index;
	uint32_t src_addr;
	uint32_t src_size;
	uint32_t dst_addr;
	uint32_t *dst_size;
	uint32_t flag;
};

struct tcc_thsm_ioctl_run_digest_param
{
	uint32_t key_index;
	uint8_t *chunk;
	uint32_t chunk_len;
	uint8_t *hash;
	uint32_t *hash_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_run_digest_dma_param
{
	uint32_t key_index;
	uint32_t chunk_addr;
	uint32_t chunk_len;
	uint8_t *hash;
	uint32_t *hash_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compute_mac_param
{
	uint32_t key_index;
	uint8_t *message;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t *mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compute_mac_dma_param
{
	uint32_t key_index;
	uint32_t message_addr;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t *mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compare_mac_param
{
	uint32_t key_index;
	uint8_t *message;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compare_mac_dma_param
{
	uint32_t key_index;
	uint32_t message_addr;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_rng_param
{
	uint32_t *rng;
	uint32_t size;
};

struct tcc_thsm_ioctl_gen_key_param
{
	uint8_t *obj_id;
	uint32_t obj_len;
	uint32_t algorithm;
	uint32_t key_size;
};

struct tcc_thsm_ioctl_del_key_param
{
	uint8_t *obj_id;
	uint32_t obj_len;
};

struct tcc_thsm_ioctl_write_key_param
{
	uint8_t *obj_id;
	uint32_t obj_len;
	uint8_t *buffer;
	uint32_t size;
};

struct tcc_thsm_ioctl_otp_param
{
	uint32_t otp_addr;
	uint8_t *buf;
	uint32_t size;
};

struct tcc_thsm_ioctl_otpimage_param
{
	uint32_t otp_addr;
	uint32_t size;
};

struct tcc_thsm_ioctl_asym_enc_dec_param
{
	uint32_t key_index;
	uint8_t *src_addr;
	uint32_t src_size;
	uint8_t *dst_addr;
	uint32_t *dst_size;
	uint32_t enc;
};

struct tcc_thsm_ioctl_asym_enc_dec_dma_param
{
	uint32_t key_index;
	uint32_t src_addr;
	uint32_t src_size;
	uint32_t dst_addr;
	uint32_t *dst_size;
	uint32_t enc;
};

struct tcc_thsm_ioctl_asym_sign_digest_param
{
	uint32_t key_index;
	uint8_t *dig;
	uint32_t dig_size;
	uint8_t *sig;
	uint32_t *sig_size;
};

struct tcc_thsm_ioctl_asym_verify_digest_param
{
	uint32_t key_index;
	uint8_t *dig;
	uint32_t dig_size;
	uint8_t *sig;
	uint32_t sig_size;
};

#endif /*_TCC_THSM_H_*/
