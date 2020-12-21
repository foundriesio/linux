// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/device.h>
#include <linux/stat.h>

#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <linux/i2c/tcc_i2c_slave.h>

#include <asm/dma.h>

#include "i2c-slave-tcc-chdrv.h"

static struct class *tcc_i2c_slave_class;
static uint32_t tcc_i2c_slave_major;	/* mojor number of device */

static ssize_t tcc_i2c_slave_fifo_byte_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf);
static ssize_t tcc_i2c_slave_fifo_byte_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count);
DEVICE_ATTR(fifo_byte, 0664, tcc_i2c_slave_fifo_byte_show,
	    tcc_i2c_slave_fifo_byte_store);

static int32_t tcc_i2c_slave_push_one_byte(struct tcc_i2c_slave *i2c, uint8_t data,
				       int32_t rx)
{
	struct circ_buf *circ;
	int32_t buf_size;
	char buf = (char)data;
	spinlock_t *lock;
	ulong flags;

	if (rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf_size = i2c->buf_size;

	spin_lock_irqsave(lock, flags);

	if (circ_space(circ, buf_size) == 0) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "[DEBUG][I2C] %s: buffer is full\n",
			__func__);
		return -ENOMEM;
	}

	circ->buf[circ->head] = buf;
	circ->head++;
	if (circ->head == buf_size) {
		circ->head = 0;
	}

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int32_t tcc_i2c_slave_pop_one_byte(struct tcc_i2c_slave *i2c, uint8_t *data,
				      int32_t rx)
{
	struct circ_buf *circ;
	int32_t buf_size;
	uint8_t *buf;
	spinlock_t *lock;
	ulong flags;

	if (rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf_size = i2c->buf_size;
	buf = data;
	spin_lock_irqsave(lock, flags);

	if (circ_cnt(circ, buf_size) == 0) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "[DEBUG][I2C]%s buffer is empty\n", __func__);
		return -ENODATA;
	}

	*buf = circ->buf[circ->tail];
	circ->tail++;
	if (circ->tail == buf_size) {
		circ->tail = 0;
	}

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int32_t tcc_i2c_slave_push(struct tcc_i2c_slave *i2c, const char *src,
				 int32_t sz, int32_t rx)
{
	struct circ_buf *circ;
	int32_t buf_size, writeSize, size;
	int32_t ret;
	spinlock_t *lock;
	ulong flags;

	if (rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	size = sz;
	buf_size = i2c->buf_size;
	ret = 0;

	spin_lock_irqsave(lock, flags);

	/* check circ buf space */
	if (circ_space(circ, buf_size) < size) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "[DEBUG][I2C]%s buffer is full\n", __func__);
		return 0;
	}

	if (IS_ERR(src)) {
		spin_unlock_irqrestore(lock, flags);
		dev_err(i2c->dev, "[ERROR][I2C]%s src ptr is invalid\n",
			__func__);
		return 0;
	}

	if (circ->head >= circ->tail) {
		writeSize = circ_space_to_end(circ, buf_size);
		if (writeSize > size) {
			writeSize = size;
		}

		(void)memcpy(&circ->buf[circ->head], src, (size_t)writeSize);
		size -= writeSize;
		ret = writeSize;
		circ->head += writeSize;
		if (circ->head >= buf_size) {
			circ->head = 0;
		}

		if (size > 0) {
			(void)memcpy(&circ->buf[circ->head], src + writeSize, (size_t)size);
			circ->head = size;
			ret += size;
		}
	} else {
		writeSize = circ->tail - circ->head;
		if (writeSize > size) {
			writeSize = size;
		}

		(void)memcpy(&circ->buf[circ->head], src, (size_t)writeSize);
		circ->head += writeSize;
		ret = writeSize;
	}

	dev_dbg(i2c->dev,
		"[DEBUG][I2C] %s: write %d(req %d) circ buf(0x%08lx) head %d tail %d size %d space %d\n",
		__func__,
		ret,
		size,
		(ulong)circ->buf,
		circ->head, circ->tail, buf_size, circ_space(circ, buf_size));

	spin_unlock_irqrestore(lock, flags);

	return ret;
}

static int32_t tcc_i2c_slave_pop(struct tcc_i2c_slave *i2c, char *dst, int32_t sz,
				int32_t rx)
{
	struct circ_buf *circ;
	char *buf;
	int32_t buf_size, readSize, size;
	int32_t ret;
	spinlock_t *lock;
	ulong flags;

	if (rx != 0) {
		circ = &i2c->rx_buf.circ;
		lock = &i2c->rx_buf.lock;
	} else {
		circ = &i2c->tx_buf.circ;
		lock = &i2c->tx_buf.lock;
	}
	buf = dst;
	size = sz;
	buf_size = i2c->buf_size;
	ret = 0;

	spin_lock_irqsave(lock, flags);

	if (circ_cnt(circ, buf_size) == 0) {
		spin_unlock_irqrestore(lock, flags);
		dev_dbg(i2c->dev, "[DEBUG][I2C]%s buffer is empty\n", __func__);
		return 0;
	}

	if (IS_ERR(buf)) {
		spin_unlock_irqrestore(lock, flags);
		dev_err(i2c->dev, "[ERROR][I2C]%s dst ptr is invalid\n",
			__func__);
		return 0;
	}

	if (circ->head > circ->tail) {
		readSize = circ->head - circ->tail;
		if (readSize > size) {
			readSize = size;
		}

		(void)memcpy(buf, &circ->buf[circ->tail], (size_t)readSize);
		circ->tail += readSize;
		ret = readSize;
	} else {
		readSize = buf_size - circ->tail;
		if (readSize > size) {
			readSize = size;
		}

		(void)memcpy(buf, &circ->buf[circ->tail], (size_t)readSize);
		buf += readSize;
		size -= readSize;
		ret = readSize;
		circ->tail += readSize;
		if (circ->tail >= size) {
			circ->tail = 0;
		}

		(void)memcpy(buf, &circ->buf[circ->tail], (size_t)size);
		circ->tail = (int32_t)size;
		ret += size;
	}

	dev_dbg(i2c->dev,
		"[DEBUG][I2C] %s: read %d(req %d) circ buf(0x%08lx) head %d tail %d size %d count %d\n",
		__func__,
		ret,
		size,
		(ulong)circ->buf,
		circ->head, circ->tail, buf_size, circ_cnt(circ, buf_size));

	spin_unlock_irqrestore(lock, flags);

	return ret;
}

#define TCC_I2C_SLV_DMA_BSIZE	1
#define TCC_I2C_SLV_DMA_SG_LEN	1

/* Terminate all dma-engine channel */
static void tcc_i2c_slave_stop_dma_engine(struct tcc_i2c_slave *i2c, bool is_rx)
{
	if (is_rx && (i2c->dma.chan_rx != NULL)) {
		dmaengine_terminate_all(i2c->dma.chan_rx);
	}
	if ((!is_rx) && (i2c->dma.chan_tx != NULL)) {
		dmaengine_terminate_all(i2c->dma.chan_tx);
	}
}

/* Submit dma-engine descriptor */
static int32_t tcc_i2c_slave_dma_dma_engine_submit(struct tcc_i2c_slave *i2c,
					       dma_async_tx_callback callback,
					       void *callback_param,
					       ssize_t len, bool is_rx)
{
	struct tcc_i2c_slave_dma *dma;
	struct dma_chan *chan;
	struct dma_async_tx_descriptor *desc;
	struct dma_slave_config slave_config;
	struct scatterlist *sg;
	enum dma_transfer_direction dma_dir;
	dma_addr_t dma_addr;
	dma_cookie_t cookie;
	uint32_t dma_length;

	dma = &i2c->dma;
	if (is_rx) {
		chan = dma->chan_rx;
		sg = &dma->sgrx;
		dma_addr = dma->rx_dma_addr;
		dma_length = (uint32_t)len;
		dma_dir = DMA_DEV_TO_MEM;
		slave_config.src_addr =
		    (dma_addr_t) (i2c->phy_regs);
	} else {
		chan = dma->chan_tx;
		sg = &dma->sgtx;
		dma_addr = dma->tx_dma_addr;
		dma_length = (uint32_t)len;
		dma_dir = DMA_MEM_TO_DEV;
		slave_config.dst_addr =
		    (dma_addr_t) (i2c->phy_regs);
	}

	if (chan == NULL) {
		dev_err(i2c->dev,
			"[ERROR][I2C]%s chan(%p) is invalid pointer\n",
			(is_rx ? "rx" : "tx"), chan);
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
	if (dmaengine_slave_config(chan, &slave_config) < 0) {
		dev_err(i2c->dev,
			"[ERROR][I2C] Failed to configrue %s dma channel\n",
			(is_rx ? "rx" : "tx"));
		return -EINVAL;
	}

	desc = dmaengine_prep_slave_sg(chan, sg, TCC_I2C_SLV_DMA_SG_LEN,
				       dma_dir,
				       DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (desc == NULL) {
		dev_err(i2c->dev, "[ERROR][I2C] Failed preparing %s dma desc\n",
			(is_rx ? "rx" : "tx"));
		tcc_i2c_slave_stop_dma_engine(i2c, is_rx);
		return -ENOMEM;
	}

	desc->callback = callback;
	desc->callback_param = callback_param;

	cookie = dmaengine_submit(desc);
	if (dma_submit_error(cookie) != 0) {
		dev_err(i2c->dev,
			"%s dma desc submitting error! (cookie: %X)\n",
			(is_rx ? "rx" : "tx"), (uint32_t)cookie);
		tcc_i2c_slave_stop_dma_engine(i2c, is_rx);
		return -ENOMEM;
	}

	dma_async_issue_pending(chan);

	dev_dbg(i2c->dev,
			"[DEBUG][I2C]%s (%s) dma_addr 0x%x size %d cb %p cb_param %p\n",
			__func__,
			(is_rx ? "rx" : "tx"),
			(uint32_t)dma_addr,
			dma_length,
			desc->callback,
			desc->callback_param);
	return 0;
}

/* dma-engine tx channel callback */
static void tcc_i2c_slave_tx_dma_callback(void *data)
{
	struct tcc_i2c_slave *i2c = (struct tcc_i2c_slave *)data;
	int32_t ret;
	ssize_t size;
	void *pv_i2c;

	if (i2c == NULL) {
		pr_err("[ERROR][I2C] %s i2c is invalid pointer\n", __func__);
		return;
	}
	pv_i2c = i2c;
	size = (ssize_t)tcc_i2c_slave_pop(i2c,
			i2c->dma.tx_v_addr,
			i2c->dma.size,
			I2C_SLV_TX);
	if (size != 0) {
		ret = tcc_i2c_slave_dma_dma_engine_submit(i2c,
				tcc_i2c_slave_tx_dma_callback,
				pv_i2c,
				size,
				I2C_SLV_TX);
		if (ret != 0) {
			dev_err(i2c->dev,
					"[ERROR][I2C] failed to submit tx dma-engine descriptor\n");
		} else {
			dev_dbg(i2c->dev,
				"[DEBUG][I2C]queue is not empty, restart dma (size %ld)\n",
				size);
		}
	} else {
		spin_lock(&i2c->lock);
		i2c_slave_writel((i2c_slave_readl(i2c->regs + I2C_CTL) &
				  ~(BIT(17))), i2c->regs + I2C_CTL);
		i2c->is_tx_dma_running = 0;
		tcc_i2c_slave_stop_dma_engine(i2c, I2C_SLV_TX);
		dev_dbg(i2c->dev, "[DEBUG][I2C]queue is empty, stop dma\n");
		spin_unlock(&i2c->lock);
	}
}

/* dma-engine rx channel callback */
static void tcc_i2c_slave_rx_dma_callback(void *data)
{
	struct tcc_i2c_slave *i2c = (struct tcc_i2c_slave *)data;
	void *pv_i2c;
	int32_t ret = 0;

	if (i2c == NULL) {
		pr_err("[ERROR][I2C] %s i2c is invalid pointer\n", __func__);
		return;
	}
	pv_i2c = i2c;
	(void)tcc_i2c_slave_push(i2c, i2c->dma.rx_v_addr, i2c->poll_count,
			   I2C_SLV_RX);
	wake_up(&i2c->wait_q);

	if (i2c->dma.chan_rx != NULL) {
		/* Re-enable Rx DMA */
		ret = tcc_i2c_slave_dma_dma_engine_submit(i2c,
				tcc_i2c_slave_rx_dma_callback,
				pv_i2c,
				(ssize_t) i2c->poll_count,
				I2C_SLV_RX);
	}

	if (ret != 0) {
		dev_warn(i2c->dev,
				"[WARN][I2C] tcc_i2c_slave_dma_dma_engine_submit return %d\n",
				ret);
	}
	dev_dbg(i2c->dev, "[DEBUG][I2C]%s (v_addr: 0x%lx, poll_count: %d)\n",
		__func__, (ulong)i2c->dma.rx_v_addr, i2c->poll_count);
}

/* dma-engine filter */
static bool tcc_i2c_slave_dma_engine_filter(struct dma_chan *chan, void *pdata)
{
	struct tcc_i2c_slave_dma *dma = pdata;
	struct tcc_dma_slave *dma_slave;

	if (dma == NULL) {
		pr_err("[ERROR][I2C][%s] tcc_i2c_gdma is NULL!!\n", __func__);
		return (bool) false;
	}

	dma_slave = &dma->dma_slave;
	if (dma_slave->dma_dev == chan->device->dev) {
		chan->private = dma_slave;
		return (bool) true;
	} else {
		dev_err(chan->device->dev, "dma_dev(%p) != dev(%p)\n",
			dma_slave->dma_dev, chan->device->dev);
		return (bool) false;
	}
}

/* Release dma-eninge channel */
static void tcc_i2c_slave_release_dma_engine(struct tcc_i2c_slave *i2c)
{
	if (i2c->dma.chan_tx != NULL) {
		dma_release_channel(i2c->dma.chan_tx);
		i2c->dma.chan_tx = NULL;
	}
	if (i2c->dma.chan_rx != NULL) {
		dma_release_channel(i2c->dma.chan_rx);
		i2c->dma.chan_rx = NULL;
	}

	if (i2c->dma.tx_v_addr != NULL) {
		dma_free_writecombine(i2c->dev, (size_t)i2c->buf_size,
				      i2c->dma.tx_v_addr, i2c->dma.tx_dma_addr);
		i2c->dma.tx_v_addr = NULL;
		i2c->dma.tx_dma_addr = 0;
	}
	if (i2c->dma.rx_v_addr != NULL) {
		dma_free_writecombine(i2c->dev, (size_t)i2c->buf_size,
				      i2c->dma.rx_v_addr, i2c->dma.rx_dma_addr);
		i2c->dma.rx_v_addr = NULL;
		i2c->dma.rx_dma_addr = 0;
	}
}

/* Probe dma-engine channels */
static int32_t tcc_i2c_slave_dma_engine_probe(struct platform_device *pdev,
					  struct tcc_i2c_slave *i2c)
{
	struct tcc_i2c_slave_dma *dma;
	struct device *dev = &pdev->dev;
	dma_cap_mask_t mask;
	int32_t ret, count, i;
	const char *str;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	dma = &i2c->dma;
	(void)memset(dma, 0, sizeof(struct tcc_i2c_slave_dma));

#ifndef TCC_DMA_ENGINE
	dev_dbg(i2c->dev, "[DEBUG][I2C]DMA-engine is not enabled\n");
	return -ENXIO;
#endif

	dma->size = i2c->buf_size;

	count = of_property_count_strings(dev->of_node, "dma-names");
	if (count < 0) {
		dev_dbg(i2c->dev,
			"[DEBUG][I2C]dma-names property of node missing or empty\n");
		return count;
	}

	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(
				dev->of_node,
				"dma-names",
				i, &str);
		if (ret < 0) {
			goto error;
		}
		if (strncmp(str, "tx", 2) == 0) {
			dma->chan_tx =
				dma_request_slave_channel_compat(mask,
						tcc_i2c_slave_dma_engine_filter,
						dma, dev, "tx");
			if (IS_ERR(dma->chan_tx)) {
				dev_err(dev,
					"DMA TX channel request Error!(%p)\n",
					dma->chan_tx);
				ret = -EBUSY;
				goto error;
			} else {
				dma->tx_v_addr =
				    dma_alloc_writecombine(dev, (size_t)dma->size,
							   &dma->tx_dma_addr,
							   GFP_KERNEL);
				if (dma->tx_v_addr == NULL) {
					dev_err(i2c->dev,
						"[ERROR][I2C] Fail to allocate the dma buffer (tx)\n");
					ret = -ENOMEM;
					goto error;
				}
			}
		} else if (strncmp(str, "rx", 2) == 0) {
			dma->chan_rx =
				dma_request_slave_channel_compat(mask,
						tcc_i2c_slave_dma_engine_filter,
						dma, dev, "rx");
			if (IS_ERR(dma->chan_rx)) {
				dev_err(dev,
					"DMA RX channel request Error!(%p)\n",
					dma->chan_rx);
				ret = -EBUSY;
				goto error;
			} else {
				dma->rx_v_addr =
				    dma_alloc_writecombine(dev, (size_t)dma->size,
							   &dma->rx_dma_addr,
							   GFP_KERNEL);
				if (dma->rx_v_addr == NULL) {
					dev_err(i2c->dev,
						"[ERROR][I2C] Fail to allocate the dma buffer (rx)\n");
					ret = -ENOMEM;
					goto error;
				}
			}
		} else {
			dev_err(dev, "Wrong dmatype %s\n", str);
			goto error;
		}
	}

	return 0;

error:
	tcc_i2c_slave_release_dma_engine(i2c);
	return ret;
}

static int32_t tcc_i2c_slave_init_buf(struct tcc_i2c_slave *i2c, int32_t dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;
	struct tcc_i2c_slave_buf *buf;

	dev = i2c->dev;

	v_addr = dma_alloc_writecombine(dev,
			(size_t)i2c->buf_size,
			&dma_addr,
			GFP_KERNEL);
	if (v_addr == NULL) {
		dev_err(i2c->dev,
			"[ERROR][I2C] Fail to allocate the dma buffer (%s)\n",
			(dma_to_mem != 0) ? "rx" : "tx");
		return -ENOMEM;
	}

	if (dma_to_mem != 0) {
		buf = &i2c->rx_buf;
	} else {
		buf = &i2c->tx_buf;
	}

	buf->v_addr = v_addr;
	buf->dma_addr = dma_addr;
	buf->size = i2c->buf_size;

	buf->circ.buf = v_addr;
	buf->circ.head = 0;
	buf->circ.tail = buf->circ.head;
	spin_lock_init(&buf->lock);

	(void)memset(buf->v_addr, 0, (size_t)buf->size);

	dev_dbg(i2c->dev,
		"[DEBUG][I2C] [%s] dma_to_mem: %d v_addr: 0x%lX dma_addr: 0x%08X size: %d\n",
		__func__, dma_to_mem, (ulong)v_addr,
		(uint32_t)dma_addr, i2c->buf_size);
	return 0;
}

static void tcc_i2c_slave_deinit_buf(struct tcc_i2c_slave *i2c, int32_t dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct tcc_i2c_slave_buf *buf;

	if (dma_to_mem != 0) {
		buf = &i2c->rx_buf;
	} else {
		buf = &i2c->tx_buf;
	}

	v_addr = buf->v_addr;
	dma_addr = buf->dma_addr;
	buf->v_addr = (void *)NULL;
	buf->dma_addr = 0;
	buf->circ.buf = (char *)NULL;

	dma_free_writecombine(i2c->dev, (size_t) i2c->buf_size, v_addr,
			      dma_addr);

	dev_dbg(i2c->dev,
		"[DEBUG][I2C] [%s] dma_to_mem: %d v_addr: 0x%08lX dma_addr: 0x%08X size: %d\n",
		__func__, dma_to_mem, (ulong)v_addr,
		(uint32_t)dma_addr, i2c->buf_size);
}

static irqreturn_t tcc_i2c_slave_isr(int32_t irq, void *arg)
{
	struct tcc_i2c_slave *i2c = (struct tcc_i2c_slave *)arg;
	uint32_t status, intr_mask;
	uint32_t mbfr, mbft, mbf;
	uint32_t data, i, stat;
	uint32_t reg_shift;
	uint8_t val, cnt;
	int32_t circ_q_cnt;
	int32_t ret;

	status = i2c_slave_readl(i2c->regs + I2C_INT);
	intr_mask = status & 0xFFFU;
	status = status & (intr_mask << 16);
	mbf = i2c_slave_readl(i2c->regs + I2C_MBF);
	dev_dbg(i2c->dev,
		"[DEBUG][I2C] slave status 0x%08x intr_mask 0x%08x mbf 0x%08x\n",
		status, intr_mask, mbf);

	/* Data Buffer has been read by a master */
	if ((status & BIT(25)) != 0U) {
		mbft = (mbf >> 16U) & 0xFFU;
		for (i = 0U; i < 8U; i++) {
			if ((mbft & BIT(i)) != 0U) {
				/* Notify read request and read data */
				if (i < 4U) {
					reg_shift = i << 3U;
					data =
						readl(i2c->regs+I2C_MB0);
					data = ((data >> reg_shift) & 0xFFU);
				} else {
					reg_shift = (i - 4U) << 3U;
					data =
						readl(i2c->regs+I2C_MB1);
					data =
						((data >> reg_shift) & 0xFFU);
				}
				val = (uint8_t)data;
				dev_dbg(i2c->dev,
					"[DEBUG][I2C] Data buffer has been read by master (s_addr 0x%02x val 0x%02x)\n",
					i, data);
			}
		}
	}

	/* Data Buffer has been written by a master */
	if ((status & BIT(24)) != 0U) {
		mbfr = mbf & 0xFFU;
		for (i = 0U; i < 8U; i++) {
			if ((mbfr & BIT(i)) != (u32) 0U) {
				/* Notify write request and written data */
				if (i < 4U) {
					reg_shift = i << 3U;
					data = i2c_slave_readl(
							i2c->regs + I2C_MB0);
					data = ((data >> reg_shift) & 0xFFU);
				} else {
					reg_shift = (i - 4U) << 3U;
					data =
						i2c_slave_readl(
i2c->regs + I2C_MB1);
					data =
						((data >> reg_shift) & 0xFFU);
				}
				val = (uint8_t)data;
				dev_dbg(i2c->dev,
					"[DEBUG][I2C] Data buffer has been written by master (s_addr 0x%02x val 0x%02x)\n",
					i, data);
			}
		}
	}

	if (mbf != 0U) {
		i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_MB0),
				 i2c->regs + I2C_MB0);
		i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_MB1),
				 i2c->regs + I2C_MB1);
	}

	/* RX FIFO Level (RXCV <= RXTH) */
	if ((status & BIT(16)) != 0U) {
		while (1) {
			val = i2c_slave_readb(i2c->regs + I2C_DPORT);

			ret = tcc_i2c_slave_push_one_byte(i2c, val, I2C_SLV_RX);
			if (ret != 0) {
				dev_err(i2c->dev,
					"[ERROR][I2C] Rx buffer is full\n");
			}

			stat = i2c_slave_readl(i2c->regs + I2C_INT);
			if ((stat & BIT(16)) == 0U) {
				break;
			}
		}
		dev_dbg(i2c->dev,
			"[DEBUG][I2C] Rx FIFO level interrupt, status 0x%08x\n",
			status);

		circ_q_cnt = circ_cnt(&(i2c->rx_buf.circ), i2c->buf_size);
		if (circ_q_cnt >= i2c->poll_count) {
			wake_up(&i2c->wait_q);
			dev_dbg(i2c->dev,
				"[DEBUG][I2C] Rx count(%d) is over poll count(%d) - wakeup\n",
				circ_q_cnt, i2c->poll_count);
		}
	}

	/* TX FIFO Level (TXCV <= TXTH) */
	if ((status & BIT(17)) != 0U) {
		dev_dbg(i2c->dev,
			"[DEBUG][I2C] Tx FIFO level interrupt, status 0x%08x\n",
			status);
		cnt = tcc_i2c_slave_tx_fifo_cnt(i2c);
		while (1) {
			ret = tcc_i2c_slave_pop_one_byte(i2c, &val, I2C_SLV_TX);
			if (ret != 0) {
				/* disable interrupt */
				i2c_slave_writel(i2c_slave_readl
						 (i2c->regs +
						  I2C_INT) & ~(BIT(1)),
						 i2c->regs + I2C_INT);
				break;
			}
			i2c_slave_writeb(val, i2c->regs + I2C_DPORT);
			cnt++;
			if (cnt >= TCC_I2C_SLV_MAX_FIFO_CNT) {
				break;
			}
		}
	}

	return IRQ_HANDLED;
}

/* Set i2c port configuration */
#ifdef TCC_USE_GFB_PORT
static int32_t tcc_i2c_slave_set_port(struct tcc_i2c_slave *i2c)
{
	u32 port_value;
	u32 pcfg_value, pcfg_offset, pcfg_shift;

	if (i2c->id >= 4) {
		return -EINVAL;
	}

	if (i2c->id < 4)
		pcfg_offset = I2C_PORT_CFG4;
	else
		pcfg_offset = I2C_PORT_CFG5;

	pcfg_shift = (i2c->id % 2) * 16;
	port_value = (i2c->port_mux[0] |
			(i2c->port_mux[1] << 8));	/* [0]SCL [1]SDA */

	pcfg_value = i2c_slave_readl(i2c->port_cfg + pcfg_offset);
	pcfg_value &= ~(0xFFFF << pcfg_shift);
	pcfg_value |= (0xFFFF & port_value) << pcfg_shift;
	i2c_slave_writel(pcfg_value, i2c->port_cfg + pcfg_offset);

	pcfg_value = i2c_slave_readl(i2c->port_cfg + pcfg_offset);

	dev_info(i2c->dev, "[INFO][I2C][%s][GFB]  SCL: 0x%X SDA: 0x%X\n",
		 __func__, i2c->port_mux[0], i2c->port_mux[1]);

	return 0;
}
#else
static int32_t tcc_i2c_slave_set_port(struct tcc_i2c_slave *i2c)
{
	int32_t i;
	uint32_t reg_shift, pcfg_val;
	uint8_t port = (u8) i2c->port_mux;

	if (i2c->id >= TCC_I2C_SLV_MAX_NUM) {
		dev_err(i2c->dev, "[ERROR][I2C] %s: id %d is invalid!\n",
			__func__, i2c->id);
		return -EINVAL;
	}

	if (port == 0xFFU) {
		dev_err(i2c->dev, "[ERROR][I2C] %s: port %d is invalid!\n",
			__func__, port);
		return -EINVAL;
	};

	/* Check Master port */
	pcfg_val = i2c_slave_readl(i2c->port_cfg + I2C_PORT_CFG0);
	for (i = 0; i < 4; i++) {
		reg_shift = (u32) i << 3;
		if (port == ((pcfg_val >> reg_shift) & 0xFFU)) {
			/* Clear duplicated port */
			pcfg_val |= ((u32) 0xFFU << reg_shift);
		}
	}
	i2c_slave_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG0);

	pcfg_val = i2c_slave_readl(i2c->port_cfg + I2C_PORT_CFG2);
	for (i = 0; i < 4; i++) {
		reg_shift = (u32) i << 3;
		if (port == ((pcfg_val >> reg_shift) & 0xFFU)) {
			/* Clear duplicated port */
			pcfg_val |= ((u32) 0xFFU << reg_shift);
		}
	}
	i2c_slave_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG2);

	/* Check Slave port */
	pcfg_val = i2c_slave_readl(i2c->port_cfg + I2C_PORT_CFG1);
	for (i = 0; i < 4; i++) {
		reg_shift = (u32) i << 3;
		if (port == ((pcfg_val >> reg_shift) & 0xFFU)) {
			/* Clear duplicated port */
			pcfg_val |= ((u32) 0xFFU << reg_shift);
		}
	}
	i2c_slave_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG1);

	/* Set i2c port-mux */
	reg_shift = (u32) i2c->id << 3;
	pcfg_val = i2c_slave_readl(i2c->port_cfg + I2C_PORT_CFG1);
	pcfg_val &= ~((u32) 0xFFU << reg_shift);
	pcfg_val |= ((u32) port << reg_shift);
	i2c_slave_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG1);

	dev_dbg(i2c->dev, "[DEBUG][I2C][%s] PORT MUX: %d\n", __func__, port);

	return 0;
}
#endif

/* Initialize i2c slave */
static void tcc_i2c_slave_hwinit(struct tcc_i2c_slave *i2c)
{
	uint32_t addr;

	addr = ((u32) i2c->addr & 0x7FU) << 1;
	/*
	 * Enable i2c, i2c spec. protocol and FIFO access
	 * Disable clock stretching (BIT(4) 0: enable 1: disable)
	 */
	i2c_slave_writel((BIT(0) | BIT(1) | BIT(4)), i2c->regs + I2C_CTL);
	i2c_slave_writel(addr, i2c->regs + I2C_ADDR);

	/* Enable data buffer interrupt */
	i2c_slave_writel((BIT(8) | BIT(9)), i2c->regs + I2C_INT);
}

static int32_t tcc_i2c_slave_open(struct inode *inode, struct file *filp)
{
	uint32_t minor;
	int32_t ret;
	ulong flags = 0;
	struct tcc_i2c_slave *i2c;
	void *pv_i2c;

	if (filp->private_data == NULL) {
		minor = iminor(inode);
		if (minor < TCC_I2C_SLV_MAX_NUM) {
			filp->private_data = &i2c_slave_priv[minor];
		} else {
			pr_err("[ERROR][I2C] %s: wrong minor number %d\n",
			       __func__, minor);
			return -EINVAL;
		}
	}

	pv_i2c = filp->private_data;
	i2c = pv_i2c;
	spin_lock_irqsave(&i2c->lock, flags);

	if (i2c->opened) {
		spin_unlock_irqrestore(&i2c->lock, flags);
		return -EBUSY;
	}

	if (i2c->read_buf == NULL) {
		i2c->read_buf = kmalloc(i2c->rw_buf_size, GFP_KERNEL);
		if (i2c->read_buf == NULL) {
			spin_unlock_irqrestore(&i2c->lock, flags);
			ret = -ENOMEM;
			goto memerr;
		}
	}

	if (i2c->write_buf == NULL) {
		i2c->write_buf = kmalloc(i2c->rw_buf_size, GFP_KERNEL);
		if (i2c->write_buf == NULL) {
			spin_unlock_irqrestore(&i2c->lock, flags);
			ret = -ENOMEM;
			goto memerr;
		}
	}
	dev_dbg(i2c->dev,
		"[DEBUG][I2C] success alloc read/write buf mem (%ld)\n",
		i2c->rw_buf_size);

	/* set port mux */
	ret = tcc_i2c_slave_set_port(i2c);
	if (ret != 0) {
		dev_err(i2c->dev,
			"[ERROR][I2C] failed to set port configuration\n");
		spin_unlock_irqrestore(&i2c->lock, flags);
		ret = -ENXIO;
		goto memerr;
	}

	if (i2c->hclk != NULL) {
		ret = clk_prepare_enable(i2c->hclk);
		if (ret != 0) {
			dev_err(i2c->dev,
				"[ERROR][I2C] can't do i2c slave hclk clock enable\n");
			spin_unlock_irqrestore(&i2c->lock, flags);
			ret = -ENXIO;
			goto memerr;
		}
	} else {
		dev_err(i2c->dev,
			"[ERROR][I2C] failed to enable i2c slave clock\n");
		spin_unlock_irqrestore(&i2c->lock, flags);
		ret = -ENXIO;
		goto memerr;
	}

	ret = request_irq((uint32_t)i2c->irq,
			tcc_i2c_slave_isr,
			IRQF_SHARED,
			dev_name(i2c->dev),
			pv_i2c);
	if (ret < 0) {
		dev_err(i2c->dev, "[ERROR][I2C] failed to request irq %i\n",
			i2c->irq);
		spin_unlock_irqrestore(&i2c->lock, flags);
		goto clkerr;
	}

	i2c->opened = (bool)true;

	init_waitqueue_head(&(i2c->wait_q));

	/* Initialize circular buffer */
	i2c->tx_buf.circ.head = 0;
	i2c->tx_buf.circ.tail = i2c->tx_buf.circ.head;
	i2c->rx_buf.circ.head = 0;
	i2c->rx_buf.circ.tail = i2c->rx_buf.circ.head;

	i2c->is_tx_dma_running = 0;

	dev_dbg(i2c->dev, "[DEBUG][I2C] %s\n", __func__);

	spin_unlock_irqrestore(&i2c->lock, flags);

	return ret;

clkerr:
	if (i2c->hclk != NULL) {
		clk_disable(i2c->hclk);
	}
memerr:

	if (i2c->read_buf != NULL) {
		kfree(i2c->read_buf);
	}
	if (i2c->write_buf != NULL) {
		kfree(i2c->write_buf);
	}

	i2c->read_buf = NULL;
	i2c->write_buf = NULL;

	return ret;
}

static int32_t tcc_i2c_slave_release(struct inode *inode, struct file *filp)
{
	struct tcc_i2c_slave *i2c;
	ulong flags = 0;
	void *pv_i2c;

	if (filp->private_data == NULL) {
		return 0;
	}

	pv_i2c = (struct tcc_i2c_slave *)filp->private_data;
	i2c = pv_i2c;
	dev_dbg(i2c->dev, "[DEBUG][I2C] %s\n", __func__);

	spin_lock_irqsave(&i2c->lock, flags);

	/* Disable i2c slave core */
	i2c_slave_writel(0x0, i2c->regs + I2C_CTL);

	/* Clear and disable interrupt */
	i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_INT),
			 i2c->regs + I2C_INT);
	i2c_slave_writel(0x0, i2c->regs + I2C_INT);

	i2c->addr = 0xff;
	i2c_slave_writel(i2c->addr, i2c->regs + I2C_ADDR);

	if (i2c->hclk != NULL) {
		clk_disable(i2c->hclk);
	}

	free_irq(i2c->irq, pv_i2c);

	if (i2c->read_buf != NULL) {
		kfree(i2c->read_buf);
	}
	if (i2c->write_buf != NULL) {
		kfree(i2c->write_buf);
	}

	i2c->read_buf = NULL;
	i2c->write_buf = NULL;
	i2c->opened = (bool)false;

	spin_unlock_irqrestore(&i2c->lock, flags);

	tcc_i2c_slave_stop_dma_engine(i2c, I2C_SLV_RX);
	tcc_i2c_slave_stop_dma_engine(i2c, I2C_SLV_TX);

	filp->private_data = NULL;

	return 0;
}

static ssize_t tcc_i2c_slave_read(struct file *filp, char *buf, size_t sz,
				  loff_t *ppos)
{
	struct tcc_i2c_slave *i2c;
	ulong err;
	size_t offset, rd_sz, size;

	if (filp->private_data == NULL) {
		pr_err("[ERROR][I2C] %s: priv data is invalid\n", __func__);
		return -EFAULT;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;
	size = sz;
	offset = 0;

	while (1) {
		rd_sz = (size > i2c->rw_buf_size) ? i2c->rw_buf_size : size;
		rd_sz = (size_t)tcc_i2c_slave_pop(i2c,
				i2c->read_buf,
				(int32_t)rd_sz,
				I2C_SLV_RX);
		if (rd_sz != 0U) {
			err = copy_to_user(buf + offset, i2c->read_buf, rd_sz);
			if (err != 0U) {
				dev_err(i2c->dev,
					"[ERROR][I2C] failed to copy_to_user\n");
				return (ssize_t) err;
			}
		} else {
			break;
		}
		offset += rd_sz;
		if (size < rd_sz) {
			dev_err(i2c->dev,
					"[ERROR][I2C] %s: size %ld is under\n",
					__func__, size);
		}else {
			size -= rd_sz;
		}

		if (size == 0UL) {
			break;
		}
	}

	return (ssize_t) offset;
}

static ssize_t tcc_i2c_slave_write(struct file *filp, const char *buf,
				   size_t sz, loff_t *ppos)
{
	struct tcc_i2c_slave *i2c;
	uint32_t status;
	uint8_t cnt, val;
	ulong err, flags;
	size_t offset, wr_sz, size;
	ssize_t ret = 0;

	if (filp->private_data == NULL) {
		pr_err("[ERROR][I2C] %s: priv data is invalid\n", __func__);
		return 0;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;
	size = sz;

	offset = 0;
	while (1) {
		wr_sz = (size > i2c->rw_buf_size) ? i2c->rw_buf_size : size;
		err = copy_from_user(i2c->write_buf, buf + offset, wr_sz);
		if (err != 0U) {
			dev_err(i2c->dev,
				"[ERROR][I2C] failed to copy_to_user\n");
			return (ssize_t) err;
		}
		wr_sz = (size_t)tcc_i2c_slave_push(i2c,
				i2c->write_buf,
				(int32_t)wr_sz,
				I2C_SLV_TX);
		if (wr_sz == 0U) {
			break;
		}
		offset += wr_sz;
		if (size < wr_sz) {
			dev_err(i2c->dev,
					"[ERROR][I2C] %s: size %ld is under\n",
					__func__, size);
		}else {
			size -= wr_sz;
		}

		if (size == 0U) {
			break;
		}
	}

	if (offset == 0U) {
		return (ssize_t) offset;
	}
	dev_dbg(i2c->dev, "[DEBUG][I2C] succeed to push data(%ld)", offset);

	if (i2c->dma.chan_tx == NULL) {
		/* PIO mode */
		ret = (ssize_t) offset;

		/* Check whether tx fifo level interrupt */
		status = i2c_slave_readl(i2c->regs + I2C_INT);
		if ((status & BIT(1)) == 0U) {
			/* Until fifo is full, dequeue and write data at fifo */
			while (1) {
				cnt = tcc_i2c_slave_tx_fifo_cnt(i2c);
				if (cnt >= TCC_I2C_SLV_MAX_FIFO_CNT) {
					break;
				}
				err = (ulong)
				    tcc_i2c_slave_pop_one_byte(i2c, &val,
							       I2C_SLV_TX);
				if (err != 0U) {
					break;
				} else {
					i2c_slave_writeb(val,
							 i2c->regs + I2C_DPORT);
				}
			}

			/* Enable interrupt */
			status = i2c_slave_readl(i2c->regs + I2C_INT);
			status &= 0x0FFFU;
			status |= BIT(1);
			i2c_slave_writel(status, i2c->regs + I2C_INT);
		}
		dev_dbg(i2c->dev, "[DEBUG][I2C] %s: tx fifo cnt 0x%x\n",
			__func__, tcc_i2c_slave_tx_fifo_cnt(i2c));
	} else {
		/* DMA mode */
		spin_lock_irqsave(&i2c->lock, flags);
		if (i2c->is_tx_dma_running == 0) {
			int32_t copy_size;

			i2c->is_tx_dma_running = 1;
			spin_unlock_irqrestore(&i2c->lock, flags);

			copy_size = tcc_i2c_slave_pop(i2c,
					i2c->dma.tx_v_addr,
					i2c->dma.size,
					I2C_SLV_TX);
			if (copy_size == 0) {
				dev_err(i2c->dev,
					"[ERROR][I2C] failed to pop data from tx fifo\n");
				return (ssize_t)copy_size;
			}
			dev_dbg(i2c->dev,
				"[DEBUG][I2C] start tx dma (size %08d)\n",
				copy_size);

			/* Enable Tx DMA request */
			i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_CTL) |
					 BIT(17), i2c->regs + I2C_CTL);
			ret = tcc_i2c_slave_dma_dma_engine_submit(i2c,
					tcc_i2c_slave_tx_dma_callback,
					i2c, (ssize_t)copy_size,
					I2C_SLV_TX);
			if (ret != 0) {
				tcc_i2c_slave_stop_dma_engine(i2c, I2C_SLV_TX);
				ret = 0;

				spin_lock_irqsave(&i2c->lock, flags);
				i2c->is_tx_dma_running = 0;
				spin_unlock_irqrestore(&i2c->lock, flags);

				spin_lock_irqsave(&i2c->tx_buf.lock, flags);
				i2c->tx_buf.circ.head = 0;
				i2c->tx_buf.circ.tail = i2c->tx_buf.circ.head;
				spin_unlock_irqrestore(&i2c->tx_buf.lock,
						       flags);

				dev_err(i2c->dev,
					"[ERROR][I2C] failed to submit tx dma-engine descriptor\n");
			} else {
				ret = (ssize_t) offset;
			}
		}
	}

	return ret;
}

static long tcc_i2c_slave_ioctl(struct file *filp, uint32_t cmd,
				ulong arg)
{
	struct tcc_i2c_slave *i2c;
	uint8_t addr, val[2], reg_shift;
	ulong flags = 0;
	uint32_t temp;
	int32_t count;
	long ret;

	if (filp->private_data == NULL) {
		pr_err("[ERROR][I2C] %s: priv data is invalid\n", __func__);
		return -EFAULT;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	switch (cmd) {
	case IOCTL_I2C_SLAVE_SET_ADDR:
		/* Set address */
		if (copy_from_user(&addr, (void *)arg, sizeof(addr)) != 0UL) {
			ret = -EFAULT;
			break;
		}

		/* set i2c slave address */
		if (addr > 0x7FU) {
			dev_err(i2c->dev,
				"[ERROR][I2C] %s: slave address is invalid addr 0x%02X\n",
				__func__, (uint32_t)addr);
			ret = -EINVAL;
			break;
		}

		spin_lock_irqsave(&i2c->lock, flags);

		/* set address */
		i2c->addr = addr;

		tcc_i2c_slave_hwinit(i2c);

		temp = i2c_slave_readl(i2c->regs + I2C_ADDR);
		ret = (temp >> 1U) & 0x7FU;

		spin_unlock_irqrestore(&i2c->lock, flags);

		dev_dbg(i2c->dev, "[DEBUG][I2C] %s: set slave addr 0x%x\n",
			__func__, i2c->addr);
		break;
	case IOCTL_I2C_SLAVE_GET_ADDR:
		/* Get address */
		spin_lock_irqsave(&i2c->lock, flags);

		temp = i2c_slave_readl(i2c->regs + I2C_ADDR);
		ret = (temp >> 1U) & 0x7FU;

		spin_unlock_irqrestore(&i2c->lock, flags);

		dev_dbg(i2c->dev, "[DEBUG][I2C]%s get slave addr 0x%08lx\n",
			__func__, ret);
		break;
	case IOCTL_I2C_SLAVE_SET_MB:
		/* Set memory buffers (MB0/MB1), sub-address 0x00 - 0x07 */
		(void)memset(val, 0xFF, sizeof(uint8_t) * 2U);
		if (copy_from_user(val, (void *)arg, sizeof(uint8_t) * 2U)
		    != 0UL) {
			ret = -EFAULT;
			dev_err(i2c->dev,
				"[ERROR][I2C] [%s:%d] failed to copy mem from user\n",
				__func__, __LINE__);
			break;
		}

		if (val[0] > 0x07U) {
			ret = -EINVAL;
			dev_err(i2c->dev,
				"[ERROR][I2C] [%s:%d] wrong sub address 0x%02x\n",
				__func__, __LINE__, val[0]);
			break;
		}

		addr = val[0];
		dev_dbg(i2c->dev, "[DEBUG][I2C] %s: set 0x%02x at 0x%02x\n",
			__func__, addr, val[1]);

		spin_lock_irqsave(&i2c->lock, flags);

		if (addr < 0x4U) {
			reg_shift = (addr << 3U);
			temp = i2c_slave_readl(i2c->regs + I2C_MB0);
			temp = temp & ~((u32) 0xFFU << reg_shift);
			temp = temp | ((u32) val[1] << reg_shift);
			i2c_slave_writel(temp, i2c->regs + I2C_MB0);
		} else {
			reg_shift = (addr - 4U) << 3U;
			temp = i2c_slave_readl(i2c->regs + I2C_MB1);
			temp = temp & ~((u32) 0xFFU << reg_shift);
			temp = temp | ((u32) val[1] << reg_shift);
			i2c_slave_writel(temp, i2c->regs + I2C_MB1);
		}

		spin_unlock_irqrestore(&i2c->lock, flags);

		ret = 0;
		break;

	case IOCTL_I2C_SLAVE_GET_MB:
		/* Get memory buffers (MB0/MB1), sub-address 0x00 - 0x07 */
		if (copy_from_user(&addr, (void *)arg, sizeof(addr)) != 0UL) {
			ret = -EFAULT;
			dev_err(i2c->dev,
				"[ERROR][I2C] [%s:%d] failed to copy mem from user\n",
				__func__, __LINE__);
			break;
		}

		if (addr > 0x07U) {
			ret = -EINVAL;
			dev_err(i2c->dev,
				"[ERROR][I2C] [%s:%d] wrong sub address 0x%02x\n",
				__func__, __LINE__, addr);
			break;
		}

		spin_lock_irqsave(&i2c->lock, flags);

		if (addr < 0x4U) {
			temp = i2c_slave_readl(i2c->regs + I2C_MB0);
			temp = ((temp >> (addr << 3U)) & 0xFFU);
		} else {
			temp = i2c_slave_readl(i2c->regs + I2C_MB1);
			temp = ((temp >> ((addr - 4U) << 3U)) & 0xFFU);
		}
		ret = (long)temp;

		spin_unlock_irqrestore(&i2c->lock, flags);

		dev_dbg(i2c->dev,
			"[DEBUG][I2C] %s: get sub address 0x%02x = 0x%02lx\n",
			__func__, addr, ret);
		break;
	case IOCTL_I2C_SLAVE_SET_POLL_CNT:
		/* Get memory buffers (MB0/MB1), sub-address 0x00 - 0x07 */
		if (copy_from_user(&count, (void *)arg, sizeof(count)) != 0UL) {
			ret = -EFAULT;
			dev_err(i2c->dev,
				"[ERROR][I2C] [%s:%d] failed to copy mem from user\n",
				__func__, __LINE__);
			break;
		}

		if (count > i2c->buf_size) {
			i2c->poll_count = i2c->buf_size;
			dev_dbg(i2c->dev,
				"[DEBUG][I2C] poll count(%d) is bigger than queue size(%d), poll count %d\n",
				count, i2c->buf_size, i2c->poll_count);
		} else {
			i2c->poll_count = count;
		}

		/* Set interrupt */
		if (i2c->dma.chan_rx != NULL) {
			/* DMA mode - disable Rx FIFO level interrupt */
			if (i2c->poll_count > i2c->dma.size) {
				i2c->poll_count = i2c->dma.size;
			}

			/* Enable Rx DMA request */
			i2c_slave_writel((i2c_slave_readl(i2c->regs + I2C_CTL) |
					 BIT(16)), i2c->regs + I2C_CTL);
			ret =
				tcc_i2c_slave_dma_dma_engine_submit(i2c,
						tcc_i2c_slave_rx_dma_callback,
						i2c,
						(ssize_t)i2c->poll_count,
						I2C_SLV_RX);
			if (ret != 0) {
				dev_err(i2c->dev,
					"[ERROR][I2C] %s: failed to submit rx dma-engine descriptor\n",
					__func__);
			}
		} else {
			/* PIO mode - enable Rx FIFO level interrupt */
			i2c_slave_writel((i2c_slave_readl(i2c->regs + I2C_INT) |
					  BIT(0)), i2c->regs + I2C_INT);
			ret = 0;
		}
		dev_dbg(i2c->dev, "[DEBUG][I2C] %s: set poll count %d\n",
			__func__, i2c->poll_count);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static uint32_t tcc_i2c_slave_poll(struct file *filp,
				       struct poll_table_struct *wait)
{
	struct tcc_i2c_slave *i2c;
	int32_t circ_q_cnt;
	ulong flags;

	if (filp->private_data == NULL) {
		pr_err("[ERROR][I2C] %s: priv data is invalid\n", __func__);
		return POLL_ERR;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	spin_lock_irqsave(&i2c->rx_buf.lock, flags);
	circ_q_cnt = circ_cnt(&(i2c->rx_buf.circ), i2c->buf_size);
	spin_unlock_irqrestore(&i2c->rx_buf.lock, flags);
	if (circ_q_cnt >= (int32_t)i2c->poll_count) {
		dev_dbg(i2c->dev,
			"[DEBUG][I2C] Rx count(%d) is over poll count(%d)\n",
			circ_q_cnt, i2c->poll_count);
		return (POLL_IN | POLLRDNORM);
	}

	poll_wait(filp, &(i2c->wait_q), wait);

	spin_lock_irqsave(&i2c->rx_buf.lock, flags);
	circ_q_cnt = circ_cnt(&(i2c->rx_buf.circ), i2c->buf_size);
	spin_unlock_irqrestore(&i2c->rx_buf.lock, flags);
	if (circ_q_cnt >= (int32_t)i2c->poll_count) {
		dev_dbg(i2c->dev,
			"[DEBUG][I2C] Rx count(%d) is over poll count(%d)\n",
			circ_q_cnt, i2c->poll_count);
		return (POLL_IN | POLLRDNORM);
	}

	return POLL_ERR;
}

const struct file_operations tcc_i2c_slv_fops = {
	.owner = THIS_MODULE,
	.open = tcc_i2c_slave_open,
	.release = tcc_i2c_slave_release,
	.compat_ioctl = tcc_i2c_slave_ioctl,
	.unlocked_ioctl = tcc_i2c_slave_ioctl,
	.read = tcc_i2c_slave_read,
	.write = tcc_i2c_slave_write,
	.poll = tcc_i2c_slave_poll,
};

/* Get port configuration */
#ifdef CONFIG_OF
static int32_t tcc_i2c_slave_parse_dt(struct device_node *np,
				  struct tcc_i2c_slave *i2c)
{
	int32_t ret;
#ifdef TCC_USE_GFB_PORT
	/* Port array order : [0]SCL [1]SDA */
	ret = of_property_read_u32_array(np, "port-mux", i2c->port_mux,
			(size_t)of_property_count_elems_of_size(np,
				"port-mux",
				sizeof(u32)));

#else
	ret = of_property_read_u32(np, "port-mux", &i2c->port_mux);
#endif
	if (ret != 0) {
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to get port-mux\n",
			__func__);
		return ret;
	}

	return 0;
}
#else
static int32_t tcc_i2c_slave_parse_dt(struct device_node *np,
				  struct tcc_i2c_slave *i2c)
{
	return -ENXIO;
}
#endif

static int32_t tcc_i2c_slave_probe(struct platform_device *pdev)
{
	struct tcc_i2c_slave *i2c;
	struct resource *res;
	struct device *dev;
	void __iomem *byte_cmd;
	void *pv_i2c;
	uint32_t id, byte_cmd_val;
	int32_t ret;

	if (pdev->dev.of_node == NULL) {
		return -EINVAL;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "id", &id);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][I2C] failed to get id\n");
		return ret;
	}

	if (id >= TCC_I2C_SLV_MAX_NUM) {
		dev_err(&pdev->dev, "[ERROR][I2C] wrong id %d\n", id);
		return -EINVAL;
	}

	pv_i2c = &i2c_slave_priv[id];
	i2c = pv_i2c;
	(void)memset(i2c, 0, sizeof(struct tcc_i2c_slave));
	i2c->id = id;

	/* get base register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][I2C] failed to get base address\n");
		return -ENODEV;
	}

	i2c->phy_regs = (u32) res->start;
	dev_dbg(&pdev->dev, "[DEBUB][I2C] base 0x%08x\n", i2c->phy_regs);

	i2c->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->regs)) {
		ret = (int32_t)PTR_ERR(i2c->regs);
		dev_err(&pdev->dev,
			"[ERROR][I2C] failed to remap base address\n");
		return ret;
	}

	/* get i2c port config register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res == NULL) {
		dev_err(&pdev->dev,
			"[ERROR][I2C] failed to get port config address\n");
		return -ENODEV;
	}

	dev_dbg(&pdev->dev, "[DEBUG][I2C] port 0x%08llx\n", res->start);

	i2c->port_cfg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->port_cfg)) {
		ret = (int32_t)PTR_ERR(i2c->port_cfg);
		dev_err(&pdev->dev,
			"[ERROR][I2C] failed to remap port config address\n");
		return ret;
	}

	/* get port config info */
	ret = tcc_i2c_slave_parse_dt(pdev->dev.of_node, i2c);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][I2C] failed to get port-mux and id\n");
		return ret;
	}

	/* get byte command register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res != NULL) {
		dev_info(&pdev->dev,
			 "[INFO][I2C] set memory bus byte command masking register\n");
		dev_dbg(&pdev->dev,
			"[DEBUG][I2C] byte command register 0x%08llx\n",
			res->start);
		byte_cmd = devm_ioremap_resource(&pdev->dev, res);
		if (i2c_slave_readl(byte_cmd) == 0xFFFF0000U) {
			byte_cmd_val =
			    0xFFF00000U | ((i2c->phy_regs >> 16U) & 0x0000FFF0U);
			dev_dbg(&pdev->dev,
				"[DEBUG][I2C] byte command write value = 0x%08x\n",
				byte_cmd_val);
			i2c_slave_writel(byte_cmd_val, byte_cmd);
		}
		dev_dbg(&pdev->dev,
			"[DEBUG][I2C] byte command read value  = 0x%08x\n",
			i2c_slave_readl(byte_cmd));
	}

	/* get i2c irq number */
	i2c->irq = platform_get_irq(pdev, 0);
	if (i2c->irq < 0) {
		dev_err(&pdev->dev, "[ERROR][I2C] failed to get irq number\n");
		return i2c->irq;
	}
	dev_dbg(&pdev->dev, "[DEBUG][I2C] irq 0x%08x\n", i2c->irq);

	/* get and set iobus clk */
	i2c->hclk = of_clk_get(pdev->dev.of_node, 0);
	if (i2c->hclk == NULL) {
		dev_err(&pdev->dev, "[ERROR][I2C] failed to get clock info\n");
		return -ENXIO;
	}

	i2c->dev = &pdev->dev;
	i2c->addr = 0xFF;
	i2c->opened = (bool)false;
	i2c->buf_size = TCC_I2C_SLV_DMA_BUF_SIZE;
	i2c->poll_count = TCC_I2C_SLV_DEF_POLL_CNT;
	i2c->rw_buf_size = TCC_I2C_SLV_WR_BUF_SIZE;
	i2c->write_buf = NULL;
	i2c->read_buf = NULL;

	/* Allocate buffer */
	ret = tcc_i2c_slave_init_buf(i2c, 1);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][I2C] Failed to allocate rx dma buffer\n");
		goto err_clk;
	}
	ret = tcc_i2c_slave_init_buf(i2c, 0);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][I2C] Failed to allocate tx dma buffer\n");
		goto err_mem_rx;
	}

	/* Disable i2c slave core */
	i2c_slave_writel(0x0, i2c->regs + I2C_CTL);

	dev =
	    device_create(tcc_i2c_slave_class, NULL,
			  MKDEV(tcc_i2c_slave_major, i2c->id), NULL,
			  TCC_I2C_SLV_DEV_NAMES, i2c->id);
	if (IS_ERR(dev)) {
		dev_err(&pdev->dev, "[ERROR][I2C] failed to create device\n");
		ret = (int32_t)PTR_ERR(dev);
		goto err_mem_tx;
	}

	spin_lock_init(&i2c->lock);
	platform_set_drvdata(pdev, pv_i2c);

	ret = device_create_file(&pdev->dev, &dev_attr_fifo_byte);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"[ERROR][I2C] failed to create attr (fifo byte)\n");
		goto err_dev;
	}

	ret = tcc_i2c_slave_dma_engine_probe(pdev, i2c);
	if (ret != 0) {
		dev_dbg(&pdev->dev,
			"[DEBUG][I2C] slave ch %d - TX/RX PIO mode\n", i2c->id);
	}

	if (i2c->dma.chan_tx != NULL) {
		dev_info(&pdev->dev,
			 "[INFO][I2C] slave ch %d - Tx DMA mode(%s %p)\n",
			 i2c->id, dma_chan_name(i2c->dma.chan_tx),
			 i2c->dma.chan_tx);
	} else {
		dev_info(&pdev->dev, "[INFO][I2C] slave ch %d - Tx PIO mode\n",
			 i2c->id);
	}

	if (i2c->dma.chan_rx != NULL) {
		dev_info(&pdev->dev,
			 "[INFO][I2C] slave ch %d - Rx DMA mode(%s %p)\n",
			 i2c->id, dma_chan_name(i2c->dma.chan_rx),
			 i2c->dma.chan_rx);
	} else {
		dev_info(&pdev->dev, "[INFO][I2C] slave ch %d - Rx PIO mode\n",
			 i2c->id);
	}

	dev_dbg(&pdev->dev, "[DEBUG][I2C] debugging log is enabled\n");
#ifdef TCC_USE_GFB_PORT
	dev_info(&pdev->dev, "[INFO][I2C] slave ch %d gfb(scl %d sda %d)\n",
		 i2c->id, i2c->port_mux[0], i2c->port_mux[1]);
#else
	dev_info(&pdev->dev, "[INFO][I2C] slave ch %d port %d\n",
		 i2c->id, i2c->port_mux);
#endif

	return 0;

err_dev:
	device_destroy(tcc_i2c_slave_class,
		       MKDEV(tcc_i2c_slave_major, i2c->id));
err_mem_tx:
	tcc_i2c_slave_deinit_buf(i2c, 0);
err_mem_rx:
	tcc_i2c_slave_deinit_buf(i2c, 1);
err_clk:
	clk_disable(i2c->hclk);
	clk_put(i2c->hclk);
	i2c->hclk = NULL;
	return ret;

}

static int32_t tcc_i2c_slave_remove(struct platform_device *pdev)
{
	struct tcc_i2c_slave *i2c = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	device_remove_file(&pdev->dev, &dev_attr_fifo_byte);
	device_destroy(tcc_i2c_slave_class,
		       MKDEV(tcc_i2c_slave_major, i2c->id));

	tcc_i2c_slave_deinit_buf(i2c, 0);
	tcc_i2c_slave_deinit_buf(i2c, 1);

	tcc_i2c_slave_release_dma_engine(i2c);

	/* I2C bus & clock disable */
	if (i2c->hclk != NULL) {
		clk_disable(i2c->hclk);
		clk_put(i2c->hclk);
		i2c->hclk = NULL;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_i2c_slave_suspend(struct device *dev)
{
	struct tcc_i2c_slave *i2c = dev_get_drvdata(dev);

	spin_lock(&i2c->lock);
	if (i2c->hclk != NULL) {
		clk_disable_unprepare(i2c->hclk);
	}
	i2c->is_suspended = (bool)true;
	spin_unlock(&i2c->lock);
	return 0;
}

static int32_t tcc_i2c_slave_resume(struct device *dev)
{
	int32_t ret;
	struct tcc_i2c_slave *i2c = dev_get_drvdata(dev);

	spin_lock(&i2c->lock);
	/* Enable Clock */
	if (i2c->hclk != NULL) {
		ret = clk_prepare_enable(i2c->hclk);
		if (ret < 0) {
			dev_err(i2c->dev,
				"[ERROR][I2C] can't do i2c slave hclk clock enable\n");
			spin_unlock(&i2c->lock);
			return -ENXIO;
		}
	}
	/* Config Pinctrl */
	i2c->pinctrl = devm_pinctrl_get_select(dev, "default");
	if (IS_ERR(i2c->pinctrl)) {
		dev_err(i2c->dev,
			"[ERROR][I2C] Failed to get pinctrl (default state)\n");
		clk_disable_unprepare(i2c->hclk);
		spin_unlock(&i2c->lock);
		return -ENODEV;
	}
	/* Initialize I2C Slave */
	tcc_i2c_slave_hwinit(i2c);
	i2c->is_suspended = (bool)false;

	spin_unlock(&i2c->lock);
	return 0;
}

static ssize_t tcc_i2c_slave_fifo_byte_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct tcc_i2c_slave *i2c;
	int32_t ret;
	uint32_t status;
	uint8_t val, cnt;
	ulong flags;

	dev_dbg(dev, "[DEBUG][I2C] %s\n", __func__);

	i2c = dev_get_drvdata(dev);
	ret = kstrtou8(buf, 16, &val);
	if (ret < 0) {
		pr_err("[ERROR][I2C] %s: failed to get fifo byte\n", __func__);
	}

	if (IS_ERR(i2c)) {
		pr_err("[ERROR][I2C] %s: null pointer err\n", __func__);
		return 0;
	}

	if (i2c->dma.chan_tx == NULL) {
		/* PIO mode */
		ret = tcc_i2c_slave_push_one_byte(i2c, val, I2C_SLV_TX);
		if (ret != 0) {
			dev_err(i2c->dev,
				"[ERROR][I2C] failed to push 0x%02x at queue\n",
				val);
		} else {
			dev_dbg(i2c->dev, "[DEBUG][I2C] send 0x%02x\n", val);
			/* Check whether tx fifo level interrupt */
			status = i2c_slave_readl(i2c->regs + I2C_INT);
/* Until fifo is full, dequeue and write data at fifo */
			if ((status & BIT(1)) == 0U) {
				while (1) {
					cnt = tcc_i2c_slave_tx_fifo_cnt(i2c);
					if (cnt >= TCC_I2C_SLV_MAX_FIFO_CNT) {
						break;
					}
					ret =
						tcc_i2c_slave_pop_one_byte(i2c,
								&val,
								I2C_SLV_TX);
					if (ret != 0) {
						break;
					} else {
						i2c_slave_writeb(val,
								 i2c->regs +
								 I2C_DPORT);
					}
				}

				/* Enable interrupt */
				status = i2c_slave_readl(i2c->regs + I2C_INT);
				status &= (u32) 0x0FFFU;
				status |= BIT(1);
				i2c_slave_writel(status, i2c->regs + I2C_INT);
			}
		}
		dev_info(dev, "[INFO][I2C] tx fifo cnt 0x%x\n",
			 tcc_i2c_slave_tx_fifo_cnt(i2c));
	} else {
		/* DMA mode */
		spin_lock_irqsave(&i2c->lock, flags);
		if (i2c->is_tx_dma_running != 0) {
			ret = tcc_i2c_slave_push_one_byte(i2c, val, I2C_SLV_TX);
			if (ret != 0) {
				dev_err(i2c->dev,
					"[ERROR][I2C] failed to push 0x%02x at queue\n",
					val);
			}
			spin_unlock_irqrestore(&i2c->lock, flags);
		} else {
			i2c->is_tx_dma_running = 1;
			spin_unlock_irqrestore(&i2c->lock, flags);

			(void)memcpy(i2c->dma.tx_v_addr, &val, sizeof(char));
			dev_dbg(i2c->dev,
				"[DEBUG][I2C] start tx dma (dma_buf 1 byte)\n");

			/* Enable Tx DMA request */
			i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_CTL) |
					BIT(17), i2c->regs + I2C_CTL);
			ret =
				tcc_i2c_slave_dma_dma_engine_submit(i2c,
						tcc_i2c_slave_tx_dma_callback,
						i2c, 1,
						I2C_SLV_TX);
			if (ret != 0) {
				tcc_i2c_slave_stop_dma_engine(i2c, I2C_SLV_TX);

				spin_lock_irqsave(&i2c->lock, flags);
				i2c->is_tx_dma_running = 0;
				spin_unlock_irqrestore(&i2c->lock, flags);

				spin_lock_irqsave(&i2c->tx_buf.lock, flags);
				i2c->tx_buf.circ.head = 0;
				i2c->tx_buf.circ.tail = i2c->tx_buf.circ.head;
				spin_unlock_irqrestore(&i2c->tx_buf.lock,
						       flags);

				dev_err(i2c->dev,
					"[ERROR][I2C] failed to submit tx dma-engine descriptor\n");
			}
		}
	}

	return (ssize_t)count;
}

static ssize_t tcc_i2c_slave_fifo_byte_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct tcc_i2c_slave *i2c;
	int32_t ret;
	char val;

	dev_dbg(dev, "[DEBUG][I2C] %s\n", __func__);

	i2c = dev_get_drvdata(dev);

	if (IS_ERR(i2c)) {
		pr_err("[ERROR][I2C] %s null pointer err\n", __func__);
		return 0;
	}

	dev_info(i2c->dev, "[INFO][I2C] rx fifo cnt 0x%x\n",
		 tcc_i2c_slave_rx_fifo_cnt(i2c));

	ret = tcc_i2c_slave_pop_one_byte(i2c, &val, I2C_SLV_RX);
	if (ret != 0) {
		return sprintf(buf, "no rx data\n");
	}
	return sprintf(buf, "read 0x%02x\n", val);
}

static SIMPLE_DEV_PM_OPS(tcc_i2c_slave_pm,
			 tcc_i2c_slave_suspend, tcc_i2c_slave_resume);
#define TCC_I2C_SLAVE_PM (&tcc_i2c_slave_pm)
#else
#define TCC_I2C_SLAVE_PM (NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_i2c_slave_of_match[] = {
	{.compatible = "telechips,i2c-slave"},
	{.compatible = "telechips,tcc803x-i2c-slave"},
	{.compatible = "telechips,tcc897x-i2c-slave"},
	{.compatible = "telechips,tcc899x-i2c-slave"},
	{.compatible = "telechips,tcc901x-i2c-slave"},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_i2c_slave_of_match);
#endif

static struct platform_driver tcc_i2c_slave_driver = {
	.probe = tcc_i2c_slave_probe,
	.remove = tcc_i2c_slave_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "tcc-i2c-slave",
		   .pm = TCC_I2C_SLAVE_PM,
		   .of_match_table = of_match_ptr(tcc_i2c_slave_of_match),
		   },
};

static int32_t __init tcc_i2c_slave_init(void)
{
	int32_t ret;

	ret = register_chrdev(0, TCC_I2C_SLV_CHRDEV_NAME, &tcc_i2c_slv_fops);
	if (ret < 0) {
		pr_err
		    ("[ERROR][I2C] %s: failed to register character device (ret %d)\n",
		     __func__, ret);
		return ret;
	}

	tcc_i2c_slave_major = (u32) ret;

	tcc_i2c_slave_class = class_create(THIS_MODULE,
					   TCC_I2C_SLV_CHRDEV_CLASS_NAME);
	if (IS_ERR(tcc_i2c_slave_class)) {
		pr_err("[ERROR][I2C] %s: failed to create class\n", __func__);
		ret = (int32_t)PTR_ERR(tcc_i2c_slave_class);
		goto init_err;
	}

	ret = platform_driver_register(&tcc_i2c_slave_driver);
	if (ret != 0) {
		pr_err("[ERROR][I2C] %s: failed to create class\n", __func__);
		goto init_err;
	}

	pr_debug("[DEBUG][I2C] %s: Complete\n", __func__);

	return ret;
init_err:
	unregister_chrdev(tcc_i2c_slave_major, TCC_I2C_SLV_CHRDEV_NAME);

	return ret;
}

static void __exit tcc_i2c_slave_exit(void)
{
	class_destroy(tcc_i2c_slave_class);
	if (tcc_i2c_slave_major != 0U) {
		unregister_chrdev(tcc_i2c_slave_major, TCC_I2C_SLV_CHRDEV_NAME);
	}
	platform_driver_unregister(&tcc_i2c_slave_driver);
}

module_init(tcc_i2c_slave_init);
module_exit(tcc_i2c_slave_exit);

MODULE_AUTHOR("Telechips Inc. linux@telechips.com");
MODULE_DESCRIPTION("Telechips H/W I2C slave driver");
MODULE_LICENSE("GPL");
