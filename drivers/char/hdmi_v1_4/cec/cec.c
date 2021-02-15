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

@note Tab size is 8
****************************************************************************/
#define pr_fmt(fmt) "\x1b[1;38m CEC: \x1b[0m" fmt

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

#define HDMI_CEC_DEBUG 0
#define dpr_info(msg, ...) if(HDMI_CEC_DEBUG) { pr_info("[INFO][HDMI_V14] " msg, ##__VA_ARGS__); }

#define HDMI_MSG_DEBUG 0
#define HDMI_IOCTL_DEBUG 0
#define dpr_io_info(msg, ...) if(HDMI_IOCTL_DEBUG) { pr_info("[INFO][HDMI_V14] "msg, ##__VA_ARGS__); }

#define SRC_VERSION                     "4.4_1.0.2b" /* Driver version number */

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

static unsigned int cec_reg_read(struct tcc_hdmi_cec_dev *dev, unsigned offset){
        unsigned int val = 0;
        volatile void __iomem *hdmi_io = NULL;

	do {
		if(offset & 0x3){
                	break;
		}

		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		if(dev->enable_cnt == 0) {
			//pr_err("[ERROR][HDMI_V14]%s cec is not started(%d)\r\n", __func__, __LINE__);
	                break;
		}

		if(offset >= HDMIDP_PHYREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_CECREG(0)) {
			hdmi_io = (volatile void __iomem *)dev->hdmi_cec_io;
			offset -= HDMIDP_CECREG(0);
		} else if(offset >= HDMIDP_I2SREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_SPDIFREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_AESREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_HDMIREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_HDMI_SSREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		}

		if(hdmi_io == NULL) {
			pr_err("[ERROR][HDMI_V14]%s hdmi_io is NULL at offset(0x%x)\r\n", __func__, offset);
			break;
		}

                //pr_info("[INFO][HDMI_V14] >> Read (%p)\r\n", (void*)(hdmi_io + offset));
                val = ioread32((void*)(hdmi_io + offset));
	}while(0);
        return val;
}

static void cec_reg_write(struct tcc_hdmi_cec_dev *dev, unsigned int data, unsigned offset){
        volatile void __iomem *hdmi_io = NULL;

	do {
		if(offset & 0x3){
                	break;
		}

		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}

		if(dev->enable_cnt == 0) {
			//pr_err("[ERROR][HDMI_V14]%s cec is not started(%d)\r\n", __func__, __LINE__);
	                break;
		}

		if(offset >= HDMIDP_PHYREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_CECREG(0)) {
			hdmi_io = (volatile void __iomem *)dev->hdmi_cec_io;
			offset -= HDMIDP_CECREG(0);
		} else if(offset >= HDMIDP_I2SREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_SPDIFREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_AESREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_HDMIREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		} else if(offset >= HDMIDP_HDMI_SSREG(0)) {
			pr_err("[ERROR][HDMI_V14]%s output range at line(%d)\r\n", __func__, __LINE__);
		}

		if(hdmi_io == NULL) {
			pr_err("[ERROR][HDMI_V14]%s hdmi_io is NULL at offset(0x%x)\r\n", __func__, offset);
			break;
		}

                //pr_info("[INFO][HDMI_V14] >> Write(%p) = 0x%x\r\n", (void*)(hdmi_io + offset), data);
                iowrite32(data, (void*)(hdmi_io + offset));
        }while(0);
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

        pr_info("[INFO][HDMI_V14]%s : blank(mode=%d)\n",__func__, blank_mode);

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
                        pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
                        break;
                }
                if(dev->pclk == NULL) {
                        pr_err("[ERROR][HDMI_V14]%s pclk is NULL\r\n", __func__);
                        break;
                }
        	source_bus_clk = clk_get_rate(dev->pclk);
        	dpr_info("cec src clk = %dmhz\n", source_bus_clk);

        	source_bus_clk = source_bus_clk/(1000*1000);
        	div = source_bus_clk * 50 - 1;

        	dpr_info("cec divisor = %d\n", div);

        	cec_reg_write(dev, ((div>>24) & 0xff), CEC_DIVISOR_3);
        	cec_reg_write(dev, ((div>>16) & 0xff), CEC_DIVISOR_2);
        	cec_reg_write(dev, ((div>> 8) & 0xff), CEC_DIVISOR_1);
        	cec_reg_write(dev, ((div>> 0) & 0xff), CEC_DIVISOR_0);
        }while(0);
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
static int cec_start(struct tcc_hdmi_cec_dev *dev)
{
	int ret = -1;

        dpr_info( "%s\n", __func__);

	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}
		if(IS_ERR_OR_NULL(dev->pclk)) {
			pr_err("[ERROR][HDMI_V14]%s cec clock is NULL\r\n", __func__);
			break;
		}

		if(++dev->enable_cnt == 1) {
			if(dev->suspend) {
				pr_info("[INFO][HDMI_V14]%s cec is suspend .. skip \r\n", __func__);
				break;
			}
			dpr_info("%s start..!!\r\n", __func__);
			clk_set_rate(dev->pclk, dev->pclk_freq);
        		clk_prepare_enable(dev->pclk);

	                cec_reg_write(dev, CEC_RX_CTRL_RESET, CEC_RX_CTRL); // reset CEC Rx
	                cec_reg_write(dev, CEC_TX_CTRL_RESET, CEC_TX_CTRL); // reset CEC Tx

	                cec_set_divider(dev);

	                //enable filter
	                cec_reg_write(dev, CEC_FILTER_THRESHOLD, CEC_RX_FILTER_TH); // setup filter
	                cec_reg_write(dev, CEC_FILTER_EN, CEC_RX_FILTER_CTRL);

	                cec_unmask_tx_interrupts(dev);

	                cec_set_rx_state(dev, STATE_RX);
	                cec_unmask_rx_interrupts(dev);
	                cec_enable_rx(dev);

	                dev->rx.flag = 0;
		}
		ret = 0;
        }while(0);

	return ret;
}

static int cec_stop(struct tcc_hdmi_cec_dev *dev)
{
	int ret = -1;
        unsigned int status;
        unsigned int wait_cnt = 0;

        dpr_info( "%s\n", __func__);
	do {
		if(dev == NULL) {
			pr_err("[ERROR][HDMI_V14]%s dev is NULL\r\n", __func__);
			break;
		}
		if(IS_ERR_OR_NULL(dev->pclk)) {
			pr_err("[ERROR][HDMI_V14]%s cec clock is NULL\r\n", __func__);
			break;
		}

		if(dev->enable_cnt == 1) {
			do {
	                        status = cec_reg_read(dev, CEC_STATUS_0);
	                        status |= cec_reg_read(dev, CEC_STATUS_1) << 8;
	                        status |= cec_reg_read(dev, CEC_STATUS_2) << 16;
	                        status |= cec_reg_read(dev, CEC_STATUS_3) << 24;

	                        udelay(1000);
	                        wait_cnt++;
	                }while(status & CEC_STATUS_TX_RUNNING);

			dpr_info("%s : wait_cnt = %d\n", __func__, wait_cnt);
	                cec_mask_tx_interrupts(dev);
	                cec_mask_rx_interrupts(dev);

			clk_disable_unprepare(dev->pclk);
			dev->enable_cnt = 0;
		} else {
			if(dev->enable_cnt > 1) {
				dev->enable_cnt--;
			}
		}
		ret = 0;
	} while(0);
	return ret;
}

/**
 * @brief CEC interrupt handler
 *
 * Handles interrupt requests from CEC hardware. \n
 * Action depends on current state of CEC hardware.
 */
static irqreturn_t cec_irq_handler(int irq, void *dev_id)
{
        unsigned int val, status;

        #if (HDMI_CEC_DEBUG)
        unsigned int tx_stat, rx_stat;
        #endif

        struct tcc_hdmi_cec_dev *dev =  (struct tcc_hdmi_cec_dev *)dev_id;

        if(dev != NULL) {
                val = cec_reg_read(dev, CEC_STATUS_0);
                status = val & 0xFF;
                val = cec_reg_read(dev, CEC_STATUS_1);
                status |= ((val & 0xFF) << 8);
                val = cec_reg_read(dev, CEC_STATUS_2);
                status |= ((val & 0xFF) << 16);
                val = cec_reg_read(dev, CEC_STATUS_3);
                status |= ((val & 0xFF) << 24);

                #if (HDMI_CEC_DEBUG)
                val = cec_reg_read(dev, CEC_TX_STAT0);
                tx_stat = val & 0xFF;
                val = cec_reg_read(dev, CEC_TX_STAT1);
                tx_stat |= ((val & 0xFF) << 8);

                dpr_info( "IRQ status = 0x%x! [0x%x] \n", status, tx_stat);
                #endif

                if (status & CEC_STATUS_TX_DONE) {
                        if (status & CEC_STATUS_TX_ERROR) {
                                dpr_info( "IRQ CEC_STATUS_TX_ERROR!\n");
                                cec_reg_write(dev, CEC_TX_CTRL_RESET, CEC_TX_CTRL); // reset CEC Tx

                                cec_set_tx_state(dev, STATE_ERROR);
                        } else {
                                dpr_info( "IRQ CEC_STATUS_TX_DONE!\n");
                                cec_set_tx_state(dev, STATE_DONE);
                        }
                        /* clear interrupt pending bit */
                        cec_reg_write(dev, CEC_IRQ_TX_DONE | CEC_IRQ_TX_ERROR, CEC_IRQ_CLEAR);
                        wake_up_interruptible(&dev->tx.waitq);
                }


                if (status & CEC_STATUS_RX_ERROR) {
                        if (status & CEC_STATUS_RX_DONE) {
                                dpr_info( "IRQ CEC_STATUS_RX_DONE!\n");
                        }
                        dpr_info( "IRQ CEC_STATUS_RX_ERROR!\n");

                        #if (HDMI_CEC_DEBUG)
                        val = cec_reg_read(dev, CEC_RX_STAT0);
                        rx_stat = val & 0xFF;
                        val = cec_reg_read(dev, CEC_RX_STAT1);
                        rx_stat |= ((val & 0xFF) << 8);

                        dpr_info( "IRQ rx_status = 0x%x\n", rx_stat);
                        #endif

                        cec_set_rx_state(dev, STATE_ERROR);

                        /* clear interrupt pending bit */
                        cec_reg_write(dev, CEC_IRQ_RX_ERROR, CEC_IRQ_CLEAR);
                }

                if (status & CEC_STATUS_RX_DONE) {
                        unsigned int size, i = 0;

                        dpr_info( "IRQ CEC_STATUS_RX_DONE!\n");

                        /* copy data from internal buffer */
                        size = status >> 24;

                        spin_lock(&dev->rx.lock);

                        while (i < size) {
                                dev->rx.buffer[i] = cec_reg_read(dev, CEC_RX_BUFF0 + (i*4));
                                i++;
                        }

                        if (size > 0) {
                                dpr_info( "fsize: %d ", size);
                                dpr_info( "frame: ");
                                for (i = 0; i < size; i++) {
                                        dpr_info( "0x%02x ", dev->rx.buffer[i]);
                                }
                                dpr_info( "End ~\n");
                        }

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
        struct miscdevice *misc = (struct miscdevice *)(file!=NULL)?file->private_data:NULL;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(misc!=NULL)?dev_get_drvdata(misc->parent):NULL;

	if(file != NULL) {
		file->private_data = dev;
	}

	return 0;
}

int cec_release(struct inode *inode, struct file *file)
{
	/*
        int ret = -1;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file!=NULL)?file->private_data:NULL;
	*/
	return 0;
}

ssize_t cec_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
        ssize_t ret = -1;
        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file !=NULL)?file->private_data:NULL;

        do {
                if(dev == NULL) {
                        pr_err("[ERROR][HDMI_V14]%s parameter is null at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

		if(dev->enable_cnt == 0) {
			pr_err("[ERROR][HDMI_V14]%s cec is not started(%d)\r\n", __func__, __LINE__);
                        break;
		}

                if (atomic_read(&dev->rx.state) == STATE_ERROR) {
                        pr_err("[ERROR][HDMI_V14] %s end, because of RX error\n", __func__);
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
                        pr_err("[ERROR][HDMI_V14]CEC: copy_to_user() failed!\n");
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
                        pr_info("[INFO][HDMI_V14]%s\r\n", msg_debug);
                }
                #endif

                dpr_info( "%s end\n", __func__);
        } while(0);

        return ret;
}

ssize_t cec_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
        int i;
	int timeout_ms;
        unsigned int reg;

        struct tcc_hdmi_cec_dev *dev = (struct tcc_hdmi_cec_dev *)(file !=NULL)?file->private_data:NULL;

        do {
                if(dev == NULL) {
                        pr_err("[ERROR][HDMI_V14]%s parameter is null at line(%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
                }

		if(dev->enable_cnt == 0) {
			pr_err("[ERROR][HDMI_V14]%s cec is not started(%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
		}

		/* check data size */
                if (count == 0) {
                        pr_err("[ERROR][HDMI_V14]%s count is inavlid at line(%d)\r\n", __func__, __LINE__);
                        break;
                }

                if (count > CEC_TX_BUFF_SIZE) {
			count = CEC_TX_BUFF_SIZE;
                }

                if (copy_from_user(dev->tx.buffer, buffer, count)) {
                        pr_err("[ERROR][HDMI_V14]%s copy_from_user is failed at line(%d)\r\n", __func__, __LINE__);
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
                        pr_info("[INFO][HDMI_V14]%s\r\n", msg_debug);
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
		reg = CEC_TX_CTRL_START;

                /* if message is broadcast message - set corresponding bit */
                if ((dev->tx.buffer[0] & CEC_MESSAGE_BROADCAST_MASK) == CEC_MESSAGE_BROADCAST)
                        reg |= CEC_TX_CTRL_BCAST;

                /* set number of retransmissions */
                reg |= (5 << 4);

                cec_reg_write(dev, reg, CEC_TX_CTRL);

                /* wait for interrupt */



		/* CEC Frame
		 *  Start    4.7ms
		 *  Heaader  2.75ms
		 *  Data_n   2.75ms *n
		 * S(4.7ms) + H (27.5ms) + D ((27.5ms) * (count -1)) + 15ms margin
		 * Max 485ms */
		timeout_ms = 20 + (30 * count);
		dpr_info("%s wait %dms \r\n", __func__, timeout_ms);
                if (0 == wait_event_interruptible_timeout(dev->tx.waitq,
			atomic_read(&dev->tx.state) != STATE_TX, msecs_to_jiffies(timeout_ms))) {
                        //pr_err("[ERROR][HDMI_V14]%s wait failed at line (%d)\r\n", __func__, __LINE__);
                        count = 0;
                        break;
                }

                if (atomic_read(&dev->tx.state) == STATE_ERROR) {
                        pr_err("[ERROR][HDMI_V14] %s end, because of TX error\n", __func__);
                        count = 0;
                        break;
                }

                dpr_info( "%s end\n", __func__);
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
                                dpr_io_info("CEC: ioctl(CEC_IOC_START)\r\n");
                                ret = cec_start(dev);
                                break;

                        case CEC_IOC_STOP:
                                dpr_io_info("CEC: ioctl(CEC_IOC_STOP)\r\n");
                                ret = cec_stop(dev);
                                break;

                        case CEC_IOC_SETLADDR:
                                {
                                        unsigned int laddr;
                                        dpr_io_info( "CEC: ioctl(CEC_IOC_SETLADDR)\n");

                                        if(copy_from_user(&laddr, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }

					if(dev->enable_cnt == 0) {
						pr_err("[ERROR][HDMI_V14]%s cec is not started(%d)\r\n", __func__, __LINE__);
			                        break;
					}

                                        dpr_io_info( "CEC: logical address = 0x%02x\n", laddr);
                                        cec_reg_write(dev, laddr & 0x0F, CEC_LOGIC_ADDR);
                                        ret = 0;
                                }
                                break;

                        case CEC_IOC_SENDDATA:
                                {
                                        /* It will be deprecated */
                                        #if 0
                                        unsigned int uiData;
                                        dpr_io_info( "CEC: ioctl(CEC_IOC_SENDDATA)\n");
                                        if(copy_from_user(&uiData, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        dpr_info( "CEC: SendData = 0x%02x\n", uiData);
                                        #endif
                                }
                                break;

                        case CEC_IOC_RECVDATA:
                                /* It will be deprecated */
                                dpr_io_info("CEC: ioctl(CEC_IOC_RECVDATA)\r\n");
                                ret = 0;
                                break;

                        case CEC_IOC_GETRESUMESTATUS:
                                dpr_io_info( "CEC: ioctl(CEC_IOC_GETRESUMESTATUS)\n");
                                if(copy_to_user((void __user *)arg, &dev->resume, sizeof(unsigned int))) {
                                        pr_err("[ERROR][HDMI_V14]%s failed copy_to_user at line(%d)\r\n", __func__, __LINE__);
                                        break;
                                }
                                ret = 0;
                                break;
                        case CEC_IOC_CLRRESUMESTATUS:
                                {
                                        unsigned int resume_status;
                                        dpr_io_info( "CEC: ioctl(CEC_IOC_SENDDATA)\n");
                                        if(copy_from_user(&resume_status, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
                                                break;
                                        }
                                        dev->resume = (resume_status > 0)?1:0;
                                        ret = 0;
                                }
                                break;
                        case CEC_IOC_BLANK:
                                {
                                        unsigned int cmd;
                                        dpr_io_info("CEC: ioctl(CEC_IOC_BLANK)\r\n");
                                        if(copy_from_user(&cmd, (void __user *)arg, sizeof(unsigned int))) {
                                                pr_err("[ERROR][HDMI_V14]%s failed copy_from_user at line(%d)\r\n", __func__, __LINE__);
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
	int backup_enable_cnt;
        struct tcc_hdmi_cec_dev *tcc_hdmi_cec_dev;

        pr_info("[INFO][HDMI_V14]### %s \n", __func__);

        if(dev != NULL) {
                tcc_hdmi_cec_dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(dev);
                if(tcc_hdmi_cec_dev != NULL) {
                        if(!tcc_hdmi_cec_dev->suspend) {
				backup_enable_cnt = tcc_hdmi_cec_dev->enable_cnt;
				if(tcc_hdmi_cec_dev->enable_cnt > 0) {
					tcc_hdmi_cec_dev->enable_cnt = 1;
				}
                                cec_stop(tcc_hdmi_cec_dev);
				tcc_hdmi_cec_dev->enable_cnt = backup_enable_cnt;
                                tcc_hdmi_cec_dev->suspend = 1;
                        }
                        tcc_hdmi_cec_dev->resume = 0;
                }
        }

        return 0;
}

static int cec_resume(struct device *dev)
{
	int backup_enable_cnt;
        struct tcc_hdmi_cec_dev *tcc_hdmi_cec_dev;

        pr_info("[INFO][HDMI_V14]### %s \n", __func__);

        if(dev != NULL) {
                tcc_hdmi_cec_dev = (struct tcc_hdmi_cec_dev *)dev_get_drvdata(dev);
                if(tcc_hdmi_cec_dev != NULL) {
                        if(tcc_hdmi_cec_dev->suspend  && !tcc_hdmi_cec_dev->runtime_suspend) {
				if(tcc_hdmi_cec_dev->enable_cnt > 0) {
					backup_enable_cnt = tcc_hdmi_cec_dev->enable_cnt;
					tcc_hdmi_cec_dev->enable_cnt = 0;
					cec_start(tcc_hdmi_cec_dev);
					tcc_hdmi_cec_dev->enable_cnt = backup_enable_cnt;
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
		memset(dev, 0, sizeof(struct tcc_hdmi_cec_dev ));

                dev->pdev = &pdev->dev;

                dev->pclk = of_clk_get(pdev->dev.of_node, 0);
                if (IS_ERR_OR_NULL(dev->pclk)) {
                    	pr_err("[ERROR][HDMI_V14]HDMI: failed to get hdmi pclk\n");
                    	dev->pclk = NULL;
                    	break;
                }

		ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &dev->pclk_freq);
	        if(ret < 0) {
	                pr_err("[ERROR][HDMI_V14]HDMI: failed to get cec pclk frequency\n");
			break;
	        }

                clk_set_rate(dev->pclk, dev->pclk_freq);
                clk_prepare_enable(dev->pclk);

                dev->cec_irq = of_irq_to_resource(pdev->dev.of_node, 0, NULL);
                if (dev->cec_irq < 0) {
                        pr_err("[ERROR][HDMI_V14]%s failed get irq for hdmi cec\r\n", __func__);
                        break;
                }

                dev->hdmi_cec_io = of_iomap(pdev->dev.of_node, 0);
                if(dev->hdmi_cec_io == NULL){
                        pr_err("[ERROR][HDMI_V14]%s:Unable to map hdmi ctrl resource at line(%d)\n", __func__, __LINE__);
                        break;
                }

                dev->misc = devm_kzalloc(&pdev->dev, sizeof(struct miscdevice), GFP_KERNEL);
                if(dev->misc == NULL) {
                        pr_err("[ERROR][HDMI_V14]%s:Unable to createe hdmi misc at line(%d)\n", __func__, __LINE__);
                        ret = -ENOMEM;
                        break;
                }
                dev->misc->minor = MISC_DYNAMIC_MINOR;
                dev->misc->name = "CEC";
                dev->misc->fops = &cec_fops;
                dev->misc->parent = dev->pdev;
                ret = misc_register(dev->misc);
                if(ret < 0) {
                        pr_err("[ERROR][HDMI_V14]%s failed misc_register for hdmi cec\r\n", __func__);
                        break;
                }
                dev_set_drvdata(dev->pdev, dev);

                pr_info("[INFO][HDMI_V14]%s:HDMI CEC driver %s\n", __func__, SRC_VERSION);

                init_waitqueue_head(&dev->tx.waitq);
                init_waitqueue_head(&dev->rx.waitq);
                spin_lock_init(&dev->rx.lock);

                ret = devm_request_irq(dev->pdev, dev->cec_irq, cec_irq_handler, IRQF_SHARED, "hdmi-cec", dev);
                if(ret < 0) {
                        pr_err("[ERROR][HDMI_V14]%s failed request interrupt for cec\r\n", __func__);
                }

                dev->rx.buffer = devm_kmalloc(&pdev->dev, CEC_RX_BUFF_SIZE, GFP_KERNEL);
                if (dev->rx.buffer == NULL) {
                        pr_err("[ERROR][HDMI_V14]%s failed kmalloc \r\n", __func__);
                        ret = -1;
                        break;
                }
                dev->rx.size   = 0;

                dev->tx.buffer = devm_kmalloc(&pdev->dev, CEC_TX_BUFF_SIZE, GFP_KERNEL);
                if (dev->tx.buffer == NULL) {
                        pr_err("[ERROR][HDMI_V14]%s failed kmalloc \r\n", __func__);
                        ret = -1;
                        break;
                }

                #ifdef CONFIG_PM
                pm_runtime_set_active(dev->pdev);
                pm_runtime_enable(dev->pdev);
                pm_runtime_get_noresume(dev->pdev);  //increase usage_count
                #endif

                /* disable the clocks */
                clk_disable(dev->pclk);
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


