/*
 * linux/drivers/i2c/busses/i2c-slave-tcc-chdrv.c
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

#ifndef DEBUG
//#define DEBUG
#endif

#include <linux/device.h>
#include <linux/stat.h>

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
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>

#include <linux/i2c/tcc_i2c_slave.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>

#include "i2c-slave-tcc-chdrv.h"

static struct class *tcc_i2c_slave_class;
static unsigned int tcc_i2c_slave_major;	/* mojor number of device */

static ssize_t tcc_i2c_slave_fifo_byte_store(struct device *dev, struct device_attribute *attr,
										const char *buf, size_t count)
{
	struct tcc_i2c_slave *i2c;
	int ret;
	unsigned int status;
	char val, cnt;
	unsigned long flags;

	dev_dbg(dev, "%s\n", __func__);

	i2c = dev_get_drvdata(dev);
	ret = simple_strtoul(buf, NULL, 16);
	val = (char)(ret & 0xFF);

	if(IS_ERR(i2c)) {
		printk(KERN_ERR "%s null pointer err\n", __func__);
		return 0;
	}

	if(i2c->dma.chan_tx == NULL) {
		/* PIO mode */
		ret = tcc_i2c_slave_push_one_byte(i2c, val, 0);
		if(ret != 0) {
			printk("failed to push 0x%02x at queue\n", val);
		} else {
			printk("0x%02x\n", val);
			/* Check whether tx fifo level interrupt */
			status = i2c_slave_readl(i2c->regs + I2C_INT);
			if((status & BIT(1)) == 0) {
				/* Until fifo is full, dequeue and write data at fifo */
				while(1) {
					cnt = tcc_i2c_slave_tx_fifo_cnt(i2c);
					if(cnt >= TCC_I2C_SLV_MAX_FIFO_CNT) {
						break;
					}
					ret = tcc_i2c_slave_pop_one_byte(i2c, &val, 0);
					if(ret != 0) {
						break;
					} else {
						i2c_slave_writeb(val, i2c->regs + I2C_DPORT);
					}
				}

				/* Enable interrupt */
				status = i2c_slave_readl(i2c->regs + I2C_INT);
				status &= 0x0FFF;
				status |= BIT(1);
				i2c_slave_writel(status, i2c->regs + I2C_INT);
			}
		}
		dev_info(dev, "tx fifo cnt 0x%x\n", tcc_i2c_slave_tx_fifo_cnt(i2c));
	} else {
		/* DMA mode */
		spin_lock_irqsave(&i2c->lock, flags);
		if(i2c->is_tx_dma_running != 0) {
			ret = tcc_i2c_slave_push_one_byte(i2c, val, 0);
			if(ret != 0) {
				printk("failed to push 0x%02x at queue\n", val);
			}
			spin_unlock_irqrestore(&i2c->lock, flags);
		} else {
			i2c->is_tx_dma_running = 1;
			spin_unlock_irqrestore(&i2c->lock, flags);

			memcpy(i2c->dma.tx_v_addr, &val, sizeof(char));
			dev_dbg(i2c->dev, "start tx dma (dma_buf 1 byte)\n");

			/* Enable Tx DMA request */
			i2c_slave_writel(
				i2c_slave_readl(i2c->regs + I2C_CTL) | BIT(17),
				i2c->regs+I2C_CTL
			);
			ret = tcc_i2c_slave_dma_dma_engine_submit(i2c, tcc_i2c_slave_tx_dma_callback, i2c,
				1, 0);
			if(ret != 0) {
				tcc_i2c_slave_stop_dma_engine(i2c, 0);

				spin_lock_irqsave(&i2c->lock, flags);
				i2c->is_tx_dma_running = 0;
				spin_unlock_irqrestore(&i2c->lock, flags);

				spin_lock_irqsave(&i2c->tx_buf.lock, flags);
				i2c->tx_buf.circ.head = 0;
				i2c->tx_buf.circ.tail = i2c->tx_buf.circ.head;
				spin_unlock_irqrestore(&i2c->tx_buf.lock, flags);

				dev_err(i2c->dev, "failed to submit tx dma-engine descriptor\n");
			}
		}
	}

	return count;
}

static ssize_t tcc_i2c_slave_fifo_byte_show(struct device *dev, struct device_attribute *attr,
										char *buf)
{
	struct tcc_i2c_slave *i2c;
	int ret;
	char val;

	dev_dbg(dev, "%s\n", __func__);

	i2c = dev_get_drvdata(dev);

	if(IS_ERR(i2c)) {
		printk(KERN_ERR "%s null pointer err\n", __func__);
		return 0;
	}

	dev_info(dev, "rx fifo cnt 0x%x\n", tcc_i2c_slave_rx_fifo_cnt(i2c));
	
	ret = tcc_i2c_slave_pop_one_byte(i2c, &val, 1);
	if(ret != 0) {
		return sprintf(buf, "no rx data\n");
	}

	return sprintf(buf, "0x%02x\n", val);
}

DEVICE_ATTR(fifo_byte, S_IRUSR | S_IWUSR, tcc_i2c_slave_fifo_byte_show, tcc_i2c_slave_fifo_byte_store);

static int tcc_i2c_slave_init_buf(struct tcc_i2c_slave *i2c, int dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;
	struct tcc_i2c_slave_buf *buf;

	dev = i2c->dev;

	v_addr = dma_alloc_writecombine(dev, i2c->buf_size, &dma_addr, GFP_KERNEL);
	//v_addr = dma_alloc_coherent(dev, tccspi->dma_buf_size, &dma_addr, GFP_KERNEL);
	if(!v_addr) {
		dev_err(i2c->dev, "Fail to allocate the dma buffer (%s)\n",
			(dma_to_mem != 0) ? "rx" : "tx");
		return -ENOMEM;
	}

	if(dma_to_mem) {
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

	memset(buf->v_addr, 0, buf->size);

	dev_dbg(i2c->dev,"%s dma_to_mem: %d v_addr: 0x%08X dma_addr: 0x%08X size: %d\n",
		__func__, dma_to_mem, (unsigned int)v_addr, dma_addr, i2c->buf_size);

	return 0;
}

static void tcc_i2c_slave_deinit_buf(struct tcc_i2c_slave *i2c, int dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;
	struct tcc_i2c_slave_buf *buf;

	dev = i2c->dev;

	if(dma_to_mem) {
		buf = &i2c->rx_buf;
	} else {
		buf = &i2c->tx_buf;
	}

	v_addr = buf->v_addr;
	dma_addr = buf->dma_addr;
	buf->v_addr = (void *)NULL;
	buf->dma_addr = 0;
	buf->circ.buf = (char *)NULL;

	dma_free_writecombine(i2c->dev, i2c->buf_size, v_addr, dma_addr);
	//dma_free_coherent(tccspi->dev, tccspi->dma_buf_size, v_addr, dma_addr);

	dev_dbg(i2c->dev,"%s dma_to_mem: %d v_addr: 0x%08X dma_addr: 0x%08X size: %d\n",
		__func__, dma_to_mem, (unsigned int)v_addr, dma_addr, i2c->buf_size);

}

static irqreturn_t tcc_i2c_slave_isr(int irq, void *arg)
{
	struct tcc_i2c_slave *i2c = (struct tcc_i2c_slave *)arg;
	unsigned int status = 0, intr_mask;
	unsigned int mbfr = 0, mbft = 0, mbf = 0;
	unsigned int data, i;
	char val, cnt;
	int ret;

	status = i2c_slave_readl(i2c->regs + I2C_INT);
	intr_mask = status & 0xFFF;
	status = status & (intr_mask << 16);
	mbf = i2c_slave_readl(i2c->regs + I2C_MBF);
	dev_dbg(i2c->dev, "i2c slave status 0x%08x intr_mask 0x%08d mbf 0x%08x\n",
		status, intr_mask, mbf);

	if((status & BIT(25)) != 0) {
		mbft = (mbf >> 16) & 0xFF;
		for(i = 0; i < 8; i++) {
			if((mbft & (0x1 << i)) != 0) {
				/* Notify read request and read data */
				val = i;
				if(i < 4) {
					data = i2c_slave_readl(i2c->regs + I2C_MB0);
					data = ((data >> (i * 8)) & 0xFF);
				} else {
					data = i2c_slave_readl(i2c->regs + I2C_MB1);
					data = ((data >> ((i-4) * 8)) & 0xFF);
				}
				val = (char)(data & 0xFF);
				dev_dbg(i2c->dev, "Data buffer has been read by master (s_addr 0x%02x val 0x%02x)\n", 
					i, data);
			}
		}
	}

	if((status & BIT(24)) != 0) {
		mbfr = mbf & 0xFF;
		for(i = 0; i < 8; i++) {
			if((mbfr & (0x1 << i)) != 0) {
				/* Notify write request and written data */
				val = i;
				if(i < 4) {
					data = i2c_slave_readl(i2c->regs + I2C_MB0);
					data = ((data >> (i * 8)) & 0xFF);
				} else {
					data = i2c_slave_readl(i2c->regs + I2C_MB1);
					data = ((data >> ((i-4) * 8)) & 0xFF);
				}
				val = (char)(data & 0xFF);
				dev_dbg(i2c->dev, "Data buffer has been written by master (s_addr 0x%02x val 0x%02x)\n", 
					i, data);
			}
		} 
	}
	
	if(mbf != 0) {
		i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_MB0), i2c->regs + I2C_MB0);
		i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_MB1), i2c->regs + I2C_MB1);
	}
	
	if((status & BIT(16)) != 0) {
		unsigned int stat, circ_q_cnt;
		cnt = 0;
		while(1) {
			val = i2c_slave_readb(i2c->regs + I2C_DPORT);
			val = val & 0xFF;
			cnt++;
			
			ret = tcc_i2c_slave_push_one_byte(i2c, val, 1);
			if(ret != 0) {
				dev_err(i2c->dev, "Rx buffer is full\n");
			}

			stat = i2c_slave_readl(i2c->regs + I2C_INT);
			if((stat & BIT(16)) == 0) {
				break;
			}
		}
		dev_dbg(i2c->dev, "Rx FIFO level interrupt, status 0x%08x, get %d\n", status, cnt);

		circ_q_cnt = circ_cnt(&(i2c->rx_buf.circ), i2c->buf_size);
		if(circ_q_cnt >= i2c->poll_count) {
			wake_up(&i2c->wait_q);
			dev_dbg(i2c->dev, "Rx count(%d) is over poll count(%d) - wakeup\n",
				circ_q_cnt, i2c->poll_count);
		}
	}

	if((status & BIT(17)) != 0) {
		dev_dbg(i2c->dev, "Tx FIFO level interrupt, status 0x%08x\n", status);
		cnt = tcc_i2c_slave_tx_fifo_cnt(i2c);
		while(1) {
			ret = tcc_i2c_slave_pop_one_byte(i2c, &val, 0);
			if(ret != 0) {
				/* disable interrupt */
				i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_INT) & ~(BIT(1)), i2c->regs + I2C_INT);
				break;
			} else {
				i2c_slave_writeb((val & 0xFF), i2c->regs + I2C_DPORT);
				cnt++;
			}
			if(cnt >= TCC_I2C_SLV_MAX_FIFO_CNT) {
				break;
			}
		}
	}

	return IRQ_HANDLED;
}

/* Set i2c port configuration */
#ifdef TCC_USE_GFB_PORT
static int tcc_i2c_slave_set_port(struct tcc_i2c_slave *i2c)
{
	u32 port_value;
	u32 pcfg_value, pcfg_offset, pcfg_shift;

	if(i2c->id >= 4)
	{
		BUG();
		return -EINVAL;
	}

	if(i2c->id < 4)
		pcfg_offset = I2C_PORT_CFG4;
	else
		pcfg_offset = I2C_PORT_CFG5;

	pcfg_shift = (i2c->id % 2) * 16;
	port_value = (i2c->port_mux[0] | (i2c->port_mux[1] << 8)); /* [0]SCL [1]SDA */

	pcfg_value  = i2c_slave_readl(i2c->port_cfg+pcfg_offset);
	pcfg_value &= ~(0xFFFF << pcfg_shift);
	pcfg_value |= (0xFFFF & port_value) << pcfg_shift;
	i2c_slave_writel(pcfg_value, i2c->port_cfg + pcfg_offset);

	pcfg_value = i2c_slave_readl(i2c->port_cfg+pcfg_offset);
	//printk("\x1b[1;33m[%s:%d][GFB]  SCL: 0x%X SDA: 0x%X pcfg@0x%X = %x\x1b[0m\n", __func__, __LINE__,
	//	i2c->port_mux[0], i2c->port_mux[1], (u32)i2c->port_cfg + pcfg_offset, pcfg_value);

	return 0;
}
#else
static int tcc_i2c_slave_set_port(struct tcc_i2c_slave *i2c)
{
	if (i2c->port_mux != 0xFF) {
		int i;
		unsigned port_value;
		// Check Master port /
		port_value = i2c_slave_readl(i2c->port_cfg+I2C_PORT_CFG0);
		for (i=0 ; i<4 ; i++) {
			if ((i2c->port_mux&0xFF) == ((port_value>>(i*8))&0xFF))
				port_value |= (0xFF<<(i*8));
		}
		i2c_slave_writel(port_value, i2c->port_cfg+I2C_PORT_CFG0);

		// Check Slave port /
		port_value = i2c_slave_readl(i2c->port_cfg+I2C_PORT_CFG1);
		for (i=0 ; i<4 ; i++) {
			if ((i2c->port_mux&0xFF) == ((port_value>>(i*8))&0xFF))
				port_value |= (0xFF<<(i*8));
		}
		i2c_slave_writel(port_value, i2c->port_cfg+I2C_PORT_CFG1);

		// Dummy(?) Port /
		i2c_slave_writel(0xFFFFFFFF, i2c->port_cfg+I2C_PORT_CFG0+0x8);

		if (i2c->id < 4){
			i2c_slave_writel((i2c_slave_readl(i2c->port_cfg+I2C_PORT_CFG1)& ~(0xFF<<(i2c->id*8)))
					| ((i2c->port_mux&0xFF)<<(i2c->id*8)), i2c->port_cfg+I2C_PORT_CFG1);
		}
		else
		{
			BUG();
			return -EINVAL;
		}
		return 0;
	}
	return -EINVAL;
}
#endif

/* Initialize i2c slave */
static int tcc_i2c_slave_hwinit(struct tcc_i2c_slave *i2c)
{
	/*
	 * Enable i2c, i2c spec. protocol and FIFO access
	 * Disable clock stretching (BIT(4) 0: enable 1: disable)
	 */
	i2c_slave_writel((BIT(0) | BIT(1) | BIT(4)), i2c->regs + I2C_CTL);
	i2c_slave_writel(((i2c->addr & 0x7F) << 1), i2c->regs + I2C_ADDR);

	/* Enable data buffer interrupt */
	i2c_slave_writel((BIT(8) | BIT(9)), i2c->regs + I2C_INT);
	return 0;
}

static int tcc_i2c_slave_open(struct inode *inode, struct file *filp)
{
	unsigned int minor = 0;
	int ret = 0;
	unsigned long flags = 0;
	struct tcc_i2c_slave *i2c;

	if(filp->private_data == NULL) {
		minor = iminor(inode);
		if(minor < TCC_I2C_SLV_MAX_NUM) {
			filp->private_data = &i2c_slave_priv[minor];
		} else {
			printk(KERN_ERR "%s wrong minor number %d\n", __func__, minor);
			return -EINVAL;
		}
	}

	i2c = filp->private_data;

	spin_lock_irqsave(&i2c->lock, flags);

	if(i2c->open_cnt != 0) {
		spin_unlock_irqrestore(&i2c->lock, flags);
		return -EBUSY;
	}

	if(!i2c->read_buf) {
		i2c->read_buf = kmalloc(i2c->rw_buf_size, GFP_KERNEL);
		if (!i2c->read_buf) {
			dev_err(i2c->dev, "failed to alloc read buf mem\n");
			spin_unlock_irqrestore(&i2c->lock, flags);
			ret = -ENOMEM;
			goto memerr;
		}
	}

	if(!i2c->write_buf) {
		i2c->write_buf = kmalloc(i2c->rw_buf_size, GFP_KERNEL);
		if (!i2c->write_buf) {
			dev_err(i2c->dev, "failed to alloc write buf mem\n");
			spin_unlock_irqrestore(&i2c->lock, flags);
			ret = -ENOMEM;
			goto memerr;
		}
	}
	dev_dbg(i2c->dev, "success alloc read/write buf mem (%d)\n",
		i2c->rw_buf_size);
	
	/* set port mux */
	ret = tcc_i2c_slave_set_port(i2c);
	if(ret != 0) {
		dev_err(i2c->dev, "failed to create attr\n");
		spin_unlock_irqrestore(&i2c->lock, flags);
		ret = -ENXIO;
		goto memerr;
	}

	if(i2c->hclk != 0) {
		if(clk_prepare_enable(i2c->hclk) != 0) {
			dev_err(i2c->dev, "can't do i2c slave hclk clock enable\n");
			spin_unlock_irqrestore(&i2c->lock, flags);
			ret = -ENXIO;
			goto memerr;
		}
	} else {
		dev_err(i2c->dev, "failed to enable i2c slave clock\n");
		spin_unlock_irqrestore(&i2c->lock, flags);
		ret = -ENXIO;
		goto memerr;
	}

	ret = request_irq(i2c->irq, tcc_i2c_slave_isr, IRQF_SHARED, dev_name(i2c->dev), i2c);
	if(ret) {
		dev_err(i2c->dev, "failed to request irq %i\n", i2c->irq);
		spin_unlock_irqrestore(&i2c->lock, flags);
		goto clkerr;
	}

	i2c->open_cnt++;

    init_waitqueue_head(&(i2c->wait_q));

	/* Initialize circular buffer */
	i2c->tx_buf.circ.head = 0;
	i2c->tx_buf.circ.tail = i2c->tx_buf.circ.head;
	i2c->rx_buf.circ.head = 0;
	i2c->rx_buf.circ.tail = i2c->rx_buf.circ.head;

	i2c->is_tx_dma_running = 0;

	dev_dbg(i2c->dev, "%s\n", __func__);

	spin_unlock_irqrestore(&i2c->lock, flags);

	return ret;

clkerr:
	if(i2c->hclk != 0) {
		clk_disable(i2c->hclk);
	}
memerr:

	if(i2c->read_buf) {
		kfree(i2c->read_buf);
	}
	if(i2c->write_buf) {
		kfree(i2c->write_buf);
	}

	i2c->read_buf = NULL;
	i2c->write_buf = NULL;

	return ret;
}

static int tcc_i2c_slave_release(struct inode *inode, struct file *filp)
{
	struct tcc_i2c_slave *i2c;
	unsigned long flags = 0;

	if(filp->private_data == NULL) {
		return 0;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	dev_dbg(i2c->dev, "%s\n", __func__);

	spin_lock_irqsave(&i2c->lock, flags);

	/* Disable i2c slave core */
	i2c_slave_writel(0x0, i2c->regs + I2C_CTL);

	/* Clear and disable interrupt */
	i2c_slave_writel(i2c_slave_readl(i2c->regs + I2C_INT), i2c->regs + I2C_INT);
	i2c_slave_writel(0x0, i2c->regs + I2C_INT);

	i2c->addr = 0xff;
	i2c_slave_writel(i2c->addr, i2c->regs + I2C_ADDR);

	if(i2c->hclk != 0) {
		clk_disable(i2c->hclk);
	}

	free_irq(i2c->irq, i2c);

	if(i2c->read_buf) {
		kfree(i2c->read_buf);
	}
	if(i2c->write_buf) {
		kfree(i2c->write_buf);
	}

	i2c->read_buf = NULL;
	i2c->write_buf = NULL;
	i2c->open_cnt--;

	spin_unlock_irqrestore(&i2c->lock, flags);

	tcc_i2c_slave_stop_dma_engine(i2c, 1);
	tcc_i2c_slave_stop_dma_engine(i2c, 0);

	filp->private_data = NULL;

	return 0;
}

static ssize_t tcc_i2c_slave_read(struct file * filp, char *buf, size_t sz, loff_t *ppos)
{
	struct tcc_i2c_slave *i2c;
	int err;
	size_t	_sz, offset, rd_sz;

	if(filp->private_data == NULL) {
		printk(KERN_ERR "%s priv data is invalid\n", __func__);
		return -EFAULT;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	_sz = sz;
	offset = 0;
	while(1) {
		rd_sz = (_sz > i2c->rw_buf_size) ? i2c->rw_buf_size : _sz;
		rd_sz = tcc_i2c_slave_pop(i2c, i2c->read_buf, rd_sz, 1);
		if(rd_sz != 0) {
			err = copy_to_user(buf + offset, i2c->read_buf, rd_sz);
			if(err != 0) {
				dev_err(i2c->dev, "failed to copy_to_user\n");
				return err;
			}
		} else {
			break;
		}
		offset += rd_sz;
		_sz -= rd_sz;

		if(_sz == 0)
			break;
	}

	return offset;
}

static ssize_t tcc_i2c_slave_write(struct file *filp, const char *buf, size_t sz, loff_t *ppos)
{
	struct tcc_i2c_slave *i2c;
	ssize_t ret = 0;
	int status, err;
	char cnt, val;
	unsigned long flags;
	size_t	_sz, offset, wr_sz;

	if(filp->private_data == NULL) {
		printk(KERN_ERR "%s priv data is invalid\n", __func__);
		return 0;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	_sz = sz;
	offset = 0;
	while(1) {
		wr_sz = (_sz > i2c->rw_buf_size) ? i2c->rw_buf_size : _sz;
		err = copy_from_user(i2c->write_buf, buf + offset, wr_sz);
		if(err != 0) {
			dev_err(i2c->dev, "failed to copy_to_user\n");
			return err;
		}
		wr_sz = tcc_i2c_slave_push(i2c, i2c->write_buf, wr_sz, 0);
		if(wr_sz == 0) {
			break;
		}
		offset += wr_sz;
		_sz -= wr_sz;

		if(_sz == 0)
			break;
	}

	if(offset == 0)
		return offset;

	dev_dbg(i2c->dev, "succeed to push data(%d)", offset);

	if(i2c->dma.chan_tx == NULL) {
		/* PIO mode */
		ret = offset;

		/* Check whether tx fifo level interrupt */
		status = i2c_slave_readl(i2c->regs + I2C_INT);
		if((status & BIT(1)) == 0) {
			/* Until fifo is full, dequeue and write data at fifo */
			while(1) {
				cnt = tcc_i2c_slave_tx_fifo_cnt(i2c);
				if(cnt >= TCC_I2C_SLV_MAX_FIFO_CNT) {
					break;
				}
				err = tcc_i2c_slave_pop_one_byte(i2c, &val, 0);
				if(err != 0) {
					break;
				} else {
					i2c_slave_writeb(val, i2c->regs + I2C_DPORT);
				}
			}

			/* Enable interrupt */
			status = i2c_slave_readl(i2c->regs + I2C_INT);
			status &= 0x0FFF;
			status |= BIT(1);
			i2c_slave_writel(status, i2c->regs + I2C_INT);
		}
		dev_dbg(i2c->dev, "tx fifo cnt 0x%x\n", tcc_i2c_slave_tx_fifo_cnt(i2c));
	}else {
		/* DMA mode */
		spin_lock_irqsave(&i2c->lock, flags);
		if(i2c->is_tx_dma_running == 0) {
			unsigned int copy_size;

			i2c->is_tx_dma_running = 1;
			spin_unlock_irqrestore(&i2c->lock, flags);

			copy_size = tcc_i2c_slave_pop(i2c, i2c->dma.tx_v_addr, i2c->dma.size, 0);
			if(copy_size == 0) {
				dev_err(i2c->dev, "failed to pop data from tx fifo\n");
				return copy_size;
			}
			dev_dbg(i2c->dev, "start tx dma (size %d)\n", copy_size);

			/* Enable Tx DMA request */
			i2c_slave_writel(
				i2c_slave_readl(i2c->regs + I2C_CTL) | BIT(17),
				i2c->regs+I2C_CTL
			);
			ret = tcc_i2c_slave_dma_dma_engine_submit(i2c, tcc_i2c_slave_tx_dma_callback, i2c,
				copy_size, 0);
			if(ret != 0) {
				tcc_i2c_slave_stop_dma_engine(i2c, 0);
				ret = 0;

				spin_lock_irqsave(&i2c->lock, flags);
				i2c->is_tx_dma_running = 0;
				spin_unlock_irqrestore(&i2c->lock, flags);

				spin_lock_irqsave(&i2c->tx_buf.lock, flags);
				i2c->tx_buf.circ.head = 0;
				i2c->tx_buf.circ.tail = i2c->tx_buf.circ.head;
				spin_unlock_irqrestore(&i2c->tx_buf.lock, flags);

				dev_err(i2c->dev, "failed to submit tx dma-engine descriptor\n");
			} else {
				ret = offset;
			}
		}
	}

	return ret;
}

static long tcc_i2c_slave_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tcc_i2c_slave *i2c;
	unsigned char addr, val[2];
	unsigned long flags = 0;
	unsigned int temp, count;
	int ret = 0;

	if(filp->private_data == NULL) {
		printk(KERN_ERR "%s priv data is invalid\n", __func__);
		return -EFAULT;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	switch(cmd) {
	case IOCTL_I2C_SLAVE_SET_ADDR:
		/* Set address */
		if(copy_from_user(&addr, (void *)arg, sizeof(addr)) != 0) {
			ret = -EFAULT;
			break;
		}

		/* set i2c slave address */
		if(addr > 0x7F) {
			dev_err(i2c->dev, "%s slave address is invalid addr 0x%02X\n", __func__, (unsigned int)addr);
			ret =  -EINVAL;
			break;
		}

		spin_lock_irqsave(&i2c->lock, flags);

		/* set address */
		i2c->addr = addr;

		ret = tcc_i2c_slave_hwinit(i2c);
		if (ret != 0) {
			dev_err(i2c->dev, "failed to enable i2c slave core err %d\n",
				ret);
			ret = -EFAULT;
			break;
		}

		ret = i2c_slave_readl(i2c->regs + I2C_ADDR);
		ret = (ret >> 1) & 0x7F;

		spin_unlock_irqrestore(&i2c->lock, flags);

		dev_dbg(i2c->dev, "%s set slave addr 0x%x\n", __func__, i2c->addr);
		break;
	case IOCTL_I2C_SLAVE_GET_ADDR:
		/* Get address */
		dev_dbg(i2c->dev, "%s get slave addr 0x%x\n", __func__, i2c->addr);

		spin_lock_irqsave(&i2c->lock, flags);

		ret = i2c_slave_readl(i2c->regs + I2C_ADDR);
		ret = (ret >> 1) & 0x7F;

		spin_unlock_irqrestore(&i2c->lock, flags);

		break;
	case IOCTL_I2C_SLAVE_SET_MB:
		/* Set memory buffers (MB0/MB1), sub-address 0x00 - 0x07 */
		memset(val, 0xFF, sizeof(unsigned char) * 2);
		if(copy_from_user(val, (void *)arg, sizeof(unsigned char) * 2)) {
			ret = -EFAULT;
			dev_err(i2c->dev, "%s:%d failed to copy mem from user\n", __func__, __LINE__);
			break;
		}

		if(val[0] > 0x07) {
			ret = -EINVAL;
			dev_err(i2c->dev, "%s:%d wrong sub address 0x%02x\n", __func__, __LINE__, val[0]);
			break;
		}

		addr = val[0];
		dev_dbg(i2c->dev, "%s set 0x%02x at 0x%02x\n", __func__, addr, val[1]);

		spin_lock_irqsave(&i2c->lock, flags);

		if(addr < 0x4) {
			temp = i2c_slave_readl(i2c->regs + I2C_MB0);
			temp = temp & ~(0xFF << (addr * 8));
			temp = temp | ((val[1] & 0xFF) << (addr * 8));
			i2c_slave_writel(temp, i2c->regs + I2C_MB0);
		} else {
			temp = i2c_slave_readl(i2c->regs + I2C_MB1);
			temp = temp & ~(0xFF << ((addr - 4) * 8));
			temp = temp | ((val[1] & 0xFF) << ((addr - 4) * 8));
			i2c_slave_writel(temp, i2c->regs + I2C_MB1);
		}
		
		spin_unlock_irqrestore(&i2c->lock, flags);

		ret = 0;
		break;
		
	case IOCTL_I2C_SLAVE_GET_MB:
		/* Get memory buffers (MB0/MB1), sub-address 0x00 - 0x07 */
		if(copy_from_user(&addr, (void *)arg, sizeof(addr)) != 0) {
			ret = -EFAULT;
			dev_err(i2c->dev, "%s:%d failed to copy mem from user\n", __func__, __LINE__);
			break;
		}

		if(addr > 0x07) {
			ret = -EINVAL;
			dev_err(i2c->dev, "%s:%d wrong sub address 0x%02x\n", __func__, __LINE__, addr);
			break;
		}

		spin_lock_irqsave(&i2c->lock, flags);

		if(addr < 0x4) {
			ret = i2c_slave_readl(i2c->regs + I2C_MB0);
			ret = ((ret >> (addr * 8)) & 0xFF);
		} else {
			ret = i2c_slave_readl(i2c->regs + I2C_MB1);
			ret = ((ret >> ((addr - 4) * 8)) & 0xFF);
		}

		spin_unlock_irqrestore(&i2c->lock, flags);

		dev_dbg(i2c->dev, "%s get sub address 0x%02x = 0x%02x\n", __func__, addr, ret);
		break;
	case IOCTL_I2C_SLAVE_SET_POLL_CNT:
		/* Get memory buffers (MB0/MB1), sub-address 0x00 - 0x07 */
		if(copy_from_user(&count, (void *)arg, sizeof(count)) != 0) {
			ret = -EFAULT;
			dev_err(i2c->dev, "%s:%d failed to copy mem from user\n", __func__, __LINE__);
			break;
		}

		if(count > i2c->buf_size) {
			i2c->poll_count = i2c->buf_size;
			dev_dbg(i2c->dev, "poll count(%d) is bigger than queue size(%d), poll count %d\n",
				count, i2c->buf_size, i2c->poll_count);
		} else {
			i2c->poll_count = count;
		}

		/* Set interrupt */
		if(i2c->dma.chan_rx != 0) {
			/* DMA mode - disable Rx FIFO level interrupt */
			if(i2c->poll_count > i2c->dma.size) {
				i2c->poll_count = i2c->dma.size;
			}

			/* Enable Rx DMA request */
			i2c_slave_writel(
				i2c_slave_readl(i2c->regs + I2C_CTL) | BIT(16),
				i2c->regs+I2C_CTL
			);
			ret = tcc_i2c_slave_dma_dma_engine_submit(i2c, tcc_i2c_slave_rx_dma_callback, i2c,
				i2c->poll_count, 1);
			if(ret != 0) {
				dev_err(i2c->dev, "failed to submit rx dma-engine descriptor\n");
			}
		} else {
			/* PIO mode - enable Rx FIFO level interrupt */
			i2c_slave_writel((i2c_slave_readl(i2c->regs + I2C_INT) | BIT(0)), i2c->regs + I2C_INT);
			ret = 0;
		}
		dev_dbg(i2c->dev, "%s set poll count %d\n", __func__, i2c->poll_count);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int tcc_i2c_slave_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct tcc_i2c_slave *i2c;
	unsigned int circ_q_cnt;
	unsigned long flags;

	if(filp->private_data == NULL) {
		printk(KERN_ERR "%s priv data is invalid\n", __func__);
		return -EFAULT;
	}

	i2c = (struct tcc_i2c_slave *)filp->private_data;

	spin_lock_irqsave(&i2c->rx_buf.lock, flags);
	circ_q_cnt = circ_cnt(&(i2c->rx_buf.circ), i2c->buf_size);
	spin_unlock_irqrestore(&i2c->rx_buf.lock, flags);
	if(circ_q_cnt >= i2c->poll_count) {
		dev_dbg(i2c->dev, "Rx count(%d) is over poll count(%d)\n",
			circ_q_cnt, i2c->poll_count);
		return (POLL_IN | POLLRDNORM);
	}

    poll_wait(filp, &(i2c->wait_q), wait);

	spin_lock_irqsave(&i2c->rx_buf.lock, flags);
	circ_q_cnt = circ_cnt(&(i2c->rx_buf.circ), i2c->buf_size);
	spin_unlock_irqrestore(&i2c->rx_buf.lock, flags);
	if(circ_q_cnt >= i2c->poll_count) {
		dev_dbg(i2c->dev, "Rx count(%d) is over poll count(%d)\n",
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
static int tcc_i2c_slave_parse_dt(struct device_node *np, struct tcc_i2c_slave *i2c)
{
	int ret = 0;
#ifdef TCC_USE_GFB_PORT
	/* Port array order : [0]SCL [1]SDA */
	ret = of_property_read_u32_array(np, "port-mux", i2c->port_mux,
		(size_t)of_property_count_elems_of_size(np, "port-mux", sizeof(u32)));

	//printk("\x1b[1;33mSCL: %d SDA: %d\x1b[0m\n", i2c->port_mux[0], i2c->port_mux[1]);
#else
	ret = of_property_read_u32(np, "port-mux", &i2c->port_mux);
#endif
	if(ret != 0) {
		dev_err(i2c->dev, "failed to get port-mux\n");
		return ret;
	}
	return 0;
}
#else
static int tcc_i2c_slave_parse_dt(struct device_node *np, struct tcc_i2c_slave *i2c)
{
	return -ENXIO;
}
#endif

static int tcc_i2c_slave_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int id;
	struct tcc_i2c_slave *i2c;
	struct resource *res;
	struct device *dev;
	void __iomem *byte_cmd;
	u32 byte_cmd_val = 0;

	if (!pdev->dev.of_node)
		return -EINVAL;

	ret = of_property_read_u32(pdev->dev.of_node, "id", &id);
	if(ret != 0) {
		dev_err(&pdev->dev, "failed to get id\n");
		return ret;
	}

	if(id >= TCC_I2C_SLV_MAX_NUM) {
		dev_err(&pdev->dev, "wrong id %d\n", id);
		return -EINVAL;
	}

	i2c = &i2c_slave_priv[id];
	memset(i2c, 0, sizeof(i2c));
	i2c->id = id;

	/* get base register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get base address\n");
		return -ENODEV;
	}

	i2c->phy_regs = res->start;
	dev_dbg(&pdev->dev, "base 0x%08x\n", i2c->phy_regs);

	i2c->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->regs)) {
		ret = PTR_ERR(i2c->regs);
		dev_err(&pdev->dev, "failed to remap base address\n");
		return ret;
	}

	/* get i2c port config register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "failed to get port config address\n");
		return -ENODEV;
	}

	dev_dbg(&pdev->dev, "port 0x%08x\n", res->start);

	i2c->port_cfg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->port_cfg)) {
		ret = PTR_ERR(i2c->port_cfg);
		dev_err(&pdev->dev, "failed to remap port config address\n");
		return ret;
	}

	/* get port config info */
	ret = tcc_i2c_slave_parse_dt(pdev->dev.of_node, i2c);
	if(ret != 0) {
		dev_err(&pdev->dev, "failed to get port-mux and id\n");
		return ret;
	}

	/* get byte command register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res) {
		dev_info(&pdev->dev, "set memory bus byte command masking register\n");
		dev_dbg(&pdev->dev, "byte command register 0x%08x\n", res->start);
		byte_cmd = devm_ioremap_resource(&pdev->dev, res);
		if (i2c_slave_readl(byte_cmd) == 0xFFFF0000) {
			byte_cmd_val = 0xFFF00000 | ((i2c->phy_regs >> 16) & 0x0000FFF0);
			dev_dbg(&pdev->dev, "byte command write value = 0x%08x\n", byte_cmd_val);
			i2c_slave_writel(byte_cmd_val, byte_cmd);
		}
		dev_dbg(&pdev->dev, "byte command read value  = 0x%08x\n", i2c_slave_readl(byte_cmd));
	}

	/* get i2c irq number */
	i2c->irq = platform_get_irq(pdev, 0);
	if (i2c->irq < 0) {
		dev_err(&pdev->dev, "failed to get irq number\n");
		return i2c->irq;
	}
	dev_dbg(&pdev->dev, "irq 0x%08x\n", i2c->irq);

	/* get and set iobus clk */
	i2c->hclk = of_clk_get(pdev->dev.of_node, 0);
	if(i2c->hclk == NULL) {
		dev_err(&pdev->dev, "failed to get clock info\n");
		return -ENXIO;
	}

	i2c->dev = &pdev->dev;
	i2c->addr = 0xFF;
	i2c->open_cnt = 0;
	i2c->buf_size = TCC_I2C_SLV_DMA_BUF_SIZE;
	i2c->poll_count = TCC_I2C_SLV_DEF_POLL_CNT;
	i2c->rw_buf_size = TCC_I2C_SLV_WR_BUF_SIZE;
	i2c->write_buf = NULL;
	i2c->read_buf = NULL;

	/* Allocate buffer */
	ret = tcc_i2c_slave_init_buf(i2c, 1);
	if(ret != 0) {
		dev_err(&pdev->dev,
			"Failed to allocate rx dma buffer\n");
		goto err_clk;
	}
	ret = tcc_i2c_slave_init_buf(i2c, 0);
	if(ret != 0) {
		dev_err(&pdev->dev,
			"Failed to allocate tx dma buffer\n");
		goto err_mem_rx;
	}

	/* Disable i2c slave core */
	i2c_slave_writel(0x0, i2c->regs + I2C_CTL);

    dev = device_create(tcc_i2c_slave_class, NULL, MKDEV(tcc_i2c_slave_major, i2c->id),
		NULL, TCC_I2C_SLV_DEV_NAMES, i2c->id);
	if(IS_ERR(dev)) {
		dev_err(&pdev->dev, "failed to create device\n");
		ret = PTR_ERR(dev);
		goto err_mem_tx;
	}

	spin_lock_init(&i2c->lock);
	platform_set_drvdata(pdev, i2c);

	ret = device_create_file(&pdev->dev, &dev_attr_fifo_byte);
	if(ret != 0) {
		dev_err(&pdev->dev, "failed to create attr (fifo byte)\n");
		goto err_dev;
	}

	ret = tcc_i2c_slave_dma_engine_probe(pdev, i2c);
	if(ret != 0) {
		dev_dbg(&pdev->dev, "i2c slave ch %d - TX/RX PIO mode\n", i2c->id);
	}

	if(i2c->dma.chan_tx != NULL) {
		dev_info(&pdev->dev, "i2c slave ch %d - Tx DMA mode(%s %p)\n", i2c->id,
			dma_chan_name(i2c->dma.chan_tx), i2c->dma.chan_tx);
	} else {
		dev_info(&pdev->dev, "i2c slave ch %d - Tx PIO mode\n", i2c->id);
	}

	if(i2c->dma.chan_rx != NULL) {
		dev_info(&pdev->dev, "i2c slave ch %d - Rx DMA mode(%s %p)\n", i2c->id,
			dma_chan_name(i2c->dma.chan_rx), i2c->dma.chan_rx);
	} else {
		dev_info(&pdev->dev, "i2c slave ch %d - Rx PIO mode\n", i2c->id);
	}

	dev_dbg(&pdev->dev, "i2c debugging log is enabled\n");
#ifdef TCC_USE_GFB_PORT
	dev_info(&pdev->dev, "i2c slave ch %d @0x%08x gfb(scl %d sda %d)\n",
		i2c->id, (unsigned int)i2c->regs, i2c->port_mux[0], i2c->port_mux[1]);
#else
	dev_info(&pdev->dev, "i2c slave ch %d @0x%08x port %d\n",
		i2c->id, (unsigned int)i2c->regs, i2c->port_mux);
#endif

	return 0;

err_dev:
	device_destroy(tcc_i2c_slave_class, MKDEV(tcc_i2c_slave_major, i2c->id));
err_mem_tx:
	tcc_i2c_slave_deinit_buf(i2c, 0);
err_mem_rx:
	tcc_i2c_slave_deinit_buf(i2c, 1);
err_clk:
	if(i2c->hclk) {
		clk_disable(i2c->hclk);
		clk_put(i2c->hclk);
		i2c->hclk = NULL;
	}
	return ret;

}
static int tcc_i2c_slave_remove(struct platform_device *pdev)
{
	struct tcc_i2c_slave *i2c = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	device_remove_file(&pdev->dev, &dev_attr_fifo_byte);
	device_destroy(tcc_i2c_slave_class, MKDEV(tcc_i2c_slave_major, i2c->id));

	tcc_i2c_slave_deinit_buf(i2c, 0);
	tcc_i2c_slave_deinit_buf(i2c, 1);

	tcc_i2c_slave_release_dma_engine(i2c);

	/* I2C bus & clock disable */
	if (i2c->hclk) {
		clk_disable(i2c->hclk);
		clk_put(i2c->hclk);
		i2c->hclk = NULL;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_i2c_slave_suspend(struct device *dev)
{
	struct tcc_i2c_slave *i2c = dev_get_drvdata(dev);
	spin_lock(&i2c->lock);
	if(i2c->hclk) {
		clk_disable_unprepare(i2c->hclk);
	}
	i2c->is_suspended = true;
	spin_unlock(&i2c->lock);
	return 0;
}

static int tcc_i2c_slave_resume(struct device *dev)
{
	int ret;
	struct tcc_i2c_slave *i2c = dev_get_drvdata(dev);

	spin_lock(&i2c->lock);

	if(i2c->hclk) {
		if(clk_prepare_enable(i2c->hclk) != 0) {
			dev_err(dev, "can't do i2c slave hclk clock enable\n");
			return -ENXIO;
		}
	}
	ret = tcc_i2c_slave_hwinit(i2c);
	if (ret != 0) {
		dev_err(dev, "failed to set clock and port configurations err %d\n",
			ret);
		clk_disable_unprepare(i2c->hclk);
		return ret;
	}
	i2c->is_suspended = false;

	spin_unlock(&i2c->lock);
	return 0;
}
static SIMPLE_DEV_PM_OPS(tcc_i2c_slave_pm, tcc_i2c_slave_suspend, tcc_i2c_slave_resume);
#define TCC_I2C_SLAVE_PM (&tcc_i2c_slave_pm)
#else
#define TCC_I2C_SLAVE_PM (NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_i2c_slave_of_match[] = {
	{ .compatible = "telechips,i2c-slave" },
	{ .compatible = "telechips,tcc803x-i2c-slave" },
	{ .compatible = "telechips,tcc897x-i2c-slave" },
	{ .compatible = "telechips,tcc899x-i2c-slave" },
	{ .compatible = "telechips,tcc901x-i2c-slave" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_i2c_slave_of_match);
#endif

static struct platform_driver tcc_i2c_slave_driver = {
	.probe			= tcc_i2c_slave_probe,
	.remove			= tcc_i2c_slave_remove,
	.driver			= {
		.owner		= THIS_MODULE,
		.name		= "tcc-i2c-slave",
		.pm		= TCC_I2C_SLAVE_PM,
		.of_match_table	= of_match_ptr(tcc_i2c_slave_of_match),
	},
};

static int __init tcc_i2c_slave_init(void)
{
	int ret;

	ret = register_chrdev(0, TCC_I2C_SLV_CHRDEV_NAME, &tcc_i2c_slv_fops);
	if(ret < 0) {
		printk(KERN_ERR "tcc-i2c-slave : failed to register character device ret %d\n", ret);
		return ret;
	}

	tcc_i2c_slave_major = ret;

	tcc_i2c_slave_class = class_create(THIS_MODULE, TCC_I2C_SLV_CHRDEV_CLASS_NAME);
	if(IS_ERR(tcc_i2c_slave_class)) {
		printk(KERN_ERR "tcc-i2c-slave : failed to create class\n");
		ret = PTR_ERR(tcc_i2c_slave_class);
		goto init_err;
	}

	ret = platform_driver_register(&tcc_i2c_slave_driver);
	if(ret != 0) {
		printk(KERN_ERR "tcc-i2c-slave : failed to create class\n");
		goto init_err;
	}

	printk("\x1b[32m[tcc_i2c_slave_init] Complete\n\x1b[0m");

	return ret;
init_err:
	unregister_chrdev(tcc_i2c_slave_major, TCC_I2C_SLV_CHRDEV_NAME);
	
	return ret;
}

static void __exit tcc_i2c_slave_exit(void)
{
	class_destroy(tcc_i2c_slave_class);
	if(tcc_i2c_slave_major != 0) {
		unregister_chrdev(tcc_i2c_slave_major, TCC_I2C_SLV_CHRDEV_NAME);
	}
	platform_driver_unregister(&tcc_i2c_slave_driver);
}

module_init(tcc_i2c_slave_init);
module_exit(tcc_i2c_slave_exit);

MODULE_AUTHOR("Telechips Inc. linux@telechips.com");
MODULE_DESCRIPTION("Telechips H/W I2C slave driver");
MODULE_LICENSE("GPL");
