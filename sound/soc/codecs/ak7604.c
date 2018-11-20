/*
 * ak7604.c  --  audio driver for AK7604
 *
 * Copyright (C) 2017 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      17/11/27        1.0
 *                      18/04/30        2.0 : Audio Effect Implemented
 *                      18/05/30        2.1 : Feature name modified(Line# 2155,2211,2267,2323) 
 *                                            Subwoofer sw struct modified(Line# 3686)
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
#if defined(CONFIG_SND_SPI)
#include <linux/spi/spi.h>
#endif//CONFIG_SND_SPI
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>

#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>

#include "ak7604.h"
#include "ak7604_dsp_code.h"

//#define AK7604_DEBUG			//used at debug mode

#ifdef AK7604_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

#define SND_SOC_SPI 1
#define SND_SOC_I2C 2

#define AK7604_CRAMRUN_CHECKCOUNT  10 // wait count for cram write
static int calc_CRC(int length, u8 *data ); // 20180222
static int ak7604_ram_download(struct snd_soc_codec *codec, const u8 *tx_ram, u64 num, u16 crc); // 20180222

/* AK7604 Codec Private Data */
struct ak7604_priv {
	int control_type;
	struct snd_soc_codec *codec;
#if defined(CONFIG_SND_SPI)
	struct spi_device *spi;
#endif//CONFIG_SND_SPI
	struct i2c_client *i2c;
	struct regmap *regmap;
	int fs;
	
	int pdn_gpio;
	int MIRNo;
	int status;
	
	int dresetn;
	int ckresetn; // 20180222

	int DSPPramMode;
	int DSPCramMode;
	int DSPOfregMode;

    int PLLInput;  // 0 : XTI,   1 : BICK1 ...3 : BICK3
	int XtiFs;     // 0 : 12.288MHz,  1: 18.432MHz

	int SDfs[NUM_SYNCDOMAIN];     // 0:8kHz, 1:12kHz, 2:16kHz, 3:24kHz, 4:32kHz, 5:48kHz, 6:96kHz, 7:192kHz
    int SDBick[NUM_SYNCDOMAIN];   // 0:64fs, 1:48fs, 2:32fs, 3:128fs, 4:256fs

	int Master[NUM_SYNCDOMAIN];  // 0 : Slave Mode, 1 : Master
	int SDCks[NUM_SYNCDOMAIN];   // 0 : Low,  1: PLLMCLK,  2:XTI

	int TDMSDINbit[NUM_SDIO];
	int TDMSDOUTbit[NUM_SDIO];
	int DIEDGEbit[NUM_SDIO];
	int DOEDGEbit[NUM_SDIO];
	int DISLbit[NUM_SDIO];
	int DOSLbit[NUM_SDIO];
	
	int cramaddr;
	int cramcount;
	unsigned char cramvalue[48];
#ifdef AK7604_AUDIO_EFFECT
	int AE_A_Lch_dVol;
	int AE_A_Rch_dVol;
	int AE_D_Lch_dVol;
	int AE_D_Rch_dVol;
	int AE_IN_dVol;

	int AE_INT1_Lch_dVol;
	int AE_INT1_Rch_dVol;
	int AE_INT2_Lch_dVol;
	int AE_INT2_Rch_dVol;
	int AE_FL_dVol;
	int AE_FR_dVol;
	int AE_RL_dVol;
	int AE_RR_dVol;
	int AE_SWL_dVol;
	int AE_SWR_dVol;

	int AE_IN_mute;
	int AE_INT1_mute;
	int AE_INT2_mute;
	int AE_FRONT_mute;
	int AE_REAR_mute;
	int AE_SW_mute;

	int AE_F1_DeEmphasis;
	int AE_F2_BaseMidTreble;
	int AE_F2_BaseMidTreble_Sel;
	int AE_F3_BEX;
	int AE_F3_BEX_sel;
	int AE_F4_Compressor;
	int AE_F5_Surround;
	int AE_F6_Loudness;
	int AE_F6_Loudness_Flt_Sel;

	int AE_EQ_Mode;

	int AE_Delay_Mode;

	int AE_Input_Sel;
	int AE_SubWoofer_Sw;
	int AE_Function_Sw;

	int AE_MixerFL1;
	int AE_MixerFL2;
	int AE_MixerFL3;
	int AE_MixerFR1;
	int AE_MixerFR2;
	int AE_MixerFR3;
	int AE_MixerRL1;
	int AE_MixerRL2;
	int AE_MixerRL3;
	int AE_MixerRR1;
	int AE_MixerRR2;
	int AE_MixerRR3;

	int AE_MICnMusic_Lch_Sel;
	int AE_MICnMusic_Rch_Sel;

	int AE_DZF_Enable_Sw;
#endif
	int muteon;
	int cramwrite;
	int dsprun;

};

struct _ak7604_pd_handler {
	int ref_count;
	struct mutex lock;
	struct ak7604_priv *data;
} ak7604_pd_handler = {
	.ref_count = -1,
	.data = NULL,
};

/* ak7604 register cache & default register settings */
static const struct reg_default ak7604_reg[] = {
  { 0x0000, 0x00 },   // AK7604_00_SYSTEMCLOCK_1
  { 0x0001, 0x00 },   // AK7604_01_SYSTEMCLOCK_2
  { 0x0002, 0x10 },   // AK7604_02_SYSTEMCLOCK_3
  { 0x0003, 0x04 },   // AK7604_03_RESERVED
  { 0x0004, 0x00 },   // AK7604_04_SYNCDOMAIN_MS
  { 0x0005, 0x08 },   // AK7604_05_SYNCDOMAIN1_SET1
  { 0x0006, 0x00 },   // AK7604_06_SYNCDOMAIN1_SET2
  { 0x0007, 0x08 },   // AK7604_07_SYNCDOMAIN2_SET1
  { 0x0008, 0x00 },   // AK7604_08_SYNCDOMAIN2_SET2
  { 0x0009, 0x08 },   // AK7604_09_SYNCDOMAIN3_SET1
  { 0x000A, 0x00 },   // AK7604_0A_SYNCDOMAIN3_SET2
  { 0x000B, 0x08 },   // AK7604_0B_SYNCDOMAIN4_SET1
  { 0x000C, 0x00 },   // AK7604_0C_SYNCDOMAIN4_SET2
  { 0x000D, 0x00 },   // AK7604_0D_SDIN1_2_SYNC
  { 0x000E, 0x00 },   // AK7604_0E_SDIN3_4_SYNC
  { 0x000F, 0x00 },   // AK7604_0F_SYNCDOMAIN_SEL1
  { 0x0010, 0x00 },   // AK7604_10_SYNCDOMAIN_SEL2
  { 0x0011, 0x00 },   // AK7604_11_SYNCDOMAIN_SEL3
  { 0x0012, 0x00 },   // AK7604_12_SYNCDOMAIN_SEL4
  { 0x0013, 0x00 },   // AK7604_13_SYNCDOMAIN_SEL5
  { 0x0014, 0x00 },   // AK7604_14_SYNCDOMAIN_SEL6
  { 0x0015, 0x00 },   // AK7604_15_SYNCDOMAIN_SEL7
  { 0x0016, 0x00 },   // AK7604_16_SYNCDOMAIN_SEL8
  { 0x0017, 0x00 },   // AK7604_17_SYNCDOMAIN_SEL9
  { 0x0018, 0x00 },   // AK7604_18_SYNCDOMAIN_SEL10
  { 0x0019, 0x00 },   // AK7604_19_SDOUT1A_SELECT
  { 0x001A, 0x00 },   // AK7604_1A_SDOUT1B_SELECT
  { 0x001B, 0x00 },   // AK7604_1B_SDOUT1C_SELECT
  { 0x001C, 0x00 },   // AK7604_1C_SDOUT1D_SELECT
  { 0x001D, 0x00 },   // AK7604_1D_SDOUT2_SELECT
  { 0x001E, 0x00 },   // AK7604_1E_SDOUT3_SELECT
  { 0x001F, 0x00 },   // AK7604_1F_DAC1_SELECT
  { 0x0020, 0x00 },   // AK7604_20_DAC2_SELECT
  { 0x0021, 0x00 },   // AK7604_21_DAC3_SELECT
  { 0x0022, 0x00 },   // AK7604_22_DSPDIN1_SELECT
  { 0x0023, 0x00 },   // AK7604_23_DSPDIN2_SELECT
  { 0x0024, 0x00 },   // AK7604_24_DSPDIN3_SELECT
  { 0x0025, 0x00 },   // AK7604_25_DSPDIN4_SELECT
  { 0x0026, 0x00 },   // AK7604_26_DSPDIN5_SELECT
  { 0x0027, 0x00 },   // AK7604_27_DSPDIN6_SELECT
  { 0x0028, 0x00 },   // AK7604_28_SRC1_SELECT
  { 0x0029, 0x00 },   // AK7604_29_SRC2_SELECT
  { 0x002A, 0x00 },   // AK7604_2A_SRC3_SELECT
  { 0x002B, 0x00 },   // AK7604_2B_SRC4_SELECT
  { 0x002C, 0x00 },   // AK7604_2C_CLOCKFORMAT1
  { 0x002D, 0x00 },   // AK7604_2D_CLOCKFORMAT2
  { 0x002E, 0x00 },   // AK7604_2E_SDIN1_FORMAT
  { 0x002F, 0x00 },   // AK7604_2F_SDIN2_FORMAT
  { 0x0030, 0x00 },   // AK7604_30_SDIN3_FORMAT
  { 0x0031, 0x00 },   // AK7604_31_SDIN4_FORMAT
  { 0x0032, 0x00 },   // AK7604_32_SDOUT1_FORMAT
  { 0x0033, 0x00 },   // AK7604_33_SDOUT2_FORMAT
  { 0x0034, 0x00 },   // AK7604_34_SDOUT3_FORMAT
  { 0x0035, 0x00 },   // 
  { 0x0036, 0x00 },   // 
  { 0x0037, 0x00 },   // 
  { 0x0038, 0x00 },   // 
  { 0x0039, 0x00 },   // 
  { 0x003A, 0x00 },   // 
  { 0x003B, 0x00 },   // 
  { 0x003C, 0x00 },   // 
  { 0x003D, 0x00 },   // 
  { 0x003E, 0x00 },   // 
  { 0x003F, 0x00 },   // 
  { 0x0040, 0x00 },   // 
  { 0x0041, 0x00 },   // 
  { 0x0042, 0x00 },   // 
  { 0x0043, 0x00 },   // 
  { 0x0044, 0x00 },   // 
  { 0x0045, 0x00 },   // 
  { 0x0046, 0x00 },   // 
  { 0x0047, 0x00 },   // 
  { 0x0048, 0x00 },   // 
  { 0x0049, 0x00 },   // 
  { 0x004A, 0x00 },   // 
  { 0x004B, 0x00 },   // 
  { 0x004C, 0x00 },   // 
  { 0x004D, 0x00 },   // 
  { 0x004E, 0x00 },   // 
  { 0x004F, 0x00 },   // 
  { 0x0050, 0x00 },   // AK7604_50_INPUT_DATA
  { 0x0051, 0x00 },   // AK7604_51_SELECT_DATA
  { 0x0052, 0x00 },   // 
  { 0x0053, 0x00 },   // AK7604_53_STO_SETTING
  { 0x0054, 0x00 },   // AK7604_60_DSP_SETTING1
  { 0x0055, 0x00 },   // AK7604_61_DSP_SETTING2
  { 0x0056, 0x00 },   // 
  { 0x0057, 0x00 },   // 
  { 0x0058, 0x00 },   // 
  { 0x0059, 0x00 },   // 
  { 0x005A, 0x00 },   // 
  { 0x005B, 0x00 },   // 
  { 0x005C, 0x00 },   // 
  { 0x005D, 0x00 },   // 
  { 0x005E, 0x00 },   // 
  { 0x005F, 0x00 },   // 
  { 0x0060, 0x00 },   // 
  { 0x0061, 0x00 },   // 
  { 0x0062, 0x00 },   // 
  { 0x0063, 0x00 },   // 
  { 0x0064, 0x00 },   // 
  { 0x0065, 0x00 },   // 
  { 0x0066, 0x00 },   // 
  { 0x0067, 0x00 },   // 
  { 0x0068, 0x00 },   // 
  { 0x0069, 0x00 },   // 
  { 0x006A, 0x00 },   // 
  { 0x006B, 0x00 },   // 
  { 0x006C, 0x00 },   // 
  { 0x006D, 0x00 },   // 
  { 0x006E, 0x00 },   // 
  { 0x006F, 0x00 },   // 
  { 0x0070, 0x00 },   // 
  { 0x0071, 0x00 },   // AK7604_71_SRCMUTE_SETTING
  { 0x0072, 0x00 },   // AK7604_72_SRCGROUTP_SETTING
  { 0x0073, 0x0F },   // AK7604_73_SRCFILTER_SETTING
  { 0x0074, 0x00 },   // 
  { 0x0075, 0x00 },   // 
  { 0x0076, 0x00 },   // 
  { 0x0077, 0x00 },   // 
  { 0x0078, 0x00 },   // 
  { 0x0079, 0x00 },   // 
  { 0x007A, 0x00 },   // 
  { 0x007B, 0x00 },   // 
  { 0x007C, 0x00 },   // 
  { 0x007D, 0x00 },   // 
  { 0x007E, 0x00 },   // 
  { 0x007F, 0x00 },   // 
  { 0x0080, 0x00 },   // 
  { 0x0081, 0x00 },   // AK7604_81_MIC_SETTING
  { 0x0082, 0x00 },   // AK7604_82_MIC_GAIN
  { 0x0083, 0x18 },   // AK7604_83_DAC1L_VOLUME
  { 0x0084, 0x18 },   // AK7604_84_DAC1R_VOLUME
  { 0x0085, 0x18 },   // AK7604_85_DAC2L_VOLUME
  { 0x0086, 0x18 },   // AK7604_86_DAC2R_VOLUME
  { 0x0087, 0x18 },   // AK7604_87_DAC3L_VOLUME
  { 0x0088, 0x18 },   // AK7604_88_DAC3R_VOLUME
  { 0x0089, 0x00 },   // AK7604_89_DAC_MUTEFILTER
  { 0x008A, 0x15 },   // AK7604_8A_DAC_DEEM_
  { 0x008B, 0x30 },   // AK7604_8B_ADCL_VOLUME
  { 0x008C, 0x30 },   // AK7604_8C_ADCR_VOLUME
  { 0x008D, 0x00 },   // AK7604_8D_AIN_SELECT
  { 0x008E, 0x00 },   // AK7604_8E_ADC_MUTEHPF
  { 0x008F, 0x00 },   // 
  { 0x0090, 0x00 },   // 
  { 0x0091, 0x00 },   // 
  { 0x0092, 0x00 },   // 
  { 0x0093, 0x00 },   // 
  { 0x0094, 0x00 },   // 
  { 0x0095, 0x00 },   // 
  { 0x0096, 0x00 },   // 
  { 0x0097, 0x00 },   // 
  { 0x0098, 0x00 },   // 
  { 0x0099, 0x00 },   // 
  { 0x009A, 0x00 },   // 
  { 0x009B, 0x00 },   // 
  { 0x009C, 0x00 },   // 
  { 0x009D, 0x00 },   // 
  { 0x009E, 0x00 },   // 
  { 0x009F, 0x00 },   // 
  { 0x00A0, 0x00 },   // 
  { 0x00A1, 0x00 },   // AK7604_A1_POWERMANAGEMENT1
  { 0x00A2, 0x00 },   // AK7604_A2_POWERMANAGEMENT2
  { 0x00A3, 0x00 },   // AK7604_A3_RESETCONTROL
  { 0x00A4, 0x00 },   // 
  { 0x00A5, 0x00 },   // 
  { 0x00A6, 0x00 },   // 
  { 0x00A7, 0x00 },   // 
  { 0x00A8, 0x00 },   // 
  { 0x00A9, 0x00 },   // 
  { 0x00AA, 0x00 },   // 
  { 0x00AB, 0x00 },   // 
  { 0x00AC, 0x00 },   // 
  { 0x00AD, 0x00 },   // 
  { 0x00AE, 0x00 },   // 
  { 0x00AF, 0x00 },   // 
  { 0x00B0, 0x00 },   // 
  { 0x00B1, 0x00 },   // 
  { 0x00B2, 0x00 },   // 
  { 0x00B3, 0x00 },   // 
  { 0x00B4, 0x00 },   // 
  { 0x00B5, 0x00 },   // 
  { 0x00B6, 0x00 },   // 
  { 0x00B7, 0x00 },   // 
  { 0x00B8, 0x00 },   // 
  { 0x00B9, 0x00 },   // 
  { 0x00BA, 0x00 },   // 
  { 0x00BB, 0x00 },   // 
  { 0x00BC, 0x00 },   // 
  { 0x00BD, 0x00 },   // 
  { 0x00BE, 0x00 },   // 
  { 0x00BF, 0x00 },   // 
  { 0x00C0, 0x04 },   // AK7604_C0_DEVICE_ID
  { 0x00C1, 0x00 },   // AK7604_C1_REVISION_NUM
  { 0x00C2, 0xA0 },   // AK7604_C2_DSPERROR_STATUS
  { 0x00C3, 0x00 },   // AK7604_C3_SRC_STATUS
  { 0x00C4, 0x00 },   // 
  { 0x00C5, 0x00 },   // 
  { 0x00C6, 0x80 },   // AK7604_C6_STO_READOUT
  { 0x00C7, 0x00 },   // AK7604_VIRT_C7_DSPOUT1_MIX
  { 0x00C8, 0x00 },   // AK7604_VIRT_C8_DSPOUT2_MIX
  { 0x00C9, 0x00 },   // AK7604_VIRT_C9_DSPOUT3_MIX
  { 0x00CA, 0x00 },   // AK7604_VIRT_CA_DSPOUT4_MIX
  { 0x00CB, 0x00 },   // AK7604_VIRT_CB_DSPOUT5_MIX
  { 0x00CC, 0x00 },   // AK7604_VIRT_CC_DSPOUT6_MIX
};


/* MIC Input Volume control:
 * from 0 to 36 dB (quantity of each step is various) */
static DECLARE_TLV_DB_MINMAX(mgn_tlv, 0, 3600);

/* ADC, ADC2 Digital Volume control:
 * from -103.5 to 24 dB in 0.5 dB steps (mute instead of -103.5 dB) */
static DECLARE_TLV_DB_SCALE(voladc_tlv, -10350, 50, 0);

/* DAC Digital Volume control:
 * from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB) */
static DECLARE_TLV_DB_SCALE(voldac_tlv, -11550, 50, 0);

static const char *atspad_texts[] = {
	"4/fs", "16/fs"
};

static const char *atspda_texts[] = {
	"4/fs", "16/fs"
};

static const char *dac_digfilsel_texts[] = {
	"Sharp",
	"Slow",
	"Short Delay Sharp",
	"Short Delay Slow",
};

static const char *dac_deemphasis_texts[] = {
	"44.1kHz", "OFF", "48kHz", "32kHz"
};


static const struct soc_enum ak7604_codec_enum[] = {
	SOC_ENUM_SINGLE(AK7604_8E_ADC_MUTEHPF, 7, ARRAY_SIZE(atspad_texts), atspad_texts),
	SOC_ENUM_SINGLE(AK7604_89_DAC_MUTEFILTER, 7, ARRAY_SIZE(atspda_texts), atspda_texts),
	SOC_ENUM_SINGLE(AK7604_89_DAC_MUTEFILTER, 0, ARRAY_SIZE(dac_digfilsel_texts), dac_digfilsel_texts),
	SOC_ENUM_SINGLE(AK7604_8A_DAC_DEEM_, 0, ARRAY_SIZE(dac_deemphasis_texts), dac_deemphasis_texts),//DAC1
	SOC_ENUM_SINGLE(AK7604_8A_DAC_DEEM_, 2, ARRAY_SIZE(dac_deemphasis_texts), dac_deemphasis_texts),//DAC2

	SOC_ENUM_SINGLE(AK7604_8A_DAC_DEEM_, 4, ARRAY_SIZE(dac_deemphasis_texts), dac_deemphasis_texts),//DAC3
};

static const char *sdselbit_texts[] = {
	"Low", "SYNC1", "SYNC2", "SYNC3", "SYNC4",
};

static const struct soc_enum ak7604_sdsel_enum[] = {
	SOC_ENUM_SINGLE(AK7604_11_SYNCDOMAIN_SEL3, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDOUT1
	SOC_ENUM_SINGLE(AK7604_11_SYNCDOMAIN_SEL3, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDOUT2
	SOC_ENUM_SINGLE(AK7604_12_SYNCDOMAIN_SEL4, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDOUT3
	SOC_ENUM_SINGLE(AK7604_12_SYNCDOMAIN_SEL4, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DAC
	SOC_ENUM_SINGLE(AK7604_13_SYNCDOMAIN_SEL5, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //ADC

	SOC_ENUM_SINGLE(AK7604_13_SYNCDOMAIN_SEL5, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSP
	SOC_ENUM_SINGLE(AK7604_14_SYNCDOMAIN_SEL6, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSPO1
	SOC_ENUM_SINGLE(AK7604_14_SYNCDOMAIN_SEL6, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSPO2
	SOC_ENUM_SINGLE(AK7604_15_SYNCDOMAIN_SEL7, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSPO3
	SOC_ENUM_SINGLE(AK7604_15_SYNCDOMAIN_SEL7, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSPO4

	SOC_ENUM_SINGLE(AK7604_16_SYNCDOMAIN_SEL8, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSPO5
	SOC_ENUM_SINGLE(AK7604_16_SYNCDOMAIN_SEL8, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //DSPO6
	SOC_ENUM_SINGLE(AK7604_17_SYNCDOMAIN_SEL9, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SRCO1
	SOC_ENUM_SINGLE(AK7604_17_SYNCDOMAIN_SEL9, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SRCO2
	SOC_ENUM_SINGLE(AK7604_18_SYNCDOMAIN_SEL10, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SRCO3

	SOC_ENUM_SINGLE(AK7604_18_SYNCDOMAIN_SEL10, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SRCO4
};

static const char *fsmodebit_texts[] = {
	"8kHz:8kHz", "12kHz:12kHz", "16kHz:16kHz", "24kHz:24kHz",
	"32kHz:32kHz", "32kHz:16kHz", "32kHz:8kHz", "48kHz:48kHz",
	"48kHz:24kHz", "48kHz:16kHz", "48kHz:8kHz", "96kHz:96kHz",
	"96kHz:48kHz", "96kHz:32kHz", "96kHz:24kHz", "96kHz:16kHz",
	"96kHz:8kHz"
};

static const struct soc_enum ak7604_fsmode_enum[] = {
	SOC_ENUM_SINGLE(AK7604_01_SYSTEMCLOCK_2, 0, ARRAY_SIZE(fsmodebit_texts), fsmodebit_texts),
};

// ak7604_set_dai_fmt() takes priority
// This is for SD that is not assigned to BICK/LRCK pin
static const char *msnbit_texts[] = {
	"Slave", "Master"
};

static const struct soc_enum ak7604_msnbit_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(msnbit_texts), msnbit_texts),	
};

static const struct soc_enum ak7604_portsdsel_enum[] = {
	SOC_ENUM_SINGLE(AK7604_0F_SYNCDOMAIN_SEL1, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBCK1[2:0]
	SOC_ENUM_SINGLE(AK7604_0F_SYNCDOMAIN_SEL1, 0, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBCK2[2:0]
	SOC_ENUM_SINGLE(AK7604_10_SYNCDOMAIN_SEL2, 4, ARRAY_SIZE(sdselbit_texts), sdselbit_texts), //SDBCK3[2:0]
};

static const char *sdcks_texts[] = {
	"Low", "PLLMCLK", "XTI", "BICK1", "BICK2", "BICK3"
};

static const struct soc_enum ak7604_sdcks_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sdcks_texts), sdcks_texts), 
};


static const char *sd_fs_texts[] = {
	"8kHz", "12kHz",  "16kHz", "24kHz", 
    "32kHz", "48kHz", "96kHz"
};

static int sdfstab[] = {
	8000, 12000, 16000, 24000, 
    32000, 48000, 96000
};

static const char *sd_bick_texts[] = {
	"64fs", "48fs", "32fs",  "128fs", "256fs"
};

static int sdbicktab[] = {
	64, 48, 32, 128, 256
};

static const struct soc_enum ak7604_sd_fs_enum[] = {
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
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int nPLLInFs;
	int value, nBfs, fs;
	int n, nMax;

	value = ( ak7604->PLLInput << 5 );
	snd_soc_update_bits(codec, AK7604_00_SYSTEMCLOCK_1, 0x60, value);

	if ( ak7604->PLLInput == 0 ) {
		nPLLInFs = XtiFsTab[ak7604->XtiFs];
	}
	else {
		nBfs = sdbicktab[ak7604->SDBick[ak7604->PLLInput-1]];
		fs = sdfstab[ak7604->SDfs[ak7604->PLLInput-1]];
		akdbgprt("[AK7604] %s nBfs=%d fs=%d\n",__FUNCTION__, nBfs, fs);
		nPLLInFs = nBfs * fs;
	}

	n = 0;
	nMax = sizeof(PLLInFsTab) / sizeof(PLLInFsTab[0]);

	do {
		akdbgprt("[AK7604] %s n=%d PLL:%d %d\n",__FUNCTION__, n, nPLLInFs, PLLInFsTab[n]);
		if ( nPLLInFs == PLLInFsTab[n] ) break;
		n++;
	} while ( n < nMax);


	if ( n == nMax ) {
		pr_err("%s: PLL1 setting Error!\n", __func__);
		return(-1);
	}

	snd_soc_update_bits(codec, AK7604_00_SYSTEMCLOCK_1, 0x1F, n);

	return(0);

}

static int setSDMaster(
struct snd_soc_codec *codec,
int nSDNo,
int nMaster)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec); 
	int addr;
	int value;

	if ( nSDNo >= NUM_SYNCDOMAIN ) return(0);

	ak7604->Master[nSDNo] = nMaster;

	addr = AK7604_05_SYNCDOMAIN1_SET1 + ( 2 * nSDNo );
	value =  ak7604->SDCks[nSDNo] << 3;
	snd_soc_update_bits(codec, addr, 0x38, value);

	addr = AK7604_04_SYNCDOMAIN_MS;
	value = nMaster << nSDNo;

	snd_soc_update_bits(codec, addr, 1<<nSDNo, value);

	return(0);

}

static int setSDClock(
struct snd_soc_codec *codec,
int nSDNo)
{

	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int addr;
	int fs, bickfs;
	int cksBickFs;
	int sdv, bdv;
	int csksd;

	if ( nSDNo >= NUM_SYNCDOMAIN ) return(0);

	fs = sdfstab[ak7604->SDfs[nSDNo]];
	bickfs = sdbicktab[ak7604->SDBick[nSDNo]] * fs;

	addr = AK7604_05_SYNCDOMAIN1_SET1 + ( 2 * nSDNo );

	switch(ak7604->SDCks[nSDNo]) {
	case 3:  // BICK1
	case 4:  // BICK2
	case 5:  // BICK3
		csksd = ak7604->SDCks[nSDNo] - 3;
		cksBickFs = sdbicktab[csksd] * 	sdfstab[ak7604->SDfs[csksd]];
		bdv = cksBickFs / bickfs;
		if ( ( cksBickFs % bickfs ) != 0 ) {
			pr_err("[ak7604]%s: BDV Error! SD No = %d,cks=%d, bick=%d\n", __func__, nSDNo, cksBickFs, bickfs);
		}
		break;
	case 2:  // XTI
		bdv = XtiFsTab[ak7604->XtiFs] / bickfs;
		break;
	default:
		bdv = 122880000 / bickfs;
		break;
	}

	sdv = ak7604->SDBick[nSDNo];
	akdbgprt("\t[AK7604]%s, BDV=%d, SDV=%d\n",__FUNCTION__, bdv, sdbicktab[sdv]);
	bdv--;
	if ( bdv > 255 ) {
		pr_err("%s: BDV Error! SD No = %d, bdv bit = %d\n", __func__, nSDNo, bdv);
		bdv = 255;
	}

	snd_soc_update_bits(codec, addr, 0x07, sdv);
	addr++;
	snd_soc_write(codec, addr, bdv);
	if ( ak7604->PLLInput == (nSDNo + 1) ) {
		setPLLOut(codec);
	}
	return(0);
}

static int get_sd1_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDCks[0];

    return 0;
}

static int set_sd1_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sdcks_texts)){
		if ( ak7604->SDCks[0] != currMode ) {
			ak7604->SDCks[0] = currMode;
			setSDClock(codec, 0);
			setSDMaster(codec, 0, ak7604->Master[0]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd2_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDCks[1];

    return 0;
}

static int set_sd2_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sdcks_texts)){
		if ( ak7604->SDCks[1] != currMode ) {
			ak7604->SDCks[1] = currMode;
			setSDClock(codec, 1);
			setSDMaster(codec, 1, ak7604->Master[1]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd3_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDCks[2];

    return 0;
}

static int set_sd3_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sdcks_texts)){
		if ( ak7604->SDCks[2] != currMode ) {
			ak7604->SDCks[2] = currMode;
			setSDClock(codec, 2);
			setSDMaster(codec, 2, ak7604->Master[2]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd4_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDCks[3];

    return 0;
}

static int set_sd4_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sdcks_texts)){
		if ( ak7604->SDCks[3] != currMode ) {
			ak7604->SDCks[3] = currMode;
			setSDClock(codec, 3);
			setSDMaster(codec, 3, ak7604->Master[3]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd1_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->Master[0];

    return 0;
}

static int set_sd1_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 0, currMode);

    return 0;
}


static int get_sd2_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->Master[1];

    return 0;
}

static int set_sd2_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 1, currMode);

    return 0;
}


static int get_sd3_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->Master[2];

    return 0;
}

static int set_sd3_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
    int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(codec, 2, currMode);

    return 0;
}

static int get_sd1_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDfs[0];

    return 0;
}

static int set_sd1_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_fs_texts)){
		if ( ak7604->SDfs[0] != currMode ) {
			ak7604->SDfs[0] = currMode;
			setSDClock(codec, 0);
			setSDMaster(codec, 0, ak7604->Master[0]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDfs[1];

    return 0;
}

static int set_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_fs_texts)){
		if ( ak7604->SDfs[1] != currMode ) {
			ak7604->SDfs[1] = currMode;
			setSDClock(codec, 1);
			setSDMaster(codec, 1, ak7604->Master[1]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd3_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDfs[2];

    return 0;
}

static int set_sd3_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_fs_texts)){
		if ( ak7604->SDfs[2] != currMode ) {
			ak7604->SDfs[2] = currMode;
			setSDClock(codec, 2);
			setSDMaster(codec, 2,  ak7604->Master[2]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd4_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDfs[3];

    return 0;
}


static int set_sd4_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_fs_texts)){
		if ( ak7604->SDfs[3] != currMode ) {
			ak7604->SDfs[3] = currMode;
			setSDClock(codec, 3);
			setSDMaster(codec, 3, ak7604->Master[3]);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;

}

static int get_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDBick[0];

    return 0;
}

static int set_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_bick_texts)){
		if ( ak7604->SDBick[0] != currMode ) {
			ak7604->SDBick[0] = currMode;
			setSDClock(codec, 0);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDBick[1];

    return 0;
}

static int set_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_bick_texts)){
	if ( ak7604->SDBick[1] != currMode ) {
		ak7604->SDBick[1] = currMode;
		setSDClock(codec, 1);
	}
	}
	else {
	akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd3_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDBick[2];

    return 0;
}

static int set_sd3_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_bick_texts)){
	if ( ak7604->SDBick[2] != currMode ) {
		ak7604->SDBick[2] = currMode;
		setSDClock(codec, 2);
	}
	}
	else {
	akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sd4_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->SDBick[3];

    return 0;
}

static int set_sd4_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(sd_bick_texts)){
	if ( ak7604->SDBick[3] != currMode ) {
		ak7604->SDBick[3] = currMode;
		setSDClock(codec, 3);
	}
	}
	else {
	akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}


static const char *pllinput_texts[]  = {
	"XTI", "BICK1",  "BICK2",  "BICK3",
};

static const char *xtifs_texts[]  = {
	"12.288MHz", "18.432MHz"
};

static const struct soc_enum ak7604_pllset_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(pllinput_texts), pllinput_texts), 
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(xtifs_texts), xtifs_texts), 
};

static int get_pllinput(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->PLLInput;

    return 0;
}

static int set_pllinput(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(pllinput_texts)){
		if ( ak7604->PLLInput != currMode ) {
			ak7604->PLLInput = currMode;
			setPLLOut(codec);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_xtifs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->XtiFs;

    return 0;
}

static int set_xtifs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(xtifs_texts)){
		if ( ak7604->XtiFs != currMode ) {
			ak7604->XtiFs = currMode;
			setPLLOut(codec);
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static const char *ak7604_tdm_texts[] = {
	"TDM Off", "TDM On"
};

static const struct soc_enum ak7604_tdm_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_tdm_texts), ak7604_tdm_texts), 
};

static int get_sdin1_tdm( struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->TDMSDINbit[0];

    return 0;
}

static int set_sdin1_tdm( struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak7604->TDMSDINbit[0] != currMode ) {
			ak7604->TDMSDINbit[0] = currMode;
			ak7604->DIEDGEbit[0] = ak7604->TDMSDINbit[0];
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdout1_tdm( struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->TDMSDOUTbit[0];

    return 0;
}

static int set_sdout1_tdm( struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 2){
		if ( ak7604->TDMSDOUTbit[0] != currMode ) {
			ak7604->TDMSDOUTbit[0] = currMode;
			ak7604->DOEDGEbit[0] = ak7604->TDMSDOUTbit[0];
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static const char *ak7604_sd_ioformat_texts[] = {
	"24bit",
	"20bit",
	"16bit",
	"32bit",
};

static const struct soc_enum ak7604_slotlen_enum[] = {
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_sd_ioformat_texts), ak7604_sd_ioformat_texts), 
};

static int get_sdin1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DISLbit[0];

    return 0;
}

static int set_sdin1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DISLbit[0] != currMode ) {
			ak7604->DISLbit[0] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdin2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DISLbit[1];

    return 0;
}

static int set_sdin2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DISLbit[1] != currMode ) {
			ak7604->DISLbit[1] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdin3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DISLbit[2];

    return 0;
}

static int set_sdin3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DISLbit[2] != currMode ) {
			ak7604->DISLbit[2] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdin4_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DISLbit[3];

    return 0;
}

static int set_sdin4_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DISLbit[3] != currMode ) {
			ak7604->DISLbit[3] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdout1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DOSLbit[0];

    return 0;
}

static int set_sdout1_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DOSLbit[0] != currMode ) {
			ak7604->DOSLbit[0] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdout2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DOSLbit[1];

    return 0;
}

static int set_sdout2_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DOSLbit[1] != currMode ) {
			ak7604->DOSLbit[1] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static int get_sdout3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    ucontrol->value.enumerated.item[0] = ak7604->DOSLbit[2];

    return 0;
}

static int set_sdout3_slot(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < ARRAY_SIZE(ak7604_sd_ioformat_texts)){
		if ( ak7604->DOSLbit[2] != currMode ) {
			ak7604->DOSLbit[2] = currMode;
		}
	}
	else {
		akdbgprt(" [AK7604] %s Invalid Value selected!\n",__FUNCTION__);
	}
    return 0;
}

static const char *sdout_modeset_texts[] = {
	"Slow", "Fast"
};

static const struct soc_enum ak7604_sdout_modeset_enum[] = {
	SOC_ENUM_SINGLE(AK7604_50_INPUT_DATA, 4, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7604_50_INPUT_DATA, 5, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
	SOC_ENUM_SINGLE(AK7604_50_INPUT_DATA, 6, ARRAY_SIZE(sdout_modeset_texts), sdout_modeset_texts),
};

static const char *do1sel_texts[] = {
	"L", "SDOUT1"
};
static const char *do2sel_texts[] = {
	"L", "L1", "L2", "L3", "DZF", "DZF Invert", "SDOUT2", "RDY"
};
static const char *do3sel_texts[] = {
	"STO", "SDOUT3", "GPO", "GPO Invert", "L"
};


static const struct soc_enum ak7604_outportsel_enum[] = {
	SOC_ENUM_SINGLE(AK7604_51_SELECT_DATA, 7, ARRAY_SIZE(do1sel_texts), do1sel_texts),
	SOC_ENUM_SINGLE(AK7604_51_SELECT_DATA, 4, ARRAY_SIZE(do2sel_texts), do2sel_texts),
	SOC_ENUM_SINGLE(AK7604_51_SELECT_DATA, 0, ARRAY_SIZE(do3sel_texts), do3sel_texts),
};

static const char *di3sel_texts[] = {
	"SDIN3", "JX3"
};

static const char *bck3sel_texts[] = {
	"BICK3", "JX1"
};

static const char *lck3sel_texts[] = {
	"LRCK3", "JX0", "N/A", "SDIN4"
};

static const struct soc_enum ak7604_inportsel_enum[] = {
	SOC_ENUM_SINGLE(AK7604_50_INPUT_DATA, 3, ARRAY_SIZE(di3sel_texts), di3sel_texts),
	SOC_ENUM_SINGLE(AK7604_50_INPUT_DATA, 2, ARRAY_SIZE(bck3sel_texts), bck3sel_texts),
	SOC_ENUM_SINGLE(AK7604_50_INPUT_DATA, 0, ARRAY_SIZE(lck3sel_texts), lck3sel_texts),
};

static const char *src_softmute_texts[] = {
	"Manual", "Semi Auto"
};

static const struct soc_enum ak7604_src_softmute_enum[] = {
	SOC_ENUM_SINGLE(AK7604_71_SRCMUTE_SETTING, 0, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7604_71_SRCMUTE_SETTING, 1, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7604_71_SRCMUTE_SETTING, 2, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
	SOC_ENUM_SINGLE(AK7604_71_SRCMUTE_SETTING, 3, ARRAY_SIZE(src_softmute_texts), src_softmute_texts),
};

static const char *src_softdfil_texts[] = {
	"Voice", "Audio"
};

static const struct soc_enum ak7604_src_dfil_enum[] = {
	SOC_ENUM_SINGLE(AK7604_73_SRCFILTER_SETTING, 0, ARRAY_SIZE(src_softdfil_texts), src_softdfil_texts),
	SOC_ENUM_SINGLE(AK7604_73_SRCFILTER_SETTING, 1, ARRAY_SIZE(src_softdfil_texts), src_softdfil_texts),
	SOC_ENUM_SINGLE(AK7604_73_SRCFILTER_SETTING, 2, ARRAY_SIZE(src_softdfil_texts), src_softdfil_texts),
	SOC_ENUM_SINGLE(AK7604_73_SRCFILTER_SETTING, 3, ARRAY_SIZE(src_softdfil_texts), src_softdfil_texts),
};


static const char *dsp_drmbk_texts[] = { // BANK1/BANK0
	"0/6144", "1024/5120", "2048/4096", "3072/3072"
};

static const char *dsp_assign_dram_texts[] = { // BANK1/BANK0
	"Ring/Ring", "Ring/Linear", "Linear/Ring", "Linear/Linear"
};

static const struct soc_enum ak7604_dsp_dram_enum[] = {
	SOC_ENUM_SINGLE(AK7604_60_DSP_SETTING1, 0, ARRAY_SIZE(dsp_drmbk_texts), dsp_drmbk_texts),
	SOC_ENUM_SINGLE(AK7604_60_DSP_SETTING1, 2, ARRAY_SIZE(dsp_assign_dram_texts), dsp_assign_dram_texts),
};

static const struct soc_enum ak7604_firmware_enum[] = 
{
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_firmware_pram), ak7604_firmware_pram),
    SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_firmware_cram), ak7604_firmware_cram),
};
#ifdef AK7604_AUDIO_EFFECT
static const struct soc_enum ak7604_dsp_data_enum[] = 
{
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_equalizer_texts), ak7604_equalizer_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_input_sw_texts), ak7604_input_sw_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_dspSWout_SW_texts), ak7604_dspSWout_SW_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7758_cram_dVol), ak7758_cram_dVol),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_loudness_pattern_texts), ak7604_loudness_pattern_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_dsp_delay_texts), ak7604_dsp_delay_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_dsp_outLsel_texts), ak7604_dsp_outLsel_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_dsp_out_Rsel_texts), ak7604_dsp_out_Rsel_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_dsp_DZF_texts), ak7604_dsp_DZF_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_basemidtreble_sel_texts), ak7604_basemidtreble_sel_texts), 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7604_bex_sel_texts), ak7604_bex_sel_texts), 
};
#endif

static int ak7604_firmware_write_ram(struct snd_soc_codec *codec, u16 mode, u16 cmd);
static int ak7604_set_status(struct snd_soc_codec *codec, enum ak7604_status status);
static int ak7604_write_cram(struct snd_soc_codec *codec, int addr, int len, unsigned char *cram_data);

static int get_DSP_write_pram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
  
   /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7604->DSPPramMode;	

    return 0;

}

static int set_DSP_write_pram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	akdbgprt("\t%s PRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7604_firmware_write_ram(codec, RAMTYPE_PRAM, currMode); 
	if ( ret != 0 ) return(-1);

	ak7604->DSPPramMode = currMode;
	
	return(0);
}

static int get_DSP_write_cram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

    /* Get the current output routing */
    ucontrol->value.enumerated.item[0] = ak7604->DSPCramMode;	

    return 0;

}

static int set_DSP_write_cram(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    int    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	akdbgprt("\t%s CRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7604_firmware_write_ram(codec, RAMTYPE_CRAM, currMode); 
	if ( ret != 0 ) return(-1);

	ak7604->DSPCramMode =currMode;
	
	return(0);
}

static int ak7604_reads(struct snd_soc_codec *codec, u8 *, size_t, u8 *, size_t);

#ifdef AK7604_DEBUG
static bool ak7604_readable(struct device *dev, unsigned int reg);

static int test_read_ram(struct snd_soc_codec *codec, int mode)
{
	u8	tx[3], rx[512];
	int i, n, plen, clen;

	akdbgprt("*****[AK7604] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7604_set_status(codec, DOWNLOAD);
	mdelay(1);
	if ( mode == 0 ) {
		plen = 16;
		for ( n = 0 ; n < (5 * plen) ; n++ ) rx[n] = 0;
		tx[0] = (COMMAND_WRITE_PRAM & 0x7F) + mode;
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7604_reads(codec, tx, 3, rx, 5 * plen);
		printk("***** AK7604 PRAM LEN = %d *******\n", plen);
		n = 0;
		for ( i = 0 ; i < plen ; i ++ ) {
			printk("AK7604 PAddr=%03X : %02X%02X%02X%02X%02X\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2], (int)rx[n+3], (int)rx[n+4]);
			n += 5;
		}
	}
	else {
		clen = 16;
		for ( n = 0 ; n < (3 * clen) ; n++ ) rx[n] = 0;
		tx[0] =  (COMMAND_WRITE_CRAM & 0x7F);
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7604_reads(codec, tx, 3, rx, 3 * clen);
		printk("*****AK7604 CRAM CMD=%d,  LEN = %d*******\n", (int)tx[0], clen);
		n = 0;
		for ( i = 0 ; i < clen ; i ++ ) {
			printk("AK7604 CAddr=%03X : %02X%02X%02X\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2]);	
			n += 3;
		}
	}
	mdelay(1);
	ak7604_set_status(codec, DOWNLOAD_FINISH);

	return(0);

}

static const char *test_reg_select[]   = 
{
    "read AK7604 Reg 00:34",
    "read AK7604 Reg 50:A3",
    "read AK7604 Reg C0:C6",
	"read DSP PRAM",
	"read DSP CRAM",
};

static const struct soc_enum ak7604_test_enum[] = 
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

	if ( currMode < 3 ) {
		switch(currMode) {
			case 0:
				regs = 0x00;
				rege = 0x34;
				break;
			case 1:
				regs = 0x50;
				rege = 0xA3;
				break;
			case 2:
			default:
				regs = 0xC0;
				rege = 0xC6;
				break;
		}
		for ( i = regs ; i <= rege ; i++ ){
			if ( ak7604_readable(NULL, i) ) {
				value = snd_soc_read(codec, i);
				printk("***AK7604 Addr,Reg=(%x, %x)\n", i, value);
			}
		}
	}
	else if ( currMode < 5 ) {
		test_read_ram(codec, (currMode - 3));
	}

	return 0;

}
#endif

#ifdef AK7604_AUDIO_EFFECT
static int get_dsp_input_mode(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_Input_Sel;	
    return 0;

}

static int set_dsp_input_mode(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dspDataInputSW)/sizeof(dspDataInputSW[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_Input_Sel = currMode;

	wCRC = calc_CRC(dspDataInputSW[currMode].len, dspDataInputSW[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dspDataInputSW[currMode].dspdata, dspDataInputSW[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_IN_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_IN_mute;	
    return 0;

}

static int set_IN_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_IN_MUTE_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_IN_mute = currMode;

	if(currMode == 0){
		cram_data[0] = 0x7F;cram_data[1] = 0xFF;cram_data[2] = 0xFF;
	}else{
		cram_data[0] = 0x0;cram_data[1] = 0x0;cram_data[2] = 0x0;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_INT1_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_INT1_mute;	
    return 0;

}

static int set_INT1_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_INT1_MUTE_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_INT1_mute = currMode;

	if(currMode == 0){
		cram_data[0] = 0x7F;cram_data[1] = 0xFF;cram_data[2] = 0xFF;
	}else{
		cram_data[0] = 0x0;cram_data[1] = 0x0;cram_data[2] = 0x0;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_INT2_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_INT2_mute;	
    return 0;

}

static int set_INT2_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_INT2_MUTE_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_INT2_mute = currMode;

	if(currMode == 0){
		cram_data[0] = 0x7F;cram_data[1] = 0xFF;cram_data[2] = 0xFF;
	}else{
		cram_data[0] = 0x0;cram_data[1] = 0x0;cram_data[2] = 0x0;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_FRONT_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_FRONT_mute;	
    return 0;

}

static int set_FRONT_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_FRONT_MUTE_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_FRONT_mute = currMode;

	if(currMode == 0){
		cram_data[0] = 0x7F;cram_data[1] = 0xFF;cram_data[2] = 0xFF;
	}else{
		cram_data[0] = 0x0;cram_data[1] = 0x0;cram_data[2] = 0x0;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_REAR_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_REAR_mute;	
    return 0;

}

static int set_REAR_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_REAR_MUTE_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_REAR_mute = currMode;

	if(currMode == 0){
		cram_data[0] = 0x7F;cram_data[1] = 0xFF;cram_data[2] = 0xFF;
	}else{
		cram_data[0] = 0x0;cram_data[1] = 0x0;cram_data[2] = 0x0;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);
}

static int get_SW_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_SW_mute;	
    return 0;

}

static int set_SW_mute(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_SW_MUTE_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_SW_mute = currMode;

	if(currMode == 0){
		cram_data[0] = 0x7F;cram_data[1] = 0xFF;cram_data[2] = 0xFF;
	}else{
		cram_data[0] = 0x0;cram_data[1] = 0x0;cram_data[2] = 0x0;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_ANA_L_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_A_Lch_dVol;	
    return 0;

}

static int set_ANA_L_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_ANA_L_GAIN_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_A_Lch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_ANA_R_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_A_Rch_dVol;	
    return 0;

}

static int set_ANA_R_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_ANA_R_GAIN_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_A_Rch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_DIG_L_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_D_Lch_dVol;	
    return 0;

}

static int set_DIG_L_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_DIG_L_GAIN_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_D_Lch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_DIG_R_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_D_Rch_dVol;	
    return 0;

}

static int set_DIG_R_Gain(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_DIG_R_GAIN_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_D_Rch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}


static int get_IN_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_IN_dVol;	
    return 0;

}

static int set_IN_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_IN_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_IN_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_INT1L_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_INT1_Lch_dVol;	
    return 0;

}

static int set_INT1L_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_INT1L_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_INT1_Lch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_INT1R_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_INT1_Rch_dVol;	
    return 0;

}

static int set_INT1R_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_INT1R_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_INT1_Rch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}


static int get_INT2L_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_INT2_Lch_dVol;	
    return 0;

}

static int set_INT2L_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_INT2L_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_INT2_Lch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}


static int get_INT2R_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_INT2_Rch_dVol;	
    return 0;

}

static int set_INT2R_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_INT2R_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_INT2_Rch_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_FL_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_FL_dVol;	
    return 0;

}

static int set_FL_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];
	
	ak7604_wcram.addr = AK7604_FL_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_FL_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_FR_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_FR_dVol;	
    return 0;

}

static int set_FR_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_FR_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_FR_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_RL_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_RL_dVol;	
    return 0;

}

static int set_RL_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_RL_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_RL_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_RR_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_RR_dVol;	
    return 0;

}

static int set_RR_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_RR_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_RR_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_SWL_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_SWL_dVol;	
    return 0;

}

static int set_SWL_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_SWL_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_SWL_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_SWR_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_SWR_dVol;	
    return 0;

}

static int set_SWR_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_SWR_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_SWR_dVol = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

#if defined(AK7604_AUDIO_EFFECT) && defined(AK7604_DEBUG)
static unsigned long ak7604_readMIR(struct snd_soc_codec *codec,int nMIRNo,unsigned long *dwMIRData);
static int get_read_spectrum(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	// struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	// struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = 0;	
    return 0;

}

static int set_read_spectrum(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	// struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	unsigned long MIRData;

	int    ret, n;
    // u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_SPECTRUM_READ_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;


// -------------------------Band 1------------------------------------------------------------
	cram_data[0] = 0x02;cram_data[1] = 0x4C;cram_data[2] = 0x00;
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}
	msleep(10);
	ak7604_readMIR(codec,0,&MIRData);
	printk("01: %x\n",(unsigned int)MIRData);
	ak7604_readMIR(codec,1,&MIRData);
	printk("02: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,2,&MIRData);
	printk("03: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,3,&MIRData);
	printk("04: %x\n", (unsigned int)MIRData);

// -----------------------------------------------------------------------------------------------------
	cram_data[0] = 0x02;cram_data[1] = 0x6C;cram_data[2] = 0x00;
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}
	msleep(10);
	ak7604_readMIR(codec,0,&MIRData);
	printk("05: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,1,&MIRData);
	printk("06: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,2,&MIRData);
	printk("07: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,3,&MIRData);
	printk("08: %x\n", (unsigned int)MIRData);
// -----------------------------------------------------------------------------------------------------
	cram_data[0] = 0x02;cram_data[1] = 0x8C;cram_data[2] = 0x00;
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}
	msleep(10);
	ak7604_readMIR(codec,0,&MIRData);
	printk("09: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,1,&MIRData);
	printk("10: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,2,&MIRData);
	printk("11: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,3,&MIRData);
	printk("12: %x\n", (unsigned int)MIRData);
// -----------------------------------------------------------------------------------------------------
	cram_data[0] = 0x02;cram_data[1] = 0xAC;cram_data[2] = 0x00;
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}
	msleep(10);
	ak7604_readMIR(codec,0,&MIRData);
	printk("13: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,1,&MIRData);
	printk("14: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,2,&MIRData);
	printk("15: %x\n", (unsigned int)MIRData);
	ak7604_readMIR(codec,3,&MIRData);
	printk("16: %x\n", (unsigned int)MIRData);

	return 0;
}
#endif

static int get_DeEmphasis(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F1_DeEmphasis;	
    return 0;

}

static int set_DeEmphasis(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_DEEMPHASIS_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F1_DeEmphasis = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_BaseMidTreble(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F2_BaseMidTreble;	
    return 0;

}

static int set_BaseMidTreble(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_BASSMIDTREBLE_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F2_BaseMidTreble = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_BaseMidTreble_sel(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F2_BaseMidTreble_Sel;	
    return 0;

}

static int set_BaseMidTreble_sel(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dspDatabasemidtreble)/sizeof(dspDatabasemidtreble[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_F2_BaseMidTreble_Sel = currMode;

	wCRC = calc_CRC(dspDatabasemidtreble[currMode].len, dspDatabasemidtreble[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dspDatabasemidtreble[currMode].dspdata, dspDatabasemidtreble[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);


}

static int get_BEX(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F3_BEX;	
    return 0;

}

static int set_BEX(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_BEX_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F3_BEX = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}


static int get_BEX_sel(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F3_BEX_sel;	
    return 0;

}

static int set_BEX_sel(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dspDataBEX)/sizeof(dspDataBEX[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_EQ_Mode = currMode;

	wCRC = calc_CRC(dspDataBEX[currMode].len, dspDataBEX[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dspDataBEX[currMode].dspdata, dspDataBEX[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);


}


static int get_Compressor(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F4_Compressor;	
    return 0;

}

static int set_Compressor(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_COMPRESSOR_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F4_Compressor = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_Surround(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F5_Surround;	
    return 0;

}

static int set_Surround(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_SURROUND_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F5_Surround = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_Loudness(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F6_Loudness;	
    return 0;

}

static int set_Loudness(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_LOUDNESS_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F6_Loudness = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_Loudness_Flt_sel(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_F6_Loudness_Flt_Sel;	
    return 0;

}

static int set_Loudness_Flt_sel(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	int    ret, n, pattern_no;
	unsigned char cram_data[3];
    u32    currMode = ucontrol->value.enumerated.item[0];

	if ( ( currMode < 0 )||( currMode >= (sizeof(ak7604_loudness_patterns)/sizeof(ak7604_loudness_patterns[0]))/3)){
		printk("%s: currMode = %d \n", __func__, currMode); 
           return(-EINVAL);
	}

	ak7604_wcram.addr = AK7604_LOUDNESS_FLT_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_F6_Loudness_Flt_Sel = currMode;
	
	pattern_no = currMode * 3;

	cram_data[0] = ak7604_loudness_patterns[pattern_no];
	cram_data[1] = ak7604_loudness_patterns[++pattern_no];
	cram_data[2] = ak7604_loudness_patterns[++pattern_no];

	printk("%s: cram_data[0] = %x,cram_data[1] = %x,cram_data[2] = %x,\n", __func__, cram_data[0],cram_data[1],cram_data[2]); 

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_equalizer(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_EQ_Mode;	
    return 0;

}

static int set_dsp_equalizer(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dspDataEqualizer)/sizeof(dspDataEqualizer[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_EQ_Mode = currMode;

	wCRC = calc_CRC(dspDataEqualizer[currMode].len, dspDataEqualizer[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dspDataEqualizer[currMode].dspdata, dspDataEqualizer[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_delay(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_Delay_Mode;	
    return 0;

}

static int set_dsp_delay(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dspdelayfunc)/sizeof(dspdelayfunc[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_Delay_Mode = currMode;

	wCRC = calc_CRC(dspdelayfunc[currMode].len, dspdelayfunc[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dspdelayfunc[currMode].dspdata, dspdelayfunc[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_Subwoofer_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_SubWoofer_Sw;	
    return 0;

}

static int set_dsp_Subwoofer_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dspSubwooferSW)/sizeof(dspSubwooferSW[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_SubWoofer_Sw = currMode;

	wCRC = calc_CRC(dspSubwooferSW[currMode].len, dspSubwooferSW[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dspSubwooferSW[currMode].dspdata, dspSubwooferSW[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_Function_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_Function_Sw;	
    return 0;

}

static int set_dsp_Function_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_FUNCTION_SW_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_Function_Sw = currMode;

	if(currMode == 0){
		cram_data[0] = 0x00;cram_data[1] = 0x00;cram_data[2] = 0x00;
	}else{
		cram_data[0] = 0x00;cram_data[1] = 0x01;cram_data[2] = 0x00;
	}

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);
}

static int get_MixerFL1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerFL1;	
    return 0;

}

static int set_MixerFL1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_FL1_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerFL1 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerFL2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerFL2;	
    return 0;

}

static int set_MixerFL2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_FL2_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerFL2 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerFL3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerFL3;	
    return 0;

}

static int set_MixerFL3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_FL3_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerFL3 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerFR1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerFR1;	
    return 0;

}

static int set_MixerFR1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_FR1_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerFR1 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerFR2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerFR2;	
    return 0;

}

static int set_MixerFR2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_FR2_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerFR2 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerFR3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerFR3;	
    return 0;

}

static int set_MixerFR3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_FR3_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerFR3 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerRL1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerRL1;	
    return 0;

}

static int set_MixerRL1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_RL1_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerRL1 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerRL2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerRL2;	
    return 0;

}

static int set_MixerRL2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_RL2_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerRL2 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerRL3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerRL3;	
    return 0;

}

static int set_MixerRL3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_RL3_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerRL3 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerRR1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerRR1;	
    return 0;

}

static int set_MixerRR1_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_RR1_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerRR1 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerRR2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerRR2;	
    return 0;

}

static int set_MixerRR2_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_RR2_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerRR2 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_MixerRR3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MixerRR3;	
    return 0;

}

static int set_MixerRR3_dVol(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	struct ak7604_wcram_handle  ak7604_wcram;
	unsigned char cram_data[3];
	int    ret, n;
    u32    currMode = ucontrol->value.enumerated.item[0];

	ak7604_wcram.addr = AK7604_MIXER_RR3_VOL_ADDR;
	ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	ak7604_wcram.cram = cram_data;

	ak7604->AE_MixerRR3 = currMode;

	if (currMode >= ARRAY_SIZE(ak7758_cram_dVol))
	{
		pr_info(" [AK7758] %s Invalid Value selected!\n",__FUNCTION__);
		return (-1);
	}

	//data
	cram_data[0] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 16));
	cram_data[1] = (unsigned char)(0xFF & (ak7758_cram_dVol_val[currMode] >> 8));
	cram_data[2] = (unsigned char)(0xFF & ak7758_cram_dVol_val[currMode]);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_write_cram(codec, ak7604_wcram.addr, ak7604_wcram.len, ak7604_wcram.cram);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: Writing Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_outLsel_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MICnMusic_Lch_Sel;	
    return 0;

}

static int set_dsp_outLsel_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dsp_outL_sel)/sizeof(dsp_outL_sel[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_MICnMusic_Lch_Sel = currMode;

	wCRC = calc_CRC(dsp_outL_sel[currMode].len, dsp_outL_sel[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dsp_outL_sel[currMode].dspdata, dsp_outL_sel[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_outRsel_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_MICnMusic_Rch_Sel;	
    return 0;

}

static int set_dsp_outRsel_SW(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dsp_outR_sel)/sizeof(dsp_outR_sel[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_MICnMusic_Rch_Sel = currMode;

	wCRC = calc_CRC(dsp_outR_sel[currMode].len, dsp_outR_sel[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dsp_outR_sel[currMode].dspdata, dsp_outR_sel[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}

static int get_dsp_DZF_Setting(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7604->AE_DZF_Enable_Sw;	
    return 0;

}

static int set_dsp_DZF_Setting(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
    struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
    u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int n;
	int	wCRC;

	if ( ( currMode < 0 ) || 
			( currMode >= (sizeof(dsp_DZF_sel)/sizeof(dsp_DZF_sel[0])))) {
           return(-EINVAL);
	}

	ak7604->AE_DZF_Enable_Sw = currMode;

	wCRC = calc_CRC(dsp_DZF_sel[currMode].len, dsp_DZF_sel[currMode].dspdata);
	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, dsp_DZF_sel[currMode].dspdata, dsp_DZF_sel[currMode].len, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! Ret = %d \n", __func__, ret); 
		return(-1);
	}

	return(ret);

}
#endif // AK7604_AUDIO_EFFECT

static const struct snd_kcontrol_new ak7604_snd_controls[] = {
	SOC_SINGLE_TLV("MIC Input Volume L", AK7604_82_MIC_GAIN, 4, 0x0F, 0, mgn_tlv),
	SOC_SINGLE_TLV("MIC Input Volume R", AK7604_82_MIC_GAIN, 0, 0x0F, 0, mgn_tlv),

	SOC_SINGLE_TLV("ADC Digital Volume L", AK7604_8B_ADCL_VOLUME, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("ADC Digital Volume R", AK7604_8C_ADCR_VOLUME, 0, 0xFF, 1, voladc_tlv),

	SOC_SINGLE_TLV("DAC1 Digital Volume L", AK7604_83_DAC1L_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC1 Digital Volume R", AK7604_84_DAC1R_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC2 Digital Volume L", AK7604_85_DAC2L_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC2 Digital Volume R", AK7604_86_DAC2R_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC3 Digital Volume L", AK7604_87_DAC3L_VOLUME, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("DAC3 Digital Volume R", AK7604_88_DAC3R_VOLUME, 0, 0xFF, 1, voldac_tlv),

	SOC_SINGLE("ADC Mute", AK7604_8E_ADC_MUTEHPF, 6, 1, 0),
	SOC_ENUM("ADC Volume Transition Time", ak7604_codec_enum[0]),

	SOC_SINGLE("DAC1 Mute", AK7604_89_DAC_MUTEFILTER, 6, 1, 0), 
	SOC_SINGLE("DAC2 Mute", AK7604_89_DAC_MUTEFILTER, 5, 1, 0), 
	SOC_SINGLE("DAC3 Mute", AK7604_89_DAC_MUTEFILTER, 4, 1, 0), 
	SOC_ENUM("DAC Volume Transition Time", ak7604_codec_enum[1]),
	SOC_ENUM("DAC Digital Filter",ak7604_codec_enum[2]),

	SOC_ENUM("DAC1 De-Emphasis Filter", ak7604_codec_enum[3]),
	SOC_ENUM("DAC2 De-Emphasis Filter", ak7604_codec_enum[4]),
	SOC_ENUM("DAC3 De-Emphasis Filter", ak7604_codec_enum[5]),

	SOC_ENUM("SDOUT1 Sync Domain", ak7604_sdsel_enum[0]), 
	SOC_ENUM("SDOUT2 Sync Domain", ak7604_sdsel_enum[1]), 
	SOC_ENUM("SDOUT3 Sync Domain", ak7604_sdsel_enum[2]), 
	SOC_ENUM("DAC Sync Domain", ak7604_sdsel_enum[3]), 
	SOC_ENUM("ADC Sync Domain", ak7604_sdsel_enum[4]), 

	SOC_ENUM("DSP Sync Domain", ak7604_sdsel_enum[5]), 
	SOC_ENUM("DSPOUT1 Sync Domain", ak7604_sdsel_enum[6]), 
	SOC_ENUM("DSPOUT2 Sync Domain", ak7604_sdsel_enum[7]), 
	SOC_ENUM("DSPOUT3 Sync Domain", ak7604_sdsel_enum[8]), 
	SOC_ENUM("DSPOUT4 Sync Domain", ak7604_sdsel_enum[9]), 
	SOC_ENUM("DSPOUT5 Sync Domain", ak7604_sdsel_enum[10]), 
	SOC_ENUM("DSPOUT6 Sync Domain", ak7604_sdsel_enum[11]), 

	SOC_ENUM("SRC1OUT Sync Domain", ak7604_sdsel_enum[12]), 
	SOC_ENUM("SRC2OUT Sync Domain", ak7604_sdsel_enum[13]),
	SOC_ENUM("SRC3OUT Sync Domain", ak7604_sdsel_enum[14]),
	SOC_ENUM("SRC4OUT Sync Domain", ak7604_sdsel_enum[15]),

	SOC_ENUM("CODEC Sampling Frequency", ak7604_fsmode_enum[0]), 
	SOC_ENUM_EXT("LRCK1/BICK1 Master", ak7604_msnbit_enum[0], get_sd1_ms, set_sd1_ms), 
	SOC_ENUM_EXT("LRCK2/BICK2 Master", ak7604_msnbit_enum[0], get_sd2_ms, set_sd2_ms), 
	SOC_ENUM_EXT("LRCK3/BICK3 Master", ak7604_msnbit_enum[0], get_sd3_ms, set_sd3_ms), 

	SOC_ENUM("LRCK1/BICK1 Sync Domain", ak7604_portsdsel_enum[0]), 
	SOC_ENUM("LRCK2/BICK2 Sync Domain", ak7604_portsdsel_enum[1]), 
	SOC_ENUM("LRCK3/BICK3 Sync Domain", ak7604_portsdsel_enum[2]), 

	SOC_ENUM_EXT("Sync Domain 1 Clock Source", ak7604_sdcks_enum[0], get_sd1_cks, set_sd1_cks),
	SOC_ENUM_EXT("Sync Domain 2 Clock Source", ak7604_sdcks_enum[0], get_sd2_cks, set_sd2_cks),
	SOC_ENUM_EXT("Sync Domain 3 Clock Source", ak7604_sdcks_enum[0], get_sd3_cks, set_sd3_cks),
	SOC_ENUM_EXT("Sync Domain 4 Clock Source", ak7604_sdcks_enum[0], get_sd4_cks, set_sd4_cks),

	SOC_ENUM_EXT("Sync Domain 1 fs", ak7604_sd_fs_enum[0], get_sd1_fs, set_sd1_fs),
	SOC_ENUM_EXT("Sync Domain 2 fs", ak7604_sd_fs_enum[0], get_sd2_fs, set_sd2_fs),
	SOC_ENUM_EXT("Sync Domain 3 fs", ak7604_sd_fs_enum[0], get_sd3_fs, set_sd3_fs),
	SOC_ENUM_EXT("Sync Domain 4 fs", ak7604_sd_fs_enum[0], get_sd4_fs, set_sd4_fs),

	SOC_ENUM_EXT("Sync Domain 1 BICK", ak7604_sd_fs_enum[1], get_sd1_bick, set_sd1_bick),
	SOC_ENUM_EXT("Sync Domain 2 BICK", ak7604_sd_fs_enum[1], get_sd2_bick, set_sd2_bick),
	SOC_ENUM_EXT("Sync Domain 3 BICK", ak7604_sd_fs_enum[1], get_sd3_bick, set_sd3_bick),
	SOC_ENUM_EXT("Sync Domain 4 BICK", ak7604_sd_fs_enum[1], get_sd4_bick, set_sd4_bick),

	SOC_ENUM_EXT("PLL Input Clock", ak7604_pllset_enum[0], get_pllinput, set_pllinput),
	SOC_ENUM_EXT("XTI Frequency", ak7604_pllset_enum[1], get_xtifs, set_xtifs),

	SOC_ENUM_EXT("SDIN1 TDM Setting", ak7604_tdm_enum[0], get_sdin1_tdm, set_sdin1_tdm),
	SOC_ENUM_EXT("SDOUT1 TDM Setting", ak7604_tdm_enum[0], get_sdout1_tdm, set_sdout1_tdm),

	SOC_ENUM_EXT("SDIN1 Slot Length", ak7604_slotlen_enum[0], get_sdin1_slot, set_sdin1_slot),
	SOC_ENUM_EXT("SDIN2 Slot Length", ak7604_slotlen_enum[0], get_sdin2_slot, set_sdin2_slot),
	SOC_ENUM_EXT("SDIN3 Slot Length", ak7604_slotlen_enum[0], get_sdin3_slot, set_sdin3_slot),
	SOC_ENUM_EXT("SDIN4 Slot Length", ak7604_slotlen_enum[0], get_sdin4_slot, set_sdin4_slot),

	SOC_ENUM_EXT("SDOUT1 Slot Length", ak7604_slotlen_enum[0], get_sdout1_slot, set_sdout1_slot),
	SOC_ENUM_EXT("SDOUT2 Slot Length", ak7604_slotlen_enum[0], get_sdout2_slot, set_sdout2_slot),
	SOC_ENUM_EXT("SDOUT3 Slot Length", ak7604_slotlen_enum[0], get_sdout3_slot, set_sdout3_slot),

	SOC_ENUM("SDOUT1 Fast Mode Setting", ak7604_sdout_modeset_enum[0]),
	SOC_ENUM("SDOUT2 Fast Mode Setting", ak7604_sdout_modeset_enum[1]),
	SOC_ENUM("SDOUT3 Fast Mode Setting", ak7604_sdout_modeset_enum[2]),

	SOC_ENUM("SDOUT1 Pin Output Select", ak7604_outportsel_enum[0]),
	SOC_ENUM("SDOUT2 Pin Output Select", ak7604_outportsel_enum[1]),
	SOC_ENUM("SDOUT3 Pin Output Select", ak7604_outportsel_enum[2]),

	SOC_ENUM("SDIN3 Pin Input Select", ak7604_inportsel_enum[0]),
	SOC_ENUM("BICK3 Pin Input Select", ak7604_inportsel_enum[1]),
	SOC_ENUM("LRCK3 Pin Input Select", ak7604_inportsel_enum[2]),

	SOC_SINGLE("SRC1 Mute", AK7604_71_SRCMUTE_SETTING, 4, 1, 0),
	SOC_SINGLE("SRC2 Mute", AK7604_71_SRCMUTE_SETTING, 5, 1, 0),
	SOC_SINGLE("SRC3 Mute", AK7604_71_SRCMUTE_SETTING, 6, 1, 0),
	SOC_SINGLE("SRC4 Mute", AK7604_71_SRCMUTE_SETTING, 7, 1, 0),
	SOC_ENUM("SRC1 Mute Semi-Auto", ak7604_src_softmute_enum[0]),
	SOC_ENUM("SRC2 Mute Semi-Auto", ak7604_src_softmute_enum[1]),
	SOC_ENUM("SRC3 Mute Semi-Auto", ak7604_src_softmute_enum[2]),
	SOC_ENUM("SRC4 Mute Semi-Auto", ak7604_src_softmute_enum[3]),
	SOC_SINGLE("SRC1 Group Delay Matching", AK7604_72_SRCGROUTP_SETTING, 0, 3, 0),
	SOC_SINGLE("SRC2 Group Delay Matching", AK7604_72_SRCGROUTP_SETTING, 2, 3, 0),
	SOC_SINGLE("SRC3 Group Delay Matching", AK7604_72_SRCGROUTP_SETTING, 4, 3, 0),
	SOC_SINGLE("SRC4 Group Delay Matching", AK7604_72_SRCGROUTP_SETTING, 6, 3, 0),
	SOC_ENUM("SRC1 Digital Filter", ak7604_src_dfil_enum[0]),
	SOC_ENUM("SRC2 Digital Filter", ak7604_src_dfil_enum[1]),
	SOC_ENUM("SRC3 Digital Filter", ak7604_src_dfil_enum[2]),
	SOC_ENUM("SRC4 Digital Filter", ak7604_src_dfil_enum[3]),

	SOC_SINGLE("DSP JX0 Enable", AK7604_61_DSP_SETTING2, 4, 1, 0),
	SOC_SINGLE("DSP JX1 Enable", AK7604_61_DSP_SETTING2, 5, 1, 0),
	SOC_SINGLE("DSP JX2 Enable", AK7604_61_DSP_SETTING2, 6, 1, 0),

	SOC_SINGLE("STO DSP WDT Error Out", AK7604_53_STO_SETTING, 6, 1, 1),
	SOC_SINGLE("STO CRC Error Out", AK7604_53_STO_SETTING, 5, 1, 0),
	SOC_SINGLE("STO PLL Lock Signal Out", AK7604_53_STO_SETTING, 4, 1, 0),
	SOC_SINGLE("STO SRC4 Lock Out", AK7604_53_STO_SETTING, 3, 1, 0),
	SOC_SINGLE("STO SRC3 Lock Out", AK7604_53_STO_SETTING, 2, 1, 0),
	SOC_SINGLE("STO SRC2 Lock Out", AK7604_53_STO_SETTING, 1, 1, 0),
	SOC_SINGLE("STO SRC1 Lock Out", AK7604_53_STO_SETTING, 0, 1, 0),

	SOC_ENUM("DSP DRAM Bank Size", ak7604_dsp_dram_enum[0]),
	SOC_ENUM("DSP DRAM Bank Addressing", ak7604_dsp_dram_enum[1]),

	SOC_ENUM_EXT("DSP Firmware PRAM", ak7604_firmware_enum[0], get_DSP_write_pram, set_DSP_write_pram),
	SOC_ENUM_EXT("DSP Firmware CRAM", ak7604_firmware_enum[1], get_DSP_write_cram, set_DSP_write_cram),
#ifdef AK7604_DEBUG
	SOC_ENUM_EXT("Reg Read", ak7604_test_enum[0], get_test_reg, set_test_reg),
#endif

#ifdef AK7604_AUDIO_EFFECT
	SOC_SINGLE_EXT("AE_IN_mute", SND_SOC_NOPM, 0, 1, 0, get_IN_mute, set_IN_mute),
	SOC_SINGLE_EXT("AE_INT1_mute", SND_SOC_NOPM, 0, 1, 0, get_INT1_mute, set_INT1_mute),
	SOC_SINGLE_EXT("AE_INT2_mute", SND_SOC_NOPM, 0, 1, 0, get_INT2_mute, set_INT2_mute),
	SOC_SINGLE_EXT("AE_Front_mute", SND_SOC_NOPM, 0, 1, 0, get_FRONT_mute, set_FRONT_mute),
	SOC_SINGLE_EXT("AE_Rear_mute", SND_SOC_NOPM, 0, 1, 0, get_REAR_mute, set_REAR_mute),
	SOC_SINGLE_EXT("AE_SW_mute", SND_SOC_NOPM, 0, 1, 0, get_SW_mute, set_SW_mute),
	SOC_ENUM_EXT("AE_A_Lch_Gain", ak7604_dsp_data_enum[3],get_ANA_L_Gain, set_ANA_L_Gain),
	SOC_ENUM_EXT("AE_A_Rch_Gain", ak7604_dsp_data_enum[3],get_ANA_R_Gain, set_ANA_R_Gain),
	SOC_ENUM_EXT("AE_D_Lch_Gain", ak7604_dsp_data_enum[3],get_DIG_L_Gain, set_DIG_L_Gain),
	SOC_ENUM_EXT("AE_D_Rch_Gain", ak7604_dsp_data_enum[3],get_DIG_R_Gain, set_DIG_R_Gain),
	SOC_ENUM_EXT("AE_IN_dVol", ak7604_dsp_data_enum[3],get_IN_dVol, set_IN_dVol),
	SOC_ENUM_EXT("AE_INT1_Lch_dVol", ak7604_dsp_data_enum[3],get_INT1L_dVol, set_INT1L_dVol),
	SOC_ENUM_EXT("AE_INT1_Rch_dVol", ak7604_dsp_data_enum[3],get_INT1R_dVol, set_INT1R_dVol),
	SOC_ENUM_EXT("AE_INT2_Lch_dVol", ak7604_dsp_data_enum[3],get_INT2L_dVol, set_INT2L_dVol),
	SOC_ENUM_EXT("AE_INT2_Rch_dVol", ak7604_dsp_data_enum[3],get_INT2R_dVol, set_INT2R_dVol),
	SOC_ENUM_EXT("AE_FL_dVol", ak7604_dsp_data_enum[3],get_FL_dVol, set_FL_dVol),
	SOC_ENUM_EXT("AE_FR_dVol", ak7604_dsp_data_enum[3],get_FR_dVol, set_FR_dVol),
	SOC_ENUM_EXT("AE_RL_dVol", ak7604_dsp_data_enum[3],get_RL_dVol, set_RL_dVol),
	SOC_ENUM_EXT("AE_RR_dVol", ak7604_dsp_data_enum[3],get_RR_dVol, set_RR_dVol),
	SOC_ENUM_EXT("AE_SWL_dVol", ak7604_dsp_data_enum[3],get_SWL_dVol, set_SWL_dVol),
	SOC_ENUM_EXT("AE_SWR_dVol", ak7604_dsp_data_enum[3],get_SWR_dVol, set_SWR_dVol),
#ifdef AK7604_DEBUG
	SOC_SINGLE_EXT("AE_SpectrumAnalyzer_test", SND_SOC_NOPM, 0, 1, 0, get_read_spectrum, set_read_spectrum),
#endif
	SOC_SINGLE_EXT("AE_F1_DeEmphasis", SND_SOC_NOPM, 0, 1, 0, get_DeEmphasis, set_DeEmphasis),
	SOC_SINGLE_EXT("AE_F2_BaseMidTreble", SND_SOC_NOPM, 0, 1, 0, get_BaseMidTreble, set_BaseMidTreble),
	SOC_ENUM_EXT("AE_F3_BaseMidTreble_Sel",  ak7604_dsp_data_enum[9], get_BaseMidTreble_sel, set_BaseMidTreble_sel),
	SOC_SINGLE_EXT("AE_F3_BEX", SND_SOC_NOPM, 0, 1, 1, get_BEX, set_BEX),
	SOC_ENUM_EXT("AE_F3_BEX_Sel",  ak7604_dsp_data_enum[10], get_BEX_sel, set_BEX_sel),
	SOC_SINGLE_EXT("AE_F4_Compressor", SND_SOC_NOPM, 0, 1, 0, get_Compressor, set_Compressor),
	SOC_SINGLE_EXT("AE_F5_Surround", SND_SOC_NOPM, 0, 1, 0, get_Surround, set_Surround),
	SOC_SINGLE_EXT("AE_F6_Loudness", SND_SOC_NOPM, 0, 1, 0, get_Loudness, set_Loudness),
	SOC_ENUM_EXT("AE_F6_Loudness_Flt_Sel", ak7604_dsp_data_enum[4], get_Loudness_Flt_sel, set_Loudness_Flt_sel),

	SOC_ENUM_EXT("AE_EQ_Mode", ak7604_dsp_data_enum[0], get_dsp_equalizer, set_dsp_equalizer),

	SOC_ENUM_EXT("AE_Delay_Mode", ak7604_dsp_data_enum[5], get_dsp_delay, set_dsp_delay),

	SOC_ENUM_EXT("AE_Input_Sel", ak7604_dsp_data_enum[1], get_dsp_input_mode, set_dsp_input_mode),
	SOC_SINGLE_EXT("AE_Function_Sw", SND_SOC_NOPM, 0, 1, 1, get_dsp_Function_SW, set_dsp_Function_SW),
	SOC_ENUM_EXT("AE_SubWoofer_Sw", ak7604_dsp_data_enum[2], get_dsp_Subwoofer_SW, set_dsp_Subwoofer_SW),

	SOC_ENUM_EXT("AE_MixerFL_1", ak7604_dsp_data_enum[3],get_MixerFL1_dVol, set_MixerFL1_dVol),
	SOC_ENUM_EXT("AE_MixerFL_2", ak7604_dsp_data_enum[3],get_MixerFL2_dVol, set_MixerFL2_dVol),
	SOC_ENUM_EXT("AE_MixerFL_3", ak7604_dsp_data_enum[3],get_MixerFL3_dVol, set_MixerFL3_dVol),
	SOC_ENUM_EXT("AE_MixerFR_1", ak7604_dsp_data_enum[3],get_MixerFR1_dVol, set_MixerFR1_dVol),
	SOC_ENUM_EXT("AE_MixerFR_2", ak7604_dsp_data_enum[3],get_MixerFR2_dVol, set_MixerFR2_dVol),
	SOC_ENUM_EXT("AE_MixerFR_3", ak7604_dsp_data_enum[3],get_MixerFR3_dVol, set_MixerFR3_dVol),
	SOC_ENUM_EXT("AE_MixerRL_1", ak7604_dsp_data_enum[3],get_MixerRL1_dVol, set_MixerRL1_dVol),
	SOC_ENUM_EXT("AE_MixerRL_2", ak7604_dsp_data_enum[3],get_MixerRL2_dVol, set_MixerRL2_dVol),
	SOC_ENUM_EXT("AE_MixerRL_3", ak7604_dsp_data_enum[3],get_MixerRL3_dVol, set_MixerRL3_dVol),
	SOC_ENUM_EXT("AE_MixerRR_1", ak7604_dsp_data_enum[3],get_MixerRR1_dVol, set_MixerRR1_dVol),
	SOC_ENUM_EXT("AE_MixerRR_2", ak7604_dsp_data_enum[3],get_MixerRR2_dVol, set_MixerRR2_dVol),
	SOC_ENUM_EXT("AE_MixerRR_3", ak7604_dsp_data_enum[3],get_MixerRR3_dVol, set_MixerRR3_dVol),

	SOC_ENUM_EXT("AE_MICnMusic_Lch_Sel", ak7604_dsp_data_enum[6],get_dsp_outLsel_SW, set_dsp_outLsel_SW),
	SOC_ENUM_EXT("AE_MICnMusic_Rch_Sel", ak7604_dsp_data_enum[7],get_dsp_outRsel_SW, set_dsp_outRsel_SW),

	SOC_ENUM_EXT("AE_DZF_Enable_Sw", ak7604_dsp_data_enum[8],get_dsp_DZF_Setting, set_dsp_DZF_Setting),
#endif

};

static const char *ak7604_micbias1_select_texts[] =
		{"LineIn", "MicBias"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak7604_micbias1_mux_enum, ak7604_micbias1_select_texts);

static const struct snd_kcontrol_new ak7604_micbias1_mux_control =
	SOC_DAPM_ENUM("MicBias1 Select", ak7604_micbias1_mux_enum);


static const char *ak7604_micbias2_select_texts[] =
		{"LineIn", "MicBias"};

static SOC_ENUM_SINGLE_VIRT_DECL(ak7604_micbias2_mux_enum, ak7604_micbias2_select_texts);

static const struct snd_kcontrol_new ak7604_micbias2_mux_control =
	SOC_DAPM_ENUM("MicBias2 Select", ak7604_micbias2_mux_enum);

static const char * const ak7604_adc_input_texts[] = {
	"INP_N", "AIN1_GND", "AIN1", "AIN2", "AIN3"
};

static int ak7604_adc_input_values[] = {
	0, 1, 2, 4, 6
};

static SOC_VALUE_ENUM_SINGLE_DECL(ak7604_adc_input_enum,
				  AK7604_8D_AIN_SELECT, 1,
				  0x7, ak7604_adc_input_texts,
				  ak7604_adc_input_values);

static const struct snd_kcontrol_new ak7604_adc_input_mux_control =
	SOC_DAPM_ENUM("ADC select", ak7604_adc_input_enum);

static const char *ak7604_source_select_texts[] = {
	"ALL0",    "SDIN1A",   "SDIN1B",  "SDIN1C",  "SDIN1D",
	"SDIN2",   "SDIN3",    "SDIN4",   "DSPO1",   "DSPO2",
    "DSPO3",   "DSPO4",    "DSPO5",   "DSPO6",    "ADC",
    "SRC1O",   "SRC2O",    "SRC3O",   "SRC4O"
};

static const struct soc_enum ak7604_sout1a_mux_enum =
	SOC_ENUM_SINGLE(AK7604_19_SDOUT1A_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_sout1a_mux_control =
	SOC_DAPM_ENUM("SOUT1A Select", ak7604_sout1a_mux_enum);

static const struct soc_enum ak7604_sout1b_mux_enum =
	SOC_ENUM_SINGLE(AK7604_1A_SDOUT1B_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_sout1b_mux_control =
	SOC_DAPM_ENUM("SOUT1B Select", ak7604_sout1b_mux_enum);

static const struct soc_enum ak7604_sout1c_mux_enum =
	SOC_ENUM_SINGLE(AK7604_1B_SDOUT1C_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_sout1c_mux_control =
	SOC_DAPM_ENUM("SOUT1C Select", ak7604_sout1c_mux_enum);

static const struct soc_enum ak7604_sout1d_mux_enum =
	SOC_ENUM_SINGLE(AK7604_1C_SDOUT1D_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_sout1d_mux_control =
	SOC_DAPM_ENUM("SOUT1D Select", ak7604_sout1d_mux_enum);

static const struct soc_enum ak7604_sout2_mux_enum =
	SOC_ENUM_SINGLE(AK7604_1D_SDOUT2_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_sout2_mux_control =
	SOC_DAPM_ENUM("SOUT2 Select", ak7604_sout2_mux_enum);

static const struct soc_enum ak7604_sout3_mux_enum =
	SOC_ENUM_SINGLE(AK7604_1E_SDOUT3_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_sout3_mux_control =
	SOC_DAPM_ENUM("SOUT3 Select", ak7604_sout3_mux_enum);

static const struct soc_enum ak7604_dac1_mux_enum =
	SOC_ENUM_SINGLE(AK7604_1F_DAC1_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dac1_mux_control =
	SOC_DAPM_ENUM("DAC1 Select", ak7604_dac1_mux_enum);

static const struct soc_enum ak7604_dac2_mux_enum =
	SOC_ENUM_SINGLE(AK7604_20_DAC2_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dac2_mux_control =
	SOC_DAPM_ENUM("DAC2 Select", ak7604_dac2_mux_enum);

static const struct soc_enum ak7604_dac3_mux_enum =
	SOC_ENUM_SINGLE(AK7604_21_DAC3_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dac3_mux_control =
	SOC_DAPM_ENUM("DAC3 Select", ak7604_dac3_mux_enum);


static const struct soc_enum ak7604_dspin1_mux_enum =
	SOC_ENUM_SINGLE(AK7604_22_DSPDIN1_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dspin1_mux_control =
	SOC_DAPM_ENUM("DSPIN1 Select", ak7604_dspin1_mux_enum);


static const struct soc_enum ak7604_dspin2_mux_enum =
	SOC_ENUM_SINGLE(AK7604_23_DSPDIN2_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dspin2_mux_control =
	SOC_DAPM_ENUM("DSP1 IN2 Select", ak7604_dspin2_mux_enum);

static const struct soc_enum ak7604_dspin3_mux_enum =
	SOC_ENUM_SINGLE(AK7604_24_DSPDIN3_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dspin3_mux_control =
	SOC_DAPM_ENUM("DSP1 IN3 Select", ak7604_dspin3_mux_enum);

static const struct soc_enum ak7604_dspin4_mux_enum =
	SOC_ENUM_SINGLE(AK7604_25_DSPDIN4_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dspin4_mux_control =
	SOC_DAPM_ENUM("DSP1 IN4 Select", ak7604_dspin4_mux_enum);

static const struct soc_enum ak7604_dspin5_mux_enum =
	SOC_ENUM_SINGLE(AK7604_26_DSPDIN5_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dspin5_mux_control =
	SOC_DAPM_ENUM("DSP1 IN5 Select", ak7604_dspin5_mux_enum);

static const struct soc_enum ak7604_dspin6_mux_enum =
	SOC_ENUM_SINGLE(AK7604_27_DSPDIN6_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_dspin6_mux_control =
	SOC_DAPM_ENUM("DSP1 IN6 Select", ak7604_dspin6_mux_enum);

static const struct soc_enum ak7604_src1_mux_enum =
	SOC_ENUM_SINGLE(AK7604_28_SRC1_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_src1_mux_control =
	SOC_DAPM_ENUM("SRC1 Select", ak7604_src1_mux_enum);

static const struct soc_enum ak7604_src2_mux_enum =
	SOC_ENUM_SINGLE(AK7604_29_SRC2_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_src2_mux_control =
	SOC_DAPM_ENUM("SRC2 Select", ak7604_src2_mux_enum);

static const struct soc_enum ak7604_src3_mux_enum =
	SOC_ENUM_SINGLE(AK7604_2A_SRC3_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_src3_mux_control =
	SOC_DAPM_ENUM("SRC3 Select", ak7604_src3_mux_enum);

static const struct soc_enum ak7604_src4_mux_enum =
	SOC_ENUM_SINGLE(AK7604_2B_SRC4_SELECT, 0,
			ARRAY_SIZE(ak7604_source_select_texts), ak7604_source_select_texts);

static const struct snd_kcontrol_new ak7604_src4_mux_control =
	SOC_DAPM_ENUM("SRC4 Select", ak7604_src4_mux_enum);


static int ak7604_ClockReset(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	//struct snd_soc_codec *codec = w->codec;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int n;

	n = AK7604_CRAMRUN_CHECKCOUNT;
	while( ( n > 0 ) && (ak7604->cramwrite==1) ) { 
		mdelay(1);
		n--;
	}

	switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			ak7604->dsprun = 1;
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x01, 0x01);   // HRESETN bit = 1
			break;

		case SND_SOC_DAPM_POST_PMD:
			snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x01, 0x0);   // HRESETN bit = 0
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, 0x0);  // CKRESETN bit = 0
			ak7604->dsprun = 0;
			break;
	}
	return 0;
}

static const struct snd_kcontrol_new dspout1_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSPIN1", AK7604_VIRT_C7_DSPOUT1_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSPIN2", AK7604_VIRT_C7_DSPOUT1_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSPIN3", AK7604_VIRT_C7_DSPOUT1_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSPIN4", AK7604_VIRT_C7_DSPOUT1_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSPIN5", AK7604_VIRT_C7_DSPOUT1_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSPIN6", AK7604_VIRT_C7_DSPOUT1_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dspout2_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSPIN1", AK7604_VIRT_C8_DSPOUT2_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSPIN2", AK7604_VIRT_C8_DSPOUT2_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSPIN3", AK7604_VIRT_C8_DSPOUT2_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSPIN4", AK7604_VIRT_C8_DSPOUT2_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSPIN5", AK7604_VIRT_C8_DSPOUT2_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSPIN6", AK7604_VIRT_C8_DSPOUT2_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dspout3_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSPIN1", AK7604_VIRT_C9_DSPOUT3_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSPIN2", AK7604_VIRT_C9_DSPOUT3_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSPIN3", AK7604_VIRT_C9_DSPOUT3_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSPIN4", AK7604_VIRT_C9_DSPOUT3_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSPIN5", AK7604_VIRT_C9_DSPOUT3_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSPIN6", AK7604_VIRT_C9_DSPOUT3_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dspout4_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSPIN1", AK7604_VIRT_CA_DSPOUT4_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSPIN2", AK7604_VIRT_CA_DSPOUT4_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSPIN3", AK7604_VIRT_CA_DSPOUT4_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSPIN4", AK7604_VIRT_CA_DSPOUT4_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSPIN5", AK7604_VIRT_CA_DSPOUT4_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSPIN6", AK7604_VIRT_CA_DSPOUT4_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dspout5_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSPIN1", AK7604_VIRT_CB_DSPOUT5_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSPIN2", AK7604_VIRT_CB_DSPOUT5_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSPIN3", AK7604_VIRT_CB_DSPOUT5_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSPIN4", AK7604_VIRT_CB_DSPOUT5_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSPIN5", AK7604_VIRT_CB_DSPOUT5_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSPIN6", AK7604_VIRT_CB_DSPOUT5_MIX, 5, 1, 0),
};

static const struct snd_kcontrol_new dspout6_mixer_kctrl[] = {
	SOC_DAPM_SINGLE("DSPIN1", AK7604_VIRT_CC_DSPOUT6_MIX, 0, 1, 0),
	SOC_DAPM_SINGLE("DSPIN2", AK7604_VIRT_CC_DSPOUT6_MIX, 1, 1, 0),
	SOC_DAPM_SINGLE("DSPIN3", AK7604_VIRT_CC_DSPOUT6_MIX, 2, 1, 0),
	SOC_DAPM_SINGLE("DSPIN4", AK7604_VIRT_CC_DSPOUT6_MIX, 3, 1, 0),
	SOC_DAPM_SINGLE("DSPIN5", AK7604_VIRT_CC_DSPOUT6_MIX, 4, 1, 0),
	SOC_DAPM_SINGLE("DSPIN6", AK7604_VIRT_CC_DSPOUT6_MIX, 5, 1, 0),
};

/* ak7604 dapm widgets */
static const struct snd_soc_dapm_widget ak7604_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY_S("Clock Power", 1, SND_SOC_NOPM, 0, 0, ak7604_ClockReset, 
                                   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("CODEC Power", 2, AK7604_A3_RESETCONTROL, 1, 0, NULL, 0),

// CODEC
	SND_SOC_DAPM_ADC("ADC", NULL, AK7604_A1_POWERMANAGEMENT1, 7, 0),
	SND_SOC_DAPM_DAC("DAC1", NULL, AK7604_A1_POWERMANAGEMENT1, 4, 0),
	SND_SOC_DAPM_DAC("DAC2", NULL, AK7604_A1_POWERMANAGEMENT1, 5, 0),
	SND_SOC_DAPM_DAC("DAC3", NULL, AK7604_A1_POWERMANAGEMENT1, 6, 0),

// SRC
	SND_SOC_DAPM_PGA("SRC1", AK7604_A2_POWERMANAGEMENT2, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SRC2", AK7604_A2_POWERMANAGEMENT2, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SRC3", AK7604_A2_POWERMANAGEMENT2, 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SRC4", AK7604_A2_POWERMANAGEMENT2, 7, 0, NULL, 0),

// DSP
	SND_SOC_DAPM_SUPPLY_S("DSP", 3, AK7604_A3_RESETCONTROL, 2, 0, NULL,0),

	SND_SOC_DAPM_PGA("DSPO1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("DSPO6", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("DSPOUT1 Mixer", SND_SOC_NOPM, 0, 0, &dspout1_mixer_kctrl[0], ARRAY_SIZE(dspout1_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSPOUT2 Mixer", SND_SOC_NOPM, 0, 0, &dspout2_mixer_kctrl[0], ARRAY_SIZE(dspout2_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSPOUT3 Mixer", SND_SOC_NOPM, 0, 0, &dspout3_mixer_kctrl[0], ARRAY_SIZE(dspout3_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSPOUT4 Mixer", SND_SOC_NOPM, 0, 0, &dspout4_mixer_kctrl[0], ARRAY_SIZE(dspout4_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSPOUT5 Mixer", SND_SOC_NOPM, 0, 0, &dspout5_mixer_kctrl[0], ARRAY_SIZE(dspout5_mixer_kctrl)),
	SND_SOC_DAPM_MIXER("DSPOUT6 Mixer", SND_SOC_NOPM, 0, 0, &dspout6_mixer_kctrl[0], ARRAY_SIZE(dspout6_mixer_kctrl)),

// Digital Input/Output
	SND_SOC_DAPM_AIF_IN("SDIN1A", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN1B", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN1C", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN1D", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN3", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN4", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	
	SND_SOC_DAPM_AIF_OUT("SDOUT1", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT2", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT3", "AIF3 Capture", 0, SND_SOC_NOPM, 0, 0),

// Analog Input
	SND_SOC_DAPM_INPUT("INP_N"),
	SND_SOC_DAPM_INPUT("AIN1_GND"),
	SND_SOC_DAPM_INPUT("AIN1"),
	SND_SOC_DAPM_INPUT("AIN2"),
	SND_SOC_DAPM_INPUT("AIN3"),

	SND_SOC_DAPM_MUX("ADC MUX", SND_SOC_NOPM, 0, 0, &ak7604_adc_input_mux_control),

// MIC Bias
	SND_SOC_DAPM_MICBIAS("MicBias1", AK7604_A1_POWERMANAGEMENT1, 2, 0),
	SND_SOC_DAPM_MUX("MicBias1 MUX", SND_SOC_NOPM, 0, 0, &ak7604_micbias1_mux_control),

	SND_SOC_DAPM_MICBIAS("MicBias2", AK7604_A1_POWERMANAGEMENT1, 3, 0),
	SND_SOC_DAPM_MUX("MicBias2 MUX", SND_SOC_NOPM, 0, 0, &ak7604_micbias2_mux_control),

// Analog Output
	SND_SOC_DAPM_OUTPUT("AOUT1"),
	SND_SOC_DAPM_OUTPUT("AOUT2"),
	SND_SOC_DAPM_OUTPUT("AOUT3"),


// Source Selector
	SND_SOC_DAPM_MUX("SDOUT1A Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_sout1a_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1B Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_sout1b_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1C Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_sout1c_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1D Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_sout1d_mux_control),
	SND_SOC_DAPM_MUX("SDOUT2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_sout2_mux_control),
	SND_SOC_DAPM_MUX("SDOUT3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_sout3_mux_control),

	SND_SOC_DAPM_MUX("DAC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dac1_mux_control),
	SND_SOC_DAPM_MUX("DAC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dac2_mux_control),
	SND_SOC_DAPM_MUX("DAC3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dac3_mux_control),

	SND_SOC_DAPM_MUX("DSPIN1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dspin1_mux_control),
	SND_SOC_DAPM_MUX("DSPIN2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dspin2_mux_control),
	SND_SOC_DAPM_MUX("DSPIN3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dspin3_mux_control),
	SND_SOC_DAPM_MUX("DSPIN4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dspin4_mux_control),
	SND_SOC_DAPM_MUX("DSPIN5 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dspin5_mux_control),
	SND_SOC_DAPM_MUX("DSPIN6 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_dspin6_mux_control),

	SND_SOC_DAPM_MUX("SRC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_src1_mux_control),
	SND_SOC_DAPM_MUX("SRC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_src2_mux_control),
	SND_SOC_DAPM_MUX("SRC3 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_src3_mux_control),
	SND_SOC_DAPM_MUX("SRC4 Source Selector", SND_SOC_NOPM, 0, 0, &ak7604_src4_mux_control),

};

static const struct snd_soc_dapm_route ak7604_intercon[] = 
{
	{"CODEC Power", NULL, "Clock Power"},
	{"SDOUT1", NULL, "Clock Power"},
	{"SDOUT2", NULL, "Clock Power"},
	{"SDOUT3", NULL, "Clock Power"},

	{"ADC", NULL, "CODEC Power"},
	{"DAC1", NULL, "CODEC Power"},
	{"DAC2", NULL, "CODEC Power"},
	{"DAC3", NULL, "CODEC Power"},

	{"ADC MUX", "INP_N", "INP_N"},
	{"ADC MUX", "AIN1_GND", "AIN1_GND"},
	{"ADC MUX", "AIN1", "AIN1"},
	{"ADC MUX", "AIN2", "AIN2"},
	{"ADC MUX", "AIN3", "AIN3"},

	{"MicBias1", NULL, "ADC MUX"},
	{"MicBias1 MUX", "LineIn", "ADC MUX"},
	{"MicBias1 MUX", "MicBias", "MicBias1"},

	{"MicBias2", NULL, "MicBias1 MUX"},
	{"MicBias2 MUX", "LineIn", "ADC MUX"},
	{"MicBias2 MUX", "MicBias", "MicBias2"},

	{"ADC", NULL, "MicBias2 MUX"},

	{"SDOUT1", NULL, "SDOUT1A Source Selector"},
	{"SDOUT1", NULL, "SDOUT1B Source Selector"},
	{"SDOUT1", NULL, "SDOUT1C Source Selector"},
	{"SDOUT1", NULL, "SDOUT1D Source Selector"},
	{"SDOUT2", NULL, "SDOUT2 Source Selector"},
	{"SDOUT3", NULL, "SDOUT3 Source Selector"},

	{"DAC1", NULL, "DAC1 Source Selector"},
	{"DAC2", NULL, "DAC2 Source Selector"},
	{"DAC3", NULL, "DAC3 Source Selector"},

	{"AOUT1", NULL, "DAC1"},
	{"AOUT2", NULL, "DAC2"},
	{"AOUT3", NULL, "DAC3"},

	{"DSPO1", NULL, "DSP"},
	{"DSPO2", NULL, "DSP"},
	{"DSPO3", NULL, "DSP"},
	{"DSPO4", NULL, "DSP"},
	{"DSPO5", NULL, "DSP"},
	{"DSPO6", NULL, "DSP"},
	
	{"DSPO1", NULL, "DSPOUT1 Mixer"},
	{"DSPO2", NULL, "DSPOUT2 Mixer"},
	{"DSPO3", NULL, "DSPOUT3 Mixer"},
	{"DSPO4", NULL, "DSPOUT4 Mixer"},
	{"DSPO5", NULL, "DSPOUT5 Mixer"},
	{"DSPO6", NULL, "DSPOUT6 Mixer"},

	{"DSPOUT1 Mixer", "DSPIN1", "DSPIN1 Source Selector"},
	{"DSPOUT1 Mixer", "DSPIN2", "DSPIN2 Source Selector"},
	{"DSPOUT1 Mixer", "DSPIN3", "DSPIN3 Source Selector"},
	{"DSPOUT1 Mixer", "DSPIN4", "DSPIN4 Source Selector"},
	{"DSPOUT1 Mixer", "DSPIN5", "DSPIN5 Source Selector"},
	{"DSPOUT1 Mixer", "DSPIN6", "DSPIN6 Source Selector"},

	{"DSPOUT2 Mixer", "DSPIN1", "DSPIN1 Source Selector"},
	{"DSPOUT2 Mixer", "DSPIN2", "DSPIN2 Source Selector"},
	{"DSPOUT2 Mixer", "DSPIN3", "DSPIN3 Source Selector"},
	{"DSPOUT2 Mixer", "DSPIN4", "DSPIN4 Source Selector"},
	{"DSPOUT2 Mixer", "DSPIN5", "DSPIN5 Source Selector"},
	{"DSPOUT2 Mixer", "DSPIN6", "DSPIN6 Source Selector"},

	{"DSPOUT3 Mixer", "DSPIN1", "DSPIN1 Source Selector"},
	{"DSPOUT3 Mixer", "DSPIN2", "DSPIN2 Source Selector"},
	{"DSPOUT3 Mixer", "DSPIN3", "DSPIN3 Source Selector"},
	{"DSPOUT3 Mixer", "DSPIN4", "DSPIN4 Source Selector"},
	{"DSPOUT3 Mixer", "DSPIN5", "DSPIN5 Source Selector"},
	{"DSPOUT3 Mixer", "DSPIN6", "DSPIN6 Source Selector"},

	{"DSPOUT4 Mixer", "DSPIN1", "DSPIN1 Source Selector"},
	{"DSPOUT4 Mixer", "DSPIN2", "DSPIN2 Source Selector"},
	{"DSPOUT4 Mixer", "DSPIN3", "DSPIN3 Source Selector"},
	{"DSPOUT4 Mixer", "DSPIN4", "DSPIN4 Source Selector"},
	{"DSPOUT4 Mixer", "DSPIN5", "DSPIN5 Source Selector"},
	{"DSPOUT4 Mixer", "DSPIN6", "DSPIN6 Source Selector"},

	{"DSPOUT5 Mixer", "DSPIN1", "DSPIN1 Source Selector"},
	{"DSPOUT5 Mixer", "DSPIN2", "DSPIN2 Source Selector"},
	{"DSPOUT5 Mixer", "DSPIN3", "DSPIN3 Source Selector"},
	{"DSPOUT5 Mixer", "DSPIN4", "DSPIN4 Source Selector"},
	{"DSPOUT5 Mixer", "DSPIN5", "DSPIN5 Source Selector"},
	{"DSPOUT5 Mixer", "DSPIN6", "DSPIN6 Source Selector"},

	{"DSPOUT6 Mixer", "DSPIN1", "DSPIN1 Source Selector"},
	{"DSPOUT6 Mixer", "DSPIN2", "DSPIN2 Source Selector"},
	{"DSPOUT6 Mixer", "DSPIN3", "DSPIN3 Source Selector"},
	{"DSPOUT6 Mixer", "DSPIN4", "DSPIN4 Source Selector"},
	{"DSPOUT6 Mixer", "DSPIN5", "DSPIN5 Source Selector"},
	{"DSPOUT6 Mixer", "DSPIN6", "DSPIN6 Source Selector"},

	{"SRC1", NULL, "SRC1 Source Selector"},
	{"SRC2", NULL, "SRC2 Source Selector"},
	{"SRC3", NULL, "SRC3 Source Selector"},
	{"SRC4", NULL, "SRC4 Source Selector"},

	{"DSPIN1 Source Selector", "SDIN1A", "SDIN1A"},
	{"DSPIN1 Source Selector", "SDIN1B", "SDIN1B"},
	{"DSPIN1 Source Selector", "SDIN1C", "SDIN1C"},
	{"DSPIN1 Source Selector", "SDIN1D", "SDIN1D"},
	{"DSPIN1 Source Selector", "SDIN2", "SDIN2"},
	{"DSPIN1 Source Selector", "SDIN3", "SDIN3"},
	{"DSPIN1 Source Selector", "SDIN4", "SDIN4"},
	{"DSPIN1 Source Selector", "DSPO1", "DSPO1"},
	{"DSPIN1 Source Selector", "DSPO2", "DSPO2"},
	{"DSPIN1 Source Selector", "DSPO3", "DSPO3"},
	{"DSPIN1 Source Selector", "DSPO4", "DSPO4"},
	{"DSPIN1 Source Selector", "DSPO5", "DSPO5"},
	{"DSPIN1 Source Selector", "DSPO6", "DSPO6"},
	{"DSPIN1 Source Selector", "ADC", "ADC"},
	{"DSPIN1 Source Selector", "SRC1O", "SRC1"},
	{"DSPIN1 Source Selector", "SRC2O", "SRC2"},
	{"DSPIN1 Source Selector", "SRC3O", "SRC3"},
	{"DSPIN1 Source Selector", "SRC4O", "SRC4"},

	{"DSPIN2 Source Selector", "SDIN1A", "SDIN1A"},
	{"DSPIN2 Source Selector", "SDIN1B", "SDIN1B"},
	{"DSPIN2 Source Selector", "SDIN1C", "SDIN1C"},
	{"DSPIN2 Source Selector", "SDIN1D", "SDIN1D"},
	{"DSPIN2 Source Selector", "SDIN2", "SDIN2"},
	{"DSPIN2 Source Selector", "SDIN3", "SDIN3"},
	{"DSPIN2 Source Selector", "SDIN4", "SDIN4"},
	{"DSPIN2 Source Selector", "DSPO1", "DSPO1"},
	{"DSPIN2 Source Selector", "DSPO2", "DSPO2"},
	{"DSPIN2 Source Selector", "DSPO3", "DSPO3"},
	{"DSPIN2 Source Selector", "DSPO4", "DSPO4"},
	{"DSPIN2 Source Selector", "DSPO5", "DSPO5"},
	{"DSPIN2 Source Selector", "DSPO6", "DSPO6"},
	{"DSPIN2 Source Selector", "ADC", "ADC"},
	{"DSPIN2 Source Selector", "SRC1O", "SRC1"},
	{"DSPIN2 Source Selector", "SRC2O", "SRC2"},	
	{"DSPIN2 Source Selector", "SRC3O", "SRC3"},	
	{"DSPIN2 Source Selector", "SRC4O", "SRC4"},	
	
	{"DSPIN3 Source Selector", "SDIN1A", "SDIN1A"},
	{"DSPIN3 Source Selector", "SDIN1B", "SDIN1B"},
	{"DSPIN3 Source Selector", "SDIN1C", "SDIN1C"},
	{"DSPIN3 Source Selector", "SDIN1D", "SDIN1D"},
	{"DSPIN3 Source Selector", "SDIN2", "SDIN2"},
	{"DSPIN3 Source Selector", "SDIN3", "SDIN3"},
	{"DSPIN3 Source Selector", "SDIN4", "SDIN4"},
	{"DSPIN3 Source Selector", "DSPO1", "DSPO1"},
	{"DSPIN3 Source Selector", "DSPO2", "DSPO2"},
	{"DSPIN3 Source Selector", "DSPO3", "DSPO3"},
	{"DSPIN3 Source Selector", "DSPO4", "DSPO4"},
	{"DSPIN3 Source Selector", "DSPO5", "DSPO5"},
	{"DSPIN3 Source Selector", "DSPO6", "DSPO6"},
	{"DSPIN3 Source Selector", "ADC", "ADC"},
	{"DSPIN3 Source Selector", "SRC1O", "SRC1"},
	{"DSPIN3 Source Selector", "SRC2O", "SRC2"},
	{"DSPIN3 Source Selector", "SRC3O", "SRC3"},
	{"DSPIN3 Source Selector", "SRC4O", "SRC4"},
	
	{"DSPIN4 Source Selector", "SDIN1A", "SDIN1A"},
	{"DSPIN4 Source Selector", "SDIN1B", "SDIN1B"},
	{"DSPIN4 Source Selector", "SDIN1C", "SDIN1C"},
	{"DSPIN4 Source Selector", "SDIN1D", "SDIN1D"},
	{"DSPIN4 Source Selector", "SDIN2", "SDIN2"},
	{"DSPIN4 Source Selector", "SDIN3", "SDIN3"},
	{"DSPIN4 Source Selector", "SDIN4", "SDIN4"},
	{"DSPIN4 Source Selector", "DSPO1", "DSPO1"},
	{"DSPIN4 Source Selector", "DSPO2", "DSPO2"},
	{"DSPIN4 Source Selector", "DSPO3", "DSPO3"},
	{"DSPIN4 Source Selector", "DSPO4", "DSPO4"},
	{"DSPIN4 Source Selector", "DSPO5", "DSPO5"},
	{"DSPIN4 Source Selector", "DSPO6", "DSPO6"},
	{"DSPIN4 Source Selector", "ADC", "ADC"},
	{"DSPIN4 Source Selector", "SRC1O", "SRC1"},
	{"DSPIN4 Source Selector", "SRC2O", "SRC2"},
	{"DSPIN4 Source Selector", "SRC3O", "SRC3"},
	{"DSPIN4 Source Selector", "SRC4O", "SRC4"},
	
	{"DSPIN5 Source Selector", "SDIN1A", "SDIN1A"},
	{"DSPIN5 Source Selector", "SDIN1B", "SDIN1B"},
	{"DSPIN5 Source Selector", "SDIN1C", "SDIN1C"},
	{"DSPIN5 Source Selector", "SDIN1D", "SDIN1D"},
	{"DSPIN5 Source Selector", "SDIN2", "SDIN2"},
	{"DSPIN5 Source Selector", "SDIN3", "SDIN3"},
	{"DSPIN5 Source Selector", "SDIN4", "SDIN4"},
	{"DSPIN5 Source Selector", "DSPO1", "DSPO1"},
	{"DSPIN5 Source Selector", "DSPO2", "DSPO2"},
	{"DSPIN5 Source Selector", "DSPO3", "DSPO3"},
	{"DSPIN5 Source Selector", "DSPO4", "DSPO4"},
	{"DSPIN5 Source Selector", "DSPO5", "DSPO5"},
	{"DSPIN5 Source Selector", "DSPO6", "DSPO6"},
	{"DSPIN5 Source Selector", "ADC", "ADC"},
	{"DSPIN5 Source Selector", "SRC1O", "SRC1"},
	{"DSPIN5 Source Selector", "SRC2O", "SRC2"},
	{"DSPIN5 Source Selector", "SRC3O", "SRC3"},
	{"DSPIN5 Source Selector", "SRC4O", "SRC4"},
	
	{"DSPIN6 Source Selector", "SDIN1A", "SDIN1A"},
	{"DSPIN6 Source Selector", "SDIN1B", "SDIN1B"},
	{"DSPIN6 Source Selector", "SDIN1C", "SDIN1C"},
	{"DSPIN6 Source Selector", "SDIN1D", "SDIN1D"},
	{"DSPIN6 Source Selector", "SDIN2", "SDIN2"},
	{"DSPIN6 Source Selector", "SDIN3", "SDIN3"},
	{"DSPIN6 Source Selector", "SDIN4", "SDIN4"},
	{"DSPIN6 Source Selector", "DSPO1", "DSPO1"},
	{"DSPIN6 Source Selector", "DSPO2", "DSPO2"},
	{"DSPIN6 Source Selector", "DSPO3", "DSPO3"},
	{"DSPIN6 Source Selector", "DSPO4", "DSPO4"},
	{"DSPIN6 Source Selector", "DSPO5", "DSPO5"},
	{"DSPIN6 Source Selector", "DSPO6", "DSPO6"},
	{"DSPIN6 Source Selector", "ADC", "ADC"},
	{"DSPIN6 Source Selector", "SRC1O", "SRC1"},
	{"DSPIN6 Source Selector", "SRC2O", "SRC2"},
	{"DSPIN6 Source Selector", "SRC3O", "SRC3"},
	{"DSPIN6 Source Selector", "SRC4O", "SRC4"},

	{"SRC1 Source Selector", "SDIN1A", "SDIN1A"},
	{"SRC1 Source Selector", "SDIN1B", "SDIN1B"},
	{"SRC1 Source Selector", "SDIN1C", "SDIN1C"},
	{"SRC1 Source Selector", "SDIN1D", "SDIN1D"},
	{"SRC1 Source Selector", "SDIN2", "SDIN2"},
	{"SRC1 Source Selector", "SDIN3", "SDIN3"},
	{"SRC1 Source Selector", "SDIN4", "SDIN4"},
	{"SRC1 Source Selector", "DSPO1", "DSPO1"},
	{"SRC1 Source Selector", "DSPO2", "DSPO2"},
	{"SRC1 Source Selector", "DSPO3", "DSPO3"},
	{"SRC1 Source Selector", "DSPO4", "DSPO4"},
	{"SRC1 Source Selector", "DSPO5", "DSPO5"},
	{"SRC1 Source Selector", "DSPO6", "DSPO6"},
	{"SRC1 Source Selector", "ADC", "ADC"},
	{"SRC1 Source Selector", "SRC1O", "SRC1"},
	{"SRC1 Source Selector", "SRC2O", "SRC2"},
	{"SRC1 Source Selector", "SRC3O", "SRC3"},
	{"SRC1 Source Selector", "SRC4O", "SRC4"},

	{"SRC2 Source Selector", "SDIN1A", "SDIN1A"},
	{"SRC2 Source Selector", "SDIN1B", "SDIN1B"},
	{"SRC2 Source Selector", "SDIN1C", "SDIN1C"},
	{"SRC2 Source Selector", "SDIN1D", "SDIN1D"},
	{"SRC2 Source Selector", "SDIN2", "SDIN2"},
	{"SRC2 Source Selector", "SDIN3", "SDIN3"},
	{"SRC2 Source Selector", "SDIN4", "SDIN4"},
	{"SRC2 Source Selector", "DSPO1", "DSPO1"},
	{"SRC2 Source Selector", "DSPO2", "DSPO2"},
	{"SRC2 Source Selector", "DSPO3", "DSPO3"},
	{"SRC2 Source Selector", "DSPO4", "DSPO4"},
	{"SRC2 Source Selector", "DSPO5", "DSPO5"},
	{"SRC2 Source Selector", "DSPO6", "DSPO6"},
	{"SRC2 Source Selector", "ADC", "ADC"},
	{"SRC2 Source Selector", "SRC1O", "SRC1"},
	{"SRC2 Source Selector", "SRC2O", "SRC2"},
	{"SRC2 Source Selector", "SRC3O", "SRC3"},
	{"SRC2 Source Selector", "SRC4O", "SRC4"},

	{"SRC4 Source Selector", "SDIN1A", "SDIN1A"},
	{"SRC4 Source Selector", "SDIN1B", "SDIN1B"},
	{"SRC4 Source Selector", "SDIN1C", "SDIN1C"},
	{"SRC4 Source Selector", "SDIN1D", "SDIN1D"},
	{"SRC4 Source Selector", "SDIN2", "SDIN2"},
	{"SRC4 Source Selector", "SDIN3", "SDIN3"},
	{"SRC4 Source Selector", "SDIN4", "SDIN4"},
	{"SRC4 Source Selector", "DSPO1", "DSPO1"},
	{"SRC4 Source Selector", "DSPO2", "DSPO2"},
	{"SRC4 Source Selector", "DSPO3", "DSPO3"},
	{"SRC4 Source Selector", "DSPO4", "DSPO4"},
	{"SRC4 Source Selector", "DSPO5", "DSPO5"},
	{"SRC4 Source Selector", "DSPO6", "DSPO6"},
	{"SRC4 Source Selector", "ADC", "ADC"},
	{"SRC4 Source Selector", "SRC1O", "SRC1"},
	{"SRC4 Source Selector", "SRC2O", "SRC2"},
	{"SRC4 Source Selector", "SRC3O", "SRC3"},
	{"SRC4 Source Selector", "SRC4O", "SRC4"},

	{"SRC3 Source Selector", "SDIN1A", "SDIN1A"},
	{"SRC3 Source Selector", "SDIN1B", "SDIN1B"},
	{"SRC3 Source Selector", "SDIN1C", "SDIN1C"},
	{"SRC3 Source Selector", "SDIN1D", "SDIN1D"},
	{"SRC3 Source Selector", "SDIN2", "SDIN2"},
	{"SRC3 Source Selector", "SDIN3", "SDIN3"},
	{"SRC3 Source Selector", "SDIN4", "SDIN4"},
	{"SRC3 Source Selector", "DSPO1", "DSPO1"},
	{"SRC3 Source Selector", "DSPO2", "DSPO2"},
	{"SRC3 Source Selector", "DSPO3", "DSPO3"},
	{"SRC3 Source Selector", "DSPO4", "DSPO4"},
	{"SRC3 Source Selector", "DSPO5", "DSPO5"},
	{"SRC3 Source Selector", "DSPO6", "DSPO6"},
	{"SRC3 Source Selector", "ADC", "ADC"},
	{"SRC3 Source Selector", "SRC1O", "SRC1"},
	{"SRC3 Source Selector", "SRC2O", "SRC2"},
	{"SRC3 Source Selector", "SRC3O", "SRC3"},
	{"SRC3 Source Selector", "SRC4O", "SRC4"},

	{"SDOUT1A Source Selector", "SDIN1A", "SDIN1A"},
	{"SDOUT1A Source Selector", "SDIN1B", "SDIN1B"},
	{"SDOUT1A Source Selector", "SDIN1C", "SDIN1C"},
	{"SDOUT1A Source Selector", "SDIN1D", "SDIN1D"},
	{"SDOUT1A Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1A Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1A Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT1A Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1A Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1A Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1A Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1A Source Selector", "DSPO5", "DSPO5"},
	{"SDOUT1A Source Selector", "DSPO6", "DSPO6"},
	{"SDOUT1A Source Selector", "ADC", "ADC"},
	{"SDOUT1A Source Selector", "SRC1O", "SRC1"},
	{"SDOUT1A Source Selector", "SRC2O", "SRC2"},
	{"SDOUT1A Source Selector", "SRC3O", "SRC3"},
	{"SDOUT1A Source Selector", "SRC4O", "SRC4"},

	{"SDOUT1B Source Selector", "SDIN1A", "SDIN1A"},
	{"SDOUT1B Source Selector", "SDIN1B", "SDIN1B"},
	{"SDOUT1B Source Selector", "SDIN1C", "SDIN1C"},
	{"SDOUT1B Source Selector", "SDIN1D", "SDIN1D"},
	{"SDOUT1B Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1B Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1B Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT1B Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1B Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1B Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1B Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1B Source Selector", "DSPO5", "DSPO5"},
	{"SDOUT1B Source Selector", "DSPO6", "DSPO6"},
	{"SDOUT1B Source Selector", "ADC", "ADC"},
	{"SDOUT1B Source Selector", "SRC1O", "SRC1"},
	{"SDOUT1B Source Selector", "SRC2O", "SRC2"},
	{"SDOUT1B Source Selector", "SRC3O", "SRC3"},
	{"SDOUT1B Source Selector", "SRC4O", "SRC4"},

	{"SDOUT1C Source Selector", "SDIN1A", "SDIN1A"},
	{"SDOUT1C Source Selector", "SDIN1B", "SDIN1B"},
	{"SDOUT1C Source Selector", "SDIN1C", "SDIN1C"},
	{"SDOUT1C Source Selector", "SDIN1D", "SDIN1D"},
	{"SDOUT1C Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1C Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1C Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT1C Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1C Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1C Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1C Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1C Source Selector", "DSPO5", "DSPO5"},
	{"SDOUT1C Source Selector", "DSPO6", "DSPO6"},
	{"SDOUT1C Source Selector", "ADC", "ADC"},
	{"SDOUT1C Source Selector", "SRC1O", "SRC1"},
	{"SDOUT1C Source Selector", "SRC2O", "SRC2"},
	{"SDOUT1C Source Selector", "SRC3O", "SRC3"},
	{"SDOUT1C Source Selector", "SRC4O", "SRC4"},

	{"SDOUT1D Source Selector", "SDIN1A", "SDIN1A"},
	{"SDOUT1D Source Selector", "SDIN1B", "SDIN1B"},
	{"SDOUT1D Source Selector", "SDIN1C", "SDIN1C"},
	{"SDOUT1D Source Selector", "SDIN1D", "SDIN1D"},
	{"SDOUT1D Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT1D Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT1D Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT1D Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT1D Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT1D Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT1D Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT1D Source Selector", "DSPO5", "DSPO5"},
	{"SDOUT1D Source Selector", "DSPO6", "DSPO6"},
	{"SDOUT1D Source Selector", "ADC", "ADC"},
	{"SDOUT1D Source Selector", "SRC1O", "SRC1"},
	{"SDOUT1D Source Selector", "SRC2O", "SRC2"},
	{"SDOUT1D Source Selector", "SRC3O", "SRC3"},
	{"SDOUT1D Source Selector", "SRC4O", "SRC4"},

	{"SDOUT2 Source Selector", "SDIN1A", "SDIN1A"},
	{"SDOUT2 Source Selector", "SDIN1B", "SDIN1B"},
	{"SDOUT2 Source Selector", "SDIN1C", "SDIN1C"},
	{"SDOUT2 Source Selector", "SDIN1D", "SDIN1D"},
	{"SDOUT2 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT2 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT2 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT2 Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT2 Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT2 Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT2 Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT2 Source Selector", "DSPO5", "DSPO5"},
	{"SDOUT2 Source Selector", "DSPO6", "DSPO6"},
	{"SDOUT2 Source Selector", "ADC", "ADC"},
	{"SDOUT2 Source Selector", "SRC1O", "SRC1"},
	{"SDOUT2 Source Selector", "SRC2O", "SRC2"},
	{"SDOUT2 Source Selector", "SRC3O", "SRC3"},
	{"SDOUT2 Source Selector", "SRC4O", "SRC4"},

	{"SDOUT3 Source Selector", "SDIN1A", "SDIN1A"},
	{"SDOUT3 Source Selector", "SDIN1B", "SDIN1B"},
	{"SDOUT3 Source Selector", "SDIN1C", "SDIN1C"},
	{"SDOUT3 Source Selector", "SDIN1D", "SDIN1D"},
	{"SDOUT3 Source Selector", "SDIN2", "SDIN2"},
	{"SDOUT3 Source Selector", "SDIN3", "SDIN3"},
	{"SDOUT3 Source Selector", "SDIN4", "SDIN4"},
	{"SDOUT3 Source Selector", "DSPO1", "DSPO1"},
	{"SDOUT3 Source Selector", "DSPO2", "DSPO2"},
	{"SDOUT3 Source Selector", "DSPO3", "DSPO3"},
	{"SDOUT3 Source Selector", "DSPO4", "DSPO4"},
	{"SDOUT3 Source Selector", "DSPO5", "DSPO5"},
	{"SDOUT3 Source Selector", "DSPO6", "DSPO6"},
	{"SDOUT3 Source Selector", "ADC", "ADC"},
	{"SDOUT3 Source Selector", "SRC1O", "SRC1"},
	{"SDOUT3 Source Selector", "SRC2O", "SRC2"},
	{"SDOUT3 Source Selector", "SRC3O", "SRC3"},
	{"SDOUT3 Source Selector", "SRC4O", "SRC4"},

	{"DAC1 Source Selector", "SDIN1A", "SDIN1A"},
	{"DAC1 Source Selector", "SDIN1B", "SDIN1B"},
	{"DAC1 Source Selector", "SDIN1C", "SDIN1C"},
	{"DAC1 Source Selector", "SDIN1D", "SDIN1D"},
	{"DAC1 Source Selector", "SDIN2", "SDIN2"},
	{"DAC1 Source Selector", "SDIN3", "SDIN3"},
	{"DAC1 Source Selector", "DSPO1", "DSPO1"},
	{"DAC1 Source Selector", "DSPO2", "DSPO2"},
	{"DAC1 Source Selector", "DSPO3", "DSPO3"},
	{"DAC1 Source Selector", "DSPO4", "DSPO4"},
	{"DAC1 Source Selector", "DSPO5", "DSPO5"},
	{"DAC1 Source Selector", "DSPO6", "DSPO6"},
	{"DAC1 Source Selector", "ADC", "ADC"},
	{"DAC1 Source Selector", "SRC1O", "SRC1"},
	{"DAC1 Source Selector", "SRC2O", "SRC2"},
	{"DAC1 Source Selector", "SRC3O", "SRC3"},
	{"DAC1 Source Selector", "SRC4O", "SRC4"},

	{"DAC2 Source Selector", "SDIN1A", "SDIN1A"},
	{"DAC2 Source Selector", "SDIN1B", "SDIN1B"},
	{"DAC2 Source Selector", "SDIN1C", "SDIN1C"},
	{"DAC2 Source Selector", "SDIN1D", "SDIN1D"},
	{"DAC2 Source Selector", "SDIN2", "SDIN2"},
	{"DAC2 Source Selector", "SDIN3", "SDIN3"},
	{"DAC2 Source Selector", "SDIN4", "SDIN4"},
	{"DAC2 Source Selector", "DSPO1", "DSPO1"},
	{"DAC2 Source Selector", "DSPO2", "DSPO2"},
	{"DAC2 Source Selector", "DSPO3", "DSPO3"},
	{"DAC2 Source Selector", "DSPO4", "DSPO4"},
	{"DAC2 Source Selector", "DSPO5", "DSPO5"},
	{"DAC2 Source Selector", "DSPO6", "DSPO6"},
	{"DAC2 Source Selector", "ADC", "ADC"},
	{"DAC2 Source Selector", "SRC1O", "SRC1"},
	{"DAC2 Source Selector", "SRC2O", "SRC2"},
	{"DAC2 Source Selector", "SRC3O", "SRC3"},
	{"DAC2 Source Selector", "SRC4O", "SRC4"},

	{"DAC3 Source Selector", "SDIN1A", "SDIN1A"},
	{"DAC3 Source Selector", "SDIN1B", "SDIN1B"},
	{"DAC3 Source Selector", "SDIN1C", "SDIN1C"},
	{"DAC3 Source Selector", "SDIN1D", "SDIN1D"},
	{"DAC3 Source Selector", "SDIN2", "SDIN2"},
	{"DAC3 Source Selector", "SDIN3", "SDIN3"},
	{"DAC3 Source Selector", "SDIN4", "SDIN4"},
	{"DAC3 Source Selector", "DSPO1", "DSPO1"},
	{"DAC3 Source Selector", "DSPO2", "DSPO2"},
	{"DAC3 Source Selector", "DSPO3", "DSPO3"},
	{"DAC3 Source Selector", "DSPO4", "DSPO4"},
	{"DAC3 Source Selector", "DSPO5", "DSPO5"},
	{"DAC3 Source Selector", "DSPO6", "DSPO6"},
	{"DAC3 Source Selector", "ADC", "ADC"},
	{"DAC3 Source Selector", "SRC1O", "SRC1"},
	{"DAC3 Source Selector", "SRC2O", "SRC2"},
	{"DAC3 Source Selector", "SRC3O", "SRC3"},
	{"DAC3 Source Selector", "SRC4O", "SRC4"},


};

static int ak7604_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int nSDNo;
	int fsno, nmax;
	int DIODLbit, addr, value;

	akdbgprt("\t[AK7604] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7604->fs = params_rate(params);

	akdbgprt("\t[AK7604] %s fs=%d\n",__FUNCTION__, ak7604->fs );
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

	switch (dai->id) {
		case AIF_PORT1: nSDNo = 0; break;
		case AIF_PORT2: nSDNo = 1; break;
		case AIF_PORT3: nSDNo = 2; break;
		case AIF_PORT4: nSDNo = 3; break;
		default:
			pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
			return -EINVAL;
			break;
	}			

	fsno = 0;
	nmax = sizeof(sdfstab) / sizeof(sdfstab[0]);
	akdbgprt("\t[AK7604] %s nmax = %d\n",__FUNCTION__, nmax);
	
	do {
		if ( ak7604->fs <= sdfstab[fsno] ) break;
		fsno++;
	} while ( fsno < nmax );
	akdbgprt("\t[AK7604] %s fsno = %d\n",__FUNCTION__, fsno);

	if ( fsno == nmax ) {
		pr_err("%s: not support Sampling Frequency : %d\n", __func__, ak7604->fs);
		return -EINVAL;
	}

	akdbgprt("\t[AK7604] %s setSDClock\n",__FUNCTION__);
    mdelay(10);

	ak7604->SDfs[nSDNo] = fsno;
	setSDClock(codec, nSDNo);

	/* set Word length */
	
	addr = AK7604_2E_SDIN1_FORMAT + nSDNo;
	value = DIODLbit;
	snd_soc_update_bits(codec, addr, 0x03, value);
	addr = AK7604_32_SDOUT1_FORMAT + nSDNo;
	snd_soc_update_bits(codec, addr, 0x03, value);

	return 0;
}

static int ak7604_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	akdbgprt("\t[AK7604] %s(%d)\n",__FUNCTION__,__LINE__);

	return 0;
}

static int ak7604_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_codec *codec = dai->codec;
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int format, diolsb, diedge, doedge, dislot, doslot;
	int msnbit;
	int nSDNo, value;
	int addr, mask, shift;

	akdbgprt("\t[AK7604] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (dai->id) {
		case AIF_PORT1: nSDNo = 0; break;
		case AIF_PORT2: nSDNo = 1; break;
		case AIF_PORT3: nSDNo = 2; break;
		case AIF_PORT4: nSDNo = 3; break;
		default:
			pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
			return -EINVAL;
			break;
	}

	/* set master/slave audio interface */

    switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBS_CFS:
			msnbit = 0;
			akdbgprt("\t[AK7604] %s(Slave_nSDNo=%d)\n",__FUNCTION__,nSDNo);
            break;
        case SND_SOC_DAIFMT_CBM_CFM:
			msnbit = 1;
			akdbgprt("\t[AK7604] %s(Master_nSDNo=%d)\n",__FUNCTION__,nSDNo);
            break;
        case SND_SOC_DAIFMT_CBS_CFM:
        case SND_SOC_DAIFMT_CBM_CFS:
        default:
            dev_err(codec->dev, "Clock mode unsupported");
           return -EINVAL;
    	}
	
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			format = AK7604_LRIF_I2S_MODE; // 0
			diolsb = 0;
			diedge = ak7604->DIEDGEbit[nSDNo];
			doedge = ak7604->DOEDGEbit[nSDNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			format = AK7604_LRIF_MSB_MODE; // 5
			diolsb = 0;
			diedge = ak7604->DIEDGEbit[nSDNo];
			doedge = ak7604->DOEDGEbit[nSDNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			format = AK7604_LRIF_LSB_MODE; // 5
			diolsb = 1;
			diedge = ak7604->DIEDGEbit[nSDNo];
			doedge = ak7604->DOEDGEbit[nSDNo];
			dislot = 3;
			doslot = 3;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			format = AK7604_LRIF_PCM_SHORT_MODE; // 6
			diolsb = 0;
			diedge = 1;
			doedge = 1;
			dislot = ak7604->DISLbit[nSDNo];
			doslot = ak7604->DOSLbit[nSDNo];
			break;
		case SND_SOC_DAIFMT_DSP_B:
			format = AK7604_LRIF_PCM_LONG_MODE; // 7
			diolsb = 0;
			diedge = 1;
			doedge = 1;
			dislot = ak7604->DISLbit[nSDNo];
			doslot = ak7604->DOSLbit[nSDNo];
			break;
		default:
			return -EINVAL;
	}

	/* set format */
	setSDMaster(codec, nSDNo, msnbit);
	addr = AK7604_2C_CLOCKFORMAT1 + (nSDNo/2);
	shift = 4 * ((nSDNo+1) % 2);
	value = (format << shift);
	mask = 0xF << shift;
	snd_soc_update_bits(codec, addr, mask, value);

	/* set SDIO format */
	/* set Slot length */
	
	addr = AK7604_2E_SDIN1_FORMAT + nSDNo;
	value = (diedge <<  7) + (diolsb <<  3) + (dislot <<  4);
	snd_soc_update_bits(codec, addr, 0xB8, value);
	
	addr = AK7604_32_SDOUT1_FORMAT + nSDNo;
	value = (doedge <<  7) + (diolsb <<  3) + (doslot <<  4);
	snd_soc_update_bits(codec, addr, 0xB8, value);

	return 0;
}

static bool ak7604_volatile(struct device *dev, unsigned int reg)
{
	bool	ret;

#ifdef AK7604_DEBUG
	if ( reg < AK7604_VIRT_REGISTER ) ret = true;
	else ret = false;
#else
	if ( reg < AK7604_C0_DEVICE_ID ) ret = false;
	else if ( reg < AK7604_VIRT_REGISTER ) ret = true;
	else ret = false;
#endif
	return(ret);
}

static bool ak7604_readable(struct device *dev, unsigned int reg)
{

	bool ret;

	if ( reg == AK7604_03_RESERVED ) ret = false;
	else if ( reg <= AK7604_34_SDOUT3_FORMAT ) ret = true;
	else if ( reg < AK7604_50_INPUT_DATA ) ret = false;
	else if ( reg <= AK7604_53_STO_SETTING ) ret = true;
	else if ( reg < AK7604_60_DSP_SETTING1 ) ret = false;
	else if ( reg <= AK7604_61_DSP_SETTING2 ) ret = true;
	else if ( reg < AK7604_71_SRCMUTE_SETTING ) ret = false;
	else if ( reg <= AK7604_73_SRCFILTER_SETTING ) ret = true;
	else if ( reg < AK7604_81_MIC_SETTING ) ret = false;
	else if ( reg <= AK7604_8E_ADC_MUTEHPF ) ret = true;
	else if ( reg < AK7604_A1_POWERMANAGEMENT1 ) ret = false;
	else if (reg <= AK7604_A3_RESETCONTROL ) ret = true;
	else if ( reg < AK7604_C0_DEVICE_ID ) ret = false;
	else if (reg <= AK7604_VIRT_CC_DSPOUT6_MIX ) ret = true;
	else ret = true;

	return ret;
}

static bool ak7604_writeable(struct device *dev, unsigned int reg)
{
	bool ret;

	if ( reg == AK7604_03_RESERVED ) ret = false;
	else if ( reg <= AK7604_34_SDOUT3_FORMAT ) ret = true;
	else if ( reg < AK7604_50_INPUT_DATA ) ret = false;
	else if ( reg <= AK7604_53_STO_SETTING ) ret = true;
	else if ( reg < AK7604_60_DSP_SETTING1 ) ret = false;
	else if ( reg <= AK7604_61_DSP_SETTING2 ) ret = true;
	else if ( reg < AK7604_71_SRCMUTE_SETTING ) ret = false;
	else if ( reg <= AK7604_73_SRCFILTER_SETTING ) ret = true;
	else if ( reg < AK7604_81_MIC_SETTING ) ret = false;
	else if ( reg <= AK7604_8E_ADC_MUTEHPF ) ret = true;
	else if ( reg < AK7604_A1_POWERMANAGEMENT1 ) ret = false;
	else if (reg <= AK7604_A3_RESETCONTROL ) ret = true;
	else if (reg < AK7604_VIRT_C7_DSPOUT1_MIX ) ret = false;
	else if (reg <= AK7604_VIRT_CC_DSPOUT6_MIX ) ret = true;
	else ret = false;

	return ret;
}

static unsigned int ak7604_i2c_read(
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

unsigned int ak7604_read_register(struct snd_soc_codec *codec, unsigned int reg)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[3], rx[1];
	int	wlen, rlen;
	int val, ret;
	unsigned int rdata;

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > AK7604_MAX_REGISTER);

	if (!ak7604_readable(NULL, reg)) return 0xFF;

	if (!ak7604_volatile(NULL, reg) && ak7604_readable(NULL, reg)) {
		ret = regmap_read(ak7604->regmap, reg, &val);  
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
#if defined(CONFIG_SND_SPI)
	if (ak7604->control_type == SND_SOC_SPI) 
		ret = spi_write_then_read(ak7604->spi, tx, wlen, rx, rlen);
	else
#endif//CONFIG_SND_SPI
		ret = ak7604_i2c_read(ak7604->i2c, tx, wlen, rx, rlen);

	if (ret < 0) {
		akdbgprt("\t[AK7604] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}

	return rdata;
}

static int ak7604_reads(struct snd_soc_codec *codec, u8 *tx, size_t wlen, u8 *rx, size_t rlen)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int ret;
	
	akdbgprt("*****[AK7604] %s tx[0]=%x, %d, %d\n",__FUNCTION__, tx[0],(int)wlen,(int)rlen);
#if defined(CONFIG_SND_SPI)
	if (ak7604->control_type == SND_SOC_SPI)
		ret = spi_write_then_read(ak7604->spi, tx, wlen, rx, rlen);
	else 
#endif//CONFIG_SND_SPI
		ret = ak7604_i2c_read(ak7604->i2c, tx, wlen, rx, rlen);
	

	return ret;  

}

static int ak7604_write_register(struct snd_soc_codec *codec,  unsigned int reg,  unsigned int value)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int ret;

	akdbgprt("[AK7604] %s (%x, %x)\n",__FUNCTION__, reg, value);

	if (reg == SND_SOC_NOPM)
		return 0;

	if (!ak7604_volatile(NULL, reg)) {
		ret = regmap_write(ak7604->regmap, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	wlen = 4;
	tx[0] = (unsigned char)COMMAND_WRITE_REG;
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);
	tx[3] = value;
#if defined(CONFIG_SND_SPI)
	if (ak7604->control_type == SND_SOC_SPI)
		ret = spi_write(ak7604->spi, tx, wlen);
	else
#endif//CONFIG_SND_SPI
		ret = i2c_master_send(ak7604->i2c, tx, wlen);

	return ret;
}

static int ak7604_write_spidmy(struct snd_soc_codec *codec)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	unsigned char tx[4];
	int wlen;
	int rd;

	akdbgprt("[AK7604] %s\n",__FUNCTION__);
	
	rd = 0;

	wlen = 4;
	tx[0] = (unsigned char)(0xDE);
	tx[1] = (unsigned char)(0xAD);
	tx[2] = (unsigned char)(0xDA);
	tx[3] = (unsigned char)(0x7A);
#if defined(CONFIG_SND_SPI)
	if (ak7604->control_type == SND_SOC_SPI)
		rd = spi_write(ak7604->spi, tx, wlen);
#endif//CONFIG_SND_SPI	
	return rd;
}

static int ak7604_writes(struct snd_soc_codec *codec, const u8 *tx, size_t wlen)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int n;
	int rc;

	akdbgprt("[AK7604W] %s tx[0]=%x tx[1]=%x tx[2]=%x, len=%d\n",__FUNCTION__, (int)tx[0], (int)tx[1], (int)tx[2], (int)wlen);

	for ( n = 3 ; n < wlen; n ++ ) {
		akdbgprt("[AK7604W] %s tx[%d]=%x\n",__FUNCTION__, n, (int)tx[n]);
	}
#if defined(CONFIG_SND_SPI)	
	if (ak7604->control_type == SND_SOC_SPI)
		rc = spi_write(ak7604->spi, tx, wlen);
	else 
#endif//CONFIG_SND_SPI	
		rc = i2c_master_send(ak7604->i2c, tx, wlen);

	return rc;
}

static int crc_read(struct snd_soc_codec *codec)
{
	int rc;

	u8	tx[3], rx[2];

	tx[0] = COMMAND_CRC_READ;
	tx[1] = 0;
	tx[2] = 0;

	rc =  ak7604_reads(codec, tx, 3, rx, 2);

	return (rc < 0) ? rc : ((rx[0] << 8) + rx[1]);
}

static int ak7604_set_status(struct snd_soc_codec *codec, enum ak7604_status status)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	switch (status) {
		case RUN:
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, 0x80);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x07, 0x07);  // CRESETN bit = D1RESETN = HRESETN = 1;
			break;
		case DOWNLOAD:
			// 20180222
			ak7604->ckresetn = snd_soc_read(codec, AK7604_02_SYSTEMCLOCK_3);
			ak7604->ckresetn &= 0x80;
			if ( ak7604->ckresetn ) snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, 0x0); // D1RESETN = 0;
			ak7604->dresetn = snd_soc_read(codec, AK7604_A3_RESETCONTROL);
			ak7604->dresetn &= 0x7;
			if ( ak7604->dresetn ) snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x07, 0x0); // D1RESETN = 0;
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x01, 0x01);  // DLRDY bit = 1
			mdelay(1);
			break;
		case DOWNLOAD_FINISH:
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x01, 0x0);  // DLRDY bit = 0
			mdelay(1);
			if(ak7604->dsprun == 1)
			{
				if ( ak7604->ckresetn )snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, ak7604->ckresetn);
				mdelay(10);
				if ( ak7604->ckresetn )snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x07, ak7604->dresetn);
			}
			break;
		case STANDBY:
			snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x07, 0x0); // CRESETN bit = D1RESETN = HRESETN = 0; // 20180222
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, 0x80);  // CKRESETN bit = 1
			break;
		case SUSPEND:
		case POWERDOWN:
			snd_soc_update_bits(codec, AK7604_A3_RESETCONTROL, 0x07, 0x0); // CRESETN bit = D1RESETN = HRESETN = 0; // 20180222
			snd_soc_update_bits(codec, AK7604_02_SYSTEMCLOCK_3, 0x80, 0x0);  // CKRESETN bit = 0
			break;
		default:
		return -EINVAL;
	}

	ak7604->status = status;

	return 0;
}

static int ak7604_ram_download(struct snd_soc_codec *codec, const u8 *tx_ram, u64 num, u16 crc)
{
	int rc;
	int nDSPRun;
	u16	read_crc;
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	if ( ak7604->muteon == 0 ) {
		snd_soc_update_bits(codec, AK7604_89_DAC_MUTEFILTER, 0x70, 0x70);  // DAC Soft Mute ON (DAC1,2,3)
		msleep(50);
	}

	ak7604->cramwrite = 1;
	akdbgprt("\t[AK7604] %s num=%ld\n",__FUNCTION__, (long int)num);

	nDSPRun = snd_soc_read(codec, AK7604_A3_RESETCONTROL);

	ak7604_set_status(codec, DOWNLOAD);
	
	rc = ak7604_writes(codec, tx_ram, num);
	if (rc < 0) {		
		snd_soc_write(codec, AK7604_A3_RESETCONTROL, nDSPRun);
		return rc;
	}

	if ( ( crc != 0 ) && (rc >= 0) )  {
		read_crc = crc_read(codec);
		akdbgprt("\t[AK7604] %s CRC Cal=%x Read=%x\n",__FUNCTION__, (int)crc,(int)read_crc);		
	
		if ( read_crc == crc ) rc = 0;
		else rc = 1;
	}

	ak7604_set_status(codec, DOWNLOAD_FINISH);
	snd_soc_write(codec, AK7604_A3_RESETCONTROL, nDSPRun);

	if ( ak7604->muteon == 0 ) {
		snd_soc_update_bits(codec, AK7604_89_DAC_MUTEFILTER, 0x70, 0x00);  // DAC Soft Mute OFF (DAC1,2,3)
		msleep(50);
	}

	ak7604->cramwrite = 0;
	return rc;
}

#define MAX_RUNTIME_WRITE 51 // 51Byte/3 - 1 = Total 16 CRAM Data at once.
static int ak7604_ram_download_in_runtime(struct snd_soc_codec *codec, const u8 *tx_ram, u64 num)
{
	int rc, i, n;
	int nDSPRun, nCKRESET;
	unsigned char tx[MAX_RUNTIME_WRITE];
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK7604] %s num=%ld\n",__FUNCTION__, (long int)num);

	if(num > MAX_RUNTIME_WRITE)
	{
		akdbgprt("\t[AK7604] %s Max num Error ! =%ld\n",__FUNCTION__,(long int)num);
		return -1;
	}

	ak7604->cramwrite = 1;

	nDSPRun = snd_soc_read(codec, AK7604_A3_RESETCONTROL);
	nCKRESET = snd_soc_read(codec, AK7604_02_SYSTEMCLOCK_3);

	if(!(nCKRESET&0x80 && nDSPRun&0x07) && (ak7604->dsprun == 0))
	{
		akdbgprt("\t[AK7604] %s nCKRESET=%x, nDSPRun=%x\n",__FUNCTION__, nCKRESET,nDSPRun);
		return (-1);
	}	

	tx[0] = COMMAND_WRITE_CRAM_RUN + (unsigned char)((num / 3) - 1);
	tx[1] = (unsigned char)(0xFF & (tx_ram[1] >> 8));
	tx[2] = (unsigned char)(0xFF & tx_ram[2]);

	n = 3;
	for ( i = 3 ; i < num; i ++ ) 
	{
		tx[n++] = tx_ram[i];
	}

	akdbgprt("CAddr : %02x %02x\n", (int)tx[1], (int)tx[2]);
	akdbgprt("CData : %02x %02x %02x\n", (int)tx_ram[0], (int)tx_ram[1], (int)tx_ram[2]);

	rc = ak7604_writes(codec, tx, n);

	if (rc < 0) {		
		return rc;
	}

	tx[0] = COMMAND_WRITE_CRAM_EXEC;
	tx[1] = 0;
	tx[2] = 0;
	rc = ak7604_writes(codec, tx, 3);

	if (rc < 0) {		
		return rc;
	}

	mdelay(1);

	ak7604->cramwrite = 0;

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

	akdbgprt("[AK7604] %s CRC=%x\n",__FUNCTION__, crc);

	return crc;
}

static int ak7604_write_ram(
struct snd_soc_codec *codec,
int	 nPCRam,  // 0 : PRAM, 1 : CRAM
u8 	*upRam,
int	 nWSize)
{
	int n, ret;
	int	wCRC;
	int nMaxSize;

	akdbgprt("[AK7604] %s RamNo=%d, len=%d\n",__FUNCTION__, nPCRam, nWSize);

	switch(nPCRam) {
		case RAMTYPE_PRAM:
			nMaxSize = TOTAL_NUM_OF_PRAM_MAX;
			if (  nWSize > nMaxSize ) {
				printk("%s: PRAM Write size is over! \n", __func__);      
				return(-1);
			}
			break;
		case RAMTYPE_CRAM:
			nMaxSize = TOTAL_NUM_OF_CRAM_MAX;
			if (  nWSize > nMaxSize ) {
				printk("%s: CRAM Write size is over! \n", __func__);      
				return(-1);
			}
			break;
		default:
			return(-1);
			break;
	}

	wCRC = calc_CRC(nWSize, upRam);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7604_ram_download(codec, upRam, nWSize, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! RAM No = %d \n", __func__, nPCRam); 
		return(-1);
	}

	return(0);

}

static int ak7604_firmware_write_ram(struct snd_soc_codec *codec, u16 mode, u16 cmd)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int nNumMode, nMaxLen;
	int nRamSize;
	u8  *ram_basic;
	const struct firmware *fw;
	u8  *fwdn;
	char szFileName[32];

	akdbgprt("[AK7604] %s mode=%d, cmd=%d\n",__FUNCTION__, mode, cmd);

	switch(mode) {
		case RAMTYPE_PRAM:
			nNumMode = sizeof(ak7604_firmware_pram) / sizeof(ak7604_firmware_pram[0]);
			break;
		case RAMTYPE_CRAM:
			nNumMode = sizeof(ak7604_firmware_cram) / sizeof(ak7604_firmware_cram[0]);
			break;
		default:
			akdbgprt("[AK7604] %s mode Error=%d\n",__FUNCTION__, mode);
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
				ram_basic = ak7604_pram_basic;
				nRamSize = sizeof(ak7604_pram_basic);
				break;
			case RAMTYPE_CRAM:
				ram_basic = ak7604_cram_basic;
				nRamSize = sizeof(ak7604_cram_basic);
				break;
			default:
				return( -EINVAL);
		}
		ret = ak7604_write_ram(codec, (int)mode, ram_basic, nRamSize);
	}
	else if(cmd == 7) { //header type
		switch(mode) {
			case RAMTYPE_PRAM:
				ram_basic = ak7604_pram_header;
				nRamSize = sizeof(ak7604_pram_header);
				break;
			case RAMTYPE_CRAM:
				ram_basic = ak7604_cram_header;
				nRamSize = sizeof(ak7604_cram_header);
				break;
			default:
				return( -EINVAL);
		}
		ret = ak7604_write_ram(codec, (int)mode, ram_basic, nRamSize);	
	}	
	else {
		switch(mode) {
			case RAMTYPE_PRAM:
				sprintf(szFileName, "ak7604_pram_%s.bin", ak7604_firmware_pram[cmd]);
				nMaxLen = TOTAL_NUM_OF_PRAM_MAX;
				break;
			case RAMTYPE_CRAM:
				sprintf(szFileName, "ak7604_cram_%s.bin", ak7604_firmware_cram[cmd]);
				nMaxLen = TOTAL_NUM_OF_CRAM_MAX;
				break;
			default:
				return( -EINVAL);
		}
#if defined(CONFIG_SND_SPI)
		if (ak7604->control_type == SND_SOC_SPI)
			ret = request_firmware(&fw, szFileName, &(ak7604->spi->dev));
		else
#endif//CONFIG_SND_SPI		
			ret = request_firmware(&fw, szFileName, &(ak7604->i2c->dev));
		
		if (ret) {
			akdbgprt("[AK7604] %s could not load firmware=%d\n", szFileName, ret);
			return -EINVAL;
		}

		akdbgprt("[AK7604] %s name=%s size=%d\n",__FUNCTION__, szFileName, (int)fw->size);
		if ( fw->size > nMaxLen ) {
			akdbgprt("[AK7604] %s RAM Size Error : %d\n",__FUNCTION__, (int)fw->size);
			release_firmware(fw);
			return -ENOMEM;
		}

		fwdn = kmalloc((unsigned long)fw->size, GFP_KERNEL);
		if (fwdn == NULL) {
			printk(KERN_ERR "failed to buffer vmalloc: %d\n", (int)fw->size);
			release_firmware(fw);
			return -ENOMEM;
		}

		memcpy((void *)fwdn, fw->data, fw->size);

		ret = ak7604_write_ram(codec, (int)mode, (u8 *)fwdn, (fw->size));

		kfree(fwdn);
		release_firmware(fw);
	}

	return ret;
}

static int ak7604_write_cram(
struct snd_soc_codec *codec,
int addr,
int len,
unsigned char *cram_data)
{
	int i, n, ret;
	int nDSPRun, nCKRESET;
	unsigned char tx[MAX_RUNTIME_WRITE];
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("[AK7604] %s addr=%d, len=%d\n",__FUNCTION__, addr, len);

	if ( len > 48 ) {
		akdbgprt("[AK7604] %s Length over!\n",__FUNCTION__);
		return(-1);
	}

	ak7604->cramwrite = 1;

	nDSPRun = snd_soc_read(codec, AK7604_A3_RESETCONTROL);
	nCKRESET = snd_soc_read(codec, AK7604_02_SYSTEMCLOCK_3);

	if ((nCKRESET&0x80 && nDSPRun&0x04)&&(ak7604->dsprun == 1)) {
		tx[0] = COMMAND_WRITE_CRAM_RUN + (unsigned char)((len / 3) - 1);
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}
	else {
		ak7604_set_status(codec, DOWNLOAD);
		tx[0] = COMMAND_WRITE_CRAM;
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}

	n = 3;
	for ( i = 0 ; i < len; i ++ ) {
		tx[n++] = cram_data[i];
	}

	ret = ak7604_writes(codec, tx, n);

	if ((nCKRESET&0x80 && nDSPRun&0x04)&&(ak7604->dsprun == 1)) {
		tx[0] = COMMAND_WRITE_CRAM_EXEC;
		tx[1] = 0;
		tx[2] = 0;
		ret = ak7604_writes(codec, tx, 3);
	}
	else {
		ak7604_set_status(codec, DOWNLOAD_FINISH);
		snd_soc_write(codec, AK7604_A3_RESETCONTROL, nDSPRun);
	}

	mdelay(1);

	ak7604->cramwrite = 0;

	return ret;

}

// 20180223
static int ak7604_read_cram(
struct snd_soc_codec *codec,
int addr,
int len,
unsigned char *cram_data)
{
	int n, max_addr;
	unsigned char tx[3], rx[3];

	akdbgprt("%s:addr=0x%x, len=%d\n", __func__, addr, len);

	max_addr = (TOTAL_NUM_OF_CRAM_MAX-3)/3 - 1;     // 0~4095(0xfff) range

	if (addr > max_addr) {
		akdbgprt("[AK7758] %s max addr(0x%x) reached\n",__FUNCTION__, max_addr);
		return(-1);
	}

	tx[0] = 0x34;
	tx[1] = (unsigned char)(0x0F & (addr >> 8));
	tx[2] = (unsigned char)(0xFF & addr);

	for ( n = 0 ; n < 3 ; n++ ) rx[n] = 0;

	ak7604_set_status(codec, DOWNLOAD);
	ak7604_reads(codec, tx, 3, rx, 3);
	ak7604_set_status(codec, DOWNLOAD_FINISH);

	for ( n = 0 ; n < 3 ; n++ ) cram_data[n] = rx[n];

	akdbgprt("****** CRAM Read ******\n");
	akdbgprt("CAddr : %02x %02x\n", (int)tx[1], (int)tx[2]);
	akdbgprt("CData : %02x %02x %02x\n", (int)rx[0], (int)rx[1], (int)rx[2]);

	return 0;
}


static unsigned long ak7604_readMIR(
struct snd_soc_codec *codec,
int nMIRNo,
unsigned long *dwMIRData)
{
	unsigned char tx[3];
	unsigned char rx[32];
	int n, nRLen;

	if ( nMIRNo >= 8 ) return(-1);

	tx[0] = (unsigned char)COMMAND_MIR_READ;
	tx[1] = 0;
	tx[2] = 0;
	nRLen = 4 * (nMIRNo + 1);

	ak7604_reads(codec, tx, 3, rx, nRLen);

	n = 4 * nMIRNo;
	*dwMIRData = ((0xFF & (unsigned long)rx[n++]) << 20);
	*dwMIRData += ((0xFF & (unsigned long)rx[n++]) << 12);
    *dwMIRData += ((0xFF & (unsigned long)rx[n++]) << 4);
    *dwMIRData += ((0xFF & (unsigned long)rx[n++]) >> 4);

	return(0);

}

static int ak7604_reg_cmd(struct snd_soc_codec *codec, REG_CMD *reg_param, int len)
{
	int i;
	int rc;
	int  addr, value;	

	akdbgprt("*****[AK7604] %s len = %d\n",__FUNCTION__, len);

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

#ifdef AK7604_IO_CONTROL
static long ak7604_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	struct ak7604_priv *ak7604 = (struct ak7604_priv*)file->private_data;
	struct ak7604_wreg_handle ak7604_wreg;
	struct ak7604_wcram_handle  ak7604_wcram;
	struct ak7604_rcram_handle ak7604_rcram;
	void __user *data = (void __user *)args;
	int *val = (int *)args;
	int i;
	unsigned long dwMIRData;
	int ret = 0;
	REG_CMD      regcmd[MAX_WREG];
	unsigned char cram_data[MAX_WCRAM];

	akdbgprt("*****[AK7604] %s cmd, val=%x, %x\n",__FUNCTION__, cmd, val[0]);

	switch(cmd) {
	case AK7604_IOCTL_WRITECRAM:
		if (copy_from_user(&ak7604_wcram, data, sizeof(struct ak7604_wcram_handle)))
			return -EFAULT;
		if ( (  ak7604_wcram.len % 3 ) != 0 ) {
			printk(KERN_ERR "[AK7604] %s CRAM len error\n",__FUNCTION__);
			return -EFAULT;
		}
		if ( ( ak7604_wcram.len < 3 ) || ( ak7604_wcram.len > MAX_WCRAM ) ) {
			printk(KERN_ERR "[AK7604] %s CRAM len error2\n",__FUNCTION__);
			return -EFAULT;
		}
		for ( i = 0 ; i < ak7604_wcram.len ; i ++ ) {
			cram_data[i] = ak7604_wcram.cram[i];
		}
		ret = ak7604_write_cram(ak7604->codec, ak7604_wcram.addr, ak7604_wcram.len, cram_data);
		break;
	case AK7604_IOCTL_WRITEREG:
		if (copy_from_user(&ak7604_wreg, data, sizeof(struct ak7604_wreg_handle)))
			return -EFAULT;
		if ( ( ak7604_wreg.len < 1 ) || ( ak7604_wreg.len > MAX_WREG ) ) {
			printk(KERN_ERR "MAXREG ERROR %d\n", ak7604_wreg.len );
			return -EFAULT;
		}
		for ( i = 0 ; i < ak7604_wreg.len; i ++ ) {
			regcmd[i].addr = ak7604_wreg.regcmd[i].addr;
			regcmd[i].data = ak7604_wreg.regcmd[i].data;
		}
		ak7604_reg_cmd(ak7604->codec, regcmd, ak7604_wreg.len);
		break;	

// 20180223
	case AK7604_IOCTL_READCRAM:
		if (copy_from_user(&ak7604_rcram, data, sizeof(struct ak7604_rcram_handle))) 
		{
			printk(KERN_ERR "[AK7758] %s IOR error\n",__FUNCTION__);
			ret = -EFAULT;
		}

		ret = ak7604_read_cram(ak7604->codec, ak7604_rcram.addr, ak7604_rcram.len, ak7604_rcram.data);

		if(copy_to_user(data, &ak7604_rcram, sizeof(struct ak7604_rcram_handle))) {
			printk(KERN_ERR "[AK7758] %s IOR error\n",__FUNCTION__);
			ret = -EFAULT;
		}
		break;

	case AK7604_IOCTL_GETSTATUS:
		{
			ret = copy_to_user(data, (const void*)&ak7604->status, sizeof(ak7604->status));
			if (ret < 0) {
				akdbgprt("ak7604: get_status error: \n");
				return ret;
			}
		}
		break;

	case AK7604_IOCTL_SETSTATUS:
		ret = ak7604_set_status(ak7604->codec, val[0]);
		if (ret < 0) {
			akdbgprt("ak7604: set_status error: \n");
			return ret;
		}
		break;
#ifdef AK7604_AUDIO_EFFECT
 	case AK7604_IOCTL_SET_SPECTRUM_4BAND:
		ak7604_wcram.addr = AK7604_SPECTRUM_READ_ADDR;
		ak7604_wcram.len = AK7604_WORD_OF_CRAM;
	
		if(val[0]==0){
			cram_data[0]=0x02;cram_data[1]=0x4C;cram_data[2]=0x00;}
		else if(val[0]==1){
			cram_data[0]=0x02;cram_data[1]=0x6C;cram_data[2]=0x00;}
		else if(val[0]==2){
			cram_data[0]=0x02;cram_data[1]=0x8C;cram_data[2]=0x00;}
		else if(val[0]==3){
			cram_data[0]=0x02;cram_data[1]=0xAC;cram_data[2]=0x00;}
		else {
			akdbgprt("ak7604: set_status error: \n");
			return (-EINVAL);
		}
		
		ret = ak7604_write_cram(ak7604->codec, ak7604_wcram.addr, ak7604_wcram.len, &cram_data[0]);
		break;
#endif
 	case AK7604_IOCTL_SETMIR:
		ak7604->MIRNo = val[0];
		if (ret < 0) {
			printk(KERN_ERR "ak7604: set MIR error\n");
			return -EFAULT;
		}
		break;

	case AK7604_IOCTL_GETMIR:
		ak7604_readMIR(ak7604->codec, (0xF & (ak7604->MIRNo)), &dwMIRData);
		ret = copy_to_user(data, (const void*)&dwMIRData, (unsigned long)4);
		if (ret < 0) {
			printk(KERN_ERR "ak7604: get status error\n");
			return -EFAULT;
		}
		break;

	default:
		printk(KERN_ERR "Unknown command required: %d\n", cmd);
		return -EINVAL;
	}

	return ret;
}

static int init_ak7604_pd(struct ak7604_priv *data)
{
	struct _ak7604_pd_handler *ak7604 = &ak7604_pd_handler;

	if (data == NULL)
		return -EFAULT;

	mutex_init(&ak7604->lock);

	mutex_lock(&ak7604->lock);
	ak7604->data = data;
	mutex_unlock(&ak7604->lock);

	return 0;
}

static struct ak7604_priv* get_ak7604_pd(void)
{
	struct _ak7604_pd_handler *ak7604 = &ak7604_pd_handler;

	if (ak7604->data == NULL)
		return NULL;

	mutex_lock(&ak7604->lock);
	ak7604->ref_count++;
	mutex_unlock(&ak7604->lock);

	return ak7604->data;
}

static int rel_ak7604_pd(struct ak7604_priv *data)
{
	struct _ak7604_pd_handler *ak7604 = &ak7604_pd_handler;

	if (ak7604->data == NULL)
		return -EFAULT;

	mutex_lock(&ak7604->lock);
	ak7604->ref_count--;
	mutex_unlock(&ak7604->lock);

	data = NULL;

	return 0;
}

/* AK7604 Misc driver interfaces */
static int ak7604_open(struct inode *inode, struct file *file)
{
	struct ak7604_priv *ak7604;

	ak7604 = get_ak7604_pd();
	file->private_data = ak7604;

	return 0;
}

static int ak7604_close(struct inode *inode, struct file *file)
{
	struct ak7604_priv *ak7604 = (struct ak7604_priv*)file->private_data;

	rel_ak7604_pd(ak7604);

	return 0;
}

// 20180223
static int ak7604_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	struct ak7604_priv *ak7604;

	ak7604 = get_ak7604_pd();
	// file->private_data = ak7604;

	return 0;
}

// 20180223
static int ak7604_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	struct ak7604_priv *ak7604 = (struct ak7604_priv*)file->private_data;
	rel_ak7604_pd(ak7604);

	return 0;
}

static const struct file_operations ak7604_fops = {
	.owner = THIS_MODULE,
	.open = ak7604_open,
	.read = ak7604_read, // 20180223
	.write = ak7604_write, // 20180223
	.release = ak7604_close,
	.unlocked_ioctl = ak7604_ioctl,
};

static struct miscdevice ak7604_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ak7604-dsp",
	.fops = &ak7604_fops,
};

#endif

static int ak7604_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	akdbgprt("\t[AK7604] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		ak7604_set_status(codec, POWERDOWN);
		break;
	}
	//codec->dapm.bias_level = level; //org
	//codec->driver->dapm_widgets->dapm->bias_level = level; // modi but panic

	return 0;
}

static int ak7604_set_dai_mute(struct snd_soc_dai *dai, int mute) 
{
	struct snd_soc_codec *codec = dai->codec;
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int n;

	n = AK7604_CRAMRUN_CHECKCOUNT;
	while( ( n > 0 ) && (ak7604->cramwrite==1) ) { 
		mdelay(1);
		n--;
	}

	if (mute)
		ak7604->muteon = 1;
	else
		ak7604->muteon = 0;

	akdbgprt("\t[AK7604] %s Mute=[%s], play=%d cap=%d\n", __FUNCTION__, mute ? "ON" : "OFF", dai->playback_active, dai->capture_active);

	return 0;
}

#define AK7604_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
				SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
				SNDRV_PCM_RATE_24000 | SNDRV_PCM_RATE_32000 |\
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |\
				SNDRV_PCM_RATE_64000 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_96000 )

#define AK7604_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE

static struct snd_soc_dai_ops ak7604_dai_ops = {
	.hw_params	= ak7604_hw_params,
	.set_sysclk	= ak7604_set_dai_sysclk,
	.set_fmt	= ak7604_set_dai_fmt,
	.digital_mute = ak7604_set_dai_mute,
};

struct snd_soc_dai_driver ak7604_dai[] = {   
	{										 
		.name = "ak7604-aif1",
		.id = AIF_PORT1,
		.playback = {
		       .stream_name = "AIF1 Playback",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7604_RATES,
		       .formats = AK7604_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF1 Capture",
		       .channels_min = 1,
		       .channels_max = 8,
		       .rates = AK7604_RATES,
		       .formats = AK7604_FORMATS,
		},
		.ops = &ak7604_dai_ops,
	},
	{										 
		.name = "ak7604-aif2",
		.id = AIF_PORT2,
		.playback = {
		       .stream_name = "AIF2 Playback",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK7604_RATES,
		       .formats = AK7604_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF2 Capture",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK7604_RATES,
		       .formats = AK7604_FORMATS,
		},
		.ops = &ak7604_dai_ops,
	},
	{										 
		.name = "ak7604-aif3",
		.id = AIF_PORT3,
		.playback = {
		       .stream_name = "AIF3 Playback",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK7604_RATES,
		       .formats = AK7604_FORMATS,
		},
		.capture = {
		       .stream_name = "AIF3 Capture",
		       .channels_min = 1,
		       .channels_max = 2,
		       .rates = AK7604_RATES,
		       .formats = AK7604_FORMATS,
		},
		.ops = &ak7604_dai_ops,
	},
};

static int ak7604_init_reg(struct snd_soc_codec *codec)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int i, devid;

	if ( ak7604->pdn_gpio > 0 ) {
		gpio_set_value(ak7604->pdn_gpio, 0);	
		msleep(1);
		gpio_set_value(ak7604->pdn_gpio, 1);	
		msleep(1);
	}
#if defined(CONFIG_SND_SPI)
	if( ak7604->control_type == SND_SOC_SPI ){
		ak7604_write_spidmy(codec);
	}
#endif//CONFIG_SND_SPI

	devid = snd_soc_read(codec, AK7604_C0_DEVICE_ID);
	printk("[AK7604] %s  Device ID = 0x%X\n",__FUNCTION__, devid);
	if ( devid != 0x04 ) {
		akdbgprt("********* This is not AK7604! *********\n");
		akdbgprt("********* This is not AK7604! *********\n");
		akdbgprt("********* This is not AK7604! *********\n");
	}
	
	ak7604->fs = 48000;
	ak7604->PLLInput = 0;
	ak7604->XtiFs    = 0;
	ak7604->dresetn  = 0;

	setPLLOut(codec);

	for ( i = 0 ; i < (NUM_SYNCDOMAIN - 1) ; i ++ ) ak7604->Master[i]=0;
	ak7604->Master[3] = 1;//SYNC4 fixed master. 

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak7604->SDBick[i]  = 0; //64fs
		ak7604->SDfs[i] = 5;  // 48kHz
		ak7604->SDCks[i] = 0;  // Low

		setSDClock(codec, i);
	}

	for ( i = 0 ; i < NUM_SYNCDOMAIN ; i ++ ) {
		ak7604->TDMSDINbit[i] = 0;
		ak7604->TDMSDOUTbit[i] = 0;
		ak7604->DIEDGEbit[i] = 0;
		ak7604->DOEDGEbit[i] = 0;
		ak7604->DISLbit[i] = 0;
		ak7604->DOSLbit[i] = 0;
	}

	// DSP BANK Setting for Audio Effect.
	snd_soc_write(codec, AK7604_60_DSP_SETTING1, 0x02); // DRMBK -> 2048/4096, DRMA -> Ring/Ring

	snd_soc_write(codec, AK7604_0F_SYNCDOMAIN_SEL1, 0x12);//SyncN -> LR/BICKN(if Master)
	snd_soc_write(codec, AK7604_10_SYNCDOMAIN_SEL2, 0x30);//SyncN -> LR/BICKN(if Master)
	
	snd_soc_write(codec, AK7604_0D_SDIN1_2_SYNC, 0x12); //SDIN1(LR/BICK1), SDIN2(LR/BICK2)
	snd_soc_write(codec, AK7604_0E_SDIN3_4_SYNC, 0x31); //SDIN3(LR/BICK3), SDIN4(LR/BICK1)

	snd_soc_write(codec, AK7604_81_MIC_SETTING, 0x3); // MIC Gain Zero Cross Enable

	return 0;
}

static int ak7604_parse_dt(struct ak7604_priv *ak7604)
{
	struct device *dev;
	struct device_node *np;
#if defined(CONFIG_SND_SPI)
	if (ak7604->control_type == SND_SOC_SPI)
		dev = &(ak7604->spi->dev);
	else
#endif//CONFIG_SND_SPI	
		dev = &(ak7604->i2c->dev);

	np = dev->of_node;

	if (!np) {
		akdbgprt("\t[AK7604] %s np error!\n",__FUNCTION__);
		return -1;
	}

	ak7604->pdn_gpio = of_get_named_gpio(np, "pdn-gpio", 0);
	akdbgprt("\t[AK7604] %s pdn-gpio=%d\n",__FUNCTION__, ak7604->pdn_gpio);
	if (ak7604->pdn_gpio < 0) {
		ak7604->pdn_gpio = -1;
		return -1;
	}

	if( !gpio_is_valid(ak7604->pdn_gpio) ) {
		printk(KERN_ERR "ak7604 pdn pin(%u) is invalid\n", ak7604->pdn_gpio);
		return -1;
	}

	return 0;
}

static int ak7604_probe(struct snd_soc_codec *codec)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	akdbgprt("\t[AK7604] %s(%d)\n",__FUNCTION__,__LINE__);

	ak7604->codec = codec;
	ret = ak7604_parse_dt(ak7604);
	if( ret < 0 )ak7604->pdn_gpio = -1;
	if( ak7604->pdn_gpio > 0){
		ret = gpio_request(ak7604->pdn_gpio, "ak7604 pdn");
		akdbgprt("\t[AK7604] %s : gpio_request ret = %d\n",__FUNCTION__, ret);
		gpio_direction_output(ak7604->pdn_gpio, 0);
	}

	ak7604_init_reg(codec);

	ak7604->MIRNo = 0;
	ak7604->status = POWERDOWN;
	
	ak7604->DSPPramMode = 0;
	ak7604->DSPCramMode = 0;

	ak7604->cramaddr = 0;
	ak7604->cramcount = 0;

#ifdef AK7604_AUDIO_EFFECT
	ak7604->AE_A_Lch_dVol=121; // 0dB
	ak7604->AE_A_Rch_dVol=121; // 0dB
	ak7604->AE_D_Lch_dVol=121; // 0dB
	ak7604->AE_D_Rch_dVol=121; // 0dB
	ak7604->AE_IN_dVol=121; // 0dB

	ak7604->AE_INT1_Lch_dVol=121; // 0dB
	ak7604->AE_INT1_Rch_dVol=121; // 0dB
	ak7604->AE_INT2_Lch_dVol=121; // 0dB
	ak7604->AE_INT2_Rch_dVol=121; // 0dB
	ak7604->AE_FL_dVol=121; // 0dB;
	ak7604->AE_FR_dVol=121; // 0dB;
	ak7604->AE_RL_dVol=121; // 0dB;
	ak7604->AE_RR_dVol=121; // 0dB;
	ak7604->AE_SWL_dVol=121; // 0dB;
	ak7604->AE_SWR_dVol=121; // 0dB;

	ak7604->AE_IN_mute=0; // off;
	ak7604->AE_INT1_mute=0; // off;;
	ak7604->AE_INT2_mute=0; // off;;
	ak7604->AE_FRONT_mute=0; // off;;
	ak7604->AE_REAR_mute=0; // off;;
	ak7604->AE_SW_mute=0;

	ak7604->AE_F1_DeEmphasis=0; // off
	ak7604->AE_F2_BaseMidTreble=0; // off
	ak7604->AE_F2_BaseMidTreble_Sel=0; // off
	ak7604->AE_F3_BEX=0; // off
	ak7604->AE_F3_BEX_sel = 0; // default
	ak7604->AE_F4_Compressor=0; // off
	ak7604->AE_F5_Surround=0; // off
	ak7604->AE_F6_Loudness=0; // off
	ak7604->AE_F6_Loudness_Flt_Sel=0; // pattern1

	ak7604->AE_EQ_Mode=0; // Flat

	ak7604->AE_Delay_Mode=0; // Default(off)

	ak7604->AE_Input_Sel=1; // Digital
	ak7604->AE_SubWoofer_Sw=0; // Subwoofer
	ak7604->AE_Function_Sw=1; // on

	ak7604->AE_MixerFL1=121; // 0dB
	ak7604->AE_MixerFL2=0; // Mute
	ak7604->AE_MixerFL3=0; // Mute
	ak7604->AE_MixerFR1=121; // 0dB 
	ak7604->AE_MixerFR2=0; // Mute
	ak7604->AE_MixerFR3=0;// Mute
	ak7604->AE_MixerRL1=121; // 0dB
	ak7604->AE_MixerRL2=0; // Mute
	ak7604->AE_MixerRL3=0; // Mute
	ak7604->AE_MixerRR1=121; // 0dB
	ak7604->AE_MixerRR2=0; // Mute
	ak7604->AE_MixerRR3=0; // Mute

	ak7604->AE_MICnMusic_Lch_Sel=0; // Analog IN Lch
	ak7604->AE_MICnMusic_Rch_Sel=0; // FL

	ak7604->AE_DZF_Enable_Sw=0; // AllOff
#endif
	ak7604->cramwrite = 0;
	ak7604->dsprun = 0;
#ifdef AK7604_IO_CONTROL
	init_ak7604_pd(ak7604);
#endif

	return 0;
}

static int ak7604_remove(struct snd_soc_codec *codec)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);

	akdbgprt("\t[AK7604] %s(%d)\n",__func__,__LINE__);

	ak7604_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if ( ak7604->pdn_gpio > 0 ) {
		gpio_set_value(ak7604->pdn_gpio, 0);
		msleep(1);
		gpio_free(ak7604->pdn_gpio);
	}

	return 0;
}

static int ak7604_suspend(struct snd_soc_codec *codec)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);	

	ak7604_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if ( ak7604->pdn_gpio > 0 ) {
		gpio_set_value(ak7604->pdn_gpio, 0);
		msleep(1);
	}
	
	return 0;
}

static int ak7604_resume(struct snd_soc_codec *codec)
{
	struct ak7604_priv *ak7604 = snd_soc_codec_get_drvdata(codec);	
	int i;

	for ( i = 0 ; i < ARRAY_SIZE(ak7604_reg) ; i++ ) {
		regmap_write(ak7604->regmap, ak7604_reg[i].reg, ak7604_reg[i].def);
	}

	ak7604_init_reg(codec);

	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_ak7604 = {
	.probe = ak7604_probe,
	.remove = ak7604_remove,
	.suspend =	ak7604_suspend,
	.resume =	ak7604_resume,

	.component_driver = {
		.controls = ak7604_snd_controls,
		.num_controls = ARRAY_SIZE(ak7604_snd_controls),
		.dapm_widgets = ak7604_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ak7604_dapm_widgets),
		.dapm_routes = ak7604_intercon,
		.num_dapm_routes = ARRAY_SIZE(ak7604_intercon),
	},
	.read = ak7604_read_register,
	.write = ak7604_write_register,
	
	.idle_bias_off = true,
	.set_bias_level = ak7604_set_bias_level,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ak7604);

static const struct regmap_config ak7604_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	
	.max_register = AK7604_MAX_REGISTER,
	.volatile_reg = ak7604_volatile,
	.writeable_reg = ak7604_writeable,
	.readable_reg = ak7604_readable,
	
	.reg_defaults = ak7604_reg,
	.num_reg_defaults = ARRAY_SIZE(ak7604_reg),
	.cache_type = REGCACHE_RBTREE,
};


static struct of_device_id ak7604_dt_ids[] = {
	{ .compatible = "akm,ak7604"},
	{   }
};
MODULE_DEVICE_TABLE(of, ak7604_dt_ids);

#ifdef AK7604_I2C_IF

static int ak7604_i2c_probe(struct i2c_client *i2c,
                            const struct i2c_device_id *id)
{
	struct ak7604_priv *ak7604;
	int ret=0;
	
	akdbgprt("\t[AK7604] %s(%d)\n",__func__,__LINE__);

	ak7604 = devm_kzalloc(&i2c->dev, sizeof(struct ak7604_priv), GFP_KERNEL);
	if (ak7604 == NULL) return -ENOMEM;

	ak7604->regmap = devm_regmap_init_i2c(i2c, &ak7604_regmap); 
	if (IS_ERR(ak7604->regmap)) {
		devm_kfree(&i2c->dev, ak7604);
		return PTR_ERR(ak7604->regmap);
	}

	i2c_set_clientdata(i2c, ak7604);

	ak7604->control_type = SND_SOC_I2C;
	ak7604->i2c = i2c;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak7604, &ak7604_dai[0], ARRAY_SIZE(ak7604_dai));
	if (ret < 0){
		devm_kfree(&i2c->dev, ak7604);
		akdbgprt("\t[AK7604 Error!] %s(%d)\n",__func__,__LINE__);
	}
	return ret;
}

static int ak7604_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id ak7604_i2c_id[] = {
	{ "ak7604", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak7604_i2c_id);

static struct i2c_driver ak7604_i2c_driver = {
	.driver = {
		.name = "ak7604",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak7604_dt_ids),
	},
	.probe = ak7604_i2c_probe,
	.remove = ak7604_i2c_remove,
	.id_table = ak7604_i2c_id,
};

#else

#if defined(SND_SOC_SPI)
static int ak7604_spi_probe(struct spi_device *spi)
{
	struct ak7604_priv *ak7604;
	int ret;

	akdbgprt("\t[AK7604] %s(%d)\n",__func__,__LINE__);

	ak7604 = devm_kzalloc(&spi->dev, sizeof(struct ak7604_priv),
			      GFP_KERNEL);
	if (ak7604 == NULL)
		return -ENOMEM;

	ak7604->regmap = devm_regmap_init_spi(spi, &ak7604_regmap);
	if (IS_ERR(ak7604->regmap)) {
		ret = PTR_ERR(ak7604->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	spi_set_drvdata(spi, ak7604);

	ak7604->control_type = SND_SOC_SPI;
	ak7604->spi = spi;

	ret = snd_soc_register_codec(&spi->dev,
			&soc_codec_dev_ak7604, &ak7604_dai[0], ARRAY_SIZE(ak7604_dai));

	if (ret != 0) {
		dev_err(&spi->dev, "Failed to register CODEC: %d\n", ret);
		return ret;
	}

	return 0;

}

static int  ak7604_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	return 0;
}

static struct spi_driver ak7604_spi_driver = {
	.driver = {
		.name = "ak7604",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ak7604_dt_ids),
	},
	.probe = ak7604_spi_probe,
	.remove = ak7604_spi_remove,
};
#endif//SND_SOC_SPI
#endif//AK7604_I2C_IF

static int __init ak7604_modinit(void)
{
	int ret = 0;

	akdbgprt("\t[AK7604] %s(%d)\n", __FUNCTION__,__LINE__);
#ifdef AK7604_I2C_IF
	ret = i2c_add_driver(&ak7604_i2c_driver);
	if ( ret != 0 ) {
		printk(/*KERN_ERR*/ "Failed to register AK7604 I2C driver: %d\n", ret);

	}
#else
#if defined(CONFIG_SND_SPI)
	ret = spi_register_driver(&ak7604_spi_driver);
	if ( ret != 0 ) {
		printk(/*KERN_ERR*/ "Failed to register AK7604 SPI driver: %d\n",  ret);

	}
#endif//CONFIG_SND_SPI
#endif//AK7604_I2C_IF

#ifdef AK7604_IO_CONTROL
	ret = misc_register(&ak7604_misc);
	if (ret < 0) {
		printk(/*KERN_ERR*/ "Failed to register AK7604 MISC driver: %d\n",  ret);
	}
#endif
	return ret;
}

module_init(ak7604_modinit);

static void __exit ak7604_exit(void)
{
#ifdef AK7604_I2C_IF
	i2c_del_driver(&ak7604_i2c_driver);
#else
#if defined(CONFIG_SND_SPI)
	spi_unregister_driver(&ak7604_spi_driver);
#endif//CONFIG_SND_SPI
#endif//AK7604_I2C_IF

#ifdef AK7604_IO_CONTROL
	misc_deregister(&ak7604_misc);
#endif

}
module_exit(ak7604_exit);

MODULE_AUTHOR("J.Wakasugi <wakasugi.jb@om.asahi-kasei.co.jp>");
MODULE_DESCRIPTION("ak7604 codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
