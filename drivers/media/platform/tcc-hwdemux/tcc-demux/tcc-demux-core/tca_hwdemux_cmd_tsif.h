 /*
  * Copyright (C) Telechips, Inc.
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
  */

#ifndef __TCA_HWDEMUX_TSIF_CMD_H__
#define __TCA_HWDEMUX_TSIF_CMD_H__


#define HW_DEMUX_NORMAL     0
#define HW_DEMUX_BYPASS     1

#define HW_DEMUX_TSIF_SERIAL       0
#define HW_DEMUX_TSIF_PARALLEL     1
#define HW_DEMUX_SPI               2
#define HW_DEMUX_INTERNAL          3

enum TSDMX_CMD {
	HW_DEMUX_INIT = 0xD0,
	HW_DEMUX_DEINIT,
	HW_DEMUX_ADD_FILTER,
	HW_DEMUX_DELETE_FILTER,
	HW_DEMUX_GET_STC,
	HW_DEMUX_SET_PCR_PID,
	HW_DEMUX_GET_VERSION,
	HW_DEMUX_INTERNAL_SET_INPUT,
	HW_CM_CIPHER_SET_ALGORITHM,	// = 0xC0,
	HW_CM_CIPHER_SET_KEY,
	HW_CM_CIPHER_SET_IV,
	HW_CM_CIPHER_EXECUTE,
	HW_DEMUX_CHANGE_INTERFACE,
};

//RESPONSE
enum TSDMX_RES {
	HW_DEMUX_BUFFER_UPDATED = 0xE1,
	HW_DEMUX_DEBUG_DATA = 0xF1,
};

//FILTER TYPE
enum TSDMX_FILTER_TYPE {
	HW_DEMUX_SECTION = 0,
	HW_DEMUX_TS,
	HW_DEMUX_PES,
};

struct ARG_TSDMXINIT {
	unsigned int uiDMXID;
	unsigned int uiMode;
	unsigned int uiChipID;
	unsigned int uiTSRingBufAddr;
	unsigned int uiTSRingBufSize;
	unsigned int uiSECRingBufAddr;
	unsigned int uiSECRingBufSize;
	unsigned int uiTSIFInterface;
	unsigned int uiTSIFCh;
	unsigned int uiTSIFPort;
	unsigned int uiTSIFPol;
};

struct ARG_TSDMX_ADD_FILTER {
	unsigned int uiDMXID;
	unsigned int uiFID;
	unsigned int uiTotalIndex;
	unsigned int uiCurrentIndex;
	unsigned int uiTYPE;
	unsigned int uiPID;
	unsigned int uiFSIZE;
	unsigned int uiVectorData[20];
	unsigned int uiVectorIndex;
};

struct ARG_TSDMX_DELETE_FILTER {
	unsigned int uiDMXID;
	unsigned int uiFID;
	unsigned int uiTYPE;
	unsigned int uiPID;
};

struct ARG_TSDMX_SET_PCR_PID {
	unsigned int uiDMXID;
	unsigned int uiPCRPID;
};

struct ARG_TSDMX_SET_IN_BUFFER {
	/* Set input buffer in HW_DEMUX_INTERNAL mode
	 */
	unsigned int uiDMXID;
	unsigned int uiInBufferAddr;
	unsigned int uiInBufferSize;
};

extern void TSDMX_Prepare(void);
extern void TSDMX_Release(void);
extern int TSDMXCMD_Init(struct ARG_TSDMXINIT *pARG, unsigned int uiTimeOut);
extern int TSDMXCMD_DeInit(unsigned int uiDMXID, unsigned int uiTimeOut);
extern int TSDMXCMD_ADD_Filter(struct ARG_TSDMX_ADD_FILTER *pARG,
			       unsigned int uiTimeOut);
extern int TSDMXCMD_DELETE_Filter(struct ARG_TSDMX_DELETE_FILTER *pARG,
				  unsigned int uiTimeOut);
extern long long TSDMXCMD_GET_STC(unsigned int uiDMXID, unsigned int uiTimeOut);
extern int TSDMXCMD_SET_PCR_PID(struct ARG_TSDMX_SET_PCR_PID *pARG,
				unsigned int uiTimeOut);
extern int TSDMXCMD_SET_IN_BUFFER(struct ARG_TSDMX_SET_IN_BUFFER *pARG,
				  unsigned int uiTimeOut);
extern int TSDMXCMD_GET_VERSION(unsigned int uiDMXID, unsigned int uiTimeOut);
extern int TSDMXCMD_CHANGE_INTERFACE(unsigned int uiDMXID,
				     unsigned int uiTSIFInterface,
				     unsigned int uiTimeOut);

#endif //__TCA_HWDEMUX_TSIF_CMD_H__
