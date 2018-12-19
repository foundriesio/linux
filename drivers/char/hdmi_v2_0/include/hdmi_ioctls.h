/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved
*  \file        hdmi_ioctls.h
*  \brief       HDMI TX controller driver
*  \details
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular
purpose or non-infringement  of  any  patent,  copyright  or  other third party
intellectual property right. No warranty is made, express or implied, regarding
the information's accuracy, completeness, or performance.
In no event shall Telechips be liable for any claim, damages or other liability
arising from, out of or in connection with this source  code or the  use in the
source code.
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure
Agreement between Telechips and Company.
*******************************************************************************/

#ifndef __HDMI_V2_0_H__
#define __HDMI_V2_0_H__

#if !defined(APP_BUILD)
typedef enum {
        MODE_UNDEFINED = -1,
        DVI = 0,
        HDMI
} video_mode_t;

typedef struct {
        /** VIC code */
        uint32_t mCode;

        /** Identifies modes that ONLY can be displayed in YCC 4:2:0 */
        uint8_t mLimitedToYcc420;

        /** Identifies modes that can also be displayed in YCC 4:2:0 */
        uint8_t mYcc420;

        uint16_t mPixelRepetitionInput;

        /** In units of 1MHz */
        uint32_t mPixelClock;

        /** 1 for interlaced, 0 progressive */
        uint8_t mInterlaced;

        uint16_t mHActive;

        uint16_t mHBlanking;

        uint16_t mHBorder;

        uint16_t mHImageSize;

        uint16_t mHSyncOffset;

        uint16_t mHSyncPulseWidth;

        /** 0 for Active low, 1 active high */
        uint8_t mHSyncPolarity;

        uint16_t mVActive;

        uint16_t mVBlanking;

        uint16_t mVBorder;

        uint16_t mVImageSize;

        uint16_t mVSyncOffset;

        uint16_t mVSyncPulseWidth;

        /** 0 for Active low, 1 active high */
        uint8_t mVSyncPolarity;

} dtd_t;

typedef enum {
        COLOR_DEPTH_8 = 8,
        COLOR_DEPTH_10 = 10,
        COLOR_DEPTH_12 = 12,
        COLOR_DEPTH_16 = 16
} color_depth_t;

typedef enum {
        PIXEL_REPETITION_OFF = 0,
        PIXEL_REPETITION_1 = 1,
        PIXEL_REPETITION_2 = 2,
        PIXEL_REPETITION_3 = 3,
        PIXEL_REPETITION_4 = 4,
        PIXEL_REPETITION_5 = 5,
        PIXEL_REPETITION_6 = 6,
        PIXEL_REPETITION_7 = 7,
        PIXEL_REPETITION_8 = 8,
        PIXEL_REPETITION_9 = 9,
        PIXEL_REPETITION_10 = 10
} pixel_repetition_t;

typedef enum {
        HDMI_14 = 1,
        HDMI_20,
        MHL_24 ,
        MHL_PACKEDPIXEL
} operation_mode_t;

typedef enum {
        ENC_UNDEFINED = -1,
        RGB = 0,
        YCC444,
        YCC422,
        YCC420,
        ENC_AUTO=125,
} encoding_t;

typedef enum {
        ITU601 = 1,
        ITU709,
        EXTENDED_COLORIMETRY
} colorimetry_t;

typedef enum {
        COLORMETRY_INVALID=-1,
        XV_YCC601 = 0,
        XV_YCC709,
        S_YCC601,
        ADOBE_YCC601,
        ADOBE_RGB,
        BT2020YCCBCR, // ITU-R BT.2020 Y'CC'BCC'RC
        BT2020YCBCR // ITU-R BT.2020 R¡¯G¡¯B¡¯ or Y'C'BC'R
} ext_colorimetry_t;

typedef struct {
        video_mode_t mHdmi;
        encoding_t mEncodingOut;
        encoding_t mEncodingIn;
        u8 mColorResolution;
        u8 mPixelRepetitionFactor;
        dtd_t mDtd;
        u8 mRgbQuantizationRange;
        u8 mYuvQuantizationRange;
        u8 mPixelPackingDefaultPhase;
        u8 mColorimetry;
        u8 mScanInfo;
        u8 mActiveFormatAspectRatio;
        u8 mNonUniformScaling;
        ext_colorimetry_t mExtColorimetry;
        u8 mColorimetryDataBlock;
        u8 mItContent;
        u16 mEndTopBar;
        u16 mStartBottomBar;
        u16 mEndLeftBar;
        u16 mStartRightBar;
        u16 mCscFilter;
        u16 mCscA[4];
        u16 mCscC[4];
        u16 mCscB[4];
        u16 mCscScale;
        u8 mHdmiVideoFormat;
        u8 m3dStructure;
        u8 m3dExtData;
        u8 mHdmiVic;
        u8 mHdmi20;
        u8 mScrambling;
        u8 mHdrEnable;
        u8 mDolbyVision;       // [2:0] b`000: No DV, b`001: DV-YCC444, b`010: DV-RGB [5:3] b`000: SDR, b`0
        u8 mScdcPresent;
} videoParams_t;

typedef enum {
	INTERFACE_NOT_DEFINED = -1, I2S = 0, SPDIF, HBR, GPA, DMA
} interfaceType_t;

typedef enum {
	PACKET_NOT_DEFINED = -1, AUDIO_SAMPLE = 1, HBR_STREAM
} packet_t;

typedef enum {
	CODING_NOT_DEFINED = -1,
	PCM = 1,
	AC3,
	MPEG1,
	MP3,
	MPEG2,
	AAC,
	DTS,
	ATRAC,
	ONE_BIT_AUDIO,
	DOLBY_DIGITAL_PLUS,
	DTS_HD,
	MAT,
	DST,
	WMAPRO
} codingType_t;

typedef enum {
	DMA_NOT_DEFINED = -1,
	DMA_4_BEAT_INCREMENT = 0,
	DMA_8_BEAT_INCREMENT,
	DMA_16_BEAT_INCREMENT,
	DMA_UNUSED_BEAT_INCREMENT,
	DMA_UNSPECIFIED_INCREMENT
} dmaIncrement_t;

/* Supplementary Audio type, table 8-14 HDMI 2.0 Spec. pg 79 */
typedef enum {
	RESERVED = 0,
	AUDIO_FOR_VIS_IMP_NARR,
	AUDIO_FOR_VIS_IMP_SPOKEN,
	AUDIO_FOR_HEAR_IMPAIRED,
	ADDITIONAL_AUDIO
} suppl_A_Type_t;

/* Audio Metadata Packet Header, table 8-4 HDMI 2.0 Spec. pg 71 */
typedef struct {
	u8 m3dAudio;
	u8 mNumViews;
	u8 mNumAudioStreams;
} audioMetaDataHeader_t;

/* Audio Metadata Descriptor, table 8-13 HDMI 2.0 Spec. pg 78 */
typedef struct {
	u8 mMultiviewRightLeft;
	u8 mLC_Valid;
	u8 mSuppl_A_Valid;
	u8 mSuppl_A_Mixed;
	suppl_A_Type_t mSuppl_A_Type;
	u8 mLanguage_Code[3];	/*ISO 639.2 alpha-3 code, examples: eng,fre,spa,por,jpn,chi */

} audioMetaDataDescriptor_t;

typedef struct {
	audioMetaDataHeader_t mAudioMetaDataHeader;
	audioMetaDataDescriptor_t mAudioMetaDataDescriptor[4];
} audioMetaDataPacket_t;

/**
 * For detailed handling of this structure,
 * refer to documentation of the functions
 */
typedef struct {
	interfaceType_t mInterfaceType;

	codingType_t mCodingType; /** (audioParams_t *params, see InfoFrame) */

	u8 mChannelAllocation; /** channel allocation (audioParams_t *params,
						   see InfoFrame) */

	u8 mSampleSize;	/**  sample size (audioParams_t *params, 16 to 24) */

	u8 mDataWidth;

	u32 mSamplingFrequency;	/** sampling frequency (audioParams_t *params, Hz) */

	u8 mLevelShiftValue; /** level shift value (audioParams_t *params,
						 see InfoFrame) */

	u8 mDownMixInhibitFlag;	/** down-mix inhibit flag (audioParams_t *params,
							see InfoFrame) */

	u8 mIecCopyright; /** IEC copyright */

	u8 mIecCgmsA; /** IEC CGMS-A */

	u8 mIecPcmMode;	/** IEC PCM mode */

	u8 mIecCategoryCode; /** IEC category code */

	u8 mIecSourceNumber; /** IEC source number */

	u8 mIecClockAccuracy; /** IEC clock accuracy */

	packet_t mPacketType; /** packet type. currently only Audio Sample (AUDS)
						  and High Bit Rate (HBR) are supported */

	u16 mClockFsFactor; /** Input audio clock Fs factor used at the audio
						packetizer to calculate the CTS value and ACR packet
						insertion rate */

	dmaIncrement_t mDmaBeatIncrement; /** Incremental burst modes: unspecified
									lengths (upper limit is 1kB boundary) and
									INCR4, INCR8, and INCR16 fixed-beat burst */

	u8 mDmaThreshold; /** When the number of samples in the Audio FIFO is lower
						than the threshold, the DMA engine requests a new burst
						request to the AHB master interface */

	u8 mDmaHlock; /** Master burst lock mechanism */

	u8 mGpaInsertPucv;	/* discard incoming (Parity, Channel status, User bit,
				   Valid and B bit) data and insert data configured in
				   controller instead */
	audioMetaDataPacket_t mAudioMetaDataPacket; /** Audio Multistream variables, to be written to the Audio Metadata Packet */

	u8	mAudioType;
} audioParams_t;


typedef struct {
        /* Vendor Name of eight 7-bit ASCII characters */
        u8 mVendorName[8];

        u8 mVendorNameLength;

        /* Product name or description, consists of sixteen 7-bit ASCII characters */
        u8 mProductName[16];

        u8 mProductNameLength;

        /* Code that classifies the source device (CEA Table 15) */
        u8 mSourceType;

        /* oui 24 bit IEEE Registration Identifier */
        u32 mOUI;

        u8 mVendorPayload[24];

        u8 mVendorPayloadLength;

} productParams_t;


typedef struct {
	/** HDMI Link data enable polarity value */
	uint8_t uDataEnablePol;

	/** current HDMI mode : DVI = 0, HDMI = 1 */
	int iHdmiMode;

	/** HSync Polarity */
	uint8_t uHSPol;

	/** VSync Polarity */
	uint8_t uVSPol;

        /** Enable HDCP1.4 - 2:HDCP 2.2 Enable 1:Enable, 0:Disable */
	int mHdcp14Enable;

        /** Bypass encryption */
        int bypass;

        /** Enable Feature 1.1 */
        int mEnable11Feature;

        /** Check Ri every 128th frame */
        int mRiCheck;

        /** I2C fast mode */
        int mI2cFastMode;

        /** Enhanced link verification */
        int mEnhancedLinkVerification;

        /** Number of supported devices
         * (depending on instantiated KSV MEM RAM – Revocation Memory to support
         * HDCP repeaters)
         */
        u8 maxDevices;

        /** KSV List buffer
         * Shall be dimensioned to accommodate 5[bytes] x No. of supported devices
         * (depending on instantiated KSV MEM RAM – Revocation Memory to support
         * HDCP repeaters)
         * plus 8 bytes (64-bit) M0 secret value
         * plus 2 bytes Bstatus
         * Plus 20 bytes to calculate the SHA-1 (VH0-VH4)
         * Total is (30[bytes] + 5[bytes] x Number of supported devices)
         */
        u8 *mKsvListBuffer;

        /** aksv total of 14 chars**/
        u8 *mAksv;

        /** Keys list
         * 40 Keys of 14 characters each
         * stored in segments of 8 bits (2 chars)
         * **/
        u8 *mKeys;

        u8 *mSwEncKey;
} hdcpParams_t;

#endif

typedef struct {
	u8 version;
	u8 length;
} DRM_Packet_Header;

typedef struct {
	u8	EOTF;
	u8	Descriptor_ID;
	u16	disp_primaries_x[3];
	u16	disp_primaries_y[3];
	u16	white_point_x;
	u16 	white_point_y;
	u16 	max_disp_mastering_luminance;
	u16 	min_disp_mastering_luminance;
	u16 	max_content_light_level;
	u16 	max_frame_avr_light_level;

} DRM_Packet_Body;

typedef struct {
	DRM_Packet_Header	mInfoFrame;
	DRM_Packet_Body		mDescriptor_type1;
} DRM_Packet_t;


/**
 * Structure that interfaces with the IOCTL of the hdmi_tx. To the IOCTL
 * that support read and write parameters use this structure to get and set
 * data from and to the driver
 */
typedef struct {
        unsigned int address;
        unsigned int value;
} dwc_hdmi_ioctl_data;


/**
 * Structure that interfaces with the IOCTL of the hdmi_tx. To the IOCTL
 * that configure to hdmi phy use this structure to search valid config data.
 */
typedef struct {
        unsigned int freq_hz;
        unsigned int pixel;
}dwc_hdmi_phy_config_data;


typedef struct {
        uint16_t sfrClock;
        uint16_t ss_low_ckl;
        uint16_t ss_high_ckl;
        uint16_t fs_low_ckl;
        uint16_t fs_high_ckl;
}dwc_hdmi_ddc_config_data;

typedef struct {
        uint8_t i2cAddr;
        uint8_t segment;
        uint8_t pointer;
        uint8_t addr;
        uint8_t len;
        uint8_t *data;
}dwc_hdmi_ddc_transfer_data;


typedef struct {
        uint32_t code;
        uint32_t refreshRate;
        dtd_t dtd;
}dwc_hdmi_dtd_data;

typedef struct {
        videoParams_t videoParam;
        audioParams_t audioParam;
        productParams_t productParam;
        hdcpParams_t hdcpParam;
}dwc_hdmi_api_data;


typedef struct {
        unsigned int sink_manufacture; /* 0-bit means vizio tv */
        unsigned int param0; /* futher used */
        unsigned int param1; /* futher used */
}dwc_hdmi_api_extension;

typedef struct {
        videoParams_t videoParam;
        audioParams_t audioParam;
        productParams_t productParam;
        hdcpParams_t hdcpParam;
        dwc_hdmi_api_extension api_extension;
}dwc_hdmi_api_ex_data;

typedef struct {
        unsigned short colorimetry;
        unsigned short ext_colorimetry;
}dwc_colorimetry_data;

#define SCDC_STATUS_CHANGE          0
#define SCDC_STATUS_CED             1
#define SCDC_STATUS_SET_TMDS        2
#define SCDC_STATUS_SET_SCRAMBLE    3
#define SCDC_STATUS_SCRAMBLED       4
#define SCDC_STATUS_VALID           5
#define SCDC_STATUS_CLK_LOCK        6

#define SCDC_CHANNEL_LOCK           0
#define SCDC_CHANNEL_VALID          1
#define SCDC_CHANNEL_ERROR_SHIFT    16
#define SCDC_CHANNEL_ERROR_MASK     0xFFFF

struct hdmi_scdc_error_data{
        volatile unsigned long status;
        volatile unsigned long ch0;
        volatile unsigned long ch1;
        volatile unsigned long ch2;
};


/**
 * SOC_FEATURE_DV_TO_DISP
 * @short Display device can using dolby-vision output as its input source */
#define SOC_FEATURE_DV_TO_DISP          (1 << 6)
/**
  * SOC_FEATURE_DOLBYVISION
  * @short Support dolby-vision output */
#define SOC_FEATURE_DOLBYVISION         (1 << 5)
/**
  * SOC_FEATURE_HDR
  * @short Support HDR/HLG output */
#define SOC_FEATURE_HDR                 (1 << 4)
/**
  * SOC_FEATURE_HPD_LINK_MODE
  * @short Support hotplug detection from hdmi link */
#define SOC_FEATURE_HPD_LINK_MODE       (1 << 3)
/**
  * SOC_FEATURE_30_36_BIT
  * @short Support 36-bit deep color */
#define SOC_FEATURE_30_36_BIT           (1 << 2)
/**
  * SOC_FEATURE_30_BIT
  * @short Support 30-bit deep color */
#define SOC_FEATURE_30_BIT              (1 << 1)
/**
  * SOC_FEATURE_YCBCR420
  * @short Support ycbcr420 color space at 4k 50/60Hz */
#define SOC_FEATURE_YCBCR420            (1 << 0)

typedef struct hdmi_soc_features{
        unsigned int max_tmds_mhz;      // Mhz
        unsigned int support_feature_1; /*
                                                [05:--] : Soc has dolbyvision limitation
                                                          0 - no
                                                          1 - yes (480p)
                                                [04:--] : Soc can support HDR
                                                          0 - not support
                                                          1 - support HDR
                                                [03:--] : HPD interrupt model
                                                          00 - gpio
                                                          01 - hdi link
                                                [02:01] : Maximum depth that Soc can support.
                                                          00 - reserved
                                                          01 - 30-bit
                                                          10 - 36-bit
                                                [00:--] : Soc can support YCbCr420
                                                          0 - not support
                                                          1 - support
                                        */
        unsigned int support_feature_2;        // reserved for future use
        unsigned int support_feature_3;        // reserved for future use
        unsigned int support_feature_4;        // reserved for future use
}hdmi_soc_features;


typedef struct hdmi_board_features{
        unsigned int support_feature_1;         // fixed video identification code
        unsigned int support_feature_2;         // reserved for future use
        unsigned int support_feature_3;         // reserved for future use
        unsigned int support_feature_4;         // reserved for future use
}hdmi_board_features;


#define HDCP_SUPPORT	1

#define IOCTL_HDMI_MAGIC            'H'
/**
 * IOCTL defines
 */

/**
 * @short IOCTL to read a byte from HDMI TX CORE
 * - fb_data->address -> address to read
 * - fb_data->value -> return value
 */
#define FB_HDMI_CORE_READ                       _IO( IOCTL_HDMI_MAGIC, 0x000)

/**
 * @short IOCTL to write a byte to HDMI TX CORE
 * fb_data->address -> address to write to
 * fb_data->value -> value to write
 */
#define FB_HDMI_CORE_WRITE                      _IO( IOCTL_HDMI_MAGIC, 0x001)

/**
 * @short IOCTL to read a byte from HDMI TX HDCP
 * - fb_data->address -> address to read
 * - fb_data->value -> return value
 */
#define FB_HDMI_HDCP_READ                       _IO( IOCTL_HDMI_MAGIC, 0x002)

/**
 * @short IOCTL to write a byte to HDMI TX HDCP
 * fb_data->address -> address to write to
 * fb_data->value -> value to write
 */
#define FB_HDMI_HDCP_WRITE                      _IO( IOCTL_HDMI_MAGIC, 0x003)

/**
 * @short IOCTL to read a byte from HDMI TX PHY INTERFACE
 * - fb_data->address -> address to read
 * - fb_data->value -> return value
 */
#define FB_HDMI_PHY_IF_READ                     _IO( IOCTL_HDMI_MAGIC, 0x004)

/**
 * @short IOCTL to control power management
 */
#define FB_HDMI_PHY_IF_WRITE                    _IO( IOCTL_HDMI_MAGIC, 0x005)

#define HDMI_DRV_LOG                            _IO( IOCTL_HDMI_MAGIC, 0x008)

/**
 * @short IOCTL to write a byte to HDMI TX PHY INTERFACE
 * fb_data->address -> address to write to
 * fb_data->value -> value to write
 */
#define HDMI_GET_PWR_STATUS                     _IOR( IOCTL_HDMI_MAGIC, 0x100, unsigned int)
#define HDMI_SET_PWR_CONTROL                    _IOW( IOCTL_HDMI_MAGIC, 0x101, unsigned int)
#define HDMI_GET_SUSPEND_STATUS                 _IOR( IOCTL_HDMI_MAGIC, 0x102, unsigned int)
#define HDMI_GET_OUTPUT_ENABLE_STATUS           _IOR( IOCTL_HDMI_MAGIC, 0x103, unsigned int)

#define HDMI_IOC_BLANK                          _IOW( IOCTL_HDMI_MAGIC, 0x110, unsigned int)



#define HDMI_GET_SOC_FEATURES                   _IOR( IOCTL_HDMI_MAGIC, 0x121, hdmi_soc_features)
#define HDMI_GET_BOARD_FEATURES                 _IOR( IOCTL_HDMI_MAGIC, 0x122, hdmi_board_features)

/**
 * @short IOCTL to get the device base address
 * fb_data->value -> base address
 */
#define HDMI_GET_DTD_INFO                       _IOR( IOCTL_HDMI_MAGIC, 0x201, dwc_hdmi_dtd_data)
#define FB_HDMI_SET_HPD                         _IO( IOCTL_HDMI_MAGIC, 0x202)
#define FB_HDMI_GET_HDCP22                      _IO( IOCTL_HDMI_MAGIC, 0x203)
#define FB_HDMI_FLUSH_SIG                       _IO( IOCTL_HDMI_MAGIC, 0x204)



/**
 * @short This IOCTL was deprecated.
 */
#define HDMI_TIME_PROFILE                       _IOW( IOCTL_HDMI_MAGIC, 0x220, char*)

#define HDMI_DDC_READ_DATA                      _IOR( IOCTL_HDMI_MAGIC, 0x222, dwc_hdmi_ddc_transfer_data)
#define HDMI_DDC_WRITE_DATA                     _IOW( IOCTL_HDMI_MAGIC, 0x223, dwc_hdmi_ddc_transfer_data)
#define HDMI_SCDC_GET_SINK_VERSION              _IOR( IOCTL_HDMI_MAGIC, 0x224, unsigned int)
#define HDMI_SCDC_GET_SOURCE_VERSION            _IOW( IOCTL_HDMI_MAGIC, 0x225, unsigned int)
#define HDMI_SCDC_SET_SOURCE_VERSION            _IOW( IOCTL_HDMI_MAGIC, 0x226, unsigned int)
#define HDMI_DDC_BUS_CLEAR			_IO( IOCTL_HDMI_MAGIC, 0x227)


#define HDMI_VIDEO_SET_SCRAMBLING               _IOW( IOCTL_HDMI_MAGIC, 0x232, int)
#define HDMI_VIDEO_GET_ERROR_DETECTION          _IO( IOCTL_HDMI_MAGIC, 0x234)
#define HDMI_VIDEO_GET_SCRAMBLE_STATUS          _IOR( IOCTL_HDMI_MAGIC, 0x235, int)
#define HDMI_VIDEO_GET_SCRAMBLE_WAIT_TIME       _IOR( IOCTL_HDMI_MAGIC, 0x236, int)


/**
 * @short IOCTL to control HDMI Audio Interface
 * audioParams_t -> set parameters for HDMI Audio.
 */

#define HDMI_AUDIO_INIT                         _IO( IOCTL_HDMI_MAGIC, 0x240)
#define HDMI_AUDIO_CONFIG                       _IOW( IOCTL_HDMI_MAGIC, 0x241, audioParams_t)


/**
 * @short IOCTL to control HDMI HPD Interface
 */

#define HDMI_HPD_SET_ENABLE                     _IO( IOCTL_HDMI_MAGIC, 0x250)
#define HDMI_HPD_GET_STATUS                     _IOR( IOCTL_HDMI_MAGIC, 0x251, int)


#define HDMI_SET_VENDOR_INFOFRAME               _IOW( IOCTL_HDMI_MAGIC, 0x261, productParams_t)


#define HDMI_PACKET_CONFIG                      _IO( IOCTL_HDMI_MAGIC, 0x270)
#define HDMI_AV_MUTE_MAGIC                      0xF0F0A0A0
#define HDMI_SET_AV_MUTE                        _IOW( IOCTL_HDMI_MAGIC, 0x271, unsigned int)


#define HDMI_API_GET_NEED_PRE_CONFIG            _IOR( IOCTL_HDMI_MAGIC, 0x278, int)
#define HDMI_API_PRE_CONFIG                     _IO( IOCTL_HDMI_MAGIC, 0x279)
#define HDMI_API_CONFIG                         _IOW( IOCTL_HDMI_MAGIC, 0x280, dwc_hdmi_api_data)
#define HDMI_API_CONFIG_EX                      _IOW( IOCTL_HDMI_MAGIC, 0x280, dwc_hdmi_api_ex_data)
#define HDMI_API_DISABLE_MAGIC                  0xA0500281
#define HDMI_API_DISABLE                        _IOW( IOCTL_HDMI_MAGIC, 0x281, unsigned int)
#define HDMI_API_PHY_MASK_MAGIC			0xCC104770
#define HDMI_API_PHY_MASK			_IOW( IOCTL_HDMI_MAGIC, 0x282, unsigned int)

/**
 * @short IOCTL to update drm config of user to drmParm.
 */
#define HDMI_API_DRM_CONFIG			_IOW(IOCTL_HDMI_MAGIC, 0x285, DRM_Packet_t)

/**
 * @short IOCTL to apply drm config of drmparm to hdmi link.
 */
#define HDMI_API_DRM_SET			_IO(IOCTL_HDMI_MAGIC, 0x286)

/**
 * @short IOCTL to clear drm config of drmparm to hdmi link.
 */
#define HDMI_API_DRM_CLEAR                      _IO(IOCTL_HDMI_MAGIC, 0x287)


/**
 * @short IOCTL get validation of drmparam

 */
#define HDMI_API_DRM_GET_VALID                  _IOR( IOCTL_HDMI_MAGIC, 0x288, int)

#if defined(HDCP_SUPPORT)
#define HDCP22_CTRL_REG_RESET			_IO( IOCTL_HDMI_MAGIC, 0x290)
#define HDMI_GET_PHY_RX_SENSE_STATUS		_IOR( IOCTL_HDMI_MAGIC, 0x291, int)
#define HDMI_GET_HDCP22_STATUS			_IOR( IOCTL_HDMI_MAGIC, 0x292, int)
#endif

#endif  // __HDMI_V2_0_H__



