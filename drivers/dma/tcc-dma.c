// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>

#include <linux/platform_data/dma-tcc.h>
#include <linux/types.h>

#include "dmaengine.h"

#define TCC_MAX_DMA_CHANNELS	(5)
#define TCC_DMA_MAX_XFER_CNT	(0xFFFF)

#define DMA_CHANNEL_REG_OFFSET	(0x30)

#define DMA_ST_SADR	(0x0)	/* Start Source Address register */
#define DMA_SPARAM	(0x4)	/* Source Block Parameter register */
#define DMA_C_SADR	(0xC)	/* Current Source Address register */
#define DMA_ST_DADR	(0x10)	/* Start Destination Address register */
#define DMA_DPARAM	(0x14)	/* Destination Block Parameter register */
#define DMA_HCOUNT	(0x20)	/* HOP Count register */
#define DMA_CHCTRL	(0x24)	/* Channel Control register */
#define DMA_RPTCTRL	(0x28)	/* Repeat Control register */
#define DMA_EXTREQ	(0x2C)	/* External DMA Request register */

#define DMA_CHCONFIG	(0x90)	/* Channel Configuration register */

#define DMA_AC0_START	(0x00)
#define DMA_AC0_LIMIT	(0x04)
#define DMA_AC1_START	(0x08)
#define DMA_AC1_LIMIT	(0x0C)
#define DMA_AC2_START	(0x10)
#define DMA_AC2_LIMIT	(0x14)
#define DMA_AC3_START	(0x18)
#define DMA_AC3_LIMIT	(0x1C)

#define DMA_SPARAM_SMASK(x)	(((x) & 0xFFFFFFUL) << 8U)
#define DMA_SPARAM_SINC(x)	((x) & 0xFFU)

#define DMA_DPARAM_DMASK(x)	(((x) & 0xFFFFFFU) << 8U)
#define DMA_DPARAM_DINC(x)	((x) & 0xFFU)

#define DMA_HCOUNT_ST_HCOUNT(x)	((x) & 0xFFFFU)

/* DMA channel enable */
#define DMA_CHCTRL_EN		((u32)1U << 0U)
/* repeat mode */
#define DMA_CHCTRL_REP		((u32)1U << 1U)
/* interrupt enable */
#define DMA_CHCTRL_IEN		((u32)1U << 2U)
/* DMA done flag */
#define DMA_CHCTRL_FLAG		((u32)1U << 3U)
/* word size */
#define DMA_CHCTRL_WSIZE(x)	((u32)(x) << 4U)
/* burst size */
#define DMA_CHCTRL_BSIZE(x)	((u32)(x) << 6U)
/* transfer type */
#define DMA_CHCTRL_TYPE(x)	((u32)(x) << 8U)
/* burst transfer */
#define DMA_CHCTRL_BST		((u32)1U << 10U)
/* locked transfer */
#define DMA_CHCTRL_LOCK		((u32)1U << 11U)
/* hardware request direction */
#define DMA_CHCTRL_HRD		((u32)1U << 12U)
/* hardware request sync */
#define DMA_CHCTRL_SYNC		((u32)1U << 13U)
/* differential transfer mode */
#define DMA_CHCTRL_DTM		((u32)1U << 14U)
/* continuous transfer */
#define DMA_CHCTRL_CONT		((u32)1U << 15U)

#define DMA_CHCONFIG_IS(x, n)	(((x) >> 20U) & ((u32)1U << (n)))
#define DMA_CHCONFIG_MIS(x, n)	(((x) >> 16U) & ((u32)1U << (n)))

struct tcc_dma_desc {
	struct list_head node;
	struct list_head tx_list;
	struct dma_async_tx_descriptor txd;

	u32 src_addr;
	u32 src_inc;
	u32 dst_addr;
	u32 dst_inc;
	u32 len;
	u32 burst;
	s32 slave_id;
};

struct tcc_dma_chan {
	struct dma_chan chan;
	struct dma_pool *desc_pool;
	struct device *dev;
	struct tasklet_struct tasklet;

	struct dma_slave_config slave_config;

	struct list_head desc_submitted;
	struct list_head desc_issued;
	struct list_head desc_completed;

	spinlock_t lock;

	enum dma_transfer_direction direction;
};

struct tcc_dma {
	struct dma_device dma;
	void __iomem *regs;
	void __iomem *req_reg;
	void __iomem *ac_reg;
	struct clk *clk;
	u32 ac_val[4][2];

	struct tcc_dma_chan *chan;
};

struct tcc_dma_of_filter_args {
	struct tcc_dma *tdma;
	s32 chan_id;
	s32 slave_id;
};

static inline struct tcc_dma_chan *to_tcc_dma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct tcc_dma_chan, chan);
}

static inline struct tcc_dma_desc *to_tcc_dma_desc(
					struct dma_async_tx_descriptor *txd)
{
	return container_of(txd, struct tcc_dma_desc, txd);
}

static inline struct tcc_dma *to_tcc_dma(struct dma_device *dma)
{
	return container_of(dma, struct tcc_dma, dma);
}

static inline struct device *chan2dev(struct dma_chan *chan)
{
	return &chan->dev->device;
}

static inline u32 tcc_readl(void __iomem *addr, u32 off)
{
	u32 val;

	val = readl(addr + off);

	return val;
}

static inline void tcc_writel(void __iomem *addr, u32 off, u32 val)
{
	writel(val, addr + off);
}

static inline u32 channel_readl(struct tcc_dma_chan *tdmac, u32 off)
{
	struct dma_chan *chan;
	struct tcc_dma *tdma;
	s32 chan_id;
	s32 reg_off;
	u32 val;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return 0;
	}
	chan = &tdmac->chan;
	chan_id = chan->chan_id;
	tdma = to_tcc_dma(chan->device);
	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL\n", __func__);
		return 0;
	}

	reg_off = (DMA_CHANNEL_REG_OFFSET * chan_id) + (s32)off;
	val = tcc_readl(tdma->regs, (u32)reg_off);
	dev_vdbg(chan2dev(chan), "[DEBUG][GDMA] %s: ch=%d: addr=%p, off=%x, val=%08x\n",
		 __func__, chan_id, tdma->regs, reg_off, val);
	return val;
}

static inline void channel_writel(struct tcc_dma_chan *tdmac, u32 off, u32 val)
{
	struct dma_chan *chan;
	struct tcc_dma *tdma;
	s32 chan_id;
	s32 reg_off;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return;
	}
	chan = &tdmac->chan;
	chan_id = chan->chan_id;
	tdma = to_tcc_dma(chan->device);
	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL\n", __func__);
		return;
	}

	reg_off = (DMA_CHANNEL_REG_OFFSET * chan_id) + (s32)off;
	dev_vdbg(chan2dev(chan), "[DEBUG][GDMA] %s: ch=%d: addr=%p, off=%x, val=%08x\n",
		 __func__, chan_id, tdma->regs, reg_off, val);
	tcc_writel(tdma->regs, (u32)reg_off, val);
}

/**
 * Peak at the next descriptor to be processed.
 */
static struct tcc_dma_desc *tcc_dma_next_desc(struct tcc_dma_chan *tdmac)
{
	struct tcc_dma_desc *ret;

	if (list_empty(&tdmac->desc_issued) != 0) {
		return NULL;
	}

	ret = list_first_entry(&tdmac->desc_issued, struct tcc_dma_desc, node);
	return ret;
}

/**
 * Obtain all submitted and issued descriptors.
 */
static void tcc_dma_get_all_descriptors(struct tcc_dma_chan *tdmac,
					struct list_head *head)
{
	list_splice_tail_init(&tdmac->desc_submitted, head);
	list_splice_tail_init(&tdmac->desc_issued, head);
}

static void tcc_dma_desc_free_list(struct tcc_dma_chan *tdmac,
				   struct list_head *head)
{
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *desc_tmp;

	list_for_each_entry_safe(desc, desc_tmp, head, node) {
		struct dma_async_tx_descriptor *txd = &desc->txd;

		if (txd == NULL) {
			dev_err(tdmac->dev,
				"[ERROR][GDMA] %s: txd is NULL\n", __func__);
			continue;
		}
		dev_vdbg(tdmac->dev, "[DEBUG][GDMA] freeing descriptor %p\n",
			 desc);
		list_del(&desc->node);
		dma_pool_free(tdmac->desc_pool, (void *)desc, txd->phys);
	}
}

/**
 * Obtain all completed descriptors.
 */
static void tcc_dma_get_completed_descriptors(struct tcc_dma_chan *tdmac,
					      struct list_head *head)
{
	list_splice_tail_init(&tdmac->desc_completed, head);
}

static void tcc_dma_clean_completed_descriptors(struct tcc_dma_chan *tdmac)
{
	LIST_HEAD(head);

	tcc_dma_get_completed_descriptors(tdmac, &head);
	tcc_dma_desc_free_list(tdmac, &head);
}

static s32 tcc_dma_width(enum dma_slave_buswidth width)
{
	s32 ret;
	u32 uret;

	switch (width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
		uret = DMA_CHCTRL_WSIZE(0U);
		ret = (s32)uret;
		break;
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
		uret = DMA_CHCTRL_WSIZE(1U);
		ret = (s32)uret;
		break;
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
		uret = DMA_CHCTRL_WSIZE(2U);
		ret = (s32)uret;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static s32 tcc_dma_burst(s32 burst)
{
	s32 ret;
	u32 uret;

	switch (burst) {
	case 1:
		uret = DMA_CHCTRL_BSIZE(0U);
		ret = (s32)uret;
		break;
	case 2:
		uret = DMA_CHCTRL_BSIZE(1U);
		ret = (s32)uret;
		break;
	case 4:
		uret = DMA_CHCTRL_BSIZE(2U);
		ret = (s32)uret;
		break;
	case 8:
		uret = DMA_CHCTRL_BSIZE(3U);
		ret = (s32)uret;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static void tcc_dma_do_single_block(struct tcc_dma_chan *tdmac,
				    struct tcc_dma_desc *desc)
{
	u32 chctrl;
	s32 width;
	s32 burst;
	u32 hop_cnt;

	if ((tdmac == NULL) || (desc == NULL)) {
		(void)pr_err(
			"[ERROR][GDMA] %s: tdmac or desc is NULL\n", __func__);
		return;
	}

	dev_vdbg(chan2dev(&tdmac->chan),
		 "[DEBUG][GDMA] %s: ch%d: src=0x%08x, dest=0x%08x, len=%d\n",
		 __func__, tdmac->chan.chan_id, desc->src_addr,
		 desc->dst_addr, desc->len);

	/* Re-setting current count address
	 * In this time, dma read 1 data from source address but doesn't write
	 * data to destination address.
	 * So, source address must be memory address not peripheral register.
	 */
	channel_writel(tdmac, DMA_CHCTRL, 0);
	if (tdmac->direction == DMA_DEV_TO_MEM) {
		channel_writel(tdmac, DMA_ST_SADR, desc->dst_addr);
	} else {
		channel_writel(tdmac, DMA_ST_SADR, desc->src_addr);
	}
	channel_writel(tdmac, DMA_HCOUNT, 0);
	channel_writel(tdmac, DMA_CHCTRL, 0x201);
	channel_writel(tdmac, DMA_CHCTRL, DMA_CHCTRL_FLAG);

	chctrl = DMA_CHCTRL_IEN | DMA_CHCTRL_WSIZE(0U) | DMA_CHCTRL_BSIZE(0U)
	    | DMA_CHCTRL_FLAG | DMA_CHCTRL_TYPE(2U) | DMA_CHCTRL_EN;

	/* Configure dma channel for current issue */
	if ((desc->len % desc->burst) != 0) {
		dev_err(chan2dev(&tdmac->chan),
			"[ERROR][GDMA] ch%d: len(%d) should be a multiple of burst(%d)\n",
			tdmac->chan.chan_id, desc->len, desc->burst);
		return;
	}
	hop_cnt = desc->len / desc->burst;

	channel_writel(tdmac, DMA_ST_SADR, desc->src_addr);
	channel_writel(tdmac, DMA_SPARAM, DMA_SPARAM_SINC(desc->src_inc));
	channel_writel(tdmac, DMA_ST_DADR, desc->dst_addr);
	channel_writel(tdmac, DMA_DPARAM, DMA_DPARAM_DINC(desc->dst_inc));
	channel_writel(tdmac, DMA_HCOUNT, DMA_HCOUNT_ST_HCOUNT(hop_cnt));
	if (is_slave_direction(tdmac->direction)) {
		u32 slave_id;

		slave_id = (u32)desc->slave_id;
		if (slave_id >= 32U) {
			slave_id = slave_id - 32U;
		}

		chctrl |= DMA_CHCTRL_TYPE(3U);
		channel_writel(tdmac, DMA_EXTREQ, ((u32)1UL << slave_id));
	}

	if (tdmac->slave_config.src_addr_width !=
			DMA_SLAVE_BUSWIDTH_UNDEFINED) {
		width = tcc_dma_width(tdmac->slave_config.src_addr_width);
		burst = tcc_dma_burst((s32)tdmac->slave_config.src_maxburst);
	} else if (tdmac->slave_config.dst_addr_width !=
			DMA_SLAVE_BUSWIDTH_UNDEFINED) {
		width = tcc_dma_width(tdmac->slave_config.dst_addr_width);
		burst = tcc_dma_burst((s32)tdmac->slave_config.dst_maxburst);
	} else {
		width = -1;
		burst = -1;
	}

	/* Set word size */
	if (width == -1) {
		dev_err(chan2dev(&tdmac->chan),
			"[ERROR][GDMA] ch%d: Word size is wrong\n",
			tdmac->chan.chan_id);
		return;
	}
	chctrl |= (u32)width;

	/* Set burst size */
	if (burst == -1) {
		dev_err(chan2dev(&tdmac->chan),
			"[ERROR][GDMA] ch%d: Burst size is wrong\n",
			tdmac->chan.chan_id);
		return;
	}
	chctrl |= (u32)burst;

	channel_writel(tdmac, DMA_CHCTRL, chctrl);
}

static void tcc_dma_tasklet(ulong data)
{
	struct tcc_dma_chan *tdmac = (struct tcc_dma_chan *)data;
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *desc_next;
	ulong flags;

	spin_lock_irqsave(&tdmac->lock, flags);

	desc = tcc_dma_next_desc(tdmac);
	dev_vdbg(tdmac->dev,
		 "[DEBUG][GDMA] %s: chan=%d, cookie=%d: src=0x%08x, dest=0x%08x, len=%d\n",
		 __func__, tdmac->chan.chan_id, desc->txd.cookie,
		 desc->src_addr, desc->dst_addr, desc->len);

	list_move_tail(&desc->node, &tdmac->desc_completed);

	desc_next = tcc_dma_next_desc(tdmac);
	if (desc_next != NULL) {
		tcc_dma_do_single_block(tdmac, desc_next);
		spin_unlock_irqrestore(&tdmac->lock, flags);
		return;
	}

	spin_unlock_irqrestore(&tdmac->lock, flags);

	desc =
	    list_first_entry(&tdmac->desc_completed, struct tcc_dma_desc, node);
	dma_cookie_complete(&desc->txd);

	if (desc->txd.callback != NULL) {
		desc->txd.callback(desc->txd.callback_param);
	}

	tcc_dma_clean_completed_descriptors(tdmac);
#if 0
	INIT_LIST_HEAD(&tdmac->desc_submitted);
	INIT_LIST_HEAD(&tdmac->desc_issued);
#endif
	INIT_LIST_HEAD(&tdmac->desc_completed);
}

static irqreturn_t tcc_dma_interrupt(int irq, void *data)
{
	struct tcc_dma *tdma = (struct tcc_dma *)data;
	u32 i;

	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL(irq: %d)\n",
				__func__, irq);
		return (irqreturn_t)IRQ_NONE;
	}

	for (i = 0U; i < tdma->dma.chancnt; i++) {
		struct tcc_dma_chan *tdmac = &tdma->chan[i];
		u32 chan_id = (u32)tdmac->chan.chan_id;
		u32 chconfig;

		chconfig = tcc_readl(tdma->regs, DMA_CHCONFIG);
		if (DMA_CHCONFIG_MIS(chconfig, chan_id) != (u32)0UL) {
			channel_writel(tdmac, DMA_CHCTRL, DMA_CHCTRL_FLAG);
			tasklet_schedule(&tdmac->tasklet);
		}
	}

	return (irqreturn_t)IRQ_HANDLED;
}

static void tcc_dma_dostart(struct tcc_dma_chan *tdmac,
			    struct tcc_dma_desc *first)
{
	tcc_dma_do_single_block(tdmac, first);
}

static dma_cookie_t tcc_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct tcc_dma_desc *desc;
	struct tcc_dma_chan *tdmac;
	dma_cookie_t cookie;
	ulong flags;

	if (tx == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tx is NULL\n", __func__);
		return -ENXIO;
	}
	desc = to_tcc_dma_desc(tx);
	tdmac = to_tcc_dma_chan(tx->chan);

	spin_lock_irqsave(&tdmac->lock, flags);
	cookie = dma_cookie_assign(tx);
	list_add_tail(&desc->node, &tdmac->desc_submitted);
	if (desc->tx_list.next != &desc->tx_list) {
		list_splice_tail(&desc->tx_list, &tdmac->desc_submitted);
	}

	spin_unlock_irqrestore(&tdmac->lock, flags);

	dev_vdbg(chan2dev(tx->chan),
		 "[DEBUG][GDMA] %s: ch=%d: cookie=%d, tx=%p", __func__,
		 tx->chan->chan_id, cookie, tx);

	return cookie;
}

static struct tcc_dma_desc *tcc_dma_alloc_descriptor(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct tcc_dma_desc *desc;
	dma_addr_t phys;

	desc = dma_pool_alloc(tdmac->desc_pool, GFP_ATOMIC, &phys);
	if (desc == NULL) {
		dev_err(tdmac->dev,
			"[ERROR][GDMA] failed to allocate descriptor pool\n");
		return NULL;
	}
	(void)memset((void *)desc, 0, sizeof(struct tcc_dma_desc));

	INIT_LIST_HEAD(&desc->tx_list);
	dma_async_tx_descriptor_init(&desc->txd, &tdmac->chan);
	desc->txd.tx_submit = &tcc_dma_tx_submit;
	desc->txd.phys = phys;

	return desc;
}

/**
 * Allocate resources for DMA channel
 *
 * Returns the number of descriptors allocated
 */
static int tcc_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return -ENXIO;
	}

	dma_cookie_init(chan);

	tdmac->desc_pool = dmam_pool_create(dev_name(chan2dev(chan)),
					    tdmac->dev,
					    sizeof(struct tcc_dma_desc),
					    __alignof(struct tcc_dma_desc), 0);
	if (tdmac->desc_pool == NULL) {
		dev_err(tdmac->dev,
			"[ERROR][GDMA] failed to create descriptor pool\n");
		return -ENOMEM;
	}

	return 1;
}

/**
 * Free all resources of the channel
 */
static void tcc_dma_free_chan_resources(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	ulong flags;
	LIST_HEAD(head);

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return;
	}

	spin_lock_irqsave(&tdmac->lock, flags);
	tcc_dma_get_all_descriptors(tdmac, &head);
	spin_unlock_irqrestore(&tdmac->lock, flags);

	tcc_dma_desc_free_list(tdmac, &head);
	tcc_dma_clean_completed_descriptors(tdmac);

	dma_pool_destroy(tdmac->desc_pool);
}

static bool tcc_dma_of_filter(struct dma_chan *chan, void *param)
{
	struct tcc_dma_of_filter_args *fargs = param;
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);

	if ((chan == NULL) || (param == NULL)) {
		(void)pr_err(
			"[ERROR][GDMA] %s: chan or param is NULL\n", __func__);
		return (bool)false;
	}

	if (chan->device != &fargs->tdma->dma) {
		return (bool)false;
	}

	chan->chan_id = fargs->chan_id;
	tdmac->slave_config.slave_id = (u32)fargs->slave_id;

	return (bool)true;
}

static void tcc_dma_set_req(struct of_dma *ofdma, s32 num)
{
	struct tcc_dma *tdma = ofdma->of_dma_data;
	u32 req;
	u32 req_val;
	s32 sel;
	s32 sw = 0;

	if (tdma->req_reg == NULL) {
		dev_err(tdma->dma.dev,
				"[ERROR][GDMA] %s req_reg is NULL\n", __func__);
		return;
	}

	if (num > 31) {
		sel = num - 32;
		sw = 1;
	} else {
		sel = num;
	}

	if (sel > 31) {
		dev_err(tdma->dma.dev, "[ERROR][GDMA] %s wrong slave id %d\n",
			__func__, num);
		return;
	}

	req = tcc_readl(tdma->req_reg, 0x0);
	req_val = ((u32)1UL << (u32)sel);
	req &= ~(req_val);
	if (sw == 1) {
		req |= (req_val);
	}

	tcc_writel(tdma->req_reg, 0x0, req);
}

static struct dma_chan *tcc_dma_of_xlate(struct of_phandle_args *dma_spec,
					 struct of_dma *ofdma)
{
	struct tcc_dma *tdma;
	struct tcc_dma_of_filter_args fargs;
	s32 count;
	dma_cap_mask_t cap;

	if ((dma_spec == NULL) || (ofdma == NULL)) {
		(void)pr_err("[ERROR][GDMA] %s: dma_spec or ofdma is NULL\n",
				__func__);
		return NULL;
	}
	tdma = ofdma->of_dma_data;
	count = dma_spec->args_count;

	dev_vdbg(tdma->dma.dev, "[DEBUG][GDMA] %s: args_count=%d, args[0]=%d\n",
			__func__, count, dma_spec->args[0]);

	if (count != 2) {
		return NULL;
	}

	fargs.tdma = tdma;
	fargs.chan_id = (s32)dma_spec->args[0];
	fargs.slave_id = (s32)dma_spec->args[1];

	tcc_dma_set_req(ofdma, fargs.slave_id);

	dma_cap_zero(cap);
	dma_cap_set(DMA_SLAVE, cap);

	return dma_request_channel(cap, &tcc_dma_of_filter, (void *)&fargs);
}

/**
 * Poll for transaction completion
 */
static enum dma_status tcc_dma_tx_status(struct dma_chan *chan,
					 dma_cookie_t cookie,
					 struct dma_tx_state *txstate)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	enum dma_status ret;
	u32 bytes;
	u32 current_count;
	u32 addr_width;
	u32 start_addr;
	u32 curr_addr;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return DMA_ERROR;
	}

	ret = dma_cookie_status(chan, cookie, txstate);
	if (ret == DMA_COMPLETE) {
		dev_vdbg(chan2dev(chan), "[DEBUG][GDMA] DMA complete transaction. cookie=%d\n",
					cookie);
		return ret;
	}
	dev_vdbg(chan2dev(chan), "[DEBUG][GDMA] ch=%d, cookie=%d, status=%d\n",
		 chan->chan_id, cookie, ret);

	ret = dma_cookie_status(chan, cookie, txstate);
	if (ret == DMA_COMPLETE) {
		return ret;
	}

	if (tdmac->direction == DMA_MEM_TO_DEV) {
		current_count = channel_readl(tdmac, 0x20) >> 16U;
		if (current_count != 0U) {
			addr_width = (u32)tdmac->slave_config.src_addr_width;
			curr_addr = channel_readl(tdmac, 0xC);
			start_addr = channel_readl(tdmac, 0x0);
			bytes = (curr_addr - start_addr) * addr_width;
		} else {
			bytes = 0U;
		}
	} else if (tdmac->direction == DMA_DEV_TO_MEM) {
		current_count = channel_readl(tdmac, 0x20) >> 16U;
		if (current_count != 0U) {
			addr_width = (u32)tdmac->slave_config.dst_addr_width;
			curr_addr = channel_readl(tdmac, 0x1C);
			start_addr = channel_readl(tdmac, 0x10);
			bytes = (curr_addr - start_addr) * addr_width;
		} else {
			bytes = 0U;
		}
	} else {
		return ret;
	}

	dma_set_residue(txstate, bytes);

	return ret;
}

/**
 * Prepare a memcpy operation
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_dma_memcpy(
						struct dma_chan *chan,
						dma_addr_t dest,
						dma_addr_t src,
						size_t len,
						ulong flags)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct device *dev;
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *first = NULL;
	u32 xfer_count;
	u32 offset = 0;
	u32 remain_len;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return NULL;
	}
	dev = tdmac->dev;

	dev_vdbg(chan2dev(chan),
		 "[DEBUG][GDMA] %s: chan=%d, dest=0x%08x, src=0x%08x, len=%d, flags=0x%lx\n",
		 __func__, chan->chan_id, (uint)dest, (uint)src,
		 (uint)len, flags);

	tdmac->direction = DMA_MEM_TO_MEM;
	remain_len = (u32)len;

	do {
		desc = tcc_dma_alloc_descriptor(chan);
		if (desc == NULL) {
			dev_err(dev,
				"[ERROR][GDMA] failed to allocate descriptor\n");
			goto fail;
		}
		xfer_count = min_t(u32, remain_len, TCC_DMA_MAX_XFER_CNT);

		desc->src_addr = (u32)src;
		if ((U32_MAX - offset) < desc->src_addr) {
			dev_err(dev,
				"[ERROR][GDMA] source address exceeded the maximum\n");
			goto fail;
		}
		desc->src_addr += offset;
		desc->src_inc = 1;

		desc->dst_addr = (u32)dest;
		if ((U32_MAX - offset) < desc->dst_addr) {
			dev_err(dev,
				"[ERROR][GDMA] source address exceeded the maximum\n");
			goto fail;
		}
		desc->dst_addr += offset;
		desc->dst_inc = 1;

		desc->len = xfer_count;

		if (first == NULL) {
			first = desc;
		} else {
			list_add_tail(&desc->node, &first->tx_list);
		}

		remain_len -= xfer_count;
		offset = xfer_count;
	} while (remain_len != 0UL);

	return &first->txd;

fail:
	/* TODO: free all descriptors */

	return NULL;
}

/**
 * Prepare a slave DMA operation
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_slave_sg(
					struct dma_chan *chan,
					struct scatterlist *sgl,
					uint sg_len,
					enum dma_transfer_direction direction,
					ulong flags,
					void *context)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct device *dev;
	struct dma_slave_config *config;
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *first = NULL;
	struct scatterlist *sg;
	u32 mem_addr;
	u32 dev_addr;
	u32 len;
	s32 i;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return NULL;
	}
	dev = tdmac->dev;
	config = &tdmac->slave_config;

	dev_vdbg(chan2dev(chan),
		 "[DEBUG][GDMA] %s: chan=%d, direction=%d, flags=0x%lx\n",
		 __func__, chan->chan_id, direction, flags);

	if (!is_slave_direction(direction)) {
		return NULL;
	}

	tdmac->direction = direction;

	if (direction == DMA_MEM_TO_DEV) {
		dev_addr = (u32)config->dst_addr;
		config->dst_maxburst = 0U;
		config->dst_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;

		for_each_sg(sgl, sg, sg_len, i) {
			mem_addr = (u32)sg_dma_address(sg);
			len = sg_dma_len(sg);

			desc = tcc_dma_alloc_descriptor(chan);
			if (desc == NULL) {
				dev_err(dev,
					"[ERROR][GDMA] failed to allocate descriptor\n");
				goto fail;
			}

			desc->src_addr = mem_addr;
			desc->src_inc = (u32)config->src_addr_width;
			desc->dst_addr = dev_addr;
			desc->dst_inc = (u32)config->dst_addr_width;
			desc->len = len;
			desc->slave_id = (s32)config->slave_id;
			if (config->src_maxburst == 0U) {
				dev_err(dev,
					"[ERROR][GDMA] source burst is wrong\n");
				goto fail;
			}
			desc->burst = config->src_maxburst;

			if (first == NULL) {
				first = desc;
			} else {
				list_add_tail(&desc->node, &first->tx_list);
			}
		}
	} else { /* DMA_DEV_TO_MEM */
		dev_addr = (u32)config->src_addr;
		config->src_maxburst = 0U;
		config->src_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;

		for_each_sg(sgl, sg, sg_len, i) {
			mem_addr = (u32)sg_dma_address(sg);
			len = sg_dma_len(sg);

			desc = tcc_dma_alloc_descriptor(chan);
			if (desc == NULL) {
				dev_err(dev,
					"[ERROR][GDMA] failed to allocate descriptor\n");
				goto fail;
			}

			desc->src_addr = dev_addr;
			desc->src_inc = (u32)config->src_addr_width;
			desc->dst_addr = mem_addr;
			desc->dst_inc = (u32)config->dst_addr_width;
			desc->len = len;
			desc->slave_id = (s32)config->slave_id;
			if (config->dst_maxburst == 0U) {
				dev_err(dev,
					"[ERROR][GDMA] destination burst is wrong\n");
				goto fail;
			}
			desc->burst = config->dst_maxburst;

			if (first == NULL) {
				first = desc;
			} else {
				list_add_tail(&desc->node, &first->tx_list);
			}
		}
	}

	return &first->txd;

fail:
	/* TODO: add error handling */
	return NULL;
}

/**
 * Prepare a cyclic DMA operation suitable for audio
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_dma_cyclic(
					struct dma_chan *chan,
					dma_addr_t buf_addr,
					size_t buf_len,
					size_t period_len,
					enum dma_transfer_direction direction,
					ulong flags)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return NULL;
	}

	dev_vdbg(chan2dev(chan),
		 "[DEBUG][GDMA] %s: chan=%d, direction=%d\n, flags=0x%lx",
		 __func__, chan->chan_id, direction, flags);

	tdmac->direction = direction;

	return NULL;
}

static int tcc_dma_terminate_all(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	ulong flags;
	LIST_HEAD(head);

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return -ENXIO;
	}

	spin_lock_irqsave(&tdmac->lock, flags);

	/* Disable DMA channel */
	channel_writel(tdmac, DMA_CHCTRL, 0);

	tcc_dma_get_all_descriptors(tdmac, &head);
	spin_unlock_irqrestore(&tdmac->lock, flags);
	tcc_dma_desc_free_list(tdmac, &head);
	tcc_dma_clean_completed_descriptors(tdmac);

	return 0;
}

static int tcc_dma_slave_config(struct dma_chan *chan,
				struct dma_slave_config *cfg)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	u32 slave_id;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		return -ENXIO;
	}

	dev_vdbg(chan2dev(chan),
		 "[DEBUG][GDMA] %s: ch=%d, src_addr=0x%x, dst_addr=0x%x, dir=%d\n",
		 __func__, chan->chan_id, (uint)cfg->src_addr,
		 (uint)cfg->dst_addr, cfg->direction);

	slave_id = tdmac->slave_config.slave_id;
	(void)memcpy(&tdmac->slave_config, cfg,
			sizeof(struct dma_slave_config));
	tdmac->slave_config.slave_id = slave_id;

	return 0;
}

/**
 * Push pending transactions to hardware
 */
static void tcc_dma_issue_pending(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct tcc_dma_desc *desc;

	dev_vdbg(chan2dev(chan), "[DEBUG][GDMA] %s: chan=%d\n", __func__,
		 chan->chan_id);

	list_splice_init(&tdmac->desc_submitted, &tdmac->desc_issued);

	if (list_empty(&tdmac->desc_issued) != 0) {
		return;
	}

	desc = tcc_dma_next_desc(tdmac);
	tcc_dma_dostart(tdmac, desc);
}

#ifdef CONFIG_OF
static struct tcc_dma_platform_data *tcc_dma_parse_dt(
						struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct tcc_dma_platform_data *pdata;
	u32 nr_channels = 0;
	s32 ret;

	pdata =
	    devm_kzalloc(&pdev->dev, sizeof(struct tcc_dma_platform_data),
			 GFP_KERNEL);
	if (pdata == NULL) {
		return NULL;
	}

	ret = of_property_read_u32(np, "dma-channels", &nr_channels);
	if (ret != 0) {
		return NULL;
	}
	pdata->nr_channels = (s32)nr_channels;

	return pdata;
}
#else
static struct tcc_dma_platform_data *tcc_dma_parse_dt(
						struct platform_device *pdev)
{
	return NULL;
}
#endif

static void tcc_dma_set_access_control(struct tcc_dma *tdma)
{
	if (tdma->ac_reg == NULL) {
		return;
	}

	tcc_writel(tdma->ac_reg, DMA_AC0_START, tdma->ac_val[0][0]);
	tcc_writel(tdma->ac_reg, DMA_AC0_LIMIT, tdma->ac_val[0][1]);
	tcc_writel(tdma->ac_reg, DMA_AC1_START, tdma->ac_val[1][0]);
	tcc_writel(tdma->ac_reg, DMA_AC1_LIMIT, tdma->ac_val[1][1]);
	tcc_writel(tdma->ac_reg, DMA_AC2_START, tdma->ac_val[2][0]);
	tcc_writel(tdma->ac_reg, DMA_AC2_LIMIT, tdma->ac_val[2][1]);
	tcc_writel(tdma->ac_reg, DMA_AC3_START, tdma->ac_val[3][0]);
	tcc_writel(tdma->ac_reg, DMA_AC3_LIMIT, tdma->ac_val[3][1]);
}

static int __init tcc_dma_probe(struct platform_device *pdev)
{
	struct tcc_dma_platform_data *pdata;
	struct tcc_dma *tdma;
	struct resource *res;
	void __iomem *regs;
	s32 irq;
	s32 ret;
	s32 i;
	struct device_node *ac_np;
	u32 ac_val[2] = { 0 };

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs)) {
		dev_err(&pdev->dev, "[ERROR][GDMA] failed ioremap, err: %ld\n",
				PTR_ERR(regs));
		return -ENXIO;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		return irq;
	}

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][GDMA] Unable to set DMA mask\n");
		return ret;
	}

	pdata = dev_get_platdata(&pdev->dev);
	if (pdata == NULL) {
		pdata = tcc_dma_parse_dt(pdev);
	}

	if ((pdata == NULL) || (pdata->nr_channels > TCC_MAX_DMA_CHANNELS)) {
		return -EINVAL;
	}

	tdma = devm_kzalloc(&pdev->dev, sizeof(struct tcc_dma), GFP_KERNEL);
	if (tdma == NULL) {
		dev_err(&pdev->dev, "[ERROR][GDMA] failed to alloc tdma\n");
		return -ENOMEM;
	}

	tdma->chan = devm_kcalloc(&pdev->dev, (size_t)pdata->nr_channels,
			sizeof(struct tcc_dma_chan), GFP_KERNEL);
	if (tdma->chan == NULL) {
		dev_err(&pdev->dev, "[ERROR][GDMA] failed to alloc chan\n");
		return -ENOMEM;
	}

	tdma->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(tdma->clk)) {
		dev_err(&pdev->dev,
			"[ERROR][GDMA] failed to get clk info\n");
		return -ENXIO;
	}
	ret = clk_prepare_enable(tdma->clk);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][GDMA] failed to enable clk\n");
		return ret;
	}

	ret = devm_request_irq(&(pdev->dev), (uint)irq, &tcc_dma_interrupt,
			       IRQF_SHARED, "tcc_dma", (void *)tdma);
	if (ret != 0) {
		return ret;
	}

	platform_set_drvdata(pdev, (void *)tdma);

	if (pdev->dev.of_node == NULL) {
		dev_err(&pdev->dev, "[ERROR][GDMA] No node\n");
		return -EINVAL;
	}

	tdma->regs = regs;
	tdma->req_reg = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tdma->req_reg)) {
		dev_err(&pdev->dev, "[ERROR][GDMA] failed req_reg ioremap, err: %ld\n",
				PTR_ERR(tdma->req_reg));
		return -ENXIO;
	}

	/* Get GDMA Access Control base address */
	ac_np = of_parse_phandle(pdev->dev.of_node, "access-control", 0);
	if (ac_np != NULL) {
		tdma->ac_reg = of_iomap(ac_np, 0);
		if (IS_ERR(tdma->ac_reg)) {
			dev_err(&pdev->dev, "[ERROR][GDMA] failed ac_reg ioremap, err: %ld\n",
					PTR_ERR(tdma->ac_reg));
			return -ENXIO;
		}

		ret = of_property_read_u32_array(
				ac_np, "access-control0", ac_val, 2);
		if (ret == 0) {
			dev_vdbg(&pdev->dev,
				 "[DEBUG][GDMA] access-control0 start:0x%08x limit:0x%08x\n",
				 ac_val[0], ac_val[1]);
			tdma->ac_val[0][0] = ac_val[0];
			tdma->ac_val[0][1] = ac_val[1];
		}
		ret = of_property_read_u32_array(
				ac_np, "access-control1", ac_val, 2);
		if (ret == 0) {
			dev_vdbg(&pdev->dev,
				 "[DEBUG][GDMA] access-control1 start:0x%08x limit:0x%08x\n",
				 ac_val[0], ac_val[1]);
			tdma->ac_val[1][0] = ac_val[0];
			tdma->ac_val[1][1] = ac_val[1];
		}
		ret = of_property_read_u32_array(
				ac_np, "access-control2", ac_val, 2);
		if (ret == 0) {
			dev_vdbg(&pdev->dev,
				 "[DEBUG][GDMA] access-control2 start:0x%08x limit:0x%08x\n",
				 ac_val[0], ac_val[1]);
			tdma->ac_val[2][0] = ac_val[0];
			tdma->ac_val[2][1] = ac_val[1];
		}
		ret = of_property_read_u32_array(
				ac_np, "access-control3", ac_val, 2);
		if (ret == 0) {
			dev_vdbg(&pdev->dev,
				 "[DEBUG][GDMA] access-control3 start:0x%08x limit:0x%08x\n",
				 ac_val[0], ac_val[1]);
			tdma->ac_val[3][0] = ac_val[0];
			tdma->ac_val[3][1] = ac_val[1];
		}
	} else {
		tdma->ac_reg = NULL;
	}
	tcc_dma_set_access_control(tdma);

	INIT_LIST_HEAD(&tdma->dma.channels);
	for (i = 0; i < pdata->nr_channels; i++) {
		struct tcc_dma_chan *tdmac = &tdma->chan[i];

		if (tdmac == NULL) {
			dev_err(&pdev->dev,
				"[ERROR][GDMA] ch%d, tdmac is NULL\n", i);
			break;
		}

		tasklet_init(&tdmac->tasklet, &tcc_dma_tasklet, (ulong)tdmac);

		tdmac->chan.device = &tdma->dma;
		tdmac->dev = &pdev->dev;
		tdmac->direction = DMA_TRANS_NONE;

		dma_cookie_init(&tdmac->chan);

		INIT_LIST_HEAD(&tdmac->desc_submitted);
		INIT_LIST_HEAD(&tdmac->desc_issued);
		INIT_LIST_HEAD(&tdmac->desc_completed);

		spin_lock_init(&tdmac->lock);

		list_add_tail(&tdmac->chan.device_node, &tdma->dma.channels);
	}

	dma_cap_set(DMA_MEMCPY, tdma->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, tdma->dma.cap_mask);
	dma_cap_set(DMA_CYCLIC, tdma->dma.cap_mask);

	tdma->dma.dev = &pdev->dev;

	tdma->dma.device_alloc_chan_resources = &tcc_dma_alloc_chan_resources;
	tdma->dma.device_free_chan_resources = &tcc_dma_free_chan_resources;
	tdma->dma.device_tx_status = &tcc_dma_tx_status;
	tdma->dma.device_prep_dma_memcpy = &tcc_dma_prep_dma_memcpy;
	tdma->dma.device_prep_slave_sg = &tcc_dma_prep_slave_sg;
	tdma->dma.device_prep_dma_cyclic = &tcc_dma_prep_dma_cyclic;
	tdma->dma.device_issue_pending = &tcc_dma_issue_pending;
	tdma->dma.device_config = &tcc_dma_slave_config;
	tdma->dma.device_terminate_all = &tcc_dma_terminate_all;

	ret = dma_async_device_register(&tdma->dma);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][GDMA] failed to register DMA engine device\n");
		return ret;
	}

	ret = of_dma_controller_register(
			pdev->dev.of_node, &tcc_dma_of_xlate, (void *)tdma);
	if (ret != 0) {
		dev_err(&pdev->dev,
				"[ERROR][GDMA] failed to register of_dma_controller\n");
		dma_async_device_unregister(&tdma->dma);
	}

	dev_info(&pdev->dev,
		 "[INFO][GDMA] Telechips DMA controller initialized (irq=%d)\n",
		 irq);

	return 0;
}

static int tcc_dma_remove(struct platform_device *pdev)
{
	struct device_node *np;
	struct tcc_dma *tdma = platform_get_drvdata(pdev);

	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL\n", __func__);
		return -ENXIO;
	}

	np = dev_of_node(&pdev->dev);
	if (np != NULL) {
		of_dma_controller_free(np);
	}
	dma_async_device_unregister(&tdma->dma);
	clk_disable_unprepare(tdma->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_dma_suspend(struct device *dev)
{
	struct tcc_dma *tdma = dev_get_drvdata(dev);

	clk_disable_unprepare(tdma->clk);

	return 0;
}

static int tcc_dma_resume(struct device *dev)
{
	struct tcc_dma *tdma = dev_get_drvdata(dev);
	s32 ret;

	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL\n", __func__);
		return -ENXIO;
	}

	ret = clk_prepare_enable(tdma->clk);
	if (ret != 0) {
		dev_err(dev, "[ERROR][DMA] Failed to enable DMA HCLK\n");
		return ret;
	}

	tcc_dma_set_access_control(tdma);

	return 0;
}

static const struct dev_pm_ops tcc_dma_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_dma_suspend, tcc_dma_resume)
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_dma_of_id_table[6] = {
	{.compatible = "telechips,tcc897x-dma",},
	{.compatible = "telechips,tcc899x-dma",},
	{.compatible = "telechips,tcc803x-dma",},
	{.compatible = "telechips,tcc805x-dma",},
	{.compatible = "telechips,tcc901x-dma",},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_dma_of_id_table);
#endif

static struct platform_driver tcc_dma_driver = {
	.driver = {
		   .name = "tcc-dma",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		   .pm = &tcc_dma_pm_ops,
#endif
		   .of_match_table = of_match_ptr(tcc_dma_of_id_table),
		   },
	.probe = tcc_dma_probe,
	.remove = tcc_dma_remove,
};

static int tcc_dma_init(void)
{
	return platform_driver_register(&tcc_dma_driver);
}

subsys_initcall(tcc_dma_init);

static void __exit tcc_dma_exit(void)
{
	platform_driver_unregister(&tcc_dma_driver);
}

module_exit(tcc_dma_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips DMA driver");
MODULE_LICENSE("GPL");
