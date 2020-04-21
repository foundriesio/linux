/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __TCC_SAP_H__
#define __TCC_SAP_H__

//#define INCLUDE_VIDEO_CODEC
#ifdef INCLUDE_VIDEO_CODEC
#include "TCC_VP9DEC.h"
#include "tcc_vp9_ioctl.h"
#endif

#define SAP_MAX_IN_PORT		4
#define SAP_MAX_OUT_PORT	2

enum {
    OK                      = 0,
    NO_ERROR                = 0,

    ERROR_UNDEFINED_COMMAND = -1,
    ERROR_UNINITIALIZED_ID  = -2,
    ERROR_CREATE_INSTANCE   = -3,
    ERROR_INIT_INSTANCE     = -4,
    ERROR_COPY_FROM_USER    = -5,
    ERROR_COPY_TO_USER      = -6,
    ERROR_INVALID_MODE      = -7,
    ERROR_MAILBOX_INIT      = -8,
    ERROR_MAILBOX_SEND      = -9,
    ERROR_MAILBOX_TIMEOUT   = -10,
    ERROR_INVALID_CMD       = -11,
    ERROR_TOO_MANY_ID       = -12,
    ERROR_OUTPUT_IS_TOO_BIG = -13,

    UNKNOWN_ERROR           = (-2147483647-1), // INT32_MIN value
};

enum {
    COMP_TYPE_AUDIO_DECODER_BASE = 0,
    COMP_TYPE_MP3_DEC = COMP_TYPE_AUDIO_DECODER_BASE,
    COMP_TYPE_MP2_DEC,
    COMP_TYPE_MP1_DEC,
    COMP_TYPE_AAC_DEC,
    COMP_TYPE_AC3_DEC,
    COMP_TYPE_DTS_DEC,
    COMP_TYPE_PASSTHR,

    COMP_TYPE_VIDEO_DECODER_BASE = 0x00001000,
    COMP_TYPE_VP9_DEC = COMP_TYPE_VIDEO_DECODER_BASE,

    COMP_TYPE_ENCODER_BASE = 0x10000000,

    COMP_TYPE_MAX = 0x7FFFFFFF
};

typedef unsigned int union_audio_codec_t[32];

enum {
    FILTER_MODE,  // codec dec/enc mode
    FILTER_MODE_SECURE_IN,
    FILTER_MODE_SECURE_IN_OUT,
    FILTER_MODE_SECURE_OUT, // for test
    SINK_MODE,	  // dec -> alsa
    SOURCE_MODE,  // alsa -> enc
};

typedef struct {
    unsigned int mSampleRate;
    unsigned int mChannels;
    unsigned int mBitRate;
    unsigned int mProfile;
    unsigned int mFrameLength;
    unsigned int mStreamFormat;
    unsigned int mChannelMode;
} SapAACInfoParam;

typedef struct {
    unsigned int mSampleRate;
    unsigned int mChannels;
    unsigned int mBitRate;
    unsigned int mFormat;
    unsigned int mChannelMode;
} SapAC3InfoParam;

// structs for Get/Set Info Parameters
typedef struct {
    unsigned int mCodingType;
    unsigned int mSampleRate;
    unsigned int mChannels;
    unsigned int mBitrate;
    unsigned int mFlag;
    unsigned int mMaxFrameSize;
} SapAudioStreamInfoParam;

typedef struct {
    unsigned int mSampleRate;
    unsigned int mChannels;
    unsigned int mBitsPerSample;
    unsigned int mPcmInterLeaved;
    unsigned int mChannelTyp;
    unsigned int mSamplePerChannel;
    unsigned int mFlag;
    unsigned int mMaxFrameSize;
} SapAudioPcmInfoParam;

typedef struct {
    unsigned int mMode;				// Dynamic range compression mode
    unsigned int mCutFactor; 		// Dynamic range compression cut scale factor
    unsigned int mBoostFactor; 		// Dynamic range compression boost scale factor
    unsigned int mReferenceLevel;	// for future use
} SapDRCInfoParam;

typedef struct {
    unsigned int mOperationMode;
    unsigned int mDirectRead;	// 0 : input -> shared mem -> ca7sp, 1: input -> ca7sp
    unsigned int mCodingType;
    unsigned int mInportNum;
    unsigned int mOutportNum;
    unsigned int mDebugMode;// 0 : off log, 1 : print ca7sp's log in the kernel, 2 : Returns a buffer containing the ca7sp's log.
    unsigned int mDebugLevel;	// 0 : off log, 1 : DEBUG, 2 : INFO, 3 : ERROR
} SapCreateParam;

typedef struct {
    union_audio_codec_t mAudioCodecParams;
    SapAudioStreamInfoParam mStream;
    SapAudioPcmInfoParam mPcm;
    unsigned long long mExtra;
    unsigned int mExtraSize;
} SapAudioSetInfoParam;

typedef struct {
    unsigned int mOperationMode;
    unsigned int mDirectRead;	// 0 : input -> shared mem -> ca7sp, 1: input -> ca7sp
    unsigned int mInportNum;
    unsigned int mOutportNum;
    unsigned int mDebugMode;// 0 : off log, 1 : print ca7sp's log in the kernel, 2 : Returns a buffer containing the ca7sp's log.
    unsigned int mDebugLevel;	// 0 : off log, 1 : DEBUG, 2 : INFO, 3 : ERROR
    SapAudioSetInfoParam mSetInfo;
} SapInitParam;

typedef struct {
    unsigned long long mData;
    unsigned int mBufferSize;
    unsigned int mDataSize;
    unsigned int mFlag;
    unsigned int mBufferNeeded;
} SapBufferInfo;

typedef struct {
    SapBufferInfo mInput[4];
    SapBufferInfo mOutput[2];
    SapBufferInfo mDebug;

    unsigned int mRet;
    unsigned int mUpdate;
} SapProcessBuffer;

#ifdef INCLUDE_VIDEO_CODEC
// video
typedef struct {
    unsigned int mOperationMode;
    unsigned int mDirectRead;	// 0 : input -> shared mem -> ca7sp, 1: input -> ca7sp
    unsigned int mInportNum;
    unsigned int mOutportNum;
    unsigned int mDebugMode;// 0 : off log, 1 : print ca7sp's log in the kernel, 2 : Returns a buffer containing the ca7sp's log.
    unsigned int mDebugLevel;	// 0 : off log, 1 : DEBUG, 2 : INFO, 3 : ERROR
} SapInitVideoParam;

typedef struct {
    SapInitVideoParam mCommon;
    VP9_INIT_t mSetInfo;
} SapVideoInitParam;

typedef struct {
    unsigned int mIndex;

} SapVideoBufferClearParam;
#endif

#if 1//__SAP_AUDIO__
typedef struct {
    unsigned int cmdid;
    char dev; /* analog or spdif or hdma */
    char cmd;
    char rev_0;
    char rev_1;
    union {
        unsigned int msg[6];

        struct {
            int ret;
        } retpar;

        struct {
            char ch; /* 2 or 8 */
            char format; /* 16 or 24bit(32bit) */
            char type; /* audio or data */
            char reserved;
            int samplerate;
            int buffer_bytes;
            int period_bytes;
            int start_threshold_bytes;
        } openpar;

        struct {
            void *pdata;
            int size; /* unit is bytes. */
        } writepar;
    } param;
} cmdmsg;

typedef struct {
    dma_addr_t paddr;
    unsigned char *vaddr;
    unsigned int size;
} audiomem;

enum{
    AUD_CMD_OPEN,
    AUD_CMD_GETAVAILABLE,
    AUD_CMD_WRITE,
    AUD_CMD_GETSTATUS,
    AUD_CMD_CLOSE,
};

enum {
    AUD_IPC_ID_TX_ANALOG = 0,
    AUD_IPC_ID_TX_SPDIF,
    AUD_IPC_ID_TX_HDMI,
    AUD_IPC_MAX
};

typedef struct{
    bool done;
    int ret;
    audiomem txdmamem;
    wait_queue_head_t wait;
}audio_data_queue_t;
#endif

// basic operation
#define IOCTL_TCC_SAP_CREATE			30	// SapCreateParam
#define IOCTL_TCC_SAP_DESTROY			31

#define IOCTL_TCC_SAP_INIT				32	// SapInitParam
#define IOCTL_TCC_SAP_DEINIT			33

#define IOCTL_TCC_SAP_PROCESS_BUFFER	34	// SapProcessBuffer
#define IOCTL_TCC_SAP_FLUSH				35

#define IOCTL_TCC_SAP_VPU_DEC_INIT              36	// SapVideoInitParam
#define IOCTL_TCC_SAP_VPU_DEC_SEQ_HEADER        37	// SapVideoSeqHeaderParam
#define IOCTL_TCC_SAP_VPU_DEC_REG_FRAME_BUFFER  38	// SapVideoRegFrameBufferParam
#define IOCTL_TCC_SAP_VPU_DEC_DECODE            39	// SapVideoDecodeParam
#define IOCTL_TCC_SAP_VPU_DEC_BUF_FLAG_CLEAR    40	// SapVideoBufferClearParam
#define IOCTL_TCC_SAP_VPU_DEC_FLUSH_OUTPUT      41	// SapVideoFlushOutputParam
#define IOCTL_TCC_SAP_VPU_DEC_SWRESET           42	// SapVideoSwResetParam
#define IOCTL_TCC_SAP_VPU_DEC_CLOSE             43	//
#define IOCTL_TCC_SAP_VPU_CODEC_GET_VERSION     44	// SapVideoGetVersionParam

// set/get info
#define IOCTL_TCC_SAP_GET_PCM_INFO		50	// SapAudioPcmInfoParam
#define IOCTL_TCC_SAP_SET_PCM_INFO		51	// SapAudioPcmInfoParam
#define IOCTL_TCC_SAP_GET_DRC_INFO		52	// SapDRCInfoParam
#define IOCTL_TCC_SAP_SET_DRC_INFO		53	// SapDRCInfoParam
#define IOCTL_TCC_SAP_GET_AAC_INFO		54	// SapAACInfoParam
#define IOCTL_TCC_SAP_SET_AAC_INFO		55	// SapAACInfoParam
#define IOCTL_TCC_SAP_GET_AC3_INFO		56	// SapAC3InfoParam
#define IOCTL_TCC_SAP_SET_AC3_INFO		57	// SapAC3InfoParam
#if 1//__SAP_AUDIO__
#define IOCTL_TCC_SAP_AUDIO_CMD			90
#endif
// debug info
#define IOCTL_TCC_MBOX_SEND				100

#endif//__TCC_SAP_H__
