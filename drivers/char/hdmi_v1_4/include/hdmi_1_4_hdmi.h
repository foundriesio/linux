/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi_1_4_hdmi.h
*  \brief       HDMI controller driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
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
#ifndef _LINUX_HDMI_H_
#define _LINUX_HDMI_H_

#define HDMI_IOC_MAGIC      'h'


#ifndef __HDMI_AUDIO_AUDIOSAMPLEPACKETTYPE__
#define __HDMI_AUDIO_AUDIOSAMPLEPACKETTYPE__
/**
 * @enum HDMIASPType
 * Type of HDMI audio sample packet
 */
enum HDMIASPType
{
    /** Audio Sample Packet Type */
    HDMI_ASP,
    /** One Bit Audio Packet Type */
    HDMI_DSD,
    /** High Bit Rate Packet Type */
    HDMI_HBR,
    /** DST Packet Type */
    HDMI_DST
};
#endif /* __HDMI_AUDID_AUDIOSAMPLEPACKETTYPE__ */

#ifndef __DEVICE_VIDEO_PARAMS__
#define __DEVICE_VIDEO_PARAMS__
/**
 * @struct device_video_params
 * Video timing paramters to set HDMI H/W @n
 * For more information, refer to HDMI register map doc.
 */
struct device_video_params
{
	/** [H Total] */
	unsigned int HTotal;

	/** [H Blank] */
	unsigned int HBlank;

	/** [V Total] */
	unsigned int VTotal;

	/** [V Blank] : in interlaced mode, this is VBlank - 0.5
	*               but, VIC ID = 39, this is VBlank
	*/
	unsigned int VBlank;

	/** [HFront] */
	unsigned int HFront;

	/** [HSync] */
	unsigned int HSync;

	/** H Sync polarity */
	unsigned int HPol;

	/** [VFront] */
	unsigned int VFront;

	/** [VSync] */
	unsigned int VSync;

	/** V Sync polarity */
	unsigned int VPol;

	/** CEA VIC */
	unsigned int AVI_VIC;

	/** CEA VIC for 16:9 pixel ratio */
	unsigned int AVI_VIC_16_9;

	/** 0 - progresive, 1 - interlaced */
	unsigned int interlaced;

	/** Pixel repetition if double, set 1 */
	unsigned int repetition;
};
#endif /* __DEVICE_VIDEO_PARAMS__ */

#ifndef __HDMI_DEVICE_LCDC_TIMING_PARAMS__
#define __HDMI_DEVICE_LCDC_TIMING_PARAMS__
/**
 * @struct device_lcdc_timing_params
 * Video timing paramters to set LCDC H/W @n
 * For more information, refer to LCDC register map doc.
 */
struct device_lcdc_timing_params
{
	unsigned int id;
	unsigned int iv;
	unsigned int ih;
	unsigned int ip;
	unsigned int dp;
	unsigned int ni;
	unsigned int tv;
	unsigned int tft;
	unsigned int stn;

	//LHTIME1
	unsigned int lpw;
	unsigned int lpc;
	//LHTIME2
	unsigned int lswc;
	unsigned int lewc;
	//LVTIME1
	unsigned int vdb;
	unsigned int vdf;
	unsigned int fpw;
	unsigned int flc;
	//LVTIME2
	unsigned int fswc;
	unsigned int fewc;
	//LVTIME3
	unsigned int fpw2;
	unsigned int flc2;
	//LVTIME4
	unsigned int fswc2;
	unsigned int fewc2;

	unsigned int framepacking;

};

#endif /* __HDMI_DEVICE_VIDEO_PARAMS__ */

/**
 * @struct hdmi_dbg
 * Contains offset and value to set.
 */
struct hdmi_dbg{
	/** offset */
	unsigned int offset;
	/** value */
	unsigned int value;
};



#ifndef __HDMI_PHY_PIXELFREQUENCY__
#define __HDMI_PHY_PIXELFREQUENCY__

/**
 * @enum PHYFreq
 * PHY Frequency
 */
enum PHYFreq
{
	/** Not supported */
	PHY_FREQ_NOT_SUPPORTED = -1,
	/** 25.2 MHz pixel frequency */
	PHY_FREQ_25_200 = 0,
	/** 25.175 MHz pixel frequency */
	PHY_FREQ_25_175,
	/** 27 MHz pixel frequency */
	PHY_FREQ_27,
	/** 27.027 MHz pixel frequency */
	PHY_FREQ_27_027,
	/** 54 MHz pixel frequency */
	PHY_FREQ_54,
	/** 54.054 MHz pixel frequency */
	PHY_FREQ_54_054,
	/** 74.25 MHz pixel frequency */
	PHY_FREQ_74_250,
	/** 74.176 MHz pixel frequency */
	PHY_FREQ_74_176,
	/** 148.5 MHz pixel frequency */
	PHY_FREQ_148_500,
	/** 148.352 MHz pixel frequency */
	PHY_FREQ_148_352,
	/** 108.108 MHz pixel frequency */
	PHY_FREQ_108_108 = 10,
	/** 72 MHz pixel frequency */
	PHY_FREQ_72,
	/** 25 MHz pixel frequency */
	PHY_FREQ_25,
	/** 65 MHz pixel frequency */
	PHY_FREQ_65,
	/** 108 MHz pixel frequency */
	PHY_FREQ_108,
	/** 162 MHz pixel frequency */
	PHY_FREQ_162,
	/** 45 MHz pixel frequency */
	PHY_FREQ_45,
	/** 45.955 MHz pixel frequency */
	PHY_FREQ_44_955,
	/** 297 MHz pixel frequency */
	PHY_FREQ_297,
	/** 296.703 MHz pixel frequency */
	PHY_FREQ_296_703,
	/** 59.4 MHz pixel frequency */
	PHY_FREQ_59_400 = 20,
	/** 36 MHz pixel frequency */
	PHY_FREQ_36,
	/** 40 MHz pixel frequency */
	PHY_FREQ_40,
	/** 71 MHz pixel frequency */
	PHY_FREQ_71,
	/** 83.5 MHz pixel frequency */
	PHY_FREQ_83_500,
	/** 106.5 MHz pixel frequency */
	PHY_FREQ_106_500 = 25,
	/** 122.5 MHz pixel frequency */
	PHY_FREQ_122_500,
	/** 146.25 MHz pixel frequency */
	PHY_FREQ_146_250,
	/** 97.5 MHz pixel frequency */
	PHY_FREQ_97_340 = 28,	
	PHY_FREQ_MAX,
};

#endif /* __HDMI_PHY_PIXELFREQUENCY__ */

struct HDMIVideoFormatCtrl{
    unsigned char video_format;
	unsigned char structure_3D;
	unsigned char ext_data_3D;
};


// IOW
/** Device request code to set color space. */
#define HDMI_IOC_SET_COLORSPACE             _IOW(HDMI_IOC_MAGIC,0,enum ColorSpace)

/** Device request code to set color depth */
#define HDMI_IOC_SET_COLORDEPTH             _IOW(HDMI_IOC_MAGIC,1,enum ColorDepth)

/** Device request code to set video system */
#define HDMI_IOC_SET_HDMIMODE               _IOW(HDMI_IOC_MAGIC,2,enum HDMIMode)

/** Device request code to set video timing parameters */
#define HDMI_IOC_SET_VIDEOMODE              _IOW(HDMI_IOC_MAGIC,3,struct device_video_params)

/**
 * Device requset code to set/clear blue screen mode @n
 * 1 to set, 0 to clear
 */
#define HDMI_IOC_SET_BLUESCREEN             _IOW(HDMI_IOC_MAGIC,4,unsigned char)

/** Device requset code to set pixel limitation */
#define HDMI_IOC_SET_PIXEL_LIMIT            _IOW(HDMI_IOC_MAGIC,5,enum PixelLimit)

/**
 * Device requset code to set/clear AVMute @n
 * 1 to set, 0 to clear
 */
#define HDMI_IOC_SET_AVMUTE                 _IOW(HDMI_IOC_MAGIC,6,unsigned char)

/** Device requset code to set packet type of HDMI audio output */
#define HDMI_IOC_SET_AUDIOPACKETTYPE        _IOW(HDMI_IOC_MAGIC,7,enum HDMIASPType)


/** Device requset code to set audio sample freqency */
#define HDMI_IOC_SET_AUDIOSAMPLEFREQ        _IOW(HDMI_IOC_MAGIC,8,enum SamplingFreq)

/** Device requset code to set number of channels */
#define HDMI_IOC_SET_AUDIOCHANNEL           _IOW(HDMI_IOC_MAGIC,9,enum ChannelNum)

/** Device requset code to set audio speaker allocation information */
#define HDMI_IOC_SET_SPEAKER_ALLOCATION     _IOW(HDMI_IOC_MAGIC,10,unsigned int)
/**
 * Device requset code to start/stop sending audio packet @n
 * 1 to start, 0 to stop
 */
#define HDMI_IOC_SET_AUDIO_ENABLE           _IOW(HDMI_IOC_MAGIC,18,unsigned char)

/** Device request code to set pixel aspect ratio */
#define HDMI_IOC_SET_PIXEL_ASPECT_RATIO     _IOW(HDMI_IOC_MAGIC,19,enum PixelAspectRatio)

/** Device requset code to set colorimetry */
#define HDMI_IOC_SET_COLORIMETRY			_IOW(HDMI_IOC_MAGIC,20,enum HDMIColorimetry)

/** Device requset code to set HDMI video source */
#define HDMI_IOC_SET_VIDEO_SOURCE			_IOW(HDMI_IOC_MAGIC,21,enum HDMIVideoSource)

/** Device requset code to fill one subpacket in Audio Sample Packet */
#define HDMI_IOC_FILL_ONE_SUBPACKET			_IOW(HDMI_IOC_MAGIC,23,unsigned char)

// for debugging
/** Device requset code to set HDMI debug */
#define HDMI_IOC_SET_DEBUG					_IOW(HDMI_IOC_MAGIC,24,struct hdmi_dbg)

/** Device request code to set video format information */
#define HDMI_IOC_SET_VIDEOFORMAT_INFO       _IOW(HDMI_IOC_MAGIC,26,enum VideoFormat)

#define HDMI_IOC_SET_PHY_FREQ		       _IOW(HDMI_IOC_MAGIC,27,enum PHYFreq)

#define HDMI_IOC_SET_PHY_INDEX       _IOW(HDMI_IOC_MAGIC,28,unsigned char)

/** Device request code to set lcdc timing parameters */
#define HDMI_IOC_SET_LCDC_TIMING			_IOW(HDMI_IOC_MAGIC,30,struct device_lcdc_timing_params)

/** Device request code to set/get Hdmi Device Power */
#define HDMI_IOC_SET_PWR_CONTROL			_IOW(HDMI_IOC_MAGIC,31, unsigned int)

#define HDMI_IOC_SET_PWR_V5_CONTROL			_IOW(HDMI_IOC_MAGIC,32, unsigned int)

#define HDMI_IOC_VIDEO_FORMAT_CONTROL		_IOW(HDMI_IOC_MAGIC,40, struct HDMIVideoFormatCtrl)

#define HDMI_IOC_BLANK						_IOW(HDMI_IOC_MAGIC,50, unsigned int)


/**
 * Device requset code to get status of HDMI PHY H/W @n
 * if 1, PHY is ready; if 0, PHY is not ready.
 */
#define HDMI_IOC_GET_PHYREADY               _IOR(HDMI_IOC_MAGIC,0,unsigned char)

/** Device request code to get video configuration information (video parameters) */
#define HDMI_IOC_GET_VIDEOCONFIG           _IOR(HDMI_IOC_MAGIC,8,struct HDMIVideoParameter)

/** Device request code to get hdmi start/stop status */
#define HDMI_IOC_GET_HDMISTART_STATUS      _IOR(HDMI_IOC_MAGIC,9,unsigned int)



/** Device requset code to get pixel limitation */
#define HDMI_IOC_GET_PIXEL_LIMIT            _IOR(HDMI_IOC_MAGIC,10,enum PixelLimit)

/** Device requset code to get audio speaker allocation information */
#define HDMI_IOC_GET_SPEAKER_ALLOCATION     _IOR(HDMI_IOC_MAGIC,11,unsigned int)

#define HDMI_IOC_GET_AUDIOCHANNEL           _IOR(HDMI_IOC_MAGIC,12,enum ChannelNum)

#define HDMI_IOC_GET_AUDIOSAMPLEFREQ        _IOR(HDMI_IOC_MAGIC,13,enum SamplingFreq)

#define HDMI_IOC_GET_AUDIOPACKETTYPE        _IOR(HDMI_IOC_MAGIC,14,enum HDMIASPType)

#define HDMI_IOC_GET_PWR_STATUS				_IOR(HDMI_IOC_MAGIC,31, unsigned int)

#define HDMI_IOC_GET_SUSPEND_STATUS				_IOR(HDMI_IOC_MAGIC,32, unsigned int)

/** Device requset code to start sending HDMI output */
#define HDMI_IOC_START_HDMI                 _IO(HDMI_IOC_MAGIC,0)

/** Device requset code to stop sending HDMI output */
#define HDMI_IOC_STOP_HDMI                  _IO(HDMI_IOC_MAGIC,1)

/** Device requset code to reset AUI Sampling Freq Fields  */
#define HDMI_IOC_RESET_AUISAMPLEFREQ        _IO(HDMI_IOC_MAGIC,8)

#define HDMI_IOC_SET_PHYRESET               _IO(HDMI_IOC_MAGIC,9)

/** Device requset code to print HDMI H/W registers */
#define HDMI_IOC_DEBUG_HDMI_CORE			_IO(HDMI_IOC_MAGIC,10)

/** Device requset code to print HDMI H/W Video registers */
#define HDMI_IOC_DEBUG_HDMI_CORE_VIDEO		_IO(HDMI_IOC_MAGIC,11)

#define HDMI_IOC_SET_HPD_SWITCH_STATUS      _IO(HDMI_IOC_MAGIC,12)

#endif /* _LINUX_HDMI_H_ */
