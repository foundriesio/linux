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
/*#############################################################################*/

//global

#define GPIO_INT_EDGE_RISING	0
#define GPIO_INT_EDGE_FALLING	1
#define GPIO_INT_EDGE_BOTH	2
#define GPIO_INT_LEVEL_HIGH	3
#define GPIO_INT_LEVEL_LOW	4

//#define DEBUG_GPIO

#ifdef DEBUG_GPIO
#define debug_gpio(msg...) pr_info("[DEBUG][GPIO SAMPLE DRIVER]" msg);
#define err_gpio(msg...) pr_err("[ERROR][GPPIO SAMPLE DRIVER]" msg);
#else
#define debug_gpio(msg...)
#define err_gpio(msg...)
#endif


extern int tcc_gpio_config(unsigned, unsigned);

struct gpio_data {
	int gpio;
	const char *desc;
	int value;
	int irq_enable;
	int irq;
	unsigned int irq_type;
};

struct gpio_sample_platform_data {
	struct gpio_data *gdata;
	const char *name;
};

/*#############################################################################*/


/*#############################################################################*/

//sysfs

static ssize_t gpio_dir_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	if(!gpio_is_valid(pdata->gdata->gpio)) {
		return -ENODEV;
	}

	return sprintf(buf,"direction out : 1, direction in : 0\n");
}

static ssize_t gpio_dir_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	int dir;
	int ret;

	if(!gpio_is_valid(pdata->gdata->gpio)) {
		return -ENODEV;
	}

	gpio_request(pdata->gdata->gpio, "TCC gpio sample");

	ret = kstrtoint(buf, 10, &dir);
	if(ret)
		return ret;

	dev_set_drvdata(dev, pdata);

	if(dir)
		gpio_direction_output(pdata->gdata->gpio, pdata->gdata->value);
	else
		gpio_direction_input(pdata->gdata->gpio);

	return count;
}


static ssize_t gpio_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	if(!gpio_is_valid(pdata->gdata->gpio)) {
		return -ENODEV;
	}

	return sprintf(buf,"gpio : %d\n",pdata->gdata->gpio);
}

static ssize_t gpio_num_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	int gpio;
	int ret;

	if(!gpio_is_valid(pdata->gdata->gpio)) {
		return -ENODEV;
	}


	ret = kstrtoint(buf, 10, &gpio);
	if(ret)
		return ret;

	pdata->gdata->gpio=gpio;
	pdata->gdata->value=0;

	dev_set_drvdata(dev, pdata);

	return count;
}


static ssize_t gpio_val_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	int val;
	if(!gpio_is_valid(pdata->gdata->gpio)) {
		return -ENODEV;
	}


	val = gpio_get_value(pdata->gdata->gpio);
	pdata->gdata->value=val;

	return sprintf(buf,"gpio : %d, value : %d\n",pdata->gdata->gpio, val);
}

static ssize_t gpio_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct gpio_sample_platform_data *pdata = dev_get_drvdata(dev);
	int val;
	int ret;

	if(!gpio_is_valid(pdata->gdata->gpio)) {
		return -ENODEV;
	}


	ret = kstrtoint(buf, 10 , &val);
	if(ret)
		return ret;

	gpio_set_value(pdata->gdata->gpio, val);

	return count;
}



static DEVICE_ATTR(gpio_dir, S_IRUGO|S_IWUSR, gpio_dir_show, gpio_dir_store);
static DEVICE_ATTR(gpio_num, S_IRUGO|S_IWUSR, gpio_num_show, gpio_num_store);
static DEVICE_ATTR(gpio_val, S_IRUGO|S_IWUSR, gpio_val_show, gpio_val_store);

static struct attribute *gpio_sample_attrs[] = {
	&dev_attr_gpio_dir.attr,
	&dev_attr_gpio_val.attr,
	&dev_attr_gpio_num.attr,
	NULL,
};

static const struct attribute_group gpio_sample_attr_group = {
	.attrs = gpio_sample_attrs,
};

/*#############################################################################*/

//device tree parsing

#ifdef CONFIG_OF
	static struct gpio_sample_platform_data *
gpio_sample_get_devtree_pdata(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct gpio_sample_platform_data *pdata;
	struct gpio_data *gdata;
	enum of_gpio_flags flags;
	int error;

	if (!node)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev,
			sizeof(*pdata) + sizeof(*gdata),
			GFP_KERNEL);
	gdata = devm_kzalloc(dev, sizeof(*gdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gdata = gdata;

	gdata->gpio = of_get_gpio_flags(node, 0, &flags); //get gpio from device tree
	if (gdata->gpio < 0) {
		error = gdata->gpio;
		if (error != -ENOENT) {
			if (error != -EPROBE_DEFER)
				dev_err(dev,
						"Failed to get gpio flags, error: %d\n",
						error);
			return ERR_PTR(error);
		}
	}

	if (!gpio_is_valid(gdata->gpio)) {
		dev_err(dev, "Found button without gpios or irqs\n");
		return ERR_PTR(-EINVAL);
	}

	gdata->desc = of_get_property(node, "label", NULL); //get label from device tree

	return pdata;
}

static const struct of_device_id gpio_sample_of_match[] = {
	{ .compatible = "gpio-sample", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_sample_of_match);

#else

	static inline struct gpio_sample_platform_data *
gpio_sample_get_devtree_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

/*#############################################################################*/


//isr function

static irqreturn_t gpio_sample_isr(int irq, void *dev)
{
        //struct gpio_button_data *pdata = dev_get_platdata(dev);

	debug_gpio("gpio sample interrupt");

        return IRQ_HANDLED;
}


/*#############################################################################*/


static int gpio_sample_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_sample_platform_data *pdata = dev_get_platdata(dev);
	//struct gpio_struct *gpio_desc;
	irq_handler_t isr;
	unsigned long irq_type = 0;
	int irq = 0;
	int error;

	if(!pdata) {
		pdata = gpio_sample_get_devtree_pdata(dev);
		if (IS_ERR(pdata)){
			error = PTR_ERR(pdata);
			goto err_remove_group;
		}
		if (!pdata) {
			dev_err(dev, "missing platform data\n");
			error = -EINVAL;
			goto err_remove_group;
		}
	}

	pdata->gdata->irq_enable=0;

	error = gpio_request(pdata->gdata->gpio, "sample_gpio"); 

	if(error) {
		goto err_remove_group;
	}

//set irq

	isr = gpio_sample_isr;
	irq = platform_get_irq(pdev, 0);
	irq_type = irq_get_trigger_type(irq);

	if(irq == 0) {
	    debug_gpio("%s: irq null\n", __func__);
	    return -EINVAL;
	}

	debug_gpio("############################################irq : %d##################################################", irq);
	if(irq < 0)
		goto err_remove_group;

	irq_set_status_flags(irq, IRQ_NOAUTOEN);

	if ((irq_type == IRQF_TRIGGER_RISING)||
		(irq_type == IRQF_TRIGGER_FALLING)||
		(irq_type == (IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING))) {
		debug_gpio("%s:flag edge %lu\n", __func__, irq_type);
	// 1st : irq number, 2nd : interrupt service routine,
	// 3rd : irq type(if '0', default type at device tree node is used),
	// 4th : name, 5th : parameter to isr.
		error = request_any_context_irq(irq, isr, 0,
			    "sample_gpio_interrupt", dev);
	} else if (irq_type == IRQF_TRIGGER_HIGH) {
		debug_gpio("%s:flag high %lu\n", __func__, irq_type);
	// 1st : irq number, 2nd : primary handler(should be NULL),
	// 3rd : thread handler,
	// 4th : irq type,
	// 5th : name, 6th : parameter to isr.
		error = request_threaded_irq(irq, NULL, isr,
			IRQF_TRIGGER_HIGH|IRQF_ONESHOT,
			    "sample_gpio_interrupt", dev);
	} else if (irq_type == IRQF_TRIGGER_LOW) {
		debug_gpio("%s:flag low %lu\n", __func__, irq_type);
	// 1st : irq number, 2nd : primary handler(should be NULL),
	// 3rd : thread handler,
	// 4th : irq type,
	// 5th : name, 6th : parameter to isr.
		error = request_threaded_irq(irq, NULL, isr,
			IRQF_TRIGGER_LOW|IRQF_ONESHOT,
			    "sample_gpio_interrupt", dev);
	}


	enable_irq(irq);

        if (error) {
                dev_err(dev, "Unable requests irq, error: %d\n",
                                error);
		free_irq(irq, dev);
                goto err_remove_group;
        }

	dev_set_drvdata(dev, pdata);

	error = sysfs_create_group(&pdev->dev.kobj, &gpio_sample_attr_group); //create nodes to sysfs with attribute_group
	if (error) {
		dev_err(dev, "Unable to export sample/switches, error: %d\n",
				error);
		goto err_remove_group;
	}



	return 0;

err_remove_group:
	sysfs_remove_group(&pdev->dev.kobj, &gpio_sample_attr_group);
	return error;
}

static int gpio_sample_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &gpio_sample_attr_group);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gpio_sample_suspend(struct device *dev)
{

	return 0;
}

static int gpio_sample_resume(struct device *dev)
{
	return 0;
}
#endif


static SIMPLE_DEV_PM_OPS(gpio_sample_pm_ops, gpio_sample_suspend, gpio_sample_resume);

static struct platform_driver gpio_sample_device_driver = {
	.probe		= gpio_sample_probe,
	.remove		= gpio_sample_remove,
	.driver		= {
		.name	= "gpio-tcc-sample",
		.pm	= &gpio_sample_pm_ops,
		.of_match_table = of_match_ptr(gpio_sample_of_match),
	}
};

static int __init gpio_sample_init(void)
{
	return platform_driver_register(&gpio_sample_device_driver);
}

static void __exit gpio_sample_exit(void)
{
	platform_driver_unregister(&gpio_sample_device_driver);
}

late_initcall(gpio_sample_init);
module_exit(gpio_sample_exit);

MODULE_AUTHOR("Telechips.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO sample driver");
MODULE_ALIAS("platform:tcc-gpio-sample");
