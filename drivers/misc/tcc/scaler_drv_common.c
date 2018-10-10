/****************************************************************************
 * Copyright (C) 2015 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_types.h>

#include <video/tcc/tcc_scaler_common_ioctl.h>
#include <video/tcc/tccfb_ioctrl.h>

#include <video/tcc/tcc_scaler_ioctrl.h>
//#include <video/tcc/vioc_api.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut.h>
#include <video/tcc/vioc_mc.h>
#include <video/tcc/vioc_config.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/TCC_HEVCDEC.h>
#else
#include <video/tcc/tcc_video_private.h>
#endif
#include <video/tcc/tca_map_converter.h>
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif

#if defined(CONFIG_VIOC_AFBCDEC)
#include <video/tcc/vioc_afbcdec.h>
#endif
#if defined(CONFIG_VIOC_SAR)
#include <video/tcc/vioc_sar.h>
extern int CheckSarPathSelection(unsigned int component);
#endif//
#if defined(CONFIG_VIOC_2D_FILTER)
#include <video/tcc/vioc_filter2D.h>
#endif//

//#define  DUMP_REG

static int debug	   = 0;
#define dprintk(msg...)	if(debug) { 	printk( "\x1b[1;38m scaler_common:  " msg); printk(" \x1b[0m ");	}

static int pldebug	   = 0;
#define plug_debug(msg...)	if(pldebug) { printk("\x1b[1;32m scaler_common:  " msg); printk(" \x1b[0m "); }

#define TCC_PA_VIOC_CFGINT	(HwVIOC_BASE + 0xA000)

struct scaler_data {
	// wait for poll
	wait_queue_head_t	poll_wq;
	spinlock_t		poll_lock;
	unsigned int		poll_count;

	// wait for ioctl command
	wait_queue_head_t	cmd_wq;
	spinlock_t		cmd_lock;
	unsigned int		cmd_count;

	struct mutex		io_mutex;
	unsigned char		block_operating;
	unsigned char		block_waiting;
	unsigned char		irq_reged;
	unsigned int		dev_opened;
};

struct vioc_block_res_type {
	volatile void __iomem *reg;
	unsigned int id;
	unsigned int path; 	//component position   0 : before mixer  , 1 : after mixer
	unsigned int mixer_bypass;
};

struct vioc_filter2d_block_res_type {
	volatile void __iomem *reg;
	unsigned int id;
	//component position  berfore Mixer ( 0 : before scaler,  1 : after scaler) after Mixer( 2 : before scaler,  3 : after scaler)
	unsigned int path;
	unsigned int level;
};

struct vioc_sar_block_res_type {
	volatile void __iomem *reg;
	unsigned int id;
	unsigned int level;
};


struct scaler_common_drv_type {
	struct vioc_intr_type	*vioc_intr;

	unsigned int		irq;
	struct miscdevice	*misc;

	struct vioc_block_res_type	rdma;
 	struct vioc_block_res_type	wmix;
	struct vioc_block_res_type	sc;
	struct vioc_block_res_type	wdma;
#if defined(CONFIG_VIOC_AFBCDEC)
	struct vioc_block_res_type	afbc_dec;
#endif

#if defined(CONFIG_VIOC_2D_FILTER)
	struct vioc_filter2d_block_res_type	filter2d;
#endif

#if defined(CONFIG_VIOC_SAR)
	struct vioc_sar_block_res_type	sar;
#endif
	unsigned int force_fmt_RGB; //wmixer path formt : RGB
	unsigned int force_fmt_YUV; //wmixer path formt : YUV
	int pixel_mapper_n;


	struct clk		*clk;
	struct scaler_data	*data;
	SCALER_TYPE		*info;

};

extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy,
								unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);
extern int range_is_allowed(unsigned long pfn, unsigned long size);

static int scaler_drv_common_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		printk(KERN_ERR	 "%s():  This address is not allowed. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		printk(KERN_ERR	 "%s():  Virtual address page port error. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_ops	= NULL;
	vma->vm_flags 	|= VM_IO;
	vma->vm_flags 	|= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static int tcc_scaler_drv_common_register_interrupt(struct scaler_common_drv_type *scaler, unsigned int bOn)
{
	dprintk("tcc_scaler_drv_common_register_interrupt INTR ID:%d bit :%d  \n ", scaler->vioc_intr->id, scaler->vioc_intr->bits);
	synchronize_irq(scaler->irq);
	vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);

	if(bOn)
		vioc_intr_enable(scaler->irq, scaler->vioc_intr->id, scaler->vioc_intr->bits);

	return 0;
}

int tcc_scaler_drv_common_set_path_resource(struct scaler_common_drv_type *scaler, struct tc_scaler_path_set *vioc_resorce)
{
	if(scaler->sc.id >= VIOC_SCALER){
		tcc_scaler_drv_common_register_interrupt(scaler, 0);
		if(scaler->rdma.reg){
			VIOC_RDMA_SetImageDisable(scaler->rdma.reg);
	#if defined(CONFIG_VIOC_AFBCDEC)
			if(scaler->afbc_dec.reg) {
				VIOC_AFBCDec_TurnOFF(scaler->afbc_dec.reg);
				VIOC_CONFIG_AFBCDECPath(scaler->afbc_dec.id, scaler->rdma.id, 0);
				scaler->afbc_dec.reg = NULL;
			}
	#endif
			VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		}
		if(scaler->sc.mixer_bypass)
			VIOC_CONFIG_WMIXPath(scaler->rdma.id, 1);
		VIOC_CONFIG_PlugOut(scaler->sc.id);
		printk("\x1b[1;38m SC(0x%x) Plugged-Out  \x1b[0m \n", scaler->sc.id);
	}
	else{
		printk("\x1b[1;38m SC(0x%x) No Plugged-Out  \x1b[0m \n", scaler->sc.id);
	}
	
	scaler->rdma.id = VIOC_RDMA + vioc_resorce->rdma_num;
	scaler->rdma.reg = 	VIOC_RDMA_GetAddress(scaler->rdma.id);

#if defined(CONFIG_VIOC_AFBCDEC)
	if(vioc_resorce->afbc_dec_need){
		scaler->afbc_dec.id = VIOC_AFBCDEC0 + vioc_resorce->afbc_dec_num;
		scaler->afbc_dec.reg = VIOC_AFBCDec_GetAddress(scaler->afbc_dec.id);
		VIOC_CONFIG_AFBCDECPath(scaler->afbc_dec.id, scaler->rdma.id, 1);
	}
	else {
		scaler->afbc_dec.reg = NULL;
	}
#endif

#ifdef CONFIG_VIOC_PIXEL_MAPPER
	if((scaler->pixel_mapper_n >= VIOC_PIXELMAP) && (scaler->pixel_mapper_n < (VIOC_PIXELMAP +VIOC_PIXELMAP_MAX)))
		VIOC_CONFIG_PlugOut(scaler->pixel_mapper_n);

	scaler->pixel_mapper_n = -1;

	if(vioc_resorce->pixel_mapper_n >= 0)
		scaler->pixel_mapper_n = VIOC_PIXELMAP + vioc_resorce->pixel_mapper_n;
#endif

#if defined(CONFIG_VIOC_SAR)
	if(vioc_resorce->sar_need ==  1)
	{
		scaler->sar.reg = VIOC_SAR_GetAddress(SAR);
		scaler->sar.id = VIOC_SAR0;
		scaler->sar.level = vioc_resorce->sar_level;
	}	
#endif//

	if(vioc_resorce->wdma_num < 5)
		scaler->wmix.id = VIOC_WMIX + vioc_resorce->wdma_num;
	else if(vioc_resorce->wdma_num >= 5)
		scaler->wmix.id = VIOC_WMIX + 5 + (vioc_resorce->wdma_num - 5)/2;
	
	scaler->wmix.reg = 	VIOC_WMIX_GetAddress(scaler->wmix.id);

	scaler->wdma.id = VIOC_WDMA + vioc_resorce->wdma_num;
	scaler->wdma.reg = 	VIOC_WDMA_GetAddress(scaler->wdma.id);

	scaler->sc.id = VIOC_SCALER + vioc_resorce->scaler_num;
	scaler->sc.reg = VIOC_SC_GetAddress(scaler->sc.id);
	scaler->sc.path = vioc_resorce->scaler_pos;
	scaler->sc.mixer_bypass = vioc_resorce->mixer_bypass;

	scaler->vioc_intr->id = VIOC_INTR_WD0 + get_vioc_index(scaler->wdma.id);
	scaler->vioc_intr->bits = (1<<VIOC_WDMA_INTR_EOFR);

	VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);



	printk("\x1b[1;38m Comp_Num:: rdma:0x%x wmixer:0x%x wdma:0x%x sc:0x%x sc_path:%s mixer_bypass(%d) \n ",
			scaler->rdma.id, scaler->wmix.id, scaler->wdma.id, scaler->sc.id, scaler->sc.path == 0 ? "front" : "rear", scaler->sc.mixer_bypass);
	dprintk("\x1b[1;38m Register:: rdma:0x%p wmixer:0x%p wdma:0x%p sc:0x%p \n ", scaler->rdma.reg, scaler->wmix.reg, scaler->wdma.reg, scaler->sc.reg);
	printk("INTR ID:%d bit :%d \x1b[0m  \n ", scaler->vioc_intr->id, scaler->vioc_intr->bits);
	
	return 0;
}



int tcc_scaler_drv_common_set_filter2d(struct scaler_common_drv_type *scaler, struct tc_scaler_filter2D_set *filter_resorce)
{
#if defined(CONFIG_VIOC_2D_FILTER)
	if(filter_resorce->filter2d_need){
		plug_debug("%s en : %d num:%d  pos:%d  level:%d\n", 
			__func__, filter_resorce->filter2d_need, filter_resorce->filter2d_num, filter_resorce->filter2d_pos, filter_resorce->filter2d_level);
		scaler->filter2d.id = VIOC_F2D + filter_resorce->filter2d_num;
		scaler->filter2d.reg = VIOC_Filter2D_GetAddress(scaler->filter2d.id);
		scaler->filter2d.path = filter_resorce->filter2d_pos;
		scaler->filter2d.level = filter_resorce->filter2d_level;
	}
	else {
		scaler->filter2d.id = VIOC_F2D + filter_resorce->filter2d_num;
		VIOC_CONFIG_PlugOut(scaler->filter2d.id);
		filt2d_enable(scaler->filter2d.id, 0);
		scaler->filter2d.reg = NULL;
	}
#endif

	return 0;
}


static int tcc_scaler_drv_common_set_path_selection(struct scaler_common_drv_type *scaler)
{
	//VRDMA , GRDMA plug in to path.
	if(VIOC_CONFIG_DMAPath_Support())
	{
		unsigned int component_num = 0x00;

		if(scaler->rdma.id == VIOC_RDMA02 || scaler->rdma.id == VIOC_RDMA03 || scaler->rdma.id == VIOC_RDMA07 || scaler->rdma.id >= VIOC_RDMA11)
			component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
		else
			return 0;

		if(component_num < 0)
			pr_info(" %s  : RDMA :0x%x dma path selection none\n", __func__, scaler->rdma.id);
		else if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX))){
				VIOC_CONFIG_DMAPath_UnSet(component_num);
		}

		// It is default path selection(VRDMA)

		VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
	}
	return 0;
}
	
#if defined(CONFIG_VIOC_AFBCDEC)
static void tcc_scaler_drv_configure_AFBCDEC(volatile void __iomem *pAFBC_Dec, unsigned int base_addr,
										volatile void __iomem *pRDMA,
										unsigned int bSplitMode, unsigned int bWideMode,
										unsigned int fmt, unsigned int width, unsigned int height)
{
	if(!pAFBC_Dec)
		return;

	VIOC_AFBCDec_SurfaceCfg(pAFBC_Dec, base_addr, fmt, width, height, 0, bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0);
	VIOC_AFBCDec_SetContiDecEnable(pAFBC_Dec, 0);
	VIOC_AFBCDec_SetSurfaceN(pAFBC_Dec, VIOC_AFBCDEC_SURFACE_0, 1);
	VIOC_AFBCDec_SetIrqMask(pAFBC_Dec, 0, AFBCDEC_IRQ_ALL); //disable all
	VIOC_AFBCDec_TurnOn(pAFBC_Dec, VIOC_AFBCDEC_SWAP_DIRECT);
}
#endif
static int tcc_component_plug_in_set(struct scaler_common_drv_type *scaler)
{
	int support_plug, ret = 0;
	unsigned int strength = 0;
	
	#ifdef CONFIG_VIOC_PIXEL_MAPPER
	if(scaler->pixel_mapper_n > 0) {
		support_plug = CheckPixelMapPathSelection(scaler->rdma.id);
		pr_info("pixel mapper 0x%x  plug in to RDMA :0x%x \n", scaler->pixel_mapper_n, scaler->rdma.id);

		if( support_plug >= 0 )
			ret = VIOC_CONFIG_PlugIn((unsigned int)scaler->pixel_mapper_n, scaler->rdma.id);
	}
	#endif//
	
	#ifdef CONFIG_VIOC_2D_FILTER
	if(scaler->filter2d.reg != NULL)
	{
		if(scaler->filter2d.path <= 1) 		{
			plug_debug("2d filter  RDMA:0x%x path:%d \n", scaler->rdma.id, scaler->filter2d.path);
		}
		else {
			plug_debug("2d filter  WDMA:0x%x path:%d \n", scaler->wdma.id, scaler->filter2d.path);
		}
		if(scaler->filter2d.path == 0)
			ret = VIOC_CONFIG_FILTER2D_PlugIn(scaler->filter2d.id, scaler->rdma.id, 0);
		else if(scaler->filter2d.path == 1)
			ret = VIOC_CONFIG_FILTER2D_PlugIn(scaler->filter2d.id, scaler->rdma.id, 1);
		else if(scaler->filter2d.path == 2)
			ret = VIOC_CONFIG_FILTER2D_PlugIn(scaler->filter2d.id, scaler->wdma.id, 0);
		else
			ret = VIOC_CONFIG_FILTER2D_PlugIn(scaler->filter2d.id, scaler->wdma.id, 1);

		if(ret >= 0)
		{
			if(scaler->filter2d.level <= STRENGTH_0)
			{
				strength = scaler->filter2d.level;
				filt2d_mode(scaler->filter2d.id, 1, 1, 1, 0, 0, 0, 0, 0, 0);
				filt2d_coeff_hpf(scaler->filter2d.id, strength, strength, strength);
			}
			else if(scaler->filter2d.level <= (STRENGTH_0 *2))
			{
				strength = scaler->filter2d.level - STRENGTH_0;
				filt2d_mode(scaler->filter2d.id, 1, 1, 1, 0, 0, 0, 0, 0, 0);
				filt2d_coeff_lpf(scaler->filter2d.id, strength, strength, strength);

			}
			else
			{
				strength = (scaler->filter2d.level % (SMODE_02 + 1));
				filt2d_coeff_set(scaler->filter2d.id, strength);
				filt2d_mode(scaler->filter2d.id, 0, 0, 0, 0, 0, 0, 1, 1, 1);
			}
			filt2d_enable(scaler->filter2d.id, 1);

			scaler->force_fmt_RGB = 1;
		}
	}
	#endif//
	
	#ifdef CONFIG_VIOC_SAR
	if(scaler->sar.reg != NULL)
	{
		support_plug = CheckPixelMapPathSelection(scaler->rdma.id);
		plug_debug("pixel sar %x  plug in to RDMA :%x \n", scaler->pixel_mapper_n, scaler->rdma.id);

		if( support_plug >= 0 )
			ret = VIOC_CONFIG_PlugIn((unsigned int)scaler->pixel_mapper_n, scaler->rdma.id);
		
	}	
	#endif//
	if(ret < 0) 
		pr_err("%s :  dma: %x  pixel:%x ", __func__, scaler->rdma.id, scaler->pixel_mapper_n);

	return ret;
}

static char tcc_scaler_drv_common_run(struct scaler_common_drv_type *scaler)
{
	int ret = 0;
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int crop_width;
	unsigned int component_num = 0;
	unsigned int bypass_mixer;
	volatile void __iomem *pSC_RDMABase = scaler->rdma.reg;
	volatile void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	volatile void __iomem *pSC_WDMABase = scaler->wdma.reg;
	volatile void __iomem *pSC_SCALERBase = scaler->sc.reg;

	scaler->force_fmt_RGB = 0;
	scaler->force_fmt_YUV = 0;
// vioc component plut in setting without scaler.
	tcc_component_plug_in_set(scaler);
	
	spin_lock_irq(&(scaler->data->cmd_lock));

#if 0//defined(CONFIG_VIOC_AFBCDEC) //test
	if(scaler->afbc_dec.reg != NULL ) {
		unsigned int w, h;
		w = 4096;//256;
		h = 2160;//256;
		
		scaler->info->src_Yaddr = 0x7d000000;
		scaler->info->src_Uaddr = 0x000000;
		scaler->info->src_Vaddr = 0x000000;
		scaler->info->src_fmt = TCC_LCDC_IMG_FMT_RGB888;
		scaler->info->src_ImgWidth = w;
		scaler->info->src_ImgHeight = h;
		scaler->info->src_winLeft = scaler->info->src_winTop = 0;
		scaler->info->src_winRight = w;
		scaler->info->src_winBottom = h;
		scaler->info->src_bit_depth = 8;

		//output dump test
		scaler->info->dest_Yaddr = 0x42300000;
		scaler->info->dest_Uaddr = 0x000000;
		scaler->info->dest_Vaddr = 0x000000;
		scaler->info->dest_fmt = TCC_LCDC_IMG_FMT_RGB888;
		scaler->info->dest_ImgWidth = w;
		scaler->info->dest_ImgHeight = h;
		scaler->info->dest_winLeft = scaler->info->src_winTop = 0;
		scaler->info->dest_winRight = w;
		scaler->info->dest_winBottom = h;
		scaler->info->dest_bit_depth = 8;
		VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
	}
#endif

	dprintk("src   add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d )%d %d %d %d    interlace: %d  \n", 
		scaler->info->src_Yaddr, scaler->info->src_Uaddr, scaler->info->src_Vaddr, scaler->info->src_fmt, 
		scaler->info->src_ImgWidth, scaler->info->src_ImgHeight, 
		scaler->info->src_winLeft, scaler->info->src_winTop, scaler->info->src_winRight, scaler->info->src_winBottom, scaler->info->interlaced);
	dprintk("  dest  add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d )%d %d %d %d \n", 
		scaler->info->dest_Yaddr, scaler->info->dest_Uaddr, scaler->info->dest_Vaddr, scaler->info->dest_fmt, 
		scaler->info->dest_ImgWidth, scaler->info->dest_ImgHeight, 
		scaler->info->dest_winLeft, scaler->info->dest_winTop, scaler->info->dest_winRight, scaler->info->dest_winBottom);


	crop_width = scaler->info->src_winRight - scaler->info->src_winLeft;
	#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
	if (VIOC_CONFIG_DMAPath_Support()) {
		if(scaler->rdma.id == VIOC_RDMA02 || scaler->rdma.id == VIOC_RDMA03 || scaler->rdma.id == VIOC_RDMA07 || scaler->rdma.id >= VIOC_RDMA11)
			component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
	}
	#endif

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if(scaler->info->mapConv_info.m_CompressedY[0] != 0)
	{
		int y2r = 0;
		dprintk("scaler %d path-id(rdma:%d, sc:%d) \n", scaler->sc.id, scaler->rdma.id, scaler->sc.path);

		dprintk("scaler %d src  map converter: size: %d %d pos:%d %d\n", scaler->sc.id, scaler->info->src_winRight - scaler->info->src_winLeft, 
				scaler->info->src_winBottom - scaler->info->src_winTop,
				scaler->info->src_winLeft, scaler->info->src_winTop);

		if (VIOC_CONFIG_DMAPath_Support()) {
			if((component_num < VIOC_MC0 ) || (component_num > (VIOC_MC0 + VIOC_MC_MAX))){
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(VIOC_MC1);  //disable it to prevent system-hang!!
			}
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, VIOC_MC1);
		}

		if( scaler->info->dest_fmt < TCC_LCDC_IMG_FMT_COMP )
			y2r = 1;

		// scaler limitation 
		tca_map_convter_driver_set(VIOC_MC1, scaler->info->src_ImgWidth, scaler->info->src_ImgHeight,
				scaler->info->src_winLeft, scaler->info->src_winTop, scaler->info->src_winRight - scaler->info->src_winLeft, scaler->info->src_winBottom - scaler->info->src_winTop,
				y2r, &scaler->info->mapConv_info);
		tca_map_convter_onoff(VIOC_MC1, 1, 0);
	}
	else
	#endif//
	#ifdef CONFIG_VIOC_DTRC_DECOMP
	if(scaler->info->dtrcConv_info.m_CompressedY[0] != 0)
	{
		int y2r = 0;

		dprintk("scaler %d src  dtrc converter: size: %d %d pos:%d %d\n", scaler->sc.id, scaler->info->src_winRight - scaler->info->src_winLeft, 
				scaler->info->src_winBottom - scaler->info->src_winTop,
				scaler->info->src_winLeft, scaler->info->src_winTop);

		if (VIOC_CONFIG_DMAPath_Support()) {
			if((component_num < VIOC_DTRC0 ) || (component_num > (VIOC_DTRC0 + VIOC_DTRC_MAX))){
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				//tca_dtrc_convter_swreset(VIOC_DTRC1); //disable it to prevent system-hang!!
			}
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, VIOC_DTRC1);
		}

		if( scaler->info->dest_fmt < TCC_LCDC_IMG_FMT_COMP )
			y2r = 1;

		// scaler limitation 
		tca_dtrc_convter_driver_set(VIOC_DTRC1, scaler->info->src_winRight - scaler->info->src_winLeft, scaler->info->src_winBottom - scaler->info->src_winTop, 
				scaler->info->src_winLeft, scaler->info->src_winTop, y2r,
				&scaler->info->dtrcConv_info);
		tca_dtrc_convter_onoff(VIOC_DTRC1, 1, 0);
	}
	else
	#endif//		
	{
		tcc_scaler_drv_common_set_path_selection(scaler);
		VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);

		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler->info->src_fmt);

		#ifdef CONFIG_VIOC_10BIT
		if(scaler->info->src_bit_depth == 10)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x1, 0x1);
		else if(scaler->info->src_bit_depth == 11)
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x3, 0x0);	/* YUV real 10bit support */
		else
			VIOC_RDMA_SetDataFormat(pSC_RDMABase, 0x0, 0x0);
		#endif

		//interlaced frame process ex) MPEG2
		if (scaler->info->interlaced) {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->src_winRight - scaler->info->src_winLeft), (scaler->info->src_winBottom - scaler->info->src_winTop)/2);
			VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->src_fmt, scaler->info->src_ImgWidth*2);
		} 
		else {
			VIOC_RDMA_SetImageSize(pSC_RDMABase, (scaler->info->src_winRight - scaler->info->src_winLeft), (scaler->info->src_winBottom - scaler->info->src_winTop));

			#ifdef CONFIG_VIOC_10BIT
			if(scaler->info->src_bit_depth == 10)
				VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->src_fmt, scaler->info->src_ImgWidth*2);
			else
			#endif//	
			{
				VIOC_RDMA_SetImageOffset(pSC_RDMABase, scaler->info->src_fmt, scaler->info->src_ImgWidth);
			}
		}

		pSrcBase0 = (unsigned int)scaler->info->src_Yaddr;
		pSrcBase1 = (unsigned int)scaler->info->src_Uaddr;
		pSrcBase2 = (unsigned int)scaler->info->src_Vaddr;
		if (scaler->info->src_fmt >= VIOC_IMG_FMT_444SEP) { // address limitation!!
			scaler->info->src_winLeft 	= (scaler->info->src_winLeft>>3)<<3;
			scaler->info->src_winRight = scaler->info->src_winLeft + crop_width;
			scaler->info->src_winRight = scaler->info->src_winLeft + (scaler->info->src_winRight - scaler->info->src_winLeft);
			tccxxx_GetAddress(scaler->info->src_fmt, (unsigned int)scaler->info->src_Yaddr, 		\
					scaler->info->src_ImgWidth, scaler->info->src_ImgHeight, 	\
					scaler->info->src_winLeft, scaler->info->src_winTop, 		\
					&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}

		if(scaler->force_fmt_RGB && scaler->force_fmt_YUV) {
			scaler->force_fmt_RGB = 0 ;
			scaler->force_fmt_YUV = 0;
		}

		if((scaler->info->src_fmt > VIOC_IMG_FMT_COMP)
			&&((scaler->info->dest_fmt < VIOC_IMG_FMT_COMP) ||scaler->force_fmt_RGB))
			VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 1);
		else
			VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);

#if defined(CONFIG_VIOC_AFBCDEC)
		if(scaler->afbc_dec.reg != NULL )
			VIOC_RDMA_SetIssue(pSC_RDMABase, 7, 16);
		else
#endif		
			VIOC_RDMA_SetIssue(pSC_RDMABase, 15, 16);

		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}

	// look up table use
	if(scaler->info->lut.use_lut) {
		tcc_set_lut_plugin(VIOC_LUT + scaler->info->lut.use_lut_number, scaler->rdma.id);
		tcc_set_lut_enable(VIOC_LUT + scaler->info->lut.use_lut_number, true);
	}
	
#if defined(CONFIG_VIOC_SAR)
	if(scaler->sar.reg != NULL)	{
		int ret = -1;
		ret = CheckSarPathSelection(scaler->rdma.id);
		if(ret >= 0){
			VIOC_SAR_POWER_ONOFF(1);
			VIOC_CONFIG_PlugIn(VIOC_SAR, scaler->rdma.id);
			vioc_sar_on((scaler->info->src_winRight - scaler->info->src_winLeft), (scaler->info->src_winBottom - scaler->info->src_winTop), scaler->sar.level);
		}
	}
#endif//


	if(scaler->sc.path == 0) {
		VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
	}
	else {
		VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->wdma.id);
	}

	if(scaler->sc.mixer_bypass) {
		bypass_mixer = 1;
		if(0 > VIOC_CONFIG_WMIXPath(scaler->rdma.id, 0))
			bypass_mixer = 0;
	}
	else {
		//VIOC_CONFIG_WMIXPath(scaler->rdma.id, 1);
		bypass_mixer = 0;
	}

	VIOC_SC_SetBypass(pSC_SCALERBase, 0);
	VIOC_SC_SetDstSize(pSC_SCALERBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop/* + 6*/));
	VIOC_SC_SetOutSize(pSC_SCALERBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop));
	VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
	VIOC_SC_SetUpdate(pSC_SCALERBase);

	if(!bypass_mixer) {
		if(scaler->sc.path == 0)
			VIOC_WMIX_SetSize(pSC_WMIXBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop));
		else
			VIOC_WMIX_SetSize(pSC_WMIXBase, (scaler->info->src_winRight - scaler->info->src_winLeft), (scaler->info->src_winBottom - scaler->info->src_winTop));
		VIOC_WMIX_SetUpdate(pSC_WMIXBase);
	}

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, scaler->info->dest_fmt);
	VIOC_WDMA_SetImageSize(pSC_WDMABase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop));

	#if !defined(CONFIG_PLATFORM_AVN) && !defined(CONFIG_PLATFORM_STB)
	if(scaler->info->wdma_aligned_offset)
		VIOC_WDMA_SetImageOffset_withYV12(pSC_WDMABase, scaler->info->dest_ImgWidth);
	else
		VIOC_WDMA_SetImageOffset(pSC_WDMABase, scaler->info->dest_fmt, scaler->info->dest_ImgWidth);
	#else
	VIOC_WDMA_SetImageOffset(pSC_WDMABase, scaler->info->dest_fmt, scaler->info->dest_ImgWidth);
	#endif//

	VIOC_WDMA_SetImageBase(pSC_WDMABase, (unsigned int)scaler->info->dest_Yaddr, (unsigned int)scaler->info->dest_Uaddr, (unsigned int)scaler->info->dest_Vaddr);
	if ((scaler->info->dest_fmt > VIOC_IMG_FMT_COMP) && 
		((scaler->info->src_fmt < VIOC_IMG_FMT_COMP) || scaler->force_fmt_RGB))
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 1);
	else
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);

#if defined(CONFIG_VIOC_AFBCDEC)
	tcc_scaler_drv_configure_AFBCDEC(scaler->afbc_dec.reg, scaler->info->src_Yaddr,
										pSC_RDMABase, 1, 0, scaler->info->src_fmt, 
										scaler->info->src_ImgWidth, scaler->info->src_ImgHeight);
#endif

	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, 0xFFFFFFFF);		// wdma status register all clear.

	spin_unlock_irq(&(scaler->data->cmd_lock));
	if (scaler->info->responsetype  == SCALER_POLLING) {
		ret = wait_event_interruptible_timeout(scaler->data->poll_wq,  scaler->data->block_operating == 0, msecs_to_jiffies(500));
		if (ret <= 0) {
			scaler->data->block_operating = 0;
			printk("%s():  time out(%d), line(%d). \n", __func__, ret, __LINE__);
		}
		// look up table use
		if(scaler->info->lut.use_lut) {
			tcc_set_lut_enable(VIOC_LUT + scaler->info->lut.use_lut_number, false);
			scaler->info->lut.use_lut = false;
		}
	} else if (scaler->info->responsetype  == SCALER_NOWAIT) {
		if(scaler->info->viqe_onthefly & 0x2)
			scaler->data->block_operating = 0;
	}
	return ret;
}


static unsigned int scaler_drv_common_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_common_drv_type	*scaler = dev_get_drvdata(misc->parent);
	int ret = 0;

	if(scaler->data == NULL)
		return 0;

	poll_wait(filp, &(scaler->data->poll_wq), wait);
	spin_lock_irq(&(scaler->data->poll_lock));
	if (scaler->data->block_operating == 0)
		ret = (POLLIN|POLLRDNORM);
	spin_unlock_irq(&(scaler->data->poll_lock));

	return ret;
}

static irqreturn_t scaler_drv_common_handler(int irq, void *client_data)
{
	struct scaler_common_drv_type *scaler = (struct scaler_common_drv_type *)client_data;

	if (is_vioc_intr_activatied(scaler->vioc_intr->id, scaler->vioc_intr->bits) == false){
		return IRQ_NONE;
	}

	vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);

	dprintk("%s():  block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
			scaler->data->block_operating, scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->poll_count);		

	if(scaler->data->block_operating >= 1)
		scaler->data->block_operating = 0;

	wake_up_interruptible(&(scaler->data->poll_wq));

	if(scaler->data->block_waiting)
		wake_up_interruptible(&scaler->data->cmd_wq);

	// look up table use
	if(scaler->info->lut.use_lut) {
		tcc_set_lut_enable(VIOC_LUT + scaler->info->lut.use_lut_number, false);
		scaler->info->lut.use_lut = false;
	}

	if(scaler->rdma.id == VIOC_RDMA02 || scaler->rdma.id == VIOC_RDMA03 || scaler->rdma.id == VIOC_RDMA07 || scaler->rdma.id >= VIOC_RDMA11) {
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if (VIOC_CONFIG_DMAPath_Support()) {
			VIOC_CONFIG_DMAPath_UnSet(VIOC_MC1);
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		}
		#endif//

		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if (VIOC_CONFIG_DMAPath_Support()) {
			VIOC_CONFIG_DMAPath_UnSet(VIOC_DTRC1);
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		}
		#endif//
	}

	return IRQ_HANDLED;
}

static long scaler_drv_common_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_common_drv_type	*scaler = dev_get_drvdata(misc->parent);
	int ret = 0;

	dprintk("%s():  cmd(%d), block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
			cmd, scaler->data->block_operating, scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->poll_count);

	switch(cmd) {
		case TCC_SCALER_PATH:
			{
				struct tc_scaler_path_set sc_drv_res;

				if(copy_from_user(&sc_drv_res,(struct tc_scaler_path_set *)arg, sizeof(struct tc_scaler_path_set))) {
					printk(KERN_ALERT "%s():  Error copy_from_user(%d)\n", __func__, cmd);
					ret = -EFAULT;
				}
				mutex_lock(&scaler->data->io_mutex);

				if(ret >= 0)
				{
					tcc_scaler_drv_common_set_path_resource(scaler, &sc_drv_res);
					tcc_scaler_drv_common_register_interrupt(scaler, 1);
					scaler->data->cmd_count = 0;
				}
				mutex_unlock(&scaler->data->io_mutex);

			}
			break;
			
		case TCC_SCALER_FILTER2D:
			{
				struct tc_scaler_filter2D_set sc_drv_filter_res;
				if(copy_from_user(&sc_drv_filter_res,(struct tc_scaler_filter2D_set *)arg, sizeof(struct tc_scaler_filter2D_set))) {
					printk(KERN_ALERT "%s():  Error copy_from_user(%d)\n", __func__, cmd);
					ret = -EFAULT;
				}
				mutex_lock(&scaler->data->io_mutex);

				if(ret >= 0)
				{
					tcc_scaler_drv_common_set_filter2d(scaler, &sc_drv_filter_res);
					scaler->data->cmd_count = 0;
				}
				mutex_unlock(&scaler->data->io_mutex);
			}		
			break;
			
		case TCC_SCALER_IOCTRL:
		case TCC_SCALER_IOCTRL_KERENL:
			mutex_lock(&scaler->data->io_mutex);
			if(scaler->data->block_operating) {
				scaler->data->block_waiting = 1;
				ret = wait_event_interruptible_timeout(scaler->data->cmd_wq, scaler->data->block_operating == 0, msecs_to_jiffies(200));
				if(ret <= 0) {
					scaler->data->block_operating = 0;
					printk("%s(%d):  timed_out block_operation(%d), cmd_count(%d). \n", __func__, ret, scaler->data->block_waiting, scaler->data->cmd_count);
				}
			#if 1//def DUMP_REG
				VIOC_RDMA_DUMP(NULL, scaler->rdma.id);
				VIOC_WDMA_DUMP(NULL, scaler->wdma.id);
				if(scaler->sc.reg != NULL)
					VIOC_SCALER_DUMP(scaler->sc.reg, scaler->sc.id);
				VIOC_IREQConfig_DUMP(0x200, 0x80);
				VIOC_IREQConfig_DUMP(0x40, 0x40);
			#endif//
				ret = 0;
			}

			if(cmd == TCC_SCALER_IOCTRL_KERENL) {
				memcpy(scaler->info, (SCALER_TYPE*)arg, sizeof(SCALER_TYPE));
			} else {
				if(copy_from_user(scaler->info, (SCALER_TYPE*)arg, sizeof(SCALER_TYPE))) {
					printk(KERN_ALERT "%s():  Not Supported copy_from_user(%d). \n", __func__, cmd);
					ret = -EFAULT;
				}
			}

			if(ret >= 0) {
				if(scaler->data->block_operating >= 1) {
					printk("%s():  block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
							scaler->data->block_operating, scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->poll_count);
				}

//				convert_image_format(scaler);

				scaler->data->block_waiting = 0;
				scaler->data->block_operating = 1;
				ret = tcc_scaler_drv_common_run(scaler);
				if(ret < 0) scaler->data->block_operating = 0;
				scaler->data->cmd_count += 1;
			}
			mutex_unlock(&scaler->data->io_mutex);
			return ret;
		default:
			printk(KERN_ALERT "%s():  Not Supported SCALER0_IOCTL(%d). \n", __func__, cmd);
			break;			
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long scaler_drv_common_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return scaler_drv_common_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int scaler_drv_common_release(struct inode *inode, struct file *filp)
{
	int component_num;
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_common_drv_type	*scaler = dev_get_drvdata(misc->parent);

	int ret = 0;
	dprintk("%s():  In -release(%d), block(%d), wait(%d), cmd(%d), irq(%d) \n", __func__, scaler->data->dev_opened, scaler->data->block_operating,
			scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->irq_reged);

#ifdef DUMP_REG
	VIOC_RDMA_DUMP(NULL, scaler->rdma.id);
	VIOC_WDMA_DUMP(NULL, scaler->wdma.id);
	VIOC_SCALER_DUMP(NULL, scaler->sc.id);
	VIOC_IREQConfig_DUMP(0x40, 0x40); 	//Path_SC
	VIOC_IREQConfig_DUMP(0x200, 0x80); 	//Path_RDMA
#endif//
	if (scaler->data->dev_opened > 0)
		scaler->data->dev_opened--;

	if (scaler->data->dev_opened == 0) {
		if (scaler->data->block_operating) {
			ret = wait_event_interruptible_timeout(scaler->data->cmd_wq, scaler->data->block_operating == 0, msecs_to_jiffies(200));
			if (ret <= 0){
	#if 1//def DUMP_REG
				VIOC_RDMA_DUMP(NULL, scaler->rdma.id);
				VIOC_WDMA_DUMP(NULL, scaler->wdma.id);
				VIOC_SCALER_DUMP(NULL, scaler->sc.id);
				VIOC_IREQConfig_DUMP(0x40, 0x40); 	//Path_SC
				VIOC_IREQConfig_DUMP(0x200, 0x80); 	//Path_RDMA
	#endif//
				printk("%s(%d):  timed_out block_operation:%d, cmd_count:%d. \n", __func__, ret, scaler->data->block_waiting, scaler->data->cmd_count);
			}
		}

		if (scaler->data->irq_reged) {
			vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);
			vioc_intr_disable(scaler->irq, scaler->vioc_intr->id, scaler->vioc_intr->bits);
			free_irq(scaler->irq, scaler);
			scaler->data->irq_reged = 0;
		}

	#if defined(CONFIG_VIOC_AFBCDEC)
		if(scaler->afbc_dec.reg) {
			VIOC_AFBCDec_TurnOFF(scaler->afbc_dec.reg);
			VIOC_CONFIG_AFBCDECPath(scaler->afbc_dec.id, scaler->rdma.id, 0);
			scaler->afbc_dec.reg = NULL;
		}
	#endif
		VIOC_CONFIG_PlugOut(scaler->sc.id);

		if(scaler->rdma.id == VIOC_RDMA02 || scaler->rdma.id == VIOC_RDMA03 || scaler->rdma.id == VIOC_RDMA07 || scaler->rdma.id >= VIOC_RDMA11) {
			#if defined(CONFIG_VIOC_MAP_DECOMP) || defined(CONFIG_VIOC_DTRC_DECOMP)
			if (VIOC_CONFIG_DMAPath_Support()) {
				component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
				if((component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
					VIOC_CONFIG_DMAPath_UnSet(scaler->rdma.id);
				VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
			}
			#endif
		}

		scaler->data->block_operating = scaler->data->block_waiting = 0;
		scaler->data->poll_count = scaler->data->cmd_count = 0;
	}

	if (scaler->clk)
		clk_disable_unprepare(scaler->clk);

	dprintk("%s():  Out - release(%d). \n", __func__, scaler->data->dev_opened);

	return 0;
}

static int scaler_drv_common_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_common_drv_type	*scaler = dev_get_drvdata(misc->parent);
	struct tc_scaler_path_set vioc_resorce;

	int ret = 0;
	printk("%s():  In -open(%d), block(%d), wait(%d), cmd(%d), irq(%d) \n", __func__, scaler->data->dev_opened, scaler->data->block_operating, \
			scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->irq_reged);

	if (scaler->clk)
		clk_prepare_enable(scaler->clk);

	if (!scaler->data->irq_reged) {
		// VIOC_CONFIG_StopRequest(1);

		ret = request_irq(scaler->irq, scaler_drv_common_handler, IRQF_SHARED, scaler->misc->name, scaler);
		if (ret) {
			if (scaler->clk)
				clk_disable_unprepare(scaler->clk);
			printk("failed(ret %d) to aquire %s request_irq(%d). \n", ret, scaler->misc->name, scaler->irq);
			return -EFAULT;
		}
		dprintk("success(ret %d) to aquire %s request_irq(%d). \n", ret, scaler->misc->name, scaler->irq);

		#if 1 //scaler 1 drv
		vioc_resorce.rdma_num = 16;
		vioc_resorce.scaler_num = 1;
		vioc_resorce.wmixer_num = 5;
		vioc_resorce.wdma_num = 6;
		vioc_resorce.scaler_pos = 0;
		vioc_resorce.mixer_bypass = 0;
		#else //scaler 3 drv
		vioc_resorce.rdma_num = 7;
		vioc_resorce.scaler_num = 3;	
		vioc_resorce.wmixer_num = 1;
		vioc_resorce.wdma_num = 1;		
		vioc_resorce.scaler_pos = 0;
		vioc_resorce.mixer_bypass = 0;
		#endif//
		#if 1//defined(CONFIG_VIOC_AFBCDEC)
		vioc_resorce.afbc_dec_need = 0;
		vioc_resorce.afbc_dec_num = 0;
		#if defined(CONFIG_VIOC_AFBCDEC)
		scaler->afbc_dec.reg = NULL;
		scaler->afbc_dec.id = 0;
		#endif
		#endif
		vioc_resorce.pixel_mapper_n = -1;
		tcc_scaler_drv_common_set_path_resource(scaler, &vioc_resorce);
		tcc_scaler_drv_common_register_interrupt(scaler, 1);
		scaler->data->irq_reged = 1;
	}

	scaler->data->dev_opened++;

	printk("%s():  Out - open(%d). \n", __func__, scaler->data->dev_opened);
	return ret;
}

static struct file_operations scaler_drv_common_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= scaler_drv_common_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= scaler_drv_common_compat_ioctl,
#endif
	.mmap			= scaler_drv_common_mmap,
	.open			= scaler_drv_common_open,
	.release		= scaler_drv_common_release,
	.poll			= scaler_drv_common_poll,
};

static int scaler_drv_common_probe(struct platform_device *pdev)
{
	struct scaler_common_drv_type *scaler;
	int ret = -ENODEV;
	dprintk("%s \n", __func__);
	scaler = kzalloc(sizeof(struct scaler_common_drv_type), GFP_KERNEL);
	if (!scaler)
		return -ENOMEM;

	scaler->clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(scaler->clk))
		scaler->clk = NULL;

	scaler->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (scaler->misc == 0)
		goto err_misc_alloc;

	scaler->info = kzalloc(sizeof(SCALER_TYPE), GFP_KERNEL);
	if (scaler->info == 0)
		goto err_info_alloc;

	scaler->data = kzalloc(sizeof(struct scaler_data), GFP_KERNEL);
	if (scaler->data == 0)
		goto err_data_alloc;

	scaler->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler->vioc_intr == 0)
		goto err_vioc_intr_alloc;

 	/* register scaler discdevice */
	scaler->misc->minor = MISC_DYNAMIC_MINOR;
	scaler->misc->fops = &scaler_drv_common_fops;
	scaler->misc->name = "scaler_drv_common";
	scaler->misc->parent = &pdev->dev;
	ret = misc_register(scaler->misc);
	if (ret)
		goto err_misc_register;

	scaler->irq = platform_get_irq(pdev, 0);

	dprintk("%s: irq: %d \n", scaler->misc->name, scaler->irq);
	
	spin_lock_init(&(scaler->data->poll_lock));
	spin_lock_init(&(scaler->data->cmd_lock));
	mutex_init(&(scaler->data->io_mutex));
	
	init_waitqueue_head(&(scaler->data->poll_wq));
	init_waitqueue_head(&(scaler->data->cmd_wq));

	platform_set_drvdata(pdev, scaler);

	pr_info("%s: Scaler Driver Initialized\n", pdev->name);
	
	return 0;

	misc_deregister(scaler->misc);

err_misc_register:
	kfree(scaler->vioc_intr);

err_vioc_intr_alloc:
	kfree(scaler->data);

err_data_alloc:
	kfree(scaler->info);

err_info_alloc:
	kfree(scaler->misc);

err_misc_alloc:
	kfree(scaler);

	printk("%s: %s: err ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int scaler_drv_common_remove(struct platform_device *pdev)
{
	struct scaler_common_drv_type *scaler = (struct scaler_common_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(scaler->misc);
	kfree(scaler->vioc_intr);
	kfree(scaler->data);
	kfree(scaler->info);
	kfree(scaler->misc);
	kfree(scaler);
	return 0;
}

static int scaler_drv_common_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int scaler_drv_common_resume(struct platform_device *pdev)
{

	return 0;
}

static struct of_device_id scaler_common_of_match[] = {
	{ .compatible = "telechips,scaler_drv_common" },
	{}
};
MODULE_DEVICE_TABLE(of, scaler_common_of_match);

static struct platform_driver scaler_common_driver = {
	.probe		= scaler_drv_common_probe,
	.remove		= scaler_drv_common_remove,
	.suspend	= scaler_drv_common_suspend,
	.resume		= scaler_drv_common_resume,
	.driver 	= {
		.name	= "scaler_common_pdev",
		.owner	= THIS_MODULE,
		#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(scaler_common_of_match),
		#endif
	},
};

static int __init scaler_common_drv_init(void)
{
	dprintk("%s \n", __func__);
	return platform_driver_register(&scaler_common_driver);
}

static void __exit scaler_common_drv_exit(void)
{
	platform_driver_unregister(&scaler_common_driver);
}

module_init(scaler_common_drv_init);
module_exit(scaler_common_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips Scaler_common Driver");
MODULE_LICENSE("GPL");

