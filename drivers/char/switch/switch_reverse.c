/*
 *  drivers/char/switch/switch_reverse.c
 *
 * Copyright (C) Telechips, Inc.
 *
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>	/* msleep_interruptible */
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

static int					debug = 0;
#define TAG					"switch_reverse"
#define log(msg, arg...)	do { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } while(0)
#define dlog(msg, arg...)	do { if(debug) { printk(KERN_INFO TAG ": %s - " msg, __func__, ## arg); } } while(0)
#define FUNCTION_IN			dlog("IN\n");
#define FUNCTION_OUT		dlog("OUT\n");

#define MODULE_NAME			"switch_gpio_reverse"

#define SWITCH_IOCTL_CMD_ENABLE			0x10
#define SWITCH_IOCTL_CMD_DISABLE		0x11
#define SWITCH_IOCTL_CMD_GET_STATE		0x50

atomic_t switch_reverse_attr;

long switch_reverse_get_state(void) {
	long	state = atomic_read(&switch_reverse_attr);;
	dlog("state: %lu\n", state);
	return state;
}

void switch_reverse_set_state(long state) {
	dlog("state: %lu\n", state);
	atomic_set(&switch_reverse_attr, state);
}

ssize_t switch_reverse_attr_show(struct device * dev, struct device_attribute * attr, char * buf) {
	return sprintf(buf, "%ld\n", switch_reverse_get_state());
}

ssize_t switch_reverse_attr_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if(error)
		return error;

	switch_reverse_set_state(data);

	return count;
}

static DEVICE_ATTR(switch_reverse_attr, S_IRUGO|S_IWUSR|S_IWGRP, switch_reverse_attr_show, switch_reverse_attr_store);

struct switch_reverse_data {
	unsigned int		switch_gpio;
	unsigned int		switch_active;
	unsigned int		enabled;
};

static struct switch_reverse_data * pdata;

int switch_reverse_is_enabled(void) {
	struct switch_reverse_data * data = pdata;

	return data->enabled;
}

int switch_reverse_check_state(void) {
#ifndef CONFIG_PMAP_CA7S
	struct switch_reverse_data * data = pdata;
	int gear_value = -1;
#endif//CONFIG_PMAP_CA7S
	int ret = -1;

#ifndef CONFIG_PMAP_CA7S
	gear_value = !!gpio_get_value(data->switch_gpio);
	ret = (gear_value == data->switch_active);
	atomic_set(&switch_reverse_attr, ret);
	dlog("gpio: %d, value: %d, active: %d, result: %d\n", data->switch_gpio, gear_value, data->switch_active, ret);

	switch_reverse_set_state(ret);
#endif//CONFIG_PMAP_CA7S

	ret = switch_reverse_get_state();

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

static dev_t			switch_reverse_dev_region;
static struct class		* switch_reverse_class;
static struct cdev		switch_reverse_cdev;

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
	
	// Create the CM control class
	switch_reverse_class = class_create(THIS_MODULE, MODULE_NAME);
	if(switch_reverse_class == NULL) {
		log("ERROR: Create the \"%s\" class\n", MODULE_NAME);
		goto goto_unregister_chrdev_region;
	}

	// Create the CM control device file system
	if(device_create(switch_reverse_class, NULL, switch_reverse_dev_region, NULL, MODULE_NAME) == NULL) {
		log("ERROR: Create the \"%s\" device file\n", MODULE_NAME);
		goto goto_destroy_class;
	}

	// Register the CM control device as a charactor device
	cdev_init(&switch_reverse_cdev, &switch_reverse_fops);
	ret = cdev_add(&switch_reverse_cdev, switch_reverse_dev_region, 1);
	if(ret < 0) {
		log("ERROR: Register the \"%s\" device as a charactor device\n", MODULE_NAME);
		goto goto_destroy_device;
	}

	// Allocate memory for SwitchManager Platform Data.
	data = kzalloc(sizeof(struct switch_reverse_data), GFP_KERNEL);
	if(!data)
		return -ENOMEM;

	pdata = data;

	// Set the SwitchManager Platform Data.
	pdev->dev.platform_data	= data;

	// pinctrl
	pinctrl = pinctrl_get_select(&pdev->dev, "default");
	if(IS_ERR(pinctrl))
		log("%s: pinctrl select failed\n", MODULE_NAME);
	else
		pinctrl_put(pinctrl);

	data->switch_gpio	= of_get_named_gpio(pdev->dev.of_node, "switch-gpios", 0);

	of_property_read_u32_index(pdev->dev.of_node, "switch-active", 0, &data->switch_active);

	data->enabled		= 0;	// disabled now

	// Create the switchmanager sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_switch_reverse_attr);
	if(ret < 0)
		log("failed create sysfs\r\n");

	FUNCTION_OUT
	return 0;

//goto_unregister_cdev:
//	// Unregister the CM control device
//	cdev_del(&switch_reverse_cdev);

goto_destroy_device:
	// Destroy the CM control device file system
	device_destroy(switch_reverse_class, switch_reverse_dev_region);
	
goto_destroy_class:
	// Destroy the CM control class
	class_destroy(switch_reverse_class);

goto_unregister_chrdev_region:
	// Unregister the charactor device region
	unregister_chrdev_region(switch_reverse_dev_region, 1);

	FUNCTION_OUT
	return -1;
}

int switch_reverse_remove(struct platform_device * pdev) {
	FUNCTION_IN

	// Unregister the CM control device
	cdev_del(&switch_reverse_cdev);

	// Destroy the CM control device file system
	device_destroy(switch_reverse_class, switch_reverse_dev_region);

	// Destroy the CM control class
	class_destroy(switch_reverse_class);

	// Unregister the charactor device region
	unregister_chrdev_region(switch_reverse_dev_region, 1);

	FUNCTION_OUT
	return 0;
}

const struct of_device_id switch_reverse_of_match[] = {
        {.compatible = "telechips,switch_reverse", },
        { },
};
MODULE_DEVICE_TABLE(of, switch_reverse_of_match);

struct platform_driver switch_reverse_driver = {
	.probe		= switch_reverse_probe,
	.remove		= switch_reverse_remove,
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
		.of_match_table = switch_reverse_of_match,
	},
};

module_platform_driver(switch_reverse_driver);

MODULE_AUTHOR("Jay HAN<jjongspi@telechips.com>");
MODULE_DESCRIPTION("SWITCH REVERSE Driver");
MODULE_LICENSE("GPL");

