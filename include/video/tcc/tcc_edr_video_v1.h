#ifndef __TCC_EDR_VIDEO_V1_H__
#define __TCC_EDR_VIDEO_V1_H__

#ifndef RMuint32
#define RMuint32 unsigned int
#endif

/**
  @file tcc_edr_video_v1.h. Adapted from file vp_edr_stb_core_v1.h and vp_edr_stb_source_display_management_v1.h in SMP8760.
  @brief Definitions of register structures. Modules name: vp_edr_stb_core and vp_edr_stb_source_display_management
  @author Aurelia Popa-Radu.
*/

/* Known differences:
   SMP8760 interface is organized as Core = mainly composer and specific SMP8760 interface, and DisplayManagement = DM = video input, graphics input and output.
   TCC interface is organized as V_EDR = video path including composer, and V_PANEL = graphics and output.
   Note. SMP8760 register name prefix VsyncRegVpEdrCore was changed to TccEdrCore.
   Note. SMP8760 register name prefix VsyncRegVpEdrStbSourceDisplayManagement was changed to TccEdrDM.
   Unclear registers/definitions were marked with //xx.
*/

union TccEdrCoreGlb0Reg { //xx
	struct {
		/** 00 */	RMuint32 glb_pclk_composer_on:2;     // 'b11 enable clock on for composer parts
		/** 02 */	RMuint32 glb_pclk_dm_on:2;           // 'b11 enable clock on for DM parts
		/** 04 */	RMuint32 glb_icsc_cvm_clk_on:6;      // 'h3f enable clock on for input mapping and cvm
		/** 10 */	RMuint32 glb_icsc_cvm_vs_latch_en:1; // nonuse in V_EDR
		/** 11 */	RMuint32 rsvd0:21;
	} bits;
	RMuint32 value;
};

union TccEdrCoreResetReg { //xx
	struct {
		/** 00 */	RMuint32 rstn_crtc:1;     // soft reset of edr_crtc, low actived
		/** 01 */	RMuint32 rstn_dm:1;       // soft reset of dm, low actived
		/** 02 */	RMuint32 rstn_composer:1; // soft reset of composer, low actived
		/** 03 */	RMuint32 rsvd0:29;
	} bits;
	RMuint32 value;
};

union TccEdrCoreGlb1Reg { //xx
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
		/** 23 */	RMuint32 rsvd2:1;
		/** 24 */	RMuint32 glb_cvmLut_cen:2;     // ToneMapping
		/** 26 */	RMuint32 glb_cvmLut_pd:1;
		/** 27 */	RMuint32 glb_cvmLut_lut_en:1;
		/** 28 */	RMuint32 glb_420to422_cen:2;
		/** 30 */	RMuint32 glb_420to422_pd:1;
		/** 31 */	RMuint32 rsvd3:1;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp1ExtReg { //xx moved here from VsyncRegVpEdrStbCoreComp1Reg
	struct {
		/** 00 */	RMuint32 lumaBlDownsamplingCen:2;
		/** 02 */	RMuint32 lumaBlDownsamplingPd:1;
		/** 03 */	RMuint32 rsvd0:1;
		/** 04 */	RMuint32 spatialUpsamplingCen:2;
		/** 06 */	RMuint32 spatialUpsamplingPd:1;
		/** 07 */	RMuint32 rsvd1:25;
	} bits;
	RMuint32 value;
};

union TccEdrCoreGlb2Reg { //xx
	struct {
		/** 00 */	RMuint32 glb_edr_m1_load_en:1; // 1'b1 active all reg setting for page M1 at vsync rising edge
		/** 01 */	RMuint32 glb_edr_m2_load_en:1; // 1'b1 active all reg setting for page M2 at vsync rising edge
		/** 02 */	RMuint32 glb_edr_m3_load_en:1; // 1'b1 active all reg setting for page M3 at vsync rising edge
		/** 03 */	RMuint32 glb_edr_m4_load_en:1; // 1'b1 active all reg setting for page M4 at vsync rising edge
		/** 04 */	RMuint32 rsvd0:28;
	} bits;
	RMuint32 value;
};

union TccEdrCoreUnknown5Reg { //xx
	struct {
		/** 00 */	RMuint32 vswidth_crtc:8;     // vsync width
		/** 08 */	RMuint32 hswidth_crtc:8;     // hsync width
		/** 16 */	RMuint32 vs_src_sel_crtc:2;  // 0: vsync from V_PANEL, others nonuse
		/** 18 */	RMuint32 hs_src_sel_crtc:2;  // 0: hsync from V_PANEL, others nonuse
		/** 20 */	RMuint32 sel_ext_vs_crtc:1;  // sel external vsync as vs source
		/** 21 */	RMuint32 sel_ext_hs_crtc:1;  // sel external hsync as hs source
		/** 22 */	RMuint32 load_en_crtc:1;     // 1'b1 active all new CRTC setting at vsync rising edge, 1'b1 sel exteranl VSYNC after any cycle delay
		/** 23 */	RMuint32 mclk_dly_en_crtc:1; // 1'b0 sel external VSYNC without delay"
		/** 24 */	RMuint32 dbus_vpanel_en:1;   // 1'b1 for normal work mode
		/** 25 */	RMuint32 rsvd0:7;
	} bits;
	RMuint32 value;
};

union TccEdrCoreUnknown6Reg { //xx -- related to composerHdeStartCnt, composerVdeStartCnt ?
	struct {
		/** 00 */	RMuint32 hstart_crtc:16; // hde start position
		/** 16 */	RMuint32 hsize_crtc:16;  // hor size
	} bits;
	RMuint32 value;
};

union TccEdrCoreUnknown7Reg { //xx
	struct {
		/** 00 */	RMuint32 vstart_crtc:16; // vde start position
		/** 16 */	RMuint32 vsize_crtc:16;  // ver size
	} bits;
	RMuint32 value;
};

union TccEdrCoreUnknown8Reg { //xx
	struct {
		/** 00 */	RMuint32 vtotal_crtc:16; // ver total
		/** 16 */	RMuint32 htotal_crtc:16; // hor total
	} bits;
	RMuint32 value;
};

union TccEdrCoreUnknown9Reg { //xx
	struct {
		/** 00 */	RMuint32 ext_vs_width_crtc:16; // vsync width after cycle delay
		/** 16 */	RMuint32 vs_dly_en_crtc:1;     // enable external VSYNC cycle delay
		/** 17 */	RMuint32 rsvd0:15;
	} bits;
	RMuint32 value;
};

union TccEdrCoreUnknown10Reg { //xx
	struct {
		/** 00 */	RMuint32 ext_vs_start_crtc:32; // vsync position after cycle delay
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp0Reg {
	struct {
		/** 00 */	RMuint32 blMappingModeV:1;
		/** 01 */	RMuint32 blMappingModeU:1;
		/** 02 */	RMuint32 elBypass:1;
		/** 03 */	RMuint32 vdrBitDepth:4;
		/** 07 */	RMuint32 elBitDepth:1;
		/** 08 */	RMuint32 blBitDepth:1;
		/** 09 */	RMuint32 rsvd0:7;
		/** 16 */	RMuint32 spfCoefLog2Denom:8;
		/** 24 */	RMuint32 coefLog2Denom:8;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp1Reg {
	struct {
		/** 00 */	RMuint32 horUpsamplingFilterY:1;
		/** 01 */	RMuint32 horUpsamplingMode:1;
		/** 02 */	RMuint32 verUpsamplingFilterV:1;
		/** 03 */	RMuint32 verUpsamplingFilterU:1;
		/** 04 */	RMuint32 verUpsamplingFilterY:2;
		/** 06 */	RMuint32 verUpsamplingMode:2;
		/** 08 */	RMuint32 elUpsamplingFlag:1;
		/** 09 */	RMuint32 blUpsamplingFlag:1;
		/** 10 */   RMuint32 rsvd0:22;
		// lumaBlDownsamplingPd, lumaBlDownsamplingCen, spatialUpsamplingPd, spatialUpsamplingCen moved to TccEdrCoreComp1ExtReg
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp2Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotV270:28;
		/** 28 */	RMuint32 blNumPivotV:4;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp3Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotV5928:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp4Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffV8364:20;
		/** 20 */	RMuint32 elNlqOffsetV:12;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp5Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffV6332:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp6Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffV310:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp7Reg {
	struct {
		/** 00 */	RMuint32 blMapOrderV:8;
		/** 08 */	RMuint32 blMapOrderU:8;
		/** 16 */	RMuint32 blMapOrderY:16;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp8Reg {
	struct {
		/** 00 */	RMuint32 upsamplingPivot:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreBlMapCoeffUReg {
	struct {
		/** 00 */	RMuint32 blMapCoeffU:32;
	} bits;
	RMuint32 value;
};


union TccEdrCoreBlMapCoeffVReg {
	struct {
		/** 00 */	RMuint32 blMapCoeffV:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp63Reg {
	struct {
		/** 00 */	RMuint32 blMapCoeffV27:16;
		/** 16 */	RMuint32 blMapCoeffU27:16;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp64Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffY8364:20;
		/** 20 */	RMuint32 elNlqOffsetY:12;
	} bits;
	RMuint32 value;
};

union TccEdrCoreElNlqCoeffY6332Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffY6332:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreElNlqCoeffY310Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffY310:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp67Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffU8364:20;
		/** 20 */	RMuint32 elNlqOffsetU:12;
	} bits;
	RMuint32 value;
};

union TccEdrCoreElNlqCoeffU6332Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffU6332:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreElNlqCoeffU310Reg {
	struct {
		/** 00 */	RMuint32 elNlqCoeffU310:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp70Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotU5932:28;
		/** 28 */	RMuint32 blNumPivotU:4;
	} bits;
	RMuint32 value;
};

union TccEdrCoreBlMapPivotU310Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotU310:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreComp72Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotY10796:12;
		/** 12 */	RMuint32 blNumPivotY:4;
		/** 16 */	RMuint32 upsamplingCoeffY52:16;
	} bits;
	RMuint32 value;
};

union TccEdrCoreBlMapPivotY310Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotY310:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreBlMapPivotY6332Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotY6332:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreBlMapPivotY9564Reg {
	struct {
		/** 00 */	RMuint32 blMapPivotY9564:32;
	} bits;
	RMuint32 value;
};

union TccEdrCoreUpsamplingCoeffYReg {
	struct {
		/** 00 */	RMuint32 upsamplingCoeffY:32;
	} bits;
	RMuint32 value;
};


union TccEdrCoreBlMapCoeffYReg {
	struct {
		/** 00 */	RMuint32 blMapCoeffY:32;
	} bits;
	RMuint32 value;
};


#if 0 //xx start specific SMP8760 registers
union VsyncRegVpEdrStbCoreReadLutTimeoutReg {
	struct {
		/** 00 */	RMuint32 readLutTimeout:3;
		/** 03 */	RMuint32 rsvd0:29;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutErrorReg {
	struct {
		/** 00 */	RMuint32 composerVdeUnderflowError:1;
		/** 01 */	RMuint32 composerVdeOverflowError:1;
		/** 02 */	RMuint32 composerHdeUnderflowError:1;
		/** 03 */	RMuint32 composerHdeOverflowError:1;
		/** 04 */	RMuint32 rsvd0:28;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmOutErrorReg {
	struct {
		/** 00 */	RMuint32 sdmVdeUnderflowError:1;
		/** 01 */	RMuint32 sdmVdeOverflowError:1;
		/** 02 */	RMuint32 sdmHdeUnderflowError:1;
		/** 03 */	RMuint32 sdmHdeOverflowError:1;
		/** 04 */	RMuint32 rsvd0:28;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerIdleToHsCntReg {
	struct {
		/** 00 */	RMuint32 composerIdleToHsCnt:26;
		/** 26 */	RMuint32 rsvd0:6;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerVdeStartCntReg {
	struct {
		/** 00 */	RMuint32 composerVdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerHdeStartCntReg {
	struct {
		/** 00 */	RMuint32 composerHdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerHdeEndCntReg {
	struct {
		/** 00 */	RMuint32 composerHdeEndCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerPictureHsizeReg {
	struct {
		/** 00 */	RMuint32 composerPictureHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerPictureVsizeReg {
	struct {
		/** 00 */	RMuint32 composerPictureVsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerTopBlankingHdeStartCntReg {
	struct {
		/** 00 */	RMuint32 composerTopBlankingHdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerTopBlankingHdeEndCntReg {
	struct {
		/** 00 */	RMuint32 composerTopBlankingHdeEndCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerTopBlankingHsizeReg {
	struct {
		/** 00 */	RMuint32 composerTopBlankingHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmIdleToHsCntReg {
	struct {
		/** 00 */	RMuint32 sdmIdleToHsCnt:26;
		/** 26 */	RMuint32 rsvd0:6;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmVdeStartCntReg {
	struct {
		/** 00 */	RMuint32 sdmVdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmHdeStartCntReg {
	struct {
		/** 00 */	RMuint32 sdmHdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmHdeEndCntReg {
	struct {
		/** 00 */	RMuint32 sdmHdeEndCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmPictureHsizeReg {
	struct {
		/** 00 */	RMuint32 sdmPictureHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmPictureVsizeReg {
	struct {
		/** 00 */	RMuint32 sdmPictureVsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmTopBlankingHdeStartCntReg {
	struct {
		/** 00 */	RMuint32 sdmTopBlankingHdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmTopBlankingHdeEndCntReg {
	struct {
		/** 00 */	RMuint32 sdmTopBlankingHdeEndCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmTopBlankingHsizeReg {
	struct {
		/** 00 */	RMuint32 sdmTopBlankingHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxIdleToHsCntReg {
	struct {
		/** 00 */	RMuint32 sdmGfxIdleToHsCnt:26;
		/** 26 */	RMuint32 rsvd0:6;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxVdeStartCntReg {
	struct {
		/** 00 */	RMuint32 sdmGfxVdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxHdeStartCntReg {
	struct {
		/** 00 */	RMuint32 sdmGfxHdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxHdeEndCntReg {
	struct {
		/** 00 */	RMuint32 sdmGfxHdeEndCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxPictureHsizeReg {
	struct {
		/** 00 */	RMuint32 sdmGfxPictureHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxPictureVsizeReg {
	struct {
		/** 00 */	RMuint32 sdmGfxPictureVsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxTopBlankingHdeStartCntReg {
	struct {
		/** 00 */	RMuint32 sdmGfxTopBlankingHdeStartCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxTopBlankingHdeEndCntReg {
	struct {
		/** 00 */	RMuint32 sdmGfxTopBlankingHdeEndCnt:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxTopBlankingHsizeReg {
	struct {
		/** 00 */	RMuint32 sdmGfxTopBlankingHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerBaseLayerEnableReg {
	struct {
		/** 00 */	RMuint32 composerBaseLayerEnable:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerBaseLayerSampleFormatReg {
	struct {
		/** 00 */	RMuint32 composerBaseLayerSampleFormat:2;
		/** 02 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerBaseLayerInterleavePlanarbReg {
	struct {
		/** 00 */	RMuint32 composerBaseLayerInterleavePlanarb:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerBaseLayerChromaOddLineZeroReg {
	struct {
		/** 00 */	RMuint32 composerBaseLayerChromaOddLineZero:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerEnhancedLayerEnableReg {
	struct {
		/** 00 */	RMuint32 composerEnhancedLayerEnable:2;
		/** 02 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerEnhancedLayerSampleFormatReg {
	struct {
		/** 00 */	RMuint32 composerEnhancedLayerSampleFormat:2;
		/** 02 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerEnhancedLayerInterleavePlanarbReg {
	struct {
		/** 00 */	RMuint32 composerEnhancedLayerInterleavePlanarb:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerEnhancedLayerChromaOddLineZeroReg {
	struct {
		/** 00 */	RMuint32 composerEnhancedLayerChromaOddLineZero:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutSampleFormatReg {
	struct {
		/** 00 */	RMuint32 composerOutSampleFormat:2;
		/** 02 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutInterleavePlanarbReg {
	struct {
		/** 00 */	RMuint32 composerOutInterleavePlanarb:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutChromaOddLineZeroReg {
	struct {
		/** 00 */	RMuint32 composerOutChromaOddLineZero:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutFormat42014bitReg {
	struct {
		/** 00 */	RMuint32 composerOutFormat42014bit:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutPictureHsizeReg {
	struct {
		/** 00 */	RMuint32 composerOutPictureHsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreComposerOutPictureVsizeReg {
	struct {
		/** 00 */	RMuint32 composerOutPictureVsize:13;
		/** 13 */	RMuint32 rsvd0:19;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreCfgDmFromComposerReg {
	struct {
		/** 00 */	RMuint32 cfgDmFromComposer:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmInSampleFormatReg {
	struct {
		/** 00 */	RMuint32 sdmInSampleFormat:2;
		/** 02 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmInInterleavePlanarbReg {
	struct {
		/** 00 */	RMuint32 sdmInInterleavePlanarb:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmInChromaOddLineZeroReg {
	struct {
		/** 00 */	RMuint32 sdmInChromaOddLineZero:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmInFormat42014bitReg {
	struct {
		/** 00 */	RMuint32 sdmInFormat42014bit:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxBackgroundEnableReg {
	struct {
		/** 00 */	RMuint32 sdmGfxBackgroundEnable:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbCoreSdmGfxForegroundEnableReg {
	struct {
		/** 00 */	RMuint32 sdmGfxForegroundEnable:1;
		/** 01 */	RMuint32 rsvd0:31;
	} bits;
	RMuint32 value;
};
#endif //xx - end specific SMP8760 registers

//===============================================================================================

union TccEdrDMBioCtrl0Reg {//xx fields from VsyncRegVpEdrStbSourceDisplayManagementBioCtrl0Reg and VsyncRegVpEdrStbSourceDisplayManagementBioKsuds01Reg
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcolusscale2p:5;
		/** 05 */	RMuint32 rsvd0:3;
		/** 08 */	RMuint32 ksudsFilteruvrowusscale2p:5;
		/** 13 */	RMuint32 rsvd1:3;
		/** 16 */	RMuint32 ksdmctrlBypass422to444:1;
		/** 17 */	RMuint32 ksdmctrlBypass420to422:1;
		/** 18 */	RMuint32 ksdmctrlInputalign:4;
	} bits;
	RMuint32 value;
};

#if 0 //xx start specific SMP8760 registers
union VsyncRegVpEdrStbSourceDisplayManagementBioCtrl0Reg { //xx
	struct {
		/** 00 */	RMuint32 ksdmctrlAlphamaxpower:1;  // NA
		/** 01 */	RMuint32 ksdmctrlAlphamaxmode:1;   // moved on bit 0 of TccEdrDMUnknowng14Reg ?
		/** 02 */	RMuint32 ksdmctrlMainin:1;         // NA
		/** 03 */	RMuint32 ksdmctrlSecin:1;          // NA
		/** 04 */	RMuint32 ksdmctrlThirdin:1;        // NA
		/** 05 */	RMuint32 ksdmctrlVideoalpha:8;     // NA
		/** 13 */	RMuint32 ksdmctrlShadowcontext:1;  // moved on bit 0 and 1 of TccEdrDMShadowContextReg ?
		/** 14 */	RMuint32 ksdmctrlBypass420to422:1; // moved on bit 17 of BioCtrl0Tcc
		/** 15 */	RMuint32 ksdmctrlBypass422to444:1; // moved on bit 16 of BioCtrl0Tcc
		/** 16 */	RMuint32 ksdmctrlInputalign:4;     // moved on bits 21:18 of BioCtrl0Tcc
		/** 20 */	RMuint32 ksdmctrlOutputalign:1;    // moved on bit 6 of BioKsuds01Reg
		/** 21 */	RMuint32 rsvd0:11;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbSourceDisplayManagementBioGlb3Reg { //xx
	struct {
		/** 00 */	RMuint32 pq2llutVdGlbLutSel:3;     // moved on TccEdrCoreGlb1Reg
		/** 03 */	RMuint32 rsvd0:6;
		/** 09 */	RMuint32 pq2llutOutGlbLutSel:3;
		/** 12 */	RMuint32 l2pqlutVdGlbLutSel:3;     // moved on TccEdrCoreGlb1Reg
		/** 15 */	RMuint32 l2pqlutGbGlbLutSel:3;
		/** 18 */	RMuint32 l2pqlutGtGlbLutSel:3;
		/** 21 */	RMuint32 l2pqlutOutGlbLutSel:3;
		/** 24 */	RMuint32 rsvd1:8;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbSourceDisplayManagementBioGlb4Reg { //xx
	struct {
		/** 00 */	RMuint32 g2llutVdGlbLutSel:3;      // moved on TccEdrCoreGlb1Reg
		/** 03 */	RMuint32 g2llutGbGlbLutSel:3;
		/** 06 */	RMuint32 g2llutGtGlbLutSel:3;
		/** 09 */	RMuint32 l2glutOutGlbLutSel:3;
		/** 12 */	RMuint32 rsvd0:20;
	} bits;
	RMuint32 value;
};

union VsyncRegVpEdrStbSourceDisplayManagementBioKsuds01Reg { //xx
	struct {
		/** 00 */	RMuint32 rsvd0:4;
		/** 04 */	RMuint32 ksudsFilteruvrowusscale2p:5; // moved on bits 12:8 of BioCtrl0Tcc
		/** 09 */	RMuint32 rsvd1:4;
		/** 13 */	RMuint32 ksudsFilteruvcolusscale2p:5; // moved on bits 4:0 of BioCtrl0Tcc
		/** 18 */	RMuint32 rsvd2:4;
		/** 22 */	RMuint32 ksudsFilteruvcoldsscale2p:5; // NA
		/** 27 */	RMuint32 rsvd3:5;
	} bits;
	RMuint32 value;
};
#endif //xx end specific SMP8760 registers

union TccEdrDMBioKsuds02Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksudsMaxus:16;
		/** 16 */	RMuint32 ksudsMinus:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds03Reg { //xx swapped fields - duplicated in TCC out ?
	struct {
		/** 00 */	RMuint32 ksudsMaxds:16;
		/** 16 */	RMuint32 ksudsMinds:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds04Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvrowus0M1:16;
		/** 16 */	RMuint32 ksudsFilteruvrowus0M0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds07Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvrowus1M1:16;
		/** 16 */	RMuint32 ksudsFilteruvrowus1M0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds10Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcolusM1:16;
		/** 16 */	RMuint32 ksudsFilteruvcolusM0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds11Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcolusM3:16;
		/** 16 */	RMuint32 ksudsFilteruvcolusM2:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds12Reg { // u9 swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcoldsM1:16;
		/** 16 */	RMuint32 ksudsFilteruvcoldsM0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds13Reg { // u9 swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcoldsM3:16;
		/** 16 */	RMuint32 ksudsFilteruvcoldsM2:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds14Reg { // u9 swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcoldsM5:16;
		/** 16 */	RMuint32 ksudsFilteruvcoldsM4:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsuds15Reg { // u9 swapped fields
	struct {
		/** 00 */	RMuint32 ksudsFilteruvcoldsM7:16;
		/** 16 */	RMuint32 ksudsFilteruvcoldsM6:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap00Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33yuv2rgb00:16;
		/** 16 */	RMuint32 ksimapM33yuv2rgbscale2p:5;
		/** 21 */	RMuint32 rsvd0:3;
		/** 24 */	RMuint32 ksimapClr:8;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap01Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33yuv2rgb02:16;
		/** 16 */	RMuint32 ksimapM33yuv2rgb01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap02Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33yuv2rgb11:16;
		/** 16 */	RMuint32 ksimapM33yuv2rgb10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap03Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33yuv2rgb20:16;
		/** 16 */	RMuint32 ksimapM33yuv2rgb12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap04Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33yuv2rgb22:16;
		/** 16 */	RMuint32 ksimapM33yuv2rgb21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap05Reg {
	struct {
		/** 00 */	RMuint32 ksimapV3yuv2rgboffinrgb0:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap06Reg {
	struct {
		/** 00 */	RMuint32 ksimapV3yuv2rgboffinrgb1:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap07Reg {
	struct {
		/** 00 */	RMuint32 ksimapV3yuv2rgboffinrgb2:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap08Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapEotfparamRange:16;
		/** 16 */	RMuint32 ksimapEotfparamRangemin:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap09Reg {
	struct {
		/** 00 */	RMuint32 ksimapEotfparamRangeinv:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap10Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapEotfparamGamma:16;
		/** 16 */	RMuint32 ksimapEotfparamEotf:8;
		/** 24 */	RMuint32 ksimapEotfparamBdp:5;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap11Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapEotfparamB:16;
		/** 16 */	RMuint32 ksimapEotfparamA:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap12Reg {
	struct {
		/** 00 */	RMuint32 ksimapEotfparamG:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap13Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33rgb2lms00:16;
		/** 16 */	RMuint32 ksimapM33rgb2lmsscale2p:5;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap14Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33rgb2lms02:16;
		/** 16 */	RMuint32 ksimapM33rgb2lms01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap15Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33rgb2lms11:16;
		/** 16 */	RMuint32 ksimapM33rgb2lms10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap16Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33rgb2lms20:16;
		/** 16 */	RMuint32 ksimapM33rgb2lms12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap17Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33rgb2lms22:16;
		/** 0016 */	RMuint32 ksimapM33rgb2lms21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap18Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33lms2ipt00:16;
		/** 16 */	RMuint32 ksimapM33lms2iptscale2p:5;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap19Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33lms2ipt02:16;
		/** 16 */	RMuint32 ksimapM33lms2ipt01:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap20Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33lms2ipt11:16;
		/** 16 */	RMuint32 ksimapM33lms2ipt10:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap21Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33lms2ipt20:16;
		/** 16 */	RMuint32 ksimapM33lms2ipt12:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap22Reg { // swapped fields
	struct {
		/** 00 */	RMuint32 ksimapM33lms2ipt22:16;
		/** 16 */	RMuint32 ksimapM33lms2ipt21:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap23Reg {
	struct {
		/** 00 */	RMuint32 ksimapIptscale:32;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap24Reg { // more than swapped fields
	struct {
		/** 00 */	RMuint32 ksimapV3iptoff0:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioKsimap25Reg { // more than swapped fields
	struct {
		/** 00 */	RMuint32 ksimapV3iptoff2:16;
		/** 16 */	RMuint32 ksimapV3iptoff1:16;
	} bits;
	RMuint32 value;
};

union TccEdrDMBioCtrl1Reg { //xx
	struct {
		/** 00 */	RMuint32 ksdmctrlShadowcontext_cvm:1; // shadow LUT control for input csc
		/** 01 */	RMuint32 ksdmctrlShadowcontext_icsc:1;  // shadow LUT control for cvm or tone mapping
		/** 02 */	RMuint32 rsvd0:30;
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown1Reg { //xx ??
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

union TccEdrDMUnknown2Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_iwin_b:13;  // manul B of in-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_iwin_g:13;  // manul G of in-win
		/** 29 */	RMuint32 rsvd1:3;
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown3Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_owin_r:13;    // manul R of out-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_owin_alpha:8; // manul alpha of out-win, nonuse in V_EDR
		/** 24 */	RMuint32 win_ext_owin_en:1;    // 1'b1 set RGBa which outside of window as manul value for debug purpose
		/** 25 */	RMuint32 rsvd1:7;
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown4Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_owin_b:13;  // manul B of out-win
		/** 13 */	RMuint32 rsvd0:3;
		/** 16 */	RMuint32 win_ext_owin_g:13;  // manul G of out-win
		/** 29 */	RMuint32 rsvd1:3;
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown5Reg { //xx ??
	struct {
		/** 00 */	RMuint32 b_value:9;
		/** 09 */	RMuint32 g_value:9;
		/** 18 */	RMuint32 r_value:9;
		/** 27 */	RMuint32 alpha:4;
		/** 31 */	RMuint32 border_enable:1; // enable in-win border for debug purpose
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown6Reg { //xx ??
	struct {
		/** 00 */	RMuint32 b_value:9;
		/** 09 */	RMuint32 g_value:9;
		/** 18 */	RMuint32 r_value:9;
		/** 27 */	RMuint32 alpha:4;
		/** 31 */	RMuint32 border_enable:1; // enable out-win border for debug purpose
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown7Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_vstart:16; // vde start position for extended window
		/** 16 */	RMuint32 win_ext_hstart:16; // hde start position for extended window
	} bits;
	RMuint32 value;
};

union TccEdrDMUnknown8Reg { //xx ??
	struct {
		/** 00 */	RMuint32 win_ext_vsize:16; // vde size for extended window
		/** 16 */	RMuint32 win_ext_hsize:16; // hde size for extended window
	} bits;
	RMuint32 value;
};

struct TccEdrCoreV1Reg {
	// base address V_EDR = 0x1253C000 - derived from struct VsyncRegVpEdrStbCoreV1Reg
	volatile union TccEdrCoreGlb0Reg glb0;                             //  0
	volatile union TccEdrCoreResetReg reset;                           //  4
	volatile union TccEdrCoreGlb1Reg glb1;                             //  8
	volatile union TccEdrCoreComp1ExtReg comp1Ext;                     //  c
	volatile union TccEdrCoreGlb2Reg glb2;                             // 10
	volatile union TccEdrCoreUnknown5Reg unk5;                         // 14
	volatile union TccEdrCoreUnknown6Reg unk6;                         // 18
	volatile union TccEdrCoreUnknown7Reg unk7;                         // 1c
	volatile union TccEdrCoreUnknown8Reg unk8;                         // 20
	volatile union TccEdrCoreUnknown9Reg unk9;                         // 24
	volatile union TccEdrCoreUnknown10Reg unk10;                       // 28
	volatile RMuint32 rsvd1[53];                                       // 2c .. fc
	volatile union TccEdrCoreComp0Reg comp0;                           // 100
	volatile union TccEdrCoreComp1Reg comp1;                           // 104
	volatile union TccEdrCoreComp2Reg comp2;                           // 108
	volatile union TccEdrCoreComp3Reg comp3;                           // 10c
	volatile union TccEdrCoreComp4Reg comp4;                           // 110
	volatile union TccEdrCoreComp5Reg comp5;                           // 114
	volatile union TccEdrCoreComp6Reg comp6;                           // 118
	volatile union TccEdrCoreComp7Reg comp7;                           // 11c
	volatile union TccEdrCoreComp8Reg comp8;                           // 120
	volatile union TccEdrCoreBlMapCoeffUReg blMapCoeffU[27];           // 124 .. 18c
	volatile union TccEdrCoreBlMapCoeffVReg blMapCoeffV[27];           // 190 .. 1f8
	volatile union TccEdrCoreComp63Reg comp63;                         // 1fc
	volatile union TccEdrCoreComp64Reg comp64;                         // 200
	volatile union TccEdrCoreElNlqCoeffY6332Reg elNlqCoeffY6332;       // 204
	volatile union TccEdrCoreElNlqCoeffY310Reg elNlqCoeffY310;         // 208
	volatile union TccEdrCoreComp67Reg comp67;                         // 20c
	volatile union TccEdrCoreElNlqCoeffU6332Reg elNlqCoeffU6332;       // 210
	volatile union TccEdrCoreElNlqCoeffU310Reg elNlqCoeffU310;         // 214
	volatile union TccEdrCoreComp70Reg comp70;                         // 218
	volatile union TccEdrCoreBlMapPivotU310Reg blMapPivotU310;         // 21c
	volatile union TccEdrCoreComp72Reg comp72;                         // 220
	volatile union TccEdrCoreBlMapPivotY310Reg blMapPivotY310;         // 224
	volatile union TccEdrCoreBlMapPivotY6332Reg blMapPivotY6332;       // 228
	volatile union TccEdrCoreBlMapPivotY9564Reg blMapPivotY9564;       // 22c
	volatile union TccEdrCoreUpsamplingCoeffYReg upsamplingCoeffY[52]; // 230 .. 2fc
	volatile union TccEdrCoreBlMapCoeffYReg blMapCoeffY[24];           // 300 .. 35c
	volatile RMuint32 rsvd2[40];                                       // 360 .. 3fc
};

struct TccEdrDMVideoV1Reg {
	// base address = 0x1253c400 - derived from struct VsyncRegVpEdrStbSourceDisplayManagementV1Reg
	volatile union TccEdrDMBioCtrl0Reg bioCtrl0;       // 00 //xx combined with bioKsuds01
	volatile union TccEdrDMBioKsuds02Reg bioKsuds02;   // 04
	volatile union TccEdrDMBioKsuds03Reg bioKsuds03;   // 08 //xx repeated in TCC_out_Reg!
	volatile union TccEdrDMBioKsuds04Reg bioKsuds04;   // 0c
	volatile union TccEdrDMBioKsuds07Reg bioKsuds07;   // 10
	volatile union TccEdrDMBioKsuds10Reg bioKsuds10;   // 14
	volatile union TccEdrDMBioKsuds11Reg bioKsuds11;   // 18
	volatile RMuint32 rsvd0[4];                        // 1c..28 TCC_out_Reg bioKsuds12 .. bioKsuds15, partially moved to TCC_out_Reg!
	volatile union TccEdrDMBioKsimap00Reg bioKsimap00; // 2c
	volatile union TccEdrDMBioKsimap01Reg bioKsimap01; // 30
	volatile union TccEdrDMBioKsimap02Reg bioKsimap02; // 34
	volatile union TccEdrDMBioKsimap03Reg bioKsimap03; // 38
	volatile union TccEdrDMBioKsimap04Reg bioKsimap04; // 3c
	volatile union TccEdrDMBioKsimap05Reg bioKsimap05; // 40
	volatile union TccEdrDMBioKsimap06Reg bioKsimap06; // 44
	volatile union TccEdrDMBioKsimap07Reg bioKsimap07; // 48
	volatile union TccEdrDMBioKsimap08Reg bioKsimap08; // 4c
	volatile union TccEdrDMBioKsimap09Reg bioKsimap09; // 50
	volatile union TccEdrDMBioKsimap10Reg bioKsimap10; // 54
	volatile union TccEdrDMBioKsimap11Reg bioKsimap11; // 58
	volatile union TccEdrDMBioKsimap12Reg bioKsimap12; // 5c
	volatile union TccEdrDMBioKsimap13Reg bioKsimap13; // 60
	volatile union TccEdrDMBioKsimap14Reg bioKsimap14; // 64
	volatile union TccEdrDMBioKsimap15Reg bioKsimap15; // 68
	volatile union TccEdrDMBioKsimap16Reg bioKsimap16; // 6c
	volatile union TccEdrDMBioKsimap17Reg bioKsimap17; // 70
	volatile union TccEdrDMBioKsimap18Reg bioKsimap18; // 74
	volatile union TccEdrDMBioKsimap19Reg bioKsimap19; // 78
	volatile union TccEdrDMBioKsimap20Reg bioKsimap20; // 7c
	volatile union TccEdrDMBioKsimap21Reg bioKsimap21; // 80
	volatile union TccEdrDMBioKsimap22Reg bioKsimap22; // 84
	volatile union TccEdrDMBioKsimap23Reg bioKsimap23; // 88
	volatile union TccEdrDMBioKsimap24Reg bioKsimap24; // 8c
	volatile union TccEdrDMBioKsimap25Reg bioKsimap25; // 90
	volatile union TccEdrDMBioCtrl1Reg bioCtrl1;       // 94
	volatile RMuint32 rsvd1[10];                       // 98 .. bc
	volatile union TccEdrDMUnknown1Reg unk1;           // c0
	volatile union TccEdrDMUnknown2Reg unk2;           // c4
	volatile union TccEdrDMUnknown3Reg unk3;           // c8
	volatile union TccEdrDMUnknown4Reg unk4;           // cc
	volatile union TccEdrDMUnknown5Reg unk5;           // d0
	volatile union TccEdrDMUnknown6Reg unk6;           // d4
	volatile union TccEdrDMUnknown7Reg unk7;           // d8
	volatile union TccEdrDMUnknown8Reg unk8;           // dc
	volatile RMuint32 rsvd2[8];                        // e0 .. fc
};

struct TccEdrVideoV1Reg {
	// base address = V_EDR = 0x1253C000
	volatile struct TccEdrCoreV1Reg core;  //0x1253C000 ~ 0x1253C3FF
	volatile struct TccEdrDMVideoV1Reg dm; //0x1253C400 ~
};

#endif /** #ifndef __TCC_EDR_VIDEO_V1_H__ */

