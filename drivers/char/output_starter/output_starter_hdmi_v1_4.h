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
#ifndef __OUTPUT_STARTER_HDMI_H__
#define __OUTPUT_STARTER_HDMI_H__

#define HDMI_VIDEO_MODE_VIC     16
#define HDMI_VIDEO_MODE_HZ      60000
#define HDMI_VIDEO_DEPTH        8

#define HDMI_IMG_WIDTH          1920
#define HDMI_IMG_HEIGHT         1080
#define HDMI_VIDEO_FORMAT       YCC444

int tcc_output_starter_parse_hdmi_dt(struct device_node *np);
int tcc_output_starter_hdmi_v1_4(unsigned int display_device, volatile void __iomem *pRDMA, volatile void __iomem *pDISP);
int tcc_output_starter_hdmi_disable(void);
int tcc_hdmi_detect_cable(void);

#endif //__OUTPUT_STARTER_HDMI_H__

