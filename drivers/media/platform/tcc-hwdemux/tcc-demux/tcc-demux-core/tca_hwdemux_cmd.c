/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include "tca_hwdemux.h"
#include "tca_hwdemux_cmd.h"

#define REQUEST_STC_INTERVAL 1000
#define REQUEST_PCRUPDATE_RES_MS 200

enum {
	QUEUE_HW_DEMUX_INIT = 0,
	QUEUE_HW_DEMUX_DEINIT,
	QUEUE_HW_DEMUX_ADD_FILTER,
	QUEUE_HW_DEMUX_DELETE_FILTER,
	QUEUE_HW_DEMUX_GET_STC,
	QUEUE_HW_DEMUX_SET_PCR_PID,
	QUEUE_HW_DEMUX_GET_VERSION,
	QUEUE_HW_DEMUX_INTERNAL_SET_INPUT,
	QUEUE_HW_DEMUX_CHANGE_INTERFACE,
	QUEUE_HW_DEMUX_CIPHER_SET_ALGORITHM,
	QUEUE_HW_DEMUX_CIPHER_SET_KEY,
	QUEUE_HW_DEMUX_CIPHER_SET_IV,
	QUEUE_HW_DEMUX_CIPHER_EXECUTE,
	QUEUE_HW_DEMUX_TYPE_MAX,
};

static int g_ret, g_ret_done;
static wait_queue_head_t cmd_queue[QUEUE_HW_DEMUX_TYPE_MAX];

static irqreturn_t MailBox_Handler(int irq, void *dev)
{
	volatile struct PMAILBOX *pMailBox = hHWDMX.mbox0_base;
	volatile int nReg[8];
	unsigned char cmd;

	nReg[0] = pMailBox->uMBOX_RX0.nREG;
	nReg[1] = pMailBox->uMBOX_RX1.nREG;	// Updated Position
	nReg[2] = pMailBox->uMBOX_RX2.nREG;	// End Buffer Position
	nReg[3] = pMailBox->uMBOX_RX3.nREG;
	nReg[4] = pMailBox->uMBOX_RX4.nREG;
	nReg[5] = pMailBox->uMBOX_RX5.nREG;
	nReg[6] = pMailBox->uMBOX_RX6.nREG;
	nReg[7] = pMailBox->uMBOX_RX7.nREG;

	cmd = (nReg[0] & 0xFF000000) >> 24;

	switch (cmd) {
	case HW_DEMUX_INIT:	// DEMUX_INIT
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_INIT]));
		break;

	case HW_DEMUX_DEINIT:	// DEMUX_DEINIT
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_DEINIT]));
		break;

	case HW_DEMUX_ADD_FILTER:	// DEMUX_ADD_FILTER
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_ADD_FILTER]));
		break;

	case HW_DEMUX_DELETE_FILTER:	// DEMUX_DELETE_FILTER
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_DELETE_FILTER]));
		break;

	case HW_DEMUX_SET_PCR_PID:	// DEMUX_SET_PCR_PID
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_SET_PCR_PID]));
		break;

	case HW_DEMUX_GET_VERSION:	// HW_DEMUX_GET_VERSION
		g_ret = (nReg[0] & 0x0000FFFF);
		pr_info("[INFO][HWDMX]Version[%X] Date[%X]\n", nReg[1],
			nReg[2]);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_GET_VERSION]));
		break;

	case HW_DEMUX_INTERNAL_SET_INPUT:	// HW_DEMUX_INTERNAL_SET_INPUT
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_INTERNAL_SET_INPUT]));
		break;

	case HW_CM_CIPHER_SET_ALGORITHM:
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_CIPHER_SET_ALGORITHM]));
		break;

	case HW_CM_CIPHER_SET_KEY:
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_CIPHER_SET_KEY]));
		break;

	case HW_CM_CIPHER_SET_IV:
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_CIPHER_SET_IV]));
		break;

	case HW_CM_CIPHER_EXECUTE:
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_CIPHER_EXECUTE]));
		break;

	case HW_DEMUX_CHANGE_INTERFACE:
		g_ret = (nReg[0] & 0x0000FFFF);
		g_ret_done = 1;
		wake_up(&(cmd_queue[QUEUE_HW_DEMUX_CHANGE_INTERFACE]));
		break;

	default:
		TSDMXCallback(irq, dev, nReg);
		break;
	}

	return IRQ_HANDLED;
}

void TSDMX_Prepare(void)
{
	volatile struct PMAILBOX *pMailBoxMain;
	volatile struct PMAILBOX *pMailBoxSub;
	volatile struct PCM_TSD_CFG *pTSDCfg;

	pMailBoxMain = hHWDMX.mbox0_base;
	pMailBoxSub = hHWDMX.mbox1_base;
	pTSDCfg = hHWDMX.cfg_base;

	int cmd_id;

	for (cmd_id = QUEUE_HW_DEMUX_INIT; cmd_id < QUEUE_HW_DEMUX_TYPE_MAX;
	     cmd_id++) {
		init_waitqueue_head(&cmd_queue[cmd_id]);
	}

	BITSET(pMailBoxMain->uMBOX_CTL_016.nREG, Hw0 | Hw1 | Hw4 | Hw5 | Hw6);
	BITSET(pMailBoxSub->uMBOX_CTL_016.nREG, Hw0 | Hw1 | Hw4 | Hw5 | Hw6);
	BITSET(pTSDCfg->IRQ_MASK_POL.nREG, Hw16 | Hw22);

	if (request_irq
	    (hHWDMX.irq, MailBox_Handler,
	     IRQ_TYPE_LEVEL_LOW, "mailbox", NULL) == 0) {
	}

}

void TSDMX_Release(void)
{
	free_irq(hHWDMX.irq, NULL);
}

int TSDMXCMD_Init(struct ARG_TSDMXINIT *pARG, unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_INIT << 24 | (pARG->uiDMXID & 0xff) << 16 | (
	    pARG->uiMode & 0xff) << 8 | pARG->uiChipID;
	pMailBox->uMBOX_TX1.nREG = pARG->uiTSRingBufAddr;
	pMailBox->uMBOX_TX2.nREG = pARG->uiTSRingBufSize;
	pMailBox->uMBOX_TX3.nREG = pARG->uiSECRingBufAddr;
	pMailBox->uMBOX_TX4.nREG = pARG->uiSECRingBufSize;
	pMailBox->uMBOX_TX5.nREG =
	    pARG->uiTSIFInterface << 24 | (
	    pARG->uiTSIFCh & 0xff) << 16 | (
	    pARG->uiTSIFPort & 0xff) <<
	    8 | (pARG->uiTSIFPol & 0xff);
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_INIT]), g_ret_done == 1,
			   uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int TSDMXCMD_DeInit(unsigned int uiDMXID, unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_DEINIT << 24 | (uiDMXID & 0xff) << 16;
	pMailBox->uMBOX_TX1.nREG = 0;
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_DEINIT]), g_ret_done == 1,
			   uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int TSDMXCMD_ADD_Filter(struct ARG_TSDMX_ADD_FILTER *pARG,
	unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	if (pARG->uiTYPE == HW_DEMUX_SECTION) {
		pMailBox->uMBOX_TX0.nREG =
		    HW_DEMUX_ADD_FILTER << 24 | (
		    pARG->uiDMXID & 0xff) << 16 | (
		    pARG->uiFID & 0xff)
		    << 8 | (pARG->uiTotalIndex & 0xf) << 4 | (
		    pARG->uiCurrentIndex & 0xf);
		if (pARG->uiCurrentIndex == 0) {
			pMailBox->uMBOX_TX1.nREG =
			    (pARG->uiTYPE & 0xff) << 24 | (
			    pARG->uiPID & 0xffff) << 8
			    | (pARG->uiFSIZE & 0xff);
			pMailBox->uMBOX_TX2.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX3.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX4.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX5.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX6.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX7.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
		} else {
			pMailBox->uMBOX_TX1.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX2.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX3.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX4.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX5.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX6.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
			pMailBox->uMBOX_TX7.nREG =
			    (pARG->uiVectorData[pARG->uiVectorIndex++]);
		}
	} else {
		pMailBox->uMBOX_TX0.nREG =
		    HW_DEMUX_ADD_FILTER << 24 | (pARG->uiDMXID & 0xff) << 16;
		pMailBox->uMBOX_TX1.nREG =
		    pARG->uiTYPE << 24 | (pARG->uiPID & 0xffff) << 8;
		pMailBox->uMBOX_TX2.nREG = 0;
		pMailBox->uMBOX_TX3.nREG = 0;
		pMailBox->uMBOX_TX4.nREG = 0;
		pMailBox->uMBOX_TX5.nREG = 0;
		pMailBox->uMBOX_TX6.nREG = 0;
		pMailBox->uMBOX_TX7.nREG = 0;
	}

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_ADD_FILTER]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int TSDMXCMD_DELETE_Filter(struct ARG_TSDMX_DELETE_FILTER *pARG,
			   unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_DELETE_FILTER << 24 | (
	    pARG->uiDMXID & 0xff) << 16 | (
	    pARG->uiFID & 0xff) << 8
	    | (pARG->uiTYPE & 0xff);
	pMailBox->uMBOX_TX1.nREG = (pARG->uiPID & 0xffff) << 16;
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_DELETE_FILTER]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

long long TSDMXCMD_GET_STC(unsigned int uiDMXID, unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_GET_STC << 24 | (uiDMXID & 0xff) << 16 |
	    (REQUEST_STC_INTERVAL & 0xffff);
	pMailBox->uMBOX_TX1.nREG = 0;
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high

	return 0;
}

int TSDMXCMD_SET_PCR_PID(struct ARG_TSDMX_SET_PCR_PID *pARG,
	unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_SET_PCR_PID << 24 | (pARG->uiDMXID & 0xff) << 16 | (
	    pARG->uiPCRPID & 0xffff);
	pMailBox->uMBOX_TX1.nREG =
	    (REQUEST_STC_INTERVAL & 0xffff) << 16 | (
	    REQUEST_PCRUPDATE_RES_MS & 0xffff);
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_SET_PCR_PID]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int TSDMXCMD_SET_IN_BUFFER(struct ARG_TSDMX_SET_IN_BUFFER *pARG,
			   unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_INTERNAL_SET_INPUT << 24 | (pARG->uiDMXID & 0xff) << 16;
	pMailBox->uMBOX_TX1.nREG = pARG->uiInBufferAddr;
	pMailBox->uMBOX_TX2.nREG = pARG->uiInBufferSize;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_INTERNAL_SET_INPUT]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int TSDMXCMD_GET_VERSION(unsigned int uiDMXID, unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_GET_VERSION << 24 | (uiDMXID & 0xff) << 16;
	pMailBox->uMBOX_TX1.nREG = 0;
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_GET_VERSION]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}

	return g_ret;
}

int TSDMXCMD_CHANGE_INTERFACE(unsigned int uiDMXID,
			      unsigned int uiTSIFInterface,
			      unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    HW_DEMUX_CHANGE_INTERFACE << 24 | (uiDMXID & 0xff) << 16;
	pMailBox->uMBOX_TX1.nREG = uiTSIFInterface;
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_CHANGE_INTERFACE]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int HWDEMUXCIPHERCMD_Set_Algorithm(struct stHWDEMUXCIPHER_ALGORITHM *pARG,
				   unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    (HW_CM_CIPHER_SET_ALGORITHM << 24 | (pARG->uDemuxid & 0xff << 16) |
	     (pARG->uOperationMode & 0xff) << 8 | (pARG->uAlgorithm & 0xff));
	pMailBox->uMBOX_TX1.nREG =
	    ((pARG->uArgument1 & 0xff) << 24 | (
	    pARG->uArgument1 & 0xff) << 16 |
	     (pARG->uArgument1 & 0xff) << 8 | (pARG->uArgument2 & 0xff));
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_CIPHER_SET_ALGORITHM]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int HWDEMUXCIPHERCMD_Set_Key(struct stHWDEMUXCIPHER_KEY *pARG,
			     unsigned int uiTotalIndex,
			     unsigned int uiCurrentIndex,
			     unsigned int uiTimeOut)
{
	unsigned long *pulKeyData;

	volatile struct PMAILBOX *pMailBox;

	pulKeyData = (unsigned long *)pARG->pucData;
	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    (HW_CM_CIPHER_SET_KEY << 24 | (pARG->uDemuxid & 0xff << 16) |
	     (pARG->uLength & 0xff) << 8 | (pARG->uOption & 0xff));
	pMailBox->uMBOX_TX1.nREG =
	    (uiTotalIndex & 0xf) << 28 | (uiCurrentIndex & 0xf) << 24;

	if (uiCurrentIndex == 1) {
		pMailBox->uMBOX_TX2.nREG = *pulKeyData++;
		pMailBox->uMBOX_TX3.nREG = *pulKeyData++;

		if (pARG->uLength > 8) {
			pMailBox->uMBOX_TX4.nREG = *pulKeyData++;
			pMailBox->uMBOX_TX5.nREG = *pulKeyData++;
		} else {
			pMailBox->uMBOX_TX4.nREG = 0;
			pMailBox->uMBOX_TX5.nREG = 0;
		}

		if (pARG->uLength > 16) {
			pMailBox->uMBOX_TX6.nREG = *pulKeyData++;
			pMailBox->uMBOX_TX7.nREG = *pulKeyData++;
		} else {
			pMailBox->uMBOX_TX6.nREG = 0;
			pMailBox->uMBOX_TX7.nREG = 0;
		}
	} else {
		pMailBox->uMBOX_TX2.nREG =
		    *(pulKeyData + ((uiCurrentIndex - 1) * 6));
		pMailBox->uMBOX_TX3.nREG =
		    *(pulKeyData + ((uiCurrentIndex - 1) * 6) + 1);
		pMailBox->uMBOX_TX4.nREG = 0;
		pMailBox->uMBOX_TX5.nREG = 0;
		pMailBox->uMBOX_TX6.nREG = 0;
		pMailBox->uMBOX_TX7.nREG = 0;
	}

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_CIPHER_SET_KEY]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int HWDEMUXCIPHERCMD_Set_IV(struct stHWDEMUXCIPHER_VECTOR *pARG,
			    unsigned int uiTimeOut)
{
	unsigned long *pulVectorData;

	volatile struct PMAILBOX *pMailBox;

	pulVectorData = (unsigned long *)pARG->pucData;
	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    (HW_CM_CIPHER_SET_IV << 24 | (pARG->uDemuxid & 0xff << 16) |
	     (pARG->uLength & 0xff) << 8 | (pARG->uOption & 0xff));
	pMailBox->uMBOX_TX1.nREG = *pulVectorData++;
	pMailBox->uMBOX_TX2.nREG = *pulVectorData++;
	if (pARG->uLength > 8) {
		pMailBox->uMBOX_TX3.nREG = *pulVectorData++;
		pMailBox->uMBOX_TX4.nREG = *pulVectorData++;
	} else {
		pMailBox->uMBOX_TX3.nREG = 0;
		pMailBox->uMBOX_TX4.nREG = 0;
	}
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_CIPHER_SET_IV]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}

int HWDEMUXCIPHERCMD_Cipher_Run(struct stHWDEMUXCIPHER_EXECUTE *pARG,
				unsigned int uiTimeOut)
{
	volatile struct PMAILBOX *pMailBox;

	pMailBox = hHWDMX.mbox0_base;

	BITCLR(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN low

	pMailBox->uMBOX_TX0.nREG =
	    (HW_CM_CIPHER_EXECUTE << 24 | (pARG->uDemuxid & 0xff << 16) |
	     (pARG->uExecuteOption & 0xff) << 8 | (
	     pARG->uCipherExecute & 0xff));
	pMailBox->uMBOX_TX1.nREG = 0;
	pMailBox->uMBOX_TX2.nREG = 0;
	pMailBox->uMBOX_TX3.nREG = 0;
	pMailBox->uMBOX_TX4.nREG = 0;
	pMailBox->uMBOX_TX5.nREG = 0;
	pMailBox->uMBOX_TX6.nREG = 0;
	pMailBox->uMBOX_TX7.nREG = 0;

	g_ret_done = 0;
	BITSET(pMailBox->uMBOX_CTL_016.nREG, Hw5);	//OEN high
	wait_event_timeout((cmd_queue[QUEUE_HW_DEMUX_CIPHER_EXECUTE]),
			   g_ret_done == 1, uiTimeOut);
	if (g_ret_done == 0) {
		pr_err("[ERROR][HWDMX]%s:time-out\n", __func__);
		return -2;	//time out
	}
	return g_ret;
}
