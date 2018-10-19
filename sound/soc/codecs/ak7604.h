/*
 * ak7604.h  --  audio driver for ak7604
 *
 * Copyright (C) 2017 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      17/11/27        1.0
 *                      18/04/30        2.0 : Audio Effect Implemented
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _AK7604_H
#define _AK7604_H

#include "ak7604ctl.h"

#define AK7604_I2C_IF
#define AK7604_IO_CONTROL

/* AUDIO EFFECT FUNCTION DEFINITION */
#define AK7604_AUDIO_EFFECT

#define NUM_SYNCDOMAIN  4
#define NUM_SDIO		4

#define AK7604_00_SYSTEMCLOCK_1     0x00
#define AK7604_01_SYSTEMCLOCK_2     0x01
#define AK7604_02_SYSTEMCLOCK_3     0x02
#define AK7604_03_RESERVED          0x03
#define AK7604_04_SYNCDOMAIN_MS     0x04
#define AK7604_05_SYNCDOMAIN1_SET1  0x05
#define AK7604_06_SYNCDOMAIN1_SET2  0x06
#define AK7604_07_SYNCDOMAIN2_SET1  0x07
#define AK7604_08_SYNCDOMAIN2_SET2  0x08
#define AK7604_09_SYNCDOMAIN3_SET1  0x09
#define AK7604_0A_SYNCDOMAIN3_SET2  0x0A
#define AK7604_0B_SYNCDOMAIN4_SET1  0x0B
#define AK7604_0C_SYNCDOMAIN4_SET2  0x0C
#define AK7604_0D_SDIN1_2_SYNC      0x0D
#define AK7604_0E_SDIN3_4_SYNC      0x0E
#define AK7604_0F_SYNCDOMAIN_SEL1   0x0F
#define AK7604_10_SYNCDOMAIN_SEL2   0x10
#define AK7604_11_SYNCDOMAIN_SEL3   0x11
#define AK7604_12_SYNCDOMAIN_SEL4   0x12
#define AK7604_13_SYNCDOMAIN_SEL5   0x13
#define AK7604_14_SYNCDOMAIN_SEL6   0x14
#define AK7604_15_SYNCDOMAIN_SEL7   0x15
#define AK7604_16_SYNCDOMAIN_SEL8   0x16
#define AK7604_17_SYNCDOMAIN_SEL9   0x17
#define AK7604_18_SYNCDOMAIN_SEL10  0x18
#define AK7604_19_SDOUT1A_SELECT    0x19
#define AK7604_1A_SDOUT1B_SELECT    0x1A
#define AK7604_1B_SDOUT1C_SELECT    0x1B
#define AK7604_1C_SDOUT1D_SELECT    0x1C
#define AK7604_1D_SDOUT2_SELECT     0x1D
#define AK7604_1E_SDOUT3_SELECT     0x1E
#define AK7604_1F_DAC1_SELECT       0x1F
#define AK7604_20_DAC2_SELECT       0x20
#define AK7604_21_DAC3_SELECT       0x21
#define AK7604_22_DSPDIN1_SELECT    0x22
#define AK7604_23_DSPDIN2_SELECT    0x23
#define AK7604_24_DSPDIN3_SELECT    0x24
#define AK7604_25_DSPDIN4_SELECT    0x25
#define AK7604_26_DSPDIN5_SELECT    0x26
#define AK7604_27_DSPDIN6_SELECT    0x27
#define AK7604_28_SRC1_SELECT       0x28
#define AK7604_29_SRC2_SELECT       0x29
#define AK7604_2A_SRC3_SELECT       0x2A
#define AK7604_2B_SRC4_SELECT       0x2B
#define AK7604_2C_CLOCKFORMAT1      0x2C
#define AK7604_2D_CLOCKFORMAT2      0x2D
#define AK7604_2E_SDIN1_FORMAT      0x2E
#define AK7604_2F_SDIN2_FORMAT      0x2F
#define AK7604_30_SDIN3_FORMAT      0x30
#define AK7604_31_SDIN4_FORMAT      0x31
#define AK7604_32_SDOUT1_FORMAT     0x32
#define AK7604_33_SDOUT2_FORMAT     0x33
#define AK7604_34_SDOUT3_FORMAT     0x34
#define AK7604_50_INPUT_DATA        0x50
#define AK7604_51_SELECT_DATA       0x51
#define AK7604_53_STO_SETTING       0x53
#define AK7604_60_DSP_SETTING1      0x60
#define AK7604_61_DSP_SETTING2      0x61
#define AK7604_71_SRCMUTE_SETTING   0x71
#define AK7604_72_SRCGROUTP_SETTING 0x72
#define AK7604_73_SRCFILTER_SETTING 0x73
#define AK7604_81_MIC_SETTING       0x81
#define AK7604_82_MIC_GAIN          0x82
#define AK7604_83_DAC1L_VOLUME      0x83
#define AK7604_84_DAC1R_VOLUME      0x84
#define AK7604_85_DAC2L_VOLUME      0x85
#define AK7604_86_DAC2R_VOLUME      0x86
#define AK7604_87_DAC3L_VOLUME      0x87
#define AK7604_88_DAC3R_VOLUME      0x88
#define AK7604_89_DAC_MUTEFILTER    0x89
#define AK7604_8A_DAC_DEEM_         0x8A
#define AK7604_8B_ADCL_VOLUME       0x8B
#define AK7604_8C_ADCR_VOLUME       0x8C
#define AK7604_8D_AIN_SELECT        0x8D
#define AK7604_8E_ADC_MUTEHPF       0x8E
#define AK7604_A1_POWERMANAGEMENT1  0xA1
#define AK7604_A2_POWERMANAGEMENT2  0xA2
#define AK7604_A3_RESETCONTROL      0xA3
#define AK7604_C0_DEVICE_ID         0xC0
#define AK7604_C1_REVISION_NUM      0xC1
#define AK7604_C2_DSPERROR_STATUS   0xC2
#define AK7604_C3_SRC_STATUS        0xC3
#define AK7604_C6_STO_READOUT       0xC6
#define AK7604_VIRT_C7_DSPOUT1_MIX  0xC7
#define AK7604_VIRT_C8_DSPOUT2_MIX  0xC8
#define AK7604_VIRT_C9_DSPOUT3_MIX  0xC9
#define AK7604_VIRT_CA_DSPOUT4_MIX  0xCA
#define AK7604_VIRT_CB_DSPOUT5_MIX  0xCB
#define AK7604_VIRT_CC_DSPOUT6_MIX  0xCC

#define AK7604_MAX_REGISTER     AK7604_VIRT_CC_DSPOUT6_MIX

#define AK7604_VIRT_REGISTER    AK7604_VIRT_C7_DSPOUT1_MIX

/* AK7604_C1_CLOCK_2 (0xC1)  Fields */
#define AK7604_AIF_BICK32		(2 << 4)
#define AK7604_AIF_BICK48		(1 << 4)
#define AK7604_AIF_BICK64		(0 << 4)
#define AK7604_AIF_TDM			(3 << 4)	//TDM256 bit is set "1" at initialization.

/* TDMMODE Input Source */
#define AK7604_TDM_DSP				(0 << 2)
#define AK7604_TDM_DSP_AD1			(1 << 2)
#define AK7604_TDM_DSP_AD1_AD2		(2 << 2)

/* User Setting */
//#define DIGITAL_MIC
//#define CLOCK_MODE_BICK
//#define CLOCK_MODE_18_432
#define AK7604_AUDIO_IF_MODE		AK7604_AIF_BICK64	//32fs, 48fs, 64fs, 256fs(TDM)
#define AK7604_TDM__INPUTSOURCE		AK7604_TDM_DSP		//Effective only in TDM mode
#define AK7604_BCKP_BIT				(0 << 6)	//BICK Edge Setting
#define AK7604_SOCFG_BIT  			(0 << 4)	//SO pin Hi-z Setting
#define AK7604_DMCLKP1_BIT			(0 << 6)	//DigitalMIC 1 Channnel Setting
#define AK7604_DMCLKP2_BIT			(0 << 3)	//DigitalMIC 1 Channnel Setting
/* User Setting */

/* Bitfield Definitions */

/* AK7604_C0_CLOCK_1 (0xC0) Fields */
#define AK7604_FS				0x07
#define AK7604_FS_8KHZ			(0x00 << 0)
#define AK7604_FS_12KHZ			(0x01 << 0)
#define AK7604_FS_16KHZ			(0x02 << 0)
#define AK7604_FS_24KHZ			(0x03 << 0)
#define AK7604_FS_32KHZ			(0x04 << 0)
#define AK7604_FS_48KHZ			(0x05 << 0)
#define AK7604_FS_96KHZ			(0x06 << 0)

#define AK7604_M_S				0x30		//CKM1-0 (CKM2 bit is not use)
#define AK7604_M_S_0			(0 << 4)	//Master, XTI=12.288MHz
#define AK7604_M_S_1			(1 << 4)	//Master, XTI=18.432MHz
#define AK7604_M_S_2			(2 << 4)	//Slave, XTI=12.288MHz
#define AK7604_M_S_3			(3 << 4)	//Slave, BICK

/* AK7604_C2_SERIAL_DATA_FORMAT (0xC2) Fields */
/* LRCK I/F Format */
#define AK7604_LRIF_I2S_MODE		0
#define AK7604_LRIF_MSB_MODE		5
#define AK7604_LRIF_LSB_MODE		5
#define AK7604_LRIF_PCM_SHORT_MODE	6
#define AK7604_LRIF_PCM_LONG_MODE	7
/* Input Format is set as "MSB(24- bit)" by following register.
   CONT03(DIF2,DOF2), CONT06(DIFDA, DIF1), CONT07(DOF4,DOF3,DOF1) */

/* AK7604_CA_CLK_SDOUT_ (0xCA) Fields */
#define AK7604_BICK_LRCK			(3 << 5)	//BICKOE, LRCKOE


#define MAX_LOOP_TIMES		3

#define COMMAND_WRITE_REG			0xC0
#define COMMAND_READ_REG			0x40

#define COMMAND_CRC_READ			0x72
#define COMMAND_MIR_READ			0x76

#define COMMAND_WRITE_CRAM_RUN		0x80
#define COMMAND_WRITE_CRAM_EXEC     0xA4

#define COMMAND_WRITE_CRAM          0xB4
#define COMMAND_WRITE_PRAM          0xB8

#define	TOTAL_NUM_OF_PRAM_MAX		20483
#define	TOTAL_NUM_OF_CRAM_MAX		18435

static const char *ak7604_firmware_pram[] =
{
    "Off",
    "basic",
    "data1",
    "data2",
    "data3",
    "data4",
    "data5",
	"header",
};

static const char *ak7604_firmware_cram[] =
{
    "Off",
    "basic",
    "data1",
    "data2",
    "data3",
    "data4",
    "data5",
	"header",
};

enum {
	AIF_PORT1 = 0,
	AIF_PORT2,
	AIF_PORT3,
	AIF_PORT4,
	NUM_AIF_DAIS,
};

#endif

