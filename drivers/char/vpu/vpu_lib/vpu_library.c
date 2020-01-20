/*
 * drivers/vpu_new/vpu_lib.c
 *
 * TCC VPU LIB modules
 *
 * Copyright (C) 2009 Telechips, Inc.
 * Author : ZzaU(B070371)
 *
 */

#include <linux/moduleparam.h>
#include <linux/module.h>
#include "../vpu_type.h"

typedef long codec_handle_t; 		//!< handle
typedef int codec_result_t; 		//!< return value

int tcc_vpu_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_vpu_dec);

int
tcc_vpu_dec_esc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
    return 0;
}
EXPORT_SYMBOL(tcc_vpu_dec_esc);

int
tcc_vpu_dec_ext( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
    return 0;
}
EXPORT_SYMBOL(tcc_vpu_dec_ext);

#if !defined(VPU_D6)
int tcc_vpu_enc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 )
{
	return 0;
}
EXPORT_SYMBOL(tcc_vpu_enc);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu library");
MODULE_LICENSE("Proprietary");
