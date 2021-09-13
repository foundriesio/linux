/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2020 Telechips Inc.
 */
#ifndef DT_BINDINGS_PMAP_TCC897X_VIDEO_H
#define DT_BINDINGS_PMAP_TCC897X_VIDEO_H

/*
* For configuration for vpu's memory
*/
#define _CODEC_HANDLE_T_
#define _CODEC_RESULT_T_
#define _CODEC_ADDR_T_
#define INC_DEVICE_TREE_PMAP
//#define TEST_VPU_DRAM_INTLV
//#define PMAP_TO_CMA

/*
 * vpu's configuration for TCC803x
 */
#define TCC_SUPPORT_JPU
#define TCC_SUPPORT_WAVE410_HEVC

/*
 * [20210903] Temporary fix -because followings are used
 * in tcc-pmap-common.dtsi. If these are defined
 * in display code, these will be removed in future.
 */
#if defined(ARCH_TCC897X_LINUX)

#undef PRIMARY_FRAMEBUFFER_WIDTH
#define PRIMARY_FRAMEBUFFER_WIDTH       (1920)
#undef PRIMARY_FRAMEBUFFER_HEIGHT
#define PRIMARY_FRAMEBUFFER_HEIGHT      (720)
#undef SECONDARY_FRAMEBUFFER_WIDTH
#define SECONDARY_FRAMEBUFFER_WIDTH     (1920)
#undef SECONDARY_FRAMEBUFFER_HEIGHT
#define SECONDARY_FRAMEBUFFER_HEIGHT    (720)
#undef TERTIARY_FRAMEBUFFER_WIDTH
#define TERTIARY_FRAMEBUFFER_WIDTH      (0)
#undef TERTIARY_FRAMEBUFFER_HEIGHT
#define TERTIARY_FRAMEBUFFER_HEIGHT     (0)

#undef PRIMARY_TARGET_WIDTH
#define PRIMARY_TARGET_WIDTH            (1920)
#undef PRIMARY_TARGET_HEIGHT
#define PRIMARY_TARGET_HEIGHT           (720)
#undef SECONDARY_TARGET_WIDTH
#define SECONDARY_TARGET_WIDTH          (1920)
#undef SECONDARY_TARGET_HEIGHT
#define SECONDARY_TARGET_HEIGHT         (720)
#undef TERTIARY_TARGET_WIDTH
#define TERTIARY_TARGET_HEIGHT          (0)
#undef TERTIARY_TARGET_WIDTH
#define TERTIARY_TARGET_HEIGHT          (0)
#undef SUPPORT_DISPLAY_MAX_WIDTH
#undef SUPPORT_DISPLAY_MAX_HEIGHT
#if ((PRIMARY_TARGET_WIDTH*PRIMARY_TARGET_HEIGHT) > \
    (SECONDARY_TARGET_WIDTH*SECONDARY_TARGET_HEIGHT))
#define SUPPORT_DISPLAY_MAX_WIDTH       PRIMARY_TARGET_WIDTH
#define SUPPORT_DISPLAY_MAX_HEIGHT      PRIMARY_TARGET_HEIGHT
#else
#define SUPPORT_DISPLAY_MAX_WIDTH       SECONDARY_TARGET_WIDTH
#define SUPPORT_DISPLAY_MAX_HEIGHT      SECONDARY_TARGET_HEIGHT
#endif

#endif //ARCH_TCC897X_LINUX

#define SUPPORT_VIQE_MAX_WIDTH          (1920)
#define SUPPORT_VIQE_MAX_HEIGHT         (1088)
//*/

#undef SZ_1MB
#define SZ_1MB              (1024*1024)

#undef ARRAY_16MBYTE
#define ARRAY_16MBYTE(x)    ((((x) + ((16*SZ_1MB)-1))>> 24) << 24)

#undef ARRAY_MBYTE
#define ARRAY_MBYTE(x)      ((((x) + (SZ_1MB-1))>> 20) << 20)

#undef ARRAY_256KBYTE
#define ARRAY_256KBYTE(x)   ((((x) + ((SZ_1MB/4)-1))>> 18) << 18)

#undef ALIGNED_BUF256
#define ALIGNED_BUF256(x)   (((x) + 255) & ~(255))

#undef ALIGNED_BUF64
#define ALIGNED_BUF64(x)    (((x) + 63) & ~(63))

//for HEVC/VPU framebuffer_size Calculation
#define WAVE_CAL_PROCBUFFER(use,w,h,f) \
    (((0x1400000 \
    /*the rest except framebuffer, userdatabuffer and workbuffer*/) \
    + (ARRAY_MBYTE(((w*h*2)+((w*h*37)/100))*(f+10))))*use)
#define VPU_CAL_PROCBUFFER(use,w,h,f) \
    (((0x400000 \
    /*the rest except framebuffer and workbuffer*/) \
    + ARRAY_MBYTE(((w*h*182)/100)*(f+7)))*use)
#define VPU_CAL_ENC_PROCBUFFER(use,w,h,f) \
    (((0x300000 \
    /*the rest except framebuffer and workbuffer*/) \
    + ARRAY_MBYTE(w*h*3/2) \
    + ARRAY_MBYTE((((w+15)/16*16)*((h+15)/16*16)*3/2)*f))*use)
#define VPU_HEVC_ENC_CAL_PROCBUFFER(use,w,h,f) \
    (((0x00C00000 \
    /*the rest except framebuffer, userdatabuffer and workbuffer*/) \
    +(f*ARRAY_MBYTE(ALIGNED_BUF256(w)*ALIGNED_BUF64(h)*69/32+(20*1024)))) \
    *use) /*f=2*/

/************************* SYNC WITH tcc_vpu_wbuffer.h ************************/
#ifndef _VPU_WBUFFER_DTSI_
#define _VPU_WBUFFER_DTSI_

#ifdef TCC_SUPPORT_JPU
#if defined(JPU_C5)
#include <video/tcc/TCC_JPU_CODEC.h> //JPU C5
#else
#include <video/tcc/TCC_JPU_C6.h> //JPU C6
#endif
#endif

#if defined(SUPPORT_TYPE_C5)
#include <video/tcc/TCC_VPUs_CODEC.h> // VPU video codec
#else
#include <video/tcc/TCC_VPU_CODEC.h> // CODA960 or BODA950
#endif

#ifdef TCC_SUPPORT_WAVE410_HEVC
#include <video/tcc/TCC_HEVC_CODEC.h> //WAVE410
#endif

#ifdef TCC_SUPPORT_G2V2_VP9
#include <video/tcc/TCC_VP9_CODEC.h>
#endif

#ifdef TCC_SUPPORT_WAVE512_4K_D2
#include <video/tcc/TCC_VPU_4K_D2_CODEC.h> //WAVE512
#endif

#ifdef TCC_SUPPORT_WAVE420L_VPU_HEVC_ENC
#include <video/tcc/TCC_VPU_HEVC_ENC_CODEC.h> //WAVE420L
#endif

#if INST_5TH_USE
#define VPU_INST_MAX 5
#elif INST_4TH_USE
#define VPU_INST_MAX 4
#elif INST_3RD_USE
#define VPU_INST_MAX 3
#elif INST_2ND_USE
#define VPU_INST_MAX 2
#elif INST_1ST_USE
#define VPU_INST_MAX 1
#else
#define VPU_INST_MAX 0
#endif

#if INST_ENC_16TH_USE
#define VPU_ENC_MAX_CNT 16
#elif INST_ENC_15TH_USE
#define VPU_ENC_MAX_CNT 15
#elif INST_ENC_14TH_USE
#define VPU_ENC_MAX_CNT 14
#elif INST_ENC_13TH_USE
#define VPU_ENC_MAX_CNT 13
#elif INST_ENC_12TH_USE
#define VPU_ENC_MAX_CNT 12
#elif INST_ENC_11TH_USE
#define VPU_ENC_MAX_CNT 11
#elif INST_ENC_10TH_USE
#define VPU_ENC_MAX_CNT 10
#elif INST_ENC_9TH_USE
#define VPU_ENC_MAX_CNT 9
#elif INST_ENC_8TH_USE
#define VPU_ENC_MAX_CNT 8
#elif INST_ENC_7TH_USE
#define VPU_ENC_MAX_CNT 7
#elif INST_ENC_6TH_USE
#define VPU_ENC_MAX_CNT 6
#elif INST_ENC_5TH_USE
#define VPU_ENC_MAX_CNT 5
#elif INST_ENC_4TH_USE
#define VPU_ENC_MAX_CNT 4
#elif INST_ENC_3RD_USE
#define VPU_ENC_MAX_CNT 3
#elif INST_ENC_2ND_USE
#define VPU_ENC_MAX_CNT 2
#elif INST_ENC_1ST_USE
#define VPU_ENC_MAX_CNT 1
#else
#define VPU_ENC_MAX_CNT 0
#endif

#define VPU_WORK_BUF_SIZE        (WORK_CODE_PARA_BUF_SIZE)

#if defined(TCC_SUPPORT_WAVE512_4K_D2) // HEVC/VP9
#define WAVExxx_WORK_BUF_SIZE    (WAVE5_WORK_CODE_BUF_SIZE)
#elif defined(TCC_SUPPORT_WAVE410_HEVC) // HEVC
#define WAVExxx_WORK_BUF_SIZE    (WAVE4_WORK_CODE_BUF_SIZE)
#else
#define WAVExxx_WORK_BUF_SIZE    (0)
#endif

#ifdef TCC_SUPPORT_G2V2_VP9 // VP9
#define G2V2_VP9_WORK_BUF_SIZE   (0) //(PAGE_ALIGN(VP9_WORK_CODE_BUF_SIZE))
#else
#define G2V2_VP9_WORK_BUF_SIZE   (0)
#endif

#ifdef TCC_SUPPORT_JPU
#define JPU_WORK_BUF_SIZE        (0) //(PAGE_ALIGN(JPU_WORK_CODE_BUF_SIZE))
#else
#define JPU_WORK_BUF_SIZE        (0)
#endif

#ifdef TCC_SUPPORT_WAVE420L_VPU_HEVC_ENC
#define VPU_HEVC_ENC_WORK_BUF_SIZE \
    (VPU_HEVC_ENC_WORK_CODE_BUF_SIZE)
#else
#define VPU_HEVC_ENC_WORK_BUF_SIZE (0)
#endif

#if INST_ENC_1ST_USE  || INST_ENC_2ND_USE   || \
   INST_ENC_3RD_USE   || INST_ENC_4TH_USE   || \
   INST_ENC_5TH_USE   || INST_ENC_6TH_USE   || \
   INST_ENC_7TH_USE   || INST_ENC_8TH_USE   || \
   INST_ENC_9TH_USE   || INST_ENC_10TH_USE  || \
   INST_ENC_11TH_USE  || INST_ENC_12TH_USE  || \
   INST_ENC_13TH_USE  || INST_ENC_14TH_USE  || \
   INST_ENC_15TH_USE  || INST_ENC_16TH_USE
#if INST_ENC_1ST_IS_HEVC_TYPE  || INST_ENC_2ND_IS_HEVC_TYPE   || \
   INST_ENC_3RD_IS_HEVC_TYPE   || INST_ENC_4TH_IS_HEVC_TYPE   || \
   INST_ENC_5TH_IS_HEVC_TYPE   || INST_ENC_6TH_IS_HEVC_TYPE   || \
   INST_ENC_7TH_IS_HEVC_TYPE   || INST_ENC_8TH_IS_HEVC_TYPE   || \
   INST_ENC_9TH_IS_HEVC_TYPE   || INST_ENC_10TH_IS_HEVC_TYPE  || \
   INST_ENC_11TH_IS_HEVC_TYPE  || INST_ENC_12TH_IS_HEVC_TYPE  || \
   INST_ENC_13TH_IS_HEVC_TYPE  || INST_ENC_14TH_IS_HEVC_TYPE  || \
   INST_ENC_15TH_IS_HEVC_TYPE  || INST_ENC_16TH_IS_HEVC_TYPE
#define ENC_HEADER_BUF_SIZE (VPU_HEVC_ENC_HEADER_BUF_SIZE)
#else
#define ENC_HEADER_BUF_SIZE (VPU_ENC_PUT_HEADER_SIZE)
#endif
#else
#define ENC_HEADER_BUF_SIZE (0)
#endif

#if defined(TCC_SUPPORT_WAVE512_4K_D2) // HEVC/VP9
#define USER_DATA_BUF_SIZE (WAVE5_USERDATA_BUF_SIZE)
#elif defined(TCC_SUPPORT_WAVE410_HEVC) // HEVC
#define USER_DATA_BUF_SIZE (WAVE4_USERDATA_BUF_SIZE)
#else
#define USER_DATA_BUF_SIZE (50*1024)
#endif

#if defined(TEST_VPU_DRAM_INTLV)
#define VPU_SW_ACCESS_REGION_SIZE (ARRAY_MBYTE( \
                        VPU_WORK_BUF_SIZE + \
                        WAVExxx_WORK_BUF_SIZE + \
                        G2V2_VP9_WORK_BUF_SIZE + \
                        JPU_WORK_BUF_SIZE + \
                        VPU_HEVC_ENC_WORK_BUF_SIZE + \
                        (ENC_HEADER_BUF_SIZE * VPU_ENC_MAX_CNT) + \
                        (USER_DATA_BUF_SIZE * VPU_INST_MAX) + \
                        (PS_SAVE_SIZE + \
                            SLICE_SAVE_SIZE + \
                            LARGE_STREAM_BUF_SIZE)))
#else
#define VPU_SW_ACCESS_REGION_SIZE (ARRAY_MBYTE( \
                        VPU_WORK_BUF_SIZE + \
                        WAVExxx_WORK_BUF_SIZE + \
                        G2V2_VP9_WORK_BUF_SIZE + \
                        JPU_WORK_BUF_SIZE + \
                        VPU_HEVC_ENC_WORK_BUF_SIZE + \
                        (ENC_HEADER_BUF_SIZE * VPU_ENC_MAX_CNT) + \
                        (USER_DATA_BUF_SIZE * VPU_INST_MAX)))
#endif

#endif //_VPU_WBUFFER_DTSI_
/************************* SYNC WITH tcc_vpu_wbuffer.h ************************/

// Decoder
#define NUM_OF_MAIN_INSTANCE    (INST_1ST_USE + INST_2ND_USE)

#if (INST_1ST_IS_HEVC_TYPE > 0)
#define INST_1ST_PROC_SIZE WAVE_CAL_PROCBUFFER( \
                        INST_1ST_USE, \
                        INST_1ST_VIDEO_WIDTH, \
                        INST_1ST_VIDEO_HEIGHT, \
                        INST_1ST_MAX_FRAMEBUFFERS)
#else
#define INST_1ST_PROC_SIZE VPU_CAL_PROCBUFFER( \
                        INST_1ST_USE, \
                        INST_1ST_VIDEO_WIDTH, \
                        INST_1ST_VIDEO_HEIGHT, \
                        INST_1ST_MAX_FRAMEBUFFERS)
#endif

#if (INST_2ND_IS_HEVC_TYPE > 0)
#define INST_2ND_PROC_SIZE WAVE_CAL_PROCBUFFER( \
                        INST_2ND_USE, \
                        INST_2ND_VIDEO_WIDTH, \
                        INST_2ND_VIDEO_HEIGHT, \
                        INST_2ND_MAX_FRAMEBUFFERS)
#else
#define INST_2ND_PROC_SIZE VPU_CAL_PROCBUFFER( \
                        INST_2ND_USE, \
                        INST_2ND_VIDEO_WIDTH, \
                        INST_2ND_VIDEO_HEIGHT, \
                        INST_2ND_MAX_FRAMEBUFFERS)
#endif

// to support 3rd and 4th decoder with 1st and 2nd at the same time.
#if (INST_3RD_IS_HEVC_TYPE > 0)
#define INST_3RD_PROC_SIZE WAVE_CAL_PROCBUFFER( \
                        INST_3RD_USE, \
                        INST_3RD_VIDEO_WIDTH, \
                        INST_3RD_VIDEO_HEIGHT, \
                        INST_3RD_MAX_FRAMEBUFFERS)
#else
#define INST_3RD_PROC_SIZE VPU_CAL_PROCBUFFER( \
                        INST_3RD_USE, \
                        INST_3RD_VIDEO_WIDTH, \
                        INST_3RD_VIDEO_HEIGHT, \
                        INST_3RD_MAX_FRAMEBUFFERS)
#endif

#if (INST_4TH_IS_HEVC_TYPE > 0)
#define INST_4TH_PROC_SIZE WAVE_CAL_PROCBUFFER( \
                        INST_4TH_USE, \
                        INST_4TH_VIDEO_WIDTH, \
                        INST_4TH_VIDEO_HEIGHT, \
                        INST_4TH_MAX_FRAMEBUFFERS)
#else
#define INST_4TH_PROC_SIZE VPU_CAL_PROCBUFFER( \
                        INST_4TH_USE, \
                        INST_4TH_VIDEO_WIDTH, \
                        INST_4TH_VIDEO_HEIGHT, \
                        INST_4TH_MAX_FRAMEBUFFERS)
#endif

#if (INST_5TH_IS_HEVC_TYPE > 0)
#define INST_5TH_PROC_SIZE WAVE_CAL_PROCBUFFER( \
                        INST_5TH_USE, \
                        INST_5TH_VIDEO_WIDTH, \
                        INST_5TH_VIDEO_HEIGHT, \
                        INST_5TH_MAX_FRAMEBUFFERS)
#else
#define INST_5TH_PROC_SIZE VPU_CAL_PROCBUFFER( \
                        INST_5TH_USE, \
                        INST_5TH_VIDEO_WIDTH, \
                        INST_5TH_VIDEO_HEIGHT, \
                        INST_5TH_MAX_FRAMEBUFFERS)
#endif


#if (NUM_OF_MAIN_INSTANCE > 1)
#define VIDEO_MAIN_SIZE ARRAY_MBYTE( \
                        INST_1ST_PROC_SIZE + \
                        INST_2ND_PROC_SIZE)
#else
#if (INST_1ST_PROC_SIZE > INST_2ND_PROC_SIZE)
#define VIDEO_MAIN_SIZE ARRAY_MBYTE(INST_1ST_PROC_SIZE)
#else
#define VIDEO_MAIN_SIZE ARRAY_MBYTE(INST_2ND_PROC_SIZE)
#endif
#endif

#if INST_3RD_USE || \
   INST_4TH_USE || \
   INST_5TH_USE
#define VIDEO_SUB_SIZE ARRAY_MBYTE(INST_3RD_PROC_SIZE + INST_4TH_PROC_SIZE)
#else
#define VIDEO_SUB_SIZE ARRAY_MBYTE(0)
#endif

#if INST_5TH_USE
#define VIDEO_SUB2_SIZE ARRAY_MBYTE(INST_5TH_PROC_SIZE)
#else
#define VIDEO_SUB2_SIZE ARRAY_MBYTE(0)
#endif

// Encoder
#if (INST_ENC_1ST_IS_HEVC_TYPE > 0)
#define INST_1ST_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_1ST_USE, \
                        INST_ENC_1ST_VIDEO_WIDTH, \
                        INST_ENC_1ST_VIDEO_HEIGHT, \
                        INST_ENC_1ST_MAX_FRAMEBUFFERS)
#else
#define INST_1ST_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_1ST_USE, \
                        INST_ENC_1ST_VIDEO_WIDTH, \
                        INST_ENC_1ST_VIDEO_HEIGHT, \
                        INST_ENC_1ST_MAX_FRAMEBUFFERS)
#endif
#define ENC_SIZE ARRAY_MBYTE(INST_1ST_ENC_PROC_SIZE)

#if (INST_ENC_2ND_IS_HEVC_TYPE > 0)
#define INST_2ND_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_2ND_USE, \
                        INST_ENC_2ND_VIDEO_WIDTH, \
                        INST_ENC_2ND_VIDEO_HEIGHT, \
                        INST_ENC_2ND_MAX_FRAMEBUFFERS)
#else
#define INST_2ND_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_2ND_USE, \
                        INST_ENC_2ND_VIDEO_WIDTH, \
                        INST_ENC_2ND_VIDEO_HEIGHT, \
                        INST_ENC_2ND_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT_SIZE ARRAY_MBYTE(INST_2ND_ENC_PROC_SIZE)

#if (INST_ENC_3RD_IS_HEVC_TYPE > 0)
#define INST_3RD_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_3RD_USE, \
                        INST_ENC_3RD_VIDEO_WIDTH, \
                        INST_ENC_3RD_VIDEO_HEIGHT, \
                        INST_ENC_3RD_MAX_FRAMEBUFFERS)
#else
#define INST_3RD_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_3RD_USE, \
                        INST_ENC_3RD_VIDEO_WIDTH, \
                        INST_ENC_3RD_VIDEO_HEIGHT, \
                        INST_ENC_3RD_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT2_SIZE ARRAY_MBYTE(INST_3RD_ENC_PROC_SIZE)

#if (INST_ENC_4TH_IS_HEVC_TYPE > 0)
#define INST_4TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_4TH_USE, \
                        INST_ENC_4TH_VIDEO_WIDTH, \
                        INST_ENC_4TH_VIDEO_HEIGHT, \
                        INST_ENC_4TH_MAX_FRAMEBUFFERS)
#else
#define INST_4TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_4TH_USE, \
                        INST_ENC_4TH_VIDEO_WIDTH, \
                        INST_ENC_4TH_VIDEO_HEIGHT, \
                        INST_ENC_4TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT3_SIZE ARRAY_MBYTE(INST_4TH_ENC_PROC_SIZE)

#if (INST_ENC_5TH_IS_HEVC_TYPE > 0)
#define INST_5TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_5TH_USE, \
                        INST_ENC_5TH_VIDEO_WIDTH, \
                        INST_ENC_5TH_VIDEO_HEIGHT, \
                        INST_ENC_5TH_MAX_FRAMEBUFFERS)
#else
#define INST_5TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_5TH_USE, \
                        INST_ENC_5TH_VIDEO_WIDTH, \
                        INST_ENC_5TH_VIDEO_HEIGHT, \
                        INST_ENC_5TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT4_SIZE ARRAY_MBYTE(INST_5TH_ENC_PROC_SIZE)

#if (INST_ENC_6TH_IS_HEVC_TYPE > 0)
#define INST_6TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_6TH_USE, \
                        INST_ENC_6TH_VIDEO_WIDTH, \
                        INST_ENC_6TH_VIDEO_HEIGHT, \
                        INST_ENC_6TH_MAX_FRAMEBUFFERS)
#else
#define INST_6TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_6TH_USE, \
                        INST_ENC_6TH_VIDEO_WIDTH, \
                        INST_ENC_6TH_VIDEO_HEIGHT, \
                        INST_ENC_6TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT5_SIZE ARRAY_MBYTE(INST_6TH_ENC_PROC_SIZE)

#if (INST_ENC_7TH_IS_HEVC_TYPE > 0)
#define INST_7TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_7TH_USE, \
                        INST_ENC_7TH_VIDEO_WIDTH, \
                        INST_ENC_7TH_VIDEO_HEIGHT, \
                        INST_ENC_7TH_MAX_FRAMEBUFFERS)
#else
#define INST_7TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_7TH_USE, \
                        INST_ENC_7TH_VIDEO_WIDTH, \
                        INST_ENC_7TH_VIDEO_HEIGHT, \
                        INST_ENC_7TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT6_SIZE ARRAY_MBYTE(INST_7TH_ENC_PROC_SIZE)

#if (INST_ENC_8TH_IS_HEVC_TYPE > 0)
#define INST_8TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_8TH_USE, \
                        INST_ENC_8TH_VIDEO_WIDTH, \
                        INST_ENC_8TH_VIDEO_HEIGHT, \
                        INST_ENC_8TH_MAX_FRAMEBUFFERS)
#else
#define INST_8TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_8TH_USE, \
                        INST_ENC_8TH_VIDEO_WIDTH, \
                        INST_ENC_8TH_VIDEO_HEIGHT, \
                        INST_ENC_8TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT7_SIZE ARRAY_MBYTE(INST_8TH_ENC_PROC_SIZE)

#if (INST_ENC_9TH_IS_HEVC_TYPE > 0)
#define INST_9TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_9TH_USE, \
                        INST_ENC_9TH_VIDEO_WIDTH, \
                        INST_ENC_9TH_VIDEO_HEIGHT, \
                        INST_ENC_9TH_MAX_FRAMEBUFFERS)
#else
#define INST_9TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_9TH_USE, \
                        INST_ENC_9TH_VIDEO_WIDTH, \
                        INST_ENC_9TH_VIDEO_HEIGHT, \
                        INST_ENC_9TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT8_SIZE ARRAY_MBYTE(INST_9TH_ENC_PROC_SIZE)

#if (INST_ENC_10TH_IS_HEVC_TYPE > 0)
#define INST_10TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_10TH_USE, \
                        INST_ENC_10TH_VIDEO_WIDTH, \
                        INST_ENC_10TH_VIDEO_HEIGHT, \
                        INST_ENC_10TH_MAX_FRAMEBUFFERS)
#else
#define INST_10TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_10TH_USE, \
                        INST_ENC_10TH_VIDEO_WIDTH, \
                        INST_ENC_10TH_VIDEO_HEIGHT, \
                        INST_ENC_10TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT9_SIZE ARRAY_MBYTE(INST_10TH_ENC_PROC_SIZE)

#if (INST_ENC_11TH_IS_HEVC_TYPE > 0)
#define INST_11TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_11TH_USE, \
                        INST_ENC_11TH_VIDEO_WIDTH, \
                        INST_ENC_11TH_VIDEO_HEIGHT, \
                        INST_ENC_11TH_MAX_FRAMEBUFFERS)
#else
#define INST_11TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_11TH_USE, \
                        INST_ENC_11TH_VIDEO_WIDTH, \
                        INST_ENC_11TH_VIDEO_HEIGHT, \
                        INST_ENC_11TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT10_SIZE ARRAY_MBYTE(INST_11TH_ENC_PROC_SIZE)

#if (INST_ENC_12TH_IS_HEVC_TYPE > 0)
#define INST_12TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_12TH_USE, \
                        INST_ENC_12TH_VIDEO_WIDTH, \
                        INST_ENC_12TH_VIDEO_HEIGHT, \
                        INST_ENC_12TH_MAX_FRAMEBUFFERS)
#else
#define INST_12TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_12TH_USE, \
                        INST_ENC_12TH_VIDEO_WIDTH, \
                        INST_ENC_12TH_VIDEO_HEIGHT, \
                        INST_ENC_12TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT11_SIZE ARRAY_MBYTE(INST_12TH_ENC_PROC_SIZE)

#if (INST_ENC_13TH_IS_HEVC_TYPE > 0)
#define INST_13TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_13TH_USE, \
                        INST_ENC_13TH_VIDEO_WIDTH, \
                        INST_ENC_13TH_VIDEO_HEIGHT, \
                        INST_ENC_13TH_MAX_FRAMEBUFFERS)
#else
#define INST_13TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_13TH_USE, \
                        INST_ENC_13TH_VIDEO_WIDTH, \
                        INST_ENC_13TH_VIDEO_HEIGHT, \
                        INST_ENC_13TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT12_SIZE ARRAY_MBYTE(INST_13TH_ENC_PROC_SIZE)

#if (INST_ENC_14TH_IS_HEVC_TYPE > 0)
#define INST_14TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_14TH_USE, \
                        INST_ENC_14TH_VIDEO_WIDTH, \
                        INST_ENC_14TH_VIDEO_HEIGHT, \
                        INST_ENC_14TH_MAX_FRAMEBUFFERS)
#else
#define INST_14TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_14TH_USE, \
                        INST_ENC_14TH_VIDEO_WIDTH, \
                        INST_ENC_14TH_VIDEO_HEIGHT, \
                        INST_ENC_14TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT13_SIZE ARRAY_MBYTE(INST_14TH_ENC_PROC_SIZE)

#if (INST_ENC_15TH_IS_HEVC_TYPE > 0)
#define INST_15TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_15TH_USE, \
                        INST_ENC_15TH_VIDEO_WIDTH, \
                        INST_ENC_15TH_VIDEO_HEIGHT, \
                        INST_ENC_15TH_MAX_FRAMEBUFFERS)
#else
#define INST_15TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_15TH_USE, \
                        INST_ENC_15TH_VIDEO_WIDTH, \
                        INST_ENC_15TH_VIDEO_HEIGHT, \
                        INST_ENC_15TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT14_SIZE ARRAY_MBYTE(INST_15TH_ENC_PROC_SIZE)

#if (INST_ENC_16TH_IS_HEVC_TYPE > 0)
#define INST_16TH_ENC_PROC_SIZE VPU_HEVC_ENC_CAL_PROCBUFFER( \
                        INST_ENC_16TH_USE, \
                        INST_ENC_16TH_VIDEO_WIDTH, \
                        INST_ENC_16TH_VIDEO_HEIGHT, \
                        INST_ENC_16TH_MAX_FRAMEBUFFERS)
#else
#define INST_16TH_ENC_PROC_SIZE VPU_CAL_ENC_PROCBUFFER( \
                        INST_ENC_16TH_USE, \
                        INST_ENC_16TH_VIDEO_WIDTH, \
                        INST_ENC_16TH_VIDEO_HEIGHT, \
                        INST_ENC_16TH_MAX_FRAMEBUFFERS)
#endif
#define ENC_EXT15_SIZE ARRAY_MBYTE(INST_16TH_ENC_PROC_SIZE)

#endif//DT_BINDINGS_PMAP_TCC897X_VIDEO_H
