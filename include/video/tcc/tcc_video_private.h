// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_video_private.h
 * Description: TCC VPU h/w block
 */

#ifndef VIDEO_PRIVATE_H_
#define VIDEO_PRIVATE_H_

#include "../../generated/autoconf.h"

#define UMP_SW_BLOCK_SIZE	(2 * 1024) // 2kB, remember to keep the ()s
#define UMP_SW_BLOCK_MAX_CNT	128

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

#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) \
	|| defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
typedef struct hevc_userdata_output_t {
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

typedef struct hdr10p_userdata_info_t {
	unsigned int mOUI; //fixed value, 0x008B8490 for PB0 ~ PB4
	/*
	 *	PB04 :
	 *		[7:6] : application version [1:0]
	 *		[5:1] : display maximum luminance[4:0]
	 *		[0]   : reserved[0]
	 *	PB05 : average maxrgb
	 *	PB06 : distribution values 0 @1%
	 *	PB07 : distribution values 1 @DistributionY99
	 *	PB08 : distribution values 2 @DistributionY100nit cd/m2
	 *	PB09 : distribution values 3 @25%
	 *	PB10 : distribution values 4 @50%
	 *	PB11 : distribution values 5 @75%
	 *	PB12 : distribution values 6 @90%
	 *	PB13 : distribution values 7 @95%
	 *	PB14 : distribution values 8 @99.98%
	 *	PB15 :
	 *		[7:4] : num bezier curve anchors[3:0]
	 *		[3:0] : knee_pointx[9:6]
	 *	PB16 :
	 *		[7:2] : knee_pointx[5:0]
	 *		[1:0] : knee_pointy[9:8]
	 *	PB17 : knee_pointy[7:0]
	 *	PB18 : bezier curve anchors 0
	 *	PB19 : bezier curve anchors 1
	 *	PB20 : bezier curve anchors 2
	 *	PB21 : bezier curve anchors 3
	 *	PB22 : bezier curve anchors 4
	 *	PB23 : bezier curve anchors 5
	 *	PB24 : bezier curve anchors 6
	 *	PB25 : bezier curve anchors 7
	 *	PB26 : bezier curve anchors 8
	 *	PB27 :	// maybe always 0 from the decoder.
	 *		[7] : graphic overlay flag
	 *		[6] : vsif timming mode
	 *		[5:0] : reserved
	 */
	unsigned char mVendorPayload[24];	//PB4 ~ PB27

	//real used payload among 24, PB4 ~ PB27.
	unsigned char mVendorPayloadLength;
} hdr10p_userdata_info_t;
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
typedef struct dolby_layer_info_t {
	unsigned int offset[3];
	unsigned int format;
	unsigned int width;
	  unsigned int height;
	unsigned int bit10;
} dolby_layer_info_t;

typedef struct dolby_vision_info_t {
	unsigned int reg_addr;
	unsigned int md_hdmi_addr;
	unsigned int osd_addr[2];	// ARGB8888
	unsigned int osd_width;
	unsigned int osd_height;
	unsigned int reg_out_type;	// 0: DOVI, 1: HDR10, 2: SDR

	//////// reference for renderer to extract dolby-reg ////////
	////////  :: from decoder ////////
	unsigned int dv_input_format; // 0: DOVI, 1: HDR10, 2: SDR
	unsigned int dm_phy_addr;
	unsigned int comp_phy_addr;
	unsigned int el_offset[3];
	unsigned int el_buffer_width;
	unsigned int el_buffer_height;
	unsigned int el_frame_width;
	unsigned int el_frame_height;
} dolby_vision_info_t;
#endif

enum optional_info_description {
	VID_OPT_BUFFER_WIDTH = 0,	// Buffer width
	VID_OPT_BUFFER_HEIGHT = 1,	// Buffer height
	VID_OPT_FRAME_WIDTH = 2,	// Frame Width
	VID_OPT_FRAME_HEIGHT = 3,	// Frame Height
	VID_OPT_BUFFER_ID = 4,		// buffer_id
	VID_OPT_TIMESTAMP = 5,		// timeStamp
	VID_OPT_SYNC_TIME = 6,		// curTime
	VID_OPT_FLAGS = 7,		// flags
	VID_OPT_FPS = 8,		// framerate
	VID_OPT_MVC_DISP_IDX = 9,	// display out index for MVC
	VID_OPT_MVC_ADDR_0 = 10,	// MVC Base addr0
	VID_OPT_MVC_ADDR_1 = 11,	// MVC Base addr1
	VID_OPT_MVC_ADDR_2 = 12,	// MVC Base addr2
	VID_OPT_VSYNC_SUPPORT = 13,	// Vsync enable field
	VID_OPT_DISP_OUT_IDX = 14,	// display out index of VPU
	VID_OPT_VPU_INST_IDX = 15,	// vpu instance-index.
	VID_OPT_HAVE_MC_INFO = 16,	// Have mapConv_info.
	VID_OPT_LOCAL_PLAYBACK = 17,	// local playback : 1, streaming: 0
	VID_OPT_PLAYER_IDX = 18,	// Player Index for PIP
	VID_OPT_WIDTH_STRIDE = 19,	// stride_width
	VID_OPT_HEIGHT_STRIDE = 20,	// stride_height
	VID_OPT_BIT_DEPTH = 21,		// Bit Depth
					//  1:10 bit(used 16 bit LSB),
					// 11:10 bit (real 10bit),
	VID_OPT_HAVE_USERDATA = 22,	// Have userData_info. (for HDR10)
	VID_OPT_HAVE_DTRC_INFO = 23,	// Have dtrc_info.
	VID_OPT_HAVE_DOLBYVISION_INFO = 24, // Have Dolby-Hdr-info.
	VID_OPT_CONTENT_TYPE = 25,	// Content type
					// 0: SDR content,
					// 1: DV without Backward Compatibility,
					// 2: DV with Backward Compatibility
	VID_OPT_PLAYER_NOW_MS = 26,
	VID_OPT_HAVE_HDR10P_USERDATA = 27, // Have userData_info. (for HDR10+)
	VID_OPT_RESERVED_1 = 28,
	VID_OPT_RESERVED_2 = 29,
	VID_OPT_RESERVED_3 = 30,	// VSIF index for Dolby Certification
	VID_OPT_RESERVED_4 = 31,	// PUSH_VSYNC_EXT
	VID_OPT_MAX
};

enum vid_opt_flag_description {
	VOF_FIRST_SEEKED_FRAME  = 0x00000001,
	VOF_DISPLAY_DIRECTLY    = 0x00000002,
	VOF_DXB_IPTV            = 0x00000010,
	VOF_OTT_IPTV            = 0x00000020,
	VOF_DISCONTINUITY_FRAME = 0x00002000,
	VOF_M2M_PATH_FRAME      = 0x00010000,
	VOF_OTF_PTAH_FRAME      = 0x00020000,
	VOF_ODD_FIRST_FRAME     = 0x10000000,
	VOF_INTERLACE_ONE_FIELD = 0x20000000,
	VOF_INTERLACED_FRAME    = 0x40000000,
};

typedef struct TCC_PLATFORM_PRIVATE_PMEM_INFO {
	// Never change the value and position(1st)
	unsigned int gralloc_phy_address;
	unsigned int width;	// Width for main video
	unsigned int height;	// Height for main video
	unsigned int format;	// Color format for main video
	unsigned int offset[3];	// VPU Address for main video

	// Optional information for current video
	unsigned int optional_info[VID_OPT_MAX];

	unsigned char name[6];
	unsigned int unique_addr; // Unique address for each playback

	// To check if decoded data is copied into gralloc buffer
	unsigned int copied;
#if defined(CONFIG_VIOC_MAP_DECOMP)
	hevc_MapConv_info_t mapConv_info;
#endif
#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) \
	|| defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
	union {
		hevc_userdata_output_t userData_Info;
		hdr10p_userdata_info_t hdr10p_Info;
	};
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
	int mType;		// 0:Image, 1:Video
	int mFPS;
	int mErrMB;
	int mFlags;
	int mWidth;
	int mHeight;
	int mDispIdx;
	int mUniqueId;
	int mDropFrame;
	int mContentType;	// 0: SDR content,
				// 1: DV without Backward Compatibility,
				// 2: DV with Backward Compatibility
	int mColorFormat;	// 4:2:0 sp = 5,
				// if codef is mjpeg,
				// 0 - 4:2:0,
				// 1 - 4:2:2,
				// 2 - 4:2:2 vertical,
				// 3 - 4:4:4,
				// 4 - 4:0:0

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
#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC) \
	|| defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
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
