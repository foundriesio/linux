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

#define LOG_TAG			"VSRC:ar0147"

#define loge(fmt, ...)		\
		pr_err("[ERROR][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)		\
		pr_warn("[WARN][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)		\
		pr_debug("[DEBUG][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)		\
		pr_info("[INFO][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct ar0147 {
	struct v4l2_subdev		sd;
	struct v4l2_mbus_framefmt	fmt;
	struct v4l2_ctrl_handler	hdl;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence ar0147_reg_init[] = {
	/* 1296x816 30fps parallel 12bit */
	{0x301A, 0x10D8, 20*1000},	/* RESET_REGISTER */
	{0x3500, 0x0100, 1*1000},	/* DAC_LD_0_1 */
	{0x30B0, 0x980E, 0},		/* DIGITAL_TEST */
	{0x3C08, 0x0000, 0},		/* CONFIGURE_BUFFERS2 */
	{0x3C0C, 0x0518, 0},		/* DELAY_BUFFER_LLPCK_RD_WR_OVERLAP */
	{0x3092, 0x1A24, 0},		/* ROW_NOISE_CONTROL */
	{0x30B4, 0x0047, 0},		/* TEMPSENS0_CTRL_REG */
	{0x3372, 0x710F, 0},		/* DBLC_FS0_CONTROL */
	{0x337A, 0x0E10, 0},		/* DBLC_SCALE0 */
	{0x33EE, 0x0344, 0},		/* TEST_CTRL */
	{0x350C, 0x035A, 0},		/* DAC_LD_12_13 */
	{0x350E, 0x0514, 0},		/* DAC_LD_14_15 */
	{0x3518, 0x14FE, 0},		/* DAC_LD_24_25 */
	{0x351A, 0x6000, 0},		/* DAC_LD_26_27 */
	{0x3520, 0x08CC, 0},		/* DAC_LD_32_33 */
	{0x3522, 0xCC08, 0},		/* DAC_LD_34_35 */
	{0x3524, 0x0C00, 0},		/* DAC_LD_36_37 */
	{0x3526, 0x0F00, 0},		/* DAC_LD_38_39 */
	{0x3528, 0xEEEE, 0},		/* DAC_LD_40_41 */
	{0x352A, 0x089F, 0},		/* DAC_LD_42_43 */
	{0x352C, 0x0012, 0},		/* DAC_LD_44_45 */
	{0x352E, 0x00EE, 0},		/* DAC_LD_46_47 */
	{0x3530, 0xEE00, 0},		/* DAC_LD_48_49 */
	{0x3536, 0xFF20, 0},		/* DAC_LD_54_55 */
	{0x3538, 0x3CFF, 0},		/* DAC_LD_56_57 */
	{0x353A, 0x9000, 0},		/* DAC_LD_58_59 */
	{0x353C, 0x7F00, 0},		/* DAC_LD_60_61 */
	{0x3540, 0xC62C, 0},		/* DAC_LD_64_65 */
	{0x3542, 0x4B4B, 0},		/* DAC_LD_66_67 */
	{0x3544, 0x3C46, 0},		/* DAC_LD_68_69 */
	{0x3546, 0x5A5A, 0},		/* DAC_LD_70_71 */
	{0x3548, 0x6400, 0},		/* DAC_LD_72_73 */
	{0x354A, 0x007F, 0},		/* DAC_LD_74_75 */
	{0x3556, 0x1010, 0},		/* DAC_LD_86_87 */
	{0x3566, 0x7328, 0},		/* DAC_LD_102_103 */
	{0x3F90, 0x0800, 0},		/* TEMPVSENS0_TMG_CTRL */
	{0x3510, 0x011F, 0},		/* DAC_LD_16_17 */
	{0x353E, 0x801F, 0},		/* DAC_LD_62_63 */
	{0x3F9A, 0x0001, 0},		/* TEMPVSENS0_BOOST_SAMP_CTRL */
	{0x3116, 0x0001, 0},		/* HDR_CONTROL3 */
	{0x3102, 0x60A0, 0},		/* DLO_CONTROL1 */
	{0x3104, 0x60A0, 0},		/* DLO_CONTROL2 */
	{0x3106, 0x60A0, 0},		/* DLO_CONTROL3 */
	{0x3362, 0x0000, 0},		/* DC_GAIN */
	{0x3366, 0xCCCC, 0},		/* ANALOG_GAIN */
	{0x31E0, 0x0003, 0},		/* PIX_DEF_ID */
	{0x30C6, 0x0214, 0},		/* TEMPSENS0_CALIB1 */
	{0x30C8, 0x01B8, 0},		/* TEMPSENS0_CALIB2 */
	{0x3F94, 0xFF80, 0},		/* TEMPVSENS0_FLAG_CTRL */
	{0x3F74, 0x40E8, 0},		/* GCF_TRIM_1 */
	{0x3552, 0x0EB0, 0},		/* DAC_LD_82_83 */
	{0x2512, 0x8000, 0},		/* SEQ_CTRL_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3350, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2004, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1420, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1578, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x087B, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x24FF, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x24FF, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x24EA, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2410, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2224, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xD813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0214, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0024, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFF24, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFF24, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xEA23, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2464, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7A24, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0405, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2C40, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0AFF, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0A78, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3851, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2A54, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1440, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0804, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0801, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0408, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2652, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xC810, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1611, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8111, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8910, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5612, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x020D, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0903, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1402, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x15A8, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1388, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0938, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1199, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x11D9, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x091E, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1214, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x10D6, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x11DD, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x11D9, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1441, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0904, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1056, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0811, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xDB09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0311, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFB11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xBB12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1A12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1008, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1250, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0B10, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7610, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xE614, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6109, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0612, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1C14, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1215, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xC813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xC808, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1066, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x090B, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1588, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1388, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0913, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0C14, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0310, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xE611, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFB12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFF11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFB14, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4109, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6609, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1211, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xBB12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6312, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6014, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1811, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xB812, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0010, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2610, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0013, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0030, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5342, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1100, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1002, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1016, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1101, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1109, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1056, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0D09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0314, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0214, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4309, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0514, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0115, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xC813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xC809, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1A11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0815, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8809, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1B11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0B12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1409, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0112, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1010, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xD612, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5D11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5910, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5609, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0311, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5B08, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1441, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1440, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x090C, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x117B, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x113B, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x121A, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1250, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x10F6, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x10E6, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1460, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x15A8, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x13A8, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1240, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1260, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0924, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1588, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1066, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0B08, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1388, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0925, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0C09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0214, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0710, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xE612, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7F11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7B10, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6609, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0614, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4109, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0114, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0D11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3B12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6312, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6014, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1811, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3812, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0010, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2610, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0013, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0043, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7A06, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0507, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x410E, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0237, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x502C, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4414, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4000, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1508, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0408, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0104, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0826, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5508, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x13C8, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1002, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1016, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1181, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1189, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1056, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0902, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0D09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0314, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0215, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA814, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0309, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0614, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0209, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1F15, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8809, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0B11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x9911, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xD909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1E12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1409, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0312, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xDD11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xD909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0114, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0711, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xDB09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0311, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFB11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xBB12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1A12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0112, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x500B, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1076, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1066, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1460, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x15C8, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1240, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1260, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x13C8, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0956, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1588, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0C14, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0511, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFB12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFF11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFB09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1911, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xBB12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6312, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6014, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1811, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xB812, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0010, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2610, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0013, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0030, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5345, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1444, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1002, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1016, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1101, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1109, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1056, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1210, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0D09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0314, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0614, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4709, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0514, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4409, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0115, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x9813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x9809, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1A11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0815, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8809, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1B11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0B12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1409, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0112, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0112, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5D11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5909, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0511, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5B09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1311, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7B11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3B12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1A12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0112, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x5010, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7610, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6614, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6409, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0115, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA813, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA812, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6009, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x8809, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x020B, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0901, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1388, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0925, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0C09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0214, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x4409, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0912, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6212, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7F11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7B09, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1C11, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3B12, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6312, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x6014, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0015, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x1811, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x3812, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xA012, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0010, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2610, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0013, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0011, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0008, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x7A06, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0508, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x070E, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x0237, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x502C, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFE32, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0xFE06, 0},		/* SEQ_DATA_PORT */
	{0x2510, 0x2C2C, 0},		/* SEQ_DATA_PORT */
	{0x32E6, 0x009A, 0},		/* MIN_SUBROW */
	{0x322E, 0x258C, 0},		/* CLKS_PER_SAMPLE */
	{0x32D0, 0x3A02, 0},		/* SHUT_RST */
	{0x32D2, 0x3508, 0},		/* SHUT_TX */
	{0x32D4, 0x3702, 0},		/* SHUT_DCG */
	{0x32D6, 0x3C04, 0},		/* SHUT_RST_BOOST */
	{0x32DC, 0x370A, 0},		/* SHUT_TX_BOOST */
	{0x32EA, 0x3CA8, 0},		/* SHUT_CTRL */
	{0x351E, 0x0000, 0},		/* DAC_LD_30_31 */
	{0x3510, 0x811F, 0},		/* DAC_LD_16_17 */
	{0x1010, 0x0155, 0},		/* FINE_INTEGRATION_TIME4_MIN */
	{0x3236, 0x00B2, 0},		/* FINE_CORRECTION4 */
	{0x32EA, 0x3C0E, 0},		/* SHUT_CTRL */
	{0x32EC, 0x7151, 0},		/* SHUT_CTRL2 */
	{0x3116, 0x0001, 0},		/* HDR_CONTROL3 */
	{0x33E2, 0x0000, 0},		/* SAMPLE_CTRL */
	{0x3088, 0x0400, 0},		/* LFM_CTRL */
	{0x322A, 0x0039, 0},		/* FINE_INTEGRATION_CTRL */
	{0x3238, 0x0333, 0},		/* EXPOSURE_RATIO */
	{0x3C06, 0x141C, 0},		/* CONFIGURE_BUFFERS1 */
	{0x3C08, 0x2114, 0},		/* CONFIGURE_BUFFERS2 */
	{0x3088, 0x0400, 0},		/* LFM_CTRL */
	{0x32EC, 0x7107, 0},		/* SHUT_CTRL2 */
	{0x33E2, 0x0530, 0},		/* SAMPLE_CTRL */
	{0x322A, 0x0939, 0},		/* FINE_INTEGRATION_CTRL */
	{0x1008, 0x0271, 0},		/* FINE_INTEGRATION_TIME_MIN */
	{0x1010, 0x0129, 0},		/* FINE_INTEGRATION_TIME4_MIN */
	{0x3230, 0x020F, 0},		/* FINE_CORRECTION */
	{0x3236, 0x00C7, 0},		/* FINE_CORRECTION4 */
	{0x30FE, 0x0040, 0},		/* NOISE_PEDESTAL */
	{0x3180, 0x0188, 0},		/* DELTA_DK_CONTROL */
	{0x3108, 0x0CB1, 0},		/* DLO_CONTROL4 */
	{0x3280, 0x0CB2, 0},		/* T1_BARRIER_C0 */
	{0x3282, 0x0CB2, 0},		/* T1_BARRIER_C1 */
	{0x3284, 0x0CB2, 0},		/* T1_BARRIER_C2 */
	{0x3286, 0x0CB2, 0},		/* T1_BARRIER_C3 */
	{0x3288, 0x0226, 0},		/* T2_BARRIER_C0 */
	{0x328A, 0x0226, 0},		/* T2_BARRIER_C1 */
	{0x328C, 0x0226, 0},		/* T2_BARRIER_C2 */
	{0x328E, 0x0226, 0},		/* T2_BARRIER_C3 */
	{0x3290, 0x0CB2, 0},		/* T3_BARRIER_C0 */
	{0x3292, 0x0CB2, 0},		/* T3_BARRIER_C1 */
	{0x3294, 0x0CB2, 0},		/* T3_BARRIER_C2 */
	{0x3296, 0x0CB2, 0},		/* T3_BARRIER_C3 */
	{0x3298, 0x0ED8, 0},		/* T4_BARRIER_C0 */
	{0x329A, 0x0ED8, 0},		/* T4_BARRIER_C1 */
	{0x329C, 0x0ED8, 0},		/* T4_BARRIER_C2 */
	{0x329E, 0x0ED8, 0},		/* T4_BARRIER_C3 */
	{0x32F6, 0x3A01, 0},		/* SHUT_RS */
	{0x32D2, 0x200A, 0},		/* SHUT_TX */
	{0x32D0, 0x3005, 0},		/* SHUT_RST */
	{0x32D4, 0x3505, 0},		/* SHUT_DCG */
	{0x32F8, 0x3C03, 0},		/* SHUT_RS_BOOST */
	{0x32DC, 0x220C, 0},		/* SHUT_TX_BOOST */
	{0x32D6, 0x3207, 0},		/* SHUT_RST_BOOST */
	{0x32E2, 0x3707, 0},		/* SHUT_DCG_BOOST */
	{0x3260, 0x00FF, 0},		/* SHUT_AB */
	{0x3262, 0x00FF, 0},		/* SHUT_AB_BOOST */
	{0x32EA, 0x3C69, 0},		/* SHUT_CTRL */
	{0x3250, 0x0005, 0},		/* LFM_AB_CTRL */
	{0x3256, 0x03E8, 0},		/* LFM_AB_PULSE */
	{0x3258, 0x0300, 0},		/* LFM_TX_CTRL */
	{0x325A, 0x0A13, 0},		/* LFM_TX_CTRL2 */
	{0x325E, 0x0186, 0},		/* LFM_EXP_FINE */
	{0x3252, 0x0107, 0},		/* LFM_AB_CTRL2 */
	{0x325E, 0x0186, 0},		/* LFM_EXP_FINE */
	{0x3752, 0x0000, 0},		/* ROW_NOISE_ADJUST_2X_LCG_T1 */
	{0x3370, 0x0111, 0},		/* DBLC_CONTROL */
	{0x3372, 0x750F, 0},		/* DBLC_FS0_CONTROL */
	{0x337A, 0x100E, 0},		/* DBLC_SCALE0 */
	{0x3504, 0x9100, 0},		/* DAC_LD_4_5 */
	{0x350C, 0x034A, 0},		/* DAC_LD_12_13 */
	{0x350E, 0x051C, 0},		/* DAC_LD_14_15 */
	{0x3510, 0x9753, 0},		/* DAC_LD_16_17 */
	{0x351E, 0x0100, 0},		/* DAC_LD_30_31 */
	{0x3520, 0x0800, 0},		/* DAC_LD_32_33 */
	{0x3522, 0x0008, 0},		/* DAC_LD_34_35 */
	{0x3524, 0x0C00, 0},		/* DAC_LD_36_37 */
	{0x3526, 0x0F00, 0},		/* DAC_LD_38_39 */
	{0x3528, 0xDDDD, 0},		/* DAC_LD_40_41 */
	{0x352A, 0x089F, 0},		/* DAC_LD_42_43 */
	{0x352C, 0x0012, 0},		/* DAC_LD_44_45 */
	{0x352E, 0x00FF, 0},		/* DAC_LD_46_47 */
	{0x3530, 0xFF00, 0},		/* DAC_LD_48_49 */
	{0x3532, 0x8F08, 0},		/* DAC_LD_50_51 */
	{0x3536, 0xFF38, 0},		/* DAC_LD_54_55 */
	{0x3538, 0x24FF, 0},		/* DAC_LD_56_57 */
	{0x353A, 0x9000, 0},		/* DAC_LD_58_59 */
	{0x353C, 0x7F00, 0},		/* DAC_LD_60_61 */
	{0x353E, 0x801F, 0},		/* DAC_LD_62_63 */
	{0x3540, 0xC62C, 0},		/* DAC_LD_64_65 */
	{0x3542, 0x4B4B, 0},		/* DAC_LD_66_67 */
	{0x3544, 0x4B46, 0},		/* DAC_LD_68_69 */
	{0x3546, 0x5A5A, 0},		/* DAC_LD_70_71 */
	{0x3548, 0x6400, 0},		/* DAC_LD_72_73 */
	{0x354A, 0x007F, 0},		/* DAC_LD_74_75 */
	{0x354E, 0x1480, 0},		/* DAC_LD_78_79 */
	{0x3554, 0x1599, 0},		/* DAC_LD_84_85 */
	{0x355A, 0x0B0E, 0},		/* DAC_LD_90_91 */
	{0x3364, 0x0004, 0},		/* DCG_TRIM */
	{0x3F72, 0x0A08, 0},		/* GCF_TRIM_0 */
	{0x3F76, 0x0E09, 0},		/* GCF_TRIM_2 */
	{0x3F78, 0x0210, 0},		/* GCF_TRIM_3 */
	{0x3116, 0x0001, 0},		/* HDR_CONTROL3 */
	{0x3100, 0xE007, 0},		/* DLO_CONTROL0 */
	{0x3102, 0x8FA0, 0},		/* DLO_CONTROL1 */
	{0x3104, 0x8FA0, 0},		/* DLO_CONTROL2 */
	{0x3106, 0x9400, 0},		/* DLO_CONTROL3 */
	{0x312A, 0x83E8, 0},		/* HDR_SE_CONTROL0 */
	{0x3C82, 0x4FFF, 0},		/* HDR_SE_CONTROL1 */
	{0x3F7A, 0x01D6, 0},		/* DLO_CONTROL5 */
	{0x3F7C, 0x00C8, 0},		/* DLO_CONTROL6 */
	{0x302A, 0x0008, 0},		/* VT_PIX_CLK_DIV */
	{0x302C, 0x0001, 0},		/* VT_SYS_CLK_DIV */
	{0x302E, 0x0003, 0},		/* PRE_PLL_CLK_DIV */
	{0x3030, 0x0030, 0},		/* PLL_MULTIPLIER */
	{0x3036, 0x0008, 0},		/* OP_WORD_CLK_DIV */
	{0x3038, 0x0001, 0},		/* OP_SYS_CLK_DIV */
	{0x30B0, 0x980E, 0},		/* DIGITAL_TEST */
	{0x30B0, 0x980E, 0},		/* DIGITAL_TEST */
	{0x31DC, 0x1FB8, 0},		/* PLL_CONTROL */
	{0x3040, 0xC004, 0},		/* READ_MODE */
	{0x3082, 0x0008, 0},		/* OPERATION_MODE_CTRL */
	{0x3044, 0x0400, 0},		/* DARK_CONTROL */
	{0x3064, 0x0180, 0},		/* SMIA_TEST */
	{0x33E0, 0x0C80, 0},		/* TEST_ASIL_ROWS */
	{0x3180, 0x0188, 0},		/* DELTA_DK_CONTROL */
	{0x33E4, 0x00C0, 0},		/* VERT_SHADING_CONTROL */
	{0x33E0, 0x0C80, 0},		/* TEST_ASIL_ROWS */

	{0x3004, 0x0018, 0},		/* X_ADDR_START_ */
	{0x3008, 0x0527, 0},		/* X_ADDR_END_ */
	{0x3002, 0x0040, 0},		/* Y_ADDR_START_ */
	{0x3006, 0x036F, 0},		/* Y_ADDR_END_ */

	{0x3402, 0x0500, 0},		/* X_OUTPUT_CONTROL */
	{0x3404, 0x03B0, 0},		/* Y_OUTPUT_CONTROL */
	{0x3400, 0x0010, 0},		/* SCALE_M */
	{0x3082, 0x000C, 0},		/* OPERATION_MODE_CTRL */
	{0x30BA, 0x1003, 0},		/* DIGITAL_CTRL */
	{0x300C, 0x0688, 0},		/* LINE_LENGTH_PCK_ */
	{0x300A, 0x0434, 0},		/* FRAME_LENGTH_LINES_ */
	{0x3042, 0x0000, 0},		/* EXTRA_DELAY */
	{0x3012, 0x0418, 0},		/* COARSE_INTEGRATION_TIME_ */
	{0x321A, 0x000F, 0},		/* COARSE_INTEGRATION_TIME4 */
	{0x3238, 0x8777, 0},		/* EXPOSURE_RATIO */
	{0x322A, 0x0939, 0},		/* FINE_INTEGRATION_CTRL */
	{0x3014, 0x0271, 0},		/* FINE_INTEGRATION_TIME_ */
	{0x3226, 0x0596, 0},		/* FINE_INTEGRATION_TIME4 */
	{0x3362, 0x0D02, 0},		/* DC_GAIN */
	{0x3366, 0xDE79, 0},		/* ANALOG_GAIN */
	{0x30B0, 0x980E, 0},		/* DIGITAL_TEST */
	{0x32EC, 0x7107, 0},		/* SHUT_CTRL2 */
	{0x33E0, 0x0080, 0},		/* TEST_ASIL_ROWS */
	{0x31D0, 0x0001, 0},		/* COMPANDING */
#if 1
	{0x33DA, 0x0000, 0},		/* OC_LUT_CONTROL */
#else	/* enable legacy compand */
	{0x33DA, 0x0001, 0},		/* OC_LUT_CONTROL */
#endif
	{0x33C0, 0x0200, 0},		/* OC_LUT_00 */
	{0x33C2, 0x3440, 0},		/* OC_LUT_01 */
	{0x33C4, 0x4890, 0},		/* OC_LUT_02 */
	{0x33C6, 0x5CE0, 0},		/* OC_LUT_03 */
	{0x33C8, 0x7140, 0},		/* OC_LUT_04 */
	{0x33CA, 0x8590, 0},		/* OC_LUT_05 */
	{0x33CC, 0x99E0, 0},		/* OC_LUT_06 */
	{0x33CE, 0xAE40, 0},		/* OC_LUT_07 */
	{0x33D0, 0xC290, 0},		/* OC_LUT_08 */
	{0x33D2, 0xD6F0, 0},		/* OC_LUT_09 */
	{0x33D4, 0xEB40, 0},		/* OC_LUT_10 */
	{0x33D6, 0xFA00, 0},		/* OC_LUT_11 */
	{0x31AE, 0x0201, 0},		/* SERIAL_FORMAT */
	{0x31AE, 0x0001, 0},		/* SERIAL_FORMAT */
	{0x31AC, 0x140C, 0},		/* DATA_FORMAT_BITS */
	{0x301A, 0x10D8, 0},		/* RESET_REGISTER */

	/* Trigger mode */
	{0x301A, 0x11D8, 0},		/* Enable GPI */
	{0x340A, 0x0077, 0},		/*
					 * Disable GPIO_3 Output Buffer and
					 * enable Input Buffer
					 */
	{0x340C, 0x0080, 0},		/*
					 * Enable GPIO_3 as input trigger mode
					 */
	{0x340E, 0x0210, 0},		/*
					 * Disable GPIO_3 output
					 * select function
					 */
	{0x30CE, 0x0120, 0},
	{0x301A, 0x11DC, 0},		/* RESET_REGISTER */

	/* Disable Embedded data */
	{0x301A, 0x11D8, 0},		/* RESET_REGISTER */
	{0x3064, 0x0080, 0},		/* SMIA_TEST */
	{0x3064, 0x0000, 0},		/* SMIA_TEST */
	{0x301A, 0x11DC, 0},		/* RESET_REGISTER */
};

static const struct regmap_config ar0147_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static void ar0147_init_format(struct ar0147 *dev)
{
	dev->fmt.width = 1296;
	dev->fmt.height	= 816;
	dev->fmt.code = MEDIA_BUS_FMT_SGRBG12_1X12;
	dev->fmt.field = V4L2_FIELD_NONE;
	dev->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
}

/*
 * Helper fuctions for reflection
 */
static inline struct ar0147 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0147, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int ar0147_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int	ret = 0;

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
static int ar0147_init(struct v4l2_subdev *sd, u32 enable)
{
	struct ar0147		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		/* enable ar0147 */
		ret = regmap_multi_reg_write(dev->regmap,
				ar0147_reg_init,
				ARRAY_SIZE(ar0147_reg_init));
		if (ret) {
			/* err status */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* disable ar0147 */
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
static int ar0147_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0147		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);


	mutex_unlock(&dev->lock);
	return ret;
}

static int ar0147_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct ar0147		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	memcpy((void *)&format->format, (const void *)&dev->fmt,
		sizeof(struct v4l2_mbus_framefmt));

	mutex_unlock(&dev->lock);
	return ret;
}

static int ar0147_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *format)
{
	struct ar0147		*dev	= to_dev(sd);
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
static const struct v4l2_ctrl_ops ar0147_ctrl_ops = {
	.s_ctrl			= ar0147_s_ctrl,
};

static const struct v4l2_subdev_core_ops ar0147_core_ops = {
	.init			= ar0147_init,
};

static const struct v4l2_subdev_video_ops ar0147_video_ops = {
	.s_stream		= ar0147_s_stream,
};

static const struct v4l2_subdev_pad_ops ar0147_pad_ops = {
	.get_fmt		= ar0147_get_fmt,
	.set_fmt		= ar0147_set_fmt,
};

static const struct v4l2_subdev_ops ar0147_ops = {
	.core			= &ar0147_core_ops,
	.video			= &ar0147_video_ops,
	.pad			= &ar0147_pad_ops,
};

struct ar0147 ar0147_data = {
};

static const struct i2c_device_id ar0147_id[] = {
	{ "ar0147", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0147_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ar0147_of_match[] = {
	{
		.compatible	= "onnn,ar0147",
		.data		= &ar0147_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ar0147_of_match);
#endif

int ar0147_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ar0147			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct ar0147), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(ar0147_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &ar0147_ops);

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &ar0147_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&ar0147_ctrl_ops,
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

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &ar0147_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	/* init format info */
	ar0147_init_format(dev);

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int ar0147_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct ar0147		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver ar0147_driver = {
	.probe		= ar0147_probe,
	.remove		= ar0147_remove,
	.driver		= {
		.name		= "ar0147",
		.of_match_table	= of_match_ptr(ar0147_of_match),
	},
	.id_table	= ar0147_id,
};

module_i2c_driver(ar0147_driver);
