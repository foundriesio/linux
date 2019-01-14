/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

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
#ifndef HALFRAMECOMPOSERVIDEO_H_
#define HALFRAMECOMPOSERVIDEO_H_

int fc_video_config(struct hdmi_tx_dev *dev, videoParams_t *video);
void fc_video_hdcp_keepout(struct hdmi_tx_dev *dev, u8 bit);
void fc_video_VSyncPulseWidth(struct hdmi_tx_dev *dev, u16 value);
int fc_video_config_timing(struct hdmi_tx_dev *dev, videoParams_t *video);
int fc_video_config_default(struct hdmi_tx_dev *dev);

#endif	/* HALFRAMECOMPOSERVIDEO_H_ */
