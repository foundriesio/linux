/*
 * Copyright (C) Telechips, Inc.
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
#ifndef	_TCC_COMPONENT_H_
#define	_TCC_COMPONENT_H_

#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
#include "tcc_component_adv7343.h"
#elif defined(CONFIG_FB_TCC_COMPONENT_THS8200)
#include "tcc_component_ths8200.h"
#define TCC_COMPONENT_24BPP_RGB888
//#define TCC_COMPONENT_16BPP_YCBCR
#endif

/*==============================================================================
 *				NTSC/PAL Encoder
 *==============================================================================
 */
#define	LCD_CH	1
#define	IMG_CH0	2
#define	IMG_CH1	4
#define	IMG_CH2	8

// flag
#define SET_IMAGE_INTL  ((0x1)<<12) // DMA Interlace Mode
#define SET_IMAGE_AEN   ((0x1)<<11) // Alpha-blending Function for Each Image
#define SET_IMAGE_CEN   ((0x1)<<10) // Chroma-keying Function for Each Image
#define SET_IMAGE_IEN   ((0x1)<<9)  // Image Displaying Function for Each Image
#define SET_IMAGE_SRC   ((0x1)<<8)  // Image Source Select
#define SET_IMAGE_AOPT  ((0x1)<<7)  // Alpha-blending Option selection
#define SET_IMAGE_ASEL  ((0x1)<<6)  // Alpha-blending Alpha type
#define SET_IMAGE_UPD   ((0x1)<<5)  // Bit padding
#define SET_IMAGE_PD    ((0x1)<<4)  // Data Update Enable
#define SET_IMAGE_Y2RMD ((0x1)<<3)  // Y2R mode set
#define SET_IMAGE_Y2R   ((0x1)<<2)  // Y2R Conversion Enable bit
#define SET_IMAGE_BR    ((0x1)<<1)  // Bit Reverse
#define SET_IMAGE_FMT   (0x1)       // Image Format

// type
#define SET_IMAGE_SIZE		0x00000001
#define SET_IMAGE_POSITION	0x00000002
#define SET_IMAGE_OFFSET	0x00000004
#define SET_IMAGE_SCALE		0x00000008
#define READ_IMAGE_POSITION	0x00000020
#define SET_IMAGE_ALL		0x0000003F

enum COMPONENT_MODE_TYPE {
	COMPONENT_MODE_NTSC_M = 0,
	COMPONENT_MODE_NTSC_M_J,
	COMPONENT_MODE_NTSC_N,
	COMPONENT_MODE_NTSC_N_J,
	COMPONENT_MODE_NTSC_443,
	COMPONENT_MODE_PAL_M,
	COMPONENT_MODE_PAL_N,
	COMPONENT_MODE_PAL_B,
	COMPONENT_MODE_PAL_G,
	COMPONENT_MODE_PAL_H,
	COMPONENT_MODE_PAL_I,
	COMPONENT_MODE_PSEUDO_NTSC,
	COMPONENT_MODE_PSEUDO_PAL,
	COMPONENT_MODE_720P,
	COMPONENT_MODE_1080I,
	COMPONENT_MODE_MAX
};

enum COMPONENT_LCDC_AOPT_TYPE {
	COMPONENT_LCDC_AOPT_0,	// 100% ~ 0.39% transparency
	COMPONENT_LCDC_AOPT_1,	// 100% ~ 0.39% transparency
	COMPONENT_LCDC_AOPT_2,	// 100% ~ 0% transparency
	COMPONENT_LCDC_AOPT_MAX
};

enum COMPONENT_LCDC_ASEL_TYPE {
	COMPONENT_LCDC_ALPHA_VALUE    = 0,
	COMPONENT_LCDC_ALPHA_PIXVALUE = 1    //alpha RGB
};

enum COMPONENT_LCDC_YUV2RGB_TYPE {
	COMPONENT_LCDC_YUV2RGB_TYPE0,
	COMPONENT_LCDC_YUV2RGB_TYPE1,
	COMPONENT_LCDC_YUV2RGB_TYPE2,
	COMPONENT_LCDC_YUV2RGB_TYPE3
};

enum COMPONENT_LCDC_IMG_FMT_TYPE {
	COMPONENT_LCDC_IMG_FMT_1BPP,
	COMPONENT_LCDC_IMG_FMT_2BPP,
	COMPONENT_LCDC_IMG_FMT_4BPP,
	COMPONENT_LCDC_IMG_FMT_8BPP,
	COMPONENT_LCDC_IMG_FMT_RGB332      = 8,
	COMPONENT_LCDC_IMG_FMT_RGB444      = 9,
	COMPONENT_LCDC_IMG_FMT_RGB565      = 10,
	COMPONENT_LCDC_IMG_FMT_RGB555      = 11,
	COMPONENT_LCDC_IMG_FMT_RGB888      = 12,
	COMPONENT_LCDC_IMG_FMT_RGB666      = 13,
	COMPONENT_LCDC_IMG_FMT_YUV420SP    = 24,
	COMPONENT_LCDC_IMG_FMT_YUV422SP    = 25,
	COMPONENT_LCDC_IMG_FMT_YUV422SQ    = 26,
	COMPONENT_LCDC_IMG_FMT_YUV420ITL0  = 28,
	COMPONENT_LCDC_IMG_FMT_YUV420ITL1  = 29,
	COMPONENT_LCDC_IMG_FMT_YUV422ITL0  = 30,
	COMPONENT_LCDC_IMG_FMT_YUV422ITL1  = 31,
	COMPONENT_LCDC_IMG_FMT_MAX
};

struct COMPONENT_LCDC_IMG_CTRL_TYPE {
	char INTL;
	char AEN;
	char CEN;
	char IEN;
	char SRC;
	enum COMPONENT_LCDC_AOPT_TYPE AOPT;
	enum COMPONENT_LCDC_ASEL_TYPE ASEL;
	char UPD;
	char PD;
	enum COMPONENT_LCDC_YUV2RGB_TYPE Y2RMD;
	char Y2R;
	char BR;
	enum COMPONENT_LCDC_IMG_FMT_TYPE FMT;
};

struct COMPONENT_SPEC_TYPE {
	unsigned int component_clk;
	unsigned int component_bus_width;
	unsigned int component_lcd_width;
	unsigned int component_lcd_height;
	unsigned int component_LPW;
	unsigned int component_LPC;
	unsigned int component_LSWC;
	unsigned int component_LEWC;
	unsigned int component_VDB;
	unsigned int component_VDF;

	unsigned int component_FPW1;
	unsigned int component_FLC1;
	unsigned int component_FSWC1;
	unsigned int component_FEWC1;

	unsigned int component_FPW2;
	unsigned int component_FLC2;
	unsigned int component_FSWC2;
	unsigned int component_FEWC2;
};

/*
 * cgms functions
 */
extern void component_chip_set_cgms(unsigned int enable, unsigned int data);
extern void component_chip_get_cgms(unsigned int *enable, unsigned int *data);

/*
 * extern functions
 */
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_TCC_DV_IN)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>
extern unsigned int dv_reg_phyaddr, dv_md_phyaddr;
extern int tca_edr_path_configure(void);
#endif

#include <video/tcc/tccfb.h>
#include <video/tcc/vioc_disp.h>
extern struct tcc_dp_device *tca_fb_get_displayType(
	TCC_OUTPUT_TYPE check_type);
extern void tca_scale_display_update(struct tcc_dp_device *pdp_data,
	struct tcc_lcdc_image_update *ImageInfo);
extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo,
	int specific_pclk);
extern void tca_vioc_displayblock_powerOff(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_disable(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_ctrl_set(unsigned int outDevice,
	struct tcc_dp_device *pDisplayInfo,
	stLTIMING *pstTiming, stLCDCTR *pstCtrl);
extern void tca_fb_attach_start(struct tccfb_info *info);
extern int tca_fb_attach_stop(struct tccfb_info *info);

#if defined(CONFIG_TCC_VTA)
extern int vta_cmd_notify_change_status(const char *);
#endif

extern char fb_power_state;


#endif //_TCC_COMPONENT_H_
