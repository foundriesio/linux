/****************************************************************************
FileName    : kernel/drivers/video/tcc/vioc/tcc_component.c
Description :

Copyright (C) 2013 Telechips Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>
#ifndef CONFIG_ARM64
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#endif
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <video/tcc/tcc_component_ioctl.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/tca_display_config.h>
#include <video/tcc/tcc_board_component.h>
#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif
#ifdef CONFIG_HDMI_DISPLAY_LASTFRAME
#include <video/tcc/tcc_vsync_ioctl.h>
#endif
#include "tcc_component.h"

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_TCC_DV_IN)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>
extern unsigned int dv_reg_phyaddr, dv_md_phyaddr;
extern int tca_edr_path_configure(void);
#endif

extern struct tcc_dp_device *tca_fb_get_displayType(TCC_OUTPUT_TYPE check_type);
extern void tca_scale_display_update(struct tcc_dp_device *pdp_data,
				     struct tcc_lcdc_image_update *ImageInfo);
extern void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo,
					  int specific_pclk);
extern void tca_vioc_displayblock_powerOff(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_disable(struct tcc_dp_device *pDisplayInfo);
extern void tca_vioc_displayblock_ctrl_set(unsigned int outDevice,
					   struct tcc_dp_device *pDisplayInfo,
					   stLTIMING *pstTiming,
					   stLCDCTR *pstCtrl);
extern void tca_fb_attach_start(struct tccfb_info *info);
extern int tca_fb_attach_stop(struct tccfb_info *info);

#if defined(CONFIG_TCC_VTA)
extern int vta_cmd_notify_change_status(const char *);
#endif

/*****************************************************************************

 VARIABLES

******************************************************************************/

extern char fb_power_state;

/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...) if(debug){printk("[DBG][COMPONENT] " msg);}

#define TCC_LCDC1_USE

static int Component_LCDC_Num = -1;
static int Component_Disp_Num;
static int Component_Scaler_Num;
static int Component_Mode = -1;
static int Component_RDMA_VIDEO_Num;
static volatile void __iomem *pComponent_DISP;
static volatile void __iomem *pComponent_WMIX;
static volatile void __iomem *pComponent_RDMA_UI;
static volatile void __iomem *pComponent_RDMA_VIDEO;
static volatile void __iomem *pComponent_SCALER;

static volatile void __iomem *pComponent_Attach_DISP;
static volatile void __iomem *pComponent_Attach_WMIX;
static volatile void __iomem *pComponent_Attach_RDMA_UI;
static volatile void __iomem *pComponent_Attach_RDMA_VIDEO;

#define DEVICE_NAME "component"
#define COMPONENT_MINOR 206

#define COMPONENT_DETECT_GPIO NULL
#define COMPONENT_DETECT_EINTSEL 0
#define COMPONENT_DETECT_EINTNUM 0
#define COMPONENT_DETECT_EINT NULL

static struct clk *component_lcdc0_clk;
static struct clk *component_lcdc1_clk;

static char tcc_component_mode = COMPONENT_MAX;
static char tcc_component_attached = 0;
static char tcc_component_starter = 0;

static char component_start = 0;
static char component_plugout = 0;
static unsigned int gComponentSuspendStatus = 0;
static unsigned int gLinuxComponentSuspendStatus = 0;

static struct device *pdev_component;

static int component_io_port_num = 0;

#define RDMA_UVI_MAX_WIDTH 3072

#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
extern int hdmi_get_hotplug_status(void);
#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
#error "display dual auto mode needs hdmi v2.0"
#endif

/*****************************************************************************

 CGMS-A FUNCTIONs

******************************************************************************/
unsigned int component_calc_cgms_crc(unsigned int data)
{
    int i;
    unsigned int org = data;//0x000c0;
    unsigned int dat;
    unsigned int tmp;
    unsigned int crc[6] = {1,1,1,1,1,1};
    unsigned int crc_val;

    dat = org;
    for (i= 0; i < 14; i++) {
        tmp = crc[5];
        crc[5] = crc[4];
        crc[4] = crc[3];
        crc[3] = crc[2];
        crc[2] = crc[1];
        crc[1] = ((dat & 0x01)) ^ crc[0] ^ tmp;
        crc[0] = ((dat & 0x01)) ^ tmp;
        dat = (dat >> 1);
    }

    crc_val = 0;
    for (i = 0; i < 6; i++) {
        crc_val |= crc[i] << (5 - i);
    }

    dprintk("%s: key=0x%x crc=0x%x (%d %d %d %d %d %d)\n", __func__,
        org, crc_val, crc[0], crc[1], crc[2], crc[3], crc[4], crc[5]);

    return crc_val;
}

void component_get_cgms(TCC_COMPONENT_CGMS_TYPE *cgms)
{
	if (component_start) {
		component_chip_get_cgms(&cgms->enable, &cgms->data);

		dprintk("%s: %s, 0x%05x\n", __func__, cgms->enable ? "on" : "off", cgms->data);
	}
}

void component_set_cgms(TCC_COMPONENT_CGMS_TYPE *cgms)
{
	if (component_start) {
		dprintk("%s: %s, 0x%05x\n", __func__, cgms->enable ? "on" : "off", cgms->data);

		component_chip_set_cgms(cgms->enable, cgms->data);

		if (unlikely(debug))
			component_get_cgms(cgms);
		dprintk("CGMS-A %s - Component\n", cgms->enable ? "on" : "off");
	}
}

void component_cgms_helper(unsigned int enable, unsigned int key)
{
	TCC_COMPONENT_CGMS_TYPE cgms;
	unsigned int data, crc;

	crc = component_calc_cgms_crc(key);
	data = (crc << 14) | key;

	cgms.enable = enable;
	cgms.data = data;
	component_chip_set_cgms(cgms.enable, cgms.data);

	if (unlikely(debug))
		component_chip_get_cgms(&cgms.enable, &cgms.data);

	printk("CGMS-A %s - Component\n", cgms.enable ? "ON" : "OFF");
}


/*****************************************************************************

 FUNCTIONS

******************************************************************************/
#if defined(CONFIG_SWITCH_GPIO_COMPONENT)
static struct platform_device tcc_component_detect_device = {
	.name = "switch-gpio-component-detect",
	.id = -1,
	.dev =
		{
			.platform_data = NULL,
		},
};
#endif


/*****************************************************************************
 Function Name : tcc_component_detect()
******************************************************************************/
int tcc_component_detect(void)
{
	int detect = true;

#if defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT) || defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CVBS)
	detect = false;
#elif defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
	/* Check the HDMI detection */
	if (hdmi_get_hotplug_status())
		detect = false;
#endif

	return detect;
}

/*****************************************************************************
 Function Name : tcc_component_get_spec()
******************************************************************************/
void tcc_component_get_spec(COMPONENT_MODE_TYPE mode, COMPONENT_SPEC_TYPE *spec)
{
	switch (mode) {
	case COMPONENT_MODE_NTSC_M:
	case COMPONENT_MODE_NTSC_M_J:
	case COMPONENT_MODE_PAL_M:
	case COMPONENT_MODE_NTSC_443:
	case COMPONENT_MODE_PSEUDO_PAL:
		spec->component_clk = 27 * 1000 * 1000;
		spec->component_bus_width = 8;
		spec->component_lcd_width = 720;
		spec->component_lcd_height = 480;
		spec->component_LPW = 128 - 1;
		spec->component_LPC = 720 * 2 - 1;
		spec->component_LSWC = 116 - 1;
		spec->component_LEWC = 32 - 1;

		spec->component_VDB = 0;
		spec->component_VDF = 0;

		spec->component_FPW1 = 6 - 1;
		spec->component_FLC1 = 480 - 1;
		spec->component_FSWC1 = 30 - 1;
		spec->component_FEWC1 = 9 - 1;
		spec->component_FPW2 = 6 - 1;
		spec->component_FLC2 = 480 - 1;
		spec->component_FSWC2 = 31 - 1;
		spec->component_FEWC2 = 8 - 1;
		break;

	case COMPONENT_MODE_NTSC_N:
	case COMPONENT_MODE_NTSC_N_J:
	case COMPONENT_MODE_PAL_N:
	case COMPONENT_MODE_PAL_B:
	case COMPONENT_MODE_PAL_G:
	case COMPONENT_MODE_PAL_H:
	case COMPONENT_MODE_PAL_I:
	case COMPONENT_MODE_PSEUDO_NTSC:
		spec->component_clk = 27 * 1000 * 1000;
		spec->component_bus_width = 8;
		spec->component_lcd_width = 720;
		spec->component_lcd_height = 576;
		spec->component_LPW = 128 - 1;
		spec->component_LPC = 720 * 2 - 1;
		spec->component_LSWC = 136 - 1;
		spec->component_LEWC = 24 - 1;

		spec->component_VDB = 0;
		spec->component_VDF = 0;

		spec->component_FPW1 = 5 - 1;
		spec->component_FLC1 = 576 - 1;
		spec->component_FSWC1 = 39 - 1;
		spec->component_FEWC1 = 5 - 1;
		spec->component_FPW2 = 5 - 1;
		spec->component_FLC2 = 576 - 1;
		spec->component_FSWC2 = 40 - 1;
		spec->component_FEWC2 = 4 - 1;
		break;

	case COMPONENT_MODE_720P:
		spec->component_clk = 74250 * 1000;
		spec->component_bus_width = 24;
		spec->component_lcd_width = 1280;
		spec->component_lcd_height = 720;

#if defined(CONFIG_FB_TCC_COMPONENT_THS8200)
		spec->component_LPW = 9 - 1;
		spec->component_LPC = 1280 - 1;
		spec->component_LSWC = 349 - 1;
		spec->component_LEWC = 12 - 1;

		spec->component_VDB = 0;
		spec->component_VDF = 0;

		spec->component_FPW1 = 3 - 1;
		spec->component_FLC1 = 720 - 1;
		spec->component_FSWC1 = 26 - 1;
		spec->component_FEWC1 = 1 - 1;
#elif defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
		spec->component_LPW = 9 - 1;
		spec->component_LPC = 1280 - 1;
		spec->component_LSWC = 313 - 1;
		spec->component_LEWC = 39 - 1;

		spec->component_VDB = 0;
		spec->component_VDF = 0;

		spec->component_FPW1 = 1 - 1;
		spec->component_FLC1 = 720 - 1;
		spec->component_FSWC1 = 32 - 1;
		spec->component_FEWC1 = 1 - 1;
#endif

		spec->component_FPW2 = spec->component_FPW1;
		spec->component_FLC2 = spec->component_FLC1;
		spec->component_FSWC2 = spec->component_FSWC1;
		spec->component_FEWC2 = spec->component_FEWC1;
		break;

	case COMPONENT_MODE_1080I:
		spec->component_clk = 74250 * 1000;
		spec->component_bus_width = 24;
		spec->component_lcd_width = 1920;
		spec->component_lcd_height = 1080;
		spec->component_LPW = 24 - 1;
		spec->component_LPC = 1920 - 1;
		spec->component_LSWC = 254 - 1;
		spec->component_LEWC = 2 - 1;

		spec->component_VDB = 0;
		spec->component_VDF = 0;

		spec->component_FPW1 = 5 * 2 - 1;
		spec->component_FLC1 = 540 * 2 - 1;
		spec->component_FSWC1 = 15 * 2 - 1;
		spec->component_FEWC1 = 2.5 * 2 - 1;
		spec->component_FPW2 = 5 * 2 - 1;
		spec->component_FLC2 = 540 * 2 - 1;
		spec->component_FSWC2 = 15.5 * 2 - 1;
		spec->component_FEWC2 = 2 * 2 - 1;
		break;

	default:
		break;
	}
}

/*****************************************************************************
 Function Name : tcc_component_set_lcd2tv()
******************************************************************************/
void tcc_component_set_lcd2tv(COMPONENT_MODE_TYPE mode, TCC_COMPONENT_START_TYPE start)
{
	COMPONENT_SPEC_TYPE spec;
	stLTIMING ComponentTiming;
	stLCDCTR LcdCtrlParam;
	int ret;

	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;
	struct tcc_dp_device *dp_device = NULL;

	unsigned int align_swap = 0;

	dprintk("%s, mode=%d\n", __func__, mode);

	tcc_component_get_spec(mode, &spec);

	/* set io ports for component output */
	ret = VIOC_CONFIG_LCDPath_Select(Component_LCDC_Num, component_io_port_num);
	if (ret < 0) {
		pr_err("[ERR][COMPONENT] %s: invalid lcd(%d) lcd_if(%d)\n", __func__,
				Component_LCDC_Num, component_io_port_num);
	}

	dprintk("%s : component_io_port_num = %d, spec.component_bus_width = %d\n",
		__func__, component_io_port_num, spec.component_bus_width);

	ComponentTiming.lpw = spec.component_LPW;
	ComponentTiming.lpc = spec.component_LPC + 1;
	ComponentTiming.lswc = spec.component_LSWC + 1;
	ComponentTiming.lewc = spec.component_LEWC + 1;

	ComponentTiming.vdb = spec.component_VDB;
	ComponentTiming.vdf = spec.component_VDF;
	ComponentTiming.fpw = spec.component_FPW1;
	ComponentTiming.flc = spec.component_FLC1;
	ComponentTiming.fswc = spec.component_FSWC1;
	ComponentTiming.fewc = spec.component_FEWC1;
	ComponentTiming.fpw2 = spec.component_FPW2;
	ComponentTiming.flc2 = spec.component_FLC2;
	ComponentTiming.fswc2 = spec.component_FSWC2;
	ComponentTiming.fewc2 = spec.component_FEWC2;

	memset(&LcdCtrlParam, 0, sizeof(LcdCtrlParam));

	switch (mode) {
	case COMPONENT_MODE_NTSC_M:
	case COMPONENT_MODE_NTSC_M_J:
	case COMPONENT_MODE_PAL_M:
	case COMPONENT_MODE_NTSC_443:
	case COMPONENT_MODE_PSEUDO_PAL:
	case COMPONENT_MODE_NTSC_N:
	case COMPONENT_MODE_NTSC_N_J:
	case COMPONENT_MODE_PAL_N:
	case COMPONENT_MODE_PAL_B:
	case COMPONENT_MODE_PAL_G:
	case COMPONENT_MODE_PAL_H:
	case COMPONENT_MODE_PAL_I:
	case COMPONENT_MODE_PSEUDO_NTSC:
		break;

	case COMPONENT_MODE_720P:
		// LcdCtrlParam.r2ymd = 3;
		LcdCtrlParam.ckg = 1;
		LcdCtrlParam.id = 0;
		LcdCtrlParam.iv = 1;
		LcdCtrlParam.ih = 1;
		LcdCtrlParam.ip = 1;
		LcdCtrlParam.pxdw = 12;
		LcdCtrlParam.ni = 1;
		break;

	case COMPONENT_MODE_1080I:
		//LcdCtrlParam.r2ymd = 3;
	#if defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
		LcdCtrlParam.advi = 0;
	#else
		#ifdef CONFIG_ARCH_TCC898X
		LcdCtrlParam.advi = 0; // 0 is enable (only tcc898x)
		#else
		LcdCtrlParam.advi = 1; // 1 is enable
		#endif
	#endif
		LcdCtrlParam.ckg = 1;
		LcdCtrlParam.id = 0;
		LcdCtrlParam.iv = 0;
		LcdCtrlParam.ih = 1;
		LcdCtrlParam.ip = 1;
		LcdCtrlParam.pxdw = 12;
		LcdCtrlParam.ni = 0;
		LcdCtrlParam.tv = 1;
		break;

	default:
		break;
	}

	#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
	LcdCtrlParam.r2ymd = 0;
	#else
	LcdCtrlParam.r2ymd = 3;
	#endif

	#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
	switch (ADV7343_DEFAULT_OUTPUT_FORMAT) {
	case OUTPUT_FORMAT_RGB888:
		LcdCtrlParam.r2y = 0;
		LcdCtrlParam.pxdw = 12; // 24bit RGB888
		break;
	case OUTPUT_FORMAT_YCbCr_24bit:
		LcdCtrlParam.r2ymd = 3;
		LcdCtrlParam.r2y = 1;
		LcdCtrlParam.pxdw = 12; // 24bit RGB888 with r2y enable = 24bit (fake) YCbCr
		// dctrl_swap = 0x0; //0x4;
		align_swap = 0x4; // 0x0;
		break;
	case OUTPUT_FORMAT_YCbCr_16bit:
	default:
		LcdCtrlParam.r2ymd = 3;
		LcdCtrlParam.r2y = 1;
		LcdCtrlParam.pxdw = 8; // 16bit YCbCr
		break;
	}
	/* set VE_PBLK(GPIO_B19) and id=active high */
	LcdCtrlParam.id = 0;
	#endif

	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	LcdCtrlParam.y2r = LcdCtrlParam.r2y ? 0 : 1;
	LcdCtrlParam.r2y = 0;
	#endif

	tccfb_info = info->par;

	#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
	dp_device = &tccfb_info->pdata.Sdp_data;
	#else
	if (tccfb_info->pdata.Mdp_data.DispNum == Component_LCDC_Num) {
		dp_device = &tccfb_info->pdata.Mdp_data;
	} else if (tccfb_info->pdata.Sdp_data.DispNum == Component_LCDC_Num) {
		dp_device = &tccfb_info->pdata.Sdp_data;
		dp_device->DispOrder = 1;
	}
	#endif

	if (dp_device) {
		tca_vioc_displayblock_disable(dp_device);

		dp_device->DispDeviceType = TCC_OUTPUT_COMPONENT;
		if (tcc_component_attached) {
			dp_device->FbUpdateType = FB_ATTACH_UPDATE;
		} else {
			dp_device->FbUpdateType = FB_SC_RDMA_UPDATE;
			dp_device->sc_num0 = VIOC_SCALER2;
		}

	#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
		dp_device->DispOrder = 1; // Set to Sub Display

		#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
		dp_device->FbUpdateType = FB_SC_RDMA_UPDATE;
		dp_device->sc_num0 = VIOC_SCALER4;
		#else
		dp_device->FbUpdateType = FB_RDMA_UPDATE;
		#endif

		#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
		if (dp_device->FbUpdateType == FB_SC_RDMA_UPDATE) {
			if (VIOC_CONFIG_PlugOut(dp_device->sc_num0) == VIOC_PATH_DISCONNECTED) {
				VIOC_CONFIG_SWReset(dp_device->sc_num0, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(dp_device->sc_num0, VIOC_CONFIG_CLEAR);
				VIOC_CONFIG_SWReset(dp_device->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(dp_device->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
				VIOC_CONFIG_PlugIn(dp_device->sc_num0, dp_device->rdma_info[RDMA_FB].blk_num);
			}
		}
		#endif
	#endif // CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB

		tca_vioc_displayblock_powerOn(dp_device, spec.component_clk);

	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_TCC_DV_IN)
		dv_reg_phyaddr = dv_md_phyaddr = 0x00;
		//pr_info("[INF][COMPONENT] #### DV EDR Mode ? format(%d), reg(0x%x)/meta(0x%x), outDevice(%d)/Disp_0(%p =? %p)\n",
		//			mode->format, mode->dv_reg_phyaddr, mode->dv_md_phyaddr,
		//			outDevice, pDISP, VIOC_DISP_GetAddress(0));
		if( (!tcc_component_attached) && (dp_device->ddc_info.virt_addr == VIOC_DISP_GetAddress(0)) )
		{
			if(start.dv_reg_phyaddr)
			{
				struct lcdc_timimg_parms_t component_timing;
				pr_info("[INF][COMPONENT] #### DV EDR Mode!!! SDR with Component 0x%x \n", start.dv_reg_phyaddr);

				component_timing.format = 0;

				component_timing.lpc 	= spec.component_LPC;
				component_timing.flc 	= spec.component_FLC1;
				component_timing.lewc 	= spec.component_LEWC;
				component_timing.lpw 	= spec.component_LPW;
				component_timing.lswc 	= spec.component_LSWC;
				component_timing.fewc 	= spec.component_FEWC1;
				component_timing.fpw 	= spec.component_FPW1;
				component_timing.fswc 	= spec.component_FSWC1;

				component_timing.dv_noYUV422_SDR = 0;

				voic_v_dv_set_hdmi_timming(&component_timing, 0, 0);
				vioc_v_dv_set_stage(DV_STANDBY);
				vioc_v_dv_set_mode(DV_STD, NULL, 0);
				VIOC_V_DV_Power(1);
			}
			else
			{
				VIOC_CONFIG_DV_SET_EDR_PATH(0);
			}
		}
	#endif

		tca_vioc_displayblock_ctrl_set(VIOC_OUTCFG_HDVENC, dp_device,
					       &ComponentTiming, &LcdCtrlParam);

	#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
		VIOC_DISP_SetSwapbf(VIOC_DISP_GetAddress(get_vioc_index(dp_device->ddc_info.blk_num)),
						align_swap); // overwrite swap reg. after tca_vioc_displayblock_ctrl_set()
	#endif
	}
}

/*****************************************************************************
 Function Name : tcc_component_get_lcdsize()
******************************************************************************/
void tcc_component_get_lcdsize(unsigned int *width, unsigned int *height)
{
	unsigned int lcdsize;

	lcdsize = __raw_readl(pComponent_DISP + DDS);

	*width = lcdsize & 0x0000FFFF;
	*height = lcdsize >> 16;
}

static int onthefly_using;

// 0 : 3 : layer enable/disable
void tcc_plugout_for_component(int ch_layer)
{
	unsigned int iSCType;

	iSCType = VIOC_SCALER1;

	if (ISSET(onthefly_using, 1 << ch_layer)) {
		VIOC_CONFIG_PlugOut(iSCType);
		BITCLR(onthefly_using, 1 << ch_layer);
	}
}
EXPORT_SYMBOL(tcc_plugout_for_component);

/*****************************************************************************
 Function Name : tcc_component_end()
******************************************************************************/
void tcc_component_end(void)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;
	struct tcc_dp_device *dp_device = NULL;

	dprintk("%s, LCDC_Num = %d\n", __func__, Component_LCDC_Num);

	// tcc_component_set_interrupt(false, Component_LCDC_Num);

	#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
	/*
	 * Reset the device:
	 * If ALSB=1 then the ADV7343 include a power-on-reset circuit
	 * to ensure correct operation after power-up
	 */
	#elif defined(CONFIG_FB_TCC_COMPONENT_THS8200)
	// ths8200_power(0);
	ths8200_reset();
	#endif

	/* Clear the start flag */
	component_start = 0;

	if (component_plugout)
		component_plugout = 0;

	tccfb_info = info->par;
	
	if (tccfb_info->pdata.Mdp_data.DispNum == Component_LCDC_Num)
		dp_device = &tccfb_info->pdata.Mdp_data;
	else if (tccfb_info->pdata.Sdp_data.DispNum == Component_LCDC_Num)
		dp_device = &tccfb_info->pdata.Sdp_data;
	else
		return;

	#if defined(CONFIG_TCC_VTA)
	vta_cmd_notify_change_status(__func__);
	#endif

	tca_vioc_displayblock_disable(dp_device);
	tca_vioc_displayblock_powerOff(dp_device);
	dp_device->DispDeviceType = TCC_OUTPUT_NONE;
           
}

/*****************************************************************************
 Function Name : tcc_component_start()
******************************************************************************/
void tcc_component_start(TCC_COMPONENT_START_TYPE start)
{
	printk("%s mode=%d, lcdc_num=%d\n", __func__, start.mode, Component_LCDC_Num);

	tcc_component_mode = start.mode;

	switch (start.mode) {
	case COMPONENT_NTST_M:
		Component_Mode = COMPONENT_MODE_NTSC_M;
		break;
	case COMPONENT_PAL:
		Component_Mode = COMPONENT_MODE_PAL_B;
		break;
	case COMPONENT_720P:
		Component_Mode = COMPONENT_MODE_720P;
		break;
	case COMPONENT_1080I:
		Component_Mode = COMPONENT_MODE_1080I;
		break;
	default:
		break;
	}

	tcc_component_set_lcd2tv(Component_Mode, start);

	#if defined(CONFIG_FB_TCC_COMPONENT_ADV7343)
	adv7343_enable(Component_Mode, ADV7343_DEFAULT_OUTPUT_FORMAT, tcc_component_starter);
	#elif defined(CONFIG_FB_TCC_COMPONENT_THS8200)
	ths8200_enable(Component_Mode, tcc_component_starter);
	#endif

	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_TCC_DV_IN)
	dv_reg_phyaddr = start.dv_reg_phyaddr;
	dv_md_phyaddr = start.dv_md_phyaddr;
	tca_edr_path_configure();
	#endif

	/* Set the start flag */
	component_start = 1;
}

/*****************************************************************************
 Function Name : tcc_component_clock_onoff()
******************************************************************************/
void tcc_component_clock_onoff(char OnOff)
{
	dprintk("%s, Onoff = %d \n", __func__, OnOff);
}

#ifdef CONFIG_PM
static int component_enable(void)
{
	printk("%s\n", __func__);

	pm_runtime_get_sync(pdev_component);

	return 0;
}

static int component_disable(void)
{
	printk("%s\n", __func__);

	pm_runtime_put_sync(pdev_component);

	return 0;
}
#endif

static int component_blank(int blank_mode)
{
	int ret = 0;

	printk("%s : blank(mode=%d)\n", __func__, blank_mode);

	#ifdef CONFIG_PM
	if ((pdev_component->power.usage_count.counter == 1) &&
	    (blank_mode == 0)) {
		// usage_count = 1 ( resume ), blank_mode = 0 ( FB_BLANK_UNBLANK
		// ) is stable state when booting don't call runtime_suspend or
		// resume state
		// printk("%s ### state = [%d] count =[%d] power_cnt=[%d]
		// \n",__func__,blank_mode, pdev_component->power.usage_count,
		// pdev_component->power.usage_count.counter);
		return 0;
	}

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		component_disable();
		break;
	case FB_BLANK_UNBLANK:
		component_enable();
		break;
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_VSYNC_SUSPEND:
	default:
		ret = -EINVAL;
	}
	#endif

	return ret;
}

/*****************************************************************************
 Function Name : tcc_component_attach()
******************************************************************************/
void tcc_component_attach(char lcdc_num, char mode, char starter_flag)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;
	TCC_COMPONENT_START_TYPE start;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_TCC_DV_IN)
	start.dv_reg_phyaddr = start.dv_md_phyaddr = 0x00;
#endif
	dprintk("%s, lcdc_num=%d, mode=%d, starter=%d\n", __func__, lcdc_num,
		mode, starter_flag);

	/* reset previous display path */
	if (starter_flag) {
		VIOC_DISP_TurnOff(pComponent_Attach_DISP);
		VIOC_DISP_Wait_DisplayDone(pComponent_Attach_DISP);
		VIOC_RDMA_SetImageDisable(pComponent_Attach_RDMA_UI);
		VIOC_RDMA_SetImageDisable(pComponent_Attach_RDMA_VIDEO);
	}

	tccfb_info = info->par;

	tcc_component_attached = 1;
	tcc_component_starter = starter_flag;

	Component_LCDC_Num = lcdc_num;

	if (mode >= COMPONENT_MAX)
		start.mode = COMPONENT_720P;
	else
		start.mode = mode;

	tcc_component_clock_onoff(true);
	tcc_component_start(start);

	tca_fb_attach_start(tccfb_info);
}

/*****************************************************************************
 Function Name : tcc_component_detach()
******************************************************************************/
void tcc_component_detach(void)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;

	printk("%s\n", __func__);

	tccfb_info = info->par;

	tcc_component_end();

	tca_fb_attach_stop(tccfb_info);

	tcc_component_attached = 0;
	tcc_component_starter = 0;

	Component_LCDC_Num = Component_Disp_Num;
}

/*****************************************************************************
 Function Name : tcc_component_ioctl()
******************************************************************************/
static long tcc_component_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	TCC_COMPONENT_START_TYPE start;
	TCC_COMPONENT_CGMS_TYPE cgms;

	switch (cmd) {
	case TCC_COMPONENT_IOCTL_HPD_SWITCH_STATUS: {
		#if defined(CONFIG_SWITCH_GPIO_COMPONENT)
		unsigned int enable = 0;

		if (copy_from_user(&enable, (void *)arg, sizeof(enable)))
			return -EFAULT;

		if (tcc_component_detect_device.dev.platform_data != NULL) {
			struct component_gpio_switch_data *switch_data =
				tcc_component_detect_device.dev.platform_data;

			if (enable) {
				switch_data->send_component_event(
					switch_data, TCC_COMPONENT_ON);
				pr_info("[INF][COMPONENT] TCC_COMPONENT_IOCTL_HPD_SWITCH_STATUS: enable(%d)\n", enable);
			} else {
				switch_data->send_component_event(
					switch_data, TCC_COMPONENT_OFF);
				pr_info("[INF][COMPONENT] TCC_COMPONENT_IOCTL_HPD_SWITCH_STATUS: enable(%d)\n", enable);
			}
		}
		#endif
		break;
	}
	case TCC_COMPONENT_IOCTL_START:
		dprintk("%s [TCC_COMPONENT_IOCTL_START] \n", __func__);
		if (copy_from_user(&start, (void *)arg, sizeof(start)))
			return -EFAULT;

		if (tcc_component_attached)
			tcc_component_detach();

		tcc_component_start(start);
		break;

	case TCC_COMPONENT_IOCTL_UPDATE: {
		struct tcc_dp_device *dp_device =
			tca_fb_get_displayType(TCC_OUTPUT_COMPONENT);
		struct tcc_lcdc_image_update update;
		if (copy_from_user(&update, (void *)arg, sizeof(update)))
			return -EFAULT;

		if (dp_device)
			tca_scale_display_update(dp_device, &update);
	} break;

	case TCC_COMPONENT_IOCTL_END:
		dprintk("%s [TCC_COMPONENT_IOCTL_END] \n", __func__);
		#if defined(CONFIG_SWITCH_GPIO_COMPONENT)
		if (tcc_component_detect_device.dev.platform_data != NULL) {
			struct component_gpio_switch_data *switch_data =
				tcc_component_detect_device.dev.platform_data;
			switch_data->send_component_event(switch_data, TCC_COMPONENT_OFF);
		}
		#endif
		if (component_start)
			tcc_component_end();
		break;

	case TCC_COMPONENT_IOCTL_PROCESS:
		break;

	case TCC_COMPONENT_IOCTL_ATTACH:
		#if defined(CONFIG_TCC_DISPLAY_MODE_USE)
		if (copy_from_user(&start, (void *)arg, sizeof(start)))
			return -EFAULT;

		tcc_component_mode = start.mode;

		if (tcc_component_attached)
			tcc_component_detach();

		if (start.lcdc)
			tcc_component_attach(0, start.mode, 0);
		else
			tcc_component_attach(1, start.mode, 0);
		#endif
		break;

	case TCC_COMPONENT_IOCTL_DETACH:
		#if defined(CONFIG_TCC_DISPLAY_MODE_USE)
		tcc_component_detach();
		#endif
		break;

	case TCC_COMPONENT_IOCTL_BLANK: {
		unsigned int cmd;

		if (get_user(cmd, (unsigned int __user *)arg))
			return -EFAULT;

		dprintk("TCC_COMPONENT_IOCTL_BLANK: %d\n", cmd);

		component_blank(cmd);
		break;
	}

	case TCC_COPONENT_IOCTL_GET_SUSPEND_STATUS: {
		if (gLinuxComponentSuspendStatus) {
			put_user(gLinuxComponentSuspendStatus, (unsigned int __user *)arg);
			gLinuxComponentSuspendStatus = 0;
		} else {
			put_user(gComponentSuspendStatus, (unsigned int __user *)arg);
		}
		break;
	}

	case TCC_COMPONENT_IOCTL_SET_CGMS:
		if (copy_from_user(&cgms, (void __user*)arg, sizeof(cgms)))
			return -EFAULT;
		component_set_cgms(&cgms);
		break;

	case TCC_COMPONENT_IOCTL_GET_CGMS:
		component_get_cgms(&cgms);
		if (copy_to_user((void __user *)arg, &cgms, sizeof(cgms)))
			return -EFAULT;
		break;

	default:
		printk("%d, Unsupported IOCTL!!!\n", cmd);
		break;
	}

	return 0;
}

/*****************************************************************************
 Function Name : tcc_component_open()
******************************************************************************/
static int tcc_component_open(struct inode *inode, struct file *filp)
{
	dprintk("%s  \n", __func__);

	return 0;
}

/*****************************************************************************
 Function Name : tcc_component_release()
******************************************************************************/
static int tcc_component_release(struct inode *inode, struct file *file)
{
	dprintk("%s  \n", __func__);

	return 0;
}

#ifdef CONFIG_PM
int component_runtime_suspend(struct device *dev)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;
	struct tcc_dp_device *dp_device = NULL;

	printk("%s+++\n", __func__);

	if (component_start) {
		tcc_component_end();
	}

	gComponentSuspendStatus = 1;

	printk("%s---\n", __func__);
	return 0;
}

int component_runtime_resume(struct device *dev)
{
	printk("%s:  \n", __func__);

	gComponentSuspendStatus = 0;

	return 0;
}

static int component_suspend(struct device *dev)
{
	#ifndef CONFIG_ANDROID
	/* Linux Platform */
	if (component_start)
		tcc_component_end();
	gLinuxComponentSuspendStatus = 1;
	#endif //

	dprintk("%s\n", __func__);
	return 0;
}

static int component_resume(struct device *dev)
{
	dprintk("%s\n", __func__);
	return 0;
}
#endif

static struct file_operations tcc_component_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tcc_component_ioctl,
	.open = tcc_component_open,
	.release = tcc_component_release,
};

static struct miscdevice tcc_component_misc_device = {
	.minor = COMPONENT_MINOR,
	.name = "component",
	.fops = &tcc_component_fops,
};

#ifdef CONFIG_OF
static int component_parse_dt(struct device_node *np)
{
	struct device_node *np_fb_child, *np_fb, *np_fb_1st, *np_fb_2nd;
	int index = 0, ret = 0;

	/* get the number of io port - [0]:GPIO_B, [1]:GPIO_E */
	of_property_read_u32(np, "io_port_num", &component_io_port_num);

	/* get the information of component device node */
	component_lcdc0_clk = of_clk_get(np, 0);
	BUG_ON(component_lcdc0_clk == NULL);
	component_lcdc1_clk = of_clk_get(np, 1);
	BUG_ON(component_lcdc1_clk == NULL);

	np_fb_child = of_parse_phandle(np, "scaler", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np, "scaler", 1, &index);
		pComponent_SCALER = VIOC_SC_GetAddress(index);
		Component_Scaler_Num = index;
	} else {
		printk("%s, could not find scaler node\n", __func__);
		ret = -ENODEV;
	}

	/* get the information of vioc-fb device node */
	np_fb = of_find_compatible_node(NULL, NULL, "telechips,vioc-fb");

	if (of_property_read_u32(np_fb, "telechips,fbdisplay_num", &Component_Disp_Num)) {
		pr_err("[ERR][COMPONENT] %s, could not find fbdisplay_num\n", __func__);
		ret = -ENODEV;
	}

	#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
	component_io_port_num = 0;
	Component_Disp_Num = 1;
	#endif

	if (Component_Disp_Num) {
		np_fb_1st = of_find_node_by_name(np_fb, "fbdisplay1");
		np_fb_2nd = of_find_node_by_name(np_fb, "fbdisplay0");
	} else {
		np_fb_1st = of_find_node_by_name(np_fb, "fbdisplay0");
		np_fb_2nd = of_find_node_by_name(np_fb, "fbdisplay1");
	}

	Component_LCDC_Num = Component_Disp_Num;

	/* get register address for main output */
	np_fb_child = of_parse_phandle(np_fb_1st, "telechips,disp", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np_fb_1st, "telechips,disp", 1, &index);
		pComponent_DISP = VIOC_DISP_GetAddress(index);
	} else {
		pr_err("[ERR][COMPONENT] %s, could not find disp node\n", __func__);
		ret = -ENODEV;
	}

	np_fb_child = of_parse_phandle(np_fb_1st, "telechips,wmixer", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np_fb_1st, "telechips,wmixer", 1, &index);
		pComponent_WMIX = VIOC_WMIX_GetAddress(index);
	} else {
		pr_err("[ERR][COMPONENT] %s, could not find disp wmixer\n", __func__);
		ret = -ENODEV;
	}

	np_fb_child = of_parse_phandle(np_fb_1st, "telechips,rdma", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np_fb_1st, "telechips,rdma", 1 + 0, &index);
		pComponent_RDMA_UI = VIOC_RDMA_GetAddress(index);
		of_property_read_u32_index(np_fb_1st, "telechips,rdma", 1 + 3, &index);
		pComponent_RDMA_VIDEO = VIOC_RDMA_GetAddress(index);
		Component_RDMA_VIDEO_Num = index;
	} else {
		pr_err("[ERR][COMPONENT] %s, could not find disp rdma\n", __func__);
		ret = -ENODEV;
	}

	/* get register address for attached output */
	np_fb_child = of_parse_phandle(np_fb_2nd, "telechips,disp", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np_fb_2nd, "telechips,disp", 1, &index);
		pComponent_Attach_DISP = VIOC_DISP_GetAddress(index);
	} else {
		pr_err("[ERR][COMPONENT] %s, could not find disp node for attached output\n", __func__);
		ret = -ENODEV;
	}

	np_fb_child = of_parse_phandle(np_fb_2nd, "telechips,wmixer", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np_fb_2nd, "telechips,wmixer", 1, &index);
		pComponent_Attach_WMIX = VIOC_WMIX_GetAddress(index);
	} else {
		pr_err("[ERR][COMPONENT] %s, could not find disp wmixer for attached output\n", __func__);
		ret = -ENODEV;
	}

	np_fb_child = of_parse_phandle(np_fb_2nd, "telechips,rdma", 0);
	if (np_fb_child) {
		of_property_read_u32_index(np_fb_2nd, "telechips,rdma", 1 + 0, &index);
		pComponent_Attach_RDMA_UI = VIOC_RDMA_GetAddress(index);
		of_property_read_u32_index(np_fb_2nd, "telechips,rdma", 1 + 3, &index);
		pComponent_Attach_RDMA_VIDEO = VIOC_RDMA_GetAddress(index);
	} else {
		pr_err("[ERR][COMPONENT] %s, could not find disp rdma for attached output\n", __func__);
		ret = -ENODEV;
	}
	printk("%s, Component_LCDC_Num = %d \n", __func__, Component_Disp_Num);

	return ret;
}
#else
static int component_parse_dt(struct device_node *np)
{
}
#endif

static int component_probe(struct platform_device *pdev)
{
	pr_info("[INF][COMPONENT] %s\n", __func__);

	pdev_component = &pdev->dev;

	/* parse device tree */
	component_parse_dt(pdev->dev.of_node);

	if (misc_register(&tcc_component_misc_device)) {
		printk(KERN_WARNING
		       "COMPONENT: Couldn't register device 10, %d.\n",
		       COMPONENT_MINOR);
		return -EBUSY;
	}

	#if defined(CONFIG_SWITCH_GPIO_COMPONENT)
	platform_device_register(&tcc_component_detect_device);
	#endif

	#ifdef CONFIG_PM
	pm_runtime_set_active(pdev_component);
	pm_runtime_enable(pdev_component);
	pm_runtime_get_noresume(pdev_component); // increase usage_count
	#endif

	return 0;
}

static int component_remove(struct platform_device *pdev)
{
	printk("%s LCDC:%d \n", __func__, Component_LCDC_Num);

	misc_deregister(&tcc_component_misc_device);

	if (component_lcdc0_clk)
		clk_put(component_lcdc0_clk);
	if (component_lcdc1_clk)
		clk_put(component_lcdc1_clk);

	return 0;
}

static const struct dev_pm_ops component_pm_ops = {
	#ifdef CONFIG_PM
	.runtime_suspend = component_runtime_suspend,
	.runtime_resume = component_runtime_resume,
	.suspend = component_suspend,
	.resume = component_resume,
	#endif
};

#ifdef CONFIG_OF
static struct of_device_id component_of_match[] = {
	{.compatible = "telechips,tcc-component"},
	{}};
MODULE_DEVICE_TABLE(of, component_of_match);
#endif

static struct platform_driver tcc_component_driver = {
	.probe = component_probe,
	.remove = component_remove,
	.driver = {
		.name = "tcc_component",
		.owner = THIS_MODULE,
		#ifdef CONFIG_PM
		.pm = &component_pm_ops,
		#endif
		#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(component_of_match),
		#endif
	},
};

/*****************************************************************************
 Function Name : tcc_component_init()
******************************************************************************/
int __init tcc_component_init(void)
{
	return platform_driver_register(&tcc_component_driver);
}

/*****************************************************************************
 Function Name : tcc_component_cleanup()
******************************************************************************/
void __exit tcc_component_exit(void)
{
	platform_driver_unregister(&tcc_component_driver);
}

module_init(tcc_component_init);
module_exit(tcc_component_exit);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips COMPONENT Out driver");
MODULE_LICENSE("GPL");
