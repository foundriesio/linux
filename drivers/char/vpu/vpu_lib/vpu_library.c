// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#include <linux/module.h>
#include <linux/moduleparam.h>

#include "../vpu_type.h"

typedef long codec_handle_t; //!< handle
typedef int codec_result_t; //!< return value

int tcc_vpu_dec(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_dec);

int
tcc_vpu_dec_esc(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_dec_esc);

int
tcc_vpu_dec_ext(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_dec_ext);

#if !defined(VPU_D6)
int tcc_vpu_enc(int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2)
{
	return 1;
}
EXPORT_SYMBOL(tcc_vpu_enc);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu library");
MODULE_LICENSE("Proprietary");
