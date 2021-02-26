// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __DPTX_V14_H__
#define __DPTX_V14_H__

#include <linux/compat.h>
#include <linux/irqreturn.h>
#include <linux/regmap.h>
#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
#include <linux/input/tcc_tsc_serdes.h>
#endif

#define TCC_DPTX_DRV_MAJOR_VER			2
#define TCC_DPTX_DRV_MINOR_VER			1
#define TCC_DPTX_DRV_SUBTITLE_VER		0

#define TCC805X_REVISION_CS				0x01

#define DP_DDIBUS_BASE_REG_ADDRESS		0x12400000
#define DP_MICOM_BASE_REG_ADDRESS		0x1BD00000
#define DP_HDCP_OFFSET					0x00040000
#define DP_REGISTER_BANK_OFFSET			0x00080000
#define DP_CKC_OFFSET					0x000C0000
#define DP_PROTECT_OFFSET				0x000D0000

#define DP_CUSTOM_1025_DTD_VIC			1025	/** 1025 : 1024x600 : AV080WSM-NW0*/
#define DP_CUSTOM_1026_DTD_VIC			1026	/** 1026 : 5760x900@54p */
#define DP_CUSTOM_1027_DTD_VIC			1027	/** 1027 : 1920x720 : PVLBJT_020_01 */
#define DP_CUSTOM_MAX_DTD_VIC			1030

#define DPTX_SINK_CAP_SIZE				0x100
#define DPTX_SDP_NUM					0x10
#define DPTX_SDP_LEN					0x9
#define DPTX_SDP_SIZE					( 9 * 4 )

#define DPTX_DEFAULT_LINK_RATE			DPTX_PHYIF_CTRL_RATE_HBR2
#define DPTX_MAX_LINK_LANES				4
#define DPTX_MAX_LINK_SYMBOLS			64
#define DPTX_MAX_LINK_SLOTS				64

#define DP_LINK_STATUS_SIZE	  			6
#define MAX_VOLTAGE_SWING_LEVEL			4
#define MAX_PRE_EMPHASIS_LEVEL			4

#define INVALID_MST_PORT_NUM			0xFF
#define DPTX_EDID_BUFLEN				512
#define DPTX_ONE_EDID_BLK_LEN			128

#define DPTX_RETURN_SUCCESS				false
#define DPTX_RETURN_FAIL				true


#define LINK_TRAINING_SKIP_LANE_DROP
//#define ENABLE_FAST_LINK_TRAINING

#if defined( ENABLE_FAST_LINK_TRAINING )
#define PREDETERMINED_PRE_EMP_ON_CR			0 // PRE_EMPHASIS_LEVEL_0
#define PREDETERMINED_VSW_ON_CR				0 // VOLTAGE_SWING_LEVEL_0

#define PREDETERMINED_PRE_EMP_ON_EQ			0 // PRE_EMPHASIS_LEVEL_0
#define PREDETERMINED_VSW_ON_EQ				0 // VOLTAGE_SWING_LEVEL_0
#endif

enum TCC805X_EVB_TYPE
{
	TCC8059_EVB_01			= 0,
	TCC8050_SV_01			= 1,
	TCC8050_SV_10			= 2,
	TCC805X_EVB_UNKNOWN		= 0xFE
};

enum REG_DIV_CFG
{
	DIV_CFG_CLK_INVALID		= 0,	
	DIV_CFG_CLK_200HMZ		= 0x83,
	DIV_CFG_CLK_160HMZ		= 0xB1,
	DIV_CFG_CLK_100HMZ		= 0x87,
	DIV_CFG_CLK_40HMZ		= 0x93
};

enum PHY_POWER_STATE
{
	PHY_POWER_ON = 0,	
	PHY_POWER_DOWN_SWITCHING_RATE		= 0x02,
	PHY_POWER_DOWN_PHY_CLOCK			= 0x03,
	PHY_POWER_DOWN_REF_CLOCK			= 0x0C
};

enum PHY_LINK_RATE 
{
	LINK_RATE_RBR = 0,	/* 1.62 Gbs */
	LINK_RATE_HBR,	/* 2.7 Gbs */
	LINK_RATE_HBR2,	/* 5.4 Gbs */
	LINK_RATE_HBR3,	/* 8.1 Gbs */
	LINK_RATE_MAX
};

enum PHY_PRE_EMPHASIS_LEVEL
{
	PRE_EMPHASIS_LEVEL_0 = 0,
	PRE_EMPHASIS_LEVEL_1,
	PRE_EMPHASIS_LEVEL_2,
	PRE_EMPHASIS_LEVEL_3,
	PRE_EMPHASIS_LEVEL_INVALID
};

enum PHY_VOLTAGE_SWING_LEVEL
{
	VOLTAGE_SWING_LEVEL_0 = 0,
	VOLTAGE_SWING_LEVEL_1,
	VOLTAGE_SWING_LEVEL_2,
	VOLTAGE_SWING_LEVEL_3,
	VOLTAGE_SWING_LEVEL_INVALID
};

enum PHY_INPUT_STREAM_INDEX
{
	PHY_INPUT_STREAM_0		= 0,
	PHY_INPUT_STREAM_1		= 1,
	PHY_INPUT_STREAM_2		= 2,
	PHY_INPUT_STREAM_3		= 3,
	PHY_INPUT_STREAM_MAX	= 4
};

enum PHY_LANE_INDEX
{
	PHY_LANE_0		= 0,
	PHY_LANE_1		= 1,
	PHY_LANE_2		= 2,
	PHY_LANE_4		= 4,
	PHY_LANE_MAX	= 5
};

enum MST_INPUT_PORT_TYPE
{
	INPUT_PORT_TYPE_TX			= 0,
	INPUT_PORT_TYPE_RX			= 1,
	INPUT_PORT_TYPE_INVALID		= 2
};

enum MST_BRANCH_UNIT_INDEX
{
	FIRST_MST_BRANCH_UNIT			= 0,
	SECOND_MST_BRANCH_UNIT			= 1,
	THIRRD_MST_BRANCH_UNIT			= 2,
	INVALID_MST_BRANCH_UNIT			= 3
};

enum MST_PEER_DEV_TYPE
{
	PEER_NO_DEV_CONNECTED			= 0,
	PEER_SOURCE_DEV					= 1,
	PEER_BRANCHING_DEV				= 2,
	PEER_STREAM_SINK_DEV			= 3,
	PEER_DP_TO_LEGECY_CONV			= 4,
	PEER_DP_TO_WIRELESS_CONV		= 5,
	PEER_WIRELESS_TO_DP_CONV		= 6
};

enum SER_DES_INPUT_INDEX
{
	SER_INPUT_INDEX_0		= 0,
	DES_INPUT_INDEX_0		= 1,
	DES_INPUT_INDEX_1		= 2,
	DES_INPUT_INDEX_2		= 3,
	DES_INPUT_INDEX_3		= 4,
	SER_DES_INPUT_INDEX_MAX		= 5
};

enum VIDEO_RGB_TYPE
{
	RGB_STANDARD_LEGACY		= 0, /* ITU 601 */
	RGB_STANDARD_S			= 1,  /* ITU 709 */	
	RGB_STANDARD_MAX		= 2
};

enum VIDEO_COLORIMETRY_TYPE
{
	COLORIMETRY_ITU601		= 1,
	COLORIMETRY_ITU709		= 2,
	COLORIMETRY_MAX			= 3
};

enum VIDEO_PIXEL_COLOR_DEPTH
{
	PIXEL_COLOR_DEPTH_8				= 8,
	PIXEL_COLOR_DEPTH_MAX			= 9
};

enum VIDEO_LINK_BPP
{
	VIDEO_LINK_BPP_INVALID		= 0,
	VIDEO_LINK_BPP_YCbCr422				= 2, 
	VIDEO_LINK_BPP_RGB_YCbCr444			= 3
};

enum VIDEO_SINK_DPCD_BPC
{
	VIDEO_SINK_DPCD_8BPC		= 0,
	VIDEO_SINK_DPCD_10BPC		= 1,
	VIDEO_SINK_DPCD_12BPC		= 2,
	VIDEO_SINK_DPCD_16BPC		= 3
};

enum VIDEO_FORMAT_TYPE
{
	VIDEO_FORMAT_CEA_861		= 0,	/* CEA-861-F */
	VIDEO_FORMAT_VESA_CVT		= 1,	/* VESA CVT */
	VIDEO_FORMAT_VESA_DMT		= 2,	/* VESA DMT */
	VIDEO_FORMAT_MAX
};

enum VIDEO_BIT_PER_COMPONET
{
	BIT_PER_COMPONENT_8BPC	= 8,
	BIT_PER_COMPONENT_10BPC = 10,
	BIT_PER_COMPONENT_12BPC = 12,
	BIT_PER_COMPONENT_16BPC = 16
};

enum VIDEO_REFRESH_RATE
{
	VIDEO_REFRESH_RATE_30_00HZ = 30000,
	VIDEO_REFRESH_RATE_49_92HZ = 49920,
	VIDEO_REFRESH_RATE_50_00HZ = 50000,
	VIDEO_REFRESH_RATE_50_08HZ = 50080,
	VIDEO_REFRESH_RATE_59_94HZ = 59940,
	VIDEO_REFRESH_RATE_60_00HZ = 60000,
	VIDEO_REFRESH_RATE_60_54HZ = 60540,
	VIDEO_REFRESH_RATE_MAX			= 60541 
};

enum VIDEO_PIXEL_ENCODING_TYPE
{
	PIXEL_ENCODING_TYPE_RGB				= 0,
	PIXEL_ENCODING_TYPE_YCBCR422		= 1,
	PIXEL_ENCODING_TYPE_YCBCR444		= 2,
	PIXEL_ENCODING_TYPE_MAX
};

enum VIDEO_MULTI_PIXEL_TYPE
{
	MULTI_PIXEL_TYPE_SINGLE	= 0,
	MULTI_PIXEL_TYPE_DUAL		= 1,
	MULTI_PIXEL_TYPE_QUAD		= 2,
	MULTI_PIXEL_TYPE_MAX		= 3
};

enum DPTX_VIDEO_CTS_PATTERN_MODE
{
	DP_TRAIN_NONE_LNANE_REMAINS	= 0,
	DP_TRAIN_LNANE0_REMAINS = 1,
	DP_TRAIN_LNANE1_REMAINS = 2,
	DP_TRAIN_LNANE2_REMAINS = 4,
	DP_TRAIN_LNANE3_REMAINS = 8
};

enum AUDIO_INPUT_MUTE
{
	AUDIO_INPUT_CLEAR_MUTE_FLAG_VBID		= 0,
	AUDIO_INPUT_SET_MUTE_FLAG_VBID			= 1,
	AUDIO_INPUT_SET_MUTE_FLAG_MAX			= 2
};

enum AUDIO_INPUT_INTERFACE
{
	AUDIO_INPUT_INTERFACE_I2S		= 0,
	AUDIO_INPUT_INTERFACE_INVALID
};

enum AUDIO_MAX_INPUT_DATA_WIDTH
{
	MAX_INPUT_DATA_WIDTH_16BIT = 16,
	MAX_INPUT_DATA_WIDTH_17BIT = 17,
	MAX_INPUT_DATA_WIDTH_18BIT = 18,
	MAX_INPUT_DATA_WIDTH_19BIT = 19,
	MAX_INPUT_DATA_WIDTH_20BIT = 20,
	MAX_INPUT_DATA_WIDTH_21BIT = 21,
	MAX_INPUT_DATA_WIDTH_22BIT = 22,
	MAX_INPUT_DATA_WIDTH_23BIT = 23,
	MAX_INPUT_DATA_WIDTH_24BIT = 24,
	MAX_INPUT_DATA_WIDTH_INVALID = 25
};

enum AUDIO_INPUT_MAX_NUM_OF_CH
{
	INPUT_MAX_1_CH			= 0,
	INPUT_MAX_2_CH			= 1,
	INPUT_MAX_3_CH			= 2,
	INPUT_MAX_4_CH			= 3,
	INPUT_MAX_5_CH			= 4,
	INPUT_MAX_6_CH			= 5,
	INPUT_MAX_7_CH			= 6,
	INPUT_MAX_8_CH			= 7
};

enum AUDIO_EDID_MAX_SAMPLE_FREQ
{
	SAMPLE_FREQ_32			= 0,
	SAMPLE_FREQ_44_1		= 1,
	SAMPLE_FREQ_48			= 2,
	SAMPLE_FREQ_88_2		= 3,
	SAMPLE_FREQ_96			= 4,
	SAMPLE_FREQ_176_4		= 5,
	SAMPLE_FREQ_192			= 6,
	SAMPLE_FREQ_INVALID		= 7
};

enum AUDIO_IEC60958_3_SAMPLE_FREQ
{
	IEC60958_3_SAMPLE_FREQ_44_1			= 0,
	IEC60958_3_SAMPLE_FREQ_88_2 		= 1,
	IEC60958_3_SAMPLE_FREQ_22_05		= 2,
	IEC60958_3_SAMPLE_FREQ_176_4		= 3,
	IEC60958_3_SAMPLE_FREQ_48			= 4,
	IEC60958_3_SAMPLE_FREQ_96			= 5,
	IEC60958_3_SAMPLE_FREQ_24			= 6,
	IEC60958_3_SAMPLE_FREQ_192			= 7,
	IEC60958_3_SAMPLE_FREQ_32			= 12,
	IEC60958_3_SAMPLE_FREQ_INVALID		= 13
};

enum AUDIO_IEC60958_3_ORIGINAL_SAMPLE_FREQ
{
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_16			= 1,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_32			= 3,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_12			= 4,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_11_025		= 5,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_8			= 6,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_192			= 8,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_24			= 9,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_96			= 10,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_48			= 11,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_176_4		= 12,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_22_05		= 13,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_88_2			= 14,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_44_1			= 15
};

enum DMT_ESTABLISHED_TIMING
{
	DMT_640x480_60hz,
	DMT_800x600_60hz,
	DMT_1024x768_60hz,
	DMT_NOT_SUPPORTED
};

enum HPD_Detection_Status
{
	HPD_STATUS_UNPLUGGED = 0,
	HPD_STATUS_PLUGGED
};

enum HDCP_Detection_Status
{
	HDCP_STATUS_NOT_DETECTED = 0,
	HDCP_STATUS_DETECTED
};

enum AUX_REPLY_Status
{
	AUX_REPLY_NOT_RECEIVED = 0,
	AUX_REPLY_RECEIVED
};


typedef void (*Dptx_HPD_Intr_Callback)( u8 ucDP_Index, bool bHPD_State );


struct Dptx_Link_Params 
{
	u8		aucTraining_Status[DP_LINK_STATUS_SIZE];
	u8		ucLinkRate;
	u8		ucNumOfLanes;
	u8		aucPreEmphasis_level[MAX_PRE_EMPHASIS_LEVEL];
	u8		aucVoltageSwing_level[MAX_VOLTAGE_SWING_LEVEL];
};

struct Dptx_Aux_Params
{
	u32			uiAuxStatus;
	u32			auiReadData[4];
};

struct Dptx_Avgen_SDP_FullData 
{
	bool			ucEnabled;
	u8				ucBlanking;
	u8				ucCount;
	u32				auiPayload[DPTX_SDP_LEN];
};

struct Dptx_Hdcp_Aksv 
{
	u32					lsb;
	u32					msb;
};

struct Dptx_Hdcp_Dpk 
{
	u32					lsb;
	u32					msb;
};

struct Dptx_Hdcp_Params 
{
	struct Dptx_Hdcp_Aksv	stHdcpAksv;
	struct Dptx_Hdcp_Dpk	astHdcpDpk[40];
	u32						uiHdcp_EncKey;
	u8						ucAuthen_fail_count;
	u8						ucHDCP13_Enable;
	u8						ucHDCP22_Enable;
};

struct Dptx_Dtd_Params /* Display Timing Data */
{
	u8						interlaced;			/* 1 : interlaced, 0 : progressive */
	u8						h_sync_polarity;
	u8						v_sync_polarity;
	u16						pixel_repetition_input;
	u16						h_active;
	u16						h_blanking;
	u16						h_image_size;
	u16						h_sync_offset;
	u16						h_sync_pulse_width;
	u16						v_active;
	u16						v_blanking;
	u16						v_image_size;
	u16						v_sync_offset;
	u16						v_sync_pulse_width;
	u32						uiPixel_Clock;
};

struct Dptx_Video_Params 
{
	u8						ucMultiPixel; /* Controls multipixel configuration. 0-Single, 1-Dual, 2-Quad */
	u8						ucPixel_Encoding;		/* 0 : RGB, 1 : YCC444,  2 : YCC422,  3 : YCC420, 4 : YOnly, 5 : RAW*/
	u8						ucBitPerComponent;		/* Bit Per Component ==> 6 : Depth 6, 8 : Depth 8, 10 : Depth 10, 12 : Depth 12, 16 : Depth 16 */	
	u8						ucRGB_Standard;			/* 0 : Legacy( VESA ) 1 : sRGB( CEA ) */
	u8						ucAverage_BytesPerTu;
	u8						ucAver_BytesPer_Tu_Frac;
	u8						ucInit_Threshold;
	u8						ucVideo_Format; 		/* 0 : CEA-861,  1 : VESA CVT,  2 : VESA DMT */
	u8						ucColorimetry;			/* 1 : ITU-R BT.601, 2 : ITU-R BT.709 */
	u32 					uiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX];
	u32						auiVideo_Code[PHY_INPUT_STREAM_MAX];			/* Video code */	
	u32 					uiRefresh_Rate;	/* 49920, 50080, 59940, 60054  */

	struct Dptx_Dtd_Params	stDtdParams[PHY_INPUT_STREAM_MAX];	
};

struct Dptx_Audio_Params 
{
	uint8_t ucInput_InterfaceType;			/* 0 : I2S is selected for audio input interface( Default ), 1 : SPDIF is selected */
	uint8_t ucInput_Max_NumOfchannels;		/* 0 : 1 channels, 1 : 2 channels, others : 8 channels --> updated by edid */
	uint8_t ucInput_DataWidth;				/* Input Audio data width */
	uint8_t ucInput_TimestampVersion;		/* Audio timestamp version number - according to the DP Spec 1.4, it should 0x12 */
	uint8_t ucInput_HBR_Mode;				/* 0 : I2S - 92kHz x 8 x 16 bits in 8-channels is delivered to controller, 1 : SPDIF */
	uint8_t ucIEC_Sampling_Freq;			/* IEC sampling frequency */	
	uint8_t ucIEC_OriginSamplingFreq;
};

struct Dptx_Params
{
	struct mutex	Mutex;
	struct device	*pstParentDev;
	wait_queue_head_t	WaitQ;
	
	atomic_t		HPD_IRQ_State;
	atomic_t		Sink_request;

	u32				uiHPD_IRQ;
	u32				uiTCC805X_Revision;

	bool			bUsed_TCC_DRM_Interface;
	bool			bSideBand_MSG_Supported;
	bool			bSerDes_Reset_STR;
	bool			bPHY_Lane_Reswap;
	bool			bSDM_Bypass;
	bool			bSRVC_Bypass;

	char			*pcDevice_Name;

	void __iomem	*pioDPLink_BaseAddr;/* DP register base address */
	void __iomem	*pioMIC_SubSystem_BaseAddr;/* Micom sub-system register base address */
	void __iomem	*pioPMU_BaseAddr;/* PMU register base address */
	
	u32 			uiHDCP22_RegAddr_Offset;
	u32 			uiRegBank_RegAddr_Offset;
	u32 			uiCKC_RegAddr_Offset;
	u32 			uiProtect_RegAddr_Offset;

	bool			bSpreadSpectrum_Clock;	
	bool			bMultStreamTransport; 		/* Multi Stream Transport */

	uint8_t			ucNumOfStreams;
	u8				ucNumOfPorts;
	u8				ucMax_Rate;		/* The maximum rate that the controller supports -> Default setting is DPTX_PHYIF_CTRL_RATE_HBR as 0x01 ==> 0 - RBR, 1 - HBR, 2 - HBR2, 3 - HBR3 */
	u8				ucMax_Lanes;	/* The maximum lane count that the controller supports -> Default setting is 4 */
	u8				*pucEdidBuf;	/* 512Bytes EDID data */
	u8				*paucEdidBuf_Entry[PHY_INPUT_STREAM_MAX];
    u8				aucStreamSink_PortNumber[PHY_INPUT_STREAM_MAX];
	u8				aucRAD_PortNumber[PHY_INPUT_STREAM_MAX];
	u8				aucVCP_Id[PHY_INPUT_STREAM_MAX];
	u8				aucMuxInput_Index[PHY_INPUT_STREAM_MAX];
	u8				aucDPCD_Caps[DPTX_SINK_CAP_SIZE];
	u8				aucNumOfSlots[PHY_INPUT_STREAM_MAX];
	u16				ausPayloadBandwidthNumber[PHY_INPUT_STREAM_MAX];

	enum HPD_Detection_Status eLast_HPDStatus;
	enum DMT_ESTABLISHED_TIMING			eEstablished_Timing;
	
	struct Dptx_Video_Params			stVideoParams;
	struct Dptx_Audio_Params			stAudioParams;
	struct Dptx_Hdcp_Params				stHdcpParams;
	struct Dptx_Aux_Params				stAuxParams;
	struct Dptx_Link_Params				stDptxLink;

	struct proc_dir_entry				*pstDP_Proc_Dir;
	struct proc_dir_entry				*pstDP_HPD_Dir;
	struct proc_dir_entry				*pstDP_Topology_Dir;
	struct proc_dir_entry				*pstDP_EDID_Dir;
	struct proc_dir_entry				*pstDP_LinkT_Dir;
	struct proc_dir_entry				*pstDP_Video_Dir;
	struct proc_dir_entry				*pstDP_Auio_Dir;

	Dptx_HPD_Intr_Callback				pvHPD_Intr_CallBack;
};


bool Dptx_Platform_Init_Params( struct Dptx_Params	*pstDptx, struct device	*pstParentDev );
bool Dptx_Platform_Init( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Deinit( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_ProtectRegister_PW( struct Dptx_Params	*pstDptx, u32 uiProtect_Cfg_PW );
bool Dptx_Platform_Set_ProtectRegister_CfgAccess( struct Dptx_Params	*pstDptx, bool bAccessable );
bool Dptx_Platform_Set_ProtectRegister_CfgLock( struct Dptx_Params	*pstDptx, bool bAccessable );
bool Dptx_Platform_Set_RegisterBank(	struct Dptx_Params	*pstDptx, enum PHY_LINK_RATE eLinkRate );
bool Dptx_Platform_Set_PLL_Divisor(	struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_PLL_ClockSource( struct Dptx_Params	*pstDptx, u8 ucClockSource );
bool Dptx_Platform_Set_APAccess_Mode( struct Dptx_Params *pstDptx );
bool Dptx_Platform_Set_PMU_ColdReset_Release(	struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_ClkPath_To_XIN(	struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_PHY_StandardLane_PinConfig( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_SDMBypass_Ctrl( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_SRVCBypass_Ctrl( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Set_MuxSelect( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Get_PLLLock_Status( struct Dptx_Params	*pstDptx, bool *pbPll_Locked );
bool Dptx_Platform_Get_PHY_StandardLane_PinConfig( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Get_SDMBypass_Ctrl( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Get_SRVCBypass_Ctrl( struct Dptx_Params	*pstDptx );
bool Dptx_Platform_Get_MuxSelect( struct Dptx_Params	*pstDptx );
void Dptx_Platform_Set_Device_Handle( struct Dptx_Params	*pstDptx );
struct Dptx_Params *Dptx_Platform_Get_Device_Handle( void );
bool Dptx_Platform_Free_Handle( struct Dptx_Params	*pstDptx_Handle );



/* Dptx Core */
bool Dptx_Core_Link_Power_On( struct Dptx_Params *pstDptx );
bool Dptx_Core_Init_Params( struct Dptx_Params *pstDptx );
bool Dptx_Core_Init( struct Dptx_Params *pstDptx );
bool Dptx_Core_Deinit( struct Dptx_Params *pstDptx );
void Dptx_Core_Soft_Reset( struct Dptx_Params *pstDptx, u32 uiReset_Bits );
void Dptx_Core_Init_PHY( struct Dptx_Params *pstDptx );
void Dptx_Core_Enable_Global_Intr( struct Dptx_Params *pstDptx, u32 uiEnable_Bits );
void Dptx_Core_Disable_Global_Intr( struct Dptx_Params *pstDptx );
bool Dptx_Core_Clear_General_Interrupt( struct Dptx_Params *pstDptx, u32 uiClear_Bits );
bool Dptx_Core_Get_PHY_BUSY_Status( struct Dptx_Params *dptx, u8 ucNumOfLanes );
bool Dptx_Core_Get_PHY_NumOfLanes( struct Dptx_Params *dptx, u8 *pucNumOfLanes );
bool Dptx_Core_Get_Sink_SSC_Capability( struct Dptx_Params *dptx, bool *pbSSC_Profiled );
bool Dptx_Core_Get_PHY_Rate( struct Dptx_Params *dptx, u8 *pucPHY_Rate );
bool Dptx_Core_Get_Stream_Mode( struct Dptx_Params *pstDptx, bool *pbMST_Mode );

bool Dptx_Core_Set_PHY_PowerState( struct Dptx_Params *pstDptx, enum PHY_POWER_STATE ePowerState );
bool Dptx_Core_Set_PHY_NumOfLanes( struct Dptx_Params *pstDptx, u8 ucNumOfLanes );
bool Dptx_Core_Set_PHY_SSC( struct Dptx_Params *dptx, bool bSink_Supports_SSC );
bool Dptx_Core_Get_PHY_SSC( struct Dptx_Params *pstDptx, bool *pbSSC_Enabled );
bool Dptx_Core_Set_PHY_Rate( struct Dptx_Params *pstDptx, enum PHY_LINK_RATE eRate );
bool Dptx_Core_Set_PHY_PreEmphasis( struct Dptx_Params *pstDptx,  			        unsigned int iLane_Index, enum PHY_PRE_EMPHASIS_LEVEL ePreEmphasisLevel );
bool Dptx_Core_Set_PHY_VSW( struct Dptx_Params *pstDptx, unsigned int iLane_Index, enum PHY_VOLTAGE_SWING_LEVEL eVoltageSwingLevel );
bool Dptx_Core_Set_PHY_Pattern( struct Dptx_Params *pstDptx, u32 uiPattern );
bool Dptx_Core_Enable_PHY_XMIT( struct Dptx_Params *pstDptx, u32 iNumOfLanes );
bool Dptx_Core_Disable_PHY_XMIT( struct Dptx_Params *pstDptx, u32 iNumOfLanes );
bool Dptx_Core_PHY_Rate_To_Bandwidth( struct Dptx_Params *pstDptx, u8 ucRate, u8 *pucBandWidth );
bool Dptx_Core_PHY_Bandwidth_To_Rate( struct Dptx_Params *pstDptx, u8 ucBandWidth, u8 *pucRate );


/* Dptx AV Generator */
bool Dptx_Avgen_Init_Video_Params( struct Dptx_Params *pstDptx, u32 uiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX] );
bool Dptx_Avgen_Set_Video_Stream_Enable( struct Dptx_Params *pstDptx, bool bEnable_Stream, u8 ucStream_Index );
bool Dptx_Avgen_Get_Video_Stream_Enable( struct Dptx_Params *pstDptx, bool *pbEnable_Stream, u8 ucStream_Index );
bool Dptx_Avgen_Set_Video_Detailed_Timing( struct Dptx_Params *pstDptx, u8 ucStream_Index, struct Dptx_Dtd_Params *pstDtd_Params );
bool Dptx_Avgen_Set_Video_Timing( struct Dptx_Params *pstDptx, u8 ucStream_Index );
bool Dptx_Avgen_Get_Pixel_Mode( struct Dptx_Params *pstDptx, u8 ucStream_Index, u8 *pucPixelMode );
bool Dptx_Avgen_Get_Video_NumOfStreams( struct Dptx_Params *pstDptx, u8 *pucEnabled_Streams );
bool Dptx_Avgen_Get_Video_PixelEncoding_Type( struct Dptx_Params *pstDptx, u8 ucStream_Index, u8 *pucPixel_Encoding );
bool Dptx_Avgen_Get_Video_Configured_Timing( struct Dptx_Params *pstDptx, u8 ucStream_Index, struct Dptx_Dtd_Params		*pstDtd );
bool Dptx_Avgen_Calculate_Video_Average_TU_Symbols( struct Dptx_Params *pstDptx, int iNumOfLane, int iLinkRate, int iBpc, int iEncodingType, int iPixel_Clock, u8 ucStream_Index );

void Dptx_Avgen_Configure_Audio(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, struct Dptx_Audio_Params *pstAudioParams);
void Dptx_Avgen_Get_Audio_SamplingFreq(struct Dptx_Params *pstDptx, uint8_t	*pucSamplingFreq, uint8_t	*pucOrgSamplingFreq );
void Dptx_Avgen_Set_Audio_MaxNumOfChannels(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucInput_Max_NumOfCh);
void Dptx_Avgen_Get_Audio_MaxNumOfChannels(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucInput_Max_NumOfCh);
void Dptx_Avgen_Set_Audio_DataWidth(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucInput_DataWidth);
void Dptx_Avgen_Get_Audio_DataWidth(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucInput_DataWidth);
void Dptx_Avgen_Set_Audio_Input_InterfaceType(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t ucAudInput_InterfaceType);
void Dptx_Avgen_Get_Audio_Input_InterfaceType(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t *pucAudInput_InterfaceType);
void Dptx_Avgen_Set_Audio_HBR_En(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t ucEnable);
void Dptx_Avgen_Get_Audio_HBR_En(struct Dptx_Params* pstDptx, uint8_t ucStream_Index, uint8_t *pucEnable);
void Dptx_Avgen_Set_Audio_SDP_InforFrame(struct Dptx_Params *pstDptx,
																uint8_t ucIEC_Sampling_Freq,
																uint8_t ucIEC_OriginSamplingFreq,
																uint8_t ucInput_Max_NumOfchannels,
																uint8_t ucInput_DataWidth);
void Dptx_Avgen_Set_Audio_Timestamp_Ver(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucAudInput_TimestampVersion);
void Dptx_Avgen_Set_Audio_Stream_Enable( struct Dptx_Params *pstDptx, u8 ucSatrem_Index, bool bEnable );
void Dptx_Avgen_Set_Audio_Mute(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucMute);
void Dptx_Avgen_Get_Audio_Mute(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucMute);
void Dptx_Avgen_Enable_Audio_SDP(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable);
void Dptx_Avgen_Enable_Audio_Timestamp(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t ucEnable);
bool Dptx_Avgen_Get_VIC_From_Dtd( struct Dptx_Params *pstDptx, u8 ucStream_Index, u32 *puiVideo_Code );
bool Dptx_Avgen_Fill_Dtd( struct Dptx_Dtd_Params *pstDtd, u32 uiVideo_Code, u32 uiRefreshRate, u8 ucVideoFormat );


/* Dptx Link */
bool Dptx_Link_Perform_Training( struct Dptx_Params *pstDptx, u8 ucRate, u8 ucNumOfLanes );
bool Dptx_Link_Perform_BringUp( struct Dptx_Params *pstDptx, bool bSink_MST_Supported );
bool Dptx_Link_Get_LinkTraining_Status( struct Dptx_Params *pstDptx, bool *pbTrainingState );


/* Dptx Interrupt */
irqreturn_t Dptx_Intr_IRQ( int irq, void *dev );
irqreturn_t Dptx_Intr_Threaded_IRQ( int irq, void *dev );
bool Dptx_Intr_Get_Port_Composition( struct Dptx_Params *pstDptx );
bool Dptx_Intr_Register_HPD_Callback( struct Dptx_Params *pstDptx, Dptx_HPD_Intr_Callback HPD_Intr_Callback );
bool Dptx_Intr_Handle_HotUnplug( struct Dptx_Params *pstDptx );
int32_t Dptx_Intr_Get_HotPlug_Status(struct Dptx_Params *pstDptx, uint8_t *pucHotPlug_Status);


/* Dptx EDID */
bool Dptx_Edid_Verify_BaseBlk_Data( u8 *pucEDID_Buf );
bool Dptx_Edid_Read_EDID_I2C_Over_Aux( struct Dptx_Params *pstDptx );
bool Dptx_Edid_Read_EDID_Over_Sideband_Msg( struct Dptx_Params *pstDptx, u8 ucStream_Index, bool bSkipped_PortComposition );


/* Dptx HDCP */
bool Dptx_Hdcp_Init( struct Dptx_Params *pstDptx );
bool Dptx_Hdcp_Init_Authenticate( struct Dptx_Params *pstDptx );
void Dptx_Hdcp_Set_HDCP13_State( struct Dptx_Params *pstDptx, bool bEnable );
void Dptx_Hdcp_Set_HDCP22_State( struct Dptx_Params *pstDptx, bool bEnable );
bool Dptx_Hdcp_Configure_HDCP13( struct Dptx_Params *pstDptx );


/* Dptx Extension */
bool Dptx_Ext_Clear_VCP_Tables( struct Dptx_Params *pstDptx );
bool Dptx_Ext_Initiate_MST_Act( struct Dptx_Params *pstDptx );
bool Dptx_Ext_Set_Stream_Mode( struct Dptx_Params *pstDptx, bool bMST_Supported, u8 ucNumOfPorts );
bool Dptx_Ext_Get_Stream_Mode( struct Dptx_Params *pstDptx, bool *pbMST_Supported, u8 *pucNumOfPorts );
bool Dptx_Ext_Get_Sink_Stream_Capability( struct Dptx_Params *pstDptx, bool *pbMST_Supported );
bool Dptx_Ext_Set_Stream_Capability( struct Dptx_Params *pstDptx );
bool Dptx_Ext_Set_Link_VCP_Tables( struct Dptx_Params *pstDptx, u8 ucStream_Index );
bool Dptx_Ext_Set_Sink_VCP_Table_Slots( struct Dptx_Params *pstDptx, u8 ucStream_Index );
bool Dptx_Ext_Get_TopologyState( struct Dptx_Params *pstDptx, u8 *pucNumOfHotpluggedPorts );
bool Dptx_Ext_Set_Topology_Configuration( struct Dptx_Params *pstDptx, u8 ucNumOfPorts, bool bSideBand_MSG_Supported );
bool Dptx_Ext_Remote_I2C_Read( struct Dptx_Params *pstDptx, u8 ucStream_Index, bool bSkipped_PortComposition );
int32_t Dptx_Ext_Proc_Interface_Init(struct Dptx_Params *pstDptx);



/* Dptx Register */
u32  Dptx_Reg_Readl( struct Dptx_Params *pstDptx, u32 uiOffset );
void Dptx_Reg_Writel( struct Dptx_Params *pstDptx, u32 uiOffset, u32 uiData );
u32 Dptx_Reg_Direct_Read( volatile void __iomem *Reg );
void Dptx_Reg_Direct_Write( volatile void __iomem *Reg, u32 uiData );

/* Dptx Aux */
bool Dptx_Aux_Read_DPCD( struct Dptx_Params *pstDptx, u32 uiAddr, u8 *pucBuffer );
bool Dptx_Aux_Read_Bytes_From_DPCD( struct Dptx_Params *pstDptx, u32 uiAddr, u8 *pucBuffer, u32 len );
bool Dptx_Aux_Write_DPCD( struct Dptx_Params *pstDptx, u32 uiAddr, u8 ucBuffer );
bool Dptx_Aux_Write_Bytes_To_DPCD( struct Dptx_Params *pstDptx, u32 uiAddr, u8 *pucBuffer, u32 uiLength );
int  Dptx_Aux_Read_Bytes_From_I2C( struct Dptx_Params *pstDptx, u32 uiDevice_Addr, u8 *pucBuffer, u32 uiLength );
int  Dptx_Aux_Write_Bytes_To_I2C( struct Dptx_Params *pstDptx, u32 uiDevice_Addr, u8 *pucBuffer, u32 uiLength );
int  Dptx_Aux_Write_AddressOnly_To_I2C( struct Dptx_Params *pstDptx,        		          unsigned int uiDevice_Addr );


int Dpv14_Tx_Suspend_T( struct Dptx_Params	*pstDptx );
int Dpv14_Tx_Resume_T( struct Dptx_Params	*pstDptx );


/* Dptx SerDes */
bool Dptx_Max968XX_Reset( struct Dptx_Params *pstDptx );
bool Dptx_Max968XX_Get_TopologyState( u8 *pucNumOfPluggedPorts );
//bool Dptx_Max968XX_Get_Des_VideoLocked( bool *pbVideoLocked );
bool Touch_Max968XX_update_reg(struct Dptx_Params *pstDptx);

#endif /* __DPTX_API_H__  */
