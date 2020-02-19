/****************************************************************************
 *
 * Copyright (C) 2010 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

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

static int32_t aux_verbose_mode = 0;

#define elog(fmt, ...)		((void)printk(KERN_ERR "[ERROR][AUX_DET_DRV]%s : " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__))
#define wlog(fmt, ...)		((void)printk(KERN_WARNING "[WARN][AUX_DET_DRV]%s : " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__))
#define dlog(fmt, ...)		((void)printk(KERN_DEBUG "[DEBUG][AUX_DET_DRV]%s : " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__))
#define ilog(fmt, ...)		((void)printk(KERN_INFO "[INFO][AUX_DET_DRV]%s : " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__))
#define vlog(fmt, ...)      { if(aux_verbose_mode == (int32_t)1) { (void)printk(KERN_DEBUG "[DEBUG][AUX_DEV_DRV]%s - " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__); } }

#define FUNCTION_IN			(dlog("IN\n"))
#define FUNCTION_OUT		(dlog("OUT\n"))

#define MODULE_NAME			("aux_detect")

#define AUX_IOCTL_CMD_GET_STATE		(0x10)

static dev_t			aux_detect_dev_region;
static struct class		* aux_detect_class;
static struct cdev		aux_detect_cdev;

struct aux_detect_data
{
	int32_t aux_detect_gpio;
	int32_t aux_active;
	int32_t enabled;
};

static struct aux_detect_data * pdata;

static atomic_t aux_detect_status;

static int32_t aux_detect_get_state(void) {
	int32_t	state = (int)atomic_read(&aux_detect_status);
	dlog("state: %d\n", state);
	return state;
}

static void aux_detect_set_state(int32_t state) {
	dlog("state: %d\n", state);
	atomic_set(&aux_detect_status, (int)state);
}

static int32_t pre_aux_status = 0;

static ssize_t aux_detect_status_show(struct device * dev, struct device_attribute * attr, char * buf) {
	struct aux_detect_data		* aux_data		= pdata;
	int32_t	aux_value	= -1;
	int32_t	ret	= -1;

	if((aux_data->aux_detect_gpio != -1) && (aux_data->aux_active != -1)) {
		aux_value = gpio_get_value(aux_data->aux_detect_gpio);
		if(aux_value == aux_data->aux_active){
			ret = 1;
		}else{
			ret = 0;
		}

		vlog("gpio: %d, value: %d, active: %d, result: %d\n", aux_data->aux_detect_gpio, aux_value, aux_data->aux_active, ret);
		if(pre_aux_status != ret)
		{
			dlog("aux status change: %d -> %d\n", pre_aux_status, ret);
		}
		pre_aux_status = ret;

	} else {
		elog("HW Switch is not supported, gpio: %d, active: %d\n", aux_data->aux_detect_gpio, aux_data->aux_active);
		ret = 0;
	}

	vlog("aux detect status: %d\n", ret);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t aux_detect_status_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	(void)dev;
	(void)attr;
	(void)buf;
	(void)count;
	ilog("Not support write to aux detect");

	return 0;
}

static DEVICE_ATTR(aux_detect_status, S_IRUGO|S_IWUSR|S_IWGRP, aux_detect_status_show, aux_detect_status_store);

static int32_t aux_detect_is_enabled(void) {
	struct aux_detect_data * aux_data = pdata;

	return aux_data->enabled;
}

static int32_t aux_detect_check_state(void) {
	struct aux_detect_data	* aux_data = pdata;
	int32_t	aux_value = -1;
	int32_t	ret	= -1;

	if((aux_data->aux_detect_gpio != -1) && (aux_data->aux_active != -1)) {
		aux_value = gpio_get_value(aux_data->aux_detect_gpio);

		if(aux_value == aux_data->aux_active){
			ret = 1;
		}else{
			ret = 0;
		}
		vlog("gpio: %d, value: %d, active: %d, result: %d\n", aux_data->aux_detect_gpio, aux_value, aux_data->aux_active, ret);
		if(pre_aux_status != ret)
		{
			dlog("aux status change: %d -> %d\n", pre_aux_status, ret);
		}
		pre_aux_status = ret;
	} else {
		elog("HW Switch is not supported, gpio: %d, active: %d\n", aux_data->aux_detect_gpio, aux_data->aux_active);
		ret = aux_detect_get_state();
	}

	vlog("aux detect status: %d\n", ret);

	return ret;
}

static long aux_detect_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	struct aux_detect_data * aux_data = pdata;
	int state = 0;
	int ret = 0;

	(void)aux_data;

	switch(cmd) {

	case AUX_IOCTL_CMD_GET_STATE:
		state = (long)aux_detect_check_state();
		dlog("state: %d\n", state);

		if(copy_to_user((void *)arg, (const void *)&state, (unsigned long)sizeof(state)) != (unsigned long) 0) {
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

static int aux_detect_open(struct inode * inode, struct file * filp) {
	return 0;
}

static int aux_detect_release(struct inode * inode, struct file * filp) {
	return 0;
}

struct file_operations aux_detect_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= aux_detect_ioctl,
	.open			= aux_detect_open,
	.release		= aux_detect_release,
};

static int aux_detect_probe(struct platform_device * pdev) {
	struct aux_detect_data	* aux_data	= NULL;
	struct pinctrl * pinctrl = NULL;
	int ret = 0;

	FUNCTION_IN;

	if(pdev != NULL)
	{
		/* Allocate a charactor device region */
		ret = alloc_chrdev_region(&aux_detect_dev_region, 0, 1, MODULE_NAME);
		if(ret < 0) {
			elog("Allocate a charactor device region for the \"%s\"\n", (const char *)MODULE_NAME);
		}
		else
		{
			/* Create class */
			aux_detect_class = class_create(THIS_MODULE, MODULE_NAME);
			if(aux_detect_class == NULL) {
				elog("Create the %s class\n", (const char *)MODULE_NAME);
				ret = -1;
			}
			else
			{
				/* Create the Reverse Switch device file system */
				if(device_create(aux_detect_class, NULL, aux_detect_dev_region, NULL, (const char *)MODULE_NAME) == NULL) {
					elog("Create the %s device file\n",(const char *) MODULE_NAME);
					ret = -1;
				}
				else
				{
					/* Register  charactor device */
					cdev_init(&aux_detect_cdev, &aux_detect_fops);
					ret = cdev_add(&aux_detect_cdev, aux_detect_dev_region, 1);
					if(ret < 0) {
						elog("Register the %s device as a charactor device\n", (const char *)MODULE_NAME);
					}
					else
					{
						/* Allocate memory for aux detect Platform Data.*/
						aux_data = kzalloc(sizeof(struct aux_detect_data), GFP_KERNEL);
						if(!aux_data){
							ret = -ENOMEM;
						}
						else
						{
							pdata = aux_data;

							pdev->dev.platform_data = aux_data;

							/* pinctrl */
							pinctrl = pinctrl_get_select(&pdev->dev, "default");
							if(IS_ERR(pinctrl)){
								elog("%s: pinctrl select failed\n", (const char *)MODULE_NAME);
								ret = -1;
							}else{
								pinctrl_put(pinctrl);

								aux_data->aux_detect_gpio	= -1;
								aux_data->aux_active	= -1;

								if(of_parse_phandle(pdev->dev.of_node, "aux-gpios", 0) != NULL) {
									aux_data->aux_detect_gpio	= of_get_named_gpio(pdev->dev.of_node, "aux-gpios", 0);
									of_property_read_u32_index(pdev->dev.of_node, "aux-active", 0, &aux_data->aux_active);

									ilog("switch-gpios: %d, switch-active: %d\n", aux_data->aux_detect_gpio, aux_data->aux_active);
									aux_data->enabled = 0;

									/* Create the aux_detect_status sysfs */
									ret = device_create_file(&pdev->dev, &dev_attr_aux_detect_status);
									if(ret < 0)
									{
										elog("failed create sysfs, aux_detect_status\r\n");
									}

								} else {
									elog("\"switch-gpios\" node is not found.\n");
									ret = -1;
								}

							}
						}
					}

					if(ret < 0)
					{
						device_destroy(aux_detect_class, aux_detect_dev_region);
					}
				}
			}

			if(ret < 0 )
			{
				if(aux_detect_class != NULL)
				{
					class_destroy(aux_detect_class);
				}

				unregister_chrdev_region(aux_detect_dev_region, 1);
			}
		}
	}
	else
	{
		ret = -1;
	}
	FUNCTION_OUT;
	return ret;
}

static int aux_detect_remove(struct platform_device * pdev) {
	FUNCTION_IN;

	cdev_del(&aux_detect_cdev);

	device_destroy(aux_detect_class, aux_detect_dev_region);

	class_destroy(aux_detect_class);

	unregister_chrdev_region(aux_detect_dev_region, 1);

	FUNCTION_OUT;
	return 0;
}

#ifdef CONFIG_OF
const struct of_device_id aux_detect_of_match[] = {
        { .compatible = "telechips,aux_detect", },
        { },
};
MODULE_DEVICE_TABLE(of, aux_detect_of_match);
#endif

struct platform_driver aux_detect_driver = {
	.probe		= aux_detect_probe,
	.remove		= aux_detect_remove,
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

