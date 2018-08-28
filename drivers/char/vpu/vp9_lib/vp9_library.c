/*
 * drivers/vpu_new/vpu_lib.c
 *
 * TCC VP9 LIB modules
 *
 * Copyright (C) 2009 Telechips, Inc.
 * Author : ZzaU(B070371)
 *
 */

#include <linux/moduleparam.h>
#include <linux/module.h>
#include "../vpu_type.h"

typedef int codec_handle_t; 		//!< handle
typedef int codec_result_t; 		//!< return value

int tcc_vp9_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_vp9_dec);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC VP9 library");
MODULE_LICENSE("Proprietary");
