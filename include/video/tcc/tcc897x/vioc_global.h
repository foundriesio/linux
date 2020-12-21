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
#ifndef __VIOC_GLOBAL_H__
#define	__VIOC_GLOBAL_H__


/* VIOC Overall Register Map.
 * ex)
 *  upper 8bits are component device [3:2]
 *  lower 8bits are component device number [1:0]
 *  0x0000 => RDMA[0]
 *  0x0001 => RDMA[1]
 *  0x0101 => WDMA[0]
 *  ...
 *  WARNING : DO NOT add 0x00XX type. it will be potential problem.
 */

#define get_vioc_type(x)		((x) >> 8)
#define get_vioc_index(x)		((x) & 0xFF)

/* DISP : 0x01XX */
#define VIOC_DISP			(0x0100)
#define VIOC_DISP0			(0x0100)
#define VIOC_DISP1			(0x0101)
//#define VIOC_DISP2			(0x0102)
#define VIOC_DISP_MAX			(0x0003)

/* RDMA : 0x02XX */
#define VIOC_RDMA			(0x0200)
#define VIOC_RDMA00			(0x0200)
#define VIOC_RDMA01			(0x0201)
#define VIOC_RDMA02			(0x0202)
#define VIOC_RDMA03			(0x0203)
#define VIOC_RDMA04			(0x0204)
#define VIOC_RDMA05			(0x0205)
//#define VIOC_RDMA06			(0x0206)
#define VIOC_RDMA07			(0x0207)
//#define VIOC_RDMA08			(0x0208)
//#define VIOC_RDMA09			(0x0209)
//#define VIOC_RDMA10			(0x020A)
//#define VIOC_RDMA11			(0x020B)
#define VIOC_RDMA12			(0x020C)
#define VIOC_RDMA13			(0x020D)
#define VIOC_RDMA14			(0x020E)
#define VIOC_RDMA15			(0x020F)
#define VIOC_RDMA16			(0x0210)
//#define VIOC_RDMA17			(0x0211)
#define VIOC_RDMA_MAX			(0x0012)

/* WMIXER : 0x04XX */
#define VIOC_WMIX			(0x0400)
#define VIOC_WMIX0			(0x0400)
#define VIOC_WMIX1			(0x0401)
//#define VIOC_WMIX2			(0x0402)
#define VIOC_WMIX3			(0x0403)
#define VIOC_WMIX4			(0x0404)
#define VIOC_WMIX5			(0x0405)
//#define VIOC_WMIX6			(0x0406)
#define VIOC_WMIX_MAX			(0x0007)

/* Scaler : 0x05XX */
#define VIOC_SCALER			(0x0500)
#define VIOC_SCALER0			(0x0500)
#define VIOC_SCALER1			(0x0501)
#define VIOC_SCALER2			(0x0502)
#define VIOC_SCALER3			(0x0503)
#define VIOC_SCALER_MAX			(0x0004)

/* DTRC : 0x06XX */
//#define VIOC_DTRC			(0x0600)

/* WDMA : 0x07XX */
#define VIOC_WDMA			(0x0700)
#define VIOC_WDMA00			(0x0700)
#define VIOC_WDMA01			(0x0701)
//#define VIOC_WDMA02			(0x0702)
#define VIOC_WDMA03			(0x0703)
#define VIOC_WDMA04			(0x0704)
#define VIOC_WDMA05			(0x0705)
#define VIOC_WDMA06			(0x0706)
//#define VIOC_WDMA07			(0x0707)
//#define VIOC_WDMA08			(0x0708)
#define VIOC_WDMA_MAX			(0x0009)

/* DEINTL_S : 0x08XX */
#define VIOC_DEINTLS			(0x0800)
#define VIOC_DEINTLS0			(0x0800)
#define VIOC_DEINTLS_MAX		(0x0001)

/* Async FIFO : 0x09XX */
#define VIOC_FIFO			(0x0900)
#define VIOC_FIFO0			(0x0900)
#define VIOC_FIFO_MAX			(0x0001)

/* VIDEO IN : 0x0AXX */
#define VIOC_VIN			(0x0A00)
#define VIOC_VIN00			(0x0A00)
//#define VIOC_VIN01			(0x0A01)
//#define VIOC_VIN10			(0x0A02)
//#define VIOC_VIN11			(0x0A03)
//#define VIOC_VIN20			(0x0A04)
//#define VIOC_VIN21			(0x0A05)
#define VIOC_VIN30			(0x0A06)
#define VIOC_VIN31			(0x0A07)
//#define VIOC_VIN40			(0x0A08)
//#define VIOC_VIN41			(0x0A09)
//#define VIOC_VIN50			(0x0A0A)
//#define VIOC_VIN51			(0x0A0B)
//#define VIOC_VIN60			(0x0A0C)
//#define VIOC_VIN61			(0x0A0D)
#define VIOC_VIN_MAX			(0x000E)

/* LUT DEV : 0x0BXX */
#define VIOC_LUT			(0x0B00)
#define VIOC_LUT_DEV0			(0x0B00)
#define VIOC_LUT_DEV1			(0x0B01)
//#define VIOC_LUT_DEV2			(0x0B02)
#define VIOC_LUT_COMP0			(0x0B03)
#define VIOC_LUT_COMP1			(0x0B04)
#define VIOC_LUT_COMP2			(0x0B05)
#define VIOC_LUT_COMP3			(0x0B06)
#define VIOC_LUT_MAX		(0x0007)
#define VIOC_LUT_COMP_MAX			VIOC_LUT_MAX

/* LUT TABLE : 0x0CXX */
#define VIOC_LUT_TABLE			(0x0C00)
#define VIOC_LUT_TABLE0			(0x0C00)
#define VIOC_LUT_TABLE_MAX		(0x0001)

/* VIOC CONFIG : 0x0DXX */
#define VIOC_CONFIG			(0x0D00)

/* VIN DEMUX : 0x0EXX */
#define VIOC_VIN_DEMUX			(0x0E00)

/* TIMER : 0x0FXX */
#define VIOC_TIMER			(0x0F00)

/* VIQE : 0x10XX */
#define VIOC_VIQE			(0x1000)
#define VIOC_VIQE0			(0x1000)
//#define VIOC_VIQE1			(0x1001)
#define VIOC_VIQE_MAX			(0x0002)

/* OUTCFG : 0x13XX */
#define VIOC_OUTCFG			(0x1300)

/* Display Device : PXDW FORMAT */
#define VIOC_PXDW_FMT_04_STN			(0)
#define VIOC_PXDW_FMT_08_STN			(1)
#define VIOC_PXDW_FMT_08_RGB_STRIPE		(2)
#define VIOC_PXDW_FMT_16_RGB565			(3)
#define VIOC_PXDW_FMT_15_RGB555			(4)
#define VIOC_PXDW_FMT_18_RGB666			(5)
#define VIOC_PXDW_FMT_08_UY			(6)
#define VIOC_PXDW_FMT_08_VY			(7)
#define VIOC_PXDW_FMT_16_YU			(8)
#define VIOC_PXDW_FMT_16_YV			(9)
#define VIOC_PXDW_FMT_08_RGB_DELTA0		(10)
#define VIOC_PXDW_FMT_08_RGB_DELTA1		(11)
#define VIOC_PXDW_FMT_24_RGB888			(12)
#define VIOC_PXDW_FMT_08_RGBD			(13)
#define VIOC_PXDW_FMT_16_RGB666			(14)
#define VIOC_PXDW_FMT_16_RGB888			(15)
#define VIOC_PXDW_FMT_10_RGB_STRIPE_RGB		(16)
#define VIOC_PXDW_FMT_10_RGB_DELTA_RGB_GBR	(17)
#define VIOC_PXDW_FMT_10_RGB_DELTA_GBR_RGB	(18)
#define VIOC_PXDW_FMT_10_YCBCR_CBYCRY		(19)
#define VIOC_PXDW_FMT_10_YCBCR_CRYCBY		(20)
#define VIOC_PXDW_FMT_20_YCBCR_YCBCR		(21)
#define VIOC_PXDW_FMT_20_YCBCR_YCRCB		(22)
#define VIOC_PXDW_FMT_30_RGB101010		(23)
#define VIOC_PXDW_FMT_10_RGB_DUMMY		(24)
#define VIOC_PXDW_FMT_20_RGB101010		(25)
#define VIOC_PXDW_FMT_24_YCBCR420		(26)
#define VIOC_PXDW_FMT_30_YCBCR420		(27)

/* RDMA/WDMA : RGB Swap */
#define VIOC_SWAP_RGB				(0)
#define VIOC_SWAP_RBG				(1)
#define VIOC_SWAP_GRB				(2)
#define VIOC_SWAP_GBR				(3)
#define VIOC_SWAP_BRG				(4)
#define VIOC_SWAP_BGR				(5)

/* RDMA/WDMA : Image Format */
#define VIOC_IMG_FMT_BPP1		(0)  // 1bit
#define VIOC_IMG_FMT_BPP2		(1)  // 2bits
#define VIOC_IMG_FMT_BPP4		(2)  // 4bits
#define VIOC_IMG_FMT_BPP8		(3)  // 1byte
#define VIOC_IMG_FMT_RGB332		(8)  // 1byte
#define VIOC_IMG_FMT_ARGB4444	(9)  // 2bytes
#define VIOC_IMG_FMT_RGB565		(10) // 2bytes
#define VIOC_IMG_FMT_ARGB1555	(11) // 2bytes
#define VIOC_IMG_FMT_ARGB8888	(12) // 4bytes
#define VIOC_IMG_FMT_ARGB6666_4	(13) // 4bytes
#define	VIOC_IMG_FMT_RGB888		(14) // 3bytes
#define VIOC_IMG_FMT_ARGB6666_3	(15) // 3bytes
#define	VIOC_IMG_FMT_COMP		(16) // 4bytes
#define	VIOC_IMG_FMT_DECOMP		(VIOC_IMG_FMT_COMP)
#define VIOC_IMG_FMT_444SEP		(21) // 3bytes
#define	VIOC_IMG_FMT_UYVY		(22) // 2bytes
#define	VIOC_IMG_FMT_VYUY		(23) // 2bytes
#define VIOC_IMG_FMT_YUV420SEP	(24) // 1,1byte
#define	VIOC_IMG_FMT_YUV422SEP	(25) // 1,1byte
#define	VIOC_IMG_FMT_YUYV		(26) // 2bytes
#define	VIOC_IMG_FMT_YVYU		(27) // 2bytes
#define	VIOC_IMG_FMT_YUV420IL0	(28) // 1,2byte
#define	VIOC_IMG_FMT_YUV420IL1	(29) // 1,2byte
#define	VIOC_IMG_FMT_YUV422IL0	(30) // 1,2bytes
#define	VIOC_IMG_FMT_YUV422IL1	(31) // 1,2bytes

/* Configuration & Interrupt */
#define VIOC_EDR_WMIX0				(0)
#define VIOC_EDR_EDR				(1)

#define	VIOC_SC_RDMA_00				(0x00)
#define	VIOC_SC_RDMA_01				(0x01)
#define	VIOC_SC_RDMA_02				(0x02)
#define	VIOC_SC_RDMA_03				(0x03)
#define	VIOC_SC_RDMA_04				(0x04)
#define	VIOC_SC_RDMA_05				(0x05)
#define	VIOC_SC_RDMA_06				(0x06)
#define	VIOC_SC_RDMA_07				(0x07)
#define	VIOC_SC_RDMA_08				(0x08)
#define	VIOC_SC_RDMA_09				(0x09)
#define	VIOC_SC_RDMA_10				(0x0A)
#define	VIOC_SC_RDMA_11				(0x0B)
#define	VIOC_SC_RDMA_12				(0x0C)
#define	VIOC_SC_RDMA_13				(0x0D)
#define	VIOC_SC_RDMA_14				(0x0E)
#define	VIOC_SC_RDMA_15				(0x0F)
#define	VIOC_SC_VIN_00				(0x10)
#define	VIOC_SC_RDMA_16				(0x11)
#define	VIOC_SC_VIN_01				(0x12)
#define	VIOC_SC_RDMA_17				(0x13)
#define	VIOC_SC_WDMA_00				(0x14)
#define	VIOC_SC_WDMA_01				(0x15)
#define	VIOC_SC_WDMA_02				(0x16)
#define	VIOC_SC_WDMA_03				(0x17)
#define	VIOC_SC_WDMA_04				(0x18)
#define	VIOC_SC_WDMA_05				(0x19)
#define	VIOC_SC_WDMA_06				(0x1A)
#define	VIOC_SC_WDMA_07				(0x1B)
#define	VIOC_SC_WDMA_08				(0x1C)

#define VIOC_VIQE_RDMA_00			(0x00)
#define VIOC_VIQE_RDMA_01			(0x01)
#define VIOC_VIQE_RDMA_02			(0x02)
#define VIOC_VIQE_RDMA_03			(0x03)
#define VIOC_VIQE_RDMA_06			(0x04)
#define VIOC_VIQE_RDMA_07			(0x05)
#define VIOC_VIQE_RDMA_10			(0x06)
#define VIOC_VIQE_RDMA_11			(0x07)
#define VIOC_VIQE_RDMA_12			(0x08)
#define VIOC_VIQE_RDMA_14			(0x09)
#define VIOC_VIQE_VIN_00			(0x0A)
#define VIOC_VIQE_RDMA_16			(0x0B)
#define VIOC_VIQE_VIN_01			(0x0C)
#define VIOC_VIQE_RDMA_17			(0x0D)

#define VIOC_DEINTLS_RDMA_00			(0x00)
#define VIOC_DEINTLS_RDMA_01			(0x01)
#define VIOC_DEINTLS_RDMA_02			(0x02)
#define VIOC_DEINTLS_RDMA_03			(0x03)
#define VIOC_DEINTLS_RDMA_06			(0x04)
#define VIOC_DEINTLS_RDMA_07			(0x05)
#define VIOC_DEINTLS_RDMA_10			(0x06)
#define VIOC_DEINTLS_RDMA_11			(0x07)
#define VIOC_DEINTLS_RDMA_12			(0x08)
#define VIOC_DEINTLS_RDMA_14			(0x09)
#define VIOC_DEINTLS_VIN_00			(0x0A)
#define VIOC_DEINTLS_RDMA_16			(0x0B)
#define VIOC_DEINTLS_VIN_01			(0x0C)
#define VIOC_DEINTLS_RDMA_17			(0x0D)

#define	VIOC_VM_RDMA_00				(0x00)
#define	VIOC_VM_RDMA_01				(0x01)
#define	VIOC_VM_RDMA_02				(0x02)
#define	VIOC_VM_RDMA_03				(0x03)
#define	VIOC_VM_RDMA_04				(0x04)
#define	VIOC_VM_RDMA_05				(0x05)
#define	VIOC_VM_RDMA_06				(0x06)
#define	VIOC_VM_RDMA_07				(0x07)
#define	VIOC_VM_RDMA_08				(0x08)
#define	VIOC_VM_RDMA_09				(0x09)
#define	VIOC_VM_RDMA_10				(0x0A)
#define	VIOC_VM_RDMA_11				(0x0B)
#define	VIOC_VM_RDMA_12				(0x0C)
#define	VIOC_VM_RDMA_13				(0x0D)
#define	VIOC_VM_RDMA_14				(0x0E)
#define	VIOC_VM_RDMA_15				(0x0F)
#define	VIOC_VM_VIN_00				(0x10)
#define	VIOC_VM_RDMA_16				(0x11)
#define	VIOC_VM_VIN_01				(0x12)
#define	VIOC_VM_RDMA_17				(0x13)
#define	VIOC_VM_WDMA_00				(0x14)
#define	VIOC_VM_WDMA_01				(0x15)
#define	VIOC_VM_WDMA_02				(0x16)
#define	VIOC_VM_WDMA_03				(0x17)
#define	VIOC_VM_WDMA_04				(0x18)
#define	VIOC_VM_WDMA_05				(0x19)
#define	VIOC_VM_WDMA_06				(0x1A)
#define	VIOC_VM_WDMA_07				(0x1B)
#define	VIOC_VM_WDMA_08				(0x1C)

#define	VIOC_NG_RDMA_00				(0x00)
#define	VIOC_NG_RDMA_01				(0x01)
#define	VIOC_NG_RDMA_02				(0x02)
#define	VIOC_NG_RDMA_03				(0x03)
#define	VIOC_NG_RDMA_04				(0x04)
#define	VIOC_NG_RDMA_05				(0x05)
#define	VIOC_NG_RDMA_06				(0x06)
#define	VIOC_NG_RDMA_07				(0x07)
#define	VIOC_NG_RDMA_08				(0x08)
#define	VIOC_NG_RDMA_09				(0x09)
#define	VIOC_NG_RDMA_10				(0x0A)
#define	VIOC_NG_RDMA_11				(0x0B)
#define	VIOC_NG_RDMA_12				(0x0C)
#define	VIOC_NG_RDMA_13				(0x0D)
#define	VIOC_NG_RDMA_14				(0x0E)
#define	VIOC_NG_RDMA_15				(0x0F)
#define	VIOC_NG_VIN_00				(0x10)
#define	VIOC_NG_RDMA_16				(0x11)
#define	VIOC_NG_VIN_01				(0x12)
#define	VIOC_NG_RDMA_17				(0x13)
#define	VIOC_NG_WDMA_00				(0x14)
#define	VIOC_NG_WDMA_01				(0x15)
#define	VIOC_NG_WDMA_02				(0x16)
#define	VIOC_NG_WDMA_03				(0x17)
#define	VIOC_NG_WDMA_04				(0x18)
#define	VIOC_NG_WDMA_05				(0x19)
#define	VIOC_NG_WDMA_06				(0x1A)
#define	VIOC_NG_WDMA_07				(0x1B)
#define	VIOC_NG_WDMA_08				(0x1C)

#define VIOC_RDMA_RDMA02			(0x02)
#define VIOC_RDMA_RDMA03			(0x03)
#define VIOC_RDMA_RDMA06			(0x06)
#define VIOC_RDMA_RDMA07			(0x07)
#define VIOC_RDMA_RDMA11			(0x0B)
#define VIOC_RDMA_RDMA12			(0x0C)
#define VIOC_RDMA_RDMA13			(0x0D)
#define VIOC_RDMA_RDMA14			(0x0E)
#define VIOC_RDMA_RDMA16			(0x10)
#define VIOC_RDMA_RDMA17			(0x11)

#define VIOC_MC_RDMA02				(0x02)
#define VIOC_MC_RDMA03				(0x03)
#define VIOC_MC_RDMA06				(0x06)
#define VIOC_MC_RDMA07				(0x07)
#define VIOC_MC_RDMA11				(0x0B)
#define VIOC_MC_RDMA12				(0x0C)
#define VIOC_MC_RDMA13				(0x0D)
#define VIOC_MC_RDMA14				(0x0E)
#define VIOC_MC_RDMA16				(0x10)
#define VIOC_MC_RDMA17				(0x11)

#define VIOC_DTRC_RDMA02			(0x02)
#define VIOC_DTRC_RDMA03			(0x03)
#define VIOC_DTRC_RDMA06			(0x06)
#define VIOC_DTRC_RDMA07			(0x07)
#define VIOC_DTRC_RDMA11			(0x0B)
#define VIOC_DTRC_RDMA12			(0x0C)
#define VIOC_DTRC_RDMA13			(0x0D)
#define VIOC_DTRC_RDMA14			(0x0E)
#define VIOC_DTRC_RDMA16			(0x10)
#define VIOC_DTRC_RDMA17			(0x11)

#define	VIOC_AD_RDMA_00				(0x00)
#define	VIOC_AD_RDMA_01				(0x01)
#define	VIOC_AD_RDMA_02				(0x02)
#define	VIOC_AD_RDMA_03				(0x03)
#define	VIOC_AD_RDMA_04				(0x04)
#define	VIOC_AD_RDMA_05				(0x05)
#define	VIOC_AD_RDMA_06				(0x06)
#define	VIOC_AD_RDMA_07				(0x07)
#define	VIOC_AD_RDMA_08				(0x08)
#define	VIOC_AD_RDMA_09				(0x09)
#define	VIOC_AD_RDMA_10				(0x0A)
#define	VIOC_AD_RDMA_11				(0x0B)
#define	VIOC_AD_RDMA_12				(0x0C)
#define	VIOC_AD_RDMA_13				(0x0D)
#define	VIOC_AD_RDMA_14				(0x0E)
#define	VIOC_AD_RDMA_15				(0x0F)
#define	VIOC_AD_RDMA_16				(0x11)
#define	VIOC_AD_RDMA_17				(0x13)

////////////////////////////////////////////////////////////////////////////////
//
//	CPUIF FORMAT
//
#define	CPUIF_OFMT_08_TYPE0   0 // LCDC : 8-8-8, SI(08bits) 8-8-8
#define	CPUIF_OFMT_24_TYPE0   4 // LCDC : 8-8-8, SI(16bits) 9-9
#define	CPUIF_OFMT_24_TYPE1   5
#define	CPUIF_OFMT_24_TYPE2   6
#define	CPUIF_OFMT_24_TYPE3   7
#define	CPUIF_OFMT_18_TYPE0   8
#define	CPUIF_OFMT_18_TYPE1   9
#define	CPUIF_OFMT_18_TYPE2   10
#define	CPUIF_OFMT_18_TYPE3   11
#define	CPUIF_OFMT_16_TYPE0   12
#define	CPUIF_OFMT_16_TYPE1   13
#define	CPUIF_OFMT_16_TYPE2   14
//
//	CPUIF FORMAT
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//	WDMA SYNC SELECTION
//
#define	VIOC_WDMA_SYNC_ABSOLUTE		(0<<0) // SYNCMD_ADDR
#define	VIOC_WDMA_SYNC_RELATIVE		(1<<0) // SYNCMD_ADDR
#define	VIOC_WDMA_SYNC_START_EDGE	(0<<0) // SYNCMD_SENS
#define	VIOC_WDMA_SYNC_START_LEVEL	(1<<0) // SYNCMD_SENS

#define	VIOC_WDMA_SYNC_RDMA00		(0)
#define	VIOC_WDMA_SYNC_RDMA01		(1)
#define	VIOC_WDMA_SYNC_RDMA02		(2)
#define	VIOC_WDMA_SYNC_RDMA03		(3)
#define	VIOC_WDMA_SYNC_RDMA04		(4)
#define	VIOC_WDMA_SYNC_RDMA05		(5)
#define	VIOC_WDMA_SYNC_RDMA06		(6)
#define	VIOC_WDMA_SYNC_RDMA07		(7)
#define	VIOC_WDMA_SYNC_RDMA08		(8)
#define	VIOC_WDMA_SYNC_RDMA09		(9)
#define	VIOC_WDMA_SYNC_RDMA10		(10)
#define	VIOC_WDMA_SYNC_RDMA11		(11)
#define	VIOC_WDMA_SYNC_RDMA12		(12)
#define	VIOC_WDMA_SYNC_RDMA13		(13)
#define	VIOC_WDMA_SYNC_RDMA14		(14)
#define	VIOC_WDMA_SYNC_RDMA15		(15)
#define	VIOC_WDMA_SYNC_RDMA16		(16)
#define	VIOC_WDMA_SYNC_RDMA17		(17)
//
//	WDMA SYNC SELECTION
//
////////////////////////////////////////////////////////////////////////////////

/* VIOC DRIVER STATUS TYPE */
#define	VIOC_DEVICE_INVALID			(-2)
#define	VIOC_DEVICE_BUSY			(-1)
#define	VIOC_DEVICE_CONNECTED		(0)

/* VIOC DRIVER ERROR TYPE */
#define VIOC_DRIVER_ERR_INVALID		(-3)
#define VIOC_DRIVER_ERR_BUSY		(-2)
#define VIOC_DRIVER_ERR				(-1)
#define VIOC_DRIVER_NOERR			(0)

/* VIOC PATH STATUS TYPE */
#define VIOC_PATH_DISCONNECTED		(0)
#define VIOC_PATH_CONNECTING		(1)
#define VIOC_PATH_CONNECTED			(2)
#define VIOC_PATH_DISCONNECTING		(3)

#define FBX_MODE(x)			(x)
#define FBX_SINGLE			FBX_MODE(0x0)
#define FBX_DOUBLE			FBX_MODE(0x1)
#define FBX_TRIPLE			FBX_MODE(0x2)

#define FBX_UPDATE(x)		(x)
#define FBX_RDMA_UPDATE			FBX_UPDATE(0x0)
#define FBX_M2M_RDMA_UPDATE		FBX_UPDATE(0x1)
#define FBX_ATTACH_UPDATE		FBX_UPDATE(0x2)
#define FBX_OVERLAY_UPDATE		FBX_UPDATE(0x3)
#define FBX_NOWAIT_UPDATE		FBX_UPDATE(0x4)

#define FBX_DEVICE(x)		(x)
#define FBX_DEVICE_NONE			FBX_DEVICE(0x0)
#define FBX_DEVICE_LVDS			FBX_DEVICE(0x1)
#define FBX_DEVICE_HDMI			FBX_DEVICE(0x2)
#define FBX_DEVICE_COMPOSITE		FBX_DEVICE(0x3)
#define FBX_DEVICE_COMPONENT		FBX_DEVICE(0x4)

#endif
