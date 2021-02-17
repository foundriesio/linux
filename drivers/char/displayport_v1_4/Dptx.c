/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

//#include <linux/of_dma.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"


extern void Dpv14_Tx_API_Hpd_Intr_CB( u8 ucDP_Index, bool bHPD_State );

static bool of_parse_dp_dt( struct Dptx_Params	*pstDptx, struct device_node *pstOfNode, u32 *puiPeri0_PClk, u32 *puiPeri1_PClk, u32 *puiPeri2_PClk, u32 *puiPeri3_PClk )
{
	int				iRetVal;
	u32				uiSerDes_Reset_STR, uiVCP_Id;
	unsigned long	ulDispX_Peri_Clk;
	struct clk		*DISP0_Peri_Clk, *DISP1_Peri_Clk, *DISP2_Peri_Clk, *DISP3_Peri_Clk;
	
	DISP0_Peri_Clk = of_clk_get_by_name( pstOfNode, "disp0-clk" );
	if( !DISP0_Peri_Clk ) 
	{
		dptx_err("Can't find DISP0 Peri clock \n");
	}

	DISP1_Peri_Clk = of_clk_get_by_name( pstOfNode, "disp1-clk" );
	if( !DISP1_Peri_Clk ) 
	{
		dptx_err("Can't find DISP1 Peri clock \n");
	}

	DISP2_Peri_Clk = of_clk_get_by_name( pstOfNode, "disp2-clk" );
	if( !DISP0_Peri_Clk ) 
	{
		dptx_err("Can't find DISP2 Peri clock \n");
	}

	DISP3_Peri_Clk = of_clk_get_by_name( pstOfNode, "disp3-clk" );
	if( !DISP3_Peri_Clk ) 
	{
		dptx_err("Can't find DISP3 Peri clock \n");
	}

	ulDispX_Peri_Clk = clk_get_rate( DISP0_Peri_Clk );
	*puiPeri0_PClk = ( ulDispX_Peri_Clk / 1000 );

	ulDispX_Peri_Clk = clk_get_rate( DISP1_Peri_Clk );
	*puiPeri1_PClk = ( ulDispX_Peri_Clk / 1000 );

	ulDispX_Peri_Clk = clk_get_rate( DISP2_Peri_Clk );
	*puiPeri2_PClk = ( ulDispX_Peri_Clk / 1000 );

	ulDispX_Peri_Clk = clk_get_rate( DISP3_Peri_Clk );
	*puiPeri3_PClk = ( ulDispX_Peri_Clk / 1000 );

	iRetVal = of_property_read_u32( pstOfNode, "serdes_reset_str", &uiSerDes_Reset_STR );
	if( iRetVal < 0)
	{
		dptx_err("Can't get SerDes reset option on STR.. set to 'On' by default");
		uiSerDes_Reset_STR = (u32)1;
	}
	pstDptx->bSerDes_Reset_STR = (bool)uiSerDes_Reset_STR;

	iRetVal = of_property_read_u32( pstOfNode, "vcp_id_1st_sink", &uiVCP_Id );
	if( iRetVal < 0)
	{
		dptx_err("Can't get 1st Sink VCP Id.. set to '1' by default");
		uiVCP_Id = (u32)1;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_0] = uiVCP_Id;

	iRetVal = of_property_read_u32( pstOfNode, "vcp_id_2nd_sink", &uiVCP_Id );
	if( iRetVal < 0)
	{
		dptx_err("Can't get 2nd Sink VCP Id.. set to '2' by default");
		uiVCP_Id = (u32)2;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_1] = uiVCP_Id;

	iRetVal = of_property_read_u32( pstOfNode, "vcp_id_3rd_sink", &uiVCP_Id );
	if( iRetVal < 0)
	{
		dptx_err("Can't get 3rd Sink VCP Id.. set to '3' by default");
		uiVCP_Id = (u32)3;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_2] = uiVCP_Id;

	iRetVal = of_property_read_u32( pstOfNode, "vcp_id_4th_sink", &uiVCP_Id );
	if( iRetVal < 0)
	{
		dptx_err("Can't get 4th Sink VCP Id.. set to '4' by default");
		uiVCP_Id = (u32)4;
	}
	pstDptx->aucVCP_Id[PHY_INPUT_STREAM_3] = uiVCP_Id;

	return ( 0 );
}

static int Dpv14_Tx_Probe( struct platform_device *pdev)
{
	bool					bRetVal = 0;
	bool					bHotPlugged = false;
	u32						auiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX] = { 0, };
	struct resource			*pstResource;
	struct Dptx_Params		*pstDptx;
	struct Dptx_Video_Params	*pstVideoParams;

	dptx_dbg("");
	dptx_dbg("****************************************");
	dptx_dbg("DP V1.4 driver ");
	dptx_dbg("****************************************");

	if( pdev->dev.of_node == NULL ) 
	{
		dptx_err("Node wasn't found");
		return -ENODEV;
	}

	pstDptx = devm_kzalloc( &pdev->dev, sizeof(*pstDptx), GFP_KERNEL );
	if( pstDptx == NULL )
	{
		dptx_err("Could not allocated DP V1.4 TX Device");
		return -ENOMEM;
	}

	memset(pstDptx, 0, sizeof(struct Dptx_Params));

	pstDptx->pstParentDev = &pdev->dev;
	pstDptx->pcDevice_Name = "DPTx_V14";

	pstResource = platform_get_resource( pdev, IORESOURCE_MEM, 0 );
	if( !pstResource ) 
	{
		dptx_err("[%s:%d]No memory resource\n", __func__, __LINE__);
		return -ENODEV; 
	}

	pstDptx->pioDPLink_BaseAddr = devm_ioremap( &pdev->dev, pstResource->start, pstResource->end - pstResource->start );
	if( pstDptx->pioDPLink_BaseAddr == NULL ) 
	{
		dptx_err("Failed to map memory resource\n");
		return -ENODEV;
	}

	pstResource = platform_get_resource( pdev, IORESOURCE_MEM, 1 );
	if( !pstResource ) 
	{
		dptx_err("[%s:%d]No memory resource\n", __func__, __LINE__);
		return -ENODEV; 
	}

	pstDptx->pioMIC_SubSystem_BaseAddr = devm_ioremap( &pdev->dev, pstResource->start, pstResource->end - pstResource->start );
	if( pstDptx->pioMIC_SubSystem_BaseAddr == NULL ) 
	{
		dptx_err("Failed to map memory resource\n");
		return -ENODEV;
	}

	pstResource = platform_get_resource( pdev, IORESOURCE_MEM, 2 );
	if( !pstResource ) 
	{
		dptx_err("[%s:%d]No memory resource\n", __func__, __LINE__);
		return ( -ENODEV );
	}

	pstDptx->pioPMU_BaseAddr = devm_ioremap( &pdev->dev, pstResource->start, pstResource->end - pstResource->start );
	if( pstDptx->pioPMU_BaseAddr == NULL ) 
	{
		dptx_err("Failed to map memory resource\n");
		return -ENODEV;
	}

	pstDptx->uiHPD_IRQ = platform_get_irq( pdev, 0 );
	if( pstDptx->uiHPD_IRQ < 0) 
	{
		dptx_err("No IRQ\n");
		return -ENODEV;
	}

#ifdef CONFIG_OF
	of_parse_dp_dt( pstDptx, pdev->dev.of_node, &auiPeri_Pixel_Clock[PHY_INPUT_STREAM_0], &auiPeri_Pixel_Clock[PHY_INPUT_STREAM_1], &auiPeri_Pixel_Clock[PHY_INPUT_STREAM_2], &auiPeri_Pixel_Clock[PHY_INPUT_STREAM_3] );
#endif

	platform_set_drvdata( pdev, pstDptx );

	Dptx_Platform_Set_Device_Handle( pstDptx );

	bRetVal = Dptx_Platform_Init_Params( pstDptx, &pdev->dev );
	if( bRetVal )
	{
		dptx_err("from Dptx_Platform_Init_Params()");
		return -ENODEV;
	}

	bRetVal = Dptx_Core_Init_Params( pstDptx );
	if( bRetVal )
	{
		dptx_err("from Dptx_Core_Init_Params()");
		return -ENODEV;
	}
	
	bRetVal = Dptx_Avgen_Init_Video_Params( pstDptx, auiPeri_Pixel_Clock );
	if( bRetVal )
	{
		dptx_err("from Dptx_Avgen_Init_Video_Params()");
		return -ENODEV;
	}
	
	bRetVal = Dptx_Avgen_Init_Audio_Params( pstDptx );
	if( bRetVal )
	{
		dptx_err("from Dptx_Avgen_Init_Audio_Params()");
		return -ENODEV;
	}

	mutex_init( &pstDptx->Mutex );

	init_waitqueue_head( &pstDptx->WaitQ );

	atomic_set( &pstDptx->Sink_request, 0 );
	atomic_set( &pstDptx->HPD_IRQ_State, 0 );

	Dptx_Core_Disable_Global_Intr( pstDptx );

	bRetVal = (bool)devm_request_threaded_irq(	&pdev->dev,
													pstDptx->uiHPD_IRQ,
													Dptx_Intr_IRQ,
													Dptx_Intr_Threaded_IRQ,
													IRQF_SHARED | IRQ_LEVEL,
													"Dpv14_Tx",
													pstDptx );
	if( bRetVal ) 
	{
		dptx_err("from devm_request_threaded_irq()");
		return -ENODEV;
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstDptx, &bHotPlugged );
	if( bRetVal )
	{
		dptx_err("from Dptx_Intr_Get_HotPlug_Status()");
	}

	pstDptx->eLast_HPDStatus = ( bHotPlugged == (bool)HPD_STATUS_PLUGGED ) ? (bool)HPD_STATUS_PLUGGED : (bool)HPD_STATUS_UNPLUGGED;
	if( bHotPlugged == (bool)HPD_STATUS_PLUGGED )
	{
		Dptx_Intr_Get_Port_Composition( pstDptx );
	}

	pstVideoParams = &pstDptx->stVideoParams;

	bRetVal = Touch_Max968XX_update_reg(pstDptx);
	if (bRetVal) {
		dptx_err("failed to update Ser/Des Register for Touch");
	}

	dptx_notice("TCC-DPTX-V %d.%d.%d", TCC_DPTX_DRV_MAJOR_VER, TCC_DPTX_DRV_MINOR_VER, TCC_DPTX_DRV_SUBTITLE_VER );
	dptx_notice("	TCC805X %s ", ( pstDptx->uiTCC805X_Revision == TCC805X_REVISION_CS ) ? "CS":"ES");
	dptx_notice("	Hot %s ", ( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged" );
	dptx_notice("	%s mode, %d lanes, %s rate", pstDptx->bMultStreamTransport ? "MST":"SST", pstDptx->stDptxLink.ucNumOfLanes, 
					pstDptx->ucMax_Rate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( pstDptx->ucMax_Rate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( pstDptx->ucMax_Rate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3");
	dptx_notice("	SerDes reset from STR is %s ", ( pstDptx->bSerDes_Reset_STR ) ? "On":"Off" );
	dptx_notice("	PHY Lane is %s ", ( pstDptx->bPHY_Lane_Reswap ) ? "re-swapped":"not re-swapped");
	dptx_notice("	SDM Bypass is %s ", ( pstDptx->bSDM_Bypass ) ? "On":"Off");
	dptx_notice("	SRVC Bypass is %s ", ( pstDptx->bSRVC_Bypass ) ? "On":"Off");
	dptx_notice("	VCP Id :  %d %d %d %d", pstDptx->aucVCP_Id[0], pstDptx->aucVCP_Id[1], pstDptx->aucVCP_Id[2], pstDptx->aucVCP_Id[3]);
	dptx_notice("	%d streams enable -> Pixel encoding = %s, VIC = %d %d %d %d, PClk = %d %d %d %d",
					pstDptx->ucNumOfStreams,
					( pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_RGB ) ? "RGB":( pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_YCBCR422 ) ? "YCbCr422":"YCbCr444",
					pstVideoParams->auiVideo_Code[0], pstVideoParams->auiVideo_Code[1], pstVideoParams->auiVideo_Code[2], pstVideoParams->auiVideo_Code[3],
					pstVideoParams->uiPeri_Pixel_Clock[0], pstVideoParams->uiPeri_Pixel_Clock[1], pstVideoParams->uiPeri_Pixel_Clock[2], pstVideoParams->uiPeri_Pixel_Clock[3] );

	Dptx_Intr_Register_HPD_Callback( pstDptx,	Dpv14_Tx_API_Hpd_Intr_CB );
	Dptx_Core_Enable_Global_Intr( pstDptx,	( DPTX_IEN_HPD | DPTX_IEN_HDCP | DPTX_IEN_SDP | DPTX_IEN_TYPE_C ) );

	Dptx_Ext_Proc_Interface_Init( pstDptx );

	return ( 0 );
}


static int Dpv14_Tx_Remove(struct platform_device *pstDev)
{
	bool	bRetVal;
	struct Dptx_Params		*pstDptx;
	
	dptx_dbg("");
	dptx_dbg("****************************************");
    dptx_dbg("Remove: DP V1.4 driver ");
    dptx_dbg("****************************************");
	dptx_dbg("");

	pstDptx = platform_get_drvdata( pstDev );
	
	bRetVal = Dptx_Core_Deinit( pstDptx );
	if( bRetVal )
	{
		dptx_err("from Dptx_Core_Deinit()");
	}

	bRetVal = Dptx_Intr_Handle_HotUnplug( pstDptx );
	if( bRetVal )
	{
		dptx_err("from Dptx_Core_Deinit()");
	}

	mutex_destroy( &pstDptx->Mutex );

	return 0;
}


int Dpv14_Tx_Suspend_T( struct Dptx_Params	*pstDptx )
{
	bool	bRetVal;

	dptx_info("*** PM Suspend Test ***");

	bRetVal = Dptx_Core_Deinit( pstDptx );
	if( bRetVal )
	{
		dptx_err("from Dptx_Core_Deinit()");
	}

	bRetVal = Dptx_Intr_Handle_HotUnplug( pstDptx );
	if( bRetVal )
	{
		dptx_err("from Dptx_Core_Deinit()");
	}

	return ( 0 );
}

int Dpv14_Tx_Resume_T( struct Dptx_Params	*pstDptx )
{
	bool	bRetVal;
	bool	bHotPlugged;
	struct Dptx_Video_Params	*pstVideoParams;

	dptx_info("*** PM Resume Test *** ");

	pstVideoParams = &pstDptx->stVideoParams;

	dptx_info("Resume: Hot %s, HPD IRQ %d, SSC = %s, PClk %d %d %d %d \n",
				( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged",
				pstDptx->uiHPD_IRQ,
				pstDptx->bSpreadSpectrum_Clock ? "On":"Off",
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0], pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2], pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3] );

	if( pstDptx->bSerDes_Reset_STR )
	{
		Dptx_Max968XX_Reset( pstDptx );
	}

	Dptx_Platform_Set_PMU_ColdReset_Release( pstDptx );
	Dptx_Platform_Set_APAccess_Mode( pstDptx );
	Dptx_Platform_Set_ClkPath_To_XIN( pstDptx );

	if( pstDptx->uiTCC805X_Revision == TCC805X_REVISION_CS )
	{
		Dptx_Platform_Set_PHY_StandardLane_PinConfig( pstDptx );
		Dptx_Platform_Set_SDMBypass_Ctrl( pstDptx );
		Dptx_Platform_Set_SRVCBypass_Ctrl( pstDptx );
		Dptx_Platform_Set_MuxSelect( pstDptx );
	}

	bRetVal = Dptx_Platform_Init( pstDptx );
	if( bRetVal )
	{
		return -ENODEV;
	}

	bRetVal = Dptx_Core_Init( pstDptx );
	if( bRetVal )
	{
		return -ENODEV;
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstDptx, &bHotPlugged );
	if( bRetVal )
	{
		return -ENODEV;
	}

	pstDptx->eLast_HPDStatus = (enum HPD_Detection_Status)bHotPlugged;

	if( bHotPlugged == (bool)HPD_STATUS_PLUGGED )
	{
		bRetVal = Dptx_Link_Perform_BringUp( pstDptx, pstDptx->bMultStreamTransport );
		if( bRetVal == DPTX_RETURN_SUCCESS )
		{
			Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes );

			if( pstDptx->bMultStreamTransport  )
			{
				Dptx_Ext_Set_Topology_Configuration( pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported );
			}
		}
	}

	return 0;
}


#if defined(CONFIG_PM)
static int Dpv14_Tx_Suspend( struct platform_device *pdev, pm_message_t state )
{
	bool	bRetVal;
	struct Dptx_Params		*pstDptx;
	
	dptx_dbg("");
	dptx_dbg("****************************************");
	dptx_dbg("PM Suspend: DP V1.4 driver ");
	dptx_dbg("****************************************");
	dptx_dbg("");

	pstDptx = platform_get_drvdata( pdev );
	
	bRetVal = Dptx_Core_Deinit( pstDptx );
	if( bRetVal ) 
	{
		dptx_err("from Dptx_Core_Deinit()");
	}

	bRetVal = Dptx_Intr_Handle_HotUnplug( pstDptx );
	if( bRetVal ) 
	{
		dptx_err("from Dptx_Core_Deinit()");
	}

	return ( 0 );
}

static int Dpv14_Tx_Resume( struct platform_device *pdev )
{
	bool	bRetVal;
	bool	bHotPlugged;
	int32_t	iError;
	struct Dptx_Params		*pstDptx;
	struct Dptx_Video_Params	*pstVideoParams;

	dptx_dbg("");
	dptx_dbg("****************************************");
	dptx_dbg("PM Resume: DP V1.4 driver ");
	dptx_dbg("****************************************");
	dptx_dbg("");

	pstDptx = platform_get_drvdata( pdev );

	pstVideoParams = &pstDptx->stVideoParams;

	iError = pinctrl_pm_select_default_state(&pdev->dev);
	if (iError) {
		dptx_err("from pinctrl_pm_select_default_state()");
	}

	dptx_info("Resume: Hot %s, HPD IRQ %d, SSC = %s, PClk %d %d %d %d \n", 
				( pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED ) ? "Plugged":"Unplugged", 
				pstDptx->uiHPD_IRQ,
				pstDptx->bSpreadSpectrum_Clock ? "On":"Off", 
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0], pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1], 
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2], pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3] );

	if( pstDptx->bSerDes_Reset_STR )
	{
			Dptx_Max968XX_Reset( pstDptx );
	}

	Dptx_Platform_Set_PMU_ColdReset_Release( pstDptx );
	Dptx_Platform_Set_APAccess_Mode( pstDptx );
	Dptx_Platform_Set_ClkPath_To_XIN( pstDptx );

	if( pstDptx->uiTCC805X_Revision == TCC805X_REVISION_CS )
	{
		Dptx_Platform_Set_PHY_StandardLane_PinConfig( pstDptx );
		Dptx_Platform_Set_SDMBypass_Ctrl( pstDptx );
		Dptx_Platform_Set_SRVCBypass_Ctrl( pstDptx );
		Dptx_Platform_Set_MuxSelect( pstDptx );
	}

	bRetVal = Dptx_Platform_Init( pstDptx );
	if( bRetVal ) 
	{
		return -ENODEV;
	}

	bRetVal = Dptx_Core_Init( pstDptx );
	if( bRetVal ) 
	{
		return -ENODEV;
	}

	bRetVal = Dptx_Intr_Get_HotPlug_Status( pstDptx, &bHotPlugged );
	if( bRetVal ) 
	{
		return -ENODEV;
	}

	pstDptx->eLast_HPDStatus = (enum HPD_Detection_Status)bHotPlugged;

	if( bHotPlugged == (bool)HPD_STATUS_PLUGGED )
	{
		bRetVal = Dptx_Link_Perform_BringUp( pstDptx, pstDptx->bMultStreamTransport );
		if( bRetVal == DPTX_RETURN_SUCCESS ) 
		{
			Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes );

			if( pstDptx->bMultStreamTransport  )
			{
				Dptx_Ext_Set_Topology_Configuration( pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported );
			}
		}
	}

	return 0;
}
#endif


static const struct of_device_id dpv14_tx[] = {
		{ .compatible =	"telechips,dpv14-tx" },
		{ }
};

static struct platform_driver __refdata stDpv14_Tx_pdrv = {
		.probe = Dpv14_Tx_Probe,
		.remove = Dpv14_Tx_Remove,
		.driver = {
				.name = "telechips,dpv14-tx",
				.owner = THIS_MODULE,

#if defined(CONFIG_OF)
				.of_match_table = dpv14_tx,
#endif
		},

#if defined(CONFIG_PM)
		.suspend = Dpv14_Tx_Suspend,
		.resume = Dpv14_Tx_Resume,
#endif

};

static __init int Dpv14_Tx_init(void)
{
	return platform_driver_register( &stDpv14_Tx_pdrv );
}

static __exit void Dpv14_Tx_exit(void)
{
	return platform_driver_unregister( &stDpv14_Tx_pdrv );
}

module_init( Dpv14_Tx_init );
module_exit( Dpv14_Tx_exit );


/** @short License information */
MODULE_LICENSE("GPL v2");
/** @short Author information */
MODULE_AUTHOR("Telechips");
/** @short Device description */
MODULE_DESCRIPTION("DP TX module driver");
/** @short Device version */
MODULE_VERSION("1.0");

