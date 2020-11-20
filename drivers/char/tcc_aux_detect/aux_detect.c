// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/types.h>

static int32_t aux_verbose_mode;

#ifndef char_t
typedef char char_t;
#endif

#ifndef long_t
typedef long long_t;
#endif

#define elog(fmt, ...) \
	((void)pr_err("[ERROR][AUX_DET_DRV]%s : " \
		pr_fmt(fmt), __func__, ##__VA_ARGS__))

#define wlog(fmt, ...) \
	((void)pr_warn("[WARN][AUX_DET_DRV]%s : " \
		pr_fmt(fmt), __func__, ##__VA_ARGS__))

#define dlog(fmt, ...) \
	((void)pr_notice("[DEBUG][AUX_DET_DRV]%s : " \
		pr_fmt(fmt), __func__, ##__VA_ARGS__))

#define ilog(fmt, ...) \
	((void)pr_info("[INFO][AUX_DET_DRV]%s : " \
		pr_fmt(fmt), __func__, ##__VA_ARGS__))

#define vlog(fmt, ...) \
	{ if (aux_verbose_mode == (int32_t)1) \
	{ (void)pr_info("[DEBUG][AUX_DEV_DRV]%s - " \
	pr_fmt(fmt), __func__, ##__VA_ARGS__); } }

#define FUNCTION_IN			(dlog("IN\n"))
#define FUNCTION_OUT		(dlog("OUT\n"))

#define MODULE_NAME			("aux_detect")

#define AUX_IOCTL_CMD_GET_STATE		(0x10)

static dev_t			aux_detect_dev_region;
static struct class		*aux_detect_class;
static struct cdev		aux_detect_cdev;

struct aux_detect_data {
	int32_t aux_detect_gpio;
	int32_t aux_active;
	int32_t enabled;
};

static struct aux_detect_data *pdata;

static atomic_t aux_detect_status;

static int32_t aux_detect_get_state(void)
{
	int32_t	state = (int32_t)atomic_read(&aux_detect_status);

	dlog("state: %d\n", state);
	return state;
}

#if 0
static void aux_detect_set_state(int32_t state)
{
	dlog("state: %d\n", state);
	atomic_set(&aux_detect_status, (int)state);
}
#endif

static int32_t pre_aux_status;

static ssize_t aux_detect_status_show(
		struct device *dev,
		struct device_attribute *attr,
		char_t *buf)
{
	struct aux_detect_data *aux_data = pdata;
	int32_t	aux_value = -1;
	int32_t	ret	= -1;

	if ((aux_data->aux_detect_gpio != -1) &&
		(aux_data->aux_active != -1)) {
		aux_value = gpio_get_value((uint32_t)aux_data->aux_detect_gpio);
		if (aux_value == aux_data->aux_active) {
			ret = 1;
		} else {
			ret = 0;
		}

		vlog("gpio: %d, value: %d, active: %d, result: %d\n",
			aux_data->aux_detect_gpio,
			aux_value,
			aux_data->aux_active,
			ret);

		if (pre_aux_status != ret) {
			dlog("aux status change: %d -> %d\n",
				pre_aux_status, ret);
		}
		pre_aux_status = ret;

	} else {
		elog("HW Switch is not supported, gpio: %d, active: %d\n",
			aux_data->aux_detect_gpio,
			aux_data->aux_active);
		ret = 0;
	}

	vlog("aux detect status: %d\n", ret);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t aux_detect_status_store(
			struct device *dev,
			struct device_attribute *attr,
			const char_t *buf,
			size_t count)
{
	(void)dev;
	(void)attr;
	(void)buf;
	(void)count;
	ilog("Not support write to aux detect");

	return 0;
}

static DEVICE_ATTR(aux_detect_status,
					0664,
					aux_detect_status_show,
					aux_detect_status_store);

#if 0
static int32_t aux_detect_is_enabled(void)
{
	struct aux_detect_data *aux_data = pdata;

	return aux_data->enabled;
}
#endif
static int32_t aux_detect_check_state(void)
{

	struct aux_detect_data	*aux_data = pdata;
	int32_t	aux_value = -1;
	int32_t	ret	= -1;

	if ((aux_data->aux_detect_gpio != -1) &&
		(aux_data->aux_active != -1)) {

		aux_value = gpio_get_value((uint32_t)aux_data->aux_detect_gpio);

		if (aux_value == aux_data->aux_active) {
			ret = 1;
		} else {
			ret = 0;
		}
		vlog("gpio: %d, value: %d, active: %d, result: %d\n",
			aux_data->aux_detect_gpio,
			aux_value,
			aux_data->aux_active,
			ret);

		if (pre_aux_status != ret) {
			dlog("aux status change: %d -> %d\n",
				pre_aux_status, ret);
		}
		pre_aux_status = ret;
	} else {
		elog("HW Switch is not supported, gpio: %d, active: %d\n",
			aux_data->aux_detect_gpio,
			aux_data->aux_active);

		ret = aux_detect_get_state();
	}

	vlog("aux detect status: %d\n", ret);

	return ret;
}

static long_t aux_detect_ioctl(
			struct file *filp,
			uint32_t cmd,
			ulong arg)
{

	struct aux_detect_data *aux_data = pdata;
	int32_t state = 0;
	int32_t ret = 0;

	(void)filp;
	(void)aux_data;

	switch (cmd) {

	case AUX_IOCTL_CMD_GET_STATE:
		state = (int32_t)aux_detect_check_state();

		if (copy_to_user((void __user *)arg,
				(const void *)&state,
				(ulong)sizeof(state))
				!= (ulong)0) {
			elog("FAILED: copy_to_user\n");
			ret = -1;
			break;
		}
		break;

	default:
		elog("FAILED: Unsupported command\n");
		ret = -1;
		break;
	}
	return ret;
}

static int32_t aux_detect_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;

	return 0;
}

static int32_t aux_detect_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;

	return 0;
}

static const struct file_operations aux_detect_fops = {
	.owner			= THIS_MODULE,
	.compat_ioctl	= aux_detect_ioctl,
	.unlocked_ioctl	= aux_detect_ioctl,
	.open			= aux_detect_open,
	.release		= aux_detect_release,
};

static int32_t aux_detect_create_cdev(struct platform_device *pdev)
{
	struct aux_detect_data	*aux_data	= NULL;
	struct pinctrl *pinctrl = NULL;
	int32_t ret = 0;

	if (pdev != NULL) {

		/* Register  charactor device */
		cdev_init(&aux_detect_cdev, &aux_detect_fops);

		ret = cdev_add(&aux_detect_cdev,
			aux_detect_dev_region, 1);

		if (ret < 0) {
			elog("Register the %s device as a charactor device\n",
				(const char_t *)MODULE_NAME);
		} else {
			/* Allocate memory for aux detect Platform Data.*/
			aux_data = kzalloc(sizeof(struct aux_detect_data),
							GFP_KERNEL);
			if (!aux_data) {
				ret = -ENOMEM;
			}
		}

		if (ret > -1) {

			pdata = aux_data;

			pdev->dev.platform_data = aux_data;

			/* pinctrl */
			pinctrl = pinctrl_get_select(&pdev->dev, "default");
			if (IS_ERR(pinctrl)) {
				elog("%s: pinctrl select failed\n",
					(const char_t *)MODULE_NAME);
				ret = -1;
			} else {
				pinctrl_put(pinctrl);

				aux_data->aux_detect_gpio	= -1;
				aux_data->aux_active	= -1;

				if (of_parse_phandle(
						pdev->dev.of_node,
						"aux-gpios",
						0)
					!= NULL) {

					aux_data->aux_detect_gpio =
						of_get_named_gpio(
							pdev->dev.of_node,
							"aux-gpios", 0);

					of_property_read_u32_index(
						pdev->dev.of_node,
						"aux-active",
						0,
						&aux_data->aux_active);

					ilog("switch-gpios:%d\n",
						aux_data->aux_detect_gpio);
					ilog("switch-active:%d\n",
						aux_data->aux_active);


					aux_data->enabled = 0;

					/* Create the aux_detect_status sysfs */
					ret = device_create_file(&pdev->dev,
						&dev_attr_aux_detect_status);
					if (ret < 0) {
						elog("failed create sysfs\r\n");
					}
				} else {
					elog("switch-gpios is not found.\n");
					ret = -1;
				}
			}
		}
	}
	return ret;
}

static int32_t aux_detect_probe(struct platform_device *pdev)
{
	int32_t ret = 0;

	FUNCTION_IN;

	if (pdev != NULL) {
		pre_aux_status = 0;
		/* Allocate a charactor device region */
		ret = alloc_chrdev_region(&aux_detect_dev_region,
							0, 1, MODULE_NAME);

		if (ret < 0) {
			elog("Allocate a cdev for \"%s\"\n",
				(const char_t *)MODULE_NAME);
		} else {
			/* Create class */
			aux_detect_class = class_create(
								THIS_MODULE,
								MODULE_NAME);

			if (aux_detect_class == NULL) {
				elog("Create the %s class\n",
					(const char_t *)MODULE_NAME);
				ret = -1;
			} else {
			/* Create the Reverse Switch device file system */
				if (device_create(aux_detect_class,
						NULL,
						aux_detect_dev_region,
						NULL,
						(const char_t *)MODULE_NAME)
						== NULL) {

					elog("Create the %s device file\n",
						(const char_t *) MODULE_NAME);
					ret = -1;
				} else {

					ret = aux_detect_create_cdev(pdev);
					if (ret < 0) {
						device_destroy(
							aux_detect_class,
							aux_detect_dev_region);
					}
				}
			}

			if (ret < 0) {
				if (aux_detect_class != NULL) {
					class_destroy(aux_detect_class);
					aux_detect_class = NULL;
				}

				unregister_chrdev_region(
					aux_detect_dev_region, 1);
			}
		}
	} else {
		ret = -1;
	}
	FUNCTION_OUT;
	return ret;
}

static int32_t aux_detect_remove(struct platform_device *pdev)
{
	FUNCTION_IN;

	cdev_del(&aux_detect_cdev);

	device_destroy(aux_detect_class, aux_detect_dev_region);

	class_destroy(aux_detect_class);

	unregister_chrdev_region(aux_detect_dev_region, 1);

	FUNCTION_OUT;
	return 0;
}

#if defined(CONFIG_PM)
static int32_t aux_detect_suspend(struct platform_device *pdev, pm_message_t state)
{

	return 0;
}

static int32_t aux_detect_resume(struct platform_device *pdev)
{

	return 0;
}


#endif


#ifdef CONFIG_OF
static const struct of_device_id aux_detect_of_match[] = {
	{ .compatible = "telechips,aux_detect", },
	{ },
};
MODULE_DEVICE_TABLE(of, aux_detect_of_match);
#endif

static struct platform_driver aux_detect_driver = {
	.probe		= aux_detect_probe,
	.remove		= aux_detect_remove,
#if defined(CONFIG_PM)
	.suspend = aux_detect_suspend,
	.resume = aux_detect_resume,
#endif
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = aux_detect_of_match,
#endif
	},
};

module_platform_driver(aux_detect_driver);

MODULE_AUTHOR("<jun@telechips.com>");
MODULE_DESCRIPTION("Aux detect driver");
MODULE_LICENSE("GPL");

