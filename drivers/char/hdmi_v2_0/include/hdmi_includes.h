/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        include.h
*  \brief       HDMI TX controller driver
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
#ifndef __INCLUDES_H__
#define __INCLUDES_H__

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
#include <linux/of_address.h> // of_iomap
#include <linux/poll.h> // poll_table
#include <linux/device.h> // dev_xet_drv_data
#include <asm/io.h>

#define HDMI_DRV_VERSION        "4.14_1.0.0"

// HDMI COMPONENTS
#define PROTO_HDMI_TX_CORE      0
#define PROTO_HDMI_TX_HDCP      1
#define PROTO_HDMI_TX_PHY       2
#define PROTO_HDMI_TX_I2C_MAP   3
#define PROTO_HDMI_TX_IO_BASE   4
#define PROTO_HDMI_TX_MAX       5

// HDMI IRQS
#define HDMI_IRQ_TX_CORE        0
#define HDMI_IRQ_TX_HDCP        1
#if defined(CONFIG_HDMI_USE_CEC_IRQ)
#define HDMI_IRQ_TX_CEC         2
#define HDMI_IRQ_TX_MAX         3
#else
#define HDMI_IRQ_TX_MAX         2
#endif

// HDMI CLOCKS
#define HDMI_CLK_INDEX_PERI     0
#define HDMI_CLK_INDEX_SPDIF    1        
#define HDMI_CLK_INDEX_APB      2
#define HDMI_CLK_INDEX_CEC      3
#define HDMI_CLK_INDEX_HDCP14   4
#define HDMI_CLK_INDEX_HDCP22   5
#define HDMI_CLK_INDEX_DDIBUS   6
#define HDMI_CLK_INDEX_ISOIP    7
#define HDMI_CLK_INDEX_MAX      8


// STATUS BITS
#define HDMI_STATUS_BIT_CORE_POWERON      1


// CLOCK FREQ
#define HDMI_PHY_REF_CLK_RATE   (24000000)
#define HDMI_HDCP14_CLK_RATE    (36000000)
/** 
 * DDC_SFRCLK is HDMI_HDCP14_CLK_RATE divided by 20000.
 */
#define HDMI_DDC_SFRCLK         (1800) 
#define HDMI_HDCP22_CLK_RATE    (50000000)
#define HDMI_SPDIF_REF_CLK_RATE (44100*64*2)

/**
 * This macro only works in C99 so you can disable this, by placing
 * #define FUNC_NAME "HDMI_RX", or __FUNCTION__
 */
#define FUNC_NAME __func__

/**
 * Verbose levels
 */
typedef enum {
	VERBOSE_BASIC = 1,
	VERBOSE_IO,
	VERBOSE_IRQ
}verbose_level;


enum {
        MUTE_ID_IRQ=1,
        MUTE_ID_API,
        MUTE_ID_HPD,
        MUTE_ID_DDC,

};

enum {
        hdmi_phy_8980_stb=0,
        hdmi_phy_8985_ott,
        hdmi_phy_8980_dv_0_1,
        hdmi_phy_8980_dv_lab1,
		hdmi_phy_8985_stk_tcm3830,
        hdmi_phy_type_max
};


/**
 * @short HDMI TX controller status information
 *
 * Initialize @b user fields (set status to zero).
 * After opening this data is for internal use only.
 */
struct hdmi_tx_ctrl {
        unsigned char csc_on;
        unsigned char audio_on;
        unsigned char cec_on;
        unsigned char hdcp_on;
        unsigned char data_enable_polarity;
        
        /* 
         * In general, HDCP Keepout is set to 1 when TMDS frequencyrk is higher than 
         * 340 MHz or when HDCP is enabled.
         * When HDCP Keepout is set to 1, the control period configuration is changed.
         * Exceptionally, if HDCP keepout is set to 0 for VIZIO TV, there is a problem 
         * of swinging HPD.
         * hdmi driver reads the EDID of the SINK and sets HDCP keepout to always 1 
         * if this SINK is a VIZIO TV. */
        unsigned char sink_is_vizio;

        unsigned int pixel_clock;
};


struct drv_enable_entry
{
    struct list_head list;
    unsigned int id;
};

/** HDMI Status */
#define HDMI_TX_STATUS_POWER_ON         0
#define HDMI_TX_STATUS_OUTPUT_ON        1
#define HDMI_TX_STATUS_STARTUP          2       // tcc_output_starter_hdmi_v2_0 
#define HDMI_TX_PRE_API_CONFIG          3
#define HDMI_TX_INTERRUPT_HANDLER_ON    4

#define HDMI_TX_HDR_VALID               5       
#define HDMI_TX_HLG_VALID               6

#define HDMI_TX_HOTPLUG_STATUS_LOCK     7

#define HDMI_TX_STATUS_PHY_ALIVE        8


#define HDMI_TX_STATUS_SUSPEND_L0      10       // Level 0) AV Mute
#define HDMI_TX_STATUS_SUSPEND_L1      11       // Level 1) Request PM Runtime_Suspend/Resume
#define HDMI_TX_STATUS_SUSPEND_L2      12       // Level 2) Suspend/Resume

#define HDMI_TX_STATUS_SCDC_CHECK      20 


struct irq_dev_id {
        void *dev;
};

/**
 * @short Main structures to instantiate the driver
 */
struct hdmi_tx_dev{

        /** Device node */
        struct device 		*parent_dev;

        /** Device list */
        struct list_head 	devlist;

        /** Misc Device */
        struct miscdevice       *misc;

        /** Verbose */
        int 			verbose;

        /** HDMI Status */
        volatile unsigned long  status;

        /** Interrupts */
        int		        irq[HDMI_IRQ_TX_MAX];
        struct irq_dev_id       irq_dev[HDMI_IRQ_TX_MAX];

        /** Clock */
        struct clk              *clk[HDMI_CLK_INDEX_MAX];

        unsigned int            clk_freq_apb;

        /** Device Open Count */
        int                     open_cs;
        int                     hdmi_clock_enable_count;
        int                     display_clock_enable_count;

        struct drv_enable_entry irq_enable_entry;
                
        #if defined(CONFIG_REGULATOR)
        struct regulator        *regulator;
        #endif

        /** Spinlock */
        spinlock_t 		slock;
        spinlock_t 		slock_power;
        
        /** Mutex - future use*/
        struct mutex 		mutex;

        /** Device Tree Information */
        char 			*device_name;
        /** HDMI TX Controller */
        volatile void __iomem   *dwc_hdmi_tx_core_io;

        /** HDCP */
        volatile void __iomem   *hdcp_io;

        /** HDMI TX PHY interface */
        volatile void __iomem   *hdmi_tx_phy_if_io;

        /** DDI_BUS interface */
        volatile void __iomem   *ddibus_io;

        /** CKC interface */
        volatile void __iomem   *io_bus;

        /** io offset of audio reg  */
        unsigned int	        hdmi_audio_if_sel_ofst;

        /** HDMIRX and TX Channel Mux Selection Register  */
        unsigned int	        hdmi_rx_tx_chmux;

        
        /** hdmi phy type  */
        unsigned int            hdmi_phy_type;  // 0 8980, 1 8985

        /** hdmi reference source clock */
        unsigned int            hdmi_ref_src_clk; 

        /** Proc file system */
        struct proc_dir_entry	*hdmi_proc_dir;
        struct proc_dir_entry   *hdmi_proc_hpd;
        struct proc_dir_entry   *hdmi_proc_hpd_lock;
        struct proc_dir_entry   *hdmi_proc_hdcp22;
	struct proc_dir_entry   *hdmi_proc_hdcp_status;
        struct proc_dir_entry   *hdmi_proc_scdc_check;
        struct proc_dir_entry   *hdmi_proc_ddc_check;
        
        #if defined(CONFIG_TCC_RUNTIME_DRM_TEST)
        struct proc_dir_entry   *hdmi_proc_drm;
        #endif

        #if defined(CONFIG_TCC_RUNTIME_GET_EDID_ID)
        struct proc_dir_entry   *hdmi_proc_edid_machine_id;
        #endif

        /** Supports variable PHY settings */
        #if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
        struct proc_dir_entry   *hdmi_proc_phy_regs;
        #endif
                

        /** Hot Plug */
        int                     hotplug_gpio;
        int                     hotplug_irq;
        /* Debugging purpose 
         * If this value set to 1 then hotplug_status is 
         * fixed with hotplug_real_status of that moment */
        int                     hotplug_locked;
        /* This variable represent real hotplug status */
        int                     hotplug_real_status;
        /* This variable represent virtual hotplug status 
         * If hotplug_locked was 0, It represent hotplug_real_status
         * On the other hand, If hotplug_locked was 1, it represent 
         * hotplug_real_status at the time of hotplug_locked was set to 1 */
        int                     hotplug_status;
        int                     hpd_enable;

        
        int                     rxsense;

        int                     hdcp22;
	int			hdcp_enable;
	int			hdcp_status;

        /* Set whether to use DDC.
         * Some boards may exclude the ddc line at the design time.
         * In such cases, you should set this variable to 1 to disable ddc communication */
        int                     ddc_disable;

        // Controller Information. 
        struct hdmi_tx_ctrl     hdmi_tx_ctrl;

        void                    *videoParam;
        void                    *audioParam;
        void                    *productParam;
        void                    *drmParm;

        /** uevent work */
        struct work_struct      hdmi_output_event_work;
        #if defined(CONFIG_TCC_RUNTIME_TUNE_HDMI_PHY)
        struct delayed_work      hdmi_restore_hotpug_work;
        #endif
                
        /** Interrupt Work */
        struct work_struct      tx_handler;
        struct work_struct      tx_hdcp_handler;
        struct work_struct      tx_hotplug_handler;

        /** support poll */
        wait_queue_head_t       poll_wq;

        /** Supports variable PHY settings */
        int                     edid_machine_id;  

        /* Source Product Description */
        char                    vendor_name[8];
        char                    product_description[16];
        unsigned int            source_information;
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

/**
 * @short Allocate memory for the driver
 * @param[in] into allocation name
 * @param[in] size allocation size
 * @param[in,out] allocated return structure for the allocation, may be NULL
 * @return if successful, the pointer to the new created memory
 * 	   if not, NULL
 */
void *
alloc_mem(char *info, size_t size, struct mem_alloc *allocated);


/**
 * hdmi misc api 
 */ 
void dwc_hdmi_set_hdcp_keepout(struct hdmi_tx_dev *dev);


#endif /* __INCLUDES_H__ */
