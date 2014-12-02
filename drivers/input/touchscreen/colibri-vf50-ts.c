/* Copyright 2014 Toradex AG
 *
 * Toradex Colibri VF50 Touchscreen driver
 *
 * Originally authored by Stefan Agner. Taken from the colibri_vf branch
 * at git.toradex.com/cgit/linux-toradex.git from the driver of the same
 * name and ported for mainline.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/pinctrl/consumer.h>

#define DRIVER_NAME "colibri-vf50-ts"
#define DRV_VERSION "1.0"

#define VF_ADC_MAX ((1 << 12) - 1)

#define COLI_TOUCH_MIN_DELAY_US 1000
#define COLI_TOUCH_MAX_DELAY_US 2000

static int min_pressure = 200;

struct vf50_touch_device {
	struct platform_device	*pdev;
	struct input_dev		*ts_input;
	struct workqueue_struct *ts_workqueue;
	struct work_struct		ts_work;
	struct iio_channel		*channels;
	int pen_irq;
	int gpio_xp;
	int gpio_xm;
	int gpio_yp;
	int gpio_ym;
	int gpio_pen_detect;
	int gpio_pen_detect_pullup;
	bool stop_touchscreen;
};

/*
 * Enables given plates and measures touch parameters using ADC
 */
static int adc_ts_measure(struct iio_channel *channel, int plate_p, int plate_m)
{
	int i, value = 0, val = 0;
	int ret;

	gpio_set_value(plate_p, 0); /* Low active */
	gpio_set_value(plate_m, 1); /* High active */

	/* Use hrtimer sleep since msleep sleeps 10ms+ */
	usleep_range(COLI_TOUCH_MIN_DELAY_US, COLI_TOUCH_MAX_DELAY_US);

	for (i = 0; i < 5; i++) {
		ret = iio_read_channel_raw(channel, &val);
		if (ret < 0)
			return -EINVAL;

		value += val;
	}

	value /= 5;

	gpio_set_value(plate_p, 1);
	gpio_set_value(plate_m, 0);

	return value;
}

/*
 * Enable touch detection using falling edge detection on XM
 */
static void vf50_ts_enable_touch_detection(struct vf50_touch_device *vf50_ts)
{
	/* Enable plate YM (needs to be strong GND, high active) */
	gpio_set_value(vf50_ts->gpio_ym, 1);

	/* Let the platform mux to GPIO in order to enable Pull-Up on GPIO */
	pinctrl_pm_select_idle_state(&vf50_ts->pdev->dev);
}

/*
 * ADC touch screen sampling worker function
 */
static void vf50_ts_work(struct work_struct *ts_work)
{
	struct vf50_touch_device *vf50_ts = container_of(ts_work,
				struct vf50_touch_device, ts_work);
	struct device *dev = &vf50_ts->pdev->dev;
	int val_x, val_y, val_z1, val_z2, val_p = 0;
	bool discard_val_on_start = true;

	while (!vf50_ts->stop_touchscreen) {
		/* X-Direction */
		val_x = adc_ts_measure(&vf50_ts->channels[0], vf50_ts->gpio_xp, vf50_ts->gpio_xm);
		if (val_x < 0)
			continue;

		/* Y-Direction */
		val_y = adc_ts_measure(&vf50_ts->channels[1], vf50_ts->gpio_yp, vf50_ts->gpio_ym);
		if (val_y < 0)
			continue;

		/* Touch pressure
		 * Measure on XP/YM
		 */
		val_z1 = adc_ts_measure(&vf50_ts->channels[2], vf50_ts->gpio_yp, vf50_ts->gpio_xm);
		if (val_z1 < 0)
			continue;
		val_z2 = adc_ts_measure(&vf50_ts->channels[3], vf50_ts->gpio_yp, vf50_ts->gpio_xm);
		if (val_z2 < 0)
			continue;

		/* According to datasheet of our touchscreen,
		 * resistance on X axis is 400~1200..
		 */
		/* Validate signal (avoid calculation using noise) */
		if (val_z1 > 64 && val_x > 64) {
			/* Calculate resistance between the plates
			 * lower resistance means higher pressure */
			int r_x = (1000 * val_x) / VF_ADC_MAX;
			val_p = (r_x * val_z2) / val_z1 - r_x;
		} else {
			val_p = 2000;
		}

		val_p = 2000 - val_p;
		dev_dbg(dev, "Measured values: x: %d, y: %d, z1: %d, z2: %d, "
			"p: %d\n", val_x, val_y, val_z1, val_z2, val_p);

		/*
		 * If touch pressure is too low, stop measuring and reenable
		 * touch detection
		 */

		if (val_p < min_pressure || val_p > 2000)
			break;

		/*
		 * The pressure may not be enough for the first x and the second
		 * y measurement, but, the pressure is ok when the driver is then
		 * third and fourth measurement. To take care of this, we drop the
		 * first measurement always
		 */

		if (discard_val_on_start) {
			discard_val_on_start = false;
		} else {
			/* Report touch position and sleep for next measurement */
			input_report_abs(vf50_ts->ts_input, ABS_X, VF_ADC_MAX - val_x);
			input_report_abs(vf50_ts->ts_input, ABS_Y, VF_ADC_MAX - val_y);
			input_report_abs(vf50_ts->ts_input, ABS_PRESSURE, val_p);
			input_report_key(vf50_ts->ts_input, BTN_TOUCH, 1);
			input_sync(vf50_ts->ts_input);
		}

		msleep(10);
	}

	/* Report no more touch, reenable touch detection */
	input_report_abs(vf50_ts->ts_input, ABS_PRESSURE, 0);
	input_report_key(vf50_ts->ts_input, BTN_TOUCH, 0);
	input_sync(vf50_ts->ts_input);

	/* Wait the pull-up to be stable on high */
	vf50_ts_enable_touch_detection(vf50_ts);
	msleep(10);

	/* Reenable IRQ to detect touch */
	enable_irq(vf50_ts->pen_irq);

	dev_dbg(dev, "Reenabled touch detection interrupt\n");
}

static irqreturn_t vf50_tc_touched(int irq, void *dev_id)
{
	struct vf50_touch_device *vf50_ts = (struct vf50_touch_device *)dev_id;
	struct device *dev = &vf50_ts->pdev->dev;

	dev_dbg(dev, "Touch detected, start worker thread\n");

	/* Stop IRQ */
	disable_irq_nosync(irq);

	/* Disable the touch detection plates */
	gpio_set_value(vf50_ts->gpio_ym, 0);

	/* Let the platform mux to GPIO in order to enable Pull-Up on GPIO */
	pinctrl_pm_select_default_state(dev);

	/* Start worker thread */
	queue_work(vf50_ts->ts_workqueue, &vf50_ts->ts_work);

	return IRQ_HANDLED;
}

static int vf50_ts_open(struct input_dev *dev_input)
{
	int ret;
	struct vf50_touch_device *touchdev = input_get_drvdata(dev_input);
	struct device *dev = &touchdev->pdev->dev;

	dev_dbg(dev, "Input device %s opened, starting touch detection\n",
			dev_input->name);

	touchdev->stop_touchscreen = false;

	/* Initialize GPIOs, leave FETs closed by default */
	gpio_direction_output(touchdev->gpio_xp, 1); /* Low active */
	gpio_direction_output(touchdev->gpio_xm, 0); /* High active */
	gpio_direction_output(touchdev->gpio_yp, 1); /* Low active */
	gpio_direction_output(touchdev->gpio_ym, 0); /* High active */

	/* Mux detection before request IRQ, wait for pull-up to settle */
	vf50_ts_enable_touch_detection(touchdev);
	msleep(10);

	touchdev->pen_irq = gpio_to_irq(touchdev->gpio_pen_detect);
	if (touchdev->pen_irq < 0) {
		dev_err(dev, "Unable to get IRQ for GPIO %d\n",
				touchdev->gpio_pen_detect);
		return touchdev->pen_irq;
	}

	ret = request_irq(touchdev->pen_irq, vf50_tc_touched, IRQF_TRIGGER_FALLING,
			"touch detected", touchdev);
	if (ret < 0) {
		dev_err(dev, "Unable to request IRQ %d\n", touchdev->pen_irq);
		return ret;
	}

	return 0;
}

static void vf50_ts_close(struct input_dev *dev_input)
{
	struct vf50_touch_device *touchdev = input_get_drvdata(dev_input);
	struct device *dev = &touchdev->pdev->dev;

	free_irq(touchdev->pen_irq, touchdev);

	touchdev->stop_touchscreen = true;

	/* Wait until touchscreen thread finishes any possible remnants. */
	cancel_work_sync(&touchdev->ts_work);

	dev_dbg(dev, "Input device %s closed, disable touch detection\n",
			dev_input->name);
}

static int vf50_ts_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct vf50_touch_device *touchdev;
	struct input_dev *input;
	struct iio_channel *channels;

	if (!node) {
		dev_err(dev, "Device does not have associated DT data\n");
		return -EINVAL;
	}

	channels = iio_channel_get_all(dev);
	if (IS_ERR(channels))
		return PTR_ERR(channels);

	touchdev = devm_kzalloc(dev, sizeof(*touchdev), GFP_KERNEL);
	if (touchdev == NULL) {
		ret = -ENOMEM;
		goto error_release_channels;
	}

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "Failed to allocate TS input device\n");
		ret = -ENOMEM;
		goto error_release_channels;
	}

	platform_set_drvdata(pdev, touchdev);

	touchdev->pdev = pdev;
	touchdev->channels = channels;

	input->name = DRIVER_NAME;
	input->id.bustype = BUS_HOST;
	input->dev.parent = dev;
	input->open = vf50_ts_open;
	input->close = vf50_ts_close;

	_set_bit(EV_ABS, input->evbit);
	_set_bit(EV_KEY, input->evbit);
	_set_bit(BTN_TOUCH, input->keybit);
	input_set_abs_params(input, ABS_X, 0, VF_ADC_MAX, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, VF_ADC_MAX, 0, 0);
	input_set_abs_params(input, ABS_PRESSURE, 0, VF_ADC_MAX, 0, 0);

	touchdev->ts_input = input;
	input_set_drvdata(input, touchdev);
	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "Failed to register input device\n");
		goto error_release_channels;
	}

	ret = of_property_read_u32(node, "vf,gpio-xp", &touchdev->gpio_xp);
	if (ret)
		goto error_release_channels;

	ret = of_property_read_u32(node, "vf,gpio-xm", &touchdev->gpio_xm);
	if (ret)
		goto error_release_channels;

	ret = of_property_read_u32(node, "vf,gpio-yp", &touchdev->gpio_yp);
	if (ret)
		goto error_release_channels;

	ret = of_property_read_u32(node, "vf,gpio-ym", &touchdev->gpio_ym);
	if (ret)
		goto error_release_channels;

	ret = of_property_read_u32(node, "vf,gpio-pen-detect", &touchdev->gpio_pen_detect);
	if (ret)
		goto error_release_channels;

	ret = of_property_read_u32(node, "vf,gpio-pen-pullup", &touchdev->gpio_pen_detect_pullup);
	if (ret)
		goto error_release_channels;

	/* Request GPIO's for FET's and touch detection  */
	ret = devm_gpio_request(dev, touchdev->gpio_xp, "Touchscreen XP");
	if (ret)
		goto error_release_channels;

	ret = devm_gpio_request(dev, touchdev->gpio_xm, "Touchscreen XM");
	if (ret)
		goto error_release_channels;

	ret = devm_gpio_request(dev, touchdev->gpio_yp, "Touchscreen YP");
	if (ret)
		goto error_release_channels;

	ret = devm_gpio_request(dev, touchdev->gpio_ym, "Touchscreen YM");
	if (ret)
		goto error_release_channels;

	ret = devm_gpio_request(dev, touchdev->gpio_pen_detect, "Pen/Touch Detect");
	if (ret)
		goto error_release_channels;

	ret = devm_gpio_request(dev, touchdev->gpio_pen_detect_pullup, "Pen/Touch Detect pull-up");
	if (ret)
		goto error_release_channels;

	/* Create workqueue for ADC sampling and calculation */
	INIT_WORK(&touchdev->ts_work, vf50_ts_work);
	touchdev->ts_workqueue = create_singlethread_workqueue("vf50-ts-touch");

	if (!touchdev->ts_workqueue) {
		dev_err(dev, "Failed to create vf50-ts-touch workqueue");
		goto error_release_channels;
	}

	dev_info(dev, "Attached colibri-vf50-ts driver successfully\n");

	return 0;

error_release_channels:
	iio_channel_release_all(channels);
	return ret;
}

static int vf50_ts_remove(struct platform_device *pdev)
{
	struct vf50_touch_device *touchdev = platform_get_drvdata(pdev);

	input_unregister_device(touchdev->ts_input);
	input_free_device(touchdev->ts_input);

	destroy_workqueue(touchdev->ts_workqueue);
	iio_channel_release_all(touchdev->channels);

	return 0;
}

static const struct of_device_id vf50_touch_of_match[] = {
	{ .compatible = "toradex,vf50-touchctrl", },
	{ }
};
MODULE_DEVICE_TABLE(of, vf50_touch_of_match);

static struct platform_driver __refdata vf50_touch_driver = {
	.driver = {
		.name = "toradex,vf50_touchctrl",
		.owner = THIS_MODULE,
		.of_match_table = vf50_touch_of_match,
	},
	.probe = vf50_ts_probe,
	.remove = vf50_ts_remove,
	.prevent_deferred_probe = false,
};

module_platform_driver(vf50_touch_driver);

module_param(min_pressure, int, 0600);
MODULE_PARM_DESC(min_pressure, "Minimum pressure for touch detection");
MODULE_AUTHOR("Sanchayan Maity");
MODULE_DESCRIPTION("Colibri VF50 Touchscreen driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
