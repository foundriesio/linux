/*
 * Copyright (C) 2016 Telechips
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

typedef union union_audio_codec_t {
    //! Data structure for AAC/BSAC decoding
    struct {
        //!     AAC Object type : (2 : AAC_LC, 4: LTP, 5: SBR, 22: BSAC, ...)
        int m_iAACObjectType;
        //!     AAC Stream Header type : ( 0 : RAW-AAC, 1: ADTS, 2: ADIF)
        int m_iAACHeaderType;
        //!     upsampling flag ( 0 : disable, 1: enable)
        //!     only, if( ( m_iAACForceUpsampling == 1 ) && ( samplerate <= 24000 ) ), then out_samplerate = samplerate*2
        int m_iAACForceUpsampling;
        //!		upmix (mono to stereo) flag (0 : disable, 1: enable)
        //!     only, if( ( m_iAACForceUpmix == 1 ) && ( channel == mono ) ), then out_channel = 2;
        int m_iAACForceUpmix;
        //!		Dynamic Range Control
        //!		Dynamic Range Control, Enable Dynamic Range Control (0 : disable (default), 1: enable)
        int m_iEnableDRC;
        //!		Dynamic Range Control, Scaling factor for boosting gain value, range: 0 (not apply) ~ 127 (fully apply)
        int m_iDrcBoostFactor;
        //!		Dynamic Range Control, Scaling factor for cutting gain value, range: 0 (not apply) ~ 127 (fully apply)
        int m_iDrcCutFactor;
        //!		Dynamic Range Control, Target reference level, range: 0 (full scale) ~ 127 (31.75 dB below full-scale)
        int m_iDrcReferenceLevel;
        //!		Dynamic Range Control, Enable DVB specific heavy compression (aka RF mode), (0 : disable (default), 1: enable)
        int m_iDrcHeavyCompression;
        //!		ChannelMasking
        //!		bit:  8    7    6    5    4    3    2    1    0
        //!		ch : RR | LR | CS | LFE | RS | LS | CF | RF | LF
        int m_uiChannelMasking;
        //!		Disable HE-AAC Decoding (Decodig AAC only): 0 (enable HE-AAC Decoding (default), 1 (disable HE-AAC Decoding)
        int m_uiDisableHEAACDecoding;
        //!		RESERVED
        int reserved[32 - 11];
    } m_unAAC;

    //! Data structure for DTS decoding. These are used by application layer
    struct {
        //!		Specifies the speaker out configuration(1~8)
        int m_iDTSSetOutCh;
        //!		set	interleaving mode for output PCM  (interleaving(1)/non-onterleaving(0))
        int m_iDTSInterleavingFlag;
        //!		Instructs the decoder to use the specified value as the target sample rate for sub audio(8000,12000..)
        int m_iDTSUpSampleRate;
        //!		Instruct the decoder to use the specified percentage for dynamic range control(default(0), 0~100)
        int m_iDTSDrcPercent;
        //!		Instruct the decoder to use the LFE mixing (No LFE mixing(0), LFE mixing(1))
        int m_iDTSLFEMix;
        //!		Has secondary audio		[only DTS-M6 library]
        int m_iHasSecondary;
        //!		Disable Resync event	[only DTS-M6 library]
        int m_iNoResyncMode;
        //!		additional input setting
        //void *pExtra;
        unsigned int pExtra; // 32bit address for ca7

        //!		reserved for future needs
        int reserved[32 - 8];
    } m_unDTS;

    //! Data structure for MP2 decoding
    struct {
        //!		DAB mode select ( 0: OFF,  1: ON)
        int m_iDABMode;
        //!		reserved for future needs
        int reserved[32 - 1];
    } m_unMP2;

    //! Data structure for DDPDec decoding
    struct {
        //!		DDP stream buffer size in words
        int m_iDDPStreamBufNwords;
        //!     floating point value of gain
        //char *m_pcMixdataTextbuf;
        unsigned int m_pcMixdataTextbuf; // 32 bit address
        //!     dynamic range disable
        int m_iMixdataTextNsize;
        //!     dynamic range compression mode
        int m_iCompMode;
        //!     output LFE channel present
        int m_iOutLfe;
        //!		output channel configuration
        int m_iOutChanConfig;
        //!		PCM scale factor
        int m_iPcmScale;
        //!		stereo output mode
        int m_iStereMode;
        //!		dual mono reproduction mode
        int m_iDualMode;
        //!		dynamic range compression cut scale factor
        int m_iDynScaleHigh;
        //! 	dynamic range compression boost scale factor
        int m_iDynScaleLow;
        //!		Karaoke capable mode
        int m_iKMode;
        //!		Frame debug flags
        int m_iFrmDebugFlags;
        //!		Decode debug flags
        int m_iDecDebugFlags;
        //!		number of output channels set by user
        int m_iNumOutChannels;
        //!		mix meta data write flag
        int m_imixdataout;
        //!		[only DDP Converter library]
        //!		not detect error
        int m_iQuitOnErr;
        //!		[only DDP Converter library]
        //!		use SRC mode
        int m_iSRCMode;
        //!		[only DDP Converter library]
        //!		out stream mode, (0:default(PCM/DD both out), 1:DD stream don't out, 2:PCM data don't out)
        int m_iOutPutMode;
        //!		running mode        (0:DECODE_ONLY, 1:CONVERT_ONLY, 2:DECODE_CONVERT)
        int m_iRunningMode;
        //!		User Setting value for DRC
        int m_fUseDRCSetting;
        int m_iUserDRCMode;
        int m_iUserDRCHigh;
        int m_iUserDRCLow;
        //!		User Setting value for StereoMode
        int m_fUseStereoMode;
        //!		reserved for future needs
        int reserved[32 - 25];

    } m_unDDPDec;

    //! Data structure for MPEG Audio Layer 3/2/1 decoding
    struct {
        //!		Layer type given by parser (3 or 2 or 1)
        int m_iLayer;
        //!		DAB mode selection for layer 2 decoding ( 0: OFF,  1: ON)
        int m_iDABMode;
        //!		Enable only one specific layer decoding ( 0: enables decoding all layers, n(=1,2,3): enables decoding layer n only)
        int m_iEnableLayer;

        //!		reserved for future needs
        int reserved[32 - 3];
    } m_unMP321Dec;

} union_audio_codec_t;

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
