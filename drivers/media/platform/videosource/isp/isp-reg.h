// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __ISP_H__
#define __ISP_H__

#define ISP_0					(0)
#define ISP_1					(1)
#define ISP_2					(2)
#define ISP_3					(3)
#define ISP_MAX					(4)

#define ISP_ENTITY_IP				(0)
#define ISP_ENTITY_MEM				(1)
#define ISP_ENTITY_MAX				(2)

#define ISP_MEM_OFFSET_SFR			(0x0)
#define ISP_MEM_OFFSET_DMEM			(0x200)
#define ISP_MEM_OFFSET_PMEM			(0x3800)

#define reg_sleep_mode_ctl			(0x0004)
#define reg_soft_reset				(0x0008)
#define reg_parity_chk_reset			(0x000C)

#define reg_img_size_width			(0x0010)
#define reg_img_size_height			(0x0014)
#define reg_img_done_height			(0x0018)

#define reg_update_ctl				(0x0030)
#define reg_update_sel1				(0x0034)
#define reg_update_sel2				(0x0038)
#define reg_update_mod1				(0x003C)
#define reg_update_mod2				(0x0040)
#define reg_frame_ctl				(0x0044)
#define reg_update_user_cnt1			(0x0048)
#define reg_update_user_cnt2			(0x004C)

#define reg_intr_mask				(0x0060)
#define reg_intr_clear				(0x0064)
#define reg_intr_status 			(0x0068)

#define reg_misr_ctl				(0x0070) 	  // 13
#define reg_misr_byr_seed1			(0x0074)
#define reg_misr_byr_seed2			(0x0078)
#define reg_misr_rgb_seed1			(0x007C)
#define reg_misr_rgb_seed2			(0x0080)
#define reg_misr_yuv_seed1			(0x0084)
#define reg_misr_yuv_seed2			(0x0088)
#define reg_misr_byr_rdata1 			(0x008C)
#define reg_misr_byr_rdata2 			(0x0090)
#define reg_misr_rgb_rdata1 			(0x0094)
#define reg_misr_rgb_rdata2 			(0x0098)
#define reg_misr_yuv_rdata1 			(0x009C)
#define reg_misr_yuv_rdata2 			(0x00A0)

#define rst_sleep_mode_ctl			(0x0000)
#define rst_soft_reset				(0x0000)
#define rst_parity_chk_reset			(0x0000)

#define rst_img_size_width			(0x0CC0)
#define rst_img_size_height 			(0x0990)
#define rst_img_done_height 			(0x044B)
// 256x320 = (0x01000140),	3M 2048x1536 = (0x08000600)

#define rst_update_ctl				(0x0000)
#define rst_update_sel1 			(0x0000)
#define rst_update_sel2 			(0x0400)
#define rst_update_mod1 			(0x0000)
#define rst_update_mod2 			(0x0000)
#define rst_frame_ctl				(0x0000)
#define rst_update_user_cnt1			(0x0000)
#define rst_update_user_cnt2			(0x0100)

#define rst_intr_mask				(0xFFFF)
#define rst_intr_clear				(0x0000)
//#define rst_intr_flag 			(0x0000)

#define rst_misr_ctl				(0x1080)
#define rst_misr_byr_seed1			(0x010A)
#define rst_misr_byr_seed2			(0x8001)
#define rst_misr_rgb_seed1			(0x010A)
#define rst_misr_rgb_seed2			(0x8001)
#define rst_misr_yuv_seed1			(0x010A)
#define rst_misr_yuv_seed2			(0x8001)


/*
================================================================================
=== Image Format Window Control  ( 1 Page 0x000 ~ 0x0FF )
================================================================================
*/
#define reg_imgout_window_ctl			(0x1000)
#define reg_imgout_window_x_start		(0x1004)  // 13
#define reg_imgout_window_y_start		(0x1008)  // 13
#define reg_imgout_window_x_width		(0x100C)  // 13
#define reg_imgout_window_y_width		(0x1010)  // 13
#define reg_imgout_window_cfg			(0x1014)

#define reg_imgin_order_ctl 			(0x1020)

#define reg_ptrngen_ctl 			(0x1030)
#define reg_ptrngen_idx_ctl 			(0x1034)
#define reg_ptrngen_rand			(0x1038)
#define reg_ptrngen_seed			(0x103C)
#define reg_ptrngen_frm_wid 			(0x1040)
#define reg_ptrngen_frm_hgt 			(0x1044)
#define reg_ptrngen_frm_str 			(0x1048)
#define reg_ptrngen_frm_end 			(0x104C)
#define reg_ptrngen_img_stx 			(0x1050)
#define reg_ptrngen_img_sty 			(0x1054)
#define reg_ptrngen_img_enx 			(0x1058)
#define reg_ptrngen_img_eny 			(0x105C)

#define reg_imgout_gpu_window_ctl		(0x1060)
#define reg_imgout_gpu_window_x_start		(0x1064)  // 13
#define reg_imgout_gpu_window_y_start		(0x1068)  // 13
#define reg_imgout_gpu_window_x_width		(0x106C)  // 13
#define reg_imgout_gpu_window_y_width		(0x1070)  // 13
#define reg_imgout_gpu_window_cfg		(0x1074)

#define reg_deblank_ctl 			(0x1084)
#define reg_deblank_cfg 			(0x1088)
#define reg_deblank_wid_ro			(0x108C)
#define reg_deblank_hgt_ro			(0x108F)

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

#define rst_imgout_gpu_window_ctl		(0x0000)
#define rst_imgout_gpu_window_x_start		(0x0000)
#define rst_imgout_gpu_window_y_start		(0x0000)
#define rst_imgout_gpu_window_x_width		(0x0CC0)
#define rst_imgout_gpu_window_y_width		(0x0990)
#define rst_imgout_gpu_window_cfg		(0x0005)

#define rst_deblank_ctl 			(0x0000)
#define rst_deblank_cfg 			(0x1F12)

/*
================================================================================
=== AF Window Control & Filter Read  ( 1 Page 0x100 ~ 0x16C )
================================================================================
*/
#define reg_af_win1_xm_start			(0x1100)  //0x1100
#define reg_af_win1_ym_start			(0x1102)  //0x1104
#define reg_af_win1_xm_end			(0x1104)  //0x1108
#define reg_af_win1_ym_end			(0x1106)  //0x110C
#define reg_af_win2_xm_start			(0x1108)  //0x1110
#define reg_af_win2_ym_start			(0x110A)  //0x1114
#define reg_af_win2_xm_end			(0x110C)  //0x1118
#define reg_af_win2_ym_end			(0x110E)  //0x111C
#define reg_af_win3_xm_start			(0x1110)  //0x1120
#define reg_af_win3_ym_start			(0x1112)  //0x1124
#define reg_af_win3_xm_end			(0x1114)  //0x1128
#define reg_af_win3_ym_end			(0x1116)  //0x112C
#define reg_af_win4_xm_start			(0x1118)  //0x1130
#define reg_af_win4_ym_start			(0x111A)  //0x1134
#define reg_af_win4_xm_end			(0x111C)  //0x1138
#define reg_af_win4_ym_end			(0x111E)  //0x113C
#define reg_af_win5_xm_start			(0x1120)  //0x1140
#define reg_af_win5_ym_start			(0x1122)  //0x1144
#define reg_af_win5_xm_end			(0x1124)  //0x1148
#define reg_af_win5_ym_end			(0x1126)  //0x114C
#define reg_af_win6_xm_start			(0x1128)  //0x1150
#define reg_af_win6_ym_start			(0x112A)  //0x1154
#define reg_af_win6_xm_end			(0x112C)  //0x1158
#define reg_af_win6_ym_end			(0x112E)  //0x115C

#define rst_af_win1_xm_start			(0x0000)
#define rst_af_win1_ym_start			(0x0000)
#define rst_af_win1_xm_end			(0x0000)
#define rst_af_win1_ym_end			(0x0000)
#define rst_af_win2_xm_start			(0x0000)
#define rst_af_win2_ym_start			(0x0000)
#define rst_af_win2_xm_end			(0x0000)
#define rst_af_win2_ym_end			(0x0000)
#define rst_af_win3_xm_start			(0x0000)
#define rst_af_win3_ym_start			(0x0000)
#define rst_af_win3_xm_end			(0x0000)
#define rst_af_win3_ym_end			(0x0000)
#define rst_af_win4_xm_start			(0x0000)
#define rst_af_win4_ym_start			(0x0000)
#define rst_af_win4_xm_end			(0x0000)
#define rst_af_win4_ym_end			(0x0000)
#define rst_af_win5_xm_start			(0x0000)
#define rst_af_win5_ym_start			(0x0000)
#define rst_af_win5_xm_end			(0x0000)
#define rst_af_win5_ym_end			(0x0000)
#define rst_af_win6_xm_start			(0x0000)
#define rst_af_win6_ym_start			(0x0000)
#define rst_af_win6_xm_end			(0x0000)
#define rst_af_win6_ym_end			(0x0000)

/*
==================	 Read-Only Registers  ========================
=== AF Window Filter DATA Read	( 1 Page 0x170 ~ 0x1D0 )
==================	 Read-Only Registers  ========================
*/

#define  reg_af_data1_h_hi			(0x1170)  //0x1170
#define  reg_af_data1_h_lo			(0x1172)  //0x1174
#define  reg_af_data2_h_hi			(0x1174)  //0x1178
#define  reg_af_data2_h_lo			(0x1176)  //0x117C
#define  reg_af_data3_h_hi			(0x1178)  //0x1180
#define  reg_af_data3_h_lo			(0x117A)  //0x1184
#define  reg_af_data4_h_hi			(0x117C)  //0x1188
#define  reg_af_data4_h_lo			(0x117E)  //0x118C
#define  reg_af_data5_h_hi			(0x1180)  //0x1190
#define  reg_af_data5_h_lo			(0x1182)  //0x1194
#define  reg_af_data6_h_hi			(0x1184)  //0x1198
#define  reg_af_data6_h_lo			(0x1186)  //0x119C
#define  reg_af_data1_m_hi			(0x1188)  //0x11A0
#define  reg_af_data1_m_lo			(0x118A)  //0x11A4
#define  reg_af_data2_m_hi			(0x118C)  //0x11A8
#define  reg_af_data2_m_lo			(0x118E)  //0x11AC
#define  reg_af_data3_m_hi			(0x1190)  //0x11B0
#define  reg_af_data3_m_lo			(0x1192)  //0x11B4
#define  reg_af_data4_m_hi			(0x1194)  //0x11B8
#define  reg_af_data4_m_lo			(0x1196)  //0x11BC
#define  reg_af_data5_m_hi			(0x1198)  //0x11C0
#define  reg_af_data5_m_lo			(0x119A)  //0x11C4
#define  reg_af_data6_m_hi			(0x119C)  //0x11C8
#define  reg_af_data6_m_lo			(0x119E)  //0x11CC

#define  reg_pxlcount_ro		 	(0x11D0)

/*
================================================================================
=== Tri-AUTO Register Control ( 1 Page 0x200 ~ 0x210 )
================================================================================
*/
#define reg_triauto_ctl 			(0x1200) //0x1200
#define reg_triauto_cfg1			(0x1202) //0x1204
#define reg_triauto_cfg2			(0x1204) //0x1208
#define reg_af_ctl				(0x1206) //0x120C
#define reg_triauto_status_ro			(0x1208) //0x1210 // RO,  Nick 12.23 2240 --> 2270

#define rst_triauto_ctl 			(0x0000)
#define rst_triauto_cfg1			(0x0A23)
#define rst_triauto_cfg2			(0x140A)
#define rst_af_ctl				(0x0000)
////=== Page-8,9,A,B  Reserved Address for Tri-Auto Memory Access ( 0x8000 ~ 0xBFFF )
#define reg_triauto_mem 			(0x8000) // not check

/*
================================================================================
=== Bayer Channel Gain Register Define	(1Page 0x300~0x3FF)
================================================================================
*/
#define reg_chgain_ctl1 			(0x1300) //0x1300
#define reg_PrePosAgain_b_pr			(0x1302) //0x1304
#define reg_PrePosAgain_gb_pr			(0x1304) //0x1308
#define reg_PrePosAgain_gr_pr			(0x1306) //0x130C
#define reg_PrePosAgain_r_pr			(0x1308) //0x1310
#define reg_chMgain_mr				(0x130A) //0x1314
#define reg_chMgain_mb				(0x130C) //0x1318
#define reg_chMgain_mgr 			(0x130E) //0x131C
#define reg_chMgain_mgb 			(0x1310) //0x1320

/* reg_chgain_ctl1 */
#define CHGAIN_CTL_ON_OFF_SHIFT			(0)
#define CHGAIN_CTL_ENABLE_GLOBAL_OFFSET_SHIFT	(1)
#define CHGAIN_CTL_B_SIGN_SHIFT			(4)
#define CHGAIN_CTL_GB_SIGN_SHIFT		(5)
#define CHGAIN_CTL_GR_SIGN_SHIFT		(6)
#define CHGAIN_CTL_R_SIGN_SHIFT			(7)

#define CHGAIN_CTL_ON_OFF_MASK			((0x1) << CHGAIN_CTL_ON_OFF_SHIFT)
#define CHGAIN_CTL_ENABLE_GLOBAL_OFFSET_MASK	((0X1) << CHGAIN_CTL_ENABLE_GLOBAL_OFFSET_SHIFT)
#define CHGAIN_CTL_B_SIGN_MASK			((0X1) << CHGAIN_CTL_B_SIGN_SHIFT)
#define CHGAIN_CTL_GB_SIGN_MASK			((0X1) << CHGAIN_CTL_GB_SIGN_SHIFT)
#define CHGAIN_CTL_GR_SIGN_MASK			((0X1) << CHGAIN_CTL_GR_SIGN_SHIFT)
#define CHGAIN_CTL_R_SIGN_MASK			((0X1) << CHGAIN_CTL_R_SIGN_SHIFT)

/* RESET VALUE */
#define rst_chgain_ctl1 			(0x0001)
#define rst_PrePosAgain_b_pr			(0x0000)
#define rst_PrePosAgain_gb_pr			(0x0000)
#define rst_PrePosAgain_gr_pr			(0x0000)
#define rst_PrePosAgain_r_pr			(0x0000)
#define rst_chMgain_mr				(0x0040)
#define rst_chMgain_mb				(0x0040)
#define rst_chMgain_mgr 			(0x0040)
#define rst_chMgain_mgb 			(0x0040)

/*
================================================================================
=== Bayer AWB Gain Register Define	(1Page 0x400~0x4FF)
================================================================================
*/
#define reg_wbgain_ctl				(0x1400) //0x1400
#define reg_wbgain_r				(0x1402) //0x1404
#define reg_wbgain_g				(0x1404) //0x1408
#define reg_wbgain_b				(0x1406) //0x140C

#define rst_wbgain_ctl				(0x0001)
#define rst_wbgain_r				(0x0100)
#define rst_wbgain_g				(0x0100)
#define rst_wbgain_b				(0x0100)

/*
================================================================================
=== Bayer AE Gain Register Define  (1Page 0x500~0x5FF)
================================================================================
*/
#define reg_aegain_ctl				(0x1500) //0x1500
#define reg_taegain 				(0x1502) //0x1504

#define rst_aegain_ctl				(0x0001)
#define rst_taegain 				(0x0100)

/*
================================================================================
=== YUV Channel Gain Register Define  (1Page 0x600~0x6FF)
================================================================================
*/
//#define reg_yuvchgain_ctl 			(0x1600)
//#define reg_yuvchgain_gain			(0x1604)

// YP = 0x370*Y/0x400 + 0x40
// CP = 0x384*C/0x400 + 0x40
//#define rst_yuvchgain_ctl 			(0x0000)
//#define rst_yuvchgain_gain			(0x4040)

/*
================================================================================
=== Bayer Curve Gain   (1 Page 0x800~0x8FF)
================================================================================
*/
#define reg_byrcrv_ctl				(0x1800)  //0x1800
#define reg_byrcrvRGB_0 			(0x1802)  //0x1804
#define reg_byrcrvRGB_1 			(0x1804)  //0x1808
#define reg_byrcrvRGB_2 			(0x1806)  //0x180C
#define reg_byrcrvRGB_3 			(0x1808)  //0x1810
#define reg_byrcrvRGB_4 			(0x180A)  //0x1814
#define reg_byrcrvRGB_5 			(0x180C)  //0x1818
#define reg_byrcrvRGB_6 			(0x180E)  //0x181C
#define reg_byrcrvRGB_7 			(0x1810)  //0x1820
#define reg_byrcrvRGB_8 			(0x1812)  //0x1824
#define reg_byrcrvRGB_9 			(0x1814)  //0x1828
#define reg_byrcrvRGB_10			(0x1816)  //0x182C
#define reg_byrcrvRGB_11			(0x1818)  //0x1830
#define reg_byrcrvRGB_12			(0x181A)  //0x1834
#define reg_byrcrvRGB_13			(0x181C)  //0x1838
#define reg_byrcrvRGB_14			(0x181E)  //0x183C
#define reg_byrcrvRGB_15			(0x1820)  //0x1840
#define reg_byrcrvRGB_16			(0x1822)  //0x1844
#define reg_byrcrvRGB_17			(0x1824)  //0x1848
#define reg_byrcrvRGB_18			(0x1826)  //0x184C
#define reg_byrcrvRGB_19			(0x1828)  //0x1850

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


/*
================================================================================
=== DCPD(De-Companding)   (1 Page 0x860~0x8FF)
================================================================================
*/
/*
 * De-Companding BASE
 */
#define reg_dcpd_ctl				(0x1860)
#define reg_dcpd_crv_0				(0x1862)
#define reg_dcpd_crv_1				(0x1864)
#define reg_dcpd_crv_2				(0x1866)
#define reg_dcpd_crv_3				(0x1868)
#define reg_dcpd_crv_4				(0x186A)
#define reg_dcpd_crv_5				(0x186C)
#define reg_dcpd_crv_6				(0x186E)
#define reg_dcpd_crv_7				(0x1870)

#define reg_dcpdx_0 				(0x1872)
#define reg_dcpdx_1 				(0x1874)
#define reg_dcpdx_2 				(0x1876)
#define reg_dcpdx_3 				(0x1878)
#define reg_dcpdx_4 				(0x187A)
#define reg_dcpdx_5 				(0x187C)
#define reg_dcpdx_6 				(0x187E)
#define reg_dcpdx_7 				(0x1880)

/*
 * De-Companding reset value
 */
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

//=======================================================================================
//=== Bayer Shading Correctin Register Define ( 1Page 0x900 ~ 0x9FF )
//=======================================================================================
#define reg_shading_ctl_h			(0x1900)  //0x1900
#define reg_shading_ctl_l			(0x1902)  //0x1904
#define reg_shading_x_inc			(0x1904)  //0x1908
#define reg_shading_y_inc			(0x1906)  //0x190C
#define reg_shading_gain			(0x1908)  //0x1910
#define reg_shading_rd_addr 			(0x1910)
#define reg_shading_rd_dat1 			(0x1914)  //RO
#define reg_shading_rd_dat2 			(0x1916)  //RO
#define reg_shading_rd_ctrl 			(0x1918)

#define rst_shading_ctl_h			(0x0000)  //0x0000
#define rst_shading_ctl_l			(0x0000)  //0x003E
#define rst_shading_x_inc			(0x0080)  //0x00A0
#define rst_shading_y_inc			(0x0065)  //0x0078
#define rst_shading_gain			(0x0400)  //0x0400
#define rst_shading_rd_addr 			(0x0000)
//#define rst_shading_rd_dat1			(0x0000)
//#define rst_shading_rd_dat2			(0x0000)
#define rst_shading_rd_ctrl 			(0x0000)
//=======================================================================================
//=== DPC Register Define (1Page 0xA00 ~ 0xA0F)
//=======================================================================================
#define  reg_prebyr_dpc_ctl 			(0x1A00)  //0x1A00
#define  reg_postbyr_dpc_ctl			(0x1A02)  //0x1A04

#define  rst_prebyr_dpc_ctl 			(0x0001)
#define  rst_postbyr_dpc_ctl			(0x0001)

//=======================================================================================
//=== Gb/Gr Correction (4page 0xA10 ~ 0xA2F)
//=======================================================================================
#define  reg_gbgr_ctl				(0x1A10)  //0x1A10
#define  reg_gbgr_flat_rate10			(0x1A12)  //0x1A14
#define  reg_gbgr_flat_rate32			(0x1A14)  //0x1A18
#define  reg_gbgr_flat_ratex4			(0x1A16)  //0x1A1C

#define  rst_gbgr_ctl				(0x0001)
#define  rst_gbgr_flat_rate10			(0x4000)
#define  rst_gbgr_flat_rate32			(0xC476)
#define  rst_gbgr_flat_ratex4			(0x00FF)

//=======================================================================================
//=== Bayer Pre-Process Register Define  (3Page 0xXXX~0xXXX)
//=======================================================================================
//
//=======================================================================================
//=== Pre-Bayer NR (1Page 0xA30 ~ 0xA4F)
//=======================================================================================
#define reg_prebyr_nr_ctl			(0x1A30)  //0x1A30
#define reg_byr_nr_std_gain10			(0x1A32)  //0x1A34
#define reg_byr_nr_std_gain32			(0x1A34)  //0x1A38
#define reg_byr_nr_std_gainx4			(0x1A36)  //0x1A3C

#define rst_prebyr_nr_ctl			(0x0001)
#define rst_byr_nr_std_gain10			(0x0000)
#define rst_byr_nr_std_gain32			(0x0000)
#define rst_byr_nr_std_gainx4			(0x0000)

//=======================================================================================
//=== Pre-Bayer Sharp (1Page 0xA50 ~ 0xA7F)
//=======================================================================================
#define  reg_prebyr_sharp_ctl			(0x1A50)  //0x1A50
#define  reg_prebyr_sharp_rbgain		(0x1A52)  //0x1A54
#define  reg_prebyr_sharp_ggain 		(0x1A54)  //0x1A58
#define  reg_prebyr_sharp_std10 		(0x1A56)  //0x1A5C
#define  reg_prebyr_sharp_std32 		(0x1A58)  //0x1A60
#define  reg_prebyr_sharp_stdx4 		(0x1A5A)  //0x1A64
#define  reg_prebyr_sharp_coreval		(0x1A5C)  //0x1A68

#define  rst_prebyr_sharp_ctl			(0x0002)
#define  rst_prebyr_sharp_rbgain		(0x1014)
#define  rst_prebyr_sharp_ggain 		(0x0018)
#define  rst_prebyr_sharp_std10 		(0x0400)
#define  rst_prebyr_sharp_std32 		(0x200F)
#define  rst_prebyr_sharp_stdx4 		(0x5a3a)
#define  rst_prebyr_sharp_coreval		(0x000A)

//=======================================================================================
//=== Bayer Post-Process Register Define  (3Page 0xXXX~0xXXX)
//=======================================================================================
//
//=======================================================================================
//=== Post-Bayer NR (1Page 0xA80 ~ 0xAAF)
//=======================================================================================
#define  reg_postbyr_nr_ctl 			(0x1A80)  //0x1A80
#define  reg_postbyr_beseg			(0x1A82)  //0x1A84
#define  reg_byr_flat_nr_rate10 		(0x1A84)  //0x1A88
#define  reg_byr_flat_nr_rate32 		(0x1A86)  //0x1A8C
#define  reg_byr_flat_nr_ratex4 		(0x1A88)  //0x1A90
#define  reg_postbyr_nr_rate_lum10		(0x1A8A)  //0x1A94
#define  reg_postbyr_nr_rate_lum32		(0x1A8C)  //0x1A98
#define  reg_postbyr_nr_rate_lumx4		(0x1A8E)  //0x1A9C

#define  rst_postbyr_nr_ctl 			(0x0003)
#define  rst_postbyr_beseg			(0x3020)
#define  rst_byr_flat_nr_rate10 		(0x80FF)
#define  rst_byr_flat_nr_rate32 		(0x2040)
#define  rst_byr_flat_nr_ratex4 		(0x0010)
#define  rst_postbyr_nr_rate_lum10		(0x1420)
#define  rst_postbyr_nr_rate_lum32		(0x070C)
#define  rst_postbyr_nr_rate_lumx4		(0x0304)

//=======================================================================================
//=== Post-Bayer Sharp (1Page 0xAB0 ~ 0xACF)
//=======================================================================================
#define  reg_postbyr_sharp_ctl			(0x1AB0)  //0x1AB0
#define  reg_postbyr_sharp_rate 		(0x1AB2)  //0x1AB4
#define  reg_postbyr_sharp_std_gain10		(0x1AB4)  //0x1AB8
#define  reg_postbyr_sharp_std_gain32		(0x1AB6)  //0x1ABC
#define  reg_postbyr_sharp_std_gainx4		(0x1AB8)  //0x1AC0
#define  reg_postbyr_sharp_coreval		(0x1ABA)  //0x1AC4

#define  rst_postbyr_sharp_ctl			(0x0000)
#define  rst_postbyr_sharp_rate 		(0x1810)
#define  rst_postbyr_sharp_std_gain10		(0x0400)
#define  rst_postbyr_sharp_std_gain32		(0x190c)
#define  rst_postbyr_sharp_std_gainx4		(0x462d)
#define  rst_postbyr_sharp_coreval		(0x0000)

//=======================================================================================
//=== RGB NR (2Page 0x000 ~ 0x03F)
//=======================================================================================
#define reg_rgb_nr_ctl				(0x2000)  //0x2000
#define reg_rgb_beseg				(0x2002)  //0x2004
#define reg_rgb_moire				(0x2004)  //0x2008
#define reg_rgb_linenr_rate 			(0x2006)  //0x200C
#define reg_rgb_intpo_rate0 			(0x2008)  //0x2010
#define reg_rgb_intpo_rate1 			(0x200A)  //0x2014
#define reg_rgb_line_awm_th10			(0x200C)  //0x2018
#define reg_rgb_line_awm_th32			(0x200E)  //0x201C
#define reg_rgb_line_awm_thx4			(0x2010)  //0x2020
#define reg_rgb_flat_awm_th10			(0x2012)  //0x2024
#define reg_rgb_flat_awm_th32			(0x2014)  //0x2028
#define reg_rgb_flat_awm_thx4			(0x2016)  //0x202C

#define rst_rgb_nr_ctl				(0x0003)
#define rst_rgb_beseg				(0x3020)
#define rst_rgb_moire				(0x2001)
#define rst_rgb_linenr_rate 			(0x0008)
#define rst_rgb_intpo_rate0 			(0x0004)
#define rst_rgb_intpo_rate1 			(0x2020)
#define rst_rgb_line_awm_th10			(0x080a)
#define rst_rgb_line_awm_th32			(0x0305)
#define rst_rgb_line_awm_thx4			(0x0102)
#define rst_rgb_flat_awm_th10			(0x0406)
#define rst_rgb_flat_awm_th32			(0x0203)
#define rst_rgb_flat_awm_thx4			(0x0101)

//=======================================================================================
//=== RGB Sharp (2Page 0x040 ~ 0x06F)
//=======================================================================================
#define  reg_rgb_sharp_ctl			(0x2040)  //0x2040
#define  reg_rgb_sharp_rate 		 	(0x2042)  //0x2044
#define  reg_rgb_sharp_sel_rate 		(0x2044)  //0x2048
#define  reg_rgb_sharp_coreval			(0x2046)  //0x204C
#define  reg_rgb_sharp_std_gain10		(0x2048)  //0x2050
#define  reg_rgb_sharp_std_gain32		(0x204A)  //0x2054
#define  reg_rgb_sharp_std_gainx4		(0x204C)  //0x2058
#define  reg_rgb_sharp_lum_gain10		(0x204E)  //0x205C
#define  reg_rgb_sharp_lum_gain32		(0x2050)  //0x2060
#define  reg_rgb_sharp_lum_gainx4		(0x2052)  //0x2064
#define  reg_rgb_sharp_gainlmt			(0x2054)  //0x2068

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

//=======================================================================================
//=== Y-NR (2Page 0x070 ~ 0x09F)
//=======================================================================================
#define reg_ypre_rgn_ctl			(0x2070)  //0x2070
#define reg_narr_nr_ctl0			(0x2072)  //0x2074
#define reg_narr_nr_ctl1			(0x2074)  //0x2078
#define reg_yuv_ynr_dc_ctl			(0x2076)  //0x207C
#define reg_yuv_ynr_hdc_wgt10			(0x2078)  //0x2080
#define reg_yuv_ynr_hdc_wgt32			(0x207A)  //0x2084
#define reg_yuv_ynr_vdc_wgt10			(0x207C)  //0x2088
#define reg_yuv_ynr_vdc_wgt32			(0x207E)  //0x208C`

#define reg_yuv_ynr_hdc_ofs10			(0x2080)  //0x2090
#define reg_yuv_ynr_hdc_ofs32			(0x2082)  //0x2094
#define reg_yuv_ynr_vdc_ofs10			(0x2084)  //0x2098
#define reg_yuv_ynr_vdc_ofs32			(0x2086)  //0x209C
#define reg_yuv_std_ctl 			(0x2088)  //0x20A0
#define reg_yuv_ynr_dc_std_th			(0x208A)  //0x20A4

#define reg_yuv_debug				(0x2FF0)

#define rst_ypre_rgn_ctl			(0x0303)
#define rst_narr_nr_ctl0			(0x0100)
#define rst_narr_nr_ctl1			(0x0404)
#define rst_yuv_ynr_dc_ctl			(0x0404)
#define rst_yuv_ynr_hdc_wgt10			(0x0A0A)
#define rst_yuv_ynr_hdc_wgt32			(0x0A0A)
#define rst_yuv_ynr_vdc_wgt10			(0x0404)
#define rst_yuv_ynr_vdc_wgt32			(0x0404)
#define rst_yuv_ynr_hdc_ofs10			(0x0C0C)
#define rst_yuv_ynr_hdc_ofs32			(0x0C0C)
#define rst_yuv_ynr_vdc_ofs10			(0x0608)
#define rst_yuv_ynr_vdc_ofs32			(0x0405)
#define rst_yuv_std_ctl 			(0x1000)
#define rst_yuv_ynr_dc_std_th			(0x004B)

//=======================================================================================
//=== Y-Contrast (2Page 0x0A8 ~ 0x0B0)
//=======================================================================================
#define reg_y_contrast_ctl			(0x20A8)  //0x20A8
#define reg_y_cont_ax				(0x20AA)  //0x20AC
#define reg_y_cont_b				(0x20AC)  //0x20B0

#define rst_y_contrast_ctl			(0x0000)

#define rst_y_cont_ax				(0x161e)
#define rst_y_cont_b				(0x3c1e)

//=======================================================================================
//=== C-Pre-Processing (Sharp/NR) (2Page 0x0B8 ~ 0x0DF)
//=======================================================================================
#define  reg_yuv_cnr_ctl			(0x20B8)  //0x20B8
#define  reg_yuv_cnr_wtprt_ctl			(0x20BA)  //0x20BC
#define  reg_yuv_awm_axb			(0x20BC)  //0x20C0
#define  reg_yuv_awm_maxmin 			(0x20BE)  //0x20C4
#define  reg_yuv_awm_sub_th 			(0x20C0)  //0x20C8
#define  reg_yuv_cnr_dc_ctl 			(0x20C2)  //0x20CC
#define  reg_yuv_cnr_dc_wgt10			(0x20C4)  //0x20D0
#define  reg_yuv_cnr_dc_wgt32			(0x20C6)  //0x20D4
#define  reg_yuv_cnr_dc_ofs10			(0x20C8)  //0x20D8
#define  reg_yuv_cnr_dc_ofs32			(0x20CA)  //0x20DC

#define  rst_yuv_cnr_ctl			(0x000f)
#define  rst_yuv_cnr_wtprt_ctl			(0x0003)
#define  rst_yuv_awm_axb			(0x2008)
#define  rst_yuv_awm_maxmin 			(0xff00)
#define  rst_yuv_awm_sub_th 			(0x0023)
#define  rst_yuv_cnr_dc_ctl 			(0x0508)
#define  rst_yuv_cnr_dc_wgt10			(0x0304)
#define  rst_yuv_cnr_dc_wgt32			(0x0202)
#define  rst_yuv_cnr_dc_ofs10			(0x0101)
#define  rst_yuv_cnr_dc_ofs32			(0x0202)

//=======================================================================================
//=== Y-Preprocessing Sharpness (2Page 0x0E0 ~ 0x0FF)
//=======================================================================================
#define  reg_y_sharp_ctl			(0x20E0)  //0x20E0
#define  reg_y_sharp_coreval			(0x20E2)  //0x20E4
#define  reg_y_sharp_rate			(0x20E4)  //0x20E8
#define  reg_y_sharp_dy_gain10			(0x20E6)  //0x20EC
#define  reg_y_sharp_dy_gain32			(0x20E8)  //0x20F0
#define  reg_y_sharp_dy_gain54			(0x20EA)  //0x20F4
#define  reg_y_sharp_dy_gain76			(0x20EC)  //0x20F8
#define  reg_y_sharp_lum_gain10 		(0x20EE)  //0x20FC
#define  reg_y_sharp_lum_gain32 		(0x20F0)  //0x2100
#define  reg_y_sharp_lum_gain54 		(0x20F2)  //0x2104
#define  reg_y_sharp_lmt			(0x20F4)  //0x2108

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

//=======================================================================================
//=== RGB Color Correction Matrix Register ( 2Page 0x500 ~ 5FF)
//=======================================================================================
#define reg_rgb_ccm_ctl 			(0x2500)  //0x2500
#define reg_rgb_ccm_rr_val			(0x2502)  //0x2504
#define reg_rgb_ccm_rg_val			(0x2504)  //0x2508
#define reg_rgb_ccm_rb_val			(0x2506)  //0x250C
#define reg_rgb_ccm_gr_val			(0x2508)  //0x2510
#define reg_rgb_ccm_gg_val			(0x250A)  //0x2514
#define reg_rgb_ccm_gb_val			(0x250C)  //0x2518
#define reg_rgb_ccm_br_val			(0x250E)  //0x251C
#define reg_rgb_ccm_bg_val			(0x2510)  //0x2520
#define reg_rgb_ccm_bb_val			(0x2512)  //0x2524
#define reg_int_ps_ctl				(0x2514)  //0x2528
#define reg_int_ps_gain 			(0x2516)  //0x252C
#define reg_int_ps_lmt				(0x2518)  //0x2530

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

#define rst_int_ps_ctl				(0x0003)
#define rst_int_ps_gain 			(0x0002)
#define rst_int_ps_lmt				(0x0056)
//=======================================================================================
//=== YUV Saturation/Hue Contrast/Brightness ( 2Page 0x600 ~ 6FF)
//=======================================================================================
#define reg_yuv_sat_ctl 			(0x2600)  //0x2600
#define reg_yuv_sat_cb_gain_m			(0x2602)  //0x2604
#define reg_yuv_sat_cr_gain_m			(0x2604)  //0x2608
#define reg_yuv_sat_graph_10			(0x2606)  //0x260C
#define reg_yuv_sat_graph_32			(0x2608)  //0x2610
#define reg_yuv_sat_graph_54			(0x260A)  //0x2614
#define reg_yuv_sat_core			(0x260C)  //0x2618
#define reg_yuv_sat_etc_ctl 			(0x260E)  //0x261C

#define rst_yuv_sat_ctl 			(0x0003)
#define rst_yuv_sat_cb_gain_m			(0x0040)
#define rst_yuv_sat_cr_gain_m			(0x0040)
#define rst_yuv_sat_graph_10			(0x4040)
#define rst_yuv_sat_graph_32			(0x4040)
#define rst_yuv_sat_graph_54			(0x4040)
#define rst_yuv_sat_core			(0x0000)
#define rst_yuv_sat_etc_ctl 			(0x0202)
//=======================================================================================
//=== RGB Color Enhancement ( 2Page 0x700 ~ 7FF)
//=======================================================================================

#define reg_rgb_clren_ctl			(0x2700)  //0x2700
#define reg_rgb_r_sat				(0x2702)  //0x2704
#define reg_rgb_g_sat				(0x2704)  //0x2708
#define reg_rgb_b_sat				(0x2706)  //0x270C
#define reg_rgb_y_sat				(0x2708)  //0x2710
#define reg_rgb_c_sat				(0x270A)  //0x2714
#define reg_rgb_m_sat				(0x270C)  //0x2718
#define reg_rgb_r_hue				(0x270E)  //0x271C
#define reg_rgb_g_hue				(0x2710)  //0x2720
#define reg_rgb_b_hue				(0x2712)  //0x2724
#define reg_rgb_y_hue				(0x2714)  //0x2728
#define reg_rgb_c_hue				(0x2716)  //0x272C
#define reg_rgb_m_hue				(0x2718)  //0x2730
#define reg_rgb_r_lum				(0x271A)  //0x2734
#define reg_rgb_g_lum				(0x271C)  //0x2738
#define reg_rgb_b_lum				(0x271E)  //0x273C
#define reg_rgb_y_lum				(0x2720)  //0x2740
#define reg_rgb_c_lum				(0x2722)  //0x2744
#define reg_rgb_m_lum				(0x2724)  //0x2748
#define reg_rgb_lum_sat_ctl 			(0x2726)  //0x274C
#define reg_rgb_lum_tri_ctl0			(0x2728)  //0x2750
#define reg_rgb_lum_tri_ctl1			(0x272A)  //0x2754

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

#define rst_rgb_lum_sat_ctl 			(0x0000)
#define rst_rgb_lum_tri_ctl0			(0x0000)
#define rst_rgb_lum_tri_ctl1			(0x0000)

//=======================================================================================
//=== WDR  [tone_map] ( 2Page 0x800 ~ 7FF)
//=======================================================================================
#define reg_wdr_ctl0				(0x2810)
#define reg_wdr_cfg0				(0x2812)
#define reg_wdr_cfg1				(0x2814)
#define reg_wdr_gain_c0 			(0x2816)
#define reg_wdr_gain_c1 			(0x2818)
#define reg_wdr_gain_c2 			(0x281a)
#define reg_wdr_gain_c3 			(0x281c)
#define reg_wdr_gain_c4 			(0x281e)

#define reg_wdr_gain_c5 			(0x2820)
#define reg_wdr_gain_c6 			(0x2822)
#define reg_wdr_gain_c7 			(0x2824)
#define reg_wdr_gain_c8 			(0x2826)
#define reg_wdr_gain_c9 			(0x2828)
#define reg_wdr_gain_w0 			(0x282a)
#define reg_wdr_gain_w1 			(0x282c)
#define reg_wdr_gain_w2 			(0x282e)

#define reg_wdr_gain_w3 			(0x2830)
#define reg_wdr_gain_w4 			(0x2832)
#define reg_wdr_gain_w5 			(0x2834)
#define reg_wdr_gain_w6 			(0x2836)
#define reg_wdr_gain_w7 			(0x2838)
#define reg_wdr_gain_w8 			(0x283a)
#define reg_wdr_gain_w9 			(0x283c)
#define reg_wdr_gm_gain_c0			(0x283e)

#define reg_wdr_gm_gain_c1			(0x2840)
#define reg_wdr_gm_gain_c2			(0x2842)
#define reg_wdr_gm_gain_c3			(0x2844)
#define reg_wdr_gm_gain_c4			(0x2846)
#define reg_wdr_gm_gain_w0			(0x2848)
#define reg_wdr_gm_gain_w1			(0x284a)
#define reg_wdr_gm_gain_w2			(0x284c)
#define reg_wdr_gm_gain_w3			(0x284e)

#define reg_wdr_gm_gain_w4			(0x2850)
#define reg_wdr_replc_rate			(0x2852)
#define reg_wdr_replc_lum_gain_c0		(0x2854)
#define reg_wdr_replc_lum_gain_c1		(0x2856)
#define reg_wdr_replc_lum_gain_c2		(0x2858)
#define reg_wdr_replc_lum_gain_c3		(0x285a)
#define reg_wdr_replc_lum_gain_c4		(0x285c)
#define reg_wdr_replc_lum_gain_w0		(0x285e)

#define reg_wdr_replc_lum_gain_w1		(0x2860)
#define reg_wdr_replc_lum_gain_w2		(0x2862)
#define reg_wdr_replc_lum_gain_w3		(0x2864)
#define reg_wdr_replc_lum_gain_w4		(0x2866)
#define reg_wdr_replc_lum2_gain_w0		(0x2868)
#define reg_wdr_replc_lum2_gain_w1		(0x286a)
#define reg_wdr_replc_lum2_gain_w2		(0x286c)
#define reg_wdr_replc_lum2_gain_w3		(0x286e)

#define reg_wdr_replc_lum2_gain_w4		(0x2870)
#define reg_wdr_cfg2				(0x2872)
#define reg_wdr_crc_val 			(0x2874)
#define reg_wdr_vsync_width_l			(0x2876)
#define reg_wdr_vsync_width_h			(0x2878)
#define reg_wdr_vsync_margin			(0x287a)
#define reg_wdr_img_width			(0x287c)
#define reg_wdr_img_height			(0x287e)

#define reg_wdr_soi 				(0x2880)
#define reg_wdr_cfg3				(0x2882)

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

//=======================================================================================
//=== Contrast Enhancement [HDR] ( 2Page 0x900 ~ 7FF)
//=======================================================================================

#define reg_hdr_ctl 				(0x2900)
#define reg_hist_update_rate			(0x2902)
#define reg_low_gain_max_rate			(0x2904)
#define reg_region_gain_min 			(0x2906)
#define reg_region_gain_max 			(0x2908)
#define reg_low_scene_gain_min			(0x290a)
#define reg_low_scene_gain_max			(0x290c)
#define reg_hdr_off_mid_rate			(0x290e)

#define reg_min_hdr_gain			(0x2910)
#define reg_max_hdr_gain			(0x2912)
#define reg_hdr_init_val			(0x2914)
#define reg_hdr_rate				(0x2916)
#define reg_hdr_black_gain_min_rate 		(0x2918)
#define reg_hdr_black_gain_max_rate 		(0x291a)
#define reg_lowY_gain_rate			(0x291c)
#define reg_single_region_st			(0x291e)

#define reg_single_rate 			(0x2920)
#define reg_single_hdr_rate 			(0x2922)
#define reg_hdr_img_width			(0x2924)
#define reg_hdr_img_height			(0x2926)
#define reg_hdr_gain_0_ro			(0x2928)
#define reg_hdr_gain_1_ro			(0x292a)
#define reg_hdr_gain_2_ro			(0x292c)
#define reg_hdr_gain_3_ro			(0x292e)

#define reg_hdr_gain_4_ro			(0x2930)
#define reg_hdr_gain_5_ro			(0x2932)
#define reg_hdr_gain_6_ro			(0x2934)
#define reg_hdr_gain_7_ro			(0x2936)

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

//=======================================================================================
//=== I2C Master & Test Control Address ( 0x2B00~ 0x2B7F )
//=======================================================================================
#define reg_i2c_mst_ctl 			(0x2B00)
#define reg_i2c_mst_enp 			(0x2B04)
#define reg_i2c_mst_speed			(0x2B08)
#define reg_i2c_mst_sub_addr			(0x2B0C)
#define reg_i2c_mst_wdata			(0x2B10)
#define reg_i2c_mst_rdata			(0x2B14)

#define rst_i2c_mst_ctl 			(0x0186)
#define rst_i2c_mst_enp 			(0x0000)
#define rst_i2c_mst_speed			(0x0000)
#define rst_i2c_mst_sub_addr			(0x0000)
#define rst_i2c_mst_wdata			(0x0000)
//#define rst_i2c_mst_rdata 			(0x0000)  // RO
//=======================================================================================
//=== MSP430 Configuration	(0x2B80~0x2BFF)
//=======================================================================================
#define reg_msp430_ctl				(0x2B80)

#define reg_msp430_mem_ctl			(0x2BE0)
#define reg_msp430_cmem_addr			(0x2BF0)
#define reg_msp430_cmem_data			(0x2BF4)
#define reg_msp430_dmem_addr			(0x2BF8)
#define reg_msp430_dmem_data			(0x2BFC)

#define reg_msp430_gpio_1			(0x2B90)
#define reg_msp430_gpio_2			(0x2B94)

#define reg_nn_to_msp430_ro 			(0x2BA0)  // not use
#define reg_msp430_dbg_ro			(0x2BA4)

#define rst_msp430_ctl				(0x0000)

#define rst_msp430_mem_ctl			(0x0000)
#define rst_msp430_cmem_addr			(0x0000)
#define rst_msp430_cmem_data			(0x0000)
#define rst_msp430_dmem_addr			(0x0000)
#define rst_msp430_dmem_data			(0x0000)

#define rst_msp430_gpio_1			(0x0000)
#define rst_msp430_gpio_2			(0x0000)

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

#define MSP430_MEM_CTL_DOWNLOAD_ENABLE_MASK	(0x1 << MSP430_MEM_CTL_DOWNLOAD_ENABLE_SHIFT)
#define MSP430_MEM_CTL_HOLD_ENABLE_MASK		(0x1 << MSP430_MEM_CTL_HOLD_ENABLE_SHIFT)
#define MSP430_MEM_CTL_READ_TIME_LATENCY_MASK	(0x3 << MSP430_MEM_CTL_READ_TIME_LATENCY_SHIFT)

//=======================================================================================
//=== APB to I2C Master(Tele) (Page C 0x2C00~0x2C4E)
//=======================================================================================

#define ATI_BASE_ADDR				(0x2C00)

#define reg_ati_i2c_s_ctl			((0x00) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_cfg			((0x02) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_0			((0x04) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_0			((0x06) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_1			((0x08) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_1			((0x0a) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_2			((0x1c) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_2			((0x1e) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_3			((0x20) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_3			((0x22) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_4			((0x24) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_4			((0x26) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_5			((0x28) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_5			((0x2a) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_6			((0x2c) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_6			((0x2e) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_addr_7			((0x30) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_data_7			((0x32) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_0			((0x34) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_1			((0x36) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_2			((0x38) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_3			((0x3a) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_4			((0x3c) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_5			((0x3e) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_6			((0x40) + ATI_BASE_ADDR)
#define reg_ati_i2c_s_rdata_7			((0x42) + ATI_BASE_ADDR)
#define reg_ati_i2c_m_ctl			((0x44) + ATI_BASE_ADDR)
#define reg_ati_i2c_m_addr			((0x46) + ATI_BASE_ADDR)
#define reg_ati_i2c_m_wdata_0			((0x48) + ATI_BASE_ADDR)
#define reg_ati_i2c_m_wdata_1			((0x4a) + ATI_BASE_ADDR)
#define reg_ati_i2c_m_rdata_0			((0x4c) + ATI_BASE_ADDR)
#define reg_ati_i2c_m_rdata_1			((0x4e) + ATI_BASE_ADDR)

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


//=======================================================================================
//=== TP (Page F 0x010~0x040)
//=======================================================================================
#define reg_tp_ctl				(0x2F10)
#define reg_tp_lock_1				(0x2F20)
#define reg_tp_lock_2				(0x2F30)
#define reg_tp_lock_3				(0x2F40)

//=======================================================================================
//=== Shading Memory (Page 7 0x000~0x17F)
//=======================================================================================
//=== Page-6 Reserved Address for Shading Memory Access ( 0x7000 ~ 0x717F )
#define reg_shading_mem 			(0x7000)

//=======================================================================================
//=== DMA Configuration (Page 4 0x0x4000~0x0x401c)
//=======================================================================================
#define reg_isp_wdma_cfg1			(0x4000)

#define reg_isp_wdma_b0_p0adr_0 		(0x4008)
#define reg_isp_wdma_b0_p0adr_1 		(0x400a)

#define reg_isp_wdma_b0_p1adr_0 		(0x400c)
#define reg_isp_wdma_b0_p1adr_1 		(0x400e)

#define reg_isp_wdma_b0_p2adr_0			(0x4010)
#define reg_isp_wdma_b0_p2adr_1			(0x4012)

#endif
