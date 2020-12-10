/*
 * Copyright (C) 20010-2020 Telechips
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

#include "tcc_hdcp_log.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include "hdcp_api.h"
#include "hdcp_ss.h"


//#define HDCP_IRQ_HANDLING		// kernel 3.18

#define HDCP_DEV_NAME "hdcp"
//#define HDCP_DEV_MAJOR 233
#define HDCP_DEV_MINOR 0

// int hdcp_api_initialize(void) { return 0; }
// int hdcp_api_cmd_process(unsigned int cmd, unsigned long arg) { return 0; }
// int hdcp_api_status_chk(unsigned char flag) { return 0; }
// unsigned int hdcp_api_poll_chk(struct file *file, poll_table *wait) { return
// 0; } int hdcp_api_open(void) { return 0; } int hdcp_api_close(void) { return
// 0; }

static long hdcp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	hdcp_api_cmd_process(cmd, arg);
	return 0;
}

#if defined(HDCP_IRQ_HANDLING)
/**
 * HDCP IRQ handler. @n
 * If HDCP IRQ occurs, set hdcp_event and wake up the waitqueue.
 */
static irqreturn_t hdcp_handler(int irq, void *dev_id)
{
	unsigned char flag;

	// check HDCP INT
	flag = hdcp_readb(HDMI_SS_INTC_FLAG);

	if (flag & (1 << HDMI_IRQ_HDCP)) {
		// ILOG("%s : flag = %d\n", __func__, flag)
		// check HDCP Status
		flag = hdcp_readb(HDMI_STATUS);

		if (flag)
			hdcp_api_status_chk(flag);
	} else {
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}
#endif

static unsigned int hdcp_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	mask = hdcp_api_poll_chk(file, wait);

	return mask;
}

static int hdcp_release(struct inode *inode, struct file *filp)
{
	hdcp_api_close();

	return 0;
}

static int hdcp_open(struct inode *inode, struct file *filp)
{
	hdcp_api_open();

	return 0;
}

const struct file_operations hdcp_fops = {
	.owner = THIS_MODULE,
	.open = hdcp_open,
	.release = hdcp_release,
	.unlocked_ioctl = hdcp_ioctl,
	.poll = hdcp_poll,
};

static struct miscdevice hdcp_misc_device = {
	.name = HDCP_DEV_NAME,
	//.major = HDCP_DEV_MAJOR,
	.minor = HDCP_DEV_MINOR,
	.fops = &hdcp_fops,
};

static __init int hdcp_init(void)
{
	int ret = -ENODEV;

	ret = misc_register(&hdcp_misc_device);
	if (ret < 0) {
		ILOG("%s: Couldn't register device - (ret: 0x%X)\n",
		     HDCP_DEV_NAME, ret);
		return -EBUSY;
	}

#if defined(HDCP_IRQ_HANDLING)
	ret = request_irq(
		IRQ_HDMI, hdcp_handler, IRQF_SHARED, HDCP_DEV_NAME,
		hdcp_handler);
	if (ret < 0) {
		ILOG("IRQ %d is not free. - (ret: 0x%X)\n", IRQ_HDMI, ret);
		misc_deregister(&hdcp_misc_device);
		return -EIO;
	}
#endif

	hdcp_api_initialize();

	ILOG("init() ret: %d (0x%X)\n", ret, ret);

	return 0;
}

static __exit void hdcp_exit(void)
{
	ILOG("exit()\n");

#if defined(HDCP_IRQ_HANDLING)
	free_irq(IRQ_HDMI, hdcp_handler);
#endif

	misc_deregister(&hdcp_misc_device);
}

module_init(hdcp_init);
module_exit(hdcp_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCCxxx hdcp driver");
MODULE_LICENSE("GPL");
