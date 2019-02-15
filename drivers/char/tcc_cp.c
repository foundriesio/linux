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

#include <asm/io.h>
#include <linux/uaccess.h>
#include <mach/tcccp_ioctl.h>

#define CP_DEV_NAME			"tcc-cp"

#define CP_GPIO_HI			1
#define CP_GPIO_LOW			0

#define CP_STATE_ENABLE	        		1
#define CP_STATE_DISABLE                        0

#define CP_DEBUG    0

#if CP_DEBUG
#define cp_dbg(fmt, arg...)     printk(fmt, ##arg)
#else
#define cp_dbg(arg...)
#endif

static struct class *tcc_cp_class;

static int gCPMajorNum = 0;
static int gCPPowerState = 0;
static int gCPPowerGpio = 0;
static int gCPResetGpio = 0;

unsigned int gCpVersion = 0;
unsigned int gCpI2cChannel = 0;
static unsigned int gCpState = 0;


int tcc_cp_gpio_init(void)
{
	int err;
    cp_dbg("%s  \n", __FUNCTION__);

    if(gCPPowerState == 1)
    {
		if(!gpio_is_valid(gCPPowerGpio)) {
			cp_dbg("can't find dev of node: cp reset gpio\n");
			return -ENODEV;
		}

		err = gpio_request(gCPPowerGpio, "cp_power_gpio");
		if(err) {
			cp_dbg("can't request cp reset gpio\n");
			return err;
		}
	    gpio_direction_output(gCPPowerGpio, 0);
    }

	if(!gpio_is_valid(gCPResetGpio)) {
		cp_dbg("can't find dev of node: cp reset gpio\n");
		return -ENODEV;
	}

	err = gpio_request(gCPResetGpio, "cp_reset_gpio");
	if(err) {
		cp_dbg("can't request cp reset gpio\n");
		return err;
	}
    gpio_direction_output(gCPResetGpio, 0);
	return 0;
}

void tcc_cp_pwr_control(int value)
{
    if(gCPPowerState == 1)
    {
        cp_dbg("%s value : %d\n", __FUNCTION__, value);
		if (gpio_cansleep(gCPPowerGpio))
			gpio_set_value_cansleep(gCPPowerGpio, value);
		else
			gpio_set_value(gCPPowerGpio, value);
    }
}

void tcc_cp_reset_control(int value)
{
	if((gCPPowerState == 1) || (gCpVersion == 0x20B))
	{
		cp_dbg("%s value : %d\n", __FUNCTION__, value);
		if (gpio_cansleep(gCPResetGpio))
			gpio_set_value_cansleep(gCPResetGpio, value);
		else
			gpio_set_value(gCPResetGpio, value);

		if(gCpVersion == 0x20B)
			mdelay(30);
	}
}


long tcc_cp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    cp_dbg("%s  (0x%x)  \n", __FUNCTION__, cmd);

    switch (cmd)
    {
        case IOCTL_CP_CTRL_INIT:
        {
            tcc_cp_gpio_init();
            cp_dbg("%s:  IOCTL_CP_SET_GPIO_INIT (0x%x) \n", __FUNCTION__, cmd);
        }
            break;
        case IOCTL_CP_CTRL_PWR:
        {
            if(arg != 0)
                tcc_cp_pwr_control(CP_GPIO_HI);
            else
                tcc_cp_pwr_control(CP_GPIO_LOW);
            cp_dbg("%s:  IOCTL_CP_CTRL_PWR arg(%lu) \n", __FUNCTION__, arg);
        }
            break;
        case IOCTL_CP_CTRL_RESET:
        {
            if(arg != 0)
                tcc_cp_reset_control(CP_GPIO_HI);
            else
                tcc_cp_reset_control(CP_GPIO_LOW);

            cp_dbg("%s:  IOCTL_CP_CTRL_RESET arg(%lu) \n", __FUNCTION__, arg);
        }
            break;
        case IOCTL_CP_CTRL_ALL:
        {
            if(arg != 0){
                tcc_cp_pwr_control(CP_GPIO_HI);
                tcc_cp_reset_control(CP_GPIO_HI);
            } else{
                tcc_cp_pwr_control(CP_GPIO_LOW);
                tcc_cp_reset_control(CP_GPIO_LOW);
            }

            cp_dbg("%s:  IOCTL_CP_CTRL_ALL arg(%lu) \n", __FUNCTION__, arg);
        }
            break;
        case IOCTL_CP_GET_VERSION:
            if(copy_to_user((unsigned int*) arg, &gCpVersion, sizeof(unsigned int))!=0)
            {
                printk(" %s copy version error~!\n",__func__);
			}
            break;
        case IOCTL_CP_GET_CHANNEL:
            if(copy_to_user((unsigned int*) arg, &gCpI2cChannel, sizeof(unsigned int))!=0)
            {
                printk(" %s copy channel error~!\n",__func__);
            }
            break;
        case IOCTL_CP_SET_STATE:
            if(arg != 0){
                gCpState = CP_STATE_ENABLE;
            }else{
                gCpState = CP_STATE_DISABLE;
            }
            break;
        case IOCTL_CP_GET_STATE:
            if(copy_to_user((unsigned int*) arg, &gCpState, sizeof(unsigned int))!=0)
            {
                printk(" %s copy state error~!\n",__func__);
            }
            break;
        default:
            cp_dbg("cp: unrecognized ioctl (0x%x)\n", cmd);
            return -EINVAL;
            break;
    }
    return 0;
}

static int tcc_cp_release(struct inode *inode, struct file *filp)
{
    cp_dbg("%s \n", __FUNCTION__);

    return 0;
}

static int tcc_cp_open(struct inode *inode, struct file *filp)
{
    cp_dbg("%s : \n", __FUNCTION__);

    return 0;
}

struct file_operations tcc_cp_fops =
{
    .owner    = THIS_MODULE,
    .open     = tcc_cp_open,
    .release  = tcc_cp_release,
    .unlocked_ioctl = tcc_cp_ioctl,
};

/*****************************************************************************
 * TCC_CP_CTRL Probe/Remove
 ******************************************************************************/
static int tcc_cp_ctrl_probe(struct platform_device *pdev)
{
	int res;
    unsigned int temp;
	struct device *dev = &pdev->dev;

    cp_dbg("%s \n", __FUNCTION__);

	res = register_chrdev(CP_DEV_MAJOR_NUM, CP_DEV_NAME, &tcc_cp_fops);
    cp_dbg("%s: register_chrdev ret : %d\n", __func__, res);
	if (res < 0)
		return res;

	gCPMajorNum = CP_DEV_MAJOR_NUM;
	tcc_cp_class = class_create(THIS_MODULE, CP_DEV_NAME);
	if(NULL == device_create(tcc_cp_class, NULL, MKDEV(gCPMajorNum, CP_DEV_MINOR_NUM), NULL, CP_DEV_NAME))
		cp_dbg("%s device_create failed\n", __FUNCTION__);

#ifdef CONFIG_OF
    if (of_find_property(pdev->dev.of_node, "power-ctrl-able", 0)) {
        gCPPowerState = 1;
    }
    else {
        gCPPowerState = 0;
    }
	gCPPowerGpio = of_get_named_gpio(dev->of_node, "cp_power-gpio", 0);
    gCPResetGpio = of_get_named_gpio(dev->of_node, "cp_reset-gpio", 0);

	if (of_find_property(dev->of_node, "cp-type", 0)) {
		of_property_read_u32(dev->of_node, "cp-type", &temp);

		/*
        * Auth CP type 2 = 3.0
        * Auth CP type 1 = 2.0C
		* Auth CP type 0 = 2.0B
		*/
        if (temp == 2)
        {
			cp_dbg("3.0 type\n");
			gCpVersion = 0x30;
        }
		else if (temp == 1) 
        {
			cp_dbg("2.0C type\n");
			gCpVersion = 0x20C;
		}
		else if (temp == 0) 
        {
			cp_dbg("2.0B type\n");
			gCpVersion = 0x20B;
		}
		else
			cp_dbg("bad Auth CP type!\n");
	} else {
		gCpVersion = 0x30;	// default 3.0 type
		cp_dbg("default 3.0 type\n");
	}

	if (of_find_property(dev->of_node, "i2c-channel", 0)) {
		of_property_read_u32(dev->of_node, "i2c-channel", &gCpI2cChannel);
		cp_dbg("cp i2c channel (%d)\n", gCpI2cChannel);
	} else {
	    gCpI2cChannel = 0;	// default 0 channel
	    cp_dbg("default cp i2c channel (%d)\n", gCpI2cChannel);
	}
#endif

    tcc_cp_gpio_init();

	return 0;
}

static int tcc_cp_ctrl_remove(struct platform_device *pdev)
{
    cp_dbg("%s\n", __FUNCTION__);

	device_destroy(tcc_cp_class, MKDEV(gCPMajorNum, CP_DEV_MINOR_NUM));
	class_destroy(tcc_cp_class);
	unregister_chrdev(gCPMajorNum, CP_DEV_NAME);

	return 0;
}

/*****************************************************************************
 * TCC_CP_CTRL Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_cp_ctrl_of_match[] = {
	{.compatible = "telechips, tcc-cp", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_cp_ctrl_of_match);
#endif

static struct platform_driver tcc_cp_ctrl = {
	.probe	= tcc_cp_ctrl_probe,
	.remove	= tcc_cp_ctrl_remove,
	.driver	= {
		.name	= CP_DEV_NAME,
		.owner	= THIS_MODULE,
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


