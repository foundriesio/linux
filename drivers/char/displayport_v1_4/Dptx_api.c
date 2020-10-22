// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/delay.h>

#include <drm/drm_encoder.h>

#include "Dptx_api.h"
#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"
#include "dptx_drm.h"

#if defined( CONFIG_DRM_TCC )
#include <tcc_drm_dpi.h>

//#define ENABLE_DRM_INTRFACE_TEST

#if defined( ENABLE_DRM_INTRFACE_TEST )
#define RAW_DATA_DUMP
#define PRINT_RAW_DATA_BUF_SIZE				DPTX_EDID_BUFLEN
#endif
#endif

#define MAX_CHECK_HPD_NUM					200

#define REG_PRINT_BUF_SIZE					1024
#define VIDEO_REG_DUMP_START_OFFSET			0x300
#define VIDEO_REG_DUMP_SIZE					( 0x34 / 4 )

#define VCP_REG_DUMP_START_OFFSET			0x200
#define VCP_REG_DUMP_SIZE					( 0x30 / 4 )

#define VCP_DPCD_DUMP_SIZE					64


#if defined( CONFIG_DRM_TCC )
static int dpv14_api_get_hpd_state( int dp_id, unsigned char *hpd_state );
static int dpv14_api_get_edid( int dp_id, unsigned char *edid, int buf_length );
static int dpv14_api_set_video_timing( int dp_id, struct dptx_detailed_timing_t *dptx_detailed_timing );
static int dpv14_api_set_video_stream_enable( int dp_id, unsigned char enable );
static int dpv14_api_set_audio_stream_enable( int dp_id, unsigned char enable );

static struct dptx_drm_helper_funcs dptx_drm_ops = {
    .get_hpd_state = dpv14_api_get_hpd_state,
	.get_edid = dpv14_api_get_edid,
	.set_video = dpv14_api_set_video_timing,
	.set_enable_video = dpv14_api_set_video_stream_enable,
	.set_enable_audio = dpv14_api_set_audio_stream_enable,
};

struct Dptx_drm_context_t {
	bool				bRegistered;
	struct drm_encoder *pstDrm_encoder;
	struct tcc_drm_dp_callback_funcs sttcc_drm_dp_callbacks;
};


static struct Dptx_drm_context_t	stDptx_drm_context[PHY_INPUT_STREAM_MAX] = {0, };


#if defined( RAW_DATA_DUMP )
static void dptx_Print_U8_Buf( u8 *pucBuf, u32 uiStart_RegOffset, u32 uiLength  )
{
	int			iOffset;
	char		acStr[PRINT_RAW_DATA_BUF_SIZE];
	int			iNumOfWritten = 0;
	
	iNumOfWritten += snprintf( &acStr[iNumOfWritten], PRINT_RAW_DATA_BUF_SIZE - iNumOfWritten, "\n" );

	for( iOffset = 0; iOffset < uiLength; iOffset++ ) 
	{
		if( !( iOffset % 16 ) ) 
		{
			iNumOfWritten += snprintf( &acStr[iNumOfWritten], PRINT_RAW_DATA_BUF_SIZE - iNumOfWritten, "\n%02x:", ( uiStart_RegOffset + iOffset ));
			if( iNumOfWritten >= PRINT_RAW_DATA_BUF_SIZE )
			{
				break;
			}
		}

		iNumOfWritten += snprintf( &acStr[iNumOfWritten],  PRINT_RAW_DATA_BUF_SIZE - iNumOfWritten, " %02x", pucBuf[iOffset] );
		if( iNumOfWritten >= PRINT_RAW_DATA_BUF_SIZE )
		{
			break;
		}
	}

	printk("%s", acStr);
	return;
}
#endif

static int dpv14_api_attach_drm( u8 ucDP_Index )
{
	struct Dptx_drm_context_t	*pstDptx_drm_context = &stDptx_drm_context[ucDP_Index];
	struct drm_encoder			*pstDrm_encoder;

	if( !pstDptx_drm_context->bRegistered )
	{
		dptx_err("Port %d is not registered", ucDP_Index );
		return ( -ENOMEM );
	}

	if( pstDptx_drm_context->sttcc_drm_dp_callbacks.attach == NULL )
	{
		dptx_err("Port %d callback is not registered", ucDP_Index );
		return ( -ENOMEM );
	}

	pstDrm_encoder = pstDptx_drm_context->pstDrm_encoder;
	pstDptx_drm_context->sttcc_drm_dp_callbacks.attach( pstDrm_encoder, (int)ucDP_Index, (int)0 );

	return ( 0 );
}

static int dpv14_api_detach_drm( u8 ucDP_Index )
{
	struct Dptx_drm_context_t	*pstDptx_drm_context = &stDptx_drm_context[ucDP_Index];
	struct drm_encoder			*pstDrm_encoder;

	if( !pstDptx_drm_context->bRegistered )
	{
		dptx_err("Port %d is not registered", ucDP_Index );
		return ( -ENOMEM );
	}

	if( pstDptx_drm_context->sttcc_drm_dp_callbacks.detach == NULL )
	{
		dptx_err("Port %d callback is not registered", ucDP_Index );
		return ( -ENOMEM );
	}

	pstDrm_encoder = pstDptx_drm_context->pstDrm_encoder;
	pstDptx_drm_context->sttcc_drm_dp_callbacks.detach( pstDrm_encoder, (int)ucDP_Index, (int)0 );

	return ( 0 );
}


int tcc_dp_register_drm( struct drm_encoder *encoder, struct tcc_drm_dp_callback_funcs *callbacks )
{
	u8							ucDP_Index;
	struct Dptx_drm_context_t	*pstDptx_drm_context;
	struct Dptx_Params 			*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get DP handle" );
		return ( -ENODEV );
	}

	for( ucDP_Index = 0; ucDP_Index < PHY_INPUT_STREAM_MAX; ucDP_Index++ )
	{
		pstDptx_drm_context = &stDptx_drm_context[ucDP_Index];

		if( !pstDptx_drm_context->bRegistered )
		{
			pstDptx_drm_context->bRegistered = true;
			break;
		}
	}

	if( ucDP_Index == PHY_INPUT_STREAM_MAX )
	{
		dptx_err("Port index is reached to limit %d \n", ucDP_Index );
		return ( -1 );
	}

	if( encoder == NULL )
	{
		dptx_err("drm encoder ptr is NULL");
		return ( -ENODEV );
	}

	dptx_info("DP is registered by DRM");

	pstDptx_drm_context->pstDrm_encoder = encoder;
	pstDptx_drm_context->sttcc_drm_dp_callbacks.attach = callbacks->attach;
	pstDptx_drm_context->sttcc_drm_dp_callbacks.detach = callbacks->detach;

	callbacks->register_helper_funcs( pstDptx_drm_context->pstDrm_encoder, &dptx_drm_ops );

	return ( ucDP_Index );
}
EXPORT_SYMBOL( tcc_dp_register_drm );

int tcc_dp_unregister_drm( void )
{

	memset( &stDptx_drm_context[PHY_INPUT_STREAM_0], 0, sizeof(struct Dptx_drm_context_t) * PHY_INPUT_STREAM_MAX );

	return ( 0 );
}
EXPORT_SYMBOL( tcc_dp_unregister_drm );


int dpv14_api_get_hpd_state( int dp_id, unsigned char *hpd_state )
{
	struct Dptx_Params 	*pstHandle;

	if(( dp_id >= PHY_INPUT_STREAM_MAX ) || ( dp_id < PHY_INPUT_STREAM_0 ))
	{
		dptx_err("Invalid dp id as %d", dp_id );
		return ( -ENODEV );
	}

	if( hpd_state == NULL )
	{
		dptx_err("drm hpd buffer ptr is NULL");
		return ( -ENODEV );
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( -ENODEV );
	}

	if( (u8)dp_id >= pstHandle->ucNumOfPorts )
	{
		dptx_info("DP %d is not attached", dp_id);
		*hpd_state = (unsigned char)0;
	}
	else
	{
		dptx_info("DP %d is attached", dp_id);
		*hpd_state = (unsigned char)1;
	}
	
	return ( 0 );
}

int dpv14_api_get_edid( int dp_id, unsigned char *edid, int buf_length )
{
	bool				bRetVal;
	u8					*pucEDID_Buf;
	struct Dptx_Params 	*pstHandle;

	if(( dp_id >= PHY_INPUT_STREAM_MAX ) || ( dp_id < PHY_INPUT_STREAM_0 ))
	{
		dptx_err("Invalid dp id as %d", dp_id );
		return ( -ENODEV );
	}
	if( edid == NULL )
	{
		dptx_err("drm edid buffer ptr is NULL");
		return ( -ENODEV );
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( -ENODEV );
	}

	pucEDID_Buf = pstHandle->paucEdidBuf_Entry[dp_id];
	if( pucEDID_Buf == NULL )
	{
		dptx_err("DP %d EDID buffer is not available", dp_id );
		
		memset( edid, 0, buf_length );
		return ( -ENODEV );
	}

	bRetVal = Dptx_Edid_Verify_BaseBlk_Data( pucEDID_Buf );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		memset( edid, 0, buf_length );
		return ( -ENODEV );
	}

	if( buf_length < (int)DPTX_EDID_BUFLEN )
	{
		memcpy( edid, pucEDID_Buf, buf_length );
	}
	else
	{
		memcpy( edid, pucEDID_Buf, DPTX_EDID_BUFLEN );
	}

	return ( 0 );
}

int dpv14_api_set_video_timing( int dp_id, struct dptx_detailed_timing_t *dptx_detailed_timing )
{
	bool					bRetVal;
	bool					bHPDStatus, bMST_Supported;
	u8						ucNumOfPorts;
	struct Dptx_Params 	*pstHandle;
	struct Dptx_Dtd_Params	stDtd_Params;
	struct Dptx_Video_Params	*pstVideoParams;

	if(( dp_id >= PHY_INPUT_STREAM_MAX ) || ( dp_id < PHY_INPUT_STREAM_0 ))
	{
		dptx_err("Invalid dp id as %d", dp_id );
		return ( -ENODEV );
	}

	if( dptx_detailed_timing == NULL )
	{
		dptx_err("drm timing buffer ptr is NULL");
		return ( -ENODEV );
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( -ENODEV );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( -ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		return ( -ENODEV );
	}

	pstVideoParams = &pstHandle->stVideoParams;

	stDtd_Params.pixel_repetition_input = (u16)dptx_detailed_timing->pixel_repetition_input;
	stDtd_Params.interlaced			= dptx_detailed_timing->interlaced;
	stDtd_Params.h_active			= (u16)dptx_detailed_timing->h_active;
	stDtd_Params.h_blanking			= (u16)dptx_detailed_timing->h_blanking;
	stDtd_Params.h_sync_offset		= (u16)dptx_detailed_timing->h_sync_offset;
	stDtd_Params.h_sync_pulse_width	= (u16)dptx_detailed_timing->h_sync_pulse_width;
	stDtd_Params.h_sync_polarity	= (u16)dptx_detailed_timing->h_sync_polarity;
	stDtd_Params.v_active			= (u16)dptx_detailed_timing->v_active;
	stDtd_Params.v_blanking			= (u16)dptx_detailed_timing->v_blanking;
	stDtd_Params.v_sync_offset		= (u16)dptx_detailed_timing->v_sync_offset;
	stDtd_Params.v_sync_pulse_width	= (u16)dptx_detailed_timing->v_sync_pulse_width;
	stDtd_Params.v_sync_polarity	= (u16)dptx_detailed_timing->v_sync_polarity;
	stDtd_Params.uiPixel_Clock		= dptx_detailed_timing->pixel_clock;
	stDtd_Params.h_image_size		= 16;
	stDtd_Params.v_image_size		= 9;

	bRetVal = Dptx_Ext_Get_Stream_Mode( pstHandle, &bMST_Supported, &ucNumOfPorts );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( -ENODEV );
	}

	if( bMST_Supported == (bool)DPTX_STREAM_CAP_SST )
	{
		dptx_info("Set video timing on SST mode..." );

		if( dp_id != PHY_INPUT_STREAM_0 )
		{
			dptx_err("dp id isn't 0 on SST mode");
			return ( -ENODEV );
		}
	}
	else
	{
		dptx_info("Set video timing on MST mode..." );
	}

	bRetVal = Dptx_Avgen_Set_Video_Detailed_Timing( pstHandle, (u8)dp_id, &stDtd_Params );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( -ENODEV );
	}
	
	dptx_info("\n[Dptx_Avgen_Get_VIC_From_Dtd] : ");
	dptx_info("		Pixel clk = %d ", (u32)dptx_detailed_timing->pixel_clock );
	dptx_info("		%s", ( dptx_detailed_timing->interlaced ) ? "Interlace" : "Progressive" );
	dptx_info("		H Active(%d), V Active(%d)", (u32)dptx_detailed_timing->h_active, (u32)dptx_detailed_timing->v_active );
	dptx_info("		H Blanking(%d), V Blanking(%d)", (u32)dptx_detailed_timing->h_blanking, (u32)dptx_detailed_timing->v_blanking);
	dptx_info("		H Sync offset(%d), V Sync offset(%d) ", (u32)dptx_detailed_timing->h_sync_offset, (u32)dptx_detailed_timing->v_sync_offset);
	dptx_info("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)dptx_detailed_timing->h_sync_pulse_width, (u32)dptx_detailed_timing->v_sync_pulse_width );
	dptx_info("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)dptx_detailed_timing->h_sync_polarity, (u32)dptx_detailed_timing->v_sync_polarity );
	
	return ( 0 );
}

int dpv14_api_set_video_stream_enable( int dp_id, unsigned char enable )
{
	bool				bRetVal;
	bool				bHPDStatus;
	struct Dptx_Params 	*pstHandle;

	if(( dp_id >= PHY_INPUT_STREAM_MAX ) || ( dp_id < PHY_INPUT_STREAM_0 ))
	{
		dptx_err("Invalid dp id as %d", dp_id );
		return ( -ENODEV );
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( -ENODEV );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( -ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("HPD is unplugged" );
		return ( -ENODEV );
	}

	dptx_info("Set video timing on MST mode..." );

	bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle, (bool)enable, (u8)dp_id );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( -ENODEV );
	}

	return ( 0 );
}

int dpv14_api_set_audio_stream_enable( int dp_id, unsigned char enable )
{
	bool				bRetVal;
	bool				bHPDStatus;
	struct Dptx_Params 	*pstHandle;

	if(( dp_id >= PHY_INPUT_STREAM_MAX ) || ( dp_id < PHY_INPUT_STREAM_0 ))
	{
		dptx_err("Invalid dp id as %d", dp_id );
		return ( -ENODEV );
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( -ENODEV );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( -ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("HPD is unplugged" );
		return ( -ENODEV );
	}

	Dptx_Avgen_Set_Audio_Stream_Enable( pstHandle, (u8)dp_id, (bool)enable );
	
	return ( 0 );
}

#endif

void Dpv14_Tx_API_Hpd_Intr_CB( u8 ucDP_Index, bool bHPD_State )
{

	dptx_info("Callback called with DP %d, HPD %s", ucDP_Index, bHPD_State ? "Plugged":"Unplugged" );

#if defined( CONFIG_DRM_TCC )
	if( bHPD_State == (bool)HPD_STATUS_PLUGGED )
	{
#if defined( ENABLE_DRM_INTRFACE_TEST )
		u8		ucHDP_State;
		u8		aucEDID_Data[DPTX_EDID_BUFLEN];
		struct dptx_detailed_timing_t 		stDptx_detailed_timing;
		struct Dptx_Params			*pstHandle;
		struct Dptx_Video_Params	*pstVideoParams;
		struct Dptx_Dtd_Params		stDtd;
			
		pstHandle = Dptx_Platform_Get_Device_Handle();
		if( !pstHandle )
		{
			dptx_err("Failed to get DP handle" );
			return ( -ENODEV );
		}

		dpv14_api_get_hpd_state( ucDP_Index, &ucHDP_State );
		dptx_info("DP %d: HPD %s from dpv14_api_get_hpd_state()", ucDP_Index, bHPD_State ? "Plugged":"Unplugged" );

		dpv14_api_get_edid( ucDP_Index, aucEDID_Data, (int)DPTX_EDID_BUFLEN);
		dptx_Print_U8_Buf( aucEDID_Data, 0, (u32)DPTX_EDID_BUFLEN );

		pstVideoParams = &pstHandle->stVideoParams;
		Dptx_Avgen_Fill_Dtd( &stDtd, pstVideoParams->auiVideo_Code[ucDP_Index], (u32)VIDEO_REFRESHRATE_60_00HZ, (u8)VIDEO_FORMAT_CEA_861 );

		stDptx_detailed_timing.interlaced = stDtd.interlaced;
		stDptx_detailed_timing.pixel_repetition_input = stDtd.pixel_repetition_input;
		stDptx_detailed_timing.h_active = stDtd.h_active;
		stDptx_detailed_timing.v_active = stDtd.v_active;
		stDptx_detailed_timing.h_blanking = stDtd.h_blanking;
		stDptx_detailed_timing.v_blanking = stDtd.v_blanking;
		stDptx_detailed_timing.h_sync_offset = stDtd.h_sync_offset;
		stDptx_detailed_timing.v_sync_offset = stDtd.v_sync_offset;
		stDptx_detailed_timing.h_sync_pulse_width = stDtd.h_sync_pulse_width;
		stDptx_detailed_timing.v_sync_pulse_width = stDtd.v_sync_pulse_width;
		stDptx_detailed_timing.h_sync_polarity = stDtd.h_sync_polarity;
		stDptx_detailed_timing.v_sync_polarity = stDtd.v_sync_polarity;
		stDptx_detailed_timing.pixel_clock = pstVideoParams->uiPeri_Pixel_Clock[ucDP_Index];

		dpv14_api_set_video_timing( ucDP_Index, &stDptx_detailed_timing );

		mdelay( 3000 );

		dptx_info("Delay 3 Sec. before enabling video", ucDP_Index, bHPD_State ? "Plugged":"Unplugged" );

		dpv14_api_set_video_stream_enable( ucDP_Index, (u8)1 );
#else
		dpv14_api_attach_drm( ucDP_Index );
#endif
	}
	else
	{
		dpv14_api_detach_drm( ucDP_Index );
	}
#else
	if( bHPD_State == (bool)HPD_STATUS_PLUGGED )
	{
		dptx_info("DP %d: HPD Plugged");
	}
	else
	{
		dptx_info("DP %d: HPD Unplugged");
	}
#endif

}
EXPORT_SYMBOL( Dpv14_Tx_API_Hpd_Intr_CB );


int Dpv14_Tx_API_Get_HPD_State( bool *pbHPD_State )
{
	bool				bRetVal;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( EACCES );
	}
	
	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, pbHPD_State );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( ENODEV );
	}
	
	return ( 0 );
}

int Dpv14_Tx_API_Get_Port_Composition( bool *pbHPD_State )
{
	bool	bRetVal;
	bool				bHPD_State, bSink_MST_Supported;
	u8					ucNumOfPluggedPorts;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( EACCES );
	}
	
	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPD_State );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( ENODEV );
	}
	if( bHPD_State == (bool)HPD_STATUS_UNPLUGGED )
	{
		dptx_err("Hot unplugged" );
		return ( EACCES );
	}

	bRetVal = Dptx_Ext_Get_Sink_Stream_Capability( pstHandle, &bSink_MST_Supported );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( ENODEV );
	}

	if( bSink_MST_Supported )
	{
		bRetVal = Dptx_Ext_Get_TopologyState( pstHandle, &ucNumOfPluggedPorts );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			bRetVal = Dptx_Max968XX_Get_TopologyState( &ucNumOfPluggedPorts );
			if( bRetVal == DPTX_RETURN_FAIL )
			{
				dptx_err("There is no sink devices connected.. %d", ucNumOfPluggedPorts);
				return ( ENODEV );
			}
			
			pstHandle->bSideBand_MSG_Supported = false;
		}
		else
		{
			pstHandle->bSideBand_MSG_Supported = true;
		}
	}
	else
	{
		ucNumOfPluggedPorts = 1;
		dptx_info("1 %s is connected", ucNumOfPluggedPorts ? "SerDes":"Ext. monitor");
	}

	if( ucNumOfPluggedPorts == 1 )
	{
		Dptx_Ext_Set_Stream_Mode( pstHandle, false,     ucNumOfPluggedPorts );
	}
	else
	{
		Dptx_Ext_Set_Stream_Mode( pstHandle, true,     ucNumOfPluggedPorts );
	}
	
	return ( 0 );
}

int Dpv14_Tx_API_Get_Edid( u8 ucStream_Index )
{
	bool				bRetVal;
	bool				bHPDStatus, bSink_MST_Supported;
	u8					ucNumOfPorts;
	struct Dptx_Params 	*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( EACCES );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( ENODEV );
	}

	Dptx_Ext_Get_Stream_Mode( pstHandle, &bSink_MST_Supported, &ucNumOfPorts );

	if( ucNumOfPorts == 0 )
	{
		dptx_err("Get Port Composition first ");
		return ( EACCES );
	}

	if( ucStream_Index >= ucNumOfPorts )
	{
		dptx_err("DP %d isn't plugged", ucStream_Index );
		return ( EACCES );
	}

	if( bSink_MST_Supported )
	{
		bRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg( pstHandle, ucStream_Index, false );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			return ( ENODEV );
		}
	}
	else
	{
		bRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux( pstHandle );
		if( bRetVal == DPTX_API_RETURN_FAIL )
		{
			return ( ENODEV );
		}
	}

	return ( 0 );
}

int Dpv14_Tx_API_Perform_LinkTraining( u8 ucStream_Index )
{
	bool				bRetVal;
	bool				bHPDStatus, bSink_MST_Supported, bTrainingState;
	u8					ucNumOfPorts;
	struct Dptx_Params	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( EACCES );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( ENODEV );
	}

	Dptx_Ext_Get_Stream_Mode( pstHandle, &bSink_MST_Supported, &ucNumOfPorts );

	if( ucNumOfPorts == 0 )
	{
		dptx_err("Get Port Composition first ");
		return ( EACCES );
	}

	if( ucStream_Index >= ucNumOfPorts )
	{
		dptx_err("DP %d isn't plugged", ucStream_Index );
		return ( EACCES );
	}

	bRetVal = Dptx_Link_Get_LinkTraining_Status( pstHandle, &bTrainingState );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( ENODEV );
	}
	if( bTrainingState )
	{
		return ( 0 );
	}

	bRetVal = Dptx_Link_Perform_BringUp( pstHandle, bSink_MST_Supported );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( ENODEV );
	}

	bRetVal = Dptx_Link_Perform_Training(     pstHandle, pstHandle->ucMax_Rate, pstHandle->ucMax_Lanes );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( ENODEV );
	}

	if( bSink_MST_Supported  )
	{
		bRetVal = Dptx_Ext_Set_Topology_Configuration( pstHandle, pstHandle->ucNumOfPorts, pstHandle->bSideBand_MSG_Supported );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( ENODEV );
		}
	}

	return ( 0 );
}


int Dpv14_Tx_API_Set_Video_Timing( u8 ucStream_Index, struct DPTX_API_Dtd_Params_t *dptx_detailed_timing )
{
	bool					bRetVal;
	bool					bHPDStatus, bMST_Supported;
	u8						ucNumOfPorts;
	struct Dptx_Params 		*pstHandle;
	struct Dptx_Dtd_Params	stDtd_Params;

	if(( ucStream_Index >= PHY_INPUT_STREAM_MAX ) || ( ucStream_Index < PHY_INPUT_STREAM_0 ))
	{
		dptx_err("Invalid dp id as %d", ucStream_Index );
		return ( -ENODEV );
	}

	if( dptx_detailed_timing == NULL )
	{
		dptx_err("drm timing buffer ptr is NULL");
		return ( -ENODEV );
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( -ENODEV );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( -ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		return ( -ENODEV );
	}

	bRetVal = Dptx_Ext_Get_Stream_Mode( pstHandle, &bMST_Supported, &ucNumOfPorts );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
		return ( -ENODEV );
		}

	if( ucNumOfPorts == 0 )
	{
		dptx_err("Get Port Composition first ");
		return ( EACCES );
	}
	if( ucStream_Index >= ucNumOfPorts )
	{
		dptx_err("DP %d isn't plugged", ucStream_Index );
		return ( EACCES );
	}

	memcpy( &stDtd_Params, dptx_detailed_timing, sizeof(struct Dptx_Dtd_Params) );
	
	bRetVal = Dptx_Avgen_Set_Video_Detailed_Timing( pstHandle, (u8)ucStream_Index, &stDtd_Params );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ( ENODEV );
	}

	return ( 0 );
}

int Dpv14_Tx_API_Set_Video_Enable( u8 ucStream_Index, bool bEnable )
{
	bool				bRetVal;
	bool				bHPDStatus;
	struct Dptx_Params 	*pstHandle;
	
	pstHandle = Dptx_Platform_Get_Device_Handle();
	if( !pstHandle )
	{
		dptx_err("Failed to get handle" );
		return ( EACCES );
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &bHPDStatus );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( ENODEV );
	}
	if( bHPDStatus == (bool)HPD_STATUS_UNPLUGGED ) 
	{
		dptx_err("Hot unplugged..");
		return ( ENODEV );
	}

	if( bEnable )
	{
		bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle,      false, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			dptx_err("from Dptx_Avgen_Set_Video_Stream_Enable() " );
			return ( ENODEV );
		}
	}
	else
	{
		bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle,      true, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			dptx_err("from Dptx_Avgen_Set_Video_Stream_Enable() " );
			return ( ENODEV );
		}
	}

	dptx_dbg("Stream %d to %s", ucStream_Index, bMute ? "mute":"unmute"  );

	return ( 0 );
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


