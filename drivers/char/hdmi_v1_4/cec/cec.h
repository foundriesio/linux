// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
        unsigned int open_cnt;
        struct device *pdev;

        /* HDMI CEC Clock */
        struct clk *pclk;
        struct clk *hclk;
        struct clk *ipclk;

        struct miscdevice *misc;

        int cec_irq;
        
        unsigned int suspend;
        unsigned int runtime_suspend;
        /* it is indicate to cec driver was resume */
        unsigned int resume;
       
        /** Register interface */
        volatile void __iomem   *hdmi_ctrl_io;
        volatile void __iomem   *hdmi_cec_io;

        volatile unsigned long  status;

        struct cec_rx_struct rx;
        struct cec_tx_struct tx;

        /* CEC Input Device Interface */
        struct input_dev *fake_ir_dev;  
};

#endif /* __TCC_HDMI_CEC_H__ */


