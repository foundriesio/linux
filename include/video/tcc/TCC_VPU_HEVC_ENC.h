// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : TCC_VPU_HEVC_ENC.h
 * Description: TCC VPU h/w block
 *              main api
 *             (HEVC/H.265 Main Profile @ L5.0 High-tier)
 */

#ifndef _TCC_VPU_HEVC_ENC_H_
#define _TCC_VPU_HEVC_ENC_H_

#include "TCCxxxx_VPU_CODEC_COMMON.h"

#include "TCC_VPU_HEVC_ENC_DEF.h"

#ifndef STD_HEVC_ENC
    #define STD_HEVC_ENC  17
#endif

#define RETCODE_HEVCENCERR_HW_PRODUCTID		 2000

#ifndef INC_DEVICE_TREE_PMAP

typedef enum {
	// YUV420 format
	HEVC_ENC_SRC_FORMAT_YUV_NOT_PACKED = 0,
	// Packed YUYV format : Y0U0Y1V0 Y2U1Y3V1 ...
	HEVC_ENC_SRC_FORMAT_YUYV  = 1,
	// Packed YVYU format : Y0V0Y1U0 Y2V1Y3U1 ...
	HEVC_ENC_SRC_FORMAT_YVYU  = 2,
	// Packed UYVY format : U0Y0V0Y1 U1Y2V1Y3 ...
	HEVC_ENC_SRC_FORMAT_UYVY  = 3,
	// Packed VYUY format : V0Y0U0Y1 V1Y2U1Y3 ...
	HEVC_ENC_SRC_FORMAT_VYUY  = 4,
	HEVC_ENC_SRC_FORMAT_NULL  = 0x7FFFFFFF
} HevcEncSrcFormat;

/*!
 * This is a special enumeration type for explicit encoding headers
 *  such as VPS, SPS, PPS.
 */
typedef enum {
	// A flag to encode VPS nal unit explicitly
	HEVC_ENC_CODEOPT_ENC_VPS = (1 << 2),
	// A flag to encode SPS nal unit explicitly
	HEVC_ENC_CODEOPT_ENC_SPS = (1 << 3),
	// A flag to encode PPS nal unit explicitly
	HEVC_ENC_CODEOPT_ENC_PPS = (1 << 4),
} HevcEncHeaderType;

// Physical address (VPU uses only 32bits physical address)
typedef unsigned int hevc_physical_addr_t;

typedef struct hevc_enc_rc_init_t {	//TBD
	unsigned int m_Reserved[128];	//! Reserved
} hevc_enc_rc_init_t;

typedef struct hevc_enc_init_t {

	//////// Base Address ////////

	// physical[0] and virtual[1] address of a hevc encoder registers
	// (refer to datasheet)
	codec_addr_t m_RegBaseAddr[2];
	// physical[0] and virtual[1] address of a working space of
	//  the encoder. This working buffer space consists of work buffer,
	//  code buffer, and parameter buffer.
	codec_addr_t m_BitWorkAddr[2];
	// The size of the buffer in bytes pointed by VPU Work Buffer.
	// This value must be a multiple of 1024.
	int m_iBitWorkSize;
	// TBD : 0 (The size of the buffer in bytes pointed by Code Buffer.
	// This value must be a multiple of 1024).
	int m_iCodeSize;
	// TBD : 0 (physical[0] and virtual[1] address of
	// a code buffer of the encoder.)
	codec_addr_t m_CodeAddr[2];

	//////// Callback Func ////////


	// Copies the values of num bytes from the location pointed to
	//  by source directly to the memory block pointed to by destination.
	// [input] Pointer to the destination buffer
	//         where the content is to be copied
	// [input] srcPointer to the source of data to be copied
	// [input] num : Number of bytes to copy
	// [input] type : option (default 0)
	// [output] Returns a pointer to the destination area str.
	void *(*m_Memcpy)(void *, const void *, unsigned int, unsigned int);

	// Sets the first num bytes of the block of memory pointed by ptr
	//  to the specified value
	// [input] ptr : Pointer to the block of memory to fill
	// [input] value : Value to be set.
	// [input] num : Number of bytes to be set to the value.
	// [input] type : option (default 0)
	// [output] none
	void (*m_Memset)(void *, int, unsigned int, unsigned int);

	// hw interrupt (return value is always 0)
	int  (*m_Interrupt)(void);
	void (*m_Usleep)(unsigned int, unsigned int);

	void *(*m_Ioremap)(unsigned int, unsigned int);
	void  (*m_Iounmap)(void *);

	// [input]  void * vritual_base_reg_addr
	// [input]  unsigned int offset
	// [output] reading register 32-bits data
	// example:
	//  *((volatile codec_addr_t *)(vritual_base_video_reg_addr+offset))
	unsigned int (*m_reg_read)(void *pAddr, unsigned int offset);

	// [input] void *vritual_base_video_reg_addr
	// [input] unsigned int offset
	// [input] unsigned int data
	// [output] none
	// example:
	//  *((volatile codec_addr_t *)(vritual_base_video_reg_addr+offset))
	//  = (unsigned int)(data);
	void (*m_reg_write)(void *, unsigned int, unsigned int);

	void *m_CallbackReserved[4];	//!< TBD

	//////// Encoding Info ////////

	int m_iBitstreamFormat;	//!< TBD : STD_HEVC_ENC (17) only

	// The width of a picture to be encoded in pixels (multiple of 8)
	int m_iPicWidth;
	// The height of a picture to be encoded in pixels (multiple of 8)
	int m_iPicHeight;
	// frames per second
	int m_iFrameRate;

	// Target bit rate in Kbps. if 0, there will be no rate control,
	//  and pictures will be encoded with a quantization parameter
	//  equal to quantParam
	int m_iTargetKbps;

	int m_iKeyInterval;	// max 32767

	// The start physical[0] and virtual[1] address of output bitstream
	//  buffer into which encoder puts bitstream. (multiple of 1024)
	codec_addr_t m_BitstreamBufferAddr[2];

	// The size of the buffer in bytes pointed by output bitstreamBuffer.
	// This value must be a multiple of 1024.
	// The maximum size is 16383 x 1024 bytes.
	// A maximum output bitstream buffer size is the same size
	//  of uncompressed frame size.
	//  HD - 1080p @ L4.1 Main Profile : 2.97 Mbytes
	//  4K - 3840x2160 @ L5.0 Main Profile : 11.87 Mbytes
	int m_iBitstreamBufferSize;

	int m_iSrcFormat;	// Reserved : 0(YUV420)

	// It specifies a chroma interleave mode of input frame buffer.
	//  0 : chroma separate mode
	//      (CbCr data is written in separate frame memories)
	//  1 : chroma interleave mode
	//      (CbCr data is interleaved in chroma memory)
	unsigned int m_bCbCrInterleaveMode;


	unsigned int m_bCbCrOrder;	// Reserved : 0

	unsigned int m_uiEncOptFlags;	// Reserved : 0

	int m_iUseSpecificRcOption;	// Reserved : 0
	hevc_enc_rc_init_t m_stRcInit;	// Reserved : 0

	unsigned int m_uiChipsetInfo;	// TBD:(default 0)
	// example : TCC8059
	//  m_uiChipsetInfo = (1<<28)|(0<<24)|(8<<20)|(0<<16)|(5<<12)|(9<<8)|0

	unsigned int m_Reserved[31];	// Reserved

} hevc_enc_init_t;

typedef struct hevc_enc_initial_info_t {
	int m_iMinFrameBufferCount;	// [out] minimum frame buffer count
	int m_iMinFrameBufferSize;	// [out] minimum frame buffer size
	unsigned int m_Reserved[6];
} hevc_enc_initial_info_t;

typedef struct hevc_enc_input_t {
	// It indicates the base address for Y component in
	//  the physical address space. (VPU uses only 32bits physical address)
	hevc_physical_addr_t m_PicYAddr;
	// It indicates the base address for Cb component in
	//  the physical address space. (VPU uses only 32bits physical address)
	// In case of CbCr interleave mode, Cb and Cr frame data are written
	//  to memory area started from m_PicCbAddr address.
	hevc_physical_addr_t m_PicCbAddr;
	// It indicates the base address for Cr component in
	//  the physical address space. (VPU uses only 32bits physical address)
	hevc_physical_addr_t m_PicCrAddr;

	int m_iBitstreamBufferSize;
	// physical[0] and virtual[1] address
	//  (VPU uses only 32bits physical address)
	codec_addr_t m_BitstreamBufferAddr[2];


	// If this value is 0, the encoder encodes a picture as normal.
	// If this value is 1, the encoder ignores sourceFrame and generates
	//  a skipped picture.
	// In this case, the reconstructed image at decoder side is
	//  a duplication of the previous picture. The skipped picture is
	//  encoded as P-type regardless of the GOP size.
	int m_iSkipPicture;

	// If this value is 0, the picture type is determined by VPU according
	//  to the various parameters such as encoded frame number and GOP size.
	// If this value is 1, the frame is encoded as an I-picture regardless
	//  of the frame number or GOP size, and I-picture period calculation
	//  is reset to initial state.
	// A force picture type (I, P, B, IDR, CRA)  type
	//  0: disable
	//  1: IDR(Instantaneous Decoder Refresh) picture
	//  2: I picture
	//  3: CRA(Clean Random Access) picture
	// This value is ignored if m_iSkipPicture is 1.
	int m_iForceIPicture;

	// If this value is (1~51), the value is used for all quantization
	//  parameters in case of VBR (no rate control).
	//  In other cases, the QP is determined by VPU.
	int m_iQuantParam;

	// RC(Rate control) parameter changing mode
	//  bit[0]: 0-disable, 1-enable
	//  bit[1]: 0-disable, 1-enable(change a bitrate)
	//  bit[2]: 0-disable, 1-enable(change a framerate)
	//  bit[3]: 0-disable, 1-enable(change a Keyframe interval)
	int m_iChangeRcParamFlag;
	int m_iChangeTargetKbps;	//!< Target bit rate in Kbps
	int m_iChangeFrameRate;	//!< Target frame rate
	int m_iChangeKeyInterval;	//!< Keyframe interval(max 32767)

	unsigned int m_Reserved[32];
} hevc_enc_input_t;

typedef struct hevc_enc_buffer_t {
	codec_addr_t m_FrameBufferStartAddr[2]; //!< [in]
} hevc_enc_buffer_t;

typedef struct hevc_enc_output_t { //TBD
	// [out] The start physical[0] and virtual[1] address of
	//       output bitstream buffer into which encoder puts bitstream.
	codec_addr_t m_BitstreamOutAddr[2];

	// [out] The size of the buffer in bytes pointed
	//       by output bitstreamBuffer.
	int m_iBitstreamOutSize;

	// [out] This is the picture type of encoded picture.(I- or P- picture)
	int m_iPicType;

	unsigned int m_Reserved[16];
} hevc_enc_output_t;

/*!
 *
 */
// This structure is used for adding a header syntax layer
//  into the encoded bit stream.
// The parameter headerType is the input parameter to VPU, and
//  the other two parameters are returned values from VPU
//  after completing requested operation.
typedef struct hevc_enc_header_t {	//TBD
	// [in] This is a type of header that User wants to generate and
	//       have values as (HEVC_ENC_CODEOPT_ENC_VPS |
	//       HEVC_ENC_CODEOPT_ENC_SPS | HEVC_ENC_CODEOPT_ENC_PPS)
	int m_iHeaderType;

	// [in] The size of header stream buffer
	// [out] The size of the generated stream in bytes
	int m_iHeaderSize;

	// [in/out] A physical[0] and virtual[1] address pointing
	//          the generated stream location.
	codec_addr_t m_HeaderAddr[2];

	unsigned int m_Reserved[8];
} hevc_enc_header_t;

/*!
 *******************************************************************************
 * \brief
 *	TCC_VPU_HEVC_ENC	: main api function of libvpuhevcenc
 * \param
 *	[in]Op			: encoder operation
 * \param
 *	[in,out]pHandle	: libvpuhevcenc's handle
 * \param
 *	[in]pParam1		: init or input parameter
 * \param
 *	[in]pParam2		: output or info parameter
 * \return
 *	If successful, TCC_VPU_HEVC_ENC returns 0 or plus.
 *	Otherwise, it returns a minus value.
 * \example
 *     TCC_VPU_HEVC_ENC(VPU_ENC_INIT            , codec_handle_t *,
 *                      hevc_enc_init_t *       , hevc_enc_initial_info_t *);
 *     TCC_VPU_HEVC_ENC(VPU_ENC_REG_FRAME_BUFFER, codec_handle_t *,
 *                      hevc_enc_buffer_t *     , (void *)NULL);
 *     TCC_VPU_HEVC_ENC(VPU_ENC_PUT_HEADER , codec_handle_t *,
 *                      hevc_enc_header_t *, (void *)NULL);
 *     TCC_VPU_HEVC_ENC(VPU_ENC_ENCODE     , codec_handle_t *,
 *                       hevc_dec_input_t *, hevc_dec_output_t *);
 *     TCC_VPU_HEVC_ENC(VPU_ENC_CLOSE      , codec_handle_t *,
 *                      (void *)NULL       , (void *)NULL);
 *******************************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC(int Op, codec_handle_t *pHandle,
			void *pParam1, void *pParam2);

/*!
 *******************************************************************************
 * \brief
 *	TCC_VPU_HEVC_ENC_ESC	: exit api function of libvpuhevcenc
 * \param
 *	[in]Op			: forced exit operation, 1 (only)
 * \param
 *	[in,out]pHandle	: libvpuhevcenc's handle
 * \param
 *	[in]pParam1		: NULL
 * \param
 *	[in]pParam2		: NULL
 * \return
 *	TCC_VPU_HEVC_ENC_ESC returns 0.
 * \example
 *      TCC_VPU_HEVC_ENC_ESC( 1, codec_handle_t *, (void *)NULL, (void *)NULL );
 *******************************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC_ESC(int Op, codec_handle_t *pHandle,
			void *pParam1, void *pParam2);

/*!
 *******************************************************************************
 * \brief
 *	TCC_VPU_HEVC_ENC_EXT	: exit api function of libvpuhevcenc
 * \param
 *	[in]Op			: forced exit operation, 1 (only)
 * \param
 *	[in,out]pHandle	: libvpuhevcenc's handle
 * \param
 *	[in]pParam1		: NULL
 * \param
 *	[in]pParam2		: NULL
 * \return
 *	TCC_VPU_HEVC_ENC_EXT returns 0.
 * \example
 *      TCC_VPU_HEVC_ENC_EXT( 1, codec_handle_t *, (void *)NULL, (void *)NULL );
 *******************************************************************************
 */
codec_result_t
TCC_VPU_HEVC_ENC_EXT(int Op, codec_handle_t *pHandle,
			void *pParam1, void *pParam2);

#endif  //INC_DEVICE_TREE_PMAP

#endif//_TCC_VPU_HEVC_ENC_H_
