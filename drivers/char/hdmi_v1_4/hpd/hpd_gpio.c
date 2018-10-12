/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi.c
*  \brief       HDMI TX controller driver
*  \details   
*  \version     1.0
*  \date        2014-2015
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

#include <hdmi_hpd.h>

#define HPD_DEBUG 		0
#define HPD_DEBUG_GPIO 	0
#if HPD_DEBUG
#define DPRINTK(args...)    printk("hpd-gpio:" args)
#else
#define DPRINTK(args...)
#endif

#define VERSION         "1.0" /* Driver version number */


struct hpd_dev {
        struct device *pdev;

        /** Misc Device */
        struct miscdevice *misc;
        struct work_struct tx_hotplug_handler;
        
        /** Hot Plug */
        int hotplug_irq_enable;
        
        int hotplug_gpio;
        int hotplug_irq;

        /* This variable represent real hotplug status */
        int hotplug_real_status;
        /* This variable represent virtual hotplug status 
         * If hotplug_locked was 0, It represent hotplug_real_status
         * On the other hand, If hotplug_locked was 1, it represent 
         * hotplug_real_status at the time of hotplug_locked was set to 1 */
        int hotplug_status;
        

        int hotplug_locked;

        /** support poll */
        wait_queue_head_t poll_wq;
        int prev_hotplug_status; 

};        


static void hpd_set_hotplug_interrupt(struct hpd_dev *dev, int enable)
{
        int flag;

        if(dev != NULL) {
                if(dev->hotplug_irq_enable != enable) {
                        if(enable) {
                                //pr_info("%s hotplug_real_status = %d\r\n", __func__, dev->hotplug_real_status);
                                flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
                                irq_set_irq_type(dev->hotplug_irq, flag);
                                enable_irq(dev->hotplug_irq);
                        } else {
                                disable_irq(dev->hotplug_irq);
                                cancel_work_sync(&dev->tx_hotplug_handler);
                        }
                        dev->hotplug_irq_enable = enable;
                }
        }
}

static void hpd_hotplug_thread(struct work_struct *work)
{
        int i;
        int match = 0;
        struct hpd_dev *dev;
        int prev_hpd, current_hpd;

        dev = container_of(work, struct hpd_dev, tx_hotplug_handler);

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
                hpd_set_hotplug_interrupt(dev, 1);
        }
}

static irqreturn_t hpd_irq_handler(int irq, void *dev_id)
{
        struct hpd_dev *dev =  (struct hpd_dev *)dev_id;
        
        if(dev == NULL) {
                pr_err("%s: irq_dev is NULL\r\n", __func__);
                goto end_handler;
        }
        /* disable hpd irq */
        disable_irq_nosync(dev->hotplug_irq);
                
        schedule_work(&dev->tx_hotplug_handler);     
        
        return IRQ_HANDLED;
        
end_handler:
        return IRQ_NONE;
}

static int hpd_deinit_interrupts(struct hpd_dev *dev)
{
        cancel_work_sync(&dev->tx_hotplug_handler);
 
        devm_free_irq(dev->pdev, dev->hotplug_irq, &dev);
        return 0;
}

static int hpd_init_interrupts(struct hpd_dev *dev)
{
        int flag;
        int ret = 0;

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
                                pr_info(" IRQ=%d HPD=%d\r\n", dev->hotplug_irq, dev->hotplug_status);
                                flag = (dev->hotplug_real_status?IRQ_TYPE_LEVEL_LOW:IRQ_TYPE_LEVEL_HIGH)|IRQF_ONESHOT;
                                ret = devm_request_irq(dev->pdev, dev->hotplug_irq, hpd_irq_handler, flag, "hpd_irq_handler", dev);
                                //disable_irq(dev->hotplug_irq);
                        }
                        if(ret < 0) {
                                pr_err("%s failed request interrupt for hotplug\r\n", __func__);
                        }
                }
        }
        
        return ret;
}



int hpd_open(struct inode *inode, struct file *file)
{
        struct miscdevice *misc = (struct miscdevice *)file->private_data;
        struct hpd_dev *dev = dev_get_drvdata(misc->parent);

        file->private_data = dev;        

        return 0;
}

int hpd_release(struct inode *inode, struct file *file)
{
        //struct hpd_dev *dev = (struct hpd_dev *)file->private_data;   

        return 0;
}

ssize_t hpd_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
        ssize_t retval = 0;
        struct hpd_dev *dev = NULL;

        if(file != NULL) {
                dev = (struct hpd_dev *)file->private_data;  

                if(dev != NULL) {
        	        retval = put_user(dev->hotplug_status, (int __user *) buffer);
                }
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
        struct device *pdev = NULL;

        pr_info("%s : blank(mode=%d)\n",__func__, blank_mode);
        
        if(dev != NULL) {
                pdev = dev->pdev;
        }

        if(pdev != NULL) {

                #ifdef CONFIG_PM
        	if( (pdev->power.usage_count.counter==1) && (blank_mode == 0)) {	  
        		ret = 0;
        	} else {
                	switch(blank_mode)
                	{
                		case FB_BLANK_POWERDOWN:
                		case FB_BLANK_NORMAL:
                			pm_runtime_put_sync(pdev);
                                        ret = 0;
                			break;
                		case FB_BLANK_UNBLANK:
                			pm_runtime_get_sync(pdev);
                                        ret = 0;
                			break;
                		case FB_BLANK_HSYNC_SUSPEND:
                		case FB_BLANK_VSYNC_SUSPEND:
                		default:
                			ret = -EINVAL;
                	}
        	}
                #endif
        }

	return ret;
}


int hpd_start(struct hpd_dev *dev)
{
        //hpd_set_hotplug_interrupt(dev, 1);
        return 0;
}

int hpd_stop(struct hpd_dev *dev)
{
        //hpd_set_hotplug_interrupt(dev, 0);
        return 0;
}

long hpd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    
    struct hpd_dev *dev = (struct hpd_dev *)file->private_data;        
    

    switch (cmd) {
		case HPD_IOC_START:
			hpd_start(dev);
			break;
		case HPD_IOC_STOP:
			hpd_stop(dev);
			break;
		case HPD_IOC_BLANK:
		{
			unsigned int cmd;

			if (get_user(cmd, (unsigned int __user *) arg))
				return -EFAULT;

			printk(KERN_INFO "HPD: ioctl(HPD_IOC_BLANK :  %d )\n", cmd);

			hpd_blank(dev, cmd);

			break;

		}
        default:
            return -EINVAL;	
	}

	return 0;
}



#ifdef CONFIG_PM
int hpd_runtime_suspend(struct device *dev)
{
        struct hpd_dev *hpd_dev = NULL;
                                
        if(dev != NULL) {
                hpd_dev = (struct hpd_dev *)dev_get_drvdata(dev);

	        hpd_stop(hpd_dev);
        }

	return 0;
}

int hpd_runtime_resume(struct device *dev)
{
        struct hpd_dev *hpd_dev = NULL;
                                
        if(dev != NULL) {
                hpd_dev = (struct hpd_dev *)dev_get_drvdata(dev);
        
                hpd_start(hpd_dev);
        }
        
	return 0;
}

static int hpd_suspend(struct device *dev)
{
	DPRINTK(KERN_INFO "%s\n", __FUNCTION__);
	
	return 0;
}

static int hpd_resume(struct device *dev)
{
	DPRINTK(KERN_INFO "%s\n", __FUNCTION__);

	return 0;
}
#endif

static unsigned int hpd_poll(struct file *file, poll_table *wait){
        unsigned int mask = 0;
        struct hpd_dev *dev = (struct hpd_dev *)file->private_data;  
        
        poll_wait(file, &dev->poll_wq, wait);

        if(dev->prev_hotplug_status != dev->hotplug_status) {
                dev->prev_hotplug_status = dev->hotplug_status;
                mask = POLLIN;
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
        struct hpd_dev *dev = NULL;
                        
        if(pdev != NULL) {
                dev = (struct hpd_dev *)dev_get_drvdata(pdev->dev.parent);

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
                pr_info("HPD Driver ver. %s\n", VERSION);
                dev = devm_kzalloc(&pdev->dev, sizeof(struct hpd_dev), GFP_KERNEL);
                if (dev == NULL) {
                        break;
                }

                dev->pdev = &pdev->dev;
                dev->hotplug_gpio = of_get_gpio(pdev->dev.of_node, 0);

                dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL);
                if(dev->misc == NULL) {
                        break;
                }
                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "hpd";
                dev->misc->fops = &hpd_fops;
                dev->misc->parent = dev->pdev;
                ret = misc_register(dev->misc);
                if(ret < 0) {
                        devm_kfree(dev->pdev, dev->misc);
                        break;
                }
                
                dev_set_drvdata(dev->pdev, dev);
                
                // wait queue of poll
                init_waitqueue_head(&dev->poll_wq);

                dev->prev_hotplug_status = -1;
                hpd_init_interrupts(dev);
                
                #ifdef CONFIG_PM
                pm_runtime_set_active(dev->pdev);	
                pm_runtime_enable(dev->pdev);  
                pm_runtime_get_noresume(dev->pdev);  //increase usage_count 
                #endif

                
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
	//pr_info("%s\n", __func__);

	return platform_driver_register(&tcc_hdmi_hpd);
}

static __exit void hpd_exit(void)
{
	//pr_info("%s\n", __func__);
	platform_driver_unregister(&tcc_hdmi_hpd);
}

module_init(hpd_init);
module_exit(hpd_exit);
MODULE_DESCRIPTION("TCC897x hdmi hpd driver");
MODULE_LICENSE("GPL");

