/*
 * Copyright (C) Telechips Inc.
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

#ifndef _PLATFORM_REG_PHYSICAL_H_
#define _PLATFORM_REG_PHYSICAL_H_

/************************************************************************
*    Bit Field Definition
************************************************************************/
#define    Hw37                                 (1LL << 37)
#define    Hw36                                 (1LL << 36)
#define    Hw35                                 (1LL << 35)
#define    Hw34                                 (1LL << 34)
#define    Hw33                                 (1LL << 33)
#define    Hw32                                 (1LL << 32)
#define    Hw31                                 0x80000000
#define    Hw30                                 0x40000000
#define    Hw29                                 0x20000000
#define    Hw28                                 0x10000000
#define    Hw27                                 0x08000000
#define    Hw26                                 0x04000000
#define    Hw25                                 0x02000000
#define    Hw24                                 0x01000000
#define    Hw23                                 0x00800000
#define    Hw22                                 0x00400000
#define    Hw21                                 0x00200000
#define    Hw20                                 0x00100000
#define    Hw19                                 0x00080000
#define    Hw18                                 0x00040000
#define    Hw17                                 0x00020000
#define    Hw16                                 0x00010000
#define    Hw15                                 0x00008000
#define    Hw14                                 0x00004000
#define    Hw13                                 0x00002000
#define    Hw12                                 0x00001000
#define    Hw11                                 0x00000800
#define    Hw10                                 0x00000400
#define    Hw9                                  0x00000200
#define    Hw8                                  0x00000100
#define    Hw7                                  0x00000080
#define    Hw6                                  0x00000040
#define    Hw5                                  0x00000020
#define    Hw4                                  0x00000010
#define    Hw3                                  0x00000008
#define    Hw2                                  0x00000004
#define    Hw1                                  0x00000002
#define    Hw0                                  0x00000001
#define    HwZERO                               0x00000000

/*******************************************************************************
 *******************************************************************************/
#define HwOVERLAYMIXER_BASE             (0x72400000)		//TCC893x

#define HwGE_FCH_SSB                    (Hw24+Hw25+Hw26)                    // Operation Mode
#define HwGE_DCH_SSB                    (Hw24+Hw25+Hw26)                    // Operation Mode

// Front-End Channel 0 Control
#define HwGE_FCHO_OPMODE                (Hw8+Hw9+Hw10)                      // Operation Mode
#define HwGE_FCHO_SDFRM                 (Hw0+Hw1+Hw2+Hw3+Hw4)               // Source Data Format

// Front-End Channel 1 Control
#define HwGE_FCH1_OPMODE                (Hw8+Hw9+Hw10)                      // Operation Mode
#define HwGE_FCH1_SDFRM                 (Hw0+Hw1+Hw2+Hw3+Hw4)               // Source Data Format

// Front-End Channel 2 Control
#define HwGE_FCH2_OPMODE                (Hw8+Hw9+Hw10)                      // Operation Mode
#define HwGE_FCH2_SDFRM                 (Hw0+Hw1+Hw2+Hw3+Hw4)               // Source Data Format

// Source Control
#define Hw2D_SACTRL_S2_ARITHMODE        (Hw10+Hw9+Hw8)
#define Hw2D_SACTRL_S1_ARITHMODE        (Hw6+Hw5+Hw4)
#define Hw2D_SACTRL_S0_ARITHMODE        (Hw2+Hw1+Hw0)
#define Hw2D_SFCTRL_S2_Y2REN            (Hw26)
#define Hw2D_SFCTRL_S1_Y2REN            (Hw25)
#define Hw2D_SFCTRL_S0_Y2REN            (Hw24)
#define Hw2D_SFCTRL_S2_Y2RMODE          (Hw21+Hw20)
#define Hw2D_SFCTRL_S1_Y2RMODE          (Hw19+Hw18)
#define Hw2D_SFCTRL_S0_Y2RMODE          (Hw17+Hw16)
#define Hw2D_SACTRL_S2_CHROMAEN         (Hw18)
#define Hw2D_SACTRL_S1_CHROMAEN         (Hw17)
#define Hw2D_SACTRL_S0_CHROMAEN         (Hw16)
#define Hw2D_SFCTRL_S3_SEL				(Hw6+Hw7)
#define Hw2D_SFCTRL_S2_SEL              (Hw5+Hw4)
#define Hw2D_SFCTRL_S1_SEL              (Hw3+Hw2)
#define Hw2D_SFCTRL_S0_SEL              (Hw1+Hw0)

// Source Operator Pattern
#define HwGE_OP_ALL                     (HwGE_ALPHA + HwGE_PAT_RY + HwGE_PAT_GU + HwGE_PAT_BV)
#define HwGE_ALPHA                      (HwGE_PAT_GU + HwGE_PAT_BV )                // ALPHA VALUE
#define HwGE_PAT_RY                     (Hw16+Hw17+Hw18+Hw19+Hw20+Hw21+Hw22+Hw23)   // Pattern Value RED,   Y
#define HwGE_PAT_GU                     (Hw8+Hw9+Hw10+Hw11+Hw12+Hw13+Hw14+Hw15)     // Pattern Value GREEN, U
#define HwGE_PAT_BV                     (Hw0+Hw1+Hw2+Hw3+Hw4+Hw5+Hw6+Hw7)           // Pattern Value BULE,  V

// Source Operation Control
#define HwGE_OP_CTRL_ACON1              (Hw30+Hw29+Hw28)                    // Alpha-value control 1
#define HwGE_OP_CTRL_ACON0              (Hw26+Hw25+Hw24)                    // Alpha-value control 0
#define HwGE_OP_CTRL_CCON1              (Hw23+Hw22+Hw21+Hw20)               // color control 1
#define HwGE_OP_CTRL_CCON0              (Hw19+Hw18+Hw17+Hw16)               // color control 1
#define HwGE_OP_CTRL_ATUNE              (Hw13+Hw12)                         // Alpha value tuning 
#define HwGE_OP_CTRL_CSEL               (Hw9+Hw8)                           // chroma-key 
#define HwGE_OP_CTRL_OPMODE             (Hw4+Hw3+Hw2+Hw1+Hw0)               // operation mode

// Back -End Channel Control
#define HwGE_BCH_DCTRL_MABC             Hw21
#define HwGE_BCH_DCTRL_YSEL             Hw18                                // YUV4:4:4 to YUVx:x:x Y Control
#define HwGE_BCH_DCTRL_XSEL             (Hw16+Hw17)                         // YUV4:4:4 to YUVx:x:x X Control
#define HwGE_BCH_DCTRL_CEN              Hw15                                // Destination Format Converter Control
#define HwGE_BCH_DCTRL_CMODE            (Hw13+Hw14)                         // RGBtoYUV Converter Type
#define HwGE_BCH_DCTRL_DSUV             Hw11
#define HwGE_BCH_DCTRL_OPMODE           (Hw8+Hw9+Hw10)                      // Operation Mode COPY, MIRROR, ROTATE
#define HwGE_BCH_DCTRL_DOP              Hw6
#define HwGE_BCH_DCTRL_DEN              Hw5
#define HwGE_BCH_DCTRL_DDFRM            (Hw0+Hw1+Hw2+Hw3+Hw4)               // Destination Data Format

// Graphic Engine Control
#define HwGE_GE_INT_EN                  Hw16                                // Graphic Engine Interrupt Enable
#define HwGE_GE_CTRL_EN                 (Hw0+Hw1+Hw2)                       // Graphic Engine Enable

// Graphic Engine Interrupt Request
#define HwGE_GE_IREQ_FLG                Hw16                                // Graphic Engine Flag Bit
#define HwGE_GE_IREQ_IRQ                Hw0

typedef struct {
    unsigned VALUE          :32;
} TCC_DEF32BIT_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    TCC_DEF32BIT_IDX_TYPE   bREG;
} TCC_DEF32BIT_TYPE;


typedef struct _OVERLAYMIXER{
    volatile TCC_DEF32BIT_TYPE  FCH0_SADDR0;    // 0x000  R/W  0x00000000   Front-End Channel 0 Source Address 0
    volatile TCC_DEF32BIT_TYPE  FCH0_SADDR1;    // 0x004  R/W  0x00000000   Front-End Channel 0 Source Address 1
    volatile TCC_DEF32BIT_TYPE  FCH0_SADDR2;    // 0x008  R/W  0x00000000   Front-End Channel 0 Source Address 2
    volatile TCC_DEF32BIT_TYPE  FCH0_SFSIZE;    // 0x00C  R/W  0x00000000   Front-End Channel 0 Source Frame Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH0_SOFF;      // 0x010  R/W  0x00000000   Front-End Channel 0 Source Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH0_SISIZE;    // 0x014  R/W  0x00000000   Front-End Channel 0 Source Image Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH0_WOFF;      // 0x018  R/W  0x00000000   Front-End Channel 0 Window Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH0_SCTRL;     // 0x01C  R/W  0x00000000   Front-End Channel 0 Control

    volatile TCC_DEF32BIT_TYPE  FCH1_SADDR0;    // 0x020  R/W  0x00000000   Front-End Channel 1 Source Address 0
    volatile TCC_DEF32BIT_TYPE  FCH1_SADDR1;    // 0x024  R/W  0x00000000   Front-End Channel 1 Source Address 1
    volatile TCC_DEF32BIT_TYPE  FCH1_SADDR2;    // 0x028  R/W  0x00000000   Front-End Channel 1 Source Address 2
    volatile TCC_DEF32BIT_TYPE  FCH1_SFSIZE;    // 0x02C  R/W  0x00000000   Front-End Channel 1 Source Frame Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH1_SOFF;      // 0x030  R/W  0x00000000   Front-End Channel 1 Source Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH1_SISIZE;    // 0x034  R/W  0x00000000   Front-End Channel 1 Source Image Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH1_WOFF;      // 0x038  R/W  0x00000000   Front-End Channel 1 Window Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH1_SCTRL;     // 0x03C  R/W  0x00000000   Front-End Channel 1 Control
/*
    volatile TCC_DEF32BIT_TYPE  FCH2_SADDR0;    // 0x040  R/W  0x00000000   Front-End Channel 2 Source Address 0
    volatile TCC_DEF32BIT_TYPE  FCH2_SADDR1;    // 0x044  R/W  0x00000000   Front-End Channel 2 Source Address 1
    volatile TCC_DEF32BIT_TYPE  FCH2_SADDR2;    // 0x048  R/W  0x00000000   Front-End Channel 2 Source Address 2
    volatile TCC_DEF32BIT_TYPE  FCH2_SFSIZE;    // 0x04C  R/W  0x00000000   Front-End Channel 2 Source Frame Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH2_SOFF;      // 0x050  R/W  0x00000000   Front-End Channel 2 Source Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH2_SISIZE;    // 0x054  R/W  0x00000000   Front-End Channel 2 Source Image Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH2_WOFF;      // 0x058  R/W  0x00000000   Front-End Channel 2 Window Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH2_SCTRL;     // 0x05C  R/W  0x00000000   Front-End Channel 2 Control

    volatile TCC_DEF32BIT_TYPE  FCH3_SADDR0;    // 0x060  R/W  0x00000000   Front-End Channel 3 Source Address 0
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  FCH3_SFSIZE;    // 0x06C  R/W  0x00000000   Front-End Channel 3 Source Frame Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH3_SOFF;      // 0x070  R/W  0x00000000   Front-End Channel 3 Source Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH3_SISIZE;    // 0x074  R/W  0x00000000   Front-End Channel 3 Source Image Pixel Size
    volatile TCC_DEF32BIT_TYPE  FCH3_WOFF;      // 0x078  R/W  0x00000000   Front-End Channel 3 Window Pixel Offset
    volatile TCC_DEF32BIT_TYPE  FCH3_SCTRL;     // 0x07C  R/W  0x00000000   Front-End Channel 3 Control
*/
    volatile TCC_DEF32BIT_TYPE  S0_CHROMA;      // 0x080  R/W  0x00000000   Source 0 Chroma-Key Parameter
    volatile TCC_DEF32BIT_TYPE  S0_PAR;         // 0x084  R/W  0x00000000   Source 0 Arithmetic Parameter
/*
    volatile TCC_DEF32BIT_TYPE  S1_CHROMA;      // 0x088  R/W  0x00000000   Source 1 Chroma-Key Parameter
    volatile TCC_DEF32BIT_TYPE  S1_PAR;         // 0x08C  R/W  0x00000000   Source 1 Arithmetic Parameter

    volatile TCC_DEF32BIT_TYPE  S2_CHROMA;      // 0x090  R/W  0x00000000   Source 2 Chroma-Key Parameter
    volatile TCC_DEF32BIT_TYPE  S2_PAR;         // 0x094  R/W  0x00000000   Source 2 Arithmetic Parameter
    volatile TCC_DEF32BIT_TYPE  S3_CHROMA;      // 0x098  R/W  0x00000000   Source 3 Chroma-Key Parameter
    volatile TCC_DEF32BIT_TYPE  S3_PAR;         // 0x09C  R/W  0x00000000   Source 3 Arithmetic Parameter
*/
    volatile TCC_DEF32BIT_TYPE  SF_CTRL;        // 0x0A0  R/W  0x00000000   Source Function Control Register
    volatile TCC_DEF32BIT_TYPE  SA_CTRL;        // 0x0A4  R/W  0x00000000   Source Arithmetic Control Register
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  OP0_ALPHA;      // 0x0B0  R/W  0x00000000   Source Operator 0 Alpha
//    volatile TCC_DEF32BIT_TYPE  OP1_ALPHA;      // 0x0B4  R/W  0x00000000   Source Operator 1 Alpha
//    volatile TCC_DEF32BIT_TYPE  OP2_ALPHA;      // 0x0B4  R/W  0x00000000   Source Operator 2 Alpha
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  OP0_PAT;        // 0x0C0  R/W  0x00000000   Source Operator 0 Pattern
//    volatile TCC_DEF32BIT_TYPE  OP1_PAT;        // 0x0C4  R/W  0x00000000   Source Operator 1 Pattern
//    volatile TCC_DEF32BIT_TYPE  OP2_PAT;        // 0x0C8  R/W  0x00000000   Source Operator 2 Pattern
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  OP0_CTRL;       // 0x0D0  R/W  0x00000000   Source Operation 0 Control Register
//    volatile TCC_DEF32BIT_TYPE  OP1_CTRL;       // 0x0D4  R/W  0x00000000   Source Operation 1 Control Register
//    volatile TCC_DEF32BIT_TYPE  OP2_CTRL;       // 0x0D8  R/W  0x00000000   Source Operation 2 Control Register
    unsigned :32;
    volatile TCC_DEF32BIT_TYPE  BCH_DADDR0;     // 0x0E0  R/W  0x00000000   Back-End Channel Destination Address 0
    volatile TCC_DEF32BIT_TYPE  BCH_DADDR1;     // 0x0E4  R/W  0x00000000   Back-End Channel Destination Address 1
    volatile TCC_DEF32BIT_TYPE  BCH_DADDR2;     // 0x0E8  R/W  0x00000000   Back-End Channel Destination Address 2
    volatile TCC_DEF32BIT_TYPE  BCH_DFSIZE;     // 0x0EC  R/W  0x00000000   Back-End Channel Destination Frame Pixel Size
    volatile TCC_DEF32BIT_TYPE  BCH_DOFF;       // 0x0F0  R/W  0x00000000   Back-End Channel Destination Pixel Offset
    volatile TCC_DEF32BIT_TYPE  BCH_DCTRL;      // 0x0F4  R/W  0x00000000   Back-End Channel Control
    unsigned :32; unsigned :32;
    volatile TCC_DEF32BIT_TYPE  BCH_DDMAT0;     // 0x100  R/W  0x00000000   Back-End Channel Destination Dither Matrix 0
    volatile TCC_DEF32BIT_TYPE  BCH_DDMAT1;     // 0x104  R/W  0x00000000   Back-End Channel Destination Dither Matrix 1
    volatile TCC_DEF32BIT_TYPE  BCH_DDMAT2;     // 0x108  R/W  0x00000000   Back-End Channel Destination Dither Matrix 2
    volatile TCC_DEF32BIT_TYPE  BCH_DDMAT3;     // 0x10C  R/W  0x00000000   Back-End Channel Destination Dither Matrix 3
    volatile TCC_DEF32BIT_TYPE  OM_CTRL;        // 0x110  R/W  0x00000000   Overlay Mixer Control
    volatile TCC_DEF32BIT_TYPE  OM_IREQ;        // 0x114  R/W  0x00000000   Overlay Mixer Interrupt Request
    unsigned :32; unsigned :32;
    TCC_DEF32BIT_TYPE           NOTDEFINED[184];  // 0x120 - 0x3FF //kch 120->184
    volatile TCC_DEF32BIT_TYPE  FCH0_LUT[256];  // 0x400  R/W  -            Front-End Channel 0 Lookup Table
    volatile TCC_DEF32BIT_TYPE  FCH1_LUT[256];  // 0x800  R/W  -            Front-End Channel 1 Lookup Table
//    volatile TCC_DEF32BIT_TYPE  FCH2_LUT[256];  // 0xC00  R/W  -            Front-End Channel 2 Lookup Table
} OVERLAYMIXER, *POVERLAYMIXER;

#endif /* _PLATFORM_REG_PHYSICAL_H_ */
