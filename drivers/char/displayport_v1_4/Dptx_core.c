/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
*/

/*
* Modified by Telechips Inc.
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


static int32_t dptx_core_check_vendor_id(struct Dptx_Params *pstDptx)
{
	u32				uiDptx_id;

	uiDptx_id = Dptx_Reg_Readl(pstDptx, DPTX_ID);
	if (uiDptx_id != (u32)((DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT) | DPTX_ID_VENDOR_ID)) {
		dptx_err("Invalid DPTX Id : 0x%x<->0x%x", uiDptx_id, ((DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT) | DPTX_ID_VENDOR_ID));
		return DPTX_RETURN_EINVAL;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Link_Power_On(struct Dptx_Params *pstDptx)
{
	int32_t	iRetVal;

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_ON);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Init_Params(struct Dptx_Params *pstDptx)
{
	int32_t	iRetVal;

	iRetVal = Dptx_Core_Get_PHY_NumOfLanes(pstDptx, &pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		pstDptx->ucMax_Lanes = (u8)PHY_LANE_4;

	pstDptx->stDptxLink.ucNumOfLanes = pstDptx->ucMax_Lanes;

	iRetVal = Dptx_Core_Get_PHY_Rate(pstDptx, &pstDptx->ucMax_Rate);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		pstDptx->ucMax_Rate  = (u8)LINK_RATE_HBR3;

	pstDptx->stDptxLink.ucLinkRate  = (u8)pstDptx->ucMax_Rate;

	iRetVal = Dptx_Core_Get_Stream_Mode(pstDptx, &pstDptx->bMultStreamTransport);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		pstDptx->bMultStreamTransport = false;

	iRetVal = Dptx_Core_Get_PHY_SSC(pstDptx, &pstDptx->bSpreadSpectrum_Clock);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		pstDptx->bSpreadSpectrum_Clock = true;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Init(struct Dptx_Params *pstDptx)
{
	char	aucVerStr[15];
	int32_t	iRetVal;
	u32	uiDptx_Version, uiRegMap_HPD_IEN, uiRegMap_HDCP_INTR, uiRegMap_TYPEC_CTRL, uiRegMap_Cctl;

	iRetVal = dptx_core_check_vendor_id(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	Dptx_Core_Disable_Global_Intr(pstDptx);

	Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_ALL);

	memset(aucVerStr, 0, sizeof(aucVerStr));

	uiDptx_Version = Dptx_Reg_Readl(pstDptx, DPTX_VER_NUMBER);
	aucVerStr[0] = (uiDptx_Version >> 24) & 0xff;
	aucVerStr[1] = '.';
	aucVerStr[2] = (uiDptx_Version >> 16) & 0xff;
	aucVerStr[3] = (uiDptx_Version >> 8) & 0xff;
	aucVerStr[4] = (uiDptx_Version & 0xff);

	uiDptx_Version = Dptx_Reg_Readl(pstDptx, DPTX_VER_TYPE);
	aucVerStr[5] = '-';
	aucVerStr[6] = (uiDptx_Version >> 24) & 0xff;
	aucVerStr[7] = (uiDptx_Version >> 16) & 0xff;
	aucVerStr[8] = (uiDptx_Version >> 8) & 0xff;
	aucVerStr[9] = (uiDptx_Version & 0xff);

	dptx_dbg("Core version: %s ", aucVerStr);

	Dptx_Core_Init_PHY(pstDptx);

	uiRegMap_HPD_IEN = Dptx_Reg_Readl(pstDptx, DPTX_HPD_IEN);

	uiRegMap_HPD_IEN |= (DPTX_HPD_IEN_IRQ_EN | DPTX_HPD_IEN_HOT_PLUG_EN | DPTX_HPD_IEN_HOT_UNPLUG_EN | DPTX_HPDSTS_UNPLUG_ERR_EN);
	Dptx_Reg_Writel(pstDptx, DPTX_HPD_IEN, uiRegMap_HPD_IEN);

	uiRegMap_HDCP_INTR = Dptx_Reg_Readl(pstDptx, DPTX_HDCP_API_INT_MSK);
	uiRegMap_HDCP_INTR |= DPTX_HDCP22_GPIOINT;
	Dptx_Reg_Writel(pstDptx, DPTX_HDCP_API_INT_MSK, uiRegMap_HDCP_INTR);

	Dptx_Core_Enable_Global_Intr(pstDptx, (DPTX_IEN_HPD | DPTX_IEN_HDCP | DPTX_IEN_SDP | DPTX_IEN_TYPE_C));

	uiRegMap_TYPEC_CTRL = Dptx_Reg_Readl(pstDptx, DPTX_TYPE_C_CTRL);
	uiRegMap_TYPEC_CTRL &= ~(DPTX_TYPEC_DISABLE_ACK);
	uiRegMap_TYPEC_CTRL &= ~(DPTX_TYPEC_DISABLE_STATUS);
	uiRegMap_TYPEC_CTRL |= DPTX_TYPEC_INTRURPPT_STATUS;

	Dptx_Reg_Writel(pstDptx, DPTX_TYPE_C_CTRL, uiRegMap_TYPEC_CTRL);

	uiRegMap_Cctl = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);

	uiRegMap_Cctl |= DPTX_CCTL_ENH_FRAME_EN;
	uiRegMap_Cctl &= ~DPTX_CCTL_SCALE_DOWN_MODE;
	uiRegMap_Cctl &= ~DPTX_CCTL_FAST_LINK_TRAINED_EN;

	Dptx_Reg_Writel(pstDptx, DPTX_CCTL, uiRegMap_Cctl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Deinit(struct Dptx_Params *pstDptx)
{
	Dptx_Core_Disable_Global_Intr(pstDptx);
	Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_ALL);

	return DPTX_RETURN_NO_ERROR;
}

void Dptx_Core_Init_PHY(struct Dptx_Params *pstDptx)
{
	u32 ucRegMap_PhyCtrl;

	ucRegMap_PhyCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	ucRegMap_PhyCtrl &= ~DPTX_PHYIF_CTRL_WIDTH;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, ucRegMap_PhyCtrl);
}

void Dptx_Core_Soft_Reset(struct Dptx_Params *pstDptx, u32 uiReset_Bits)
{
	u32 ucRegMap_Reset, uiRegMap_BitMask;

	uiRegMap_BitMask = (uiReset_Bits & DPTX_SRST_CTRL_ALL);

	ucRegMap_Reset = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);
	ucRegMap_Reset |= uiRegMap_BitMask;
	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, ucRegMap_Reset);

	udelay(20);

	ucRegMap_Reset = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);
	ucRegMap_Reset &= ~uiRegMap_BitMask;
	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, ucRegMap_Reset);
}

void Dptx_Core_Enable_Global_Intr(struct Dptx_Params *pstDptx, u32 uiEnable_Bits)
{
	u32 uiRegMap_IntEn;

	uiRegMap_IntEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	uiRegMap_IntEn |= uiEnable_Bits;
	Dptx_Reg_Writel(pstDptx, DPTX_IEN, uiRegMap_IntEn);
}

void Dptx_Core_Disable_Global_Intr(struct Dptx_Params *pstDptx)
{
	u32			uiIntEnable;

	uiIntEnable = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	uiIntEnable &= ~DPTX_IEN_ALL_INTR;
	Dptx_Reg_Writel(pstDptx, DPTX_IEN, uiIntEnable);
}

int32_t Dptx_Core_Clear_General_Interrupt(struct Dptx_Params *pstDptx, u32 uiClear_Bits)
{
	u32		ucRegMap_GeneralIntr;

	ucRegMap_GeneralIntr = Dptx_Reg_Readl(pstDptx, DPTX_ISTS);

	ucRegMap_GeneralIntr |= uiClear_Bits;

	Dptx_Reg_Writel(pstDptx, DPTX_ISTS, ucRegMap_GeneralIntr);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_SSC(struct Dptx_Params *pstDptx, bool bSink_Supports_SSC)
{
	int32_t	iRetVal;
	u32	uiRegMap_PhyIfCtrl;

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_REF_CLOCK);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_PHY_CLOCK);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	if (pstDptx->bSpreadSpectrum_Clock && bSink_Supports_SSC)
		uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_SSC_DIS;
	else
		uiRegMap_PhyIfCtrl |= DPTX_PHYIF_CTRL_SSC_DIS;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_ON);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_SSC(struct Dptx_Params *pstDptx, bool *pbSSC_Enabled)
{
	u32 uiRegMap_PhyIfCtrl;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	if (uiRegMap_PhyIfCtrl & DPTX_PHYIF_CTRL_SSC_DIS)
		*pbSSC_Enabled = false;
	else
		*pbSSC_Enabled = true;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_Sink_SSC_Capability(struct Dptx_Params *pstDptx, bool *pbSSC_Profiled)
{
	u8	ucDCDPValue;
	int32_t	iRetVal;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_MAX_DOWNSPREAD, &ucDCDPValue);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	if (ucDCDPValue & SINK_TDOWNSPREAD_MASK) {
		dptx_dbg("SSC enable on the sink side");
		*pbSSC_Profiled = true;
	} else {
		dptx_dbg("SSC disabled on the sink side");
		*pbSSC_Profiled = false;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_Stream_Mode(struct Dptx_Params *pstDptx, bool *pbMST_Mode)
{
	u32	uiRegMap_Cctl;

	uiRegMap_Cctl = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);

	if (uiRegMap_Cctl & DPTX_CCTL_ENABLE_MST_MODE)
		*pbMST_Mode = true;
	else
		*pbMST_Mode = false;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_NumOfLanes(struct Dptx_Params *pstDptx, u8 *pucNumOfLanes)
{
	u8	ucNumOfLanes;
	u32	uiRagMap_PhyIfCtrl;

	uiRagMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	ucNumOfLanes = (u8)((uiRagMap_PhyIfCtrl & DPTX_PHYIF_CTRL_LANES_MASK) >> DPTX_PHYIF_CTRL_LANES_SHIFT);

	*pucNumOfLanes = (1 << ucNumOfLanes);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_NumOfLanes(struct Dptx_Params *pstDptx, u8 ucNumOfLanes)
{
	u8	ucPHY_Lanes;
	u32	uiRegMap_PhyIfCtrl;

	switch (ucNumOfLanes) {
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
	}

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_LANES_MASK;
	uiRegMap_PhyIfCtrl |= (ucPHY_Lanes << DPTX_PHYIF_CTRL_LANES_SHIFT);

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_PowerState(struct Dptx_Params *pstDptx, enum PHY_POWER_STATE ePowerState)
{
	u32	uiRegMap_PhyIfCtrl;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK;

	switch (ePowerState) {
	case PHY_POWER_ON:
	case PHY_POWER_DOWN_SWITCHING_RATE:
	case PHY_POWER_DOWN_PHY_CLOCK:
	case PHY_POWER_DOWN_REF_CLOCK:
		uiRegMap_PhyIfCtrl |= (ePowerState << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT);
		break;
	default:
		dptx_err("Invalid power state: %d\n", (u32)ePowerState);
		return DPTX_RETURN_EINVAL;
	}

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_Rate(struct Dptx_Params *pstDptx, enum PHY_LINK_RATE eRate)
{
	u32	uiPhyIfCtrl;

	uiPhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiPhyIfCtrl &= ~DPTX_PHYIF_CTRL_RATE_MASK;
	uiPhyIfCtrl |= (u32)eRate << DPTX_PHYIF_CTRL_RATE_SHIFT;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiPhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_Rate(struct Dptx_Params *pstDptx, u8 *pucPHY_Rate)
{
	u32	UiRegMap_PHY_IF_Ctrl, uiRate;

	UiRegMap_PHY_IF_Ctrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiRate = (UiRegMap_PHY_IF_Ctrl & DPTX_PHYIF_CTRL_RATE_MASK) >> DPTX_PHYIF_CTRL_RATE_SHIFT;

	*pucPHY_Rate = uiRate;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_BUSY_Status(struct Dptx_Params *pstDptx, u8 ucNumOfLanes)
{
	u32	uiRegMap_PhyIfCtrl, uiBitMask = 0, uiCount = 0;;

	switch (ucNumOfLanes) {
	case PHY_NUM_OF_4_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(3);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(2);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(1);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	case PHY_NUM_OF_2_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(1);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	case PHY_NUM_OF_1_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	default:
		dptx_err("Invalid number of lanes %d", (u32)ucNumOfLanes);
		return DPTX_RETURN_EINVAL;
	}

	do {
		uiRegMap_PhyIfCtrl  = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

		if (!(uiRegMap_PhyIfCtrl & uiBitMask)) {
			dptx_dbg("PHY status droped to 0, lanes = %d, count = %d, mask = 0x%x", ucNumOfLanes, uiCount, uiBitMask);
			break;
		}

		if (uiCount == MAX_NUM_OF_LOOP_PHY_STATUS) {
			dptx_err("PHY BUSY timed out");
			return DPTX_RETURN_ENODEV;
		}

		mdelay(1);
	} while (uiCount++ < MAX_NUM_OF_LOOP_PHY_STATUS);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_PreEmphasis(struct Dptx_Params *pstDptx, unsigned int iLane_Index, enum PHY_PRE_EMPHASIS_LEVEL ePreEmphasisLevel)
{
	u32	uiRegMap_PhyTxEQ;

	if (iLane_Index > (u32)PHY_LANE_ID_3) {
		dptx_err("Invalid lane %d ", iLane_Index);
		return DPTX_RETURN_EINVAL;
	}

	if (ePreEmphasisLevel > (u32)PRE_EMPHASIS_LEVEL_3) {
		dptx_err("Invalid pre-emphasis level %d, using 3 ", ePreEmphasisLevel);
		ePreEmphasisLevel = PRE_EMPHASIS_LEVEL_3;
	}

	uiRegMap_PhyTxEQ = Dptx_Reg_Readl(pstDptx, DPTX_PHY_TX_EQ);
	uiRegMap_PhyTxEQ &= ~(DPTX_PHY_TX_EQ_PREEMP_MASK(iLane_Index));
	uiRegMap_PhyTxEQ |= ((u32)ePreEmphasisLevel << DPTX_PHY_TX_EQ_PREEMP_SHIFT(iLane_Index)) & DPTX_PHY_TX_EQ_PREEMP_MASK(iLane_Index);

	Dptx_Reg_Writel(pstDptx, DPTX_PHY_TX_EQ, uiRegMap_PhyTxEQ);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_VSW(struct Dptx_Params *pstDptx, unsigned int iLane_Index, enum PHY_VOLTAGE_SWING_LEVEL eVoltageSwingLevel)
{
	u32	uiRegMap_PhyTxEQ;

	if (iLane_Index > (u32)PHY_LANE_ID_3) {
		dptx_err("Invalid lane %d ", iLane_Index);
		return DPTX_RETURN_EINVAL;
	}

	if (eVoltageSwingLevel > VOLTAGE_SWING_LEVEL_3) {
		dptx_err("Invalid vswing level %d, using 3 ", eVoltageSwingLevel);
		eVoltageSwingLevel = VOLTAGE_SWING_LEVEL_3;
	}

	uiRegMap_PhyTxEQ = Dptx_Reg_Readl(pstDptx, DPTX_PHY_TX_EQ);
	uiRegMap_PhyTxEQ &= ~(DPTX_PHY_TX_EQ_VSWING_MASK(iLane_Index));
	uiRegMap_PhyTxEQ |= ((u32)eVoltageSwingLevel << DPTX_PHY_TX_EQ_VSWING_SHIFT(iLane_Index)) & DPTX_PHY_TX_EQ_VSWING_MASK(iLane_Index);

	Dptx_Reg_Writel(pstDptx, DPTX_PHY_TX_EQ, uiRegMap_PhyTxEQ);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_Pattern(struct Dptx_Params *pstDptx, u32 uiPattern)
{
	u32	uiPhyTPSSelection = 0;

	uiPhyTPSSelection = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiPhyTPSSelection &= ~DPTX_PHYIF_CTRL_TPS_SEL_MASK;
	uiPhyTPSSelection |= ((uiPattern << DPTX_PHYIF_CTRL_TPS_SEL_SHIFT) & DPTX_PHYIF_CTRL_TPS_SEL_MASK);

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiPhyTPSSelection);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Enable_PHY_XMIT(struct Dptx_Params *pstDptx, u32 iNumOfLanes)
{
	u32	uiRegMap_PhyIfCtrl, uiBitMask = 0;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	switch (iNumOfLanes) {
	case PHY_NUM_OF_4_LANE:
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(3);
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(2);
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_2_LANE:
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_1_LANE:
		uiBitMask |= (u32)DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	default:
		dptx_err("Invalid number of lanes %d", (u32)iNumOfLanes);
		break;
	}

	uiRegMap_PhyIfCtrl |= uiBitMask;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Disable_PHY_XMIT(struct Dptx_Params *pstDptx, u32 iNumOfLanes)
{
	u32	uiRegMap_PhyIfCtrl, uiBitMask = 0;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	switch (iNumOfLanes) {
	case PHY_NUM_OF_4_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(3);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(2);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_2_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_1_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	default:
		dptx_err("Invalid number of lanes %d", (u32)iNumOfLanes);
		break;
	}

	uiRegMap_PhyIfCtrl &= ~uiBitMask;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_PHY_Rate_To_Bandwidth(struct Dptx_Params *pstDptx, u8 ucRate, u8 *pucBandWidth)
{
	switch (ucRate) {
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

int32_t Dptx_Core_PHY_Bandwidth_To_Rate(struct Dptx_Params *pstDptx, u8 ucBandWidth, u8 *pucRate)
{
	switch (ucBandWidth) {
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
	}

	return DPTX_RETURN_NO_ERROR;
}

