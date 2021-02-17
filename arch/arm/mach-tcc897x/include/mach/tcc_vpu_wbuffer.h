// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_vpu_wbuffer.h
 * Description: TCC VPU h/w block
 */

#define VPU_INST_MAX 5

#if defined(CONFIG_VENC_CNT_8)
#define VPU_ENC_MAX_CNT 8
#elif defined(CONFIG_VENC_CNT_7)
#define VPU_ENC_MAX_CNT 7
#elif defined(CONFIG_VENC_CNT_6)
#define VPU_ENC_MAX_CNT 6
#elif defined(CONFIG_VENC_CNT_5)
#define VPU_ENC_MAX_CNT 5
#elif defined(CONFIG_VENC_CNT_4)
#define VPU_ENC_MAX_CNT 4
#elif defined(CONFIG_VENC_CNT_3)
#define VPU_ENC_MAX_CNT 3
#elif defined(CONFIG_VENC_CNT_2)
#define VPU_ENC_MAX_CNT 2
#elif defined(CONFIG_VENC_CNT_1)
#define VPU_ENC_MAX_CNT 1
#else
#define VPU_ENC_MAX_CNT 0
#endif

#if defined(CONFIG_TYPE_C5)
#include <video/tcc/TCC_VPUs_CODEC.h> // VPU video codec
#else
#include <video/tcc/TCC_VPU_CODEC.h> // VPU video codec
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
#if defined(JPU_C5)
#include <video/tcc/TCC_JPU_CODEC.h>
#else
#include <video/tcc/TCC_JPU_C6.h>
#endif
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include <video/tcc/TCC_HEVC_CODEC.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include <video/tcc/TCC_VP9_CODEC.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
#include <video/tcc/TCC_VPU_4K_D2_CODEC.h>
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
#include <video/tcc/TCC_VPU_HEVC_ENC_CODEC.h>
#endif

#ifndef _VPU_WBUFFER_H_
#define _VPU_WBUFFER_H_

#define ARRAY_MBYTE(x)	((((x) + (SZ_1M-1)) >> 20) << 20)

#define VPU_WORK_BUF_SIZE	(PAGE_ALIGN(WORK_CODE_PARA_BUF_SIZE))

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
#define WAVExxx_WORK_BUF_SIZE	(PAGE_ALIGN(WAVE5_WORK_CODE_BUF_SIZE))
#elif defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) // HEVC
#define WAVExxx_WORK_BUF_SIZE	(PAGE_ALIGN(WAVE4_WORK_CODE_BUF_SIZE))
#else
#define WAVExxx_WORK_BUF_SIZE	(0)
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
#define G2V2_VP9_WORK_BUF_SIZE	(0) //(PAGE_ALIGN(VP9_WORK_CODE_BUF_SIZE))
#else
#define G2V2_VP9_WORK_BUF_SIZE	(0)
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
#define JPU_WORK_BUF_SIZE	(0) //(PAGE_ALIGN(JPU_WORK_CODE_BUF_SIZE))
#else
#define JPU_WORK_BUF_SIZE	(0)
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
#define VPU_HEVC_ENC_WORK_BUF_SIZE \
	(PAGE_ALIGN(VPU_HEVC_ENC_WORK_CODE_BUF_SIZE))
#else
#define VPU_HEVC_ENC_WORK_BUF_SIZE 0
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || \
	defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4) || \
	defined(CONFIG_VENC_CNT_5) || defined(CONFIG_VENC_CNT_6) || \
	defined(CONFIG_VENC_CNT_7) || defined(CONFIG_VENC_CNT_8)
#if defined(CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC)
#define ENC_HEADER_BUF_SIZE (PAGE_ALIGN(VPU_HEVC_ENC_HEADER_BUF_SIZE))
#else
#define ENC_HEADER_BUF_SIZE (PAGE_ALIGN(VPU_ENC_PUT_HEADER_SIZE))
#endif
#else
#define ENC_HEADER_BUF_SIZE 0
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
#define USER_DATA_BUF_SIZE (WAVE5_USERDATA_BUF_SIZE)
#elif defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC)  // HEVC
#define USER_DATA_BUF_SIZE (WAVE4_USERDATA_BUF_SIZE)
#else
#define USER_DATA_BUF_SIZE (50*1024)
#endif

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
#define VPU_SW_ACCESS_REGION_SIZE ARRAY_MBYTE( \
				VPU_WORK_BUF_SIZE + \
				WAVExxx_WORK_BUF_SIZE + \
				G2V2_VP9_WORK_BUF_SIZE + \
				JPU_WORK_BUF_SIZE + \
				VPU_HEVC_ENC_WORK_BUF_SIZE + \
				(ENC_HEADER_BUF_SIZE * VPU_ENC_MAX_CNT) + \
				(USER_DATA_BUF_SIZE * VPU_INST_MAX) + \
				(PAGE_ALIGN(PS_SAVE_SIZE) + \
					PAGE_ALIGN(SLICE_SAVE_SIZE) + \
					PAGE_ALIGN(LARGE_STREAM_BUF_SIZE)))
#else
#define VPU_SW_ACCESS_REGION_SIZE ARRAY_MBYTE( \
				VPU_WORK_BUF_SIZE + \
				WAVExxx_WORK_BUF_SIZE + \
				G2V2_VP9_WORK_BUF_SIZE + \
				JPU_WORK_BUF_SIZE + \
				VPU_HEVC_ENC_WORK_BUF_SIZE + \
				(ENC_HEADER_BUF_SIZE * VPU_ENC_MAX_CNT) + \
				(USER_DATA_BUF_SIZE * VPU_INST_MAX))
#endif
#endif
