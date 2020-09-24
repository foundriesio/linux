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
	
    pstDev->bHPD_IRQ_Enable = false;

    schedule_work( &pstDev->stDPTx_HPD_Handler );

    return IRQ_HANDLED;

end_handler:
        return IRQ_NONE;
}
#endif

static void dptx_intr_handler_thread( struct work_struct *work )
{
	return;
}

static void dptx_intr_HPD_handler_thread( struct work_struct *work )
{
	return;
}

#if 0
bool Dptx_Intr_Init( struct Dptx_Params *pstDptx )
{
	INIT_WORK( &pstDptx->stDPTx_Intr_Handler, dptx_intr_handler_thread );
    INIT_WORK( &pstDptx->stDPTx_HPD_Handler, dptx_intr_HPD_handler_thread );

	dptx_intr_HPD_handler_thread
		/* Check GPIO HPD */
        dev->hotplug_irq = -1;
        if (gpio_is_valid(dev->hotplug_gpio)) {
                if(devm_gpio_request(dev->parent_dev, dev->hotplug_gpio, "hdmi_hotplug") < 0 ) {
                        pr_err("%s failed get gpio request\r\n", __func__);
                } else {
                        gpio_direction_input(dev->hotplug_gpio);
                        dev->hotplug_irq = gpio_to_irq(dev->hotplug_gpio);

                        if(dev->hotplug_irq < 0) {
                                pr_err("%s can not convert gpio to irq\r\n", __func__);
                                ret = -1;
                        } else {
                                pr_info("%s using gpio hotplug interrupt (%d)\r\n", __func__, dev->hotplug_irq);
                                dev->hotplug_status = dev->hotplug_real_status = gpio_get_value(dev->hotplug_gpio)?1:0;
                                /* Disable IRQ auto enable */
                                irq_set_status_flags(dev->hotplug_irq, IRQ_NOAUTOEN);
                                flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
                                ret = devm_request_irq(dev->parent_dev, dev->hotplug_irq,
                                        dwc_hdmi_tx_hpd_irq_handler, flag,
                                        "hpd_irq_handler", dev);

                                dwc_hdmi_tx_set_hotplug_interrupt(dev, 1);
                        }
                        if(ret < 0) {
                                pr_err("%s failed request interrupt for hotplug\r\n", __func__);
                        }
                }
        }
}
#endif

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
	int			retval;
	u32			uiHpdStatus;
	enum HPD_Detection_Status eHPDStatus;

	struct Dptx_Params *pstDptx = dev;

	mutex_lock( &pstDptx->Mutex );

	if( atomic_read( &pstDptx->HPD_IRQ_State )) 
	{
		atomic_set( &pstDptx->HPD_IRQ_State, 0 );

		uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );

		dptx_dbg("Read HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, uiHpdStatus );

		if( uiHpdStatus & DPTX_HPDSTS_STATUS )
		{
			eHPDStatus = HPD_STATUS_PLUGGED;
		}
		else
		{
			eHPDStatus = HPD_STATUS_UNPLUGGED;
		}
		
		if( pstDptx->eLast_HPDStatus == eHPDStatus )
		{
			dptx_dbg("HPD state is not changed as %s", ( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged");

			mutex_unlock( &pstDptx->Mutex );
			return ( IRQ_HANDLED );
		}
		else
		{
			pr_info("[INF][DP V14]HPD state is changed from %s to %s", ( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged", ( eHPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged");
		
			/*
			 * TODO this should be set after all AUX transactions that are
			 * queued are aborted. Currently we don't queue AUX and AUX is
			 * only started from this function.
			 */
			atomic_set( &pstDptx->stAuxParams.Abort, 0 );
			dptx_dbg("To do for pstDptx->stAuxParams.Abort");

			if( eHPDStatus == HPD_STATUS_PLUGGED )
			{
				Dptx_Intr_Handle_Hotplug( pstDptx );
			}
			else
			{
				Dptx_Intr_Handle_HotUnplug( pstDptx );
			}

			pstDptx->eLast_HPDStatus = eHPDStatus;
		}
	}

	if( atomic_read(&pstDptx->Sink_request) ) 
	{
		atomic_set( &pstDptx->Sink_request, 0 );
//		retval = handle_sink_request(dptx);
		if( retval)
		{
			dptx_err("Unable to handle sink request \n");
		}
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
		pr_info("[INF][DP V14]IRQ_NONE");
		return  (IRQ_NONE);
	}

	if( uiInterrupt_Status & ( DPTX_ISTS_AUX_REPLY | DPTX_ISTS_AUX_CMD_INVALID ))
	{
		dptx_dbg("Should not receive AUX interrupts( 0x%x )", uiInterrupt_Status);
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
			atomic_set( &pstDptx->stAuxParams.Abort, 1 );
			
			dptx_intr_notify( pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if( uiHpdStatus & DPTX_HPDSTS_HOT_UNPLUG )
		{
			dptx_dbg("DPTX_HPDSTS_HOT_UNPLUG");

			Dptx_Reg_Writel( pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_UNPLUG );

			atomic_set( &pstDptx->HPD_IRQ_State, 1 );
			atomic_set( &pstDptx->stAuxParams.Abort, 1 );
			
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

bool Dptx_Intr_Handle_Hotplug( struct Dptx_Params *pstDptx )
{
	bool		bRetVal;
	bool		bSink_SSC_Profiled;
	u8			ucStreamIndex;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;
	struct Dptx_Hdcp_Params		*hparams = &pstDptx->stHdcpParams;

	bRetVal = Dptx_Core_Set_PHY_NumOfLanes( pstDptx, (u8)pstDptx->ucMax_Lanes );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	/* 
	 * DP_SET_POWER[0600] - SET_POWER_STATE[Bit2:0]
	 *	01 = Local Siink and all downstream Sink devices to D0( normal operation mode )
	 *	02 = Local Siink and all downstream Sink devices to D3( power down mode )
	 *	05 = Main Link for local Siink and all downstream Sink devices to D3( power down mode ), Keep AUX block fully powered.
	*/
	bRetVal = Dptx_Aux_Write_DPCD( pstDptx, DP_SET_POWER, DP_SET_POWER_D0 ); /*  Dptx_Aux_Write_DPCD( pstDptx, 0x600, 1 ); */ 
	if( bRetVal ==  DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Get_Sink_SSC_Capability( pstDptx, &bSink_SSC_Profiled );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Set_PHY_SSC( pstDptx, bSink_SSC_Profiled );
	if( bRetVal ==  DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}
	
	bRetVal = Dptx_Ext_Set_Stream_Capability( pstDptx );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}
		
	if( pstDptx->stVideoParams.auiVideo_Code[PHY_INPUT_STREAM_0] == DPTX_VIC_READ_PANEL_EDID )
	{
		for( ucStreamIndex = 0; ucStreamIndex < pstDptx->ucNumOfStreams; ucStreamIndex++ )
		{
			bRetVal = Dptx_Intr_Handle_Edid( pstDptx, ucStreamIndex );
			if( bRetVal == DPTX_RETURN_FAIL )
			{
				return ( DPTX_RETURN_FAIL );
			}
		}
	}	
	
	/*
	 * DPCD_REV[00000]: 10h: DPCD 1.0, 11h: DPCD 1.1, 12h: DPCD 1.2, 14h: DPCD 1.4
	 * -.Minor Revision Number[Bit3:0] 
	 * -.Major Revision Number[Bit7:4]
	 */ 
	memset( pstDptx->aucDPCD_Caps, 0, DPTX_SINK_CAP_SIZE );

	bRetVal = Dptx_Aux_Read_Bytes_From_DPCD( pstDptx, DP_DPCD_REV, pstDptx->aucDPCD_Caps, DPTX_SINK_CAP_SIZE );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	/*
	 * TRAINING_AUX_RD_INTERVAL[0000E]
	 * -.TRAINING_AUX_RD_INTERVAL[Bit6:0]( 0: 400us, 1: 4ms, 2: 8ms, 3: 12ms, 4: 16ms )
	 * -.EXTENDED_RECEIVER_CAPABILITY_FEILD_PRESENT[Bit7]( 0: Not present , 1: Present at DPCD Address 02200h through 022FFh )
	 *
	 *  DPCD_REV[2200] DPCD data structure revision number
	 */ 
	if( pstDptx->aucDPCD_Caps[DP_TRAINING_AUX_RD_INTERVAL] & DP_EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT ) 
	{
		dptx_dbg("Sink has extended receiver capability... read from 0x2200" );

		bRetVal = Dptx_Aux_Read_Bytes_From_DPCD( pstDptx, 0x2200, pstDptx->aucDPCD_Caps, DPTX_SINK_CAP_SIZE );	
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
	}

	dptx_dbg("Sink DP Revision %x.%x ", (pstDptx->aucDPCD_Caps[0] & 0xF0) >> 4, pstDptx->aucDPCD_Caps[0] & 0xF );

/*	dptx_dbg("Read %d bytes DPCD", (u32)DPTX_SINK_CAP_SIZE );
	for( u16 usCount = 0; usCount < DPTX_SINK_CAP_SIZE; usCount++ )
	{
		dptx_dbg(" 0x%02x", pstDptx->aucDPCD_Caps[usCount]);

		if(( usCount != 0 ) && ( usCount % 8 ) == 0 )
		{
			dptx_dbg("\n");
			}
	} */

	bRetVal = Dptx_Link_Perform_Training( pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Get_RTL_Configuration_Parameters( pstDptx );
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	if( pstDptx->bMultStreamTransport ) 
	{
		bRetVal = Dptx_Ext_Get_Topology( pstDptx );			
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
	}

	if(  pstDptx->stVideoParams.auiVideo_Code[PHY_INPUT_STREAM_0] == DPTX_VIC_READ_PANEL_EDID ) 
	{
		dptx_dbg("Using video code by reading EDID... " );/* It needs to handle sideband message for reading EDID from each sink devices */

		for( ucStreamIndex = 0; ucStreamIndex < pstDptx->ucNumOfStreams; ucStreamIndex++ ) 
		{
			pstVideoParams->stDtdParams[ucStreamIndex].uiPixel_Clock = pstVideoParams->uiPeri_Pixel_Clock[ucStreamIndex];	
			
			bRetVal = Dptx_Avgen_Calculate_Video_Average_TU_Symbols(	pstDptx,	
																			pstDptx->stDptxLink.ucNumOfLanes, 
																			pstDptx->stDptxLink.ucLinkRate, 
																			pstVideoParams->ucBitPerComponent, 
																			pstVideoParams->ucPixel_Encoding, 
																			pstVideoParams->stDtdParams[ucStreamIndex].uiPixel_Clock,
																			ucStreamIndex );
			if( bRetVal == DPTX_RETURN_FAIL )
			{
				return ( DPTX_RETURN_FAIL );
			}

			bRetVal = Dptx_Avgen_Set_Video_Timing( pstDptx, ucStreamIndex );
			if( bRetVal == DPTX_RETURN_FAIL )
			{
				return ( DPTX_RETURN_FAIL );
			}
		}
	}
	else
	{
		for( ucStreamIndex = 0; ucStreamIndex < pstDptx->ucNumOfStreams; ucStreamIndex++ ) 
		{
			bRetVal = Dptx_Avgen_Set_Video_Code( pstDptx, pstVideoParams->auiVideo_Code[ucStreamIndex], (u8)ucStreamIndex );
			if( bRetVal == DPTX_RETURN_FAIL )
			{
				return ( DPTX_RETURN_FAIL );
			}
		}
	}

	Dptx_Avgen_Configure_Audio( pstDptx );

	if( hparams->ucHDCP13_Enable ) 
	{
		Dptx_Hdcp_Set_HDCP13_State( pstDptx, true );
	}

	if( hparams->ucHDCP22_Enable ) 
	{
		Dptx_Hdcp_Set_HDCP22_State( pstDptx, true );
	}

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Intr_Handle_HotUnplug( struct Dptx_Params *pstDptx )
{
	bool			bRetVal;

	pstDptx->bEstablish_Timing_Present = false;

#if 0	
	u32 			uiRegMap_DptxCctl, uiRegMap_PhyIFCtrl;

	uiRegMap_DptxCctl = Dptx_Reg_Readl( pstDptx, DPTX_CCTL );// Disable forward error correction
	uiRegMap_DptxCctl &= ~DPTX_CCTL_ENABLE_FEC;
	Dptx_Reg_Writel( pstDptx, DPTX_CCTL, uiRegMap_DptxCctl );

	mdelay( 100 );
#endif

	bRetVal = Dptx_Core_Disable_PHY_XMIT( pstDptx, pstDptx->ucMax_Lanes );
	if( bRetVal == DPTX_RETURN_FAIL ) 
	{
		 return ( DPTX_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_DOWN_PHY_CLOCK );
	if( bRetVal == DPTX_RETURN_FAIL ) 
	{
		 return ( DPTX_RETURN_FAIL );
	}

	bRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, pstDptx->ucMax_Lanes );
	if( bRetVal == DPTX_RETURN_FAIL ) 
	{
		 return ( DPTX_RETURN_FAIL );
	}

#if 0
	Dptx_Hdcp_Set_HDCP13_State( pstDptx, false );
    Dptx_Hdcp_Set_HDCP22_State( pstDptx, false );
#endif

	pstDptx->stDptxLink.bTraining_Done = false;
	pstDptx->eLast_HPDStatus = HPD_STATUS_UNPLUGGED;


	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Intr_Handle_Edid( struct Dptx_Params *pstDptx, u8 ucStream_Index )
{
	bool						bRetVal;
	u8							aucPreferred_VIC[EDID_SIZE_OF_DETAILED_DES];
	u8							ucPClock_MSB, ucPClock_LSB;
	u8							ucDesc_Inex;
	struct Dptx_Dtd_Params		stDTD;
	struct Dptx_Video_Params	*pstVideoParams = &pstDptx->stVideoParams;

	if( pstDptx->bMultStreamTransport  )
	{
		if( ucStream_Index >= PHY_INPUT_STREAM_MAX )
		{
			dptx_err("Invalid stream index( %d ) on MST", ucStream_Index);
			return ( DPTX_RETURN_FAIL );
		}
		
		bRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg( pstDptx, ucStream_Index );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
	}
	else
	{
		if( ucStream_Index >= PHY_INPUT_STREAM_1 )
		{
			dptx_err("Invalid stream index( %d ) on SST", ucStream_Index);
			return ( DPTX_RETURN_FAIL );
		}
		
		bRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux( pstDptx );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
	}

	bRetVal = Dptx_Edid_Check_Detailed_Timing_Descriptors( pstDptx );/* Check and parse Established Timings */
	if( bRetVal == DPTX_RETURN_FAIL )
	{
		return ( DPTX_RETURN_FAIL );
	}

	if( pstDptx->pucEdidBuf[126] > 0 )
	{
		bRetVal = Dptx_Edid_Parse_Audio_Data_Block( pstDptx );/* Configure audio params based on sinks EDID */
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
		
		bRetVal = Dptx_Edid_Config_Audio_BasedOn_EDID( pstDptx );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			return ( DPTX_RETURN_FAIL );
		}
	}

	if( pstDptx->bEstablish_Timing_Present ) 
	{
		bRetVal = Dptx_Avgen_Fill_DTD_BasedOn_EST_Timings( pstDptx, &stDTD );
		if( bRetVal == DPTX_RETURN_FAIL )
		{
			bRetVal = Dptx_Avgen_Fill_Dtd( &stDTD, (u8)DPTX_DEFAULT_VIDEO_CODE, pstVideoParams->uiRefresh_Rate, pstVideoParams->ucVideo_Format );
			if( bRetVal == DPTX_RETURN_FAIL )
			{
				return ( DPTX_RETURN_FAIL );
			}
		}
	}
	else 
	{
		bRetVal = Dptx_Edid_Verify_EDID( pstDptx );
		if( bRetVal == DPTX_RETURN_FAIL ) 
		{
			bRetVal = Dptx_Avgen_Fill_Dtd( &stDTD, (u8)DPTX_DEFAULT_VIDEO_CODE, pstVideoParams->uiRefresh_Rate, pstVideoParams->ucVideo_Format );
			if( bRetVal == DPTX_RETURN_FAIL ) 
			{
				return ( DPTX_RETURN_FAIL );
			}
		} 
		else 
		{
			for( ucDesc_Inex = 0; ucDesc_Inex < 4; ucDesc_Inex++ )
			{
				ucPClock_MSB = pstDptx->pucEdidBuf[(EDID_START_BIT_OF_1ST_DETAILED_DES + (ucDesc_Inex * EDID_SIZE_OF_DETAILED_DES) + 1)];
				ucPClock_LSB = pstDptx->pucEdidBuf[(EDID_START_BIT_OF_1ST_DETAILED_DES + (ucDesc_Inex * EDID_SIZE_OF_DETAILED_DES))];
				
				if(( ucPClock_MSB != 0 ) && ( ucPClock_LSB != 0 ))
				{
					memcpy( aucPreferred_VIC, pstDptx->pucEdidBuf + EDID_START_BIT_OF_1ST_DETAILED_DES + (ucDesc_Inex * EDID_SIZE_OF_DETAILED_DES), EDID_SIZE_OF_DETAILED_DES );
				}
			}

			bRetVal = Dptx_Avgen_Parse_Dtd( &stDTD, aucPreferred_VIC );
			if( bRetVal == DPTX_RETURN_FAIL ) 
			{
				bRetVal = Dptx_Avgen_Fill_Dtd( &stDTD, (u8)DPTX_DEFAULT_VIDEO_CODE, pstVideoParams->uiRefresh_Rate, pstVideoParams->ucVideo_Format );
				if( bRetVal ==  DPTX_RETURN_FAIL )
				{
					return ( DPTX_RETURN_FAIL );
				}
			}
		}
	}

	memcpy( &pstVideoParams->stDtdParams[ucStream_Index], &stDTD, sizeof( struct Dptx_Dtd_Params ) );

	return ( DPTX_RETURN_SUCCESS );
}


bool Dptx_Intr_Get_HPD_Status( struct Dptx_Params *pstDptx, enum HPD_Detection_Status *peHPD_Status )
{
	u32			uiHpdStatus, uiInterrupt_Status;

	/*
	 * GENERAL_INTERRUPT[0xD00]: This register notifies the software of any pending interrrupts.  Wrtied to 1 to clear the interrupt.
	 *							 If any of the register bits is asserted and the corresponding Interrupt Enable register bit is set then the core asserts the Interrupt output
	 *							 In MST mode, even though this is visible for each streams this should be accessed with respect to Stream-0 address only. 
	 * -.HDP_EVENT[Bit0]<Reset 0x0> This bit indicates that an HPD event is captured in the HPD status register.  To reset the bit, the SW needs to clear the status at the source in HPD Status register.
	 */

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
#if defined( HPD_CHECK_WITH_GPIO )	 
	*peHPD_Status = gpio_get( DP_HPD_GPIO_PORT );

	dptx_dbg(" HPD GPIO C14 = %s ... ", *peHPD_Status ? "High":"Low" );
#else	

	uiInterrupt_Status = Dptx_Reg_Readl( pstDptx, DPTX_ISTS );
	uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );

	dptx_dbg("Read GENERAL_INTERRUPT[0x%08x]: 0x%08x, HPD_STATUS[0x%08x]: 0x%08x", DPTX_ISTS, uiInterrupt_Status, DPTX_HPDSTS, uiHpdStatus );
	if( uiInterrupt_Status & DPTX_ISTS_HPD )
	{
		if(( uiHpdStatus & DPTX_HPDSTS_HOT_PLUG ) && ( uiHpdStatus & DPTX_HPDSTS_STATUS ))
		{
			dptx_dbg("HPD plugged... " );
			*peHPD_Status = HPD_STATUS_PLUGGED;
		}
		else
		{
			dptx_dbg("HPD unplugged... " );
			*peHPD_Status = HPD_STATUS_UNPLUGGED;
		}
	}
	else
	{
		dptx_dbg("HDP Intr isn't triggered... " );
		*peHPD_Status = HPD_STATUS_UNPLUGGED;
	} 
#endif	

	return ( DPTX_RETURN_SUCCESS );
}

bool Dptx_Intr_Get_HotPlug_Status( struct Dptx_Params *pstDptx, bool *pbHotPlug_Status )
{
	u32			uiHpdStatus;

	uiHpdStatus = Dptx_Reg_Readl( pstDptx, DPTX_HPDSTS );

	dptx_dbg("HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, uiHpdStatus );

	if( uiHpdStatus & DPTX_HPDSTS_STATUS ) 
	{
		*pbHotPlug_Status = (bool)HPD_STATUS_PLUGGED;
	}
	else
	{
		dptx_dbg("DP isn't connected... " );
		*pbHotPlug_Status = (bool)HPD_STATUS_UNPLUGGED;
	}

	return ( DPTX_RETURN_SUCCESS );
}


bool Dptx_Intr_Get_HDCP_Status( struct Dptx_Params *pstDptx, enum HDCP_Detection_Status *peHDCP_Status )
{
	u32				uiInterrupt_Status;

	uiInterrupt_Status = Dptx_Reg_Readl( pstDptx, DPTX_ISTS );
	if( uiInterrupt_Status & DPTX_ISTS_HDCP )
	{
		*peHDCP_Status = HDCP_STATUS_DETECTED;
	}
	else
	{
		*peHDCP_Status = HDCP_STATUS_NOT_DETECTED;
	}

	return ( DPTX_RETURN_SUCCESS );
}


