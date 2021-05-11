// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/delay.h>
//#include <linux/drm_dp_helper.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"


//#define HPD_CHECK_WITH_GPIO

#define EDID_START_BIT_OF_1ST_DETAILED_DES			54
#define EDID_SIZE_OF_DETAILED_DES					18

#if defined( HPD_CHECK_WITH_GPIO ) 
#include <linux/stddef.h>
#include <configs/tcc/tcc805x-reg.h>
#include <asm/telechips/gpio.h>

#define DP_HPD_GPIO_PORT		TCC_GPC(14)
#endif

#if 0
static irqreturn_t dptx_intr_HPD_IRQ_Handler( int iIRQ, void *pvDev_id )
{
	struct Dptx_Params *pstDev =   (struct Dptx_Params *)pvDev_id;

	if( pstDev == NULL ) 
	{
		dptx_err("IRQ Dev is NULL\r\n", __func__);
		goto end_handler;
	}
	
	    /* disable hpd irq */
	disable_irq_nosync( pstDev->uiHPD_IRQ );
	
	schedule_work( &pstDev->stDPTx_HPD_Handler );

	return IRQ_HANDLED;

end_handler:
	return IRQ_NONE;
}
#endif


static int dptx_intr_handle_drm_interface( struct Dptx_Params *pstDptx, bool bHotPlugged )
{
	bool bTrainingState;
	u8 ucDP_Index;
	int32_t iRetVal;

	if( bHotPlugged == (bool)HPD_STATUS_UNPLUGGED )
	{
		iRetVal = Dptx_Intr_Handle_HotUnplug( pstDptx );
		if(iRetVal !=  DPTX_RETURN_NO_ERROR )
		{
			return iRetVal;
		}

		for( ucDP_Index = 0; ucDP_Index < (u8)pstDptx->ucNumOfPorts; ucDP_Index++ )
		{
			if( pstDptx->pvHPD_Intr_CallBack == NULL )
			{
				dptx_err("HPD callback isn't registered");
				return DPTX_RETURN_ENODEV;
			}
			
			pstDptx->pvHPD_Intr_CallBack( ucDP_Index, (bool)HPD_STATUS_UNPLUGGED );
		}

		pstDptx->ucNumOfPorts = 0;

		return DPTX_RETURN_NO_ERROR;
	}
	
	iRetVal = Dptx_Intr_Get_Port_Composition( pstDptx, 1 );
	if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = (int32_t)Dptx_Link_Get_LinkTraining_Status( pstDptx, &bTrainingState );
	if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	if( !bTrainingState )
	{
		iRetVal = Dptx_Link_Perform_BringUp( pstDptx, pstDptx->bMultStreamTransport );
		if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = Dptx_Link_Perform_Training(     pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes );
		if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		if( pstDptx->bMultStreamTransport  )
		{
			iRetVal = Dptx_Ext_Set_Topology_Configuration( pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported );
			if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
				return iRetVal;
			}
		}
	}

	for( ucDP_Index = 0; ucDP_Index < pstDptx->ucNumOfPorts; ucDP_Index++ )
	{
		if( pstDptx->pvHPD_Intr_CallBack == NULL )
		{
			dptx_err("HPD callback isn't registered");
			return DPTX_RETURN_ENODEV;
		}
		
		pstDptx->pvHPD_Intr_CallBack( ucDP_Index, (bool)HPD_STATUS_PLUGGED );
	}

	return DPTX_RETURN_NO_ERROR;
}

static void dptx_intr_notify( struct Dptx_Params *pstDptx )
{
	wake_up_interruptible( &pstDptx->WaitQ );
}

static void dptx_intr_handle_hpd_irq( struct Dptx_Params *pstDptx )
{
	atomic_set( &pstDptx->Sink_request, 1 );
	dptx_intr_notify( pstDptx );
}

irqreturn_t Dptx_Intr_Threaded_IRQ( int irq, void *dev )
{
	u8			ucStream_Index;
	u32			uiHpdStatus;
	enum HPD_Detection_Status eHPDStatus;

	struct Dptx_Params *pstDptx = dev;

	mutex_lock( &pstDptx->Mutex );

	if( atomic_read( &pstDptx->HPD_IRQ_State )) 
	{
		atomic_set( &pstDptx->HPD_IRQ_State, 0 );

		uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );

		eHPDStatus = ( uiHpdStatus & DPTX_HPDSTS_STATUS ) ? HPD_STATUS_PLUGGED:HPD_STATUS_UNPLUGGED;

		if( pstDptx->eLast_HPDStatus == eHPDStatus )
		{
			dptx_info("HPD state is not changed as %s", ( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged");
		}
		else
		{
			dptx_info("HPD state is changed from %s to %s", ( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged", ( eHPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged");
		
			if( eHPDStatus == HPD_STATUS_PLUGGED )
			{
				if( pstDptx->bUsed_TCC_DRM_Interface )
				{
					dptx_intr_handle_drm_interface( pstDptx, (bool)HPD_STATUS_PLUGGED );
				}
				else
				{
					for( ucStream_Index = 0; ucStream_Index < pstDptx->ucNumOfStreams; ucStream_Index++ )
					{
						pstDptx->pvHPD_Intr_CallBack( ucStream_Index, (bool)HPD_STATUS_PLUGGED );
					}
				}
			}
			else
			{
				if( pstDptx->bUsed_TCC_DRM_Interface )
				{
					dptx_intr_handle_drm_interface( pstDptx, (bool)HPD_STATUS_UNPLUGGED );
				}
				else
				{
					Dptx_Intr_Handle_HotUnplug( pstDptx );

					for( ucStream_Index = 0; ucStream_Index < pstDptx->ucNumOfStreams; ucStream_Index++ )
					{
						pstDptx->pvHPD_Intr_CallBack( ucStream_Index, (bool)HPD_STATUS_UNPLUGGED );
					}
				}
			}

			pstDptx->eLast_HPDStatus = eHPDStatus;
		}
	}

	if( atomic_read(&pstDptx->Sink_request) ) 
	{
		atomic_set( &pstDptx->Sink_request, 0 );
/*		
		iRetVal = handle_sink_request(dptx);
		if( iRetVal)
		{
			dptx_err("Unable to handle sink request \n");
		}
*/		
	}

	dptx_dbg("DONE");
	dptx_dbg("=======================================\n\n");

	mutex_unlock( &pstDptx->Mutex );

	return ( IRQ_HANDLED );
}

irqreturn_t Dptx_Intr_IRQ( int irq, void *dev )
{
	irqreturn_t eRetVal = IRQ_HANDLED;
	u32			uiInterrupt_Status, uiHpdStatus, uiRegMap_InterEn;

	struct Dptx_Params *pstDptx = dev;

	uiInterrupt_Status = Dptx_Reg_Readl( pstDptx, DPTX_ISTS );
	uiRegMap_InterEn = Dptx_Reg_Readl( pstDptx, DPTX_IEN );
	uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );

	dptx_dbg("Read: GENERAL_INTERRUPT[0x%08x]: 0x%08x, INTERRUPT_EN[0x%08x]: 0x%08x, INTERRUPT_STATE[0x%08x]: 0x%08x", 
				DPTX_ISTS, uiInterrupt_Status, DPTX_IEN, uiRegMap_InterEn, DPTX_HPDSTS, uiHpdStatus );
	
	if( !( uiInterrupt_Status & DPTX_ISTS_ALL_INTR ) ) 
	{
		dptx_dbg("IRQ_NONE");
		return  (IRQ_NONE);
	}

	if( uiInterrupt_Status & ( DPTX_ISTS_AUX_REPLY | DPTX_ISTS_AUX_CMD_INVALID ))
	{
		//dptx_dbg("Should not receive AUX interrupts( 0x%x )", uiInterrupt_Status);
	}

	if( uiInterrupt_Status & DPTX_ISTS_HDCP )// Handle HDCP Intr
	{
		dptx_dbg("DPTX_ISTS_HDCP");
	}

	if( uiInterrupt_Status & DPTX_ISTS_SDP )// Handle SDP Intr
	{
		dptx_dbg("DPTX_ISTS_SDP");
	}

	if( uiInterrupt_Status & DPTX_ISTS_VIDEO_FIFO_OVERFLOW )
	{
		uiRegMap_InterEn = Dptx_Reg_Readl( pstDptx, DPTX_IEN );
		if( uiRegMap_InterEn & DPTX_IEN_VIDEO_FIFO_OVERFLOW )
		{
			dptx_warn("DPTX_ISTS_VIDEO_FIFO_OVERFLOW");
			Dptx_Reg_Writel( pstDptx, DPTX_ISTS, DPTX_ISTS_VIDEO_FIFO_OVERFLOW );
		}
	}

	if( uiInterrupt_Status & DPTX_ISTS_AUDIO_FIFO_OVERFLOW )
	{
		uiRegMap_InterEn = Dptx_Reg_Readl( pstDptx, DPTX_IEN );
		if( uiRegMap_InterEn & DPTX_IEN_AUDIO_FIFO_OVERFLOW )
		{
			dptx_warn("DPTX_ISTS_AUDIO_FIFO_OVERFLOW");
			Dptx_Reg_Writel( pstDptx, DPTX_ISTS, DPTX_ISTS_AUDIO_FIFO_OVERFLOW );
		}
	}

	if( uiInterrupt_Status & DPTX_ISTS_HPD )
	{
#ifdef CONFIG_HDCP_DWC_HLD
		/*
		 * == Workaround : Toggling CP_IRQ by software ==
		 * Sending CP_IRQ to ESM when processing HPD interrups.
		 */
		u32 uiHdcpCfg;
		if (Dptx_Reg_Readl( pstDptx, DPTX_HDCP_OBS ) | DPTX_HDCP22_BOOTED ) {
			uiHdcpCfg = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_CFG );
			Dptx_Reg_Writel( pstDptx, DPTX_HDCP_CFG, uiHdcpCfg | DPTX_CFG_CP_IRQ );
			Dptx_Reg_Writel( pstDptx, DPTX_HDCP_CFG, uiHdcpCfg & ~DPTX_CFG_CP_IRQ );
		}
#endif
		uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );

		dptx_dbg("Read GENERAL_INTERRUPT[0x%08x]: 0x%08x, HPD_STATUS[0x%08x]: 0x%08x", DPTX_ISTS, uiInterrupt_Status, DPTX_HPDSTS, uiHpdStatus );

		if( uiHpdStatus & DPTX_HPDSTS_IRQ )
		{
			dptx_dbg("DPTX_HPDSTS_IRQ");
			Dptx_Reg_Writel( pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_IRQ );

			dptx_intr_handle_hpd_irq( pstDptx );
			
			eRetVal = IRQ_WAKE_THREAD;
		}

		if( uiHpdStatus & DPTX_HPDSTS_HOT_PLUG )
		{
			dptx_dbg("DPTX_HPDSTS_HOT_PLUG");

			Dptx_Reg_Writel( pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_PLUG );

			atomic_set( &pstDptx->HPD_IRQ_State, 1 );
			
			dptx_intr_notify( pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if( uiHpdStatus & DPTX_HPDSTS_HOT_UNPLUG )
		{
			dptx_dbg("DPTX_HPDSTS_HOT_UNPLUG");

			Dptx_Reg_Writel( pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_UNPLUG );

			atomic_set( &pstDptx->HPD_IRQ_State, 1 );
			
			dptx_intr_notify( pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if( uiHpdStatus & 0x80 )
		{
			dptx_dbg("DPTX_HPDSTS[7] HOTPLUG DEBUG INTERRUPT");

			Dptx_Reg_Writel( pstDptx, DPTX_HPDSTS, ( 0x80 | DPTX_HPDSTS_HOT_UNPLUG ) );
			dptx_intr_notify( pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}
	}

	return eRetVal;
}

int32_t Dptx_Intr_Get_Port_Composition(struct Dptx_Params *pstDptx, uint8_t ucClear_PayloadID)
{
	uint8_t	ucMST_Supported = 0, ucNumOfPluggedPorts = 0, ucDP_Index;
	int32_t	iRetVal;

	iRetVal = Dptx_Ext_Get_Sink_Stream_Capability( pstDptx, &ucMST_Supported );
	if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
	if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
		pstDptx->bSideBand_MSG_Supported = false;
	}
	else {
		pstDptx->bSideBand_MSG_Supported = true;
	}

	if( pstDptx->bSideBand_MSG_Supported )
	{
		if( ucMST_Supported ) 
		{
			if( ucClear_PayloadID )
			{
				iRetVal = Dptx_Ext_Clear_SidebandMsg_PayloadID_Table( pstDptx );
				if(iRetVal != DPTX_RETURN_NO_ERROR)
				{
					dptx_err("Failed to clear payload id table");
				}
			}

			iRetVal = Dptx_Ext_Get_TopologyState( pstDptx, &ucNumOfPluggedPorts );
			if( iRetVal !=  DPTX_RETURN_NO_ERROR )
			{
				ucNumOfPluggedPorts = 1;
			}
		}
		else
		{
			ucNumOfPluggedPorts = 1;
		}

		if( ucNumOfPluggedPorts == 1 )
		{
			iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux( pstDptx );
			if( iRetVal !=  DPTX_RETURN_NO_ERROR )
			{
				memset( pstDptx->paucEdidBuf_Entry[ucDP_Index], 0,   DPTX_EDID_BUFLEN );
			}
		}
		else
		{
			for( ucDP_Index = 0; ucDP_Index < ucNumOfPluggedPorts; ucDP_Index++ )
			{
				iRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg( pstDptx, ucDP_Index );
				if( iRetVal != DPTX_RETURN_NO_ERROR )
				{
					memset( pstDptx->paucEdidBuf_Entry[ucDP_Index], 0,   DPTX_EDID_BUFLEN );
				}
				else
				{
					memcpy( pstDptx->paucEdidBuf_Entry[ucDP_Index], pstDptx->pucEdidBuf,	DPTX_EDID_BUFLEN );
				}
			}
		}
		dptx_notice("%d %s connected", ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ? "Ext. monitor is":"Ext. monitors are");
	}
	else
	{
		iRetVal = Dptx_Max968XX_Get_TopologyState( &ucNumOfPluggedPorts );
		if( iRetVal != DPTX_RETURN_NO_ERROR )
		{
			ucNumOfPluggedPorts = 1;
		}

		for( ucDP_Index = 0; ucDP_Index < ucNumOfPluggedPorts; ucDP_Index++ )
		{
			memset( pstDptx->paucEdidBuf_Entry[ucDP_Index], 0,	 DPTX_EDID_BUFLEN );
		}

		dptx_notice("%d %s connected", ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ? " panel is":" panels are");
	}

	pstDptx->ucNumOfPorts = ucNumOfPluggedPorts;
	pstDptx->bMultStreamTransport = ( ucNumOfPluggedPorts == 1 ) ? false:true;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Register_HPD_Callback( struct Dptx_Params *pstDptx, Dptx_HPD_Intr_Callback HPD_Intr_Callback )
{
	pstDptx->pvHPD_Intr_CallBack = HPD_Intr_Callback;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Handle_HotUnplug( struct Dptx_Params *pstDptx )
{
	int32_t	iRetVal;

	if( pstDptx->ucMax_Lanes >= PHY_LANE_MAX )
	{
		dptx_err("Invalid the number of lanes : %d isn't allocated ", pstDptx->ucMax_Lanes);
		return DPTX_RETURN_EINVAL;
	}

	pstDptx->eLast_HPDStatus = HPD_STATUS_UNPLUGGED;

	iRetVal = Dptx_Core_Disable_PHY_XMIT( pstDptx, pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_DOWN_PHY_CLOCK );
	if(iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, pstDptx->ucMax_Lanes );
	if(iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Get_HotPlug_Status(struct Dptx_Params *pstDptx, uint8_t *pucHotPlug_Status)
{
	u32			uiHpdStatus;

	/*
	 * HPD_STATUS[0xD08]: This register shows the HPD interrupt events, the status of HPD interface and timers.
	 *							 In MST mode, even though this is visible for each streams this should be accessed with respect to Stream-0 address only. 
	 * -.HPD_IRQT[Bit0]<Reset 0x0> IRQ from the HPD. This bit is set to 1 after a low pulse on HPD has been detected between 0.25 ~ 2ms
	 * -.HPD_HOT_PLUG[Bit1]<Reset 0x0> Hot plug detected.  This bit is asserted after the sinik plugged and asserts HPD for at least 100ms.
	 * -.HPD_HOT_UNPLUG[Bit2]<Reset 0x0> Unplug detected.  This bit is set to 1 after HPD is deasserted for at least 2ms.
	 * -.HPD_UUNPLUG_ERR[Bit3]<Reset 0x0> Unplug detected.  This bit is set to 1 after HPD is deasserted for less than 250 uSec.
	 * -.HPD_STATUS[Bit8]<Reset 0x0> ( 0: No connected, 1: Connected )
	 * -.HPD_STATE[Bit11:9]( 0: Sink is not connected, 1: HPD gets deasserted after being in the plugged state in ithis state for 2ms, 3: HPD is high and Sink is connected, 4: HPD is deasserted )
	 */

	if( pucHotPlug_Status == NULL )
	{
		dptx_err("pucHotPlug_Status == NULL" );
		return DPTX_RETURN_EINVAL;
	}
	
	uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );
	if( uiHpdStatus & DPTX_HPDSTS_STATUS )
	{
		dptx_dbg("Hot plugged -> HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, uiHpdStatus );
		*pucHotPlug_Status = (uint8_t)HPD_STATUS_PLUGGED;
	} 
	else 
	{
		dptx_dbg("Hot unplugged -> HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, uiHpdStatus );
		*pucHotPlug_Status = (uint8_t)HPD_STATUS_UNPLUGGED;
	}

	return DPTX_RETURN_NO_ERROR;
}


