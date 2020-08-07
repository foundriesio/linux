/* linux/drivers/char/tcc_hsm.h
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

#ifndef _TCC_HSM_H_
#define _TCC_HSM_H_

#define TCCHSM_DEVICE_NAME "tcc_hsm"

#define TCC_HSM_AES_KEY_SIZE 32
#define TCC_HSM_AES_IV_SIZE 32
#define TCC_HSM_AES_TAG_SIZE 32
#define TCC_HSM_AES_AAD_SIZE 32

#define TCC_HSM_MAC_KEY_SIZE 32
#define TCC_HSM_MAC_MSG_SIZE 32

#define TCC_HSM_HASH_DIGEST_SIZE 64
#define TCC_HSM_ECDSA_KEY_SIZE 64
#define TCC_HSM_ECDSA_DIGEST_SIZE 64
#define TCC_HSM_ECDSA_SIGN_SIZE 64

#define TCC_HSM_MODE_N_SIZE 128
#define TCC_HSM_RSA_DIG_SIZE 128
#define TCC_HSM_RSA_SIG_SIZE 128

enum tcc_hsm_ioctl_obj_id_aes
{
	OID_AES_ENCRYPT = 0x00000000,
	OID_AES_DECRYPT = 0x01000000,
	OID_AES_ECB_128 = 0x00100008,
	OID_AES_ECB_192 = 0x00180008,
	OID_AES_ECB_256 = 0x00200008,
	OID_AES_CBC_128 = 0x00100108,
	OID_AES_CBC_192 = 0x00180108,
	OID_AES_CBC_256 = 0x00200108,
	OID_AES_CTR_128 = 0x00100208,
	OID_AES_CTR_192 = 0x00180208,
	OID_AES_CTR_256 = 0x00200208,
	OID_AES_XTS_128 = 0x00100308,
	OID_AES_XTS_256 = 0x00200308,
	OID_AES_CCM_128 = 0x00101008,
	OID_AES_CCM_192 = 0x00181008,
	OID_AES_CCM_256 = 0x00201008,
	OID_AES_GCM_128 = 0x00101108,
	OID_AES_GCM_192 = 0x00181108,
	OID_AES_GCM_256 = 0x00201108,
};

enum tcc_hsm_ioctl_obj_id_sm4
{
	OID_SM4_ENCRYPT = 0x00000000,
	OID_SM4_DECRYPT = 0x01000000,
	OID_SM4_ECB_128 = 0x00100008,
	OID_SM4_CBC_128 = 0x00100108,
};

enum tcc_hsm_ioctl_obj_id_hmac
{
	OID_HMAC_SHA1_160 = 0x00011100,
	OID_HMAC_SHA2_224 = 0x00012200,
	OID_HMAC_SHA2_256 = 0x00012300,
	OID_HMAC_SHA2_384 = 0x00012400,
	OID_HMAC_SHA2_512 = 0x00012500,
	OID_HMAC_SHA3_224 = 0x00013200,
	OID_HMAC_SHA3_256 = 0x00013300,
	OID_HMAC_SHA3_384 = 0x00013400,
	OID_HMAC_SHA3_512 = 0x00013500,
};

enum tcc_hsm_ioctl_obj_id_hash
{
	OID_SHA1_160 = 0x00001100,
	OID_SHA2_224 = 0x00002200,
	OID_SHA2_256 = 0x00002300,
	OID_SHA2_384 = 0x00002400,
	OID_SHA2_512 = 0x00002500,
	OID_SHA3_224 = 0x00003200,
	OID_SHA3_256 = 0x00003300,
	OID_SHA3_384 = 0x00003400,
	OID_SHA3_512 = 0x00003500,
	OID_SM3_256 = 0x01002300,
};

enum tcc_hsm_ioctl_obj_id_ecc
{
	OID_ECC_P256 = 0x00000013,
	OID_ECC_P384 = 0x00000014,
	OID_ECC_P521 = 0x00000015,
	OID_ECC_BP256 = 0x00000053,
	OID_ECC_BP384 = 0x00000054,
	OID_ECC_BP512 = 0x00000055,
	OID_SM2_256_SM3_256 = 0x010023A3,
};

struct tcc_hsm_ioctl_set_key_param
{
	uint32_t addr;
	uint32_t data_size;
	uint32_t key_index;
};

struct tcc_hsm_ioctl_aes_param
{
	uint32_t obj_id;
	uint8_t key[TCC_HSM_AES_KEY_SIZE];
	uint32_t key_size;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	uint64_t src;
	uint32_t src_size;
	uint64_t dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_aes_by_kt_param
{
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	uint64_t src;
	uint32_t src_size;
	uint64_t dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_param
{
	uint32_t obj_id;
	uint8_t key[TCC_HSM_MAC_KEY_SIZE];
	uint32_t key_size;
	uint64_t src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_by_kt_param
{
	uint32_t obj_id;
	uint32_t key_index;
	uint64_t src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_hash_param
{
	uint32_t obj_id;
	uint64_t src;
	uint32_t src_size;
	uint8_t digest[TCC_HSM_HASH_DIGEST_SIZE];
	uint32_t digest_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_ecdsa_param
{
	uint32_t obj_id;
	uint8_t key[TCC_HSM_ECDSA_KEY_SIZE];
	uint32_t key_size;
	uint8_t digest[TCC_HSM_ECDSA_DIGEST_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_ECDSA_SIGN_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_rsassa_param
{
	uint32_t obj_id;
	uint8_t modN[TCC_HSM_MODE_N_SIZE];
	uint32_t modN_size;
	uint64_t key;
	uint32_t key_size;
	uint8_t digest[TCC_HSM_RSA_DIG_SIZE];
	uint32_t digest_size;
	uint8_t sign[TCC_HSM_RSA_SIG_SIZE];
	uint32_t sign_size;
};

struct tcc_hsm_ioctl_write_param
{
	uint32_t addr;
	uint64_t data;
	uint32_t data_size;
};

struct tcc_hsm_ioctl_rng_param
{
	uint64_t rng;
	uint32_t rng_size;
};

struct tcc_hsm_ioctl_version_param
{
	uint32_t x;
	uint32_t y;
	uint32_t z;
};

// clang-format off
#define HSM_IOCTL_MAGIC 'H'
#define	HSM_SET_KEY_FROM_OTP_CMD		_IOWR(HSM_IOCTL_MAGIC, 0, unsigned int)
#define	HSM_SET_KEY_FROM_SNOR_CMD		_IOWR(HSM_IOCTL_MAGIC, 1, unsigned int)
#define	HSM_RUN_AES_CMD					_IOWR(HSM_IOCTL_MAGIC, 2, unsigned int)
#define	HSM_RUN_AES_BY_KT_CMD			_IOWR(HSM_IOCTL_MAGIC, 3, unsigned int)
#define	HSM_RUN_SM4_CMD					_IOWR(HSM_IOCTL_MAGIC, 4, unsigned int)
#define	HSM_RUN_SM4_BY_KT_CMD			_IOWR(HSM_IOCTL_MAGIC, 5, unsigned int)
#define	HSM_GEN_CMAC_VERIFY_CMD			_IOWR(HSM_IOCTL_MAGIC, 6, unsigned int)
#define	HSM_GEN_CMAC_VERIFY_BY_KT_CMD	_IOWR(HSM_IOCTL_MAGIC, 7, unsigned int)
#define	HSM_GEN_GMAC_CMD				_IOWR(HSM_IOCTL_MAGIC, 8, unsigned int)
#define	HSM_GEN_GMAC_BY_KT_CMD			_IOWR(HSM_IOCTL_MAGIC, 9, unsigned int)
#define	HSM_GEN_HMAC_CMD				_IOWR(HSM_IOCTL_MAGIC, 10, unsigned int)
#define	HSM_GEN_HMAC_BY_KT_CMD			_IOWR(HSM_IOCTL_MAGIC, 11, unsigned int)
#define	HSM_GEN_SM3_HMAC_CMD			_IOWR(HSM_IOCTL_MAGIC, 12, unsigned int)
#define	HSM_GEN_SM3_HMAC_BY_KT_CMD		_IOWR(HSM_IOCTL_MAGIC, 13, unsigned int)
#define	HSM_GEN_SHA_CMD					_IOWR(HSM_IOCTL_MAGIC, 14, unsigned int)
#define	HSM_GEN_SM3_CMD					_IOWR(HSM_IOCTL_MAGIC, 15, unsigned int)
#define	HSM_RUN_ECDSA_SIGN_CMD			_IOWR(HSM_IOCTL_MAGIC, 16, unsigned int)
#define	HSM_RUN_ECDSA_VERIFY_CMD		_IOWR(HSM_IOCTL_MAGIC, 17, unsigned int)
#define	HSM_RUN_RSASSA_PKCS_SIGN_CMD	_IOWR(HSM_IOCTL_MAGIC, 18, unsigned int)
#define	HSM_RUN_RSASSA_PKCS_VERIFY_CMD	_IOWR(HSM_IOCTL_MAGIC, 19, unsigned int)
#define	HSM_RUN_RSASSA_PSS_SIGN_CMD		_IOWR(HSM_IOCTL_MAGIC, 20, unsigned int)
#define	HSM_RUN_RSASSA_PSS_VERIFY_CMD	_IOWR(HSM_IOCTL_MAGIC, 21, unsigned int)
#define	HSM_GET_RNG_CMD					_IOWR(HSM_IOCTL_MAGIC, 22, unsigned int)
#define	HSM_WRITE_OTP_CMD				_IOWR(HSM_IOCTL_MAGIC, 23, unsigned int)
#define	HSM_WRITE_SNOR_CMD				_IOWR(HSM_IOCTL_MAGIC, 24, unsigned int)
#define	HSM_GET_VER_CMD					_IOWR(HSM_IOCTL_MAGIC, 25, unsigned int)
// clang-format on

#endif /*_TCC_HSM_H_*/
