/****************************************************************************
Copyright (C) 2018 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/clk-provider.h>

#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/delay.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#include <asm/gpio.h>
#endif

#include <soc/tcc/pmap.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_ddicfg.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_composite_ioctl.h>
#include <video/tcc/tcc_component_ioctl.h>
#include <video/tcc/tcc_scaler_ctrl.h>


#if defined(CONFIG_TCC_HDMI_DRIVER_V1_4)
#include "output_starter_hdmi_v1_4.h"
#endif
#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
#include "output_starter_hdmi.h"
#endif


#include <vioc/tcc_component.h>
#include <video/tcc/vioc_tve.h>

/* Debugging stuff */
static int debug = 0;
#define DPRINTF(msg...)	if (debug) { printk("[DBG][O_STARTER] " msg); }

enum
{
        STARTER_LCDC_0 = 0,
        STARTER_LCDC_1,
        STARTER_LCDC_MAX
};

enum
{
        STARTER_OUTPUT_LCD = 0,
        STARTER_OUTPUT_HDMI,
        STARTER_OUTPUT_COMPOSITE,
        STARTER_OUTPUT_COMPONENT,
        STARTER_OUTPUT_MAX
};

enum
{
        STARTER_HDMI_1920x1080P_60Hz = 0,
        STARTER_HDMI_1920x1080P_50Hz = 1,
        STARTER_HDMI_1920x1080I_60Hz = 2,
        STARTER_HDMI_1920x1080I_50Hz = 3,
        STARTER_HDMI_1280x720P_60Hz = 4,
        STARTER_HDMI_1280x720P_50Hz = 5,
        STARTER_HDMI_720x576P_50Hz = 6,
        STARTER_HDMI_720x480P_60Hz = 7,
        STARTER_HDMI_640x480P_60Hz = 8,
        STARTER_HDMI_1920x720P_60Hz = 9,
        STARTER_HDMI_MAX
};

enum
{
        STARTER_COMPOSITE_NTSC = 0,
        STARTER_COMPOSITE_PAL,
        STARTER_COMPOSITE_MAX
};

enum
{
        STARTER_COMPONENT_480I_NTSC = 0,
        STARTER_COMPONENT_576I_PAL,
        STARTER_COMPONENT_720P,
        STARTER_COMPONENT_1080I,
        STARTER_COMPONENT_MAX
};

#if defined(CONFIG_TCC_DISPMG)
extern int set_persist_display_mode(int persist_display_mode);
#endif
#if defined(CONFIG_FB_TCC_COMPOSITE)
extern void tcc_composite_get_spec(enum COMPOSITE_MODE_TYPE mode, struct COMPOSITE_SPEC_TYPE *spec);
extern void tcc_composite_attach(char lcdc_num, char mode, char starter_flag);
extern int tcc_composite_connect_lcdc(int lcdc_num, int enable);
#endif
#if defined(CONFIG_FB_TCC_COMPONENT)
extern void tcc_component_get_spec(enum COMPONENT_MODE_TYPE mode,
	struct COMPONENT_SPEC_TYPE *spec);
#endif
#if defined(CONFIG_FB_TCC_COMPONENT_THS8200)
extern void ths8200_enable(int mode, int starter_flag);
#endif
#if defined(CONFIG_OUTPUT_SKIP_KERNEL_LOGO)
extern char* boot_recovery_mode;
#endif
extern void tccfb_output_starter(char output_type, char lcdc_num, stLTIMING *pstTiming, stLCDCTR *pstCtrl, int specific_pclk);
extern void vioc_reset_rdma_on_display_path(int DispNum);

static char default_composite_resolution = STARTER_COMPOSITE_NTSC;
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC901X)
char default_component_resolution = STARTER_COMPONENT_1080I;
#else
char default_component_resolution = STARTER_COMPONENT_720P;
#endif
EXPORT_SYMBOL(default_component_resolution);

static struct pmap pmap_fb;
static struct pmap pmap_attach;

static int persist_display_mode = -1;
static int output_starter_display_mode = 0;
static int Output_Starter_LCDC_Num;
static volatile void __iomem *pOutput_Starter_DISP;
static volatile void __iomem *pOutput_Starter_WMIX;
static volatile void __iomem *pOutput_Starter_RDMA;
#if defined(CONFIG_FB_TCC_COMPONENT)
static volatile void __iomem *pOutput_Starter_SCALER;
#endif
static volatile void __iomem *pOutput_Starter_DDICFG;

#define BOOTLOADER_FB_WIDTH (1280)	// hdmi width in bootloader
#define BOOTLOADER_FB_HEIGHT (720)	// hdmi height in bootloader


void vioc_sub_disp_composite_disable(int disp_num)
{
	volatile void __iomem *pDISP = VIOC_DISP_GetAddress(disp_num);
	volatile void __iomem *pDDICfg = pOutput_Starter_DDICFG;

	/*
	 * Disable sub disp block
	 */
	VIOC_DISP_TurnOff(pDISP);
	DPRINTF("[%s] VIOC_DISP_TurnOff(%d)\n", __func__, disp_num);

	#if defined(CONFIG_FB_TCC_COMPOSITE)
	/*
	 * Disconnect tvo/bvo & disable clk, if composite was enabled.
	 */
	VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_SDVENC, disp_num);
	VIOC_DDICONFIG_NTSCPAL_SetEnable(pDDICfg, 0, disp_num);
	VIOC_DDICONFIG_SetPWDN(pDDICfg, DDICFG_TYPE_NTSCPAL, 0);
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	VIOC_DDICONFIG_DAC_PWDN_Control(pDDICfg, 0);	
	#endif
	DPRINTF("[%s] Disable composite output\n", __func__);
	#endif

	vioc_reset_rdma_on_display_path(disp_num);
}

void tcc_output_starter_memclr(int img_width, int img_height)
{
	unsigned char *pBaseAddr;
	unsigned int img_fmt_factor;
	volatile void __iomem *pRDMA = pOutput_Starter_RDMA;

	img_fmt_factor = 4;	// The image format is supposed to be TCC_LCDC_IMG_FMT_RGB56

	if (pmap_fb.base) {
		pBaseAddr = (unsigned char *)ioremap_nocache(pmap_fb.base, pmap_fb.size);
		if (pBaseAddr) {
			memset_io(pBaseAddr, 0x00, pmap_fb.size);
 			iounmap(pBaseAddr);
			VIOC_RDMA_SetImageBase(pRDMA, pmap_fb.base + (img_width*img_height*img_fmt_factor), 0, 0);
		} else {
			// Force Code.!!
			VIOC_RDMA_SetImageBase(pRDMA, pmap_fb.base + (img_width*img_height*img_fmt_factor), 0, 0);
		}
		//pr_debug("[DBG][O_STARTER] %s fb_paddr=0x%08x fb_laddr=0x%08x\n", __func__, pmap_fb.base, (unsigned int)pBaseAddr);
	}

	if (pmap_attach.base) {
		pBaseAddr = (unsigned char *)ioremap_nocache(pmap_attach.base, pmap_attach.size/2);
		if (pBaseAddr) {
			memset_io(pBaseAddr, 0x00, pmap_attach.size/2);
 			iounmap(pBaseAddr);
		}
		//pr_debug("[DBG][O_STARTER] %s attach_paddr=0x%08x attach_laddr=0x%08x\n", __func__, pmap_attach.base, (unsigned int)pBaseAddr);
	}
}

void tcc_output_starter_composite(unsigned char lcdc_num, unsigned char type, struct platform_device *pdev)
{
#ifdef CONFIG_FB_TCC_COMPOSITE
	enum COMPOSITE_MODE_TYPE mode;
	struct COMPOSITE_SPEC_TYPE spec;
	stLTIMING CompositeTiming;
	stLCDCTR LcdCtrlParam;
	volatile void __iomem *pDISP = pOutput_Starter_DISP;
	volatile void __iomem *pRDMA = pOutput_Starter_RDMA;
	volatile void __iomem *pDDICfg = pOutput_Starter_DDICFG;

	unsigned int clock_rate;
	struct clk *clock;
	struct device_node *np_parent = pdev->dev.of_node;
	struct device_node *np_child;

	pr_info("[INF][O_STARTER] %s, lcdc_num=%d(%d), type=%d\n", __func__, Output_Starter_LCDC_Num, lcdc_num, type);

	if (type >= STARTER_COMPOSITE_MAX)
		type = default_composite_resolution;

	if (type == STARTER_COMPOSITE_NTSC)
		mode = NTSC_M;
	else
		mode = PAL_B;

	tcc_composite_get_spec(mode, &spec);

	if (Output_Starter_LCDC_Num)
		VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_SDVENC, VIOC_OUTCFG_DISP1);
	else
		VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_SDVENC, VIOC_OUTCFG_DISP0);

	internal_tve_clock_onoff(1);

	VIOC_DDICONFIG_SetPWDN(pDDICfg, DDICFG_TYPE_NTSCPAL, 1);
	VIOC_DDICONFIG_SetSWRESET(pDDICfg, DDICFG_TYPE_NTSCPAL, 0);
	VIOC_DDICONFIG_SetSWRESET(pDDICfg, DDICFG_TYPE_NTSCPAL, 1);

	VIOC_DDICONFIG_NTSCPAL_SetEnable(pDDICfg, 1, lcdc_num);

	np_child = of_get_child_by_name(np_parent, "composite_starter");

	/* set and enable vioc clock */
	if(Output_Starter_LCDC_Num)
		clock = of_clk_get_by_name(np_child, "lcdc1-clk");
	else
		clock = of_clk_get_by_name(np_child, "lcdc0-clk");

	/* disp pclk */
	clock_rate = spec.composite_clk;
	clk_set_rate(clock, clock_rate);
	clk_prepare_enable(clock);

	VIOC_DISP_TurnOff(pDISP);
	VIOC_RDMA_SetImageDisable(pRDMA);

	tcc_output_starter_memclr(BOOTLOADER_FB_WIDTH, BOOTLOADER_FB_HEIGHT);

	CompositeTiming.lpw = spec.composite_LPW;
	CompositeTiming.lpc = spec.composite_LPC + 1;
	CompositeTiming.lswc = spec.composite_LSWC + 1;
	CompositeTiming.lewc = spec.composite_LEWC + 1;

	CompositeTiming.vdb = spec.composite_VDB;
	CompositeTiming.vdf = spec.composite_VDF;
	CompositeTiming.fpw = spec.composite_FPW1;
	CompositeTiming.flc = spec.composite_FLC1;
	CompositeTiming.fswc = spec.composite_FSWC1;
	CompositeTiming.fewc = spec.composite_FEWC1;
	CompositeTiming.fpw2 = spec.composite_FPW2;
	CompositeTiming.flc2 = spec.composite_FLC2;
	CompositeTiming.fswc2 = spec.composite_FSWC2;
	CompositeTiming.fewc2 = spec.composite_FEWC2;

	memset((stLCDCTR *)&LcdCtrlParam, 0x00, sizeof(stLCDCTR));
#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
	//LcdCtrlParam.evp = 0;
	//LcdCtrlParam.evs = 0;
	//LcdCtrlParam.ckg = 0;
	//sreq; // force set 0x1
	//srst; // force set 0x1
	//clen;
	LcdCtrlParam.ccir656 = 0;
	LcdCtrlParam.y2r = 0;
	LcdCtrlParam.r2y = 1;
	LcdCtrlParam.r2ymd = 0;
	LcdCtrlParam.advi = 1;
	LcdCtrlParam.id = 0;
	LcdCtrlParam.pxdw = spec.pxdw;
	LcdCtrlParam.iv = spec.iv;
	LcdCtrlParam.ih = spec.ih;
	LcdCtrlParam.ip = spec.ip;
	LcdCtrlParam.dp = spec.dp;
	LcdCtrlParam.ni = spec.ni;
	LcdCtrlParam.tv = spec.tv;
#else
	LcdCtrlParam.r2ymd = 3;
	LcdCtrlParam.ckg = 1;
	LcdCtrlParam.id= 0;
	LcdCtrlParam.iv = 0;
	LcdCtrlParam.ih = 1;
	LcdCtrlParam.ip = 1;
	LcdCtrlParam.clen = 1;
	LcdCtrlParam.r2y = 1;
	LcdCtrlParam.pxdw = 6;
	LcdCtrlParam.dp = 0;
	LcdCtrlParam.ni = 0;
	LcdCtrlParam.tv = 1;
	#ifdef CONFIG_ARCH_TCC898X
	LcdCtrlParam.advi = 0;		// 0 is enable (only tcc898x)
	#else
	LcdCtrlParam.advi = 1;		// 1 is enable
	#endif
#endif

	tccfb_output_starter(TCC_OUTPUT_COMPOSITE, Output_Starter_LCDC_Num, &CompositeTiming, &LcdCtrlParam, spec.composite_clk);

	internal_tve_set_config(mode);

	VIOC_DISP_TurnOn(pDISP);
#endif
}

void tcc_output_starter_component(unsigned char lcdc_num, unsigned char type, struct platform_device *pdev)
{
#ifdef CONFIG_FB_TCC_COMPONENT
	int mode;
	int component_io_port_num, ret;
	struct device_node *np_component;

	struct COMPONENT_SPEC_TYPE component_spec;
	stLTIMING ComponentTiming;
	stLCDCTR LcdCtrlParam;

	volatile void __iomem *pDISP;
	volatile void __iomem *pWMIX;
	volatile void __iomem *pRDMA;
	volatile void __iomem *pSC;

	unsigned int clock_rate;
	struct clk *clock;
	struct device_node *np_parent = pdev->dev.of_node;
	struct device_node *np_child;

	unsigned int align_swap;
	align_swap = 0;

	pr_info("[INF][O_STARTER] %s, lcdc_num=%d, type=%d\n", __func__, Output_Starter_LCDC_Num, type);

	if(type >= STARTER_COMPONENT_MAX)
		type = default_component_resolution;

	switch (type) {
	case STARTER_COMPONENT_1080I:
		mode = COMPONENT_MODE_1080I;
		break;
	case STARTER_COMPONENT_720P:
	default:
		mode = COMPONENT_MODE_720P;
		break;
	}
	tcc_component_get_spec(mode, &component_spec);

	/*
	 * Set LCD IO interface ports for the Component output
	 * - CFG_MISC1 LCD Path Select
	 */
	np_component = of_find_compatible_node(NULL, NULL, "telechips,tcc-component");
	of_property_read_u32(np_component, "io_port_num", &component_io_port_num);

	ret = VIOC_CONFIG_LCDPath_Select(Output_Starter_LCDC_Num, component_io_port_num);
	if (ret < 0) {
		pr_err("[ERR][O_STARTER] %s: invalid lcd(%d) lcd_if(%d)\n", __func__,
				Output_Starter_LCDC_Num, component_io_port_num);
	}

	pDISP = pOutput_Starter_DISP;
	pWMIX = pOutput_Starter_WMIX;
	pRDMA = pOutput_Starter_RDMA;
	pSC = pOutput_Starter_SCALER;
	DPRINTF("%s: RDMA(%p)-SC(%p)-WMIX(%p)-DISP(%p)\n", __func__, pRDMA, pSC, pWMIX, pDISP);

	np_child = of_get_child_by_name(np_parent, "component_starter");

	/* set and enable vioc clock */
	if(Output_Starter_LCDC_Num)
		clock = of_clk_get_by_name(np_child, "lcdc1-clk");
	else
		clock = of_clk_get_by_name(np_child, "lcdc0-clk");

	#if 0
	if (of_property_read_u32(np_child, "lcdc-clock-frequency", &clock_rate)) {
		printk("%s, Can't read clock-frequency\n", __func__);
		clock_rate = 74250000;
	}
	#else
	clock_rate = component_spec.component_clk;
	#endif

	clk_set_rate(clock, clock_rate);
	clk_prepare_enable(clock);

	VIOC_DISP_TurnOff(pDISP);
	VIOC_RDMA_SetImageDisable(pRDMA);

	tcc_output_starter_memclr(BOOTLOADER_FB_WIDTH, BOOTLOADER_FB_HEIGHT);

	ComponentTiming.lpw = component_spec.component_LPW;
	ComponentTiming.lpc = component_spec.component_LPC + 1;
	ComponentTiming.lswc = component_spec.component_LSWC + 1;
	ComponentTiming.lewc = component_spec.component_LEWC + 1;
	ComponentTiming.vdb = component_spec.component_VDB;
	ComponentTiming.vdf = component_spec.component_VDF;
	ComponentTiming.fpw = component_spec.component_FPW1;
	ComponentTiming.flc = component_spec.component_FLC1;
	ComponentTiming.fswc = component_spec.component_FSWC1;
	ComponentTiming.fewc = component_spec.component_FEWC1;
	ComponentTiming.fpw2 = component_spec.component_FPW2;
	ComponentTiming.flc2 = component_spec.component_FLC2;
	ComponentTiming.fswc2 = component_spec.component_FSWC2;
	ComponentTiming.fewc2 = component_spec.component_FEWC2;

	memset((stLCDCTR *)&LcdCtrlParam, 0x00, sizeof(stLCDCTR));

	switch(type)
	{
		case STARTER_COMPONENT_480I_NTSC:
		case STARTER_COMPONENT_576I_PAL:
			break;

		case STARTER_COMPONENT_720P:
			LcdCtrlParam.r2ymd = 3;
			LcdCtrlParam.ckg = 1;
			LcdCtrlParam.id= 0;
			LcdCtrlParam.iv = 1;
			LcdCtrlParam.ih = 1;
			LcdCtrlParam.ip = 0;
			LcdCtrlParam.pxdw = 12;
			LcdCtrlParam.ni = 1;
			break;

		case STARTER_COMPONENT_1080I:
			LcdCtrlParam.r2ymd = 3;
			LcdCtrlParam.ckg = 1;
			LcdCtrlParam.id= 0;
			LcdCtrlParam.iv = 0;
			LcdCtrlParam.ih = 1;
			LcdCtrlParam.ip = 1;
			LcdCtrlParam.pxdw = 12;
			LcdCtrlParam.ni = 0;
			LcdCtrlParam.tv = 1;
			#ifdef CONFIG_ARCH_TCC898X
			LcdCtrlParam.advi = 0;		// 0 is enable (only tcc898x)
			#else
			LcdCtrlParam.advi = 1;		// 1 is enable
			#endif
			break;
		default:
			break;
	}

	#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
	switch (ADV7343_DEFAULT_OUTPUT_FORMAT) {
	case OUTPUT_FORMAT_RGB888:
		LcdCtrlParam.r2y = 0;
		LcdCtrlParam.pxdw = 12;	// 24bit RGB888
		break;
	case OUTPUT_FORMAT_YCbCr_24bit:
		LcdCtrlParam.r2ymd = 3;
		LcdCtrlParam.r2y = 1;
		LcdCtrlParam.pxdw = 12;	// 24bit RGB888 with r2y enable = 24bit (fake) YCbCr
		//dctrl_swap = 0x0; //0x4;
		align_swap = 0x4; //0x0;
		break;
	case OUTPUT_FORMAT_YCbCr_16bit:
	default:
		LcdCtrlParam.r2ymd = 3;
		LcdCtrlParam.r2y = 1;
		LcdCtrlParam.pxdw = 8;	// 16bit YCbCr
		break;
	}
	/* set VE_PBLK(GPIO_B19) and id=active high */
	LcdCtrlParam.id = 0;
	#endif

	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		//#if defined(CONFIG_FB_TCC_COMPONENT_THS8200)
		//if (ths8200_setting_mode == SETTING_MODE_TI) {
		//	LcdCtrlParam.y2r = LcdCtrlParam.r2y?0:1;
		//	LcdCtrlParam.r2y = 0;
		//} else {
		//	LcdCtrlParam.y2r = 0;
		//	LcdCtrlParam.r2y = 0;
		//	align_swap = 0x4;
		//}
		//#endif
		//#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
		LcdCtrlParam.y2r = LcdCtrlParam.r2y?0:1;
		LcdCtrlParam.r2y = 0;
		//#endif
	#endif

	tccfb_output_starter(TCC_OUTPUT_COMPONENT, Output_Starter_LCDC_Num, &ComponentTiming, &LcdCtrlParam, component_spec.component_clk);

	//#ifndef CONFIG_ARCH_TCC898X
	//tcc_gpio_config(TCC_GPE(27), GPIO_FN0|GPIO_OUTPUT|GPIO_HIGH);	// VE_FIELD: GPIO_E27
	//#endif

	#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
	VIOC_DISP_SetSwapbf(pDISP, align_swap);	// overwrite swap reg. after tccfb_output_starter()
	adv7343_enable(mode, ADV7343_DEFAULT_OUTPUT_FORMAT, 1);
	#elif defined(CONFIG_FB_TCC_COMPONENT_THS8200)
	ths8200_enable(mode, 1);
	#endif

	#if 0 // for debugging
	{
		unsigned int i;
		unsigned int *pReg = (unsigned int *)pLCDC_CH;
		for(i=0; i<32; i++)
		{
			printk("[DBG][O_STARTER] 0x%08x: 0x%08x\n", pReg+i, *(pReg+i));
		}
	}
	#endif

	VIOC_DISP_TurnOn(pDISP);
#endif
}

#ifdef CONFIG_OF
static int tcc_output_starter_parse_dt(struct device_node *np)
{
	struct device_node *np_fb_child, *np_fb;
	int index = 0, ret = 0;


	pOutput_Starter_DDICFG = VIOC_DDICONFIG_GetAddress();

	/* Get persist_display_mode from board_specific.dts files */
	ret = of_property_read_u32(np, "persist_display_mode", &persist_display_mode);
	if (ret) {
		persist_display_mode = -1;
		//pr_debug("[DBG][O_STARTER] %s, using output_starter's display_mode from Kconfig\n", __func__);
	} else {
		//pr_debug("[DBG][O_STARTER] %s, persist_display_mode(%d)\n", __func__, persist_display_mode);
	}


	#if defined(CONFIG_FB_TCC_COMPONENT)
	/* get the information of output_starter device node*/
	np_fb_child = of_parse_phandle(np, "scaler", 0);
	if(np_fb_child) {
		of_property_read_u32_index(np, "scaler", 1, &index);
		pOutput_Starter_SCALER = VIOC_SC_GetAddress(index);
	} else {
		pr_err("[ERR][O_STARTER] %s, could not find scaler node\n", __func__);
		ret = -ENODEV;
	}
	#endif

	/* get the information of vioc-fb device node*/
	np_fb = of_find_compatible_node(NULL, NULL, "telechips,vioc-fb");
	if(of_property_read_u32(np_fb, "telechips,fbdisplay_num", &Output_Starter_LCDC_Num)) {
		pr_err("[ERR][O_STARTER] %s, could not find fbdisplay_num\n", __func__);
		ret = -ENODEV;
	}
	pOutput_Starter_DISP = VIOC_DISP_GetAddress(Output_Starter_LCDC_Num);
	pOutput_Starter_WMIX = VIOC_WMIX_GetAddress(Output_Starter_LCDC_Num);

	if(Output_Starter_LCDC_Num)
		np_fb_child = of_find_node_by_name(np_fb, "fbdisplay1");
	else
		np_fb_child = of_find_node_by_name(np_fb, "fbdisplay0");

	if(np_fb_child) {
		if(of_property_read_u32_index(np_fb_child, "telechips,rdma", 1+0, &index) == 0)
			pOutput_Starter_RDMA = VIOC_RDMA_GetAddress(index);
	} else {
		pr_err("[ERR][O_STARTER] %s, could not find fbdisplay node\n", __func__);
		ret = -ENODEV;
	}

	ret = tcc_output_starter_parse_hdmi_dt(np);
	//pr_debug("[DBG][O_STARTER] %s, Output_Starter_LCDC_Num = %d \n", __func__, Output_Starter_LCDC_Num);

	return ret;
}
#else
static int tcc_output_starter_parse_dt(struct device_node *np)
{
}
#endif

static int tcc_output_starter_probe(struct platform_device *pdev)
{
	int hdmi_detect = 0;
	unsigned char lcdc_1st = STARTER_LCDC_0;
	unsigned char lcdc_2nd = STARTER_LCDC_1;

	tcc_output_starter_parse_dt(pdev->dev.of_node);

	/*
	 * output_starter_display_mode
	 *  0: Normal
	 *  1: Auto-detection (HDMI <-> CVBS)
	 *  2: HDMI + CVBS <-> CVBS
	 *  3: HDMI + CVBS <-> Component + CVBS
	 *
	 * Extend-display Output Setting (tccfb.h)
	 * ---------------------------------------
	 * #if defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)
	 *     #define TCC_OUTPUT_STARTER_AUTO_HDMI_CVBS
	 * #elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CVBS)
	 *     #define TCC_OUTPUT_STARTER_ATTACH_HDMI_CVBS
	 * #elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
	 *     #define TCC_OUTPUT_STARTER_ATTACH_DUAL_AUTO
	 * #else
	 *     #if defined(CONFIG_STB_BOARD_STB1)
	 *         #define TCC_OUTPUT_STARTER_ATTACH_DUAL_AUTO
	 *     #else
	 *         #define TCC_OUTPUT_STARTER_NORMAL
	 *     #endif
	 * #endif
	 */
	if (persist_display_mode != -1) {
		output_starter_display_mode = persist_display_mode;
		#if !defined(CONFIG_ANDROID) && defined(CONFIG_TCC_DISPMG)
		set_persist_display_mode(persist_display_mode);
		#endif
	} else {
		#ifdef TCC_OUTPUT_STARTER_NORMAL
		output_starter_display_mode = 0;
		#endif
		#ifdef TCC_OUTPUT_STARTER_AUTO_HDMI_CVBS
		output_starter_display_mode = 1;
		#endif
		#ifdef TCC_OUTPUT_STARTER_ATTACH_HDMI_CVBS
		output_starter_display_mode = 2;
		#endif
		#ifdef TCC_OUTPUT_STARTER_ATTACH_DUAL_AUTO
		output_starter_display_mode = 3;
		#endif
	}


	#if defined(CONFIG_OUTPUT_SKIP_KERNEL_LOGO)
	if (!strncmp(boot_recovery_mode, "boot_recovery", 13)) {
		pr_info("[INF][O_STARTER] %s, boot mode is recovery mode\n", __func__);
	} else {
		pr_info("[INF][O_STARTER] %s, skip displaying kernel logo\n", __func__);
		return 0;
	}
	#endif


	pmap_get_info("fb_video", &pmap_fb);
	pmap_get_info("output_attach", &pmap_attach);

	if(Output_Starter_LCDC_Num) {
		lcdc_1st = STARTER_LCDC_1; /* LCDC0: HDMI/Component/CVBS */
		lcdc_2nd = STARTER_LCDC_0; /* LCDC1: Attached CVBS */
	}
	else {
		lcdc_1st = STARTER_LCDC_0; /* LCDC0: HDMI/Component/CVBS */
		lcdc_2nd = STARTER_LCDC_1; /* LCDC1: Attached CVBS */
	}

        if (output_starter_display_mode == 1 || output_starter_display_mode == 3) {
                hdmi_detect = tcc_hdmi_detect_cable();
                pr_info("[INF][O_STARTER] %s hdmi cable = %d\r\n", __func__, hdmi_detect);
                if(!hdmi_detect) {
                        mdelay(10);
                        hdmi_detect = tcc_hdmi_detect_cable();
                        pr_info("[INF][O_STARTER] %s retry-hdmi cable = %d\r\n", __func__, hdmi_detect);
                }
        }

	switch (output_starter_display_mode) {
	case 1:
		if (hdmi_detect == true) {
			pr_info("[INF][O_STARTER] AUTO_HDMI_CVBS: hdmi\n");
                        #if defined(CONFIG_TCC_HDMI_DRIVER_V1_4)
                        tcc_output_starter_hdmi_v1_4(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                        #endif
                        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
			tcc_output_starter_hdmi_v2_0(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                        #endif
		} else {
			pr_info("[INF][O_STARTER] AUTO_HDMI_CVBS: composite\n");
			tcc_output_starter_hdmi_disable();
			tcc_output_starter_composite(lcdc_1st, default_composite_resolution, pdev);
		}
		break;

	case 2:
		/*
		 * 1st output - HDMI
		 */
		pr_info("[INF][O_STARTER] ATTACH_HDMI_CVBS: hdmi\n");
		#if defined(CONFIG_TCC_HDMI_DRIVER_V1_4)
                tcc_output_starter_hdmi_v1_4(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                #endif
                #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
		tcc_output_starter_hdmi_v2_0(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                #endif

		/*
		 * 2nd output - CVBS
		 */
		pr_info("[INF][O_STARTER] ATTACH_HDMI_CVBS: composite\n");
		#if defined(CONFIG_FB_TCC_COMPOSITE)
		tcc_composite_attach(lcdc_2nd, default_composite_resolution, 1);
		#endif
		break;

	case 3:
		/*
		 * 1st output - HDMI or Component
		 */
		if (hdmi_detect == true) {
			pr_info("[INF][O_STARTER] ATTACH_DUAL_AUTO: hdmi\n");
			#if defined(CONFIG_TCC_HDMI_DRIVER_V1_4)
                        tcc_output_starter_hdmi_v1_4(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                        #endif
                        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
			tcc_output_starter_hdmi_v2_0(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                        #endif
		} else {
			pr_info("[INF][O_STARTER] ATTACH_DUAL_AUTO: component\n");
			#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
			tcc_output_starter_hdmi_disable();
			#endif
			tcc_output_starter_component(lcdc_1st, default_component_resolution, pdev);
		}

		/*
		 * 2nd output - CVBS
		 */
		pr_info("[INF][O_STARTER] ATTACH_DUAL_AUTO: composite\n");
		#if defined(CONFIG_FB_TCC_COMPOSITE)
		tcc_composite_attach(lcdc_2nd, default_composite_resolution, 1);
		#endif
		break;

	case 0:
	default:
		/*
		 * TCC_OUTPUT_STARTER_NORMAL - HDMI
		 */

		pr_info("[INF][O_STARTER] OUTPUT_STARTER_NORMAL: hdmi\n");
		#if defined(CONFIG_TCC_HDMI_DRIVER_V1_4)
                tcc_output_starter_hdmi_v1_4(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                #endif
                #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
		tcc_output_starter_hdmi_v2_0(lcdc_1st, pOutput_Starter_RDMA, pOutput_Starter_DISP);
                #endif

		/* Disable sub disp (composite) */
		pr_info("[INF][O_STARTER] OUTPUT_STARTER_NORMAL: turn off sub display\n");
		vioc_sub_disp_composite_disable(lcdc_2nd);
		break;
	}

	return 0;
}

static int tcc_output_starter_remove(struct platform_device *pdev)
{
	if(pmap_fb.base){
		pmap_release_info("fb_video");
        pmap_fb.base = 0x00;
    }
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id output_starter_of_match[] = {
	{ .compatible = "telechips,tcc-output-starter" },
	{}
};
MODULE_DEVICE_TABLE(of, output_starter_of_match);
#endif

static struct platform_driver tcc_output_starter_driver = {
	.probe	= tcc_output_starter_probe,
	.remove	= tcc_output_starter_remove,
	.driver	= {
		.name	= "tcc_output_starter",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(output_starter_of_match),
	},
};

int __init tcc_output_starter_init(void)
{
	return platform_driver_register(&tcc_output_starter_driver);
}

static __exit void tcc_output_starter_exit(void)
{
	platform_driver_unregister(&tcc_output_starter_driver);
}

MODULE_AUTHOR("TELECHIPS");
MODULE_LICENSE("GPL");

module_init(tcc_output_starter_init);
module_exit(tcc_output_starter_exit);
