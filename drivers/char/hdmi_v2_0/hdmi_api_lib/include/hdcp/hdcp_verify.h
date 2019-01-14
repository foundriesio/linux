/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
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
