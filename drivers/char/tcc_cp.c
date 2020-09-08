/*
 * linux/drivers/char/tcc_cp.c
 *
 * Author:  <android_ce@telechips.com>
 * Created: 7th May, 2013
 * Description: Telechips Linux CP-GPIO DRIVER
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <linux/io.h>
#include <linux/uaccess.h>
#include <mach/tcccp_ioctl.h>

#define TCC_CP_DEV_NAME			        "tcc-cp"
#define TCC_CP_TYPE_20B                 0
#define TCC_CP_TYPE_20C                 1
#define TCC_CP_TYPE_030                 2
#define TCC_CP_NOERR                    0
#define TCC_CP_GPIO_HI			        1
#define TCC_CP_GPIO_LOW			        0
#define TCC_CP_STATE_ENABLE             1
#define TCC_CP_STATE_DISABLE            0
#define TCC_CP_MATCH_SIZE               2

static int gCPPowerState;
static int gCPPowerGpio;
static int gCPResetGpio;
static unsigned int gCpVersion;
static unsigned int gCpI2cChannel;
static unsigned int gCpState;
static struct class *tcc_cp_class;
static struct device *tcc_cp_dev;

void tcc_cp_gpio_init(struct device *dev)
{
	int err = TCC_CP_NOERR;

	dev_dbg(dev, "%s\n", __func__);
	if (gCPPowerState == 1) {
		if (!gpio_is_valid(gCPPowerGpio)) {
			dev_err(dev, "can't find dev of node: cp reset gpio\n");
			return;
		}

		err = gpio_request(gCPPowerGpio, "cp_power_gpio");
		if (err) {
			dev_err(dev, "can't request cp reset gpio\n");
			return;
		}
		gpio_direction_output(gCPPowerGpio, 0);
	}

	if (gpio_is_valid(gCPResetGpio)) {
		err = gpio_request(gCPResetGpio, "cp_reset_gpio");
		if (err) {
			dev_err(dev, "can't request cp reset gpio [%d]\n",
				gCPResetGpio);
			return;
		}
		gpio_direction_output(gCPResetGpio, 0);
	} else {
		if (gCpVersion == 0x20B)
			dev_err(dev,
				"Invalid GPIO Set in Device Tree: Reset GPIO[%d]\n",
				gCPResetGpio);
		else if (gCpVersion == 0x20C)
			dev_warn(dev,
				 "I2C Slave Address Will Not Be Changed: GPIO[%d]\n",
				 gCPResetGpio);
		else
			dev_dbg(dev, "No Need To Control Reset: GPIO[%d]\n",
				gCPResetGpio);
	}
}

void tcc_cp_pwr_control(struct device *dev, int value)
{
	if (gCPPowerState == 1) {
		dev_dbg(dev, "%s value : %d\n", __func__, value);
		if (gpio_cansleep(gCPPowerGpio))
			gpio_set_value_cansleep(gCPPowerGpio, value);
		else
			gpio_set_value(gCPPowerGpio, value);
	}
}

void tcc_cp_reset_control(struct device *dev, int value)
{
	if ((gCPPowerState == 1) || (gCpVersion == 0x20B)) {
		dev_dbg(dev, "%s value : %d\n", __func__, value);
		if (gpio_cansleep(gCPResetGpio))
			gpio_set_value_cansleep(gCPResetGpio, value);
		else
			gpio_set_value(gCPResetGpio, value);

		if (gCpVersion == 0x20B)
			mdelay(30);
	}
}

long tcc_cp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct device *dev = tcc_cp_dev;

	dev_dbg(dev, "%s  (0x%x)\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_CP_CTRL_INIT:
		{
			tcc_cp_gpio_init(dev);
			dev_dbg(dev, "%s:  IOCTL_CP_SET_GPIO_INIT (0x%x)\n",
				__func__, cmd);
		}
		break;
	case IOCTL_CP_CTRL_PWR:
		{
			if (arg != 0)
				tcc_cp_pwr_control(dev, TCC_CP_GPIO_HI);
			else
				tcc_cp_pwr_control(dev, TCC_CP_GPIO_LOW);
			dev_dbg(dev, "%s:  IOCTL_CP_CTRL_PWR arg(%lu)\n",
				__func__, arg);
		}
		break;
	case IOCTL_CP_CTRL_RESET:
		{
			if (arg != 0)
				tcc_cp_reset_control(dev, TCC_CP_GPIO_HI);
			else
				tcc_cp_reset_control(dev, TCC_CP_GPIO_LOW);

			dev_dbg(dev, "%s:  IOCTL_CP_CTRL_RESET arg(%lu)\n",
				__func__, arg);
		}
		break;
	case IOCTL_CP_CTRL_ALL:
		{
			if (arg != 0) {
				tcc_cp_pwr_control(dev, TCC_CP_GPIO_HI);
				tcc_cp_reset_control(dev, TCC_CP_GPIO_HI);
			} else {
				tcc_cp_pwr_control(dev, TCC_CP_GPIO_LOW);
				tcc_cp_reset_control(dev, TCC_CP_GPIO_LOW);
			}

			dev_dbg(dev, "%s:  IOCTL_CP_CTRL_ALL arg(%lu)\n",
				__func__, arg);
		}
		break;
	case IOCTL_CP_GET_VERSION:
		if (copy_to_user
		    ((unsigned int *)arg, &gCpVersion,
		     sizeof(unsigned int)) != 0) {
			dev_dbg(dev, " %s copy version error~!\n", __func__);
		}
		break;
	case IOCTL_CP_GET_CHANNEL:
		if (copy_to_user
		    ((unsigned int *)arg, &gCpI2cChannel,
		     sizeof(unsigned int)) != 0) {
			dev_dbg(dev, " %s copy channel error~!\n", __func__);
		}
		break;
	case IOCTL_CP_SET_STATE:
		if (arg != 0)
			gCpState = TCC_CP_STATE_ENABLE;
		else
			gCpState = TCC_CP_STATE_DISABLE;
		break;
	case IOCTL_CP_GET_STATE:
		if (copy_to_user
		    ((unsigned int *)arg, &gCpState,
		     sizeof(unsigned int)) != 0) {
			dev_dbg(dev, " %s copy state error~!\n", __func__);
		}
		break;
	default:
		dev_dbg(dev, "cp: unrecognized ioctl (0x%x)\n", cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int tcc_cp_release(struct inode *inode, struct file *filp)
{
	struct device *dev = tcc_cp_dev;

	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int tcc_cp_open(struct inode *inode, struct file *filp)
{
	struct device *dev = tcc_cp_dev;

	dev_dbg(dev, "%s :\n", __func__);
	return 0;
}

const struct file_operations tcc_cp_fops = {
	.owner = THIS_MODULE,
	.open = tcc_cp_open,
	.release = tcc_cp_release,
	.unlocked_ioctl = tcc_cp_ioctl,
};

#ifdef CONFIG_OF
static void tcc_cp_read_device_tree(struct device *dev)
{
	unsigned int temp = 0;

	if (of_find_property(dev->of_node, "cp-type", 0)) {
		of_property_read_u32(dev->of_node, "cp-type", &temp);
		/* Auth CP type 2 = 3.0, */
		/* Auth CP type 1 = 2.0C */
		/* Auth CP type 0 = 2.0B */
		switch (temp) {
		case TCC_CP_TYPE_20B:
			gCpVersion = 0x20B;
			break;
		case TCC_CP_TYPE_20C:
			gCpVersion = 0x20C;
			break;
		case TCC_CP_TYPE_030:
			gCpVersion = 0x030;
			break;
		default:
			dev_err(dev, "Invalid Auth CP Type Read [%u]\n", temp);
		}

		dev_dbg(dev, "Auth CP Type [0x%04X]\n", gCpVersion);
	} else {
		gCpVersion = 0x030;	// default 3.0 type
		dev_err(dev, "Force To Set 3.0 Type\n");
	}

	if (of_find_property(dev->of_node, "i2c-channel", 0)) {
		of_property_read_u32(dev->of_node, "i2c-channel",
				     &gCpI2cChannel);
		dev_dbg(dev, "cp i2c channel (%d)\n", gCpI2cChannel);
	} else {
		gCpI2cChannel = 0;	// default 0 channel
		dev_err(dev, "Force To Set i2c channel (%d)\n", gCpI2cChannel);
	}

	if (of_find_property(dev->of_node, "power-ctrl-able", 0)) {
		gCPPowerState = 1;
		dev_dbg(dev, "Power Controlable Enabled\n");
	} else {
		gCPPowerState = 0;
		dev_dbg(dev, "Power Controlable Disabled\n");
	}

	gCPPowerGpio = of_get_named_gpio(dev->of_node, "cp_power-gpio", 0);
	gCPResetGpio = of_get_named_gpio(dev->of_node, "cp_reset-gpio", 0);
	dev_dbg(dev, "Power GIPO[%u] Reset GPIO[%u]\n", gCPPowerGpio,
		gCPResetGpio);

}
#endif

/*****************************************************************************
 * TCC_CP_CTRL Probe/Remove
 ******************************************************************************/
static int tcc_cp_ctrl_probe(struct platform_device *pdev)
{
	int res = TCC_CP_NOERR;
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "%s pdev[%p] dev[%p]\n", __func__, pdev, dev);

	res = register_chrdev(CP_DEV_MAJOR_NUM, TCC_CP_DEV_NAME, &tcc_cp_fops);
	if (res >= TCC_CP_NOERR) {
		tcc_cp_class = class_create(THIS_MODULE, TCC_CP_DEV_NAME);
		if (tcc_cp_class != NULL) {
			tcc_cp_dev =
			    device_create(tcc_cp_class, NULL,
					  MKDEV(CP_DEV_MAJOR_NUM,
						CP_DEV_MINOR_NUM), NULL,
					  TCC_CP_DEV_NAME);
			if (tcc_cp_dev != NULL) {
#ifdef CONFIG_OF
				tcc_cp_read_device_tree(dev);
#endif
				tcc_cp_gpio_init(dev);
			} else {
				dev_err(dev, "%s device_create failed\n",
					__func__);
			}
		} else {
			dev_err(dev, "%s class_create failed\n", __func__);
		}
	} else {
		dev_err(dev, "%s register_chrdev failed res[%d]\n",
			__func__, res);
	}

	return 0;
}

static int tcc_cp_ctrl_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);
	device_destroy(tcc_cp_class, MKDEV(CP_DEV_MAJOR_NUM, CP_DEV_MINOR_NUM));
	class_destroy(tcc_cp_class);
	unregister_chrdev(CP_DEV_MAJOR_NUM, TCC_CP_DEV_NAME);

	return 0;
}

/*****************************************************************************
 * TCC_CP_CTRL Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_cp_ctrl_of_match[TCC_CP_MATCH_SIZE] = {
	{.compatible = "telechips, tcc-cp",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_cp_ctrl_of_match);
#endif

static struct platform_driver tcc_cp_ctrl = {
	.probe = tcc_cp_ctrl_probe,
	.remove = tcc_cp_ctrl_remove,
	.driver = {
		   .name = TCC_CP_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = tcc_cp_ctrl_of_match,
#endif
		   },
};

static int __init tcc_cp_ctrl_init(void)
{
	return platform_driver_register(&tcc_cp_ctrl);
}

static void __exit tcc_cp_ctrl_exit(void)
{
	platform_driver_unregister(&tcc_cp_ctrl);
}

module_init(tcc_cp_ctrl_init);
module_exit(tcc_cp_ctrl_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC cp-gpio driver");
MODULE_LICENSE("GPL");
