// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/slab.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"


#define EDID_I2C_ADDR				0x50
#define EDID_I2C_SEGMENT_ADDR		0x30

#define EDID_MAX_EXTRA_BLK			4

#define PRINT_BUF_SIZE			1024

/* EDID Audio Data Block */
#define AUDIO_TAG			1
#define VIDEO_TAG			2

#define EDID_TAG_MASK		GENMASK(7,5)
#define EDID_TAG_SHIFT		5
#define EDID_SIZE_MASK		GENMASK(4,0)
#define EDID_SIZE_SHIFT		0

/* Established timing blocks */
#define ET1_800x600_60hz		BIT(0)
#define ET1_800x600_56hz		BIT(1)
#define ET1_640x480_75hz		BIT(2)
#define ET1_640x480_72hz		BIT(3)
#define ET1_640x480_67hz		BIT(4)
#define ET1_640x480_60hz		BIT(5)
#define ET1_720x400_88hz		BIT(6)
#define ET1_720x400_70hz		BIT(7)

#define ET2_1280x1024_75hz		BIT(0)
#define ET2_1024x768_75hz		BIT(1)
#define ET2_1024x768_70hz		BIT(2)
#define ET2_1024x768_60hz		BIT(3)
#define ET2_1024x768_87hz		BIT(4)
#define ET2_832x624_75hz		BIT(5)
#define ET2_800x600_75hz		BIT(6)
#define ET2_800x600_72hz		BIT(7)
#define ET3_1152x870_75hz		BIT(7)


static void dptx_edid_Print_U8_Buf( u8 *pucBuf, u32 uiStart_RegOffset, u32 uiLength  )
{
	int			iOffset;
	char		acStr[PRINT_BUF_SIZE];
	int			iNumOfWritten = 0;
	
	iNumOfWritten += snprintf( &acStr[iNumOfWritten], PRINT_BUF_SIZE - iNumOfWritten, "\n" );

	for( iOffset = 0; iOffset < uiLength; iOffset++ ) 
	{
		if( !( iOffset % 16 ) ) 
		{
			iNumOfWritten += snprintf( &acStr[iNumOfWritten], PRINT_BUF_SIZE - iNumOfWritten, "\n%02x:", ( uiStart_RegOffset + iOffset ));
			if( iNumOfWritten >= PRINT_BUF_SIZE )
			{
				break;
			}

		}

		iNumOfWritten += snprintf( &acStr[iNumOfWritten],  PRINT_BUF_SIZE - iNumOfWritten, " %02x", pucBuf[iOffset] );
		if( iNumOfWritten >= PRINT_BUF_SIZE )
		{
			break;
		}
	}

	printk("%s", acStr);
}


static bool dptx_edid_parse_established_timing( struct Dptx_Params * dptx )
{
	u8 byte1, byte2, byte3;

	byte1 = dptx->pucEdidBuf[35];
	byte2 = dptx->pucEdidBuf[36];
	byte3 = dptx->pucEdidBuf[37];

	if( byte1 & ET1_800x600_60hz ) 
	{
		dptx_dbg("Sink supports ET1_800x600_60hz\n");
		
		dptx->eEstablished_Timing = DMT_800x600_60hz;
		return ( DPTX_RETURN_SUCCESS );
	}
	if( byte1 & ET1_640x480_60hz ) 
	{
		dptx_dbg("Sink supports ET1_640x480_60hz " );
		
		dptx->eEstablished_Timing = DMT_640x480_60hz;
		return ( DPTX_RETURN_SUCCESS );
	}
	if( byte2 & ET2_1024x768_60hz ) 
	{
		dptx_dbg("Sink supports ET2_1024x768_60hz " );
		
		dptx->eEstablished_Timing = DMT_1024x768_60hz;
		return ( DPTX_RETURN_SUCCESS );
	}

	if( byte1 & ET1_800x600_56hz ) 
		dptx_dbg("Sink supports ET1_800x600_56hz, but we dont " );
	if( byte1 & ET1_640x480_75hz )
		dptx_dbg("Sink supports ET1_640x480_75hz, but we dont " );
	if( byte1 & ET1_640x480_72hz )
		dptx_dbg("Sink supports ET1_640x480_72hz, but we dont " );
	if( byte1 & ET1_640x480_67hz )
		dptx_dbg("Sink supports ET1_640x480_67hz, but we dont " );
	if( byte1 & ET1_720x400_88hz )
		dptx_dbg("Sink supports ET1_720x400_88hz, but we dont " );
	if( byte1 & ET1_720x400_70hz )
		dptx_dbg("Sink supports ET1_720x400_70hz, but we dont " );

	if( byte2 & ET2_1280x1024_75hz )
		dptx_dbg("Sink supports ET2_1280x1024_75hz, but we dont " );
	if( byte2 & ET2_1024x768_75hz )
		dptx_dbg("Sink supports ET2_1024x768_75hz, but we dont " );
	if( byte2 & ET2_1024x768_70hz )
		dptx_dbg("Sink supports ET2_1024x768_70hz, but we dont " );
	if( byte2 & ET2_1024x768_87hz )
		dptx_dbg("Sink supports ET2_1024x768_87hz, but we dont " );
	if( byte2 & ET2_832x624_75hz )
		dptx_dbg("Sink supports ET2_832x624_75hz, but we dont " );
	if( byte2 & ET2_800x600_75hz )
		dptx_dbg("Sink supports ET2_800x600_75hz, but we dont " );
	if( byte2 & ET2_800x600_72hz )
		dptx_dbg("Sink supports ET2_800x600_72hz, but we dont " );

	if( byte3 & ET3_1152x870_75hz )
		dptx_dbg("Sink supports ET3_1152x870_75hz, but we dont " );


	dptx->eEstablished_Timing = DMT_NONE;

	return ( DPTX_RETURN_SUCCESS );
}

static bool dptx_edid_read_block( struct Dptx_Params *pstDptx,	u8 ucBlockNum )
{
	bool	bRetVal, bRetry = false;
	u8		ucOffset, ucSegment;
	u8		*pucBuffer_Copied;

again:
	ucOffset  = ( ucBlockNum * DPTX_EDID_BUFLEN );
	ucSegment = ( ucBlockNum >> 1 );

	bRetVal = Dptx_Aux_Write_Bytes_To_I2C( pstDptx, EDID_I2C_SEGMENT_ADDR, &ucSegment, 1 );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("Error from Dptx_Aux_Write_Bytes_To_I2C() by block %d ", ucBlockNum );
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal |= Dptx_Aux_Write_Bytes_To_I2C( pstDptx, EDID_I2C_ADDR, &ucOffset, 1 );/* TODO Skip if no E-DDC */
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		dptx_err("Error from Dptx_Aux_Write_Bytes_To_I2C() by block %d ", ucBlockNum );
		return ( DPTX_RETURN_FAIL );
	}
	
	bRetVal |= Dptx_Aux_Read_Bytes_From_I2C( pstDptx, EDID_I2C_ADDR, &pstDptx->pucEdidBuf[ucBlockNum * DPTX_EDID_BUFLEN], DPTX_EDID_BUFLEN );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		if( !bRetry ) // retry if edid read failed
		{
			bRetry = true;
			dptx_warn("Retry from Dptx_Aux_Read_Bytes_From_I2C() by block %d ", ucBlockNum );
			goto again;
		}
		else
		{
			dptx_err("Error from Dptx_Aux_Read_Bytes_From_I2C() by block %d ", ucBlockNum );
			return ( DPTX_RETURN_FAIL );
		}
	}

	bRetVal = Dptx_Aux_Write_AddressOnly_To_I2C( pstDptx, 0x50 );
    if( bRetVal == DPTX_RETURN_FAIL )
	{
		memset( pstDptx->pucSecondary_EDID, 0, DPTX_EDID_BUFLEN );
		return ( DPTX_RETURN_FAIL );
	}

	if( ucBlockNum > 0 )
	{
		pucBuffer_Copied = ( pstDptx->pucEdidBuf + DPTX_EDID_BUFLEN );
		memcpy( pstDptx->pucSecondary_EDID, pucBuffer_Copied, DPTX_EDID_BUFLEN );
	}

	pucBuffer_Copied = ( pstDptx->pucEdidBuf + ( DPTX_EDID_BUFLEN * ucBlockNum ));

	dptx_edid_Print_U8_Buf( pucBuffer_Copied, 0, DPTX_EDID_BUFLEN );

	return ( DPTX_RETURN_SUCCESS );
}


static void dptx_edid_get_audio_sampling_freq_based_on_edid( struct Dptx_Params *pstDptx, int iEDID_Index )
{
	u8										ucSample_Freq;
	struct Dptx_Audio_Short_Descriptor		*pstAudio_Short_Descriptor =     &pstDptx->stAudio_Short_Descriptor;

	ucSample_Freq = ( pstDptx->pucSecondary_EDID[iEDID_Index + 2] & GENMASK(6, 0) );

	pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_INVALID;

	if( ucSample_Freq & BIT(0) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 32khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_32;
	}
	if( ucSample_Freq & BIT(1) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 44.1khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_44_1;
	}
	if( ucSample_Freq & BIT(2) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 48khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_48;
	}
	if( ucSample_Freq & BIT(3) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 88.2khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_88_2;
	}
	if( ucSample_Freq & BIT(4) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 96khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_96;
	}
	if( ucSample_Freq & BIT(5) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 176.4khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_176_4;
	}
	if( ucSample_Freq & BIT(6) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 192khz audio " );
		pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq = SAMPLE_FREQ_192;
	}
}

static void dptx_edid_audio_max_data_width_based_on_edid( struct Dptx_Params *pstDptx, int iEDID_Index )
{
	u8										bpsample;
	struct Dptx_Audio_Short_Descriptor		*pstAudio_Short_Descriptor = &pstDptx->stAudio_Short_Descriptor;

	dptx_dbg( "Calling... edid_index = %d ", iEDID_Index );

	bpsample = pstDptx->pucSecondary_EDID[iEDID_Index + 3] & GENMASK(2, 0);

	pstAudio_Short_Descriptor->ucEDID_Max_Bit_Per_Sample = MAX_INPUT_DATA_WIDTH_INVALID;

	if( bpsample & BIT(0) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 16 bit audio " );
		pstAudio_Short_Descriptor->ucEDID_Max_Bit_Per_Sample = 16;
	}
	if( bpsample & BIT(1) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 20 bit audio " );
		pstAudio_Short_Descriptor->ucEDID_Max_Bit_Per_Sample = 20;
	}
	if( bpsample & BIT(2) ) 
	{
		dptx_dbg("AUDIO EDID: Sink supports 24 bit audio " );
		pstAudio_Short_Descriptor->ucEDID_Max_Bit_Per_Sample = 24;
	}
}

static void dptx_edid_fill_audio_short_desc( struct Dptx_Params *pstDptx, int iEDID_Index )
{
	struct Dptx_Audio_Short_Descriptor		*pstAudio_Short_Descriptor     = &pstDptx->stAudio_Short_Descriptor;


	pstAudio_Short_Descriptor->ucEDID_Max_Input_NumOfCh = ( ( pstDptx->pucSecondary_EDID[iEDID_Index + 1] & GENMASK(2, 0) ) + 1 );
	
	dptx_dbg("AUDIO EDID: Sink supports up to [%d] channels  ", pstAudio_Short_Descriptor->ucEDID_Max_Input_NumOfCh );

	/* Detect audio sampling frequency supported by the sink*/
	dptx_edid_get_audio_sampling_freq_based_on_edid( pstDptx, iEDID_Index );

	/* Detect bit per sample supported by the sink */
	dptx_edid_audio_max_data_width_based_on_edid( pstDptx, iEDID_Index );
}


bool Dptx_Edid_Check_Detailed_Timing_Descriptors( struct Dptx_Params *pstDptx )
{
	if(( pstDptx->pucEdidBuf[54] == 0 && pstDptx->pucEdidBuf[55] == 0 ) && ( pstDptx->pucEdidBuf[72] == 0 && pstDptx->pucEdidBuf[73] == 0 ) &&
		( pstDptx->pucEdidBuf[90] == 0 && pstDptx->pucEdidBuf[91] == 0 ) && ( pstDptx->pucEdidBuf[108] == 0 && pstDptx->pucEdidBuf[109] == 0 )) 
	{
		dptx_dbg( "Establish timing bitmap is presented..." );
	
		pstDptx->bEstablish_Timing_Present = true;
		
		dptx_edid_parse_established_timing( pstDptx );
	}
	else 
	{
		dptx_dbg("Detailed timing descriptor is presented... continuing with usual way " );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Edid_Read_EDID_I2C_Over_Aux( struct Dptx_Params *pstDptx )
{
	bool			bRetVal;
	u8				*pucFirst_Edid_Block, ucExt_Blocks;
	unsigned int	i;

	memset( pstDptx->pucEdidBuf, 0, DPTX_EDID_BUFLEN );
	
	bRetVal = dptx_edid_read_block( pstDptx, 0 );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	ucExt_Blocks = pstDptx->pucEdidBuf[126];
	if( ucExt_Blocks == 0 )
	{
		dptx_edid_Print_U8_Buf( pstDptx->pucEdidBuf, 0, DPTX_EDID_BUFLEN );
		return ( DPTX_RETURN_SUCCESS );
	}

	if( ucExt_Blocks > EDID_MAX_EXTRA_BLK ) 
	{
		dptx_warn("num_ext_blocks( %d ) are larger than Max 4 -> down to 4 ", pstDptx->pucEdidBuf[126] );
		ucExt_Blocks = EDID_MAX_EXTRA_BLK;
	}

		dptx_dbg("The number of num_ext_blocks = %d ", pstDptx->pucEdidBuf[126] );

	pucFirst_Edid_Block = kmalloc( DPTX_EDID_BUFLEN, GFP_KERNEL );
	if( pucFirst_Edid_Block == NULL ) 
	{
		dptx_err("pucFirst_Edid_Block is NULL " );
		return ( DPTX_RETURN_FAIL );
	}
		
	memcpy( pucFirst_Edid_Block, pstDptx->pucEdidBuf, DPTX_EDID_BUFLEN );

	kfree( pstDptx->pucEdidBuf );
	
	pstDptx->pucEdidBuf = kmalloc( DPTX_EDID_BUFLEN * ucExt_Blocks + DPTX_EDID_BUFLEN, GFP_KERNEL );
	if( pstDptx->pucEdidBuf == NULL ) 
	{
		dptx_err("dptx->edid is NULL " );
		return ( DPTX_RETURN_FAIL );
	}
	
	memcpy( pstDptx->pucEdidBuf, pucFirst_Edid_Block, DPTX_EDID_BUFLEN );

	for( i = 1; i <= ucExt_Blocks; i++ ) 
	{
		bRetVal = dptx_edid_read_block( pstDptx, i );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			goto fail;
		}
	}

	kfree( pucFirst_Edid_Block );

	return ( DPTX_RETURN_SUCCESS );
	
fail:
	kfree( pucFirst_Edid_Block );
	
	return ( DPTX_RETURN_FAIL );
}

bool Dptx_Edid_Read_EDID_Over_Sideband_Msg( struct Dptx_Params *pstDptx, u8 ucStream_Index )
{
	bool		bRetVal;
	u8			ucExt_Blocks;

	memset( pstDptx->pucEdidBuf, 0, DPTX_EDID_BUFLEN );
	memset( pstDptx->pucSecondary_EDID, 0, DPTX_EDID_BUFLEN );

	bRetVal = Dptx_Ext_Remote_I2C_Read( pstDptx, ucStream_Index );

	dptx_edid_Print_U8_Buf( pstDptx->pucEdidBuf, 0, DPTX_EDID_BUFLEN );
	
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	ucExt_Blocks = pstDptx->pucEdidBuf[126];

	if( ucExt_Blocks > 0 )
	{
		dptx_edid_Print_U8_Buf( pstDptx->pucSecondary_EDID, 0, DPTX_EDID_BUFLEN );
	}

	return ( DPTX_RETURN_SUCCESS );
}


bool Dptx_Edid_Verify_EDID( struct Dptx_Params *pstDptx )
{
	const u8	aucEdid_Header[] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
	int			iRetVal;
	int			iElements;
	u32			uiEdid_CheckSum = 0;

	iRetVal = memcmp( pstDptx->pucEdidBuf, aucEdid_Header, sizeof(aucEdid_Header));
    if( iRetVal )
	{
        dptx_err( "Invalid EDID header( 0x%x, 0x%x, 0x%x, 0x%x, 0x%x ) ", pstDptx->pucEdidBuf[0], pstDptx->pucEdidBuf[1], pstDptx->pucEdidBuf[2], pstDptx->pucEdidBuf[3], pstDptx->pucEdidBuf[4] );
		return ( DPTX_RETURN_FAIL );
    }

	for( iElements = 0; iElements < DPTX_EDID_BUFLEN; iElements++ )
	{
		uiEdid_CheckSum += pstDptx->pucEdidBuf[iElements];
	}
	
	if( uiEdid_CheckSum & 0xFF ) 
	{
		dptx_err("Invalid EDID checksum( 0x%x )", uiEdid_CheckSum);
		return ( DPTX_RETURN_FAIL );
	}
	
	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Edid_Parse_Audio_Data_Block( struct Dptx_Params *pstDptx ) 
{
	u8				ucByteVal, ucTag, ucSize;
	int				iIndex = 4;

	ucByteVal	= pstDptx->pucSecondary_EDID[4];
	ucTag		= ( ( ucByteVal & EDID_TAG_MASK ) >> EDID_TAG_SHIFT );
	ucSize		= ( ( ucByteVal & EDID_SIZE_MASK ) >> EDID_SIZE_SHIFT );

	while( ucTag != AUDIO_TAG ) /* find the audio tag  containing byte */
	{
		ucSize = ( ( ucByteVal & EDID_SIZE_MASK ) >> EDID_SIZE_SHIFT );
		
		iIndex = ( iIndex + ucSize + 1 );
		
		ucByteVal = pstDptx->pucSecondary_EDID[iIndex];
		ucTag = ( ( ucByteVal & EDID_TAG_MASK) >> EDID_TAG_SHIFT );
	}

	dptx_edid_fill_audio_short_desc( pstDptx, iIndex );

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Edid_Config_Audio_BasedOn_EDID( struct Dptx_Params *pstDptx )
{
//	u32											audio_clock_freq;
	enum AUDIO_IEC60958_3_ORIGINAL_SAMPLE_FREQ	eIEC60958_3_ORG_SAMPLE_FREQ; 
	enum AUDIO_IEC60958_3_SAMPLE_FREQ			eIEC60958_3_SAMPLE_FREQ;
	struct Dptx_Audio_Short_Descriptor			*pstAudio_Short_Descriptor = &pstDptx->stAudio_Short_Descriptor;

	if( pstAudio_Short_Descriptor->ucEDID_Max_Input_NumOfCh > EDID_MAX_INPUT_NUM_OF_CHANNELS /* 8 channels */)
	{
		dptx_err("Invalid max num of input channels = %d", pstAudio_Short_Descriptor->ucEDID_Max_Input_NumOfCh);
		return ( DPTX_RETURN_FAIL );
	}

	/* Configure sampling frequency */
	switch( pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq ) 
	{
		case SAMPLE_FREQ_32:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_32");
			eIEC60958_3_ORG_SAMPLE_FREQ	= IEC60958_3_ORIGINAL_SAMPLE_FREQ_32;
			eIEC60958_3_SAMPLE_FREQ 	= IEC60958_3_SAMPLE_FREQ_32;
//			audio_clock_freq 			= 320;
			break;
		case SAMPLE_FREQ_44_1:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_44_1\n");
			eIEC60958_3_ORG_SAMPLE_FREQ = IEC60958_3_ORIGINAL_SAMPLE_FREQ_44_1;
			eIEC60958_3_SAMPLE_FREQ 	= IEC60958_3_SAMPLE_FREQ_44_1;
//			audio_clock_freq 			= 441;
			break;
		case SAMPLE_FREQ_48:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_48");
			eIEC60958_3_ORG_SAMPLE_FREQ = IEC60958_3_ORIGINAL_SAMPLE_FREQ_48;
			eIEC60958_3_SAMPLE_FREQ 	= IEC60958_3_SAMPLE_FREQ_48;
//			audio_clock_freq 			= 480;
			break;
		case SAMPLE_FREQ_88_2:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_88_2");
			eIEC60958_3_ORG_SAMPLE_FREQ	= IEC60958_3_ORIGINAL_SAMPLE_FREQ_88_2;
			eIEC60958_3_SAMPLE_FREQ 	= IEC60958_3_SAMPLE_FREQ_88_2;
//			audio_clock_freq			= 882;
			break;
		case SAMPLE_FREQ_96:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_96");
			eIEC60958_3_ORG_SAMPLE_FREQ	= IEC60958_3_ORIGINAL_SAMPLE_FREQ_96;
			eIEC60958_3_SAMPLE_FREQ		= IEC60958_3_SAMPLE_FREQ_96;
//			audio_clock_freq			= 960;
			break;
		case SAMPLE_FREQ_176_4:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_176_4");
			eIEC60958_3_ORG_SAMPLE_FREQ = IEC60958_3_ORIGINAL_SAMPLE_FREQ_176_4;
			eIEC60958_3_SAMPLE_FREQ 	= IEC60958_3_SAMPLE_FREQ_176_4;
//			audio_clock_freq			= 1764;
			break;
		case SAMPLE_FREQ_192:
			dptx_dbg("SHORT AUDIO DESC AUDIO_SAMPLING_RATE_192");
			eIEC60958_3_ORG_SAMPLE_FREQ = IEC60958_3_ORIGINAL_SAMPLE_FREQ_192;
			eIEC60958_3_SAMPLE_FREQ 	= IEC60958_3_SAMPLE_FREQ_192;
//			audio_clock_freq			= 1920;
			break;
		default:
			dptx_err("Invalid SHORT AUDIO DESC AUDIO_SAMPLING_RATE");
			return ( DPTX_RETURN_FAIL );
	}

	dptx_dbg("max sample freq = %d, sample_freq = %d, eIEC60958_3_ORG_SAMPLE_FREQ = %d ", 
							pstAudio_Short_Descriptor->eEDID_Max_Sampling_Freq, eIEC60958_3_SAMPLE_FREQ, eIEC60958_3_ORG_SAMPLE_FREQ );

	Dptx_Avgen_Set_Audio_DataWidth( pstDptx, pstAudio_Short_Descriptor->ucEDID_Max_Bit_Per_Sample );
	Dptx_Avgen_Set_Audio_MaxNumOfChannels( pstDptx, pstAudio_Short_Descriptor->ucEDID_Max_Input_NumOfCh );
	Dptx_Avgen_Set_Audio_SamplingFreq( pstDptx, eIEC60958_3_SAMPLE_FREQ, eIEC60958_3_ORG_SAMPLE_FREQ );
	Dptx_Avgen_Set_Audio_SDP_InforFrame( pstDptx, false );

	return ( DPTX_RETURN_SUCCESS );
}



