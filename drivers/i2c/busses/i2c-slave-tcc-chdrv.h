/*
 * linux/drivers/i2c/busses/i2c-slave-tcc-chdrv.h
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

#ifndef TCC_I2C_SLAVE_CHDRV
#define TCC_I2C_SLAVE_CHDRV

#include <linux/circ_buf.h>
#include <linux/poll.h>

#ifdef CONFIG_ARCH_TCC802X
#ifndef TCC_USE_GFB_PORT
#define TCC_USE_GFB_PORT
#endif
#endif

#ifdef CONFIG_TCC_DMA
#define TCC_DMA_ENGINE
#endif

#ifndef I2C_SLV_TX
#define I2C_SLV_TX 0
#endif

#ifndef I2C_SLV_RX
#define I2C_SLV_RX 1
#endif

#define i2c_slave_readb		__raw_readb
#define i2c_slave_writeb	__raw_writeb

#define i2c_slave_readl		__raw_readl
#define i2c_slave_writel	__raw_writel

/* I2C Slave Configuration Reg. */
#define I2C_DPORT			(0x00)
#define I2C_CTL				(0x04)
#define I2C_ADDR			(0x08)
#define I2C_INT				(0x0C)
#define I2C_STAT			(0x10)
#define I2C_MBF				(0x1C)
#define I2C_MB0				(0x20)
#define I2C_MB1				(0x24)

/* I2C Port Configuration Reg. */
#ifdef TCC_USE_GFB_PORT
#define I2C_PORT_CFG0		(0x00)	// Master
#define I2C_PORT_CFG1		(0x04)
#define I2C_PORT_CFG4		(0x10)	// Slave
#define I2C_PORT_CFG5		(0x14)

#define I2C_IRQ_STS			(0x1C)
#else
#define I2C_PORT_CFG0		(0x0)
#define I2C_PORT_CFG1		(0x4)
#define I2C_PORT_CFG2		(0x08)
#define I2C_IRQ_STS			(0x0C)
#endif

#define TCC_I2C_SLV_MAX_NUM		4U
#define TCC_I2C_SLV_DMA_BUF_SIZE	1024
#define TCC_I2C_SLV_WR_BUF_SIZE		1024
#define TCC_I2C_SLV_DEF_POLL_CNT	1

#define TCC_I2C_SLV_CHRDEV_NAME	"i2c-slave"
#define TCC_I2C_SLV_CHRDEV_CLASS_NAME "tcc-i2c-slave"
#define TCC_I2C_SLV_DEV_NAMES "tcc-i2c-slave%d"

#define TCC_I2C_SLV_MAX_FIFO_CNT	0x4U
#define tcc_i2c_slave_tx_fifo_cnt(i2c)\
(i2c_slave_readb((i2c)->regs + I2C_DPORT + 0x03U) & 0x7U)
#define tcc_i2c_slave_rx_fifo_cnt(i2c)\
(i2c_slave_readb((i2c)->regs + I2C_DPORT + 0x02U) & 0x7U)

#define circ_cnt(circ, size) \
		CIRC_CNT((circ)->head, (circ)->tail, size)
#define circ_space(circ, size) \
		CIRC_SPACE((circ)->head, (circ)->tail, size)
#define circ_cnt_to_end(circ, size)\
	CIRC_CNT_TO_END((circ)->head, (circ)->tail, size)
#define circ_space_to_end(circ, size) \
	CIRC_SPACE_TO_END((circ)->head, (circ)->tail, size)

struct tcc_dma_slave {
	struct device *dma_dev;
};

struct tcc_i2c_slave_dma {
	/* Virtual address */
	void			*rx_v_addr;
	void			*tx_v_addr;
	/* DMA address */
	dma_addr_t		rx_dma_addr;
	dma_addr_t		tx_dma_addr;
	/* Buffer size */
	size_t			size;

	struct dma_chan *chan_rx;
	struct dma_chan *chan_tx;

	struct scatterlist sgrx;
	struct scatterlist sgtx;

	struct dma_async_tx_descriptor  *data_desc_rx;
	struct dma_async_tx_descriptor  *data_desc_tx;

	struct tcc_dma_slave dma_slave;
	struct device *dev;
};

struct tcc_i2c_slave_buf {
	/* Virtual address */
	void			*v_addr;
	/* DMA address */
	dma_addr_t		dma_addr;
	/* Buffer size */
	size_t			size;
	/* Circular buffer info */
	struct circ_buf circ;
	spinlock_t		lock;
};

struct tcc_i2c_slave {
	unsigned int			id;         /* slave channel */
	void __iomem			*regs;      /* base address */
	void __iomem			*port_cfg;  /* port config address */

	unsigned int			phy_regs;

	struct clk				*hclk;      /* iobus clock */
	struct pinctrl          *pinctrl;   /* pin control */
	struct device			*dev;

	struct bin_attribute	bin;

	/* port configuration */
#ifdef TCC_USE_GFB_PORT
	unsigned int			port_mux[2];
#else
	unsigned int			port_mux;
#endif
	int						irq;

	uint8_t					addr;       /* slave address */

	unsigned int			is_suspended;
	spinlock_t				lock;

	unsigned int			open_cnt;

	int						buf_size;
	struct tcc_i2c_slave_buf	tx_buf;
	struct tcc_i2c_slave_buf	rx_buf;

	size_t					rw_buf_size;
	void					*write_buf;
	void					*read_buf;

	wait_queue_head_t		wait_q;
	size_t					poll_count;

	int						is_tx_dma_running;
	struct tcc_i2c_slave_dma dma;
};
struct tcc_i2c_slave i2c_slave_priv[TCC_I2C_SLV_MAX_NUM];
#endif /* TCC_I2C_SLAVE_CHDRV */
