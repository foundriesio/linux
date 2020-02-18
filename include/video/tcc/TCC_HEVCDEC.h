/*
 *   FileName    : TCC_HEVCDEC.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/*!
 ***********************************************************************
 *
 * \file
 *		TCC_HEVCDEC.h
 * \date
 *		2020/02/06
 *		2018/11/13
 *		2018/04/02
 *		2018/03/22
 *		2016/08/21
 * \author
 *		AV algorithm group(AValgorithm@telechips.com) 
 * \brief
 *		main api
 * \version
 *		3.03.0.00(2020/02/06) : Added sleep (m_Usleep) callback and Forcely escape (TCC_HEVC_DEC_ESC, TCC_HEVC_DEC_EXT) funtions on API
 *		3.02.0.00(2018/11/13)
 *		3.01.0.00(2018/04/02)
 *		3.00.0.00(2018/03/22)
 *		2.3.0.0(2016/11/23)
 *		2.2.0.0(2016/08/23)
 *		2.1.0.0(2016/03/07)
 *		0.3.0.0(2015/03/13)
 *		0.2.0.0(2015/01/09)
 *		0.1.0.0(2014/11/28)
 *		0.0.0.0(2014/11/01) : first beta release
 *
 ***********************************************************************
 */

#ifndef _TCC_HEVC_DEC_H_
#define _TCC_HEVC_DEC_H_

#include "TCCxxxx_VPU_CODEC_COMMON.h"

//#define USE_VPU_DISPLAY_MODE  //! use ring buffer

#define HEVC_MAX_NUM_INSTANCE       4

#define RETCODE_HEVCERR_SEQ_HEADER_NOT_FOUND     31
#define RETCODE_HEVCERR_STRIDE_ZERO_OR_ALIGN8   100
#define RETCODE_HEVCERR_MIN_RESOLUTION          101
#define RETCODE_HEVCERR_MAX_RESOLUTION          102
#define RETCODE_HEVCERR_SEQ_INIT_HANGUP         103
#define RETCODE_HEVCERR_CHROMA_FORMAT           104
#define RETCODE_HEVCERR_PROFILE                 110

#ifndef RETCODE_WRAP_AROUND
#define RETCODE_WRAP_AROUND                     -10
#endif

#define WAVE4_STACK_SIZE            (8*1024)
#define WAVE4_MAX_CODE_BUF_SIZE     (1024*1024)
#define WAVE4_WORKBUF_SIZE          (3*1024*1024)
#define WAVE4_TEMPBUF_SIZE          (1024*1024)

#define WAVE4_SEC_AXI_BUF_SIZE      (128*1024)

#define WAVE4_SIZE_BIT_WORK         (WAVE4_MAX_CODE_BUF_SIZE + WAVE4_STACK_SIZE + WAVE4_TEMPBUF_SIZE + WAVE4_SEC_AXI_BUF_SIZE)
#define WAVE4_WORK_CODE_BUF_SIZE    (WAVE4_SIZE_BIT_WORK + (WAVE4_WORKBUF_SIZE*HEVC_MAX_NUM_INSTANCE))

#define WAVE4_STREAM_BUF_SIZE       0x600000
#define WAVE4_USERDATA_BUF_SIZE     (512*1024)

#define WAVE4_FBC_LUMA_TABLE_SIZE(_w, _h)       (((_h+15)/16)*((_w+255)/256)*128)
#define WAVE4_FBC_CHROMA_TABLE_SIZE(_w, _h)   (((_h+15)/16)*((_w/2+255)/256)*128)

#ifndef INC_DEVICE_TREE_PMAP

//------------------------------------------------------------------------------
// decode struct and definition
//------------------------------------------------------------------------------
//! represents rectangular window information in a frame
typedef struct wave4_pic_crop_t
{
    unsigned int m_iCropLeft;
    unsigned int m_iCropTop;
    unsigned int m_iCropRight;
    unsigned int m_iCropBottom;
} wave4_pic_crop_t;

//------------------------------------------------------------------------------
// user data struct and definition
//------------------------------------------------------------------------------
#define HEVC_MAX_DPB_SIZE                     17
#define HEVC_MAX_NUM_SUB_LAYER                 8
#define HEVC_MAX_NUM_ST_RPS                   64
#define HEVC_MAX_CPB_CNT                      32
#define HEVC_MAX_NUM_VERTICAL_FILTERS          5
#define HEVC_MAX_NUM_HORIZONTAL_FILTERS        3
#define HEVC_MAX_TAP_LENGTH                   32
#define HEVC_MAX_NUM_KNEE_POINT              999

#define HEVC_MAX_NUM_TONE_VALUE             1024
#define HEVC_MAX_NUM_FILM_GRAIN_COMPONENT      3
#define HEVC_MAX_NUM_INTENSITY_INTERVALS     256
#define HEVC_MAX_NUM_MODEL_VALUES              5

#define HEVC_MAX_LUT_NUM_VAL                   3
#define HEVC_MAX_LUT_NUM_VAL_MINUS1           33
#define HEVC_MAX_COLOUR_REMAP_COEFFS           3

typedef struct hevc_win_t
{
    short left;
    short right;
    short top;
    short bottom;
} hevc_win_t;

typedef struct hevc_hrd_param_t
{
    unsigned char   nal_hrd_param_present_flag;
    unsigned char   vcl_hrd_param_present_flag;
    unsigned char   sub_pic_hrd_params_present_flag;
    unsigned char   tick_divisor_minus2;
    char            du_cpb_removal_delay_inc_length_minus1;
    char            sub_pic_cpb_params_in_pic_timing_sei_flag;
    char            dpb_output_delay_du_length_minus1;
    char            bit_rate_scale;
    char            cpb_size_scale;
    char            initial_cpb_removal_delay_length_minus1;
    char            cpb_removal_delay_length_minus1;
    char            dpb_output_delay_length_minus1;

    unsigned char   fixed_pic_rate_gen_flag[HEVC_MAX_NUM_SUB_LAYER];
    unsigned char   fixed_pic_rate_within_cvs_flag[HEVC_MAX_NUM_SUB_LAYER];
    unsigned char   low_delay_hrd_flag[HEVC_MAX_NUM_SUB_LAYER];
    char            cpb_cnt_minus1[HEVC_MAX_NUM_SUB_LAYER];
    short           elemental_duration_in_tc_minus1[HEVC_MAX_NUM_SUB_LAYER];

    unsigned int    nal_bit_rate_value_minus1[HEVC_MAX_NUM_SUB_LAYER][HEVC_MAX_CPB_CNT];
    unsigned int    nal_cpb_size_value_minus1[HEVC_MAX_NUM_SUB_LAYER][HEVC_MAX_CPB_CNT];
    unsigned int    nal_cpb_size_du_value_minus1[HEVC_MAX_NUM_SUB_LAYER];
    unsigned int    nal_bit_rate_du_value_minus1[HEVC_MAX_NUM_SUB_LAYER];
    unsigned char   nal_cbr_flag[HEVC_MAX_NUM_SUB_LAYER][HEVC_MAX_CPB_CNT];

    unsigned int    vcl_bit_rate_value_minus1[HEVC_MAX_NUM_SUB_LAYER][HEVC_MAX_CPB_CNT];
    unsigned int    vcl_cpb_size_value_minus1[HEVC_MAX_NUM_SUB_LAYER][HEVC_MAX_CPB_CNT];
    unsigned int    vcl_cpb_size_du_value_minus1[HEVC_MAX_NUM_SUB_LAYER];
    unsigned int    vcl_bit_rate_du_value_minus1[HEVC_MAX_NUM_SUB_LAYER];
    unsigned char   vcl_cbr_flag[HEVC_MAX_NUM_SUB_LAYER][HEVC_MAX_CPB_CNT];
    unsigned int    m_Reserved[17];
} hevc_hrd_param_t;

typedef struct hevc_vui_param_t
{
    unsigned char   aspect_ratio_info_present_flag;
    unsigned char   aspect_ratio_idc;
    unsigned char   overscan_info_present_flag;
    unsigned char   overscan_appropriate_flag;

    unsigned char   video_signal_type_present_flag;
    char            video_format;

    unsigned char   video_full_range_flag;
    unsigned char   colour_description_present_flag;

    unsigned short  sar_width;
    unsigned short  sar_height;

    unsigned char   colour_primaries;
    unsigned char   transfer_characteristics; //16: HDR, 18:HLG
    unsigned char   matrix_coefficients;

    unsigned char   chroma_loc_info_present_flag;
    char            chroma_sample_loc_type_top_field;
    char            chroma_sample_loc_type_bottom_field;

    unsigned char   neutral_chroma_indication_flag;

    unsigned char   field_seq_flag;

    unsigned char   frame_field_info_present_flag;
    unsigned char   default_display_window_flag;
    unsigned char   vui_timing_info_present_flag;
    unsigned char   vui_poc_proportional_to_timing_flag;

    unsigned int    vui_num_units_in_tick;
    unsigned int    vui_time_scale;

    unsigned char   vui_hrd_parameters_present_flag;
    unsigned char   bitstream_restriction_flag;

    unsigned char   tiles_fixed_structure_flag;
    unsigned char   motion_vectors_over_pic_boundaries_flag;
    unsigned char   restricted_ref_pic_lists_flag;
    char            min_spatial_segmentation_idc;
    char            max_bytes_per_pic_denom;
    char            max_bits_per_mincu_denom;

    short           vui_num_ticks_poc_diff_one_minus1;
    char            log2_max_mv_length_horizontal;
    char            log2_max_mv_length_vertical;

    hevc_win_t      def_disp_win;
    hevc_hrd_param_t hrd_param;
    unsigned int    m_Reserved[19];
} hevc_vui_param_t;

typedef struct hevc_mastering_display_colour_volume_t
{
    unsigned int    display_primaries_x[3];
    unsigned int    display_primaries_y[3];
    unsigned int    white_point_x                   : 16;
    unsigned int    white_point_y                   : 16;
    unsigned int    max_display_mastering_luminance : 32;
    unsigned int    min_display_mastering_luminance : 32;
    unsigned int    m_Reserved[22];
} hevc_mastering_display_colour_volume_t;

typedef struct hevc_chroma_resampling_filter_hint_t
{
    unsigned int    ver_chroma_filter_idc               : 8;
    unsigned int    hor_chroma_filter_idc               : 8;
    unsigned int    ver_filtering_field_processing_flag : 1;
    unsigned int    target_format_idc                   : 2;
    unsigned int    num_vertical_filters                : 3;
    unsigned int    num_horizontal_filters              : 3;
    unsigned char   ver_tap_length_minus1[ HEVC_MAX_NUM_VERTICAL_FILTERS ];
    unsigned char   hor_tap_length_minus1[ HEVC_MAX_NUM_HORIZONTAL_FILTERS ];
    int             ver_filter_coeff[ HEVC_MAX_NUM_VERTICAL_FILTERS ][ HEVC_MAX_TAP_LENGTH ];
    int             hor_filter_coeff[ HEVC_MAX_NUM_HORIZONTAL_FILTERS ][ HEVC_MAX_TAP_LENGTH ];
    unsigned int    m_Reserved[24];
} hevc_chroma_resampling_filter_hint_t;

typedef struct hevc_knee_function_info_t
{
    unsigned int    knee_function_id;
    unsigned char   knee_function_cancel_flag;
    unsigned char   knee_function_persistence_flag;
    unsigned char   m_Reserved[2];
    unsigned int    input_disp_luminance;
    unsigned int    input_d_range;
    unsigned int    output_d_range;
    unsigned int    output_disp_luminance;
    unsigned short  num_knee_points_minus1;
    unsigned short  input_knee_point[ HEVC_MAX_NUM_KNEE_POINT ];
    unsigned short  output_knee_point[ HEVC_MAX_NUM_KNEE_POINT ];
} hevc_knee_function_info_t;

typedef struct hevc_content_light_level_info_t
{
    unsigned short max_content_light_level;
    unsigned short max_pic_average_light_level;
} hevc_content_light_level_info_t;

typedef struct hevc_colour_remapping_info_t
{
    unsigned int    colour_remap_id;
    unsigned char   colour_remap_cancel_flag;
    unsigned char   colour_remap_persistence_flag;
    unsigned char   colour_remap_video_signal_info_present_flag;
    unsigned char   colour_remap_full_range_flag;

    unsigned char   colour_remap_primaries;
    unsigned char   colour_remap_transfer_function;
    unsigned char   colour_remap_matrix_coefficients;
    unsigned char   colour_remap_input_bit_depth;

    unsigned char   colour_remap_bit_depth;
    unsigned char   pre_lut_num_val_minus1[HEVC_MAX_LUT_NUM_VAL];
    unsigned short  pre_lut_coded_value[HEVC_MAX_LUT_NUM_VAL][HEVC_MAX_LUT_NUM_VAL_MINUS1];
    unsigned short  pre_lut_target_value[HEVC_MAX_LUT_NUM_VAL][HEVC_MAX_LUT_NUM_VAL_MINUS1];

    unsigned char   colour_remap_matrix_present_flag;
    unsigned char   log2_matrix_denom;
    unsigned char   colour_remap_coeffs[HEVC_MAX_COLOUR_REMAP_COEFFS][HEVC_MAX_COLOUR_REMAP_COEFFS];

    unsigned char   post_lut_num_val_minus1[HEVC_MAX_LUT_NUM_VAL];
    unsigned short  post_lut_coded_value[HEVC_MAX_LUT_NUM_VAL][HEVC_MAX_LUT_NUM_VAL_MINUS1];
    unsigned short  post_lut_target_value[HEVC_MAX_LUT_NUM_VAL][HEVC_MAX_LUT_NUM_VAL_MINUS1];
} hevc_colour_remapping_info_t;

typedef struct hevc_film_grain_characteristics_t
{
    unsigned char film_grain_characteristics_cancel_flag;
    unsigned char film_grain_model_id;
    unsigned char separate_colour_description_present_flag;
    unsigned char film_grain_bit_depth_luma_minus8;
    unsigned char film_grain_bit_depth_chroma_minus8;
    unsigned char film_grain_full_range_flag;
    unsigned char film_grain_colour_primaries;
    unsigned char film_grain_transfer_characteristics;
    unsigned char film_grain_matrix_coeffs;

    unsigned char blending_mode_id;
    unsigned char log2_scale_factor;

    unsigned char   comp_model_present_flag[HEVC_MAX_NUM_FILM_GRAIN_COMPONENT];
    unsigned char   num_intensity_intervals_minus1[HEVC_MAX_NUM_FILM_GRAIN_COMPONENT];
    unsigned char   num_model_values_minus1[HEVC_MAX_NUM_FILM_GRAIN_COMPONENT];
    unsigned char   intensity_interval_lower_bound[HEVC_MAX_NUM_FILM_GRAIN_COMPONENT][HEVC_MAX_NUM_INTENSITY_INTERVALS];
    unsigned char   intensity_interval_upper_bound[HEVC_MAX_NUM_FILM_GRAIN_COMPONENT][HEVC_MAX_NUM_INTENSITY_INTERVALS];
    unsigned int    comp_model_value[HEVC_MAX_NUM_FILM_GRAIN_COMPONENT][HEVC_MAX_NUM_INTENSITY_INTERVALS][HEVC_MAX_NUM_MODEL_VALUES];

    unsigned char film_grain_characteristics_persistence_flag;
} hevc_film_grain_characteristics_t;

typedef struct hevc_tone_mapping_info_t
{
    unsigned int    tone_map_id;
    unsigned char   tone_map_cancel_flag;

    unsigned char   tone_map_persistence_flag;
    unsigned int    coded_data_bit_depth;
    unsigned int    target_bit_depth;
    unsigned char   tone_map_model_id;
    unsigned int    min_value;
    unsigned int    max_value;
    unsigned int    sigmoid_midpoint;
    unsigned int    sigmoid_width;
    unsigned short  start_of_coded_interval[ HEVC_MAX_NUM_TONE_VALUE ];
    unsigned short  num_pivots;
    unsigned short  coded_pivot_value[ HEVC_MAX_NUM_TONE_VALUE ];
    unsigned short  target_pivot_value[ HEVC_MAX_NUM_TONE_VALUE ];
    unsigned char   camera_iso_speed_idc;
    unsigned int    camera_iso_speed_value;
    unsigned char   exposure_index_idc;
    unsigned int    exposure_index_value;
    unsigned char   exposure_compensation_value_sign_flag;
    unsigned short  exposure_compensation_value_numerator;
    unsigned short  exposure_compensation_value_denom_idc;
    unsigned int    ref_screen_luminance_white;
    unsigned int    extended_range_white_level;
    unsigned short  nominal_black_level_code_value;
    unsigned short  nominal_white_level_code_value;
    unsigned short  extended_white_level_code_value;
} hevc_tone_mapping_info_t;

typedef struct hevc_sei_pic_timing_t
{
    char    status;
    char    pic_struct;
    char    source_scan_type;
    char    duplicate_flag;
} hevc_sei_pic_timing_t;

typedef struct hevc_alternative_transfer_characteristics_info_t
{
    unsigned int preferred_transfer_characteristics;
} hevc_alternative_transfer_characteristics_info_t;

typedef struct hevc_dec_UserData_info_t
{
    hevc_sei_pic_timing_t m_SeiPicTiming;
    hevc_vui_param_t m_VuiParam;

    hevc_mastering_display_colour_volume_t m_MasteringDisplayColorVolume;
    hevc_content_light_level_info_t m_ContentLightLevelInfo;

    hevc_chroma_resampling_filter_hint_t m_ChromaResamplingFilterHint;
    hevc_knee_function_info_t m_KneeFunctionInfo;
    hevc_tone_mapping_info_t m_ToneMappingInfo;

    hevc_colour_remapping_info_t    m_ColourRemappingInfo;
    hevc_film_grain_characteristics_t m_FilmGrainCharInfo;

    hevc_alternative_transfer_characteristics_info_t m_AlternativeTransferCharacteristicsInfo;

    unsigned int m_Reserved[22];
} hevc_dec_UserData_info_t;

//-----------------------------------------------------
// data structure to get information necessary to
// start decoding from the decoder (this is an output parameter)
//-----------------------------------------------------
typedef struct hevc_dec_initial_info_t
{
    int m_iPicWidth;                //!< {(PicX+15)/16} * 16  (this width  will be used while allocating decoder frame buffers. picWidth  is a multiple of 16)
    int m_iPicHeight;               //!< {(PicY+15)/16} * 16  (this height will be used while allocating decoder frame buffers. picHeight is a multiple of 16)
    unsigned int m_uiFrameRateRes;  //!< decoded picture frame rate residual(number of time units of a clock operating at the frequency[m_iFrameRateDiv] Hz, frameRateInfo = m_uiFrameRateRes/m_uiFrameRateDiv
    unsigned int m_uiFrameRateDiv;  //!< decoded picture frame rate unit number in Hz
    int m_iMinFrameBufferCount;     //!< the minimum number of frame buffers that are required for decoding. application must allocate at least this number of frame buffers.
    int m_iMinFrameBufferSize;      //!< minimum frame buffer size
    int m_iFrameBufferFormat;       //!< frame buffer format

    wave4_pic_crop_t m_PicCrop;     //!< represents rectangular window information in a frame

    unsigned int m_uiUserData;
    hevc_dec_UserData_info_t m_UserDataInfo;

    int m_iFrameBufDelay;           //!< maximum display frame buffer delay to process reordering

    //! Additional Info
    int m_iProfile;                 //!< profile of the decoded stream
    int m_iLevel;                   //!< level of the decoded stream
    int m_iTier;                    //!< 0: Main Tier, 1: High Tier for HEVC
    int m_iAspectRateInfo;          //!< aspect rate information. if this value is 0, then aspect ratio information is not present
    int m_iReportErrorReason;       //!< reports the reason of 'RETCODE_CODEC_SPECOUT' or 'RETCODE_INVALID_STRIDE' error
    int m_iSourceFormat;            //!< source bit format
    unsigned int m_Reserved[15];    //!< Reserved.
} hevc_dec_initial_info_t;

//! data structure for initializing Video unit
typedef struct hevc_dec_init_t
{
    codec_addr_t m_BitWorkAddr[2];      //!< physical[0] and virtual[1] address of a working space of the decoder. This working buffer space consists of work buffer, code buffer, and parameter buffer.
    codec_addr_t m_CodeAddr[2];         //!< physical[0] and virtual[1] address of a code buffer of the decoder.    // TEST_FW_WRITING
    codec_addr_t m_RegBaseVirtualAddr;  //!< virtual address BIT_BASE

    //! Bitstream Info
    int m_iBitstreamFormat;             //!< bitstream format
    codec_addr_t m_BitstreamBufAddr[2]; //!< bitstream buf address : multiple of 4
    int m_iBitstreamBufSize;            //!< bitstream buf size    : multiple of 1024

    //! Decoding Options
#define WAVE4_WTL_ENABLE            (1<<0) //!< WTL Enable
#define SEC_AXI_BUS_DISABLE_SDRAM   (1<<1) //!< Disable SDRAM for sec. AXI bus
#define WAVE4_10BITS_DISABLE        (1<<3) //!< 10 to 8 bits Output Enable

    unsigned int m_uiDecOptFlags;


    //! HEVC Control
    unsigned int m_bEnableUserData;     //!< If this is set, HEVC returns userdata.
    codec_addr_t m_UserDataAddr[2];
    int m_iUserDataBufferSize;
    unsigned int m_bCbCrInterleaveMode; //!< 0 (chroma separate mode   : CbCr data is written in separate frame memories)
                                        //!< 1 (chroma interleave mode : CbCr data is interleaved in chroma memory)
    int m_iFilePlayEnable;              //!< enable file play mode. If this value is set to 0, streaming mode with ring buffer will be used

    //! Callback Func
#if 1//def __LP64__
    void* (*m_Memcpy ) ( void*, const void*, unsigned int, unsigned int );  //!< memcpy
    void  (*m_Memset ) ( void*, int, unsigned int, unsigned int );          //!< memset
    int   (*m_Interrupt ) ( void );                             //!< hw interrupt (return value is always 0)
    void* (*m_Ioremap ) ( unsigned int, unsigned int );
    void  (*m_Iounmap ) ( void* );
    unsigned int (*m_reg_read)(void *, unsigned int);
    void (*m_reg_write)(void *, unsigned int, unsigned int);
	void (*m_Usleep)(unsigned int, unsigned int);
#else
    unsigned long long cb_dummy_memcpy;
    unsigned long long cb_dummy_memset;
    unsigned long long cb_dummy_interrupt;
    unsigned long long cb_dummy_ioremap;
    unsigned long long cb_dummy_iounmap;
    unsigned long long cb_dummy_reg_read;
    unsigned long long cb_dummy_reg_write;
#endif

	unsigned int m_Reserved[31];		//! Reserved.	// TEST_FW_WRITING
} hevc_dec_init_t;

typedef struct hevc_dec_input_t
{
    codec_addr_t m_BitstreamDataAddr[2];//!< bitstream data address
    int m_iBitstreamDataSize;           //!< bitstream data size
    codec_addr_t m_UserDataAddr[2];//!< Picture Layer User-data address
    int m_iUserDataBufferSize;  //!< Picture Layer User-data Size
    int m_iSkipFrameMode;               //!< Skip Frame Mode
                                        //!< 0 ( Skip disable )
                                        //!< 1 ( Skip except I(IDR) picture )
                                        //!< 2 ( Skip B picture : skip if nal_ref_idc==0 in H.264 )
    unsigned int m_Reserved[25];        //! Reserved.
} hevc_dec_input_t;

typedef struct hevc_dec_buffer_t
{
    codec_addr_t m_FrameBufferStartAddr[2]; //!< physical[0] and virtual[1] address of a frame buffer of the decoder.
    int m_iFrameBufferCount;                //!< allocated frame buffer count
} hevc_dec_buffer_t;


typedef struct hevc_dec_buffer2_t
{
    codec_addr_t    m_addrFrameBuffer[2][32];       //!< physical[0] and virtual[1] address of a frame buffer of the decoder.
    unsigned int    m_ulFrameBufferCount;           //!< allocated frame buffer count
} hevc_dec_buffer2_t;


typedef struct hevc_dec_ring_buffer_setting_in_t
{
    unsigned int m_OnePacketBufferAddr;
    unsigned int m_iOnePacketBufferSize;
} hevc_dec_ring_buffer_setting_in_t;


typedef struct hevc_dec_ring_buffer_status_out_t
{
    unsigned int m_ulAvailableSpaceInRingBuffer;
    unsigned int m_ptrReadAddr_PA;
    unsigned int m_ptrWriteAddr_PA;
} hevc_dec_ring_buffer_status_out_t;


typedef struct hevc_dec_MapConv_info_t
{
    codec_addr_t m_CompressedY[2];
    codec_addr_t m_CompressedCb[2];

    codec_addr_t m_FbcYOffsetAddr[2];
    codec_addr_t m_FbcCOffsetAddr[2];

	unsigned int m_uiCompressionTableLumaSize;
	unsigned int m_uiCompressionTableChromaSize;

	unsigned int m_uiLumaStride;
	unsigned int m_uiChromaStride;

    unsigned int m_uiLumaBitDepth;
    unsigned int m_uiChromaBitDepth;

    unsigned int m_uiFrameEndian;

	unsigned int m_Reserved[17];	//! Reserved. 
} hevc_dec_MapConv_info_t;

typedef struct hevc_dec_output_info_t
{
    int m_iPicType;                 //!< ( 0- I picture,  1- P picture,  2- B picture )

    int m_iDispOutIdx;              //!< index of output frame buffer
    int m_iDecodedIdx;              //!< index of decoded frame buffer

    int m_iOutputStatus;
    int m_iDecodingStatus;

    int m_iNumOfErrMBs;             //!< number of error macroblocks

    int m_iDecodedHeight;                   //!< Height of input bitstream. In some cases, this value can be different from the height of previous frames.
    int m_iDecodedWidth;                    //!< Width of input bitstream. In some cases, this value can be different from the height of previous frames.

    int m_iDisplayHeight;                   //!< Height of input bitstream. In some cases, this value can be different from the height of previous frames.
    int m_iDisplayWidth;                    //!< Width of input bitstream. In some cases, this value can be different from the height of previous frames.

    wave4_pic_crop_t m_DecodedCropInfo; //!< Cropping information of decoded frame.
    wave4_pic_crop_t m_DisplayCropInfo; //!< Cropping information of output frame.

    //! User Data Buffer Address
    unsigned int m_uiUserData;
    hevc_dec_UserData_info_t m_UserDataInfo;
    codec_addr_t m_UserDataAddress[2];  //!< If contents have picture-layer user-data, return it.

    int m_iConsumedBytes;           //!< consumed bytes (only for file play mode, m_iFilePlayEnable=1)

    int m_iInvalidDispCount;        //!< counter of consecutive display index error
    int m_iAspectRateInfo;          //!< aspect rate information. if this value is 0, then aspect ratio information is not present.
    int m_Reserved2;

    hevc_dec_MapConv_info_t m_DispMapConvInfo;  // display compressed frame info. for map converter
    hevc_dec_MapConv_info_t m_CurrMapConvInfo;  // current compressed frame info. for map converter
    hevc_dec_MapConv_info_t m_PrevMapConvInfo;  // previous compressed frame info. for map converter

    unsigned int m_Reserved[41];    //! Reserved.

} hevc_dec_output_info_t;

typedef struct hevc_dec_output_t
{
    hevc_dec_output_info_t m_DecOutInfo;
    codec_addr_t m_pDispOut[2][3]; //! physical[0] and virtual[1] display  address of Y, Cb, Cr component
    codec_addr_t m_pCurrOut[2][3]; //! physical[0] and virtual[1] current  address of Y, Cb, Cr component
    codec_addr_t m_pPrevOut[2][3]; //! physical[0] and virtual[1] previous address of Y, Cb, Cr component
} hevc_dec_output_t;

/*!
 ***********************************************************************
 * \brief
 *      TCC_HEVC_DEC    : main api function of libhevcdec
 * \param
 *      [in]Op          : decoder operation
 * \param
 *      [in,out]pHandle : libhevcdec's handle
 * \param
 *      [in]pParam1     : init or input parameter
 * \param
 *      [in]pParam2     : output or info parameter
 * \return
 *      If successful, TCC_HEVC_DEC returns 0 or plus. Otherwise, it returns a minus value.
 ***********************************************************************
 */
codec_result_t
TCC_HEVC_DEC( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );

codec_result_t
TCC_HEVC_DEC_ESC( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );

codec_result_t
TCC_HEVC_DEC_EXT( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );

#endif
#endif//_TCC_HEVC_DEC_H_
