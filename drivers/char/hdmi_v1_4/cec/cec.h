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
#ifndef __TCC_HDMI_CEC_H__
#define __TCC_HDMI_CEC_H__

/**
 * @struct dev->rx
 * Holds CEC Rx state and data
 */
struct cec_rx_struct {
        spinlock_t lock;
        wait_queue_head_t waitq;
        atomic_t state;
        char* buffer;
        unsigned int size;
        unsigned int flag;
};

/**
 * @struct dev->tx
 * Holds CEC Tx state and data
 */
struct cec_tx_struct {
        wait_queue_head_t waitq;
        atomic_t state;
        char* buffer;
};

struct tcc_hdmi_cec_dev {
        struct device *pdev;

        /* HDMI CEC Clock */
        struct clk *pclk;
	unsigned int pclk_freq;

        struct miscdevice *misc;

        int cec_irq;

        unsigned int suspend;
        unsigned int runtime_suspend;
        /* it is indicate to cec driver was resume */
        unsigned int resume;

        /** Register interface */
        volatile void __iomem   *hdmi_cec_io;

        struct cec_rx_struct rx;
        struct cec_tx_struct tx;

        int enable_cnt;
};

#endif /* __TCC_HDMI_CEC_H__ */


