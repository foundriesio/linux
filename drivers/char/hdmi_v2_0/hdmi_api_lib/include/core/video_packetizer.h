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
#ifndef HALVIDEOPACKETIZER_H_
#define HALVIDEOPACKETIZER_H_

u8 vp_PixelPackingPhase(struct hdmi_tx_dev *dev);

void vp_ColorDepth(struct hdmi_tx_dev *dev, u8 value);

void vp_PixelPackingDefaultPhase(struct hdmi_tx_dev *dev, u8 bit);

void vp_PixelRepetitionFactor(struct hdmi_tx_dev *dev, u8 value);

void vp_Ycc422RemapSize(struct hdmi_tx_dev *dev, u8 value);

void vp_OutputSelector(struct hdmi_tx_dev *dev, u8 value);

#endif	/* HALVIDEOPACKETIZER_H_ */
