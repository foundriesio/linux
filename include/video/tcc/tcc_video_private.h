/****************************************************************************
 *   FileName    : tcc_video_private.h
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

#ifndef VIDEO_PRIVATE_H_
#define VIDEO_PRIVATE_H_

#include "autoconf.h"

#define UMP_SW_BLOCK_SIZE 		(2 * 1024)  /* 2kB, remember to keep the ()s */
#define UMP_SW_BLOCK_MAX_CNT  	128

#if defined(CONFIG_SUPPORT_TCC_G2V2_VP9)
#include "TCC_VP9DEC.h"
#endif

#define VPU_BUFFER_MANAGE_COUNT 6

#if defined(CONFIG_VIOC_MAP_DECOMP)
#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
#include "TCC_VPU_4K_D2_CODEC.h"

#define MC_LUMA_TABLE_SIZE     WAVE5_FBC_LUMA_TABLE_SIZE
#define MC_CHROMA_TABLE_SIZE   WAVE5_FBC_CHROMA_TABLE_SIZE
typedef struct vpu_4K_D2_dec_MapConv_info_t hevc_MapConv_info_t;
#else
#include "TCC_HEVCDEC.h"

#define MC_LUMA_TABLE_SIZE     WAVE4_FBC_LUMA_TABLE_SIZE
#define MC_CHROMA_TABLE_SIZE   WAVE4_FBC_CHROMA_TABLE_SIZE
typedef struct hevc_dec_MapConv_info_t hevc_MapConv_info_t;
#endif
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) || defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
typedef struct hevc_userdata_output_t
{
   int version;
   int struct_size;
   int bit_depth;

   int colour_primaries;
   int transfer_characteristics;
   int matrix_coefficients;

   int display_primaries_x[3];
   int display_primaries_y[3];
   int white_point_x;
   int white_point_y;
   int max_display_mastering_luminance;
   int min_display_mastering_luminance;

   int max_content_light_level;
   int max_pic_average_light_level;

   int eotf;
   int static_metadata_descriptor;
} hevc_userdata_output_t;
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
typedef struct dolby_layer_info_t
{
   unsigned int offset[3];
   unsigned int format;
   unsigned int width;
   unsigned int height;
   unsigned int bit10;
} dolby_layer_info_t;

typedef struct dolby_vision_info_t
{
   unsigned int reg_addr;
   unsigned int md_hdmi_addr;
   unsigned int el_offset[3];
   unsigned int el_buffer_width;
   unsigned int el_buffer_height;
   unsigned int el_frame_width;
   unsigned int el_frame_height;
   unsigned int osd_addr;
   unsigned int reg_out_type;	// 0: DOVI, 1: HDR10, 2: SDR
}dolby_vision_info_t;
#endif

enum optional_info_description {
	VID_OPT_BUFFER_WIDTH = 0,			//Buffer width
	VID_OPT_BUFFER_HEIGHT = 1,			//Buffer height
	VID_OPT_FRAME_WIDTH = 2,			//Frame Width
	VID_OPT_FRAME_HEIGHT = 3,			//Frame Height
	VID_OPT_BUFFER_ID = 4,				//buffer_id
	VID_OPT_TIMESTAMP = 5,				//timeStamp
	VID_OPT_SYNC_TIME = 6,				//curTime
	VID_OPT_FLAGS = 7,					//flags
	VID_OPT_FPS = 8,					//framerate
	VID_OPT_MVC_DISP_IDX = 9,			//display out index for MVC
	VID_OPT_MVC_ADDR_0 = 10,			//MVC Base addr0
	VID_OPT_MVC_ADDR_1 = 11,			//MVC Base addr1
	VID_OPT_MVC_ADDR_2 = 12,			//MVC Base addr2
	VID_OPT_VSYNC_SUPPORT = 13,			//Vsync enable field
	VID_OPT_DISP_OUT_IDX = 14,			//display out index of VPU
	VID_OPT_VPU_INST_IDX = 15,			//vpu instance-index.
	VID_OPT_HAVE_MC_INFO = 16,			//Have mapConv_info.
	VID_OPT_LOCAL_PLAYBACK = 17,		//local playback : 1, streaming: 0
	VID_OPT_PLAYER_IDX = 18, 			//Player Index for PIP
	VID_OPT_WIDTH_STRIDE = 19,			//stride_width
	VID_OPT_HEIGHT_STRIDE = 20,			//stride_height
	VID_OPT_BIT_DEPTH = 21, 			//Bit Depth // 1:10 bit(used 16 bit LSB), 11:10 bit (real 10bit),
	VID_OPT_HAVE_USERDATA = 22,			//Have userData_info. (for HDR10)
	VID_OPT_HAVE_DTRC_INFO = 23,		//Have dtrc_info.
	VID_OPT_HAVE_DOLBYVISION_INFO = 24,	//Have Dolby-Hdr-info.
	VID_OPT_CONTENT_TYPE = 25,			//Content type // 0: SDR content, 1: DV without Backward Compatibility, 2: DV with Backward Compatibility
	VID_OPT_PLAYER_NOW_MS = 26,
	VID_OPT_RESERVED_1 = 27,
	VID_OPT_RESERVED_2 = 28,
	VID_OPT_RESERVED_3 = 29,
	VID_OPT_MAX
};

typedef struct TCC_PLATFORM_PRIVATE_PMEM_INFO
{
	unsigned int gralloc_phy_address;			// Never change the value and position(1st)!!
	unsigned int width;							// Width for main video
	unsigned int height;						// Height for main video
	unsigned int format;						// Color format for main video
	unsigned int offset[3]; 					// VPU Address for main video
	unsigned int optional_info[VID_OPT_MAX];	// Optional information for current video

	unsigned char name[6];
	unsigned int unique_addr;					// Unique address for each playback
	unsigned int copied; 						// To check if decoded data is copied into gralloc buffer
#if defined(CONFIG_VIOC_MAP_DECOMP)
	hevc_MapConv_info_t mapConv_info;
#endif
#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) || defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	hevc_userdata_output_t userData_Info;
#endif
#if defined(CONFIG_VIOC_DTRC_DECOMP)
	vp9_compressed_info_t dtrcConv_info;
#endif
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	dolby_vision_info_t dolbyVision_info;
#endif
} TCC_PLATFORM_PRIVATE_PMEM_INFO;

//For Linux OMX
typedef enum _CONTENT_TYPES {
	TYPE_IMAGE = 0,
	TYPE_VIDEO = 1,
} CONTENT_TYPES;

typedef enum _DISP_BIT {
	BIT_8  =  8,
	BIT_10 = 10,
	BIT_16 = 16,
} DISP_BIT;

typedef enum _DEC_FLAGS {
	DEC_FLAGS_INTERLACED           = 0x00000001,
	DEC_FLAGS_INTERLACED_ODD_FIRST = 0x00000002,

	DEC_FLAGS_RESOLUTION_CHANGE    = 0x00000004,

	DEC_FLAGS_USE_MAP_CONV         = 0x00000010,
	DEC_FLAGS_USE_DTRC             = 0x00000020,
	DEC_FLAGS_USE_10BIT_2BYTE      = 0x10000000,
	DEC_FLAGS_USE_10BIT_REAL       = 0x20000000,

	DEC_FLAGS_DISCONTINUITY        = 0x80000000,
} DEC_FLAGS;

typedef struct _CropInfo {
	int iCropLeft;
	int iCropTop;
	int iCropWidth;
	int iCropHeight;
} CropInfo;

typedef struct _StrideInfo {
	int iY;
	int iCbCr;
} StrideInfo;

typedef struct _SubTitleInfo {
	int iSubEnable;
	int iSubAddr;
	int iSubIdx;
	int iSubWidth;
	int iSubHeight;
	int iSuboffset_x;
	int iSuboffset_y;
} SubTitleInfo;

typedef struct _tcc_video_out_info {
    int mType;                   // 0:Image, 1:Video
    int mFPS;
    int mErrMB;
    int mFlags;                  //
    int mWidth;
    int mHeight;
    int mDispIdx;
    int mUniqueId;
    int mDropFrame;
    int mContentType;            // 0: SDR content, 1: DV without Backward Compatibility,
                                 //  2: DV with Backward Compatibility
    int mColorFormat;            // 4:2:0 sp = 5,
                                 // if codef is mjpeg, 0 - 4:2:0,          1 - 4:2:2,
                                 //                   2 - 4:2:2 vertical, 3 - 4:4:4, 4 - 4:0:0

	float mAspectratio;

	unsigned int pCurrOut[2][3]; // PA/VA , Y/Cb/Cr
	unsigned int pMVCInfo[2][3]; // PA/VA , Y/Cb/Cr

	CropInfo     stCropInfo;
	StrideInfo   stStride;
	SubTitleInfo stSubTitileInfo;

	int mDolbyHDREnable;
    int mHEVCUserDataEnalbe;

#if defined(CONFIG_VIOC_MAP_DECOMP)
	hevc_MapConv_info_t stHEVCMapConv;
#endif
#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) || defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	hevc_userdata_output_t  stHEVCUserInfo;
#endif
#if defined(CONFIG_VIOC_DTRC_DECOMP)
	vp9_compressed_info_t   stDTRCInfo;
#endif
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	dolby_vision_info_t dolbyVision_info;
#endif
} tcc_video_out_info;
#endif
