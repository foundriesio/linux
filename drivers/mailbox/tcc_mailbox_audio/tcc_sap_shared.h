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
