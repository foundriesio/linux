/*
 * linux/drivers/spi/tsdemux/TSDEMUX_sys.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/****************************************************************************
 *
 *  Revision History
 *
 ****************************************************************************/

#ifndef _TSDEMUX_SYS_H_
#define _TSDEMUX_SYS_H_

enum  MpegTsAdaptationCtrl {
	TS_ADAPTATION_RESERVED = 0x0,
	TS_ADAPTATION_PAYLOAD_ONLY,
	TS_ADAPTATION_ONLY,
	TS_ADAPTATION_AND_PAYLOAD
};

struct MpegTsAdaptionFlg {
	unsigned char ext_flag:1;
	unsigned char private_data_flag:1;
	unsigned char splicing_point_flag:1;
	unsigned char OPCR:1;
	unsigned char PCR:1;
	unsigned char priority_indicator:1;
	unsigned char random_access_indicator:1;
	unsigned char discontinuity_indicator:1;
};

struct MpegTsAdaptation {
	int length;
	long long PCR;
	struct MpegTsAdaptionFlg flag;
};

struct MpegTsHeader {
	int error_indicator;
	int pusi;
	int priority;
	int pid;
	int scrambling_control;
	enum MpegTsAdaptationCtrl adaptation_control;
	int cc;
	int payload_size;
	char *payload;
	struct MpegTsAdaptation adap;
};

struct MpegPesFlag {
	unsigned short PES_extension_flag:1;
	unsigned short PES_CRC_flag:1;
	unsigned short additional_copy_info_flag:1;
	unsigned short DSM_trick_mode_flag:1;
	unsigned short ES_rate_flag:1;
	unsigned short ESCR_flag:1;
	unsigned short PTS_DTS_flags:2;
	unsigned short original_or_copy:1;
	unsigned short copyright:1;
	unsigned short data_alignment_indicator:1;
	unsigned short PES_priority:1;
	unsigned short PES_scrambling_control:2;
	unsigned short dummy_pes_data:2;
};

struct MpegPesHeader {
	int stream_id;
	int length;
	int header_length;
	int payload_size;
	char *payload;
	struct MpegPesFlag flag;
	long long pts;
	long long dts;
};

enum MPEGPES_STREAM_ID {
	STREAM_ID_PROGRAM_STREAM_MAP = 0xBC,
	STREAM_ID_PADDING_STREAM = 0xBE,
	STREAM_ID_PRIVATE_STREAM_2 = 0xBF,
	STREAM_ID_ECM_STREAM = 0xF0,
	STREAM_ID_EMM_STREAM = 0xF1,
	STREAM_ID_DSMCC = 0xF2,
	STREAM_ID_ITU_T_REC_H222_1_TYPE_E = 0xF8,
	STREAM_ID_PROGRAM_STREAM_DIRECTORY = 0xFF,
	STREAM_ID_MAX
};

#define MPEGSYS_TS_PACKETSIZE 188
#define MPEGSYS_TS_PACKETSYNC 0x47

int TSDEMUX_UpdatePCR(unsigned int uiPCR, int index);
int TSDEMUX_MakeSTC(unsigned char *pucTS, unsigned int uiTSSize,
		    unsigned int uiPCR, int index);
int TSDEMUX_Open(int index);
void TSDEMUX_Close(void);
unsigned int TSDEMUX_GetSTC(int index);
#endif /* _TSDEMUX_SYS_H_ */
