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
#ifndef _LINUX_HDMI_H_
#define _LINUX_HDMI_H_

#define HDMI_IOC_MAGIC      'h'

typedef struct {
        /** Vendor Name of eight 7-bit ASCII characters */
        unsigned char vendor_name[8];

        unsigned char vendor_name_length;

        /** Product name or description, consists of sixteen 7-bit ASCII characters */
        unsigned char product_name[16];

        unsigned char product_name_length;

        /** Code that classifies the source device (CEA Table 15) */
        unsigned char source_information;
}spd_packet_t;

typedef struct  {
        unsigned int oui;
        unsigned char payload[24];
        unsigned char payload_length;
}vendor_packet_t;

typedef struct {
	spd_packet_t spd;
	vendor_packet_t vsif;
}packetParam_t;

struct hdmi_phy_params {
	unsigned int pixel_clock;
	unsigned int bpp;
};

struct hdmi_dtd_params {
	unsigned int code;
	unsigned int refreshRate;
	dtd_t detailed_timing;
};

#include <video/tcc/tccfb_ioctrl.h>

struct hdmi_dtd_display_params {
	videoParams_t videoParam;
	struct lcdc_timimg_parms_t lcdc_timimg_parm;
};

struct hdmi_dtd_extra_data_params {
	videoParams_t videoParam;
	struct tcc_fb_extra_data fb_extra_data;
};



/* PHY   ----------------------------------------------- */
#define HDMI_IOC_SET_PHY_FREQ				_IOW(HDMI_IOC_MAGIC,0x21, struct hdmi_phy_params)

/**
 * Device requset code to get status of HDMI PHY H/W @n
 * if 1, PHY is ready; if 0, PHY is not ready.
 */
#define HDMI_IOC_GET_PHYREADY               		_IOR(HDMI_IOC_MAGIC,0x22,unsigned char)



/* VIDEO ----------------------------------------------- */

/**
 * Device requset code to set/clear blue screen mode @n
 * 1 to set, 0 to clear
 */
#define HDMI_IOC_SET_BLUESCREEN             		_IOW(HDMI_IOC_MAGIC,0x31,unsigned char)

/**
 * Device requset code to set/clear AVMute @n
 * 1 to set, 0 to clear
 */
#define HDMI_IOC_SET_AVMUTE                 		_IOW(HDMI_IOC_MAGIC,0x32,int)


/* MISC   ----------------------------------------------- */

/** Device request code to set/get Hdmi Device Power */
#define HDMI_IOC_SET_PWR_CONTROL			_IOW(HDMI_IOC_MAGIC,0x33, int)
#define HDMI_IOC_GET_PWR_STATUS				_IOR(HDMI_IOC_MAGIC,0x34, int)

#define HDMI_IOC_SET_PWR_V5_CONTROL			_IOW(HDMI_IOC_MAGIC,0x35, unsigned int)

#define HDMI_IOC_BLANK					_IOW(HDMI_IOC_MAGIC,0x36, unsigned int)

/** Device request code to get hdmi start/stop status */
#define HDMI_IOC_GET_HDMISTART_STATUS      		_IOR(HDMI_IOC_MAGIC,0x37,unsigned int)

#define HDMI_IOC_GET_SUSPEND_STATUS			_IOR(HDMI_IOC_MAGIC,0x38, int)

/** Device requset code to start sending HDMI output */
#define HDMI_IOC_START_HDMI                 		_IO(HDMI_IOC_MAGIC,0x39)

/** Device requset code to stop sending HDMI output */
#define HDMI_IOC_STOP_HDMI                  		_IO(HDMI_IOC_MAGIC,0x3A)


#define HDMI_IOC_GET_DTD_FILL				_IOR(HDMI_IOC_MAGIC,0x41, struct hdmi_dtd_params)
#define HDMI_IOC_GET_DTD_DISPLAY_PARAM			_IOR(HDMI_IOC_MAGIC,0x42, struct hdmi_dtd_display_params)
#define HDMI_IOC_GET_DTD_EXTRA_DATA			_IOR(HDMI_IOC_MAGIC,0x43, struct hdmi_dtd_extra_data_params)

#define HDMI_IOC_SET_SPD_INFOFRAME			_IOW(HDMI_IOC_MAGIC,0x44, spd_packet_t)
#define HDMI_IOC_SET_VSIF_INFOFRAME			_IOW(HDMI_IOC_MAGIC,0x45, vendor_packet_t)

#define HDMI_IOC_SET_VIDEO_CONFIG			_IOW(HDMI_IOC_MAGIC,0x46, vendor_packet_t)






#endif /* _LINUX_HDMI_H_ */
