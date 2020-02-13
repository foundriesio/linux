/*
 * tcc_isdbt_control.c
 *
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2008
 * Description: Telechips Linux ISDBT Control DRIVER
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
#include "../hwdmx-core/hwdmx_cmd.h"
#include "../hwdmx-core/hwdmx_cipher.h"
#include "tcc_isdbt_control.h"


/*****************************************************************************
 * Log Message
 ******************************************************************************/
#define LOG_TAG    "[TCC_ISDBT_CTRL]"
#define IRQF_DISABLED		0x00000020
static int dev_debug = 0;

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


/*****************************************************************************
 * structures
 ******************************************************************************/
struct tcc_isdbt_ctrl_t
{
	int board_type;
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
};

/*****************************************************************************
 * Variables
 ******************************************************************************/
static struct class *tcc_isdbt_ctrl_class;
struct tcc_isdbt_ctrl_t *gIsdbtCtrl = NULL;
struct tcc_tsif_handle *h = NULL;

/*****************************************************************************
 * Gpio Functions
 ******************************************************************************/
static void GPIO_OUT_INIT(int pin)
{
	int rc;
	if (!gpio_is_valid(pin)) {
		eprintk("[ERROR][HWDMX] %s pin(0x%X) error\n", __FUNCTION__, pin);
		return;
	}
	rc = gpio_request(pin, NULL);
	if (rc) {
		eprintk("[ERROR][HWDMX] %s pin(0x%X) error(%d)\n", __FUNCTION__, pin, rc);
		return;
	}
	dprintk("[DEBUG][HWDMX] %s pin[0x%X] value[0]\n", __func__, pin);
	gpio_direction_output(pin, 0);
}

static void GPIO_IN_INIT(int pin)
{
	int rc;
	if (!gpio_is_valid(pin)) {
		eprintk("[ERROR][HWDMX] %s pin(0x%X) error\n", __FUNCTION__, pin);
		return;
	}
	rc = gpio_request(pin, NULL);
	if (rc) {
		eprintk("[ERROR][HWDMX] %s pin(0x%X) error(%d)\n", __FUNCTION__, pin, rc);
		return;
	}
	dprintk("[DEBUG][HWDMX] %s pin[0x%X]\n", __func__, pin);
	gpio_direction_input(pin);
}

static void GPIO_SET_VALUE(int pin, int value)
{
	if (!gpio_is_valid(pin)) {
		eprintk("[ERROR][HWDMX] %s pin(0x%X) error\n", __FUNCTION__, pin);
		return;
	}
	dprintk("[DEBUG][HWDMX] %s pin[0x%X] value[%d]\n", __func__, pin, value);
	if (gpio_cansleep(pin))
		gpio_set_value_cansleep(pin, value);
	else
		gpio_set_value(pin, value);
}

static int GPIO_GET_VALUE(int pin)
{
	if (!gpio_is_valid(pin)) {
		eprintk("[ERROR][HWDMX] %s pin(0x%X) error\n", __FUNCTION__, pin);
		return -1;
	}
	dprintk("[DEBUG][HWDMX] %s pin[0x%X]\n", __func__, pin);
	if (gpio_cansleep(pin))
		return gpio_get_value_cansleep(pin);
	else
		return gpio_get_value(pin);
}

static void GPIO_FREE(int pin)
{
	dprintk("[DEBUG][HWDMX] %s pin[0x%X]\n", __func__, pin);
	if (gpio_is_valid(pin)) {
		gpio_free(pin);
	}
}


/*****************************************************************************
 * Baseband Functions
 ******************************************************************************/
#include "tcc353x.h"

struct baseband {
	int type;
	int (*ioctl)(struct tcc_isdbt_ctrl_t *ctrl, unsigned int cmd, unsigned long arg);
	int tsif_mode; // tsif interface (0: serial, 1: parallel)
	int ant_ctrl;  // antenna power control (0: disable, 1: enable)
} ISDBT_BB[] = {
	{ BOARD_ISDBT_TCC353X,      TCC353X_IOCTL,      0,  0 },
};


/*****************************************************************************
 * TCC_ISDBT_CTRL Functions
 ******************************************************************************/
static irqreturn_t isr_checking_ant_overload(int irq, void *dev_data)
{
	struct tcc_isdbt_ctrl_t *ctrl = (struct tcc_isdbt_ctrl_t *)dev_data;

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

static int tcc_isdbt_ctrl_ant_on(struct tcc_isdbt_ctrl_t *ctrl)
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
		ctrl->irq_check_ant_overlaod = gpio_to_irq(gIsdbtCtrl->gpio_check_ant_overload);
		err = request_irq(ctrl->irq_check_ant_overlaod, isr_checking_ant_overload, IRQ_TYPE_EDGE_FALLING | IRQF_DISABLED, "ANT_OVERLOAD_F", NULL);
		if (err) {
			printk("Unable to request ANT_OVERLOAD IRQ[%d].\n", err);
			ctrl->irq_check_ant_overlaod = 0;
		}
		break;
	}

	return 0;
}

static int tcc_isdbt_ctrl_ant_off(struct tcc_isdbt_ctrl_t *ctrl)
{
	if (ctrl->gpio_ant_pwr == 0)
		return 0;

	if (ctrl->irq_check_ant_overlaod)
		free_irq(ctrl->irq_check_ant_overlaod, NULL);

	ctrl->irq_check_ant_overlaod = 0;

	GPIO_SET_VALUE(ctrl->gpio_ant_pwr, 0);

	return 0;
}

static long tcc_isdbt_ctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	dprintk("[DEBUG][HWDMX] %s cmd[0x%X]\n", __func__, cmd);
	switch (cmd)
	{
		// for HWDEMUX Cipher
		case IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_ALGORITHM:
			{
				stHWDEMUXCIPHER_ALGORITHM stAlgorithm;
				if (copy_from_user((void *)&stAlgorithm, (const void *)arg, sizeof(stHWDEMUXCIPHER_ALGORITHM)) == 0)
				{
					if(h)
					{
						h->dmx_id = stAlgorithm.uDemuxId;
						hwdmx_set_algo_cmd(h, stAlgorithm.uAlgorithm, stAlgorithm.uOperationMode, stAlgorithm.uResidual, stAlgorithm.uSmsg, 0, NULL);
					}
				}
			}
			break;

		case IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_KEY:
			{
				stHWDEMUXCIPHER_KEY stKeyInfo;
				if (copy_from_user((void *)&stKeyInfo, (const void *)arg, sizeof(stHWDEMUXCIPHER_KEY)) == 0)
				{
					if(h)
					{
						h->dmx_id = stKeyInfo.uDemuxId;
						hwdmx_set_key_cmd(h, stKeyInfo.uKeyType, stKeyInfo.uKeyMode, stKeyInfo.uLength, stKeyInfo.pucData);
					}
				}
			}
			break;

		case IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_VECTOR:
			{
				stHWDEMUXCIPHER_VECTOR stVectorInfo;
				if (copy_from_user((void *)&stVectorInfo, (const void *)arg, sizeof(stHWDEMUXCIPHER_VECTOR)) == 0)
				{
					if(h)
					{
						h->dmx_id = stVectorInfo.uDemuxId;
						hwdmx_set_iv_cmd(h, stVectorInfo.uOption, stVectorInfo.uLength, stVectorInfo.pucData);
					}
				}
			}
			break;

		case IOCTL_DXB_CTRL_HWDEMUX_CIPHER_SET_DATA:
			{
				stHWDEMUXCIPHER_SENDDATA stSendData;
				if (copy_from_user((void *)&stSendData, (const void *)arg, sizeof(stHWDEMUXCIPHER_SENDDATA)) == 0)
				{
					hwdmx_set_data_cmd(stSendData.uDemuxId, stSendData.pucSrcAddr, stSendData.pucDstAddr, stSendData.uLength);
				}
			}
			break;

		case IOCTL_DXB_CTRL_HWDEMUX_CIPHER_EXECUTE:
			{
				stHWDEMUXCIPHER_EXECUTE stExecute;
				if (copy_from_user((void *)&stExecute, (const void *)arg, sizeof(stHWDEMUXCIPHER_EXECUTE)) == 0)
				{
					hwdmx_run_cipher_cmd(stExecute.uDemuxId, stExecute.uExecuteOption, stExecute.uCWsel, stExecute.uKLIdx, stExecute.uKeyMode);
				}
			}
			break;

		case IOCTL_DXB_CTRL_SET_CTRLMODE:
			{
				unsigned int uiAntCtrlMode;
				if (copy_from_user((void *)&uiAntCtrlMode, (const void *)arg, sizeof(unsigned int)) != 0)
					break;
				gIsdbtCtrl->ant_ctrl_mode = uiAntCtrlMode;
			}
			break;

		case IOCTL_DXB_CTRL_OFF:
			{
				if (gIsdbtCtrl->board_type < BOARD_MAX)
				{
					if (ISDBT_BB[gIsdbtCtrl->board_type].ant_ctrl == 1)
					{
						tcc_isdbt_ctrl_ant_off(gIsdbtCtrl);
					}
					if (ISDBT_BB[gIsdbtCtrl->board_type].ioctl != NULL)
					{
						return ISDBT_BB[gIsdbtCtrl->board_type].ioctl(gIsdbtCtrl, cmd, arg);
					}
				}
			}
			break;

		case IOCTL_DXB_CTRL_ON:
			{
				if (gIsdbtCtrl->board_type < BOARD_MAX)
				{
					if (ISDBT_BB[gIsdbtCtrl->board_type].ant_ctrl == 1)
					{
						tcc_isdbt_ctrl_ant_on(gIsdbtCtrl);
					}
					if (ISDBT_BB[gIsdbtCtrl->board_type].ioctl != NULL)
					{
						return ISDBT_BB[gIsdbtCtrl->board_type].ioctl(gIsdbtCtrl, cmd, arg);
					}
				}
			}
			break;

		case IOCTL_DXB_CTRL_SET_BOARD:
			{
				unsigned int uiboardtype;
				if (copy_from_user((void *)&uiboardtype, (const void *)arg, sizeof(unsigned int)) != 0)
					break;

				gIsdbtCtrl->board_type = uiboardtype;
				if (gIsdbtCtrl->board_type < BOARD_MAX)
				{
					hwdmx_set_interface_cmd(-1, ISDBT_BB[gIsdbtCtrl->board_type].tsif_mode);
					if (ISDBT_BB[gIsdbtCtrl->board_type].ioctl != NULL)
					{
						return ISDBT_BB[gIsdbtCtrl->board_type].ioctl(gIsdbtCtrl, cmd, arg);
					}
				}
			}
			break;

		default:
			{
				if (gIsdbtCtrl->board_type < BOARD_MAX)
				{
					if (ISDBT_BB[gIsdbtCtrl->board_type].ioctl != NULL)
					{
						return ISDBT_BB[gIsdbtCtrl->board_type].ioctl(gIsdbtCtrl, cmd, arg);
					}
				}
			}
			break;
	}
	return 0;
}

static int tcc_isdbt_ctrl_release(struct inode *inode, struct file *filp)
{
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_on);

	GPIO_FREE(gIsdbtCtrl->gpio_dxb_0_pwdn);
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_0_rst);
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_0_irq);
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_0_sdo);

	GPIO_FREE(gIsdbtCtrl->gpio_dxb_1_pwdn);
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_1_rst);
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_1_irq);
	GPIO_FREE(gIsdbtCtrl->gpio_dxb_1_sdo);

	GPIO_FREE(gIsdbtCtrl->gpio_ant_pwr);
	GPIO_FREE(gIsdbtCtrl->gpio_check_ant_overload);

	if(h)
	{
		kfree(h);
		h = NULL;
	}
	dprintk("[DEBUG][HWDMX] %s::%d\n", __FUNCTION__, __LINE__);

	return 0;
}

static int tcc_isdbt_ctrl_open(struct inode *inode, struct file *filp)
{
	dprintk("[DEBUG][HWDMX] %s::%d\n", __FUNCTION__, __LINE__);

	if(h == NULL){
		h = kzalloc(sizeof(struct tcc_tsif_handle), GFP_KERNEL);
		if (h == NULL)
			return -ENOMEM;
	}
	return 0;
}

struct file_operations tcc_isdbt_ctrl_fops =
{
	.owner          = THIS_MODULE,
	.open           = tcc_isdbt_ctrl_open,
	.release        = tcc_isdbt_ctrl_release,
	.unlocked_ioctl = tcc_isdbt_ctrl_ioctl,
};


/*****************************************************************************
 * TCC_ISDBT_CTRL Probe/Remove
 ******************************************************************************/
static ssize_t tcc_isdbt_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t tcc_isdbt_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int overload = -1;
	if (gIsdbtCtrl->gpio_check_ant_overload != 0)
	{
		overload = GPIO_GET_VALUE(gIsdbtCtrl->gpio_check_ant_overload);
	}
	return sprintf(buf, "ANT_OVERLOAD=%d\n", overload);
}
static DEVICE_ATTR(state, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, tcc_isdbt_ctrl_show, tcc_isdbt_ctrl_store);


/*****************************************************************************
 * TCC_ISDBT_CTRL Probe/Remove
 ******************************************************************************/
static int tcc_isdbt_ctrl_probe(struct platform_device *pdev)
{
	int res, major_num;
	struct device *dev = &pdev->dev;

	res = register_chrdev(0, ISDBT_CTRL_DEV_NAME, &tcc_isdbt_ctrl_fops);
	if (res < 0)
		return res;

	major_num = res;
	tcc_isdbt_ctrl_class = class_create(THIS_MODULE, ISDBT_CTRL_DEV_NAME);
	if(NULL == device_create(tcc_isdbt_ctrl_class, NULL, MKDEV(major_num, ISDBT_CTRL_DEV_MINOR), NULL, ISDBT_CTRL_DEV_NAME))
		eprintk("[ERROR][HWDMX] %s device_create failed\n", __FUNCTION__);

	gIsdbtCtrl = kzalloc(sizeof(struct tcc_isdbt_ctrl_t), GFP_KERNEL);
	if (gIsdbtCtrl == NULL)
		return -ENOMEM;

	if (device_create_file(dev, &dev_attr_state))
		eprintk("[ERROR][HWDMX] Failed to create file.\n");

#ifdef CONFIG_OF
	gIsdbtCtrl->gpio_dxb_on     = of_get_named_gpio(dev->of_node, "pw-gpios",   0);

	gIsdbtCtrl->gpio_dxb_0_pwdn = of_get_named_gpio(dev->of_node, "dxb0-gpios", 0);
	gIsdbtCtrl->gpio_dxb_0_rst  = of_get_named_gpio(dev->of_node, "dxb0-gpios", 1);
	gIsdbtCtrl->gpio_dxb_0_irq  = of_get_named_gpio(dev->of_node, "dxb0-gpios", 2);
	gIsdbtCtrl->gpio_dxb_0_sdo  = of_get_named_gpio(dev->of_node, "dxb0-gpios", 3);

	gIsdbtCtrl->gpio_dxb_1_pwdn = of_get_named_gpio(dev->of_node, "dxb1-gpios", 0);
	gIsdbtCtrl->gpio_dxb_1_rst  = of_get_named_gpio(dev->of_node, "dxb1-gpios", 1);
	gIsdbtCtrl->gpio_dxb_1_irq  = of_get_named_gpio(dev->of_node, "dxb1-gpios", 2);
	gIsdbtCtrl->gpio_dxb_1_sdo  = of_get_named_gpio(dev->of_node, "dxb1-gpios", 3);

	gIsdbtCtrl->gpio_ant_pwr            = of_get_named_gpio(dev->of_node, "ant-gpios",   0);
	gIsdbtCtrl->gpio_check_ant_overload = of_get_named_gpio(dev->of_node, "ant-gpios",   1);
#endif
	dprintk(
		"[DEBUG][HWDMX] %s [0x%X][0x%X][0x%X][0x%X][0x%X][0x%X][0x%X][0x%X][0x%X][0x%X][0x%X]\n",
		__func__, gIsdbtCtrl->gpio_dxb_on, gIsdbtCtrl->gpio_dxb_0_pwdn, gIsdbtCtrl->gpio_dxb_0_rst,
		gIsdbtCtrl->gpio_dxb_0_irq, gIsdbtCtrl->gpio_dxb_0_sdo, gIsdbtCtrl->gpio_dxb_1_pwdn,
		gIsdbtCtrl->gpio_dxb_1_rst, gIsdbtCtrl->gpio_dxb_1_irq, gIsdbtCtrl->gpio_dxb_1_sdo,
		gIsdbtCtrl->gpio_ant_pwr, gIsdbtCtrl->gpio_check_ant_overload);

	gIsdbtCtrl->board_type = BOARD_ISDBT_TCC353X;
	gIsdbtCtrl->ant_ctrl_mode = PWRCTRL_AUTO;

	printk("%s\n", __FUNCTION__);

	return 0;
}

static int tcc_isdbt_ctrl_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_state);
	device_destroy(tcc_isdbt_ctrl_class, MKDEV(ISDBT_CTRL_DEV_MAJOR, ISDBT_CTRL_DEV_MINOR));
	class_destroy(tcc_isdbt_ctrl_class);
	unregister_chrdev(ISDBT_CTRL_DEV_MAJOR, ISDBT_CTRL_DEV_NAME);

	if (gIsdbtCtrl)
		kfree(gIsdbtCtrl);
	gIsdbtCtrl = NULL;

	return 0;
}


/*****************************************************************************
 * TCC_ISDBT_CTRL Module Init/Exit
 ******************************************************************************/
#ifdef CONFIG_OF
static const struct of_device_id tcc_isdbt_ctrl_of_match[] = {
	{.compatible = "telechips,tcc_isdbt_ctrl", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_isdbt_ctrl_of_match);
#endif

static struct platform_driver tcc_isdbt_ctrl = {
	.probe	= tcc_isdbt_ctrl_probe,
	.remove	= tcc_isdbt_ctrl_remove,
	.driver	= {
		.name	= ISDBT_CTRL_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tcc_isdbt_ctrl_of_match,
#endif
	},
};

static int __init tcc_isdbt_ctrl_init(void)
{
	return platform_driver_register(&tcc_isdbt_ctrl);
}

static void __exit tcc_isdbt_ctrl_exit(void)
{
	platform_driver_unregister(&tcc_isdbt_ctrl);
}

module_init(tcc_isdbt_ctrl_init);
module_exit(tcc_isdbt_ctrl_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC ISDBT Broadcasting Control(Power, Reset..)");
MODULE_LICENSE("GPL");
