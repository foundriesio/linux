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

#define	MAX_NUM_OF_LOOP_PHY_STATUS			100

#define PHY_LANE_ID_0						0
#define PHY_LANE_ID_1						1
#define PHY_LANE_ID_2						2
#define PHY_LANE_ID_3						3

#define	PHY_NUM_OF_1_LANE					1
#define	PHY_NUM_OF_2_LANE					2
#define	PHY_NUM_OF_4_LANE					4


static int32_t dptx_core_check_vendor_id( struct Dptx_Params *pstDptx )
{
	u32				uiDptx_id;

	uiDptx_id = Dptx_Reg_Readl( pstDptx, DPTX_ID );
	if( uiDptx_id != (u32)(( DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT ) | DPTX_ID_VENDOR_ID ))
	{
		dptx_err("Invalid DPTX Id : 0x%x<->0x%x ", uiDptx_id, (( DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT ) | DPTX_ID_VENDOR_ID ) );
		return DPTX_RETURN_EINVAL;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Link_Power_On( struct Dptx_Params *pstDptx )
{
	int32_t	iRetVal;

	iRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_ON );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Init_Params( struct Dptx_Params *pstDptx )
{
	int32_t	iRetVal;
	
	iRetVal = Dptx_Core_Get_PHY_NumOfLanes( pstDptx, &pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		pstDptx->ucMax_Lanes = (u8)PHY_LANE_4;
	}

	pstDptx->stDptxLink.ucNumOfLanes = pstDptx->ucMax_Lanes;

	iRetVal = Dptx_Core_Get_PHY_Rate( pstDptx, &pstDptx->ucMax_Rate );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		pstDptx->ucMax_Rate  = (u8)LINK_RATE_HBR3;
	}
	pstDptx->stDptxLink.ucLinkRate  = (u8)pstDptx->ucMax_Rate;

	iRetVal = Dptx_Core_Get_Stream_Mode( pstDptx, &pstDptx->bMultStreamTransport );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		pstDptx->bMultStreamTransport = false;
	}

	iRetVal = Dptx_Core_Get_PHY_SSC( pstDptx, &pstDptx->bSpreadSpectrum_Clock );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		pstDptx->bSpreadSpectrum_Clock = true;
	}
	
//	dptx_info("Stream mode = %s, NumOfLanes = %d, Max. rates = %d, SSC = %s", pstDptx->bMultStreamTransport ? "MST":"SST", pstDptx->stDptxLink.ucNumOfLanes, pstDptx->stDptxLink.ucLinkRate, ( pstDptx->bSpreadSpectrum_Clock ) ? "On":"Off" );
	
	return DPTX_RETURN_NO_ERROR;
}

/* Initialize the DP TX core and put it in a known state. */
int32_t Dptx_Core_Init( struct Dptx_Params *pstDptx )
{
	char	aucVerStr[15];
	int32_t	iRetVal;
	u32	uiDptx_Version, uiRegMap_HPD_IEN, uiRegMap_HDCP_INTR, uiRegMap_TYPEC_CTRL, uiRegMap_Cctl;

	iRetVal = dptx_core_check_vendor_id( pstDptx ); 
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	Dptx_Core_Disable_Global_Intr( pstDptx );

	Dptx_Core_Soft_Reset( pstDptx, DPTX_SRST_CTRL_ALL );	/* #define DPTX_SRST_CTRL_ALL ( DPTX_SRST_CTRL_CONTROLLER | DPTX_SRST_CTRL_HDCP | DPTX_SRST_CTRL_AUDIO_SAMPLER | DPTX_SRST_CTRL_AUX ) */

	/* Check the core version */
	memset( aucVerStr, 0, sizeof(aucVerStr) );
	
	uiDptx_Version = Dptx_Reg_Readl( pstDptx, DPTX_VER_NUMBER );
	aucVerStr[0] = ( uiDptx_Version >> 24 ) & 0xff;
	aucVerStr[1] = '.';
	aucVerStr[2] = ( uiDptx_Version >> 16 ) & 0xff;
	aucVerStr[3] = ( uiDptx_Version >> 8 ) & 0xff; 
	aucVerStr[4] = ( uiDptx_Version & 0xff );

	uiDptx_Version = Dptx_Reg_Readl( pstDptx, DPTX_VER_TYPE );
	aucVerStr[5] = '-';
	aucVerStr[6] = ( uiDptx_Version >> 24 ) & 0xff;
	aucVerStr[7] = ( uiDptx_Version >> 16 ) & 0xff;
	aucVerStr[8] = ( uiDptx_Version >> 8 ) & 0xff;
	aucVerStr[9] = ( uiDptx_Version & 0xff );

	dptx_dbg( "Core version: %s ", aucVerStr );

	Dptx_Core_Init_PHY( pstDptx );

	/* 
	* HPD_INTERRUPT_ENABLE[0xD0C + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register configures whether can active Interrupt status bit causes the Interrupt output to be asserted.
	*					HDP_EVENT_EN in GENERAL Interrupt enable registers has to be also set for the Interrupt output to be asserted.
	*					In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.HPD_IRQ_EN[Bit 0]: Set to '1' to enable HPD_IRQ_EN.
	*	-.HPD_PLUG_EN[Bit 1]: Set to '1' to enable HPD_PLUG_EN.
	*	-.HPD_UNPLUG_EN[Bit 2]: Set to '1' to enable HPD_UNPLUG_EN.
	*	-.HPD_UNPLUG_ERR_EN[Bit 3]: Set to '1' to enable HPD_UNPLUG_ERR_EN.
	*/
	uiRegMap_HPD_IEN = Dptx_Reg_Readl( pstDptx, DPTX_HPD_IEN );

	//uiRegMap_HPD_IEN |= ( DPTX_HPD_IEN_IRQ_EN | DPTX_HPD_IEN_HOT_PLUG_EN | DPTX_HPD_IEN_HOT_UNPLUG_EN );
	uiRegMap_HPD_IEN |= ( DPTX_HPD_IEN_IRQ_EN | DPTX_HPD_IEN_HOT_PLUG_EN | DPTX_HPD_IEN_HOT_UNPLUG_EN | DPTX_HPDSTS_UNPLUG_ERR_EN ); /* Guild from SoC driver development guild document */
	Dptx_Reg_Writel( pstDptx, DPTX_HPD_IEN, uiRegMap_HPD_IEN );

	/* Mask interrupt related to HDCP22 GPIO output status */
	uiRegMap_HDCP_INTR = Dptx_Reg_Readl( pstDptx, DPTX_HDCP_API_INT_MSK );
	uiRegMap_HDCP_INTR |= DPTX_HDCP22_GPIOINT;
	Dptx_Reg_Writel( pstDptx, DPTX_HDCP_API_INT_MSK, uiRegMap_HDCP_INTR );

	//Dptx_Core_Enable_Global_Intr( pstDptx, ( DPTX_IEN_HPD | DPTX_IEN_HDCP | DPTX_IEN_SDP | DPTX_IEN_AUDIO_FIFO_OVERFLOW | DPTX_IEN_VIDEO_FIFO_OVERFLOW | DPTX_IEN_TYPE_C ) );
	Dptx_Core_Enable_Global_Intr( pstDptx, ( DPTX_IEN_HPD | DPTX_IEN_HDCP | DPTX_IEN_SDP | DPTX_IEN_TYPE_C ) ); /* Enable all top-level interrupts */

	/* 
	* TYPEC[0xC08 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register has controls for Type-C crossbar switch and for controlling the flip input to the cross bar and the Aux phy.
	*					 Please see section Type-C mode Usage for more details.
	*					In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.TYPEC_DISABLE_ACK[Bit 0]: This register controls the primary output pin tca_disable_ack_o. The value in ithis register is directly reflected on the output.
	*	-.TYPEC_DISABLE_STATUS[Bit 1]: This is a read only register bit which directly reflects the status of the primary input pin tca_disable_i. After receving a TYPE_C interrupt, software reads this register bit to get the status of the tca_disable
	*	-.TYPEC_INTRURPPT_STATUS[Bit 2]: This is the interrrupt status pin for TYPE-C. On a type-C interrupt software reads this register to confirm a Type-C interrupt. Writing 1 by the software clears TYPE-C interrupt.
	*/
	uiRegMap_TYPEC_CTRL = Dptx_Reg_Readl( pstDptx, DPTX_TYPE_C_CTRL );	/* Guild fro SoC driver development document */
	uiRegMap_TYPEC_CTRL &= ~(DPTX_TYPEC_DISABLE_ACK);
	uiRegMap_TYPEC_CTRL &= ~(DPTX_TYPEC_DISABLE_STATUS);
	uiRegMap_TYPEC_CTRL |= DPTX_TYPEC_INTRURPPT_STATUS;

	Dptx_Reg_Writel( pstDptx, DPTX_TYPE_C_CTRL, uiRegMap_TYPEC_CTRL );
	/* 
	* CCTL[0x200 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1]  This register control the global functionality of the core.
	*													In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.ENHANCE_FRAMING_EN[bit1] : When set, controller follows enhanced framing as spcified in section 2.2.1.2 of the Spec. Configure this bit based on enchanced framing support of Sink.
	*	-.ENABLE_MST_MODE[bit3] : When programmed to 1, the debounce filter on the HPD input is reduced from 100ms in normal operation to 10us. This bit doesn't have any impact on the IRQ generation timing
	*	-.ENABLE_MST_MODE[bit25] : Thsis bit is used for enabling MST mode. If the controller is configured MST mode, driver shall set this bit to '1'.
	*	-.ENHANCE_FRAMING_WITH_FEC_EN[bit29]: When set, controller follows enhanced framing with FEC as specified in section 3.5.1.1 of DP1.4 Spec. 
	*										   Configure this bit when enabling FEC on Sink ie before setting
	*/
	uiRegMap_Cctl = Dptx_Reg_Readl( pstDptx, DPTX_CCTL );

	uiRegMap_Cctl |= DPTX_CCTL_ENH_FRAME_EN;
	uiRegMap_Cctl &= ~DPTX_CCTL_SCALE_DOWN_MODE;
	uiRegMap_Cctl &= ~DPTX_CCTL_FAST_LINK_TRAINED_EN;

	Dptx_Reg_Writel( pstDptx, DPTX_CCTL, uiRegMap_Cctl);

	return DPTX_RETURN_NO_ERROR;
}

/* Disable the core in preparation for module shutdown. */
int32_t Dptx_Core_Deinit( struct Dptx_Params *pstDptx )
{
	Dptx_Core_Disable_Global_Intr( pstDptx );
	Dptx_Core_Soft_Reset( pstDptx, DPTX_SRST_CTRL_ALL );	/* #define DPTX_SRST_CTRL_ALL ( DPTX_SRST_CTRL_CONTROLLER | DPTX_SRST_CTRL_HDCP | DPTX_SRST_CTRL_AUDIO_SAMPLER |	 DPTX_SRST_CTRL_AUX ) */

	return DPTX_RETURN_NO_ERROR;
}

/* Synopsys -> Initializes the PHY layer of the core. This needs to be called whenever the PHY layer is reset. */
void Dptx_Core_Init_PHY( struct Dptx_Params *pstDptx )
{
	u32		ucRegMap_PhyCtrl;

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_LANES[Bit6:5]( 0: 1 Lane, 1: 2 Lanes, 2: 4 Lanes )
	 * -.PHY_LANES[Bit7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *                   	cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timout that is appropriate to the PHTY. Generally this is 100ms
	 * -.SSC_DIS[Bit16]( 0: Enable SSC, 1: Disable SSC )
	 * -.PHY_POWERDOWN[Bit20:17]( 0: Powered on, 2: Intermediate P2 Power state for switching rates, 3: PHY is powered down phy_clk might stop, 4: PHY is powered down, reference clock can be stopped
	 * -.PHY_WIDTH[Bit25]( 0: 20-bit inteface, 1: 40-bit inteface ): This bit is used to configure the data width of the main link interface to the PHY.
	 */

	ucRegMap_PhyCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	ucRegMap_PhyCtrl &= ~DPTX_PHYIF_CTRL_WIDTH;

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, ucRegMap_PhyCtrl );
}

void Dptx_Core_Soft_Reset( struct Dptx_Params *pstDptx, u32 uiReset_Bits )
{
	u32			ucRegMap_Reset, uiRegMap_BitMask;

	uiRegMap_BitMask = ( uiReset_Bits & DPTX_SRST_CTRL_ALL ); 

	/* 
	* SOFT_RESET_CTRL[0x204 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register controls the software initiated reset of the controller.
																	 The soft reset should be asserted for at least 5u Sec. to insure a successful reset.
	*																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.CONTROLLER_RESET[Bit 0]: Software sets this bit to start the controller reset process. This also initiate a PHY reset.
	*   -.PHY_SOFT_RESET[Bit 1]: Software sets this bit to start the PHY reset process. 
	*   -.HDCP_MODULE_RESET[Bit 2]: Software sets this bit to start the HDCP module reset process. 
	*   -.AUDIO_SAMPLER_RESET[Bit 3]: Software sets this bit to start the audio sampler reset process.  This also clears the audio FIFO. In MST mode, this bit is used for Stream 0
	*   -.AUX_RESET[Bit 4]: Software sets this bit to start the aux channel reset process. 
	*   -.VIDEO_RESET[Bit 5~8]: Software sets this bit to start the video logic.  In MST mode, each bit controls the reset of a specific Video stream. Bit5~8 controls the soft reset of Video stream 0 to 3.
	*   -.AUDIO_SAMPLER_RESET_STREAM1[Bit9]: Software sets this bit to start the audio sampler reset.
	*   -.AUDIO_SAMPLER_RESET_STREAM2[Bit10]: Software sets this bit to start the audio sampler reset.
	*   -.AUDIO_SAMPLER_RESET_STREAM3[Bit11]: Software sets this bit to start the audio sampler reset.
	*   -.Reserved[Bit31~12]
	*/
	ucRegMap_Reset = Dptx_Reg_Readl( pstDptx, DPTX_SRST_CTRL );
	ucRegMap_Reset |= uiRegMap_BitMask;
	Dptx_Reg_Writel( pstDptx, DPTX_SRST_CTRL, ucRegMap_Reset );

	udelay(  20 );

	ucRegMap_Reset = Dptx_Reg_Readl( pstDptx, DPTX_SRST_CTRL );
	ucRegMap_Reset &= ~uiRegMap_BitMask;
	Dptx_Reg_Writel( pstDptx, DPTX_SRST_CTRL, ucRegMap_Reset );
}

void Dptx_Core_Enable_Global_Intr( struct Dptx_Params *pstDptx, u32 uiEnable_Bits )
{
	u32			uiRegMap_IntEn;
	
	/* 
	* GENERAL_INTERRUPT_ENABLE[0xD04 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register configures whether can active Interrupt status bit causes the Interrupt output to be asserted.
	*																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.HPD_EVENT_EN[Bit 0]: Set to '1' to enable HPD_EVENT.
	*   -.AUX_REPLY_EVENT_EN[Bit 1]: Set to '1' to enable AUX_REPLY_EVENT.
	*   -.HDCP_EVENT_EN[Bit 2]: Set to '1' to enable HDCP_EVENT.
	*   -.AUX_CMD_INVALID_EN[Bit 3]: Set to '1' to enable AUX_CMD_INVALID_EVENT.
	*   -.SDP_EVENT_EN_STREAM0[Bit 4]: Set to '1' to enable SDP EVENT.
	*   -.AUDIO_FIFO_OVERFLOW_EN_STREAM0[Bit 5]: Set to '1' to enable AUDIO_FIFO_OVERFLOW_EVENT for Stream 0.
	*   -.VIDEO_FIFO_OVERFLOW_EN_STREAM0[Bit 6]: Set to '1' to enable VIDEO_FIFO_OVERFLOW_EVENT for Stream 0.
	*/
	uiRegMap_IntEn = Dptx_Reg_Readl( pstDptx, DPTX_IEN );
	uiRegMap_IntEn |= uiEnable_Bits;
	Dptx_Reg_Writel( pstDptx, DPTX_IEN, uiRegMap_IntEn );
}

void Dptx_Core_Disable_Global_Intr( struct Dptx_Params *pstDptx )
{
	u32			uiIntEnable;

	uiIntEnable = Dptx_Reg_Readl( pstDptx, DPTX_IEN );
	uiIntEnable &= ~DPTX_IEN_ALL_INTR;
	Dptx_Reg_Writel( pstDptx, DPTX_IEN, uiIntEnable );
}

int32_t Dptx_Core_Clear_General_Interrupt( struct Dptx_Params *pstDptx, u32 uiClear_Bits )
{
	u32		ucRegMap_GeneralIntr;

	/* 
	* GENERAL_INTERRUPT[0xD00 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1] This register notifies the software of any pending interrrupts. Write 1 to clear the interrupt.
	*																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.HPD_EVENT[Bit 0]: This bit indicates that an HDP event is captured in the HPD status register
	*   -.AUX_REPLY_EVENT[Bit 1]: After initiating an AUX request, this bit is set to 1 after receiving a reply. 
	*   -.HDCP_EVENT[Bit 2]: An HDCP event has occurrred. 
	*   -.AUX_CMD_INVALID[Bit 3]: After initiating an AUX request, this bit is set to 1 if the command or length was invalid.
	*   -.SDP_EVENT_STREAM0[Bit 4]: An event indicating the completion of transmission of an SDP sor stream 0
	*   -.AUDIO_FIFO_OVERFLOW_STREAM0[Bit 5]: Indicates that one or more of the audio sampler FIFOs are getting into overflow condition for stream 0
	*   -.VIDEO_FIFO_OVERFLOW_STREAM0[Bit 6]: Indicates that video lane steering FIFO for stream 0 gets overflow
	*	-.TYPE_C_EVENT[Bit 7]: Indicates and event related to Type-C assit block.
	*	-.Reserved[Bit11 ~ 8]
	*   -.SDP_EVENT_STREAM1[Bit 12]: An event indicating the completion of transmission of an SDP sor stream 1
	*   -.AUDIO_FIFO_OVERFLOW_STREAM1[Bit 13]: Indicates that one or more of the audio sampler FIFOs are getting into overflow condition for stream 1
	*   -.VIDEO_FIFO_OVERFLOW_STREAM1[Bit 14]: Indicates that video lane steering FIFO for stream 1 gets overflow
	*	-.Reserved[Bit17 ~ 15]
	*	-.SDP_EVENT_STREAM2[Bit 18]: An event indicating the completion of transmission of an SDP sor stream 2
	*	-.AUDIO_FIFO_OVERFLOW_STREAM2[Bit 19]: Indicates that one or more of the audio sampler FIFOs are getting into overflow condition for stream 2
	*	-.VIDEO_FIFO_OVERFLOW_STREAM2[Bit 20]: Indicates that video lane steering FIFO for stream 2 gets overflow
	*	-.Reserved[Bit23 ~ 21]
	*	-.SDP_EVENT_STREAM3[Bit 24]: An event indicating the completion of transmission of an SDP sor stream 3
	*	-.AUDIO_FIFO_OVERFLOW_STREAM3[Bit 25]: Indicates that one or more of the audio sampler FIFOs are getting into overflow condition for stream 3
	*	-.VIDEO_FIFO_OVERFLOW_STREAM3[Bit 26]: Indicates that video lane steering FIFO for stream 3 gets overflow
	*	-.Reserved[Bit29 ~ 27]
	*	-.DSC_EVENT[Bit 30]: An DSC evnent has occurred.
	*   -.Reserved[Bit31]
	*/

	ucRegMap_GeneralIntr = Dptx_Reg_Readl( pstDptx, DPTX_ISTS );

	ucRegMap_GeneralIntr |= uiClear_Bits; 

	Dptx_Reg_Writel( pstDptx, DPTX_ISTS, ucRegMap_GeneralIntr );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_SSC( struct Dptx_Params *pstDptx, bool bSink_Supports_SSC )
{
	int32_t	iRetVal;
	u32	uiRegMap_PhyIfCtrl;

	iRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_DOWN_REF_CLOCK );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_DOWN_PHY_CLOCK );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 *					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 *					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_LANES[Bit6:5]( 0: 1 Lane, 1: 2 Lanes, 2: 4 Lanes )
	 * -.PHY_LANES[Bit7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *						If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface. 	 
	 * -.PHY_BUSY[Bit15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *						cleared when the PHY sends a phy_laneX_phystatus pulse.
	 *						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 *						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 *						Software can implement a timout that is appropriate to the PHTY. Generally this is 100ms
	 * -.SSC_DIS[Bit16]( 0: Enable SSC, 1: Disable SSC )
	 * -.PHY_POWERDOWN[Bit20:17]( 0: Powered on, 2: Intermediate P2 Power state for switching rates, 3: PHY is powered down phy_clk might stop, 4: PHY is powered down, reference clock can be stopped
	 * -.PHY_WIDTH[Bit25]( 0: 20-bit inteface, 1: 40-bit inteface ): This bit is used to configure the data width of the main link interface to the PHY.
	 */
	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	if( pstDptx->bSpreadSpectrum_Clock && bSink_Supports_SSC )
	{
		uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_SSC_DIS;
	    }
	else
	{
		uiRegMap_PhyIfCtrl |= DPTX_PHYIF_CTRL_SSC_DIS;
	}

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl );

	iRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_ON );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}
	
	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_SSC( struct Dptx_Params *pstDptx, bool *pbSSC_Enabled )
{
	u32				uiRegMap_PhyIfCtrl;
	
	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );

	if( uiRegMap_PhyIfCtrl & DPTX_PHYIF_CTRL_SSC_DIS )
	{
		*pbSSC_Enabled = false;
	}
	else
	{
		*pbSSC_Enabled = true;
	}
	
	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_Sink_SSC_Capability( struct Dptx_Params *pstDptx, bool *pbSSC_Profiled )
{
	u8	ucDCDPValue;
	int32_t	iRetVal;
	
	/*
	 * DP_MAX_DOWNSPREAD[00003]: SSC( Spread-Spectrum Clock )
	 * -.MAX_DOWNSPREAD[Bit0]( 0: No down-spread, 1: Up to 0.5% down-spread )
	 * -.Sparead Spectrum = In telecommunication and radio communication, 
	 *		  			     spread-spectrum techniques are methods by which a signal (e.g., an electrical, electromagnetic, or acoustic signal)	generated with a particular bandwidth is deliberately spread in the frequency domain, 
	 *					     resulting in a signal with a wider bandwidth.
	 */
	iRetVal = Dptx_Aux_Read_DPCD( pstDptx, DP_MAX_DOWNSPREAD, &ucDCDPValue );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	if(ucDCDPValue & SINK_TDOWNSPREAD_MASK) {
		dptx_dbg("SSC enable on the sink side" );
		*pbSSC_Profiled = true;
	}
	else {
		dptx_dbg("SSC disabled on the sink side" );  
		*pbSSC_Profiled = false;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_Stream_Mode( struct Dptx_Params *pstDptx, bool *pbMST_Mode )
{
	u32		uiRegMap_Cctl;

	/* 
	* CCTL[0x200 + ( i*10000 ), i = DPTX_NUM_STREAMS - 1]  This register control the global functionality of the core.
	*													In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	*	-.ENHANCE_FRAMING_EN[bit1] : When set, controller follows enhanced framing as spcified in section 2.2.1.2 of the Spec. Configure this bit based on enchanced framing support of Sink.
	*	-.SCALE_DOWN_MDE[bit3] : When programmed to 1, the debounce filter on the HPD input is reduced from 100ms in normal operation to 10us. This bit doesn't have any impact on the IRQ generation timing
	*	-.ENABLE_MST_MODE[bit25] : Thsis bit is used for enabling MST mode. If the controller is configured MST mode, driver shall set this bit to '1'.
	*	-.ENHANCE_FRAMING_WITH_FEC_EN[bit29]: When set, controller follows enhanced framing with FEC as specified in section 3.5.1.1 of DP1.4 Spec. 
	*										   Configure this bit when enabling FEC on Sink ie before setting
	*/

	uiRegMap_Cctl = Dptx_Reg_Readl( pstDptx, DPTX_CCTL );

	if( uiRegMap_Cctl & DPTX_CCTL_ENABLE_MST_MODE )
	{
		*pbMST_Mode = true;
	}
	else
	{
		*pbMST_Mode = false;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_NumOfLanes( struct Dptx_Params *pstDptx, u8 *pucNumOfLanes )
{
	u8		ucNumOfLanes;
	u32		uiRagMap_PhyIfCtrl;

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_RATE[Bit6:5]( 0: RBR 1.63Gbs, 1:  HBR 2.7Gbs, 2: HBR2 5.4Gbs, 3: HBR3 8.1Gbs )
	 * -.PHY_LANES[Bit7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *                   	cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timout that is appropriate to the PHTY. Generally this is 100ms
	 * -.SSC_DIS[Bit16]( 0: Enable SSC, 1: Disable SSC )
	 * -.PHY_POWERDOWN[Bit20:17]( 0: Powered on, 2: Intermediate P2 Power state for switching rates, 3: PHY is powered down phy_clk might stop, 4: PHY is powered down, reference clock can be stopped
	 * -.PHY_WIDTH[Bit25]( 0: 20-bit inteface, 1: 40-bit inteface ): This bit is used to configure the data width of the main link interface to the PHY.
	 */ 
	uiRagMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL);

	ucNumOfLanes = (u8)( ( uiRagMap_PhyIfCtrl & DPTX_PHYIF_CTRL_LANES_MASK ) >> DPTX_PHYIF_CTRL_LANES_SHIFT );

	*pucNumOfLanes = ( 1 << ucNumOfLanes );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_NumOfLanes( struct Dptx_Params *pstDptx, u8 ucNumOfLanes )
{
	u8		ucPHY_Lanes;
	u32		uiRegMap_PhyIfCtrl;

	//dptx_dbg("Calling... ucNumOfLanes = %d lanes ", ucNumOfLanes );

	switch( ucNumOfLanes )
	{
		case 1:
			ucPHY_Lanes = 0;	
			break;
		case 2:
			ucPHY_Lanes = 1;
			break;
		case 4:
			ucPHY_Lanes = 2;
			break;
		default:
			dptx_err("Invalid number of lanes -> %d lanes", (u32)ucNumOfLanes);
			return DPTX_RETURN_EINVAL;
			break;
	}

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_RATE[Bit6:5]( 0: RBR 1.63Gbs, 1:  HBR 2.7Gbs, 2: HBR2 5.4Gbs, 3: HBR3 8.1Gbs )
	 * -.PHY_LANES[Bit7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *                   	cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timout that is appropriate to the PHTY. Generally this is 100ms
	 * -.SSC_DIS[Bit16]( 0: Enable SSC, 1: Disable SSC )
	 * -.PHY_POWERDOWN[Bit20:17]( 0: Powered on, 2: Intermediate P2 Power state for switching rates, 3: PHY is powered down phy_clk might stop, 4: PHY is powered down, reference clock can be stopped
	 * -.PHY_WIDTH[Bit25]( 0: 20-bit inteface, 1: 40-bit inteface ): This bit is used to configure the data width of the main link interface to the PHY.
	 */ 
	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_LANES_MASK;
	uiRegMap_PhyIfCtrl |= ( ucPHY_Lanes << DPTX_PHYIF_CTRL_LANES_SHIFT );

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_PowerState( struct Dptx_Params *pstDptx, enum PHY_POWER_STATE ePowerState )
{
	u32			uiRegMap_PhyIfCtrl;

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_RATE[Bit6:5]( 0: RBR 1.63Gbs, 1:  HBR 2.7Gbs, 2: HBR2 5.4Gbs, 3: HBR3 8.1Gbs )
	 * -.PHY_LANES[Bit7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *                   	cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timout that is appropriate to the PHTY. Generally this is 100ms
	 * -.SSC_DIS[Bit16]( 0: Enable SSC, 1: Disable SSC )
	 * -.PHY_POWERDOWN[Bit20:17]<Reset 0x03>( 0: Powered on, 2: Intermediate P2 Power state for switching rates, 3: PHY is powered down phy_clk might stop, 4: PHY is powered down, reference clock can be stopped
	 * -.PHY_WIDTH[Bit25]( 0: 20-bit inteface, 1: 40-bit inteface ): This bit is used to configure the data width of the main link interface to the PHY.
	 */ 
	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK;

	//dptx_dbg("Calling... ePowerState = %d, uiRegMap_PhyIfCtrl = 0x%x ", (u32)ePowerState, (u32)uiRegMap_PhyIfCtrl );

	switch( ePowerState )
	{
		case PHY_POWER_ON:
		case PHY_POWER_DOWN_SWITCHING_RATE:
		case PHY_POWER_DOWN_PHY_CLOCK:
		case PHY_POWER_DOWN_REF_CLOCK:
			uiRegMap_PhyIfCtrl |= ( ePowerState << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT );
			break;
		default:
			dptx_err( "Invalid power state: %d \n", (u32)ePowerState );
			return DPTX_RETURN_EINVAL;
			break;
	}

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl );

	return DPTX_RETURN_NO_ERROR;
}


/* 
	Synopsys -> If the Synopsys Combo PHY is used, the software must first program PHY_POWERDOWN to 2 or 3 first and 
				watiting for PHYBUSY to clear. Afterwards, rate can be changed along with PHY_POWERDOWN
*/
int32_t Dptx_Core_Set_PHY_Rate( struct Dptx_Params *pstDptx, enum PHY_LINK_RATE eRate )
{
	u32		uiPhyIfCtrl;

	//dptx_dbg( "Calling... " );

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *																In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_RATE[Bit6:5]( 0: RBR 1.63Gbs, 1:  HBR 2.7Gbs, 2: HBR2 5.4Gbs, 3: HBR3 8.1Gbs )
	 * -.PHY_LANES[Bit7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *                   	cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timout that is appropriate to the PHTY. Generally this is 100ms
	 * -.SSC_DIS[Bit16]( 0: Enable SSC, 1: Disable SSC )
	 * -.PHY_POWERDOWN[Bit20:17]( 0: Powered on, 2: Intermediate P2 Power state for switching rates, 3: PHY is powered down phy_clk might stop, 4: PHY is powered down, reference clock can be stopped
	 * -.PHY_WIDTH[Bit25]( 0: 20-bit inteface, 1: 40-bit inteface ): This bit is used to configure the data width of the main link interface to the PHY.
	 */ 
	uiPhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	uiPhyIfCtrl &= ~DPTX_PHYIF_CTRL_RATE_MASK;
	uiPhyIfCtrl |= (u32)eRate << DPTX_PHYIF_CTRL_RATE_SHIFT;

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiPhyIfCtrl );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_Rate( struct Dptx_Params *pstDptx, u8 *pucPHY_Rate )
{
	u32			UiRegMap_PHY_IF_Ctrl, uiRate;

	UiRegMap_PHY_IF_Ctrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	uiRate = ( UiRegMap_PHY_IF_Ctrl & DPTX_PHYIF_CTRL_RATE_MASK) >> DPTX_PHYIF_CTRL_RATE_SHIFT;

	*pucPHY_Rate = uiRate;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_BUSY_Status( struct Dptx_Params *pstDptx, u8 ucNumOfLanes )
{
	u32			uiRegMap_PhyIfCtrl, uiBitMask = 0, uiCount = 0;;

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *									In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit 3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit 5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_LANES[Bit 7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit 11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit 15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timeout that is appropriate to the PHTY. Generally this is 100ms
	 */
	switch( ucNumOfLanes ) 
	{
		case PHY_NUM_OF_4_LANE:
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 3 );
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 2 );
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 1 );
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 0 );
			break;
		case PHY_NUM_OF_2_LANE:
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 1 );
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 0 );
			break;
		case PHY_NUM_OF_1_LANE:
			uiBitMask |= DPTX_PHYIF_CTRL_BUSY( 0 );
			break;
		default:
			dptx_err("Invalid number of lanes %d", (u32)ucNumOfLanes);
			return DPTX_RETURN_EINVAL;
			break;
	}

	do{
		uiRegMap_PhyIfCtrl  = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );

		if( !( uiRegMap_PhyIfCtrl & uiBitMask ) )
		{
			dptx_dbg("PHY status droped to 0, lanes = %d, count = %d, mask = 0x%x ", ucNumOfLanes, uiCount, uiBitMask );
			break;
		}

		if( uiCount == MAX_NUM_OF_LOOP_PHY_STATUS )
		{
			dptx_err( "PHY BUSY timed out" );
			return DPTX_RETURN_ENODEV;
		}

		mdelay( 1 );/* Register map mentions appropriate thimeout is 100ms */
	}while( uiCount++ < MAX_NUM_OF_LOOP_PHY_STATUS );


	return DPTX_RETURN_NO_ERROR;
	
}

int32_t Dptx_Core_Set_PHY_PreEmphasis( struct Dptx_Params *pstDptx,  			        unsigned int iLane_Index, enum PHY_PRE_EMPHASIS_LEVEL ePreEmphasisLevel )
{
	u32		uiRegMap_PhyTxEQ;

	//dptx_dbg("Set_PHY_PreEmphasis => lane index =%d, Preemp level=%d", iLane_Index, ePreEmphasisLevel );

	/*
	 * PHY_TX_EQ[0xA04 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures TX pre-emphasis and voltage swing levles for PHY interface for all lanes.
	 *									The PHY is responsible for taking these levels aand mapping to the spec voltage swing, pre-emphasis Post Cursor2 levels to emply with DP1.3 Spec.
	 *								In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.LANE0_TX_PREEMP[Bit1:0]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3  )
	 * -.LANE0_TX_VSWING[Bit3:2]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3  )
	 */
	if( iLane_Index > (u32)PHY_LANE_ID_3 )
	{
		dptx_err("Invalid lane %d ", iLane_Index );
		return DPTX_RETURN_EINVAL;
	}
	if( ePreEmphasisLevel > (u32)PRE_EMPHASIS_LEVEL_3 )
	{
		dptx_err("Invalid pre-emphasis level %d, using 3 ", ePreEmphasisLevel );
		ePreEmphasisLevel = PRE_EMPHASIS_LEVEL_3;
	}

	uiRegMap_PhyTxEQ = Dptx_Reg_Readl( pstDptx, DPTX_PHY_TX_EQ );
	uiRegMap_PhyTxEQ &= ~( DPTX_PHY_TX_EQ_PREEMP_MASK( iLane_Index ) );
	uiRegMap_PhyTxEQ |= ( (u32)ePreEmphasisLevel << DPTX_PHY_TX_EQ_PREEMP_SHIFT(iLane_Index) ) &		DPTX_PHY_TX_EQ_PREEMP_MASK(iLane_Index);

	Dptx_Reg_Writel( pstDptx, DPTX_PHY_TX_EQ, uiRegMap_PhyTxEQ );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_VSW( struct Dptx_Params *pstDptx, unsigned int iLane_Index, enum PHY_VOLTAGE_SWING_LEVEL eVoltageSwingLevel )
{
	u32			uiRegMap_PhyTxEQ;

	//dptx_dbg("Set_PHY_VSW => lane index =%d, Voltage level=%d", iLane_Index, eVoltageSwingLevel );

	/*
	 * PHY_TX_EQ[0xA04 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures TX pre-emphasis and voltage swing levles for PHY interface for all lanes.
	 *									The PHY is responsible for taking these levels aand mapping to the spec voltage swing, pre-emphasis Post Cursor2 levels to emply with DP1.3 Spec.
	 *								In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.LANE0_TX_PREEMP[Bit1:0]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3  )
	 * -.LANE0_TX_VSWING[Bit3:2]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3  )
	 */

	if( iLane_Index > (u32)PHY_LANE_ID_3 )
	{
		dptx_err("Invalid lane %d ", iLane_Index );
		return DPTX_RETURN_EINVAL;
	}

	if( eVoltageSwingLevel > VOLTAGE_SWING_LEVEL_3 )
	{
		dptx_err("Invalid vswing level %d, using 3 ", eVoltageSwingLevel );
		eVoltageSwingLevel = VOLTAGE_SWING_LEVEL_3;
	}

	uiRegMap_PhyTxEQ = Dptx_Reg_Readl( pstDptx, DPTX_PHY_TX_EQ );
	uiRegMap_PhyTxEQ &= ~( DPTX_PHY_TX_EQ_VSWING_MASK(iLane_Index) );
	uiRegMap_PhyTxEQ |= ( (u32)eVoltageSwingLevel << DPTX_PHY_TX_EQ_VSWING_SHIFT(iLane_Index) ) &	DPTX_PHY_TX_EQ_VSWING_MASK( iLane_Index );

	Dptx_Reg_Writel( pstDptx, DPTX_PHY_TX_EQ, uiRegMap_PhyTxEQ );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_Pattern( struct Dptx_Params *pstDptx, u32 uiPattern )
{
	u32			uiPhyTPSSelection = 0;

	//dptx_dbg( "Calling... " );

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *									In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit 3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit 5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 *					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 *					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_LANES[Bit 7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit 11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *						If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface. 	 
	 * -.PHY_BUSY[Bit 15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *						cleared when the PHY sends a phy_laneX_phystatus pulse.
	 *						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 *						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 *						Software can implement a timeout that is appropriate to the PHTY. Generally this is 100ms
	 */

	uiPhyTPSSelection = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );
	uiPhyTPSSelection &= ~DPTX_PHYIF_CTRL_TPS_SEL_MASK;
	uiPhyTPSSelection |= (( uiPattern << DPTX_PHYIF_CTRL_TPS_SEL_SHIFT ) & DPTX_PHYIF_CTRL_TPS_SEL_MASK );
	
	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiPhyTPSSelection );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Enable_PHY_XMIT( struct Dptx_Params *pstDptx, u32 iNumOfLanes )
{
	u32			uiRegMap_PhyIfCtrl, uiBitMask = 0;

	//dptx_dbg( "Enable_PHY_XMIT... iNumOfLanes=%d ", iNumOfLanes );

	/*
	 * PHYIF_CTRL[0xA00 + (i * 10000 ), i = DWC_DPTX_NUM_STREAMS - 1]: This register configures the PHY interface layer of the DWC_dptx.
	 *									In MST mode, even though this is visible for each streams this should be accessed with respect to Stream 0 address only. 
	 * -.TPS_SEL[Bit 3:0]( 0: No training patter, 1: TPS1, 2: TPS2, 3: TPS3, 4: TPS4 ): Select the training pattern to be transmitted to the PHY.
	 * -.PHYRATE[Bit 5:4]( 0: RBR 1.62GB, 1: HBR 2.7GB, 2: HBR2 5.4GB, 3: HBR3 8.1GB ): Rate setting for the PHY
	 * 					 If the Synopsys Combo PHY is used, the software must firts program PHY_POWERDOWN to 2 or 3 and waiting for PHYBUY to clear.
	 * 					 Afterwards, rate can be changed along with PHY_POWERDOWN.	 
	 * -.PHY_LANES[Bit 7:6]( 0: 1 Lane 0, 1: 2 Lanes 2: 4 Lanes ): Number of lanes active
	 * -.XMIT_ENABLE[Bit 11:8]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						Enable transmitter on the PHY per lane. 
	 *						If TPS_SEL is 0 then main link porovides the data to be transmitted.
	 *                  	If PHY_POWERDOWN is not 0 then the transmitter is not enabled on the PHY interface.      
	 * -.PHY_BUSY[Bit 15:12]( 1: Lane 0, 2: Lane 1, 4: Lane 3, 8: Lane 4 ): 
	 *						This bit for each lane is set when a power state's change or rate's change is requested and 
	 *                   	cleared when the PHY sends a phy_laneX_phystatus pulse.
	 * 						This bit is also be set when PHY reset is asserted and cleared when a phy_laneX_phystatus drops to 0 after reset finishes.
	 * 						No changes to PHY_RATE or PHY_POWERDOWN are to occur when PHY_BUSY is high.
	 * 						Software can implement a timeout that is appropriate to the PHTY. Generally this is 100ms
	 */
	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );

	switch( iNumOfLanes ) 
	{
		case PHY_NUM_OF_4_LANE:
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 3 );
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 2 );
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 1 );
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 0 );
			break;
		case PHY_NUM_OF_2_LANE:
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 1 );
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 0 );
			break;
		case PHY_NUM_OF_1_LANE:
			uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN( 0 );
			break;
		default:
			dptx_err("Invalid number of lanes %d", (u32)iNumOfLanes);
			break;
	}

	uiRegMap_PhyIfCtrl |= uiBitMask;

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Disable_PHY_XMIT( struct Dptx_Params *pstDptx, u32 iNumOfLanes )
{
	u32			uiRegMap_PhyIfCtrl, 	uiBitMask = 0;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl( pstDptx, DPTX_PHYIF_CTRL );

	switch( iNumOfLanes ) 
	{
		case PHY_NUM_OF_4_LANE:
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 3 );
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 2 );
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 1 );
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 0 );
			break;
		case PHY_NUM_OF_2_LANE:
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 1 );
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 0 );
			break;
		case PHY_NUM_OF_1_LANE:
			uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN( 0 );
			break;
		default:
			dptx_err("Invalid number of lanes %d", (u32)iNumOfLanes);
			break;
	}

	uiRegMap_PhyIfCtrl &= ~uiBitMask;

	Dptx_Reg_Writel( pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_PHY_Rate_To_Bandwidth( struct Dptx_Params *pstDptx, u8 ucRate, u8 *pucBandWidth )
{
	switch( ucRate ) 
	{
		case DPTX_PHYIF_CTRL_RATE_RBR:
			*pucBandWidth = DP_LINK_BW_1_62;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR:
			*pucBandWidth = DP_LINK_BW_2_7;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			*pucBandWidth = DP_LINK_BW_5_4;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			*pucBandWidth = DP_LINK_BW_8_1;
			break;
		default:
			dptx_err("Invalid rate %d ", (u32)ucRate);
			return DPTX_RETURN_EINVAL;
			break;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_PHY_Bandwidth_To_Rate( struct Dptx_Params *pstDptx, u8 ucBandWidth, u8 *pucRate )
{
	switch( ucBandWidth ) 
	{
		case DP_LINK_BW_1_62:
			*pucRate = DPTX_PHYIF_CTRL_RATE_RBR;
			break;
		case DP_LINK_BW_2_7:
			*pucRate = DPTX_PHYIF_CTRL_RATE_HBR;
			break;
		case DP_LINK_BW_5_4:
			*pucRate = DPTX_PHYIF_CTRL_RATE_HBR2;
			break;
		case DP_LINK_BW_8_1:
			*pucRate = DPTX_PHYIF_CTRL_RATE_HBR3;
			break;
		default:
			dptx_err("Invalid link rate -> %d ", (u32)ucBandWidth);
			return DPTX_RETURN_EINVAL;
			break;
	}

	return DPTX_RETURN_NO_ERROR;
}


