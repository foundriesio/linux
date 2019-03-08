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
****************************************************************************/
#ifndef TCC_HDMI_V_2_0_CEC_H
#define TCC_HDMI_V_2_0_CEC_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <asm/io.h>

#include "../hdmi_cec_lib/cec_reg.h"

#define HDMI_CLK_CEC_INDEX_CORE		0
#define HDMI_CLK_CEC_INDEX_SFR		1
#define HDMI_CLK_CEC_INDEX_IOBUS	2
#define HDMI_CLK_CEC_INDEX_MAX		3

#define HDMI_CEC_CORE_CLK_RATE (32768)
#define HDMI_CEC_SFR_CLK_RATE (27000000)

struct cec_buffer{
	char	send_buf[CEC_TX_DATA_SIZE];
	char	recv_buf[CEC_RX_DATA_SIZE];
	unsigned	int size;
};

/**
 * @short Main structures to instantiate the driver
 */
struct cec_device{

	/** Device node */
	struct device 		*parent_dev;

	/** Device Tree Information */
	char 			*device_name;

	/** clocks **/
    struct clk     *clk[HDMI_CLK_CEC_INDEX_MAX];

	/** iobus cec base address **/
	volatile void __iomem *cec_core_io;
	volatile void __iomem *cec_clk_sel;

	/** IRQ number **/
	uint32_t		cec_irq;
	uint32_t		cec_wake_up_irq;

	unsigned int 	standby_status;

    /** Misc Device */
    struct miscdevice	*misc;

	/** Device list **/
	struct list_head	devlist;

	struct cec_buffer buf;

	int			l_address;
	int			p_address;
	int			cec_enable;

	struct proc_dir_entry	*cec_proc_dir;
	struct proc_dir_entry	*cec_proc_wakeup_test;
};

/**
 * @brief Dynamic memory allocation
 * Instance 0 will have the total size allocated so far and also the number
 * of calls to this function (number of allocated instances)
 */
struct mem_alloc{
	int instance;
	const char *info;
	size_t size;
	void *pointer; // the pointer allocated by this instance
	struct mem_alloc *last;
	struct mem_alloc *prev;
};
#endif
