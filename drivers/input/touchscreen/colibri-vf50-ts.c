/* Copyright 2013 Toradex AG
 *
 * Toradex Colibri VF50 Touchscreen driver
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
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/mvf_adc.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/colibri-ts.h>

#define DRIVER_NAME "colibri-vf50-ts"
#define DRV_VERSION "1.0"

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

struct adc_touch_device {
	struct platform_device	*pdev;

	bool stop_touchscreen;

	int pen_irq;
	struct input_dev	*ts_input;
	struct workqueue_struct *ts_workqueue;
	struct work_struct	ts_work;
};

struct adc_touch_device *touch;

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
		int ret = mvf_adc_register_and_convert(adc, adc_channel);
		if (ret < 0)
			return -EINVAL;

		value += ret;
	}

	value /= 5;

	col_ts_disable_wire(plate1);
	col_ts_disable_wire(plate2);

	return value;
}

/*
 * Enable touch detection using falling edge detection on XM 
 */
static void adc_ts_enable_touch_detection(struct adc_touch_device *adc_ts)
{
	struct colibri_ts_platform_data *pdata = adc_ts->pdev->dev.platform_data;

	/* Enable plate YM (needs to be strong 0) */
	col_ts_enable_wire(COL_TS_WIRE_YM);

	/* Let the platform mux to GPIO in order to enable Pull-Up on GPIO */
	if (pdata->mux_pen_interrupt)
		pdata->mux_pen_interrupt(adc_ts->pdev);
}

/*
 * ADC touch screen sampling worker function
 */
static void adc_ts_work(struct work_struct *ts_work)
{
	struct adc_touch_device *adc_ts = container_of(ts_work,
				struct adc_touch_device, ts_work);
	struct device *dev = &adc_ts->pdev->dev;
	int val_x, val_y, val_z1, val_z2, val_p = 0;

	struct adc_feature feature = {
		.channel = ADC0,
		.clk_sel = ADCIOC_BUSCLK_SET,
		.clk_div_num = 1,
		.res_mode = 12,
		.sam_time = 6,
		.lp_con = ADCIOC_LPOFF_SET,
		.hs_oper = ADCIOC_HSOFF_SET,
		.vol_ref = ADCIOC_VR_VREF_SET,
		.tri_sel = ADCIOC_SOFTTS_SET,
		.ha_sel = ADCIOC_HA_SET,
		.ha_sam = 8,
		.do_ena = ADCIOC_DOEOFF_SET,
		.ac_ena = ADCIOC_ADACKENOFF_SET,
		.dma_ena = ADCIDC_DMAOFF_SET,
		.cc_ena = ADCIOC_CCEOFF_SET,
		.compare_func_ena = ADCIOC_ACFEOFF_SET,
		.range_ena = ADCIOC_ACRENOFF_SET,
		.greater_ena = ADCIOC_ACFGTOFF_SET,
		.result0 = 0,
		.result1 = 0,
	};

	mvf_adc_initiate(0);
	mvf_adc_set(0, &feature);

	mvf_adc_initiate(1);
	mvf_adc_set(1, &feature);

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

	/* Mux detection before request IRQ, wait for pull-up to settle */
	adc_ts_enable_touch_detection(adc_ts);
	msleep(10);

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

	dev_info(dev, "attached driver successfully\n");

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

static struct platform_driver adc_ts_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= adc_ts_probe,
	.remove		= __devexit_p(adc_ts_remove),
};

static int __init adc_ts_init(void)
{
	int ret;

	ret = platform_driver_register(&adc_ts_driver);
	if (ret)
		printk(KERN_ERR "%s: failed to add adc touchscreen driver\n", 
				__func__);

	return ret;
}

static void __exit adc_ts_exit(void)
{
	platform_driver_unregister(&adc_ts_driver);
}
module_init(adc_ts_init);
module_exit(adc_ts_exit);

MODULE_AUTHOR("Stefan Agner");
MODULE_DESCRIPTION("Colibri VF50 Touchscreen driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
