// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : TCC_VP9DEC.h
 * Description: TCC VPU h/w block
 */

#ifndef _TCC_VP9DEC_H_
#define _TCC_VP9DEC_H_

#include "TCCxxxx_VPU_CODEC_COMMON.h"

#define VP9_MAX_NUM_INSTANCE        4

#define RETCODE_VP9ERR_SEQ_HEADER_NOT_FOUND      31
#define RETCODE_VP9ERR_MIN_RESOLUTION           101
#define RETCODE_VP9ERR_MAX_RESOLUTION           102
#define RETCODE_VP9ERR_BITDEPTH                 103
#define RETCODE_VP9ERR_RGB_FORMAT               104
#define RETCODE_VP9ERR_PROFILE                  110
//#define RETCODE_VP9ERR_VERSION                  111


#define VP9_LARGE_STREAM_BUF_SIZE   0x300000
#define VP9_ENTROPY_SAVE_SIZE       0x005000

//-----------------------------------------------------------------------------
// decode struct and definition
//-----------------------------------------------------------------------------
typedef struct vp9_pic_crop_t {
	unsigned int m_iCropLeft;
	unsigned int m_iCropTop;
	unsigned int m_iCropRight;
	unsigned int m_iCropBottom;
} vp9_pic_crop_t;

typedef struct dscale_info_t {
	// Height of down scaled output picture
	int m_iDscaleHeight;
	// Width of down scaled output picture
	int m_iDscaleWidth;
	// Stride of down scaled output picture
	int m_iDscaleStride;

	// physical[0] and virtual[1] display  address of Y, Cb, Cr component
	unsigned char *m_pDscaleDispOut[2][3];
	// physical[0] and virtual[1] current  address of Y, Cb, Cr component
	unsigned char *m_pDscaleCurrOut[2][3];
	unsigned int m_Reserved[17];            // Reserved.
} dscale_info_t;

//-----------------------------------------------------------------------------
// data structure to get information necessary to
// start decoding from the decoder (this is an output parameter)
//-----------------------------------------------------------------------------
typedef struct vp9_dec_initial_info_t {
	// {(PicX+15)/16} * 16  (this width  will be used while allocating
	//  decoder frame buffers. picWidth  is a multiple of 16)
	int m_iPicWidth;
	// {(PicY+15)/16} * 16  (this height will be used while allocating
	//  decoder frame buffers. picHeight is a multiple of 16)
	int m_iPicHeight;
	// the minimum number of frame buffers that are required for decoding.
	//  application must allocate at least this number of frame buffers.
	int m_iMinFrameBufferCount;
	// minimum frame buffer size
	int m_iMinFrameBufferSize;
	// temp buffer size for segment map & loop filter
	int m_iTempBufferSize;

	// represents rectangular window information in a frame
	vp9_pic_crop_t m_iVp9PicCrop;

	//////// Additional Info ////////
	int m_iVersion;                 // version of the decoded stream
	int m_iProfile;                 // profile of the decoded stream
	int m_iSrcBitDepth;             // Source BitDepth
	int m_iOutBitDepth;             // Decoded BitDepth
	// reports the reason of 'RETCODE_CODEC_SPECOUT' or
	//  'RETCODE_INVALID_STRIDE' error
	int m_iReportErrorReason;
	unsigned int m_Reserved[21];    // Reserved.
} vp9_dec_initial_info_t;

// data structure for initializing Video unit
typedef struct vp9_dec_init_t {
	codec_addr_t m_RegBaseVirtualAddr;  // virtual register base address

	int m_iBitstreamFormat;             // bitstream format
	codec_addr_t m_BitstreamBufAddr[2]; // 128 align
	int m_iBitstreamBufSize;            // multiple of 1024

	//////// Decoding Options ////////
#define VP9_TILED_ENABLE                (1<<0)  // Tiled Output Enable
						// (No Compression Output)
#define VP9_RASTER_ENABLE               (1<<1)  // YUV420 Output Enable
#define VP9_10BITS_DISABLE              (1<<3)  // 10 bits Output Disable
	unsigned int m_uiDecOptFlags;

	codec_addr_t m_EntropySaveBuffer[2];
	int m_iEntropySaveBufferSize;

	//////// VP9 Control ////////

	// enable file play mode. If this value is set to 0,
	//  streaming mode with ring buffer will be used
	int m_iFilePlayEnable;

	// maximum resolution limitation option
	int m_iMaxResolution;

	//////// Callback Func ////////
	void *(*m_Memcpy)(void *, const void *, unsigned int);
	void (*m_Memset)(void *, int, unsigned int);
	int   (*m_Interrupt)(void);  // hw interrupt (return value is always 0)
	void *(*m_Ioremap)(unsigned int, unsigned int);
	void  (*m_Iounmap)(void *);

	unsigned int m_Reserved[16];
} vp9_dec_init_t;

typedef struct vp9_dec_input_t {
	codec_addr_t m_BitstreamDataAddr[2];    // bitstream data address
	int m_iBitstreamDataSize;               // bitstream data size

	// I-frame Search Mode
	// If this option is enabled, then decoder performs skipping
	//  frame decoding until decoder meet I-frame.
	// 0x000 ( Disable )
	// 0x001 ( Enable  )
	int m_iFrameSearchEnable;

	unsigned int m_Reserved[12];
} vp9_dec_input_t;

typedef struct vp9_dec_buffer_t {
	// physical[0] and virtual[1] address of a frame buffer of the decoder.
	codec_addr_t m_FrameBufferStartAddr[2];
	// allocated frame buffer count
	int m_iFrameBufferCount;

	// physical[0] and virtual[1] address of a temp buffer
	//  (segment & loopfilter)
	codec_addr_t m_TempBufferAddr[2];
	int m_iTempBufferSize;                  // size of temp buffer

	unsigned int m_Reserved[10];            // Reserved.
} vp9_dec_buffer_t;

typedef struct vp9_dec_buffer2_t {
	// physical[0] and virtual[1] address of a frame buffer of the decoder.
	codec_addr_t    m_addrFrameBuffer[2][16];
	// allocated frame buffer count
	unsigned long   m_ulFrameBufferCount;

	// physical[0] and virtual[1] address of a temp buffer
	// (segment & loopfilter)
	codec_addr_t m_TempBufferAddr[2];
	int m_iTempBufferSize;                  // size of temp buffer

	unsigned int m_Reserved[28];            // Reserved.
} vp9_dec_buffer2_t;

typedef struct vp9_dec_ring_buffer_setting_in_t {
	codec_addr_t m_OnePacketBufferAddr;
	unsigned int m_iOnePacketBufferSize;
} vp9_dec_ring_buffer_setting_in_t;

typedef struct vp9_dec_ring_buffer_status_out_t {
	unsigned int m_iAvailableSpaceInRingBuffer;
} vp9_dec_ring_buffer_status_out_t;

//-----------------------------------------------------------------------------
// data structure to get resulting information from VP9 after decoding a frame
//-----------------------------------------------------------------------------

typedef struct vp9_compressed_info_t {
	codec_addr_t m_CompressedY[2];
	codec_addr_t m_CompressedCb[2];

	// physical[0] and virtual[1] address of Luma Compression Table
	//  for compressed frame buffer.
	codec_addr_t m_CompressionTableLuma[2];
	codec_addr_t m_CompressionTableChroma[2];

	int m_iCompressionTableLumaSize;
	int m_iCompressionTableChromaSize;

	int m_iBitDepthLuma;            // Output Luma Bit depth.
	int m_iBitDepthChroma;          // Output Chroma Bit depth.

	// Height of input bitstream. In some cases,
	//  this value can be different from the height of previous frames.
	int m_iHeight;
	// Width of input bitstream. In some cases,
	// this value can be different from the height of previous frames.
	int m_iWidth;
	int m_iPicStride;               // Picture Stride.

	unsigned int m_Reserved[17];    //! Reserved.
} vp9_compressed_info_t;

typedef struct vp9_dec_output_info_t {
	// 0: I-picture, 1: P-picture, 2: B-picture
	int m_iPicType;

	int m_iDispOutIdx;              // index of output frame buffer
	int m_iDecodedIdx;              // index of decoded frame buffer

	int m_iOutputStatus;
	int m_iDecodingStatus;

	// Height of input bitstream. In some cases,
	//  this value can be different from the height of previous frames.
	int m_iHeight;
	// Width of input bitstream. In some cases,
	//  this value can be different from the height of previous frames.
	int m_iWidth;


	int m_iBitDepthLuma;            // Output Luma Bit depth.
	int m_iBitDepthChroma;          // Output Chroma Bit depth.
	int m_iPicStride;               // Picture Stride.

	// display compressed frame info. for map converter
	vp9_compressed_info_t m_DispCompressedInfo;
	// current compressed frame info. for map converter
	vp9_compressed_info_t m_CurrCompressedInfo;

	vp9_pic_crop_t m_CropInfo;      // Cropping information.

	dscale_info_t m_DscaleInfo;     // Down Scale information.

	unsigned int m_Reserved[18];    // Reserved.
} vp9_dec_output_info_t;

typedef struct vp9_dec_output_t {
	vp9_dec_output_info_t m_DecOutInfo;

	// physical[0] and virtual[1] display  address of Y, Cb, Cr component
	unsigned char *m_pDispOut[2][3];
	// physical[0] and virtual[1] current  address of Y, Cb, Cr component
	unsigned char *m_pCurrOut[2][3];

	unsigned int   m_Reserved[19];      // Reserved.
} vp9_dec_output_t;

/*!
 *******************************************************************************
 * \brief
 *      TCC_VP9_DEC     : main api function of libvp9dec
 * \param
 *      [in]Op          : decoder operation
 * \param
 *      [in,out]pHandle : libvp9dec's handle
 * \param
 *      [in]pParam1     : init or input parameter
 * \param
 *      [in]pParam2     : output or info parameter
 * \return
 *      If successful, TCC_VP9_DEC returns 0.
 *******************************************************************************
 */
codec_result_t TCC_VP9_DEC(int Op, codec_handle_t *pHandle,
			void *pParam1, void *pParam2);

#endif // _TCC_VP9DEC_H_

