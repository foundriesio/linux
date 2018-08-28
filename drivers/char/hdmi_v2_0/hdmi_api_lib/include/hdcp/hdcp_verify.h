/*
 * hdcp_verify.h
 *
 *  Created on: Jul 20, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HDCPVERIFY_H_
#define HDCPVERIFY_H_

#include "../../../include/hdmi_includes.h"

typedef struct {
	u8 mLength[8];
	u8 mBlock[64];
	int mIndex;
	int mComputed;
	int mCorrupted;
	unsigned mDigest[5];
} sha_t;

#define KSV_MSK 0x7F
#define VRL_LENGTH 0x05
#define VRL_HEADER 5
#define VRL_NUMBER 3
#define HEADER 10
#define SHAMAX 20
#define DSAMAX 20

void sha_reset(struct hdmi_tx_dev *dev, sha_t *sha);

int sha_result(struct hdmi_tx_dev *dev, sha_t *sha);

void sha_input(struct hdmi_tx_dev *dev, sha_t *sha, const u8 *data, size_t size);

void sha_process_block(struct hdmi_tx_dev *dev, sha_t *sha);

void sha_pad_message(struct hdmi_tx_dev *dev, sha_t *sha);

int hdcp_verify_dsa(struct hdmi_tx_dev *dev, const u8 *M, size_t n, const u8 *r, const u8 *s);

int hdcp_array_add(struct hdmi_tx_dev *dev, u8 *r, const u8 *a, const u8 *b, size_t n);

int hdcp_array_cmp(struct hdmi_tx_dev *dev, const u8 *a, const u8 *b, size_t n);

void hdcp_array_cpy(struct hdmi_tx_dev *dev, u8 *dst, const u8 *src, size_t n);

int hdcp_array_div(struct hdmi_tx_dev *dev, u8 *r, const u8 *D, const u8 *d, size_t n);

int hdcp_array_mac(struct hdmi_tx_dev *dev, u8 *r, const u8 *M, const u8 m, size_t n);

int hdcp_array_mul(struct hdmi_tx_dev *dev, u8 *r, const u8 *M, const u8 *m, size_t n);

void hdcp_array_set(struct hdmi_tx_dev *dev, u8 *dst, const u8 src, size_t n);

int hdcp_array_usb(struct hdmi_tx_dev *dev, u8 *r, const u8 *a, const u8 *b, size_t n);

void hdcp_array_swp(struct hdmi_tx_dev *dev, u8 *r, size_t n);

int hdcp_array_tst(struct hdmi_tx_dev *dev, const u8 *a, const u8 b, size_t n);

int hdcp_compute_exp(
	struct hdmi_tx_dev *dev, u8 *c, const u8 *M, const u8 *e, const u8 *p, size_t n, size_t nE);

int hdcp_compute_inv(struct hdmi_tx_dev *dev, u8 *out, const u8 *z, const u8 *a, size_t n);

int hdcp_compute_mod(struct hdmi_tx_dev *dev, u8 *dst, const u8 *src, const u8 *p, size_t n);

int hdcp_compute_mul(
	struct hdmi_tx_dev *dev, u8 *p, const u8 *a, const u8 *b, const u8 *m, size_t n);

int hdcp_verify_ksv(struct hdmi_tx_dev *dev, const u8 *data, size_t size);

int hdcp_verify_srm(struct hdmi_tx_dev *dev, const u8 *data, size_t size);

#endif /* HDCPVERIFY_H_ */
