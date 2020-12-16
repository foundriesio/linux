// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : TCC_VPU_HEVC_ENC_DEF.h
 * Description: TCC VPU h/w block
 */

#ifndef _TCC_VPU_HEVC_ENC_DEF_H_
#define _TCC_VPU_HEVC_ENC_DEF_H_

// HEVC/H.265 Main Profile @ L5.0 High-tier
#define VPU_HEVC_ENC_PROFILE  1      //!< Main Profile
#define VPU_HEVC_ENC_MAX_LEVEL 50 //!< level 5.0

// Max resolution: widthxheight = 3840x2160
#define VPU_HEVC_ENC_MAX_WIDTH   3840
#define VPU_HEVC_ENC_MAX_HEIGHT  2160

// Min resolution: widthxheight = 256x128
#define VPU_HEVC_ENC_MIM_WIDTH   256
#define VPU_HEVC_ENC_MIM_HEIGHT  128

#define VPU_HEVC_ENC_MAX_NUM_INSTANCE 4


// VPU_HEVC_ENC Memory map
// Total Size = (1) + (2) + (3)
// (1) VPU_HEVC_ENC_WORK_CODE_BUF_SIZE  (video_sw)
// (2) VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE
// (3) VPU_HEVC_ENC_STREAM_BUF_SIZE

#define VPU_HEVC_ENC_MAX_CODE_BUF_SIZE	(1024*1024)
#define VPU_HEVC_ENC_TEMPBUF_SIZE	(2*1024*1024)
#define VPU_HEVC_ENC_SEC_AXI_BUF_SIZE	(256*1024)
#define VPU_HEVC_ENC_STACK_SIZE		(8*1024)
#define VPU_HEVC_ENC_WORKBUF_SIZE	(3*1024*1024)
#define VPU_HEVC_ENC_HEADER_BUF_SIZE	(16*1024)

#define VPU_HEVC_ENC_SIZE_BIT_WORK	\
	(VPU_HEVC_ENC_MAX_CODE_BUF_SIZE + \
	 VPU_HEVC_ENC_TEMPBUF_SIZE + \
	 VPU_HEVC_ENC_SEC_AXI_BUF_SIZE + \
	 VPU_HEVC_ENC_STACK_SIZE)
#define VPU_HEVC_ENC_WORK_CODE_BUF_SIZE	\
	(VPU_HEVC_ENC_SIZE_BIT_WORK + \
	((VPU_HEVC_ENC_WORKBUF_SIZE+VPU_HEVC_ENC_HEADER_BUF_SIZE)\
	 *VPU_HEVC_ENC_MAX_NUM_INSTANCE))  // 16MB


// Calculation formula
//  = FrameCount*Align1M((Align256(width)*Align64(height)*69/32+20*1024))
// The size of frame buffers is calculated
//  by VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE macro.
// The size of one frame buffer is calculated
//  by VPU_CAL_HEVC_ENC_ONE_FRAME_BUF_SIZE macro.
// In the currnet version, the count of frame buffer, FrameCount is 2.
// example)  3840x2160:36MB  2560x1920:22MB  2560x1440:16MB
//           1920x1088:10MB  1920x 720: 8MB  1280x 720: 6MB  1024x768:4MB

#if !defined(ALIGNED_SIZE_VPU_HEVC_ENC)
#define ALIGNED_SIZE_VPU_HEVC_ENC(buffer_size, align)	\
	(((unsigned int)(buffer_size) + ((align) - 1)) & ~((align) - 1))
#endif

#define VPU_CAL_HEVC_ENC_ONE_FRAME_BUF_SIZE(width, height) \
	ALIGNED_SIZE_VPU_HEVC_ENC(\
		(ALIGNED_SIZE_VPU_HEVC_ENC((width), 256)\
		* ALIGNED_SIZE_VPU_HEVC_ENC((height), 64) * 69 / 32\
		+ (20*1024)),\
		(1024*1024))

#define VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE(width, height, frame_count)	\
	(VPU_CAL_HEVC_ENC_ONE_FRAME_BUF_SIZE(width, height) * (frame_count))

// A maximum bitstream buffer size is the same size of uncompressed frame size.
// HD - 1080p @ L4.1 Main Profile : 2.97 Mbytes  (0x002F8800)
// 4K - 3840x2160 @ L5.0 Main Profile : 11.87 Mbytes (0x00BDEC00)
#define VPU_HEVC_ENC_STREAM_BUF_SIZE	0x00BDEC00

//frame_count = 2;
#define VPU_CAL_HEVC_ENC_PROCBUFFER(width, height, frame_count)	\
	(ALIGNED_SIZE_VPU_HEVC_ENC(VPU_HEVC_ENC_STREAM_BUF_SIZE, (1024*1024))\
	+ VPU_CAL_HEVC_ENC_FRAME_BUF_SIZE(width, height, frame_count))

#define VPU_HEVC_ENC_USERDATA_BUF_SIZE	(512*1024)

#endif//_TCC_VPU_HEVC_ENC_DEF_H_
