/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _TCC_ASRC_DRV_H_
#define _TCC_ASRC_DRV_H_

#include <sound/soc.h>

#include "tcc_pl080.h"
#include "tcc_asrc.h"
#include "tcc_asrc_m2m.h"
#include "tcc_asrc_m2m_ioctl.h"

#define NUM_OF_ASRC_PAIR\
	(4)
#define NUM_OF_ASRC_MCAUDIO\
	(4)
#define ASRC_RX_DMA_OFFSET\
	(4)
#define NUM_OF_AUX_PERI_CLKS\
	(4)
#define TCC_ASRC_MIN_CHANNELS\
	(2)

#define DEFAULT_VOLUME_GAIN\
	(0x100000) // 0dB

#define DEFAULT_VOLUME_RAMP_TIME\
	(TCC_ASRC_RAMP_0_125DB_PER_1SAMPLE)
#define DEFAULT_VOLUME_RAMP_WAIT\
	(0)
#define DEFAULT_VOLUME_RAMP_GAIN\
	(0) // 0dB

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define ASRC_M2M_INTERRUPT_MODE
#endif

enum tcc_asrc_drv_path_t {
	TCC_ASRC_M2M_PATH = 0,
	TCC_ASRC_M2P_PATH = 1,
	TCC_ASRC_P2M_PATH = 2,
};

enum tcc_asrc_async_refclk_t {
	TCC_ASRC_ASYNC_REFCLK_DAI0 = 0,
	TCC_ASRC_ASYNC_REFCLK_DAI1 = 1,
	TCC_ASRC_ASYNC_REFCLK_DAI2 = 2,
	TCC_ASRC_ASYNC_REFCLK_DAI3 = 3,
	TCC_ASRC_ASYNC_REFCLK_AUX  = 99,
};

enum tcc_asrc_drv_sync_mode_t {
	TCC_ASRC_SYNC_MODE	= 0,
	TCC_ASRC_ASYNC_MODE = 1,
};

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
enum tcc_asrc_drv_fifo_size_t {
	TCC_ASRC_FIFO_SIZE_256WORD = 256,
	TCC_ASRC_FIFO_SIZE_128WORD = 128,
	TCC_ASRC_FIFO_SIZE_64WORD = 64,
	TCC_ASRC_FIFO_SIZE_32WORD = 32,
	TCC_ASRC_FIFO_SIZE_16WORD = 16,
	TCC_ASRC_FIFO_SIZE_8WORD = 8,
	TCC_ASRC_FIFO_SIZE_4WORD = 4,
	TCC_ASRC_FIFO_SIZE_2WORD = 2,
};
#endif

struct tcc_pl080_buf_t {
	void *virt;
	dma_addr_t phys;

	struct pl080_lli *lli_virt;
	dma_addr_t lli_phys;
};

struct tcc_asrc_t {
	struct platform_device *pdev;
	void __iomem *asrc_reg;
	uint32_t asrc_reg_phys;
	struct clk *aux_pclk[NUM_OF_AUX_PERI_CLKS];
	struct clk *asrc_hclk;
	int asrc_irq;

	void __iomem *pl080_reg;
	int pl080_irq;
	struct clk *pl080_hclk;

#ifdef CONFIG_ARCH_TCC802X
	bool chip_rev_xx;
#endif
	int m2m_open_cnt;
	//dma buffers
	struct {
		struct tcc_pl080_buf_t txbuf;
		struct tcc_pl080_buf_t rxbuf;
		struct {
			uint32_t max_channel;
			enum tcc_asrc_drv_path_t path;
			enum tcc_asrc_drv_sync_mode_t sync_mode;
			enum tcc_asrc_async_refclk_t async_refclk;
			enum tcc_asrc_peri_t peri_dai;
			uint32_t peri_dai_rate;
			enum tcc_asrc_drv_bitwidth_t peri_dai_format;
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
			enum tcc_asrc_drv_fifo_size_t fifo_in_size;
#endif
		} hw;
		struct {
			enum tcc_asrc_drv_bitwidth_t tx_bitwidth;
			enum tcc_asrc_drv_bitwidth_t rx_bitwidth;
			enum tcc_asrc_drv_ch_t channels;
			uint32_t ratio_shift22; //|Int(31~22)|Fraction(21~0)|
		} cfg;
		struct {
			uint32_t started;
			uint32_t readable_size;
			uint32_t read_offset;
			uint32_t writable_size;
			uint32_t write_offset;
			struct snd_pcm_substream *substream;
		} stat; //m2m stat
		struct {
			uint32_t gain; // -0.125 * ramp_gain dB
			uint32_t up_time;
			uint32_t dn_time;
			uint32_t up_wait;
			uint32_t dn_wait;
		} volume_ramp;
		uint32_t volume_gain;

		spinlock_t lock;
		struct mutex m;
		wait_queue_head_t wq;
		struct completion comp;
	} pair[NUM_OF_ASRC_PAIR];

	//m2p or p2m
	struct snd_soc_dai_driver dai_drv[NUM_OF_ASRC_PAIR];
	int mcaudio_m2p_mux[NUM_OF_ASRC_MCAUDIO];

	struct asrc_reg_t asrc_regs_backup;
};

extern uint32_t tcc_asrc_txbuf_lli_phys_address(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	int idx);
extern uint32_t tcc_asrc_rxbuf_lli_phys_address(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	int idx);
extern int tcc_asrc_volume_gain(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_volume_ramp(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);

extern int tcc_asrc_tx_dma_start(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern int tcc_asrc_tx_dma_stop(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern int tcc_asrc_tx_dma_halt(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern int tcc_asrc_tx_fifo_enable(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	int enable);

extern int tcc_asrc_rx_dma_start(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern int tcc_asrc_rx_dma_stop(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern int tcc_asrc_rx_dma_halt(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern int tcc_asrc_rx_fifo_enable(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	int enable);

extern int tcc_asrc_m2m_sync_setup(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	enum tcc_asrc_fifo_fmt_t tx_fmt,
	enum tcc_asrc_fifo_mode_t tx_mode,
	enum tcc_asrc_fifo_fmt_t rx_fmt,
	enum tcc_asrc_fifo_mode_t rx_mode,
	uint32_t ratio_shift22);

extern int tcc_asrc_set_m2p_mux_select(
	struct tcc_asrc_t *asrc,
	int peri_target,
	int asrc_pair);
extern int tcc_asrc_m2p_setup(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	enum tcc_asrc_drv_sync_mode_t sync_mode,
	enum tcc_asrc_drv_bitwidth_t bitwidth,
	enum tcc_asrc_drv_ch_t channels,
	enum tcc_asrc_peri_t peri_target,
	enum tcc_asrc_async_refclk_t refclk,
	uint32_t src_rate,
	uint32_t dst_rate);

extern int tcc_asrc_p2m_setup(
	struct tcc_asrc_t *asrc,
	int asrc_pair,
	enum tcc_asrc_drv_sync_mode_t sync_mode,
	enum tcc_asrc_drv_bitwidth_t bitwidth,
	enum tcc_asrc_drv_ch_t channels,
	enum tcc_asrc_peri_t peri_src,
	enum tcc_asrc_drv_bitwidth_t peri_bitwidth,
	enum tcc_asrc_async_refclk_t refclk,
	uint32_t src_rate,
	uint32_t dst_rate);

extern int tcc_asrc_stop(
	struct tcc_asrc_t *asrc,
	int asrc_pair);
extern struct tcc_asrc_t *tcc_asrc_get_handle_by_node(struct device_node *np);

#endif /*_TCC_ASRC_DRV_H_*/
