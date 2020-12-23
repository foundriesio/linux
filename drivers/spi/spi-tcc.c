// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/spidev.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/module.h>

#include <linux/bits.h>

#include "spi-tcc.h"

/* Get tcc spi platform data */
static struct tcc_spi_pl_data *tcc_spi_get_pl_data(struct tcc_spi *tccspi)
{
	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tcc_spi is null!\n", __func__);
		return NULL;
	}

	return (struct tcc_spi_pl_data *)tccspi->pd;
}

/* 0: Dedicated DMA 1: GDMA */
static int32_t tcc_spi_is_use_gdma(struct tcc_spi *tccspi)
{
	if (tccspi->pd == NULL) {
		return 0;
	}
	return tccspi->pd->dma_enable;
}

/* Print values of GPSB registers */
#ifdef DEBUG
static void tcc_spi_regs_dump(struct tcc_spi *tccspi)
{
	struct tcc_spi_pl_data *pd;
	int32_t gpsb_channel = -1;
	int32_t gdma_enable;

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	pd = tcc_spi_get_pl_data(tccspi);
	if (pd != NULL) {
		gpsb_channel = pd->gpsb_channel;
	}
	gdma_enable = tcc_spi_is_use_gdma(tccspi);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] ##\tGPSB REGS DUMP [CH: %d]\t##\n",
			gpsb_channel);
	dev_dbg(tccspi->dev, "[DEBUG][SPI] STAT\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_STAT));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] INTEN\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_INTEN));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] MODE\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_MODE));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] CTRL\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_CTRL));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] EVTCTRL\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_EVTCTRL));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] CCV\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_CCV));

	if (gdma_enable == 0) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] TXBASE\t: 0x%08X\n",
				readl(tccspi->base + TCC_GPSB_TXBASE));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] RXBASE\t: 0x%08X\n",
				readl(tccspi->base + TCC_GPSB_RXBASE));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] PACKET\t: 0x%08X\n",
				readl(tccspi->base + TCC_GPSB_PACKET));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] DMACTR\t: 0x%08X\n",
				readl(tccspi->base + TCC_GPSB_DMACTR));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] DMASTR\t: 0x%08X\n",
				readl(tccspi->base + TCC_GPSB_DMASTR));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] DMAICR\t: 0x%08X\n",
				readl(tccspi->base + TCC_GPSB_DMAICR));
	}
}
#else
static void tcc_spi_regs_dump(struct tcc_spi *tccspi)
{
}
#endif

/* Clear TCC GPSB DMA Packet counter */
static void tcc_spi_clear_packet_cnt(struct tcc_spi *tccspi)
{
	int32_t gdma_enable;

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable == 0) {
		/* Clear GPSB DMA Packet counter */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_PCLR);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_PCLR);
	}
}

/* Set TCC GPSB DMA Packet counter */
static void tcc_spi_set_packet_size(struct tcc_spi *tccspi, uint32_t size)
{
	int32_t gdma_enable;

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable == 0) {
		writel(TCC_GPSB_PACKET_SIZE(size),
				tccspi->base + TCC_GPSB_PACKET);
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] size: %d packet reg 0x%08X\n",
				__func__,
				size,
				readl(tccspi->base + TCC_GPSB_PACKET));
	}
}

/* Clear Tx and Rx FIFO counter */
static void tcc_spi_clear_fifo(struct tcc_spi *tccspi)
{
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	/* Clear Tx and Rx FIFO counter */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
			TCC_GPSB_MODE_CRF | TCC_GPSB_MODE_CWF);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
			TCC_GPSB_MODE_CRF | TCC_GPSB_MODE_CWF);
}

#ifdef TCC_USE_GFB_PORT
static void tcc_spi_set_port(struct tcc_spi *tccspi)
{
	int32_t i;
	uint32_t pcfg_offset;
	struct tcc_spi_pl_data *pd = tcc_spi_get_pl_data(tccspi);

	if (pd == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI][%s] tcc_pl_data is null\n",
				__func__);
		return;
	}

	pcfg_offset = TCC_GPSB_PCFG_OFFSET * pd->gpsb_channel;

	for (i = 0; i < 4; i++) {
		TCC_GPSB_BITCSET(tccspi->pcfg + pcfg_offset,
				(0xFF)<<(i * TCC_GPSB_PCFG_SHIFT),
				pd->port[i] << (i * TCC_GPSB_PCFG_SHIFT));
	}

	dev_info(tccspi->dev,
			"[INFO][SPI] ch %d pcfg(0x%08X)=0x%08X\n",
			pd->gpsb_channel,
			(u32)(tccspi->pcfg + pcfg_offset),
			readl(tccspi->pcfg + pcfg_offset));
}
#else
static void TCC_GPSB_PCFG_CSET(struct tcc_spi *tccspi,
		int32_t ch, uint32_t cmsk, uint32_t smsk)
{
	uint32_t offset;

	if (ch <= 3) {
		offset = (u32)ch << 3U;
		TCC_GPSB_BITCSET(tccspi->pcfg + TCC_GPSB_PCFG0,
				((cmsk & 0xFFU) << offset),
				((smsk & 0xFFU) << offset));
	} else {
		offset = ((u32)ch - 4U) << 3U;
		TCC_GPSB_BITCSET(tccspi->pcfg + TCC_GPSB_PCFG1,
				((cmsk & 0xFFU) << offset),
				((smsk & 0xFFU) << offset));
	}
}

/* Clear on port configuration */
static void tcc_spi_clear_port(struct tcc_spi *tccspi, int32_t channel)
{
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);
	TCC_GPSB_PCFG_CSET(tccspi, channel, 0xFFU, 0xFFU);
}

/* Get the port configuration */
static uint32_t tcc_spi_get_port(struct tcc_spi *tccspi, int32_t channel)
{
	uint32_t pcfg, port, offset;


	if (channel <= 3) {
		offset = (u32)channel << 3U;
		pcfg = readl(tccspi->pcfg + TCC_GPSB_PCFG0);
		port = (pcfg >> offset);
	} else {
		offset = ((u32)channel - 4U) << 3U;
		pcfg = readl(tccspi->pcfg + TCC_GPSB_PCFG1);
		port = (pcfg >> offset);
	}
	port &= 0xFFU;
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] channel: %d port: %d\n",
			__func__, channel, port);

	return port;
}

/* Set the port configuration and check the port setting conflict */
static void tcc_spi_set_port(struct tcc_spi *tccspi)
{
	int32_t channel, i;
	uint32_t port, port_conflict;
	struct tcc_spi_pl_data *pd = tcc_spi_get_pl_data(tccspi);

	if (pd == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI][%s] tcc_pl_data is null\n",
				__func__);
		return;
	}

	channel = pd->gpsb_channel;
	port = pd->port;

	if ((channel < 0) || (port > 0xFFU)) {
		dev_err(tccspi->dev, "[ERROR][SPI] channel(%d), port(%d) is wrong\n",
				channel, port);
		return;
	}

	/* Set the port */
	TCC_GPSB_PCFG_CSET(tccspi, channel, 0xFFU, port);
	dev_info(tccspi->dev, "[INFO][SPI] set port config. ch - %d port - %d\n",
			channel, port);

	/* Check the port setting conflict */
	for (i = 0; i < TCC_GPSB_MAX_CH; i++) {
		if (i == channel) {
			continue;
		}
		port_conflict = tcc_spi_get_port(tccspi, i);
		if (port_conflict == port) {
			tcc_spi_clear_port(tccspi, i);
			dev_warn(tccspi->dev,
					"[WARN][SPI] port conflict! [[ch %d[%d]]] : ch %d[%d] pcfg0: 0x%08X pcfg1: 0x%08X\n",
					channel, port, i, port_conflict,
					readl(tccspi->pcfg + TCC_GPSB_PCFG0),
					readl(tccspi->pcfg + TCC_GPSB_PCFG1));
		}
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] pcfg0: 0x%08X pcfg1: 0x%08X\n",
			__func__,
			readl(tccspi->pcfg + TCC_GPSB_PCFG0),
			readl(tccspi->pcfg + TCC_GPSB_PCFG1)
		   );
}
#endif

/* Set the bit width */
static void tcc_spi_set_bit_width(struct tcc_spi *tccspi, uint32_t width)
{
	int32_t gdma_enable;
	uint32_t val, tmp = 1;

	if (width < tmp) {
		dev_warn(tccspi->dev, "[WARN][SPI]%s: not supported bpw(%d)\n",
				__func__, width);
		return;
	}
	val = width - tmp;
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
			TCC_GPSB_MODE_BWS(0xFFUL));
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
			TCC_GPSB_MODE_BWS(val));

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable == 0) {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
				(TCC_GPSB_INTEN_SHT
				 | TCC_GPSB_INTEN_SBT
				 | TCC_GPSB_INTEN_SHR
				 | TCC_GPSB_INTEN_SBR));
		/* Set the endian mode according to the bit-width */
		if ((val & ((u32)1U << 4)) > 0U) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
					TCC_GPSB_DMACTR_END);
		} else {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
					TCC_GPSB_DMACTR_END);
			if (width == 16U) {
				TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
						(TCC_GPSB_INTEN_SBT |
						 TCC_GPSB_INTEN_SBR));
			}
		}
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] bitwidth: %d endian: %d\n",
			__func__, width, (s32)(val & BIT(4)));
}

/* Set the SCLK */
static int32_t tcc_spi_set_clk(struct tcc_spi *tccspi, uint32_t clk,
		uint32_t divldv, int32_t enable)
{
	uint32_t pclk, clk_, tmp;
	int32_t ret;
	struct tcc_spi_pl_data *pd;

	if (enable == 0) {
		return 0;
	}
	pd = tcc_spi_get_pl_data(tccspi);

	if (clk == 0U) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] clk err (%u)\n",
				clk);
		return -EINVAL;
	}

	if (pd == NULL) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] pl data is null!\n");
		return -EINVAL;
	}

	if (pd->pclk == NULL) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] pclk is null!!\n");
		return -EINVAL;
	}

	clk_ = clk;

	/* Set the GPSB clock divider load value */
	TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_MODE,
			TCC_GPSB_MODE_DIVLDV(0xFFUL),
			TCC_GPSB_MODE_DIVLDV(divldv));

	/* Calculate Peri Clock */
	tmp = 1U;
	if ((UINT_MAX - tmp) < divldv) {
		return -EIO;
	}
	if ((UINT_MAX / clk_) < (divldv + tmp)) {
		return -EIO;
	}
	pclk = (clk_ * (divldv + tmp)) << 1U;
	ret = clk_set_rate(pd->pclk, pclk);
	if (ret != 0) {
		return ret;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI][%s] sclk: %dHz divldv: %d pclk: %luHz\n",
			__func__, clk_, divldv, clk_get_rate(pd->pclk));

	return 0;
}

/* Set SPI modes */
static int32_t tcc_spi_set_mode(struct tcc_spi *tccspi, uint32_t mode)
{
	bool is_slave;

	is_slave = spi_controller_is_slave(tccspi->master);

	if (is_slave) {
		/* slave mode */
		if ((mode == SPI_MODE_1) || (mode == SPI_MODE_2)) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		} else {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		}
	} else {
		/* master mode */
		if ((mode & SPI_CPOL) != 0U) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		} else {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		}
		if ((mode & SPI_CPHA) != 0U) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PWD | TCC_GPSB_MODE_PRD);
		} else {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PWD | TCC_GPSB_MODE_PRD);
		}
	}
	if ((mode & SPI_CS_HIGH) != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_PCS | TCC_GPSB_MODE_PCD);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_PCS | TCC_GPSB_MODE_PCD);
	}
	if ((mode & SPI_LSB_FIRST) != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SD);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SD);
	}
	if ((mode & SPI_LOOP) != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_LB);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_LB);
	}
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] mode: 0x%X (mode reg: 0x%08X)\n",
			__func__,
			mode,
			tcc_spi_readl(tccspi->base + TCC_GPSB_MODE));
	return 0;
}

/* Initialize GPSB register settings */
static void tcc_spi_hwinit(struct tcc_spi *tccspi)
{
	int32_t gdma_enable;
	bool is_slave;

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	/* Reset GPSB registers */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_STAT, 0xFFFFFFFFU);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN, 0xFFFFFFFFU);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, 0xFFFFFFFFU);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_CTRL, 0xFFFFFFFFU);
	if (gdma_enable == 0) {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_TXBASE, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_RXBASE, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_PACKET, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMASTR, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR, 0xFFFFFFFFU);
	}

	/* Disable operation */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);

	/* Set bitwidth (Default: 8) */
	tcc_spi_set_bit_width(tccspi, 8);

	/* Set operation mode (SPI compatible) */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_MD_MASK);

	if (tccspi->pd->ctf) {
		/* Set CTF Mode */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_CTF);
	}
	/* Set Tx and Rx FIFO threshold for interrupt/DMA request */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
			TCC_GPSB_INTEN_CFGRTH_MASK);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
			TCC_GPSB_INTEN_CFGWTH_MASK);

	is_slave = spi_controller_is_slave(tccspi->master);
	if (is_slave) {
		/* Set SPI slave mode */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SLV);
		TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_CFGWTH_MASK,
				TCC_GPSB_INTEN_CFGWTH(6UL));
	} else {
		/* Set SPI master mode */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SLV);
	}

	/* Enable operation */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);
}

/* Set TCC GPSB DMA Tx and Rx base address and DMA request */
static void tcc_spi_set_dma_addr(struct tcc_spi *tccspi,
		dma_addr_t tx, dma_addr_t rx)
{
	int32_t gdma_enable;

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable == 0) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] - dedicated dma\n",
				__func__);
		/* Set Base address */
		writel((tx & 0xFFFFFFFFU), tccspi->base + TCC_GPSB_TXBASE);
		writel((rx & 0xFFFFFFFFU), tccspi->base + TCC_GPSB_RXBASE);
	}

	/* Set DMA request */
	if (tx != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DW);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DW);
	}
	if (rx != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DR);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DR);
	}
}

/* Stop transfer */
static void tcc_spi_stop_dma(struct tcc_spi *tccspi)
{
	int32_t gdma_enable;

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable == 0) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] - dedicated dma\n",
				__func__);
		/* Clear DMA done and packet interrupt status */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_ISD | TCC_GPSB_DMAICR_ISP);

		/* Disable GPSB DMA operation */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_EN);

		/* Disable DMA Tx and Rx request */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_DTE | TCC_GPSB_DMACTR_DRE);
	}
}

/* Start transfer */
static void tcc_spi_start_dma(struct tcc_spi *tccspi)
{
	int32_t gdma_enable;

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable == 0) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] - dedicated dma\n",
				__func__);
		/* Set GPSB DMA address mode (Multiple address mode) */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				(TCC_GPSB_DMACTR_RXAM_MASK |
				 TCC_GPSB_DMACTR_TXAM_MASK));

		/* Enable DMA Tx and Rx request */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
				(TCC_GPSB_DMACTR_DTE |
				 TCC_GPSB_DMACTR_DRE));

		/* Enable DMA done interrupt */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_IED);

		/* Disable DMA packet interrupt */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_IEP);

		/* Set DMA Rx interrupt */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_IRQS);

		/* Enable GPSB DMA operation */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_EN);
	}
}

/* Allocate dma buffer */
static int32_t tcc_spi_init_dma_buf(struct tcc_spi *tccspi, int32_t dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;

	dev = tccspi->dev;

	v_addr = dma_alloc_writecombine(dev,
			tccspi->dma_buf_size,
			&dma_addr,
			GFP_KERNEL);
	if (v_addr == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI] Fail to allocate the dma buffer\n");
		return -ENOMEM;
	}

	if (dma_to_mem != 0) {
		tccspi->rx_buf.v_addr = v_addr;
		tccspi->rx_buf.dma_addr = dma_addr;
		tccspi->rx_buf.size = tccspi->dma_buf_size;
	} else {
		tccspi->tx_buf.v_addr = v_addr;
		tccspi->tx_buf.dma_addr = dma_addr;
		tccspi->tx_buf.size = tccspi->dma_buf_size;
	}

	dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] dma_to_mem: %d v_addr: %#lX dma_addr: 0x%08X size: %ld\n",
			__func__, dma_to_mem,
			(ulong)v_addr,
			(u32)dma_addr,
			tccspi->dma_buf_size);

	return 0;
}

/* De-allocate dma buffer */
static void tcc_spi_deinit_dma_buf(struct tcc_spi *tccspi, int32_t dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;


	if (dma_to_mem != 0) {
		v_addr = tccspi->rx_buf.v_addr;
		dma_addr = tccspi->rx_buf.dma_addr;
	} else {
		v_addr = tccspi->tx_buf.v_addr;
		dma_addr = tccspi->tx_buf.dma_addr;
	}

	dma_free_writecombine(tccspi->dev,
			tccspi->dma_buf_size,
			v_addr,
			dma_addr);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] dma_to_mem: %d v_addr: %#lX dma_addr: 0x%08X size: %ld\n",
			__func__, dma_to_mem,
			(ulong)v_addr,
			(u32)dma_addr,
			tccspi->dma_buf_size);
}

/* Copy client buf to spi bux (tx) */
static void tcc_spi_txbuf_copy_client_to_spi(struct tcc_spi *tccspi,
		struct spi_transfer *xfer, uint32_t len)
{
	if (xfer->tx_buf == NULL) {
		(void)memset(tccspi->tx_buf.v_addr, 0, len);
		return;
	}

	(void)memcpy(tccspi->tx_buf.v_addr, xfer->tx_buf + tccspi->cur_tx_pos, len);
	if ((UINT_MAX - len) < tccspi->cur_tx_pos) {
		dev_warn(tccspi->dev, "[WARN][SPI] %s: len(%d) is too long\n",
				__func__, len);
	}
	tccspi->cur_tx_pos += len;

	dev_dbg(tccspi->dev, "[DEBUG][SPI][%s] tx - client_buf: %#lX offset: %#X spi_buf: %#lX\n",
			__func__,
			(ulong)xfer->tx_buf,
			tccspi->cur_tx_pos,
			(ulong)tccspi->tx_buf.v_addr);
}

/* Copy client buf to spi bux (rx) */
static void tcc_spi_rxbuf_copy_client_to_spi(struct tcc_spi *tccspi,
		struct spi_transfer *xfer, uint32_t len)
{
	if (xfer->rx_buf == NULL) {
		(void)memset(tccspi->rx_buf.v_addr, 0, len);
		return;
	}

	(void)memcpy(xfer->rx_buf + tccspi->cur_rx_pos, tccspi->rx_buf.v_addr, len);
	if ((UINT_MAX - len) < tccspi->cur_rx_pos) {
		dev_warn(tccspi->dev, "[WARN][SPI] %s: len(%d) is too long\n",
				__func__, len);
	}
	tccspi->cur_rx_pos += len;

	dev_dbg(tccspi->dev, "[DEBUG][SPI][%s] rx - client_buf: %#lX offset: %#X spi_buf: %#lX\n",
			__func__,
			(ulong)xfer->rx_buf,
			tccspi->cur_rx_pos,
			(ulong)tccspi->rx_buf.v_addr);
}

/* Check channel DMA IRQ status */
static int32_t tcc_spi_check_dma_irq_status(struct tcc_spi *tccspi)
{
	struct tcc_spi_pl_data *pd;
	uint32_t val;
	int32_t ch, offset, ret;

	pd = tcc_spi_get_pl_data(tccspi);
	if (pd == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI][%s] tcc_spi pd is null\n",
				__func__);
		return -EINVAL;
	}

	ch = pd->gpsb_channel;
	if (ch >= TCC_GPSB_MAX_CH) {
		dev_err(tccspi->dev, "[ERROR][SPI][%s] gpsb channel is wrong (ch: %d)\n",
				__func__, ch);
		return -EINVAL;
	}
	offset = (ch * 2) + 1;
	val = readl(tccspi->pcfg + TCC_GPSB_CIRQST);

	/* Check dma irq status */
	if (((val >> (u32)offset) & 0x1U) > 0U) {
		ret = 1;
	} else {
		ret = 0;
	}
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] irq_status: %d\n",
			__func__, ret);

	return ret;
}

/* TCC GPSB DMA IRQ Handler */
static irqreturn_t tcc_spi_dma_irq(int32_t irq, void *data)
{
	struct spi_master *master = data;
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	uint32_t dmaicr, status;
	int32_t ret;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tccspi is null (irq: %d)\n",
				__func__, irq);
		return IRQ_NONE;
	}

	ret = tcc_spi_check_dma_irq_status(tccspi);
	/* Check dma irq status*/
	if (ret < 1) {
		return IRQ_NONE;
	}
	/* Check GPSB error flag status */
	status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
	if ((status & TCC_GPSB_STAT_ERR) > 0U) {
		dev_warn(tccspi->dev, "[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
	}

	/* Handle DMA done interrupt */
	dmaicr = readl(tccspi->base + TCC_GPSB_DMAICR);
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] dmaicr: 0x%08X\n",
			__func__, dmaicr);
	if ((dmaicr & ((u32)TCC_GPSB_DMAICR_ISD)) > 0U) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] irq %d - channel :%d\n",
				__func__, irq, tccspi->pd->gpsb_channel);
		/* Stop dma operation to hanlde buffer */
		tcc_spi_stop_dma(tccspi);
		complete(&tccspi->xfer_complete);
	}

	return IRQ_HANDLED;
}

#ifdef TCC_DMA_ENGINE
/****** DMA-engine specific ******/
#define TCC_SPI_GDMA_WSIZE	1 /* Default word size */
#define TCC_SPI_GDMA_BSIZE	1 /* Default burst size */
#define TCC_SPI_GDMA_SG_LEN	1

/* Release dma-eninge channel */
static void tcc_spi_release_dma_engine(struct tcc_spi *tccspi)
{
	if (tccspi->dma.chan_tx != NULL) {
		dma_release_channel(tccspi->dma.chan_tx);
		tccspi->dma.chan_tx = NULL;
	}
	if (tccspi->dma.chan_rx != NULL) {
		dma_release_channel(tccspi->dma.chan_rx);
		tccspi->dma.chan_rx = NULL;
	}

}

/* dma-engine filter */
static bool tcc_spi_dma_engine_filter(struct dma_chan *chan, void *pdata)
{
	struct tcc_spi_gdma *tccdma = pdata;
	struct device *dma_dev;

	if (tccdma == NULL) {
		dev_err(chan->device->dev, "[ERROR][SPI] [%s] tcc_spi_gdma is NULL!!\n",
				__func__);
		return (bool)false;
	}

	dma_dev = tccdma->dma_dev;
	if (dma_dev == chan->device->dev) {
		chan->private = dma_dev;
		return (bool)true;
	}

	dev_err(chan->device->dev, "[ERROR][SPI] dma_dev(%p) != dev(%p)\n",
			dma_dev, chan->device->dev);
	return (bool)false;
}

/* Terminate all dma-engine channel */
static void tcc_spi_stop_dma_engine(struct tcc_spi *tccspi)
{
	if (tccspi->dma.chan_tx != NULL) {
		(void)dmaengine_terminate_all(tccspi->dma.chan_tx);
	}
	if (tccspi->dma.chan_rx != NULL) {
		(void)dmaengine_terminate_all(tccspi->dma.chan_rx);
	}
}

/* dma-engine tx channel callback */
static void tcc_dma_engine_tx_callback(void *data)
{
	struct spi_master *master = data;
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	uint32_t status;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tcc_spi is NULL!!\n", __func__);
		return;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] gpsb channel %d\n",
			__func__, tccspi->pd->gpsb_channel);

	/* Check GPSB error flag status */
	status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
	if ((status & TCC_GPSB_STAT_ERR) > 0U) {
		dev_warn(tccspi->dev, "[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
	}
}

/* dma-engine rx channel callback */
static void tcc_dma_engine_rx_callback(void *data)
{
	struct spi_master *master = data;
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	uint32_t status;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tcc_spi is NULL!!\n", __func__);
		return;
	}

	dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] gpsb channel %d\n",
			__func__, tccspi->pd->gpsb_channel);

	/* Check GPSB error flag status */
	status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
	if ((status & TCC_GPSB_STAT_ERR) > 0U) {
		dev_warn(tccspi->dev, "[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
	}

	/* Stop dma operation */
	tcc_spi_stop_dma(tccspi);
	complete(&tccspi->xfer_complete);
}

/* Set dma-engine slave config word size */
static void tcc_spi_dma_engine_slv_cfg_addr_width
(struct dma_slave_config *slave_config, u8 bpw, int32_t src)
{
	// Set WSIZE
	if (src > 0) {
		slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;
	} else {
		slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;
	}
	if (bpw == 4U) {
		if (src > 0) {
			slave_config->src_addr_width =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
		} else {
			slave_config->dst_addr_width =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
		}
	} else if (bpw == 2U) {
		if (src > 0) {
			slave_config->src_addr_width =
				DMA_SLAVE_BUSWIDTH_2_BYTES;
		} else {
			slave_config->dst_addr_width =
				DMA_SLAVE_BUSWIDTH_2_BYTES;
		}
	} else {
		if (src > 0) {
			slave_config->src_addr_width =
				DMA_SLAVE_BUSWIDTH_1_BYTE;
		} else {
			slave_config->dst_addr_width =
				DMA_SLAVE_BUSWIDTH_1_BYTE;
		}
	}
}

/* Configure dma-engine slaves */
static int32_t tcc_spi_dma_engine_slv_cfg
(struct tcc_spi *tccspi, struct dma_slave_config *slave_config, u8 bpw)
{
	int32_t ret, error = 0;

	/* Set Busrt size(BSIZE) */
	slave_config->dst_maxburst = TCC_SPI_GDMA_BSIZE;
	slave_config->src_maxburst = TCC_SPI_GDMA_BSIZE;

	/* Set source and destincation address */
	slave_config->dst_addr = (dma_addr_t)(tccspi->pbase); /* GPSB PORT */
	slave_config->src_addr = (dma_addr_t)(tccspi->pbase); /* GPSB PORT */

	/* Set tx channel */
	slave_config->direction = DMA_MEM_TO_DEV;
	tcc_spi_dma_engine_slv_cfg_addr_width(slave_config, bpw, 1);
	ret = dmaengine_slave_config(tccspi->dma.chan_tx, slave_config);
	if (ret < 0) {
		dev_err(tccspi->dev, "[ERROR][SPI] Failed to configrue tx dma channel.\n");
		error = ret;
	}

	/* Set rx channel */
	slave_config->direction = DMA_DEV_TO_MEM;
	tcc_spi_dma_engine_slv_cfg_addr_width(slave_config, bpw, 0);
	ret = dmaengine_slave_config(tccspi->dma.chan_rx, slave_config);
	if (ret < 0) {
		dev_err(tccspi->dev, "[ERROR][SPI] Fail to configrue rx dma channel\n");
		error = ret;
	}

	return error;

}

/* Submit dma-engine descriptor */
static int32_t tcc_spi_dma_engine_submit(struct tcc_spi *tccspi, u32 flen)
{
	struct dma_chan *txchan = tccspi->dma.chan_tx;
	struct dma_chan *rxchan = tccspi->dma.chan_rx;
	struct dma_async_tx_descriptor *txdesc;
	struct dma_async_tx_descriptor *rxdesc;
	struct dma_slave_config slave_config;
	dma_cookie_t cookie;
	u32 len, bits_per_word;
	u8 bpw;
	int32_t ret;

	if ((rxchan == NULL) || (txchan == NULL)) {
		dev_err(tccspi->dev, "[ERROR][SPI] rxchan(%p) or txchan(%p) are NULL\n",
				rxchan, txchan);
		return -ENODEV;
	}

	bits_per_word = readl(tccspi->base + TCC_GPSB_MODE);
	bits_per_word = ((bits_per_word & TCC_GPSB_MODE_BWS_MASK) >> 8U) + 1U;
	bpw = ((u8)bits_per_word / 8U);
	len = flen / bpw;
	if (len == 0UL) {
		dev_err(tccspi->dev, "[ERROR][SPI] dma xfer len err, bpw %d len %d\n",
				bits_per_word, len);
		return -EINVAL;
	}

	/* Prepare the TX dma transfer */
	sg_init_table(&tccspi->dma.sgtx, TCC_SPI_GDMA_SG_LEN);
	sg_dma_len(&tccspi->dma.sgtx) = len;
	sg_dma_address(&tccspi->dma.sgtx) = tccspi->tx_buf.dma_addr;

	/* Prepare the RX dma transfer */
	sg_init_table(&tccspi->dma.sgrx, TCC_SPI_GDMA_SG_LEN);
	sg_dma_len(&tccspi->dma.sgrx) = len;
	sg_dma_address(&tccspi->dma.sgrx) = tccspi->rx_buf.dma_addr;

	/* Config dma slave */
	ret = tcc_spi_dma_engine_slv_cfg(tccspi, &slave_config, bpw);
	if (ret < 0) {
		dev_err(tccspi->dev, "[ERROR][SPI] Slave config failed.\n");
		return -ENOMEM;
	}

	/* Send scatterlists */
	txdesc = dmaengine_prep_slave_sg(txchan, &tccspi->dma.sgtx,
			TCC_SPI_GDMA_SG_LEN,
			DMA_MEM_TO_DEV,
			DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (txdesc == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI] Failed preparing TX DMA Desc.\n");
		goto err_dma;
	}

	txdesc->callback = tcc_dma_engine_tx_callback;
	txdesc->callback_param = tccspi->master;

	rxdesc = dmaengine_prep_slave_sg(rxchan, &tccspi->dma.sgrx,
			TCC_SPI_GDMA_SG_LEN,
			DMA_DEV_TO_MEM,
			DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (rxdesc == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI] Failed preparing RX DMA Desc.\n");
		goto err_dma;
	}

	rxdesc->callback = tcc_dma_engine_rx_callback;
	rxdesc->callback_param = tccspi->master;

	/* GPSB half-word and byte swap settings */
	if (bpw == 4U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
				(TCC_GPSB_INTEN_SHT |
				 TCC_GPSB_INTEN_SBT |
				 TCC_GPSB_INTEN_SHR |
				 TCC_GPSB_INTEN_SBR));
	} else if (bpw == 2U) {
		TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN,
				(TCC_GPSB_INTEN_SHT |
				 TCC_GPSB_INTEN_SHR),
				(TCC_GPSB_INTEN_SBT |
				 TCC_GPSB_INTEN_SBR));
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
				(TCC_GPSB_INTEN_SHT |
				 TCC_GPSB_INTEN_SBT |
				 TCC_GPSB_INTEN_SHR |
				 TCC_GPSB_INTEN_SBR));
	}

	/* Submit desctriptors */
	cookie = dmaengine_submit(txdesc);
	ret = dma_submit_error(cookie);
	if (ret != 0) {
		dev_err(tccspi->dev, "[ERROR][SPI] TX Desc. submit error (cookie:%X)\n",
				(u32)cookie);
		goto err_dma;
	}

	cookie = dmaengine_submit(rxdesc);
	ret = dma_submit_error(cookie);
	if (ret != 0) {
		dev_err(tccspi->dev, "[ERROR][SPI] RX Desc. submit error (cookie:%X)\n",
				(u32)cookie);
		goto err_dma;
	}

	// Issue pendings
	dma_async_issue_pending(rxchan);
	dma_async_issue_pending(txchan);

	return 0;
err_dma:
	/* Stop dma */
	tcc_spi_stop_dma_engine(tccspi);
	dev_err(tccspi->dev, "[ERROR][SPI] terminate dma-engine\n");

	return -ENOMEM;
}

/* Probe dma-engine channel */
static int32_t tcc_spi_dma_engine_probe(struct platform_device *pdev,
		struct tcc_spi *tccspi)
{
	struct dma_slave_config slave_config;
	struct device *dev = &pdev->dev;
	dma_cap_mask_t mask;
	int32_t ret;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	tccspi->dma.chan_tx = dma_request_slave_channel_compat(mask,
			tcc_spi_dma_engine_filter,
			&tccspi->dma,
			dev,
			"tx");
	if (tccspi->dma.chan_tx == NULL) {
		dev_err(dev, "[ERROR][SPI] DMA TX channel request Error!(%p)\n",
				tccspi->dma.chan_tx);
		ret = -EBUSY;
		goto error;
	}

	tccspi->dma.chan_rx = dma_request_slave_channel_compat(mask,
			tcc_spi_dma_engine_filter,
			&tccspi->dma,
			dev,
			"rx");
	if (tccspi->dma.chan_rx == NULL) {
		dev_err(dev, "[ERROR][SPI] DMA RX channel request Error!(%p)\n",
				tccspi->dma.chan_rx);
		ret = -EBUSY;
		goto error;
	}

	ret = tcc_spi_dma_engine_slv_cfg(tccspi,
			&slave_config,
			TCC_SPI_GDMA_WSIZE);
	if (ret < 0) {
		goto error;
	}
	dev_info(dev, "[INFO][SPI] DMA-engine Tx(%s) Rx(%s)\n",
			dma_chan_name(tccspi->dma.chan_tx),
			dma_chan_name(tccspi->dma.chan_rx));

	return 0;

error:
	tcc_spi_release_dma_engine(tccspi);
	return ret;
}
#else
static void tcc_spi_release_dma_engine(struct tcc_spi *tccspi)
{
}
static int32_t tcc_spi_dma_engine_submit(struct tcc_spi *tccspi, u32 flen)
{
	return -EPERM;
}
static void tcc_spi_stop_dma_engine(struct tcc_spi *tccspi)
{
}

static int32_t tcc_spi_dma_engine_probe(struct platform_device *pdev,
		struct tcc_spi *tccspi)
{
	return -EPERM;
}
#endif

/*
 * SPI API
 */

/* SPI setsup */
static int32_t tcc_spi_setup(struct spi_device *spi)
{
	int32_t status;
	struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);
	uint32_t bits;
	bool valid_gpio;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] tcc_spi data is not exist\n");
		return -ENXIO;
	}
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	if (spi->controller_state == NULL) {
		spi->controller_state = (void *)true;

		valid_gpio = gpio_is_valid(spi->cs_gpio);
		/* Initialize cs gpio */
		if ((spi->master->cs_gpios != NULL) && valid_gpio) {
			char cs_name[28];
			int32_t ret, cs_value;

			sprintf(cs_name,
					"tcc_spi_cs_gpio_%d",
					spi->chip_select);
			ret = gpio_request(spi->cs_gpio, cs_name);
			if (ret != 0) {
				dev_err(&spi->dev, "[ERROR][SPI] gpio_request err (gpio %d ret %d)\n",
						spi->cs_gpio, ret);
			}
			if ((spi->mode & SPI_CS_HIGH) == 0U) {
				cs_value = 1;
			} else {
				cs_value = 0;
			}
			gpio_direction_output(spi->cs_gpio, cs_value);
		}
	}

	if (spi->chip_select > spi->master->num_chipselect) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] setup: invalid chipselect %u (%u defined)\n",
				spi->chip_select, spi->master->num_chipselect);
		return -EINVAL;
	}

	/* Set the bitwidth */
	bits = spi->bits_per_word;
	tcc_spi_set_bit_width(tccspi, bits);

	/* Set spi mode (clock and loopback)*/
	status = tcc_spi_set_mode(tccspi, spi->mode);
	if (status < 0) {
		return status;
	}

	/* Set the clock */
	status = tcc_spi_set_clk(tccspi, spi->max_speed_hz, 0, 1);
	if (status < 0) {
		return status;
	}
	return 0;
}

/* SPI master cleanup */
static void tcc_spi_cleanup(struct spi_device *spi)
{
	struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);
	bool ret;

	if (spi->controller_state == NULL) {
		return;
	}
	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tcc_spi is null\n", __func__);
		return;
	}
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	ret = gpio_is_valid(spi->cs_gpio);
	if (ret) {
		gpio_free(spi->cs_gpio);
	}
	spi->controller_state = NULL;
}

/* Control CS by GPSB */
static void tcc_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);
	bool cs_active;

	cs_active = enable;
	/* When do not use cs-gpios, SPI_CS_HIGH is set when call spi_setup() */
	if ((spi->mode & SPI_CS_HIGH) != 0U) {
		cs_active = !cs_active;
	}

	if (!cs_active) {
		if (tccspi->pd->ctf) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_CTF);
		}
		if (!tccspi->pd->contm_support) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_EN);
		}
	} else {
		if (tccspi->pd->ctf) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_CTF);
		}
		if (!tccspi->pd->contm_support) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_EN);
		}
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] contm %d en %d\n",
			__func__, tccspi->pd->contm_support, cs_active);
}

/* Hanlde one spi_transfer */
static int32_t tcc_spi_transfer_one(struct spi_master *master,
		struct spi_device *spi,
		struct spi_transfer *xfer)
{
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	int32_t ret, gdma_enable, status = 0;
	uint32_t timeout_ms;
	bool is_slave;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	is_slave = spi_controller_is_slave(tccspi->master);

	if (xfer == NULL) {
		dev_err(tccspi->dev, "[ERROR][SPI] xfer is null\n");
		status = -EINVAL;
		goto exit;
	}

	if (xfer->len == 0U) {
		dev_warn(tccspi->dev, "[WARN][SPI] xfer length is zero\n");
		status = -EINVAL;
		goto exit;
	}

	/* Get SPI transfer speed (in Hz)*/
	if (xfer->speed_hz != spi->max_speed_hz) {
		tccspi->clk = xfer->speed_hz;
	} else {
		tccspi->clk = spi->max_speed_hz;
	}
	ret = tcc_spi_set_clk(tccspi, tccspi->clk, 0, 1);
	if (ret != 0) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] %s: fail to set clk\n", __func__);
		status = ret;
		goto exit;
	}

	/* Get Bit width */
	if (xfer->bits_per_word != spi->bits_per_word) {
		tccspi->bits = xfer->bits_per_word;
	} else {
		tccspi->bits = spi->bits_per_word;
	}
	tcc_spi_set_bit_width(tccspi, tccspi->bits);

	tccspi->cur_rx_pos = 0;
	tccspi->cur_tx_pos = 0;
	tccspi->current_xfer = xfer;
	tccspi->current_remaining_bytes = xfer->len;

	/* Check GDMA enable */
	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	dev_dbg(tccspi->dev, "[DEBUG][SPI][%s] xfer length %d\n",
			__func__, xfer->len);

	while (tccspi->current_remaining_bytes > 0U) {
		uint32_t len;

		reinit_completion(&tccspi->xfer_complete);

		/* Set Packet size (packet count = 0) */
		if (tccspi->current_remaining_bytes > tccspi->dma_buf_size) {
			len = (u32)tccspi->dma_buf_size;
			tccspi->tx_packet_remain = 1;
			dev_dbg(tccspi->dev,
					"[DEBUG][SPI] remain %d xfer %d\n",
					tccspi->current_remaining_bytes,
					len);
		} else {
			len = tccspi->current_remaining_bytes;
			tccspi->tx_packet_remain = 0;
			dev_dbg(tccspi->dev,
					"[DEBUG][SPI] xfer %d\n",
					len);
		}

		/* Set TXBASE and RXBASE registers */
		tcc_spi_set_dma_addr(tccspi,
				tccspi->tx_buf.dma_addr,
				tccspi->rx_buf.dma_addr);

		/* Copy client txbuf to spi txbuf */
		tcc_spi_txbuf_copy_client_to_spi(tccspi, xfer, len);

		/* Reset FIFO and Packet counter */
		tcc_spi_clear_fifo(tccspi);
		tcc_spi_clear_packet_cnt(tccspi);
		tcc_spi_set_packet_size(tccspi, len);

		/* Setup GDMA, if use */
		if (gdma_enable != 0) {
			/* Submit dma engine descriptor */
			ret = tcc_spi_dma_engine_submit(tccspi, len);
			if (ret < 0) {
				dev_err(tccspi->dev,
						"[ERROR][SPI] spi dma transfer err %d\n",
						ret);
				status = ret;
				tcc_spi_stop_dma_engine(tccspi);

				goto exit;
			}
		}

		tcc_spi_regs_dump(tccspi);
		/* Start Tx and Rx DMA operation */
		tcc_spi_start_dma(tccspi);

		/* calculate timeout */
		timeout_ms = ((len << 3U) * 1000U) / tccspi->clk;
		timeout_ms += 100U; /* some tolerance */

		/* Wait until transfer is finished */
		if (is_slave) {
			ret = wait_for_completion_interruptible(
					&tccspi->xfer_complete);
			if (ret == 0) {
				ret = 1;
			}
		} else {
			ret = wait_for_completion_timeout(
					&tccspi->xfer_complete,
					msecs_to_jiffies(timeout_ms));
		}

		if (ret == 0) {
			dev_err(tccspi->dev,
					"[ERROR][SPI] [%s] spi interrupted or trasfer timeout (%d ms), err %d\n",
					__func__, timeout_ms, ret);
			tcc_spi_regs_dump(tccspi);
			status = -ETIME;

			if (gdma_enable != 0) {
				tcc_spi_stop_dma_engine(tccspi);
			}
			tcc_spi_stop_dma(tccspi);

			goto exit;
		} else {
			status = 0;
		}

		/* Copy spi rxbuf to client rxbuf */
		tcc_spi_rxbuf_copy_client_to_spi(tccspi, xfer, len);

		if (tccspi->current_remaining_bytes < len) {
			dev_err(tccspi->dev,
					"[ERROR][SPI]%s: remaining bytes (%d) < xfered len (%d)\n",
					__func__,
					tccspi->current_remaining_bytes,
					len);
			status = -EIO;
			goto exit;
		}
		tccspi->current_remaining_bytes -= len;
		dev_dbg(tccspi->dev, "[DEBUG][SPI] completed remain %d xfered %d\n",
				tccspi->current_remaining_bytes, len);
	}

exit:
	return status;
}

static int32_t tcc_spi_init(struct tcc_spi *tccspi)
{
	int32_t status;
	uint32_t ac_val[2] = {0,};
	struct device_node *np = tccspi->dev->of_node;

	/* Check CONTM support */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_EVTCTRL,
			TCC_GPSB_EVTCTRL_CONTM(0x3U));
	tccspi->pd->contm_support = (bool)true;

	/* access control */
	if (!IS_ERR(tccspi->ac)) {
		if (of_property_read_u32_array(np,
					"access-control0", ac_val, 2) == 0) {
			dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control0 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC0_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC0_LIMIT);
		}
		if (of_property_read_u32_array(np,
					"access-control1", ac_val, 2) == 0) {
			dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control1 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC1_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC1_LIMIT);
		}
		if (of_property_read_u32_array(np,
					"access-control2", ac_val, 2) == 0) {
			dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control2 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC2_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC2_LIMIT);
		}
		if (of_property_read_u32_array(np,
					"access-control3", ac_val, 2) == 0) {
			dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control3 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC3_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC3_LIMIT);
		}
	}

	/* Initialize GPSB registers */
	tcc_spi_clear_fifo(tccspi);
	tcc_spi_clear_packet_cnt(tccspi);
	tcc_spi_stop_dma(tccspi);
	tcc_spi_hwinit(tccspi);

	/* Set port configuration */
	tcc_spi_set_port(tccspi);

	/* Enable clock */
	if (tccspi->pd->hclk != NULL)  {
		status = clk_prepare_enable(tccspi->pd->hclk);
		if (status < 0) {
			dev_err(tccspi->dev, "[ERROR][SPI] Failed to enable hclk\n");
			clk_disable_unprepare(tccspi->pd->hclk);
			return status;
		}
	}
	if (tccspi->pd->pclk != NULL)  {
		status = clk_prepare_enable(tccspi->pd->pclk);
		if (status < 0) {
			dev_err(tccspi->dev, "[ERROR][SPI] Failed to enable pclk\n");
			clk_disable_unprepare(tccspi->pd->pclk);
			clk_disable_unprepare(tccspi->pd->hclk);
			return status;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static int32_t tcc_spi_get_gpsb_ch(struct tcc_spi_pl_data *pd)
{
	const char *gpsb_name = pd->name;
	int32_t gpsb_ch, ret;

	ret = sscanf(gpsb_name, "gpsb%d", &gpsb_ch);
	if (ret != 1) {
		return -EINVAL;
	}
	if ((gpsb_ch < 0) || (gpsb_ch >= TCC_GPSB_MAX_CH)) {
		return -EINVAL;
	}

	return gpsb_ch;
}

static struct tcc_spi_pl_data *tcc_spi_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct tcc_spi_pl_data *pd;
	int32_t status;

	if (np == NULL) {
		dev_err(dev, "[ERROR][SPI] no dt node defined\n");
		return NULL;
	}

	pd = devm_kzalloc(dev, sizeof(struct tcc_spi_pl_data), GFP_KERNEL);
	if (pd == NULL) {
		return NULL;
	}
	/* Get the bus id */
	pd->id = 0xFU;
	status = of_property_read_u32(np, "gpsb-id", &pd->id);
	if (status != 0) {
		dev_err(dev,
				"[ERROR][SPI] No SPI id.\n");
		return NULL;
	}

	/* Get the driver name */
	pd->name = np->name;
	pd->gpsb_channel = tcc_spi_get_gpsb_ch(pd);
	if (pd->gpsb_channel < 0) {
		dev_err(dev, "[ERROR][SPI] wrong spi driver name (%s)\n",
				np->name);
		return NULL;
	}

	/* Get the port number */
#ifdef TCC_USE_GFB_PORT
	size_t property_size;

	/* Port array order : [0]SDI [1]SCLK [2]SFRM [3]SDO */
	property_size = (size_t)of_property_count_elems_of_size(np,
			"gpsb-port",
			sizeof(u32));
	status = of_property_read_u32_array(np,
			"gpsb-port",
			pd->port,
			property_size);
	if (status != 0) {
		dev_err(dev,
				"[ERROR][SPI] No SPI port info.\n");
		return NULL;
	}

	dev_info(dev, "[INFO][SPI] sdi: %d sclk: %d sfrm: %d sdo: %d\n",
			pd->port[0], pd->port[1],
			pd->port[2], pd->port[3]);
#else
	status = of_property_read_u32(np, "gpsb-port", &pd->port);
	if (status != 0) {
		dev_err(dev,
				"[ERROR][SPI] No SPI port info.\n");
		return NULL;
	}
	dev_info(dev, "[INFO][SPI] port %d\n", pd->port);
#endif

	if (of_property_read_bool(np, "ctf-mode-disable")) {
		pd->ctf = (bool)false;
	} else {
		pd->ctf = (bool)true;
	}

	pd->is_slave = of_property_read_bool(np, "spi-slave");
	if (pd->is_slave) {
		dev_info(dev,
				"[INFO][SPI] GPSB Slave ctf mode: %d\n",
				pd->ctf);
	} else {
		dev_info(dev,
				"[INFO][SPI] GPSB Master ctf mode: %d\n",
				pd->ctf);
	}
	/*
	 * TCC GPSB CH 3-5 don't have dedicated dma
	 * GPSB CH 3-5 should use gdma(dma-engine)
	 */
	if (pd->gpsb_channel > 2) {
		pd->dma_enable = 1;
		dev_dbg(dev, "[DEBUG][SPI] gpsb ch %d - use gdma\n",
				pd->gpsb_channel);
	} else {
		pd->dma_enable = 0;
	}

	/* Get the hclk and pclk */
	pd->pclk = of_clk_get(np, 0);
	if (IS_ERR(pd->pclk)) {
		dev_err(dev,
				"[ERROR][SPI] No SPI clock info.\n");
		return NULL;
	}

	pd->hclk = of_clk_get(np, 1);
	if (IS_ERR(pd->hclk)) {
		dev_err(dev,
				"[ERROR][SPI] No SPI clock info.\n");
		return NULL;
	}

	return pd;
}

#else
static struct tcc_spi_pl_data *tcc_spi_parse_dt(struct device *dev)
{
	return NULL;
}
#endif

/* Probe spi master driver */
static int32_t tcc_spi_probe(struct platform_device *pdev)
{
	int32_t gdma_enable, ret;
	struct device	*dev = &pdev->dev;
	struct resource *regs;
	struct spi_master *master;
	struct tcc_spi		*tccspi;
	struct tcc_spi_pl_data *pd;

	if (pdev->dev.of_node == NULL) {
		return -EINVAL;
	}
	dev_dbg(dev, "[DEBUG][SPI] [%s]\n", __func__);

	/* Get TCC GPSB SPI master platform data */
	pd = tcc_spi_parse_dt(dev);
	if (pd == NULL) {
		dev_err(dev,
				"[ERROR][SPI] No TCC SPI master platform data.\n");
		return -ENODEV;
	}

	/* allocate master or slave controller */
	if (pd->is_slave) {
		master = spi_alloc_slave(dev, sizeof(struct tcc_spi));
	} else {
		master = spi_alloc_master(dev, sizeof(struct tcc_spi));
	}
	if (master == NULL) {
		dev_err(dev,
				"[ERROR][SPI] SPI memory allocation failed.\n");
		return -ENOMEM;
	}

	/* the spi->mode bits understood by this driver: */
	master->bus_num = (s16)pd->id;
	/* master->num_chipselect = 1. */
	master->mode_bits = TCC_SPI_MODE_BITS;
	master->bits_per_word_mask =
		SPI_BPW_MASK(32) | SPI_BPW_MASK(16) | SPI_BPW_MASK(8);
	master->dev.of_node = dev->of_node;
	master->max_speed_hz = TCC_GPSB_MAX_FREQ;
	master->setup = tcc_spi_setup;
	master->transfer_one = tcc_spi_transfer_one;
	master->cleanup = tcc_spi_cleanup;
	master->set_cs = tcc_spi_set_cs;

	platform_set_drvdata(pdev, master);

	tccspi = spi_master_get_devdata(master);

	/* Initialize tcc_spi */
	tccspi->dev = dev;
	tccspi->master = master;
	tccspi->pd = pd;

	spin_lock_init(&(tccspi->lock));

	/* Set device dma coherent mask */
	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(32));
	if (ret != 0) {
		dev_err(dev,
				"[ERROR][SPI] Failed to set dma mask\n");
		goto exit_free_master;
	}
	if (dev->dma_mask == NULL) {
		dev->dma_mask = &dev->coherent_dma_mask;
	} else {
		dma_set_mask(dev, DMA_BIT_MASK(32));
	}
	/* Get TCC GPSB SPI master base address */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (regs == NULL) {
		dev_err(dev,
				"[ERROR][SPI] No SPI base register addr.\n");
		ret = -ENXIO;
		goto exit_free_master;
	}

	tccspi->pbase = regs->start;
	tccspi->base = devm_ioremap_resource(dev, regs);

	/* Get TCC GPSB SPI master port configuration reg. address */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (regs == NULL) {
		dev_err(dev,
				"[ERROR][SPI] No SPI PCFG register addr.\n");
		ret = -ENXIO;
		goto exit_free_master;
	}

	tccspi->pcfg = of_iomap(pdev->dev.of_node, 1);
	/* Configure the AHB Access Filter */
	tccspi->ac = of_iomap(pdev->dev.of_node, 2);

	/* initialize GPSB */
	ret = tcc_spi_init(tccspi);
	if (ret < 0) {
		dev_err(dev,
				"[ERROR][SPI] %s: failed to init GPSB\n",
				__func__);
		goto exit_free_master;
	}

	/* Get pin control (active state)*/
	tccspi->pinctrl = devm_pinctrl_get_select(tccspi->dev, "active");
	if (IS_ERR(tccspi->pinctrl)) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to get pinctrl (active state)\n");
		goto exit_unprepare_clk;
	}

	/* Get TCC GPSB IRQ number */
	tccspi->irq = -1;
	tccspi->irq = platform_get_irq(pdev, 0);
	if (tccspi->irq < 0) {
		dev_err(dev,
				"[ERROR][SPI] No SPI IRQ Number.\n");
		ret = -ENXIO;
		goto exit_unprepare_clk;
	}

	/* Get GDMA data */
	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable != 0) {
		ret = tcc_spi_dma_engine_probe(pdev, tccspi);
		if (ret < 0) {
			dev_err(dev,
					"[ERROR][SPI] Failed to get dma-engine\n");
			goto exit_unprepare_clk;
		}
	} else {
		ret = devm_request_irq(dev,
				(u32)tccspi->irq,
				tcc_spi_dma_irq,
				IRQF_SHARED,
				dev_name(dev),
				master);
		if (ret < 0) {
			dev_err(dev,
					"[ERROR][SPI] Failed to enter irq handler\n");
			ret = -ENXIO;
			goto exit_unprepare_clk;
		}
	}

	/* Allocate SPI master dma buffer (rx) */
	tccspi->dma_buf_size = TCC_SPI_DMA_MAX_SIZE;
	ret = tcc_spi_init_dma_buf(tccspi, 1);
	if (ret < 0) {
		dev_err(dev,
				"[ERROR][SPI] Failed to allocate rx dma buffer\n");
		goto exit_unprepare_clk;
	}

	/* Allocate SPI master dma buffer (tx) */
	ret = tcc_spi_init_dma_buf(tccspi, 0);
	if (ret < 0) {
		dev_err(dev,
				"[ERROR][SPI] Failed to allocate tx dma buffer\n");
		goto exit_rx_dma_free;
	}

	/* Initialize xfer completion */
	init_completion(&tccspi->xfer_complete);

	platform_set_drvdata(pdev, master);

	ret = devm_spi_register_master(dev, master);
	if (ret != 0) {
		dev_err(dev,
				"[ERROR][SPI] No SPI IRQ Number.\n");
		goto exit_tx_dma_free;
	}

#ifdef TCC_USE_GFB_PORT
	dev_info(dev, "[INFO][SPI] TCC SPI [%s] Id: %d [ch:%d][port: %d %d %d %d][irq: %d][CONTM: %d][gdma: %d][cs_num: %d]\n",
			pd->name,
			master->bus_num,
			pd->gpsb_channel,
			pd->port[0], pd->port[1], pd->port[2], pd->port[3],
			tccspi->irq,
			tccspi->pd->contm_support,
			tcc_spi_is_use_gdma(tccspi),
			master->num_chipselect);
#else
	dev_info(dev, "[INFO][SPI] TCC SPI [%s] Id: %d [ch:%d][port: %d][irq: %d][CONTM: %d][gdma: %d][cs_num: %d]\n",
			pd->name,
			master->bus_num,
			pd->gpsb_channel,
			pd->port,
			tccspi->irq,
			tccspi->pd->contm_support,
			tcc_spi_is_use_gdma(tccspi),
			master->num_chipselect);
#endif

	return ret;

exit_tx_dma_free:
	tcc_spi_deinit_dma_buf(tccspi, 0);
exit_rx_dma_free:
	tcc_spi_deinit_dma_buf(tccspi, 1);
exit_unprepare_clk:
	clk_disable_unprepare(pd->pclk);
	clk_disable_unprepare(pd->hclk);
exit_free_master:
	spi_master_put(master);
	return ret;
}

static int32_t tcc_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	struct tcc_spi_pl_data *pd;
	ulong flags;
	int32_t gdma_enable;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	pd = tcc_spi_get_pl_data(tccspi);

	/* Get pin control (idle state)*/
	tccspi->pinctrl = devm_pinctrl_get_select(tccspi->dev, "idle");
	if (IS_ERR(tccspi->pinctrl)) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to get pinctrl (idle state)\n");
		return -ENODEV;
	}

	/* Disable clock */
	spin_lock_irqsave(&(tccspi->lock), flags);
	if (pd->pclk != NULL) {
		clk_disable_unprepare(pd->pclk);
	}
	if (pd->hclk != NULL) {
		clk_disable_unprepare(pd->hclk);
	}
	spin_unlock_irqrestore(&(tccspi->lock), flags);

	/* Release DMA buffers */
	if (tccspi->rx_buf.v_addr != NULL) {
		tcc_spi_deinit_dma_buf(tccspi, 1);
	}
	if (tccspi->tx_buf.v_addr != NULL) {
		tcc_spi_deinit_dma_buf(tccspi, 0);
	}
	gdma_enable = tcc_spi_is_use_gdma(tccspi);
	if (gdma_enable != 0) {
		tcc_spi_release_dma_engine(tccspi);
	}
	iounmap(tccspi->pcfg);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_spi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	struct tcc_spi_pl_data *pd;
	ulong flags;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	pd = tcc_spi_get_pl_data(tccspi);

	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);
	tcc_spi_stop_dma(tccspi);

	spin_lock_irqsave(&(tccspi->lock), flags);
	if (pd->pclk != NULL) {
		clk_disable_unprepare(pd->pclk);
	}
	if (pd->hclk != NULL) {
		clk_disable_unprepare(pd->hclk);
	}
	spin_unlock_irqrestore(&(tccspi->lock), flags);

	return spi_master_suspend(master);
}

static int32_t tcc_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	ulong flags;
	int32_t status;

	if (tccspi == NULL) {
		pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	/* Initialize GPSB */
	spin_lock_irqsave(&(tccspi->lock), flags);
	status = tcc_spi_init(tccspi);
	spin_unlock_irqrestore(&(tccspi->lock), flags);

	if (status < 0) {
		dev_err(dev,
				"[ERROR][SPI] %s: failed to init GPSB\n",
				__func__);
		return status;
	}

	/* Get pin control (active state)*/
	tccspi->pinctrl = devm_pinctrl_get_select(tccspi->dev, "active");
	if (IS_ERR(tccspi->pinctrl)) {
		dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to get pinctrl (active state)\n");
		return -ENXIO;
	}

	return spi_master_resume(master);
}

static const struct dev_pm_ops tcc_spi_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_spi_suspend, tcc_spi_resume)
};

#define TCC_SPI_PM	(&tcc_spi_pmops)
#else
#define TCC_SPI_PM	(NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_spi_of_match[] = {
	{ .compatible = "telechips,tcc-spi" },
	{ .compatible = "telechips,tcc-spi-slave" },
	{ .compatible = "telechips,tcc805x-spi" },
	{ .compatible = "telechips,tcc803x-spi" },
	{ .compatible = "telechips,tcc897x-spi" },
	{ .compatible = "telechips,tcc899x-spi" },
	{ .compatible = "telechips,tcc901x-spi" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_spi_of_match);
#endif

static struct platform_driver tcc_spidrv = {
	.probe		= tcc_spi_probe,
	.remove		= tcc_spi_remove,
	.driver		= {
		.name		= "tcc-spi",
		.owner		= THIS_MODULE,
		.pm = TCC_SPI_PM,
		.of_match_table = of_match_ptr(tcc_spi_of_match)
	},
};

module_platform_driver(tcc_spidrv);

MODULE_AUTHOR("Telechips Inc. linux@telechips.com");
MODULE_DESCRIPTION("Telechips GPSB SPI Driver");
MODULE_LICENSE("GPL");
