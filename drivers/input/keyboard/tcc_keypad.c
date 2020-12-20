// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input-polldev.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/iio/types.h>
#include <linux/iio/consumer.h>

/* For id.version */
#define TCCKEYVERSION        0x0003
#define DRV_NAME             "tcc-keypad"

#define BUTTON_DELAY   20 /* ms */
#define KEY_RELEASED    0
#define KEY_PRESSED     1

struct tcc_button {
	uint32_t    s_scancode;
	uint32_t    e_scancode;
	int32_t             vkcode;
	const char      *label;
};

struct tcc_key {
	struct iio_channel *ch;
	struct tcc_button *map;
	uint32_t poll_interval;
	int32_t key_num;
	int32_t key_code;
	int32_t key_status;
};

static void tcc_key_poll_callback(struct input_polled_dev *dev)
{
	struct tcc_key *key = dev->private;
	int32_t i, value, ret;
	int32_t pressed_keycode = -1;
	int32_t val_start;
	int32_t val_end;

	ret = iio_read_channel_raw(key->ch, &value);
	if (ret < 0) {
		pr_err("[ERROR][ADC] %s: failed to get adc data %d\n",
				__func__, ret);
		return;
	}

	for (i = 0; i < key->key_num; i++) {
		val_start = (int32_t)key->map[i].s_scancode;
		val_end = (int32_t)key->map[i].e_scancode;
		/* Check whether the value is within the range */
		if ((val_start <= value) && (value <= val_end)) {
			pressed_keycode = key->map[i].vkcode;
		}
	}

	if (pressed_keycode >= 0) {
		if (key->key_code == pressed_keycode) {
			input_report_key(dev->input,
					(u32)key->key_code,
					KEY_PRESSED);
			key->key_status = KEY_PRESSED;
		} else {
			input_report_key(dev->input,
					(u32)key->key_code,
					KEY_RELEASED);
			key->key_status = KEY_RELEASED;
		}
	} else {
		if (key->key_code >= 0) {
			input_report_key(dev->input,
					(u32)key->key_code,
					KEY_RELEASED);
			key->key_status = KEY_RELEASED;
		} else {
			/* invalid key code, Nothing to do */
		}
	}

	input_sync(dev->input);
	key->key_code = pressed_keycode;
}

/*
 * parsing sub node in device tree.
 */
static void tcc_key_load_map(struct device *dev,
		struct device_node *child,
		struct tcc_button *map)
{
	uint32_t voltage_range[2] = {0,};
	int32_t ret;

	ret = of_property_read_string(child, "label", &map->label);
	if (ret != 0) {
		dev_dbg(dev, "[DEBUG][ADC] failed to get label\n");
	}

	ret = of_property_read_u32(child,
			"linux,code",
			&map->vkcode);
	if (ret != 0) {
		dev_err(dev, "[ERROR][ADC] failed to get invalid linux,code\n");
	}

	ret = of_property_read_u32_array(child,
			"voltage-range",
			voltage_range, 2);
	if (ret != 0) {
		dev_err(dev, "[ERROR][ADC] failed to get voltage range.\n");
	}

	map->s_scancode = voltage_range[0];
	map->e_scancode = voltage_range[1];
}

static int32_t tcc_key_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct tcc_key *key;
	struct tcc_button *map;
	struct input_polled_dev *poll_dev;
	struct input_dev *input_dev;
	enum iio_chan_type type;
	int32_t  i, error, cnt = 0;

	key = devm_kzalloc(&pdev->dev, sizeof(*key), GFP_KERNEL);
	if (key == NULL) {
		return -ENOMEM;
	}

	key->ch = iio_channel_get(dev, "buttons");
	if (IS_ERR(key->ch)) {
		return (int32_t)PTR_ERR(key->ch);
	}
	error = iio_get_channel_type(key->ch, &type);
	if (error < 0) {
		dev_err(dev, "[ERROR][ADC] failed to get iio channel type.\n");
		return error;
	}
	if (type != IIO_VOLTAGE) {
		dev_err(dev, "[ERROR][ADC] Incompatible channel type %d\n",
				type);
		return -EINVAL;
	}

	error = of_property_read_u32(np,
			"poll-interval",
			&key->poll_interval);
	if (error != 0) {
		key->poll_interval = BUTTON_DELAY;
	}

	/* get num of subnode */
	key->key_num = of_get_child_count(np);

	/* allocate key map(sub node) */
	map = devm_kmalloc_array(dev,
			(size_t)key->key_num,
			sizeof(*map),
			GFP_KERNEL);
	if (map == NULL) {
		return -ENOMEM;
	}
	key->map = map;

	/* get sub node information */
	for_each_child_of_node(np, child) {
		if (cnt < key->key_num) {
			tcc_key_load_map(dev, child, &key->map[cnt]);
			cnt++;
		} else {
			break;
		}
	}

	poll_dev = devm_input_allocate_polled_device(dev);
	if (poll_dev == NULL) {
		error = -ENOMEM;
		goto fail;
	}

	poll_dev->private = key;
	poll_dev->poll = tcc_key_poll_callback;
	poll_dev->poll_interval = (uint32_t)msecs_to_jiffies(key->poll_interval);

	input_dev = poll_dev->input;
	input_dev->evbit[0] = BIT(EV_KEY);
	input_dev->name = "telechips keypad";
	input_dev->phys = "tcc-keypad";
	input_dev->id.version = TCCKEYVERSION;

	for (i = 0; i < key->key_num; i++) {
		set_bit(key->map[i].vkcode, input_dev->keybit);
	}

	error = input_register_polled_device(poll_dev);
	if (error < 0) {
		dev_err(dev,
				"[ERROR][ADC] %s: Unable to register input device.\n",
				__func__);
		goto fail;
	}

	return 0;

fail:
	input_free_polled_device(poll_dev);
	return error;
}

#ifdef CONFIG_PM
static int32_t tcc_key_suspend(struct device *dev)
{
	return 0;
}

static int32_t tcc_key_resume(struct device *dev)
{
	return 0;
}

#else
#define tcc_key_suspend NULL
#define tcc_key_resume  NULL
#endif

static SIMPLE_DEV_PM_OPS(tcc_key_pm_ops, tcc_key_suspend, tcc_key_resume);

#ifdef CONFIG_OF
static const struct of_device_id tcc_key_dt_ids[] = {
	{.compatible = "telechips,tcckey",},
	{}
};
#else
#define tcc_key_dt_ids NULL
#endif

static struct platform_driver tcc_key_driver = {
	.driver         = {
		.name   = "tcc-keypad",
		.owner  = THIS_MODULE,
		.pm	= &tcc_key_pm_ops,
		.of_match_table	= of_match_ptr(tcc_key_dt_ids),
	},
	.probe          = tcc_key_probe,

};

module_platform_driver(tcc_key_driver);

MODULE_AUTHOR("linux@telechips.com");
MODULE_DESCRIPTION("Telechips keypad driver");
MODULE_LICENSE("GPL");

