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

#ifndef __TCC_I2C_SLAVE_CHDRV__
#define __TCC_I2C_SLAVE_CHDRV__

#include <linux/circ_buf.h>
#include <linux/sysfs.h>
#include <linux/poll.h>
#include <linux/dmaengine.h>

#ifdef CONFIG_ARCH_TCC802X
#ifndef TCC_USE_GFB_PORT
#define TCC_USE_GFB_PORT
#endif
#endif

#ifdef CONFIG_TCC_DMA
#define TCC_DMA_ENGINE
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
#define I2C_IRQ_STS			(0xC)
#endif

#define TCC_I2C_SLV_MAX_NUM			4
#define TCC_I2C_SLV_DMA_BUF_SIZE	1024
#define TCC_I2C_SLV_WR_BUF_SIZE		1024
#define TCC_I2C_SLV_DEF_POLL_CNT	1

#define TCC_I2C_SLV_CHRDEV_NAME	"i2c-slave"
#define TCC_I2C_SLV_CHRDEV_CLASS_NAME "tcc-i2c-slave"
#define TCC_I2C_SLV_DEV_NAMES "tcc-i2c-slave%d"

#define TCC_I2C_SLV_MAX_FIFO_CNT		0x4
#define tcc_i2c_slave_tx_fifo_cnt(i2c) (i2c_slave_readb((i2c)->regs + I2C_DPORT + 0x03) & 0x7)
#define tcc_i2c_slave_rx_fifo_cnt(i2c) (i2c_slave_readb((i2c)->regs + I2C_DPORT + 0x02) & 0x7)

#define circ_cnt(circ, size) 			CIRC_CNT((circ)->head, (circ)->tail, size)
#define circ_space(circ, size)			CIRC_SPACE((circ)->head, (circ)->tail, size)
#define circ_cnt_to_end(circ, size)		CIRC_CNT_TO_END((circ)->head, (circ)->tail, size)
#define circ_space_to_end(circ, size) 	CIRC_SPACE_TO_END((circ)->head, (circ)->tail, size)

struct tcc_dma_slave {
	struct device *dma_dev;
};

struct tcc_i2c_slave_dma {
	/* Virtual address */
	void 			*rx_v_addr;
	void 			*tx_v_addr;
	/* DMA address */
	dma_addr_t 		rx_dma_addr;
	dma_addr_t 		tx_dma_addr;
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
	void 			*v_addr;
	/* DMA address */
	dma_addr_t 		dma_addr;
	/* Buffer size */
	size_t			size;
	/* Circular buffer info */
	struct circ_buf circ;
	spinlock_t		lock;
};

struct tcc_i2c_slave {
	unsigned int			id;				/* slave channel */
	void __iomem			*regs;			/* base address */
	void __iomem			*port_cfg;		/* port config address */

	unsigned int			phy_regs;

	struct clk				*hclk;			/* iobus clock */
	struct device			*dev;

	struct bin_attribute	bin;

	/* port configuration */
#ifdef TCC_USE_GFB_PORT
	unsigned int			port_mux[2];
#else
	unsigned int			port_mux;
#endif
	int						irq;

	unsigned char			addr;			/* slave address */
	
	unsigned int			is_suspended;
	spinlock_t				lock;

	unsigned int			open_cnt;

	size_t					buf_size;
	struct tcc_i2c_slave_buf	tx_buf;
	struct tcc_i2c_slave_buf	rx_buf;

	size_t					rw_buf_size;
	void					*write_buf;
	void					*read_buf;

    wait_queue_head_t 		wait_q;
	size_t					poll_count;

	struct tcc_i2c_slave_dma dma;
	int						is_tx_dma_running;
};
struct tcc_i2c_slave i2c_slave_priv[TCC_I2C_SLV_MAX_NUM];

static int tcc_i2c_slave_push_one_byte(struct tcc_i2c_slave *i2c, char data, int rx)
{
	char ret;
	struct circ_buf *circ;
	ssize_t buf_size;

	spinlock_t *lock = NULL;
	unsigned long flags;

	if(rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf_size = i2c->buf_size;
	ret = 0;

	spin_lock_irqsave(lock, flags);

	if(circ_space(circ, buf_size) == 0) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "%s buffer is full\n", __func__);
		return -ENOMEM;
	}

	circ->buf[circ->head] = data;
	circ->head++;
	if(circ->head == buf_size) {
		circ->head = 0;
	}

	spin_unlock_irqrestore(lock, flags);

	return 0;	
}

static int tcc_i2c_slave_pop_one_byte(struct tcc_i2c_slave *i2c, char *data, int rx)
{
	char ret;
	struct circ_buf *circ;
	ssize_t buf_size;

	spinlock_t *lock = NULL;
	unsigned long flags;

	if(rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf_size = i2c->buf_size;
	ret = 0;

	spin_lock_irqsave(lock, flags);

	if(circ_cnt(circ, buf_size) == 0) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "%s buffer is empty\n", __func__);
		return -ENODATA;
	}

	*data = circ->buf[circ->tail];
	circ->tail++;
	if(circ->tail == buf_size) {
		circ->tail = 0;
	}

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static ssize_t tcc_i2c_slave_push(struct tcc_i2c_slave *i2c, const char *src, ssize_t sz, int rx)
{
	ssize_t ret;
	struct circ_buf *circ;
	ssize_t buf_size;
	unsigned int writeSize;

	spinlock_t *lock = NULL;
	unsigned long flags;

	if(rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf_size = i2c->buf_size;
	ret = 0;

	spin_lock_irqsave(lock, flags);

	/* check circ buf space */
	if(circ_space(circ, buf_size) < sz) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "%s buffer is full\n", __func__);
		return 0;
	}

	if(IS_ERR(src)) {
		spin_unlock_irqrestore(lock, flags);
		dev_err(i2c->dev, "%s src ptr is invalid\n", __func__);
		return 0;
	}

	if(circ->head >= circ->tail) {
		writeSize = circ_space_to_end(circ, buf_size);
		if(writeSize > sz) {
			writeSize = sz;
		}

		memcpy(&circ->buf[circ->head], src, writeSize);
		sz -= writeSize;
		ret = writeSize;
		circ->head += writeSize;
		if(circ->head >= buf_size) {
			circ->head = 0;
		}

		if(sz > 0) {
			memcpy(&circ->buf[circ->head], src + writeSize, sz);
			circ->head = sz;
			ret += sz;
		}
	} else {
		writeSize = circ->tail - circ->head;
		if(writeSize > sz) {
			writeSize = sz;
		}

		memcpy(&circ->buf[circ->head], src, writeSize);
		circ->head += writeSize;
		ret = writeSize;
	}

	//print_hex_dump(KERN_INFO, "PUSH:", DUMP_PREFIX_ADDRESS, 16, 4,circ->buf, 128, 1);

#if 0
	dev_dbg(i2c->dev,
		"%s write %d(req %d) circ buf(@0x%08x) head %d tail %d size %d space %d\n",
		__func__, ret, sz, (unsigned int)circ->buf, circ->head, circ->tail,
		buf_size, circ_space(circ, buf_size));
#endif

	spin_unlock_irqrestore(lock, flags);

	return ret;
}

static ssize_t tcc_i2c_slave_pop(struct tcc_i2c_slave *i2c, char *dst, ssize_t sz, int rx)
{
	ssize_t ret;
	struct circ_buf *circ;
	ssize_t buf_size;
	unsigned int readSize;

	spinlock_t *lock = NULL;
	unsigned long flags;

	if(rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf_size = i2c->buf_size;
	ret = 0;

	spin_lock_irqsave(lock, flags);

	if(circ_cnt(circ, buf_size) == 0) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "%s buffer is empty\n", __func__);
		return 0;
	}

	if(IS_ERR(dst)) {
		spin_unlock_irqrestore(lock, flags);
		dev_err(i2c->dev, "%s dst ptr is invalid\n", __func__);
		return 0;
	}

	if(circ->head > circ->tail) {
		readSize = circ->head - circ->tail;
		if(readSize > sz) {
			readSize = sz;
		}

		memcpy(dst, &circ->buf[circ->tail], readSize);
		circ->tail += readSize;
		ret = readSize;
	} else {
		readSize = buf_size - circ->tail;
		if(readSize > sz) {
			readSize = sz;
		}

		memcpy(dst, &circ->buf[circ->tail], readSize);
		dst += readSize;
		sz -= readSize;
		ret = readSize;
		circ->tail += readSize;
		if(circ->tail >= sz) {
			circ->tail = 0;
		}

		if(sz >= 0) {
			memcpy(dst, &circ->buf[circ->tail], sz);
			circ->tail = sz;
			ret += sz;
		}
	}

#if 0
	dev_dbg(i2c->dev,
		"%s read %d(req %d) circ buf(@0x%08x) head %d tail %d size %d count %d\n",
		__func__, ret, sz, (unsigned int)circ->buf, circ->head, circ->tail,
		buf_size, circ_cnt(circ, buf_size));
#endif

	spin_unlock_irqrestore(lock, flags);

	return ret;
}

#define TCC_I2C_SLV_DMA_BSIZE	1
#define TCC_I2C_SLV_DMA_SG_LEN	1

/* Terminate all dma-engine channel */
static void tcc_i2c_slave_stop_dma_engine(struct tcc_i2c_slave *i2c, int rx)
{
	if(rx != 0) {
		if(i2c->dma.chan_rx)
			dmaengine_terminate_all(i2c->dma.chan_rx);
	} else {
		if(i2c->dma.chan_tx)
			dmaengine_terminate_all(i2c->dma.chan_tx);
	}
}

/* Submit dma-engine descriptor */
static int tcc_i2c_slave_dma_dma_engine_submit(struct tcc_i2c_slave *i2c,
						dma_async_tx_callback callback, void *callback_param, unsigned int len, int rx)
{
	struct tcc_i2c_slave_dma *dma;
	struct dma_chan *chan;
	struct dma_async_tx_descriptor *desc;
	struct dma_slave_config slave_config;
	struct scatterlist *sg;
	enum dma_transfer_direction dma_dir;
	dma_addr_t dma_addr;
	dma_cookie_t cookie;
	unsigned int dma_length;

	dma = &i2c->dma;
	if(rx != 0) {
		chan = dma->chan_rx;
		sg = &dma->sgrx;
		dma_addr = dma->rx_dma_addr;
		dma_length = len;
		dma_dir = DMA_DEV_TO_MEM;
		slave_config.src_addr = (dma_addr_t)(i2c->phy_regs + I2C_DPORT);
	} else {
		chan = dma->chan_tx;
		sg = &dma->sgtx;
		dma_addr = dma->tx_dma_addr;
		dma_length = len;
		dma_dir = DMA_MEM_TO_DEV;
		slave_config.dst_addr = (dma_addr_t)(i2c->phy_regs + I2C_DPORT);
	}

	if(chan == NULL) {
		dev_err(i2c->dev,"%s chan(%p) is invalid pointer\n",
			((rx != 0) ? "rx" : "tx"), chan);
		return -ENODEV;
	}

	/* Prepare the dma transfer */
	sg_init_table(sg, TCC_I2C_SLV_DMA_SG_LEN);
	sg_dma_len(sg) = dma_length;
	sg_dma_address(sg) = dma_addr;

	/* Set dma channel */
	slave_config.direction = dma_dir;
	slave_config.dst_maxburst = TCC_I2C_SLV_DMA_BSIZE;
	slave_config.src_maxburst = TCC_I2C_SLV_DMA_BSIZE;
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	if(dmaengine_slave_config(chan, &slave_config))
	{
		dev_err(i2c->dev, "Failed to configrue %s dma channel\n",
			((rx != 0) ? "rx" : "tx"));
		return -EINVAL;
	}

	desc = dmaengine_prep_slave_sg(chan, sg, TCC_I2C_SLV_DMA_SG_LEN,
		dma_dir,
		DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if(!desc)
	{
		dev_err(i2c->dev, "Failed preparing %s dma desc\n",
			((rx != 0) ? "rx" : "tx"));
		tcc_i2c_slave_stop_dma_engine(i2c, rx);
		return -ENOMEM;
	}

	desc->callback = callback;
	desc->callback_param = callback_param;

	cookie = dmaengine_submit(desc);
	if(dma_submit_error(cookie))
	{
		dev_err(i2c->dev, "%s dma desc submitting error! (cookie: %X)\n",
			((rx != 0) ? "rx" : "tx"), (unsigned int)cookie);
		tcc_i2c_slave_stop_dma_engine(i2c, rx);
		return -ENOMEM;
	}

	dma_async_issue_pending(chan);

	dev_dbg(i2c->dev, "%s (%s) dma_addr 0x%08x size %d cb %p cb_param %p\n",
		__func__, ((rx != 0) ? "rx" : "tx"),
		dma_addr, dma_length, desc->callback, desc->callback_param);

	return 0;
}

/* dma-engine tx channel callback */
static void tcc_i2c_slave_tx_dma_callback(void *data)
{
	struct tcc_i2c_slave *i2c = (struct tcc_i2c_slave *)data;
	int ret, size;

	if(i2c == NULL) {
		printk(KERN_ERR "%s i2c is invalid pointer\n", __func__);
		return;
	}

	size = tcc_i2c_slave_pop(i2c, i2c->dma.tx_v_addr, i2c->dma.size, 0);
	if(size != 0) {
		ret = tcc_i2c_slave_dma_dma_engine_submit(i2c, tcc_i2c_slave_tx_dma_callback, i2c,
			size, 0);
		if(ret != 0) {
			dev_err(i2c->dev, "failed to submit tx dma-engine descriptor\n");
		} else {
			dev_dbg(i2c->dev, "queue is not empty, restart dma (sz %d)\n",
				size);
		}
	} else {
		spin_lock(&i2c->lock);
		i2c_slave_writel(
			(i2c_slave_readl(i2c->regs + I2C_CTL) & ~(BIT(17))),
			i2c->regs+I2C_CTL
		);
		i2c->is_tx_dma_running = 0;
		tcc_i2c_slave_stop_dma_engine(i2c, 0);
		dev_dbg(i2c->dev, "queue is empty, stop dma\n");
		spin_unlock(&i2c->lock);
	}
}

/* dma-engine rx channel callback */
static void tcc_i2c_slave_rx_dma_callback(void *data)
{
	struct tcc_i2c_slave *i2c = (struct tcc_i2c_slave *)data;

	if(i2c == NULL) {
		printk(KERN_ERR "%s i2c is invalid pointer\n", __func__);
		return;
	}

	tcc_i2c_slave_push(i2c, i2c->dma.rx_v_addr, i2c->poll_count, 1);
	wake_up(&i2c->wait_q);

	if(i2c->dma.chan_rx != 0) {
		/* Re-enable Rx DMA */
		tcc_i2c_slave_dma_dma_engine_submit(i2c, tcc_i2c_slave_rx_dma_callback, i2c,
			i2c->poll_count, 1);
	}

	dev_dbg(i2c->dev, "%s (v_addr: 0x%08x, poll_count: %d)\n",
		__func__, (unsigned int)i2c->dma.rx_v_addr, i2c->poll_count);
}

/* dma-engine filter */
static bool tcc_i2c_slave_dma_engine_filter(struct dma_chan *chan, void *pdata)
{
	struct tcc_i2c_slave_dma *dma = pdata;
	struct tcc_dma_slave *dma_slave;

	if(!dma)
	{
		printk("[%s] tcc_spi_gdma is NULL!!\n", __func__);
		return -EINVAL;
	}

	dma_slave = &dma->dma_slave;
	if(dma_slave->dma_dev == chan->device->dev) {
		chan->private = dma_slave;
		return 0;
	}
	else {
		dev_err(chan->device->dev,"dma_dev(%p) != dev(%p)\n",
			dma_slave->dma_dev, chan->device->dev);
		return -ENODEV;
	}
}

/* Release dma-eninge channel */
static void tcc_i2c_slave_release_dma_engine(struct tcc_i2c_slave *i2c)
{
	if(i2c->dma.chan_tx)
	{
		dma_release_channel(i2c->dma.chan_tx);
		i2c->dma.chan_tx = NULL;
	}
	if(i2c->dma.chan_rx)
	{
		dma_release_channel(i2c->dma.chan_rx);
		i2c->dma.chan_rx = NULL;
	}

	if(i2c->dma.tx_v_addr != NULL) {
		dma_free_writecombine(i2c->dev, i2c->buf_size, i2c->dma.tx_v_addr, i2c->dma.tx_dma_addr);
		i2c->dma.tx_v_addr = NULL;
		i2c->dma.tx_dma_addr = 0;
	}
	if(i2c->dma.rx_v_addr != NULL) {
		dma_free_writecombine(i2c->dev, i2c->buf_size, i2c->dma.rx_v_addr, i2c->dma.rx_dma_addr);
		i2c->dma.rx_v_addr = NULL;
		i2c->dma.rx_dma_addr = 0;
	}
}

/* Probe dma-engine channels */
static int tcc_i2c_slave_dma_engine_probe(struct platform_device *pdev, struct tcc_i2c_slave *i2c)
{
	struct tcc_i2c_slave_dma *dma;
	struct device *dev = &pdev->dev;
	dma_cap_mask_t mask;
	int ret, count, i;
	const char *str;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	dma = &i2c->dma;
	memset(dma, 0, sizeof(struct tcc_i2c_slave_dma));

#ifndef TCC_DMA_ENGINE
	dev_dbg(i2c->dev, "DMA-engine is not enabled\n");
	return -ENOSYS;
#endif

	dma->size = i2c->buf_size;

	count = of_property_count_strings(dev->of_node, "dma-names");
	if (count < 0) {
		dev_dbg(i2c->dev, "dma-names property of node missing or empty\n");
		return count;
	}

	for(i = 0; i < count; i++) {
		ret = of_property_read_string_index(dev->of_node, "dma-names", i, &str);
		if (ret)
			goto error;

		if (!strncmp(str, "tx", 2)) {
			dma->chan_tx = dma_request_slave_channel_compat(mask, tcc_i2c_slave_dma_engine_filter, dma, dev, "tx");
			//dma->chan_tx = dma_request_slave_channel(dev, "tx");
			if(IS_ERR(dma->chan_tx))
			{
				dev_err(dev, "DMA TX channel request Error!(%p)\n",dma->chan_tx);
				ret = -EBUSY;
				goto error;
			} else {
				dma->tx_v_addr = dma_alloc_writecombine(dev, dma->size, &dma->tx_dma_addr, GFP_KERNEL);
				if(!dma->tx_v_addr) {
					dev_err(i2c->dev, "Fail to allocate the dma buffer (tx)\n");
					ret = -ENOMEM;
					goto error;
				}
			}
		}
		else if (!strncmp(str, "rx", 2)) {
			dma->chan_rx = dma_request_slave_channel_compat(mask, tcc_i2c_slave_dma_engine_filter, dma, dev, "rx");
			//dma->chan_rx = dma_request_slave_channel(dev, "rx");
			if(IS_ERR(dma->chan_rx))
			{
				dev_err(dev, "DMA RX channel request Error!(%p)\n",dma->chan_rx);
				ret = -EBUSY;
				goto error;
			} else {
				dma->rx_v_addr = dma_alloc_writecombine(dev, dma->size, &dma->rx_dma_addr, GFP_KERNEL);
				if(!dma->rx_v_addr) {
					dev_err(i2c->dev, "Fail to allocate the dma buffer (rx)\n");
					ret = -ENOMEM;
					goto error;
				}
			}
		}
		else {
			dev_err(dev, "Wrong dmatype %s\n", str);
			goto error;
		}
	}

	return 0;

error:
	tcc_i2c_slave_release_dma_engine(i2c);
	return ret;
}
 #endif /*__TCC_I2C_SLAVE_CHDRV__ */
