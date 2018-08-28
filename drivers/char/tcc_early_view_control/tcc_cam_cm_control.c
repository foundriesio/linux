/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "tcc_cam_cm_control.h"
#include "tcc_avn_proc.h"

static int debug		= 0;
#define TAG				"tcc_cm_ctrl"
#define log(msg...)		{ printk(KERN_INFO TAG ": %s - ", __func__); printk(msg); }
#define dlog(msg...)	if(debug) { printk(KERN_INFO TAG ": %s - ", __func__); printk(msg); }
#define FUNCTION_IN		dlog("IN\n");
#define FUNCTION_OUT	dlog("OUT\n");

#define MODULE_NAME		"tcc_cm_ctrl"

#define CM_CTRL_IOCTL_OFF		0
#define CM_CTRL_IOCTL_ON		1
#define CM_CTRL_IOCTL_CMD		3
#define CM_CTRL_IOCTL_KNOCK		4

/*
 * register offset
 */
#define	CMBUS_MP_BOX_MP_PORT		(0x000000)
#define	CMBUS_MP_BOX_SP_PORT		(0x010000)
#define	CMBUS_CODE_MEM				(0x080000)
#define	CMBUS_DATA_MEM				(0x090000)
#define	CMBUS_BUS_CONFIG			(0x0F0000)

/*
 * CMBUS_XX_BOX_XX_PORT(Mailbox Control) Register
 */
#define MBOXTXD						(0x00)
#define MBOXRXD						(0x20)
#define MBOXCTR						(0x40)
#define MBOXSTR                     (0x44)
#define MBOX_DT_STR                 (0x50)
#define MBOX_RT_STR                 (0x54)
#define MBOXDTXD                    (0x60)
#define MBOXRTXD                    (0x70)

#define MBOXCTR_D_FLUSH_SHIFT		(7)
#define MBOXCTR_FLUSH_SHIFT			(6)
#define MBOXCTR_OEN_SHIFT			(5)
#define MBOXCTR_IEN_SHIFT			(4)
#define MBOXCTR_ILEVEL_SHIFT		(0)

#define MBOXCTR_D_FLUSH_MASK		(0x1 << MBOXCTR_D_FLUSH_SHIFT)
#define MBOXCTR_FLUSH_MASK			(0x1 << MBOXCTR_FLUSH_SHIFT)
#define MBOXCTR_OEN_MASK			(0x1 << MBOXCTR_OEN_SHIFT)
#define MBOXCTR_IEN_MASK			(0x1 << MBOXCTR_IEN_SHIFT)
#define MBOXCTR_ILEVEL_MASK			(0x3 << MBOXCTR_ILEVEL_SHIFT)
#define MBOXCTR_ALL_MASK			(MBOXCTR_D_FLUSH_MASK | MBOXCTR_FLUSH_MASK | MBOXCTR_OEN_MASK | MBOXCTR_IEN_MASK | MBOXCTR_ILEVEL_MASK)

#define MBOXSTR_SCOUNT_SHIFT        (20)

#define MBOXSTR_SCOUNT_MASK         (0xf << MBOXSTR_SCOUNT_SHIFT)


/*
 * CMBUS_BUS_CONFIG Register
 */
#define IRQ_MASK_POL				(0x10)
#define CM4_RESET					(0x18)

#define CM4_RESET_SYS_SHIFT			(2)
#define CM4_RESET_POR_SHIFT			(1)

#define CM4_RESET_SYS_MASK			(0x1 << CM4_RESET_SYS_SHIFT)
#define CM4_RESET_POR_MASK			(0x1 << CM4_RESET_POR_SHIFT)

//#define	MAILBOX_RX_METHOD_INTERRUPT

struct clk							* cm_clk;

void __iomem						* pConfig;
void __iomem						* pMboxMain;
void __iomem						* pMboxSub;
#if defined(MAILBOX_RX_METHOD_INTERRUPT)
int									cmbus_irq;
DECLARE_WAIT_QUEUE_HEAD(wq_cm_cmd);
int									cm_cmd_status;
cm_ctrl_msg_t						msg_by_interrupt;
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

//#define MBOX_TEST

int tcc_cm_ctrl_parse_device_tree(struct platform_device * pdev) {
	struct device_node	* main_node	= pdev->dev.of_node;
	struct device_node	* bus_node	= NULL;
	int					idxReg		= 0;
	int					ret			= 0;

	FUNCTION_IN

	// clock
	cm_clk = of_clk_get(main_node, 0);
	if(IS_ERR(cm_clk)) {
		dev_err(&pdev->dev, "failed to get cm_clk\n");
		ret = -ENXIO;
		goto goto_return;
	}

	idxReg = of_property_match_string(main_node, "reg-names", "mbox_0");
	if(0 <= idxReg) {
		pMboxMain = (void __iomem *)of_iomap(main_node, idxReg);
		dlog("pMboxMain: 0x%p\n", pMboxMain);
	} else {
		log("ERROR: Can NOT find a node \"%s\"\n", "mbox_0");
		ret = -ENXIO;
		goto goto_return;
	}

	idxReg = of_property_match_string(main_node, "reg-names", "mbox_1");
	if(0 <= idxReg) {
		pMboxSub = (void __iomem *)of_iomap(main_node, idxReg);
		dlog("pMboxSub: 0x%p\n", pMboxSub);
	} else {
		log("ERROR: Can NOT find a node \"%s\"\n", "mbox_1");
		ret = -ENXIO;
		goto goto_return;
	}

	idxReg = of_property_match_string(main_node, "reg-names", "config");
	if(0 <= idxReg) {
		pConfig = (void __iomem *)of_iomap(main_node, idxReg);
		dlog("pConfig: 0x%p\n", pConfig);
	} else {
		log("ERROR: Can NOT find a node \"%s\"\n", "config");
		ret = -ENXIO;
		goto goto_return;
	}

#if defined(MAILBOX_RX_METHOD_INTERRUPT)
	bus_node = of_parse_phandle(main_node, "tcc_cm_bus", 0);
	if(bus_node) {
		cmbus_irq = irq_of_parse_and_map(bus_node, 0);
		dlog("cmbus_irq num: %d\n", cmbus_irq);
	} else {
		log("ERROR: Find the \"tcc_cm_bus\" node.\n");
		ret = -ENODEV;
	}
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

goto_return:
	FUNCTION_OUT
	return ret;
}

int tcc_mbox_send_msg(cm_ctrl_msg_t * msg) {
	void __iomem	* reg	= pMboxMain;
	unsigned long	value	= 0;

	FUNCTION_IN

	// Flush transmit FIFO or Wait until empty of transmit FIFO
	// Can be skipped by case

	// Set "OEN" to Low
	// To protect the invalid event which can be generated during writing data
	value = (__raw_readl(reg + MBOXCTR) & ~(0x1 << MBOXCTR_OEN_SHIFT));
	__raw_writel(value, reg + MBOXCTR);

	// Write Data to FIFO
	// The sequence of writing operation can be replaced to burst writing to the transmit fifo data region
	dlog("Tx: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",	\
		msg->preamble, msg->data[5], msg->data[4], msg->data[3], msg->data[2], msg->data[1], msg->data[0], msg->cmd);

	value = msg->cmd;
	__raw_writel(value, reg + MBOXTXD + 0x00);
	value = msg->data[0];
	__raw_writel(value, reg + MBOXTXD + 0x04);
	value = msg->data[1];
	__raw_writel(value, reg + MBOXTXD + 0x08);
	value = msg->data[2];
	__raw_writel(value, reg + MBOXTXD + 0x0C);
	value = msg->data[3];
	__raw_writel(value, reg + MBOXTXD + 0x10);
	value = msg->data[4];
	__raw_writel(value, reg + MBOXTXD + 0x14);
	value = msg->data[5];
	__raw_writel(value, reg + MBOXTXD + 0x18);
	value = msg->preamble;
	__raw_writel(value, reg + MBOXTXD + 0x1C);

	// Set "OEN" to HIGH
	// After this, the event(interrupt) for "SendMessage" will occur to counter-part
	value = __raw_readl(reg + MBOXCTR) | (0x1 << MBOXCTR_OEN_SHIFT);
    __raw_writel(value, reg + MBOXCTR);

	// Check "MEMPTY" being true
//	while(reg->uMBOX_STATUS.bREG.MEMP == 0);

	FUNCTION_OUT
	return 0;
}

int tcc_mbox_receive_msg(cm_ctrl_msg_t * msg) {
	void __iomem	* reg	= pMboxMain;
	unsigned long	scount	= 0;

	FUNCTION_IN

	// If "SEMP" is low, the message has been arrived and than "SCOUNT"
	// indicates the total number of received messages in word unit.
	scount = ((__raw_readl(reg + MBOXSTR) & MBOXSTR_SCOUNT_MASK) >> MBOXSTR_SCOUNT_SHIFT);

	dlog("scount: %d\n", scount);
	if(scount != 8) {
		return -1;
	}

	// The sequence of read operation can be replaced to burst read to the receive fifo data region
	// After reading done, the empty status of the counter-part goes to "HIGH" means empty
	msg->cmd		= __raw_readl(reg + MBOXRXD + 0x00);
	msg->data[0]	= __raw_readl(reg + MBOXRXD + 0x04);
	msg->data[1]	= __raw_readl(reg + MBOXRXD + 0x08);
	msg->data[2]	= __raw_readl(reg + MBOXRXD + 0x0C);
	msg->data[3]	= __raw_readl(reg + MBOXRXD + 0x10);
	msg->data[4]	= __raw_readl(reg + MBOXRXD + 0x14);
	msg->data[5]	= __raw_readl(reg + MBOXRXD + 0x18);
	msg->preamble	= __raw_readl(reg + MBOXRXD + 0x1C);

	dlog("Rx: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",	\
		msg->preamble, msg->data[5], msg->data[4], msg->data[3], msg->data[2], msg->data[1], msg->data[0], msg->cmd);

	FUNCTION_OUT
	return 0;
}

int tcc_mbox_wait_receive_msg(cm_ctrl_msg_t * msg_to_receive) {
	unsigned int	ack_to_check	= msg_to_receive->cmd | MAILBOX_MSG_ACK;
	int				idxTry			= 0;
	int				nTry			= 60;
	int				ret				= 0;

	FUNCTION_IN

	for(idxTry=0; idxTry<nTry; idxTry++) {
		ret = tcc_mbox_receive_msg(msg_to_receive);
		if(!ret) {
			dlog("cmd: 0x%08x, ack: 0x%08x - %s\n", \
				msg_to_receive->cmd, ack_to_check, (msg_to_receive->cmd == ack_to_check ? "O" : "X"));
			if(msg_to_receive->cmd == ack_to_check)
				break;
		} else {
			msleep(50);
		}
	}

	FUNCTION_OUT
	return ret;
}

#if defined(MAILBOX_RX_METHOD_INTERRUPT)
static irqreturn_t CM_MailBox_Handler(int irq, void * dev) {
	int		ret = 0;

//	FUNCTION_IN

	// Clear the mailbox message
	memset((void *)&msg_by_interrupt, 0, sizeof(cm_ctrl_msg_t));

	// Receive a ack message
	ret = tcc_mbox_receive_msg(&msg_by_interrupt);

	cm_cmd_status = 0;
	wake_up_interruptible(&wq_cm_cmd);

//	FUNCTION_OUT
	return IRQ_HANDLED;
}
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

int CM_SEND_COMMAND(cm_ctrl_msg_t * pTxMsg, cm_ctrl_msg_t * pRxMsg) {
	int		ret		= 0;

	FUNCTION_IN

	// Print the tx msg
	printk(KERN_ALERT "[KN] Tx: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", \
		pTxMsg->preamble, pTxMsg->data[5], pTxMsg->data[4], pTxMsg->data[3], pTxMsg->data[2], pTxMsg->data[1], pTxMsg->data[0], pTxMsg->cmd);

	// Send the mailbox msg
	tcc_mbox_send_msg(pTxMsg);

	// Receive a mailbox msg
#if defined(MAILBOX_RX_METHOD_INTERRUPT)
	cm_cmd_status = 1;
	ret = wait_event_interruptible_timeout(wq_cm_cmd, cm_cmd_status == 0, msecs_to_jiffies(1000)) ;
	if(ret <= 0) {
		log("wait_event_interruptible_timeout: %d\n", ret);
	} else {
		*pRxMsg = msg_by_interrupt;
	}
#else
	ret = tcc_mbox_wait_receive_msg(pRxMsg);
	if(ret < 0) {
		printk(KERN_ERR "ERROR: tcc_mbox_wait_receive_msg returned %d\n", ret);
	}
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

	// Print the rx msg
	printk(KERN_ALERT "[KN] Rx: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", \
		pRxMsg->preamble, pRxMsg->data[5], pRxMsg->data[4], pRxMsg->data[3], pRxMsg->data[2], pRxMsg->data[1], pRxMsg->data[0], pRxMsg->cmd);

	FUNCTION_OUT
	return ret;
}

static void CM_UnloadBinary(void) {
	void __iomem	* reg	= pConfig;
    unsigned long	value	= 0;

	FUNCTION_IN

    value = __raw_readl(reg + CM4_RESET);
    value |= ((1 << CM4_RESET_SYS_SHIFT) | (1 << CM4_RESET_POR_SHIFT));
    __raw_writel(value, reg + CM4_RESET);

	FUNCTION_OUT
}

static void CM_MailBox_Configure(void) {
	void __iomem	* reg	= NULL;
	unsigned long	value	= 0;

	FUNCTION_IN

	// main
	reg = pMboxMain;
	value = __raw_readl(reg + MBOXCTR) & ~(MBOXCTR_ALL_MASK);
	value |= (	(0x1 << MBOXCTR_D_FLUSH_SHIFT)	| \
				(0x1 << MBOXCTR_FLUSH_SHIFT)	| \
			/*	(0x1 << MBOXCTR_OEN_SHIFT)		| \	*/	// OEN is enabled only when transmit
				(0x1 << MBOXCTR_IEN_SHIFT)		| \
				(0x3 << MBOXCTR_ILEVEL_SHIFT)	);
	__raw_writel(value, reg + MBOXCTR);

	// sub
	// DO NOT Configure

#if defined(MAILBOX_RX_METHOD_INTERRUPT)
	// irq
	reg = pConfig;
	value = __raw_readl(reg + IRQ_MASK_POL);
	value |= (	(0x1 << 22)	| \
				(0x1 << 16)	);
	__raw_writel(value, reg + IRQ_MASK_POL);
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

	FUNCTION_OUT
}

int tcc_cm_ctrl_off(void) {
	int		ret = 0;

	FUNCTION_IN

	ret = IS_ERR_OR_NULL(cm_clk);
	if(!ret) {
		// Unload the CM image.
		CM_UnloadBinary();

		// Clock off
		clk_disable_unprepare(cm_clk);
		clk_put(cm_clk);
	} else {
		log("ERROR: CM clock is ERROR or NULL\n");
	}

	FUNCTION_OUT
	return ret;
}

int tcc_cm_ctrl_on(void) {
	int		ret = 0;

	FUNCTION_IN

	ret = IS_ERR_OR_NULL(cm_clk);
	if(!ret) {
		clk_prepare_enable(cm_clk);
		clk_set_rate(cm_clk, 100 * 1000 * 1000);
//		msleep(100);	// Wait for CM Booting
	} else {
		log("ERROR: CM clock is ERROR or NULL\n");
	}

	FUNCTION_OUT
	return ret;
}

int tcc_cm_ctrl_check_clock(void) {
	void __iomem	* reg	= pConfig;
	int				status	= 0;

	FUNCTION_IN

	status = __raw_readl(reg + CM4_RESET) & (CM4_RESET_SYS_MASK | CM4_RESET_POR_MASK);
	dlog("Cortex-M status is 0x%08x, and it's %s working\n", status, (status ? "NOT" : ""));

	FUNCTION_OUT
	return status;
}

#ifdef MBOX_TEST
static struct task_struct * thread;

static int tcc_cm_ctrl_thread(void * data) {
	int ret = 0;

	FUNCTION_IN

	while(!kthread_should_stop()) {
		tcc_cm_ctrl_knock_earlycamera();
		msleep_interruptible(3000);
	}

	FUNCTION_OUT
	return 0;
}
#endif

long tcc_cm_ctrl_ioctl(struct file * filp, unsigned int cmd, unsigned long arg) {
	int		ret	= 0;

	FUNCTION_IN

	dlog("command: 0x%x\n", cmd);
	switch(cmd) {
	case CM_CTRL_IOCTL_OFF:
		tcc_cm_ctrl_off();
		break;

	case CM_CTRL_IOCTL_ON:
		tcc_cm_ctrl_on();
		break;

	case CM_CTRL_IOCTL_CMD:
		ret = (* CM_AVN_Proc)(arg);
		break;

	case CM_CTRL_IOCTL_KNOCK:
		ret = tcc_cm_ctrl_check_clock();
		break;

	default:
		log("The command(0x%08x) is WRONG.\n", cmd);
		WARN_ON(1);
	}

	FUNCTION_OUT
	return ret;
}

int tcc_cm_ctrl_open(struct inode * inode, struct file * filp) {
	return 0;
}

int tcc_cm_ctrl_release(struct inode * inode, struct file * filp) {
	return 0;
}

struct file_operations tcc_cm_ctrl_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = tcc_cm_ctrl_ioctl,
	.open           = tcc_cm_ctrl_open,
	.release        = tcc_cm_ctrl_release,
};

static dev_t			tcc_cm_ctrl_dev_region;
static struct class 	* tcc_cm_ctrl_class;
static struct cdev		tcc_cm_ctrl_cdev;

int tcc_cm_ctrl_probe(struct platform_device * pdev) {
	int		idx	= 0;
	int		ret	= 0;

	FUNCTION_IN

	// Allocate a charactor device region
	ret = alloc_chrdev_region(&tcc_cm_ctrl_dev_region, 0, 1, MODULE_NAME);
	if(ret < 0) {
		log("ERROR: Allocate a charactor device region for the \"%s\"\n", MODULE_NAME);
		return ret;
	}
	
	// Create the CM control class
	tcc_cm_ctrl_class = class_create(THIS_MODULE, MODULE_NAME);
	if(tcc_cm_ctrl_class == NULL) {
		log("ERROR: Create the \"%s\" class\n", MODULE_NAME);
		goto goto_unregister_chrdev_region;
	}

	// Create the CM control device file system
	if(device_create(tcc_cm_ctrl_class, NULL, tcc_cm_ctrl_dev_region, NULL, MODULE_NAME) == NULL) {
		log("ERROR: Create the \"%s\" device file\n", MODULE_NAME);
		goto goto_destroy_class;
	}

	// Register the CM control device as a charactor device
	cdev_init(&tcc_cm_ctrl_cdev, &tcc_cm_ctrl_fops);
	ret = cdev_add(&tcc_cm_ctrl_cdev, tcc_cm_ctrl_dev_region, 1);
	if(ret < 0) {
		log("ERROR: Register the \"%s\" device as a charactor device\n", MODULE_NAME);
		goto goto_destroy_device;
	}

	// Parse the device tree
	tcc_cm_ctrl_parse_device_tree(pdev);

	// Configure the CM clock
	clk_prepare_enable(cm_clk);
	clk_set_rate(cm_clk, 100 * 1000 * 1000);

	// Config the mailbox
	CM_MailBox_Configure();

#if defined(MAILBOX_RX_METHOD_INTERRUPT)
	// Request MBOX irq
//	ret = request_irq(cmbus_irq, CM_MailBox_Handler, IRQ_TYPE_LEVEL_LOW, MODULE_NAME, NULL);
	ret = request_irq(cmbus_irq, CM_MailBox_Handler, IRQF_SHARED, MODULE_NAME, NULL);
	if(ret < 0) {
		log("ERROR: Request_irq %d\n", ret);
		return -1;
	}
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

#ifdef MBOX_TEST
    {
    	thread = kthread_create(tcc_cm_ctrl_thread, &ret, MODULE_NAME);
    	if(!IS_ERR(thread)) {
    		wake_up_process(thread);
    	} else {
    		log("!@#---- ERROR: kthread_create\n");
    	}
    }
#endif

	FUNCTION_OUT
	return 0;

goto_unregister_cdev:
	// Unregister the CM control device
	cdev_del(&tcc_cm_ctrl_cdev);

goto_destroy_device:
	// Destroy the CM control device file system
	device_destroy(tcc_cm_ctrl_class, tcc_cm_ctrl_dev_region);
	
goto_destroy_class:
	// Destroy the CM control class
	class_destroy(tcc_cm_ctrl_class);

goto_unregister_chrdev_region:
	// Unregister the charactor device region
	unregister_chrdev_region(tcc_cm_ctrl_dev_region, 1);

	FUNCTION_OUT
	return -1;
}

int tcc_cm_ctrl_remove(struct platform_device * pdev) {
	FUNCTION_IN

#if defined(MAILBOX_RX_METHOD_INTERRUPT)
	// Free MBOX irq
	free_irq(cmbus_irq, NULL);
#endif//defined(MAILBOX_RX_METHOD_INTERRUPT)

	// Unregister the CM control device
	cdev_del(&tcc_cm_ctrl_cdev);

	// Destroy the CM control device file system
	device_destroy(tcc_cm_ctrl_class, tcc_cm_ctrl_dev_region);

	// Destroy the CM control class
	class_destroy(tcc_cm_ctrl_class);

	// Unregister the charactor device region
	unregister_chrdev_region(tcc_cm_ctrl_dev_region, 1);

	FUNCTION_OUT
	return 0;
}

static struct of_device_id tcc_cm_ctrl_of_match[] = {
	{ .compatible = "telechips,tcc_cm_ctrl" },
};
MODULE_DEVICE_TABLE(of, tcc_cm_ctrl_of_match);

static struct platform_driver tcc_cm_ctrl_driver = {
	.probe		= tcc_cm_ctrl_probe,
	.remove		= tcc_cm_ctrl_remove,
	.driver		= {
		.name			= MODULE_NAME,
		.owner			= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_cm_ctrl_of_match),
	},
};
module_platform_driver(tcc_cm_ctrl_driver);

MODULE_DESCRIPTION("Telechips Cortex-M Control Driver");
MODULE_VERSION("v1.0");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips.Co.Ltd");

