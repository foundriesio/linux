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
#include <linux/delay.h>	/* msleep_interruptible */
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif//CONFIG_OF

#include <linux/uaccess.h>
#include <linux/fs.h>

static int					debug = 0;
#define log(fmt, ...)		printk(KERN_INFO "%s - " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__)
#define dlog(fmt, ...)		do { if(debug) { printk(KERN_INFO "%s - " pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

#define MODULE_NAME			"switch_gpio_reverse"

#define SWITCH_IOCTL_CMD_ENABLE			0x10
#define SWITCH_IOCTL_CMD_DISABLE		0x11
#define SWITCH_IOCTL_CMD_GET_STATE		0x50

static dev_t			switch_reverse_dev_region;
static struct class		* switch_reverse_class;
static struct cdev		switch_reverse_cdev;

atomic_t switch_reverse_type;
atomic_t switch_reverse_status;
atomic_t switch_reverse_loglevel;

long switch_reverse_get_type(void) {
	long	type = (long)atomic_read(&switch_reverse_type);
	dlog("type: %ld\n", type);
	return type;
}

void switch_reverse_set_type(long type) {
	dlog("type: %ld\n", type);
	atomic_set(&switch_reverse_type, type);
}

ssize_t switch_reverse_type_show(struct device * dev, struct device_attribute * attr, char * buf) {
	return sprintf(buf, "%ld\n", switch_reverse_get_type());
}

ssize_t switch_reverse_type_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);

	if(error)
		return error;

	switch_reverse_set_type(data);

	return count;
}

static DEVICE_ATTR(switch_reverse_type, S_IRUGO|S_IWUSR|S_IWGRP, switch_reverse_type_show, switch_reverse_type_store);

long switch_reverse_get_state(void) {
	long	state = (long)atomic_read(&switch_reverse_status);
	dlog("state: %ld\n", state);
	return state;
}

void switch_reverse_set_state(long state) {
	dlog("state: %ld\n", state);
	atomic_set(&switch_reverse_status, state);
}

ssize_t switch_reverse_status_show(struct device * dev, struct device_attribute * attr, char * buf) {
	return sprintf(buf, "%ld\n", switch_reverse_get_state());
}

ssize_t switch_reverse_status_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	switch_reverse_set_state(data);

	return count;
}

static DEVICE_ATTR(switch_reverse_status, S_IRUGO|S_IWUSR|S_IWGRP, switch_reverse_status_show, switch_reverse_status_store);

ssize_t switch_reverse_loglevel_show(struct device * dev, struct device_attribute * attr, char * buf) {
	return sprintf(buf, "%d\n", atomic_read(&switch_reverse_loglevel));
}

ssize_t switch_reverse_loglevel_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	atomic_set(&switch_reverse_loglevel, data);
	debug = data;

	return count;
}

static DEVICE_ATTR(switch_reverse_loglevel, S_IRUGO|S_IWUSR|S_IWGRP, switch_reverse_loglevel_show, switch_reverse_loglevel_store);

struct switch_reverse_data {
	int		switch_gpio;
	int		switch_active;
	int		enabled;
};

static struct switch_reverse_data * pdata;

int switch_reverse_is_enabled(void) {
	struct switch_reverse_data * data = pdata;

	return data->enabled;
}

int switch_reverse_check_state(void) {
	long							type		= atomic_read(&switch_reverse_type);
	struct switch_reverse_data		* data		= pdata;
	int								gear_value	= -1;
	int								ret			= -1;

	if(type == 1) { // hw switch
		if((data->switch_gpio != -1) && (data->switch_active != -1)) {
			gear_value = !!gpio_get_value(data->switch_gpio);
			ret = (gear_value == data->switch_active);
			dlog("gpio: %d, value: %d, active: %d, result: %d\n", data->switch_gpio, gear_value, data->switch_active, ret);
		} else {
			log("HW Switch is not supported, gpio: %d, active: %d\n", data->switch_gpio, data->switch_active);
			ret = switch_reverse_get_state();
		}
	} else { // sw switch
		ret = switch_reverse_get_state();
	}

	switch(type) {
	case 0:		// sw switch
		ret = switch_reverse_get_state();
		break;

	case 1:		// hw switch
		if((data->switch_gpio != -1) && (data->switch_active != -1)) {
			gear_value = !!gpio_get_value(data->switch_gpio);
			ret = (gear_value == data->switch_active);
			dlog("gpio: %d, value: %d, active: %d, result: %d\n", data->switch_gpio, gear_value, data->switch_active, ret);
		} else {
			log("HW Switch is not supported, gpio: %d, active: %d\n", data->switch_gpio, data->switch_active);
			ret = switch_reverse_get_state();
		}
		break;

	default:
		log("ERRPR: switch type(%d) is WRONG\n", type);
		break;
	}

	dlog("reverse switch status: %d\n", ret);

	return ret;
}

long switch_reverse_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	struct switch_reverse_data * data = pdata;
	int state = 0, ret = 0;

	switch(cmd) {
	case SWITCH_IOCTL_CMD_ENABLE:
		data->enabled		= 1;
		dlog("enabled: %d\n", data->enabled);
		break;

	case SWITCH_IOCTL_CMD_DISABLE:
		data->enabled		= 0;
		dlog("enabled: %d\n", data->enabled);
		break;

	case SWITCH_IOCTL_CMD_GET_STATE:
		state = switch_reverse_check_state();
		dlog("state: %d\n", state);

		if((ret = copy_to_user((void *)arg, (const void *)&state, sizeof(state))) < 0) {
			log("FAILED: copy_to_user\n");
			ret = -1;
			break;
		}
		break;

	default:
		log("FAILED: Unsupported command\n");
		ret = -1;
		break;
	}
	return ret;
}

int switch_reverse_open(struct inode * inode, struct file * filp) {
	return 0;
}

int switch_reverse_release(struct inode * inode, struct file * filp) {
	return 0;
}

struct file_operations switch_reverse_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= switch_reverse_ioctl,
	.open			= switch_reverse_open,
	.release		= switch_reverse_release,
};

int switch_reverse_probe(struct platform_device * pdev) {
	struct switch_reverse_data	* data		= NULL;
	struct pinctrl					* pinctrl	= NULL;
	int ret = 0;

	FUNCTION_IN

	// Allocate a charactor device region
	ret = alloc_chrdev_region(&switch_reverse_dev_region, 0, 1, MODULE_NAME);
	if(ret < 0) {
		log("ERROR: Allocate a charactor device region for the \"%s\"\n", MODULE_NAME);
		return ret;
	}
	
	// Create the Reverse Switch class
	switch_reverse_class = class_create(THIS_MODULE, MODULE_NAME);
	if(switch_reverse_class == NULL) {
		log("ERROR: Create the \"%s\" class\n", MODULE_NAME);
		goto goto_unregister_chrdev_region;
	}

	// Create the Reverse Switch device file system
	if(device_create(switch_reverse_class, NULL, switch_reverse_dev_region, NULL, MODULE_NAME) == NULL) {
		log("ERROR: Create the \"%s\" device file\n", MODULE_NAME);
		goto goto_destroy_class;
	}

	// Register the Reverse Switch device as a charactor device
	cdev_init(&switch_reverse_cdev, &switch_reverse_fops);
	ret = cdev_add(&switch_reverse_cdev, switch_reverse_dev_region, 1);
	if(ret < 0) {
		log("ERROR: Register the \"%s\" device as a charactor device\n", MODULE_NAME);
		goto goto_destroy_device;
	}

	// Allocate memory for Reverse Switch Platform Data.
	data = kzalloc(sizeof(struct switch_reverse_data), GFP_KERNEL);
	if(!data)
		return -ENOMEM;

	pdata = data;

	// Set the Reverse Switch Platform Data.
	pdev->dev.platform_data	= data;

	// pinctrl
	pinctrl = pinctrl_get_select(&pdev->dev, "default");
	if(IS_ERR(pinctrl))
		log("%s: pinctrl select failed\n", MODULE_NAME);
	else
		pinctrl_put(pinctrl);

	data->switch_gpio	= -1;
	data->switch_active	= -1;
	if(of_parse_phandle(pdev->dev.of_node, "switch-gpios", 0) != NULL) {
		data->switch_gpio	= of_get_named_gpio(pdev->dev.of_node, "switch-gpios", 0);
		of_property_read_u32_index(pdev->dev.of_node, "switch-active", 0, &data->switch_active);
	} else {
		log("\"switch-gpios\" node is not found.\n");
	}
	log("switch-gpios: %d, switch-active: %d\n", data->switch_gpio, data->switch_active);

	data->enabled		= 0;	// disabled now

	// Create the switch_reverse_type sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_switch_reverse_type);
	if(ret < 0)
		log("failed create sysfs, switch_reverse_type\r\n");

	// Set switch_reverse_type as 1 if there is a hw switch node
	if(data->switch_gpio != -1) {
		switch_reverse_set_type(1);
	}

	// Create the switch_reverse_status sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_switch_reverse_status);
	if(ret < 0)
		log("failed create sysfs, switch_reverse_status\r\n");

	// Create the switch_reverse_loglevel sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_switch_reverse_loglevel);
	if(ret < 0)
		log("failed create sysfs, switch_reverse_loglevel\r\n");

	FUNCTION_OUT
	return 0;

//goto_unregister_cdev:
//	// Unregister the Reverse Switch device
//	cdev_del(&switch_reverse_cdev);

goto_destroy_device:
	// Destroy the Reverse Switch device file system
	device_destroy(switch_reverse_class, switch_reverse_dev_region);
	
goto_destroy_class:
	// Destroy the Reverse Switch class
	class_destroy(switch_reverse_class);

goto_unregister_chrdev_region:
	// Unregister the charactor device region
	unregister_chrdev_region(switch_reverse_dev_region, 1);

	FUNCTION_OUT
	return -1;
}

int switch_reverse_remove(struct platform_device * pdev) {
	FUNCTION_IN

	// Unregister the Reverse Switch device
	cdev_del(&switch_reverse_cdev);

	// Destroy the Reverse Switch device file system
	device_destroy(switch_reverse_class, switch_reverse_dev_region);

	// Destroy the Reverse Switch class
	class_destroy(switch_reverse_class);

	// Unregister the charactor device region
	unregister_chrdev_region(switch_reverse_dev_region, 1);

	FUNCTION_OUT
	return 0;
}

#ifdef CONFIG_OF
const struct of_device_id switch_reverse_of_match[] = {
        { .compatible = "telechips,switch_reverse", },
        { },
};
MODULE_DEVICE_TABLE(of, switch_reverse_of_match);
#endif//CONFIG_OF

struct platform_driver switch_reverse_driver = {
	.probe		= switch_reverse_probe,
	.remove		= switch_reverse_remove,
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = switch_reverse_of_match,
#endif//CONFIG_OF
	},
};

module_platform_driver(switch_reverse_driver);

MODULE_AUTHOR("Jay HAN<jjongspi@telechips.com>");
MODULE_DESCRIPTION("SWITCH REVERSE Driver");
MODULE_LICENSE("GPL");

