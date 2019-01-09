/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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
 * this program; if not	 write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __DT_BINDINGS_TCC899X_VIOC_H
#define __DT_BINDINGS_TCC899X_VIOC_H

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
#define VIOC_RDMA08			(0x0208)
#define VIOC_RDMA09			(0x0209)
#define VIOC_RDMA10			(0x020A)
#define VIOC_RDMA11			(0x020B)
#define VIOC_RDMA12			(0x020C)
#define VIOC_RDMA13			(0x020D)
//#define VIOC_RDMA14			(0x020E)
//#define VIOC_RDMA15			(0x020F)
#define VIOC_RDMA16			(0x0210)
#define VIOC_RDMA17			(0x0211)
#define VIOC_RDMA_MAX			(0x0012)

/* MC : 0x03XX */
#define VIOC_MC				(0x0300)
#define VIOC_MC0			(0x0300)
#define VIOC_MC1			(0x0301)
#define VIOC_MC_MAX			(0x0002)

/* WMIXER : 0x04XX */
#define VIOC_WMIX			(0x0400)
#define VIOC_WMIX0			(0x0400)
#define VIOC_WMIX1			(0x0401)
#define VIOC_WMIX2			(0x0402)
#define VIOC_WMIX3			(0x0403)
//#define VIOC_WMIX4			(0x0404)
#define VIOC_WMIX5			(0x0405)
#define VIOC_WMIX6			(0x0406)
#define VIOC_WMIX_MAX			(0x0007)

/* Scaler : 0x05XX */
#define VIOC_SCALER			(0x0500)
#define VIOC_SCALER0			(0x0500)
#define VIOC_SCALER1			(0x0501)
#define VIOC_SCALER2			(0x0502)
#define VIOC_SCALER3			(0x0503)
#define VIOC_SCALER4			(0x0504)
#define VIOC_SCALER5			(0x0505)
#define VIOC_SCALER_MAX			(0x0006)

/* WDMA : 0x07XX */
#define VIOC_WDMA			(0x0700)
#define VIOC_WDMA00			(0x0700)
#define VIOC_WDMA01			(0x0701)
#define VIOC_WDMA02			(0x0702)
#define VIOC_WDMA03			(0x0703)
//#define VIOC_WDMA04			(0x0704)
#define VIOC_WDMA05			(0x0705)
#define VIOC_WDMA06			(0x0706)
#define VIOC_WDMA07			(0x0707)
#define VIOC_WDMA08			(0x0708)
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
#define VIOC_VIN01			(0x0A01)
#define VIOC_VIN10			(0x0A02)
#define VIOC_VIN11			(0x0A03)
#define VIOC_VIN_MAX			(0x0004)

/* LUT : 0x0BXX */
#define VIOC_LUT			(0x0B00)
#define VIOC_LUT0			(0x0B00)
#define VIOC_LUT1			(0x0B01)
#define VIOC_LUT_MAX			(0x0002)

/* LUT TABLE : 0x0CXX */
#define VIOC_LUT_TABLE			(0x0C00)
#define VIOC_LUT_TABLE0			(0x0C00)
#define VIOC_LUT_TABLE1			(0x0C01)
#define VIOC_LUT_TABLE_MAX		(0x0002)

/* VIOC CONFIG : 0x0DXX */
#define VIOC_CONFIG			(0x0D00)

/* VIN DEMUX : 0x0EXX */
#define VIOC_VIN_DEMUX			(0x0E00)

/* TIMER : 0x0FXX */
#define VIOC_TIMER			(0x0F00)

/* VIQE : 0x10XX */
#define VIOC_VIQE			(0x1000)
#define VIOC_VIQE0			(0x1000)
#define VIOC_VIQE1			(0x1001)
#define VIOC_VIQE_MAX			(0x0002)

/* VIDEO MARK : 0x11XX */
#define VIOC_VIDEOMARK			(0x1100)
#define VIOC_VIDEOMARK0			(0x1100)
#define VIOC_VIDEOMARK1			(0x1101)
#define VIOC_VIDEOMARK_MAX		(0x0002)

/* NEXGUARD : 0x12XX */
#define VIOC_NEXGUARD			(0x1200)
#define VIOC_NEXGUARD0			(0x1200)
#define VIOC_NEXGUARD1			(0x1201)
#define VIOC_NEXGUARD_MAX		(0x0002)

/* OUTCFG : 0x1CXX */
#define VIOC_OUTCFG			(0x1300)

/* V_EDR : 0x14XX */
#define VIOC_V_EDR			(0x1400)

/* V_PANEL : 0x15XX */
#define VIOC_V_PANEL			(0x1500)

/* V_PANEL_LUT : 0x16XX */
#define VIOC_V_PANEL_LUT		(0x1600)

/* V_DV_CFG : 0x17XX */
#define VIOC_V_DV_CFG			(0x1700)

/* AFBC_DEC : 0x18XX */
#define VIOC_AFBCDEC			(0x1800)
#define VIOC_AFBCDEC0			(0x1800)
#define VIOC_AFBCDEC1			(0x1801)
#define VIOC_AFBCDEC_MAX		(0x0002)

/* PIXEL_MAP : 0x19XX */
#define VIOC_PIXELMAP			(0x1900)
#define VIOC_PIXELMAP0			(0x1900)
#define VIOC_PIXELMAP1			(0x1901)
#define VIOC_PIXELMAP_MAX		(0x0002)

/* SAR : 0x1AXX */
#define VIOC_SAR				(0x1A00)
#define VIOC_SAR0				(0x1A00)
#define VIOC_SAR1				(0x1A01)
#define VIOC_SAR_MAX			(0x0002)

/* CINTPL : 0x1BXX */
#define VIOC_CINTPL				(0x1B00)
#define VIOC_CINTPL0			(0x1B00)
#define VIOC_CINTPL1			(0x1B01)
#define VIOC_CINTPL_MAX			(0x0002)

/* 2D FILTER : 0x1CXX */
#define VIOC_F2D				(0x1C00)
#define VIOC_F2D0				(0x1C00)
#define VIOC_F2D1				(0x1C01)
#define VIOC_F2D_MAX			(0x0002)

/* VIOC_DV_IN : 0x1DXX */
#define VIOC_DV_IN				(0x1D00)

/* VIOC_DV_DISP : 0x1EXX */
#define VIOC_DV_DISP			(0x1E00)
#define VIOC_DV_DISP0			(0x1E00)
#define VIOC_DV_DISP1			(0x1E01)
#define VIOC_DV_DISP2			(0x1E02)
#define VIOC_DV_DISP3			(0x1E03)
#define VIOC_DV_DISP_MAX		(0x0004)

/* VIOC_V_DV : 0x1FXX */
#define VIOC_V_DV				(0x1F00)

/* VIOC Interrupt sources */
#define VIOC0_IRQI			(115)
#define VIOC1_IRQI			(116)
#define VIOC2_IRQI			(117)
#define VIOC3_IRQI			(118)
#define VIOC_IRQ_MAX		(4)

/* Configuration & Interrupt */
#define VIOC_EDR_WMIX0			(0)
#define VIOC_EDR_EDR			(1)

/*
#define WMIX00					(0x00)
#define WMIX03					(0x01)
#define WMIX10					(0x02)
#define WMIX13					(0x03)
#define WMIX30					(0x04)
#define WMIX40					(0x05)
#define WMIX50					(0x06)
#define WMIX60					(0x07)
#define WMIX_MAX				(0x08)
*/

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
#endif /* __DT_BINDINGS_TCC899X_VIOC_H */
