/****************************************************************************
 *
 * Copyright (C) 2020 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tcc_sc_mbox

#if !defined(_TRACE_TCC_SC_MBOX_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TCC_SC_MBOX_H

#include <linux/mailbox/mailbox-tcc.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include <linux/device.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tcc_sc_mbox_tx_start,

	TP_PROTO(struct tcc_sc_mbox_msg *msg),

	TP_ARGS(msg),

	TP_STRUCT__entry(
		__field(u32,			cmd_len)
		__field(u32,			data_len)
		__field(struct tcc_sc_mbox_msg *,	msg)
	),

	TP_fast_assign(
		__entry->cmd_len = msg ? msg->cmd_len : 0;
		__entry->data_len = msg ? msg->data_len : 0;
		__entry->msg = msg;
	),

	TP_printk("tx_start struct tcc_sc_mbox_msg[%p]: "
		  "cmd_len %u data_len %u",
		  __entry->msg, __entry->cmd_len,  __entry->data_len)
);

TRACE_EVENT(tcc_sc_mbox_tx_done,

	TP_PROTO(struct device *dev),

	TP_ARGS(dev),

	TP_STRUCT__entry(
		__string(name,			dev_name(dev))
		__field(struct device *,	dev)
	),

	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__entry->dev = dev;
	),

	TP_printk("tx_done dev %s", __get_str(name))
);


TRACE_EVENT(tcc_sc_mbox_rx_irq,

	TP_PROTO(struct device *dev),

	TP_ARGS(dev),

	TP_STRUCT__entry(
		__string(name,			dev_name(dev))
		__field(struct device *,	dev)
	),

	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__entry->dev = dev;
	),

	TP_printk("rx_irq dev %s", __get_str(name))
);

TRACE_EVENT(tcc_sc_mbox_rx_isr,

	TP_PROTO(struct device *dev),

	TP_ARGS(dev),

	TP_STRUCT__entry(
		__string(name,			dev_name(dev))
		__field(struct device *,	dev)
	),

	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__entry->dev = dev;
	),

	TP_printk("rx_isr dev %s", __get_str(name))
);

#endif /* _TRACE_TCC_SC_MBOX_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
