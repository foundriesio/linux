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

#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include "Dptx_v14.h"
#include "Dptx_drm_dp_addition.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"

#define NUM_OF_CLEAR_VC_PAYLOAD_IDS			3
#define NUM_OF_MST_VCP_TABLEs				8

#define EDID_MAX_EXTRA_BLK					3
#define EDID_EXT_BLK_FIELD					126

#define DPCD_DOWN_REP_SIZE					256
#define MAX_MSG_BUFFER_SIZE					256

#define MAX_CHECK_DPCD_VCP_UPDATED			500
#define MAX_NUMBER_TO_WAIT_MSG_REPLY		500
#define MAX_CHECK_MST_ACT					1000

//#define PRINT_BUF_SIZE						1024

#define MAX_NUM_OF_SUB_BRANCH				2

struct Dptx_Topology_Params {
	struct drm_dp_sideband_msg_rx			stMainBranch_Msg_Rx;
	struct drm_dp_sideband_msg_reply_body	stMainBranch_Msg_Reply;
	struct drm_dp_sideband_msg_rx			stSubBranch_Msg_Rx[MAX_NUM_OF_SUB_BRANCH];
	struct drm_dp_sideband_msg_reply_body	stSubBranch_Msg_Reply[MAX_NUM_OF_SUB_BRANCH];
};

struct Dptx_Topology_Params		stDptx_Topology_Params;


static int32_t dptx_ext_wait_sideband_msg_reply_ready(struct Dptx_Params *pstDptx)
{
	int	iCount = 0;
	int32_t	iRetVal;
	u8	ucSink_Service_IRQ_Vector_ESI0, ucSink_Service_IRQ_Vector;

	while (true) {
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0, &ucSink_Service_IRQ_Vector_ESI0);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  iRetVal;

		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR, &ucSink_Service_IRQ_Vector);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  iRetVal;

		if ((ucSink_Service_IRQ_Vector & DP_DOWN_REP_MSG_RDY) || (ucSink_Service_IRQ_Vector_ESI0 & DP_DOWN_REP_MSG_RDY)) {
			break;
		}

		if (iCount++ > MAX_NUMBER_TO_WAIT_MSG_REPLY) {
			dptx_err("Timed out(%d ms ) from down sideband messagge reply ready...", iCount);
			return DPTX_RETURN_ENODEV;
		}

		mdelay(1);
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_clear_sideband_msg_reply(struct Dptx_Params *pstDptx)
{
	int 	iCount = 0;
	int32_t	iRetVal;
	u8	ucSink_Service_IRQ_Vector_ESI0, ucSink_Service_IRQ_Vector;

	while (true) {
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR, &ucSink_Service_IRQ_Vector);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  DPTX_RETURN_ENODEV;

		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0, &ucSink_Service_IRQ_Vector_ESI0);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  DPTX_RETURN_ENODEV;

		if (!(ucSink_Service_IRQ_Vector & DP_DOWN_REP_MSG_RDY || ucSink_Service_IRQ_Vector_ESI0 & DP_DOWN_REP_MSG_RDY)) {
			break;
		}

		iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR, DP_DOWN_REP_MSG_RDY);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  DPTX_RETURN_ENODEV;

		iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0, ucSink_Service_IRQ_Vector_ESI0);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  DPTX_RETURN_ENODEV;

		if (iCount++ > MAX_NUMBER_TO_WAIT_MSG_REPLY) {
			dptx_info("Timed out(%d ms ) from clear sideband messagge reply... ", iCount);
			return DPTX_RETURN_I2C_OVER_AUX_NO_ACK;
		}

		mdelay(1);
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_get_sideband_msg_down_req_reply(struct Dptx_Params *pstDptx, u8 ucRequest_id, u8 *pucMsg_Out, u8 *pucMsg_len)
{
	bool	bFirst = true;
	u8	aucSink_SidebandMsg_Buf[DPCD_DOWN_REP_SIZE], aucMessage[DPCD_DOWN_REP_SIZE];
	u8	ucHeader_Len, ucMsg_Len, ucRetries = 0;
	int32_t	iRetVal;
	struct	drm_dp_sideband_msg_hdr stDp_SidebandMsg_Header;

	*pucMsg_len = 0;

again:
	memset(aucMessage, 0, DPCD_DOWN_REP_SIZE);
	ucMsg_Len = 0;

	while (true) {
		iRetVal = dptx_ext_wait_sideband_msg_reply_ready(pstDptx);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		iRetVal = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_SIDEBAND_MSG_DOWN_REP_BASE, aucSink_SidebandMsg_Buf, DPCD_DOWN_REP_SIZE);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		iRetVal = Drm_Addition_Decode_Sideband_Msg_Hdr(&stDp_SidebandMsg_Header, aucSink_SidebandMsg_Buf, DPCD_DOWN_REP_SIZE, &ucHeader_Len);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Drm_Addition_Decode_Sideband_Msg_Hdr()");
			return  iRetVal;
		}

		dptx_dbg("Request id(%d): lct=%d, lcr=%d, rad=0x%x, bcast=%d, path=%d, msglen=%d, somt=%d, eomt=%d, seqno=%d",
						ucRequest_id,
						 stDp_SidebandMsg_Header.lct, stDp_SidebandMsg_Header.lcr,
						 stDp_SidebandMsg_Header.lct > 1 ? stDp_SidebandMsg_Header.rad[0]:0,
						 stDp_SidebandMsg_Header.broadcast, stDp_SidebandMsg_Header.path_msg,
						 stDp_SidebandMsg_Header.msg_len, stDp_SidebandMsg_Header.somt,
						 stDp_SidebandMsg_Header.eomt, stDp_SidebandMsg_Header.seqno);

		stDp_SidebandMsg_Header.msg_len -= 1;

		memcpy(&aucMessage[ucMsg_Len], &aucSink_SidebandMsg_Buf[ucHeader_Len], stDp_SidebandMsg_Header.msg_len);

		ucMsg_Len += stDp_SidebandMsg_Header.msg_len;

		if (bFirst && !stDp_SidebandMsg_Header.somt) {
			dptx_err("SOMT is not set ");
			return  DPTX_RETURN_EBUSY;
		}

		bFirst =  false;

		iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_DEVICE_SERVICE_IRQ_VECTOR, DP_DOWN_REP_MSG_RDY);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return  iRetVal;

		if (stDp_SidebandMsg_Header.eomt)
			break;
	}

	if ((aucMessage[0] & 0x7F) != ucRequest_id) {
		if (ucRetries < 3) {
			dptx_warn("ucRequest_id %d does not match expected %d, retrying ", aucMessage[0] & 0x7F, ucRequest_id);
			ucRetries++;
			goto again;
		} else {
			dptx_err("ucRequest_id %d does not match expected %d, giving up ", aucMessage[0] & 0x7F, ucRequest_id);
			return  DPTX_RETURN_EBUSY;
		}
	}

	iRetVal = dptx_ext_clear_sideband_msg_reply(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	if (pucMsg_Out)
		memcpy(pucMsg_Out, aucMessage, ucMsg_Len);

	*pucMsg_len = ucMsg_Len;

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_set_sideband_msg_enum_path_resources(struct Dptx_Params *pstDptx, u8 ucStreamSink_PortNum, u8 ucVCP_Id, u8 ucRAD_PortNum)
{
	u8	ucReply_Len;
	u8	aucMsg_Buf[256];
	u8	*msg;
	int	iMsg_Len = 256;
	int32_t	iRetVal;
	struct drm_dp_sideband_msg_hdr stMsg_Header;

	memset(&stMsg_Header, 0, sizeof(struct drm_dp_sideband_msg_hdr));

	dptx_dbg("Port %d for allocate: RAD Port = %d", ucStreamSink_PortNum, ucRAD_PortNum);

	stMsg_Header.lct		= 1;
	stMsg_Header.lcr		= 0;
	stMsg_Header.rad[0]		= 0;
	stMsg_Header.broadcast  = false;
	stMsg_Header.path_msg	= 0;
	stMsg_Header.msg_len	= 3;
	stMsg_Header.somt		= 1;
	stMsg_Header.eomt		= 1;
	stMsg_Header.seqno		= 0;

	if (ucRAD_PortNum != INVALID_MST_PORT_NUM) {
		stMsg_Header.lct = 2;
		stMsg_Header.lcr = 1;
		stMsg_Header.rad[0] |= ((ucRAD_PortNum << 4) & 0xF0);
	}

	Drm_Addition_Encode_Sideband_Msg_Hdr(&stMsg_Header, aucMsg_Buf, &iMsg_Len);

	msg		= &aucMsg_Buf[iMsg_Len];
	msg[0]	= DP_ENUM_PATH_RESOURCES;
	msg[1]	= ((ucStreamSink_PortNum & 0xF) << 4);

	Drm_Addition_Encode_SideBand_Msg_CRC(msg, 2);

	iMsg_Len += 3;

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_SIDEBAND_MSG_DOWN_REQ_BASE, aucMsg_Buf, iMsg_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_ext_get_sideband_msg_down_req_reply(pstDptx, DP_ENUM_PATH_RESOURCES, NULL, &ucReply_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_set_sideband_msg_allocate_payload(struct Dptx_Params *pstDptx, u8 ucStreamSink_PortNum, u8 ucVCP_Id, u16 usPBN, u8 ucRAD_PortNum)
{
	u8	ucReply_Len;
	u8	aucMsg_Buf[256];
	int	iMsg_Len = 256;
	u8	*msg;
	int32_t	iRetVal;
	struct drm_dp_sideband_msg_hdr stMsg_Header;

	memset(&stMsg_Header, 0, sizeof(struct drm_dp_sideband_msg_hdr));

	dptx_dbg("Port %d allocates payload: VCP Id= %d, RAD Port = %d ", ucStreamSink_PortNum, ucVCP_Id, ucRAD_PortNum);

	stMsg_Header.lct		= 1;
	stMsg_Header.lcr		= 0;
	stMsg_Header.rad[0]		= 0;
	stMsg_Header.broadcast	= false;
	stMsg_Header.path_msg	= 1;
	stMsg_Header.msg_len	= 6;
	stMsg_Header.somt		= 1;
	stMsg_Header.eomt		= 1;
	stMsg_Header.seqno		= 0;

	if (ucRAD_PortNum != INVALID_MST_PORT_NUM) {
		stMsg_Header.lct = 2;
		stMsg_Header.lcr = 1;
		stMsg_Header.rad[0] |= ((ucRAD_PortNum << 4) & 0xF0);
	}

	Drm_Addition_Encode_Sideband_Msg_Hdr(&stMsg_Header, aucMsg_Buf, &iMsg_Len);

	msg		= &aucMsg_Buf[iMsg_Len];

	msg[0]	= DP_ALLOCATE_PAYLOAD;
	msg[1]	= ((ucStreamSink_PortNum & 0xF) << 4);
	msg[2]	= (ucVCP_Id & 0x7F);
	msg[3]	= (usPBN >> 8);
	msg[4]	= (usPBN & 0xFF);

	Drm_Addition_Encode_SideBand_Msg_CRC(msg, 5);

	iMsg_Len += 6;

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_SIDEBAND_MSG_DOWN_REQ_BASE, aucMsg_Buf, iMsg_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_ext_get_sideband_msg_down_req_reply(pstDptx, DP_ALLOCATE_PAYLOAD, NULL, &ucReply_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_set_sideband_msg_link_address(struct Dptx_Params *dptx,
																				struct drm_dp_sideband_msg_rx *pstSideband_Msg_Rx,
																				struct drm_dp_sideband_msg_reply_body *pstSideband_Msg_Reply,
																				u8		ucPort_ConnectToBranch)
{
	u8	aucMsgHdr_Buf[256];
	u8	*pucMsg;
	int 	iMsg_Len = 256;
	int32_t	iRetVal;
	struct	drm_dp_sideband_msg_hdr stMsg_Header;

	memset(&stMsg_Header, 0, sizeof(struct drm_dp_sideband_msg_hdr));

	stMsg_Header.lct		= 1;
	stMsg_Header.lcr		= 0;
	stMsg_Header.rad[0]		= 0;
	stMsg_Header.broadcast	= false;
	stMsg_Header.path_msg	= 0;
	stMsg_Header.msg_len	= 2;
	stMsg_Header.somt		= 1;
	stMsg_Header.eomt		= 1;
	stMsg_Header.seqno		= 0;

	if (ucPort_ConnectToBranch != INVALID_MST_PORT_NUM) {
		stMsg_Header.lct = 2;
		stMsg_Header.lcr = 1;
		stMsg_Header.rad[0] |= ((ucPort_ConnectToBranch << 4) & 0xF0);
	}

	Drm_Addition_Encode_Sideband_Msg_Hdr(&stMsg_Header, aucMsgHdr_Buf, &iMsg_Len);

	pucMsg = &aucMsgHdr_Buf[iMsg_Len];
	pucMsg[0] = DP_LINK_ADDRESS;

	Drm_Addition_Encode_SideBand_Msg_CRC(pucMsg, 1);

	iMsg_Len += 2;

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(dptx, DP_SIDEBAND_MSG_DOWN_REQ_BASE, aucMsgHdr_Buf, iMsg_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_ext_get_sideband_msg_down_req_reply(dptx, DP_LINK_ADDRESS, pstSideband_Msg_Rx->msg, (u8 *)&iMsg_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	pstSideband_Msg_Rx->curlen = iMsg_Len;

	iRetVal = Drm_Addition_Parse_Sideband_Link_Address(pstSideband_Msg_Rx, pstSideband_Msg_Reply);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		dptx_err("Error from Drm_Addition_Parse_Sideband_Link_Address()");
		return	iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_get_link_numof_slots(struct Dptx_Params *pstDptx, u16 iPlayloadBandWidth_Number, u8 *pucNumOfSlots)
{
	u8	ucLink_BandWidth;
	int32_t	iRetVal;
	u32	uiDivider;

	iRetVal = Dptx_Core_PHY_Rate_To_Bandwidth(pstDptx, pstDptx->stDptxLink.ucLinkRate, &ucLink_BandWidth);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return  iRetVal;

	switch (ucLink_BandWidth) {
	case DP_LINK_BW_1_62:
		uiDivider = (3 * pstDptx->stDptxLink.ucNumOfLanes);
		break;
	case DP_LINK_BW_2_7:
		uiDivider = (5 * pstDptx->stDptxLink.ucNumOfLanes);
		break;
	case DP_LINK_BW_5_4:
		uiDivider = (10 * pstDptx->stDptxLink.ucNumOfLanes);
		break;
	case DP_LINK_BW_8_1:
		uiDivider = (15 * pstDptx->stDptxLink.ucNumOfLanes);
		break;
	default:
		dptx_err("Invalid index of BW : %d", (uint32_t)ucLink_BandWidth);
		return  DPTX_RETURN_EINVAL;
	}

	*pucNumOfSlots = DIV_ROUND_UP(iPlayloadBandWidth_Number, uiDivider);

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_clear_sink_vcpid_table(struct Dptx_Params *pstDptx)
{
	u8	aucPayload_Allocate_Set[NUM_OF_CLEAR_VC_PAYLOAD_IDS] = { 0x00, 0x00, 0x3F };
	u8	ucPayload_Updated_Status;
	int32_t	iRetVal;
	int	uiRetry_LinkUpdated = 0;

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, DP_PAYLOAD_TABLE_UPDATED);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_PAYLOAD_ALLOCATE_SET, aucPayload_Allocate_Set, NUM_OF_CLEAR_VC_PAYLOAD_IDS);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	while (true) {
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, &ucPayload_Updated_Status);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		if (ucPayload_Updated_Status & DP_PAYLOAD_TABLE_UPDATED)
			break;

		if (uiRetry_LinkUpdated++ > MAX_CHECK_DPCD_VCP_UPDATED)
			dptx_warn("Payload table in Sink is not updated for %dms", (uiRetry_LinkUpdated * 1));

		mdelay(1);
	}

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, DP_PAYLOAD_TABLE_UPDATED);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return  DPTX_RETURN_ENODEV;

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_clear_link_vcp_tables(struct Dptx_Params *pstDptx)
{
	u8 ucElements;

	for (ucElements = 0; ucElements < NUM_OF_MST_VCP_TABLEs; ucElements++) {
		Dptx_Reg_Writel(pstDptx, DPTX_MST_VCP_TABLE_REG_N(ucElements), 0);
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_set_link_vcpid_table_slot(struct Dptx_Params *pstDptx, u8 ucStart_SlotNum, u8 ucNumOfSlots, u8 ucStream_Index)
{
	u8		ucSlotNum, ucCount;
	u32		uiVCP_RegOffset, uiRegMap_VCPTable, uiBit_Shift = 0,        uiBit_Mask = 0;

	if ((ucStart_SlotNum + ucNumOfSlots) > DPTX_MAX_LINK_SLOTS) {
		dptx_err("Start Slot(%d) + Num of Slots(%d) is larger than max slots(%d)", ucStart_SlotNum, ucNumOfSlots, (u32)DPTX_MAX_LINK_SLOTS);
		return DPTX_RETURN_EINVAL;
	}

	dptx_dbg("----- Setting %d slots for stream %d", ucStart_SlotNum, ucStream_Index);

	for (ucCount = 0; ucCount < ucNumOfSlots; ucCount++) {
		ucSlotNum = (ucStart_SlotNum + ucCount);

		uiBit_Shift = ((ucSlotNum & 0x7) * 4);
		uiBit_Mask = GENMASK(uiBit_Shift + 3, uiBit_Shift);

		uiVCP_RegOffset		= DPTX_MST_VCP_TABLE_REG_N(ucSlotNum >> 3);
		uiRegMap_VCPTable	= Dptx_Reg_Readl(pstDptx, uiVCP_RegOffset);

		uiRegMap_VCPTable	&= ~uiBit_Mask;
		uiRegMap_VCPTable	|= ((ucStream_Index << uiBit_Shift) & uiBit_Mask);

		Dptx_Reg_Writel(pstDptx, uiVCP_RegOffset, uiRegMap_VCPTable);
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_ext_set_sink_vcpid_table_slot(struct Dptx_Params *pstDptx, u8 ucStart_SlotNum, u8 ucNumOfSlots, u8 ucStream_Index)
{
	u8	ucStatus, ucWriteBuf[3];
	int32_t	iRetVal;
	u32	uiRetry_LinkUpdated = 0;

	ucWriteBuf[0] = ucStream_Index;
	ucWriteBuf[1] = ucStart_SlotNum;
	ucWriteBuf[2] = ucNumOfSlots;

	dptx_dbg("----- Setting %d slots for stream %d", ucStart_SlotNum, ucStream_Index);

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_PAYLOAD_ALLOCATE_SET, ucWriteBuf, 3);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	do {
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, &ucStatus);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		if (ucStatus & DP_PAYLOAD_TABLE_UPDATED)
			break;

		if (uiRetry_LinkUpdated == MAX_CHECK_DPCD_VCP_UPDATED)
			dptx_info("Payload table in Sink is not updated for %dms", (uiRetry_LinkUpdated * 1));

		udelay(1);
	} while (uiRetry_LinkUpdated++ < MAX_CHECK_DPCD_VCP_UPDATED);

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, DP_PAYLOAD_TABLE_UPDATED);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return DPTX_RETURN_ENODEV;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Set_Stream_Mode(struct Dptx_Params *pstDptx, bool bMST_Supported, u8 ucNumOfPorts)
{
	pstDptx->bMultStreamTransport  = bMST_Supported;
	pstDptx->ucNumOfPorts = ucNumOfPorts;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Get_Stream_Mode(struct Dptx_Params *pstDptx, bool *pbMST_Supported, u8 *pucNumOfPorts)
{
	if (pbMST_Supported == NULL) {
		dptx_err("pbMST_Supported is NULL");
		return DPTX_RETURN_EINVAL;
	}

	*pbMST_Supported = pstDptx->bMultStreamTransport;
	*pucNumOfPorts = pstDptx->ucNumOfPorts;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Get_Sink_Stream_Capability(struct Dptx_Params *pstDptx, uint8_t *pucMST_Supported)
{
	u8	ucMST_Mode_Caps;
	int32_t	iRetVal;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_MSTM_CAP, &ucMST_Mode_Caps);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return DPTX_RETURN_ENODEV;

	if (ucMST_Mode_Caps & DP_MST_CAP) {
		*pucMST_Supported = 1;
		dptx_dbg("Sink supports MST");
	} else {
		*pucMST_Supported = 0;
		dptx_dbg("Sink supports SST only");
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Set_Stream_Capability(struct Dptx_Params *pstDptx)
{
	u8	ucMST_Mode_Caps;
	int32_t	iRetVal;
	u32	uiRegMap_Cctl;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_MSTM_CAP, &ucMST_Mode_Caps);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return DPTX_RETURN_ENODEV;

	uiRegMap_Cctl = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);

	if (pstDptx->bMultStreamTransport) {
		if (ucMST_Mode_Caps & DP_MST_CAP) {
			dptx_info("MST is profiled and Sink supports MST -> enable MST in Link( DPTX_CCTL ) and Sink( DP_MSTM_CTRL to 0x07)");
			uiRegMap_Cctl |= DPTX_CCTL_ENABLE_MST_MODE;
			Dptx_Reg_Writel(pstDptx, DPTX_CCTL, uiRegMap_Cctl);

			iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_MSTM_CTRL, (DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC));
			if (iRetVal != DPTX_RETURN_NO_ERROR)
				return iRetVal;
		} else {
			dptx_info("MST is profiled in Source but Sink doesn't support MST -> Disable MST in Source( DPTX_CCTL )");

			pstDptx->bMultStreamTransport	= false;
			pstDptx->ucNumOfStreams 		= 1;

			uiRegMap_Cctl &= ~DPTX_CCTL_ENABLE_MST_MODE;
			Dptx_Reg_Writel(pstDptx, DPTX_CCTL, uiRegMap_Cctl);
		}
	} else {
		uiRegMap_Cctl &= ~DPTX_CCTL_ENABLE_MST_MODE;
		Dptx_Reg_Writel(pstDptx, DPTX_CCTL, uiRegMap_Cctl);

		if (ucMST_Mode_Caps & DP_MST_CAP) {
			dptx_info("MST is NOT profiled in Source but Sink supports MST -> disable MST in Sink( DP_MSTM_CTRL ) to 0");

			iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_MSTM_CTRL, ~(DP_MST_EN | DP_UP_REQ_EN | DP_UPSTREAM_IS_SRC));
			if (iRetVal != DPTX_RETURN_NO_ERROR)
				return iRetVal;
		} else {
			dptx_info("MST is not profiled in Source and Sink doesn't supports MST");
		}
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Get_Link_PayloadBandwidthNumber(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8	ucRest;
	int32_t	iRetVal;
	u32			uiPBN_BPP = 0, uiPBN_M_BPP, uiTenfold_M_BPP;
	struct Dptx_Video_Params *pstVideoParams = &pstDptx->stVideoParams;

	if ((pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_RGB) || (pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_YCBCR444)) {
		uiPBN_BPP = ((pstDptx->stVideoParams.uiPeri_Pixel_Clock[ucStream_Index] * VIDEO_LINK_BPP_RGB_YCbCr444 * 64) / 54);
	} else if (pstVideoParams->ucPixel_Encoding == PIXEL_ENCODING_TYPE_YCBCR422) {
		uiPBN_BPP = ((pstDptx->stVideoParams.uiPeri_Pixel_Clock[ucStream_Index] * VIDEO_LINK_BPP_YCbCr422 * 64) / 54);
	} else {
		dptx_err("Unknown Pixel encoding type(%d)", pstVideoParams->ucPixel_Encoding);
		return DPTX_RETURN_EINVAL;
	}

	uiPBN_BPP *= (1006);

	uiPBN_M_BPP	= (uiPBN_BPP / 1000000);
	uiTenfold_M_BPP	= (uiPBN_BPP / 100000);

	ucRest = (uiTenfold_M_BPP - (uiPBN_M_BPP * 10));
	if (ucRest >= 5)
		pstDptx->ausPayloadBandwidthNumber[ucStream_Index] = (u16)(uiPBN_M_BPP + 1);
	else
		pstDptx->ausPayloadBandwidthNumber[ucStream_Index] = (u16)uiPBN_M_BPP;

	iRetVal = dptx_ext_get_link_numof_slots(pstDptx, pstDptx->ausPayloadBandwidthNumber[ucStream_Index], &pstDptx->aucNumOfSlots[ucStream_Index]);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	dptx_dbg("Stream %d : PBN(%d) <- Rest(%d), The number of slots: %d", ucStream_Index, pstDptx->ausPayloadBandwidthNumber[ucStream_Index], ucRest, pstDptx->aucNumOfSlots[ucStream_Index]);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Set_Link_VCP_Tables(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	int32_t	iRetVal;
	u8	ucPrev_NumOfSlots, ucCurrent_NumOfSlots;

	ucCurrent_NumOfSlots = pstDptx->aucNumOfSlots[ucStream_Index];

	switch (ucStream_Index) {
	case PHY_INPUT_STREAM_0:
		ucPrev_NumOfSlots = 0;
		break;
	case PHY_INPUT_STREAM_1:
		ucPrev_NumOfSlots = pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_0];
		break;
	case PHY_INPUT_STREAM_2:
		ucPrev_NumOfSlots = pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_0];
		ucPrev_NumOfSlots += pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_1];
		break;
	case PHY_INPUT_STREAM_3:
		ucPrev_NumOfSlots = pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_0];
		ucPrev_NumOfSlots += pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_1];
		ucPrev_NumOfSlots += pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_2];
		break;
	default:
		dptx_err("Invalid stream index(%d)", ucStream_Index);
		return DPTX_RETURN_EINVAL;
	}

	iRetVal = dptx_ext_set_link_vcpid_table_slot(pstDptx, (ucPrev_NumOfSlots + 1), ucCurrent_NumOfSlots, (ucStream_Index + 1));
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return  iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Set_Sink_VCP_Table_Slots(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8	ucPrev_NumOfSlots, ucCurrent_NumOfSlots;
	int32_t	iRetVal;

	ucCurrent_NumOfSlots = pstDptx->aucNumOfSlots[ucStream_Index];

	switch (ucStream_Index) {
	case PHY_INPUT_STREAM_0:
		ucPrev_NumOfSlots = 0;
		break;
	case PHY_INPUT_STREAM_1:
		ucPrev_NumOfSlots = pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_0];
		break;
	case PHY_INPUT_STREAM_2:
		ucPrev_NumOfSlots = pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_0];
		ucPrev_NumOfSlots += pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_1];
		break;
	case PHY_INPUT_STREAM_3:
		ucPrev_NumOfSlots = pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_0];
		ucPrev_NumOfSlots += pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_1];
		ucPrev_NumOfSlots += pstDptx->aucNumOfSlots[PHY_INPUT_STREAM_2];
		break;
	default:
		dptx_err("Invalid stream index(%d)", ucStream_Index);
		return DPTX_RETURN_EINVAL;
	}

	iRetVal = dptx_ext_set_sink_vcpid_table_slot(pstDptx, (ucPrev_NumOfSlots + 1), ucCurrent_NumOfSlots, (ucStream_Index + 1));
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return  iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Clear_VCP_Tables(struct Dptx_Params *pstDptx)
{
	int32_t	iRetVal;

	iRetVal = dptx_ext_clear_sink_vcpid_table(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return  iRetVal;

	iRetVal = dptx_ext_clear_link_vcp_tables(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return  iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Initiate_MST_Act(struct Dptx_Params *pstDptx)
{
	u32		uiRegMap_Cctl, uiRetry_MstAct = 0;

	uiRegMap_Cctl = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);
	uiRegMap_Cctl |= DPTX_CCTL_INITIATE_MST_ACT;

	Dptx_Reg_Writel(pstDptx, DPTX_CCTL, uiRegMap_Cctl);

	while (true) {
		uiRegMap_Cctl = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);
		if (!(uiRegMap_Cctl & DPTX_CCTL_INITIATE_MST_ACT))
			break;

		if (uiRetry_MstAct++ > MAX_CHECK_MST_ACT) {
			dptx_info("CCTL.ACT timeout.. it was waiting for %dms ", (uiRetry_MstAct * 1));
			return DPTX_RETURN_MST_ACT_TIMEOUT;
		}

		mdelay(1);
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Clear_SidebandMsg_PayloadID_Table(struct Dptx_Params *pstDptx)
{
	u8		ucReply_Len;
	u8		aucMsg_Buf[MAX_MSG_BUFFER_SIZE], *pucMssage;
	int32_t	iRetVal;
	int		iMsg_Len = 0;

	struct drm_dp_sideband_msg_hdr stSideBand_MsgHeader = {
		.lct = 1,
		.lcr = 6,
		.rad = { 0, },
		.broadcast = true,
		.path_msg = 1,
		.msg_len = 2,
		.somt = 1,
		.eomt = 1,
		.seqno = 0,
	};

	Drm_Addition_Encode_Sideband_Msg_Hdr(&stSideBand_MsgHeader, aucMsg_Buf, &iMsg_Len);

	pucMssage		= &aucMsg_Buf[iMsg_Len];
	pucMssage[0]	= DP_CLEAR_PAYLOAD_ID_TABLE;

	Drm_Addition_Encode_SideBand_Msg_CRC(pucMssage, 1);

	iMsg_Len += 2;

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_SIDEBAND_MSG_DOWN_REQ_BASE, aucMsg_Buf, iMsg_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_ext_get_sideband_msg_down_req_reply(pstDptx, DP_CLEAR_PAYLOAD_ID_TABLE, NULL, &ucReply_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Get_TopologyState(struct Dptx_Params *pstDptx, u8 *pucNumOfHotpluggedPorts)
{
	u8	ucMainPort_Count, ucBranchPort_Count, ucBranchPort_Number = 0,        ucSinkDevPort_Index = 0;
	int32_t	iRetVal;
	struct drm_dp_sideband_msg_rx			*pstMain_Msg_Rx, *pstMsg_Rx;
	struct drm_dp_sideband_msg_reply_body	*pstMain_Msg_Reply, *pstMsg_Reply;
	struct Dptx_Topology_Params				*pstDptx_Topology_Params = &stDptx_Topology_Params;

	memset(&pstDptx_Topology_Params->stMainBranch_Msg_Rx, 0, sizeof(pstDptx_Topology_Params->stMainBranch_Msg_Rx));
	memset(&pstDptx_Topology_Params->stMainBranch_Msg_Reply, 0, sizeof(pstDptx_Topology_Params->stMainBranch_Msg_Reply));

	memset(&pstDptx->aucStreamSink_PortNumber[0], INVALID_MST_PORT_NUM, (sizeof(u8) * PHY_INPUT_STREAM_MAX));
	memset(&pstDptx->aucRAD_PortNumber[0], INVALID_MST_PORT_NUM, (sizeof(u8) * PHY_INPUT_STREAM_MAX));

	pstMain_Msg_Rx = &pstDptx_Topology_Params->stMainBranch_Msg_Rx;
	pstMain_Msg_Reply = &pstDptx_Topology_Params->stMainBranch_Msg_Reply;

	iRetVal = dptx_ext_set_sideband_msg_link_address(pstDptx, pstMain_Msg_Rx, pstMain_Msg_Reply, INVALID_MST_PORT_NUM);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	dptx_dbg("1st Brach DP_LINK_ADDRESS-NPORTS = %d", pstMain_Msg_Reply->u.link_addr.nports);

	for (ucMainPort_Count = 0; ucMainPort_Count < pstMain_Msg_Reply->u.link_addr.nports; ucMainPort_Count++) {
		dptx_dbg(" -.Input port(%d): %s ", ucMainPort_Count,
											pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].input_port == INPUT_PORT_TYPE_RX ? "DP Rx":"DP Tx");
		dptx_dbg(" -.Peer Dev type: %s",	pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].peer_device_type == PEER_STREAM_SINK_DEV ? "Stream Sink" :
											(pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].peer_device_type == PEER_BRANCHING_DEV) ? "Branching Unit" : "Other");
		dptx_dbg(" -.%s Port Num: %d",		pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].port_number >= 8 ? "Logical" : "Physical",
											pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].port_number);
		dptx_dbg(" -.MCS: %s",	pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].mcs == 1 ? "DP Rx Port":"Stream Sink");
		dptx_dbg(" -.DP Dev. Plug Status: %s",	pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].mcs == 1 ? "Connected":"Connected to Sink");

		if (pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].peer_device_type == PEER_STREAM_SINK_DEV &&
			!pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].mcs &&
			pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].ddps) {
			pstDptx->aucStreamSink_PortNumber[ucSinkDevPort_Index] = pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].port_number;
			dptx_info(" ==> Stream Sink port[%d] = %d, RAD Port = %d", ucSinkDevPort_Index,
																		pstDptx->aucStreamSink_PortNumber[ucSinkDevPort_Index],
																		pstDptx->aucRAD_PortNumber[ucSinkDevPort_Index]);

			ucSinkDevPort_Index++;
		}

		if (pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].input_port == INPUT_PORT_TYPE_TX &&
			pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].peer_device_type == PEER_BRANCHING_DEV &&
			pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].mcs &&
			pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].ddps) {

			if (ucBranchPort_Number >= MAX_NUM_OF_SUB_BRANCH) {
				dptx_info("Num of branchs is reached to Max(%d)", (MAX_NUM_OF_SUB_BRANCH + 1));

				*pucNumOfHotpluggedPorts = ucSinkDevPort_Index;
				return DPTX_RETURN_NO_ERROR;
			}

			pstMsg_Rx = &pstDptx_Topology_Params->stSubBranch_Msg_Rx[ucBranchPort_Number];
			pstMsg_Reply = &pstDptx_Topology_Params->stSubBranch_Msg_Reply[ucBranchPort_Number];

			ucBranchPort_Number++;

			iRetVal = dptx_ext_set_sideband_msg_link_address(pstDptx, pstMsg_Rx, pstMsg_Reply, pstMain_Msg_Reply->u.link_addr.ports[ucMainPort_Count].port_number);
			if (iRetVal !=  DPTX_RETURN_NO_ERROR)
				return iRetVal;

			dptx_dbg("%d Brach DP_LINK_ADDRESS-NPORTS = %d", (ucBranchPort_Number + 1), pstMsg_Reply->u.link_addr.nports);

			for (ucBranchPort_Count = 0; ucBranchPort_Count < pstMsg_Reply->u.link_addr.nports; ucBranchPort_Count++) {
				dptx_dbg(" -.Input port(%d): %s ", ucBranchPort_Count,
													pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].input_port == INPUT_PORT_TYPE_RX ? "DP Rx" : "DP Tx");
				dptx_dbg(" -.Peer Dev type: %s", pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].peer_device_type == PEER_STREAM_SINK_DEV ? "Stream Sink" :
													(pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].peer_device_type == PEER_BRANCHING_DEV) ? "Branching Unit" : "Other");
				dptx_dbg(" -.%s Port Num: %d", pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].port_number >= 8 ? "Logical" : "Physical",
												pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].port_number);
				dptx_dbg(" -.MCS: %s",	pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].mcs == 1 ? "DP Rx Port":"Stream Sink");
				dptx_dbg(" -.DP Dev. Plug Status: %s",	pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].mcs == 1 ? "Connected":"Connected to Sink");

				if (pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].input_port == INPUT_PORT_TYPE_TX &&
					pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].peer_device_type == PEER_STREAM_SINK_DEV &&
					!pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].mcs &&
					pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].ddps) {
					pstDptx->aucStreamSink_PortNumber[ucSinkDevPort_Index] = pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].port_number;
					pstDptx->aucRAD_PortNumber[ucSinkDevPort_Index] = pstMsg_Reply->u.link_addr.ports[ucMainPort_Count].port_number;

					dptx_info(" ==> Stream Sink port[%d] = %d, RAD Port = %d", ucSinkDevPort_Index,
																				pstMsg_Reply->u.link_addr.ports[ucBranchPort_Count].port_number,
																				pstDptx->aucRAD_PortNumber[ucSinkDevPort_Index]);

					ucSinkDevPort_Index++;

					if (ucSinkDevPort_Index >= PHY_INPUT_STREAM_MAX) {
						dptx_info("Port index is reached to Max(%d)", PHY_INPUT_STREAM_MAX);

						*pucNumOfHotpluggedPorts = ucSinkDevPort_Index;
						return DPTX_RETURN_NO_ERROR;
					}
				}
			}
		}
	}

	*pucNumOfHotpluggedPorts = ucSinkDevPort_Index;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Set_Topology_Configuration(struct Dptx_Params *pstDptx, u8 ucNumOfPorts, bool bSideBand_MSG_Supported)
{
	u8	ucSink_Playload_Status, ucStream_Index;
	int32_t	iRetVal;
	u32	uiRetry_LinkUpdated = 0;

	iRetVal = Dptx_Ext_Clear_VCP_Tables(pstDptx);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	for (ucStream_Index = 0; ucStream_Index < ucNumOfPorts; ucStream_Index++) {
		iRetVal = Dptx_Ext_Get_Link_PayloadBandwidthNumber(pstDptx, ucStream_Index);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;
	}

	for (ucStream_Index = 0; ucStream_Index < ucNumOfPorts; ucStream_Index++) {
		iRetVal = Dptx_Ext_Set_Link_VCP_Tables(pstDptx, ucStream_Index);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}
	}

	for (ucStream_Index = 0; ucStream_Index < ucNumOfPorts; ucStream_Index++) {
		iRetVal = Dptx_Ext_Set_Sink_VCP_Table_Slots(pstDptx, ucStream_Index);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;
	}

	iRetVal = Dptx_Ext_Initiate_MST_Act(pstDptx);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	do {
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, &ucSink_Playload_Status);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return DPTX_RETURN_ENODEV;

		if (ucSink_Playload_Status & DP_PAYLOAD_ACT_HANDLED)
			break;

		if (uiRetry_LinkUpdated == MAX_CHECK_DPCD_VCP_UPDATED) {
			dptx_warn("Act in Sink is not handled for %dms ", (uiRetry_LinkUpdated * 2));
		}

		mdelay(2);
	} while (uiRetry_LinkUpdated++ < MAX_CHECK_DPCD_VCP_UPDATED);

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_PAYLOAD_TABLE_UPDATE_STATUS, 0x3);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return DPTX_RETURN_ENODEV;

	if (bSideBand_MSG_Supported) {
		for (ucStream_Index = 0; ucStream_Index < ucNumOfPorts; ucStream_Index++) {
			if (pstDptx->aucStreamSink_PortNumber[ucStream_Index] != INVALID_MST_PORT_NUM) {
				iRetVal = dptx_ext_set_sideband_msg_enum_path_resources(pstDptx,
																				pstDptx->aucStreamSink_PortNumber[ucStream_Index],
																				(ucStream_Index + 1),
																				pstDptx->aucRAD_PortNumber[ucStream_Index]);
				if (iRetVal !=  DPTX_RETURN_NO_ERROR)
					return iRetVal;

				iRetVal = dptx_ext_set_sideband_msg_allocate_payload(pstDptx,
																			pstDptx->aucStreamSink_PortNumber[ucStream_Index],
																			pstDptx->aucVCP_Id[ucStream_Index],
																			pstDptx->ausPayloadBandwidthNumber[ucStream_Index],
																			pstDptx->aucRAD_PortNumber[ucStream_Index]);
				if (iRetVal !=  DPTX_RETURN_NO_ERROR)
					return iRetVal;
			} else {
				dptx_info("DP %d port number isn't available ", ucStream_Index);
			}
		}
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Ext_Remote_I2C_Read(struct Dptx_Params *pstDptx, u8 ucStream_Index)
{
	u8	ucReply_Len, ucPort_Index, ucRad_Port, ucExt_Blocks, ucBlk_Index;
	u8	aucReq_Buf[MAX_MSG_BUFFER_SIZE], aucRep_Buf[MAX_MSG_BUFFER_SIZE];
	u8 	*pucMsg;
	int	len = 256;
	int32_t	iRetVal;
	struct drm_dp_sideband_msg_hdr stMsg_Header;

	ucPort_Index = pstDptx->aucStreamSink_PortNumber[ucStream_Index];
	if (ucPort_Index == INVALID_MST_PORT_NUM) {
		dptx_err("Stream %d isn't allocated ", ucStream_Index);
		return DPTX_RETURN_EINVAL;
	}

	ucRad_Port = pstDptx->aucRAD_PortNumber[ucStream_Index];

	memset(&stMsg_Header, 0, sizeof(struct drm_dp_sideband_msg_hdr));

	stMsg_Header.lct		= 1;
	stMsg_Header.lcr		= 0;
	stMsg_Header.rad[0]		= 0;
	stMsg_Header.broadcast	= false;
	stMsg_Header.path_msg	= 0;
	stMsg_Header.msg_len	= 9;
	stMsg_Header.somt		= 1;
	stMsg_Header.eomt		= 1;
	stMsg_Header.seqno		= 0;

	if (ucRad_Port != INVALID_MST_PORT_NUM) {
		stMsg_Header.lct = 2;
		stMsg_Header.lcr = 1;
		stMsg_Header.rad[0] |= ((ucRad_Port << 4) & 0xF0);
	}

	Drm_Addition_Encode_Sideband_Msg_Hdr(&stMsg_Header, aucReq_Buf, &len);

	pucMsg	 	= &aucReq_Buf[len];
	pucMsg[0]	= DP_REMOTE_I2C_READ;
	pucMsg[1]	= ((ucPort_Index & 0xF) << 4);
	pucMsg[1]	|= (1 & 0x3);
	pucMsg[2]	= (0x50 & 0x7F);
	pucMsg[3]	= 1;// Num of bytes to write
	pucMsg[4]	= (0 << 5);// I2C data to write
	pucMsg[5]	= (0 & 0xF);
	pucMsg[6]	= (0x50 & 0x7F);
	pucMsg[7]	= (DPTX_ONE_EDID_BLK_LEN);

	Drm_Addition_Encode_SideBand_Msg_CRC(pucMsg, 8);

	len += 9;

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_SIDEBAND_MSG_DOWN_REQ_BASE, aucReq_Buf, len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	iRetVal = dptx_ext_get_sideband_msg_down_req_reply(pstDptx, DP_REMOTE_I2C_READ, aucRep_Buf, &ucReply_Len);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	if (aucRep_Buf[2] == 0) {
		dptx_warn("No EDID data in Sink");
		return DPTX_RETURN_ENODEV;
	}

	memcpy(pstDptx->pucEdidBuf, &aucRep_Buf[3], DPTX_ONE_EDID_BLK_LEN);

	dptx_dbg("I2C Remote messages replied => ");
	dptx_dbg(" -.Reply type: %s", (aucRep_Buf[0] & 0x80) ? "NAK":"ACK");
	dptx_dbg(" -.Request id: %s", ((aucRep_Buf[0] & 0x7F) == DP_REMOTE_I2C_READ) ? "REMOTE_I2C_READ":"Wrong ID");
	dptx_dbg(" -.Port Number: %d <- (%d, %d)", (aucRep_Buf[1] & 0x0F), ucPort_Index, ucRad_Port);
	dptx_dbg(" -.Num of bytes read: %d", aucRep_Buf[2]);
	dptx_dbg(" -.Num of extensions: %d", aucRep_Buf[126 + 3]);

	ucExt_Blocks = aucRep_Buf[(EDID_EXT_BLK_FIELD + 3)];
	if (ucExt_Blocks == 0)
		return DPTX_RETURN_NO_ERROR;

	if (ucExt_Blocks > EDID_MAX_EXTRA_BLK) {
		dptx_warn("The number of extended blocks is larger than Max %d -> down to %d", (u32)EDID_MAX_EXTRA_BLK, (u32)EDID_MAX_EXTRA_BLK);
		ucExt_Blocks = EDID_MAX_EXTRA_BLK;
	}

	for (ucBlk_Index = 1; ucBlk_Index <= ucExt_Blocks; ucBlk_Index++) {
		Drm_Addition_Encode_Sideband_Msg_Hdr(&stMsg_Header, aucReq_Buf, &len);

		pucMsg	 	= &aucReq_Buf[len];
		pucMsg[0]	= DP_REMOTE_I2C_READ;
		pucMsg[1]	= ((ucPort_Index & 0xF) << 4);
		pucMsg[1]	|= (1 & 0x3);
		pucMsg[2]	= (0x50 & 0x7F);
		pucMsg[3]	= 1;
		pucMsg[4]	= (ucBlk_Index * DPTX_ONE_EDID_BLK_LEN);
		pucMsg[5]	= (0 & 0xF);
		pucMsg[6]	= (0x50 & 0x7F);
		pucMsg[7]	= (DPTX_ONE_EDID_BLK_LEN);

		Drm_Addition_Encode_SideBand_Msg_CRC(pucMsg, 8);

		len += 9;

		iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_SIDEBAND_MSG_DOWN_REQ_BASE, aucReq_Buf, len);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		iRetVal = dptx_ext_get_sideband_msg_down_req_reply(pstDptx, DP_REMOTE_I2C_READ, aucRep_Buf, &ucReply_Len);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return iRetVal;

		memcpy(&pstDptx->pucEdidBuf[DPTX_ONE_EDID_BLK_LEN], &aucRep_Buf[3], DPTX_ONE_EDID_BLK_LEN);

		dptx_dbg("Extended messages replied => ");
		dptx_dbg(" -.Reply type: %s", (aucRep_Buf[0] & 0x80) ? "ACK":"NAK");
		dptx_dbg(" -.Request id: %s", ((aucRep_Buf[0] & 0x7F) == DP_REMOTE_I2C_READ) ? "REMOTE_I2C_READ":"Wrong ID");
		dptx_dbg(" -.Port Number: %d <- (%d, %d)", (aucRep_Buf[1] & 0x0F), ucPort_Index, ucRad_Port);
		dptx_dbg(" -.Num of bytes read: %d", aucRep_Buf[2]);
		dptx_dbg(" -.Num of extensions: %d", aucRep_Buf[126 + 3]);
	}

	return DPTX_RETURN_NO_ERROR;
}



/*************** Proc interface *****************/

#define DPTX_DEBUGFS_BUF_SIZE		1024
#define DATA_DUMP_BUF_SIZE			DPTX_EDID_BUFLEN

static int dptx_ext_proc_open(struct inode *inode, struct file *filp);
static int dptx_ext_proc_close(struct inode *inode, struct file *filp);
static ssize_t dptx_ext_proc_read_hpd_state(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_read_port_composition(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_read_edid_data(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_read_link_training_status(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_read_str_status(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_read_video_timing(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_write_video_timing(
	struct file *filp,
	const char __user *buffer,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_read_audio_configuration(
	struct file *filp,
	char __user *usr_buf,
	size_t cnt,
	loff_t *off_set);
static ssize_t dptx_ext_proc_write_audio_configuration(
	struct file *filp,
	const char __user *buffer,
	size_t cnt,
	loff_t *off_set);


static const struct file_operations proc_fops_hpd_state = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_hpd_state,
};

static const struct file_operations proc_fops_topology_state = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_port_composition,
};

static const struct file_operations proc_fops_edid_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_edid_data,
};

static const struct file_operations proc_fops_linkT_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_link_training_status,
};

static const struct file_operations proc_fops_str_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_str_status,
};

static const struct file_operations proc_fops_video_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_video_timing,
	.write	 = dptx_ext_proc_write_video_timing,
};

static const struct file_operations proc_fops_audio_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_ext_proc_open,
	.release = dptx_ext_proc_close,
	.read    = dptx_ext_proc_read_audio_configuration,
	.write	 = dptx_ext_proc_write_audio_configuration,
};


static void dptx_ext_Print_U8_Buf(
			u8 *pucBuf,
			u32 uiStart_RegOffset,
			u32 uiLength)
{
	int			iOffset;
	char		acStr[DATA_DUMP_BUF_SIZE];
	int			iNumOfWritten = 0;

	iNumOfWritten += snprintf(&acStr[iNumOfWritten],
					DATA_DUMP_BUF_SIZE - iNumOfWritten,
					"\n");

	for (iOffset = 0; iOffset < uiLength; iOffset++) {
		if (!(iOffset % 16)) {
			iNumOfWritten += snprintf(&acStr[iNumOfWritten],
					DATA_DUMP_BUF_SIZE - iNumOfWritten,
					"\n%02x:",
					(uiStart_RegOffset + iOffset));
			if (iNumOfWritten >= DATA_DUMP_BUF_SIZE)
				break;
		}

		iNumOfWritten += snprintf(&acStr[iNumOfWritten],
					DATA_DUMP_BUF_SIZE - iNumOfWritten,
					" %02x",
					pucBuf[iOffset]);
		if (iNumOfWritten >= DATA_DUMP_BUF_SIZE)
			break;
	}

	dptx_info("%s", acStr);
}


int dptx_ext_proc_open(struct inode *inode, struct file *filp)
{
	try_module_get(THIS_MODULE);
	return 0;
}

int dptx_ext_proc_close(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);
	return 0;
}

ssize_t dptx_ext_proc_read_hpd_state(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	ssize_t	stSize;
	char	*pcHpd_Buf;
	uint8_t ucHPD_State;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcHpd_Buf = devm_kzalloc(pstDptx->pstParentDev,
							DPTX_DEBUGFS_BUF_SIZE,
							GFP_KERNEL);
	if (pcHpd_Buf == NULL)
		dptx_err("Could not allocate HPD state buffer ");

	Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);

	stSize = sprintf(pcHpd_Buf, "%s\n",
			ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED ?
			"Hot plugged" : "Hot unplugged");

	stSize = simple_read_from_buffer(
					usr_buf,
					cnt,
					off_set,
					(void *)pcHpd_Buf,
					stSize);

	devm_kfree(pstDptx->pstParentDev, pcHpd_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_read_port_composition(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	uint8_t ucHPD_State;
	ssize_t	stSize;
	char	*pcTopology_Buf;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcTopology_Buf = devm_kzalloc(pstDptx->pstParentDev,
							DPTX_DEBUGFS_BUF_SIZE,
							GFP_KERNEL);
	if (pcTopology_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer ");
		return  0;
	}

	Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
	if (ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged...");
		return  0;
	}

	Dptx_Intr_Get_Port_Composition(pstDptx, 0);

	stSize = sprintf(pcTopology_Buf,
			"%s : %d %s %s connected\n",
			pstDptx->bMultStreamTransport ? "MST mode":"SST mode",
			pstDptx->ucNumOfPorts,
			(pstDptx->bSideBand_MSG_Supported) ?
			"Ext. monitor":"SerDes",
			pstDptx->ucNumOfPorts == 1 ? "port is":"ports are");

	stSize = simple_read_from_buffer(
					usr_buf,
					cnt,
					off_set,
					(void *)pcTopology_Buf,
					stSize);

	devm_kfree(pstDptx->pstParentDev, pcTopology_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_read_edid_data(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	bool	bSink_MST_Supported, bSink_Has_EDID = true;
	uint8_t ucHPD_State, ucNumOfPluggedPorts = 0, ucStream_Index;
	char	*pcEdid_Buf;
	int32_t	iRetVal;
	ssize_t	stSize;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcEdid_Buf = devm_kzalloc(pstDptx->pstParentDev,
						DPTX_DEBUGFS_BUF_SIZE,
						GFP_KERNEL);
	if (pcEdid_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer ");
		return  0;
	}

	Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
	if (ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged...");
		return  0;
	}

	Dptx_Ext_Get_Stream_Mode(pstDptx,
						&bSink_MST_Supported,
						&ucNumOfPluggedPorts);

	if (ucNumOfPluggedPorts == 0) {
		dptx_err("Get Port Composition first ");
		return  0;
	}

	if (bSink_MST_Supported) {
		for (ucStream_Index = 0;
				ucStream_Index < ucNumOfPluggedPorts;
				ucStream_Index++) {
			iRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg(
								pstDptx,
								ucStream_Index);
			if (iRetVal != DPTX_RETURN_NO_ERROR)
				break;

			dptx_ext_Print_U8_Buf(
				pstDptx->pucEdidBuf,
				0,
				(DPTX_ONE_EDID_BLK_LEN +
				DPTX_ONE_EDID_BLK_LEN));
		}

		if (ucStream_Index == 0)
			bSink_Has_EDID = false;
	} else {
		iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			bSink_Has_EDID = false;
		else
			dptx_ext_Print_U8_Buf(
				pstDptx->pucEdidBuf,
				0,
				(DPTX_ONE_EDID_BLK_LEN +
				DPTX_ONE_EDID_BLK_LEN));
	}

	stSize = sprintf(pcEdid_Buf, "%s : %s %d %s connected\n",
			bSink_MST_Supported ? "MST mode":"SST mode",
			bSink_Has_EDID ?
			"Sink has EDID from":"Sink doesn't have EDID from",
			ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ?
			"port is":"ports are");

	stSize = simple_read_from_buffer(
						usr_buf,
						cnt,
						off_set,
						(void *)pcEdid_Buf,
						stSize);

	devm_kfree(pstDptx->pstParentDev, pcEdid_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_read_link_training_status(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	bool	bSink_MST_Supported, bTrainingState;
	uint8_t ucHPD_State, ucNumOfPluggedPorts = 0;
	char	*pcEdid_Buf;
	int32_t	iRetVal;
	ssize_t	stSize;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcEdid_Buf = devm_kzalloc(pstDptx->pstParentDev,
							DPTX_DEBUGFS_BUF_SIZE,
							GFP_KERNEL);
	if (pcEdid_Buf == NULL)
		dptx_err("Could not allocate HPD state buffer ");

	Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
	if (ucHPD_State == (uint8_t)HPD_STATUS_UNPLUGGED) {
		dptx_err("Hot unplugged...");
		return 0;
	}

	Dptx_Ext_Get_Stream_Mode(pstDptx,
						&bSink_MST_Supported,
						&ucNumOfPluggedPorts);

	if (ucNumOfPluggedPorts == 0) {
		dptx_err("Get Port Composition first");
		return 0;
	}

	iRetVal = Dptx_Link_Get_LinkTraining_Status(pstDptx, &bTrainingState);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return 0;

	if (bTrainingState) {
		stSize = sprintf(pcEdid_Buf,
				"%s : link training was already successed\n",
				bSink_MST_Supported ? "MST mode":"SST mode");

		stSize = simple_read_from_buffer(
						usr_buf,
						cnt,
						off_set,
						(void *)pcEdid_Buf,
						stSize);

		devm_kfree(pstDptx->pstParentDev, pcEdid_Buf);

		return stSize;
	}

	iRetVal = Dptx_Link_Perform_BringUp(pstDptx, bSink_MST_Supported);
	if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Link_Perform_Training(pstDptx,
						pstDptx->ucMax_Rate,
						pstDptx->ucMax_Lanes);
		if (iRetVal ==  DPTX_RETURN_NO_ERROR)
			bTrainingState = true;
		else
			bTrainingState = false;
	} else {
		bTrainingState = false;
	}

	if (bSink_MST_Supported)
		Dptx_Ext_Set_Topology_Configuration(pstDptx,
				pstDptx->ucNumOfPorts,
				pstDptx->bSideBand_MSG_Supported);

	stSize = sprintf(pcEdid_Buf,
			"%s : link training %s with %s on %d lanes\n",
			bSink_MST_Supported ? "MST mode" : "SST mode",
			bTrainingState ? "successed" : "failed",
			pstDptx->stDptxLink.ucLinkRate ==
			DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
			(pstDptx->stDptxLink.ucLinkRate ==
			DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
			(pstDptx->stDptxLink.ucLinkRate ==
			DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3",
			pstDptx->stDptxLink.ucNumOfLanes);

	stSize = simple_read_from_buffer(
						usr_buf,
						cnt,
						off_set,
						(void *)pcEdid_Buf,
						stSize);

	devm_kfree(pstDptx->pstParentDev, pcEdid_Buf);

	return  stSize;
}

ssize_t dptx_ext_proc_read_video_timing(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	bool bSink_MST_Supported;
	u8 ucNumOfPluggedPorts = 0, ucStream_Index;
	u16 usH_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	u16 usV_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	char	*pcVideoTiming_Buf;
	int32_t	iRetVal;
	ssize_t				stSize;
	struct Dptx_Dtd_Params	stDtd_Params;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcVideoTiming_Buf = devm_kzalloc(pstDptx->pstParentDev,
							DPTX_DEBUGFS_BUF_SIZE,
							GFP_KERNEL);
	if (pcVideoTiming_Buf == NULL)
		dptx_err("Could not allocate HPD state buffer ");

	Dptx_Ext_Get_Stream_Mode(pstDptx,
					&bSink_MST_Supported,
					&ucNumOfPluggedPorts);

	if ((ucNumOfPluggedPorts == 0) ||
		(ucNumOfPluggedPorts >= PHY_INPUT_STREAM_MAX)) {
		dptx_err("Invalid the Num. of Ports %d\n",
					ucNumOfPluggedPorts);
		dptx_err("-> Get Port Composition first");
		return 0;
	}

	for (ucStream_Index = 0;
			ucStream_Index < ucNumOfPluggedPorts;
			ucStream_Index++) {
		iRetVal = Dptx_Avgen_Get_Video_Configured_Timing(
						pstDptx,
						ucStream_Index,
						&stDtd_Params);
		if (iRetVal != DPTX_RETURN_NO_ERROR)
			return 0;

		usH_Active[ucStream_Index] = stDtd_Params.h_active;
		usV_Active[ucStream_Index] = stDtd_Params.v_active;
	}

	stSize = sprintf(pcVideoTiming_Buf,
			"%s : 1st %d x %d, 2nd : %d x %d, 3rd : %d x %d, 4th : %d x %d\n",
			bSink_MST_Supported ? "MST mode":"SST mode",
			usH_Active[PHY_INPUT_STREAM_0],
			usV_Active[PHY_INPUT_STREAM_0],
			usH_Active[PHY_INPUT_STREAM_1],
			usV_Active[PHY_INPUT_STREAM_1],
			usH_Active[PHY_INPUT_STREAM_2],
			usV_Active[PHY_INPUT_STREAM_2],
			usH_Active[PHY_INPUT_STREAM_3],
			usV_Active[PHY_INPUT_STREAM_3]);

	stSize = simple_read_from_buffer(
					usr_buf,
					cnt,
					off_set,
					(void *)pcVideoTiming_Buf,
					stSize);

	devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_write_video_timing(
			struct file *filp,
			const char __user *buffer,
			size_t cnt,
			loff_t *off_set)
{
	char				*pcVideoTiming_Buf;
	int32_t	 iRetVal;
	uint32_t uiVideoCode, uiStream_Index;
	ssize_t				stSize;
	struct Dptx_Dtd_Params	stDtd_Params;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcVideoTiming_Buf = devm_kzalloc(pstDptx->pstParentDev,
						DPTX_DEBUGFS_BUF_SIZE,
						GFP_KERNEL);
	if (pcVideoTiming_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer ");
		return  0;
	}

	stSize = simple_write_to_buffer(pcVideoTiming_Buf,
									cnt,
									off_set,
									buffer,
									cnt);
	if ((stSize != cnt) && (stSize >= 0)) {
		dptx_err("Can't get input data : %d <-> %d ",
					(uint32_t)stSize,
					(uint32_t)cnt);

		devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);
		return  -EIO;
	}

	pcVideoTiming_Buf[cnt] = '\0';

	iRetVal = sscanf(pcVideoTiming_Buf, "%u %u",
					&uiStream_Index,
					&uiVideoCode);
	if (iRetVal < 0) {
		devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);
		return  0;
	}

	dptx_info("Stream index : %d, Vidoe code : %d",
				(u32)uiStream_Index,
				(u32)uiVideoCode);

	iRetVal = Dptx_Avgen_Fill_Dtd(
					&stDtd_Params,
					uiVideoCode,
					60000,
					(u8)VIDEO_FORMAT_CEA_861);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		dptx_err("Can't find VIC %d from dtd", (u32)uiVideoCode);

		devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);
		return  0;
	}

	iRetVal = Dptx_Avgen_Set_Video_Detailed_Timing(
						pstDptx,
						(u8)uiStream_Index,
						&stDtd_Params);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);
		return  0;
	}

	Dptx_Avgen_Set_Video_Stream_Enable(pstDptx, true, uiStream_Index);

	devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_read_audio_configuration(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	bool	bSink_MST_Supported;
	uint8_t ucInfType,  ucNumOfCh, ucDataWidth;
	uint8_t ucHBRMode, ucSamplingFreq, ucOrgSamplingFreq;
	uint8_t ucNumOfPluggedPorts = 0, ucStream_Index;
	char *pcAudioConf_Buf;
	ssize_t stSize;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcAudioConf_Buf = devm_kzalloc(pstDptx->pstParentDev,
						DPTX_DEBUGFS_BUF_SIZE,
						GFP_KERNEL);
	if (pcAudioConf_Buf == NULL) {
		dptx_err("Could not allocate HPD buffer");
		return 0;
	}

	Dptx_Ext_Get_Stream_Mode(pstDptx,
					&bSink_MST_Supported,
					&ucNumOfPluggedPorts);
	if ((ucNumOfPluggedPorts == 0) ||
		(ucNumOfPluggedPorts >= PHY_INPUT_STREAM_MAX)) {
		dptx_err("Invalid the Num. of Ports %d\n",
					ucNumOfPluggedPorts);
		dptx_err("-> Get Port Composition first");
		return 0;
	}

	for (ucStream_Index = 0;
			ucStream_Index < ucNumOfPluggedPorts;
			ucStream_Index++) {
		Dptx_Avgen_Get_Audio_Input_InterfaceType(pstDptx,
			ucStream_Index,
			&ucInfType);
		Dptx_Avgen_Get_Audio_DataWidth(pstDptx,
		ucStream_Index,
		&ucDataWidth);
		Dptx_Avgen_Get_Audio_MaxNumOfChannels(pstDptx,
			ucStream_Index,
			&ucNumOfCh);
		Dptx_Avgen_Get_Audio_HBR_En(pstDptx,
			ucStream_Index,
			&ucHBRMode);
		Dptx_Avgen_Get_Audio_SamplingFreq(pstDptx,
			&ucSamplingFreq,
			&ucOrgSamplingFreq);

		stSize = sprintf(pcAudioConf_Buf,
				"DP %d : IT(%d), DW(%d), NumOfCh(%d), HBR(%d), SFq(%d), OrgSF(%d)\n",
				ucStream_Index,
				ucInfType,
				ucDataWidth,
				ucNumOfCh,
				ucHBRMode,
				ucSamplingFreq,
				ucOrgSamplingFreq);

		stSize = simple_read_from_buffer(usr_buf,
						cnt,
						off_set,
						(void *)pcAudioConf_Buf,
						stSize);
	}

	devm_kfree(pstDptx->pstParentDev, pcAudioConf_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_write_audio_configuration(
			struct file *filp,
			const char __user *buffer,
			size_t cnt,
			loff_t *off_set)
{
	char	 *pcAudioConf_Buf;
	int32_t  iRetVal;
	uint32_t uiStream_Index, uiInfType;
	uint32_t uiDataWidth, uiNumOfCh;
	uint32_t uiHBRMode, uiSamplingFreq;
	uint32_t uiOrgSamplingFreq;
	ssize_t  stSize;
	struct Dptx_Audio_Params stAudioParams;
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	pcAudioConf_Buf = devm_kzalloc(pstDptx->pstParentDev,
							DPTX_DEBUGFS_BUF_SIZE,
							GFP_KERNEL);
	if (pcAudioConf_Buf == NULL) {
		dptx_err("Could not allocate HPD buf");
		return  0;
	}

	stSize = simple_write_to_buffer(pcAudioConf_Buf,
									cnt,
									off_set,
									buffer,
									cnt);
	if ((stSize != cnt) && (stSize >= 0)) {
		dptx_err("Can't get input data : %d <-> %d ",
					(uint32_t)stSize,
					(uint32_t)cnt);

		devm_kfree(pstDptx->pstParentDev, pcAudioConf_Buf);
		return  -EIO;
	}

	pcAudioConf_Buf[cnt] = '\0';

	iRetVal = sscanf(pcAudioConf_Buf, "%u %u %u %u %u %u %u",
						&uiStream_Index,
						&uiInfType,
						&uiDataWidth,
						&uiNumOfCh,
						&uiHBRMode,
						&uiSamplingFreq,
						&uiOrgSamplingFreq);
	if (iRetVal < 0) {
		devm_kfree(pstDptx->pstParentDev, pcAudioConf_Buf);
		return  0;
	}

	stAudioParams.ucInput_InterfaceType     = (uint8_t)uiInfType;
	stAudioParams.ucInput_DataWidth         = (uint8_t)uiDataWidth;
	stAudioParams.ucInput_Max_NumOfchannels = (uint8_t)uiNumOfCh;
	stAudioParams.ucInput_HBR_Mode          = (uint8_t)uiHBRMode;
	stAudioParams.ucIEC_Sampling_Freq       = (uint8_t)uiSamplingFreq;
	stAudioParams.ucIEC_OriginSamplingFreq  = (uint8_t)uiOrgSamplingFreq;
	stAudioParams.ucInput_TimestampVersion	= 0x12;

	Dptx_Avgen_Configure_Audio(pstDptx,
							(uint8_t)uiStream_Index,
							&stAudioParams);

	dptx_info("\n[Write audio Conf]DP %d: Input type(%d), Data Width(%d), NumOfCh(%d), HBR(%d), SFreq(%d), OrgSFreq(%d)\n",
							uiStream_Index,
							uiInfType,
							uiDataWidth,
							uiNumOfCh,
							uiHBRMode,
							uiSamplingFreq,
							uiOrgSamplingFreq);

	devm_kfree(pstDptx->pstParentDev, pcAudioConf_Buf);

	return stSize;
}

ssize_t dptx_ext_proc_read_str_status(
			struct file *filp,
			char __user *usr_buf,
			size_t cnt,
			loff_t *off_set)
{
	struct Dptx_Params *pstDptx = PDE_DATA(file_inode(filp));

	Dpv14_Tx_Suspend_T(pstDptx);

	mdelay(5000);

	Dpv14_Tx_Resume_T(pstDptx);

	return  0;
}

int32_t Dptx_Ext_Proc_Interface_Init(struct Dptx_Params *pstDptx)
{
	pstDptx->pstDP_Proc_Dir = proc_mkdir("dptx_v14", NULL);
	if (pstDptx->pstDP_Proc_Dir == NULL)
		dptx_err("Could not create file system @ /proc/dptx_v14");

	pstDptx->pstDP_HPD_Dir = proc_create_data("hpd",
						S_IFREG | 0444,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_hpd_state,
						pstDptx);
	if (pstDptx->pstDP_HPD_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/hpd");

	pstDptx->pstDP_Topology_Dir = proc_create_data("topology",
						S_IFREG | 0444,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_topology_state,
						pstDptx);
	if (pstDptx->pstDP_Topology_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/topology");

	pstDptx->pstDP_EDID_Dir = proc_create_data("edid",
						S_IFREG | 0444,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_edid_data,
						pstDptx);
	if (pstDptx->pstDP_EDID_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/edid");

	pstDptx->pstDP_LinkT_Dir = proc_create_data("link",
						S_IFREG | 0444,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_linkT_data,
						pstDptx);
	if (pstDptx->pstDP_LinkT_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/link");

	pstDptx->pstDP_LinkT_Dir = proc_create_data("str",
						S_IFREG | 0444,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_str_data,
						pstDptx);
	if (pstDptx->pstDP_LinkT_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/str");

	pstDptx->pstDP_Video_Dir = proc_create_data("video",
						S_IFREG | 0444 | 0222,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_video_data,
						pstDptx);
	if (pstDptx->pstDP_Video_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/video");

	pstDptx->pstDP_Auio_Dir = proc_create_data("audio",
						S_IFREG | 0444,
						pstDptx->pstDP_Proc_Dir,
						&proc_fops_audio_data,
						pstDptx);
	if (pstDptx->pstDP_Auio_Dir == NULL)
		dptx_err("Could not create file system data @ /proc/dptx_v14/audio");

	return 0;
}


