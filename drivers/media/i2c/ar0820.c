/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <video/tcc/vioc_vin.h>

#define LOG_TAG				"VSRC:AR0820"

#define loge(fmt, ...) \
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...) \
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...) \
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...) \
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define DEFAULT_WIDTH			(2560)
#define DEFAULT_HEIGHT			(1600)

#define	DEFAULT_FRAMERATE		(30)

struct frame_size {
	u32 width;
	u32 height;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct ar0820 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;
	struct v4l2_ctrl_handler	hdl;
	int				framerate;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence ar0820_reg_init[] = {
	{0x301A, 0x0059, 20 * 1000}, /* RESET_REGISTER */
	{0x301A, 0x0058, 0}, /* RESET_REGISTER */

	{0x301A, 0x0058, 0}, /* RESET_REGISTER */

	/* Rev2 Sequencer */
	{0x2512, 0x8000, 0}, /* SEQ_CTRL_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFF07, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xFFFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3001, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3010, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3006, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3020, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3008, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xB031, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xA824, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x003C, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x001F, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xB2F9, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x006F, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0078, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x005C, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x106F, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xC013, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x006E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0079, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x007B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xC806, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x106E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0017, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0013, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x004B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0002, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x90F2, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x90FF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xD034, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1032, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0000, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0033, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x00D1, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x092E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1333, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x123D, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x045B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x11BB, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x133A, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1013, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1017, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1015, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1099, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x14DB, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x00DD, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3088, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3084, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x2003, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x11F9, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x02DA, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xD80C, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x2006, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x017A, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x01F0, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x14F0, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x008B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x10F8, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x118B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x00ED, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x00E4, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0072, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x203B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x8828, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x2003, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1064, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0063, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1072, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x003E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xC00A, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x05CD, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x006E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x100E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0019, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0015, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x16EE, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0071, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x10BE, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1063, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1671, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1095, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1019, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3088, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3084, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x2003, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x018B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x110B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x117B, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x00E4, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0072, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x20C4, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1064, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x107A, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1072, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3041, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xD800, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x881A, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x100C, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x000E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x100D, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3081, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x10CB, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1052, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x0038, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xC200, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xCA00, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xD230, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x8200, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x11AE, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xB041, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xD000, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x106D, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x101F, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x100E, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x100A, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3042, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3086, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x102F, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x3090, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x9010, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0xB000, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x30A0, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x1016, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */
	{0x2510, 0x7FFF, 0}, /* SEQ_DATA_PORT */

	/* Common */
	{0x3508, 0xAA80, 0}, /* RESERVED_MFR_3508 */
	{0x350A, 0xC5C0, 0}, /* RESERVED_MFR_350A */
	{0x350C, 0xC8C4, 0}, /* RESERVED_MFR_350C */
	{0x350E, 0x8C8C, 0}, /* RESERVED_MFR_350E */
	{0x3510, 0x8C88, 0}, /* RESERVED_MFR_3510 */
	{0x3512, 0x8C8C, 0}, /* RESERVED_MFR_3512 */
	{0x3514, 0xA0A0, 0}, /* RESERVED_MFR_3514 */
	{0x3518, 0x0040, 0}, /* RESERVED_MFR_3518 */
	{0x351A, 0x8600, 0}, /* RESERVED_MFR_351A */
	{0x351E, 0x0E40, 0}, /* RESERVED_MFR_351E */
	{0x3506, 0x004A, 0}, /* RESERVED_MFR_3506 */
	{0x3520, 0x0E19, 0}, /* RESERVED_MFR_3520 */
	{0x3522, 0x7F7F, 0}, /* RESERVED_MFR_3522 */
	{0x3524, 0x7F7F, 0}, /* RESERVED_MFR_3524 */
	{0x3526, 0x7F7F, 0}, /* RESERVED_MFR_3526 */
	{0x3528, 0x7F7F, 0}, /* RESERVED_MFR_3528 */
	{0x30FE, 0x00A8, 0}, /* RESERVED_MFR_30FE */
	{0x3584, 0x0000, 0}, /* RESERVED_MFR_3584 */
	{0x3540, 0x8308, 0}, /* RESERVED_MFR_3540 */
	{0x354C, 0x0031, 0}, /* RESERVED_MFR_354C */
	{0x354E, 0x535C, 0}, /* RESERVED_MFR_354E */
	{0x3550, 0x5C7F, 0}, /* RESERVED_MFR_3550 */
	{0x3552, 0x0011, 0}, /* RESERVED_MFR_3552 */
	{0x3370, 0x0111, 0}, /* DBLC_CONTROL */
	{0x337A, 0x0F50, 0}, /* RESERVED_MFR_337A */
	{0x337E, 0xFFF8, 0}, /* DBLC_OFFSET0 */
	{0x3110, 0x0011, 0}, /* HDR_CONTROL0 */
	{0x3100, 0x4000, 0}, /* DLO_CONTROL0 */
	{0x3364, 0x0173, 0}, /* DCG_TRIM */
	{0x3180, 0x0021, 0}, /* RESERVED_MFR_3180 */
	{0x3E4C, 0x0404, 0}, /* RESERVED_MFR_3E4C */
	{0x3E52, 0x0060, 0}, /* RESERVED_MFR_3E52 */
	{0x3180, 0x0021, 0}, /* RESERVED_MFR_3180 */
	{0x37A0, 0x0001, 0}, /* COARSE_INTEGRATION_AD_TIME */
	{0x37A4, 0x0000, 0}, /* COARSE_INTEGRATION_AD_TIME2 */
	{0x37A8, 0x0000, 0}, /* COARSE_INTEGRATION_AD_TIME3 */
	{0x37AC, 0x0000, 0}, /* COARSE_INTEGRATION_AD_TIME4 */

	{0x3E94, 0x2F0F, 0}, /* RESERVED_MFR_3E94 */
	{0x3372, 0xF50F, 0}, /* DBLC_FS0_CONTROL (No Bin) */

	/* PLL Clocks - Pix 156Mhz Op_78Mhz */
	{0x302A, 0x0003, 0}, /* VT_PIX_CLK_DIV */
	{0x302C, 0x0701, 0}, /* VT_SYS_CLK_DIV */
	{0x302E, 0x0009, 0}, /* PRE_PLL_CLK_DIV */
	{0x3030, 0x009C, 0}, /* PLL_MULTIPLIER */
	{0x3036, 0x0006, 0}, /* OP_WORD_CLK_DIV */
	{0x3038, 0x0001, 0}, /* OP_SYS_CLK_DIV */
	{0x303A, 0x0085, 0}, /* PLL_MULTIPLIER_ANA */
	{0x303C, 0x0003, 0}, /* PRE_PLL_CLK_DIV_ANA */

	/* MIPI Timing - Pix 156Mhz Op_78Mhz */
	{0x31B0, 0x0047, 0}, /* FRAME_PREAMBLE */
	{0x31B2, 0x0026, 0}, /* LINE_PREAMBLE */
	{0x31B4, 0x5187, 0}, /* RESERVED_MFR_31B4 */
	{0x31B6, 0x5248, 0}, /* RESERVED_MFR_31B6 */
	{0x31B8, 0x70CA, 0}, /* RESERVED_MFR_31B8 */
	{0x31BA, 0x028A, 0}, /* RESERVED_MFR_31BA */
	{0x31BC, 0x8A88, 0}, /* MIPI_TIMING_4 */
	{0x31BE, 0x0023, 0}, /* MIPI_CONFIG_STATUS */

	/* Resolution 2560 x 1600 */
	{0x3002, 0x011C, 0}, /* Y_ADDR_START_ */
	{0x3004, 0x0284, 0}, /* X_ADDR_START_ */
	{0x3006, 0x075B, 0}, /* Y_ADDR_END_ */
	{0x3008, 0x0C83, 0}, /* X_ADDR_END_ */
	{0x32FC, 0x0000, 0}, /* READ_MODE2 */
	{0x37E0, 0x8421, 0}, /* ROW_TX_RO_ENABLE */
	{0x37E2, 0x8421, 0}, /* ROW_TX_RO_ENABLE_CB */
	{0x323C, 0x8421, 0}, /* ROW_TX_ENABLE */
	{0x323E, 0x8421, 0}, /* ROW_TX_ENABLE_CB */
	{0x3040, 0x0001, 0}, /* READ_MODE */
	{0x301D, 0x0000, 0}, /* IMAGE_ORIENTATION_ */

	/* SDR */
	{0x3082, 0x0000, 0}, /* 0x3082 = 1 exposures */
	{0x30BA, 0x1110, 0}, /* Num_exp_max = 1 exposure, decompress dither */
	{0x3012, 0x0045, 0}, /* coarse_integration_time */
	{0x3362, 0x00FF, 0}, /* DCG =high */
	{0x3366, 0x0000, 0}, /* Coarse Gain =1x */
	{0x336A, 0x0000, 0}, /* Fine Gain =1x */
	{0x300C, 0x1158, 0}, /* line_length_pclk */
	{0x300A, 0x0920, 0}, /* frame_length_lines */

	/* Color */
	{0x3056, 0x0080, 0}, /* Green Gain1 */
	{0x3058, 0x00e0, 0}, /* Blue Gain */
	{0x305A, 0x00e0, 0}, /* Red Gain */
	{0x305C, 0x0080, 0}, /* Green Gain2 */

	/* OC */
	{0x33C0, 0x2000, 0}, /* OC_LUT_00 */
	{0x33C2, 0x4000, 0}, /* OC_LUT_01 */
	{0x33C4, 0x6000, 0}, /* OC_LUT_02 */
	{0x33C6, 0x8000, 0}, /* OC_LUT_03 */
	{0x33C8, 0xA000, 0}, /* OC_LUT_04 */
	{0x33CA, 0xC000, 0}, /* OC_LUT_05 */
	{0x33CC, 0xE000, 0}, /* OC_LUT_06 */
	{0x33CE, 0xF000, 0}, /* OC_LUT_07 */
	{0x33D0, 0xF400, 0}, /* OC_LUT_08 */
	{0x33D2, 0xF800, 0}, /* OC_LUT_09 */
	{0x33D4, 0xFC00, 0}, /* OC_LUT_10 */
	{0x33D6, 0xFFC0, 0}, /* OC_LUT_11 */
	{0x33D8, 0xFFC0, 0}, /* OC_LUT_12 */
	{0x33DA, 0xFFC0, 0}, /* OC_LUT_13 */
	{0x33DC, 0xFFC0, 0}, /* OC_LUT_14 */
	{0x33DE, 0xFFC0, 0}, /* OC_LUT_15 */

	{0x37A0, 0x0001, 0}, /* COARSE_INTEGRATION_AD_TIME */
	{0x37A4, 0x0001, 0}, /* COARSE_INTEGRATION_AD_TIME2 */
	{0x37A8, 0x0001, 0}, /* COARSE_INTEGRATION_AD_TIME3 */
	{0x37AC, 0x0000, 0}, /* COARSE_INTEGRATION_AD_TIME4 */

	/* DLO Tuning */
	{0x3280, 0x0FA0, 0}, /* T1_BARRIER_C0 */
	{0x3282, 0x0FA0, 0}, /* T1_BARRIER_C1 */
	{0x3284, 0x0FA0, 0}, /* T1_BARRIER_C2 */
	{0x3286, 0x0FA0, 0}, /* T1_BARRIER_C3 */
	{0x3288, 0x0FA0, 0}, /* T2_BARRIER_C0 */
	{0x328A, 0x0FA0, 0}, /* T2_BARRIER_C1 */
	{0x328C, 0x0FA0, 0}, /* T2_BARRIER_C2 */
	{0x328E, 0x0FA0, 0}, /* T2_BARRIER_C3 */
	{0x3290, 0x0FA0, 0}, /* T3_BARRIER_C0 */
	{0x3292, 0x0FA0, 0}, /* T3_BARRIER_C1 */
	{0x3294, 0x0FA0, 0}, /* T3_BARRIER_C2 */
	{0x3296, 0x0FA0, 0}, /* T3_BARRIER_C3 */
	{0x3298, 0x0FA0, 0}, /* T4_BARRIER_C0 */
	{0x329A, 0x0FA0, 0}, /* T4_BARRIER_C1 */
	{0x329C, 0x0FA0, 0}, /* T4_BARRIER_C2 */
	{0x329E, 0x0FA0, 0}, /* T4_BARRIER_C3 */
	{0x3100, 0x4000, 0}, /* DLO_CONTROL0 */
	{0x3102, 0x6064, 0}, /* DLO_CONTROL1 */
	{0x3104, 0x6064, 0}, /* DLO_CONTROL2 */
	{0x3106, 0x6064, 0}, /* DLO_CONTROL3 */
	{0x3108, 0x07D0, 0}, /* DLO_CONTROL4 */
	{0x31AE, 0x0204, 0}, /* SERIAL_FORMAT */
	{0x31AC, 0x180C, 0}, /* DATA_FORMAT_BITS */
	{0x301A, 0x0058, 10 * 1000}, /* RESET_REGISTER */
	{0x3064, 0x0000, 0}, /* SMIA_TEST */
	{0x301A, 0x005C, 0}, /* RESET_REGISTER */
	{0x301A, 0x0058, 10 * 1000}, /* RESET_REGISTER */
	{0x3064, 0x0000, 0}, /* SMIA_TEST */
	{0x301A, 0x005C, 0}, /* RESET_REGISTER */
	{0x301A, 0x005C, 0}, /* RESET_REGISTER */
};

static const struct regmap_config ar0820_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size ar0820_framesizes[] = {
	{	2560,	1600	},
};

static u32 ar0820_framerates[] = {
	30,
};

static void ar0820_init_format(struct ar0820 *dev)
{
	dev->fmt.width = DEFAULT_WIDTH;
	dev->fmt.height	= DEFAULT_HEIGHT;
	dev->fmt.code = MEDIA_BUS_FMT_SGRBG12_1X12;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}

/*
 * Helper functions for reflection
 */
static inline struct ar0820 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0820, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int ar0820_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int			ret	= 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_DO_WHITE_BALANCE:
	default:
		loge("V4L2_CID_BRIGHTNESS is not implemented yet.\n");
		ret = -EINVAL;
	}

	return ret;
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int ar0820_init(struct v4l2_subdev *sd, u32 enable)
{
	struct ar0820		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		/* enable ar0820 */
		ret = regmap_multi_reg_write(dev->regmap,
				ar0820_reg_init,
				ARRAY_SIZE(ar0820_reg_init));
		if (ret) {
			/* err status */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* disable ar0820 */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int ar0820_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0820		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);


	mutex_unlock(&dev->lock);
	return ret;
}

static int ar0820_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0820		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	interval->pad = 0;
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->framerate;

	return 0;
}

static int ar0820_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0820		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	/* set framerate with i2c setting if supported */

	dev->framerate = interval->interval.denominator;

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int ar0820_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(ar0820_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &ar0820_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int ar0820_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(ar0820_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = ar0820_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}
static int ar0820_get_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct ar0820		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int ar0820_set_fmt(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	struct ar0820		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&dev->fmt, (const void *)&format->format,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static const struct v4l2_ctrl_ops ar0820_ctrl_ops = {
	.s_ctrl			= ar0820_s_ctrl,
};

static const struct v4l2_subdev_core_ops ar0820_core_ops = {
	.init			= ar0820_init,
};

static const struct v4l2_subdev_video_ops ar0820_video_ops = {
	.s_stream		= ar0820_s_stream,
	.g_frame_interval	= ar0820_g_frame_interval,
	.s_frame_interval	= ar0820_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops ar0820_pad_ops = {
	.enum_frame_size	= ar0820_enum_frame_size,
	.enum_frame_interval	= ar0820_enum_frame_interval,
	.get_fmt		= ar0820_get_fmt,
	.set_fmt		= ar0820_set_fmt,
};

static const struct v4l2_subdev_ops ar0820_ops = {
	.core			= &ar0820_core_ops,
	.video			= &ar0820_video_ops,
	.pad			= &ar0820_pad_ops,
};

struct ar0820 ar0820_data = {
};

static const struct i2c_device_id ar0820_id[] = {
	{ "ar0820", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0820_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ar0820_of_match[] = {
	{
		.compatible	= "onnn,ar0820",
		.data		= &ar0820_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ar0820_of_match);
#endif

int ar0820_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ar0820			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct ar0820), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(ar0820_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &ar0820_ops);

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &ar0820_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&ar0820_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	/* register a v4l2 sub device */
	ret = v4l2_async_register_subdev(&dev->sd);
	if (ret)
		loge("Failed to register subdevice\n");
	else
		logi("%s is registered as a v4l2 sub device.\n", dev->sd.name);

	/* init framerate */
	dev->framerate = DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &ar0820_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	ar0820_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int ar0820_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct ar0820		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver ar0820_driver = {
	.probe		= ar0820_probe,
	.remove		= ar0820_remove,
	.driver		= {
		.name		= "ar0820",
		.of_match_table	= of_match_ptr(ar0820_of_match),
	},
	.id_table	= ar0820_id,
};

module_i2c_driver(ar0820_driver);
