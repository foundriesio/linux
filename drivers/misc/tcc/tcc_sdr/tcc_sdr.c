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

//#define TCC_SDR_RX_ISR_DEBUG
//#define TCC_SDR_READ_DEBUG

/* //for debug
 *#define SDR_DBG(fmt, args...)  pr_info("[DEBUG][SDR] "fmt, ## args)
 *#define DBG(fmt, args...)  pr_info(fmt, ## args)
 */
#define SDR_DBG(fmt, args...)  do { } while (0)
#define DBG(fmt, args...)  do { } while (0)

#ifdef TCC_SDR_RX_ISR_DEBUG
#define RX_ISR_DBG(fmt, args...)  pr_info("[DEBUG][SDR_RX_ISR] "fmt, ## args)
#else
#define RX_ISR_DBG(fmt, args...)  do { } while (0)
#endif

#ifdef TCC_SDR_READ_DEBUG
#define READ_DBG(fmt, args...)  pr_info("[DEBUG][SDR_READ] "fmt, ## args)
#else
#define READ_DBG(fmt, args...)  do { } while (0)
#endif

#define SDR_WARN(fmt, args...)	pr_info("[WARN][SDR] "fmt, ## args)
#define SDR_ERR(fmt, args...)	pr_info("[ERROR][SDR] "fmt, ## args)

#define PREALLOCATE_DMA_BUFFER_MODE

#define SDR_MAX_PORT_NUM	(4)
#define SDR_READ_TIMEOUT 1000
#define OVERRUN_NOTIFY_INTERVAL 100 //ms

//#define HRSTEN_ENABLE
//#define RADIO_STOP_MIN_DELAY
struct tcc_sdr_port_t {
	void *dma_vaddr;
	dma_addr_t dma_paddr;
	uint32_t dma_sz;

	uint32_t valid_sz;
	uint32_t read_pos;
	uint32_t write_pos;
	int overrun;
};

struct tcc_sdr_t {
	int blk_no;
	struct platform_device *pdev;
	void __iomem *dai_reg;
	void __iomem *adma_reg;
#ifdef HRSTEN_ENABLE
	void __iomem *iocfg_reg;
	uint32_t hrsten_reg_offset;
	uint32_t hrsten_bit_offset;
#endif
	struct clk *dai_pclk;
	struct clk *dai_hclk;
	struct clk *dai_filter_clk;	//This is for normal I2S slave mode
	uint32_t adma_irq;
	uint32_t adrcnt_mode;
	uint32_t dai_clk_rate[2];

	//cfg value
	bool radio_mode;
	uint32_t ports_num;
	uint32_t buffer_bytes;
	uint32_t bitmode;
	uint32_t bit_polarity;
	//This is for normal I2S slave mode
	uint32_t channels;
	enum TCC_ADMA_I2S_TYPE dev_type;
	uint32_t bclk_ratio;
	uint32_t mclk_div;
	void *mono_dma_vaddr;
	dma_addr_t mono_dma_paddr;
	//This is for normal I2S slave mode
#ifdef RADIO_STOP_MIN_DELAY
	uint32_t nperiod;
	uint32_t nirq;
#endif
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
#ifdef OVERRUN_NOTIFY_INTERVAL
	struct timeval pre_overrun;
	bool first_overrun;
#endif
};

uint32_t get_current_dma_period_base_addr(
	struct tcc_sdr_t *sdr,
	uint32_t sdr_port)
{
	uint32_t addr = 0;

	if (sdr->radio_mode) {
		addr =
		    tcc_adma_radio_rx_get_cur_dma_addr(sdr->adma_reg, sdr_port);
	} else {
		addr = tcc_adma_dai_rx_get_cur_dma_addr(sdr->adma_reg);
	}

	return addr;
}

uint32_t calc_valid_sz_offset(
	uint32_t pre_offset,
	uint32_t cur_offset,
	uint32_t max_sz)
{
	if (cur_offset >= pre_offset)
		return cur_offset - pre_offset;

	return cur_offset + (max_sz - pre_offset);
}

static int tcc_adma_get_radio_dbth_value(
	enum TCC_ADMA_I2S_TYPE dev_type,
	uint32_t fifo_thresh)
{
	//fifo_threshold: 64, 128, 256
	const int dbth_tbl_2ch[3] = { 0x07, 0x07, 0x07 };	// burst_16
	const int dbth_tbl_7_1ch[3] = { 0x03, 0x07, 0x0f };	// burst_16
	const int dbth_tbl_9_1ch[3] = { 0x07, 0x07, 0x07 };	// burst_16
	int thresh_idx = (fifo_thresh == TCC_RADIO_FIFO_THRESH_64) ? 0 :
	    (fifo_thresh == TCC_RADIO_FIFO_THRESH_128) ? 1 : 2;

	return (dev_type == TCC_ADMA_I2S_9_1CH) ?
		dbth_tbl_9_1ch[thresh_idx] :
		(dev_type == TCC_ADMA_I2S_7_1CH) ?
		dbth_tbl_7_1ch[thresh_idx] :
		dbth_tbl_2ch[thresh_idx];
}

static int tcc_adma_get_i2s_dbth_value(
	enum TCC_ADMA_I2S_TYPE dev_type,
	int channels,
	enum TCC_ADMA_BURST_SIZE burst_size)
{
	//tdm, mono, 2ch, 4ch, 6ch, 8ch, 10ch
	const int dbth_tbl_2ch[2][7] = {
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},	// burst_4
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},	// burst_8
	};
	const int dbth_tbl_7_1ch[2][7] = {
		{0x0f, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f},	// burst_4
		{0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07},	// burst_8
	};
	const int dbth_tbl_9_1ch[2][7] = {
		{0x01, 0x01, 0x01, 0x07, 0x0b, 0x0f, 0x13},	// burst_4
		{0x01, 0x01, 0x01, 0x03, 0x05, 0x07, 0x09},	// burst_8
	};
	int ch_idx = (channels / 2) + 1;
	int burst_idx = (burst_size == TCC_ADMA_BURST_CYCLE_4) ? 0 : 1;

	return (dev_type == TCC_ADMA_I2S_9_1CH) ?
		dbth_tbl_9_1ch[burst_idx][ch_idx] :
		(dev_type == TCC_ADMA_I2S_7_1CH) ?
		dbth_tbl_7_1ch[burst_idx][ch_idx] :
		dbth_tbl_2ch[burst_idx][ch_idx];
}

static void set_radio_dma_inbuffer(
	struct tcc_sdr_t *sdr,
	uint32_t length,
	gfp_t gfp)
{
	bool mono_mode =
		((sdr->radio_mode == 0) && (sdr->channels == 1)) ?
		true : false;
	ptrdiff_t align_offset, align_vaddr;
	uint32_t align_paddr;
	int ret = 0, i = 0;

   //SDR_DBG("period_bytes : %d\n", sdr->period_bytes);

#ifndef PREALLOCATE_DMA_BUFFER_MODE
	if (sdr->dma_vaddr_base == NULL) {
		// plus 1 for align
		sdr->dma_total_size = length * (sdr->ports_num + 1);
		SDR_DBG("dma_total_size : %d\n",
			sdr->dma_total_size);
		sdr->dma_vaddr_base =
			dma_alloc_coherent(&sdr->pdev->dev, sdr->dma_total_size,
			  &sdr->dma_paddr_base, gfp);
		SDR_DBG("dma_vaddr_base : %p\n",
			(void *)sdr->dma_vaddr_base);
	} else {
		SDR_DBG("dma buffer is already allocated\n");
	}

	if ((mono_mode) && (sdr->mono_dma_vaddr == NULL)) {
		sdr->mono_dma_vaddr =
		dma_alloc_coherent(&sdr->pdev->dev, length,
			&sdr->mono_dma_paddr, GFP_KERNEL);
		SDR_DBG("mono_dma_vaddr : %p\n",
		(void *)sdr->mono_dma_vaddr);
	} else {
		SDR_DBG("mono dma buffer is already allocated\n");
	}
#endif

	if (sdr->radio_mode || ((sdr->radio_mode == 0) &&
			(sdr->adrcnt_mode == 0))) {
		align_offset = ((((uint32_t) sdr->dma_paddr_base &
			~(length - 1)) + length) - sdr->dma_paddr_base);
		align_offset = (align_offset == length) ?
			0 : align_offset;
	} else
		align_offset = 0;

	align_vaddr = (ptrdiff_t) sdr->dma_vaddr_base + align_offset;
	align_paddr = (uint32_t) sdr->dma_paddr_base + align_offset;

	SDR_DBG("align_offset:0x%08x, align_vaddr:0x%08x, align_paddr:0x%08x\n",
		  align_offset, align_vaddr, align_paddr);

	for (i = 0; i < sdr->ports_num; i++) {
		uint32_t offset = length * i;

		sdr->port[i].dma_vaddr = (void *)(align_vaddr + offset);
		sdr->port[i].dma_paddr = (dma_addr_t) (align_paddr + offset);

		sdr->port[i].dma_sz = length;

		SDR_DBG("port(%d) vaddr: %p\n", i,
			(void *)sdr->port[i].dma_vaddr);
	}

	if (sdr->radio_mode) {

		ret = tcc_adma_set_rx_dma_params(sdr->adma_reg, length,
			sdr->period_bytes, sdr->bitmode,
			TCC_ADMA_BURST_CYCLE_16,
			mono_mode, sdr->radio_mode,
			sdr->ports_num,
			sdr->adrcnt_mode);
		if (ret < 0)
			SDR_DBG("It has something wrong. ret = %d\n", ret);

		for (i = 0; i < sdr->ports_num; i++) {
			ret =
				tcc_adma_set_rx_dma_addr(sdr->adma_reg,
				(uint32_t) sdr->port[i].dma_paddr,
				(uint32_t) sdr->mono_dma_paddr,
				mono_mode,
				sdr->radio_mode, i);
			if (ret < 0) {
				SDR_DBG("It has something wrong.");
				DBG("ret[%d] = %d\n", i, ret);
			}
		}

	} else {

		ret =
			tcc_adma_set_rx_dma_params(sdr->adma_reg, length,
				sdr->period_bytes, sdr->bitmode,
				TCC_ADMA_BURST_CYCLE_8,
				mono_mode, sdr->radio_mode,
				sdr->ports_num,
				sdr->adrcnt_mode);
		if (ret < 0) {
			SDR_DBG("It has something wrong. ret = %d\n",
			ret);
		}

		for (i = 0; i < sdr->ports_num; i++) {
			ret =
			tcc_adma_set_rx_dma_addr(sdr->adma_reg,
				(uint32_t) sdr->port[i].dma_paddr,
				(uint32_t) sdr->mono_dma_paddr,
				mono_mode,
				sdr->radio_mode, i);
			if (ret < 0) {
				SDR_DBG("It has something wrong.");
				pr_info("ret[%d] = %d\n", i, ret);
			}
		}
	}

	tcc_adma_set_rx_dma_repeat_type(sdr->adma_reg,
		TCC_ADMA_REPEAT_FROM_CUR_ADDR);
	tcc_adma_repeat_infinite_mode(sdr->adma_reg);

	SDR_DBG("%s - HwRxDaParam [0x%X]\n", __func__,
		readl(sdr->adma_reg + TCC_ADMA_RXDAPARAM_OFFSET));
	SDR_DBG("%s - HwRxDaTCnt [%d]\n", __func__,
		readl(sdr->adma_reg + TCC_ADMA_RXDATCNT_OFFSET));
}

int tcc_sdr_initialize(struct tcc_sdr_t *sdr, gfp_t gfp)
{
	uint32_t dbth = 0;
	int ret = 0;

	if (sdr->dai_hclk)
		clk_prepare_enable(sdr->dai_hclk);

	tcc_dai_set_rx_mute(sdr->dai_reg, true);

	if (sdr->dai_pclk) {
		clk_set_rate(sdr->dai_filter_clk,
				TCC_DAI_FILTER_MAX_FREQ);
		clk_prepare_enable(sdr->dai_filter_clk);
		clk_set_rate(sdr->dai_pclk, TCC_DAI_MAX_FREQ);
		clk_prepare_enable(sdr->dai_pclk);
	}
	//SLAVE MODE Setting mclk_mst, bclk_mst,
	//lrck_mst, tdm_mode, is_pinctrl_export
	tcc_dai_set_master_mode(sdr->dai_reg, false, false, false, false, true);

	set_radio_dma_inbuffer(sdr, sdr->buffer_bytes, gfp);
	//Bit Mode Setting
	if (sdr->radio_mode) {
		tcc_radio_set_fifo_threshold(sdr->dai_reg,
					RADIO_RX_FIFO_THRESHOLD);
		tcc_radio_set_bitmode(sdr->dai_reg, sdr->bitmode);
		tcc_radio_set_portsel(sdr->dai_reg, sdr->ports_num);
		tcc_dai_set_dao_mask(sdr->dai_reg, true, true, true, true,
				true);

		dbth =
		tcc_adma_get_radio_dbth_value(sdr->dev_type,
					 RADIO_RX_FIFO_THRESHOLD);
		tcc_adma_dai_threshold(sdr->adma_reg, dbth);

		//audio filter enable
		tcc_dai_set_audio_filter_enable(sdr->dai_reg, false);

		tcc_dai_dma_threshold_enable(sdr->dai_reg, true);

	} else {
		if (sdr->bitmode == 16) {
			tcc_dai_set_rx_format(sdr->dai_reg, TCC_DAI_LSB_16);
		} else {    //24bitmode
			tcc_dai_set_rx_format(sdr->dai_reg, TCC_DAI_LSB_24);
		}
		tcc_adma_set_dai_rx_dma_width(sdr->adma_reg, sdr->bitmode);
		dbth =
		tcc_adma_get_i2s_dbth_value(sdr->dev_type, sdr->channels,
				   TCC_ADMA_BURST_CYCLE_8);
		tcc_adma_dai_threshold(sdr->adma_reg, dbth);

		//audio filter enable
		tcc_dai_set_audio_filter_enable(sdr->dai_reg, false);

		tcc_dai_dma_threshold_enable(sdr->dai_reg, true);
		tcc_dai_set_dao_mask(sdr->dai_reg, true, true, true, true,
				true);

		if (sdr->channels == 1) {   //mono channel
			tcc_dai_set_multiport_mode(sdr->dai_reg, false);
			tcc_adma_set_dai_rx_multi_ch(sdr->adma_reg, false,
						TCC_ADMA_MULTI_CH_MODE_7_1);
		} else if (sdr->channels == 2) {    //2port mode
			tcc_dai_set_multiport_mode(sdr->dai_reg, false);
			tcc_adma_set_dai_rx_multi_ch(sdr->adma_reg, false,
						TCC_ADMA_MULTI_CH_MODE_7_1);
		} else if ((sdr->channels == 8)) {  //4port mode
			tcc_dai_set_multiport_mode(sdr->dai_reg, true);
			tcc_adma_set_dai_rx_multi_ch(sdr->adma_reg, true,
						TCC_ADMA_MULTI_CH_MODE_7_1);
		} else {
			SDR_DBG("I2S Can't support %d channel mode\n",
			  sdr->channels);
		}
	}

	tcc_adma_set_dai_rx_dma_repeat_enable(sdr->adma_reg, true);
	return ret;
}

int tcc_sdr_set_param(struct tcc_sdr_t *sdr, struct HS_I2S_PARAM *p)
{
	uint32_t period_min = 0, sz_check = 0, overall_sz = 0;
	int ret = 0;

	SDR_DBG("%s\n", __func__);
	SDR_DBG("eChannel : %d\n", p->eChannel);
	SDR_DBG("eBitMode : %d\n", p->eBitMode);
	SDR_DBG("eBitPolarity: %d\n", p->eBitPolarity);
	SDR_DBG("eBufferSize : %d\n", p->eBufferSize);
	SDR_DBG("ePeriodSize : %d\n", p->ePeriodSize);

	if (p->eRadioMode) {
		if ((p->eChannel > 4) || (p->eChannel <= 0)) {
			SDR_WARN("eChannel[%d] should be 1/2/4.",
				p->eChannel);
			pr_info("So, it is changed by default[%d]\n",
			    RADIO_MODE_DEFAULT_CHANNEL);
			p->eChannel = RADIO_MODE_DEFAULT_CHANNEL;
			ret = 1;
		}

		if ((p->eBitMode != 16) && (p->eBitMode != 20) &&
		    (p->eBitMode != 24) && (p->eBitMode != 30) &&
		    (p->eBitMode != 32) && (p->eBitMode != 40) &&
		    (p->eBitMode != 48) && (p->eBitMode != 60) &&
		    (p->eBitMode != 64) && (p->eBitMode != 80)) {
			SDR_WARN("eBitMode[%d] is wrong.",
				p->eBitMode);
			pr_info("So, it is changed by default[%d]\n",
				RADIO_MODE_DEFAULT_BITMODE);
			p->eBitMode = RADIO_MODE_DEFAULT_BITMODE;
			ret = 1;
		}
		period_min = TCC_SDR_PERIOD_SZ_RADIO_MIN;
	} else {		//I2S Slave Mode
		if ((p->eChannel != 1) && (p->eChannel != 2)
		    && (p->eChannel != 8)) {
			SDR_WARN("eChannel[%d] should be 2 or 8.",
				p->eChannel);
			pr_info("So, it is changed by default[%d]\n",
				I2S_MODE_DEFAULT_CHANNEL);
			p->eChannel = I2S_MODE_DEFAULT_CHANNEL;
			ret = 1;
		}

		if ((p->eBitMode != 16) && (p->eBitMode != 24)) {
			SDR_WARN("eBitMode[%d] is wrong.",
				p->eBitMode);
			pr_info("So, it is changed by default[%d]\n",
				RADIO_MODE_DEFAULT_BITMODE);
			p->eBitMode = I2S_MODE_DEFAULT_BITMODE;
			ret = 1;
		}
		period_min = TCC_SDR_PERIOD_SZ_I2S_MIN;
	}

	if (p->eBufferSize < TCC_SDR_BUFFER_SZ_MIN) {
		SDR_WARN("eBufferSize[0x%08x] is wrong.",
			p->eBufferSize);
		pr_info("So, it is changed by min[0x%08x]\n",
			TCC_SDR_BUFFER_SZ_MIN);
		p->eBufferSize = TCC_SDR_BUFFER_SZ_MIN;
		ret = 1;
	}
#ifdef PREALLOCATE_DMA_BUFFER_MODE
	if (p->eBufferSize > TCC_SDR_BUFFER_SZ_MAX) {
		ret = -EINVAL;
		goto end_set_param;
	}
#endif

	if (p->ePeriodSize < period_min) {
		if (p->eRadioMode) {
			if (p->eBufferSize / RADIO_MODE_DEFAULT_PERIOD_DIV >
			    period_min)
				period_min =
				    p->eBufferSize /
				    RADIO_MODE_DEFAULT_PERIOD_DIV;
		} else {
			if (p->eBufferSize / I2S_MODE_DEFAULT_PERIOD_DIV >
			    period_min)
				period_min =
				    p->eBufferSize /
				    I2S_MODE_DEFAULT_PERIOD_DIV;
		}
		SDR_WARN("ePeriodSize[0x%08x] is wrong.",
			p->ePeriodSize);
		pr_info("So, it is changed by min[0x%08x]\n",
			period_min);
		p->ePeriodSize = period_min;
		ret = 1;
	}

	if (p->ePeriodSize > TCC_SDR_PERIOD_SZ_MAX) {
		ret = -EINVAL;
		goto end_set_param;
	}

	sz_check = 31;
	while (sz_check > 3) {
		if ((0x00000001 << sz_check) & p->eBufferSize)
			break;

		sz_check--;
	};

	if (p->eRadioMode) {
		int i;

		sdr->radio_mode = 1;
		sdr->ports_num = p->eChannel;
		sdr->bitmode = p->eBitMode;
		sdr->bit_polarity = p->eBitPolarity;

		for (i = 0; i < sdr->ports_num; i++)
			sdr->port[i].overrun = 0;
#ifdef RADIO_STOP_MIN_DELAY
		overall_sz = TCC_SDR_PERIOD_SZ_RADIO_MIN;
#else
		overall_sz = 64;
		//2^(16BSIZE+32WSIZE)
#endif
	} else {		//I2S Slave Mode
		sdr->radio_mode = 0;
		sdr->channels = p->eChannel;
		sdr->ports_num = 1;
		sdr->bclk_ratio = DEFAULT_BCLK_RATIO;
		sdr->mclk_div = DEFAULT_MCLK_DIV;
		sdr->bitmode = p->eBitMode;
		sdr->port[0].overrun = 0;
		overall_sz = 32;
		//2^(8BSIZE+32WSIZE)
	}

	if (p->eBufferSize != (1 << sz_check)) {
		SDR_WARN("buffer_bytes[%u] should be 2^N[%u]\n",
		       p->eBufferSize, 1 << (sz_check + 1));
		SDR_WARN("buffer_bytes[%u] change to [%u]\n",
		       p->eBufferSize, 1 << (sz_check + 1));
		p->eBufferSize = 1 << (sz_check + 1);
		sdr->buffer_bytes = 1 << (sz_check + 1);
		ret = 1;
	} else {
		sdr->buffer_bytes = p->eBufferSize;
	}

	sz_check = p->ePeriodSize % overall_sz;
	if (sz_check) {
		sz_check = p->ePeriodSize / overall_sz;
		SDR_WARN("period_bytes[%u] should be multiple of %d[%u]\n",
		       p->ePeriodSize, overall_sz, sz_check * overall_sz);
		SDR_WARN("period_bytes[%u] change to [%u]\n",
		       p->ePeriodSize, sz_check * overall_sz);
		p->ePeriodSize = sz_check * overall_sz;
		sdr->period_bytes = p->ePeriodSize;
		ret = 1;
	} else {
		sdr->period_bytes = p->ePeriodSize;
	}

#ifdef RADIO_STOP_MIN_DELAY
	if (p->eRadioMode) {
		sdr->nperiod = p->ePeriodSize / TCC_SDR_PERIOD_SZ_RADIO_MIN;
		sdr->period_bytes = TCC_SDR_PERIOD_SZ_RADIO_MIN;
	} else {		//I2S Slave Mode
		sdr->nperiod = 1;
	}
#endif

	tcc_sdr_initialize(sdr, GFP_KERNEL);

end_set_param:
	return ret;
}

void tcc_sdr_rx_isr(struct tcc_sdr_t *sdr)
{
	int i;
	bool mono_mode =
		((sdr->radio_mode == 0) && (sdr->channels == 1)) ?
		true :
		false;
	uint32_t cur_period_offset;
	unsigned long flags;
#ifdef OVERRUN_NOTIFY_INTERVAL
	struct timeval pre, next;
	u64 overrun_us64 = 0;
	uint32_t overrun_us = 0;
#endif
	spin_lock_irqsave(&sdr->lock, flags);

	for (i = 0; i < sdr->ports_num; i++) {
		struct tcc_sdr_port_t *port = &sdr->port[i];

		if (mono_mode) {
			cur_period_offset =
			    (uint32_t) (get_current_dma_period_base_addr(sdr, i)
					- sdr->mono_dma_paddr);
		} else {
			cur_period_offset =
			    (uint32_t) (get_current_dma_period_base_addr(sdr, i)
					- port->dma_paddr);
		}

		if (sdr->opened && sdr->started) {
			port->valid_sz += calc_valid_sz_offset(port->write_pos,
				cur_period_offset, port->dma_sz);

#ifdef TCC_SDR_RX_ISR_DEBUG
			RX_ISR_DBG("port(%d) valid_sz:0x%8x, read_pos:0x%8x,",
				i, port->valid_sz, port->read_pos);
			pr_info("write_pos:0x%8x, new_write_pos:0x%8x\n",
				port->write_pos, cur_period_offset);
#endif

			if (port->valid_sz > port->dma_sz) {// Overrun
				port->valid_sz = 0;
				port->read_pos = cur_period_offset;

				port->overrun = true;
			}
		}
		port->write_pos = cur_period_offset;

	}
	spin_unlock_irqrestore(&sdr->lock, flags);

	wake_up_interruptible(&sdr->wq);

	for (i = 0; i < sdr->ports_num; i++) {
		if (sdr->port[i].overrun) {	// Overrun
			if (sdr->opened && sdr->started) {
#ifdef OVERRUN_NOTIFY_INTERVAL
				do_gettimeofday(&next);
				pre = sdr->pre_overrun;
				overrun_us64 =
					timeval_to_ns(&next) -
					timeval_to_ns(&pre);
				do_div(overrun_us64, NSEC_PER_USEC);
				overrun_us = overrun_us64;
				if ((sdr->first_overrun == false) ||
					(overrun_us >=
					(OVERRUN_NOTIFY_INTERVAL*1000))) {
					sdr->first_overrun = true;
				SDR_WARN("[dev-%d] %s - Overrun(%d),",
					sdr->blk_no, __func__, i);
				pr_info("new_read_pos:0x%x, dma_sz:0x%x,",
					sdr->port[i].read_pos,
					sdr->port[i].dma_sz);
				pr_info("valid_sz(%p):0x%x\n",
					&sdr->port[i].valid_sz,
					sdr->port[i].valid_sz);
					sdr->pre_overrun.tv_sec = next.tv_sec;
					sdr->pre_overrun.tv_usec = next.tv_usec;
				}
#else
				SDR_WARN("[dev-%d] %s - Overrun(%d),",
					sdr->blk_no, __func__, i);
				pr_info("new_read_pos:0x%x, dma_sz:0x%x,",
					sdr->port[i].read_pos,
					sdr->port[i].dma_sz);
				pr_info("valid_sz(%p):0x%x\n",
					&sdr->port[i].valid_sz,
					sdr->port[i].valid_sz);
#endif
			}
		}
	}
}

static irqreturn_t tcc_sdr_isr(int irq, void *dev)
{
	struct tcc_sdr_t *sdr = (struct tcc_sdr_t *)dev;
	bool adma_istatus;

	adma_istatus = tcc_adma_dai_rx_irq_check(sdr->adma_reg);

#ifdef RADIO_STOP_MIN_DELAY
	if (adma_istatus == true) {

		sdr->nirq++;
		if (sdr->nirq >= sdr->nperiod) {
			tcc_sdr_rx_isr(sdr);
			sdr->nirq = 0;
		}
	}
#else
	if (adma_istatus == true)
		tcc_sdr_rx_isr(sdr);
#endif

	tcc_adma_dai_rx_irq_clear(sdr->adma_reg);
	adma_istatus = tcc_adma_dai_rx_irq_check(sdr->adma_reg);
	if (adma_istatus == true)
		SDR_ERR("%s : DAI RX IRQ not clear!\n", __func__);


	return IRQ_HANDLED;
}

int tcc_sdr_copy_from_dma(
	struct tcc_sdr_t *sdr,
	uint32_t sdr_port,
	char *buf,
	unsigned int readcnt)
{
	struct tcc_sdr_port_t *port = &sdr->port[sdr_port];
	uint32_t first_sz, second_sz;
	uint32_t read_pos, new_read_pos;
	unsigned long flags = 0;
	int ret = 0;

	if (!sdr->started) {
		SDR_DBG("%s - not started\n", __func__);
		return -EPROTO;
	}

	if (port->overrun) {
		SDR_DBG("dev-%d Overrun detected\n",
		       sdr->blk_no);
		port->overrun = false;
		return -EPIPE;
	}

	ret =
	    wait_event_interruptible_timeout(sdr->wq,
			port->valid_sz >= readcnt,
			msecs_to_jiffies(SDR_READ_TIMEOUT));

	if (ret <= 0) {
		SDR_ERR("%s - dev-%d timeout[%d]sec, ret=[%d].",
		       __func__, sdr->blk_no, SDR_READ_TIMEOUT, ret);
		SDR_ERR("Please check turner status.\n");
		return -EIO;
	}

	spin_lock_irqsave(&sdr->lock, flags);

	if (port->valid_sz < readcnt)
		readcnt = port->valid_sz;

#ifdef TCC_SDR_READ_DEBUG
	READ_DBG("<bf>(%d) base:0x%08x, len:0x%x\n",
	       sdr_port, port->read_pos, readcnt);
#endif

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

#ifdef TCC_SDR_READ_DEBUG
	READ_DBG("<ft>-valid_sz:0x%x, read_pos:0x%x, readcnt:0x%x\n",
	       port->valid_sz, port->read_pos, readcnt);
#endif

	spin_unlock_irqrestore(&sdr->lock, flags);

	//first copy
	if (copy_to_user(buf, port->dma_vaddr + read_pos, first_sz) != 0) {
		pr_err("[%s][%d]dev-%d copy_to_user failed\n", __func__,
		       __LINE__, sdr->blk_no);
		return -EFAULT;
	}
	if (second_sz > 0) {
		//second copy
		if (copy_to_user(buf + first_sz, port->dma_vaddr,
			second_sz) != 0) {
			pr_err("[%s][%d]dev-%d copy_to_user failed\n", __func__,
			       __LINE__, sdr->blk_no);
			return -EFAULT;
		}
	}

	return readcnt;
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

	if ((sdr->dai_pclk) && (__clk_is_enabled(sdr->dai_pclk)))
		clk_disable_unprepare(sdr->dai_pclk);

	if ((sdr->dai_hclk) && (__clk_is_enabled(sdr->dai_hclk)))
		clk_disable_unprepare(sdr->dai_hclk);

	if ((sdr->dai_filter_clk) && (__clk_is_enabled(sdr->dai_filter_clk)))
		clk_disable_unprepare(sdr->dai_filter_clk);

	return 0;
}

int tcc_sdr_start(struct tcc_sdr_t *sdr, gfp_t gfp)
{
	uint32_t i;
#ifdef OVERRUN_NOTIFY_INTERVAL
	struct timeval start;
#endif

	if (sdr->started) {
		SDR_DBG("%s - already started\n",
		       __func__);
		return -EPROTO;
	}
#ifdef RADIO_STOP_MIN_DELAY
	sdr->nirq = 0;
#endif
	for (i = 0; i < sdr->ports_num; i++) {
		sdr->port[i].valid_sz = 0;
		sdr->port[i].read_pos = 0;
		sdr->port[i].write_pos = 0;
		sdr->port[i].overrun = 0;
	}

	tcc_adma_dai_rx_irq_enable(sdr->adma_reg, true);
	tcc_adma_dai_rx_dma_enable(sdr->adma_reg, true);

	tcc_dai_set_rx_mute(sdr->dai_reg, false); //mute disable

#ifdef OVERRUN_NOTIFY_INTERVAL
	sdr->first_overrun = false;
	do_gettimeofday(&start);
	sdr->pre_overrun.tv_sec = start.tv_sec;
	sdr->pre_overrun.tv_usec = start.tv_usec;
#endif

	if (sdr->radio_mode)
		tcc_radio_enable(sdr->dai_reg, true); //radio enable
	else
		tcc_dai_enable(sdr->dai_reg, true); //dai enable

	tcc_dai_rx_enable(sdr->dai_reg, true); //dai rx enable

	//tcc_dai_dump(sdr->dai_reg);
	//tcc_adma_dump(sdr->adma_reg);
	sdr->started = true;

	return 0;
}

int tcc_sdr_stop(struct tcc_sdr_t *sdr)
{
	uint32_t ret = 0;

	if (!sdr->started) {
		SDR_DBG("%s - not started\n", __func__);
		return -EPROTO;
	}

	SDR_DBG("%s\n", __func__);

	tcc_dai_set_rx_mute(sdr->dai_reg, true);

	ret = tcc_adma_dai_rx_auto_dma_disable(sdr->adma_reg);
	if (ret != 0)
		tcc_adma_dai_rx_dma_enable(sdr->adma_reg, false);

	tcc_adma_dai_rx_irq_enable(sdr->adma_reg, false);

	/* HopCount Clear */
	tcc_adma_dai_rx_hopcnt_clear(sdr->adma_reg);

	tcc_adma_dai_rx_reset_enable(sdr->adma_reg, true);

	//FIFO Clear
	tcc_dai_rx_fifo_clear(sdr->dai_reg);

	udelay(1000);

	tcc_dai_rx_enable(sdr->dai_reg, false); //dai rx disable

	tcc_dai_rx_fifo_release(sdr->dai_reg);

	tcc_adma_dai_rx_reset_enable(sdr->adma_reg, false);
	if (sdr->radio_mode)
		tcc_radio_enable(sdr->dai_reg, false); //radio disable
	else
		tcc_dai_enable(sdr->dai_reg, false); //dai disable
	//audio filter disable
	tcc_dai_set_audio_filter_enable(sdr->dai_reg, false);

//      tcc_sdr_deinitialize(sdr);

	sdr->started = false;

	return 0;
}

ssize_t tcc_sdr_read(
	struct file *flip,
	char __user *ibuf,
	size_t count,
	loff_t *f_pos)
{
	return 0;
}

ssize_t tcc_sdr_write(
	struct file *flip,
	const char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	return count;
}

unsigned int tcc_sdr_poll(
	struct file *flip,
	struct poll_table_struct *wait)
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
	int i = 0, ret = -EINVAL;

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
			if (ret < 0) {
				param.eReadCount = ret;
				if (copy_to_user((void *)arg,
					&param, sizeof(param)) != 0)
					SDR_DBG("HSI2S_RADIO_MODE_RX_DAI");
					DBG("Fail!!\n");
			}
		}
		break;
	case HSI2S_I2S_MODE_RX_DAI:
		{
			struct HS_RADIO_RX_PARAM param;

			if (!copy_from_user(&param, (const void *)arg,
			     sizeof(struct HS_RADIO_RX_PARAM))) {
				param.eIndex = 0;
				ret =
					tcc_sdr_copy_from_dma(sdr,
					param.eIndex,
					param.eBuf,
					param.eReadCount);
			} else {
				SDR_DBG("HSI2S_I2S_MODE_RX_DAI Fail!!\n");
			}
			if (ret < 0) {
				param.eReadCount = ret;
				if (copy_to_user
				    ((void *)arg, &param,
				     sizeof(param)) != 0)
					SDR_DBG("HSI2S_I2S_MODE_RX_DAI");
					DBG("Fail!!\n");
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
				ret = -EFAULT;
				goto ioctl_end;
			}

			if (copy_to_user
			    ((void __user *)arg, (void *)&param,
			     sizeof(struct HS_I2S_PARAM)) != 0) {
				SDR_DBG("HSI2S_SET_PARAMS Fail!!\n");
				ret = -EFAULT;
			}
		}
		break;
	case HSI2S_GET_VALID_BYTES:
		{
			uint32_t valid[SDR_MAX_PORT_NUM] = { 0, };

			for (i = 0; i < sdr->ports_num; i++) {
				valid[i] = sdr->port[i].valid_sz;
				//SDR_DBG("[dev-%d][%d] valid=%d\n",
				//sdr->blk_no, i, valid[i]);
			}

			if (copy_to_user
			    ((void __user *)arg, (void *)valid,
			     sizeof(uint32_t) * SDR_MAX_PORT_NUM) != 0) {
				SDR_DBG("HSI2S_GET_VALID_BYTES Fail!!\n");
				ret = -EFAULT;
			} else {
				ret = 0;
			}
		}
		break;
	default:
		SDR_DBG("CMD(0x%x)is not supported\n",
		       cmd);
		ret = 0;
		break;
	}
ioctl_end:
	mutex_unlock(&sdr->m);

	return (long)ret;
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
	tcc_sdr_deinitialize(sdr);

	sdr->opened = false;
	sdr->started = false;

#ifdef HRSTEN_ENABLE
	if (sdr->iocfg_reg != NULL) {
		tcc_audio_sw_reset_enable(sdr->iocfg_reg, sdr->hrsten_reg_offset, sdr->hrsten_bit_offset, true);
		mdelay(5);
		tcc_audio_sw_reset_enable(sdr->iocfg_reg, sdr->hrsten_reg_offset, sdr->hrsten_bit_offset, false);
	}
#endif
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
	sdr->blk_no = of_alias_get_id(pdev->dev.of_node, "i2s");
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

	sdr->dai_filter_clk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(sdr->dai_filter_clk))
		sdr->dai_filter_clk = NULL;

	if (of_property_read_u32
	    (pdev->dev.of_node, "clock-frequency", &sdr->dai_clk_rate[1]) < 0) {
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
			SDR_DBG("adma_reg=%p\n",
			       sdr->adma_reg);

		sdr->adma_irq = of_irq_get(of_node_adma, 0);
#ifdef HRSTEN_ENABLE
		sdr->iocfg_reg = of_iomap(of_node_adma, 1);
		if (IS_ERR((void *)sdr->iocfg_reg)) {
			sdr->iocfg_reg = NULL;
			SDR_DBG("iocfg_reg is NULL\n");
		} else {
			SDR_DBG("iocfg_reg=%p\n", sdr->iocfg_reg);
			sdr->hrsten_reg_offset = 0;
			of_property_read_u32(of_node_adma, "hrsten-reg-offset", &sdr->hrsten_reg_offset);
			if (sdr->hrsten_reg_offset != 0)
				SDR_DBG("hrsten_reg_offset : 0x%08x\n", sdr->hrsten_reg_offset);
			of_property_read_u32(of_node_adma, "hrsten-bit-offset", &sdr->hrsten_bit_offset);
			if (sdr->hrsten_bit_offset != 0)
				SDR_DBG("hrsten_bit_offset : %u\n", sdr->hrsten_bit_offset);
		}
#endif
	} else {
		SDR_DBG("of_node_adma is NULL\n");
		return -EINVAL;
	}

	if (of_property_read_string(pdev->dev.of_node, "dev-name", &devname) ==
	    0) {
		sdr->misc_dev->name = devname;
		SDR_DBG("dev-name : %s\n", devname);
	} else {
		SDR_DBG("default dev-name\n");
	}

	of_property_read_u32(pdev->dev.of_node, "block-type", &sdr->dev_type);
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
		SDR_DBG("%s : Fail to parse sdr dt\n",
		       __func__);
		return ret;
	}
	//setup default cfg value
	sdr->ports_num = RADIO_MODE_DEFAULT_CHANNEL;
	sdr->bitmode = RADIO_MODE_DEFAULT_BITMODE;
	sdr->buffer_bytes = TCC_SDR_BUFFER_SZ_MAX;

	platform_set_drvdata(pdev, sdr);
	sdr->pdev = pdev;

	init_waitqueue_head(&sdr->wq);
	mutex_init(&sdr->m);
	spin_lock_init(&sdr->lock);

#ifdef PREALLOCATE_DMA_BUFFER_MODE
	// plus 1 for align
	sdr->dma_total_size = TCC_SDR_BUFFER_SZ_MAX * (SDR_MAX_PORT_NUM + 1);
	SDR_DBG("dma_total_size : %d\n",
	       sdr->dma_total_size);
	sdr->dma_vaddr_base =
	    dma_alloc_coherent(&sdr->pdev->dev, sdr->dma_total_size,
			       &sdr->dma_paddr_base, GFP_KERNEL);
	SDR_DBG("dma_vaddr_base : %p\n",
	       (void *)sdr->dma_vaddr_base);

	if (sdr->dma_vaddr_base == NULL) {
		SDR_DBG("dma memory allocation failed\n");
		return -ENOMEM;
	}

	sdr->mono_dma_vaddr =
	    dma_alloc_coherent(&sdr->pdev->dev, TCC_SDR_BUFFER_SZ_MAX,
			       &sdr->mono_dma_paddr, GFP_KERNEL);
	SDR_DBG("mono_dma_vaddr : %p\n",
	       (void *)sdr->mono_dma_vaddr);

	if (sdr->mono_dma_vaddr == NULL) {
		SDR_DBG("dma memory allocation failed\n");
		return -ENOMEM;
	}
#endif

	sdr->opened = false;

	if (misc_register(sdr->misc_dev)) {
		SDR_WARN("Couldn't register device .\n");
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
		pr_err("%s : pinctrl suspend error[0x%p]\n",
			__func__, pinctrl);

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
		pr_err("%s : pinctrl resume error[0x%p]\n",
			__func__, pinctrl);

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
