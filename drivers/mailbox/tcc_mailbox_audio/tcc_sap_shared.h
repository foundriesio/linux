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

#ifndef __TCC_SAP_SHARED_H__
#define __TCC_SAP_SHARED_H__

typedef struct tcc_sap_init_struct {    // == SapAudioSetInfoParam
    union_audio_codec_t mAudioCodecParams;
    SapAudioStreamInfoParam mStream;
    SapAudioPcmInfoParam mPcm;
    unsigned int mExtra;
    unsigned int mExtraSize;
} tcc_sap_init_struct;

typedef struct tcc_sap_shared_buffer {
    union {
        unsigned int mSharedInfo[1024 / sizeof(unsigned int)];	// 1k

        //unsigned int mDebug[512 * 1024 / sizeof(unsigned int)];		// 512k

        tcc_sap_init_struct init;
    } mem;
} tcc_sap_shared_buffer;

#endif//__TCC_SAP_SHARED_H__
