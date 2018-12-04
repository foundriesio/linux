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

#ifndef _MAILBOX_COMMAND_H_

#define _MAILBOX_COMMAND_H_

#define INPUT_PORT	0
#define OUTPUT_PORT	1

#define MAX_INSTANCE_NUM   4

enum {
    CA7_MBOX_CMD_GET_NEW_INSTANCE_ID = 0xA0,
    CA7_MBOX_CMD_CREATE_INSTANCE = 0xA1,
    CA7_MBOX_CMD_INIT_INSTANCE = 0xA2,
    CA7_MBOX_CMD_REMOVE_INSTANCE = 0xA3,
    CA7_MBOX_CMD_PROCESS_BUFFER = 0xA4,
    CA7_MBOX_CMD_GET_PARAM = 0xA5,
    CA7_MBOX_CMD_PREPARE = 0xA6,
    CA7_MBOX_CMD_FLUSH = 0xA7,
    CA7_MBOX_CMD_SET_PARAM = 0xA8,
    CA7_MBOX_CMD_SET_SHARED_BUFFER = 0xA9,

#if 1//__SAP_AUDIO__
    CA7_MBOX_CMD_AUDIO_DRV = 0xB0,
#endif

    // for debug
    CA7_MBOX_CMD_DBG_Notify = 0xC0,
};

enum {
    CA7_MBOX_NOTI_ERROR = 0xE8,

#if 1//__SAP_AUDIO__
    CA7_MBOX_NOTI_AUDIO_DRV = 0xF0,
#endif
};

enum {
    PARAM_INDEX_START = 0x00000000,
    PARAM_INDEX_PCM,
    PARAM_INDEX_DRC,

    // for vp9
    PARAM_INDEX_VPU_DEC_SEQ_HEADER,
    PARAM_INDEX_VPU_DEC_REG_FRAME_BUFFER,
    PARAM_INDEX_VPU_DEC_DECODE,
    PARAM_INDEX_VPU_DEC_BUF_FLAG_CLEAR,
    PARAM_INDEX_VPU_DEC_FLUSH_OUTPUT,
    PARAM_INDEX_VPU_DEC_SWRESET,
    PARAM_INDEX_VPU_CODEC_GET_VERSION,

    PARAM_INDEX_MAX = 0x7FFFFFFF
};

/* structs for command */
typedef struct {
    unsigned int mMode;
    unsigned int mInportNum;
    unsigned int mOutportNum;
    unsigned int mSharedBase;
    unsigned int mSharedSize;
    unsigned int mCodingType;
} MBOXCmdCreate;

typedef struct {
    unsigned int mConfigData;
    unsigned int mConfigSize;
} MBOXCmdInit;

typedef struct {
    unsigned int mInBuffer;
    unsigned int mInSize;
    unsigned int mOutBuffer;
    unsigned int mOutSize;
    unsigned int mInBufferSize;
} MBOXCmdProcessBuffer;

typedef struct {
    unsigned int mParamIndex;
    unsigned int mParamAddr;
    unsigned int mParamSize;
} MBOXCmdInfo;

/* structs for reply */
typedef struct {
    unsigned int mError;
} MBOXReply;

typedef struct {
    unsigned int mError;
    unsigned int mNewID;
    unsigned int mFwVersion;
    unsigned int mTotalID;
} MBOXReplyGetNewId;

typedef struct {
    unsigned int mError;
    unsigned int mUpdate;
} MBOXReplyInit;

typedef struct {
    unsigned int mError;
    unsigned int mFilledLength;
    unsigned int mNeedMoreInput;
    unsigned int mUpdate;
} MBOXReplyProcessBuffer;

typedef enum SAP_ERROR {
    SAP_ErrorNone = 0,

    // common
    SAP_ErrorResource = (int)0x80001000,
    SAP_ErrorUndefined = (int)0x80001001,
    SAP_ErrorInvalidID = (int)0x80001002,
    SAP_ErrorUnknownID = (int)0x80001003,
    SAP_ErrorUndefinedCommand = (int)0x80001004,
    SAP_ErrorUninitializedID = (int)0x80001005,
    SAP_ErrorMsgQueueIsFull = (int)0x80001006,
    SAP_ErrorInputQueueIsFull = (int)0x80001007,
    SAP_ErrorOutputQueueIsFull = (int)0x80001008,
    SAP_ErrorTimeOut = (int)0x80001009,
    SAP_ErrorNotSupportedCodec = (int)0x8000100A,
    SAP_ErrorNoConfigData = (int)0x8000100B,
    SAP_ErrorConfigLength = (int)0x8000100C,
    SAP_ErrorNullPointerToParam = (int)0x8000100D,
    SAP_ErrorVersionMismatch = (int)0x8000100E,
    SAP_ErrorUndefinedParamIndex = (int)0x8000100F,
    SAP_ErrorNotImplemented = (int)0x80001010,
    SAP_ErrorUninitializedComponent = (int)0x80001005,

    // audio
    // (int)0x80002001,

    // video
    // (int)0x80003001,

    // secure
    SAP_ErrorSecureArea = (int)0x80004001,
    SAP_ErrorNonSecureArea = (int)0x80004002,

    // debug
    SAP_ErrorDebugNotify = (int)0x8000F004,
    SAP_ErrorMax = 0x7FFFFFFF
} SAP_ERROR;

#endif // _MAILBOX_COMMAND_H_
