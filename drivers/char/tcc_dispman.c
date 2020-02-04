/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/fb.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/div64.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_disp.h>

//static int debug	   		= 0;
//#define dprintk(msg...)	if(debug) { 	printk( "dispman:  " msg); 	}

#define DEVICE_NAME			"tcc_dispman"
#define MANAGER_MAJOR_ID	250
#define MANAGER_MINOR_ID 	1

static int tcc_dispman_major = MANAGER_MAJOR_ID;

enum {
	MGEM_TCC_HDMI_720P_FIXED = 0,
	MGEM_TCC_AUDIO_HDMI_LINK,
	MGEM_TCC_AUDIO_SAMPLING_RATE,
	MGEM_TCC_AUDIO_CHANNELS,
	MGEM_TCC_CEC_IMAGEVIEW_ON,
	MGEM_TCC_CEC_TEXTVIEW_ON,
	MGEM_TCC_CEC_ACTIVE_SOURCE,
	MGEM_TCC_CEC_IN_ACTIVE_SOURCE,
	MGEM_TCC_CEC_STANDBY,
	MGEM_TCC_CEC_CONNECTION,
	MGEM_TCC_HDMI_AUDIO_TYPE,
	MGEM_TCC_OUTPUT_DISPLAY_TYPE,
	MGEM_TCC_OUTPUT_HDMI_3D_FORMAT,
	MGEM_TCC_OUTPUT_HDMI_AUDIO_ONOFF,
	MGEM_TCC_OUTPUT_HDMI_AUDIO_DISABLE,
	MGEM_TCC_OUTPUT_HDMI_VIDEO_FORMAT,
	MGEM_TCC_OUTPUT_HDMI_STRUCTURE_3D,
	MGEM_TCC_OUTPUT_HDMI_SUPPORTED_RESOLUTION,
	MGEM_TCC_OUTPUT_HDMI_SUPPORTED_3D_MODE,
        MGEM_TCC_OUTPUT_HDMI_SUPPORTED_HDR,
	MGEM_TCC_VIDEO_HDMI_RESOLUTION,
	MGEM_TCC_OUTPUT_MODE_DETECTED,
	MGEM_TCC_OUTPUT_MODE_STB,
	MGEM_TCC_OUTPUT_MODE_PLUGOUT,
	MGEM_TCC_2D_COMPRESSION,

	MGEM_PERSIST_DISPLAY_MODE,
	MGEM_PERSIST_EXTENDDISPLAY_RESET,
	MGEM_PERSIST_OUTPUT_ATTACH_MAIN,
	MGEM_PERSIST_OUTPUT_ATTACH_SUB,
	MGEM_PERSIST_OUTPUT_MODE,
	MGEM_PERSIST_AUTO_RESOLUTION,
	MGEM_PERSIST_SPDIF_SETTING,
	MGEM_PERSIST_HDMI_MODE,
	MGEM_PERSIST_HDMI_RESIZE_UP,
	MGEM_PERSIST_HDMI_RESIZE_DN,
	MGEM_PERSIST_HDMI_RESIZE_LT,
	MGEM_PERSIST_HDMI_RESIZE_RT,
	MGEM_PERSIST_HDMI_CEC,
	MGEM_PERSIST_HDMI_RESOLUTION,
	MGEM_PERSIST_HDMI_DETECTED_RES,
	MGEM_PERSIST_HDMI_PRINTLOG,
	MGEM_PERSIST_HDMI_COLOR_DEPTH,
	MGEM_PERSIST_HDMI_COLOR_SPACE,
	MGEM_PERSIST_HDMI_COLORIMETRY,
	MGEM_PERSIST_HDMI_ASPECT_RATIO,
	MGEM_PERSIST_HDMI_DETECTED,
	MGEM_PERSIST_HDMI_DETECTED_MODE,
	MGEM_PERSIST_HDMI_EXTRA_MODE,
	MGEM_PERSIST_HDMI_REFRESH_RATE,
	MGEM_PERSIST_COMPOSITE_MODE,
	MGEM_PERSIST_COMPOSITE_RESIZE_UP,
	MGEM_PERSIST_COMPOSITE_RESIZE_DN,
	MGEM_PERSIST_COMPOSITE_RESIZE_LT,
	MGEM_PERSIST_COMPOSITE_RESIZE_RT,
	MGEM_PERSIST_COMPOSITE_DETECTED,
	MGEM_PERSIST_COMPONENT_MODE,
	MGEM_PERSIST_COMPONENT_RESIZE_UP,
	MGEM_PERSIST_COMPONENT_RESIZE_DN,
	MGEM_PERSIST_COMPONENT_RESIZE_LT,
	MGEM_PERSIST_COMPONENT_RESIZE_RT,
	MGEM_PERSIST_COMPONENT_DETECTED,
	MGEM_TCCDISPMAN_SUPPORT_RESOLUTION_COUNT,
	MGEM_TCC_HDCP_HDMI_ENABLE,		// for hdcp
	MGEM_PERSIST_HDMI_NATIVE_FIRST,
	MGEM_PERSIST_HDMI_HW_CTS,
	MGEM_TCC_SYS_OUTPUT_SECOND_ATTACH,
	MGEM_ATTR_MAX
};

#define SR_ATTR_MAX	500

static DEFINE_MUTEX(tcc_dispman_mutex);

static  atomic_t tcc_dispman_attribute_data[MGEM_ATTR_MAX];

// This is an old interface.. It will be deprecate
static  int  tcc_dispman_support_resolution_length = 0;
static  char tcc_dispman_support_resolution[SR_ATTR_MAX];

// This is a new interface for supporeted list of 3D and resolutions.
static  char tcc_dispman_supported_3d_mode[93];

static  char tcc_dispman_supported_resolution[93];
static  char tcc_dispman_supported_hdr[93];

extern struct lcd_panel *tccfb_get_panel(void);
extern unsigned int tca_get_lcd_lcdc_num(void);
extern unsigned int tca_get_output_lcdc_num(void);
#if defined(CONFIG_TCC_OUTPUT_STARTER)
extern char default_component_resolution;
#endif

// Support Lock_F
static  atomic_t tcc_dispman_lock_file;

static ssize_t tcc_hdmi_720p_fixed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_HDMI_720P_FIXED]));
}
static ssize_t tcc_hdmi_720p_fixed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_HDMI_720P_FIXED], data);
	return count;
}

static ssize_t tcc_audio_hdmi_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_AUDIO_HDMI_LINK]));
}
static ssize_t tcc_audio_hdmi_link_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_AUDIO_HDMI_LINK], data);
	return count;
}

static ssize_t tcc_audio_sampling_rate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_AUDIO_SAMPLING_RATE]));
}
static ssize_t tcc_audio_sampling_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_AUDIO_SAMPLING_RATE], data);
	return count;
}

static ssize_t tcc_audio_channels_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_AUDIO_CHANNELS]));
}

static ssize_t tcc_audio_channels_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_AUDIO_CHANNELS], data);
	return count;
}

static ssize_t tcc_cec_imageview_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_CEC_IMAGEVIEW_ON]));
}

static ssize_t tcc_cec_imageview_on_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_IMAGEVIEW_ON], data);
	return count;
}


static ssize_t tcc_cec_textview_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_CEC_TEXTVIEW_ON]));
}

static ssize_t tcc_cec_textview_on_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_TEXTVIEW_ON], data);
	return count;
}


static ssize_t tcc_cec_active_source_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_CEC_ACTIVE_SOURCE]));
}

static ssize_t tcc_cec_active_source_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_ACTIVE_SOURCE], data);
	return count;
}


static ssize_t tcc_cec_in_active_source_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_CEC_IN_ACTIVE_SOURCE]));
}

static ssize_t tcc_cec_in_active_source_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_IN_ACTIVE_SOURCE], data);
	return count;
}


static ssize_t tcc_cec_standby_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_CEC_STANDBY]));
}

static ssize_t tcc_cec_standby_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_STANDBY], data);
	return count;
}


static ssize_t tcc_cec_connection_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_CEC_CONNECTION]));
}

static ssize_t tcc_cec_connection_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_CONNECTION], data);
	return count;
}




static ssize_t tcc_hdmi_audio_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_HDMI_AUDIO_TYPE]));
}

static ssize_t tcc_hdmi_audio_type_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_HDMI_AUDIO_TYPE], data);
	return count;
}

static ssize_t tcc_output_display_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_DISPLAY_TYPE]));
}

static ssize_t tcc_output_display_type_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_DISPLAY_TYPE], data);
	return count;
}

static ssize_t tcc_output_hdmi_3d_format_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_3D_FORMAT]));
}

static ssize_t tcc_output_hdmi_3d_format_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_3D_FORMAT], data);
	return count;
}


static ssize_t tcc_output_hdmi_audio_onoff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_AUDIO_ONOFF]));
}

static ssize_t tcc_output_hdmi_audio_onoff_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_AUDIO_ONOFF], data);
	return count;
}

static ssize_t tcc_output_hdmi_audio_disable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_AUDIO_DISABLE]));
}

static ssize_t tcc_output_hdmi_audio_disable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_AUDIO_DISABLE], data);
	return count;
}

static ssize_t tcc_output_hdmi_video_format_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_VIDEO_FORMAT]));
}

static ssize_t tcc_output_hdmi_video_format_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_VIDEO_FORMAT], data);
	return count;
}


static ssize_t tcc_output_hdmi_structure_3d_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_STRUCTURE_3D]));
}

static ssize_t tcc_output_hdmi_structure_3d_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_STRUCTURE_3D], data);
	return count;
}


static ssize_t tcc_output_hdmi_supported_resolution_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int tcc_dispman_supported_resolution_length =
(int)atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_SUPPORTED_RESOLUTION]);
        ssize_t ret_size = 0;
        mutex_lock(&tcc_dispman_mutex);
        if(tcc_dispman_supported_resolution_length > 0) {
                memcpy(buf, tcc_dispman_supported_resolution, tcc_dispman_supported_resolution_length);
                ret_size = tcc_dispman_supported_resolution_length;
        }
        mutex_unlock(&tcc_dispman_mutex);
        return ret_size;
}

static ssize_t tcc_output_hdmi_supported_resolution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        mutex_lock(&tcc_dispman_mutex);
        if(count > sizeof(tcc_dispman_supported_resolution))
                count = sizeof(tcc_dispman_supported_resolution) -1;
        memcpy(tcc_dispman_supported_resolution, buf, count);
        tcc_dispman_supported_resolution[count] = 0;
        atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_SUPPORTED_RESOLUTION], count);
        mutex_unlock(&tcc_dispman_mutex);
        return count;

}

static ssize_t tcc_output_hdmi_supported_3d_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        int tcc_dispman_supported_3d_mode_lenght =
(int)atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_SUPPORTED_3D_MODE]);
        ssize_t ret_size = 0;
        mutex_lock(&tcc_dispman_mutex);
        if(tcc_dispman_supported_3d_mode_lenght > 0) {
                memcpy(buf, tcc_dispman_supported_3d_mode, tcc_dispman_supported_3d_mode_lenght);
                ret_size = tcc_dispman_supported_3d_mode_lenght;
        }
        mutex_unlock(&tcc_dispman_mutex);
        return ret_size;

}

static ssize_t tcc_output_hdmi_supported_3d_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        mutex_lock(&tcc_dispman_mutex);
        if(count > sizeof(tcc_dispman_supported_3d_mode))
                count = sizeof(tcc_dispman_supported_3d_mode) -1;
        memcpy(tcc_dispman_supported_3d_mode, buf, count);
        tcc_dispman_supported_3d_mode[count] = 0;
        atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_SUPPORTED_3D_MODE], count);
        mutex_unlock(&tcc_dispman_mutex);
        return count;
}

static ssize_t tcc_output_hdmi_supported_hdr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        ssize_t ret_size = 0;
        int tcc_dispman_supported_hdr_lenght =
                (int)atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_SUPPORTED_HDR]);
        mutex_lock(&tcc_dispman_mutex);
        if(tcc_dispman_supported_hdr_lenght > 0) {
                memcpy(buf, tcc_dispman_supported_hdr, tcc_dispman_supported_hdr_lenght);
                ret_size = tcc_dispman_supported_hdr_lenght;
        }
        mutex_unlock(&tcc_dispman_mutex);
        return ret_size;
}

static ssize_t tcc_output_hdmi_supported_hdr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        mutex_lock(&tcc_dispman_mutex);
        if(count > sizeof(tcc_dispman_supported_hdr))
                count = sizeof(tcc_dispman_supported_hdr) -1;
        memcpy(tcc_dispman_supported_hdr, buf, count);
        tcc_dispman_supported_hdr[count] = 0;
        atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_HDMI_SUPPORTED_HDR], count);
        mutex_unlock(&tcc_dispman_mutex);
        return count;
}

static ssize_t tcc_video_hdmi_resolution_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_VIDEO_HDMI_RESOLUTION]));
}

static ssize_t tcc_video_hdmi_resolution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_VIDEO_HDMI_RESOLUTION], data);
	return count;
}


static ssize_t tcc_output_mode_detected_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_DETECTED]));
}

static ssize_t tcc_output_mode_detected_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_DETECTED], data);
	return count;
}

static ssize_t tcc_output_panel_width_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_FB_VIOC
	ssize_t result_size;
	struct lcd_panel *panel = tccfb_get_panel();

	if(panel) {
		result_size = sprintf(buf, "%d\n", panel->xres);
	}
	else {
		result_size = sprintf(buf, "%d\n", 0);
	}

	return result_size;
#else
	return 0;
#endif
}

static ssize_t tcc_output_panel_height_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_FB_VIOC
	ssize_t result_size;
	struct lcd_panel *panel = tccfb_get_panel();

	if(panel) {
		result_size = sprintf(buf, "%d\n", panel->yres);
	}
	else {
		result_size = sprintf(buf, "%d\n", 0);
	}

	return result_size;
#else
	return 0;
#endif
}

static ssize_t tcc_output_dispdev_width_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	volatile void __iomem *pDISPBase = NULL;
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *ptccfb_info = NULL;
	struct tcc_dp_device *pdp_data = NULL;
	unsigned int lcd_width, lcd_height;
	ssize_t result_size;

	ptccfb_info = info->par;

	if((ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
	    || (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
		|| (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) ) {
		pdp_data = &ptccfb_info->pdata.Mdp_data;
	} else if((ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
		|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
		|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) ) {
		pdp_data = &ptccfb_info->pdata.Sdp_data;
	} else {
		pr_err("[ERR][DISP_MAN] %s Can't find  output , Main:%d, Sub :%d \n",
				__func__,
				ptccfb_info->pdata.Mdp_data.DispDeviceType,
				ptccfb_info->pdata.Sdp_data.DispDeviceType);

		return 0;
	}

	pDISPBase = pdp_data->ddc_info.virt_addr;
	VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);

	if(pdp_data) {
		result_size = sprintf(buf, "%d\n", lcd_width);
	}
	else {
		result_size = sprintf(buf, "%d\n", 0);
	}

	return result_size;
}

static ssize_t tcc_output_dispdev_height_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	volatile void __iomem *pDISPBase = NULL;
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *ptccfb_info = NULL;
	struct tcc_dp_device *pdp_data = NULL;
	unsigned int lcd_width, lcd_height;
	ssize_t result_size;

	ptccfb_info = info->par;

	if((ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
	    || (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
		|| (ptccfb_info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) ) {
		pdp_data = &ptccfb_info->pdata.Mdp_data;
	} else if((ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
		|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPOSITE)
		|| (ptccfb_info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_COMPONENT) ) {
		pdp_data = &ptccfb_info->pdata.Sdp_data;
	} else {
		pr_err("[ERR][DISP_MAN] %s Can't find  output , Main:%d, Sub :%d \n",
				__func__,
				ptccfb_info->pdata.Mdp_data.DispDeviceType,
				ptccfb_info->pdata.Sdp_data.DispDeviceType);

		return 0;
	}

	pDISPBase = pdp_data->ddc_info.virt_addr;
	VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);

	if(pdp_data) {
		result_size = sprintf(buf, "%d\n", lcd_height);
	}
	else {
		result_size = sprintf(buf, "%d\n", 0);
	}

	return result_size;
}

static ssize_t tcc_output_mode_stb_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_STB]));
}

static ssize_t tcc_output_mode_plugout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_PLUGOUT]));
}

static ssize_t tcc_output_mode_plugout_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_PLUGOUT], data);
	return count;
}

int tcc_2d_compression_read(void)
{
	return atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_2D_COMPRESSION]);
}
EXPORT_SYMBOL(tcc_2d_compression_read);

static ssize_t tcc_2d_compression_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	    return sprintf(buf, "%d\n", tcc_2d_compression_read());
}

static ssize_t tcc_2d_compression_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long data;
    int error = kstrtoul(buf, 10, &data);
    if (error)
        return error;
    //if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_2D_COMPRESSION], data);
    return count;
}

static DEVICE_ATTR(tcc_hdmi_720p_fixed, S_IRUGO|S_IWUSR|S_IWGRP, tcc_hdmi_720p_fixed_show, tcc_hdmi_720p_fixed_store);
static DEVICE_ATTR(tcc_audio_hdmi_link, S_IRUGO|S_IWUSR|S_IWGRP, tcc_audio_hdmi_link_show, tcc_audio_hdmi_link_store);
static DEVICE_ATTR(tcc_audio_sampling_rate, S_IRUGO|S_IWUSR|S_IWGRP, tcc_audio_sampling_rate_show, tcc_audio_sampling_rate_store);
static DEVICE_ATTR(tcc_audio_channels, S_IRUGO|S_IWUSR|S_IWGRP, tcc_audio_channels_show, tcc_audio_channels_store);
static DEVICE_ATTR(tcc_cec_imageview_on, S_IRUGO|S_IWUSR|S_IWGRP, tcc_cec_imageview_on_show, tcc_cec_imageview_on_store);
static DEVICE_ATTR(tcc_cec_textview_on , S_IRUGO|S_IWUSR|S_IWGRP, tcc_cec_textview_on_show, tcc_cec_textview_on_store);
static DEVICE_ATTR(tcc_cec_active_source, S_IRUGO|S_IWUSR|S_IWGRP, tcc_cec_active_source_show, tcc_cec_active_source_store);
static DEVICE_ATTR(tcc_cec_in_active_source, S_IRUGO|S_IWUSR|S_IWGRP, tcc_cec_in_active_source_show, tcc_cec_in_active_source_store);
static DEVICE_ATTR(tcc_cec_standby, S_IRUGO|S_IWUSR|S_IWGRP, tcc_cec_standby_show, tcc_cec_standby_store);
static DEVICE_ATTR(tcc_cec_connection, S_IRUGO|S_IWUSR|S_IWGRP, tcc_cec_connection_show, tcc_cec_connection_store);
static DEVICE_ATTR(tcc_hdmi_audio_type, S_IRUGO|S_IWUSR|S_IWGRP, tcc_hdmi_audio_type_show, tcc_hdmi_audio_type_store);
static DEVICE_ATTR(tcc_output_display_type, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_display_type_show, tcc_output_display_type_store);
static DEVICE_ATTR(tcc_output_hdmi_3d_format, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_3d_format_show, tcc_output_hdmi_3d_format_store);
static DEVICE_ATTR(tcc_output_hdmi_audio_onoff, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_audio_onoff_show, tcc_output_hdmi_audio_onoff_store);
static DEVICE_ATTR(tcc_output_hdmi_audio_disable, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_audio_disable_show, tcc_output_hdmi_audio_disable_store);
static DEVICE_ATTR(tcc_output_hdmi_video_format, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_video_format_show, tcc_output_hdmi_video_format_store);
static DEVICE_ATTR(tcc_output_hdmi_structure_3d, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_structure_3d_show, tcc_output_hdmi_structure_3d_store);
static DEVICE_ATTR(tcc_output_hdmi_supported_resolution, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_supported_resolution_show, tcc_output_hdmi_supported_resolution_store);
static DEVICE_ATTR(tcc_output_hdmi_supported_3d_mode, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_supported_3d_mode_show, tcc_output_hdmi_supported_3d_mode_store);
static DEVICE_ATTR(tcc_output_hdmi_supported_hdr, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_hdmi_supported_hdr_show, tcc_output_hdmi_supported_hdr_store);
static DEVICE_ATTR(tcc_video_hdmi_resolution, S_IRUGO|S_IWUSR|S_IWGRP, tcc_video_hdmi_resolution_show, tcc_video_hdmi_resolution_store);
static DEVICE_ATTR(tcc_output_mode_detected, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_mode_detected_show, tcc_output_mode_detected_store);

static DEVICE_ATTR(tcc_output_panel_width, S_IRUGO, tcc_output_panel_width_show, NULL);
static DEVICE_ATTR(tcc_output_panel_height, S_IRUGO, tcc_output_panel_height_show, NULL);
static DEVICE_ATTR(tcc_output_dispdev_width, S_IRUGO, tcc_output_dispdev_width_show, NULL);
static DEVICE_ATTR(tcc_output_dispdev_height, S_IRUGO, tcc_output_dispdev_height_show, NULL);

static DEVICE_ATTR(tcc_output_mode_stb, S_IRUGO, tcc_output_mode_stb_show, NULL);
static DEVICE_ATTR(tcc_output_mode_plugout, S_IRUGO|S_IWUSR|S_IWGRP, tcc_output_mode_plugout_show, tcc_output_mode_plugout_store);
static DEVICE_ATTR(tcc_2d_compression, S_IRUGO|S_IWUSR|S_IWGRP, tcc_2d_compression_show, tcc_2d_compression_store);

//for hdcp
static ssize_t tcc_hdcp_hdmi_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_HDCP_HDMI_ENABLE]));
}

static ssize_t tcc_hdcp_hdmi_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_HDCP_HDMI_ENABLE], data);
	return count;
}

static DEVICE_ATTR(tcc_hdcp_hdmi_enable, S_IRUGO|S_IWUSR|S_IWGRP, tcc_hdcp_hdmi_enable_show, tcc_hdcp_hdmi_enable_store);


// == COMMON ==

static ssize_t persist_hdcp1x_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_TCC_HDMI_HDCP_USED
	return sprintf(buf, "%d\n", 1);
#else
	return sprintf(buf, "%d\n", 0);
#endif
}

static ssize_t persist_display_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_DISPLAY_MODE]));
}

static ssize_t persist_extenddisplay_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_EXTENDDISPLAY_RESET]));
}

static ssize_t persist_extenddisplay_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_EXTENDDISPLAY_RESET], data);
	return count;
}

static ssize_t persist_output_attach_main_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_ATTACH_MAIN]));
}

static ssize_t persist_output_attach_main_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_ATTACH_MAIN], data);
	return count;
}

static ssize_t persist_output_attach_sub_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_ATTACH_SUB]));
}

static ssize_t persist_output_attach_sub_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_ATTACH_SUB], data);
	return count;
}


static ssize_t persist_output_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_MODE]));
}

static ssize_t persist_output_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_MODE], data);
	return count;
}


static ssize_t persist_auto_resolution_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_AUTO_RESOLUTION]));
}

static ssize_t persist_auto_resolution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_AUTO_RESOLUTION], data);
	return count;
}


static ssize_t persist_spdif_setting_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_SPDIF_SETTING]));
}

static ssize_t persist_spdif_setting_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_SPDIF_SETTING], data);
	return count;
}

static DEVICE_ATTR(persist_hdcp1x_enable, S_IRUGO, persist_hdcp1x_enable_show, NULL);
static DEVICE_ATTR(persist_display_mode, S_IRUGO, persist_display_mode_show, NULL);
static DEVICE_ATTR(persist_extenddisplay_reset, S_IRUGO|S_IWUSR|S_IWGRP, persist_extenddisplay_reset_show, persist_extenddisplay_reset_store);
static DEVICE_ATTR(persist_output_attach_main, S_IRUGO|S_IWUSR|S_IWGRP, persist_output_attach_main_show, persist_output_attach_main_store);
static DEVICE_ATTR(persist_output_attach_sub, S_IRUGO|S_IWUSR|S_IWGRP, persist_output_attach_sub_show, persist_output_attach_sub_store);
static DEVICE_ATTR(persist_output_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_output_mode_show, persist_output_mode_store);
static DEVICE_ATTR(persist_auto_resolution, S_IRUGO|S_IWUSR|S_IWGRP, persist_auto_resolution_show, persist_auto_resolution_store);
static DEVICE_ATTR(persist_spdif_setting, S_IRUGO|S_IWUSR|S_IWGRP, persist_spdif_setting_show, persist_spdif_setting_store);


//== HDMI ==

static ssize_t persist_hdmi_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_MODE]));
}

static ssize_t persist_hdmi_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_MODE], data);
	return count;
}

static ssize_t persist_hdmi_resize_up_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_UP]));
}

static ssize_t persist_hdmi_resize_up_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_UP], data);
	return count;
}

static ssize_t persist_hdmi_resize_dn_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_DN]));
}

static ssize_t persist_hdmi_resize_dn_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_DN], data);
	return count;
}


static ssize_t persist_hdmi_resize_lt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_LT]));
}

static ssize_t persist_hdmi_resize_lt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_LT], data);
	return count;
}

static ssize_t persist_hdmi_resize_rt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_RT]));
}

static ssize_t persist_hdmi_resize_rt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESIZE_RT], data);
	return count;
}


static ssize_t persist_hdmi_cec_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_CEC]));
}

static ssize_t persist_hdmi_cec_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_CEC], data);
	return count;
}



static ssize_t persist_hdmi_resolution_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESOLUTION]));
}

static ssize_t persist_hdmi_resolution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESOLUTION], data);
	return count;
}



static ssize_t persist_hdmi_detected_res_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED_RES]));
}

static ssize_t persist_hdmi_detected_res_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED_RES], data);
	return count;
}


static ssize_t persist_hdmi_printlog_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_PRINTLOG]));
}

static ssize_t persist_hdmi_printlog_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_PRINTLOG], data);
	return count;
}

static ssize_t persist_hdmi_color_depth_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLOR_DEPTH]));
}

static ssize_t persist_hdmi_color_depth_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLOR_DEPTH], data);
	return count;
}



static ssize_t persist_hdmi_color_space_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLOR_SPACE]));
}

static ssize_t persist_hdmi_color_space_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLOR_SPACE], data);
	return count;
}

static ssize_t persist_hdmi_colorimetry_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLORIMETRY]));
}

static ssize_t persist_hdmi_colorimetry_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        //if (data > 1) data = 1;
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLORIMETRY], data);
        return count;
}

static ssize_t persist_hdmi_aspect_ratio_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_ASPECT_RATIO]));
}

static ssize_t persist_hdmi_aspect_ratio_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_ASPECT_RATIO], data);
	return count;
}



static ssize_t persist_hdmi_detected_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED]));
}

static ssize_t persist_hdmi_detected_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data, prev_data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	prev_data = atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED]);
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED], data);
	return count;
}

static ssize_t persist_hdmi_detected_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED_MODE]));
}

static ssize_t persist_hdmi_detected_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_DETECTED_MODE], data);
	return count;
}

static ssize_t persist_hdmi_extra_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_EXTRA_MODE]));
}

static ssize_t persist_hdmi_extra_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        //if (data > 1) data = 1;
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_EXTRA_MODE], data);
        return count;
}


static ssize_t persist_hdmi_refresh_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_REFRESH_RATE]));
}

static ssize_t persist_hdmi_refresh_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        //if (data > 1) data = 1;
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_REFRESH_RATE], data);
        return count;
}

static DEVICE_ATTR(persist_hdmi_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_mode_show, persist_hdmi_mode_store);
static DEVICE_ATTR(persist_hdmi_resize_up, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_resize_up_show, persist_hdmi_resize_up_store);
static DEVICE_ATTR(persist_hdmi_resize_dn, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_resize_dn_show, persist_hdmi_resize_dn_store);
static DEVICE_ATTR(persist_hdmi_resize_lt, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_resize_lt_show, persist_hdmi_resize_lt_store);
static DEVICE_ATTR(persist_hdmi_resize_rt, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_resize_rt_show, persist_hdmi_resize_rt_store);
static DEVICE_ATTR(persist_hdmi_cec, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_cec_show, persist_hdmi_cec_store);
static DEVICE_ATTR(persist_hdmi_resolution, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_resolution_show, persist_hdmi_resolution_store);
static DEVICE_ATTR(persist_hdmi_detected_res, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_detected_res_show, persist_hdmi_detected_res_store);
static DEVICE_ATTR(persist_hdmi_printlog, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_printlog_show, persist_hdmi_printlog_store);
static DEVICE_ATTR(persist_hdmi_color_depth, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_color_depth_show, persist_hdmi_color_depth_store);
static DEVICE_ATTR(persist_hdmi_color_space, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_color_space_show, persist_hdmi_color_space_store);
static DEVICE_ATTR(persist_hdmi_colorimetry, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_colorimetry_show, persist_hdmi_colorimetry_store);
static DEVICE_ATTR(persist_hdmi_aspect_ratio, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_aspect_ratio_show, persist_hdmi_aspect_ratio_store);
static DEVICE_ATTR(persist_hdmi_detected, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_detected_show, persist_hdmi_detected_store);
static DEVICE_ATTR(persist_hdmi_detected_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_detected_mode_show, persist_hdmi_detected_mode_store);
static DEVICE_ATTR(persist_hdmi_extra_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_extra_mode_show, persist_hdmi_extra_mode_store);
static DEVICE_ATTR(persist_hdmi_refresh_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_hdmi_refresh_mode_show, persist_hdmi_refresh_mode_store);


//==COMPOSITE==

static ssize_t persist_composite_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_MODE]));
}

static ssize_t persist_composite_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_MODE], data);
	return count;
}


static ssize_t persist_composite_resize_up_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_UP]));
}

static ssize_t persist_composite_resize_up_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_UP], data);
	return count;
}


static ssize_t persist_composite_resize_dn_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_DN]));
}

static ssize_t persist_composite_resize_dn_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_DN], data);
	return count;
}

static ssize_t persist_composite_resize_lt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_LT]));
}

static ssize_t persist_composite_resize_lt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_LT], data);
	return count;
}

static ssize_t persist_composite_resize_rt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_RT]));
}

static ssize_t persist_composite_resize_rt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_RESIZE_RT], data);
	return count;
}

static ssize_t persist_composite_detected_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_DETECTED]));
}

static ssize_t persist_composite_detected_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data, prev_data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	prev_data = atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_DETECTED]);
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPOSITE_DETECTED], data);
	return count;
}

static DEVICE_ATTR(persist_composite_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_composite_mode_show, persist_composite_mode_store);
static DEVICE_ATTR(persist_composite_resize_up, S_IRUGO|S_IWUSR|S_IWGRP, persist_composite_resize_up_show, persist_composite_resize_up_store);
static DEVICE_ATTR(persist_composite_resize_dn, S_IRUGO|S_IWUSR|S_IWGRP, persist_composite_resize_dn_show, persist_composite_resize_dn_store);
static DEVICE_ATTR(persist_composite_resize_lt, S_IRUGO|S_IWUSR|S_IWGRP, persist_composite_resize_lt_show, persist_composite_resize_lt_store);
static DEVICE_ATTR(persist_composite_resize_rt, S_IRUGO|S_IWUSR|S_IWGRP, persist_composite_resize_rt_show, persist_composite_resize_rt_store);
static DEVICE_ATTR(persist_composite_detected, S_IRUGO|S_IWUSR|S_IWGRP, persist_composite_detected_show, persist_composite_detected_store);


//==COMPONENT==
static ssize_t persist_component_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_MODE]));
}

static ssize_t persist_component_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_MODE], data);
	return count;
}


static ssize_t persist_component_resize_up_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_UP]));
}

static ssize_t persist_component_resize_up_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_UP], data);
	return count;
}


static ssize_t persist_component_resize_dn_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_DN]));
}

static ssize_t persist_component_resize_dn_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_DN], data);
	return count;
}


static ssize_t persist_component_resize_lt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_LT]));
}

static ssize_t persist_component_resize_lt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_LT], data);
	return count;
}

static ssize_t persist_component_resize_rt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_RT]));
}

static ssize_t persist_component_resize_rt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_RESIZE_RT], data);
	return count;
}

static ssize_t persist_component_detected_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_DETECTED]));
}

static ssize_t persist_component_detected_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data, prev_data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	prev_data = atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_DETECTED]);
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_DETECTED], data);
	return count;
}
static DEVICE_ATTR(persist_component_mode, S_IRUGO|S_IWUSR|S_IWGRP, persist_component_mode_show, persist_component_mode_store);
static DEVICE_ATTR(persist_component_resize_up, S_IRUGO|S_IWUSR|S_IWGRP, persist_component_resize_up_show, persist_component_resize_up_store);
static DEVICE_ATTR(persist_component_resize_dn, S_IRUGO|S_IWUSR|S_IWGRP, persist_component_resize_dn_show, persist_component_resize_dn_store);
static DEVICE_ATTR(persist_component_resize_lt, S_IRUGO|S_IWUSR|S_IWGRP, persist_component_resize_lt_show, persist_component_resize_lt_store);
static DEVICE_ATTR(persist_component_resize_rt, S_IRUGO|S_IWUSR|S_IWGRP, persist_component_resize_rt_show, persist_component_resize_rt_store);
static DEVICE_ATTR(persist_component_detected, S_IRUGO|S_IWUSR|S_IWGRP, persist_component_detected_show, persist_component_detected_store);


static ssize_t persist_supported_resolution_count_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCCDISPMAN_SUPPORT_RESOLUTION_COUNT]));
}

static ssize_t persist_supported_resolution_count_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	//if (data > 1) data = 1;
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCCDISPMAN_SUPPORT_RESOLUTION_COUNT], data);
	return count;
}

static ssize_t persist_supported_resolution_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret_size = 0;
	mutex_lock(&tcc_dispman_mutex);
	if(tcc_dispman_support_resolution_length > 0) {
		memcpy(buf, tcc_dispman_support_resolution, sizeof(char) * tcc_dispman_support_resolution_length);
		ret_size = tcc_dispman_support_resolution_length;
	}
	mutex_unlock(&tcc_dispman_mutex);
	return ret_size;
}

static ssize_t persist_supported_resolution_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&tcc_dispman_mutex);
	memset(tcc_dispman_support_resolution, 0, sizeof(tcc_dispman_support_resolution));
	memcpy(tcc_dispman_support_resolution, buf, sizeof(char) * count);
	tcc_dispman_support_resolution_length = count;
	mutex_unlock(&tcc_dispman_mutex);
	return count;
}

static DEVICE_ATTR(persist_supported_resolution_count, S_IRUGO|S_IWUSR|S_IWGRP, persist_supported_resolution_count_show, persist_supported_resolution_count_store);
static DEVICE_ATTR(persist_supported_resolution, S_IRUGO|S_IWUSR|S_IWGRP, persist_supported_resolution_show, persist_supported_resolution_store);

static ssize_t hdmi_native_first_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_NATIVE_FIRST]));
}

static ssize_t hdmi_native_first_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        //if (data > 1) data = 1;
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_NATIVE_FIRST], data);
        return count;

}
static DEVICE_ATTR(persist_hdmi_native_first, S_IRUGO|S_IWUSR|S_IWGRP, hdmi_native_first_show, hdmi_native_first_store);



static ssize_t hdmi_hw_cts_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_HW_CTS]));
}

static ssize_t hdmi_hw_cts_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        //if (data > 1) data = 1;
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_HW_CTS], data);
        return count;

}
static DEVICE_ATTR(persist_hdmi_hw_cts, S_IRUGO|S_IWUSR|S_IWGRP, hdmi_hw_cts_show, hdmi_hw_cts_store);




static ssize_t tcc_dispman_lock_file_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_lock_file));
}

static ssize_t tcc_dispman_lock_file_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        //if (data > 1) data = 1;
        atomic_set(&tcc_dispman_lock_file, data);
        return count;

}
static DEVICE_ATTR(tcc_dispman_lock_file, S_IRUGO|S_IWUSR|S_IWGRP, tcc_dispman_lock_file_show, tcc_dispman_lock_file_store);

static ssize_t tcc_sys_output_second_attach_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%d\n", atomic_read(&tcc_dispman_attribute_data[MGEM_TCC_SYS_OUTPUT_SECOND_ATTACH]));
}

static ssize_t tcc_sys_output_second_attach_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
        unsigned long data;
        int error = kstrtoul(buf, 10, &data);
        if (error)
                return error;
        atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_SYS_OUTPUT_SECOND_ATTACH], data?1:0);
        return count;

        return count;
}

static DEVICE_ATTR(tcc_sys_output_second_attach, S_IRUGO|S_IWUSR|S_IWGRP, tcc_sys_output_second_attach_show, tcc_sys_output_second_attach_store);


static struct attribute *tcc_dispman_attributes[] = {
	&dev_attr_tcc_hdmi_720p_fixed.attr,
	&dev_attr_tcc_audio_hdmi_link.attr,
	&dev_attr_tcc_audio_sampling_rate.attr,
	&dev_attr_tcc_audio_channels.attr,
	&dev_attr_tcc_cec_imageview_on.attr,
	&dev_attr_tcc_cec_textview_on.attr,
	&dev_attr_tcc_cec_active_source.attr,
	&dev_attr_tcc_cec_in_active_source.attr,
	&dev_attr_tcc_cec_standby.attr,
	&dev_attr_tcc_cec_connection.attr,
	&dev_attr_tcc_hdmi_audio_type.attr,
	&dev_attr_tcc_output_display_type.attr,
	&dev_attr_tcc_output_hdmi_3d_format.attr,
	&dev_attr_tcc_output_hdmi_audio_onoff.attr,
	&dev_attr_tcc_output_hdmi_audio_disable.attr,
	&dev_attr_tcc_output_hdmi_video_format.attr,
	&dev_attr_tcc_output_hdmi_structure_3d.attr,
	&dev_attr_tcc_output_hdmi_supported_resolution.attr,
	&dev_attr_tcc_output_hdmi_supported_3d_mode.attr,
        &dev_attr_tcc_output_hdmi_supported_hdr.attr,
	&dev_attr_tcc_video_hdmi_resolution.attr,
	&dev_attr_tcc_output_mode_detected.attr,
	&dev_attr_tcc_output_panel_width.attr,
	&dev_attr_tcc_output_panel_height.attr,
	&dev_attr_tcc_output_dispdev_width.attr,
	&dev_attr_tcc_output_dispdev_height.attr,
	&dev_attr_tcc_output_mode_stb.attr,
	&dev_attr_tcc_output_mode_plugout.attr,
	&dev_attr_tcc_2d_compression.attr,
	&dev_attr_persist_hdcp1x_enable.attr,
	&dev_attr_persist_display_mode.attr,
	&dev_attr_persist_extenddisplay_reset.attr,
	&dev_attr_persist_output_attach_main.attr,
	&dev_attr_persist_output_attach_sub.attr,
	&dev_attr_persist_output_mode.attr,
	&dev_attr_persist_auto_resolution.attr,
	&dev_attr_persist_spdif_setting.attr,
	&dev_attr_persist_hdmi_mode.attr,
	&dev_attr_persist_hdmi_resize_up.attr,
	&dev_attr_persist_hdmi_resize_dn.attr,
	&dev_attr_persist_hdmi_resize_lt.attr,
	&dev_attr_persist_hdmi_resize_rt.attr,
	&dev_attr_persist_hdmi_cec.attr,
	&dev_attr_persist_hdmi_resolution.attr,
	&dev_attr_persist_hdmi_detected_res.attr,
	&dev_attr_persist_hdmi_printlog.attr,
	&dev_attr_persist_hdmi_color_depth.attr,
	&dev_attr_persist_hdmi_color_space.attr,
	&dev_attr_persist_hdmi_colorimetry.attr,
	&dev_attr_persist_hdmi_aspect_ratio.attr,
	&dev_attr_persist_hdmi_detected.attr,
	&dev_attr_persist_hdmi_detected_mode.attr,
	&dev_attr_persist_hdmi_extra_mode.attr,
	&dev_attr_persist_hdmi_refresh_mode.attr,
	&dev_attr_persist_composite_mode.attr,
	&dev_attr_persist_composite_resize_up.attr,
	&dev_attr_persist_composite_resize_dn.attr,
	&dev_attr_persist_composite_resize_lt.attr,
	&dev_attr_persist_composite_resize_rt.attr,
	&dev_attr_persist_composite_detected.attr,
	&dev_attr_persist_component_mode.attr,
	&dev_attr_persist_component_resize_up.attr,
	&dev_attr_persist_component_resize_dn.attr,
	&dev_attr_persist_component_resize_lt.attr,
	&dev_attr_persist_component_resize_rt.attr,
	&dev_attr_persist_supported_resolution.attr,
	&dev_attr_persist_component_detected.attr,
	&dev_attr_persist_supported_resolution_count.attr,
	&dev_attr_persist_hdmi_native_first.attr,
	&dev_attr_persist_hdmi_hw_cts.attr,
	&dev_attr_tcc_dispman_lock_file.attr,
	&dev_attr_tcc_sys_output_second_attach.attr,
	&dev_attr_tcc_hdcp_hdmi_enable.attr, //for hdcp
	NULL
};


static struct attribute_group tcc_dispman_attribute_group = {
	.name = NULL,		/* put in device directory */
	.attrs = tcc_dispman_attributes,
};

int set_persist_display_mode(int persist_display_mode)
{
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_DISPLAY_MODE], (unsigned long)persist_display_mode);
	pr_info("[INF][DISP_MAN] set persist_display_mode(%d)\n", atomic_read(&tcc_dispman_attribute_data[MGEM_PERSIST_DISPLAY_MODE]));
	return 0;
}

extern int range_is_allowed(unsigned long pfn, unsigned long size);
static int tcc_dispman_mmap(struct file *file, struct vm_area_struct *vma)
{
	if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0){
		pr_err("[ERR][DISP_MAN] %s():  This address is not allowed. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
	{
		pr_err("[ERR][DISP_MAN] %s():  Virtual address page port error. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_ops	= NULL;
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	return 0;
}

static unsigned int tcc_dispman_poll(struct file *filp, poll_table *wait)
{
#if 1
	return 0;
#else
	int ret = 0;
	intr_data_t *msc_data = (intr_data_t *)filp->private_data;

	if (msc_data == NULL) {
		return -EFAULT;
	}

	poll_wait(filp, &(msc_data->poll_wq), wait);

	spin_lock_irq(&(msc_data->poll_lock));

	if (msc_data->block_operating == 0) 	{
		ret =  (POLLIN|POLLRDNORM);
	}

	spin_unlock_irq(&(msc_data->poll_lock));

	return ret;
#endif
}

long tcc_dispman_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	return 0;
}
EXPORT_SYMBOL(tcc_dispman_ioctl);

int tcc_dispman_release(struct inode *inode, struct file *filp)
{
	return 0;
}
EXPORT_SYMBOL(tcc_dispman_release);

int tcc_dispman_open(struct inode *inode, struct file *filp)
{
	return 0;
}
EXPORT_SYMBOL(tcc_dispman_open);


static struct file_operations tcc_dispman_fops =
{
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= tcc_dispman_ioctl,
	.mmap			= tcc_dispman_mmap,
	.open			= tcc_dispman_open,
	.release		= tcc_dispman_release,
	.poll			= tcc_dispman_poll,
};




static char banner[] __initdata = KERN_INFO "dispman driver initializing....... \n";
static struct class *tcc_dispman_class;

void __exit tcc_dispman_cleanup(void)
{
	unregister_chrdev(tcc_dispman_major, DEVICE_NAME);
	device_destroy(tcc_dispman_class, MKDEV(tcc_dispman_major, MANAGER_MINOR_ID));
	class_destroy(tcc_dispman_class);

	return;
}
static struct device *tcc_dispman_dev;

int __init tcc_dispman_init(void)
{
	int ret;
	printk(banner);

	memset(tcc_dispman_attribute_data, 0, sizeof(tcc_dispman_attribute_data));

	tcc_dispman_major = MANAGER_MAJOR_ID;

	if (register_chrdev(tcc_dispman_major, DEVICE_NAME, &tcc_dispman_fops)) {
		tcc_dispman_major = register_chrdev(0, DEVICE_NAME, &tcc_dispman_fops);
		if (tcc_dispman_major <= 0) {
			pr_err("[ERR][DISP_MAN] unable to register tcc_dispman device\n");
			return -EIO;
		}
		pr_err("[ERR][DISP_MAN] unable to register major %d. Registered %d instead\n", MANAGER_MAJOR_ID, tcc_dispman_major);
	}

	tcc_dispman_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(tcc_dispman_class)) {
		unregister_chrdev(tcc_dispman_major, DEVICE_NAME);
		return PTR_ERR(tcc_dispman_class);
	}

#if defined(CONFIG_TCC_DISPLAY_MODE_USE)
	#if defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_DISPLAY_MODE], (unsigned long)1);
	#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CVBS)
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_DISPLAY_MODE], (unsigned long)2);
	#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_DISPLAY_MODE], (unsigned long)3);
	#endif
#endif
        // Support Lock_F
        atomic_set(&tcc_dispman_lock_file, (unsigned long)0);

	// HDMI AUTO RESOLUTION

        #if defined(CONFIG_LCD_HDMI1920X720_ADV7613)
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESOLUTION], (unsigned long)16);
        #else
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_RESOLUTION], (unsigned long)125);
        #endif
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_VIDEO_HDMI_RESOLUTION], (unsigned long)999);

	#ifdef CONFIG_TCC_DISPLAY_MODE_USE
		pr_info("[INF][DISP_MAN] %s - STB Mode\n", __func__);
		// STB MODE
		atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_STB], (unsigned long)1);

		// PLUGOUT
		atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_OUTPUT_MODE_PLUGOUT], (unsigned long)0);

		// HDMI OUTPUT
		atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_OUTPUT_MODE], (unsigned long)1);
	#endif

	#if defined(CONFIG_TCC_DISPLAY_HDMI_LVDS)
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_SYS_OUTPUT_SECOND_ATTACH], 1);
	#endif

	// auto color space
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLOR_SPACE], (unsigned long)125);

        // auto colorimetry
	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_COLORIMETRY], (unsigned long)125);

	atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_REFRESH_RATE], (unsigned long)0); // default refresh is HZ/1.00

        // support hdr & hlg
        atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_HDMI_EXTRA_MODE], (unsigned long)3);

	//cec connection information
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_CEC_CONNECTION], (unsigned long)0);

	// TCC HDCP enable
	atomic_set(&tcc_dispman_attribute_data[MGEM_TCC_HDCP_HDMI_ENABLE], 0);

	#if defined(CONFIG_TCC_OUTPUT_STARTER)
	if (default_component_resolution == 3/*STARTER_COMPONENT_1080I*/) {
		atomic_set(&tcc_dispman_attribute_data[MGEM_PERSIST_COMPONENT_MODE], 1);
	}
	#endif

	tcc_dispman_dev = device_create(tcc_dispman_class, NULL,MKDEV(tcc_dispman_major, MANAGER_MINOR_ID), NULL, DEVICE_NAME);

	ret = sysfs_create_group(&tcc_dispman_dev->kobj, &tcc_dispman_attribute_group);
	if(ret)
		pr_err("[ERR][DISP_MAN] failed create sysfs\r\n");

	return 0;
}


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("dispman driver");
MODULE_LICENSE("GPL");


late_initcall(tcc_dispman_init);
module_exit(tcc_dispman_cleanup);


