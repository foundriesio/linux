/*
 * DMA engine driver for Telechips DMA controller
 *
 * Copyright (C) 2015 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#include "dmaengine.h"

#define TCC_MAX_DMA_CHANNELS	5
#define TCC_DMA_MAX_XFER_CNT	0xffff

#define DMA_CHANNEL_REG_OFFSET	0x30

#define DMA_ST_SADR	0x0	/* Start Source Address register */
#define DMA_SPARAM	0x4	/* Source Block Parameter register */
#define DMA_C_SADR	0xc	/* Current Source Address register */
#define DMA_ST_DADR	0x10	/* Start Destination Address register */
#define DMA_DPARAM	0x14	/* Destination Block Parameter register */
#define DMA_HCOUNT	0x20	/* HOP Count register */
#define DMA_CHCTRL	0x24	/* Channel Control register */
#define DMA_RPTCTRL	0x28	/* Repeat Control register */
#define DMA_EXTREQ	0x2c	/* External DMA Request register */

#define DMA_CHCONFIG	0x90	/* Channel Configuration register */

#define DMA_AC0_START	0x00
#define DMA_AC0_LIMIT	0x04
#define DMA_AC1_START	0x08
#define DMA_AC1_LIMIT	0x0C
#define DMA_AC2_START	0x10
#define DMA_AC2_LIMIT	0x14
#define DMA_AC3_START	0x18
#define DMA_AC3_LIMIT	0x1C

#define DMA_SPARAM_SMASK(x)	(((x) & 0xffffff) << 8)
#define DMA_SPARAM_SINC(x)	(((x) & 0xff) << 0)

#define DMA_DPARAM_DMASK(x)	(((x) & 0xffffff) << 8)
#define DMA_DPARAM_DINC(x)	(((x) & 0xff) << 0)

#define DMA_HCOUNT_ST_HCOUNT(x)	(((x) & 0xffff) << 0)

#define DMA_CHCTRL_EN		(1 << 0)	/* DMA channel enable */
#define DMA_CHCTRL_REP		(1 << 1)	/* repeat mode */
#define DMA_CHCTRL_IEN		(1 << 2)	/* interrupt enable */
#define DMA_CHCTRL_FLAG		(1 << 3)	/* DMA done flag */
#define DMA_CHCTRL_WSIZE(x)	((x) << 4)	/* word size */
#define DMA_CHCTRL_BSIZE(x)	((x) << 6)	/* burst size */
#define DMA_CHCTRL_TYPE(x)	((x) << 8)	/* transfer type */
#define DMA_CHCTRL_BST		(1 << 10)	/* burst transfer */
#define DMA_CHCTRL_LOCK		(1 << 11)	/* locked transfer */
#define DMA_CHCTRL_HRD		(1 << 12)	/* hardware request direction */
#define DMA_CHCTRL_SYNC		(1 << 13)	/* hardware request sync */
#define DMA_CHCTRL_DTM		(1 << 14)	/* differential transfer mode */
#define DMA_CHCTRL_CONT		(1 << 15)	/* continuous transfer */

#define DMA_CHCONFIG_IS(x, n)	((x >> (20 + (n))) & 1)

struct tcc_dma_desc {
	struct list_head node;
	struct list_head tx_list;
	struct dma_async_tx_descriptor txd;

	u32 src_addr;
	u32 src_inc;
	u32 dst_addr;
	u32 dst_inc;
	u32 len;
	int slave_id;
};

struct tcc_dma_chan {
	struct dma_chan		chan;
	struct dma_pool		*desc_pool;
	struct device		*dev;
	struct tasklet_struct	tasklet;

	struct dma_slave_config	slave_config;

	struct list_head	desc_submitted;
	struct list_head	desc_issued;
	struct list_head	desc_completed;

	spinlock_t		lock;

	enum dma_transfer_direction direction;
};

struct tcc_dma {
	struct dma_device	dma;
	void __iomem		*regs;
	void __iomem		*req_reg;
	void __iomem		*ac_reg;
	struct clk		*clk;

	struct tcc_dma_chan	chan[0];
};

struct tcc_dma_of_filter_args {
	struct tcc_dma *tdma;
	int chan_id;
	int slave_id;
};

static inline struct tcc_dma_desc *to_tcc_desc(struct list_head *node)
{
	return list_entry(node, struct tcc_dma_desc, node);
}

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

static inline u32 channel_readl(struct tcc_dma_chan *tdmac, u32 off)
{
	struct dma_chan *chan = &tdmac->chan;
	struct tcc_dma *tdma = to_tcc_dma(chan->device);
	int chan_id = chan->chan_id;
	void __iomem *addr;
	u32 val;

	addr = tdma->regs + DMA_CHANNEL_REG_OFFSET*chan_id + off;
	val = readl(addr);
	dev_vdbg(chan2dev(chan), "%s: ch=%d: addr=%p, val=%08x\n", __func__,
		 chan_id, addr, val);
	return val;
}

static inline void channel_writel(struct tcc_dma_chan *tdmac, u32 off, u32 val)
{
	struct dma_chan *chan = &tdmac->chan;
	struct tcc_dma *tdma = to_tcc_dma(chan->device);
	int chan_id = chan->chan_id;
	void __iomem *addr;

	addr = tdma->regs + DMA_CHANNEL_REG_OFFSET*chan_id + off;
	dev_vdbg(chan2dev(chan), "%s: ch=%d: addr=%p, val=%08x\n", __func__,
		 chan_id, addr, val);
	writel(val, addr);
}

/**
 * Peak at the next descriptor to be processed.
 */
static struct tcc_dma_desc *tcc_dma_next_desc(struct tcc_dma_chan *tdmac)
{
	if (list_empty(&tdmac->desc_issued))
		return NULL;

	return list_first_entry(&tdmac->desc_issued, struct tcc_dma_desc, node);
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
	struct tcc_dma_desc *desc, *_desc;

	list_for_each_entry_safe(desc, _desc, head, node) {
		dev_vdbg(tdmac->dev, "freeing descriptor %p\n", desc);
		list_del(&desc->node);
		dma_pool_free(tdmac->desc_pool, desc, desc->txd.phys);
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
	tcc_dma_desc_free_list(tdmac,&head);
}

static int tcc_dma_width(enum dma_slave_buswidth width)
{
	switch(width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
		return DMA_CHCTRL_WSIZE(0);
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
		return DMA_CHCTRL_WSIZE(1);
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
		return DMA_CHCTRL_WSIZE(2);
	default:
		return -1;
	}
}

static int tcc_dma_burst(int burst)
{
	switch(burst) {
	case 1:
		return DMA_CHCTRL_BSIZE(0);
	case 2:
		return DMA_CHCTRL_BSIZE(1);
	case 4:
		return DMA_CHCTRL_BSIZE(2);
	case 8:
		return DMA_CHCTRL_BSIZE(3);
	default:
		return -1;
	}
}

static void tcc_dma_do_single_block(struct tcc_dma_chan *tdmac,
				    struct tcc_dma_desc *desc)
{
	u32 chctrl;
	int width = -1;
	int burst = -1;

	dev_vdbg(chan2dev(&tdmac->chan),
		 "%s: ch%d: src=0x%08x, dest=0x%08x, len=%d\n",
		 __func__, tdmac->chan.chan_id, desc->src_addr,
		 desc->dst_addr, desc->len);

	chctrl = DMA_CHCTRL_IEN | DMA_CHCTRL_WSIZE(0) | DMA_CHCTRL_BSIZE(0)
			| DMA_CHCTRL_FLAG | DMA_CHCTRL_TYPE(2) | DMA_CHCTRL_EN;

	channel_writel(tdmac, DMA_ST_SADR, desc->src_addr);
	channel_writel(tdmac, DMA_SPARAM, DMA_SPARAM_SINC(desc->src_inc));
	channel_writel(tdmac, DMA_ST_DADR, desc->dst_addr);
	channel_writel(tdmac, DMA_DPARAM, DMA_DPARAM_DINC(desc->dst_inc));
	channel_writel(tdmac, DMA_HCOUNT, DMA_HCOUNT_ST_HCOUNT(desc->len));
	if (is_slave_direction(tdmac->direction)) {
		unsigned int slave_id;

		slave_id = desc->slave_id;
		if(slave_id > 31)
			slave_id = slave_id - 32;

		chctrl |= DMA_CHCTRL_SYNC | DMA_CHCTRL_TYPE(3);
		channel_writel(tdmac, DMA_EXTREQ, 1 << slave_id);
	}

	if(tdmac->slave_config.src_addr_width != 0) {
		width = tcc_dma_width(tdmac->slave_config.src_addr_width);
		burst = tcc_dma_burst(tdmac->slave_config.src_maxburst);
	}
	else if(tdmac->slave_config.dst_addr_width != 0) {
		width = tcc_dma_width(tdmac->slave_config.dst_addr_width);
		burst = tcc_dma_burst(tdmac->slave_config.dst_maxburst);
	}

	// Set word size
	if(width != -1) {
		chctrl &= ~(DMA_CHCTRL_WSIZE(0x3));
		chctrl |= width;
	}
	// Set burst size
	if(burst != -1) {
		chctrl &= ~(DMA_CHCTRL_BSIZE(0x3));
		chctrl |= burst;
	}

	channel_writel(tdmac, DMA_CHCTRL, chctrl);
}

static void tcc_dma_tasklet(unsigned long data)
{
	struct tcc_dma_chan *tdmac = (struct tcc_dma_chan *) data;
	struct tcc_dma_desc *desc;
	unsigned long flags;

	spin_lock_irqsave(&tdmac->lock, flags);

	desc = tcc_dma_next_desc(tdmac);
	dev_vdbg(tdmac->dev,
		 "%s: chan=%d, cookie=%d: src=0x%08x, dest=0x%08x, len=%d\n",
		 __func__, tdmac->chan.chan_id, desc->txd.cookie,
		 desc->src_addr, desc->dst_addr, desc->len);

	list_move_tail(&desc->node, &tdmac->desc_completed);

	if (tcc_dma_next_desc(tdmac)) {
		tcc_dma_do_single_block(tdmac, tcc_dma_next_desc(tdmac));
		spin_unlock_irqrestore(&tdmac->lock, flags);
		return;
	}

	spin_unlock_irqrestore(&tdmac->lock, flags);

	desc = list_first_entry(&tdmac->desc_completed, struct tcc_dma_desc, node);
	dma_cookie_complete(&desc->txd);

	if (desc->txd.callback)
		desc->txd.callback(desc->txd.callback_param);

	tcc_dma_clean_completed_descriptors(tdmac);
#if 0
	INIT_LIST_HEAD(&tdmac->desc_submitted);
	INIT_LIST_HEAD(&tdmac->desc_issued);
#endif
	INIT_LIST_HEAD(&tdmac->desc_completed);
}

static irqreturn_t tcc_dma_interrupt(int irq, void *data)
{
	struct tcc_dma *tdma = (struct tcc_dma *) data;
	int i;

	for (i = 0; i < tdma->dma.chancnt; i++) {
		struct tcc_dma_chan *tdmac = &tdma->chan[i];
		int chan_id = tdmac->chan.chan_id;
		u32 chconfig;

		chconfig = readl(tdma->regs + DMA_CHCONFIG);
		if (DMA_CHCONFIG_IS(chconfig, chan_id)) {
			channel_writel(tdmac, DMA_CHCTRL,DMA_CHCTRL_FLAG);
			tasklet_schedule(&tdmac->tasklet);
		}
	}

	return IRQ_HANDLED;
}

static void tcc_dma_dostart(struct tcc_dma_chan *tdmac,
			    struct tcc_dma_desc *first)
{
	tcc_dma_do_single_block(tdmac, first);
}

static dma_cookie_t tcc_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct tcc_dma_desc *desc = to_tcc_dma_desc(tx);
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(tx->chan);
	dma_cookie_t cookie;
	unsigned long flags;

	spin_lock_irqsave(&tdmac->lock, flags);
	cookie = dma_cookie_assign(tx);
	list_add_tail(&desc->node, &tdmac->desc_submitted);
	if(desc->tx_list.next != &desc->tx_list) {
		list_splice_tail(&desc->tx_list, &tdmac->desc_submitted);
	}
	spin_unlock_irqrestore(&tdmac->lock, flags);

	dev_vdbg(chan2dev(tx->chan), "%s: ch=%d: cookie=%d, tx=%p", __func__,
		 tx->chan->chan_id, cookie, tx);

	return cookie;
}

static struct tcc_dma_desc *tcc_dma_alloc_descriptor(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct tcc_dma_desc *desc;
	dma_addr_t phys;

	desc = dma_pool_alloc(tdmac->desc_pool, GFP_ATOMIC, &phys);
	if (!desc) {
		dev_err(tdmac->dev, "failed to allocate descriptor pool\n");
		return ERR_PTR(-ENOMEM);
	}
	memset(desc, 0, sizeof(struct tcc_dma_desc));

	INIT_LIST_HEAD(&desc->tx_list);
	dma_async_tx_descriptor_init(&desc->txd, &tdmac->chan);
	desc->txd.tx_submit = tcc_dma_tx_submit;
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

	dma_cookie_init(chan);

	tdmac->desc_pool = dmam_pool_create(dev_name(chan2dev(chan)),
			tdmac->dev, sizeof(struct tcc_dma_desc),
			__alignof(struct tcc_dma_desc), 0);
	if (!tdmac->desc_pool) {
		dev_err(tdmac->dev, "failed to create descriptor pool\n");
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
	unsigned long flags;
	LIST_HEAD(head);

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

	if (chan->device != &fargs->tdma->dma)
		return false;

	chan->chan_id = fargs->chan_id;
	tdmac->slave_config.slave_id = fargs->slave_id;

	return true;
}

static void tcc_dma_set_req(struct of_dma *ofdma, int num)
{
	struct tcc_dma *tdma = ofdma->of_dma_data;
	unsigned int req;
	int sel;
	int sw = 0;

	if(!tdma->req_reg)
		return;

	if(num > 31) {
		sel = num - 32;
		sw = 1;
	} else {
		sel = num;
	}

	if(sel > 31) {
		dev_err(tdma->dma.dev, "%s wrong slave id %d\n",
			__func__, num);
		return;
	}

	req = readl(tdma->req_reg);
	req &= ~(1<<sel);
	if(sw == 1) {
		req |= (1<<sel);
	}
	writel(req, tdma->req_reg);
}

static struct dma_chan *tcc_dma_of_xlate(struct of_phandle_args *dma_spec,
					 struct of_dma *ofdma)
{
	struct tcc_dma *tdma = ofdma->of_dma_data;
	struct tcc_dma_of_filter_args fargs;
	struct device *dev = tdma->dma.dev;
	int count = dma_spec->args_count;
	dma_cap_mask_t cap;

	dev_vdbg(dev, "%s: args_count=%d, args[0]=%d\n", __func__, count,
		 dma_spec->args[0]);

	if (count != 2)
		return NULL;

	fargs.tdma = tdma;
	fargs.chan_id = dma_spec->args[0];
	fargs.slave_id = dma_spec->args[1];

	tcc_dma_set_req(ofdma, fargs.slave_id);

	dma_cap_zero(cap);
	dma_cap_set(DMA_SLAVE, cap);

	return dma_request_channel(cap, tcc_dma_of_filter, &fargs);
}

/**
 * Poll for transaction completion
 */
static enum dma_status tcc_dma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	enum dma_status ret;
	unsigned int bytes;
	unsigned int current_count;

	ret = dma_cookie_status(chan, cookie, txstate);
	if(ret == DMA_COMPLETE) {
		dev_vdbg(chan2dev(chan), "%s, DMA complete transaction.\n", dma_chan_name(chan));
		return ret;
	}
	dev_vdbg(chan2dev(chan), "ch=%d, cookie=%d, status=%d\n",
		 chan->chan_id, cookie, ret);

	ret = dma_cookie_status(chan, cookie, txstate);
	if(ret != DMA_COMPLETE) {

		if(tdmac->direction == DMA_MEM_TO_DEV) {
			current_count = channel_readl(tdmac, 0x20) >> 16;
			if(current_count != 0) {
				bytes = (channel_readl(tdmac, 0xc) - channel_readl(tdmac, 0x0)) * tdmac->slave_config.src_addr_width;
				dma_set_residue(txstate, bytes);
			}
			else
				dma_set_residue(txstate, 0);

		}
		else if(tdmac->direction == DMA_DEV_TO_MEM) {
			current_count = channel_readl(tdmac, 0x20) >> 16;
			if(current_count != 0) {
				bytes = (channel_readl(tdmac, 0x1c) - channel_readl(tdmac, 0x10)) * tdmac->slave_config.dst_addr_width;
				dma_set_residue(txstate, bytes);
			}
			else
				dma_set_residue(txstate, 0);
		}
	}
	return ret;
}

/**
 * Prepare a memcpy operation
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_dma_memcpy(
		struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
		size_t len, unsigned long flags)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct device *dev = tdmac->dev;
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *first = NULL;
	size_t xfer_count;
	size_t offset = 0;

	dev_vdbg(chan2dev(chan),
		 "%s: chan=%d, dest=0x%08x, src=0x%08x, len=%d, flags=0x%lx\n",
		 __func__, chan->chan_id, (unsigned int)dest, (unsigned int)src, (unsigned int)len, flags);

	tdmac->direction = DMA_MEM_TO_MEM;

	do {
		desc = tcc_dma_alloc_descriptor(chan);
		if (!desc) {
			dev_err(dev, "failed to allocate descriptor\n");
			goto fail;
		}
		xfer_count = min(len, (size_t) TCC_DMA_MAX_XFER_CNT);

		desc->src_addr = src + offset;
		desc->src_inc = 1;
		desc->dst_addr = dest + offset;
		desc->dst_inc = 1;
		desc->len = xfer_count;
		desc->slave_id = 0;

		if (!first)
			first = desc;
		else
			list_add_tail(&desc->node, &first->tx_list);

		len -= xfer_count;
		offset = xfer_count;
	} while (len);

	return &first->txd;

fail:
	if (first) {
		/* TODO: free all descriptors */
	}
	return NULL;
}

/**
 * Prepare a slave DMA operation
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct device *dev = tdmac->dev;
	struct dma_slave_config *config = &tdmac->slave_config;
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *first = NULL;
	struct scatterlist *sg;
	u32 mem_addr, dev_addr, len;
	int i;

	dev_vdbg(chan2dev(chan), "%s: chan=%d, direction=%d, flags=0x%lx\n",
		 __func__, chan->chan_id, direction, flags);

	if (unlikely(!is_slave_direction(direction)))
		return NULL;

	tdmac->direction = direction;

	switch (direction) {
	case DMA_MEM_TO_DEV:
		dev_addr = config->dst_addr;
		config->dst_maxburst = 0;
		config->dst_addr_width = 0;

		for_each_sg(sgl, sg, sg_len, i) {
			mem_addr = sg_dma_address(sg);
			len = sg_dma_len(sg);

			desc = tcc_dma_alloc_descriptor(chan);
			if (!desc) {
				dev_err(dev, "failed to allocate descriptor\n");
				goto fail;
			}

			desc->src_addr = mem_addr;
			desc->src_inc = config->src_addr_width;
			desc->dst_addr = dev_addr;
			desc->dst_inc = config->dst_addr_width;
			desc->len = len;
			desc->slave_id = config->slave_id;

			if (!first)
				first = desc;
			else
				list_add_tail(&desc->node, &first->tx_list);
		}
		break;

	case DMA_DEV_TO_MEM:
		dev_addr = config->src_addr;
		config->src_maxburst = 0;
		config->src_addr_width = 0;

		for_each_sg(sgl, sg, sg_len, i) {
			mem_addr = sg_dma_address(sg);
			len = sg_dma_len(sg);

			desc = tcc_dma_alloc_descriptor(chan);
			if (!desc) {
				dev_err(dev, "failed to allocate descriptor\n");
				goto fail;
			}

			desc->src_addr = dev_addr;
			desc->src_inc = config->src_addr_width;
			desc->dst_addr = mem_addr;
			desc->dst_inc = config->dst_addr_width;
			desc->len = len;
			desc->slave_id = config->slave_id;

			if (!first)
				first = desc;
			else
				list_add_tail(&desc->node, &first->tx_list);
		}
		break;

	default:
		return NULL;
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
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);

	dev_vdbg(chan2dev(chan), "%s: chan=%d, direction=%d\n, flags=0x%lx",
		 __func__, chan->chan_id, direction, flags);

	tdmac->direction = direction;

	return NULL;
}

static int tcc_dma_terminate_all(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
       unsigned long flags;
       LIST_HEAD(head);

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
	unsigned int slave_id;

	dev_vdbg(chan2dev(chan),
		 "%s: ch=%d, src_addr=0x%x, dst_addr=0x%x, dir=%d\n",
		 __func__, chan->chan_id, (unsigned int)cfg->src_addr, (unsigned int)cfg->dst_addr,
		 cfg->direction);

	slave_id = tdmac->slave_config.slave_id;
	memcpy(&tdmac->slave_config, cfg, sizeof(struct dma_slave_config));
	tdmac->slave_config.slave_id = slave_id;

	//re-setting current count address
	channel_writel(tdmac, DMA_HCOUNT, 0);

	channel_writel(tdmac, DMA_CHCTRL, 0x201);
	mb();
	channel_writel(tdmac, DMA_CHCTRL, DMA_CHCTRL_FLAG);
	mb();
	channel_writel(tdmac, DMA_CHCTRL, 0x0);

	return 0;
}

/**
 * Push pending transactions to hardware
 */
static void tcc_dma_issue_pending(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct tcc_dma_desc *desc;

	dev_vdbg(chan2dev(chan), "%s: chan=%d\n", __func__, chan->chan_id);

	list_splice_init(&tdmac->desc_submitted, &tdmac->desc_issued);

	if (list_empty(&tdmac->desc_issued))
		return;

	desc = tcc_dma_next_desc(tdmac);
	tcc_dma_dostart(tdmac, desc);
}

#ifdef CONFIG_OF
static struct tcc_dma_platform_data *tcc_dma_parse_dt(
		struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct tcc_dma_platform_data *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct tcc_dma_platform_data), GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_u32(np, "dma-channels", &pdata->nr_channels))
		return NULL;

	return pdata;
}
#else
static struct tcc_dma_platform_data *tcc_dma_parse_dt(
		struct platform_device *pdev)
{
	return NULL;
}
#endif

static int __init tcc_dma_probe(struct platform_device *pdev)
{
	struct tcc_dma_platform_data *pdata;
	struct tcc_dma *tdma;
	struct resource *res;
	void __iomem *regs;
	int irq;
	int ret;
	int i;
	struct device_node *ac_np;
	unsigned int ac_val[2] = {0};

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(regs)) {
		return -ENXIO;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	if (!pdev->dev.dma_mask) {
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	}

	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata)
		pdata = tcc_dma_parse_dt(pdev);

	if (!pdata || pdata->nr_channels > TCC_MAX_DMA_CHANNELS)
		return -EINVAL;

	tdma = devm_kzalloc(&pdev->dev, sizeof(struct tcc_dma) +
			pdata->nr_channels * sizeof(struct tcc_dma_chan),
			GFP_KERNEL);
	if (!tdma)
		return -ENOMEM;

	tdma->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR_OR_NULL(tdma->clk)) {
		return -ENXIO;
	}
	clk_prepare_enable(tdma->clk);

	ret = devm_request_irq(&pdev->dev, irq, tcc_dma_interrupt,
			       IRQF_SHARED, "tcc_dma", tdma);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, tdma);

	tdma->regs = regs;
	tdma->req_reg = of_iomap(pdev->dev.of_node, 1);
	if (IS_ERR(tdma->req_reg)) {
		return PTR_ERR(tdma->req_reg);
	}

	/* Get GDMA Access Control base address */
	ac_np = of_parse_phandle(pdev->dev.of_node, "access-control", 0);
	if (ac_np != NULL) {
		tdma->ac_reg = of_iomap(ac_np, 0);
		if (IS_ERR(tdma->ac_reg)) {
			return PTR_ERR(tdma->ac_reg);
		}
		if (!of_property_read_u32_array(ac_np, "access-control0", ac_val, 2)) {
			dev_vdbg(&pdev->dev, "access-control0 start:0x%08x limit:0x%08x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tdma->ac_reg + DMA_AC0_START);
			writel(ac_val[1], tdma->ac_reg + DMA_AC0_LIMIT);
		}
		if (!of_property_read_u32_array(ac_np, "access-control1", ac_val, 2)) {
			dev_vdbg(&pdev->dev, "access-control1 start:0x%08x limit:0x%08x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tdma->ac_reg + DMA_AC1_START);
			writel(ac_val[1], tdma->ac_reg + DMA_AC1_LIMIT);
		}
		if (!of_property_read_u32_array(ac_np, "access-control2", ac_val, 2)) {
			dev_vdbg(&pdev->dev, "access-control2 start:0x%08x limit:0x%08x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tdma->ac_reg + DMA_AC2_START);
			writel(ac_val[1], tdma->ac_reg + DMA_AC2_LIMIT);
		}
		if (!of_property_read_u32_array(ac_np, "access-control3", ac_val, 2)) {
			dev_vdbg(&pdev->dev, "access-control3 start:0x%08x limit:0x%08x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tdma->ac_reg + DMA_AC3_START);
			writel(ac_val[1], tdma->ac_reg + DMA_AC3_LIMIT);
		}
	}

	INIT_LIST_HEAD(&tdma->dma.channels);
	for (i = 0; i < pdata->nr_channels; i++) {
		struct tcc_dma_chan *tdmac = &tdma->chan[i];

		tasklet_init(&tdmac->tasklet, tcc_dma_tasklet,
			     (unsigned long) tdmac);

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

	tdma->dma.device_alloc_chan_resources = tcc_dma_alloc_chan_resources;
	tdma->dma.device_free_chan_resources = tcc_dma_free_chan_resources;
	tdma->dma.device_tx_status = tcc_dma_tx_status;
	tdma->dma.device_prep_dma_memcpy = tcc_dma_prep_dma_memcpy;
	tdma->dma.device_prep_slave_sg = tcc_dma_prep_slave_sg;
	tdma->dma.device_prep_dma_cyclic = tcc_dma_prep_dma_cyclic;
	tdma->dma.device_issue_pending = tcc_dma_issue_pending;
	tdma->dma.device_config = tcc_dma_slave_config;
	tdma->dma.device_terminate_all = tcc_dma_terminate_all;

	ret = dma_async_device_register(&tdma->dma);
	if (ret) {
		dev_err(&pdev->dev, "failed to register DMA engine device\n");
		return ret;
	}

	if (pdev->dev.of_node) {
		ret = of_dma_controller_register(pdev->dev.of_node,
				tcc_dma_of_xlate, tdma);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to register of_dma_controller\n");
			dma_async_device_unregister(&tdma->dma);
		}
	}

	dev_info(&pdev->dev, "Telechips DMA controller initialized (irq=%d)\n",
		       irq);

	return 0;
}

static int tcc_dma_remove(struct platform_device *pdev)
{
	struct tcc_dma *tdma = platform_get_drvdata(pdev);

	if (pdev->dev.of_node)
		of_dma_controller_free(pdev->dev.of_node);

	dma_async_device_unregister(&tdma->dma);

	clk_disable_unprepare(tdma->clk);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_dma_of_id_table[] = {
	{ .compatible = "telechips,tcc896x-dma", },
	{ .compatible = "telechips,tcc897x-dma", },
	{ .compatible = "telechips,tcc898x-dma", },
	{ .compatible = "telechips,tcc899x-dma", },
	{ .compatible = "telechips,tcc802x-dma", },
	{ .compatible = "telechips,tcc803x-dma", },
	{ .compatible = "telechips,tcc901x-dma", },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_dma_of_id_table);
#endif

static struct platform_driver tcc_dma_driver = {
	.driver	= {
		.name	= "tcc-dma",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_dma_of_id_table),
	},
	.probe		= tcc_dma_probe,
	.remove		= tcc_dma_remove,
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

MODULE_AUTHOR("Albert Kim <kimys@telechips.com>");
MODULE_DESCRIPTION("Telechips DMA driver");
MODULE_LICENSE("GPL");
