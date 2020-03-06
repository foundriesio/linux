/*
 * tcc_dxb_control.c
 *
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux DxB Control DRIVER
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
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include "tcc_dxb_control.h"


/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[TCC_DXB_CTRL]"
#define IRQF_DISABLED		0x00000020
static int dev_debug = 1;

module_param(dev_debug, int, 0644);
MODULE_PARM_DESC(dev_debug, "Turn on/off device debugging (default:off).");

#define dprintk(msg...)                                \
{                                                      \
	if (dev_debug)                                     \
		printk(KERN_INFO LOG_TAG "(D)" msg);           \
}

#define eprintk(msg...)                                \
{                                                      \
	printk(KERN_INFO LOG_TAG " (E) " msg);             \
}


/*****************************************************************************
 * Defines
 ******************************************************************************/
#define MAX_DEVICE_NO  (3)
#define PWRCTRL_OFF     0
#define PWRCTRL_ON      1
#define PWRCTRL_AUTO    2

#define DEFAULT_BOARD_TYPE (BOARD_DXB_TCC3171)

/*****************************************************************************
 * structures
 ******************************************************************************/
struct tcc_dxb_ctrl_t
{
	int device_major_number;
	int board_type;
	int bb_index;
	
	int power_status[MAX_DEVICE_NO + 1];

	int ant_ctrl_mode;

	int gpio_dxb_on;

	int gpio_dxb_0_pwdn;
	int gpio_dxb_0_rst;
	int gpio_dxb_0_irq;
	int gpio_dxb_0_sdo;

	int gpio_dxb_1_pwdn;
	int gpio_dxb_1_rst;
	int gpio_dxb_1_irq;
	int gpio_dxb_1_sdo;

	int gpio_ant_pwr;
	int gpio_check_ant_overload;

	int irq_check_ant_overlaod;

	int gpio_tuner_pwr;
	int gpio_tuner_rst;
};

/*****************************************************************************
 * Variables
 ******************************************************************************/
static struct class *tcc_dxb_ctrl_class;
struct tcc_dxb_ctrl_t *gDxbCtrl = NULL;


/*****************************************************************************
 * Gpio Functions
 ******************************************************************************/
static void GPIO_OUT_INIT(int pin)
{
	int rc;
	if (!gpio_is_valid(pin)) {
		eprintk("%s pin(0x%X) error\n", __FUNCTION__, pin);
		return;
	}
	rc = gpio_request(pin, NULL);
	if (rc) {
		eprintk("%s pin(0x%X) error(%d)\n", __FUNCTION__, pin, rc);
		return;
	}
	dprintk("%s pin[0x%X] value[0]\n", __func__, pin);
	gpio_direction_output(pin, 0);
}

static void GPIO_IN_INIT(int pin)
{
	int rc;
	if (!gpio_is_valid(pin)) {
		eprintk("%s pin(0x%X) error\n", __FUNCTION__, pin);
		return;
	}
	rc = gpio_request(pin, NULL);
	if (rc) {
		eprintk("%s pin(0x%X) error(%d)\n", __FUNCTION__, pin, rc);
		return;
	}
	dprintk("%s pin[0x%X]\n", __func__, pin);
	gpio_direction_input(pin);
}

static void GPIO_SET_VALUE(int pin, int value)
{
	if (!gpio_is_valid(pin)) {
		eprintk("%s pin(0x%X) error\n", __FUNCTION__, pin);
		return;
	}
	dprintk("%s pin[0x%X] value[%d]\n", __func__, pin, value);
	if (gpio_cansleep(pin))
		gpio_set_value_cansleep(pin, value);
	else
		gpio_set_value(pin, value);
}

static int GPIO_GET_VALUE(int pin)
{
	if (!gpio_is_valid(pin)) {
		eprintk("%s pin(0x%X) error\n", __FUNCTION__, pin);
		return -1;
	}
	dprintk("%s pin[0x%X]\n", __func__, pin);
	if (gpio_cansleep(pin))
		return gpio_get_value_cansleep(pin);
	else
		return gpio_get_value(pin);
}

static void GPIO_FREE(int pin)
{
	dprintk("%s pin[0x%X]\n", __func__, pin);
	if (gpio_is_valid(pin)) {
		gpio_free(pin);
	}
}


/*****************************************************************************
 * Baseband Functions
 ******************************************************************************/
#include "tcc3171.h"
#include "amfmtuner.h"

struct baseband {
	int type;
	int (*ioctl)(struct tcc_dxb_ctrl_t *ctrl, unsigned int cmd, unsigned long arg);
	int tsif_mode; // tsif interface (0: serial, 1: parallel)
	int ant_ctrl;  // antenna power control (0: disable, 1: enable)
} BB[] = {
	{ BOARD_DXB_TCC3171,   TCC3171_IOCTL, 0, 0 },
	{ BOARD_AMFM_TUNER,   amfmtuner_ioctl, 0, 0 },
	/* don't not modify or remove next line. when add new device, add to above this line */
	{ BOARD_DXB_UNDEFINED, NULL,         0, 0 } 
};

/* return : index of BB which has given type. if not found, return (-1) */
static int get_bb_index(int type)
{
	int found_index = -1;
	int index = 0;

	do
	{
		if( BB[index].type == type )
		{
			found_index = index;
			break;
		}
		index++;
	}while(BB[index].type!=BOARD_DXB_UNDEFINED);
	
	return found_index;
}

/*****************************************************************************
 * TCC_DXB_CTRL Functions
 ******************************************************************************/
static irqreturn_t isr_checking_ant_overload(int irq, void *dev_data)
{
	struct tcc_dxb_ctrl_t *ctrl = (struct tcc_dxb_ctrl_t *)dev_data;

	printk("%s : ant overload = %d\n", __func__, GPIO_GET_VALUE(ctrl->gpio_check_ant_overload));
	if(GPIO_GET_VALUE(ctrl->gpio_check_ant_overload) == 1)
	{
		GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 1);
	}
	else
	{
		GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 0);
	}
	return IRQ_HANDLED;
}

static int tcc_dxb_ctrl_ant_on(struct tcc_dxb_ctrl_t *ctrl)
{
	int err;

	if (ctrl->gpio_ant_pwr == 0)
		return 0;

	switch (ctrl->ant_ctrl_mode)
	{
	case PWRCTRL_OFF:
		break;

	case PWRCTRL_ON:
		GPIO_OUT_INIT(ctrl->gpio_ant_pwr);
		GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 1);//ANT_PWR_CTRL
		break;
	case PWRCTRL_AUTO:
		GPIO_OUT_INIT(ctrl->gpio_ant_pwr);
		GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 1);

		if (ctrl->gpio_check_ant_overload == 0)
			break;

		GPIO_IN_INIT(ctrl->gpio_check_ant_overload);
		msleep (5);
		if(GPIO_GET_VALUE(ctrl->gpio_check_ant_overload) == 0) {
			printk("ANT_OVERLOAD is low\n");
			GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 0);
			return 0;
		}
		ctrl->irq_check_ant_overlaod = gpio_to_irq(gDxbCtrl->gpio_check_ant_overload);
		#if 1
			err = request_irq(ctrl->irq_check_ant_overlaod, isr_checking_ant_overload, IRQ_TYPE_EDGE_FALLING | IRQF_DISABLED, "ANT_OVERLOAD_F", NULL);
		#else
			err = request_irq(ctrl->irq_check_ant_overlaod, isr_checking_ant_overload, IRQ_TYPE_EDGE_RISING  | IRQF_DISABLED, "ANT_OVERLOAD_R", NULL);
		#endif
		if (err) {
			printk("Unable to request ANT_OVERLOAD IRQ[%d].\n", err);
			ctrl->irq_check_ant_overlaod = 0;
		}
		break;
	}

	return 0;
}

static int tcc_dxb_ctrl_ant_off(struct tcc_dxb_ctrl_t *ctrl)
{
	if (ctrl->gpio_ant_pwr == 0)
		return 0;

	if (ctrl->irq_check_ant_overlaod)
		free_irq(ctrl->irq_check_ant_overlaod, NULL);

	ctrl->irq_check_ant_overlaod = 0;

	GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 0);

	return 0;
}

static long tcc_dxb_ctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	dprintk("%s cmd[0x%X]\n", __func__, cmd);
	switch (cmd)
	{
		case IOCTL_DXB_CTRL_SET_CTRLMODE:
			{
				unsigned int uiAntCtrlMode;
				if (copy_from_user((void *)&uiAntCtrlMode, (const void *)arg, sizeof(unsigned int)) != 0)
					break;
				gDxbCtrl->ant_ctrl_mode = uiAntCtrlMode;
			}
			break;

		case IOCTL_DXB_CTRL_OFF:
			{
				if (gDxbCtrl->bb_index != (-1))
				{
					if (BB[gDxbCtrl->bb_index].ant_ctrl == 1)
					{
						tcc_dxb_ctrl_ant_off(gDxbCtrl);
					}
					if (BB[gDxbCtrl->bb_index].ioctl != NULL)
					{
						return BB[gDxbCtrl->bb_index].ioctl(gDxbCtrl, cmd, arg);
					}
				}
				else
				{
					return -1;
				}
			}
			break;

		case IOCTL_DXB_CTRL_ON:
			{
				if (gDxbCtrl->bb_index != (-1))
				{
					if (BB[gDxbCtrl->bb_index].ant_ctrl == 1)
					{
						tcc_dxb_ctrl_ant_on(gDxbCtrl);
					}
					if (BB[gDxbCtrl->bb_index].ioctl != NULL)
					{
						return BB[gDxbCtrl->bb_index].ioctl(gDxbCtrl, cmd, arg);
					}
				}
				else
				{
					return -1;
				}
			}
			break;

		case IOCTL_DXB_CTRL_SET_BOARD:
			{
				unsigned int new_board_type;
				if (copy_from_user((void *)&new_board_type, (const void *)arg, sizeof(unsigned int)) != 0)
				{
					break;
				}

				gDxbCtrl->board_type = new_board_type;
				gDxbCtrl->bb_index = get_bb_index(new_board_type);
				if( gDxbCtrl->bb_index == (-1) )
				{
					/* undefined board type */
					eprintk("%s cmd[0x%X] undefined board type %d\n", __func__, cmd, new_board_type);
					return -1;
				}

				dprintk("new_board_type=%d, bb_index=%d\n", gDxbCtrl->board_type, gDxbCtrl->bb_index);
				if (BB[gDxbCtrl->bb_index].ioctl != NULL)
				{
					return BB[gDxbCtrl->bb_index].ioctl(gDxbCtrl, cmd, arg);
				}
			}
			break;
		default:
			{
				if (gDxbCtrl->bb_index != (-1))
				{
					if (BB[gDxbCtrl->bb_index].ioctl != NULL)
					{
						return BB[gDxbCtrl->bb_index].ioctl(gDxbCtrl, cmd, arg);
					}
				}
				else
				{
					return -1;
				}
			}
			break;
	}
	return 0;
}

static int tcc_dxb_ctrl_release(struct inode *inode, struct file *filp)
{
	GPIO_FREE(gDxbCtrl->gpio_dxb_on);

	GPIO_FREE(gDxbCtrl->gpio_dxb_0_pwdn);
	GPIO_FREE(gDxbCtrl->gpio_dxb_0_rst);
	GPIO_FREE(gDxbCtrl->gpio_dxb_0_irq);
	GPIO_FREE(gDxbCtrl->gpio_dxb_0_sdo);

	GPIO_FREE(gDxbCtrl->gpio_dxb_1_pwdn);
	GPIO_FREE(gDxbCtrl->gpio_dxb_1_rst);
	GPIO_FREE(gDxbCtrl->gpio_dxb_1_irq);
	GPIO_FREE(gDxbCtrl->gpio_dxb_1_sdo);

	GPIO_FREE(gDxbCtrl->gpio_ant_pwr);
	GPIO_FREE(gDxbCtrl->gpio_check_ant_overload);

	GPIO_FREE(gDxbCtrl->gpio_tuner_rst);
	GPIO_FREE(gDxbCtrl->gpio_tuner_pwr);

	dprintk("%s::%d\n", __FUNCTION__, __LINE__);

	return 0;
}

static int tcc_dxb_ctrl_open(struct inode *inode, struct file *filp)
{
	dprintk("%s::%d\n", __FUNCTION__, __LINE__);

	return 0;
}

struct file_operations tcc_dxb_ctrl_fops =
{
	.owner          = THIS_MODULE,
	.open           = tcc_dxb_ctrl_open,
	.release        = tcc_dxb_ctrl_release,
	.unlocked_ioctl = tcc_dxb_ctrl_ioctl,
};


/*****************************************************************************
 * TCC_DXB_CTRL Probe/Remove
 ******************************************************************************/
static ssize_t tcc_dxb_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t tcc_dxb_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int overload = -1;
	if (gDxbCtrl->gpio_check_ant_overload != 0)
	{
		overload = GPIO_GET_VALUE(gDxbCtrl->gpio_check_ant_overload);
	}
	return sprintf(buf, "ANT_OVERLOAD=%d\n", overload);
}
static DEVICE_ATTR(state, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, tcc_dxb_ctrl_show, tcc_dxb_ctrl_store);


/*****************************************************************************
 * TCC_DXB_CTRL Probe/Remove
 ******************************************************************************/
static int tcc_dxb_ctrl_probe(struct platform_device *pdev)
{
	int res, major_num;
	struct device *dev = &pdev->dev;

	res = register_chrdev(0, DXB_CTRL_DEV_NAME, &tcc_dxb_ctrl_fops);
	if (res < 0)
		return res;

	major_num = res;
	tcc_dxb_ctrl_class = class_create(THIS_MODULE, DXB_CTRL_DEV_NAME);
	if(NULL == device_create(tcc_dxb_ctrl_class, NULL, MKDEV(major_num, DXB_CTRL_DEV_MINOR), NULL, DXB_CTRL_DEV_NAME))
	{
		eprintk("%s device_create failed\n", __FUNCTION__);
	}

	gDxbCtrl = kzalloc(sizeof(struct tcc_dxb_ctrl_t), GFP_KERNEL);
	if (gDxbCtrl == NULL)
		return -ENOMEM;

	gDxbCtrl->device_major_number = major_num;

	if (device_create_file(dev, &dev_attr_state))
	{
		eprintk("Failed to create file.\n");
	}

#ifdef CONFIG_OF
	gDxbCtrl->gpio_dxb_on     = of_get_named_gpio(dev->of_node, "pw-gpios",   0);

	gDxbCtrl->gpio_dxb_0_pwdn = of_get_named_gpio(dev->of_node, "dxb0-gpios", 0);
	gDxbCtrl->gpio_dxb_0_rst  = of_get_named_gpio(dev->of_node, "dxb0-gpios", 1);
	gDxbCtrl->gpio_dxb_0_irq  = of_get_named_gpio(dev->of_node, "dxb0-gpios", 2);
	gDxbCtrl->gpio_dxb_0_sdo  = of_get_named_gpio(dev->of_node, "dxb0-gpios", 3);

	gDxbCtrl->gpio_dxb_1_pwdn = of_get_named_gpio(dev->of_node, "dxb1-gpios", 0);
	gDxbCtrl->gpio_dxb_1_rst  = of_get_named_gpio(dev->of_node, "dxb1-gpios", 1);
	gDxbCtrl->gpio_dxb_1_irq  = of_get_named_gpio(dev->of_node, "dxb1-gpios", 2);
	gDxbCtrl->gpio_dxb_1_sdo  = of_get_named_gpio(dev->of_node, "dxb1-gpios", 3);

	gDxbCtrl->gpio_ant_pwr            = of_get_named_gpio(dev->of_node, "ant-gpios",   0);
	gDxbCtrl->gpio_check_ant_overload = of_get_named_gpio(dev->of_node, "ant-gpios",   1);

	gDxbCtrl->gpio_tuner_rst  = of_get_named_gpio(dev->of_node, "tuner-gpios", 0);
	gDxbCtrl->gpio_tuner_pwr  = of_get_named_gpio(dev->of_node, "tuner-gpios", 1);
#endif
	dprintk("%s [0x%X] [0x%X][0x%X][0x%X][0x%X] [0x%X][0x%X][0x%X][0x%X] [0x%X][0x%X]  [0x%X][0x%X]\n", __func__,
		gDxbCtrl->gpio_dxb_on,
		gDxbCtrl->gpio_dxb_0_pwdn, gDxbCtrl->gpio_dxb_0_rst, gDxbCtrl->gpio_dxb_0_irq, gDxbCtrl->gpio_dxb_0_sdo,
		gDxbCtrl->gpio_dxb_1_pwdn, gDxbCtrl->gpio_dxb_1_rst, gDxbCtrl->gpio_dxb_1_irq, gDxbCtrl->gpio_dxb_1_sdo,
		gDxbCtrl->gpio_ant_pwr, gDxbCtrl->gpio_check_ant_overload,
		gDxbCtrl->gpio_tuner_rst, gDxbCtrl->gpio_tuner_pwr
		);

	gDxbCtrl->board_type = DEFAULT_BOARD_TYPE;
	gDxbCtrl->bb_index = get_bb_index(gDxbCtrl->board_type);
	if( gDxbCtrl->bb_index == -1 )
	{
		eprintk("[%s] default board type is undefined\n", __func__);
	}
	gDxbCtrl->ant_ctrl_mode = PWRCTRL_AUTO;

	return 0;
}

static int tcc_dxb_ctrl_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_state);
	device_destroy(tcc_dxb_ctrl_class, MKDEV(gDxbCtrl->device_major_number, DXB_CTRL_DEV_MINOR));
	class_destroy(tcc_dxb_ctrl_class);
	unregister_chrdev(gDxbCtrl->device_major_number, DXB_CTRL_DEV_NAME);

	if (gDxbCtrl)
		kfree(gDxbCtrl);
	gDxbCtrl = NULL;

	return 0;
}


/*****************************************************************************
 * TCC_DXB_CTRL Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_dxb_ctrl_of_match[] = {
	{.compatible = "telechips,tcc_dxb_ctrl", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_dxb_ctrl_of_match);
#endif

static struct platform_driver tcc_dxb_ctrl = {
	.probe	= tcc_dxb_ctrl_probe,
	.remove	= tcc_dxb_ctrl_remove,
	.driver	= {
		.name	= DXB_CTRL_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tcc_dxb_ctrl_of_match,
#endif
	},
};

static int __init tcc_dxb_ctrl_init(void)
{
	return platform_driver_register(&tcc_dxb_ctrl);
}

static void __exit tcc_dxb_ctrl_exit(void)
{
	platform_driver_unregister(&tcc_dxb_ctrl);
}

module_init(tcc_dxb_ctrl_init);
module_exit(tcc_dxb_ctrl_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC Broadcasting Control(Power, Reset..)");
MODULE_LICENSE("GPL");
