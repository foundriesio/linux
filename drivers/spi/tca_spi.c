/*
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux SPI Driver
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include "tca_spi.h"
//#include <mach/bsp.h>
//#include <mach/tca_spi.h>
//#include <mach/io.h>
//#include <mach/iomap.h>
//#include <mach/gpio.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/spi/tcc_tsif.h>
#include <linux/device.h>

int tca_spi_is_use_gdma(tca_spi_handle_t *h)
{
	return h->gdma_use;
}

int tca_spi_is_normal_slave(tca_spi_handle_t *h)
{
	return h->is_slave & TCC_GPSB_SLAVE_NORMAL;
}

static int tca_spi_isenabledma(struct tca_spi_handle *h)
{
	if(tca_spi_is_use_gdma(h))
	{
		return 0;
	}

	return tca_spi_readl(h->regs + TCC_GPSB_DMACTR) & Hw0 ? 1 : 0;
}

static int tca_spi_dmastop(struct tca_spi_handle *h)
{
	if (!h->tx_pkt_remain) {
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw3); /* Operation Disable */
	}

	// If GPSB use GDMA
	if(tca_spi_is_use_gdma(h))
	{
		return 0;
	}

	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw31 | Hw30); /* disable DMA Transmit & Receive */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, Hw29| Hw28);
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw0); /* DMA disable */

	return 0;
}

static int tca_spi_dmastop_slave(struct tca_spi_handle *h)
{
	// If GPSB use GDMA
	if(tca_spi_is_use_gdma(h))
	{
		return 0;
	}

	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw31 | Hw30); /* disable DMA Transmit & Receive */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, Hw29| Hw28);
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw0); /* DMA disable */

	return 0;
}

static int tca_spi_dmastart(struct tca_spi_handle *h)
{

	if (h->ctf) {
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw4);
	} else {
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw4);
	}
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw3); /* Operation Enable */

	// If GPSB use GDMA
	if(!tca_spi_is_use_gdma(h))
	{
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw31 | Hw30); /* enable DMA Transmit & Receive */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw17 | Hw16 | Hw15 | Hw14); /* set Multiple address mode */

		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, Hw16); /* disable DMA Packet Interrupt */
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, Hw17); /* enable DMA Done Interrupt */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, Hw20); /* set rx interrupt */

		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw0); /* DMA enable */
	}

	return 0;
}

static int tca_spi_dmastart_slave(struct tca_spi_handle *h)
{
	// If GPSB use GDMA
	if(!tca_spi_is_use_gdma(h))
	{
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw30); /* enable DMA Transmit */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw17 | Hw16 | Hw15 | Hw14); /* set Multiple address mode */

		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, Hw16); /* enable DMA Packet Interrupt */
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, Hw20); /* set rx interrupt */

		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMAICR, Hw16); /* enable DMA Packet Interrupt */

		if (h->dma_mode == 0) {
			TCC_GPSB_BITCSET(h->regs + TCC_GPSB_DMACTR, Hw5|Hw4, Hw29);	/* Normal mode & Continuous mode*/
		} else {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw4);				/* MPEG2-TS mode */
		}
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw0); /* DMA enable */
	}

	return 0;
}

static void tca_spi_clearfifopacket(struct tca_spi_handle *h)
{
	/* clear tx/rx FIFO & Packet counter  */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw15 | Hw14);
	if(!tca_spi_is_use_gdma(h))
	{
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw2);
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw2);
	}
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw15 | Hw14);
}

static void tca_spi_setpacketcnt(struct tca_spi_handle *h, int size)
{
	if(!tca_spi_is_use_gdma(h))
	{
		/* set packet count & size */
		tca_spi_writel((size & 0x1FFF), h->regs + TCC_GPSB_PACKET);
	}
}

static void tca_spi_setpacketcnt_slave(struct tca_spi_handle *h, int size)
{
	unsigned int packet_cnt = (h->dma_total_packet_cnt & 0x1FFF) - 1;
	unsigned int packet_size;
	unsigned int intr_packet_cnt = (h->dma_intr_packet_cnt & 0x1FFF) - 1;

	if(tca_spi_is_normal_slave(h))
		packet_size = (size & 0x1FFF);
	else
		packet_size = (TSIF_PACKET_SIZE & 0x1FFF);

	if(!tca_spi_is_use_gdma(h))
	{
		tca_spi_writel(((packet_cnt << 16) | packet_size), h->regs + TCC_GPSB_PACKET);
		TCC_GPSB_BITCSET(h->regs + TCC_GPSB_DMAICR, 0x1FFF, intr_packet_cnt);
	}
}

static void tca_spi_setbitwidth(struct tca_spi_handle *h, int width)
{
	int width_value = (width - 1) & 0x1F;

	/* set bit width */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw12 | Hw11 | Hw10 | Hw9 | Hw8);
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, (width_value << 8));

	if(!tca_spi_is_use_gdma(h))
	{
		if (width_value & Hw4) {
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw28);
		} else {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw28);
		}
	}
}

static void tca_spi_setdmaaddr(struct tca_spi_handle *h)
{
	// If GPSB use GDMA
	if(!tca_spi_is_use_gdma(h))
	{
		/* set dma txbase/rxbase & request DMA tx/rx */
		tca_spi_writel((u32)h->tx_dma.dma_addr, h->regs + TCC_GPSB_TXBASE);
		tca_spi_writel((u32)h->rx_dma.dma_addr, h->regs + TCC_GPSB_RXBASE);
	}

	if(h->tx_dma.dma_addr)
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN, Hw31);
	else
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw31);

	if(h->rx_dma.dma_addr)
		TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN, Hw30);
	else
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw30);
}

static void tca_spi_setdmaaddr_slave(struct tca_spi_handle *h)
{
	// If GPSB use GDMA
	if(!tca_spi_is_use_gdma(h))
	{
		/* set dma txbase/rxbase & request DMA tx/rx */
		tca_spi_writel(h->rx_dma.dma_addr, h->regs + TCC_GPSB_RXBASE);
		//Hw18~Hw16 : Revceive FIFO threshold for interrupt/DMA request
	}
	if(h->dma_mode == 0) {
		if(h->rx_dma.dma_addr)
		{
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN, Hw30|Hw15);
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw18|Hw17|Hw16);
		}
		else
		{
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw30);
		}
	} else {
		if(h->rx_dma.dma_addr)
		{
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_INTEN, Hw30|Hw15);
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw18|Hw17|Hw16);
		}
		else
		{
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw30);
		}
	}
}

static void tca_spi_regs_reset(struct tca_spi_handle *h)
{
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_STAT, Hw31 - Hw0);
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_INTEN, Hw31 - Hw0);
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw31 - Hw0);
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_CTRL, Hw31 - Hw0);
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_EVTCTRL, Hw31 - Hw0);
	if(!tca_spi_is_use_gdma(h))
	{
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_TXBASE, Hw31 - Hw0);
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_RXBASE, Hw31 - Hw0);
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_PACKET, Hw31 - Hw0);
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw31 - Hw0);
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMASTR, Hw31 - Hw0);
		TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMAICR, Hw31 - Hw0);
	}
}

static void tca_spi_hwinit(struct tca_spi_handle *h)
{
	/* Reset Registers */
	tca_spi_regs_reset(h);

	/* Disable Operation */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw3);

	h->set_bit_width(h, 32);

	h->set_dma_addr(h);

	/* Clear Fifo must be operated before operation enable */
	h->clear_fifo_packet(h);

	/* Set SPI Master mode*/
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw0 | Hw1 | Hw2);

	/* [SCK] Tx: risiing edge, Rx: falling edge */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw17 | Hw18);

#ifdef CONFIG_SPI_MST_LOOPBACK
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw6);
#endif
	if(tca_spi_readl(h->regs + TCC_GPSB_MODE) & Hw6)
		dev_dbg(h->dev, "[DEBUG][SPI] [%s] Telechips SPI Master Loop-back Test Enabled.\n", __func__);
}

static void tca_spi_hwinit_slave(struct tca_spi_handle *h)
{
	/* Reset Registers */
	tca_spi_regs_reset(h);

	/* Disable Operation */
	TCC_GPSB_BITCLR(h->regs + TCC_GPSB_MODE, Hw3);

	h->set_bit_width(h, 32);

	h->set_dma_addr(h);

	/* Clear Fifo must be operated before operation enable */
	h->clear_fifo_packet(h);

	/* Set SPI Slave Mode, Disable SDO */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw2 | Hw5);

#if !defined(CONFIG_ARCH_TCC898X) && !defined(CONFIG_ARCH_TCC897X)
	/* Set SDOE, SDO is driven only when cs is active. */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_EVTCTRL, Hw25);
#endif
	/* Enable Operation */
	TCC_GPSB_BITSET(h->regs + TCC_GPSB_MODE, Hw3);
}

static void tca_spi_set_mpegtspidmode(struct tca_spi_handle *h, int is_set)
{
	h->hw_init(h);
	if(!tca_spi_is_use_gdma(h))
	{
		if (is_set) {
			TCC_GPSB_BITSET(h->regs + TCC_GPSB_DMACTR, Hw19 | Hw18);
		} else {
			TCC_GPSB_BITCLR(h->regs + TCC_GPSB_DMACTR, Hw19 | Hw18);
		}
	}
}

#ifdef TCC_USE_GFB_PORT
static void tca_spi_set_port(struct tca_spi_handle *h, struct tca_spi_port_config *port)
{
	void __iomem *gpsb_pcf_regs = h->port_regs;
	unsigned int pcfg_offset = TCC_GPSB_PCFG_OFFSET * h->gpsb_channel;
	int i;

	/* [0]SDI [1]SCLK [2]SFRM [3]SDO */
	for(i=0; i<4; i++)
	{
		TCC_GPSB_BITCSET(gpsb_pcf_regs + pcfg_offset, (Hw8-Hw0)<<(i *TCC_GPSB_PCFG_SHIFT), port->gpsb_port[i] << (i *TCC_GPSB_PCFG_SHIFT));
	}

	dev_info(h->dev, "[INFO][SPI] [%s][GFB]: SDI: 0x%X, SCLK: 0x%X, SFRM: 0x%X, SDO: 0x%X\n", __func__,
			port->gpsb_port[0], port->gpsb_port[1], port->gpsb_port[2], port->gpsb_port[3]);
	dev_info(h->dev, "[INFO][SPI] [%s][CH:%d] PCFG(@0x%X): 0x%x\n", __func__,
			h->gpsb_channel, (unsigned int)(gpsb_pcf_regs + pcfg_offset), tca_spi_readl(gpsb_pcf_regs + pcfg_offset));
}
static void tca_spi_clear_port(struct tca_spi_handle *h)
{
	void __iomem *gpsb_pcf_regs = h->port_regs;
	unsigned int pcfg_offset = TCC_GPSB_PCFG_OFFSET * h->gpsb_channel;

	TCC_GPSB_BITCSET(gpsb_pcf_regs + pcfg_offset, 0xFFFFFFFF, 0xFFFFFFFF);
}
#else
static void tca_spi_set_port(struct tca_spi_handle *h, struct tca_spi_port_config *port)
{
	unsigned int gpsb_channel, gpsb_port;
	void __iomem *gpsb_pcf_regs = h->port_regs;
	int i;

	gpsb_channel = h->gpsb_channel;
	gpsb_port    = h->gpsb_port;

	dev_info(h->dev, "[INFO][SPI] [%s] CH: %d PORT: %d\n", __func__, gpsb_channel, port->gpsb_port);

	if(h->gpsb_channel <= 3)
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG0, (Hw8-Hw0)<<(gpsb_channel*8), gpsb_port<<(gpsb_channel*8));
	else
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG1, (Hw8-Hw0)<<((gpsb_channel-4)*8), gpsb_port<<((gpsb_channel-4)*8));

	for(i=0;i<TCC_GPSB_MAX_CH;i++)
	{
		unsigned int _gpsb_port;
		if(i == h->gpsb_channel) continue;

		if(i < 4)
		{
			_gpsb_port = ((tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG0) & (Hw8-Hw0)<<(i*8)) >> (i*8));
			if (_gpsb_port == gpsb_port) {
				TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG0, (Hw8-Hw0)<<(i*8), 0xFF<<(i*8));

				dev_warn(h->dev,
						"[WARN][SPI] warning port conflict! [[ch %d[%d]]] : ch %d[%d] pcfg0: 0x%08X pcfg1: 0x%08X\n",
						gpsb_channel, gpsb_port, i, _gpsb_port,
						tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG0),
						tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG1));
			}
		}
		else
		{
			_gpsb_port = ((tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG1) & (Hw8-Hw0)<<((i-4)*8)) >> ((i-4)*8));
			if (_gpsb_port == gpsb_port) {
				TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG1, (Hw8-Hw0)<<((i-4)*8), 0xFF<<((i-4)*8));

				dev_warn(h->dev,
						"[WARN][SPI] warning port conflict! [[ch %d[%d]]] : ch %d[%d] pcfg0: 0x%08X pcfg1: 0x%08X\n",
						gpsb_channel, gpsb_port, i, _gpsb_port,
						tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG0),
						tca_spi_readl(gpsb_pcf_regs + TCC_GPSB_PCFG1));
			}
		}
	}
}

static void tca_spi_clear_port(struct tca_spi_handle *h)
{
	unsigned int gpsb_channel;
	void __iomem *gpsb_pcf_regs = h->port_regs;
	gpsb_channel = h->gpsb_channel;
	if(gpsb_channel <= 3)
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG0, (Hw8-Hw0)<<(gpsb_channel*8), 0xFF<<(gpsb_channel*8));
	else
		TCC_GPSB_BITCSET(gpsb_pcf_regs + TCC_GPSB_PCFG1, (Hw8-Hw0)<<((gpsb_channel-4)*8), 0xFF<<((gpsb_channel-4)*8));
}
#endif

/******************************
 * return value
 *
 * ret == 0: success
 * ret > 0 or ret < 0: fail
 ******************************/
int tca_spi_init(tca_spi_handle_t *h,
		void __iomem *regs,
		unsigned int phy_reg_base,
		void __iomem *port_regs,
		void __iomem *pid_regs,
		int irq,
		dma_alloc_f tea_dma_alloc,
		dma_free_f tea_dma_free,
		int dma_size,
		int id,
		int is_slave,
		struct tca_spi_port_config *port,
		const char *gpsb_name,
		struct device *dev)
{
	int ret = -1;
	struct tcc_spi_dma	dma;

	if (h) {
		dma = h->dma;
		if(!dma.chan_rx)

			memset(h, 0, sizeof(tca_spi_handle_t));
	}

	// Memory Copy of GPSB Port Configuration
	memcpy(&h->port_config, port, sizeof(struct tca_spi_port_config));

	if (regs) {
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
			int i;
			for(i=0;i<4;i++)
				h->gpsb_port[i] = port->gpsb_port[i];
		}
#else
		h->gpsb_port = port->gpsb_port;
#endif

		if(!strncmp(gpsb_name,"gpsb0", 5))
			h->gpsb_channel = 0;
		else if(!strncmp(gpsb_name,"gpsb1", 5))
			h->gpsb_channel = 1;
		else if(!strncmp(gpsb_name,"gpsb2", 5))
			h->gpsb_channel = 2;
		else if(!strncmp(gpsb_name,"gpsb3", 5))
			h->gpsb_channel = 3;
		else if(!strncmp(gpsb_name,"gpsb4", 5))
			h->gpsb_channel = 4;
		else if(!strncmp(gpsb_name,"gpsb5", 5))
			h->gpsb_channel = 5;
		else
			return ret;

		if(h->gpsb_channel < 3)
			h->gdma_use = 0;
		else
			h->gdma_use = 1;

		h->dma_stop = is_slave ? tca_spi_dmastop_slave : tca_spi_dmastop;
		h->dma_start = is_slave ? tca_spi_dmastart_slave : tca_spi_dmastart;
		h->clear_fifo_packet = tca_spi_clearfifopacket;
		h->set_packet_cnt = is_slave ? tca_spi_setpacketcnt_slave : tca_spi_setpacketcnt;
		h->set_bit_width = tca_spi_setbitwidth;
		h->set_dma_addr = is_slave ? tca_spi_setdmaaddr_slave : tca_spi_setdmaaddr;
		h->hw_init = is_slave ? tca_spi_hwinit_slave : tca_spi_hwinit;
		h->is_enable_dma = tca_spi_isenabledma;
		h->set_mpegts_pidmode = tca_spi_set_mpegtspidmode;

		h->tea_dma_alloc = tea_dma_alloc;
		h->tea_dma_free = tea_dma_free;

		h->dma_total_size = dma_size;
		h->dma_mode = 1;	/* default MPEG2-TS DMA mode */

		h->ctf = 0;
		h->tx_pkt_remain= 0;

		tca_spi_set_port(h, port);

		if (h->tea_dma_alloc) {
			if (h->tea_dma_alloc(&(h->rx_dma), dma_size,h->dev) == 0) {
				if (is_slave) {
					ret = 0;
				} else if (h->tea_dma_alloc(&(h->tx_dma), dma_size,h->dev) == 0 && h->tea_dma_alloc(&(h->tx_dma_1), dma_size,h->dev) == 0) {
					ret = 0;
				}
			}
		} else {
			/* Already, tsif has rx_dma buf */
			ret = 0;
		}
	}

	return ret;
}

void tca_spi_clean(tca_spi_handle_t *h)
{
	if (h) {
		struct tcc_spi_dma	dma;
		dma = h->dma;
		if (h->tea_dma_free) {
			h->tea_dma_free(&(h->tx_dma), h->dev);
			h->tea_dma_free(&(h->rx_dma), h->dev);
			h->tea_dma_free(&(h->tx_dma_1), h->dev);
		}

		/* Clear PCFG (only one channel being used)*/
		tca_spi_clear_port(h);

		memset(h, 0, sizeof(tca_spi_handle_t));
		h->dma = dma;
	}
}

int tca_spi_register_pids(tca_spi_handle_t *h, unsigned int *pids, unsigned int count)
{
	int ret = 0, gpsb_channel = -1;
	gpsb_channel = h->gpsb_channel;

	//supporting pids is 32
	if (count <= 32) {
#ifdef CONFIG_ARM64
		void __iomem *PIDT;
#else
		volatile unsigned long* PIDT;
#endif
		int i = 0, pid_ch = 0;

		if(gpsb_channel == 0)
			pid_ch = Hw29;//HwGPSB_PIDT_CH0;
		else if(gpsb_channel == 1)
			pid_ch = Hw30;//HwGPSB_PIDT_CH1;
		else if(gpsb_channel == 2)
			pid_ch = Hw31;//HwGPSB_PIDT_CH2;

		for (i = 0; i < 32; i++) {
#ifdef CONFIG_ARM64
			PIDT = h->pid_regs + (4*i);
			tca_spi_writel(0x0, PIDT);
#else
			PIDT = (volatile unsigned long *)(h->pid_regs+4*i);//(volatile unsigned long *)tcc_p2v(HwGPSB_PIDT(i));
			*PIDT = 0;
#endif
		}
		if (count > 0) {
			for (i = 0; i < count; i++) {
#ifdef CONFIG_ARM64
				PIDT = h->pid_regs + (4*i);
				tca_spi_writel(pids[i] & 0x1FFFFFFF, PIDT);
				TCC_GPSB_BITSET(PIDT, pid_ch);
				dev_dbg(h->dev, "[DEBUG][SPI] PIDT 0x%p : 0x%08X\n", PIDT, tca_spi_readl(PIDT));
#else
				PIDT = (volatile unsigned long *)(h->pid_regs+4*i);//(volatile unsigned long *)tcc_p2v(HwGPSB_PIDT(i));
				*PIDT = pids[i] & 0x1FFFFFFF;
				BITSET(*PIDT, pid_ch);
				dev_dbg(h->dev, "[DEBUG][SPI] PIDT 0x%08X : 0x%08X\n", (unsigned int)PIDT, (unsigned int)*PIDT);
#endif
			}
			h->set_mpegts_pidmode(h, 1);
		}
	}
	else {
		dev_err(h->dev, "[ERROR][SPI] tsif: PID TABLE is so big !!!\n");
		ret = -EINVAL;
	}
	return ret;
}

EXPORT_SYMBOL(tca_spi_init);
EXPORT_SYMBOL(tca_spi_clean);
EXPORT_SYMBOL(tca_spi_register_pids);

