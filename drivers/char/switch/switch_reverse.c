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
#include <uapi/misc/switch.h>

static int debug = 0;

#define MODULE_NAME				"switch_gpio_reverse"

#define logl(level, fmt, ...)	printk(level "[%s][%s] %s - " pr_fmt(fmt), #level + 5, MODULE_NAME, __FUNCTION__, ##__VA_ARGS__)
#define log(fmt, ...)			logl(KERN_INFO, fmt, ##__VA_ARGS__)
#define loge(fmt, ...)			logl(KERN_ERR, fmt, ##__VA_ARGS__)
#define logw(fmt, ...)			logl(KERN_WARNING, fmt, ##__VA_ARGS__)
#define logd(fmt, ...)			logl(KERN_DEBUG, fmt, ##__VA_ARGS__)
#define dlog(fmt, ...)			do { if(debug) { logl(KERN_DEBUG, fmt, ##__VA_ARGS__); } } while(0)

typedef struct switch_t {
	struct device	* dev_plt;

	dev_t			cdev_region;
	struct class	* cdev_class;
	struct cdev		cdev;

	int				enabled;
	atomic_t		type;
	int				switch_gpio;
	int				switch_active;
	atomic_t		status;

	atomic_t		loglevel;
} switch_t;

int switch_get_type(switch_t * vdev) {
	return atomic_read(&vdev->type);
}

void switch_set_type(switch_t * vdev, int type) {
	atomic_set(&vdev->type, type);
}

ssize_t switch_type_show(struct device * dev, struct device_attribute * attr, char * buf) {
	switch_t * vdev = (switch_t *)dev->platform_data;

	return sprintf(buf, "%d\n", switch_get_type(vdev));
}

ssize_t switch_type_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	switch_t		* vdev	= (switch_t *)dev->platform_data;
	unsigned int	data	= 0;
	int				error	= 0;

	error = kstrtouint(buf, 10, &data);
	if(error)
		return error;

	switch_set_type(vdev, data);

	return count;
}

static DEVICE_ATTR(switch_type, S_IRUGO|S_IWUSR|S_IWGRP, switch_type_show, switch_type_store);

int switch_get_status(switch_t * vdev) {
	int	type		= 0;
	int	gpio		= 0;
	int	gpio_value	= 0;
	int	gpio_active	= 0;
	int	status		= 0;

	// get the switch type
	type = switch_get_type(vdev);

	switch(type) {
	case 0:		// sw switch
		status = atomic_read(&vdev->status);
		break;

	case 1:		// hw switch
		gpio		= vdev->switch_gpio;
		if(gpio != -1) {
			gpio_value	= !!gpio_get_value(gpio);
			gpio_active	= vdev->switch_active;
			if(gpio_active == -1)
				status = gpio_value;
			else
				status = (gpio_value == gpio_active);
			dlog("gpio: %d, value: %d, active: %d, status: %d\n", gpio, gpio_value, gpio_active, status);
		} else {
			loge("HW Switch is not supported, gpio: %d\n", gpio);
			status = atomic_read(&vdev->status);
		}
		break;

	default:
		loge("switch type(%d) is WRONG\n", type);
		break;
	}

	dlog("switch status: %d\n", status);

	return status;
}

void switch_set_status(switch_t * vdev, int status) {
	int	type		= 0;

	// get the switch type
	type = switch_get_type(vdev);

	switch(type) {
	case 0:		// sw switch
		atomic_set(&vdev->status, status);
		break;

	case 1:		// hw switch
		logd("Not supported in case of switch type(%d)\n", type);
		break;

	default:
		loge("switch type(%d) is WRONG\n", type);
		break;
	}

	dlog("switch status: %d\n", status);
}

ssize_t switch_status_show(struct device * dev, struct device_attribute * attr, char * buf) {
	switch_t * vdev = (switch_t *)dev->platform_data;

	return sprintf(buf, "%d\n", atomic_read(&vdev->status));
}

ssize_t switch_status_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	switch_t		* vdev	= (switch_t *)dev->platform_data;
	unsigned int	data	= 0;
	int				error	= 0;

	error = kstrtouint(buf, 10, &data);
	if(error)
		return error;

	switch_set_status(vdev, data);

	return count;
}

static DEVICE_ATTR(switch_status, S_IRUGO|S_IWUSR|S_IWGRP, switch_status_show, switch_status_store);

ssize_t loglevel_show(struct device * dev, struct device_attribute * attr, char * buf) {
	switch_t		* vdev	= (switch_t *)dev->platform_data;

	return sprintf(buf, "%d\n", atomic_read(&vdev->loglevel));
}

ssize_t loglevel_store(struct device * dev, struct device_attribute * attr, const char * buf, size_t count) {
	switch_t		* vdev	= (switch_t *)dev->platform_data;
	unsigned int	data	= 0;
	int				error	= 0;

	error = kstrtouint(buf, 10, &data);
	if(error)
		return error;

	atomic_set(&vdev->loglevel, data);
	debug = data;

	return count;
}

static DEVICE_ATTR(loglevel, S_IRUGO|S_IWUSR|S_IWGRP, loglevel_show, loglevel_store);

long switch_ioctl(struct file * filp, unsigned int cmd, long unsigned int arg) {
	switch_t		* vdev	= filp->private_data;
	int				status	= 0;
	int				ret		= 0;

	switch(cmd) {
	case SWITCH_IOCTL_CMD_ENABLE:
		vdev->enabled		= 1;
		dlog("enabled: %d\n", vdev->enabled);
		break;

	case SWITCH_IOCTL_CMD_DISABLE:
		vdev->enabled		= 0;
		dlog("enabled: %d\n", vdev->enabled);
		break;

	case SWITCH_IOCTL_CMD_GET_STATE:
		status = switch_get_status(vdev);
		dlog("status: %d\n", status);

		if((ret = copy_to_user((void *)arg, (const void *)&status, sizeof(status))) < 0) {
			loge("FAILED: copy_to_user\n");
			ret = -1;
			break;
		}
		break;

	default:
		loge("FAILED: Unsupported command\n");
		ret = -1;
		break;
	}
	return ret;
}

int switch_open(struct inode * inode, struct file * filp) {
	switch_t	* vdev = container_of(inode->i_cdev, switch_t, cdev);

	filp->private_data = vdev;

	return 0;
}

int switch_release(struct inode * inode, struct file * filp) {
	return 0;
}

struct file_operations switch_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= switch_ioctl,
	.open			= switch_open,
	.release		= switch_release,
};

int switch_probe(struct platform_device * pdev) {
	switch_t		* vdev		= NULL;
	struct pinctrl	* pinctrl	= NULL;
	int				index		= 0;
	char			name[32]	= "";
	int				ret			= 0;

	// allocate memory for switch device data.
	vdev = kzalloc(sizeof(switch_t), GFP_KERNEL);
	if(vdev == NULL) {
		loge("Allocate a videosource device struct.\n");
		return -ENOMEM;
	}

	// clear switch device data
	memset(vdev, 0x00, sizeof(switch_t));

	if(pdev->dev.of_node) {
		pdev->dev.platform_data = vdev;
	} else {
		loge("Find a device tree.\n");
		return -ENODEV;
	}

	// get the index from its alias
	index = of_alias_get_id(pdev->dev.of_node, /*MODULE_NAME*/"switch");

	// set the charactor device name
	if((index == -ENODEV) || (index == 0)) {
		sprintf(name, "%s", MODULE_NAME);
	} else {
		sprintf(name, "%s%d", MODULE_NAME, index);
	}

	// allocate a charactor device region
	ret = alloc_chrdev_region(&vdev->cdev_region, 0, 1, name);
	if(ret < 0) {
		loge("Allocate a charactor device region for the \"%s\"\n", name);
		return ret;
	}

	// create a class
	vdev->cdev_class = class_create(THIS_MODULE, name);
	if(vdev->cdev_class == NULL) {
		loge("Create the \"%s\" class\n", name);
		goto goto_unregister_chrdev_region;
	}

	// create a device file system
	if(device_create(vdev->cdev_class, NULL, vdev->cdev_region, NULL, name) == NULL) {
		loge("Create the \"%s\" device file\n", name);
		goto goto_destroy_class;
	}

	// register a device as a charactor device
	cdev_init(&vdev->cdev, &switch_fops);
	ret = cdev_add(&vdev->cdev, vdev->cdev_region, 1);
	if(ret < 0) {
		loge("Register the \"%s\" device as a charactor device\n", name);
		goto goto_destroy_device;
	}

	vdev->enabled		= 0;

	// pinctrl
	pinctrl = pinctrl_get_select(&pdev->dev, "default");
	if(IS_ERR(pinctrl))
		logd("\"pinctrl\" node is not found.\n");
	else
		pinctrl_put(pinctrl);

	vdev->switch_gpio	= -1;
	vdev->switch_active	= -1;
	if(of_parse_phandle(pdev->dev.of_node, "switch-gpios", 0) != NULL) {
		vdev->switch_gpio	= of_get_named_gpio(pdev->dev.of_node, "switch-gpios", 0);
		of_property_read_u32_index(pdev->dev.of_node, "switch-active", 0, &vdev->switch_active);
		logd("switch-gpios: %d, switch-active: %d\n", vdev->switch_gpio, vdev->switch_active);
	} else {
		logd("\"switch-gpios\" node is not found.\n");
	}

	// Create the type sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_switch_type);
	if(ret < 0)
		loge("failed create sysfs, type\r\n");

	// Set type as 1 if there is a hw switch node
	if(vdev->switch_gpio != -1) {
		switch_set_type(vdev, 1);
	}

	// Create the status sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_switch_status);
	if(ret < 0)
		loge("failed create sysfs, status\r\n");

	// Create the loglevel sysfs
	ret = device_create_file(&pdev->dev, &dev_attr_loglevel);
	if(ret < 0)
		loge("failed create sysfs, loglevel\r\n");

	goto goto_end;

goto_destroy_device:
	// destroy the device file system
	device_destroy(vdev->cdev_class, vdev->cdev_region);
	
goto_destroy_class:
	// destroy the class
	class_destroy(vdev->cdev_class);

goto_unregister_chrdev_region:
	// unregister the charactor device region
	unregister_chrdev_region(vdev->cdev_region, 1);

goto_end:
	return ret;
}

int switch_remove(struct platform_device * pdev) {
	switch_t	* vdev		= NULL;

	vdev = pdev->dev.platform_data;

	// unregister the device
	cdev_del(&vdev->cdev);

	// destroy the device file system
	device_destroy(vdev->cdev_class, vdev->cdev_region);

	// destroy the class
	class_destroy(vdev->cdev_class);

	// unregister the charactor device region
	unregister_chrdev_region(vdev->cdev_region, 1);

	return 0;
}

#ifdef CONFIG_OF
const struct of_device_id switch_of_match[] = {
        { .compatible = "telechips,switch", },
        { },
};
MODULE_DEVICE_TABLE(of, switch_of_match);
#endif//CONFIG_OF

struct platform_driver switch_driver = {
	.probe		= switch_probe,
	.remove		= switch_remove,
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = switch_of_match,
#endif//CONFIG_OF
	},
};

module_platform_driver(switch_driver);

MODULE_AUTHOR("Jay HAN<jjongspi@telechips.com>");
MODULE_DESCRIPTION("SWITCH Driver");
MODULE_LICENSE("GPL");

