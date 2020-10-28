// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/moduleparam.h>
#include <linux/module.h>

typedef long codec_handle_t; //!< handle
typedef int codec_result_t; //!< return value

int tcc_vpu_hevc_enc(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_hevc_enc);

int tcc_vpu_hevc_enc_esc(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_hevc_enc_esc);

int tcc_vpu_hevc_enc_ext(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_hevc_enc_ext);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC HEVC ENC library");
MODULE_LICENSE("Proprietary");
