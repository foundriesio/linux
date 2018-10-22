/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        cec.h
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


