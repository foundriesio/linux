/****************************************************************************
 * Copyright (C) 2015 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <asm/io.h>
#include <linux/of_irq.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>
#endif

static int vioc_base_irq_num[4] = {0,};

int vioc_intr_enable(int irq, int id, unsigned mask)
{

	volatile void __iomem *reg;
	unsigned int sub_id;
	unsigned int type_clr_offset;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return -1;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_DEV2:
#endif
		sub_id = id - VIOC_INTR_DEV0;
		reg = VIOC_DISP_GetAddress(sub_id);
		__raw_writel((__raw_readl(reg + DIM) &
			      ~(mask & VIOC_DISP_INT_MASK)),
			     reg + DIM);
		break;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	case VIOC_INTR_V_DV:
	{
		volatile void __iomem *pDV_Cfg =
			VIOC_DV_VEDR_GetAddress(VDV_CFG);
		VIOC_V_DV_SetInterruptEnable(pDV_Cfg,
					     (mask & VIOC_V_DV_INT_MASK), 1);

		//dprintk_dv_sequence("### V_DV INT On \n");
	}
	break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
		/* clera irq status */
		sub_id = id - VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(sub_id) + RDMASTAT;
		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_RDMA_GetAddress(sub_id) +RDMAIRQMSK;
		__raw_writel(__raw_readl(reg) & ~(mask & VIOC_RDMA_INT_MASK),
				reg);
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		sub_id = id - VIOC_INTR_WD0;

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress(sub_id) +
		      WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress(sub_id) +
		      WDMAIRQMSK_OFFSET;
		__raw_writel(__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK),
			     reg);
		break;
#ifdef CONFIG_ARCH_TCC803X
    case VIOC_INTR_WD9:
    case VIOC_INTR_WD10:
    case VIOC_INTR_WD11:
    case VIOC_INTR_WD12:
        sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD0;

        /* clera irq status */
        reg = VIOC_WDMA_GetAddress(sub_id) +
              WDMAIRQSTS_OFFSET;
        __raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

        /* enable irq */
        reg = VIOC_WDMA_GetAddress(sub_id) +
              WDMAIRQMSK_OFFSET;
        __raw_writel(__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK),
                 reg);
        break;
#endif

	}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
	if( irq == vioc_base_irq_num[0] )
		type_clr_offset = IRQMASKCLR0_0_OFFSET;
	else if( irq == vioc_base_irq_num[1] )
		type_clr_offset = IRQMASKCLR1_0_OFFSET;
	else if( irq == vioc_base_irq_num[2] )
		type_clr_offset = IRQMASKCLR2_0_OFFSET;
	else if( irq == vioc_base_irq_num[3] )
		type_clr_offset = IRQMASKCLR3_0_OFFSET;
	else {
		pr_err("%s-%d :: irq(%d) is wierd. \n", __func__, __LINE__, irq);
		return -1;
	}
#else
	type_clr_offset = IRQMASKCLR0_0_OFFSET;
#endif

	reg = VIOC_IREQConfig_GetAddress();
#if defined(CONFIG_ARCH_TCC897X)
	if (id < 32)
		__raw_writel(1 << id, reg + type_clr_offset);
	else
		__raw_writel(1 << (id - 32), reg + type_clr_offset + 0x4);
#else
	if (id >= 64)
		__raw_writel(1 << (id - 64), reg + type_clr_offset + 0x8);
	else if (id >= 32 && id < 64)
		__raw_writel(1 << (id - 32), reg + type_clr_offset + 0x4);
	else
		__raw_writel(1 << id, reg + type_clr_offset);
#endif

	return 0;
}

int vioc_intr_disable(int irq, int id, unsigned mask)
{
	volatile void __iomem *reg;
	unsigned int sub_id;
	unsigned do_irq_mask = 1;
	unsigned int type_set_offset;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return -1;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_DEV2:
#endif
		sub_id = id - VIOC_INTR_DEV0;
		reg = VIOC_DISP_GetAddress(sub_id) + DIM;
		__raw_writel(__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK),
			     reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK) !=
		    VIOC_DISP_INT_MASK)
			do_irq_mask = 0;
		break;
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	case VIOC_INTR_V_DV:
	{
		volatile void __iomem *pDV_Cfg =
			VIOC_DV_VEDR_GetAddress(VDV_CFG);
		VIOC_V_DV_SetInterruptEnable(pDV_Cfg,
					     (mask & VIOC_V_DV_INT_MASK), 0);
		if (!(__raw_readl(pDV_Cfg) & (mask & VIOC_V_DV_INT_MASK)))
			do_irq_mask = 0;
		//dprintk_dv_sequence("### V_DV INT Off \n");
	}
	break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
		sub_id = id - VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(sub_id) + RDMAIRQMSK;
		__raw_writel(__raw_readl(reg) | (mask & VIOC_RDMA_INT_MASK),
				reg);
		if((__raw_readl(reg) & VIOC_RDMA_INT_MASK) !=
				VIOC_RDMA_INT_MASK)
			do_irq_mask = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		sub_id = id - VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(sub_id) +
		      WDMAIRQMSK_OFFSET;
		__raw_writel(__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK),
			     reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK) !=
		    VIOC_WDMA_INT_MASK)
			do_irq_mask = 0;
		break;
#ifdef CONFIG_ARCH_TCC803X
    case VIOC_INTR_WD9:
    case VIOC_INTR_WD10:
    case VIOC_INTR_WD11:
    case VIOC_INTR_WD12:
        sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD0;
        reg = VIOC_WDMA_GetAddress(sub_id) +
              WDMAIRQMSK_OFFSET;
        __raw_writel(__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK),
                 reg);
        if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK) !=
            VIOC_WDMA_INT_MASK)
            do_irq_mask = 0;
        break;
#endif
	}

	if (do_irq_mask) {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		if( irq == vioc_base_irq_num[0] )
			type_set_offset = IRQMASKSET0_0_OFFSET;
		else if( irq == vioc_base_irq_num[1] )
			type_set_offset = IRQMASKSET1_0_OFFSET;
		else if( irq == vioc_base_irq_num[2] )
			type_set_offset = IRQMASKSET2_0_OFFSET;
		else if( irq == vioc_base_irq_num[3] )
			type_set_offset = IRQMASKSET3_0_OFFSET;
		else {
			pr_err("%s-%d :: irq(%d) is wierd. \n", __func__, __LINE__, irq);
			return -1;
		}
#else
		type_set_offset = IRQMASKSET0_0_OFFSET;
#endif

		reg = VIOC_IREQConfig_GetAddress();
#if defined(CONFIG_ARCH_TCC897X)
		if (id < 32)
			__raw_writel(1 << id, reg + type_set_offset);
		else
			__raw_writel(1 << (id - 32), reg + type_set_offset + 0x4);
#else
		if (id >= 64)
			__raw_writel(1 << (id - 64), reg + type_set_offset + 0x8);
		else if (id >= 32 && id < 64)
			__raw_writel(1 << (id - 32), reg + type_set_offset + 0x4);
		else
			__raw_writel(1 << id, reg + type_set_offset);
#endif
	}

	return 0;
}

unsigned int vioc_intr_get_status(int id)
{
	volatile void __iomem *reg;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return 0;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_DEV2:
#endif
		reg = VIOC_DISP_GetAddress(id) + DSTATUS;
		return (__raw_readl(reg) & VIOC_DISP_INT_MASK);
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(id) + RDMASTAT;
		return (__raw_readl(reg) & VIOC_RDMA_INT_MASK);
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		return (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
#ifdef CONFIG_ARCH_TCC803X
    case VIOC_INTR_WD9:
    case VIOC_INTR_WD10:
    case VIOC_INTR_WD11:
    case VIOC_INTR_WD12:
        id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
        reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
        return (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
#endif
	}
	return 0;
}

bool check_vioc_irq_status(volatile void __iomem *reg, int id)
{
	unsigned flag;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return false;

	if (id < 32) {
		flag = (__raw_readl(reg + IRQSELECT0_0_OFFSET) & (1 << id))
			       ? (__raw_readl(reg + SYNCSTATUS0_OFFSET) & (1 << id))
			       : (__raw_readl(reg + RAWSTATUS0_OFFSET) & (1 << id));
	} 
    else if (id >= 32 && id < 64) {
		flag = (__raw_readl(reg + IRQMASKCLR0_1_OFFSET) &
			(1 << (id - 32)))
			       ? (__raw_readl(reg + SYNCSTATUS1_OFFSET) &
				  (1 << (id - 32)))
			       : (__raw_readl(reg + RAWSTATUS1_OFFSET) &
				  (1 << (id - 32)));
    }
#if !defined(CONFIG_ARCH_TCC897X)
    else {
		flag = (__raw_readl(reg + IRQMASKCLR0_2_OFFSET) &
			(1 << (id - 64)))
			       ? (__raw_readl(reg + SYNCSTATUS2_OFFSET) &
				  (1 << (id - 64)))
			       : (__raw_readl(reg + RAWSTATUS2_OFFSET) &
				  (1 << (id - 64)));        
	}
#endif

	if (flag)
		return true;
	return false;
}

bool is_vioc_intr_activatied(int id, unsigned mask)
{
	volatile void __iomem *reg;
	if ((id < 0) || (id > VIOC_INTR_NUM))
		return false;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_DEV2:
#endif
		reg = VIOC_DISP_GetAddress(id);
		if (__raw_readl(reg + DSTATUS) &
		    (mask & VIOC_DISP_INT_MASK))
			return true;
		return false;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(id) + RDMASTAT;
		if(__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK))
			return true;
		return false;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return true;
		return false;
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
    case VIOC_INTR_WD12:
        id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return true;
		return false;
#endif
	}
	return false;
}

bool is_vioc_intr_unmasked(int id, unsigned mask)
{
	volatile void __iomem *reg;
	if ((id < 0) || (id > VIOC_INTR_NUM))
		return false;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_DEV2:
#endif
		reg = VIOC_DISP_GetAddress(id);
		if (__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK))
			return true;
		return false;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(id) + RDMAIRQMSK;
		if(__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK))
			return false;
		return true;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQMSK_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return false;
		return true;
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
    case VIOC_INTR_WD12:
        id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQMSK_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return false;
		return true;
#endif
	}
	return false;
}

int vioc_intr_clear(int id, unsigned mask)
{
	volatile void __iomem *reg;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return -1;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_DEV2:
#endif
		reg = VIOC_DISP_GetAddress(id);
		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		break;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#ifdef CONFIG_ARCH_TCC803X
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(id) + RDMASTAT;
		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		break;
#ifdef CONFIG_ARCH_TCC803X
    case VIOC_INTR_WD9:
    case VIOC_INTR_WD10:
    case VIOC_INTR_WD11:
    case VIOC_INTR_WD12:
        id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
        reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
        __raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
        break;
#endif
	}

	return 0;
}

void vioc_intr_initialize(void)
{
	volatile void __iomem *reg = VIOC_IREQConfig_GetAddress();
	int i;

	__raw_writel(0xffffffff, reg + IRQMASKCLR0_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_1_OFFSET);
#if !defined(CONFIG_ARCH_TCC897X)
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_2_OFFSET);
#endif

	/* disp irq mask & status clear */
	for (i = 0; i < (VIOC_INTR_DEV2 - VIOC_INTR_DEV0); i++) {
		reg = VIOC_DISP_GetAddress(i);
		if (reg == NULL)
			continue;
		__raw_writel(VIOC_DISP_INT_MASK, reg + DIM);
		__raw_writel(VIOC_DISP_INT_MASK, reg + DSTATUS);
	}

	/* rdma irq mask & status clear */
	for (i=0; i < (VIOC_INTR_RD11 - VIOC_INTR_RD0); i++) {
		reg = VIOC_RDMA_GetAddress(i);
		if(reg == NULL)
			continue;
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMAIRQMSK);
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMASTAT);
	}

	/* wdma irq mask & status clear */
#ifdef CONFIG_ARCH_TCC803X
    for (i = 0; i < (VIOC_INTR_WD12 - (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0)); i++) {
        reg = VIOC_WDMA_GetAddress(i);
        if (reg == NULL)
            continue;
        __raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
        __raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
    }
#else
    for (i = 0; i < (VIOC_INTR_WD8 - VIOC_INTR_WD0); i++) {
        reg = VIOC_WDMA_GetAddress(i);
        if (reg == NULL)
            continue;
        __raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
        __raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
    }
#endif

}

static int __init vioc_intr_init(void)
{
	struct device_node *ViocIntr_np;

	ViocIntr_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_intr");

	if (ViocIntr_np == NULL) {
		pr_info("vioc-intr: disabled [this is mandatory for vioc display]\n");
	} else {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		int i = 0;
		for(i = 0; i < VIOC_IRQ_MAX; i++) {
			vioc_base_irq_num[i] = irq_of_parse_and_map(ViocIntr_np, i);
			pr_info("vioc-intr%d: irq %d\n", i, vioc_base_irq_num[i]);
		}
#else
		vioc_base_irq_num[0] = irq_of_parse_and_map(ViocIntr_np, 0);
		pr_info("vioc-intr%d : %d\n", 0, vioc_base_irq_num[0]);
#endif
	}

	return 0;
}
fs_initcall(vioc_intr_init);

