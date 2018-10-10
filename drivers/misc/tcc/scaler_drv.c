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
#include <asm/system_info.h>

#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/tcc_scaler_ioctrl.h>
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
#include <video/tcc/tcc_video_private.h>
#include <video/tcc/tca_map_converter.h>
#endif

#ifdef CONFIG_VIOC_DTRC_DECOMP
#include <video/tcc/tca_dtrc_converter.h>
#endif

static int debug	   		= 0;
#define dprintk(msg...)	if(debug) { 	printk( "TCC_SCALER1:  " msg); 	}

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

struct scaler_drv_vioc {
	volatile void __iomem *reg;
	unsigned int id;
//	unsigned int path;
};

struct scaler_drv_type {
	struct vioc_intr_type	*vioc_intr;

	unsigned int		id;
	unsigned int		irq;

	struct miscdevice	*misc;

	struct scaler_drv_vioc	rdma;
 	struct scaler_drv_vioc	wmix;
	struct scaler_drv_vioc	sc;
	struct scaler_drv_vioc	wdma;

	struct clk		*clk;
	struct scaler_data	*data;
	SCALER_TYPE		*info;

	unsigned int		settop_support;
};

extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy,
								unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);
extern int range_is_allowed(unsigned long pfn, unsigned long size);

static int scaler_drv_mmap(struct file *filp, struct vm_area_struct *vma)
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

static char tcc_scaler_run(struct scaler_drv_type *scaler)
{
	int ret = 0;
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int crop_width = 0;

	volatile void __iomem *pSC_RDMABase = scaler->rdma.reg;
	volatile void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	volatile void __iomem *pSC_WDMABase = scaler->wdma.reg;
	volatile void __iomem *pSC_SCALERBase = scaler->sc.reg;

	dprintk("%s():  IN. \n", __func__);

	spin_lock_irq(&(scaler->data->cmd_lock));

	dprintk("scaler %d src   add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d )%d %d %d %d \n", scaler->sc.id, 
		scaler->info->src_Yaddr, scaler->info->src_Uaddr, scaler->info->src_Vaddr, scaler->info->src_fmt, 
		scaler->info->src_ImgWidth, scaler->info->src_ImgHeight, 
		scaler->info->src_winLeft, scaler->info->src_winTop, scaler->info->src_winRight, scaler->info->src_winBottom);
	dprintk("scaler %d dest  add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d )%d %d %d %d \n", scaler->sc.id, 
		scaler->info->dest_Yaddr, scaler->info->dest_Uaddr, scaler->info->dest_Vaddr, scaler->info->dest_fmt, 
		scaler->info->dest_ImgWidth, scaler->info->dest_ImgHeight, 
		scaler->info->dest_winLeft, scaler->info->dest_winTop, scaler->info->dest_winRight, scaler->info->dest_winBottom);
	dprintk("scaler %d interlace:%d \n", scaler->sc.id,  scaler->info->interlaced);

	crop_width = scaler->info->src_winRight - scaler->info->src_winLeft;

	#ifdef CONFIG_VIOC_MAP_DECOMP
	if(scaler->info->mapConv_info.m_CompressedY[0] != 0)
	{
		int y2r = 0;
		dprintk("scaler %d path-id(rdma:%d) \n", scaler->sc.id, scaler->rdma.id);
		dprintk("scaler %d src  map converter: size: %d %d pos:%d %d\n", scaler->sc.id, scaler->info->src_winRight - scaler->info->src_winLeft, 
									scaler->info->src_winBottom - scaler->info->src_winTop, 
									scaler->info->src_winLeft, scaler->info->src_winTop);	

		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
			if((component_num < VIOC_MC0 ) || (component_num > (VIOC_MC0 + VIOC_MC_MAX))){
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				tca_map_convter_swreset(VIOC_MC1);  //disable it to prevent system-hang!!
			}
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, VIOC_MC1);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(scaler->wmix.id, VIOC_MC1);
			#endif
		}

		if( scaler->info->dest_fmt < SCLAER_COMPRESS_DATA )
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
			int component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
			if((component_num < VIOC_DTRC0 ) || (component_num > (VIOC_DTRC0 + VIOC_DTRC_MAX))){
				VIOC_CONFIG_DMAPath_UnSet(component_num);
				//tca_dtrc_convter_swreset(VIOC_DTRC1); //disable it to prevent system-hang!!
			}
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, VIOC_DTRC1);
		}

		if( scaler->info->dest_fmt < SCLAER_COMPRESS_DATA )
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
		if (scaler->settop_support) {
			VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		}
		
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
			dprintk("%s():  src addr is not allocate. \n", __func__);
			scaler->info->src_winLeft 	= (scaler->info->src_winLeft>>3)<<3;
			scaler->info->src_winRight = scaler->info->src_winLeft + crop_width;
			scaler->info->src_winRight = scaler->info->src_winLeft + (scaler->info->src_winRight - scaler->info->src_winLeft);
			tccxxx_GetAddress(scaler->info->src_fmt, (unsigned int)scaler->info->src_Yaddr, 		\
										scaler->info->src_ImgWidth, scaler->info->src_ImgHeight, 	\
										scaler->info->src_winLeft, scaler->info->src_winTop, 		\
										&pSrcBase0, &pSrcBase1, &pSrcBase2);
		}

		if ((scaler->info->src_fmt > VIOC_IMG_FMT_COMP) && (scaler->info->dest_fmt < VIOC_IMG_FMT_COMP)) {
			VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 1);
		}
		else{		
			VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, 0);
		}

		if(scaler->info->src_fmt < VIOC_IMG_FMT_COMP)
			VIOC_RDMA_SetImageRGBSwapMode(pSC_RDMABase, scaler->info->src_rgb_swap);
		else
			VIOC_RDMA_SetImageRGBSwapMode(pSC_RDMABase, 0);

		VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)pSrcBase0, (unsigned int)pSrcBase1, (unsigned int)pSrcBase2);
		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}

	// look up table use
	if(scaler->info->lut.use_lut) {
		tcc_set_lut_plugin(VIOC_LUT + scaler->info->lut.use_lut_number, scaler->rdma.id);
		tcc_set_lut_enable(VIOC_LUT + scaler->info->lut.use_lut_number, true);
	}

	VIOC_SC_SetBypass(pSC_SCALERBase, 0);
#if defined(CONFIG_MC_WORKAROUND)
	if(!system_rev && scaler->info->mapConv_info.m_CompressedY[0] != 0)
	{
		unsigned int plus_height = VIOC_SC_GetPlusSize((scaler->info->src_winBottom - scaler->info->src_winTop), (scaler->info->dest_winBottom - scaler->info->dest_winTop));

		VIOC_SC_SetDstSize(pSC_SCALERBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop) + plus_height);
	}
	else
#endif
		VIOC_SC_SetDstSize(pSC_SCALERBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop));
	VIOC_SC_SetOutSize(pSC_SCALERBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop));
	VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
	VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
	VIOC_SC_SetUpdate(pSC_SCALERBase);

	if(get_vioc_index(scaler->rdma.id) == get_vioc_index(VIOC_RDMA07))
		VIOC_CONFIG_WMIXPath(scaler->rdma.id, 0); // wmixer op is bypass mode.
	else
		VIOC_CONFIG_WMIXPath(scaler->rdma.id, 1); // wmixer op is mixing mode.	
		
	VIOC_WMIX_SetSize(pSC_WMIXBase, (scaler->info->dest_winRight - scaler->info->dest_winLeft), (scaler->info->dest_winBottom - scaler->info->dest_winTop));
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, scaler->info->dest_fmt);

	if ( scaler->info->dest_fmt < VIOC_IMG_FMT_COMP )
		VIOC_WDMA_SetImageRGBSwapMode( pSC_WDMABase, scaler->info->dst_rgb_swap);
	else
		VIOC_WDMA_SetImageRGBSwapMode( pSC_WDMABase, 0);

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
	if ((scaler->info->src_fmt < VIOC_IMG_FMT_COMP) && (scaler->info->dest_fmt > VIOC_IMG_FMT_COMP)) {
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 1);
	} else {
		VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
	}

	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.
	spin_unlock_irq(&(scaler->data->cmd_lock));

	if (scaler->info->responsetype  == SCALER_POLLING) {
		ret = wait_event_interruptible_timeout(scaler->data->poll_wq,  scaler->data->block_operating == 0, msecs_to_jiffies(500));
		if (ret <= 0) {
			scaler->data->block_operating = 0;
			printk("%s():  time out(%d), line(%d). \n", __func__, ret, __LINE__);
		}
		// look up table use
		if(scaler->info->lut.use_lut)		{
			tcc_set_lut_enable(VIOC_LUT + scaler->info->lut.use_lut_number, false);
			scaler->info->lut.use_lut = false;
		}
	} else if (scaler->info->responsetype  == SCALER_NOWAIT) {
		if(scaler->info->viqe_onthefly & 0x2)
			 scaler->data->block_operating = 0;
	}

	return ret;
}

static char tcc_scaler_data_copy_run(struct scaler_drv_type *scaler, SCALER_DATA_COPY_TYPE *copy_info)
{
	int ret = 0;
	volatile void __iomem *pSC_RDMABase = scaler->rdma.reg;
	volatile void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	volatile void __iomem *pSC_WDMABase = scaler->wdma.reg;
	volatile void __iomem *pSC_SCALERBase = scaler->sc.reg;

	dprintk("%s():  \n", __func__);
	dprintk("Src  : addr:0x%x 0x%x 0x%x  fmt:%d \n", copy_info->src_y_addr, copy_info->src_u_addr, copy_info->src_v_addr, copy_info->src_fmt);
	dprintk("Dest: addr:0x%x 0x%x 0x%x  fmt:%d \n", copy_info->dst_y_addr, copy_info->dst_u_addr, copy_info->dst_v_addr, copy_info->dst_fmt);
	dprintk("Size : W:%d  H:%d \n", copy_info->img_width, copy_info->img_height);


	spin_lock_irq(&(scaler->data->cmd_lock));

	VIOC_RDMA_SetImageFormat(pSC_RDMABase, copy_info->src_fmt);
	VIOC_RDMA_SetImageSize(pSC_RDMABase, copy_info->img_width, copy_info->img_height);
	VIOC_RDMA_SetImageOffset(pSC_RDMABase, copy_info->src_fmt, copy_info->img_width);
	VIOC_RDMA_SetImageBase(pSC_RDMABase, (unsigned int)copy_info->src_y_addr, (unsigned int)copy_info->src_u_addr,  (unsigned int)copy_info->src_v_addr);

	VIOC_SC_SetBypass(pSC_SCALERBase, false);
	VIOC_SC_SetDstSize(pSC_SCALERBase, copy_info->img_width, copy_info->img_height);
	VIOC_SC_SetOutSize(pSC_SCALERBase, copy_info->img_width, copy_info->img_height);
	VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
	VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
	VIOC_SC_SetUpdate(pSC_SCALERBase);
	VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.


	if(get_vioc_index(scaler->rdma.id) == get_vioc_index(VIOC_RDMA07))
		VIOC_CONFIG_WMIXPath(scaler->rdma.id, 0); // wmixer op is bypass mode.
	else
		VIOC_CONFIG_WMIXPath(scaler->rdma.id, 1); // wmixer op is mixing mode.	
		
	VIOC_WMIX_SetSize(pSC_WMIXBase, copy_info->img_width, copy_info->img_height);
	VIOC_WMIX_SetUpdate(pSC_WMIXBase);

	VIOC_WDMA_SetImageFormat(pSC_WDMABase, copy_info->dst_fmt);
	VIOC_WDMA_SetImageSize(pSC_WDMABase, copy_info->img_width, copy_info->img_height);
	VIOC_WDMA_SetImageOffset(pSC_WDMABase, copy_info->dst_fmt, copy_info->img_width);
	VIOC_WDMA_SetImageBase(pSC_WDMABase, (unsigned int)copy_info->dst_y_addr, (unsigned int)copy_info->dst_u_addr, (unsigned int)copy_info->dst_v_addr);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0/*OFF*/);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK); // wdma status register all clear.

	spin_unlock_irq(&(scaler->data->cmd_lock));

	if (copy_info->rsp_type == SCALER_POLLING) {
		ret = wait_event_interruptible_timeout(scaler->data->poll_wq, scaler->data->block_operating == 0, msecs_to_jiffies(500));
		if (ret <= 0) {
			 scaler->data->block_operating = 0;
			printk("wmixer time out: %d, Line: %d. \n", ret, __LINE__);
		}
	}

	return ret;
}

static unsigned int scaler_drv_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);
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

static irqreturn_t scaler_drv_handler(int irq, void *client_data)
{
	struct scaler_drv_type *scaler = (struct scaler_drv_type *)client_data;

	if (is_vioc_intr_activatied(scaler->vioc_intr->id, scaler->vioc_intr->bits) == false)
		return IRQ_NONE;
	vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);

	dprintk("%s():  block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
			scaler->data->block_operating, scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->poll_count);		

	if(scaler->data->block_operating >= 1)
		scaler->data->block_operating = 0;

	wake_up_interruptible(&(scaler->data->poll_wq));

	if(scaler->data->block_waiting)
		wake_up_interruptible(&scaler->data->cmd_wq);

	// look up table use
	if(scaler->info->lut.use_lut)		{
		tcc_set_lut_enable(VIOC_LUT + scaler->info->lut.use_lut_number, false);
		scaler->info->lut.use_lut = false;
	}

	if (VIOC_CONFIG_DMAPath_Support()) {
		int component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX))) {
			VIOC_CONFIG_DMAPath_UnSet(VIOC_MC1);
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		}
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		if((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX))) {
			VIOC_CONFIG_DMAPath_UnSet(VIOC_DTRC1);
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		}
		#endif
	} else {
		#ifdef CONFIG_ARCH_TCC803X
		VIOC_CONFIG_MCPath(scaler->wmix.id, scaler->rdma.id);
		#endif
	}

	return IRQ_HANDLED;
}

static long scaler_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

//	volatile void __iomem *pSC_RDMABase = scaler->rdma.reg;
//	SCALER_PLUGIN_Type scaler_plugin;

	SCALER_DATA_COPY_TYPE copy_info;
	int ret = 0;

	dprintk("%s():  cmd(%d), block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
			cmd, scaler->data->block_operating, scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->poll_count);

	switch(cmd) {
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
				ret = tcc_scaler_run(scaler);
				if(ret < 0) scaler->data->block_operating = 0;
			}
			mutex_unlock(&scaler->data->io_mutex);
			return ret;
#if 0
		case TCC_SCALER_VIOC_PLUGIN:
			mutex_lock(&scaler->data->io_mutex);
			if(copy_from_user(&scaler_plugin,(SCALER_PLUGIN_Type *)arg, sizeof(SCALER_PLUGIN_Type))) {
				printk(KERN_ALERT "%s():  Not Supported copy_from_user(%d)\n", __func__, cmd);
				ret = -EFAULT;
			}

			// set to scaler & plug in.
			VIOC_CONFIG_SWReset(scaler_plugin.scaler_no, VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(scaler_plugin.scaler_no, VIOC_CONFIG_CLEAR);

			VIOC_SC_SetBypass(scaler->sc.reg, scaler_plugin.bypass_mode);
			VIOC_SC_SetDstSize(scaler->sc.reg, scaler_plugin.dst_width, scaler_plugin.dst_height);
			VIOC_SC_SetOutSize(scaler->sc.reg, scaler_plugin.dst_width, scaler_plugin.dst_height);
			VIOC_SC_SetOutPosition(scaler->sc.reg, scaler_plugin.dst_win_left, scaler_plugin.dst_win_top);
			VIOC_CONFIG_PlugIn(scaler_plugin.scaler_no, scaler_plugin.plugin_path);
			VIOC_SC_SetUpdate(scaler->sc.reg);
			mutex_unlock(&scaler->data->io_mutex);
			return ret;

		case TCC_SCALER_VIOC_PLUGOUT:
			VIOC_RDMA_SetImageIntl(pSC_RDMABase, 0);
			ret = VIOC_CONFIG_PlugOut((unsigned int)arg);
			return ret;
#endif
		case TCC_SCALER_VIOC_DATA_COPY:
			mutex_lock(&scaler->data->io_mutex);
			if(scaler->data->block_operating) {
				scaler->data->block_waiting = 1;
				ret = wait_event_interruptible_timeout(scaler->data->cmd_wq, scaler->data->block_operating == 0, msecs_to_jiffies(200));
				if(ret <= 0) {
					scaler->data->block_operating = 0;
					printk("%s(%d):  wmixer 0 timed_out block_operation(%d), cmd_count(%d). \n", __func__, ret, scaler->data->block_waiting, scaler->data->cmd_count);
				}
				ret = 0;
			}

			if(copy_from_user(&copy_info, (SCALER_DATA_COPY_TYPE *)arg, sizeof(SCALER_DATA_COPY_TYPE))) {
				printk(KERN_ALERT "%s():  Not Supported copy_from_user(%d)\n", __func__, cmd);
				ret = -EFAULT;
			}

			if(ret >= 0) {
				if(scaler->data->block_operating >= 1) {
					printk("%s():  block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
						scaler->data->block_operating, scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->poll_count);
				}

				scaler->data->block_waiting = 0;
				scaler->data->block_operating = 1;
				ret = tcc_scaler_data_copy_run(scaler, &copy_info);
				if(ret < 0) 	scaler->data->block_operating = 0;
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
static long scaler_drv_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return scaler_drv_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int scaler_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

	int ret = 0;
	dprintk("%s():  In -release(%d), block(%d), wait(%d), cmd(%d), irq(%d) \n", __func__, scaler->data->dev_opened, scaler->data->block_operating, \
			scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->irq_reged);

	if (scaler->data->dev_opened > 0) {
		scaler->data->dev_opened--;
	}

	if (scaler->data->dev_opened == 0) {
		if (scaler->data->block_operating) {
			ret = wait_event_interruptible_timeout(scaler->data->cmd_wq, scaler->data->block_operating == 0, msecs_to_jiffies(200));
			if (ret <= 0) {
	 			printk("%s(%d):  timed_out block_operation:%d, cmd_count:%d. \n", __func__, ret, scaler->data->block_waiting, scaler->data->cmd_count);
		#if 0 // debug!!
				{
					if(scaler->info->mapConv_info.m_CompressedY[0] != 0)
					{
						VIOC_MC_DUMP(VIOC_MC1);
						printk("Map-converter :: 0x%x/0x%x - 0x%x/0x%x, Stride(%d/%d), Depth(%d/%d)\n",
								scaler->info->mapConv_info.m_CompressedY[0], scaler->info->mapConv_info.m_CompressedCb[0],
								scaler->info->mapConv_info.m_FbcYOffsetAddr[0], scaler->info->mapConv_info.m_FbcCOffsetAddr[0],
								scaler->info->mapConv_info.m_uiLumaStride, scaler->info->mapConv_info.m_uiChromaStride,
								scaler->info->mapConv_info.m_uiLumaBitDepth, scaler->info->mapConv_info.m_uiChromaBitDepth);
					}
					else{
						VIOC_RDMA_DUMP(scaler->rdma.reg);
					}
					VIOC_SCALER_DUMP(scaler->sc.reg);
					VIOC_WMIX_DUMP(scaler->wmix.reg);
					VIOC_WDMA_DUMP(scaler->wdma.reg);
					printk("scaler src :: addr 0x%p 0x%p 0x%p, fmt :0x%x, %d bpp IMG:(%d %d )%d %d %d %d \n",
								scaler->info->src_Yaddr, scaler->info->src_Uaddr, scaler->info->src_Vaddr, scaler->info->src_fmt, scaler->info->src_bit_depth,
								scaler->info->src_ImgWidth, scaler->info->src_ImgHeight,
								scaler->info->src_winLeft, scaler->info->src_winTop, scaler->info->src_winRight, scaler->info->src_winBottom);
					printk("scaler dest :: addr 0x%p 0x%p 0x%p, fmt :0x%x, %d bpp IMG:(%d %d )%d %d %d %d \n",
								scaler->info->dest_Yaddr, scaler->info->dest_Uaddr, scaler->info->dest_Vaddr, scaler->info->dest_fmt, scaler->info->dest_bit_depth,
								scaler->info->dest_ImgWidth, scaler->info->dest_ImgHeight,
								scaler->info->dest_winLeft, scaler->info->dest_winTop, scaler->info->dest_winRight, scaler->info->dest_winBottom);
					printk("scaler etc :: otf: %d, interlaced %d, plug: %d, div: %d, color: %d, align: %d \n",
								scaler->info->viqe_onthefly, scaler->info->interlaced, scaler->info->plugin_path, scaler->info->divide_path,
								scaler->info->color_base, scaler->info->wdma_aligned_offset);
				}
		#endif
			}
		}

		if (scaler->data->irq_reged) {
			vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);
			vioc_intr_disable(scaler->irq, scaler->vioc_intr->id, scaler->vioc_intr->bits);
			free_irq(scaler->irq, scaler);
			scaler->data->irq_reged = 0;
		}

		VIOC_CONFIG_PlugOut(scaler->sc.id);

		if (VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
			#ifdef CONFIG_VIOC_MAP_DECOMP
			if((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX))) {
				VIOC_CONFIG_DMAPath_UnSet(scaler->rdma.id);
				tca_map_convter_swreset(VIOC_MC1);
			}
			#endif
			#ifdef CONFIG_VIOC_DTRC_DECOMP
			if((component_num >= VIOC_DTRC0) && (component_num <= (VIOC_DTRC0 + VIOC_DTRC_MAX))) {
				VIOC_CONFIG_DMAPath_UnSet(scaler->rdma.id);
				tca_dtrc_convter_swreset(VIOC_DTRC1);
			}
			#endif

			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(scaler->wmix.id, scaler->rdma.id);
			#endif
		}

		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);

		scaler->data->block_operating = scaler->data->block_waiting = 0;
		scaler->data->poll_count = scaler->data->cmd_count = 0;
	}

	if (scaler->clk)
		clk_disable_unprepare(scaler->clk);
	dprintk("%s():  Out - release(%d). \n", __func__, scaler->data->dev_opened);

	return 0;
}

static int scaler_drv_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

	int ret = 0;
	dprintk("%s():  In -open(%d), block(%d), wait(%d), cmd(%d), irq(%d :: %d/0x%x) \n", __func__, scaler->data->dev_opened, scaler->data->block_operating, \
			scaler->data->block_waiting, scaler->data->cmd_count, scaler->data->irq_reged, scaler->vioc_intr->id, scaler->vioc_intr->bits);

	if (scaler->clk)
		clk_prepare_enable(scaler->clk);

	if (!scaler->data->irq_reged) {
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);
		#ifdef CONFIG_VIOC_MAP_DECOMP
        tca_map_convter_swreset(VIOC_MC1);
		#endif
		#ifdef CONFIG_VIOC_DTRC_DECOMP
		tca_dtrc_convter_swreset(VIOC_DTRC1);
		#endif
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);

		// VIOC_CONFIG_StopRequest(1);
		synchronize_irq(scaler->irq);
		vioc_intr_clear(scaler->vioc_intr->id, scaler->vioc_intr->bits);
		ret = request_irq(scaler->irq, scaler_drv_handler, IRQF_SHARED, scaler->misc->name, scaler);
		if (ret) {
			if (scaler->clk)
				clk_disable_unprepare(scaler->clk);
			printk("failed(ret %d) to aquire %s request_irq(%d). \n", ret, scaler->misc->name, scaler->irq);
			return -EFAULT;
		}
		dprintk("success(ret %d) to aquire %s request_irq(%d). \n", ret, scaler->misc->name, scaler->irq);
		vioc_intr_enable(scaler->irq, scaler->vioc_intr->id, scaler->vioc_intr->bits);

		if(VIOC_CONFIG_DMAPath_Support()) {
			int component_num = VIOC_CONFIG_DMAPath_Select(scaler->rdma.id);
			if((int)component_num < 0)
				pr_info(" %s  : RDMA :%d dma path selection none\n", __func__, scaler->rdma.id);
			else if(((int)component_num < VIOC_RDMA00) && (component_num > (VIOC_RDMA00 + VIOC_RDMA_MAX)))
				VIOC_CONFIG_DMAPath_UnSet(component_num);

			// It is default path selection(VRDMA)
			VIOC_CONFIG_DMAPath_Set(scaler->rdma.id, scaler->rdma.id);
		} else {
			#ifdef CONFIG_ARCH_TCC803X
			VIOC_CONFIG_MCPath(scaler->wmix.id, scaler->rdma.id);
			#endif
		}

		scaler->data->irq_reged = 1;
	}

	scaler->data->dev_opened++;
	dprintk("%s():  Out - open(%d). \n", __func__, scaler->data->dev_opened);
	return ret;
}

static struct file_operations scaler_drv_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= scaler_drv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= scaler_drv_compat_ioctl,
#endif
	.mmap			= scaler_drv_mmap,
	.open			= scaler_drv_open,
	.release		= scaler_drv_release,
	.poll			= scaler_drv_poll,
};

static int scaler_drv_probe(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler;
	struct device_node *dev_np;
	unsigned int index;
	int ret = -ENODEV;

	scaler = kzalloc(sizeof(struct scaler_drv_type), GFP_KERNEL);
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

	scaler->id = of_alias_get_id(pdev->dev.of_node, "scaler_drv");	

 	/* register scaler discdevice */
	scaler->misc->minor = MISC_DYNAMIC_MINOR;
	scaler->misc->fops = &scaler_drv_fops;
	scaler->misc->name = kasprintf(GFP_KERNEL,"scaler%d", scaler->id);
	scaler->misc->parent = &pdev->dev;

	ret = misc_register(scaler->misc);
	if (ret)
		goto err_misc_register;

	dev_np = of_parse_phandle(pdev->dev.of_node, "rdmas", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "rdmas", 1, &index);
		scaler->rdma.reg = VIOC_RDMA_GetAddress(index);
		scaler->rdma.id = index;
	} else {
		printk("could not find rdma node of %s driver. \n", scaler->misc->name);
		scaler->rdma.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "scalers", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "scalers", 1, &index);
		scaler->sc.reg = VIOC_SC_GetAddress(index);
		scaler->sc.id = index;
	} else {
		printk("could not find scaler node of %s driver. \n", scaler->misc->name);
		scaler->sc.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wmixs", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "wmixs", 1, &index);
		scaler->wmix.reg = VIOC_WMIX_GetAddress(index);
		scaler->wmix.id = index;
	} else {
		printk("could not find wmix node of %s driver. \n", scaler->misc->name);
		scaler->wmix.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wdmas", 0);
	if(dev_np) {
		of_property_read_u32_index(pdev->dev.of_node, "wdmas", 1, &index);
		scaler->wdma.reg = VIOC_WDMA_GetAddress(index);
		scaler->wdma.id = index;
		scaler->irq = irq_of_parse_and_map(dev_np, get_vioc_index(index));
		scaler->vioc_intr->id	= VIOC_INTR_WD0 + get_vioc_index(scaler->wdma.id);
		scaler->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
	} else {
		printk("could not find wdma node of %s driver. \n", scaler->misc->name);
		scaler->wdma.reg = NULL;
	}

	of_property_read_u32(pdev->dev.of_node, "settop_support", &scaler->settop_support);

	spin_lock_init(&(scaler->data->poll_lock));
	spin_lock_init(&(scaler->data->cmd_lock));

	mutex_init(&(scaler->data->io_mutex));
	
	init_waitqueue_head(&(scaler->data->poll_wq));
	init_waitqueue_head(&(scaler->data->cmd_wq));

	platform_set_drvdata(pdev, scaler);

	pr_info("%s: id:%d, Scaler Driver Initialized\n", pdev->name, scaler->id);
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

static int scaler_drv_remove(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler = (struct scaler_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(scaler->misc);
	kfree(scaler->vioc_intr);
	kfree(scaler->data);
	kfree(scaler->info);
	kfree(scaler->misc);
	kfree(scaler);
	return 0;
}

static int scaler_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int scaler_drv_resume(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler = (struct scaler_drv_type *)platform_get_drvdata(pdev);

	if(scaler->data->dev_opened > 0) {
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);
	}

	return 0;
}

static struct of_device_id scaler_of_match[] = {
	{ .compatible = "telechips,scaler_drv" },
	{}
};
MODULE_DEVICE_TABLE(of, scaler_of_match);

static struct platform_driver scaler_driver = {
	.probe		= scaler_drv_probe,
	.remove		= scaler_drv_remove,
	.suspend	= scaler_drv_suspend,
	.resume		= scaler_drv_resume,
	.driver 	= {
		.name	= "scaler_pdev",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(scaler_of_match),
#endif
	},
};

static int __init scaler_drv_init(void)
{
	return platform_driver_register(&scaler_driver);
}

static void __exit scaler_drv_exit(void)
{
	platform_driver_unregister(&scaler_driver);
}

module_init(scaler_drv_init);
module_exit(scaler_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips Scaler Driver");
MODULE_LICENSE("GPL");

