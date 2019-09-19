/*
 *   FileName    : tcc_video_common.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef phys_addr_t
#define phys_addr_t unsigned int
#endif

#ifndef kVpu32bitUserSpaceIoctlMode
#define kVpu32bitUserSpaceIoctlMode 32264
#endif

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC899X) \
	|| defined(CONFIG_ARCH_TCC901X)
#   define _VPU_D6_INCLUDE_
#else
#   define _VPU_C7_INCLUDE_
#endif

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC803X)
#   define _HEVC_D8_INCLUDE_
#elif defined(CONFIG_ARCH_TCC898X)
#   define _HEVC_D9_INCLUDE_
#endif

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC898X) \
	|| defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) \
	|| defined(CONFIG_ARCH_TCC901X)
#   define _JPU_C6_INCLUDE_
#else
#   define _JPU_C5_INCLUDE_
#endif

#ifndef _VIDEO_COMMON_H_
#define _VIDEO_COMMON_H_

#if defined(CONFIG_VIOC_MAP_DECOMP)
#   define _MAP_COMP_SUPPORT_
#endif

#if defined(CONFIG_VIOC_DTRC_DECOMP)
#   define _DTRC_COMP_SUPPORT_
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
#   define _VPU_4K_D2_INCLUDE_
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) || defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
#   define _HEVC_USER_DATA_SUPPORT_
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#   define _DOLBY_VISION_SUPPORT_
#endif

#ifndef CONFIG_ANDROID
#   define DO_NOT_USE_REMAP_ADDR
#endif

typedef enum {
    VPU_DEC = 0,
    VPU_DEC_EXT,
    VPU_DEC_EXT2,
    VPU_DEC_EXT3,
    VPU_DEC_EXT4,
    VPU_ENC,
    VPU_ENC_EXT,
    VPU_ENC_EXT2,
    VPU_ENC_EXT3,
    VPU_MAX
} vputype, E_VPU_TYPE;

typedef enum {
    DEC_WITH_ENC,
    DEC_ONLY // memory will be allocaed without regarding encoder!!
} Open_Type;

typedef struct {
    E_VPU_TYPE      type;
    unsigned int    width;
    unsigned int    height;
    unsigned int    bitrate;
    unsigned int    framerate;
    unsigned int    isSWCodec;  // in case of encoder type, used to decide whether applying clock limitation or not.
} CONTENTS_INFO;

typedef struct {
    Open_Type type;
    unsigned int opened_cnt;
} OPENED_sINFO;

typedef struct {
    int type;
    int nInstance;
} INSTANCE_INFO;

typedef enum {
    BUFFER_ELSE,
    BUFFER_WORK,
    BUFFER_STREAM,
    BUFFER_SEQHEADER,
    BUFFER_FRAMEBUFFER,
    BUFFER_PS,
    BUFFER_SLICE,
    BUFFER_USERDATA
} Buffer_Type, E_BUFFER_TYPE;

typedef struct {
    unsigned int request_size;
    phys_addr_t phy_addr;
    void *kernel_remap_addr;
    Buffer_Type buffer_type;
} MEM_ALLOC_INFO_t;

typedef struct {
    unsigned int        request_size;
    phys_addr_t         phy_addr;
    unsigned long long  kernel_remap_addr;
    E_BUFFER_TYPE       buffer_type;
} MEM_ALLOC_INFO_EX_t;

typedef struct dec_user_info_t {
    unsigned int  bitrate_mbps;             //!< video BitRate (Mbps)
    unsigned int  frame_rate;               //!< video FrameRate (fps)
    unsigned int  m_bStillJpeg;             //!< If this is set, content is jpeg only file (ex. **.jpg)
    unsigned int  jpg_ScaleRatio; // 0: Original Size, 1: 1/2 Scaling Down, 2: 1/4 Scaling Down, 3: 1/8 Scaling Down
} dec_user_info_t;


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
/* Macro constants of VPU Command: EN/DECODER */

#define VPU_SET_CLK                     20
#define VPU_SET_MEM_ALLOC_MODE          30
#define VPU_FORCE_CLK_CLOSE             100
#define VPU_CHECK_CODEC_STATUS          200
#define VPU_GET_INSTANCE_IDX            201
#define VPU_CLEAR_INSTANCE_IDX          202
#define VPU_USE_WAIT_LIST               203
#define VPU_SET_RENDERED_FRAMEBUFFER    204
#define VPU_GET_RENDERED_FRAMEBUFFER    205
#define VPU_ENABLE_JPU_CLOCK            206
#define VPU_DISABLE_JPU_CLOCK           207
#define VPU_CHECK_INSTANCE_AVAILABLE    208

#define VPU_TRY_FORCE_CLOSE				500
#define VPU_TRY_CLK_RESTORE				501

#define VPU_GET_FREEMEM_SIZE    40
#define VPU_HW_RESET            50

///////////////////////////////////////////////////////////////////////////////////////////////////
/* Macro constants of VPU Command: DECODER */

#define DEVICE_INITIALIZE               100

#define V_DEC_INIT                      11  //!< init
#define V_DEC_SEQ_HEADER                12  //!< decode sequence header
#define V_DEC_GET_INFO                  13
#define V_DEC_REG_FRAME_BUFFER          14  //!< register frame buffer
#define V_DEC_REG_FRAME_BUFFER2         15  //!< register frame buffer
#define V_DEC_DECODE                    16  //!< decode
#define V_DEC_BUF_FLAG_CLEAR            17  //!< display buffer flag clear
#define V_DEC_FLUSH_OUTPUT              18  //!< flush delayed output frame
#define V_GET_RING_BUFFER_STATUS        19
#define V_FILL_RING_BUFFER_AUTO                     20    //!< Fill the ring buffer
#define V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY  21    //!< Get initial Info for ring buffer use
#define V_DEC_CLOSE                                 22  //!< close

#define V_DEC_ALLOC_MEMORY              30
#define V_DEC_ALLOC_MEMORY_EX           31  // use-case: 32 bit user space, 64 bit kernel space

#define V_DEC_GENERAL_RESULT            50
#define V_DEC_INIT_RESULT               51
#define V_DEC_SEQ_HEADER_RESULT         52
#define V_DEC_DECODE_RESULT             53
#define V_DEC_UPDATE_RINGBUF_WP         54

#define V_GET_RING_BUFFER_STATUS_RESULT 55
#define V_FILL_RING_BUFFER_AUTO_RESULT  56
#define V_DEC_UPDATE_RINGBUF_WP_RESULT  57
#define V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT 58

#define V_GET_VPU_VERSION                   59
#define V_GET_VPU_VERSION_RESULT            60

#define V_DEC_SWRESET                       61

#define V_DEC_FLUSH_OUTPUT_RESULT           62
#define V_DEC_FREE_MEMORY                   63

#define V_DEC_GET_OUTPUT_INFO               64 //!< decode
#define V_DEC_GET_OUTPUT_INFO_RESULT        65

#define V_DEC_OP_CODE_MAX                   V_DEC_GET_OUTPUT_INFO_RESULT

///////////////////////////////////////////////////////////////////////////////////////////////////
/* Macro constants of VPU Command: ENCODER */

#define V_ENC_INIT                  10
#define V_ENC_REG_FRAME_BUFFER      11  // register frame buffer
#define V_ENC_PUT_HEADER            12
#define V_ENC_ENCODE                13
#define V_ENC_CLOSE                 14

#define V_ENC_ALLOC_MEMORY          16
#define V_ENC_GENERAL_RESULT        17
#define V_ENC_INIT_RESULT           18
#define V_ENC_PUT_HEADER_RESULT     19
#define V_ENC_ENCODE_RESULT         20
#define V_ENC_FREE_MEMORY           21

/**
 * Opcode : DECODER for Kernel
 */
#define V_DEC_KERNEL_OP_BASE			(0x10000)
#define VPU_KERNEL_OP_BASE				(0x10000)

#define VPU_SET_CLK_KERNEL 		 		( VPU_KERNEL_OP_BASE + VPU_SET_CLK)
#define VPU_SET_MEM_ALLOC_MODE_KERNEL 	( VPU_KERNEL_OP_BASE + VPU_SET_MEM_ALLOC_MODE)
#define VPU_FORCE_CLK_CLOSE_KERNEL  	( VPU_KERNEL_OP_BASE + VPU_FORCE_CLK_CLOSE)
#define VPU_CHECK_CODEC_STATUS_KERNEL  	( VPU_KERNEL_OP_BASE + VPU_CHECK_CODEC_STATUS)
#define VPU_GET_INSTANCE_IDX_KERNEL    	( VPU_KERNEL_OP_BASE + VPU_GET_INSTANCE_IDX)
#define VPU_CLEAR_INSTANCE_IDX_KERNEL  	( VPU_KERNEL_OP_BASE + VPU_CLEAR_INSTANCE_IDX)
#define VPU_USE_WAIT_LIST_KERNEL       	( VPU_KERNEL_OP_BASE + VPU_USE_WAIT_LIST)
#define VPU_SET_RENDERED_FRAMEBUFFER_KERNEL  ( VPU_KERNEL_OP_BASE + VPU_SET_RENDERED_FRAMEBUFFER)
#define VPU_GET_RENDERED_FRAMEBUFFER_KERNEL  ( VPU_KERNEL_OP_BASE + VPU_GET_RENDERED_FRAMEBUFFER)
#define VPU_ENABLE_JPU_CLOCK_KERNEL  	( VPU_KERNEL_OP_BASE + VPU_ENABLE_JPU_CLOCK)
#define VPU_DISABLE_JPU_CLOCK_KERNEL  	( VPU_KERNEL_OP_BASE + VPU_DISABLE_JPU_CLOCK)
#define VPU_CHECK_INSTANCE_AVAILABLE_KERNEL ( VPU_KERNEL_OP_BASE + VPU_CHECK_INSTANCE_AVAILABLE)

#define VPU_TRY_FORCE_CLOSE_KERNEL 		( VPU_KERNEL_OP_BASE + VPU_TRY_FORCE_CLOSE)
#define VPU_TRY_CLK_RESTORE_KERNEL 		( VPU_KERNEL_OP_BASE + VPU_TRY_CLK_RESTORE)

#define VPU_GET_FREEMEM_SIZE_KERNEL		( VPU_KERNEL_OP_BASE + VPU_GET_FREEMEM_SIZE)
#define VPU_HW_RESET_KERNEL 			( VPU_KERNEL_OP_BASE + VPU_HW_RESET)

#define DEVICE_INITIALIZE_KERNEL 		( V_DEC_KERNEL_OP_BASE + DEVICE_INITIALIZE)
#define V_DEC_INIT_KERNEL		 		( V_DEC_KERNEL_OP_BASE + V_DEC_INIT)
#define V_DEC_SEQ_HEADER_KERNEL			( V_DEC_KERNEL_OP_BASE + V_DEC_SEQ_HEADER)
#define V_DEC_GET_INFO_KERNEL			( V_DEC_KERNEL_OP_BASE + V_DEC_GET_INFO)
#define V_DEC_REG_FRAME_BUFFER_KERNEL	( V_DEC_KERNEL_OP_BASE + V_DEC_REG_FRAME_BUFFER)
#define V_DEC_REG_FRAME_BUFFER2_KERNEL	( V_DEC_KERNEL_OP_BASE + V_DEC_REG_FRAME_BUFFER2)
#define V_DEC_DECODE_KERNEL				( V_DEC_KERNEL_OP_BASE + V_DEC_DECODE)
#define V_DEC_BUF_FLAG_CLEAR_KERNEL		( V_DEC_KERNEL_OP_BASE + V_DEC_BUF_FLAG_CLEAR)
#define V_DEC_FLUSH_OUTPUT_KERNEL		( V_DEC_KERNEL_OP_BASE + V_DEC_FLUSH_OUTPUT)
#define V_GET_RING_BUFFER_STATUS_KERNEL	( V_DEC_KERNEL_OP_BASE + V_GET_RING_BUFFER_STATUS)
#define V_FILL_RING_BUFFER_AUTO_KERNEL	( V_DEC_KERNEL_OP_BASE + V_FILL_RING_BUFFER_AUTO)
#define V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL ( V_DEC_KERNEL_OP_BASE + V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY)
#define V_DEC_CLOSE_KERNEL				( V_DEC_KERNEL_OP_BASE + V_DEC_CLOSE)

#define V_DEC_ALLOC_MEMORY_KERNEL		( V_DEC_KERNEL_OP_BASE + V_DEC_ALLOC_MEMORY)
#define V_DEC_GENERAL_RESULT_KERNEL		( V_DEC_KERNEL_OP_BASE + V_DEC_GENERAL_RESULT)
#define V_DEC_INIT_RESULT_KERNEL		( V_DEC_KERNEL_OP_BASE + V_DEC_INIT_RESULT)
#define V_DEC_SEQ_HEADER_RESULT_KERNEL	( V_DEC_KERNEL_OP_BASE + V_DEC_SEQ_HEADER_RESULT )
#define V_DEC_DECODE_RESULT_KERNEL		( V_DEC_KERNEL_OP_BASE + V_DEC_DECODE_RESULT )
#define V_DEC_UPDATE_RINGBUF_WP_KERNEL	( V_DEC_KERNEL_OP_BASE + V_DEC_UPDATE_RINGBUF_WP)

#define V_GET_RING_BUFFER_STATUS_RESULT_KERNEL	( V_DEC_KERNEL_OP_BASE + V_GET_RING_BUFFER_STATUS_RESULT)
#define V_FILL_RING_BUFFER_AUTO_RESULT_KERNEL	( V_DEC_KERNEL_OP_BASE + V_FILL_RING_BUFFER_AUTO_RESULT)
#define V_DEC_UPDATE_RINGBUF_WP_RESULT_KERNEL	( V_DEC_KERNEL_OP_BASE + V_DEC_UPDATE_RINGBUF_WP_RESULT)
#define V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT_KERNEL ( V_DEC_KERNEL_OP_BASE + V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT)

#define V_GET_VPU_VERSION_KERNEL		( V_DEC_KERNEL_OP_BASE + V_GET_VPU_VERSION)
#define V_GET_VPU_VERSION_RESULT_KERNEL	( V_DEC_KERNEL_OP_BASE + V_GET_VPU_VERSION_RESULT)

#define V_DEC_SWRESET_KERNEL			( V_DEC_KERNEL_OP_BASE + V_DEC_SWRESET)
#define V_DEC_FLUSH_OUTPUT_RESULT_KERNEL	( V_DEC_KERNEL_OP_BASE + V_DEC_FLUSH_OUTPUT_RESULT)
#define V_DEC_FREE_MEMORY_KERNEL   		( V_DEC_KERNEL_OP_BASE + V_DEC_FREE_MEMORY)

#define V_DEC_GET_OUTPUT_INFO_KERNEL    ( V_DEC_KERNEL_OP_BASE + V_DEC_GET_OUTPUT_INFO)
#define V_DEC_GET_OUTPUT_INFO_RESULT_KERNEL  ( V_DEC_KERNEL_OP_BASE + V_DEC_GET_OUTPUT_INFO_RESULT)

#endif // _VIDEO_COMMON_H_lif defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC803X)
