/*
 * drivers/char/hevc/lib/jpu_lib.c
 *
 * TCC JPU LIB modules
 *
 * Copyright (C) 2015 Telechips, Inc.
 * Author : ZzaU(B070371)
 *
 */

#include <linux/moduleparam.h>
#include <linux/module.h>

typedef long codec_handle_t; 		//!< handle
typedef int codec_result_t; 		//!< return value

int tcc_jpu_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_jpu_dec);

int tcc_jpu_enc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_jpu_enc);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu library");
MODULE_LICENSE("Proprietary");
