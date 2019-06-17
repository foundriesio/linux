/*
 * linux/drivers/video/tcc/tcc_vdv_interface.c
 *
 * Based on: SigmaDesign
 * Author:  <linux@telechips.com>
 * Created:
 * Description: 
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

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_dv_cfg.h>
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_global.h>

#include <video/tcc/tccfb_ioctrl.h>
#include <soc/tcc/pmap.h>


#define __dv_reg_r	__raw_readl
#define __dv_reg_w	__raw_writel


#define BASE_ADDR_VEDR       (0x12500000)
#define BASE_ADDR_VPANEL     (0x12540000)
#define BASE_ADDR_VPANEL_LUT (0x12580000)
#define BASE_ADDR_VDV_CFG    (0x125C0000)

//#define COMPOSER_BYPASS
//#define USE_SINGLE_FRAME
//#define USE_PACKED_FILE

#define DEF_1920x1080P

unsigned int Hactive = 1920; // lpc + 1
unsigned int Vactive = 1080; // flc + 1
unsigned int Hfront = 88;    // lewc + 1
unsigned int Hsync	= 44;    // lpw + 1
unsigned int Hback	= 148;   // lswc + 1
unsigned int Vfront = 4;     // fewc + 1
unsigned int Vsync	= 5;     // fpw + 1
unsigned int Vback	= 36;    // fswc + 1

unsigned int VDE_VStart	= 10;
unsigned int VDE_HStart	= 20;

static OUT_TYPE out_type = DOVI;
static OUT_FORMAT out_color_format = DV_OUT_FMT_YUV422;

static pmap_t pmap_dv_regs;
static void __iomem *pBase_vAddr;
static char bOsd1_on = 0;
static char bOsd3_on = 0;
static char bStart_config = 0;
static char bShadow_context = 1;
static char bMeta_PrevStatus = 0;
static char bMeta_changed = 0;
#if defined(CONFIG_TCC_DV_IN)
static DV_PATH dv_hdmi_path = DV_PATH_DIRECT_VIN_WDMA;
static DV_PATH dv_hdmi_noYuv422_path = DV_PATH_VIN_ALL;
static DV_PATH dv_component_path = DV_PATH_VIN_ALL;
//static DV_PATH dv_hdmi_path = DV_PATH_VIN; //Can't use it due to DV_IN tunneling bug!!
#else
static DV_PATH dv_hdmi_path = DV_PATH_DIRECT;
#endif
static DV_MODE dv_mode = DV_STD;
static DV_STAGE dv_stage = DV_OFF;
static VIDEO_ATTR video_attr = ATTR_SDR;

static char DV_HDMI_OUT = 1;
static unsigned int DV_HDMI_CLK_Khz = 0;
static char DV_HDMI_noYUV422_OUT = 0;
static char hdmi_vsvdb[48];
static unsigned int hdmi_sz_vsvdb = 0;

static int debug = 0;
#define dprintk(msg...)	 if (debug) { printk( "vioc_v_dv: " msg); }

#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
extern void tca_edr_inc_check_count(unsigned int nInt, unsigned int nTry, unsigned int nProc, unsigned int nUpdated, unsigned int bInit_all);
#endif

//#define SHADOW_CONTEXT_AT_THE_SAME_TIME

DV_PATH vioc_get_path_type(void)
{
	DV_PATH dv_out_path = 0;

#if defined(CONFIG_TCC_DV_IN)
	if(DV_HDMI_OUT)
	{
		if((vioc_get_out_type() == DOVI) && (dv_mode == DV_STD))
		{
			//In case of Standard Dolby output, the output format is ICtCp which can't be converted other color format.
			//Lion chipset doesn't have CSC(Color Space Conversion) h/w block for ICtCp color format.
			dv_out_path = DV_PATH_DIRECT;
		}
		else
		{
			//In case of Low-Latency or SDR/HDR10 output, the output format is YCbCr which can be converted other color format.
			if(DV_HDMI_noYUV422_OUT)
				dv_out_path = dv_hdmi_noYuv422_path;
			else
				dv_out_path = dv_hdmi_path;
		}
	}
	else
	{
		dv_out_path = dv_component_path;
	}
#else	
	dv_out_path = dv_hdmi_path;
#endif

//	dprintk("### DV_PATH = 0x%x \n", dv_out_path);
	return dv_out_path;
}

void vioc_set_out_type(OUT_TYPE type)
{
	out_type = type;
	if( out_type == DOVI_LL)
		dv_mode = DV_LL;
	else
		dv_mode = DV_STD;
}

OUT_TYPE vioc_get_out_type(void)
{
	return out_type;
}

VIDEO_ATTR vioc_get_video_attribute(void)
{
#if 1 // [no-output when playing dolby-contents] In case of analog-output.
	if(video_attr != ATTR_SDR)
		return ATTR_DV_WITHOUT_BC;
#endif
	return video_attr;
}

void __iomem * _get_virtual_address(unsigned int phy_addr)
{
	if(pBase_vAddr == NULL)
		return 0x00;

	if((phy_addr < pmap_dv_regs.base) || (phy_addr > (pmap_dv_regs.base + pmap_dv_regs.size)))
	{
		printk("Error: phy(0x%x) is more or less than available region(0x%x ~ 0x%x).\n", phy_addr, pmap_dv_regs.base, pmap_dv_regs.base + pmap_dv_regs.size); 
		return 0x00;
	}

	return pBase_vAddr + (phy_addr - (unsigned int)pmap_dv_regs.base);
}

void vioc_v_dv_set_mode(DV_MODE mode, unsigned char* vsvdb, unsigned int sz_vsvdb)
{
	dprintk_dv_sequence("### DV_Mode %d \n", mode);
	dv_mode = mode;
	hdmi_sz_vsvdb = sz_vsvdb;
	if(vsvdb && (sz_vsvdb > 0)){
		memcpy((void*)hdmi_vsvdb, (void*)vsvdb, sz_vsvdb);
	}
}

DV_MODE vioc_v_dv_get_mode(void)
{
	return dv_mode;
}

unsigned int vioc_v_dv_get_vsvdb(unsigned char* vsvdb)
{
	if((vioc_get_out_type() != DOVI) && (vioc_get_out_type() != DOVI_LL))
		return 0;

	if(vsvdb && (hdmi_sz_vsvdb > 0)){
		memcpy((void*)vsvdb, (void*)hdmi_vsvdb, hdmi_sz_vsvdb);
	}

	return hdmi_sz_vsvdb;
}

void vioc_v_dv_set_stage(DV_STAGE stage)
{
	dprintk_dv_sequence("### DV_Stage %d \n", stage);
	dv_stage = stage;
}

DV_STAGE vioc_v_dv_get_stage(void)
{
	return dv_stage;
}

void vioc_v_dv_reset(void)
{
	vioc_set_out_type(SDR);
	dv_mode = DV_OFF;
}

char vioc_v_dv_check_hdmi_out(void)
{
	return DV_HDMI_OUT;
}

void vioc_v_dv_set_output_color_format(unsigned int pxdw, unsigned int swap)
{
	switch(pxdw) {
		case 21:
			out_color_format = 2;
			break;
		case 12:
		case 23:
			if(swap == 0) {
				out_color_format = 0;
			} else {
				out_color_format = 1;
			}
			break;
		case 26:
		case 27:
			out_color_format = 3;
			break;
		default:
			out_color_format = 2;
			break;
	}

	printk("%s-%d :: color format %d :: pxdw(%d), swap(%d)\n",
					__func__, __LINE__, out_color_format, pxdw, swap);

}

OUT_FORMAT vioc_v_dv_get_output_color_format(void)
{
	return out_color_format;
}

unsigned int vioc_v_dv_get_lcd0_clk_khz(void)
{
	unsigned int adjust_clk_khz, temp;

	if(DV_HDMI_CLK_Khz >= 100000)
	{
		temp = (DV_HDMI_CLK_Khz * 130)/100;
		adjust_clk_khz = ((temp + 99999)/100000) * 100000;
	}
	else
	{
		temp = (DV_HDMI_CLK_Khz * 130)/100;
		adjust_clk_khz = ((temp + 49999)/50000) * 50000;
	}
		
	return adjust_clk_khz;
}

void voic_v_dv_set_hdmi_timming(struct lcdc_timimg_parms_t *mode, int bHDMI_Out, unsigned int hdmi_khz)
{
	DV_HDMI_OUT = bHDMI_Out;
	DV_HDMI_CLK_Khz = hdmi_khz * mode->dv_noYUV422_SDR;

	if(!bHDMI_Out)
		out_color_format = 2;

	if( mode->format >= 2 ){
		if(mode->dv_ll_mode == DV_STD)
			vioc_set_out_type(DOVI);
		else
			vioc_set_out_type(DOVI_LL);
	}
	else if(mode->format == 1)
		vioc_set_out_type(HDR10);
	else
		vioc_set_out_type(SDR);

	DV_HDMI_noYUV422_OUT = mode->dv_noYUV422_SDR;

	Hactive = mode->lpc + 1;
	Vactive = mode->flc + 1;
	Hfront 	= mode->lewc + 1;
	Hsync	= mode->lpw + 1;
	Hback	= mode->lswc + 1;
	Vfront 	= mode->fewc + 1;
	Vsync	= mode->fpw + 1;
	Vback	= mode->fswc + 1;
	VDE_VStart	= 10; //Need to fix
	VDE_HStart	= 20; //Need to fix

	pr_info("@@ Dolby-path (%d : 0-DOVI, 1-HDR10, 2-SDR(%d), 3-DOVI_LL) :: HDMI ? %d (%d khz):: %d / %d / %d / %d / %d / %d / %d / %d \n",
			vioc_get_out_type(), DV_HDMI_noYUV422_OUT,
			bHDMI_Out, DV_HDMI_CLK_Khz,
			Hactive, Vactive, Hfront, Hsync, Hback, Vfront, Vsync, Vback);
}

void _vioc_v_dv_prog_1st_done(void) {
	volatile void __iomem *pVEDR = NULL;
	volatile void __iomem *pVPANEL = NULL;
	unsigned int value = 0x00;

	pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);
	pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);

    //@ (program_done);//force all LUT to function read mode
    //force   `V_EDR.reg_glb_pq2lLut_lut_en      = 1'b0; // reg02
    //force   `V_EDR.reg_glb_l2pqLut_lut_en      = 1'b0; // reg02
    //*(volatile unsigned int *)(pVEDR+0x3c008)   &= ~((0x1<<19)|(0x1<<11));
	value = __dv_reg_r(pVEDR+0x3c008) & ~((0x1<<19)|(0x1<<11));
	value |= ((0x0<<19)|(0x0<<11));
	__dv_reg_w(value, pVEDR+0x3c008);

    //force   `V_PANEL.reg_glb_pq2lLut_lut_en_u4 = 1'b0; // 014h
    //force   `V_PANEL.reg_glb_l2pqLut_lut_en_u4 = 1'b0; // 014h
    //*(volatile unsigned int *)(pVPANEL+0x00014) &= ~((0x1<<19)|(0x1<<11));
	value = __dv_reg_r(pVPANEL+0x00014) & ~((0x1<<19)|(0x1<<11));
	value |= ((0x0<<19)|(0x0<<11));
	__dv_reg_w(value, pVPANEL+0x00014);

    //force   `V_PANEL.reg_glb_pq2lLut_lut_en_u6 = 1'b0; // 0b0h
    //force   `V_PANEL.reg_glb_l2pqLut_lut_en_u6 = 1'b0; // 0b0h
    //*(volatile unsigned int *)(pVPANEL+0x000b0) &= ~((0x1<<19)|(0x1<<11));
	value = __dv_reg_r(pVPANEL+0x000b0) & ~((0x1<<19)|(0x1<<11));
	value |= ((0x0<<19)|(0x0<<11));
	__dv_reg_w(value, pVPANEL+0x000b0);

    //force   `V_PANEL.reg_glb_pq2lLut_lut_en_u8 = 1'b0; // 148h
    //force   `V_PANEL.reg_glb_l2pqLut_lut_en_u8 = 1'b0; // 148h
    //force   `V_PANEL.reg_glb_l2gLut_lut_en_u8  = 1'b0; // 148h
    //*(volatile unsigned int *)(pVPANEL+0x00148) &= ~((0x1<<19)|(0x1<<11)|(0x1<<3));
	value = __dv_reg_r(pVPANEL+0x00148) & ~((0x1<<19)|(0x1<<11)|(0x1<<3));
	value |= ((0x0<<19)|(0x0<<11)|(0x0<<3));
	__dv_reg_w(value, pVPANEL+0x00148);

    //force   `V_PANEL.reg_soft_arstn            = 16'hffff;
    //*(volatile unsigned int *)(pVPANEL+0x0000c) |= (0xffff<<0);
	value = __dv_reg_r(pVPANEL+0x0000c) & ~(0xffff<<0);
	value |= (0xffff<<0);
	__dv_reg_w(value, pVPANEL+0x0000c);
}

void _vioc_v_dv_prog_start(void) {
	volatile void __iomem *pVEDR = NULL;
	volatile void __iomem *pVPANEL = NULL;
	volatile void __iomem *pVDVCFG = NULL;
	unsigned int value = 0x00;

	pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);
	pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);
	pVDVCFG = VIOC_DV_VEDR_GetAddress(VDV_CFG);
	
    //@ (program_start);//force all LUT to CPU prog mode
    //repeat(200) @(posedge cpu_clk);
    //force   `V_EDR.reg_glb_g2lLut_lut_en       = 1'b1; // reg02
    //force   `V_EDR.reg_glb_cvmLut_lut_en       = 1'b1; // reg02
    //*(volatile unsigned int *)(pVEDR+0x3c008)   |= ((0x1<<3)|(0x1<<27));
	value = __dv_reg_r(pVEDR+0x3c008) & ~((0x1<<3)|(0x1<<27));
	value |= ((0x1<<3)|(0x1<<27));
	__dv_reg_w(value, pVEDR+0x3c008);

    //force   `V_PANEL.reg_glb_cvmLut_lut_en_u4  = 1'b1; // 010h
    //*(volatile unsigned int *)(pVPANEL+0x00010) |= (0x1<<11);
	value = __dv_reg_r(pVPANEL+0x00010) & ~(0x1<<11);
	value |= (0x1<<11);
	__dv_reg_w(value, pVPANEL+0x00010);

    //force   `V_PANEL.reg_glb_g2lLut_lut_en_u4  = 1'b1; // 014h
    //*(volatile unsigned int *)(pVPANEL+0x00014) |= (0x1<<3);
	value = __dv_reg_r(pVPANEL+0x00014) & ~(0x1<<3);
	value |= (0x1<<3);
	__dv_reg_w(value, pVPANEL+0x00014);

    //force   `V_PANEL.reg_glb_cvmLut_lut_en_u6  = 1'b1; // 0ach
    //*(volatile unsigned int *)(pVPANEL+0x000ac) |= (0x1<<11);
	value = __dv_reg_r(pVPANEL+0x000ac) & ~(0x1<<11);
	value |= (0x1<<11);
	__dv_reg_w(value, pVPANEL+0x000ac);

    //force   `V_PANEL.reg_glb_g2lLut_lut_en_u6  = 1'b1; // 0b0h
    //*(volatile unsigned int *)(pVPANEL+0x000b0) |= (0x1<<3);
	value = __dv_reg_r(pVPANEL+0x000b0) & ~(0x1<<3);
	value |= (0x1<<3);
	__dv_reg_w(value, pVPANEL+0x000b0);

	value = __dv_reg_r(pVDVCFG+TX_INV) & ~(TX_INV_HS_MASK | TX_INV_VS_MASK);
	if(Hactive == 720 && Vactive == 480){
		//*(volatile unsigned int *)(pVDVCFG+0x00010) |= (0x1<<1 | 0x1<<0);
		value |= ((0x1<<TX_INV_HS_SHIFT | 0x1<<TX_INV_VS_SHIFT));
	}
	else {
		//*(volatile unsigned int *)(pVDVCFG+0x00010) &= ~(0x3);
		value |= ((0x0<<TX_INV_HS_SHIFT | 0x0<<TX_INV_VS_SHIFT));
	}
	__dv_reg_w(value, pVDVCFG+TX_INV);

}

void _vioc_v_dv_prog_done(void) {
	volatile void __iomem *pVEDR = NULL;
	volatile void __iomem *pVPANEL = NULL;
	unsigned int value = 0x00;

	pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);
	pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);

    //@ (program_done);//force all LUT to function read mode
    //repeat(200) @(posedge cpu_clk);
    //force   `V_EDR.reg_glb_g2lLut_lut_en       = 1'b0; // reg02
    //force   `V_EDR.reg_glb_cvmLut_lut_en       = 1'b0; // reg02
    //*(volatile unsigned int *)(pVEDR+0x3c008)   &= ~((0x1<<3)|(0x1<<27));
	value = __dv_reg_r(pVEDR+0x3c008) & ~((0x1<<3)|(0x1<<27));
	value |= ((0x0<<3)|(0x0<<27));
	__dv_reg_w(value, pVEDR+0x3c008);

    //force   `V_PANEL.reg_glb_cvmLut_lut_en_u4  = 1'b0; // 010h
    //*(volatile unsigned int *)(pVPANEL+0x00010) &= ~(0x1<<11);
	value = __dv_reg_r(pVPANEL+0x00010) & ~(0x1<<11);
	value |= (0x0<<11);
	__dv_reg_w(value, pVPANEL+0x00010);

    //force   `V_PANEL.reg_glb_g2lLut_lut_en_u4  = 1'b0; // 014h
    //*(volatile unsigned int *)(pVPANEL+0x00014) &= ~(0x1<<3);
	value = __dv_reg_r(pVPANEL+0x00014) & ~(0x1<<3);
	value |= (0x0<<3);
	__dv_reg_w(value, pVPANEL+0x00014);

    //force   `V_PANEL.reg_glb_cvmLut_lut_en_u6  = 1'b0; // 0ach
    //*(volatile unsigned int *)(pVPANEL+0x000ac) &= ~(0x1<<11);
	value = __dv_reg_r(pVPANEL+0x000ac) & ~(0x1<<11);
	value |= (0x0<<11);
	__dv_reg_w(value, pVPANEL+0x000ac);

    //force   `V_PANEL.reg_glb_g2lLut_lut_en_u6  = 1'b0; // 0b0h
    //*(volatile unsigned int *)(pVPANEL+0x000b0) &= ~(0x1<<3);
	value = __dv_reg_r(pVPANEL+0x000b0) & ~(0x1<<3);
	value |= (0x0<<3);
	__dv_reg_w(value, pVPANEL+0x000b0);

}

unsigned int get_register(unsigned int base_addr, VEDR_TYPE type)
{
	unsigned int reg_offset;

	if(type == VEDR)
		reg_offset = (base_addr - BASE_ADDR_VEDR);
	else if(type == VPANEL)
		reg_offset = (base_addr - BASE_ADDR_VPANEL);
	else if(type == VPANEL_LUT)
		reg_offset = (base_addr - BASE_ADDR_VPANEL_LUT);
	else//(type == VDV_CFG)
		reg_offset = (base_addr - BASE_ADDR_VDV_CFG);

	return reg_offset;
}

void _void_reset_edr_compnent(int ctrc, int dm, int composer)
{
	unsigned int reset = 0;
	unsigned int value = 0x00;
	volatile void __iomem *pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);

	if(!ctrc)
		reset |= 0x1;
	if(!dm)
		reset |= 0x2;
	if(!composer)
		reset |= 0x4;

//	pr_info("EDR_Reset : 0x%x \n", reset);

	//pVEDR->vedr.core.reset.value = reset;
	value = __dv_reg_r(pVEDR+0x3c004) & ~((0x1<<0)|(0x1<<1)|(0x1<<2));
	value = reset;
	__dv_reg_w(value, pVEDR+0x3c004);
}

void vioc_v_dv_swreset(unsigned int edr, unsigned panel, unsigned int crtc)
{
	if(panel || crtc)
	{
		volatile void __iomem *pVPANEL = NULL;
		unsigned int value = 0x00;

		pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);

		if(panel)
		{
			value = __dv_reg_r(pVPANEL+0xC) & ~(0xFFFF);
			__dv_reg_w(value, pVPANEL+0xC);

			msleep(1);
			dprintk_dv_sequence("### V_DV PANEL s/w reset\n");

			value = __dv_reg_r(pVPANEL+0xC) & ~(0xFFFF);
			value |= 0xFFFF;
			__dv_reg_w(value, pVPANEL+0xC);
		}

		if(crtc)
		{
			value = __dv_reg_r(pVPANEL+0xC) & ~(0x1 << 31);
			value |= (0x1 << 31);
			__dv_reg_w(value, pVPANEL+0x0810);

			msleep(1);
			dprintk_dv_sequence("### V_DV CRTC s/w reset\n");

			value = __dv_reg_r(pVPANEL+0xC) & ~(0x1 << 31);
			__dv_reg_w(value, pVPANEL+0x0810);
		}
	}

	if(edr)
	{
		_void_reset_edr_compnent(1,1,1);
		msleep(1);
		dprintk_dv_sequence("### V_DV EDR s/w reset\n");
		_void_reset_edr_compnent(0,0,0);
	}
}

void _voic_set_edr_lut(void __iomem *reg_VirtAddr)
{
	int i = 0;	
	volatile void __iomem *pVEDR = NULL;
	struct TccEdrV1Reg *pReg_Src = (struct TccEdrV1Reg *)reg_VirtAddr;

//LUT :: 0x12500000
	pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);

	for(i=0; i<256; i++){
		//pVEDR->vedrLut.vdG2llut[i] = pReg_Src->vedrLut.vdG2llut[i]; // 0x0000
		__dv_reg_w(pReg_Src->vedrLut.vdG2llut[i].value, pVEDR+0x0000+(0x4*i));
	}

	for(i=0; i<1024; i++){
		//pVEDR->vedrLut.pq2llut[i] = pReg_Src->vedrLut.pq2llut[i]; // 0x1000
		__dv_reg_w(pReg_Src->vedrLut.pq2llut[i].value, pVEDR+0x1000+(0x4*i));
	}

	for(i=0; i<128; i++){
		//pVEDR->vedrLut.l2pqlutx[i] = pReg_Src->vedrLut.l2pqlutx[i]; // 0x2000
		__dv_reg_w(pReg_Src->vedrLut.l2pqlutx[i].value, pVEDR+0x2000+(0x4*i));
	}

	for(i=0; i<128; i++){
		//pVEDR->vedrLut.l2pqluta[i] = pReg_Src->vedrLut.l2pqluta[i]; // 0x2200
		__dv_reg_w(pReg_Src->vedrLut.l2pqluta[i].value, pVEDR+0x2200+(0x4*i));
	}

	for(i=0; i<128; i++){
		//pVEDR->vedrLut.l2pqlutb[i] = pReg_Src->vedrLut.l2pqlutb[i]; // 0x2400
		__dv_reg_w(pReg_Src->vedrLut.l2pqlutb[i].value, pVEDR+0x2400+(0x4*i));
	}

	for(i=0; i<256; i++){
		//pVEDR->vedrLut.vdTmluti[i] = pReg_Src->vedrLut.vdTmluti[i]; // 0x3000
		__dv_reg_w(pReg_Src->vedrLut.vdTmluti[i].value, pVEDR+0x3000+(0x4*i));
	}

	for(i=0; i<256; i++){
		//pVEDR->vedrLut.vdSmluti[i] = pReg_Src->vedrLut.vdSmluti[i]; // 0x3400
		__dv_reg_w(pReg_Src->vedrLut.vdSmluti[i].value, pVEDR+0x3400+(0x4*i));
	}

	for(i=0; i<256; i++){
		//pVEDR->vedrLut.vdTmluts[i] = pReg_Src->vedrLut.vdTmluts[i]; // 0x3800
		__dv_reg_w(pReg_Src->vedrLut.vdTmluts[i].value, pVEDR+0x3800+(0x4*i));
	}

	for(i=0; i<256; i++){
		//pVEDR->vedrLut.vdSmluts[i] = pReg_Src->vedrLut.vdSmluts[i]; // 0x3c00
		__dv_reg_w(pReg_Src->vedrLut.vdSmluts[i].value, pVEDR+0x3c00+(0x4*i));
	}
}

void __vioc_set_edr_specific(struct TccEdrV1Reg *pReg_Src)
{
	volatile void __iomem *pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);
	unsigned int value = 0x00;

//Core :: 0x1253C014 ~ 2C
	value 	= pReg_Src->vedr.core.unk5.value;
    value	 &= ~(0xFF<<0); // vswidth
    value	 &= ~(0xFF<<8); // hswidth
    value	 &= ~(0x1<<20); // sel_ext_vs
    value	 &= ~(0x1<<21); // sel_ext_hs
    value	|= ((Vsync<<0)|(Hsync<<8)|(1<<20)|(1<<21));
	//pVEDR->vedr.core.unk5.value	 	= value;    // 14
	__dv_reg_w(value, pVEDR+0x3c014);

	value 	= pReg_Src->vedr.core.unk6.value;
    value	 &= ~(0xFFFFFFFF);
	value	|= (((Hsync + Hback)<<0)|(Hactive<<16));
    //pVEDR->vedr.core.unk6.value	 	= value;    // 18
	__dv_reg_w(value, pVEDR+0x3c018);

	value 	= pReg_Src->vedr.core.unk7.value;
    value	 &= ~(0xFFFFFFFF);
	value	|= (((Vsync+Vback-12)<<0)|(Vactive<<16));
    //pVEDR->vedr.core.unk7.value	 	= value;    // 1c	
	__dv_reg_w(value, pVEDR+0x3c01c);

	value 	= pReg_Src->vedr.core.unk8.value;
    value	 &= ~(0xFFFFFFFF);
	value	|= (((Vactive+Vfront+Vsync+Vback)<<0)|((Hactive+Hfront+Hsync+Hback)<<16));
    //pVEDR->vedr.core.unk8.value	 	= value;    // 20
	__dv_reg_w(value, pVEDR+0x3c020);
	
	//pVEDR->vedr.core.unk9 		= pReg_Src->vedr.core.unk9;    // 24 //ZzaU :: Need to change, It depends on HDMI Resolution.
	__dv_reg_w(pReg_Src->vedr.core.unk9.value, pVEDR+0x3c024);
	//pVEDR->vedr.core.unk10 		= pReg_Src->vedr.core.unk10;   // 28 //ZzaU :: Need to change, It depends on HDMI Resolution.
	__dv_reg_w(pReg_Src->vedr.core.unk10.value, pVEDR+0x3c028);

	value 	= pReg_Src->vedr.dm.unk7.value;
	value	&= ~(0xFFFFFFFF);
	value	|= ((VDE_VStart<<0)|(VDE_HStart<<16));
	//pVEDR->vedr.dm.unk7.value	= value;        // d8
	__dv_reg_w(value, pVEDR+0x3c4d8);

	value 	= pReg_Src->vedr.dm.unk8.value;
	value	&= ~(0xFFFFFFFF);
	value	|= ((Vactive<<0)|(Hactive<<16));
	//pVEDR->vedr.dm.unk8.value	= value;        // dc
	__dv_reg_w(value, pVEDR+0x3c4dc);
}

void _voic_set_edr(void __iomem *reg_VirtAddr, unsigned int frmcnt)
{
	int i = 0;
	unsigned int value = 0x00;
	volatile void __iomem *pVEDR = NULL;
	struct TccEdrV1Reg *pReg_Src = (struct TccEdrV1Reg *)reg_VirtAddr;

	pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);

//Core :: 0x1253C000
	//pVEDR->vedr.core.glb0 		= pReg_Src->vedr.core.glb0;    //  0
	__dv_reg_w(pReg_Src->vedr.core.glb0.value, pVEDR+0x3c000);
    if ( frmcnt == 0 ){
		dprintk("%s-%d :: reset DV\n", __func__, __LINE__);
		mdelay(16);
		_void_reset_edr_compnent(1, 1, 1); // reset all.
		_void_reset_edr_compnent(1, 0, 0); // reset ctrc only.
		//pVEDR->vedr.core.glb1 		= pReg_Src->vedr.core.glb1;    //  8
		__dv_reg_w(pReg_Src->vedr.core.glb1.value, pVEDR+0x3c008);
    }
	//pVEDR->vedr.core.comp1Ext 	= pReg_Src->vedr.core.comp1Ext;//  c
	__dv_reg_w(pReg_Src->vedr.core.comp1Ext.value, pVEDR+0x3c00c);
	//pVEDR->vedr.core.glb2 		= pReg_Src->vedr.core.glb2;    // 10
	__dv_reg_w(pReg_Src->vedr.core.glb2.value, pVEDR+0x3c010);

#if 0 //el bypass test
	{
		value 	= pReg_Src->vedr.core.comp0.value;
		value	|= (0x4); // 100
		//pVEDR->vedr.core.comp0.value	= value;
		__dv_reg_w(value, pVEDR+0x3c100);
	}
#else
	//pVEDR->vedr.core.comp0 		= pReg_Src->vedr.core.comp0;   // 100
	__dv_reg_w(pReg_Src->vedr.core.comp0.value, pVEDR+0x3c100);
#endif
	//pVEDR->vedr.core.comp1 		= pReg_Src->vedr.core.comp1;   // 104
	__dv_reg_w(pReg_Src->vedr.core.comp1.value, pVEDR+0x3c104);
	//pVEDR->vedr.core.comp2 		= pReg_Src->vedr.core.comp2;   // 108
	__dv_reg_w(pReg_Src->vedr.core.comp2.value, pVEDR+0x3c108);
	//pVEDR->vedr.core.comp3 		= pReg_Src->vedr.core.comp3;   // 10c
	__dv_reg_w(pReg_Src->vedr.core.comp3.value, pVEDR+0x3c10c);
	//pVEDR->vedr.core.comp4 		= pReg_Src->vedr.core.comp4;   // 110
	__dv_reg_w(pReg_Src->vedr.core.comp4.value, pVEDR+0x3c110);
	//pVEDR->vedr.core.comp5 		= pReg_Src->vedr.core.comp5;   // 114
	__dv_reg_w(pReg_Src->vedr.core.comp5.value, pVEDR+0x3c114);
	//pVEDR->vedr.core.comp6 		= pReg_Src->vedr.core.comp6;   // 118
	__dv_reg_w(pReg_Src->vedr.core.comp6.value, pVEDR+0x3c118);
	//pVEDR->vedr.core.comp7 		= pReg_Src->vedr.core.comp7;   // 11c
	__dv_reg_w(pReg_Src->vedr.core.comp7.value, pVEDR+0x3c11c);
	//pVEDR->vedr.core.comp8 		= pReg_Src->vedr.core.comp8;   // 120
	__dv_reg_w(pReg_Src->vedr.core.comp8.value, pVEDR+0x3c120);

	for(i=0; i<27; i++)
	{
		//pVEDR->vedr.core.blMapCoeffU[i] = pReg_Src->vedr.core.blMapCoeffU[i];	// 124 .. 18c		
		__dv_reg_w(pReg_Src->vedr.core.blMapCoeffU[i].value, pVEDR+0x3c124+(0x4*i));
		//pVEDR->vedr.core.blMapCoeffV[i] = pReg_Src->vedr.core.blMapCoeffV[i];	// 190 .. 1f8
		__dv_reg_w(pReg_Src->vedr.core.blMapCoeffV[i].value, pVEDR+0x3c190+(0x4*i));
	}
	
	//pVEDR->vedr.core.comp63 			= pReg_Src->vedr.core.comp63;           // 1fc  
	__dv_reg_w(pReg_Src->vedr.core.comp63.value, pVEDR+0x3c1fc);  
	//pVEDR->vedr.core.comp64 			= pReg_Src->vedr.core.comp64;           // 200
	__dv_reg_w(pReg_Src->vedr.core.comp64.value, pVEDR+0x3c200);

	//pVEDR->vedr.core.elNlqCoeffY6332 	= pReg_Src->vedr.core.elNlqCoeffY6332;  // 204
	__dv_reg_w(pReg_Src->vedr.core.elNlqCoeffY6332.value, pVEDR+0x3c204);
	//pVEDR->vedr.core.elNlqCoeffY310 	= pReg_Src->vedr.core.elNlqCoeffY310;   // 208
	__dv_reg_w(pReg_Src->vedr.core.elNlqCoeffY310.value, pVEDR+0x3c208);
	//pVEDR->vedr.core.comp67 			= pReg_Src->vedr.core.comp67;           // 20c
	__dv_reg_w(pReg_Src->vedr.core.comp67.value, pVEDR+0x3c20c);

	//pVEDR->vedr.core.elNlqCoeffU6332 	= pReg_Src->vedr.core.elNlqCoeffU6332;  // 210
	__dv_reg_w(pReg_Src->vedr.core.elNlqCoeffU6332.value, pVEDR+0x3c210);
	//pVEDR->vedr.core.elNlqCoeffU310 	= pReg_Src->vedr.core.elNlqCoeffU310;   // 214
	__dv_reg_w(pReg_Src->vedr.core.elNlqCoeffU310.value, pVEDR+0x3c214);
	//pVEDR->vedr.core.comp70 			= pReg_Src->vedr.core.comp70;           // 218
	__dv_reg_w(pReg_Src->vedr.core.comp70.value, pVEDR+0x3c218);
	//pVEDR->vedr.core.blMapPivotU310 	= pReg_Src->vedr.core.blMapPivotU310;   // 21c
	__dv_reg_w(pReg_Src->vedr.core.blMapPivotU310.value, pVEDR+0x3c21c);
	//pVEDR->vedr.core.comp72 			= pReg_Src->vedr.core.comp72;           // 220
	__dv_reg_w(pReg_Src->vedr.core.comp72.value, pVEDR+0x3c220);
	//pVEDR->vedr.core.blMapPivotY310 	= pReg_Src->vedr.core.blMapPivotY310;   // 224
	__dv_reg_w(pReg_Src->vedr.core.blMapPivotY310.value, pVEDR+0x3c224);
	//pVEDR->vedr.core.blMapPivotY6332 	= pReg_Src->vedr.core.blMapPivotY6332;  // 228
	__dv_reg_w(pReg_Src->vedr.core.blMapPivotY6332.value, pVEDR+0x3c228);
	//pVEDR->vedr.core.blMapPivotY9564 	= pReg_Src->vedr.core.blMapPivotY9564;  // 22c
	__dv_reg_w(pReg_Src->vedr.core.blMapPivotY9564.value, pVEDR+0x3c22c);

	for(i=0; i<52; i++)
	{
		//pVEDR->vedr.core.upsamplingCoeffY[i] = pReg_Src->vedr.core.upsamplingCoeffY[i];    // 230 .. 2fc
		__dv_reg_w(pReg_Src->vedr.core.upsamplingCoeffY[i].value, pVEDR+0x3c230+(0x4*i));
	}
	for(i=0; i<24; i++)
	{
		//pVEDR->vedr.core.blMapCoeffY[i] = pReg_Src->vedr.core.blMapCoeffY[i];    // 300 .. 35c
		__dv_reg_w(pReg_Src->vedr.core.blMapCoeffY[i].value, pVEDR+0x3c300+(0x4*i));
	}         

//DM :: 0x1253C400
	//pVEDR->vedr.dm.bioCtrl0 	= pReg_Src->vedr.dm.bioCtrl0;    // 00	
	__dv_reg_w(pReg_Src->vedr.dm.bioCtrl0.value, pVEDR+0x3c400);
	//pVEDR->vedr.dm.bioKsuds02 	= pReg_Src->vedr.dm.bioKsuds02;  // 04
	__dv_reg_w(pReg_Src->vedr.dm.bioKsuds02.value, pVEDR+0x3c404);
	//pVEDR->vedr.dm.bioKsuds03 	= pReg_Src->vedr.dm.bioKsuds03;  // 08
	__dv_reg_w(pReg_Src->vedr.dm.bioKsuds03.value, pVEDR+0x3c408);
	//pVEDR->vedr.dm.bioKsuds04 	= pReg_Src->vedr.dm.bioKsuds04;  // 0c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsuds04.value, pVEDR+0x3c40c);
	//pVEDR->vedr.dm.bioKsuds07 	= pReg_Src->vedr.dm.bioKsuds07;  // 10
	__dv_reg_w(pReg_Src->vedr.dm.bioKsuds07.value, pVEDR+0x3c410);
	//pVEDR->vedr.dm.bioKsuds10 	= pReg_Src->vedr.dm.bioKsuds10;  // 14
	__dv_reg_w(pReg_Src->vedr.dm.bioKsuds10.value, pVEDR+0x3c414);
	//pVEDR->vedr.dm.bioKsuds11 	= pReg_Src->vedr.dm.bioKsuds11;  // 18
	__dv_reg_w(pReg_Src->vedr.dm.bioKsuds11.value, pVEDR+0x3c418);
	//pVEDR->vedr.dm.bioKsimap00 	= pReg_Src->vedr.dm.bioKsimap00; // 2c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap00.value, pVEDR+0x3c42c);
	//pVEDR->vedr.dm.bioKsimap01 	= pReg_Src->vedr.dm.bioKsimap01; // 30
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap01.value, pVEDR+0x3c430);
	//pVEDR->vedr.dm.bioKsimap02 	= pReg_Src->vedr.dm.bioKsimap02; // 34
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap02.value, pVEDR+0x3c434);
	//pVEDR->vedr.dm.bioKsimap03  = pReg_Src->vedr.dm.bioKsimap03; // 38
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap03.value, pVEDR+0x3c438);
	//pVEDR->vedr.dm.bioKsimap04  = pReg_Src->vedr.dm.bioKsimap04; // 3c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap04.value, pVEDR+0x3c43c);
	//pVEDR->vedr.dm.bioKsimap05  = pReg_Src->vedr.dm.bioKsimap05; // 40
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap05.value, pVEDR+0x3c440);
	//pVEDR->vedr.dm.bioKsimap06  = pReg_Src->vedr.dm.bioKsimap06; // 44
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap06.value, pVEDR+0x3c444);
	//pVEDR->vedr.dm.bioKsimap07  = pReg_Src->vedr.dm.bioKsimap07; // 48
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap07.value, pVEDR+0x3c448);
	//pVEDR->vedr.dm.bioKsimap08  = pReg_Src->vedr.dm.bioKsimap08; // 4c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap08.value, pVEDR+0x3c44c);
	//pVEDR->vedr.dm.bioKsimap09  = pReg_Src->vedr.dm.bioKsimap09; // 50
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap09.value, pVEDR+0x3c450);
	//pVEDR->vedr.dm.bioKsimap10  = pReg_Src->vedr.dm.bioKsimap10; // 54
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap10.value, pVEDR+0x3c454);
	//pVEDR->vedr.dm.bioKsimap11  = pReg_Src->vedr.dm.bioKsimap11; // 58
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap11.value, pVEDR+0x3c458);
	//pVEDR->vedr.dm.bioKsimap12  = pReg_Src->vedr.dm.bioKsimap12; // 5c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap12.value, pVEDR+0x3c45c);
	//pVEDR->vedr.dm.bioKsimap13  = pReg_Src->vedr.dm.bioKsimap13; // 60
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap13.value, pVEDR+0x3c460);
	//pVEDR->vedr.dm.bioKsimap14  = pReg_Src->vedr.dm.bioKsimap14; // 64
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap14.value, pVEDR+0x3c464);
	//pVEDR->vedr.dm.bioKsimap15  = pReg_Src->vedr.dm.bioKsimap15; // 68
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap15.value, pVEDR+0x3c468);
	//pVEDR->vedr.dm.bioKsimap16  = pReg_Src->vedr.dm.bioKsimap16; // 6c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap16.value, pVEDR+0x3c46c);
	//pVEDR->vedr.dm.bioKsimap17  = pReg_Src->vedr.dm.bioKsimap17; // 70
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap17.value, pVEDR+0x3c470);
	//pVEDR->vedr.dm.bioKsimap18  = pReg_Src->vedr.dm.bioKsimap18; // 74
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap18.value, pVEDR+0x3c474);
	//pVEDR->vedr.dm.bioKsimap19  = pReg_Src->vedr.dm.bioKsimap19; // 78
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap19.value, pVEDR+0x3c478);
	//pVEDR->vedr.dm.bioKsimap20  = pReg_Src->vedr.dm.bioKsimap20; // 7c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap20.value, pVEDR+0x3c47c);
	//pVEDR->vedr.dm.bioKsimap21  = pReg_Src->vedr.dm.bioKsimap21; // 80
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap21.value, pVEDR+0x3c480);
	//pVEDR->vedr.dm.bioKsimap22  = pReg_Src->vedr.dm.bioKsimap22; // 84
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap22.value, pVEDR+0x3c484);
	//pVEDR->vedr.dm.bioKsimap23  = pReg_Src->vedr.dm.bioKsimap23; // 88
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap23.value, pVEDR+0x3c488);
	//pVEDR->vedr.dm.bioKsimap24  = pReg_Src->vedr.dm.bioKsimap24; // 8c
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap24.value, pVEDR+0x3c48c);
	//pVEDR->vedr.dm.bioKsimap25  = pReg_Src->vedr.dm.bioKsimap25; // 90
	__dv_reg_w(pReg_Src->vedr.dm.bioKsimap25.value, pVEDR+0x3c490);

#if !defined(SHADOW_CONTEXT_AT_THE_SAME_TIME)
	if (0){// frmcnt%2 == 0 ){
		//pVEDR->vedr.dm.bioCtrl1 	= pReg_Src->vedr.dm.bioCtrl1;    // 94
		__dv_reg_w(pReg_Src->vedr.dm.bioCtrl1.value, pVEDR+0x3c494);
	}
	else{
/*
		volatile union TccEdrDMBioCtrl1Reg ctrl1Reg;
		ctrl1Reg = pReg_Src->vedr.dm.bioCtrl1;
		ctrl1Reg.bits.ksdmctrlShadowcontext_icsc = bShadow_context;
		ctrl1Reg.bits.ksdmctrlShadowcontext_cvm = bShadow_context;
		//printk("shadow edr : 0x%x \n", ctrl1Reg.value);
		pVEDR->vedr.dm.bioCtrl1	= ctrl1Reg; 			// 94
*/
		value = pReg_Src->vedr.dm.bioCtrl1.value & ~((0x1<<1)|(0x1<<0));
		value |= ((bShadow_context<<1)|(bShadow_context<<0));
		//printk("shadow edr : 0x%x \n", value);
		__dv_reg_w(value, pVEDR+0x3c494);
	}
#endif

	//pVEDR->vedr.dm.unk1 		= pReg_Src->vedr.dm.unk1;        // c0
	__dv_reg_w(pReg_Src->vedr.dm.unk1.value, pVEDR+0x3c4c0);
	//pVEDR->vedr.dm.unk2 		= pReg_Src->vedr.dm.unk2;        // c4
	__dv_reg_w(pReg_Src->vedr.dm.unk2.value, pVEDR+0x3c4c4);
	//pVEDR->vedr.dm.unk3 		= pReg_Src->vedr.dm.unk3;        // c8
	__dv_reg_w(pReg_Src->vedr.dm.unk3.value, pVEDR+0x3c4c8);
	//pVEDR->vedr.dm.unk4 		= pReg_Src->vedr.dm.unk4;        // cc
	__dv_reg_w(pReg_Src->vedr.dm.unk4.value, pVEDR+0x3c4cc);
	//pVEDR->vedr.dm.unk5 		= pReg_Src->vedr.dm.unk5;        // d0
	__dv_reg_w(pReg_Src->vedr.dm.unk5.value, pVEDR+0x3c4d0);
	//pVEDR->vedr.dm.unk6 		= pReg_Src->vedr.dm.unk6;        // d4
	__dv_reg_w(pReg_Src->vedr.dm.unk6.value, pVEDR+0x3c4d4);
	//pVEDR->vedr.dm.unk7 		= pReg_Src->vedr.dm.unk7;        // d8
	__dv_reg_w(pReg_Src->vedr.dm.unk7.value, pVEDR+0x3c4d8);
	//pVEDR->vedr.dm.unk8 		= pReg_Src->vedr.dm.unk8;        // dc
	__dv_reg_w(pReg_Src->vedr.dm.unk8.value, pVEDR+0x3c4dc);

	__vioc_set_edr_specific(pReg_Src);						     // 0x1253C014 ~ 2C, 0x1253C4D8 ~ DC

}


void __vioc_set_panel_specific(struct TccEdrV1Reg *pReg_Src)
{
	volatile void __iomem *pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);
	unsigned int value = 0x00;

	value 	= pReg_Src->vpanel.osd1.unkimapg12.value;
	value	&= ~(0xFFFFFFFF); // 98
	value	|= ((VDE_VStart<<0)|(VDE_HStart<<16));
	//pVPANEL->osd1.unkimapg12.value	= value;
	__dv_reg_w(value, pVPANEL+0x0098);

	value 	= pReg_Src->vpanel.osd1.unkimapg13.value;
	value	&= ~(0xFFFFFFFF); // 9c
	value	|= ((Vactive<<0)|(Hactive<<16));
	//pVPANEL->osd1.unkimapg13.value	= value;
	__dv_reg_w(value, pVPANEL+0x009c);

	value 	= pReg_Src->vpanel.osd3.unkimapgb12.value;
	value	&= ~(0xFFFFFFFF); // 134
	value	|= ((VDE_VStart<<0)|(VDE_HStart<<16));
	//pVPANEL->osd3.unkimapgb12.value	= value;
	__dv_reg_w(value, pVPANEL+0x0134);

	value 	= pReg_Src->vpanel.osd3.unkimapgb13.value;
	value	&= ~(0xFFFFFFFF); // 138
	value	|= ((Vactive<<0)|(Hactive<<16));
	//pVPANEL->osd3.unkimapgb13.value	= value;
	__dv_reg_w(value, pVPANEL+0x0138);

	value 	= pReg_Src->vpanel.out.unkomap01.value;
	value   	&= ~(0xFFFFFFFF);    // 800
	value	|= (((Vsync+Vback+Vactive)<<0)|((Vsync+Vback)<<16));
	//pVPANEL->out.unkomap01.value	= value;
	__dv_reg_w(value, pVPANEL+0x0800);

	value 	= pReg_Src->vpanel.out.unkomap02.value;
	value	&= ~(0xFFFFFFFF);    // 804
	value	|= (((Hsync+Hback+Hactive)<<0)|((Hsync+Hback)<<16));
	//pVPANEL->out.unkomap02.value	= value;
	__dv_reg_w(value, pVPANEL+0x0804);

	value 	= pReg_Src->vpanel.out.unkomap03.value;
	value	&= ~(0xFFFFFFFF);    // 808
	value	|= (((Hactive+Hfront+Hsync+Hback-1)<<0)|((Vactive+Vfront+Vsync+Vback-1)<<16));
	//pVPANEL->out.unkomap03.value	= value;
	__dv_reg_w(value, pVPANEL+0x0808);

	value 	= pReg_Src->vpanel.out.unkomap04.value;
	value	&= ~(0x1FFF);    	  // 80C
	value	|= ((Hsync<<0)|((Vsync-1)<<8));
	//pVPANEL->out.unkomap04.value	= value;
	__dv_reg_w(value, pVPANEL+0x080C);

	value 	= pReg_Src->vpanel.out.unkomap05.value;
	value	|= 0x5;			  	  // 810
	//pVPANEL->out.unkomap05.value	= value;
	__dv_reg_w(value, pVPANEL+0x0810);
}

void _voic_set_panel(void __iomem *reg_VirtAddr, void __iomem *meta_VirtAddr, unsigned int frmcnt)
{
	volatile void __iomem *pVPANEL = NULL;
	struct TccEdrV1Reg *pReg_Src = (struct TccEdrV1Reg *)reg_VirtAddr;
	unsigned int value = 0x0;

	pVPANEL = (struct TccEdrPanelV1Reg *)VIOC_DV_VEDR_GetAddress(VPANEL);

//OSD1 :: 0x12540000
	//pVPANEL->osd1.unkimapg00	= pReg_Src->vpanel.osd1.unkimapg00; // 00
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg00.value, pVPANEL+0x0000);
    //pVPANEL->osd1.unkimapg01	= pReg_Src->vpanel.osd1.unkimapg01; // 04
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg01.value, pVPANEL+0x0004);
    //pVPANEL->osd1.unkimapg02	= pReg_Src->vpanel.osd1.unkimapg02; // 08
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg02.value, pVPANEL+0x0008);

	if((frmcnt == 0 || bMeta_changed) && (meta_VirtAddr != 0x00)){
		unsigned int osd3_blend = 0x1;
		unsigned int yuv444to422_blend = 0x1;

		value	= pReg_Src->vpanel.osd1.unkimapg03.value;
		if(frmcnt != 0)
			value	|= 0xFFFF; // soft reset of vpanel, low active
		value	|= (osd3_blend<<23);	// clk on for blend
		value	&= ~(osd3_blend<<22);	// clk bypass for blend
		value	|= (yuv444to422_blend<<21);	// clk on for 444
		value	&= ~(yuv444to422_blend<<20);// clk on for 444
		//pVPANEL->osd1.unkimapg03.value	= value; // 0c
		__dv_reg_w(value, pVPANEL+0x000c);
	}
	else{
		if(bMeta_changed){
			value	= pReg_Src->vpanel.osd1.unkimapg03.value;
			value	|= 0xFFFF; // no soft_reset
			//pVPANEL->osd1.unkimapg03.value = value; // 0c
			__dv_reg_w(value, pVPANEL+0x000c);
		}
	}

#if !defined(SHADOW_CONTEXT_AT_THE_SAME_TIME)
	if(0){//frmcnt%2 == 0){
		//pVPANEL->osd1.unkimapg04	= pReg_Src->vpanel.osd1.unkimapg04; // 10
		__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg04.value, pVPANEL+0x0010);
	}
	else {
/*
		volatile union TccEdrDMImapgUnknown04Reg unK04Reg;
		unK04Reg = pReg_Src->vpanel.osd1.unkimapg04;
		unK04Reg.bits.shadowContext_cvm_in = bShadow_context;
		unK04Reg.bits.shadowContext_icsc_in = bShadow_context;
*/
		value = pReg_Src->vpanel.osd1.unkimapg04.value & ~((0x1<<12)|(0x1<<13));
		value |= ((bShadow_context<<12)|(bShadow_context<<13));

		//printk("shadow osd1 : 0x%x \n", unK04Reg.value);

		//pVPANEL->osd1.unkimapg04	= unK04Reg; // 10
		__dv_reg_w(value, pVPANEL+0x0010);
	}
#endif

	if(frmcnt == 0){
    	//pVPANEL->osd1.unkimapg05	= pReg_Src->vpanel.osd1.unkimapg05; // 14
		__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg05.value, pVPANEL+0x0014);
	}

	value = pReg_Src->vpanel.osd1.bioKsimapg00.value;
	if(bOsd1_on)
		value	&=  ~(0x1<<29); // osd1 on
	else
		value	|=  (0x1<<29); // osd1 off	
	//pVPANEL->osd1.bioKsimapg00.value = value; 						 // 18
	__dv_reg_w(value, pVPANEL+0x0018);

	//pVPANEL->osd1.bioKsimapg01 = pReg_Src->vpanel.osd1.bioKsimapg01; // 1c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg01.value, pVPANEL+0x001c);
	//pVPANEL->osd1.bioKsimapg02 = pReg_Src->vpanel.osd1.bioKsimapg02; // 20
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg02.value, pVPANEL+0x0020);
	//pVPANEL->osd1.bioKsimapg03 = pReg_Src->vpanel.osd1.bioKsimapg03; // 24
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg03.value, pVPANEL+0x0024);
	//pVPANEL->osd1.bioKsimapg04 = pReg_Src->vpanel.osd1.bioKsimapg04; // 28
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg04.value, pVPANEL+0x0028);
	//pVPANEL->osd1.bioKsimapg05 = pReg_Src->vpanel.osd1.bioKsimapg05; // 2c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg05.value, pVPANEL+0x002c);
	//pVPANEL->osd1.bioKsimapg06 = pReg_Src->vpanel.osd1.bioKsimapg06; // 30
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg06.value, pVPANEL+0x0030);
	//pVPANEL->osd1.bioKsimapg07 = pReg_Src->vpanel.osd1.bioKsimapg07; // 34
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg07.value, pVPANEL+0x0034);
	//pVPANEL->osd1.bioKsimapg08 = pReg_Src->vpanel.osd1.bioKsimapg08; // 38
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg08.value, pVPANEL+0x0038);
	//pVPANEL->osd1.bioKsimapg09 = pReg_Src->vpanel.osd1.bioKsimapg09; // 3c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg09.value, pVPANEL+0x003c);
	//pVPANEL->osd1.bioKsimapg10 = pReg_Src->vpanel.osd1.bioKsimapg10; // 40
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg10.value, pVPANEL+0x0040);
	//pVPANEL->osd1.bioKsimapg11 = pReg_Src->vpanel.osd1.bioKsimapg11; // 44
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg11.value, pVPANEL+0x0044);
	//pVPANEL->osd1.bioKsimapg12 = pReg_Src->vpanel.osd1.bioKsimapg12; // 48
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg12.value, pVPANEL+0x0048);
	//pVPANEL->osd1.bioKsimapg13 = pReg_Src->vpanel.osd1.bioKsimapg13; // 4c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg13.value, pVPANEL+0x004c);
	//pVPANEL->osd1.bioKsimapg14 = pReg_Src->vpanel.osd1.bioKsimapg14; // 50
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg14.value, pVPANEL+0x0050);
	//pVPANEL->osd1.bioKsimapg15 = pReg_Src->vpanel.osd1.bioKsimapg15; // 54
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg15.value, pVPANEL+0x0054);
	//pVPANEL->osd1.bioKsimapg16 = pReg_Src->vpanel.osd1.bioKsimapg16; // 58
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg16.value, pVPANEL+0x0058);
	//pVPANEL->osd1.bioKsimapg17 = pReg_Src->vpanel.osd1.bioKsimapg17; // 5c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg17.value, pVPANEL+0x005c);
	//pVPANEL->osd1.bioKsimapg18 = pReg_Src->vpanel.osd1.bioKsimapg18; // 60
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg18.value, pVPANEL+0x0060);
	//pVPANEL->osd1.bioKsimapg19 = pReg_Src->vpanel.osd1.bioKsimapg19; // 64
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg19.value, pVPANEL+0x0064);
	//pVPANEL->osd1.bioKsimapg20 = pReg_Src->vpanel.osd1.bioKsimapg20; // 68
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg20.value, pVPANEL+0x0068);
	//pVPANEL->osd1.bioKsimapg21 = pReg_Src->vpanel.osd1.bioKsimapg21; // 6c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg21.value, pVPANEL+0x006c);
	//pVPANEL->osd1.bioKsimapg22 = pReg_Src->vpanel.osd1.bioKsimapg22; // 70
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg22.value, pVPANEL+0x0070);
	//pVPANEL->osd1.bioKsimapg23 = pReg_Src->vpanel.osd1.bioKsimapg23; // 74
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg23.value, pVPANEL+0x0074);
	//pVPANEL->osd1.bioKsimapg24 = pReg_Src->vpanel.osd1.bioKsimapg24; // 78
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg24.value, pVPANEL+0x0078);
	//pVPANEL->osd1.bioKsimapg25 = pReg_Src->vpanel.osd1.bioKsimapg25; // 7c
	__dv_reg_w(pReg_Src->vpanel.osd1.bioKsimapg25.value, pVPANEL+0x007c);
	//pVPANEL->osd1.unkimapg06   = pReg_Src->vpanel.osd1.unkimapg06; // 80
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg06.value, pVPANEL+0x0080);
	//pVPANEL->osd1.unkimapg07   = pReg_Src->vpanel.osd1.unkimapg07; // 84
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg07.value, pVPANEL+0x0084);
	//pVPANEL->osd1.unkimapg08   = pReg_Src->vpanel.osd1.unkimapg08; // 88
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg08.value, pVPANEL+0x0088);
	//pVPANEL->osd1.unkimapg09   = pReg_Src->vpanel.osd1.unkimapg09; // 8c
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg09.value, pVPANEL+0x008c);
	//pVPANEL->osd1.unkimapg10   = pReg_Src->vpanel.osd1.unkimapg10; // 90
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg10.value, pVPANEL+0x0090);
	//pVPANEL->osd1.unkimapg11   = pReg_Src->vpanel.osd1.unkimapg11; // 94
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg11.value, pVPANEL+0x0094);
	//pVPANEL->osd1.unkimapg12   = pReg_Src->vpanel.osd1.unkimapg12; // 98
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg12.value, pVPANEL+0x0098);
	//pVPANEL->osd1.unkimapg13   = pReg_Src->vpanel.osd1.unkimapg13; // 9c
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg13.value, pVPANEL+0x009c);
    //pVPANEL->osd1.unkimapg14   = pReg_Src->vpanel.osd1.unkimapg14; // a0
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg14.value, pVPANEL+0x00a0);
    //pVPANEL->osd1.unkimapg15   = pReg_Src->vpanel.osd1.unkimapg15; // a4
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg15.value, pVPANEL+0x00a4);
    //pVPANEL->osd1.unkimapg16   = pReg_Src->vpanel.osd1.unkimapg16; // a8
	__dv_reg_w(pReg_Src->vpanel.osd1.unkimapg16.value, pVPANEL+0x00a8);

#if !defined(SHADOW_CONTEXT_AT_THE_SAME_TIME)
//OSD3 :: 0x125400AC
	if(0){//frmcnt%2 == 0){
		//pVPANEL->osd3.unkimapgb04   = pReg_Src->vpanel.osd3.unkimapgb04; // ac
		__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb04.value, pVPANEL+0x00ac);	
	}
	else {
/*
		volatile union TccEdrDMImapgUnknown04Reg unK04Reg;
		unK04Reg = pReg_Src->vpanel.osd3.unkimapgb04;
		unK04Reg.bits.shadowContext_cvm_in = bShadow_context;
		unK04Reg.bits.shadowContext_icsc_in = bShadow_context;
		//printk("shadow osd3 : 0x%x \n", unK04Reg.value);
		pVPANEL->osd3.unkimapgb04	= unK04Reg; 						 // ac
*/
		value = pReg_Src->vpanel.osd3.unkimapgb04.value & ~((0x1<<12)|(0x1<<13));
		value |= ((bShadow_context<<12)|(bShadow_context<<13));
		__dv_reg_w(value, pVPANEL+0x00ac);
	}
#endif

	if(frmcnt == 0){
	    //pVPANEL->osd3.unkimapgb05   = pReg_Src->vpanel.osd3.unkimapgb05; // b0
		__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb05.value, pVPANEL+0x00b0);
	}

#if 1 // osd3 off //on in ff_player case!!
	{
		unsigned int value = pReg_Src->vpanel.osd3.bioKsimapgb00.value; // b4
		if(bOsd3_on)
			value	&=  ~(0x1<<29); // osd1 on
		else
			value	|=  (0x1<<29); // osd1 off
		//pVPANEL->osd3.bioKsimapgb00.value = value;
		__dv_reg_w(value, pVPANEL+0x00b4);
	}
#else
	//pVPANEL->osd3.bioKsimapgb00 = pReg_Src->vpanel.osd3.bioKsimapgb00; // b4
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb00.value, pVPANEL+0x00b4);
#endif
	//pVPANEL->osd3.bioKsimapgb01 = pReg_Src->vpanel.osd3.bioKsimapgb01; // b8
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb01.value, pVPANEL+0x00b8);
	//pVPANEL->osd3.bioKsimapgb02 = pReg_Src->vpanel.osd3.bioKsimapgb02; // bc
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb02.value, pVPANEL+0x00bc);
	//pVPANEL->osd3.bioKsimapgb03 = pReg_Src->vpanel.osd3.bioKsimapgb03; // c0
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb03.value, pVPANEL+0x00c0);
	//pVPANEL->osd3.bioKsimapgb04 = pReg_Src->vpanel.osd3.bioKsimapgb04; // c4
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb04.value, pVPANEL+0x00c4);
	//pVPANEL->osd3.bioKsimapgb05 = pReg_Src->vpanel.osd3.bioKsimapgb05; // c8
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb05.value, pVPANEL+0x00c8);
	//pVPANEL->osd3.bioKsimapgb06 = pReg_Src->vpanel.osd3.bioKsimapgb06; // cc
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb06.value, pVPANEL+0x00cc);
	//pVPANEL->osd3.bioKsimapgb07 = pReg_Src->vpanel.osd3.bioKsimapgb07; // d0
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb07.value, pVPANEL+0x00d0);
	//pVPANEL->osd3.bioKsimapgb08 = pReg_Src->vpanel.osd3.bioKsimapgb08; // d4
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb08.value, pVPANEL+0x00d4);
	//pVPANEL->osd3.bioKsimapgb09 = pReg_Src->vpanel.osd3.bioKsimapgb09; // d8
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb09.value, pVPANEL+0x00d8);
	//pVPANEL->osd3.bioKsimapgb10 = pReg_Src->vpanel.osd3.bioKsimapgb10; // dc
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb10.value, pVPANEL+0x00dc);
	//pVPANEL->osd3.bioKsimapgb11 = pReg_Src->vpanel.osd3.bioKsimapgb11; // e0
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb11.value, pVPANEL+0x00e0);
	//pVPANEL->osd3.bioKsimapgb12 = pReg_Src->vpanel.osd3.bioKsimapgb12; // e4
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb12.value, pVPANEL+0x00e4);
	//pVPANEL->osd3.bioKsimapgb13 = pReg_Src->vpanel.osd3.bioKsimapgb13; // e8
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb13.value, pVPANEL+0x00e8);
	//pVPANEL->osd3.bioKsimapgb14 = pReg_Src->vpanel.osd3.bioKsimapgb14; // ec
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb14.value, pVPANEL+0x00ec);
	//pVPANEL->osd3.bioKsimapgb15 = pReg_Src->vpanel.osd3.bioKsimapgb15; // f0
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb15.value, pVPANEL+0x00f0);
	//pVPANEL->osd3.bioKsimapgb16 = pReg_Src->vpanel.osd3.bioKsimapgb16; // f4
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb16.value, pVPANEL+0x00f4);
	//pVPANEL->osd3.bioKsimapgb17 = pReg_Src->vpanel.osd3.bioKsimapgb17; // f8
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb17.value, pVPANEL+0x00f8);
	//pVPANEL->osd3.bioKsimapgb18 = pReg_Src->vpanel.osd3.bioKsimapgb18; // fc
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb18.value, pVPANEL+0x00fc);
	//pVPANEL->osd3.bioKsimapgb19 = pReg_Src->vpanel.osd3.bioKsimapgb19; // 100
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb19.value, pVPANEL+0x0100);
	//pVPANEL->osd3.bioKsimapgb20 = pReg_Src->vpanel.osd3.bioKsimapgb20; // 104
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb20.value, pVPANEL+0x0104);
	//pVPANEL->osd3.bioKsimapgb21 = pReg_Src->vpanel.osd3.bioKsimapgb21; // 108
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb21.value, pVPANEL+0x0108);
	//pVPANEL->osd3.bioKsimapgb22 = pReg_Src->vpanel.osd3.bioKsimapgb22; // 10c
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb22.value, pVPANEL+0x010c);
	//pVPANEL->osd3.bioKsimapgb23 = pReg_Src->vpanel.osd3.bioKsimapgb23; // 110
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb23.value, pVPANEL+0x0110);
	//pVPANEL->osd3.bioKsimapgb24 = pReg_Src->vpanel.osd3.bioKsimapgb24; // 114
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb24.value, pVPANEL+0x0114);
	//pVPANEL->osd3.bioKsimapgb25 = pReg_Src->vpanel.osd3.bioKsimapgb25; // 118
	__dv_reg_w(pReg_Src->vpanel.osd3.bioKsimapgb25.value, pVPANEL+0x0118);
	//pVPANEL->osd3.unkimapgb06   = pReg_Src->vpanel.osd3.unkimapgb06; // 11c
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb06.value, pVPANEL+0x011c);
	//pVPANEL->osd3.unkimapgb07   = pReg_Src->vpanel.osd3.unkimapgb07; // 120
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb07.value, pVPANEL+0x0120);
	//pVPANEL->osd3.unkimapgb08   = pReg_Src->vpanel.osd3.unkimapgb08; // 124
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb08.value, pVPANEL+0x0124);
	//pVPANEL->osd3.unkimapgb09   = pReg_Src->vpanel.osd3.unkimapgb09; // 128
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb09.value, pVPANEL+0x0128);
	//pVPANEL->osd3.unkimapgb10   = pReg_Src->vpanel.osd3.unkimapgb10; // 12c
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb10.value, pVPANEL+0x012c);
	//pVPANEL->osd3.unkimapgb11   = pReg_Src->vpanel.osd3.unkimapgb11; // 130
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb11.value, pVPANEL+0x0130);
	//pVPANEL->osd3.unkimapgb12   = pReg_Src->vpanel.osd3.unkimapgb12; // 134
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb12.value, pVPANEL+0x0134);
	//pVPANEL->osd3.unkimapgb13   = pReg_Src->vpanel.osd3.unkimapgb13; // 138
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb13.value, pVPANEL+0x0138);
    //pVPANEL->osd3.unkimapgb14   = pReg_Src->vpanel.osd3.unkimapgb14; // 13c
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb14.value, pVPANEL+0x013c);
    //pVPANEL->osd3.unkimapgb15   = pReg_Src->vpanel.osd3.unkimapgb15; // 140
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb15.value, pVPANEL+0x0140);
    //pVPANEL->osd3.unkimapgb16   = pReg_Src->vpanel.osd3.unkimapgb16; // 144
	__dv_reg_w(pReg_Src->vpanel.osd3.unkimapgb16.value, pVPANEL+0x0144);

//OUT :: 0x12540148
	if(frmcnt == 0){
		//pVPANEL->out.unkomap00   = pReg_Src->vpanel.out.unkomap00;    // 148
		__dv_reg_w(pReg_Src->vpanel.out.unkomap00.value, pVPANEL+0x0148);
	}
	//pVPANEL->out.bioKsomap00 = pReg_Src->vpanel.out.bioKsomap00;    // 14c
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap00.value, pVPANEL+0x014c);
	//pVPANEL->out.bioKsomap01 = pReg_Src->vpanel.out.bioKsomap01;    // 150
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap01.value, pVPANEL+0x0150);
	//pVPANEL->out.bioKsomap02 = pReg_Src->vpanel.out.bioKsomap02;    // 154
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap02.value, pVPANEL+0x0154);
	//pVPANEL->out.bioKsomap03 = pReg_Src->vpanel.out.bioKsomap03;    // 158
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap03.value, pVPANEL+0x0158);
	//pVPANEL->out.bioKsomap04 = pReg_Src->vpanel.out.bioKsomap04;    // 15c
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap04.value, pVPANEL+0x015c);
	//pVPANEL->out.bioKsomap06 = pReg_Src->vpanel.out.bioKsomap06;    // 160
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap06.value, pVPANEL+0x0160);
	//pVPANEL->out.bioKsomap07 = pReg_Src->vpanel.out.bioKsomap07;    // 164
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap07.value, pVPANEL+0x0164);
	//pVPANEL->out.bioKsomap08 = pReg_Src->vpanel.out.bioKsomap08;    // 168
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap08.value, pVPANEL+0x0168);
	//pVPANEL->out.bioKsomap09 = pReg_Src->vpanel.out.bioKsomap09;    // 16c
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap09.value, pVPANEL+0x016c);
	//pVPANEL->out.bioKsomap10 = pReg_Src->vpanel.out.bioKsomap10;    // 170
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap10.value, pVPANEL+0x0170);
	//pVPANEL->out.bioKsomap11 = pReg_Src->vpanel.out.bioKsomap11;    // 174
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap11.value, pVPANEL+0x0174);
	//pVPANEL->out.bioKsomap12 = pReg_Src->vpanel.out.bioKsomap12;    // 178
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap12.value, pVPANEL+0x0178);
	//pVPANEL->out.bioKsomap13 = pReg_Src->vpanel.out.bioKsomap13;    // 17c
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap13.value, pVPANEL+0x017c);
	//pVPANEL->out.bioKsomap14 = pReg_Src->vpanel.out.bioKsomap14;    // 180
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap14.value, pVPANEL+0x0180);
	//pVPANEL->out.bioKsomap15 = pReg_Src->vpanel.out.bioKsomap15;    // 184
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap15.value, pVPANEL+0x0184);
	//pVPANEL->out.bioKsomap16 = pReg_Src->vpanel.out.bioKsomap16;    // 188
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap16.value, pVPANEL+0x0188);
	//pVPANEL->out.bioKsomap17 = pReg_Src->vpanel.out.bioKsomap17;    // 18c
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap17.value, pVPANEL+0x018c);
	//pVPANEL->out.bioKsomap18 = pReg_Src->vpanel.out.bioKsomap18;    // 190
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap18.value, pVPANEL+0x0190);
	//pVPANEL->out.bioKsomap19 = pReg_Src->vpanel.out.bioKsomap19;    // 194
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap19.value, pVPANEL+0x0194);
	//pVPANEL->out.bioKsomap20 = pReg_Src->vpanel.out.bioKsomap20;    // 198
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap20.value, pVPANEL+0x0198);
	//pVPANEL->out.bioKsomap21 = pReg_Src->vpanel.out.bioKsomap21;    // 19c
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap21.value, pVPANEL+0x019c);
	//pVPANEL->out.bioKsomap22 = pReg_Src->vpanel.out.bioKsomap22;    // 1a0
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap22.value, pVPANEL+0x01a0);
	//pVPANEL->out.bioKsomap23 = pReg_Src->vpanel.out.bioKsomap23;    // 1a4
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap23.value, pVPANEL+0x01a4);
	//pVPANEL->out.bioKsomap24 = pReg_Src->vpanel.out.bioKsomap24;    // 1a8
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap24.value, pVPANEL+0x01a8);
	//pVPANEL->out.bioKsomap25 = pReg_Src->vpanel.out.bioKsomap25;    // 1ac
	__dv_reg_w(pReg_Src->vpanel.out.bioKsomap25.value, pVPANEL+0x01ac);
	//pVPANEL->out.bioKsuds03  = pReg_Src->vpanel.out.bioKsuds03;    // 1b0
	__dv_reg_w(pReg_Src->vpanel.out.bioKsuds03.value, pVPANEL+0x01b0);
	//pVPANEL->out.bioKsuds12  = pReg_Src->vpanel.out.bioKsuds12;    // 1b4
	__dv_reg_w(pReg_Src->vpanel.out.bioKsuds12.value, pVPANEL+0x01b4);
	//pVPANEL->out.bioKsuds13  = pReg_Src->vpanel.out.bioKsuds13;    // 1b8
	__dv_reg_w(pReg_Src->vpanel.out.bioKsuds13.value, pVPANEL+0x01b8);
	//pVPANEL->out.bioKsuds14  = pReg_Src->vpanel.out.bioKsuds14;    // 1bc
	__dv_reg_w(pReg_Src->vpanel.out.bioKsuds14.value, pVPANEL+0x01bc);
	//pVPANEL->out.bioKsuds15  = pReg_Src->vpanel.out.bioKsuds15;    // 1d0
	__dv_reg_w(pReg_Src->vpanel.out.bioKsuds15.value, pVPANEL+0x01d0);

	value 	= pReg_Src->vpanel.out.bioKsuds01.value;
	if((frmcnt == 0 || bMeta_changed) && (meta_VirtAddr != 0x00)){
		value	|=  (0x1<<31); //enable vs load en for YUV444 -> YUV422
	}
	//pVPANEL->out.bioKsuds01.value	= value;    // 1d4
	__dv_reg_w(value, pVPANEL+0x01d4);

#if 1//!defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
	if(frmcnt == 0 || bMeta_changed)
#endif
	{
		value	= pReg_Src->vpanel.out.bioKsuds16.value;
		if((frmcnt == 0 || bMeta_changed) && (meta_VirtAddr != 0x00)){
			value	&= ~(0x1<<31); // meta scrambling bypass
			value	&= ~(0x1<<30); // uv reverse
			value	&= ~(0xFFFF<<0); // metadata length
			if(vioc_v_dv_get_mode() != DV_STD)
				value |= (0x1<<31);
			value	|= 	(((*(volatile unsigned char *)(meta_VirtAddr+3))<<8) | ((*(volatile unsigned char *)(meta_VirtAddr+4))<<0));
		}
		//pVPANEL->out.bioKsuds16.value	= value;		// 1d8
		__dv_reg_w(value, pVPANEL+0x01d8);
	}

#if 0 //just testing!!
	pVPANEL->out.rsvd1[0] = 0x03aa0255; // 1dc
	pVPANEL->out.rsvd1[1] = 0x03aa0255; // 1e0
	pVPANEL->out.rsvd1[2] = 0x03aa0255; // 1e4
	pVPANEL->out.rsvd1[3] = 0x03aa0001; // 1e8
	pVPANEL->out.rsvd1[4] = 0x0aaa0555; // 1ec
	pVPANEL->out.rsvd1[5] = 0x0aaa0aaa; // 1f0
	pVPANEL->out.rsvd1[6] = 0x05550555; // 1f4
	pVPANEL->out.rsvd1[7] = 0x7fffffff; // 1f8
	pVPANEL->out.rsvd1[8] = 0x3003aaaa; // 1fc
	pVPANEL->out.rsvd1[9] = 0x88884444; // 200
	pVPANEL->out.rsvd1[10] = 0x00040005; // 204
	pVPANEL->out.rsvd1[11] = 0x00060007; // 208
	pVPANEL->out.rsvd1[12] = 0x00080009; // 20c
	pVPANEL->out.rsvd1[13] = 0x00030001; // 210
#endif

//Telechips, Specific Register
	 __vioc_set_panel_specific(pReg_Src);
}

void _voic_set_panel_lut(void __iomem *reg_VirtAddr)
{
	int i = 0; 
	volatile void __iomem *pVPANEL_LUT = NULL;
	struct TccEdrV1Reg *pReg_Src = (struct TccEdrV1Reg *)reg_VirtAddr;

	pVPANEL_LUT = VIOC_DV_VEDR_GetAddress(VPANEL_LUT);

//OSD1 :: 0x12580000
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd1Lut.grG2llut[i] = pReg_Src->osd1Lut.grG2llut[i]; // 0x0000
		__dv_reg_w(pReg_Src->osd1Lut.grG2llut[i].value, pVPANEL_LUT+0x0000+(0x4*i));
	}

	for(i=0; i<1024; i++){
		//pVPANEL_LUT->osd1Lut.pq2llut[i] = pReg_Src->osd1Lut.pq2llut[i]; // 0x1000
		__dv_reg_w(pReg_Src->osd1Lut.pq2llut[i].value, pVPANEL_LUT+0x1000+(0x4*i));
	}

	for(i=0; i<128; i++){
		//pVPANEL_LUT->osd1Lut.l2pqlutx[i] = pReg_Src->osd1Lut.l2pqlutx[i]; // 0x2000
		__dv_reg_w(pReg_Src->osd1Lut.l2pqlutx[i].value, pVPANEL_LUT+0x2000+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->osd1Lut.l2pqluta[i] = pReg_Src->osd1Lut.l2pqluta[i]; // 0x2200
		__dv_reg_w(pReg_Src->osd1Lut.l2pqluta[i].value, pVPANEL_LUT+0x2200+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->osd1Lut.l2pqlutb[i] = pReg_Src->osd1Lut.l2pqlutb[i]; // 0x2400
		__dv_reg_w(pReg_Src->osd1Lut.l2pqlutb[i].value, pVPANEL_LUT+0x2400+(0x4*i));
	}

	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd1Lut.grTmluti[i] = pReg_Src->osd1Lut.grTmluti[i]; // 0x3000
		__dv_reg_w(pReg_Src->osd1Lut.grTmluti[i].value, pVPANEL_LUT+0x3000+(0x4*i));
	}
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd1Lut.grSmluti[i] = pReg_Src->osd1Lut.grSmluti[i]; // 0x3400
		__dv_reg_w(pReg_Src->osd1Lut.grSmluti[i].value, pVPANEL_LUT+0x3400+(0x4*i));
	}
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd1Lut.grTmluts[i] = pReg_Src->osd1Lut.grTmluts[i]; // 0x3800
		__dv_reg_w(pReg_Src->osd1Lut.grTmluts[i].value, pVPANEL_LUT+0x3800+(0x4*i));
	}
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd1Lut.grSmluts[i] = pReg_Src->osd1Lut.grSmluts[i]; // 0x3c00
		__dv_reg_w(pReg_Src->osd1Lut.grSmluts[i].value, pVPANEL_LUT+0x3c00+(0x4*i));
	}

//OSD3 :: 0x12584000
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd3Lut.grG2llut[i] = pReg_Src->osd3Lut.grG2llut[i]; // 0x0000
		__dv_reg_w(pReg_Src->osd3Lut.grG2llut[i].value, pVPANEL_LUT+0x4000+0x0000+(0x4*i));
	}

	for(i=0; i<1024; i++){
		//pVPANEL_LUT->osd3Lut.pq2llut[i] = pReg_Src->osd3Lut.pq2llut[i]; // 0x1000
		__dv_reg_w(pReg_Src->osd3Lut.pq2llut[i].value, pVPANEL_LUT+0x4000+0x1000+(0x4*i));
	}

	for(i=0; i<128; i++){
		//pVPANEL_LUT->osd3Lut.l2pqlutx[i] = pReg_Src->osd3Lut.l2pqlutx[i]; // 0x2000
		__dv_reg_w(pReg_Src->osd3Lut.l2pqlutx[i].value, pVPANEL_LUT+0x4000+0x2000+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->osd3Lut.l2pqluta[i] = pReg_Src->osd3Lut.l2pqluta[i]; // 0x2200
		__dv_reg_w(pReg_Src->osd3Lut.l2pqluta[i].value, pVPANEL_LUT+0x4000+0x2200+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->osd3Lut.l2pqlutb[i] = pReg_Src->osd3Lut.l2pqlutb[i]; // 0x2400
		__dv_reg_w(pReg_Src->osd3Lut.l2pqlutb[i].value, pVPANEL_LUT+0x4000+0x2400+(0x4*i));
	}

	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd3Lut.grTmluti[i] = pReg_Src->osd3Lut.grTmluti[i]; // 0x3000
		__dv_reg_w(pReg_Src->osd3Lut.grTmluti[i].value, pVPANEL_LUT+0x4000+0x3000+(0x4*i));
	}
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd3Lut.grSmluti[i] = pReg_Src->osd3Lut.grSmluti[i]; // 0x3400
		__dv_reg_w(pReg_Src->osd3Lut.grSmluti[i].value, pVPANEL_LUT+0x4000+0x3400+(0x4*i));
	}
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd3Lut.grTmluts[i] = pReg_Src->osd3Lut.grTmluts[i]; // 0x3800
		__dv_reg_w(pReg_Src->osd3Lut.grTmluts[i].value, pVPANEL_LUT+0x4000+0x3800+(0x4*i));
	}
	for(i=0; i<256; i++){
		//pVPANEL_LUT->osd3Lut.grSmluts[i] = pReg_Src->osd3Lut.grSmluts[i]; // 0x3c00
		__dv_reg_w(pReg_Src->osd3Lut.grSmluts[i].value, pVPANEL_LUT+0x4000+0x3c00+(0x4*i));
	}

//Out :: 0x12588000
	for(i=0; i<128; i++){
		//pVPANEL_LUT->outLut.l2glutx[i] =  pReg_Src->outLut.l2glutx[i];  // 0x0000
		__dv_reg_w(pReg_Src->outLut.l2glutx[i].value, pVPANEL_LUT+0x8000+0x0000+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->outLut.l2gluta[i] =  pReg_Src->outLut.l2gluta[i];  // 0x0200
		__dv_reg_w(pReg_Src->outLut.l2gluta[i].value, pVPANEL_LUT+0x8000+0x0200+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->outLut.l2glutb[i] =  pReg_Src->outLut.l2glutb[i];  // 0x0400
		__dv_reg_w(pReg_Src->outLut.l2glutb[i].value, pVPANEL_LUT+0x8000+0x0400+(0x4*i));
	}

	for(i=0; i<1024; i++){
		//pVPANEL_LUT->outLut.pq2llut[i] = pReg_Src->outLut.pq2llut[i]; // 0x1000
		__dv_reg_w(pReg_Src->outLut.pq2llut[i].value, pVPANEL_LUT+0x8000+0x1000+(0x4*i));
	}

	for(i=0; i<128; i++){
		//pVPANEL_LUT->outLut.l2pqlutx[i] = pReg_Src->outLut.l2pqlutx[i]; // 0x2000
		__dv_reg_w(pReg_Src->outLut.l2pqlutx[i].value, pVPANEL_LUT+0x8000+0x2000+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->outLut.l2pqluta[i] = pReg_Src->outLut.l2pqluta[i]; // 0x2200
		__dv_reg_w(pReg_Src->outLut.l2pqluta[i].value, pVPANEL_LUT+0x8000+0x2200+(0x4*i));
	}
	for(i=0; i<128; i++){
		//pVPANEL_LUT->outLut.l2pqlutb[i] = pReg_Src->outLut.l2pqlutb[i]; // 0x2400
		__dv_reg_w(pReg_Src->outLut.l2pqlutb[i].value, pVPANEL_LUT+0x8000+0x2400+(0x4*i));
	}
}

void _voic_set_metadata(unsigned int meta_PhyAddr, void __iomem *meta_VirtAddr, void __iomem *reg_VirtAddr, unsigned int frmcnt)
{
	volatile void __iomem *pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);
//	struct TccEdrV1Reg *pReg_Src = (struct TccEdrV1Reg *)reg_VirtAddr;
	unsigned int value;

    if (meta_VirtAddr != 0x00) {
		//value	= pVPANEL->out.bioKsuds16.value;
		value   = __dv_reg_r(pVPANEL+0x01d8);
		value	&= ~(0xFFFF<<0);
		value	|= 	(((*(volatile unsigned char *)(meta_VirtAddr+3))<<8) | ((*(volatile unsigned char *)(meta_VirtAddr+4))<<0));
		//pVPANEL->out.bioKsuds16.value	= value;
		__dv_reg_w(value, pVPANEL+0x01d8);
		dprintk("md len : 0x%x \n", value);

        VIOC_CONFIG_DV_Metadata_Enable(meta_PhyAddr, 0);
    }
	else {
		if(bMeta_changed)
			VIOC_CONFIG_DV_Metadata_Disable();
	}
}

char vioc_v_dv_get_sc(void)
{
	return bShadow_context;
}

void vioc_v_dv_block_off(void)
{
	volatile void __iomem *pVEDR = NULL;
	unsigned int value = 0x00;

	if(!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	dprintk("%s-%d\n", __func__, __LINE__);

	pVEDR = VIOC_DV_VEDR_GetAddress(VEDR);
	//value = pVEDR->vedr.core.glb0.value;
	value = __dv_reg_r(pVEDR+0x3c000);

	value &= ~(0x3FF);
	//pVEDR->vedr.core.glb0.value = value;
	__dv_reg_w(value, pVEDR+0x3c000);	
}

void voic_v_dv_osd_ctrl(DV_DISP_TYPE type, unsigned int on)
{
	volatile void __iomem *pVPANEL = NULL;
	unsigned int value;

	if(!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	if(bStart_config)
	{
//		dprintk("%s-%d\n", __func__, __LINE__);

		pVPANEL = VIOC_DV_VEDR_GetAddress(VPANEL);
		if(type == EDR_OSD1)
		{
			//value = pVPANEL->osd1.bioKsimapg00.value;
			value = __dv_reg_r(pVPANEL+0x0018);
			if(on)
				value	&=  ~(0x1<<29); // osd1 on
			else
				value	|=  (0x1<<29); // osd1 off
			//pVPANEL->osd1.bioKsimapg00.value = value;					 // 18
			__dv_reg_w(value, pVPANEL+0x0018);
			bOsd1_on = on;
		}
		else
		{
			//value = pVPANEL->osd3.bioKsimapgb00.value;
			value = __dv_reg_r(pVPANEL+0x00b4);
			if(on)
				value	&=  ~(0x1<<29); // osd3 on
			else
				value	|=  (0x1<<29); // osd3 off
			//pVPANEL->osd3.bioKsimapgb00.value = value;					 // b4
			__dv_reg_w(value, pVPANEL+0x00b4);
			bOsd3_on = on;
		}
	}
}

void vioc_v_dv_el_bypass(void)
{
	volatile void __iomem *pVEDR = NULL;

	unsigned int value;

	if(!VIOC_CONFIG_DV_GET_EDR_PATH())
		return;

	if(bStart_config)
	{
		pVEDR = (struct TccEdrReg *)VIOC_DV_VEDR_GetAddress(VEDR);
		//value 	= pVEDR->vedr.core.comp0.value;
		value 	= __dv_reg_r(pVEDR+0x3c100);
		value	|= (0x4); // 100
		//pVEDR->vedr.core.comp0.value	= value;
		__dv_reg_w(value, pVEDR+0x3c100);
	}
}

#if defined(SHADOW_CONTEXT_AT_THE_SAME_TIME)
static void _vioc_v_dv_set_shadow_context(void __iomem *reg_VirtAddr)
{
	volatile void __iomem *pVPANEL, *pVEDR;
	unsigned int value = 0x0;
	struct TccEdrV1Reg *pReg_Src = (struct TccEdrV1Reg *)reg_VirtAddr;

	pVEDR = (struct TccEdrPanelV1Reg *)VIOC_DV_VEDR_GetAddress(VEDR);
	pVPANEL = (struct TccEdrPanelV1Reg *)VIOC_DV_VEDR_GetAddress(VPANEL);

	if(bOsd1_on){
		value = pReg_Src->vpanel.osd1.unkimapg04.value & ~((0x1<<12)|(0x1<<13));
		value |= ((bShadow_context<<12)|(bShadow_context<<13));
		__dv_reg_w(value, pVPANEL+0x0010);
	}
	if(bOsd3_on){
		value = pReg_Src->vpanel.osd3.unkimapgb04.value & ~((0x1<<12)|(0x1<<13));
		value |= ((bShadow_context<<12)|(bShadow_context<<13));
		__dv_reg_w(value, pVPANEL+0x00ac);
	}

	value = pReg_Src->vedr.dm.bioCtrl1.value & ~((0x1<<1)|(0x1<<0));
	value |= ((bShadow_context<<1)|(bShadow_context<<0));
	__dv_reg_w(value, pVEDR+0x3c494);

}
#endif

int vioc_v_dv_prog(unsigned int meta_PhyAddr, unsigned int reg_PhyAddr, unsigned int video_attribute, unsigned int frmcnt)
{
	void __iomem * meta_VirtAddr = 0x00;
	void __iomem * reg_VirtAddr = 0x00;
	char bMeta_CurrStatus = 0;

	if(frmcnt == 0)
	{
		dprintk("%s-%d :: restart DV\n", __func__, __LINE__);
		bOsd1_on = (RDMA_FB == EDR_OSD1) ? 1 : 0;
		bOsd3_on = (RDMA_FB == EDR_OSD3) ? 1 : 0;
		bStart_config = 0;
		bShadow_context = 1;
		bMeta_PrevStatus = bMeta_changed = 0;
	}

	if(reg_PhyAddr == 0x00){
		pr_err("reg_PhyAddr is NULL \n");
		return -1;
	}

	reg_VirtAddr = _get_virtual_address(reg_PhyAddr);
	if(reg_VirtAddr == 0x00){
		pr_err("reg_VirtAddr is NULL \n");
		return -1;
	}

	if( ((DOVI == vioc_get_out_type()) || (DOVI_LL == vioc_get_out_type())) && meta_PhyAddr == 0x00) {
		pr_err("meta_PhyAddr is NULL \n");
		return -1;
	}

	if(meta_PhyAddr != 0x00){
		meta_VirtAddr = _get_virtual_address(meta_PhyAddr);
		if(meta_VirtAddr == 0x00){
			pr_err("meta_VirtAddr is NULL \n");
			return -1;
		}
		bMeta_CurrStatus = 1;
	}

	if(!VIOC_CONFIG_DV_GET_EDR_PATH())
		return -1;

	if( frmcnt != 0 )
		video_attr = (VIDEO_ATTR)video_attribute;

//	dprintk("%s-%d\n", __func__, __LINE__);

	if((bMeta_CurrStatus != bMeta_PrevStatus) || !frmcnt)
		bMeta_changed = 1;
	else
		bMeta_changed = 0;

    _vioc_v_dv_prog_start();

	_voic_set_edr(reg_VirtAddr, frmcnt);
	
	_voic_set_edr_lut(reg_VirtAddr);

	_voic_set_panel(reg_VirtAddr, meta_VirtAddr, frmcnt);
	
	_voic_set_panel_lut(reg_VirtAddr);

#if defined(SHADOW_CONTEXT_AT_THE_SAME_TIME)
	_vioc_v_dv_set_shadow_context(reg_VirtAddr);
#endif

	_voic_set_metadata(meta_PhyAddr, meta_VirtAddr, reg_VirtAddr, frmcnt);

    if ( frmcnt == 0 )
		_void_reset_edr_compnent(0, 0, 0);

    if ( frmcnt == 0 )
		_vioc_v_dv_prog_1st_done();

    _vioc_v_dv_prog_done();

	dprintk("%s :: context(%d), meta_changed(%d) =>  reg(0x%x) meta(0x%x) \n", __func__, bShadow_context, bMeta_changed, reg_PhyAddr, meta_PhyAddr);

	bShadow_context = bShadow_context^1;
    bStart_config = 1;

	bMeta_PrevStatus = bMeta_CurrStatus;
	bMeta_changed = 0;

#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
	tca_edr_inc_check_count(0, 0, 0, 1, 0);
#endif

	return 0;
}

static int __init _vioc_v_dv_init(void)
{
	pmap_get_info("dolby_regs", &pmap_dv_regs);

	pBase_vAddr = ioremap_nocache((unsigned int)pmap_dv_regs.base, PAGE_ALIGN(pmap_dv_regs.size));
	if(pBase_vAddr == NULL){
		pr_err("Regs ioremap failed \n");
	}

	pr_info("Pmap for Dolby :: Phy(0x%x - 0x%x) => Virt(0x%p) \n", (unsigned int)pmap_dv_regs.base, pmap_dv_regs.size, pBase_vAddr);

	return 0;
}
arch_initcall(_vioc_v_dv_init);
#endif
