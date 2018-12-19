/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        extenddisplay.cpp
*  \brief       HDMI TX controller driver
*  \details
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
#ifndef HALFRAMECOMPOSERVIDEO_H_
#define HALFRAMECOMPOSERVIDEO_H_

int fc_video_config(struct hdmi_tx_dev *dev, videoParams_t *video);
void fc_video_hdcp_keepout(struct hdmi_tx_dev *dev, u8 bit);
void fc_video_VSyncPulseWidth(struct hdmi_tx_dev *dev, u16 value);
int fc_video_config_timing(struct hdmi_tx_dev *dev, videoParams_t *video);
int fc_video_config_default(struct hdmi_tx_dev *dev);

#endif	/* HALFRAMECOMPOSERVIDEO_H_ */
