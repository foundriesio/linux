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
****************************************************************************/
#ifndef __OUTPUT_STARTER_HDMI_H__
#define __OUTPUT_STARTER_HDMI_H__


#if defined(CONFIG_LCD_HDMI1920X720_ADV7613)
#define HDMI_1920X720_60P
#else
#define HDMI_1280X720_60P
#endif

#if defined(HDMI_720X480_60P)
#define HDMI_VIDEO_MODE_VIC     2
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_IMG_WIDTH          720
#define HDMI_IMG_HEIGHT         480
#endif


#if defined(HDMI_1280X720_60P)
#define HDMI_VIDEO_MODE_VIC     4
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_IMG_WIDTH          1280
#define HDMI_IMG_HEIGHT         720

#endif

#if defined(HDMI_1920X1080_30P)
#define HDMI_VIDEO_MODE_VIC     34
#define HDMI_VIDEO_MODE_HZ      30000
#define HDMI_IMG_WIDTH          1920
#define HDMI_IMG_HEIGHT         1080

#endif


#if defined(HDMI_1920X1080_60I)
#define HDMI_VIDEO_MODE_VIC     5
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_IMG_WIDTH          1920
#define HDMI_IMG_HEIGHT         1080
#endif

#if defined(HDMI_1920X1080_59P)
#define HDMI_VIDEO_MODE_VIC     16
#define HDMI_VIDEO_MODE_HZ      59940
#define HDMI_IMG_WIDTH          1920
#define HDMI_IMG_HEIGHT         1080

#endif

#if defined(HDMI_1920X1080_60P)
#define HDMI_VIDEO_MODE_VIC     16
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_IMG_WIDTH          1920
#define HDMI_IMG_HEIGHT         1080
#endif


#if defined(HDMI_3840X2160_30P)
#define HDMI_VIDEO_MODE_VIC     95
#define HDMI_VIDEO_MODE_HZ      30000
#define HDMI_IMG_WIDTH          3840
#define HDMI_IMG_HEIGHT         2160
#endif


#if defined(HDMI_3840X2160_59P)
#define HDMI_VIDEO_MODE_VIC     97
#define HDMI_VIDEO_MODE_HZ      59940
#define HDMI_IMG_WIDTH          3840
#define HDMI_IMG_HEIGHT         2160
#endif


#if defined(HDMI_3840X2160_60P)
#define HDMI_VIDEO_MODE_VIC     97
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_IMG_WIDTH          3840
#define HDMI_IMG_HEIGHT         2160

#endif
#if defined(HDMI_1920X720_60P)
#define HDMI_VIDEO_MODE_VIC     1024
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_IMG_WIDTH          1920
#define HDMI_IMG_HEIGHT         720

#endif

#define HDMI_IMG_FMT            TCC_LCDC_IMG_FMT_RGB565

int tcc_output_starter_parse_hdmi_dt(struct device_node *np);
int tcc_output_starter_hdmi_v2_0(unsigned int display_device, volatile void __iomem *pRDMA, volatile void __iomem *pDISP);
int tcc_output_starter_hdmi_disable(void);
int tcc_hdmi_detect_cable(void);

#endif //__OUTPUT_STARTER_HDMI_H__

