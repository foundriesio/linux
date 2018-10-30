/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        cec.c
*  \brief       HDMI CEC controller driver
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
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include <regs-hdmi.h>
#include <regs-cec.h>
#include <cec.h>
#include <hdmi_1_4_cec.h>
#include <tcc_cec_interface.h>

#define CEC_DEBUG 0

#if CEC_DEBUG
#define DPRINTK(...) pr_info(__VA_ARGS__)
#else
#define DPRINTK(...)
#endif

#define HDMI_MSG_DEBUG 1

#define HDMI_IOCTL_DEBUG 0
#if HDMI_IOCTL_DEBUG 
#define io_debug(...) pr_info(__VA_ARGS__)
#else
#define io_debug(...)
#endif

#define SRC_VERSION                     "4.14_1.0.1" /* Driver version number */

#define CEC_MESSAGE_BROADCAST_MASK      0x0F
#define CEC_MESSAGE_BROADCAST           0x0F
#define CEC_FILTER_THRESHOLD            0x20


/**
 * @enum cec_state
 * Defines all possible states of CEC software state machine
 */
enum cec_state {
        STATE_RX,
        STATE_TX,
        STATE_DONE,
        STATE_ERROR
};


#if HDMI_MSG_DEBUG
static char msg_debug[200];
#endif

extern int hdmi_api_get_power_status(void);

static unsigned int cec_reg_read(struct tcc_hdmi_cec_dev *dev, unsigned offset){
        unsigned int val = 0;
        volatile void __iomem *hdmi_io = NULL;
        
        if(offset & 0x3){
                return val;
        }
       
        if(dev != NULL) {
                if(offset >= HDMIDP_PHYREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_CECREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_cec_io;
                        offset -= HDMIDP_CECREG(0);
                } else if(offset >= HDMIDP_I2SREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_SPDIFREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_AESREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMIREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMI_SSREG(0)) { 
                        hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
                        offset -= HDMIDP_HDMI_SSREG(0);
                }
        }

        if(hdmi_io == NULL) {
                pr_err("%s hdmi_io is NULL at offset(0x%x)\r\n", __func__, offset);
        } else {
                //pr_info(" >> Read (%p)\r\n", (void*)(hdmi_io + offset));
                val = ioread32((void*)(hdmi_io + offset));
        }
        return val;
}

static void cec_reg_write(struct tcc_hdmi_cec_dev *dev, unsigned int data, unsigned offset){
        volatile void __iomem *hdmi_io = NULL;

        if(offset & 0x3){
                return;
        }
       
        if(dev != NULL) {
                if(offset >= HDMIDP_PHYREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_CECREG(0)) {
                        hdmi_io = (volatile void __iomem *)dev->hdmi_cec_io;
                        offset -= HDMIDP_CECREG(0);
                } else if(offset >= HDMIDP_I2SREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_SPDIFREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_AESREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMIREG(0)) {
                        pr_err("%s output range at line(%d)\r\n", __func__, __LINE__);
                } else if(offset >= HDMIDP_HDMI_SSREG(0)) { 
                        hdmi_io = (volatile void __iomem *)dev->hdmi_ctrl_io;
                        offset -= HDMIDP_HDMI_SSREG(0);
                }
        }
        if(hdmi_io == NULL) {
                pr_err("%s hdmi_io is NULL at offset(0x%x)\r\n", __func__, offset);
        } else {
                //pr_info(" >> Write(%p) = 0x%x\r\n", (void*)(hdmi_io + offset), data);
                iowrite32(data, (void*)(hdmi_io + offset));
        }
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
static int cec_blank(struct tcc_hdmi_cec_dev *dev, int blank_mode)
{
        int ret = -EINVAL;
        struct device *pdev = NULL;
        
        pr_info("%s : blank(mode=%d)\n",__func__, blank_mode);
        
        if(dev != NULL) {
                pdev = dev->pdev;
        }
        
        if(pdev != NULL) {
        #ifdef CONFIG_PM
                switch(blank_mode) {
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
                                ret = 0;
                                break;
                        default:
                                ret = -EINVAL;
                                break;
                }
        #endif
        }
        return ret;
}


/**
 * Set CEC divider value.
 */
static void cec_set_divider(struct tcc_hdmi_cec_dev *dev)
{
        /**
        * (CEC_DIVISOR) * (clock cycle time) = 0.05ms \n
        * for 27MHz clock divisor is 0x0545
        */
	unsigned int source_bus_clk;
	unsigned int div = 0;

        do {
                if(dev == NULL) {
                        pr_err("%s dev is NULL\r\n", __func__);
                        break;
                }
                if(dev->ipclk == NULL) {
                        pr_err("%s ipclk is NULL\r\n", __func__);
                        break;
                }
        	source_bus_clk = clk_get_rate(dev->ipclk);
        	DPRINTK("cec src clk = %dmhz\n", source_bus_clk);
                
        	source_bus_clk = source_bus_clk/(1000*1000);
        	div = source_bus_clk * 50 - 1;

        	DPRINTK("cec divisor = %d\n", div);

        	cec_reg_write(dev, ((div>>24) & 0xff), CEC_DIVISOR_3);
        	cec_reg_write(dev, ((div>>16) & 0xff), CEC_DIVISOR_2);
        	cec_reg_write(dev, ((div>> 8) & 0xff), CEC_DIVISOR_1);
        	cec_reg_write(dev, ((div>> 0) & 0xff), CEC_DIVISOR_0);
        }while(0);
}

/**
 * Enable CEC interrupts
 */
static void cec_enable_interrupts(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, HDMI_SS_INTC_CON);
                cec_reg_write(dev, reg | (1<<HDMI_IRQ_CEC) | (1<<HDMI_IRQ_GLOBAL), HDMI_SS_INTC_CON);
        }
}

/**
 * Disable CEC interrupts
 */
static void cec_disable_interrupts(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, HDMI_SS_INTC_CON);
                cec_reg_write(dev, reg & ~(1<<HDMI_IRQ_CEC), HDMI_SS_INTC_CON);
        }
}

/**
 * Enable CEC Rx engine
 */
static void cec_enable_rx(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, CEC_RX_CTRL);
                reg |= CEC_RX_CTRL_ENABLE;
                reg |= CEC_RX_CTRL_CHK_SAMPLING_ERROR;
                reg |= CEC_RX_CTRL_CHK_LOW_TIME_ERROR;
                reg |= CEC_RX_CTRL_CHK_START_BIT_ERROR;
                cec_reg_write(dev, reg, CEC_RX_CTRL);

                // mandatory: enable timing monitor
                // inter-bit time error limitation enable.
                cec_reg_write(dev, 0x0B, CEC_RX_CTRL1);
        }
}

/**
 * Mask CEC Rx interrupts
 */
static void cec_mask_rx_interrupts(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, CEC_IRQ_MASK);
                reg |= CEC_IRQ_RX_DONE;
                reg |= CEC_IRQ_RX_ERROR;
                cec_reg_write(dev, reg, CEC_IRQ_MASK);
        }
}

/**
 * Unmask CEC Rx interrupts
 */
static void cec_unmask_rx_interrupts(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, CEC_IRQ_MASK);
                reg &= ~CEC_IRQ_RX_DONE;
                reg &= ~CEC_IRQ_RX_ERROR;
                cec_reg_write(dev, reg, CEC_IRQ_MASK);
        }
}

/**
 * Mask CEC Tx interrupts
 */
static void cec_mask_tx_interrupts(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, CEC_IRQ_MASK);
                reg |= CEC_IRQ_TX_DONE;
                reg |= CEC_IRQ_TX_ERROR;
                cec_reg_write(dev, reg, CEC_IRQ_MASK);
        }
}

/**
 * Unmask CEC Tx interrupts
 */
static void cec_unmask_tx_interrupts(struct tcc_hdmi_cec_dev *dev)
{
        unsigned int reg;
        if(dev != NULL) {
                reg = cec_reg_read(dev, CEC_IRQ_MASK);
                reg &= ~CEC_IRQ_TX_DONE;
                reg &= ~CEC_IRQ_TX_ERROR;
                cec_reg_write(dev, reg, CEC_IRQ_MASK);
        }
}

/**
 * Change CEC Tx state to state
 * @param state [in] new CEC Tx state.
 */
static void cec_set_tx_state(struct tcc_hdmi_cec_dev *dev, enum cec_state state)
{
        if(dev != NULL) {
                atomic_set(&dev->tx.state, state);
        }
}

/**
 * Change CEC Rx state to @c state.
 * @param state [in] new CEC Rx state.
 */
static void cec_set_rx_state(struct tcc_hdmi_cec_dev *dev, enum cec_state state)
{
        if(dev != NULL) {
                atomic_set(&dev->rx.state, state);
        }
}
static void cec_start(struct tcc_hdmi_cec_dev *dev)
{
        DPRINTK( "%s\n", __func__);

        if(dev != NULL) { 
                cec_reg_write(dev, CEC_RX_CTRL_RESET, CEC_RX_CTRL); // reset CEC Rx
                cec_reg_write(dev, CEC_TX_CTRL_RESET, CEC_TX_CTRL); // reset CEC Tx

                cec_set_divider(dev);

                //enable filter
                cec_reg_write(dev, CEC_FILTER_THRESHOLD, CEC_RX_FILTER_TH); // setup filter
                cec_reg_write(dev, CEC_FILTER_EN, CEC_RX_FILTER_CTRL);

                cec_enable_interrupts(dev);

                cec_unmask_tx_interrupts(dev);

                cec_set_rx_state(dev, STATE_RX);
                cec_unmask_rx_interrupts(dev);
                cec_enable_rx(dev);

                dev->rx.flag = 0;
        }
}

static void cec_stop(struct tcc_hdmi_cec_dev *dev)
{
        u32 status;
        unsigned int wait_cnt = 0;

        DPRINTK( "%s\n", __func__);
        if(dev != NULL) {
                do {
                        status = cec_reg_read(dev, CEC_STATUS_0);
                        status |= cec_reg_read(dev, CEC_STATUS_1) << 8;
                        status |= cec_reg_read(dev, CEC_STATUS_2) << 16;
                        status |= cec_reg_read(dev, CEC_STATUS_3) << 24;

                        udelay(1000);
                        wait_cnt++;
                }while(status & CEC_STATUS_TX_RUNNING);

                cec_mask_tx_interrupts(dev);
                cec_mask_rx_interrupts(dev);
                cec_disable_interrupts(dev);
        }
        printk("%s : wait_cnt = %d\n", __func__, wait_cnt);

}

/**
 * @brief CEC interrupt handler
 *
 * Handles interrupt requests from CEC hardware. \n
 * Action depends on current state of CEC hardware.
 */
static irqreturn_t cec_irq_handler(int irq, void *dev_id)
{
        unsigned int flag, val, status;
    
        #if (CEC_DEBUG)
        unsigned int tx_stat, rx_stat;
        #endif

        struct tcc_hdmi_cec_dev *dev =  (struct tcc_hdmi_cec_dev *)dev_id;

        if(dev != NULL) {
                /* read flag register */
                flag = cec_reg_read(dev, HDMI_SS_INTC_FLAG);

                DPRINTK( "%s flag = 0x%08x\n", __func__, flag);

                /* is this our interrupt? */
                if (!(flag & (1<<HDMI_IRQ_CEC))) {
                        return IRQ_NONE;
                }

                val = cec_reg_read(dev, CEC_STATUS_0);
                status = val & 0xFF;
                val = cec_reg_read(dev, CEC_STATUS_1);
                status |= ((val & 0xFF) << 8);
                val = cec_reg_read(dev, CEC_STATUS_2);
                status |= ((val & 0xFF) << 16);
                val = cec_reg_read(dev, CEC_STATUS_3);
                status |= ((val & 0xFF) << 24);

                #if (CEC_DEBUG)
                val = cec_reg_read(dev, CEC_TX_STAT0);
                tx_stat = val & 0xFF;
                val = cec_reg_read(dev, CEC_TX_STAT1);
                tx_stat |= ((val & 0xFF) << 8);

                //      DPRINTK( "CEC: status = 0x%x!\n", status);
                DPRINTK( "CEC: status = 0x%x! [0x%x] \n", status, tx_stat);
                #endif

                if (status & CEC_STATUS_TX_DONE) {
                        if (status & CEC_STATUS_TX_ERROR) {
                                DPRINTK( "CEC: CEC_STATUS_TX_ERROR!\n");
                                cec_reg_write(dev, CEC_TX_CTRL_RESET, CEC_TX_CTRL); // reset CEC Tx

                                cec_set_tx_state(dev, STATE_ERROR);
                        } else {
                                DPRINTK( "CEC: CEC_STATUS_TX_DONE!\n");
                                cec_set_tx_state(dev, STATE_DONE);
                        }
                        /* clear interrupt pending bit */
                        cec_reg_write(dev, CEC_IRQ_TX_DONE | CEC_IRQ_TX_ERROR, CEC_IRQ_CLEAR);
                        wake_up_interruptible(&dev->tx.waitq);
                }


                if (status & CEC_STATUS_RX_ERROR) {
                        if (status & CEC_STATUS_RX_DONE) {
                                DPRINTK( "CEC: CEC_STATUS_RX_DONE!\n");
                        }
                        DPRINTK( "CEC: CEC_STATUS_RX_ERROR!\n");

                        #if (CEC_DEBUG)
                        val = cec_reg_read(dev, CEC_RX_STAT0);
                        rx_stat = val & 0xFF;
                        val = cec_reg_read(dev, CEC_RX_STAT1);
                        rx_stat |= ((val & 0xFF) << 8);

                        DPRINTK( "CEC: rx_status = 0x%x\n", rx_stat);
                        #endif

                        cec_set_rx_state(dev, STATE_ERROR);

                        /* clear interrupt pending bit */
                        cec_reg_write(dev, CEC_IRQ_RX_ERROR, CEC_IRQ_CLEAR);
                }
                
                if (status & CEC_STATUS_RX_DONE) {
                        unsigned int size, i = 0;

                        DPRINTK( "CEC: CEC_STATUS_RX_DONE!\n");

                        /* copy data from internal buffer */
                        size = status >> 24;

                        spin_lock(&dev->rx.lock);

                        while (i < size) {
                                dev->rx.buffer[i] = cec_reg_read(dev, CEC_RX_BUFF0 + (i*4));
                                i++;
                        }

                        if (size > 0) {
                                DPRINTK( "fsize: %d ", size);
                                DPRINTK( "frame: ");
                                for (i = 0; i < size; i++) {
                                        DPRINTK( "0x%02x ", dev->rx.buffer[i]);
                                }
                                DPRINTK( "End ~\n");
                        }

                        TccCECInterface_ParseMessage(dev, size);

                        dev->rx.size = size;
                        cec_set_rx_state(dev, STATE_DONE);
                        dev->rx.flag = 1;

                        spin_unlock(&dev->rx.lock);

                        cec_enable_rx(dev);

                        /* clear interrupt pending bit */
                        cec_reg_write(dev, CEC_IRQ_RX_DONE | CEC_IRQ_RX_ERROR, CEC_IRQ_CLEAR);
                        wake_up_interruptible(&dev->rx.waitq);
                }

                
        }
        return IRQ_HANDLED;
}


static int cec_open(struct inode *inode, struct file *file)
{

        int ret = -1;
        
        struct miscdevice *misc = (struct miscdevice *)(file!=NULL)?file->private_data:NULL;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(misc!=NULL)?dev_get_drvdata(misc->parent):NULL;
        if(dev != NULL) {
                file->private_data = dev;  
        
                if(dev->hclk != NULL)
                        clk_prepare_enable(dev->hclk);
                if(dev->pclk != NULL)
                        clk_prepare_enable(dev->pclk);
                ret = 0;
        }
        
        return ret;
}

int cec_release(struct inode *inode, struct file *file)
{
        int ret = -1;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file!=NULL)?file->private_data:NULL;

        if(dev != NULL) {
                if(dev->pclk != NULL)
                        clk_disable_unprepare(dev->pclk);
                if(dev->hclk != NULL)
                        clk_disable_unprepare(dev->hclk);
                ret = 0;
        }

        return ret;
}
        
ssize_t cec_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
        ssize_t ret = -1;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file !=NULL)?file->private_data:NULL;

        do {
                if(dev == NULL) {
                        pr_err("%s parameter is null at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                if (atomic_read(&dev->rx.state) == STATE_ERROR) {
                        pr_err( "%s end, because of RX error\n", __func__);
                        break;
                }

                if( dev->rx.flag != 1) {
                        ret = 0;
                        break;
                }

                spin_lock_irq(&dev->rx.lock);

                if (dev->rx.size > count) {
                        spin_unlock_irq(&dev->rx.lock);
                        break;
                }

                if (copy_to_user(buffer, dev->rx.buffer, dev->rx.size)) {
                        spin_unlock_irq(&dev->rx.lock);
                        pr_err("CEC: copy_to_user() failed!\n");
                        break;
                }

                ret = dev->rx.size;
                cec_set_rx_state(dev, STATE_RX);
                spin_unlock_irq(&dev->rx.lock);

                dev->rx.flag = 0;

                #if HDMI_MSG_DEBUG
                {
                        int loop, len;
                        len = sprintf(msg_debug, "\r\nR: ");
                        for(loop = 0; loop < dev->rx.size; loop++) {
                                len+= sprintf(msg_debug+len, "[%02x] ", dev->rx.buffer[loop]);
                        }
                        msg_debug[len] = 0;
                        pr_info("%s\r\n", msg_debug);
                }
                #endif

                DPRINTK( "%s end\n", __func__);
        } while(0);

        return ret;
}

ssize_t cec_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
        int i;
        unsigned int reg;

        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file !=NULL)?file->private_data:NULL;

        do {
                if(dev == NULL) {
                        pr_err("%s parameter is null at line(%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
                }

                /* check data size */
                if (count > CEC_TX_BUFF_SIZE || count == 0) {
                        pr_err("%s count is inavlid at line(%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
                }

                if (copy_from_user(dev->tx.buffer, buffer, count))
                {
                        pr_err("%s copy_from_user is failed at line(%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
                }

                #if HDMI_MSG_DEBUG
                {
                        int loop, len;
                        len = sprintf(msg_debug, "\r\nW: ");
                        for(loop = 0; loop < count; loop++) {
                                len += sprintf(msg_debug+len, "[%02x] ", dev->tx.buffer[loop]);
                        }
                        msg_debug[len] = 0;
                        pr_info("%s\r\n", msg_debug);
                }
                #endif

                /* clear interrupt pending bit */
                reg = CEC_IRQ_TX_DONE | CEC_IRQ_TX_ERROR;
                cec_reg_write(dev, reg, CEC_IRQ_CLEAR);
                
                /* copy packet to hardware buffer */
                for(i = 0; i < count; i++) {
                        reg = (unsigned int)(dev->tx.buffer[i] & 0xFF);
                        cec_reg_write(dev, reg, CEC_TX_BUFF0 + (i << 2));
                }

                /* set number of bytes to transfer */
                reg = (unsigned int)count;
                cec_reg_write(dev, reg, CEC_TX_BYTES);

                cec_set_tx_state(dev, STATE_TX);

                /* start transfer */
                reg = cec_reg_read(dev, CEC_TX_CTRL);
                reg |= CEC_TX_CTRL_START;
                
                /* if message is broadcast message - set corresponding bit */
                if ((dev->tx.buffer[0] & CEC_MESSAGE_BROADCAST_MASK) == CEC_MESSAGE_BROADCAST)
                        reg |= CEC_TX_CTRL_BCAST;
                else
                        reg &= ~CEC_TX_CTRL_BCAST;

                /* set number of retransmissions */
                reg &= ~(7 << 4);
                reg |= (5 << 4); 

                cec_reg_write(dev, reg, CEC_TX_CTRL);

                /* wait for interrupt */
                if (0 == wait_event_interruptible_timeout(dev->tx.waitq, atomic_read(&dev->tx.state) != STATE_TX, msecs_to_jiffies(10))) {
                        //pr_err("%s wait failed at line (%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
                }

                if (atomic_read(&dev->tx.state) == STATE_ERROR) {
                        //pr_err( "%s end, because of TX error\n", __func__);
                        count = 0;
                        break;
                }

                DPRINTK( "%s end\n", __func__);
        } while(0);
        return count;
}


long  cec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
                        
        long ret = -EINVAL;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file != NULL)?file->private_data:NULL;

        if(dev != NULL) {
                switch (cmd) {
                        case CEC_IOC_START:
                                io_debug("CEC: ioctl(CEC_IOC_START)\r\n");
                                cec_start(dev);
                                ret = 0;
                                break;
                                
                        case CEC_IOC_STOP:
                                io_debug("CEC: ioctl(CEC_IOC_STOP)\r\n");
                                cec_stop(dev);
                                ret = 0;
                                break;
                                
                        case CEC_IOC_SETLADDR:
                                {
                                        unsigned int laddr;
                                        io_debug( "CEC: ioctl(CEC_IOC_SETLADDR)\n");
                                
                                        if(copy_from_user(&laddr, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

                                        io_debug( "CEC: logical address = 0x%02x\n", laddr);
                                        cec_reg_write(dev, laddr & 0x0F, CEC_LOGIC_ADDR);
                                        ret = 0;
                                }       
                                break;
                                
                        case CEC_IOC_SENDDATA:
                                {
                                        /* It will be deprecated */
                                        #if 0
                                        unsigned int uiData;
                                        io_debug( "CEC: ioctl(CEC_IOC_SENDDATA)\n");
                                        if(copy_from_user(&uiData, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        DPRINTK( "CEC: SendData = 0x%02x\n", uiData);
                                        ret = TccCECInterface_SendData(dev, 0, uiData);
                                        #endif
                                }
                                break;
                                
                        case CEC_IOC_RECVDATA:
                                /* It will be deprecated */
                                io_debug("CEC: ioctl(CEC_IOC_RECVDATA)\r\n");
                                ret = 0;
                                break;

                        case CEC_IOC_GETRESUMESTATUS:
                                io_debug( "CEC: ioctl(CEC_IOC_GETRESUMESTATUS)\n");
                                if(copy_to_user((void __user *)arg, &dev->resume, sizeof(unsigned int))) {
                                        pr_err("%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                ret = 0;
                                break;
                        case CEC_IOC_CLRRESUMESTATUS:
                                {
                                        unsigned int resume_status;
                                        io_debug( "CEC: ioctl(CEC_IOC_SENDDATA)\n");
                                        if(copy_from_user(&resume_status, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        dev->resume = (resume_status > 0)?1:0;
                                        ret = 0;
                                }
                                break;
                        case CEC_IOC_BLANK:
                                {
                                        unsigned int cmd;
                                        io_debug("CEC: ioctl(CEC_IOC_BLANK)\r\n");
                                        if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                      
                                        ret = cec_blank(dev, cmd);
                                }
                                break;
                        default:
                                break;
                  }
        }
        return ret;
}

static unsigned int cec_poll(struct file *file, poll_table *wait)
{
        unsigned int mask = 0;
        struct tcc_hdmi_cec_dev *dev = NULL;
        if(file != NULL) {
                dev = (struct tcc_hdmi_cec_dev *)file->private_data; 
        }

        if(dev != NULL) {
                poll_wait(file, &dev->rx.waitq, wait);

                if (atomic_read(&dev->rx.state) == STATE_DONE) {
                        mask = POLLIN | POLLRDNORM;
                }
        }
        
    return mask;
}

static const struct file_operations cec_fops =
{
    .owner          = THIS_MODULE,
    .open           = cec_open,
    .release        = cec_release,
    .read           = cec_read,
    .write          = cec_write,
    .unlocked_ioctl = cec_ioctl,
    .poll           = cec_poll,
};


#ifdef CONFIG_PM
static int cec_suspend(struct device *dev)
{
        struct tcc_hdmi_cec_dev *tcc_hdmi_cec_dev;
        
        pr_info("### %s \n", __func__);
        
        if(dev != NULL) {
                tcc_hdmi_cec_dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(dev);
                if(tcc_hdmi_cec_dev != NULL) {
                        if(!tcc_hdmi_cec_dev->suspend) {
                                if(hdmi_api_get_power_status()) {
                                        cec_stop(tcc_hdmi_cec_dev);
                                }
                                tcc_hdmi_cec_dev->suspend = 1;
                        }
                        tcc_hdmi_cec_dev->resume = 0;
                }
        }

        return 0;
}

static int cec_resume(struct device *dev)
{
        struct tcc_hdmi_cec_dev *tcc_hdmi_cec_dev;
        
        pr_info("### %s \n", __func__);
        
        if(dev != NULL) {
                tcc_hdmi_cec_dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(dev);
                if(tcc_hdmi_cec_dev != NULL) {
                        if(tcc_hdmi_cec_dev->suspend  && !tcc_hdmi_cec_dev->runtime_suspend) {
                                if(hdmi_api_get_power_status()) {
                                        cec_start(tcc_hdmi_cec_dev);
                                }
                                tcc_hdmi_cec_dev->suspend = 0;
                                tcc_hdmi_cec_dev->resume = 1;
                        }
                }
        }
        return 0;

}

static int cec_runtime_suspend(struct device *dev) 
{
        int ret = 0;
        struct tcc_hdmi_cec_dev * tcc_hdmi_cec_dev;
        if(dev != NULL) {
                tcc_hdmi_cec_dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(dev);
                if(tcc_hdmi_cec_dev != NULL) {
                        tcc_hdmi_cec_dev->runtime_suspend = 1;
                }
                ret = cec_suspend(dev);
        }

        return ret;
}

static int cec_runtime_resume(struct device *dev)
{
        int ret = 0;
        struct tcc_hdmi_cec_dev * tcc_hdmi_cec_dev;
        if(dev != NULL) {
                tcc_hdmi_cec_dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(dev);
                if(tcc_hdmi_cec_dev != NULL) {
                        tcc_hdmi_cec_dev->runtime_suspend = 0;
                }
                ret = cec_resume(dev);
        }
        return ret;
}
#endif

static int cec_remove(struct platform_device *pdev)
{
        struct tcc_hdmi_cec_dev *dev = NULL;
                                
        if(pdev != NULL) {
                dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(pdev->dev.parent);

                if(dev != NULL) {
                        devm_free_irq(dev->pdev, dev->cec_irq, &dev);
                        if(dev->misc != NULL) {
                                misc_deregister(dev->misc);
                                devm_kfree(dev->pdev, dev->misc);
                        }
                        if(dev->rx.buffer != NULL) {
                                devm_kfree(dev->pdev, dev->rx.buffer);
                        }
			if(dev->tx.buffer != NULL) {
                                devm_kfree(dev->pdev, dev->tx.buffer);
                        }
                        devm_kfree(dev->pdev, dev);
                }
        }
        return 0;
}

static int cec_probe(struct platform_device *pdev)
{
        int ret = -1;
        struct tcc_hdmi_cec_dev *dev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hdmi_cec_dev), GFP_KERNEL);        
        do {
                if (dev == NULL) {
                    ret = -ENOMEM;
                    break;
                }
                dev->pdev = &pdev->dev;
                
                dev->pclk = of_clk_get(pdev->dev.of_node, 0);
                if (IS_ERR_OR_NULL(dev->pclk)) {
                    pr_err("HDMI: failed to get hdmi hclk\n");
                    dev->pclk = NULL;
                    break;
                }
                dev->hclk = of_clk_get(pdev->dev.of_node, 1);
                if (IS_ERR_OR_NULL(dev->hclk)) {
                    pr_err("HDMI: failed to get hdmi hclk\n");
                    dev->hclk = NULL;
                    break;
                }
                dev->ipclk = of_clk_get(pdev->dev.of_node, 2);
                if (IS_ERR_OR_NULL(dev->ipclk)) {
                    pr_err("HDMI: failed to get hdmi ipclk\n");
                    dev->ipclk = NULL;
                    break;
                }
                clk_set_rate(dev->ipclk, HDMI_LINK_CLK_FREQ);
                clk_prepare_enable(dev->pclk);
                
                clk_prepare_enable(dev->hclk);
                clk_set_rate(dev->ipclk, HDMI_PCLK_FREQ);
                clk_prepare_enable(dev->ipclk);

                dev->cec_irq = of_irq_to_resource(pdev->dev.of_node, 0, NULL);
                if (dev->cec_irq < 0) {
                        pr_err("%s failed get irq for hdmi cec\r\n", __func__);
                        break;
                }

                dev->hdmi_ctrl_io = of_iomap(pdev->dev.of_node, 0);
                if(dev->hdmi_ctrl_io == NULL){
                        pr_err("%s:Unable to map hdmi ctrl resource at line(%d)\n", __func__, __LINE__);
                        break;
                }
                dev->hdmi_cec_io = of_iomap(pdev->dev.of_node, 1);
                if(dev->hdmi_cec_io == NULL){
                        pr_err("%s:Unable to map hdmi ctrl resource at line(%d)\n", __func__, __LINE__);
                        break;
                }

                dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL); 
                if(dev->misc == NULL) {
                        pr_err("%s:Unable to createe hdmi misc at line(%d)\n", __func__, __LINE__);
                        ret = -ENOMEM;
                        break;
                }
                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "CEC";
                dev->misc->fops = &cec_fops;
                dev->misc->parent = dev->pdev;
                ret = misc_register(dev->misc);
                if(ret < 0) {
                        pr_err("%s failed misc_register for hdmi cec\r\n", __func__);
                        break;
                }
                dev_set_drvdata(dev->pdev, dev);

                pr_info("%s:HDMI CEC driver %s\n", __func__, SRC_VERSION);

                init_waitqueue_head(&dev->tx.waitq);
                init_waitqueue_head(&dev->rx.waitq);
                spin_lock_init(&dev->rx.lock);
                                
                cec_disable_interrupts(dev);
                ret = devm_request_irq(dev->pdev, dev->cec_irq, cec_irq_handler, IRQF_SHARED, "hdmi-cec", dev);
                if(ret < 0) {
                        pr_err("%s failed request interrupt for cec\r\n", __func__);
                }
                
                dev->rx.buffer = devm_kmalloc(&pdev->dev, CEC_RX_BUFF_SIZE, GFP_KERNEL);
                if (dev->rx.buffer == NULL) {
                        pr_err("%s failed kmalloc \r\n", __func__);
                        ret = -1;
                        break;
                }
                dev->rx.size   = 0;

                dev->tx.buffer = devm_kmalloc(&pdev->dev, CEC_TX_BUFF_SIZE, GFP_KERNEL);
                if (dev->tx.buffer == NULL) {
                        pr_err("%s failed kmalloc \r\n", __func__);
                        ret = -1;
                        break;
                }
                TccCECInterface_Init(dev);

                #ifdef CONFIG_PM
                pm_runtime_set_active(dev->pdev);        
                pm_runtime_enable(dev->pdev);  
                pm_runtime_get_noresume(dev->pdev);  //increase usage_count 
                #endif

                /* disable the clocks */
                clk_disable(dev->pclk);
                clk_disable(dev->hclk);
        } while(0);
        if(ret < 0) {
                cec_remove(pdev);
        }
        return ret;
}



static struct of_device_id cec_of_match[] = {
	{ .compatible = "telechips,tcc897x-hdmi-cec" },
	{}
};
MODULE_DEVICE_TABLE(of, cec_of_match);

#ifdef CONFIG_PM
static const struct dev_pm_ops cec_pm_ops = {
	.runtime_suspend      	= cec_runtime_suspend,
	.runtime_resume       	= cec_runtime_resume,
	.suspend		= cec_suspend,
	.resume 		= cec_resume,
};
#endif

static struct platform_driver tcc_hdmi_cec = {
	.probe	= cec_probe,
	.remove	= cec_remove,
	.driver	= {
		.name	= "tcc_hdmi_cec",
		.owner	= THIS_MODULE,
                #ifdef CONFIG_PM
                .pm		= &cec_pm_ops,
                #endif
		.of_match_table = of_match_ptr(cec_of_match),
	},
};


static __init int cec_init(void)
{
	return platform_driver_register(&tcc_hdmi_cec);
}

static __exit void cec_exit(void)
{
	platform_driver_unregister(&tcc_hdmi_cec);
}


module_init(cec_init);
module_exit(cec_exit);

MODULE_DESCRIPTION("TCCxxx hdmi cec driver");
MODULE_LICENSE("GPL");


