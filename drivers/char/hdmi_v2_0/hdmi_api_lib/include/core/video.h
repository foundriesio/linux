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
#ifndef VIDEO_H_
#define VIDEO_H_
/**
 * @file
 * Video controller layer.
 * Configure and set up the video transmitted to sink.
 * This includes colour-space conversion as well as pixel packetising.
 */

/**
 * Configures the video blocks to do any video processing and to
 * transmit the video set up required by the user, allowing to
 * force video pixels (from the DEBUG pixels) to be transmitted
 * rather than the video stream being received.
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_Configure(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * Set up color space converter to video requirements
 * (if there is any encoding type conversion or csc coefficients)
 * @param baseAddr Base Address of module 
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_ColorSpaceConverter(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * Set up video packetizer which "packetizes" pixel transmission
 * (in deep colour mode, YCC422 mapping and pixel repetition)
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_VideoPacketizer(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * Set up video mapping and stuffing
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @return TRUE if successful
 */
int video_VideoSampler(struct hdmi_tx_dev *dev, videoParams_t * params);

/**
 * A test only method that is used for a test module
 * @param baseAddr Base Address of module
 * @param params VideoParams
 * @param dataEnablePolarity
 * @return TRUE if successful
 */
int video_VideoGenerator(struct hdmi_tx_dev *dev, videoParams_t * params,
			 u8 dataEnablePolarity);

#endif	/* VIDEO_H_ */
