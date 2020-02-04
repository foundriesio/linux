/*
 * drivers/misc/tcc/tcc_grp.c
 *
 * Copyright (C) 2004 Texas Instruments, Inc. 
 *
 * Video-for-Linux (Version 2) graphic capture driver for
 * the OMAP H2 and H3 camera controller.
 *
 * Adapted from omap24xx driver written by Andy Lowe (source@mvista.com)
 * Copyright (C) 2003-2004 MontaVista Software, Inc.
 * 
 * This package is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. 
 * 
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED 
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *
 * History:
 *   27/03/05   Vladimir Barinov - Added support for power management
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
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
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/uaccess.h>

#include <asm/io.h>
#include <asm/div64.h>

#include "tcc_grp.h"
#include <video/tcc/tcc_gre2d.h>
#include <video/tcc/tcc_gre2d_api.h>
#include <video/tcc/tcc_gre2d_type.h>
#include <video/tcc/tcc_grp_ioctrl.h>

extern G2D_DITHERING_TYPE gG2D_Dithering_type;
extern unsigned char gG2D_Dithering_en;

static int debug  = 0;
#define dprintk(msg...)	if (debug) { printk("[DBG][GRP] " msg); }

#if 0
#define GRP_DBG(msg...)		printk(msg)
#else
#define GRP_DBG(msg...)
#endif

struct g2d_data {
	unsigned int  dev_opened;

	wait_queue_head_t poll_wq;
	wait_queue_head_t cmd_wq;

	spinlock_t g2d_spin_lock;
	struct mutex io_mutex;

	unsigned char block_waiting;
	unsigned char block_operating;

	unsigned char irq_reged;
};

struct g2d_drv_type {
	unsigned int		irq;

	struct miscdevice	*misc;
	volatile void __iomem *reg;

	struct clk		*clk;
	struct g2d_data	*data;
//	SCALER_TYPE		*info;
};

/* YUV FORMAT의 U, V 의 ADDRESS를 구함 */
void grp_get_address(G2D_DATA_FM fmt, unsigned int add, short  src_imgx, short  src_imgy,
							unsigned int* U,unsigned int* V)
{
	if(fmt == GE_YUV420_sp)
	{
		*U = GET_ADDR_YUV42X_spU(add, src_imgx, src_imgy);
		*V = GET_ADDR_YUV420_spV(*U, src_imgx, src_imgy);
	}
	else
	{
		*U = GET_ADDR_YUV42X_spU(add, src_imgx, src_imgy);
		*V = GET_ADDR_YUV422_spV(*U, src_imgx, src_imgy);
	}
}


void g2d_drv_response_check(struct g2d_data	*data, unsigned int responsetype)
{
	int ret;

	if(responsetype == G2D_INTERRUPT) {
		dprintk("g2d_drv_response_check:[%d][%d]\n", responsetype, data->block_waiting);
			// call poll function 
	}
	else if(responsetype == G2D_POLLING) {
		dprintk("g2d_drv_response_check:[%d][%d]\n", responsetype, data->block_waiting);

		ret = wait_event_interruptible_timeout(data->poll_wq,  data->block_operating == 0, msecs_to_jiffies(500));
		if(ret <= 0) {
			pr_warn("[WAR][GRP] G2D time out :%d Line :%d \n", __LINE__, ret);
		}
	}
}

int grp_common_ctrl(struct g2d_data	*data, G2D_COMMON_TYPE *g2d_p)
{
	unsigned int srcY = 0, srcU = 0, srcV = 0;
	unsigned int tgtY = 0, tgtU = 0, tgtV = 0;
	unsigned int result = 0;

	srcY = g2d_p->src0;
	
	if( g2d_p->src1 == 0 || g2d_p->src2  == 0 )
		grp_get_address(g2d_p->srcfm.format, srcY , g2d_p->src_imgx ,g2d_p->src_imgy, &srcU, &srcV);
	else
	{
		srcU = g2d_p->src1;
		srcV = g2d_p->src2;
	}
	
	if(g2d_p->DefaultBuffer)
	{
		return -EFAULT;
	}
	else
	{
		tgtY = g2d_p->tgt0;

		if( g2d_p->tgt1 == 0 || g2d_p->tgt2  == 0 )
			grp_get_address(g2d_p->tgtfm.format, tgtY, g2d_p->dst_imgx, g2d_p->dst_imgy, &tgtU, &tgtV);	
		else
		{
			tgtU = g2d_p->tgt1;
			tgtV = g2d_p->tgt2;
		}
	}

	if((g2d_p->ch_mode == ROTATE_90) || (g2d_p->ch_mode == ROTATE_270))
	{
		if(g2d_p->crop_imgy > (g2d_p->dst_imgx - g2d_p->dst_off_x))
		{
			dprintk("rg2d_p->crop_imgx[%d][%d][%d]!!!\n", g2d_p->crop_imgx, g2d_p->dst_imgx, g2d_p->dst_off_x);
			g2d_p->crop_imgy = g2d_p->dst_imgx - g2d_p->dst_off_x;
		}

		if(g2d_p->crop_imgx > (g2d_p->dst_imgy - g2d_p->dst_off_y))
		{
			dprintk("rg2d_p->crop_imgy[%d][%d][%d]!!!\n", g2d_p->crop_imgy, g2d_p->dst_imgy, g2d_p->dst_off_y);
			g2d_p->crop_imgx = g2d_p->dst_imgy - g2d_p->dst_off_y;
		}
	}
	else
	{
		if(g2d_p->crop_imgx > (g2d_p->dst_imgx - g2d_p->dst_off_x))
		{
			dprintk("g2d_p->crop_imgx[%d][%d][%d]!!!\n", g2d_p->crop_imgx, g2d_p->dst_imgx, g2d_p->dst_off_x);
			g2d_p->crop_imgx = g2d_p->dst_imgx - g2d_p->dst_off_x;
		}
		
		if(g2d_p->crop_imgy > (g2d_p->dst_imgy - g2d_p->dst_off_y))
		{
			dprintk("g2d_p->crop_imgy[%d][%d][%d]!!!\n", g2d_p->crop_imgy, g2d_p->dst_imgy, g2d_p->dst_off_y);
			g2d_p->crop_imgy = g2d_p->dst_imgy - g2d_p->dst_off_y;
		}
	}
	
	gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);

	if((g2d_p->srcfm.format <= GE_YUV422_sq) && (g2d_p->tgtfm.format == GE_RGB565))	
	{
		gG2D_Dithering_en = 1;
		gG2D_Dithering_type = ADD_1_OP;
	}
	else
	{
		gG2D_Dithering_en = 0;
	}

	spin_lock_irq(&(data->g2d_spin_lock));

	gre2d_ImgRotate_Ex(srcY , srcU, srcV, 
							g2d_p->srcfm , g2d_p->src_imgx ,g2d_p->src_imgy,
							g2d_p->crop_offx, g2d_p->crop_offy, g2d_p->crop_imgx, g2d_p->crop_imgy, 
							tgtY,tgtU, tgtV,
							g2d_p->tgtfm, g2d_p->dst_imgx, g2d_p->dst_imgy, 
							g2d_p->dst_off_x, g2d_p->dst_off_y, g2d_p->ch_mode, g2d_p->parallel_ch_mode);


	result = gre2d_interrupt_ctrl(0, (G2D_INT_TYPE)0, 0, 0);
	spin_unlock_irq(&(data->g2d_spin_lock));

	if(gG2D_Dithering_en)	{
		gG2D_Dithering_en = 0;
	}
	
	g2d_drv_response_check(data, g2d_p->responsetype);
	
	return 0;
}


int grp_overlaymixer_ctrl(struct g2d_data *data, G2D_BITBLIT_TYPE_1 *g2d_p)
{

	G2D_ROP_FUNC rop;
	G2D_EN	chEn = GRP_DISABLE;
	unsigned int result = 0;
		
	unsigned int src_addr_comp0 = 0, src_addr_comp1 = 0, src_addr_comp2 = 0;
	unsigned int dest_addr_comp = 0;
	unsigned int src_addr0, src_addr1, src_addr2;
	unsigned int dest_addr;

	src_addr0 = g2d_p->src0;
	src_addr1 = g2d_p->src1;
	src_addr2 = g2d_p->src2;
	
	dest_addr = g2d_p->tgt0;

	GRP_DBG("src_addr0:0x%X, src_img(x:%d, y:%d), crop_img(%d:%d:%d:%d) fmt:%d !!!\n",  
				src_addr0, 
				g2d_p->src0_imgx, g2d_p->src0_imgy, 
				g2d_p->src0_crop_offx, g2d_p->src0_crop_offy,  
				g2d_p->src0_crop_imgx, g2d_p->src0_crop_imgy, 
				g2d_p->src0_fmt);

	GRP_DBG("src_addr1:0x%X, src_img(x:%d, y:%d), crop_img(%d:%d:%d:%d) fmt:%d !!!\n",  
				src_addr1, 
				g2d_p->src1_imgx, g2d_p->src1_imgy, 
				g2d_p->src1_crop_offx, g2d_p->src1_crop_offy,  
				g2d_p->src1_crop_imgx, g2d_p->src1_crop_imgy, 
				g2d_p->src1_fmt);

	GRP_DBG("src_addr2:0x%X, src_img(x:%d, y:%d), crop_img(%d:%d:%d:%d) fmt:%d !!!\n",  
				src_addr2, 
				g2d_p->src2_imgx, g2d_p->src2_imgy, 
				g2d_p->src2_crop_offx, g2d_p->src2_crop_offy,  
				g2d_p->src2_crop_imgx, g2d_p->src2_crop_imgy, 
				g2d_p->src2_fmt);

	GRP_DBG("dst_addr:0x%X, dst_img(x:%d, y:%d), dst_offset(x:%d, y:%d)\n",  
				dest_addr, 
				g2d_p->dst_imgx, g2d_p->dst_imgy,				
				g2d_p->dst_off_x,  g2d_p->dst_off_y);

	GRP_DBG("ch_mode:%d, tgtfm:%d\n", g2d_p->ch_mode, g2d_p->tgtfm);

	src_addr_comp0 = (src_addr0 % 8)/2;
	src_addr_comp1 = (src_addr1 % 8)/2;
	src_addr_comp2 = (src_addr2 % 8)/2;
	
	dest_addr_comp = (dest_addr % 8)/2;
	GRP_DBG("src_addr_comp0:%d, src_addr_comp1:%d, src_addr_comp2:%d, dest_addr_comp:%d\n", \
		src_addr_comp0, src_addr_comp1, src_addr_comp2, dest_addr_comp);

	src_addr0 = (src_addr0>>3)<<3;
	src_addr1 = (src_addr1>>3)<<3;
	src_addr2 = (src_addr2>>3)<<3;
	
	dest_addr = (dest_addr>>3)<<3;
	GRP_DBG("src_addr0:0x%X, src_addr1:0x%X, src_addr2:0x%X, dest_addr:0x%X\n", \
				src_addr0, src_addr1, src_addr2, dest_addr);

	memset(&rop, 0x0, sizeof(G2D_ROP_FUNC));

	rop.src0.add0			= src_addr0;
	rop.src0.frame_pix_sx	= g2d_p->src0_imgx;
	rop.src0.frame_pix_sy	= g2d_p->src0_imgy;
	rop.src0.src_off_sx 	= g2d_p->src0_crop_offx + src_addr_comp0;
	rop.src0.src_off_sy 	= g2d_p->src0_crop_offy;  
	rop.src0.img_pix_sx 	= g2d_p->src0_crop_imgx;
	rop.src0.img_pix_sy 	= g2d_p->src0_crop_imgy;  
	rop.src0.win_off_sx 	= 0;
	rop.src0.win_off_sy 	= 0;  
	rop.src0.op_mode		= NOOP;//g2d_p->ch_mode;
	rop.src0.src_form		= g2d_p->src0_fmt;
	rop.src0.arith_mode 	= AR_NOOP;		// rotation mode setting
	rop.src0.arith_RY		= 0x0;
	rop.src0.arith_GU		= 0x0;
	rop.src0.arith_BV		= 0x0;
#if defined(TCC_OVERLAY_MIXER_CLUT_SUPPORT)
	rop.src0.clut_en		= g2d_p->src0_clut_en;
#endif /* TCC_OVERLAY_MIXER_CLUT_SUPPORT */

	if(rop.src0.add0) chEn = GRP_F0;

	rop.src1.add0			= src_addr1;
	rop.src1.frame_pix_sx	= g2d_p->src1_imgx;
	rop.src1.frame_pix_sy	= g2d_p->src1_imgy;
	rop.src1.src_off_sx 	= g2d_p->src1_crop_offx + dest_addr_comp;
	rop.src1.src_off_sy 	= g2d_p->src1_crop_offy;  
	rop.src1.img_pix_sx 	= g2d_p->src1_crop_imgx;
	rop.src1.img_pix_sy 	= g2d_p->src1_crop_imgy;	
	rop.src1.win_off_sx 	= 0;
	rop.src1.win_off_sy 	= 0;  
	rop.src1.op_mode		= NOOP;
	rop.src1.src_form		= g2d_p->src1_fmt;
	rop.src1.arith_mode 	= AR_NOOP;
	rop.src1.arith_RY		= 0x0;
	rop.src1.arith_GU		= 0x0;
	rop.src1.arith_BV		= 0x0;
#if defined(TCC_OVERLAY_MIXER_CLUT_SUPPORT)
	rop.src1.clut_en		= g2d_p->src1_clut_en;
#endif /* TCC_OVERLAY_MIXER_CLUT_SUPPORT */

	if(rop.src1.add0) chEn = GRP_F0F1;

	rop.src2.add0			= src_addr2;
	rop.src2.frame_pix_sx	= g2d_p->src2_imgx;
	rop.src2.frame_pix_sy	= g2d_p->src2_imgy;
	rop.src2.src_off_sx 	= g2d_p->src2_crop_offx + dest_addr_comp;
	rop.src2.src_off_sy 	= g2d_p->src2_crop_offy;  
	rop.src2.img_pix_sx 	= g2d_p->src2_crop_imgx;
	rop.src2.img_pix_sy 	= g2d_p->src2_crop_imgy;	
	rop.src2.win_off_sx 	= 0;
	rop.src2.win_off_sy 	= 0;  
	rop.src2.op_mode		= NOOP;
	rop.src2.src_form		= g2d_p->src2_fmt;
	rop.src2.arith_mode 	= AR_NOOP;
	rop.src2.arith_RY		= 0x0;
	rop.src2.arith_GU		= 0x0;
	rop.src2.arith_BV		= 0x0;
#if defined(TCC_OVERLAY_MIXER_CLUT_SUPPORT)
	rop.src2.clut_en		= g2d_p->src2_clut_en;
#endif /* TCC_OVERLAY_MIXER_CLUT_SUPPORT */

	if(rop.src2.add0) chEn = GRP_F0F1F2;

	rop.dest.add0			= dest_addr;
	rop.dest.frame_pix_sx	= g2d_p->dst_imgx;
	rop.dest.frame_pix_sy	= g2d_p->dst_imgy;
	rop.dest.dest_off_sx	= g2d_p->dst_off_x + dest_addr_comp;
	rop.dest.dest_off_sy	= g2d_p->dst_off_y;  
	rop.dest.op_mode		= NOOP;
	rop.dest.dest_form		= g2d_p->tgtfm;

	rop.op_ctrl.csel1		= CHROMA_OP1_NOOP;	//operator 1 chroma-key source select 0:disable 1: op0 & S0_chroam 2:op0 & S1_chroma 3:src2 & S2_chroma 
	rop.op_ctrl.csel0		= CHROMA_OP0_NOOP;	//operator 0 chroma-key source select 0:disable 1: src0 & S0_chroam 2:src1 & S1_chroma 3:reseved

	rop.op_ctrl.op_mode0 = (g2d_p->alpha_type == G2d_ALPHA_VALUE)?GE_ROP_ALPHA_0:GE_ROP_ALPHA_1;
	rop.op_ctrl.op_mode1 = (g2d_p->alpha_type == G2d_ALPHA_VALUE)?GE_ROP_ALPHA_0:GE_ROP_ALPHA_1;		
	
	rop.op_pat_0.op_alpha = g2d_p->alpha_value;
	rop.op_pat_1.op_alpha = g2d_p->alpha_value;

	if(chEn == GRP_F0){
		rop.op_ctrl.op_mode0 = GE_ROP_SRC_COPY;
		rop.op_ctrl.op_mode1 = GE_ROP_SRC_COPY;
	}
	else if(chEn == GRP_F0F1){
		/* In case of 2Ch mix(=GRP_F0F1), op1 must be set as GE_ROP_SRC_COPY */
		rop.op_ctrl.op_mode1 = GE_ROP_SRC_COPY;
	}

	GRP_DBG("op_mode0:%d, op_mode1:%d\n", rop.op_ctrl.op_mode0, rop.op_ctrl.op_mode1);
	GRP_DBG("en_channel:%d\n",chEn);
	
	spin_lock_irq(&(data->g2d_spin_lock));
	gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);
	
	gre2d_ImgROP(rop, chEn);
	
	result = gre2d_interrupt_ctrl(0,(G2D_INT_TYPE)0,0,0);

	spin_unlock_irq(&(data->g2d_spin_lock));

	g2d_drv_response_check(data, g2d_p->responsetype);
	
	return result;
}

#define G2D_MAX_SIZE	4080
extern void Gre2d_SetBCh_address(G2D_CHANNEL ch, unsigned int add0, unsigned int add1, unsigned int add2);
int grp_alphablending_value_set(struct g2d_data *data, G2D_ARITH_OP_TYPE *g2d_p)
{
	unsigned int i = 0, loop_cnt = 0, remainder = 0;

	dprintk("%s!!!\n", __func__);
	dprintk("SRC:[0x%x][0x%x][0x%x]- DEST:[0x%x][0x%x][0x%x]\n",
		g2d_p->src0, g2d_p->src1, g2d_p->src2, g2d_p->tgt0, g2d_p->tgt1, g2d_p->tgt2);

	dprintk("SRC:[%d][%d]- DEST:[%d][%d] OFFSET:[%d][%d]!!!\n",
		g2d_p->src_imgx, g2d_p->src_imgy, g2d_p->dst_imgx, g2d_p->dst_imgy, g2d_p->dst_off_x, g2d_p->dst_off_y);

	loop_cnt = (g2d_p->dst_imgy / G2D_MAX_SIZE);
	
	remainder = g2d_p->dst_imgy % G2D_MAX_SIZE; 
	
	dprintk("loop_cnt:%d  remainder:%d!!!\n", loop_cnt, remainder);

	if(g2d_p->src_imgy > G2D_MAX_SIZE)
		g2d_p->src_imgy = G2D_MAX_SIZE;
	
	for(i =0; i < loop_cnt; i++)
	{
		gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);
		if(i == 0)
		{
			gre2d_ImgArithmetic(g2d_p->src0 ,g2d_p->src1, g2d_p->src2, 
								g2d_p->srcfm, g2d_p->src_imgx, g2d_p->src_imgy,
								g2d_p->tgt0 + (G2D_MAX_SIZE * 4 * i), g2d_p->tgt1, g2d_p->tgt2, g2d_p->tgtfm,
								g2d_p->dst_imgx, g2d_p->dst_imgy, g2d_p->dst_off_x, g2d_p->dst_off_y,
								g2d_p->arith, g2d_p->R, g2d_p->G, g2d_p->B);

		}
		else
		{
			//destination address setting
			GRE_2D_SetBChAddress(DEST_CH,  g2d_p->tgt0 + (G2D_MAX_SIZE* 4 * i), 0, 0);

			gre2d_waiting_result(GRP_F0);
		}

		g2d_drv_response_check(data, g2d_p->responsetype);
	}

	if(remainder)
	{
		dprintk("remainder:%d  addr:0x%x!!!\n", remainder, g2d_p->tgt0 + (G2D_MAX_SIZE * 4 * loop_cnt));

		gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);

		gre2d_ImgArithmetic(g2d_p->src0 ,g2d_p->src1, g2d_p->src2, 
							g2d_p->srcfm, g2d_p->src_imgx, g2d_p->src_imgy,
							g2d_p->tgt0 + (G2D_MAX_SIZE * 4 * loop_cnt), g2d_p->tgt1, g2d_p->tgt2, g2d_p->tgtfm,
							g2d_p->dst_imgx, g2d_p->dst_imgy, g2d_p->dst_off_x, g2d_p->dst_off_y,
							g2d_p->arith, g2d_p->R, g2d_p->G, g2d_p->B);

		g2d_drv_response_check(data, g2d_p->responsetype);
	}
	return 0;
}

int grp_arithmeic_operation_ctrl(struct g2d_data *data, G2D_ARITH_OP_TYPE *g2d_p)
{
	unsigned int result = 0;
	

	dprintk("%s!!!\n", __func__);
	dprintk("SRC:[0x%x][0x%x][0x%x]- DEST:[0x%x][0x%x][0x%x]\n",
		g2d_p->src0, g2d_p->src1, g2d_p->src2, g2d_p->tgt0, g2d_p->tgt1, g2d_p->tgt2);

	dprintk("SRC:[%d][%d]- DEST:[%d][%d] OFFSET:[%d][%d]!!!\n",
		g2d_p->src_imgx, g2d_p->src_imgy, g2d_p->dst_imgx, g2d_p->dst_imgy, g2d_p->dst_off_x, g2d_p->dst_off_y);

	spin_lock_irq(&(data->g2d_spin_lock));

	gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);
	gre2d_ImgArithmetic(g2d_p->src0 ,g2d_p->src1, g2d_p->src2, 
						g2d_p->srcfm, g2d_p->src_imgx, g2d_p->src_imgy,
						g2d_p->tgt0 , g2d_p->tgt1, g2d_p->tgt2, g2d_p->tgtfm,
						g2d_p->dst_imgx, g2d_p->dst_imgy, g2d_p->dst_off_x, g2d_p->dst_off_y,
						g2d_p->arith, g2d_p->R, g2d_p->G, g2d_p->B);


	result = gre2d_interrupt_ctrl(0,(G2D_INT_TYPE)0,0,0);
	spin_unlock_irq(&(data->g2d_spin_lock));
	
	g2d_drv_response_check(data, g2d_p->responsetype);

	return 0;
}

void grp_rotate_ctrl(struct g2d_data *data, G2D_BITBLIT_TYPE *g2d_p)
{
	int result;
	unsigned int src_addr_comp = 0, dest_addr_comp = 0;
	unsigned int src_addr, dest_addr;

	src_addr = g2d_p->src0;
	dest_addr = g2d_p->tgt0;

	src_addr_comp = (src_addr % 8)/2;
	dest_addr_comp = (dest_addr % 8)/2;

	src_addr = (src_addr>>3)<<3;
	dest_addr = (dest_addr>>3)<<3;

	dprintk("%s  add:0x%x w:%d h:%d sx:%d sy:%d !!!\n",  __func__,src_addr, g2d_p->src_imgx, g2d_p->src_imgy, g2d_p->crop_imgx, g2d_p->crop_imgy);
	dprintk("%s  0x%x %d %d	%d %d !! %d %d  !\n", __func__,dest_addr, g2d_p->dst_imgx, g2d_p->dst_imgy, g2d_p->crop_offx, g2d_p->crop_offy,  g2d_p->dst_off_x,  g2d_p->dst_off_y);

	spin_lock_irq(&(data->g2d_spin_lock));

	gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);	
	gre2d_ImgRotate_Ex(src_addr, 0, 0, 
					g2d_p->srcfm ,
					g2d_p->src_imgx ,g2d_p->src_imgy,
					g2d_p->crop_offx + src_addr_comp, g2d_p->crop_offy, 
					g2d_p->crop_imgx, g2d_p->crop_imgy, 
					dest_addr,0, 0,
					g2d_p->tgtfm,
					g2d_p->dst_imgx, g2d_p->dst_imgy, 
					g2d_p->dst_off_x + dest_addr_comp, g2d_p->dst_off_y, 
					g2d_p->ch_mode, 0);
	
	result = gre2d_interrupt_ctrl(0,(G2D_INT_TYPE)0,0,0);
	spin_unlock_irq(&(data->g2d_spin_lock));
	
	g2d_drv_response_check(data, g2d_p->responsetype);
}

#if defined(TCC_OVERLAY_MIXER_CLUT_SUPPORT)
void grp2d_overlaymixer_clut_ctrl(G2D_CLUT_TYPE *pClut)
{
	int i;

	if(pClut->clut_type == CLUT_DATA_ALL)
	{
		for(i=0; i<0xFF; i++)
		{
			GRE_2D_ClutCtrl(pClut->clut.all.ch, i, pClut->clut.all.data[i]);
		}
	}
	else{
		GRE_2D_ClutCtrl(pClut->clut.one.ch, pClut->clut.one.index, pClut->clut.one.data);
	}
}
#endif /* TCC_OVERLAY_MIXER_CLUT_SUPPORT */

extern int range_is_allowed(unsigned long pfn, unsigned long size);
static int g2d_drv_mmap(struct file *file, struct vm_area_struct *vma)
{
	if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0){
		pr_err("[ERR][GRP] this address is not allowed \n");
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
	{
		return -EAGAIN;
	}

	vma->vm_ops		= NULL;
	vma->vm_flags 	|= VM_IO;
	vma->vm_flags 	|= VM_DONTEXPAND | VM_PFNMAP;
	
	return 0;
}


static unsigned int g2d_drv_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct g2d_drv_type	*g2d = dev_get_drvdata(misc->parent);
	int ret = 0;

	if(g2d->data == NULL)
		return 0;
	
	dprintk(" g2d_drv_poll !!!\n");

	poll_wait(filp, &(g2d->data->poll_wq), wait);
	
	spin_lock_irq(&(g2d->data->g2d_spin_lock));

	if (g2d->data->block_operating == 0) 	{
		ret =  (POLLIN | POLLRDNORM);
	}
	spin_unlock_irq(&(g2d->data->g2d_spin_lock));

	return ret;
}

// interrupt handler
static irqreturn_t g2d_drv_handler(int irq, void *client_data)
{  	
	struct g2d_drv_type *g2d = (struct g2d_drv_type *)client_data;
	G2D_INT_TYPE IFlag;

	IFlag = gre2d_interrupt_ctrl(0,(G2D_INT_TYPE)0,0,0);	//GE_IREQ

	dprintk(" g2d_drv_handler irq[%d] %d block[%d]!!!\n", IFlag, g2d->data->block_waiting, g2d->data->block_operating);
	
	if(IFlag & G2D_INT_R_FLG)
	{
		IFlag = gre2d_interrupt_ctrl(1,(G2D_INT_TYPE)(G2D_INT_R_IRQ|G2D_INT_R_FLG), 0, TRUE);
		gre2d_set_dma_interrupt(SET_G2D_DMA_INT_DISABLE);

		wake_up_interruptible(&g2d->data->poll_wq);

		if(g2d->data->block_operating >= 1) {
			g2d->data->block_operating = 0;
		}

		if(g2d->data->block_waiting)
			wake_up_interruptible(&g2d->data->cmd_wq);
	}	

	return IRQ_HANDLED;
}


static long g2d_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct g2d_drv_type	*g2d = dev_get_drvdata(misc->parent);
	int ret = 0;

	dprintk(" g2d_drv_ioctl IOCTL [%d] [%d]!!!\n", cmd, g2d->data->block_operating);

	mutex_lock(&g2d->data->io_mutex);

	switch(cmd)
	{
		case TCC_GRP_COMMON_IOCTRL:
		case TCC_GRP_COMMON_IOCTRL_KERNEL:
			{
				G2D_COMMON_TYPE	*g2d_p = (G2D_COMMON_TYPE *)kmalloc(sizeof(G2D_COMMON_TYPE), GFP_KERNEL);

				if(cmd == TCC_GRP_COMMON_IOCTRL) {
					if(copy_from_user((void *)(g2d_p), (void __user *)arg, sizeof(G2D_COMMON_TYPE))){
						kfree((const void*)g2d_p);
						ret = -EFAULT;
						goto Ioctl_Error;
					}
				}
				else {
					memcpy(g2d_p, (G2D_COMMON_TYPE*)arg ,sizeof(G2D_COMMON_TYPE));
				}

				if(g2d->data->block_operating >= 1)	 {
					g2d->data->block_waiting = 1;
					ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line ret:%d block:%d \n", __LINE__, ret, g2d->data->block_operating);
						g2d->data->block_operating = 0;
					}
				}

				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				g2d->data->block_waiting = 0;
				g2d->data->block_operating++;
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));
				
				grp_common_ctrl(g2d->data, (G2D_COMMON_TYPE *)g2d_p);
				
				kfree((const void*)g2d_p);
			}
			break;

		case TCC_GRP_ARITH_IOCTRL:
		case TCC_GRP_ARITH_IOCTRL_KERNEL:			
			{
				G2D_ARITH_OP_TYPE g2d_arith;

				if(cmd == TCC_GRP_ARITH_IOCTRL) {
					if(copy_from_user((void *)(&g2d_arith), (void __user *)arg, sizeof(g2d_arith))){
						ret = -EFAULT;
						goto Ioctl_Error;
					}
				}
				else {
					memcpy(&g2d_arith, (G2D_ARITH_OP_TYPE*)arg ,sizeof(g2d_arith));
				}

				if(g2d->data->block_operating >= 1)			{
					g2d->data->block_waiting = 1;					
					ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line :%d block:%d \n", __LINE__, ret, g2d->data->block_operating);
						g2d->data->block_operating = 0;
					}
				}

				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				g2d->data->block_waiting = 0;				
				g2d->data->block_operating++;
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));
				
				grp_arithmeic_operation_ctrl(g2d->data, (G2D_ARITH_OP_TYPE *)&g2d_arith);
			}
			break;

		case TCC_GRP_ALPHA_VALUE_SET_IOCTRL:
		case TCC_GRP_ALPHA_VALUE_SET_IOCTRL_KERNEL:			
			{
				G2D_ARITH_OP_TYPE g2d_arith;

				if(cmd == TCC_GRP_ALPHA_VALUE_SET_IOCTRL) {
					if(copy_from_user((void *)(&g2d_arith), (void __user *)arg, sizeof(g2d_arith))){
						ret = -EFAULT;
						goto Ioctl_Error;
					}
				}
				else {
					memcpy(&g2d_arith, (G2D_ARITH_OP_TYPE*)arg ,sizeof(g2d_arith));
				}

				if(g2d->data->block_operating >= 1)			{
					g2d->data->block_waiting = 1;
					ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line :%d block:%d \n", __LINE__, ret, g2d->data->block_operating);
						g2d->data->block_operating = 0;
					}
				}

				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				g2d->data->block_waiting = 0;
				g2d->data->block_operating++;
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));

				grp_alphablending_value_set(g2d->data, (G2D_ARITH_OP_TYPE *)&g2d_arith);
			}
			break;

		case TCC_OVERLAY_MIXER_IOCTRL:
		case TCC_OVERLAY_MIXER_IOCTRL_KERNEL:
			{
				G2D_BITBLIT_TYPE_1	*g2d_p1 = (G2D_BITBLIT_TYPE_1 *)kmalloc(sizeof(G2D_BITBLIT_TYPE_1), GFP_KERNEL);

				if(cmd == TCC_OVERLAY_MIXER_IOCTRL) {
					if(copy_from_user((void *)(g2d_p1), (void __user *)arg, sizeof(G2D_BITBLIT_TYPE_1))) {
						kfree((const void*)g2d_p1);
						ret = -EFAULT;
						goto Ioctl_Error;
					}
				}
				else {
					memcpy(g2d_p1, (G2D_BITBLIT_TYPE_1*)arg ,sizeof(G2D_BITBLIT_TYPE_1));
				}

				if(g2d->data->block_operating >= 1)	{
					g2d->data->block_waiting = 1;					
					ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line :%d block:%d \n", __LINE__, ret, g2d->data->block_operating);
						g2d->data->block_operating = 0;
					}
				}
				
				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				g2d->data->block_waiting = 0;
				g2d->data->block_operating++;
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));

				grp_overlaymixer_ctrl(g2d->data, g2d_p1);

				kfree((const void*)g2d_p1);
			}
			break;
			
		case TCC_GRP_ROTATE_IOCTRL:
		case TCC_GRP_ROTATE_IOCTRL_KERNEL:
			{
				G2D_BITBLIT_TYPE	g2d_p1;

				if(cmd == TCC_GRP_ROTATE_IOCTRL) {
					if(copy_from_user((void *)(&g2d_p1), (void __user *)arg, sizeof(g2d_p1))){
						ret = -EFAULT;
						goto Ioctl_Error;
					}
				}
				else {
					memcpy(&g2d_p1, (G2D_BITBLIT_TYPE*)arg ,sizeof(g2d_p1));
				}

				if(g2d->data->block_operating >= 1)	{
					g2d->data->block_waiting = 1;					
					ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line :%d block:%d \n", __LINE__, ret, g2d->data->block_operating);
						g2d->data->block_operating = 0;
					}
				}
				
				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				g2d->data->block_waiting = 0;
				g2d->data->block_operating++;
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));

				grp_rotate_ctrl(g2d->data, &g2d_p1);
			}
			break;

	#if defined(TCC_OVERLAY_MIXER_CLUT_SUPPORT)
		case TCC_OVERLAY_MIXER_CLUT_IOCTRL:
			{
				G2D_CLUT_TYPE *p_g2d_clut;
				p_g2d_clut = (G2D_CLUT_TYPE *)kmalloc(PAGE_SIZE, GFP_KERNEL);
				if(!p_g2d_clut) {
					WARN_ON(1);
					return -ENOMEM;
				}

				if(copy_from_user((void *)(p_g2d_clut), (void __user *)arg, sizeof(G2D_CLUT_TYPE))) {
					kfree(p_g2d_clut);
					ret = -EFAULT;
					goto Ioctl_Error;
				}

				grp2d_overlaymixer_clut_ctrl(p_g2d_clut);
				kfree(p_g2d_clut);
			}
			break;
	#endif /* TCC_OVERLAY_MIXER_CLUT_SUPPORT */

		case TCC_OVERLAY_IOCTRL:
		case TCC_OVERLAY_IOCTRL_KERNEL:
			{
				G2D_OVERY_FUNC	g2d_p1;

				if(cmd == TCC_OVERLAY_IOCTRL) {
					if(copy_from_user((void *)(&g2d_p1), (void __user *)arg, sizeof(g2d_p1))){
						ret = -EFAULT;
						goto Ioctl_Error;
					}
				}
				else {
					memcpy(&g2d_p1, (G2D_OVERY_FUNC*)arg ,sizeof(g2d_p1));
				}

				if(g2d->data->block_operating >= 1)	{
					g2d->data->block_waiting = 1;
					ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0, msecs_to_jiffies(200));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line :%d block:%d \n", __LINE__, ret, g2d->data->block_operating);
						g2d->data->block_operating = 0;
					}
				}

				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				g2d->data->block_waiting = 0;
				g2d->data->block_operating++;
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));

				spin_lock_irq(&(g2d->data->g2d_spin_lock));
				gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);
				gre2d_ImgOverlay(&g2d_p1);
				spin_unlock_irq(&(g2d->data->g2d_spin_lock));

				if((g2d_p1.responsetype == G2D_INTERRUPT) || (g2d_p1.responsetype == G2D_POLLING))	{
					ret = wait_event_interruptible_timeout(g2d->data->poll_wq,  g2d->data->block_operating == 0, msecs_to_jiffies(500));
					if(ret <= 0){
						pr_warn("[WAR][GRP] G2D time out :%d Line :%d \n", __LINE__, ret);
					}
				}
			}
			break;

		default:
			dprintk(" Unsupported IOCTL!!!\n");      
			ret = -EFAULT;
			goto Ioctl_Error;
			break;

	}

Ioctl_Error:
	mutex_unlock(&g2d->data->io_mutex);

	return ret;
}

EXPORT_SYMBOL(g2d_drv_ioctl);


int g2d_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct g2d_drv_type	*g2d = dev_get_drvdata(misc->parent);
	int ret = 0;

	mutex_lock(&g2d->data->io_mutex);

	if (g2d->data->dev_opened > 0) {
		g2d->data->dev_opened--;
	}

	if(g2d->data->dev_opened == 0)
	{
		if (g2d->data->irq_reged) {
			free_irq(g2d->irq, g2d);
			g2d->data->irq_reged = 0;
		}
	}
	if (g2d->clk)
		clk_disable_unprepare(g2d->clk);

	mutex_unlock(&g2d->data->io_mutex);
	
	dprintk("%s open_cnt :%d \n",__func__, g2d->data->dev_opened);

	return ret;
}
EXPORT_SYMBOL(g2d_drv_release);

int g2d_drv_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct g2d_drv_type	*g2d = dev_get_drvdata(misc->parent);
	int ret = 0;
	
	mutex_lock(&g2d->data->io_mutex);

	if (g2d->clk)
		clk_prepare_enable(g2d->clk);

	if (!g2d->data->irq_reged) {
		ret = request_irq(g2d->irq, g2d_drv_handler, IRQF_SHARED, g2d->misc->name, g2d);
		if (ret) {
			if (g2d->clk)
				clk_disable_unprepare(g2d->clk);
			pr_err("[ERR][GRP] aquire %s request_irq. \n", g2d->misc->name);
			ret = -EFAULT;
			goto Open_Error;
		}
		g2d->data->irq_reged = 1;
	}

	g2d->data->dev_opened++;

	dprintk("%s open_cnt :%d \n",__func__, g2d->data->dev_opened);

Open_Error:
	mutex_unlock(&g2d->data->io_mutex);
	
	return ret;
}
EXPORT_SYMBOL(g2d_drv_open);

static struct file_operations g2d_drv_fops = 
{
	.owner			= THIS_MODULE,
	.poll 			= g2d_drv_poll,
	.unlocked_ioctl	= g2d_drv_ioctl,
	.mmap			= g2d_drv_mmap,
	.open			= g2d_drv_open,
	.release		= g2d_drv_release,
};

static int g2d_drv_remove(struct platform_device *pdev)
{
	struct g2d_drv_type *g2d = (struct g2d_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(g2d->misc);
	kfree(g2d->data);
	kfree(g2d->misc);
	kfree(g2d);
	return 0;
}

static int g2d_drv_probe(struct platform_device *pdev)
{
	struct g2d_drv_type *g2d;
	int ret = -ENODEV;

	g2d = kzalloc(sizeof(struct g2d_drv_type), GFP_KERNEL);
	if (!g2d)
		return -ENOMEM;

	g2d->clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(g2d->clk))
		g2d->clk = NULL;

	g2d->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (g2d->misc == 0)
		goto err_misc_alloc;

	g2d->data = kzalloc(sizeof(struct g2d_data), GFP_KERNEL);
	if (g2d->data == 0)
		goto err_data_alloc;

 	/* register g2d discdevice */
	g2d->misc->minor = MISC_DYNAMIC_MINOR;
	g2d->misc->fops = &g2d_drv_fops;
	g2d->misc->name = "g2d";
	g2d->misc->parent = &pdev->dev;
	ret = misc_register(g2d->misc);
	if (ret)
		goto err_misc_register;

        g2d->irq = platform_get_irq(pdev, 0);

        dprintk("%s: irq: %d \n", g2d->misc->name, g2d->irq);

	spin_lock_init(&(g2d->data->g2d_spin_lock));
	mutex_init(&g2d->data->io_mutex);

	init_waitqueue_head(&g2d->data->poll_wq);
	init_waitqueue_head(&g2d->data->cmd_wq);

	platform_set_drvdata(pdev, g2d);

	g2d->data->block_waiting = 0;
	g2d->data->block_operating = 0;

        dprintk("%s: G2D Driver Initialized\n", __func__);
        return 0;

	misc_deregister(g2d->misc);

err_misc_register:
	kfree(g2d->data);
err_data_alloc:
	kfree(g2d->misc);
err_misc_alloc:
	kfree(g2d);

	pr_err("[ERR][GRP] %s: %s: err ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int g2d_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int g2d_drv_resume(struct platform_device *pdev)
{
	// TODO:
	return 0;
}

static struct of_device_id g2d_of_match[] = {
	{ .compatible = "telechips,graphic.2d" },
	{}
};
MODULE_DEVICE_TABLE(of, g2d_of_match);

static struct platform_driver g2d_driver = {
	.probe		= g2d_drv_probe,
	.remove		= g2d_drv_remove,
	.suspend	= g2d_drv_suspend,
	.resume		= g2d_drv_resume,
	.driver 	= {
		.name	= "g2d",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(g2d_of_match),
#endif
	},
};

static int __init g2d_drv_init(void)
{
	return platform_driver_register(&g2d_driver);
}

static void __exit g2d_drv_exit(void)
{
	platform_driver_unregister(&g2d_driver);
}

module_init(g2d_drv_init);
module_exit(g2d_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC Video for Linux g2d driver");
MODULE_LICENSE("GPL");

