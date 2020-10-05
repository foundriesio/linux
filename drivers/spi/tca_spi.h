/*
 * Author:  Telechips Inc.
 * Created: 10th Jun, 2013
 * Description: LINUX SPI DRIVER FUNCTIONS
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

#ifndef TCA_SPI_H
#define TCA_SPI_H

#include <linux/scatterlist.h>
#ifdef CONFIG_TCC_DMA
#define TCC_DMA_ENGINE
#endif

#ifdef CONFIG_ARCH_TCC802X
#define TCC_USE_GFB_PORT
#endif

#ifdef CONFIG_TCC_GPSB_PARTIAL_TRANSFERS
#define TCC_SPI_PARTIAL_TRANSFERS
#endif

#define tca_spi_writel	__raw_writel
#define tca_spi_readl	__raw_readl

#ifndef ENABLE
#define ENABLE 1
#endif
#ifndef DISABLE
#define DISABLE 0
#endif

#define ON 1
#define OFF 0

#define FALSE 0
#define TRUE 1

#define BITSET(X, MASK) ((u32)(X) |= (u32)(MASK))
#define BITSCLR(X, SMASK, CMASK) do { \
	(u32)(X) |= (u32)(SMASK); \
	(u32)(X) &= ~((u32)(CMASK)); \
} while (0)
#define BITCSET(X, CMASK, SMASK) do { \
	(u32)(X) &= ~(u32)(CMASK); \
	(u32)(X) |= (u32)(SMASK); \
} while (0)
#define BITCLR(X, MASK) ((u32)(X) &= ~((u32)(MASK)))
#define BITXOR(X, MASK) ((u32)(X) ^= (u32)(MASK))
#define ISZERO(X, MASK) (!(((u32)(X)) & ((u32)(MASK))))
#define ISSET(X, MASK) ((u32)(X) & ((u32)(MASK)))

#define TCC_GPSB_BITSET(X, MASK) do { \
	u32 tmp_reg_val = tca_spi_readl((X)) | (u32)(MASK); \
	tca_spi_writel(tmp_reg_val, (X)); \
} while (0)
#define TCC_GPSB_BITSCLR(X, SMASK, CMASK) do { \
	u32 tmp_reg_val = tca_spi_readl((X)) | (u32)(SMASK); \
	tmp_reg_val &= ~((u32)(CMASK)); \
	tca_spi_writel(tmp_reg_val, (X)); \
} while (0)
#define TCC_GPSB_BITCSET(X, CMASK, SMASK) do { \
	u32 tmp_reg_val = tca_spi_readl((X)) & ~((u32)(CMASK)); \
	tmp_reg_val |= (u32)(SMASK); \
	tca_spi_writel(tmp_reg_val, (X)); \
} while (0)
#define TCC_GPSB_BITCLR(X, MASK) do { \
	u32 tmp_reg_val = tca_spi_readl((X)) & ~((u32)(MASK)); \
	tca_spi_writel(tmp_reg_val, (X)); \
} while (0)
#define TCC_GPSB_BITXOR(X, MASK) do { \
	u32 tmp_reg_val = tca_spi_readl((X)) ^ (u32)(MASK); \
	tca_spi_writel(tmp_reg_val, (X)); \
} while (0)
#define TCC_GPSB_ISZERO(X, MASK) (!(tca_spi_readl((X))) & ((u32)(MASK)))
#define TCC_GPSB_ISSET(X, MASK) ((tca_spi_readl((X))) & ((u32)(MASK)))

/* DMA Default Interrupt Interval for GPSB TSIF */
#define TCC_GPSB_TSIF_DEF_INTR_INTERVAL 4

/*
 * GPSB Registers
 */
#define TCC_GPSB_PORT		0x00 /* Data port */
#define TCC_GPSB_STAT		0x04 /* Status register */
#define TCC_GPSB_INTEN		0x08 /* Interrupt enable */
#define TCC_GPSB_MODE		0x0C /* Mode register */
#define TCC_GPSB_CTRL		0x10 /* Control register */
#define TCC_GPSB_EVTCTRL	0x14 /* Counter and Ext. Event Control */
#define TCC_GPSB_CCV		0x18 /* Counter current value */
#define TCC_GPSB_TXBASE		0x20 /* TX base address register */
#define TCC_GPSB_RXBASE		0x24 /* RX base address register */
#define TCC_GPSB_PACKET		0x28 /* Packet register */
#define TCC_GPSB_DMACTR		0x2C /* DMA control register */
#define TCC_GPSB_DMASTR		0x30 /* DMA status register */
#define TCC_GPSB_DMAICR		0x34 /* DMA interrupt control register */

/*
 * GPSB PORT Registers
 */
#ifdef TCC_USE_GFB_PORT
#define TCC_GPSB_PCFG0	0x00 /* Port configuration register 0 */
#define TCC_GPSB_PCFG1	0x04 /* Port configuration register 1 */
#define TCC_GPSB_PCFG2	0x08 /* Port configuration register 2 */
#define TCC_GPSB_PCFG3	0x0C /* Port configuration register 3 */
#define TCC_GPSB_PCFG4	0x10 /* Port configuration register 4 */
#define TCC_GPSB_PCFG5	0x14 /* Port configuration register 5 */
#define TCC_GPSB_CIRQST	0x1C /* Channel IRQ status register */
#define TCC_GPSB_PCFG_OFFSET 0x4
#define TCC_GPSB_PCFG_SHIFT  8
#define TCC_GPSB_MAX_CH	6
#else
#define TCC_GPSB_PCFG0	0x00 /* Port configuration register 0 */
#define TCC_GPSB_PCFG1	0x04 /* Port configuration register 1 */
#define TCC_GPSB_CIRQST	0x0C /* Channel IRQ status register */
#define TCC_GPSB_MAX_CH	6
#endif

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

#define SPI_GDMA_PACKET_SIZE 256
#define MST_GDMA_BSIZE 1 /* read cycle per 1 burst transfer */
#define SLV_GDMA_BSIZE 1 /* read cycle per 1 burst transfer */

#define MST_GDMA_WSIZE 1 /* in Bytes */
#define SLV_GDMA_WSIZE 1 /* in Bytes */
/*
 * This is for TSIF BLOCK.
 * '0' means there is no TSIF BLOCK device--ehk23
 */
#define SPI_MINOR_NUM_OFFSET 2

/* SPI SLAVE normal mode dev name. */
#define SPI_SLAVE_DEV_NAMES "tcc-spislv%d"

#define WAIT_TIME_FOR_DMA_DONE (1000 * 8)

#define TCC_GPSB_MASTER         0x0UL
#define TCC_GPSB_SLAVE_TSIF     0x1UL
#define TCC_GPSB_SLAVE_NORMAL   0x2UL

struct tea_dma_buf {
	void *v_addr;
	dma_addr_t dma_addr;
	int buf_size; /* total size of DMA */
};

struct tca_spi_port_config {
	int          gpsb_id;
#ifdef TCC_USE_GFB_PORT
	unsigned int gpsb_port[4];
#else
	unsigned int gpsb_port;
#endif
	const char *name;
};

struct tcc_dma_slave {
	struct device	*dma_dev;
};

struct tcc_spi_dma {
	struct dma_chan *chan_rx;
	struct dma_chan *chan_tx;
	struct scatterlist sgrx;
	struct scatterlist sgtx;
	struct dma_async_tx_descriptor	*data_desc_rx;
	struct dma_async_tx_descriptor	*data_desc_tx;

	struct tcc_dma_slave dma_slave;
};

struct tca_spi_handle {
	struct device *dev;
	void __iomem *regs;
	phys_addr_t phy_reg_base; /* GPSB Phy Register */
	void __iomem *port_regs; /* Port Configuration Register */
	void __iomem *pid_regs; /* PID Table Register */
	struct tea_dma_buf tx_dma, rx_dma;
	struct tea_dma_buf tx_dma_1;
	struct tca_spi_port_config port_config;
	int flag;
	int irq;
	void *private_data;
	int id;
	int is_slave;
#ifdef TCC_USE_GFB_PORT
	unsigned int gpsb_port[4];
#else
	int gpsb_port;
#endif
	int gpsb_channel;

	int (*is_enable_dma)(struct tca_spi_handle *h);
	int (*dma_stop)(struct tca_spi_handle *h);
	int (*dma_start)(struct tca_spi_handle *h);
	void (*clear_fifo_packet)(struct tca_spi_handle *h);
	void (*set_packet_cnt)(struct tca_spi_handle *h, int cnt);
	void (*set_bit_width)(struct tca_spi_handle *h, int width);
	void (*set_dma_addr)(struct tca_spi_handle *h);
	void (*hw_init)(struct tca_spi_handle *h);
	void (*set_mpegts_pidmode)(struct tca_spi_handle *h, int is_set);

	/* tea function. */
	int (*tea_dma_alloc)(struct tea_dma_buf *tdma, unsigned int size,
				struct device *dev, int id);
	/* tea function. */
	void (*tea_dma_free)(struct tea_dma_buf *tdma,
				struct device *dev, int id);

	int clk; /* Mhz */
	int ctf; /* continuous transfer mode */
	int tx_pkt_remain;

	/* add for slave */
	unsigned int dma_total_packet_cnt, dma_intr_packet_cnt;
	int q_pos, cur_q_pos;
	int dma_total_size;
	int dma_mode;

	/* backup gpsb regs */
	unsigned int bak_gpio_port;
	unsigned int bak_gpsb_port;

	/* DMA-engine specific */
	int gdma_use;
	struct tcc_spi_dma	dma;
};

#define tca_spi_setCPOL(R, S) do { \
	if (S) { \
		tca_spi_writel(tca_spi_readl((R) + TCC_GPSB_MODE) | BIT(16), \
				(R) + TCC_GPSB_MODE); \
	} else { \
		tca_spi_writel(tca_spi_readl((R) + TCC_GPSB_MODE) & ~BIT(16), \
				(R) + TCC_GPSB_MODE); \
	} \
} while (0)

#define tca_spi_setCPHA(R, S) do { \
	if (S) { \
		tca_spi_writel( \
			tca_spi_readl((R) + TCC_GPSB_MODE) | \
				(BIT(18) | BIT(17)), \
			(R) + TCC_GPSB_MODE); \
	} else { \
		tca_spi_writel( \
			tca_spi_readl((R) + TCC_GPSB_MODE) & \
				~(BIT(18) | BIT(17)), \
			(R) + TCC_GPSB_MODE); \
} while (0)

#define tca_spi_setCS_HIGH(R, S) do { \
	if (S) { \
		tca_spi_writel( \
			tca_spi_readl((R) + TCC_GPSB_MODE) | \
				(BIT(20) | BIT(19)), \
			(R) + TCC_GPSB_MODE); \
	} else { \
		tca_spi_writel( \
			tca_spi_readl((R) + TCC_GPSB_MODE) & \
				~(BIT(20) | BIT(19)), \
			(R) + TCC_GPSB_MODE); \
} while (0)
#define tca_spi_setLSB_FIRST(R, S) do { \
	if (S) { \
		tca_spi_writel( \
			tca_spi_readl((R) + TCC_GPSB_MODE) | BIT(7), \
			(R) + TCC_GPSB_MODE); \
	} else { \
		tca_spi_writel( \
			tca_spi_readl((R) + TCC_GPSB_MODE) & ~BIT(7), \
			(R) + TCC_GPSB_MODE); \
} while (0)

#ifdef __cplusplus
extern "C" {
#endif

int tca_spi_init(struct tca_spi_handle *h,
		void __iomem *regs,
		unsigned int phy_reg_base,
		void __iomem *port_regs,
		void __iomem *pid_regs,
		int irq,
		int (*tea_dma_alloc)(struct tea_dma_buf *tdma,
					unsigned int size, struct device *dev,
					int id),
		void (*tea_dma_free)(struct tea_dma_buf *tdma,
					struct device *dev, int id),
		unsigned int dma_size,
		int id,
		int is_slave,
		struct tca_spi_port_config *port,
		const char *gpsb_name,
		struct device *dev);

void tca_spi_clean(struct tca_spi_handle *h);
int tca_spi_register_pids(struct tca_spi_handle *h, unsigned int *pids,
			unsigned int count);
int tca_spi_is_use_gdma(struct tca_spi_handle *h);
int tca_spi_is_normal_slave(struct tca_spi_handle *h);

#ifdef __cplusplus
}
#endif

#endif /* TCA_SPI_H */
