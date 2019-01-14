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
