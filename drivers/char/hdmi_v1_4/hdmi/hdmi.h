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
#ifndef __TCC_HDMI_H__
#define __TCC_HDMI_H__

#define HDMI_TX_STATUS_POWER_ON         0
#define HDMI_TX_STATUS_OUTPUT_ON        1
#define HDMI_TX_STATUS_STARTUP          2
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
        struct clk *hdmi_ref_clk;
        struct clk *hdmi_ddibus_clk;
        struct clk *hdmi_phy_apb_clk;
        struct clk *pmu_iso_clk;
	struct clk *hdmi_audio_clk;
	struct clk *hdmi_audio_spdif_clk;

        struct miscdevice *misc;

        #if defined(CONFIG_PLATFORM_AVN)
        struct work_struct hdmi_output_event_work;
        #endif

        int phy_depth; /* 0: 24-bit, 1: 30-bit, 2: 36-bit */

        int suspend;
        int os_suspend;
        int runtime_suspend;

        /** Register interface */
        volatile void __iomem   *hdmi_ctrl_io;
        volatile void __iomem   *hdmi_core_io;
        volatile void __iomem   *hdmi_phy_io;
        volatile void __iomem   *hdmi_hdcp_io;
        volatile void __iomem   *hdmi_rgb_io;
        volatile void __iomem   *hdmi_aes_io;
	volatile void __iomem   *hdmi_spdif_io;
	volatile void __iomem   *hdmi_i2s_io;
        volatile void __iomem   *ddibus_io;

	/* Interrupt */
	int irq;

        /** power */
        int power_status;
        int power_enable_count;

        int hdmi_started;

        //unsigned int color_range; /* Full or Limited Range */
        int spdif_audio;

        unsigned int audio_channel;
        unsigned int audio_sample_freq;
        unsigned int audio_packet;

        #ifdef CONFIG_REGULATOR
        struct regulator *vdd_v5p0;
        struct regulator *vdd_hdmi;
        #endif

        videoParams_t videoParam;
	packetParam_t packetParam;
	audioParams_t audioParam;

	struct proc_dir_entry	*hdmi_proc_dir;
        struct proc_dir_entry   *hdmi_proc_sampling_frequency;

        volatile unsigned long  status;
};


/* Local Global APIs */
/** ddibus access */
unsigned int ddi_reg_read(struct tcc_hdmi_dev *dev, unsigned int offset);
void ddi_reg_write(struct tcc_hdmi_dev *dev, unsigned int data, unsigned int offset);
void tcc_ddi_swreset_hdmi(struct tcc_hdmi_dev *dev, char onoff);

/* hdmi access */
unsigned int hdmi_reg_read(struct tcc_hdmi_dev *dev, unsigned int offset);
void hdmi_reg_write(struct tcc_hdmi_dev *dev, unsigned int data, unsigned int offset);

void hdmi_aui_update_checksum(struct tcc_hdmi_dev *dev);

/* Video config */
int hdmi_video_config(struct tcc_hdmi_dev *dev);

/* Packet config */
int hdmi_set_spd_infoframe(struct tcc_hdmi_dev *dev);
int hdmi_set_vsif_infoframe(struct tcc_hdmi_dev *dev);


int hdmi_get_power_status(struct tcc_hdmi_dev *dev);
int hdmi_get_VBlank_internal(struct tcc_hdmi_dev *dev);

int hdmi_av_mute(struct tcc_hdmi_dev *dev, int av_mute);
void hdmi_start_internal(struct tcc_hdmi_dev *dev);
void hdmi_stop_internal(struct tcc_hdmi_dev *dev);

/* Power Config */
void tcc_hdmi_power_on(struct tcc_hdmi_dev *dev);
void tcc_hdmi_power_off(struct tcc_hdmi_dev *dev);

/* HDMI Detailed Timing */
#include <video/tcc/tccfb_ioctrl.h> /* Defines lcdc_timimg_parms_t and tcc_fb_extra_data */
int hdmi_dtd_fill(dtd_t * dtd, unsigned int code, unsigned int refreshRate);
unsigned int hdmi_dtd_get_refresh_rate(dtd_t *dtd);
unsigned int hdmi_dtd_get_vactive(videoParams_t *videoParam);
int hdmi_dtd_get_display_param(videoParams_t *videoParam,
		struct lcdc_timimg_parms_t *lcdc_timimg_parms);
int hdmi_dtb_get_extra_data(videoParams_t *videoParam,
		struct tcc_fb_extra_data *tcc_fb_extra_data);

/* HDMI PHY Pixel_clocks */
void hdmi_phy_reset(struct tcc_hdmi_dev *dev);
int tcc_hdmi_phy_power_down(struct tcc_hdmi_dev *dev);
int tcc_hdmi_phy_config_by_rawdata(struct tcc_hdmi_dev *dev, unsigned char* phy_regs, unsigned int len);
int tcc_hdmi_phy_config(struct tcc_hdmi_dev *dev, unsigned int pixel_clock, unsigned int bpp);

int hdmi_audio_set_mode(struct tcc_hdmi_dev *dev);

void hdmi_api_register_dev(struct tcc_hdmi_dev *dev);

int hdmi_audio_set_enable(struct tcc_hdmi_dev *dev, int enable);

void proc_interface_init(struct tcc_hdmi_dev *dev);
void proc_interface_remove(struct tcc_hdmi_dev *dev);
#endif
