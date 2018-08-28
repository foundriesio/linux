/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        include.h
*  \brief       HDMI CEC controller driver
*  \details   
*  \version     1.0
*  \date        2014-2015
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
	volatile void __iomem *pmu_base;

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
