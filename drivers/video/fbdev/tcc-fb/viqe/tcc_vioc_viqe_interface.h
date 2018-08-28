/****************************************************************************
linux/drivers/video/tcc/viqe/tcc_vioc_viqe_interface.h
Description: TCC VIOC h/w block 

Copyright (C) 2013 Telechips Inc.

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

#ifndef TCC_VIOC_VIQE_INTERFACE_H_INCLUDED
#define TCC_VIOC_VIQE_INTERFACE_H_INCLUDED

#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/irq.h>

#include <video/tcc/tcc_viqe_ioctl.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_fb.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_viqe.h>

enum VIQE_RESET_REASON{
	VIQE_RESET_NONE = 0,
	VIQE_RESET_CROP_CHANGED,
	VIQE_RESET_RECOVERY
};

struct tcc_viqe_common_virt_addr_info_t{
	volatile void __iomem *pVIQE0;
	volatile void __iomem *pVIQE1;
	volatile void __iomem *pDEINTLS;
	int gVIOC_Deintls;
	int gVIOC_VIQE0;
	int gVIOC_VIQE1;
	int gBoard_stb;
};

struct tcc_viqe_m2m_virt_addr_info_t{
	volatile void __iomem *pRDMABase_m2m;
	union {
		int gVIQE_RDMA_num_m2m;
		int gDEINTLS_RDMA_num_m2m;
	};
};

struct tcc_viqe_60hz_virt_addr_info_t{
	volatile void __iomem *pRDMABase_60Hz;
	volatile void __iomem *pSCALERBase_60Hz;
	volatile void __iomem *pWMIXBase_60Hz;
	volatile void __iomem *pDISPBase_60Hz;
	int gVIQE_RDMA_num_60Hz;
	int gSCALER_num_60Hz;
};

struct tcc_viqe_m2m_scaler_data {
        unsigned char           irq_reged;
        unsigned int            dev_opened;
};

struct tcc_viqe_m2m_scaler_vioc {
	volatile void __iomem *reg;
	unsigned int id;
	unsigned int path;
};

struct tcc_viqe_m2m_scaler_type_t {
        struct vioc_intr_type   *vioc_intr;

        unsigned int            id;
        unsigned int            irq;

        struct tcc_viqe_m2m_scaler_vioc	*rdma;
        struct tcc_viqe_m2m_scaler_vioc	*wmix;
        struct tcc_viqe_m2m_scaler_vioc	*sc;
        struct tcc_viqe_m2m_scaler_vioc	*wdma;

        struct tcc_viqe_m2m_scaler_data      *data;
        struct tcc_lcdc_image_update             *info;

        unsigned int            settop_support;
};

#ifdef CONFIG_USE_SUB_MULTI_FRAME
struct video_queue_t {
	struct list_head list;
	int type;
};
void TCC_VIQE_DI_Push60Hz_M2M(struct tcc_lcdc_image_update *input_image, int type);
#endif
void TCC_VIQE_DI_PlugInOut_forAlphablending(int plugIn);
void TCC_VIQE_DI_Init(VIQE_DI_TYPE *viqe_arg);
void TCC_VIQE_DI_Run(VIQE_DI_TYPE *viqe_arg);
void TCC_VIQE_DI_DeInit(VIQE_DI_TYPE *viqe_arg);
void TCC_VIQE_DI_GET_SOURCE_INFO(struct tcc_lcdc_image_update* input_image, unsigned int layer);
void TCC_VIQE_DI_Init60Hz_M2M(TCC_OUTPUT_TYPE outputMode, struct tcc_lcdc_image_update *input_image);
void TCC_VIQE_DI_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image, int reset_frmCnt);
void TCC_VIQE_DI_DeInit60Hz_M2M(int layer);
void TCC_VIQE_Scaler_Init60Hz_M2M(struct tcc_lcdc_image_update *input_image);
void TCC_VIQE_Scaler_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image);
void TCC_VIQE_Scaler_Release60Hz_M2M(void);
void TCC_VIQE_DI_Sub_Init60Hz_M2M(TCC_OUTPUT_TYPE outputMode, struct tcc_lcdc_image_update *input_image);
void TCC_VIQE_DI_Sub_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image, int reset_frmCnt);
void TCC_VIQE_DI_Sub_DeInit60Hz_M2M(int layer);
void TCC_VIQE_Scaler_Sub_Init60Hz_M2M(struct tcc_lcdc_image_update *input_image);
void TCC_VIQE_Scaler_Sub_Run60Hz_M2M(struct tcc_lcdc_image_update* input_image);
void TCC_VIQE_Scaler_Sub_Release60Hz_M2M(void);
irqreturn_t TCC_VIQE_Scaler_Handler60Hz_M2M(int irq, void *client_data);
irqreturn_t TCC_VIQE_Scaler_Sub_Handler60Hz_M2M(int irq, void *client_data);
void TCC_VIQE_Display_Update60Hz_M2M(struct tcc_lcdc_image_update *input_image);
void TCC_VIQE_DI_Init60Hz(TCC_OUTPUT_TYPE outputMode, int lcdCtrlNum, struct tcc_lcdc_image_update *input_image);
void TCC_VIQE_DI_Swap60Hz(int mode);
void TCC_VIQE_DI_SetFMT60Hz(int enable);
void TCC_VIQE_DI_Run60Hz(struct tcc_lcdc_image_update *input_image ,int reset_frmCnt);
void TCC_VIQE_DI_DeInit60Hz(void);


#endif
