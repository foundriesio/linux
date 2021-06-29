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


#define EDID_START_BIT_OF_1ST_DETAILED_DES			54
#define EDID_SIZE_OF_DETAILED_DES					18

static int dptx_intr_handle_drm_interface(struct Dptx_Params *pstDptx, bool bHotPlugged)
{
	bool bTrainingState;
	u8 ucDP_Index;
	int32_t iRetVal;

	if (bHotPlugged == (bool)HPD_STATUS_UNPLUGGED) {
		iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;

		for (ucDP_Index = 0; ucDP_Index < (u8)pstDptx->ucNumOfPorts; ucDP_Index++) {
			if (pstDptx->pvHPD_Intr_CallBack == NULL) {
				dptx_err("HPD callback isn't registered");
				return DPTX_RETURN_ENODEV;
			}

			pstDptx->pvHPD_Intr_CallBack(ucDP_Index, (bool)HPD_STATUS_UNPLUGGED);
		}

		pstDptx->ucNumOfPorts = 0;

		return DPTX_RETURN_NO_ERROR;
	}

	iRetVal = Dptx_Intr_Get_Port_Composition(pstDptx, 1);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Link_Get_LinkTraining_Status(pstDptx, &bTrainingState);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
				return iRetVal;

	if (!bTrainingState) {
		iRetVal = Dptx_Link_Perform_BringUp(pstDptx, pstDptx->bMultStreamTransport);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;

		iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;

		if (pstDptx->bMultStreamTransport) {
			iRetVal = Dptx_Ext_Set_Topology_Configuration(pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported);
			if (iRetVal !=  DPTX_RETURN_NO_ERROR)
				return iRetVal;
		}
	}

	for (ucDP_Index = 0; ucDP_Index < pstDptx->ucNumOfPorts; ucDP_Index++) {
		if (pstDptx->pvHPD_Intr_CallBack == NULL) {
			dptx_err("HPD callback isn't registered");
			return DPTX_RETURN_ENODEV;
		}

		pstDptx->pvHPD_Intr_CallBack(ucDP_Index, (bool)HPD_STATUS_PLUGGED);
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

irqreturn_t Dptx_Intr_Threaded_IRQ(int irq, void *dev)
{
	u8 ucStream_Index;
	u32 uiHpdStatus;
	enum HPD_Detection_Status eHPDStatus;

	struct Dptx_Params *pstDptx = dev;

	mutex_lock(&pstDptx->Mutex);

	if (atomic_read(&pstDptx->HPD_IRQ_State)) {
		atomic_set(&pstDptx->HPD_IRQ_State, 0);

		uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);

		eHPDStatus = (uiHpdStatus & DPTX_HPDSTS_STATUS) ? HPD_STATUS_PLUGGED:HPD_STATUS_UNPLUGGED;

		if (pstDptx->eLast_HPDStatus == eHPDStatus) {
			dptx_info("HPD state is not changed as %s", (pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged");
		} else {
			dptx_info("HPD state is changed from %s to %s",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged",
				(eHPDStatus == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged");

			if (eHPDStatus == HPD_STATUS_PLUGGED) {
				if (pstDptx->bUsed_TCC_DRM_Interface) {
					dptx_intr_handle_drm_interface(pstDptx, (bool)HPD_STATUS_PLUGGED);
				} else {
					for (ucStream_Index = 0; ucStream_Index < pstDptx->ucNumOfStreams; ucStream_Index++)
						pstDptx->pvHPD_Intr_CallBack(ucStream_Index, (bool)HPD_STATUS_PLUGGED);
				}
			} else {
				if (pstDptx->bUsed_TCC_DRM_Interface) {
					dptx_intr_handle_drm_interface(pstDptx, (bool)HPD_STATUS_UNPLUGGED);
				} else {
					Dptx_Intr_Handle_HotUnplug(pstDptx);

					for (ucStream_Index = 0; ucStream_Index < pstDptx->ucNumOfStreams; ucStream_Index++)
						pstDptx->pvHPD_Intr_CallBack(ucStream_Index, (bool)HPD_STATUS_UNPLUGGED);
				}
			}

			pstDptx->eLast_HPDStatus = eHPDStatus;
		}
	}

	if (atomic_read(&pstDptx->Sink_request))
		atomic_set(&pstDptx->Sink_request, 0);

	dptx_dbg("DONE");
	dptx_dbg("=======================================\n\n");

	mutex_unlock(&pstDptx->Mutex);

	return IRQ_HANDLED;
}

irqreturn_t Dptx_Intr_IRQ(int irq, void *dev)
{
	irqreturn_t eRetVal = IRQ_HANDLED;
	u32 uiInterrupt_Status, uiHpdStatus, uiRegMap_InterEn;

	struct Dptx_Params *pstDptx = dev;

	uiInterrupt_Status = Dptx_Reg_Readl(pstDptx, DPTX_ISTS);
	uiRegMap_InterEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);

	dptx_dbg("Read: GENERAL_INTERRUPT[0x%08x]: 0x%08x, INTERRUPT_EN[0x%08x]: 0x%08x, INTERRUPT_STATE[0x%08x]: 0x%08x",
				DPTX_ISTS, uiInterrupt_Status, DPTX_IEN, uiRegMap_InterEn, DPTX_HPDSTS, uiHpdStatus);

	if (!(uiInterrupt_Status & DPTX_ISTS_ALL_INTR)) {
		dptx_dbg("IRQ_NONE");
		return  IRQ_NONE;
	}

	if (uiInterrupt_Status & (DPTX_ISTS_AUX_REPLY | DPTX_ISTS_AUX_CMD_INVALID))
		//dptx_dbg("Should not receive AUX interrupts( 0x%x )", uiInterrupt_Status);

	if (uiInterrupt_Status & DPTX_ISTS_HDCP)
		dptx_dbg("DPTX_ISTS_HDCP");

	if (uiInterrupt_Status & DPTX_ISTS_SDP)
		dptx_dbg("DPTX_ISTS_SDP");

	if (uiInterrupt_Status & DPTX_ISTS_VIDEO_FIFO_OVERFLOW) {
		uiRegMap_InterEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
		if (uiRegMap_InterEn & DPTX_IEN_VIDEO_FIFO_OVERFLOW) {
			dptx_warn("DPTX_ISTS_VIDEO_FIFO_OVERFLOW");
			Dptx_Reg_Writel(pstDptx, DPTX_ISTS, DPTX_ISTS_VIDEO_FIFO_OVERFLOW);
		}
	}

	if (uiInterrupt_Status & DPTX_ISTS_AUDIO_FIFO_OVERFLOW) {
		uiRegMap_InterEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
		if (uiRegMap_InterEn & DPTX_IEN_AUDIO_FIFO_OVERFLOW) {
			dptx_warn("DPTX_ISTS_AUDIO_FIFO_OVERFLOW");
			Dptx_Reg_Writel(pstDptx, DPTX_ISTS, DPTX_ISTS_AUDIO_FIFO_OVERFLOW);
		}
	}

	if (uiInterrupt_Status & DPTX_ISTS_HPD) {
#ifdef CONFIG_HDCP_DWC_HLD
		/*
		 * == Workaround : Toggling CP_IRQ by software ==
		 * Sending CP_IRQ to ESM when processing HPD interrups.
		 */
		u32 uiHdcpCfg;
		if (Dptx_Reg_Readl(pstDptx, DPTX_HDCP_OBS) | DPTX_HDCP22_BOOTED) {
			uiHdcpCfg = Dptx_Reg_Readl(pstDptx, DPTX_HDCP_CFG);
			Dptx_Reg_Writel(pstDptx, DPTX_HDCP_CFG, uiHdcpCfg | DPTX_CFG_CP_IRQ);
			Dptx_Reg_Writel(pstDptx, DPTX_HDCP_CFG, uiHdcpCfg & ~DPTX_CFG_CP_IRQ);
		}
#endif
		uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);

		dptx_dbg("Read GENERAL_INTERRUPT[0x%08x]: 0x%08x, HPD_STATUS[0x%08x]: 0x%08x", DPTX_ISTS, uiInterrupt_Status, DPTX_HPDSTS, uiHpdStatus);

		if (uiHpdStatus & DPTX_HPDSTS_IRQ) {
			dptx_dbg("DPTX_HPDSTS_IRQ");
			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_IRQ);

			dptx_intr_handle_hpd_irq(pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if (uiHpdStatus & DPTX_HPDSTS_HOT_PLUG) {
			dptx_dbg("DPTX_HPDSTS_HOT_PLUG");

			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_PLUG);

			atomic_set(&pstDptx->HPD_IRQ_State, 1);

			dptx_intr_notify(pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if (uiHpdStatus & DPTX_HPDSTS_HOT_UNPLUG) {
			dptx_dbg("DPTX_HPDSTS_HOT_UNPLUG");

			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_UNPLUG);

			atomic_set(&pstDptx->HPD_IRQ_State, 1);

			dptx_intr_notify(pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if (uiHpdStatus & 0x80) {
			dptx_dbg("DPTX_HPDSTS[7] HOTPLUG DEBUG INTERRUPT");

			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, (0x80 | DPTX_HPDSTS_HOT_UNPLUG));
			dptx_intr_notify(pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}
	}

	return eRetVal;
}

int32_t Dptx_Intr_Get_Port_Composition(struct Dptx_Params *pstDptx, uint8_t ucClear_PayloadID)
{
	uint8_t ucMST_Supported = 0, ucNumOfPluggedPorts = 0, ucDP_Index;
	int32_t iRetVal;

	iRetVal = Dptx_Ext_Get_Sink_Stream_Capability(pstDptx, &ucMST_Supported);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		pstDptx->bSideBand_MSG_Supported = false;
	else
		pstDptx->bSideBand_MSG_Supported = true;

	if (pstDptx->bSideBand_MSG_Supported) {
		if (ucMST_Supported) {
			if (ucClear_PayloadID) {
				iRetVal = Dptx_Ext_Clear_SidebandMsg_PayloadID_Table(pstDptx);
				if (iRetVal != DPTX_RETURN_NO_ERROR)
					dptx_err("Failed to clear payload id table");
			}

			iRetVal = Dptx_Ext_Get_TopologyState(pstDptx, &ucNumOfPluggedPorts);
			if (iRetVal !=  DPTX_RETURN_NO_ERROR)
				ucNumOfPluggedPorts = 1;
		} else {
			ucNumOfPluggedPorts = 1;
		}

		if (ucNumOfPluggedPorts == 1) {
			iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
			if (iRetVal !=  DPTX_RETURN_NO_ERROR)
				memset(pstDptx->paucEdidBuf_Entry[ucDP_Index], 0,   DPTX_EDID_BUFLEN);
		} else {
			for (ucDP_Index = 0; ucDP_Index < ucNumOfPluggedPorts; ucDP_Index++) {
				iRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg(pstDptx, ucDP_Index);
				if (iRetVal != DPTX_RETURN_NO_ERROR)
					memset(pstDptx->paucEdidBuf_Entry[ucDP_Index], 0,   DPTX_EDID_BUFLEN);
				else
					memcpy(pstDptx->paucEdidBuf_Entry[ucDP_Index], pstDptx->pucEdidBuf, DPTX_EDID_BUFLEN);
			}
		}
		dptx_notice("%d %s connected", ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ? "Ext. monitor is":"Ext. monitors are");
	} else {
		iRetVal = Dptx_Max968XX_Get_TopologyState(&ucNumOfPluggedPorts);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			ucNumOfPluggedPorts = 1;

		for (ucDP_Index = 0; ucDP_Index < ucNumOfPluggedPorts; ucDP_Index++)
			memset(pstDptx->paucEdidBuf_Entry[ucDP_Index], 0,	 DPTX_EDID_BUFLEN);

		dptx_notice("%d %s connected", ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ? " panel is":" panels are");
	}

	pstDptx->ucNumOfPorts = ucNumOfPluggedPorts;
	pstDptx->bMultStreamTransport = (ucNumOfPluggedPorts == 1) ? false:true;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Register_HPD_Callback(struct Dptx_Params *pstDptx, Dptx_HPD_Intr_Callback HPD_Intr_Callback)
{
	pstDptx->pvHPD_Intr_CallBack = HPD_Intr_Callback;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Handle_HotUnplug(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal;

	if (pstDptx->ucMax_Lanes >= PHY_LANE_MAX) {
		dptx_err("Invalid the number of lanes : %d isn't allocated ", pstDptx->ucMax_Lanes);
		return DPTX_RETURN_EINVAL;
	}

	pstDptx->eLast_HPDStatus = HPD_STATUS_UNPLUGGED;

	iRetVal = Dptx_Core_Disable_PHY_XMIT(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_PHY_CLOCK);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Get_HotPlug_Status(struct Dptx_Params *pstDptx, uint8_t *pucHotPlug_Status)
{
	u32 uiHpdStatus;

	if (pucHotPlug_Status == NULL) {
		dptx_err("pucHotPlug_Status == NULL");
		return DPTX_RETURN_EINVAL;
	}

	uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);
	if (uiHpdStatus & DPTX_HPDSTS_STATUS) {
		dptx_dbg("Hot plugged -> HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, uiHpdStatus);
		*pucHotPlug_Status = (uint8_t)HPD_STATUS_PLUGGED;
	} else {
		dptx_dbg("Hot unplugged -> HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, uiHpdStatus);
		*pucHotPlug_Status = (uint8_t)HPD_STATUS_UNPLUGGED;
	}

	return DPTX_RETURN_NO_ERROR;
}


