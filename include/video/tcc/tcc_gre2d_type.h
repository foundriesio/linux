/*
 * Copyright (C) Telechips Inc.
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
 */
#ifndef	_GRE2D_TYPE_H_
#define	_GRE2D_TYPE_H_

// CHANNEL SETTING
enum G2D_CHANNEL {
	FCH0_CH,
	FCH1_CH,
	FCH2_CH,
	FCH3_CH,
	DEST_CH
};

enum G2D_OP_MODE {
	NOOP,
	FLIP_HOR = 0x2,
	FLIP_VER = 0x3,
	FLIP_HV = 0x4,
	ROTATE_90  = 0x5,
	ROTATE_180 = 0x6,
	ROTATE_270 = 0x7
};

enum G2D_DATA_FM {
	GE_YUV444_sp = 0x0,
	GE_YUV440_sp,
	GE_YUV422_sp,
	GE_YUV420_sp,
	GE_YUV411_sp,
	GE_YUV410_sp,
	GE_YUV422_in = 0x6,
	GE_YUV420_in,
	GE_YUV444_sq = 0x8,
	GE_YUV422_sq,
	GE_RGB332 = 0xA,
	GE_RGB444 = 0xC,
	GE_ARGB4444,
	GE_RGB454,
	GE_ARGB3454,
	GE_RGB555,
	GE_ARGB1555,
	GE_RGB565,
	GE_RGB666 = 0x14,
	GE_ARGB4666,
	GE_ARGB6666,
	GE_RGB888,
	GE_ARGB4888,
	GE_ARGB8888,
	GE_FMT_MAX
};

enum G2D_DATA_ORDER {
	G2D_ARGB,
	G2D_ARBG,
	G2D_AGRB,
	G2D_AGBR,
	G2D_ABRG,
	G2D_ABGR,
	G2D_RABG,
	G2D_BGRA,
};

struct G2D_FMT_CTRL {
	unsigned int format; //G2D_DATA_FM
	unsigned int data_swap;//G2D_DATA_ORDER
	unsigned int uv_order;//UV_order 0: uv  1:vu it is use
};

enum G2D_ARITH_TYPE {
	AR_NOOP,
	AR_FILLING = 0x2,
	AR_INVERT,
	AR_ADD,
	AR_SUBTRACT_A,
	AR_SUBTRACT_B,
	AR_MULTIPLY
};

enum G2D_Y2R_TYPE {
	Y2R_TYP0,
	Y2R_TYP1,
	Y2R_TYP2,
	Y2R_TYP3
};

struct G2D_SRC_Y2R_CTRL {
	unsigned char src_y2r;
	enum G2D_Y2R_TYPE src_y2r_type;
};

struct G2D_SRC_CTRL {
	enum G2D_ARITH_TYPE src0_arith;
	struct G2D_SRC_Y2R_CTRL src0_y2r;
	unsigned char src0_chroma_en;
	enum G2D_CHANNEL src_sel_0;

	enum G2D_ARITH_TYPE src1_arith;
	struct G2D_SRC_Y2R_CTRL src1_y2r;
	unsigned char src1_chroma_en;
	enum G2D_CHANNEL src_sel_1;

	enum G2D_ARITH_TYPE src2_arith;
	struct G2D_SRC_Y2R_CTRL src2_y2r;
	unsigned char src2_chroma_en;
	enum G2D_CHANNEL src_sel_2;
	enum G2D_CHANNEL src_sel_3;
};

/* OPERATOR DEFINE TYPE */
enum G2D_OP_TYPE {
	OP_0,
	OP_1,
	OP_2
};

enum G2D_OP_ACON {
	ACON_0,
	ACON_1,
	ACON_2,
	ACON_3 = 0x4,
	ACOn_4
};

enum G2D_OP_CCON {
	CCON_0,
	CCON_1,
	CCON_2,
	CCON_3,
	CCON_4,
	CCON_5 = 0x8,
	CCON_6
};

enum G2D_OP_ATUNE {
	Alpha_00,			// 0~255
	Alpha_10 = 0x2,			// 1~256
	Alpha_11				// 0~127 and 129~256
};

//operator 0 chroma-key source select
enum G2D_OP0_CHROMA {
	CHROMA_OP0_NOOP,    //disable
	CHROMA_S0,      //src0 & S0_chroam
	CHROMA_S1,      //src1 & S1_chroma
	CHROMA_RESEVE   //reseved
};

//operator 1 chroma-key source select
enum G2D_OP1_CHROMA {
	CHROMA_OP1_NOOP,    //disable
	CHROMA_OP0,     //op0 & S0_chroam
	CHROMA_OP1,     //op0 & S1_chroma
	CHROMA_S2       //src2 & S2_chroma
};

// operator ROP
enum GE_ROP_TYPE {
	GE_ROP_BLACKNESS,
	GE_ROP_MERGE_COPY,
	GE_ROP_MERGE_PAINT,
	GE_ROP_PAT_COPY,
	GE_ROP_PAT_INVERT,
	GE_ROP_PAT_PAINT,
	GE_ROP_SRC_COPY,
	GE_ROP_SRC_INVERT,
	GE_ROP_SRC_PAINT,
	GE_ROP_SRC_AND,
	GE_ROP_SRC_ERASE,
	GE_ROP_NOT_SRC_COPY,
	GE_ROP_NOT_SRC_ERASE,
	GE_ROT_DST_COPY,
	GE_ROP_DST_INVERT,
	GE_ROP_WHITENESS,
	GE_ROP_ALPHA_0 = 0x10,
	GE_ROP_ALPHA_1 = 0x18
};

enum G2D_EN {
	GRP_DISABLE,
	GRP_F0,
	GRP_F1,
	GRP_F2,
	GRP_F0F1,
	GRP_F0F2,
	GRP_F1F2,
	GRP_F0F1F2
};

enum G2D_R2Y_TYPE {
	R2Y_TYP0,
	R2Y_TYP1,
	R2Y_TYP2,
	R2Y_TYP3
};

enum G2D_INT_TYPE {
	G2D_INT_R_IRQ = 0x1,
	G2D_INT_R_FLG =  0x2
};

enum IMGFMT_CONV_TYPE {
	NONE,
	R2Y_TYPE,
	Y2R_TYPE
};

enum G2D_MABC_TYPE {
	MABC_4KBYTE,
	MABC_1KBYTE,
	MABC_MAX
};

enum G2D_ZF_TYPE {
	ZF_MSB_FILL,
	ZF_ZERO_FILL,
	ZF_HOB_FILL,
	ZF_MAX
};

enum G2D_ASEL_TYPE {
	ASEL_0,	// Alpha value is 0 ~ 255 (0% ~ 99.6%)
	ASEL_1,	// Alpha value is 1 ~ 256 (0.39% ~ 100%)
	ASEL_2	// Alpha value is 0 ~ 127, 129 ~ 256 (0% ~ 49.6%, 50.39% ~ 100%)
};

enum G2D_DITHERING_TYPE {
	BIT_TOGGLE_OP,
	ADD_1_OP
};

struct G2D_BCH_CTRL_TYPE {
	enum G2D_MABC_TYPE MABC;
	unsigned char ysel;
	unsigned char xsel;
	unsigned char converter_en;
	enum G2D_R2Y_TYPE converter_mode;
	unsigned char	DSUV;
	enum G2D_OP_MODE opmode;
	enum G2D_DITHERING_TYPE dithering_type;
	unsigned char	dithering_en;
	struct G2D_FMT_CTRL data_form;
};

/*-------------------   graphic engine application --------------------*/
struct G2D_FCH_TYPE {
	unsigned int add0; // Front-End ch. src Address 0
	unsigned int add1; // Front-End ch. src Address 1
	unsigned int add2; // Front-End ch. src Address 2
	unsigned int  frame_pix_sx; // Front-End ch. src Frame Pixel Size sx
	unsigned int  frame_pix_sy; // Front-End ch. src Frame Pixel Size sy
	unsigned int  src_off_sx;  // Front-End ch. src Pixel Offset sx
	unsigned int  src_off_sy;  // Front-End ch. src Pixel Offset sy
	unsigned int  img_pix_sx;  // Front-End ch. src Image Pixel Size sx
	unsigned int  img_pix_sy;  // Front-End ch. src Image Pixel Size sy
	unsigned int  win_off_sx;  // Front-End ch. Window Pixel Offset sx
	unsigned int  win_off_sy;  // Front-End ch. Window Pixel Offset sy
	enum G2D_OP_MODE op_mode; // Front-End ch. Control operation mode
	struct G2D_FMT_CTRL src_form; // Front-End ch. Control src data format

	unsigned char chroma_RY; // src Chroma Key Parameter R Y
	unsigned char chroma_GU; // src Chroma Key Parameter G U
	unsigned char chroma_BV; // src Chroma Key Parameter B V

	unsigned char src_chroma_en; // src Chroma-key Enable for Arithmetic
	enum G2D_ARITH_TYPE arith_mode; // src Arithmetic Mode

	unsigned char src_y2r; // src YUV to RGB Converter Enable
	enum G2D_Y2R_TYPE src_y2r_type; // src YUV to RGB Converter Type

	unsigned char arith_RY; // src Chroma Key Parameter R Y
	unsigned char arith_GU; // src Chroma Key Parameter G U
	unsigned char arith_BV; // src Chroma Key Parameter B V

	unsigned int clut_en;
};

struct G2D_OPERATOR_PATT {
	unsigned short op_alpha; //operator alpha blending (0 ~ 15)
	unsigned char op_pat_RY; //operator pattern value
	unsigned char op_pat_GU; //operator pattern value
	unsigned char op_pat_BV; //operator pattern value
};

struct G2D_OPERATOR_CONT {
	// operator 1 chroma-key source select
	// 0:disable 1: op0 & S0_chroam 2:op0 & S1_chroma 3:src2 & S2_chroma
	enum G2D_OP1_CHROMA csel0;
	enum G2D_OP1_CHROMA csel1;

	// operator 0 operation mode
	enum GE_ROP_TYPE op_mode0;
	enum GE_ROP_TYPE op_mode1;
};

struct G2D_BCH_TYPE {
	unsigned int add0; // Back-End dst Address 0
	unsigned int add1; // Back-End dst Address 1
	unsigned int add2; // Back-End dst Address 2
	unsigned int frame_pix_sx; // Back-End ch. dst Frame Pixel Size x
	unsigned int frame_pix_sy; // Back-End ch. dst Frame Pixel Size y
	unsigned int dest_off_sx; // Back-End ch. dst Pixel Offset x
	unsigned int dest_off_sy; // Back-End ch. dst Pixel Offset y

	// Back-End Channel Control
	unsigned char ysel; // YUV4:4:4 to YUVx:x:x Y Control
	unsigned char xsel; // YUV4:4:4 to YUVx:x:x X Control
	unsigned char converter_en; // dst Format Converter Control
	enum G2D_R2Y_TYPE converter_mode; // RGB to YUV Converter Type
	enum G2D_OP_MODE op_mode; // Operation Mode
	struct G2D_FMT_CTRL dest_form; // dst Data Format
};

struct G2D_FUNC_TYPE {
	// front end channel 정의
	struct G2D_FCH_TYPE src0;
	struct G2D_FCH_TYPE src1;
	struct G2D_FCH_TYPE src2;

	// operator 정의
	struct G2D_OPERATOR_PATT op_pat_0;
	struct G2D_OPERATOR_PATT op_pat_1;
	struct G2D_OPERATOR_CONT op_ctrl;

	// back end channel 정의
	struct G2D_BCH_TYPE dest;
};

// gre2d_2ch_dma_main_func 사용시 사용
struct G2d_2CH_FUNC {
	// front end channel 정의
	struct G2D_FCH_TYPE src0;
	struct G2D_FCH_TYPE src1;

	// operator 정의
	struct G2D_OPERATOR_PATT op_pat_0;

	// operator 0 chroma-key source select
	// 0:disable 1: src0 & S0_chroam 2:src1 & S1_chroma 3:reseved
	enum G2D_OP0_CHROMA csel0;

	// operator 0 operation mode
	enum GE_ROP_TYPE op_mode0;

	// back end channel 정의
	struct G2D_BCH_TYPE dest;
};

// gre2d_2ch_dma_main_func 사용시 사용
struct G2d_1CH_FUNC {
	// front end channel 정의
	struct G2D_FCH_TYPE src0;

	// back end channel 정의
	struct G2D_BCH_TYPE dest;
};

/* OVERLAY 함수를 위한 관련 구조체 */
enum G2d_OVERLAY_ALPHA {
	G2d_ALPHA_NONE,
	G2d_ALPHA_VALUE, //alpha value
	G2d_ALAPH_RGB    // source 1 alpha value
};

struct G2D_OVERLAY_FCH {
	unsigned int add0;
	unsigned int add1;
	unsigned int add2;
	unsigned int  frame_pix_sx;
	unsigned int  frame_pix_sy;
	unsigned int  src_off_sx;
	unsigned int  src_off_sy;
	unsigned int  img_pix_sx;
	unsigned int  img_pix_sy;
	unsigned int  win_off_sx;
	unsigned int  win_off_sy;
	enum G2D_OP_MODE op_mode;
	struct G2D_FMT_CTRL src_form;
};

struct G2D_OVERLAY_BCH {
	unsigned int  add0;
	unsigned int  add1;
	unsigned int  add2;
	unsigned int  frame_pix_sx;
	unsigned int  frame_pix_sy;
	unsigned int  dest_off_sx;
	unsigned int  dest_off_sy;
	enum G2D_OP_MODE op_mode;
	struct G2D_FMT_CTRL dest_form;
};

struct G2D_OVERY_FUNC {
	// G2D_ResponseType
	unsigned int responsetype;

	// front end channel 정의
	struct G2D_OVERLAY_FCH src0;
	struct G2D_OVERLAY_FCH src1;

	// operator 정의
	enum G2D_OP0_CHROMA op_ctrl_scel0; // Chroma-key Source Select
	unsigned char chroma_RY;  //chroma - key value
	unsigned char chroma_GU;  //chroma - key value
	unsigned char chroma_BV;  //chroma - key value

	enum G2d_OVERLAY_ALPHA alpha_en; // alpha-blending enable
	unsigned char alpha_value; // alpha-blending value

// back end channel 정의
	struct G2D_OVERLAY_BCH dest;
};

struct G2D_ROP_FCH {
	unsigned int add0;
	unsigned int add1;
	unsigned int add2;
	unsigned int frame_pix_sx;
	unsigned int frame_pix_sy;
	unsigned int src_off_sx;
	unsigned int src_off_sy;
	unsigned int img_pix_sx;
	unsigned int img_pix_sy;
	unsigned int win_off_sx;
	unsigned int win_off_sy;
	enum G2D_OP_MODE op_mode;
	struct G2D_FMT_CTRL src_form;

	enum G2D_ARITH_TYPE arith_mode;
	unsigned char arith_RY; // Source Chroma Key Parameter R Y
	unsigned char arith_GU; // Source Chroma Key Parameter G U
	unsigned char arith_BV; // Source Chroma Key Parameter B V

	unsigned char chroma_R; // Source Chroma Key Parameter R  only RGB type
	unsigned char chroma_G; // Source Chroma Key Parameter G  only RGB type
	unsigned char chroma_B; // Source Chroma Key Parameter B  only RGB type

unsigned int clut_en;
};

struct G2D_ROP_FUNC {
// front end channel 정의
	struct G2D_ROP_FCH src0;
	struct G2D_ROP_FCH src1;
	struct G2D_ROP_FCH src2;

// operator 정의
	struct G2D_OPERATOR_PATT op_pat_0;
	struct G2D_OPERATOR_PATT op_pat_1;
	struct G2D_OPERATOR_CONT op_ctrl;

// back end channel 정의
	struct G2D_OVERLAY_BCH dest;
};

#endif //_GRE2D_TYPE_H_
