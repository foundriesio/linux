/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi_cec_misc.c
*  \brief       HDMI CEC controller driver
*  \details   
*  \version     1.0
*  \date        2016
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not 
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source 
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular 
purpose or non-infringement  of  any  patent,  copyright  or  other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source  code or the  use in the 
source code. 
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure 
Agreement between Telechips and Company.
*******************************************************************************/

#include "include/hdmi_cec.h"
#include "include/hdmi_cec_misc.h"
#include "include/hdmi_cec_ioctl.h"
#include "include/tcc_cec_interface.h"

#include "hdmi_cec_lib/cec.h"
#include "hdmi_cec_lib/cec_access.h"
#include "hdmi_cec_lib/cec_reg.h"

#include<linux/clk.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

//#define CEC_KERNEL_DEBUG

#ifdef CONFIG_PM
static int hdmi_cec_blank(struct cec_device *dev, int blank_mode)
{
	int ret = 0;

	struct device *pdev_cec = dev->parent_dev;

	printk("%s : blank(mode=%d)\n", __func__, blank_mode);

	if( (pdev_cec->power.usage_count.counter==1) && (blank_mode == 0))
	{
		// usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK ) is stable state when booting
		// don't call runtime_suspend or resume state 
	        //printk("%s ### state = [%d] count =[%d] power_cnt=[%d] \n",__func__,blank_mode, pdev_cec->power.usage_count, pdev_cec->power.usage_count.counter);		  
		return 0;
	}

	switch(blank_mode)
	{
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_NORMAL:
			pm_runtime_put_sync(pdev_cec);
			break;
		case FB_BLANK_UNBLANK:
			pm_runtime_get_sync(pdev_cec);
			break;
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		default:
			ret = -EINVAL;
	}

	return ret;
}
#endif

static long hdmi_cec_ioctl(struct file *file, unsigned int cmd, unsigned long arg){

        long ret = 0;
        struct cec_device *dev = (struct cec_device *)file->private_data; 

	switch (cmd) {
		case CEC_IOC_START:
		{
			cec_Init(dev);
		}
		break;

		case CEC_IOC_STOP:
		{
			int wakeup;
			ret = copy_from_user(&wakeup, (void __user *)arg, sizeof(int));
			cec_Disable(dev, wakeup);
		}
		break;

		case CEC_IOC_SETLADDR:
		{
			unsigned int laddr, enable = 1;
			
			if (get_user(laddr, (unsigned int __user *) arg))
				return -EFAULT;
		#ifdef CEC_KERNEL_DEBUG
			printk("%s: ioctl(CEC_IOC_SETLADDR)\n",__func__);				
			printk("%s: logical address = 0x%02x\n", __func__, laddr);
		#endif					
			dev->l_address = laddr;
			cec_CfgLogicAddr(dev,laddr, enable);
		}
		break;

		case CEC_IOC_CLEARLADDR:
		{
			unsigned int laddr, enable = 0;
			
			if (get_user(laddr, (unsigned int __user *) arg))
				return -EFAULT;
		#ifdef CEC_KERNEL_DEBUG
			printk("%s: ioctl(CEC_IOC_CLEARLADDR)\n",__func__);				
			printk("%s: logical address = 0x%02x\n", __func__, laddr);
		#endif					
			cec_CfgLogicAddr(dev,laddr, enable);
		}
		break;

		case CEC_IOC_SETPADDR:
		{
			unsigned int paddr;
			if (get_user(paddr, (unsigned int __user *) arg))
				return -EFAULT;

			dev->p_address = paddr;
		}
		break;

		case CEC_IOC_SENDDATA:
		{
		#ifdef CEC_KERNEL_DEBUG
			int i;
		#endif
			memset(dev->buf.send_buf,0,CEC_TX_DATA_SIZE);
			ret = copy_from_user(&dev->buf, (void __user *)arg, sizeof(dev->buf));
		#ifdef CEC_KERNEL_DEBUG
			printk("\nCEC Tx Data Count = %d\n",dev->buf.size);
			for(i = 0; i < dev->buf.size; i++)
			printk("CEC Tx Buffer[%d] = 0x%x \n",i,dev->buf.send_buf[i]);

			printk("\n");
		#endif
			ret = cec_ctrlSendFrame(dev, dev->buf.send_buf, dev->buf.size);
//			TccCECInterface_SendData(0, (unsigned int)dev->buf.send_buf);
		}
		break;

		case CEC_IOC_RECVDATA:
		{
		#ifdef CEC_KERNEL_DEBUG
			int i;
		#endif
			int size = 0;
			memset(dev->buf.recv_buf,0,CEC_RX_DATA_SIZE);
			size = cec_ctrlReceiveFrame(dev,dev->buf.recv_buf,CEC_RX_DATA_SIZE);

			if(size > 0)
			{
		#ifdef CEC_KERNEL_DEBUG						
				printk("\nCEC Rx Data Count = %d\n",size);
				for(i = 0; i < size; i++)
				printk("CEC Rx Buffer[%d] = 0x%x \n",i,dev->buf.recv_buf[i]);

				printk("\n");
		#endif
				ret = copy_to_user((void __user *)arg, dev->buf.recv_buf, size);

				//TccCECInterface_ParseMessage(dev, dev->buf.recv_buf, size);

				return size;						
			}
		}
		break;

		case CEC_IOC_BLANK:
		{
		#ifdef CONFIG_PM
			unsigned int data ;                       
			ret = copy_from_user(&data, (void __user *)arg, sizeof(unsigned int));

			hdmi_cec_blank(dev, data);
		#endif

		}
		break;

		case CEC_IOC_GETRESUMESTATUS:
		{
			return dev->standby_status;
		}
		break;

		case CEC_IOC_CLRRESUMESTATUS:
		break;

		case CEC_IOC_STATUS:
			ret = copy_from_user(&dev->cec_enable, (void __user *)arg, sizeof(int));
		break;

		default:
			return -EINVAL;
	}


	return ret;

}

static irqreturn_t cec_irq_handler(int irq, void *dev_id){
#if 0
	struct cec_device *dev = NULL;

	if(dev_id == NULL)
		return IRQ_NONE;

	dev = dev_id;
#endif
	printk("%s\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t cec_wake_up_irq_handler(int irq, void *dev_id){
#if 0
	struct cec_device *dev = NULL;

	if(dev_id == NULL)
		return IRQ_NONE;

	dev = dev_id;
#endif
	printk("%s\n", __func__);

	return IRQ_HANDLED;
}

int hdmi_cec_request_irq(struct cec_device *dev) {

	if (request_irq(dev->cec_irq, cec_irq_handler, IRQF_SHARED, "hdmi_cec", dev))
	{
	    printk(KERN_WARNING "CEC: IRQ %d is not free.\n", dev->cec_irq);
	    hdmi_cec_misc_deregister(dev);
	    return -1;
	}
	
	if (request_irq(dev->cec_wake_up_irq, cec_wake_up_irq_handler, IRQF_SHARED, "hdmi_wake_up_cec", dev))
	{
	    printk(KERN_WARNING "CEC: IRQ %d is not free.\n", dev->cec_wake_up_irq);
	    hdmi_cec_misc_deregister(dev);
	    return -1;
	}
	return 0;
}


static int hdmi_cec_open(struct inode *inode, struct file *file) {

        struct miscdevice *misc = (struct miscdevice *)file->private_data;
        struct cec_device *dev = dev_get_drvdata(misc->parent);
#ifdef CEC_KERNEL_DEBUG        
        printk("### %s \n", __func__);
#endif
        file->private_data = dev;        

	if(dev->clk[HDMI_CLK_CEC_INDEX_CORE]) {
		clk_set_rate(dev->clk[HDMI_CLK_CEC_INDEX_CORE], HDMI_CEC_CORE_CLK_RATE);
		clk_prepare_enable(dev->clk[HDMI_CLK_CEC_INDEX_CORE]);
	}

	if(dev->clk[HDMI_CLK_CEC_INDEX_SFR]) {
		clk_set_rate(dev->clk[HDMI_CLK_CEC_INDEX_SFR], HDMI_CEC_SFR_CLK_RATE);
		clk_prepare_enable(dev->clk[HDMI_CLK_CEC_INDEX_SFR]);
	}

	if(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS]) {
		clk_prepare_enable(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS]);
	}

        
	return 0;
}

static int hdmi_cec_release(struct inode *inode, struct file *file){
        struct cec_device *dev = (struct cec_device *)file->private_data;   
#ifdef CEC_KERNEL_DEBUG
        printk("### %s \n", __func__);
#endif
	if(dev->clk[HDMI_CLK_CEC_INDEX_CORE])
		clk_disable(dev->clk[HDMI_CLK_CEC_INDEX_CORE]);

	if(dev->clk[HDMI_CLK_CEC_INDEX_SFR])
		clk_disable(dev->clk[HDMI_CLK_CEC_INDEX_SFR]);


	if(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS])
		clk_disable(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS]);

	return 0;
}

static ssize_t hdmi_cec_read( struct file *file, char *buf, size_t count, loff_t *f_pos ){
//        struct cec_device *dev = (struct cec_device *)file->private_data;   

	return 0;
}

static ssize_t hdmi_cec_write( struct file *file, const char *buf, size_t count, loff_t *f_pos ){
//        struct cec_device *dev = (struct cec_device *)file->private_data;   

	return count;
}

static unsigned int hdmi_cec_poll(struct file *file, poll_table *wait){
        unsigned int mask = 0;
//        struct cec_device *dev = (struct cec_device *)file->private_data;   

   return mask;
}

static const struct file_operations hdmi_cec_fops =
{
        .owner          = THIS_MODULE,
        .open           = hdmi_cec_open,
        .release        = hdmi_cec_release,
        .read           = hdmi_cec_read,
        .write          = hdmi_cec_write,
        .unlocked_ioctl = hdmi_cec_ioctl,
        .poll           = hdmi_cec_poll,
};

/**
 * @short misc register routine
 * @param[in] dev pointer to the hdmi_tx_devstructure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int hdmi_cec_misc_register(struct cec_device *dev) {
        int ret = 0;
        
        dev->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
        dev->misc->minor = MISC_DYNAMIC_MINOR;
        dev->misc->name = "dw-hdmi-cec";
        dev->misc->fops = &hdmi_cec_fops;
        dev->misc->parent = dev->parent_dev;
        ret = misc_register(dev->misc);
        if(ret) {
                goto end_process;
        }
        dev_set_drvdata(dev->parent_dev, dev);

end_process:
        return ret;
}

/**
 * @short misc deregister routine
 * @param[in] dev pointer to the hdmi_tx_devstructure
 * @return 0 on success and a negative number on failure
 * Refer to Linux errors.
 */
int hdmi_cec_misc_deregister(struct cec_device *dev) {
        if(dev->misc) {
                misc_deregister(dev->misc);
                kfree(dev->misc);
                dev->misc = 0;
        }

        return 0;
}
