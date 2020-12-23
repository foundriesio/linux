// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef SPI_TCC_H
#define SPI_TCC_H

#define tcc_spi_writel	__raw_writel
#define tcc_spi_readl	__raw_readl

#define TCC_GPSB_BITSET(X, MASK)\
(tcc_spi_writel(((tcc_spi_readl(X)) | ((u32)(MASK))), X))

#define TCC_GPSB_BITSCLR(X, SMASK, CMASK)\
(tcc_spi_writel((((tcc_spi_readl(X)) | ((u32)(SMASK))) & ~((u32)(CMASK))), X))

#define TCC_GPSB_BITCSET(X, CMASK, SMASK)\
(tcc_spi_writel((((tcc_spi_readl(X)) & ~((u32)(CMASK))) | ((u32)(SMASK))), X))

#define TCC_GPSB_BITCLR(X, MASK)\
(tcc_spi_writel(((tcc_spi_readl(X)) & ~((u32)(MASK))), X))

#define TCC_GPSB_BITXOR(X, MASK)\
(tcc_spi_writel(((tcc_spi_readl(X)) ^ ((u32)(MASK))), X))

#define TCC_GPSB_ISZERO(X, MASK)\
(!(tcc_spi_readl(X)) & ((u32)(MASK)))

#define	TCC_GPSB_ISSET(X, MASK)\
((tcc_spi_readl(X)) & ((u32)(MASK)))

#ifdef CONFIG_TCC_DMA
#include <linux/scatterlist.h>
#define TCC_DMA_ENGINE
#endif

#ifndef CONFIG_ARCH_TCC802X
#else
#define TCC_USE_GFB_PORT
#endif

#ifdef CONFIG_TCC_GPSB_PARTIAL_TRANSFERS
#define TCC_SPI_PARTIAL_TRANSFERS
#endif

/* TCC GPSB Maximum Operating Clock */
#define TCC_GPSB_MAX_FREQ	25000000 /* 25 MHz */

/* TCC GPSB Clock mode */
#define TCC_SPI_MODE_BITS\
	(SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST | SPI_LOOP)

/*
 * GPSB Registers
 */
#define TCC_GPSB_PORT	0x00UL
/* Data port */
#define TCC_GPSB_STAT	0x04UL
/* Status register */
#define TCC_GPSB_INTEN	0x08UL
/* Interrupt enable */
#define TCC_GPSB_MODE	0x0CUL
/* Mode register */
#define TCC_GPSB_CTRL	0x10UL
/* Control register */
#define TCC_GPSB_EVTCTRL 0x14UL
/* Counter and Ext. Event Control */
#define TCC_GPSB_CCV	0x18UL
/* Counter current value */
#define TCC_GPSB_TXBASE	0x20UL
/* TX base address register */
#define TCC_GPSB_RXBASE	0x24UL
/* RX base address register */
#define TCC_GPSB_PACKET	0x28UL
/* Packet register */
#define TCC_GPSB_DMACTR	0x2CUL
/* DMA control register */
#define TCC_GPSB_DMASTR	0x30UL
/* DMA status register */
#define TCC_GPSB_DMAICR	0x34UL
/* DMA interrupt control register */

/*
 * GPSB STAT Register
 */
#define TCC_GPSB_STAT_ERR          ((u32)0x1FU << 5U)

/*
 * GPSB INTEN Register
 */
#define TCC_GPSB_INTEN_DW			BIT(31)
/* DMA Request enable for TX FIFO */
#define TCC_GPSB_INTEN_DR			BIT(30)
/* DMA Request enable for RX FIFO */
#define TCC_GPSB_INTEN_SHT			BIT(27)
/* TX half-word swap in word */
#define TCC_GPSB_INTEN_SBT			BIT(26)
/* TX byte swap in half-word */
#define TCC_GPSB_INTEN_SHR			BIT(25)
/* RX half-word swap in word */
#define TCC_GPSB_INTEN_SBR			BIT(24)
/* RX half-word swap in word */
#define TCC_GPSB_INTEN_CFGWTH(x)	((u32)((x) & 0x7UL) << 20)
/* Transmit FIFO threshold for interrupt/DMA request */
#define TCC_GPSB_INTEN_CFGWTH_MASK	(0x7UL << 20)
#define TCC_GPSB_INTEN_CFGRTH(x)	((u32)((x) & 0x7UL) << 16)
/* Receive FIFO threshold for interrupt/DMA request */
#define TCC_GPSB_INTEN_CFGRTH_MASK	(0x7UL << 16)
#define TCC_GPSB_INTEN_RC			BIT(15)
/* Clear status[8:0] at the end of read cycle */

/*
 * GPSB MODE Register
 */
#define TCC_GPSB_MODE_DIVLDV(x)		((u32)((x) & 0xFFUL) << 24)
/* Clock divider load value (FSCKO = FGCLK / ((DIVLDV+1)*2)) */
#define TCC_GPSB_MODE_DIVLDV_MASK	((u32)0xFFUL << 24)
#define TCC_GPSB_MODE_TRE			BIT(23)
/* Master recovery time (tRECV = (TRE + 1) * (SCKO period)) */
#define TCC_GPSB_MODE_THL			BIT(22)
/* Master hold time (tHOLD = (THL + 1) * (SCKO period)) */
#define TCC_GPSB_MODE_TSU			BIT(21)
/* Master setup time (tSETUP = (TSU + 1) * (SCKO period)) */
#define TCC_GPSB_MODE_PCS			BIT(20)
/* Polarity control for CS(FRM) - Master Only */
#define TCC_GPSB_MODE_PCD			BIT(19)
/* Polarity control for CMD(FRM) - Master Only */
#define TCC_GPSB_MODE_PWD			BIT(18)
/* Polarity control for transmitting data - Master Only */
#define TCC_GPSB_MODE_PRD			BIT(17)
/* Polarity control for receiving data - Master Only */
#define TCC_GPSB_MODE_PCK			BIT(16)
/* Polarity control for serial clock */
#define TCC_GPSB_MODE_CRF			BIT(15)
/* Clear receive FIFO counter */
#define TCC_GPSB_MODE_CWF			BIT(14)
/* Clear transmit FIFO counter */
#define TCC_GPSB_MODE_BWS(x)		((u32)((x) & 0x1FUL) << 8)
/* Bit width selection */
#define TCC_GPSB_MODE_BWS_MASK		((u32)0x1FU << 8)
#define TCC_GPSB_MODE_SD			BIT(7)
/* Data shift direction control (0: MSB first) */
#define TCC_GPSB_MODE_LB			BIT(6)
/* Data loop-back enable */
#define TCC_GPSB_MODE_SDO			BIT(5)
/* SDO output disable - Slave Only */
#define TCC_GPSB_MODE_CTF			BIT(4)
/* Continuous transfer mode enable (0: Single 1: Continuous) */
#define TCC_GPSB_MODE_EN			BIT(3)
/* Operation enable bit */
#define TCC_GPSB_MODE_SLV			BIT(2)
/* Slave mode configuration */
#define TCC_GPSB_MODE_MD(x)			(u32)((x) & 0x3UL)
/* Operation mode (0b00: SPI compatible 0b1x: reserved) */
#define TCC_GPSB_MODE_MD_MASK		(BIT(1) | BIT(0))

/*
 * GPSB EVTCTRL Register
 */
#define TCC_GPSB_EVTCTRL_SDOE      BIT(25)
#define TCC_GPSB_EVTCTRL_CONTM(x)  ((u32)((x) & 0x3UL) << 22)

/*
 * GPSB PACKET Register
 */
#define TCC_GPSB_PACKET_COUNT(x)	((u32)((x) & 0x1FFFUL) << 16)
/* Packet number information (COUNT + 1) */
#define TCC_GPSB_PACKET_COUNT_MASK	(0x1FFFUL << 16)
#define TCC_GPSB_PACKET_SIZE(x)		(u32)((x) & 0x1FFFUL)
/* Packet size information */
#define TCC_GPSB_PACKET_SIZE_MASK	0x1FFFUL
#define TCC_GPSB_PACKET_MAX_SIZE	0x1FFCU

/*
 * GPSB DMA Control Register
 */
#define TCC_GPSB_DMACTR_DTE			BIT(31)
/* Transmit DMA request enable */
#define TCC_GPSB_DMACTR_DRE			BIT(30)
/* Receive DMA request enable */
#define TCC_GPSB_DMACTR_CT			BIT(29)
/* Continuous mode enable - Master Only */
#define TCC_GPSB_DMACTR_END			BIT(28)
/* Byte endian mode register (0: Little 1: Big) */
#define TCC_GPSB_DMACTR_MP			BIT(19)
/* PID match mode register (1: MPEG2-TS mode) */
#define TCC_GPSB_DMACTR_MS			BIT(18)
/* Sync byte match control register (1: MPEG2-TS mode) */
#define TCC_GPSB_DMACTR_TXAM(x)		((u32)((x) & 0x3UL) << 16)
#define TCC_GPSB_DMACTR_TXAM_MASK	((u32)0x3UL << 16)
#define TCC_GPSB_DMACTR_RXAM(x)		((u32)((x) & 0x3UL) << 14)
/* addressing mode */
/* (0: Multiple Packet 1: Fixed Address Others: Single Packet) */
#define TCC_GPSB_DMACTR_RXAM_MASK	(0x3UL << 14)
#define TCC_GPSB_DMACTR_MD(x)		((u32)((x) & 0x3UL) << 4)
/* DMA mode register (0b00: Normal 0b01: MPEG2-TS 0b1x: Reserved) */
#define TCC_GPSB_DMACTR_MD_MASK		(0x3UL << 4)
#define TCC_GPSB_DMACTR_PCLR		BIT(2)
/* Clear TX/RX packet counter */
#define TCC_GPSB_DMACTR_TSIF		BIT(1)
/* IOBUS TSIF Select register */
#define TCC_GPSB_DMACTR_EN			BIT(0)
/* DMA enable register */

/*
 * GPSB IRQ Register
 */
#define TCC_GPSB_DMAICR_ISD			BIT(29)
/* IRQ status for "Done Interrupt" */
#define TCC_GPSB_DMAICR_ISP			BIT(28)
/* IRQ status for "Packet Interrupt" */
#define TCC_GPSB_DMAICR_IRQS		BIT(20)
/* IRQ select register (0: Receiving 1: Transmitting) */
#define TCC_GPSB_DMAICR_IED			BIT(17)
/* IRQ enable for "Done Interrupt" */
#define TCC_GPSB_DMAICR_IEP			BIT(16)
/* IRQ enable for "Packet Interrupt" */
#define TCC_GPSB_DMAICR_IRQPCNT(x)	((x) & 0x1FFFUL)
/* IRQ packet count register */
#define TCC_GPSB_DMAICR_IRQPCNT_MASK 0x1FFFUL

/*
 * GPSB PORT Registers
 */
#ifdef TCC_USE_GFB_PORT
/* Support GFB Port Configuration */
#define TCC_GPSB_PCFG0	0x00
#define TCC_GPSB_PCFG1	0x04
#define TCC_GPSB_PCFG2	0x08
#define TCC_GPSB_PCFG3	0x0C
#define TCC_GPSB_PCFG4	0x10
#define TCC_GPSB_PCFG5	0x14
#define TCC_GPSB_PCFG_OFFSET 0x4
#define TCC_GPSB_PCFG_SHIFT	8

/* Channel IRQ status register */
#define TCC_GPSB_CIRQST	0x1C

/* The maximum number of channel */
#define TCC_GPSB_MAX_CH	6

#else
/* Support Legacy Port Configuration */
#define TCC_GPSB_PCFG0	0x00
#define TCC_GPSB_PCFG1	0x04

/* Channel IRQ status register */
#define TCC_GPSB_CIRQST	0x0C

/*
 * GPSB Access Control Register
 */
#define TCC_GPSB_AC0_START 0x00
#define TCC_GPSB_AC0_LIMIT 0x04
#define TCC_GPSB_AC1_START 0x08
#define TCC_GPSB_AC1_LIMIT 0x0C
#define TCC_GPSB_AC2_START 0x10
#define TCC_GPSB_AC2_LIMIT 0x14
#define TCC_GPSB_AC3_START 0x18
#define TCC_GPSB_AC3_LIMIT 0x1C
/* The maximum number of channel */
#define TCC_GPSB_MAX_CH	6
#endif

#define TCC_SPI_DMA_MAX_SIZE	TCC_GPSB_PACKET_MAX_SIZE

/* TCC GDMA Date for GPSB w/o dedicated DMA channel */
struct tcc_spi_gdma {
	/* DMA ENGINE */
	struct dma_chan *chan_rx;
	struct dma_chan *chan_tx;
	struct scatterlist sgrx;
	struct scatterlist sgtx;
	struct dma_async_tx_descriptor	*data_desc_rx;
	struct dma_async_tx_descriptor	*data_desc_tx;

	/* dma */
	struct device *dma_dev;

	struct device	*dev;
};

/* DMA Buffer Data */
struct tcc_spi_dma_buf {
	/* Virtual address */
	void *v_addr;
	/* DMA address */
	dma_addr_t dma_addr;
	/* Buffer size */
	size_t size;
};

/* TCC GPSB SPI Platform Data */
struct tcc_spi_pl_data {
	/* Bus id*/
	uint32_t id;

	/* GPSB channel */
	int32_t gpsb_channel;

	/* Driver name */
	const char	*name;

	/* SPI Master/Slave mode */
	bool is_slave;

	/* Port configuration */
#ifdef TCC_USE_GFB_PORT
	uint32_t port[4];
#else
	uint32_t port;
#endif

	/* GDMA usage */
	int32_t dma_enable;

	/* GDMA data */
	struct tcc_spi_gdma	dma;

	/* Clock */
	struct clk	*hclk;
	struct clk	*pclk;

	/* CONTM support */
	bool contm_support;

	/* Continuous mode */
	bool ctf;
};

/* TCC GPSB SPI Data */
struct tcc_spi {
	/* GPSB base phy address */
	resource_size_t		pbase;
	/* GPSB base re-mapped address */
	void __iomem		*base;
	/* GPSB pcfg re-mapped address */
	void __iomem		*pcfg;
	/* GPSB Access Control re-mapped address */
	void __iomem		*ac;

	/* IRQ number */
	int32_t			irq;

	struct device		*dev;

	struct spi_master	*master;
	struct spi_message	*msg;
	struct spi_transfer	*transfer;

	/* DMA buffer */
	size_t			dma_buf_size;
	struct tcc_spi_dma_buf	tx_buf;
	struct tcc_spi_dma_buf	rx_buf;
	uint32_t cur_tx_pos;
	uint32_t cur_rx_pos;
	uint32_t cur_pos;

	/* Platform data */
	struct tcc_spi_pl_data	*pd;

	/* GDMA data */
	struct tcc_spi_gdma	dma;

	/* Bitwidth */
	uint32_t		bits;

	/* SCLK */
	uint32_t		clk;

	/* Pin-control */
	struct pinctrl *pinctrl;

	/* message queue */
	struct workqueue_struct *workqueue;
	struct work_struct work;
	spinlock_t lock;
	struct list_head queue;

	/* for spi stransfer */
	int32_t tx_packet_remain;
	uint32_t current_remaining_bytes;
	struct spi_transfer *current_xfer;
	struct completion xfer_complete;
};

#endif /* SPI_TCC_H */

