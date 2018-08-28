/*
 * drivers/char/hevc/lib/hevc_lib.c
 *
 * TCC HEVC LIB modules
 *
 * Copyright (C) 2015 Telechips, Inc.
 * Author : ZzaU(B070371)
 *
 */

#include <linux/moduleparam.h>
#include <linux/module.h>

typedef long codec_handle_t; 		//!< handle
typedef int codec_result_t; 		//!< return value

int tcc_hevc_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_hevc_dec);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc library");
MODULE_LICENSE("Proprietary");
