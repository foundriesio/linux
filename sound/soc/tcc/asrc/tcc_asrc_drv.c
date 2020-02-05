/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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
#include <asm/io.h>
#include <asm/div64.h>

#include <linux/timekeeping.h>

#include "tcc_asrc_drv.h"
#include "tcc_asrc_dai.h"
#include "tcc_asrc_pcm.h"
#include "tcc_asrc_m2m.h"
#include "tcc_asrc.h"
#include "tcc_pl080.h"

#define DEFAULT_PERI_DAI_RATE	(48000)
#define DEFAULT_PERI_DAI_FORMAT	(SNDRV_PCM_FORMAT_S16_LE)
#define DEFAULT_PERI_DAI		(TCC_ASRC_PERI_DAI0)

struct tcc_asrc_t* tcc_asrc_get_handle_by_node(struct device_node *np)
{
	struct platform_device *pdev;

	if ((pdev = of_find_device_by_node(np)) == NULL) {
		return NULL;
	}

	return platform_get_drvdata(pdev);
}

uint32_t tcc_asrc_txbuf_lli_phys_address(struct tcc_asrc_t *asrc, uint32_t asrc_pair, int idx)
{
	return (uint32_t)(asrc->pair[asrc_pair].txbuf.lli_phys + (idx * sizeof(struct pl080_lli)));
}

uint32_t tcc_asrc_rxbuf_lli_phys_address(struct tcc_asrc_t *asrc, uint32_t asrc_pair, int idx)
{
	return (uint32_t)(asrc->pair[asrc_pair].rxbuf.lli_phys + (idx * sizeof(struct pl080_lli)));
}

int tcc_asrc_volume_gain(struct tcc_asrc_t *asrc, uint32_t asrc_pair)
{
	tcc_asrc_set_volume_gain(asrc->asrc_reg, asrc_pair, asrc->pair[asrc_pair].volume_gain);
	tcc_asrc_volume_enable(asrc->asrc_reg, asrc_pair, 1);

	return 0;
}

int tcc_asrc_volume_ramp(struct tcc_asrc_t *asrc, uint32_t asrc_pair)
{
	void __iomem *asrc_reg = asrc->asrc_reg;
	uint32_t gain = asrc->pair[asrc_pair].volume_ramp.gain; 
	uint32_t dn_time = asrc->pair[asrc_pair].volume_ramp.dn_time;
	uint32_t dn_wait = asrc->pair[asrc_pair].volume_ramp.dn_wait;
	uint32_t up_time = asrc->pair[asrc_pair].volume_ramp.up_time;
	uint32_t up_wait = asrc->pair[asrc_pair].volume_ramp.up_wait;

	tcc_asrc_set_volume_ramp_dn_time(asrc_reg, asrc_pair, dn_time, dn_wait);
	tcc_asrc_set_volume_ramp_up_time(asrc_reg, asrc_pair, up_time, up_wait);
	tcc_asrc_set_volume_ramp_gain(asrc_reg, asrc_pair, gain); // 0 dB
	tcc_asrc_volume_ramp_enable(asrc_reg, asrc_pair, 1);

	return 0;
}

int tcc_asrc_tx_dma_start(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;

	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_set_first_lli(asrc->pl080_reg, dma_tx_ch, &asrc->pair[asrc_pair].txbuf.lli_virt[0]);
	tcc_pl080_set_channel_mem2per(asrc->pl080_reg, dma_tx_ch, dma_tx_ch, 1, 1);
	tcc_pl080_channel_enable(asrc->pl080_reg, dma_tx_ch, 1);
	tcc_pl080_channel_sync_mode(asrc->pl080_reg, dma_tx_ch, 1);

	return 0;
}

int tcc_asrc_tx_dma_stop(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;

	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_channel_enable(asrc->pl080_reg, dma_tx_ch, 0);

	return 0;
}

int tcc_asrc_tx_dma_halt(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;

	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_halt_enable(asrc->pl080_reg, dma_tx_ch, 1);

	return 0;
}

int tcc_asrc_tx_fifo_enable(struct tcc_asrc_t *asrc, int asrc_pair, int enable)
{
	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_asrc_fifo_in_dma_en(asrc->asrc_reg, asrc_pair, enable);

	return 0;
}

int tcc_asrc_rx_dma_start(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_rx_ch = asrc_pair + ASRC_RX_DMA_OFFSET;

	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_set_first_lli(asrc->pl080_reg, dma_rx_ch, &asrc->pair[asrc_pair].rxbuf.lli_virt[0]);
	tcc_pl080_set_channel_per2mem(asrc->pl080_reg, dma_rx_ch, dma_rx_ch, 1, 1);
	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, 1);
	tcc_pl080_channel_sync_mode(asrc->pl080_reg, dma_rx_ch, 1);

	return 0;
}

int tcc_asrc_rx_dma_stop(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_rx_ch = asrc_pair + ASRC_RX_DMA_OFFSET;

	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, 0);

	return 0;
}

int tcc_asrc_rx_dma_halt(struct tcc_asrc_t *asrc, int asrc_pair)
{
	uint32_t dma_rx_ch = asrc_pair + ASRC_RX_DMA_OFFSET;

	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_halt_enable(asrc->pl080_reg, dma_rx_ch, 1);

	return 0;
}

int tcc_asrc_rx_fifo_enable(struct tcc_asrc_t *asrc, int asrc_pair, int enable)
{
	if (asrc_pair > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_asrc_fifo_out_dma_en(asrc->asrc_reg, asrc_pair, enable);

	return 0;
}


// M2M SYNC
static void __tcc_asrc_m2m_sync_setup(void __iomem *asrc_reg,
		int asrc_pair, 
		enum tcc_asrc_fifo_fmt_t tx_fmt, 
		enum tcc_asrc_fifo_mode_t tx_mode,
		enum tcc_asrc_fifo_fmt_t rx_fmt, 
		enum tcc_asrc_fifo_mode_t rx_mode,
		uint32_t ratio_shift22)
{
	enum tcc_asrc_component_t asrc_comp;

	asrc_comp = (asrc_pair == 0) ? TCC_ASRC0 :
		(asrc_pair == 1) ? TCC_ASRC1 :
		(asrc_pair == 2) ? TCC_ASRC2 : TCC_ASRC3;

	tcc_asrc_set_inport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);
	tcc_asrc_set_outport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_pair, ratio_shift22); 
	tcc_asrc_set_ratio(asrc_reg, asrc_pair, TCC_ASRC_MODE_SYNC, ratio_shift22);
	tcc_asrc_component_reset(asrc_reg, asrc_comp);

	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_pair, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_pair, 0x1f);

	tcc_asrc_set_inport_timing(asrc_reg, asrc_pair, IP_OP_TIMING_ASRC_REQUEST);
	tcc_asrc_set_outport_timing(asrc_reg, asrc_pair, IP_OP_TIMING_ASRC_REQUEST);

	tcc_asrc_fifo_in_config(asrc_reg, asrc_pair, tx_fmt, tx_mode, 0);
	tcc_asrc_fifo_out_config(asrc_reg, asrc_pair, rx_fmt, rx_mode, 0);

	tcc_asrc_component_enable(asrc_reg, asrc_comp, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_INPORT, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_OUTPORT, 1);

	tcc_asrc_fifo_in_dma_en(asrc_reg, asrc_pair, 1);
	tcc_asrc_fifo_out_dma_en(asrc_reg, asrc_pair, 1);
}

int tcc_asrc_m2m_sync_setup(struct tcc_asrc_t *asrc, 
		int asrc_pair, 
		enum tcc_asrc_fifo_fmt_t tx_fmt, 
		enum tcc_asrc_fifo_mode_t tx_mode,
		enum tcc_asrc_fifo_fmt_t rx_fmt, 
		enum tcc_asrc_fifo_mode_t rx_mode,
		uint32_t ratio_shift22)
{
	uint32_t dma_rx_ch = asrc_pair + ASRC_RX_DMA_OFFSET;

	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, 0); //disable rx dma channel

	__tcc_asrc_m2m_sync_setup(asrc->asrc_reg, asrc_pair, tx_fmt, tx_mode, rx_fmt, rx_mode, ratio_shift22);

	tcc_asrc_volume_ramp(asrc, asrc_pair);
	tcc_asrc_volume_gain(asrc, asrc_pair);

	return 0;
}

static int tcc_asrc_check_supported_channel(struct tcc_asrc_t *asrc, int asrc_pair, enum tcc_asrc_drv_ch_t channels)
{
	enum tcc_asrc_drv_ch_t max_channel;

	max_channel = (asrc->pair[asrc_pair].hw.max_channel == 2) ? TCC_ASRC_NUM_OF_CH_2 : 
		(asrc->pair[asrc_pair].hw.max_channel == 4) ? TCC_ASRC_NUM_OF_CH_4 : 
		(asrc->pair[asrc_pair].hw.max_channel == 6) ? TCC_ASRC_NUM_OF_CH_6 : TCC_ASRC_NUM_OF_CH_8;

	if (max_channel < channels) 
		return -1;

	return 0;
}

// M2P 
static void __tcc_asrc_m2p_setup(void __iomem *asrc_reg,
		int asrc_pair, 
		enum tcc_asrc_drv_sync_mode_t sync_mode,
		enum tcc_asrc_drv_bitwidth_t bitwidth, 
		enum tcc_asrc_drv_ch_t channels,
		enum tcc_asrc_peri_t peri_target,
		enum tcc_asrc_async_refclk_t refclk,
		uint32_t ratio_shift22)
{
	enum tcc_asrc_mode_t asrc_mode;
	enum tcc_asrc_component_t asrc_comp;
//	enum tcc_asrc_op_route_t op_route;
	enum tcc_asrc_clksel_t op_clksel, aux_sel, ip_clksel;
	enum tcc_asrc_fifo_fmt_t fifo_fmt; 
	enum tcc_asrc_fifo_mode_t fifo_mode;

	asrc_mode = (sync_mode == TCC_ASRC_ASYNC_MODE) ? TCC_ASRC_MODE_ASYNC : TCC_ASRC_MODE_SYNC;

	asrc_comp = (asrc_pair == 0) ? TCC_ASRC0 :
				(asrc_pair == 1) ? TCC_ASRC1 :
				(asrc_pair == 2) ? TCC_ASRC2 : TCC_ASRC3;

	aux_sel = (asrc_pair == 0) ? TCC_ASRC_CLKSEL_AUXCLK0 :
			  (asrc_pair == 1) ? TCC_ASRC_CLKSEL_AUXCLK1 :
			  (asrc_pair == 2) ? TCC_ASRC_CLKSEL_AUXCLK2 : TCC_ASRC_CLKSEL_AUXCLK3;

	ip_clksel =	(refclk == TCC_ASRC_ASYNC_REFCLK_DAI0) ? TCC_ASRC_CLKSEL_MCAUDIO0_LRCK :
				(refclk == TCC_ASRC_ASYNC_REFCLK_DAI1) ? TCC_ASRC_CLKSEL_MCAUDIO1_LRCK :
				(refclk == TCC_ASRC_ASYNC_REFCLK_DAI2) ? TCC_ASRC_CLKSEL_MCAUDIO2_LRCK :
				(refclk == TCC_ASRC_ASYNC_REFCLK_DAI3) ? TCC_ASRC_CLKSEL_MCAUDIO3_LRCK : aux_sel;

	//op_route = (asrc_pair == 0) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR0_10:
	//		   (asrc_pair == 1) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR1 :
	//		   (asrc_pair == 2) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR2 : TCC_ASRC_OP_ROUTE_ASRC_PAIR3;

	op_clksel = (peri_target == TCC_ASRC_PERI_DAI0) ? TCC_ASRC_CLKSEL_MCAUDIO0_LRCK :
				(peri_target == TCC_ASRC_PERI_DAI1) ? TCC_ASRC_CLKSEL_MCAUDIO1_LRCK :
				(peri_target == TCC_ASRC_PERI_DAI2) ? TCC_ASRC_CLKSEL_MCAUDIO2_LRCK : TCC_ASRC_CLKSEL_MCAUDIO3_LRCK;

	fifo_fmt = (bitwidth == TCC_ASRC_16BIT) ? TCC_ASRC_FIFO_FMT_16BIT : TCC_ASRC_FIFO_FMT_24BIT;

	fifo_mode = (channels == TCC_ASRC_NUM_OF_CH_8) ? TCC_ASRC_FIFO_MODE_8CH:
				(channels == TCC_ASRC_NUM_OF_CH_6) ? TCC_ASRC_FIFO_MODE_6CH :
				(channels == TCC_ASRC_NUM_OF_CH_4) ? TCC_ASRC_FIFO_MODE_4CH : TCC_ASRC_FIFO_MODE_2CH;

	tcc_asrc_set_inport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);
	tcc_asrc_set_outport_path(asrc_reg, peri_target, TCC_ASRC_PATH_EXTIO);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_pair, ratio_shift22); 
	tcc_asrc_set_ratio(asrc_reg, asrc_pair, asrc_mode, ratio_shift22);
	tcc_asrc_component_reset(asrc_reg, asrc_comp);
	
	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_pair, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_pair, 0x1f);

	tcc_asrc_set_inport_timing(asrc_reg, asrc_pair, IP_OP_TIMING_ASRC_REQUEST);
	tcc_asrc_set_outport_timing(asrc_reg, asrc_pair, IP_OP_TIMING_EXTERNEL_CLK);

	if (sync_mode == TCC_ASRC_ASYNC_MODE) {
		tcc_asrc_set_inport_clksel(asrc_reg, asrc_pair, ip_clksel);
	}

	tcc_asrc_set_outport_clksel(asrc_reg, asrc_pair, op_clksel);
	//tcc_asrc_set_outport_route(asrc_reg, peri_target, op_route);

	tcc_asrc_fifo_in_config(asrc_reg, asrc_pair, fifo_fmt, fifo_mode, 0);

	tcc_asrc_set_outport_format(asrc_reg, peri_target, TCC_ASRC_FMT_NO_CHANGE, 0);

	tcc_asrc_component_enable(asrc_reg, asrc_comp, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_INPORT, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_OUTPORT, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_EXTIO, 1);
}

int tcc_asrc_m2p_setup(struct tcc_asrc_t *asrc, 
		int asrc_pair, 
		enum tcc_asrc_drv_sync_mode_t sync_mode,
		enum tcc_asrc_drv_bitwidth_t bitwidth, 
		enum tcc_asrc_drv_ch_t channels,
		enum tcc_asrc_peri_t peri_target,
		enum tcc_asrc_async_refclk_t refclk,
		uint32_t src_rate,
		uint32_t dst_rate)
{
	uint64_t ratio_shift22 = (1<<22);

	if (tcc_asrc_check_supported_channel(asrc, asrc_pair, channels) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] %s - asrc_pair %d supports max channel(%d)\n", __func__, asrc_pair, (int)asrc->pair[asrc_pair].hw.max_channel);
		return -1;
	}

	ratio_shift22 = ((uint64_t)0x400000 * dst_rate);
	do_div(ratio_shift22, src_rate);

	if ((sync_mode == TCC_ASRC_ASYNC_MODE) && (refclk == TCC_ASRC_ASYNC_REFCLK_AUX)) { //set aux pclk
		clk_set_rate(asrc->aux_pclk[asrc_pair], src_rate);
		clk_prepare_enable(asrc->aux_pclk[asrc_pair]);
	}

	__tcc_asrc_m2p_setup(asrc->asrc_reg, asrc_pair, sync_mode, bitwidth, channels, peri_target, refclk, (uint32_t)ratio_shift22); 

	tcc_asrc_volume_ramp(asrc, asrc_pair);
	tcc_asrc_volume_gain(asrc, asrc_pair);

	return 0;
}

int tcc_asrc_set_m2p_mux_select(struct tcc_asrc_t *asrc, int peri_target, int asrc_pair)
{
	enum tcc_asrc_op_route_t op_route;

	if (asrc_pair >= NUM_OF_ASRC_PAIR)
		return -EINVAL;

	if (peri_target >= NUM_OF_ASRC_MCAUDIO)
		return -EINVAL;

	asrc->mcaudio_m2p_mux[peri_target] = asrc_pair;
	op_route = (asrc_pair == 0) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR0_10 :
			   (asrc_pair == 1) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR1 :
			   (asrc_pair == 2) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR2 : TCC_ASRC_OP_ROUTE_ASRC_PAIR3;

	tcc_asrc_set_outport_route(asrc->asrc_reg, peri_target, op_route);

	return 0;
}

// P2M
static void __tcc_asrc_p2m_setup(void __iomem *asrc_reg, 
		int asrc_pair, 
		enum tcc_asrc_drv_sync_mode_t sync_mode,
		enum tcc_asrc_drv_bitwidth_t bitwidth, 
		enum tcc_asrc_drv_ch_t channels,
		enum tcc_asrc_peri_t peri_src,
		enum tcc_asrc_drv_bitwidth_t peri_bitwidth, 
		enum tcc_asrc_async_refclk_t refclk,
		uint32_t ratio_shift22)
{
	enum tcc_asrc_mode_t asrc_mode;
	enum tcc_asrc_component_t asrc_comp;
	enum tcc_asrc_ip_route_t ip_route;
	enum tcc_asrc_clksel_t aux_sel, ip_clksel, op_clksel;
	enum tcc_asrc_fifo_fmt_t fifo_fmt; 
	enum tcc_asrc_fifo_mode_t fifo_mode;

	asrc_mode = (sync_mode == TCC_ASRC_ASYNC_MODE) ? TCC_ASRC_MODE_ASYNC : TCC_ASRC_MODE_SYNC;

	asrc_comp = (asrc_pair == 0) ? TCC_ASRC0 :
		(asrc_pair == 1) ? TCC_ASRC1 :
		(asrc_pair == 2) ? TCC_ASRC2 : TCC_ASRC3;

	aux_sel = (asrc_pair == 0) ? TCC_ASRC_CLKSEL_AUXCLK0 :
		(asrc_pair == 1) ? TCC_ASRC_CLKSEL_AUXCLK1 :
		(asrc_pair == 2) ? TCC_ASRC_CLKSEL_AUXCLK2 : TCC_ASRC_CLKSEL_AUXCLK3;

	op_clksel =	(refclk == TCC_ASRC_ASYNC_REFCLK_DAI0) ? TCC_ASRC_CLKSEL_MCAUDIO0_LRCK :
				(refclk == TCC_ASRC_ASYNC_REFCLK_DAI1) ? TCC_ASRC_CLKSEL_MCAUDIO1_LRCK :
				(refclk == TCC_ASRC_ASYNC_REFCLK_DAI2) ? TCC_ASRC_CLKSEL_MCAUDIO2_LRCK :
				(refclk == TCC_ASRC_ASYNC_REFCLK_DAI3) ? TCC_ASRC_CLKSEL_MCAUDIO3_LRCK : aux_sel;

	ip_route =  (peri_src == TCC_ASRC_PERI_DAI0) ? TCC_ASRC_IP_ROUTE_MCAUDIO0_10 :
				(peri_src == TCC_ASRC_PERI_DAI1) ? TCC_ASRC_IP_ROUTE_MCAUDIO1 :
				(peri_src == TCC_ASRC_PERI_DAI2) ? TCC_ASRC_IP_ROUTE_MCAUDIO2 : TCC_ASRC_IP_ROUTE_MCAUDIO3;


	ip_clksel = (peri_src == TCC_ASRC_PERI_DAI0) ? TCC_ASRC_CLKSEL_MCAUDIO0_LRCK :
				(peri_src == TCC_ASRC_PERI_DAI1) ? TCC_ASRC_CLKSEL_MCAUDIO1_LRCK :
				(peri_src == TCC_ASRC_PERI_DAI2) ? TCC_ASRC_CLKSEL_MCAUDIO2_LRCK : TCC_ASRC_CLKSEL_MCAUDIO3_LRCK;

	fifo_fmt = (bitwidth == TCC_ASRC_16BIT) ? TCC_ASRC_FIFO_FMT_16BIT : TCC_ASRC_FIFO_FMT_24BIT;

	fifo_mode = (channels == TCC_ASRC_NUM_OF_CH_8) ? TCC_ASRC_FIFO_MODE_8CH:
				(channels == TCC_ASRC_NUM_OF_CH_6) ? TCC_ASRC_FIFO_MODE_6CH :
				(channels == TCC_ASRC_NUM_OF_CH_4) ? TCC_ASRC_FIFO_MODE_4CH : TCC_ASRC_FIFO_MODE_2CH;

	tcc_asrc_set_inport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_EXTIO);
	tcc_asrc_set_outport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_pair, ratio_shift22); 
	tcc_asrc_set_ratio(asrc_reg, asrc_pair, asrc_mode, ratio_shift22);
	tcc_asrc_component_reset(asrc_reg, asrc_comp);

	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_pair, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_pair, 0x1f);

	tcc_asrc_set_inport_timing(asrc_reg, asrc_pair, IP_OP_TIMING_EXTERNEL_CLK);
	tcc_asrc_set_outport_timing(asrc_reg, asrc_pair, IP_OP_TIMING_ASRC_REQUEST);

	if (sync_mode == TCC_ASRC_ASYNC_MODE) {
		tcc_asrc_set_outport_clksel(asrc_reg, asrc_pair, op_clksel);
	}

	tcc_asrc_set_inport_clksel(asrc_reg, asrc_pair, ip_clksel);
	tcc_asrc_set_inport_route(asrc_reg, asrc_pair, ip_route);

	tcc_asrc_fifo_out_config(asrc_reg, asrc_pair, fifo_fmt, fifo_mode, 0);

	if (peri_bitwidth == TCC_ASRC_16BIT) {
		tcc_asrc_set_inport_format(asrc_reg, peri_src, TCC_ASRC_16BIT_LEFT_8BIT, 1);
	} else {
		tcc_asrc_set_inport_format(asrc_reg, peri_src, TCC_ASRC_FMT_NO_CHANGE, 1);
	}

	tcc_asrc_component_enable(asrc_reg, asrc_comp, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_INPORT, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_OUTPORT, 1);
	tcc_asrc_component_enable(asrc_reg, TCC_EXTIO, 1);
}

int tcc_asrc_p2m_setup(struct tcc_asrc_t *asrc, 
		int asrc_pair, 
		enum tcc_asrc_drv_sync_mode_t sync_mode,
		enum tcc_asrc_drv_bitwidth_t bitwidth, 
		enum tcc_asrc_drv_ch_t channels,
		enum tcc_asrc_peri_t peri_src,
		enum tcc_asrc_drv_bitwidth_t peri_bitwidth, 
		enum tcc_asrc_async_refclk_t refclk,
		uint32_t src_rate,
		uint32_t dst_rate)
{
	uint64_t ratio_shift22 = (1<<22);

	if (tcc_asrc_check_supported_channel(asrc, asrc_pair, channels) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] %s - asrc_pair %d supports max channel(%d)\n", __func__, asrc_pair, (int)asrc->pair[asrc_pair].hw.max_channel);
		return -1;
	}

	ratio_shift22 = ((uint64_t)0x400000 * dst_rate);
	do_div(ratio_shift22, src_rate);

	if ((sync_mode == TCC_ASRC_ASYNC_MODE) && (refclk == TCC_ASRC_ASYNC_REFCLK_AUX)) { //set aux pclk
		clk_set_rate(asrc->aux_pclk[asrc_pair], dst_rate);
		clk_prepare_enable(asrc->aux_pclk[asrc_pair]);
	}

	__tcc_asrc_p2m_setup(asrc->asrc_reg, asrc_pair, sync_mode, bitwidth, channels, peri_src, peri_bitwidth, refclk, (uint32_t)ratio_shift22);

	tcc_asrc_volume_ramp(asrc, asrc_pair);
	tcc_asrc_volume_gain(asrc, asrc_pair);

	return 0;
}

int tcc_asrc_stop(struct tcc_asrc_t *asrc, int asrc_pair)
{
	enum tcc_asrc_component_t asrc_comp;

	asrc_comp = (asrc_pair == 0) ? TCC_ASRC0 :
		(asrc_pair == 1) ? TCC_ASRC1 :
		(asrc_pair == 2) ? TCC_ASRC2 : TCC_ASRC3;

	tcc_asrc_rx_dma_stop(asrc, asrc_pair);
	tcc_asrc_tx_dma_stop(asrc, asrc_pair);

	tcc_asrc_fifo_in_dma_en(asrc->asrc_reg, asrc_pair, 0);
	tcc_asrc_fifo_out_dma_en(asrc->asrc_reg, asrc_pair, 0);

	tcc_asrc_component_enable(asrc->asrc_reg, asrc_comp, 0);
	tcc_asrc_component_reset(asrc->asrc_reg, asrc_comp);

	return 0;
}

static irqreturn_t tcc_pl080_isr(int irq, void *dev)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)dev;
	uint32_t int_status = readl(asrc->pl080_reg+PL080_INT_STATUS);
	int i;

	printk(KERN_DEBUG "[DEBUG][ASRC_DRV] %s(0x%08x)\n", __func__, int_status);
	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		if (int_status & (1<<i)) {
			writel(1<<i, asrc->pl080_reg+PL080_TC_CLEAR);
			writel(1<<i, asrc->pl080_reg+PL080_ERR_CLEAR);

			if (asrc->pair[i].hw.path == TCC_ASRC_M2M_PATH) {
				tcc_pl080_asrc_m2m_txisr_ch(asrc, i);
			} else if (asrc->pair[i].hw.path == TCC_ASRC_M2P_PATH) {
				tcc_pl080_asrc_pcm_isr_ch(asrc, i);
			}
		}
	}

	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		if (int_status & (1<<(i+NUM_OF_ASRC_PAIR))) {
			writel(1<<(i+NUM_OF_ASRC_PAIR), asrc->pl080_reg+PL080_TC_CLEAR);
			writel(1<<(i+NUM_OF_ASRC_PAIR), asrc->pl080_reg+PL080_ERR_CLEAR);

			if ((asrc->pair[i].hw.path == TCC_ASRC_P2M_PATH)) {
				tcc_pl080_asrc_pcm_isr_ch(asrc, i);
			}
		}
	}


	return IRQ_HANDLED;
}

static int parse_asrc_dt(struct platform_device *pdev, struct tcc_asrc_t *asrc)
{
	struct device_node *of_node_asrc = pdev->dev.of_node;
	struct device_node *of_node_dma = NULL;
	struct platform_device *pdev_asrc = NULL;
	struct platform_device *pdev_dma = NULL;
	uint32_t max_channel[NUM_OF_ASRC_PAIR];
	uint32_t path_type[NUM_OF_ASRC_PAIR];
	uint32_t sync_mode[NUM_OF_ASRC_PAIR];
	uint32_t async_refclk[NUM_OF_ASRC_PAIR];
	struct resource res;
	int i;

	printk(KERN_DEBUG "[DEBUG][ASRC_DRV] %s\n", __func__);

	if ((of_node_dma = of_parse_phandle(pdev->dev.of_node, "dma", 0)) == NULL) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] of_node_dma is NULL\n");
		return -EINVAL;
	}

	pdev_asrc = of_find_device_by_node(of_node_asrc);
	pdev_dma = of_find_device_by_node(of_node_dma);

	if ((asrc->asrc_reg = of_iomap(of_node_asrc, 0)) == NULL) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] asrc_reg is NULL\n");
		return -EINVAL;
	}

	if (of_address_to_resource(of_node_asrc, 0, &res) < 0 ) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] asrc_reg_phys is error\n");
		return -EINVAL;
	}
	asrc->asrc_reg_phys = res.start;

	if ((asrc->pl080_reg = of_iomap(of_node_dma, 0)) == NULL) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] pl080_reg is NULL\n");
		return -EINVAL;
	}

	for (i=0; i<NUM_OF_AUX_PERI_CLKS; i++) {
		if (IS_ERR(asrc->aux_pclk[i]=of_clk_get(of_node_asrc, i))) {
			printk(KERN_ERR "[ERROR][ASRC_DRV] aux%d_pclk is NULL\n", i);
			return PTR_ERR(asrc->aux_pclk[i]);
		}
	}

    if (IS_ERR(asrc->asrc_hclk=of_clk_get_by_name(of_node_asrc, "asrc_hclk"))) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] asrc_hclk is invalid\n");
		return PTR_ERR(asrc->asrc_hclk);
	}

    if (IS_ERR(asrc->pl080_hclk = of_clk_get_by_name(of_node_dma, "pl080_hclk"))) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] asrc_hclk is invalid\n");
		return PTR_ERR(asrc->pl080_hclk);
	}

	if ((asrc->asrc_irq = platform_get_irq(pdev_asrc, 0)) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] asrc_irq is invalid\n");
		return asrc->asrc_irq;
	}
	if ((asrc->pl080_irq = platform_get_irq(pdev_dma, 0)) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] asrc_irq is invalid\n");
		return asrc->asrc_irq;
	}

	of_property_read_u32_array(of_node_asrc, "max-ch-per-pair", max_channel, NUM_OF_ASRC_PAIR);
	of_property_read_u32_array(of_node_asrc, "path-type", path_type, NUM_OF_ASRC_PAIR);
	of_property_read_u32_array(of_node_asrc, "sync-mode", sync_mode, NUM_OF_ASRC_PAIR);
	of_property_read_u32_array(of_node_asrc, "async-refclk", async_refclk, NUM_OF_ASRC_PAIR);

	for(i=0; i<NUM_OF_ASRC_PAIR; i++) {
		asrc->pair[i].hw.path = (path_type[i]  == 2) ? TCC_ASRC_P2M_PATH :
								(path_type[i]  == 1) ? TCC_ASRC_M2P_PATH : TCC_ASRC_M2M_PATH;
		asrc->pair[i].hw.max_channel = max_channel[i];
		asrc->pair[i].hw.sync_mode = (sync_mode[i] == 1) ? TCC_ASRC_SYNC_MODE : TCC_ASRC_ASYNC_MODE;
		asrc->pair[i].hw.async_refclk = (async_refclk[i] == 0) ? TCC_ASRC_ASYNC_REFCLK_DAI0 :
										(async_refclk[i] == 1) ? TCC_ASRC_ASYNC_REFCLK_DAI1 :
										(async_refclk[i] == 2) ? TCC_ASRC_ASYNC_REFCLK_DAI2 :
										(async_refclk[i] == 3) ? TCC_ASRC_ASYNC_REFCLK_DAI3 : TCC_ASRC_ASYNC_REFCLK_AUX;
		asrc->pair[i].hw.peri_dai = TCC_ASRC_PERI_DAI0;
		asrc->pair[i].hw.peri_dai_rate = DEFAULT_PERI_DAI_RATE;
		asrc->pair[i].hw.peri_dai_format = DEFAULT_PERI_DAI_FORMAT;
	}

	for(i=0; i<NUM_OF_ASRC_PAIR; i++) {
		asrc->pair[i].volume_ramp.gain = DEFAULT_VOLUME_RAMP_GAIN;;
		asrc->pair[i].volume_ramp.dn_time = DEFAULT_VOLUME_RAMP_TIME;;
		asrc->pair[i].volume_ramp.dn_wait = DEFAULT_VOLUME_RAMP_WAIT;;
		asrc->pair[i].volume_ramp.up_time = DEFAULT_VOLUME_RAMP_TIME;;
		asrc->pair[i].volume_ramp.up_wait = DEFAULT_VOLUME_RAMP_WAIT;;

		asrc->pair[i].volume_gain = DEFAULT_VOLUME_GAIN;;
	}

	for(i=0; i<NUM_OF_ASRC_MCAUDIO; i++) {
		asrc->mcaudio_m2p_mux[i] = -1;
	}
	printk(KERN_DEBUG "[DEBUG][ASRC_DRV] asrc_reg : %p\n", asrc->asrc_reg);
	printk(KERN_DEBUG "[DEBUG][ASRC_DRV] asrc_irq: %d\n", asrc->asrc_irq);
	printk(KERN_DEBUG "[DEBUG][ASRC_DRV] pl080_reg : %p\n", asrc->pl080_reg);
	printk(KERN_DEBUG "[DEBUG][ASRC_DRV] pl080_irq : %d\n", asrc->pl080_irq);

	for(i=0; i<NUM_OF_ASRC_PAIR; i++) {
		printk(KERN_DEBUG "[DEBUG][ASRC_DRV] max-ch-per-pair(%d) : %d\n", i, max_channel[i]);
		printk(KERN_DEBUG "[DEBUG][ASRC_DRV] path_type(%d) : %d\n", i, path_type[i]);
		printk(KERN_DEBUG "[DEBUG][ASRC_DRV] sync-mode(%d) : %d\n", i, sync_mode[i]);
		printk(KERN_DEBUG "[DEBUG][ASRC_DRV] async-refclk(%d) : %d\n", i, async_refclk[i]);
	}

	return 0;
}

static int tcc_asrc_init(struct tcc_asrc_t *asrc)
{
	clk_prepare_enable(asrc->pl080_hclk);
	clk_prepare_enable(asrc->asrc_hclk);

	tcc_asrc_reset(asrc->asrc_reg);
	tcc_asrc_dma_arbitration(asrc->asrc_reg, 0);

	tcc_pl080_enable(asrc->pl080_reg, 1);

	tcc_pl080_clear_int(asrc->pl080_reg, 0xff);
	tcc_pl080_clear_err(asrc->pl080_reg, 0xff);

	return 0;
}

static int tcc_asrc_deinit(struct tcc_asrc_t *asrc)
{
	clk_disable_unprepare(asrc->pl080_hclk);
	clk_disable_unprepare(asrc->asrc_hclk);

	return 0;
}

#ifdef CONFIG_ARCH_TCC802X
void check_tcc802x_rev_xx(struct tcc_asrc_t *asrc)
{
#define CHIP_ID_ADDR0 0xE0003C10
#define CHIP_ID_ADDR1 0xF400001C

#define CHIP_ID_REV_XX_ADDR	(0x16042200)

	volatile uint32_t *p0 = ioremap_nocache(CHIP_ID_ADDR0, 4);
	volatile uint32_t *p1 = ioremap_nocache(CHIP_ID_ADDR1, 4);
	uint32_t val;

	val = (*p0 >> 8) & 0x0f;

	if ((val == 0)  && (*p1 == CHIP_ID_REV_XX_ADDR)) {
		asrc->chip_rev_xx = true;
	} else {
		asrc->chip_rev_xx = false;
	}

	iounmap(p0);
	iounmap(p1);
}
#endif

static int tcc_asrc_probe(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = kzalloc(sizeof(struct tcc_asrc_t), GFP_KERNEL);
	int ret = 0;

	if (asrc == NULL) {
		printk(KERN_WARNING "[WARN][ASRC_DRV] %s - kzalloc failed.\n", __func__);
		return -ENOMEM;
	}

	if ((ret = parse_asrc_dt(pdev, asrc)) < 0) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] %s : Fail to parse asrc dt\n", __func__);
		goto error;
	}

	platform_set_drvdata(pdev, asrc);
	asrc->pdev = pdev;

#ifdef CONFIG_ARCH_TCC802X
	check_tcc802x_rev_xx(asrc);
#endif

	tcc_asrc_init(asrc);

	tcc_asrc_m2m_drvinit(pdev);
	tcc_asrc_dai_drvinit(pdev);
	tcc_asrc_pcm_drvinit(pdev);

	ret = request_irq(asrc->pl080_irq, tcc_pl080_isr, IRQF_TRIGGER_HIGH, "tcc-asrc-pl080", (void*)asrc);
	if (ret < 0) {
		printk(KERN_ERR "[ERROR][ASRC_DRV] pl080 request_irq(%d) failed\n", asrc->pl080_irq);
		goto error;
	}

	return ret;

error:
	kfree(asrc);
	return ret;
}

static int tcc_asrc_remove(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	free_irq(asrc->pl080_irq, (void*)asrc);
		
	tcc_asrc_deinit(asrc);

	return 0;
}

static int tcc_asrc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	tcc_asrc_reg_backup(asrc->asrc_reg, &asrc->asrc_regs_backup);

	tcc_asrc_deinit(asrc);

	return 0;
}

static int tcc_asrc_resume(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);
	int i;

	tcc_asrc_init(asrc);

	tcc_asrc_reg_restore(asrc->asrc_reg, &asrc->asrc_regs_backup);

	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		tcc_asrc_volume_enable(asrc->asrc_reg, i, 1);
		tcc_asrc_volume_ramp_enable(asrc->asrc_reg, i, 1);
	}

	return 0;
}

static struct of_device_id tcc_asrc_of_match[] = {
	{ .compatible = "telechips,asrc" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_asrc_of_match);

static struct platform_driver tcc_asrc_driver = {
	.probe		= tcc_asrc_probe,
	.remove		= tcc_asrc_remove,
	.suspend	= tcc_asrc_suspend,
	.resume		= tcc_asrc_resume,
	.driver 	= {
		.name	= "tcc_asrc_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_asrc_of_match),
#endif
	},
};

module_platform_driver(tcc_asrc_driver);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips ASRC Driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(tcc_asrc_get_handle_by_node);
