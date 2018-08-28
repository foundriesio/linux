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

#include <mach/tca_mailbox.h>

#include "tcc_dsp.h"
#include "tcc_dsp_ctrl.h"

#define MBOX_TIMEOUT    1000000

struct tcc_dsp_ipc_list_t {
	struct tcc_dsp_ipc_data_t data;
	struct list_head      queue; //for linked list
};

struct tcc_dsp_ipc_t {
	struct platform_device *pdev;
	struct kthread_worker  kworker;
	struct task_struct     *kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             rx_queue_lock;
	struct list_head       rx_queue;

	tcc_dsp_ipc_handler_t  handler;
	void                   *handler_pdata;
};

static spinlock_t             tx_queue_lock;
static struct tcc_dsp_ipc_t *g_dsp_ipc_info[TCC_DSP_IPC_MAX] = {0};

void tcc_dsp_ipc_pump_messages(struct kthread_work *work)
{
	struct tcc_dsp_ipc_t *ipc = container_of(work, struct tcc_dsp_ipc_t, pump_messages);
	struct tcc_dsp_ipc_list_t *ipc_list, *ipc_list_tmp;
	unsigned long flags;

	spin_lock_irqsave(&ipc->rx_queue_lock, flags);

	list_for_each_entry_safe(ipc_list, ipc_list_tmp, &ipc->rx_queue, queue) {
		if (ipc->handler) {
			spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);
			ipc->handler(&ipc_list->data, ipc->handler_pdata); //call IPC handler
			spin_lock_irqsave(&ipc->rx_queue_lock, flags);
		}

		list_del_init(&ipc_list->queue);
		kfree(ipc_list);
	}

	spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);
}


int tcc_dsp_add_queue_and_work(struct tcc_dsp_ipc_t *ipc, struct tcc_dsp_ipc_list_t *ipc_list)
{
	unsigned long flags;

	spin_lock_irqsave(&ipc->rx_queue_lock, flags);

	if (ipc_list) {
		list_add_tail(&ipc_list->queue, &ipc->rx_queue);
	}
	spin_unlock_irqrestore(&ipc->rx_queue_lock, flags);

	queue_kthread_work(&ipc->kworker, &ipc->pump_messages);

	return 0;
}

int tcc_dsp_ipc_init(struct tcc_dsp_ipc_t *ipc, tcc_dsp_ipc_handler_t handler, void *handler_pdata, char *name)
{
	INIT_LIST_HEAD(&ipc->rx_queue);
	spin_lock_init(&ipc->rx_queue_lock);

	ipc->handler = handler;
	ipc->handler_pdata = handler_pdata;

	init_kthread_worker(&ipc->kworker);
	ipc->kworker_task = kthread_run(kthread_worker_fn,
			&ipc->kworker,
			name);
	if (IS_ERR(ipc->kworker_task)) {
		pr_err("failed to create message pump task\n");
		return -ENOMEM;
	}
	init_kthread_work(&ipc->pump_messages, tcc_dsp_ipc_pump_messages);

	return 0;
}

int register_tcc_dsp_ipc(unsigned int ipc_id, tcc_dsp_ipc_handler_t handler, void *handler_pdata, char *name)
{
	if (ipc_id >= TCC_DSP_IPC_MAX) {
		return -EINVAL;
	}

	if (g_dsp_ipc_info[ipc_id]) {
		pr_err("ipc_id %d is already registered\n", ipc_id);
		return -EINVAL;
	}

	g_dsp_ipc_info[ipc_id] = kzalloc(sizeof(struct tcc_dsp_ipc_t), GFP_KERNEL);

	return tcc_dsp_ipc_init(g_dsp_ipc_info[ipc_id], handler, handler_pdata, name);
}

EXPORT_SYMBOL(register_tcc_dsp_ipc);

int deregister_tcc_dsp_ipc(unsigned int ipc_id)
{
	if (ipc_id >= TCC_DSP_IPC_MAX) {
		return -EINVAL;
	}

	if (!g_dsp_ipc_info[ipc_id]) {
		pr_err("ipc_id %d isn't registered\n", ipc_id);
		return -EINVAL;
	}

	kfree(g_dsp_ipc_info[ipc_id]);
	g_dsp_ipc_info[ipc_id] = NULL;

	return 0;
}
EXPORT_SYMBOL(deregister_tcc_dsp_ipc);

int tcc_dsp_ipc_send_msg(struct tcc_dsp_ipc_data_t *p)
{
	int ret = 0;

	spin_lock(&tx_queue_lock);
	ret = tca_mailbox_send((unsigned int *)p, MBOX_TIMEOUT);
	spin_unlock(&tx_queue_lock);

	if (ret == 0) {
		pr_err("%s : timeout occured\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL(tcc_dsp_ipc_send_msg);

void mbox_handler(unsigned int *pdata)
{
	struct tcc_dsp_ipc_list_t *ipc_list = kzalloc(sizeof(struct tcc_dsp_ipc_list_t), GFP_ATOMIC);

	INIT_LIST_HEAD(&ipc_list->queue);

	memcpy(&ipc_list->data, pdata, sizeof(ipc_list->data));

	if (ipc_list->data.id >= TCC_DSP_IPC_MAX) {
		return; //-EINVAL
	}

	if (g_dsp_ipc_info[ipc_list->data.id]) {
		tcc_dsp_add_queue_and_work(g_dsp_ipc_info[ipc_list->data.id], ipc_list);
	} else {
		pr_debug("%s : ipc id(0x%x) is not registered\n", __func__, ipc_list->data.id);
	}
}

int tcc_dsp_ipc_initialize(void)
{
	tca_mailbox_init(mbox_handler);
	memset(g_dsp_ipc_info, 0, sizeof(struct tcc_dsp_ipc_t*) * TCC_DSP_IPC_MAX);
	spin_lock_init(&tx_queue_lock);

	return 0;
}

EXPORT_SYMBOL(tcc_dsp_ipc_initialize);

