// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

/* ----------------------------
 * for TCC DIBCOM DVB-T module
 * [M] i2c-core.c, i2c-dev.c
 * [M] include/linux/i2c-dev.h, include/linux/i2c.h
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
#include <linux/of_device.h>
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

#if defined(CONFIG_ARCH_TCC802X)
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

struct tcc_i2c {
	spinlock_t              lock;
	wait_queue_head_t       wait;
	void __iomem            *regs;
	void __iomem            *port_cfg;
	void __iomem            *share_core;

	int32_t                     core;
	uint32_t            i2c_clk_rate;
	uint32_t            core_clk_rate;
	struct clk              *pclk; /* I2C PERI CLK */
	struct clk              *hclk; /* I2C IO config */
	struct clk              *fclk; /* FBUS IO CLK */
	struct i2c_msg          *msg;
	uint32_t            msg_num;
	uint32_t            msg_idx;
	uint32_t            msg_ptr;

	struct completion       msg_complete;
	struct device           *dev;
	struct i2c_adapter      adap;
	bool                    is_suspended;
	uint32_t            port_mux[2];

	int32_t                     irq;
	uint32_t            interrupt_mode;
	uint32_t            noise_filter;

	bool                    use_pw;
	uint32_t            pwh;
	uint32_t            pwl;

	const struct tcc_i2c_soc_data *soc_data;
};

static ssize_t tcc_i2c_show(struct device *dev,
		struct device_attribute *attr,
		char *buf);
static ssize_t tcc_i2c_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count);
DEVICE_ATTR(tcci2c, 0664, tcc_i2c_show, tcc_i2c_store);

static struct tcc_i2c_soc_data{
	void (*set_prescale)(struct tcc_i2c *i2c);
	int32_t (*set_port)(struct tcc_i2c *i2c);
	void (*get_port)(struct tcc_i2c *i2c, uint32_t *port);
};

#ifdef DEBUG
static void tcc_i2c_reg_dump(void __iomem *regs, void __iomem *port_cfg)
{
	pr_debug("[DEBUG][I2C] I2C_PRES     : %#X\n",
			i2c_readl(regs + I2C_PRES));
	pr_debug("[DEBUG][I2C] I2C_CTRL     : %#X\n",
			i2c_readl(regs + I2C_CTRL));
	pr_debug("[DEBUG][I2C] I2C_CMD      : %#X\n",
			i2c_readl(regs + I2C_CMD));
	pr_debug("[DEBUG][I2C] I2C_SR       : %#X\n",
			i2c_readl(regs + I2C_SR));
	pr_debug("[DEBUG][I2C] I2C_TIME     : %#X\n",
			i2c_readl(regs + I2C_TIME));
	pr_debug("[DEBUG][I2C] I2C_TR1      : %#X\n",
			i2c_readl(regs + I2C_TR1));
	pr_debug("[DEBUG][I2C] I2C_PORT_CFG0: %#X\n",
			i2c_readl(port_cfg + I2C_PORT_CFG0));
	pr_debug("[DEBUG][I2C] I2C_PORT_CFG1: %#X\n",
			i2c_readl(port_cfg + I2C_PORT_CFG1));
	pr_debug("[DEBUG][I2C] I2C_PORT_CFG2: %#X\n",
			i2c_readl(port_cfg + I2C_PORT_CFG2));
	pr_debug("[DEBUG][I2C] I2C_IRQ_STS  : %#X\n",
			i2c_readl(port_cfg + I2C_IRQ_STS));
}
#endif

static void tcc_i2c_enable_irq(struct tcc_i2c *i2c)
{
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) | (1U<<6),
			i2c->regs+I2C_CTRL);
}

static void tcc_i2c_disable_irq(struct tcc_i2c *i2c)
{
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1U<<6),
			i2c->regs+I2C_CTRL);
	i2c_writel((i2c_readl(i2c->regs+I2C_CMD) | 1U),
			i2c->regs+I2C_CMD);
}
/* Before disable clock, TCC803x have to check corresponding i2c core.
 * Because peripheral clock of i2c controller 0 ~ 3 is connected with 4 ~ 7.
 */
static int32_t tcc_i2c_check_share_core(struct tcc_i2c *i2c)
{
	int32_t ret;
	uint32_t is_enabled = 0;

	if (i2c->share_core != NULL) {
		is_enabled = ((i2c_readl(i2c->share_core) >> 7U) & 1U);
	}
	if (is_enabled != 0U) {
		ret = 1;
	} else {
		ret = 0;
}
	dev_dbg(i2c->dev, "[DEBUG][I2C] %s: share_core enable %d\n",
			__func__, ret);
	return ret;
}

static int32_t tcc_i2c_set_noise_filter(struct tcc_i2c *i2c)
{
	uint32_t time_reg;
	uint32_t filter_cnt;
	uint32_t fbus_ioclk_Mhz;
	uint32_t temp;

	/* Calculate noise filter counter load value */
	fbus_ioclk_Mhz = (u32)(clk_get_rate(i2c->fclk) / 1000000U);
	if (fbus_ioclk_Mhz < 1U) {
		dev_err(i2c->dev,
				"[ERROR][I2C] %s: FBUS_IO is lower than 1Mhz, not enough!\n",
				__func__);
		return -EIO;
	}
	if ((UINT_MAX / fbus_ioclk_Mhz) < i2c->noise_filter) {
		dev_err(i2c->dev, "[ERROR][I2C] %s: noise filter %dns is too much\n",
				__func__, i2c->noise_filter);
		return -EIO;
	}
	temp = i2c->noise_filter * fbus_ioclk_Mhz;
	filter_cnt = (temp / 1000U) + 2U;

	/* set filter counter load value */
	time_reg = i2c_readl(i2c->regs + I2C_TIME);
	time_reg = (time_reg & (~0x1FU)) | (filter_cnt & 0x1FU);
	i2c_writel(time_reg, i2c->regs + I2C_TIME);

	dev_dbg(&i2c->adap.dev,
			"[DEBUG][I2C] <%s> fbus_io clk: %dHz, noise filter load value %d\n",
			__func__, fbus_ioclk_Mhz, filter_cnt);
	return 0;
}

static irqreturn_t tcc_i2c_isr(int32_t irq, void *dev_id)
{
	struct tcc_i2c *i2c = dev_id;

	/* check status register - interrupt flag */
	if ((i2c_readl(i2c->regs+I2C_SR) & 1U) != 0U) {
		i2c_writel((i2c_readl(i2c->regs+I2C_CMD) | 1U),
				i2c->regs+I2C_CMD);
		complete(&i2c->msg_complete);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int32_t tcc_i2c_bus_busy(struct tcc_i2c *i2c, int32_t start_stop)
{
	ulong orig_jiffies, tmo_jiffies;
	uint32_t tmo, temp;

	tmo = I2C_CMD_TIMEOUT;
	orig_jiffies = jiffies;
	tmo_jiffies = msecs_to_jiffies(tmo);

	while (1) {
		temp = i2c_readl(i2c->regs+I2C_SR);
		if (((temp >> 6U) & 1U) == (u32)start_stop) {
			break;
		}
		if (time_after(jiffies, (orig_jiffies + tmo_jiffies))) {
			dev_warn(&i2c->adap.dev,
					"[WARN][I2C] <%s> I2C bus is busy\n",
					__func__);
			return -ETIMEDOUT;
		}
		schedule();
	}

	return 0;
}

static int32_t wait_intr(struct tcc_i2c *i2c)
{
	ulong cnt;
	ulong orig_jiffies, tmo_jiffies;
	uint32_t tmo;
	ulong ret;

	tmo = I2C_CMD_TIMEOUT;
	tmo_jiffies = msecs_to_jiffies(tmo);

	if (i2c->interrupt_mode != 0UL) {
		reinit_completion(&i2c->msg_complete);
		tcc_i2c_enable_irq(i2c);
		ret = wait_for_completion_timeout(&i2c->msg_complete,
				tmo_jiffies);
		tcc_i2c_disable_irq(i2c);
		if (ret == 0U) {
			dev_err(i2c->dev, "[ERROR][I2C] i2c cmd timeout (check sclk status)\n");
			return -ETIMEDOUT;
		}
	} else{
		orig_jiffies = jiffies;
		cnt = 0;
		while ((i2c_readl(i2c->regs + I2C_CMD) & 0xF0UL) != 0UL) {
			if (time_after(jiffies, (orig_jiffies + tmo_jiffies))) {
				dev_err(i2c->dev, "[ERROR][I2C] i2c cmd timeout - 0 (check sclk status)\n");
				return -ETIMEDOUT;
			}
		}

		orig_jiffies = jiffies;
		cnt = 0;
		/* Check whether transfer is in progress */
		while ((i2c_readl(i2c->regs + I2C_SR) & (1UL<<1)) != 0UL) {
			if (time_after(jiffies, (orig_jiffies + tmo_jiffies))) {
				dev_err(i2c->dev, "[ERROR][I2C] i2c cmd timeout - 0 (check sclk status)\n");
				return -ETIMEDOUT;
			}
		}

		/* Clear a pending interrupt */
		i2c_writel(i2c_readl(i2c->regs+I2C_CMD)|1u,
				i2c->regs+I2C_CMD);
	}

	if ((i2c_readl(i2c->regs + I2C_SR) & (1U<<5)) != 0U) {
		dev_err(i2c->dev, "[ERROR][I2C] arbitration lost (check sclk, sda status)\n");
		return -EIO;
	}
	return 0;
}

/*
 * tcc_i2c_message_start
 * put the start of a message onto the bus
 */
static int32_t tcc_i2c_message_start(struct tcc_i2c *i2c, struct i2c_msg *msg)
{
	uint32_t addr = ((u32)msg->addr & 0x7fu) << 1;

	if ((msg->flags & (u16)I2C_M_RD) != (u16)0) {
		addr |= 1u;
	}
	if ((msg->flags & (u16)I2C_M_REV_DIR_ADDR) != (u16)0) {
		addr ^= 1u;
	}
	i2c_writel(addr, i2c->regs+I2C_TXR);
	i2c_writel((1U<<7)|(1U<<4), i2c->regs+I2C_CMD);

	return wait_intr(i2c);
}

static int32_t tcc_i2c_stop(struct tcc_i2c *i2c)
{
	int32_t ret;

	i2c_writel(((u32)1U<<6), i2c->regs+I2C_CMD);
	ret = wait_intr(i2c);
	return ret;
}

static int32_t tcc_i2c_acked(struct tcc_i2c *i2c)
{
	ulong orig_jiffies = jiffies, tmo_jiffies;
	uint32_t tmo, temp;
	int32_t ret;

	tmo = I2C_ACK_TIMEOUT;
	tmo_jiffies = msecs_to_jiffies(tmo);

	if ((i2c->msg->flags & (u16)I2C_M_IGNORE_NAK) != (u16)0) {
		return 0;
	}
	while (1) {
		temp = i2c_readl(i2c->regs+I2C_SR);
		if ((temp & (1U<<7)) == 0u) {
			break;
		}
		ret = tcc_i2c_message_start(i2c, i2c->msg);
		if (ret != 0) {
			return ret;
		}
		if (time_after(jiffies, (orig_jiffies + tmo_jiffies))) {
			dev_dbg(&i2c->adap.dev,
					"[DEBUG][I2C] <%s> No ACK\n", __func__);
			return -EIO;
		}
		schedule();
	}
	dev_dbg(&i2c->adap.dev, "[DEBUG][I2C] <%s> ACK received\n", __func__);
	return 0;
}

static int32_t recv_i2c(struct tcc_i2c *i2c)
{
	int32_t ret;
	u16 i;

	dev_dbg(&i2c->adap.dev,
			"[DEBUG][I2C] READ [%x][%d]\n",
			i2c->msg->addr, i2c->msg->len);

	ret = tcc_i2c_message_start(i2c, i2c->msg);
	if (ret != 0) {
		return ret;
	}
	ret = tcc_i2c_acked(i2c);
	if (ret != 0) {
		return ret;
	}
	for (i = 0; i < i2c->msg->len; i++) {
		/* B090183 */
		if ((i2c->msg->flags & (u16)I2C_M_WM899x_CODEC) != 0U) {
			if (i == (u16)0) {
				i2c_writel((u32)1UL<<5, i2c->regs+I2C_CMD);
			} else {
				i2c_writel((1U<<5)|(1U<<3), i2c->regs+I2C_CMD);
			}
		} else {
			if (i == (i2c->msg->len - 1U)) {
				i2c_writel((1U<<5)|(1U<<3), i2c->regs+I2C_CMD);
			} else {
				i2c_writel((u32)1U<<5, i2c->regs+I2C_CMD);
			}
		}

		ret = wait_intr(i2c);
		if (ret != 0) {
			return ret;
		}
		i2c->msg->buf[i] = (uint8_t)i2c_readl(i2c->regs+I2C_RXR);
	}

	return 0;
}

static int32_t send_i2c(struct tcc_i2c *i2c)
{
	int32_t ret;
	u16 i;

	dev_dbg(&i2c->adap.dev,
			"[DEBUG][I2C] SEND [%x][%d]",
			i2c->msg->addr, i2c->msg->len);

	ret = tcc_i2c_message_start(i2c, i2c->msg);
	if (ret != 0) {
		return ret;
	}
	ret = tcc_i2c_acked(i2c);
	if (ret != 0) {
		return ret;
	}
	for (i = 0; i < i2c->msg->len; i++) {
		i2c_writel(i2c->msg->buf[i], i2c->regs+I2C_TXR);

		if (i2c->msg->flags == (u16)I2C_M_WM899x_CODEC) {/* B090183 */
			i2c_writel((1U<<4)|(1U<<3), i2c->regs+I2C_CMD);
		} else {
			i2c_writel(((u32)1U<<4), i2c->regs+I2C_CMD);
		}
		ret = wait_intr(i2c);
		if (ret != 0) {
			return ret;
		}
		ret = tcc_i2c_acked(i2c);
		if (ret != 0) {
			return ret;
		}
	}
	return 0;
}

/* tcc_i2c_doxfer
 *
 * this starts an i2c transfer
 */
static int32_t tcc_i2c_doxfer(
		struct tcc_i2c *i2c,
		struct i2c_msg *msgs,
		int32_t num)
{
	int32_t ret = 0;
	int32_t i;

	for (i = 0; i < num; i++) {
		spin_lock_irq(&i2c->lock);
		i2c->msg        = &msgs[i];
		i2c->msg->flags = msgs[i].flags;
		i2c->msg_num    = (u16)num;
		i2c->msg_ptr    = 0;
		i2c->msg_idx    = 0;
		spin_unlock_irq(&i2c->lock);

		if ((i2c->msg->flags & (u16)I2C_M_RD) != (u16)0) {
			ret = recv_i2c(i2c);
			if (ret != 0) {
				dev_dbg(i2c->dev,
						"[DEBUG][I2C] receiving error addr 0x%x err %d\n",
						i2c->msg->addr, ret);
				goto fail;
			}
		} else {
			ret = send_i2c(i2c);
			if (ret != 0) {
				dev_dbg(i2c->dev,
						"[DEBUG][I2C] sending error addr 0x%x err %d\n",
						i2c->msg->addr, ret);
				goto fail;
			}
		}

		if ((i2c->msg->flags & (u16)I2C_M_STOP) != (u16)0) {
			(void)tcc_i2c_stop(i2c);
		}
	}

	if ((i2c->msg->flags & (u16)I2C_M_NO_STOP) != (u16)0) {
		goto no_stop;
	}
fail:
	(void)tcc_i2c_stop(i2c);
	(void)tcc_i2c_bus_busy(i2c, 0);
	return (ret < 0) ? ret : i;

no_stop:
	pr_debug("[DEBUG][I2C] %s: no stop\n", __func__);
	(void)tcc_i2c_bus_busy(i2c, 1);
	return i;
}

/* tcc_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
 */
static int32_t tcc_i2c_xfer(
		struct i2c_adapter *adap,
		struct i2c_msg *msgs,
		int32_t num)
{
	struct tcc_i2c *i2c = (struct tcc_i2c *)adap->algo_data;
	int32_t retry;
	int32_t ret = 0;

	if (i2c->is_suspended) {
		return -EBUSY;
	}
	for (retry = 0; retry < adap->retries; retry++) {
		ret = tcc_i2c_doxfer(i2c, msgs, num);
		if (ret > 0) {
			return ret;
		}
		dev_dbg(&i2c->adap.dev,
				"Retrying transmission (%d)\n",
				retry);
	}
	return ret;
}

static u32 tcc_i2c_func(struct i2c_adapter *adap)
{
	return (u32)(I2C_FUNC_I2C) | (u32)(I2C_FUNC_SMBUS_EMUL);
}

/* i2c bus registration info */
static const struct i2c_algorithm tcc_i2c_algo = {
	.master_xfer    = tcc_i2c_xfer,
	.functionality  = tcc_i2c_func,
};

/* tcc_i2c_set_port, tcc802x_i2c_set_port
 * set the port of i2c
 */
static int32_t tcc_i2c_set_port(struct tcc_i2c *i2c)
{
	int32_t i, offset;
	uint32_t reg_shift, pcfg_val;
	uint8_t port = (u8)(i2c->port_mux[0] & 0xFFU);

	if (port == 0xFFU) {
		return -EINVAL;
	}
	/* Check conflict of i2c master port */
	pcfg_val = i2c_readl(i2c->port_cfg + I2C_PORT_CFG0);
	for (i = 0; i < 4; i++) {
		reg_shift = (u32)i << 3;
		if (port == ((pcfg_val >> reg_shift) & 0xFFU)) {
			/* Clear duplicated port */
			pcfg_val |= ((u32)0xFFU << reg_shift);
		}
	}
	i2c_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG0);

	pcfg_val = i2c_readl(i2c->port_cfg + I2C_PORT_CFG2);
	for (i = 0; i < 4; i++) {
		reg_shift = (u32)i << 3;
		if (port == ((pcfg_val >> reg_shift) & 0xFFU)) {
			/* Clear duplicated port */
			pcfg_val |= ((u32)0xFFU << reg_shift);
		}
	}
	i2c_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG2);

	pcfg_val = i2c_readl(i2c->port_cfg + I2C_PORT_CFG1);
	for (i = 0; i < 4; i++) {
		reg_shift = (u32)i << 3;
		if (port == ((pcfg_val >> reg_shift) & 0xFFU)) {
			/* Clear duplicated port */
			pcfg_val |= ((u32)0xFFU << reg_shift);
		}
	}
	i2c_writel(pcfg_val, i2c->port_cfg + I2C_PORT_CFG1);

	/* Set i2c port-mux */
	if (i2c->core < 4) {
		offset = I2C_PORT_CFG0;
		reg_shift = (u32)i2c->core << 3;
	} else {
		offset = I2C_PORT_CFG2;
		reg_shift = ((u32)i2c->core - 4U) << 3U;
	}
	pcfg_val = i2c_readl(i2c->port_cfg + offset);
	pcfg_val &= ~((u32)0xFFU << reg_shift);
	pcfg_val |= ((u32)port << reg_shift);
	i2c_writel(pcfg_val, i2c->port_cfg + offset);

	dev_info(i2c->dev, "[INFO][I2C] PORT MUX: %d\n", port);
	return 0;
}

static int32_t tcc802x_i2c_set_port(struct tcc_i2c *i2c)
{
	u32 port_value;
	u32 pcfg_value, pcfg_offset, pcfg_shift;

	if (i2c->core > 3) {
		return -EINVAL;
	}
	if (i2c->core < 2) {
		pcfg_offset = I2C_PORT_CFG0;
	} else {
		pcfg_offset = I2C_PORT_CFG1;
	}
	pcfg_shift = ((u32)i2c->core % 2U) << 4U;
	/* [0]SCL [1]SDA */
	port_value = (i2c->port_mux[0] | (i2c->port_mux[1] << 8));
	pcfg_value  = i2c_readl(i2c->port_cfg+pcfg_offset);
	pcfg_value &= ~((u32)0xFFFFU << pcfg_shift);
	pcfg_value |= (0xFFFFU & port_value) << pcfg_shift;
	i2c_writel(pcfg_value, i2c->port_cfg + pcfg_offset);

	pcfg_value = i2c_readl(i2c->port_cfg+pcfg_offset);
	dev_info(i2c->dev, "[INFO][I2C] SCL PORT: GFB[%d], SDA PORT: GFB[%d] PCFG(%#lX) = %#X\n",
			i2c->port_mux[0],
			i2c->port_mux[1],
			(ulong)(i2c->port_cfg + pcfg_offset),
			pcfg_value);

	return 0;
}


/* tcc_i2c_get_port, tcc802x_i2c_get_port
 * get the port of i2c
 */
static void tcc_i2c_get_port(struct tcc_i2c *i2c, uint32_t *port)
{
	uint32_t port_shift, port_cfg, pcfg_offset;

	if (i2c->core < 4) {
		pcfg_offset = I2C_PORT_CFG0;
		port_shift = (u32)i2c->core << 3;
	} else {
		pcfg_offset = I2C_PORT_CFG2;
		port_shift = ((u32)i2c->core - 4U) << 3;
	}

	port_cfg = i2c_readl(i2c->port_cfg + pcfg_offset);
	port[0] = (port_cfg >> port_shift) & 0xFFU;
}

static void tcc802x_i2c_get_port(struct tcc_i2c *i2c, uint32_t *port)
{
	uint32_t port_value, port_shift, port_cfg, pcfg_offset = 0;

	if (i2c->core < 2) {
		pcfg_offset = I2C_PORT_CFG0;
		port_shift = (u32)i2c->core << 4;
	} else {
		pcfg_offset = I2C_PORT_CFG1;
		port_shift = ((u32)i2c->core - 2U) << 4;
	}

	port_cfg = i2c_readl(i2c->port_cfg + pcfg_offset);
	port_value = (port_cfg >> port_shift);

	port[0] = port_value & 0xFFU;
	port[1] = (port_value >> 8U) & 0xFFU;
}

static void tcc_i2c_set_prescale(struct tcc_i2c *i2c)
{
	uint32_t prescale;
	ulong peri_clk;

	peri_clk = clk_get_rate(i2c->pclk);
	prescale = ((u32)peri_clk / (i2c->i2c_clk_rate * 5U)) - 1U;
	i2c_writel(prescale, i2c->regs+I2C_PRES);
}

static void tcc803x_i2c_set_prescale(struct tcc_i2c *i2c)
{
	uint32_t prescale, tmp;
	ulong peri_clk;

	peri_clk = clk_get_rate(i2c->pclk);
	prescale = ((u32)peri_clk / (i2c->i2c_clk_rate * 5U)) - 1U;
	i2c_writel(prescale, i2c->regs+I2C_PRES);

	if (i2c->core > 3) {
		/* TR.CKSEL = 0
		 * TR.STD = 0
		 */
		tmp = i2c_readl(i2c->regs+I2C_TIME);
		tmp &= (~0x000000E0u);
		i2c_writel(tmp, i2c->regs+I2C_TIME);
		if (i2c->use_pw) {
			tmp = i2c->i2c_clk_rate * (i2c->pwh + i2c->pwl);
			prescale = ((u32)peri_clk / tmp) - 1U;
			i2c_writel(prescale, i2c->regs+I2C_PRES);
		}
		i2c_writel(((i2c->pwh << 16) | (i2c->pwl)), i2c->regs+I2C_TR1);

		dev_info(i2c->dev,
				"[INFO][I2C] pulse-width-high: %d\n",
				i2c->pwh);
		dev_info(i2c->dev,
				"[INFO][I2C] pulse-width-low: %d\n",
				i2c->pwl);
	}
}

/* tcc_i2c_init
 *
 * initialize the I2C controller, set the IO lines and frequency
 */
static int32_t tcc_i2c_init(struct tcc_i2c *i2c)
{
	ulong fbus_ioclk, peri_clk;
	struct device_node *np = i2c->dev->of_node;
	int32_t ret;

	if (i2c->fclk == NULL) {
		i2c->fclk = of_clk_get(np, 2);
	}
	if (i2c->hclk == NULL) {
		i2c->hclk = of_clk_get(np, 1);
	}
	if (i2c->pclk == NULL) {
		i2c->pclk = of_clk_get(np, 0);
	}

	ret = clk_prepare_enable(i2c->hclk);
	if (ret < 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't do i2c_hclk clock enable\n");
		return ret;
	}
	ret = clk_prepare_enable(i2c->pclk);
	if (ret < 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't do i2c_pclk clock enable\n");
		clk_disable_unprepare(i2c->hclk);
		return ret;
	}

	ret = clk_set_rate(i2c->pclk, i2c->core_clk_rate);
	if (ret < 0) {
		dev_err(i2c->dev, "[ERROR][I2C] can't set i2c_pclk\n");
		return ret;
	}


	fbus_ioclk = clk_get_rate(i2c->fclk);
	peri_clk = clk_get_rate(i2c->pclk);
	if (fbus_ioclk < (peri_clk << 2U)) { /* IO CLK > (I2C peri clk x 4) */
		dev_err(i2c->dev, "[ERROR][I2C] %s: iobus clk(%ldHz) is not enough.\n",
				__func__, fbus_ioclk);
		return -EINVAL;
	}
	/* Set prescale */
	i2c->soc_data->set_prescale(i2c);

	/* set noise filter */
	ret = tcc_i2c_set_noise_filter(i2c);
	if (ret < 0) {
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to set i2c noise filter.\n",
				__func__);
		return ret;
	}
	/* Enable core, Disable interrupt, Set 8 bit mode */
	i2c_writel(((u32)1U<<7), i2c->regs+I2C_CTRL);

	/* Clear pending interrupt */
	i2c_writel((i2c_readl(i2c->regs+I2C_CMD) | 1U), i2c->regs+I2C_CMD);

	/* set port mux */
	ret = i2c->soc_data->set_port(i2c);
	if (ret < 0) {
		dev_err(i2c->dev, "[ERROR][I2C] %s: failed to set i2c port.\n",
				__func__);
		return ret;
	}

	return 0;
}

/* parsing data from device tree */
static void tcc_i2c_parse_dt(struct device_node *np, struct tcc_i2c *i2c)
{
	int32_t ret, ret1, ret2;
	size_t property_size;

	ret = of_property_read_u32(np, "peri-clock-frequency",
			&i2c->core_clk_rate);
	if (ret != 0) {
		i2c->core_clk_rate = 4000000;
	}
	ret = of_property_read_u32(np, "clock-frequency", &i2c->i2c_clk_rate);
	if (ret != 0) {
		i2c->i2c_clk_rate = 100000;
	}
	if (i2c->i2c_clk_rate > 400000U) {
		dev_warn(i2c->dev, "[WARN][I2C] SCL %dHz is not supported\n",
				i2c->i2c_clk_rate);
		i2c->i2c_clk_rate = 400000;
	}

	property_size = (size_t)of_property_count_elems_of_size(np,
			"port-mux",
			sizeof(u32));
	ret = of_property_read_u32_array(np, "port-mux",
			i2c->port_mux,
			property_size);
	if (ret != 0) {
		dev_warn(i2c->dev, "[WARN][I2C] ch %d - failed to get port-mux\n",
				i2c->core);
	}

	(void)of_property_read_u32(np, "interrupt_mode", &i2c->interrupt_mode);

	ret = of_property_read_u32(np, "noise_filter", &i2c->noise_filter);
	if (ret != 0) {
		i2c->noise_filter = 50;
	}
	if (i2c->noise_filter < 50U) {
		dev_warn(i2c->dev, "[WARN][I2C] I2C must suppress noise of less than 50ns.\n");
		i2c->noise_filter = 50;
	}

	ret1 = of_property_read_u32(np, "pulse-width-high", &i2c->pwh);
	ret2 = of_property_read_u32(np, "pulse-width-low", &i2c->pwl);
	if ((ret1 != 0) || (ret2 != 0)) {
		i2c->pwh = 2;
		i2c->pwl = 3;
		i2c->use_pw = (bool)false;
	} else {
		i2c->use_pw = (bool)true;
	}
}

static const struct tcc_i2c_soc_data tcc_soc_data = {
	.set_prescale = tcc_i2c_set_prescale,
	.set_port = tcc_i2c_set_port,
	.get_port = tcc_i2c_get_port,
};

static const struct tcc_i2c_soc_data tcc803x_soc_data = {
	.set_prescale = tcc803x_i2c_set_prescale,
	.set_port = tcc_i2c_set_port,
	.get_port = tcc_i2c_get_port,
};

static const struct tcc_i2c_soc_data tcc802x_soc_data = {
	.set_prescale = tcc_i2c_set_prescale,
	.set_port = tcc802x_i2c_set_port,
	.get_port = tcc802x_i2c_get_port,
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

static int32_t tcc_i2c_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct tcc_i2c *i2c;
	struct resource *res;
	void *pv_i2c;
	const struct of_device_id *match;

	if (pdev->dev.of_node == NULL) {
		return -EINVAL;
	}
	match = of_match_node(tcc_i2c_of_match, pdev->dev.of_node);
	if (match == NULL) {
		return -EINVAL;
	}
	pv_i2c = devm_kzalloc(&pdev->dev,
			sizeof(struct tcc_i2c),
			GFP_KERNEL);
	i2c = pv_i2c;
	if (i2c == NULL) {
		return -ENOMEM;
	}
	/* get base register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "[ERROR][I2C] no mem resource\n");
		return -ENODEV;
	}
	i2c->regs = devm_ioremap_resource(&pdev->dev, res);
	ret = (int32_t)IS_ERR(i2c->regs);
	if (ret != 0) {
		ret = (int32_t)PTR_ERR(i2c->regs);
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
	tcc_i2c_parse_dt(pdev->dev.of_node, i2c);

	init_completion(&i2c->msg_complete);

	pdev->id = of_alias_get_id(pdev->dev.of_node, "i2c");
	i2c->core = pdev->id;
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &tcc_i2c_algo;
	i2c->adap.retries = I2C_DEF_RETRIES;
	i2c->dev = &(pdev->dev);
	(void)sprintf(i2c->adap.name, "%s", pdev->name);
	dev_info(&pdev->dev, "[INFO][I2C] sclk: %d kHz retry: %d irq mode: %d noise filter: %dns\n",
			(i2c->i2c_clk_rate/1000U), i2c->adap.retries,
			i2c->interrupt_mode, i2c->noise_filter);
	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

#ifdef CONFIG_I2C_TCC_CM4
	/* Disable recovery operation of M4 */
	if (i2c->core == CONFIG_I2C_TCC_CM4_CH) {
		dev_dbg(&pdev->dev,
				"[DEBUG][I2C] %s: CM CTRL DISABLE RECOVERY (i2c ch %d)\n",
				__func__, i2c->core);
		tcc_cm_ctrl_disable_recovery();
	}
#endif

	/* set clock & port_mux */
	if (tcc_i2c_init(i2c) < 0) {
		goto err_io;
	}
#ifdef CONFIG_I2C_TCC_CM4
	/* Enable recovery operation of M4 */
	if (i2c->core == CONFIG_I2C_TCC_CM4_CH) {
		dev_dbg(&pdev->dev,
				"[DEBUG][I2C] %s: CM CTRL ENABLE RECOVERY (i2c ch %d)\n",
				__func__, i2c->core);
		tcc_cm_ctrl_enable_recovery();
	}
#endif

	if ((i2c->interrupt_mode) != 0UL) {
		ret = request_irq((u32)(i2c->irq),
				tcc_i2c_isr,
				IRQF_SHARED,
				i2c->adap.name,
				pv_i2c);
		if (ret != 0) {
			dev_err(&pdev->dev, "[ERROR][I2C] Failed to request irq %d\n",
					i2c->irq);
			goto err_clk;
		}
	}

	i2c->adap.algo_data = pv_i2c;
	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.class = (u32)(I2C_CLASS_HWMON) | (u32)(I2C_CLASS_SPD);
	i2c->adap.nr = pdev->id;
	i2c->adap.dev.of_node = pdev->dev.of_node;

	ret = i2c_add_numbered_adapter(&(i2c->adap));
	if (ret < 0) {
		dev_err(&pdev->dev, "[ERROR][I2C] %s: failed to add bus\n",
				i2c->adap.name);
		i2c_del_adapter(&i2c->adap);
		goto err_clk;
	}

	platform_set_drvdata(pdev, pv_i2c);

	ret = device_create_file(&pdev->dev, &dev_attr_tcci2c);
	if (ret < 0) {
		dev_err(&pdev->dev, "[ERROR][I2C] failed to create i2c device file\n");
		i2c_del_adapter(&i2c->adap);
		goto err_clk;
	}

	return 0;

err_clk:
#ifdef CONFIG_I2C_TCC_CM4
	/* Disable recovery operation of M4 */
	if (i2c->core == CONFIG_I2C_TCC_CM4_CH) {
		pr_debug("[DEBUG][I2C] [%s:%d] CM CTRL DISABLE RECOVERY (i2c ch %d)\n",
				__func__, __LINE__, i2c->core);
		tcc_cm_ctrl_disable_recovery();
	}
#endif

	ret = tcc_i2c_check_share_core(i2c);
	if (ret == 0) {
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

err_io:
	/* Disable I2C Core */
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1U<<7),
			i2c->regs+I2C_CTRL);
	kfree(pv_i2c);
	return ret;
}

static int32_t tcc_i2c_remove(struct platform_device *pdev)
{
	struct tcc_i2c *i2c;
	void *pv_i2c;
	int32_t ret;

	pv_i2c = platform_get_drvdata(pdev);
	i2c = pv_i2c;

	platform_set_drvdata(pdev, NULL);
	device_remove_file(&pdev->dev, &dev_attr_tcci2c);

	/* Disable I2C Core */
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1U<<7),
			i2c->regs+I2C_CTRL);
	/* I2C clock Disable */
	i2c_del_adapter(&(i2c->adap));
	/* I2C bus & clock disable */
	ret = tcc_i2c_check_share_core(i2c);
	if (ret == 0) {
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

	kfree(pv_i2c);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_i2c_suspend(struct device *dev)
{
	struct tcc_i2c *i2c = dev_get_drvdata(dev);
	int32_t ret;

	i2c_lock_adapter(&i2c->adap);

	/* Disable I2C Core */
	i2c_writel(i2c_readl(i2c->regs+I2C_CTRL) & ~(1U<<7),
			i2c->regs+I2C_CTRL);
	ret = tcc_i2c_check_share_core(i2c);
	if (ret == 0) {
		if (i2c->pclk != NULL) {
			clk_disable_unprepare(i2c->pclk);
		}
		if (i2c->hclk != NULL) {
			clk_disable_unprepare(i2c->hclk);
		}
	}
	i2c->is_suspended = (bool)true;

	i2c_unlock_adapter(&i2c->adap);
	return 0;
}

static int32_t tcc_i2c_resume(struct device *dev)
{
	void *pv_i2c = dev_get_drvdata(dev);
	struct tcc_i2c *i2c;
	int32_t ret;

	i2c = pv_i2c;
	i2c_lock_adapter(&i2c->adap);
	ret = tcc_i2c_init(i2c);
	i2c->is_suspended = (bool)false;
	i2c_unlock_adapter(&i2c->adap);

	if (ret < 0) {
		kfree(pv_i2c);
	}
	return ret;
}
static SIMPLE_DEV_PM_OPS(tcc_i2c_pm, tcc_i2c_suspend, tcc_i2c_resume);
#define TCC_I2C_PM (&tcc_i2c_pm)
#else
#define TCC_I2C_PM NULL
#endif

static ssize_t tcc_i2c_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct tcc_i2c *i2c = dev_get_drvdata(dev);
	uint32_t port_value[2] = {0xFFU, 0xFFU};
	int32_t ret;

	ret = sprintf(buf, "[DEBUG][I2C] channel %d, speed %d kHz, ",
			i2c->core, (i2c->i2c_clk_rate / 1000U));

	i2c->soc_data->get_port(i2c, port_value);
	if (port_value[1] != 0xFFU) {
		ret += sprintf(buf + strnlen(buf, (size_t)ret),
				"SCL PORT: GFB[%d] SDA PORT: GFB[%d]\n",
				port_value[0], port_value[1]);
	} else {
		ret += sprintf(buf + strnlen(buf, (size_t)ret), " PORT: %d\n",
				port_value[0]);
	}
	return ret;
}

static ssize_t tcc_i2c_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
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

static int32_t __init tcc_i2c_adap_init(void)
{
	return platform_driver_register(&tcc_i2c_driver);
}

static void __exit tcc_i2c_adap_exit(void)
{
	platform_driver_unregister(&tcc_i2c_driver);
}

subsys_initcall(tcc_i2c_adap_init);
module_exit(tcc_i2c_adap_exit);

MODULE_AUTHOR("Telechips Inc. androidce@telechips.com");
MODULE_DESCRIPTION("Telechips H/W I2C driver");
MODULE_LICENSE("GPL");
