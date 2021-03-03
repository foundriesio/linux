// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_ISP_SETTINGS_H__
#define __TCC_ISP_SETTINGS_H__

#include "tcc-isp.h"

/*
 * To use ISP UART,
 * enable "USE_ISP_UART" define and pinctl properties of isp device node in
 * device tree
 *
 * TCC8059 does not have an ball out of ISP UART.
 * TCC8050/53 has an ball out of ISP UART. Refer to the GPIO pins below.
 * UART TX - GPIO_MB[0] / GPIO_MB[6] / GPIO_MB[12] / GPIO_MB[24]
 * UART RX - GPIO_MB[1] / GPIO_MB[7] / GPIO_MB[13] / GPIO_MB[25]
 * But, GPIO pins above is used for other purpose.
 * So, H/W modification is needed to use ISP UART on the TCC8050/53 EVB.
 * Refer to the TCS(CV8050C-606)
 */
//#define USE_ISP_UART

/* decompanding mode(input 12bits, output 20bits) */
const struct hdr_state setting_hdr0 = {
	.mode				= HDR_MODE_COMPANDING,

	.decompanding			= DCPD_CTL_DCPD_EN_ENABLE,
	.decompanding_curve_maxval	= 7,
	.decompanding_input_bit		= DCPD_CTL_IN_BIT_SEL_12BITS,
	.decompanding_output_bit	= DCPD_CTL_OUT_BIT_SEL_20BITS,

	.dcpd_cur_gain[0]		= 0x0002,
	.dcpd_cur_gain[1]		= 0x0040,
	.dcpd_cur_gain[2]		= 0x03ff,
	.dcpd_cur_gain[3]		= 0x03ff,
	.dcpd_cur_gain[4]		= 0x03ff,
	.dcpd_cur_gain[5]		= 0x03ff,
	.dcpd_cur_gain[6]		= 0x03ff,
	.dcpd_cur_gain[7]		= 0x03ff,

	.dcpd_cur_x_axis[0]		= 0x0200,
	.dcpd_cur_x_axis[1]		= 0x02f8,
	.dcpd_cur_x_axis[2]		= 0x03e8,
	.dcpd_cur_x_axis[3]		= 0x03ff,
	.dcpd_cur_x_axis[4]		= 0x03ff,
	.dcpd_cur_x_axis[5]		= 0x03ff,
	.dcpd_cur_x_axis[6]		= 0x03ff,
	.dcpd_cur_x_axis[7]		= 0x03ff,
};

/* setting values(isp and adaptive_data are from btree) */
const struct reg_setting setting_isp0[] = {
	/* TODO */
};

const struct reg_setting setting_adaptive_data0[] = {
	/* TODO */
};

/*
 * setting values below are used to set ISP0 ~ 3.
 * If the setting value is different for each ISP,
 * make setting_xxx structure and change below structure's member initialization
 */
const struct hdr_state *hdr_value[4] = {
	&setting_hdr0,	/* FOR ISP0 */
	&setting_hdr0,	/* FOR ISP1 */
	&setting_hdr0,	/* FOR ISP2 */
	&setting_hdr0,	/* FOR ISP3 */
};

const struct isp_tune tune_value[4] = {
	/* FOR ISP0 */
	{
		setting_isp0,
		ARRAY_SIZE(setting_isp0),
		setting_adaptive_data0,
		ARRAY_SIZE(setting_adaptive_data0),
	},
	/* FOR ISP1 */
	{
		setting_isp0,
		ARRAY_SIZE(setting_isp0),
		setting_adaptive_data0,
		ARRAY_SIZE(setting_adaptive_data0),
	},
	/* FOR ISP2 */
	{
		setting_isp0,
		ARRAY_SIZE(setting_isp0),
		setting_adaptive_data0,
		ARRAY_SIZE(setting_adaptive_data0),
	},
	/* FOR ISP3 */
	{
		setting_isp0,
		ARRAY_SIZE(setting_isp0),
		setting_adaptive_data0,
		ARRAY_SIZE(setting_adaptive_data0),
	},
};

#endif
