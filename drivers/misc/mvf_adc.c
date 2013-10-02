/* Copyright 2012 Freescale Semiconductor, Inc.
 * Copyright 2013 Toradex AG
 *
 * Freescale Faraday Quad ADC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mvf_adc.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/colibri-ts.h>

#define DRIVER_NAME "mvf-adc"
#define DRIVER_TS_NAME "mvf-adc-ts"
#define DRV_VERSION "1.2"

#define MVF_ADC_MAX_DEVICES 4
#define MVF_ADC_MAX ((1 << 12) - 1)

#define COLI_TOUCH_MIN_DELAY_US 1000
#define COLI_TOUCH_MAX_DELAY_US 2000

#define COL_TS_GPIO_XP 0
#define COL_TS_GPIO_XM 1
#define COL_TS_GPIO_YP 2
#define COL_TS_GPIO_YM 3
#define COL_TS_GPIO_TOUCH 4
#define COL_TS_GPIO_PULLUP 5

#define COL_TS_WIRE_XP 0
#define COL_TS_WIRE_XM 1
#define COL_TS_WIRE_YP 2
#define COL_TS_WIRE_YM 3

struct col_ts_driver {
	int enable_state;
	struct gpio *control_gpio;
};

/* GPIO */
static struct gpio col_ts_gpios[] = {
	[COL_TS_GPIO_XP] = { 13, GPIOF_IN, "Touchscreen PX" },
	[COL_TS_GPIO_XM] = { 5, GPIOF_IN, "Touchscreen MX" },
	[COL_TS_GPIO_YP] = { 12, GPIOF_IN, "Touchscreen PY" },
	[COL_TS_GPIO_YM] = { 4, GPIOF_IN, "Touchscreen MY" },
	[COL_TS_GPIO_TOUCH] = { 8, GPIOF_IN, "Touchscreen (Touch interrupt)" },
	[COL_TS_GPIO_PULLUP] = { 9, GPIOF_IN, "Touchscreen (Pull-up)" },
};

/* GPIOs and their FET configuration */
static struct col_ts_driver col_ts_drivers[] = {
	[COL_TS_WIRE_XP] = {
		.enable_state = 0,
		.control_gpio = &col_ts_gpios[COL_TS_GPIO_XP],
	},
	[COL_TS_WIRE_XM] = {
		.enable_state = 1,
		.control_gpio = &col_ts_gpios[COL_TS_GPIO_XM],
	},
	[COL_TS_WIRE_YP] = {
		.enable_state = 0,
		.control_gpio = &col_ts_gpios[COL_TS_GPIO_YP],
	},
	[COL_TS_WIRE_YM] = {
		.enable_state = 1,
		.control_gpio  = &col_ts_gpios[COL_TS_GPIO_YM],
	},
};

#define col_ts_init_wire(ts_gpio) \
	gpio_direction_output(col_ts_drivers[ts_gpio].control_gpio->gpio, \
			!col_ts_drivers[ts_gpio].enable_state)

#define col_ts_enable_wire(ts_gpio) \
	gpio_set_value(col_ts_drivers[ts_gpio].control_gpio->gpio, \
			col_ts_drivers[ts_gpio].enable_state)

#define col_ts_disable_wire(ts_gpio) \
	gpio_set_value(col_ts_drivers[ts_gpio].control_gpio->gpio, \
			!col_ts_drivers[ts_gpio].enable_state)

/*
 * wait_event_interruptible(wait_queue_head_t suspendq, int suspend_flag)
 */
static struct class *adc_class;
static int mvf_adc_major;

struct adc_client {
	struct platform_device	*pdev;
	struct list_head	 list;
	wait_queue_head_t	*wait;

	unsigned int	result;
	unsigned int	channel;
};

static LIST_HEAD(client_list);/* Initialization client list head */
static DECLARE_COMPLETION(adc_tsi);

struct adc_device {
	struct platform_device	*pdev;
	struct platform_device	*owner;
	struct clk		*clk;
	struct adc_client	*cur;
	void __iomem		*regs;
	spinlock_t		 lock;

	struct cdev		 cdev;

	int			 irq;
};

static struct adc_device *adc_devices[MVF_ADC_MAX_DEVICES];

struct adc_touch_device {
	struct platform_device	*pdev;

	bool stop_touchscreen;

	int pen_irq;
	struct input_dev	*ts_input;
	struct workqueue_struct *ts_workqueue;
	struct work_struct	ts_work;
};

struct data {
	unsigned int res_value;
	bool flag;
};

struct adc_touch_device *touch;

struct data data_array[7];

#define adc_dbg(_adc, msg...) dev_dbg(&(_adc)->pdev->dev, msg)

static int res_proc(struct adc_device *adc);
struct adc_client *adc_register(struct platform_device *pdev,
		unsigned char channel);

/* select channel and enable conversion  */
static inline void adc_convert(struct adc_device *adc,
		struct adc_client *client)
{

	unsigned con = readl(adc->regs + ADC_HC0);
	con  = ADCHC0(client->channel);
	con |= IRQ_EN;
	writel(con, adc->regs + ADC_HC0);
}

/* find next conversion client */
static void adc_try(struct adc_device *adc)
{
	struct adc_client *next = adc->cur;
	if (!list_empty(&client_list)) {
		next = list_entry(client_list.next,
				struct adc_client, list);
		list_del(client_list.next);

		adc->cur = next;
		adc_convert(adc, adc->cur);
	}
}

int adc_initiate(struct adc_device *adc_dev)
{
	unsigned long reg, tmp, pin;
	struct adc_device *adc = adc_dev;


	/* PCTL: pin control 5 for Sliding rheostat */
	pin = readl(adc->regs+ADC_PCTL);
	pin &= ~CLEPCTL23;
	pin &= SETPCTL5;
	writel(pin, adc->regs+ADC_PCTL);

	/* CFG: Feature set */
	reg = readl(adc->regs+ADC_CFG);
	reg &= ~CLECFG16;
	reg |= SETCFG;
	writel(reg, adc->regs+ADC_CFG);

	/* GC: software trigger */
	tmp = readl(adc->regs+ADC_GC);
	tmp = 0;
	writel(tmp, adc->regs+ADC_GC);

	return 0;
}

static int adc_set(struct adc_device *adc_dev, struct adc_feature *adc_fea)
{
	struct adc_device *adc = adc_dev;
	int con, res;
	con = readl(adc->regs+ADC_CFG);
	res = readl(adc->regs+ADC_GC);

	/* clock select and clock devide */
	switch (adc_fea->clk_sel) {
	case ADCIOC_BUSCLK_SET:
			/* clear 1-0,6-5 */
		con &= ~CLEAR_CLK_BIT;
		switch (adc_fea->clk_div_num) {
		case 1:
			break;
		case 2:
			con |= CLK_DIV2;
			break;
		case 4:
			con |= CLK_DIV4;
			break;
		case 8:
			con |= CLK_DIV8;
			break;
		case 16:
			con |= BUSCLK2_SEL | CLK_DIV8;
			break;
		default:
			return -EINVAL;
	}
	writel(con, adc->regs+ADC_CFG);

	break;

	case ADCIOC_ALTCLK_SET:
	/* clear 1-0,6-5 */
		con &= ~CLEAR_CLK_BIT;

		con |= ALTCLK_SEL;
	switch (adc_fea->clk_div_num) {
	case 1:
		break;
	case 2:
		con |= CLK_DIV2;
		break;
	case 4:
		con |= CLK_DIV4;
		break;
	case 8:
		con |= CLK_DIV8;
		break;
	default:
		return -EINVAL;
	}
	writel(con, adc->regs+ADC_CFG);

	break;

	case ADCIOC_ADACK_SET:
		/* clear 1-0,6-5 */
		con &= ~CLEAR_CLK_BIT;

		con |= ADACK_SEL;
		switch (adc_fea->clk_div_num) {
		case 1:
			break;
		case 2:
			con |= CLK_DIV2;
			break;
		case 4:
			con |= CLK_DIV4;
			break;
		case 8:
			con |= CLK_DIV8;
			break;
		default:
			return -EINVAL;
		}
		writel(con, adc->regs+ADC_CFG);
	break;

	default:
		pr_debug("adc_ioctl: unsupported ioctl command 0x%x\n",
				adc_fea->clk_sel);
		return -EINVAL;
	}

	/* resolution mode,clear 3-2 */
	con &= ~CLEAR_MODE_BIT;
	switch (adc_fea->res_mode) {
	case 8:
		break;
	case 10:
		con |= BIT10;
		break;
	case 12:
		con |= BIT12;
		break;
	default:
		pr_debug("adc_ioctl: unsupported ioctl resolution mode 0x%x\n",
				adc_fea->res_mode);
		return -EINVAL;
	}
	writel(con, adc->regs+ADC_CFG);

	/* Defines the sample time duration */
	/* clear 4, 9-8 */
	con &= ~CLEAR_LSTC_BIT;
	switch (adc_fea->sam_time) {
	case 2:
		break;
	case 4:
		con |= ADSTS_SHORT;
		break;
	case 6:
		con |= ADSTS_NORMAL;
		break;
	case 8:
		con |= ADSTS_LONG;
		break;
	case 12:
		con |= ADLSMP_LONG;
		break;
	case 16:
		con |= ADLSMP_LONG | ADSTS_SHORT;
		break;
	case 20:
		con |= ADLSMP_LONG | ADSTS_NORMAL;
		break;
	case 24:
		con |= ADLSMP_LONG | ADSTS_LONG;
		break;
	default:
		pr_debug("adc_ioctl: unsupported ioctl sample"
				"time duration 0x%x\n",
				adc_fea->sam_time);
		return -EINVAL;
	}
	writel(con, adc->regs+ADC_CFG);

	/* low power configuration */
	/* */
	switch (adc_fea->lp_con) {
	case ADCIOC_LPOFF_SET:
		con &= ~CLEAR_ADLPC_BIT;
		writel(con, adc->regs+ADC_CFG);
		break;

	case ADCIOC_LPON_SET:
		con &= ~CLEAR_ADLPC_BIT;
		con |= ADLPC_EN;
		writel(con, adc->regs+ADC_CFG);
		break;
	default:
		return -EINVAL;

	}
	/* high speed operation */
	switch (adc_fea->hs_oper) {
	case ADCIOC_HSON_SET:
		con &= ~CLEAR_ADHSC_BIT;
		con |= ADHSC_EN;
		writel(con, adc->regs+ADC_CFG);
		break;

	case ADCIOC_HSOFF_SET:
		con &= ~CLEAR_ADHSC_BIT;
		writel(con, adc->regs+ADC_CFG);
		break;
	default:
		return -EINVAL;
	}

	/* voltage reference*/
	switch (adc_fea->vol_ref) {
	case ADCIOC_VR_VREF_SET:
		con &= ~CLEAR_REFSEL_BIT;
		writel(con, adc->regs+ADC_CFG);
		break;

	case ADCIOC_VR_VALT_SET:
		con &= ~CLEAR_REFSEL_BIT;
		con |= REFSEL_VALT;
		writel(con, adc->regs+ADC_CFG);
		break;

	case ADCIOC_VR_VBG_SET:
		con &= ~CLEAR_REFSEL_BIT;
		con |= REFSEL_VBG;
		writel(con, adc->regs+ADC_CFG);
		break;
	default:
		return -EINVAL;
	}

	/* trigger select */
	switch (adc_fea->tri_sel) {
	case ADCIOC_SOFTTS_SET:
		con &= ~CLEAR_ADTRG_BIT;
		writel(con, adc->regs+ADC_CFG);
		break;

	case ADCIOC_HARDTS_SET:
		con &= ~CLEAR_ADTRG_BIT;
		con |= ADTRG_HARD;
		writel(con, adc->regs+ADC_CFG);
		break;
	default:
		return -EINVAL;
	}

	/* hardware average select */
	switch (adc_fea->ha_sel) {
	case ADCIOC_HA_DIS:
		res &= ~CLEAR_AVGE_BIT;
		writel(con, adc->regs+ADC_GC);
		break;

	case ADCIOC_HA_SET:
		con &= ~CLEAR_AVGS_BIT;
		switch (adc_fea->ha_sam) {
		case 4:
			break;
		case 8:
			con |= AVGS_8;
			break;
		case 16:
			con |= AVGS_16;
			break;
		case 32:
			con |= AVGS_32;
			break;
		default:
			return -EINVAL;
		}
		res &= ~CLEAR_AVGE_BIT;
		res |= AVGEN;
		writel(con, adc->regs+ADC_CFG);
		writel(res, adc->regs+ADC_GC);

		break;
	default:
		return -EINVAL;
	}

	/* data overwrite enable */
	switch (adc_fea->do_ena) {
	case ADCIOC_DOEON_SET:
		con &= ~CLEAR_OVWREN_BIT;
		writel(con, adc->regs+ADC_CFG);
		break;

	case ADCIOC_DOEOFF_SET:
		con |= OVWREN;
		writel(con, adc->regs+ADC_CFG);
		break;
	default:
		return -EINVAL;
	}

	/* Asynchronous clock output enable */
	switch (adc_fea->ac_ena) {
	case ADCIOC_ADACKENON_SET:
		res &= ~CLEAR_ADACKEN_BIT;
		writel(res, adc->regs+ADC_GC);
		break;

	case ADCIOC_ADACKENOFF_SET:
		res &= ~CLEAR_ADACKEN_BIT;
		res |= ADACKEN;
		writel(res, adc->regs+ADC_GC);
		break;
	default:
		return -EINVAL;
	}

	/* dma enable */
	switch (adc_fea->dma_ena) {
	case ADCIDC_DMAON_SET:
		res &= ~CLEAR_DMAEN_BIT;
		writel(res, adc->regs+ADC_GC);
		break;

	case ADCIDC_DMAOFF_SET:
		res &= ~CLEAR_DMAEN_BIT;
		res |= DMAEN;
		writel(res, adc->regs+ADC_GC);
		break;
	default:
		return -EINVAL;
	}

	/* continue function enable */
	switch (adc_fea->cc_ena) {
	case ADCIOC_CCEOFF_SET:
		res &= ~CLEAR_ADCO_BIT;
		writel(res, adc->regs+ADC_GC);
		break;

	case ADCIOC_CCEON_SET:
		res &= ~CLEAR_ADCO_BIT;
		res |= ADCON;
		writel(res, adc->regs+ADC_GC);
		break;
	default:
		return -EINVAL;
	}

	/* compare function enable */
	switch (adc_fea->compare_func_ena) {
	case ADCIOC_ACFEON_SET:
		res &= ~CLEAR_ACFE_BIT;
		res |= ACFE;
		writel(res, adc->regs+ADC_GC);
		break;

	case ADCIOC_ACFEOFF_SET:
		res &= ~CLEAR_ACFE_BIT;
		writel(res, adc->regs+ADC_GC);
		break;
	default:
		return -EINVAL;
	}

	/* greater than enable */
	switch (adc_fea->greater_ena) {
	case ADCIOC_ACFGTON_SET:
		res &= ~CLEAR_ACFGT_BIT;
		res |= ACFGT;
		writel(res, adc->regs+ADC_GC);
		break;

	case ADCIOC_ACFGTOFF_SET:
		res &= ~CLEAR_ACFGT_BIT;
		writel(res, adc->regs+ADC_GC);
		break;
	default:
		return -EINVAL;
	}

	/* range enable */
	switch (adc_fea->range_ena) {
	case ADCIOC_ACRENON_SET:
		res &= ~CLEAR_ACREN_BIT;
		res |= ACREN;
		writel(res, adc->regs+ADC_GC);
		break;

	case ADCIOC_ACRENOFF_SET:
		res &= ~CLEAR_ACREN_BIT;
		writel(res, adc->regs+ADC_GC);
		break;

	default: return -ENOTTY;
	}
	return 0;
}

static int adc_open(struct inode *inode, struct file *file)
{
	struct adc_device *dev = container_of(inode->i_cdev,
			struct adc_device, cdev);

	file->private_data = dev;

	return 0;
}

static long adc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct adc_device *adc_dev = file->private_data;
	void __user *argp = (void __user *)arg;
	struct adc_feature feature;
	int channel;

	if (_IOC_TYPE(cmd) != 'p')
		return -ENOTTY;

	if (copy_from_user(&feature, (struct adc_feature *)argp,
				sizeof(feature))) {
		return -EFAULT;
	}

	switch (cmd) {
	case ADC_INIT:
		adc_initiate(adc_dev);
		break;

	case ADC_CONFIGURATION:

		adc_set(adc_dev, &feature);
		break;

	case ADC_REG_CLIENT:
		channel = feature.channel;
		adc_register(adc_dev->pdev, channel);
		break;

	case ADC_CONVERT:
		INIT_COMPLETION(adc_tsi);
		adc_try(adc_dev);
		wait_for_completion_interruptible(&adc_tsi);
		if (data_array[feature.channel].flag) {
			feature.result0 = data_array[feature.channel].res_value;
			data_array[feature.channel].flag = 0;
		}
		if (copy_to_user((struct adc_feature *)argp, &feature,
				sizeof(feature)))
			return -EFAULT;

		kfree(adc_dev->cur);
		break;

	default:
		pr_debug("adc_ioctl: unsupported ioctl command 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

/* client register */
struct adc_client *adc_register(struct platform_device *pdev,
		unsigned char channel)
{
	struct adc_client *client = kzalloc(sizeof(struct adc_client),
			GFP_KERNEL);

	WARN_ON(!pdev);

	if (!pdev)
		return ERR_PTR(-EINVAL);


	if (!client) {
		dev_err(&pdev->dev, "no memory for adc client\n");
		return ERR_PTR(-ENOMEM);
	}

	client->pdev = pdev;
	client->channel = channel;
	list_add(&client->list, &client_list);

	return client;
}


/*result process */
static int res_proc(struct adc_device *adc)
{
	int con, res;
	con = readl(adc->regs + ADC_CFG);

	if ((con & (1 << 2)) == 0) {
		if ((con & (1 << 3)) == 1)
			res = (0xFFF & readl(adc->regs + ADC_R0));
		else
			res = (0xFF & readl(adc->regs + ADC_R0));
	} else
		res = (0x3FF & readl(adc->regs + ADC_R0));

	return readl(adc->regs + ADC_R0);
	return res;
}

static irqreturn_t adc_irq(int irq, void *pw)
{
	int coco;
	struct adc_device *adc = pw;
	struct adc_client *client = adc->cur;

	if (!client) {
		dev_warn(&adc->pdev->dev, "%s: no adc pending\n", __func__);
		goto exit;
	}

	coco = readl(adc->regs + ADC_HS);
	if (coco & 1) {
		data_array[client->channel].res_value = res_proc(adc);
		data_array[client->channel].flag = 1;
		complete(&adc_tsi);
	}

exit:
	return IRQ_NONE;
}

static const struct file_operations adc_fops = {
	.owner			= THIS_MODULE,
	.open			= adc_open,
	.unlocked_ioctl		= adc_ioctl,
	.read			= NULL,
};

/*
 * Enables given plates and measures touch parameters using ADC
 */
static int adc_ts_measure(int plate1, int plate2, int adc, int adc_channel)
{
	int i, value = 0;
	col_ts_enable_wire(plate1);
	col_ts_enable_wire(plate2);

	/* Use hrtimer sleep since msleep sleeps 10ms+ */
	usleep_range(COLI_TOUCH_MIN_DELAY_US, COLI_TOUCH_MAX_DELAY_US);

	for (i = 0; i < 5; i++) {
		adc_register(adc_devices[adc]->pdev, adc_channel);

		INIT_COMPLETION(adc_tsi);
		adc_try(adc_devices[adc]);
		wait_for_completion(&adc_tsi);

		if (!data_array[adc_channel].flag)
			return -EINVAL;

		value += data_array[adc_channel].res_value;
		data_array[adc_channel].flag = 0;
	}

	value /= 5;

	col_ts_disable_wire(plate1);
	col_ts_disable_wire(plate2);

	return value;
}

static void adc_ts_enable_touch_detection(struct adc_touch_device *adc_ts)
{
	struct colibri_ts_platform_data *pdata = adc_ts->pdev->dev.platform_data;

	/* Enable plate YM (needs to be strong 0) */
	col_ts_enable_wire(COL_TS_WIRE_YM);

	/* Let the platform mux to GPIO in order to enable Pull-Up on GPIO */
	if (pdata->mux_pen_interrupt)
		pdata->mux_pen_interrupt(adc_ts->pdev);
}

static void adc_ts_work(struct work_struct *ts_work)
{
	struct adc_touch_device *adc_ts = container_of(ts_work,
				struct adc_touch_device, ts_work);
	struct device *dev = &adc_ts->pdev->dev;
	int val_x, val_y, val_z1, val_z2, val_p = 0;

	struct adc_feature feature = {
		.channel = 0,
		.clk_sel = ADCIOC_BUSCLK_SET,
		.clk_div_num = 1,
//		.res_mode = 12,
		.ha_sam = 32,
		.sam_time = 24,
		.hs_oper = ADCIOC_HSOFF_SET
	};

	adc_initiate(adc_devices[0]);
	adc_set(adc_devices[0], &feature);

	adc_initiate(adc_devices[1]);
	adc_set(adc_devices[1], &feature);


	while (!adc_ts->stop_touchscreen)
	{
		/* X-Direction */
		val_x = adc_ts_measure(COL_TS_WIRE_XP, COL_TS_WIRE_XM, 1, 0);
		if (val_x < 0)
			continue;

		/* Y-Direction */
		val_y = adc_ts_measure(COL_TS_WIRE_YP, COL_TS_WIRE_YM, 0, 0);
		if (val_y < 0)
			continue;

		/* Touch pressure
		 * Measure on XP/YM
		 */
		val_z1 = adc_ts_measure(COL_TS_WIRE_YP, COL_TS_WIRE_XM, 0, 1);
		if (val_z1 < 0)
			continue;
		val_z2 = adc_ts_measure(COL_TS_WIRE_YP, COL_TS_WIRE_XM, 1, 2);
		if (val_z2 < 0)
			continue;

		/* According to datasheet of our touchscreen, 
		 * resistance on X axis is 400~1200.. 
		 */
	//	if ((val_z2 - val_z1) < (MVF_ADC_MAX - (1<<9)) {
		/* Validate signal (avoid calculation using noise) */
		if (val_z1 > 64 && val_x > 64) {
			/* Calculate resistance between the plates
			 * lower resistance means higher pressure */
			int r_x = (1000 * val_x) / MVF_ADC_MAX;
			val_p = (r_x * val_z2) / val_z1 - r_x;
		} else {
			val_p = 2000;
		}
		/*
		dev_dbg(dev, "Measured values: x: %d, y: %d, z1: %d, z2: %d, "
			"p: %d\n", val_x, val_y, val_z1, val_z2, val_p);
*/
		/* If difference of the AD levels of the two plates is near
		 * the maximum, there is no touch..
		 */
		/*
		 * If touch pressure is too low, stop measuring and reenable
		 * touch detection
		 */
		if (val_p > 1050)
			break;

		/* Report touch position and sleep for next measurement */
		input_report_abs(adc_ts->ts_input, ABS_X, MVF_ADC_MAX - val_x);
		input_report_abs(adc_ts->ts_input, ABS_Y, MVF_ADC_MAX - val_y);
		input_report_abs(adc_ts->ts_input, ABS_PRESSURE, 2000 - val_p);
		input_report_key(adc_ts->ts_input, BTN_TOUCH, 1);
		input_sync(adc_ts->ts_input);

		msleep(10);
	}

	/* Report no more touch, reenable touch detection */
	input_report_abs(adc_ts->ts_input, ABS_PRESSURE, 0);
	input_report_key(adc_ts->ts_input, BTN_TOUCH, 0);
	input_sync(adc_ts->ts_input);

	/* Wait the pull-up to be stable on high */
	adc_ts_enable_touch_detection(adc_ts);
	msleep(10);

	/* Reenable IRQ to detect touch */
	enable_irq(adc_ts->pen_irq);

	dev_dbg(dev, "Reenabled touch detection interrupt\n");
}

static irqreturn_t adc_tc_touched(int irq, void *dev_id)
{
	struct adc_touch_device *adc_ts = (struct adc_touch_device *)dev_id;
	struct device *dev = &adc_ts->pdev->dev;
	struct colibri_ts_platform_data *pdata = adc_ts->pdev->dev.platform_data;

	dev_dbg(dev, "Touch detected, start worker thread\n");

	/* Stop IRQ */
	disable_irq_nosync(irq);

	/* Disable the touch detection plates */
	col_ts_disable_wire(COL_TS_WIRE_YM);

	/* Let the platform mux to GPIO in order to enable Pull-Up on GPIO */
	if (pdata->mux_adc)
		pdata->mux_adc(adc_ts->pdev);

	/* Start worker thread */
	queue_work(adc_ts->ts_workqueue, &adc_ts->ts_work);

	return IRQ_HANDLED;
}

static int adc_ts_open(struct input_dev *dev_input)
{
	int ret;
	struct adc_touch_device *adc_ts = input_get_drvdata(dev_input);
	struct device *dev = &adc_ts->pdev->dev;
	struct colibri_ts_platform_data *pdata = adc_ts->pdev->dev.platform_data;

	dev_dbg(dev, "Input device %s opened, starting touch detection\n", 
			dev_input->name);

	adc_ts->stop_touchscreen = false;

	/* Initialize GPIOs, leave FETs closed by default */
	col_ts_init_wire(COL_TS_WIRE_XP);
	col_ts_init_wire(COL_TS_WIRE_XM);
	col_ts_init_wire(COL_TS_WIRE_YP);
	col_ts_init_wire(COL_TS_WIRE_YM);

	/* Mux detection before request IRQ */
	adc_ts_enable_touch_detection(adc_ts);

	adc_ts->pen_irq = gpio_to_irq(pdata->gpio_pen);
	if (adc_ts->pen_irq < 0) {
		dev_err(dev, "Unable to get IRQ for GPIO %d\n", pdata->gpio_pen);
		return adc_ts->pen_irq;
	}

	ret = request_irq(adc_ts->pen_irq, adc_tc_touched, IRQF_TRIGGER_FALLING,
			"touch detected", adc_ts);
	if (ret < 0) {
		dev_err(dev, "Unable to request IRQ %d\n", adc_ts->pen_irq);
		return ret;
	}

	return 0;
}

static void adc_ts_close(struct input_dev *dev_input)
{
	struct adc_touch_device *adc_ts = input_get_drvdata(dev_input);
	struct device *dev = &adc_ts->pdev->dev;

	free_irq(gpio_to_irq(col_ts_gpios[COL_TS_GPIO_TOUCH].gpio), adc_ts);

	adc_ts->stop_touchscreen = true;

	/* Wait until touchscreen thread finishes any possible remnants. */
	cancel_work_sync(&adc_ts->ts_work);

	dev_dbg(dev, "Input device %s closed, disable touch detection\n", 
			dev_input->name);
}


static int __devinit adc_ts_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	struct adc_touch_device *adc_ts;

	adc_ts = kzalloc(sizeof(struct adc_touch_device), GFP_KERNEL);
	if (!adc_ts) {
		dev_err(dev, "Failed to allocate TS device!\n");
		return -ENOMEM;
	}

	adc_ts->pdev = pdev;

	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "Failed to allocate TS input device!\n");
		ret = -ENOMEM;
		goto err_input_allocate;
	}

	input->name = DRIVER_NAME;
	input->id.bustype = BUS_HOST;
	input->dev.parent = dev;
	input->open = adc_ts_open;
	input->close = adc_ts_close;

	__set_bit(EV_ABS, input->evbit);
	__set_bit(EV_KEY, input->evbit);
	__set_bit(BTN_TOUCH, input->keybit);
	input_set_abs_params(input, ABS_X, 0, MVF_ADC_MAX, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, MVF_ADC_MAX, 0, 0);
	input_set_abs_params(input, ABS_PRESSURE, 0, MVF_ADC_MAX, 0, 0);

	adc_ts->ts_input = input;
	input_set_drvdata(input, adc_ts);
	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "failed to register input device\n");
		goto err;
	}

	/* Create workqueue for ADC sampling and calculation */
	INIT_WORK(&adc_ts->ts_work, adc_ts_work);
	adc_ts->ts_workqueue = create_singlethread_workqueue("mvf-adc-touch");

	if (!adc_ts->ts_workqueue) {
		dev_err(dev, "failed to create workqueue");
		goto err;
	}

	/* Request GPIOs for FETs and touch detection */
	ret = gpio_request_array(col_ts_gpios, COL_TS_GPIO_PULLUP + 1);
	if (ret) {
		dev_err(dev, "failed to request GPIOs\n");
		goto err;
	}

	return 0;
err:
	input_free_device(touch->ts_input);

err_input_allocate:
	kfree(adc_ts);

	return ret;
}

static int __devexit adc_ts_remove(struct platform_device *pdev)
{
	struct adc_touch_device *adc_ts = platform_get_drvdata(pdev);

	input_unregister_device(adc_ts->ts_input);

	destroy_workqueue(adc_ts->ts_workqueue);
	kfree(adc_ts->ts_input);
	kfree(adc_ts);

	return 0;
}


/* probe */
static int __devinit adc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct adc_device *adc;
	struct resource *regs;
	int ret;


	adc = kzalloc(sizeof(struct adc_device), GFP_KERNEL);
	if (adc == NULL) {
		dev_err(dev, "failed to allocate adc_device\n");
		return -ENOMEM;
	}
	adc->pdev = pdev;

	/* Initialize spin_lock */
	spin_lock_init(&adc->lock);

	adc->irq = platform_get_irq(pdev, 0);
	if (adc->irq <= 0) {
		dev_err(dev, "failed to get adc irq\n");
		ret = -EINVAL;
		goto err_alloc;
	}

	ret = request_irq(adc->irq, adc_irq, 0, dev_name(dev), adc);
	if (ret < 0) {
		dev_err(dev, "failed to attach adc irq\n");
		goto err_alloc;
	}

	adc->clk = clk_get(dev, NULL);
	if (IS_ERR(adc->clk)) {
		dev_err(dev, "failed to get adc clock\n");
		ret = PTR_ERR(adc->clk);
		goto err_irq;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(dev, "failed to find registers\n");
		ret = -ENXIO;
		goto err_clk;
	}

	adc->regs = ioremap(regs->start, resource_size(regs));
	if (!adc->regs) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto err_clk;
	}


	cdev_init(&adc->cdev, &adc_fops);
	adc->cdev.owner = THIS_MODULE;
	ret = cdev_add(&adc->cdev, MKDEV(mvf_adc_major, pdev->id), 1);
	if (ret < 0)
		return ret;

	if (IS_ERR(device_create(adc_class, &pdev->dev,
				 MKDEV(mvf_adc_major, pdev->id),
				 NULL, "mvf-adc.%d", pdev->id)))
		dev_err(dev, "failed to create device\n");

	/* clk enable */
	clk_enable(adc->clk);
	/* Associated structures */
	platform_set_drvdata(pdev, adc);

	/* Save device structure by Platform device ID for touch */
	adc_devices[pdev->id] = adc;

	dev_info(dev, "attached adc driver\n");

	return 0;

err_clk:
	clk_put(adc->clk);

err_irq:
	free_irq(adc->irq, adc);

err_alloc:
	kfree(adc);

	return ret;
}

static int __devexit adc_remove(struct platform_device *pdev)
{
	struct adc_device *adc = platform_get_drvdata(pdev);

	iounmap(adc->regs);
	free_irq(adc->irq, adc);
	clk_disable(adc->clk);
	clk_put(adc->clk);
	kfree(adc);

	return 0;
}

static struct platform_driver adc_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= adc_probe,
	.remove		= __devexit_p(adc_remove),
};

static struct platform_driver adc_ts_driver = {
	.driver		= {
		.name	= DRIVER_TS_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= adc_ts_probe,
	.remove		= __devexit_p(adc_ts_remove),
};

static int __init adc_init(void)
{
	int ret;
	dev_t dev;

	adc_class = class_create(THIS_MODULE, "mvf-adc");
	if (IS_ERR(adc_class)) {
		ret = PTR_ERR(adc_class);
		printk(KERN_ERR "%s: can't register mvf-adc class\n",__func__);
		goto err;
	}

	/* Obtain device numbers and register char device */
	ret = alloc_chrdev_region(&dev, 0, MVF_ADC_MAX_DEVICES, "mvf-adc");
	if (ret)
	{
		printk(KERN_ERR "%s: can't register character device\n", 
				__func__);
		goto err_class;
	}
	mvf_adc_major = MAJOR(dev);

	ret = platform_driver_register(&adc_driver);
	if (ret)
		printk(KERN_ERR "%s: failed to add adc driver\n", __func__);

	ret = platform_driver_register(&adc_ts_driver);
	if (ret)
		printk(KERN_ERR "%s: failed to add adc touchscreen driver\n", 
				__func__);

err_class:
	class_destroy(adc_class);
err:
	return ret;
}

static void __exit adc_exit(void)
{
	platform_driver_unregister(&adc_ts_driver);
	platform_driver_unregister(&adc_driver);
	class_destroy(adc_class);
}
module_init(adc_init);
module_exit(adc_exit);

MODULE_AUTHOR("Xiaojun Wang, Stefan Agner");
MODULE_DESCRIPTION("Vybrid ADC driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
