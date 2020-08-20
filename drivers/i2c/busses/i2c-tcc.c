
/*
 * linux/drivers/i2c/busses/i2c-tcc.c
 *
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips I2C Controller
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ----------------------------
 * for TCC DIBCOM DVB-T module
 * [M] i2c-core.c, i2c-dev.c
 * [M] include/linux/i2c-dev.h, include/linux/i2c.h
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
//#include <linux/of_i2c.h>
#include <linux/of_device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/irq.h>

#define I2C_M_MODE              0x0040
#define I2C_M_WR_RD             0x0080
#define I2C_M_WM899x_CODEC      0x0100
#define I2C_M_TCC_IPOD          0x0200

/* I2C Configuration Reg. */
#define I2C_PRES                0x00
#define I2C_CTRL                0x04
#define I2C_TXR                 0x08
#define I2C_CMD                 0x0C
#define I2C_RXR                 0x10
#define I2C_SR                  0x14
#define I2C_TIME                0x18
#define I2C_TR1                 0x24

/* I2C Port Configuration Reg. */
#define I2C_PORT_CFG0           0x00
#define I2C_PORT_CFG1           0x04
#define I2C_PORT_CFG2           0x08
#define I2C_PORT_CFG4           0x10
#define I2C_PORT_CFG5           0x14

#if defined (CONFIG_ARCH_TCC802X)
#define I2C_IRQ_STS             0x1C
#elif defined(CONFIG_ARCH_TCC803X)
#define I2C_IRQ_STS             0x10
#else
#define I2C_IRQ_STS             0x0C
#endif

#define I2C_DEF_RETRIES         2

#define I2C_ACK_TIMEOUT         50   /* in msec */
#define I2C_CMD_TIMEOUT         500  /* in msec */

#define i2c_readl       __raw_readl
#define i2c_writel      __raw_writel

enum tcc_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};

struct tcc_i2c {
	spinlock_t              lock;
	wait_queue_head_t       wait;
	void __iomem            *regs;
	void __iomem            *port_cfg;
	void __iomem            *share_core;

	int                     core;
	unsigned int            i2c_clk_rate;
	unsigned int            core_clk_rate;
	struct clk              *pclk; /* I2C PERI CLK */
	struct clk              *hclk; /* I2C IO config */
	struct clk              *fclk; /* FBUS IO CLK */
	struct i2c_msg          *msg;
	unsigned int            msg_num;
	unsigned int            msg_idx;
	unsigned int            msg_ptr;

	struct completion       msg_complete;
	enum tcc_i2c_state      state;
	struct device           *dev;
	struct i2c_adapter      adap;
	bool                    is_suspended;
	unsigned int            port_mux[2];

	int                     irq;
	unsigned int            interrupt_mode;
	unsigned int            noise_filter;

	bool                    use_pw;
	unsigned int            pwh;
	unsigned int            pwl;

	const struct tcc_i2c_soc_data *soc_data;
};

static ssize_t tcc_i2c_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t tcc_i2c_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
DEVICE_ATTR(tcci2c, S_IRUGO | S_IWUSR, tcc_i2c_show, tcc_i2c_store);

static struct tcc_i2c_soc_data{
	void (*parse_dt)(struct device_node *np, struct tcc_i2c *i2c);
	int (*init)(struct tcc_i2c *i2c);
	int (*set_port)(struct tcc_i2c *i2c);
	void (*get_port)(struct tcc_i2c *i2c, unsigned int *port);
	void (*reg_dump)(void __iomem *regs, void __iomem *port_cfg);
	int (*set_noise_filter)(struct tcc_i2c *i2c);
};

static void tcc_i2c_reg_dump(void __iomem *regs, void __iomem *port_cfg)
{
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PRES     : 0x%08x\n", i2c_readl(regs + I2C_PRES));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_CTRL     : 0x%08x\n", i2c_readl(regs + I2C_CTRL));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_CMD      : 0x%08x\n", i2c_readl(regs + I2C_CMD));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_SR       : 0x%08x\n", i2c_readl(regs + I2C_SR));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_TIME     : 0x%08x\n", i2c_readl(regs + I2C_TIME));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PORT_CFG0: 0x%08x\n", i2c_readl(port_cfg + I2C_PORT_CFG0));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_IRQ_STS  : 0x%08x\n", i2c_readl(port_cfg + I2C_IRQ_STS));
}

static void tcc803x_i2c_reg_dump(void __iomem *regs, void __iomem *port_cfg)
{
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PRES     : 0x%08x\n", i2c_readl(regs + I2C_PRES));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_CTRL     : 0x%08x\n", i2c_readl(regs + I2C_CTRL));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_CMD      : 0x%08x\n", i2c_readl(regs + I2C_CMD));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_SR       : 0x%08x\n", i2c_readl(regs + I2C_SR));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_TIME     : 0x%08x\n", i2c_readl(regs + I2C_TIME));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_TR1      : 0x%08x\n", i2c_readl(regs + I2C_TR1));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PORT_CFG0: 0x%08x\n", i2c_readl(port_cfg + I2C_PORT_CFG0));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PORT_CFG2: 0x%08x\n", i2c_readl(port_cfg + I2C_PORT_CFG2));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_IRQ_STS  : 0x%08x\n", i2c_readl(port_cfg + I2C_IRQ_STS));
}

static void tcc802x_i2c_reg_dump(void __iomem *regs, void __iomem *port_cfg)
{
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PRES     : 0x%08x\n", i2c_readl(regs + I2C_PRES));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_CTRL     : 0x%08x\n", i2c_readl(regs + I2C_CTRL));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_CMD      : 0x%08x\n", i2c_readl(regs + I2C_CMD));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_SR       : 0x%08x\n", i2c_readl(regs + I2C_SR));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_TIME     : 0x%08x\n", i2c_readl(regs + I2C_TIME));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PORT_CFG0: 0x%08x\n", i2c_readl(port_cfg + I2C_PORT_CFG0));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_PORT_CFG1: 0x%08x\n", i2c_readl(port_cfg + I2C_PORT_CFG1));
	printk(KERN_DEBUG "[DEBUG][I2C] I2C_IRQ_STS  : 0x%08x\n", i2c_readl(port_cfg + I2C_IRQ_STS));
}

static inline void tcc_i2c_enable_irq(struct tcc_i2c *i2c)
{
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) | (1<<6), i2c->regs+I2C_CTRL);
}

static inline void tcc_i2c_disable_irq(struct tcc_i2c *i2c)
{
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1<<6), i2c->regs+I2C_CTRL);
	i2c_writel(i2c_readl(i2c->regs+I2C_CMD) | (1<<0), i2c->regs+I2C_CMD);
}
/* Before disable clock, TCC803x have to check corresponding i2c core.
 * Because peripheral clock of i2c controller 0 ~ 3 is connected with 4 ~ 7.
 */
static int tcc_i2c_check_share_core(struct tcc_i2c *i2c)
{
	int ret;
	if(i2c->share_core != NULL){
		ret = ((i2c_readl(i2c->share_core) >> 7) & 0x1);
	}else{
		ret = 0;
	}
	dev_dbg(i2c->dev, "[DEBUG][I2C] %s: return %d\n", __func__, ret);
	return ret;
}


static int tcc_i2c_set_noise_filter(struct tcc_i2c *i2c)
{
	unsigned int time_reg;
	unsigned int fc_val;
	unsigned long fbus_ioclk_Mhz;

	/* Calculate noise filter counter load value */
	fbus_ioclk_Mhz = (clk_get_rate(i2c->fclk) / 1000000);
	if(fbus_ioclk_Mhz <= 0){
		dev_err(i2c->dev, "[ERROR][I2C] %s: FBUS_IO is lower than 1Mhz, not enough!\n", __func__);
		return -1;
	}
	fc_val = ((i2c->noise_filter * fbus_ioclk_Mhz) / 1000) + 2;

	time_reg = i2c_readl(i2c->regs + I2C_TIME);
	time_reg = (time_reg & (~0x1F)) | (fc_val & 0x1F);
	i2c_writel(time_reg, i2c->regs + I2C_TIME);

	dev_dbg(&i2c->adap.dev,
			"[DEBUG][I2C] <%s> fbus_io clk: %ldHz, noise filter load value %d\n",
			__func__, fbus_ioclk_Mhz, fc_val);
	return 0;
}

static int tcc803x_i2c_set_noise_filter(struct tcc_i2c *i2c)
{
	unsigned int time_reg;
	unsigned int fc_val;
	unsigned long fbus_ioclk_Mhz;

	/* Calculate noise filter counter load value */
	fbus_ioclk_Mhz = (clk_get_rate(i2c->fclk) / 1000000);
	if(fbus_ioclk_Mhz <= 0){
		dev_err(i2c->dev, "[ERROR][I2C] %s: FBUS_IO is lower than 1Mhz, not enough!\n", __func__);
		return -1;
	}
	fc_val = ((i2c->noise_filter * fbus_ioclk_Mhz) / 1000) + 2;

	time_reg = i2c_readl(i2c->regs + I2C_TIME);
	if (i2c->core < 4) {
		time_reg = (time_reg & (~0x1F)) | (fc_val & 0x1F);
	} else {
		time_reg = (time_reg & (~0x1FF00000)) | ((fc_val & 0x1FF) << 20);
	}
	i2c_writel(time_reg, i2c->regs + I2C_TIME);

	dev_dbg(&i2c->adap.dev,
			"[DEBUG][I2C] <%s> fbus_io clk: %ldHz, noise filter load value %d\n",
			__func__, fbus_ioclk_Mhz, fc_val);
	return 0;
}

static irqreturn_t tcc_i2c_isr(int irq, void *dev_id)
{
	struct tcc_i2c *i2c = dev_id;

	if(i2c_readl(i2c->regs+I2C_SR) & (1<<0)) { // check status register interrupt flag
		i2c_writel(i2c_readl(i2c->regs+I2C_CMD) | (1<<0), i2c->regs+I2C_CMD);
		complete(&i2c->msg_complete);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int tcc_i2c_bus_busy(struct tcc_i2c *i2c, int start_stop)
{
	unsigned long orig_jiffies = jiffies;
	unsigned int temp;

	while(1){
		temp = i2c_readl(i2c->regs+I2C_SR);
		if(start_stop && (temp & (1<<6)))
			break;
		if(!start_stop && !(temp & (1<<6)))
			break;
		if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(100))) {
			dev_warn(&i2c->adap.dev,
					"[WARN][I2C] <%s> I2C bus is busy\n", __func__);
			return -ETIMEDOUT;
		}
		schedule();
	}

	return 0;
}

static int wait_intr(struct tcc_i2c *i2c)
{
	unsigned long cnt;
	unsigned long orig_jiffies;
	unsigned long timeout;
	int ret;

	timeout = I2C_CMD_TIMEOUT;

	if(i2c->interrupt_mode) {
		reinit_completion(&i2c->msg_complete);
		tcc_i2c_enable_irq(i2c);
		ret = wait_for_completion_timeout(&i2c->msg_complete, msecs_to_jiffies(timeout));
		tcc_i2c_disable_irq(i2c);
		if (ret == 0) {
			dev_err(i2c->dev, "[ERROR][I2C] i2c cmd timeout (check sclk status)\n");
			return -ETIMEDOUT;
		}
	} else{
		orig_jiffies = jiffies;
		cnt = 0;
		while((i2c_readl(i2c->regs + I2C_CMD) & 0xF0) != 0) {
			if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(timeout))) {
				dev_err(i2c->dev, "[ERROR][I2C] i2c cmd timeout - 0 (check sclk status)\n");
				return -ETIMEDOUT;
			}
		}

		orig_jiffies = jiffies;
		cnt = 0;
		/* Check whether transfer is in progress */
		while((i2c_readl(i2c->regs + I2C_SR) & (1<<1)) != 0) {
			if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(timeout))) {
				dev_err(i2c->dev, "[ERROR][I2C] i2c cmd timeout - 1 (check sclk status)\n");
				return -ETIMEDOUT;
			}
		}

		/* Clear a pending interrupt */
		i2c_writel(i2c_readl(i2c->regs+I2C_CMD)|(1<<0), i2c->regs+I2C_CMD);
	}

	if((i2c_readl(i2c->regs + I2C_SR) & (1 << 5))!= 0) {
		dev_err(i2c->dev, "[ERROR][I2C] arbitration lost (check sclk, sda status)\n");
		return -1;
	}
	return 0;
}

/*
 * tcc_i2c_message_start
 * put the start of a message onto the bus
 */
static int tcc_i2c_message_start(struct tcc_i2c *i2c, struct i2c_msg *msg)
{
	unsigned int addr = (msg->addr & 0x7f) << 1;

	if (msg->flags & I2C_M_RD)
		addr |= 1;
	if (msg->flags & I2C_M_REV_DIR_ADDR)
		addr ^= 1;
	i2c_writel(addr, i2c->regs+I2C_TXR);
	i2c_writel((1<<7)|(1<<4), i2c->regs+I2C_CMD);

	return wait_intr(i2c);
}

static int tcc_i2c_stop(struct tcc_i2c *i2c)
{
	int ret = 0;

	i2c_writel(1<<6, i2c->regs+I2C_CMD);
	ret = wait_intr(i2c);
	return ret;
}

static int tcc_i2c_acked(struct tcc_i2c *i2c)
{
	unsigned long orig_jiffies = jiffies;
	unsigned int temp;
	if((i2c->msg->flags&0x1000) ==  I2C_M_IGNORE_NAK)
		return 0;

	while(1){
		temp = i2c_readl(i2c->regs+I2C_SR);
		if(!(temp & (1<<7))) break;

		tcc_i2c_message_start(i2c, i2c->msg);

		if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(I2C_ACK_TIMEOUT))) {
			dev_dbg(&i2c->adap.dev,
					"[DEBUG][I2C] <%s> No ACK\n", __func__);
			return -EIO;
		}
		schedule();
	}
	dev_dbg(&i2c->adap.dev, "[DEBUG][I2C] <%s> ACK received\n", __func__);
	return 0;
}

static int recv_i2c(struct tcc_i2c *i2c)
{
	int ret, i;
	dev_dbg(&i2c->adap.dev, "[DEBUG][I2C] READ [%x][%d]\n", i2c->msg->addr, i2c->msg->len);

	ret = tcc_i2c_message_start(i2c, i2c->msg);
	if(ret)
		return ret;

	ret = tcc_i2c_acked(i2c);
	if(ret)
		return ret;

	for (i = 0; i < i2c->msg->len; i++) {
		if (i2c->msg->flags & I2C_M_WM899x_CODEC) { /* B090183 */
			if (i == 0)
				i2c_writel(1<<5, i2c->regs+I2C_CMD);
			else
				i2c_writel((1<<5)|(1<<3), i2c->regs+I2C_CMD);
		} else {
			if (i == (i2c->msg->len - 1))
				i2c_writel((1<<5)|(1<<3), i2c->regs+I2C_CMD);
			else
				i2c_writel(1<<5, i2c->regs+I2C_CMD);
		}

		ret = wait_intr(i2c);
		if (ret)
			return ret;

		i2c->msg->buf[i] = (unsigned char)i2c_readl(i2c->regs+I2C_RXR);
	}

	return 0;
}

static int send_i2c(struct tcc_i2c *i2c)
{
	int ret, i;
	dev_dbg(&i2c->adap.dev, "[DEBUG][I2C] SEND [%x][%d]", i2c->msg->addr, i2c->msg->len);

	ret = tcc_i2c_message_start(i2c, i2c->msg);
	if(ret)
		return ret;

	ret = tcc_i2c_acked(i2c);
	if(ret)
		return ret;

	for (i = 0; i < i2c->msg->len; i++) {
		i2c_writel(i2c->msg->buf[i], i2c->regs+I2C_TXR);

		if(i2c->msg->flags == I2C_M_WM899x_CODEC) /* B090183 */
			i2c_writel((1<<4)|(1<<3), i2c->regs+I2C_CMD);
		else
			i2c_writel(1<<4, i2c->regs+I2C_CMD);

		ret = wait_intr(i2c);
		if (ret)
			return ret;

		ret = tcc_i2c_acked(i2c);
		if(ret)
			return ret;
	}
	return 0;
}

/* tcc_i2c_doxfer
 *
 * this starts an i2c transfer
 */
static int tcc_i2c_doxfer(struct tcc_i2c *i2c, struct i2c_msg *msgs, int num)
{
	int ret = 0;
	int i;

	for (i = 0; i < num; i++) {
		spin_lock_irq(&i2c->lock);
		i2c->msg        = &msgs[i];
		i2c->msg->flags = msgs[i].flags;
		i2c->msg_num    = num;
		i2c->msg_ptr    = 0;
		i2c->msg_idx    = 0;
		i2c->state      = STATE_START;
		spin_unlock_irq(&i2c->lock);

		if (i2c->msg->flags & I2C_M_RD) {
			ret = recv_i2c(i2c);
			if (ret){
				dev_dbg(i2c->dev, "[DEBUG][I2C] receiving error addr 0x%x err %d\n", i2c->msg->addr, ret);
				goto fail;
			}
		} else {
			ret = send_i2c(i2c);
			if (ret){
				dev_dbg(i2c->dev, "[DEBUG][I2C] sending error addr 0x%x err %d\n", i2c->msg->addr, ret);
				goto fail;
			}
		}

		if (i2c->msg->flags & I2C_M_STOP) {
			tcc_i2c_stop(i2c);
		}
	}

	if(i2c->msg->flags & I2C_M_NO_STOP)
		goto no_stop;


fail:
	tcc_i2c_stop(i2c);
	tcc_i2c_bus_busy(i2c, 0);
	return (ret < 0) ? ret : i;

no_stop:
	//printk(KERN_DEBUG "[DEBUG][I2C] [%s:%d] no stop \n", __func__, __LINE__);
	tcc_i2c_bus_busy(i2c, 1);
	return (ret < 0) ? ret : i;
}

/* tcc_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
 */
static int tcc_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct tcc_i2c *i2c = (struct tcc_i2c *)adap->algo_data;
	int retry;
	int ret=0;

	if (i2c->is_suspended)
		return -EBUSY;

	for (retry = 0; retry < adap->retries; retry++) {
		ret = tcc_i2c_doxfer(i2c, msgs, num);
		if (ret>0) {
			return ret;
		}
		dev_dbg(&i2c->adap.dev, "[DEBUG][I2C] Retrying transmission (%d)\n", retry);
		udelay(100);
	}
	return ret;
}

static u32 tcc_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

/* i2c bus registration info */
static const struct i2c_algorithm tcc_i2c_algo = {
	.master_xfer    = tcc_i2c_xfer,
	.functionality  = tcc_i2c_func,
};

/* tcc_i2c_set_port, tcc802x_i2c_set_port
 * set the port of i2c
 */
static int tcc_i2c_set_port(struct tcc_i2c *i2c)
{
	if (i2c->port_mux[0] != 0xFF) {
		int i;
		unsigned port_value;
		// Check Master port /
		port_value = i2c_readl(i2c->port_cfg+I2C_PORT_CFG0);
		for (i=0 ; i<4 ; i++) {
			if ((i2c->port_mux[0] & 0xFF) == ((port_value>>(i*8))&0xFF))
				port_value |= (0xFF<<(i*8));
		}
		i2c_writel(port_value, i2c->port_cfg+I2C_PORT_CFG0);

		port_value = i2c_readl(i2c->port_cfg+I2C_PORT_CFG2);
		for (i=0 ; i<4 ; i++) {
			if ((i2c->port_mux[0] & 0xFF) == ((port_value>>(i*8))&0xFF))
				port_value |= (0xFF<<(i*8));
		}
		i2c_writel(port_value, i2c->port_cfg+I2C_PORT_CFG2);

		// Check Slave port /
		port_value = i2c_readl(i2c->port_cfg+I2C_PORT_CFG1);
		for (i=0 ; i<4 ; i++) {
			if ((i2c->port_mux[0] & 0xFF) == ((port_value>>(i*8))&0xFF))
				port_value |= (0xFF<<(i*8));
		}
		i2c_writel(port_value, i2c->port_cfg+I2C_PORT_CFG1);

		if (i2c->core < 4){
			i2c_writel((i2c_readl(i2c->port_cfg+I2C_PORT_CFG0)& ~(0xFF<<(i2c->core*8)))
					| ((i2c->port_mux[0] & 0xFF)<<(i2c->core*8)), i2c->port_cfg+I2C_PORT_CFG0);
		} else if (i2c->core < 8) {
			i2c_writel((i2c_readl(i2c->port_cfg+I2C_PORT_CFG2)& ~(0xFF<<((i2c->core-4)*8)))
					| ((i2c->port_mux[0] & 0xFF)<<((i2c->core-4)*8)), i2c->port_cfg+I2C_PORT_CFG2);
		}
		else
		{
			BUG();
			return -1;
		}
		dev_info(i2c->dev, "[INFO][I2C] PORT MUX: %d\n", i2c->port_mux[0]);
		return 0;
	}
	return -1;
}

static int tcc802x_i2c_set_port(struct tcc_i2c *i2c)
{
	u32 port_value;
	u32 pcfg_value, pcfg_offset, pcfg_shift;

	if(i2c->core > 3)
	{
		BUG();
		return -1;
	}
	if(i2c->core < 2){
		pcfg_offset = I2C_PORT_CFG0;
	}else{
		pcfg_offset = I2C_PORT_CFG1;
	}

	pcfg_shift = (i2c->core % 2) * 16;
	port_value = (i2c->port_mux[0] | (i2c->port_mux[1] << 8)); /* [0]SCL [1]SDA */

	pcfg_value  = i2c_readl(i2c->port_cfg+pcfg_offset);
	pcfg_value &= ~(0xFFFF << pcfg_shift);
	pcfg_value |= (0xFFFF & port_value) << pcfg_shift;
	i2c_writel(pcfg_value, i2c->port_cfg + pcfg_offset);

	pcfg_value = i2c_readl(i2c->port_cfg+pcfg_offset);
	dev_info(i2c->dev, "[INFO][I2C] SCL PORT: GFB[%d], SDA PORT: GFB[%d] PCFG(%#X) = %#X\n",
			i2c->port_mux[0],
			i2c->port_mux[1],
			(u32)i2c->port_cfg + pcfg_offset,
			pcfg_value);

	return 0;
}


/* tcc_i2c_get_port, tcc802x_i2c_get_port
 * get the port of i2c
 */
static void tcc_i2c_get_port(struct tcc_i2c *i2c, unsigned int *port)
{
	unsigned int port_shift, port_cfg, pcfg_offset = 0;

	if(i2c->core < 4){
		pcfg_offset = I2C_PORT_CFG0;
		port_shift = i2c->core * 8;
	}else{
		pcfg_offset = I2C_PORT_CFG2;
		port_shift =(i2c->core - 4) * 8;
	}

	port_cfg = i2c_readl(i2c->port_cfg + pcfg_offset);
	port[0] = (port_cfg >> port_shift) & 0xFF;
}

static void tcc802x_i2c_get_port(struct tcc_i2c *i2c, unsigned int *port)
{
	unsigned int port_value, port_shift, port_cfg, pcfg_offset = 0;

	if(i2c->core > 3){
		BUG();
	}else if(i2c->core < 2){
		pcfg_offset = I2C_PORT_CFG0;
		port_shift = i2c->core * 16;
	}else{
		pcfg_offset = I2C_PORT_CFG1;
		port_shift = (i2c->core - 2) * 16;
	}

	port_cfg = i2c_readl(i2c->port_cfg + pcfg_offset);
	port_value = (port_cfg >> port_shift);

	port[0] = port_value & 0xFF;
	port[1] = (port_value >> 8) & 0xFF;
}

/* tcc_i2c_init, tcc803x_i2c_init
 *
 * initialize the I2C controller, set the IO lines and frequency
 */
static int tcc_i2c_init(struct tcc_i2c *i2c)
{
	unsigned int prescale;
	unsigned long peri_clk, fbus_ioclk;
	struct device_node *np = i2c->dev->of_node;
	int ret;

	if (i2c->fclk == NULL)
		i2c->fclk = of_clk_get(np, 2);
	if (i2c->hclk == NULL)
		i2c->hclk = of_clk_get(np, 1);
	if (i2c->pclk == NULL)
		i2c->pclk = of_clk_get(np, 0);

	if(clk_prepare_enable(i2c->hclk) != 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't do i2c_hclk clock enable\n");
		return -1;
	}
	if(clk_prepare_enable(i2c->pclk) != 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't do i2c_pclk clock enable\n");
		return -1;
	}
	clk_set_rate(i2c->pclk, i2c->core_clk_rate);

	fbus_ioclk = clk_get_rate(i2c->fclk);
	peri_clk = clk_get_rate(i2c->pclk);
	if(fbus_ioclk < (peri_clk * 4)){
		dev_err(i2c->dev, "[ERROR][I2C] %s: iobus clk is not enough.\n", __func__);
		return -1;
	}
	/* Set prescale */
	prescale = (peri_clk / (i2c->i2c_clk_rate * 5)) - 1;
	i2c_writel(prescale, i2c->regs+I2C_PRES);

	/* set noise filter */
	ret = i2c->soc_data->set_noise_filter(i2c);
	if(ret < 0){
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to set i2c noise filter.\n", __func__);
		return ret;
	}
	/* Enable core, Disable interrupt, Set 8 bit mode */
	i2c_writel((1<<7)|0, i2c->regs+I2C_CTRL);

	/* Clear pending interrupt */
	i2c_writel((1<<0)|i2c_readl(i2c->regs+I2C_CMD), i2c->regs+I2C_CMD);

	/* set port mux */
	ret = i2c->soc_data->set_port(i2c);
	if(ret < 0){
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to set i2c port.\n", __func__);
		return ret;
	}

	return 0;
}

static int tcc803x_i2c_init(struct tcc_i2c *i2c)
{
	unsigned int prescale;
	struct device_node *np = i2c->dev->of_node;
	unsigned int tmp;
	unsigned long peri_clk, fbus_ioclk;
	int ret;

	if (i2c->fclk == NULL)
		i2c->fclk = of_clk_get(np, 2);
	if (i2c->hclk == NULL)
		i2c->hclk = of_clk_get(np, 1);
	if (i2c->pclk == NULL)
		i2c->pclk = of_clk_get(np, 0);

	if(clk_prepare_enable(i2c->hclk) != 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't do i2c_hclk clock enable\n");
		return -1;
	}
	if(clk_prepare_enable(i2c->pclk) != 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't do i2c_pclk clock enable\n");
		return -1;
	}
	clk_set_rate(i2c->pclk, i2c->core_clk_rate);

	fbus_ioclk = clk_get_rate(i2c->fclk);
	peri_clk = clk_get_rate(i2c->pclk);
	if(fbus_ioclk < (peri_clk * 4)){
		dev_err(i2c->dev, "[ERROR][I2C] %s: iobus clk is not enough.\n", __func__);
		return -1;
	}
	/* Set prescale */
	prescale = (peri_clk / (i2c->i2c_clk_rate * 5)) - 1;
	i2c_writel(prescale, i2c->regs+I2C_PRES);

	if (i2c->core > 3) {
		/*
		 * TR.CKSEL = 0
		 * TR.STD = 0
		 */
		tmp = i2c_readl(i2c->regs+I2C_TIME);
		tmp &= (~0x000000E0);
		i2c_writel(tmp, i2c->regs+I2C_TIME);
		if (i2c->use_pw) {
			prescale = (peri_clk / (i2c->i2c_clk_rate * (i2c->pwh + i2c->pwl))) - 1;
			i2c_writel(prescale, i2c->regs+I2C_PRES);
		}
		i2c_writel(((i2c->pwh << 16) | (i2c->pwl)), i2c->regs+I2C_TR1);

		dev_info(i2c->dev, "[INFO][I2C] pulse-width-high: %d\n", i2c->pwh);
		dev_info(i2c->dev, "[INFO][I2C] pulse-width-low: %d\n", i2c->pwl);
	}

	/* set noise filter */
	ret = i2c->soc_data->set_noise_filter(i2c);
	if(ret < 0){
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to set i2c noise filter.\n", __func__);
		return ret;
	}

	/* Enable core, Disable interrupt, Set 8 bit mode */
	i2c_writel((1<<7)|0, i2c->regs+I2C_CTRL);

	/* Clear pending interrupt */
	i2c_writel((1<<0)|i2c_readl(i2c->regs+I2C_CMD), i2c->regs+I2C_CMD);

	/* set port mux */
	ret = i2c->soc_data->set_port(i2c);
	if(ret < 0){
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to set i2c port.\n", __func__);
		return ret;
	}

	return 0;
}

/* parsing data from device tree */
static void tcc_i2c_parse_dt(struct device_node *np, struct tcc_i2c *i2c)
{
	if (of_property_read_u32(np, "peri-clock-frequency",
				&i2c->core_clk_rate))
		i2c->core_clk_rate = 4000000;
	if (of_property_read_u32(np, "clock-frequency", &i2c->i2c_clk_rate))
		i2c->i2c_clk_rate = 100000;

	of_property_read_u32_array(np, "port-mux", i2c->port_mux,
			(size_t)of_property_count_elems_of_size(np, "port-mux", sizeof(u32)));

	of_property_read_u32(np, "interrupt_mode", &i2c->interrupt_mode);
	if(of_property_read_u32(np, "noise_filter", &i2c->noise_filter)){
		i2c->noise_filter = 50;
	}
	if(i2c->noise_filter < 50){
		dev_warn(i2c->dev, "[WARN][I2C] I2C must suppress noise of less than 50ns.\n");
		i2c->noise_filter = 50;
	}
}

static void tcc803x_i2c_parse_dt(struct device_node *np, struct tcc_i2c *i2c)
{
	if (of_property_read_u32(np, "peri-clock-frequency",
				&i2c->core_clk_rate))
		i2c->core_clk_rate = 4000000;
	if (of_property_read_u32(np, "clock-frequency", &i2c->i2c_clk_rate))
		i2c->i2c_clk_rate = 100000;

	of_property_read_u32_array(np, "port-mux", i2c->port_mux,
			(size_t)of_property_count_elems_of_size(np, "port-mux", sizeof(u32)));

	of_property_read_u32(np, "interrupt_mode", &i2c->interrupt_mode);
	if(of_property_read_u32(np, "noise_filter", &i2c->noise_filter)){
		i2c->noise_filter = 50;
	}
	if(i2c->noise_filter < 50){
		dev_warn(i2c->dev, "[WARN][I2C] I2C must suppress noise of less than 50ns.\n");
		i2c->noise_filter = 50;
	}

	if (of_property_read_u32(np, "pulse-width-high", &i2c->pwh) ||
			of_property_read_u32(np, "pulse-width-low", &i2c->pwl)) {
		i2c->pwh = 2;
		i2c->pwl = 3;
		i2c->use_pw = 0;
	} else {
		i2c->use_pw = 1;
	}
}

static void tcc802x_i2c_parse_dt(struct device_node *np, struct tcc_i2c *i2c)
{
	if (of_property_read_u32(np, "peri-clock-frequency",
				&i2c->core_clk_rate))
		i2c->core_clk_rate = 4000000;
	if (of_property_read_u32(np, "clock-frequency", &i2c->i2c_clk_rate))
		i2c->i2c_clk_rate = 100000;

	/* Port array order : [0]SCL [1]SDA */
	of_property_read_u32_array(np, "port-mux", i2c->port_mux,
			(size_t)of_property_count_elems_of_size(np, "port-mux", sizeof(u32)));

	of_property_read_u32(np, "interrupt_mode", &i2c->interrupt_mode);
	if(of_property_read_u32(np, "noise_filter", &i2c->noise_filter)){
		i2c->noise_filter = 50;
	}
	if(i2c->noise_filter < 50){
		dev_warn(i2c->dev, "[WARN][I2C] I2C must suppress noise of less than 50ns.\n");
		i2c->noise_filter = 50;
	}

}

static const struct tcc_i2c_soc_data tcc_soc_data = {
	.parse_dt = tcc_i2c_parse_dt,
	.init = tcc_i2c_init,
	.set_port = tcc_i2c_set_port,
	.get_port = tcc_i2c_get_port,
	.reg_dump = tcc_i2c_reg_dump,
	.set_noise_filter = tcc_i2c_set_noise_filter,
};

static const struct tcc_i2c_soc_data tcc803x_soc_data = {
	.parse_dt = tcc803x_i2c_parse_dt,
	.init = tcc803x_i2c_init,
	.set_port = tcc_i2c_set_port,
	.get_port = tcc_i2c_get_port,
	.reg_dump = tcc803x_i2c_reg_dump,
	.set_noise_filter = tcc803x_i2c_set_noise_filter,
};

static const struct tcc_i2c_soc_data tcc802x_soc_data = {
	.parse_dt = tcc802x_i2c_parse_dt,
	.init = tcc_i2c_init,
	.set_port = tcc802x_i2c_set_port,
	.get_port = tcc802x_i2c_get_port,
	.reg_dump = tcc802x_i2c_reg_dump,
	.set_noise_filter = tcc_i2c_set_noise_filter,
};

static const struct of_device_id tcc_i2c_of_match[] = {
	{ .compatible = "telechips,i2c",
		.data = &tcc_soc_data, },
	{ .compatible = "telechips,tcc803x-i2c",
		.data = &tcc803x_soc_data, },
	{ .compatible = "telechips,tcc802x-i2c",
		.data = &tcc802x_soc_data, },
	{ .compatible = "telechips,tcc897x-i2c",
		.data = &tcc_soc_data, },
	{ .compatible = "telechips,tcc899x-i2c",
		.data = &tcc_soc_data, },
	{ .compatible = "telechips,tcc901x-i2c",
		.data = &tcc_soc_data, },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_i2c_of_match);

#ifdef CONFIG_I2C_TCC_CM4
#include "../../char/tcc_early_view_control/tcc_avn_proc.h"
#endif

static int tcc_i2c_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_i2c *i2c;
	struct resource *res;
	const struct of_device_id *match;

	if (!pdev->dev.of_node)
		return -EINVAL;

	match = of_match_node(tcc_i2c_of_match, pdev->dev.of_node);
	if(!match){
		return -EINVAL;
	}

	i2c = devm_kzalloc(&pdev->dev, sizeof(struct tcc_i2c), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	/* get base register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "[ERROR][I2C] no mem resource\n");
		return -ENODEV;
	}
	i2c->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c->regs)) {
		ret = PTR_ERR(i2c->regs);
		goto err_io;
	}

	/* get i2c port config register */
	i2c->port_cfg = of_iomap(pdev->dev.of_node, 1);
	i2c->share_core = of_iomap(pdev->dev.of_node, 2);

	i2c->irq = platform_get_irq(pdev, 0);
	if (i2c->irq < 0) {
		dev_err(&pdev->dev, "[ERROR][I2C] no irq resource\n");
		ret = i2c->irq;
		goto err_io;
	}

	i2c->soc_data = match->data;
	i2c->soc_data->parse_dt(pdev->dev.of_node, i2c);

	init_completion(&i2c->msg_complete);

	pdev->id = of_alias_get_id(pdev->dev.of_node, "i2c");
	i2c->core = pdev->id;
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &tcc_i2c_algo;
	i2c->adap.retries = I2C_DEF_RETRIES;
	i2c->dev = &(pdev->dev);
	sprintf(i2c->adap.name, "%s", pdev->name);
	dev_info(&pdev->dev, "[INFO][I2C] sclk: %d kHz retry: %d irq mode: %d noise filter: %dns\n",
			(i2c->i2c_clk_rate/1000), i2c->adap.retries,
			i2c->interrupt_mode, i2c->noise_filter);
	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

#ifdef CONFIG_I2C_TCC_CM4
	/* Disable recovery operation of M4 */
	if(i2c->core == CONFIG_I2C_TCC_CM4_CH) {
		printk(KERN_DEBUG "[DEBUG][I2C] [%s:%d] CM CTRL DISABLE RECOVERY (i2c ch %d)\n", __func__, __LINE__, i2c->core);
		tcc_cm_ctrl_disable_recovery();
	}
#endif

	/* set clock & port_mux */
	if (i2c->soc_data->init(i2c) < 0)
		goto err_io;

#ifdef CONFIG_I2C_TCC_CM4
	/* Enable recovery operation of M4 */
	if(i2c->core == CONFIG_I2C_TCC_CM4_CH) {
		printk(KERN_DEBUG "[DEBUG][I2C] [%s:%d] CM CTRL ENABLE RECOVERY (i2c ch %d)\n", __func__, __LINE__, i2c->core);
		tcc_cm_ctrl_enable_recovery();
	}
#endif

	if(i2c->interrupt_mode){
		ret = request_irq(i2c->irq, tcc_i2c_isr, IRQF_SHARED, i2c->adap.name, i2c);
		if(ret) {
			dev_err(&pdev->dev, "[ERROR][I2C] Failed to request irq %i\n", i2c->irq);
			return ret;
		}
	}

	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	i2c->adap.nr = pdev->id;
	i2c->adap.dev.of_node = pdev->dev.of_node;

	ret = i2c_add_numbered_adapter(&(i2c->adap));
	if (ret < 0) {
		dev_err(&pdev->dev, "[ERROR][I2C] %s: failed to add bus\n", i2c->adap.name);
		i2c_del_adapter(&i2c->adap);
		goto err_clk;
	}

	platform_set_drvdata(pdev, i2c);
	device_create_file(&pdev->dev, &dev_attr_tcci2c);
	return 0;

err_clk:
#ifdef CONFIG_I2C_TCC_CM4
	/* Disable recovery operation of M4 */
	if(i2c->core == CONFIG_I2C_TCC_CM4_CH) {
		printk(KERN_DEBUG "[DEBUG][I2C] [%s:%d] CM CTRL DISABLE RECOVERY (i2c ch %d)\n", __func__, __LINE__, i2c->core);
		tcc_cm_ctrl_disable_recovery();
	}
#endif

	ret = tcc_i2c_check_share_core(i2c);
	if(ret == 0){
		if(i2c->pclk != NULL){
			clk_disable(i2c->pclk);
			clk_put(i2c->pclk);
			i2c->pclk = NULL;
		}
		if(i2c->hclk != NULL) {
			clk_disable(i2c->hclk);
			clk_put(i2c->hclk);
			i2c->hclk = NULL;
		}
	}

err_io:
	/* Disable I2C Core */
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1<<7), i2c->regs+I2C_CTRL);
	kfree(i2c);
	return ret;
}

static int tcc_i2c_remove(struct platform_device *pdev)
{
	struct tcc_i2c *i2c = platform_get_drvdata(pdev);
	int ret;

	platform_set_drvdata(pdev, NULL);
	device_remove_file(&pdev->dev, &dev_attr_tcci2c);

	/* Disable I2C Core */
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1<<7), i2c->regs+I2C_CTRL);
	/* I2C clock Disable */
	i2c_del_adapter(&(i2c->adap));
	/* I2C bus & clock disable */
	ret = tcc_i2c_check_share_core(i2c);
	if(ret == 0){
		if (i2c->pclk != NULL) {
			clk_disable(i2c->pclk);
			clk_put(i2c->pclk);
			i2c->pclk = NULL;
		}
		if (i2c->hclk != NULL) {
			clk_disable(i2c->hclk);
			clk_put(i2c->hclk);
			i2c->hclk = NULL;
		}
	}

	kfree(i2c);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_i2c_suspend(struct device *dev)
{
	struct tcc_i2c *i2c = dev_get_drvdata(dev);
	int ret;
	i2c_lock_adapter(&i2c->adap);

	/* Disable I2C Core */
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1<<7), i2c->regs+I2C_CTRL);
	ret = tcc_i2c_check_share_core(i2c);
	if(ret == 0){
		if(i2c->pclk != NULL)
			clk_disable_unprepare(i2c->pclk);
		if(i2c->hclk != NULL)
			clk_disable_unprepare(i2c->hclk);
	}
	i2c->is_suspended = true;

	i2c_unlock_adapter(&i2c->adap);
	return 0;
}

static int tcc_i2c_resume(struct device *dev)
{
	struct tcc_i2c *i2c = dev_get_drvdata(dev);
	i2c_lock_adapter(&i2c->adap);
	i2c->soc_data->init(i2c);
	i2c->is_suspended = false;
	i2c_unlock_adapter(&i2c->adap);
	return 0;
}
static SIMPLE_DEV_PM_OPS(tcc_i2c_pm, tcc_i2c_suspend, tcc_i2c_resume);
#define TCC_I2C_PM (&tcc_i2c_pm)
#else
#define TCC_I2C_PM NULL
#endif

static ssize_t tcc_i2c_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcc_i2c *i2c = dev_get_drvdata(dev);
	unsigned int port_value[2] = {0xFF, 0xFF};
	int ret;

	ret = sprintf(buf, "[DEBUG][I2C] channel %d, speed %d kHz, ",
			i2c->core, (i2c->i2c_clk_rate / 1000));

	i2c->soc_data->get_port(i2c, port_value);
	if(port_value[1] != 0xFF){
		ret += sprintf(buf + strlen(buf), "SCL PORT: GFB[%d] SDA PORT: GFB[%d]\n",
				port_value[0], port_value[1]);
	}else{
		ret += sprintf(buf + strlen(buf), " PORT: %d\n",
				port_value[0]);
	}

	return ret;
}

static ssize_t tcc_i2c_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* TODO: change port or speed or interrupt mode */
	return 0;
}
static struct platform_driver tcc_i2c_driver = {
	.probe                  = tcc_i2c_probe,
	.remove                 = tcc_i2c_remove,
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = "tcc-i2c",
		.pm             = TCC_I2C_PM,
		.of_match_table = of_match_ptr(tcc_i2c_of_match),
	},
};

static int __init tcc_i2c_adap_init(void)
{
	return platform_driver_register(&tcc_i2c_driver);
}

static void __exit tcc_i2c_adap_exit(void)
{
	platform_driver_unregister(&tcc_i2c_driver);
}

subsys_initcall(tcc_i2c_adap_init);
//module_init(tcc_i2c_adap_init);
module_exit(tcc_i2c_adap_exit);

MODULE_AUTHOR("Telechips Inc. androidce@telechips.com");
MODULE_DESCRIPTION("Telechips H/W I2C driver");
MODULE_LICENSE("GPL");
