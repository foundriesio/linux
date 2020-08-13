// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
