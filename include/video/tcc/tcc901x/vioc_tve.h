/*
 * linux/include/video/tcc/vioc_cvbs.h
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block 
 *
 * Copyright (C) 2008-2009 Telechips
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

#ifndef __VIOC_TVE_H__
#define __VIOC_TVE_H__

/*==============================================================================
				NTSC/PAL Encoder
  ==============================================================================*/
#define	LCD_CH	1
#define	IMG_CH0	2
#define	IMG_CH1	4
#define	IMG_CH2	8

// flag
#define SET_IMAGE_INTL	((0x1)<<12)	// DMA Interlace Mode
#define SET_IMAGE_AEN	((0x1)<<11)	// Alpha-blending Function for Each Image
#define SET_IMAGE_CEN	((0x1)<<10)	// Chroma-keying Function for Each Image
#define SET_IMAGE_IEN	((0x1)<<9)	// Image Displaying Function for Each Image
#define SET_IMAGE_SRC	((0x1)<<8)	// Image Source Select
#define SET_IMAGE_AOPT	((0x1)<<7)	// Alpha-blending Option selection
#define SET_IMAGE_ASEL	((0x1)<<6)	// Alpha-blending Alpha type
#define SET_IMAGE_UPD	((0x1)<<5)	// Data Update Enable
#define SET_IMAGE_PD	((0x1)<<4)	// Bit padding
#define SET_IMAGE_Y2RMD	((0x1)<<3)	// Y2R mode set
#define SET_IMAGE_Y2R	((0x1)<<2)	// Y2R Conversion Enable bit
#define SET_IMAGE_BR	((0x1)<<1)	// Bit Reverse
#define SET_IMAGE_FMT	(0x1)		// Image Format 

// type
#define SET_IMAGE_SIZE			0x00000001
#define SET_IMAGE_POSITION		0x00000002
#define SET_IMAGE_OFFSET		0x00000004
#define SET_IMAGE_SCALE			0x00000008
#define READ_IMAGE_POSITION		0x00000020
#define SET_IMAGE_ALL			0x0000003F

//#define TCC_COMPOSITE_CCIR656

/*
 * Register offset
 */
#define TVE_STATA		(0x00)
#define TVE_ECMDA		(0x04)
#define TVE_ECMDB		(0x08)
#define TVE_GLK		(0x0C)
#define TVE_SCH		(0x10)
#define TVE_HUE		(0x14)
#define TVE_SAT		(0x18)
#define TVE_CONT		(0x1C)
#define TVE_BRIGHT		(0x20)
#define TVE_FSC_ADJM		(0x24)
#define TVE_FSC_ADJL		(0x28)
#define TVE_ECMDC		(0x2C)
#define TVE_DACSEL		(0x40)
#define TVE_DACPD		(0x50)
#define TVE_ICNTL		(0x80)
#define TVE_HVOFFSET		(0x84)
#define TVE_HOFFSET		(0x88)
#define TVE_VOFFSET		(0x8C)
#define TVE_HSVSO		(0x90)
#define TVE_HSOE		(0x94)
#define TVE_HSOB		(0x98)
#define TVE_VSOB		(0x9C)
#define TVE_VSOE		(0xA0)

/*
 * STATA
 */
#define TVE_STATA_REVID_SHIFT		(5)
#define TVE_STATA_FIELD_SHIFT		(0)

#define TVE_STATA_REVID_MASK		(0x7 << TVE_STATA_REVID_SHIFT)
#define TVE_STATA_FIELD_MASK		(0x7 << TVE_STATA_FIELD_SHIFT)

/*
 * ECMDA
 */
#define TVE_ECMDA_PWDNENC_SHIFT		(7)
#define TVE_ECMDA_FDRST_SHIFT		(6)
#define TVE_ECMDA_FSCCELL_SHIFT		(4)
#define TVE_ECMDA_PED_SHIFT		(3)
#define TVE_ECMDA_PIXEL_SHIFT		(2)
#define TVE_ECMDA_IFMT_SHIFT		(1)
#define TVE_ECMDA_PHALT_SHIFT		(0)

#define TVE_ECMDA_PWDNENC_MASK		(0x1 << TVE_ECMDA_PWDNENC_SHIFT)
#define TVE_ECMDA_FDRST_MASK		(0x1 << TVE_ECMDA_FDRST_SHIFT)
#define TVE_ECMDA_FSCCELL_MASK		(0x3 << TVE_ECMDA_FSCCELL_SHIFT)
#define TVE_ECMDA_PED_MASK		(0x1 << TVE_ECMDA_PED_SHIFT)
#define TVE_ECMDA_PIXEL_MASK		(0x1 << TVE_ECMDA_PIXEL_SHIFT)
#define TVE_ECMDA_IFMT_MASK		(0x1 << TVE_ECMDA_IFMT_SHIFT)
#define TVE_ECMDA_PHALT_MASK		(0x1 << TVE_ECMDA_PHALT_SHIFT)

/*
 * ECMDB
 */
#define TVE_ECMDB_YBIBLK_SHIFT		(4)
#define TVE_ECMDB_CBW_SHIFT		(2)
#define TVE_ECMDB_YBW_SHIFT		(0)

#define TVE_ECMDB_YBIBLK_MASK		(0x1 << TVE_ECMDB_YBIBLK_SHIFT)
#define TVE_ECMDB_CBW_MASK		(0x3 << TVE_ECMDB_CBW_SHIFT)
#define TVE_ECMDB_YBW_MASK		(0x3 << TVE_ECMDB_YBW_SHIFT)

/*
 * GLK
 */
#define TVE_GLK_XT24_SHIFT		(4)
#define TVE_GLK_GLKEN_SHIFT		(3)
#define TVE_GLK_GLKE_SHIFT		(1)
#define TVE_GLK_GLKPL_SHIFT		(0)

#define TVE_GLK_XT24_MASK		(0x1 << TVE_GLK_XT24_SHIFT)
#define TVE_GLK_GLKEN_MASK		(0x1 << TVE_GLK_GLKEN_SHIFT)
#define TVE_GLK_GLKE_MASK		(0x3 << TVE_GLK_GLKE_SHIFT)
#define TVE_GLK_GLKPL_MASK		(0x1 << TVE_GLK_GLKPL_SHIFT)

/*
 * SCH
 */
#define TVE_SCH_SCH_SHIFT		(0)

#define TVE_SCH_SCH_MASK		(0xFF << TVE_SCH_SCH_SHIFT)

/*
 * HUE
 */
#define TVE_HUE_HUE_SHIFT		(0)

#define TVE_HUE_HUE_MASK		(0xFF << TVE_HUE_HUE_SHIFT)


/*
 * SAT
 */
#define TVE_SAT_SAT_SHIFT		(0)

#define TVE_SAT_SAT_MASK		(0xFF << TVE_SAT_SAT_SHIFT)

/*
 * CONT
 */
#define TVE_CONT_CONT_SHIFT		(0)

#define TVE_CONT_CONT_MASK		(0xFF << TVE_CONT_CONT_SHIFT)

/*
 * BRIGHT
 */
#define TVE_BRIGHT_BRIGHT_SHIFT		(0)

#define TVE_BRIGHT_BRIGHT_MASK		(0xFF << TVE_BRIGHT_BRIGHT_SHIFT)

/*
 * FSC_ADJM
 */
#define TVE_FSC_ADJM_FSC_ADJ_SHIFT		(0)

#define TVE_FSC_ADJM_FSC_ADJ_MASK		(0xFF << TVE_FSC_ADJM_FSC_ADJ_SHIFT)

/*
 * FSC_ADJL
 */
#define TVE_FSC_ADJL_FSC_ADJ_SHIFT		(0)

#define TVE_FSC_ADJL_FSC_ADJ_MASK		(0xFF << TVE_FSC_ADJL_FSC_ADJ_SHIFT)

/*
 * ECMDC
 */
#define TVE_ECMDC_CSMDE_SHIFT		(7)
#define TVE_ECMDC_CSMD_SHIFT		(5)
#define TVE_ECMDC_RGBSYNC_SHIFT		(3)

#define TVE_ECMDC_CSMDE_MASK		(0x1 << TVE_ECMDC_CSMDE_SHIFT)
#define TVE_ECMDC_CSMD_MASK		(0x3 << TVE_ECMDC_CSMD_SHIFT)
#define TVE_ECMDC_RGBSYNC_MASK		(0x3 << TVE_ECMDC_RGBSYNC_SHIFT)

/*
 * DACSEL
 */
#define TVE_DACSEL_DACSEL_SHIFT		(0)

#define TVE_DACSEL_DACSEL_MASK		(0xF << TVE_DACSEL_DACSEL_SHIFT)

/*
 * DACPD
 */
#define TVE_DACPD_PD_SHIFT		(0)

#define TVE_DACPD_PD_MASK		(0x1 << TVE_DACPD_PD_SHIFT)

/*
 * ICNTL
 */
#define TVE_ICNTL_FSIP_SHIFT		(7)
#define TVE_ICNTL_VSIP_SHIFT		(6)
#define TVE_ICNTL_HSIP_SHIFT		(5)
#define TVE_ICNTL_HSVSP_SHIFT		(4)
#define TVE_ICNTL_VSMD_SHIFT		(3)
#define TVE_ICNTL_ISYNC_SHIFT		(0)

#define TVE_ICNTL_FSIP_MASK		(0x1 << TVE_ICNTL_FSIP_SHIFT)
#define TVE_ICNTL_VSIP_MASK		(0x1 << TVE_ICNTL_VSIP_SHIFT)
#define TVE_ICNTL_HSIP_MASK		(0x1 << TVE_ICNTL_HSIP_SHIFT)
#define TVE_ICNTL_HSVSP_MASK		(0x1 << TVE_ICNTL_HSVSP_SHIFT)
#define TVE_ICNTL_VSMD_MASK		(0x1 << TVE_ICNTL_VSMD_SHIFT)
#define TVE_ICNTL_ISYNC_MASK		(0x7 << TVE_ICNTL_ISYNC_SHIFT)

/*
 * HVOFFSET
 */
#define TVE_HVOFFSET_INSEL_SHIFT		(6)
#define TVE_HVOFFSET_VOFFST_SHIFT		(3)
#define TVE_HVOFFSET_HOFFSET_SHIFT		(0)

#define TVE_HVOFFSET_INSEL_MASK		(0x3 << TVE_HVOFFSET_INSEL_SHIFT)
#define TVE_HVOFFSET_VOFFST_MASK		(0x1 << TVE_HVOFFSET_VOFFST_SHIFT)
#define TVE_HVOFFSET_HOFFSET_MASK		(0x7 << TVE_HVOFFSET_HOFFSET_SHIFT)

/*
 * HOFFST
 */
#define TVE_HOFFSET_HOFFSET_SHIFT		(0)

#define TVE_HOFFSET_HOFFSET_MASK		(0xFF << TVE_HOFFSET_HOFFSET_SHIFT)


/*
 * VOFFST
 */
#define TVE_VOFFSET_VOFFSET_SHIFT		(0)

#define TVE_VOFFSET_VOFFSET_MASK		(0xFF << TVE_VOFFSET_VOFFSET_SHIFT)

/*
 * HSVSO
 */
#define TVE_HSVSO_VSOB_SHIFT		(6)
#define TVE_HSVSO_HSOB_SHIFT		(3)
#define TVE_HSVSO_HSOE_SHIFT		(0)

#define TVE_HSVSO_VSOB_MASK		(0x1 << TVE_HSVSO_VSOB_SHIFT)
#define TVE_HSVSO_HSOB_MASK		(0x7 << TVE_HSVSO_HSOB_SHIFT)
#define TVE_HSVSO_HSOE_MASK		(0x7 << TVE_HSVSO_HSOE_SHIFT)

/*
 * HSOE
 */
#define TVE_HSOE_HSOE_SHIFT		(0)

#define TVE_HSOE_HSOE_MASK			(0xFF << TVE_HSOE_HSOE_SHIFT)

/*
 * HSOB
 */
#define TVE_HSOB_HSOB_SHIFT		(0)

#define TVE_HSOB_HSOB_MASK			(0xFF << TVE_HSOB_HSOB_SHIFT)

/*
 * VSOB
 */
#define TVE_VSOB_VSOB_SHIFT		(0)

#define TVE_VSOB_VSOB_MASK			(0xFF << TVE_VSOB_VSOB_SHIFT)

/*
 * VSOE
 */
#define TVE_VSOE_VSOST_SHIFT		(6)
#define TVE_VSOE_NOVRST_SHIFT		(5)
#define TVE_VSOE_VSOE_SHIFT		(0)

#define TVE_VSOE_VSOST_MASK			(0x3 << TVE_VSOE_VSOST_SHIFT)
#define TVE_VSOE_NOVRST_MASK			(0x1 << TVE_VSOE_NOVRST_SHIFT)
#define TVE_VSOE_VSOE_MASK			(0x1F << TVE_VSOE_VSOE_SHIFT)

/*
 * Encoder Control Register offset
 */
#define VENCON		(0x00)
#define VENCIF		(0x04)

/*
 * VENCON
 */

#define VENCON_EN_SHIFT		(0)

#define VENCON_EN_MASK		(0x1 << VENCON_EN_SHIFT)

/*
 * VENCIF
 */
#define VENCIF_UPS_SHIFT		(4)
#define VENCIF_POL_SHIFT		(3)
#define VENCIF_MV_SHIFT		(1)
#define VENCIF_FMT_SHIFT		(0)

#define VENCIF_UPS_MASK		(0x3 << VENCIF_UPS_SHIFT)
#define VENCIF_POL_MASK		(0x1 << VENCIF_POL_SHIFT)
#define VENCIF_MV_MASK		(0x1 << VENCIF_MV_SHIFT)
#define VENCIF_FMT_MASK		(0x1 << VENCIF_FMT_SHIFT)

/*
 * CGMS Register offset
 */
#define CGMS_VSTAT		(0xC0)
#define CGMS_VCTRL		(0xC4)
#define CGMS_CGMSA		(0xE0)
#define CGMS_CGMSB		(0xE4)
#define CGMS_CGMSC		(0xE8)

/*
 * VSTAT
 */
#define CGMS_VSTAT_CGRDY_SHIFT		(1)

#define CGMS_VSTAT_CGRDY_MASK		(0x1 << CGMS_VSTAT_CGRDY_SHIFT)

/*
 * VCTRL
 */
#define CGMS_VCTRL_CGOE_SHIFT		(2)
#define CGMS_VCTRL_CGEE_SHIFT		(1)

#define CGMS_VCTRL_CGOE_MASK		(0x1 << CGMS_VCTRL_CGOE_SHIFT)
#define CGMS_VCTRL_CGEE_MASK		(0x1 << CGMS_VCTRL_CGEE_SHIFT)

/*
 * CGMSA
 */
#define CGMS_CGMSA_CGMS6_SHIFT		(5)
#define CGMS_CGMSA_CGMS5_SHIFT		(4)
#define CGMS_CGMSA_CGMS4_SHIFT		(3)
#define CGMS_CGMSA_CGMS3_SHIFT		(2)
#define CGMS_CGMSA_CGMS2_SHIFT		(1)
#define CGMS_CGMSA_CGMS1_SHIFT		(0)

#define CGMS_CGMSA_CGMS6_MASK		(0x1 << CGMS_CGMSA_CGMS6_SHIFT)
#define CGMS_CGMSA_CGMS5_MASK		(0x1 << CGMS_CGMSA_CGMS5_SHIFT)
#define CGMS_CGMSA_CGMS4_MASK		(0x1 << CGMS_CGMSA_CGMS4_SHIFT)
#define CGMS_CGMSA_CGMS3_MASK		(0x1 << CGMS_CGMSA_CGMS3_SHIFT)
#define CGMS_CGMSA_CGMS2_MASK		(0x1 << CGMS_CGMSA_CGMS2_SHIFT)
#define CGMS_CGMSA_CGMS1_MASK		(0x1 << CGMS_CGMSA_CGMS1_SHIFT)

/*
 * CGMSB
 */
#define CGMS_CGMSB_CGMS14_SHIFT		(7)
#define CGMS_CGMSB_CGMS13_SHIFT		(6)
#define CGMS_CGMSB_CGMS12_SHIFT		(5)
#define CGMS_CGMSB_CGMS11_SHIFT		(4)
#define CGMS_CGMSB_CGMS10_SHIFT		(3)
#define CGMS_CGMSB_CGMS9_SHIFT		(2)
#define CGMS_CGMSB_CGMS8_SHIFT		(1)
#define CGMS_CGMSB_CGMS7_SHIFT		(0)

#define CGMS_CGMSB_CGMS14_MASK		(0x1 << CGMS_CGMSB_CGMS14_SHIFT)
#define CGMS_CGMSB_CGMS13_MASK		(0x1 << CGMS_CGMSB_CGMS13_SHIFT)
#define CGMS_CGMSB_CGMS12_MASK		(0x1 << CGMS_CGMSB_CGMS12_SHIFT)
#define CGMS_CGMSB_CGMS11_MASK		(0x1 << CGMS_CGMSB_CGMS11_SHIFT)
#define CGMS_CGMSB_CGMS10_MASK		(0x1 << CGMS_CGMSB_CGMS10_SHIFT)
#define CGMS_CGMSB_CGMS9_MASK		(0x1 << CGMS_CGMSB_CGMS9_SHIFT)
#define CGMS_CGMSB_CGMS8_MASK		(0x1 << CGMS_CGMSB_CGMS8_SHIFT)
#define CGMS_CGMSB_CGMS7_MASK		(0x1 << CGMS_CGMSB_CGMS7_SHIFT)

/*
 * CGMSC
 */
#define CGMS_CGMSC_CGMS20_SHIFT		(5)
#define CGMS_CGMSC_CGMS19_SHIFT		(4)
#define CGMS_CGMSC_CGMS18_SHIFT		(3)
#define CGMS_CGMSC_CGMS17_SHIFT		(2)
#define CGMS_CGMSC_CGMS16_SHIFT		(1)
#define CGMS_CGMSC_CGMS15_SHIFT		(0)

#define CGMS_CGMSC_CGMS20_MASK		(0x1 << CGMS_CGMSC_CGMS20_SHIFT)
#define CGMS_CGMSC_CGMS19_MASK		(0x1 << CGMS_CGMSC_CGMS19_SHIFT)
#define CGMS_CGMSC_CGMS18_MASK		(0x1 << CGMS_CGMSC_CGMS18_SHIFT)
#define CGMS_CGMSC_CGMS17_MASK		(0x1 << CGMS_CGMSC_CGMS17_SHIFT)
#define CGMS_CGMSC_CGMS16_MASK		(0x1 << CGMS_CGMSC_CGMS16_SHIFT)
#define CGMS_CGMSC_CGMS15_MASK		(0x1 << CGMS_CGMSC_CGMS15_SHIFT)

typedef enum{
	NTSC_M			= 0x0000,
	NTSC_M_J		= 0x0001,
	NTSC_N			= 0x0010,
	NTSC_N_J		= 0x0011,
	NTSC_443		= 0x0020,
	PAL_M			= 0x0100,
	PAL_N			= 0x0110,
	PAL_B			= 0x0120,
	PAL_G			= 0x0130,
	PAL_H			= 0x0140,
	PAL_I			= 0x0150,
	PSEUDO_NTSC		= 0x1000,
	PSEUDO_PAL		= 0x1010,
	COMPOSITE_MODE_MAX
}COMPOSITE_MODE_TYPE;

typedef enum{
	COMPOSITE_LCDC_AOPT_0,	// 100% ~ 0.39% transparency
	COMPOSITE_LCDC_AOPT_1,	// 100% ~ 0.39% transparency
	COMPOSITE_LCDC_AOPT_2,	// 100% ~ 0% transparency
	COMPOSITE_LCDC_AOPT_MAX
}COMPOSITE_LCDC_AOPT_TYPE;

typedef enum{
	COMPOSITE_LCDC_ALPHA_VALUE 	= 0,
	COMPOSITE_LCDC_ALPHA_PIXVALUE = 1	//alpha RGB
}COMPOSITE_LCDC_ASEL_TYPE;

typedef enum{
	COMPOSITE_LCDC_YUV2RGB_TYPE0,
	COMPOSITE_LCDC_YUV2RGB_TYPE1,
	COMPOSITE_LCDC_YUV2RGB_TYPE2,
	COMPOSITE_LCDC_YUV2RGB_TYPE3
}COMPOSITE_LCDC_YUV2RGB_TYPE;

typedef enum{
	COMPOSITE_LCDC_IMG_FMT_1BPP,
	COMPOSITE_LCDC_IMG_FMT_2BPP,
	COMPOSITE_LCDC_IMG_FMT_4BPP,
	COMPOSITE_LCDC_IMG_FMT_8BPP,
	COMPOSITE_LCDC_IMG_FMT_RGB332		= 8,
	COMPOSITE_LCDC_IMG_FMT_RGB444		= 9,
	COMPOSITE_LCDC_IMG_FMT_RGB565		= 10,
	COMPOSITE_LCDC_IMG_FMT_RGB555		= 11,
	COMPOSITE_LCDC_IMG_FMT_RGB888		= 12,
	COMPOSITE_LCDC_IMG_FMT_RGB666		= 13,
	COMPOSITE_LCDC_IMG_FMT_YUV420SP		= 24,	
	COMPOSITE_LCDC_IMG_FMT_YUV422SP		= 25, 
	COMPOSITE_LCDC_IMG_FMT_YUV422SQ		= 26, 
	COMPOSITE_LCDC_IMG_FMT_YUV420ITL0	= 28, 
	COMPOSITE_LCDC_IMG_FMT_YUV420ITL1	= 29, 
	COMPOSITE_LCDC_IMG_FMT_YUV422ITL0	= 30, 
	COMPOSITE_LCDC_IMG_FMT_YUV422ITL1	= 31, 
	COMPOSITE_LCDC_IMG_FMT_MAX
}COMPOSITE_LCDC_IMG_FMT_TYPE;

typedef struct{
	char INTL;							// DMA Interlace Mode
	char AEN;							// Alpha-blending Function for Each Image
	char CEN;							// Chroma-keying Function for Each Image
	char IEN;							// Image Displaying Function for Each Image
	char SRC;							// Image Source Select
	COMPOSITE_LCDC_AOPT_TYPE AOPT;		// Alpha-blending Option selection
	COMPOSITE_LCDC_ASEL_TYPE ASEL;		// Alpha-blending Alpha type
	char UPD;							// Data Update Enable
	char PD;							// Bit padding
	COMPOSITE_LCDC_YUV2RGB_TYPE Y2RMD;	// Y2R mode set
	char Y2R;							// Y2R Conversion Enable bit
	char BR;							// Bit Reverse
	COMPOSITE_LCDC_IMG_FMT_TYPE FMT;	// Image Format 
}COMPOSITE_LCDC_IMG_CTRL_TYPE;

typedef struct{
	unsigned int composite_clk; 		// pixel clock
	unsigned int composite_bus_width;	// data bus width
	unsigned int composite_lcd_width;	// lcd width
	unsigned int composite_lcd_height;	// lcd height

	int pxdw;
	int clkdiv;
	int iv;
	int ih;
	int ip;
	int dp;
	int ni;
	int tv;

	unsigned int composite_LPW; 		// line pulse width
	unsigned int composite_LPC; 		// line pulse count (active horizontal pixel - 1)
	unsigned int composite_LSWC;		// line start wait clock (the number of dummy pixel clock - 1)
	unsigned int composite_LEWC;		// line end wait clock (the number of dummy pixel clock - 1)
	unsigned int composite_VDB; 		// Back porch Vsync delay
	unsigned int composite_VDF; 		// front porch of Vsync delay

	unsigned int composite_FPW1;		// TFT/TV : Frame pulse width is the pulse width of frmae clock
	unsigned int composite_FLC1;		// frmae line count is the number of lines in each frmae on the screen
	unsigned int composite_FSWC1;		// frmae start wait cycle is the number of lines to insert at the end each frame
	unsigned int composite_FEWC1;		// frame start wait cycle is the number of lines to insert at the begining each frame

	unsigned int composite_FPW2;		// TFT/TV : Frame pulse width is the pulse width of frmae clock
	unsigned int composite_FLC2;		// frmae line count is the number of lines in each frmae on the screen
	unsigned int composite_FSWC2;		// frmae start wait cycle is the number of lines to insert at the end each frame
	unsigned int composite_FEWC2; 		// frame start wait cycle is the number of lines to insert at the begining each frame
}COMPOSITE_SPEC_TYPE;

/* Interface API */
extern void internal_tve_set_config(unsigned int type);
extern void internal_tve_clock_onoff(unsigned int onoff);
extern void internal_tve_enable(unsigned int type, unsigned int onoff);
extern void internal_tve_init(void);
extern void internal_tve_set_cgms(unsigned char odd_field_en, unsigned char even_field_en, unsigned int data);
extern void internal_tve_get_cgms(unsigned char *odd_field_en, unsigned char *even_field_en, unsigned int *data, unsigned char *status);
extern void internal_tve_set_cgms_helper(unsigned char odd_field_en, unsigned char even_field_en, unsigned int key);
extern volatile void __iomem* VIOC_TVE_VEN_GetAddress(void);
extern volatile void __iomem* VIOC_TVE_GetAddress(void);
#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
extern void internal_bvo_get_spec(COMPOSITE_MODE_TYPE type, COMPOSITE_SPEC_TYPE *spec);
extern void internal_tve_mv(COMPOSITE_MODE_TYPE type, unsigned int enable);
#endif
#endif /* __VIOC_TVE_H__ */
