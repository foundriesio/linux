/*
 * linux/drivers/input/keyboard/tcc-keys.c
 *
 * Based on:     	drivers/input/keyboard/bf54x-keys.c
 * Author: <linux@telechips.com>
 * Created: June 10, 2008
 * Description: Keypad ADC driver on Telechips TCC Series
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input-polldev.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/device.h>
#include <asm/io.h>
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

struct tcc_button
{
	int             s_scancode;
	int             e_scancode;
	int             vkcode;
	const char      *label;
};

struct tcc_key
{
	struct iio_channel *ch;
	struct tcc_button *map;
	uint32_t poll_interval;
	int key_num;
	int key_code;
	int key_status;
};

static void tcc_key_poll_callback(struct input_polled_dev *dev)
{
	struct tcc_key *tcc_key = dev->private;
	int i, value, ret;
	int pressed_keycode = -1;

	ret = iio_read_channel_raw(tcc_key->ch, &value);
	if(ret < 0){
		printk(KERN_ERR "[ERROR][ADC] %s: failed to get adc data %d\n", __func__, ret);
		return;
	}

	for(i = 0; i < tcc_key->key_num; i++){
		if(tcc_key->map[i].s_scancode <= value && value <= tcc_key->map[i].e_scancode){
			pressed_keycode = tcc_key->map[i].vkcode;
		}
	}

	if(pressed_keycode >= 0){
		if(tcc_key->key_code == pressed_keycode){
			input_report_key(dev->input, tcc_key->key_code, KEY_PRESSED);
			//input_sync(dev->input);
			tcc_key->key_status = KEY_PRESSED;
		} else {
			input_report_key(dev->input, tcc_key->key_code, KEY_RELEASED);
			//input_sync(dev->input);
			tcc_key->key_status = KEY_RELEASED;
		}
	}else{
		if (tcc_key->key_code >= 0)
		{
			input_report_key(dev->input, tcc_key->key_code, KEY_RELEASED);
			//input_sync(dev->input);
			tcc_key->key_status = KEY_RELEASED;
		}
	}

	input_sync(dev->input);
	tcc_key->key_code = pressed_keycode;
}

/*
 * parsing sub node in device tree.
 */
static void tcc_key_load_map(struct device *dev, struct device_node *child, struct tcc_button *map)
{
	uint32_t voltage_range[2] = {0,};
	int ret;

	of_property_read_string(child, "label", &map->label);

	ret = of_property_read_u32(child, "linux,code", &map->vkcode);
	if(ret){
		dev_err(dev, "[ERROR][ADC] %s: failed to get invalid linux,code\n", __func__);
	}

	ret = of_property_read_u32_array(child, "voltage-range", voltage_range, 2);
	if(ret){
		dev_err(dev, "[ERROR][ADC] %s: failed to get voltage range.\n", __func__);
	}

	map->s_scancode = voltage_range[0];
	map->e_scancode = voltage_range[1];
}

static int tcc_key_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct tcc_key *tcc_key;
	struct tcc_button *map;
	struct input_polled_dev *poll_dev;
	struct input_dev *input_dev;
	enum iio_chan_type type;
	int  i, error, cnt = 0;

	tcc_key = devm_kzalloc(&pdev->dev, sizeof(*tcc_key), GFP_KERNEL);
	if(!tcc_key){
		dev_err(dev, "[ERROR][ADC] %s: failed to allocate tcc_key.\n", __func__);
		return -ENOMEM;
	}

	tcc_key->ch = iio_channel_get(dev, "buttons");
	if(IS_ERR(tcc_key->ch)){
		return PTR_ERR(tcc_key->ch);
	}
	if(!tcc_key->ch->indio_dev){
		return -ENXIO;
	}

	error = iio_get_channel_type(tcc_key->ch, &type);
	if(error < 0){
		dev_err(dev, "[ERROR][ADC] %s: failed to get iio channel type.\n", __func__);
		return error;
	}
	if(type != IIO_VOLTAGE){
		dev_err(dev, "[ERROR][ADC] %s: Incompatible channel type %d\n", __func__, type);
		return -EINVAL;
	}

	error = of_property_read_u32(np, "poll-interval", &tcc_key->poll_interval);
	if(error){
		tcc_key->poll_interval = BUTTON_DELAY;
	}

	/* get num of subnode */
	tcc_key->key_num = of_get_child_count(np);

	/* allocate key map(sub node) */
	map = devm_kmalloc_array(dev, tcc_key->key_num, sizeof(*map), GFP_KERNEL);
	if(!map){
		dev_err(dev, "[ERROR][ADC] %s: failed to allocate key map\n", __func__);
		return -ENOMEM;
	}
	tcc_key->map = map;

	/* get sub node information */
	for_each_child_of_node(np, child){
		if(cnt < tcc_key->key_num){
			tcc_key_load_map(dev, child, &tcc_key->map[cnt]);
			cnt++;
		}else{
			break;
		}
	}

	poll_dev = devm_input_allocate_polled_device(dev);
	if (!tcc_key || !poll_dev) {
		dev_err(dev, "[ERROR][ADC] %s: failed to allocate polled device.\n", __func__);
		error = -ENOMEM;
		goto fail;
	}

	poll_dev->private = tcc_key;
	poll_dev->poll = tcc_key_poll_callback;
	poll_dev->poll_interval = msecs_to_jiffies(tcc_key->poll_interval);

	input_dev = poll_dev->input;
	input_dev->evbit[0] = BIT(EV_KEY);
	input_dev->name = "telechips keypad";
	input_dev->phys = "tcc-keypad";
	input_dev->id.version = TCCKEYVERSION;

	for(i = 0; i < tcc_key->key_num; i++){
		set_bit(tcc_key->map[i].vkcode, input_dev->keybit);
	}

	error = input_register_polled_device(poll_dev);
	if(error){
		dev_err(dev, "[ERROR][ADC] %s: Unable to register input device.\n", __func__);
		goto fail;
	}

	return 0;

fail:
	input_free_polled_device(poll_dev);
	return error;
}

#ifdef CONFIG_PM
static int tcc_key_suspend(struct device *dev)
{
	return 0;
}

static int tcc_key_resume(struct device *dev)
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

