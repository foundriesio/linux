/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA

NOTE: Tab size is 8
****************************************************************************/
#include "include/hdmi_cec.h"
#include "include/hdmi_cec_misc.h"
#include "include/hdmi_cec_ioctl.h"

#include "hdmi_cec_lib/cec.h"
#include "hdmi_cec_lib/cec_access.h"
#include "hdmi_cec_lib/cec_reg.h"

#include<linux/clk.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

//#define CEC_KERNEL_DEBUG

#if defined(CEC_KERNEL_DEBUG)
#define dpr_info(fmt,...) pr_info(fmt, ##__VA_ARGS__))
#else
#define dpr_info
#endif

#ifdef CONFIG_PM
static int hdmi_cec_blank(struct cec_device *dev, int blank_mode)
{
	int ret = 0;

	struct device *pdev_cec = dev->parent_dev;

	printk("%s : blank(mode=%d)\n", __func__, blank_mode);

        if(pdev_cec != NULL) {
                switch(blank_mode)
                {
                        case FB_BLANK_POWERDOWN:
                        case FB_BLANK_NORMAL:
                                pm_runtime_put_sync(pdev_cec);
                                break;

                        case FB_BLANK_UNBLANK:
                                if(pdev_cec->power.usage_count.counter == 1) {
                                /*
                                 * usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK ) means that
                                 * this driver is stable state when booting. don't call runtime_suspend or resume state  */
                                } else {
                                        pm_runtime_get_sync(pdev_cec);
                                }
                                break;
                        case FB_BLANK_HSYNC_SUSPEND:
                        case FB_BLANK_VSYNC_SUSPEND:
                        default:
                                ret = -EINVAL;
                }
        }
        return ret;
}
#endif

static long hdmi_cec_ioctl(struct file *file, unsigned int cmd, unsigned long arg){

        long ret = -EINVAL;
        struct cec_device *dev = (struct cec_device *)file->private_data;

        if(dev == NULL){
                pr_err("%s:cec_device device is NULL\n", __func__);
                return ret;
        }

        switch (cmd) {
		case CEC_IOC_START:
        		{
        			ret = cec_Init(dev);
        		}
        		break;

		case CEC_IOC_STOP:
        		{
        			int wakeup;
                                if(copy_from_user(&wakeup, (void __user *)arg, sizeof(int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                mutex_lock(&dev->mutex);
                                if(!wakeup && dev->cec_wakeup_enable > 1) {
                                        pr_info("%s CEC_IOC_STOP : clean wakeup\r\n", __func__);
                                        dev->clk_enable_count = dev->reference_count;
                                        dev->cec_wakeup_enable = 0;
                                } else {
                                        dev->cec_wakeup_enable = (wakeup>0)?1:0;
                                }
                                mutex_unlock(&dev->mutex);
        			ret = cec_Disable(dev, wakeup);
        		}
        		break;

		case CEC_IOC_SETLADDR:
        		{
        			unsigned int laddr;
                                if(copy_from_user(&laddr, (void __user *)arg, sizeof(unsigned int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }

        		        #ifdef CEC_KERNEL_DEBUG
        			printk("%s: ioctl(CEC_IOC_SETLADDR)\n",__func__);
        			printk("%s: logical address = 0x%02x\n", __func__, laddr);
        		        #endif
        			dev->l_address = laddr;
        			cec_CfgLogicAddr(dev,laddr, 1);
                                ret = 0;
        		}
        		break;

		case CEC_IOC_CLEARLADDR:
        		{
        			unsigned int laddr;
                                if(copy_from_user(&laddr, (void __user *)arg, sizeof(unsigned int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
        		        #ifdef CEC_KERNEL_DEBUG
        			printk("%s: ioctl(CEC_IOC_CLEARLADDR)\n",__func__);
        			printk("%s: logical address = 0x%02x\n", __func__, laddr);
        		        #endif
        			cec_CfgLogicAddr(dev,laddr, 0);
                                ret = 0;
        		}
        		break;

		case CEC_IOC_SETPADDR:
        		{
        			unsigned int paddr;
                                if(copy_from_user(&paddr, (void __user *)arg, sizeof(unsigned int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }

        			dev->p_address = paddr;
                                ret = 0;
        		}
        		break;

		case CEC_IOC_SENDDATA:
        		{
        		        #ifdef CEC_KERNEL_DEBUG
        			int i;
        		        #endif
        			memset(dev->buf.send_buf,0,CEC_TX_DATA_SIZE);
                                if(copy_from_user(&dev->buf, (void __user *)arg, sizeof(dev->buf))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
        		        #ifdef CEC_KERNEL_DEBUG
        			printk("\nCEC Tx(%d): ",dev->buf.size);
        			for(i = 0; i < dev->buf.size; i++)
        			        printk("%02x ", dev->buf.send_buf[i]);
        			printk("\n");
        		        #endif
        			ret = cec_ctrlSendFrame(dev, dev->buf.send_buf, dev->buf.size);
        		}
        		break;

		case CEC_IOC_RECVDATA:
        		{
        		        #ifdef CEC_KERNEL_DEBUG
        			int i;
        		        #endif
        			memset(dev->buf.recv_buf,0,CEC_RX_DATA_SIZE);
        			ret = cec_ctrlReceiveFrame(dev,dev->buf.recv_buf,CEC_RX_DATA_SIZE);
        			if(ret > 0)
        			{
        		                #ifdef CEC_KERNEL_DEBUG
        				printk("\nCEC Rx(%d): ", (int)ret);
        				for(i = 0; i < ret; i++)
        				        printk("%02x ", dev->buf.recv_buf[i]);
        				printk("\n");
        		                #endif
                                        if(copy_to_user((void __user *)arg, dev->buf.recv_buf, ret)) {
                                                ret = -EFAULT;
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        break;
        			}
        		}
        		break;

		case CEC_IOC_BLANK:
        		{
        		        #ifdef CONFIG_PM
        			unsigned int data ;
                                if(copy_from_user(&data, (void __user *)arg, sizeof(unsigned int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                ret = hdmi_cec_blank(dev, data);
        		        #endif
        		}
        		break;

		case CEC_IOC_GETRESUMESTATUS:
        		{
        			ret = dev->standby_status;
        		}
        		break;

		case CEC_IOC_CLRRESUMESTATUS:
                        ret = 0;
        		break;

		case CEC_IOC_STATUS:
                        if(copy_from_user(&dev->cec_enable, (void __user *)arg, sizeof(int))) {
                                ret = -EFAULT;
                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                break;
                        }
                        ret = 0;
		        break;

                case CEC_IOC_SET_WAKEUP:
                        {
                                int wakeup;
                                if(copy_from_user(&wakeup, (void __user *)arg, sizeof(int))) {
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                mutex_lock(&dev->mutex);
                                if(!wakeup && dev->cec_wakeup_enable > 1) {
                                        pr_info("%s CEC_IOC_STOP : clean wakeup\r\n", __func__);
                                        dev->clk_enable_count = dev->reference_count;
                                        dev->cec_wakeup_enable = 0;
                                } else {
                                        dev->cec_wakeup_enable = (wakeup>0)?1:0;
                                }
                                mutex_unlock(&dev->mutex);
                                ret = 0;
                        }
                        break;

                case CEC_IOC_SENDDATA_EX:
                        {
                                #ifdef CEC_KERNEL_DEBUG
                                int i;
                                #endif
                                struct cec_transmit_data cec_tx_data;
                                memset(&cec_tx_data, 0, sizeof(struct cec_transmit_data));
                                if(copy_from_user(&cec_tx_data, (void __user *)arg, sizeof(int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                pr_info("%s CEC_IOC_SENDDATA_EX write size=%d\r\n", __func__, cec_tx_data.size );
                                if(copy_from_user(&cec_tx_data, (void __user *)arg, (cec_tx_data.size * sizeof(unsigned char )) + sizeof(int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                pr_info("%s CEC_IOC_SENDDATA_EX write size=%d\r\n", __func__, cec_tx_data.size );

                                #ifdef CEC_KERNEL_DEBUG
                                printk("\nCEC TxEx(%d): ",cec_tx_data.size);
                                for(i = 0; i < cec_tx_data.size; i++)
                                        printk("%02x ", cec_tx_data.buff[i]);
                                printk("\n");
                                #endif

                                memcpy(dev->buf.send_buf, cec_tx_data.buff, cec_tx_data.size);
                                dev->buf.size = cec_tx_data.size;
                                cec_tx_data.size = cec_ctrlSendFrame(dev, dev->buf.send_buf, dev->buf.size);
                                if(copy_to_user((void __user *)arg, &cec_tx_data.size, sizeof(int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                ret = 0;
                        }
                        break;

                case CEC_IOC_RECVDATA_EX:
        		{

        		        #ifdef CEC_KERNEL_DEBUG
        			int i;
        		        #endif
                                struct cec_transmit_data cec_rx_data;
        			memset(cec_rx_data.buff, 0, MAX_CEC_BUFFER);
        			cec_rx_data.size = cec_ctrlReceiveFrame(dev, cec_rx_data.buff, MAX_CEC_BUFFER);
        			if(cec_rx_data.size >= 0) {
        		                #ifdef CEC_KERNEL_DEBUG
                                        if(cec_rx_data.size > 0) {
                				printk("\nCEC RxEx(%d): ", cec_rx_data.size);
                				for(i = 0; i < cec_rx_data.size; i++)
                				        printk("%02x ", cec_rx_data.buff[i]);
                				printk("\n");
                                        }
        		                #endif
                                        if(copy_to_user((void __user *)arg, &cec_rx_data, (cec_rx_data.size * sizeof(unsigned char )) + sizeof(int))) {
                                                ret = -EFAULT;
                                                pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        ret = 0;
        			}
        		}
        		break;


                case CEC_IOC_GETRESUMESTATUS_EX:
        		{
                                if(copy_to_user((void __user *)arg, &dev->standby_status, sizeof(unsigned int))) {
                                        ret = -EFAULT;
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                ret = 0;
        		}
        		break;
		default:
			break;
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

	if (request_irq(dev->cec_irq, cec_irq_handler, IRQF_SHARED, "hdmi_cec", dev)) {
	    pr_err("%s: IRQ %d is not free.\n", __func__, dev->cec_irq);
	    hdmi_cec_misc_deregister(dev);
	    return -1;
	}

	if (request_irq(dev->cec_wake_up_irq, cec_wake_up_irq_handler, IRQF_SHARED, "hdmi_wake_up_cec", dev)) {
	    pr_err("%s: IRQ %d is not free.\n", __func__, dev->cec_wake_up_irq);
	    hdmi_cec_misc_deregister(dev);
	    return -1;
	}
	return 0;
}

void cec_power_on(struct cec_device *dev)
{
        if(dev !=NULL) {
                if(++dev->clk_enable_count == 1) {
                        pr_info("%s cound = %d\r\n", __func__, dev->clk_enable_count);

                        if(!IS_ERR(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS])) {
                                clk_prepare_enable(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS]);
                        }
                } else {
                        pr_info("%s is SKIP (cound is %d)\r\n", __func__, dev->clk_enable_count);
                }
        }
}

void cec_power_off(struct cec_device *dev)
{
        if(dev !=NULL) {
                if(dev->clk_enable_count == 1) {
                        if(dev->cec_wakeup_enable) {
                                dev->cec_wakeup_enable++;
                                pr_info("%s is skipped because cec_wakeup_enable(%d) is setted\r\n", __func__, dev->cec_wakeup_enable);
                        } else {
                                pr_info("%s cound = %d\r\n", __func__, dev->clk_enable_count);

                                cec_clear_wakeup(dev);

                                if(!IS_ERR(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS])) {
                                        clk_disable_unprepare(dev->clk[HDMI_CLK_CEC_INDEX_IOBUS]);
                                }

                                dev->clk_enable_count = 0;
                        }
                } else {
                        if(dev->clk_enable_count > 1) {
                                dev->clk_enable_count--;
                        }
                }
        }
}

static int hdmi_cec_open(struct inode *inode, struct file *file) {

        struct miscdevice *misc = (struct miscdevice *)file->private_data;
        struct cec_device *dev = dev_get_drvdata(misc->parent);
        #ifdef CEC_KERNEL_DEBUG
        printk("### %s \n", __func__);
        #endif
        file->private_data = dev;
        if(dev != NULL) {
                mutex_lock(&dev->mutex);
                dev->reference_count++;
                cec_power_on(dev);
                mutex_unlock(&dev->mutex);
        }
	return 0;
}

static int hdmi_cec_release(struct inode *inode, struct file *file){
        struct cec_device *dev = (struct cec_device *)file->private_data;
        #ifdef CEC_KERNEL_DEBUG
        printk("### %s \n", __func__);
        #endif
        if(dev != NULL) {
                mutex_lock(&dev->mutex);
                if(dev->reference_count > 0) {
                        cec_power_off(dev);
                        dev->reference_count--;
                }
                mutex_unlock(&dev->mutex);
        }
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
