/*******************************************************************************
 *
 * Copyright (C) 2010 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 * .
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ******************************************************************************/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#endif/* defined(CONFIG_OF) */

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <uapi/misc/switch.h>

static int32_t			debug;

#define MODULE_NAME		("switch_gpio_reverse")

#define LOG_TAG			("SWITCH")

#define loge(fmt, ...)		{ pr_err("[ERROR][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define logw(fmt, ...)		{ pr_warn("[WARN][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define logd(fmt, ...)		{ pr_debug("[DEBUG][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define logi(fmt, ...)		{ pr_info("[INFO][%s] %s - " fmt, LOG_TAG, \
					__func__, ##__VA_ARGS__); }
#define dlog(fmt, ...)		do { if (debug) { ; logd(fmt, \
					##__VA_ARGS__); } while (0)

struct switch_dev {
	struct device		*dev_plt;

	dev_t			cdev_region;
	struct class		*cdev_class;
	struct cdev		cdev;

	uint32_t		enabled;
	atomic_t		type;
	int32_t			switch_gpio;
	int32_t			switch_active;
	atomic_t		status;

	atomic_t		loglevel;
};

int32_t switch_get_type(struct switch_dev *vdev)
{
	return (int32_t)atomic_read(&vdev->type);
}

int32_t switch_set_type(struct switch_dev *vdev, int32_t type)
{
	int32_t			ret		= 0;

	if (vdev == NULL) {
		// vdev is NULL
		return -1;
	}

	atomic_set(&vdev->type, type);

	return ret;
}

ssize_t switch_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rvalue;
	struct switch_dev *vdev;

	vdev = (struct switch_dev *)dev->platform_data;
	rvalue = sprintf(buf, "%d\n", switch_get_type(vdev));
	return rvalue;
}

ssize_t switch_type_store(struct device *dev,
	struct device_attribute *attr, const char *buf,	size_t count)
{
	struct switch_dev *vdev;
	int32_t error = 0;
	uint32_t data = 0;

	if (dev == NULL) {
		loge("An object of device is NULL\n");
		return -1;
	}

	vdev = (struct switch_dev *)dev->platform_data;
	error = kstrtouint(buf, 10, &data);
	if (error > 0) {
		/* Return if error occurs */
		return error;
	}

	switch_set_type(vdev, (int32_t)data);

	return (ssize_t)count;
}

DEVICE_ATTR(switch_type, 0664, switch_type_show, switch_type_store);

int32_t switch_get_status(struct switch_dev *vdev)
{
	int32_t	type		= 0;
	int32_t	gpio		= 0;
	int32_t	gpio_value	= 0;
	int32_t	gpio_active	= 0;
	int32_t	status		= 0;

	if (vdev == NULL) {
		loge("An object of vdev is NULL");
		return -1;
	}

	/* get the switch type */
	type = switch_get_type(vdev);

	switch (type) {
	/* sw switch */
	case 0:
		status = atomic_read(&vdev->status);
		break;
	/* hw switch */
	case 1:
		gpio		= vdev->switch_gpio;
		if (gpio != -1) {
			gpio_value	= (!!gpio_get_value(gpio) == 1) ? 1 : 0;
			gpio_active	= vdev->switch_active;
			if (gpio_active == -1) {
				/* set gpio_value to status as an initialization
				 */
				status = gpio_value;
			} else {
				/* set status value */
				status = (gpio_value == gpio_active) ? 1 : 0;
			}
			logd("gpio: %d, value: %d, active: %d, status: %d\n",
				gpio, gpio_value, gpio_active, status);

			atomic_set(&vdev->status, status);
		} else {
			loge("HW Switch is not supported, gpio: %d\n", gpio);
			status = atomic_read(&vdev->status);
		}
		break;

	default:
		loge("switch type(%d) is WRONG\n", type);
		break;
	}

	logd("switch status: %d\n", status);

	return status;
}

int32_t switch_set_status(struct switch_dev *vdev, int32_t status)
{
	int32_t			type		= 0;
	int32_t			ret		= 0;

	if (vdev == NULL) {
		loge("vdev is NULL\n");
		return -1;
	}

	/* get the switch type */
	type = switch_get_type(vdev);

	switch (type) {
	/* sw switch */
	case 0:
		atomic_set(&vdev->status, status);
		break;
	/* hw switch */
	case 1:
		logd("Not supported in case of switch type(%d)\n", type);
		break;

	default:
		loge("switch type(%d) is WRONG\n", type);
		break;
	}

	logd("switch status: %d\n", status);

	return ret;
}

ssize_t switch_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct switch_dev *vdev;
	ssize_t rvalue;

	if (dev == NULL) {
		loge("dev is NULL\n");
		return -1;
	}

	vdev = (struct switch_dev *)dev->platform_data;
	rvalue = sprintf(buf, "%d\n", atomic_read(&vdev->status));

	return rvalue;
}

ssize_t switch_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct switch_dev *vdev;
	uint32_t data	= 0;
	int32_t	error	= 0;

	if (dev == NULL) {
		loge("dev is NULL\n");
		return -1;
	}

	vdev = (struct switch_dev *)dev->platform_data;
	error = kstrtouint(buf, 10, &data);
	if (error > 0) {
		/* return if error occurs */
		return error;
	}

	switch_set_status(vdev, (int32_t)data);

	return (ssize_t)count;
}

DEVICE_ATTR(switch_status, 0664, switch_status_show, switch_status_store);

ssize_t loglevel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rvalue;
	struct switch_dev *vdev;

	if (dev == NULL) {
		loge("dev is NULL\n");
		return -1;
	}

	vdev = (struct switch_dev *)dev->platform_data;
	rvalue = sprintf(buf, "%d\n", atomic_read(&vdev->loglevel));

	return rvalue;
}

ssize_t loglevel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct switch_dev	*vdev;
	uint32_t		data	= 0;
	int32_t			error	= 0;

	if (dev == NULL) {
		loge("dev is NULL\n");
		return -1;
	}

	vdev = (struct switch_dev *)dev->platform_data;
	error = kstrtouint(buf, 10, &data);
	if (error > 0) {
		/* return if error occurs */
		return error;
	}

	atomic_set(&vdev->loglevel, (int32_t)data);
	debug = (int32_t)data;

	return (ssize_t)count;
}

DEVICE_ATTR(loglevel, 0664, loglevel_show, loglevel_store);

long switch_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
{
	struct switch_dev	*vdev;
	int32_t			status	= 0;
	unsigned long		byte	= 0;
	int32_t			ret	= 0;

	if (filp == NULL) {
		loge("filp is NULL\n");
		return -1;
	}

	vdev = filp->private_data;

	switch (cmd) {
	case SWITCH_IOCTL_CMD_ENABLE:
		vdev->enabled	= 1;
		logd("enabled: %d\n", vdev->enabled);
		break;

	case SWITCH_IOCTL_CMD_DISABLE:
		vdev->enabled	= 0;
		logd("enabled: %d\n", vdev->enabled);
		break;

	case SWITCH_IOCTL_CMD_GET_STATE:
		status		= switch_get_status(vdev);
		logd("status: %d\n", status);

		byte = copy_to_user((void *)arg, (const void *)&status,
			sizeof(status));
		if (byte < 0) {
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

int32_t switch_open(struct inode *inode, struct file *filp)
{
	struct switch_dev	*vdev;

	if (inode == NULL) {
		loge("Failed to open switch device: inode is NULL\n");
		return -1;
	}

	if (filp == NULL) {
		loge("filp is NULL\n");
		return -1;
	}

	vdev = container_of(inode->i_cdev, struct switch_dev, cdev);
	filp->private_data = vdev;
	return 0;
}

int32_t switch_release(struct inode *inode, struct file *filp)
{
	struct switch_dev	*vdev;

	if (inode == NULL) {
		loge("Failed to open switch device: inode is NULL\n");
		return -1;
	}

	if (filp == NULL) {
		loge("filp is NULL\n");
		return -1;
	}

	vdev = container_of(inode->i_cdev, struct switch_dev, cdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t switch_suspend(struct device *dev)
{
	return 0;
}

static int32_t switch_resume(struct device *dev)
{
	struct pinctrl *pinctrl	= NULL;

	// pinctrl
	if (dev == NULL) {
		loge("switch device is not found.\n");
	} else {
		pinctrl = pinctrl_get_select(dev, "default");
		if (IS_ERR(pinctrl)) {
			// print out the error
			logd("\"pinctrl\" node is not found.\n");
		}
	}

	return 0;
}

static const struct dev_pm_ops swich_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(switch_suspend, switch_resume)
};

#define DEV_PM_OPS  (&swich_pm_ops)
#else
#define DEV_PM_OPS  NULL
#endif

const struct file_operations switch_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= switch_ioctl,
	.open		= switch_open,
	.release	= switch_release,
};

int32_t switch_probe(struct platform_device *pdev)
{
	struct switch_dev	*vdev		= NULL;
	struct pinctrl		*pinctrl	= NULL;
	struct device		*dev_ret	= NULL;
	int32_t			index		= 0;
	int32_t			ret		= 0;
	int8_t			name[32]	= "";
	struct device_node	*phandle	= NULL;

	/* allocate memory for switch device data */
	vdev = kzalloc(sizeof(struct switch_dev), GFP_KERNEL);
	if (vdev == NULL) {
		loge("Allocate a videosource device struct.\n");
		return -ENOMEM;
	}

	/* clear switch device data */
	memset(vdev, 0x00, sizeof(struct switch_dev));

	if (pdev->dev.of_node != NULL) {
		pdev->dev.platform_data = vdev;
	} else {
		loge("Find a device tree.\n");
		return -ENODEV;
	}

	/* get the index from its alias */
	index = of_alias_get_id(pdev->dev.of_node, /*MODULE_NAME*/"switch");

	/* set the character device name */
	if ((index == -ENODEV) || (index == 0)) {
		/* if the device name is like "/dev/name" */
		sprintf(name, "%s", MODULE_NAME);
	} else {
		/* if the device name is like "/dev/name{n}" */
		sprintf(name, "%s%d", MODULE_NAME, index);
	}
	/* allocate a character device region */
	ret = alloc_chrdev_region(&vdev->cdev_region, 0, 1, name);
	if (ret < 0) {
		loge("Allocate a character device region for the \"%s\"\n",
			name);
		return ret;
	}

	/* create a class */
	vdev->cdev_class = class_create(THIS_MODULE, name);
	if (vdev->cdev_class == NULL) {
		loge("Create the \"%s\" class\n", name);
		goto goto_unregister_chrdev_region;
	}

	/* create a device file system */
	dev_ret = device_create(vdev->cdev_class, NULL, vdev->cdev_region, NULL,
		name);
	if (dev_ret == NULL) {
		loge("Create the \"%s\" device file\n", name);
		goto goto_destroy_class;
	}

	/* register a device as a character device */
	cdev_init(&vdev->cdev, &switch_fops);
	ret = cdev_add(&vdev->cdev, vdev->cdev_region, 1);
	if (ret < 0) {
		loge("Register the \"%s\" device as a character device\n",
			name);
		goto goto_destroy_device;
	}

	/* parse device tree */
	vdev->switch_gpio	= -1;
	vdev->switch_active	= -1;

	/* pinctrl */
	pinctrl = pinctrl_get_select(&pdev->dev, "default");
	if (IS_ERR(pinctrl)) {
		/* print out the error */
		logd("\"pinctrl\" node is not found.\n");
	} else {
		/* otherwise, do pin-control */
		pinctrl_put(pinctrl);

		phandle = of_parse_phandle(pdev->dev.of_node,
			"switch-gpios", 0);
		if (phandle != NULL) {
			vdev->switch_gpio = of_get_named_gpio(pdev->dev.of_node,
				"switch-gpios", 0);
			of_property_read_u32_index(pdev->dev.of_node,
				"switch-active", 0, &vdev->switch_active);
			logd("switch-gpios: %d, switch-active: %d\n",
				vdev->switch_gpio, vdev->switch_active);
		} else {
			logd("\"switch-gpios\" node is not found.\n");
		}
	}

	/* Create the type sysfs */
	ret = device_create_file(&pdev->dev, &dev_attr_switch_type);
	if (ret < 0) {
		/* print out the error */
		loge("failed create sysfs, type\r\n");
	}
	if (vdev->switch_gpio != -1) {
		/* Set type as 1 if there is a hw switch node */
		switch_set_type(vdev, 1);
	}

	/* Create the status sysfs */
	ret = device_create_file(&pdev->dev, &dev_attr_switch_status);
	if (ret < 0) {
		/* error */
		loge("failed create sysfs, status\r\n");
	}
	/* Create the loglevel sysfs */
	ret = device_create_file(&pdev->dev, &dev_attr_loglevel);
	if (ret < 0) {
		/* error */
		loge("failed create sysfs, loglevel\r\n");
	}
	goto goto_end;

goto_destroy_device:
	/* destroy the device file system */
	device_destroy(vdev->cdev_class, vdev->cdev_region);

goto_destroy_class:
	/* destroy the class */
	class_destroy(vdev->cdev_class);

goto_unregister_chrdev_region:
	/* unregister the character device region */
	unregister_chrdev_region(vdev->cdev_region, 1);

goto_end:
	return ret;
}

int32_t switch_remove(struct platform_device *pdev)
{
	struct switch_dev	*vdev		= NULL;

	if (pdev == NULL) {
		loge("An instance of pdev is NULL\n");
		return -1;
	}

	vdev = pdev->dev.platform_data;

	if (vdev == NULL) {
		loge("An instance of vdev is NULL\n");
		return -1;
	}

	/* unregister the device */
	cdev_del(&vdev->cdev);

	/* destroy the device file system */
	device_destroy(vdev->cdev_class, vdev->cdev_region);

	/* destroy the class */
	class_destroy(vdev->cdev_class);

	/* unregister the character device region */
	unregister_chrdev_region(vdev->cdev_region, 1);

	return 0;
}

#if defined(CONFIG_OF)
const struct of_device_id switch_of_match[1] = {
	{ .compatible = "telechips,switch", }
};
MODULE_DEVICE_TABLE(of, switch_of_match);
#endif/* defined(CONFIG_OF) */

struct platform_driver switch_driver = {
	.probe		= switch_probe,
	.remove		= switch_remove,
	.driver		= {
		.name		= MODULE_NAME,
		.owner		= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= switch_of_match,
#endif/* defined(CONFIG_OF) */
		.pm = DEV_PM_OPS,
	},
};

module_platform_driver(switch_driver);

MODULE_AUTHOR("Jay HAN<jjongspi@telechips.com>");
MODULE_DESCRIPTION("SWITCH Driver");
MODULE_LICENSE("GPL");
