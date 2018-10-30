/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hpd.c
*  \brief       HDMI HPD controller driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not 
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source 
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular 
purpose or non-infringement of any patent, copyright or other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source code or the use in the 
source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure 
Agreement between Telechips and Company. 
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include <asm/io.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include <hdmi_1_4_hpd.h>

#define HPD_DEBUG 	0
#define HPD_DEBUG_GPIO 	0
#if HPD_DEBUG
#define DPRINTK(args...)    printk("hpd-gpio:" args)
#else
#define DPRINTK(args...)
#endif

#define VERSION "4.14_1.0.5"

struct hpd_dev {
        struct device *pdev;

        /** Misc Device */
        struct miscdevice *misc;
        struct work_struct tx_hotplug_handler;
        
        /** Hot Plug */        
        int hotplug_gpio;
        int hotplug_irq;
        int hotplug_irq_enable;
        int hotplug_irq_enabled;

        /* This variable represent real hotplug status */
        int hotplug_real_status;
        /* This variable represent virtual hotplug status 
         * If hotplug_locked was 0, It represent hotplug_real_status
         * On the other hand, If hotplug_locked was 1, it represent 
         * hotplug_real_status at the time of hotplug_locked was set to 1 */
        int hotplug_status;
        

        int hotplug_locked;

        int suspend;
        int runtime_suspend;

        /** support poll */
        wait_queue_head_t poll_wq;
        int prev_hotplug_status; 

        struct mutex mutex;

};        

static void hpd_set_hotplug_interrupt(struct hpd_dev *dev, int enable)
{
        int flag;

        if(dev != NULL) {
                
                if(enable) {
                        pr_info("%s hotplug_real_status = %d\r\n", __func__, dev->hotplug_real_status);
                        flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
                        irq_set_irq_type(dev->hotplug_irq, flag);
                        if(!dev->hotplug_irq_enabled) {
                                enable_irq(dev->hotplug_irq);
                                dev->hotplug_irq_enabled = 1;
                        }
                } else {
                        if(dev->hotplug_irq_enabled) {
                                disable_irq(dev->hotplug_irq);
                                dev->hotplug_irq_enabled = 0;
                        }
                        cancel_work_sync(&dev->tx_hotplug_handler);
                }
        }
}

static void hpd_hotplug_thread(struct work_struct *work)
{
        int i;
        int match = 0;
        struct hpd_dev *dev;
        int prev_hpd, current_hpd;

        dev = (work!=NULL)?container_of(work, struct hpd_dev, tx_hotplug_handler):NULL;

        if(dev != NULL) {   
                prev_hpd = gpio_get_value(dev->hotplug_gpio);
                  
                /* Check HPD */
                for(i=0;i<4;i++) {
                        current_hpd = gpio_get_value(dev->hotplug_gpio);
                        if(current_hpd == prev_hpd) {
                                match++;
                        }
                        udelay(10);
                }

                /* If match is less than 4, it is assumed to be noise. */
                if(match >= 4) {
                        if(dev->hotplug_real_status != current_hpd) {
                                pr_info("%s hotplug_real_status = %d\r\n", __func__, current_hpd);
                                wake_up_interruptible(&dev->poll_wq);        
                        }
                        dev->hotplug_real_status = current_hpd;
                        if(dev->hotplug_locked == 0) {
                                dev->hotplug_status = dev->hotplug_real_status;
                        }
                }
                mutex_lock(&dev->mutex);
                hpd_set_hotplug_interrupt(dev, 1);
                mutex_unlock(&dev->mutex);
        }
}

static irqreturn_t hpd_irq_handler(int irq, void *dev_id)
{
        struct hpd_dev *dev =  (struct hpd_dev *)dev_id;
        
        if(dev != NULL) {
                /* disable hpd irq */
                dev->hotplug_irq_enabled = 0;
                disable_irq_nosync(dev->hotplug_irq);
                
                schedule_work(&dev->tx_hotplug_handler);     
        }
        
        return IRQ_HANDLED;
}

static int hpd_deinit_interrupts(struct hpd_dev *dev)
{
        if(dev != NULL) {
                cancel_work_sync(&dev->tx_hotplug_handler);

                devm_free_irq(dev->pdev, dev->hotplug_irq, &dev);
        }
        return 0;
}

static int hpd_init_interrupts(struct hpd_dev *dev)
{
        int flag, ret = -1;

        if(dev != NULL) {
                INIT_WORK(&dev->tx_hotplug_handler, hpd_hotplug_thread);

                /* Check GPIO HPD */
                dev->hotplug_irq = -1;
                if (gpio_is_valid(dev->hotplug_gpio)) {
                        if(devm_gpio_request(dev->pdev, dev->hotplug_gpio, "hdmi_hotplug") < 0 ) {
                                pr_err("%s failed get gpio request\r\n", __func__);
                        } else {
                                gpio_direction_input(dev->hotplug_gpio);
                                dev->hotplug_irq = gpio_to_irq(dev->hotplug_gpio);
                                if(dev->hotplug_irq < 0) {
                                        pr_err("%s can not convert gpio to irq\r\n", __func__);
                                        ret = -1;
                                } else {
                                        dev->hotplug_real_status = gpio_get_value(dev->hotplug_gpio)?1:0;
                                        if(dev->hotplug_locked == 0) {
                                                dev->hotplug_status = dev->hotplug_real_status;
                                        }       
                                        flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
                                        ret = devm_request_irq(dev->pdev, dev->hotplug_irq, hpd_irq_handler, flag, "hdmi-hotplug", dev);
                                        if(ret == 0) {
                                                /* Disable HPD */
                                                dev->hotplug_irq_enabled = 1;
                                        }
                                        
                                }
                                if(ret < 0) {
                                        pr_err("%s failed request interrupt for hotplug\r\n", __func__);
                                }
                        }
                }
        }
        
        return ret;
}



int hpd_open(struct inode *inode, struct file *file)
{
        int ret = -1;
        struct miscdevice *misc = (struct miscdevice *)(file!=NULL)?file->private_data:NULL;
        struct hpd_dev *hpd_dev = (struct hpd_dev *)(misc!=NULL)?dev_get_drvdata(misc->parent):NULL;
        
        if(hpd_dev != NULL) {
                file->private_data = hpd_dev;        
                ret = 0;
        }
        return ret;
}

int hpd_release(struct inode *inode, struct file *file)
{
        return 0;
}

ssize_t hpd_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
        ssize_t retval = 0;
        struct hpd_dev *dev = (struct hpd_dev *)(file!=NULL)?(struct hpd_dev *)file->private_data:NULL;

        if(dev != NULL) {
                retval = put_user(dev->hotplug_status, (int __user *) buffer);
        }
        return retval;
}

/**
 *      tccfb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *	blank_mode == 2: suspend vsync
 *	blank_mode == 3: suspend hsync
 *	blank_mode == 4: powerdown
 *
 *	Returns negative errno on error, or zero on success.
 *
 */

static int hpd_blank(struct hpd_dev *dev, int blank_mode)
{
	int ret = -EINVAL;
        struct device *pdev = (dev!=NULL)?dev->pdev:NULL;

        pr_info("%s : blank(mode=%d)\n",__func__, blank_mode);
        if(pdev != NULL) {
                #ifdef CONFIG_PM
        	switch(blank_mode)
        	{
        		case FB_BLANK_POWERDOWN:
        		case FB_BLANK_NORMAL:
        			pm_runtime_put_sync(pdev);
                                ret = 0;
        			break;
        		case FB_BLANK_UNBLANK:
                                if(pdev->power.usage_count.counter == 1) {
                                /* 
                                 * usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK ) means that 
                                 * this driver is stable state when booting. don't call runtime_suspend or resume state  */
                                } else {
                	                pm_runtime_get_sync(dev->pdev);
                                }
                                ret = 0;
        			break;
        		case FB_BLANK_HSYNC_SUSPEND:
        		case FB_BLANK_VSYNC_SUSPEND:
        		default:
        			ret = -EINVAL;
        	}
                #endif
        }

	return ret;
}


int hpd_start(struct hpd_dev *dev)
{
        if(dev != NULL) {
                dev->hotplug_irq_enable = 1;
                if(dev->suspend) {
                        pr_err("%s hpd is suspended\r\n", __func__);
                } else {
                        mutex_lock(&dev->mutex);
                        hpd_set_hotplug_interrupt(dev, dev->hotplug_irq_enable);
                        mutex_unlock(&dev->mutex);
                }
        }
        return 0;
}

int hpd_stop(struct hpd_dev *dev)
{
        if(dev != NULL) {
                dev->hotplug_irq_enable = 0;
                if(dev->suspend) {
                        pr_err("%s hpd is suspended\r\n", __func__);
                } else {
                        mutex_lock(&dev->mutex);
                        hpd_set_hotplug_interrupt(dev, dev->hotplug_irq_enable);
                        mutex_unlock(&dev->mutex);
                }
        }
        return 0;
}

long hpd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        long ret = -EINVAL;

        struct hpd_dev *dev = (struct hpd_dev *)(file!=NULL)?(struct hpd_dev *)file->private_data:NULL;

        if(dev != NULL) {
                switch (cmd) {
                        case HPD_IOC_START:
                        	ret = hpd_start(dev);
                        	break;
                        case HPD_IOC_STOP:
                        	ret = hpd_stop(dev);
                        	break;
                        case HPD_IOC_BLANK:
                                {
                                	unsigned int cmd;
                                	if(get_user(cmd, (unsigned int __user *) arg))
                                                break;
                                	ret = hpd_blank(dev, cmd);
                                }
                                break;
                        default:
                                break;
                }
        }
        return ret;
}



#ifdef CONFIG_PM
static int hpd_suspend(struct device *dev)
{
        struct hpd_dev *hpd_dev = (struct hpd_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;

        if(hpd_dev != NULL) {
                if(hpd_dev->runtime_suspend) {
                        pr_info("hpd_runtime_suspend\r\n");
                } else {
                        pr_info("hpd_suspend\r\n");
                }
	        hpd_set_hotplug_interrupt(hpd_dev, 0);
        }
        return 0;
}

static int hpd_resume(struct device *dev)
{
        struct hpd_dev *hpd_dev = (struct hpd_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        pr_info("hpd_resume\r\n");                        
        if(hpd_dev != NULL) {
                hpd_dev->hotplug_real_status = gpio_get_value(hpd_dev->hotplug_gpio);
                if(!hpd_dev->runtime_suspend) {
                        hpd_set_hotplug_interrupt(hpd_dev, hpd_dev->hotplug_irq_enable);
                }
        }
        return 0;
}

int hpd_runtime_suspend(struct device *dev)
{
        struct hpd_dev *hpd_dev = (struct hpd_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        
        if(hpd_dev != NULL) {
                hpd_dev->runtime_suspend = 1;
                mutex_lock(&hpd_dev->mutex);
                hpd_suspend(dev);
                mutex_unlock(&hpd_dev->mutex);
        }
	return 0;
}

int hpd_runtime_resume(struct device *dev)
{
        struct hpd_dev *hpd_dev = (struct hpd_dev *)(dev!=NULL)?dev_get_drvdata(dev):NULL;
        if(hpd_dev != NULL) {
                if(hpd_dev->runtime_suspend) {
                        hpd_dev->runtime_suspend = 0;
                        mutex_lock(&hpd_dev->mutex);
                        hpd_resume(dev);
                        mutex_unlock(&hpd_dev->mutex);
                }
        }
        return 0;
}
#endif

static unsigned int hpd_poll(struct file *file, poll_table *wait){
        unsigned int mask = 0;
        struct hpd_dev *dev = (struct hpd_dev *)(file!=NULL)?file->private_data:NULL;

        if(dev != NULL) {
                poll_wait(file, &dev->poll_wq, wait);

                if(dev->prev_hotplug_status != dev->hotplug_status) {
                        dev->prev_hotplug_status = dev->hotplug_status;
                        mask = POLLIN;
                }
        }
        return mask;
}

static const struct file_operations hpd_fops =
{
    .owner   = THIS_MODULE,
    .open    = hpd_open,
    .release = hpd_release,
    .read    = hpd_read,
    .unlocked_ioctl = hpd_ioctl,
    .poll    = hpd_poll,
};

static int hpd_remove(struct platform_device *pdev)
{
        struct hpd_dev *dev = (struct hpd_dev *)(pdev!=NULL)?dev_get_drvdata(pdev->dev.parent):NULL;
                        
        if(pdev != NULL) {
                if(dev != NULL) {
                        hpd_deinit_interrupts(dev);
                        if(dev->misc != NULL) {
                                misc_deregister(dev->misc);
                                devm_kfree(dev->pdev, dev->misc);
                        }
                }
                devm_kfree(dev->pdev, dev);
        }        
        return 0;
}

static int hpd_probe(struct platform_device *pdev)
{
        int ret = -ENOMEM;
        struct hpd_dev *dev = NULL;
        do {
                pr_info("%s: HDMI HPD driver %s\n", __func__, VERSION);
                dev = devm_kzalloc(&pdev->dev, sizeof(struct hpd_dev), GFP_KERNEL);
                if (dev == NULL) {
                        break;
                }
                dev->pdev = &pdev->dev;
                dev->hotplug_gpio = of_get_gpio(pdev->dev.of_node, 0);

                mutex_init(&dev->mutex);
                dev_set_drvdata(dev->pdev, dev);
                
                // wait queue of poll
                init_waitqueue_head(&dev->poll_wq);

                dev->prev_hotplug_status = -1;
                if(hpd_init_interrupts(dev) < 0) {
                        break;
                }
                dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL);
                if(dev->misc == NULL) {
                        break;
                }
                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "hpd";
                dev->misc->fops = &hpd_fops;
                dev->misc->parent = dev->pdev;
                if(misc_register(dev->misc) < 0) {
                        break;
                }
                #ifdef CONFIG_PM
                pm_runtime_set_active(dev->pdev);	
                pm_runtime_enable(dev->pdev);  
                pm_runtime_get_noresume(dev->pdev);  //increase usage_count 
                #endif

                ret = 0;
        } while(0);

        if(ret < 0) {
                hpd_remove(pdev);
        }
        
        return ret;
}


/** 
 * @short of_device_id structure
 */
 static const struct of_device_id hdp_dev_match[] = {
	{ .compatible = "telechips,tcc897x-hdmi-hpd" },
	{}
};
MODULE_DEVICE_TABLE(of, hdp_dev_match);

#if CONFIG_PM
static const struct dev_pm_ops hpd_pm_ops = {
	.runtime_suspend = hpd_runtime_suspend,
	.runtime_resume = hpd_runtime_resume,
	.suspend = hpd_suspend,
	.resume = hpd_resume,
};
#endif

/** 
 * @short Platform driver structure
 */
static struct platform_driver __refdata tcc_hdmi_hpd = {
	.probe	= hpd_probe,
	.remove	= hpd_remove,
	.driver	= {
		.name	= "telechips,hpd",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(hdp_dev_match),
                #ifdef CONFIG_PM
		.pm = &hpd_pm_ops,
                #endif
	},
};

static __init int hpd_init(void)
{
	return platform_driver_register(&tcc_hdmi_hpd);
}

static __exit void hpd_exit(void)
{
	platform_driver_unregister(&tcc_hdmi_hpd);
}

module_init(hpd_init);
module_exit(hpd_exit);
MODULE_DESCRIPTION("TCC897x hdmi hpd driver");
MODULE_LICENSE("GPL");

