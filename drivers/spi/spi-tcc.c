/*
 * linux/drivers/spi/spi-tcc.c
 *
 * Copyright (C) 2017 Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

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

#include <asm/io.h>
#include <asm/dma.h>
#include <linux/module.h>

#include <linux/bitops.h>

#include "spi-tcc.h"

/* Get tcc spi platform data */
static struct tcc_spi_pl_data *tcc_spi_get_pl_data(struct tcc_spi *tccspi)
{
	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tcc_spi is null!\n", __func__);
		return NULL;
	}

	return (struct tcc_spi_pl_data *)tccspi->pd;
}

/* 0: Dedicated DMA 1: GDMA */
static int tcc_spi_is_use_gdma(struct tcc_spi *tccspi)
{
	if(!tccspi->pd)
		return 0;

	return tccspi->pd->dma_enable;
}

/* Print values of GPSB registers */
#ifdef DEBUG
static void tcc_spi_regs_dump(struct tcc_spi *tccspi)
{
	void __iomem *base = NULL;
	struct tcc_spi_pl_data *pd;
	int gpsb_channel = -1;

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	pd = tcc_spi_get_pl_data(tccspi);
	if(pd)
		gpsb_channel = pd->gpsb_channel;

	base = tccspi->base;
	dev_dbg(tccspi->dev, "[DEBUG][SPI] ##\tGPSB REGS DUMP [CH: %d][@%X]\t##\n", gpsb_channel, (unsigned int)base);
	dev_dbg(tccspi->dev, "[DEBUG][SPI] STAT\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_STAT));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] INTEN\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_INTEN));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] MODE\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_MODE));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] CTRL\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_CTRL));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] EVTCTRL\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_EVTCTRL));
	dev_dbg(tccspi->dev, "[DEBUG][SPI] CCV\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_CCV));

	if(!tcc_spi_is_use_gdma(tccspi)) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] TXBASE\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_TXBASE));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] RXBASE\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_RXBASE));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] PACKET\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_PACKET));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] DMACTR\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_DMACTR));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] DMASTR\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_DMASTR));
		dev_dbg(tccspi->dev, "[DEBUG][SPI] DMAICR\t: 0x%08X\n", tcc_spi_readl(base + TCC_GPSB_DMAICR));
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
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	if(!tcc_spi_is_use_gdma(tccspi)) {
		/* Clear GPSB DMA Packet counter */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_PCLR);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_PCLR);
	}
}

/* Set TCC GPSB DMA Packet counter */
static void tcc_spi_set_packet_size(struct tcc_spi *tccspi, unsigned int size)
{
	if(!tcc_spi_is_use_gdma(tccspi)) {
		tcc_spi_writel(TCC_GPSB_PACKET_SIZE(size), tccspi->base + TCC_GPSB_PACKET);
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] size: %d packet reg 0x%08X\n",
			__func__, size, tcc_spi_readl(tccspi->base + TCC_GPSB_PACKET));
	}
}

/* Clear Tx and Rx FIFO counter */
static void tcc_spi_clear_fifo(struct tcc_spi *tccspi)
{
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	/* Clear Tx and Rx FIFO counter */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_CRF | TCC_GPSB_MODE_CWF);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_CRF | TCC_GPSB_MODE_CWF);
}

#ifdef TCC_USE_GFB_PORT
static void tcc_spi_set_port(struct tcc_spi *tccspi)
{
	int i;
	unsigned int pcfg_offset;
	struct tcc_spi_pl_data *pd = tcc_spi_get_pl_data(tccspi);

	if(!pd) {
		dev_err(tccspi->dev, "[ERROR][SPI] [%s] tcc_pl_data is not exist",__func__);
		return;
	}

	pcfg_offset = TCC_GPSB_PCFG_OFFSET * pd->gpsb_channel;

	for(i=0;i<4;i++) {
		TCC_GPSB_BITCSET(tccspi->pcfg + pcfg_offset, (0xFF)<<(i *TCC_GPSB_PCFG_SHIFT), pd->port[i] << (i *TCC_GPSB_PCFG_SHIFT));
	}

	dev_info(tccspi->dev,
		"[INFO][SPI] ch %d pcfg(@0x%08X) 0x%08X\n",
		pd->gpsb_channel, (unsigned int)(tccspi->pcfg + pcfg_offset),
		tcc_spi_readl(tccspi->pcfg + pcfg_offset));
}
#else
static void TCC_GPSB_PCFG_CSET(struct tcc_spi *tccspi, int ch, int cmsk, int smsk)
{
	if(ch <= 3)
		TCC_GPSB_BITCSET(tccspi->pcfg + TCC_GPSB_PCFG0, ((cmsk & 0xFF)<< (ch * 8)), ((smsk & 0xFF) << (ch * 8)) );
	else
		TCC_GPSB_BITCSET(tccspi->pcfg + TCC_GPSB_PCFG1, ((cmsk & 0xFF)<< ((ch-4)* 8)), ((smsk & 0xFF) << ((ch-4) * 8)) );

}

/* Clear on port configuration */
static void tcc_spi_clear_port(struct tcc_spi *tccspi, int channel)
{
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);
	TCC_GPSB_PCFG_CSET(tccspi, channel, 0xFF, 0xFF);
}

/* Get the port configuration */
static int tcc_spi_get_port(struct tcc_spi *tccspi, int channel)
{
	int ret = -1;

	if(channel <= 3)
		ret = (tcc_spi_readl(tccspi->pcfg + TCC_GPSB_PCFG0) >> (channel * 8));
	else
		ret = (tcc_spi_readl(tccspi->pcfg + TCC_GPSB_PCFG1) >> ((channel-4) * 8));

	ret &= 0xFF;
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] channel: %d port: %d\n", __func__, channel, ret);

	return ret & 0xFF;
}

/* Set the port configuration and check the port setting conflict */
static void tcc_spi_set_port(struct tcc_spi *tccspi)
{
	int channel, port, i, _port;
	struct tcc_spi_pl_data *pd = tcc_spi_get_pl_data(tccspi);

	if(!pd) {
		dev_err(tccspi->dev, "[ERROR][SPI] [%s] tcc_pl_data is not exist\n",__func__);
		return;
	}

	channel = pd->gpsb_channel;
	port = pd->port;

	if((channel < 0) || (port < 0)) {
		dev_err(tccspi->dev, "[ERROR][SPI] channel(%d), port(%d) is wrong number !\n", channel, port);
		return;
	}

	/* Set the port */
	TCC_GPSB_PCFG_CSET(tccspi,channel,0xFF,port);
	dev_info(tccspi->dev, "[INFO][SPI] set port config. ch - %d port - %d\n",channel, port);

	/* Check the port setting conflict */
	for(i=0;i<TCC_GPSB_MAX_CH;i++) {
		if(i == channel) continue;

		_port = tcc_spi_get_port(tccspi, i);
		if(_port == port) {
			tcc_spi_clear_port(tccspi, i);
			dev_warn(tccspi->dev,
				"[WARN][SPI] port conflict! [[ch %d[%d]]] : ch %d[%d] pcfg0: 0x%08X pcfg1: 0x%08X\n",
				channel, port, i, _port,
				tcc_spi_readl(tccspi->pcfg + TCC_GPSB_PCFG0),
				tcc_spi_readl(tccspi->pcfg + TCC_GPSB_PCFG1));
		}
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] pcfg0: 0x%08X pcfg1: 0x%08X\n",
		__func__,
		tcc_spi_readl(tccspi->pcfg + TCC_GPSB_PCFG0),
		tcc_spi_readl(tccspi->pcfg + TCC_GPSB_PCFG1)
		);

	return;
}
#endif

/* Set the bit width */
static void tcc_spi_set_bit_width(struct tcc_spi *tccspi, unsigned int width)
{
	int val = (width -1) & 0x1F;

	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_BWS(0xFF));
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_BWS(val));

	if(!tcc_spi_is_use_gdma(tccspi))
	{
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
			(TCC_GPSB_INTEN_SHT | TCC_GPSB_INTEN_SBT | TCC_GPSB_INTEN_SHR | TCC_GPSB_INTEN_SBR) );
		/* Set the endian mode according to the bit-width */
		if(val & BIT(4)) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_END);
		}
		else {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_END);
			if(width == 16) {
				TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
					(TCC_GPSB_INTEN_SBT | TCC_GPSB_INTEN_SBR) );
			}
		}
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] bitwidth: %d endian: %d\n",
		__func__, width, (int)(val & BIT(4)));
}

/* Set the SCLK */
static int tcc_spi_set_clk(struct tcc_spi *tccspi, unsigned int clk, unsigned int divldv, int enable)
{
	unsigned int pclk, _clk;
	struct tcc_spi_pl_data *pd;

	if(!enable) {
		return 0;
	}

	pd = tcc_spi_get_pl_data(tccspi);

	if(!clk) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] clk err (%u)\n",
			clk);
		return -EINVAL;
	}

	if(!pd) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] pl data is null!\n");
		return -EINVAL;
	}

	if(!pd->pclk) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] pclk is null!!\n");
		return -EINVAL;
	}

	_clk = clk;

	/* Set the GPSB clock divider load value */
	TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_MODE,TCC_GPSB_MODE_DIVLDV(0xFF), TCC_GPSB_MODE_DIVLDV(divldv));

	pclk = _clk * (divldv + 1) * 2;
	clk_set_rate(pd->pclk,pclk);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] sclk: %d Hz divldv: %d pclk: %lu Hz\n",
		__func__, _clk, divldv, clk_get_rate(pd->pclk));

	return 0;
}

/* Set SPI modes */
static int tcc_spi_set_mode(struct tcc_spi *tccspi, unsigned int mode)
{
	int slave = 0;
	slave = spi_controller_is_slave(tccspi->master);

	if(slave){
		/* slave mode */
		if((mode == SPI_MODE_1) || (mode == SPI_MODE_2)){
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PCK);
		}else{
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PCK);
		}
	}else{
		/* master mode */
		if(mode & SPI_CPOL){
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PCK);
		}else{
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PCK);
		}
		if(mode & SPI_CPHA){
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PWD | TCC_GPSB_MODE_PRD);
		}else{
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PWD | TCC_GPSB_MODE_PRD);
		}
	}

	if(mode & SPI_CS_HIGH)
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PCS | TCC_GPSB_MODE_PCD);
	else
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_PCS | TCC_GPSB_MODE_PCD);

	if(mode & SPI_LSB_FIRST)
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_SD);
	else
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_SD);

	if(mode & SPI_LOOP)
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_LB);
	else
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_LB);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] mode: 0x%X (mode reg: 0x%08X)\n",
		__func__, mode, tcc_spi_readl(tccspi->base + TCC_GPSB_MODE));

	return 0;
}

/* Initialize GPSB register settings */
static void tcc_spi_hwinit(struct tcc_spi *tccspi)
{
	/* Reset GPSB registers */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_STAT, 0xFFFFFFFF);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN, 0xFFFFFFFF);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, 0xFFFFFFFF);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_CTRL, 0xFFFFFFFF);
	if(!tcc_spi_is_use_gdma(tccspi)) {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_TXBASE, 0xFFFFFFFF);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_RXBASE, 0xFFFFFFFF);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_PACKET, 0xFFFFFFFF);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, 0xFFFFFFFF);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMASTR, 0xFFFFFFFF);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR, 0xFFFFFFFF);
	}

	/* Disable operation */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);

	/* Set bitwidth (Default: 8) */
	tcc_spi_set_bit_width(tccspi, 8);

	/* Set operation mode (SPI compatible) */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_MD_MASK);

	if(tccspi->pd->ctf){
		/* Set CTF Mode */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_CTF);
	}
	/* Set Tx and Rx FIFO threshold for interrupt/DMA request */
	TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_CFGRTH_MASK, TCC_GPSB_INTEN_CFGRTH(0));
	TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_CFGWTH_MASK, TCC_GPSB_INTEN_CFGWTH(0));

	if(spi_controller_is_slave(tccspi->master)){
		/* Set SPI slave mode */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_SLV);
		TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_CFGWTH_MASK, TCC_GPSB_INTEN_CFGWTH(7));
	}else{
		/* Set SPI master mode */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_SLV);
	}

	/* Enable operation */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);
}

/* Set TCC GPSB DMA Tx and Rx base address and DMA request */
static int tcc_spi_set_dma_addr(struct tcc_spi *tccspi, dma_addr_t tx, dma_addr_t rx)
{
	int ret  = 0;

	if(!tcc_spi_is_use_gdma(tccspi)) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] - dedicated dma\n", __func__);
		/* Set Base address */
		tcc_spi_writel((tx & 0xFFFFFFFF), tccspi->base + TCC_GPSB_TXBASE);
		tcc_spi_writel((rx & 0xFFFFFFFF), tccspi->base + TCC_GPSB_RXBASE);
	}

	/* Set DMA request */
	if(tx)
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_DW);
	else
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_DW);

	if(rx)
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_DR);
	else
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN, TCC_GPSB_INTEN_DR);

	return ret;
}

/* Stop transfer */
static void tcc_spi_stop_dma(struct tcc_spi *tccspi)
{
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	if(!tcc_spi_is_use_gdma(tccspi)) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] - dedicated dma\n", __func__);
		/* Disable DMA Tx and Rx request */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_DTE | TCC_GPSB_DMACTR_DRE);

		/* Clear DMA done and packet interrupt status */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMAICR, TCC_GPSB_DMAICR_ISD | TCC_GPSB_DMAICR_ISP);

		/* Disable GPSB DMA operation */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_EN);
	}
}

/* Start transfer */
static void tcc_spi_start_dma(struct tcc_spi *tccspi)
{
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	if(!tcc_spi_is_use_gdma(tccspi)) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] - dedicated dma\n", __func__);
		/* Set GPSB DMA address mode (Multiple address mode) */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_RXAM_MASK | TCC_GPSB_DMACTR_TXAM_MASK);

		/* Enable DMA Tx and Rx request */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_DTE | TCC_GPSB_DMACTR_DRE);

		/* Enable DMA done interrupt */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMAICR, TCC_GPSB_DMAICR_IED);

		/* Disable DMA packet interrupt */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR, TCC_GPSB_DMAICR_IEP);

		/* Set DMA Rx interrupt */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR, TCC_GPSB_DMAICR_IRQS);

		/* Enable GPSB DMA operation */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR, TCC_GPSB_DMACTR_EN);
	}
}

/* Allocate dma buffer */
static int tcc_spi_init_dma_buf(struct tcc_spi *tccspi, int dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;

	dev = tccspi->dev;

	v_addr = dma_alloc_writecombine(dev, tccspi->dma_buf_size, &dma_addr, GFP_KERNEL);
	//v_addr = dma_alloc_coherent(dev, tccspi->dma_buf_size, &dma_addr, GFP_KERNEL);
	if(!v_addr) {
		dev_err(tccspi->dev, "[ERROR][SPI] Fail to allocate the dma buffer\n");
		return -ENOMEM;
	}

	if(dma_to_mem) {
		tccspi->rx_buf.v_addr = v_addr;
		tccspi->rx_buf.dma_addr = dma_addr;
		tccspi->rx_buf.size = tccspi->dma_buf_size;
	} else {
		tccspi->tx_buf.v_addr = v_addr;
		tccspi->tx_buf.dma_addr = dma_addr;
		tccspi->tx_buf.size = tccspi->dma_buf_size;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] dma_to_mem: %d v_addr: 0x%08X dma_addr: 0x%08X size: %d\n",
		__func__, dma_to_mem,
		(unsigned int)v_addr, (unsigned int)dma_addr, tccspi->dma_buf_size);

	return 0;
}

/* De-allocate dma buffer */
static void tcc_spi_deinit_dma_buf(struct tcc_spi *tccspi, int dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;

	dev = tccspi->dev;

	if(dma_to_mem) {
		v_addr = tccspi->rx_buf.v_addr;
		dma_addr = tccspi->rx_buf.dma_addr;
	} else {
		v_addr = tccspi->tx_buf.v_addr;
		dma_addr = tccspi->tx_buf.dma_addr;
	}

	dma_free_writecombine(tccspi->dev, tccspi->dma_buf_size, v_addr, dma_addr);
	//dma_free_coherent(tccspi->dev, tccspi->dma_buf_size, v_addr, dma_addr);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] dma_to_mem: %d v_addr: 0x%08X dma_addr: 0x%08X size: %d\n",
		__func__, dma_to_mem,
		(unsigned int)v_addr, (unsigned int)dma_addr, tccspi->dma_buf_size);

}

/* Copy client buf to spi bux (tx) */
static void tcc_spi_txbuf_copy_client_to_spi(struct tcc_spi *tccspi, struct spi_transfer *xfer, unsigned int len)
{
	if(!xfer->tx_buf)
		return;

	//dma_sync_single_for_cpu(tccspi->dev, tccspi->tx_buf.dma_addr,
	//	tccspi->tx_buf.size, DMA_TO_DEVICE);

	memcpy(tccspi->tx_buf.v_addr, xfer->tx_buf + tccspi->cur_tx_pos, len);
	tccspi->cur_tx_pos += len;

	//dma_sync_single_for_device(tccspi->dev, tccspi->tx_buf.dma_addr,
	//			tccspi->tx_buf.size, DMA_TO_DEVICE);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] tx - client_buf: 0x%X offset: 0x%X spi_buf: 0x%08X\n",
		__func__, (unsigned int)xfer->tx_buf, tccspi->cur_tx_pos, (unsigned int)tccspi->tx_buf.v_addr);
}

/* Copy client buf to spi bux (rx) */
static void tcc_spi_rxbuf_copy_client_to_spi(struct tcc_spi *tccspi, struct spi_transfer *xfer, unsigned int len)
{
	if(!xfer->rx_buf)
		return;

	//dma_sync_single_for_cpu(tccspi->dev, tccspi->rx_buf.dma_addr,
	//	tccspi->rx_buf.size, DMA_FROM_DEVICE);

	memcpy(xfer->rx_buf + tccspi->cur_rx_pos, tccspi->rx_buf.v_addr, len);
	tccspi->cur_rx_pos += len;

	//dma_sync_single_for_device(tccspi->dev, tccspi->rx_buf.dma_addr,
	//			tccspi->rx_buf.size, DMA_FROM_DEVICE);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] rx - client_buf: 0x%X offset: 0x%X spi_buf: 0x%08X\n",
		__func__, (unsigned int)xfer->rx_buf, tccspi->cur_rx_pos, (unsigned int)tccspi->rx_buf.v_addr);
}

/* Check channel DMA IRQ status */
static int tcc_spi_check_dma_irq_status(struct tcc_spi *tccspi)
{
	struct tcc_spi_pl_data *pd;
	unsigned int val, ch, offset;
	int ret;

	pd = tcc_spi_get_pl_data(tccspi);
	if(!pd) {
		dev_err(tccspi->dev, "[ERROR][SPI] [%s] tcc_spi pd is null!!\n", __func__);
		return -EINVAL;
	}

	ch = pd->gpsb_channel;
	if(ch < 0) {
		dev_err(tccspi->dev, "[ERROR][SPI] [%s] gpsb channel is wrong number (ch: %d)!!\n", __func__, ch);
		return -EINVAL;
	}
	offset = (ch * 2) + 1;
	val = tcc_spi_readl(tccspi->pcfg + TCC_GPSB_CIRQST);

	/* Check dma irq status */
	ret = (((val >> offset) & 0x1) == 0);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] irq_status: %d\n", __func__, ret);

	return ret;
}

/* TCC GPSB DMA IRQ Handler */
static irqreturn_t tcc_spi_dma_irq(int irq, void *data)
{
	struct spi_master *master = data;
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	unsigned int dmaicr, status;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tccspi is null ! (irq: %d)\n", __func__, irq);
		return IRQ_NONE;
	}

	/* Check dma irq status*/
	if(tcc_spi_check_dma_irq_status(tccspi)) {
		return IRQ_NONE;
	}

	/* Check GPSB error flag status */
	status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
	if(status & 0x3E0){
		dev_warn(tccspi->dev, "[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
	}

	/* Handle DMA done interrupt */
	dmaicr = tcc_spi_readl(tccspi->base + TCC_GPSB_DMAICR);
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] dmaicr: 0x%08X\n", __func__, dmaicr);
	if(dmaicr & TCC_GPSB_DMAICR_ISD) {
		dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] irq %d - channel :%d\n", __func__, irq, tccspi->pd->gpsb_channel);

		/* Stop dma operation to hanlde buffer */
		tcc_spi_stop_dma(tccspi);
		complete(&tccspi->xfer_complete);
	}

	return IRQ_HANDLED;
}

#ifdef TCC_DMA_ENGINE
/****** DMA-engine specific ******/
#include <linux/dmaengine.h>
#define TCC_SPI_GDMA_WSIZE	1 /* Default word size */
#define TCC_SPI_GDMA_BSIZE	1 /* Default burst size */
#define TCC_SPI_GDMA_SG_LEN	1

/* Release dma-eninge channel */
static void tcc_spi_release_dma_engine(struct tcc_spi *tccspi)
{
	if(tccspi->dma.chan_tx)
	{
		dma_release_channel(tccspi->dma.chan_tx);
		tccspi->dma.chan_tx = NULL;
	}
	if(tccspi->dma.chan_rx)
	{
		dma_release_channel(tccspi->dma.chan_rx);
		tccspi->dma.chan_rx = NULL;
	}

}

/* dma-engine filter */
static bool tcc_spi_dma_engine_filter(struct dma_chan *chan, void *pdata)
{
	struct tcc_spi_gdma *tccdma = pdata;
	struct tcc_dma_slave *dma_slave;

	if(!tccdma)
	{
		printk(KERN_ERR "[ERROR][SPI] [%s] tcc_spi_gdma is NULL!!\n", __func__);
		return -EINVAL;
	}

	dma_slave = &tccdma->dma_slave;
	if(dma_slave->dma_dev == chan->device->dev) {
		chan->private = dma_slave;
		return 0;
	}
	else {
		dev_err(chan->device->dev,"[ERROR][SPI] dma_dev(%p) != dev(%p)\n",
			dma_slave->dma_dev, chan->device->dev);
		return -ENODEV;
	}
}

/* Terminate all dma-engine channel */
static void tcc_spi_stop_dma_engine(struct tcc_spi *tccspi)
{
	if(tccspi->dma.chan_tx)
		dmaengine_terminate_all(tccspi->dma.chan_tx);
	if(tccspi->dma.chan_rx)
		dmaengine_terminate_all(tccspi->dma.chan_rx);
}

/* dma-engine tx channel callback */
static void tcc_dma_engine_tx_callback(void *data)
{
	struct spi_master *master = data;
    struct tcc_spi *tccspi = spi_master_get_devdata(master);

	if(!tccspi)
	{
		printk(KERN_ERR "[ERROR][SPI] [%s] tcc_spi is NULL!!\n", __func__);
		return;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] gpsb channel %d \n", __func__, tccspi->pd->gpsb_channel);
}

/* dma-engine rx channel callback */
static void tcc_dma_engine_rx_callback(void *data)
{
	struct spi_master *master = data;
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	uint32_t status;

	if(!tccspi)
	{
		printk(KERN_ERR "[ERROR][SPI] [%s] tcc_spi is NULL!!\n", __func__);
		return;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] gpsb channel %d \n", __func__, tccspi->pd->gpsb_channel);

	/* Check GPSB error flag status */
	status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
	if(status & 0x3E0){
		dev_warn(tccspi->dev, "[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
	}

	/* Stop dma operation */
	tcc_spi_stop_dma(tccspi);
	complete(&tccspi->xfer_complete);
}

/* Set dma-engine slave config word size */
static void tcc_spi_dma_engine_slave_config_addr_width(struct dma_slave_config *slave_config, u8 bytes_per_word, int src)
{
	// Set WSIZE
	if(src)
		slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;
	else
		slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;

	if(bytes_per_word == 4)
	{
		if(src)
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		else
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	else if(bytes_per_word == 2)
	{
		if(src)
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		else
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	}
	else
	{
		if(src)
			slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		else
			slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	}
}

/* Configure dma-engine slaves */
static int tcc_spi_dma_engine_slave_config(struct tcc_spi *tccspi, struct dma_slave_config *slave_config, u8 bytes_per_word)
{
	int ret = 0;

	/* Set Busrt size(BSIZE) */
	slave_config->dst_maxburst = TCC_SPI_GDMA_BSIZE;
	slave_config->src_maxburst = TCC_SPI_GDMA_BSIZE;

	/* Set source and destincation address */
	slave_config->dst_addr = (dma_addr_t)(tccspi->pbase + TCC_GPSB_PORT);
	slave_config->src_addr = (dma_addr_t)(tccspi->pbase + TCC_GPSB_PORT);

	/* Set tx channel */
	slave_config->direction = DMA_MEM_TO_DEV;
	tcc_spi_dma_engine_slave_config_addr_width(slave_config, bytes_per_word, 1);
	if(dmaengine_slave_config(tccspi->dma.chan_tx, slave_config))
	{
		dev_err(tccspi->dev, "[ERROR][SPI] Failed to configrue tx dma channel.\n");
		ret = -EINVAL;
	}

	/* Set rx channel */
	slave_config->direction = DMA_DEV_TO_MEM;
	tcc_spi_dma_engine_slave_config_addr_width(slave_config, bytes_per_word, 0);
	if(dmaengine_slave_config(tccspi->dma.chan_rx, slave_config))
	{
		dev_err(tccspi->dev, "[ERROR][SPI] Failed to configrue rx dma channel.\n");
		ret = -EINVAL;
	}

	return ret;

}

/* Submit dma-engine descriptor */
static int tcc_spi_dma_engine_submit(struct tcc_spi *tccspi, u32 flen)
{
	struct dma_chan *txchan = tccspi->dma.chan_tx;
	struct dma_chan *rxchan = tccspi->dma.chan_rx;
	struct dma_async_tx_descriptor *txdesc;
	struct dma_async_tx_descriptor *rxdesc;
	struct dma_slave_config slave_config;
	dma_cookie_t cookie;
	u32 len, bits_per_word;
	u8 bytes_per_word;

	if(!rxchan || !txchan) {
		dev_err(tccspi->dev,"[ERROR][SPI] rxchan(%p) or txchan(%p) are NULL\n",rxchan,txchan);
		return -ENODEV;
	}

	bits_per_word = tcc_spi_readl(tccspi->base + TCC_GPSB_MODE);
	bits_per_word = ((bits_per_word & TCC_GPSB_MODE_BWS_MASK) >> 8) + 1;
	bytes_per_word = (bits_per_word / 8);
	len = flen / bytes_per_word;
	if(!len) {
		dev_err(tccspi->dev,"[ERROR][SPI] dma xfer len err, bpw %d len %d\n",
			bits_per_word, len);
		return -EINVAL;
	}

	/* Prepare the TX dma transfer */
	sg_init_table(&tccspi->dma.sgtx,TCC_SPI_GDMA_SG_LEN);
	sg_dma_len(&tccspi->dma.sgtx) = len;
	sg_dma_address(&tccspi->dma.sgtx) = tccspi->tx_buf.dma_addr;

	/* Prepare the RX dma transfer */
	sg_init_table(&tccspi->dma.sgrx,TCC_SPI_GDMA_SG_LEN);
	sg_dma_len(&tccspi->dma.sgrx) = len;
	sg_dma_address(&tccspi->dma.sgrx) = tccspi->rx_buf.dma_addr;

	/* Config dma slave */
	if(tcc_spi_dma_engine_slave_config(tccspi, &slave_config, bytes_per_word))
	{
		dev_err(tccspi->dev, "[ERROR][SPI] Slave config failed.\n");
		return -ENOMEM;
	}

	/* Send scatterlists */
	txdesc = dmaengine_prep_slave_sg(txchan, &tccspi->dma.sgtx, TCC_SPI_GDMA_SG_LEN,
		DMA_MEM_TO_DEV,
		DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if(!txdesc)
	{
		dev_err(tccspi->dev, "[ERROR][SPI] Failed preparing TX DMA Desc.\n");
		goto err_dma;
	}

	txdesc->callback = tcc_dma_engine_tx_callback;
	txdesc->callback_param = tccspi->master;

	rxdesc = dmaengine_prep_slave_sg(rxchan, &tccspi->dma.sgrx, TCC_SPI_GDMA_SG_LEN,
		DMA_DEV_TO_MEM,
		DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if(!rxdesc)
	{
		dev_err(tccspi->dev, "[ERROR][SPI] Failed preparing RX DMA Desc.\n");
		goto err_dma;
	}

	rxdesc->callback = tcc_dma_engine_rx_callback;
	rxdesc->callback_param = tccspi->master;

	/* GPSB half-word and byte swap settings */
	if(bytes_per_word == 4){
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
			(TCC_GPSB_INTEN_SHT | TCC_GPSB_INTEN_SBT | TCC_GPSB_INTEN_SHR | TCC_GPSB_INTEN_SBR) );
	} else if(bytes_per_word == 2) {
		TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN,
			(TCC_GPSB_INTEN_SHT | TCC_GPSB_INTEN_SBT | TCC_GPSB_INTEN_SHR | TCC_GPSB_INTEN_SBR),
			(TCC_GPSB_INTEN_SBT | TCC_GPSB_INTEN_SBR) );
	} else{
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
			(TCC_GPSB_INTEN_SHT | TCC_GPSB_INTEN_SBT | TCC_GPSB_INTEN_SHR | TCC_GPSB_INTEN_SBR) );
	}

	/* Submit desctriptors */
	cookie = dmaengine_submit(txdesc);
	if(dma_submit_error(cookie))
	{
		dev_err(tccspi->dev, "[ERROR][SPI] TX Desc. submitting error! (cookie: %X)\n", (unsigned int)cookie);
		goto err_dma;
	}

	cookie = dmaengine_submit(rxdesc);
	if(dma_submit_error(cookie))
	{
		dev_err(tccspi->dev, "[ERROR][SPI] RX Desc. submitting error! (cookie: %X)\n", (unsigned int)cookie);
		goto err_dma;
	}

	// Issue pendings
	dma_async_issue_pending(txchan);
	dma_async_issue_pending(rxchan);

	return 0;
err_dma:
	/* Stop dma */
	tcc_spi_stop_dma_engine(tccspi);
	dev_err(tccspi->dev, "[ERROR][SPI] terminate dma-engine\n");

	return -ENOMEM;
}

/* Probe dma-engine channel */
static int tcc_spi_dma_engine_probe(struct platform_device *pdev, struct tcc_spi *tccspi)
{
	struct dma_slave_config slave_config;
	struct device *dev = &pdev->dev;
	dma_cap_mask_t mask;
	int ret;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	tccspi->dma.chan_tx = dma_request_slave_channel_compat(mask,tcc_spi_dma_engine_filter,&tccspi->dma,dev,"tx");
	//tccspi->dma.chan_tx = dma_request_slave_channel(dev, "tx");
	if(!tccspi->dma.chan_tx)
	{
		dev_err(dev, "[ERROR][SPI] DMA TX channel request Error!(%p)\n",tccspi->dma.chan_tx);
		ret = -EBUSY;
		goto error;
	}

	tccspi->dma.chan_rx = dma_request_slave_channel_compat(mask,tcc_spi_dma_engine_filter,&tccspi->dma,dev,"rx");
	//tccspi->dma.chan_rx = dma_request_slave_channel(dev, "rx");
	if(!tccspi->dma.chan_rx)
	{
		dev_err(dev, "[ERROR][SPI] DMA RX channel request Error!(%p)\n",tccspi->dma.chan_rx);
		ret = -EBUSY;
		goto error;
	}

	ret = tcc_spi_dma_engine_slave_config(tccspi,&slave_config,TCC_SPI_GDMA_WSIZE);
	if(ret)
		goto error;

	dev_info(dev, "[INFO][SPI] DMA-engine Tx(%s) Rx(%s)\n",
		dma_chan_name(tccspi->dma.chan_tx),dma_chan_name(tccspi->dma.chan_rx));

	return 0;

error:
	tcc_spi_release_dma_engine(tccspi);
	return ret;
}
#else
static void tcc_spi_release_dma_engine(struct tcc_spi *tccspi)
{
}
static int tcc_spi_dma_engine_submit(struct tcc_spi *tccspi, u32 flen)
{
	return -EPERM;
}
static void tcc_spi_stop_dma_engine(struct tcc_spi *tccspi)
{
}
static int tcc_spi_dma_engine_probe(struct platform_device *pdev, struct tcc_spi *tccspi)
{
	return -EPERM;
}
#endif

/******* SPI API *******/

/* SPI setsup */
static int tcc_spi_setup(struct spi_device *spi)
{
	int status;
	struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);
	unsigned int bits;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] tcc_spi data is not exist\n");
		return -ENXIO;
	}
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	if(!spi->controller_state) {
		spi->controller_state = (void *)true;

		/* Initialize cs gpio */
		if(spi->master->cs_gpios && gpio_is_valid(spi->cs_gpio)) {
			char cs_gpio_name[20];
			int ret;
			sprintf(cs_gpio_name,"tcc_spi_cs_gpio_%d", spi->chip_select);
			ret = gpio_request(spi->cs_gpio, cs_gpio_name);
			if(ret) {
				dev_err(&spi->dev, "[ERROR][SPI] gpio_request err 1 (gpio %d ret %d)\n",
					spi->cs_gpio, ret);
			}
			gpio_direction_output(spi->cs_gpio, !(spi->mode & SPI_CS_HIGH));
		}
	}

	if(spi->chip_select > spi->master->num_chipselect) {
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
	if(status)
		return status;

	/* Set the clock */
	status = tcc_spi_set_clk(tccspi, spi->max_speed_hz, 0,1);
	if(status)
		return status;

	return 0;
}

/* SPI master cleanup */
static void tcc_spi_cleanup(struct spi_device *spi)
{
	struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);

	if(!spi->controller_state)
		return;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tcc_spi is null\n", __func__);
		return;
	}
	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	if(gpio_is_valid(spi->cs_gpio))
		gpio_free(spi->cs_gpio);

	spi->controller_state = NULL;

	return;
}

/* Control CS by GPSB */
static void tcc_spi_set_cs(struct spi_device *spi, bool enable)
{
	struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] contm %d en %d\n",
		__func__, tccspi->pd->contm_support, enable);

	/* When do not use cs-gpios, SPI_CS_HIGH is set when call spi_setup() */
	if (spi->mode & SPI_CS_HIGH)
		enable = !enable;

	if(!enable) {
		if(tccspi->pd->ctf){
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_CTF);
		}
		if(!tccspi->pd->contm_support) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);
		}
	} else {
		if(tccspi->pd->ctf){
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_CTF);
		}
		if(!tccspi->pd->contm_support) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);
		}
	}
}

/* Hanlde one spi_transfer */
static int tcc_spi_transfer_one(struct spi_master *master, struct spi_device *spi,
			    struct spi_transfer *xfer)
{
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	int status, timeout_ms, is_slave;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s]\n", __func__);

	is_slave = spi_controller_is_slave(tccspi->master);

	if(!xfer) {
		dev_err(tccspi->dev, "[ERROR][SPI] xfer is null\n");
		status = -EINVAL;
		goto exit;
	}

	if (!(xfer->tx_buf || xfer->rx_buf) && xfer->len) {
		dev_err(tccspi->dev, "[ERROR][SPI] missing rx or tx buf\n");
		status =  -EINVAL;
		goto exit;
	}

	/* Get SPI transfer speed (in Hz)*/
	tccspi->clk = (xfer->speed_hz != spi->max_speed_hz) ? xfer->speed_hz : spi->max_speed_hz;
	tcc_spi_set_clk(tccspi, tccspi->clk, 0, 1);

	/* Get Bit width */
	tccspi->bits = (xfer->bits_per_word != spi->bits_per_word) ? xfer->bits_per_word : spi->bits_per_word;
	tcc_spi_set_bit_width(tccspi, tccspi->bits);

	tccspi->cur_rx_pos = 0;
	tccspi->cur_tx_pos = 0;
	tccspi->current_xfer = xfer;
	tccspi->current_remaining_bytes = xfer->len;

	dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] xfer length %d\n", __func__, xfer->len);

	while(tccspi->current_remaining_bytes) {
		int len = 0;
		int ret = 0;

		reinit_completion(&tccspi->xfer_complete);

		/* Set Packet size (packet count = 0) */
		if(tccspi->current_remaining_bytes > tccspi->dma_buf_size) {
			len = tccspi->dma_buf_size;
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
		tcc_spi_set_dma_addr(tccspi, tccspi->tx_buf.dma_addr, tccspi->rx_buf.dma_addr);

		/* Copy client txbuf to spi txbuf */
		tcc_spi_txbuf_copy_client_to_spi(tccspi, xfer, len);

		/* Reset FIFO and Packet counter */
		tcc_spi_clear_fifo(tccspi);
		tcc_spi_clear_packet_cnt(tccspi);
		tcc_spi_set_packet_size(tccspi, len);

		/* Setup GDMA, if use */
		if(tcc_spi_is_use_gdma(tccspi)) {
			/* Submit dma engine descriptor */
			ret = tcc_spi_dma_engine_submit(tccspi, len);
			if(ret) {
				dev_err(tccspi->dev,
					"[ERROR][SPI] spi dma transfer err %d\n", ret);
				status = ret;

				if(tcc_spi_is_use_gdma(tccspi)) {
					tcc_spi_stop_dma_engine(tccspi);
				}
				tcc_spi_stop_dma(tccspi);

				goto exit;
			}
		}

		/* Start Tx and Rx DMA operation */
		tcc_spi_start_dma(tccspi);

		timeout_ms = len * 8 * 1000 / tccspi->clk;
		timeout_ms += timeout_ms + 100; /* some tolerance */

		/* Wait until transfer is finished */
		if(is_slave){
			ret = wait_for_completion_interruptible(&tccspi->xfer_complete);
			if(ret == 0){
				ret = 1;
			}
		}else{
		ret = wait_for_completion_timeout(&tccspi->xfer_complete, msecs_to_jiffies(timeout_ms));
		}

		if(!ret) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] [%s] spi interrupted or trasfer timeout (%d ms), err %d\n",
							 __func__, timeout_ms, ret);
			tcc_spi_regs_dump(tccspi);
			status = -EIO;

			if(tcc_spi_is_use_gdma(tccspi)) {
				tcc_spi_stop_dma_engine(tccspi);
			}
			tcc_spi_stop_dma(tccspi);

			goto exit;
		} else {
			ret = 0;
			status = 0;
		}

		/* Copy spi rxbuf to client rxbuf */
		tcc_spi_rxbuf_copy_client_to_spi(tccspi, xfer, len);

		tccspi->current_remaining_bytes -= len;
		dev_dbg(tccspi->dev, "[DEBUG][SPI] completed remain %d xfered %d\n",
			tccspi->current_remaining_bytes, len);
	}

exit:

	/* Restore original setting */
	tccspi->clk = spi->max_speed_hz;
	tcc_spi_set_clk(tccspi, tccspi->clk, 0, 1);

	tccspi->bits = spi->bits_per_word;
	tcc_spi_set_bit_width(tccspi, tccspi->bits);

	return status;
}

#ifdef CONFIG_OF
static int tcc_spi_get_gpsb_ch(struct tcc_spi_pl_data *pd)
{
	int ret = -1;
	const char *gpsb_name = pd->name;

    if(!strncmp(gpsb_name,"gpsb0", 5))
		ret = 0;
    else if(!strncmp(gpsb_name,"gpsb1", 5))
		ret = 1;
    else if(!strncmp(gpsb_name,"gpsb2", 5))
		ret = 2;
    else if(!strncmp(gpsb_name,"gpsb3", 5))
		ret = 3;
    else if(!strncmp(gpsb_name,"gpsb4", 5))
		ret = 4;
    else if(!strncmp(gpsb_name,"gpsb5", 5))
		ret = 5;

	return ret;
}

static struct tcc_spi_pl_data *tcc_spi_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct tcc_spi_pl_data *pd;
	int status;

	if(!np) {
		dev_err(dev, "[ERROR][SPI] no dt node defined\n");
		return NULL;
	}

	pd = devm_kzalloc(dev, sizeof(struct tcc_spi_pl_data), GFP_KERNEL);
	if(!pd)
		return NULL;

	/* Get the bus id */
	pd->id = -1;
	status = of_property_read_u32(np, "gpsb-id", &pd->id);
	if(status != 0){
		dev_err(dev,
			"[ERROR][SPI] No SPI id.\n");
		return NULL;
	}

	/* Get the driver name */
	pd->name = np->name;
	pd->gpsb_channel = tcc_spi_get_gpsb_ch(pd);
	if(pd->gpsb_channel < 0) {
		dev_err(dev, "[ERROR][SPI] wrong spi driver name (%s)\n", np->name);
		return NULL;
	}

	/* Get the port number */
#ifdef TCC_USE_GFB_PORT
	/* Port array order : [0]SDI [1]SCLK [2]SFRM [3]SDO */
	status = of_property_read_u32_array(np, "gpsb-port", pd->port,
		(size_t)of_property_count_elems_of_size(np, "gpsb-port", sizeof(u32)));
	if(status != 0){
		dev_err(dev,
			"[ERROR][SPI] No SPI port info.\n");
		return NULL;
	}

	dev_info(dev, "[INFO][SPI] sdi: %d sclk: %d sfrm: %d sdo: %d\n",
		pd->port[0], pd->port[1], pd->port[2], pd->port[3]);
#else
	status = of_property_read_u32(np, "gpsb-port", &pd->port);
	if(status != 0){
		dev_err(dev,
			"[ERROR][SPI] No SPI port info.\n");
		return NULL;
	}
	dev_info(dev, "[INFO][SPI] port %d\n", pd->port);
#endif

	pd->ctf = true;
	if(of_property_read_bool(np, "ctf-mode-disable")){
		pd->ctf = false;
	}
	pd->is_slave = of_property_read_bool(np, "spi-slave");
	if(pd->is_slave != 0){
		dev_info(dev, "[INFO][SPI] GPSB Slave ctf mode: %d\n", pd->ctf);
	}else{
		dev_info(dev, "[INFO][SPI] GPSB Master ctf mode: %d\n", pd->ctf);
	}

	/*
	 * TCC GPSB CH 3-5 don't have dedicated dma
	 * GPSB CH 3-5 should use gdma(dma-engine)
	 */
	if(pd->gpsb_channel > 2) {
		pd->dma_enable = 1;
		dev_dbg(dev, "[DEBUG][SPI] gpsb ch %d - use gdma\n", pd->gpsb_channel);
	}
	else {
		pd->dma_enable = 0;
	}

	/* Get the hclk and pclk */
	pd->pclk = of_clk_get(np, 0);
	pd->hclk = of_clk_get(np, 1);
	if(IS_ERR(pd->pclk) || IS_ERR(pd->hclk)) {
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
static int tcc_spi_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device	*dev = &pdev->dev;
	struct resource *regs = NULL;
	struct spi_master *master;
	struct tcc_spi		*tccspi;
	struct tcc_spi_pl_data *pd;
	unsigned int ac_val[2] = {0};

	if(!pdev->dev.of_node)
		return -EINVAL;

	dev_dbg(dev, "[DEBUG][SPI] [%s]\n", __func__);

	/* Get TCC GPSB SPI master platform data */
	pd = tcc_spi_parse_dt(dev);
	if(!pd) {
		dev_err(dev,
			"[ERROR][SPI] No TCC SPI master platform data.\n");
		return -ENODEV;
	}

	/* allocate master or slave controller */
	if(pd->is_slave){
		master = spi_alloc_slave(dev, sizeof(struct tcc_spi));
	}else{
	master = spi_alloc_master(dev, sizeof(struct tcc_spi));
	}

	if(!master) {
		dev_err(dev,
			"[ERROR][SPI] SPI memory allocation failed.\n");
		return -ENOMEM;
	}

	/* the spi->mode bits understood by this driver: */
	master->bus_num = pd->id;
	//master->num_chipselect = 1;
	master->mode_bits = TCC_SPI_MODE_BITS;
	master->bits_per_word_mask = SPI_BPW_MASK(32) | SPI_BPW_MASK(16) | SPI_BPW_MASK(8);
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
	if(ret) {
		dev_err(dev,
			"[ERROR][SPI] Failed to set dma mask\n");
		goto exit_free_master;
	}
	if(!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	else
		dma_set_mask(dev, DMA_BIT_MASK(32));

	/* Get TCC GPSB SPI master base address */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!regs) {
		dev_err(dev,
			"[ERROR][SPI] No SPI base register addr.\n");
		ret = -ENXIO;
		goto exit_free_master;
	}

	tccspi->pbase = regs->start;
	tccspi->base = devm_ioremap_resource(dev,regs);

	/* Get TCC GPSB SPI master port configuration reg. address */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if(!regs) {
		dev_err(dev,
			"[ERROR][SPI] No SPI PCFG register addr.\n");
		ret = -ENXIO;
		goto exit_free_master;
	}

	tccspi->pcfg = of_iomap(pdev->dev.of_node, 1); //devm_ioremap_resource(dev,regs);

	/* Configure the AHB Access Filter */
	tccspi->ac = of_iomap(pdev->dev.of_node, 2);
	if (!IS_ERR(tccspi->ac)) {
		if (!of_property_read_u32_array(pdev->dev.of_node,
					"access-control0", ac_val, 2)) {
			dev_dbg(&pdev->dev,
				"[DEBUG][SPI] access-control0 start:0x%x limit:0x%x\n",
				ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC0_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC0_LIMIT);
		}
		if (!of_property_read_u32_array(pdev->dev.of_node,
					"access-control1", ac_val, 2)) {
			dev_dbg(&pdev->dev,
				"[DEBUG][SPI] access-control1 start:0x%x limit:0x%x\n",
				ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC1_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC1_LIMIT);
		}
		if (!of_property_read_u32_array(pdev->dev.of_node,
					"access-control2", ac_val, 2)) {
			dev_dbg(&pdev->dev,
				"[DEBUG][SPI] access-control2 start:0x%x limit:0x%x\n",
				ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC2_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC2_LIMIT);
		}
		if (!of_property_read_u32_array(pdev->dev.of_node,
					"access-control3", ac_val, 2)) {
			dev_dbg(&pdev->dev,
				"[DEBUG][SPI] access-control3 start:0x%x limit:0x%x\n",
				ac_val[0], ac_val[1]);
			writel(ac_val[0], tccspi->ac + TCC_GPSB_AC3_START);
			writel(ac_val[1], tccspi->ac + TCC_GPSB_AC3_LIMIT);
		}
	}

	/* Check CONTM support */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_EVTCTRL, TCC_GPSB_EVTCTRL_CONTM(0x3));
	ret = tcc_spi_readl(tccspi->base + TCC_GPSB_EVTCTRL) & TCC_GPSB_EVTCTRL_CONTM(0x3);
	tccspi->pd->contm_support = !!ret;

	/* Get TCC GPSB IRQ number */
	tccspi->irq = -1;
	tccspi->irq = platform_get_irq(pdev, 0);
	if(tccspi->irq < 0) {
		dev_err(dev,
			"[ERROR][SPI] No SPI IRQ Number.\n");
		ret = -ENXIO;
		goto exit_free_master;
	}

	/* Get GDMA data */
	if(tcc_spi_is_use_gdma(tccspi)) {
		ret = tcc_spi_dma_engine_probe(pdev, tccspi);
		if(ret < 0) {
			dev_err(dev,
				"[ERROR][SPI] Failed to get dma-engine\n");
			goto exit_free_master;
		}
	} else {
		ret = devm_request_irq(dev, tccspi->irq, tcc_spi_dma_irq,
						IRQF_SHARED, dev_name(dev), master);
		if(ret < 0) {
			dev_err(dev,
				"[ERROR][SPI] Failed to enter irq handler\n");
			ret = -ENXIO;
			goto exit_free_master;
		}
	}

	/* Get pin control (active state)*/
	tccspi->pinctrl = devm_pinctrl_get_select(dev, "active");
	if(IS_ERR(tccspi->pinctrl)) {
		dev_err(dev,
			"[ERROR][SPI] Failed to get pinctrl (active state)\n");
		ret = -ENXIO;
		goto exit_free_master;
	}

	/* Allocate SPI master dma buffer (rx) */
	tccspi->dma_buf_size = TCC_SPI_DMA_MAX_SIZE;
	ret = tcc_spi_init_dma_buf(tccspi, 1);
	if(ret < 0) {
		dev_err(dev,
			"[ERROR][SPI] Failed to allocate rx dma buffer\n");
		goto exit_free_master;
	}

	/* Allocate SPI master dma buffer (tx) */
	ret = tcc_spi_init_dma_buf(tccspi, 0);
	if(ret < 0) {
		dev_err(dev,
			"[ERROR][SPI] Failed to allocate tx dma buffer\n");
		goto exit_rx_dma_free;
	}

	/* Initialize xfer completion */
	init_completion(&tccspi->xfer_complete);

	platform_set_drvdata(pdev, master);

	/* Enable clock */
	ret = clk_prepare_enable(pd->hclk);
	if(ret) {
		dev_err(dev, "[ERROR][SPI] Failed to enable hclk\n");
		goto exit_tx_dma_free;
	}

	ret = clk_prepare_enable(pd->pclk);
	if(ret) {
		dev_err(dev, "[ERROR][SPI] Failed to enable pclk\n");
		goto exit_unprepare_clk;
	}

	/* Initialize GPSB registers */
	tcc_spi_clear_fifo(tccspi);
	tcc_spi_clear_packet_cnt(tccspi);
	tcc_spi_stop_dma(tccspi);
	tcc_spi_hwinit(tccspi);

	/* Set port configuration */
	tcc_spi_set_port(tccspi);

	ret = devm_spi_register_master(dev, master);
	if(ret) {
		dev_err(dev,
			"[ERROR][SPI] No SPI IRQ Number.\n");
		goto exit_tx_dma_free;
	}

#ifdef TCC_USE_GFB_PORT
	dev_info(dev, "[INFO][SPI] TCC SPI [%s] Id: %d [ch:%d @%X][port: %d %d %d %d][irq: %d][CONTM: %d][gdma: %d][cs_num: %d]\n",
		pd->name,
		master->bus_num,
		pd->gpsb_channel,
		(unsigned int)tccspi->base,
		pd->port[0],pd->port[1],pd->port[2],pd->port[3],
		tccspi->irq,
		tccspi->pd->contm_support,
		tcc_spi_is_use_gdma(tccspi),
		master->num_chipselect);
#else
	dev_info(dev, "[INFO][SPI] TCC SPI [%s] Id: %d [ch:%d @%X][port: %d][irq: %d][CONTM: %d][gdma: %d][cs_num: %d]\n",
		pd->name,
		master->bus_num,
		pd->gpsb_channel,
		(unsigned int)tccspi->base,
		pd->port,
		tccspi->irq,
		tccspi->pd->contm_support,
		tcc_spi_is_use_gdma(tccspi),
		master->num_chipselect);
#endif

	return ret;

exit_unprepare_clk:
	clk_disable_unprepare(pd->hclk);
exit_tx_dma_free:
	tcc_spi_deinit_dma_buf(tccspi, 0);
exit_rx_dma_free:
	tcc_spi_deinit_dma_buf(tccspi, 1);
exit_free_master:
	spi_master_put(master);
	return ret;
}

static int tcc_spi_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	struct tcc_spi_pl_data *pd = NULL;
	unsigned long flags;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	pd = tcc_spi_get_pl_data(tccspi);

	/* Get pin control (idle state)*/
	tccspi->pinctrl = devm_pinctrl_get_select(tccspi->dev, "idle");
	if(IS_ERR(tccspi->pinctrl)) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Failed to get pinctrl (idle state)\n");
		return -ENODEV;
	}

	/* Disable clock */
	spin_lock_irqsave(&(tccspi->lock), flags);
	if(pd->pclk)
		clk_disable_unprepare(pd->pclk);
	if(pd->hclk)
		clk_disable_unprepare(pd->hclk);
	spin_unlock_irqrestore(&(tccspi->lock), flags);

	/* Release DMA buffers */
	if(tccspi->rx_buf.v_addr)
		tcc_spi_deinit_dma_buf(tccspi, 1);
	if(tccspi->tx_buf.v_addr)
		tcc_spi_deinit_dma_buf(tccspi, 0);

	if(tcc_spi_is_use_gdma(tccspi)) {
		tcc_spi_release_dma_engine(tccspi);
	}

	iounmap(tccspi->pcfg);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_spi_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	struct tcc_spi_pl_data *pd = NULL;
	unsigned long flags;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	pd = tcc_spi_get_pl_data(tccspi);

	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);
	tcc_spi_stop_dma(tccspi);

	spin_lock_irqsave(&(tccspi->lock), flags);
	if(pd->pclk)
		clk_disable_unprepare(pd->pclk);
	if(pd->hclk)
		clk_disable_unprepare(pd->hclk);
	spin_unlock_irqrestore(&(tccspi->lock), flags);

	return spi_master_suspend(master);
}

static int tcc_spi_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct tcc_spi *tccspi = spi_master_get_devdata(master);
	struct tcc_spi_pl_data *pd = NULL;
	unsigned long flags;
	int status;

	if(!tccspi) {
		printk(KERN_ERR "[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		return -EINVAL;
	}

	pd = tcc_spi_get_pl_data(tccspi);

	spin_lock_irqsave(&(tccspi->lock), flags);
	/* Enable clock */
	if(pd->hclk)  {
		status = clk_prepare_enable(pd->hclk);
		if(status) {
			dev_err(tccspi->dev, "[ERROR][SPI] Failed to enable hclk\n");
			return status;
		}
	}
	if(pd->pclk)  {
		status = clk_prepare_enable(pd->pclk);
		if(status) {
			dev_err(tccspi->dev, "[ERROR][SPI] Failed to enable pclk\n");
			return status;
		}
	}
	spin_unlock_irqrestore(&(tccspi->lock), flags);

	/* Initialize GPSB registers */
	tcc_spi_clear_fifo(tccspi);
	tcc_spi_clear_packet_cnt(tccspi);
	tcc_spi_stop_dma(tccspi);
	tcc_spi_hwinit(tccspi);

	/* Set port configuration */
	tcc_spi_set_port(tccspi);

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
static struct of_device_id tcc_spi_of_match[] = {
	{ .compatible = "telechips,tcc-spi" },
	{ .compatible = "telechips,tcc-spi-slave" },
	{ .compatible = "telechips,tcc805x-spi" },
	{ .compatible = "telechips,tcc803x-spi" },
	{ .compatible = "telechips,tcc897x-spi" },
	{ .compatible = "telechips,tcc899x-spi" },
	{ .compatible = "telechips,tcc901x-spi" },
	{}
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
