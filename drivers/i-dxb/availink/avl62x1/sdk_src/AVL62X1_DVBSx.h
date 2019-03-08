/*
 *           Copyright 2007-2017 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2017-8-3 15:59 $
///

#ifndef AVL62X1_DVBSX_H
#define AVL62X1_DVBSX_H

#include "AVL62X1_Internal.h"
#include "AVL62X1_Reg.h"
#include "AVL_Tuner.h"

#ifdef AVL_CPLUSPLUS
extern "C" {
#endif

#define AVL62X1_PL_SCRAM_XSTATE            (0x040000)
#define AVL62X1_PL_SCRAM_AUTO              (0x800000)
#define AVL62X1_PL_SCRAM_LFSR_MASK         (0x03FFFF)

#define AVL62X1_IF_SHIFT_MAX_SR_HZ			(15*1000*1000)

	/* command enumeration definitions */
#define CMD_IDLE             0
#define CMD_LD_DEFAULT       1
#define CMD_ACQUIRE          2
#define CMD_HALT             3 
#define CMD_DEBUG            4
#define CMD_SLEEP            7
#define CMD_WAKE             8
#define CMD_BLIND_SCAN       9
#define CMD_ROM_CRC          10
#define CMD_DEBUG_FW         15
#define CMD_ADC_TEST         20
#define CMD_DMA              21
#define CMD_CALC_CRC         22
#define CMD_PING             23
#define CMD_DECOMPRESS       24
#define CMD_CAPTURE_IQ       25
#define CMD_FAILED			 255

#define SP_CMD_IDLE			 0
#define SP_CMD_LD_DEFAULT    1
#define SP_CMD_ACQUIRE       2
#define SP_CMD_HALT          3
#define SP_CMD_BLIND_SCAN_CLR 4

	/*
	* Patch file stuff
	*/
#define PATCH_CMD_VALIDATE_CRC              0
#define PATCH_CMD_PING                      1 
#define PATCH_CMD_LD_TO_DEVICE              2 
#define PATCH_CMD_DMA                       3 
#define PATCH_CMD_DECOMPRESS                4
#define PATCH_CMD_ASSERT_CPU_RESET          5 
#define PATCH_CMD_RELEASE_CPU_RESET         6
#define PATCH_CMD_LD_TO_DEVICE_IMM          7
#define PATCH_CMD_RD_FROM_DEVICE            8
#define PATCH_CMD_DMA_HW                    9
#define PATCH_CMD_SET_COND_IMM              10
#define PATCH_CMD_EXIT                      11
#define PATCH_CMD_POLL_WAIT                 12
#define PATCH_CMD_LD_TO_DEVICE_PACKED       13

#define PATCH_CMP_TYPE_ZLIB                 0
#define PATCH_CMP_TYPE_ZLIB_NULL            1
#define PATCH_CMP_TYPE_GLIB                 2
#define PATCH_CMP_TYPE_NONE                 3

#define PATCH_VAR_ARRAY_SIZE                32

	//Addr modes 2 bits
#define PATCH_OP_ADDR_MODE_VAR_IDX          0
#define PATCH_OP_ADDR_MODE_IMMIDIATE        1

	//Unary operators 6 bits
#define PATCH_OP_UNARY_NOP                  0
#define PATCH_OP_UNARY_LOGICAL_NEGATE       1
#define PATCH_OP_UNARY_BITWISE_NEGATE       2
#define PATCH_OP_UNARY_BITWISE_AND          3
#define PATCH_OP_UNARY_BITWISE_OR           4

	//Binary operators 1 Byte
#define PATCH_OP_BINARY_LOAD                0
#define PATCH_OP_BINARY_AND                 1
#define PATCH_OP_BINARY_OR                  2
#define PATCH_OP_BINARY_BITWISE_AND         3
#define PATCH_OP_BINARY_BITWISE_OR          4
#define PATCH_OP_BINARY_EQUALS              5
#define PATCH_OP_BINARY_STORE               6
#define PATCH_OP_BINARY_NOT_EQUALS          7

#define PATCH_COND_EXIT_AFTER_LD            8

#define PATCH_STD_DVBC                      0 
#define PATCH_STD_DVBSx                     1
#define PATCH_STD_DVBTx                     2
#define PATCH_STD_ISDBT                     3

	typedef enum AVL62X1_Slave_Addr
	{
		AVL62X1_SA_0 = 0x14,
		AVL62X1_SA_1 = 0x15
	}AVL62X1_Slave_Addr;

	typedef enum AVL62X1_Xtal
	{
		AVL62X1_RefClk_16M,
		AVL62X1_RefClk_27M,
		AVL62X1_RefClk_30M,
	}AVL62X1_Xtal;

	// Defines the device functional mode.
	typedef enum AVL62X1_FunctionalMode
	{
		AVL62X1_FuncMode_Idle       =   0,
		AVL62X1_FuncMode_Demod      =   1,              // The device is in demod mode.
		AVL62X1_FuncMode_BlindScan  =   2               // The device is in blind scan mode.
	}AVL62X1_FunctionalMode;

	/// Defines the device spectrum polarity setting. 
	typedef enum AVL62X1_SpectrumPolarity
	{
		AVL62X1_Spectrum_Normal = 0,		///< = 0 The received signal spectrum is not inverted.
		AVL62X1_Spectrum_Invert = 1		///< = 1 The received signal spectrum is inverted.
	}AVL62X1_SpectrumPolarity;

	// Defines demod lock status
	typedef enum AVL62X1_LockStatus
	{
		AVL62X1_STATUS_UNLOCK   =   0,                          // demod isn't locked
		AVL62X1_STATUS_LOCK     =   1                           // demod is in locked state.
	}AVL62X1_LockStatus;

	typedef enum AVL62X1_LostLockStatus
	{
		AVL62X1_Lost_Lock_No   =   0,                          // demod has not lost lock since last check
		AVL62X1_Lost_Lock_Yes  =   1                           // demod has lost lock since last check
	}AVL62X1_LostLockStatus;

	// Defines demod stream discovery status
	typedef enum AVL62X1_DiscoveryStatus
	{
		AVL62X1_DISCOVERY_RUNNING   =   0,                          // stream discovery is still running
		AVL62X1_DISCOVERY_FINISHED  =   1                           // stream discovery has finished
	}AVL62X1_DiscoveryStatus;

	// Defines the ON/OFF options for the EigX device.
	typedef enum AVL62X1_Switch
	{
		AVL62X1_ON  =   0,                              // switched on
		AVL62X1_OFF =   1                               // switched off
	}AVL62X1_Switch;

	typedef enum AVL62X1_Standard
	{
		AVL62X1_DVBS     =    0,                          // DVBS standard
		AVL62X1_DVBS2    =    1,                          // DVBS2 standard
		AVL62X1_QAM_Carrier = 2,
		AVL62X1_Edge_Pair   = 3
	}AVL62X1_Standard;    

	typedef enum AVL62X1_ModulationMode
	{
		AVL62X1_BPSK   =   1,
		AVL62X1_QPSK   =   2,
		AVL62X1_8PSK   =   3,
		AVL62X1_16APSK =   4,
		AVL62X1_32APSK =   5,
		AVL62X1_64APSK =   6,
		AVL62X1_128APSK=   7,
		AVL62X1_256APSK=   8
	}AVL62X1_ModulationMode;

	typedef enum AVL62X1_DVBS_CodeRate
	{
		AVL62X1_DVBS_CR_1_2  =   0, 
		AVL62X1_DVBS_CR_2_3  =   1,
		AVL62X1_DVBS_CR_3_4  =   2,
		AVL62X1_DVBS_CR_5_6  =   3,
		AVL62X1_DVBS_CR_7_8  =   4
	}AVL62X1_DVBS_CodeRate;

	typedef enum AVL62X1_DVBS2_CodeRate
	{
		AVL62X1_DVBS2_CR_1_4     =  0,
		AVL62X1_DVBS2_CR_1_3     =  1,
		AVL62X1_DVBS2_CR_2_5     =  2,
		AVL62X1_DVBS2_CR_1_2     =  3,
		AVL62X1_DVBS2_CR_3_5     =  4,
		AVL62X1_DVBS2_CR_2_3     =  5,
		AVL62X1_DVBS2_CR_3_4     =  6,
		AVL62X1_DVBS2_CR_4_5     =  7,
		AVL62X1_DVBS2_CR_5_6     =  8,
		AVL62X1_DVBS2_CR_8_9     =  9,
		AVL62X1_DVBS2_CR_9_10    = 10,
		AVL62X1_DVBS2_CR_2_9     = 11,
		AVL62X1_DVBS2_CR_13_45   = 12,
		AVL62X1_DVBS2_CR_9_20    = 13,
		AVL62X1_DVBS2_CR_90_180  = 14,
		AVL62X1_DVBS2_CR_96_180  = 15,
		AVL62X1_DVBS2_CR_11_20   = 16,
		AVL62X1_DVBS2_CR_100_180 = 17,
		AVL62X1_DVBS2_CR_104_180 = 18,
		AVL62X1_DVBS2_CR_26_45   = 19,
		AVL62X1_DVBS2_CR_18_30   = 20,
		AVL62X1_DVBS2_CR_28_45   = 21,
		AVL62X1_DVBS2_CR_23_36   = 22,
		AVL62X1_DVBS2_CR_116_180 = 23,
		AVL62X1_DVBS2_CR_20_30   = 24,
		AVL62X1_DVBS2_CR_124_180 = 25,
		AVL62X1_DVBS2_CR_25_36   = 26,
		AVL62X1_DVBS2_CR_128_180 = 27,
		AVL62X1_DVBS2_CR_13_18   = 28,
		AVL62X1_DVBS2_CR_132_180 = 29,
		AVL62X1_DVBS2_CR_22_30   = 30,
		AVL62X1_DVBS2_CR_135_180 = 31,
		AVL62X1_DVBS2_CR_140_180 = 32,
		AVL62X1_DVBS2_CR_7_9     = 33,
		AVL62X1_DVBS2_CR_154_180 = 34,
		AVL62X1_DVBS2_CR_11_45   = 35,
		AVL62X1_DVBS2_CR_4_15    = 36,
		AVL62X1_DVBS2_CR_14_45   = 37,
		AVL62X1_DVBS2_CR_7_15    = 38,
		AVL62X1_DVBS2_CR_8_15    = 39,
		AVL62X1_DVBS2_CR_32_45   = 40
	}AVL62X1_DVBS2_CodeRate;

	typedef enum AVL62X1_Pilot
	{
		AVL62X1_Pilot_OFF  =   0,                  // Pilot off
		AVL62X1_Pilot_ON   =   1                   // Pilot on
	}AVL62X1_Pilot;

	typedef enum AVL62X1_RollOff
	{
		AVL62X1_RollOff_35 = 0,
		AVL62X1_RollOff_25 = 1,
		AVL62X1_RollOff_20 = 2,
		AVL62X1_RollOff_15 = 3,
		AVL62X1_RollOff_10 = 4,
		AVL62X1_RollOff_05 = 5,
		AVL62X1_RollOff_UNKNOWN = 6
	}AVL62X1_RollOff;

	typedef enum AVL62X1_DVBS2_FECLength
	{
		AVL62X1_DVBS2_FEC_SHORT  =   0,
		AVL62X1_DVBS2_FEC_MEDIUM =   2,
		AVL62X1_DVBS2_FEC_LONG   =   1
	}AVL62X1_DVBS2_FECLength;

	typedef enum AVL62X1_DVBS2_CCMACM
	{
		AVL62X1_DVBS2_ACM  =   0,
		AVL62X1_DVBS2_CCM  =   1
	}AVL62X1_DVBS2_CCMACM; 

	typedef enum AVL62X1_DVBStreamType
	{
		AVL62X1_GENERIC_PACKETIZED = 0,
		AVL62X1_GENERIC_CONTINUOUS = 1,
		AVL62X1_GSE_HEM            = 2,
		AVL62X1_TRANSPORT          = 3,
		AVL62X1_UNKNOWN            = 4, //stream type unknown/don't care. will output BB frames directly over TS
		AVL62X1_GSE_LITE           = 5,
		AVL62X1_GSE_HEM_LITE       = 6,
		AVL62X1_T2MI               = 7,
		AVL62X1_UNDETERMINED       = 255 //don't know stream type. demod will scan for streams then output first one found
	}AVL62X1_DVBStreamType;

	/// The MPEG output format. The default value in the Availink device is \a AVL_DVBSx_MPF_TS
	typedef enum AVL62X1_MpegFormat
	{
		AVL62X1_MPF_TS = 0,		///< = 0  Transport stream format.
		AVL62X1_MPF_TSP = 1		///< = 1  Transport stream plus parity format.
	}AVL62X1_MpegFormat;

	/// The MPEG output mode. The default value in the Availink device is \a AVL_DVBSx_MPM_Parallel
	typedef enum AVL62X1_MpegMode
	{
		AVL62X1_MPM_Parallel = 0,		///< = 0  Output MPEG data in parallel mode
		AVL62X1_MPM_Serial = 1,		///< = 0  Output MPEG data in serial mode
		AVL62X1_MPM_2BitSerial = 2     //output in 2bit serial mode
	}AVL62X1_MpegMode;

	/// The MPEG output clock polarity. The clock polarity should be configured to meet the back end device's requirement.
	/// The default value in the Availink device is \a AVL_DVBSx_MPCP_Rising
	typedef enum AVL62X1_MpegClockPolarity
	{
		AVL62X1_MPCP_Falling = 0,		///<  = 0  The MPEG data is valid on the falling edge of the clock.
		AVL62X1_MPCP_Rising = 1		///<  = 1  The MPEG data is valid on the rising edge of the clock.
	}AVL62X1_MpegClockPolarity;

	// Define MPEG bit order
	typedef enum AVL62X1_MpegBitOrder
	{
		AVL62X1_MPBO_LSB    =   0,      /// least significant bit first when serial transport mode is used
		AVL62X1_MPBO_MSB    =   1       /// most significant bit first when serial transport mode is used
	}AVL62X1_MpegBitOrder;

	///
	/// Defines the pin on which the Availink device outputs the MPEG data when the MPEG interface has been configured to operate 
	/// in serial mode.
	typedef enum AVL62X1_MpegSerialPin
	{
		AVL62X1_MPSP_DATA0 = 0,        ///< = 0 Serial data is output on pin MPEG_DATA_0
		AVL62X1_MPSP_DATA7 = 1	        ///< = 1 Serial data is output on pin MPEG_DATA_7
	}AVL62X1_MpegSerialPin;

	// Defines the polarity of the MPEG error signal.
	typedef enum AVL62X1_MpegPolarity
	{
		AVL62X1_MPEP_Normal     =   0,                  // The MPEG error signal is high during the payload of a packet which contains uncorrectable error(s).
		AVL62X1_MPEP_Invert     =   1                   // The MPEG error signal is low during the payload of a packet which contains uncorrectable error(s).
	}AVL62X1_MpegPolarity;

	// Defines whether the feeback bit of the LFSR used to generate the BER/PER test pattern is inverted.
	typedef enum AVL62X1_LFSR_FbBit
	{
		AVL62X1_LFSR_FB_NOT_INVERTED    =   0,          // LFSR feedback bit isn't inverted
		AVL62X1_LFSR_FB_INVERTED        =   1           // LFSR feedback bit is inverted
	}AVL62X1_LFSR_FbBit;

	// Defines the test pattern being used for BER/PER measurements.
	typedef enum AVL62X1_TestPattern
	{
		AVL62X1_TEST_LFSR_15    =   0,                  // BER test pattern is LFSR15
		AVL62X1_TEST_LFSR_23    =   1                   // BER test pattern is LFSR23        
	}AVL62X1_TestPattern;

	// Defines the type of auto error statistics 
	typedef enum AVL62X1_AutoErrorStat_Type
	{
		AVL62X1_ERROR_STAT_BYTE     =   0,                      // error statistics will be reset according to the number of received bytes.
		AVL62X1_ERROR_STAT_TIME     =   1                       // error statistics will be reset according to time interval.
	}AVL62X1_AutoErrorStat_Type;

	// Defines Error statistics mode
	typedef enum AVL62X1_ErrorStat_Mode
	{
		AVL62X1_ERROR_STAT_MANUAL   =   0,
		AVL62X1_ERROR_STAT_AUTO     =   1
	}AVL62X1_ErrorStat_Mode;

	// Defines the DiSEqC status    
	typedef enum AVL62X1_DiseqcStatus
	{
		AVL62X1_DOS_Uninitialized = 0,                  // DiSEqC has not been initialized yet.
		AVL62X1_DOS_Initialized   = 1,                  // DiSEqC has been initialized.
		AVL62X1_DOS_InContinuous  = 2,                  // DiSEqC is in continuous mode.
		AVL62X1_DOS_InTone        = 3,                  // DiSEqC is in tone burst mode.
		AVL62X1_DOS_InModulation  = 4                   // DiSEqC is in modulation mode.
	}AVL62X1_DiseqcStatus;

	typedef enum AVL62X1_Diseqc_TxGap
	{
		AVL62X1_DTXG_15ms = 0,                          // The gap is 15 ms.
		AVL62X1_DTXG_20ms = 1,                          // The gap is 20 ms.
		AVL62X1_DTXG_25ms = 2,                          // The gap is 25 ms.
		AVL62X1_DTXG_30ms = 3                           // The gap is 30 ms.
	}AVL62X1_Diseqc_TxGap;

	typedef enum AVL62X1_Diseqc_TxMode
	{
		AVL62X1_DTM_Modulation = 0,                     // Use modulation mode.
		AVL62X1_DTM_Tone0 = 1,                          // Send out tone 0.
		AVL62X1_DTM_Tone1 = 2,                          // Send out tone 1.
		AVL62X1_DTM_Continuous = 3                      // Continuously send out pulses.
	}AVL62X1_Diseqc_TxMode;

	typedef enum AVL62X1_Diseqc_RxTime
	{
		AVL62X1_DRT_150ms = 0,                          // Wait 150 ms for receive data and then close the input FIFO.
		AVL62X1_DRT_170ms = 1,                          // Wait 170 ms for receive data and then close the input FIFO.
		AVL62X1_DRT_190ms = 2,                          // Wait 190 ms for receive data and then close the input FIFO.
		AVL62X1_DRT_210ms = 3                           // Wait 210 ms for receive data and then close the input FIFO.
	}AVL62X1_Diseqc_RxTime;

	typedef enum AVL62X1_Diseqc_WaveFormMode
	{
		AVL62X1_DWM_Normal = 0,                         // Normal waveform mode
		AVL62X1_DWM_Envelope = 1                        // Envelope waveform mode
	}AVL62X1_Diseqc_WaveFormMode;

	//GPIO pins by number and name
	typedef enum AVL62X1_GPIO_Pin 
	{
		AVL62X1_GPIO_Pin_10 = 0,
		AVL62X1_GPIO_Pin_TUNER_SDA = 0,
		AVL62X1_GPIO_Pin_11 = 1,
		AVL62X1_GPIO_Pin_TUNER_SCL = 1,
		AVL62X1_GPIO_Pin_12 = 2,
		AVL62X1_GPIO_Pin_S_AGC2 = 2,
		AVL62X1_GPIO_Pin_37 = 3,
		AVL62X1_GPIO_Pin_LNB_PWR_EN = 3,
		AVL62X1_GPIO_Pin_38 = 4,
		AVL62X1_GPIO_Pin_LNB_PWR_SEL = 4
	}AVL62X1_GPIO_Pin;

	typedef enum AVL62X1_GPIO_Pin_Direction 
	{
		AVL62X1_GPIO_DIR_OUTPUT = 0,
		AVL62X1_GPIO_DIR_INPUT = 1
	}AVL62X1_GPIO_Pin_Direction;

	typedef enum AVL62X1_GPIO_Pin_Value 
	{
		AVL62X1_GPIO_VALUE_LOGIC_0 = 0,
		AVL62X1_GPIO_VALUE_LOGIC_1 = 1,
		AVL62X1_GPIO_VALUE_HIGH_Z = 2
	}AVL62X1_GPIO_Pin_Value;

	typedef enum AVL62X1_SIS_MIS
	{
		AVL62X1_MIS = 0,
		AVL62X1_SIS = 1,
		AVL62X1_SIS_MIS_UNKNOWN = 2
	}AVL62X1_SIS_MIS;

	typedef struct AVL62X1_CarrierInfo
	{
		AVL_uchar                    m_carrier_index; //index of this carrier
		AVL_uint32                   m_rf_freq_kHz; //RF frequency of the carrier in kHz
		AVL_int32                    m_carrier_freq_offset_Hz; //carrier frequency offset (from RF freq) in Hz

		//When locking with blind_sym_rate=false, this is the nominal symbol rate
		//When locking with blind_sym_rate=true, this is the max symbol rate to consider
		AVL_uint32                   m_symbol_rate_Hz;

		enum AVL62X1_RollOff            m_roll_off;
		enum AVL62X1_Standard           m_signal_type;

		//AVL62X1_PL_SCRAM_AUTO: turn on/off automatic PL scrambling detection
		//AVL62X1_PL_SCRAM_XSTATE: 1 - LSB's represent the initial state of the x(i) LFSR
		//AVL62X1_PL_SCRAM_XSTATE: 1 - LSB's represent the sequence shift of the x(i) sequence in the Gold code, defined as the "code number n" in the DVB-S2 standard
		AVL_uint32                   m_PL_scram_key;

		AVL_uchar                    m_PLS_ACM; //PLS if CCM, 0 if ACM
		enum AVL62X1_SIS_MIS            m_SIS_MIS;
		AVL_uchar                    m_num_streams; //number of supported streams (that can be output)
		AVL_int16                    m_SNR_dB_x100;  //
		enum AVL62X1_ModulationMode     m_modulation;
		enum AVL62X1_Pilot              m_pilot;
		enum AVL62X1_DVBS2_FECLength    m_dvbs2_fec_length;		
		union
		{
			enum AVL62X1_DVBS_CodeRate  m_dvbs_code_rate;
			enum AVL62X1_DVBS2_CodeRate m_dvbs2_code_rate;
		} m_coderate;
		enum AVL62X1_DVBS2_CCMACM       m_dvbs2_ccm_acm; //1:CCM, 0:ACM
	}AVL62X1_CarrierInfo;

	typedef struct AVL62X1_StreamInfo
	{
		AVL_uchar           m_carrier_index; //index of carrier (AVL62X1_CarrierInfo.m_CarrierIndex) that this stream is in
		enum AVL62X1_DVBStreamType  m_stream_type;
		AVL_uchar           m_ISI;
		AVL_uchar           m_PLP_ID;
	}AVL62X1_StreamInfo;

	typedef struct AVL62X1_ErrorStatus
	{
		AVL_uint16 m_LFSR_Sync;                         // Indicates whether the receiver is synchronized with the transmitter generating the BER test pattern.
		AVL_uint16 m_LostLock;                          // Indicates whether the receiver has lost lock since the BER/PER measurement was started.
		AVL62X1_uint64 m_SwCntNumBits;                     // A software counter which stores the number of bits which have been received.
		AVL62X1_uint64 m_SwCntBitErrors;                   // A software counter which stores the number of bit errors which have been detected.
		AVL62X1_uint64 m_NumBits;                          // The total number of bits which have been received.
		AVL62X1_uint64 m_BitErrors;                        // The total number of bit errors which have been detected.
		AVL62X1_uint64 m_SwCntNumPkts;                     // A software counter which stores the number of packets which have been received.
		AVL62X1_uint64 m_SwCntPktErrors;                   // A software counter which stores the number of packet errors which have been detected.
		AVL62X1_uint64 m_NumPkts;                          // The total number of packets which have been received.
		AVL62X1_uint64 m_PktErrors;                        // The total number of packet errors which have been detected.
		AVL_uint32 m_BER;                               // The bit error rate scaled by 1e9.
		AVL_uint32 m_PER;                               // The packet error rate scaled by 1e9.
	}AVL62X1_ErrorStatus;

	// Contains variables for storing error statistics used in the BER and PER calculations.
	typedef struct AVL62X1_ErrorStatConfig
	{
		enum AVL62X1_ErrorStat_Mode     eErrorStatMode;           // indicates the error statistics mode. 
		enum AVL62X1_AutoErrorStat_Type eAutoErrorStatType;       // indicates the MPEG data sampling clock mode.
		AVL_uint32                   uiTimeThresholdMs;        // used to set time interval for auto error statistics.
		AVL_uint32                   uiNumberThresholdByte;    // used to set the received byte number threshold for auto error statistics.
	}AVL62X1_ErrorStatConfig;

	// Contains variables for storing error statistics used in the BER and PER calculations.
	typedef struct AVL62X1_BERConfig
	{
		enum AVL62X1_TestPattern   eBERTestPattern;          // indicates the pattern of LFSR.
		enum AVL62X1_LFSR_FbBit    eBERFBInversion;          // indicates LFSR feedback bit inversion.
		AVL_uint32              uiLFSRSynced;             // indicates the LFSR synchronization status.
		AVL_uint32              uiLFSRStartPos;           //set LFSR start byte positon
	}AVL62X1_BERConfig;

	// Stores DiSEqC operation parameters
	typedef struct AVL62X1_Diseqc_Para
	{
		AVL_uint16                    uiToneFrequencyKHz;// The DiSEqC bus speed in units of kHz. Normally, it is 22kHz. 
		enum AVL62X1_Diseqc_TxGap        eTXGap;            // Transmit gap
		enum AVL62X1_Diseqc_WaveFormMode eTxWaveForm;       // Transmit waveform format
		enum AVL62X1_Diseqc_RxTime       eRxTimeout;        // Receive time frame window
		enum AVL62X1_Diseqc_WaveFormMode eRxWaveForm;       // Receive waveform format
	}AVL_Diseqc_Para;

	/// Stores the DiSEqC transmitter status.
	/// 
	typedef struct AVL62X1_Diseqc_TxStatus
	{
		AVL_uchar m_TxDone;             ///< Indicates whether the transmission is complete (1 - transmission is finished, 0 - transmission is still in progress).
		AVL_uchar m_TxFifoCount;        ///< The number of bytes remaining in the transmit FIFO
	}AVL62X1_Diseqc_TxStatus;

	/// Stores the DiSEqC receiver status
	/// 
	typedef struct AVL62X1_Diseqc_RxStatus
	{
		AVL_uchar m_RxFifoCount;        ///< The number of bytes in the DiSEqC receive FIFO.
		AVL_uchar m_RxDone;             ///< 1 if the receiver window is turned off, 0 if it is still in receiving state.
	}AVL62X1_Diseqc_RxStatus;

	// The Availink version structure.
	typedef struct AVL_VerInfo
	{
		AVL_uchar   m_Major;                            // The major version number.
		AVL_uchar   m_Minor;                            // The minor version number.
		AVL_uint16  m_Build;                            // The build version number.
	}AVL_VerInfo;

	// Stores AVL62X1 device version information.
	typedef struct AVL62X1_VerInfo
	{
		AVL_uint32  m_Chip;                             // Hardware version information. 0xPPPPSSSS (P:part number (6261), S:SVN revision in hex (23720))
		AVL_VerInfo m_API;                              // SDK version information.
		AVL_VerInfo m_Patch;                            // The version of the internal patch.
	} AVL62X1_VerInfo;

	typedef struct AVL62X1_Chip
	{
		AVL_uint16 usI2CAddr;
		enum AVL62X1_Xtal e_Xtal;                          // Reference clock
		AVL_puchar pPatchData;

		struct AVL_Tuner *pTuner;                      // Pointer to AVL_Tuner struct instance
		enum AVL62X1_SpectrumPolarity e_TunerPol;         // Tuner spectrum polarity (e.g. I/Q input swapped)

		enum AVL62X1_MpegMode e_Mode;
		enum AVL62X1_MpegClockPolarity e_ClkPol;
		enum AVL62X1_MpegFormat e_Format;
		enum AVL62X1_MpegSerialPin e_SerPin;

		//The desired MPEG clock frequency in units of Hz.
		//It is updated with the exact value after the demod has initialized
		AVL_uint32 m_MPEGFrequency_Hz;
		AVL_uint32 m_CoreFrequency_Hz;                  // The internal core clock frequency in units of Hz.
		AVL_uint32 m_FECFrequency_Hz;                   // FEC clk in Hz

		enum AVL62X1_DiseqcStatus m_Diseqc_OP_Status;

		AVL_semaphore m_semRx;                          // A semaphore used to protect the receiver command channel.
		AVL_semaphore m_semDiseqc;                      // A semaphore used to protect DiSEqC operation.

		AVL_uint32 m_variable_array[PATCH_VAR_ARRAY_SIZE];
	}AVL62X1_Chip;

	//structure to configure blind scan for a SINGLE TUNER STEP
	typedef struct AVL62X1_BlindScanParams
	{
		AVL_uint16              m_TunerCenterFreq_100kHz;
		AVL_uint16              m_TunerLPF_100kHz;
		AVL_uint16              m_MinSymRate_kHz;
	} AVL62X1_BlindScanParams;

	//structure to capture the information from blind scan of a SINGLE TUNER STEP
	typedef struct AVL62X1_BlindScanInfo
	{
		AVL_uchar               m_ScanProgress; //approx progress in percent
		AVL_uchar               m_BSFinished; //!0 when BS completed
		AVL_uchar               m_NumCarriers; //number of confirmed S or S2/X carriers
		AVL_uchar               m_NumStreams; //DEPRECATED number of confirmed DVB output streams (e.g. TS, GSE)
		AVL_uint32              m_NextFreqStep_Hz; //amount to move tuner (relative to its current position) for next BS step
	} AVL62X1_BlindScanInfo;


	AVL_ErrorCode Init_AVL62X1_ChipObject(struct AVL62X1_Chip * pAVL_ChipObject);

	AVL_ErrorCode IBase_CheckChipReady_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_Initialize_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_GetVersion_AVL62X1(struct AVL62X1_VerInfo * pVer_info, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_GetFunctionMode_AVL62X1(enum AVL62X1_FunctionalMode * pFuncMode, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_GetRxOPStatus_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_SendRxOP_AVL62X1(AVL_uchar ucOpCmd, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_GetSPOPStatus_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_SendSPOP_AVL62X1(AVL_uchar ucOpCmd, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_Halt_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_Sleep_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_Wakeup_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IBase_Initialize_TunerI2C_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);

	AVL_ErrorCode IRx_Initialize_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_GetTunerPola_AVL62X1(enum AVL62X1_SpectrumPolarity *pTuner_Pol, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_SetTunerPola_AVL62X1(enum AVL62X1_SpectrumPolarity enumTuner_Pol, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_DriveAGC_AVL62X1(enum AVL62X1_Switch enumOn_Off, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_GetCarrierFreqOffset_AVL62X1(AVL_pint32 piFreqOffsetHz, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_GetSROffset_AVL62X1(AVL_pint32 piSROffsetPPM, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_ErrorStatMode_AVL62X1(struct AVL62X1_ErrorStatConfig stErrorStatConfig, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_ResetBER_AVL62X1(struct AVL62X1_BERConfig *pBERConfig, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_GetBER_AVL62X1(AVL_puint32 puiBER_x1e9, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_GetAcqRetries_AVL62X1(AVL_puchar pucRetryNum, struct AVL62X1_Chip * pAVL_Chip);

	AVL_ErrorCode IRx_SetMpegMode_AVL62X1( struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_SetMpegBitOrder_AVL62X1(enum AVL62X1_MpegBitOrder e_BitOrder, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_SetMpegErrorPolarity_AVL62X1(enum AVL62X1_MpegPolarity e_ErrorPol, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_SetMpegValidPolarity_AVL62X1(enum AVL62X1_MpegPolarity e_ValidPol, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_EnableMpegContiClock_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_DisableMpegContiClock_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IRx_DriveMpegOutput_AVL62X1(enum AVL62X1_Switch enumOn_Off, struct AVL62X1_Chip * pAVL_Chip);
    AVL_ErrorCode IRx_ConfigPLS_AVL62X1(AVL_uint32 uiShiftValue, struct AVL62X1_Chip * pAVL_Chip);

	AVL_ErrorCode IDiseqc_Initialize_AVL62X1(struct AVL62X1_Diseqc_Para * pDiseqcPara, struct AVL62X1_Chip * pAVL_Chip);
	AVL_ErrorCode IDiseqc_IsSafeToSwitchMode_AVL62X1(struct AVL62X1_Chip * pAVL_Chip);

	AVL_ErrorCode IBase_DownloadPatch_AVL62X1(struct AVL62X1_Chip * pAVL_Chip );

#ifdef AVL_CPLUSPLUS
}
#endif

#endif
