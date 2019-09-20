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

#define HDMI_CEC_VERSION        "1.0.1"

/** This constant is used to define index of IOBUS clocks */
#define HDMI_CLK_CEC_INDEX_IOBUS       0
/** This constant is used to define maximum number of clocks */
#define HDMI_CLK_CEC_INDEX_MAX         1

struct cec_buffer{
        /** This is buffers used for storing the data waiting for transmission */
	char	send_buf[CEC_TX_DATA_SIZE];
        /** This is buffers used for storing the received data */
	char	recv_buf[CEC_RX_DATA_SIZE];
        /** Number of transmission or received data */
	unsigned int size;
};

/**
 * @short This structure defines the device context of cec driver.
 */
struct cec_device{
        /** Device pointer to indicates device of platform device */
        struct device 		*parent_dev;

        /** Name of this driver */
        char 			*device_name;

        /** Array of cec clocks */
        struct clk              *clk[HDMI_CLK_CEC_INDEX_MAX];

        /** Count of clock enable */
        int                     clk_enable_count;

        /** Base address of CEC core register */
        volatile void __iomem *cec_core_io;

        /** Base address of CEC interrupt register */
        volatile void __iomem *cec_irq_io;

        /** Base address of CEC clock select register */
        volatile void __iomem *cec_clk_sel;

        /* IRQ number */
        /** CEC interrupt number */
        uint32_t		cec_irq;
        /** CEC wakeup interrupt number */
        uint32_t		cec_wake_up_irq;

        /**
         * It stores status of device standby or resume(normal)
         *  0: resume(normal)
         *  1: standby */
        unsigned int 	standby_status;

        /** Device pointer to miscellaneous device */
        struct miscdevice	*misc;

        /** Reference count for miscellaneous device */
        int                     reference_count;

        /** Device list **/
        struct list_head	devlist;

        /** Buffer for cec communication  */
        struct cec_buffer buf;

        /** It stores logical address */
        int			l_address;
        /** It stores physical address */
        int			p_address;
        /** Not used, It will be deprecated */
        int			cec_enable;
        /**
         * It stores whether cec interrupts are used as wakeup source.
         *  0: The cec interrupt is not wakeup source
         *  1: The cec interrupt is used to wakeup source
         *  n:                                              */
        int                     cec_wakeup_enable;

        struct proc_dir_entry	*cec_proc_dir;
        struct proc_dir_entry	*cec_proc_wakeup;

        struct mutex            mutex;
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
