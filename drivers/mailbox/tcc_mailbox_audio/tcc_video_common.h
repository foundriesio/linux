/****************************************************************************
 *   FileName    : tcc_vp9_ioctl.h
 *   Description :
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved

This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS" and nothing contained in this source code shall constitute any express or implied warranty of any kind, including without limitation, any warranty of merchantability, fitness for a particular purpose or non-infringement of any patent, copyright or other third party intellectual property right. No warranty is made, express or implied, regarding the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability arising from, out of or in connection with this source code or the use in the source code.
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement between Telechips and Company.
*
****************************************************************************/
#ifndef phys_addr_t
#define phys_addr_t unsigned int
#endif

#ifndef _VIDEO_COMMON_H_
#define _VIDEO_COMMON_H_

typedef enum {
    VPU_DEC = 0,
    VPU_DEC_EXT,
    VPU_DEC_EXT2,
    VPU_DEC_EXT3,
    VPU_ENC,
    VPU_ENC_EXT,
    VPU_ENC_EXT2,
    VPU_ENC_EXT3,
    VPU_MAX
}vputype;

typedef enum
{
    DEC_WITH_ENC,
    DEC_ONLY // memory will be allocaed without regarding encoder!!
} Open_Type;

typedef struct
{
    vputype				type;
    unsigned int		width;
    unsigned int		height;
    unsigned int		bitrate;
    unsigned int		framerate;
    unsigned int		isSWCodec;	//In case of Encoder type, this value use to decide if clock limitation apply or not.
}CONTENTS_INFO;

typedef struct
{
    Open_Type type;
    unsigned int opened_cnt;
}OPENED_sINFO;

typedef struct
{
    int type;
    int nInstance;
}INSTANCE_INFO;

typedef enum
{
    BUFFER_ELSE,
    BUFFER_WORK,
    BUFFER_STREAM,
    BUFFER_SEQHEADER,
    BUFFER_FRAMEBUFFER,
    BUFFER_PS,
    BUFFER_SLICE,
    BUFFER_USERDATA
} Buffer_Type;

typedef struct
{
    unsigned int request_size;
    phys_addr_t phy_addr;
    void *kernel_remap_addr;
    Buffer_Type buffer_type;
}MEM_ALLOC_INFO_t;

typedef struct dec_user_info_t
{
    unsigned int  bitrate_mbps;             //!< video BitRate (Mbps)
    unsigned int  frame_rate;               //!< video FrameRate (fps)
    unsigned int  m_bStillJpeg;              //!< If this is set, content is jpeg only file (ex. **.jpg)
    unsigned int  jpg_ScaleRatio;           ////!< 0 ( Original Size )
                                            //!< 1 ( 1/2 Scaling Down )
                                            //!< 2 ( 1/4 Scaling Down )
                                            //!< 3 ( 1/8 Scaling Down )
}dec_user_info_t;

#define VPU_SET_CLK 		 	20
#define VPU_SET_MEM_ALLOC_MODE 	30
#define VPU_FORCE_CLK_CLOSE  	100
#define VPU_CHECK_CODEC_STATUS  200
#define VPU_GET_INSTANCE_IDX    201
#define VPU_CLEAR_INSTANCE_IDX  202
#define VPU_USE_WAIT_LIST       203
#define VPU_SET_RENDERED_FRAMEBUFFER  204
#define VPU_GET_RENDERED_FRAMEBUFFER  205
#define VPU_ENABLE_JPU_CLOCK  	206
#define VPU_DISABLE_JPU_CLOCK  	207
#define VPU_CHECK_INSTANCE_AVAILABLE 208

#define VPU_GET_FREEMEM_SIZE 	40
#define VPU_HW_RESET		 	50

////////////////////////////////////////////////////////////////////////////////////////
/*DECODER */
#define DEVICE_INITIALIZE				100
#define V_DEC_INIT						11	//!< init
#define V_DEC_SEQ_HEADER				12	//!< decode sequence header
#define V_DEC_GET_INFO					13
#define V_DEC_REG_FRAME_BUFFER			14	//!< register frame buffer
#define V_DEC_REG_FRAME_BUFFER2			15	//!< register frame buffer
#define V_DEC_DECODE					16	//!< decode
#define V_DEC_BUF_FLAG_CLEAR			17	//!< display buffer flag clear
#define V_DEC_FLUSH_OUTPUT				18	//!< flush delayed output frame
#define V_GET_RING_BUFFER_STATUS		19
#define V_FILL_RING_BUFFER_AUTO						20    //!< Fill the ring buffer
#define V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY	21    //!< Get initial Info for ring buffer use
#define V_DEC_CLOSE									22	//!< close

#define V_DEC_ALLOC_MEMORY				23
#define V_DEC_GENERAL_RESULT			24
#define V_DEC_INIT_RESULT				25
#define V_DEC_SEQ_HEADER_RESULT			26
#define V_DEC_DECODE_RESULT				27
#define V_DEC_UPDATE_RINGBUF_WP			28

#define V_GET_RING_BUFFER_STATUS_RESULT	29
#define V_FILL_RING_BUFFER_AUTO_RESULT	30
#define V_DEC_UPDATE_RINGBUF_WP_RESULT	31
#define V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT 32

#define V_GET_VPU_VERSION					33
#define V_GET_VPU_VERSION_RESULT			34

#define V_DEC_SWRESET						35

#define V_DEC_FLUSH_OUTPUT_RESULT			36
#define V_DEC_FREE_MEMORY                   37
#endif
