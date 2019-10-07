/*
 * linux/drivers/char/tcc_lut.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#define pr_fmt(fmt) "\x1b[1;38m TCC LUT: \x1b[0m" fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <asm/io.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut.h>
#include <video/tcc/tcc_lut_ioctl.h>

#define LUT_VERSION "v1.6"

#define TCC_LUT_DEBUG	0
#define dprintk(msg, ...) if(TCC_LUT_DEBUG) { pr_info(msg, ##__VA_ARGS__); }


#define DEFAULT_DEV_MAX		2 /* 0: DEV0, 1: DEV1 and 2: DEV2 */
#define DEFAULT_VIOC_MAX	6 /* 3: VIOC_LUT0, 4: VIOC_LUT1, 5: VIOC_LUT2 and 6: VIOC_LUT3 */

struct lut_drv_type {
	unsigned int dev_opened;
	unsigned int dev_max;
	unsigned int vioc_max;
	struct miscdevice *misc;
};

struct lut_drv_type *lut_api = NULL;

static int lut_drv_set_plugin(struct lut_drv_type *lut, unsigned int lut_number, int plugin, int plug_in_ch);

/* External APIs */
/* if lut is not plugged then return value will be -1 */
int lut_drv_api_get_plugin(unsigned int lut_number)
{
	int ret = -1;
	if(lut_api != NULL) {
		if(lut_number > lut_api->dev_max &&
			lut_number <= lut_api->vioc_max) {
			if(tcc_get_lut_enable(lut_number)) {
				ret = tcc_get_lut_plugin(lut_number);
			}
		}else {
			pr_err("%s lut number %d is out of range\r\n", __func__, lut_number);
		}
	} else {
		pr_err("%s may be lut driver does not probed\r\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(lut_drv_api_get_plugin);

int lut_drv_api_set_plugin(unsigned int lut_number, int plugin, int plug_in_ch)
{
	int ret = -1;
	if(lut_api != NULL) {
		ret = lut_drv_set_plugin(lut_api, lut_number, plugin, plug_in_ch);
	} else {
		pr_err("%s may be lut driver does not probed\r\n", __func__);
	}
	return ret;
}
EXPORT_SYMBOL(lut_drv_api_set_plugin);

/* Internal APIs */
static unsigned int lut_get_real_lut_table_number(unsigned int input_lut_number)
{
	unsigned int lut_number = (unsigned int)-1;

	switch(input_lut_number) {
		case LUT_DEV0:
		case LUT_DEV1:
			lut_number = input_lut_number;
			break;

		#if defined(CONFIG_ARCH_TCC803X)
		case LUT_DEV2:
			lut_number = input_lut_number;
			break;
		#endif

		case LUT_COMP0:
			lut_number = input_lut_number;
			break;

		case LUT_COMP1:
			#if defined(CONFIG_ARCH_TCC898X) ||defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
			lut_number = 5; /* Number of LUT in VIOC1 RGB is 5 */
			#else
			lut_number = input_lut_number;
			#endif
			break;

		#if defined(CONFIG_ARCH_TCC897X)
		case LUT_COMP2:
		case LUT_COMP3:
			lut_number = input_lut_number;
			break;
		#endif

	}

	return lut_number;
}

static int lut_drv_set_plugin(struct lut_drv_type *lut, unsigned int lut_number, int plugin, int plug_in_ch)
{
	int ret = -1;

	do {
		if(lut == NULL) {
			break;
		}
		if(lut_number <= lut->dev_max) {
			pr_err("%s lut number %d is out of range\r\n",
							__func__, lut_number);
			break;
		}

		if(lut_number > lut->vioc_max) {
			pr_err("%s lut number %d is out of range\r\n",
							__func__, lut_number);
			break;
		}

		if(!plugin) {
			dprintk("%s disable lut_number(%d)\r\n", __func__, VIOC_LUT + lut_number);
			tcc_set_lut_enable(VIOC_LUT + lut_number, 0);
		}
		else	{
			dprintk("%s enable lut_number(%d)\r\n", __func__, VIOC_LUT + lut_number);
			tcc_set_lut_plugin(VIOC_LUT + lut_number, VIOC_RDMA + plug_in_ch);
			tcc_set_lut_enable(VIOC_LUT + lut_number, 1);
		}
		ret = 0;
	}while(0);
	return ret;
}

static int lut_drv_set_onoff(struct lut_drv_type *lut, unsigned int lut_number, int onoff)
{
	int ret = -1;

	do {
		if(lut == NULL) {
			break;
		}
		if(lut_number > lut->vioc_max) {
			pr_err("%s lut number %d is out of range\r\n",
							__func__, lut_number);
			break;
		}
		dprintk("lut num:%d enable:%d \n", VIOC_LUT + lut_number, onoff);
		tcc_set_lut_enable(VIOC_LUT + lut_number, onoff);
		ret = 0;
 	} while(0);
	return ret;
}

static long lut_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);

	switch(cmd) {
		case TCC_LUT_SET:
			{
				unsigned int lut_number;
				struct VIOC_LUT_VALUE_SET *lut_cmd;

				lut_cmd = kmalloc(sizeof(struct VIOC_LUT_VALUE_SET), GFP_KERNEL);

				if(copy_from_user((void *)lut_cmd, (const void *)arg, sizeof(struct VIOC_LUT_VALUE_SET)))				{
					kfree(lut_cmd);
					break;
				}
				lut_number = lut_get_real_lut_table_number(lut_cmd->lut_number);
				if(lut_number == (unsigned int)-1 ) {
					pr_err("%s TCC_LUT_SET invalid lut number[%d]\r\n", __func__, lut_cmd->lut_number);
					break;
				}

				tcc_set_lut_table(VIOC_LUT + lut_number, lut_cmd->Gamma);
				kfree(lut_cmd);
				ret = 0;
			}
			break;

                case TCC_LUT_SET_EX:
                        {
				unsigned int lut_number;
                                struct VIOC_LUT_VALUE_SET_EX *lut_value_set_ex = NULL;

                                lut_value_set_ex =
					kmalloc(sizeof(struct VIOC_LUT_VALUE_SET_EX), GFP_KERNEL);

				do {
	                                if(lut_value_set_ex == NULL) {
	                                        pr_err("%s TCC_LUT_SET_EX out of memory\r\n", __func__);
	                                        break;
	                                }
	                                if(copy_from_user((void *)lut_value_set_ex,
	                                        (const void *)arg, sizeof(struct VIOC_LUT_VALUE_SET_EX))) {
	                                        pr_err("%s TCC_LUT_SET_EX failed copy from user\r\n", __func__);
	                                        break;
	                                }

	                                if(LUT_TABLE_SIZE != lut_value_set_ex->lut_size) {
	                                        pr_err("%s TCC_LUT_SET_EX table size mismatch %d != %d\r\n",
	                                                __func__, LUT_TABLE_SIZE, lut_value_set_ex->lut_size);
	                                        break;
	                                }
					lut_number = lut_get_real_lut_table_number(lut_value_set_ex->lut_number);
					if(lut_number == (unsigned int)-1 ) {
						pr_err("%s TCC_LUT_SET_EX invalid lut number[%d]\r\n", __func__, lut_value_set_ex->lut_number);
						break;
					}
					dprintk("TCC_LUT_SET_EX lut_sel = %d\r\n", lut_number);

					/* Y Table? */
					#if defined(CONFIG_ARCH_TCC898X) ||defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
					dprintk("TCC_LUT_SET_EX para = %d\r\n", lut_value_set_ex->param);
					if(lut_value_set_ex->param & 1) {
						if(lut_number == 3 || lut_number == 5) {
							lut_number++;
							dprintk("TCC_LUT_SET_EX y-lut lut_sel = %d\r\n", lut_number);
						}
					}
					#endif

					dprintk("TCC_LUT_SET_EX tcc_set_lut_table with lut_sel = %d\r\n", lut_number);
	                                tcc_set_lut_table(VIOC_LUT + lut_number, lut_value_set_ex->Gamma);
					ret = 0;
				}while(0);

				if(lut_value_set_ex != NULL) {
					kfree(lut_value_set_ex);
				}
			}
                        break;

		case TCC_LUT_ONOFF:
			{
				struct VIOC_LUT_ONOFF_SET lut_cmd;

				if(copy_from_user((void *)&lut_cmd, (const void *)arg, sizeof(struct VIOC_LUT_ONOFF_SET))) {
					break;
				}
				ret = lut_drv_set_onoff(lut, lut_cmd.lut_number, lut_cmd.lut_onoff);
			}
			break;

		case TCC_LUT_PLUG_IN:
			{
				struct VIOC_LUT_PLUG_IN_SET lut_cmd;

				if(copy_from_user((void *)&lut_cmd, (const void *)arg, sizeof(lut_cmd)))
					break;
				if(lut_cmd.lut_number == (unsigned int)-1 ) {
					pr_err("%s TCC_LUT_ONOFF invalid lut number[%d]\r\n", __func__, lut_cmd.lut_number);
					break;
				}
				ret = lut_drv_set_plugin(lut, lut_cmd.lut_number, lut_cmd.enable, lut_cmd.lut_plug_in_ch);
			}
			break;

                case TCC_LUT_GET_DEPTH:
                        {
                                unsigned int lut_depth = LUT_COLOR_DEPTH;
                                if(copy_to_user((void __user *)arg, &lut_depth, sizeof(lut_depth))) {
                                        break;
                                }
				dprintk("TCC_LUT_GET_DEPTH LUT depth is  %d\r\n", lut_depth);
				ret = 0;
                        }
                        break;

		#if defined(CONFIG_ARCH_TCC898X) ||defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		case TCC_LUT_SET_CSC_COEFF:
			{
				if((void*)arg == NULL) {
					tcc_set_default_lut_csc_coeff();
				} else {
					struct VIOC_LUT_CSC_COEFF csc_coeff;
					unsigned int lut_csc_11_12, lut_csc_13_21, lut_csc_22_23, lut_csc_31_32, lut_csc_33;
					if(copy_from_user((void *)&csc_coeff, (const void *)arg, sizeof(struct VIOC_LUT_CSC_COEFF)))
						break;

					lut_csc_11_12 = csc_coeff.csc_coeff_1[0] | (csc_coeff.csc_coeff_1[1] << 16);
	                                lut_csc_13_21 = csc_coeff.csc_coeff_1[2] | (csc_coeff.csc_coeff_2[0] << 16);
	                                lut_csc_22_23 = csc_coeff.csc_coeff_2[1] | (csc_coeff.csc_coeff_2[2] << 16);
	                                lut_csc_31_32 = csc_coeff.csc_coeff_3[0] | (csc_coeff.csc_coeff_3[1] << 16);
	                                lut_csc_33 = csc_coeff.csc_coeff_3[2];
	                                dprintk("TCC_LUT_PRESET_SET csc (0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\r\n",
						lut_csc_11_12, lut_csc_13_21, lut_csc_22_23, lut_csc_31_32, lut_csc_33);
	                                tcc_set_lut_csc_coeff(lut_csc_11_12, lut_csc_13_21, lut_csc_22_23, lut_csc_31_32, lut_csc_33);
				}
				ret = 0;
			}
			break;

		case TCC_LUT_SET_MIX_CONIG:
			{
				struct VIOC_LUT_MIX_CONFIG mix_config;
				if(copy_from_user((void *)&mix_config, (const void *)arg, sizeof(struct VIOC_LUT_MIX_CONFIG)))
						break;
				dprintk("TCC_LUT_SET_MIX_CONIG r2y_sel(%d), bypass(%d)\r\n", mix_config.r2y_sel, mix_config.bypass);
				tcc_set_mix_config(mix_config.r2y_sel, mix_config.bypass);
				ret = 0;
			}
			break;
		#endif

		case TCC_LUT_GET_UPDATE_PEND:
			{
				int tcc_get_lut_update_pend(unsigned int lut_n);
				unsigned int lut_number;
				struct VIOC_LUT_UPDATE_PEND lut_update_pend;

                                if(copy_from_user((void *)&lut_update_pend,
                                        (const void *)arg, sizeof(struct VIOC_LUT_UPDATE_PEND))) {
                                        pr_err("%s TCC_LUT_GET_UPDATE_PEND failed copy from user\r\n", __func__);
                                        break;
                                }

				#if defined(CONFIG_ARCH_TCC898X) ||defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
				lut_number = lut_get_real_lut_table_number(lut_update_pend.lut_number);
				if(lut_number == (unsigned int)-1 ) {
					pr_err("%s TCC_LUT_GET_UPDATE_PEND invalid lut number[%d]\r\n", __func__, lut_update_pend.lut_number);
					break;
				}

				/* Y Table? */
				if(lut_update_pend.param & 1) {
					if(lut_number == 3 || lut_number == 5) {
						lut_number++;
						dprintk("TCC_LUT_GET_UPDATE_PEND y-lut lut_sel = %d\r\n", lut_number);
					}
				}
				//pr_info(" %s LUT\r\n", (lut_number == 4 || lut_number == 6)?"Y":"RGB");
				lut_update_pend.update_pend = tcc_get_lut_update_pend(lut_number);
				#else
				lut_update_pend.update_pend = 0;
				#endif
                                if(copy_to_user((void __user *)arg, &lut_update_pend, sizeof(struct VIOC_LUT_UPDATE_PEND))) {
					pr_err("%s TCC_LUT_GET_UPDATE_PEND failed copy to user\r\n", __func__);
                                        break;
                                }
				ret = 0;
			}
			break;

		default:
			printk(KERN_ALERT "not supported LUT IOCTL(0x%x). \n", cmd);
			break;
	}

	return ret;
}

static int lut_drv_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct lut_drv_type	*lut = dev_get_drvdata(misc->parent);

	dprintk("%s():  In -open(%d) \n", __func__, lut->dev_opened);

	if(lut != NULL) {
		lut->dev_opened++;
	}
	return ret;
}

static int lut_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct lut_drv_type	*lut = dev_get_drvdata(misc->parent);

	if(lut != NULL && lut->dev_opened > 0) {
		lut->dev_opened--;
	}
	dprintk("%s(): open(%d). \n", __func__, lut->dev_opened);

	return 0;
}

static struct file_operations lut_drv_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= lut_drv_ioctl,
	.open			= lut_drv_open,
	.release		= lut_drv_release,
};

static int lut_drv_probe(struct platform_device *pdev)
{
	struct lut_drv_type *lut;
	int ret = -ENODEV;

	lut = kzalloc(sizeof(struct lut_drv_type), GFP_KERNEL);
	if (lut == NULL) {
		return -ENOMEM;
	}

	if(of_property_read_u32(pdev->dev.of_node, "dev_max", &lut->dev_max) < 0) {
		lut->dev_max = DEFAULT_DEV_MAX;
	}

	if(of_property_read_u32(pdev->dev.of_node, "vioc_max", &lut->vioc_max) < 0) {
		lut->vioc_max = DEFAULT_VIOC_MAX;
	}
	pr_info("dev_max is %d and vioc_max is %d\r\n", lut->dev_max, lut->vioc_max);

	lut->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (lut->misc == NULL) {
		goto err_misc_alloc;
	}

 	/* register scaler discdevice */
	lut->misc->minor = MISC_DYNAMIC_MINOR;
	lut->misc->fops = &lut_drv_fops;
	lut->misc->name = "tcc_lut";
	lut->misc->parent = &pdev->dev;
	ret = misc_register(lut->misc);
	if (ret)
		goto err_misc_register;

	platform_set_drvdata(pdev, lut);

	/* Copy lut to lut_api to support external APIs */
	lut_api = lut;

	pr_info("%s: :%s, Driver %s Initialized  lut set num :0x%x\n",__func__, LUT_VERSION, pdev->name, TCC_LUT_SET);
	return 0;

err_misc_register:
	misc_deregister(lut->misc);

err_misc_alloc:
	kfree(lut->misc);
	kfree(lut);
	printk("%s: %s: err ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int lut_drv_remove(struct platform_device *pdev)
{
	struct lut_drv_type *lut = (struct lut_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(lut->misc);
	kfree(lut);

	lut_api = NULL;
	return 0;
}

static int lut_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int lut_drv_resume(struct platform_device *pdev)
{
	return 0;
}


static struct of_device_id lut_of_match[] = {
	{ .compatible = "telechips,vioc_lut" },
	{}
};
MODULE_DEVICE_TABLE(of, lut_of_match);

static struct platform_driver lut_driver = {
	.probe		= lut_drv_probe,
	.remove		= lut_drv_remove,
	.suspend		= lut_drv_suspend,
	.resume		= lut_drv_resume,
	.driver 	= {
		.name	= "tcc_lut",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(lut_of_match),
#endif
	},
};

static int __init lut_drv_init(void)
{
	return platform_driver_register(&lut_driver);
}

static void __exit lut_drv_exit(void)
{
	platform_driver_unregister(&lut_driver);
}

module_init(lut_drv_init);
module_exit(lut_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips look up table Driver");
MODULE_LICENSE("GPL");



