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

#if defined(CONFIG_DRM_TCC)
#include <tcc_drm_dpi.h>
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
		dptx_err("DP %d attach callback is not registered", ucDP_Index );
		return ( -ENOMEM );
	}

	dptx_info("Calling attach() with DP %d to DRM", ucDP_Index);

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
		dptx_err("DP %d detach callback is not registered", ucDP_Index );
		return ( -ENOMEM );
	}

	dptx_info("Calling dettach() with DP %d to DRM", ucDP_Index);

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

	dptx_notice("DP %d is registered by DRM", ucDP_Index);

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
	uint8_t	ucHPD_State;
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

	Dptx_Intr_Get_HotPlug_Status(pstHandle, &ucHPD_State);
	if(ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_info("DP %d is not plugged", dp_id);

		*hpd_state = (unsigned char)0;
		return 0;
	}

	if( (u8)dp_id >= pstHandle->ucNumOfPorts )
	{
		dptx_info("DP %d is not plugged", dp_id);
		*hpd_state = (unsigned char)0;
	}
	else
	{
		dptx_info("DP %d is plugged", dp_id);
		*hpd_state = (unsigned char)1;
	}

	return 0;
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
		dptx_err("DP %d EDID data is not valid", dp_id );

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
	bool					bVStream_Enabled;
	struct Dptx_Params 	*pstHandle;
	struct Dptx_Dtd_Params	stDtd_Params;
	struct Dptx_Dtd_Params	stDtd_Params_Configured;

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

	Dptx_Avgen_Get_Video_Stream_Enable( pstHandle, &bVStream_Enabled, (u8)dp_id );
	if( bVStream_Enabled )
	{
		Dptx_Avgen_Get_Video_Configured_Timing( pstHandle, (u8)dp_id, &stDtd_Params_Configured );

		if(( stDtd_Params.interlaced			== stDtd_Params_Configured.interlaced ) && 
			( stDtd_Params.h_sync_polarity		== stDtd_Params_Configured.h_sync_polarity ) && 
			( stDtd_Params.h_active				== stDtd_Params_Configured.h_active ) && 
			( stDtd_Params.h_blanking			== stDtd_Params_Configured.h_blanking ) && 
			( stDtd_Params.h_sync_offset		== stDtd_Params_Configured.h_sync_offset ) && 
			( stDtd_Params.h_sync_pulse_width	== stDtd_Params_Configured.h_sync_pulse_width ) && 
			( stDtd_Params.v_sync_polarity		== stDtd_Params_Configured.v_sync_polarity ) && 
			( stDtd_Params.v_active				== stDtd_Params_Configured.v_active ) && 
			( stDtd_Params.v_blanking			== stDtd_Params_Configured.v_blanking ) && 
			( stDtd_Params.v_sync_offset		== stDtd_Params_Configured.v_sync_offset ) && 
			( stDtd_Params.v_sync_pulse_width	== stDtd_Params_Configured.v_sync_pulse_width ))
	{
				dptx_info("[Detailed timing from DRM] : Video timing of DP %d was already configured --> Skip", dp_id);
				dptx_info("		Pixel clk = %d ", (u32)dptx_detailed_timing->pixel_clock );
				dptx_info("		%s", ( dptx_detailed_timing->interlaced ) ? "Interlace" : "Progressive" );
				dptx_info("		H Active(%d), V Active(%d)", (u32)dptx_detailed_timing->h_active, (u32)dptx_detailed_timing->v_active );
				dptx_info("		H Blanking(%d), V Blanking(%d)", (u32)dptx_detailed_timing->h_blanking, (u32)dptx_detailed_timing->v_blanking);
				dptx_info("		H Sync offset(%d), V Sync offset(%d) ", (u32)dptx_detailed_timing->h_sync_offset, (u32)dptx_detailed_timing->v_sync_offset);
				dptx_info("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)dptx_detailed_timing->h_sync_pulse_width, (u32)dptx_detailed_timing->v_sync_pulse_width );
				dptx_info("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)dptx_detailed_timing->h_sync_polarity, (u32)dptx_detailed_timing->v_sync_polarity );

				return ( 0 );
		}
	}

	bRetVal = Dptx_Avgen_Set_Video_Detailed_Timing( pstHandle, (u8)dp_id, &stDtd_Params );
	if( bRetVal == DPTX_API_RETURN_FAIL ) 
	{
		return ( -ENODEV );
	}

	dptx_info("[Detailed timing from DRM] : Video timing of DP %d is being aconfigured ", dp_id);
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

	dptx_info("Set DP %d video %s...", dp_id, enable ? "enable":"disable" );

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

	dptx_info("Set DP %d audio %s...",	enable ? "enable":"disable" );

	Dptx_Avgen_Set_Audio_Mute(pstHandle, (u8)dp_id, enable);

	return ( 0 );
}

#endif

void Hpd_Intr_CallBabck( u8 ucDP_Index, bool bHPD_State )
{
	dptx_info("Callback called with DP %d, HPD %s", ucDP_Index, bHPD_State ? "Plugged":"Unplugged" );

#if defined( CONFIG_DRM_TCC )
	if( bHPD_State == (bool)HPD_STATUS_PLUGGED )
	{
		dpv14_api_attach_drm( ucDP_Index );
	}
	else
	{
		dpv14_api_detach_drm( ucDP_Index );
	}
#endif

}
EXPORT_SYMBOL( Hpd_Intr_CallBabck );


int32_t Dpv14_Tx_API_Get_HPD_State(uint8_t *pucHPD_State)
{
	int32_t iRetVal;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	if(pucHPD_State == NULL) {
		dptx_err("pucHPD_State == NULL" );
		return ENXIO;
	}

	iRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, pucHPD_State );
	if(iRetVal != 0) {
		return ENODEV;
	}

	return 0;
}

int32_t Dpv14_Tx_API_Get_Port_Composition( bool *pbMST_Supported, u8 *pucNumOfPluggedPorts)
{
	bool	bRetVal;
	bool    bSink_MST_Supported;
	int32_t iRetVal;
	uint8_t	ucHPD_State, ucNumOfPluggedPorts;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	iRetVal = Dptx_Intr_Get_HotPlug_Status(pstHandle, &ucHPD_State);
	if(iRetVal != 0) {
		return ENODEV;
	}
	if(ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged" );
		return ENXIO;
	}

	bRetVal = Dptx_Ext_Get_Sink_Stream_Capability( pstHandle, &bSink_MST_Supported );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ENODEV;
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
				return ENODEV;
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
	}

	if( ucNumOfPluggedPorts == 1 )
	{
		bSink_MST_Supported = false;

		Dptx_Ext_Set_Stream_Mode( pstHandle, false,     ucNumOfPluggedPorts );
	}
	else
	{
		Dptx_Ext_Set_Stream_Mode( pstHandle, true,     ucNumOfPluggedPorts );
	}

	*pbMST_Supported = bSink_MST_Supported;
	*pucNumOfPluggedPorts = ucNumOfPluggedPorts;

	return ( 0 );
}

int32_t Dpv14_Tx_API_Get_Edid( u8 ucStream_Index, u8 *pucEDID_Buf, u32 uiBuf_Size )
{
	bool				bRetVal;
	bool    bSink_MST_Supported;
	uint8_t ucHPD_State, ucNumOfPorts;
	int32_t iRetVal;
	struct Dptx_Params 	*pstHandle;
	
	if( pucEDID_Buf == NULL )
	{
		dptx_err("pucEDID_Buf is NULL");
		return ENXIO;
	}

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	iRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &ucHPD_State );
	if(iRetVal != 0) {
		return ENODEV;
	}
	if(ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged..");
		return ENODEV;
	}

	Dptx_Ext_Get_Stream_Mode( pstHandle, &bSink_MST_Supported, &ucNumOfPorts );

	if( ucNumOfPorts == 0 )
	{
		dptx_err("Get Port Composition first ");
		return ENXIO;
	}

	if( ucStream_Index >= ucNumOfPorts )
	{
		dptx_err("DP %d isn't plugged", ucStream_Index );
		return ENXIO;
	}

	if( bSink_MST_Supported )
	{
		bRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg( pstHandle, ucStream_Index, true );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			return ENODEV;
		}
	}
	else
	{
		bRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux( pstHandle );
		if( bRetVal == DPTX_API_RETURN_FAIL )
		{
			return ENODEV;
		}
	}

	if( uiBuf_Size < (int)DPTX_EDID_BUFLEN )
	{
		memcpy( pucEDID_Buf, pstHandle->pucEdidBuf, uiBuf_Size );
	}
	else
	{
		memcpy( pucEDID_Buf, pstHandle->pucEdidBuf, DPTX_EDID_BUFLEN );
	}

	return ( 0 );
}

int32_t Dpv14_Tx_API_Perform_LinkTraining( bool *pbLinkTraining_Status )
{
	bool				bRetVal;
	bool    bSink_MST_Supported, bTrainingState;
	uint8_t ucHPD_State, ucNumOfPorts;
	int32_t iRetVal;
	struct Dptx_Params	*pstHandle;

	*pbLinkTraining_Status = false;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	iRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &ucHPD_State );
	if(iRetVal != 0) {
		return ENODEV;
	}
	if(ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged..");
		return ENODEV;
	}

	Dptx_Ext_Get_Stream_Mode( pstHandle, &bSink_MST_Supported, &ucNumOfPorts );

	if( ucNumOfPorts == 0 )
	{
		dptx_err("Get Port Composition first ");
		return ENXIO;
	}

	bRetVal = Dptx_Link_Get_LinkTraining_Status( pstHandle, &bTrainingState );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ENODEV;
	}
	
	if( bTrainingState )
	{
		*pbLinkTraining_Status = true;
		
		return ( 0 );
	}

	bRetVal = Dptx_Link_Perform_BringUp( pstHandle, bSink_MST_Supported );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ENODEV;
	}

	bRetVal = Dptx_Link_Perform_Training(     pstHandle, pstHandle->ucMax_Rate, pstHandle->ucMax_Lanes );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ENODEV;
	}

	*pbLinkTraining_Status = true;

	if( bSink_MST_Supported  )
	{
		bRetVal = Dptx_Ext_Set_Topology_Configuration( pstHandle, pstHandle->ucNumOfPorts, pstHandle->bSideBand_MSG_Supported );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ENODEV;
		}
	}

	return 0;
}

int32_t Dpv14_Tx_API_Set_Video_Timing( u8 ucStream_Index, struct DPTX_API_Dtd_Params_t *dptx_detailed_timing )
{
	bool					bRetVal;
	bool					bMST_Supported;
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
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	bRetVal = Dptx_Ext_Get_Stream_Mode( pstHandle, &bMST_Supported, &ucNumOfPorts );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
		return ( -ENODEV );
		}

	if( ucNumOfPorts == 0 )
	{
		dptx_err("Get Port Composition first ");
		return ENXIO;
	}
	if( ucStream_Index >= ucNumOfPorts )
	{
		dptx_err("DP %d isn't plugged", ucStream_Index );
		return ENXIO;
	}

	memcpy( &stDtd_Params, dptx_detailed_timing, sizeof(struct Dptx_Dtd_Params) );
	
	bRetVal = Dptx_Avgen_Set_Video_Detailed_Timing( pstHandle, (u8)ucStream_Index, &stDtd_Params );
	if( bRetVal == DPTX_API_RETURN_FAIL )
	{
		return ENODEV;
	}

	return ( 0 );
}

int32_t Dpv14_Tx_API_Set_Video_Enable( u8 ucStream_Index, bool bEnable )
{
	bool				bRetVal;
 	uint8_t ucHPD_State;
	int32_t iRetVal;
	struct Dptx_Params 	*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	iRetVal = Dptx_Intr_Get_HotPlug_Status( pstHandle, &ucHPD_State );
	if(iRetVal != 0) {
		return ENODEV;
	}
	if(ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged..");
		return ENODEV;
	}

	if( bEnable )
	{
		bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle,      false, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			dptx_err("from Dptx_Avgen_Set_Video_Stream_Enable() " );
			return ENODEV;
		}
	}
	else
	{
		bRetVal = Dptx_Avgen_Set_Video_Stream_Enable( pstHandle,      true, ucStream_Index );
		if( bRetVal == DPTX_API_RETURN_FAIL ) 
		{
			dptx_err("from Dptx_Avgen_Set_Video_Stream_Enable() " );
			return ENODEV;
		}
	}

	dptx_dbg("Stream %d to %s", ucStream_Index, bMute ? "mute":"unmute"  );

	return 0;
}

int32_t Dpv14_Tx_API_Set_Audio_Configuration(uint8_t ucDP_Index, struct DPTX_API_Audio_Params *pstDptx_audio_params)
{
	struct Dptx_Params 		*pstHandle;
	struct Dptx_Audio_Params stAudioParams;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err("Failed to get handle" );
		return ENXIO;
	}

	if(pstDptx_audio_params == NULL) {
		dptx_err("pstDptx_audio_params == NULL" );
		return ENXIO;
}

	stAudioParams.ucInput_InterfaceType     = (uint8_t)pstDptx_audio_params->eIn_InterfaceType;
	stAudioParams.ucInput_DataWidth         = (uint8_t)pstDptx_audio_params->eIn_DataWidth;
	stAudioParams.ucInput_Max_NumOfchannels = (uint8_t)pstDptx_audio_params->eIn_Max_NumOfchannels;
	stAudioParams.ucInput_HBR_Mode          = (uint8_t)pstDptx_audio_params->eIn_HBR_Mode;
	stAudioParams.ucIEC_Sampling_Freq       = (uint8_t)pstDptx_audio_params->eIEC_Sampling_Freq;
	stAudioParams.ucIEC_OriginSamplingFreq  = (uint8_t)pstDptx_audio_params->eIEC_OrgSamplingFreq;
	stAudioParams.ucInput_TimestampVersion  = 0x12;

	Dptx_Avgen_Configure_Audio(pstHandle, ucDP_Index, &stAudioParams);

	return 0;
}

int32_t Dpv14_Tx_API_Get_Audio_Configuration(uint8_t ucDP_Index, struct DPTX_API_Audio_Params *pstDptx_audio_params)
{
	uint8_t  ucInfType, ucNumOfCh, ucDataWidth, ucHBRMode, ucSamplingFreq, ucOrgSamplingFreq;
	struct Dptx_Params			*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err( "Failed to get handle" );
		return ENXIO;
	}

	if(pstDptx_audio_params == NULL) {
		dptx_err("pstDptx_audio_params == NULL" );
		return ENXIO;
	}

	Dptx_Avgen_Get_Audio_Input_InterfaceType(pstHandle, ucDP_Index, &ucInfType);
	Dptx_Avgen_Get_Audio_DataWidth(pstHandle, ucDP_Index, &ucDataWidth);
	Dptx_Avgen_Get_Audio_MaxNumOfChannels(pstHandle, ucDP_Index, &ucNumOfCh);
	Dptx_Avgen_Get_Audio_HBR_En(pstHandle, ucDP_Index, &ucHBRMode);
	Dptx_Avgen_Get_Audio_SamplingFreq(pstHandle, &ucSamplingFreq, &ucOrgSamplingFreq);

	pstDptx_audio_params->eIn_InterfaceType        = (enum  DPTX_AUDIO_INTERFACE_TYPE)ucInfType;
	pstDptx_audio_params->eIn_DataWidth            = (enum  DPTX_AUDIO_DATA_WIDTH)ucDataWidth;
	pstDptx_audio_params->eIn_Max_NumOfchannels = (enum  DPTX_AUDIO_NUM_OF_CHANNELS)ucNumOfCh;
	pstDptx_audio_params->eIn_HBR_Mode             = (enum  DPTX_AUDIO_INTERFACE_TYPE)ucHBRMode;
	pstDptx_audio_params->eIEC_Sampling_Freq       = (enum  DPTX_AUDIO_IEC60958_3_SAMPLE_FREQ)ucSamplingFreq;
	pstDptx_audio_params->eIEC_OrgSamplingFreq     = (enum  DPTX_AUDIO_IEC60958_3_ORIGINAL_SAMPLE_FREQ)ucOrgSamplingFreq;

	return 0;
	}


int32_t Dpv14_Tx_API_Set_Audio_Mute(uint8_t ucDP_Index, uint8_t ucMute)
{
	struct Dptx_Params 		*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err( "Failed to get handle" );
		return ENXIO;
	}

	Dptx_Avgen_Set_Audio_Mute(pstHandle, ucDP_Index, ucMute);

	return 0;
}

int32_t Dpv14_Tx_API_Get_Audio_Mute(uint8_t ucDP_Index, uint8_t *pucMute)
{
	struct Dptx_Params 		*pstHandle;

	pstHandle = Dptx_Platform_Get_Device_Handle();
	if(pstHandle == NULL) {
		dptx_err( "Failed to get handle" );
		return ENXIO;
	}

	Dptx_Avgen_Get_Audio_Mute(pstHandle, ucDP_Index, pucMute);

	return 0;
}

