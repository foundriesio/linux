/* linux/arch/arm/mach-msm/irq.c
 *
 * Copyright (C) 2008, 2009 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


//#include <linux/ioctl.h>
//#include "tccfb_ioctrl.h"
#include "tcc_video_private.h"

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT 							(0x8-1)
#define BIT_0 								3
#define GET_ADDR_YUV42X_spY(Base_addr) 		(((((unsigned int)Base_addr) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) 	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT)>> BIT_0)<<BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) 	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) 	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0)<<BIT_0)
#endif

typedef enum {
	WMIXER_POLLING,
	WMIXER_INTERRUPT,
	WMIXER_NOWAIT
} WMIXER_RESPONSE_TYPE;


typedef struct {
	unsigned int 			rsp_type; 		// wmix response type

	unsigned int 			src_y_addr;		// source y address
	unsigned int 			src_u_addr;		// source u address
	unsigned int 			src_v_addr;		// source v address
	unsigned int 			src_fmt; 		// source image format
	unsigned int			src_rgb_swap;
	unsigned int 			src_img_width; 	// source image width
	unsigned int 			src_img_height; 	// source image height
	unsigned int 			src_win_left;
	unsigned int			src_win_top;
	unsigned int			src_win_right;
	unsigned int			src_win_bottom;

	unsigned int			src_fmt_ext_info;	//0, 8 : 8bit normal  10 : 10bit data type(16bit), 11: 10bit data type(real 10bit)  0x10 : map converter, 0x20 : dtrc converter

#if defined(CONFIG_VIOC_MAP_DECOMP)
	hevc_MapConv_info_t mapConv_info;
#endif
#if defined(CONFIG_SUPPORT_TCC_G2V2_VP9)
	vp9_compressed_info_t dtrcConv_info;
#endif

	unsigned int 			dst_y_addr; 		// destination image address
	unsigned int 			dst_u_addr; 		// destination image address
	unsigned int 			dst_v_addr; 		// destination image address
	unsigned int 			dst_fmt;			// destination image format
	unsigned int			dst_rgb_swap;
	unsigned int 			dst_img_width; 	// destination image width
	unsigned int 			dst_img_height; // destination image height
	unsigned int 			dst_win_left;
	unsigned int			dst_win_top;
	unsigned int			dst_win_right;
	unsigned int			dst_win_bottom;

	unsigned int			dst_fmt_ext_info;	//0, 8 : 8bit normal  10 : 10bit data type,  0x10 : map converter
} WMIXER_INFO_TYPE;

typedef struct {
	unsigned int 			rsp_type; 		// wmix response type

	unsigned int 			src_y_addr;		// source y address
	unsigned int 			src_u_addr;		// source u address
	unsigned int 			src_v_addr;		// source v address
	unsigned int 			src_fmt; 		// source image format
	unsigned int 			src_img_width;
	unsigned int 			src_img_height;
	unsigned int 			src_win_left;
	unsigned int 			src_win_top;
	unsigned int 			src_win_right;
	unsigned int 			src_win_bottom;

	unsigned int			src_fmt_ext_info;	//0, 8 : 8bit normal  10 : 10bit data type(16bit), 11: 10bit data type(real 10bit)  0x10 : map converter, 0x20 : dtrc converter
#if defined(CONFIG_VIOC_MAP_DECOMP)
	hevc_MapConv_info_t mapConv_info;
#endif
#if defined(CONFIG_SUPPORT_TCC_G2V2_VP9)
	vp9_compressed_info_t dtrcConv_info;
#endif

	unsigned int 			dst_y_addr; 		// destination image address
	unsigned int 			dst_u_addr; 		// destination image address
	unsigned int 			dst_v_addr; 		// destination image address
	unsigned int 			dst_fmt;			// destination image format
	unsigned int 			dst_img_width;
	unsigned int 			dst_img_height;
	unsigned int 			dst_win_left;
	unsigned int 			dst_win_top;
	unsigned int 			dst_win_right;
	unsigned int 			dst_win_bottom;

	unsigned int			interlaced;
	// for only TCC898x 
	unsigned int			mc_num;
	unsigned int			dst_fmt_ext_info;	//0, 8 : 8bit normal  10 : 10bit data type,  0x10 : map converter
} WMIXER_ALPHASCALERING_INFO_TYPE;

typedef struct {
	unsigned char 	rsp_type;
	unsigned char 	region;

	unsigned char 	src0_fmt;
	unsigned char 	src0_layer;
	unsigned short 	src0_acon0;
	unsigned short 	src0_acon1;
	unsigned short 	src0_ccon0;
	unsigned short 	src0_ccon1;
	unsigned short 	src0_rop_mode;
	unsigned short 	src0_asel;
	unsigned short 	src0_alpha0;
	unsigned short 	src0_alpha1;
	unsigned int 		src0_Yaddr;
	unsigned int 		src0_Uaddr;
	unsigned int 		src0_Vaddr;
	unsigned short 	src0_width;
	unsigned short 	src0_height;
	unsigned short 	src0_winLeft;
	unsigned short	src0_winTop;
	unsigned short	src0_winRight;
	unsigned short	src0_winBottom;


	unsigned char 	src1_fmt;
	unsigned char 	src1_layer;
	unsigned short 	src1_acon0;
	unsigned short 	src1_acon1;
	unsigned short 	src1_ccon0;
	unsigned short 	src1_ccon1;
	unsigned short 	src1_rop_mode;
	unsigned short 	src1_asel;
	unsigned short 	src1_alpha0;
	unsigned short 	src1_alpha1;
	unsigned int 		src1_Yaddr;
	unsigned int 		src1_Uaddr;
	unsigned int 		src1_Vaddr;
	unsigned short 	src1_width;
	unsigned short 	src1_height;
	unsigned short 	src1_winLeft;
	unsigned short	src1_winTop;
	unsigned short	src1_winRight;
	unsigned short	src1_winBottom;

	unsigned char 	dst_fmt;
	unsigned int 		dst_Yaddr;
	unsigned int 		dst_Uaddr;
	unsigned int 		dst_Vaddr;
	unsigned short 	dst_width;
	unsigned short 	dst_height;
	unsigned short 	dst_winLeft;
	unsigned short	dst_winTop;
	unsigned short	dst_winRight;
	unsigned short	dst_winBottom;
} WMIXER_ALPHABLENDING_TYPE;


//VIOC Block ID infomation
typedef struct {
	unsigned char rdma[4];
	unsigned char wmixer;
	unsigned char wdma;
} WMIXER_VIOC_INFO;



#define WMIXER                  'w'
#define TCC_WMIXER_IOCTRL 					0x01
#define TCC_WMIXER_IOCTRL_KERNEL 			0x02	
#define TCC_WMIXER_ALPHA_SCALING 			0x04
#define TCC_WMIXER_ALPHA_SCALING_KERNEL		0x08
#define TCC_WMIXER_ALPHA_MIXING 			0x10
#define TCC_WMIXER_ALPHA_MIXING_KERNEL		0x12


#define TCC_WMIXER_VIOC_INFO               			_IOR(WMIXER, 0x20, WMIXER_VIOC_INFO)
#define TCC_WMIXER_VIOC_INFO_KERNEL               	_IOR(WMIXER, 0x40, WMIXER_VIOC_INFO)
