/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
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
#include <linux/io.h>

#include <linux/timekeeping.h>
#include <linux/pinctrl/consumer.h>
#include "tcc_sdr.h"

#define PREALLOCATE_DMA_BUFFER_MODE

//#define TCC_SDR_RX_ISR_DEBUG
//#define TCC_SDR_READ_DEBUG

#if 0
#define SDR_DBG(fmt, args...)  pr_info("<SDR> "fmt, ## args)
#else
#define SDR_DBG(fmt, args...)  do { } while (0)
#endif

#ifdef TCC_SDR_RX_ISR_DEBUG
#define RX_ISR_DBG(fmt, args...)  pr_info("<RX_ISR> "fmt, ## args)
#else
#define RX_ISR_DBG(fmt, args...)  do { } while (0)
#endif

#ifdef TCC_SDR_READ_DEBUG
#define READ_DBG(fmt, args...)  pr_info("<READ> "fmt, ## args)
#else
#define READ_DBG(fmt, args...)  do { } while (0)
#endif

#define SDR_MAX_PORT_NUM	(4)
struct tcc_sdr_port_t {
	void *dma_vaddr;
	dma_addr_t dma_paddr;
	uint32_t dma_sz;

	uint32_t valid_sz;
	uint32_t read_pos;
	uint32_t write_pos;
};

struct tcc_sdr_t {
	struct platform_device *pdev;
	void __iomem *dai_reg;
	void __iomem *adma_reg;
	struct clk *dai_pclk;
	struct clk *dai_hclk;
	uint32_t adma_irq;
	uint32_t adrcnt_mode;
	uint32_t dai_clk_rate[2];

	//cfg value
	bool radio_mode;
	uint32_t ports_num;
	uint32_t buffer_size;
	uint32_t bitmode;
	uint32_t bit_polarity;

	uint32_t period_bytes;
	void *dma_vaddr_base;
	dma_addr_t dma_paddr_base;
	uint32_t dma_total_size;
	struct tcc_sdr_port_t port[SDR_MAX_PORT_NUM];

	spinlock_t lock;
	struct mutex m;
	wait_queue_head_t wq;

	bool opened;
	bool started;

	struct miscdevice *misc_dev;
};

dma_addr_t get_current_dma_period_base_addr(struct tcc_sdr_t *sdr,
					    uint32_t sdr_port)
{
	dma_addr_t addr;

	addr = (sdr_port == 0) ? readl(sdr->adma_reg + ADMA_RxDaCdar) :
	    (sdr_port == 1) ? readl(sdr->adma_reg + ADMA_RxDaCar1) :
	    (sdr_port == 2) ? readl(sdr->adma_reg + ADMA_RxDaCar2) :
	    (sdr_port ==
	     3) ? readl(sdr->adma_reg + ADMA_RxDaCar3) : (dma_addr_t) NULL;

	return addr & ~(sdr->period_bytes - 1);
}

uint32_t calc_valid_sz_offset(uint32_t pre_offset, uint32_t cur_offset,
			      uint32_t max_sz)
{
	if (cur_offset >= pre_offset)
		return cur_offset - pre_offset;

	return cur_offset + (max_sz - pre_offset);
}

static unsigned int tcc_i2s_get_mclk_fs(unsigned int freq, struct clk *dai_pclk)
{
	unsigned int fs = 1024, minfs = 0, mdiff = freq;
	unsigned int temp = 0;

	while ((fs >= MIN_TCC_MCLK_FS) && (fs <= MAX_TCC_MCLK_FS)) {
		if ((fs < MIN_TCC_MCLK_FS) || (fs > MAX_TCC_MCLK_FS))
			break;
		temp = fs * freq;
		if (temp > MAX_TCC_SYSCLK)
			goto next;

		clk_set_rate(dai_pclk, freq * fs);
		temp = clk_get_rate(dai_pclk);
		temp = abs(freq * fs - temp);
		if (temp < mdiff) {
			mdiff = temp;
			minfs = fs;
		}
next:
		fs = fs / 2;
	}
	return minfs;
}

static unsigned int tcc_i2s_set_clock(struct tcc_sdr_t *sdr,
				      unsigned int req_lrck)
{
	unsigned int pre_mclk = 0, dai_damr = 0, temp = 0, ret = 0;

	dai_damr = readl(sdr->dai_reg + I2S_DAMR);

	pre_mclk = clk_get_rate(sdr->dai_pclk);

	if ((sdr->dai_clk_rate[0] != req_lrck)
	    || (sdr->dai_clk_rate[1] != pre_mclk)) {

		if (dai_damr & DAMR_DAI_EN) {
			temp = dai_damr & (~DAMR_DAI_EN);	//DAI Disable
			writel(temp, sdr->dai_reg + I2S_DAMR);
		}

		if (sdr->dai_pclk) {
			if (__clk_get_enable_count(sdr->dai_pclk))
				clk_disable_unprepare(sdr->dai_pclk);

			temp = tcc_i2s_get_mclk_fs(req_lrck, sdr->dai_pclk);

			clk_set_rate(sdr->dai_pclk, req_lrck * temp);

			SDR_DBG("CKC_CHECK: set_rate[%d],
				get_rate[%lu], fs[%d]\n",
			     (req_lrck * temp), clk_get_rate(sdr->dai_pclk),
			     temp);

			dai_damr &=
			    ~(DAMR_BITCLK_DIV_MASK | DAMR_FRMCLK_DIV_MASK);
			switch (temp) {
			case 256:
				dai_damr |= (DAMR_FRMCLK_DIV_64);
				break;
			case 512:
				dai_damr |=
				    (DAMR_BITCLK_DIV_8 | DAMR_FRMCLK_DIV_64);
				break;
			case 1024:
				dai_damr |=
				    (DAMR_BITCLK_DIV_16 | DAMR_FRMCLK_DIV_64);
				break;
			default:
				break;
			}

			//clk_prepare_enable(sdr->dai_pclk);

			sdr->dai_clk_rate[0] = req_lrck;
			sdr->dai_clk_rate[1] = clk_get_rate(sdr->dai_pclk);
		}
	}
	//    writel(dai_damr, sdr->dai_reg+I2S_DAMR);
	ret = dai_damr & (DAMR_BITCLK_DIV_MASK | DAMR_FRMCLK_DIV_MASK);
	return ret;
}

int tcc_sdr_set_param(struct tcc_sdr_t *sdr, struct HS_I2S_PARAM *p)
{
	SDR_DBG("%s\n", __func__);
	SDR_DBG("eChannel : %d\n", p->eChannel);
	SDR_DBG("eBitMode : %d\n", p->eBitMode);
	SDR_DBG("eBitPolarity: %d\n", p->eBitPolarity);
	SDR_DBG("eBufferSize : %d\n", p->eBufferSize);

	if (p->eRadioMode) {
		if (p->eChannel > 4)
			return -EINVAL;

		if ((p->eBitMode != 16) && (p->eBitMode != 20) &&
		    (p->eBitMode != 24) && (p->eBitMode != 30) &&
		    (p->eBitMode != 32) && (p->eBitMode != 40) &&
		    (p->eBitMode != 48) && (p->eBitMode != 60) &&
		    (p->eBitMode != 64) && (p->eBitMode != 80)) {
			return -EINVAL;
		}
	} else {		//I2S Slave Mode
		if ((p->eChannel != 1) && (p->eChannel != 2)
		    && (p->eChannel != 8)) {
			SDR_DBG("ERROR!! eChannel[%d] should be 2 or 8.\n",
				p->eChannel);
			return -EINVAL;
		}

		if ((p->eBitMode != 16) && (p->eBitMode != 24))
			return -EINVAL;
	}

	if (p->eBufferSize < TCC_SDR_BUF_SZ_MIN)
		return -EINVAL;

#ifdef PREALLOCATE_DMA_BUFFER_MODE
	if (p->eBufferSize > TCC_SDR_BUF_SZ_MAX)
		return -EINVAL;

#endif

	if (p->eRadioMode) {
		sdr->radio_mode = 1;
		sdr->ports_num = p->eChannel;
		sdr->bitmode = (p->eBitMode == 16) ? DRMR_BITMODE_16BIT :
		    (p->eBitMode == 20) ? DRMR_BITMODE_20BIT :
		    (p->eBitMode == 24) ? DRMR_BITMODE_24BIT :
		    (p->eBitMode == 30) ? DRMR_BITMODE_30BIT :
		    (p->eBitMode == 32) ? DRMR_BITMODE_32BIT :
		    (p->eBitMode == 40) ? DRMR_BITMODE_40BIT :
		    (p->eBitMode == 48) ? DRMR_BITMODE_48BIT :
		    (p->eBitMode == 60) ? DRMR_BITMODE_60BIT :
		    (p->eBitMode == 64) ? DRMR_BITMODE_64BIT :
		    (p->eBitMode ==
		     80) ? DRMR_BITMODE_80BIT : DRMR_BITMODE_20BIT;
		sdr->bit_polarity =
		    (p->eBitPolarity ==
		     SDR_BIT_POLARITY_POSITIVE_EDGE) ? 0 : DAMR_BIT_POLARITY;
	} else {		//I2S Slave Mode
		sdr->radio_mode = 0;
		sdr->ports_num = p->eChannel;
		sdr->bitmode =
		    (p->eBitMode ==
		     16) ? DAMR_RXS_LSB_16BIT : DAMR_RXS_LSB_24BIT;
		sdr->bitmode |= tcc_i2s_set_clock(sdr, p->eSampleRate);
	}
	sdr->buffer_size = p->eBufferSize;
	return 0;
}

void tcc_sdr_rx_isr(struct tcc_sdr_t *sdr)
{
	int i;
	uint32_t Overrun[SDR_MAX_PORT_NUM] = { 0 };
	uint32_t cur_period_offset;
	unsigned long flags;

	spin_lock_irqsave(&sdr->lock, flags);

	for (i = 0; i < sdr->ports_num; i++) {
		struct tcc_sdr_port_t *port = &sdr->port[i];

		cur_period_offset =
		    (uint32_t) (get_current_dma_period_base_addr(sdr, i) -
				port->dma_paddr);
		if (sdr->opened && sdr->started) {
			port->valid_sz +=
			    calc_valid_sz_offset(port->write_pos,
						 cur_period_offset,
						 port->dma_sz);

			RX_ISR_DBG("port(%d) valid_sz:0x%8x, read_pos:0x%8x,
			    write_pos:0x%8x, new_write_pos:0x%8x\n",
			     i, port->valid_sz, port->read_pos, port->write_pos,
			     cur_period_offset);

			if (port->valid_sz > port->dma_sz) {	// Overrun
				port->valid_sz = 0;
				port->read_pos = cur_period_offset;

				Overrun[i] = 1;
			}
		}
		port->write_pos = cur_period_offset;

	}
	spin_unlock_irqrestore(&sdr->lock, flags);

	wake_up(&sdr->wq);

	for (i = 0; i < sdr->ports_num; i++) {
		if (Overrun[i]) {	// Overrun
			if (sdr->opened && sdr->started) {
				SDR_DBG("%s - Overrun(%d), new_read_pos:0x%x,
				    dma_sz:0x%x, valid_sz(%p):0x%x\n",
				     __func__, i, sdr->port[i].read_pos,
				     sdr->port[i].dma_sz,
				     &sdr->port[i].valid_sz,
				     sdr->port[i].valid_sz);

			}
		}
	}
}

static irqreturn_t tcc_sdr_isr(int irq, void *dev)
{
	struct tcc_sdr_t *sdr = (struct tcc_sdr_t *)dev;
	uint32_t adma_istatus;

	adma_istatus = readl(sdr->adma_reg + ADMA_IntStatus);

	if (adma_istatus & (ADMA_INTSTATUS_DRI | ADMA_INTSTATUS_DRMI))
		tcc_sdr_rx_isr(sdr);

	writel(adma_istatus, sdr->adma_reg + ADMA_IntStatus);

	return IRQ_HANDLED;
}

unsigned int tcc_sdr_copy_from_dma(struct tcc_sdr_t *sdr, uint32_t sdr_port,
				   char *buf, unsigned int readcnt)
{
	struct tcc_sdr_port_t *port = &sdr->port[sdr_port];
	uint32_t first_sz, second_sz;
	uint32_t read_pos, new_read_pos;
	unsigned long flags = 0;

	if (!sdr->started) {
		SDR_DBG("%s - not started\n", __func__);
		return -1;
	}

	wait_event(sdr->wq, port->valid_sz >= readcnt);

	spin_lock_irqsave(&sdr->lock, flags);

	if (port->valid_sz < readcnt)
		readcnt = port->valid_sz;

	READ_DBG("<bf>(%d) base:0x%08x, len:0x%x\n", sdr_port, port->read_pos,
		 readcnt);

	read_pos = port->read_pos;
	new_read_pos = port->read_pos + readcnt;
	if (new_read_pos > port->dma_sz) {
		first_sz = port->dma_sz - port->read_pos;
		second_sz = readcnt - first_sz;
		port->read_pos = second_sz;
	} else {
		first_sz = readcnt;
		second_sz = 0;
		port->read_pos =
		    (new_read_pos == port->dma_sz) ? 0 : new_read_pos;
	}

	port->valid_sz -= readcnt;

	READ_DBG("<ft>-valid_sz:0x%x, read_pos:0x%x, readcnt:0x%x\n",
		 port->valid_sz, port->read_pos, readcnt);

	spin_unlock_irqrestore(&sdr->lock, flags);

	//first copy
	if (copy_to_user(buf, port->dma_vaddr + read_pos, first_sz) != 0) {
		pr_err("copy_to_user failed\n");
		return -EFAULT;
	}
	if (second_sz > 0) {
		//second copy
		if (copy_to_user(buf + first_sz, port->dma_vaddr,
			second_sz) != 0) {
			pr_err("copy_to_user failed\n");
			return -EFAULT;
		}
	}

	return readcnt;
}

static void set_radio_dma_inbuffer(struct tcc_sdr_t *sdr, uint32_t length,
				   gfp_t gfp)
{
	unsigned long dma_buffer = 0;
	int bit_count, i;
	ptrdiff_t align_offset, align_vaddr;
	uint32_t align_paddr;
	uint32_t rxdaadrcnt;
	uint32_t regval = readl(sdr->adma_reg + ADMA_ChCtrl);

	regval &= ~ADMA_CHCTRL_DAI_RX_EN;
	writel(regval, sdr->adma_reg + ADMA_ChCtrl);	// Disable ADMA

	//    dma_buffer = 0xFFFFFF00 / ((length >> 4) << 8);
	//    dma_buffer = dma_buffer * ((length >> 4) << 8);

	bit_count = 31;
	while (bit_count > 3) {
		if ((0x00000001 << bit_count) & length)
			break;

		bit_count--;
	};

	if (bit_count <= 3) {
		pr_err("Error : not valid length\n");
		return;
	}

	if (length != (1 << bit_count))
		length = 1 << (bit_count + 1);

	SDR_DBG("[%s], original len[%u]\n", __func__, length);


	dma_buffer = 0xFFFFFF00 & (~((length << 4) - 1));

	sdr->period_bytes = length / PERIOD_DIV_IN;

	writel(dma_buffer | 4, sdr->adma_reg + ADMA_RxDaParam);
	if (sdr->radio_mode) {
		if (sdr->adrcnt_mode) {
			rxdaadrcnt = sdr->period_bytes * sdr->ports_num * 2;
			SDR_DBG("<radio mode> rxdaadrcnt : %u\n", rxdaadrcnt);
			rxdaadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
		} else {
			rxdaadrcnt = ADMA_ADRCNT_MODE_SMASK | 0x7fffffff;
		}
		writel(((sdr->period_bytes * sdr->ports_num) >> 6) - 1,
		       sdr->adma_reg + ADMA_RxDaTCnt);
	} else {
		if (sdr->adrcnt_mode) {
			rxdaadrcnt = sdr->period_bytes;
			SDR_DBG("<i2s mode> rxdaadrcnt : %u\n", rxdaadrcnt);
			rxdaadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
		} else {
			rxdaadrcnt = ADMA_ADRCNT_MODE_SMASK | 0x7fffffff;
		}
		writel(((sdr->period_bytes) >> 5) - 1,
		       sdr->adma_reg + ADMA_RxDaTCnt);
	}
	writel(rxdaadrcnt, sdr->adma_reg + ADMA_RxDaAdrcnt);

	SDR_DBG("period_bytes : %d\n", sdr->period_bytes);

#ifndef PREALLOCATE_DMA_BUFFER_MODE
	if (sdr->dma_vaddr_base == NULL) {
		// plus 1 for align
		sdr->dma_total_size = length * (sdr->ports_num + 1);
		SDR_DBG("dma_total_size : %d\n", sdr->dma_total_size);
		sdr->dma_vaddr_base =
		    dma_alloc_coherent(&sdr->pdev->dev, sdr->dma_total_size,
				       &sdr->dma_paddr_base, gfp);
		SDR_DBG("dma_vaddr_base : %p, dma_paddr_base : %p\n",
			(void *)sdr->dma_vaddr_base,
			(void *)sdr->dma_paddr_base);
	} else {
		SDR_DBG("dma buffer is already allocated\n");
	}
#endif

	align_offset =
	    (((uint32_t) sdr->dma_paddr_base & ~(length - 1)) + length) -
	    sdr->dma_paddr_base;
	align_offset = (align_offset == length) ? 0 : align_offset;
	align_vaddr = (ptrdiff_t) sdr->dma_vaddr_base + align_offset;
	align_paddr = (uint32_t) sdr->dma_paddr_base + align_offset;

	SDR_DBG("align_offset:0x%08x, align_vaddr:0x%08x, align_paddr:0x%08x\n",
		align_offset, align_vaddr, align_paddr);

	for (i = 0; i < sdr->ports_num; i++) {
		uint32_t offset = length * i;

		sdr->port[i].dma_vaddr = (void *)(align_vaddr + offset);
		sdr->port[i].dma_paddr = (dma_addr_t) (align_paddr + offset);

		sdr->port[i].dma_sz = length;

		SDR_DBG("port(%d) vaddr: %p, paddr: %p\n",
			i, (void *)sdr->port[i].dma_vaddr,
			(void *)sdr->port[i].dma_paddr);
	}

	SDR_DBG("%s - HwRxDaParam [0x%X]\n", __func__,
		readl(sdr->adma_reg + ADMA_RxDaParam));
	SDR_DBG("%s - HwRxDaTCnt [%d]\n", __func__,
		readl(sdr->adma_reg + ADMA_RxDaTCnt));

	//For Multi-channel
	if (sdr->ports_num >= 2)
		writel(sdr->port[1].dma_paddr, sdr->adma_reg + ADMA_RxDaDar1);

	if (sdr->ports_num == 4) {
		writel(sdr->port[2].dma_paddr, sdr->adma_reg + ADMA_RxDaDar2);
		writel(sdr->port[3].dma_paddr, sdr->adma_reg + ADMA_RxDaDar3);
	}
	writel(sdr->port[0].dma_paddr, sdr->adma_reg + ADMA_RxDaDar);

	regval = readl(sdr->adma_reg + ADMA_TransCtrl);
	regval &= ~ADMA_TRANS_DAI_RX_WSIZE_MASK;
	regval |=
	    (ADMA_TRANS_DAI_RX_WSIZE_32 | ADMA_TRANS_RX_CONTINUOUS_MODE_EN);
	writel(regval, sdr->adma_reg + ADMA_TransCtrl);
}

int tcc_sdr_initialize(struct tcc_sdr_t *sdr, gfp_t gfp)
{
	uint32_t dai_damr, dai_drmr, dai_mccr0;
	uint32_t adma_chctrl, adma_transctrl, adma_rptctrl;
	int ret = 0;

	if (sdr->dai_hclk)
		clk_prepare_enable(sdr->dai_hclk);

	writel(0, sdr->dai_reg + I2S_DAVC);	// volume 0db

	if (sdr->dai_pclk) {
		if (sdr->radio_mode)
			clk_set_rate(sdr->dai_pclk, MAX_TCC_SYSCLK);
		else
			clk_set_rate(sdr->dai_pclk, sdr->dai_clk_rate[1]);
		clk_prepare_enable(sdr->dai_pclk);
	}
	//SLAVE MODE Setting
	dai_damr = readl(sdr->dai_reg + I2S_DAMR);
	dai_damr &= ~(DAMR_BCLK_PAD_SEL | DAMR_LRCK_PAD_SEL);
	dai_damr &= ~(DAMR_BIT_POLARITY);
	dai_damr |= sdr->bit_polarity;
	writel(dai_damr, sdr->dai_reg + I2S_DAMR);

	//Bit Mode Setting
	dai_drmr = readl(sdr->dai_reg + I2S_DRMR);
	if (sdr->radio_mode) {
		dai_drmr &= ~(DRMR_BITMODE_MASK | DRMR_FIFO_THRESHOLD_MASK);
		dai_drmr |= (sdr->bitmode | RADIO_RX_FIFO_THRESHOLD);
		writel(dai_drmr, sdr->dai_reg + I2S_DRMR);

		dai_drmr = readl(sdr->dai_reg + I2S_DRMR);
		dai_drmr &= ~DRMR_PORTSEL_MASK;
	} else {
		dai_damr = readl(sdr->dai_reg + I2S_DAMR);
		dai_damr &= ~(DAMR_RXS_MASK);
		dai_damr |= sdr->bitmode;
		dai_damr |= DAMR_DBTEN_EN;
		dai_damr |= DAMR_SYSCLK_MST_SEL;
		writel(dai_damr, sdr->dai_reg + I2S_DAMR);
	}

	dai_mccr0 = readl(sdr->dai_reg + I2S_MCCR0);
	dai_mccr0 &= ~MCCR0_DAO_DISABLE_MASK;

	adma_chctrl = readl(sdr->adma_reg + ADMA_ChCtrl);
	adma_transctrl = readl(sdr->adma_reg + ADMA_TransCtrl);
	adma_rptctrl = readl(sdr->adma_reg + ADMA_RptCtrl);

	adma_chctrl &=
	    ~(ADMA_CHCTRL_DAI_RX_MULTI_MASK | ADMA_CHCTRL_DAI_RX_MULTI_CH_EN);
	//ADMA Bit Mode Setting for I2S Mode
	if (!sdr->radio_mode) {
		sdr->bitmode = sdr->bitmode & DAMR_RXS_MASK;
		if ((sdr->bitmode == DAMR_RXS_LSB_16BIT)
		    || (sdr->bitmode == DAMR_RXS_MSB_16BIT)) {
			//16bit mode
			adma_chctrl |= ADMA_CHCTRL_DAI_RX_WIDTH_MASK;
		} else {
			//24bit mode
			adma_chctrl &= ~ADMA_CHCTRL_DAI_RX_WIDTH_MASK;
		}
	}

	adma_transctrl &= ~ADMA_TRANS_DAI_RX_BSIZE_MASK;
	adma_transctrl |= ADMA_TRANS_DAI_RX_BSIZE_8CYCLE;

	if (sdr->radio_mode) {
		if (sdr->ports_num == 1) {	//1port mode
			dai_drmr |= DRMR_PORTSEL_1;
			dai_mccr0 |=
			    (MCCR0_DAO1_DISABLE | MCCR0_DAO2_DISABLE |
			     MCCR0_DAO3_DISABLE);
			adma_rptctrl &= ~ADMA_RPTCTRL_DBTH_MASK;
			adma_rptctrl |= ADMA_RPTCTRL_DBTH(RADIO_RX_DBTH);
		} else if (sdr->ports_num == 2) {	//2port mode
			dai_drmr |= DRMR_PORTSEL_2;
			dai_mccr0 |= (MCCR0_DAO2_DISABLE | MCCR0_DAO3_DISABLE);
			adma_rptctrl &= ~ADMA_RPTCTRL_DBTH_MASK;
			adma_rptctrl |= ADMA_RPTCTRL_DBTH(RADIO_RX_DBTH);
		} else if ((sdr->ports_num == 4)) {	//4port mode
			dai_drmr |= DRMR_PORTSEL_4;
			adma_rptctrl &= ~ADMA_RPTCTRL_DBTH_MASK;
			adma_rptctrl |= ADMA_RPTCTRL_DBTH(RADIO_RX_DBTH);
		} else {
			SDR_DBG("Can't support %d port mode\n", sdr->ports_num);
		}
	} else {
		adma_rptctrl &= ~ADMA_RPTCTRL_DBTH_MASK;
		adma_rptctrl |= ADMA_RPTCTRL_DBTH(I2S_RX_DBTH);
		if (sdr->ports_num == 1) {	//mono channel
		} else if (sdr->ports_num == 2) {	//2port mode
			dai_damr &= ~DAMR_MULTI_PORT_EN;
			dai_mccr0 |=
			    (MCCR0_DAO1_DISABLE | MCCR0_DAO2_DISABLE |
			     MCCR0_DAO3_DISABLE);
			adma_chctrl &= ~(ADMA_CHCTRL_DAI_RX_MULTI_CH_EN);
			adma_chctrl |= ADMA_CHCTRL_DAI_RX_MULTI_7_1;
		} else if ((sdr->ports_num == 8)) {	//4port mode
			dai_damr |= DAMR_MULTI_PORT_EN;
			dai_mccr0 &= ~MCCR0_DAO_DISABLE_MASK;
			adma_chctrl |=
			    (ADMA_CHCTRL_DAI_RX_MULTI_CH_EN |
			     ADMA_CHCTRL_DAI_RX_MULTI_7_1);
		} else {
			SDR_DBG("I2S Can't support %d channel mode\n",
				sdr->ports_num);
		}
		sdr->ports_num = 1;
	}

	writel(dai_damr, sdr->dai_reg + I2S_DAMR);
	writel(dai_drmr, sdr->dai_reg + I2S_DRMR);
	writel(dai_mccr0, sdr->dai_reg + I2S_MCCR0);

	writel(adma_transctrl, sdr->adma_reg + ADMA_TransCtrl);
	writel(adma_rptctrl, sdr->adma_reg + ADMA_RptCtrl);
	writel(adma_chctrl, sdr->adma_reg + ADMA_ChCtrl);

	set_radio_dma_inbuffer(sdr, sdr->buffer_size, gfp);

	return ret;
}

int tcc_sdr_deinitialize(struct tcc_sdr_t *sdr)
{
#ifndef PREALLOCATE_DMA_BUFFER_MODE
	if (sdr->dma_vaddr_base != NULL) {
		SDR_DBG("Free pre-allocated dma buffer\n");
		dma_free_writecombine(&sdr->pdev->dev, sdr->dma_total_size,
				      sdr->dma_vaddr_base, sdr->dma_paddr_base);
		sdr->dma_vaddr_base = NULL;
		sdr->dma_paddr_base = (dma_addr_t) NULL;
	}
#endif

	if ((sdr->dai_pclk) && (__clk_get_enable_count(sdr->dai_pclk)))
		clk_disable_unprepare(sdr->dai_pclk);

	if ((sdr->dai_hclk) && (__clk_get_enable_count(sdr->dai_hclk)))
		clk_disable_unprepare(sdr->dai_hclk);

	return 0;
}

int tcc_sdr_start(struct tcc_sdr_t *sdr, gfp_t gfp)
{
	uint32_t adma_transctrl, adma_chctrl;
	uint32_t dai_drmr, dai_damr;
	uint32_t i;

	if (sdr->started) {
		SDR_DBG("%s - already started\n", __func__);
		return -1;
	}

	tcc_sdr_initialize(sdr, gfp);

	for (i = 0; i < sdr->ports_num; i++) {
		sdr->port[i].valid_sz = 0;
		sdr->port[i].read_pos = 0;
		sdr->port[i].write_pos = 0;
	}

	if (sdr->radio_mode) {
		dai_drmr = readl(sdr->dai_reg + I2S_DRMR);
		dai_drmr |= DRMR_RADIO_EN;
		writel(dai_drmr, sdr->dai_reg + I2S_DRMR);

		dai_damr = readl(sdr->dai_reg + I2S_DAMR);
		dai_damr |= DAMR_DAI_RX_EN;
		writel(dai_damr, sdr->dai_reg + I2S_DAMR);
	} else {
		dai_damr = readl(sdr->dai_reg + I2S_DAMR);
		dai_damr |= (DAMR_DAI_RX_EN | DAMR_DAI_EN);
		writel(dai_damr, sdr->dai_reg + I2S_DAMR);
	}

	adma_transctrl = readl(sdr->adma_reg + ADMA_TransCtrl);
	adma_transctrl |= ADMA_TRANS_DAI_RX_REPEAT_MODE_EN;
	writel(adma_transctrl, sdr->adma_reg + ADMA_TransCtrl);

	adma_chctrl = readl(sdr->adma_reg + ADMA_ChCtrl);
	adma_chctrl |= (ADMA_CHCTRL_DAI_RX_EN | ADMA_CHCTRL_DAI_RX_INT_EN);
	writel(adma_chctrl, sdr->adma_reg + ADMA_ChCtrl);

	sdr->started = true;

	return 0;
}

int tcc_sdr_stop(struct tcc_sdr_t *sdr)
{
	uint32_t adma_transctrl, dai_drmr, dai_damr;
	uint32_t adma_chctrl, adma_rxdadar;

	if (!sdr->started) {
		SDR_DBG("%s - not started\n", __func__);
		return -1;
	}

	SDR_DBG("%s\n", __func__);

	adma_chctrl = readl(sdr->adma_reg + ADMA_ChCtrl);
	adma_chctrl &= ~ADMA_CHCTRL_DAI_RX_EN;
	writel(adma_chctrl, sdr->adma_reg + ADMA_ChCtrl);

	/* HopCount Clear */
	adma_transctrl = readl(sdr->adma_reg + ADMA_TransCtrl);
	adma_transctrl |= ADMA_TRANS_HCNT_CLR;
	writel(adma_transctrl, sdr->adma_reg + ADMA_TransCtrl);

	adma_rxdadar = readl(sdr->adma_reg + ADMA_RxDaDar);
	writel(adma_rxdadar, sdr->adma_reg + ADMA_RxDaDar);

	msleep(1);

	adma_transctrl &= ~ADMA_TRANS_HCNT_CLR;
	writel(adma_transctrl, sdr->adma_reg + ADMA_TransCtrl);

	if (sdr->radio_mode) {
		dai_drmr = readl(sdr->dai_reg + I2S_DRMR);
		dai_drmr &= ~DRMR_RADIO_EN;
		writel(dai_drmr, sdr->dai_reg + I2S_DRMR);
	}

	dai_damr = readl(sdr->dai_reg + I2S_DAMR);
	dai_damr &= ~(DAMR_DAI_RX_EN | DAMR_DAI_EN);
	writel(dai_damr, sdr->dai_reg + I2S_DAMR);

	//FIFO Clear
	dai_drmr = readl(sdr->dai_reg + I2S_DRMR);
	dai_drmr |= DRMR_FIFO_RX_CLR;
	writel(dai_drmr, sdr->dai_reg + I2S_DRMR);
	msleep(1);
	dai_drmr &= ~DRMR_FIFO_RX_CLR;
	writel(dai_drmr, sdr->dai_reg + I2S_DRMR);

	tcc_sdr_deinitialize(sdr);

	sdr->started = false;

	return 0;
}

ssize_t tcc_sdr_read(struct file *flip, char __user *ibuf, size_t count,
		     loff_t *f_pos)
{
	return 0;
}

ssize_t tcc_sdr_write(struct file *flip, const char __user *buf, size_t count,
		      loff_t *f_pos)
{
	return count;
}

unsigned int tcc_sdr_poll(struct file *flip, struct poll_table_struct *wait)
{
	struct miscdevice *misc = (struct miscdevice *)flip->private_data;
	struct tcc_sdr_t *sdr =
	    (struct tcc_sdr_t *)dev_get_drvdata(misc->parent);
	int i;

	//SDR_DBG("%s\n", __func__);

	for (i = 0; i < sdr->ports_num; i++) {
		if (sdr->port[i].valid_sz)
			return (POLLIN | POLLRDNORM);
	}

	poll_wait(flip, &sdr->wq, wait);

	for (i = 0; i < sdr->ports_num; i++) {
		if (sdr->port[i].valid_sz)
			return (POLLIN | POLLRDNORM);
	}

	return 0;
}

long tcc_sdr_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice *)flip->private_data;
	struct tcc_sdr_t *sdr =
	    (struct tcc_sdr_t *)dev_get_drvdata(misc->parent);
	int ret = -1;

	mutex_lock(&sdr->m);
	switch (cmd) {
	case HSI2S_RADIO_MODE_RX_DAI:
		{
			struct HS_RADIO_RX_PARAM param;

			if (!copy_from_user(&param,
				(const void *)arg,
				sizeof(struct HS_RADIO_RX_PARAM))) {
				ret = tcc_sdr_copy_from_dma(sdr,
				param.eIndex,
				param.eBuf,
				param.eReadCount);
			} else {
				SDR_DBG("HSI2S_RADIO_MODE_RX_DAI Fail!!\n");
			}
		}
		break;
	case HSI2S_I2S_MODE_RX_DAI:
		{
			struct HS_RADIO_RX_PARAM param;

			if (!copy_from_user(&param,
				(const void *)arg,
				sizeof(struct HS_RADIO_RX_PARAM))) {
				param.eIndex = 0;
				ret = tcc_sdr_copy_from_dma(sdr,
					param.eIndex,
					param.eBuf,
					param.eReadCount);
			} else {
				SDR_DBG("HSI2S_I2S_MODE_RX_DAI Fail!!\n");
			}
		}
		break;
	case HSI2S_RX_START:
		SDR_DBG("HSI2S_RX_START\n");
		ret = tcc_sdr_start(sdr, GFP_KERNEL);
		break;
	case HSI2S_RX_STOP:
		SDR_DBG("HSI2S_RX_STOP\n");
		ret = tcc_sdr_stop(sdr);
		break;
	case HSI2S_SET_PARAMS:
		SDR_DBG("HSI2S_SET_PARAMS\n");
		{
			struct HS_I2S_PARAM param;

			if (!copy_from_user(&param,
				(const void *)arg,
				sizeof(struct HS_I2S_PARAM))) {
				ret = tcc_sdr_set_param(sdr, &param);
			} else {
				SDR_DBG("HSI2S_SET_PARAMS Fail!!\n");
			}
		}
		break;
	default:
		SDR_DBG("CMD(0x%x)is not supported\n", cmd);
		ret = 0;
		break;
	}
	mutex_unlock(&sdr->m);

	return ret;
}

int tcc_sdr_open(struct inode *inode, struct file *flip)
{
	struct miscdevice *misc = (struct miscdevice *)flip->private_data;
	struct tcc_sdr_t *sdr =
	    (struct tcc_sdr_t *)dev_get_drvdata(misc->parent);
	int ret;

	SDR_DBG("%s\n", __func__);

	ret =
	    request_irq(sdr->adma_irq, tcc_sdr_isr, 0x0, "tcc-sdr",
			(void *)sdr);

	if (sdr->opened == true) {
		SDR_DBG("it is already opend.\n");
		return -EMFILE;
	}

	sdr->opened = true;

	return ret;
}

int tcc_sdr_release(struct inode *inode, struct file *flip)
{
	struct miscdevice *misc = (struct miscdevice *)flip->private_data;
	struct tcc_sdr_t *sdr =
	    (struct tcc_sdr_t *)dev_get_drvdata(misc->parent);

	SDR_DBG("%s\n", __func__);

	free_irq(sdr->adma_irq, sdr);

	tcc_sdr_stop(sdr);

	sdr->opened = false;
	sdr->started = false;

	return 0;
}

static const struct file_operations tcc_sdr_fops = {
	.owner = THIS_MODULE,
	.read = tcc_sdr_read,
	.write = tcc_sdr_write,
	.poll = tcc_sdr_poll,
	.unlocked_ioctl = tcc_sdr_ioctl,
	.open = tcc_sdr_open,
	.release = tcc_sdr_release,
};

static int parse_sdr_dt(struct platform_device *pdev, struct tcc_sdr_t *sdr)
{
	struct device_node *of_node_adma;
	const char *devname = NULL;
	/* get dai info. */
	sdr->dai_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)sdr->dai_reg)) {
		sdr->dai_reg = NULL;
		SDR_DBG("dai_reg is NULL\n");
		return -EINVAL;
	}

	SDR_DBG("dai_reg=%p\n", sdr->dai_reg);

	sdr->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(sdr->dai_pclk))
		sdr->dai_pclk = NULL;

	sdr->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(sdr->dai_hclk))
		sdr->dai_hclk = NULL;

	if (of_property_read_u32(pdev->dev.of_node, "clock-frequency",
	    &sdr->dai_clk_rate[1]) < 0) {
		SDR_DBG("clock-frequency value is not exist\n");
		return -EINVAL;
	}
	sdr->dai_clk_rate[0] =
	    (sdr->dai_clk_rate[1] > 48000) ? 48000 : sdr->dai_clk_rate[1];
	sdr->dai_clk_rate[1] = 1;
	SDR_DBG("clk_rate=%u\n", sdr->dai_clk_rate[0]);

	of_node_adma = of_parse_phandle(pdev->dev.of_node, "adma", 0);
	/* get adma info */
	if (of_node_adma) {
		sdr->adma_reg = of_iomap(of_node_adma, 0);
		if (IS_ERR((void *)sdr->adma_reg))
			sdr->adma_reg = NULL;
		else
			SDR_DBG("adma_reg=%p\n", sdr->adma_reg);

		sdr->adma_irq = of_irq_get(of_node_adma, 0);
		of_property_read_u32(of_node_adma, "adrcnt-mode",
				     &sdr->adrcnt_mode);
		SDR_DBG("adrcnt_mode : %u\n", sdr->adrcnt_mode);
	} else {
		SDR_DBG("of_node_adma is NULL\n");
		return -1;
	}

	if (of_property_read_string(pdev->dev.of_node,
		"dev-name", &devname) == 0) {
		sdr->misc_dev->name = devname;
		SDR_DBG("dev-name : %s\n", devname);
	} else {
		SDR_DBG("default dev-name\n");
	}
	return 0;
}

static int tcc_sdr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_sdr_t *sdr = kzalloc(sizeof(struct tcc_sdr_t), GFP_KERNEL);

	SDR_DBG("%s\n", __func__);

	memset(sdr, 0, sizeof(struct tcc_sdr_t));

	sdr->misc_dev = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	sdr->misc_dev->parent = &pdev->dev;
	sdr->misc_dev->minor = MISC_DYNAMIC_MINOR;
	sdr->misc_dev->name = "tcc-sdr";
	sdr->misc_dev->fops = &tcc_sdr_fops;

	ret = parse_sdr_dt(pdev, sdr);
	if (ret < 0) {
		SDR_DBG("%s : Fail to parse sdr dt\n", __func__);
		return ret;
	}
	//setup default cfg value
	sdr->ports_num = 4;
	sdr->bitmode = DRMR_BITMODE_20BIT;
	sdr->buffer_size = TCC_SDR_BUF_SZ_MAX;

	platform_set_drvdata(pdev, sdr);
	sdr->pdev = pdev;

	init_waitqueue_head(&sdr->wq);
	mutex_init(&sdr->m);
	spin_lock_init(&sdr->lock);

#ifdef PREALLOCATE_DMA_BUFFER_MODE
	// plus 1 for align
	sdr->dma_total_size = TCC_SDR_BUF_SZ_MAX * (SDR_MAX_PORT_NUM + 1);
	SDR_DBG("dma_total_size : %d\n", sdr->dma_total_size);
	sdr->dma_vaddr_base =
	    dma_alloc_coherent(&sdr->pdev->dev, sdr->dma_total_size,
			       &sdr->dma_paddr_base, GFP_KERNEL);
	SDR_DBG("dma_vaddr_base : %p, dma_paddr_base : %p\n",
		(void *)sdr->dma_vaddr_base, (void *)sdr->dma_paddr_base);

	if (sdr->dma_vaddr_base == NULL) {
		SDR_DBG("dma memory allocation failed\n");
		return -ENOMEM;
	}
#endif

	sdr->opened = false;

	if (misc_register(sdr->misc_dev)) {
		SDR_DBG(KERN_WARNING "Couldn't register device .\n");
		return -EBUSY;
	}

	return 0;
}

static int tcc_sdr_remove(struct platform_device *pdev)
{
	struct tcc_sdr_t *sdr = (struct tcc_sdr_t *)platform_get_drvdata(pdev);

	SDR_DBG("%s\n", __func__);

	if (sdr->dma_vaddr_base) {
		dma_free_coherent(&sdr->pdev->dev, sdr->dma_total_size,
				  sdr->dma_vaddr_base, sdr->dma_paddr_base);
	}

	kfree(sdr->misc_dev);
	sdr->misc_dev = NULL;

	kfree(sdr);

	return 0;
}

static int tcc_sdr_suspend(struct platform_device *pdev, pm_message_t state)
{
	//struct tcc_sdr_t *sdr =
	//	(struct tcc_sdr_t*)platform_get_drvdata(pdev);
	struct pinctrl *pinctrl;

	SDR_DBG("%s\n", __func__);

	pinctrl = pinctrl_get_select(&pdev->dev, "idle");
	if (IS_ERR(pinctrl))
		pr_err("%s : pinctrl suspend error[0x%p]\n", __func__, pinctrl);

	//sdr->started = false;
	return 0;
}

static int tcc_sdr_resume(struct platform_device *pdev)
{
	//struct tcc_sdr_t *sdr =
	//	(struct tcc_sdr_t*)platform_get_drvdata(pdev);
	struct pinctrl *pinctrl;

	SDR_DBG("%s\n", __func__);
	//tcc_sdr_start(sdr, GFP_ATOMIC);

	pinctrl = pinctrl_get_select(&pdev->dev, "default");
	if (IS_ERR(pinctrl))
		pr_err("%s : pinctrl resume error[0x%p]\n", __func__, pinctrl);

	return 0;
}

static const struct of_device_id tcc_sdr_of_match[] = {
	{.compatible = "telechips,sdr"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_sdr_of_match);

static struct platform_driver tcc_sdr_driver = {
	.probe = tcc_sdr_probe,
	.remove = tcc_sdr_remove,
	.suspend = tcc_sdr_suspend,
	.resume = tcc_sdr_resume,
	.driver = {
		   .name = "tcc_sdr_drv",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(tcc_sdr_of_match),
#endif
		   },
};

static int __init tcc_sdr_init(void)
{
	SDR_DBG("%s\n", __func__);
	return platform_driver_register(&tcc_sdr_driver);
}

static void __exit tcc_sdr_exit(void)
{
	SDR_DBG("%s\n", __func__);
	platform_driver_unregister(&tcc_sdr_driver);
}

//------------------------------------------------------

module_init(tcc_sdr_init);
module_exit(tcc_sdr_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips SDR Driver");
MODULE_LICENSE("GPL");
