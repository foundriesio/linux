/*
 * ak4601.c  --  audio driver for AK4601
 *
 * Copyright (C) 2016 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   Tsuyoshi Mutsuro   16/04/12	    1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/of_gpio.h>		//16/04/11

#include <sound/ak4601_pdata.h>

#include "ak4601.h"

//#define AK4601_DEBUG			//used at debug mode

#define PORT3_LINK	1			//Port Sel(1~2)


#define AIF1	"AIF1"
#define AIF2	"AIF2"

#if PORT3_LINK == 1
#define SDIO3_LINK	AIF1
#elif (PORT3_LINK == 2)
#define SDIO3_LINK	AIF2
#else
#define SDIO3_LINK	AIF1
#endif


#ifdef AK4601_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

#define TCC_CODEC_SAMPLING_FREQ

static const u8 ak4601_backup_reg_list[] = {
	AK4601_000_SYSTEM_CLOCK_SETTING_1			,
	AK4601_001_SYSTEM_CLOCK_SETTING_2			,
	AK4601_002_MIC_BIAS_POWER_MANAGEMENT		,
	AK4601_003_SYNC_DOMAIN_1_SETTING_1			,
	AK4601_004_SYNC_DOMAIN_1_SETTING_2			,
	AK4601_005_SYNC_DOMAIN_2_SETTING_1			,
	AK4601_006_SYNC_DOMAIN_2_SETTING_2			,
	AK4601_00F_CLKO_OUTPUT_SETTING				,
	AK4601_010_PIN_SETTING						,
	AK4601_011_SYNC_DOMAIN_SELECT_1				,
	AK4601_013_SYNC_DOMAIN_SELECT_3				,
	AK4601_014_SYNC_DOMAIN_SELECT_4				,
	AK4601_016_SYNC_DOMAIN_SELECT_6				,
	AK4601_017_SYNC_DOMAIN_SELECT_7				,
	AK4601_018_SYNC_DOMAIN_SELECT_8				,
	AK4601_019_SYNC_DOMAIN_SELECT_9				,
	AK4601_01F_SYNC_DOMAIN_SELECT_15			,
	AK4601_020_SYNC_DOMAIN_SELECT_16			,
	AK4601_021_SDOUT1_TDM_SLOT1_2_DATA_SELECT	,
	AK4601_022_SDOUT1_TDM_SLOT3_4_DATA_SELECT	,
	AK4601_023_SDOUT1_TDM_SLOT5_6_DATA_SELECT	,
	AK4601_024_SDOUT1_TDM_SLOT7_8_DATA_SELECT	,
	AK4601_025_SDOUT1_TDM_SLOT9_10_DATA_SELECT	,
	AK4601_026_SDOUT1_TDM_SLOT11_12_DATA_SELECT	,
	AK4601_027_SDOUT1_TDM_SLOT13_14_DATA_SELECT	,
	AK4601_028_SDOUT1_TDM_SLOT15_16_DATA_SELECT	,
	AK4601_029_SDOUT2_OUTPUT_DATA_SELECT		,
	AK4601_02A_SDOUT2_OUTPUT_DATA_SELECT		,
	AK4601_035_DAC1_INPUT_DATA_SELECT			,
	AK4601_036_DAC2_INPUT_DATA_SELECT			,
	AK4601_037_DAC3_INPUT_DATA_SELECT			,
	AK4601_038_VOL1_INPUT_DATA_SELECT			,
	AK4601_039_VOL2_INPUT_DATA_SELECT			,
	AK4601_03A_VOL3_INPUT_DATA_SELECT			,
	AK4601_045_MIXER_A_CH1_INPUT_DATA_SELECT	,
	AK4601_046_MIXER_A_CH2_INPUT_DATA_SELECT	,
	AK4601_047_MIXER_B_CH1_INPUT_DATA_SELECT	,
	AK4601_048_MIXER_B_CH2_INPUT_DATA_SELECT	,
	AK4601_04C_CLOCK_FORMAT_SETTING_1			,
	AK4601_050_SDIN1_DIGITAL_INPUT_FORMAT		,
	AK4601_051_SDIN2_DIGITAL_INPUT_FORMAT		,
	AK4601_052_SDIN3_DIGITAL_INPUT_FORMAT		,
	AK4601_055_SDOUT1_DIGITAL_OUTPUT_FORMAT		,
	AK4601_056_SDOUT2_DIGITAL_OUTPUT_FORMAT		,
	AK4601_057_SDOUT3_DIGITAL_OUTPUT_FORMAT		,
	AK4601_05A_SDOUT_PHASE_SETTING				,
	AK4601_05E_OUTPUT_PORT_ENABLE_SETTING		,
	AK4601_060_MIXER_A_SETTING					,
	AK4601_061_MIXER_B_SETTING					,
	AK4601_062_MIC_AMP_GAIN						,
	AK4601_063_ANALOG_INPUT_GAIN_CONTROL		,
	AK4601_064_ADC1LCH_DIGITAL_VOLUME			,
	AK4601_065_ADC1RCH_DIGITAL_VOLUME			,
	AK4601_066_ADC2LCH_DIGITAL_VOLUME			,
	AK4601_067_ADC2RCH_DIGITAL_VOLUME			,
	AK4601_068_ADCM_DIGITAL_VOLUME				,
	AK4601_06B_ANALOG_INPUT_SELECT_SETTING		,
	AK4601_06C_ADC_MUTE_AND_HPF_CONTROL			,
	AK4601_06D_DAC1LCH_DIGITAL_VOLUME			,
	AK4601_06E_DAC1RCH_DIGITAL_VOLUME			,
	AK4601_06F_DAC2LCH_DIGITAL_VOLUME			,
	AK4601_070_DAC2RCH_DIGITAL_VOLUME			,
	AK4601_071_DAC3LCH_DIGITAL_VOLUME			,
	AK4601_072_DAC3RCH_DIGITAL_VOLUME			,
	AK4601_073_DAC_MUTE_AND_FILTER_SETTING		,
	AK4601_074_DAC_DEM_SETTING					,
	AK4601_075_VOL1LCH_DIGITAL_VOLUME			,
	AK4601_076_VOL1RCH_DIGITAL_VOLUME			,
	AK4601_077_VOL2LCH_DIGITAL_VOLUME			,
	AK4601_078_VOL2RCH_DIGITAL_VOLUME			,
	AK4601_079_VOL3LCH_DIGITAL_VOLUME			,
	AK4601_07A_VOL3RCH_DIGITAL_VOLUME			,
	AK4601_07F_VOL_SETTING						,
	AK4601_083_STO_FLAG_SETTING1				,
	AK4601_08A_POWER_MANAGEMENT1				,
	AK4601_08C_RESET_CONTROL					,
};

/* AK4601 Codec Private Data */
struct ak4601_priv {
	struct snd_soc_codec *codec;
	struct spi_device *spi;
	struct i2c_client *i2c;
	int fs;
	
	int pdn_gpio;

    int PLLInput;  // 0 : XTI,   1 : BICK1,	2 : BICK2
	int XtiFs;     // 0 : 12.288MHz,  1: 18.432MHz,	 2 : 24.576MHz 
	
	int EXBCK[NUM_SDIO];		//0:Low 1:SD1 2:SD2
	int SDfs[NUM_SYNCDOMAIN];     // 0:8kHz, 1:12kHz, 2:16kHz, 3:24kHz, 4:32kHz, 5:48kHz, 6:96kHz, 7:192kHz
    int SDBick[NUM_SYNCDOMAIN];   // 0:64fs, 1:48fs, 2:32fs, 3:128fs, 4:256fs, 5:512fs

	int Master[NUM_SYNCDOMAIN];  // 0 : Slave Mode, 1 : Master
	int SDCks[NUM_SYNCDOMAIN];   // 0 : Low,  1 : PLLMCLK,	2 : XTI,	3~4 : BICK1~2(not used)

	int TDMSDINbit[NUM_SDIO];	// 0 : TDM off,	1 : TDM on
	int TDMSDOUTbit[NUM_SDIO];	// 0 : TDM off,	1 : TDM on

	int DIEDGEbit[NUM_SDIO];	//
	int DOEDGEbit[NUM_SDIO];	//
	int DISLbit[NUM_SDIO];		// 0:24bit 1:20bit 2:16bit 3:32bit
	int DOSLbit[NUM_SDIO];		// 0:24bit 1:20bit 2:16bit 3:32bit
	
	int DIDL3;

	uint8_t reg_backup[ARRAY_SIZE(ak4601_backup_reg_list)];

};

/* ak4601 register cache & default register settings */
static const u8 ak4601_reg[] = {
	0x00,	/*	AK4601_000_SYSTEM_CLOCK_SETTING_1			*/
	0x00,	/*	AK4601_001_SYSTEM_CLOCK_SETTING_2			*/
	0x00,	/*	AK4601_002_MIC_BIAS_POWER_MANAGEMENT		*/
	0x00,	/*	AK4601_003_SYNC_DOMAIN_1_SETTING_1			*/
	0x00,	/*	AK4601_004_SYNC_DOMAIN_1_SETTING_2			*/
	0x00,	/*	AK4601_005_SYNC_DOMAIN_2_SETTING_1			*/
	0x00,	/*	AK4601_006_SYNC_DOMAIN_2_SETTING_2			*/
	0x00,	/*	AK4601_007_RESERVED							*/
	0x00,	/*	AK4601_008_RESERVED							*/
	0x00,	/*	AK4601_009_RESERVED							*/
	0x00,	/*	AK4601_00A_RESERVED							*/
	0x00,	/*	AK4601_00B_RESERVED							*/
	0x00,	/*	AK4601_00C_RESERVED							*/
	0x00,	/*	AK4601_00D_RESERVED							*/
	0x00,	/*	AK4601_00E_RESERVED							*/
	0x00,	/*	AK4601_00F_CLKO_OUTPUT_SETTING				*/
	0x00,	/*	AK4601_010_PIN_SETTING						*/
	0x00,	/*	AK4601_011_SYNC_DOMAIN_SELECT_1				*/
	0x00,	/*	AK4601_012_RESERVED							*/
	0x00,	/*	AK4601_013_SYNC_DOMAIN_SELECT_3				*/
	0x00,	/*	AK4601_014_SYNC_DOMAIN_SELECT_4				*/
	0x00,	/*	AK4601_015_RESERVED							*/
	0x00,	/*	AK4601_016_SYNC_DOMAIN_SELECT_6				*/
	0x00,	/*	AK4601_017_SYNC_DOMAIN_SELECT_7				*/
	0x00,	/*	AK4601_018_SYNC_DOMAIN_SELECT_8				*/
	0x00,	/*	AK4601_019_SYNC_DOMAIN_SELECT_9				*/
	0x00,	/*	AK4601_01A_RESERVED							*/
	0x00,	/*	AK4601_01B_RESERVED							*/
	0x00,	/*	AK4601_01C_RESERVED							*/
	0x00,	/*	AK4601_01D_RESERVED							*/
	0x00,	/*	AK4601_01E_RESERVED							*/
	0x00,	/*	AK4601_01F_SYNC_DOMAIN_SELECT_15			*/
	0x00,	/*	AK4601_020_SYNC_DOMAIN_SELECT_16			*/
	0x00,	/*	AK4601_021_SDOUT1_TDM_SLOT1_2_DATA_SELECT	*/
	0x00,	/*	AK4601_022_SDOUT1_TDM_SLOT3_4_DATA_SELECT	*/
	0x00,	/*	AK4601_023_SDOUT1_TDM_SLOT5_6_DATA_SELECT	*/
	0x00,	/*	AK4601_024_SDOUT1_TDM_SLOT7_8_DATA_SELECT	*/
	0x00,	/*	AK4601_025_SDOUT1_TDM_SLOT9_10_DATA_SELECT	*/
	0x00,	/*	AK4601_026_SDOUT1_TDM_SLOT11_12_DATA_SELECT	*/
	0x00,	/*	AK4601_027_SDOUT1_TDM_SLOT13_14_DATA_SELECT	*/
	0x00,	/*	AK4601_028_SDOUT1_TDM_SLOT15_16_DATA_SELECT	*/
	0x00,	/*	AK4601_029_SDOUT2_OUTPUT_DATA_SELECT		*/
	0x00,	/*	AK4601_02A_SDOUT2_OUTPUT_DATA_SELECT		*/
	0x00,	/*	AK4601_02B_RESERVED							*/
	0x00,	/*	AK4601_02C_RESERVED							*/
	0x00,	/*	AK4601_02D_RESERVED							*/
	0x00,	/*	AK4601_02E_RESERVED							*/
	0x00,	/*	AK4601_02F_RESERVED							*/
	0x00,	/*	AK4601_030_RESERVED							*/
	0x00,	/*	AK4601_031_RESERVED							*/
	0x00,	/*	AK4601_032_RESERVED							*/
	0x00,	/*	AK4601_033_RESERVED							*/
	0x00,	/*	AK4601_034_RESERVED							*/
	0x00,	/*	AK4601_035_DAC1_INPUT_DATA_SELECT			*/
	0x00,	/*	AK4601_036_DAC2_INPUT_DATA_SELECT			*/
	0x00,	/*	AK4601_037_DAC3_INPUT_DATA_SELECT			*/
	0x00,	/*	AK4601_038_VOL1_INPUT_DATA_SELECT			*/
	0x00,	/*	AK4601_039_VOL2_INPUT_DATA_SELECT			*/
	0x00,	/*	AK4601_03A_VOL3_INPUT_DATA_SELECT			*/
	0x00,	/*	AK4601_03B_RESERVED							*/
	0x00,	/*	AK4601_03C_RESERVED							*/
	0x00,	/*	AK4601_03D_RESERVED							*/
	0x00,	/*	AK4601_03E_RESERVED							*/
	0x00,	/*	AK4601_03F_RESERVED							*/
	0x00,	/*	AK4601_040_RESERVED							*/
	0x00,	/*	AK4601_041_RESERVED							*/
	0x00,	/*	AK4601_042_RESERVED							*/
	0x00,	/*	AK4601_043_RESERVED							*/
	0x00,	/*	AK4601_044_RESERVED							*/
	0x00,	/*	AK4601_045_MIXER_A_CH1_INPUT_DATA_SELECT	*/
	0x00,	/*	AK4601_046_MIXER_A_CH2_INPUT_DATA_SELECT	*/
	0x00,	/*	AK4601_047_MIXER_B_CH1_INPUT_DATA_SELECT	*/
	0x00,	/*	AK4601_048_MIXER_B_CH2_INPUT_DATA_SELECT	*/
	0x00,	/*	AK4601_049_RESERVED							*/
	0x00,	/*	AK4601_04A_RESERVED							*/
	0x00,	/*	AK4601_04B_RESERVED							*/
	0x00,	/*	AK4601_04C_CLOCK_FORMAT_SETTING_1			*/
	0x00,	/*	AK4601_04D_RESERVED							*/
	0x00,	/*	AK4601_04E_RESERVED							*/
	0x00,	/*	AK4601_04F_RESERVED							*/
	0x00,	/*	AK4601_050_SDIN1_DIGITAL_INPUT_FORMAT		*/
	0x00,	/*	AK4601_051_SDIN2_DIGITAL_INPUT_FORMAT		*/
	0x00,	/*	AK4601_052_SDIN3_DIGITAL_INPUT_FORMAT		*/
	0x00,	/*	AK4601_053_RESERVED							*/
	0x00,	/*	AK4601_054_RESERVED							*/
	0x00,	/*	AK4601_055_SDOUT1_DIGITAL_OUTPUT_FORMAT		*/
	0x00,	/*	AK4601_056_SDOUT2_DIGITAL_OUTPUT_FORMAT		*/
	0x00,	/*	AK4601_057_SDOUT3_DIGITAL_OUTPUT_FORMAT		*/
	0x00,	/*	AK4601_058_RESERVED							*/
	0x00,	/*	AK4601_059_RESERVED							*/
	0x00,	/*	AK4601_05A_SDOUT_PHASE_SETTING				*/
	0x00,	/*	AK4601_05B_RESERVED							*/
	0x00,	/*	AK4601_05C_RESERVED							*/
	0x00,	/*	AK4601_05D_RESERVED							*/
	0x00,	/*	AK4601_05E_OUTPUT_PORT_ENABLE_SETTING		*/
	0x00,	/*	AK4601_05F_RESERVED							*/
	0x00,	/*	AK4601_060_MIXER_A_SETTING					*/
	0x00,	/*	AK4601_061_MIXER_B_SETTING					*/
	0x00,	/*	AK4601_062_MIC_AMP_GAIN						*/
	0x00,	/*	AK4601_063_ANALOG_INPUT_GAIN_CONTROL		*/
	0x30,	/*	AK4601_064_ADC1LCH_DIGITAL_VOLUME			*/
	0x30,	/*	AK4601_065_ADC1RCH_DIGITAL_VOLUME			*/
	0x30,	/*	AK4601_066_ADC2LCH_DIGITAL_VOLUME			*/
	0x30,	/*	AK4601_067_ADC2RCH_DIGITAL_VOLUME			*/
	0x30,	/*	AK4601_068_ADCM_DIGITAL_VOLUME				*/
	0x00,	/*	AK4601_069_RESERVED							*/
	0x00,	/*	AK4601_06A_RESERVED							*/
	0x00,	/*	AK4601_06B_ANALOG_INPUT_SELECT_SETTING		*/
	0x00,	/*	AK4601_06C_ADC_MUTE_AND_HPF_CONTROL			*/
	0x18,	/*	AK4601_06D_DAC1LCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_06E_DAC1RCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_06F_DAC2LCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_070_DAC2RCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_071_DAC3LCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_072_DAC3RCH_DIGITAL_VOLUME			*/
	0x02,	/*	AK4601_073_DAC_MUTE_AND_FILTER_SETTING		*/
	0x15,	/*	AK4601_074_DAC_DEM_SETTING					*/
	0x18,	/*	AK4601_075_VOL1LCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_076_VOL1RCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_077_VOL2LCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_078_VOL2RCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_079_VOL3LCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_07A_VOL3RCH_DIGITAL_VOLUME			*/
	0x18,	/*	AK4601_07B_RESERVED							*/
	0x18,	/*	AK4601_07C_RESERVED							*/
	0x18,	/*	AK4601_07D_RESERVED							*/
	0x18,	/*	AK4601_07E_RESERVED							*/
	0x00,	/*	AK4601_07F_VOL_SETTING						*/
	0x00,	/*	AK4601_080_RESERVED							*/
	0x00,	/*	AK4601_081_RESERVED							*/
	0x00,	/*	AK4601_082_RESERVED							*/
	0x00,	/*	AK4601_083_STO_FLAG_SETTING1				*/
	0x00,	/*	AK4601_084_RESERVED							*/
	0x00,	/*	AK4601_085_RESERVED							*/
	0x04,	/*	AK4601_086_RESERVED							*/
	0x02,	/*	AK4601_087_RESERVED							*/
	0x00,	/*	AK4601_088_RESERVED							*/
	0x00,	/*	AK4601_089_RESERVED							*/
	0x00,	/*	AK4601_08A_POWER_MANAGEMENT1				*/
	0x00,	/*	AK4601_08B_RESERVED							*/
	0x20,	/*	AK4601_08C_RESET_CONTROL					*/
	0x00,	/*	AK4601_08D_RESERVED							*/
	0x00,	/*	AK4601_08E_RESERVED							*/
	0x00,	/*	AK4601_08F_RESERVED							*/
	0x00,	/*	AK4601_090_RESERVED							*/
	0x00,	/*	AK4601_091_RESERVED							*/
	0x00,	/*	AK4601_092_RESERVED							*/
	0x00,	/*	AK4601_093_RESERVED							*/
	0x00,	/*	AK4601_094_RESERVED							*/
	0x00,	/*	AK4601_095_RESERVED							*/
	0x00,	/*	AK4601_096_RESERVED							*/
	0x00,	/*	AK4601_097_RESERVED							*/
	0x00,	/*	AK4601_098_RESERVED							*/
	0x00,	/*	AK4601_099_RESERVED							*/
	0x00,	/*	AK4601_09A_RESERVED							*/
	0x00,	/*	AK4601_09B_RESERVED							*/
	0x00,	/*	AK4601_09C_RESERVED							*/
	0x00,	/*	AK4601_09D_RESERVED							*/
	0x00,	/*	AK4601_09E_RESERVED							*/
	0x00,	/*	AK4601_09F_RESERVED							*/
	0x00,	/*	AK4601_0A0_RESERVED							*/
	0x00,	/*	AK4601_0A1_RESERVED							*/
	0x00,	/*	AK4601_0A2_RESERVED							*/
	0x00,	/*	AK4601_0A3_RESERVED							*/
	0x00,	/*	AK4601_0A4_RESERVED							*/
	0x00,	/*	AK4601_0A5_RESERVED							*/
	0x00,	/*	AK4601_0A6_RESERVED							*/
	0x00,	/*	AK4601_0A7_RESERVED							*/
	0x00,	/*	AK4601_0A8_RESERVED							*/
	0x00,	/*	AK4601_0A9_RESERVED							*/
	0x00,	/*	AK4601_0AA_RESERVED							*/
	0x00,	/*	AK4601_0AB_RESERVED							*/
	0x00,	/*	AK4601_0AC_RESERVED							*/
	0x00,	/*	AK4601_0AD_RESERVED							*/
	0x00,	/*	AK4601_0AE_RESERVED							*/
	0x00,	/*	AK4601_0AF_RESERVED							*/
	0x00,	/*	AK4601_0B0_RESERVED							*/
	0x00,	/*	AK4601_0B1_RESERVED							*/
	0x00,	/*	AK4601_0B2_RESERVED							*/
	0x00,	/*	AK4601_0B3_RESERVED							*/
	0x00,	/*	AK4601_0B4_RESERVED							*/
	0x00,	/*	AK4601_0B5_RESERVED							*/
	0x00,	/*	AK4601_0B6_RESERVED							*/
	0x00,	/*	AK4601_0B7_RESERVED							*/
	0x00,	/*	AK4601_0B8_RESERVED							*/
	0x00,	/*	AK4601_0B9_RESERVED							*/
	0x00,	/*	AK4601_0BA_RESERVED							*/
	0x00,	/*	AK4601_0BB_RESERVED							*/
	0x00,	/*	AK4601_0BC_RESERVED							*/
	0x00,	/*	AK4601_0BD_RESERVED							*/
	0x00,	/*	AK4601_0BE_RESERVED							*/
	0x00,	/*	AK4601_0BF_RESERVED							*/
	0x00,	/*	AK4601_0C0_RESERVED							*/
	0x00,	/*	AK4601_0C1_RESERVED							*/
	0x00,	/*	AK4601_0C2_RESERVED							*/
	0x00,	/*	AK4601_0C3_RESERVED							*/
	0x00,	/*	AK4601_0C4_RESERVED							*/
	0x00,	/*	AK4601_0C5_RESERVED							*/
	0x00,	/*	AK4601_0C6_RESERVED							*/
	0x00,	/*	AK4601_0C7_RESERVED							*/
	0x00,	/*	AK4601_0C8_RESERVED							*/
	0x00,	/*	AK4601_0C9_RESERVED							*/
	0x00,	/*	AK4601_0CA_RESERVED							*/
	0x00,	/*	AK4601_0CB_RESERVED							*/
	0x00,	/*	AK4601_0CC_RESERVED							*/
	0x00,	/*	AK4601_0CD_RESERVED							*/
	0x00,	/*	AK4601_0CE_RESERVED							*/
	0x00,	/*	AK4601_0CF_RESERVED							*/
	0x00,	/*	AK4601_0D0_RESERVED							*/
	0x00,	/*	AK4601_0D1_RESERVED							*/
	0x00,	/*	AK4601_0D2_RESERVED							*/
	0x00,	/*	AK4601_0D3_RESERVED							*/
	0x00,	/*	AK4601_0D4_RESERVED							*/
	0x00,	/*	AK4601_0D5_RESERVED							*/
	0x00,	/*	AK4601_0D6_RESERVED							*/
	0x00,	/*	AK4601_0D7_RESERVED							*/
	0x00,	/*	AK4601_0D8_RESERVED							*/
	0x00,	/*	AK4601_0D9_RESERVED							*/
	0x00,	/*	AK4601_0DA_RESERVED							*/
	0x00,	/*	AK4601_0DB_RESERVED							*/
	0x00,	/*	AK4601_0DC_RESERVED							*/
	0x00,	/*	AK4601_0DD_RESERVED							*/
	0x00,	/*	AK4601_0DE_RESERVED							*/
	0x00,	/*	AK4601_0DF_RESERVED							*/
	0x00,	/*	AK4601_0E0_RESERVED							*/
	0x00,	/*	AK4601_0E1_RESERVED							*/
	0x00,	/*	AK4601_0E2_RESERVED							*/
	0x00,	/*	AK4601_0E3_RESERVED							*/
	0x00,	/*	AK4601_0E4_RESERVED							*/
	0x00,	/*	AK4601_0E5_RESERVED							*/
	0x00,	/*	AK4601_0E6_RESERVED							*/
	0x00,	/*	AK4601_0E7_RESERVED							*/
	0x00,	/*	AK4601_0E8_RESERVED							*/
	0x00,	/*	AK4601_0E9_RESERVED							*/
	0x00,	/*	AK4601_0EA_RESERVED							*/
	0x00,	/*	AK4601_0EB_RESERVED							*/
	0x00,	/*	AK4601_0EC_RESERVED							*/
	0x00,	/*	AK4601_0ED_RESERVED							*/
	0x00,	/*	AK4601_0EE_RESERVED							*/
	0x00,	/*	AK4601_0EF_RESERVED							*/
	0x00,	/*	AK4601_0F0_RESERVED							*/
	0x00,	/*	AK4601_0F1_RESERVED							*/
	0x00,	/*	AK4601_0F2_RESERVED							*/
	0x00,	/*	AK4601_0F3_RESERVED							*/
	0x00,	/*	AK4601_0F4_RESERVED							*/
	0x00,	/*	AK4601_0F5_RESERVED							*/
	0x00,	/*	AK4601_0F6_RESERVED							*/
	0x00,	/*	AK4601_0F7_RESERVED							*/
	0x00,	/*	AK4601_0F8_RESERVED							*/
	0x00,	/*	AK4601_0F9_RESERVED							*/
	0x00,	/*	AK4601_0FA_RESERVED							*/
	0x00,	/*	AK4601_0FB_RESERVED							*/
	0x00,	/*	AK4601_0FC_RESERVED							*/
	0x00,	/*	AK4601_0FD_RESERVED							*/
	0x00,	/*	AK4601_0FE_RESERVED							*/
	0x00,	/*	AK4601_0FF_RESERVED							*/
	0x00,	/*	AK4601_100_RESERVED							*/
	0x00,	/*	AK4601_101_RESERVED							*/
	0x40,	/*	AK4601_102_STATUS_READ_OUT					*/
};

static const struct {
	int readable;   /* Mask of readable bits */
	int writable;   /* Mask of writable bits */
} ak4601_access_masks[] = {
	{ 0xFF, 0xFF },  // 0x000
	{ 0x9F, 0x9F },  // 0x001
	{ 0xC3, 0xC3 },  // 0x002
	{ 0xFF, 0xFF },  // 0x003
	{ 0xFF, 0xFF },  // 0x004
	{ 0xFF, 0xFF },  // 0x005
	{ 0xFF, 0xFF },  // 0x006
	{ 0x00, 0x00 },  // 0x007
	{ 0x00, 0x00 },  // 0x008
	{ 0x00, 0x00 },  // 0x009
	{ 0x00, 0x00 },  // 0x00A
	{ 0x00, 0x00 },  // 0x00B
	{ 0x00, 0x00 },  // 0x00C
	{ 0x00, 0x00 },  // 0x00D
	{ 0x00, 0x00 },  // 0x00E
	{ 0x0F, 0x0F },  // 0x00F
	{ 0x01, 0x01 },  // 0x010
	{ 0x77, 0x77 },  // 0x011
	{ 0x00, 0x00 },  // 0x012
	{ 0x07, 0x07 },  // 0x013
	{ 0x77, 0x77 },  // 0x014
	{ 0x00, 0x00 },  // 0x015
	{ 0x77, 0x77 },  // 0x016
	{ 0x70, 0x70 },  // 0x017
	{ 0x07, 0x07 },  // 0x018
	{ 0x77, 0x77 },  // 0x019
	{ 0x00, 0x00 },  // 0x01A
	{ 0x00, 0x00 },  // 0x01B
	{ 0x00, 0x00 },  // 0x01C
	{ 0x00, 0x00 },  // 0x01D
	{ 0x00, 0x00 },  // 0x01E
	{ 0x77, 0x77 },  // 0x01F
	{ 0x77, 0x77 },  // 0x020
	{ 0x3F, 0x3F },  // 0x021
	{ 0x3F, 0x3F },  // 0x022
	{ 0x3F, 0x3F },  // 0x023
	{ 0x3F, 0x3F },  // 0x024
	{ 0x3F, 0x3F },  // 0x025
	{ 0x3F, 0x3F },  // 0x026
	{ 0x3F, 0x3F },  // 0x027
	{ 0x3F, 0x3F },  // 0x028
	{ 0x3F, 0x3F },  // 0x029
	{ 0x3F, 0x3F },  // 0x02A
	{ 0x00, 0x00 },  // 0x02B
	{ 0x00, 0x00 },  // 0x02C
	{ 0x00, 0x00 },  // 0x02D
	{ 0x00, 0x00 },  // 0x02E
	{ 0x00, 0x00 },  // 0x02F
	{ 0x00, 0x00 },  // 0x030
	{ 0x00, 0x00 },  // 0x031
	{ 0x00, 0x00 },  // 0x032
	{ 0x00, 0x00 },  // 0x033
	{ 0x00, 0x00 },  // 0x034
	{ 0x3F, 0x3F },  // 0x035
	{ 0x3F, 0x3F },  // 0x036
	{ 0x3F, 0x3F },  // 0x037
	{ 0x3F, 0x3F },  // 0x038
	{ 0x3F, 0x3F },  // 0x039
	{ 0x3F, 0x3F },  // 0x03A
	{ 0x00, 0x00 },  // 0x03B
	{ 0x00, 0x00 },  // 0x03C
	{ 0x00, 0x00 },  // 0x03D
	{ 0x00, 0x00 },  // 0x03E
	{ 0x00, 0x00 },  // 0x03F
	{ 0x00, 0x00 },  // 0x040
	{ 0x00, 0x00 },  // 0x041
	{ 0x00, 0x00 },  // 0x042
	{ 0x00, 0x00 },  // 0x043
	{ 0x00, 0x00 },  // 0x044
	{ 0x3F, 0x3F },  // 0x045
	{ 0x3F, 0x3F },  // 0x046
	{ 0x3F, 0x3F },  // 0x047
	{ 0x3F, 0x3F },  // 0x048
	{ 0x00, 0x00 },  // 0x049
	{ 0x00, 0x00 },  // 0x04A
	{ 0x00, 0x00 },  // 0x04B
	{ 0xFF, 0xFF },  // 0x04C
	{ 0x00, 0x00 },  // 0x04D
	{ 0x00, 0x00 },  // 0x04E
	{ 0x00, 0x00 },  // 0x04F
	{ 0xBB, 0xBB },  // 0x050
	{ 0xBB, 0xBB },  // 0x051
	{ 0xBB, 0xBB },  // 0x052
	{ 0x00, 0x00 },  // 0x053
	{ 0x00, 0x00 },  // 0x054
	{ 0xBB, 0xBB },  // 0x055
	{ 0xBB, 0xBB },  // 0x056
	{ 0xBB, 0xBB },  // 0x057
	{ 0x00, 0x00 },  // 0x058
	{ 0x00, 0x00 },  // 0x059
	{ 0x07, 0x07 },  // 0x05A
	{ 0x00, 0x00 },  // 0x05B
	{ 0x00, 0x00 },  // 0x05C
	{ 0x00, 0x00 },  // 0x05D
	{ 0x38, 0x38 },  // 0x05E
	{ 0x00, 0x00 },  // 0x05F
	{ 0xFF, 0xFF },  // 0x060
	{ 0xFF, 0xFF },  // 0x061
	{ 0xFF, 0xFF },  // 0x062
	{ 0xFB, 0xFB },  // 0x063
	{ 0xFF, 0xFF },  // 0x064
	{ 0xFF, 0xFF },  // 0x065
	{ 0xFF, 0xFF },  // 0x066
	{ 0xFF, 0xFF },  // 0x067
	{ 0xFF, 0xFF },  // 0x068
	{ 0x00, 0x00 },  // 0x069
	{ 0x00, 0x00 },  // 0x06A
	{ 0xDF, 0xDF },  // 0x06B
	{ 0xF7, 0xF7 },  // 0x06C
	{ 0xFF, 0xFF },  // 0x06D
	{ 0xFF, 0xFF },  // 0x06E
	{ 0xFF, 0xFF },  // 0x06F
	{ 0xFF, 0xFF },  // 0x070
	{ 0xFF, 0xFF },  // 0x071
	{ 0xFF, 0xFF },  // 0x072
	{ 0xF7, 0xF7 },  // 0x073
	{ 0x3F, 0x3F },  // 0x074
	{ 0xFF, 0xFF },  // 0x075
	{ 0xFF, 0xFF },  // 0x076
	{ 0xFF, 0xFF },  // 0x077
	{ 0xFF, 0xFF },  // 0x078
	{ 0xFF, 0xFF },  // 0x079
	{ 0xFF, 0xFF },  // 0x07A
	{ 0x00, 0x00 },  // 0x07B
	{ 0x00, 0x00 },  // 0x07C
	{ 0x00, 0x00 },  // 0x07D
	{ 0x00, 0x00 },  // 0x07E
	{ 0x80, 0x80 },  // 0x07F
	{ 0x00, 0x00 },  // 0x080
	{ 0x00, 0x00 },  // 0x081
	{ 0x00, 0x00 },  // 0x082
	{ 0x40, 0x40 },  // 0x083
	{ 0x00, 0x00 },  // 0x084
	{ 0x00, 0x00 },  // 0x085
	{ 0x00, 0x00 },  // 0x086
	{ 0x00, 0x00 },  // 0x087
	{ 0x00, 0x00 },  // 0x088
	{ 0x00, 0x00 },  // 0x089
	{ 0x3F, 0x3F },  // 0x08A
	{ 0x00, 0x00 },  // 0x08B
	{ 0x11, 0x11 },  // 0x08C
	{ 0x00, 0x00 },  // 0x08D
	{ 0x00, 0x00 },  // 0x08E
	{ 0x00, 0x00 },  // 0x08F
	{ 0x00, 0x00 },  // 0x090
	{ 0x00, 0x00 },  // 0x091
	{ 0x00, 0x00 },  // 0x092
	{ 0x00, 0x00 },  // 0x093
	{ 0x00, 0x00 },  // 0x094
	{ 0x00, 0x00 },  // 0x095
	{ 0x00, 0x00 },  // 0x096
	{ 0x00, 0x00 },  // 0x097
	{ 0x00, 0x00 },  // 0x098
	{ 0x00, 0x00 },  // 0x099
	{ 0x00, 0x00 },  // 0x09A
	{ 0x00, 0x00 },  // 0x09B
	{ 0x00, 0x00 },  // 0x09C
	{ 0x00, 0x00 },  // 0x09D
	{ 0x00, 0x00 },  // 0x09E
	{ 0x00, 0x00 },  // 0x09F
	{ 0x00, 0x00 },  // 0x0A0
	{ 0x00, 0x00 },  // 0x0A1
	{ 0x00, 0x00 },  // 0x0A2
	{ 0x00, 0x00 },  // 0x0A3
	{ 0x00, 0x00 },  // 0x0A4
	{ 0x00, 0x00 },  // 0x0A5
	{ 0x00, 0x00 },  // 0x0A6
	{ 0x00, 0x00 },  // 0x0A7
	{ 0x00, 0x00 },  // 0x0A8
	{ 0x00, 0x00 },  // 0x0A9
	{ 0x00, 0x00 },  // 0x0AA
	{ 0x00, 0x00 },  // 0x0AB
	{ 0x00, 0x00 },  // 0x0AC
	{ 0x00, 0x00 },  // 0x0AD
	{ 0x00, 0x00 },  // 0x0AE
	{ 0x00, 0x00 },  // 0x0AF
	{ 0x00, 0x00 },  // 0x0B0
	{ 0x00, 0x00 },  // 0x0B1
	{ 0x00, 0x00 },  // 0x0B2
	{ 0x00, 0x00 },  // 0x0B3
	{ 0x00, 0x00 },  // 0x0B4
	{ 0x00, 0x00 },  // 0x0B5
	{ 0x00, 0x00 },  // 0x0B6
	{ 0x00, 0x00 },  // 0x0B7
	{ 0x00, 0x00 },  // 0x0B8
	{ 0x00, 0x00 },  // 0x0B9
	{ 0x00, 0x00 },  // 0x0BA
	{ 0x00, 0x00 },  // 0x0BB
	{ 0x00, 0x00 },  // 0x0BC
	{ 0x00, 0x00 },  // 0x0BD
	{ 0x00, 0x00 },  // 0x0BE
	{ 0x00, 0x00 },  // 0x0BF
	{ 0x00, 0x00 },  // 0x0C0
	{ 0x00, 0x00 },  // 0x0C1
	{ 0x00, 0x00 },  // 0x0C2
	{ 0x00, 0x00 },  // 0x0C3
	{ 0x00, 0x00 },  // 0x0C4
	{ 0x00, 0x00 },  // 0x0C5
	{ 0x00, 0x00 },  // 0x0C6
	{ 0x00, 0x00 },  // 0x0C7
	{ 0x00, 0x00 },  // 0x0C8
	{ 0x00, 0x00 },  // 0x0C9
	{ 0x00, 0x00 },  // 0x0CA
	{ 0x00, 0x00 },  // 0x0CB
	{ 0x00, 0x00 },  // 0x0CC
	{ 0x00, 0x00 },  // 0x0CD
	{ 0x00, 0x00 },  // 0x0CE
	{ 0x00, 0x00 },  // 0x0CF
	{ 0x00, 0x00 },  // 0x0D0
	{ 0x00, 0x00 },  // 0x0D1
	{ 0x00, 0x00 },  // 0x0D2
	{ 0x00, 0x00 },  // 0x0D3
	{ 0x00, 0x00 },  // 0x0D4
	{ 0x00, 0x00 },  // 0x0D5
	{ 0x00, 0x00 },  // 0x0D6
	{ 0x00, 0x00 },  // 0x0D7
	{ 0x00, 0x00 },  // 0x0D8
	{ 0x00, 0x00 },  // 0x0D9
	{ 0x00, 0x00 },  // 0x0DA
	{ 0x00, 0x00 },  // 0x0DB
	{ 0x00, 0x00 },  // 0x0DC
	{ 0x00, 0x00 },  // 0x0DD
	{ 0x00, 0x00 },  // 0x0DE
	{ 0x00, 0x00 },  // 0x0DF
	{ 0x00, 0x00 },  // 0x0E0
	{ 0x00, 0x00 },  // 0x0E1
	{ 0x00, 0x00 },  // 0x0E2
	{ 0x00, 0x00 },  // 0x0E3
	{ 0x00, 0x00 },  // 0x0E4
	{ 0x00, 0x00 },  // 0x0E5
	{ 0x00, 0x00 },  // 0x0E6
	{ 0x00, 0x00 },  // 0x0E7
	{ 0x00, 0x00 },  // 0x0E8
	{ 0x00, 0x00 },  // 0x0E9
	{ 0x00, 0x00 },  // 0x0EA
	{ 0x00, 0x00 },  // 0x0EB
	{ 0x00, 0x00 },  // 0x0EC
	{ 0x00, 0x00 },  // 0x0ED
	{ 0x00, 0x00 },  // 0x0EE
	{ 0x00, 0x00 },  // 0x0EF
	{ 0x00, 0x00 },  // 0x0F0
	{ 0x00, 0x00 },  // 0x0F1
	{ 0x00, 0x00 },  // 0x0F2
	{ 0x00, 0x00 },  // 0x0F3
	{ 0x00, 0x00 },  // 0x0F4
	{ 0x00, 0x00 },  // 0x0F5
	{ 0x00, 0x00 },  // 0x0F6
	{ 0x00, 0x00 },  // 0x0F7
	{ 0x00, 0x00 },  // 0x0F8
	{ 0x00, 0x00 },  // 0x0F9
	{ 0x00, 0x00 },  // 0x0FA
	{ 0x00, 0x00 },  // 0x0FB
	{ 0x00, 0x00 },  // 0x0FC
	{ 0x00, 0x00 },  // 0x0FD
	{ 0x00, 0x00 },  // 0x0FE
	{ 0x00, 0x00 },  // 0x0FF
	{ 0x00, 0x00 },  // 0x100
	{ 0x00, 0x00 },  // 0x101
	{ 0x40, 0x40 },	 // 0x102
};

/* MIC Input Volume control:
 * from 0 to 36 dB (quantity of each step is various) */
static DECLARE_TLV_DB_MINMAX(mgnl_tlv, 0, 3600);
static DECLARE_TLV_DB_MINMAX(mgnr_tlv, 0, 3600);

/* ADC, ADC2 Digital Volume control:
 * from -103.5 to 24 dB in 0.5 dB steps (mute instead of -103.5 dB) */
static DECLARE_TLV_DB_SCALE(voladc_tlv, -10350, 50, 0);

/* DAC Digital Volume control:
 * from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB) */
static DECLARE_TLV_DB_SCALE(voldac_tlv, -11550, 50, 0);

/* VOL Digital Volume control:
 * from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB) */
static DECLARE_TLV_DB_SCALE(volvol_tlv, -11550, 50, 0);


static const char *sdselbit_texts[] = {
	"Low", "SD1", "SD2",
};

static const char *sdin_clock_pin_texts[] = {
	"Low", "SD1", "SD2",
};

static const struct soc_enum ak4601_sdsel_enum[] = {
	SOC_ENUM_SINGLE(AK4601_016_SYNC_DOMAIN_SELECT_6, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK4601_016_SYNC_DOMAIN_SELECT_6, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),
	SOC_ENUM_SINGLE(AK4601_017_SYNC_DOMAIN_SELECT_7, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),

	SOC_ENUM_SINGLE(AK4601_018_SYNC_DOMAIN_SELECT_8, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//VOL1
	SOC_ENUM_SINGLE(AK4601_019_SYNC_DOMAIN_SELECT_9, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//VOL2
	SOC_ENUM_SINGLE(AK4601_019_SYNC_DOMAIN_SELECT_9, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//VOL3
	SOC_ENUM_SINGLE(AK4601_01F_SYNC_DOMAIN_SELECT_15, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//MixerA
	SOC_ENUM_SINGLE(AK4601_01F_SYNC_DOMAIN_SELECT_15, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//MixerB
	SOC_ENUM_SINGLE(AK4601_020_SYNC_DOMAIN_SELECT_16, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//ADC
	SOC_ENUM_SINGLE(AK4601_020_SYNC_DOMAIN_SELECT_16, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),//CODEC
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sdin_clock_pin_texts), sdin_clock_pin_texts),
};

// ak4601_set_dai_fmt() takes priority
static const char *msnbit_texts[] = {
	"Slave", "Master",
};


static const struct soc_enum ak4601_msnbit_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(msnbit_texts), msnbit_texts),
};


static int setSDMaster(struct snd_soc_codec *codec,int nSDNo,int nMaster);


static int get_sd1_ms(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->Master[0];

    return 0;
}

static int set_sd1_ms(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 0, currMode);

    return 0;
}

static int get_sd2_ms(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->Master[1];

    return 0;
}

static int set_sd2_ms(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 1, currMode);

    return 0;
}

#ifdef TCC_CODEC_SAMPLING_FREQ
static unsigned int codec_fsmode_table[] = { // single port case
	0x00, /*8kHz:8kHz */
	0x01, /*12kHz:12kHz */
	0x02, /*16kHz:16kHz */
	0x03, /*24kHz:24kHz */
	0x04, /*32kHz:32kHz */
	0x07, /*48kHz:48kHz */
	0x0B, /*96kHz:96kHz */
	0x11, /*192kHz:192kHz */
};
#endif

static const char *sd_fs_texts[] = {
	"8kHz", "12kHz",  "16kHz", "24kHz", 
    "32kHz", "48kHz", "96kHz", "192kHz",
};

static int sdfstab[] = {
	8000, 12000, 16000, 24000, 
    32000, 48000, 96000, 192000, 
};

static const char *sd_bick_texts[] = {
	"64fs", "48fs", "32fs",  "128fs", "256fs", "512fs",
};

static int sdbicktab[] = {
	64, 48, 32, 128, 256, 512,
};

static const struct soc_enum ak4601_sd_fs_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_fs_texts), sd_fs_texts), 
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_bick_texts), sd_bick_texts), 
};

static const char *fsmodebit_texts[] = {
	"8kHz:8kHz", "12kHz:12kHz", "16kHz:16kHz", "24kHz:24kHz",
	"32kHz:32kHz", "32kHz:16kHz", "32kHz:8kHz", "48kHz:48kHz",
	"48kHz:24kHz", "48kHz:16kHz", "48kHz:8kHz", "96kHz:96kHz",
	"96kHz:48kHz", "96kHz:32kHz", "96kHz:24kHz", "96kHz:16kHz",
	"96kHz:8kHz", "192kHz:192kHz", "192kHz:96kHz", "192kHz:48kHz",
	"192kHz:32kHz", "192kHz:16kHz",
};

static const struct soc_enum ak4601_fsmode_enum[] = {
	SOC_ENUM_SINGLE(AK4601_001_SYSTEM_CLOCK_SETTING_2, 0, ARRAY_SIZE(fsmodebit_texts), fsmodebit_texts),
};


static int XtiFsTab[] = {
	12288000,  18432000, 24576000,
};

static int PLLInFsTab[] = {
	256000, 384000, 512000, 768000, 1024000, 
    1152000, 1536000, 2048000, 2304000, 3072000, 
    4096000, 4608000, 6144000, 8192000, 9216000, 
    12288000, 18432000, 24576000
};

static int setPLLOut(
struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int nPLLInFs;
	int value, nBfs, fs;
	int n, nMax;

	if( ak4601->PLLInput > 2 ){// 0=Low 1=MCKI
		akdbgprt("[AK4601] %s Invalid PLL Input! (%d)\n",__FUNCTION__, ak4601->PLLInput);
		return(-EINVAL);
	}
	value = ( ak4601->PLLInput << 5 );
	snd_soc_update_bits(codec, AK4601_000_SYSTEM_CLOCK_SETTING_1, 0xE0, value);

	if ( ak4601->PLLInput ==  0) {//XTI
		nPLLInFs = XtiFsTab[ak4601->XtiFs];
	}
	else {
		nBfs = sdbicktab[ak4601->SDBick[ak4601->PLLInput-1]];
		fs = sdfstab[ak4601->SDfs[ak4601->PLLInput-1]];
		akdbgprt("[AK4601] %s nBfs=%d fs=%d\n",__FUNCTION__, nBfs, fs);
		nPLLInFs = nBfs * fs;
		if ( ( fs % 4000 ) != 0 ) {
			nPLLInFs *= 441;
			nPLLInFs /= 480;
		}
	}

	n = 0;
	nMax = sizeof(PLLInFsTab) / sizeof(PLLInFsTab[0]);

	do {
		akdbgprt("[AK4601] %s n=%d PLL:%d %d\n",__FUNCTION__, n, nPLLInFs, PLLInFsTab[n]);
		if ( nPLLInFs == PLLInFsTab[n] ) break;
		n++;
	} while ( n < nMax);


	if ( n == nMax ) {
		pr_err("%s: PLL1 setting Error!\n", __func__);
		return(-EINVAL);
	}

	snd_soc_update_bits(codec, AK4601_000_SYSTEM_CLOCK_SETTING_1, 0x1F, n);

	return(0);

}

static int setSDMaster(struct snd_soc_codec *codec,int nSDNo,int nMaster)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int value;

	akdbgprt("\t[AK4601] %s  nSDNo=%d\n",__FUNCTION__, nSDNo);

	if ( nSDNo > NUM_SYNCDOMAIN ) return(0);

	ak4601->Master[nSDNo] = nMaster;
	value = (ak4601->Master[nSDNo] << 7) + (ak4601->SDCks[nSDNo] << 4);
	snd_soc_update_bits(codec, AK4601_003_SYNC_DOMAIN_1_SETTING_1 + (2 * nSDNo), 0xF0, value);

	return(0);
}

#ifdef TCC_CODEC_SAMPLING_FREQ
static int setCodecFSMode(struct snd_soc_codec *codec,int nSDNo)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	unsigned int value;

	value = codec_fsmode_table[ak4601->SDfs[nSDNo]];

	snd_soc_update_bits(codec, AK4601_001_SYSTEM_CLOCK_SETTING_2, 0x1f, value);

	return 0;
}
#endif

static int setSDClock(struct snd_soc_codec *codec,int nSDNo)
{

	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int addr;
	int fs, bickfs;
	int sdv, bdv;

		if ( nSDNo >= NUM_SYNCDOMAIN ) return(0);

		fs = sdfstab[ak4601->SDfs[nSDNo]];
		bickfs = sdbicktab[ak4601->SDBick[nSDNo]] * fs;

		addr = AK4601_003_SYNC_DOMAIN_1_SETTING_1 + ( 2 * nSDNo );

		if ( ak4601->SDCks[nSDNo] == 2 ) {//MCKI
			bdv = XtiFsTab[ak4601->XtiFs] / bickfs;
		}else if( ak4601->SDCks[nSDNo] == 1){ //PLLMCLK
			bdv = 122880000 / bickfs;
		}else{
			return 0;
		}

		sdv = ak4601->SDBick[nSDNo];
		if(sdv == 5) sdv += 2;	// fs512

		akdbgprt("\t[AK4601] %s, fs = %d BICKfs = %d BDV=%d, SDV=%d\n",__FUNCTION__,fs, bickfs, bdv, sdbicktab[sdv]);
		bdv--;
		if ( bdv > 511) {
			pr_err("%s: BDV Error! SD No = %d, bdv bit = %d\n", __func__, nSDNo, bdv);
			bdv = 511;
		}

		akdbgprt("\tsdv bits=%x\n", sdv);
		snd_soc_update_bits(codec, addr, 0x0F, (sdv | ((bdv & 0x100) >> 5) ));// BDV[8],SDV[2:0]

		addr++;
	
		snd_soc_update_bits(codec, addr, 0xFF, ( bdv & 0xFF)); //BDIV[7:0]

		if ( ak4601->PLLInput == (nSDNo + 1) ) {
			setPLLOut(codec);
		}
		return(0);
}


static int get_sd1_fs(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->SDfs[0];

    return 0;
}

static int set_sd1_fs(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
		if ( ak4601->SDfs[0] != currMode ) {
			ak4601->SDfs[0] = currMode;
			setSDClock(codec, 0);
			setSDMaster(codec, 0, ak4601->Master[0]);
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;

}

static int get_sd2_fs(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->SDfs[1];

    return 0;
}

static int set_sd2_fs(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
		if ( ak4601->SDfs[1] != currMode ) {
			ak4601->SDfs[1] = currMode;
			setSDClock(codec, 1);
			setSDMaster(codec, 1, ak4601->Master[1]);
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;

}

static int get_sd1_bick(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->SDBick[0];

    return 0;
}

static int set_sd1_bick(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 6){
		if ( ak4601->SDBick[0] != currMode ) {
			ak4601->SDBick[0] = currMode;
			setSDClock(codec, 0);
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sd2_bick(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->SDBick[1];

    return 0;
}

static int set_sd2_bick(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 6){
		if ( ak4601->SDBick[1] != currMode ) {
			ak4601->SDBick[1] = currMode;
			setSDClock(codec, 1);
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static const char *pllinput_texts[]  = {
//    0      1         2
	"XTI", "BICK1" , "BICK2",
};

static const char *xtifs_texts[]  = {
	"12.288MHz", "18.432MHz" , "24.576MHz"
};

static const struct soc_enum ak4601_pllset_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllinput_texts), pllinput_texts), 
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(xtifs_texts), xtifs_texts), 
};

static int get_pllinput(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	akdbgprt(" [AK4601] %s ak4601->PLLInput=%d\n",__FUNCTION__, ak4601->PLLInput );
    ucontrol->value.enumerated.item[0] = ak4601->PLLInput;

    return 0;
}

static int set_pllinput(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	akdbgprt(" [AK4601] %s currMode=%d\n",__FUNCTION__, currMode );

	if (currMode < 3){
		if ( ak4601->PLLInput != currMode ) {
			ak4601->PLLInput = currMode;
			setPLLOut(codec);
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_xtifs(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->XtiFs;

    return 0;
}

static int set_xtifs(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 6){
		if ( ak4601->XtiFs != currMode ) {
			ak4601->XtiFs = currMode;
			setPLLOut(codec);
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}


static const char *ak4601_tdm_texts[] = {
	"TDM OFF", "TDM ON"
};

static const struct soc_enum ak4601_tdm_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_tdm_texts), ak4601_tdm_texts), 
};

static int get_sdin1_tdm(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->TDMSDINbit[0];

    return 0;
}

static int set_sdin1_tdm(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak4601->TDMSDINbit[0] != currMode ) {
			ak4601->TDMSDINbit[0] = currMode;
			ak4601->DIEDGEbit[0] = ak4601->TDMSDINbit[0];
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdout1_tdm(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->TDMSDOUTbit[0];

    return 0;
}

static int set_sdout1_tdm(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak4601->TDMSDOUTbit[0] != currMode ) {
			ak4601->TDMSDOUTbit[0] = currMode;
			ak4601->DOEDGEbit[0] = ak4601->TDMSDOUTbit[0];
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static const char *ak4601_sd_ioformat_texts[] = {
	"24bit",
	"20bit",
	"16bit",
	"32bit",
};


static const struct soc_enum ak4601_slotlen_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak4601_sd_ioformat_texts), ak4601_sd_ioformat_texts), 
};


static int get_sdio3_word_len(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DIDL3;

    return 0;
}

static int set_sdio3_word_len(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DIDL3 != currMode ) {
			ak4601->DIDL3 = currMode;
		}
		snd_soc_update_bits( ak4601->codec, AK4601_052_SDIN3_DIGITAL_INPUT_FORMAT,  0x03, ak4601->DIDL3);
		snd_soc_update_bits( ak4601->codec, AK4601_057_SDOUT3_DIGITAL_OUTPUT_FORMAT,0x03, ak4601->DIDL3);
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdin1_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DISLbit[0];

    return 0;
}

static int set_sdin1_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DISLbit[0] != currMode ) {
			ak4601->DISLbit[0] = currMode;
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdin2_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DISLbit[1];

    return 0;
}

static int set_sdin2_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DISLbit[1] != currMode ) {
			ak4601->DISLbit[1] = currMode;
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdin3_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DISLbit[2];

    return 0;
}

static int set_sdin3_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DISLbit[2] != currMode ) {
			ak4601->DISLbit[2] = currMode;
		}
		snd_soc_update_bits( ak4601->codec, AK4601_052_SDIN3_DIGITAL_INPUT_FORMAT, 0x30, currMode );
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdout1_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DOSLbit[0];

    return 0;
}

static int set_sdout1_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DOSLbit[0] != currMode ) {
			ak4601->DOSLbit[0] = currMode;
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdout2_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DOSLbit[1];

    return 0;
}

static int set_sdout2_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DOSLbit[1] != currMode ) {
			ak4601->DOSLbit[1] = currMode;
		}
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static int get_sdout3_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak4601->DOSLbit[2];

    return 0;
}

static int set_sdout3_slot(struct snd_kcontrol *kcontrol,struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak4601->DOSLbit[2] != currMode ) {
			ak4601->DOSLbit[2] = currMode;
		}
		snd_soc_update_bits( ak4601->codec, AK4601_057_SDOUT3_DIGITAL_OUTPUT_FORMAT, 0x30, currMode << 4 );
	}
	else {
		akdbgprt(" [AK4601] %s Invalid Value selected!\n",__FUNCTION__);
		return -EINVAL;
	}
    return 0;
}

static const char *mixer_level_adjst_texts[] = {
	"No Shift", "1bit Right Shift", "Mute"
};

static const char *mixer_data_change_texts[] = {
	"Through", "Lin->LRout", "Rin->LRout", "Swap"
};

static const struct soc_enum ak4601_mixer_setting_enum[] = {
	SOC_ENUM_SINGLE(AK4601_060_MIXER_A_SETTING, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4601_060_MIXER_A_SETTING, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4601_061_MIXER_B_SETTING, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4601_061_MIXER_B_SETTING, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4601_060_MIXER_A_SETTING, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4601_060_MIXER_A_SETTING, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4601_061_MIXER_B_SETTING, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4601_061_MIXER_B_SETTING, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
};

static const char *sdout_modeset_texts[] = {
	"Slow", "Fast",
};

static const struct soc_enum ak4601_sdout_modeset_enum[] = {
	SOC_ENUM_SINGLE(AK4601_05A_SDOUT_PHASE_SETTING, 0, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts ),
	SOC_ENUM_SINGLE(AK4601_05A_SDOUT_PHASE_SETTING, 1, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts ),
	SOC_ENUM_SINGLE(AK4601_05A_SDOUT_PHASE_SETTING, 2, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts ),
};

static const char *clkosel_texts[] = {
	"12.288MHz", "24.576MHz", "8.192MHz", "6.144MHz", 
	"4.096MHz", "2.048MHz",
};

static const struct soc_enum ak4601_clkosel_enum[] = {
	SOC_ENUM_SINGLE(AK4601_00F_CLKO_OUTPUT_SETTING, 0, ARRAY_SIZE(clkosel_texts), clkosel_texts),
};

static const char *sdio3_texts[] ={
	"LRCK2,BICK2 Enable",
	"SDIN3,SDOUT3 Enable",
};

static const struct soc_enum ak4601_sdio3_enum[] ={
	SOC_ENUM_SINGLE(AK4601_010_PIN_SETTING, 0, ARRAY_SIZE(sdio3_texts), sdio3_texts),
};

static const char *adda_digfilsel_texts[] = {
	"Sharp",
	"Slow",
	"Short Delay Sharp",
	"Short Delay Slow",
};

static const struct soc_enum ak4601_adda_digfilsel_enum[] = {
	SOC_ENUM_SINGLE(AK4601_06B_ANALOG_INPUT_SELECT_SETTING, 6, ARRAY_SIZE(adda_digfilsel_texts), adda_digfilsel_texts),
	SOC_ENUM_SINGLE(AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 0, ARRAY_SIZE(adda_digfilsel_texts), adda_digfilsel_texts),

};

static const char *adda_trantime_texts[] = {
	"1/fs",
	"4/fs",
};

static const struct soc_enum ak4601_adda_trantime_enum[] = {
	SOC_ENUM_SINGLE(AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 7, ARRAY_SIZE(adda_trantime_texts), adda_trantime_texts),
	SOC_ENUM_SINGLE(AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 7, ARRAY_SIZE(adda_trantime_texts), adda_trantime_texts),
	SOC_ENUM_SINGLE(AK4601_07F_VOL_SETTING, 7, ARRAY_SIZE(adda_trantime_texts), adda_trantime_texts),
};

static const char *dsm_ckset_texts[] = {
	"12.288MHz",
	"Fs based",
};

static const struct soc_enum ak4601_dsm_ckset_enum[] = {
	SOC_ENUM_SINGLE(AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 2, ARRAY_SIZE(dsm_ckset_texts), dsm_ckset_texts),
};

static const char* adc_input_vol_texts[] ={
	"2.3Vpp/plus or minus2.3Vpp",
	"2.83Vpp/plus or minus2.83Vpp",
};

static const struct soc_enum ak4601_adc_input_vol_enum[] ={
	SOC_ENUM_SINGLE(AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 7, ARRAY_SIZE(adc_input_vol_texts),adc_input_vol_texts),//ADC1 L
	SOC_ENUM_SINGLE(AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 6, ARRAY_SIZE(adc_input_vol_texts),adc_input_vol_texts),//ADC1 R
	SOC_ENUM_SINGLE(AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 5, ARRAY_SIZE(adc_input_vol_texts),adc_input_vol_texts),//ADC2 L
	SOC_ENUM_SINGLE(AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 4, ARRAY_SIZE(adc_input_vol_texts),adc_input_vol_texts),//ADC2 R
	SOC_ENUM_SINGLE(AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 3, ARRAY_SIZE(adc_input_vol_texts),adc_input_vol_texts),//ADCM
};

//static int ak4601_reads(struct snd_soc_codec *codec, u8 *, size_t, u8 *, size_t);

#ifdef AK4601_DEBUG

static const char *test_reg_select[]   = 
{
    "read AK4601 Reg 000:102",
};

static const struct soc_enum ak4601_test_enum[] = 
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo = 0;

static int get_test_reg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = nTestRegNo;

    return 0;
}

static int set_test_reg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    i, value;
	int	   regs, rege;

	nTestRegNo = currMode;

	if ( currMode < 1 ) {
		if ( currMode == 0 ) {
			regs = 0x000;
			rege = 0x102;
		}
		for ( i = regs ; i <= rege ; i++ ){
			value = snd_soc_read(codec, i);
			printk("[AK4601] Addr,Reg=(%03x, %02x)\n", i, value);
		}
	}
	else {
		return -EINVAL;
	}

	return 0;

}

#endif

static const struct snd_kcontrol_new ak4601_snd_controls[] = {
	SOC_SINGLE("CLK Out Enable", AK4601_00F_CLKO_OUTPUT_SETTING, 3, 1, 0), 
	SOC_ENUM("CLK Out Select",  ak4601_clkosel_enum[0]), 
	SOC_ENUM("LRCK2/SDOUT3 Pin BICK2/SDIN3 Pin Setting", ak4601_sdio3_enum[0]),

	SOC_SINGLE_TLV("MIC Input Volume L", AK4601_062_MIC_AMP_GAIN, 4, 0x0F, 0, mgnl_tlv),
	SOC_SINGLE_TLV("MIC Input Volume R", AK4601_062_MIC_AMP_GAIN, 0, 0x0F, 0, mgnr_tlv),

	SOC_SINGLE_TLV("ADC1 Digital Volume L", AK4601_064_ADC1LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("ADC1 Digital Volume R", AK4601_065_ADC1RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("ADC2 Digital Volume L", AK4601_066_ADC2LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("ADC2 Digital Volume R", AK4601_067_ADC2RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("ADCM Digital Volume",   AK4601_068_ADCM_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),

	SOC_SINGLE_TLV("DAC1 Digital Volume L", AK4601_06D_DAC1LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC1 Digital Volume R", AK4601_06E_DAC1RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC2 Digital Volume L", AK4601_06F_DAC2LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC2 Digital Volume R", AK4601_070_DAC2RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC3 Digital Volume L", AK4601_071_DAC3LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC3 Digital Volume R", AK4601_072_DAC3RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),

	SOC_SINGLE_TLV("VOL1 Digital Volume L", AK4601_075_VOL1LCH_DIGITAL_VOLUME, 0, 0xFF, 1, volvol_tlv),
	SOC_SINGLE_TLV("VOL1 Digital Volume R", AK4601_076_VOL1RCH_DIGITAL_VOLUME, 0, 0xFF, 1, volvol_tlv),
	SOC_SINGLE_TLV("VOL2 Digital Volume L", AK4601_077_VOL2LCH_DIGITAL_VOLUME, 0, 0xFF, 1, volvol_tlv),
	SOC_SINGLE_TLV("VOL2 Digital Volume R", AK4601_078_VOL2RCH_DIGITAL_VOLUME, 0, 0xFF, 1, volvol_tlv),
	SOC_SINGLE_TLV("VOL3 Digital Volume L", AK4601_079_VOL3LCH_DIGITAL_VOLUME, 0, 0xFF, 1, volvol_tlv),
	SOC_SINGLE_TLV("VOL3 Digital Volume R", AK4601_07A_VOL3RCH_DIGITAL_VOLUME, 0, 0xFF, 1, volvol_tlv),

	SOC_SINGLE("ADC1 Mute", AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 6, 1, 0),
	SOC_SINGLE("ADC2 Mute", AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 5, 1, 0), 
	SOC_SINGLE("ADCM Mute", AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 4, 1, 0), 

	SOC_SINGLE("DAC1 Mute", AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 6, 1, 0), 
	SOC_SINGLE("DAC2 Mute", AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 5, 1, 0), 
	SOC_SINGLE("DAC3 Mute", AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 4, 1, 0), 


	SOC_ENUM("MixerA Sync Domain", ak4601_sdsel_enum[6]),
	SOC_ENUM("MixerB Sync Domain", ak4601_sdsel_enum[7]),
	SOC_ENUM("VOL1 Sync Domain", ak4601_sdsel_enum[3]),
	SOC_ENUM("VOL2 Sync Domain", ak4601_sdsel_enum[4]),
	SOC_ENUM("VOL3 Sync Domain", ak4601_sdsel_enum[5]),
	SOC_ENUM("ADC1 Sync Domain", ak4601_sdsel_enum[8]),
	SOC_ENUM("CODEC Sync Domain", ak4601_sdsel_enum[9]),

	SOC_ENUM("CODEC Sampling Frequency", ak4601_fsmode_enum[0]), 
	SOC_ENUM_EXT("Sync Domain 1 fs", ak4601_sd_fs_enum[0], get_sd1_fs, set_sd1_fs),
	SOC_ENUM_EXT("Sync Domain 2 fs", ak4601_sd_fs_enum[0], get_sd2_fs, set_sd2_fs),
	SOC_ENUM_EXT("Sync Domain 1 BICK", ak4601_sd_fs_enum[1], get_sd1_bick, set_sd1_bick),
	SOC_ENUM_EXT("Sync Domain 2 BICK", ak4601_sd_fs_enum[1], get_sd2_bick, set_sd2_bick),

	SOC_ENUM_EXT("LRCK1/BICK1 Master", ak4601_msnbit_enum[0], get_sd1_ms, set_sd1_ms ),
	SOC_ENUM_EXT("LRCK2/BICK2 Master", ak4601_msnbit_enum[0], get_sd2_ms, set_sd2_ms ),

	SOC_ENUM_EXT("PLL Input Clock", ak4601_pllset_enum[0], get_pllinput, set_pllinput),
	SOC_ENUM_EXT("XTI Frequency", ak4601_pllset_enum[1], get_xtifs, set_xtifs),

	SOC_ENUM_EXT("SDIN1 TDM Setting", ak4601_tdm_enum[0], get_sdin1_tdm, set_sdin1_tdm),
	SOC_ENUM_EXT("SDOUT1 TDM Setting", ak4601_tdm_enum[0], get_sdout1_tdm, set_sdout1_tdm),

	SOC_ENUM_EXT("SDIN1 Slot Length", ak4601_slotlen_enum[0], get_sdin1_slot, set_sdin1_slot),
	SOC_ENUM_EXT("SDIN2 Slot Length", ak4601_slotlen_enum[0], get_sdin2_slot, set_sdin2_slot),
	SOC_ENUM_EXT("SDIN3 Slot Length", ak4601_slotlen_enum[0], get_sdin3_slot, set_sdin3_slot),
	SOC_ENUM_EXT("SDOUT1 Slot Length", ak4601_slotlen_enum[0], get_sdout1_slot, set_sdout1_slot),
	SOC_ENUM_EXT("SDOUT2 Slot Length", ak4601_slotlen_enum[0], get_sdout2_slot, set_sdout2_slot),
	SOC_ENUM_EXT("SDOUT3 Slot Length", ak4601_slotlen_enum[0], get_sdout3_slot, set_sdout3_slot),
	SOC_ENUM_EXT("SDIO3 Word Length", ak4601_slotlen_enum[0], get_sdio3_word_len, set_sdio3_word_len),	

	SOC_ENUM("MixerA Input1 Data Change", ak4601_mixer_setting_enum[0]),
	SOC_ENUM("MixerA Input2 Data Change", ak4601_mixer_setting_enum[1]),
	SOC_ENUM("MixerB Input1 Data Change", ak4601_mixer_setting_enum[2]),
	SOC_ENUM("MixerB Input2 Data Change", ak4601_mixer_setting_enum[3]),
	SOC_ENUM("MixerA Input1 Level Adjust", ak4601_mixer_setting_enum[4]),
	SOC_ENUM("MixerA Input2 Level Adjust", ak4601_mixer_setting_enum[5]),
	SOC_ENUM("MixerB Input1 Level Adjust", ak4601_mixer_setting_enum[4]),
	SOC_ENUM("MixerB Input2 Level Adjust", ak4601_mixer_setting_enum[5]),

	SOC_SINGLE("Lch Mic ZeroCross Enable", AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 1, 1, 0),
	SOC_SINGLE("Rch Mic ZeroCross Enable", AK4601_063_ANALOG_INPUT_GAIN_CONTROL, 0, 1, 0),
	
	SOC_ENUM("ADC Digital Filter Select", ak4601_adda_digfilsel_enum[0]),
	SOC_ENUM("ADC Digital Volume Transition Time", ak4601_adda_trantime_enum[0]),
	SOC_ENUM("DAC Digital Filter Select", ak4601_adda_digfilsel_enum[1]),
	SOC_ENUM("DAC Digital Volume Transition Time", ak4601_adda_trantime_enum[1]),

	SOC_SINGLE("ADC1 HPF Enable", AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 2, 1, 1),
	SOC_SINGLE("ADC2 HPF Enable", AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 1, 1, 1),
	SOC_SINGLE("ADCM HPF Enable", AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 0, 1, 1),

	SOC_ENUM("ADC1 Lch Input Voltage Setting", ak4601_adc_input_vol_enum[0]),
	SOC_ENUM("ADC1 Rch Input Voltage Setting", ak4601_adc_input_vol_enum[1]),
	SOC_ENUM("ADC2 Lch Input Voltage Setting", ak4601_adc_input_vol_enum[2]),
	SOC_ENUM("ADC2 Rch Input Voltage Setting", ak4601_adc_input_vol_enum[3]),
	SOC_ENUM("ADCM Input Voltage Setting", ak4601_adc_input_vol_enum[4]),

	SOC_ENUM("DAC DSM Sampling Clock Setting", ak4601_dsm_ckset_enum[0]),
	SOC_ENUM("VOL Digital Volume Transition Time", ak4601_adda_trantime_enum[2]),

	SOC_SINGLE("PLL Lock Signal Out Setting", AK4601_083_STO_FLAG_SETTING1, 6, 1, 0),
	SOC_ENUM("SDOUT1 Fast Mode Setting", ak4601_sdout_modeset_enum[0]),
	SOC_ENUM("SDOUT2 Fast Mode Setting", ak4601_sdout_modeset_enum[1]),
	SOC_ENUM("SDOUT3 Fast Mode Setting", ak4601_sdout_modeset_enum[2]),
	
	SOC_SINGLE("SDOUT1 Output Enable", AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 5, 1, 0),
	SOC_SINGLE("SDOUT2 Output Enable", AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 4, 1, 0),
	SOC_SINGLE("SDOUT3 Output Enable", AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 3, 1, 0),

	SOC_SINGLE("LRCK1/BICK1 Pin Pull-Down Setting", AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 7, 1, 0),
	SOC_SINGLE("LRCK2/BICK2 Pin Pull-Down Setting", AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 6, 1, 0),

#ifdef AK4601_DEBUG
	SOC_ENUM_EXT("Reg Read", ak4601_test_enum[0], get_test_reg, set_test_reg),
#endif

};


static const char *ak4601_micbias1_select_texts[] =
		{"LineIn", "MicBias1"};

static const char *ak4601_micbias2_select_texts[] =
		{"LineIn", "MicBias2"};

static const struct soc_enum ak4601_micbias1_mux_enum =
	SOC_ENUM_SINGLE(0, 0,
			ARRAY_SIZE(ak4601_micbias1_select_texts), ak4601_micbias1_select_texts);

static const struct snd_kcontrol_new ak4601_micbias1_mux_control =
	SOC_DAPM_ENUM("MicBias1 Select", ak4601_micbias1_mux_enum);

static const struct soc_enum ak4601_micbias2_mux_enum =
	SOC_ENUM_SINGLE(0, 0,
			ARRAY_SIZE(ak4601_micbias2_select_texts), ak4601_micbias2_select_texts);

static const struct snd_kcontrol_new ak4601_micbias2_mux_control =
	SOC_DAPM_ENUM("MicBias2 Select", ak4601_micbias2_mux_enum);


static const char *ak4601_ain1l_select_texts[] =
		{"IN1P_N", "AIN1L"};

static const struct soc_enum ak4601_adc1l_mux_enum =
	SOC_ENUM_SINGLE(AK4601_06B_ANALOG_INPUT_SELECT_SETTING, 3,
			ARRAY_SIZE(ak4601_ain1l_select_texts), ak4601_ain1l_select_texts);

static const struct snd_kcontrol_new ak4601_ain1l_mux_control =
	SOC_DAPM_ENUM("AIN1 Lch Select", ak4601_adc1l_mux_enum);

static const char *ak4601_ain1r_select_texts[] =
		{"IN2P_N", "AIN1R"};

static const struct soc_enum ak4601_ain1r_mux_enum =
	SOC_ENUM_SINGLE(AK4601_06B_ANALOG_INPUT_SELECT_SETTING, 2,
			ARRAY_SIZE(ak4601_ain1r_select_texts), ak4601_ain1r_select_texts);

static const struct snd_kcontrol_new ak4601_ain1r_mux_control =
	SOC_DAPM_ENUM("AIN1 Rch Select", ak4601_ain1r_mux_enum);


static const char *ak4601_adc2_select_texts[] =
		{"AIN2", "AIN3", "AIN4", "AIN5"};

static const struct soc_enum ak4601_adc2_mux_enum =
	SOC_ENUM_SINGLE(AK4601_06B_ANALOG_INPUT_SELECT_SETTING, 0,
			ARRAY_SIZE(ak4601_adc2_select_texts), ak4601_adc2_select_texts);

static const struct snd_kcontrol_new ak4601_adc2_mux_control =
	SOC_DAPM_ENUM("ADC2 Select", ak4601_adc2_mux_enum);


static const char *ak4601_adcm_select_texts[] =
		{"AINMP_N", "AINM"};

static const struct soc_enum ak4601_adcm_mux_enum =
	SOC_ENUM_SINGLE(AK4601_06B_ANALOG_INPUT_SELECT_SETTING, 4,
			ARRAY_SIZE(ak4601_adcm_select_texts), ak4601_adcm_select_texts);

static const struct snd_kcontrol_new ak4601_adcm_mux_control =
	SOC_DAPM_ENUM("ADCM Select", ak4601_adcm_mux_enum);


static const char *ak4601_source_select_texts[] = {
	"ALL0", 
	"SDIN1",  "SDIN1B", "SDIN1C", "SDIN1D",
	"SDIN1E", "SDIN1F", "SDIN1G", "SDIN1H",
	"SDIN2",  "SDIN3",  "VOL1", "VOL2", "VOL3",
	"ADC1",   "ADC2",   "ADCM", "MixerA", "MixerB",
};

static const unsigned int ak4601_source_select_values[] = {
	0x00, 
	0x01, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x10, 0x11, 0x12,
	0x15, 0x16, 0x17, 0x18, 0x19, 
};

static const struct soc_enum ak4601_sout1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_021_SDOUT1_TDM_SLOT1_2_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1_mux_control =
	SOC_DAPM_ENUM("SDOUT1 Select", ak4601_sout1_mux_enum);

static const struct soc_enum ak4601_sout1b_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_022_SDOUT1_TDM_SLOT3_4_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1b_mux_control =
	SOC_DAPM_ENUM("SDOUT1B Select", ak4601_sout1b_mux_enum);

static const struct soc_enum ak4601_sout1c_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_023_SDOUT1_TDM_SLOT5_6_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1c_mux_control =
	SOC_DAPM_ENUM("SDOUT1C Select", ak4601_sout1c_mux_enum);

static const struct soc_enum ak4601_sout1d_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_024_SDOUT1_TDM_SLOT7_8_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1d_mux_control =
	SOC_DAPM_ENUM("SDOUT1D Select", ak4601_sout1d_mux_enum);

static const struct soc_enum ak4601_sout1e_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_025_SDOUT1_TDM_SLOT9_10_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1e_mux_control =
	SOC_DAPM_ENUM("SDOUT1E Select", ak4601_sout1e_mux_enum);

static const struct soc_enum ak4601_sout1f_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_026_SDOUT1_TDM_SLOT11_12_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1f_mux_control =
	SOC_DAPM_ENUM("SDOUT1F Select", ak4601_sout1f_mux_enum);

static const struct soc_enum ak4601_sout1g_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_027_SDOUT1_TDM_SLOT13_14_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1g_mux_control =
	SOC_DAPM_ENUM("SDOUT1G Select", ak4601_sout1g_mux_enum);

static const struct soc_enum ak4601_sout1h_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_028_SDOUT1_TDM_SLOT15_16_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout1h_mux_control =
	SOC_DAPM_ENUM("SDOUT1H Select", ak4601_sout1h_mux_enum);

static const struct soc_enum ak4601_sout2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_029_SDOUT2_OUTPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout2_mux_control =
	SOC_DAPM_ENUM("SDOUT2 Select", ak4601_sout2_mux_enum);

static const struct soc_enum ak4601_sout3_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_02A_SDOUT2_OUTPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_sout3_mux_control =
	SOC_DAPM_ENUM("SDOUT3 Select", ak4601_sout3_mux_enum);

static const struct soc_enum ak4601_mixera1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_045_MIXER_A_CH1_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_mixera1_mux_control =
	SOC_DAPM_ENUM("MixerA ch1 Input Select", ak4601_mixera1_mux_enum);

static const struct soc_enum ak4601_mixera2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_046_MIXER_A_CH2_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_mixera2_mux_control =
	SOC_DAPM_ENUM("MixerA ch2 Input Select", ak4601_mixera2_mux_enum);

static const struct soc_enum ak4601_mixerb1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_047_MIXER_B_CH1_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_mixerb1_mux_control =
	SOC_DAPM_ENUM("MixerB ch1 Input Select", ak4601_mixerb1_mux_enum);

static const struct soc_enum ak4601_mixerb2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_048_MIXER_B_CH2_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_mixerb2_mux_control =
	SOC_DAPM_ENUM("MixerB ch2 Input Select", ak4601_mixerb2_mux_enum);

static const struct soc_enum ak4601_dac1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_035_DAC1_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_dac1_mux_control =
	SOC_DAPM_ENUM("DAC1 Select", ak4601_dac1_mux_enum);

static const struct soc_enum ak4601_dac2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_036_DAC2_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_dac2_mux_control =
	SOC_DAPM_ENUM("DAC2 Select", ak4601_dac2_mux_enum);

static const struct soc_enum ak4601_dac3_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_037_DAC3_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_dac3_mux_control =
	SOC_DAPM_ENUM("DAC3 Select", ak4601_dac3_mux_enum);

static const struct soc_enum ak4601_vol1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_038_VOL1_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_vol1_mux_control =
	SOC_DAPM_ENUM("VOL1 Select", ak4601_vol1_mux_enum);

static const struct soc_enum ak4601_vol2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_039_VOL2_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_vol2_mux_control =
	SOC_DAPM_ENUM("VOL2 Select", ak4601_vol2_mux_enum);

static const struct soc_enum ak4601_vol3_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK4601_03A_VOL3_INPUT_DATA_SELECT, 0, 0x3F,
			ARRAY_SIZE(ak4601_source_select_texts), ak4601_source_select_texts, ak4601_source_select_values);
static const struct snd_kcontrol_new ak4601_vol3_mux_control =
	SOC_DAPM_ENUM("VOL3 Select", ak4601_vol3_mux_enum);

static int ak4601_ClockReset(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			akdbgprt("\t[AK4601] SND_SOC_DAPM_PRE_PMU\n");
			snd_soc_update_bits(codec, AK4601_001_SYSTEM_CLOCK_SETTING_2, 0x80, 0x80);		// CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK4601_08C_RESET_CONTROL, 0x01, 0x01);				// HRESETN bit = 1
			break;

		case SND_SOC_DAPM_POST_PMD:
			akdbgprt("\t[AK4601] SND_SOC_DAPM_POST_PMD\n");
			snd_soc_update_bits(codec, AK4601_08C_RESET_CONTROL, 0x01, 0x00);         // HRESETN bit = 0
			snd_soc_update_bits(codec, AK4601_001_SYSTEM_CLOCK_SETTING_2, 0x80, 0x00);// CKRESETN bit = 0
			break;
	}
	return 0;
}

static int ak4601_MicBias1(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{
	akdbgprt("\t[AK4601] %s\n",__FUNCTION__);

	switch( event ){
		case SND_SOC_DAPM_POST_PMU:
			akdbgprt("\t[AK4601] %s mdelay(100)\n",__FUNCTION__);
			mdelay(100);
			break;
	}

	return 0;
}

static int ak4601_MicBias2(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	int MicBias1;
	akdbgprt("\t[AK4601] %s\n",__FUNCTION__);

	switch( event ){
		case SND_SOC_DAPM_POST_PMU:
			MicBias1 = snd_soc_read(codec, AK4601_002_MIC_BIAS_POWER_MANAGEMENT);
			MicBias1 = (MicBias1 & 0x02) >> 1;
			if( !MicBias1 ){
				akdbgprt("\t[AK4601] %s mdelay(100)\n",__FUNCTION__);
				mdelay(100);
			}
			break;
	}

	return 0;
}

/* ak4601 dapm widgets */
static const struct snd_soc_dapm_widget ak4601_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("Clock Power", 1, SND_SOC_NOPM, 0, 0, ak4601_ClockReset, SND_SOC_DAPM_PRE_PMU |SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CODEC Power", 2, AK4601_08C_RESET_CONTROL, 4, 0, NULL, 0),

//CODEC
	SND_SOC_DAPM_ADC("ADC1", NULL, AK4601_08A_POWER_MANAGEMENT1, 5, 0),
	SND_SOC_DAPM_ADC("ADC2", NULL, AK4601_08A_POWER_MANAGEMENT1, 4, 0),
	SND_SOC_DAPM_ADC("ADCM", NULL, AK4601_08A_POWER_MANAGEMENT1, 3, 0),
	SND_SOC_DAPM_DAC("DAC1", NULL, AK4601_08A_POWER_MANAGEMENT1, 2, 0),
	SND_SOC_DAPM_DAC("DAC2", NULL, AK4601_08A_POWER_MANAGEMENT1, 1, 0),
	SND_SOC_DAPM_DAC("DAC3", NULL, AK4601_08A_POWER_MANAGEMENT1, 0, 0),
//VOL
	SND_SOC_DAPM_PGA("VOL1",SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VOL2",SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("VOL3",SND_SOC_NOPM, 0, 0, NULL, 0),
// Mixer
	SND_SOC_DAPM_PGA("MixerA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("MixerB", SND_SOC_NOPM, 0, 0, NULL, 0),

// Digital Input/Output
	SND_SOC_DAPM_AIF_IN("SDIN1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN3", SDIO3_LINK" Playback", 0, SND_SOC_NOPM, 0, 0),
	
	SND_SOC_DAPM_AIF_OUT("SDOUT1", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT2", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT3", SDIO3_LINK" Capture", 0, SND_SOC_NOPM, 0, 0),

// Source Selector
	SND_SOC_DAPM_MUX("SDOUT1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1B Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1b_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1C Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1c_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1D Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1d_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1E Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1e_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1F Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1f_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1G Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1g_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1H Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout1h_mux_control),
	SND_SOC_DAPM_MUX("SDOUT2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout2_mux_control),
	SND_SOC_DAPM_MUX("SDOUT3 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_sout3_mux_control),

	SND_SOC_DAPM_MUX("MixerA Ch1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_mixera1_mux_control),
	SND_SOC_DAPM_MUX("MixerA Ch2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_mixera2_mux_control),
	SND_SOC_DAPM_MUX("MixerB Ch1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_mixerb1_mux_control),
	SND_SOC_DAPM_MUX("MixerB Ch2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_mixerb2_mux_control),

	SND_SOC_DAPM_MUX("DAC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_dac1_mux_control),
	SND_SOC_DAPM_MUX("DAC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_dac2_mux_control),
	SND_SOC_DAPM_MUX("DAC3 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_dac3_mux_control),
	SND_SOC_DAPM_MUX("VOL1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_vol1_mux_control),
	SND_SOC_DAPM_MUX("VOL2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_vol2_mux_control),
	SND_SOC_DAPM_MUX("VOL3 Source Selector", SND_SOC_NOPM, 0, 0, &ak4601_vol3_mux_control),

// MIC Bias
	//SND_SOC_DAPM_SUPPLY("MicBias1", AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 1, 0, &ak4601_MicBias1, SND_SOC_DAPM_POST_PMU),
	//SND_SOC_DAPM_MICBIAS("MicBias1", AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 1, 0),
	SND_SOC_DAPM_ADC_E("MicBias1", NULL, AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 1, 0, &ak4601_MicBias1, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX("MicBias1 MUX", SND_SOC_NOPM, 0, 0, &ak4601_micbias1_mux_control),

	//SND_SOC_DAPM_SUPPLY("MicBias2", AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 0, 0, &ak4601_MicBias2, SND_SOC_DAPM_POST_PMU),
	//SND_SOC_DAPM_MICBIAS("MicBias2", AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 0, 0),
	SND_SOC_DAPM_ADC_E("MicBias2", NULL, AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 0, 0, &ak4601_MicBias2, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MUX("MicBias2 MUX", SND_SOC_NOPM, 0, 0, &ak4601_micbias2_mux_control),

// Analog Input
	SND_SOC_DAPM_INPUT("IN1P_N"),
	SND_SOC_DAPM_INPUT("AIN1L"),
	SND_SOC_DAPM_INPUT("IN2P_N"),
	SND_SOC_DAPM_INPUT("AIN1R"),

	SND_SOC_DAPM_INPUT("AIN2"),
	SND_SOC_DAPM_INPUT("AIN3"),
	SND_SOC_DAPM_INPUT("AIN4"),
	SND_SOC_DAPM_INPUT("AIN5"),

	SND_SOC_DAPM_INPUT("AINMP_N"),
	SND_SOC_DAPM_INPUT("AINM"),

	SND_SOC_DAPM_MUX("AIN1 Lch MUX", SND_SOC_NOPM, 0, 0, &ak4601_ain1l_mux_control),
	SND_SOC_DAPM_MUX("AIN1 Rch MUX", SND_SOC_NOPM, 0, 0, &ak4601_ain1r_mux_control),

	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0, &ak4601_adc2_mux_control),

	SND_SOC_DAPM_MUX("ADCM MUX", SND_SOC_NOPM, 0, 0, &ak4601_adcm_mux_control),


// Analog Output
	SND_SOC_DAPM_OUTPUT("AOUT1"),
	SND_SOC_DAPM_OUTPUT("AOUT2"),
	SND_SOC_DAPM_OUTPUT("AOUT3"),

};

static const struct snd_soc_dapm_route ak4601_intercon[] = {

	{"CODEC Power", NULL, "Clock Power"},
	{"SDOUT1", NULL, "Clock Power"},
	{"SDOUT2", NULL, "Clock Power"},
	{"SDOUT3", NULL, "Clock Power"},
	
	{"ADC1", NULL, "CODEC Power"},
	{"ADC2", NULL, "CODEC Power"},
	{"ADCM", NULL, "CODEC Power"},
	{"DAC1", NULL, "CODEC Power"},
	{"DAC2", NULL, "CODEC Power"},
	{"DAC3", NULL, "CODEC Power"},

	{"AIN1 Lch MUX", "IN1P_N", "IN1P_N"},
	{"AIN1 Lch MUX", "AIN1L", "AIN1L"},
	{"MicBias1", NULL, "AIN1 Lch MUX"},
	{"MicBias1 MUX", "LineIn", "AIN1 Lch MUX"},
	{"MicBias1 MUX", "MicBias1", "MicBias1"},

	{"AIN1 Rch MUX", "IN2P_N", "IN2P_N"},
	{"AIN1 Rch MUX", "AIN1R", "AIN1R"},
	{"MicBias2", NULL, "AIN1 Rch MUX"},
	{"MicBias2 MUX", "LineIn", "AIN1 Rch MUX"},
	{"MicBias2 MUX", "MicBias2", "MicBias2"},

	{"ADC1", NULL, "MicBias1 MUX"},
	{"ADC1", NULL, "MicBias2 MUX"},

	{"ADC2 MUX", "AIN2", "AIN2"},
	{"ADC2 MUX", "AIN3", "AIN3"},
	{"ADC2 MUX", "AIN4", "AIN4"},
	{"ADC2 MUX", "AIN5", "AIN5"},
	{"ADC2", NULL, "ADC2 MUX"},

	{"ADCM MUX", "AINMP_N", "AINMP_N"},
	{"ADCM MUX", "AINM", "AINM"},
	{"ADCM", NULL, "ADCM MUX"},

	{"DAC1", NULL, "DAC1 Source Selector"},
	{"DAC2", NULL, "DAC2 Source Selector"},
	{"DAC3", NULL, "DAC3 Source Selector"},

	{"AOUT1", NULL, "DAC1"},
	{"AOUT2", NULL, "DAC2"},
	{"AOUT3", NULL, "DAC3"},

	{"VOL1", NULL, "VOL1 Source Selector"},
	{"VOL2", NULL, "VOL2 Source Selector"},
	{"VOL3", NULL, "VOL3 Source Selector"},

	{"SDOUT1", NULL, "SDOUT1 Source Selector"},
	{"SDOUT1", NULL, "SDOUT1B Source Selector"},
	{"SDOUT1", NULL, "SDOUT1C Source Selector"},
	{"SDOUT1", NULL, "SDOUT1D Source Selector"},
	{"SDOUT1", NULL, "SDOUT1E Source Selector"},
	{"SDOUT1", NULL, "SDOUT1F Source Selector"},
	{"SDOUT1", NULL, "SDOUT1G Source Selector"},
	{"SDOUT1", NULL, "SDOUT1H Source Selector"},
	{"SDOUT2", NULL, "SDOUT2 Source Selector"},
	{"SDOUT3", NULL, "SDOUT3 Source Selector"},

	{"MixerA", NULL, "MixerA Ch1 Source Selector"},
	{"MixerA", NULL, "MixerA Ch2 Source Selector"},
	{"MixerB", NULL, "MixerB Ch1 Source Selector"},
	{"MixerB", NULL, "MixerB Ch2 Source Selector"},

	{"SDOUT1 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1 Source Selector", "VOL1", "VOL1"},
	{"SDOUT1 Source Selector", "VOL2", "VOL2"},
	{"SDOUT1 Source Selector", "VOL3", "VOL3"},
	{"SDOUT1 Source Selector", "ADC1", "ADC1"},
	{"SDOUT1 Source Selector", "ADC2", "ADC2"},
	{"SDOUT1 Source Selector", "ADCM", "ADCM"},
	{"SDOUT1 Source Selector", "MixerA", "MixerA"},
	{"SDOUT1 Source Selector", "MixerB", "MixerB"},

	{"SDOUT1B Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1B Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1B Source Selector", "VOL1", "VOL1"},
	{"SDOUT1B Source Selector", "VOL2", "VOL2"},
	{"SDOUT1B Source Selector", "VOL3", "VOL3"},
	{"SDOUT1B Source Selector", "ADC1", "ADC1"},
	{"SDOUT1B Source Selector", "ADC2", "ADC2"},
	{"SDOUT1B Source Selector", "ADCM", "ADCM"},
	{"SDOUT1B Source Selector", "MixerA", "MixerA"},
	{"SDOUT1B Source Selector", "MixerB", "MixerB"},

	{"SDOUT1C Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1C Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1C Source Selector", "VOL1", "VOL1"},
	{"SDOUT1C Source Selector", "VOL2", "VOL2"},
	{"SDOUT1C Source Selector", "VOL3", "VOL3"},
	{"SDOUT1C Source Selector", "ADC1", "ADC1"},
	{"SDOUT1C Source Selector", "ADC2", "ADC2"},
	{"SDOUT1C Source Selector", "ADCM", "ADCM"},
	{"SDOUT1C Source Selector", "MixerA", "MixerA"},
	{"SDOUT1C Source Selector", "MixerB", "MixerB"},

	{"SDOUT1D Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1D Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1D Source Selector", "VOL1", "VOL1"},
	{"SDOUT1D Source Selector", "VOL2", "VOL2"},
	{"SDOUT1D Source Selector", "VOL3", "VOL3"},
	{"SDOUT1D Source Selector", "ADC1", "ADC1"},
	{"SDOUT1D Source Selector", "ADC2", "ADC2"},
	{"SDOUT1D Source Selector", "ADCM", "ADCM"},
	{"SDOUT1D Source Selector", "MixerA", "MixerA"},
	{"SDOUT1D Source Selector", "MixerB", "MixerB"},

	{"SDOUT1E Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1E Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1E Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1E Source Selector", "VOL1", "VOL1"},
	{"SDOUT1E Source Selector", "VOL2", "VOL2"},
	{"SDOUT1E Source Selector", "VOL3", "VOL3"},
	{"SDOUT1E Source Selector", "ADC1", "ADC1"},
	{"SDOUT1E Source Selector", "ADC2", "ADC2"},
	{"SDOUT1E Source Selector", "ADCM", "ADCM"},
	{"SDOUT1E Source Selector", "MixerA", "MixerA"},
	{"SDOUT1E Source Selector", "MixerB", "MixerB"},

	{"SDOUT1F Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1F Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1F Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1F Source Selector", "VOL1", "VOL1"},
	{"SDOUT1F Source Selector", "VOL2", "VOL2"},
	{"SDOUT1F Source Selector", "VOL3", "VOL3"},
	{"SDOUT1F Source Selector", "ADC1", "ADC1"},
	{"SDOUT1F Source Selector", "ADC2", "ADC2"},
	{"SDOUT1F Source Selector", "ADCM", "ADCM"},
	{"SDOUT1F Source Selector", "MixerA", "MixerA"},
	{"SDOUT1F Source Selector", "MixerB", "MixerB"},

	{"SDOUT1G Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1G Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1G Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1G Source Selector", "VOL1", "VOL1"},
	{"SDOUT1G Source Selector", "VOL2", "VOL2"},
	{"SDOUT1G Source Selector", "VOL3", "VOL3"},
	{"SDOUT1G Source Selector", "ADC1", "ADC1"},
	{"SDOUT1G Source Selector", "ADC2", "ADC2"},
	{"SDOUT1G Source Selector", "ADCM", "ADCM"},
	{"SDOUT1G Source Selector", "MixerA", "MixerA"},
	{"SDOUT1G Source Selector", "MixerB", "MixerB"},

	{"SDOUT1H Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT1H Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1H Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1H Source Selector", "VOL1", "VOL1"},
	{"SDOUT1H Source Selector", "VOL2", "VOL2"},
	{"SDOUT1H Source Selector", "VOL3", "VOL3"},
	{"SDOUT1H Source Selector", "ADC1", "ADC1"},
	{"SDOUT1H Source Selector", "ADC2", "ADC2"},
	{"SDOUT1H Source Selector", "ADCM", "ADCM"},
	{"SDOUT1H Source Selector", "MixerA", "MixerA"},
	{"SDOUT1H Source Selector", "MixerB", "MixerB"},

	{"SDOUT2 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT2 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT2 Source Selector", "VOL1", "VOL1"},
	{"SDOUT2 Source Selector", "VOL2", "VOL2"},
	{"SDOUT2 Source Selector", "VOL3", "VOL3"},
	{"SDOUT2 Source Selector", "ADC1", "ADC1"},
	{"SDOUT2 Source Selector", "ADC2", "ADC2"},
	{"SDOUT2 Source Selector", "ADCM", "ADCM"},
	{"SDOUT2 Source Selector", "MixerA", "MixerA"},
	{"SDOUT2 Source Selector", "MixerB", "MixerB"},

	{"SDOUT3 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1E", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1F", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1G", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1H", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT3 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT3 Source Selector", "VOL1", "VOL1"},
	{"SDOUT3 Source Selector", "VOL2", "VOL2"},
	{"SDOUT3 Source Selector", "VOL3", "VOL3"},
	{"SDOUT3 Source Selector", "ADC1", "ADC1"},
	{"SDOUT3 Source Selector", "ADC2", "ADC2"},
	{"SDOUT3 Source Selector", "ADCM", "ADCM"},
	{"SDOUT3 Source Selector", "MixerA", "MixerA"},
	{"SDOUT3 Source Selector", "MixerB", "MixerB"},

	{"MixerA Ch1 Source Selector", "SDIN1", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1B", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1C", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1D", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1E", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1F", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1G", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN1H", "SDIN1"},
	{"MixerA Ch1 Source Selector", "SDIN2", "SDIN2"},
	{"MixerA Ch1 Source Selector", "SDIN3", "SDIN3"},
	{"MixerA Ch1 Source Selector", "VOL1", "VOL1"},
	{"MixerA Ch1 Source Selector", "VOL2", "VOL2"},
	{"MixerA Ch1 Source Selector", "VOL3", "VOL3"},
	{"MixerA Ch1 Source Selector", "ADC1", "ADC1"},
	{"MixerA Ch1 Source Selector", "ADC2", "ADC2"},
	{"MixerA Ch1 Source Selector", "ADCM", "ADCM"},
	{"MixerA Ch1 Source Selector", "MixerA", "MixerA"},
	{"MixerA Ch1 Source Selector", "MixerB", "MixerB"},

	{"MixerA Ch2 Source Selector", "SDIN1", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1B", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1C", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1D", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1E", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1F", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1G", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN1H", "SDIN1"},
	{"MixerA Ch2 Source Selector", "SDIN2", "SDIN2"},
	{"MixerA Ch2 Source Selector", "SDIN3", "SDIN3"},
	{"MixerA Ch2 Source Selector", "VOL1", "VOL1"},
	{"MixerA Ch2 Source Selector", "VOL2", "VOL2"},
	{"MixerA Ch2 Source Selector", "VOL3", "VOL3"},
	{"MixerA Ch2 Source Selector", "ADC1", "ADC1"},
	{"MixerA Ch2 Source Selector", "ADC2", "ADC2"},
	{"MixerA Ch2 Source Selector", "ADCM", "ADCM"},
	{"MixerA Ch2 Source Selector", "MixerA", "MixerA"},
	{"MixerA Ch2 Source Selector", "MixerB", "MixerB"},

	{"MixerB Ch1 Source Selector", "SDIN1", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1B", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1C", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1D", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1E", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1F", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1G", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN1H", "SDIN1"},
	{"MixerB Ch1 Source Selector", "SDIN2", "SDIN2"},
	{"MixerB Ch1 Source Selector", "SDIN3", "SDIN3"},
	{"MixerB Ch1 Source Selector", "VOL1", "VOL1"},
	{"MixerB Ch1 Source Selector", "VOL2", "VOL2"},
	{"MixerB Ch1 Source Selector", "VOL3", "VOL3"},
	{"MixerB Ch1 Source Selector", "ADC1", "ADC1"},
	{"MixerB Ch1 Source Selector", "ADC2", "ADC2"},
	{"MixerB Ch1 Source Selector", "ADCM", "ADCM"},
	{"MixerB Ch1 Source Selector", "MixerA", "MixerA"},
	{"MixerB Ch1 Source Selector", "MixerB", "MixerB"},


	{"MixerB Ch2 Source Selector", "SDIN1", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1B", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1C", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1D", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1E", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1F", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1G", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN1H", "SDIN1"},
	{"MixerB Ch2 Source Selector", "SDIN2", "SDIN2"},
	{"MixerB Ch2 Source Selector", "SDIN3", "SDIN3"},
	{"MixerB Ch2 Source Selector", "VOL1", "VOL1"},
	{"MixerB Ch2 Source Selector", "VOL2", "VOL2"},
	{"MixerB Ch2 Source Selector", "VOL3", "VOL3"},
	{"MixerB Ch2 Source Selector", "ADC1", "ADC1"},
	{"MixerB Ch2 Source Selector", "ADC2", "ADC2"},
	{"MixerB Ch2 Source Selector", "ADCM", "ADCM"},
	{"MixerB Ch2 Source Selector", "MixerA", "MixerA"},
	{"MixerB Ch2 Source Selector", "MixerB", "MixerB"},

	{"DAC1 Source Selector", "SDIN1", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1B", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1C", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1D", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1E", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1F", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1G", "SDIN1"},
	{"DAC1 Source Selector", "SDIN1H", "SDIN1"},
	{"DAC1 Source Selector", "SDIN2", "SDIN2"},
	{"DAC1 Source Selector", "SDIN3", "SDIN3"},
	{"DAC1 Source Selector", "VOL1", "VOL1"},
	{"DAC1 Source Selector", "VOL2", "VOL2"},
	{"DAC1 Source Selector", "VOL3", "VOL3"},
	{"DAC1 Source Selector", "ADC1", "ADC1"},
	{"DAC1 Source Selector", "ADC2", "ADC2"},
	{"DAC1 Source Selector", "ADCM", "ADCM"},
	{"DAC1 Source Selector", "MixerA", "MixerA"},
	{"DAC1 Source Selector", "MixerB", "MixerB"},

	{"DAC2 Source Selector", "SDIN1", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1B", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1C", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1D", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1E", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1F", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1G", "SDIN1"},
	{"DAC2 Source Selector", "SDIN1H", "SDIN1"},
	{"DAC2 Source Selector", "SDIN2", "SDIN2"},
	{"DAC2 Source Selector", "SDIN3", "SDIN3"},
	{"DAC2 Source Selector", "VOL1", "VOL1"},
	{"DAC2 Source Selector", "VOL2", "VOL2"},
	{"DAC2 Source Selector", "VOL3", "VOL3"},
	{"DAC2 Source Selector", "ADC1", "ADC1"},
	{"DAC2 Source Selector", "ADC2", "ADC2"},
	{"DAC2 Source Selector", "ADCM", "ADCM"},
	{"DAC2 Source Selector", "MixerA", "MixerA"},
	{"DAC2 Source Selector", "MixerB", "MixerB"},

	{"DAC3 Source Selector", "SDIN1", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1B", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1C", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1D", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1E", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1F", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1G", "SDIN1"},
	{"DAC3 Source Selector", "SDIN1H", "SDIN1"},
	{"DAC3 Source Selector", "SDIN2", "SDIN2"},
	{"DAC3 Source Selector", "SDIN3", "SDIN3"},
	{"DAC3 Source Selector", "VOL1", "VOL1"},
	{"DAC3 Source Selector", "VOL2", "VOL2"},
	{"DAC3 Source Selector", "VOL3", "VOL3"},
	{"DAC3 Source Selector", "ADC1", "ADC1"},
	{"DAC3 Source Selector", "ADC2", "ADC2"},
	{"DAC3 Source Selector", "ADCM", "ADCM"},
	{"DAC3 Source Selector", "MixerA", "MixerA"},
	{"DAC3 Source Selector", "MixerB", "MixerB"},

	{"VOL1 Source Selector", "SDIN1", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1B", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1C", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1D", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1E", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1F", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1G", "SDIN1"},
	{"VOL1 Source Selector", "SDIN1H", "SDIN1"},
	{"VOL1 Source Selector", "SDIN2", "SDIN2"},
	{"VOL1 Source Selector", "SDIN3", "SDIN3"},
	{"VOL1 Source Selector", "VOL1", "VOL1"},
	{"VOL1 Source Selector", "VOL2", "VOL2"},
	{"VOL1 Source Selector", "VOL3", "VOL3"},
	{"VOL1 Source Selector", "ADC1", "ADC1"},
	{"VOL1 Source Selector", "ADC2", "ADC2"},
	{"VOL1 Source Selector", "ADCM", "ADCM"},
	{"VOL1 Source Selector", "MixerA", "MixerA"},
	{"VOL1 Source Selector", "MixerB", "MixerB"},

	{"VOL2 Source Selector", "SDIN1", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1B", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1C", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1D", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1E", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1F", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1G", "SDIN1"},
	{"VOL2 Source Selector", "SDIN1H", "SDIN1"},
	{"VOL2 Source Selector", "SDIN2", "SDIN2"},
	{"VOL2 Source Selector", "SDIN3", "SDIN3"},
	{"VOL2 Source Selector", "VOL1", "VOL1"},
	{"VOL2 Source Selector", "VOL2", "VOL2"},
	{"VOL2 Source Selector", "VOL3", "VOL3"},
	{"VOL2 Source Selector", "ADC1", "ADC1"},
	{"VOL2 Source Selector", "ADC2", "ADC2"},
	{"VOL2 Source Selector", "ADCM", "ADCM"},
	{"VOL2 Source Selector", "MixerA", "MixerA"},
	{"VOL2 Source Selector", "MixerB", "MixerB"},

	{"VOL3 Source Selector", "SDIN1", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1B", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1C", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1D", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1E", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1F", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1G", "SDIN1"},
	{"VOL3 Source Selector", "SDIN1H", "SDIN1"},
	{"VOL3 Source Selector", "SDIN2", "SDIN2"},
	{"VOL3 Source Selector", "SDIN3", "SDIN3"},
	{"VOL3 Source Selector", "VOL1", "VOL1"},
	{"VOL3 Source Selector", "VOL2", "VOL2"},
	{"VOL3 Source Selector", "VOL3", "VOL3"},
	{"VOL3 Source Selector", "ADC1", "ADC1"},
	{"VOL3 Source Selector", "ADC2", "ADC2"},
	{"VOL3 Source Selector", "ADCM", "ADCM"},
	{"VOL3 Source Selector", "MixerA", "MixerA"},
	{"VOL3 Source Selector", "MixerB", "MixerB"},
};

static int ak4601_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	
	int fsno, nmax;
	int DIODLbit, addr, value, nPortNo;

	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	ak4601->fs = params_rate(params);

	akdbgprt("\t[AK4601] %s fs=%d\n",__FUNCTION__, ak4601->fs );

	DIODLbit = 2;

	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			DIODLbit = 2;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			DIODLbit = 0;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			DIODLbit = 3;
			break;
		default:
			pr_err("%s: invalid Audio format %u\n", __func__, params_format(params));
			return -EINVAL;
	}

	switch(dai->id){
		case AIF_PORT1: 
			nPortNo = 0;
			break;
		case AIF_PORT2:
			nPortNo = 1;
			break;

		default:
			pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
			return -EINVAL;
			break;
	}

	fsno = 0;
	nmax = sizeof(sdfstab) / sizeof(sdfstab[0]);
	akdbgprt("\t[AK4601] %s nmax = %d\n",__FUNCTION__, nmax);

	do {
		if ( ak4601->fs <= sdfstab[fsno] ) break;
		fsno++;
	} while ( fsno < nmax );
	akdbgprt("\t[AK4601] %s fsno = %d\n",__FUNCTION__, fsno);

	if ( fsno == nmax ) {
		pr_err("%s: not support Sampling Frequency : %d\n", __func__, ak4601->fs);
		return -EINVAL;
	}

	akdbgprt("\t[AK4601] %s setSDClock\n",__FUNCTION__);


	ak4601->SDfs[nPortNo] = fsno;
	setSDClock(codec, nPortNo);
#ifdef TCC_CODEC_SAMPLING_FREQ
	setCodecFSMode(codec, nPortNo);
#endif

	/* set Word length */
	value = DIODLbit;

#ifdef AK4601_MULTI_CARD_ENABLE
	/* set SDIN data format */
	addr = AK4601_050_SDIN1_DIGITAL_INPUT_FORMAT + nPortNo;
	snd_soc_update_bits(codec, addr, 0x03, value);

	/* set SDOUT data format */
	addr = AK4601_055_SDOUT1_DIGITAL_OUTPUT_FORMAT + nPortNo;
	snd_soc_update_bits(codec, addr, 0x03, value);
#else
	for(nPortNo = 0; nPortNo <= AIF_PORT2; nPortNo++)
	{
		/* set SDIN data format */
		addr = AK4601_050_SDIN1_DIGITAL_INPUT_FORMAT + nPortNo;
		snd_soc_update_bits(codec, addr, 0x03, value);

		/* set SDOUT data format */
		addr = AK4601_055_SDOUT1_DIGITAL_OUTPUT_FORMAT + nPortNo;
		snd_soc_update_bits(codec, addr, 0x03, value);
	}
#endif

	return 0;
}

static int ak4601_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	return 0;
}

static int ak4601_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_codec *codec = dai->codec;
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int format, diolsb, diedge, doedge,dislot, doslot;
	int msnbit, value;
	int addr, mask, shift, nPortNo;

	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (dai->id) {
		case AIF_PORT1: 
			nPortNo = 0;
			break;
		case AIF_PORT2:
			nPortNo = 1;
			break;

		default:
			pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
			return -EINVAL;
			break;
	}

	/* set master/slave audio interface */
    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBS_CFS:
			msnbit = 0;
			akdbgprt("\t[AK4601] %s(Slave_nPortNo=%d)\n",__FUNCTION__,nPortNo+1);
            break;
        case SND_SOC_DAIFMT_CBM_CFM:
			msnbit = 1;
			akdbgprt("\t[AK4601] %s(Master_nPortNo=%d)\n",__FUNCTION__,nPortNo+1);
            break;
        case SND_SOC_DAIFMT_CBS_CFM:
        case SND_SOC_DAIFMT_CBM_CFS:
        default:
            dev_err(codec->dev, "Clock mode unsupported");
           return -EINVAL;
    }

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			format = AK4601_LRIF_I2S_MODE; // 0
			diolsb = 0;
			diedge = ak4601->DIEDGEbit[nPortNo];
			doedge = ak4601->DOEDGEbit[nPortNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			format = AK4601_LRIF_MSB_MODE; // 5
			diolsb = 0;
			diedge = ak4601->DIEDGEbit[nPortNo];
			doedge = ak4601->DOEDGEbit[nPortNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			format = AK4601_LRIF_LSB_MODE; // 5
			diolsb = 1;
			diedge = ak4601->DIEDGEbit[nPortNo];
			doedge = ak4601->DOEDGEbit[nPortNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_DSP_A: //PCM Short			Might TDM
			format = AK4601_LRIF_PCM_SHORT_MODE; // 6
			diolsb = 0;
			diedge = 1;
			doedge = 1;
			dislot = ak4601->DISLbit[nPortNo];
			doslot = ak4601->DOSLbit[nPortNo];
			break;
		case SND_SOC_DAIFMT_DSP_B: //PCM Long			Might TDM
			format = AK4601_LRIF_PCM_LONG_MODE; // 7
			diolsb = 0;
			diedge = 1;
			doedge = 1;
			dislot = ak4601->DISLbit[nPortNo];
			doslot = ak4601->DOSLbit[nPortNo];
			break;
		default:
			return -EINVAL;
	}

	akdbgprt("\t[AK4601] %s AIF(Sync)%d diolsb=%d diedge=%d doedge=%d dislot=%d doslot=%d \n",__FUNCTION__, nPortNo+1,diolsb, diedge, doedge, dislot, doslot);

	/* set format */
	setSDMaster(codec, nPortNo, msnbit);
	addr = AK4601_04C_CLOCK_FORMAT_SETTING_1 + (nPortNo)/2;
	shift = 4 * ((nPortNo+1) % 2);
	value = format<<shift;
	mask = 0xF << shift;
	snd_soc_update_bits(codec, addr, mask, value);

	/* set SDIO format */
	/* set Slot length */
	addr = AK4601_050_SDIN1_DIGITAL_INPUT_FORMAT + nPortNo;
	value = (diedge <<  7) + (diolsb <<  3) + (dislot <<  4);
	snd_soc_update_bits(codec, addr, 0xB8, value);
	
	addr = AK4601_055_SDOUT1_DIGITAL_OUTPUT_FORMAT + nPortNo;
	value = (doedge <<  7) + (diolsb <<  3) + (doslot <<  4);
	snd_soc_update_bits(codec, addr, 0xB8, value);

	
	return 0;
}

/*
* Read ak4601 register cache
 */
static inline u32 ak4601_read_reg_cache(struct snd_soc_codec *codec, u16 reg)
{
    u8 *cache = codec->reg_cache;
    BUG_ON(reg > ARRAY_SIZE(ak4601_reg));
    return (u32)cache[reg];
}

static unsigned int ak4601_i2c_read(
struct i2c_client *client,
u8 *reg,
int reglen,
u8 *data,
int datalen)
{
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return -ret;
	else
		return -EIO;
}

unsigned int ak4601_read_register(struct snd_soc_codec *codec, unsigned int reg)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[3], rx[1];
	int	wlen, rlen;
	int ret;
	unsigned int rdata;

	wlen = 3;
	rlen = 1;
	tx[0] = (unsigned char)(COMMAND_READ_REG & 0x7F);
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);

#ifdef AK4601_I2C_IF
	ret = ak4601_i2c_read(ak4601->i2c, tx, wlen, rx, rlen);
#else
	ret = spi_write_then_read(ak4601->spi, tx, wlen, rx, rlen);
#endif

	if (ret < 0) {
		akdbgprt("\t[AK4601] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}
	//akdbgprt("[AK4601] %s (%02x, %02x)\n",__FUNCTION__, reg, rdata);
	return rdata;
}

#if 0
static int ak4601_reads(struct snd_soc_codec *codec, u8 *tx, size_t wlen, u8 *rx, size_t rlen)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int ret;
	
	akdbgprt("*****[AK4601] %s tx[0]=0x%02x, raddr=0x%02x%02x, wlen=%d, rlen=%d\n",__FUNCTION__, tx[0],tx[1],tx[2],wlen,rlen);
#ifdef AK4601_I2C_IF
	ret = ak4601_i2c_read(ak4601->i2c, tx, wlen, rx, rlen);
#else
	ret = spi_write_then_read(ak4601->spi, tx, wlen, rx, rlen);
#endif

	return ret;

}
#endif

static inline void ak4601_write_reg_cache(
struct snd_soc_codec *codec, 
u16 reg,
u16 value)
{
    u8 *cache = codec->reg_cache;
    BUG_ON(reg > ARRAY_SIZE(ak4601_reg));
    cache[reg] = (u8)value;
}

static int ak4601_write_register(struct snd_soc_codec *codec,  unsigned int reg,  unsigned int value)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int rc;
	unsigned int value2;

	value2 = ( ((unsigned int)(ak4601_access_masks[reg].writable)) & value ); 
	
	akdbgprt("[AK4601] %s (%02x, %02x)\n",__FUNCTION__, reg, value2);

	wlen = 4;
	tx[0] = (unsigned char)COMMAND_WRITE_REG;
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);
	tx[3] = value2;

#ifdef AK4601_I2C_IF
	rc = i2c_master_send(ak4601->i2c, tx, wlen);
#else
	rc = spi_write(ak4601->spi, tx, wlen);
#endif
	
	if ( rc==0 ) ak4601_write_reg_cache(codec, reg, value2);

	
	return rc;
}

#if 0
static int ak4601_writes(struct snd_soc_codec *codec, const u8 *tx, size_t wlen)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int rc;

	akdbgprt("[AK4601] %s tx[0]=%02x tx[1]=%02x tx[2]=%02x, wlen=%d\n",__FUNCTION__, (int)tx[0], (int)tx[1], (int)tx[2], wlen);

#ifdef AK4601_I2C_IF
	rc = i2c_master_send(ak4601->i2c, tx, wlen);
#else
	rc = spi_write(ak4601->spi, tx, wlen);
#endif

	return rc;
}
#endif

static int ak4601_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (level) {
	case SND_SOC_BIAS_ON:
		akdbgprt("\t[AK4601] SND_SOC_BIAS_ON\n");
		break;
	case SND_SOC_BIAS_PREPARE:
		akdbgprt("\t[AK4601] SND_SOC_BIAS_PREPARE\n");
		break;
	case SND_SOC_BIAS_STANDBY:
		akdbgprt("\t[AK4601] SND_SOC_BIAS_STANDBY\n");
		break;
	case SND_SOC_BIAS_OFF:
		akdbgprt("\t[AK4601] SND_SOC_BIAS_OFF\n");
		break;
	}
	snd_soc_codec_get_dapm(codec)->bias_level = level;

	return 0;
}

static int ak4601_set_dai_mute(struct snd_soc_dai *dai, int mute) 
{
	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	if (mute) {
		akdbgprt("\t[AK4601] Mute ON(%d)\n",__LINE__);
	} else {
		akdbgprt("\t[AK4601] Mute OFF(%d)\n",__LINE__);
	}

	return 0;
}

#define AK4601_RATES			SNDRV_PCM_RATE_8000_192000	

#define AK4601_DAC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE
#define AK4601_ADC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE

int ak4601_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(dai->codec);
	int i;

	if (slots == 0) {
		for ( i = 0 ; i < NUM_SDIO ; i ++ ) {
			ak4601->TDMSDINbit[i]  = 0;	//TDM Off
			ak4601->TDMSDOUTbit[i] = 0;	//TDM Off
			ak4601->DIEDGEbit[i] = 0;	//I2S
			ak4601->DOEDGEbit[i] = 0;	//I2S
			ak4601->DISLbit[i] = 0;		//24bit
			ak4601->DOSLbit[i] = 0;		//24bit
		}
	} else {
		for ( i = 0 ; i < NUM_SDIO ; i ++ ) {
			ak4601->TDMSDINbit[i]  = 1; //TDM On
			ak4601->TDMSDOUTbit[i] = 1; //TDM On
			ak4601->DIEDGEbit[i] = 1;
			ak4601->DOEDGEbit[i] = 1;
			ak4601->DISLbit[i] = (slot_width == 24) ? 0 : (slot_width == 20) ? 1 : (slot_width == 16) ? 2 : 3;
			ak4601->DOSLbit[i] = (slot_width == 24) ? 0 : (slot_width == 20) ? 1 : (slot_width == 16) ? 2 : 3;
		}
	}

	return 0;
}


static struct snd_soc_dai_ops ak4601_dai_ops = {
	.hw_params	= ak4601_hw_params,
	.set_sysclk	= ak4601_set_dai_sysclk,
	.set_fmt	= ak4601_set_dai_fmt,
	.digital_mute = ak4601_set_dai_mute,
	.set_tdm_slot = ak4601_set_tdm_slot,
};

struct snd_soc_dai_driver ak4601_dai[] = {   
	{										 
		.name = "ak4601-aif1",
		.id = AIF_PORT1,
		.playback = {
		       .stream_name = "AIF1 Playback",
		       .channels_min = 1,
		       .channels_max = 16,
		       .rates = AK4601_RATES,
		       .formats = AK4601_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF1 Capture",
		       .channels_min = 1,
		       .channels_max = 16,
		       .rates = AK4601_RATES,
		       .formats = AK4601_ADC_FORMATS,
		},
		.ops = &ak4601_dai_ops,
	},
	{
		.name = "ak4601-aif2",
		.id = AIF_PORT2,
		.playback = {
		       .stream_name = "AIF2 Playback",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK4601_RATES,
		       .formats = AK4601_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF2 Capture",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK4601_RATES,
		       .formats = AK4601_ADC_FORMATS,
		},
		.ops = &ak4601_dai_ops,
	},
};

#ifndef AK4601_I2C_IF
static int ak4601_write_spidmy(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int rd;

	wlen = 4;
	tx[0] = (unsigned char)(0xDE);
	tx[1] = (unsigned char)(0xAD);
	tx[2] = (unsigned char)(0xDA);
	tx[3] = (unsigned char)(0x7A);

#ifdef AK4601_I2C_IF
	rd - -EIO;
#else
	rd = spi_write(ak4601->spi, tx, wlen);
#endif
	
	return rd;
}
#endif

static int ak4601_init_reg(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int i;
	
	akdbgprt("\t[AK4601] %s\n",__FUNCTION__);

	if ( ak4601->pdn_gpio > 0 ) { 
		gpio_set_value(ak4601->pdn_gpio, 0);	
		msleep(1);
		gpio_set_value(ak4601->pdn_gpio, 1);	
		msleep(1);
	}
#ifndef AK4601_I2C_IF
	ak4601_write_spidmy(codec);
#endif

	printk("[AK4601] %s Check Connection...%s\n", __FUNCTION__, snd_soc_read(codec,AK4601_075_VOL1LCH_DIGITAL_VOLUME) == 0x18 ? "SUCCESS" : "FAILUE");

	ak4601_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	
	ak4601->fs = 48000;
	ak4601->PLLInput = 0;//XTI
	ak4601->XtiFs    = 0;

	setPLLOut(codec);

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak4601->SDBick[i]  = 0; //64fs
		ak4601->SDfs[i] = 5;  // 48kHz
		ak4601->SDCks[i] = 1;  // PLLMCLK

		setSDClock(codec, i);
	}

	for ( i = 0 ; i < NUM_SDIO ; i ++ ) {
		ak4601->TDMSDINbit[i]  = 0; //TDM Off
		ak4601->TDMSDOUTbit[i] = 0; //TDM Off
		ak4601->DIEDGEbit[i] = 0;   //I2S
		ak4601->DOEDGEbit[i] = 0;   //I2S
		ak4601->DISLbit[i] = 0;	    //24bit
		ak4601->DOSLbit[i] = 0;		//24bit
		ak4601->EXBCK[i] = i+1;		//
	}

	ak4601->DIDL3 = 0;
	ak4601->EXBCK[2] = PORT3_LINK;

	snd_soc_update_bits( ak4601->codec, AK4601_016_SYNC_DOMAIN_SELECT_6, 0x77, ((ak4601->EXBCK[0]<<4) + (ak4601->EXBCK[1])));
	snd_soc_update_bits( ak4601->codec, AK4601_017_SYNC_DOMAIN_SELECT_7, 0x70, ak4601->EXBCK[2]<<4 );


	akdbgprt("\t[AK4601] %s\n addr=%03X data=%02X",__FUNCTION__, AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 0x38);

	snd_soc_update_bits( ak4601->codec, AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 0x38, 0x38 );//SDOUT1~3 OutputEnable=1
#ifdef AK4601_MULTI_CARD_ENABLE
	snd_soc_update_bits( ak4601->codec, AK4601_013_SYNC_DOMAIN_SELECT_3, 0x07, 0x01);
	snd_soc_update_bits( ak4601->codec, AK4601_014_SYNC_DOMAIN_SELECT_4, 0x77, 0x23);
#else
	snd_soc_update_bits( ak4601->codec, AK4601_013_SYNC_DOMAIN_SELECT_3, 0x07, 0x01);
	snd_soc_update_bits( ak4601->codec, AK4601_014_SYNC_DOMAIN_SELECT_4, 0x77, 0x11);
#endif

	snd_soc_update_bits( ak4601->codec, AK4601_011_SYNC_DOMAIN_SELECT_1, 0x12, 0x12 );//LR/BICKx Output SDx

	return 0;
}

#ifdef CONFIG_TCC803X_CA7S	
static int ak4601_init_reg_for_a7s(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int i;
	
	akdbgprt("\t[AK4601] %s\n",__FUNCTION__);

	printk("[AK4601] %s Check Connection...%s\n", __FUNCTION__, snd_soc_read(codec,AK4601_075_VOL1LCH_DIGITAL_VOLUME) == 0x18 ? "SUCCESS" : "FAILUE");

	ak4601->fs = 48000;
	ak4601->PLLInput = 1; //BICK1
	ak4601->XtiFs    = 0;

	setPLLOut(codec);

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak4601->SDBick[i]  = 0; //64fs
		ak4601->SDfs[i] = 5;  // 48kHz
		ak4601->SDCks[i] = 1;  // PLLMCLK

		setSDClock(codec, i);
	}

	for ( i = 0 ; i < NUM_SDIO ; i ++ ) {
		ak4601->TDMSDINbit[i]  = 0; //TDM Off
		ak4601->TDMSDOUTbit[i] = 0; //TDM Off
		ak4601->DIEDGEbit[i] = 0;   //I2S
		ak4601->DOEDGEbit[i] = 0;   //I2S
		ak4601->DISLbit[i] = 2;	    //0:24bit, 2:16bit
		ak4601->DOSLbit[i] = 2;		//0:24bit, 2:16bit
		ak4601->EXBCK[i] = i+1;		//
	}

	ak4601->DIDL3 = 0;
	ak4601->EXBCK[2] = PORT3_LINK;

	snd_soc_update_bits( ak4601->codec, AK4601_016_SYNC_DOMAIN_SELECT_6, 0x77, ((ak4601->EXBCK[0]<<4) + (ak4601->EXBCK[1])));
	snd_soc_update_bits( ak4601->codec, AK4601_017_SYNC_DOMAIN_SELECT_7, 0x70, ak4601->EXBCK[2]<<4 );
	akdbgprt("\t[AK4601] %s\n addr=%03X data=%02X",__FUNCTION__, AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 0x38);

	snd_soc_update_bits( ak4601->codec, AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 0x38, 0x38 );//SDOUT1~3 OutputEnable=1
	//'CODEC Sync Domain' 'SD1', 'ADC1 Sync Domain' 'SD1'
	snd_soc_update_bits( ak4601->codec, AK4601_020_SYNC_DOMAIN_SELECT_16, 0x77, 0x11);
	//'MixerB Sync Domain' 'SD1'
	snd_soc_update_bits( ak4601->codec, AK4601_01F_SYNC_DOMAIN_SELECT_15, 0x07, 0x01);
	//'LRCK2/SDOUT3 Pin BICK2/SDIN3 Pin Setting' 'SDIN3,SDOUT3 Enable'
	snd_soc_update_bits( ak4601->codec, AK4601_010_PIN_SETTING, 0x01, 0x01);
	//'ADCM MUX' 'AINM'
	//'AIN1 Lch MUX' 'IN1P_N'
	//'AIN1 Rch MUX' 'IN2P_N'
	snd_soc_update_bits( ak4601->codec, AK4601_06B_ANALOG_INPUT_SELECT_SETTING, 0x1C, 0x10);
	//'MicBias1 MUX' 'MicBias1'
	//'MicBias2 MUX' 'MicBias2'
	snd_soc_update_bits( ak4601->codec, AK4601_002_MIC_BIAS_POWER_MANAGEMENT, 0xFF, 0x03);
	//'ADC1 Digital Volume L' 255
	snd_soc_update_bits( ak4601->codec, AK4601_064_ADC1LCH_DIGITAL_VOLUME, 0xFF, 0x00);
	//'ADC1 Digital Volume R' 0
	snd_soc_update_bits( ak4601->codec, AK4601_065_ADC1RCH_DIGITAL_VOLUME, 0xFF, 0xFF);
	//'MixerB Ch1 Source Selector' 'ADC1'
	snd_soc_update_bits( ak4601->codec, AK4601_047_MIXER_B_CH1_INPUT_DATA_SELECT, 0x3F, 0x15);
	//'MixerB Ch2 Source Selector' 'ADCM'
	snd_soc_update_bits( ak4601->codec, AK4601_048_MIXER_B_CH2_INPUT_DATA_SELECT, 0x3F, 0x17);
	//'MixerB Input2 Data Change' 'Swap'
	snd_soc_update_bits( ak4601->codec, AK4601_061_MIXER_B_SETTING, 0x0c, 0x0c);
	//'SDOUT1 Source Selector' 'MixerB'
	snd_soc_update_bits( ak4601->codec, AK4601_021_SDOUT1_TDM_SLOT1_2_DATA_SELECT, 0x3F, 0x19);
	//'SDOUT1 Output Enable' on        
	snd_soc_update_bits( ak4601->codec, AK4601_05E_OUTPUT_PORT_ENABLE_SETTING, 0x20, 0x20);
	//'ADC1 Mute' off                  
	//'ADCM Mute' off                  
	snd_soc_update_bits( ak4601->codec, AK4601_06C_ADC_MUTE_AND_HPF_CONTROL, 0x50, 0x00);
	//'DAC1 Source Selector' 'SDIN1'   
	//'DAC2 Source Selector' 'SDIN2'   
	//'DAC3 Source Selector' 'SDIN1'  
	snd_soc_update_bits( ak4601->codec, AK4601_035_DAC1_INPUT_DATA_SELECT, 0x3F, 0x01);
	snd_soc_update_bits( ak4601->codec, AK4601_036_DAC2_INPUT_DATA_SELECT, 0x3F, 0x09);
	snd_soc_update_bits( ak4601->codec, AK4601_037_DAC3_INPUT_DATA_SELECT, 0x3F, 0x01);
	//'DAC1 Mute' off    
	//'DAC2 Mute' off                  
	//'DAC3 Mute' off                  
	snd_soc_update_bits( ak4601->codec, AK4601_073_DAC_MUTE_AND_FILTER_SETTING, 0x70, 0x00);

	return 0;
}
#endif

#ifdef CONFIG_OF	//16/04/11
static int ak4601_parse_dt(struct ak4601_priv *ak4601)
{
	struct device *dev;
	struct device_node *np;

#ifdef AK4601_I2C_IF
	dev = &(ak4601->i2c->dev);
#else
	dev = &(ak4601->spi->dev);
#endif

	np = dev->of_node;

	if(!np)
		return -1;

	printk("[AK4601] Read PDN pin from device tree\n");

	ak4601->pdn_gpio = of_get_named_gpio(np, "ak4601,pdn-gpio", 0);
	if(ak4601->pdn_gpio < 0){
		ak4601->pdn_gpio = -1;
		return -1;
	}

	if( !gpio_is_valid(ak4601->pdn_gpio) ){
		printk(KERN_ERR "[AK4601] pdn pin(%u) is invalid\n", ak4601->pdn_gpio);
		return -1;
	}
	return 0;
}
#endif

static int ak4601_probe(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
#ifndef CONFIG_OF
	struct ak4601_platform_data *pdata = codec->dev->platform_data;
#endif
	int ret = 0;
	printk("[AK4601] probe!!\n");

	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

#if 0
	ret = snd_soc_codec_set_cache_io(codec, 16, 8, ak4601->control_type);
	akdbgprt("\t[AK4601] %s : ret = %d\n",__FUNCTION__, ret);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
#endif

	ak4601->codec = codec;
#ifdef CONFIG_OF
	ret = ak4601_parse_dt(ak4601);
	if ( ret < 0 ) {
		ak4601->pdn_gpio = -1;
		ret=0;
	}
#else
	if ( pdata != NULL ) {
		ak4601->pdn_gpio = pdata->pdn_gpio;
	}
#endif
	if ( ak4601->pdn_gpio > 0 ) { 
		ret = gpio_request(ak4601->pdn_gpio, "ak4601 pdn");
		gpio_direction_output(ak4601->pdn_gpio, 0);
	}

	ak4601_init_reg(codec);
#ifdef CONFIG_TCC803X_CA7S	
	ak4601_init_reg_for_a7s(codec);
#endif
	akdbgprt("\t[AK4601] %s return(%d)\n",__FUNCTION__, ret );
	return ret;
}

static int ak4601_remove(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	ak4601_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if ( ak4601->pdn_gpio > 0 ) {
		gpio_set_value(ak4601->pdn_gpio, 0);
		msleep(1);
		gpio_free(ak4601->pdn_gpio);
		msleep(1);
	}
	return 0;
}

static void ak4601_backup_regs(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int i;

	for (i=0; i<ARRAY_SIZE(ak4601_backup_reg_list); i++) {
		ak4601->reg_backup[i] = snd_soc_read(codec, ak4601_backup_reg_list[i]);
	}
}

static void ak4601_restore_regs(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);
	int i;

	for (i=0; i<ARRAY_SIZE(ak4601_backup_reg_list); i++) {
		snd_soc_write(codec, ak4601_backup_reg_list[i], ak4601->reg_backup[i]);
	}
}

static int ak4601_suspend(struct snd_soc_codec *codec) //16/04/11
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);
	ak4601_set_bias_level(codec, SND_SOC_BIAS_OFF);

	ak4601_backup_regs(codec);

	if ( ak4601->pdn_gpio > 0 ) { 
		gpio_set_value(ak4601->pdn_gpio, 0);
	}
	return 0;
}

static int ak4601_resume(struct snd_soc_codec *codec)
{
	struct ak4601_priv *ak4601 = snd_soc_codec_get_drvdata(codec);

//	ak4601_init_reg(codec);
	if ( ak4601->pdn_gpio > 0 ) { 
		gpio_set_value(ak4601->pdn_gpio, 1);
	}	
	ak4601_restore_regs(codec);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_ak4601 = {
	.probe = ak4601_probe,
	.remove = ak4601_remove,
	.suspend =	ak4601_suspend,
	.resume =	ak4601_resume,

	.component_driver = {
		.controls = ak4601_snd_controls,
		.num_controls = ARRAY_SIZE(ak4601_snd_controls),
		.dapm_widgets = ak4601_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ak4601_dapm_widgets),
		.dapm_routes = ak4601_intercon,
		.num_dapm_routes = ARRAY_SIZE(ak4601_intercon),
	},

	.read = ak4601_read_register,
	.write = ak4601_write_register,

	//.idle_bias_off = true,	//16/04/11
	.set_bias_level = ak4601_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(ak4601_reg),
	.reg_word_size = sizeof(u8),
	.reg_cache_default = ak4601_reg,
};


EXPORT_SYMBOL_GPL(soc_codec_dev_ak4601);
#ifdef CONFIG_OF  // 16/04/11
static struct of_device_id ak4601_if_dt_ids[] = {
	{ .compatible = "akm,ak4601"},
    { }
};
MODULE_DEVICE_TABLE(of, ak4601_if_dt_ids);
#endif

#ifdef AK4601_I2C_IF

static int ak4601_i2c_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *id)
{
	struct ak4601_priv *ak4601;
	int ret=0;
	
	akdbgprt("\t[AK4601] %s(%d)\n",__FUNCTION__,__LINE__);

	ak4601 = kzalloc(sizeof(struct ak4601_priv), GFP_KERNEL);
	if (ak4601 == NULL) return -ENOMEM;

	i2c_set_clientdata(i2c, ak4601);

	ak4601->i2c = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak4601, &ak4601_dai[0], ARRAY_SIZE(ak4601_dai));
	if (ret < 0){
		kfree(ak4601);
		akdbgprt("\t[AK4601 Error!] %s(%d)\n",__FUNCTION__,__LINE__);
		printk("[AK4601] snd_soc_resister_codec error!!\n");
	}
	return ret;
}

static int ak4601_i2c_remove(struct i2c_client *client)	//16/04/11
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id ak4601_i2c_id[] = {
	{ "ak4601", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4601_i2c_id);

static struct i2c_driver ak4601_i2c_driver = {
	.driver = {
		.name = "ak4601",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ak4601_if_dt_ids),
#endif
	},
	.probe = ak4601_i2c_probe,
	.remove = ak4601_i2c_remove,	//16/04/11
	.id_table = ak4601_i2c_id,
};

#else

static int ak4601_spi_probe(struct spi_device *spi)	//16/04/11
{
	struct ak4601_priv *ak4601;
	int	ret;

	akdbgprt("\t[AK4601] %s spi=%x\n",__FUNCTION__, (int)spi);	

	ak4601 = kzalloc(sizeof(struct ak4601_priv), GFP_KERNEL);
	if (!ak4601)
		return -ENOMEM;

	ak4601->spi = spi;

	spi_set_drvdata(spi, ak4601);

	ret = snd_soc_register_codec(&spi->dev,
			&soc_codec_dev_ak4601,  &ak4601_dai[0], ARRAY_SIZE(ak4601_dai));
	akdbgprt("\t[AK4601] %s ret=%d\n",__FUNCTION__, ret);
	if (ret < 0) {
		kfree(ak4601);
		akdbgprt("\t[AK4601 Error!] %s(%d)\n",__FUNCTION__,__LINE__);
	}

	return 0;
}

static int ak4601_spi_remove(struct spi_device *spi)	// 16/04/11
{
	kfree(spi_get_drvdata(spi));
	return 0;
}

static struct spi_driver ak4601_spi_driver = {
	.driver = {
		.name = "ak4601",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ak4601_if_dt_ids),
#endif
	},
	.probe = ak4601_spi_probe,
	.remove = ak4601_spi_remove,
};
#endif

static int __init ak4601_modinit(void)
{
	int ret = 0;

	akdbgprt("\t[AK4601] %s(%d)\n", __FUNCTION__,__LINE__);

#ifdef AK4601_I2C_IF
	ret = i2c_add_driver(&ak4601_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "[AK4601] Failed to register I2C driver: %d\n", ret);

	}
	else
	{
		printk("[AK4601] resister ak4601 i2c driver\n");
	}
#else
	ret = spi_register_driver(&ak4601_spi_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "[AK4601] Failed to register SPI driver: %d\n",  ret);

	}
#endif

	return ret;
}

module_init(ak4601_modinit);

static void __exit ak4601_exit(void)
{
#ifdef AK4601_I2C_IF
	i2c_del_driver(&ak4601_i2c_driver);
#else
	spi_unregister_driver(&ak4601_spi_driver);
#endif


}
module_exit(ak4601_exit);

MODULE_DESCRIPTION("ASoC ak4601 codec driver");
MODULE_LICENSE("GPL");

