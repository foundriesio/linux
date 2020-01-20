/*
 * drivers/char/vpu_4k_d2/lib/vpu_4k_d2_lib.c
 *
 * TCC 4K VP9/HEVC LIB modules
 *
 * Copyright (C) 2018 Telechips, Inc.
 * Author : ZzaU(B070371)
 *
 */

#include <linux/moduleparam.h>
#include <linux/module.h>

typedef long codec_handle_t; 		//!< handle
typedef int codec_result_t; 		//!< return value

int tcc_vpu_4k_d2_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_vpu_4k_d2_dec);

int tcc_vpu_4k_d2_dec_esc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_vpu_4k_d2_dec_esc);

int tcc_vpu_4k_d2_dec_ext( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_vpu_4k_d2_dec_ext);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC 4K VP9/HEVC library");
MODULE_LICENSE("Proprietary");
