/****************************************************************************
 *
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tcc_sc_fw

#if !defined(_TRACE_TCC_SC_FW_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TCC_SC_FW_H

#include <linux/mailbox/mailbox-tcc.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include <linux/device.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tcc_sc_fw_start_mmc_req,

	TP_PROTO(struct tcc_sc_fw_mmc_cmd *cmd, struct tcc_sc_fw_mmc_data *dat),

	TP_ARGS(cmd, dat),

	TP_STRUCT__entry(
		__field(u32,			cmd_opcode)
		__field(u32,			cmd_arg)
		__field(unsigned int,		cmd_flags)
		__field(unsigned int,		cmd_part_num)
		__field(unsigned int,			blksz)
		__field(unsigned int,			blocks)
		__field(unsigned int,		blk_addr)
		__field(unsigned int,		flags)
		__field(unsigned int,			sg_len)
		__field(int,			sg_count)
		__field(struct scatterlist*,	sg)
		__field(struct tcc_sc_fw_mmc_cmd *, cmd)
		__field(struct tcc_sc_fw_mmc_data *, data)
	),

	TP_fast_assign(
		__entry->cmd_opcode = cmd ? cmd->opcode : 0;
		__entry->cmd_arg = cmd ? cmd->arg : 0;
		__entry->cmd_flags = cmd ? cmd->flags : 0;
		__entry->cmd_part_num = cmd ? cmd->part_num : 0;
		__entry->blksz = dat ? dat->blksz : 0;
		__entry->blocks = dat ? dat->blocks : 0;
		__entry->blk_addr = dat ? dat->blk_addr : 0;
		__entry->sg_len = dat ? dat->sg_len : 0;
		__entry->sg_count = dat ? dat->sg_count : 0;
		__entry->sg = dat ? dat->sg : NULL;
		__entry->cmd = cmd;
		__entry->data = dat;
	),

	TP_printk(
		"start mmc_req struct tcc_sc_fw_mmc_cmd[%p]: cmd_opcode=%u cmd_arg=0x%x cmd_flags=0x%x cmd_part_num=%u struct tcc_sc_fw_mmc_data[%p]: blocks=%u block_size=%u blk_addr=%u data_flags=0x%x sg_len=%u sg_count=%u sg=%p",
		  __entry->cmd,
		  __entry->cmd_opcode, __entry->cmd_arg,
		  __entry->cmd_flags, __entry->cmd_part_num,
		  __entry->data,
		  __entry->blocks, __entry->blksz,
		  __entry->blk_addr, __entry->flags,
		  __entry->sg_len, __entry->sg_count,
		  __entry->sg)
);

TRACE_EVENT(tcc_sc_fw_done_mmc_req,

	TP_PROTO(struct tcc_sc_fw_mmc_cmd *cmd, struct tcc_sc_fw_mmc_data *dat),

	TP_ARGS(cmd, dat),

	TP_STRUCT__entry(
		__field(u32,			cmd_opcode)
		__field(u32,			cmd_arg)
		__field(unsigned int,		cmd_flags)
		__field(unsigned int,		cmd_part_num)
		__field(unsigned int,		cmd_error)
		__field(u32,			cmd_resp0)
		__field(u32,			cmd_resp1)
		__field(u32,				cmd_resp2)
		__field(u32,			cmd_resp3)
		__field(struct tcc_sc_fw_mmc_cmd *, cmd)
	),

	TP_fast_assign(
		__entry->cmd_opcode = cmd ? cmd->opcode : 0;
		__entry->cmd_arg = cmd ? cmd->arg : 0;
		__entry->cmd_flags = cmd ? cmd->flags : 0;
		__entry->cmd_part_num = cmd ? cmd->part_num : 0;
		__entry->cmd_error = cmd ? cmd->error : 0;
		__entry->cmd_resp0 = cmd ? cmd->resp[0] : 0;
		__entry->cmd_resp1 = cmd ? cmd->resp[1] : 0;
		__entry->cmd_resp2 = cmd ? cmd->resp[2] : 0;
		__entry->cmd_resp3 = cmd ? cmd->resp[3] : 0;
	),

	TP_printk(
		"end mmc_req struct tcc_sc_fw_mmc_cmd[%p]: cmd_opcode=%u cmd_arg=0x%x cmd_flags=0x%x cmd_part_num=%u cmd_error=%d cmd_resp[0]=%u, cmd_resp[1]=%u, cmd_resp[2]=%u, cmd_resp[3]=%u",
		  __entry->cmd,
		  __entry->cmd_opcode, __entry->cmd_arg,
		  __entry->cmd_flags, __entry->cmd_part_num, __entry->cmd_error,
		  __entry->cmd_resp0, __entry->cmd_resp1, __entry->cmd_resp2,
		  __entry->cmd_resp3)
);

TRACE_EVENT(tcc_sc_fw_start_xfer,

	TP_PROTO(struct tcc_sc_fw_xfer *xfer),

	TP_ARGS(xfer),

	TP_STRUCT__entry(
		__field(struct tcc_sc_fw_xfer *,	xfer)
		__field(u32,			cmd_len)
		__field(u32,			data_len)
		__field(u32,			cmd0)
		__field(u32,			cmd1)
	),

	TP_fast_assign(
		__entry->xfer = xfer;
		__entry->cmd_len = xfer ? xfer->tx_mssg.cmd_len : 0;
		__entry->data_len = xfer ? xfer->tx_mssg.data_len : 0;
		__entry->cmd0 = xfer ? xfer->tx_mssg.cmd[0] : 0;
		__entry->cmd1 = xfer ? xfer->tx_mssg.cmd[1] : 0;
	),

	TP_printk("start xfer[%p]: cmd_len %u data_len %u cmd[0] 0x%08x cmd[1] 0x%08x",
		  __entry->xfer, __entry->cmd_len,  __entry->data_len,
		  __entry->cmd0, __entry->cmd1)
);

TRACE_EVENT(tcc_sc_fw_done_xfer,

	TP_PROTO(struct tcc_sc_fw_xfer *xfer),

	TP_ARGS(xfer),

	TP_STRUCT__entry(
		__field(struct tcc_sc_fw_xfer *,	xfer)
		__field(u32,			cmd_len)
		__field(u32,			data_len)
		__field(u32,			cmd0)
		__field(u32,			cmd1)
	),

	TP_fast_assign(
		__entry->xfer = xfer;
		__entry->cmd_len = xfer ? xfer->rx_mssg.cmd_len : 0;
		__entry->data_len = xfer ? xfer->rx_mssg.data_len : 0;
		__entry->cmd0 = xfer ? xfer->rx_mssg.cmd[0] : 0;
		__entry->cmd1 = xfer ? xfer->rx_mssg.cmd[1] : 0;
	),

	TP_printk("done xfer[%p]: cmd_len %u data_len %u cmd[0] 0x%08x cmd[1] 0x%08x",
		  __entry->xfer, __entry->cmd_len,  __entry->data_len,
		  __entry->cmd0, __entry->cmd1)
);

TRACE_EVENT(tcc_sc_fw_rx_invalid_message,

	TP_PROTO(struct tcc_sc_mbox_msg *msg),

	TP_ARGS(msg),

	TP_STRUCT__entry(
		__field(struct tcc_sc_mbox_msg *,	msg)
		__field(u32,			cmd_len)
		__field(u32,			data_len)
		__field(u32,			cmd0)
		__field(u32,			cmd1)
	),

	TP_fast_assign(
		__entry->msg = msg;
		__entry->cmd_len = msg ? msg->cmd_len : 0;
		__entry->data_len = msg ? msg->data_len : 0;
		__entry->cmd0 = msg ? msg->cmd[0] : 0;
		__entry->cmd1 = msg ? msg->cmd[1] : 0;
	),

	TP_printk("invalid rx_msg[%p]: cmd_len %u data_len %u cmd[0] 0x%08x cmd[1] 0x%08x",
		  __entry->msg, __entry->cmd_len,  __entry->data_len,
		  __entry->cmd0, __entry->cmd1)
);

TRACE_EVENT(tcc_sc_fw_xfer_status,

	TP_PROTO(struct tcc_sc_fw_xfer *xfer),

	TP_ARGS(xfer),

	TP_STRUCT__entry(
		__field(struct tcc_sc_fw_xfer *,	xfer)
		__field(u32,			status)
		__field(u32,			tx_cmd_len)
		__field(u32,			tx_dat_len)
		__field(u32,			tx_cmd0)
		__field(u32,			tx_cmd1)
		__field(u32,			rx_cmd_len)
		__field(u32,			rx_dat_len)
		__field(u32,			rx_cmd0)
		__field(u32,			rx_cmd1)
	),

	TP_fast_assign(
		__entry->xfer = xfer;
		__entry->status = xfer->status;
		__entry->tx_cmd_len = xfer ? xfer->tx_mssg.cmd_len : 0;
		__entry->tx_dat_len = xfer ? xfer->tx_mssg.data_len : 0;
		__entry->tx_cmd0 = xfer ? xfer->tx_mssg.cmd[0] : 0;
		__entry->tx_cmd1 = xfer ? xfer->tx_mssg.cmd[1] : 0;
		__entry->rx_cmd_len = xfer ? xfer->rx_mssg.cmd_len : 0;
		__entry->rx_dat_len = xfer ? xfer->rx_mssg.data_len : 0;
		__entry->rx_cmd0 = xfer ? xfer->rx_mssg.cmd[0] : 0;
		__entry->rx_cmd1 = xfer ? xfer->rx_mssg.cmd[1] : 0;
	),

	TP_printk("Status %u xfer[%p]: TX(%u, %u): cmd[0] 0x%08x cmd[1] 0x%08x RX(%u, %u): cmd[0] 0x%08x cmd[1] 0x%08x",
		  __entry->status, __entry->xfer,
		  __entry->tx_cmd_len, __entry->tx_dat_len, __entry->tx_cmd0, __entry->tx_cmd1,
		  __entry->rx_cmd_len, __entry->rx_dat_len, __entry->rx_cmd0, __entry->rx_cmd1)
);

#endif /* _TRACE_TCC_SC_FW_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
