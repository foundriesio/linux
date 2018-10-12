/*
 * linux/sound/soc/tcc/tcc-pcm.h   
 *
 * Based on:    linux/sound/arm/pxa2xx-pcm.h
 * Author:  <linux@telechips.com>
 * Created:	Nov 30, 2004
 * Modified:    May 07, 2017
 * Description: ALSA PCM interface for the Intel PXA2xx chip
 *
 * Copyright:	MontaVista Software, Inc.
 * Copyright (C) 2008-2009 Telechips 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _tcc_PCM_H
#define _tcc_PCM_H

#if defined(CONFIG_ANDROID)	// Android Style
#define __play_buf_size 65536
#define __play_buf_cnt  8
#define __cap_buf_size 8192
#define __cap_buf_cnt  8
#else  // LinuxAVN TCC8935 Style
#define __play_buf_size 65536
#define __play_buf_cnt  4
#define __cap_buf_size 16384
#define __cap_buf_cnt  16
#endif

#define min_period_size 256
#define min_period_cnt 2

#define RX_FIFO_SIZE	(512)
//#define RX_FIFO_CLEAR_BY_DELAY_AND_MUTE
#define RX_FIFO_CLEAR_IN_DMA_BUF

/* prtd->flag */
#define DMA_I2S_OUT 0x0001
#define DMA_I2S_IN  0x0002
#define DMA_SPDIF_OUT 0x0004
#define DMA_SPDIF_IN 0x0008	// Planet 20150812 S/PDIF_Rx
#define DMA_CDIF 0x0010
#define DMA_CDIF_SPDIF_IN (DMA_SPDIF_IN|DMA_CDIF)
#define DMA_FLAG (DMA_I2S_OUT|DMA_I2S_IN|DMA_SPDIF_OUT|DMA_SPDIF_IN|DMA_CDIF)

#define TCC_I2S_FLAG	0x0100
#define TCC_SPDIF_FLAG	0x0200
#define TCC_CDIF_FLAG	0x0400


/* prtd->ptcc_intr->flag */
#define TCC_INTERRUPT_REQUESTED 0x1000
#define TCC_RUNNING_PLAY        0x0001
#define TCC_RUNNING_CAPTURE     0x0002
#define TCC_RUNNING_SPDIF_PLAY  0x0010
#define TCC_RUNNING_SPDIF_CAPTURE 	0x0020	// Planet 20150812 S/PDIF_Rx
#define TCC_RUNNING_CDIF_CAPTURE 0x0100
#define TCC_RUNNING_CDIF_SPDIF_CAPTURE (TCC_RUNNING_SPDIF_CAPTURE|TCC_RUNNING_CDIF_CAPTURE)
#define TCC_RUNNING_DAI (TCC_RUNNING_PLAY | TCC_RUNNING_CAPTURE)
#define TCC_RUNNING_SPDIF (TCC_RUNNING_SPDIF_PLAY | TCC_RUNNING_SPDIF_CAPTURE)
#define TCC_RUNNING_FLAG (TCC_RUNNING_DAI | TCC_RUNNING_SPDIF | TCC_RUNNING_CDIF_CAPTURE)

/* ADMA DAI TX/RX */
#define ADMA_RXDADAR	0x000 // DAI Rx (Right) Data Destination Address
#define ADMA_RXDAPARAM 	0x004 // DAI Rx Parameters
#define ADMA_RXDATCNT 	0x008 // DAI Rx Transmission Counter Register
#define ADMA_RXDACDAR 	0x00C // DAI Rx (Right) Data Current Destination Address

#define ADMA_RXDADARL 	0x028 // DAI Rx Left Data Destination Address
#define ADMA_RXDACDARL 	0x02C // DAI Rx Left Data Current Destination Address

#define ADMA_TXDASAR  	0x040 // DAI Tx (Right) Data Source Address
#define ADMA_TXDAPARAM 	0x044 // DAI Tx Parameters
#define ADMA_TXDATCNT 	0x048 // DAI Tx Transmission Counter Register
#define ADMA_TXDACSAR 	0x04C // DAI Tx (Right) Data Current Source Address

#define ADMA_TXDASARL 	0x068 // DAI Tx Left Data Source Address
#define ADMA_TXDACSARL 	0x06C // DAI Tx Left Data Current Source Address

#define ADMA_RXDADAR1	0x100 // DAI1 Rx (Right) Data Destination Address
#define ADMA_RXDADAR2	0x104 // DAI2 Rx (Right) Data Destination Address
#define ADMA_RXDADAR3	0x108 // DAI3 Rx (Right) Data Destination Address

#define ADMA_RXDACAR1	0x10C // DAI1 Rx (Right) Data Current Destination Address
#define ADMA_RXDACAR2	0x110 // DAI2 Rx (Right) Data Current Destination Address
#define ADMA_RXDACAR3	0x114 // DAI3 Rx (Right) Data Current Destination Address

#define ADMA_RXDADARL1	0x118 // DAI1 Rx Left Data Destination Address
#define ADMA_RXDADARL2	0x11C // DAI2 Rx Left Data Destination Address
#define ADMA_RXDADARL3	0x120 // DAI3 Rx Left Data Destination Address

#define ADMA_RXDACARL1	0x124 // DAI1 Rx Left Data Current Destination Address
#define ADMA_RXDACARL2	0x128 // DAI2 Rx Left Data Current Destination Address
#define ADMA_RXDACARL3	0x12C // DAI3 Rx Left Data Current Destination Address

#define ADMA_TXDASAR1	0x130 // DAI1 Tx (Right) Data Source Address
#define ADMA_TXDASAR2	0x134 // DAI2 Tx (Right) Data Source Address
#define ADMA_TXDASAR3	0x138 // DAI3 Tx (Right) Data Source Address

#define ADMA_TXDACAR1	0x13C // DAI1 Tx (Right) Data Current Source Address
#define ADMA_TXDACAR2	0x140 // DAI2 Tx (Right) Data Current Source Address
#define ADMA_TXDACAR3	0x144 // DAI3 Tx (Right) Data Current Source Address

#define ADMA_TXDASARL1	0x148 // DAI1 Tx Left Data Source Address
#define ADMA_TXDASARL2	0x14C // DAI2 Tx Left Data Source Address
#define ADMA_TXDASARL3	0x150 // DAI3 Tx Left Data Source Address

#define ADMA_TXDACARL1	0x154 // DAI1 Tx Left Data Current Source Address
#define ADMA_TXDACARL2	0x158 // DAI2 Tx Left Data Current Source Address
#define ADMA_TXDACARL3	0x15C // DAI3 Tx Left Data Current Source Address

#define ADMA_ADMARST	0x180 // Audio DMA Reset

/* ADMA SPDIF TX/RX(CDIF) */
#define ADMA_RXCDDAR 	0x010 // CDIF(SPDIF) Rx (Right) Data Destination Address
#define ADMA_RXCDPARAM 	0x014 // CDIF(SPDIF) Rx Parameters
#define ADMA_RXCDTCNT 	0x018 // CDIF(SPDIF) Rx Transmission Counter Register
#define ADMA_RXCDCDAR 	0x01C // CDIF(SPDIF) Rx (Right) Data Current Destination Address

#define ADMA_RXCDDARL 	0x030 // CDIF(SPDIF) Rx Left Data Destination Address
#define ADMA_RXCDCDARL 	0x034 // CDIF(SPDIF) Rx Left Data Current Destination Address

#define ADMA_TXSPSAR 	0x050 // SPDIF Tx (Right) Data Source Address
#define ADMA_TXSPPARAM 	0x054 // SPDIF Tx Parameters
#define ADMA_TXSPTCNT 	0x058 // SPDIF Tx Transmission Counter Register
#define ADMA_TXSPCSAR 	0x05C // SPDIF Tx (Right) Data Current Source Address

#define ADMA_TXSPSARL 	0x070 // SPDIF Tx Left Data Source Address
#define ADMA_TXSPCSARL 	0x074 // SPDIF Tx Left Data Current Source address

/* ADMA COMMON */
#define ADMA_TRANSCTRL 	0x038 // DMA Transfer Control Register
#define ADMA_RPTCTRL 	0x03C // DMA Repeat Control Register

#define ADMA_CHCTRL 	0x078 // DMA Channel Control Register
#define ADMA_INTSTATUS 	0x07C // DMA Interrupt Status Register
#define ADMA_GINTREQ 	0x080 // General Interrupt Request
#define ADMA_GINTSTATUS	0x084 // General Interrupt Status

#define pcm_writel  __raw_writel
#define pcm_readl   __raw_readl

struct tcc_audio_reg {
	void __iomem    *dai_reg;
	void __iomem    *adma_reg;
};

struct tcc_audio_clk {
	struct clk  *dai_pclk;
	struct clk  *dai_hclk;
	struct clk  *af_pclk;   //This is for I2S SLAVE MODE. Audio Filter Peri-Clock
};

struct tcc_audio_intr {
	struct snd_pcm_substream *play_ptr;
	struct snd_pcm_substream *cap_ptr;
	unsigned int flag;
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
	//This is for period counter
	unsigned int nIn;
	unsigned int nOut;
#endif
};

struct tcc_adma_drv_data {
	int id;
	spinlock_t *slock;	//For access ADMA register.
	void __iomem				*adma_reg;
	struct tcc_adma_backup_reg	*backup_adma;
};

struct tcc_adma_backup_reg {
	unsigned int rTransCtrl;
	unsigned int rRptCtrl;
	unsigned int rChCtrl;
	unsigned int rGIntReq;
};

struct tcc_runtime_data {
	int id;
	unsigned int flag;
	/*
0x0100: I2S
0x0200: SPDIF
0x0400: CDIF
*/
	unsigned int    dai_irq;
	unsigned int    adma_irq;

	struct tcc_audio_reg *ptcc_reg;
	struct tcc_audio_clk *ptcc_clk;
	struct tcc_audio_intr *ptcc_intr;

	/* This is for I2S mono channel */
	struct snd_dma_buffer *mono_dma_play;
	struct snd_dma_buffer *mono_dma_capture;

	dma_addr_t dma_buffer;          /* physical address of dma buffer */
	dma_addr_t dma_buffer_end;      /* first address beyond DMA buffer */
	dma_addr_t period_ptr;          /* physical address of next period */

	size_t period_size;
	size_t nperiod;

	spinlock_t *adma_slock;	//For access ADMA register.

	//struct tcc_adma_backup_reg *backup_adma;
	void *dai_port; //This is for port mux. That support TCC898x, TCC802x.
	void *private_data; //This is for I2S, SPIDF, CDIF data struct.

	int to_be_cleared_rx_period_cnt;
};

extern int tcc_pcm_platform_register(struct device *dev);
extern void tcc_pcm_platform_unregister(struct device *dev);

extern int tcc_pcm_dsp_platform_register(struct device *dev);
extern void tcc_pcm_dsp_platform_unregister(struct device *dev);
#endif
