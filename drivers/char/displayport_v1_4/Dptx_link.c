// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/delay.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"


#define MAX_TRY_CLOCK_RECOVERY						5		/* Vesa Spec. Figure 3-20 */
#define MAX_TRY_CHANNEL_EQ							5		/* Vesa Spec. Figure 3-21 */
#define MAX_TRY_SINK_LINK_STATUS_UPDATE				10		/* Vesa Spec. Figure 3-20 */
#define MAX_TRY_SINK_UPDATE_STATUS					100

#define DP_TRAIN_VOLTAGE_LEVEL_MASK		  			0x03
#define DP_TRAIN_PRE_EMPHASIS_LEVEL_MASK		    0x0C

#define DP_TRAIN_CLOCK_RECOVERY						true
#define DP_TRAIN_CHANNEL_EQ							false

static int32_t dptx_link_get_clock_recovery_status( struct Dptx_Params *dptx, bool bCR_Training, bool *pbCR_Done )
{
	u8	ucDPCD_AuxReadInterval, ucRetry_LinkStatus = 0;
	u8	ucDPCD_LaneAlign_Status, ucLaneX_Status;
	u8	ucLane_Index;
	int32_t	iRetVal;
	u32	uiAuxReadInterval_Delay;

	if (pbCR_Done == NULL) {
		dptx_err("Parmter is 0x%p", pbCR_Done);
		return DPTX_RETURN_EINVAL;
	}

	/*
	 * TRAINING_AUX_RD_INTERVAL[0000E]
	 * -.TRAINING_AUX_RD_INTERVAL[Bit6:0]( 0: 400us, 1: 4ms, 2: 8ms, 3: 12ms, 4: 16ms )
	 * -.EXTENDED_RECEIVER_CAPABILITY_FEILD_PRESENT[Bit7]( 0: Not present , 1: Present at DPCD Address 02200h through 022FFh )
	 *
	 *	DPCD_REV[2200] DPCD data structure revision number
	 */ 
	if( bCR_Training )
	{
		uiAuxReadInterval_Delay = 100;
		dptx_dbg("CR Training => delay time to 100us to check CR Status");
	}
	else
	{
		iRetVal = Dptx_Aux_Read_DPCD(dptx, DP_TRAINING_AUX_RD_INTERVAL, &ucDPCD_AuxReadInterval);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		uiAuxReadInterval_Delay = min_t( u32, ( ucDPCD_AuxReadInterval & SINK_TRAINING_AUX_RD_INTERVAL_MASK ), 4 );	
		if(uiAuxReadInterval_Delay == 0) {
			uiAuxReadInterval_Delay = 400;	/* It's to make 400us */
		}
		else {
			uiAuxReadInterval_Delay *= 4000;	/* It's to make ms */
		}

		dptx_dbg("EQ Training => DP_TRAINING_AUX_RD_INTERVAL(0x%x) => delay time to %dus to check CR Status",  ucDPCD_AuxReadInterval, uiAuxReadInterval_Delay );
	}
	
	udelay( uiAuxReadInterval_Delay );

	/*
	 * LINE0_1_STATUS[00202] & LINE2_3_STATUS[00203]
	 * -.LINE0_CR_DONE[Bit0]
	 * -.LINE0_CHANNEL_EQ_DONE[Bit1]
	 * -.LINE0_SYMBOL_LOCKED[Bit2]
	 * -.RESERVED[Bit3]
	 * -.LINE1_CR_DONE[Bit4]
	 * -.LINE1_CHANNEL_EQ_DONE[Bit5]
	 * -.LINE1_SYMBOL_LOCKED[Bit6]
	 */ 

	do{ 
		iRetVal = Dptx_Aux_Read_DPCD(dptx, DP_LANE_ALIGN_STATUS_UPDATED, &ucDPCD_LaneAlign_Status);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		if (ucDPCD_LaneAlign_Status & DP_LINK_STATUS_UPDATED) {
			dptx_dbg("Link status and Adjust Request updated(0x%x) after %dms", ucDPCD_LaneAlign_Status, ucRetry_LinkStatus );
			break;
		}

		if (ucRetry_LinkStatus == MAX_TRY_SINK_LINK_STATUS_UPDATE) {
			dptx_dbg("Link status and Adjust Request NOT updated(0x%x) for 100ms", ucDPCD_LaneAlign_Status );
		}

		mdelay( 1 );
	}while( ucRetry_LinkStatus++ < MAX_TRY_SINK_UPDATE_STATUS );

	iRetVal = Dptx_Aux_Read_Bytes_From_DPCD(dptx, DP_LANE0_1_STATUS, dptx->stDptxLink.aucTraining_Status, DP_LINK_STATUS_SIZE);
	if(iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

 /*
	printf("\nRead %d bytes Status DPCD from DP_LANE0_1_STATUS[0x202].. num of lanes(%d) =>  ", (u32)DP_LINK_STATUS_SIZE, dptx->stDptxLink.ucNumOfLanes );
				
	for( u8 ucCount = 0; ucCount < DP_LINK_STATUS_SIZE; ucCount++ )
		{
		printf(" 0x%02x", dptx->stDptxLink.aucTraining_Status[ucCount]);

		if(( ucCount != 0 ) && ( ucCount % 16 ) == 0 )
			{
				printf("\n");
			}
		}
		printf("\n");	*/

	*pbCR_Done = true;

	for( ucLane_Index = 0; ucLane_Index < dptx->stDptxLink.ucNumOfLanes; ucLane_Index++ )
	{
		ucLaneX_Status = Drm_Addition_Get_Lane_Status( dptx->stDptxLink.aucTraining_Status, ucLane_Index );	
		if((ucLaneX_Status & DP_LANE_CR_DONE) == 0)
		{
			*pbCR_Done = false;
		}
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_adjust_clock_recovery( struct Dptx_Params *pstDptx )
{
	u8	ucDPCD_Adjust_Request_LaneX, ucLane_Index;
	u8	aucDPCD_Adjusted_Value[DPTX_MAX_LINK_LANES] = { 0, };
	u8	aucSink_ConfigValues[DPTX_MAX_LINK_LANES] = { 0, };
	int32_t	iRetVal;

	/*
	 * ADJUST_REQUESTLINE0_1_STATUS[00206] & ADJUST_REQUESTLINE0_1_STATUS[00207]: Voltage Swing and Equalization Setting Adjust Request for Lane 0 and 1.
	 *							When this register is read and found to have changed from a previous read, both during Link Training( including the POST_LT_ADJ_REQ phase )
	 *							and automated PHY Layer testing, the DPTX shall write the voltage swing and pre-emphasis level requested by this register to be the
	 *							TRAINING_LANEx_SET register( DPCD Address 00103h through 00106h )
	 * -.VOLTAGE_SWING_LINE0[Bit1:0]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3 )
	 * -.PRE-EMPHASIS_LINE0[Bit3:2]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3 )
	 * -.VOLTAGE_SWING_LINE1[Bit5:4]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3 )
	 * -.PRE-EMPHASIS_LINE1[Bit7:6]( 0: Level 0, 1: Level 1, 2: Level 2, 3: Level 3 )
	 */ 
	switch( pstDptx->stDptxLink.ucNumOfLanes ) 
	{
		case 4:
			iRetVal = Dptx_Aux_Read_DPCD( pstDptx, DP_ADJUST_REQUEST_LANE2_3, &ucDPCD_Adjust_Request_LaneX );
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				return iRetVal;
			}
			aucDPCD_Adjusted_Value[2] = ( ucDPCD_Adjust_Request_LaneX & 0x0F );
			aucDPCD_Adjusted_Value[3] = (( ucDPCD_Adjust_Request_LaneX & 0xF0 ) >> 4 );
			
			iRetVal = Dptx_Aux_Read_DPCD( pstDptx, DP_ADJUST_REQUEST_LANE0_1, &ucDPCD_Adjust_Request_LaneX );
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				return iRetVal;
			}
			aucDPCD_Adjusted_Value[0] = ( ucDPCD_Adjust_Request_LaneX & 0x0F );
			aucDPCD_Adjusted_Value[1] = (( ucDPCD_Adjust_Request_LaneX & 0xF0 ) >> 4 );

 			dptx_dbg("LANE0(V level %d, PreEm level %d), LANE1(V level %d, PreEm level %d), LANE2(V level %d, PreEm level %d), LANE3(V level %d, PreEm level %d)", 
						( aucDPCD_Adjusted_Value[0] & 0x03 ), ( aucDPCD_Adjusted_Value[0] & 0x0C ) >> 2, ( aucDPCD_Adjusted_Value[1] & 0x03 ), ( aucDPCD_Adjusted_Value[1] & 0x0C ) >> 2,
						( aucDPCD_Adjusted_Value[2] & 0x03 ), ( aucDPCD_Adjusted_Value[2] & 0x0C ) >> 2, ( aucDPCD_Adjusted_Value[3] & 0x03 ), ( aucDPCD_Adjusted_Value[3] & 0x0C ) >> 2 );
			break;
		case 2:
		case 1:
			iRetVal = Dptx_Aux_Read_DPCD( pstDptx, DP_ADJUST_REQUEST_LANE0_1, &ucDPCD_Adjust_Request_LaneX );
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				return iRetVal;
			}
			aucDPCD_Adjusted_Value[0] = ( ucDPCD_Adjust_Request_LaneX & 0x0F );
			aucDPCD_Adjusted_Value[1] = ( ( ucDPCD_Adjust_Request_LaneX & 0xF0 ) >> 4 );

			dptx_dbg("LANE0(V level %d, PreEm level%d), LANE1(V level%d, PreEm level%d)", 
						( aucDPCD_Adjusted_Value[0] & 0x03 ), ( aucDPCD_Adjusted_Value[0] & 0x0C ) >> 2, ( aucDPCD_Adjusted_Value[1] & 0x03 ), ( aucDPCD_Adjusted_Value[1] & 0x0C ) >> 2 );
			break;
		default:
			dptx_err( "Invalid number of lanes %d ", pstDptx->stDptxLink.ucNumOfLanes);
			return DPTX_RETURN_EINVAL;
	}

	for( ucLane_Index = 0; ucLane_Index < pstDptx->stDptxLink.ucNumOfLanes; ucLane_Index++ ) 
	{
		pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] = ( aucDPCD_Adjusted_Value[ucLane_Index] & DP_TRAIN_VOLTAGE_LEVEL_MASK );
		pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] = ( ( aucDPCD_Adjusted_Value[ucLane_Index] & DP_TRAIN_PRE_EMPHASIS_LEVEL_MASK ) >> 2 );

		iRetVal = Dptx_Core_Set_PHY_PreEmphasis( pstDptx, ucLane_Index, (enum PHY_PRE_EMPHASIS_LEVEL)pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] );
		if(iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = Dptx_Core_Set_PHY_VSW( pstDptx, ucLane_Index, (enum PHY_VOLTAGE_SWING_LEVEL)pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] );
		if(iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		aucSink_ConfigValues[ucLane_Index] = 0;

		aucSink_ConfigValues[ucLane_Index] |= (( pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] << DP_TRAIN_VOLTAGE_SWING_SHIFT ) & DP_TRAIN_VOLTAGE_SWING_MASK );
		if( pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] == DP_TRAIN_VOLTAGE_SWING_LEVEL_3 )			
		{
			dptx_dbg("Lane %d VSW reached to level 3", ucLane_Index );
			aucSink_ConfigValues[ucLane_Index] |= DP_TRAIN_MAX_SWING_REACHED;
		}

		aucSink_ConfigValues[ucLane_Index] |= ((pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] <<  DP_TRAIN_PRE_EMPHASIS_SHIFT) & DP_TRAIN_PRE_EMPHASIS_MASK );
		if( pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] == ( DP_TRAIN_PRE_EMPH_LEVEL_3 >> 3 ))
		{
			dptx_dbg("Lane %d Pre-emphasis reached to level 3", ucLane_Index );
			aucSink_ConfigValues[ucLane_Index] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
		}
	}

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_TRAINING_LANE0_SET, aucSink_ConfigValues, pstDptx->stDptxLink.ucNumOfLanes);
	if(iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_get_ch_equalization_status( struct Dptx_Params *pstDptx, bool *pbCR_Done, bool *pbEQ_Done )
{
	int32_t	iRetVal;

	iRetVal = dptx_link_get_clock_recovery_status( pstDptx, DP_TRAIN_CHANNEL_EQ, pbEQ_Done );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	*pbEQ_Done = Drm_Addition_Get_Channel_EQ_Status( pstDptx->stDptxLink.aucTraining_Status, pstDptx->stDptxLink.ucNumOfLanes);

	if(( *pbCR_Done )   && ( *pbEQ_Done ))
	{
		dptx_dbg("***CH_CR_DONE => Done, CH_EQ_DONE => Done");
	}
	else
	{
		dptx_dbg("***CH_CR_DONE => %d, CH_EQ_DONE => %d", *pbCR_Done, *pbEQ_Done);
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_reduce_rate( struct Dptx_Params *pstDptx, bool *pbRate_Reduced )
{
	u8		ucLinkRate;

	*pbRate_Reduced = true;

	switch( pstDptx->stDptxLink.ucLinkRate ) 
	{
		case DPTX_PHYIF_CTRL_RATE_RBR:
			*pbRate_Reduced = false;
			dptx_warn("Rate is reached to RBR ");
			return DPTX_RETURN_NO_ERROR;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR:
			ucLinkRate = DPTX_PHYIF_CTRL_RATE_RBR;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			ucLinkRate = DPTX_PHYIF_CTRL_RATE_HBR;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			ucLinkRate = DPTX_PHYIF_CTRL_RATE_HBR2;
			break;
		default:
			*pbRate_Reduced = false;
			dptx_err("Invalid PHY rate %d\n", pstDptx->stDptxLink.ucLinkRate);
			return DPTX_RETURN_EINVAL;
			break;
	}

	dptx_warn(" Reducing rate from %s to %s", 
				pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3",
				ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3" );

	pstDptx->stDptxLink.ucLinkRate = ucLinkRate;

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_perform_clock_recovery( struct Dptx_Params *pstDptx, bool *pbCR_Done )
{
	u8	ucLaneCount;
	u8	ucCR_Try;
	int32_t	iRetVal;

	dptx_dbg("Training clock_recovery with Setting to TPS 1 in PHY and Pattern Sequence 1 in Sink... ");

	iRetVal = Dptx_Core_Set_PHY_Pattern( pstDptx, DPTX_PHYIF_CTRL_TPS_1 );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	/*
	 * TRAINING_PATTERN_SET[0102]: TRAINING_PATTERN_SET 
	 * -.TRAINING_PATTERN_SELECT[Bit3:0]( 0: Training not in progress, 1: Pattern Sequence 1, 2: Pattern Sequence 2, 3: Pattern Sequence 3, 4: Pattern Sequence 4 )
	 * -.SCRAMBLING_DISABLE[Bit5]:( 0: scrambled data symbols before transmission, 1: Disable scrambler )
	 */ 
	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_TRAINING_PATTERN_SET, DP_TRAINING_PATTERN_1);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = dptx_link_get_clock_recovery_status( pstDptx, DP_TRAIN_CLOCK_RECOVERY, pbCR_Done );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	if( *pbCR_Done )
	{
		dptx_dbg("Clock Recovery has been done at once  !!!");
		return DPTX_RETURN_NO_ERROR;
	}

	for( ucCR_Try = 0; ucCR_Try < MAX_TRY_CLOCK_RECOVERY; ucCR_Try++ ) 	/* Try each adjustment setting 5 times */
	{
		iRetVal = dptx_link_adjust_clock_recovery( pstDptx );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		for( ucLaneCount = 0; ucLaneCount < pstDptx->stDptxLink.ucNumOfLanes; ucLaneCount++ ) 
		{
			if( pstDptx->stDptxLink.aucVoltageSwing_level[ucLaneCount] != DP_TRAIN_VOLTAGE_SWING_LEVEL_3 ) 
			{
				break;
			}
		}

		if( ucLaneCount == pstDptx->stDptxLink.ucNumOfLanes )
		{
			dptx_err("All %d Lanes are reached to level 3", ucLaneCount );
			return DPTX_RETURN_ENODEV;
		}

		iRetVal = dptx_link_get_clock_recovery_status( pstDptx, DP_TRAIN_CLOCK_RECOVERY, pbCR_Done );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		if( *pbCR_Done )
		{
			dptx_dbg("Clock Recovery has been done after %d retrials !!!", ucCR_Try);
			return DPTX_RETURN_NO_ERROR;
		}
	}

	dptx_warn("Clock Recovery fails." );

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_perform_ch_equalization( struct Dptx_Params *pstDptx, bool *pbCR_Done, bool *pbEQ_Done )
{
	u8	ucSink_Pattern, ucDPCD_LaneAlign_Status, ucRetry_SinkStatus = 0;
	u8	ucEQ_Try;
	int32_t	iRetVal;
	unsigned int	uiPHY_Ppattern;

	switch( pstDptx->ucMax_Rate ) 
	{
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			if (Drm_dp_tps4_supported( pstDptx->aucDPCD_Caps)) {
				uiPHY_Ppattern = DPTX_PHYIF_CTRL_TPS_4;
				ucSink_Pattern = DP_TRAINING_PATTERN_4;
				break;
			}
			if (Drm_dp_tps3_supported( pstDptx->aucDPCD_Caps)) {
				uiPHY_Ppattern	= DPTX_PHYIF_CTRL_TPS_3;
				ucSink_Pattern	= DP_TRAINING_PATTERN_3;
				break;
			}
			uiPHY_Ppattern	= DPTX_PHYIF_CTRL_TPS_2;
			ucSink_Pattern	= DP_TRAINING_PATTERN_2;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			if (Drm_dp_tps3_supported( pstDptx->aucDPCD_Caps)) {
				uiPHY_Ppattern	= DPTX_PHYIF_CTRL_TPS_3;
				ucSink_Pattern	= DP_TRAINING_PATTERN_3;
				break;
			}
			uiPHY_Ppattern	= DPTX_PHYIF_CTRL_TPS_2;
			ucSink_Pattern	= DP_TRAINING_PATTERN_2;
			break;
		case DPTX_PHYIF_CTRL_RATE_RBR:
		case DPTX_PHYIF_CTRL_RATE_HBR:
			uiPHY_Ppattern	= DPTX_PHYIF_CTRL_TPS_2;
			ucSink_Pattern	= DP_TRAINING_PATTERN_2;
			break;
		default:
			dptx_err("Invalid rate %d ", pstDptx->stDptxLink.ucLinkRate );
			return DPTX_RETURN_ENODEV;
	}

	dptx_dbg("PHY Pattern(0x%x), Sink Pattern(0x%x)... ", uiPHY_Ppattern, ucSink_Pattern);

	iRetVal = Dptx_Core_Set_PHY_Pattern(pstDptx, uiPHY_Ppattern);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_TRAINING_PATTERN_SET, ucSink_Pattern);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = dptx_link_get_ch_equalization_status(pstDptx, pbCR_Done, pbEQ_Done);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	if( *pbCR_Done == false ) 
	{
		dptx_warn("Clock recovery fails on EQ Training. ");
		return DPTX_RETURN_NO_ERROR;;
	}

	if( *pbEQ_Done )
	{
		dptx_dbg("Channel equalization has been done at once !!!");
		return DPTX_RETURN_NO_ERROR;;
	}

	for( ucEQ_Try = 0; ucEQ_Try < MAX_TRY_CHANNEL_EQ; ucEQ_Try++ ) 
	{
		iRetVal = dptx_link_adjust_clock_recovery( pstDptx );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = dptx_link_get_ch_equalization_status( pstDptx, pbCR_Done, pbEQ_Done );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
		if( *pbCR_Done == false ) 
		{
			dptx_warn("Clock recovery fails on EQ Training ");
			return DPTX_RETURN_NO_ERROR;
		}

		if( *pbEQ_Done )
		{
			do{
				iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_LANE_ALIGN_STATUS_UPDATED, &ucDPCD_LaneAlign_Status);
				if (iRetVal != DPTX_RETURN_NO_ERROR) {
					return iRetVal;
				}

				if (ucDPCD_LaneAlign_Status & DP_LINK_STATUS_UPDATED) {
					dptx_dbg("Link status and Adjust Request updated(0x%x) after %dms", ucDPCD_LaneAlign_Status, ucRetry_SinkStatus );
					break;
				}

				if( ucRetry_SinkStatus == MAX_TRY_SINK_LINK_STATUS_UPDATE )
				{
					dptx_dbg("Link status and Adjust Request NOT updated(0x%x) for 100ms", ucDPCD_LaneAlign_Status );
				}

				mdelay( 1 ); 
			}while( ucRetry_SinkStatus++ < MAX_TRY_SINK_UPDATE_STATUS );
		}
	}

	if( *pbEQ_Done == false )
	{
		dptx_warn("Channel equalization fails" );
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_initiate_training( struct Dptx_Params *pstDptx )
{
	bool	bSink_EnhancedFraming;
	u8	ucSink_BW = 0, ucSink_LaneCountSet = 0, ucSink_DownSpread = 0, ucSink_ANSI_Coding = 0, ucSink_TrainingSet = 0;
	u8	ucLane_Index;
	int32_t	iRetVal;
	u32				uiRegMap_Cctl;

	dptx_dbg("Starting... Num of lanes(%d), Link rate(%d)", pstDptx->stDptxLink.ucNumOfLanes, pstDptx->stDptxLink.ucLinkRate );

	iRetVal = Dptx_Core_Set_PHY_NumOfLanes( pstDptx, (u8)pstDptx->stDptxLink.ucNumOfLanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

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

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, (u8)pstDptx->stDptxLink.ucNumOfLanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_Rate( pstDptx, ( enum PHY_LINK_RATE )pstDptx->stDptxLink.ucLinkRate );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_PowerState( pstDptx, PHY_POWER_ON );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status( pstDptx, (u8)pstDptx->stDptxLink.ucNumOfLanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	for(  ucLane_Index = 0; ucLane_Index < pstDptx->stDptxLink.ucNumOfLanes; ucLane_Index++ ) 
	{
		ucSink_TrainingSet = 0;
		
		iRetVal = Dptx_Core_Set_PHY_PreEmphasis( pstDptx, ucLane_Index, (enum PHY_PRE_EMPHASIS_LEVEL)pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] );
		if(iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
		
		iRetVal = Dptx_Core_Set_PHY_VSW( pstDptx, ucLane_Index, (enum PHY_VOLTAGE_SWING_LEVEL)pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] );
		if(iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		ucSink_TrainingSet |= pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index];
		ucSink_TrainingSet |= ( pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] << DP_TRAIN_PRE_EMPHASIS_SHIFT );
		
		iRetVal = Dptx_Aux_Write_DPCD(pstDptx, (DP_TRAINING_LANE0_SET + ucLane_Index), ucSink_TrainingSet);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
	}

	iRetVal = Dptx_Core_Set_PHY_Pattern( pstDptx, DPTX_PHYIF_CTRL_TPS_NONE );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_TRAINING_PATTERN_SET, DP_TRAINING_PATTERN_DISABLE);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Enable_PHY_XMIT( pstDptx, pstDptx->stDptxLink.ucNumOfLanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	/*
	 * LINK_BW_SET[00100]: Maximum link rate of Main lanes = Valeu x 0.27Gbps/lane
	 *
	 * -.MAX_LINK_RATE[Bit7:0]( 06h: 1.62Gbps/lane, 0Ah: 2.7Gbps/lane, 14h: 5.4Gbps/lane, 1Eh: 8.1Gbps/lane)
	 */ 
	iRetVal = Dptx_Core_PHY_Rate_To_Bandwidth( pstDptx, pstDptx->stDptxLink.ucLinkRate, &ucSink_BW );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}
	
	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_LINK_BW_SET, ucSink_BW);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	/*
	 * LANE_COUNT[00101]: Maximum Link Lane Count
	 *
	 * -.LANE_COUNT_SET[Bit4:0]( 1h: 1 lane, 2h: 2 lanes, 4h: 4 lanes )
	 * -.ENHANCED_FRAME_EN[Bit7]( 0: Enhanced Framing symbol sequence is NOT enabled,  1: Enabled )
	 */ 
	ucSink_LaneCountSet = pstDptx->stDptxLink.ucNumOfLanes;

	bSink_EnhancedFraming = Drm_dp_enhanced_frame_cap( pstDptx->aucDPCD_Caps );
	if( bSink_EnhancedFraming ) 
	{
		ucSink_LaneCountSet |= DP_ENHANCED_FRAME_CAP;
	}

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
	if( bSink_EnhancedFraming ) 
	{
		uiRegMap_Cctl |= DPTX_CCTL_ENH_FRAME_EN;
	} 
	else 
	{
		uiRegMap_Cctl &= ~DPTX_CCTL_ENH_FRAME_EN;
	}

	Dptx_Reg_Writel( pstDptx, DPTX_CCTL, uiRegMap_Cctl );
	
	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_LANE_COUNT_SET, ucSink_LaneCountSet);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	/* Synopsys -> C10 PHY ...
	 *
	 * DOWNSPREAD_CTRL[00107]: Down-spreqading control
	 * -.Reserved[Bit3:0]
 	 * -.SPREAD_AMP[Bit4]( 0: Main-Link signal is NOT down-spread, 1: Main-Link signal is down-spread by equal or less than 0.5% with a modulation Freq. in the range of 30 to 33Khz )
	 */ 
	if( pstDptx->bSpreadSpectrum_Clock && ( pstDptx->aucDPCD_Caps[DP_MAX_DOWNSPREAD] & SINK_TDOWNSPREAD_MASK )) 
	{
		ucSink_DownSpread = DP_SPREAD_AMP_0_5;
	}

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_DOWNSPREAD_CTRL, ucSink_DownSpread);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	/* 
	 * MAIN_LINK_CHANNEL_CODING_SET[00108]: Shall be programmed to 01h upon power-on reset or an upstream device disconnect.
	 */ 

	ucSink_ANSI_Coding = DP_SET_ANSI_8B10B;
	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_MAIN_LINK_CHANNEL_CODING_SET, ucSink_ANSI_Coding);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_init_training( struct Dptx_Params *pstDptx, u8 ucLink_Rate, u8 ucNumOfLanes )
{
	u8	ucSink_Max_Rate, ucSink_Max_Lanes;
	int32_t	iRetVal;

	if( ucLink_Rate > DPTX_PHYIF_CTRL_RATE_HBR3 )
	{
		dptx_err("Invalid rate = %d to %d", ucLink_Rate, DPTX_DEFAULT_LINK_RATE  );
		
		ucLink_Rate = DPTX_DEFAULT_LINK_RATE;
	}
	if(( ucNumOfLanes == 0 ) || ( ucNumOfLanes == 3 ) || ( ucNumOfLanes > 4 ))
	{
		dptx_err("Invalid lanes = %d to 4", ucNumOfLanes );
		
		ucNumOfLanes = 4;
	}

	memset( pstDptx->stDptxLink.aucPreEmphasis_level, 0, sizeof(u8) * MAX_PRE_EMPHASIS_LEVEL );
	memset( pstDptx->stDptxLink.aucVoltageSwing_level, 0, sizeof(u8) * MAX_VOLTAGE_SWING_LEVEL );
	memset( pstDptx->stDptxLink.aucTraining_Status, 0, DP_LINK_STATUS_SIZE );

	ucSink_Max_Lanes = Drm_dp_max_lane_count( pstDptx->aucDPCD_Caps );

	/*
	 * MAX_LINK_RATE[00001]: Maximum link rate of Main lanes = Valeu x 0.27Gbps/lane
	 *
	 * -.MAX_LINK_RATE[Bit7:0]( 06h: 1.62Gbps/lane, 0Ah: 2.7Gbps/lane, 14h: 5.4Gbps/lane, 1Eh: 8.1Gbps/lane)
	 */ 
	iRetVal = Dptx_Core_PHY_Bandwidth_To_Rate( pstDptx, pstDptx->aucDPCD_Caps[DP_MAX_LINK_RATE], &ucSink_Max_Rate );
	if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
	}

	dptx_dbg( "Init link training :" );
	dptx_dbg( "  -.Source device %d lanes <-> Sink device %d lanes", (u32)ucNumOfLanes, (u32)ucSink_Max_Lanes  );
	dptx_dbg( "  -.Source rate = %s  <-> Sink rate = %s", 
				ucLink_Rate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( ucLink_Rate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( ucLink_Rate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3",
				ucSink_Max_Rate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( ucSink_Max_Rate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( ucSink_Max_Rate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3" );

	if( ucNumOfLanes > ucSink_Max_Lanes )
	{
		ucNumOfLanes = ucSink_Max_Lanes;
	}
	if( ucLink_Rate > ucSink_Max_Rate )
	{
		ucLink_Rate = ucSink_Max_Rate;
	}

	pstDptx->stDptxLink.ucNumOfLanes	= ucNumOfLanes;
	pstDptx->stDptxLink.ucLinkRate		= ucLink_Rate;

	if( pstDptx->ucMax_Rate != pstDptx->stDptxLink.ucLinkRate )
	{
		dptx_warn("Reducing Link rate = %s -> Sink one = %s ", 
				pstDptx->ucMax_Rate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( pstDptx->ucMax_Rate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( pstDptx->ucMax_Rate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3",
				pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR":( pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR ) ?  "HBR":( pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2 ) ? "HB2":"HBR3");

		iRetVal = Dptx_Platform_Set_RegisterBank( pstDptx, (enum PHY_LINK_RATE)pstDptx->stDptxLink.ucLinkRate );
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = Dptx_Core_Init( pstDptx );
		if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = Dptx_Ext_Set_Stream_Capability( pstDptx );
		if(iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Link_Perform_Training( struct Dptx_Params *pstDptx, u8 ucRate, u8 ucNumOfLanes )
{
	bool	bCR_Done = false, bEQ_Done = false, bRate_Reduced = false;
	u8			ucDPCD_SinkCount, ucMST_Mode_Caps;
	int32_t	iRetVal;

	iRetVal = dptx_link_init_training( pstDptx, ucRate, ucNumOfLanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		goto fail;
	}

again:
	iRetVal = dptx_link_initiate_training( pstDptx );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		goto fail;
	}

	iRetVal = dptx_link_perform_clock_recovery( pstDptx, &bCR_Done );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		goto fail;
	}

	if( !bCR_Done ) 
	{
		iRetVal = dptx_link_reduce_rate( pstDptx, &bRate_Reduced );
		if ((iRetVal != DPTX_RETURN_NO_ERROR) || (!bRate_Reduced)) {
	        	goto fail;
         	}

		iRetVal = dptx_link_init_training( pstDptx, pstDptx->stDptxLink.ucLinkRate, pstDptx->stDptxLink.ucNumOfLanes );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			goto fail;
		}

		goto again;
	}

	iRetVal = dptx_link_perform_ch_equalization( pstDptx, &bCR_Done, &bEQ_Done );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		goto fail;
	}

	if( !bCR_Done ) 
	{ 
        	iRetVal = dptx_link_reduce_rate( pstDptx, &bRate_Reduced );
	       	if ((iRetVal != DPTX_RETURN_NO_ERROR) || (!bRate_Reduced)) {
			goto fail;
		}

        	iRetVal = dptx_link_init_training( pstDptx, pstDptx->stDptxLink.ucLinkRate, pstDptx->stDptxLink.ucNumOfLanes );
        	if (iRetVal != DPTX_RETURN_NO_ERROR) {
	        	goto fail;
	        }

		goto again;
	} 
	else if( !bEQ_Done ) 
	{ 
		iRetVal = dptx_link_reduce_rate( pstDptx, &bRate_Reduced  );
		if ((iRetVal != DPTX_RETURN_NO_ERROR) || (!bRate_Reduced)) {
			goto fail;
		}

		iRetVal = dptx_link_init_training( pstDptx, pstDptx->stDptxLink.ucLinkRate, pstDptx->stDptxLink.ucNumOfLanes );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			goto fail;
		}

		goto again;
	}

	iRetVal = Dptx_Core_Set_PHY_Pattern(pstDptx, DPTX_PHYIF_CTRL_TPS_NONE);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_TRAINING_PATTERN_SET, DP_TRAINING_PATTERN_DISABLE);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_MSTM_CAP, &ucMST_Mode_Caps);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Enable_PHY_XMIT(pstDptx, pstDptx->stDptxLink.ucNumOfLanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_SINK_COUNT, &ucDPCD_SinkCount);/* Branch device detection */
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, 0x2002, &ucDPCD_SinkCount);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	dptx_info("[INF][DP V14]Link training succeeded with rate=%d lanes=%d !!!", pstDptx->stDptxLink.ucLinkRate, pstDptx->stDptxLink.ucNumOfLanes );
	return DPTX_RETURN_NO_ERROR;

fail:
	Dptx_Core_Set_PHY_Pattern( pstDptx, DPTX_PHYIF_CTRL_TPS_NONE );
	Dptx_Aux_Write_DPCD( pstDptx, DP_TRAINING_PATTERN_SET, DP_TRAINING_PATTERN_DISABLE );

	dptx_err("Failed link training !!!" );

	return DPTX_RETURN_ENODEV;
}

int32_t Dptx_Link_Perform_BringUp( struct Dptx_Params *pstDptx, bool bSink_MST_Supported )
{
	bool	bSink_SSC_Profiled;
	int32_t	iRetVal;
	u32	uiRegMap_Cctl;

	iRetVal = Dptx_Core_Set_PHY_NumOfLanes( pstDptx, (u8)pstDptx->ucMax_Lanes );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Aux_Write_DPCD( pstDptx, DP_SET_POWER, DP_SET_POWER_D0 );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_Sink_SSC_Capability( pstDptx, &bSink_SSC_Profiled );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_SSC( pstDptx, bSink_SSC_Profiled );
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	if( bSink_MST_Supported ) 
	{
		uiRegMap_Cctl = Dptx_Reg_Readl( pstDptx, DPTX_CCTL );
		uiRegMap_Cctl |= DPTX_CCTL_ENABLE_MST_MODE;
		Dptx_Reg_Writel( pstDptx, DPTX_CCTL, uiRegMap_Cctl);

		iRetVal = Dptx_Aux_Write_DPCD( pstDptx, DP_MSTM_CTRL, ( DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC ) );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
	}
	else
	{
		uiRegMap_Cctl = Dptx_Reg_Readl( pstDptx, DPTX_CCTL );
		uiRegMap_Cctl &= ~DPTX_CCTL_ENABLE_MST_MODE;
		Dptx_Reg_Writel( pstDptx, DPTX_CCTL, uiRegMap_Cctl);

		iRetVal = Dptx_Aux_Write_DPCD( pstDptx, DP_MSTM_CTRL, ~( DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC ) );
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
	}

	memset( pstDptx->aucDPCD_Caps, 0, DPTX_SINK_CAP_SIZE );

	iRetVal = Dptx_Aux_Read_Bytes_From_DPCD( pstDptx, DP_DPCD_REV, pstDptx->aucDPCD_Caps, DPTX_SINK_CAP_SIZE );
	if(iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	if( pstDptx->aucDPCD_Caps[DP_TRAINING_AUX_RD_INTERVAL] & DP_EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT ) 
	{
		dptx_dbg("Sink has extended receiver capability... read from 0x2200" );

		iRetVal = Dptx_Aux_Read_Bytes_From_DPCD( pstDptx, 0x2200, pstDptx->aucDPCD_Caps, DPTX_SINK_CAP_SIZE );
		if(iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
	}

	dptx_dbg("Sink DP Revision %x.%x ", (pstDptx->aucDPCD_Caps[0] & 0xF0) >> 4, pstDptx->aucDPCD_Caps[0] & 0xF );

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Link_Get_LinkTraining_Status( struct Dptx_Params *pstDptx, bool *pbTrainingState )
{
	bool	bChannel_Eq_Status, bCR_Status;
	int32_t	iRetVal;

	iRetVal = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_LANE0_1_STATUS, pstDptx->stDptxLink.aucTraining_Status, DP_LINK_STATUS_SIZE);
	if(iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	dptx_dbg("Training status: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", 
		pstDptx->stDptxLink.aucTraining_Status[0], pstDptx->stDptxLink.aucTraining_Status[1],
		pstDptx->stDptxLink.aucTraining_Status[2], pstDptx->stDptxLink.aucTraining_Status[3],
		pstDptx->stDptxLink.aucTraining_Status[4], pstDptx->stDptxLink.aucTraining_Status[5] );

	bCR_Status = Drm_Addition_Get_Clock_Recovery_Status( pstDptx->stDptxLink.aucTraining_Status, pstDptx->ucMax_Lanes );
	if(bCR_Status) {
		bChannel_Eq_Status = Drm_Addition_Get_Channel_EQ_Status( pstDptx->stDptxLink.aucTraining_Status, pstDptx->ucMax_Lanes ); 
		if(bChannel_Eq_Status) {
			dptx_dbg("CR and EQ are done");
			*pbTrainingState = true;
		}
		else {
			dptx_dbg("Channel EQ is not done... ");
			*pbTrainingState = false;
		}
	}
	else {
		dptx_dbg("CR is not done... ");
		*pbTrainingState = false;
	}

	return DPTX_RETURN_NO_ERROR;
}
