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

#define THSM_EVENT(magic_num, evt) (((magic_num & 0xF) << 12) | (evt & 0xFFF))
/* tccTHSM command range: 0x5000 ~ 0x5FFF */
// clang-format off
#define MAGIC_NUM                        (5)  // THSM magic number
#define TCCTHSM_EVT_INIT        	  	 THSM_EVENT(MAGIC_NUM, 0x011) // Generic
#define TCCTHSM_EVT_FINALIZE       		 THSM_EVENT(MAGIC_NUM, 0x012) // Generic
#define TCCTHSM_EVT_GET_VERSION          THSM_EVENT(MAGIC_NUM, 0x013) // Generic
#define TCCTHSM_EVT_SET_MODE             THSM_EVENT(MAGIC_NUM, 0x014) // Generic
#define TCCTHSM_EVT_SET_KEY              THSM_EVENT(MAGIC_NUM, 0x015) // Generic
#define TCCTHSM_EVT_SET_KEY_FROM_STORAGE THSM_EVENT(MAGIC_NUM, 0x016) // Generic
#define TCCTHSM_EVT_SET_KEY_FROM_OTP     THSM_EVENT(MAGIC_NUM, 0x017) // Generic
#define TCCTHSM_EVT_FREE_MODE            THSM_EVENT(MAGIC_NUM, 0x018) // Generic
#define TCCTHSM_EVT_SET_IV_SYMMETRIC     THSM_EVENT(MAGIC_NUM, 0x021) // Symmetric Cipher
#define TCCTHSM_EVT_RUN_CIPHER           THSM_EVENT(MAGIC_NUM, 0x022) // Symmetric Cipher
#define TCCTHSM_EVT_RUN_DIGEST           THSM_EVENT(MAGIC_NUM, 0x031) // Message Digest (SHA)
#define TCCTHSM_EVT_SET_IV_MAC           THSM_EVENT(MAGIC_NUM, 0x041) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_COMPUTE_MAC          THSM_EVENT(MAGIC_NUM, 0x042) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_COMPARE_MAC          THSM_EVENT(MAGIC_NUM, 0x043) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_GET_RAND             THSM_EVENT(MAGIC_NUM, 0x051) // RNG
#define TCCTHSM_EVT_GEN_KEY_SS     	     THSM_EVENT(MAGIC_NUM, 0x061) // Key Generate
#define TCCTHSM_EVT_DEL_KEY_SS           THSM_EVENT(MAGIC_NUM, 0x062) // Key Generate
#define TCCTHSM_EVT_WRITE_KEY_SS         THSM_EVENT(MAGIC_NUM, 0x063) // Key Generate
#define TCCTHSM_EVT_WRITE_OTP            THSM_EVENT(MAGIC_NUM, 0x071) // OTP
#define TCCTHSM_EVT_WRITE_OTP_IMAGE      THSM_EVENT(MAGIC_NUM, 0x072) // OTP
#define TCCTHSM_EVT_ASYMMETRIC_ENC       THSM_EVENT(MAGIC_NUM, 0x081) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_DEC       THSM_EVENT(MAGIC_NUM, 0x082) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_SIGN      THSM_EVENT(MAGIC_NUM, 0x083) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_VERIFY    THSM_EVENT(MAGIC_NUM, 0x084) // Asymmetric
#define TCCTHSM_EVT_RUN_CIPHER_BY_DMA     THSM_EVENT(MAGIC_NUM, 0x091) // Symmetric Cipher
#define TCCTHSM_EVT_RUN_DIGEST_BY_DMA     THSM_EVENT(MAGIC_NUM, 0x092) // Message Digest (SHA)
#define TCCTHSM_EVT_COMPUTE_MAC_BY_DMA    THSM_EVENT(MAGIC_NUM, 0x093) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_COMPARE_MAC_BY_DMA    THSM_EVENT(MAGIC_NUM, 0x094) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_ASYMMETRIC_ENC_BY_DMA THSM_EVENT(MAGIC_NUM, 0x095) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_DEC_BY_DMA THSM_EVENT(MAGIC_NUM, 0x096) // Asymmetric

// clang-format on

#define TCCTHSM_CIPHER_KEYSIZE_FOR_64 8
#define TCCTHSM_CIPHER_KEYSIZE_FOR_128 16
#define TCCTHSM_CIPHER_KEYSIZE_FOR_192 24
#define TCCTHSM_CIPHER_KEYSIZE_FOR_256 32

enum tcc_thsm_cmd_cipher_enc
{
	DECRYPTION = 0,
	ENCRYPTION = 1,
};

int32_t tcc_thsm_cmd_init(uint32_t device_id);
int32_t tcc_thsm_cmd_finalize(uint32_t device_id);
int32_t tcc_thsm_cmd_get_version(uint32_t device_id, uint32_t *major, uint32_t *minor);
int32_t tcc_thsm_cmd_set_mode(
	uint32_t device_id, uint32_t key_index, uint32_t algorithm, uint32_t op_mode,
	uint32_t key_size);
int32_t tcc_thsm_cmd_set_key(
	uint32_t device_id, uint32_t key_index, uint32_t *key1, uint32_t key1_size, uint32_t *key2,
	uint32_t key2_size, uint32_t *key3, uint32_t key3_size);
int32_t tcc_thsm_cmd_set_key_from_storage(
	uint32_t device_id, uint32_t key_index, uint8_t *obj_id, uint32_t obj_id_len);
int32_t tcc_thsm_cmd_set_key_from_otp(
	uint32_t device_id, uint32_t key_index, uint32_t otp_addr, uint32_t key_size);
int32_t tcc_thsm_cmd_free_mode(uint32_t device_id, uint32_t key_index);
int32_t tcc_thsm_cmd_set_iv_symmetric(
	uint32_t device_id, uint32_t key_index, uint32_t *iv, uint32_t iv_size);
int32_t tcc_thsm_cmd_run_cipher_by_dma(
	uint32_t device_id, uint32_t key_index, uint32_t src_addr, uint32_t src_size, uint32_t dst_addr,
	uint32_t *dst_size, uint32_t flag);
int32_t tcc_thsm_cmd_run_digest_by_dma(
	uint32_t device_id, uint32_t key_index, uint32_t chunk_addr, uint32_t chunk_len, uint8_t *hash,
	uint32_t *hash_len, uint32_t flag);
int32_t
tcc_thsm_cmd_set_iv_mac(uint32_t device_id, uint32_t key_index, uint32_t *iv, uint32_t iv_size);
int32_t tcc_thsm_cmd_compute_mac_by_dma(
	uint32_t device_id, uint32_t key_index, uint32_t message, uint32_t message_len, uint8_t *mac,
	uint32_t *mac_len, uint32_t flag);
int32_t tcc_thsm_cmd_compare_mac_by_dma(
	uint32_t device_id, uint32_t key_index, uint32_t message, uint32_t message_len, uint8_t *mac,
	uint32_t mac_len, uint32_t flag);
int32_t tcc_thsm_cmd_get_rand(uint32_t device_id, uint32_t *rng, uint32_t rng_size);
int32_t tcc_thsm_cmd_gen_key_ss(
	uint32_t device_id, uint8_t *obj_id, uint32_t obj_len, uint32_t algorithm, uint32_t key_size);
int32_t tcc_thsm_cmd_del_key_ss(uint32_t device_id, uint8_t *obj_id, uint32_t obj_len);
int32_t tcc_thsm_cmd_write_key_ss(
	uint32_t device_id, uint8_t *obj_id, uint32_t obj_len, uint8_t *buffer, uint32_t buffer_size);
int32_t
tcc_thsm_cmd_write_otp(uint32_t device_id, uint32_t otp_addr, uint8_t *otp_buf, uint32_t otp_size);
int32_t tcc_thsm_cmd_write_otpimage(uint32_t device_id, uint32_t otp_addr, uint32_t otp_size);
int32_t tcc_thsm_cmd_asym_enc_dec_by_dma(
	uint32_t device_id, uint32_t key_index, uint32_t src_addr, uint32_t src_size, uint32_t dst_addr,
	uint32_t *dst_size, uint32_t enc);
int32_t tcc_thsm_cmd_asym_sign_digest(
	uint32_t device_id, uint32_t key_index, uint8_t *dig, uint32_t dig_size, uint8_t *sig,
	uint32_t *sig_size);
int32_t tcc_thsm_cmd_asym_verify_digest(
	uint32_t device_id, uint32_t key_index, uint8_t *dig, uint32_t dig_size, uint8_t *sig,
	uint32_t sig_size);

#endif /*_TCC_THSM_CMD_H_*/
