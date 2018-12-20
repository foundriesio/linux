/*
 *   FileName    : TCCxxxx_VPU_CODEC_COMMON.h
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
#ifndef _TCCXXXX_VPU_CODEC_COMMON_H_
#define _TCCXXXX_VPU_CODEC_COMMON_H_

#define PA 0 // physical address
#define VA 1 // virtual  address

#define PIC_TYPE_I      0x000
#define PIC_TYPE_P      0x001
#define PIC_TYPE_B      0x002
#define PIC_TYPE_IDR    0x005
#define PIC_TYPE_B_PB   0x102 //! only for MPEG-4 Packed PB-frame


#define STD_AVC          0 //!< DEC / ENC : AVC / H.264 / MPEG-4 Part 10
#define STD_VC1          1 //!< DEC
#define STD_MPEG2        2 //!< DEC
#define STD_MPEG4        3 //!< DEC / ENC
#define STD_H263         4 //!< DEC / ENC
#define STD_DIV3         5 //!< DEC
#define STD_EXT          6 //!< DEC
#define STD_AVS          7 //!< DEC
#define STD_SH263        9 //!< DEC : Sorenson Spark / Sorenson's H.263
#define STD_MJPG        10 //!< DEC
#define STD_VP8         11 //!< DEC
#define STD_THEORA      12 //!< DEC
#define STD_MVC         14 //!< DEC
#define STD_HEVC        15 //!< DEC
#define STD_VP9         16 //!< DEC


#define RETCODE_SUCCESS                     0
#define RETCODE_FAILURE                     1
#define RETCODE_INVALID_HANDLE              2
#define RETCODE_INVALID_PARAM               3
#define RETCODE_INVALID_COMMAND             4
#define RETCODE_ROTATOR_OUTPUT_NOT_SET      5
#define RETCODE_ROTATOR_STRIDE_NOT_SET      6
#define RETCODE_FRAME_NOT_COMPLETE          7
#define RETCODE_INVALID_FRAME_BUFFER        8
#define RETCODE_INSUFFICIENT_FRAME_BUFFERS  9
#define RETCODE_INVALID_STRIDE              10
#define RETCODE_WRONG_CALL_SEQUENCE         11
#define RETCODE_CALLED_BEFORE               12
#define RETCODE_NOT_INITIALIZED             13
#define RETCODE_USERDATA_BUF_NOT_SET        14
#define RETCODE_CODEC_FINISH                15      //the end of decoding
#define RETCODE_CODEC_EXIT                  16
#define RETCODE_CODEC_SPECOUT               17
#define RETCODE_MEM_ACCESS_VIOLATION        18
#define RETCODE_INSUFFICIENT_BITSTREAM      20
#define RETCODE_INSUFFICIENT_BITSTREAM_BUF  21
#define RETCODE_INSUFFICIENT_PS_BUF         22
#define RETCODE_ACCESS_VIOLATION_HW         23
#define RETCODE_INSUFFICIENT_SECAXI_BUF     24
#define RETCODE_QUEUEING_FAILURE            25
#define RETCODE_VPU_STILL_RUNNING           26
#define RETCODE_REPORT_NOT_READY            27

#ifndef RETCODE_WRAP_AROUND
#define RETCODE_WRAP_AROUND                 (-10)
#endif


#ifndef INC_DEVICE_TREE_PMAP

#ifndef _CODEC_HANDLE_T_
#define _CODEC_HANDLE_T_
#if defined(CONFIG_ARM64)
typedef long long codec_handle_t;   //!< handle - 64bit
#else
typedef long codec_handle_t;        //!< handle - 32bit
#endif
#endif
#ifndef _CODEC_RESULT_T_
#define _CODEC_RESULT_T_
typedef int codec_result_t;         //!< return value
#endif
#ifndef _CODEC_ADDR_T_
#define _CODEC_ADDR_T_
#if defined(CONFIG_ARM64)
typedef unsigned long long codec_addr_t; //!< address - 64 bit
#else
typedef unsigned long codec_addr_t;     //!< address - 32 bit
#endif
#endif

#endif

#define COMP_Y 0
#define COMP_U 1
#define COMP_V 2


#define ALIGNED_BUFF(buf, mul) ( ( (unsigned int)buf + (mul-1) ) & ~(mul-1) )


//------------------------------------------------------------------------------
// Definition of decoding process
//------------------------------------------------------------------------------

// Output Status
#define VPU_DEC_OUTPUT_FAIL         0
#define VPU_DEC_OUTPUT_SUCCESS      1

// Decoding Status
#define VPU_DEC_SUCCESS                             1
#define VPU_DEC_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF    2
#define VPU_DEC_INFO_NOT_SUFFICIENT_SLICE_BUFF      3
#define VPU_DEC_BUF_FULL                            4
#define VPU_DEC_SUCCESS_FIELD_PICTURE               5
#define VPU_DEC_DETECT_RESOLUTION_CHANGE            6
#define VPU_DEC_INVALID_INSTANCE                    7
#define VPU_DEC_DETECT_DPB_CHANGE                   8
#define VPU_DEC_QUEUEING_FAIL                       9
#define VPU_DEC_VP9_SUPER_FRAME                     10
#define VPU_DEC_CQ_EMPTY                            11
#define VPU_DEC_REPORT_NOT_READY                    12

// Decoder Op Code
#define VPU_BASE_OP_KERNEL          0x10000
#define VPU_DEC_INIT                0x00    //!< init
#define VPU_DEC_SEQ_HEADER          0x01    //!< decode sequence header
#define VPU_DEC_GET_INFO            0x02
#define VPU_DEC_REG_FRAME_BUFFER    0x03    //!< register frame buffer
#define VPU_DEC_REG_FRAME_BUFFER2   0x04    //!< register frame buffer 2
#define VPU_DEC_REG_FRAME_BUFFER3   0x05    //!< register frame buffer 3
#define VPU_DEC_GET_OUTPUT_INFO     0x06
#define VPU_DEC_DECODE              0x10    //!< decode
#define VPU_DEC_BUF_FLAG_CLEAR      0x11    //!< display buffer flag clear
#define VPU_DEC_FLUSH_OUTPUT        0x12    //!< flush delayed output frame
#define VPU_GET_RING_BUFFER_STATUS  0x13
#define VPU_FILL_RING_BUFFER_AUTO   0x14    //!< Fill the ring buffer
#define VPU_UPDATE_WRITE_BUFFER_PTR 0x16    //!< Fill the ring buffer
#define VPU_DEC_SWRESET             0x19    //!< decoder sw reset
#define VPU_DEC_CLOSE               0x20    //!< close
#define VPU_CODEC_GET_VERSION       0x3000
/**
 * Decoder Op Code for kernel
 */
#define VPU_DEC_INIT_KERNEL					(VPU_BASE_OP_KERNEL	+ VPU_DEC_INIT)
#define VPU_DEC_SEQ_HEADER_KERNEL			(VPU_BASE_OP_KERNEL	+ VPU_DEC_SEQ_HEADER)
#define VPU_DEC_GET_INFO_KERNEL				(VPU_BASE_OP_KERNEL	+ VPU_DEC_GET_INFO)
#define VPU_DEC_REG_FRAME_BUFFER_KERNEL		(VPU_BASE_OP_KERNEL	+ VPU_DEC_REG_FRAME_BUFFER)
#define VPU_DEC_REG_FRAME_BUFFER2_KERNEL	(VPU_BASE_OP_KERNEL	+ VPU_DEC_REG_FRAME_BUFFER2)
#define VPU_DEC_REG_FRAME_BUFFER3_KERNEL	(VPU_BASE_OP_KERNEL	+ VPU_DEC_REG_FRAME_BUFFER3)
#define VPU_DEC_GET_OUTPUT_INFO_KERNEL		(VPU_BASE_OP_KERNEL	+ VPU_DEC_GET_OUTPUT_INFO)
#define VPU_DEC_DECODE_KERNEL				(VPU_BASE_OP_KERNEL	+ VPU_DEC_DECODE)
#define VPU_DEC_BUF_FLAG_CLEAR_KERNEL		(VPU_BASE_OP_KERNEL	+ VPU_DEC_BUF_FLAG_CLEAR)
#define VPU_DEC_FLUSH_OUTPUT_KERNEL			(VPU_BASE_OP_KERNEL	+ VPU_DEC_FLUSH_OUTPUT)
#define VPU_GET_RING_BUFFER_STATUS_KERNEL	(VPU_BASE_OP_KERNEL	+ VPU_GET_RING_BUFFER_STATUS)
#define VPU_FILL_RING_BUFFER_AUTO_KERNEL	(VPU_BASE_OP_KERNEL	+ VPU_FILL_RING_BUFFER_AUTO)
#define VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL	(VPU_BASE_OP_KERNEL	+ VPU_UPDATE_WRITE_BUFFER_PTR)
#define VPU_DEC_SWRESET_KERNEL				(VPU_BASE_OP_KERNEL	+ VPU_DEC_SWRESET)
#define VPU_DEC_CLOSE_KERNEL				(VPU_BASE_OP_KERNEL	+ VPU_DEC_CLOSE)

#define VPU_CODEC_GET_VERSION_KERNEL		(VPU_BASE_OP_KERNEL	+ VPU_CODEC_GET_VERSION)

#define GET_RING_BUFFER_STATUS		VPU_GET_RING_BUFFER_STATUS
#define FILL_RING_BUFFER_AUTO		VPU_FILL_RING_BUFFER_AUTO
#define GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY			0x15    //!< Get initial Info for ring buffer use


#define GET_RING_BUFFER_STATUS_KERNEL		(VPU_BASE_OP_KERNEL	+ GET_RING_BUFFER_STATUS)
#define FILL_RING_BUFFER_AUTO_KERNEL		(VPU_BASE_OP_KERNEL	+ FILL_RING_BUFFER_AUTO)
#define GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL (VPU_BASE_OP_KERNEL + GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY)

//------------------------------------------------------------------------------
// Definition of encoding process
//------------------------------------------------------------------------------

#define MPEG4_VOL_HEADER    0x00
#define MPEG4_VOS_HEADER    0x01
#define MPEG4_VIS_HEADER    0x02
#define AVC_SPS_RBSP        0x10
#define AVC_PPS_RBSP        0x11

// Encoder Op Code
#define VPU_ENC_INIT                0x00    //!< init
#define VPU_ENC_REG_FRAME_BUFFER    0x01    //!< register frame buffer
#define VPU_ENC_PUT_HEADER          0x10
#define VPU_ENC_ENCODE              0x12    //!< encode
#define VPU_ENC_CLOSE               0x20    //!< close

#define VPU_RESET_SW                0x40


#define VPU_RESET_SW_KERNEL			(VPU_BASE_OP_KERNEL	+ VPU_RESET_SW)

#endif //_TCCXXXX_VPU_CODEC_COMMON_H_
