// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/delay.h>

#include <video/tcc/tcc805x/vioc_disp.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"

//#define ENABLE_AVGEN_AUDIO_DEBUG

#define	IEC60958_3_CH_STATUS_MODE0_LEFT							0x80	
#define	IEC60958_3_CH_STATUS_MODE0_RIGHT						0x40

#define	IEC60958_3_CH_STATUS_MAX_WORD_LENGTH_20BITS				0x00	
#define	IEC60958_3_CH_STATUS_MAX_WORD_LENGTH_24BITS				0x08

#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_20BITS			0x04
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_21BITS			0x03
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_22BITS			0x02
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_23BITS			0x01
#define	IEC60958_3_CH_STATUS_SAMPLE_WORD_LENGTH_24BITS			0x05

#define AUDIO_INFOFREAME_HEADER				0x481B8400

static bool dptx_avgen_set_video_sampler( struct Dptx_Params *pstDptx, u8 ucStream_Index )	
{
	u8							ucVideoMapping = 0;
	u32							uiRegMap_VidSampleCtrl;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;

	/*
	 * VSAMPLE_CTRL[0x300 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1]: This register has various control for enabling different setting of the video sampler
	 * -.Reserved[Bit3:0]
	 * -.INTERNAL_DE_GENERATOR[Bit4]( 0: Data enable signal available at the input, 1: Enable internal data generator) 
	 *								  When the external logic which drives the pixel data doesn't generate the video data enable, then enabling this biit forces the controller to generate the video enable internally.	
	 * -.VIDEO_STREAM_ENABLE[Bit5]( 0: Video stream disable, 1: Video stream enable ) 
	 * -.VIDEO_MAPPING[Bit20:16]( 0: RGB 6bits, 1: RGB 8bits, 2: RGB 10bits, 3: RGB 12bits, 4: RGB 16bits ) 
	 *							 ( 5: YCbCr444 8bits, 6: YCbCr444 10bits, 7: YCbCr444 12bits, 8: YCbCr444 16bits )
	 *							 ( 9: YCbCr422 8bits, 10: YCbCr422 10bits, 11: YCbCr422 12bits, 12: YCbCr422 16bits )
	 *							 ( 13: YCbCr420 8bits, 14: YCbCr420 10bits, 15: YCbCr420 12bits, 16: YCbCr420 16bits )
	 *							 ( 17: YOnly 8bits, 18: YOnly 10bits, 19: YOnly 12bits, 20: YOnly 16bits )
	 *							 ( 23: RAW 8bits, 24: RAW 10bits, 25: RAW 12bits, 27: RAW 16bits )
	 * -.PIXEL_MODE_SELECT[Bit22:21]( 0: Single pixel mode, 1:Dual pixel mode, 2: Quad pixel mode, 3: Reserved ) 
	 * -.ENABLE_DSC[Bit b23] : Set this bit to 1 to enable Display Stream Compression( DSC ) on this specific stream
	 */

	uiRegMap_VidSampleCtrl = Dptx_Reg_Readl( pstDptx, DPTX_VSAMPLE_CTRL_N( ucStream_Index ) );
	
	uiRegMap_VidSampleCtrl &= ~( DPTX_VSAMPLE_CTRL_VMAP_BPC_MASK | DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK );

	switch( (enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding ) 
	{
		case PIXEL_ENCODING_TYPE_RGB:
			if( pstVideoParams->ucBitPerComponent == (u8)PIXEL_COLOR_DEPTH_8 )
			{
				ucVideoMapping = 1;
			}
			else
			{
				dptx_err( "Invalid bpc = %d ", pstVideoParams->ucBitPerComponent );
				return ( DPTX_RETURN_FAIL );
			}
			break;
		case PIXEL_ENCODING_TYPE_YCBCR422:
			if( pstVideoParams->ucBitPerComponent == (u8)PIXEL_COLOR_DEPTH_8 )
			{
				ucVideoMapping = 9;
			}
			else
			{
				dptx_err( "Invalid bpc = %d ", pstVideoParams->ucBitPerComponent );
				return ( DPTX_RETURN_FAIL );
			}
			break;	
			
		case PIXEL_ENCODING_TYPE_YCBCR444:
			if( pstVideoParams->ucBitPerComponent == (u8)PIXEL_COLOR_DEPTH_8 )
			{
				ucVideoMapping = 5;
			}
			else
			{
				dptx_err( "Invalid bpc = %d ", pstVideoParams->ucBitPerComponent );
				return ( DPTX_RETURN_FAIL );
			}
			break;
		default:
			dptx_err( "Invalid encoding type = %d ", (u32)pstVideoParams->ucPixel_Encoding );
			return ( DPTX_RETURN_FAIL );
			break;
	}

	uiRegMap_VidSampleCtrl |= ( ucVideoMapping << DPTX_VSAMPLE_CTRL_VMAP_BPC_SHIFT );
	uiRegMap_VidSampleCtrl |= ( pstVideoParams->ucMultiPixel << DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VSAMPLE_CTRL_N( ucStream_Index ), uiRegMap_VidSampleCtrl );

	return ( DPTX_RETURN_SUCCESS );
}

static bool dptx_avgen_set_video_config( struct Dptx_Params *pstDptx, u8 ucStream_Index )	
{
	u32							uiVideo_Code, uiRegMap_VidConfig = 0;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params		*pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

{	/* Configure video_config1 register 
	*
	* VIDEO_CONFIG1[0x310 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Video configuration register 1
	*	-.HACTIVE[Bit31:16] : Input video Horizontal active pixel region width. Number of Horizontal active pixels[0 ~ 8191]
	*	-.HBLANK[Bit15:2] : Input video Horizontal blanking pixel region width. Number of Horizontal active pixels[0 ~ 4095] 
	*	-.I_P[bit1] : ( 0 : Progress, 1 : Interlaced )
	*	-.R_V_BLANK_IN_OSC[bit0] : Used for CEA861-D modes with fractional Vblank( for example, 5,6,7,11,20,21 and 22 ). Set this field to 1 for video mode 39, although there is no Vblank oscillation.
	*/

	uiVideo_Code = pstVideoParams->auiVideo_Code[ucStream_Index];

	if( pstVideoParams->ucVideo_Format == VIDEO_FORMAT_CEA_861 ) 
	{
		if( uiVideo_Code == 5 || uiVideo_Code == 6 || uiVideo_Code == 7 ||  uiVideo_Code == 10 || uiVideo_Code == 11 || uiVideo_Code == 20 ||
			uiVideo_Code == 21 || uiVideo_Code == 22 || uiVideo_Code == 39 || uiVideo_Code == 25 || uiVideo_Code == 26 || uiVideo_Code == 40 ||
			uiVideo_Code == 44 || uiVideo_Code == 45 || uiVideo_Code == 46 || uiVideo_Code == 50 || uiVideo_Code == 51 || uiVideo_Code == 54 ||
			uiVideo_Code == 55 || uiVideo_Code == 58 || uiVideo_Code  == 59 )
		{
			uiRegMap_VidConfig |= DPTX_VIDEO_CONFIG1_IN_OSC_EN;
		}
	}

	if( pstDtd->interlaced == 1 )
	{
		uiRegMap_VidConfig |= DPTX_VIDEO_CONFIG1_O_IP_EN;
	}

	uiRegMap_VidConfig |= ( pstDtd->h_active << DPTX_VIDEO_H_ACTIVE_SHIFT );
	uiRegMap_VidConfig |= ( pstDtd->h_blanking << DPTX_VIDEO_H_BLANK_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_CONFIG1_N( ucStream_Index ), uiRegMap_VidConfig );
}

{	/* Configure video_config2 register 
	*
	* VIDEO_CONFIG2[0x314 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Video configuration register 2
	*	-.VACTIVE[Bit15:0] : Input video Vertical  active pixel region width. Number of Horizontal active pixels[0 ~ 255] 
	*	-.VBLANK[Bit31:16] : Input video Vertical blanking pixel region width. This bit field holds bits 7:0
	*/

	uiRegMap_VidConfig = 0;

	uiRegMap_VidConfig |= ( pstDtd->v_active << DPTX_VIDEO_V_ACTIVE_SHIFT );
	uiRegMap_VidConfig |= ( pstDtd->v_blanking << DPTX_VIDEO_V_BLANK_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_CONFIG2_N( ucStream_Index ), uiRegMap_VidConfig);
}

{	/* Configure video_config3 register 
	*
	* VIDEO_CONFIG3[0x314 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Video configuration register 3
	*	-.H_FRONT_PORCH[Bit15:0] : Input video Hsync active edge delay.[0 ~ 4095] 
	*	-.H_SYNC_WIDTH[Bit31:16] : Input video Hsync active pluse delay.[0 ~ 511] 
	*/

	uiRegMap_VidConfig = 0;

	uiRegMap_VidConfig |= ( pstDtd->h_sync_offset << DPTX_VIDEO_H_FRONT_PORCH_SHIFT );
	uiRegMap_VidConfig |= ( pstDtd->h_sync_pulse_width << DPTX_VIDEO_H_SYNC_WIDTH_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_CONFIG3_N( ucStream_Index ), uiRegMap_VidConfig );
}

{	/* Configure video_config4 register 
	*
	* VIDEO_CONFIG4[0x31C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Video configuration register 3
	*	-.V_FRONT_PORCH[Bit15:0] : Input video Vsync active edge delay.[0 ~ 255] 
	*	-.V_SYNC_WIDTH[Bit31:16] : Input video Vsync active pluse delay.[0 ~ 63] 
	*/

	uiRegMap_VidConfig = 0;

	uiRegMap_VidConfig |= ( pstDtd->v_sync_offset << DPTX_VIDEO_V_FRONT_PORCH_SHIFT );
	uiRegMap_VidConfig |= ( pstDtd->v_sync_pulse_width << DPTX_VIDEO_V_SYNC_WIDTH_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_CONFIG4_N( ucStream_Index ), uiRegMap_VidConfig );
}

{	/* Configure video_config5 register 
	*
	* VIDEO_CONFIG5[0x320 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Video configuration register 3
	*	-.AVERAGE_BYTES_PER_TU[Bit6:0] : This is the average numvber of valid bytes in a transfer Unit. Driver should program this register based on the video format being transmitted and the likn rate and lane configuration being used
	*	-.AVERAGE_BYTES_PER_TU_FRAC[Bit19:14] : This is the fractional portion of value calculated in the average bytes per TU. The first decimal value in fractional part should be programmed here
	*	-.INIT_THRESHOLD[Bit13:7] : Refer to register map
	*/

	/*
	*  It's done in Dptx_Avgen_Calculate_Video_Average_TU_Symbols() in Dptx_Avgen_Set_Video_Code()
	*  Dptx_Avgen_Set_Video_Code() -> Dptx_Avgen_Set_Video_Timing() -> dptx_avgen_config_video_input() -> dptx_avgen_set_video_config()
	*/
	uiRegMap_VidConfig = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_CONFIG5_N( ucStream_Index ) );

	uiRegMap_VidConfig = uiRegMap_VidConfig & (~DPTX_VIDEO_CONFIG5_TU_MASK);
	uiRegMap_VidConfig = ( uiRegMap_VidConfig | ( pstVideoParams->ucAverage_BytesPerTu << DPTX_VIDEO_CONFIG5_TU_SHIFT ) );
	
	if( pstDptx->bMultStreamTransport ) 
	{
		uiRegMap_VidConfig &= ( ~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_MST );
		uiRegMap_VidConfig |= ( pstVideoParams->ucAver_BytesPer_Tu_Frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_MST );
	}
	else 
	{
		uiRegMap_VidConfig &= ( ~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_SST );
		uiRegMap_VidConfig |= ( pstVideoParams->ucAver_BytesPer_Tu_Frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_SST );
	}

	uiRegMap_VidConfig &= ( ~DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_MASK );
	uiRegMap_VidConfig |= ( pstVideoParams->ucInit_Threshold << DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_CONFIG5_N( ucStream_Index ), uiRegMap_VidConfig );
}

	return ( DPTX_RETURN_SUCCESS );
}


static bool dptx_avgen_set_video_msa( struct Dptx_Params *pstDptx, u8 ucStream_Index )
{
	u8							ucBpc_Mapping = 0, ucColorimetry_Mapping = 0;
	u32							uiRegMap_Msa1 = 0, uiRegMap_Msa2, uiRegMap_Msa3;
	struct Dptx_Video_Params	*pstVideoParams =	&pstDptx->stVideoParams;
	struct Dptx_Dtd_Params		*pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

{	/* Configure video_msa1 register 
	*
	* VIDEO_MSA1[0x324 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Used for programming the Main Stream Attributes. MSA gets transmitted after video is enabled.
	*	-.HSTART[Bit15:0] : Horizontal active start from leading edge of Hsync measured in pixel count, This is same as Hsync width + Vertical back porch.
	*	-.VSTART[Bit31:16] : Vertical active start from leading edge of Hsync measured in pixel count, This is same as Vsync width + Vertical back porch
	*/

	uiRegMap_Msa1 |= (( pstDtd->h_blanking - pstDtd->h_sync_offset ) << DPTX_VIDEO_MSA1_H_START_SHIFT );  // 160( 640 + 160 = 800 => Real width ) - 16 = 144
	uiRegMap_Msa1 |= (( pstDtd->v_blanking - pstDtd->v_sync_offset ) << DPTX_VIDEO_MSA1_V_START_SHIFT );
	
	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_MSA1_N( ucStream_Index ), uiRegMap_Msa1 );
}

{  /* Configure video_msa2 register 
	*
	* VIDEO_MSA3[0x32C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Used for programming the Main Stream attributes. 
	*	-.MISC0[Bit31:24]( Bit24: Synchronous mode of operation<0: Asynchronous, 1: Synchronous >, Bit25~28: Colorimetry, Bit29~31: MISC0<Bit 1~4> )
	*	-.MVID[Bit23:0]: To be used in case of synchronous mode of operation
	*/
	uiRegMap_Msa2 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_MSA2_N( ucStream_Index ) );

	uiRegMap_Msa2 &= ~( DPTX_VIDEO_VMSA2_BPC_MASK | DPTX_VIDEO_VMSA2_COL_MASK );

	/* Refer to Table 2-96 in DisplayPort spec 1.4 */
	switch( (enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding ) 
	{
		case PIXEL_ENCODING_TYPE_RGB:
			if( pstVideoParams->ucRGB_Standard == RGB_STANDARD_S )
			{
				ucColorimetry_Mapping = 4;
			}
			else if( pstVideoParams->ucRGB_Standard == RGB_STANDARD_LEGACY )
			{
				ucColorimetry_Mapping = 0;
			}
			break;
		case PIXEL_ENCODING_TYPE_YCBCR422:
			if( pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU601 )
			{
				ucColorimetry_Mapping = 5;
			}
			else if( pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU709 )
			{
				ucColorimetry_Mapping = 13;
			}
			break;	
		case PIXEL_ENCODING_TYPE_YCBCR444:
			if( pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU601 )
			{
				ucColorimetry_Mapping = 6;
			}
			else if( pstVideoParams->ucColorimetry == (u8)COLORIMETRY_ITU709 )
			{
				ucColorimetry_Mapping = 14;
			}
			break;	
		default:
			dptx_err("Invalid encoding type = 0x%x ", pstVideoParams->ucPixel_Encoding );
			return ( DPTX_RETURN_FAIL );
			break;
	}

	switch( (enum VIDEO_PIXEL_ENCODING_TYPE)pstVideoParams->ucPixel_Encoding ) 
	{
		case PIXEL_ENCODING_TYPE_RGB: /*  000: 6bpc, 001: 8bpc, 010: 10bpc, 011: 12bpc, 100: 16bpc*/
			if( pstVideoParams->ucBitPerComponent == PIXEL_COLOR_DEPTH_8 )
			{
				ucBpc_Mapping = 1;
			}
			else
			{
				dptx_err( "Invalid bpc = %d ", pstVideoParams->ucBitPerComponent );
				return ( DPTX_RETURN_FAIL );
			}
			break;
		case PIXEL_ENCODING_TYPE_YCBCR422: /* 001: 8bpc, 010: 10bpc, 011: 12bpc, 100: 16bpc */
		case PIXEL_ENCODING_TYPE_YCBCR444:	
			if( pstVideoParams->ucBitPerComponent == PIXEL_COLOR_DEPTH_8 )
			{
				ucBpc_Mapping = 1;
			}
			else
			{
				dptx_err( "Invalid bpc = %d ", pstVideoParams->ucBitPerComponent );
				return ( DPTX_RETURN_FAIL );
			}
			break;	
		default:
			dptx_err("Invalid encoding type = 0x%x ", pstVideoParams->ucPixel_Encoding );
			return ( DPTX_RETURN_FAIL );
			break;	
	}

	uiRegMap_Msa2 |= ( ucColorimetry_Mapping << DPTX_VIDEO_VMSA2_COL_SHIFT );
	uiRegMap_Msa2 |= ( ucBpc_Mapping << DPTX_VIDEO_VMSA2_BPC_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_MSA2_N( ucStream_Index ), uiRegMap_Msa2 );
}


{  /* Configure video_msa3 register 
	*
	* VIDEO_MSA3[0x32C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Used for programming the Main Stream attributes. 
	*	-.MISC1[Bit31:24]( Bit30: Bit6 of MISC1, Bit31: Bit7 of MISC1 )
	*	-.NVID[Bit23:0]: To be used in case of synchronous mode of operation
	*/
	uiRegMap_Msa3 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_MSA3_N( ucStream_Index ) );

	uiRegMap_Msa3 &= ~DPTX_VIDEO_VMSA3_PIX_ENC_MASK;

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_MSA3_N( ucStream_Index ), uiRegMap_Msa3 );
}
	
	return ( DPTX_RETURN_SUCCESS );
}

static bool dptx_avgen_set_video_hblank_interval( struct Dptx_Params *pstDptx, u8 ucStream_Index )	
{
	u32							uiRegMap_VidHblankInterval;
	u32							uiLink_Clock, uiHBlank_Interval;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params		*pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

	/* Configure DPTX_VSAMPLE_POLARITY_CTRL register 
	*
	* VINPUT_HBLANK_INTERVAL[0x330 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Number of symbols to be transmitted during the hblank interval.
	*	-.HBLANK_INTERVAL_EN[Bit0]( 0: Disable, 1: Enable ) 
	*	-.HBLANK_INTERVAL[Bit15:0]: For MST mode, the programmed value should be ( Hblank / 16 ) * average_byte_per_tu * link_clk / pixel_clk. 
	*							    In SST mode, it should be hblank * link_clk / pixel_clk
	*/

	switch( pstDptx->stDptxLink.ucLinkRate ) 
	{
		case DPTX_PHYIF_CTRL_RATE_RBR:
			uiLink_Clock = 40500;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR:
			uiLink_Clock = 67500;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			uiLink_Clock = 135000;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			uiLink_Clock = 202500;
			break;
		default:
			dptx_err( "Invalid rate 0x%x ", pstDptx->stDptxLink.ucLinkRate );
			return ( DPTX_RETURN_FAIL );
	}

	/* It needs to check MST => "For MST mode, the programmed value should be ( Hblank / 16 ) * average_byte_per_tu * link_clk / pixel_clk" */ 
	if( pstDptx->bMultStreamTransport )
	{
		uiHBlank_Interval = (( pstDtd->h_blanking / 16 ) * pstVideoParams->ucAverage_BytesPerTu * uiLink_Clock / pstDtd->uiPixel_Clock ); // ( 280 / 16 * 26 = 442 *  67500 / 148500 ) = 200
	}
	else
	{
		uiHBlank_Interval = ( pstDtd->h_blanking * uiLink_Clock / pstDtd->uiPixel_Clock );	// 280 * 67500 / 148500 = 127( 0x7F)
	}

	uiRegMap_VidHblankInterval = uiHBlank_Interval;

	/* Guild from SoC */
	uiRegMap_VidHblankInterval |= ( DPTX_VIDEO_HBLANK_INTERVAL_ENABLE << DPTX_VIDEO_HBLANK_INTERVAL_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_VIDEO_HBLANK_INTERVAL_N( ucStream_Index ), uiRegMap_VidHblankInterval );

	return ( DPTX_RETURN_SUCCESS );
}

static void dptx_avgen_active_audio_channels(struct Dptx_Params *dptx, uint8_t ucStream_Index, uint8_t ucNumOfChannels, uint8_t ucEnable)
{
	u32 uiEnabledChannels = 0, uiRegMap_EnableAudioChannels = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_EnableAudioChannels = Dptx_Reg_Readl(dptx, uiReg_Offset);
	uiRegMap_EnableAudioChannels &= ~DPTX_AUD_CONFIG1_DATA_EN_IN_MASK;

	if (ucEnable) {
		switch (ucNumOfChannels) {
			case INPUT_MAX_1_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_1;
				break;
			case INPUT_MAX_2_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_2;
				break;
			case INPUT_MAX_3_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_3;
				break;
			case INPUT_MAX_4_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_4;
				break;
			case INPUT_MAX_5_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_5;
				break;
			case INPUT_MAX_6_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_6;
				break;
			case INPUT_MAX_7_CH:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_7;
				break;
			case INPUT_MAX_8_CH:
			default:
				uiEnabledChannels = DPTX_EN_AUDIO_CH_8;
				break;
		}
		uiRegMap_EnableAudioChannels |= ( uiEnabledChannels << DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT );
	} else {
		switch (ucNumOfChannels) {
			case INPUT_MAX_1_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_1;
				break;
			case INPUT_MAX_2_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_2;
				break;
			case INPUT_MAX_3_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_3;
				break;
			case INPUT_MAX_4_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_4;
				break;
			case INPUT_MAX_5_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_5;
				break;
			case INPUT_MAX_6_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_6;
				break;
			case INPUT_MAX_7_CH:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_7;
				break;
			case INPUT_MAX_8_CH:
			default:
				uiEnabledChannels = ~DPTX_EN_AUDIO_CH_8;
				break;
		}
		uiRegMap_EnableAudioChannels &= ( uiEnabledChannels << DPTX_AUD_CONFIG1_DATA_EN_IN_SHIFT );
	}

	Dptx_Reg_Writel( dptx, uiReg_Offset, uiRegMap_EnableAudioChannels );
}

static bool dptx_avgen_config_video_input( struct Dptx_Params *pstDptx, u8 ucStream_Index )
{
	bool 						bRetVal;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params		*pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

	bRetVal = dptx_avgen_set_video_sampler( pstDptx, ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

{	/* Configure DPTX_VSAMPLE_POLARITY_CTRL register 
	*
	* VINPUT_POLARITY_CTRL[0x30C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register controls the polarity of the video input signals
	*	-.VSYNC_IN_POLARITY[Bit0]( 0: Active low, 1:Active high ) 
	*	-.HSYNC_IN_POLARITY[Bit1]( 0: Active low, 1:Active high ) 
	*/
	u32 	uiRegMap_VidPolarityCtrl = 0;

	if( pstDtd->h_sync_polarity == 1 )
	{
		uiRegMap_VidPolarityCtrl |= DPTX_POL_CTRL_H_SYNC_POL_EN;
	}
	if( pstDtd->v_sync_polarity == 1 )
	{
		uiRegMap_VidPolarityCtrl |= DPTX_POL_CTRL_V_SYNC_POL_EN;
	}

	Dptx_Reg_Writel( pstDptx, DPTX_VSAMPLE_POLARITY_CTRL_N( ucStream_Index ), uiRegMap_VidPolarityCtrl );

}
	bRetVal = dptx_avgen_set_video_config( pstDptx, ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal = dptx_avgen_set_video_msa( pstDptx, ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal = dptx_avgen_set_video_hblank_interval( pstDptx, ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	return ( DPTX_RETURN_SUCCESS );
}

static void dptx_avgen_reset_dtd( struct Dptx_Dtd_Params *pstDtd )
{
	pstDtd->pixel_repetition_input	= 0;
	pstDtd->uiPixel_Clock			= 0;
	pstDtd->h_active				= 0;
	pstDtd->h_blanking				= 0;
	pstDtd->h_sync_offset			= 0;
	pstDtd->h_sync_pulse_width		= 0;
	pstDtd->h_image_size			= 0;
	pstDtd->v_active				= 0;
	pstDtd->v_blanking				= 0;
	pstDtd->v_sync_offset			= 0;
	pstDtd->v_sync_pulse_width		= 0;
	pstDtd->v_image_size			= 0;
	pstDtd->interlaced				= 0;
	pstDtd->v_sync_polarity			= 0;
	pstDtd->h_sync_polarity			= 0;
}

bool Dptx_Avgen_Init_Video_Params( struct Dptx_Params *pstDptx, u32 uiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX] )
{
	bool						bRetVal;
	u8							ucStream_Index;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;

	pstVideoParams->ucMultiPixel		= (u8)MULTI_PIXEL_TYPE_SINGLE;
	pstVideoParams->ucBitPerComponent	= (u8)PIXEL_COLOR_DEPTH_8; /* Guild from SoC */	
	pstVideoParams->ucColorimetry		= (u8)COLORIMETRY_ITU601;
	pstVideoParams->ucRGB_Standard		= (u8)RGB_STANDARD_LEGACY; /* Guild from SoC driver development guild document => MSA2: 0x20000000... sRGB rather than legacy RGB */
	pstVideoParams->ucVideo_Format		= (u8)VIDEO_FORMAT_CEA_861;
	pstVideoParams->ucPixel_Encoding	= (u8)PIXEL_ENCODING_TYPE_RGB;
	pstVideoParams->uiRefresh_Rate		= (u32)VIDEO_REFRESH_RATE_60_00HZ;

	bRetVal = Dptx_Avgen_Get_Video_NumOfStreams( pstDptx, &pstDptx->ucNumOfStreams );
	if( bRetVal )
	{
		dptx_err("from Dptx_Avgen_Get_Video_NumOfStreams()");
		return ( DPTX_RETURN_FAIL );
	}

	if( pstDptx->ucNumOfStreams == 0 )
	{
		dptx_warn("DP was not enabled");
		return ( DPTX_RETURN_SUCCESS );
	}

	bRetVal = Dptx_Avgen_Get_Pixel_Mode( pstDptx, (u8)PHY_INPUT_STREAM_0, &pstVideoParams->ucMultiPixel  );
	if( bRetVal )
	{
		dptx_err("from Dptx_Avgen_Get_Pixel_Mode()");
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal = Dptx_Avgen_Get_Video_PixelEncoding_Type( pstDptx, (u8)PHY_INPUT_STREAM_0, &pstVideoParams->ucPixel_Encoding );
	if( bRetVal )
	{
		dptx_err("from Dptx_Avgen_Get_Video_PixelEncoding_Type()");
		return ( DPTX_RETURN_FAIL );
	}

	for( ucStream_Index = 0; ucStream_Index < pstDptx->ucNumOfStreams; ucStream_Index++ )
	{
		bRetVal = Dptx_Avgen_Get_VIC_From_Dtd( pstDptx, ucStream_Index,      &pstVideoParams->auiVideo_Code[ucStream_Index] );
		if( bRetVal )
		{
			dptx_err("from Dptx_Avgen_Get_VIC_From_Dtd()");
			continue;
		}
	}

	for( ucStream_Index = 0; ucStream_Index < (u8)PHY_INPUT_STREAM_MAX; ucStream_Index++ )
	{
		pstVideoParams->uiPeri_Pixel_Clock[ucStream_Index] = uiPeri_Pixel_Clock[ucStream_Index];
	}

	dptx_dbg("Pixel mode = %s, Pixel encoding = %s, NumOfStreams = %d, VIC = %d %d %d %d, PClk = %d %d %d %d", 
				( pstVideoParams->ucMultiPixel == 0 ) ? "Single":( pstVideoParams->ucMultiPixel == 1 ) ? "Dual":"Quad",
				( pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_RGB ) ? "RGB":( pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_YCBCR422 ) ? "YCbCr422":"YCbCr444",
				pstDptx->ucNumOfStreams,
				pstVideoParams->auiVideo_Code[0], pstVideoParams->auiVideo_Code[1], pstVideoParams->auiVideo_Code[2], pstVideoParams->auiVideo_Code[3],
				pstVideoParams->uiPeri_Pixel_Clock[0], pstVideoParams->uiPeri_Pixel_Clock[1], pstVideoParams->uiPeri_Pixel_Clock[2], pstVideoParams->uiPeri_Pixel_Clock[3] );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Set_Video_Detailed_Timing( struct Dptx_Params *pstDptx, u8 ucStream_Index, struct Dptx_Dtd_Params *pstDtd_Params )
{
	bool						bRetVal;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;

	if( ucStream_Index >= PHY_INPUT_STREAM_MAX )
	{
		dptx_err( "Invalid stream index(%d) ", ucStream_Index );
		return ( DPTX_RETURN_FAIL );
	}

	memcpy( &pstVideoParams->stDtdParams[ucStream_Index], pstDtd_Params, sizeof( struct Dptx_Dtd_Params ) );

	pstDptx->stVideoParams.uiPeri_Pixel_Clock[ucStream_Index] = pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock;

	dptx_dbg("Stream %d :  ",	ucStream_Index);
	dptx_dbg("Refresh rate: %d", pstVideoParams->uiRefresh_Rate );
	dptx_dbg("DTD format: %s", pstVideoParams->ucVideo_Format == VIDEO_FORMAT_CEA_861 ? "CEA_861":"Others" );
	dptx_dbg("Pixel Clk: %d", pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock);

	bRetVal = Dptx_Avgen_Calculate_Video_Average_TU_Symbols(	pstDptx,
																	pstDptx->stDptxLink.ucNumOfLanes, 
																	pstDptx->stDptxLink.ucLinkRate, 
																	pstVideoParams->ucBitPerComponent, 
																	pstVideoParams->ucPixel_Encoding, 
																	pstVideoParams->stDtdParams[ucStream_Index].uiPixel_Clock,
																	ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	Dptx_Avgen_Set_Video_Stream_Enable( pstDptx, false, ucStream_Index );

	bRetVal = dptx_avgen_config_video_input( pstDptx, (u8)ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Set_Video_Stream_Enable( struct Dptx_Params *pstDptx, bool bEnable_Stream, u8 ucStream_Index )
{
	u32				uiRegMap_VSampleCtrl;

	/*
	 * VSAMPLE_CTRL[0x300 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1]: This register has various control for enabling different setting of the video sampler
	 * -.Reserved[Bit3:0]
	 * -.INTERNAL_DE_GENERATOR[Bit4]( 0: Data enable signal available at the input, 1: Enable internal data generator) 
	 *								  When the external logic which drives the pixel data doesn't generate the video data enable, then enabling this biit forces the controller to generate the video enable internally.	
	 * -.VIDEO_STREAM_ENABLE[Bit5]( 0: Video stream disable, 1: Video stream enable ) 
	 * -.VIDEO_MAPPING[Bit20:16]( 0: RGB 6bits, 1: RGB 8bits, 2: RGB 10bits, 3: RGB 12bits, 4: RGB 16bits ) 
	 *							 ( 5: YCbCr444 8bits, 6: YCbCr444 10bits, 7: YCbCr444 12bits, 8: YCbCr444 16bits ) 	
	 *							 ( 9: YCbCr422 8bits, 10: YCbCr422 10bits, 11: YCbCr422 12bits, 12: YCbCr422 16bits ) 		
	 *							 ( 13: YCbCr420 8bits, 14: YCbCr420 10bits, 15: YCbCr420 12bits, 16: YCbCr420 16bits ) 	
	 *							 ( 17: YOnly 8bits, 18: YOnly 10bits, 19: YOnly 12bits, 20: YOnly 16bits )
	 *							 ( 23: RAW 8bits, 24: RAW 10bits, 25: RAW 12bits, 27: RAW 16bits )
	 * -.PIXEL_MODE_SELECT[Bit22:21]( 0: Single pixel mode, 1:Dual pixel mode, 2: Quad pixel mode, 3: Reserved ) 
	 * -.ENABLE_DSC[Bit b23] : Set this bit to 1 to enable Display Stream Compression( DSC ) on this specific stream
	 */
	uiRegMap_VSampleCtrl = Dptx_Reg_Readl( pstDptx, DPTX_VSAMPLE_CTRL_N( ucStream_Index ) );

	if( bEnable_Stream )
	{
		uiRegMap_VSampleCtrl |= DPTX_VSAMPLE_CTRL_STREAM_EN;
	}
	else
	{
		uiRegMap_VSampleCtrl &= ~DPTX_VSAMPLE_CTRL_STREAM_EN;
	}

	Dptx_Reg_Writel( pstDptx, DPTX_VSAMPLE_CTRL_N( ucStream_Index ), uiRegMap_VSampleCtrl );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Get_Video_Stream_Enable( struct Dptx_Params *pstDptx, bool *pbEnable_Stream, u8 ucStream_Index )
{
	u32				uiRegMap_VSampleCtrl;

	uiRegMap_VSampleCtrl = Dptx_Reg_Readl( pstDptx, DPTX_VSAMPLE_CTRL_N( ucStream_Index ) );

	*pbEnable_Stream = (bool)( uiRegMap_VSampleCtrl & DPTX_VSAMPLE_CTRL_STREAM_EN );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Set_Video_Timing( struct Dptx_Params *pstDptx, u8 ucStream_Index )
{
	bool	bRetVal;

	Dptx_Avgen_Set_Video_Stream_Enable( pstDptx, false, ucStream_Index );

	bRetVal = dptx_avgen_config_video_input( pstDptx, (u8)ucStream_Index );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	Dptx_Avgen_Set_Video_Stream_Enable( pstDptx, true, ucStream_Index );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Get_Video_NumOfStreams( struct Dptx_Params *pstDptx, u8 *pucEnabled_Streams )
{
	bool	bRetVal, bEnabled_Stream;
	u8		ucStreamIndex; 
	u8		ucEnabled_StreamCnt = 0;

	for( ucStreamIndex = 0; ucStreamIndex < PHY_INPUT_STREAM_MAX; ucStreamIndex++ )
	{
		bRetVal = Dptx_Avgen_Get_Video_Stream_Enable( pstDptx, &bEnabled_Stream, ucStreamIndex );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
			
		if( bEnabled_Stream )
		{
			ucEnabled_StreamCnt++;
		}
	}

	*pucEnabled_Streams = ucEnabled_StreamCnt;

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Get_Pixel_Mode( struct Dptx_Params *pstDptx, u8 ucStream_Index, u8 *pucPixelMode )
{
	u32							uiRegMap_VidSampleCtrl;

	uiRegMap_VidSampleCtrl = Dptx_Reg_Readl( pstDptx, DPTX_VSAMPLE_CTRL_N( ucStream_Index ) );

	*pucPixelMode = (u8)( ( uiRegMap_VidSampleCtrl & DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK ) >> DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Get_Video_PixelEncoding_Type( struct Dptx_Params *pstDptx, u8 ucStream_Index, u8 *pucPixel_Encoding )
{
	u8							ucColorimetry_Mapping = 0;
	u32 						uiRegMap_Msa2;

	/* Configure video_msa2 register 
	*
	* VIDEO_MSA3[0x32C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] Used for programming the Main Stream attributes. 
	*	-.MISC0[Bit31:24]( Bit24: Synchronous mode of operation<0: Asynchronous, 1: Synchronous >, Bit25~28: Colorimetry, Bit29~31: MISC0<Bit 1~4> )
	*	-.MVID[Bit23:0]: To be used in case of synchronous mode of operation
	*/
	uiRegMap_Msa2 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_MSA2_N( ucStream_Index ) );

	ucColorimetry_Mapping = ( ( uiRegMap_Msa2 & DPTX_VIDEO_VMSA2_COL_MASK ) >> DPTX_VIDEO_VMSA2_COL_SHIFT );

	if(( ucColorimetry_Mapping == 0 ) || ( ucColorimetry_Mapping == 4 ))
	{
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_RGB;
	}
	else if(( ucColorimetry_Mapping == 5 ) || ( ucColorimetry_Mapping == 13 ))
	{
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_YCBCR422;
	}
	else if(( ucColorimetry_Mapping == 6 ) || ( ucColorimetry_Mapping == 14 ))
	{
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_YCBCR444;
	}
	else
	{
		dptx_warn("Invalid encoding type %d... set default to RGB ", ucColorimetry_Mapping );
		*pucPixel_Encoding = (u8)PIXEL_ENCODING_TYPE_RGB;
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Get_Video_Configured_Timing( struct Dptx_Params *pstDptx, u8 ucStream_Index, struct Dptx_Dtd_Params	*pstDtd )
{
	u32 	uiRegMap_VidConfig1, uiRegMap_VidConfig2, uiRegMap_VidConfig3, uiRegMap_VidConfig4;
	u32		uiRegMap_VidPolarityCtrl; 

	uiRegMap_VidPolarityCtrl = Dptx_Reg_Readl( pstDptx, DPTX_VSAMPLE_POLARITY_CTRL_N( ucStream_Index ) );
	uiRegMap_VidConfig1 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_CONFIG1_N( ucStream_Index ) );
	uiRegMap_VidConfig2 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_CONFIG2_N( ucStream_Index ) );
	uiRegMap_VidConfig3 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_CONFIG3_N( ucStream_Index ) );
	uiRegMap_VidConfig4 = Dptx_Reg_Readl( pstDptx, DPTX_VIDEO_CONFIG4_N( ucStream_Index ) );

	/* Configure DPTX_VSAMPLE_POLARITY_CTRL register 
	*
	* VINPUT_POLARITY_CTRL[0x30C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register controls the polarity of the video input signals
	*	-.VSYNC_IN_POLARITY[Bit0]( 0: Active low, 1:Active high ) 
	*	-.HSYNC_IN_POLARITY[Bit1]( 0: Active low, 1:Active high ) 
	*/
	if( uiRegMap_VidPolarityCtrl &  DPTX_POL_CTRL_H_SYNC_POL_EN )
	{
		pstDtd->h_sync_polarity = 1;
	}
	else
	{
		pstDtd->h_sync_polarity = 0;
	}

	if( uiRegMap_VidPolarityCtrl &  DPTX_POL_CTRL_V_SYNC_POL_EN )
	{
		pstDtd->v_sync_polarity = 1;
	}
	else
	{
		pstDtd->v_sync_polarity = 0;
	}

	if( uiRegMap_VidConfig1 &  DPTX_VIDEO_CONFIG1_O_IP_EN )
	{
		pstDtd->interlaced = 1;
	}
	else
	{
		pstDtd->interlaced = 0;
	}

	pstDtd->h_active = (( uiRegMap_VidConfig1 & DPTX_VIDEO_H_ACTIVE_MASK ) >> DPTX_VIDEO_H_ACTIVE_SHIFT );
	pstDtd->h_blanking = (( uiRegMap_VidConfig1 & DPTX_VIDEO_H_BLANK_MASK ) >> DPTX_VIDEO_H_BLANK_SHIFT );

	pstDtd->v_active = (( uiRegMap_VidConfig2 & DPTX_VIDEO_V_ACTIVE_MASK ) >> DPTX_VIDEO_V_ACTIVE_SHIFT );
	pstDtd->v_blanking = (( uiRegMap_VidConfig2 & DPTX_VIDEO_V_BLANK_MASK ) >> DPTX_VIDEO_V_BLANK_SHIFT );

	pstDtd->h_sync_offset = (( uiRegMap_VidConfig3 & DPTX_VIDEO_H_FRONTE_PORCH_MASK ) >> DPTX_VIDEO_H_FRONT_PORCH_SHIFT );
	pstDtd->h_sync_pulse_width = (( uiRegMap_VidConfig3 & DPTX_VIDEO_H_SYNC_WIDTH_MASK ) >> DPTX_VIDEO_H_SYNC_WIDTH_SHIFT );

	pstDtd->v_sync_offset = (( uiRegMap_VidConfig4 & DPTX_VIDEO_V_FRONTE_PORCH_MASK ) >> DPTX_VIDEO_V_FRONT_PORCH_SHIFT );
	pstDtd->v_sync_pulse_width = (( uiRegMap_VidConfig4 & DPTX_VIDEO_V_SYNC_WIDTH_MASK ) >> DPTX_VIDEO_V_SYNC_WIDTH_SHIFT );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Calculate_Video_Average_TU_Symbols( struct Dptx_Params *pstDptx, int iNumOfLane, int iLinkRate, int iBpc, int iEncodingType, int iPixel_Clock, u8 ucStream_Index )
{
	int							iLink_Rate, iLink_Clock, iColor_Depth, iT1 = 0, iT2 = 0;
	int							iH_AverageSymbol_Per_TU, iAverageSymbol_Per_TU, iTransfer_Unit_Fraction;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Dtd_Params		*pstDtd = &pstVideoParams->stDtdParams[ucStream_Index];

	switch( iLinkRate ) 
	{
		case DPTX_PHYIF_CTRL_RATE_RBR:
			iLink_Rate = 162;
			iLink_Clock = 40500;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR:
			iLink_Rate = 270;
			iLink_Clock = 67500;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			iLink_Rate = 540;
			iLink_Clock = 135000;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			iLink_Rate = 810;
			iLink_Clock = 202500;
			break;
		default:    
			dptx_err("Invalid rate param = %d", iLinkRate );
			return ( DPTX_RETURN_FAIL );
			break;
	}

	switch( (enum VIDEO_PIXEL_COLOR_DEPTH)iBpc ) 
	{
		case PIXEL_COLOR_DEPTH_8:
			if(( iEncodingType == PIXEL_ENCODING_TYPE_RGB ) || ( iEncodingType == PIXEL_ENCODING_TYPE_YCBCR444 ))
			{
				iColor_Depth  = ( PIXEL_COLOR_DEPTH_8 * VIDEO_LINK_BPP_RGB_YCbCr444 );
			}
			else if( iEncodingType == PIXEL_ENCODING_TYPE_YCBCR422 )
			{
				iColor_Depth = ( PIXEL_COLOR_DEPTH_8 * VIDEO_LINK_BPP_YCbCr422 );
			}
			else
			{
				dptx_err("Invalid encoding type(%d)", iEncodingType);
				return ( DPTX_RETURN_FAIL );
			}
			break;
		default:
			dptx_err("Invalid Bpc(%d)", iBpc);
			return ( DPTX_RETURN_FAIL );
			break;
	}

	/*  "( 8 * iColor_Depth * iPixel_Clock ) / iNumOfLane" ==> Packeted data rate over 4 lanes 
	 *   According to the Section 2.2.1.4.1 in DP V1.4 Spec, Average valid symbles per TU = ( Packeted data rate over 4 lanes ) / ( Link rate )
	 *	 The TU size is fixed to 64 link symbols per lane.
	 *	  -. Packeted data rate over 4 lanes = (( Color bpp * pixel clock / lanes ) / 8( Color pixles -> 8 pixels make one packeted data ) )	
	 *    -. Example with VIC 4( 1920 x 720 ) => Pixel clock( 74250 ), 
	 *	   (( 24bpp * 74250 / 4 lanes ) / 8 pixels ) = 55687 => Average valid symbles per TU = ( 55687 * 64 ) / 270 = 13,200 -> 13,200 / 1000 = 13
	 *	   Frac = ( 13,200 / 100 ) - ( 13 * 10 ) = 132 - 130 = 2     
	 *    -. Example with VIC 16( 1920 x 1080 ) => Pixel clock( 148500 ), 
	 *	   (( 24bpp * 148500 / 4 lanes ) / 8 pixels ) = 111375 => Average valid symbles per TU = ( 111375 * 64 ) / 270 = 26,400 -> 26,400 / 1000 = 26
	 *	   Frac = ( 26,400 / 100 ) - ( 26 * 10 ) = 264 - 260 = 4     
	*/
	iH_AverageSymbol_Per_TU = (( 8 * iColor_Depth * iPixel_Clock ) / ( iNumOfLane * iLink_Rate )); 	// 17,454,528 / 3240
	iAverageSymbol_Per_TU  = ( iH_AverageSymbol_Per_TU / 1000 );
	if( iAverageSymbol_Per_TU > DPTX_MAX_LINK_SYMBOLS ) 
	{
		dptx_err("iTransfer_Unit(%d) > DPTX_MAX_LINK_SYMBOLS", iAverageSymbol_Per_TU);
		return ( DPTX_RETURN_FAIL );
	}

	iTransfer_Unit_Fraction = ( ( iH_AverageSymbol_Per_TU / 100 ) - ( iAverageSymbol_Per_TU * 10 ) ); // ( 24340 / 100 ) - ( 24 * 10 ) = 243 - 240 = '3'

	pstVideoParams->ucAverage_BytesPerTu	= iAverageSymbol_Per_TU;
	pstVideoParams->ucAver_BytesPer_Tu_Frac = iTransfer_Unit_Fraction;

	if( pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_SINGLE )
	{
		if( iAverageSymbol_Per_TU < 6 )
		{
			pstVideoParams->ucInit_Threshold = 32;
		}
		else if( pstDtd->h_blanking <= 80 )
		{
			pstVideoParams->ucInit_Threshold = 12;
		}
		else
		{
			pstVideoParams->ucInit_Threshold = 16;
		}
	} 
	else //Multiple Pixel Mode
	{
		switch( (enum VIDEO_PIXEL_COLOR_DEPTH)iBpc )
		{
			case PIXEL_COLOR_DEPTH_8:
				if(( iEncodingType == PIXEL_ENCODING_TYPE_RGB ) || ( iEncodingType == PIXEL_ENCODING_TYPE_YCBCR444 ))
				{
					if( pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_DUAL )
					{
						iT1 = ( ( 1000 / 3 ) *iNumOfLane );
					}
					else
					{
						iT1 = ( ( 3000 / 16 ) * iNumOfLane );
					}
				}
				else if( iEncodingType == PIXEL_ENCODING_TYPE_YCBCR422 )
				{
					iT1 = ( ( 1000 / 2 ) *iNumOfLane );
				}
				else
				{
					dptx_err("Invalid encoding type(%d)", iEncodingType);
					return ( DPTX_RETURN_FAIL );
				}
				break;
			default:
				dptx_err( "Invalid param iBpc = %d", iBpc );
				return ( DPTX_RETURN_FAIL );
				break;
		}

		iT2 = (( iLink_Clock * 1000 ) / iPixel_Clock);

		pstVideoParams->ucInit_Threshold = ( iT1 * iT2 * iAverageSymbol_Per_TU / ( 1000 * 1000 ));
	}

	iH_AverageSymbol_Per_TU = (( 8 * iColor_Depth * iPixel_Clock ) / ( iNumOfLane * iLink_Rate )); 

	if( !pstDptx->bMultStreamTransport ) 
	{
		dptx_info("SST %s Pixel => ", pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_SINGLE ? "Single":pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_DUAL ? "Dual":"Quad" );
		dptx_info(" -.Pixel clock: %d", iPixel_Clock );
		dptx_info(" -.Num of Lanes: %d ", iNumOfLane );
		dptx_info(" -.Link rate: %s", iLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR ) ?	"HBR":( iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3" );
		dptx_info(" -.Hblanking: %d", pstDtd->h_blanking );
		dptx_info(" -.Link Bpc: %d", iLink_Rate );
		dptx_info(" -.Link Clk: %d", iLink_Clock );
		dptx_info(" -.Average Symbols per TU: %d <- (( 8 * %d * %d ) / ( %d * %d ))", pstVideoParams->ucAverage_BytesPerTu, iColor_Depth, iPixel_Clock, iNumOfLane, iLink_Rate );
		dptx_info(" -.Fraction per TU: %d", pstVideoParams->ucAver_BytesPer_Tu_Frac );
		dptx_info(" -.Init thresh: %d",	pstVideoParams->ucInit_Threshold );
	}

	if( pstDptx->bMultStreamTransport ) 
	{
		u32				uiAverage_BytesPerTu, uiAver_BytesPer_Tu_Frac;
		int 			iNumerator, iDenominator;

		/* Average_bytes_per_tu = floor(( Peak_stream_bandwidth / Link bandwidth * 64 ))
		    -. VIC - 1( 640 x 480 ) RGB HBR => Peak_stream_bandwidth = Pixel_clock_frequency * bytes_per_pixel = 25.175 * 3 = 75.52MBps
		                                    => Link bandwidth = link_rate * 100 * number_of_lanes = ( 2.7 * 100 = 270Msymbols / Sec / Lane ) * 4 = 1080MBps
		                                    => Average_bytes_per_tu = floor(( 75.52 / 1080 ) * 64 ) = floor( 4.475 ) = 4
		                                    => Average_bytes_per_tu_frac = 0.4 * 64 = 30.08 ==> 30
			-. VIC - 4( 1920 x 720 ) RGB HBR => Peak_stream_bandwidth = 74.25 * 3 = 222.75MBps
											 => Link bandwidth = ( 2.7 * 100 = 270Msymbols / Sec / Lane ) * 4 = 1080MBps      
											 => Average_bytes_per_tu = floor(( 222.75 / 1080 ) * 64 ) = floor( 13.2 ) = 13	
											 => Average_bytes_per_tu_frac = 0.2 * 64 = 12.8 ==> 12
			-. VIC - 16( 1920 x 1080 ) RGB HBR => Peak_stream_bandwidth = 148.5 * 3 = 445.5MBps
											 => Link bandwidth = ( 2.7 * 100 = 270Msymbols / Sec / Lane ) * 4 = 1080MBps      
											 => Average_bytes_per_tu = floor(( 445.5 / 1080 ) * 64 ) = floor( 26.4 ) = 26	
											 => Average_bytes_per_tu_frac = 0.4 * 64 = 25.6 ==> 25								 
		*/

		if( iEncodingType == PIXEL_ENCODING_TYPE_YCBCR422 )
		{
			iNumerator	 = (  iPixel_Clock * 2 * 64 );
		}
		else
		{
			iNumerator	 = (  iPixel_Clock * 3 * 64 );
		}
		
		iDenominator = ( iLink_Rate * iNumOfLane );

		uiAverage_BytesPerTu = (( iNumerator / iDenominator ) / 1000 ); 
		uiAver_BytesPer_Tu_Frac = (((( iNumerator / iDenominator ) - ( uiAverage_BytesPerTu * 1000 )) * 64 ) / 1000 ); 

		pstVideoParams->ucAverage_BytesPerTu	= (u8)uiAverage_BytesPerTu;
		pstVideoParams->ucAver_BytesPer_Tu_Frac = (u8)uiAver_BytesPer_Tu_Frac;

		dptx_info("MST %s Pixel : ", pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_SINGLE ? "Single":pstVideoParams->ucMultiPixel == MULTI_PIXEL_TYPE_DUAL ? "Dual":"Quad" );
		dptx_info(" -.Pixel clock: %d", iPixel_Clock );
		dptx_info(" -.Num of Lanes: %d", iNumOfLane );
		dptx_info(" -.Link rate: %s", iLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR ) ?	"HBR":( iLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3" );
		dptx_info(" -.VCP Id: %d", pstDptx->aucVCP_Id[ucStream_Index] );
		dptx_info(" -.Pixel clock: %d", iPixel_Clock );
		dptx_info(" -.Hblanking: %d", pstDtd->h_blanking );
		dptx_info(" -.Link Bpc: %d", iLink_Rate );
		dptx_info(" -.Link Clk: %d", iLink_Clock );
		dptx_info(" -.Average Symbols Per TU: %d ", pstVideoParams->ucAverage_BytesPerTu );
		dptx_info(" -.Fraction Per TU: %d", pstVideoParams->ucAver_BytesPer_Tu_Frac );
		dptx_info(" -.Init thresh: %d",	pstVideoParams->ucInit_Threshold );
	}

	return ( DPTX_RETURN_SUCCESS );
}

void Dptx_Avgen_Configure_Audio(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, struct Dptx_Audio_Params *pstAudioParams)
{
	struct Dptx_Audio_Params	*pstDptxAudioParams = &pstDptx->stAudioParams;
	
	memcpy(pstDptxAudioParams, pstAudioParams, sizeof(struct Dptx_Audio_Params));

	Dptx_Avgen_Set_Audio_Mute(pstDptx, ucStream_Index, (uint8_t)AUDIO_INPUT_SET_MUTE_FLAG_VBID);

	Dptx_Avgen_Set_Audio_Input_InterfaceType(pstDptx, ucStream_Index, pstAudioParams->ucInput_InterfaceType);
	Dptx_Avgen_Set_Audio_HBR_En(pstDptx, ucStream_Index, pstAudioParams->ucInput_HBR_Mode);
	Dptx_Avgen_Set_Audio_MaxNumOfChannels(pstDptx, ucStream_Index, pstAudioParams->ucInput_Max_NumOfchannels);
	Dptx_Avgen_Set_Audio_DataWidth(pstDptx, ucStream_Index, pstAudioParams->ucInput_DataWidth);
	Dptx_Avgen_Set_Audio_Timestamp_Ver(pstDptx, ucStream_Index, pstAudioParams->ucInput_TimestampVersion);
	Dptx_Avgen_Set_Audio_SDP_InforFrame(pstDptx, 
												pstAudioParams->ucIEC_Sampling_Freq, 
												pstAudioParams->ucIEC_OriginSamplingFreq, 
												pstAudioParams->ucInput_Max_NumOfchannels,
												pstAudioParams->ucInput_DataWidth);
	Dptx_Avgen_Enable_Audio_SDP(pstDptx, ucStream_Index, 1);
	Dptx_Avgen_Enable_Audio_Timestamp(pstDptx, ucStream_Index, 1);
	
	Dptx_Avgen_Set_Audio_Mute(pstDptx, ucStream_Index, (uint8_t)AUDIO_INPUT_CLEAR_MUTE_FLAG_VBID);

	dptx_info( "\n DP %d : ", ucStream_Index );
	dptx_info( "  -.Interface type : %d", (u32)pstAudioParams->ucInput_InterfaceType );
	dptx_info( "  -.Max num of input channels : %d", (u32)pstAudioParams->ucInput_Max_NumOfchannels );
	dptx_info( "  -.Input data width : %d", (u32)pstAudioParams->ucInput_DataWidth );
	dptx_info( "  -.HBR Mode : %d", (u32)pstAudioParams->ucInput_HBR_Mode );
	dptx_info( "  -.Input version of time stamp : %d", (u32)pstAudioParams->ucInput_TimestampVersion );
	dptx_info( "  -.Sampling Freq : %d", (u32)pstAudioParams->ucIEC_Sampling_Freq );
	dptx_info( "  -.Orginal Sampling Freq : %d", (u32)pstAudioParams->ucIEC_OriginSamplingFreq );
}

/* DPTX_AUD_CONFIG1_NCH_SHIFT ==> 0: 1 channel, 1: 2 channels, others: 8 channels */
void Dptx_Avgen_Set_Audio_MaxNumOfChannels(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucInput_Max_NumOfCh)
{
	uint32_t uiRegMap_NumOfChannels = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_NumOfChannels = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_NumOfChannels &= ~DPTX_AUD_CONFIG1_NCH_MASK;
	uiRegMap_NumOfChannels |= (ucInput_Max_NumOfCh << DPTX_AUD_CONFIG1_NCH_SHIFT);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_NumOfChannels);

	dptx_avgen_active_audio_channels( pstDptx, ucStream_Index, ucInput_Max_NumOfCh, 1 );
	}

void Dptx_Avgen_Get_Audio_MaxNumOfChannels(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucInput_Max_NumOfCh)
{
	uint32_t uiRegMap_NumOfChannels = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_NumOfChannels = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	
	*pucInput_Max_NumOfCh = (uint8_t)((uiRegMap_NumOfChannels & DPTX_AUD_CONFIG1_NCH_MASK) >> DPTX_AUD_CONFIG1_NCH_SHIFT);
}

void Dptx_Avgen_Set_Audio_DataWidth(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucInput_DataWidth)
{
	u32 uiRegMap_AudioSamplingWidth = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudioSamplingWidth = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudioSamplingWidth &= ~DPTX_AUD_CONFIG1_DATA_WIDTH_MASK;
	uiRegMap_AudioSamplingWidth |= (ucInput_DataWidth << DPTX_AUD_CONFIG1_DATA_WIDTH_SHIFT);
	
	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudioSamplingWidth);
	}

void Dptx_Avgen_Get_Audio_DataWidth(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucInput_DataWidth)
{
	u32 uiRegMap_AudioSamplingWidth = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudioSamplingWidth = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucInput_DataWidth = (uint32_t)((uiRegMap_AudioSamplingWidth & DPTX_AUD_CONFIG1_DATA_WIDTH_MASK) >> DPTX_AUD_CONFIG1_DATA_WIDTH_SHIFT);
}

void Dptx_Avgen_Get_Audio_SamplingFreq(struct Dptx_Params *pstDptx, uint8_t	*pucSamplingFreq, uint8_t	*pucOrgSamplingFreq )
{
	struct Dptx_Audio_Params 	*pstAudioParams = &pstDptx->stAudioParams;

	*pucSamplingFreq = pstAudioParams->ucIEC_Sampling_Freq;
	*pucOrgSamplingFreq = pstAudioParams->ucIEC_OriginSamplingFreq;
}

/* DPTX_AUD_CONFIG1_INF_TYPE_MASK ==> 0 : I2S is selected for audio input interface( Default ), 1 : SPDIF is selected */
void Dptx_Avgen_Set_Audio_Input_InterfaceType(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t ucAudInput_InterfaceType)
{
	u32 uiRegMap_AudINFType = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudINFType = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudINFType &= ~DPTX_AUD_CONFIG1_INF_TYPE_MASK;
	uiRegMap_AudINFType |= (ucAudInput_InterfaceType << DPTX_AUD_CONFIG1_INF_TYPE_SHIFT);
	
	Dptx_Reg_Writel( pstDptx, uiReg_Offset, uiRegMap_AudINFType );
}

void Dptx_Avgen_Get_Audio_Input_InterfaceType(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t *pucAudInput_InterfaceType)
{
	u32 uiRegMap_AudINFType = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N( ucStream_Index );

	uiRegMap_AudINFType = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucAudInput_InterfaceType = (uiRegMap_AudINFType & DPTX_AUD_CONFIG1_INF_TYPE_MASK) ? 1 : 0;
	}

void Dptx_Avgen_Set_Audio_HBR_En(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t ucEnable)
	{
	uint32_t uiRegMap_AudHBR = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudHBR = Dptx_Reg_Readl(pstDptx, uiReg_Offset);
	uiRegMap_AudHBR &= ~DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_MASK;
	uiRegMap_AudHBR |= (ucEnable << DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_SHIFT);
	
	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudHBR);
	}

void Dptx_Avgen_Get_Audio_HBR_En(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t *pucEnable)
{
	uint32_t uiRegMap_AudHBR = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);
	
	uiRegMap_AudHBR = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucEnable = (uiRegMap_AudHBR & DPTX_AUD_CONFIG1_HBR_MODE_ENABLE_MASK) ? 1 : 0;
}

void Dptx_Avgen_Set_Audio_Timestamp_Ver(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucAudInput_TimestampVersion)
{
	u32 uiRegMap_AudConf1 = 0,    uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N(ucStream_Index);

	uiRegMap_AudConf1 = Dptx_Reg_Readl( pstDptx, uiReg_Offset );
	uiRegMap_AudConf1 &= ~DPTX_AUD_CONFIG1_ATS_VER_MASK;
	uiRegMap_AudConf1 |= ( ucAudInput_TimestampVersion << DPTX_AUD_CONFIG1_ATS_VER_SHFIT );

	Dptx_Reg_Writel( pstDptx, uiReg_Offset, uiRegMap_AudConf1 );
}


void Dptx_Avgen_Set_Audio_SDP_InforFrame(struct Dptx_Params *pstDptx,
																uint8_t ucIEC_Sampling_Freq,
																uint8_t ucIEC_OriginSamplingFreq,
																uint8_t ucInput_Max_NumOfchannels,
																uint8_t ucInput_DataWidth)
	{
	uint32_t uiAudio_SdpInfoframe_header = AUDIO_INFOFREAME_HEADER;
	uint32_t auiAudio_SdpInfoframe_data[3] = {0x00000710, 0x0, 0x0};
#if 1
/*
	Hardcoding based on SoC guilde.
*/
	auiAudio_SdpInfoframe_data[0] = 0x00000901;
	auiAudio_SdpInfoframe_data[1] = 0x00000001;

	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK, uiAudio_SdpInfoframe_header );
	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK + 4, auiAudio_SdpInfoframe_data[0] );
	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK + 8, auiAudio_SdpInfoframe_data[1] );
	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK + 12, auiAudio_SdpInfoframe_data[2] );
#else /* Synopsys reference */
	if( ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_176_4 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_176_4 ) {
		auiAudio_SdpInfoframe_data[0] = 0x00000710;
	}
	else if( ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_44_1 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_44_1 ) {
		auiAudio_SdpInfoframe_data[0] = 0x00000B10;
	}
	else if( ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_22_05 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_22_05 ) {
		auiAudio_SdpInfoframe_data[0] = 0x00000F10;
	}
	else if( ucIEC_OriginSamplingFreq == 7 && ucIEC_Sampling_Freq == 8 ) {
		auiAudio_SdpInfoframe_data[0] = 0x00001310;
	}
	else if( ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_11_025 && ucIEC_Sampling_Freq == 10 ) {
		auiAudio_SdpInfoframe_data[0] = 0x00001710;
	}
	else if( ucIEC_OriginSamplingFreq == (u8)IEC60958_3_ORIGINAL_SAMPLE_FREQ_32 && ucIEC_Sampling_Freq == (u8)IEC60958_3_SAMPLE_FREQ_32 ) {
		auiAudio_SdpInfoframe_data[0] = 0x00001B10;
	}
	else {
		auiAudio_SdpInfoframe_data[0] = 0x00001F10;
	}

	auiAudio_SdpInfoframe_data[0] |= ( ucInput_Max_NumOfchannels - 1 );
	
	if( ucInput_Max_NumOfchannels == 3 ) {
		auiAudio_SdpInfoframe_data[0] |= 0x02000000;
	}
	else if( ucInput_Max_NumOfchannels == 4 ) {
		auiAudio_SdpInfoframe_data[0] |= 0x03000000;
	}
	else if( ucInput_Max_NumOfchannels == 5 ) {
		auiAudio_SdpInfoframe_data[0] |= 0x07000000;
	}
	else if( ucInput_Max_NumOfchannels == 6 ) {
		auiAudio_SdpInfoframe_data[0] |= 0x0b000000;
	}
	else if( ucInput_Max_NumOfchannels == 7 ) {
		auiAudio_SdpInfoframe_data[0] |= 0x0f000000;
	}
	else if( ucInput_Max_NumOfchannels == 8 ) {
		auiAudio_SdpInfoframe_data[0] |= 0x13000000;
	}

	switch( ucInput_DataWidth )
	{
		case MAX_INPUT_DATA_WIDTH_16BIT:
			dptx_dbg("ucInput_DataWidth = 16" );
			auiAudio_SdpInfoframe_data[0] &= ~( GENMASK( 9, 8 ) );
			auiAudio_SdpInfoframe_data[0] |= 1 << 8;
			break;
		case MAX_INPUT_DATA_WIDTH_20BIT:
			dptx_dbg("ucInput_DataWidth = 20" );
			auiAudio_SdpInfoframe_data[0] &= ~( GENMASK( 9, 8 ) );
			auiAudio_SdpInfoframe_data[0] |= 2 << 8;
			break;
		case MAX_INPUT_DATA_WIDTH_24BIT:
			dptx_dbg("ucInput_DataWidth = 24" );
			auiAudio_SdpInfoframe_data[0] &= ~( GENMASK( 9, 8 ) );
			auiAudio_SdpInfoframe_data[0] |= 3 << 8;
			break;
		default:
			dptx_err("ucInput_DataWidth not found" );
			break;
	}

	dptx_dbg( "auiAudio_SdpInfoframe_data[0] after = %x", auiAudio_SdpInfoframe_data[0] );

	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK, uiAudio_SdpInfoframe_header );
	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK + 4, auiAudio_SdpInfoframe_data[0] );
	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK + 8, auiAudio_SdpInfoframe_data[1] );
	Dptx_Reg_Writel( pstDptx, DPTX_SDP_BANK + 12, auiAudio_SdpInfoframe_data[2] );
#endif
}

void Dptx_Avgen_Set_Audio_Mute(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucMute)
{
	u32 uiRegMap_AudioMute = 0, uiReg_Offset = 0;
	
	uiReg_Offset = DPTX_AUD_CONFIG1_N( ucStream_Index );

	uiRegMap_AudioMute = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	if(ucMute) {
		uiRegMap_AudioMute |= DPTX_AUDIO_MUTE;
	} else {
		uiRegMap_AudioMute &= ~DPTX_AUDIO_MUTE;
	}

	Dptx_Reg_Writel( pstDptx, uiReg_Offset, uiRegMap_AudioMute );
}

void Dptx_Avgen_Get_Audio_Mute(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucMute)
{
	u32 uiRegMap_AudioMute = 0, uiReg_Offset = 0;

	uiReg_Offset = DPTX_AUD_CONFIG1_N( ucStream_Index );

	uiRegMap_AudioMute = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	*pucMute = (uiRegMap_AudioMute & DPTX_AUDIO_MUTE) ? 1:0;
}


void Dptx_Avgen_Set_Audio_Stream_Enable( struct Dptx_Params *pstDptx, u8 ucSatrem_Index, bool bEnable )
{
	u32							uiRegMap_AudioMute = 0;

	uiRegMap_AudioMute = Dptx_Reg_Readl(pstDptx, DPTX_AUD_CONFIG1_N( ucSatrem_Index ));

	if( bEnable )
	{
		uiRegMap_AudioMute &= ~DPTX_AUDIO_MUTE;
	}
	else
	{
		uiRegMap_AudioMute |= DPTX_AUDIO_MUTE;
	}

	Dptx_Reg_Writel( pstDptx, DPTX_AUD_CONFIG1_N( ucSatrem_Index ), uiRegMap_AudioMute );
}

void Dptx_Avgen_Enable_Audio_SDP(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable)
{
	u32	uiRegMap_AudEnableSDP, uiReg_Offset = 0;

	uiReg_Offset = DPTX_SDP_VERTICAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableSDP = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	if(ucEnable)
	uiRegMap_AudEnableSDP |= DPTX_EN_AUDIO_STREAM_SDP;
	else
		uiRegMap_AudEnableSDP &= ~DPTX_EN_AUDIO_STREAM_SDP;
	
	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableSDP);


	uiReg_Offset = DPTX_SDP_HORIZONTAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableSDP = Dptx_Reg_Readl(pstDptx, uiReg_Offset);

	if(ucEnable)
		uiRegMap_AudEnableSDP |= DPTX_EN_AUDIO_STREAM_SDP;
	else
	uiRegMap_AudEnableSDP &= ~DPTX_EN_AUDIO_STREAM_SDP;

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableSDP);
}

void Dptx_Avgen_Enable_Audio_Timestamp(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable)
{
	u32	uiRegMap_AudEnableTimestamp,     uiReg_Offset = 0;

	uiReg_Offset = DPTX_SDP_VERTICAL_CTRL_N(ucStream_Index);

	uiRegMap_AudEnableTimestamp = Dptx_Reg_Readl(pstDptx, DPTX_SDP_VERTICAL_CTRL);

	if(ucEnable)
	uiRegMap_AudEnableTimestamp |= DPTX_EN_AUDIO_TIMESTAMP_SDP;
	else
		uiRegMap_AudEnableTimestamp &= ~DPTX_EN_AUDIO_TIMESTAMP_SDP;
	
	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableTimestamp);


	uiReg_Offset = DPTX_SDP_HORIZONTAL_CTRL_N(ucStream_Index);
	
	uiRegMap_AudEnableTimestamp = Dptx_Reg_Readl(pstDptx, DPTX_SDP_HORIZONTAL_CTRL);

	if(ucEnable)
		uiRegMap_AudEnableTimestamp |= DPTX_EN_AUDIO_TIMESTAMP_SDP;
	else 
	uiRegMap_AudEnableTimestamp &= ~DPTX_EN_AUDIO_TIMESTAMP_SDP;
	
	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiRegMap_AudEnableTimestamp);
}

bool Dptx_Avgen_Get_VIC_From_Dtd( struct Dptx_Params *pstDptx, u8 ucStream_Index, u32 *puiVideo_Code )
{
	bool	bRetVal;
	u32		uiVideoCode;
	struct Dptx_Dtd_Params	stDtd_Params;
	struct Dptx_Dtd_Params	stDtd_Timing;

	bRetVal = Dptx_Avgen_Get_Video_Configured_Timing( pstDptx, ucStream_Index, &stDtd_Timing );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("from  Dptx_Avgen_Get_Video_Configured_Timing()" );
		return ( DPTX_RETURN_FAIL );
	}

/*
	dptx_info("\n[Dptx_Avgen_Get_VIC_From_Dtd] : ");
	dptx_info("		Pixel clk = %d ", (u32)stDtd_Timing.uiPixel_Clock );
	dptx_info("		%s", ( stDtd_Timing.interlaced ) ? "Interlace" : "Progressive" );
	dptx_info("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)stDtd_Timing.h_sync_polarity, (u32)stDtd_Timing.v_sync_polarity );
	dptx_info("		H Active(%d), V Active(%d)", (u32)stDtd_Timing.h_active, (u32)stDtd_Timing.v_active );
	dptx_info("		H Blanking(%d), V Blanking(%d)", (u32)stDtd_Timing.h_blanking, (u32)stDtd_Timing.v_blanking);
	dptx_info("		H Sync offset(%d), V Sync offset(%d) ", (u32)stDtd_Timing.h_sync_offset, (u32)stDtd_Timing.v_sync_offset);
	dptx_info("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)stDtd_Timing.h_sync_pulse_width, (u32)stDtd_Timing.v_sync_pulse_width );
	*/

	for( uiVideoCode = 0; uiVideoCode < (u32)DP_CUSTOM_MAX_DTD_VIC;  uiVideoCode++ )
	{
		bRetVal = Dptx_Avgen_Fill_Dtd( &stDtd_Params, uiVideoCode, 60000, (u8)VIDEO_FORMAT_CEA_861 );
		if( bRetVal == DPTX_RETURN_SUCCESS )
		{
			if(( stDtd_Params.interlaced		== stDtd_Timing.interlaced ) && 
				( stDtd_Params.h_sync_polarity	== stDtd_Timing.h_sync_polarity ) && 
				( stDtd_Params.h_active			== stDtd_Timing.h_active ) && 
				( stDtd_Params.h_blanking		== stDtd_Timing.h_blanking ) && 
				( stDtd_Params.h_sync_offset	== stDtd_Timing.h_sync_offset ) && 
				( stDtd_Params.h_sync_pulse_width	== stDtd_Timing.h_sync_pulse_width ) && 
				( stDtd_Params.v_sync_polarity		== stDtd_Timing.v_sync_polarity ) && 
				( stDtd_Params.v_active			== stDtd_Timing.v_active ) && 
				( stDtd_Params.v_blanking		== stDtd_Timing.v_blanking ) && 
				( stDtd_Params.v_sync_offset	== stDtd_Timing.v_sync_offset ) && 
				( stDtd_Params.v_sync_pulse_width	== stDtd_Timing.v_sync_pulse_width ))
			{
				*puiVideo_Code = uiVideoCode;
				break;
			}
		}
	}

	if( uiVideoCode == DP_CUSTOM_MAX_DTD_VIC )
	{
		dptx_err("Not found VIC from dtd " );
		return ( DPTX_RETURN_FAIL );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Avgen_Fill_Dtd(             struct Dptx_Dtd_Params *pstDtd, u32 uiVideo_Code, u32 uiRefreshRate, u8 ucVideoFormat )
{
	dptx_avgen_reset_dtd( pstDtd );

	pstDtd->h_image_size = 16;
	pstDtd->v_image_size = 9;

	if( ucVideoFormat == VIDEO_FORMAT_CEA_861 ) 
	{
		switch( uiVideo_Code ) 
		{
			case DP_CUSTOM_1025_DTD_VIC:
				pstDtd->h_active = 1024;
				pstDtd->v_active = 600;
				pstDtd->h_blanking = 320;
				pstDtd->v_blanking = 35;
				pstDtd->h_sync_offset = 150;
				pstDtd->v_sync_offset = 15;
				pstDtd->h_sync_pulse_width = 20;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 51200;
				break;	
			case DP_CUSTOM_1026_DTD_VIC:
				pstDtd->h_active = 5760;
				pstDtd->v_active = 900;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 26;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;	
			case DP_CUSTOM_1027_DTD_VIC:
				pstDtd->h_active = 1920;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 64;
				pstDtd->v_blanking = 21;
				pstDtd->h_sync_offset = 30;
				pstDtd->v_sync_offset = 10;
				pstDtd->h_sync_pulse_width = 4;
				pstDtd->v_sync_pulse_width = 2;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 88200;
				break;	
			case 1: /* 640x480p @ 59.94/60Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 640;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 10;
				pstDtd->h_sync_pulse_width = 96;
				pstDtd->v_sync_pulse_width = 2;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 25175;
				break;
			case 2: /* 720x480p @ 59.94/60Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 720;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 138;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 62;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 3: /* 720x480p @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 720;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 138;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 62;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 69:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 370;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 110;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 4: /* 1280x720p @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 370;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 110;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 5: /* 1920x1080i @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 540;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 6: /* 720(1440)x480i @ 59.94/60Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 7: /* 720(1440)x480i @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 8: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = (uiRefreshRate == 59940) ? 22 : 23;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = (uiRefreshRate == 59940) ? 4 : 5;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 9: /* 720(1440)x240p @59.826/60.054/59.886/60.115Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = (uiRefreshRate == 59940) ? 22 : 23;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = (uiRefreshRate == 59940) ? 4 : 5;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 10: /* 2880x480i @ 59.94/60Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2880;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 552;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 76;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 248;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 11: /* 2880x480i @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 2880;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 552;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 76;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 248;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 12: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2880;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 552;
				pstDtd->v_blanking = (uiRefreshRate == 60054) ? 22 : 23;
				pstDtd->h_sync_offset = 76;
				pstDtd->v_sync_offset = (uiRefreshRate == 60054) ? 4 : 5;
				pstDtd->h_sync_pulse_width = 248;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 13: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
				pstDtd->h_active = 2880;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 552;
				pstDtd->v_blanking = (uiRefreshRate == 60054) ? 22 : 23;
				pstDtd->h_sync_offset = 76;
				pstDtd->v_sync_offset = (uiRefreshRate == 60054) ? 4 : 5;
				pstDtd->h_sync_pulse_width = 248;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 14: /* 1440x480p @ 59.94/60Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 32;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 15: /* 1440x480p @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 32;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 76:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 16: /* 1920x1080p @ 59.94/60Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 17: /* 720x576p @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 720;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 12;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 64;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 18: /* 720x576p @ 50Hz 16:9 */
				pstDtd->h_active = 720;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 12;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 64;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 68:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 700;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 440;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 19: /* 1280x720p @ 50Hz 16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 700;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 440;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 20: /* 1920x1080i @ 50Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 540;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 21: /* 720(1440)x576i @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 22: /* 720(1440)x576i @ 50Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 23: /* 720(1440)x288p @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = (uiRefreshRate == 50080) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = (uiRefreshRate == 50080) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 24: /* 720(1440)x288p @ 50Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = (uiRefreshRate == 50080) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = (uiRefreshRate == 50080) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 27000;
				break;
			case 25: /* 2880x576i @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2880;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 576;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 252;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 26: /* 2880x576i @ 50Hz 16:9 */
				pstDtd->h_active = 2880;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 576;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 252;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 27: /* 2880x288p @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2880;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 576;
				pstDtd->v_blanking = ( uiRefreshRate == 50080 ) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = ( uiRefreshRate == 50080 ) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
				pstDtd->h_sync_pulse_width = 252;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 28: /* 2880x288p @ 50Hz 16:9 */
				pstDtd->h_active = 2880;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 576;
				pstDtd->v_blanking = ( uiRefreshRate == 50080 ) ? 24 : ((uiRefreshRate == 49920) ? 25 : 26);
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = ( uiRefreshRate == 50080 ) ? 2 : ((uiRefreshRate == 49920) ? 3 : 4);
				pstDtd->h_sync_pulse_width = 252;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 29: /* 1440x576p @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 30: /* 1440x576p @ 50Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 75:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 31: /* 1920x1080p @ 50Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 72:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 830;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 638;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 32: /* 1920x1080p @ 23.976/24Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 830;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 638;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 73:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 33: /* 1920x1080p @ 25Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 74:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 34: /* 1920x1080p @ 29.97/30Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 35: /* 2880x480p @ 60Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2880;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 552;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 248;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 36: /* 2880x480p @ 60Hz 16:9 */
				pstDtd->h_active = 2880;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 552;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 248;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 37: /* 2880x576p @ 50Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2880;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 576;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 256;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 38: /* 2880x576p @ 50Hz 16:9 */
				pstDtd->h_active = 2880;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 576;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 256;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 39: /* 1920x1080i (1250 total) @ 50Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 540;
				pstDtd->h_blanking = 384;
				pstDtd->v_blanking = 85;
				pstDtd->h_sync_offset = 32;
				pstDtd->v_sync_offset = 23;
				pstDtd->h_sync_pulse_width = 168;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 72000;
				break;
			case 40: /* 1920x1080i @ 100Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 540;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 70:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 700;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 440;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 41: /* 1280x720p @ 100Hz 16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 700;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 440;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 42: /* 720x576p @ 100Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 720;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 12;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 64;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 43: /* 720x576p @ 100Hz 16:9 */
				pstDtd->h_active = 720;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 12;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 64;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 44: /* 720(1440)x576i @ 100Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 45: /* 720(1440)x576i @ 100Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 46: /* 1920x1080i @ 119.88/120Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 540;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 71:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 370;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 110;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 47: /* 1280x720p @ 119.88/120Hz 16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 370;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 110;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 48: /* 720x480p @ 119.88/120Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 720;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 138;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 62;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 49: /* 720x480p @ 119.88/120Hz 16:9 */
				pstDtd->h_active = 720;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 138;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 62;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 50: /* 720(1440)x480i @ 119.88/120Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 51: /* 720(1440)x480i @ 119.88/120Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 54000;
				break;
			case 52: /* 720X576p @ 200Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 720;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 12;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 64;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 53: /* 720X576p @ 200Hz 16:9 */
				pstDtd->h_active = 720;
				pstDtd->v_active = 576;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 49;
				pstDtd->h_sync_offset = 12;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 64;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 54: /* 720(1440)x576i @ 200Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 55: /* 720(1440)x576i @ 200Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 288;
				pstDtd->h_blanking = 288;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 126;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 56: /* 720x480p @ 239.76/240Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 720;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 138;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 62;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 57: /* 720x480p @ 239.76/240Hz 16:9 */
				pstDtd->h_active = 720;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 138;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 9;
				pstDtd->h_sync_pulse_width = 62;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 58: /* 720(1440)x480i @ 239.76/240Hz 4:3 */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 59: /* 720(1440)x480i @ 239.76/240Hz 16:9 */
				pstDtd->h_active = 1440;
				pstDtd->v_active = 240;
				pstDtd->h_blanking = 276;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 38;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 124;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 1;
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 65:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 2020;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 1760;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 60: /* 1280x720p @ 23.97/24Hz 16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 2020;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 1760;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 66:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 2680;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 2420;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 61: /* 1280x720p @ 25Hz 16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 2680;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 2420;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 67:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 2020;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 1760;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 62: /* 1280x720p @ 29.97/30Hz  16:9 */
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 2020;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 1760;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 74250;
				break;
			case 78:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 63: /* 1920x1080p @ 119.88/120Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 77:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 64: /* 1920x1080p @ 100Hz 16:9 */
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 720;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 528;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 79:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 1620;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 1360;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 80:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 1488;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 1228;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 81:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 960;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 700;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 82:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 520;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 260;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 82500;
				break;
			case 83:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 520;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 260;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 99000;
				break;
			case 84:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 320;
				pstDtd->v_blanking = 105;
				pstDtd->h_sync_offset = 60;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 165000;
				break;
			case 85:
				pstDtd->h_active = 1680;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 320;
				pstDtd->v_blanking = 105;
				pstDtd->h_sync_offset = 60;
				pstDtd->v_sync_offset = 5;
				pstDtd->h_sync_pulse_width = 40;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 198000;
				break;
			case 86:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 1190;
				pstDtd->v_blanking = 20;
				pstDtd->h_sync_offset = 998;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 99000;
				break;
			case 87:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 640;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 448;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 90000;
				break;
			case 88:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 960;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 768;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 118800;
				break;
			case 89:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 740;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 548;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 185625;
				break;
			case 90:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 440;
				pstDtd->v_blanking = 20;
				pstDtd->h_sync_offset = 248;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 198000;
				break;
			case 91:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 410;
				pstDtd->v_blanking = 170;
				pstDtd->h_sync_offset = 218;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 371250;
				break;
			case 92:
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 740;
				pstDtd->v_blanking = 170;
				pstDtd->h_sync_offset = 548;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 495000;
				break;
			case 101:
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1184;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 968;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 59400;
				break;
			case 100:
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 304;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 99:
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1184;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 968;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 102:
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 304;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 103:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1660;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1276;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 93:		/* 4k x 2k, 30Hz */
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1660;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1276;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 104:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1440;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1056;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 94:
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1440;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1056;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 105:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 176;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 95:
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 176;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			case 106:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1440;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1056;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 96:
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1440;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1056;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 107:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 176;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 97:
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 176;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 594000;
				break;
			case 98:
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 1404;
				pstDtd->v_blanking = 90;
				pstDtd->h_sync_offset = 1020;
				pstDtd->v_sync_offset = 8;
				pstDtd->h_sync_pulse_width = 88;
				pstDtd->v_sync_pulse_width = 10;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;
				pstDtd->uiPixel_Clock = 297000;
				break;
			default:
				//dptx_err("Invalid video code = %d ", uiVideo_Code );
				return ( DPTX_RETURN_FAIL  );
		}
	} 
	else if( ucVideoFormat == VIDEO_FORMAT_VESA_CVT ) 
	{
		switch( uiVideo_Code ) 
		{
			case 1:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 640;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 20;
				pstDtd->h_sync_offset = 8;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 8;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 23750;
				break;
			case 2:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 800;
				pstDtd->v_active = 600;
				pstDtd->h_blanking = 224;
				pstDtd->v_blanking = 24;
				pstDtd->h_sync_offset = 31;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 81;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 38250;
				break;
			case 3:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1024;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 304;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 104;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 63500;
				break;
			case 4:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 960;
				pstDtd->h_blanking = 416;
				pstDtd->v_blanking = 36;
				pstDtd->h_sync_offset = 80;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 101250;
				break;
			case 5:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1400;
				pstDtd->v_active = 1050;
				pstDtd->h_blanking = 464;
				pstDtd->v_blanking = 39;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 144;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 121750;
				break;
			case 6:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1600;
				pstDtd->v_active = 1200;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 112;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 68;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 161000;
				break;
			case 12:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 1024;
				pstDtd->h_blanking = 432;
				pstDtd->v_blanking = 39;
				pstDtd->h_sync_offset = 80;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 136;
				pstDtd->v_sync_pulse_width = 7;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 109000;
				break;
			case 13:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 384;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 7;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 79500;
				break;
			case 16:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 720;
				pstDtd->h_blanking = 384;
				pstDtd->v_blanking = 28;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 74500;
				break;
			case 17:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1360;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 416;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 72;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 136;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 84750;
				break;
			case 20:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 656;
				pstDtd->v_blanking = 40;
				pstDtd->h_sync_offset = 128;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 200;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 173000;
				break;
			case 22:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2560;
				pstDtd->v_active = 1440;
				pstDtd->h_blanking = 928;
				pstDtd->v_blanking = 53;
				pstDtd->h_sync_offset = 192;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 272;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 312250;
				break;
			case 28:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 800;
				pstDtd->h_blanking = 400;
				pstDtd->v_blanking = 31;
				pstDtd->h_sync_offset = 72;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 83500;
				break;
			case 34:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1200;
				pstDtd->h_blanking = 672;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 136;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 200;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 193250;
				break;
			case 38:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2400;
				pstDtd->h_blanking = 80;
				pstDtd->v_blanking = 69;
				pstDtd->h_sync_offset = 320;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 424;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 580128;
				break;
			case 40:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1600;
				pstDtd->v_active = 1200;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 35;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 124076;
				break;
			case 41:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 2048;
				pstDtd->v_active = 1536;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 44;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 208000;
				break;
			default:
				//dptx_err("Invalid video code = %d \n", uiVideo_Code );
				return ( DPTX_RETURN_FAIL  );
		}
	} 
	else if( ucVideoFormat == VIDEO_FORMAT_VESA_DMT ) 
	{
		switch( uiVideo_Code ) 
		{
			case 1: // HISilicon timing
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3600;
				pstDtd->v_active = 1800;
				pstDtd->h_blanking = 120;
				pstDtd->v_blanking = 128;
				pstDtd->h_sync_offset = 20;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 20;
				pstDtd->v_sync_pulse_width = 2;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 645500;
				break;
			case 2:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 3840;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 62;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 533000;
				break;
			case 4:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 640;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 144;
				pstDtd->v_blanking = 29;
				pstDtd->h_sync_offset = 8;
				pstDtd->v_sync_offset = 2;
				pstDtd->h_sync_pulse_width = 96;
				pstDtd->v_sync_pulse_width = 2;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 25175;
				break;
			case 13:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 800;
				pstDtd->v_active = 600;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 36;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 73250;
				break;
			case 14: /* 848x480p@60Hz */
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 848;
				pstDtd->v_active = 480;
				pstDtd->h_blanking = 240;
				pstDtd->v_blanking = 37;
				pstDtd->h_sync_offset = 16;
				pstDtd->v_sync_offset = 6;
				pstDtd->h_sync_pulse_width = 112;
				pstDtd->v_sync_pulse_width = 8;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI)  */;
				pstDtd->uiPixel_Clock = 33750;
				break;
			case 22:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 22;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 7;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 68250;
				break;
			case 35:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 1024;
				pstDtd->h_blanking = 408;
				pstDtd->v_blanking = 42;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 112;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 39:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1360;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 432;
				pstDtd->v_blanking = 27;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 112;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 85500;
				break;
			case 40:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1360;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 148250;
				break;
			case 81:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1366;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 426;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 70;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 142;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 85500;
				break;
			case 86:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1366;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 134;
				pstDtd->v_blanking = 32;
				pstDtd->h_sync_offset = 14;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 56;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 72000;
				break;
			case 87:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 80;
				pstDtd->v_blanking = 62;
				pstDtd->h_sync_offset = 8;
				pstDtd->v_sync_offset = 48;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 8;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 556744;
				break;
			case 88:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 4096;
				pstDtd->v_active = 2160;
				pstDtd->h_blanking = 80;
				pstDtd->v_blanking = 62;
				pstDtd->h_sync_offset = 8;
				pstDtd->v_sync_offset = 48;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 8;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 556188;
				break;
			case 41:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1400;
				pstDtd->v_active = 1050;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 101000;
				break;
			case 42:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1400;
				pstDtd->v_active = 1050;
				pstDtd->h_blanking = 464;
				pstDtd->v_blanking = 39;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 144;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 121750;
				break;
			case 46:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 900;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 26;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 88750;
				break;
			case 47:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1440;
				pstDtd->v_active = 900;
				pstDtd->h_blanking = 464;
				pstDtd->v_blanking = 34;
				pstDtd->h_sync_offset = 80;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 152;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 106500;
				break;
			case 51:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1600;
				pstDtd->v_active = 1200;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 50;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 192;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 162000;
				break;
			case 57:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1680;
				pstDtd->v_active = 1050;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 119000;
				break;
			case 58:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1680;
				pstDtd->v_active = 1050;
				pstDtd->h_blanking = 560;
				pstDtd->v_blanking = 39;
				pstDtd->h_sync_offset = 104;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 176;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 146250;
				break;
			case 68:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1200;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 35;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 154000;
				break;
			case 69:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1200;
				pstDtd->h_blanking = 672;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 136;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 200;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 193250;
				break;
			case 82:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1080;
				pstDtd->h_blanking = 280;
				pstDtd->v_blanking = 45;
				pstDtd->h_sync_offset = 88;
				pstDtd->v_sync_offset = 4;
				pstDtd->h_sync_pulse_width = 44;
				pstDtd->v_sync_pulse_width = 5;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 148500;
				break;
			case 83:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1600;
				pstDtd->v_active = 900;
				pstDtd->h_blanking = 200;
				pstDtd->v_blanking = 100;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 80;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 9:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 800;
				pstDtd->v_active = 600;
				pstDtd->h_blanking = 256;
				pstDtd->v_blanking = 28;
				pstDtd->h_sync_offset = 40;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 4;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 40000;
				break;
			case 16:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1024;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 320;
				pstDtd->v_blanking = 38;
				pstDtd->h_sync_offset = 24;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 136;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 65000;
				break;
			case 23:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 768;
				pstDtd->h_blanking = 384;
				pstDtd->v_blanking = 30;
				pstDtd->h_sync_offset = 64;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 128;
				pstDtd->v_sync_pulse_width = 7;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 79500;
				break;
			case 62:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active =  1792;
				pstDtd->v_active = 1344;
				pstDtd->h_blanking = 656;
				pstDtd->v_blanking =  50;
				pstDtd->h_sync_offset = 128;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 200;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0; /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 204750;
				break;
			case 32:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 960;
				pstDtd->h_blanking = 520;
				pstDtd->v_blanking = 40;
				pstDtd->h_sync_offset = 96;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 112;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;  /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 108000;
				break;
			case 73:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1920;
				pstDtd->v_active = 1440;
				pstDtd->h_blanking = 680;
				pstDtd->v_blanking = 60;
				pstDtd->h_sync_offset = 128;
				pstDtd->v_sync_offset = 1;
				pstDtd->h_sync_pulse_width = 208;
				pstDtd->v_sync_pulse_width = 3;
				pstDtd->h_sync_polarity = 0;
				pstDtd->v_sync_polarity = 1;
				pstDtd->interlaced = 0;  /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 234000;
				break;
			case 27:
				pstDtd->h_image_size = 4;
				pstDtd->v_image_size = 3;
				pstDtd->h_active = 1280;
				pstDtd->v_active = 800;
				pstDtd->h_blanking = 160;
				pstDtd->v_blanking = 23;
				pstDtd->h_sync_offset = 48;
				pstDtd->v_sync_offset = 3;
				pstDtd->h_sync_pulse_width = 32;
				pstDtd->v_sync_pulse_width = 6;
				pstDtd->h_sync_polarity = 1;
				pstDtd->v_sync_polarity = 0;
				pstDtd->interlaced = 0;  /* (progressive_nI) */
				pstDtd->uiPixel_Clock = 71000;
				break;
			default:
				//dptx_err("Invalid video code = %d ", uiVideo_Code );
				return ( DPTX_RETURN_FAIL  );
		}
	}
	else
	{
		dptx_err("Invalid video format = %d \n", ucVideoFormat );
		return ( DPTX_RETURN_FAIL  );
	}

	return ( DPTX_RETURN_SUCCESS );
}

