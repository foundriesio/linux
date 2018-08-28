/*
 * Copyright (c) 2011 Telechips, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef __REG_PHYSICAL_H__
#define __REG_PHYSICAL_H__

#include "tcc_types.h"

/*******************************************************************************
*
*    TCC897x DataSheet PART 2 SMU & PMU
*
*******************************************************************************/
#define HwGPIO_BASE                             (0x14200000)
#define HwGPIOA_BASE                            (0x14200000)
#define HwGPIOB_BASE                            (0x14200040)
#define HwGPIOC_BASE                            (0x14200080)
#define HwGPIOD_BASE                            (0x142000C0)
#define HwGPIOE_BASE                            (0x14200100)
#define HwGPIOF_BASE                            (0x14200140)
#define HwGPIOG_BASE                            (0x14200180)
#define HwGPIOHDMI_BASE                         (0x142001C0)
#define HwGPIOSD1_BASE                          (0x14200200)
#define HwGPIOSD0_BASE                          (0x14200240)

/*******************************************************************************
*
*    TCC897x DataSheet PART 7 DISPLAY BUS
*
*******************************************************************************/
#define HwVIOC_BASE                             (0x12000000)
#define BASE_ADDR_VIOC                          HwVIOC_BASE

/* DISP */
#define HwVIOC_DISP0            ( (BASE_ADDR_VIOC  + 0x0000 + 0x0000)) // 64 words
#define HwVIOC_DISP1            ( (BASE_ADDR_VIOC  + 0x0000 + 0x0100)) // 64 words
#define HwVIOC_DISP2            ( (BASE_ADDR_VIOC  + 0x0000 + 0x0200)) // 64 words

/* RDMA */
#define HwVIOC_RDMA_GAP     	(0x100)
#define HwVIOC_RDMA00           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0400)) // 64 words
#define HwVIOC_RDMA01           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0500)) // 64 words
#define HwVIOC_RDMA02           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0600)) // 64 words
#define HwVIOC_RDMA03           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0700)) // 64 words
#define HwVIOC_RDMA04           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0800)) // 64 words
#define HwVIOC_RDMA05           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0900)) // 64 words
#define HwVIOC_RDMA06           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0A00)) // 64 words
#define HwVIOC_RDMA07           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0B00)) // 64 words
#define HwVIOC_RDMA08           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0C00)) // 64 words
#define HwVIOC_RDMA09           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0D00)) // 64 words
#define HwVIOC_RDMA10           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0E00)) // 64 words
#define HwVIOC_RDMA11           ( (BASE_ADDR_VIOC  + 0x0000 + 0x0F00)) // 64 words
#define HwVIOC_RDMA12           ( (BASE_ADDR_VIOC  + 0x0000 + 0x1000)) // 64 words
#define HwVIOC_RDMA13           ( (BASE_ADDR_VIOC  + 0x0000 + 0x1100)) // 64 words
#define HwVIOC_RDMA14           ( (BASE_ADDR_VIOC  + 0x0000 + 0x1200)) // 64 words
#define HwVIOC_RDMA15           ( (BASE_ADDR_VIOC  + 0x0000 + 0x1300)) // 64 words
#define HwVIOC_RDMA16           ( (BASE_ADDR_VIOC  + 0x0000 + 0x1400)) // 64 words
#define HwVIOC_RDMA17           ( (BASE_ADDR_VIOC  + 0x0000 + 0x1500)) // 64 words

#define HwVIOC_MC0              ( (BASE_ADDR_VIOC  + 0x0000 + 0x1600)) // 64 words
#define HwVIOC_MC1              ( (BASE_ADDR_VIOC  + 0x0000 + 0x1700)) // 64 words


/* WMIX */
#define HwVIOC_WMIX0            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1800)) // 64 words
#define HwVIOC_WMIX1            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1900)) // 64 words
#define HwVIOC_WMIX2            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1A00)) // 64 words
#define HwVIOC_WMIX3            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1B00)) // 64 words
#define HwVIOC_WMIX4            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1C00)) // 64 words
#define HwVIOC_WMIX5            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1D00)) // 64 words
#define HwVIOC_WMIX6            ( (BASE_ADDR_VIOC  + 0x0000 + 0x1E00)) // 64 words

#define HwVIOC_WMIX0_ALPHA      ( (BASE_ADDR_VIOC  + 0x0000 + 0x1860)) // 64 words
#define HwVIOC_WMIX1_ALPHA      ( (BASE_ADDR_VIOC  + 0x0000 + 0x1960)) // 64 words
#define HwVIOC_WMIX2_ALPHA      ( (BASE_ADDR_VIOC  + 0x0000 + 0x1A60)) // 64 words

/* SCALER */
#define HwVIOC_SC0              ( (BASE_ADDR_VIOC  + 0x0000 + 0x2000)) // 64 words
#define HwVIOC_SC1              ( (BASE_ADDR_VIOC  + 0x0000 + 0x2100)) // 64 words
#define HwVIOC_SC2              ( (BASE_ADDR_VIOC  + 0x0000 + 0x2200)) // 64 words
#define HwVIOC_SC3              ( (BASE_ADDR_VIOC  + 0x0000 + 0x2300)) // 64 words
#define HwVIOC_SC4              ( (BASE_ADDR_VIOC  + 0x0000 + 0x2400)) // 64 words
#define HwVIOC_SC5              ( (BASE_ADDR_VIOC  + 0x0000 + 0x2500)) // 64 words

/* WDMA */
#define HwVIOC_WDMA00           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2800)) // 64 words
#define HwVIOC_WDMA01           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2900)) // 64 words
#define HwVIOC_WDMA02           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2A00)) // 64 words
#define HwVIOC_WDMA03           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2B00)) // 64 words
#define HwVIOC_WDMA04           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2C00)) // 64 words
#define HwVIOC_WDMA05           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2D00)) // 64 words
#define HwVIOC_WDMA06           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2E00)) // 64 words
#define HwVIOC_WDMA07           ( (BASE_ADDR_VIOC  + 0x0000 + 0x2F00)) // 64 words
#define HwVIOC_WDMA08           ( (BASE_ADDR_VIOC  + 0x0000 + 0x3000)) // 64 words

#define HwVIOC_DISP0_MIX        ( (BASE_ADDR_VIOC  + 0x0000 + 0x3200)) // NOT USED
#define HwVIOC_DISP0_MD         ( (BASE_ADDR_VIOC  + 0x0000 + 0x3300)) // NOT USED
#define HwVIOC_DISP1_MIX        ( (BASE_ADDR_VIOC  + 0x0000 + 0x3400)) // NOT USED
#define HwVIOC_DISP1_MD         ( (BASE_ADDR_VIOC  + 0x0000 + 0x3500)) // NOT USED
#define HwVIOC_DISP2_MIX        ( (BASE_ADDR_VIOC  + 0x0000 + 0x3600)) // NOT USED
#define HwVIOC_DISP2_MD         ( (BASE_ADDR_VIOC  + 0x0000 + 0x3700)) // NOT USED



/* DEINTLS */
#define HwVIOC_DEINTLS          ( (BASE_ADDR_VIOC  + 0x0000 + 0x3800)) // 64 words
#define HwVIOC_FDLY0            ( (BASE_ADDR_VIOC  + 0x0000 + 0x3900)) // 64 words
#define HwVIOC_FDLY1            ( (BASE_ADDR_VIOC  + 0x0000 + 0x3A00)) // 64 words
#define HwVIOC_FIFO             ( (BASE_ADDR_VIOC  + 0x0000 + 0x3B00)) // 64 words
#define HwVIOC_DEBLOCK          ( (BASE_ADDR_VIOC  + 0x0000 + 0x3C00)) // 64 words

//******************************************************************************
// [15:14] == 2'b01
//******************************************************************************
/* VIN */
#define HwVIOC_VINDEMUX         ( (BASE_ADDR_VIOC  + 0xA800         ))
#define HwVIOC_VIN00            ( (BASE_ADDR_VIOC  + 0x4000         ))
#define HwVIOC_VIN01            ( (BASE_ADDR_VIOC  + 0x4400         ))
#define HwVIOC_VIN10            ( (BASE_ADDR_VIOC  + 0x5000         ))
#define HwVIOC_VIN11            ( (BASE_ADDR_VIOC  + 0x5400         ))
#define HwVIOC_VIN20            ( (BASE_ADDR_VIOC  + 0x6000         ))
#define HwVIOC_VIN21            ( (BASE_ADDR_VIOC  + 0x6400         ))
#define HwVIOC_VIN30            ( (BASE_ADDR_VIOC  + 0x7000         ))
#define HwVIOC_VIN31            ( (BASE_ADDR_VIOC  + 0x7400         ))

//******************************************************************************
// [15:14] == 2'b10
//******************************************************************************
#define HwVIOC_FILT2D           ( (BASE_ADDR_VIOC  + 0x8000        ))
#define HwVIOC_LUT              ( (BASE_ADDR_VIOC  + 0x9000        ))
#define HwVIOC_LUT_TAB          ( (BASE_ADDR_VIOC  + 0x9400        ))
#define HwVIOC_CONFIG           ( (BASE_ADDR_VIOC  + 0xA000        ))
#define HwVIOC_IREQ             ( (BASE_ADDR_VIOC  + 0xA000 + 0x000)) //  16 word
#define HwVIOC_PCONFIG          ( (BASE_ADDR_VIOC  + 0xA000 + 0x040)) //  32 word
#define HwVIOC_POWER            ( (BASE_ADDR_VIOC  + 0xA000 + 0x0C0)) //  16 word
#define HwVIOC_FCODEC           ( (BASE_ADDR_VIOC  + 0xB000        ))

//******************************************************************************
// [15:14] == 2'b11
//******************************************************************************
/* VIOC TIMER */
#define HwVIOC_TIMER_BASE       ( (BASE_ADDR_VIOC  + 0xC000        ))
#define HwVIOC_TIMER            ( (BASE_ADDR_VIOC  + 0xC000        ))

/* VIQE */
#define HwVIOC_VIQE0_BASE       ( (BASE_ADDR_VIOC  + 0xD000        ))
#define HwVIOC_VIQE1_BASE       ( (BASE_ADDR_VIOC  + 0xE000        ))
#define  HwVIOC_VIQE0                           (HwVIOC_VIQE0_BASE)
#define  HwVIOC_VIQE1                           (HwVIOC_VIQE1_BASE)
#define HwVIOC_MMU_BASE         ( (BASE_ADDR_VIOC  + 0xF000        ))
#define HwVIOC_MMU              ( (BASE_ADDR_VIOC  + 0xF000        ))


/* CPUIF */
#define BASE_ADDR_CPUIF                         (0x12100000)
#define HwVIOC_CPUIF            ( (BASE_ADDR_CPUIF + 0x0000        ))
#define HwVIOC_CPUIF0           ( (BASE_ADDR_CPUIF + 0x0000        ))
// [09:08] == 2'b00,
// [09:08] == 2'b00, [07:06] == 2'b01, [05] == 1'b0
// [09:08] == 2'b00, [07:06] == 2'b01, [05] == 1'b1
// [09:08] == 2'b00, [07:06] == 2'b10, [05] == 1'b0
// [09:08] == 2'b00, [07:06] == 2'b10, [05] == 1'b1
#define HwVIOC_CPUIF1           ( (BASE_ADDR_CPUIF + 0x0100        ))

// [09:08] == 2'b01,
// [09:08] == 2'b01, [07:06] == 2'b01, [05] == 1'b0
// [09:08] == 2'b01, [07:06] == 2'b01, [05] == 1'b1
// [09:08] == 2'b01, [07:06] == 2'b10, [05] == 1'b0
// [09:08] == 2'b01, [07:06] == 2'b10, [05] == 1'b1
/* OUT CONFIGURE */
#define HwVIOC_OUTCFG           ( (BASE_ADDR_CPUIF + 0x0200        )) // [09:08] == 2'b10,


#define HwNTSCPAL_BASE                          (0x12200000)
#define HwTVE_BASE								(0x12200000)
#define HwNTSCPAL_ENC_CTRL_BASE                 (0x12200800)

// Encoder Mode Control A
#define	HwTVECMDA_PWDENC_PD							Hw7											// Power down mode for entire digital logic of TV encoder
#define	HwTVECMDA_FDRST_1							Hw6											// Chroma is free running as compared to H-sync
#define	HwTVECMDA_FDRST_0							HwZERO										// Relationship between color burst & H-sync is maintained for video standards
#define	HwTVECMDA_FSCSEL(X)							((X)*Hw4)
#define	HwTVECMDA_FSCSEL_NTSC						HwTVECMDA_FSCSEL(0)							// Color subcarrier frequency is 3.57954545 MHz for NTSC
#define	HwTVECMDA_FSCSEL_PALX						HwTVECMDA_FSCSEL(1)							// Color subcarrier frequency is 4.43361875 MHz for PAL-B,D,G,H,I,N
#define	HwTVECMDA_FSCSEL_PALM						HwTVECMDA_FSCSEL(2)							// Color subcarrier frequency is 3.57561149 MHz for PAL-M
#define	HwTVECMDA_FSCSEL_PALCN						HwTVECMDA_FSCSEL(3)							// Color subcarrier frequency is 3.58205625 MHz for PAL-combination N
#define	HwTVECMDA_FSCSEL_MASK						HwTVECMDA_FSCSEL(3)
#define	HwTVECMDA_PEDESTAL							Hw3											// Video Output has a pedestal
#define	HwTVECMDA_NO_PEDESTAL						HwZERO										// Video Output has no pedestal
#define	HwTVECMDA_PIXEL_SQUARE						Hw2											// Input data is at square pixel rates.
#define	HwTVECMDA_PIXEL_601							HwZERO										// Input data is at 601 rates.
#define	HwTVECMDA_IFMT_625							Hw1											// Output data has 625 lines
#define	HwTVECMDA_IFMT_525							HwZERO										// Output data has 525 lines
#define	HwTVECMDA_PHALT_PAL							Hw0											// PAL encoded chroma signal output
#define	HwTVECMDA_PHALT_NTSC						HwZERO										// NTSC encoded chroma signal output

// Encoder Mode Control B
#define	HwTVECMDB_YBIBLK_BLACK						Hw4											// Video data is forced to Black level for Vertical non VBI processed lines.
#define	HwTVECMDB_YBIBLK_BYPASS						HwZERO										// Input data is passed through forn non VBI processed lines.
#define	HwTVECMDB_CBW(X)							((X)*Hw2)
#define	HwTVECMDB_CBW_LOW							HwTVECMDB_CBW(0)							// Low Chroma band-width
#define	HwTVECMDB_CBW_MEDIUM						HwTVECMDB_CBW(1)							// Medium Chroma band-width
#define	HwTVECMDB_CBW_HIGH							HwTVECMDB_CBW(2)							// High Chroma band-width
#define	HwTVECMDB_CBW_MASK							HwTVECMDB_CBW(3)							//
#define	HwTVECMDB_YBW(X)							((X)*Hw0)
#define	HwTVECMDB_YBW_LOW							HwTVECMDB_YBW(0)							// Low Luma band-width
#define	HwTVECMDB_YBW_MEDIUM						HwTVECMDB_YBW(1)							// Medium Luma band-width
#define	HwTVECMDB_YBW_HIGH							HwTVECMDB_YBW(2)							// High Luma band-width
#define	HwTVECMDB_YBW_MASK							HwTVECMDB_YBW(3)							//

// Encoder Clock Generator
#define	HwTVEGLK_XT24_24MHZ							Hw4											// 24MHz Clock input
#define	HwTVEGLK_XT24_27MHZ							HwZERO										// 27MHz Clock input
#define	HwTVEGLK_GLKEN_RST_EN						Hw3											// Reset Genlock
#define	HwTVEGLK_GLKEN_RST_DIS						~Hw3										// Release Genlock
#define	HwTVEGLK_GLKE(X)							((X)*Hw1)
#define	HwTVEGLK_GLKE_INT							HwTVEGLK_GLKE(0)							// Chroma Fsc is generated from internal constants based on current user setting
#define	HwTVEGLK_GLKE_RTCO							HwTVEGLK_GLKE(2)							// Chroma Fsc is adjusted based on external RTCO input
#define	HwTVEGLK_GLKE_CLKI							HwTVEGLK_GLKE(3)							// Chroma Fsc tracks non standard encoder clock (CLKI) frequency
#define	HwTVEGLK_GLKE_MASK							HwTVEGLK_GLKE(3)							//
#define	HwTVEGLK_GLKEN_GLKPL_HIGH					Hw0											// PAL ID polarity is active high
#define	HwTVEGLK_GLKEN_GLKPL_LOW					HwZERO										// PAL ID polarity is active low

// Encoder Mode Control C
#define	HwTVECMDC_CSMDE_EN							Hw7											// Composite Sync mode enabled
#define	HwTVECMDC_CSMDE_DIS							~Hw7										// Composite Sync mode disabled (pin is tri-stated)
#define	HwTVECMDC_CSMD(X)							((X)*Hw5)
#define	HwTVECMDC_CSMD_CSYNC						HwTVECMDC_CSMD(0)							// CSYN pin is Composite sync signal
#define	HwTVECMDC_CSMD_KEYCLAMP						HwTVECMDC_CSMD(1)							// CSYN pin is Keyed clamp signal
#define	HwTVECMDC_CSMD_KEYPULSE						HwTVECMDC_CSMD(2)							// CSYN pin is Keyed pulse signal
#define	HwTVECMDC_CSMD_MASK							HwTVECMDC_CSMD(3)
#define	HwTVECMDC_RGBSYNC(X)						((X)*Hw3)
#define	HwTVECMDC_RGBSYNC_NOSYNC					HwTVECMDC_RGBSYNC(0)						// Disable RGBSYNC (when output is configured for analog EGB mode)
#define	HwTVECMDC_RGBSYNC_RGB						HwTVECMDC_RGBSYNC(1)						// Sync on RGB output signal (when output is configured for analog EGB mode)
#define	HwTVECMDC_RGBSYNC_G							HwTVECMDC_RGBSYNC(2)						// Sync on G output signal (when output is configured for analog EGB mode)
#define	HwTVECMDC_RGBSYNC_MASK						HwTVECMDC_RGBSYNC(3)

// DAC Output Selection
#define	HwTVEDACSEL_DACSEL_CODE0					HwZERO										// Data output is diabled (output is code '0')
#define	HwTVEDACSEL_DACSEL_CVBS						Hw0											// Data output in CVBS format

// DAC Power Down
#define	HwTVEDACPD_PD_EN							Hw0											// DAC Power Down Enabled
#define	HwTVEDACPD_PD_DIS							~Hw0										// DAC Power Down Disabled

// Sync Control
#define	HwTVEICNTL_FSIP_ODDHIGH						Hw7											// Odd field active high
#define	HwTVEICNTL_FSIP_ODDLOW						HwZERO										// Odd field active low
#define	HwTVEICNTL_VSIP_HIGH						Hw6											// V-sync active high
#define	HwTVEICNTL_VSIP_LOW							HwZERO										// V-sync active low
#define	HwTVEICNTL_HSIP_HIGH						Hw5											// H-sync active high
#define	HwTVEICNTL_HSIP_LOW							HwZERO										// H-sync active low
#define	HwTVEICNTL_HSVSP_RISING						Hw4											// H/V-sync latch enabled at rising edge
#define	HwTVEICNTL_HVVSP_FALLING					HwZERO										// H/V-sync latch enabled at falling edge
#define	HwTVEICNTL_VSMD_START						Hw3											// Even/Odd field H/V sync output are aligned to video line start
#define	HwTVEICNTL_VSMD_MID							HwZERO										// Even field H/V sync output are aligned to video line midpoint
#define	HwTVEICNTL_ISYNC(X)							((X)*Hw0)
#define	HwTVEICNTL_ISYNC_FSI						HwTVEICNTL_ISYNC(0)							// Alignment input format from FSI pin
#define	HwTVEICNTL_ISYNC_HVFSI						HwTVEICNTL_ISYNC(1)							// Alignment input format from HSI,VSI,FSI pin
#define	HwTVEICNTL_ISYNC_HVSI						HwTVEICNTL_ISYNC(2)							// Alignment input format from HSI,VSI pin
#define	HwTVEICNTL_ISYNC_VFSI						HwTVEICNTL_ISYNC(3)							// Alignment input format from VSI,FSI pin
#define	HwTVEICNTL_ISYNC_VSI						HwTVEICNTL_ISYNC(4)							// Alignment input format from VSI pin
#define	HwTVEICNTL_ISYNC_ESAV_L						HwTVEICNTL_ISYNC(5)							// Alignment input format from EAV,SAV codes (line by line)
#define	HwTVEICNTL_ISYNC_ESAV_F						HwTVEICNTL_ISYNC(6)							// Alignment input format from EAV,SAV codes (frame by frame)
#define	HwTVEICNTL_ISYNC_FREE						HwTVEICNTL_ISYNC(7)							// Alignment is free running (Master mode)
#define	HwTVEICNTL_ISYNC_MASK						HwTVEICNTL_ISYNC(7)

// Offset Control
#define	HwTVEHVOFFST_INSEL(X)						((X)*Hw6)
#define	HwTVEHVOFFST_INSEL_BW16_27MHZ				HwTVEHVOFFST_INSEL(0)						// 16bit YUV 4:2:2 sampled at 27MHz
#define	HwTVEHVOFFST_INSEL_BW16_13P5MH				HwTVEHVOFFST_INSEL(1)						// 16bit YUV 4:2:2 sampled at 13.5MHz
#define	HwTVEHVOFFST_INSEL_BW8_13P5MHZ				HwTVEHVOFFST_INSEL(2)						// 8bit YUV 4:2:2 sampled at 13.5MHz
#define	HwTVEHVOFFST_INSEL_MASK						HwTVEHVOFFST_INSEL(3)
#define	HwTVEHVOFFST_VOFFST_256						Hw3											// Vertical offset bit 8 (Refer to HwTVEVOFFST)
#define	HwTVEHVOFFST_HOFFST_1024					Hw2											// Horizontal offset bit 10 (Refer to HwTVEHOFFST)
#define	HwTVEHVOFFST_HOFFST_512						Hw1											// Horizontal offset bit 9 (Refer to HwTVEHOFFST)
#define	HwTVEHVOFFST_HOFFST_256						Hw0											// Horizontal offset bit 8 (Refer to HwTVEHOFFST)

// Sync Output Control
#define	HwTVEHSVSO_VSOB_256							Hw6											// VSOB bit 8 (Refer to HwVSOB)
#define	HwTVEHSVSO_HSOB_1024						Hw5											// HSOB bit 10 (Refer to HwHSOB)
#define	HwTVEHSVSO_HSOB_512							Hw4											// HSOB bit 9 (Refer to HwHSOB)
#define	HwTVEHSVSO_HSOB_256							Hw3											// HSOB bit 8 (Refer to HwHSOB)
#define	HwTVEHSVSO_HSOE_1024						Hw2											// HSOE bit 10 (Refer to HwHSOE)
#define	HwTVEHSVSO_HSOE_512							Hw1											// HSOE bit 9 (Refer to HwHSOE)
#define	HwTVEHSVSO_HSOE_256							Hw0											// HSOE bit 8 (Refer to HwHSOE)

// Trailing Edge of Vertical Sync Control
#define	HwTVEVSOE_VSOST(X)							((X)*Hw6)									// Programs V-sync relative location for Odd/Even Fields.
#define	HwTVEVSOE_NOVRST_EN							Hw5											// No vertical reset on every field
#define	HwTVEVSOE_NOVRST_NORMAL						HwZERO										// Normal vertical reset operation (interlaced output timing)
#define	HwTVEVSOE_VSOE(X)							((X)*Hw0)									// Trailing Edge of Vertical Sync Control

// VBI Control Register
#define	HwTVEVCTRL_VBICTL(X)						((X)*Hw5)									// VBI Control indicating the current line is VBI.
#define	HwTVEVCTRL_VBICTL_NONE						HwTVEVCTRL_VBICTL(0)						// Do nothing, pass as active video.
#define	HwTVEVCTRL_VBICTL_10LINE					HwTVEVCTRL_VBICTL(1)						// Insert blank(Y:16, Cb,Cr: 128), for example, 10 through 21st line.
#define	HwTVEVCTRL_VBICTL_1LINE						HwTVEVCTRL_VBICTL(2)						// Insert blank data 1 line less for CC processing.
#define	HwTVEVCTRL_VBICTL_2LINE						HwTVEVCTRL_VBICTL(3)						// Insert blank data 2 line less for CC and CGMS processing.
#define	HwTVEVCTRL_MASK								HwTVEVCTRL_VBICTL(3)
#define	HwTVEVCTRL_CCOE_EN							Hw4											// Closed caption odd field enable.
#define	HwTVEVCTRL_CCEE_EN							Hw3											// Closed caption even field enable.
#define	HwTVEVCTRL_CGOE_EN							Hw2											// Copy generation management system enable odd field.
#define	HwTVEVCTRL_CGEE_EN							Hw1											// Copy generation management system enable even field.
#define	HwTVEVCTRL_WSSE_EN							Hw0											// Wide screen enable.

// Connection between LCDC & TVEncoder Control
#define	HwTVEVENCON_EN_EN							Hw0											// Connection between LCDC & TVEncoder Enabled
#define	HwTVEVENCON_EN_DIS							~Hw0										// Connection between LCDC & TVEncoder Disabled

// I/F between LCDC & TVEncoder Selection
#define	HwTVEVENCIF_MV_1							Hw1											// reserved
#define	HwTVEVENCIF_FMT_1							Hw0											// PXDATA[7:0] => CIN[7:0], PXDATA[15:8] => YIN[7:0]
#define	HwTVEVENCIF_FMT_0							HwZERO										// PXDATA[7:0] => YIN[7:0], PXDATA[15:8] => CIN[7:0]




#define HwHDMI_CTRL_BASE                        (0x12300000)
#define HwHDMI_CORE_BASE                        (0x12310000)
#define HwHDMI_AES_BASE                         (0x12320000)
#define HwHDMI_SPDIF_BASE                       (0x12330000)
#define HwHDMI_I2S_BASE                         (0x12340000)
#define HwHDMI_CEC_BASE                         (0x12350000)

#endif //__REG_PHYSICAL_H__
