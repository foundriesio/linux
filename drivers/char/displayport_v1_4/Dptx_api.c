// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/delay.h>

#include "Dptx_api.h"
#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"


//#define ENABLE_CTS_TEST

#define MAX_CHECK_HPD_NUM					200

#define REG_PRINT_BUF_SIZE					1024
#define VIDEO_REG_DUMP_START_OFFSET			0x300
#define VIDEO_REG_DUMP_SIZE					( 0x34 / 4 )

#define VCP_REG_DUMP_START_OFFSET			0x200
#define VCP_REG_DUMP_SIZE					( 0x30 / 4 )

#define VCP_DPCD_DUMP_SIZE					64

struct Dpv14_Tx_API_Params 
{
	bool		bIntialized;
	bool		bHotPlugged;
	u8			ucNumOfStreams;
	u32 		auiVideoCode[DPTX_INPUT_STREAM_MAX];
	u32 		auiPeri_PixelClk[DPTX_INPUT_STREAM_MAX];
};

static struct Dpv14_Tx_API_Params stDpv14_Tx_API_Params = {0, };


bool Dpv14_Tx_API_Set_Core_Params( struct DPTX_API_Core_Params	*pstDpCore_Params )
{
	bool				bRetVal, bHPDStatus;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		dptx_err( "from Dptx_Intr_Get_HotPlug_Status()" );
		return ( DPTX_API_RETURN_FAIL );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Set_Params( pstHandle, (u8)pstDpCore_Params->eMaxNumOfLanes, (u8)pstDpCore_Params->eMaxRate, pstDpCore_Params->bSSC_On );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		dptx_err("from Dptx_Avgen_Set_Video_Params() " );
		return ( DPTX_API_RETURN_FAIL );
	}
	
	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Core_Params( struct DPTX_API_Core_Params	*pstDpCore_Params )
{
	bool				bRetVal, bEnabledSSC;
	u8					ucMaxNumOfLanes, ucMaxRate;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		dptx_err("from Dptx_Avgen_Set_Video_Params() " );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Get_Params( pstHandle, &ucMaxNumOfLanes, &ucMaxRate, &bEnabledSSC );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		dptx_err("from Dptx_Avgen_Set_Video_Params() " );
		return ( DPTX_API_RETURN_FAIL );
	}

	pstDpCore_Params->eMaxNumOfLanes = ucMaxNumOfLanes;
	pstDpCore_Params->eMaxRate = ucMaxRate;
	pstDpCore_Params->bSSC_On = bEnabledSSC;

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_AVGen_Params( struct DPTX_API_AVGEN_Params	*pstDpAvGen_Params, u8 ucStream_Index )
{
	bool				bRetVal, bHPDStatus;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		dptx_err( "from Dptx_Intr_Get_HotPlug_Status()" );
		return ( DPTX_API_RETURN_FAIL );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( DPTX_API_RETURN_FAIL );
	}
	
	bRetVal = Dptx_Avgen_Set_Video_Params( pstHandle, 
												(u8)pstDpAvGen_Params->eColorimetry,
												(u8)pstDpAvGen_Params->eRGB_Standard,
												(u8)pstDpAvGen_Params->eVideo_Format,
												(u8)pstDpAvGen_Params->ePixel_Encoding,
												(u32)pstDpAvGen_Params->eRefresh_Rate,
												pstDpAvGen_Params->uiVideo_Code,
												ucStream_Index );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		dptx_err("from Dptx_Avgen_Set_Video_Params() " );
		return ( DPTX_API_RETURN_FAIL );
	}
	
	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_AVGen_Params( struct DPTX_API_AVGEN_Params	*pstDpAvGen_Params, u8 ucStream_Index )
{
	bool	bRetVal;
	u8		ucColorimetry, ucRGB_Standard,     ucVideo_Format, ucPixel_Encoding;
	u32		uiRefresh_Rate, uiVideo_Code;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}
	
	bRetVal = Dptx_Avgen_Get_Video_Params( pstHandle, &ucColorimetry, &ucRGB_Standard, &ucVideo_Format, &ucPixel_Encoding, &uiRefresh_Rate, &uiVideo_Code, ucStream_Index );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		dptx_err("from Dptx_Avgen_Set_Video_Params() " );
		return ( DPTX_API_RETURN_FAIL );
	}

	pstDpAvGen_Params->eColorimetry			= (enum DPTX_API_VIDEO_COLORIMETRY_TYPE)ucColorimetry;
	pstDpAvGen_Params->eRGB_Standard		= (enum DPTX_API_VIDEO_RGB_STANDARD_TYPE)ucRGB_Standard;
	pstDpAvGen_Params->eVideo_Format		= (enum DPTX_API_VIDEO_FORMAT_TYPE)ucVideo_Format;
	pstDpAvGen_Params->ePixel_Encoding		= (enum DPTX_API_VIDEO_PIXEL_ENCODING_TYPE)ucPixel_Encoding;
	pstDpAvGen_Params->eRefresh_Rate		= (enum DPTX_API_VIDEO_REFRESH_RATE)uiRefresh_Rate;
	pstDpAvGen_Params->uiVideo_Code			= uiVideo_Code;
	
	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Video_Mute( bool bMute, u8 ucStream_Index )
{
	bool				bRetVal, bHPDStatus;
	struct Dptx_Params 	*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		dptx_err( "from Dptx_Intr_Get_HotPlug_Status()" );
		return ( DPTX_API_RETURN_FAIL );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( DPTX_API_RETURN_FAIL );
	}

	if( bMute )
	{
		bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle,      false, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			dptx_err("from Dptx_Avgen_Set_Video_Stream_Enable() " );
			return ( DPTX_API_RETURN_FAIL );
		}
	}
	else
	{
		bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle,      true, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			dptx_err("from Dptx_Avgen_Set_Video_Stream_Enable() " );
			return ( DPTX_API_RETURN_FAIL );
		}
	}

	dptx_dbg("Stream %d to %s", ucStream_Index, bMute ? "mute":"unmute"  );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Video_Mute( bool *pbMute, u8 ucStream_Index )
{
	bool				bRetVal;
	bool				bEnabled_Stream;
	struct Dptx_Params 	*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Avgen_Get_Video_Stream_Enable( pstHandle,      &bEnabled_Stream, ucStream_Index );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		dptx_err("from Dptx_Avgen_Get_Video_Stream_Enable() " );
		return ( DPTX_API_RETURN_FAIL );
	}

	if( bEnabled_Stream )
	{
		*pbMute = false;
	}
	else
	{
		*pbMute = true;
	}

	dptx_dbg("Stream %d is %s", ucStream_Index, bEnabled_Stream ? "unmute":"mute"  );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Audio_Mute( bool bMute )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	Dptx_Avgen_Set_Audio_Mute( pstHandle, (u8)bMute );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_Mute( bool *pbMute )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	*pbMute = (bool)pstHandle->stAudioParams.ucInput_Mute;

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Audio_InterfaceType( enum DPTX_AUDIO_INTERFACE_TYPE eAudio_InfType )
{
	struct Dptx_Params			*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Set Audio interface type to %d", (u32)eAudio_InfType );

	Dptx_Avgen_Set_Audio_Input_InterfaceType( pstHandle, (u8)eAudio_InfType );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_InterfaceType( u8 *pucAudio_InfType )
{
	struct Dptx_Params			*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Get Audio interface type %d", (u32)pstHandle->stAudioParams.ucInput_InterfaceType );

	*pucAudio_InfType = pstHandle->stAudioParams.ucInput_InterfaceType;

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Audio_DataWidth( enum DPTX_AUDIO_DATA_WIDTH eAudio_DataWidth )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Set Audio data width to %d", (u32)eAudio_DataWidth );

	Dptx_Avgen_Set_Audio_DataWidth( pstHandle, (u8)( (u8)eAudio_DataWidth + 16 ) );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_DataWidth( u8 *pucAudio_DataWidth )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Get Audio data width %d", (u32)( pstHandle->stAudioParams.ucInput_DataWidth - 16 ) );

	*pucAudio_DataWidth = ( pstHandle->stAudioParams.ucInput_DataWidth - 16 );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Audio_HBR_Mode( bool bEnable )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Set Audio HBR Mode to %d", (u32)bEnable );

	Dptx_Avgen_Set_Audio_HBR_Mode( pstHandle, bEnable );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_HBR_Mode( bool *pbEnable )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Get Audio HBR Mode %d", (u32)pstHandle->stAudioParams.ucInput_HBR_Mode );

	*pbEnable =  (bool)pstHandle->stAudioParams.ucInput_HBR_Mode;

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Audio_Max_NumOfCh( enum  DPTX_AUDIO_NUM_OF_CHANNELS eAudio_NumOfCh )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Set Audio num of channes to %d", (u32)eAudio_NumOfCh );

	Dptx_Avgen_Set_Audio_MaxNumOfChannels( pstHandle, (u8)eAudio_NumOfCh );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_Max_NumOfCh( u8 *pucAudio_NumOfCh )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Get Audio num of channes %d", (u32)pstHandle->stAudioParams.ucInput_Max_NumOfchannels );

	*pucAudio_NumOfCh = pstHandle->stAudioParams.ucInput_Max_NumOfchannels;

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Set_Audio_Freq( enum AUDIO_IEC60958_3_SAMPLE_FREQ  eIEC_SamplingFreq )
{
	struct Dptx_Params 		*pstHandle;
	u8 eIEC_OriginSamplingFreq = 0;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}
	
	switch( eIEC_SamplingFreq ) 
	{
		case SAMPLE_FREQ_32:      
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_32;      
			break;
		case SAMPLE_FREQ_44_1:    
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_44_1;     
			break;
		case SAMPLE_FREQ_48:      
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_48;       
			break;
		case SAMPLE_FREQ_88_2:    
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_88_2;     
			break;
		case SAMPLE_FREQ_96:      
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_96;       
			break;
		case SAMPLE_FREQ_176_4:   
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_176_4;    
			break;
		case SAMPLE_FREQ_192:     
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_192;      
			break;
		default:
			eIEC_OriginSamplingFreq = IEC60958_3_SAMPLE_FREQ_INVALID;  
			break;
	}
	dptx_dbg("Set Audio Freq to %d", (u32)eIEC_SamplingFreq );

	Dptx_Avgen_Set_Audio_SamplingFreq( pstHandle, (u8) eIEC_SamplingFreq, (u8) eIEC_OriginSamplingFreq);

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_Freq( u8 *pucIEC_SamplingFreq )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Get Audio Sampling Freq %d", (u32)pstHandle->stAudioParams.ucIEC_Sampling_Freq );

	*pucIEC_SamplingFreq = pstHandle->stAudioParams.ucIEC_Sampling_Freq;

	return ( DPTX_API_RETURN_SUCCESS );
}


bool Dpv14_Tx_API_Enable_Audio_SDP_InfoFrame( bool bEnable )
{
	struct Dptx_Params 		*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	dptx_dbg("Set Audio SDP InforFrame to %d", (u32)bEnable );

	Dptx_Avgen_Set_Audio_SDP_InforFrame( pstHandle , bEnable);

	return ( DPTX_API_RETURN_SUCCESS );
}


bool Dpv14_Tx_API_Set_Audio_Sel( u32 ucData )
{
	dptx_dbg("Set Audio sel 0x%08x", (u32)ucData );

	Dptx_Reg_Set_AudioSel( (u32)ucData );

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Audio_Sel( u32 *pucData )
{
	u32 ucAudioSel=0;
	
	ucAudioSel = Dptx_Reg_Get_AudioSel();
	dptx_dbg("Get Audio sel 0x%08x", (u32)ucAudioSel );

	*pucData = ucAudioSel;

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_Configured_Dtd_Infor( DPTX_API_Dtd_Params_t *pstDptx_Dtd_Params, u8 ucStream_Index )
{
	struct Dptx_Params 			*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	memcpy( pstDptx_Dtd_Params, &pstHandle->stVideoParams.stDtdParams[ucStream_Index], sizeof(DPTX_API_Dtd_Params_t) );

	return (DPTX_API_RETURN_SUCCESS);
}

bool Dpv14_Tx_API_Get_Dtd_Infor_From_VideoCode( u32 uiVideo_Code, DPTX_API_Dtd_Params_t *pstDptx_Dtd_Params, u32 uiRefreshRate, u8 ucVideoFormat )
{
	bool						bRetVal;

	if( pstDptx_Dtd_Params == NULL )
	{
		dptx_err("Ptr. of Dtd params is NULL" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Avgen_Fill_Dtd( ( struct Dptx_Dtd_Params *)pstDptx_Dtd_Params, uiVideo_Code, uiRefreshRate, ucVideoFormat );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( DPTX_API_RETURN_FAIL );
	}

	return (DPTX_API_RETURN_SUCCESS);
}

bool Dpv14_Tx_API_Get_Dtd_Infor_From_Edid( DPTX_API_Dtd_Params_t *pstDptx_Dtd_Params, u8 ucStream_Index )
{
	bool						bRetVal, bHPDStatus;
	struct Dptx_Params 			*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		dptx_err( "from Dptx_Intr_Get_HotPlug_Status()" );
		return ( DPTX_API_RETURN_FAIL );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Handle_Edid( pstHandle, ucStream_Index );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( DPTX_API_RETURN_FAIL );
	}

	memcpy( pstDptx_Dtd_Params, &pstHandle->stVideoParams.stDtdParams[ucStream_Index], sizeof(DPTX_API_Dtd_Params_t) );

	return (DPTX_API_RETURN_SUCCESS);
}

bool Dpv14_Tx_API_Read_Edid( bool bI2C_Over_Aux, u8 ucStream_Index )
{
	bool				bRetVal, bHPDStatus;
	struct Dptx_Params	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		dptx_err( "from Dptx_Intr_Get_HotPlug_Status()" );
		return ( DPTX_API_RETURN_FAIL );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( DPTX_API_RETURN_FAIL );
	}
	
	if( bI2C_Over_Aux && ( ucStream_Index != DPTX_INPUT_STREAM_0 ))
	{
		dptx_err("I2C Over Aux is avaialbe for SST only... not for stream %d", ucStream_Index );
		return ( DPTX_API_RETURN_FAIL );
    }

	if( bI2C_Over_Aux )
	{
		bRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux( pstHandle );
		if( bRetVal == DPTX_API_RETURN_FAIL )
		{
			return ( DPTX_API_RETURN_FAIL );
		}
	}
	else
	{
		bRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg( pstHandle, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL )
		{
			return ( DPTX_API_RETURN_FAIL );
		}
	}

	return ( DPTX_API_RETURN_SUCCESS );
}

bool Dpv14_Tx_API_Get_HotPlug_Status( bool *pbHotPlug_Status )
{
	bool					bRetVal;
	struct Dptx_Params		*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err( "Failed to get handle" );
		return ( DPTX_API_RETURN_FAIL );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, pbHotPlug_Status );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		dptx_err( "from Dptx_Intr_Get_HotPlug_Status()" );
		return ( DPTX_API_RETURN_FAIL );
	}

	return ( bRetVal );
}


