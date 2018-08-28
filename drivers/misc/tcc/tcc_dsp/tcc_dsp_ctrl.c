/****************************************************************************
 * Copyright (C) 2015 Telechips Inc.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <linux/dma-mapping.h>
#include <linux/completion.h>

#include <mach/tca_mailbox.h>

#include "tcc_dsp.h"
#include "tcc_dsp_ctrl.h"

#define TCC_DSP_CTRL_WAIT_TIMEOUT	(1000)

void tcc_dsp_ctrl_handler(struct tcc_dsp_ipc_data_t *ipc_data, void *pdata)
{
	struct tcc_dsp_ctrl_t *p = (struct tcc_dsp_ctrl_t*)pdata;
	struct tcc_ctrl_msg_t *msg = (struct tcc_ctrl_msg_t*)ipc_data->param;
	//unsigned int type = (ipc_data->type & DSP_CTRL_MSG_TYPE_MASK) >> DSP_CTRL_MSG_TYPE_OFFSET;

	if ((ipc_data->type & DSP_CTRL_MSG_ACK_FLAG) && (msg->seq_no == p->seq_no)) {
		if (ipc_data->type & DSP_CTRL_MSG_SINGLE_FLAG) {
			memcpy(p->cur_single_msgbuf, msg->u.single.value, SINGLE_MSG_MAX*sizeof(unsigned int));
			complete(&p->wait);
		} else if (ipc_data->type & DSP_CTRL_MSG_BULK_FLAG) {
			complete(&p->wait);
		}
	} else {
		printk("id:0x%08x, type:0x%08x, param[0-5]:0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x\n",
				ipc_data->id, ipc_data->type,
				ipc_data->param[0], ipc_data->param[1], ipc_data->param[2],
				ipc_data->param[3], ipc_data->param[4], ipc_data->param[5]);
	}
}

int tcc_dsp_ctrl_bulk_message(struct tcc_dsp_ctrl_t *p, unsigned int type, void *buf, unsigned int size)
{
	struct tcc_dsp_ipc_data_t ipc_data;
	struct tcc_ctrl_msg_t *msg = (struct tcc_ctrl_msg_t*)ipc_data.param;
	dma_addr_t phys_addr;
	int ret = 0;

	mutex_lock(&p->lock);

	ipc_data.id = TCC_DSP_IPC_ID_CONTROL;
	ipc_data.type = type | DSP_CTRL_MSG_BULK_FLAG;
	msg->seq_no = p->seq_no;
	msg->u.bulk.size = size;

	phys_addr = dma_map_single(p->dev, buf, size, DMA_TO_DEVICE);
	if (phys_addr == (unsigned int)NULL) {
		ret = -EFAULT;
		goto out;
	}
	msg->u.bulk.phys_addr = (unsigned int)phys_addr;

	tcc_dsp_ipc_send_msg(&ipc_data);

	if (wait_for_completion_interruptible_timeout(&p->wait, TCC_DSP_CTRL_WAIT_TIMEOUT) == 0) {
		printk("%s timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto out;
	} 

	p->seq_no++;
	dma_unmap_single(p->dev, phys_addr, size, DMA_FROM_DEVICE);
out:
	mutex_unlock(&p->lock);

	return ret;
}


int tcc_dsp_ctrl_single_message(struct tcc_dsp_ctrl_t *p, unsigned int type, void *buf, unsigned int size)
{
	struct tcc_dsp_ipc_data_t ipc_data;
	struct tcc_ctrl_msg_t *msg = (struct tcc_ctrl_msg_t*)ipc_data.param;
	int ret = 0;

	mutex_lock(&p->lock);

	ipc_data.id = TCC_DSP_IPC_ID_CONTROL;
	if (size > SINGLE_MSG_MAX*sizeof(unsigned int)) {
		printk("SINGLE MSG's size is over(%d)\n", size);
		goto out;
	}
	ipc_data.type = type | DSP_CTRL_MSG_SINGLE_FLAG;
	msg->seq_no = p->seq_no;

	memcpy(msg->u.single.value, buf, size);

	p->cur_single_msgbuf = buf;

	tcc_dsp_ipc_send_msg(&ipc_data);

	if (wait_for_completion_interruptible_timeout(&p->wait, TCC_DSP_CTRL_WAIT_TIMEOUT) == 0) {
		printk("%s timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto out;
	} 
	p->cur_single_msgbuf = NULL;
	p->seq_no++;
out:
	mutex_unlock(&p->lock);

	return ret;
}

int tcc_control_set_param(struct tcc_dsp_ctrl_t *p, struct tcc_control_param_t *param) 
{
	return tcc_dsp_ctrl_bulk_message(p, TCC_DSP_CONTROL_SET_PARAM, param, sizeof(struct tcc_control_param_t));
}

int tcc_control_get_param(struct tcc_dsp_ctrl_t *p, struct tcc_control_param_t *param) 
{
	return tcc_dsp_ctrl_bulk_message(p, TCC_DSP_CONTROL_GET_PARAM, param, sizeof(struct tcc_control_param_t));
}

int tcc_am3d_set_param(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_t *param) 
{
	return tcc_dsp_ctrl_bulk_message(p, TCC_DSP_AM3D_SET_PARAM, param, sizeof(struct tcc_am3d_param_t));
}

int tcc_am3d_set_param_tbl(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_tbl_t *param_tbl) 
{
	return tcc_dsp_ctrl_bulk_message(p, TCC_DSP_AM3D_SET_PARAM_TABLE, param_tbl, sizeof(struct tcc_am3d_param_tbl_t));
}

int tcc_am3d_get_param(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_t *param) 
{
	return tcc_dsp_ctrl_bulk_message(p, TCC_DSP_AM3D_GET_PARAM, param, sizeof(struct tcc_am3d_param_t));
}

int tcc_am3d_get_param_tbl(struct tcc_dsp_ctrl_t *p, struct tcc_am3d_param_tbl_t *param_tbl) 
{
	return tcc_dsp_ctrl_bulk_message(p, TCC_DSP_AM3D_GET_PARAM_TABLE, param_tbl, sizeof(struct tcc_am3d_param_tbl_t));
}

int tcc_dsp_ctrl_init(struct tcc_dsp_ctrl_t *p, struct device *dev)
{
	int ret = 0;
	
	p->seq_no = 0;
	p->dev = dev;
	init_completion(&p->wait);
	mutex_init(&p->lock);

	ret = register_tcc_dsp_ipc(TCC_DSP_IPC_ID_CONTROL, tcc_dsp_ctrl_handler, p, "tcc_am3d_handler");

	return ret;
}
EXPORT_SYMBOL(tcc_dsp_ctrl_init);

