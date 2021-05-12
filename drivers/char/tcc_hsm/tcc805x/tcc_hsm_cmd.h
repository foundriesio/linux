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

#ifndef TCC_HSM_CMD_H
#define TCC_HSM_CMD_H

// clang-format off
#define HSM_OK							(0)
#define HSM_GENERAL_FAIL				(-1)
#define HSM_INVALID_PARAM				(-2)

#define REQ_HSM_RUN_AES                     (0x10010000U)
#define REQ_HSM_RUN_AES_BY_KT               (0x10020000U)
#define REQ_HSM_RUN_SM4                     (0x10030000U)
#define REQ_HSM_RUN_SM4_BY_KT               (0x10040000U)

#define REQ_HSM_VERIFY_CMAC                 (0x10110000U)
#define REQ_HSM_VERIFY_CMAC_BY_KT           (0x10120000U)
#define REQ_HSM_GEN_GMAC                    (0x10130000U)
#define REQ_HSM_GEN_GMAC_BY_KT              (0x10140000U)
#define REQ_HSM_GEN_HMAC                    (0x10150000U)
#define REQ_HSM_GEN_HMAC_BY_KT              (0x10160000U)
#define REQ_HSM_GEN_SM3_HMAC                (0x10170000U)
#define REQ_HSM_GEN_SM3_HMAC_BY_KT          (0x10180000U)

#define REQ_HSM_GEN_SHA                     (0x10210000U)
#define REQ_HSM_GEN_SM3                     (0x10220000U)

#define REQ_HSM_RUN_ECDSA_SIGN              (0x10310000U)
#define REQ_HSM_RUN_ECDSA_VERIFY            (0x10320000U)
#define REQ_HSM_RUN_ECDH_PUBKEY_COMPUTE		(0x10330000u)
#define REQ_HSM_RUN_ECDH_PHASE_I			(0x10340000u)
#define REQ_HSM_RUN_ECDH_PHASE_II			(0x10350000u)

#define REQ_HSM_RUN_RSASSA_PKCS_SIGN        (0x10550000U)
#define REQ_HSM_RUN_RSASSA_PKCS_VERIFY      (0x10560000U)
#define REQ_HSM_RUN_RSASSA_PSS_SIGN         (0x10570000U)
#define REQ_HSM_RUN_RSASSA_PSS_VERIFY       (0x10580000U)

#define REQ_HSM_GET_RNG                     (0x10610000U)

#define REQ_HSM_WRITE_OTP                   (0x10710000U)
#define REQ_HSM_WRITE_SNOR                  (0x10720000U)

#define REQ_HSM_SET_KEY_FROM_OTP            (0x10810000U)
#define REQ_HSM_SET_KEY_FROM_SNOR           (0x10820000U)

#define REQ_HSM_GET_VER                     (0x20010000U)

#define HSM_NONE_DMA				(0U)
#define HSM_DMA						(1U)

#define TCC_HSM_AES_KEY_SIZE        (32U)
#define TCC_HSM_AES_IV_SIZE         (32U)
#define TCC_HSM_AES_TAG_SIZE        (32U)
#define TCC_HSM_AES_AAD_SIZE        (32U)

#define TCC_HSM_MAC_KEY_SIZE        (32U)
#define TCC_HSM_MAC_MSG_SIZE        (32U)

#define TCC_HSM_HASH_DIGEST_SIZE	(64U)
#define TCC_HSM_ECDSA_KEY_SIZE		(64U)
#define TCC_HSM_ECDSA_P521_KEY_SIZE	(68U)
#define TCC_HSM_ECDSA_DIGEST_SIZE	(64U)
#define TCC_HSM_ECDSA_SIGN_SIZE		(64U)

#define TCC_HSM_MODE_N_SIZE			(128U)
#define TCC_HSM_RSA_DIG_SIZE		(128U)
#define TCC_HSM_RSA_SIG_SIZE		(128U)
// clang-format on

struct tcc_hsm_ioctl_set_key_param {
	uint32_t addr;
	uint32_t data_size;
	uint32_t key_index;
};

struct tcc_hsm_ioctl_aes_param {
	uint32_t obj_id;
	uint8_t key[TCC_HSM_AES_KEY_SIZE];
	uint32_t key_size;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	ulong src;
	uint32_t src_size;
	ulong dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_aes_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	ulong src;
	uint32_t src_size;
	ulong dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_param {
	uint32_t obj_id;
	uint8_t key[TCC_HSM_MAC_KEY_SIZE];
	uint32_t key_size;
	ulong src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	ulong src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_hash_param {
	uint32_t obj_id;
	ulong src;
	uint32_t src_size;
	uint8_t digest[TCC_HSM_HASH_DIGEST_SIZE];
	uint32_t digest_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_ecdsa_param {
	uint32_t obj_id;
	uint8_t key[TCC_HSM_ECDSA_KEY_SIZE];
	uint32_t key_size;
	uint8_t digest[TCC_HSM_ECDSA_DIGEST_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_ECDSA_SIGN_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_rsassa_param {
	uint32_t obj_id;
	uint8_t modN[TCC_HSM_MODE_N_SIZE];
	uint32_t modN_size;
	ulong key;
	uint32_t key_size;
	uint8_t digest[TCC_HSM_RSA_DIG_SIZE];
	uint32_t digest_size;
	uint8_t sign[TCC_HSM_RSA_SIG_SIZE];
	uint32_t sign_size;
};

struct tcc_hsm_ioctl_write_param {
	uint32_t addr;
	ulong data;
	uint32_t data_size;
};

struct tcc_hsm_ioctl_rng_param {
	ulong rng;
	uint32_t rng_size;
};

struct tcc_hsm_ioctl_version_param {
	uint32_t x;
	uint32_t y;
	uint32_t z;
};

struct tcc_hsm_ioctl_ecdh_key_param {
	uint32_t key_type;
	uint32_t obj_id;
	uint8_t prikey[TCC_HSM_ECDSA_P521_KEY_SIZE];
	uint32_t prikey_size;
	uint8_t pubkey[TCC_HSM_ECDSA_P521_KEY_SIZE * 2];
	uint32_t pubkey_size;
};

int32_t tcc_hsm_cmd_set_key(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_set_key_param *param);
int32_t tcc_hsm_cmd_run_aes(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_aes_param *param);
int32_t tcc_hsm_cmd_run_aes_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_aes_by_kt_param *param);
int32_t tcc_hsm_cmd_gen_mac(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_mac_param *param);
int32_t tcc_hsm_cmd_gen_mac_by_kt(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_mac_by_kt_param *param);
int32_t tcc_hsm_cmd_gen_hash(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_hash_param *param);
int32_t tcc_hsm_cmd_run_ecdh_phaseI(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdh_key_param *param);
int32_t tcc_hsm_cmd_run_ecdsa(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_ecdsa_param *aram);
int32_t tcc_hsm_cmd_run_rsa(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_rsassa_param *param);
int32_t tcc_hsm_cmd_write(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_write_param *param);
int32_t tcc_hsm_cmd_get_version(
	uint32_t device_id, uint32_t req,
	struct tcc_hsm_ioctl_version_param *param);
int32_t tcc_hsm_cmd_get_rand(
	uint32_t device_id, uint32_t req, uint32_t rng, uint32_t rng_size);
#endif /* TCC_HSM_CMD_H */
