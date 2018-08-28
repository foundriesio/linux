#ifndef __TCC_EDR_VPANEL_V1_H__
#define __TCC_EDR_VPANEL_V1_H__

/**
  @file tcc_edr_vpanel_v1.h. Adapted from file vp_edr_stb_source_display_management_v1.h in SMP8760.
  @brief Definitions of register structures. Module name: vp_edr_stb_source_display_management.
  @author Aurelia Popa-Radu.
*/

/* Known differences:
   SMP8760 interface is organized as Core = mainly composer and specific SMP8760 interface, and DisplayManagement = DM = video input, graphics input and output.
   TCC interface is organized as V_EDR = video path including composer, and V_PANEL = graphics and output.
   Note. SMP8760 register name prefix VsyncRegVpEdrStbSourceDisplayManagement was changed to TccEdrDM.
   Unclear registers/definitions were marked with //xx.
*/

//=========================================== osd1 ================================================

#include "tcc_edr_video_v1.h"

union TccEdrDMImapgUnknown00Reg { //xx ??
	struct {
		/** 00 */	RMuint32 rsvd0:32; // nonuse in current application, set as 0
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown01Reg { //xx ??
	struct {
		/** 00 */	RMuint32 rsvd0:32; // nonuse in current application, set as 0
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown02Reg { //xx ??
	struct {
		/** 00 */	RMuint32 rsvd0:31;  // nonuse in current application, set as 0
		/** 31 */	RMuint32 load_en:1; // nonuse in current application, set as 1
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown03Reg { //xx ??
	struct {
		/** 00 */	RMuint32 soft_arst:16;               // soft reset for vpanle, low actived
		/** 16 */	RMuint32 clk_bypass_icsc_u11:1;      // nonuse in current application, set as 0
		/** 17 */	RMuint32 clk_on_icsc_u11:1;          // nonuse in current application, set as 0
		/** 18 */	RMuint32 clk_bypass_dithering_u10:1; // nonuse in current application, set as 0
		/** 19 */	RMuint32 clk_on_dithering_u10:1;     // nonuse in current application, set as 0
		/** 20 */	RMuint32 clk_bypass_444_u9:1;        // 1'b1 bypass YUV444->YUV422 blending while clock off
		/** 21 */	RMuint32 clk_on_444_u9:1;            // 1'b1 enable clock on for YUV444->YUV422
		/** 22 */	RMuint32 clk_bypass_blend_u7:1;      // 1'b1 bypass OSD3 blending while clock off
		/** 23 */	RMuint32 clk_on_blend_u7:1;          // 1'b1 enable clock on for OSD3 blending
		/** 24 */	RMuint32 clk_bypass_blend_u5:1;      // 1'b1 bypass OSD1 blending while clock off
		/** 25 */	RMuint32 clk_on_blend_u5:1;          // 1'b1 enable clock on for OSD1 blending
		/** 26 */	RMuint32 rsvd0:2;
		/** 28 */	RMuint32 source_sel_u11:1;           // nonuse in current application, set as 0
		/** 29 */	RMuint32 dbus_osd3_enable:1;         // 1'b1 for normal work mode
		/** 30 */	RMuint32 dbus_osd1_enable:1;         // 1'b1 for normal work mode
		/** 31 */	RMuint32 dbus_vedr_enable:1;         // 1'b1 for normal work mode
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown04Reg { //xx u4=osd1 u6=osd3
	struct {
		/** 00 */	RMuint32 glb_clk_on:6;             // 6'h3f enable clock on for input mapping and cvm
		/** 06 */	RMuint32 glb_vs_latch_en:1;        // 1'b1 active all reg setting at vsync rising edge
		/** 07 */	RMuint32 rsvd0:1;
		/** 08 */	RMuint32 glb_cvmLut_cen:2;         // memory cen control, 2'b01 for normal work mode
		/** 10 */	RMuint32 glb_cvmLut_pd:1;          // memory power down, low actived
		/** 11 */	RMuint32 glb_cvmLut_lut_en:1;      // 1'b1 lut program enable
		/** 12 */	RMuint32 shadowContext_icsc_in:1;  // shadow LUT control for OSD3 cvm : OSD3 is a typo?
		/** 13 */	RMuint32 shadowContext_cvm_in:1;   // shadow LUT control for OSD1 input csc
		/** 14 */	RMuint32 rsvd1:14;
		/** 28 */	RMuint32 config_read_cycles_lut:2; // nonuse in current application, set as 0
		/** 30 */	RMuint32 config_read_cycles:2;     // nonuse in current application, set as 0
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown05Reg { //xx u4=osd1 u6=osd3
	struct {
		/** 00 */	RMuint32 glb_g2lLut_cen:2;     // G2L memory cen control, 2'b01 for normal work mode
		/** 02 */	RMuint32 glb_g2lLut_pd:1;      // memory power down, low actived
		/** 03 */	RMuint32 glb_g2lLut_lut_en:1;  // 1'b1 lut program enable
		/** 04 */	RMuint32 glb_g2lLut_lut_sel:3; // lut read back sel, 0 for R, 1 for G, 2 for B
		/** 07 */	RMuint32 rsvd0:1;
		/** 08 */	RMuint32 glb_l2pqLut_cen:2;    // L2PQ
		/** 10 */	RMuint32 glb_l2pqLut_pd:1;
		/** 11 */	RMuint32 glb_l2pqLut_lut_en:1;
		/** 12 */	RMuint32 glb_l2pqLut_lut_sel:3;
		/** 15 */	RMuint32 rsvd1:1;
		/** 16 */	RMuint32 glb_pq2lLut_cen:2;    // PQ2L
		/** 18 */	RMuint32 glb_pq2lLut_pd:1;
		/** 19 */	RMuint32 glb_pq2lLut_lut_en:1;
		/** 20 */	RMuint32 glb_pq2lLut_lut_sel:3;
		/** 23 */	RMuint32 rsvd2:9;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg00Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33yuv2rgb00:16;
		/** 16 */	RMuint32 ksimapgClr:8;
		/** 24 */	RMuint32 ksimapgM33yuv2rgbscale2p:5;
		/** 29 */	RMuint32 sw_osd_off_in_u4:1; // 1'b1 means without OSD1 input
		/** 30 */	RMuint32 rsvd0:2;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg01Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33yuv2rgb02:16;
		/** 16 */	RMuint32 ksimapgM33yuv2rgb01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg02Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33yuv2rgb11:16;
		/** 16 */	RMuint32 ksimapgM33yuv2rgb10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg03Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33yuv2rgb20:16;
		/** 16 */	RMuint32 ksimapgM33yuv2rgb12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg04Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33yuv2rgb22:16;
		/** 16 */	RMuint32 ksimapgM33yuv2rgb21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg05Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgV3yuv2rgboffinrgb0:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg06Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgV3yuv2rgboffinrgb1:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg07Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgV3yuv2rgboffinrgb2:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg08Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamRange:16;
		/** 16 */	RMuint32 ksimapgEotfparamRangemin:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg09Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamRangeinv:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg10Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamGamma:16;
		/** 16 */	RMuint32 ksimapgEotfparamEotf:8;
		/** 24 */	RMuint32 ksimapgEotfparamBdp:5;
		/** 29 */	RMuint32 rsvd0:3;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg11Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamB:16;
		/** 16 */	RMuint32 ksimapgEotfparamA:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg12Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamG:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg13Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33rgb2lms00:16;
		/** 16 */	RMuint32 ksimapgM33rgb2lmsscale2p:5;
		/** 21 */	RMuint32 rsvd0:11;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg14Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33rgb2lms02:16;
		/** 16 */	RMuint32 ksimapgM33rgb2lms01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg15Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33rgb2lms11:16;
		/** 16 */	RMuint32 ksimapgM33rgb2lms10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg16Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33rgb2lms20:16;
		/** 16 */	RMuint32 ksimapgM33rgb2lms12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg17Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33rgb2lms22:16;
		/** 16 */	RMuint32 ksimapgM33rgb2lms21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg18Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33lms2ipt00:16;
		/** 16 */	RMuint32 ksimapgM33lms2iptscale2p:5;
		/** 21 */	RMuint32 rsvd0:11;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg19Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33lms2ipt02:16;
		/** 16 */	RMuint32 ksimapgM33lms2ipt01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg20Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33lms2ipt11:16;
		/** 16 */	RMuint32 ksimapgM33lms2ipt10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg21Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33lms2ipt20:16;
		/** 16 */	RMuint32 ksimapgM33lms2ipt12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg22Reg { // swapped fields u4
	struct {
		/** 00 */	RMuint32 ksimapgM33lms2ipt22:16;
		/** 16 */	RMuint32 ksimapgM33lms2ipt21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg23Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgIptscale:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg24Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgV3iptoff0:16;
		/** 16 */	RMuint32 rsvd0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimapg25Reg { // u4
	struct {
		/** 00 */	RMuint32 ksimapgV3iptoff2:16;
		/** 16 */	RMuint32 ksimapgV3iptoff1:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown06Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_iwin_r:13;    // manul R of in-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_iwin_alpha:8; // manul alpha of in-win, nonuse in V_EDR
		/** 24 */	RMuint32 win_ext_iwin_en:1;    // set RGBa which inside of window as manul vaue for debug purpose
		/** 25 */	RMuint32 rsvd1:6;
		/** 31 */	RMuint32 win_ext_prog_en:1;    // enable window extention for video/OSD overlay
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown07Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_iwin_b:13;  // manul B of in-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_iwin_g:13;  // manul G of in-win
		/** 29 */	RMuint32 rsvd1:3;
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown08Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_owin_r:13;    // manul R of out-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_owin_alpha:8; // manul alpha of out-win, nonuse in V_EDR
		/** 24 */	RMuint32 win_ext_owin_en:1;    // 1'b1 set RGBa which outside of window as manul value for debug purpose
		/** 25 */	RMuint32 rsvd1:7;
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown09Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_owin_b:13;  // manul B of out-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_owin_g:13;  // manul G of out-win
		/** 29 */	RMuint32 rsvd1:3;
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown10Reg { //xx ??
	struct {
		/** 00 */	RMuint32 b_value:9;
		/** 09 */	RMuint32 g_value:9;
		/** 18 */	RMuint32 r_value:9;
		/** 27 */	RMuint32 alpha:4;
		/** 31 */	RMuint32 border_enable:1; // enable in-win border for debug purpose
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown11Reg { //xx ??
	struct {
		/** 00 */	RMuint32 b_value:9;
		/** 09 */	RMuint32 g_value:9;
		/** 18 */	RMuint32 r_value:9;
		/** 27 */	RMuint32 alpha:4;
		/** 31 */	RMuint32 border_enable:1; // enable out-win border for debug purpose
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown12Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_vstart:16; // vde start position for extended window
		/** 16 */	RMuint32 win_ext_hstart:16; // hde start position for extended window
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown13Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_vsize:16; // vde size for extended window
		/** 16 */	RMuint32 win_ext_hsize:16; // hde size for extended window
	} bits;
	RMuint32 value;
};


union TccEdrDMImapgUnknown14Reg { //xx u5
	struct {
		/** 00 */	RMuint32 ksdmctrlAlphamaxmode:1;
		/** 01 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};
union TccEdrDMImapgUnknown15Reg { //xx u5 - same as TccEdrDMBioKsimapg08Reg !!!
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamRange:16;
		/** 16 */	RMuint32 ksimapgEotfparamRangemin:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMImapgUnknown16Reg { //xx u5 - same as TccEdrDMBioKsimapg09Reg !!!
	struct {
		/** 00 */	RMuint32 ksimapgEotfparamRangeinv:32;
	} bits;
	RMuint32 value;
};


//========================================= OMAP ======================================================

union TccEdrDMBioKsomap00Reg { //xx u8 - fields shifted between omap registers
	struct {
		/** 00 */	RMuint32 ksomapM33ipt2lms01:16;
		/** 16 */	RMuint32 ksomapM33ipt2lms00:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap01Reg { //xx u8 - fields shifted between omap registers
	struct {
		/** 00 */	RMuint32 ksomapM33ipt2lms10:16;
		/** 16 */	RMuint32 ksomapM33ipt2lms02:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap02Reg { //xx u8 - fields shifted between omap registers
	struct {
		/** 00 */	RMuint32 ksomapM33ipt2lms12:16;
		/** 16 */	RMuint32 ksomapM33ipt2lms11:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap03Reg { //xx u8 - fields shifted between omap registers
	struct {
		/** 00 */	RMuint32 ksomapM33ipt2lms21:16;
		/** 16 */	RMuint32 ksomapM33ipt2lms20:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap04Reg { //xx u8 - fields shifted between omap registers
	struct {
		/** 00 */	RMuint32 ksomapM33lms2rgb00:16;
		/** 16 */	RMuint32 ksomapM33ipt2lms22:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap06Reg { //xx u8 - fields swapped
	struct {
		/** 00 */	RMuint32 ksomapM33lms2rgb02:16;
		/** 16 */	RMuint32 ksomapM33lms2rgb01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap07Reg { //xx u8 - fields swapped
	struct {
		/** 00 */	RMuint32 ksomapM33lms2rgb11:16;
		/** 16 */	RMuint32 ksomapM33lms2rgb10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap08Reg { //xx u8 - fields swapped
	struct {
		/** 00 */	RMuint32 ksomapM33lms2rgb20:16;
		/** 16 */	RMuint32 ksomapM33lms2rgb12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap09Reg { //xx u8 - fields swapped
	struct {
		/** 00 */	RMuint32 ksomapM33lms2rgb22:16;
		/** 16 */	RMuint32 ksomapM33lms2rgb21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap10Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapOetfparamMin:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap11Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapOetfparamMax:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap12Reg { //xx u8 - fields swapped
	struct {
		/** 00 */	RMuint32 ksomapOetfparamRange:16;
		/** 16 */	RMuint32 ksomapOetfparamRangemin:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap13Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapOetfparamRangeoverone:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap14Reg { //xx u8 mixed fields
	struct {
		/** 00 */	RMuint32 ksomapM33rgb2yuvscale2p:5;
		/** 05 */	RMuint32 ksomapClr:8;
		/** 13 */	RMuint32 ksomapOetfparamOetf:8;
		/** 21 */	RMuint32 ksomapOetfparamBdp:5;
		/** 26 */	RMuint32 ksomapM33ipt2lmsscale2p:5;
		/** 31 */	RMuint32 rsvd0:1;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap15Reg { //xx u8 mixed fields
	struct {
		/** 00 */	RMuint32 ksomapM33lms2rgbscale2p:5;
		/** 05 */	RMuint32 rsvd0:11;
		/** 16 */	RMuint32 ksomapM33rgb2yuv22:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap16Reg { //xx u8 mixed fields
	struct {
		/** 00 */	RMuint32 ksomapM33rgb2yuv01:16;
		/** 16 */	RMuint32 ksomapM33rgb2yuv00:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap17Reg { //xx u8 mixed fields
	struct {
		/** 00 */	RMuint32 ksomapM33rgb2yuv10:16;
		/** 16 */	RMuint32 ksomapM33rgb2yuv02:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap18Reg { //xx u8 mixed fields
	struct {
		/** 00 */	RMuint32 ksomapM33rgb2yuv12:16;
		/** 16 */	RMuint32 ksomapM33rgb2yuv11:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap19Reg { //xx u8 mixed fields
	struct {
		/** 00 */	RMuint32 ksomapM33rgb2yuv21:16;
		/** 16 */	RMuint32 ksomapM33rgb2yuv20:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap20Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapV3rgb2yuvoff0:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap21Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapV3rgb2yuvoff1:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap22Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapV3rgb2yuvoff2:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap23Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapIptscale:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap24Reg { // u8 - fields swapped
	struct {
		/** 00 */	RMuint32 ksomapV3iptoff1:16;
		/** 16 */	RMuint32 ksomapV3iptoff0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsomap25Reg { // u8
	struct {
		/** 00 */	RMuint32 ksomapV3iptoff2:16;
		/** 16 */	RMuint32 rsvd0:16;
	} bits;
	RMuint32 value;
};

//==============================================================================


union TccEdrDMOmapUnknown00Reg { //xx u8
	struct {
		/** 00 */	RMuint32 glb_l2gLut_cen:2;     // G2L memory cen control, 2'b01 for normal work mode
		/** 02 */	RMuint32 glb_l2gLut_pd:1;      // memory power down, low actived
		/** 03 */	RMuint32 glb_l2gLut_lut_en:1;  // 1'b1 lut program enable
		/** 04 */	RMuint32 glb_l2gLut_lut_sel:3; // lut read back sel, 0 for R, 1 for G, 2 for B
		/** 07 */	RMuint32 rsvd0:1;
		/** 08 */	RMuint32 glb_l2pqLut_cen:2;    // L2PQ
		/** 10 */	RMuint32 glb_l2pqLut_pd:1;
		/** 11 */	RMuint32 glb_l2pqLut_lut_en:1;
		/** 12 */	RMuint32 glb_l2pqLut_lut_sel:3;
		/** 15 */	RMuint32 rsvd1:1;
		/** 16 */	RMuint32 glb_pq2lLut_cen:2;    // PQ2L
		/** 18 */	RMuint32 glb_pq2lLut_pd:1;
		/** 19 */	RMuint32 glb_pq2lLut_lut_en:1;
		/** 20 */	RMuint32 glb_pq2lLut_lut_sel:3;
		/** 23 */	RMuint32 rsvd2:6;
		/** 29 */	RMuint32 glb_vs_latch_en:1;    // 1'b1 active all reg setting at vsync rising edge
		/** 30 */	RMuint32 glb_clk_on:2;         // 2'b11 enable clock on for output csc
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds01Reg { // u9 - mixed fields
	struct {
		/** 00 */	RMuint32 bypass_dither_u10:1;         // nonuse in current application, set as 0
		/** 01 */	RMuint32 ksudsFilteruvcoldsscale2p:5;
		/** 06 */	RMuint32 ksdmctrlOutputalign:1;
		/** 07 */	RMuint32 rsvd0:24;
		/** 31 */	RMuint32 glb_load_en:1;               // enable vs load en for YUV444->YUV422
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds16Reg { // u9 swapped fields
	struct {
		/** 00 */	RMuint32 metadata_length:16;
		/** 07 */	RMuint32 rsvd0:14;
		/** 30 */	RMuint32 uv_reverse:1;
		/** 31 */	RMuint32 crambling_bypass:1; 
	} bits;
	RMuint32 value;
};
union TccEdrDMOmapUnknown01Reg { //xx u8
	struct {
		/** 00 */	RMuint32 vend:16;   // VDE falling edge position
		/** 16 */	RMuint32 vstart:16; // VDE position from VSYNC rising edge
	} bits;
	RMuint32 value;
};

union TccEdrDMOmapUnknown02Reg { //xx u8
	struct {
		/** 00 */	RMuint32 hend:16;   // HDE falling edge position
		/** 16 */	RMuint32 hstart:16; // HDE position from HSYNC falling edge
	} bits;
	RMuint32 value;
};

union TccEdrDMOmapUnknown03Reg { //xx u8
	struct {
		/** 00 */	RMuint32 htotal:16; // distance between 2 HSYNC rising edge
		/** 16 */	RMuint32 vtotal:16; // distance between 2 VSYNC rising edge
	} bits;
	RMuint32 value;
};

union TccEdrDMOmapUnknown04Reg { //xx u8
	struct {
		/** 00 */	RMuint32 hwidth:8; // width of HSYNC
		/** 08 */	RMuint32 vwidth:5; // width of VSYNC
	} bits;
	RMuint32 value;
};

union TccEdrDMOmapUnknown05Reg { //xx u8
	struct {
		/** 00 */	RMuint32 VLOADEN:1;        // set as 1'b1 in current application
		/** 01 */	RMuint32 rsvd0:1;
		/** 02 */	RMuint32 HLOADEN:1;        // set as 1'b1 in current application
		/** 03 */	RMuint32 rsvd1:6;
		/** 09 */	RMuint32 CRTC_VS_BYPASS:1; // set as 1'b1 in current application
		/** 10 */	RMuint32 CRTC_HS_BYPASS:1; // set as 1'b1 in current application
		/** 11 */	RMuint32 rsvd2:30;
		/** 31 */	RMuint32 CRTC_SOFT_RESET:1;     // soft reset for CRTC, high actived
	} bits;
	RMuint32 value;
};


struct TccEdrDMOsd1V1Reg {
	// base address = 0x12540000 - "u4" - derived from struct VsyncRegVpEdrStbSourceDisplayManagementV1Reg graphic top
    volatile union TccEdrDMImapgUnknown00Reg unkimapg00; // 00
    volatile union TccEdrDMImapgUnknown01Reg unkimapg01; // 04
    volatile union TccEdrDMImapgUnknown02Reg unkimapg02; // 08
    volatile union TccEdrDMImapgUnknown03Reg unkimapg03; // 0c
    volatile union TccEdrDMImapgUnknown04Reg unkimapg04; // 10
    volatile union TccEdrDMImapgUnknown05Reg unkimapg05; // 14
	volatile union TccEdrDMBioKsimapg00Reg bioKsimapg00; // 18
	volatile union TccEdrDMBioKsimapg01Reg bioKsimapg01; // 1c
	volatile union TccEdrDMBioKsimapg02Reg bioKsimapg02; // 20
	volatile union TccEdrDMBioKsimapg03Reg bioKsimapg03; // 24
	volatile union TccEdrDMBioKsimapg04Reg bioKsimapg04; // 28
	volatile union TccEdrDMBioKsimapg05Reg bioKsimapg05; // 2c
	volatile union TccEdrDMBioKsimapg06Reg bioKsimapg06; // 30
	volatile union TccEdrDMBioKsimapg07Reg bioKsimapg07; // 34
	volatile union TccEdrDMBioKsimapg08Reg bioKsimapg08; // 38
	volatile union TccEdrDMBioKsimapg09Reg bioKsimapg09; // 3c
	volatile union TccEdrDMBioKsimapg10Reg bioKsimapg10; // 40
	volatile union TccEdrDMBioKsimapg11Reg bioKsimapg11; // 44
	volatile union TccEdrDMBioKsimapg12Reg bioKsimapg12; // 48
	volatile union TccEdrDMBioKsimapg13Reg bioKsimapg13; // 4c
	volatile union TccEdrDMBioKsimapg14Reg bioKsimapg14; // 50
	volatile union TccEdrDMBioKsimapg15Reg bioKsimapg15; // 54
	volatile union TccEdrDMBioKsimapg16Reg bioKsimapg16; // 58
	volatile union TccEdrDMBioKsimapg17Reg bioKsimapg17; // 5c
	volatile union TccEdrDMBioKsimapg18Reg bioKsimapg18; // 60
	volatile union TccEdrDMBioKsimapg19Reg bioKsimapg19; // 64
	volatile union TccEdrDMBioKsimapg20Reg bioKsimapg20; // 68
	volatile union TccEdrDMBioKsimapg21Reg bioKsimapg21; // 6c
	volatile union TccEdrDMBioKsimapg22Reg bioKsimapg22; // 70
	volatile union TccEdrDMBioKsimapg23Reg bioKsimapg23; // 74
	volatile union TccEdrDMBioKsimapg24Reg bioKsimapg24; // 78
	volatile union TccEdrDMBioKsimapg25Reg bioKsimapg25; // 7c
	volatile union TccEdrDMImapgUnknown06Reg unkimapg06; // 80
	volatile union TccEdrDMImapgUnknown07Reg unkimapg07; // 84
	volatile union TccEdrDMImapgUnknown08Reg unkimapg08; // 88
	volatile union TccEdrDMImapgUnknown09Reg unkimapg09; // 8c
	volatile union TccEdrDMImapgUnknown10Reg unkimapg10; // 90
	volatile union TccEdrDMImapgUnknown11Reg unkimapg11; // 94
	volatile union TccEdrDMImapgUnknown12Reg unkimapg12; // 98
	volatile union TccEdrDMImapgUnknown13Reg unkimapg13; // 9c
    volatile union TccEdrDMImapgUnknown14Reg unkimapg14; // a0
    volatile union TccEdrDMImapgUnknown15Reg unkimapg15; // a4
    volatile union TccEdrDMImapgUnknown16Reg unkimapg16; // a8
};

struct TccEdrDMOsd3V1Reg {
	// base address = 0x12540000 - "u6" - derived from struct VsyncRegVpEdrStbSourceDisplayManagementV1Reg graphic bottom
    volatile union TccEdrDMImapgUnknown04Reg unkimapgb04; // ac
    volatile union TccEdrDMImapgUnknown05Reg unkimapgb05; // b0
	volatile union TccEdrDMBioKsimapg00Reg bioKsimapgb00; // b4
	volatile union TccEdrDMBioKsimapg01Reg bioKsimapgb01; // b8
	volatile union TccEdrDMBioKsimapg02Reg bioKsimapgb02; // bc
	volatile union TccEdrDMBioKsimapg03Reg bioKsimapgb03; // c0
	volatile union TccEdrDMBioKsimapg04Reg bioKsimapgb04; // c4
	volatile union TccEdrDMBioKsimapg05Reg bioKsimapgb05; // c8
	volatile union TccEdrDMBioKsimapg06Reg bioKsimapgb06; // cc
	volatile union TccEdrDMBioKsimapg07Reg bioKsimapgb07; // d0
	volatile union TccEdrDMBioKsimapg08Reg bioKsimapgb08; // d4
	volatile union TccEdrDMBioKsimapg09Reg bioKsimapgb09; // d8
	volatile union TccEdrDMBioKsimapg10Reg bioKsimapgb10; // dc
	volatile union TccEdrDMBioKsimapg11Reg bioKsimapgb11; // e0
	volatile union TccEdrDMBioKsimapg12Reg bioKsimapgb12; // e4
	volatile union TccEdrDMBioKsimapg13Reg bioKsimapgb13; // e8
	volatile union TccEdrDMBioKsimapg14Reg bioKsimapgb14; // ec
	volatile union TccEdrDMBioKsimapg15Reg bioKsimapgb15; // f0
	volatile union TccEdrDMBioKsimapg16Reg bioKsimapgb16; // f4
	volatile union TccEdrDMBioKsimapg17Reg bioKsimapgb17; // f8
	volatile union TccEdrDMBioKsimapg18Reg bioKsimapgb18; // fc
	volatile union TccEdrDMBioKsimapg19Reg bioKsimapgb19; // 100
	volatile union TccEdrDMBioKsimapg20Reg bioKsimapgb20; // 104
	volatile union TccEdrDMBioKsimapg21Reg bioKsimapgb21; // 108
	volatile union TccEdrDMBioKsimapg22Reg bioKsimapgb22; // 10c
	volatile union TccEdrDMBioKsimapg23Reg bioKsimapgb23; // 110
	volatile union TccEdrDMBioKsimapg24Reg bioKsimapgb24; // 114
	volatile union TccEdrDMBioKsimapg25Reg bioKsimapgb25; // 118
	volatile union TccEdrDMImapgUnknown06Reg unkimapgb06; // 11c
	volatile union TccEdrDMImapgUnknown07Reg unkimapgb07; // 120
	volatile union TccEdrDMImapgUnknown08Reg unkimapgb08; // 124
	volatile union TccEdrDMImapgUnknown09Reg unkimapgb09; // 128
	volatile union TccEdrDMImapgUnknown10Reg unkimapgb10; // 12c
	volatile union TccEdrDMImapgUnknown11Reg unkimapgb11; // 130
	volatile union TccEdrDMImapgUnknown12Reg unkimapgb12; // 134
	volatile union TccEdrDMImapgUnknown13Reg unkimapgb13; // 138
    volatile union TccEdrDMImapgUnknown14Reg unkimapgb14; // 13c
    volatile union TccEdrDMImapgUnknown15Reg unkimapgb15; // 140
    volatile union TccEdrDMImapgUnknown16Reg unkimapgb16; // 144
};

struct TccEdrDMOutputV1Reg {
	// base address = 0x12540000 - "u8" - derived from struct VsyncRegVpEdrStbSourceDisplayManagementV1Reg {
    volatile union TccEdrDMOmapUnknown00Reg unkomap00;    // 148
	volatile union TccEdrDMBioKsomap00Reg bioKsomap00;    // 14c
	volatile union TccEdrDMBioKsomap01Reg bioKsomap01;    // 150
	volatile union TccEdrDMBioKsomap02Reg bioKsomap02;    // 154
	volatile union TccEdrDMBioKsomap03Reg bioKsomap03;    // 158
	volatile union TccEdrDMBioKsomap04Reg bioKsomap04;    // 15c
	volatile union TccEdrDMBioKsomap06Reg bioKsomap06;    // 160
	volatile union TccEdrDMBioKsomap07Reg bioKsomap07;    // 164
	volatile union TccEdrDMBioKsomap08Reg bioKsomap08;    // 168
	volatile union TccEdrDMBioKsomap09Reg bioKsomap09;    // 16c
	volatile union TccEdrDMBioKsomap10Reg bioKsomap10;    // 170
	volatile union TccEdrDMBioKsomap11Reg bioKsomap11;    // 174
	volatile union TccEdrDMBioKsomap12Reg bioKsomap12;    // 178
	volatile union TccEdrDMBioKsomap13Reg bioKsomap13;    // 17c
	volatile union TccEdrDMBioKsomap14Reg bioKsomap14;    // 180
	volatile union TccEdrDMBioKsomap15Reg bioKsomap15;    // 184
	volatile union TccEdrDMBioKsomap16Reg bioKsomap16;    // 188
	volatile union TccEdrDMBioKsomap17Reg bioKsomap17;    // 18c
	volatile union TccEdrDMBioKsomap18Reg bioKsomap18;    // 190
	volatile union TccEdrDMBioKsomap19Reg bioKsomap19;    // 194
	volatile union TccEdrDMBioKsomap20Reg bioKsomap20;    // 198
	volatile union TccEdrDMBioKsomap21Reg bioKsomap21;    // 19c
	volatile union TccEdrDMBioKsomap22Reg bioKsomap22;    // 1a0
	volatile union TccEdrDMBioKsomap23Reg bioKsomap23;    // 1a4
	volatile union TccEdrDMBioKsomap24Reg bioKsomap24;    // 1a8
	volatile union TccEdrDMBioKsomap25Reg bioKsomap25;    // 1ac
	volatile union TccEdrDMBioKsuds03Reg bioKsuds03;      // 1b0
	volatile union TccEdrDMBioKsuds12Reg bioKsuds12;      // 1b4
	volatile union TccEdrDMBioKsuds13Reg bioKsuds13;      // 1b8
	volatile union TccEdrDMBioKsuds14Reg bioKsuds14;      // 1bc
	volatile RMuint32 rsvd0[4];                           // 1c0 .. 1cf
	volatile union TccEdrDMBioKsuds15Reg bioKsuds15;      // 1d0
	volatile union TccEdrDMBioKsuds01Reg bioKsuds01;      // 1d4
	volatile union TccEdrDMBioKsuds16Reg bioKsuds16;      // 1d8
	volatile RMuint32 rsvd1[393];                         // 1dc .. 7ff : nonuse in current application, set as 0 
	volatile union TccEdrDMOmapUnknown01Reg unkomap01;    // 800
	volatile union TccEdrDMOmapUnknown02Reg unkomap02;    // 804
	volatile union TccEdrDMOmapUnknown03Reg unkomap03;    // 808
	volatile union TccEdrDMOmapUnknown04Reg unkomap04;    // 80c
	volatile union TccEdrDMOmapUnknown05Reg unkomap05;    // 810
	volatile RMuint32 rsvd2[30];
};


struct TccEdrPanelV1Reg {
	// base address = V_PANEL = 0x12540000
	volatile struct TccEdrDMOsd1V1Reg osd1;  // 0x12540000
	volatile struct TccEdrDMOsd3V1Reg osd3;  // 0x125400ac
	volatile struct TccEdrDMOutputV1Reg out; // 0x12540148
};

#endif /** #ifndef __TCC_EDR_VPANEL_V1_H__ */

