/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi.c
*  \brief       HDMI TX controller driver
*  \details   
*  \version     1.0
*  \date        2014-2015
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
#define HDMI_TX_INTERRUPT_HANDLER_ON    4
#define HDMI_TX_HOTPLUG_STATUS_LOCK     7
#define HDMI_TX_STATUS_PHY_ALIVE        8
#define HDMI_TX_STATUS_SUSPEND_L0      10       // Level 0) AV Mute
#define HDMI_TX_STATUS_SUSPEND_L1      11       // Level 1) Request PM Runtime_Suspend/Resume
#define HDMI_TX_STATUS_SUSPEND_L2      12       // Level 2) Suspend/Resume



#if defined(CONFIG_PLATFORM_AVN)
struct hdmi_hotplug_uevent_t {
        int hotplug_status;
        struct device *pdev;
        struct work_struct work;
};
#endif

struct tcc_hdmi_dev {
        unsigned int open_cnt;
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

        /** Register interface */
        volatile void __iomem   *hdmi_ctrl_io;
        volatile void __iomem   *hdmi_core_io;
        volatile void __iomem   *hdmi_phy_io;
        volatile void __iomem   *ddibus_io;

        struct HDMIVideoParameter video_params;
        int power_status;
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
