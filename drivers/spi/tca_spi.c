// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include "tca_spi.h"
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spi/tcc_tsif.h>
#include <linux/device.h>

int32_t tca_spi_is_use_gdma(struct tca_spi_handle *h)
{
	return h->gdma_use;
}

int32_t tca_spi_is_normal_slave(struct tca_spi_handle *h)
{
	uint32_t tmp_val;

	tmp_val = (u32)h->is_slave;
	if ((tmp_val & TCC_GPSB_SLAVE_NORMAL) == (u32)0) {
		return 0;
	}

	return 1;
}

static int32_t tca_spi_isenabledma(struct tca_spi_handle *h)
{
	int32_t ret;
	uint32_t uret;

	ret = tca_spi_is_use_gdma(h);
	if (ret != 0) {
		return 0;
	}

	uret = (u32)(tca_spi_readl(h->regs + TCC_GPSB_DMACTR) & BIT(0));

	if (uret != (u32)0) {
		return 1;
	}

	return 0;
}

static int32_t tca_spi_dmastop(struct tca_spi_handle *h)
{
	int32_t ret;

	if (h->tx_pkt_remain == 0) {
		/* Operation Disable */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(3));
	}

	/* If GPSB use GDMA */
	ret = tca_spi_is_use_gdma(h);
	if (ret != 0) {
		return 0;
	}

	/* disable DMA Transmit & Receive */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(31) | BIT(30));
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, BIT(29) | BIT(28));
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(0)); /* DMA disable */

	return 0;
}

static int32_t tca_spi_dmastop_slave(struct tca_spi_handle *h)
{
	int32_t ret;

	/* If GPSB use GDMA */
	ret = tca_spi_is_use_gdma(h);
	if (ret != 0) {
		return 0;
	}

	/* disable DMA Transmit & Receive */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(31) | BIT(30));
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, BIT(29) | BIT(28));
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(0)); /* DMA disable */

	return 0;
}

static int32_t tca_spi_dmastart(struct tca_spi_handle *h)
{
	int32_t ret;

	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(4));
	if (h->ctf != 0) {
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(4));
	}

	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(3)); /* Operation Enable */

	/* If GPSB use GDMA */
	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		/* enable DMA Transmit & Receive */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(31) | BIT(30));
		/* set Multiple address mode */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR,
				BIT(17) | BIT(16) | BIT(15) | BIT(14));
		/* disable DMA Packet Interrupt */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, BIT(16));
		/* enable DMA Done Interrupt */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, BIT(17));
		/* set rx interrupt */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, BIT(20));

		/* DMA enable */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(0));
	}

	return 0;
}

static int32_t tca_spi_dmastart_slave(struct tca_spi_handle *h)
{
	int32_t ret;

	/* If GPSB use GDMA */
	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		/* enable DMA Transmit */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(30));
		/* set Multiple address mode */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR,
				BIT(17) | BIT(16) | BIT(15) | BIT(14));
		/* enable DMA Packet Interrupt */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, BIT(16));
		/* set rx interrupt */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, BIT(20));
		/* enable DMA Packet Interrupt */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, BIT(16));

		if (h->dma_mode == 0) {
			/* Normal mode & Continuous mode*/
			TCC_GPSB_BITCSET(h->regs + TCC_GPSB_DMACTR,
					BIT(5) | BIT(4), BIT(29));
		} else {
			/* MPEG2-TS mode */
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(4));
		}
		/* DMA enable */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(0));
	}

	return 0;
}

static void tca_spi_clearfifopacket(struct tca_spi_handle *h)
{
	int32_t ret;

	/* clear tx/rx FIFO & Packet counter  */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(15) | BIT(14));

	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(2));
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(2));
	}
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(15) | BIT(14));
}

static void tca_spi_setpacketcnt(struct tca_spi_handle *h, int32_t size)
{
	int32_t ret;

	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		/* set packet count & size */
		tca_spi_writel(((u32)size & 0x1FFFU),
				h->regs + TCC_GPSB_PACKET);
	}
}

static void tca_spi_setpacketcnt_slave(struct tca_spi_handle *h, int32_t size)
{
	uint32_t packet_cnt;
	uint32_t packet_size;
	uint32_t intr_packet_cnt;
	int32_t ret;
	uint32_t tmp_val;

	packet_cnt = h->dma_total_packet_cnt & 0x1FFFU;
	tmp_val = (u32)1;
	if (packet_cnt < tmp_val) {
		packet_cnt = 0;
	} else {
		packet_cnt = packet_cnt - tmp_val;
	}

	intr_packet_cnt = h->dma_intr_packet_cnt & 0x1FFFU;
	tmp_val = (u32)1;
	if (intr_packet_cnt < tmp_val) {
		intr_packet_cnt = 0;
	} else {
		intr_packet_cnt = intr_packet_cnt - tmp_val;
	}

	ret = tca_spi_is_normal_slave(h);
	if (ret != 0) {
		packet_size = ((u32)size & 0x1FFFU);
	} else {
		packet_size = ((u32)TSIF_PACKET_SIZE & 0x1FFFU);
	}

	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		tca_spi_writel(((packet_cnt << 16) | packet_size),
				h->regs + TCC_GPSB_PACKET);
		TCC_GPSB_BITCSET(h->regs + TCC_GPSB_DMAICR,
				0x1FFFUL, intr_packet_cnt);
	}
}

static void tca_spi_setbitwidth(struct tca_spi_handle *h, int32_t width)
{
	uint32_t width_value;
	int32_t ret;
	int32_t tmp_val;

	tmp_val = width - 1;
	width_value = (u32)tmp_val;
	width_value = width_value & 0x1FU;

	/* set bit width */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE,
			BIT(12) | BIT(11) | BIT(10) | BIT(9) | BIT(8));
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, ((u32)width_value << (u32)8));

	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		if (((u32)width_value & (u32)BIT(4)) != (u32)0) {
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(28));
		} else {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, BIT(28));
		}
	}
}

static void tca_spi_setdmaaddr(struct tca_spi_handle *h)
{
	int32_t ret;

	/* If GPSB use GDMA */
	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		/* set dma txbase/rxbase & request DMA tx/rx */
		tca_spi_writel((u32)h->tx_dma.dma_addr,
				h->regs + TCC_GPSB_TXBASE);
		tca_spi_writel((u32)h->rx_dma.dma_addr,
				h->regs + TCC_GPSB_RXBASE);
	}

	if (h->tx_dma.dma_addr != 0u) {
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN, BIT(31));
	} else {
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, BIT(31));
	}

	if (h->rx_dma.dma_addr != 0u) {
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN, BIT(30));
	} else {
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, BIT(30));
	}
}

static void tca_spi_setdmaaddr_slave(struct tca_spi_handle *h)
{
	int32_t ret;

	/* If GPSB use GDMA */
	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		/* set dma txbase/rxbase & request DMA tx/rx */
		tca_spi_writel((u32)h->rx_dma.dma_addr,
				h->regs + TCC_GPSB_RXBASE);
		/* BIT(18)~BIT(16):
		 * Revceive FIFO threshold for interrupt/DMA request
		 */
	}
	if (h->dma_mode == 0) {
		if (h->rx_dma.dma_addr != 0u) {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN,
					BIT(30) | BIT(15));
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN,
					BIT(18) | BIT(17) | BIT(16));
		} else {
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, BIT(30));
		}
	} else {
		if (h->rx_dma.dma_addr != 0u) {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN,
					BIT(30) | BIT(15));
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN,
					BIT(18) | BIT(17) | BIT(16));
		} else {
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, BIT(30));
		}
	}
}

static void tca_spi_regs_reset(struct tca_spi_handle *h)
{
	int32_t ret;

	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_STAT, BIT(31) - BIT(0));
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, BIT(31) - BIT(0));
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(31) - BIT(0));
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_CTRL, BIT(31) - BIT(0));
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_EVTCTRL, BIT(31) - BIT(0));
	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_TXBASE, BIT(31) - BIT(0));
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_RXBASE, BIT(31) - BIT(0));
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_PACKET, BIT(31) - BIT(0));
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, BIT(31) - BIT(0));
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMASTR, BIT(31) - BIT(0));
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, BIT(31) - BIT(0));
	}
}

static void tca_spi_hwinit(struct tca_spi_handle *h)
{
	/* Reset Registers */
	tca_spi_regs_reset(h);

	/* Disable Operation */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(3));

	h->set_bit_width(h, 32);

	h->set_dma_addr(h);

	/* Clear Fifo must be operated before operation enable */
	h->clear_fifo_packet(h);

	/* Set SPI Master mode*/
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(0) | BIT(1) | BIT(2));

	/* [SCK] Tx: risiing edge, Rx: falling edge */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(17) | BIT(18));

#ifdef CONFIG_SPI_MST_LOOPBACK
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(6));
#endif
	if ((tca_spi_readl(h->regs + TCC_GPSB_MODE) & BIT(6)) != (u32)0) {
		dev_dbg(h->dev,
			"[DEBUG][SPI] [%s] Telechips SPI Master Loop-back Test Enabled.\n",
			__func__);
	}
}

static void tca_spi_hwinit_slave(struct tca_spi_handle *h)
{
	/* Reset Registers */
	tca_spi_regs_reset(h);

	/* Disable Operation */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, BIT(3));

	h->set_bit_width(h, 32);

	h->set_dma_addr(h);

	/* Clear Fifo must be operated before operation enable */
	h->clear_fifo_packet(h);

	/* Set SPI Slave Mode, Disable SDO */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(2) | BIT(5));

	/* Set SDOE, SDO is driven only when cs is active. */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_EVTCTRL, BIT(25));

	/* Enable Operation */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, BIT(3));
}

static void tca_spi_set_mpegtspidmode(struct tca_spi_handle *h, int32_t is_set)
{
	int32_t ret;

	h->hw_init(h);
	ret = tca_spi_is_use_gdma(h);
	if (ret == 0) {
		if (is_set != 0) {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR,
					BIT(19) | BIT(18));
		} else {
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR,
					BIT(19) | BIT(18));
		}
	}
}

#ifdef TCC_USE_GFB_PORT
static void tca_spi_set_port(struct tca_spi_handle *h,
				struct tca_spi_port_config *port)
{
	void __iomem *gpsb_pcf_regs = h->port_regs;
	uint32_t pcfg_offset = TCC_GPSB_PCFG_OFFSET * h->gpsb_channel;
	int32_t i;

	/* [0]SDI [1]SCLK [2]SFRM [3]SDO */
	for (i = 0; i < 4; i++) {
		TCC_GPSB_BITCSET(gpsb_pcf_regs + pcfg_offset,
			(BIT(8) - BIT(0)) << (i * TCC_GPSB_PCFG_SHIFT),
			port->gpsb_port[i] << (i * TCC_GPSB_PCFG_SHIFT));
	}

	dev_info(h->dev, "[INFO][SPI] [%s][GFB]: SDI: 0x%X, SCLK: 0x%X, SFRM: 0x%X, SDO: 0x%X\n",
			__func__, port->gpsb_port[0], port->gpsb_port[1],
			port->gpsb_port[2], port->gpsb_port[3]);
	dev_info(h->dev, "[INFO][SPI] [%s][CH:%d] PCFG(0x%X): 0x%x\n",
			__func__, h->gpsb_channel,
			(uint)(gpsb_pcf_regs + pcfg_offset),
			tca_spi_readl(gpsb_pcf_regs + pcfg_offset));
}

static void tca_spi_clear_port(struct tca_spi_handle *h)
{
	void __iomem *gpsb_pcf_regs = h->port_regs;
	uint32_t pcfg_offset = TCC_GPSB_PCFG_OFFSET * h->gpsb_channel;

	TCC_GPSB_BITCSET(gpsb_pcf_regs + pcfg_offset, 0xFFFFFFFF, 0xFFFFFFFF);
}
#else
static void tca_spi_set_port(struct tca_spi_handle *h,
				struct tca_spi_port_config *port)
{
	int32_t gpsb_channel, gpsb_port, gpsb_channel2;
	void __iomem *gpsb_pcf_regs = h->port_regs;
	int32_t i, i2;
	u32 cmask_val, smask_val;
	u32 utmp_val, reg_val;
	uint32_t gpsb_port_reg_val;

	gpsb_channel = h->gpsb_channel;
	gpsb_port    = h->gpsb_port;

	dev_info(h->dev, "[INFO][SPI] [%s] CH: %d PORT: %d\n",
			__func__, gpsb_channel, port->gpsb_port);

	if (h->gpsb_channel <= 3) {
		cmask_val = (u32)0xFFU << ((u32)gpsb_channel << (u32)3);
		smask_val = ((u32)gpsb_port) << ((u32)gpsb_channel << (u32)3);
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG0,
				cmask_val, smask_val);
	} else {
		gpsb_channel2 = gpsb_channel - 4;
		cmask_val = (u32)0xFFU << ((u32)gpsb_channel2 << (u32)3);
		smask_val = ((u32)gpsb_port) << ((u32)gpsb_channel2 << (u32)3);
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG1,
				cmask_val, smask_val);
	}

	for (i = 0; i < TCC_GPSB_MAX_CH; i++) {
		if (i == h->gpsb_channel) {
			continue;
		}

		if (i < 4) {
			utmp_val = (u32)0xFFU << ((u32)i << (u32)3);
			reg_val = tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG0);
			reg_val &= utmp_val;
			gpsb_port_reg_val = reg_val >> ((u32)i << (u32)3);
			if (gpsb_port_reg_val == (u32)gpsb_port) {
				cmask_val = (u32)0xFFU << ((u32)i << (u32)3);
				smask_val = (u32)0xFFU << ((u32)i << (u32)3);
				TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG0,
						cmask_val, smask_val);

				dev_warn(h->dev,
					"[WARN][SPI] warning port conflict! [[ch %d[%d]]] : ch %d[%d] pcfg0: 0x%08X pcfg1: 0x%08X\n",
					gpsb_channel, gpsb_port,
					i, gpsb_port_reg_val,
					tca_spi_readl(gpsb_pcf_regs +
						TCC_GPSB_PCFG0),
					tca_spi_readl(gpsb_pcf_regs +
						TCC_GPSB_PCFG1));
			}
		} else {
			i2 = i - 4;
			utmp_val = (u32)0xFFU << ((u32)i2 << (u32)3);
			reg_val = tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG1);
			reg_val &= utmp_val;
			gpsb_port_reg_val = reg_val >> ((u32)i2 << (u32)3);
			if (gpsb_port_reg_val == (u32)gpsb_port) {
				cmask_val = (u32)0xFFU << ((u32)i2 << (u32)3);
				smask_val = (u32)0xFFU << ((u32)i2 << (u32)3);
				TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG1,
						cmask_val, smask_val);

				dev_warn(h->dev,
					"[WARN][SPI] warning port conflict! [[ch %d[%d]]] : ch %d[%d] pcfg0: 0x%08X pcfg1: 0x%08X\n",
					gpsb_channel, gpsb_port,
					i, gpsb_port_reg_val,
					tca_spi_readl(gpsb_pcf_regs +
						TCC_GPSB_PCFG0),
					tca_spi_readl(gpsb_pcf_regs +
						TCC_GPSB_PCFG1));
			}
		}
	}
}

static void tca_spi_clear_port(struct tca_spi_handle *h)
{
	int32_t gpsb_channel;
	void __iomem *gpsb_pcf_regs = h->port_regs;

	gpsb_channel = h->gpsb_channel;
	if (gpsb_channel <= 3) {
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG0,
				(BIT(8) - BIT(0)) << ((u32)gpsb_channel << 3),
				0xFFUL << ((u32)gpsb_channel << 3));
	} else {
		gpsb_channel = gpsb_channel - 4;
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG1,
				(BIT(8)-BIT(0)) << ((u32)gpsb_channel << 3),
				0xFFUL << ((u32)gpsb_channel << 3));
	}
}
#endif /* TCC_USE_GFB_PORT */

/******************************
 * return value
 *
 * ret == 0: success
 * ret > 0 or ret < 0: fail
 ******************************/
int32_t tca_spi_init(struct tca_spi_handle *h,
		void __iomem *regs,
		uint32_t phy_reg_base,
		void __iomem *port_regs,
		void __iomem *pid_regs,
		int32_t irq,
		int32_t (*tea_dma_alloc)(struct tea_dma_buf *tdma,
					uint32_t size, struct device *dev,
					int32_t id),
		void (*tea_dma_free)(struct tea_dma_buf *tdma,
					struct device *dev, int32_t id),
		uint32_t dma_size,
		int32_t id,
		int32_t is_slave,
		struct tca_spi_port_config *port,
		const char *gpsb_name,
		struct device *dev)
{
	int32_t ret;
	int32_t ret1;
	struct tcc_spi_dma dma;

	if ((h == NULL) || (regs == NULL)) {
		return -EINVAL;
	}

	/* Memory Copy of GPSB Port Configuration */
	(void)memset(&dma, 0, sizeof(struct tcc_spi_dma));
	dma = h->dma;
	if (dma.chan_rx == NULL) {
		(void)memset(h, 0, sizeof(struct tca_spi_handle));
	}
	(void)memcpy(&h->port_config, port, sizeof(struct tca_spi_port_config));

	h->dma = dma;
	h->dev = dev;
	h->regs = regs;
	h->phy_reg_base = phy_reg_base;
	h->port_regs = port_regs;
	h->pid_regs = pid_regs;
	h->irq = irq;
	h->id = id;
	h->is_slave = is_slave;

#ifdef TCC_USE_GFB_PORT
	{
		int32_t i;

		for (i = 0; i < 4; i++) {
			h->gpsb_port[i] = port->gpsb_port[i];
		}
	}
#else
	h->gpsb_port = (s32)port->gpsb_port;
#endif

	ret = sscanf(gpsb_name, "gpsb%d", &(h->gpsb_channel));
	if (ret != 1) {
		return -EINVAL;
	}
	if ((h->gpsb_channel < 0) || (h->gpsb_channel >= TCC_GPSB_MAX_CH)) {
		return -EINVAL;
	}

	if (h->gpsb_channel < 3) {
		h->gdma_use = 0;
	} else {
		h->gdma_use = 1;
	}

	if (is_slave == 0) {
		h->dma_stop = tca_spi_dmastop;
		h->dma_start = tca_spi_dmastart;
		h->set_packet_cnt = tca_spi_setpacketcnt;
		h->set_dma_addr = tca_spi_setdmaaddr;
		h->hw_init = tca_spi_hwinit;
	} else {
		h->dma_stop = tca_spi_dmastop_slave;
		h->dma_start = tca_spi_dmastart_slave;
		h->set_packet_cnt = tca_spi_setpacketcnt_slave;
		h->set_dma_addr = tca_spi_setdmaaddr_slave;
		h->hw_init = tca_spi_hwinit_slave;
	}

	h->clear_fifo_packet = tca_spi_clearfifopacket;
	h->set_bit_width = tca_spi_setbitwidth;
	h->is_enable_dma = tca_spi_isenabledma;
	h->set_mpegts_pidmode = tca_spi_set_mpegtspidmode;

	h->tea_dma_alloc = tea_dma_alloc;
	h->tea_dma_free = tea_dma_free;

	h->dma_total_size = (s32)dma_size;
	h->dma_mode = 1; /* default MPEG2-TS DMA mode */

	h->ctf = 0;
	h->tx_pkt_remain = 0;

	tca_spi_set_port(h, port);

	if (h->tea_dma_alloc != NULL) {
		ret = h->tea_dma_alloc(&h->rx_dma, dma_size, h->dev, id);
		if (ret == 0) {
			if (is_slave != 0) {
				ret = 0;
			} else {
				ret = h->tea_dma_alloc(&(h->tx_dma), dma_size,
							h->dev, id);
				ret1 = h->tea_dma_alloc(&(h->tx_dma_1),
							dma_size, h->dev, id);
				if ((ret == 0) && (ret1 == 0)) {
					ret = 0;
				} else {
					ret = -1;
				}
			}
		}
	} else {
		/* Already, tsif has rx_dma buf */
		ret = 0;
	}

	return ret;
}

void tca_spi_clean(struct tca_spi_handle *h)
{
	if (h != NULL) {
		struct tcc_spi_dma	dma;
		int32_t id;

		dma = h->dma;
		id = h->id;
		if (h->tea_dma_free != NULL) {
			h->tea_dma_free(&(h->tx_dma), h->dev, id);
			h->tea_dma_free(&(h->rx_dma), h->dev, id);
			h->tea_dma_free(&(h->tx_dma_1), h->dev, id);
		}

		/* Clear PCFG (only one channel being used)*/
		tca_spi_clear_port(h);

		(void)memset(h, 0, sizeof(struct tca_spi_handle));
		h->dma = dma;
	}
}

int32_t tca_spi_register_pids(struct tca_spi_handle *h, uint32_t *pids,
			uint32_t count)
{
	int32_t ret = 0, gpsb_channel;

	gpsb_channel = h->gpsb_channel;

	/* supporting pids is 32 */
	if (count <= (u32)32) {
		void __iomem *PIDT;
		int32_t i;
		uint32_t pid_ch;

		if (gpsb_channel == 0) {
			pid_ch = (u32)BIT(29); /* HwGPSB_PIDT_CH0 */
		} else if (gpsb_channel == 1) {
			pid_ch = (u32)BIT(30); /* HwGPSB_PIDT_CH1 */
		} else if (gpsb_channel == 2) {
			pid_ch = (u32)BIT(31); /* HwGPSB_PIDT_CH2 */
		} else {
			return -EINVAL;
		}

		for (i = 0; i < 32; i++) {
			PIDT = h->pid_regs + (4 * i);
			tca_spi_writel(0x0, PIDT);
		}
		if (count > (u32)0) {
			for (i = 0; i < (s32)count; i++) {
				PIDT = h->pid_regs + (4 * i);
				tca_spi_writel(pids[i] & 0x1FFFFFFFU, PIDT);
				TCC_GPSB_BITSET(PIDT, pid_ch);
				dev_dbg(h->dev,
					"[DEBUG][SPI] PIDT 0x%p : 0x%08X\n",
					PIDT, tca_spi_readl(PIDT));
			}
			h->set_mpegts_pidmode(h, 1);
		}
	} else {
		dev_err(h->dev, "[ERROR][SPI] tsif: PID TABLE is so big !!!\n");
		ret = -EINVAL;
	}

	return ret;
}
