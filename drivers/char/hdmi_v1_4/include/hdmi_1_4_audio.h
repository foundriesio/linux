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
#ifndef __TCC_HDMI_AUDIO_H__
#define __TCC_HDMI_AUDIO_H__
#define HDMI_AUDIO_IOC_MAGIC      'a'


typedef enum {
	I2S = 0, SPDIF
} interfaceType_t;

typedef enum {
	AUDIO_SAMPLE = 1, HBR_STREAM
} packet_t;

typedef enum {
	LPCM = 0,
	NLPCM,
} codingType_t;

typedef struct {
	interfaceType_t mInterfaceType;

	int mSamplingFrequency;	/** sampling frequency (audioParams_t *params, Hz) */

	codingType_t mCodingType; /** (audioParams_t *params, see InfoFrame) */

	int mChannelAllocation; /** channel allocation (audioParams_t *params,
						   see InfoFrame) */
	int mWordLength; /**	Word length - 16 to 24 */


	/* I2S config */
	int mI2S_BitPerChannel;	/** Serial data bit per channel - 16, 20, 24 */
	int mI2S_Frame; /** Bit clock per Frame - 32, 48, 64 */
	int mI2S_Format; /** I2S mode - 0: basic format, 1: left justified format, 2: right justified format */


	packet_t mPacketType; /** packet type. currently only Audio Sample (AUDS)
						  and High Bit Rate (HBR) are supported */


} audioParams_t;

#define HDMI_IOC_SET_AUDIO_CONFIG			_IOW(HDMI_AUDIO_IOC_MAGIC, 0x01, audioParams_t)
#define HDMI_IOC_SET_AUDIO_ENABLE			_IOW(HDMI_AUDIO_IOC_MAGIC, 0x02, int)

#define HDMI_IOC_SET_SPEAKER_ALLOCATION     		_IOW(HDMI_AUDIO_IOC_MAGIC,0x03, int)
#define HDMI_IOC_GET_SPEAKER_ALLOCATION     		_IOR(HDMI_AUDIO_IOC_MAGIC,0x04, int)

#endif /* __TCC_HDMI_AUDIO_H__ */
