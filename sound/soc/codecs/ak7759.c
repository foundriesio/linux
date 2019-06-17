/*
 * ak7759.c  --  audio driver for AK7759
 *
 * Copyright (C) 2018 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      18/02/21        1.0 Kernel 4_4_XX
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

#include <linux/regmap.h>
#include <linux/of_gpio.h>

#include "ak7759.h"
#include "ak7759_dsp_code.h"

//#define KERNEL_3_18_XX
#define KERNEL_4_4_XX
//#define KERNEL_4_9_XX

//#define AK7759_DEBUG			//used at debug mode

#ifdef AK7759_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif


#define INIT_REG_BASICRAM

/* AK7759 Codec Private Data */
struct ak7759_priv {
	enum snd_soc_control_type control_type;
	struct snd_soc_codec *codec;
	struct spi_device *spi;
	struct i2c_client *i2c;
	struct regmap *regmap;
	int fs;
	
	int pdn_gpio;
	int MIRNo;
	int status;

	int MicGainL;
	int MicGainR;

	int DSPPramMode;
	int DSPCramMode;
	int DSPOfregMode;

	int PLLInput;  // 0 : XTI,   1 : BICK
	int XtiFs;     // 0 : 12.288MHz,  1: 18.432MHz
	int BUSMclk;   // 0 : 24.576MHz

	int SDfs[NUM_SYNCDOMAIN];     // 0:8kHz, 1:12kHz, 2:16kHz, 3:24kHz, 4:32kHz, 5:48kHz, 6:96kHz, 7:192kHz
	int SDBick[NUM_SYNCDOMAIN];   // 0:64fs, 1:48fs, 2:32fs, 3:128fs, 4:256fs

	int Master[NUM_SYNCDOMAIN];  // 0 : Slave Mode, 1 : Master
	int SDCks[NUM_SYNCDOMAIN];   // 0 : Low,  1: PLLMCLK

	int TDMbit; //0 : TDM off, 1 : TDM on
	int DIOEDGEbit;
	int DIOSLbit;

	int DSPon;

#ifdef AK7759_DEBUG
	unsigned char regvalue[3];
	unsigned char cramvalue[5];
#endif

};

struct _ak7759_pd_handler {
	int ref_count;
	struct mutex lock;
	struct ak7759_priv *data;
} ak7759_pd_handler = {
	.ref_count = -1,
	.data = NULL,
};

/* ak7759 register cache & default register settings */
static const struct reg_default ak7759_reg[] = {
  { 0x00, 0x00 },  /* AK7759_00_SYSCLOCK_SETTING1 */
  { 0x01, 0x10 },  /* AK7759_01_SYSCLOCK_SETTING2 */
  { 0x02, 0x05 },  /* AK7759_02_BUSCLOCK_SETTING */
  { 0x03, 0x00 },  /* AK7759_03_MASTER_SLAVE_SETTING */
  { 0x04, 0x08 },  /* AK7759_04_SYNCDOMAIN1_SETTING1 */
  { 0x05, 0x00 },  /* AK7759_05_SYNCDOMAIN1_SETTING2 */
  { 0x06, 0x08 },  /* AK7759_06_SYNCDOMAIN2_SETTING1 */
  { 0x07, 0x00 },  /* AK7759_07_SYNCDOMAIN2_SETTING2 */
  { 0x08, 0x00 },  /* AK7759_08_SYNCPORT_SELECT */
  { 0x09, 0x00 },  /* AK7759_09_SYNCDOMAIN_SELECT1 */
  { 0x0A, 0x00 },  /* AK7759_0A_SYNCDOMAIN_SELECT2 */
  { 0x0B, 0x00 },  /* AK7759_0B_SYNCDOMAIN_SELECT3 */
  { 0x0C, 0x00 },  /* AK7759_0C_SYNCDOMAIN_SELECT4 */
  { 0x0D, 0x00 },  /* AK7759_0D_SYNCDOMAIN_SELECT5 */
  { 0x0E, 0x00 },  /* AK7759_0R_SYNCDOMAIN_SELECT6 */
  { 0x0F, 0x00 },  /* AK7759_0F_SYNCDOMAIN_SELECT7 */
  { 0x10, 0x00 },  /* AK7759_10_SDOUT1A_OUTPUT_SELECT */
  { 0x11, 0x00 },  /* AK7759_11_SDOUT1B_OUTPUT_SELECT */
  { 0x12, 0x00 },  /* AK7759_12_SDOUT1C_OUTPUT_SELECT */
  { 0x13, 0x00 },  /* AK7759_13_SDOUT1D_OUTPUT_SELECT */
  { 0x14, 0x00 },  /* AK7759_14_SDOUT2_OUTPUT_SELECT */
  { 0x15, 0x00 },  /* AK7759_15_SDOUT3_OUTPUT_SELECT */
  { 0x16, 0x00 },  /* AK7759_16_DSP_IN1_INPUT_SELECT */
  { 0x17, 0x00 },  /* AK7759_17_DSP_IN2_INPUT_SELECT */
  { 0x18, 0x00 },  /* AK7759_18_DSP_IN3_INPUT_SELECT */
  { 0x19, 0x00 },  /* AK7759_19_DSP_IN4_INPUT_SELECT */
  { 0x1A, 0x00 },  /* AK7759_1A_DAC_INPUT_SELECT */
  { 0x1B, 0x00 },  /* AK7759_1B_MIXER_CH1_INPUT_SELECT */
  { 0x1C, 0x00 },  /* AK7759_1C_MIXER_CH2_INPUT_SELECT */
  { 0x1D, 0x00 },  /* AK7759_1D_DIT_INPUT_SELECT */
  { 0x1E, 0x00 },  /* Reserved */
  { 0x1F, 0x00 },  /* Reserved */
  { 0x20, 0x00 },  /* AK7759_20_CLOCK_FORMAT_SETTING */
  { 0x21, 0x00 },  /* AK7759_21_SDIN1_INPUT_FORMAT */
  { 0x22, 0x00 },  /* AK7759_22_SDIN2_INPUT_FORMAT */
  { 0x23, 0x00 },  /* AK7759_23_SDOUT1_OUTPUT_FORMAT */
  { 0x24, 0x00 },  /* AK7759_24_SDOUT2_OUTPUT_FORMAT */
  { 0x25, 0x00 },  /* AK7759_25_SDOUT3_OUTPUT_FORMAT */
  { 0x26, 0x00 },  /* Reserved */
  { 0x27, 0x00 },  /* Reserved */
  { 0x28, 0x00 },  /* AK7759_28_INPUT_DATA_SELECT */
  { 0x29, 0x00 },  /* AK7759_29_OUTPUT_DATA_SELECT1 */
  { 0x2A, 0x00 },  /* AK7759_2A_OUTPUT_DATA_SELECT1 */
  { 0x2B, 0x00 },  /* AK7759_2B_CLKO_SETTING */
  { 0x2C, 0x20 },  /* AK7759_2C_OUTPUT_ENABLE */
  { 0x2D, 0x00 },  /* AK7759_2D_STO_SETTING */
  { 0x2E, 0x00 },  /* Reserved */
  { 0x2F, 0x00 },  /* Reserved */
  { 0x30, 0x00 },  /* AK7759_30_DSP_SETTING1 */
  { 0x31, 0x00 },  /* AK7759_31_DSP_SETTING2 */
  { 0x32, 0x00 },  /* AK7759_32_DSP_JX_ENABLE */
  { 0x33, 0x00 },  /* Reserved */
  { 0x34, 0x00 },  /* AK7759_34_DSP_DATA_LENGTH */
  { 0x35, 0x00 },  /* AK7759_35_DSP_JX_FILTER_SETTING */
  { 0x36, 0x00 },  /* AK7759_36_DSP_SHIFT_SETTING */
  { 0x37, 0x00 },  /* Reserved */
  { 0x38, 0x00 },  /* AK7759_38_MIXER_SETTING */
  { 0x39, 0x00 },  /* AK7759_39_DIT_STATUS1 */
  { 0x3A, 0x00 },  /* AK7759_3A_DIT_STATUS1 */
  { 0x3B, 0x04 },  /* AK7759_3B_DIT_STATUS1 */
  { 0x3C, 0x02 },  /* AK7759_3C_DIT_STATUS1 */
  { 0x3D, 0x00 },  /* Reserved */
  { 0x3E, 0x00 },  /* Reserved */
  { 0x3F, 0x00 },  /* Reserved */
  { 0x40, 0x00 },  /* AK7759_40_ADC_SETTING */
  { 0x41, 0x00 },  /* AK7759_41_DMIC_SETTING */
  { 0x42, 0x00 },  /* AK7759_42_DMIC_CLOCK_SETTING */
  { 0x43, 0x00 },  /* AK7759_43_DAC_SETTING */
  { 0x44, 0x22 },  /* AK7759_44_MIC_GAIN */
  { 0x45, 0x30 },  /* AK7759_45_ADC_LCH_DIGITAL_VOLUME */
  { 0x46, 0x30 },  /* AK7759_46_ADC_RCH_DIGITAL_VOLUME */
  { 0x47, 0x30 },  /* AK7759_47_DMIC_LCH_DIGITAL_VOLUME */
  { 0x48, 0x30 },  /* AK7759_48_DMIC_RCH_DIGITAL_VOLUME */
  { 0x49, 0x18 },  /* AK7759_49_DAC_LCH_DIGITAL_VOLUME */
  { 0x4A, 0x18 },  /* AK7759_4A_DAC_RCH_DIGITAL_VOLUME */
  { 0x4B, 0x00 },  /* AK7759_4B_ADC_DAC_MUTE */
  { 0x4C, 0x00 },  /* AK7759_4C_AIN_SW */
  { 0x4D, 0x00 },  /* AK7759_4D_OUT_SW */
  { 0x4E, 0x00 },  /* Reserved */
  { 0x4F, 0x00 },  /* Reserved */
  { 0x50, 0x00 },  /* AK7759_50_POWER_MANAGEMENT */
  { 0x51, 0x00 },  /* AK7759_51_RESET_CONTROL */
  { 0x52, 0x00 },  /* Reserved */
  { 0x53, 0x00 },  /* Reserved */
  { 0x54, 0x00 },  /* Reserved */
  { 0x55, 0x00 },  /* Reserved */
  { 0x56, 0x00 },  /* Reserved */
  { 0x57, 0x00 },  /* Reserved */
  { 0x58, 0x00 },  /* Reserved */
  { 0x59, 0x00 },  /* Reserved */
  { 0x5A, 0x00 },  /* Reserved */
  { 0x5B, 0x00 },  /* Reserved */
  { 0x5C, 0x00 },  /* Reserved */
  { 0x5D, 0x00 },  /* Reserved */
  { 0x5E, 0x00 },  /* Reserved */
  { 0x5F, 0x00 },  /* Reserved */
  { 0x60, 0x00 },  /* Reserved */
  { 0x61, 0x00 },  /* Reserved */
  { 0x62, 0x00 },  /* Reserved */
  { 0x63, 0x00 },  /* Reserved */
  { 0x64, 0x00 },  /* Reserved */
  { 0x65, 0x00 },  /* Reserved */
  { 0x66, 0x00 },  /* Reserved */
  { 0x67, 0x00 },  /* Reserved */
  { 0x68, 0x00 },  /* Reserved */
  { 0x69, 0x00 },  /* Reserved */
  { 0x6A, 0x00 },  /* Reserved */
  { 0x6B, 0x00 },  /* Reserved */
  { 0x6C, 0x00 },  /* Reserved */
  { 0x6D, 0x00 },  /* Reserved */
  { 0x6E, 0x00 },  /* Reserved */
  { 0x6F, 0x00 },  /* Reserved */
  { 0x70, 0x00 },  /* Reserved */
  { 0x71, 0x00 },  /* Reserved */
  { 0x72, 0x00 },  /* Reserved */
  { 0x73, 0x00 },  /* Reserved */
  { 0x74, 0x00 },  /* Reserved */
  { 0x75, 0x00 },  /* Reserved */
  { 0x76, 0x00 },  /* Reserved */
  { 0x77, 0x00 },  /* Reserved */
  { 0x78, 0x00 },  /* Reserved */
  { 0x79, 0x00 },  /* Reserved */
  { 0x7A, 0x00 },  /* Reserved */
  { 0x7B, 0x00 },  /* Reserved */
  { 0x7C, 0x00 },  /* Reserved */
  { 0x7D, 0x00 },  /* Reserved */
  { 0x7E, 0x00 },  /* Reserved */
  { 0x7F, 0x00 },  /* Reserved */
  { 0x80, 0x00 },  /* AK7759_80_DEVNO */
  { 0x81, 0x00 },  /* AK7759_81_REVISION_NUM */
  { 0x82, 0x00 },  /* AK7759_82_ERROR_STATUS */
  { 0x83, 0x00 },  /* AK7759_83_MIC_GAIN_READOUT */
  { 0x84, 0x00 },  /* AK7759_VIRT_84_DSP1OUT1_MIX */
  { 0x85, 0x00 },  /* AK7759_VIRT_85_DSP1OUT2_MIX */
  { 0x86, 0x00 },  /* AK7759_VIRT_86_DSP1OUT3_MIX */
  { 0x87, 0x00 },  /* AK7759_VIRT_87_DSP1OUT4_MIX */
};

static const struct {
	int readable;   /* Mask of readable bits */
	int writable;   /* Mask of writable bits */
} ak7759_access_masks[] = {
  { 0xFF, 0xFF },  // 0x0
  { 0xFF, 0xFF },  // 0x1
  { 0xFF, 0xFF },  // 0x2
  { 0xFF, 0xFF },  // 0x3
  { 0xFF, 0xFF },  // 0x4
  { 0xFF, 0xFF },  // 0x5
  { 0xFF, 0xFF },  // 0x6
  { 0xFF, 0xFF },  // 0x7
  { 0xFF, 0xFF },  // 0x8
  { 0xFF, 0xFF },  // 0x9
  { 0xFF, 0xFF },  // 0xA
  { 0xFF, 0xFF },  // 0xB
  { 0xFF, 0xFF },  // 0xC
  { 0xFF, 0xFF },  // 0xD
  { 0xFF, 0xFF },  // 0xE
  { 0xFF, 0xFF },  // 0xF
  { 0xFF, 0xFF },  // 0x10
  { 0xFF, 0xFF },  // 0x11
  { 0xFF, 0xFF },  // 0x12
  { 0xFF, 0xFF },  // 0x13
  { 0xFF, 0xFF },  // 0x14
  { 0xFF, 0xFF },  // 0x15
  { 0xFF, 0xFF },  // 0x16
  { 0xFF, 0xFF },  // 0x17
  { 0xFF, 0xFF },  // 0x18
  { 0xFF, 0xFF },  // 0x19
  { 0xFF, 0xFF },  // 0x1A
  { 0xFF, 0xFF },  // 0x1B
  { 0xFF, 0xFF },  // 0x1C
  { 0xFF, 0xFF },  // 0x1D
  { 0xFF, 0x00 },  // 0x1E
  { 0xFF, 0x00 },  // 0x1F
  { 0xFF, 0xFF },  // 0x20
  { 0xFF, 0xFF },  // 0x21
  { 0xFF, 0xFF },  // 0x22
  { 0xFF, 0xFF },  // 0x23
  { 0xFF, 0xFF },  // 0x24
  { 0xFF, 0xFF },  // 0x25
  { 0xFF, 0x00 },  // 0x26
  { 0xFF, 0x00 },  // 0x27
  { 0xFF, 0xFF },  // 0x28
  { 0xFF, 0xFF },  // 0x29
  { 0xFF, 0xFF },  // 0x2A
  { 0xFF, 0xFF },  // 0x2B
  { 0xFF, 0xFF },  // 0x2C
  { 0xFF, 0xFF },  // 0x2D
  { 0xFF, 0x00 },  // 0x2E
  { 0xFF, 0x00 },  // 0x2F
  { 0xFF, 0xFF },  // 0x30
  { 0xFF, 0xFF },  // 0x31
  { 0xFF, 0xFF },  // 0x32
  { 0xFF, 0x00 },  // 0x33
  { 0xFF, 0xFF },  // 0x34
  { 0xFF, 0xFF },  // 0x35
  { 0xFF, 0xFF },  // 0x36
  { 0xFF, 0x00 },  // 0x37
  { 0xFF, 0xFF },  // 0x38
  { 0xFF, 0xFF },  // 0x39
  { 0xFF, 0xFF },  // 0x3A
  { 0xFF, 0xFF },  // 0x3B
  { 0xFF, 0xFF },  // 0x3C
  { 0xFF, 0x00 },  // 0x3D
  { 0xFF, 0x00 },  // 0x3E
  { 0xFF, 0x00 },  // 0x3F
  { 0xFF, 0xFF },  // 0x40
  { 0xFF, 0xFF },  // 0x41
  { 0xFF, 0xFF },  // 0x42
  { 0xFF, 0xFF },  // 0x43
  { 0xFF, 0xFF },  // 0x44
  { 0xFF, 0xFF },  // 0x45
  { 0xFF, 0xFF },  // 0x46
  { 0xFF, 0xFF },  // 0x47
  { 0xFF, 0xFF },  // 0x48
  { 0xFF, 0xFF },  // 0x49
  { 0xFF, 0xFF },  // 0x4A
  { 0xFF, 0xFF },  // 0x4B
  { 0xFF, 0xFF },  // 0x4C
  { 0xFF, 0xFF },  // 0x4D
  { 0xFF, 0x00 },  // 0x4E
  { 0xFF, 0x00 },  // 0x4F
  { 0xFF, 0xFF },  // 0x50
  { 0xFF, 0xFF },  // 0x51
  { 0xFF, 0xFF },  // 0x52
  { 0x00, 0x00 },  // 0x53
  { 0x00, 0x00 },  // 0x54
  { 0x00, 0x00 },  // 0x55
  { 0x00, 0x00 },  // 0x56
  { 0x00, 0x00 },  // 0x57
  { 0x00, 0x00 },  // 0x58
  { 0x00, 0x00 },  // 0x59
  { 0x00, 0x00 },  // 0x5A
  { 0x00, 0x00 },  // 0x5B
  { 0x00, 0x00 },  // 0x5C
  { 0x00, 0x00 },  // 0x5D
  { 0x00, 0x00 },  // 0x5E
  { 0x00, 0x00 },  // 0x5F
  { 0x00, 0x00 },  // 0x60
  { 0x00, 0x00 },  // 0x61
  { 0x00, 0x00 },  // 0x62
  { 0x00, 0x00 },  // 0x63
  { 0x00, 0x00 },  // 0x64
  { 0x00, 0x00 },  // 0x65
  { 0x00, 0x00 },  // 0x66
  { 0x00, 0x00 },  // 0x67
  { 0x00, 0x00 },  // 0x68
  { 0x00, 0x00 },  // 0x69
  { 0x00, 0x00 },  // 0x6A
  { 0x00, 0x00 },  // 0x6B
  { 0x00, 0x00 },  // 0x6C
  { 0x00, 0x00 },  // 0x6D
  { 0x00, 0x00 },  // 0x6E
  { 0x00, 0x00 },  // 0x6F
  { 0x00, 0x00 },  // 0x70
  { 0x00, 0x00 },  // 0x71
  { 0x00, 0x00 },  // 0x72
  { 0x00, 0x00 },  // 0x73
  { 0x00, 0x00 },  // 0x74
  { 0x00, 0x00 },  // 0x75
  { 0x00, 0x00 },  // 0x76
  { 0x00, 0x00 },  // 0x77
  { 0x00, 0x00 },  // 0x78
  { 0x00, 0x00 },  // 0x79
  { 0x00, 0x00 },  // 0x7A
  { 0x00, 0x00 },  // 0x7B
  { 0x00, 0x00 },  // 0x7C
  { 0x00, 0x00 },  // 0x7D
  { 0x00, 0x00 },  // 0x7E
  { 0x00, 0x00 },  // 0x7F
  { 0xFF, 0xFF },  // 0x80
  { 0xFF, 0xFF },  // 0x81
  { 0xFF, 0xFF },  // 0x82
  { 0xFF, 0xFF },  // 0x83
  { 0xFF, 0xFF },  // 0x84
  { 0xFF, 0xFF },  // 0x85
  { 0xFF, 0xFF },  // 0x86
  { 0xFF, 0xFF },  // 0x87
};

static int ak7759_set_status(struct snd_soc_codec *codec, enum ak7759_status status);

/* MIC Input Volume control:
 * from -6 to 27 dB   3dB Step*/
static DECLARE_TLV_DB_SCALE(micgian_tlv, -600, 300, 0);


/* ADC Digital Volume control:
 * from -103.5 to 24 dB in 0.5 dB steps (mute instead of -103.5 dB) */
static DECLARE_TLV_DB_SCALE(voladc_tlv, -10350, 50, 0);

/* DAC Digital Volume control:
 * from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB) */
static DECLARE_TLV_DB_SCALE(voldac_tlv, -11550, 50, 0);

static const char *exbckbit_texts[] = {
	"Low", "BICK", 
};

static const struct soc_enum ak7759_exbcksel_enum[] = {
	SOC_ENUM_SINGLE(AK7759_08_SYNCPORT_SELECT, 4, ARRAY_SIZE(exbckbit_texts), exbckbit_texts),  // SDIN1
	SOC_ENUM_SINGLE(AK7759_08_SYNCPORT_SELECT, 0, ARRAY_SIZE(exbckbit_texts), exbckbit_texts),  // SDIN2
};

static const char *sdselbit_texts[] = {
	"Low", "SD1", "SD2",
};

static const struct soc_enum ak7759_sdsel_enum[] = {
	SOC_ENUM_SINGLE(AK7759_09_SYNCDOMAIN_SELECT1, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // BICK
	SOC_ENUM_SINGLE(AK7759_0A_SYNCDOMAIN_SELECT2, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // SDOUT1
	SOC_ENUM_SINGLE(AK7759_0A_SYNCDOMAIN_SELECT2, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // SDOUT2
	SOC_ENUM_SINGLE(AK7759_0B_SYNCDOMAIN_SELECT3, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // SDOUT3
	SOC_ENUM_SINGLE(AK7759_0C_SYNCDOMAIN_SELECT4, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // DSPOUT1

	SOC_ENUM_SINGLE(AK7759_0C_SYNCDOMAIN_SELECT4, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // DSPOUT2
	SOC_ENUM_SINGLE(AK7759_0D_SYNCDOMAIN_SELECT5, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // DSPOUT3
	SOC_ENUM_SINGLE(AK7759_0D_SYNCDOMAIN_SELECT5, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // DSPOUT4
	SOC_ENUM_SINGLE(AK7759_0E_SYNCDOMAIN_SELECT6, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // CODEC
	SOC_ENUM_SINGLE(AK7759_0E_SYNCDOMAIN_SELECT6, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // DSP

	SOC_ENUM_SINGLE(AK7759_0F_SYNCDOMAIN_SELECT7, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts),  // Mixer
};

static int setBUSClock(
struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int value;

	if(ak7759->BUSMclk > 2){
		pr_info("[AK7759] %s Invalid Value selected!\n",__FUNCTION__);
		return(-EINVAL);
	}
	else{
		value = 6;
		value <<= ak7759->BUSMclk;
		value--;

		snd_soc_update_bits(codec, AK7759_02_BUSCLOCK_SETTING, 0xFF, value);
		snd_soc_update_bits(codec, AK7759_01_SYSCLOCK_SETTING2, 0x30, 0x10);

		return(0);
	}
}

static const char *sd_fs_texts[] = {
    "8kHz", "12kHz",  "16kHz", "24kHz",
    "32kHz", "48kHz",
};

static int sdfstab[] = {
    8000, 12000, 16000, 24000,
    32000, 48000,
};

static const char *sd_bick_texts[] = {
	"64fs", "48fs", "32fs",  "128fs", "256fs"
};

static int sdbicktab[] = {
	64, 48, 32, 128, 256
};

static const struct soc_enum ak7759_sd_fs_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_fs_texts), sd_fs_texts),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_bick_texts), sd_bick_texts),
};


static int XtiFsTab[] = {
	12288000,  18432000
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
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int nPLLInFs;
	int value, nBfs, fs;
	int n, nMax;

	value = ( ak7759->PLLInput << 5 );
	snd_soc_update_bits(codec, AK7759_00_SYSCLOCK_SETTING1, 0x20, value);

	if ( ak7759->PLLInput == 0 ) {
		nPLLInFs = XtiFsTab[ak7759->XtiFs];
	}
	else {
		nBfs = sdbicktab[ak7759->SDBick[ak7759->PLLInput-1]];
		fs = sdfstab[ak7759->SDfs[ak7759->PLLInput-1]];
		akdbgprt("[AK7759] %s nBfs=%d fs=%d\n",__FUNCTION__, nBfs, fs);
		nPLLInFs = nBfs * fs;
		if ( ( fs % 4000 ) != 0 ) {
			nPLLInFs *= 441;
			nPLLInFs /= 480;
		}
	}

	n = 0;
	nMax = sizeof(PLLInFsTab) / sizeof(PLLInFsTab[0]);

	do {
		pr_debug("[AK7759] %s n=%d PLL:%d %d\n",__FUNCTION__, n, nPLLInFs, PLLInFsTab[n]);
		if ( nPLLInFs == PLLInFsTab[n] ) break;
		n++;
	} while ( n < nMax);


	if ( n == nMax ) {
		pr_err("%s: PLL1 setting Error!\n", __func__);
		return(-EINVAL);
	}

	snd_soc_update_bits(codec, AK7759_00_SYSCLOCK_SETTING1, 0x1F, n);

	return(0);

}

static int setSDMaster(
struct snd_soc_codec *codec,
int nSDNo,
int nMaster)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int addr;
	int value;
	int mask;

	pr_debug("\t[AK7759] %s\n",__FUNCTION__);

	if ( nSDNo >= NUM_SYNCDOMAIN ) return(0);

	ak7759->Master[nSDNo] = nMaster;
	value = (nMaster << nSDNo);
	mask = (0x01 << nSDNo);
	snd_soc_update_bits(codec, AK7759_03_MASTER_SLAVE_SETTING, mask, value);

	addr = AK7759_04_SYNCDOMAIN1_SETTING1 + ( 2 * nSDNo );
	value = (ak7759->SDCks[nSDNo] << 3);
	snd_soc_update_bits(codec, addr, 0x18, value);

	return(0);
}

static int setSDClock(
struct snd_soc_codec *codec,
int nSDNo)
{

	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int addr;
	int fs, bickfs;
	int sdv, bdv, wbdv1, wbdv2;

	if ( nSDNo >= NUM_SYNCDOMAIN ) return(0);

	fs = sdfstab[ak7759->SDfs[nSDNo]];
	bickfs = sdbicktab[ak7759->SDBick[nSDNo]] * fs;

	addr = AK7759_04_SYNCDOMAIN1_SETTING1 + ( 2 * nSDNo );

	bdv = 147456000 / bickfs;

	sdv = ak7759->SDBick[nSDNo];
	akdbgprt("\t[AK7759] %s, BDV=%d, SDV=%d\n",__FUNCTION__, bdv, sdbicktab[sdv]);
	bdv--;
	if ( bdv > 1023) {
		pr_err("%s: BDV Error! SD No = %d, bdv bit = %d\n", __func__, nSDNo, bdv);
		bdv = 1023;
	}

	akdbgprt("\tsdv bits=%x\n", sdv);
	snd_soc_update_bits(codec, addr, 0x07, sdv);

	wbdv1 = ((bdv >> 2) & 0xC0);
	akdbgprt("\twbdv1 bits=%x\n", wbdv1);
	snd_soc_update_bits(codec, addr, 0xC0, wbdv1); //BDIV[9:8]
		
	addr++;

	wbdv2 = (bdv & 0xFF);
	akdbgprt("\twbdv2 bits=%x\n", wbdv2);		
	snd_soc_update_bits(codec, addr, 0xFF, wbdv2); //BDIV[7:0]

	if ( ak7759->PLLInput == (nSDNo + 1) ) {
		setPLLOut(codec);
	}

	return(0);

}

static int get_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->SDfs[1];

    return 0;
}

static int set_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8){
		if ( ak7759->SDfs[1] != currMode ) {
			ak7759->SDfs[1] = currMode;
			setSDClock(codec, 1);
			setSDMaster(codec, 1, ak7759->Master[1]);
		}
	}
	else {
		akdbgprt(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}


static int get_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->SDBick[0];

    return 0;
}

static int set_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
		if ( ak7759->SDBick[0] != currMode ) {
			ak7759->SDBick[0] = currMode;
			setSDClock(codec, 0);
		}
	}
	else {
		pr_info(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
	return 0;
}

static int get_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->SDBick[1];

    return 0;
}

static int set_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 5){
		if ( ak7759->SDBick[1] != currMode ) {
			ak7759->SDBick[1] = currMode;
			setSDClock(codec, 1);
		}
	}
	else {
		pr_info(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
	return 0;
}


static const char *pllinput_texts[]  = {
	"XTI", "BICK"
};

static const char *xtifs_texts[]  = {
	"12.288MHz", "18.432MHz"
};

static const struct soc_enum ak7759_pllset_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllinput_texts), pllinput_texts),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(xtifs_texts), xtifs_texts),
};

static int get_pllinput(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->PLLInput;

    return 0;
}

static int set_pllinput(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak7759->PLLInput != currMode ) {
			ak7759->PLLInput = currMode;
			setPLLOut(codec);
		}
	}
	else {
		pr_info(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
	return 0;
}

static int get_xtifs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->XtiFs;

    return 0;
}

static int set_xtifs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak7759->XtiFs != currMode ) {
			ak7759->XtiFs = currMode;
			setPLLOut(codec);
		}
	}
	else {
		pr_info(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
	return 0;
}


static const char *ak7759_tdm_texts[] = {
	"TDM OFF", "TDM ON"
};

static const struct soc_enum ak7759_tdm_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7759_tdm_texts), ak7759_tdm_texts),
};

static int get_sdinout1_tdm(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->TDMbit;

    return 0;
}

static int set_sdinout1_tdm(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak7759->TDMbit != currMode ) {
			ak7759->TDMbit = currMode;
			ak7759->DIOEDGEbit = ak7759->TDMbit;
		}
	}
	else {
		pr_info(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
	return 0;
}


static const char *ak7759_dioedge_texts[] = {
	"LRCK Edge Basis",
	"Slot Length Basis",
};

static const struct soc_enum ak7759_dioedge_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7759_dioedge_texts), ak7759_dioedge_texts),
};


static const char *ak7759_sd_ioformat_texts[] = {
	"24bit",
	"20bit",
	"16bit",
	"32bit",
};

static const struct soc_enum ak7759_slotlen_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7759_sd_ioformat_texts), ak7759_sd_ioformat_texts),
};

static int get_sdio_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7759->DIOSLbit;

    return 0;
}

static int set_sdio_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 4){
		if ( ak7759->DIOSLbit != currMode ) {
			ak7759->DIOSLbit = currMode;
		}
	}
	else {
		pr_info(" [AK7759] %s Invalid Value selected!\n",__FUNCTION__);
	}
	return 0;
}

static const char *mixer_level_adjst_texts[] = {
	"No Shift", "1bit Right Shift", "Mute"
};
static const char *mixer_data_change_texts[] = {
	"Through", "Lin->LRout", "Rin->LRout", "Swap"
};

static const struct soc_enum ak7759_mixer_setting_enum[] = {
	SOC_ENUM_SINGLE(AK7759_38_MIXER_SETTING, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK7759_38_MIXER_SETTING, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK7759_38_MIXER_SETTING, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK7759_38_MIXER_SETTING, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
};

static const char *di1sel_texts[] = {
	"SDIN1", "JX0"
};

static const char *di2sel_texts[] = {
	"SDIN2", "JX1"
};

static const struct soc_enum ak7759_inportsel_enum[] = {
	SOC_ENUM_SINGLE(AK7759_28_INPUT_DATA_SELECT, 6, ARRAY_SIZE(di1sel_texts), di1sel_texts),
	SOC_ENUM_SINGLE(AK7759_28_INPUT_DATA_SELECT, 4, ARRAY_SIZE(di2sel_texts), di2sel_texts),
};

static const char *do1sel_texts[] = {
	"SDOUT1", "CLKO", "GP0"
};

static const char *do2sel_texts[] = {
	"SDOUT2", "GP1", "EEST"
};

static const char *do3sel_texts[] = {
	"SDOUT3", "DIT", "N/A"
};

static const char *rdysel_texts[] = {
	"RDY", "GP0", "N/A"
};

static const char *stosel_texts[] = {
	"STO", "GP1", "N/A"
};

static const struct soc_enum ak7759_outportsel_enum[] = {
	SOC_ENUM_SINGLE(AK7759_29_OUTPUT_DATA_SELECT1, 6, ARRAY_SIZE(do1sel_texts), do1sel_texts),
	SOC_ENUM_SINGLE(AK7759_29_OUTPUT_DATA_SELECT1, 4, ARRAY_SIZE(do2sel_texts), do2sel_texts),
	SOC_ENUM_SINGLE(AK7759_29_OUTPUT_DATA_SELECT1, 2, ARRAY_SIZE(do3sel_texts), do3sel_texts),
	SOC_ENUM_SINGLE(AK7759_2A_OUTPUT_DATA_SELECT2, 6, ARRAY_SIZE(rdysel_texts), rdysel_texts),
	SOC_ENUM_SINGLE(AK7759_2A_OUTPUT_DATA_SELECT2, 4, ARRAY_SIZE(stosel_texts), stosel_texts),
};


static const char *socfg_texts[] = {
	"HiZ", "CMOSL"
};

static const char *rdye_texts[] = {
	"I2CFP_In", "RDY_GP0_Out"
};

static const struct soc_enum ak7759_outputenable_enum[] = {
	SOC_ENUM_SINGLE(AK7759_2C_OUTPUT_ENABLE, 7, ARRAY_SIZE(socfg_texts), socfg_texts),
	SOC_ENUM_SINGLE(AK7759_2C_OUTPUT_ENABLE, 6, ARRAY_SIZE(rdye_texts), rdye_texts),
};


static const char *clkosel_texts[]  = {
	"12.288MHz", "24.576MHz", "8.192MHz", "6.144MHz",
    "4.096MHz", "2.048MHz"
};

static const struct soc_enum ak7759_clkosel_enum[] = {
	SOC_ENUM_SINGLE(AK7759_2B_CLKO_SETTING, 0, ARRAY_SIZE(clkosel_texts), clkosel_texts),
};


static const char *dsp_dlram_pointer_mode_texts[] = {
	"OFREG", "DBUS"
};

static const char *dsp_wave_point_texts[] = {
	"128ponits/33word",
	"256ponits/65word",
	"512ponits/129word",
	"1024ponits/257word",
	"2048ponits/513word",
	"4096ponits/1025word",
};

static const char *dsp_dram_addressing_texts[] = {
	"Ring/Ring", "Ring/Linear", "Linear/Ring", "Linear/Linear"  
};

static const char *dsp_dram_bank_size_texts[] = {
	"1024/3072",
	"2048/2048",
	"3072/1024",
	"4096/0"
};


static const char *dsp_dlram_sampling_mode_texts[] = {
	"1sampling", "2sampling", "4sampling", "8sampling"
};

static const char *dsp_dlram_addressing_texts[] = {
	"Ring/Ring", "Linear/Ring", 
};

static const char *dsp_ramclear_texts[] = {
	"Clear", "not Clear"
};


static const char *dsp_dlram_bank_size_texts[] = {
	"0/11264",
	"1024/10240",
	"2048/9216",
	"3072/8192",
	"4096/7168",
	"5120/6144",
	"6144/5120",
	"7168/4096",
	"8192/3072",
	"8216/2048",
	"10240/1048",
	"16384/0"
};

static const char *dspin_datalength_texts[] = {
	"24bit",
	"24.4bit"
};

static const char *dspout_datalength_texts[] = {
	"32bit",
	"24.4bit",
};

static const char *dsp_jx_input_texts[] = {
	"Direct",
	"Noise Filter",
};

static const struct soc_enum ak7759_dsp_setting_enum[] = {
	SOC_ENUM_SINGLE(AK7759_30_DSP_SETTING1, 7, ARRAY_SIZE(dsp_dlram_pointer_mode_texts), dsp_dlram_pointer_mode_texts),
	SOC_ENUM_SINGLE(AK7759_30_DSP_SETTING1, 4, ARRAY_SIZE(dsp_wave_point_texts), dsp_wave_point_texts),
	SOC_ENUM_SINGLE(AK7759_30_DSP_SETTING1, 2, ARRAY_SIZE(dsp_dram_addressing_texts), dsp_dram_addressing_texts),
	SOC_ENUM_SINGLE(AK7759_30_DSP_SETTING1, 0, ARRAY_SIZE(dsp_dram_bank_size_texts), dsp_dram_bank_size_texts),
	SOC_ENUM_SINGLE(AK7759_31_DSP_SETTING2, 6, ARRAY_SIZE(dsp_dlram_sampling_mode_texts), dsp_dlram_sampling_mode_texts),

	SOC_ENUM_SINGLE(AK7759_31_DSP_SETTING2, 5, ARRAY_SIZE(dsp_dlram_addressing_texts), dsp_dlram_addressing_texts),
	SOC_ENUM_SINGLE(AK7759_31_DSP_SETTING2, 4, ARRAY_SIZE(dsp_ramclear_texts), dsp_ramclear_texts),
	SOC_ENUM_SINGLE(AK7759_31_DSP_SETTING2, 0, ARRAY_SIZE(dsp_dlram_bank_size_texts), dsp_dlram_bank_size_texts),
	SOC_ENUM_SINGLE(AK7759_34_DSP_DATA_LENGTH, 1, ARRAY_SIZE(dspin_datalength_texts), dspin_datalength_texts),
	SOC_ENUM_SINGLE(AK7759_34_DSP_DATA_LENGTH, 0, ARRAY_SIZE(dspout_datalength_texts), dspout_datalength_texts),

	SOC_ENUM_SINGLE(AK7759_35_DSP_JX_FILTER_SETTING, 7, ARRAY_SIZE(dsp_jx_input_texts), dsp_jx_input_texts),
};

static const char *ditout_dither_texts[] = {
	"Normal", "Rounded"
};
static const char *ditout_cgmsa_texts[] = {
	"Copying permitted",
	"One generation copies",
	"Condition not used",
	"No copying permitted",
};
static const char *ditout_datasel_texts[] = {
	"Audio Data", "Digital Data"
};

static const char *ditout_category_texts[] = {
	"General",
	"Compact-disc",
	"Laser optical",
	"Mini disc",
	"DVD",
	"Other",
	"PCM",
	"Signal mixer",
	"SRC",
	"Sound sampler",
	"Sound processor",
	"Audio tape",
	"Video tape",
	"Compact cassette",
	"Magnetic disc",
	"Audio broadcast Japan",
	"Audio broadcast Europe",
	"Audio broadcast USA",
	"Electronic software",
	"Another standard",
	"Synthesizer",
	"Microphone",
	"ADC w/o copyright",
	"ADC w/ copyright",
	"Audio recorder",
	"Experimental",
};

static const unsigned int ditout_category_values[] = {
	0x00,
	0x01,
	0x09,
	0x49,
	0x19,
	0x79,
	0x02,
	0x12,
	0x1A,
	0x22,
	0x2A,
	0x03,
	0x0B,
	0x43,
	0x1B,
	0x04,
	0x0C,
	0x64,
	0x44,
	0x24,
	0x05,
	0x0D,
	0x06,
	0x16,
	0x08,
	0x40,
};

static const char *ditout_category2_texts[] = {
	"0", 
	"1",
};

static const char *ditout_clk_accuracy_texts[] = {
	"Normal", 
	"High-precision",
	"Variable Pitch", 
	"Not Specified"
};

static const char *ditout_fs_set_texts[] = {
	"22.05kHz",
	"24kHz",
	"32kHz",
	"44.1kHz",
	"48kHz",
	"64kHz",
	"88.2kHz",
	"96kHz",
	"176.4kHz",
	"192kHz",
	"352.8kHz",
	"384kHz",
	"768kHz",
};
static const unsigned int ditout_fs_set_values[] = {
	0x04,
	0x06,
	0x03,
	0x00,
	0x02,
	0x0B,
	0x08,
	0x0A,
	0x0C,
	0x0E,
	0x0D,
	0x05,
	0x09,
};

static const char *ditout_origfs_set_texts[] = {
	"8kHz",
	"11.025kHz",
	"12kHz",
	"16kHz",
	"22.05kHz",
	"24kHz",
	"32kHz",
	"44.1kHz",
	"48kHz",
	"64kHz",
	"88.2kHz",
	"96kHz",
	"128kHz",
	"176.4kHz",
	"192kH",
	"not indicated",
};

static const unsigned int ditout_origfs_set_values[] = {
	0x05,
	0x0A,
	0x02,
	0x08,
	0x0B,
	0x09,
	0x03,
	0x0F,
	0x0D,
	0x04,
	0x07,
	0x05,
	0x0E,
	0x03,
	0x01,
	0x00,
};

static const char *ditout_bitlen_set_texts[] = {
	"16bit",
	"17bit",
	"18bit",
	"19bit",
	"20bit",
	"21bit",
	"22bit",
	"23bit",
	"24bit",
	"not indicated",
};

static const unsigned int ditout_bitlen_set_values[] = {
	0x02,
	0x0C,
	0x04,
	0x08,
	0x03,
	0x0D,
	0x05,
	0x09,
	0x0B,
	0x00,
};

static const struct soc_enum ak7759_dit_statusbit_enum[] = {
	SOC_ENUM_SINGLE(AK7759_39_DIT_STATUS1, 6, ARRAY_SIZE(ditout_dither_texts), ditout_dither_texts),
	SOC_ENUM_SINGLE(AK7759_39_DIT_STATUS1, 4, ARRAY_SIZE(ditout_cgmsa_texts), ditout_cgmsa_texts),
	SOC_ENUM_SINGLE(AK7759_39_DIT_STATUS1, 1, ARRAY_SIZE(ditout_datasel_texts), ditout_datasel_texts),
	SOC_ENUM_SINGLE(AK7759_3A_DIT_STATUS2, 7, ARRAY_SIZE(ditout_category2_texts), ditout_category2_texts),
	SOC_ENUM_SINGLE(AK7759_3B_DIT_STATUS3, 4, ARRAY_SIZE(ditout_clk_accuracy_texts), ditout_clk_accuracy_texts),
};

static SOC_VALUE_ENUM_SINGLE_DECL(ak7759_ditout_categor_enum,
				  AK7759_3A_DIT_STATUS2,  0, 0x7F,
				  ditout_category_texts,
				  ditout_category_values);

static SOC_VALUE_ENUM_SINGLE_DECL(ak7759_ditout_fs_set_enum,
				  AK7759_3B_DIT_STATUS3,  0, 0x0F,
				  ditout_fs_set_texts,
				  ditout_fs_set_values);

static SOC_VALUE_ENUM_SINGLE_DECL(ak7759_ditout_origfs_set_enum,
				  AK7759_3C_DIT_STATUS4,  4, 0x0F,
				  ditout_origfs_set_texts,
				  ditout_origfs_set_values);

static SOC_VALUE_ENUM_SINGLE_DECL(ak7759_ditout_bitlen_set_enum,
				  AK7759_3C_DIT_STATUS4,  0, 0x0F,
				  ditout_bitlen_set_texts,
				  ditout_bitlen_set_values);



static const char *adc_filter_texts[] = {
	"Voice", "Normal",
};

static const char *adc_attspeed_texts[] = {
	"4/fs", "16/fs",
};

static const char *adc_input_texts[] = {
	"Differential", "Single-ended",
};

static const struct soc_enum ak7759_adc_setting_enum[] = {
	SOC_ENUM_SINGLE(AK7759_40_ADC_SETTING, 6, ARRAY_SIZE(adc_filter_texts), adc_filter_texts), 
	SOC_ENUM_SINGLE(AK7759_40_ADC_SETTING, 0, ARRAY_SIZE(adc_attspeed_texts), adc_attspeed_texts), 
	SOC_ENUM_SINGLE(AK7759_4C_AIN_SW, 4, ARRAY_SIZE(adc_input_texts), adc_input_texts), 
	SOC_ENUM_SINGLE(AK7759_4C_AIN_SW, 5, ARRAY_SIZE(adc_input_texts), adc_input_texts), 
};

static const char *dmic_chennel_texts[] = {
	"Rch", "Lch",
};

static const char *dmic_clock_texts[] = {
	"L", "64fs",
};

static const struct soc_enum ak7759_dmic_setting_enum[] = {
	SOC_ENUM_SINGLE(AK7759_41_DMIC_SETTING, 6, ARRAY_SIZE(adc_filter_texts), adc_filter_texts), 
	SOC_ENUM_SINGLE(AK7759_41_DMIC_SETTING, 0, ARRAY_SIZE(adc_attspeed_texts), adc_attspeed_texts), 
	SOC_ENUM_SINGLE(AK7759_42_DMIC_CLOCK_SETTING, 6, ARRAY_SIZE(dmic_chennel_texts), dmic_chennel_texts), 
	SOC_ENUM_SINGLE(AK7759_42_DMIC_CLOCK_SETTING, 5, ARRAY_SIZE(dmic_clock_texts), dmic_clock_texts), 
};

static const char *dac_sampling_clock_texts[] = {
	"12.288MHz", "256fs",
};

static const char *dac_Deem_texts[] = {
	"Off", "48kHz", "44.1kHz", "32kHz"
};

static const struct soc_enum ak7759_dac_setting_enum[] = {
	SOC_ENUM_SINGLE(AK7759_43_DAC_SETTING, 7, ARRAY_SIZE(dac_sampling_clock_texts), dac_sampling_clock_texts), 
	SOC_ENUM_SINGLE(AK7759_43_DAC_SETTING, 2, ARRAY_SIZE(dac_Deem_texts), dac_Deem_texts), 
	SOC_ENUM_SINGLE(AK7759_43_DAC_SETTING, 0, ARRAY_SIZE(adc_attspeed_texts), adc_attspeed_texts), 
	SOC_ENUM_SINGLE(AK7759_4D_OUT_SW, 6, ARRAY_SIZE(adc_input_texts), adc_input_texts), 
	SOC_ENUM_SINGLE(AK7759_4D_OUT_SW, 7, ARRAY_SIZE(adc_input_texts), adc_input_texts), 
};

static const struct soc_enum ak7759_firmware_enum[] = 
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7759_firmware_pram), ak7759_firmware_pram),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7759_firmware_cram), ak7759_firmware_cram),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7759_firmware_ofreg), ak7759_firmware_ofreg),
};

static int ak7759_firmware_write_ram(struct snd_soc_codec *codec, u16 mode, u16 cmd);
static int ak7759_write_cram(struct snd_soc_codec *codec, int addr, int len, unsigned char *cram_data);
//static int ak7759_mask_write_cram(struct snd_soc_codec *codec, int nDSPNo, int addr, int len,unsigned long *cram_data);


/*** DSP Binary Write ***/
static int get_DSP_write_pram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
  
   /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->DSPPramMode;	

    return 0;

}

static int set_DSP_write_pram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	akdbgprt("\t%s PRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7759_firmware_write_ram(codec, RAMTYPE_PRAM, currMode); 
	if ( ret != 0 ) return(-EINVAL);

	ak7759->DSPPramMode = currMode;
	
	return(0);
}

static int get_DSP_write_cram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->DSPCramMode;	

    return 0;

}

static int set_DSP_write_cram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	akdbgprt("\t%s CRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7759_firmware_write_ram(codec, RAMTYPE_CRAM, currMode); 
	if ( ret != 0 ) return(-EINVAL);

	ak7759->DSPCramMode = currMode;
	
	return(0);
}

static int get_DSP_write_ofreg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->DSPOfregMode;	

    return 0;

}

static int set_DSP_write_ofreg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7759_firmware_write_ram(codec, RAMTYPE_OFREG, currMode); 
	if ( ret != 0 ) return(-EINVAL);

	ak7759->DSPOfregMode = currMode;
	
	return(0);
}

#ifdef AK7759_DEBUG

static int ak7759_reads(struct snd_soc_codec *codec, u8 *, size_t, u8 *, size_t);
static int ak7759_writes(struct snd_soc_codec *codec, const u8 *tx, size_t wlen);

static const char *ak7759_data_value[] = {
   "00", "01", "02", "03", "04", "05", "06", "07",
   "08", "09", "0A", "0B", "0C", "0D", "0E", "0F",
   "10", "11", "12", "13", "14", "15", "16", "17",
   "18", "19", "1A", "1B", "1C", "1D", "1E", "1F",
   "20", "21", "22", "23", "24", "25", "26", "27",
   "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
   "30", "31", "32", "33", "34", "35", "36", "37",
   "38", "39", "3A", "3B", "3C", "3D", "3E", "3F",
   "40", "41", "42", "43", "44", "45", "46", "47",
   "48", "49", "4A", "4B", "4C", "4D", "4E", "4F",
   "50", "51", "52", "53", "54", "55", "56", "57",
   "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
   "60", "61", "62", "63", "64", "65", "66", "67",
   "68", "69", "6A", "6B", "6C", "6D", "6E", "6F",
   "70", "71", "72", "73", "74", "75", "76", "77",
   "78", "79", "7A", "7B", "7C", "7D", "7E", "7F",
   "80", "81", "82", "83", "84", "85", "86", "87",
   "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
   "90", "91", "92", "93", "94", "95", "96", "97",
   "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
   "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
   "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
   "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
   "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
   "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7",
   "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
   "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
   "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
   "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7",
   "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
   "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
   "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF"
};

static SOC_ENUM_SINGLE_EXT_DECL(ak7759_reg_write_addrH, ak7759_data_value);
static SOC_ENUM_SINGLE_EXT_DECL(ak7759_reg_write_addrL, ak7759_data_value);
static SOC_ENUM_SINGLE_EXT_DECL(ak7759_reg_write_value, ak7759_data_value);

static SOC_ENUM_SINGLE_EXT_DECL(ak7759_cram_write_addrH, ak7759_data_value);
static SOC_ENUM_SINGLE_EXT_DECL(ak7759_cram_write_addrL, ak7759_data_value);
static SOC_ENUM_SINGLE_EXT_DECL(ak7759_cram_write_valueH, ak7759_data_value);
static SOC_ENUM_SINGLE_EXT_DECL(ak7759_cram_write_valueM, ak7759_data_value);
static SOC_ENUM_SINGLE_EXT_DECL(ak7759_cram_write_valueL, ak7759_data_value);

static int get_reg_write_addrH(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->regvalue[0];	

    return 0;

}

static int set_reg_write_addrH(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->regvalue[0] = currMode;

	return(0);
}

static int get_reg_write_addrL(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->regvalue[1];	

    return 0;

}

static int set_reg_write_addrL(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->regvalue[1] = currMode;

	return(0);
}

static int get_reg_write_value(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->regvalue[2];	

    return 0;

}

static int set_reg_write_value(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    u32    currMode = ucontrol->value.enumerated.item[0];
	u8     tx[4];
	int    ret;
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->regvalue[2] = currMode;

	tx[0] = (unsigned char)COMMAND_WRITE_REG;
	tx[1] = (unsigned char)ak7759->regvalue[0];
	tx[2] = (unsigned char)ak7759->regvalue[1];
	tx[3] = (unsigned char)ak7759->regvalue[2];

	ret = ak7759_writes(codec, tx, 4);
	if ( ret < 0 ) return(ret);

	return(0);
}


static int get_cram_write_addrH(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->cramvalue[0];	

    return 0;

}

static int set_cram_write_addrH(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->cramvalue[0] = currMode;

	return(0);
}

static int get_cram_write_addrL(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->cramvalue[1];	

    return 0;

}

static int set_cram_write_addrL(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->cramvalue[1] = currMode;

	return(0);
}

static int get_cram_write_valueH(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->cramvalue[2];	

    return 0;

}

static int set_cram_write_valueH(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->cramvalue[2] = currMode;

	return(0);
}

static int get_cram_write_valueM(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->cramvalue[3];	

    return 0;

}

static int set_cram_write_valueM(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    u32    currMode = ucontrol->value.enumerated.item[0];
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->cramvalue[3] = currMode;

	return(0);
}

static int get_cram_write_valueL(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7759->cramvalue[4];	

    return 0;

}

static int set_cram_write_valueL(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

    u32    currMode = ucontrol->value.enumerated.item[0];
	unsigned char cram[3];
	int    addr;
	int    ret;
	
	if ( currMode >= (sizeof(ak7759_data_value)/sizeof(ak7759_data_value[0]))) return -EINVAL;

	ak7759->cramvalue[4] = currMode;

	addr = ((int)ak7759->cramvalue[0] << 8) + ak7759->cramvalue[1];

	if ( addr > AK7759_CRAM_MAX_ADDRESS ) {
		pr_err("AK7759 CRAM Write Address Error:addr=%x\n", addr);
		return -EINVAL;
	}

	cram[0] = ak7759->cramvalue[2];
	cram[1] = ak7759->cramvalue[3];
	cram[2] = ak7759->cramvalue[4];

	ret = ak7759_write_cram(codec, addr, 3, cram);
	if ( ret < 0 ) return(ret);

	return(0);
}


static int test_read_ram(struct snd_soc_codec *codec, int mode)
{
	u8	tx[3], rx[512];
	int i, n, plen, clen, olen;

	akdbgprt("*****[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7759_set_status(codec, DOWNLOAD);

	if ( mode == 0 ) {
		plen = 16;
		for ( n = 0 ; n < (5 * plen) ; n++ ) rx[n] = 0;
		tx[0] = (COMMAND_WRITE_PRAM & 0x7F);
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7759_reads(codec, tx, 3, rx, 5 * plen);
		printk("*****%s AK7759 RAM CMD=0x%x,  LEN = %d*******\n", __func__, (int)tx[0], plen);
		n = 0;
		for ( i = 0 ; i < plen ; i ++ ) {
			printk("AK7759 PAddr=%x %02x %02x %02x %02x %02x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2], (int)rx[n+3], (int)rx[n+4]);
			n += 5;
		}
	}
	else if ( mode == 1 ){
		clen = 16;
		for ( n = 0 ; n < (3 * clen) ; n++ ) rx[n] = 0;
		tx[0] =  (COMMAND_WRITE_CRAM & 0x7F);
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7759_reads(codec, tx, 3, rx, 3 * clen);
		printk("*****%s AK7759 RAM CMD=0x%x,  LEN = %d*******\n", __func__, (int)tx[0], clen);
		n = 0;
		for ( i = 0 ; i < clen ; i ++ ) {
			printk("AK7759 CAddr=%x %02x %02x %02x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2]);	
			n += 3;
		}
	}

	else if ( mode == 2 ){
		olen = 16;
		for ( n = 0 ; n < (3 * olen) ; n++ ) rx[n] = 0;
		tx[0] =  (COMMAND_WRITE_OFREG & 0x7F);
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7759_reads(codec, tx, 3, rx, 3 * olen);
		printk("*****%s AK7759 RAM CMD=0x%x,  LEN = %d*******\n", __func__, (int)tx[0], olen);
		n = 0;
		for ( i = 0 ; i < olen ; i ++ ) {
			printk("AK7759 OAddr=%x %02x %02x %02x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2]);	
			n += 3;
		}
	}

	ak7759_set_status(codec, DOWNLOAD_FINISH);

	return(0);

}



static const char *test_reg_select[]   = 
{
    "read AK7759 Reg 00:52",
    "read AK7759 Reg 80:83",
	"read DSP PRAM",
	"read DSP CRAM",
	"read DSP OFREG",
};

static const struct soc_enum ak7759_test_enum[] = 
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

	if ( currMode < 2 ) {
		if ( currMode == 0 ) {
			regs = 0x00;
			rege = 0x53;
		}
		else {
			regs = 0x80;
			rege = 0x83;
		}
		for ( i = regs ; i <= rege ; i++ ){
			if (ak7759_access_masks[i].readable == 0) continue;
			value = snd_soc_read(codec, i);
			printk("***AK7759 Addr,Reg=(%02x, %02x)\n", i, value);
		}
	}
	else if ( currMode < 15 ) {
		test_read_ram(codec, (currMode - 2));
	}

	return 0;

}

#endif

static const struct snd_kcontrol_new ak7759_snd_controls[] = {

	SOC_SINGLE_TLV("MIC Input Volume L",
			AK7759_44_MIC_GAIN, 0, 0x0F, 0, micgian_tlv),
	SOC_SINGLE_TLV("MIC Input Volume R",
			AK7759_44_MIC_GAIN, 4, 0x0F, 0, micgian_tlv),

	SOC_SINGLE_TLV("ADC Digital Volume L",
			AK7759_45_ADC_LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("ADC Digital Volume R",
			AK7759_46_ADC_RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("Digital MIC Volume L",
			AK7759_47_DMIC_LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("Digital MIC Volume R",
			AK7759_48_DMIC_RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voladc_tlv),

	SOC_SINGLE_TLV("DAC Digital Volume L",
			AK7759_49_DAC_LCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC Digital Volume R",
			AK7759_4A_DAC_RCH_DIGITAL_VOLUME, 0, 0xFF, 1, voldac_tlv),

	SOC_SINGLE("ADC Mute", AK7759_4B_ADC_DAC_MUTE, 7, 1, 0),
	SOC_SINGLE("Digital MIC Mute", AK7759_4B_ADC_DAC_MUTE, 6, 1, 0),
	SOC_SINGLE("DAC Mute", AK7759_4B_ADC_DAC_MUTE, 5, 1, 0),

	SOC_ENUM("SDIN1 BICK Select", ak7759_exbcksel_enum[0]),
	SOC_ENUM("SDIN2 BICK Select", ak7759_exbcksel_enum[1]),

	SOC_ENUM("BICK Sync Domain", ak7759_sdsel_enum[0]),
	SOC_ENUM("SDOUT1 Sync Domain", ak7759_sdsel_enum[1]),
	SOC_ENUM("SDOUT2 Sync Domain", ak7759_sdsel_enum[2]),
	SOC_ENUM("SDOUT3 Sync Domain", ak7759_sdsel_enum[3]),

	SOC_ENUM("DSPOUT1 Sync Domain", ak7759_sdsel_enum[4]),
	SOC_ENUM("DSPOUT2 Sync Domain", ak7759_sdsel_enum[5]),
	SOC_ENUM("DSPOUT3 Sync Domain", ak7759_sdsel_enum[6]),
	SOC_ENUM("DSPOUT4 Sync Domain", ak7759_sdsel_enum[7]),
	SOC_ENUM("CODEC Sync Domain", ak7759_sdsel_enum[8]),
	SOC_ENUM("DSP Sync Domain", ak7759_sdsel_enum[9]),
	SOC_ENUM("MIXER Sync Domain", ak7759_sdsel_enum[10]),

	SOC_ENUM_EXT("Sync Domain 2 fs", ak7759_sd_fs_enum[0], get_sd2_fs, set_sd2_fs),

	SOC_ENUM_EXT("Sync Domain 1 BICK", ak7759_sd_fs_enum[1], get_sd1_bick, set_sd1_bick),
	SOC_ENUM_EXT("Sync Domain 2 BICK", ak7759_sd_fs_enum[1], get_sd2_bick, set_sd2_bick),

	SOC_ENUM_EXT("PLL Input Clock", ak7759_pllset_enum[0], get_pllinput, set_pllinput),

	SOC_ENUM_EXT("XTI Frequency", ak7759_pllset_enum[1], get_xtifs, set_xtifs),

	SOC_ENUM_EXT("SDIN1/SDOUT1 TDM Setting", ak7759_tdm_enum[0], get_sdinout1_tdm, set_sdinout1_tdm),

	SOC_ENUM_EXT("SDIO Slot Length", ak7759_slotlen_enum[0], get_sdio_slot, set_sdio_slot),

	SOC_ENUM("Mixer Input1 Data Change", ak7759_mixer_setting_enum[0]),
	SOC_ENUM("Mixer Input2 Data Change", ak7759_mixer_setting_enum[1]),
	SOC_ENUM("Mixer Input1 Level Adjust", ak7759_mixer_setting_enum[2]),
	SOC_ENUM("Mixer Input2 Level Adjust", ak7759_mixer_setting_enum[3]),

	SOC_ENUM("SDIN1 Pin Input Select", ak7759_inportsel_enum[0]),
	SOC_ENUM("SDIN2 Pin Input Select", ak7759_inportsel_enum[1]),

	SOC_ENUM("SDOUT1 Pin Output Select", ak7759_outportsel_enum[0]),
	SOC_ENUM("SDOUT2 Pin Output Select", ak7759_outportsel_enum[1]),
	SOC_ENUM("SDOUT3 Pin Output Select", ak7759_outportsel_enum[2]),
	SOC_ENUM("RDY Pin Output Select", ak7759_outportsel_enum[3]),
	SOC_ENUM("STO Pin Output Select", ak7759_outportsel_enum[4]),

	SOC_SINGLE("CLKO Enable", AK7759_2B_CLKO_SETTING, 4, 1, 0),
	SOC_ENUM("CLKO Clock Select", ak7759_clkosel_enum[0]),

	SOC_ENUM("SO Pin Setting", ak7759_outputenable_enum[0]),
	SOC_ENUM("RDYE Pin Setting", ak7759_outputenable_enum[1]),

	SOC_SINGLE("STO Enable", AK7759_2C_OUTPUT_ENABLE, 5, 1, 0),
	SOC_SINGLE("SDOUT1 Enable", AK7759_2C_OUTPUT_ENABLE, 4, 1, 0),
	SOC_SINGLE("SDOUT2 Enable", AK7759_2C_OUTPUT_ENABLE, 3, 1, 0),
	SOC_SINGLE("SDOUT3 Enable", AK7759_2C_OUTPUT_ENABLE, 2, 1, 0),

	SOC_SINGLE("Watched Dog Output Enable", AK7759_2D_STO_SETTING, 6, 1, 1),
	SOC_SINGLE("CRCE Output Enable", AK7759_2D_STO_SETTING, 5, 1, 0),
	SOC_SINGLE("PLL Lock Output Enable", AK7759_2D_STO_SETTING, 4, 1, 0),

	SOC_ENUM("DSP DLRAM Pointer DLP0 Mode", ak7759_dsp_setting_enum[0]),
	SOC_ENUM("DSP Trigonometric Function Period", ak7759_dsp_setting_enum[1]),
	SOC_ENUM("DSP DRAM Addressing Mode", ak7759_dsp_setting_enum[2]),
	SOC_ENUM("DSP DRAM Bank1/0 Size", ak7759_dsp_setting_enum[3]),
	SOC_ENUM("DSP DLRAM Bank Sampling Mode", ak7759_dsp_setting_enum[4]),

	SOC_ENUM("DSP DLRAM Addressing Mode", ak7759_dsp_setting_enum[5]),
	SOC_ENUM("DSP DRAM Clear Setting", ak7759_dsp_setting_enum[6]),
	SOC_ENUM("DSP DLRAM Bank1/0 Size", ak7759_dsp_setting_enum[7]),

	SOC_SINGLE("DSP JX3 Enable", AK7759_32_DSP_JX_ENABLE, 7, 1, 0),
	SOC_SINGLE("DSP JX2 Enable", AK7759_32_DSP_JX_ENABLE, 6, 1, 0),
	SOC_SINGLE("DSP JX1 Enable", AK7759_32_DSP_JX_ENABLE, 5, 1, 0),
	SOC_SINGLE("DSP JX0 Enable", AK7759_32_DSP_JX_ENABLE, 4, 1, 0),

	SOC_ENUM("DSP DIN Data Length", ak7759_dsp_setting_enum[8]),
	SOC_ENUM("DSP DOUT Data Length", ak7759_dsp_setting_enum[9]),
	SOC_ENUM("DSP JX Input Path", ak7759_dsp_setting_enum[10]),

	SOC_SINGLE("DIT Validity Flag", AK7759_39_DIT_STATUS1, 7, 1, 0),
	SOC_ENUM("DIT Dither Setting", ak7759_dit_statusbit_enum[0]),
	SOC_ENUM("DIT CGMS-A Setting", ak7759_dit_statusbit_enum[1]),
	SOC_SINGLE("DIT with Pre-emphasis", AK7759_39_DIT_STATUS1, 3, 1, 0),
	SOC_SINGLE("DIT Copyright Protected Valid", AK7759_39_DIT_STATUS1, 2, 1, 1),
	SOC_ENUM("DIT Data Select", ak7759_dit_statusbit_enum[2]),
	SOC_SINGLE("DIT Chanel Number Valid", AK7759_39_DIT_STATUS1, 0, 1, 1),
	SOC_ENUM("Category Code CS[15]", ak7759_dit_statusbit_enum[3]),
	SOC_ENUM("Category Code CS[8:14]", ak7759_ditout_categor_enum),

	SOC_ENUM("DIT Clock Accuracy", ak7759_dit_statusbit_enum[4]),
	SOC_ENUM("DIT Sampling Frequency", ak7759_ditout_fs_set_enum),
	SOC_ENUM("DIT Original Sampling Frequency", ak7759_ditout_origfs_set_enum),
	SOC_ENUM("DIT Data Bit Length", ak7759_ditout_bitlen_set_enum),

	SOC_ENUM("ADC Filter", ak7759_adc_setting_enum[0]),
	SOC_ENUM("ADC Volume Transition Time", ak7759_adc_setting_enum[1]),
	SOC_ENUM("ADC Input Select Lch", ak7759_adc_setting_enum[2]),
	SOC_ENUM("ADC Input Select Rch", ak7759_adc_setting_enum[3]),

	SOC_ENUM("DMIC Filter", ak7759_dmic_setting_enum[0]),
	SOC_ENUM("DMIC Volume Transition Time", ak7759_dmic_setting_enum[1]),
	SOC_ENUM("DMIC Clock High Channel", ak7759_dmic_setting_enum[2]),
	SOC_ENUM("DMIC Clock Output", ak7759_dmic_setting_enum[3]),

	SOC_ENUM("DAC Sampling Clock", ak7759_dac_setting_enum[0]),
	SOC_ENUM("DAC De-emphasis Filter", ak7759_dac_setting_enum[1]),
	SOC_ENUM("DAC Volume Transition Time", ak7759_dac_setting_enum[2]),
	SOC_ENUM("DAC Output Select Lch", ak7759_dac_setting_enum[3]),
	SOC_ENUM("DAC Output Select Rch", ak7759_dac_setting_enum[4]),

	SOC_ENUM_EXT("DSP Firmware PRAM", ak7759_firmware_enum[0], get_DSP_write_pram, set_DSP_write_pram),
	SOC_ENUM_EXT("DSP Firmware CRAM", ak7759_firmware_enum[1], get_DSP_write_cram, set_DSP_write_cram),
	SOC_ENUM_EXT("DSP Firmware OFREG", ak7759_firmware_enum[2], get_DSP_write_ofreg, set_DSP_write_ofreg),

#ifdef AK7759_DEBUG
	SOC_ENUM_EXT("Set Register Address H", ak7759_reg_write_addrH, get_reg_write_addrH, set_reg_write_addrH),
	SOC_ENUM_EXT("Set Register Address L", ak7759_reg_write_addrL, get_reg_write_addrL, set_reg_write_addrL),
	SOC_ENUM_EXT("Set Register Data", ak7759_reg_write_value, get_reg_write_value, set_reg_write_value),

	SOC_ENUM_EXT("Set CRAM Address H", ak7759_cram_write_addrH, get_cram_write_addrH, set_cram_write_addrH),
	SOC_ENUM_EXT("Set CRAM Address L", ak7759_cram_write_addrL, get_cram_write_addrL, set_cram_write_addrL),
	SOC_ENUM_EXT("Set CRAM Data H", ak7759_cram_write_valueH, get_cram_write_valueH, set_cram_write_valueH),
	SOC_ENUM_EXT("Set CRAM Data M", ak7759_cram_write_valueM, get_cram_write_valueM, set_cram_write_valueM),
	SOC_ENUM_EXT("Set CRAM Data L", ak7759_cram_write_valueL, get_cram_write_valueL, set_cram_write_valueL),

	SOC_ENUM_EXT("Reg Read", ak7759_test_enum[0], get_test_reg, set_test_reg),
#endif

};

static const char *ak7759_dmic_input_texts[] =
		{"Off", "On"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak7759_dmicinput_enum, ak7759_dmic_input_texts);

static const struct snd_kcontrol_new ak7759_dmicinput_mux_control =
	SOC_DAPM_ENUM("DMIC Select", ak7759_dmicinput_enum);


static const char *ak7759_ain_lch_texts[] =
		{"Off", "On"};

static const struct soc_enum ak7759_ain_lch_enum =
	SOC_ENUM_SINGLE(AK7759_01_SYSCLOCK_SETTING2, 2,
			ARRAY_SIZE(ak7759_ain_lch_texts), ak7759_ain_lch_texts);

static const struct snd_kcontrol_new ak7759_ain_lch_mux_control =
	SOC_DAPM_ENUM("AIN Lch Select", ak7759_ain_lch_enum);

static const char *ak7759_ain_rch_texts[] =
		{"Off", "On"};

static const struct soc_enum ak7759_ain_rch_enum =
	SOC_ENUM_SINGLE(AK7759_01_SYSCLOCK_SETTING2, 3,
			ARRAY_SIZE(ak7759_ain_rch_texts), ak7759_ain_rch_texts);

static const struct snd_kcontrol_new ak7759_ain_rch_mux_control =
	SOC_DAPM_ENUM("AIN Rch Select", ak7759_ain_rch_enum);

static const char *ak7759_aout1_texts[] =
		{"VCOM", "AIN1", "DACL"};

static const struct soc_enum ak7759_aout1_enum =
	SOC_ENUM_SINGLE(AK7759_4D_OUT_SW, 0,
			ARRAY_SIZE(ak7759_aout1_texts), ak7759_aout1_texts);

static const struct snd_kcontrol_new ak7759_aout1_mux_control =
	SOC_DAPM_ENUM("OUT1 Select", ak7759_aout1_enum);

static const char *ak7759_aout2_texts[] =
		{"VCOM", "AIN2", "DACR"};

static const struct soc_enum ak7759_aout2_enum =
	SOC_ENUM_SINGLE(AK7759_4D_OUT_SW, 2,
			ARRAY_SIZE(ak7759_aout2_texts), ak7759_aout2_texts);

static const struct snd_kcontrol_new ak7759_aout2_mux_control =
	SOC_DAPM_ENUM("OUT2 Select", ak7759_aout2_enum);

static const char *ak7759_aout3_texts[] =
		{"VCOM", "DACL", "DACR"};

static const struct soc_enum ak7759_aout3_enum =
	SOC_ENUM_SINGLE(AK7759_4D_OUT_SW, 4,
			ARRAY_SIZE(ak7759_aout3_texts), ak7759_aout3_texts);

static const struct snd_kcontrol_new ak7759_aout3_mux_control =
	SOC_DAPM_ENUM("OUT3 Select", ak7759_aout3_enum);

static const char *ak7759_source_select_texts[] = {
	"ALL0", "SDIN1", "SDIN1B", "SDIN1C", "SDIN1D", "SDIN2",
	"DSPO1", "DSPO2", "DSPO3", "DSPO4", 
    "DMICO", "ADCO", "MIXERO",
};

static const unsigned int ak7759_source_select_values[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 
	0x08, 0x09, 0x0A, 0x0B, 
    0x0D, 0x0E, 0x0F,
};

static const struct soc_enum ak7759_sout1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_10_SDOUT1A_OUTPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_sout1_mux_control =
	SOC_DAPM_VALUE_ENUM("SDOUT1 Select", ak7759_sout1_mux_enum);

static const struct soc_enum ak7759_sout1b_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_11_SDOUT1B_OUTPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_sout1b_mux_control =
	SOC_DAPM_VALUE_ENUM("SDOUT1B Select", ak7759_sout1b_mux_enum);

static const struct soc_enum ak7759_sout1c_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_12_SDOUT1C_OUTPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_sout1c_mux_control =
	SOC_DAPM_VALUE_ENUM("SDOUT1C Select", ak7759_sout1c_mux_enum);

static const struct soc_enum ak7759_sout1d_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_13_SDOUT1D_OUTPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_sout1d_mux_control =
	SOC_DAPM_VALUE_ENUM("SDOUT1D Select", ak7759_sout1d_mux_enum);

static const struct soc_enum ak7759_sout2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_14_SDOUT2_OUTPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_sout2_mux_control =
	SOC_DAPM_VALUE_ENUM("SDOUT2 Select", ak7759_sout2_mux_enum);

static const struct soc_enum ak7759_sout3_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_15_SDOUT3_OUTPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_sout3_mux_control =
	SOC_DAPM_VALUE_ENUM("SDOUT3 Select", ak7759_sout3_mux_enum);

static const struct soc_enum ak7759_dsp1in1_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_16_DSP_IN1_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_dsp1in1_mux_control =
	SOC_DAPM_VALUE_ENUM("DSPI1 Input Select", ak7759_dsp1in1_mux_enum);

static const struct soc_enum ak7759_dsp1in2_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_17_DSP_IN2_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_dsp1in2_mux_control =
	SOC_DAPM_VALUE_ENUM("DSPI2 Input Select", ak7759_dsp1in2_mux_enum);

static const struct soc_enum ak7759_dsp1in3_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_18_DSP_IN3_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_dsp1in3_mux_control =
	SOC_DAPM_VALUE_ENUM("DSPI3 Input Select", ak7759_dsp1in3_mux_enum);

static const struct soc_enum ak7759_dsp1in4_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_19_DSP_IN4_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_dsp1in4_mux_control =
	SOC_DAPM_VALUE_ENUM("DSPI4 Input Select", ak7759_dsp1in4_mux_enum);

static const struct soc_enum ak7759_dacin_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_1A_DAC_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_dacin_mux_control =
	SOC_DAPM_VALUE_ENUM("DAC Input Select", ak7759_dacin_mux_enum);

static const struct soc_enum ak7759_mixch1in_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_1B_MIXER_CH1_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_mixch1in_mux_control =
	SOC_DAPM_VALUE_ENUM("Mixer Ch1 Input Select", ak7759_mixch1in_mux_enum);

static const struct soc_enum ak7759_mixch2in_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_1C_MIXER_CH2_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_mixch2in_mux_control =
	SOC_DAPM_VALUE_ENUM("Mixer Ch2 Input Select", ak7759_mixch2in_mux_enum);

static const struct soc_enum ak7759_ditin_mux_enum =
	SOC_VALUE_ENUM_SINGLE(AK7759_1D_DIT_INPUT_SELECT, 0, 0x0F,
			ARRAY_SIZE(ak7759_source_select_texts), ak7759_source_select_texts, ak7759_source_select_values);
static const struct snd_kcontrol_new ak7759_ditin_mux_control =
	SOC_DAPM_VALUE_ENUM("DIT Input Select", ak7759_ditin_mux_enum);

static int ak7759_ClockReset(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
#ifdef KERNEL_3_18_XX
	struct snd_soc_codec *codec = w->codec;
#else
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
#endif

	akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			akdbgprt("\t[AK7759] SND_SOC_DAPM_PRE_PMU\n");
			snd_soc_update_bits(codec, AK7759_01_SYSCLOCK_SETTING2, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7759_51_RESET_CONTROL, 0x01, 0x01);         // HRESETN bit = 1
			break;

		case SND_SOC_DAPM_POST_PMD:
			akdbgprt("\t[AK7759] SND_SOC_DAPM_POST_PMD\n");
			snd_soc_update_bits(codec, AK7759_51_RESET_CONTROL, 0x01, 0x00);   // HRESETN bit = 0
			snd_soc_update_bits(codec, AK7759_01_SYSCLOCK_SETTING2, 0x80, 0x00);  // CKRESETN bit = 0
			break;
	}
	return 0;
}


static const struct snd_kcontrol_new dsp1out1_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP IN1", AK7759_VIRT_84_DSP1OUT1_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP IN2", AK7759_VIRT_84_DSP1OUT1_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP IN3", AK7759_VIRT_84_DSP1OUT1_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP IN4", AK7759_VIRT_84_DSP1OUT1_MIX, 3, 1, 0),
};

static const struct snd_kcontrol_new dsp1out2_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP IN1", AK7759_VIRT_85_DSP1OUT2_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP IN2", AK7759_VIRT_85_DSP1OUT2_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP IN3", AK7759_VIRT_85_DSP1OUT2_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP IN4", AK7759_VIRT_85_DSP1OUT2_MIX, 3, 1, 0),
};

static const struct snd_kcontrol_new dsp1out3_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP IN1", AK7759_VIRT_86_DSP1OUT3_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP IN2", AK7759_VIRT_86_DSP1OUT3_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP IN3", AK7759_VIRT_86_DSP1OUT3_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP IN4", AK7759_VIRT_86_DSP1OUT3_MIX, 3, 1, 0),
};

static const struct snd_kcontrol_new dsp1out4_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSP IN1", AK7759_VIRT_87_DSP1OUT4_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSP IN2", AK7759_VIRT_87_DSP1OUT4_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSP IN3", AK7759_VIRT_87_DSP1OUT4_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSP IN4", AK7759_VIRT_87_DSP1OUT4_MIX, 3, 1, 0),
};

/* ak7759 dapm widgets */
static const struct snd_soc_dapm_widget ak7759_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("Clock Power", 1, SND_SOC_NOPM, 0, 0,
	                ak7759_ClockReset, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CODEC Power", 2, AK7759_51_RESET_CONTROL, 3, 0, NULL, 0),

// Analog Input
	SND_SOC_DAPM_INPUT("AIN1"),
	SND_SOC_DAPM_INPUT("AIN2"),

	SND_SOC_DAPM_MUX("AIN Lch Switch", SND_SOC_NOPM, 0, 0, &ak7759_ain_lch_mux_control),
	SND_SOC_DAPM_MUX("AIN Rch Switch", SND_SOC_NOPM, 0, 0, &ak7759_ain_rch_mux_control),

	SND_SOC_DAPM_INPUT("DMICIN"),

	SND_SOC_DAPM_MUX("DMIC Switch", SND_SOC_NOPM, 0, 0, &ak7759_dmicinput_mux_control),

// Analog Ouput
	SND_SOC_DAPM_MUX("OUT1 MUX", SND_SOC_NOPM, 0, 0, &ak7759_aout1_mux_control),
	SND_SOC_DAPM_MUX("OUT2 MUX", SND_SOC_NOPM, 0, 0, &ak7759_aout2_mux_control),
	SND_SOC_DAPM_MUX("OUT3 MUX", SND_SOC_NOPM, 0, 0, &ak7759_aout3_mux_control),

	SND_SOC_DAPM_PGA("VCOM", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("AOUT1"),
	SND_SOC_DAPM_OUTPUT("AOUT2"),
	SND_SOC_DAPM_OUTPUT("AOUT3"),

// CODEC
	SND_SOC_DAPM_ADC("ADCR", NULL, AK7759_50_POWER_MANAGEMENT, 7, 0),
	SND_SOC_DAPM_ADC("ADCL", NULL, AK7759_50_POWER_MANAGEMENT, 6, 0),
	SND_SOC_DAPM_ADC("DMIC", NULL, AK7759_50_POWER_MANAGEMENT, 5, 0),

	SND_SOC_DAPM_ADC("ADC", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC("DACR", NULL, AK7759_50_POWER_MANAGEMENT, 3, 0),
	SND_SOC_DAPM_DAC("DACL", NULL, AK7759_50_POWER_MANAGEMENT, 2, 0),


// DSP
	SND_SOC_DAPM_SUPPLY_S("DSP Power", 2, AK7759_51_RESET_CONTROL, 2, 0, NULL, 0),

	SND_SOC_DAPM_PGA("DSPO1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO4", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("DSP OUT1 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out1_mixer_kctrl[0], ARRAY_SIZE(dsp1out1_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP OUT2 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out2_mixer_kctrl[0], ARRAY_SIZE(dsp1out2_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP OUT3 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out3_mixer_kctrl[0], ARRAY_SIZE(dsp1out3_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSP OUT4 Mixer", SND_SOC_NOPM, 0, 0, &dsp1out4_mixer_kctrl[0], ARRAY_SIZE(dsp1out4_mixer_kctrl)),

// Mixer
	SND_SOC_DAPM_PGA("Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),

// Digital Input/Output
	SND_SOC_DAPM_AIF_IN("SDIN1", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2", "Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("SDOUT1", "Capture", 0, AK7759_2C_OUTPUT_ENABLE, 4, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT2", "Capture", 0, AK7759_2C_OUTPUT_ENABLE, 3, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT3", "Capture", 0, AK7759_2C_OUTPUT_ENABLE, 2, 0),

	SND_SOC_DAPM_PGA("DIT", AK7759_50_POWER_MANAGEMENT, 0, 0, NULL, 0),
	SND_SOC_DAPM_AIF_OUT("DITOUT", NULL, 0, SND_SOC_NOPM, 0, 0),

// Source Selector
	SND_SOC_DAPM_MUX("SDOUT1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_sout1_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1B Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_sout1b_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1C Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_sout1c_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1D Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_sout1d_mux_control),
	SND_SOC_DAPM_MUX("SDOUT2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_sout2_mux_control),
	SND_SOC_DAPM_MUX("SDOUT3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_sout3_mux_control),

	SND_SOC_DAPM_MUX("DSP IN1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_dsp1in1_mux_control),
	SND_SOC_DAPM_MUX("DSP IN2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_dsp1in2_mux_control),
	SND_SOC_DAPM_MUX("DSP IN3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_dsp1in3_mux_control),
	SND_SOC_DAPM_MUX("DSP IN4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_dsp1in4_mux_control),

	SND_SOC_DAPM_MUX("DAC Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_dacin_mux_control),

	SND_SOC_DAPM_MUX("Mixer Ch1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_mixch1in_mux_control),
	SND_SOC_DAPM_MUX("Mixer Ch2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_mixch2in_mux_control),

	SND_SOC_DAPM_MUX("DIT Source Selector", SND_SOC_NOPM, 0, 0, &ak7759_ditin_mux_control),

};

static const struct snd_soc_dapm_route ak7759_intercon[] = {
	{"CODEC Power", NULL, "Clock Power"},
	{"SDOUT1", NULL, "Clock Power"},
	{"SDOUT2", NULL, "Clock Power"},
	{"SDOUT3", NULL, "Clock Power"},

	{"SDIN1", NULL, "Clock Power"},
	{"SDIN2", NULL, "Clock Power"},

	{"ADCR", NULL, "CODEC Power"},
	{"ADCL", NULL, "CODEC Power"},
	{"DMIC", NULL, "CODEC Power"},

	{"DACR", NULL, "CODEC Power"},
	{"DACL", NULL, "CODEC Power"},

	{"AIN Lch Switch", "On", "AIN1"},
	{"AIN Rch Switch", "On", "AIN2"},

	{"ADCL", NULL, "AIN Lch Switch"},
	{"ADCR", NULL, "AIN Rch Switch"},

	{"ADC", NULL, "ADCL"},
	{"ADC", NULL, "ADCR"},

	{"DMIC Switch", "On", "DMICIN"},
	{"DMIC", NULL, "DMIC Switch"},

	{"DACL", NULL, "DAC Source Selector"},
	{"DACR", NULL, "DAC Source Selector"},

	{"OUT1 MUX", "VCOM", "VCOM"},
	{"OUT1 MUX", "AIN1", "AIN Rch Switch"},
	{"OUT1 MUX", "DACL", "DACL"},
	{"AOUT1", NULL, "OUT1 MUX"},

	{"OUT2 MUX", "VCOM", "VCOM"},
	{"OUT2 MUX", "AIN2", "AIN Rch Switch"},
	{"OUT2 MUX", "DACR", "DACR"},
	{"AOUT2", NULL, "OUT2 MUX"},

	{"OUT3 MUX", "VCOM", "VCOM"},
	{"OUT3 MUX", "DACL", "DACL"},
	{"OUT3 MUX", "DACR", "DACR"},
	{"AOUT3", NULL, "OUT3 MUX"},

	{"DIT", NULL, "DIT Source Selector"},
	{"DITOUT", NULL, "DIT"},

	{"SDOUT1", NULL, "SDOUT1 Source Selector"},
	{"SDOUT1", NULL, "SDOUT1B Source Selector"},
	{"SDOUT1", NULL, "SDOUT1C Source Selector"},
	{"SDOUT1", NULL, "SDOUT1D Source Selector"},
	{"SDOUT2", NULL, "SDOUT2 Source Selector"},
	{"SDOUT3", NULL, "SDOUT3 Source Selector"},

	{"DSPO1", NULL, "DSP Power"},
	{"DSPO2", NULL, "DSP Power"},
	{"DSPO3", NULL, "DSP Power"},
	{"DSPO4", NULL, "DSP Power"},

	{"DSPO1", NULL, "DSP OUT1 Mixer"},
	{"DSPO2", NULL, "DSP OUT2 Mixer"},
	{"DSPO3", NULL, "DSP OUT3 Mixer"},
	{"DSPO4", NULL, "DSP OUT4 Mixer"},

	{"DSP OUT1 Mixer", "DSP IN1", "DSP IN1 Source Selector"},
	{"DSP OUT1 Mixer", "DSP IN2", "DSP IN2 Source Selector"},
	{"DSP OUT1 Mixer", "DSP IN3", "DSP IN3 Source Selector"},
	{"DSP OUT1 Mixer", "DSP IN4", "DSP IN4 Source Selector"},

	{"DSP OUT2 Mixer", "DSP IN1", "DSP IN1 Source Selector"},
	{"DSP OUT2 Mixer", "DSP IN2", "DSP IN2 Source Selector"},
	{"DSP OUT2 Mixer", "DSP IN3", "DSP IN3 Source Selector"},
	{"DSP OUT2 Mixer", "DSP IN4", "DSP IN4 Source Selector"},

	{"DSP OUT3 Mixer", "DSP IN1", "DSP IN1 Source Selector"},
	{"DSP OUT3 Mixer", "DSP IN2", "DSP IN2 Source Selector"},
	{"DSP OUT3 Mixer", "DSP IN3", "DSP IN3 Source Selector"},
	{"DSP OUT3 Mixer", "DSP IN4", "DSP IN4 Source Selector"},

	{"DSP OUT4 Mixer", "DSP IN1", "DSP IN1 Source Selector"},
	{"DSP OUT4 Mixer", "DSP IN2", "DSP IN2 Source Selector"},
	{"DSP OUT4 Mixer", "DSP IN3", "DSP IN3 Source Selector"},
	{"DSP OUT4 Mixer", "DSP IN4", "DSP IN4 Source Selector"},

	{"Mixer", NULL, "Mixer Ch1 Source Selector"},
	{"Mixer", NULL, "Mixer Ch2 Source Selector"},

	{"SDOUT1 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1 Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1 Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1 Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1 Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1 Source Selector", "DMICO", "DMIC"},
	{"SDOUT1 Source Selector", "ADCO", "ADC"},
	{"SDOUT1 Source Selector", "MIXERO", "Mixer"},

	{"SDOUT1B Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1B Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1B Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1B Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1B Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1B Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1B Source Selector", "DMICO", "DMIC"},
	{"SDOUT1B Source Selector", "ADCO", "ADC"},
	{"SDOUT1B Source Selector", "MIXERO", "Mixer"},

	{"SDOUT1C Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1C Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1C Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1C Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1C Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1C Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1C Source Selector", "DMICO", "DMIC"},
	{"SDOUT1C Source Selector", "ADCO", "ADC"},
	{"SDOUT1C Source Selector", "MIXERO", "Mixer"},

	{"SDOUT1D Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT1D Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1D Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1D Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1D Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1D Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1D Source Selector", "DMICO", "DMIC"},
	{"SDOUT1D Source Selector", "ADCO", "ADC"},
	{"SDOUT1D Source Selector", "MIXERO", "Mixer"},

	{"SDOUT2 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT2 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT2 Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT2 Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT2 Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT2 Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT2 Source Selector", "DMICO", "DMIC"},
	{"SDOUT2 Source Selector", "ADCO", "ADC"},
	{"SDOUT2 Source Selector", "MIXERO", "Mixer"},

	{"SDOUT3 Source Selector", "SDIN1", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1B", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1C", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN1D", "SDIN1"},
	{"SDOUT3 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT3 Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT3 Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT3 Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT3 Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT3 Source Selector", "DMICO", "DMIC"},
	{"SDOUT3 Source Selector", "ADCO", "ADC"},
	{"SDOUT3 Source Selector", "MIXERO", "Mixer"},

	{"DSP IN1 Source Selector", "SDIN1", "SDIN1"},
	{"DSP IN1 Source Selector", "SDIN1B", "SDIN1"},
	{"DSP IN1 Source Selector", "SDIN1C", "SDIN1"},
	{"DSP IN1 Source Selector", "SDIN1D", "SDIN1"},
	{"DSP IN1 Source Selector", "SDIN2", "SDIN2"},
	{"DSP IN1 Source Selector", "DSPO1", "DSPO1"},
	{"DSP IN1 Source Selector", "DSPO2", "DSPO2"},
	{"DSP IN1 Source Selector", "DSPO3", "DSPO3"},
	{"DSP IN1 Source Selector", "DSPO4", "DSPO4"},
	{"DSP IN1 Source Selector", "DMICO", "DMIC"},
	{"DSP IN1 Source Selector", "ADCO", "ADC"},
	{"DSP IN1 Source Selector", "MIXERO", "Mixer"},

	{"DSP IN2 Source Selector", "SDIN1", "SDIN1"},
	{"DSP IN2 Source Selector", "SDIN1B", "SDIN1"},
	{"DSP IN2 Source Selector", "SDIN1C", "SDIN1"},
	{"DSP IN2 Source Selector", "SDIN1D", "SDIN1"},
	{"DSP IN2 Source Selector", "SDIN2", "SDIN2"},
	{"DSP IN2 Source Selector", "DSPO1", "DSPO1"},
	{"DSP IN2 Source Selector", "DSPO2", "DSPO2"},
	{"DSP IN2 Source Selector", "DSPO3", "DSPO3"},
	{"DSP IN2 Source Selector", "DSPO4", "DSPO4"},
	{"DSP IN2 Source Selector", "DMICO", "DMIC"},
	{"DSP IN2 Source Selector", "ADCO", "ADC"},
	{"DSP IN2 Source Selector", "MIXERO", "Mixer"},

	{"DSP IN3 Source Selector", "SDIN1", "SDIN1"},
	{"DSP IN3 Source Selector", "SDIN1B", "SDIN1"},
	{"DSP IN3 Source Selector", "SDIN1C", "SDIN1"},
	{"DSP IN3 Source Selector", "SDIN1D", "SDIN1"},
	{"DSP IN3 Source Selector", "SDIN2", "SDIN2"},
	{"DSP IN3 Source Selector", "DSPO1", "DSPO1"},
	{"DSP IN3 Source Selector", "DSPO2", "DSPO2"},
	{"DSP IN3 Source Selector", "DSPO3", "DSPO3"},
	{"DSP IN3 Source Selector", "DSPO4", "DSPO4"},
	{"DSP IN3 Source Selector", "DMICO", "DMIC"},
	{"DSP IN3 Source Selector", "ADCO", "ADC"},
	{"DSP IN3 Source Selector", "MIXERO", "Mixer"},

	{"DSP IN4 Source Selector", "SDIN1", "SDIN1"},
	{"DSP IN4 Source Selector", "SDIN1B", "SDIN1"},
	{"DSP IN4 Source Selector", "SDIN1C", "SDIN1"},
	{"DSP IN4 Source Selector", "SDIN1D", "SDIN1"},
	{"DSP IN4 Source Selector", "SDIN2", "SDIN2"},
	{"DSP IN4 Source Selector", "DSPO1", "DSPO1"},
	{"DSP IN4 Source Selector", "DSPO2", "DSPO2"},
	{"DSP IN4 Source Selector", "DSPO3", "DSPO3"},
	{"DSP IN4 Source Selector", "DSPO4", "DSPO4"},
	{"DSP IN4 Source Selector", "DMICO", "DMIC"},
	{"DSP IN4 Source Selector", "ADCO", "ADC"},
	{"DSP IN4 Source Selector", "MIXERO", "Mixer"},

	{"DAC Source Selector", "SDIN1", "SDIN1"},
	{"DAC Source Selector", "SDIN1B", "SDIN1"},
	{"DAC Source Selector", "SDIN1C", "SDIN1"},
	{"DAC Source Selector", "SDIN1D", "SDIN1"},
	{"DAC Source Selector", "SDIN2", "SDIN2"},
	{"DAC Source Selector", "DSPO1", "DSPO1"},
	{"DAC Source Selector", "DSPO2", "DSPO2"},
	{"DAC Source Selector", "DSPO3", "DSPO3"},
	{"DAC Source Selector", "DSPO4", "DSPO4"},
	{"DAC Source Selector", "DMICO", "DMIC"},
	{"DAC Source Selector", "ADCO", "ADC"},
	{"DAC Source Selector", "MIXERO", "Mixer"},

	{"Mixer Ch1 Source Selector", "SDIN1", "SDIN1"},
	{"Mixer Ch1 Source Selector", "SDIN1B", "SDIN1"},
	{"Mixer Ch1 Source Selector", "SDIN1C", "SDIN1"},
	{"Mixer Ch1 Source Selector", "SDIN1D", "SDIN1"},
	{"Mixer Ch1 Source Selector", "SDIN2", "SDIN2"},
	{"Mixer Ch1 Source Selector", "DSPO1", "DSPO1"},
	{"Mixer Ch1 Source Selector", "DSPO2", "DSPO2"},
	{"Mixer Ch1 Source Selector", "DSPO3", "DSPO3"},
	{"Mixer Ch1 Source Selector", "DSPO4", "DSPO4"},
	{"Mixer Ch1 Source Selector", "DMICO", "DMIC"},
	{"Mixer Ch1 Source Selector", "ADCO", "ADC"},
	{"Mixer Ch1 Source Selector", "MIXERO", "Mixer"},

	{"Mixer Ch2 Source Selector", "SDIN1", "SDIN1"},
	{"Mixer Ch2 Source Selector", "SDIN1B", "SDIN1"},
	{"Mixer Ch2 Source Selector", "SDIN1C", "SDIN1"},
	{"Mixer Ch2 Source Selector", "SDIN1D", "SDIN1"},
	{"Mixer Ch2 Source Selector", "SDIN2", "SDIN2"},
	{"Mixer Ch2 Source Selector", "DSPO1", "DSPO1"},
	{"Mixer Ch2 Source Selector", "DSPO2", "DSPO2"},
	{"Mixer Ch2 Source Selector", "DSPO3", "DSPO3"},
	{"Mixer Ch2 Source Selector", "DSPO4", "DSPO4"},
	{"Mixer Ch2 Source Selector", "DMICO", "DMIC"},
	{"Mixer Ch2 Source Selector", "ADCO", "ADC"},
	{"Mixer Ch2 Source Selector", "MIXERO", "Mixer"},

	{"DIT Source Selector", "SDIN1", "SDIN1"},
	{"DIT Source Selector", "SDIN1B", "SDIN1"},
	{"DIT Source Selector", "SDIN1C", "SDIN1"},
	{"DIT Source Selector", "SDIN1D", "SDIN1"},
	{"DIT Source Selector", "SDIN2", "SDIN2"},
	{"DIT Source Selector", "DSPO1", "DSPO1"},
	{"DIT Source Selector", "DSPO2", "DSPO2"},
	{"DIT Source Selector", "DSPO3", "DSPO3"},
	{"DIT Source Selector", "DSPO4", "DSPO4"},
	{"DIT Source Selector", "DMICO", "DMIC"},
	{"DIT Source Selector", "ADCO", "ADC"},
	{"DIT Source Selector", "MIXERO", "Mixer"},

};

static int ak7759_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

	int fsno, nmax;
	int DIODLbit, addr, value, i;

	akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7759->fs = params_rate(params);

	akdbgprt("\t[AK7759] %s fs=%d\n",__FUNCTION__, ak7759->fs );
	mdelay(10);

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

	fsno = 0;
	nmax = sizeof(sdfstab) / sizeof(sdfstab[0]);
	akdbgprt("\t[AK7759] %s nmax = %d\n",__FUNCTION__, nmax);

	do {
		if ( ak7759->fs <= sdfstab[fsno] ) break;
		fsno++;
	} while ( fsno < nmax );
	akdbgprt("\t[AK7759] %s fsno = %d\n",__FUNCTION__, fsno);

	if ( fsno == nmax ) {
		pr_err("%s: not support Sampling Frequency : %d\n", __func__, ak7759->fs);
		return -EINVAL;
	}

	akdbgprt("\t[AK7759] %s setSDClock\n",__FUNCTION__);
	mdelay(10);

	ak7759->SDfs[0] = fsno;
	setSDClock(codec, 0);

	/* set Word length */
	value = DIODLbit;

	/* set SDIN1-3 data format */
	for ( i = 0 ; i < 3 ; i ++ ) {
		addr = AK7759_21_SDIN1_INPUT_FORMAT + i;
		snd_soc_update_bits(codec, addr, 0x03, value);
	}

	/* set SDOUT1-4 data format */
	for ( i = 0 ; i < 4 ; i ++ ) {
		addr = AK7759_23_SDOUT1_OUTPUT_FORMAT + i;
		snd_soc_update_bits(codec, addr, 0x03, value);
	}

	return 0;
}

static int ak7759_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	return 0;
}

static int ak7759_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_codec *codec = dai->codec;
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int format, diolsb, dioedge[AK7759_SDOUT_NUM], dislot, doslot;
	int msnbit;
	int nSDNo, value;
	int addr, mask, i;

	akdbgprt("[AK7759] %s(%d), fmt=0x%08x\n", __FUNCTION__, __LINE__, fmt);

	nSDNo = 0;

	/* set master/slave audio interface */
	akdbgprt("(fmt & SND_SOC_DAIFMT_MASTER_MASK)=0x%08x\n",(fmt & SND_SOC_DAIFMT_MASTER_MASK));
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBS_CFS:
		msnbit = 0;
		akdbgprt("\t[AK7759] %s(Slave_nSDNo=%d)\n",__FUNCTION__,nSDNo);
            break;
        case SND_SOC_DAIFMT_CBM_CFM:
		msnbit = 1;
		akdbgprt("\t[AK7759] %s(Master_nSDNo=%d)\n",__FUNCTION__,nSDNo);
            break;
        case SND_SOC_DAIFMT_CBS_CFM:
        case SND_SOC_DAIFMT_CBM_CFS:
        default:
            dev_err(codec->dev, "Clock mode unsupported");
           return -EINVAL;
    	}

	akdbgprt("(fmt & SND_SOC_DAIFMT_FORMAT_MASK)=0x%08x\n",(fmt & SND_SOC_DAIFMT_MASTER_MASK));
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			format = AK7759_LRIF_I2S_MODE; // 0
			diolsb = 0;
			dioedge[0] = ak7759->DIOEDGEbit;
			for (i=1; i < AK7759_SDOUT_NUM; i++) dioedge[i] = 0;
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			format = AK7759_LRIF_MSB_MODE; // 5
			diolsb = 0;
			dioedge[0] = ak7759->DIOEDGEbit;
			for (i=1; i < AK7759_SDOUT_NUM; i++) dioedge[i] = 0;
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			format = AK7759_LRIF_LSB_MODE; // 5
			diolsb = 1;
			dioedge[0] = ak7759->DIOEDGEbit;
			for (i=1; i < AK7759_SDOUT_NUM; i++) dioedge[i] = 0;
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_DSP_A: //PCM Short
			format = AK7759_LRIF_PCM_SHORT_MODE; // 6
			diolsb = 0;
			for (i=0; i<AK7759_SDOUT_NUM; i++) dioedge[i] = 1;
			dislot = ak7759->DIOSLbit;
			doslot = ak7759->DIOSLbit;
			break;
		case SND_SOC_DAIFMT_DSP_B: //PCM Long
			format = AK7759_LRIF_PCM_LONG_MODE; // 7
			diolsb = 0;
			for (i=0; i<AK7759_SDOUT_NUM; i++) dioedge[i] = 1;
			dislot = ak7759->DIOSLbit;
			doslot = ak7759->DIOSLbit;
			break;
		default:
			return -EINVAL;
	}

	akdbgprt("[AK7759] format=0x%x\n", format);
	/* set format */
	setSDMaster(codec, nSDNo, msnbit);
	addr = AK7759_20_CLOCK_FORMAT_SETTING;
	value = format<<4;
	mask = 0xF0;
	snd_soc_update_bits(codec, addr, mask, value);

	/* set SDIN1-2 data format */
	for ( i = 0 ; i < AK7759_SIN_NUM ; i ++ ) {
		addr = AK7759_21_SDIN1_INPUT_FORMAT + i;
		value = (dioedge[i] <<  7) + (diolsb <<  3) + (dislot <<  4);
		snd_soc_update_bits(codec, addr, 0xB8, value);
	}

	/* set SDOUT1-3 data format */
	for ( i = 0 ; i < AK7759_SDOUT_NUM ; i ++ ) {
		addr = AK7759_23_SDOUT1_OUTPUT_FORMAT + i;
		value = (dioedge[i] <<  7) + (diolsb <<  3) + (doslot <<  4);
		snd_soc_update_bits(codec, addr, 0xB8, value);
	}

	return 0;
}


static bool ak7759_volatile(struct device *dev, unsigned int reg)
{
	bool	ret;

#ifdef AK7759_DEBUG
	if ( reg < AK7759_VIRT_84_DSP1OUT1_MIX ) ret = true;
	else ret = false;
#else
	if ( reg < AK7759_80_DEVNO  ) ret = false;
	else if ( reg < AK7759_VIRT_84_DSP1OUT1_MIX ) ret = true;
	else ret = false;
#endif
	return(ret);
}

static bool ak7759_readable(struct device *dev, unsigned int reg)
{

	if (reg > AK7759_MAX_REGISTER) return false;
	return ak7759_access_masks[reg].readable != 0;

}

static bool ak7759_writeable(struct device *dev, unsigned int reg)
{

	if (reg > AK7759_MAX_REGISTER) return false;
	return ak7759_access_masks[reg].writable != 0;

}

static int ak7759_i2c_read(
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
		return ret;
	else
		return -EIO;
}

unsigned int ak7759_read_register(struct snd_soc_codec *codec, unsigned int reg)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[3], rx[1];
	int	wlen, rlen;
	int val, ret;
	unsigned int rdata;

	if (reg == SND_SOC_NOPM)
		return 0;

	if (!ak7759_readable(NULL, reg)) return(-EINVAL);

	if (!ak7759_volatile(NULL, reg) && ak7759_readable(NULL, reg)) {
		ret = regmap_read(ak7759->regmap, reg, &val);  
		if (ret >= 0) {
			return val;
		} 
        else {
			dev_err(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
			return 0;
		}
	}

	wlen = 3;
	rlen = 1;
	tx[0] = (unsigned char)(COMMAND_READ_REG & 0x7F);
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);

	if (ak7759->control_type == SND_SOC_SPI) {
		ret = spi_write_then_read(ak7759->spi, tx, wlen, rx, rlen);
	}
	else {
		ret = ak7759_i2c_read(ak7759->i2c, tx, wlen, rx, rlen);
	}

	if (ret < 0) {
		akdbgprt("\t[AK7759] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}

	return rdata;
}

static int ak7759_reads(struct snd_soc_codec *codec, u8 *tx, size_t wlen, u8 *rx, size_t rlen)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int ret;

	pr_debug("*****[AK7759] %s tx[0]=0x%02x, raddr=0x%02x%02x, wlen=%d, rlen=%d\n",__FUNCTION__, tx[0],tx[1],tx[2],(int)wlen,(int)rlen);
	if (ak7759->control_type == SND_SOC_SPI) {
		ret = spi_write_then_read(ak7759->spi, tx, wlen, rx, rlen);
	}
	else {
		ret = ak7759_i2c_read(ak7759->i2c, tx, wlen, rx, rlen);
	}

	return ret;  

}

static int ak7759_write_register(struct snd_soc_codec *codec,  unsigned int reg,  unsigned int value)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int ret;

	akdbgprt("[AK7759] %s (%x, %x)\n",__FUNCTION__, reg, value);

	if (reg == SND_SOC_NOPM)
		return 0;

	if (!ak7759_writeable(NULL, reg)) return(-EINVAL);

	if (!ak7759_volatile(NULL, reg)) {
		ret = regmap_write(ak7759->regmap, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	wlen = 4;
	tx[0] = (unsigned char)COMMAND_WRITE_REG;
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);
	tx[3] = value;

	if (ak7759->control_type == SND_SOC_SPI)
		ret = spi_write(ak7759->spi, tx, wlen);
	else 
		ret = i2c_master_send(ak7759->i2c, tx, wlen);

	return ret;

}


static int ak7759_writes(struct snd_soc_codec *codec, const u8 *tx, size_t wlen)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int rc;

	if ( wlen > 3 ) {
		akdbgprt("[AK7759] %s tx[0]=%02x tx[1]=%02x tx[2]=%02x, tx[3]=%02x, wlen=%d\n",__FUNCTION__, (int)tx[0], (int)tx[1], (int)tx[2], (int)tx[3], (int)wlen);
	}

	if (ak7759->control_type == SND_SOC_SPI)
		rc = spi_write(ak7759->spi, tx, wlen);
	else
		rc = i2c_master_send(ak7759->i2c, tx, wlen);

	if(rc != wlen) {
		pr_err("%s: comm error, rc %d, wlen %d\n", __func__, rc, (int)wlen);
		//dbg_show(__func__);
	}

	pr_debug("%s return %d\n", __func__, rc);
	return rc;
}


static int crc_read(struct snd_soc_codec *codec)
{
	int rc;

	u8	tx[3], rx[2];

	tx[0] = COMMAND_CRC_READ;
	tx[1] = 0;
	tx[2] = 0;

	rc =  ak7759_reads(codec, tx, 3, rx, 2);

	return (rc < 0) ? rc : ((rx[0] << 8) + rx[1]);
}

static int ak7759_set_status(struct snd_soc_codec *codec, enum ak7759_status status)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int value;

	switch (status) {
		case RUN:
			snd_soc_update_bits(ak7759->codec, AK7759_01_SYSCLOCK_SETTING2, 0x81, 0x80);  // CKRESETN = 1
			mdelay(10);
			snd_soc_update_bits(ak7759->codec, AK7759_51_RESET_CONTROL, 0x0D, 0x0D);  // CRESETN = DSPRESETN = HRESETN = 1;
			break;
		case DOWNLOAD:
			value = snd_soc_read(codec, AK7759_51_RESET_CONTROL);
			if ( value & 0x04 ) {
				ak7759->DSPon = 1;
				snd_soc_update_bits(ak7759->codec, AK7759_51_RESET_CONTROL, 0x04, 0x00);   // DSPRESETN=0
			}
			else {
				ak7759->DSPon = 0;
			}
			snd_soc_update_bits(ak7759->codec, AK7759_01_SYSCLOCK_SETTING2, 0x01, 0x01);  // DLRDY = 1
			mdelay(1);
			break;
		case DOWNLOAD_FINISH:
			snd_soc_update_bits(ak7759->codec, AK7759_01_SYSCLOCK_SETTING2, 0x01, 0x00); // DLRDY = 0
			mdelay(1);
			if ( ak7759->DSPon == 1 ) {
				snd_soc_update_bits(ak7759->codec, AK7759_51_RESET_CONTROL, 0x04, 0x04);   // DSPRESETN=1
			}
			break;
		case STANDBY:
			snd_soc_update_bits(ak7759->codec, AK7759_01_SYSCLOCK_SETTING2, 0x81, 0x80);  // CKRESETN = 1
			mdelay(10);
			snd_soc_update_bits(ak7759->codec, AK7759_51_RESET_CONTROL, 0x0F, 0x00);
			break;
		case SUSPEND:
		case POWERDOWN:
			snd_soc_update_bits(ak7759->codec, AK7759_01_SYSCLOCK_SETTING2, 0x81, 0x00); // CKRESETN = DLRDY = 0
			snd_soc_update_bits(ak7759->codec, AK7759_51_RESET_CONTROL, 0x0F, 0x00);
			break;
		default:
		return -EINVAL;
	}

	ak7759->status = status;

	return 0;
}

static int ak7759_ram_download(struct snd_soc_codec *codec, const u8 *tx_ram, u64 num, u16 crc)
{
	int rc;
	u16	read_crc;

	akdbgprt("\t[AK7759] %s num=%ld\n",__FUNCTION__, (long int)num);

	ak7759_set_status(codec, DOWNLOAD);
	
	rc = ak7759_writes(codec, tx_ram, num);
	if (rc < 0) {		
		ak7759_set_status(codec, DOWNLOAD_FINISH);
		return rc;
	}

	if ( ( crc != 0 ) && (rc >= 0) )  {
		read_crc = crc_read(codec);
		akdbgprt("\t[AK7759] %s CRC Cal=%x Read=%x\n",__FUNCTION__, (int)crc,(int)read_crc);		
	
		if ( read_crc == crc ) rc = 0;
		else rc = 1;
	}

	ak7759_set_status(codec, DOWNLOAD_FINISH);

	return rc;

}

static int calc_CRC(int length, u8 *data )
{

#define CRC16_CCITT (0x1021)

	unsigned short crc = 0x0000;
	int i, j;

	for ( i = 0; i < length; i++ ) {
		crc ^= *data++ << 8;
		for ( j = 0; j < 8; j++) {
			if ( crc & 0x8000) {
				crc <<= 1;
				crc ^= CRC16_CCITT;
			}
			else {
				crc <<= 1;
			}
		}
	}

	akdbgprt("[AK7759] %s CRC=%x\n",__FUNCTION__, crc);

	return crc;
}

static int ak7759_write_ram(
struct snd_soc_codec *codec,
int	 nPCRam,  // 0 : PRAM, 1 : CRAM, 2:  OFREG
u8 	*upRam,
int	 nWSize)
{
	int n, ret;
	int	wCRC;
	int nMaxSize;
	int devWord;
	int devLen;
	int restLen;
	u8 	*ramdata;

	akdbgprt("[AK7759] %s RamNo=%d, len=%d\n",__FUNCTION__, nPCRam, nWSize);

	devWord = 0;
	devLen = nWSize;

	switch(nPCRam) {
		case RAMTYPE_PRAM:
			nMaxSize = TOTAL_NUM_OF_PRAM_MAX;
			if (  nWSize > nMaxSize ) {
				printk("%s: PRAM Write size is over! \n", __func__);      
				return(-EINVAL);
			}
			devWord = ((int)upRam[1] << 8 ) + (int)upRam[2];
			if ( devWord != 0 ) {
				devLen = 5 * devWord + 3;
			}
			akdbgprt("[AK7759] %s dev Length=%d\n",__FUNCTION__, devLen);
			break;
		case RAMTYPE_CRAM:
			nMaxSize = TOTAL_NUM_OF_CRAM_MAX;
			if (  nWSize > nMaxSize ) {
				printk("%s: CRAM Write size is over! \n", __func__);      
				return(-EINVAL);
			}
			break;
		case RAMTYPE_OFREG:
			nMaxSize = TOTAL_NUM_OF_OFREG_MAX;
			if (  nWSize > nMaxSize ) {
				printk("%s: OFREG Write size is over! \n", __func__);      
				return(-EINVAL);
			}
			break;		
		default:
			break;
	}

	ramdata = upRam;
	restLen = nWSize;

	if ( devWord != 0 ) {
		ramdata += 3;
		restLen -= 3;
	}

	do {
		if ( restLen < devLen ) devLen = restLen;
		wCRC = calc_CRC(devLen, ramdata);
		n = MAX_LOOP_TIMES;
		do {
			ret = ak7759_ram_download(codec, ramdata, devLen, wCRC);
			if ( ret >= 0 ) break;
			n--;
		} while ( n > 0 );

		if ( ret < 0 ) {
			printk("%s: RAM Write Error! RAM No = %d \n", __func__, nPCRam); 
			return(-EINVAL);
		}
		restLen -= devLen;
		if ( restLen > 0 ) ramdata += devLen;
	}while( restLen > 0 );

	return(0);

}

static int ak7759_firmware_write_ram(struct snd_soc_codec *codec, u16 mode, u16 cmd)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int nNumMode, nMaxLen;
	int nRamSize;
	u8  *ram_basic;
	const struct firmware *fw;
	u8  *fwdn;
	char szFileName[32];

	akdbgprt("[AK7759] %s mode=%d, cmd=%d\n",__FUNCTION__, mode, cmd);

	switch(mode) {
		case RAMTYPE_PRAM:
			akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__); // Temp
			nNumMode = sizeof(ak7759_firmware_pram) / sizeof(ak7759_firmware_pram[0]);
			break;
		case RAMTYPE_CRAM:
			nNumMode = sizeof(ak7759_firmware_cram) / sizeof(ak7759_firmware_cram[0]);
			break;
		case RAMTYPE_OFREG:
			nNumMode = sizeof(ak7759_firmware_ofreg) / sizeof(ak7759_firmware_ofreg[0]);
			break;
		default:
			akdbgprt("[AK7759] %s mode Error=%d\n",__FUNCTION__, mode);
			return( -EINVAL);
	}

	if ( cmd == 0 ) return(0);

	if ( cmd >= nNumMode ) {
		pr_err("%s: invalid command %d\n", __func__, cmd);
		return( -EINVAL);
	}

	if ( cmd == 1 ) {
		switch(mode) {
			case RAMTYPE_PRAM:
				ram_basic = ak7759_pram_basic;
				nRamSize = sizeof(ak7759_pram_basic);
				break;
			case RAMTYPE_CRAM:
				ram_basic = ak7759_cram_basic;
				nRamSize = sizeof(ak7759_cram_basic);
				break;
			case RAMTYPE_OFREG:
				ram_basic = ak7759_ofreg_basic;
				nRamSize = sizeof(ak7759_ofreg_basic);
				break;
			default:
				return( -EINVAL);
		}
		ret = ak7759_write_ram(codec, (int)mode, ram_basic, nRamSize);
	}
	
	else {
		switch(mode) {
			case RAMTYPE_PRAM:
				akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__); // Temp
				sprintf(szFileName, "ak7759_pram_%s.bin", ak7759_firmware_pram[cmd]);
				nMaxLen = TOTAL_NUM_OF_PRAM_MAX;
				break;
			case RAMTYPE_CRAM:
				sprintf(szFileName, "ak7759_cram_%s.bin", ak7759_firmware_cram[cmd]);
				nMaxLen = TOTAL_NUM_OF_CRAM_MAX;
				break;
			case RAMTYPE_OFREG:
				sprintf(szFileName, "ak7759_ofreg_%s.bin", ak7759_firmware_ofreg[cmd]);
				nMaxLen = TOTAL_NUM_OF_OFREG_MAX;
				break;
			default:
				return( -EINVAL);
		}

		if (ak7759->control_type == SND_SOC_SPI) {
			ret = request_firmware(&fw, szFileName, &(ak7759->spi->dev));
		}
		else {
			akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__); // Temp
			ret = request_firmware(&fw, szFileName, &(ak7759->i2c->dev));
		}
		if (ret) {
			akdbgprt("[AK7759] %s could not load firmware=%d\n", szFileName, ret);
			return -EINVAL;
		}

		akdbgprt("[AK7759] %s name=%s size=%d\n",__FUNCTION__, szFileName, (int)fw->size);
		if ( fw->size > nMaxLen ) {
			akdbgprt("[AK7759] %s RAM Size Error : %d\n",__FUNCTION__, (int)fw->size);
			release_firmware(fw);
			return -ENOMEM;
		}

		fwdn = kmalloc((unsigned long)fw->size, GFP_KERNEL);
		if (fwdn == NULL) {
			printk(KERN_ERR "failed to buffer vmalloc: %d\n", (int)fw->size);
			release_firmware(fw);
			return -ENOMEM;
		}

		akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__); // Temp
		memcpy((void *)fwdn, fw->data, fw->size);

		ret = ak7759_write_ram(codec, (int)mode, (u8 *)fwdn, (fw->size));

		kfree(fwdn);
		release_firmware(fw);

		if (mode == RAMTYPE_CRAM) {
			ak7759_set_status(codec, DOWNLOAD);
			ak7759_set_status(codec, DOWNLOAD_FINISH);	
		}

	}

	return ret;
}

static int ak7759_write_cram(
struct snd_soc_codec *codec,
int addr,
int len,
unsigned char *cram_data)
{
	int i, n, ret;
	int nDSPRun;
	unsigned char tx[51];

	akdbgprt("[AK7759] %s addr=0x%02x, len=%d\n",__FUNCTION__, addr, len);

	if ( len > 48 ) {
		akdbgprt("[AK7759] %s Length over!\n",__FUNCTION__);
		return(-EINVAL);
	}

	nDSPRun = snd_soc_read(codec, AK7759_51_RESET_CONTROL);

	if ( nDSPRun & 0x04 ) {
		tx[0] = COMMAND_WRITE_CRAM_RUN + (unsigned char)((len / 3) - 1);
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}
	else {
		ak7759_set_status(codec, DOWNLOAD);
		tx[0] = COMMAND_WRITE_CRAM;
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}

	n = 3;
	for ( i = 0 ; i < len; i ++ ) {
		tx[n++] = cram_data[i];
	}

	akdbgprt("****** AK7759 CRAM Write ******\n");
	akdbgprt("AK7759 CAddr : %02x %02x\n", (int)tx[1], (int)tx[2]);
	akdbgprt("AK7759 CData : %02x %02x %02x\n", (int)cram_data[0], (int)cram_data[1], (int)cram_data[2]);

	ret = ak7759_writes(codec, tx, n);

	if ( nDSPRun & 0x04 ) {
		tx[0] = COMMAND_WRITE_CRAM_EXEC;
		tx[1] = 0;
		tx[2] = 0;
		ret = ak7759_writes(codec, tx, 3);
	}
	else {
		ak7759_set_status(codec, DOWNLOAD_FINISH);
	}

	return ret;
}

/*
static int ak7759_mask_write_cram(
struct snd_soc_codec *codec,
int nDSPNo,
int addr,
int num,
unsigned long *cram_data)
{

	u8   bCram[48];
	int  i;
	int  n;
	int  ret;

	if ( num > 16 ) {
		akdbgprt("[AK7759] %s Length over!\n",__FUNCTION__);
		return(-EINVAL);
	}

 	n = 0;

	for (i = 0; i < num; i++) {
		bCram[n++] = (0xFF & (cram_data[i] >> 16));
		bCram[n++] = (0xFF & (cram_data[i]>> 8));
		bCram[n++] = (0xFF & (cram_data[i]));
	}

	ret = ak7759_write_cram(codec, nDSPNo, addr, n, bCram);

	return ret;
}
*/

static int ak7759_read_cram(
struct snd_soc_codec *codec,
int addr)
{
	int n;
	int nDSPRun;
	unsigned char tx[3], rx[3];

	nDSPRun = snd_soc_read(codec, AK7759_51_RESET_CONTROL);
	/*  DSPRESETN            D2RESETN           HRESETN     */
	if((nDSPRun & 0x4) || (nDSPRun & 0x2) || (nDSPRun & 0x1))
	{	
		/* DSP reset ON */
        snd_soc_write(codec,AK7759_51_RESET_CONTROL, (nDSPRun & 0xF8));
	}

	tx[0] = (COMMAND_WRITE_CRAM & 0x3F);
	tx[1] = (unsigned char)(0x0F & (addr >> 8));
	tx[2] = (unsigned char)(0xFF & addr);

	for ( n = 0 ; n < 3 ; n++ ) rx[n] = 0;

	ak7759_set_status(codec, DOWNLOAD);
	ak7759_reads(codec, tx, 3, rx, 3);
	ak7759_set_status(codec, DOWNLOAD_FINISH);	

	printk("****** CRAM Read ******\n");
	printk("CAddr : %02x %02x\n", (int)tx[1], (int)tx[2]);
	printk("CData : %02x %02x %02x\n", (int)rx[0], (int)rx[1], (int)rx[2]);

	/* Restore DSP reset back to original */
	snd_soc_update_bits(codec, AK7759_51_RESET_CONTROL, 0x07, nDSPRun);

	return 0;
}

static unsigned long ak7759_readMIR(
struct snd_soc_codec *codec,
int nMIRNo,
unsigned long *dwMIRData)
{
	unsigned char tx[3];
	unsigned char rx[32];
	int n, nRLen;

	if ( nMIRNo >= 8 ) return(-EINVAL);

	tx[0] = (unsigned char)COMMAND_MIR_READ;
	tx[1] = 0;
	tx[2] = 0;
	nRLen = 4 * (nMIRNo + 1);

	ak7759_reads(codec, tx, 3, rx, nRLen);

	n = 4 * nMIRNo;
	*dwMIRData = ((0xFF & (unsigned long)rx[n++]) << 20);
	*dwMIRData += ((0xFF & (unsigned long)rx[n++]) << 12);
    *dwMIRData += ((0xFF & (unsigned long)rx[n++]) << 4);
    *dwMIRData += ((0xFF & (unsigned long)rx[n++]) >> 4);

	return(0);

}

static int ak7759_reg_cmd(struct snd_soc_codec *codec, REG_CMD *reg_param, int len)
{
	int i;
	int rc;
	int  addr, value;	

	akdbgprt("*****[AK7759] %s len = %d\n",__FUNCTION__, len);

	rc = 0;
	for (i = 0; i < len; i++) {
		addr = (int)(reg_param[i].addr);
		value = (int)(reg_param[i].data);

		if ( addr != 0xFF ) {
			rc = snd_soc_write(codec, addr, value);
			if (rc < 0) {
				break;
			}
		}
		else {
			mdelay(value);
		}
	}

	if (rc != 0 ) rc = 0;

	return(rc);

}

#ifdef AK7759_IO_CONTROL
static long ak7759_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	struct ak7759_priv *ak7759 = (struct ak7759_priv*)file->private_data;
	struct ak7759_wreg_handle ak7759_wreg;
	struct ak7759_wcram_handle  ak7759_wcram;
	struct ak7759_rcram_handle  ak7759_rcram;
	void __user *data = (void __user *)args;
	int *val = (int *)args;
	int i;
	unsigned long dwMIRData;
	int ret = 0;
	REG_CMD      regcmd[MAX_WREG];
	unsigned char cram_data[MAX_WCRAM];

	akdbgprt("*****[AK7759] %s cmd, val=%x, %x\n",__FUNCTION__, cmd, val[0]);

	switch(cmd) {
	case AK7759_IOCTL_READCRAM: 
		if (copy_from_user(&ak7759_rcram, data, sizeof(struct ak7759_rcram_handle)))
			return -EFAULT;

		ret = ak7759_read_cram(ak7759->codec, ak7759_rcram.addr);

		break;
	case AK7759_IOCTL_WRITECRAM:
		if (copy_from_user(&ak7759_wcram, data, sizeof(struct ak7759_wcram_handle)))
			return -EFAULT;
		if ( (  ak7759_wcram.len % 3 ) != 0 ) {
			printk(KERN_ERR "[AK7759] %s CRAM len error\n",__FUNCTION__);
			return -EFAULT;
		}
		if ( ( ak7759_wcram.len < 3 ) || ( ak7759_wcram.len > MAX_WCRAM ) ) {
			printk(KERN_ERR "[AK7759] %s CRAM len error2\n",__FUNCTION__);
			return -EFAULT;
		}
		for ( i = 0 ; i < ak7759_wcram.len ; i ++ ) {
			cram_data[i] = ak7759_wcram.cram[i];
		}
		ret = ak7759_write_cram(ak7759->codec, ak7759_wcram.addr, ak7759_wcram.len, cram_data);
		break;
	case AK7759_IOCTL_WRITEREG:
		if (copy_from_user(&ak7759_wreg, data, sizeof(struct ak7759_wreg_handle)))
			return -EFAULT;
		if ( ( ak7759_wreg.len < 1 ) || ( ak7759_wreg.len > MAX_WREG ) ) {
			printk(KERN_ERR "MAXREG ERROR %d\n", ak7759_wreg.len );
			return -EFAULT;
		}
		for ( i = 0 ; i < ak7759_wreg.len; i ++ ) {
			regcmd[i].addr = ak7759_wreg.regcmd[i].addr;
			regcmd[i].data = ak7759_wreg.regcmd[i].data;
		}
		ak7759_reg_cmd(ak7759->codec, regcmd, ak7759_wreg.len);
		break;	
	case AK7759_IOCTL_SETSTATUS:
		ret = ak7759_set_status(ak7759->codec, val[0]);
		if (ret < 0) {
			printk(KERN_ERR "ak7759: set_status error: \n");
			return ret;
		}
		break;
	case AK7759_IOCTL_SETMIR:
		ak7759->MIRNo = val[0];
		if (ret < 0) {
			printk(KERN_ERR "ak7759: set MIR error\n");
			return -EFAULT;
		}
		break;
	case AK7759_IOCTL_GETMIR:
		ak7759_readMIR(ak7759->codec, (0xF & (ak7759->MIRNo)), &dwMIRData);
		ret = copy_to_user(data, (const void*)&dwMIRData, (unsigned long)4);
		if (ret < 0) {
			printk(KERN_ERR "ak7759: get status error\n");
			return -EFAULT;
		}
		break;
		break;
	default:
		printk(KERN_ERR "Unknown command required: %d\n", cmd);
		return -EINVAL;
	}

	return ret;
}

static int init_ak7759_pd(struct ak7759_priv *data)
{
	struct _ak7759_pd_handler *ak7759 = &ak7759_pd_handler;

	if (data == NULL)
		return -EFAULT;

	mutex_init(&ak7759->lock);

	mutex_lock(&ak7759->lock);
	ak7759->data = data;
	mutex_unlock(&ak7759->lock);

	akdbgprt("data:%p, ak7759->data:%p\n", data, ak7759->data);

	return 0;
}

static struct ak7759_priv* get_ak7759_pd(void)
{
	struct _ak7759_pd_handler *ak7759 = &ak7759_pd_handler;

	if (ak7759->data == NULL)
		return NULL;

	mutex_lock(&ak7759->lock);
	ak7759->ref_count++;
	mutex_unlock(&ak7759->lock);

	return ak7759->data;
}

static int rel_ak7759_pd(struct ak7759_priv *data)
{
	struct _ak7759_pd_handler *ak7759 = &ak7759_pd_handler;

	if (ak7759->data == NULL)
		return -EFAULT;

	mutex_lock(&ak7759->lock);
	ak7759->ref_count--;
	mutex_unlock(&ak7759->lock);

	data = NULL;

	return 0;
}

/* AK7759 Misc driver interfaces */
static int ak7759_open(struct inode *inode, struct file *file)
{
	struct ak7759_priv *ak7759;

	ak7759 = get_ak7759_pd();
	file->private_data = ak7759;

	return 0;
}

static int ak7759_close(struct inode *inode, struct file *file)
{
	struct ak7759_priv *ak7759 = (struct ak7759_priv*)file->private_data;

	rel_ak7759_pd(ak7759);

	return 0;
}

static const struct file_operations ak7759_fops = {
	.owner = THIS_MODULE,
	.open = ak7759_open,
	.release = ak7759_close,
	.unlocked_ioctl = ak7759_ioctl,
};

static struct miscdevice ak7759_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ak7759-dsp",
	.fops = &ak7759_fops,
};
#endif

static int ak7759_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
#ifndef KERNEL_3_18_XX
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
#endif

	akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (level) {
	case SND_SOC_BIAS_ON:
		akdbgprt("[AK7759] SND_SOC_BIAS_ON\n");
		break;
	case SND_SOC_BIAS_PREPARE:
		akdbgprt("[AK7759] SND_SOC_BIAS_PREPARE\n");
		break;
	case SND_SOC_BIAS_STANDBY:
		akdbgprt("[AK7759] SND_SOC_BIAS_STANDBY\n");
		break;
	case SND_SOC_BIAS_OFF:
		akdbgprt("\t[AK7759] SND_SOC_BIAS_OFF\n");
		ak7759_set_status(codec, POWERDOWN);
		break;
	}

#ifdef KERNEL_3_18_XX
	codec->dapm.bias_level = level;
#else
	dapm->bias_level = level;
#endif

	return 0;
}



static int ak7759_set_dai_mute(struct snd_soc_dai *dai, int mute) 
{
	akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	if (mute) {
		akdbgprt("\t[AK7759] Mute ON(%d)\n",__LINE__);
	} else {
		akdbgprt("\t[AK7759] Mute OFF(%d)\n",__LINE__);
	}

	return 0;
}

#define AK7759_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
				SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
				SNDRV_PCM_RATE_24000 | SNDRV_PCM_RATE_32000 |\
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define AK7759_DAC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE
#define AK7759_ADC_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE


static struct snd_soc_dai_ops ak7759_dai_ops = {
	.hw_params	= ak7759_hw_params,
	.set_sysclk	= ak7759_set_dai_sysclk,
	.set_fmt	= ak7759_set_dai_fmt,
	.digital_mute = ak7759_set_dai_mute,
};

struct snd_soc_dai_driver ak7759_dai[] = {   
	{										 
		.name = "ak7759-aif1",
		.id = AIF_PORT1,
		.playback = {
		       .stream_name = "Playback",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK7759_RATES,
		       .formats = AK7759_DAC_FORMATS,
		},
		.capture = {
		       .stream_name = "Capture",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK7759_RATES,
		       .formats = AK7759_ADC_FORMATS,
		},
		.ops = &ak7759_dai_ops,
	},
};

static int ak7759_init_reg(struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int i, devid;
	
	akdbgprt("\t[AK7759] %s\n",__FUNCTION__);

	if ( ak7759->pdn_gpio > 0 )  {
		gpio_set_value(ak7759->pdn_gpio, 0);	
		msleep(1);
		gpio_set_value(ak7759->pdn_gpio, 1);	
		msleep(1);
	}

	devid = snd_soc_read(codec, AK7759_80_DEVNO);
	printk("[AK7759] %s  Device ID = 0x%X\n",__FUNCTION__, devid);
	if ( devid != 0x59 ) {
		akdbgprt("***** This is not AK7759! *****\n");
		akdbgprt("***** This is not AK7759! *****\n");
		akdbgprt("***** This is not AK7759! *****\n");
	}

//	ak7759_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	ak7759->fs = 48000;
	ak7759->PLLInput = 1;
	ak7759->XtiFs    = 0;
	ak7759->BUSMclk  = 1;  // 0:24.576MHz,  1:12.288MHz

	setPLLOut(codec);
	setBUSClock(codec);

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak7759->SDBick[i]  = 0; //64fs
		ak7759->SDfs[i] = 5;  // 48kHz
		ak7759->SDCks[i] = 1;  // PLLMCLK

		setSDClock(codec, i);
	}

	setSDMaster(codec, 0, 0); //SD1 Slave
	setSDMaster(codec, 1, 1); //SD2 Master mode Fix

	snd_soc_write(codec, AK7759_08_SYNCPORT_SELECT, 0x11);    // EXBCK1=EXBCK2=1
	snd_soc_write(codec, AK7759_09_SYNCDOMAIN_SELECT1, 0x10); // SDBCK=1
	snd_soc_write(codec, AK7759_0A_SYNCDOMAIN_SELECT2, 0x11); // SDOUT1,2 : SD1
	snd_soc_write(codec, AK7759_0B_SYNCDOMAIN_SELECT3, 0x10); // SDOUT3 : SD1
	snd_soc_write(codec, AK7759_0C_SYNCDOMAIN_SELECT4, 0x11); // DSPO1,2 : SD1
	snd_soc_write(codec, AK7759_0D_SYNCDOMAIN_SELECT5, 0x11); // DSPO3,4 : SD1
	snd_soc_write(codec, AK7759_0E_SYNCDOMAIN_SELECT6, 0x11); // DSP,CODEC : SD1
	snd_soc_write(codec, AK7759_0F_SYNCDOMAIN_SELECT7, 0x10); // MIXER : SD1

	snd_soc_write(codec, AK7759_4D_OUT_SW, 0xCA);  // OUT1/OUT2 : Differential /DAC Out

	return 0;
}

static int ak7759_init_reg_for_basicram(struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int i, devid;
	
	akdbgprt("\t[AK7759] %s\n",__FUNCTION__);

	if ( ak7759->pdn_gpio > 0 )  {
		gpio_set_value(ak7759->pdn_gpio, 0);	
		msleep(1);
		gpio_set_value(ak7759->pdn_gpio, 1);	
		msleep(1);
	}

	devid = snd_soc_read(codec, AK7759_80_DEVNO);
	printk("[AK7759] %s  Device ID = 0x%X\n",__FUNCTION__, devid);
	if ( devid != 0x59 ) {
		akdbgprt("***** This is not AK7759! *****\n");
		akdbgprt("***** This is not AK7759! *****\n");
		akdbgprt("***** This is not AK7759! *****\n");
	}

//	ak7759_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	ak7759->fs = 48000;
	ak7759->PLLInput = 1;
	ak7759->XtiFs    = 0;
	ak7759->BUSMclk  = 1;  // 0:24.576MHz,  1:12.288MHz

	ak7759->DSPPramMode = 1; //basic ram
	ak7759->DSPCramMode = 1; //basic ram

	setPLLOut(codec);
	setBUSClock(codec);

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak7759->SDBick[i]  = 0; //64fs
		ak7759->SDfs[i] = 5;  // 48kHz
		ak7759->SDCks[i] = 1;  // PLLMCLK

		setSDClock(codec, i);
	}

	setSDMaster(codec, 0, 0); //SD1 Slave
	setSDMaster(codec, 1, 1); //SD2 Master mode Fix

	snd_soc_write(codec, AK7759_08_SYNCPORT_SELECT, 0x11);    // EXBCK1=EXBCK2=1
	snd_soc_write(codec, AK7759_09_SYNCDOMAIN_SELECT1, 0x10); // SDBCK=1
	snd_soc_write(codec, AK7759_0A_SYNCDOMAIN_SELECT2, 0x11); // SDOUT1,2 : SD1
	snd_soc_write(codec, AK7759_0B_SYNCDOMAIN_SELECT3, 0x10); // SDOUT3 : SD1
	snd_soc_write(codec, AK7759_0C_SYNCDOMAIN_SELECT4, 0x11); // DSPO1,2 : SD1
	snd_soc_write(codec, AK7759_0D_SYNCDOMAIN_SELECT5, 0x11); // DSPO3,4 : SD1
	snd_soc_write(codec, AK7759_0E_SYNCDOMAIN_SELECT6, 0x11); // DSP,CODEC : SD1
	snd_soc_write(codec, AK7759_0F_SYNCDOMAIN_SELECT7, 0x10); // MIXER : SD1

	snd_soc_write(codec, AK7759_4D_OUT_SW, 0xCA);  // OUT1/OUT2 : Single-ended /DAC Out
	snd_soc_write(codec, AK7759_4C_AIN_SW, 0x30);  // ADC Lch/Rch Single-ended

	ak7759_firmware_write_ram(codec, RAMTYPE_PRAM, ak7759->DSPPramMode); 
	ak7759_firmware_write_ram(codec, RAMTYPE_CRAM, ak7759->DSPCramMode); 

	snd_soc_write(codec, AK7759_16_DSP_IN1_INPUT_SELECT, 0x01); // DSPI1 : SDIN1
	snd_soc_write(codec, AK7759_17_DSP_IN2_INPUT_SELECT, 0x0E); // DSPI2 : ADCO
	snd_soc_write(codec, AK7759_1A_DAC_INPUT_SELECT, 0x08); // DAC : DSPO1
	snd_soc_write(codec, AK7759_10_SDOUT1A_OUTPUT_SELECT, 0x09); // SDOUT1 Source Selector : DSPO2

	snd_soc_write(codec, AK7759_VIRT_84_DSP1OUT1_MIX, 0x01); // DSP OUT1 Mixer DSP IN1 : on
	snd_soc_write(codec, AK7759_VIRT_85_DSP1OUT2_MIX, 0x02); // DSP OUT2 Mixer DSP IN2 : on
	snd_soc_write(codec, AK7759_VIRT_86_DSP1OUT3_MIX, 0x04); // DSP OUT3 Mixer DSP IN3 : on

	snd_soc_write(codec, AK7759_01_SYSCLOCK_SETTING2, 0x1c); // AIN Lch/Rch on


	return 0;
}



static int ak7759_parse_dt(struct ak7759_priv *ak7759)
{
	struct device *dev;
	struct device_node *np;

	if (ak7759->control_type == SND_SOC_SPI) {
		dev = &(ak7759->spi->dev);
	}
	else {
		dev = &(ak7759->i2c->dev);
	}

	np = dev->of_node;

	if (!np) {
		akdbgprt("\t[AK7759] %s np error!\n",__FUNCTION__);
		return -1;
	}

	ak7759->pdn_gpio = of_get_named_gpio(np, "ak7759,pdn-gpio", 0);
	akdbgprt("\t[AK7759] %s pdn-gpio=%d\n",__FUNCTION__, ak7759->pdn_gpio);
	if (ak7759->pdn_gpio < 0) {
		ak7759->pdn_gpio = -1;
		return -1;
	}

	if( !gpio_is_valid(ak7759->pdn_gpio) ) {
		printk(KERN_ERR "ak7759 pdn pin(%u) is invalid\n", ak7759->pdn_gpio);
		return -1;
	}

	return 0;
}


static int ak7759_probe(struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
#ifdef AK7759_DEBUG
	int i;
#endif

	akdbgprt("\t[AK7759] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7759->codec = codec;
	ret = ak7759_parse_dt(ak7759);
	if( ret < 0 )ak7759->pdn_gpio = -1;
	if( ak7759->pdn_gpio > 0){
		ret = gpio_request(ak7759->pdn_gpio, "ak7759 pdn");
		akdbgprt("\t[AK7759] %s : gpio_request ret = %d\n",__FUNCTION__, ret);
		gpio_direction_output(ak7759->pdn_gpio, 0);
	}

#ifdef INIT_REG_BASICRAM
	ak7759_init_reg_for_basicram(codec);
	
	ak7759->MIRNo = 0;
	ak7759->status = POWERDOWN;
	
	ak7759->DSPPramMode = 1;
	ak7759->DSPCramMode = 1;
	ak7759->DSPOfregMode = 1;
	
	ak7759->DSPon = 0;
#else
	ak7759_init_reg(codec);

	ak7759->MIRNo = 0;
	ak7759->status = POWERDOWN;
	
	ak7759->DSPPramMode = 0;
	ak7759->DSPCramMode = 0;
	ak7759->DSPOfregMode = 0;
	
	ak7759->DSPon = 0;
#endif

#ifdef AK7759_DEBUG
	ak7759->regvalue[0] = 0;
	ak7759->regvalue[1] = 0;
	ak7759->regvalue[2] = 0;

	for ( i = 0 ; i < 5 ; i++ ) {
		ak7759->cramvalue[i] = 0;
	}	
#endif

#ifdef AK7759_IO_CONTROL
	init_ak7759_pd(ak7759);
#endif


	return ret;
}

static int ak7759_remove(struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK7759] %s(%d)\n",__func__,__LINE__);

	ak7759_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if ( ak7759->pdn_gpio > 0 ) {
		gpio_set_value(ak7759->pdn_gpio, 0);
		msleep(1);
		gpio_free(ak7759->pdn_gpio);
	}

	return 0;
}

static int ak7759_suspend(struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);	

	ak7759_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if ( ak7759->pdn_gpio > 0 ) {
		gpio_set_value(ak7759->pdn_gpio, 0);
		msleep(1);
	}
	
	return 0;
}

static int ak7759_resume(struct snd_soc_codec *codec)
{
	struct ak7759_priv *ak7759 = snd_soc_codec_get_drvdata(codec);	
	int i;

	for ( i = 0 ; i < ARRAY_SIZE(ak7759_reg) ; i++ ) {
		regmap_write(ak7759->regmap, ak7759_reg[i].reg, ak7759_reg[i].def);
	}

#ifdef INIT_REG_BASICRAM
	ak7759_init_reg_for_basicram(codec);
#else
	ak7759_init_reg(codec);
#endif

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_ak7759 = {
	.probe = ak7759_probe,
	.remove = ak7759_remove,
	.suspend =	ak7759_suspend,
	.resume =	ak7759_resume,

	.read = ak7759_read_register,
	.write = ak7759_write_register,

	.idle_bias_off = true,
	.set_bias_level = ak7759_set_bias_level,

#ifdef KERNEL_4_9_XX
	.component_driver = {
#endif
	.controls = ak7759_snd_controls,
	.num_controls = ARRAY_SIZE(ak7759_snd_controls),
	.dapm_widgets = ak7759_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak7759_dapm_widgets),
	.dapm_routes = ak7759_intercon,
	.num_dapm_routes = ARRAY_SIZE(ak7759_intercon),
#ifdef KERNEL_4_9_XX
	},
#endif
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ak7759);

static const struct regmap_config ak7759_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	
	.max_register = AK7759_MAX_REGISTER,
	.volatile_reg = ak7759_volatile,
	.writeable_reg = ak7759_writeable,
	.readable_reg = ak7759_readable,
	
	.reg_defaults = ak7759_reg,
	.num_reg_defaults = ARRAY_SIZE(ak7759_reg),
	.cache_type = REGCACHE_RBTREE,
};

static struct of_device_id ak7759_dt_ids[] = {
	{ .compatible = "akm,ak7759"},
	{   }
};
MODULE_DEVICE_TABLE(of, ak7759_dt_ids);


#ifdef AK7759_I2C_IF

static int ak7759_i2c_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *id)
{
	struct ak7759_priv *ak7759;
	int ret=0;
	
	akdbgprt("\t[AK7759] %s(%d)\n",__func__,__LINE__);

	ak7759 = devm_kzalloc(&i2c->dev, sizeof(struct ak7759_priv), GFP_KERNEL);
	if (ak7759 == NULL) return -ENOMEM;

	ak7759->regmap = devm_regmap_init_i2c(i2c, &ak7759_regmap); 
	if (IS_ERR(ak7759->regmap)) {
		devm_kfree(&i2c->dev, ak7759);
		return PTR_ERR(ak7759->regmap);
	}

	i2c_set_clientdata(i2c, ak7759);

	ak7759->control_type = SND_SOC_I2C;
	ak7759->i2c = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak7759, &ak7759_dai[0], ARRAY_SIZE(ak7759_dai));
	if (ret < 0){
		devm_kfree(&i2c->dev, ak7759);
		akdbgprt("\t[AK7759 Error!] %s(%d)\n",__func__,__LINE__);
	}
	return ret;
}

static int ak7759_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id ak7759_i2c_id[] = {
	{ "ak7759", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak7759_i2c_id);

static struct i2c_driver ak7759_i2c_driver = {
	.driver = {
		.name = "ak7759",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak7759_dt_ids),
	},
	.probe = ak7759_i2c_probe,
	.remove = ak7759_i2c_remove,
	.id_table = ak7759_i2c_id,
};


#else

static int ak7759_spi_probe(struct spi_device *spi)
{
	struct ak7759_priv *ak7759;
	int ret;

	akdbgprt("\t[AK7759] %s(%d)\n",__func__,__LINE__);

	ak7759 = devm_kzalloc(&spi->dev, sizeof(struct ak7759_priv),
			      GFP_KERNEL);
	if (ak7759 == NULL)
		return -ENOMEM;

	ak7759->regmap = devm_regmap_init_spi(spi, &ak7759_regmap);
	if (IS_ERR(ak7759->regmap)) {
		ret = PTR_ERR(ak7759->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	spi_set_drvdata(spi, ak7759);

	ak7759->control_type = SND_SOC_SPI;
	ak7759->spi = spi;

	ret = snd_soc_register_codec(&spi->dev,
			&soc_codec_dev_ak7759, &ak7759_dai[0], ARRAY_SIZE(ak7759_dai));

	if (ret != 0) {
		dev_err(&spi->dev, "Failed to register CODEC: %d\n", ret);
		return ret;
	}

	return 0;

}

static int  ak7759_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}


static struct spi_driver ak7759_spi_driver = {
	.driver = {
		.name = "ak7759",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak7759_dt_ids),
	},
	.probe = ak7759_spi_probe,
	.remove = ak7759_spi_remove,
};
#endif

static int __init ak7759_modinit(void)
{
	int ret = 0;

	akdbgprt("\t[AK7759] %s(%d)\n", __FUNCTION__,__LINE__);

#ifdef AK7759_I2C_IF
	ret = i2c_add_driver(&ak7759_i2c_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register AK7759 I2C driver: %d\n", ret);

	}
#else
	ret = spi_register_driver(&ak7759_spi_driver);
	if ( ret != 0 ) {
		printk(KERN_ERR "Failed to register AK7759 SPI driver: %d\n",  ret);

	}
#endif

#ifdef AK7759_IO_CONTROL
	ret = misc_register(&ak7759_misc);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register AK7759 MISC driver: %d\n",  ret);
	}
#endif

	return ret;
}
module_init(ak7759_modinit);

static void __exit ak7759_exit(void)
{
#ifdef AK7759_I2C_IF
	i2c_del_driver(&ak7759_i2c_driver);
#else
	spi_unregister_driver(&ak7759_spi_driver);
#endif

#ifdef AK7759_IO_CONTROL
	misc_deregister(&ak7759_misc);
#endif

}
module_exit(ak7759_exit);

MODULE_DESCRIPTION("ak7759 codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
