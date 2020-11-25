// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_ISP_REG_H__
#define __TCC_ISP_REG_H__

#define REG_HIGH_LOW				(0x2)
#define REG_HIGH				(0x0)
#define REG_LOW					(0x2)

#define ISP_0					(0)
#define ISP_1					(1)
#define ISP_2					(2)
#define ISP_3					(3)
#define ISP_MAX					(4)

#define ISP_ENTITY_IP				(0)
#define ISP_ENTITY_MEM				(1)
#define ISP_ENTITY_MAX				(2)

/******************************************************************
 * ISP Factory Only Register Control ( 0 page 0x000~0xFFF )
 ******************************************************************/
#define reg_Sleep_mode_ctl			(0x0004 + REG_LOW)
#define reg_Soft_reset				(0x0008 + REG_LOW)
#define reg_mem_swt_en				(0x000C + REG_LOW)
#define reg_img_size_width			(0x0010 + REG_HIGH)
#define reg_img_size_height			(0x0014 + REG_HIGH)
#define reg_update_ctl				(0x0030 + REG_HIGH)
#define reg_update_sel1				(0x0034 + REG_HIGH)
#define reg_update_sel2				(0x0038 + REG_HIGH)
#define reg_update_mod1				(0x003C + REG_HIGH)
#define reg_update_mod2				(0x0040 + REG_HIGH)
#define reg_update_user_cnt1			(0x0048 + REG_HIGH)
#define reg_update_user_cnt2			(0x004C + REG_HIGH)
#define reg_intr_mask				(0x0060 + REG_LOW)
#define reg_intr_clear				(0x0064 + REG_LOW)
#define reg_intr_status 			(0x0068 + REG_LOW)
#define reg_misr_ctl				(0x0070 + REG_HIGH)
#define reg_misr_byr_seed1			(0x0074 + REG_HIGH)
#define reg_misr_byr_seed2			(0x0078 + REG_HIGH)
#define reg_misr_rgb_seed1			(0x007C + REG_HIGH)
#define reg_misr_rgb_seed2			(0x0080 + REG_HIGH)
#define reg_misr_yuv_seed1			(0x0084 + REG_HIGH)
#define reg_misr_yuv_seed2			(0x0088 + REG_HIGH)
#define reg_misr_byr_rdata1 			(0x008C + REG_HIGH)
#define reg_misr_byr_rdata2 			(0x0090 + REG_HIGH)
#define reg_misr_rgb_rdata1 			(0x0094 + REG_HIGH)
#define reg_misr_rgb_rdata2 			(0x0098 + REG_HIGH)
#define reg_misr_yuv_rdata1 			(0x009C + REG_HIGH)
#define reg_misr_yuv_rdata2 			(0x00A0 + REG_HIGH)

#define rst_sleep_mode_ctl			(0x0000)
#define rst_soft_reset				(0x0000)
#define rst_mem_swt_en				(0x0000)
#define rst_img_size_width			(0x0CC0)
#define rst_img_size_height 			(0x0990)
#define rst_update_ctl				(0x0000)
#define rst_update_sel1 			(0x0000)
#define rst_update_sel2 			(0x0400)
#define rst_update_mod1 			(0x0000)
#define rst_update_mod2 			(0x0000)
#define rst_update_user_cnt1			(0x0000)
#define rst_update_user_cnt2			(0x0100)
#define rst_intr_mask				(0xFFFF)
#define rst_intr_clear				(0x0000)
#define rst_misr_ctl				(0x1080)
#define rst_misr_byr_seed1			(0x010A)
#define rst_misr_byr_seed2			(0x8001)
#define rst_misr_rgb_seed1			(0x010A)
#define rst_misr_rgb_seed2			(0x8001)
#define rst_misr_yuv_seed1			(0x010A)
#define rst_misr_yuv_seed2			(0x8001)

/* update_ctl */
#define UPDATE_CTL_V_PTRNGEN_OUT_MASK		(0x1)
#define UPDATE_CTL_V_BYRCHGAIN_OUT_MASK		(0x1)
#define UPDATE_CTL_V_SHADING_OUT_MASK		(0x1)
#define UPDATE_CTL_V_BYRAWBGAIN_OUT_MASK	(0x1)
#define UPDATE_CTL_V_BYRCRV_OUT_MASK		(0x1)
#define UPDATE_CTL_V_PREBYR_OUT_MASK		(0x1)
#define UPDATE_CTL_V_POSTBYR_OUT_MASK		(0x1)
#define UPDATE_CTL_V_BYRINT_OUT_MASK		(0x1)
#define UPDATE_CTL_V_AFDATA_OUT_MASK		(0x1)
#define UPDATE_CTL_V_TRI_AUTO_OUT_MASK		(0x1)
#define UPDATE_CTL_V_CLREN_OUT_MASK		(0x1)
#define UPDATE_CTL_V_CONTSAT_OUT_MASK		(0x1)
#define UPDATE_CTL_V_YUVPROC_OUT_MASK		(0x1)
#define UPDATE_CTL_V_YUVPROC_OUT2_MASK		(0x1) /* TODO */
#define UPDATE_CTL_V_WINDOW_OUT_MASK		(0x1)
#define UPDATE_CTL_V_DEBLANK_OUT_MASK		(0x1)

#define UPDATE_CTL_V_PTRNGEN_OUT_SHIFT		(0)
#define UPDATE_CTL_V_BYRCHGAIN_OUT_SHIFT	(1)
#define UPDATE_CTL_V_SHADING_OUT_SHIFT		(2)
#define UPDATE_CTL_V_BYRAWBGAIN_OUT_SHIFT	(3)
#define UPDATE_CTL_V_BYRCRV_OUT_SHIFT		(4)
#define UPDATE_CTL_V_PREBYR_OUT_SHIFT		(5)
#define UPDATE_CTL_V_POSTBYR_OUT_SHIFT		(6)
#define UPDATE_CTL_V_BYRINT_OUT_SHIFT		(7)
#define UPDATE_CTL_V_AFDATA_OUT_SHIFT		(8)
#define UPDATE_CTL_V_TRI_AUTO_OUT_SHIFT		(9)
#define UPDATE_CTL_V_CLREN_OUT_SHIFT		(10)
#define UPDATE_CTL_V_CONTSAT_OUT_SHIFT		(11)
#define UPDATE_CTL_V_YUVPROC_OUT_SHIFT		(12)
#define UPDATE_CTL_V_YUVPROC_OUT2_SHIFT		(13) /* TODO */
#define UPDATE_CTL_V_WINDOW_OUT_SHIFT		(14)
#define UPDATE_CTL_V_DEBLANK_OUT_SHIFT		(15)

#define UPDATE_CTL_V_PTRNGEN_OUT		(0x1)
#define UPDATE_CTL_V_BYRCHGAIN_OUT		(0x1)
#define UPDATE_CTL_V_SHADING_OUT		(0x1)
#define UPDATE_CTL_V_BYRAWBGAIN_OUT		(0x1)
#define UPDATE_CTL_V_BYRCRV_OUT			(0x1)
#define UPDATE_CTL_V_PREBYR_OUT			(0x1)
#define UPDATE_CTL_V_POSTBYR_OUT		(0x1)
#define UPDATE_CTL_V_BYRINT_OUT			(0x1)
#define UPDATE_CTL_V_AFDATA_OUT			(0x1)
#define UPDATE_CTL_V_TRI_AUTO_OUT		(0x1)
#define UPDATE_CTL_V_CLREN_OUT			(0x1)
#define UPDATE_CTL_V_CONTSAT_OUT		(0x1)
#define UPDATE_CTL_V_YUVPROC_OUT		(0x1)
#define UPDATE_CTL_V_YUVPROC_OUT2		(0x1) /* TODO */
#define UPDATE_CTL_V_WINDOW_OUT			(0x1)
#define UPDATE_CTL_V_DEBLANK_OUT		(0x1)

/* update_sel1 */
#define UPDATE_SEL1_UPDATE_00_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_01_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_02_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_03_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_04_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_05_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_06_SEL_MASK		(0x3)
#define UPDATE_SEL1_UPDATE_07_SEL_MASK		(0x3)

#define UPDATE_SEL1_UPDATE_00_SEL_SHIFT		(0)
#define UPDATE_SEL1_UPDATE_01_SEL_SHIFT		(2)
#define UPDATE_SEL1_UPDATE_02_SEL_SHIFT		(4)
#define UPDATE_SEL1_UPDATE_03_SEL_SHIFT		(6)
#define UPDATE_SEL1_UPDATE_04_SEL_SHIFT		(8)
#define UPDATE_SEL1_UPDATE_05_SEL_SHIFT		(10)
#define UPDATE_SEL1_UPDATE_06_SEL_SHIFT		(12)
#define UPDATE_SEL1_UPDATE_07_SEL_SHIFT		(14)

/* update_sel2 */
#define UPDATE_SEL2_UPDATE_08_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_09_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_10_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_11_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_12_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_13_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_14_SEL_MASK		(0x3)
#define UPDATE_SEL2_UPDATE_15_SEL_MASK		(0x3)

#define UPDATE_SEL2_UPDATE_08_SEL_SHIFT		(0)
#define UPDATE_SEL2_UPDATE_09_SEL_SHIFT		(2)
#define UPDATE_SEL2_UPDATE_10_SEL_SHIFT		(4)
#define UPDATE_SEL2_UPDATE_11_SEL_SHIFT		(6)
#define UPDATE_SEL2_UPDATE_12_SEL_SHIFT		(8)
#define UPDATE_SEL2_UPDATE_13_SEL_SHIFT		(10)
#define UPDATE_SEL2_UPDATE_14_SEL_SHIFT		(12)
#define UPDATE_SEL2_UPDATE_15_SEL_SHIFT		(14)

#define UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING	(0)
#define UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING	(1)
#define UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING	(2)
#define UPDATE_SEL_SYNC_ON_WRITING_TIMING		(3)

#define UPDATE_SEL_ALL_SYNC_ON_USER_SPECIFIED_TIMING \
	((UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_00_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_01_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_02_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_03_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_04_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_05_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_06_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	  UPDATE_SEL1_UPDATE_07_SEL_SHIFT))

#define UPDATE_SEL_ALL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING \
	((UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_00_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_01_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_02_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_03_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_04_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_05_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_06_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_07_SEL_SHIFT))

#define UPDATE_SEL_ALL_SYNC_ON_VSYNC_RISING_EDGE_TIMING \
	((UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_00_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_01_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_02_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_03_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_04_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_05_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_06_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	  UPDATE_SEL1_UPDATE_07_SEL_SHIFT))

#define UPDATE_SEL_ALL_SYNC_ON_WRITING_TIMING \
	((UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_00_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_01_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_02_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_03_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_04_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_05_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_06_SEL_SHIFT) | \
	 (UPDATE_SEL_SYNC_ON_WRITING_TIMING << \
	  UPDATE_SEL1_UPDATE_07_SEL_SHIFT))

/* update_mod1 */
#define UPDATE_MOD1_UPDATE_MODE_0_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_1_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_2_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_3_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_4_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_5_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_6_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_7_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_8_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_9_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_10_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_11_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_12_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_13_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_14_MASK		(0x1)
#define UPDATE_MOD1_UPDATE_MODE_15_MASK		(0x1)

#define UPDATE_MOD1_UPDATE_MODE_0_SHIFT		(0)
#define UPDATE_MOD1_UPDATE_MODE_1_SHIFT		(1)
#define UPDATE_MOD1_UPDATE_MODE_2_SHIFT		(2)
#define UPDATE_MOD1_UPDATE_MODE_3_SHIFT		(3)
#define UPDATE_MOD1_UPDATE_MODE_4_SHIFT		(4)
#define UPDATE_MOD1_UPDATE_MODE_5_SHIFT		(5)
#define UPDATE_MOD1_UPDATE_MODE_6_SHIFT		(6)
#define UPDATE_MOD1_UPDATE_MODE_7_SHIFT		(7)
#define UPDATE_MOD1_UPDATE_MODE_8_SHIFT		(8)
#define UPDATE_MOD1_UPDATE_MODE_9_SHIFT		(9)
#define UPDATE_MOD1_UPDATE_MODE_10_SHIFT	(10)
#define UPDATE_MOD1_UPDATE_MODE_11_SHIFT	(11)
#define UPDATE_MOD1_UPDATE_MODE_12_SHIFT	(12)
#define UPDATE_MOD1_UPDATE_MODE_13_SHIFT	(13)
#define UPDATE_MOD1_UPDATE_MODE_14_SHIFT	(14)
#define UPDATE_MOD1_UPDATE_MODE_15_SHIFT	(15)

#define UPDATE_MOD1_SYNC_MODE			(0)
#define UPDATE_MOD1_ALWAYS_MODE			(1)

#define UPDATE_MOD1_ALL_SYNC_MODE \
	((UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_0_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_1_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_2_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_3_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_4_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_5_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_6_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_7_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_8_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_9_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_10_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_11_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_12_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_13_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_14_SHIFT) | \
	 (UPDATE_MOD1_SYNC_MODE << UPDATE_MOD1_UPDATE_MODE_15_SHIFT))

#define UPDATE_MOD1_ALL_ALWAYS_MODE \
	((UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_0_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_1_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_2_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_3_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_4_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_5_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_6_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_7_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_8_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_9_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_10_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_11_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_12_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_13_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_14_SHIFT) | \
	 (UPDATE_MOD1_ALWAYS_MODE << UPDATE_MOD1_UPDATE_MODE_15_SHIFT))

/* update_mod2 */
#define UPDATE_MOD2_VSYNC_00_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_01_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_02_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_03_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_04_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_05_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_06_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_07_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_08_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_09_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_10_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_11_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_12_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_13_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_14_SEL_MASK		(0x1)
#define UPDATE_MOD2_VSYNC_15_SEL_MASK		(0x1)

#define UPDATE_MOD2_VSYNC_00_SEL_SHIFT		(0)
#define UPDATE_MOD2_VSYNC_01_SEL_SHIFT		(1)
#define UPDATE_MOD2_VSYNC_02_SEL_SHIFT		(2)
#define UPDATE_MOD2_VSYNC_03_SEL_SHIFT		(3)
#define UPDATE_MOD2_VSYNC_04_SEL_SHIFT		(4)
#define UPDATE_MOD2_VSYNC_05_SEL_SHIFT		(5)
#define UPDATE_MOD2_VSYNC_06_SEL_SHIFT		(6)
#define UPDATE_MOD2_VSYNC_07_SEL_SHIFT		(7)
#define UPDATE_MOD2_VSYNC_08_SEL_SHIFT		(8)
#define UPDATE_MOD2_VSYNC_09_SEL_SHIFT		(9)
#define UPDATE_MOD2_VSYNC_10_SEL_SHIFT		(10)
#define UPDATE_MOD2_VSYNC_11_SEL_SHIFT		(11)
#define UPDATE_MOD2_VSYNC_12_SEL_SHIFT		(12)
#define UPDATE_MOD2_VSYNC_13_SEL_SHIFT		(13)
#define UPDATE_MOD2_VSYNC_14_SEL_SHIFT		(14)
#define UPDATE_MOD2_VSYNC_15_SEL_SHIFT		(15)

#define UPDATE_MOD2_INDIVIDUAL_SYNC		(0)
#define UPDATE_MOD2_GROUP_SYNC			(1)

#define UPDATE_MOD2_ALL_GROUP_SYNC \
	((UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_00_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_01_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_02_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_03_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_04_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_05_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_06_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_07_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_08_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_09_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_10_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_11_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_12_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_13_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_14_SEL_SHIFT) | \
	 (UPDATE_MOD2_GROUP_SYNC << UPDATE_MOD2_VSYNC_15_SEL_SHIFT))

/* update_user_cnt1 */

/* update_user_cnt2 */


/******************************************************************
 * Image Format Control (1page 0x000~0x0FF)
 ******************************************************************/
#define reg_imgout_window_ctl			(0x1000 + REG_HIGH)
#define reg_imgout_window_x_start		(0x1004 + REG_HIGH)
#define reg_imgout_window_y_start		(0x1008 + REG_HIGH)
#define reg_imgout_window_x_width		(0x100C + REG_HIGH)
#define reg_imgout_window_y_width		(0x1010 + REG_HIGH)
#define reg_imgout_window_cfg			(0x1014 + REG_HIGH)
#define reg_imgin_order_ctl 			(0x1020 + REG_HIGH)
#define reg_ptrngen_ctl 			(0x1030 + REG_HIGH)
#define reg_ptrngen_idx_ctl 			(0x1034 + REG_HIGH)
#define reg_ptrngen_rand			(0x1038 + REG_HIGH)
#define reg_ptrngen_seed			(0x103C + REG_HIGH)
#if 0
#define reg_ptrngen_frm_wid 			(0x1040)
#define reg_ptrngen_frm_hgt 			(0x1044)
#define reg_ptrngen_frm_str 			(0x1048)
#define reg_ptrngen_frm_end 			(0x104C)
#define reg_ptrngen_img_stx 			(0x1050)
#define reg_ptrngen_img_sty 			(0x1054)
#define reg_ptrngen_img_enx 			(0x1058)
#define reg_ptrngen_img_eny 			(0x105C)
#endif

#define rst_imgout_window_ctl			(0x0001)
#define rst_imgout_window_x_start		(0x0000)
#define rst_imgout_window_y_start		(0x0000)
#define rst_imgout_window_x_width		(0x0800)
#define rst_imgout_window_y_width		(0x0600)
#define rst_imgout_window_cfg			(0x0005)
#define rst_imgin_order_ctl 			(0x000C)
#define rst_ptrngen_ctl 			(0x0000)
#define rst_ptrngen_idx_ctl 			(0x00E4)
#define rst_ptrngen_rand			(0x0000)
#define rst_ptrngen_seed			(0x0000)
#define rst_ptrngen_frm_wid 			(0x083F)
#define rst_ptrngen_frm_hgt 			(0x063F)
#define rst_ptrngen_frm_str 			(0x0004)
#define rst_ptrngen_frm_end 			(0x0008)
#define rst_ptrngen_img_stx 			(0x0000)
#define rst_ptrngen_img_sty 			(0x0030)
#define rst_ptrngen_img_enx 			(0x07FF)
#define rst_ptrngen_img_eny 			(0x062F)

#define IMGIN_ORDER_CTL_B_FIRST			(0x0)
#define IMGIN_ORDER_CTL_GB_FIRST		(0x1)
#define IMGIN_ORDER_CTL_GR_FIRST		(0x2)
#define IMGIN_ORDER_CTL_R_FIRST			(0x3)

#define IMGOUT_WINDOW_CTL_ISP_OUTPUT_WINDOW_MASK	(0x1)
#define IMGOUT_WINDOW_CTL_ISP_OUTPUT_WINDOW_SHIFT	(0)
#define IMGOUT_WINDOW_CTL_ISP_OUTPUT_WINDOW_EN		(0x1)
#define IMGOUT_WINDOW_CTl_DEBLANK_MASK			(0x1)
#define IMGOUT_WINDOW_CTL_DEBLANK_SHIFT			(14)
#define IMGOUT_WINDOW_CTL_DEBLANK_EN			(0x1)

#define IMGOUT_WINDOW_CFG_FORMAT_MASK		(0x3)
#define IMGOUT_WINDOW_CFG_FORMAT_SHIFT		(0)
#define IMGOUT_WINDOW_CFG_FORMAT_YUV420		(0x0) /* do not support */
#define IMGOUT_WINDOW_CFG_FORMAT_YUV422		(0x1)
#define IMGOUT_WINDOW_CFG_FORMAT_YUV444		(0x2)
#define IMGOUT_WINDOW_CFG_FORMAT_RGB888		(0x3)

#define IMGOUT_WINDOW_CFG_DATA_ORDER_MASK	(0x3)
#define IMGOUT_WINDOW_CFG_DATA_ORDER_SHIFT	(4)
#define IMGOUT_WINDOW_CFG_DATA_ORDER_P2P1P0	(0x0)
#define IMGOUT_WINDOW_CFG_DATA_ORDER_P0P2P1	(0x1)
#define IMGOUT_WINDOW_CFG_DATA_ORDER_P2P1P2	(0x2)
#define IMGOUT_WINDOW_CFG_DATA_ORDER_P2P0P1	(0x3)


/******************************************************************
 * 3A Control Register Define (1page 0x200~2FF)
 ******************************************************************/
#define reg_tri_auto_ctl 			(0x1200 + REG_HIGH)
#define reg_tri_auto_cfg1			(0x1200 + REG_LOW)
#define reg_tri_auto_cfg2			(0x1204 + REG_HIGH)
#define reg_af_ctl				(0x1204 + REG_LOW)

#define rst_triauto_ctl 			(0x0000)
#define rst_triauto_cfg1			(0x0A23)
#define rst_triauto_cfg2			(0x140A)
#define rst_af_ctl				(0x0000)


/******************************************************************
 * Bayer Channel Gain (1page 0x300~0x330)
 ******************************************************************/
#define reg_chgain_ctl	 			(0x1300 + REG_HIGH)
#define reg_PrePosAgain_b			(0x1300 + REG_LOW)
#define reg_PrePosAgain_gb			(0x1304 + REG_HIGH)
#define reg_PrePosAgain_gr			(0x1304 + REG_LOW)
#define reg_PrePosAgain_r			(0x1308 + REG_HIGH)
#define reg_chMgain_mr				(0x1308 + REG_LOW)
#define reg_chMgain_mb				(0x130C + REG_HIGH)
#define reg_chMgain_mgr 			(0x130C + REG_LOW)
#define reg_chMgain_mgb 			(0x1310 + REG_HIGH)

#define rst_chgain_ctl 				(0x0001)
#define rst_PrePosAgain_b			(0x0000)
#define rst_PrePosAgain_gb			(0x0000)
#define rst_PrePosAgain_gr			(0x0000)
#define rst_PrePosAgain_r			(0x0000)
#define rst_chMgain_mr				(0x0040)
#define rst_chMgain_mb				(0x0040)
#define rst_chMgain_mgr 			(0x0040)
#define rst_chMgain_mgb 			(0x0040)

/* reg_chgain_ctl1 */
#define CHGAIN_CTL_ON_OFF_SHIFT			(0)
#define CHGAIN_CTL_ENABLE_GLOBAL_OFFSET_SHIFT	(1)
#define CHGAIN_CTL_B_SIGN_SHIFT			(4)
#define CHGAIN_CTL_GB_SIGN_SHIFT		(5)
#define CHGAIN_CTL_GR_SIGN_SHIFT		(6)
#define CHGAIN_CTL_R_SIGN_SHIFT			(7)

#define CHGAIN_CTL_ON_OFF_MASK			\
	((0x1) << CHGAIN_CTL_ON_OFF_SHIFT)
#define CHGAIN_CTL_ENABLE_GLOBAL_OFFSET_MASK	\
	((0X1) << CHGAIN_CTL_ENABLE_GLOBAL_OFFSET_SHIFT)
#define CHGAIN_CTL_B_SIGN_MASK			\
	((0X1) << CHGAIN_CTL_B_SIGN_SHIFT)
#define CHGAIN_CTL_GB_SIGN_MASK			\
	((0X1) << CHGAIN_CTL_GB_SIGN_SHIFT)
#define CHGAIN_CTL_GR_SIGN_MASK			\
	((0X1) << CHGAIN_CTL_GR_SIGN_SHIFT)
#define CHGAIN_CTL_R_SIGN_MASK			\
	((0X1) << CHGAIN_CTL_R_SIGN_SHIFT)


/******************************************************************
 * Bayer AWB Gain (1page 0x400~0x40A)
 ******************************************************************/
#define reg_wbgain_ctl				(0x1400 + REG_HIGH)
#define reg_wbgain_r				(0x1400 + REG_LOW)
#define reg_wbgain_g				(0x1404 + REG_HIGH)
#define reg_wbgain_b				(0x1408 + REG_LOW)

#define rst_wbgain_ctl				(0x0001)
#define rst_wbgain_r				(0x0100)
#define rst_wbgain_g				(0x0100)
#define rst_wbgain_b				(0x0100)


/******************************************************************
 * RGB AE Gain (1page 0x500~0x504)
 ******************************************************************/
#define reg_aegain_ctl				(0x1500 + REG_HIGH)
#define reg_taegain 				(0x1500 + REG_LOW)

#define rst_aegain_ctl				(0x0001)
#define rst_taegain 				(0x0100)


/******************************************************************
 * Bayer Curve Gain (1page 0x800~0x850)
 ******************************************************************/
#define reg_byrcrv_ctl				(0x1800 + REG_HIGH)
#define reg_byrcrvRGB_0 			(0x1800 + REG_LOW)
#define reg_byrcrvRGB_1 			(0x1804 + REG_HIGH)
#define reg_byrcrvRGB_2 			(0x1804 + REG_LOW)
#define reg_byrcrvRGB_3 			(0x1808 + REG_HIGH)
#define reg_byrcrvRGB_4 			(0x1808 + REG_LOW)
#define reg_byrcrvRGB_5 			(0x180C + REG_HIGH)
#define reg_byrcrvRGB_6 			(0x180C + REG_LOW)
#define reg_byrcrvRGB_7 			(0x1810 + REG_HIGH)
#define reg_byrcrvRGB_8 			(0x1810 + REG_LOW)
#define reg_byrcrvRGB_9 			(0x1814 + REG_HIGH)
#define reg_byrcrvRGB_10			(0x1814 + REG_LOW)
#define reg_byrcrvRGB_11			(0x1818 + REG_HIGH)
#define reg_byrcrvRGB_12			(0x1818 + REG_LOW)
#define reg_byrcrvRGB_13			(0x181C + REG_HIGH)
#define reg_byrcrvRGB_14			(0x181C + REG_LOW)
#define reg_byrcrvRGB_15			(0x1820 + REG_HIGH)
#define reg_byrcrvRGB_16			(0x1820 + REG_LOW)
#define reg_byrcrvRGB_17			(0x1824 + REG_HIGH)
#define reg_byrcrvRGB_18			(0x1824 + REG_LOW)
#define reg_byrcrvRGB_19			(0x1828 + REG_HIGH)

#define rst_byrcrv_ctl				(0x0000)
#define rst_byrcrvRGB_0 			(0x002c)
#define rst_byrcrvRGB_1 			(0x005c)
#define rst_byrcrvRGB_2 			(0x008c)
#define rst_byrcrvRGB_3 			(0x00bc)
#define rst_byrcrvRGB_4 			(0x0110)
#define rst_byrcrvRGB_5 			(0x0158)
#define rst_byrcrvRGB_6 			(0x0194)
#define rst_byrcrvRGB_7 			(0x01c8)
#define rst_byrcrvRGB_8 			(0x01f4)
#define rst_byrcrvRGB_9 			(0x0220)
#define rst_byrcrvRGB_10			(0x0244)
#define rst_byrcrvRGB_11			(0x0268)
#define rst_byrcrvRGB_12			(0x02ac)
#define rst_byrcrvRGB_13			(0x02e0)
#define rst_byrcrvRGB_14			(0x0308)
#define rst_byrcrvRGB_15			(0x032c)
#define rst_byrcrvRGB_16			(0x0370)
#define rst_byrcrvRGB_17			(0x03a8)
#define rst_byrcrvRGB_18			(0x03e0)
#define rst_byrcrvRGB_19			(0x03ff)


/******************************************************************
 * De-Companding (1page 0x860~0x880)
 ******************************************************************/
#define reg_dcpd_ctl				(0x1860 + REG_HIGH)
#define reg_dcpd_crv_0				(0x1860 + REG_LOW)
#define reg_dcpd_crv_1				(0x1864 + REG_HIGH)
#define reg_dcpd_crv_2				(0x1864 + REG_LOW)
#define reg_dcpd_crv_3				(0x1868 + REG_HIGH)
#define reg_dcpd_crv_4				(0x1868 + REG_LOW)
#define reg_dcpd_crv_5				(0x186C + REG_HIGH)
#define reg_dcpd_crv_6				(0x186C + REG_LOW)
#define reg_dcpd_crv_7				(0x1870 + REG_HIGH)
#define reg_dcpdx_0 				(0x1870 + REG_LOW)
#define reg_dcpdx_1 				(0x1874 + REG_HIGH)
#define reg_dcpdx_2 				(0x1874 + REG_LOW)
#define reg_dcpdx_3 				(0x1878 + REG_HIGH)
#define reg_dcpdx_4 				(0x1878 + REG_LOW)
#define reg_dcpdx_5 				(0x187C + REG_HIGH)
#define reg_dcpdx_6 				(0x187C + REG_LOW)
#define reg_dcpdx_7 				(0x1880 + REG_HIGH)

#define rst_dcpd_ctl				(0x0760)
#define rst_dcpd_crv_0				(32)
#define rst_dcpd_crv_1				(64)
#define rst_dcpd_crv_2				(128)
#define rst_dcpd_crv_3				(256)
#define rst_dcpd_crv_4				(512)
#define rst_dcpd_crv_5				(800)
#define rst_dcpd_crv_6				(900)
#define rst_dcpd_crv_7				(1023)
#define rst_dcpdx_0 				(32)
#define rst_dcpdx_1 				(64)
#define rst_dcpdx_2 				(128)
#define rst_dcpdx_3 				(256)
#define rst_dcpdx_4 				(512)
#define rst_dcpdx_5 				(800)
#define rst_dcpdx_6 				(900)
#define rst_dcpdx_7 				(1023)

#define DCPD_CTL_DCPD_CURVE_MAXVAL_NO_MASK	(0x7)
#define DCPD_CTL_OUTPUT_BIT_SELECTION_MASK	(0x7)
#define DCPD_CTL_INPUT_BIT_SELECTION_MASK	(0x7)
#define DCPD_CTL_DCPD_ON_OFF_MASK		(0x1)

#define DCPD_CTL_DCPD_CURVE_MAXVAL_NO_SHIFT	(0x8)
#define DCPD_CTL_OUTPUT_BIT_SELECTION_SHIFT	(0x4)
#define DCPD_CTL_INPUT_BIT_SELECTION_SHIFT	(0x1)
#define DCPD_CTL_DCPD_ON_OFF_SHIFT		(0x0)

#define DCPD_CTL_OUTPUT_BIT_10			(0)
#define DCPD_CTL_OUTPUT_BIT_12			(1)
#define DCPD_CTL_OUTPUT_BIT_14			(2)
#define DCPD_CTL_OUTPUT_BIT_15			(3)
#define DCPD_CTL_OUTPUT_BIT_16			(4)
#define DCPD_CTL_OUTPUT_BIT_17			(5)
#define DCPD_CTL_OUTPUT_BIT_20			(6)

#define DCPD_CTL_INPUT_BIT_10			(0)
#define DCPD_CTL_INPUT_BIT_12			(1)


/******************************************************************
 * Shading Correction Register Define (1page 0x900~0x918)
 ******************************************************************/
#define reg_shading_ctl_h			(0x1900 + REG_HIGH)
#define reg_shading_ctl_l			(0x1900 + REG_LOW)
#define reg_shading_x_inc			(0x1904 + REG_HIGH)
#define reg_shading_y_inc			(0x1904 + REG_LOW)
#define reg_shading_gain			(0x1908 + REG_HIGH)

#define rst_shading_ctl_h			(0x0000)
#define rst_shading_ctl_l			(0x0000)
#define rst_shading_x_inc			(0x0080)
#define rst_shading_y_inc			(0x0065)
#define rst_shading_gain			(0x0400)


/******************************************************************
 * Pre/Post Bayer DPC (1page 0xA00~0xA02)
 ******************************************************************/
#define  reg_prebyr_dpc_ctl 			(0x1A00 + REG_HIGH)
#define  reg_postbyr_dpc_ctl			(0x1A00 + REG_LOW)

#define  rst_prebyr_dpc_ctl 			(0x0001)
#define  rst_postbyr_dpc_ctl			(0x0001)


/******************************************************************
 * GBGR Correction (1page 0xA10~0xA16)
 ******************************************************************/
#define  reg_gbgr_ctl				(0x1A10 + REG_HIGH)
#define  reg_gbgr_flat_rate10			(0x1A10 + REG_LOW)
#define  reg_gbgr_flat_rate32			(0x1A14 + REG_HIGH)
#define  reg_gbgr_flat_ratex4			(0x1A14 + REG_LOW)

#define  rst_gbgr_ctl				(0x0001)
#define  rst_gbgr_flat_rate10			(0x4000)
#define  rst_gbgr_flat_rate32			(0xC476)
#define  rst_gbgr_flat_ratex4			(0x00FF)


/******************************************************************
 * Pre Bayer NR (1page 0xA30~0xA36)
 ******************************************************************/
#define reg_prebyr_nr_ctl			(0x1A30 + REG_HIGH)
#define reg_byr_nr_std_gain10			(0x1A30 + REG_LOW)
#define reg_byr_nr_std_gain32			(0x1A34 + REG_HIGH)
#define reg_byr_nr_std_gainx4			(0x1A34 + REG_LOW)

#define rst_prebyr_nr_ctl			(0x0001)
#define rst_byr_nr_std_gain10			(0x0000)
#define rst_byr_nr_std_gain32			(0x0000)
#define rst_byr_nr_std_gainx4			(0x0000)


/******************************************************************
 * Pre Bayer Sharpness (1page 0xA50~0xA5C)
 ******************************************************************/
#define  reg_prebyr_sharp_ctl			(0x1A50 + REG_HIGH)
#define  reg_prebyr_sharp_rbgain		(0x1A50 + REG_LOW)
#define  reg_prebyr_sharp_ggain 		(0x1A54 + REG_HIGH)
#define  reg_prebyr_sharp_std10 		(0x1A54 + REG_LOW)
#define  reg_prebyr_sharp_std32 		(0x1A58 + REG_HIGH)
#define  reg_prebyr_sharp_stdx4 		(0x1A58 + REG_LOW)
#define  reg_prebyr_sharp_coreval		(0x1A5C + REG_HIGH)

#define  rst_prebyr_sharp_ctl			(0x0002)
#define  rst_prebyr_sharp_rbgain		(0x1014)
#define  rst_prebyr_sharp_ggain 		(0x0018)
#define  rst_prebyr_sharp_std10 		(0x0400)
#define  rst_prebyr_sharp_std32 		(0x200F)
#define  rst_prebyr_sharp_stdx4 		(0x5a3a)
#define  rst_prebyr_sharp_coreval		(0x000A)


/******************************************************************
 * Post Bayer NR(Noise Reduction) (1page 0xA80~0xA8E)
 ******************************************************************/
#define  reg_postbyr_nr_ctl 			(0x1A80 + REG_HIGH)
#define  reg_postbyr_beseg			(0x1A80 + REG_LOW)
#define  reg_byr_nr_filter1_rate10 		(0x1A84 + REG_HIGH)
#define  reg_byr_nr_filter1_rate32 		(0x1A84 + REG_LOW)
#define  reg_byr_nr_filter1_ratex4 		(0x1A88 + REG_HIGH)
#define  reg_postbyr_filter2_rate10		(0x1A88 + REG_LOW)
#define  reg_postbyr_nr_filter2_rate32		(0x1A8C + REG_HIGH)
#define  reg_postbyr_nr_filte2_ratex4		(0x1A8C + REG_LOW)

#define  rst_postbyr_nr_ctl 			(0x0003)
#define  rst_postbyr_beseg			(0x3020)
#define  rst_byr_nr_filter1_rate10 		(0x80FF)
#define  rst_byr_nr_filter1_rate32 		(0x2040)
#define  rst_byr_nr_filter1_ratex4 		(0x0010)
#define  rst_postbyr_filter2_rate10		(0x1420)
#define  rst_postbyr_nr_filter2_rate32		(0x070C)
#define  rst_postbyr_nr_filte2_ratex4		(0x0304)


/******************************************************************
 * Post Bayer Sharpness (1page 0xAB0~0xABA)
 ******************************************************************/
#define  reg_postbyr_sharp_ctl			(0x1AB0 + REG_HIGH)
#define  reg_postbyr_sharp_rate 		(0x1AB0 + REG_LOW)
#define  reg_postbyr_sharp_std_gain10		(0x1AB4 + REG_HIGH)
#define  reg_postbyr_sharp_std_gain32		(0x1AB4 + REG_LOW)
#define  reg_postbyr_sharp_std_gainx4		(0x1AB8 + REG_HIGH)
#define  reg_postbyr_sharp_coreval		(0x1AB8 + REG_LOW)

#define  rst_postbyr_sharp_ctl			(0x0000)
#define  rst_postbyr_sharp_rate 		(0x1810)
#define  rst_postbyr_sharp_std_gain10		(0x0400)
#define  rst_postbyr_sharp_std_gain32		(0x190c)
#define  rst_postbyr_sharp_std_gainx4		(0x462d)
#define  rst_postbyr_sharp_coreval		(0x0000)


/******************************************************************
 * RGBInt (2page 0x000~0x060)
 ******************************************************************/
#define reg_rgb_nr_ctl				(0x2000 + REG_HIGH)
#define reg_rgb_beseg				(0x2000 + REG_LOW)
#define reg_rgb_moire				(0x2004 + REG_HIGH)
#define reg_rgb_NR_filter1_rate			(0x2004 + REG_LOW)
#define reg_rgb_intpo_rate0 			(0x2008 + REG_HIGH)
#define reg_rgb_intpo_rate1 			(0x2008 + REG_LOW)
#define reg_rgb_NR_filter2_dir_10		(0x200C + REG_HIGH)
#define reg_rgb_NR_filter2_dir_32		(0x200C + REG_LOW)
#define reg_rgb_NR_filter2_dir_x4		(0x2010 + REG_HIGH)
#define reg_rgb_NR_filter2_nondir_10		(0x2010 + REG_LOW)
#define reg_rgb_NR_filter2_nondir32		(0x2014 + REG_HIGH)
#define reg_rgb_NR_filter2_nondir_x4		(0x2014 + REG_LOW)

#define rst_rgb_nr_ctl				(0x0003)
#define rst_rgb_beseg				(0x3020)
#define rst_rgb_moire				(0x2001)
#define rst_rgb_linenr_rate 			(0x0008)
#define rst_rgb_intpo_rate0 			(0x0004)
#define rst_rgb_intpo_rate1 			(0x2020)
#define rst_rgb_NR_filter2_dir_10		(0x080a)
#define rst_rgb_NR_filter2_dir_32		(0x0305)
#define rst_rgb_NR_filter2_dir_x4		(0x0102)
#define rst_rgb_NR_filter2_nondir_10		(0x0406)
#define rst_rgb_NR_filter2_nondir32		(0x0203)
#define rst_rgb_NR_filter2_nondir_x4		(0x0101)


/******************************************************************
 * RGB Sharpness (2page 0x040~0x068)
 ******************************************************************/
#define  reg_rgb_sharp_ctl			(0x2040 + REG_HIGH)
#define  reg_rgb_sharp_rate 		 	(0x2040 + REG_LOW)
#define  reg_rgb_sharp_sel_rate 		(0x2044 + REG_HIGH)
#define  reg_rgb_sharp_coreval			(0x2044 + REG_LOW)
#define  reg_rgb_sharp_std_gain10		(0x2048 + REG_HIGH)
#define  reg_rgb_sharp_std_gain32		(0x2048 + REG_LOW)
#define  reg_rgb_sharp_std_gainx4		(0x204C + REG_HIGH)
#define  reg_rgb_sharp_lum_gain10		(0x204C + REG_LOW)
#define  reg_rgb_sharp_lum_gain32		(0x2050 + REG_HIGH)
#define  reg_rgb_sharp_lum_gainx4		(0x2050 + REG_LOW)
#define  reg_rgb_sharp_gainlmt			(0x2054 + REG_HIGH)

#define  rst_rgb_sharp_ctl			(0x0001)
#define  rst_rgb_sharp_rate 			(0x0603)
#define  rst_rgb_sharp_sel_rate 		(0x0000)
#define  rst_rgb_sharp_coreval			(0x0005)
#define  rst_rgb_sharp_std_gain10		(0x4940)
#define  rst_rgb_sharp_std_gain32		(0x4f4c)
#define  rst_rgb_sharp_std_gainx4		(0x6050)
#define  rst_rgb_sharp_lum_gain10		(0x4940)
#define  rst_rgb_sharp_lum_gain32		(0x4f4c)
#define  rst_rgb_sharp_lum_gainx4		(0x0050)
#define  rst_rgb_sharp_gainlmt			(0x207f)


/******************************************************************
 * Y NR(Noise Reduction) Define (2page 0x070~0x0A8)
 ******************************************************************/
#define reg_y_nr_ctl				(0x2070 + REG_HIGH)
#define reg_y_nr_filter1_ctl0			(0x2070 + REG_LOW)
#define reg_y_nr_filter1_ctl1			(0x2074 + REG_HIGH)
#define reg_y_nr_filter2_ctl			(0x2074 + REG_LOW)
#define reg_y_nr_h_gain10			(0x2078 + REG_HIGH)
#define reg_y_nr_h_gain32			(0x2078 + REG_LOW)
#define reg_y_nr_v_gain10			(0x207C + REG_HIGH)
#define reg_y_nr_v_gain32			(0x207C + REG_LOW)
#define reg_y_nr_h_ofs10			(0x2080 + REG_HIGH)
#define reg_y_nr_h_ofs32			(0x2080 + REG_LOW)
#define reg_y_nr_v_ofs10			(0x2084 + REG_HIGH)
#define reg_y_nr_v_ofs32			(0x2084 + REG_LOW)

#define rst_y_nr_ctl				(0x0303)
#define rst_y_nr_filter1_ctl0			(0x0100)
#define rst_y_nr_filter1_ctl1			(0x0404)
#define rst_y_nr_filter2_ctl			(0x0404)
#define rst_y_nr_h_gain10			(0x0A0A)
#define rst_y_nr_h_gain32			(0x0A0A)
#define rst_y_nr_v_gain10			(0x0404)
#define rst_y_nr_v_gain32			(0x0404)
#define rst_y_nr_h_ofs10			(0x0C0C)
#define rst_y_nr_h_ofs32			(0x0C0C)
#define rst_y_nr_v_ofs10			(0x0608)
#define rst_y_nr_v_ofs32			(0x0405)


/******************************************************************
 * Y Contrast (2page 0x0A8~0x0B0)
 ******************************************************************/
#define reg_y_contrast_ctl			(0x20A8 + REG_HIGH)
#define reg_y_cont_ax				(0x20A8 + REG_LOW)
#define reg_y_cont_b				(0x20AC + REG_HIGH)

#define rst_y_contrast_ctl			(0x0000)
#define rst_y_cont_ax				(0x161e)
#define rst_y_cont_b				(0x3c1e)


/******************************************************************
 * CNR(Chroma Noise Reduction) (2page 0x0B8~0x108)
 ******************************************************************/
#define  reg_yuv_cnr_ctl			(0x20B8 + REG_HIGH)
#define  reg_yuv_cnr_wtprt_ctl			(0x20B8 + REG_LOW)

#define  reg_yuv_filter1_control		(0x20BC + REG_HIGH)
#define  reg_yuv_filter1_maxmin 		(0x20BC + REG_LOW)
#define  reg_yuv_filter1_extention_threshold 	(0x20C0 + REG_HIGH)
#define  reg_yuv_cnr_filter2_ct			(0x20C0 + REG_LOW)
#define  reg_yuv_cnr_filter2_wgt10		(0x20C4 + REG_HIGH)
#define  reg_yuv_cnr_filter2_wgt32		(0x20C4 + REG_LOW)
#define  reg_yuv_cnr_filter2_ofs10		(0x20C8 + REG_HIGH)
#define  reg_yuv_cnr_filter2_ofs32		(0x20C8 + REG_LOW)

#define  rst_yuv_cnr_ctl			(0x000f)
#define  rst_yuv_cnr_wtprt_ctl			(0x0003)
#define  rst_yuv_filter1_control		(0x2008)
#define  rst_yuv_filter1_maxmin			(0xff00)
#define  rst_yuv_filter1_extention_threshold	(0x0023)
#define  rst_yuv_cnr_filter2_ct			(0x0508)
#define  rst_yuv_cnr_filter2_wgt10		(0x0304)
#define  rst_yuv_cnr_filter2_wgt32		(0x0202)
#define  rst_yuv_cnr_filter2_ofs10		(0x0101)
#define  rst_yuv_cnr_filter2_ofs32		(0x0202)


/******************************************************************
 * Y Sharpness (2page 0x0E0~0x108)
 ******************************************************************/
#define  reg_y_sharp_ctl			(0x20E0 + REG_HIGH)
#define  reg_y_sharp_coreval			(0x20E0 + REG_LOW)
#define  reg_y_sharp_rate			(0x20E4 + REG_HIGH)
#define  reg_y_sharp_dy_gain10			(0x20E4 + REG_LOW)
#define  reg_y_sharp_dy_gain32			(0x20E8 + REG_HIGH)
#define  reg_y_sharp_dy_gain54			(0x20E8 + REG_LOW)
#define  reg_y_sharp_dy_gain76			(0x20EC + REG_HIGH)
#define  reg_y_sharp_lum_gain10 		(0x20EC + REG_LOW)
#define  reg_y_sharp_lum_gain32 		(0x20F0 + REG_HIGH)
#define  reg_y_sharp_lum_gain54 		(0x20F0 + REG_LOW)
#define  reg_y_sharp_lmt			(0x20F4 + REG_HIGH)

#define  rst_y_sharp_ctl			(0x0201)
#define  rst_y_sharp_coreval			(0x0003)
#define  rst_y_sharp_rate			(0x1010)
#define  rst_y_sharp_dy_gain10			(0x7e80)
#define  rst_y_sharp_dy_gain32			(0x6473)
#define  rst_y_sharp_dy_gain54			(0x284b)
#define  rst_y_sharp_dy_gain76			(0x0014)
#define  rst_y_sharp_lum_gain10 		(0x7e80)
#define  rst_y_sharp_lum_gain32 		(0x6473)
#define  rst_y_sharp_lum_gain54 		(0x004b)
#define  rst_y_sharp_lmt			(0x007f)


/******************************************************************
 * RGB Color Correction Matrix (2page 0x500~0x530)
 ******************************************************************/
#define reg_rgb_ccm_ctl 			(0x2500 + REG_HIGH)
#define reg_rgb_ccm_rr_val			(0x2500 + REG_LOW)
#define reg_rgb_ccm_rg_val			(0x2504 + REG_HIGH)
#define reg_rgb_ccm_rb_val			(0x2504 + REG_LOW)
#define reg_rgb_ccm_gr_val			(0x2508 + REG_HIGH)
#define reg_rgb_ccm_gg_val			(0x2508 + REG_LOW)
#define reg_rgb_ccm_gb_val			(0x250C + REG_HIGH)
#define reg_rgb_ccm_br_val			(0x250C + REG_LOW)
#define reg_rgb_ccm_bg_val			(0x2510 + REG_HIGH)
#define reg_rgb_ccm_bb_val			(0x2510 + REG_LOW)

#define rst_rgb_ccm_ctl 			(0x007F)
#define rst_rgb_ccm_rr_val			(0x0119)
#define rst_rgb_ccm_rg_val			(0x0019)
#define rst_rgb_ccm_rb_val			(0x0000)
#define rst_rgb_ccm_gr_val			(0x0044)
#define rst_rgb_ccm_gg_val			(0x0164)
#define rst_rgb_ccm_gb_val			(0x0020)
#define rst_rgb_ccm_br_val			(0x0000)
#define rst_rgb_ccm_bg_val			(0x0060)
#define rst_rgb_ccm_bb_val			(0x0160)


/******************************************************************
 * YUV Saturation Control (2page 0x600~0x61C)
 ******************************************************************/
#define reg_yuv_sat_ctl 			(0x2600 + REG_HIGH)
#define reg_yuv_sat_cb_gain			(0x2600 + REG_LOW)
#define reg_yuv_sat_cr_gain			(0x2604 + REG_HIGH)
#define reg_yuv_sat_graph_10			(0x2604 + REG_LOW)
#define reg_yuv_sat_graph_32			(0x2608 + REG_HIGH)
#define reg_yuv_sat_graph_54			(0x2608 + REG_LOW)
#define reg_yuv_sat_core			(0x260C + REG_HIGH)

#define rst_yuv_sat_ctl 			(0x0003)
#define rst_yuv_sat_cb_gain_m			(0x0040)
#define rst_yuv_sat_cr_gain_m			(0x0040)
#define rst_yuv_sat_graph_10			(0x4040)
#define rst_yuv_sat_graph_32			(0x4040)
#define rst_yuv_sat_graph_54			(0x4040)
#define rst_yuv_sat_core			(0x0000)


/******************************************************************
 * RGB Multi-Color Enhancement (2page 0x700~0x754)
 ******************************************************************/
#define reg_rgb_clren_ctl			(0x2700 + REG_HIGH)
#define reg_rgb_r_sat				(0x2700 + REG_LOW)
#define reg_rgb_g_sat				(0x2704 + REG_HIGH)
#define reg_rgb_b_sat				(0x2704 + REG_LOW)
#define reg_rgb_y_sat				(0x2708 + REG_HIGH)
#define reg_rgb_c_sat				(0x2708 + REG_LOW)
#define reg_rgb_m_sat				(0x270C + REG_HIGH)
#define reg_rgb_r_hue				(0x270C + REG_LOW)
#define reg_rgb_g_hue				(0x2710 + REG_HIGH)
#define reg_rgb_b_hue				(0x2710 + REG_LOW)
#define reg_rgb_y_hue				(0x2714 + REG_HIGH)
#define reg_rgb_c_hue				(0x2714 + REG_LOW)
#define reg_rgb_m_hue				(0x2718 + REG_HIGH)
#define reg_rgb_r_lum				(0x2718 + REG_LOW)
#define reg_rgb_g_lum				(0x271C + REG_HIGH)
#define reg_rgb_b_lum				(0x271C + REG_LOW)
#define reg_rgb_y_lum				(0x2720 + REG_HIGH)
#define reg_rgb_c_lum				(0x2720 + REG_LOW)
#define reg_rgb_m_lum				(0x2724 + REG_HIGH)

#define rst_rgb_clren_ctl			(0x000F)
#define rst_rgb_r_sat				(0x0020)
#define rst_rgb_g_sat				(0x0020)
#define rst_rgb_b_sat				(0x0020)
#define rst_rgb_y_sat				(0x0120)
#define rst_rgb_c_sat				(0x0120)
#define rst_rgb_m_sat				(0x0120)
#define rst_rgb_r_hue				(0x0000)
#define rst_rgb_g_hue				(0x0000)
#define rst_rgb_b_hue				(0x0000)
#define rst_rgb_y_hue				(0x0000)
#define rst_rgb_c_hue				(0x0000)
#define rst_rgb_m_hue				(0x0000)
#define rst_rgb_r_lum				(0x0000)
#define rst_rgb_g_lum				(0x0000)
#define rst_rgb_b_lum				(0x0000)
#define rst_rgb_y_lum				(0x0000)
#define rst_rgb_c_lum				(0x0000)
#define rst_rgb_m_lum				(0x0000)


/******************************************************************
 * WDR (2page 0x810~0x8FF)
 ******************************************************************/
#define reg_wdr_ctl0				(0x2810 + REG_HIGH)
#define reg_wdr_cfg0				(0x2810 + REG_LOW)
#define reg_wdr_cfg1				(0x2814 + REG_HIGH)
#define reg_wdr_gain_c0 			(0x2814 + REG_LOW)
#define reg_wdr_gain_c1 			(0x2818 + REG_HIGH)
#define reg_wdr_gain_c2 			(0x2818 + REG_LOW)
#define reg_wdr_gain_c3 			(0x281C + REG_HIGH)
#define reg_wdr_gain_c4 			(0x281C + REG_LOW)
#define reg_wdr_gain_c5 			(0x2820 + REG_HIGH)
#define reg_wdr_gain_c6 			(0x2820 + REG_LOW)
#define reg_wdr_gain_c7 			(0x2824 + REG_HIGH)
#define reg_wdr_gain_c8 			(0x2824 + REG_LOW)
#define reg_wdr_gain_c9 			(0x2828 + REG_HIGH)
#define reg_wdr_gain_w0 			(0x2828 + REG_LOW)
#define reg_wdr_gain_w1 			(0x282C + REG_HIGH)
#define reg_wdr_gain_w2 			(0x282C + REG_LOW)
#define reg_wdr_gain_w3 			(0x2830 + REG_HIGH)
#define reg_wdr_gain_w4 			(0x2830 + REG_LOW)
#define reg_wdr_gain_w5 			(0x2834 + REG_HIGH)
#define reg_wdr_gain_w6 			(0x2834 + REG_LOW)
#define reg_wdr_gain_w7 			(0x2838 + REG_HIGH)
#define reg_wdr_gain_w8 			(0x2838 + REG_LOW)
#define reg_wdr_gain_w9 			(0x283C + REG_HIGH)
#define reg_wdr_gm_gain_c0			(0x283C + REG_LOW)
#define reg_wdr_gm_gain_c1			(0x2840 + REG_HIGH)
#define reg_wdr_gm_gain_c2			(0x2840 + REG_LOW)
#define reg_wdr_gm_gain_c3			(0x2844 + REG_HIGH)
#define reg_wdr_gm_gain_c4			(0x2844 + REG_LOW)
#define reg_wdr_gm_gain_w0			(0x2848 + REG_HIGH)
#define reg_wdr_gm_gain_w1			(0x2848 + REG_LOW)
#define reg_wdr_gm_gain_w2			(0x284C + REG_HIGH)
#define reg_wdr_gm_gain_w3			(0x284C + REG_LOW)
#define reg_wdr_gm_gain_w4			(0x2850 + REG_HIGH)
#define reg_wdr_replc_rate			(0x2850 + REG_LOW)
#define reg_wdr_replc_lum_gain_c0		(0x2854 + REG_HIGH)
#define reg_wdr_replc_lum_gain_c1		(0x2854 + REG_LOW)
#define reg_wdr_replc_lum_gain_c2		(0x2858 + REG_HIGH)
#define reg_wdr_replc_lum_gain_c3		(0x2858 + REG_LOW)
#define reg_wdr_replc_lum_gain_c4		(0x285C + REG_HIGH)
#define reg_wdr_replc_lum_gain_w0		(0x285C + REG_LOW)
#define reg_wdr_replc_lum_gain_w1		(0x2860 + REG_HIGH)
#define reg_wdr_replc_lum_gain_w2		(0x2860 + REG_LOW)
#define reg_wdr_replc_lum_gain_w3		(0x2864 + REG_HIGH)
#define reg_wdr_replc_lum_gain_w4		(0x2864 + REG_LOW)
#define reg_wdr_replc_lum2_gain_w0		(0x2868 + REG_HIGH)
#define reg_wdr_replc_lum2_gain_w1		(0x2868 + REG_LOW)
#define reg_wdr_replc_lum2_gain_w2		(0x286C + REG_HIGH)
#define reg_wdr_replc_lum2_gain_w3		(0x286C + REG_LOW)
#define reg_wdr_replc_lum2_gain_w4		(0x2870 + REG_HIGH)
#define reg_wdr_cfg2				(0x2870 + REG_LOW)
#define reg_wdr_vsync_width_l			(0x2874 + REG_LOW)
#define reg_wdr_vsync_width_h			(0x2878 + REG_HIGH)
#define reg_wdr_vsync_margin			(0x2878 + REG_LOW)
#define reg_wdr_img_width			(0x287C + REG_HIGH)
#define reg_wdr_img_height			(0x287C + REG_LOW)
#define reg_wdr_soi 				(0x2880 + REG_HIGH)
#define reg_wdr_cfg3				(0x2880 + REG_LOW)

#define rst_wdr_ctl0				(0x0000)
#define rst_wdr_cfg0				(0x0406)
#define rst_wdr_cfg1				(0x400a)
#define rst_wdr_gain_c0 			(0x0000)
#define rst_wdr_gain_c1 			(0x0000)
#define rst_wdr_gain_c2 			(0x0000)
#define rst_wdr_gain_c3 			(0x0000)
#define rst_wdr_gain_c4 			(0x0000)
#define rst_wdr_gain_c5 			(0x0000)
#define rst_wdr_gain_c6 			(0x0000)
#define rst_wdr_gain_c7 			(0x0064)
#define rst_wdr_gain_c8 			(0x0352)
#define rst_wdr_gain_c9 			(0x03ff)
#define rst_wdr_gain_w0 			(0x03ff)
#define rst_wdr_gain_w1 			(0x03ff)
#define rst_wdr_gain_w2 			(0x03ff)
#define rst_wdr_gain_w3 			(0x03ff)
#define rst_wdr_gain_w4 			(0x03ff)
#define rst_wdr_gain_w5 			(0x03ff)
#define rst_wdr_gain_w6 			(0x03ff)
#define rst_wdr_gain_w7 			(0x03ff)
#define rst_wdr_gain_w8 			(0x0000)
#define rst_wdr_gain_w9 			(0x0000)
#define rst_wdr_gm_gain_c0			(0x0000)
#define rst_wdr_gm_gain_c1			(0x0064)
#define rst_wdr_gm_gain_c2			(0x0096)
#define rst_wdr_gm_gain_c3			(0x0190)
#define rst_wdr_gm_gain_c4			(0x03ff)
#define rst_wdr_gm_gain_w0			(0x03ff)
#define rst_wdr_gm_gain_w1			(0x03ff)
#define rst_wdr_gm_gain_w2			(0x0200)
#define rst_wdr_gm_gain_w3			(0x0000)
#define rst_wdr_gm_gain_w4			(0x0000)
#define rst_wdr_replc_rate			(0xff)
#define rst_wdr_replc_lum_gain_c0		(0)
#define rst_wdr_replc_lum_gain_c1		(100)
#define rst_wdr_replc_lum_gain_c2		(320)
#define rst_wdr_replc_lum_gain_c3		(800)
#define rst_wdr_replc_lum_gain_c4		(1023)
#define rst_wdr_replc_lum_gain_w0		(1023)
#define rst_wdr_replc_lum_gain_w1		(1023)
#define rst_wdr_replc_lum_gain_w2		(1023)
#define rst_wdr_replc_lum_gain_w3		(1023)
#define rst_wdr_replc_lum_gain_w4		(0)
#define rst_wdr_replc_lum2_gain_w0		(0)
#define rst_wdr_replc_lum2_gain_w1		(0)
#define rst_wdr_replc_lum2_gain_w2		(0)
#define rst_wdr_replc_lum2_gain_w3		(512)
#define rst_wdr_replc_lum2_gain_w4		(1023)
#define rst_wdr_cfg2				(1120)
#define rst_wdr_img_width			(0x0784)
#define rst_wdr_img_height			(0x043f)
#define rst_wdr_vsync_width_l			(0xca80)
#define rst_wdr_vsync_width_h			(0x0000)
#define rst_wdr_vsync_margin			(0x03ff)
#define rst_wdr_soi 				(0x0000)
#define rst_wdr_cfg3				(0x4100)


/******************************************************************
 * HDR (2page 0x900~0x96C)
 ******************************************************************/
#define HDR_MODE_INTERLEAVED			(0)
#define HDR_MODE_COMPANDING			(1)
#define HDR_MODE_NONE				(2)

#define reg_hdr_ctl 				(0x2900 + REG_HIGH)
#define reg_hist_update_rate			(0x2900 + REG_LOW)
#define reg_low_gain_max_rate			(0x2904 + REG_HIGH)
#define reg_region_gain_min 			(0x2904 + REG_LOW)
#define reg_region_gain_max 			(0x2908 + REG_HIGH)
#define reg_low_scene_gain_min			(0x2908 + REG_LOW)
#define reg_low_scene_gain_max			(0x290C + REG_HIGH)
#define reg_hdr_off_mid_rate			(0x290C + REG_LOW)
#define reg_min_hdr_gain			(0x2910 + REG_HIGH)
#define reg_max_hdr_gain			(0x2910 + REG_LOW)
#define reg_hdr_init_val			(0x2914 + REG_HIGH)
#define reg_hdr_rate				(0x2914 + REG_LOW)
#define reg_hdr_black_gain_min_rate 		(0x2918 + REG_HIGH)
#define reg_hdr_black_gain_max_rate 		(0x2918 + REG_LOW)
#define reg_lowY_gain_rate			(0x291C + REG_HIGH)
#define reg_single_region_st			(0x291C + REG_LOW)
#define reg_single_rate 			(0x2920 + REG_HIGH)
#define reg_single_hdr_rate 			(0x2920 + REG_LOW)
#define reg_hdr_img_width			(0x2924 + REG_HIGH)
#define reg_hdr_img_height			(0x2924 + REG_LOW)
#define reg_hdr_gain_0_ro			(0x2928 + REG_HIGH)
#define reg_hdr_gain_1_ro			(0x2928 + REG_LOW)
#define reg_hdr_gain_2_ro			(0x292C + REG_HIGH)
#define reg_hdr_gain_3_ro			(0x292C + REG_LOW)
#define reg_hdr_gain_4_ro			(0x2930 + REG_HIGH)
#define reg_hdr_gain_5_ro			(0x2930 + REG_LOW)
#define reg_hdr_gain_6_ro			(0x2934 + REG_HIGH)
#define reg_hdr_gain_7_ro			(0x2934 + REG_LOW)

#define rst_hdr_ctl 				(0x0000)
#define rst_hist_update_rate			(0x000f)
#define rst_low_gain_max_rate			(0x00ff)
#define rst_region_gain_min 			(0x0000)
#define rst_region_gain_max 			(0x00ff)
#define rst_low_scene_gain_min			(0x000a)
#define rst_low_scene_gain_max			(0x00ff)
#define rst_hdr_off_mid_rate			(0x0020)
#define rst_min_hdr_gain			(0x0000)
#define rst_max_hdr_gain			(0x00ff)
#define rst_hdr_init_val			(0x0000)
#define rst_hdr_rate				(0x0006)
#define rst_hdr_black_gain_min_rate 		(0x0004)
#define rst_hdr_black_gain_max_rate 		(0x0010)
#define rst_lowY_gain_rate			(0x0010)
#define rst_single_region_st			(0x0003)
#define rst_single_rate 			(0x00c4)
#define rst_single_hdr_rate 			(0x0040)
#define rst_hdr_img_width			(0x0780)
#define rst_hdr_img_height			(0x0437)


/******************************************************************
 * I2C Master Control (2page 0xB00~0xB14)
 ******************************************************************/
/* TODO: REG_HIGH OR REG_LOW*/
#define reg_i2c_mst_ctl 			(0x2B00 + REG_HIGH)
#define reg_i2c_mst_enp 			(0x2B04 + REG_HIGH)
#define reg_i2c_mst_speed			(0x2B08 + REG_HIGH)
#define reg_i2c_mst_sub_addr			(0x2B0C + REG_HIGH)
#define reg_i2c_mst_wdata			(0x2B10 + REG_HIGH)
#define reg_i2c_mst_rdata			(0x2B14 + REG_HIGH)

#define rst_i2c_mst_ctl 			(0x0186)
#define rst_i2c_mst_enp 			(0x0000)
#define rst_i2c_mst_speed			(0x0000)
#define rst_i2c_mst_sub_addr			(0x0000)
#define rst_i2c_mst_wdata			(0x0000)


/******************************************************************
 * MSP430 Configuration (2page 0xB80~0xC00)
 ******************************************************************/
/* TODO: REG_HIGH OR REG_LOW*/
#define reg_msp430_ctl				(0x2B80 + REG_LOW)
#define reg_msp430_mem_ctl			(0x2BE0 + REG_LOW)

/* reg_msp430_ctl */
#define MSP430_CTL_INTERRUPT_CLEAR_SHIFT	(13)
#define MSP430_CTL_INTERRUPT_MASK_SHIFT		(7)
#define MSP430_CTL_UART_DOWNLOAD_SHIFT		(5)
#define MSP430_CTL_MSP430_DEBUG_ENABLE_SHIFT	(4)
#define MSP430_CTL_MSP430_CLOCK_DIVIDER_SHIFT	(2)
#define MSP430_CTL_MSP430_ENABLE_SHIFT		(0)

#define MSP430_CTL_INTERRUPT_CLEAR_MASK		(0x7)
#define MSP430_CTL_INTERRUPT_MASK_MASK		(0x3F)
#define MSP430_CTL_UART_DOWNLOAD_MASK		(0x1)
#define MSP430_CTL_MSP430_DEBUG_ENABLE_MASK	(0x1)
#define MSP430_CTL_MSP430_CLOCK_DIVIDER_MASK	(0x3)
#define MSP430_CTL_MSP430_ENABLE_MASK		(0x1)

/* reg_msp430_mem_ctl */
#define MSP430_MEM_CTL_DOWNLOAD_ENABLE_SHIFT	(0)
#define MSP430_MEM_CTL_HOLD_ENABLE_SHIFT	(1)
#define MSP430_MEM_CTL_READ_TIME_LATENCY_SHIFT	(2)

#define MSP430_MEM_CTL_DOWNLOAD_ENABLE_MASK	\
	(0x1 << MSP430_MEM_CTL_DOWNLOAD_ENABLE_SHIFT)
#define MSP430_MEM_CTL_HOLD_ENABLE_MASK		\
	(0x1 << MSP430_MEM_CTL_HOLD_ENABLE_SHIFT)
#define MSP430_MEM_CTL_READ_TIME_LATENCY_MASK	\
	(0x3 << MSP430_MEM_CTL_READ_TIME_LATENCY_SHIFT)

#define ISP_MEM_OFFSET_SFR			(0x0)
#define ISP_MEM_OFFSET_DATA_MEM			(0x200)
#define ISP_MEM_OFFSET_CODE_MEM			(0x3800)
#define ISP_MEM_SIZE_DATA_MEM			(14 * 1024)
#define ISP_MEM_SIZE_CODE_MEM			(50 * 1024)


/******************************************************************
 * ATI Configuration (2page 0xC00~0xC4E)
 ******************************************************************/
#define ATI_BASE_ADDR				(0x2C00)

#define reg_ati_i2c_s_ctl			(0x2C00 + REG_HIGH)
#define reg_ati_i2c_s_cfg			(0x2C00 + REG_LOW)
#define reg_ati_i2c_s_addr_0			(0x2C04 + REG_HIGH)
#define reg_ati_i2c_s_data_0			(0x2C04 + REG_LOW)
#define reg_ati_i2c_s_addr_1			(0x2C08 + REG_HIGH)
#define reg_ati_i2c_s_data_1			(0x2C08 + REG_LOW)
#define reg_ati_i2c_s_addr_2			(0x2C0C + REG_HIGH)
#define reg_ati_i2c_s_data_2			(0x2C0C + REG_LOW)
#define reg_ati_i2c_s_addr_3			(0x2C10 + REG_HIGH)
#define reg_ati_i2c_s_data_3			(0x2C10 + REG_LOW)
#define reg_ati_i2c_s_addr_4			(0x2C14 + REG_HIGH)
#define reg_ati_i2c_s_data_4			(0x2C14 + REG_LOW)
#define reg_ati_i2c_s_addr_5			(0x2C18 + REG_HIGH)
#define reg_ati_i2c_s_data_5			(0x2C18 + REG_LOW)
#define reg_ati_i2c_s_addr_6			(0x2C1C + REG_HIGH)
#define reg_ati_i2c_s_data_6			(0x2C1C + REG_LOW)
#define reg_ati_i2c_s_addr_7			(0x2C20 + REG_HIGH)
#define reg_ati_i2c_s_data_7			(0x2C20 + REG_LOW)
#define reg_ati_i2c_s_rdata_0			(0x2C24 + REG_HIGH)
#define reg_ati_i2c_s_rdata_1			(0x2C24 + REG_LOW)
#define reg_ati_i2c_s_rdata_2			(0x2C28 + REG_HIGH)
#define reg_ati_i2c_s_rdata_3			(0x2C28 + REG_LOW)
#define reg_ati_i2c_s_rdata_4			(0x2C2C + REG_HIGH)
#define reg_ati_i2c_s_rdata_5			(0x2C2C + REG_LOW)
#define reg_ati_i2c_s_rdata_6			(0x2C30 + REG_HIGH)
#define reg_ati_i2c_s_rdata_7			(0x2C30 + REG_LOW)
#define reg_ati_i2c_m_ctl			(0x2C44 + REG_HIGH)
#define reg_ati_i2c_m_addr			(0x2C44 + REG_LOW)
#define reg_ati_i2c_m_wdata_0			(0x2C48 + REG_HIGH)
#define reg_ati_i2c_m_wdata_1			(0x2C48 + REG_LOW)
#define reg_ati_i2c_m_rdata_0			(0x2C4C + REG_HIGH)
#define reg_ati_i2c_m_rdata_1			(0x2C4C + REG_LOW)

#define rst_ati_i2c_s_ctl			(0x00)
#define rst_ati_i2c_m_ctl			(0x00)
#define rst_ati_i2c_s_cfg			(0x00)
#define rst_ati_i2c_m_cfg			(0x00)
#define rst_ati_i2c_s_addr_0			(0x00)
#define rst_ati_i2c_s_data_0			(0x00)
#define rst_ati_i2c_s_addr_1			(0x00)
#define rst_ati_i2c_s_data_1			(0x00)
#define rst_ati_i2c_s_addr_2			(0x00)
#define rst_ati_i2c_s_data_2			(0x00)
#define rst_ati_i2c_s_addr_3			(0x00)
#define rst_ati_i2c_s_data_3			(0x00)
#define rst_ati_i2c_s_addr_4			(0x00)
#define rst_ati_i2c_s_data_4			(0x00)
#define rst_ati_i2c_s_addr_5			(0x00)
#define rst_ati_i2c_s_data_5			(0x00)
#define rst_ati_i2c_s_addr_6			(0x00)
#define rst_ati_i2c_s_data_6			(0x00)
#define rst_ati_i2c_s_addr_7			(0x00)
#define rst_ati_i2c_s_data_7			(0x00)
#define rst_ati_i2c_m_addr			(0x00)
#define rst_ati_i2c_m_wdata 			(0x00)


/******************************************************************
 * DMA Configuration (4page 0x000~0x01C)
 ******************************************************************/
#define reg_isp_wdma_ctl0			(0x4000 + REG_HIGH)
#define reg_isp_wdma_cfg0			(0x4000 + REG_LOW)
#define reg_isp_wdma_cfg1			(0x4004 + REG_HIGH)
#define reg_isp_wdma_b0_p0adr_0 		(0x4008 + REG_HIGH)
#define reg_isp_wdma_b0_p0adr_1 		(0x4008 + REG_LOW)
#define reg_isp_wdma_b0_p1adr_0 		(0x400C + REG_HIGH)
#define reg_isp_wdma_b0_p1adr_1 		(0x400C + REG_LOW)
#define reg_isp_wdma_b0_p2adr_0			(0x4010 + REG_HIGH)
#define reg_isp_wdma_b0_p2adr_1			(0x4012 + REG_LOW)


/******************************************************************
 * H/W Statistics
 ******************************************************************/
#define REG_HW_STATISTICS			(0x8000)
#define REG_HW_STATISTICS_R_SUM			(0x8000)
#define REG_HW_STATISTICS_G_SUM			(0x8400)
#define REG_HW_STATISTICS_B_SUM			(0x8800)
#define REG_HW_STATISTICS_Y_AVERAGE		(0x8C00)
#define REG_HW_STATISTICS_RG_RATIO		(0x9000)
#define REG_HW_STATISTICS_BG_RATIO		(0x9400)

#define REG_ANN_FLAG				(0x2C1C + REG_HIGH)
#define ANN_FLAG_WDR_SHIFT			(0) 
#define ANN_FLAG_AWB_SHIFT			(8)
#define ANN_FLAG_WDR_MASK			(0x3)
#define ANN_FLAG_AWB_MASK			(0x7)

#define REG_ANN_AWB_APPLY			(0x250C + REG_LOW)
#define ANN_AWB_APPLY_SHIFT			(0)
#define ANN_AWB_APPLY_MASK			(0x1)

#endif
