// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/tcc_gpio.h>

#include "../../pinctrl/core.h"
#include "../../pinctrl/tcc/pinctrl-tcc.h"
#include "../../pinctrl/pinconf.h"

#if defined(DEBUG_GPIO_TEST)
#define debug_gpio(msg...) printk(msg);
#else
#define debug_gpio(msg...)
#endif

#define error_gpio(msg...) \
	do { \
		printk("\033[031m" msg); \
		printk("\033[0m"); \
	} while(0);

/*
 * pinconf test
 * unit test
 * gpio module test
 */

static u32 get_pinconf_value(struct pinctrl_dev *pctldev,
		u32 param, u32 pin_num)
{
	ulong g_config;

	g_config = tcc_pinconf_pack(param, 0U/* don't care */);
	pin_config_get_for_pin(pctldev, pin_num, &g_config);

	return tcc_pinconf_unpack_value(g_config);
}

static int set_pinconf_value(struct pinctrl_dev *pctldev,
		u32 param, u32 pin_num, u32 value, u32 gpio_num)
{
	ulong s_config;
	int ret;

	s_config = tcc_pinconf_pack(param, value);
//	ret = pinconf_set_config(pctldev, pin_num, &s_config, 1U/* nconfigs */);
	ret = pinctrl_gpio_set_config(gpio_num, s_config);

	if (ret != 0) {
		error_gpio("Failed to set config\n");
	}

	return ret;
}

static int gpio_test_input(struct pinctrl_dev *pctldev,
		u32 pin_num, u32 req_value, u32 gpio_num)
{
	u32 result, result2;
	char buf[1024];
	u32 origin_value = get_pinconf_value(pctldev, TCC_PINCONF_INPUT_BUFFER_ENABLE, gpio_num);

	printk("\tbefore : %u\n", origin_value);
	printk("\trequest : %u\n", req_value);
	set_pinconf_value(pctldev, TCC_PINCONF_INPUT_ENABLE, pin_num, req_value, gpio_num);

	result = get_pinconf_value(pctldev, TCC_PINCONF_INPUT_BUFFER_ENABLE, gpio_num);
	printk("\tafter : %u\n", result);

	if (req_value != result) {
		error_gpio("Failed to test  request(%u) != result(%u)\n",
				req_value, result);
		return -1;
	}

	req_value = 0;
	printk("\tbefore2 : %u\n", result);
	printk("\trequest2 : %u\n", req_value);
	set_pinconf_value(pctldev, TCC_PINCONF_OUTPUT_LOW, pin_num, origin_value, gpio_num);
	result = get_pinconf_value(pctldev, TCC_PINCONF_INPUT_BUFFER_ENABLE, gpio_num);
	printk("\tafter2 : %u\n", result);

	if (req_value != result) {
		error_gpio("Failed to test  request(%u) != result(%u)\n",
				req_value, result);
		return -1;
	}

	return 0;
}

static int gpio_param_test(struct pinctrl_dev *pctldev,
		u32 param, u32 pin_num, u32 req_value, u32 gpio_num)
{
	u32 result;
	char buf[1024];
	u32 origin_value = get_pinconf_value(pctldev, param, gpio_num);

	debug_gpio("\tbefore : %u\n", origin_value);
	debug_gpio("\trequest : %u\n", req_value);
	set_pinconf_value(pctldev, param, pin_num, req_value, gpio_num);

	result = get_pinconf_value(pctldev, param, gpio_num);
	debug_gpio("\tafter : %u\n", result);

	/* restore to origin value */
	set_pinconf_value(pctldev, param, pin_num, origin_value, gpio_num);

	if (req_value != result) {
		error_gpio("Failed to test  request(%u) != result(%u)\n",
				req_value, result);
		return -1;
	}

	return 0;
}


ssize_t gpio_test(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pinctrl *pinctrl = NULL;
	struct pinctrl_state *state = NULL;
	struct pinctrl_setting *setting = NULL;
	char name[1024];
	int ret = 0;
	int buff_offset = 0;

	// get pinctrl
	pinctrl = pinctrl_get(dev);
	if(IS_ERR(pinctrl)) {
		debug_gpio("pinctrl_get returned %p\n", pinctrl);
		return -1;
	}

	debug_gpio("\033[32m");
	debug_gpio("Start GPIO Test\n");
	debug_gpio("\033[0m");
	state = pinctrl_lookup_state(pinctrl, "default");
	list_for_each_entry(setting, &state->settings, node) {

		struct pinctrl_dev *pctldev = setting->pctldev;
		const struct pinctrl_ops *pctlops = pctldev->desc->pctlops;
		struct tcc_pinctrl *pctl;
		struct tcc_pin_bank *pin_banks;
		u32 req_value;
		u32 pin_num;
		u32 bank_num;
		u32 gpio_num = 0U;

		pctl = pinctrl_dev_get_drvdata(pctldev);
		pin_banks = pctl->pin_banks;
		u32 max_bank_num = pctl->nbanks;

		printk("max_bank_num : %u\n", max_bank_num);
		for (bank_num = 0U; bank_num < max_bank_num; bank_num++) {
			printk("\033[33mbank - %s\033[0m\n", pin_banks[bank_num].name);
			printk("\033[33mbanks[%u].npins = %u\033[0m\n", bank_num, pin_banks[bank_num].npins);

			for (pin_num = 0U; pin_num < pin_banks[bank_num].npins; pin_num++, gpio_num++) {
				debug_gpio("pin_num - %u\n", pin_num);
				debug_gpio("gpio_num - %u\n", gpio_num);

				/* set direction test */
				//gpio_test_input(pctldev, pin_num, 1U/*not use*/, gpio_num);

				if ((pin_num == 10 || pin_num == 11)// || pin_num == 12 || pin_num == 13)
						&&	strcmp(pin_banks[bank_num].name, "gpc") == 0) {
					printk("\033[31mskip %s %u, because a72 uart pin\033[0m\n",
						pin_banks[bank_num].name, pin_num);
					continue;
				} else if(strcmp(pin_banks[bank_num].name, "gpk") == 0) {
					//printk("\033[31mskip %s %u\033[0m\n",
					//	pin_banks[bank_num].name, pin_num);
					continue;
				}

				debug_gpio("\nTCC_PINCONF_FUNC test\n");
				for (req_value = 0U; req_value < 16U; req_value++) {
					gpio_param_test(pctldev, TCC_PINCONF_FUNC, pin_num, req_value, gpio_num);
				}

				debug_gpio("\n");
				debug_gpio("TCC_PINCONF_OUTPUT_HIGH test\n");
				debug_gpio("TCC_PINCONF_OUTPUT_LOW test\n");
				gpio_param_test(pctldev, TCC_PINCONF_OUTPUT_HIGH, pin_num, 1U/* don't care */, gpio_num);
				gpio_param_test(pctldev, TCC_PINCONF_OUTPUT_LOW, pin_num, 0U/* don't care */, gpio_num);

				debug_gpio("\n");
				debug_gpio("TCC_PINCONF_INPUT_ENABLE(output disable) test\n");
				gpio_param_test(pctldev, TCC_PINCONF_INPUT_ENABLE, pin_num, 1U/* only 1(on) */, gpio_num);

				debug_gpio("\nTCC_PINCONF_DRIVE_STRENGTH test\n");
				for (req_value = 0U; req_value < 4U; req_value++) {
					gpio_param_test(pctldev, TCC_PINCONF_DRIVE_STRENGTH, pin_num, req_value, gpio_num);
				}

				debug_gpio("\n");
				debug_gpio("TCC_PINCONF_SLOW_SLEW test\n");
				debug_gpio("TCC_PINCONF_FAST_SLEW test\n");
				gpio_param_test(pctldev, TCC_PINCONF_SLOW_SLEW, pin_num, 1U/* don't care */, gpio_num);
				gpio_param_test(pctldev, TCC_PINCONF_FAST_SLEW, pin_num, 0U/* don't care */, gpio_num);

				debug_gpio("\n");
				debug_gpio("TCC_PINCONF_INPUT_BUFFER_ENABLE test\n");
				debug_gpio("TCC_PINCONF_INPUT_BUFFER_DISABLE test\n");
				gpio_param_test(pctldev, TCC_PINCONF_INPUT_BUFFER_ENABLE, pin_num, 1U/*don't care*/, gpio_num);
				gpio_param_test(pctldev, TCC_PINCONF_INPUT_BUFFER_DISABLE, pin_num, 0U/*don't care*/, gpio_num);

				debug_gpio("\n");
				debug_gpio("TCC_PINCONF_SCHMITT_INPUT test\n");
				debug_gpio("TCC_PINCONF_CMOS_INPUT test\n");
				gpio_param_test(pctldev, TCC_PINCONF_SCHMITT_INPUT, pin_num, 1U/*don't care*/, gpio_num);
				gpio_param_test(pctldev, TCC_PINCONF_CMOS_INPUT, pin_num, 0U/*don't care*/, gpio_num);
			}

			debug_gpio("\n");
		}

		return 0;
	}

	return buff_offset;
	//return sprintf(buf,"gpio_test\n");
}

static DEVICE_ATTR(gpio_test, S_IRUGO/*|S_IWUSR*/, gpio_test, NULL);

static struct attribute *gpio_test_attrs[] = {
	&dev_attr_gpio_test.attr,
	NULL,
};

static const struct attribute_group gpio_test_attr_group = {
	.attrs = gpio_test_attrs,
};

static const struct of_device_id gpio_test_of_match[] = {
	{ .compatible = "gpio-test", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_test_of_match);

static int gpio_test_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int error;

	//create nodes to sysfs  with attribute_group
	error = sysfs_create_group(&pdev->dev.kobj, &gpio_test_attr_group);
	if (error) {
		error_gpio("Unable to export sample/switches, error: %d\n",
				error);
		goto err_remove_group;
	}

	return 0;

err_remove_group:
	sysfs_remove_group(&pdev->dev.kobj, &gpio_test_attr_group);
	return -1;
}

static int gpio_test_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &gpio_test_attr_group);

	return 0;
}

static struct platform_driver gpio_test_device_driver = {
	.probe		= gpio_test_probe,
	.remove		= gpio_test_remove,
	.driver		= {
		.name	= "tcc_gpio_test",
		.of_match_table = of_match_ptr(gpio_test_of_match),
	}
};

static int __init gpio_test_init(void)
{
	return platform_driver_register(&gpio_test_device_driver);
}

static void __exit gpio_test_exit(void)
{
	platform_driver_unregister(&gpio_test_device_driver);
}

late_initcall(gpio_test_init);
module_exit(gpio_test_exit);

MODULE_AUTHOR("Telechips.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO Test Driver");
MODULE_ALIAS("platform:tcc_gpio_test");
