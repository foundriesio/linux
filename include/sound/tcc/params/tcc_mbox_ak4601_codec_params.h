/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#ifndef _TCC_MBOX_AK4601_CODEC_PARAMS_H_
#define _TCC_MBOX_AK4601_CODEC_PARAMS_H_

/** Codec command **/
/* unsigned int cmd[0] = usage | cmd_type | tx instance num | msg_size (3) ; 8bit | 8bit | 8bit | 8bit */
/* unsigned int cmd[1] = integer mapped control command string */
/* unsigned int cmd[2] = interger mapped value , if using MBOX_AUDIO_USAGE_REQUEST, do not use it */
/*                       reply value is set here */


#define AUDIO_MBOX_CODEC_GET_MESSAGE_SIZE        1
#define AUDIO_MBOX_CODEC_SET_MESSAGE_SIZE        2


#define CODEC_COMMAND_MAPPER(cvalue, cname, ctype) { .value = cvalue, .name = cname, .type = ctype}

struct codec_command_map {
	unsigned int value;
	const char * name;
	unsigned int type;
};

//for ak4601 command key map
enum ak4601_command_id {
	AK4601_VIRTUAL_001_MIC_INPUT_VOLUME_L = 0            ,
	AK4601_VIRTUAL_002_MIC_INPUT_VOLUME_R                ,
	AK4601_VIRTUAL_003_ADC1_DIGITAL_VOLUME_L             ,
	AK4601_VIRTUAL_004_ADC1_DIGITAL_VOLUME_R             ,
	AK4601_VIRTUAL_005_ADC2_DIGITAL_VOLUME_L             ,
	AK4601_VIRTUAL_006_ADC2_DIGITAL_VOLUME_R             ,
	AK4601_VIRTUAL_007_ADCM_DIGITAL_VOLUME               ,
	AK4601_VIRTUAL_008_DAC1_DIGITAL_VOLUME_L             ,
	AK4601_VIRTUAL_009_DAC1_DIGITAL_VOLUME_R             ,
	AK4601_VIRTUAL_010_DAC2_DIGITAL_VOLUME_L             ,
	AK4601_VIRTUAL_011_DAC2_DIGITAL_VOLUME_R             ,
	AK4601_VIRTUAL_012_DAC3_DIGITAL_VOLUME_L             ,
	AK4601_VIRTUAL_013_DAC3_DIGITAL_VOLUME_R             ,
	AK4601_VIRTUAL_014_ADC1_MUTE                         ,
	AK4601_VIRTUAL_015_ADC2_MUTE                         ,
	AK4601_VIRTUAL_016_ADCM_MUTE                         ,
	AK4601_VIRTUAL_017_DAC1_MUTE                         ,
	AK4601_VIRTUAL_018_DAC2_MUTE                         ,
	AK4601_VIRTUAL_019_DAC3_MUTE                         ,
	AK4601_VIRTUAL_020_ADC1_INPUT_VOLTAGE_SETTING_L      ,
	AK4601_VIRTUAL_021_ADC1_INPUT_VOLTAGE_SETTING_R      ,
	AK4601_VIRTUAL_022_ADC2_INPUT_VOLTAGE_SETTING_L      ,
	AK4601_VIRTUAL_023_ADC2_INPUT_VOLTAGE_SETTING_R      ,
	AK4601_VIRTUAL_024_ADCM_INPUT_VOLTAGE_SETTING        ,
	AK4601_VIRTUAL_025_SDOUT1_OUTPUT_ENABLE	             ,
	AK4601_VIRTUAL_026_SDOUT2_OUTPUT_ENABLE              ,
	AK4601_VIRTUAL_027_SDOUT3_OUTPUT_ENABLE              ,
	AK4601_VIRTUAL_028_SDOUT1_SOURCE_SELECTOR            ,
	AK4601_VIRTUAL_029_MIXER_B_CH_1_SOURCE_SELECTOR      ,
	AK4601_VIRTUAL_030_ADC2_MUX                          ,
	AK4601_VIRTUAL_INDEX_COUNT                           ,
};
extern const struct codec_command_map codec_command_key_map[];

//for ak4601 value key map
enum ak4601_value_on_off {
	AK4601_VIRTUAL_OFF = 0              ,
	AK4601_VIRTUAL_ON  = 1              ,
	AK4601_VIRTUAL_VALUE_BOOL_COUNT 	,
};
extern const struct codec_command_map codec_value_on_off_map[];

enum ak4601_value_input_voltage {
	AK4601_VIRTUAL_2_3_Vpp = 0          ,
	AK4601_VIRTUAL_2_83_Vpp = 1         ,
	AK4601_VIRTUAL_VALUE_VOLTAGE_COUNT  ,
};
extern const struct codec_command_map codec_value_input_voltage_map[];

enum ak4601_value_adc_mux {
	AK4601_VIRTUAL_AIN2 = 0	            ,
	AK4601_VIRTUAL_AIN3                 ,
	AK4601_VIRTUAL_AIN4                 ,
	AK4601_VIRTUAL_AIN5                 ,
	AK4601_VIRTUAL_VALUE_MUX_COUNT 	    ,
};
extern const struct codec_command_map codec_value_adc_mux_map[];

enum ak4601_value_source_selector {
	AK4601_VIRTUAL_ALL0 = 0             ,
	AK4601_VIRTUAL_SDIN1                ,
	AK4601_VIRTUAL_SDIN1B               ,
	AK4601_VIRTUAL_SDIN1C               ,
	AK4601_VIRTUAL_SDIN1D               ,
	AK4601_VIRTUAL_SDIN1E               ,
	AK4601_VIRTUAL_SDIN1F               ,
	AK4601_VIRTUAL_SDIN1G               ,
	AK4601_VIRTUAL_SDIN1H               ,
	AK4601_VIRTUAL_SDIN2                ,
	AK4601_VIRTUAL_SDIN3                ,
	AK4601_VIRTUAL_VOL1                 ,
	AK4601_VIRTUAL_VOL2                 ,
	AK4601_VIRTUAL_VOL3                 ,
	AK4601_VIRTUAL_ADC1                 ,
	AK4601_VIRTUAL_ADC2                 ,
	AK4601_VIRTUAL_ADCM                 ,
	AK4601_VIRTUAL_MIXER_A              ,
	AK4601_VIRTUAL_MIXER_B              ,
	AK4601_VIRTUAL_VALUE_SOURCE_COUNT   ,
};
extern const struct codec_command_map codec_value_source_selector_map [];

enum ak4601_command_value_type {
    AK4601_VIRTUAL_VALUE_TYPE_VOLUME  = 0    ,
	AK4601_VIRTUAL_VALUE_TYPE_BOOL           ,
	AK4601_VIRTUAL_VALUE_TYPE_ENUM_MUX       ,
	AK4601_VIRTUAL_VALUE_TYPE_ENUM_VOLTAGE   ,
    AK4601_VIRTUAL_VALUE_TYPE_ENUM_SOURCE	,
};

//backup data with extern variable
//TODO : we can use only int array..
struct ak4601_codec_data {
    int parameter;
    int value;
};

extern struct ak4601_codec_data g_ak4601_codec_data[];

#endif//_TCC_MBOX_AK4601_CODEC_PARAMS_H_
