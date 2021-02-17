// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : TCC_VPU_C5_CODEC.h
 * Description: TCC VPU h/w block
 */

#ifndef _TCC93_88XX_VPU_CODEC_H_
#define _TCC93_88XX_VPU_CODEC_H_

#include "TCCxxxx_VPU_CODEC_COMMON.h"

//#define USE_VPU_DISPLAY_MODE  // use ring buffer

#define MAX_NUM_INSTANCE        4

#define RETCODE_ERR_STRIDE_ZERO_OR_ALIGN8   100
#define RETCODE_ERR_MIN_RESOLUTION          101
#define RETCODE_ERR_MAX_RESOLUTION          102
#define RETCODE_ERR_SEQ_INIT_HANGUP         103
#define RETCODE_H264ERR_PROFILE             110
#define RETCODE_VC1ERR_COMPLEX_PROFILE      120
#define RETCODE_H263ERR_ANNEX_D             130
#define RETCODE_H263ERR_ANNEX_EFG           131
#define RETCODE_H263ERR_UFEP                132
#define RETCODE_H263ERR_ANNEX_D_PLUSPTYPE   133
#define RETCODE_H263ERR_ANNEX_EF_PLUSPTYPE  134
#define RETCODE_H263ERR_ANNEX_NRS_PLUSPTYPE 135
#define RETCODE_H263ERR_ANNEX_PQ_MPPTYPE    136
//Unlimited Unrestricted Motion Vectors Indicator(UUI)
#define RETCODE_H263ERR_UUI                 137
//Slice Structure Submode(Rectangular Slices or Arbitaray Slice Ordering)
#define RETCODE_H263ERR_SSS                 138
#define RETCODE_MPEG4ERR_OBMC_DISABLE       140
//Sprite Coding Mode(include GMC)
#define RETCODE_MPEG4ERR_SPRITE_ENABLE      141
//Scalable coding
#define RETCODE_MPEG4ERR_MP4ERR_SCALABILITY 142
#define RETCODE_MPEG4ERR_SPK_FORMAT         144
#define RETCODE_MPEG4ERR_SPK_VERSION        145
#define RETCODE_MPEG4ERR_SPK_RESOLUTION     146
#define RETCODE_MPEG2ERR_CHROMA_FORMAT      150
#define RETCODE_RVERR_SUB_MOF_FLAG31TO16    160
#define RETCODE_RVERR_SUB_MOF_FLAG16TO0     161

#ifndef RETCODE_WRAP_AROUND
#define RETCODE_WRAP_AROUND                 -10
#endif

#define CODE_BUF_SIZE           (152 * 1024)
#define FMO_SLICE_SAVE_BUF_SIZE (32)
#define WORK_BUF_SIZE           ((160 * 1024) \
				+ (MAX_NUM_INSTANCE*41*1024) + 100 * 1024)
#define PARA_BUF2_SIZE          (2 * 1024)
#define PARA_BUF_SIZE           (10 * 1024)
#define SEC_AXI_BUS_BUF_SIZE    (128 * 1024)
#define WORK_CODE_PARA_BUF_SIZE (CODE_BUF_SIZE + WORK_BUF_SIZE \
				+ PARA_BUF2_SIZE + PARA_BUF_SIZE \
				+ SEC_AXI_BUS_BUF_SIZE)

#define LARGE_STREAM_BUF_SIZE   0x200000
#define SLICE_SAVE_SIZE         0x100000
#define PS_SAVE_SIZE            0x080000


#ifndef INC_DEVICE_TREE_PMAP

//------------------------------------------------------------------------------
// decode struct and definition
//------------------------------------------------------------------------------

/*!
 * represents rectangular window information in a frame
 */
typedef struct pic_crop_t {
	unsigned int m_iCropLeft;
	unsigned int m_iCropTop;
	unsigned int m_iCropRight;
	unsigned int m_iCropBottom;
} pic_crop_t;

/*!
 * data structure to get information necessary to
 * start decoding from the decoder (this is an output parameter)
 */
typedef struct dec_initial_info_t {
	// {(PicX+15)/16} * 16  (this width  will be used while allocating
	//  decoder frame buffers. picWidth  is a multiple of 16)
	int m_iPicWidth;
	// {(PicY+15)/16} * 16  (this height will be used while allocating
	//  decoder frame buffers. picHeight is a multiple of 16)
	int m_iPicHeight;
	// decoded picture frame rate residual(number of time units of
	//  a clock operating at the frequency[m_iFrameRateDiv] Hz,
	//  frameRateInfo = m_uiFrameRateRes/m_uiFrameRateDiv
	unsigned int m_uiFrameRateRes;
	// decoded picture frame rate unit number in Hz
	unsigned int m_uiFrameRateDiv;
	// the minimum number of frame buffers that are required for decoding.
	//  application must allocate at least this number of frame buffers.
	int m_iMinFrameBufferCount;
	// minimum frame buffer size
	int m_iMinFrameBufferSize;
	// recommended size of to save slice. this value is determined
	//  as a quater of the memory size for one raw YUV image in KB unit.
	int m_iNormalSliceSize;
	// recommended size of to save slice in worst case.
	//  this value is determined as a half of the memory size
	//  for one raw YUV image in KB unit.
	int m_iWorstSliceSize;

	//////// H264/AVC only param ////////

	// represents rectangular window information in a frame
	pic_crop_t m_iAvcPicCrop;
	// syntax element which is used to make level in H.264.
	//  used only in H.264 case.
	int m_iAvcConstraintSetFlag[4];
	// These are H.264 SPS or PPS syntax element
	// [bit 0  ] direct_8x8_inference_flag in H.264 SPS
	int m_iAvcParamerSetFlag;
	// maximum display frame buffer delay to process reordering
	//  in case of H.264
	int m_iFrameBufDelay;

	//////// MPEG-4 only param ////////

	int m_iM4vDataPartitionEnable;  // 0: disable, 1: enable
	int m_iM4vReversibleVlcEnable;  // 0: disable, 1: enable
	int m_iM4vShortVideoHeader;     // 0: disable, 1: enable
	int m_iM4vH263AnnexJEnable;     // 0: disable, 1: enable

	//////// VC-1 only param ////////
	// this is only available in VC1 and indicates the value of
	//  "Progressive Segmented Frame"
	int m_iVc1Psf;

	//////// Additional Info ////////

	// profile of the decoded stream
	int m_iProfile;
	// level of the decoded stream
	int m_iLevel;
	// when this value is 1, decoded stream will be decoded into
	//  both progressive or interlace frame. otherwise,
	//  all the frames will be progressive.
	//  In H.264, this is frame_mbs_only_flag.
	int m_iInterlace;
	// aspect rate information. if this value is 0,
	//  then aspect ratio information is not present
	int m_iAspectRateInfo;
	// reports the reason of 'RETCODE_CODEC_SPECOUT' or
	//  'RETCODE_INVALID_STRIDE' error
	int m_iReportErrorReason;

	//////// MJPEG only param ////////
	// MJPEG source chroma format(only for TCC891x/88xx/93XX)
	//  0 - 4:2:0,
	//  1 - 4:2:2,
	//  2 - 4:2:2 vertical,
	//  3 - 4:4:4,
	//  4 - 4:0:0
	int m_iMjpg_sourceFormat;
	// If  thumbnail image is exist, This field is set.
	int m_iMjpg_ThumbnailEnable;
	// minimum frame buffer size for JPEG only
	//  0: Original Size
	//  1: 1/2 Scaling Down
	//  2: 1/4 Scaling Down
	//  3: 1/8 Scaling Down
	int m_iMjpg_MinFrameBufferSize[4];
} dec_initial_info_t;

/*!
 * data structure for initializing Video unit
 */
typedef struct dec_init_t {
	// physical[0] and virtual[1] address of a working space of the decoder.
	//  This working buffer space consists of work buffer, code buffer,
	//  and parameter buffer.
	codec_addr_t m_BitWorkAddr[2];
	codec_addr_t m_RegBaseVirtualAddr;  // virtual address BIT_BASE

	//////// Bitstream Info ////////

	int m_iBitstreamFormat;             // bitstream format
	codec_addr_t m_BitstreamBufAddr[2]; // multiple of 4
	int m_iBitstreamBufSize;            // multiple of 1024
	// frame width from demuxer or etc
	int m_iPicWidth;
	// frame height from demuxer or etc
	int m_iPicHeight;

	//////// Decoding Options ////////
// (default)
#define M4V_DEBLK_DISABLE            0
// mpeg-4 deblocking
#define M4V_DEBLK_ENABLE             1
// (default) seq.init failure
#define M4V_GMC_FILE_SKIP           (0<<1)
// frame skip without decoding
#define M4V_GMC_FRAME_SKIP          (1<<1)
//if only one field is fed, display it.
#define AVC_FIELD_DISPLAY           (1<<3)
//don't use sec. AXI bus.
#define SEC_AXI_BUS_DISABLE         (0<<21)
//Use SRAM for sec. AXI bus on TCC93XX
// (gets about 5% improvement of performance)
#define SEC_AXI_BUS_ENABLE_TCC93XX  (1<<21)
//Use SRAM for sec. AXI bus on TCC88XX
// (gets about 5% improvement of performance)
#define SEC_AXI_BUS_ENABLE_TCC88XX  (2<<21)

	unsigned int m_uiDecOptFlags;

	//////// H264 only param ////////

	unsigned char *m_pSpsPpsSaveBuffer; // multiple of 4
	int m_iSpsPpsSaveBufferSize;        // multiple of 1024

	//////// VPU Control ////////

	// If this is set, VPU returns userdata.
	unsigned int m_bEnableUserData;
	// video cache
	unsigned int m_bEnableVideoCache;
	// 0: chroma separate mode
	//    (CbCr data is written in separate frame memories)
	// 1: chroma interleave mode
	//    (CbCr data is interleaved in chroma memory)
	unsigned int m_bCbCrInterleaveMode;
	// enable file play mode. If this value is set to 0,
	//  streaming mode with ring buffer will be used
	int m_iFilePlayEnable;


#define RESOLUTION_1080_HD  0           // 1920x1088
#define RESOLUTION_720P_HD  1           // 1280x720
#define RESOLUTION_625_SD   2           // 720x576
	int m_iMaxResolution;           // max. resolution limitation option

	//////// Callback Func ////////

	void *(*m_Memcpy)(void *, const void *, unsigned int);
	void  (*m_Memset)(void *, int, unsigned int);
	int   (*m_Interrupt)(void);  // hw interrupt (return value is always 0)
	void *(*m_Ioremap)(unsigned int, unsigned int);
	void  (*m_Iounmap)(void *);

	unsigned int m_Reserved[10];    //! Reserved.
} dec_init_t;

typedef struct dec_input_t {
	codec_addr_t m_BitstreamDataAddr[2];// bitstream data address
	int m_iBitstreamDataSize;           // bitstream data size
	codec_addr_t m_UserDataAddr[2];// Picture Layer User-data address
	int m_iUserDataBufferSize;  // Picture Layer User-data Size

	// I-frame Search Mode
	// If this option is enabled, then decoder performs skipping
	//  frame decoding until decoder meet IDR(and/or I)-picture
	//  for H.264 or I-frame.
	// If this option is enabled, m_iSkipFrameMode option is ignored.
	//  0x000 ( Disable )
	//  0x001 ( Enable : search IDR-picture for H.264 or I-frame )
	//  0x201 ( Enable : search I(or IDR)-picture for H.264 or I-frame )
	int m_iFrameSearchEnable;

	// Skip Frame Mode
	// 0 ( Skip disable )
	// 1 ( Skip except I(IDR) picture )
	// 2 ( Skip B picture : skip if nal_ref_idc==0 in H.264 )
	// 3 ( Unconditionally Skip one picture )
	int m_iSkipFrameMode;

	// Number of skip frames (for I-frame Search Mode or Skip Frame Mode)
	// When this number is 0, m_iSkipFrameMode option is disabled.
	int m_iSkipFrameNum;
} dec_input_t;

typedef struct dec_buffer_t {
	// physical[0] and virtual[1] address of a frame buffer of the decoder.
	codec_addr_t m_FrameBufferStartAddr[2];
	// allocated frame buffer count
	int m_iFrameBufferCount;
	// start address and size of slice save buffer
	//  which the decoder can save slice RBSP : multiple of 4
	codec_addr_t m_AvcSliceSaveBufferAddr;
	// multiple of 1024
	int m_iAvcSliceSaveBufferSize;
	// JPEG Scaling Ratio
	// 0 ( Original Size )
	// 1 ( 1/2 Scaling Down )
	// 2 ( 1/4 Scaling Down )
	// 3 ( 1/8 Scaling Down )
	int m_iMJPGScaleRatio;
} dec_buffer_t;

typedef struct dec_buffer2_t {
	// physical[0] and virtual[1] address of a frame buffer of the decoder.
	codec_addr_t    m_addrFrameBuffer[2][32];
	// allocated frame buffer count
	unsigned long   m_ulFrameBufferCount;
	// start address and size of slice save buffer
	//  which the decoder can save slice RBSP : multiple of 4
	codec_addr_t    m_addrAvcSliceSaveBuffer;
	unsigned long   m_ulAvcSliceSaveBufferSize;	// multiple of 1024
	// JPEG Scaling Ratio
	//  0: Original Size
	//  1: 1/2 Scaling Down
	//  2: 1/4 Scaling Down
	//  3: 1/8 Scaling Down
	int m_iMJPGScaleRatio;
} dec_buffer2_t;

typedef struct dec_ring_buffer_setting_in_t {
	codec_addr_t m_OnePacketBufferAddr;
	unsigned int m_iOnePacketBufferSize;
} dec_ring_buffer_setting_in_t;

typedef struct dec_ring_buffer_status_out_t {
	unsigned int m_iAvailableSpaceInRingBuffer;
} dec_ring_buffer_status_out_t;

//------------------------------------------------------------------------------
// data structure to get resulting information from VPU after decoding a frame
//------------------------------------------------------------------------------

typedef struct dec_output_info_t {
	// ( 0- I picture,  1- P picture,  2- B picture )
	int m_iPicType;

	int m_iDispOutIdx;              // index of output frame buffer
	int m_iDecodedIdx;              // index of decoded frame buffer

	int m_iOutputStatus;
	int m_iDecodingStatus;

	int m_iInterlacedFrame;         // Interlaced Frame
	int m_iNumOfErrMBs;             // number of error macroblocks

	//////// VC-1 Only ////////

	// Flag for reduced resolution output in horizontal direction
	//  (in case of VC1 )
	int m_iVc1HScaleFlag;
	// Flag for reduced resolution output in vertical direction
	//  (in case of VC1 )
	int m_iVc1VScaleFlag;

	//////// MPEG-2 Only ////////

	// indicate progressive frame in picture coding extention in MPEG2
	int m_iM2vProgressiveFrame;
	int m_iM2vProgressiveSequence;
	// indicate field sequence in picture coding extention in MPEG2
	int m_iM2vFieldSequence;
	int m_iM2vAspectRatio;
	int m_iM2vFrameRate;

	//////// More Info ////////

	// indicates that the decodec picture is progressive
	//  or interlaced picture
	int m_iPictureStructure;
	// if this value is 1, top field is first decoded
	//  and then bottom field is decoded.
	int m_iTopFieldFirst;
	// repeat first field. this value is used during display process.
	int m_iRepeatFirstField;

	int m_iRvTimestamp;

	// Height of input bitstream. In some cases,
	//  this value can be different from the height of previous frames.
	int m_iHeight;
	// Width of input bitstream. In some cases,
	//  this value can be different from the height of previous frames.
	int m_iWidth;
	// Cropping information.
	pic_crop_t m_CropInfo;

	//////// User Data Buffer Address ////////

	// If contents have picture-layer user-data, return it.
	codec_addr_t m_UserDataAddress[2];

	// consumed bytes (only for file play mode, m_iFilePlayEnable=1)
	int m_iConsumedBytes;

	// counter of consecutive display index error
	int m_iInvalidDispCount;

	int m_Reserved2;

	unsigned int m_Reserved[13];
} dec_output_info_t;

typedef struct dec_output_t {
	dec_output_info_t m_DecOutInfo;

	// physical[0] and virtual[1] display  address of Y, Cb, Cr component
	unsigned char *m_pDispOut[2][3];
	// physical[0] and virtual[1] current  address of Y, Cb, Cr component
	unsigned char *m_pCurrOut[2][3];
	// physical[0] and virtual[1] previous address of Y, Cb, Cr component
	unsigned char *m_pPrevOut[2][3];
} dec_output_t;

/*!
 ***********************************************************************
 * \brief
 *      TCC_VPU_DEC     : main api function of libvpudec
 * \param
 *      [in]Op          : decoder operation
 * \param
 *      [in,out]pHandle : libvpudec's handle
 * \param
 *      [in]pParam1     : init or input parameter
 * \param
 *      [in]pParam2     : output or info parameter
 * \return
 *      If successful, TCC_VPU_DEC returns 0 or plus.
 * Otherwise, it returns a minus value.
 ***********************************************************************
 */
codec_result_t
TCC_VPU_DEC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);

//------------------------------------------------------------------------------
// encode struct and definition
//------------------------------------------------------------------------------

typedef struct enc_rc_init_t {
	//////// AVC Deblocking Filter Related /////////

	// 0: Enable, 1: Disable, 2 Disable at slice boundary
	int m_iDeblkDisable;
	// deblk_filter_offset_alpha (-6 ~ 6)
	int m_iDeblkAlpha;
	// deblk_filter_offset_beta (-6 ~ 6)
	int m_iDeblkBeta;
	// chroma_qp_offset (-12 ~ 12)
	int m_iDeblkChQpOffset;

	//////// AVC Performance Related /////////

	// 0 : Normal, 1 : 16x16 block only
	int m_iAvcFastEncoding;
	// constained_intra_pred_flag
	int m_iConstrainedIntra;

	//////// Common RC Related ////////

	int m_iPicQpY;// intra_pic_qp_y (0 ~ 51)
	// Reference decoder buffer size in bits(0 : ignore)
	int m_iVbvBufferSize;
	// 0: 128x64, 1: 64x32, 2: 32x16, 3: 16x16(Always 0 for H263)
	int m_iSearchRange;
	// 0: Enable PMV, 1: Disable PMV
	int m_iPVMDisable;
	// Intra Weight when decide INTRA/Inter
	int m_iWeightIntraCost;
	// 0: Normal, 1: Frame Mode, 2: Slice Mode, 3: MB-Num Mode
	int m_iRCIntervalMode;
	// This field is used when m_iRCIntervalMode is 3
	int m_iRCIntervalMBNum;

	//////// Error Resilience Related ////////

	int m_iIntraMBRefresh;// 0 : none, 1~ : MBNum - 1

	///////// Slice Mode Related ////////

	// 0 : frame mode, 1 : Slice mode
	int m_iSliceMode;
	// 0 : the number of bit, 1 : the number of MB
	int m_iSliceSizeMode;
	// the number of bit or MB in one Slice
	int m_iSliceSize;

	//////// Encoded Video Quality Related ////////
	// Encoded Video Quality level control (H.264 only)
	//  1(Lowest Quality) ~ 35(Highest Quality), 0 is default
	int m_iEncQualityLevel;
} enc_rc_init_t;

typedef struct enc_init_t {
	// physical[0] and virtual[1] address of a working space of the decoder.
	// This working buffer space consists of work buffer, code buffer,
	//  and parameter buffer.
	codec_addr_t m_BitWorkAddr[2];
	codec_addr_t m_RegBaseVirtualAddr;  // virtual address BIT_BASE

	//////// Encoding Info ////////

	int m_iBitstreamFormat;
	int m_iPicWidth;	// multiple of 16
	int m_iPicHeight;	              // multiple of 16
	int m_iFrameRate;
	// Target bit rate in Kbps. if 0, there will be no rate control,
	//  and pictures will be encoded with a quantization parameter
	//  equal to quantParam
	int m_iTargetKbps;
	int m_iKeyInterval;	// max 32767

	// 0: Use default setting, 1: Use parameters in m_stRcInit
	int m_iUseSpecificRcOption;
	enc_rc_init_t m_stRcInit;

	//////// VPU Control ////////

	unsigned int m_bEnableVideoCache;
	// 0: chroma separate mode
	//    (CbCr data is written in separate frame memories)
	// 1: chroma interleave mode
	//    (CbCr data is interleaved in chroma memory)
	unsigned int m_bCbCrInterleaveMode;

	codec_addr_t m_BitstreamBufferAddr; // physical addr.: multiple of 4
	codec_addr_t m_BitstreamBufferAddr_VA; // virtual addr.: multiple of 4
	int m_iBitstreamBufferSize;         // multiple of 1024

#define SEC_AXI_BUS_DISABLE     (0<<21)
#define SEC_AXI_BUS_ENABLE_TCC93XX  (1<<21)
#define SEC_AXI_BUS_ENABLE_TCC88XX  (2<<21)
 // H.264 encoder enhanced RC model enable
#define ENHANCED_RC_MODEL_ENABLE    (1<<10)

	unsigned int m_uiEncOptFlags;

	//////// Callback Func ////////

	void *(*m_Memcpy)(void *, const void *, unsigned int);
	void  (*m_Memset)(void *, int, unsigned int);
	int   (*m_Interrupt)(void); // hw interrupt (return value is always 0)

	unsigned int m_Reserved[12];
} enc_init_t;

typedef struct enc_initial_info_t {
	int m_iMinFrameBufferCount;
	int m_iMinFrameBufferSize;	// minimum frame buffer size
} enc_initial_info_t;

typedef struct enc_input_t {
	codec_addr_t m_PicYAddr;
	codec_addr_t m_PicCbAddr;
	codec_addr_t m_PicCrAddr;

	int m_iForceIPicture;
	int m_iSkipPicture;
	int m_iQuantParam;

	codec_addr_t m_BitstreamBufferAddr; // physical address
	int m_iBitstreamBufferSize;

	// RC(Rate control) parameter changing mode
	//  bit[0]: 0-disable, 1-enable
	//  bit[1]: 0-disable, 1-enable(change a bitrate)
	//  bit[2]: 0-disable, 1-enable(change a framerate)
	//  bit[3]: 0-disable, 1-enable(change a Keyframe interval)
	//  bit[4]: 0-disable, 1-enable(change a I-frame Qp)
	//  bit[5]: 0-disable, 1-enable(RC automatic skip disable)
	int m_iChangeRcParamFlag;
	int m_iChangeTargetKbps;    // Target bit rate in Kbps
	int m_iChangeFrameRate;     // Target frame rate
	int m_iChangeKeyInterval;   // Keyframe interval(max 32767)

	// Report slice information mode (0: disable, 1: enable)
	int m_iReportSliceInfoEnable;
	// Slice information buffer(last MB number and slice size in bits)
	codec_addr_t m_SliceInfoAddr[2];

	// Report MV information mode (0: disable, 1: enable)
	int m_iReportMVInfoEnable;
	// MV information buffer
	// (group of word which is formed as X and Y value of MV)
	codec_addr_t m_MVInfoAddr[2];
} enc_input_t;

typedef struct enc_buffer_t {
	codec_addr_t m_FrameBufferStartAddr[2];
} enc_buffer_t;

typedef struct enc_output_t {
	codec_addr_t m_BitstreamOut[2];
	int m_iBitstreamOutSize;
	int m_iPicType;

	// Number of encoded slices
	int m_iSliceInfoNum;
	// Size in bytes of the encoded slices information(end position)
	int m_iSliceInfoSize;
	// Address of slices information(last MB number and slice size in bits)
	codec_addr_t m_SliceInfoAddr;

	// Size in bytes of the encoded MV information
	int m_iMVInfoSize;
	// Address of slices information
	codec_addr_t m_MVInfoAddr;

	int m_Reserved;
} enc_output_t;

typedef struct enc_header_t {
	int m_iHeaderType;          // [in]
	codec_addr_t m_HeaderAddr;  // [out]
	int m_iHeaderSize;          // [out]
} enc_header_t;

/*!
 ***********************************************************************
 * \brief
 *      TCC_VPU_ENC     : main api function of libvpudec
 * \param
 *      [in]Op          : encoder operation
 * \param
 *      [in,out]pHandle : libvpudec's handle
 * \param
 *      [in]pParam1     : init or input parameter
 * \param
 *      [in]pParam2     : output or info parameter
 * \return
 *      If successful, TCC_VPU_ENC returns 0 or plus.
 *      Otherwise, it returns a minus value.
 ***********************************************************************
 */
codec_result_t
TCC_VPU_ENC(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);

#endif
#endif //_TCC93_88XX_VPU_CODEC_H_

