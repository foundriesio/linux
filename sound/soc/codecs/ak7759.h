/*
 * ak7759.h  --  audio driver for ak7759
 *
 * Copyright (C) 2018 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      18/02/19        1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef _AK7759_H
#define _AK7759_H

#include "ak7759ctl.h"

#define AK7759_I2C_IF
#define AK7759_PCM_BCKP // BICK Pos edge setting for PCM format
#define AK7759_IO_CONTROL

#define NUM_SYNCDOMAIN  2

/* User Setting */
#define  AK7759_00_SYSCLOCK_SETTING1         0x00
#define  AK7759_01_SYSCLOCK_SETTING2         0x01
#define  AK7759_02_BUSCLOCK_SETTING          0x02
#define  AK7759_03_MASTER_SLAVE_SETTING      0x03
#define  AK7759_04_SYNCDOMAIN1_SETTING1      0x04
#define  AK7759_05_SYNCDOMAIN1_SETTING2      0x05
#define  AK7759_06_SYNCDOMAIN2_SETTING1      0x06
#define  AK7759_07_SYNCDOMAIN2_SETTING2      0x07
#define  AK7759_08_SYNCPORT_SELECT           0x08
#define  AK7759_09_SYNCDOMAIN_SELECT1        0x09
#define  AK7759_0A_SYNCDOMAIN_SELECT2        0x0A
#define  AK7759_0B_SYNCDOMAIN_SELECT3        0x0B
#define  AK7759_0C_SYNCDOMAIN_SELECT4        0x0C
#define  AK7759_0D_SYNCDOMAIN_SELECT5        0x0D
#define  AK7759_0E_SYNCDOMAIN_SELECT6        0x0E
#define  AK7759_0F_SYNCDOMAIN_SELECT7        0x0F
#define  AK7759_10_SDOUT1A_OUTPUT_SELECT     0x10
#define  AK7759_11_SDOUT1B_OUTPUT_SELECT     0x11
#define  AK7759_12_SDOUT1C_OUTPUT_SELECT     0x12
#define  AK7759_13_SDOUT1D_OUTPUT_SELECT     0x13
#define  AK7759_14_SDOUT2_OUTPUT_SELECT      0x14
#define  AK7759_15_SDOUT3_OUTPUT_SELECT      0x15
#define  AK7759_16_DSP_IN1_INPUT_SELECT      0x16
#define  AK7759_17_DSP_IN2_INPUT_SELECT      0x17
#define  AK7759_18_DSP_IN3_INPUT_SELECT      0x18
#define  AK7759_19_DSP_IN4_INPUT_SELECT      0x19
#define  AK7759_1A_DAC_INPUT_SELECT          0x1A
#define  AK7759_1B_MIXER_CH1_INPUT_SELECT    0x1B
#define  AK7759_1C_MIXER_CH2_INPUT_SELECT    0x1C
#define  AK7759_1D_DIT_INPUT_SELECT          0x1D
#define  AK7759_20_CLOCK_FORMAT_SETTING      0x20
#define  AK7759_21_SDIN1_INPUT_FORMAT        0x21
#define  AK7759_22_SDIN2_INPUT_FORMAT        0x22
#define  AK7759_23_SDOUT1_OUTPUT_FORMAT      0x23
#define  AK7759_24_SDOUT2_OUTPUT_FORMAT      0x24
#define  AK7759_25_SDOUT3_OUTPUT_FORMAT      0x25
#define  AK7759_28_INPUT_DATA_SELECT         0x28
#define  AK7759_29_OUTPUT_DATA_SELECT1       0x29
#define  AK7759_2A_OUTPUT_DATA_SELECT2       0x2A
#define  AK7759_2B_CLKO_SETTING              0x2B
#define  AK7759_2C_OUTPUT_ENABLE             0x2C
#define  AK7759_2D_STO_SETTING               0x2D
#define  AK7759_30_DSP_SETTING1              0x30
#define  AK7759_31_DSP_SETTING2              0x31
#define  AK7759_32_DSP_JX_ENABLE             0x32
#define  AK7759_34_DSP_DATA_LENGTH           0x34
#define  AK7759_35_DSP_JX_FILTER_SETTING     0x35
#define  AK7759_36_DSP_SHIFT_SETTING         0x36
#define  AK7759_38_MIXER_SETTING             0x38
#define  AK7759_39_DIT_STATUS1               0x39
#define  AK7759_3A_DIT_STATUS2               0x3A
#define  AK7759_3B_DIT_STATUS3               0x3B
#define  AK7759_3C_DIT_STATUS4               0x3C
#define  AK7759_40_ADC_SETTING               0x40
#define  AK7759_41_DMIC_SETTING              0x41
#define  AK7759_42_DMIC_CLOCK_SETTING        0x42
#define  AK7759_43_DAC_SETTING               0x43
#define  AK7759_44_MIC_GAIN                  0x44
#define  AK7759_45_ADC_LCH_DIGITAL_VOLUME    0x45
#define  AK7759_46_ADC_RCH_DIGITAL_VOLUME    0x46
#define  AK7759_47_DMIC_LCH_DIGITAL_VOLUME   0x47
#define  AK7759_48_DMIC_RCH_DIGITAL_VOLUME   0x48
#define  AK7759_49_DAC_LCH_DIGITAL_VOLUME    0x49
#define  AK7759_4A_DAC_RCH_DIGITAL_VOLUME    0x4A
#define  AK7759_4B_ADC_DAC_MUTE              0x4B
#define  AK7759_4C_AIN_SW                    0x4C
#define  AK7759_4D_OUT_SW                    0x4D
#define  AK7759_50_POWER_MANAGEMENT          0x50
#define  AK7759_51_RESET_CONTROL             0x51
#define  AK7759_80_DEVNO                     0x80
#define  AK7759_81_REVISION_NUM              0x81
#define  AK7759_82_ERROR_STATUS              0x82
#define  AK7759_83_MIC_GAIN_READOUT          0x83
#define  AK7759_VIRT_84_DSP1OUT1_MIX         0x84
#define  AK7759_VIRT_85_DSP1OUT2_MIX         0x85
#define  AK7759_VIRT_86_DSP1OUT3_MIX         0x86
#define  AK7759_VIRT_87_DSP1OUT4_MIX         0x87

#define AK7759_MAX_REGISTER     AK7759_VIRT_87_DSP1OUT4_MIX
#define AK7759_VIRT_REGISTER    AK7759_VIRT_84_DSP1OUT1_MIX

/* Bitfield Definitions */


/* AK7759_C2_SERIAL_DATA_FORMAT (0xC2) Fields */
/* LRCK I/F Format */
#define AK7759_LRIF_I2S_MODE		0
#define AK7759_LRIF_MSB_MODE		5
#define AK7759_LRIF_LSB_MODE		5

#ifndef AK7759_PCM_BCKP
#define AK7759_LRIF_PCM_SHORT_MODE	0xE
#define AK7759_LRIF_PCM_LONG_MODE	0xF
#else
#define AK7759_LRIF_PCM_SHORT_MODE	6
#define AK7759_LRIF_PCM_LONG_MODE	7
#endif

#define AK7759_SIN_NUM             2
#define AK7759_SDOUT_NUM           3


/* Input Format is set as "MSB(24- bit)" by following register.
   CONT03(DIF2,DOF2), CONT06(DIFDA, DIF1), CONT07(DOF4,DOF3,DOF1) */

/* AK7759_CA_CLK_SDOUT_SETTING (0xCA) Fields */

#define MAX_LOOP_TIMES		3

#define COMMAND_WRITE_REG			0xC0
#define COMMAND_READ_REG			0x40

#define COMMAND_CRC_READ			0x72
#define COMMAND_MIR_READ			0x76

#define COMMAND_WRITE_CRAM_RUN		0x80
#define COMMAND_WRITE_CRAM_EXEC     0xA4

#define COMMAND_WRITE_CRAM          0xB4
#define COMMAND_WRITE_PRAM          0xB8
#define COMMAND_WRITE_OFREG         0xB2

#define	AK7759_CRAM_MAX_ADDRESS		4095
#define	TOTAL_NUM_OF_PRAM_MAX		25603 // 5120*5+3
#define	TOTAL_NUM_OF_CRAM_MAX		12291 // 4096*3+3
#define	TOTAL_NUM_OF_OFREG_MAX		99    // 32*3+3

static const char *ak7759_firmware_pram[] =
{
    "Off",
    "basic",
    "data1",
    "data2",
    "data3",
    "data4",
    "data5",
};

static const char *ak7759_firmware_cram[] =
{
    "Off",
    "basic",
    "data1",
    "data2",
    "data3",
    "data4",
    "data5",
};

static const char *ak7759_firmware_ofreg[] =
{
    "Off",
    "basic",
    "data1",
    "data2",
    "data3",
    "data4",
    "data5",
};


enum {
	AIF_PORT1 = 0,
	NUM_AIF_DAIS,
};

#define RAMTYPE_MAX  6
#define MAX_DSP_NAME_SIZE  128
#define SOC_DAPM_VALUE_ENUM SOC_DAPM_ENUM

enum snd_soc_control_type {
	SND_SOC_SPI,
	SND_SOC_I2C
};

#endif

