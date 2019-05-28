/*
 * linux/drivers/video/tcc/tccfb_interface.c
 *
 * Based on:    Based on s3c2410fb.c, sa1100fb.c and others
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC LCD Controller Frame Buffer Driver
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
#include <linux/err.h>
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/of_address.h>
#include <linux/clk-provider.h> /* __clk_is_enabled */
#include <asm/io.h>
#include <asm/div64.h>
#include <asm/system_info.h>

#ifdef CONFIG_PM
#include "tcc_hibernation_logo.h"
#include <linux/pm.h>
#endif
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include <soc/tcc/pmap.h>
#include <video/tcc/irqs.h>			//TODO: remove
#include <video/tcc/tcc_types.h>

#include <video/tcc/tca_lcdc.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/tccfb.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_grp_ioctrl.h>
#include <video/tcc/tcc_scaler_ctrl.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/tcc_composite_ioctl.h>
#include <video/tcc/tcc_component_ioctl.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_viqe.h>
#include <video/tcc/vioc_ddicfg.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/tcc/tca_map_converter.h>
#endif
#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif

#include <video/tcc/tca_display_config.h>
#include <video/tcc/tccfb_address.h>

#include <video/tcc/vioc_global.h>

#if defined(CONFIG_TCC_VIOCMG)
#include <video/tcc/viocmg.h>
#endif

#if defined(CONFIG_VIOC_AFBCDEC)
#include <video/tcc/vioc_afbcdec.h>
#endif

#if defined (CONFIG_VIDEO_TCC_VOUT)
#include <video/tcc/tcc_vout_v4l2.h>
extern int tcc_vout_get_status( unsigned int type );
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>
#if defined(CONFIG_TCC_DV_IN)
#include <video/tcc/vioc_lut.h>
#endif

#define DEF_DV_CHECK_NUM 1 // 2

unsigned int DV_PROC_CHECK = 0x0;
static int bStep_Check = DEF_DV_CHECK_NUM;
static unsigned int nFrame = 0;
unsigned int dv_reg_phyaddr = 0x00, dv_md_phyaddr = 0x00;
static unsigned int bUse_GAlpha = 0;
#define dvprintk(msg...) //printk( "dolby-vision[voic-interface]: " msg);

typedef struct dolby_layer_str_t
{
	unsigned int offset[3];
	unsigned int format;
	unsigned int buffer_width;
	unsigned int buffer_height;
	unsigned int frame_width;
	unsigned int frame_height;
	unsigned int bit10;
} dolby_layer_str_t;
#endif

#include "tcc_vsync.h"

//#define TEST_SC_NO_PLUGIN_IN_BYPASS_CASE

#if defined(CONFIG_ARCH_TCC803X)
#define AnD_FB_SC	(VIOC_SCALER5)
#define UI_CHROMA_EN	(0)
#else
#if defined(CONFIG_ANDROID)
#define AnD_FB_SC	(VIOC_SCALER2)
#define UI_CHROMA_EN	(0)
#elif defined(CONFIG_PLATFORM_AVN)
#define AnD_FB_SC	(VIOC_SCALER0)
#define UI_CHROMA_EN	(0)
#elif defined(CONFIG_PLATFORM_STB)
#define AnD_FB_SC	(VIOC_SCALER2)
#define UI_CHROMA_EN	(0)
#else
#define AnD_FB_SC	(VIOC_SCALER2)
#define UI_CHROMA_EN	(0)
#endif
#endif

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
#define DIV_FB_SC       (VIOC_SCALER4)
#else
#define DIV_FB_SC       (VIOC_SCALER3)
#endif
/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk( "VIOC_I: " msg); }

struct device_node *ViocScaler_np;
struct device_node *ViocConfig_np;

#if defined(CONFIG_VIOC_AFBCDEC)
static unsigned int afbc_dec_1st_cfg = 0;
static unsigned int afbc_dec_vioc_id = VIOC_AFBCDEC0;
static unsigned int afbc_wdma_need_on = 0; /* 0: no, 1: need */
#define AFBC_MAX_SURFACE (2)
#endif

#if defined(__CONFIG_HIBERNATION)
extern unsigned int do_hibernation;
#if defined(CONFIG_USING_LAST_FRAMEBUFFER)
extern void fb_quickboot_lastframe_display_release(void);
#endif
#endif//CONFIG_HIBERNATION

extern TCC_OUTPUT_TYPE Output_SelectMode;
extern void tcc_plugout_for_composite(int ch_layer);
extern void tcc_plugout_for_component(int ch_layer);
#ifdef CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME
extern void tcc_video_frame_backup(struct tcc_lcdc_image_update *Image);
#endif
#ifdef CONFIG_DISPLAY_EXT_FRAME
#ifdef CONFIG_LINUX
extern int restricted_ExtFrame;
#endif
extern int tcc_ctrl_ext_frame(char enable);
#endif

#if defined(CONFIG_TCC_HDMI_DRIVER_V1_4) || defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
extern void hdmi_stop(void);
extern void hdmi_start(void);
extern int hdmi_get_VBlank(void);
#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
extern unsigned int hdmi_get_pixel_clock(void);
#endif
#if defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
extern void set_hdmi_drm(HDMI_DRM_MODE mode, struct tcc_lcdc_image_update *pImage, unsigned int layer);
#endif
#endif

int tccxxx_grp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_grp_release(struct inode *inode, struct file *filp);
int tccxxx_grp_open(struct inode *inode, struct file *filp);

#define DEV_SCALER1	"dev/scaler1"

static struct file *scaler_filp = NULL;

static struct inode g2d_inode;
static struct file g2d_filp;

static int (*g2d_ioctl) ( struct file *, unsigned int, unsigned long);
static int (*g2d_open) (struct inode *, struct file *);
static int (*g2d_release) (struct inode *, struct file *);

static unsigned int fb_scaler_pbuf0;
static unsigned int fb_scaler_pbuf1;
static unsigned int fb_g2d_pbuf0;
static unsigned int fb_g2d_pbuf1;

#ifdef CONFIG_DIRECT_MOUSE_CTRL
static tcc_display_mouse mouse_data;
#endif
static tcc_display_resize resize_data;
static tcc_display_divide divide_data;
static tcc_display_resize output_attach_resize_data;


extern void tca_lcdc_interrupt_onoff(char onoff, char lcdc);

#ifdef CONFIG_ANDROID
#define ATTACH_SCALER (VIOC_SCALER0)
#else
#define ATTACH_SCALER (VIOC_SCALER3)
#endif//

static tcc_display_attach		attach_data;
static tcc_display_attach_intr	attach_intr;

#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
static tcc_display_resize secondary_display_resize_data;	//composite , component
#endif//

//#define CONFIG_DUMP_DISPLAY_UNDERRUN

const char nDeCompressor_Main = 0; //For MC and DTRC

#if defined(CONFIG_DUMP_DISPLAY_UNDERRUN)
static struct work_struct      dump_handle;
#endif
static unsigned int underrun_idx = 0;

#ifdef CONFIG_SUPPORT_2D_COMPRESSION
extern int tcc_2d_compression_read(void);
#endif

#if defined(CONFIG_DUMP_DISPLAY_UNDERRUN)
void tca_fb_dump_underrun_state(void)
{
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	VIOC_RDMA_DUMP(NULL, 1);
	VIOC_SCALER_DUMP(NULL, 2);
	VIOC_WMIX_DUMP(NULL, 0);
	VIOC_DISP_DUMP(NULL, 0);
	VIOC_WDMA_DUMP(NULL, 0);

	VIOC_IREQConfig_DUMP(0x200, 0x10); //RDMA plug-in
	VIOC_IREQConfig_DUMP(0x280, 0x10); //MC plug-in
	VIOC_IREQConfig_DUMP(0x40, 0x20); //SC plug-in

	VIOC_IREQConfig_DUMP(0x30, 0x10); //EDR set
	VIOC_IREQConfig_DUMP(0xF0, 0x10); //EDR set

	VIOC_RDMA_DUMP(NULL, 4);
	VIOC_RDMA_DUMP(NULL, 5);
	VIOC_DISP_DUMP(NULL, 1);

#else
	unsigned int idx = 0;

	printk("tca_fb_dump_underrun_state %d'th\r\n", underrun_idx);

	/* VIOC RDMA DUMP 0 - 3 */
	for (idx = 0; idx < 4; idx++)
		VIOC_RDMA_DUMP(VIOC_RDMA_GetAddress(idx), idx);

	/* WMIXER DUMP 0 - 1 */
	for (idx = 0; idx < 2; idx++)
		VIOC_WMIX_DUMP(VIOC_WMIX_GetAddress(idx), idx);

	/* SCALER DUMP 0 - 4 */
	for (idx = 0; idx < 4; idx++)
		VIOC_SCALER_DUMP(VIOC_SC_GetAddress(idx), idx);

	/* VIOC_CONFIGURATION DUMP */
	VIOC_IREQConfig_DUMP(0x00, 0x100);
#endif
}
#endif

void tca_fb_mem_scale_init(void)
{
	pmap_t pmap;

	pmap_get_info("fb_scale0", &pmap);
	fb_scaler_pbuf0 = pmap.base;
	pmap_get_info("fb_scale1", &pmap);
	fb_scaler_pbuf1 = pmap.base;

	pmap_get_info("fb_g2d0", &pmap);
	fb_g2d_pbuf0 = pmap.base;
	pmap_get_info("fb_g2d1", &pmap);
	fb_g2d_pbuf1 = pmap.base;

	#if defined(CONFIG_TCC_GRAPHIC)
	g2d_ioctl = tccxxx_grp_ioctl;
	g2d_open = tccxxx_grp_open;
	g2d_release = tccxxx_grp_release;
	#endif//

	pr_info("%s scaler buffer:0x%x 0x%x g2d buffer : 0x%x, 0x%x size:%d \n", __func__, fb_scaler_pbuf0, fb_scaler_pbuf1, fb_g2d_pbuf0, fb_g2d_pbuf1, pmap.size);
}


unsigned int tca_get_scaler_num(TCC_OUTPUT_TYPE Output, unsigned int Layer)
{
	unsigned int iSCNum = VIOC_SCALER3;

#ifdef CONFIG_ANDROID
	if(Output == TCC_OUTPUT_LCD)
		return VIOC_SCALER3;
	else if(Output == TCC_OUTPUT_HDMI || Output == TCC_OUTPUT_COMPOSITE || Output == TCC_OUTPUT_COMPONENT)
	{
		if (Layer == RDMA_FB)
			return VIOC_SCALER2;
		else if (Layer == RDMA_VIDEO)
			return VIOC_SCALER1;
		else // RDMA_VIDEO_SUB
			return VIOC_SCALER3;
	}
#else
	#ifdef CONFIG_PLATFORM_STB
		if (Layer == RDMA_VIDEO)
			return VIOC_SCALER1;
		else
		{
		#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			return VIOC_SCALER3;
		#else
			return VIOC_SCALER0;
		#endif
		}
	#else
		return VIOC_SCALER1;
	#endif//
#endif//

	return iSCNum;
}
EXPORT_SYMBOL(tca_get_scaler_num);

unsigned int tca_get_main_decompressor_num(void)
{
	return nDeCompressor_Main;
}
EXPORT_SYMBOL(tca_get_main_decompressor_num);

#if defined(CONFIG_VIOC_RESET_FOR_UNDERRUN)
/*
 * TODO: Change display device reset Function
 * there is problem about resetting components
 */
void vioc_display_device_reset(unsigned int device_num, struct tcc_dp_device *pDisplayDevice)
{
	#define DD0_DMA_CONNECT_CHECK(PlugInOut) 	((PlugInOut.connect_device == VIOC_SC_RDMA_00)|| (PlugInOut.connect_device == VIOC_SC_RDMA_01) \
													||(PlugInOut.connect_device == VIOC_SC_RDMA_02) || (PlugInOut.connect_device == VIOC_SC_RDMA_03) \
													|| (PlugInOut.connect_device == VIOC_SC_WDMA_00))

	#define DD1_DMA_CONNECT_CHECK(PlugInOut) 	((PlugInOut.connect_device == VIOC_SC_RDMA_04)|| (PlugInOut.connect_device == VIOC_SC_RDMA_05) \
													||(PlugInOut.connect_device == VIOC_SC_RDMA_06) || (PlugInOut.connect_device == VIOC_SC_RDMA_07)  \
													|| (PlugInOut.connect_device == VIOC_SC_WDMA_01))

	volatile void __iomem  *DISPBackup, *pDISPBackup;
	volatile void __iomem *WMIXBackup, *pWMIX_Addr;

	volatile void __iomem  *RDMABackup[RDMA_MAX_NUM], *pRDMA_Addr[RDMA_MAX_NUM];

	volatile unsigned char WMIXER_N, RDMA_N[RDMA_MAX_NUM];

	VIOC_PlugInOutCheck VIOC_PlugIn[VIOC_VIQE + 1];
	volatile unsigned char VIOC_PlugIn_reset[get_vioc_index(VIOC_VIQE0) + 1];
	volatile void __iomem *pSC_Backup[get_vioc_index(VIOC_SCALER3) + 1];
	volatile void __iomem *pSC_Addr[get_vioc_index(VIOC_SCALER3) +1];

	volatile void __iomem *ViqeBackup, *pViqe_Addr;
	volatile unsigned char i;

	unsigned int vioc_compress[RDMA_MAX_NUM] = {0,};		//dtrc, mapconverter, dec100...

	for(i = 0; i <= get_vioc_index(VIOC_VIQE_MAX); i++)
	{
		VIOC_PlugIn_reset[i] = false;

		VIOC_CONFIG_Device_PlugState((unsigned int)i, &VIOC_PlugIn[i]);

		if(VIOC_PlugIn[i].enable)
		{
			if(device_num == VIOC_DISP1) {
				if(DD1_DMA_CONNECT_CHECK(VIOC_PlugIn[i]))
					VIOC_PlugIn_reset[i] = true;
			}
			else	{
				if(DD0_DMA_CONNECT_CHECK(VIOC_PlugIn[i]))
					VIOC_PlugIn_reset[i] = true;
			}
		}
	}

	pDISPBackup = pDisplayDevice->ddc_info.virt_addr;

	pWMIX_Addr = pDisplayDevice->wmixer_info.virt_addr;

	WMIXER_N = pDisplayDevice->wmixer_info.blk_num;

	for(i = 0; i < RDMA_MAX_NUM; i++)	{
		pRDMA_Addr[i] = pDisplayDevice->rdma_info[i].virt_addr;
		RDMA_N[i] = pDisplayDevice->rdma_info[i].blk_num;
	}

	// backup RDMA_N, WMIXER, DisplayDevice register
	for(i = 0; i < RDMA_MAX_NUM; i++)	{
		RDMABackup[i] = kmalloc(0x20, GFP_KERNEL);
		memcpy((void *)RDMABackup[i], (void *)pRDMA_Addr[i], 0x20);
	}

	if(device_num == 0) {
		vioc_compress[2] = VIOC_CONFIG_DMAPath_Select(VIOC_RDMA02);
		vioc_compress[3] = VIOC_CONFIG_DMAPath_Select(VIOC_RDMA03);
	}
	else	{
		vioc_compress[3] = VIOC_CONFIG_DMAPath_Select(VIOC_RDMA07);
	}

	// off compressed block
	for(i = 0; i < RDMA_MAX_NUM; i++)
	{
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if((vioc_compress[i] >= VIOC_MC0) && (vioc_compress[i] < (VIOC_MC0 + VIOC_MC_MAX)))
			tca_map_convter_onoff(vioc_compress[i], 0, 0);
		#endif//

		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if((vioc_compress[i] >= VIOC_DTRC0) && (vioc_compress[i] < (VIOC_DTRC0 + VIOC_DTRC_MAX)))
			tca_dtrc_convter_onoff(vioc_compress[i], 0, 0);
		#endif//
	}

	DISPBackup = kmalloc(0x20, GFP_KERNEL);
	if (DISPBackup != NULL)
		memcpy((void *)DISPBackup, (void *)pDISPBackup, 0x20);
	else
		pr_err("%s: DISPBackup is null\n", __func__);

	WMIXBackup = kmalloc(0x20, GFP_KERNEL);
	if (WMIXBackup != NULL)
		memcpy((void *)WMIXBackup, (void *)pWMIX_Addr, 0x20);
	else
		pr_err("%s: WMIXBackup is null\n", __func__);

	pViqe_Addr = VIOC_VIQE_GetAddress(0);
	for(i = 0; i < (get_vioc_index(VIOC_SCALER3)+1); i++) {
		pSC_Addr[i] = VIOC_SC_GetAddress(i);
		if(VIOC_PlugIn_reset[i] && (pSC_Addr[i] != NULL)) {
			pSC_Backup[0] = kmalloc(0x20, GFP_KERNEL);
			memcpy((void *)pSC_Backup[i], (void *)pSC_Addr[i], 0x20);
		}
	}

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_VIQE0)] && (pViqe_Addr != NULL)) {
		ViqeBackup = kmalloc(0x60, GFP_KERNEL);
		if(ViqeBackup != NULL)
			memcpy((void *)ViqeBackup, (void *)pViqe_Addr, 0x60);
		else
			pr_err("%s: ViqeBackup is null\n", __func__);
	}

	// h/w block reset
	VIOC_CONFIG_SWReset(device_num, VIOC_CONFIG_RESET);		// disp reset
	VIOC_CONFIG_SWReset(device_num, VIOC_CONFIG_RESET);		// wdma reset
	VIOC_CONFIG_SWReset(WMIXER_N, VIOC_CONFIG_RESET);		// wmix reset

	for(i = 0; i < RDMA_MAX_NUM; i++)	{
		VIOC_CONFIG_SWReset(RDMA_N[i], VIOC_CONFIG_RESET);		// rdma reset
	}

	for(i = 0; i < RDMA_MAX_NUM; i++)
	{
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if((vioc_compress[i] >= VIOC_MC0) && (vioc_compress[i] < (VIOC_MC0 + VIOC_MC_MAX)))
			VIOC_CONFIG_SWReset(vioc_compress[i], VIOC_CONFIG_RESET);		// mc
		#endif//
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if((vioc_compress[i] >= VIOC_DTRC0) && (vioc_compress[i] < (VIOC_DTRC0 + VIOC_DTRC_MAX)))
			VIOC_CONFIG_SWReset(vioc_compress[i], VIOC_CONFIG_RESET);		// dtrc
		#endif//
	}

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER0)])
		VIOC_CONFIG_SWReset(VIOC_SCALER0, VIOC_CONFIG_RESET);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER1)])
		VIOC_CONFIG_SWReset(VIOC_SCALER1, VIOC_CONFIG_RESET);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER2)])
		VIOC_CONFIG_SWReset(VIOC_SCALER2, VIOC_CONFIG_RESET);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER3)])
		VIOC_CONFIG_SWReset(VIOC_SCALER3, VIOC_CONFIG_RESET);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_VIQE0)])
		VIOC_CONFIG_SWReset(VIOC_VIQE0, VIOC_CONFIG_RESET);


	// h/w block release reset
	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_VIQE0)])
		VIOC_CONFIG_SWReset(VIOC_VIQE0, VIOC_CONFIG_CLEAR);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER3)])
		VIOC_CONFIG_SWReset(VIOC_SCALER3, VIOC_CONFIG_CLEAR);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER2)])
		VIOC_CONFIG_SWReset(VIOC_SCALER2, VIOC_CONFIG_CLEAR);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER1)])
		VIOC_CONFIG_SWReset(VIOC_SCALER1, VIOC_CONFIG_CLEAR);

	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_SCALER0)])
		VIOC_CONFIG_SWReset(VIOC_SCALER0, VIOC_CONFIG_CLEAR);


	for(i = 0; i < RDMA_MAX_NUM; i++)
		VIOC_CONFIG_SWReset(RDMA_N[i], VIOC_CONFIG_CLEAR);		// rdma reset release

	for(i = 0; i < RDMA_MAX_NUM; i++)
	{
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if((vioc_compress[i] >= VIOC_MC0) && (vioc_compress[i] < (VIOC_MC0 + VIOC_MC_MAX)))
			VIOC_CONFIG_SWReset(vioc_compress[i], VIOC_CONFIG_CLEAR);		// mc reset release
		#endif//

		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if((vioc_compress[i] >= VIOC_DTRC0) && (vioc_compress[i] < (VIOC_DTRC0 + VIOC_DTRC_MAX)))
			VIOC_CONFIG_SWReset(vioc_compress[i], VIOC_CONFIG_CLEAR);		// dtrc reset release
		#endif//
	}
	VIOC_CONFIG_SWReset(WMIXER_N, VIOC_CONFIG_RESET);		// wmix reset release
	VIOC_CONFIG_SWReset(device_num, VIOC_CONFIG_RESET);		// wdma reset release
	VIOC_CONFIG_SWReset(device_num, VIOC_CONFIG_RESET);		// disp reset release

	// restore VIQE, SCALER , RDMA_N, WMIXER, DisplayDevice register
	if(VIOC_PlugIn_reset[get_vioc_index(VIOC_VIQE0)] && (pViqe_Addr != NULL)) {
		if(ViqeBackup != NULL) {
			memcpy((void *)pViqe_Addr, (void *)ViqeBackup, 0x60);
			kfree((void *)ViqeBackup);
		}
	}

	for (i = 0; i < (get_vioc_index(VIOC_SCALER3)+i); i++) {
		if(VIOC_PlugIn_reset[i] && (pSC_Addr[i] != NULL)) {
			memcpy((void *)pSC_Addr[i], (void *)pSC_Backup[i], 0x20);
			VIOC_SC_SetUpdate(pSC_Addr[i]);
		}
	}

	for(i = 0; i < RDMA_MAX_NUM; i++) {
		memcpy((void *)pRDMA_Addr[i], (void *)RDMABackup[i], 0x20);
	}

	if (WMIXBackup != NULL)
		memcpy((void *)pWMIX_Addr, (void *)WMIXBackup, 0x20);

	for(i = 0; i < RDMA_MAX_NUM; i++)	{
		VIOC_RDMA_SetImageUpdate(pRDMA_Addr[i]);
	}

	VIOC_WMIX_SetUpdate(pWMIX_Addr);

	if (DISPBackup != NULL) {
		memcpy((void *)pDISPBackup, (void *)DISPBackup, 0x20);
		kfree((void *)DISPBackup);
	}

	if (WMIXBackup != NULL) {
		kfree((void *)WMIXBackup);
	}

	for(i = 0; i < get_vioc_index(VIOC_SCALER3)+1; i++) {
		if(pSC_Backup[i] != NULL)
			kfree((void *)pSC_Backup[i]);
	}

	for(i = 0; i < RDMA_MAX_NUM; i++)	{
		kfree((void *)RDMABackup[i]);
	}

	printk("VIOC_sc0:%d VIOC_sc1:%d VIOC_sc2:%d ", VIOC_PlugIn_reset[0], VIOC_PlugIn_reset[1], VIOC_PlugIn_reset[2]);
    	printk(" VIOC_sc3:%d VIOC_VIQE:%d  \n", VIOC_PlugIn_reset[3], VIOC_PlugIn_reset[4]);
}
#endif

irqreturn_t tca_main_display_handler(int irq, void *dev_id)
{
	struct tccfb_info *fbdev = dev_id;
	unsigned int dispblock_status;
#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
	static unsigned int VIOCFifoUnderRun = 0;
#endif
#if defined(CONFIG_VIOC_AFBCDEC)
	unsigned int afbcDec_status;
#endif

	if (dev_id == NULL) {
		pr_err("%s irq: %d dev_id:%p \n",__func__, irq, dev_id);
		return IRQ_HANDLED;
	}

#if defined(CONFIG_VIOC_AFBCDEC)
	afbcDec_status = VIOC_AFBCDec_GetStatus(VIOC_AFBCDec_GetAddress(afbc_dec_vioc_id));
	if(afbcDec_status & AFBCDEC_IRQ_ALL) {
		//pr_info("AFBC(%d) INT(0x%x) ------ \n", afbc_dec_1st_cfg, afbcDec_status);
		if( afbcDec_status & AFBCDEC_IRQ_SURF_COMPLETED_MASK) {
			VIOC_AFBCDec_ClearIrq(VIOC_AFBCDec_GetAddress(afbc_dec_vioc_id), AFBCDEC_IRQ_SURF_COMPLETED_MASK);
		}		
		if( afbcDec_status & AFBCDEC_IRQ_CONFIG_SWAPPED_MASK) {
			VIOC_AFBCDec_ClearIrq(VIOC_AFBCDec_GetAddress(afbc_dec_vioc_id), AFBCDEC_IRQ_CONFIG_SWAPPED_MASK);
			//VIOC_AFBCDec_TurnOn(VIOC_AFBCDec_GetAddress(afbc_dec_vioc_id), VIOC_AFBCDEC_SWAP_PENDING);
		}
		if( afbcDec_status & AFBCDEC_IRQ_DECODE_ERR_MASK) {
			unsigned int rdma_enable;
			VIOC_AFBCDec_ClearIrq(VIOC_AFBCDec_GetAddress(afbc_dec_vioc_id), AFBCDEC_IRQ_CONFIG_SWAPPED_MASK);
			pr_info("AFBC(%d) INT(0x%x) ------ \n", afbc_dec_1st_cfg, AFBCDEC_IRQ_DECODE_ERR_MASK);
			VIOC_RDMA_GetImageEnable(fbdev->pdata.Mdp_data.rdma_info[RDMA_FB].virt_addr, &rdma_enable);
			if(rdma_enable)
				VIOC_RDMA_SetImageDisable(fbdev->pdata.Mdp_data.rdma_info[RDMA_FB].virt_addr);
			VIOC_CONFIG_SWReset(afbc_dec_vioc_id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(afbc_dec_vioc_id, VIOC_CONFIG_CLEAR);

			afbc_dec_1st_cfg = 0;
		}
		if( afbcDec_status & AFBCDEC_IRQ_DETILING_ERR_MASK) {
			unsigned int rdma_enable;
			VIOC_AFBCDec_ClearIrq(VIOC_AFBCDec_GetAddress(afbc_dec_vioc_id), AFBCDEC_IRQ_CONFIG_SWAPPED_MASK);
			pr_info("AFBC(%d) INT(0x%x) ------ \n", afbc_dec_1st_cfg, AFBCDEC_IRQ_DETILING_ERR_MASK);
			VIOC_RDMA_GetImageEnable(fbdev->pdata.Mdp_data.rdma_info[RDMA_FB].virt_addr, &rdma_enable);
			if(rdma_enable)
				VIOC_RDMA_SetImageDisable(fbdev->pdata.Mdp_data.rdma_info[RDMA_FB].virt_addr);
			VIOC_CONFIG_SWReset(afbc_dec_vioc_id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(afbc_dec_vioc_id, VIOC_CONFIG_CLEAR);
			afbc_dec_1st_cfg = 0;
		}
	}
#endif

	fbdev->vsync_timestamp = ktime_get();

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH()) {
		volatile void __iomem *pDV_Cfg = VIOC_DV_VEDR_GetAddress(VDV_CFG);
		unsigned int status = 0;
		VIOC_V_DV_GetInterruptPending(pDV_Cfg, &status);

		if(status & DV_FB_INT) {
			VIOC_V_DV_ClearInterrupt(pDV_Cfg, DV_FB_INT);

			if (fbdev->active_vsync)
				schedule_work(&fbdev->vsync_work);

			if (fbdev->pdata.Mdp_data.pandisp_sync.state == 0) {
				fbdev->pdata.Mdp_data.pandisp_sync.state = 1;
				wake_up_interruptible(&fbdev->pdata.Mdp_data.pandisp_sync.waitq);
			}
	    }
		return IRQ_HANDLED;
	}
#endif

	dispblock_status = vioc_intr_get_status(fbdev->pdata.lcdc_number);

//	dprintk("%s  sync_struct.state:0x%x dispblock_status:0x%x, fbdev:%p \n",
//				__func__, fbdev->pdata.Mdp_data.pandisp_sync.state, dispblock_status, fbdev);

	if (dispblock_status & (1<<VIOC_DISP_INTR_RU)) {
		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_RU));
		if (fbdev->active_vsync)
			schedule_work(&fbdev->vsync_work);
		if (fbdev->pdata.Mdp_data.pandisp_sync.state == 0) {
			fbdev->pdata.Mdp_data.pandisp_sync.state = 1;
			wake_up_interruptible(&fbdev->pdata.Mdp_data.pandisp_sync.waitq);
		}
		if(VIOC_DISP_Get_TurnOnOff(fbdev->pdata.Mdp_data.ddc_info.virt_addr)) {
			if(dispblock_status & (1<<VIOC_DISP_INTR_FU)) {
				vioc_intr_disable(irq, fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_FU));
				vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_FU));
		#if defined(CONFIG_DUMP_DISPLAY_UNDERRUN) && defined(CONFIG_VIOC_DOLBY_VISION_EDR)
				if ( underrun_idx++ < 1){
					pr_err(" FIFO UNDERUN(%d) STATUS:0x%x device type:%d \n", underrun_idx,dispblock_status, fbdev->pdata.Mdp_data.DispDeviceType);
					tca_fb_dump_underrun_state();
				}
				else
		#endif
				if ( (underrun_idx++) % 60 == 1){
					pr_err(" FIFO UNDERUN(%d) STATUS:0x%x device type:%d \n", underrun_idx,dispblock_status, fbdev->pdata.Mdp_data.DispDeviceType);
				}
		#if defined(CONFIG_DUMP_DISPLAY_UNDERRUN)
				if ( underrun_idx % 60 == 1) {
					schedule_work(&dump_handle);
				}
		#else
			#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
				if(VIOCFifoUnderRun == 0) {
					#if defined(CONFIG_VIOC_RESET_FOR_UNDERRUN)
					if(fbdev->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
						hdmi_stop();

					VIOC_DISP_TurnOff(fbdev->pdata.Mdp_data.ddc_info.virt_addr);
					#endif//
				}
				VIOCFifoUnderRun++;
			#endif
		#endif
			}
		} else {
			//Don't care FIFO under-run, when DISP is off.
		}
	}

	if(dispblock_status & (1<<VIOC_DISP_INTR_DD)) {
		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_DD));
#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
		if (fbdev->pdata.Mdp_data.disp_dd_sync.state == 0) {
			fbdev->pdata.Mdp_data.disp_dd_sync.state = 1;
			wake_up_interruptible(&fbdev->pdata.Mdp_data.disp_dd_sync.waitq);
		}
		#if defined(CONFIG_VIOC_RESET_FOR_UNDERRUN)
		if(VIOCFifoUnderRun) {
			vioc_display_device_reset(fbdev->pdata.lcdc_number, &fbdev->pdata.Mdp_data);
			VIOC_DISP_TurnOn(fbdev->pdata.Mdp_data.ddc_info.virt_addr);
			printk("%s DISABEL DONE Lcdc_num:%d VIOCFifoUnderRun:0x%x \n", __func__,fbdev->pdata.lcdc_number, VIOCFifoUnderRun);

			if(fbdev->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI)
				hdmi_start();

		}
		#endif//
		VIOCFifoUnderRun = 0;
		pr_info("%s DISABEL DONE Lcdc_num:%d 0x%p  STATUS:0x%x  \n",
			__func__,fbdev->pdata.lcdc_number, fbdev->pdata.Mdp_data.ddc_info.virt_addr, dispblock_status);
#endif
	}

	if(dispblock_status & (1<<VIOC_DISP_INTR_SREQ)) {
		vioc_intr_clear(fbdev->pdata.lcdc_number, (1<<VIOC_DISP_INTR_SREQ));
	}

	return IRQ_HANDLED;
}

static void _tca_vioc_intr_onoff(char on, int irq, char lcdc_num)
{
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH() && lcdc_num == 0)
	{
		if(on)
			vioc_intr_enable(irq, VIOC_INTR_V_DV, DV_FB_INT);
		else
			vioc_intr_disable(irq, VIOC_INTR_V_DV, DV_FB_INT);
	}
	else
#endif
	{
		if(on)
			vioc_intr_enable(irq, lcdc_num, VIOC_DISP_INTR_DISPLAY);
		else
			vioc_intr_disable(irq, lcdc_num, VIOC_DISP_INTR_DISPLAY);
	}
}

int tca_main_interrupt_reg(char SetClear, struct tccfb_info *info)
{
	int ret = 0;
//	pr_info("%s SetClear:%d lcdc_num:%d %d  INT_VIOC_DEV1:%d \n",__func__, SetClear, info->pdata.lcdc_number, info->pdata.Mdp_data.ddc_info.irq_num, INT_VIOC_DEV1);

	if(SetClear)	{
		vioc_intr_clear(info->pdata.lcdc_number, 0x39);

		ret = request_irq( info->pdata.Mdp_data.ddc_info.irq_num, tca_main_display_handler, IRQF_SHARED, "TCC_MAIN_D", info);

		_tca_vioc_intr_onoff(ON, info->pdata.Mdp_data.ddc_info.irq_num, info->pdata.lcdc_number);

		#if defined(CONFIG_DUMP_DISPLAY_UNDERRUN)
		INIT_WORK(&dump_handle, tca_fb_dump_underrun_state);
		#endif


	}
	else {
		#if defined(CONFIG_DUMP_DISPLAY_UNDERRUN)
		flush_work(&dump_handle);
		#endif

		_tca_vioc_intr_onoff(OFF, info->pdata.Mdp_data.ddc_info.irq_num, info->pdata.lcdc_number);

		free_irq( info->pdata.Mdp_data.ddc_info.irq_num, info);
	}

	return ret;
}
EXPORT_SYMBOL(tca_main_interrupt_reg);

static irqreturn_t tca_sub_display_handler(int irq, void *dev_id)
{
	struct tcc_dp_device *pDisplayInfo = dev_id;
	unsigned int dispblock_status;

	if(dev_id == NULL)	{
		pr_err("%s irq: %d dev_id:%p \n",__func__, irq, dev_id);
		return IRQ_HANDLED;
	}

	dispblock_status = vioc_intr_get_status(pDisplayInfo->DispNum);
	if(dispblock_status & VIOC_DISP_IREQ_RU_MASK) {
		vioc_intr_clear(pDisplayInfo->DispNum, (1<<VIOC_DISP_INTR_RU));

		if(pDisplayInfo->pandisp_sync.state == 0)		{
			pDisplayInfo->pandisp_sync.state = 1;
			wake_up_interruptible(&pDisplayInfo->pandisp_sync.waitq);
		}

		#if !(defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION) || defined(TCC_VIDEO_DISPLAY_BY_VSYNC_INT))
		//tca_lcdc_interrupt_onoff(false, pDisplayInfo->DispNum);
		#endif

		if(VIOC_DISP_Get_TurnOnOff(pDisplayInfo->ddc_info.virt_addr)) {
			if(dispblock_status & (1<<VIOC_DISP_INTR_FU)) {
	#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
				pr_err("%s FIFO UNDERUN STATUS:0x%x \n",__func__, dispblock_status);
	#endif
				vioc_intr_clear(pDisplayInfo->DispNum, (1<<VIOC_DISP_INTR_FU));
				//vioc_intr_disable(irq, pDisplayInfo->DispNum, (1<<VIOC_DISP_INTR_FU));
			}
		} else {
			//Don't care FIFO under-run, when DISP is off.
		}
	}

	if(dispblock_status & (1<<VIOC_DISP_INTR_DD)) {
		vioc_intr_clear(pDisplayInfo->DispNum, (1<<VIOC_DISP_INTR_DD));

#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
		if (pDisplayInfo->disp_dd_sync.state == 0) {
			pDisplayInfo->disp_dd_sync.state = 1;
			wake_up_interruptible(&pDisplayInfo->disp_dd_sync.waitq);
		}

		pr_info("%s DISABEL DONE Lcdc_num:%d 0x%p  STATUS:0x%x \n",
			__func__,pDisplayInfo->DispNum, pDisplayInfo->ddc_info.virt_addr, dispblock_status);
#endif
	}

	return IRQ_HANDLED;
}

void tca_vioc_displayblock_clock_select(struct tcc_dp_device *pDisplayInfo, int clk_src_hdmi_phy)
{
        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        unsigned int hdmi_pixel_clock;
        volatile void __iomem *pDDICONFIG = VIOC_DDICONFIG_GetAddress();

        clk_src_hdmi_phy = clk_src_hdmi_phy?1:0;

        if(pDisplayInfo != NULL &&
                pDDICONFIG != NULL) {
                if(pDisplayInfo->DispDeviceType == TCC_OUTPUT_HDMI) {
                        if(clk_src_hdmi_phy) {
                                //pr_info("The display device uses hdmi phy clocks\r\n");
                                hdmi_pixel_clock = 24000000;
                        } else {
				/* HDMI PHY -> Lx LCLK */
				if(!IS_ERR(pDisplayInfo->ddc_clock)) {
                                        if(__clk_is_enabled(pDisplayInfo->ddc_clock)) {
					        clk_disable_unprepare(pDisplayInfo->ddc_clock);
                                        }
	                                hdmi_pixel_clock = hdmi_get_pixel_clock();
	                                if(hdmi_pixel_clock == 0) {
	                                        hdmi_pixel_clock = 24000000;
	                                }
	                                //pr_info("The display device uses peri clock - %luHz \r\n", hdmi_pixel_clock);

	                                clk_set_rate(pDisplayInfo->ddc_clock, hdmi_pixel_clock);
					clk_prepare_enable(pDisplayInfo->ddc_clock);
				}
                        }

                        /* Set LCDx clock source is Lx_LCLK */
                        VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, pDisplayInfo->DispNum, clk_src_hdmi_phy);
                        #if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
                        /* Set DV clock source is L0_LCLK */
                        if ( DV_PATH_DIRECT & vioc_get_path_type() ) {
                                pr_info(" dv select %s clock\r\n", clk_src_hdmi_phy?"hdmiphy":"periclk");
                                VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, 3, clk_src_hdmi_phy);
                        } else if(VIOC_DDICONFIG_GetPeriClock(pDDICONFIG, 3) && clk_src_hdmi_phy == 0) {
                                pr_info(" dv select periclk clock!!\r\n");
                                VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, 3, clk_src_hdmi_phy);
                        }
                        #endif
                        if(clk_src_hdmi_phy == 1) {
                                /* Lx LCLK -> HDMI PHY */
				if(!IS_ERR(pDisplayInfo->ddc_clock)) {
                                        if(__clk_is_enabled(pDisplayInfo->ddc_clock)) {
                                                clk_disable_unprepare(pDisplayInfo->ddc_clock);
                                        }
                                	clk_set_rate(pDisplayInfo->ddc_clock, hdmi_pixel_clock);
					clk_prepare_enable(pDisplayInfo->ddc_clock);
				}
                        }
                        hdmi_pixel_clock = clk_get_rate(pDisplayInfo->ddc_clock);
                        //pr_info("L%d_LCLK is %luHz\r\n", pDisplayInfo->DispNum, hdmi_pixel_clock);
                }
        }
        #endif
}

/*
 * specific_pclk: only used by Component & Composite
 * In case of HDMI output, specific_pclk is used as skip_display_device
 */
void tca_vioc_displayblock_powerOn(struct tcc_dp_device *pDisplayInfo, int specific_pclk)
{
	int ret = 0;
	int vioc_clock_value = 0;
	struct device_node *np_output = NULL;
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	volatile void __iomem *pDDICONFIG = VIOC_DDICONFIG_GetAddress();
	#endif

	// get and set the value of vioc clock
	if(pDisplayInfo->DispDeviceType == TCC_OUTPUT_HDMI)
	{
                #if defined(CONFIG_ARCH_TCC897X)
                np_output = of_find_compatible_node(NULL, NULL, "telechips,tcc897x-hdmi");
                #endif

		#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
                if(!specific_pclk) {
                        tca_vioc_displayblock_clock_select(pDisplayInfo, 0);
                }
                specific_pclk = 0;
		#endif
	}
	else if(pDisplayInfo->DispDeviceType == TCC_OUTPUT_COMPOSITE)
	{
		np_output = of_find_compatible_node(NULL, NULL, "telechips,tcc-composite");

		#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, pDisplayInfo->DispNum, 0);
		#endif
	}
	else if(pDisplayInfo->DispDeviceType == TCC_OUTPUT_COMPONENT)
	{
		np_output = of_find_compatible_node(NULL, NULL, "telechips,tcc-component");

		#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, pDisplayInfo->DispNum, 0);
		#endif
	}

	if(np_output != NULL) {
		of_property_read_u32(np_output, "vioc-frequency", &vioc_clock_value);

		/* Component and Composite clocks vary with chip and resolution. */
		if (0 < specific_pclk) {
			vioc_clock_value = specific_pclk;
		}

		clk_set_rate(pDisplayInfo->ddc_clock, vioc_clock_value);
	}

        /* Get real vioc clock value */
        vioc_clock_value = clk_get_rate(pDisplayInfo->ddc_clock);
	pr_info("%s lcdc:%d device:%d lcdc_clock:%d\n", __func__,
		get_vioc_index(pDisplayInfo->ddc_info.blk_num), pDisplayInfo->DispDeviceType, vioc_clock_value);

	clk_prepare_enable(pDisplayInfo->vioc_clock);
	clk_prepare_enable(pDisplayInfo->ddc_clock);

	if(pDisplayInfo->DispOrder != DD_MAIN)	{
		ret = request_irq(pDisplayInfo->ddc_info.irq_num, tca_sub_display_handler, IRQF_SHARED,
				"SUB_LCDC", pDisplayInfo);
		if(ret < 0)
			pr_err("Error %s:%d  ret:%d \n", __func__,__LINE__ ,ret);
		else
			_tca_vioc_intr_onoff(ON, pDisplayInfo->ddc_info.irq_num, pDisplayInfo->DispNum);
	}
}

void tca_vioc_displayblock_powerOff(struct tcc_dp_device *pDisplayInfo)
{
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	volatile void __iomem *pDDICONFIG = VIOC_DDICONFIG_GetAddress();
#endif

	pr_info("%s lcdc: %d, type: %d \n", __func__,
		get_vioc_index(pDisplayInfo->ddc_info.blk_num), pDisplayInfo->DispDeviceType);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if( VIOC_CONFIG_DV_GET_EDR_PATH() && pDisplayInfo->DispDeviceType == TCC_OUTPUT_HDMI && pDisplayInfo->DispNum == 0 )
	{
		//vioc_v_dv_block_off(); msleep(30);
		//VIOC_V_DV_SWReset(0);
		_tca_vioc_intr_onoff(OFF, pDisplayInfo->ddc_info.irq_num, pDisplayInfo->DispNum);
		VIOC_V_DV_Power(0);
	}
#endif
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	VIOC_DDICONFIG_SetPeriClock(pDDICONFIG, pDisplayInfo->DispNum, 0);
#endif

	if(pDisplayInfo->DispOrder != DD_MAIN){
		_tca_vioc_intr_onoff(OFF, pDisplayInfo->ddc_info.irq_num, pDisplayInfo->DispNum);
		free_irq(pDisplayInfo->ddc_info.irq_num, pDisplayInfo);
	}

	clk_disable_unprepare(pDisplayInfo->ddc_clock);
	clk_disable_unprepare(pDisplayInfo->vioc_clock);

	if(g2d_filp.private_data != NULL)
		g2d_release((struct inode *)&g2d_inode, (struct file *)&g2d_filp);
	g2d_filp.private_data = NULL;

	if(scaler_filp)
		filp_close(scaler_filp, 0);
	scaler_filp = NULL;
}

void tca_vioc_displayblock_disable(struct tcc_dp_device *pDisplayInfo)
{
	int scaler_num = 0;

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        tca_vioc_displayblock_clock_select(pDisplayInfo, 0);
        #endif

	pDisplayInfo->FbPowerState = false;

	pr_info("%s lcdc:%d onoff:%d\n", __func__,
		get_vioc_index(pDisplayInfo->ddc_info.blk_num),
		VIOC_DISP_Get_TurnOnOff(pDisplayInfo->ddc_info.virt_addr));

	// Disable Display R2Y.
	VIOC_DISP_SetR2Y(pDisplayInfo->ddc_info.virt_addr, 0);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH() && (pDisplayInfo->ddc_info.virt_addr == VIOC_DISP_GetAddress(0)))
	{
		unsigned int status = 0;

		_tca_vioc_intr_onoff(OFF, pDisplayInfo->ddc_info.irq_num, pDisplayInfo->DispNum);
		VIOC_V_DV_GetInterruptPending(VIOC_DV_VEDR_GetAddress(VDV_CFG), &status);
		VIOC_V_DV_ClearInterrupt(VIOC_DV_VEDR_GetAddress(VDV_CFG), status);

		VIOC_V_DV_All_Turnoff();
	}
#endif

#ifdef CONFIG_VIDEO_TCC_VOUT
	VIOC_RDMA_SetImageDisableNW(pDisplayInfo->rdma_info[RDMA_FB].virt_addr);
	VIOC_RDMA_SetImageDisableNW(pDisplayInfo->rdma_info[RDMA_3D].virt_addr);
	if(tcc_vout_get_status(pDisplayInfo->ddc_info.blk_num) != TCC_VOUT_RUNNING) {
		VIOC_RDMA_SetImageDisableNW(pDisplayInfo->rdma_info[RDMA_VIDEO_3D].virt_addr);
		VIOC_RDMA_SetImageDisableNW(pDisplayInfo->rdma_info[RDMA_VIDEO].virt_addr);
	}
#else
	{
		int i = 0;
		for(i = 0; i < RDMA_MAX_NUM; i ++)
			VIOC_RDMA_SetImageDisableNW(pDisplayInfo->rdma_info[i].virt_addr);
	}
#endif

	if(VIOC_DISP_Get_TurnOnOff(pDisplayInfo->ddc_info.virt_addr)) {
		VIOC_DISP_TurnOff(pDisplayInfo->ddc_info.virt_addr);
	#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if( !VIOC_CONFIG_DV_GET_EDR_PATH()
			|| (DV_PATH_VIN_DISP & vioc_get_path_type())
			|| (VIOC_CONFIG_DV_GET_EDR_PATH() && (pDisplayInfo->ddc_info.virt_addr != VIOC_DISP_GetAddress(0)))
		)
	#endif
		{
	#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
			{
				pDisplayInfo->disp_dd_sync.state = 0;
				if(wait_event_interruptible_timeout(pDisplayInfo->disp_dd_sync.waitq, pDisplayInfo->disp_dd_sync.state == 1, msecs_to_jiffies(30)) == 0)
					pr_info("%s: Timeout DISABLE DONE wait for  ms \n", __FUNCTION__);
			}
	#else
			VIOC_DISP_sleep_DisplayDone(pDisplayInfo->ddc_info.virt_addr);
	#endif
		}
	}

	VIOC_CONFIG_SWReset(pDisplayInfo->ddc_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->ddc_info.blk_num, VIOC_CONFIG_CLEAR);

	if(pDisplayInfo->FbUpdateType == FB_MVC_UPDATE) {
		printk("%s : pDisplayInfo->FbUpdateType = %d\n", __func__, pDisplayInfo->FbUpdateType);

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pDisplayInfo->rdma_info[RDMA_3D].blk_num) >= VIOC_SCALER0) //scaler plug in status check
		{
			scaler_num = VIOC_CONFIG_GetScaler_PluginToRDMA(pDisplayInfo->rdma_info[RDMA_3D].blk_num);
			VIOC_CONFIG_PlugOut(scaler_num);
			VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
			printk("%s : Scaler-%d is reset!!!\n", __func__, get_vioc_index(scaler_num));
		}
	}

	if(VIOC_CONFIG_GetScaler_PluginToRDMA(pDisplayInfo->rdma_info[RDMA_FB].blk_num) >= VIOC_SCALER0) //scaler plug in status check
	{
		scaler_num = VIOC_CONFIG_GetScaler_PluginToRDMA(pDisplayInfo->rdma_info[RDMA_FB].blk_num);
		VIOC_CONFIG_PlugOut(scaler_num);
		VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
		printk("%s : UI Scaler-%d is reset!!!\n", __func__, get_vioc_index(scaler_num));
	}

	if(pDisplayInfo->FbUpdateType == FB_DIVIDE_UPDATE)
	{
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pDisplayInfo->rdma_info[RDMA_3D].blk_num) >= VIOC_SCALER0) //scaler plug in status check
		{
			scaler_num = VIOC_CONFIG_GetScaler_PluginToRDMA(pDisplayInfo->rdma_info[RDMA_3D].blk_num);
			VIOC_CONFIG_PlugOut(scaler_num);
			VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
			printk("%s : 3D Scaler-%d is reset!!!\n", __func__, get_vioc_index(scaler_num));
		}
	}

#if defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CVBS)\
	|| defined(CONFIG_TCC_DISPLAY_MODE_DUAL_ALL)|| defined(CONFIG_TCC_DISPLAY_LCD_CVBS)\
	|| defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CC)
		if(pDisplayInfo->FbUpdateType == FB_DIVIDE_UPDATE)
		{
			VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_CLEAR);
		}
#else
	#ifdef CONFIG_VIDEO_TCC_VOUT
		if(tcc_vout_get_status(pDisplayInfo->ddc_info.blk_num) != TCC_VOUT_RUNNING)
		{
			VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_CLEAR);
		}
	#else
		VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_CLEAR);
	#endif
#endif
}

#if 0
static void tca_vioc_displaytiming_dump(struct tcc_dp_device *pDisplayInfo) {
        volatile void __iomem *pDISP = pDisplayInfo->ddc_info.virt_addr;

        volatile unsigned int reg = __raw_readl(pDISP+DCTRL);
        volatile unsigned int val, val1, val2, val3, val4;
        printk("FB Timing Dump\r\n");
        printk(" SYNC VSYNC(%s), HSYNC(%s), DE(%s) PCLK(%s)\r\n",
                (reg & (1<<13))?"Low":"High",  (reg & (1<<12))?"Low":"High", (reg & (1<<14))?"Low":"High", (reg & (1<<1))?"Low":"High");

        // LPC
        val = __raw_readl(pDISP+DHTIME1);
        val &= 0x00003FFF;
        val += 1;

        // LPW - HSYNC WIDTH
        val1 = __raw_readl(pDISP+DHTIME1);
        val1 &= 0x01FF0000;
        val1 >>= 16;
        val1 += 1;

        // LSWC
        val2 = __raw_readl(pDISP+DHTIME2);
        val2 &= 0x01FF0000;
        val2 >>=16;
        val2 += 1;

        //LEWC - HSYNC EDGE DELAY
        val3 = __raw_readl(pDISP+DHTIME2);
        val3 &= 0x000001FF;
        val3 += 1;

        val4 = val1 + val2 +val3;

        printk("HSYNC ACTIVE(%d), DELAY(%d), SYNC(%d), BLANK(%d)\r\n", val,  val3, val1,  val4);


        // FLC
        val = __raw_readl(pDISP+DVTIME1);
        val &= 0x00003FFF;
        val += 1;

        // FPW - VSYNC WIDTH
        val1 = __raw_readl(pDISP+DVTIME1);
        val1 &= 0x3F0000;
        val1 >>= 16;
        val1 += 1;

        // FSWC
        val2 = __raw_readl(pDISP+DVTIME2);
        val2 &= 0x01FF0000;
        val2 >>=16;
        val2 += 1;


        //FEWC - VSYNC EDGE DELAY
        val3 = __raw_readl(pDISP+DVTIME2);
        val3 &= 0x000001FF;
        val3 += 1;

        val4 = val1 + val2 +val3;


        printk("VSYNC ACTIVE(%d), DELAY(%d), SYNC(%d), BLANK(%d)\r\n", val,  val3, val1,  val4);
}
#endif

#if defined(CONFIG_ARCH_TCC)
void tca_vioc_displayblock_extra_set(struct tcc_dp_device *pDisplayInfo, struct  tcc_fb_extra_data *tcc_fb_extra_data)
{
        volatile void __iomem *pDISP;
        #if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
        volatile void __iomem *pRDMA;
        #endif
        unsigned int pxdw_bit;
        if(!pDisplayInfo || !pDisplayInfo->ddc_info.virt_addr) {
                pr_err("%s invalid param\r\n", __func__);
                return;
        }
        pDISP = pDisplayInfo->ddc_info.virt_addr;
        if(tcc_fb_extra_data) {
	#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
			vioc_v_dv_set_output_color_format(tcc_fb_extra_data->pxdw, tcc_fb_extra_data->swapbf);
	#endif
                pxdw_bit = DCTRL_PXDW_MASK >> DCTRL_PXDW_SHIFT;
                if(pxdw_bit <= 0xF) {
                        /* VIOC data bus align is under 10-bit */
                        switch(tcc_fb_extra_data->pxdw) {
                                case 21:
                                        /* "20-bit YCbCr Format * {Y,Cb} a {Y,Cr}" to
                                                "16-bit * YCbCr Format * {Y,Cb} a {Y,Cr}" */
                                        tcc_fb_extra_data->pxdw = 8;
                                        break;
                                case 23:
                                        /* "30-bit RGB101010 Format" to "24-bit RGB888 Format" */
                                        tcc_fb_extra_data->pxdw = 12;
                                        break;
                                case 27:
                                        /* YCbCr 420 Format * {Cb,Y0,Y1} a {Cr,Y2,Y3} is not support */
                                        pr_err("%s pxdw %d is out of ranger\r\n", __func__, tcc_fb_extra_data->pxdw);
                                        tcc_fb_extra_data->pxdw = 0;
                                        break;
                        }
                }
                pr_info("%s pxdw(%d), swapbf(%d), r2ymd(%d), r2y(%d)\r\n",
                        __func__, tcc_fb_extra_data->pxdw, tcc_fb_extra_data->swapbf, tcc_fb_extra_data->r2ymd, tcc_fb_extra_data->r2y);
                VIOC_DISP_SetPXDW(pDISP, tcc_fb_extra_data->pxdw);
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
                VIOC_DISP_SetSwapaf(pDISP, tcc_fb_extra_data->swapbf);
#elif defined(CONFIG_ARCH_TCC897X)
				VIOC_DISP_SetSWAP(pDISP, tcc_fb_extra_data->swapbf);
#endif
                VIOC_DISP_SetR2YMD(pDISP, tcc_fb_extra_data->r2ymd);
                #if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
                VIOC_DISP_SetR2Y(pDISP, 0);
                VIOC_DISP_SetY2R(pDISP, tcc_fb_extra_data->r2y?0:1);
        	pRDMA = pDisplayInfo->rdma_info[RDMA_FB].virt_addr;
                if(pRDMA != NULL) {
                        VIOC_RDMA_SetImageR2YEnable(pRDMA, 1);
                        VIOC_RDMA_SetImageY2REnable(pRDMA, 0);
                        VIOC_RDMA_SetImageR2YMode(pRDMA, tcc_fb_extra_data->r2ymd);
                }
                if(pDisplayInfo->FbUpdateType == FB_DIVIDE_UPDATE || pDisplayInfo->FbUpdateType == FB_MVC_UPDATE) {
                        pRDMA = pDisplayInfo->rdma_info[RDMA_3D].virt_addr;
                        if(pRDMA != NULL) {
                                VIOC_RDMA_SetImageR2YEnable(pRDMA, 1);
                                VIOC_RDMA_SetImageY2REnable(pRDMA, 0);
                                VIOC_RDMA_SetImageR2YMode(pRDMA, tcc_fb_extra_data->r2ymd);
                        }
                }
                #else
                VIOC_DISP_SetY2R(pDISP, 0);
                VIOC_DISP_SetR2Y(pDISP, tcc_fb_extra_data->r2y);
                #endif
        }
}
#endif

void tca_vioc_displayblock_timing_set(unsigned int outDevice, struct tcc_dp_device *pDisplayInfo, struct lcdc_timimg_parms_t *mode)
{
	unsigned int width, height;
	volatile void __iomem *pDISP = pDisplayInfo->ddc_info.virt_addr;
	volatile void __iomem *pWMIX = pDisplayInfo->wmixer_info.virt_addr;
	stLCDCTR stCtrlParam;
	stLTIMING stTimingParam;
	unsigned int rdma_en = 0;

	#if defined(CONFIG_VIOC_AFBCDEC)
	afbc_dec_1st_cfg = 0;
	#endif
	
        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        tca_vioc_displayblock_clock_select(pDisplayInfo, 0);
        #endif

        #ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	dv_reg_phyaddr = dv_md_phyaddr = 0x00;
	//pr_info("#### DV EDR Mode ? format(%d), reg(0x%x)/meta(0x%x), outDevice(%d)/Disp_0(%p =? %p)\n",
	//			mode->format, mode->dv_reg_phyaddr, mode->dv_md_phyaddr,
	//			outDevice, pDISP, VIOC_DISP_GetAddress(0));
	if((outDevice == VIOC_OUTCFG_HDMI) && (pDISP == VIOC_DISP_GetAddress(0)))
	{
		if(mode->dv_reg_phyaddr)
		{
			pr_info("#### DV EDR Mode!!! format(%d) phy(0x%x) noYUV422(%d) vsvdb(0x%x) LL(%d)\n",
						mode->format, mode->dv_reg_phyaddr, mode->dv_noYUV422_SDR, mode->dv_vsvdb_size, mode->dv_ll_mode);
			voic_v_dv_set_hdmi_timming(mode, 1, mode->dv_hdmi_clk_khz);
			vioc_v_dv_set_stage(DV_STANDBY);
			vioc_v_dv_set_mode(mode->dv_ll_mode, (unsigned char*)mode->dv_vsvdb, mode->dv_vsvdb_size);
			VIOC_V_DV_Power(1);
		}
		else
		{
			VIOC_CONFIG_DV_SET_EDR_PATH(0);
			pr_info("#### Non-DV EDR Mode!!! stage(%d) mode(0x%x) path(%d) out(%d)\n", vioc_v_dv_get_stage(), vioc_v_dv_get_mode(), vioc_get_path_type(), vioc_get_out_type());
		}
	}
#endif

        #if defined(CONFIG_FB_REG_DUMP)
        pr_info("DUMP FB \r\n");
        pr_info(" id     = %d\r\n", mode->id);
        pr_info(" iv     = %d\r\n", mode->iv);
        pr_info(" ih     = %d\r\n", mode->ih);
        pr_info(" ip     = %d\r\n", mode->ip);
        pr_info(" dp     = %d\r\n", mode->dp);
        pr_info(" ni     = %d\r\n", mode->ni);
        pr_info(" tv     = %d\r\n", mode->tv);
        pr_info(" tft    = %d\r\n", mode->tft);
        pr_info(" stn    = %d\r\n", mode->stn);
        pr_info(" lpw    = %d\r\n", mode->lpw);
        pr_info(" lpc    = %d\r\n", mode->lpc);
        pr_info(" lswc   = %d\r\n", mode->lswc);
        pr_info(" lewc   = %d\r\n", mode->lewc);
        pr_info(" vdb    = %d\r\n", mode->vdb);
        pr_info(" vdf    = %d\r\n", mode->vdf);
        pr_info(" fpw    = %d\r\n", mode->fpw);
        pr_info(" flc    = %d\r\n", mode->flc);
        pr_info(" fswc   = %d\r\n", mode->fswc);
        pr_info(" fewc   = %d\r\n", mode->fewc);
        pr_info(" fpw2   = %d\r\n", mode->fpw2);
        pr_info(" flc2   = %d\r\n", mode->flc2);
        pr_info(" fswc2  = %d\r\n", mode->fswc2);
        pr_info(" fewc2  = %d\r\n", mode->fewc2);
        #endif


	if(mode->dp) {
		width = (mode->lpc + 1)/2;
		height = mode->flc + 1;
	}
	else	{
		width = (mode->lpc + 1);
		height = mode->flc + 1;
	}

        pr_info("%s width=%d, height=%d\r\n", __func__, width, height);

	if(pDisplayInfo->DispDeviceType == TCC_OUTPUT_COMPOSITE)
		width = width/2;

	stTimingParam.lpw= mode->lpw;
	stTimingParam.lpc= mode->lpc + 1;
	stTimingParam.lswc= mode->lswc+ 1;
	stTimingParam.lewc= mode->lewc+ 1;

	stTimingParam.vdb = mode->vdb;
	stTimingParam.vdf = mode->vdf;
	stTimingParam.fpw = mode->fpw;
	stTimingParam.flc = mode->flc;
	stTimingParam.fswc = mode->fswc;
	stTimingParam.fewc = mode->fewc;
	stTimingParam.fpw2 = mode->fpw2;
	stTimingParam.flc2 = mode->flc2;
	stTimingParam.fswc2 = mode->fswc2;
	stTimingParam.fewc2 = mode->fewc2;

	VIOC_CONFIG_SWReset(pDisplayInfo->ddc_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->ddc_info.blk_num, VIOC_CONFIG_CLEAR);
	VIOC_DISP_SetTimingParam(pDISP, &stTimingParam);

	memset(&stCtrlParam, 0x00, sizeof(stCtrlParam));
	stCtrlParam.id= mode->id;
	stCtrlParam.iv= mode->iv;
	stCtrlParam.ih= mode->ih;
	stCtrlParam.ip= mode->ip;
	stCtrlParam.clen = 0;
	stCtrlParam.r2y = 0;
	//stCtrlParam.pxdw = 12; //RGB888
	stCtrlParam.dp = mode->dp;
	stCtrlParam.ni = mode->ni;

#if defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
    if(mode->ni == 0)
            stCtrlParam.advi = 0;
#else
    #if defined(CONFIG_ARCH_TCC898X)
    stCtrlParam.advi = mode->ni;
    #else
    if(mode->ni == 0)
            stCtrlParam.advi = 1;
    #endif
#endif
	stCtrlParam.tv = mode->tv;

	#if 0//defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) : pjj delete please
		if(output_starter_state || hdmi_get_hdmimode() == DVI)
			stCtrlParam.pxdw = 12; //RGB888
		else
			stCtrlParam.pxdw = 8; //YCbCr
	#else
		stCtrlParam.pxdw = 12; //RGB888
	#endif

	VIOC_DISP_SetControlConfigure(pDISP, &stCtrlParam);

        #if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	VIOC_DISP_SetSwapbf(pDISP, 0);
        #endif
	VIOC_DISP_SetSize (pDISP, width, height);
	VIOC_DISP_SetBGColor(pDISP, 0, 0, 0, 1);

	// prevent display under-run
	VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_CLEAR);

#if 0//defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) : pjj delete please
	if(output_starter_state || hdmi_get_hdmimode() == DVI)
		VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x00, 0x00, 0xff);
	else
		VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x80, 0x80, 0x00);
#else
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x00, 0x00, 0x3ff);
	#endif
#endif

#if defined(CONFIG_TCC_REAR_CAMERA_DRV)
	if(get_vioc_index(pDisplayInfo->ddc_info.blk_num) == DD_MAIN)
    	printk(" \r\n\r\n%s (%d) SKIP OVP\r\n", __func__, __LINE__);
	else
#endif
	{
		VIOC_WMIX_SetOverlayPriority(pWMIX, 24);
	}
#if defined(CONFIG_PLATFORM_AVN) && !defined(CONFIG_ANDROID)
	if(pDisplayInfo->wmixer_info.blk_num == VIOC_WMIX1)
		VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 0);
#endif
	VIOC_WMIX_SetSize(pWMIX, width, height);
	VIOC_WMIX_SetUpdate (pWMIX);

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if((vioc_v_dv_get_stage() != DV_OFF)
		&& (outDevice == VIOC_OUTCFG_HDMI)
		&& (get_vioc_index(pDisplayInfo->ddc_info.blk_num) == DD_MAIN)
		&& (DV_PATH_DIRECT & vioc_get_path_type())
	)
	{
		VIOC_OUTCFG_SetOutConfig(outDevice, VIOC_OUTCFG_V_DV);
	}
	else
#endif
		VIOC_OUTCFG_SetOutConfig(outDevice, pDisplayInfo->ddc_info.blk_num);

	VIOC_RDMA_GetImageEnable(pDisplayInfo->rdma_info[RDMA_FB].virt_addr, &rdma_en);
	if(rdma_en)
		VIOC_RDMA_SetImageDisable(pDisplayInfo->rdma_info[RDMA_FB].virt_addr);

#if	defined(CONFIG_HDMI_FB_ROTATE_90)||defined(CONFIG_HDMI_FB_ROTATE_180)||defined(CONFIG_HDMI_FB_ROTATE_270)
	pDisplayInfo->FbUpdateType = FB_SC_G2D_RDMA_UPDATE;
#else
	#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) || defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
		#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		pDisplayInfo->FbUpdateType = FB_SC_RDMA_UPDATE;
		#else
		pDisplayInfo->FbUpdateType = FB_SC_M2M_RDMA_UPDATE; /* YUV output */
		#endif
	#else
		#ifdef CONFIG_TCC_EXTFB
		if(outDevice == VIOC_OUTCFG_HDMI)
		{
			pDisplayInfo->FbUpdateType = FB_RDMA_UPDATE;
		}
		else
		#endif
		{
			pDisplayInfo->FbUpdateType = FB_SC_RDMA_UPDATE;
		}
	#endif
#endif

	printk("mode->framepacking = %d\n", mode->framepacking);

	if(mode->framepacking == 1) {
		//pDisplayInfo->MVCMode = 1;
		pDisplayInfo->FbUpdateType = FB_MVC_UPDATE;
		printk(" >>FB_MVC_UPDATE\r\n");
	}
	else if(mode->framepacking == 2 || mode->framepacking == 3) {
		// If mode->framepacking is 2 then SBS
		// If mode->framepacking is 3 then TAB
		divide_data.enable = 1;

		// divide_data.mode is set 0 in SBS
		// divide_data.mode is set 1 in TAB
		divide_data.mode = mode->framepacking-2;
		//divide_data.fbtype = FB_SC_M2M_RDMA_UPDATE;

		pDisplayInfo->FbUpdateType = FB_DIVIDE_UPDATE;

		printk(" >>FB_DIVIDE_UPDATE\r\n");
	}
	else {
		memset(&divide_data, 0, sizeof(divide_data));
	}

// patch for stb presentation window.
	if(pDisplayInfo->FbUpdateType == FB_SC_RDMA_UPDATE) {
		pDisplayInfo->sc_num0 = AnD_FB_SC;
	}
	else if(pDisplayInfo->FbUpdateType == FB_MVC_UPDATE || pDisplayInfo->FbUpdateType == FB_DIVIDE_UPDATE) {
		pDisplayInfo->sc_num0 = AnD_FB_SC;
		pDisplayInfo->sc_num1 = DIV_FB_SC;
	}


	VIOC_CONFIG_PlugOut(pDisplayInfo->sc_num0);
	VIOC_CONFIG_SWReset(pDisplayInfo->sc_num0, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->sc_num0, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);

	if(pDisplayInfo->FbUpdateType == FB_SC_RDMA_UPDATE)	{
		VIOC_CONFIG_PlugIn(pDisplayInfo->sc_num0, pDisplayInfo->rdma_info[RDMA_FB].blk_num);
	}
	else  if(pDisplayInfo->FbUpdateType == FB_SC_M2M_RDMA_UPDATE) {
		if (!scaler_filp) {
			scaler_filp = filp_open(DEV_SCALER1, O_RDWR, 0666);
			if (IS_ERR(scaler_filp))
				scaler_filp = NULL;
		}
	}
	else if(pDisplayInfo->FbUpdateType == FB_SC_G2D_RDMA_UPDATE)	{
		g2d_open((struct inode *)&g2d_inode, (struct file *)&g2d_filp);
		if (!scaler_filp) {
			scaler_filp = filp_open(DEV_SCALER1, O_RDWR, 0666);
			if (IS_ERR(scaler_filp))
				scaler_filp = NULL;
		}
	}
	else if(pDisplayInfo->FbUpdateType == FB_DIVIDE_UPDATE || pDisplayInfo->FbUpdateType == FB_MVC_UPDATE) {
		VIOC_CONFIG_PlugOut(pDisplayInfo->sc_num1);
		VIOC_CONFIG_SWReset(pDisplayInfo->sc_num1, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pDisplayInfo->sc_num1, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_CLEAR);

		if(pDisplayInfo->FbUpdateType == FB_DIVIDE_UPDATE && divide_data.fbtype == FB_SC_M2M_RDMA_UPDATE) {
			if (!scaler_filp) {
				printk("Prepare Scaler driver with SC1\r\n");
				scaler_filp = filp_open(DEV_SCALER1, O_RDWR, 0666);
				if (IS_ERR(scaler_filp)) {
					scaler_filp = NULL;
					printk("Failed Open SC1 \r\n");
				}
			}
		}
		else {
			VIOC_CONFIG_PlugIn(pDisplayInfo->sc_num0, pDisplayInfo->rdma_info[RDMA_FB].blk_num);
			VIOC_CONFIG_PlugIn(pDisplayInfo->sc_num1, pDisplayInfo->rdma_info[RDMA_3D].blk_num);
		}
	}

	pDisplayInfo->FbPowerState = true;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(vioc_v_dv_get_stage() != DV_OFF && pDisplayInfo->DispNum == 0)
	{
		dv_reg_phyaddr = mode->dv_reg_phyaddr;
		dv_md_phyaddr = mode->dv_md_phyaddr;
	}
	else
#endif
	{
		_tca_vioc_intr_onoff(ON, pDisplayInfo->ddc_info.irq_num, pDisplayInfo->DispNum);
	}

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        tca_vioc_displayblock_clock_select(pDisplayInfo, 1);
        #endif

	//tca_vioc_displaytiming_dump(pDisplayInfo);
	pr_info("%s displayN:%d non-interlaced:%d w:%d h:%d FbUpdateType:%d \n",
		__func__, get_vioc_index(pDisplayInfo->ddc_info.blk_num),
		mode->ni, width, height, pDisplayInfo->FbUpdateType);
}

void tca_vioc_displayblock_ctrl_set(unsigned int outDevice,
								struct tcc_dp_device *pDisplayInfo,
								stLTIMING *pstTiming,
								stLCDCTR *pstCtrl)
{
        unsigned int rdma_en;
        int skip_display_device = 0;
	unsigned int width, height;
        volatile void __iomem *pDISP = pDisplayInfo->ddc_info.virt_addr;
        volatile void __iomem *pWMIX = pDisplayInfo->wmixer_info.virt_addr;


        if(pstTiming == NULL || pstCtrl == NULL) {
                skip_display_device = 1;
        }

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        if(!skip_display_device) {
                tca_vioc_displayblock_clock_select(pDisplayInfo, 0);
        }
        #endif

        #if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
        if(skip_display_device) {
                /* Disable Bootloader RDMA */
                VIOC_RDMA_GetImageEnable(pDisplayInfo->rdma_info[RDMA_FB1].virt_addr, &rdma_en);
                if(rdma_en)
                        VIOC_RDMA_SetImageDisable(pDisplayInfo->rdma_info[RDMA_FB1].virt_addr);
                VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB1].blk_num, VIOC_CONFIG_RESET);
	        VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB1].blk_num, VIOC_CONFIG_CLEAR);
        }
        #endif

        VIOC_RDMA_GetImageEnable(pDisplayInfo->rdma_info[RDMA_FB].virt_addr, &rdma_en);
        if(rdma_en)
                VIOC_RDMA_SetImageDisable(pDisplayInfo->rdma_info[RDMA_FB].virt_addr);

        if(skip_display_device) {
                VIOC_DISP_GetSize(pDISP, &width, &height);
        } else {
        	if(pstCtrl->dp) {
        		width = pstTiming->lpc / 2;
        		height = pstTiming->flc + 1;
        	} else {
        		width = pstTiming->lpc;
        		height = pstTiming->flc + 1;
        	}
        	if (TCC_OUTPUT_COMPOSITE == pDisplayInfo->DispDeviceType) {
        		width = width / 2;
        	}

        	VIOC_CONFIG_SWReset(pDisplayInfo->ddc_info.blk_num, VIOC_CONFIG_RESET);
        	VIOC_CONFIG_SWReset(pDisplayInfo->ddc_info.blk_num, VIOC_CONFIG_CLEAR);
        	VIOC_DISP_SetTimingParam(pDISP, pstTiming);
        	VIOC_DISP_SetControlConfigure(pDISP, pstCtrl);

			#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
        	if (outDevice == VIOC_OUTCFG_HDMI) {
        		VIOC_DISP_SetAlign(pDISP, 0);
        	} else {
        		if (TCC_OUTPUT_COMPOSITE == pDisplayInfo->DispDeviceType) {
        			#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
        			width = 720;						// 720
        			VIOC_DISP_SetAlign(pDISP, 0);		// 10 bits
        			VIOC_DISP_SetSwapaf(pDISP, 0x4);	// V-Y-U (B-R-G)
        			#else
        			VIOC_DISP_SetAlign(pDISP, 1);		// Composite tvo
        			#endif
        		} else {
        			VIOC_DISP_SetAlign(pDISP, 1);		// Component
        		}
        	}
        	#endif

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
			VIOC_DISP_SetSwapbf(pDISP, 0);
#endif
        	VIOC_DISP_SetSize(pDISP, width, height);
        	VIOC_DISP_SetBGColor(pDISP, 0, 0, 0, 1);
        	VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_RESET);
        	VIOC_CONFIG_SWReset(pDisplayInfo->wmixer_info.blk_num, VIOC_CONFIG_CLEAR);
        }
	// prevent display under-run
	VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
	VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(pDisplayInfo->wdma_info.blk_num, VIOC_CONFIG_CLEAR);

        #if 0//defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) : pjj delete please
	if(output_starter_state || hdmi_get_hdmimode() == DVI)
		VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x00, 0x00, 0xff);
	else
		VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x80, 0x80, 0x00);
        #else
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x00, 0x00, 0x3ff);
	#else
	VIOC_WMIX_SetBGColor(pWMIX, 0x00, 0x00, 0x00, 0xff);
	#endif
        #endif

        #if defined(CONFIG_TCC_VIOCMG)
	if(get_vioc_index(pDisplayInfo->ddc_info.blk_num) == DD_MAIN)
                viocmg_set_wmix_ovp(VIOCMG_CALLERID_FB, pDisplayInfo->wmixer_info.blk_num, viocmg_get_main_display_ovp());
	else
        #endif
	{
                VIOC_WMIX_SetOverlayPriority(pWMIX, 24);
	}
	VIOC_WMIX_SetSize(pWMIX, width, height);
	VIOC_WMIX_SetUpdate(pWMIX);

    if(!skip_display_device) {
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
    	if((vioc_v_dv_get_stage() != DV_OFF)
    		&& (outDevice == VIOC_OUTCFG_HDMI)
    		&& (get_vioc_index(pDisplayInfo->ddc_info.blk_num) == DD_MAIN)
    		&& (DV_PATH_DIRECT & vioc_get_path_type())
    	) {
    		VIOC_OUTCFG_SetOutConfig(outDevice, VIOC_OUTCFG_V_DV);
    	}
    	else
#endif
            VIOC_OUTCFG_SetOutConfig(outDevice, pDisplayInfo->ddc_info.blk_num);
    }

	if(pDisplayInfo->FbUpdateType != FB_ATTACH_UPDATE)
	{
		#if	defined(CONFIG_HDMI_FB_ROTATE_90)||defined(CONFIG_HDMI_FB_ROTATE_180)||defined(CONFIG_HDMI_FB_ROTATE_270)
			pDisplayInfo->FbUpdateType = FB_SC_G2D_RDMA_UPDATE;
		#else
			#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) || defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
				#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
				pDisplayInfo->FbUpdateType = FB_SC_RDMA_UPDATE;
				#else
				pDisplayInfo->FbUpdateType = FB_SC_M2M_RDMA_UPDATE; /* YUV output */
				#endif
			#else
				if(pDisplayInfo->FbUpdateType != FB_RDMA_UPDATE)
					pDisplayInfo->FbUpdateType = FB_SC_RDMA_UPDATE;
			#endif
		#endif
		// patch for stb presentation window.
		if(pDisplayInfo->FbUpdateType == FB_SC_RDMA_UPDATE) {
			if(pDisplayInfo->DispOrder == 0)
				pDisplayInfo->sc_num0 = AnD_FB_SC;
			#ifdef CONFIG_COMPONENT_PRESENTATION_WINDOW_DISPLAY_RESIZE
			else if(pDisplayInfo->DispOrder == 1)
				pDisplayInfo->sc_num0 = VIOC_SCALER4;
			#endif//
		}
		else if(pDisplayInfo->FbUpdateType == FB_MVC_UPDATE) {
			pDisplayInfo->sc_num0 = AnD_FB_SC;
			pDisplayInfo->sc_num1 = DIV_FB_SC;
		}

		if(pDisplayInfo->FbUpdateType == FB_SC_RDMA_UPDATE) {
			VIOC_CONFIG_PlugOut(pDisplayInfo->sc_num0);
			// prevent display under-run
			VIOC_CONFIG_SWReset(pDisplayInfo->sc_num0, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->sc_num0, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pDisplayInfo->rdma_info[RDMA_FB].blk_num, VIOC_CONFIG_CLEAR);
			VIOC_CONFIG_PlugIn(pDisplayInfo->sc_num0, pDisplayInfo->rdma_info[RDMA_FB].blk_num);
		}
		else  if(pDisplayInfo->FbUpdateType == FB_SC_M2M_RDMA_UPDATE) {
			if (!scaler_filp) {
				scaler_filp = filp_open(DEV_SCALER1, O_RDWR, 0666);
				if (IS_ERR(scaler_filp))
					scaler_filp = NULL;
			}
		}
		else if(pDisplayInfo->FbUpdateType == FB_SC_G2D_RDMA_UPDATE)	{
			g2d_open((struct inode *)&g2d_inode, (struct file *)&g2d_filp);
			if (!scaler_filp) {
				scaler_filp = filp_open(DEV_SCALER1, O_RDWR, 0666);
				if (IS_ERR(scaler_filp))
					scaler_filp = NULL;
			}
		}
	}

	pDisplayInfo->FbPowerState = true;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(vioc_v_dv_get_stage() == DV_OFF || (vioc_v_dv_get_stage() != DV_OFF && pDisplayInfo->DispNum != 0))
#endif
	{
		_tca_vioc_intr_onoff(ON, pDisplayInfo->ddc_info.irq_num, pDisplayInfo->DispNum);
	}

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        tca_vioc_displayblock_clock_select(pDisplayInfo, 1);
        #endif

        if(!skip_display_device) {
        	pr_info("%s displayN:%d non-interlaced:%d w:%d h:%d FbUpdateType:%d \n",
        		__func__, get_vioc_index(pDisplayInfo->ddc_info.blk_num), pstCtrl->ni, width, height, pDisplayInfo->FbUpdateType);
        }
}

unsigned int chromaY = 0x00;
unsigned int chromaU = 0x80;
unsigned int chromaV = 0x80;

#if defined(CONFIG_SUPPORT_2D_COMPRESSION)
static void tca_vioc_configure_DEC100(unsigned int base_addr, struct fb_var_screeninfo *var, volatile void __iomem *pRDMA)
{
	unsigned char bSet_Comp = false;
	unsigned int nAlpha = 0xF;
	unsigned int ts_address = base_addr + (var->xres * var->yres * (var->bits_per_pixel/8));

	//var->reserved[2] = //Compressed mode...
	//var->reserved[3] = //1 if it has alpha, otherwise 0...
	//#define ALIGNED(len, mul) ( ( (unsigned int)len + (mul-1) ) & ~(mul-1) )

	if( CONFIG_ADDITIONAL_TS_BUFFER_LINES != 0 )
	{
#if defined(CONFIG_ANDROID)
		if(var->reserved[3] != 0 ) {
			bSet_Comp = true;
			nAlpha = 0xF;
		}
#else
		if( var->reserved[2] != 0 ) {
			unsigned int *remap_addr;

			remap_addr = (unsigned int *)ioremap_nocache(ts_address, 4096);
			if( remap_addr != NULL
				&& remap_addr[0] != 0xffffffff
				&& tcc_2d_compression_read()
			) {
				bSet_Comp = true;
			}
			iounmap((void*)remap_addr);
			nAlpha = var->reserved[3];
		}
#endif
	}

	if(bSet_Comp)
	{
		VIOC_RDMA_DEC_CTRL(pRDMA, ts_address, (var->xres*var->yres/64)/2, nAlpha, 0xf);
		VIOC_RDMA_DEC_EN(pRDMA, 1); // decompress mode : 1, bypass mode : 0
		//printk("======> FBC for DEC100 - 0x%x/0x%x - %d \n", base_addr, ts_address, nAlpha);
	}
	else {
		VIOC_RDMA_DEC_EN(pRDMA, 0);
	}
}
#endif

#if defined(CONFIG_VIOC_AFBCDEC)
static void
tca_vioc_configure_AFBCDEC(unsigned int vioc_dec_id, unsigned int base_addr,
			   volatile void __iomem *pRDMA,
			   volatile void __iomem *pWDMA, unsigned int fmt,
			   unsigned int rdmaPath, unsigned char bSet_Comp,
			   unsigned int bSplitMode, unsigned int bWideMode,
			   unsigned int width, unsigned int height)
{
	volatile void __iomem *pAFBC_Dec = VIOC_AFBCDec_GetAddress(vioc_dec_id);

	if(bSet_Comp)
	{
		if(!afbc_dec_1st_cfg){
			if (VIOC_WDMA_IsImageEnable(pWDMA) && VIOC_WDMA_IsContinuousMode(pWDMA)) {
				attach_data.flag = 0;
				afbc_wdma_need_on = 1;
				VIOC_WDMA_SetImageDisable(pWDMA);
			}
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_CONFIG_AFBCDECPath(vioc_dec_id, rdmaPath, 1);
			VIOC_AFBCDec_SurfaceCfg(pAFBC_Dec, base_addr, fmt, width, height, 0, bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0, 1);
			VIOC_AFBCDec_SetContiDecEnable(pAFBC_Dec, 1);
			VIOC_AFBCDec_SetSurfaceN(pAFBC_Dec, VIOC_AFBCDEC_SURFACE_0, 1);
			VIOC_AFBCDec_SetIrqMask(pAFBC_Dec, 1, AFBCDEC_IRQ_ALL);
			VIOC_AFBCDec_TurnOn(pAFBC_Dec, VIOC_AFBCDEC_SWAP_DIRECT);
			afbc_dec_1st_cfg = 1;
		}

		else{
			VIOC_AFBCDec_SurfaceCfg(pAFBC_Dec, base_addr, fmt, width, height, 0, bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0, 0);
			VIOC_AFBCDec_TurnOn(pAFBC_Dec, VIOC_AFBCDEC_SWAP_PENDING);
			afbc_dec_1st_cfg++;
		}
	}
	else {
		if(afbc_dec_1st_cfg){
			if (VIOC_WDMA_IsImageEnable(pWDMA) && VIOC_WDMA_IsContinuousMode(pWDMA)) {
				attach_data.flag = 0;
				afbc_wdma_need_on = 1;
				VIOC_WDMA_SetImageDisable(pWDMA);
			}
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_AFBCDec_TurnOFF(pAFBC_Dec);
			VIOC_CONFIG_AFBCDECPath(vioc_dec_id, rdmaPath, 0);

			//VIOC_CONFIG_SWReset(vioc_dec_id, VIOC_CONFIG_RESET);
			//VIOC_CONFIG_SWReset(vioc_dec_id, VIOC_CONFIG_CLEAR);
			afbc_dec_1st_cfg = 0;
		}
	}
}
#endif

void tca_fb_rdma_active_var(unsigned int base_addr, struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
	unsigned int  fmt = 0;
	unsigned int lcd_width, lcd_height, img_width, img_height;

	unsigned int alpha_type = 0, alpha_blending_en = 0;
	unsigned int chromaR, chromaG, chromaB, chroma_en = 0;
	unsigned int lcd_pos_x = 0, lcd_pos_y = 0, lcd_layer = RDMA_FB;
	unsigned int blk_num=pdp_data->rdma_info[RDMA_FB].blk_num;

	volatile void __iomem *pRDMA = pdp_data->rdma_info[RDMA_FB].virt_addr;
	volatile void __iomem *pWMIX = pdp_data->wmixer_info.virt_addr;
	#if defined(CONFIG_VIOC_AFBCDEC)
	volatile void __iomem *pWDMA = pdp_data->wdma_info.virt_addr;
	#endif

	if(var->bits_per_pixel == 32) 	{
		chroma_en = UI_CHROMA_EN;
		alpha_type = 1;
		alpha_blending_en = 1;
		fmt = TCC_LCDC_IMG_FMT_RGB888;
	}
	else if(var->bits_per_pixel == 16)	{
		chroma_en = 1;
		alpha_type = 0;
		alpha_blending_en = 0;
		fmt = TCC_LCDC_IMG_FMT_RGB565;
	}
	else	{
		pr_err("%s: bits_per_pixel : %d Not Supported BPP!\n", __FUNCTION__, var->bits_per_pixel);
	}

	chromaR = chromaG = chromaB = 0;

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	//pr_info("%s: %d base_addr:0x%x w:%d h:%d offset:%d lcd:%d - %d \n", __FUNCTION__, pdp_data->ddc_info.blk_num, base_addr, var->xres, var->yres, var->yoffset, lcd_width, lcd_height);

	img_width = var->xres;
	img_height = var->yres;

	if(img_width > lcd_width)
		img_width = lcd_width;

	if(img_height > lcd_height)
		img_height = lcd_height;

	/* display 3d ui : side-by-side, top-and-bottom */
	if(divide_data.enable)
	{
		if(divide_data.count == 0)
		{
			pRDMA		= pdp_data->rdma_info[RDMA_FB].virt_addr;
			blk_num		=pdp_data->rdma_info[RDMA_FB].blk_num;
			lcd_layer	= RDMA_FB;
		}
		else
		{
			if(divide_data.enable && divide_data.count == 1){
				base_addr = __raw_readl(pRDMA+RDMABASE0);
			}
			pRDMA		= pdp_data->rdma_info[RDMA_3D].virt_addr;
			blk_num		=pdp_data->rdma_info[RDMA_3D].blk_num;
			lcd_layer	= RDMA_3D;
		}

		/* side-by-side : left/right */
		if(divide_data.mode == 0)
		{
			lcd_width	= lcd_width/2;

			if(divide_data.count == 0)
				lcd_pos_x	= lcd_pos_x/2;
			else
				lcd_pos_x	= lcd_pos_x/2 + lcd_width;
		}
		/* top-and-bottom : top/bottom */
		else
		{
			lcd_height	= lcd_height/2;

			if(divide_data.count == 0)
				lcd_pos_y	= lcd_pos_y/2;
			else
				lcd_pos_y	= lcd_pos_y/2 + lcd_height;
		}
	}

	#if defined(CONFIG_VIOC_AFBCDEC) // to disable
    if (var->reserved[3] == 0) {
        tca_vioc_configure_AFBCDEC(afbc_dec_vioc_id, base_addr, pRDMA, pWDMA, fmt, blk_num, var->reserved[3], 1, 0, img_width, img_height);
       //TCC803X Android - change UI layer to layer1, disable layer0, AFBCDec enable - prevent FIFO under-run
            #if defined(CONFIG_ARCH_TCC803X) && defined (CONFIG_ANDROID)
            pRDMA           = pdp_data->rdma_info[RDMA_FB1].virt_addr;
            blk_num         = pdp_data->rdma_info[RDMA_FB1].blk_num;
            lcd_layer       = RDMA_FB1;

            VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[RDMA_FB].virt_addr);
            #endif
       }
	#endif

	// default framebuffer
	VIOC_WMIX_SetPosition(pWMIX, lcd_layer, lcd_pos_x, lcd_pos_y);
	//overlay setting
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetChromaKey(pWMIX, lcd_layer, chroma_en, chromaR, chromaG, chromaB, 0x3FF, 0x3FF, 0x3FF);
	#else
	VIOC_WMIX_SetChromaKey(pWMIX, lcd_layer, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);
	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	VIOC_WMIX_SetChromaKey(pWMIX, lcd_layer, chroma_en, chromaY, chromaU, chromaV, 0xF8, 0xFC, 0xF8);
	#endif
	#endif//

	#if defined(CONFIG_SUPPORT_2D_COMPRESSION)
	tca_vioc_configure_DEC100(base_addr, var, pRDMA);
	#endif

	VIOC_RDMA_SetImageFormat(pRDMA, fmt);					//fmt

	VIOC_RDMA_SetImageOffset(pRDMA, fmt, img_width);		//offset
	VIOC_RDMA_SetImageSize(pRDMA, img_width, img_height);	//size
#if defined(CONFIG_VIOC_AFBCDEC)
    if(!afbc_dec_1st_cfg)
        VIOC_RDMA_SetImageBase(pRDMA, base_addr, 0, 0);
#if !defined(CONFIG_ANDROID) && defined(CONFIG_PLATFORM_STB)
    if(var->reserved[3] != 0 )
        VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0x5);//BGR	
    else
	VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0x0);//RGB
#endif
#else
    VIOC_RDMA_SetImageBase(pRDMA, base_addr, 0, 0);
#endif

	VIOC_RDMA_SetImageAlphaSelect(pRDMA, alpha_type);
	VIOC_RDMA_SetImageAlphaEnable(pRDMA, alpha_blending_en);

	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	VIOC_RDMA_SetImage3DMode(pRDMA, 0);
	VIOC_RDMA_SetImageRBase(pRDMA, 0, 0, 0);
	#endif

	VIOC_WMIX_SetUpdate(pWMIX);
	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	VIOC_RDMA_SetImageY2REnable(pRDMA, 0);
	VIOC_RDMA_SetImageR2YEnable(pRDMA, 1);

	//	VIOC_RDMA_SetImageAlpha(pRDMA, 0x80, 0x80);
	//	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 0);
	//	VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
	#endif

	#if defined(CONFIG_VIOC_AFBCDEC)
	tca_vioc_configure_AFBCDEC(afbc_dec_vioc_id, base_addr, pRDMA, pWDMA, fmt, blk_num, var->reserved[3], 1, 0, img_width, img_height);

	if(var->reserved[3] != 0 )
		VIOC_RDMA_SetIssue(pRDMA, 7, 16);
	else
	#endif
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	{
		VIOC_RDMA_SetIssue(pRDMA, 15, 16);
	}
#else
	{
		//TODO: other SoC code?
	}
#endif

	VIOC_RDMA_SetImageEnable(pRDMA);

	#if defined(CONFIG_VIOC_AFBCDEC)
	if (afbc_wdma_need_on) {
		VIOC_WDMA_SetImageEnable(pWDMA, 1);
		attach_data.flag = 1;
		afbc_wdma_need_on = 0;
	}
	#endif
//temp pjj because fifo under have to enable rdma before display device on.
//	if(pdp_data->DispOrder == DD_MAIN)
//		VIOC_DISP_TurnOn(pdp_data->ddc_info.virt_addr);
}

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
void tca_fb_rdma_active_var_rbase(struct tcc_dp_device *pdp_data)
{
	unsigned int rdma_3d_mode;
	volatile void __iomem *pRDMA = pdp_data->rdma_info[RDMA_FB].virt_addr;

	if(divide_data.mode == 0){
		rdma_3d_mode = 1;
	}
	else {
		rdma_3d_mode = 2;
	}
	VIOC_RDMA_SetImageRBase(pRDMA, __raw_readl(pRDMA+RDMABASE0), __raw_readl(pRDMA+RDMABASE1), __raw_readl(pRDMA+RDMABASE2));
	VIOC_RDMA_SetImage3DMode(pRDMA, rdma_3d_mode);
	VIOC_RDMA_SetImageEnable(pRDMA);
}
#endif

void tca_fb_sc_rdma_active_var(unsigned int base_addr, struct fb_var_screeninfo *var,struct tcc_dp_device *pdp_data)
{
	unsigned int  fmt = 0,  interlace_output = 0;
	unsigned int lcd_width, lcd_height, img_width, img_height;
	unsigned int lcd_pos_x = 0, lcd_pos_y = 0, lcd_layer = RDMA_FB;

	unsigned int alpha_type = 0, alpha_blending_en = 0;
	unsigned int chromaR, chromaG, chromaB, chroma_en = 0;
  	tcc_display_resize *pResize= &resize_data;
	volatile void __iomem *pSC;
	unsigned int blk_num=pdp_data->rdma_info[RDMA_FB].blk_num;
	volatile void __iomem *pRDMA = pdp_data->rdma_info[RDMA_FB].virt_addr;
	volatile void __iomem *pWMIX = pdp_data->wmixer_info.virt_addr;
	#if defined(CONFIG_VIOC_AFBCDEC)
	volatile void __iomem *pWDMA = pdp_data->wdma_info.virt_addr;
	#endif

	if(var->bits_per_pixel == 32) 	{
		chroma_en = UI_CHROMA_EN;
		alpha_type = 1;
		alpha_blending_en = 1;
		fmt = TCC_LCDC_IMG_FMT_RGB888;
	}
	else if(var->bits_per_pixel == 16)	{
		chroma_en = 1;
		alpha_type = 0;
		alpha_blending_en = 0;
		fmt = TCC_LCDC_IMG_FMT_RGB565;
	}
	else	{
		pr_err("%s: bits_per_pixel : %d Not Supported BPP!\n", __FUNCTION__, var->bits_per_pixel);
	}
	chromaR = chromaG = chromaB = 0;

	img_width = var->xres;
	img_height = var->yres;

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
	if((pdp_data->DispOrder == DD_SUB) && ((pdp_data->DispDeviceType== TCC_OUTPUT_COMPONENT) || (pdp_data->DispDeviceType== TCC_OUTPUT_COMPOSITE)))
	  	pResize = &secondary_display_resize_data;
	else
		pResize = &resize_data;
	#endif//CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB

	//pr_info("%s: display order:%d block:%d base_addr:0x%x w:%d h:%d offset:%d lcd:%d - %d \n", __FUNCTION__, pdp_data->DispOrder, pdp_data->ddc_info.blk_num, base_addr, var->xres, var->yres, var->yoffset, lcd_width, lcd_height);
	//pr_info("scaler num %d  l:%d r : %d u:%d d:%d \n", pdp_data->sc_num0, pResize->resize_left , pResize->resize_right, pResize->resize_up , pResize->resize_down);

	if((!lcd_height) || (!lcd_width))
		return;

	lcd_width	-= (pResize->resize_left + pResize->resize_right) * 8;
	lcd_height	-= (pResize->resize_up + pResize->resize_down) * 4;
	lcd_pos_x	= pResize->resize_left * 8;
	lcd_pos_y	= pResize->resize_up * 4;

#if 0//defined(CONFIG_VIOC_AFBCDEC) //test
	if(var->reserved[3] != 0 ) {
		unsigned int w, h;
		w = 256;
		h = 256;
		base_addr 	= 0x7d000000;
		img_width 	= w;
		img_height 	= h;
		lcd_width 	= w;
		lcd_height 	= h;
		lcd_pos_x 	= 0;
		lcd_pos_y 	= 0;
		//fmt = TCC_LCDC_IMG_FMT_RGB888;
	}
#endif

#if defined(CONFIG_VIOC_AFBCDEC) // to disable
	if( var->reserved[3] == 0)
		tca_vioc_configure_AFBCDEC(afbc_dec_vioc_id, base_addr, pRDMA, pWDMA, fmt, blk_num, var->reserved[3], 1, 0, img_width, img_height);
#endif

	/* display 3d ui : side-by-side, top-and-bottom */
	if(divide_data.enable)
	{
		if(divide_data.count == 0)
		{
			pSC			= VIOC_SC_GetAddress(pdp_data->sc_num0);
			pRDMA		= pdp_data->rdma_info[RDMA_FB].virt_addr;
			blk_num		=pdp_data->rdma_info[RDMA_FB].blk_num;
			lcd_layer	= RDMA_FB;
		}
		else
		{
			pSC			= VIOC_SC_GetAddress(pdp_data->sc_num1);
			pRDMA		= pdp_data->rdma_info[RDMA_3D].virt_addr;
			blk_num		=pdp_data->rdma_info[RDMA_3D].blk_num;
			lcd_layer	= RDMA_3D;
		}

		/* side-by-side : left/right */
		if(divide_data.mode == 0)
		{
			lcd_width	= lcd_width/2;

			if(divide_data.count == 0)
				lcd_pos_x	= lcd_pos_x/2;
			else
				lcd_pos_x	= lcd_pos_x/2 + lcd_width;
		}
		/* top-and-bottom : top/bottom */
		else
		{
			lcd_height	= lcd_height/2;

			if(divide_data.count == 0)
				lcd_pos_y	= lcd_pos_y/2;
			else
				lcd_pos_y	= lcd_pos_y/2 + lcd_height;
		}

		VIOC_RDMA_SetImageY2RMode(pRDMA, 0x2); /* Y2RMode Default 0 (Studio Color) */
	}
	else
	{
		pSC = VIOC_SC_GetAddress(pdp_data->sc_num0);
	}

	if((lcd_height == img_height) && (lcd_width == img_width))
		VIOC_SC_SetBypass (pSC, ON);
	else
		VIOC_SC_SetBypass (pSC, OFF);

	VIOC_SC_SetSrcSize(pSC, img_width, img_height);
	VIOC_SC_SetDstSize (pSC, lcd_width, lcd_height);			// set destination size in scaler
	VIOC_SC_SetOutSize (pSC, lcd_width, lcd_height);			// set output size in scaler

	// default framebuffer
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, lcd_layer, lcd_pos_x, lcd_pos_y);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if (( VIOC_CONFIG_DV_GET_EDR_PATH() || vioc_v_dv_get_stage() != DV_OFF )
	    && ( pdp_data->ddc_info.virt_addr == VIOC_DISP_GetAddress(0) )
	    //&& ( DV_PATH_VIN_DISP & vioc_get_path_type() )
    )
	{
		VIOC_V_DV_SetPosition(NULL, pRDMA, lcd_pos_x, lcd_pos_y);
	}
#endif

	VIOC_RDMA_SetImageIntl(pRDMA, interlace_output);

	VIOC_RDMA_SetImageFormat(pRDMA, fmt );				//fmt

	VIOC_RDMA_SetImageSize(pRDMA, img_width, img_height  );	//size
	VIOC_RDMA_SetImageOffset(pRDMA, fmt, var->xres_virtual);		//offset

#if defined(CONFIG_VIOC_AFBCDEC)
	if(!afbc_dec_1st_cfg)
		VIOC_RDMA_SetImageBase(pRDMA, base_addr, 0, 0);
#if !defined(CONFIG_ANDROID) && defined(CONFIG_PLATFORM_STB)
        if(var->reserved[3] != 0 )
		VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0x5); //BGR
	else
		VIOC_RDMA_SetImageRGBSwapMode(pRDMA, 0x0); //RGB
#endif
#else
	VIOC_RDMA_SetImageBase(pRDMA, base_addr, 0, 0);
#endif


#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetChromaKey(pWMIX, lcd_layer, chroma_en, chromaR, chromaG, chromaB, 0x3FF, 0x3FF, 0x3FF);
#else
	VIOC_WMIX_SetChromaKey(pWMIX, lcd_layer, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);
	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	VIOC_WMIX_SetChromaKey(pWMIX, lcd_layer, chroma_en, chromaY, chromaU, chromaV, 0xF8, 0xFC, 0xF8);
	#endif
#endif//

	VIOC_RDMA_SetImageAlphaSelect(pRDMA, alpha_type);
	VIOC_RDMA_SetImageAlphaEnable(pRDMA, alpha_blending_en);

#if defined(CONFIG_SUPPORT_2D_COMPRESSION)
	tca_vioc_configure_DEC100(base_addr, var, pRDMA);
#endif


	VIOC_WMIX_SetUpdate(pWMIX);
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	VIOC_RDMA_SetImageY2REnable(pRDMA, 0);
	VIOC_RDMA_SetImageR2YEnable(pRDMA, 1);

	//	VIOC_RDMA_SetImageAlpha(pRDMA, 0x80, 0x80);
	//	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 0);
	//	VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
#endif

#if defined(CONFIG_VIOC_AFBCDEC)
	tca_vioc_configure_AFBCDEC(afbc_dec_vioc_id, base_addr, pRDMA, pWDMA, fmt, blk_num, var->reserved[3], 1, 0, img_width, img_height);

	if(var->reserved[3] != 0 )
		VIOC_RDMA_SetIssue(pRDMA, 7, 16);
	else
#endif
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
	{
		VIOC_RDMA_SetIssue(pRDMA, 15, 16);
	}
#else
	{
		//TODO: other SoC code?
	}
#endif

	if(VIOC_DISP_Get_TurnOnOff(pdp_data->ddc_info.virt_addr) ) {
		VIOC_RDMA_SetImageUpdate(pRDMA);
	}
	VIOC_SC_SetUpdate (pSC);
	VIOC_RDMA_SetImageEnable(pRDMA);

	#if defined(CONFIG_VIOC_AFBCDEC)
	if (afbc_wdma_need_on) {
		VIOC_WDMA_SetImageEnable(pWDMA, 1);
		attach_data.flag = 1;
		afbc_wdma_need_on = 0;
	}
	#endif
}

unsigned int tca_fb_sc_m2m(unsigned int base_addr, struct fb_var_screeninfo *var, unsigned int dest_x, unsigned int dest_y)
{
	SCALER_TYPE fbscaler;
	static char sc_buf_index = 0;

//	pr_info("%s: base_addr:0x%x w:%d h:%d offset:%d lcd:%d - %d \n", __FUNCTION__, base_addr, var->xres, var->yres, var->yoffset, dest_x, dest_y);

	memset(&fbscaler, 0x00, sizeof(SCALER_TYPE));
	fbscaler.responsetype = SCALER_POLLING;
	fbscaler.viqe_onthefly = 0;

	fbscaler.src_Yaddr = base_addr;

	if(var->bits_per_pixel == 32)
		fbscaler.src_fmt = TCC_LCDC_IMG_FMT_RGB888;
	else
		fbscaler.src_fmt = TCC_LCDC_IMG_FMT_RGB565;

	fbscaler.src_ImgWidth = var->xres;
	fbscaler.src_ImgHeight = var->yres;

	fbscaler.src_winLeft = 0;
	fbscaler.src_winTop = 0;
	fbscaler.src_winRight = var->xres;
	fbscaler.src_winBottom = var->yres;

	if(sc_buf_index)
		fbscaler.dest_Yaddr = fb_scaler_pbuf1;	// destination image address
	else
		fbscaler.dest_Yaddr = fb_scaler_pbuf0;	// destination image address

	sc_buf_index = !sc_buf_index;

	fbscaler.dest_fmt = fbscaler.src_fmt;
	fbscaler.dest_ImgWidth = dest_x;		// destination image width
	fbscaler.dest_ImgHeight = dest_y; 		// destination image height

	fbscaler.dest_winLeft = 0;
	fbscaler.dest_winTop = 0;
	fbscaler.dest_winRight = dest_x;
	fbscaler.dest_winBottom = dest_y;

	if (scaler_filp)
		scaler_filp->f_op->unlocked_ioctl(scaler_filp, TCC_SCALER_IOCTRL_KERENL, (unsigned long)&fbscaler);

	return (unsigned int)fbscaler.dest_Yaddr;
}

void tca_fb_sc_m2m_rdma_active_var(unsigned int base_addr, struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
	struct fb_var_screeninfo sc_var;
	unsigned int lcd_width, lcd_height;
	unsigned int targetAddr;
  	tcc_display_resize *pResize = &resize_data;

	memcpy(&sc_var, var, sizeof(struct fb_var_screeninfo));

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);
//	pr_info("%s: %d base_addr:0x%x w:%d h:%d offset:%d lcd:%d - %d \n", __FUNCTION__, pdp_data->ddc_info.blk_num, base_addr, var->xres, var->yres, var->yoffset, lcd_width, lcd_height);

	#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
	if((pdp_data->DispOrder == DD_SUB) && ((pdp_data->DispDeviceType== TCC_OUTPUT_COMPONENT) || (pdp_data->DispDeviceType== TCC_OUTPUT_COMPOSITE)))
	  	pResize = &secondary_display_resize_data;
	else
		pResize = &resize_data;
	#endif//CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
	lcd_width	-= (pResize->resize_left + pResize->resize_right) * 8;
	lcd_height	-= (pResize->resize_up + pResize->resize_down) * 4;

	/* display 3d ui : side-by-side, top-and-bottom */
	if(divide_data.enable)
	{
		/* side-by-side : left/right */
		if(divide_data.mode == 0)
			lcd_width	= lcd_width/2;
		/* top-and-bottom : top/bottom */
		else
			lcd_height	= lcd_height/2;
	}

	if(divide_data.enable && divide_data.count == 1) {
		targetAddr = base_addr;
	}
	else {
		#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) || defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
		targetAddr = tca_fb_sc_m2m(base_addr, var, lcd_width, lcd_height);
		#else
		if(lcd_width != var->xres || lcd_height != var->yres)
			targetAddr = tca_fb_sc_m2m(base_addr, var, lcd_width, lcd_height);
		else
			targetAddr = base_addr;
		#endif

		sc_var.xres = lcd_width;
		sc_var.yres = lcd_height;
		sc_var.bits_per_pixel = var->bits_per_pixel;
	}
	tca_fb_rdma_active_var((unsigned int)targetAddr, &sc_var, pdp_data);
}

unsigned int tca_fb_g2d_m2m(unsigned int base_addr, struct fb_var_screeninfo * var, unsigned int g2d_rotate)
{
	static unsigned int g2d_buf_index;
	G2D_BITBLIT_TYPE g2d_p;

	memset(&g2d_p, 0x00, sizeof(G2D_BITBLIT_TYPE));

//	pr_info("%s: %d \n", __func__, g2d_rotate);

	g2d_p.responsetype = G2D_POLLING;
	g2d_p.src0 = (unsigned int)base_addr;

	if(var->bits_per_pixel == 32)
		g2d_p.srcfm.format = GE_RGB888;
	else
		g2d_p.srcfm.format = GE_RGB565;

	g2d_p.srcfm.data_swap = 0;
	g2d_p.src_imgx = var->xres;
	g2d_p.src_imgy = var->yres;

	g2d_p.ch_mode = g2d_rotate;

	g2d_p.crop_offx = 0;
	g2d_p.crop_offy = 0;
	g2d_p.crop_imgx = var->xres;
	g2d_p.crop_imgy = var->yres;

	if(g2d_buf_index)
		g2d_p.tgt0 = fb_g2d_pbuf1;	// destination image address
	else
		g2d_p.tgt0 = fb_g2d_pbuf0;	// destination image address

	g2d_buf_index = !g2d_buf_index;
	g2d_p.tgtfm.format = g2d_p.srcfm.format;

	// destinaion f
	g2d_p.tgtfm.data_swap = 0;

	if((g2d_rotate == ROTATE_270) || (g2d_rotate == ROTATE_90)) 	{
		g2d_p.dst_imgx = g2d_p.src_imgy;
		g2d_p.dst_imgy = g2d_p.src_imgx;
	}
	else		{
		g2d_p.dst_imgx = g2d_p.src_imgx;
		g2d_p.dst_imgy = g2d_p.src_imgy;
	}

	g2d_p.dst_off_x = 0;
	g2d_p.dst_off_y = 0;
	g2d_p.alpha_value = 255;

	g2d_p.alpha_type = G2d_ALPHA_NONE;

	g2d_ioctl((struct file *)&g2d_filp, TCC_GRP_ROTATE_IOCTRL_KERNEL, (unsigned long)&g2d_p);

	return g2d_p.tgt0;
}

void tca_fb_sc_g2d_rdma_active_var(unsigned int base_addr, struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
	struct fb_var_screeninfo tar_var;
	unsigned int targetAddr, rotate, lcd_width, lcd_height;
  	tcc_display_resize *pResize = &resize_data;

	#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
	if((pdp_data->DispOrder == DD_SUB) && ((pdp_data->DispDeviceType== TCC_OUTPUT_COMPONENT) || (pdp_data->DispDeviceType== TCC_OUTPUT_COMPOSITE)))
	  	pResize = &secondary_display_resize_data;
	else
		pResize = &resize_data;
	#endif//CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);
//	pr_info("%s: %d base_addr:0x%x w:%d h:%d offset:%d lcd:%d - %d \n", __FUNCTION__, pdp_data->ddc_info.blk_num, base_addr, var->xres, var->yres, var->yoffset, lcd_width, lcd_height);

	lcd_width	-= (pResize->resize_left + pResize->resize_right) * 8;
	lcd_height	-= (pResize->resize_up + pResize->resize_down) * 4;

	#if	defined(CONFIG_HDMI_FB_ROTATE_180)
		rotate = ROTATE_180;
	#elif defined(CONFIG_HDMI_FB_ROTATE_270)
		rotate = ROTATE_270;
	#elif defined(CONFIG_HDMI_FB_ROTATE_90)
		rotate = ROTATE_90;
	#else
		rotate = 0;
	#endif//

	tar_var = *var;

	if((lcd_width * lcd_height) > (var->xres * var->yres))
	{
		targetAddr = tca_fb_g2d_m2m(base_addr, var, rotate);
		if((rotate == ROTATE_90) || (rotate == ROTATE_270)) {
			tar_var.xres = var->yres;
			tar_var.yres = var->xres;
		}
		targetAddr = tca_fb_sc_m2m(targetAddr, &tar_var, lcd_width, lcd_height);
	}
	else
	{
		targetAddr = tca_fb_sc_m2m(base_addr, var, lcd_width, lcd_height);
		tar_var.xres = lcd_width;
		tar_var.yres = lcd_height;

		targetAddr = tca_fb_g2d_m2m(targetAddr, &tar_var, rotate);
		if((rotate == ROTATE_90) || (rotate == ROTATE_270)) {
			tar_var.xres = lcd_height;
			tar_var.yres = lcd_width;
		}
	}

	tca_fb_rdma_active_var(targetAddr, &tar_var, pdp_data);
}

void tca_fb_mvc_active_var(unsigned int base_addr, struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
	unsigned int interlace_output = 0;
	unsigned int lcd_width = 0, lcd_height = 0;
	unsigned int img_width = 0, img_height = 0, ifmt = 0;
	unsigned int chromaR = 0, chromaG = 0, chromaB = 0;
	unsigned int chroma_en = 0, alpha_blending_en = 0, alpha_type = 0;
	unsigned int iVBlank = 0;

	SCALER_TYPE fbscaler;
	volatile void __iomem *pSC0 = VIOC_SC_GetAddress(AnD_FB_SC);
	volatile void __iomem *pSC1 = VIOC_SC_GetAddress(DIV_FB_SC);

	memset(&fbscaler, 0x00, sizeof(SCALER_TYPE));

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	if((!lcd_width) || (!lcd_height))
	{
		dprintk(" %s - Err: width=%d, height=%d, bpp=%d, LCD W:%d H:%d\n", __func__, var->xres, var->yres, var->bits_per_pixel, lcd_width, lcd_height);
		return;
	}

	dprintk(" %s width=%d, height=%d, bpp=%d\n", __func__, var->xres, var->yres, var->bits_per_pixel);

	if(var->bits_per_pixel == 32)
	{
		chroma_en = UI_CHROMA_EN;
		alpha_type = 1;
		alpha_blending_en = 1;
		ifmt = TCC_LCDC_IMG_FMT_RGB888;
	}
	else
	{
		chroma_en = 1;
		alpha_type = 0;
		alpha_blending_en = 0;
		ifmt = TCC_LCDC_IMG_FMT_RGB565;
	}

	img_width = var->xres;
	img_height = var->yres;

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
	iVBlank = hdmi_get_VBlank();
        #endif
	lcd_height = (lcd_height - iVBlank)/2;

	if( lcd_width == img_width && lcd_height == img_height) {
		VIOC_SC_SetBypass (pSC0, ON);
		VIOC_SC_SetBypass (pSC1, ON);
	} else {
		VIOC_SC_SetBypass (pSC0, OFF);
		VIOC_SC_SetBypass (pSC1, OFF);
	}

	VIOC_SC_SetSrcSize(pSC0, img_width, img_height);
	VIOC_SC_SetDstSize (pSC0, lcd_width, lcd_height);	// set destination size in scaler
	VIOC_SC_SetOutSize (pSC0, lcd_width, lcd_height);	// set output size in scaler
	VIOC_SC_SetUpdate (pSC0);							// scaler update

	VIOC_SC_SetSrcSize(pSC1, img_width, img_height);
	VIOC_SC_SetDstSize (pSC1, lcd_width, lcd_height);	// set destination size in scaler
	VIOC_SC_SetOutSize (pSC1, lcd_width, lcd_height);	// set output size in scaler
	VIOC_SC_SetUpdate (pSC1);							// scaler update

	//--------------------------------------
	// set RDMA for 1st UI
	//--------------------------------------
	VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[RDMA_FB].virt_addr, true);

	// size
	VIOC_RDMA_SetImageSize(pdp_data->rdma_info[RDMA_FB].virt_addr, img_width, img_height);

	// format
	VIOC_RDMA_SetImageFormat(pdp_data->rdma_info[RDMA_FB].virt_addr, ifmt);
	if ( ifmt >= TCC_LCDC_IMG_FMT_444SEP && ifmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)	{
		VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[RDMA_FB].virt_addr, true);
		VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[RDMA_FB].virt_addr, 0x2); /* Y2RMode Default 0 (Studio Color) */
	} else {
		VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[RDMA_FB].virt_addr, 0);
	}

	VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[RDMA_FB].virt_addr, interlace_output);

	//offset
	VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[RDMA_FB].virt_addr, ifmt, img_width);

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pdp_data->rdma_info[RDMA_FB].virt_addr, true);
	VIOC_RDMA_SetImageAlphaEnable(pdp_data->rdma_info[RDMA_FB].virt_addr, true);

	// Update base addr
	VIOC_RDMA_SetImageBase(pdp_data->rdma_info[RDMA_FB].virt_addr, base_addr, 0, 0);

	//--------------------------------------
	// set RDMA for 2nd UI
	//--------------------------------------
	VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[RDMA_3D].virt_addr, true);

	// size
	VIOC_RDMA_SetImageSize(pdp_data->rdma_info[RDMA_3D].virt_addr, img_width, img_height);

	// format
	VIOC_RDMA_SetImageFormat(pdp_data->rdma_info[RDMA_3D].virt_addr, ifmt);
	if ( ifmt >= TCC_LCDC_IMG_FMT_444SEP && ifmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)	{
		VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[RDMA_3D].virt_addr, true);
		VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[RDMA_3D].virt_addr, 0x2); /* Y2RMode Default 0 (Studio Color) */
	} else {
		VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[RDMA_3D].virt_addr, 0);
	}

	VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[RDMA_3D].virt_addr, interlace_output);

	//offset
	VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[RDMA_3D].virt_addr, ifmt, img_width);

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pdp_data->rdma_info[RDMA_3D].virt_addr, true);
	VIOC_RDMA_SetImageAlphaEnable(pdp_data->rdma_info[RDMA_3D].virt_addr, true);

	// Update base addr
	VIOC_RDMA_SetImageBase(pdp_data->rdma_info[RDMA_3D].virt_addr, base_addr, 0, 0);

	//--------------------------------------
	// Set WMIX
	//--------------------------------------
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, 0, 0, 0);
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, 1, 0, lcd_height+iVBlank);

	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetBGColor(pdp_data->wmixer_info.virt_addr, 0x3ff, 0x3ff, 0x3ff, 0x3ff);
	#else
	VIOC_WMIX_SetBGColor(pdp_data->wmixer_info.virt_addr, 0xff, 0xff, 0xff, 0xff);
	#endif

	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetChromaKey(pdp_data->wmixer_info.virt_addr, 0, chroma_en, chromaR, chromaG, chromaB, 0x3FF, 0x3FF, 0x3FF);
	VIOC_WMIX_SetChromaKey(pdp_data->wmixer_info.virt_addr, 1, chroma_en, chromaR, chromaG, chromaB, 0x3FF, 0x3FF, 0x3FF);
	#else
	VIOC_WMIX_SetChromaKey(pdp_data->wmixer_info.virt_addr, 0, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);
	VIOC_WMIX_SetChromaKey(pdp_data->wmixer_info.virt_addr, 1, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);
	#endif//

		VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);

	//--------------------------------------
	// enable RDMA for 1st and 2nd UI
	//--------------------------------------
	VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[RDMA_FB].virt_addr);
	VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[RDMA_3D].virt_addr);

}

void tca_fb_attach_update(struct tcc_dp_device *pMdp_data, struct tcc_dp_device *pSdp_data)
{
	unsigned int img_width, img_height, base_addr,attach_posx,attach_posy;
	unsigned int nRDMA_Attached = RDMA_FB;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
//	nRDMA_Attached = RDMA_FB1;
#endif

	if(attach_data.flag == 0)
		return;

	base_addr = attach_data.pbuf[attach_data.index];
	attach_data.index = (attach_data.index+1)%MAX_ATTACH_BUFF_CNT;

	/* get the size of the attached output */
	VIOC_DISP_GetSize(pSdp_data->ddc_info.virt_addr, &img_width, &img_height);

	/* default framebuffer */
	VIOC_WMIX_SetPosition(pSdp_data->wmixer_info.virt_addr, nRDMA_Attached, 0, 0);
	/* overlay setting */
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetChromaKey(pSdp_data->wmixer_info.virt_addr, nRDMA_Attached, 0, 0, 0, 0, 0x3FF, 0x3FF, 0x3FF);
	#else
	VIOC_WMIX_SetChromaKey(pSdp_data->wmixer_info.virt_addr, nRDMA_Attached, 0, 0, 0, 0, 0xF8, 0xFC, 0xF8);
	#endif//
	VIOC_RDMA_SetImageFormat(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, TCC_LCDC_IMG_FMT_RGB888);				//fmt
	VIOC_RDMA_SetImageOffset(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, TCC_LCDC_IMG_FMT_RGB888, img_width);	//offset
	VIOC_RDMA_SetImageSize(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, img_width, img_height);					//size
	VIOC_RDMA_SetImageBase(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, base_addr, 0, 0);

	VIOC_RDMA_SetImageAlphaSelect(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, 0);
	VIOC_RDMA_SetImageAlphaEnable(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, 0);

	attach_posx = output_attach_resize_data.resize_left * 8;
	attach_posy = output_attach_resize_data.resize_up * 4;

	base_addr = attach_data.pbuf[attach_data.index];
	VIOC_WDMA_SetImageBase(pMdp_data->wdma_info.virt_addr, base_addr+attach_posx*4+(img_width)*attach_posy*4, 0, 0);
	VIOC_WDMA_SetImageUpdate(pMdp_data->wdma_info.virt_addr);
	VIOC_WMIX_SetUpdate(pSdp_data->wmixer_info.virt_addr);

	if(attach_data.update_started == 0)
	{
#if defined(CONFIG_TCC_DV_IN) && defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if(0)//VIOC_CONFIG_DV_GET_EDR_PATH())
		{
			unsigned int nLut_component = VIOC_LUT_COMP1;

			tcc_set_lut_csc_coeff(0x0DA706A4, 0x0F810FB4, 0x0FF60488, 0x0F980FEC, 0x479); // 2020 to 709
			tcc_set_lut_plugin(nLut_component, pSdp_data->rdma_info[nRDMA_Attached].blk_num);
			tcc_set_lut_enable(nLut_component, true);
			printk("### DV-PLUG-LUT for attach : 0x%x \n", pSdp_data->rdma_info[nRDMA_Attached].blk_num);

			VIOC_RDMA_SetImageRGBSwapMode(pSdp_data->rdma_info[nRDMA_Attached].virt_addr, 4);
		}
#endif
		attach_data.update_started = 1;
	}

	VIOC_RDMA_SetImageEnable(pSdp_data->rdma_info[nRDMA_Attached].virt_addr);

	if(pSdp_data->FbPowerState)
		VIOC_DISP_TurnOn(pSdp_data->ddc_info.virt_addr);

}

void tca_fb_attach_update_flag(void)
{
	if(attach_data.flag)
	{
		if(attach_data.update_flag == 0)
			attach_data.update_flag = 1;
	}
}

static irqreturn_t tca_fb_attach_handler(int irq, void *dev_id)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = info->par;
	struct tcc_dp_device *pMdp_data = &tccfb_info->pdata.Mdp_data;
	struct tcc_dp_device *pSdp_data = &tccfb_info->pdata.Sdp_data;


	if (!is_vioc_intr_activatied(attach_intr.vioc_intr->id, (1<<attach_intr.vioc_intr->bits))) {
		return IRQ_NONE;
	}


	tca_fb_attach_update_flag();



	if(attach_data.flag)
	{
		if(attach_data.update_start)
		{
			tca_fb_attach_update(pMdp_data, pSdp_data);
			attach_data.update_start = 0;
		}

		if(attach_data.update_flag)
		{
			attach_data.update_flag = 0;
			attach_data.update_start = 1;
		}
	}

	vioc_intr_clear(attach_intr.vioc_intr->id, (1<<attach_intr.vioc_intr->bits));

	return IRQ_HANDLED;
}

void tca_fb_attach_start(struct tccfb_info *info)
{
	volatile void __iomem *pWDMA;
	volatile void __iomem *pSC;
	unsigned int main_wd, main_ht, attach_wd, attach_ht, scaler_num, ret;
	unsigned int attach_posx = 0, attach_posy = 0, attach_buffer_len;
	pmap_t pmap;
	unsigned int *remap_addr;
	int i = 0;

	struct tcc_dp_device *pMdp_data = &info->pdata.Mdp_data;
	struct tcc_dp_device *pSdp_data = &info->pdata.Sdp_data;

	printk("%s\n", __func__);

	/* set the buffer for attached output */
	if(0 > pmap_get_info("output_attach", &pmap)){
		printk("%s-%d : output_attach allocation is failed.\n", __func__, __LINE__);
		return;
	}

	if(pmap.size == 0)      {
		printk("attach buffer cann't alloc from pmap : \n");
		return;
	}

	remap_addr = (unsigned int *)ioremap_nocache(pmap.base, pmap.size);
	memset_io((void *)remap_addr, 0x00, pmap.size);
	if(remap_addr != NULL)
		iounmap((void*)remap_addr);

	/* set WDMA register */
	pWDMA = (volatile void __iomem *)pMdp_data->wdma_info.virt_addr;

	/* enable WDMA interrupt handler */
	if(pMdp_data->DispNum)
		attach_intr.vioc_intr->id = VIOC_INTR_WD1;
	else
		attach_intr.vioc_intr->id = VIOC_INTR_WD0;

	attach_intr.vioc_intr->bits = VIOC_WDMA_INTR_EOFF;

	vioc_intr_disable(pMdp_data->wdma_info.irq_num, attach_intr.vioc_intr->id, (1<<attach_intr.vioc_intr->bits));
	ret = request_irq(pMdp_data->wdma_info.irq_num, tca_fb_attach_handler, IRQF_SHARED, "TCC_WDMA", &attach_intr);
	vioc_intr_enable(pMdp_data->wdma_info.irq_num, attach_intr.vioc_intr->id, (1<<attach_intr.vioc_intr->bits));


	/* set the attached WMIX path to Bypass */
	if(pMdp_data->DispNum)
		VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 0);
	else
		VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 0);

	/* set the scaler information */
	scaler_num = ATTACH_SCALER;

	pSC = VIOC_SC_GetAddress(scaler_num);

	/* get the resolution of main output */
	VIOC_DISP_GetSize(pMdp_data->ddc_info.virt_addr, &main_wd, &main_ht);

	/* get the resolution of attached output */
	VIOC_DISP_GetSize(pSdp_data->ddc_info.virt_addr, &attach_wd, &attach_ht);

	attach_buffer_len = attach_wd * attach_ht * 4;
	if( (attach_buffer_len*MAX_ATTACH_BUFF_CNT) > pmap.size )
		printk("%s-%d not enough pmap size (0x%x) for attach \n", __func__, __LINE__, pmap.size);

	for(i=0; i<MAX_ATTACH_BUFF_CNT; i++)
	{
		attach_data.pbuf[i] = pmap.base + (attach_buffer_len * i);
		//printk("%s-%d attach buffer[%d] = 0x%x \n", __func__, __LINE__, i, attach_data.pbuf[i]);
	}

	attach_wd -= (output_attach_resize_data.resize_left + output_attach_resize_data.resize_right) * 8;
	attach_ht -= (output_attach_resize_data.resize_up + output_attach_resize_data.resize_down) * 4;
	attach_posx = output_attach_resize_data.resize_left * 8;
	attach_posy = output_attach_resize_data.resize_up * 4;

	/* set SCALER before plugging-in */
	VIOC_SC_SetBypass(pSC, OFF);
	VIOC_SC_SetSrcSize(pSC, main_wd, main_ht);
	VIOC_SC_SetDstSize(pSC, attach_wd, attach_ht);
	VIOC_SC_SetOutSize(pSC, attach_wd, attach_ht);
	VIOC_SC_SetUpdate(pSC);

	/* plug the scaler in WDMA */
	if(attach_data.plugin == 0)
	{
		VIOC_CONFIG_PlugIn(scaler_num, pMdp_data->wdma_info.blk_num);
		attach_data.plugin = 1;
	}

	/* set the control register of WDMA */
	VIOC_WDMA_SetImageR2YEnable(pWDMA, 0);

#if defined(CONFIG_TCC_DV_IN) && defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH())
	{
		VIOC_WDMA_SetImageY2RMode(pWDMA, 2);
		VIOC_WDMA_SetImageY2REnable(pWDMA, 1);
	}
#endif

	VIOC_WDMA_SetImageSize(pWDMA, attach_wd, attach_ht);
	attach_wd += (output_attach_resize_data.resize_left + output_attach_resize_data.resize_right) * 8;
	VIOC_WDMA_SetImageOffset(pWDMA, TCC_LCDC_IMG_FMT_RGB888, attach_wd);
	VIOC_WDMA_SetImageFormat(pWDMA, TCC_LCDC_IMG_FMT_RGB888);
	VIOC_WDMA_SetImageBase(pWDMA, (unsigned int)attach_data.pbuf[0]+attach_posx*4 + attach_wd*attach_posy*4, 0, 0);

	if((__raw_readl(pWDMA + WDMACTRL_OFFSET) & WDMACTRL_IEN_MASK) == 0)
		VIOC_WDMA_SetImageEnable(pWDMA, 1 /* ON */);
	else
		VIOC_WDMA_SetImageUpdate(pWDMA);

#if defined(CONFIG_TCC_DV_IN) && defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	if(VIOC_CONFIG_DV_GET_EDR_PATH() && (DV_PATH_VIN_WDMA & vioc_get_path_type()) && !(DV_PATH_VIN_DISP & vioc_get_path_type()))
	{
		VIOC_DV_IN_Configure(main_wd, main_ht, FMT_DV_IN_RGB444_24BIT,
				(/*(DV_PATH_VIN_DISP & vioc_get_path_type()) &&*/ ((DOVI == vioc_get_out_type()) || (DOVI_LL == vioc_get_out_type()))) ? DV_IN_ON : DV_IN_OFF);
		VIOC_DV_IN_SetEnable(DV_IN_ON);

		printk("### DV-ATTACH : %d => %dx%d -> %dx%d, 0x%x\n",
				attach_data.flag, main_wd, main_ht, attach_wd, attach_ht,
				attach_data.pbuf[0]+attach_posx*4 + attach_wd*attach_posy*4);

		if( DV_PATH_DIRECT_VIN_WDMA == vioc_get_path_type() )
			VIOC_DISP_TurnOnOff_With_DV(pMdp_data->ddc_info.virt_addr, 1);
	}
#endif

	attach_data.flag = 1;
	attach_data.index = 0;
	attach_data.update_flag = 0;
	attach_data.update_start = 0;
	attach_data.update_started = 0;
}

int tca_fb_attach_stop(struct tccfb_info *info)
{
	volatile void __iomem *pWDMA;
	unsigned int scaler_num;
	struct tcc_dp_device *pMdp_data = &info->pdata.Mdp_data;

	printk("%s\n", __func__);

	if(attach_data.flag == 0)
		return -1;

	/* set WDMA register */
	pWDMA = (volatile void __iomem *)pMdp_data->wdma_info.virt_addr;

	/* disable WDMA */
	VIOC_WDMA_SetImageDisable(pWDMA);

	/* disable WDMA interrupt handler */
	vioc_intr_disable(pMdp_data->wdma_info.irq_num, attach_intr.vioc_intr->id, (1<<attach_intr.vioc_intr->bits));
	free_irq(pMdp_data->wdma_info.irq_num, &attach_intr);

	/* set the scaler number */
	scaler_num = ATTACH_SCALER;

	/* plug-out scaler */
	if(attach_data.plugin)
	{
		if(VIOC_CONFIG_PlugOut(scaler_num) != VIOC_PATH_DISCONNECTED)
		{
			if(pMdp_data->DispNum) {
				VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
				VIOC_CONFIG_SWReset(VIOC_WDMA01, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(VIOC_WDMA01, VIOC_CONFIG_CLEAR);
			} else {
				VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
				VIOC_CONFIG_SWReset(VIOC_WDMA00, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(VIOC_WDMA00, VIOC_CONFIG_CLEAR);
			}
		}

		attach_data.plugin = 0;
	}
	pmap_release_info("output_attach");

	memset((void*)&attach_data, 0x00, sizeof(tcc_display_attach));
	return 0;
}

void tca_fb_attach_stop_no_intr(struct tccfb_info *info)
{
        volatile void __iomem *pWDMA;
        unsigned int scaler_num;

        struct tcc_dp_device *pMdp_data = &info->pdata.Mdp_data;

        printk("%s\n", __func__);


        /* set WDMA register */
        pWDMA = (volatile void __iomem *)pMdp_data->wdma_info.virt_addr;

        /* disable WDMA */
        VIOC_WDMA_SetImageDisable(pWDMA);

        /* set the scaler number */
        scaler_num = ATTACH_SCALER;

        /* plug-out scaler */
        if(attach_data.plugin)
		{
			if(VIOC_CONFIG_PlugOut(scaler_num) != VIOC_PATH_DISCONNECTED)
			{
				if(pMdp_data->DispNum) {
					VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
					VIOC_CONFIG_SWReset(VIOC_WDMA01, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(VIOC_WDMA01, VIOC_CONFIG_CLEAR);
				} else {
					VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(scaler_num, VIOC_CONFIG_CLEAR);
					VIOC_CONFIG_SWReset(VIOC_WDMA00, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(VIOC_WDMA00, VIOC_CONFIG_CLEAR);
				}
			}

			attach_data.plugin = 0;
		}

        memset((void*)&attach_data, 0x00, sizeof(tcc_display_attach));
}

#ifdef CONFIG_DIRECT_MOUSE_CTRL
unsigned char tca_fb_mouse_data_out(unsigned int data_in, unsigned char yuv, unsigned char threshold_use)
{
	unsigned char data_out;
	unsigned char threshold_value = 128;

	//[19] = underflow, [18:17] = overflow
	if(data_in & (0x01 << 19))
		data_out = 0;
	else if ((data_in & (0x01 << 18)) || (data_in & (0x01 << 17)))
		data_out = 0xFF;
	else
		data_out = ((data_in >> 9) & 0x00FF);

	if((yuv == 0) && (threshold_use == 1))
	{
		if(data_out > threshold_value)
			data_out = threshold_value;
	}

	return data_out;
}

void tca_fb_mouse_set_icon(tcc_mouse_icon *mouse_icon)
{
    int i, j, k;
    char *dst, *src = mouse_icon->buf;

    if(!mouse_data.index)
        dst = (char *)mouse_data.vbuf1;
    else
        dst = (char *)mouse_data.vbuf2;

    mouse_data.width = ((mouse_icon->width + 3) >> 2) << 2;
    mouse_data.height = ((mouse_icon->height + 1)>> 1) << 1;

    for(i=0; i<mouse_icon->height; i++)
    {
        for(j=0; j<mouse_icon->width; j++)
        {
            *(dst+3) = *(src+3);
            *(dst+2) = *(src+0);
            *(dst+1) = *(src+1);
            *(dst+0) = *(src+2);
            dst+=4;
            src+=4;
        }

        for(k=0; k<(mouse_data.width - mouse_icon->width); k++)
        {
            *(dst+3) = 0;
            *(dst+2) = 0;
            *(dst+1) = 0;
            *(dst+0) = 0;
            dst+=4;
        }
    }


	#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV) || defined(CONFIG_TCC_COMPOSITE_COLOR_SPACE_YUV)
	{
		int output_mode, hdmi_mode;
		hdmi_mode	= (mouse_icon->option & 0x00ff) >> 0; /* 0: HDMI, 1: DVI */
		output_mode	= (mouse_icon->option & 0xff00) >> 8; /* 1: HDMI, 2: CVBS, 3:Component */

		#if defined(CONFIG_TCC_OUTPUT_COLOR_SPACE_YUV)
			if(output_mode != OUTPUT_HDMI || hdmi_mode != 1)
		#else
			if(output_mode == OUTPUT_COMPOSITE)
		#endif
			{
				unsigned int wYMULR, wYMULG, wYMULB;
				unsigned int wUMULR, wUMULG, wUMULB;
				unsigned int wVMULR, wVMULG, wVMULB;
				unsigned char wYADDR, wUADDR, wVADDR;
				unsigned int wY_tmp, wU_tmp, wV_tmp;
				unsigned char wA, wR, wG, wB;
				unsigned char wY, wU, wV;

			    if(!mouse_data.index)
			        dst = (char *)mouse_data.vbuf1;
			    else
			        dst = (char *)mouse_data.vbuf2;

				wYMULR = 0x06d; //	0.213
				wYMULG = 0x16e; //	0.715
				wYMULB = 0x025; //	0.072

				wUMULR = 0x03c; //	0.117
				wUMULG = 0x0ca; //	0.394
				wUMULB = 0x106; //	0.511

				wVMULR = 0x106; //	0.511
				wVMULG = 0x0ee; //	0.464
				wVMULB = 0x018; //	0.047

				wYADDR = 0x0;	//	0
				wUADDR = 0x80;	//	128
				wVADDR = 0x80;	//	128

			    for(i=0; i<mouse_icon->height; i++)
			    {
			        for(j=0; j<mouse_icon->width; j++)
			        {
			            wA = *(dst+3);
			            wR = *(dst+2);
			            wG = *(dst+1);
			            wB = *(dst+0);

						wY_tmp = (wYADDR << 9) + (wYMULR * wR) + (wYMULG * wG) + (wYMULB * wB);
						wU_tmp = (wUADDR << 9) - (wUMULR * wR) - (wUMULG * wG) + (wUMULB * wB);
						wV_tmp = (wVADDR << 9) + (wVMULR * wR) - (wVMULG * wG) - (wVMULB * wB);

						if(output_mode == OUTPUT_COMPOSITE)
							wY = tca_fb_mouse_data_out(wY_tmp, 0, 1);
						else
							wY = tca_fb_mouse_data_out(wY_tmp, 0, 0);

						wU = tca_fb_mouse_data_out(wU_tmp, 0, 0);
						wV = tca_fb_mouse_data_out(wV_tmp, 0, 0);

						*(dst+3) = wA;
						*(dst+2) = wY;
						*(dst+1) = wU;
						*(dst+0) = wV;

			            dst+=4;
			        }

					#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
					// build error fix.
			        for(k=0; k<(mouse_icon->width); k++)
					#else
					for(k=0; k<(mouse_cursor_width-mouse_icon->width); k++)
					#endif
			        {
						*(dst+3) = 0x00;	//Alpha
						*(dst+2) = 0x00;	//Y
						*(dst+1) = 0x80;	//Cb
						*(dst+0) = 0x80;	//Cr
			            dst+=4;
					}
				}
			}
	}
	#endif

    mouse_data.index = !mouse_data.index;
}

int tca_fb_mouse_move(unsigned int width, unsigned int height, tcc_mouse *mouse, struct tcc_dp_device *pdp_data)
{
	volatile void __iomem *pDISPBase = pdp_data->ddc_info.virt_addr;
	volatile void __iomem *pWMIXBase = pdp_data->wmixer_info.virt_addr;
	volatile void __iomem *pRDMABase = pdp_data->rdma_info[RDMA_MOUSE].virt_addr;

	unsigned int lcd_width, lcd_height, lcd_w_pos,lcd_h_pos, mouse_x, mouse_y;
	unsigned int interlace_output = 0, display_width, display_height;
	unsigned int wmix_channel = 0;
  	tcc_display_resize *pResize = &resize_data;

	if((pDISPBase == NULL) || (pWMIXBase == NULL) || (pRDMABase == NULL)) {
		//pr_err("%s - VIOC is not valid, RDMA:0x%08x, WMIX:0x%08x, DISP:0x%08x\n", __func__, pDISPBase, pWMIXBase, pRDMABase);
		return 0;
	}

	wmix_channel = pdp_data->rdma_info[RDMA_MOUSE].blk_num;

	#if defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
		if( !(pDISPBase->uCTRL.nREG & HwDISP_NI )) {//interlace mode
			interlace_output = 1;
		}
	#endif

	//dprintk("%s RDMA:0x%08x, WMIX:0x%08x, DISP:0x%08x\n", __func__, pRDMABase, pWMIXBase, pDISPBase);

	#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
	if((pdp_data->DispOrder == DD_SUB) && (pdp_data->DispDeviceType== TCC_OUTPUT_COMPONENT))
	  	pResize = &secondary_display_resize_data;
	else
		pResize = &resize_data;
	#endif//CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB


	VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);

	if((!lcd_width) || (!lcd_height))
		return -1;

	lcd_width	-= (pResize->resize_left + pResize->resize_right) << 3;
	lcd_height	-= (pResize->resize_up + pResize->resize_down) << 2;

	mouse_x = (unsigned int)(lcd_width * mouse->x / width);
	mouse_y = (unsigned int)(lcd_height *mouse->y / height);

	if( mouse_x > lcd_width - mouse_data.width )
		display_width = lcd_width - mouse_x;
	else
		display_width = mouse_data.width;

	if( mouse_y > lcd_height - mouse_data.height )
		display_height = lcd_height - mouse_y;
	else
		display_height = mouse_data.height;

	VIOC_RDMA_SetImageY2REnable(pRDMABase, false);

	VIOC_RDMA_SetImageOffset(pRDMABase, TCC_LCDC_IMG_FMT_RGB888, mouse_data.width);
	VIOC_RDMA_SetImageFormat(pRDMABase, TCC_LCDC_IMG_FMT_RGB888);

	mouse_x += pResize->resize_left << 3;
	if(interlace_output)
		mouse_y += pResize->resize_up << 1;
	else
		mouse_y += pResize->resize_up << 2;

	lcd_w_pos = mouse_x;
	lcd_h_pos = mouse_y;

	dprintk("%s lcd_width:%d, lcd_height:%d, lcd_w_pos:%d, lcd_h_pos:%d\n\n", __func__, lcd_width, lcd_height, lcd_w_pos, lcd_h_pos);

	// position
	VIOC_WMIX_SetPosition(pWMIXBase, wmix_channel, lcd_w_pos, lcd_h_pos);

	VIOC_RDMA_SetImageSize(pRDMABase, display_width, display_height);
	VIOC_RDMA_SetImageIntl(pRDMABase, interlace_output);

	#if !defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
	if(interlace_output)
		VIOC_WMIX_SetPosition(pWMIXBase, wmix_channel,  lcd_w_pos, (lcd_h_pos/2) );
	else
		VIOC_WMIX_SetPosition(pWMIXBase, wmix_channel, lcd_w_pos, lcd_h_pos);
	#endif

	#if defined(CONFIG_TCC_VIOCMG)
	viocmg_set_wmix_ovp(VIOCMG_CALLERID_FB, pdp_data->wmixer_info.blk_num, 8);
	#else
	VIOC_WMIX_SetOverlayPriority(pWMIXBase, 8);
	#endif

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pRDMABase, 1);
	VIOC_RDMA_SetImageAlphaEnable(pRDMABase, 1);

	// image address
	if(mouse_data.index)
		VIOC_RDMA_SetImageBase(pRDMABase, (unsigned int)mouse_data.pbuf1, 0, 0);
	else
		VIOC_RDMA_SetImageBase(pRDMABase, (unsigned int)mouse_data.pbuf2, 0, 0);

	VIOC_WMIX_SetUpdate(pWMIXBase);
	VIOC_RDMA_SetImageEnable(pRDMABase);

	#if defined(CONFIG_TCC_DISPLAY_MODE_USE) && !defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)
		tca_fb_attach_update_flag();
	#endif

	return 1;
}
#endif

void tca_fb_resize_set_value(tcc_display_resize resize_value, TCC_OUTPUT_TYPE output)
{
	printk( "%s : resize_up = %d, resize_down = %d, resize_left = %d, resize_right = %d\n", __func__,
			resize_value.resize_up, resize_value.resize_down, resize_value.resize_left, resize_value.resize_right );
#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
	if((output == TCC_OUTPUT_COMPONENT) || (output == TCC_OUTPUT_COMPOSITE))
		memcpy((void *)&secondary_display_resize_data, (void *)&resize_value, sizeof(tcc_display_resize));
	else
#endif//CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
		memcpy((void *)&resize_data, (void *)&resize_value, sizeof(tcc_display_resize));
}

void tca_fb_output_attach_resize_set_value(tcc_display_resize resize_value)
{
	printk( "%s : output_attach resize_up = %d, resize_down = %d, resize_left = %d, resize_right = %d\n", __func__,
			resize_value.resize_up, resize_value.resize_down, resize_value.resize_left, resize_value.resize_right );

	memcpy((void *)&output_attach_resize_data, (void *)&resize_value, sizeof(tcc_display_resize));
}
int tca_fb_divide_set_mode(struct tcc_dp_device *pdp_data, char enable, char mode)
{
	// deprecated
#if !defined(CONFIG_ARCH_TCC898X)

	divide_data.enable = enable;
	divide_data.mode = mode;

        pdp_data->sc_num1 = DIV_FB_SC;

	if(enable) {
		divide_data.fbtype = pdp_data->FbUpdateType;
		pdp_data->FbUpdateType = FB_DIVIDE_UPDATE;

                VIOC_CONFIG_PlugOut(pdp_data->sc_num1);
                VIOC_CONFIG_SWReset(pdp_data->sc_num1, VIOC_CONFIG_RESET);
                VIOC_CONFIG_SWReset(pdp_data->sc_num1, VIOC_CONFIG_CLEAR);
                VIOC_CONFIG_SWReset(pdp_data->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_RESET);
                VIOC_CONFIG_SWReset(pdp_data->rdma_info[RDMA_3D].blk_num, VIOC_CONFIG_CLEAR);
                VIOC_CONFIG_PlugIn(pdp_data->sc_num1, pdp_data->rdma_info[RDMA_3D].blk_num);
	} else {
		pdp_data->FbUpdateType = divide_data.fbtype;

		VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[RDMA_3D].virt_addr);

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[RDMA_3D].blk_num) != -1) //scaler plug in status check
		{
			pr_info("Scaler-%d plug out \n", get_vioc_index(pdp_data->sc_num1));
			VIOC_CONFIG_PlugOut(pdp_data->sc_num1);
			VIOC_CONFIG_SWReset(pdp_data->sc_num1, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(pdp_data->sc_num1, VIOC_CONFIG_CLEAR);
		}
	}

        pr_info("%s, enable=%d, mode=%d, fbtype=%d\n", __func__, enable, mode, divide_data.fbtype);
#endif
	return 0;
}

int tca_fb_divide_get_status(void)
{
	return divide_data.enable;
}

void tca_fb_divide_active_var(unsigned int base_addr, struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
	//struct fb_info *info = registered_fb[0];
	//struct tccfb_info *tccfb_info = info->par;

	/* Update Left/Top screen */
	divide_data.count = 0;
	if(divide_data.fbtype == FB_SC_M2M_RDMA_UPDATE)
		tca_fb_sc_m2m_rdma_active_var(base_addr, var, pdp_data);
	else
		tca_fb_sc_rdma_active_var(base_addr, var, pdp_data);

	/* Update Right/Bottom screen */
	divide_data.count = 1;
	if(divide_data.fbtype == FB_SC_M2M_RDMA_UPDATE) {
		#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
		tca_fb_rdma_active_var_rbase(pdp_data);
		#else
		tca_fb_sc_m2m_rdma_active_var(base_addr, var, pdp_data);
		#endif
	} else {
		tca_fb_sc_rdma_active_var(base_addr, var, pdp_data);
	}
}

// pjj modify need
#define RDMA_UVI_MAX_WIDTH             3072


void tca_lcdc_set_onthefly(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo)
{
	unsigned int iSCType = 0;

	#ifdef CONFIG_FB_VIOC
	if(Output_SelectMode == TCC_OUTPUT_NONE)
		return;
	#endif

	if((pdp_data == NULL) || (ImageInfo == NULL)){
		pr_err("ERROR %s pdp_data:%p ImageInfo:%p\n", __func__, pdp_data, ImageInfo);
		return;
	}

	if(ImageInfo->MVCframeView == 1)
	{
		dprintk("%s is returned because of mvc frame view\n", __func__);
		return;
	}

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
	if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO] != 0)
		return;
#endif

	if(ImageInfo->on_the_fly)
	{
		iSCType = tca_get_scaler_num(pdp_data->DispDeviceType, ImageInfo->Lcdc_layer);

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) == -1)
		{
		#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
			if(((ImageInfo->crop_right - ImageInfo->crop_left) != ImageInfo->Image_width) || ((ImageInfo->crop_bottom - ImageInfo->crop_top) != ImageInfo->Image_height))
		#endif
			{
				pr_info(" %s Scaler-%d is plug in RDMA-%d \n",
					__func__, get_vioc_index(iSCType), get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
				VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);
				VIOC_CONFIG_PlugIn (iSCType, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			}

		#ifdef CONFIG_VIOC_MAP_DECOMP
			// HEVC MAP CONVERTER compressed data
			if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
			{
				if (VIOC_CONFIG_DMAPath_Support()) {
					int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
					if((component_num < VIOC_MC0 ) || (component_num > (VIOC_MC0 + VIOC_MC_MAX))){
						pr_info("%s Map Converter-%d \n", __func__, nDeCompressor_Main);
						VIOC_CONFIG_DMAPath_UnSet(component_num);
						tca_map_convter_swreset(VIOC_MC0 + nDeCompressor_Main);
						VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_MC0 + nDeCompressor_Main);
						//pr_info("%s Map converter  %d Plugged-In\n",__func__,pdp_data->ddc_info.blk_num);
					}
				} else {
					#ifdef CONFIG_ARCH_TCC803X
					VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, VIOC_MC0 + nDeCompressor_Main);
					#endif
				}
			}
			else
		#endif//CONFIG_VIOC_MAP_DECOMP
		#ifdef CONFIG_VIOC_DTRC_DECOMP
			// VP9 DTRC CONVERTER compressed data
			if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0)
			{
				if (VIOC_CONFIG_DMAPath_Support()) {
					int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
					if((component_num < VIOC_DTRC0 ) || (component_num > (VIOC_DTRC0 + VIOC_DTRC_MAX))){
						pr_info("%s DTRC converter-%d \n", __func__, nDeCompressor_Main);
						VIOC_CONFIG_DMAPath_UnSet(component_num);
						//tca_dtrc_convter_swreset(VIOC_DTRC0 + nDeCompressor_Main);
						VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_DTRC0 + nDeCompressor_Main);
						//pr_info("%s Map converter  %d Plugged-In\n",__func__,nDeCompressor_Main);
					}
				}
			}
			else
		#endif//CONFIG_VIOC_DTRC_DECOMP
			{
		#ifndef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
				pr_info("%s RDMA-%d \n", __func__, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
		#endif
				if (VIOC_CONFIG_DMAPath_Support()) {
					int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
					#ifdef CONFIG_VIOC_MAP_DECOMP
					if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX)))
						VIOC_CONFIG_DMAPath_UnSet(component_num);
					#endif
					#ifdef CONFIG_VIOC_DTRC_DECOMP
					if((component_num < VIOC_DTRC0) && (component_num > (VIOC_DTRC0 + VIOC_RDMA_MAX)))
						VIOC_CONFIG_DMAPath_UnSet(component_num);
					#endif
					VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
				} else {
					#ifdef CONFIG_ARCH_TCC803X
					VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
					#endif
				}
		#ifndef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
				pr_info("%s RDMA-%d Plugged-In\n", __func__, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
		#endif
			}
		}
	}
	else
	{
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1)
		{
			iSCType = (unsigned int)VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);

			if (VIOC_CONFIG_DMAPath_Support()) {
				int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
				pr_info("%s Scaler-%d is plug out component_num:%x RDMA:%d \n", __func__,
					get_vioc_index(iSCType), component_num, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
				#ifdef CONFIG_VIOC_MAP_DECOMP
				if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX)))
			 	{
					tca_map_convter_onoff(component_num, 0, 1);
					VIOC_CONFIG_DMAPath_UnSet(component_num);
					tca_map_convter_swreset(component_num); //disable it to prevent system-hang!!
			 	}
				else
				#endif//CONFIG_VIOC_MAP_DECOMP
				#ifdef CONFIG_VIOC_DTRC_DECOMP
				if((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX)))
			 	{
					tca_dtrc_convter_onoff(component_num, 0, 1);
					VIOC_CONFIG_DMAPath_UnSet(component_num);
					//tca_dtrc_convter_swreset(component_num); //disable it to prevent system-hang!!
			 	}
				else
				#endif//CONFIG_VIOC_DTRC_DECOMP
				if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
				{
					volatile void __iomem *pVRDMA = VIOC_RDMA_GetAddress(component_num);
					VIOC_RDMA_SetImageDisable(pVRDMA);
					VIOC_CONFIG_DMAPath_UnSet(component_num);
				}

				VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			} else {
				#ifdef CONFIG_ARCH_TCC803X
				VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
				#endif
			}

			VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);

//			VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_RESET);
//			VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_CLEAR);

			if(iSCType <= VIOC_SCALER3)
			{
				VIOC_CONFIG_PlugOut(iSCType);
				VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_CLEAR);
			}
		}
	}
}

void tcc_video_post_process(struct tcc_lcdc_image_update *ImageInfo)
{
	if (ImageInfo == NULL) {
		pr_err("%s: ImageInfo is null\n", __func__);
		return;
	}

#ifdef CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT
	#ifdef CONFIG_HDMI_DISPLAY_LASTFRAME
	tcc_video_clear_last_frame(ImageInfo->Lcdc_layer, false);
	#endif
	#ifdef CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME
	if(ImageInfo)
		tcc_video_frame_backup(ImageInfo);
	#endif
	#ifdef CONFIG_DISPLAY_EXT_FRAME
	#ifdef CONFIG_LINUX
	if(!restricted_ExtFrame)
	#endif
		tcc_ctrl_ext_frame(0);
	#endif
#endif
}

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
int tca_edr_path_configure(void)
{
	struct fb_info *info = registered_fb[0];
    struct tccfb_info *ptccfb_info = info->par;
    struct tcc_dp_device *pdp_data = NULL;

    pdp_data = &ptccfb_info->pdata.Mdp_data;

	if(vioc_v_dv_get_stage() != DV_OFF)
	{
		unsigned int lcd_pos_x, lcd_pos_y, lcd_width, lcd_height;
		tcc_display_resize *pResize = &resize_data;

	#ifdef CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB
		if((pdp_data->DispOrder == DD_SUB) && ((pdp_data->DispDeviceType== TCC_OUTPUT_COMPONENT) || (pdp_data->DispDeviceType== TCC_OUTPUT_COMPOSITE)))
		  	pResize = &secondary_display_resize_data;
		else
			pResize = &resize_data;
	#endif//CONFIG_PRESENTATION_SECONDAY_DISPLAY_RESIZE_STB

		VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);
		lcd_width	-= (pResize->resize_left + pResize->resize_right) * 8;
		lcd_height	-= (pResize->resize_up + pResize->resize_down) * 4;
		lcd_pos_x	= pResize->resize_left * 8;
		lcd_pos_y	= pResize->resize_up * 4;

		VIOC_CONFIG_DV_SET_EDR_PATH(1);
		VIOC_DISP_GetSize(VIOC_DISP_GetAddress(0), &lcd_width, &lcd_height);

		//msleep(100);//Temp!!
		VIOC_V_DV_Base_Configure(lcd_pos_x, lcd_pos_y, lcd_width, lcd_height);
		_tca_vioc_intr_onoff(ON, pdp_data->ddc_info.irq_num, 0);

		vioc_v_dv_prog(dv_md_phyaddr, dv_reg_phyaddr, ATTR_SDR, 0);
		bUse_GAlpha = 1;
		pr_info("### V_DV 1st Configuration :: (0x%x - 0x%x)\n", dv_reg_phyaddr, dv_md_phyaddr);
		vioc_v_dv_set_stage(DV_RUN);
	}
	else
	{
		VIOC_CONFIG_DV_SET_EDR_PATH(0);
		vioc_v_dv_set_stage(DV_OFF);
	}

	return 0;
}

int tca_edr_el_configure(struct tcc_lcdc_image_update *Src_ImageInfo, struct tcc_lcdc_image_update *El_ImageInfo, unsigned int *ratio)
{
	dvprintk("%s-%d : DV_PROC_CHECK(%d) En(%d) \n", __func__, __LINE__, DV_PROC_CHECK, Src_ImageInfo->enable);

	if( Src_ImageInfo->enable == 0 )
	{
		if((DV_PROC_CHECK & DV_DUAL_MODE) == DV_DUAL_MODE) {
			memcpy(El_ImageInfo, Src_ImageInfo, sizeof(struct tcc_lcdc_image_update));
			// sub layer
			El_ImageInfo->fmt = TCC_LCDC_IMG_FMT_YUV420SP;
			El_ImageInfo->Lcdc_layer = RDMA_VIDEO_SUB;
			DV_PROC_CHECK = 0;
			dvprintk("%s-%d : DV_PROC_CHECK(%d) \n", __func__, __LINE__, DV_PROC_CHECK);
		}
		else {
			return -1;
		}
	}
	else
	{
		if(Src_ImageInfo->private_data.dolbyVision_info.el_offset[0] == 0x00){
			DV_PROC_CHECK = 0;
			dvprintk("%s-%d : DV_PROC_CHECK(%d) \n", __func__, __LINE__, DV_PROC_CHECK);
			return -1;
		}

		if(Src_ImageInfo->private_data.dolbyVision_info.el_frame_width == 0)
		{
			printk("^@New^^^^^^^^^^^^^ @@@ %d/%d, %03d :: TS: %04d  %d bpp #BL(0x%x, %dx%d (%dx%d), 0x%x fmt) #EL(0x%x, %dx%d (%dx%d)) #OSD(0x%x/0x%x) #Reg(0x%x) #Meta(0x%x)\n",
					0, 0, Src_ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID], Src_ImageInfo->private_data.optional_info[VID_OPT_TIMESTAMP],
					Src_ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH],
					Src_ImageInfo->addr0, //Src_ImageInfo->private_data.offset[0],
					Src_ImageInfo->Frame_width, Src_ImageInfo->Frame_height, //Src_ImageInfo->private_data.optional_info[VID_OPT_FRAME_WIDTH], Src_ImageInfo->private_data.optional_info[3VID_OPT_FRAME_HEIGHT],
					Src_ImageInfo->crop_right - Src_ImageInfo->crop_left, //Src_ImageInfo->private_data.optional_info[VID_OPT_BUFFER_WIDTH],
					Src_ImageInfo->crop_bottom - Src_ImageInfo->crop_top, //Src_ImageInfo->private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
					Src_ImageInfo->fmt, //Src_ImageInfo->private_data.format,
					Src_ImageInfo->private_data.dolbyVision_info.el_offset[0],
					Src_ImageInfo->private_data.dolbyVision_info.el_buffer_width, Src_ImageInfo->private_data.dolbyVision_info.el_buffer_height,
					Src_ImageInfo->private_data.dolbyVision_info.el_frame_width, Src_ImageInfo->private_data.dolbyVision_info.el_frame_height,
					Src_ImageInfo->private_data.dolbyVision_info.osd_addr[0], Src_ImageInfo->private_data.dolbyVision_info.osd_addr[1],
					Src_ImageInfo->private_data.dolbyVision_info.reg_addr, Src_ImageInfo->private_data.dolbyVision_info.md_hdmi_addr);
		}
		*ratio = ((Src_ImageInfo->crop_right - Src_ImageInfo->crop_left)/Src_ImageInfo->private_data.dolbyVision_info.el_frame_width);

		memcpy(El_ImageInfo, Src_ImageInfo, sizeof(struct tcc_lcdc_image_update));
		// sub layer
		El_ImageInfo->Lcdc_layer 	= RDMA_VIDEO_SUB;
		// buffer resolution
		El_ImageInfo->Frame_width 	= El_ImageInfo->private_data.dolbyVision_info.el_buffer_width;
		El_ImageInfo->Frame_height 	= El_ImageInfo->private_data.dolbyVision_info.el_buffer_height;
		// display-region
		El_ImageInfo->Image_width 	= Src_ImageInfo->Image_width/(*ratio);
		El_ImageInfo->Image_height 	= Src_ImageInfo->Image_height/(*ratio);
		dvprintk("%s-%d :: EL Disp-W(%d = %d/%d), ratio(%d = (%d-%d)/%d \n", __func__, __LINE__,
					El_ImageInfo->Image_width, Src_ImageInfo->Image_width, *ratio,
					*ratio, Src_ImageInfo->crop_right, Src_ImageInfo->crop_left, Src_ImageInfo->private_data.dolbyVision_info.el_frame_width);
		// crop
		El_ImageInfo->crop_left 	= El_ImageInfo->crop_top = 0;
		El_ImageInfo->crop_right 	= El_ImageInfo->private_data.dolbyVision_info.el_frame_width;
		El_ImageInfo->crop_bottom 	= El_ImageInfo->private_data.dolbyVision_info.el_frame_height;
		// buffer address
		El_ImageInfo->addr0 		= El_ImageInfo->private_data.dolbyVision_info.el_offset[0];
		El_ImageInfo->addr1 		= El_ImageInfo->private_data.dolbyVision_info.el_offset[1];
		El_ImageInfo->addr2 		= El_ImageInfo->private_data.dolbyVision_info.el_offset[2];
		// no map-converter usage
		El_ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
		//temp!!
		//if(El_ImageInfo->fmt != TCC_LCDC_IMG_FMT_YUYV)
		//	El_ImageInfo->fmt = TCC_LCDC_IMG_FMT_YUV420SP;

		DV_PROC_CHECK = DV_DUAL_MODE;
		dvprintk("%s-%d : DV_PROC_CHECK(%d) \n", __func__, __LINE__, DV_PROC_CHECK);
	}

	return 1;
}
EXPORT_SYMBOL(tca_edr_el_configure);

void tca_edr_el_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo)
{
	volatile void __iomem *pSC;
	volatile void __iomem *pDISP;
	unsigned int iSCType = 0;
	unsigned int lcd_width = 0, lcd_height = 0;
	struct tcc_lcdc_image_update el_ImageInfo;
	unsigned int ratio = 1;

	dvprintk("^@New^^^^^^^^^^^^^ @@@ %d/%d, %03d/%03d :: TS: %04ld  %d bpp #BL(0x%x, %dx%d (%dx%d), 0x%x fmt) #EL(0x%x, %dx%d (%dx%d)) #OSD(0x%x/0x%x) #Reg(0x%x) #Meta(0x%x)\n",
			bStep_Check, DEF_DV_CHECK_NUM, nFrame, ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID], ImageInfo->private_data.optional_info[VID_OPT_TIMESTAMP],
			ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH],
			ImageInfo->addr0, //ImageInfo->private_data.offset[0],
			ImageInfo->Frame_width, ImageInfo->Frame_height, //ImageInfo->private_data.optional_info[VID_OPT_FRAME_WIDTH], ImageInfo->private_data.optional_info[VID_OPT_FRAME_HEIGHT],
			ImageInfo->crop_right - ImageInfo->crop_left, //ImageInfo->private_data.optional_info[VID_OPT_BUFFER_WIDTH],
			ImageInfo->crop_bottom - ImageInfo->crop_top, //ImageInfo->private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
			ImageInfo->fmt, //ImageInfo->private_data.format,
			ImageInfo->private_data.dolbyVision_info.el_offset[0],
			ImageInfo->private_data.dolbyVision_info.el_frame_width, ImageInfo->private_data.dolbyVision_info.el_frame_height,
			ImageInfo->private_data.dolbyVision_info.el_buffer_width, ImageInfo->private_data.dolbyVision_info.el_buffer_height,
			ImageInfo->private_data.dolbyVision_info.osd_addr[0], ImageInfo->private_data.dolbyVision_info.osd_addr[1],
			ImageInfo->private_data.dolbyVision_info.reg_addr, ImageInfo->private_data.dolbyVision_info.md_hdmi_addr);

	if(0 > tca_edr_el_configure(ImageInfo, &el_ImageInfo, &ratio))
		return;

//	pr_info("%s enable:%d, layer:%d, addr:0x%x, ts:%d, fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d  map :%d \n", __func__, el_ImageInfo.enable, el_ImageInfo.Lcdc_layer, el_ImageInfo.addr0, el_ImageInfo.time_stamp,
//			el_ImageInfo.fmt,el_ImageInfo.Frame_width, el_ImageInfo.Frame_height, el_ImageInfo.Image_width, el_ImageInfo.Image_height, el_ImageInfo.fmt, el_ImageInfo.on_the_fly, el_ImageInfo.private_data.optional_info[VID_OPT_HAVE_MC_INFO]);

	if((el_ImageInfo.Lcdc_layer >= RDMA_MAX_NUM) || (el_ImageInfo.fmt >TCC_LCDC_IMG_FMT_MAX)){
		pr_err("LCD :: lcdc:%d, enable:%d, layer:%d, addr:0x%x, fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d\n",
		       get_vioc_index(pdp_data->ddc_info.blk_num), el_ImageInfo.enable, el_ImageInfo.Lcdc_layer, el_ImageInfo.addr0,
		       el_ImageInfo.fmt,el_ImageInfo.Frame_width, el_ImageInfo.Frame_height, el_ImageInfo.Image_width, el_ImageInfo.Image_height, el_ImageInfo.fmt, el_ImageInfo.on_the_fly);
		return;
	}

	if(!el_ImageInfo.enable)	{
		//map converter
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
			pr_info("%s for layer-disable: blk_num:  0x%x \n", __func__, component_num);

			if(component_num < 0) {
				pr_info("%s : RDMA :%d dma path selection none\n", __func__, get_vioc_index(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num));
			}
			#ifdef CONFIG_VIOC_MAP_DECOMP
			else if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX)))

			{
				tca_map_convter_onoff(component_num, 0, 1);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(component_num);	 //disable it to prevent system-hang!!
			}
			#endif
			#if 0//def CONFIG_VIOC_DTRC_DECOMP
			else if((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX)))
			{
				tca_dtrc_convter_onoff(component_num, 0, 1);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				//tca_dtrc_convter_swreset(component_num); //disable it to prevent system-hang!!
			}
			#endif
			else if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
			{
				volatile void __iomem *pVRDMA = VIOC_RDMA_GetAddress(component_num);
				VIOC_RDMA_SetImageDisable(pVRDMA);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
			}

			// It is default path selection(VRDMA)
			VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num, pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
		}
		VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr);

		#ifdef CONFIG_PLATFORM_AVN
		/* TODO: Will be deprecated.
		 * If CLS Platform disable below code is normal, But problem occurs in ALS
		 */
		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_CLEAR);
		#endif

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num) != -1) //scaler plug in status check
		{
			iSCType = VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
			VIOC_CONFIG_PlugOut(iSCType);
			VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_CLEAR);
			pr_info("Scaler-%d plug out for %d layer \n", get_vioc_index(iSCType), el_ImageInfo.Lcdc_layer);
		}
		return;
	}

	iSCType = tca_get_scaler_num(pdp_data->DispDeviceType, el_ImageInfo.Lcdc_layer);

	pSC = VIOC_SC_GetAddress(iSCType);

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	if((!lcd_width) || (!lcd_height)){
		pr_err("%s LCD :: Error :: lcd_width %d, lcd_height %d \n", __func__,lcd_width, lcd_height);
		return;
	}

	if(el_ImageInfo.on_the_fly)
	{
		pDISP = (volatile void __iomem *)pdp_data->ddc_info.virt_addr;
		/*VIOC_SC_SetSrcSize(pSC, el_ImageInfo.Frame_width, el_ImageInfo.Frame_height);*/		// TODO: Will be deprecated.
		VIOC_SC_SetDstSize (pSC, el_ImageInfo.Image_width, el_ImageInfo.Image_height);			// set destination size in scaler
		VIOC_SC_SetOutSize (pSC, el_ImageInfo.Image_width, el_ImageInfo.Image_height);			// set output size in scaler
		if(!(__raw_readl(pDISP+DCTRL) & DCTRL_NI_MASK)) {
			VIOC_SC_SetBypass(pSC, OFF);
			dprintk("%s  Scaler-%d is plug in SetBypass OFF \n",__func__, get_vioc_index(iSCType));
		} else {
			if(((el_ImageInfo.crop_right - el_ImageInfo.crop_left) == el_ImageInfo.Image_width) && ((el_ImageInfo.crop_bottom - el_ImageInfo.crop_top) == el_ImageInfo.Image_height)){
				VIOC_SC_SetBypass (pSC, ON);
				dprintk("%s  Scaler-%d is plug in SetBypass ON \n",__func__, get_vioc_index(iSCType));
			}else {
				VIOC_SC_SetBypass (pSC, OFF);
				dprintk("%s  Scaler-%d is plug in SetBypass OFF \n",__func__, get_vioc_index(iSCType));
			}
		}

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num) == -1) //scaler plug in status check
		{
			VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr);
			dprintk("%s Scaler-%d is plug in RDMA-%d \n", __func__,
				get_vioc_index(iSCType), get_vioc_index(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num));
			VIOC_CONFIG_PlugIn (iSCType, pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
		}
	}
	else
	{
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num) != -1)
		{
			dprintk("%s Scaler-%d is plug out \n", __func__, get_vioc_index(iSCType));
			VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr);
			VIOC_CONFIG_PlugOut(iSCType);
		}
	}
	if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num) != -1)
		VIOC_SC_SetUpdate (pSC);

#if 0
	// position
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, el_ImageInfo.Lcdc_layer, 0, 0);


	VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, true);
	VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0x2); /* Y2RMode Default 0 (Studio Color) */

	VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, false);
	VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.fmt, el_ImageInfo.Frame_width);
	VIOC_RDMA_SetImageSize(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, lcd_width, lcd_height);
	VIOC_RDMA_SetImageFormat(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.fmt);

	VIOC_RDMA_SetImageBase(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.addr0, el_ImageInfo.addr1, el_ImageInfo.addr2);

	VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0);

	VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);

	VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
	VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr);

#else
	// position
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, el_ImageInfo.Lcdc_layer, el_ImageInfo.offset_x, el_ImageInfo.offset_y);
	VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);

	//map converter
	#ifdef CONFIG_VIOC_MAP_DECOMP
	if(el_ImageInfo.private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
	{
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
			if((component_num < VIOC_MC0 ) || (component_num > (VIOC_MC0 + VIOC_MC_MAX))){
				pr_info("%s Map converter  %d \n",__func__, nDeCompressor_Main);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(VIOC_MC0 + nDeCompressor_Main);
				VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num, VIOC_MC0 + nDeCompressor_Main);
			}
		}

#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		tca_map_convter_set(VIOC_MC0 + nDeCompressor_Main, &el_ImageInfo, 0);
#else
		tca_map_convter_set(VIOC_MC0 + nDeCompressor_Main, &el_ImageInfo, 1);
#endif
	}
	else
	#endif//
	#if 0//def CONFIG_VIOC_DTRC_DECOMP
	if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0)
	{
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
			if((component_num < VIOC_DTRC0 ) || (component_num > (VIOC_DTRC0 + VIOC_DTRC_MAX))){
				pr_info("%s DTRC converter  %d \n",__func__, nDeCompressor_Main);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				//tca_dtrc_convter_swreset(VIOC_DTRC0 + nDeCompressor_Main);
				VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num, VIOC_DTRC0 + nDeCompressor_Main);
			}
		}

		tca_dtrc_convter_set(VIOC_DTRC0 + nDeCompressor_Main, &el_ImageInfo, 1);
	}
	else
	#endif//
	{
		if(VIOC_CONFIG_DMAPath_Support()) {
			#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
			if(component_num < 0)
				pr_info(" %s  : RDMA :%d dma path selection none\n", __func__, get_vioc_index(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num));
			else if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
				VIOC_CONFIG_DMAPath_UnSet(component_num);

			// It is default path selection(VRDMA)
			VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num, pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
			#endif
		}

		#if 0	/* TODO */
		if(el_ImageInfo.fmt == TCC_LCDC_IMG_FMT_COMP)
			VIOC_CONFIG_PlugIn(VIOC_FCDEC0, pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].blk_num);
		else
		#endif
		else if(el_ImageInfo.fmt >= TCC_LCDC_IMG_FMT_444SEP && el_ImageInfo.fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)
		{
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, false);
#else
			VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, true);
#endif
			VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0x2); /* Y2RMode Default 0 (Studio Color) */

			if( el_ImageInfo.Frame_width <= RDMA_UVI_MAX_WIDTH )
				VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, true);
			else
				VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, false);
		}
		else
		{
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			VIOC_RDMA_SetImageR2YEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, true);
			VIOC_RDMA_SetImageR2YMode(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 1); /* Y2RMode Default 0 (Studio Color) */
#endif
		}

		if(el_ImageInfo.one_field_only_interlace
	#ifdef CONFIG_VIOC_10BIT
			|| (el_ImageInfo.private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
	#endif//CONFIG_VIOC_10BIT
		)		{
			VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.fmt, el_ImageInfo.Frame_width*2);
		}
	#ifdef CONFIG_VIOC_10BIT
		else if(el_ImageInfo.private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.fmt, el_ImageInfo.Frame_width*125/100);
		}
	#endif
		else {
			VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.fmt, el_ImageInfo.Frame_width);
		}

	#ifdef CONFIG_VIOC_10BIT
		if(el_ImageInfo.private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
			VIOC_RDMA_SetDataFormat(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0x1, 0x1);	/* YUV 10bit(16bit) support */
		else if(el_ImageInfo.private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
			VIOC_RDMA_SetDataFormat(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0x3, 0x0);	/* YUV real 10bit support */
		else
			VIOC_RDMA_SetDataFormat(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0x0, 0x0);	/* YUV 8bit support */
	#endif

		VIOC_RDMA_SetImageFormat(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.fmt);
		VIOC_RDMA_SetImageSize(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.crop_right - el_ImageInfo.crop_left, el_ImageInfo.crop_bottom - el_ImageInfo.crop_top);

		if( !( ((el_ImageInfo.crop_left == 0) && (el_ImageInfo.crop_right == el_ImageInfo.Frame_width)) &&  ((el_ImageInfo.crop_top == 0) && (el_ImageInfo.crop_bottom == el_ImageInfo.Frame_height)))  )
		{
			int addr_Y = (unsigned int)el_ImageInfo.addr0;
			int addr_U = (unsigned int)el_ImageInfo.addr1;
			int addr_V = (unsigned int)el_ImageInfo.addr2;

			dprintk(" Image Crop left=[%d], right=[%d], top=[%d], bottom=[%d] \n", el_ImageInfo.crop_left, el_ImageInfo.crop_right, el_ImageInfo.crop_top, el_ImageInfo.crop_bottom);

			tccxxx_GetAddress(el_ImageInfo.fmt, (unsigned int)el_ImageInfo.addr0, el_ImageInfo.Frame_width, el_ImageInfo.Frame_height,
									el_ImageInfo.crop_left, el_ImageInfo.crop_top, &addr_Y, &addr_U, &addr_V);

			if(el_ImageInfo.one_field_only_interlace)
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.crop_right - el_ImageInfo.crop_left, (el_ImageInfo.crop_bottom - el_ImageInfo.crop_top)/2);
			else
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.crop_right - el_ImageInfo.crop_left, el_ImageInfo.crop_bottom - el_ImageInfo.crop_top);

			VIOC_RDMA_SetImageBase(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, addr_Y, addr_U, addr_V);
		}
		else
		{
			dprintk(" don't Image Crop left=[%d], right=[%d], top=[%d], bottom=[%d] \n", el_ImageInfo.crop_left, el_ImageInfo.crop_right, el_ImageInfo.crop_top, el_ImageInfo.crop_bottom);
			if(el_ImageInfo.one_field_only_interlace)
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.Frame_width, el_ImageInfo.Frame_height/2);
			else
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.Frame_width, el_ImageInfo.Frame_height);

			VIOC_RDMA_SetImageBase(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, el_ImageInfo.addr0, el_ImageInfo.addr1, el_ImageInfo.addr2);
		}

		VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0);

		VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if(VIOC_CONFIG_DV_GET_EDR_PATH())
		{
			VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0);
			VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0);
			VIOC_RDMA_SetImageR2YEnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0);
			VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0);

			if(el_ImageInfo.Lcdc_layer == RDMA_VIDEO_SUB) {// || el_ImageInfo.Lcdc_layer == RDMA_LASTFRM){
				if(el_ImageInfo.Lcdc_layer == RDMA_LASTFRM
					&& (el_ImageInfo.fmt >= TCC_LCDC_IMG_FMT_444SEP && el_ImageInfo.fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1))
				{
					VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, true);
					VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr, 0x2);
				}

				VIOC_V_DV_SetSize(NULL, pdp_data->rdma_info[el_ImageInfo.Lcdc_layer].virt_addr,
									el_ImageInfo.offset_x/ratio, el_ImageInfo.offset_y/ratio,
									Hactive/ratio, Vactive/ratio);
			}
			else{
				pr_err("@@@@@@@@@@ 1 @@@@@@@@@ Should be implement other layer configuration. \n");
				return;
			}
		}
#endif
	}

//	tcc_video_post_process(ImageInfo);
#endif//
}

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI
#define USE_OWN_FB 0x12345678
extern void VIOC_RDMA_SetImageUpdate_for_CertiTest(volatile void __iomem *reg, unsigned int sw, unsigned int sh, unsigned int nBase0);
#endif
#define ALIGNED_WIDTH(w, mul) ( ( (unsigned int)w + (mul-1) ) & ~(mul-1) )
void tca_edr_vioc_set(unsigned int nRDMA, volatile void __iomem *pRDMA, dolby_layer_str_t stDolby_Layer,
						struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo,
						unsigned int width, unsigned int height, DV_DISP_TYPE type, unsigned int el_ratio)
{
	unsigned int iSCType;
	volatile void __iomem *pSC;
	unsigned int target_width, target_height;

	if(type == EDR_EL){
		target_width = Hactive / el_ratio;
		target_height = Vactive / el_ratio;
	}
	else {
		target_width = Hactive;
		target_height = Vactive;
	}

	iSCType = tca_get_scaler_num(TCC_OUTPUT_HDMI, ImageInfo->Lcdc_layer);
	pSC = VIOC_SC_GetAddress(iSCType);

	if(!ImageInfo->enable) {
		bStep_Check = DEF_DV_CHECK_NUM;
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			pr_info(" %s  for layer-disable: blk_num:  0x%x \n", __func__, component_num);

			if(component_num < 0) {
				pr_info(" %s  : RDMA :%d dma path selection none\n", __func__, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
			}
			#ifdef CONFIG_VIOC_MAP_DECOMP
			else if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX)))
			{
				tca_map_convter_onoff(component_num, 0, 1);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(component_num);	 //disable it to prevent system-hang!!
			}
			#endif
			else if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
			{
				volatile void __iomem *pVRDMA = VIOC_RDMA_GetAddress(component_num);
				VIOC_RDMA_SetImageDisable(pVRDMA);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
			}

			// It is default path selection(VRDMA)
			VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
		}

		VIOC_RDMA_SetImageDisable(pRDMA);

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1) //scaler plug in status check
		{
			iSCType = VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			VIOC_CONFIG_PlugOut(iSCType);
			VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_CLEAR);
			pr_info("scaler %d plug out for %d layer \n", get_vioc_index(iSCType), ImageInfo->Lcdc_layer);
		}
		return;
	}

	//map converter
#ifdef CONFIG_VIOC_MAP_DECOMP
	if(type == EDR_BL && ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
	{
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			if((component_num < VIOC_MC0 ) || (component_num > (VIOC_MC0 + VIOC_MC_MAX))){
				//pr_info("%s Map converter  %d \n",__func__, get_vioc_index(pdp_data->ddc_info.blk_num));
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(VIOC_MC0);
				VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_MC0);
			}
		}

	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
//		tca_map_convter_set(VIOC_MC0 + pdp_data->ddc_info.blk_num, ImageInfo, 0);
	#else
//		tca_map_convter_set(VIOC_MC0 + pdp_data->ddc_info.blk_num, ImageInfo, 1);
	#endif
	}
	else
#endif//
	{
		if(VIOC_CONFIG_DV_GET_EDR_PATH())
		{
			// Initialize
			VIOC_RDMA_SetImageUVIEnable(pRDMA, 0);
			VIOC_RDMA_SetImageY2REnable(pRDMA, 0);
			VIOC_RDMA_SetImageIntl(pRDMA, 0);
		}
		else
		{
			VIOC_RDMA_SetImageY2REnable(pRDMA, true);
			VIOC_RDMA_SetImageY2RMode(pRDMA, 0x2); /* Y2RMode Default 0 (Studio Color) */

			if( stDolby_Layer.frame_width <= RDMA_UVI_MAX_WIDTH )
				VIOC_RDMA_SetImageUVIEnable(pRDMA, true);
			else
				VIOC_RDMA_SetImageUVIEnable(pRDMA, false);
		}

#ifdef CONFIG_VIOC_10BIT
		if(stDolby_Layer.bit10)
		{
			VIOC_RDMA_SetDataFormat(pRDMA, 0x1, 0x1);	/* YUV 10bit(16bit) support */
			VIOC_RDMA_SetImageOffset(pRDMA, stDolby_Layer.format, stDolby_Layer.buffer_width*2);
		}
		else
#endif//CONFIG_VIOC_10BIT
		{
			VIOC_RDMA_SetDataFormat(pRDMA, 0x0, 0x0);	/* YUV 8bit support */
			VIOC_RDMA_SetImageOffset(pRDMA, stDolby_Layer.format, stDolby_Layer.buffer_width);
		}

		VIOC_RDMA_SetImageFormat(pRDMA, stDolby_Layer.format);
		VIOC_RDMA_SetImageSize(pRDMA, stDolby_Layer.frame_width, stDolby_Layer.frame_height);
		VIOC_RDMA_SetImageBase(pRDMA, stDolby_Layer.offset[0], stDolby_Layer.offset[1], stDolby_Layer.offset[2]);
		dvprintk(" =============> Set 0x%x address for 0x%x RDMA \n", stDolby_Layer.offset[0], pRDMA);

	#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI
		if(ImageInfo->Lcdc_layer == RDMA_FB)
			VIOC_RDMA_SetImageUpdate_for_CertiTest(pRDMA, stDolby_Layer.frame_width, stDolby_Layer.frame_height, stDolby_Layer.offset[0]);
		else
	#endif
		{
			VIOC_RDMA_SetImageUpdate(pRDMA);
		}
	}

	if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) == -1)
	{
		//pr_info(" %s Scaler-%d is plug in RDMA-%d \n", __func__, get_vioc_index(iSCType), get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
		//VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);
		VIOC_CONFIG_PlugIn (iSCType, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
	}

	VIOC_SC_SetDstSize (pSC, target_width, target_height);			// set destination size in scaler
	VIOC_SC_SetOutSize (pSC, target_width, target_height);			// set output size in scaler

	if(width != target_width && height != target_height)
		VIOC_SC_SetBypass (pSC, OFF);
	else
		VIOC_SC_SetBypass (pSC, ON);
	VIOC_SC_SetUpdate (pSC);

	VIOC_V_DV_SetSize(NULL, pRDMA, 0, 0, target_width, target_height);

}

static struct tcc_lcdc_image_update nAccumulated_ImageInfo[240];
static unsigned int nINT_Total = 0, nDisp_Try = 0, nDisp_Proc = 0, nDV_Updated = 0;
static unsigned int nCopyed_Max = 0;
static struct timeval t1;

void tca_edr_inc_check_count(unsigned int nInt, unsigned int nTry, unsigned int nProc, unsigned int nUpdated, unsigned int bInit_all)
{
	if(bInit_all)
	{
		nINT_Total = nDisp_Try = nDisp_Proc = nDV_Updated = 0;
	}
	else
	{
		if(nInt)
			nINT_Total++;
		if(nTry)
			nDisp_Try++;
		if(nProc)
			nDisp_Proc++;
		if(nUpdated)
			nDV_Updated++;
	}
}
EXPORT_SYMBOL(tca_edr_inc_check_count);

int tca_edr_GetTimediff_ms(struct timeval time1, struct timeval time2)
{
    int time_diff_ms = 0;

    time_diff_ms = (time2.tv_sec- time1.tv_sec)*1000;
    time_diff_ms += (time2.tv_usec-time1.tv_usec)/1000;

    return time_diff_ms;
}

void tca_edr_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo)
{
	volatile void __iomem *pRDMA = NULL;
	unsigned int nRDMA = 0;
	dolby_layer_str_t stDolby_BL, stDolby_EL, stDolby_OSD, stDolby_OSD_1;
	unsigned int ratio = 1;

	if(!ImageInfo->enable)
	{
		int i = 0;
		ImageInfo->Lcdc_layer = RDMA_VIDEO;
		nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
		pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
		tca_edr_vioc_set(nRDMA, pRDMA, stDolby_BL, pdp_data, ImageInfo, Hactive, Vactive, EDR_BL, ratio);
	#ifdef CONFIG_TCC_HDMI_DRIVER_V2_0
		set_hdmi_drm(DRM_OFF, &ImageInfo, ImageInfo->Lcdc_layer);
	#endif

		ImageInfo->Lcdc_layer = RDMA_VIDEO_SUB;
		nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
		pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
		tca_edr_vioc_set(nRDMA, pRDMA, stDolby_EL, pdp_data, ImageInfo, Hactive, Vactive, EDR_EL, ratio);
		pr_info("%s @ :: RDMA for EDR is diabled\n", __func__);

		ImageInfo->Lcdc_layer = RDMA_FB1;
		nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
		pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
		tca_edr_vioc_set(nRDMA, pRDMA, stDolby_OSD_1, pdp_data, ImageInfo, Hactive, Vactive, /*EDR_OSD1*/RDMA_FB1, ratio);

		pr_info("%s @ :: Finish Proc => nINT_Total(%03d) nDisp_Try(%03d) nDisp_Proc(%03d) nDV_Updated(%03d) \n", __func__, nINT_Total, nDisp_Try, nDisp_Proc, nDV_Updated);

		for(i = 0; i < nCopyed_Max; i++){
			if(nAccumulated_ImageInfo[i].private_data.dolbyVision_info.osd_addr[0] != USE_OWN_FB)
			{
				if( i != nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BUFFER_ID])
				{
					printk("^@ ID[%03d] Gap[---] :######################: is not displayed!!!! \n", i);
				}
				else
				{
					printk("^@ ID[%03d] Gap[%03d] F[%01d/%01d] TS[%04ld/%04ld] %d bpp #Type[%d/%d] #BL(MC(%d), 0x%x, %dx%d (%dx%d), 0x%x fmt) #EL(0x%x, %dx%d (%dx%d)) #OSD(0x%x/0x%x) #Reg(0x%x) #Meta(0x%x)\n",
							nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BUFFER_ID],
							nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_RESERVED_1],
							(nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_RESERVED_2] >> 16) & 0xFF, //readable buffer
							nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_RESERVED_2] & 0xFF, // valid buffer
							//nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_TIMESTAMP], nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_SYNC_TIME],
							nAccumulated_ImageInfo[i].time_stamp, nAccumulated_ImageInfo[i].sync_time,
							nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BIT_DEPTH],
							vioc_get_out_type(), nAccumulated_ImageInfo[i].private_data.dolbyVision_info.reg_out_type,
							nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_HAVE_MC_INFO],
							nAccumulated_ImageInfo[i].addr0, //nAccumulated_ImageInfo[i].private_data.offset[0],
							nAccumulated_ImageInfo[i].crop_right - nAccumulated_ImageInfo[i].crop_left, //nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_FRAME_WIDTH],
							nAccumulated_ImageInfo[i].crop_bottom - nAccumulated_ImageInfo[i].crop_top, //nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_FRAME_HEIGHT],
							nAccumulated_ImageInfo[i].Frame_width, nAccumulated_ImageInfo[i].Frame_height, //nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BUFFER_WIDTH], nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BUFFER_HEIGHT],
							nAccumulated_ImageInfo[i].fmt, //nAccumulated_ImageInfo[i].private_data.format,
							nAccumulated_ImageInfo[i].private_data.dolbyVision_info.el_offset[0],
							nAccumulated_ImageInfo[i].private_data.dolbyVision_info.el_frame_width, nAccumulated_ImageInfo[i].private_data.dolbyVision_info.el_frame_height,
							nAccumulated_ImageInfo[i].private_data.dolbyVision_info.el_buffer_width, nAccumulated_ImageInfo[i].private_data.dolbyVision_info.el_buffer_height,
							nAccumulated_ImageInfo[i].private_data.dolbyVision_info.osd_addr[0], nAccumulated_ImageInfo[i].private_data.dolbyVision_info.osd_addr[1],
							nAccumulated_ImageInfo[i].private_data.dolbyVision_info.reg_addr, nAccumulated_ImageInfo[i].private_data.dolbyVision_info.md_hdmi_addr);
				}
			}
		}
		nCopyed_Max = 0;
		for(i = 0; i < 240; i++){
			nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BUFFER_ID] = 0xffff;
		}
		return;
	}
	else
	{
	    int time_gap_ms = 0;
		struct timeval t0;

		t0 = t1;
		do_gettimeofday( &t1 );

		if(nCopyed_Max == 0)
			time_gap_ms = 0;
		else
		    time_gap_ms = tca_edr_GetTimediff_ms(t0, t1);
		ImageInfo->private_data.optional_info[VID_OPT_RESERVED_1] = time_gap_ms;

		if(ImageInfo->private_data.dolbyVision_info.osd_addr[0] != USE_OWN_FB){
			if(ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID] < 240){
				memcpy(&nAccumulated_ImageInfo[ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID]], ImageInfo, sizeof(struct tcc_lcdc_image_update));
				nCopyed_Max = ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID] + 1;
			}
		}

		stDolby_BL.offset[0]		= ImageInfo->private_data.offset[0];
		stDolby_BL.offset[1]		= ImageInfo->private_data.offset[1];
		stDolby_BL.offset[2]		= ImageInfo->private_data.offset[2];
		stDolby_BL.format			= ImageInfo->fmt;
		stDolby_BL.buffer_width 	= ImageInfo->private_data.optional_info[VID_OPT_BUFFER_WIDTH];
		stDolby_BL.buffer_height	= ImageInfo->private_data.optional_info[VID_OPT_BUFFER_HEIGHT];
		stDolby_BL.frame_width 		= ImageInfo->private_data.optional_info[VID_OPT_FRAME_WIDTH];
		stDolby_BL.frame_height		= ImageInfo->private_data.optional_info[VID_OPT_FRAME_HEIGHT];
		stDolby_BL.bit10			= ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10 ? 1 : 0;

		stDolby_EL.offset[0]		= ImageInfo->private_data.dolbyVision_info.el_offset[0];
		stDolby_EL.offset[1]		= ImageInfo->private_data.dolbyVision_info.el_offset[1];
		stDolby_EL.offset[2]		= ImageInfo->private_data.dolbyVision_info.el_offset[2];
		stDolby_EL.format			= TCC_LCDC_IMG_FMT_YUV420ITL0;
		stDolby_EL.buffer_width 	= ImageInfo->private_data.dolbyVision_info.el_buffer_width;
		stDolby_EL.buffer_height	= ImageInfo->private_data.dolbyVision_info.el_buffer_height;
		stDolby_EL.frame_width 		= ImageInfo->private_data.dolbyVision_info.el_frame_width;
		stDolby_EL.frame_height		= ImageInfo->private_data.dolbyVision_info.el_frame_height;
		stDolby_EL.bit10			= stDolby_BL.bit10;

		stDolby_OSD.offset[0]		= ImageInfo->private_data.dolbyVision_info.osd_addr[0];
		stDolby_OSD.format			= TCC_LCDC_IMG_FMT_RGB888;
		stDolby_OSD.buffer_width 	= ImageInfo->private_data.dolbyVision_info.osd_width;
		stDolby_OSD.buffer_height	= ImageInfo->private_data.dolbyVision_info.osd_height;
		stDolby_OSD.frame_width 	= ImageInfo->private_data.dolbyVision_info.osd_width;
		stDolby_OSD.frame_height	= ImageInfo->private_data.dolbyVision_info.osd_height;
		stDolby_OSD.bit10			= 0;

		stDolby_OSD_1.offset[0]		= ImageInfo->private_data.dolbyVision_info.osd_addr[1];
		stDolby_OSD_1.format		= TCC_LCDC_IMG_FMT_RGB888;
		stDolby_OSD_1.buffer_width 	= ImageInfo->private_data.dolbyVision_info.osd_width;
		stDolby_OSD_1.buffer_height	= ImageInfo->private_data.dolbyVision_info.osd_height;
		stDolby_OSD_1.frame_width 	= ImageInfo->private_data.dolbyVision_info.osd_width;
		stDolby_OSD_1.frame_height	= ImageInfo->private_data.dolbyVision_info.osd_height;
		stDolby_OSD_1.bit10			= 0;

		if(stDolby_EL.offset[0] != 0x00 && stDolby_EL.frame_width != 0 && stDolby_EL.frame_height != 0)
			ratio = (stDolby_BL.frame_width/stDolby_EL.frame_width);
		else
			ratio = 1;

		if(ratio == 0)
		{
			printk("@@@@@@@@@@@@@@@@@@ ID[%d] BL(%dx%d) EL(0x%x %dx%d)\n", ImageInfo->private_data.optional_info[VID_OPT_BUFFER_ID],
						stDolby_BL.frame_width, stDolby_BL.frame_height, stDolby_EL.offset[0], stDolby_EL.frame_width, stDolby_EL.frame_height);
			return;
		}

		dvprintk("%s @1 sc(%d), reg(0x%x), md(0x%x) %d bpp @BL(0x%x, %d x %d)	@EL(0x%x, %d x %d) @OSD(0x%x)\n",
				__func__,
				vioc_v_dv_get_sc(),
				ImageInfo->private_data.dolbyVision_info.reg_addr,
				ImageInfo->private_data.dolbyVision_info.md_hdmi_addr,
				stDolby_BL.bit10,
				stDolby_BL.offset[0], stDolby_BL.frame_width, stDolby_BL.frame_height,
				stDolby_EL.offset[0], stDolby_EL.frame_width, stDolby_EL.frame_height,
				stDolby_OSD.offset[0]);

		if(stDolby_BL.offset[0] != 0x00 && stDolby_BL.frame_width != 0 && stDolby_BL.frame_height != 0){
			ImageInfo->Lcdc_layer = RDMA_VIDEO;
	#if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
			set_hdmi_drm(DRM_ON, ImageInfo, ImageInfo->Lcdc_layer);
	#endif
			if(!VIOC_CONFIG_DV_GET_EDR_PATH())
			{
				VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, ImageInfo->Lcdc_layer, 0, 0);
				VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
			}
			nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
			pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
			tca_edr_vioc_set(nRDMA, pRDMA, stDolby_BL, pdp_data, ImageInfo, stDolby_BL.frame_width, stDolby_BL.frame_height, EDR_BL, ratio);
		}

		if(stDolby_EL.offset[0] != 0x00 && stDolby_EL.frame_width != 0 && stDolby_EL.frame_height != 0) {
			ImageInfo->Lcdc_layer = RDMA_VIDEO_SUB;
			if(!VIOC_CONFIG_DV_GET_EDR_PATH())
			{
				VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, ImageInfo->Lcdc_layer, 0, 0);
				VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
			}
			nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
			pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
			tca_edr_vioc_set(nRDMA, pRDMA, stDolby_EL, pdp_data, ImageInfo, stDolby_EL.frame_width, stDolby_EL.frame_height, EDR_EL, ratio);
		}
	#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI
		if(stDolby_OSD.offset[0] != 0x00  && stDolby_OSD.offset[0] != USE_OWN_FB && stDolby_OSD.frame_width != 0 && stDolby_OSD.frame_height != 0) {
			ImageInfo->Lcdc_layer = RDMA_FB;
			if(!VIOC_CONFIG_DV_GET_EDR_PATH())
			{
				VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, ImageInfo->Lcdc_layer, 0, 0);
				VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
			}

			nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
			pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
			tca_edr_vioc_set(nRDMA, pRDMA, stDolby_OSD, pdp_data, ImageInfo, stDolby_OSD.frame_width, stDolby_OSD.frame_height, /*EDR_OSD3*/RDMA_FB, ratio);
		}

		if(stDolby_OSD_1.offset[0] != 0x00 && stDolby_OSD_1.frame_width != 0 && stDolby_OSD_1.frame_height != 0) {
			int enabled = 0;

			ImageInfo->Lcdc_layer = RDMA_FB1;
			if(!VIOC_CONFIG_DV_GET_EDR_PATH())
			{
				VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, ImageInfo->Lcdc_layer, 0, 0);
				VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
			}

			nRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num;
			pRDMA = pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr;
			tca_edr_vioc_set(nRDMA, pRDMA, stDolby_OSD_1, pdp_data, ImageInfo, stDolby_OSD_1.frame_width, stDolby_OSD_1.frame_height, /*EDR_OSD1*/RDMA_FB1, ratio);
			VIOC_RDMA_GetImageEnable(pRDMA, &enabled);
			if(!enabled){
				VIOC_RDMA_SetImageAlphaSelect(pRDMA, 1);
				VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
				VIOC_RDMA_SetImageEnable(pRDMA);
			}
		}

	#endif

		if(VIOC_CONFIG_DV_GET_EDR_PATH()){
	#ifdef CONFIG_VIOC_MAP_DECOMP
			if(stDolby_EL.offset[0] != 0x00 && stDolby_EL.frame_width!= 0 && stDolby_EL.frame_height!= 0)
				VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[RDMA_VIDEO_SUB].virt_addr);

			ImageInfo->Lcdc_layer = RDMA_VIDEO;

			if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
			{
		#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
				tca_map_convter_set(VIOC_MC0 + pdp_data->ddc_info.blk_num, ImageInfo, 0);
		#else
				tca_map_convter_set(VIOC_MC0 + pdp_data->ddc_info.blk_num, ImageInfo, 1);
		#endif
			}
			else
	#endif//
			{
				if(vioc_get_out_type() == ImageInfo->private_data.dolbyVision_info.reg_out_type)
				{
					if(stDolby_BL.offset[0] != 0x00 && stDolby_BL.frame_width != 0 && stDolby_BL.frame_height != 0)
						VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[RDMA_VIDEO].virt_addr);

					vioc_v_dv_prog( ImageInfo->private_data.dolbyVision_info.md_hdmi_addr,
										ImageInfo->private_data.dolbyVision_info.reg_addr,
										ImageInfo->private_data.optional_info[VID_OPT_CONTENT_TYPE],
										1);
				}
				else
				{
					pr_err("0 Dolby Out type mismatch (%d != %d)\n", vioc_get_out_type(), ImageInfo->private_data.dolbyVision_info.reg_out_type);
				}
			}
		}

		dvprintk("@2 \n");
	}
}
EXPORT_SYMBOL(tca_edr_display_update);
#endif
#endif

struct tcc_dp_device *tca_get_displayType(TCC_OUTPUT_TYPE check_type)
{
	struct fb_info *info = registered_fb[0];
	struct tccfb_info *tccfb_info = NULL;
	struct tcc_dp_device *dp_device = NULL;

	if(!info)
		return NULL;

	tccfb_info = info->par;

	if(!tccfb_info)
		return NULL;

	if(tccfb_info->pdata.Mdp_data.DispDeviceType == check_type)
		dp_device = &tccfb_info->pdata.Mdp_data;
	else if(tccfb_info->pdata.Sdp_data.DispDeviceType == check_type)
		dp_device = &tccfb_info->pdata.Sdp_data;

	return dp_device;
}

void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo)
{
	volatile void __iomem *pSC;
	int iSCType;
	unsigned int lcd_width = 0, lcd_height = 0;

//	pr_info("%s enable:%d, layer:%d, addr:0x%x, ts:%d, fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d  map :%d \n", __func__, ImageInfo->enable, ImageInfo->Lcdc_layer, ImageInfo->addr0, ImageInfo->time_stamp,
//			ImageInfo->fmt,ImageInfo->Frame_width, ImageInfo->Frame_height, ImageInfo->Image_width, ImageInfo->Image_height, ImageInfo->fmt, ImageInfo->on_the_fly, ImageInfo->private_data.optional_info[16]);
	if((!pdp_data) || (!ImageInfo))
		return;

	if((ImageInfo->Lcdc_layer >= RDMA_MAX_NUM) || (ImageInfo->fmt >TCC_LCDC_IMG_FMT_MAX)){
		pr_err("LCD :: lcdc:%d, enable:%d, layer:%d, addr:0x%x, fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d\n",
		       get_vioc_index(pdp_data->ddc_info.blk_num), ImageInfo->enable, ImageInfo->Lcdc_layer, ImageInfo->addr0,
		       ImageInfo->fmt,ImageInfo->Frame_width, ImageInfo->Frame_height, ImageInfo->Image_width, ImageInfo->Image_height, ImageInfo->fmt, ImageInfo->on_the_fly);
		return;
	}

#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if(!ImageInfo->enable)
	{
		bStep_Check = DEF_DV_CHECK_NUM;
	#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
		if(VIOC_CONFIG_DV_GET_EDR_PATH()){
		#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
			VIOC_RDMA_PreventEnable_for_UI(0, 0);
		#endif
			tca_edr_display_update(pdp_data, ImageInfo);
			vioc_v_dv_prog(dv_md_phyaddr, dv_reg_phyaddr, ATTR_SDR, 1);
			return;
		}
	#endif
	}
	else
	{
		#if 0//defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) // MC off
		ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] = 0;
		#endif
		if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_DOLBYVISION_INFO] != 0)
		{
			if(!bStep_Check){
				nFrame++;
		#if 0//defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST) // To display 1st frame only.
				return;
		#endif
			}
			else{
				if(bStep_Check == DEF_DV_CHECK_NUM){
					nFrame = 1;//0;
					bStep_Check--;
				}
				else{
					bStep_Check--;
					return;
				}
			}

	#if defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
			tca_edr_inc_check_count(0, 0, 1, 0, 0);
			tca_edr_display_update(pdp_data, ImageInfo);
		#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI // No UI-Blending
			if(ImageInfo->private_data.dolbyVision_info.osd_addr[0] != USE_OWN_FB)
				VIOC_RDMA_PreventEnable_for_UI(1, ImageInfo->private_data.dolbyVision_info.osd_addr[0] == 0x00 ? 1 : 0);
		#endif
			return;
	#endif
		}
	}

	#if !defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST)
	tca_edr_el_display_update(pdp_data, ImageInfo);
	#endif
#endif

//	pr_info("%s  =========>[%d] Start frame_count(%d) :: reg(0x%x), md(0x%x)\n", __func__, ImageInfo->private_data.optional_info[VID_OPT_DISP_OUT_IDX],
//					nFrame,
//					ImageInfo->private_data.dolbyVision_info.reg_addr,
//					ImageInfo->private_data.dolbyVision_info.md_hdmi_addr);

	if(!ImageInfo->enable)	{
#if !defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI)
		VIOC_RDMA_PreventEnable_for_UI(0, 0);
#endif

		//map converter
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			pr_info(" %s  for layer-disable: blk_num:  0x%x \n", __func__, component_num);

			if((int)component_num < 0) {
				pr_info(" %s  : RDMA :%d dma path selection none\n", __func__, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
			}
			#ifdef CONFIG_VIOC_MAP_DECOMP
			else if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX)))
			{
				tca_map_convter_onoff(component_num, 0, 1);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(component_num);	 //disable it to prevent system-hang!!
			}
			#endif
			#ifdef CONFIG_VIOC_DTRC_DECOMP
			else if((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX)))
			{
				tca_dtrc_convter_onoff(component_num, 0, 1);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				//tca_dtrc_convter_swreset(component_num); //disable it to prevent system-hang!!
			}
			#endif
			else if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
			{
				volatile void __iomem *pVRDMA = VIOC_RDMA_GetAddress(component_num);
				VIOC_RDMA_SetImageDisable(pVRDMA);
				VIOC_CONFIG_DMAPath_UnSet(component_num);
			}

			// It is default path selection(VRDMA)
			VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			#endif
		}

		VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);

		#ifdef CONFIG_PLATFORM_AVN
		/* TODO: Will be deprecated.
		 * If CLS Platform disable below code is normal, But problem occurs in ALS
		 */
		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_CLEAR);
		#endif

		if(ImageInfo->MVCframeView != 1){

			if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1) //scaler plug in status check
			{
				iSCType = VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
				VIOC_CONFIG_PlugOut(iSCType);
				VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_CLEAR);
				pr_info("scaler %d plug out for %d layer \n", get_vioc_index(iSCType), ImageInfo->Lcdc_layer);
			}
		}
		return;
	}

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0) && defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
	set_hdmi_drm(DRM_ON, ImageInfo, ImageInfo->Lcdc_layer);
        #endif

	iSCType = tca_get_scaler_num(pdp_data->DispDeviceType, ImageInfo->Lcdc_layer);

	pSC = VIOC_SC_GetAddress(iSCType);

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	if((!lcd_width) || (!lcd_height)){
		pr_err("%s LCD :: Error :: lcd_width %d, lcd_height %d \n", __func__,lcd_width, lcd_height);
		return;
	}

	if((ImageInfo->MVCframeView != 1) && ImageInfo->on_the_fly)
	{
		volatile void __iomem *pDISP = pdp_data->ddc_info.virt_addr;
		unsigned int bSCBypass = 0;

		/*VIOC_SC_SetSrcSize(pSC, ImageInfo->Frame_width, ImageInfo->Frame_height);*/		// TODO: Will be deprecated.
	#if defined(CONFIG_MC_WORKAROUND)
		if(!system_rev && ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
		{
			unsigned int plus_height = VIOC_SC_GetPlusSize((ImageInfo->crop_bottom - ImageInfo->crop_top), ImageInfo->Image_height);
			VIOC_SC_SetDstSize (pSC, ImageInfo->Image_width, ImageInfo->Image_height+plus_height);			// set destination size in scaler
		}
		else
	#endif
			VIOC_SC_SetDstSize (pSC, ImageInfo->Image_width, ImageInfo->Image_height);			// set destination size in scaler
		VIOC_SC_SetOutSize (pSC, ImageInfo->Image_width, ImageInfo->Image_height);			// set output size in scaler

		if( !(__raw_readl(pDISP+DCTRL) & DCTRL_NI_MASK)) {	//interlace output
			VIOC_SC_SetBypass(pSC, OFF);
			dprintk(" %s  scaler %d is plug in SetBypass OFF \n",__func__, get_vioc_index(iSCType));
		} else {
			if(( !ImageInfo->one_field_only_interlace && (((ImageInfo->crop_right - ImageInfo->crop_left) == ImageInfo->Image_width) && ((ImageInfo->crop_bottom - ImageInfo->crop_top) == ImageInfo->Image_height)))
				|| (ImageInfo->one_field_only_interlace && (((ImageInfo->crop_right - ImageInfo->crop_left) == ImageInfo->Image_width) && (((ImageInfo->crop_bottom - ImageInfo->crop_top)/2) == ImageInfo->Image_height)))
			){
			#if defined(CONFIG_MC_WORKAROUND)
				if(!system_rev && ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
					VIOC_SC_SetBypass (pSC, OFF);
				else
			#endif
					VIOC_SC_SetBypass (pSC, ON);
				bSCBypass = 1;
				dprintk(" %s  scaler %d is plug in SetBypass ON \n",__func__, get_vioc_index(iSCType));
			}else {
				VIOC_SC_SetBypass (pSC, OFF);
				dprintk(" %s  scaler %d is plug in SetBypass OFF \n",__func__, get_vioc_index(iSCType));
			}
		}

#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
		if(!bSCBypass)
#endif
		{
			if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) == -1) //scaler plug in status check
			{
				VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);
				dprintk(" %s  scaler 1 is plug in RDMA %d \n",__func__, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
				VIOC_CONFIG_PlugIn (iSCType, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			}
		}
#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
		else
		{
			if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1) //scaler plug in status check
			{
				VIOC_CONFIG_PlugOut(iSCType);
			}
		}
#endif
	}
	else
	{
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1)
		{
			dprintk(" %s  scaler 1 is plug out \n",__func__);
			VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);
			VIOC_CONFIG_PlugOut(iSCType);
		}
	}
	if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1)
		VIOC_SC_SetUpdate (pSC);

#if 0
	// position
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, ImageInfo->Lcdc_layer, 0, 0);


	VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, true);
	VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0x2); /* Y2RMode Default 0 (Studio Color) */

	VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, false);
	VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->fmt, ImageInfo->Frame_width);
	VIOC_RDMA_SetImageSize(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, lcd_width, lcd_height);
	VIOC_RDMA_SetImageFormat(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->fmt);

	VIOC_RDMA_SetImageBase(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->addr0, ImageInfo->addr1, ImageInfo->addr2);

	VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0);

	VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);

	VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);
 	VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);



#else
	// position
	VIOC_WMIX_SetPosition(pdp_data->wmixer_info.virt_addr, ImageInfo->Lcdc_layer, ImageInfo->offset_x, ImageInfo->offset_y);
	VIOC_WMIX_SetUpdate(pdp_data->wmixer_info.virt_addr);

	//map converter
	#ifdef CONFIG_VIOC_MAP_DECOMP
	if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0)
	{
		if(VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			if((component_num < VIOC_MC0 ) || (component_num > (VIOC_MC0 + VIOC_MC_MAX)))
				VIOC_CONFIG_DMAPath_UnSet(component_num);

			VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_MC0 + nDeCompressor_Main);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, VIOC_MC0 + nDeCompressor_Main);
			#endif
		}

		#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
		tca_map_convter_set(VIOC_MC0 + nDeCompressor_Main, ImageInfo, 0);
		#else
		tca_map_convter_set(VIOC_MC0 + nDeCompressor_Main, ImageInfo, 1);
		#endif

		#if !defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI)
		VIOC_RDMA_PreventEnable_for_UI(1, 1);
		#endif
	}
	else
	#endif//
	#ifdef CONFIG_VIOC_DTRC_DECOMP
	if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_DTRC_INFO] != 0)
	{
		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			if((component_num < VIOC_DTRC0 ) || (component_num > (VIOC_DTRC0 + VIOC_DTRC_MAX))){
				pr_info("%s DTRC converter  %d \n",__func__,get_vioc_index(pdp_data->ddc_info.blk_num));
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				//tca_dtrc_convter_swreset(VIOC_DTRC0 + nDeCompressor_Main);
				VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_DTRC0 + nDeCompressor_Main);
			}
		}

		tca_dtrc_convter_set(VIOC_DTRC0 + nDeCompressor_Main, ImageInfo, 1);
	}
	else
	#endif//
	{
		if(VIOC_CONFIG_DMAPath_Support()) {
			#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
			int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);

			if((int)component_num < 0) {
				pr_info(" %s  : RDMA :%d dma path selection none\n", __func__, get_vioc_index(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num));
			}
			else if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
				VIOC_CONFIG_DMAPath_UnSet(component_num);

			// It is default path selection(VRDMA)
			VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			#endif
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
			#endif
		}

		#if 0	/* TODO */
		if(ImageInfo->fmt == TCC_LCDC_IMG_FMT_COMP)
			VIOC_CONFIG_PlugIn(VIOC_FCDEC0, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
		else
		#endif
		if(ImageInfo->fmt >= TCC_LCDC_IMG_FMT_444SEP && ImageInfo->fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)
		{
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, false);
#else
			VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, true);
#endif
			VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0x2); /* Y2RMode Default 0 (Studio Color) */

			if( ImageInfo->Frame_width <= RDMA_UVI_MAX_WIDTH )
				VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, true);
			else
				VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, false);
		}
		else
		{
#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			VIOC_RDMA_SetImageR2YEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, true);
			VIOC_RDMA_SetImageR2YMode(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 1); /* Y2RMode Default 0 (Studio Color) */
#endif
		}

		if(ImageInfo->one_field_only_interlace
	#ifdef CONFIG_VIOC_10BIT
			|| (ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
	#endif//CONFIG_VIOC_10BIT
		)		{
			VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->fmt, ImageInfo->Frame_width*2);
		}
	#ifdef CONFIG_VIOC_10BIT
		else if(ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11) {
			VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->fmt, ImageInfo->Frame_width*125/100);
		}
	#endif
		else {
			VIOC_RDMA_SetImageOffset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->fmt, ImageInfo->Frame_width);
		}

		#ifdef CONFIG_VIOC_10BIT
		if(ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
			VIOC_RDMA_SetDataFormat(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0x1, 0x1);	/* YUV 10bit(16bit) support */
		else if(ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 11)
			VIOC_RDMA_SetDataFormat(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0x3, 0x0);	/* YUV real 10bit support */
		else
			VIOC_RDMA_SetDataFormat(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0x0, 0x0);	/* YUV 8bit support */
		#endif

		VIOC_RDMA_SetImageFormat(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->fmt);
		VIOC_RDMA_SetImageSize(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->crop_right - ImageInfo->crop_left, ImageInfo->crop_bottom - ImageInfo->crop_top);

		if( !( ((ImageInfo->crop_left == 0) && (ImageInfo->crop_right == ImageInfo->Frame_width)) &&  ((ImageInfo->crop_top == 0) && (ImageInfo->crop_bottom == ImageInfo->Frame_height)))  )
		{
			int addr_Y = (unsigned int)ImageInfo->addr0;
			int addr_U = (unsigned int)ImageInfo->addr1;
			int addr_V = (unsigned int)ImageInfo->addr2;

			dprintk(" Image Crop left=[%d], right=[%d], top=[%d], bottom=[%d] \n", ImageInfo->crop_left, ImageInfo->crop_right, ImageInfo->crop_top, ImageInfo->crop_bottom);

			tccxxx_GetAddress(ImageInfo->fmt, (unsigned int)ImageInfo->addr0, ImageInfo->Frame_width, ImageInfo->Frame_height,
									ImageInfo->crop_left, ImageInfo->crop_top, &addr_Y, &addr_U, &addr_V);

			if(ImageInfo->one_field_only_interlace)
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->crop_right - ImageInfo->crop_left, (ImageInfo->crop_bottom - ImageInfo->crop_top)/2);
			else
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->crop_right - ImageInfo->crop_left, ImageInfo->crop_bottom - ImageInfo->crop_top);

			VIOC_RDMA_SetImageBase(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, addr_Y, addr_U, addr_V);
		}
		else
		{
			dprintk(" don't Image Crop left=[%d], right=[%d], top=[%d], bottom=[%d] \n", ImageInfo->crop_left, ImageInfo->crop_right, ImageInfo->crop_top, ImageInfo->crop_bottom);
			if(ImageInfo->one_field_only_interlace)
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->Frame_width, ImageInfo->Frame_height/2);
			else
				VIOC_RDMA_SetImageSize(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->Frame_width, ImageInfo->Frame_height);

			VIOC_RDMA_SetImageBase(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, ImageInfo->addr0, ImageInfo->addr1, ImageInfo->addr2);
		}

		VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0);

		VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
		if(VIOC_CONFIG_DV_GET_EDR_PATH())
		{
			VIOC_RDMA_SetImageUVIEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0);
			VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0);
			VIOC_RDMA_SetImageR2YEnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0);
			VIOC_RDMA_SetImageIntl(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0);

			if(ImageInfo->Lcdc_layer == RDMA_VIDEO || ImageInfo->Lcdc_layer == RDMA_LASTFRM){

				if(ImageInfo->Lcdc_layer == RDMA_LASTFRM
					&& (ImageInfo->fmt >= TCC_LCDC_IMG_FMT_444SEP && ImageInfo->fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1))
				{
					VIOC_RDMA_SetImageY2REnable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, true);
					VIOC_RDMA_SetImageY2RMode(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr, 0x2);
				}

				if(vioc_get_out_type() == ImageInfo->private_data.dolbyVision_info.reg_out_type)
				{
					VIOC_V_DV_SetSize(NULL, pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr,
										ImageInfo->offset_x, ImageInfo->offset_y,
										Hactive, Vactive);
					vioc_v_dv_prog( ImageInfo->private_data.dolbyVision_info.md_hdmi_addr,
										ImageInfo->private_data.dolbyVision_info.reg_addr,
										ImageInfo->private_data.optional_info[VID_OPT_CONTENT_TYPE],
										1);
				}
			}
			else{
				pr_err("@@@@@@@@@@ 1 @@@@@@@@@ Should be implement other layer configuration. \n");
				return;
			}
		}
#endif
	}

	//Fix DTRC Underrun issue when playback start with scaler bypass mode.
	if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1)
		VIOC_SC_SetUpdate (pSC);

	tcc_video_post_process(ImageInfo);
#endif//
}

void tca_mvc_display_update(char hdmi_lcdc, struct tcc_lcdc_image_update *ImageInfo)
{
	volatile void __iomem *pDISPBase;
	volatile void __iomem *pWMIXBase;
	volatile void __iomem *pRDMABase0;
	volatile void __iomem *pRDMABase1;

	unsigned int lcd_width = 0, lcd_height = 0, iVBlank = 0;

	dprintk("%s enable:%d, layer:%d, fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d\n", __func__, ImageInfo->enable, ImageInfo->Lcdc_layer,
			ImageInfo->fmt,ImageInfo->Frame_width, ImageInfo->Frame_height, ImageInfo->Image_width, ImageInfo->Image_height, ImageInfo->fmt, ImageInfo->on_the_fly);


	if(hdmi_lcdc)
	{
		pDISPBase = VIOC_DISP_GetAddress(1);
		pWMIXBase = VIOC_WMIX_GetAddress(1);
		pRDMABase0 = VIOC_RDMA_GetAddress(6);
		pRDMABase1 = VIOC_RDMA_GetAddress(7);
	}
	else
	{
		pDISPBase = VIOC_DISP_GetAddress(0);
		pWMIXBase = VIOC_WMIX_GetAddress(0);
		pRDMABase0 = VIOC_RDMA_GetAddress(2);
		pRDMABase1 = VIOC_RDMA_GetAddress(3);
	}

	VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);

	if((!lcd_width) || (!lcd_height))
	{
		printk("%s - lcd width and hight is not normal!!!!\n", __func__);
		return;
	}

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
	iVBlank = hdmi_get_VBlank();
        #endif

	if(!ImageInfo->enable)	{
		VIOC_RDMA_SetImageDisable(pRDMABase0);
		VIOC_RDMA_SetImageDisable(pRDMABase1);
		printk("%s - Image Info is not enable, so RDAMA is disable.\n", __func__);
		return;
	}

        #if defined(CONFIG_TCC_HDMI_DRIVER_V2_0) && defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
	set_hdmi_drm(DRM_ON, ImageInfo, ImageInfo->Lcdc_layer);
        #endif

	//dprintk("%s lcdc:%d,pRDMA0:0x%08x pRDMA1:0x%08x, pWMIX:0x%08x, pDISP:0x%08x, addr0:0x%08x\n", __func__, hdmi_lcdc, pRDMABase0, pRDMABase1, pWMIXBase, pDISPBase, ImageInfo->addr0);
	//dprintk("%s enable:%d, layer:%d, fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d\n", __func__, ImageInfo->enable, ImageInfo->Lcdc_layer,
	//		ImageInfo->fmt,ImageInfo->Frame_width, ImageInfo->Frame_height, ImageInfo->Image_width, ImageInfo->Image_height, ImageInfo->fmt, ImageInfo->on_the_fly);

	if(ImageInfo->fmt >= TCC_LCDC_IMG_FMT_444SEP && ImageInfo->fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)
	{
		VIOC_RDMA_SetImageY2REnable(pRDMABase0, true);
		VIOC_RDMA_SetImageY2RMode(pRDMABase0, 0x2); /* Y2RMode Default 0 (Studio Color) */

		if( ImageInfo->Frame_width <= RDMA_UVI_MAX_WIDTH )
			VIOC_RDMA_SetImageUVIEnable(pRDMABase0, true);
		else
			VIOC_RDMA_SetImageUVIEnable(pRDMABase0, false);
	}

	VIOC_RDMA_SetImageOffset(pRDMABase0, ImageInfo->fmt, ImageInfo->Frame_width);
	VIOC_RDMA_SetImageFormat(pRDMABase0, ImageInfo->fmt);

	VIOC_WMIX_SetPosition(pWMIXBase, 2, 0, 0);
	VIOC_WMIX_SetPosition(pWMIXBase, 3, 0, 0);

	VIOC_RDMA_SetImageSize(pRDMABase0, ImageInfo->Image_width, ImageInfo->Image_height);
	VIOC_RDMA_SetImageBase(pRDMABase0, ImageInfo->addr0, ImageInfo->addr1, ImageInfo->addr2);

	VIOC_RDMA_SetImageIntl(pRDMABase0, false);

	if(ImageInfo->fmt >= TCC_LCDC_IMG_FMT_444SEP && ImageInfo->fmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)
	{
		VIOC_RDMA_SetImageY2REnable(pRDMABase1, true);
		VIOC_RDMA_SetImageY2RMode(pRDMABase1, 0x2); /* Y2RMode Default 0 (Studio Color) */

		if( ImageInfo->Frame_width <= RDMA_UVI_MAX_WIDTH )
			VIOC_RDMA_SetImageUVIEnable(pRDMABase1, true);
		else
			VIOC_RDMA_SetImageUVIEnable(pRDMABase1, false);

	}

	VIOC_RDMA_SetImageOffset(pRDMABase1, ImageInfo->fmt, ImageInfo->Frame_width);
	VIOC_RDMA_SetImageFormat(pRDMABase1, ImageInfo->fmt);

	// position
	VIOC_WMIX_SetPosition(pWMIXBase, ImageInfo->Lcdc_layer, 0, ImageInfo->Frame_height + iVBlank);

	VIOC_RDMA_SetImageSize(pRDMABase1, ImageInfo->Image_width, ImageInfo->Image_height);
	VIOC_RDMA_SetImageBase(pRDMABase1, ImageInfo->MVC_Base_addr0, ImageInfo->MVC_Base_addr1, ImageInfo->MVC_Base_addr2);

	VIOC_RDMA_SetImageIntl(pRDMABase1, false);

	VIOC_WMIX_SetBGColor(pWMIXBase, 0x00, 0x00, 0x00, 0x00);

	VIOC_RDMA_SetImageEnable(pRDMABase0);
	VIOC_RDMA_SetImageEnable(pRDMABase1);

	VIOC_WMIX_SetUpdate(pWMIXBase);
}

void tca_fb_rdma_pandisplay(unsigned int layer, unsigned int base_addr, struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
	int scN = 0;
	unsigned int  fmt = 0;
	unsigned int lcd_width, lcd_height, img_width, img_height;

	unsigned int alpha_type = 0, alpha_blending_en = 0;
	unsigned int chromaR, chromaG, chromaB, chroma_en = 0;
	volatile void __iomem *pSC;

	volatile void __iomem *pRDMA = pdp_data->rdma_info[layer].virt_addr;
	volatile void __iomem *pWMIX = pdp_data->wmixer_info.virt_addr;

	if(var->bits_per_pixel == 32) 	{
		chroma_en = UI_CHROMA_EN;
		alpha_type = 1;
		alpha_blending_en = 1;
		fmt = TCC_LCDC_IMG_FMT_RGB888;
	}
	else if(var->bits_per_pixel == 16)	{
		chroma_en = 1;
		alpha_type = 0;
		alpha_blending_en = 0;
		fmt = TCC_LCDC_IMG_FMT_RGB565;
	}
	else	{
		pr_err("%s: bits_per_pixel : %d Not Supported BPP!\n", __FUNCTION__, var->bits_per_pixel);
	}

	chromaR = chromaG = chromaB = 0;

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	img_width = var->xres;
	img_height = var->yres;

	scN = VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[layer].blk_num);

	if(scN < 0)	{
		if(img_width > lcd_width)
			img_width = lcd_width;

		if(img_height > lcd_height)
			img_height = lcd_height;
	}
	else
	{
		pSC = VIOC_SC_GetAddress(scN);

		if((lcd_height == img_height) && (lcd_width == img_width))
			VIOC_SC_SetBypass (pSC, ON);
		else
			VIOC_SC_SetBypass (pSC, OFF);

		VIOC_SC_SetDstSize (pSC, lcd_width, lcd_height);			// set destination size in scaler
		VIOC_SC_SetOutSize (pSC, lcd_width, lcd_height);			// set output size in scaler
		VIOC_SC_SetUpdate (pSC);
	}


	// default framebuffer
	VIOC_WMIX_SetPosition(pWMIX, layer, 0, 0);
	//overlay setting
	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	VIOC_WMIX_SetChromaKey(pWMIX, layer, chroma_en, chromaR, chromaG, chromaB, 0x3FF, 0x3FF, 0x3FF);
	#else
	VIOC_WMIX_SetChromaKey(pWMIX, layer, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);
	#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	VIOC_WMIX_SetChromaKey(pWMIX, layer, chroma_en, chromaY, chromaU, chromaV, 0xF8, 0xFC, 0xF8);
	#endif
	#endif//


	VIOC_RDMA_SetImageFormat(pRDMA, fmt);					//fmt

	VIOC_RDMA_SetImageOffset(pRDMA, fmt, img_width);		//offset
	VIOC_RDMA_SetImageSize(pRDMA, img_width, img_height);	//size
	VIOC_RDMA_SetImageBase(pRDMA, base_addr, 0, 0);

	VIOC_RDMA_SetImageAlphaSelect(pRDMA, alpha_type);
	VIOC_RDMA_SetImageAlphaEnable(pRDMA, alpha_blending_en);

	VIOC_WMIX_SetUpdate(pWMIX);

#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
	VIOC_RDMA_SetImageY2REnable(pRDMA, 0);
	VIOC_RDMA_SetImageR2YEnable(pRDMA, 1);

	//	VIOC_RDMA_SetImageAlpha(pRDMA, 0x80, 0x80);
	//	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 0);
	//	VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
#endif

	VIOC_RDMA_SetImageEnable(pRDMA);

}

void tccfb1_set_par(struct tccfb_info *fbi,  struct fb_var_screeninfo *var)
{
	unsigned int lcd_width, lcd_height, rdmaN, en_rdma;
	 struct tcc_dp_device *pdp_data = &fbi->pdata.Mdp_data;

	VIOC_DISP_GetSize(pdp_data->ddc_info.virt_addr, &lcd_width, &lcd_height);

	if(fbi->fb->node == 0)
		rdmaN = RDMA_FB;
	else if(fbi->fb->node == 1)
		rdmaN = RDMA_FB1;
	else
		rdmaN = RDMA_FB;

	if((lcd_width != var->xres) || (lcd_height != var->yres))
	{
		//scaler plug in check
		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[rdmaN].blk_num) < 0)
		{
			VIOC_PlugInOutCheck VIOC_PlugIn;

			VIOC_CONFIG_Device_PlugState(VIOC_SCALER0, &VIOC_PlugIn);
			VIOC_RDMA_GetImageEnable(pdp_data->rdma_info[rdmaN].virt_addr, &en_rdma);

			if(!en_rdma && !VIOC_PlugIn.enable)	{
				pr_info("fb %d : display scaler 1 plug in \n", fbi->fb->node);
				VIOC_CONFIG_PlugIn(VIOC_SCALER0, pdp_data->rdma_info[rdmaN].blk_num);
			}
			else 	{
				pr_err("ERR fb %d : scaler 1 plug in error RDMA enable:%d  SCALER enable:%d \n", fbi->fb->node, en_rdma ,VIOC_PlugIn.enable);
			}
		}
	}

}

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
void tcc_video_rdma_off(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo, int outputMode, int interlaced)
{
	unsigned int iSCType = 0;

	iSCType = tca_get_scaler_num(pdp_data->DispDeviceType, ImageInfo->Lcdc_layer);

	VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[ImageInfo->Lcdc_layer].virt_addr);

	if(VIOC_CONFIG_DMAPath_Support()) {
		int component_num = VIOC_CONFIG_DMAPath_Select(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);

		#ifdef CONFIG_VIOC_MAP_DECOMP
		if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX)))
	 	{
			pr_info("tcc_video_rdma[%d]_off for last-frame :: mc_num %u plug out \n", ImageInfo->Lcdc_layer, get_vioc_index(component_num));
			tca_map_convter_onoff(component_num, 0, 1);
			VIOC_CONFIG_DMAPath_UnSet(component_num);
			tca_map_convter_swreset(component_num); //disable it to prevent system-hang!!
	 	}
		else
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX)))
	 	{
			pr_info("tcc_video_rdma[%d]_off for last-frame :: dtrc_num %u plug out \n", ImageInfo->Lcdc_layer, get_vioc_index(component_num));
			tca_dtrc_convter_onoff(component_num, 0, 1);
			VIOC_CONFIG_DMAPath_UnSet(component_num);
			//tca_dtrc_convter_swreset(component_num);	 //disable it to prevent system-hang!!
	 	}
		else
		#endif
		if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
			VIOC_CONFIG_DMAPath_UnSet(component_num);

		// It is default path selection(VRDMA)
		VIOC_CONFIG_DMAPath_Set(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
	} else {
		#ifdef CONFIG_ARCH_TCC803X
		if(ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0) {
			pr_info("tcc_video_rdma[%d]_off for last-frame :: Map convter plug out \n", ImageInfo->Lcdc_layer);
			tca_map_convter_onoff(VIOC_MC0 + nDeCompressor_Main, 0, 1);
			VIOC_CONFIG_MCPath(pdp_data->wmixer_info.blk_num, pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num);
		}
		#endif
	}

	if( !interlaced && (outputMode == TCC_OUTPUT_LCD || outputMode == TCC_OUTPUT_HDMI) )
	{
//		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_RESET);
//		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_CLEAR);

		if(VIOC_CONFIG_GetScaler_PluginToRDMA(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num) != -1) //scaler plug in status check
		{
			pr_info("tcc_video_rdma[%d]_off for last-frame :: scaler %u plug out \n", ImageInfo->Lcdc_layer, get_vioc_index(iSCType));
			VIOC_CONFIG_PlugOut(iSCType);
			VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(iSCType, VIOC_CONFIG_CLEAR);
		}
	}
#if defined(CONFIG_FB_TCC_COMPOSITE)
	else if(outputMode == TCC_OUTPUT_COMPOSITE){
//		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_RESET);
//		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_CLEAR);

		#if defined(CONFIG_ANDROID) || defined(CONFIG_PLATFORM_AVN)
		tcc_plugout_for_composite(RDMA_VIDEO);
		#else
		tcc_plugout_for_composite(ImageInfo->Lcdc_layer);//linux stb
		#endif//
	}
#endif
#if defined(CONFIG_FB_TCC_COMPONENT)
	else if(outputMode == TCC_OUTPUT_COMPONENT){
//		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_RESET);
//		VIOC_CONFIG_SWReset(pdp_data->rdma_info[ImageInfo->Lcdc_layer].blk_num, VIOC_CONFIG_CLEAR);

		#if defined(CONFIG_ANDROID) || defined(CONFIG_PLATFORM_AVN)
		tcc_plugout_for_component(RDMA_VIDEO);
		#else
		tcc_plugout_for_component(ImageInfo->Lcdc_layer); //linux stb
		#endif//
	}
#endif
}
#endif

void tca_fb_vsync_activate(struct tcc_dp_device *pdata)
{
	#ifdef CONFIG_FB_TCC_USE_VSYNC_INTERRUPT
	VIOC_DISP_SetStatus(pdata->ddc_info.virt_addr, 0x3F);
	pdata->pandisp_sync.state = 0;

//	_tca_vioc_intr_onoff(ON, pdata->ddc_info.irq_num, pdata->DispNum);

//	VIOC_DISP_SetStatus(fbi->pdata.Mdp_data.ddc_info.virt_addr, 0x3F);
//	fbi->pdata.Mdp_data.pandisp_sync.state = 0;
//	tca_lcdc_interrupt_onoff(true, fbi->pdata.lcdc_number);
	#endif//
}
EXPORT_SYMBOL(tca_fb_vsync_activate);

void tca_fb_wait_for_vsync(struct tcc_dp_device *pdata)
{
	#ifdef CONFIG_FB_TCC_USE_VSYNC_INTERRUPT
	int ret = 1;

	tca_fb_vsync_activate(pdata);
	if(pdata->pandisp_sync.state == 0)
		ret = wait_event_interruptible_timeout(pdata->pandisp_sync.waitq, pdata->pandisp_sync.state == 1, msecs_to_jiffies(50));

	if(!ret)	{
	 	printk("  [%s %d]: tcc_setup_interrupt timed_out!! \n",__func__, ret);
	}
	#endif //CONFIG_FB_TCC_USE_VSYNC_INTERRUPT
}
EXPORT_SYMBOL(tca_fb_wait_for_vsync);



/* tccfb_activate_var
 * activate (set) the controller from the given framebuffer
 * information
*/
void tca_fb_activate_var(unsigned int dma_addr,  struct fb_var_screeninfo *var, struct tcc_dp_device *pdp_data)
{
#ifdef CONFIG_VIOC_DOLBY_VISION_EDR
	if(pdp_data->DispDeviceType == TCC_OUTPUT_HDMI){
		if( (!VIOC_CONFIG_DV_GET_EDR_PATH() && vioc_v_dv_get_stage() != DV_OFF)
			|| (VIOC_CONFIG_DV_GET_EDR_PATH() && vioc_v_dv_get_stage() != DV_RUN)
		)
		{
			return;
		}
	}
#endif

	switch(pdp_data->FbUpdateType)
	{
		case FB_RDMA_UPDATE:
			tca_fb_rdma_active_var(dma_addr, var, pdp_data);
			break;

		case FB_SC_RDMA_UPDATE:
			tca_fb_sc_rdma_active_var(dma_addr, var, pdp_data);
			break;

		case FB_SC_M2M_RDMA_UPDATE:
			tca_fb_sc_m2m_rdma_active_var(dma_addr, var, pdp_data);
			break;

		case FB_SC_G2D_RDMA_UPDATE:
			tca_fb_sc_g2d_rdma_active_var(dma_addr, var, pdp_data);
			break;

		case FB_DIVIDE_UPDATE:
			tca_fb_divide_active_var(dma_addr, var, pdp_data);
			break;

		case FB_MVC_UPDATE:
			tca_fb_mvc_active_var(dma_addr, var, pdp_data);
			break;

		default:
			break;
	}

	pdp_data->FbBaseAddr = dma_addr;

#if 0//def CONFIG_VIOC_DOLBY_VISION_EDR
	if( VIOC_CONFIG_DV_GET_EDR_PATH() && pdp_data->DispDeviceType == TCC_OUTPUT_HDMI && bUse_GAlpha){
		VIOC_RDMA_SetImageAlpha(pdp_data->rdma_info[RDMA_FB].virt_addr, 0x1ff, 0x1ff);
		VIOC_RDMA_SetImageAlphaSelect(pdp_data->rdma_info[RDMA_FB].virt_addr, 0);
		VIOC_RDMA_SetImageEnable(pdp_data->rdma_info[RDMA_FB].virt_addr);
		//pr_info("Global Alpha for Logo \n");
	}
#endif

	if(pdp_data->FbPowerState){
		VIOC_DISP_TurnOn(pdp_data->ddc_info.virt_addr);
	}

	#if defined(CONFIG_TCC_DISPLAY_MODE_USE) && !defined(CONFIG_TCC_DISPLAY_MODE_AUTO_DETECT)|| defined(CONFIG_TCC_DISPLAY_LCD_CVBS)
		tca_fb_attach_update_flag();
	#endif
}
EXPORT_SYMBOL(tca_fb_activate_var);


/* tcafb_activate_var
 *
 * activate (set) the controller from the given framebuffer
 * information
*/
void tcafb_activate_var(struct tccfb_info *fbi,  struct fb_var_screeninfo *var)
{
	unsigned int BaseAddr = 0;

#if defined(CONFIG_VIOC_AFBCDEC)
	if((system_rev!=0) && (var->reserved[3]!=0))
		BaseAddr = fbi->map_dma + var->xres * var->yoffset * (var->bits_per_pixel/8) + var->reserved[1];
	else
#endif
		BaseAddr = fbi->map_dma + var->xres * var->yoffset * (var->bits_per_pixel/8);

	if(fbi->pdata.Mdp_data.FbPowerState)
		tca_fb_activate_var(BaseAddr, var, &fbi->pdata.Mdp_data);

	#if !defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_STB)
		#if !defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_MID) && !defined(CONFIG_TCC_DISPLAY_MODE_USE)
		if(fbi->pdata.Sdp_data.FbPowerState)
			tca_fb_activate_var(BaseAddr, var, &fbi->pdata.Sdp_data);
		#endif
	#endif
}


/* tccfb_pan_display
 *
 * pandisplay (set) the controller from the given framebuffer
 * information
*/
int tca_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct tccfb_info *fbi = info->par;
	int ret = 0;

	/* activate this new configuration */
   	if(info->node == 0)	{

		if((fbi->pdata.Mdp_data.FbPowerState)
			#if defined(CONFIG_USE_DISPLAY_FB_LOCK)
			&& (!fb_lock)
			#endif
		)
		{

			tcafb_activate_var(fbi, var);
			tca_fb_vsync_activate(&fbi->pdata.Mdp_data);

			if(var->activate & FB_ACTIVATE_VBL)
			{
				#ifdef CONFIG_FB_TCC_USE_VSYNC_INTERRUPT
				if(fbi->pdata.Mdp_data.pandisp_sync.state == 0)
					ret = wait_event_interruptible_timeout(fbi->pdata.Mdp_data.pandisp_sync.waitq, fbi->pdata.Mdp_data.pandisp_sync.state == 1, msecs_to_jiffies(50));

				if(!ret)	{
					printk("  [%s %d]: tcc_setup_interrupt timed_out!! \n",__func__, ret);
				}
				#endif //CONFIG_FB_TCC_USE_VSYNC_INTERRUPT
			}
		}
	}
	else
	{
		unsigned int layer, base_addr;

		base_addr = fbi->map_dma + var->xres * var->yoffset * (var->bits_per_pixel/8);

		if(info->node == 1)
			layer = RDMA_FB1;
		else
			return -1;

		tca_fb_rdma_pandisplay(RDMA_FB1,base_addr, var, &fbi->pdata.Mdp_data);
	}
	return ret;
}
EXPORT_SYMBOL(tca_fb_pan_display);



/* suspend and resume support for the lcd controller */
int tca_fb_suspend(struct device *dev,
        struct lcd_panel *disp_panel, struct lcd_panel *ext_panel)
{
	struct platform_device *fb_device = container_of(dev, struct platform_device, dev);
	struct tccfb_info	   *info = platform_get_drvdata(fb_device);
	struct tcc_dp_device *pdp_data = &info->pdata.Mdp_data;
	int ret = 0;

	pr_info("fb%d: %s\n", info->fb->node, __func__);

	#if defined(__CONFIG_HIBERNATION)
	if(do_hibernation)
		return 0;
	#endif//

        #if defined(CONFIG_TCC_HDMI_DRIVER_V1_4) || defined(CONFIG_TCC_HDMI_DRIVER_V2_0)
        if(info != NULL) {
                if(info->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_HDMI ||
                        info->pdata.Sdp_data.DispDeviceType == TCC_OUTPUT_HDMI) {
                        pr_info("%s disable hdmi \r\n", __func__);
                        /* Disable HDMI Output */
                        hdmi_stop();
                        /* Change Lx LCKL Source */
                        tca_vioc_displayblock_clock_select(pdp_data, 0);
                }
        }
        #endif

	info->pdata.Mdp_data.FbPowerState = 0;

	if (disp_panel->set_power)
		disp_panel->set_power(disp_panel, 0, &info->pdata.Mdp_data);

        /* disable extended panel */
        if(ext_panel != NULL && ext_panel->set_power != NULL) {
                ext_panel->set_power(ext_panel, 0, NULL);
        }

	#ifdef CONFIG_ANDROID
	VIOC_RDMA_SetImageDisable(pdp_data->rdma_info[RDMA_FB].virt_addr);
	#else
	VIOC_RDMA_SetImageDisableNW(pdp_data->rdma_info[RDMA_FB].virt_addr);
	VIOC_DISP_TurnOff(pdp_data->ddc_info.virt_addr);
	#endif//

	#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
	/* Only fb0 has disp_dd_sync.waitq */
	pdp_data->disp_dd_sync.state = 0;
	if (info->fb->node == 0) {
		ret = wait_event_interruptible_timeout(pdp_data->disp_dd_sync.waitq, pdp_data->disp_dd_sync.state == 1, msecs_to_jiffies(30));
		if(ret == 0)
			pr_info("%s: timeout DD\n", __func__);
	} else {
		VIOC_DISP_sleep_DisplayDone(pdp_data->ddc_info.virt_addr);
	}
	#else
	VIOC_DISP_sleep_DisplayDone(pdp_data->ddc_info.virt_addr);
	#endif

	_tca_vioc_intr_onoff(OFF, pdp_data->ddc_info.irq_num, info->pdata.lcdc_number);

	clk_disable_unprepare(pdp_data->vioc_clock);
	clk_disable_unprepare(pdp_data->ddc_clock);

	return ret;
}
EXPORT_SYMBOL(tca_fb_suspend);


int tca_fb_resume(struct device *dev,
        struct lcd_panel *disp_panel, struct lcd_panel *ext_panel)
{
	struct platform_device *fb_device = container_of(dev, struct platform_device, dev);
	struct tccfb_info	   *fbi = platform_get_drvdata(fb_device);
	struct tcc_dp_device *pdp_data = &fbi->pdata.Mdp_data;

	pr_info("FB: %s: \n", __FUNCTION__);

	#if defined(__CONFIG_HIBERNATION)
	if(do_hibernation)
		return 0;
	#endif//

	clk_prepare_enable(pdp_data->vioc_clock);
	clk_prepare_enable(pdp_data->ddc_clock);

        if (disp_panel->set_power)
	        disp_panel->set_power(disp_panel, 1, &fbi->pdata.Mdp_data);

        /* resume extended panel */
        if(ext_panel != NULL && ext_panel->set_power != NULL) {
                ext_panel->set_power(ext_panel, 1, NULL);
        }

        if(fbi->pdata.Mdp_data.DispDeviceType == TCC_OUTPUT_LCD)
                fbi->pdata.Mdp_data.FbPowerState = 1;

	_tca_vioc_intr_onoff(ON, pdp_data->ddc_info.irq_num, fbi->pdata.lcdc_number);

	tca_fb_vsync_activate(pdp_data);

	return 0;
}
EXPORT_SYMBOL(tca_fb_resume);




void tca_vsync_enable(struct tccfb_info *dev, int on)
{
	dprintk("## VSYNC(%d)", on);

	dev->active_vsync = on;
}
EXPORT_SYMBOL(tca_vsync_enable);

int tca_fb_init(struct tccfb_info *fbi)
{
	printk(KERN_INFO " %s (built)\n", __func__);
	tca_fb_mem_scale_init();

	ViocScaler_np = of_find_compatible_node(NULL, NULL, "telechips,scaler");
	if(ViocScaler_np == NULL)
		pr_err("cann't find scaler\n");

	ViocConfig_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_config");
	if(ViocConfig_np == NULL)
		pr_err("cann't find vioc config \n");

	clk_prepare_enable(fbi->pdata.Mdp_data.vioc_clock);
	clk_prepare_enable(fbi->pdata.Mdp_data.ddc_clock);

#ifdef CONFIG_DIRECT_MOUSE_CTRL
	//initialize the buffers for mouse cursor
	{
		dma_addr_t	Gmap_dma;	/* physical */
		u_char *	Gmap_cpu;	/* virtual */

		Gmap_cpu = dma_alloc_writecombine(0, MOUSE_CURSOR_BUFF_SIZE, &Gmap_dma, GFP_KERNEL);
		memset(Gmap_cpu, 0x00, MOUSE_CURSOR_BUFF_SIZE);

		mouse_data.pbuf1 = (char *)Gmap_dma;
		mouse_data.pbuf2 = (char *)(Gmap_dma + MOUSE_CURSOR_BUFF_SIZE/2);
		mouse_data.vbuf1 = (char *)Gmap_cpu;
		mouse_data.vbuf2 = (char *)(Gmap_cpu + MOUSE_CURSOR_BUFF_SIZE/2);
		mouse_data.index = 0;
	}
#endif

	memset((void *)&resize_data, 0x00, sizeof(tcc_display_resize));

	attach_intr.vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (!attach_intr.vioc_intr)
		return -ENOMEM;

#if defined(CONFIG_PLATFORM_STB) && !defined(CONFIG_ANDROID)
        {
			// It will be deprecat.
			//volatile void __iomem *pDISP = fbi->pdata.Mdp_data.ddc_info.virt_addr;
			//volatile void __iomem *pRDMA = fbi->pdata.Mdp_data.rdma_info[RDMA_FB].virt_addr;
			//VIOC_DISP_TurnOff(pDISP);
			//VIOC_RDMA_SetImageDisable(pRDMA);
		}
#endif

#ifdef CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST
	{
		int i = 0;
		for(i = 0; i < 150; i++){
			nAccumulated_ImageInfo[i].private_data.optional_info[VID_OPT_BUFFER_ID] = 0xffff;
		}
	}
#endif

	return 0;
}
EXPORT_SYMBOL(tca_fb_init);

void tca_fb_exit(void)
{
	pr_info(" %s \n", __func__);

#if defined(__CONFIG_HIBERNATION) && defined(CONFIG_USING_LAST_FRAMEBUFFER)
	fb_quickboot_lastframe_display_release();
#endif
}
EXPORT_SYMBOL(tca_fb_exit);
