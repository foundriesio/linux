/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi.h
*  \brief       HDMI controller driver
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
#ifndef __TCC_HDMI_H__
#define __TCC_HDMI_H__

#define HDMI_TX_STATUS_POWER_ON         0
#define HDMI_TX_STATUS_OUTPUT_ON        1
#define HDMI_TX_STATUS_STARTUP          2       // tcc_output_starter_hdmi_v2_0 
#define HDMI_SWITCH_STATUS_ON           3


#if defined(CONFIG_PLATFORM_AVN)
struct hdmi_hotplug_uevent_t {
        int hotplug_status;
        struct device *pdev;
        struct work_struct work;
};
#endif

struct tcc_hdmi_dev {
        int open_cnt;
        struct device *pdev;

        /* HDMI Clock */
        struct clk *pclk;
        struct clk *hclk;
        struct clk *ipclk;

        struct miscdevice *misc;

        #if defined(CONFIG_PLATFORM_AVN)
        struct work_struct hdmi_output_event_work;
        #endif
        
        int phy_depth; /* 0: 24-bit, 1: 30-bit, 2: 36-bit */
        
        unsigned int suspend;
        unsigned int os_suspend;
        unsigned int runtime_suspend;

        /** Register interface */
        volatile void __iomem   *hdmi_ctrl_io;
        volatile void __iomem   *hdmi_core_io;
        volatile void __iomem   *hdmi_phy_io;
        volatile void __iomem   *ddibus_io;

        struct HDMIVideoParameter video_params;

        /** power */
        int power_status;   
        int power_enable_count;
        
        unsigned int hdmi_started;
        unsigned int color_range; /* Full or Limited Range */
        unsigned int audio_channel;
        unsigned int audio_sample_freq;
        unsigned int audio_packet;

        #ifdef CONFIG_REGULATOR
        struct regulator *vdd_v5p0;
        struct regulator *vdd_hdmi;
        #endif

        volatile unsigned long  status;
};


#endif
