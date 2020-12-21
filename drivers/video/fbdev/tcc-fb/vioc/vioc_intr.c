/*
 * Copyright (C) Telechips Inc.
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
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_intr.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_vin.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h> // is_VIOC_REMAP

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_dv_cfg.h>
#endif

#ifdef CONFIG_TCC_CORE_RESET
#define CONFIG_TCC_VIOC_CORE_RESET_SUPPORT
#define FB_CR_VIOC_REMAP_DISABLE 2
#define FB_CR_VIOC_REMAP_ENABLE 3
#define FB_CR_REG_IDX(x) (x / 32)
#define FB_CR_REG_OFFSET(x) (FB_CR_REG_IDX(x) * 4)
#define FB_CR_BIT_OFFSET(x) (x % 32)
static volatile void __iomem *pTIMER_reg;
#endif

static int vioc_base_irq_num[4] = {
	0,
};

int vioc_intr_enable(int irq, int id, unsigned int mask)
{
	volatile void __iomem *reg;
	unsigned int sub_id;
	unsigned int type_clr_offset;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return -1;

#ifdef CONFIG_TCC_VIOC_CORE_RESET_SUPPORT
	__raw_writel(
		__raw_readl(pTIMER_reg + FB_CR_REG_OFFSET(id))
			| (1 << FB_CR_BIT_OFFSET(id)),
		pTIMER_reg + FB_CR_REG_OFFSET(id));
#endif

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3)
			sub_id = VIOC_INTR_DEV3 - VIOC_INTR_DISP_OFFSET;
		else
			sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress(sub_id);
		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		break;
#endif
#endif
		sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress(sub_id);
		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		break;

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	case VIOC_INTR_V_DV: {
		volatile void __iomem *pDV_Cfg =
			VIOC_DV_VEDR_GetAddress(VDV_CFG);
		VIOC_V_DV_SetInterruptEnable(
			pDV_Cfg, (mask & VIOC_V_DV_INT_MASK), 1);

		// dprintk_dv_sequence("### V_DV INT On\n");
	} break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		/* clera irq status */
		sub_id = id - VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(sub_id) + RDMASTAT;
		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_RDMA_GetAddress(sub_id) + RDMAIRQMSK;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_RDMA_INT_MASK), reg);
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
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD0;

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		break;
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
			- VIOC_INTR_WD0;

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		break;
#endif
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		sub_id = id - VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress(sub_id * 2) + VIN_INT;

		/* clera irq status */
		__raw_writel((mask & VIOC_VIN_INT_MASK), reg);

		/* enable irq */
		__raw_writel(
			__raw_readl(reg) | ((mask & VIOC_VIN_INT_MASK) << 16),
			reg);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		sub_id = id - VIOC_INTR_VIN_OFFSET - VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress(sub_id * 2) + VIN_INT;

		/* clera irq status */
		__raw_writel((mask & VIOC_VIN_INT_MASK), reg);

		/* enable irq */
		__raw_writel(
			__raw_readl(reg) | ((mask & VIOC_VIN_INT_MASK) << 16),
			reg);
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) \
	|| defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
	if (irq == vioc_base_irq_num[0])
		type_clr_offset = IRQMASKCLR0_0_OFFSET;
	else if (irq == vioc_base_irq_num[1])
		type_clr_offset = IRQMASKCLR1_0_OFFSET;
	else if (irq == vioc_base_irq_num[2])
		type_clr_offset = IRQMASKCLR2_0_OFFSET;
	else if (irq == vioc_base_irq_num[3])
		type_clr_offset = IRQMASKCLR3_0_OFFSET;
	else {
		pr_err("[ERR][VIOC_INTR] %s-%d :: irq(%d) is wierd.\n",
		       __func__, __LINE__, irq);
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
EXPORT_SYMBOL(vioc_intr_enable);

int vioc_intr_disable(int irq, int id, unsigned int mask)
{
	volatile void __iomem *reg;
	unsigned int sub_id;
	unsigned int do_irq_mask = 1;
	unsigned int type_set_offset;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return -1;

#ifdef CONFIG_TCC_VIOC_CORE_RESET_SUPPORT
	__raw_writel(
		__raw_readl(pTIMER_reg + FB_CR_REG_OFFSET(id))
			& ~(1 << FB_CR_BIT_OFFSET(id)),
		pTIMER_reg + FB_CR_REG_OFFSET(id));
#endif

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3)
			sub_id = VIOC_INTR_DEV3 - VIOC_INTR_DISP_OFFSET;
		else
			sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress(sub_id) + DIM;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK)
		    != VIOC_DISP_INT_MASK)
			do_irq_mask = 0;
		break;
#endif
#endif
		sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress(sub_id) + DIM;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK)
		    != VIOC_DISP_INT_MASK)
			do_irq_mask = 0;
		break;
#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
	case VIOC_INTR_V_DV: {
		volatile void __iomem *pDV_Cfg =
			VIOC_DV_VEDR_GetAddress(VDV_CFG);

		VIOC_V_DV_SetInterruptEnable(
			pDV_Cfg, (mask & VIOC_V_DV_INT_MASK), 0);
		if (!(__raw_readl(pDV_Cfg) & (mask & VIOC_V_DV_INT_MASK)))
			do_irq_mask = 0;
		// dprintk_dv_sequence("### V_DV INT Off\n");
	} break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		sub_id = id - VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(sub_id) + RDMAIRQMSK;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_RDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_RDMA_INT_MASK)
		    != VIOC_RDMA_INT_MASK)
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
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK)
			do_irq_mask = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK)
			do_irq_mask = 0;
		break;
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		sub_id = id - VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
			- VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress(sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK)
			do_irq_mask = 0;
		break;
#endif
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		sub_id = id - VIOC_INTR_VIN0;
		reg = VIOC_VIN_GetAddress(sub_id * 2) + VIN_INT;
		__raw_writel(
			__raw_readl(reg) & ~((mask & VIOC_VIN_INT_MASK) << 16),
			reg);
		//if ((__raw_readl(reg) & VIOC_VIN_INT_MASK) !=
		// VIOC_VIN_INT_MASK) do_irq_mask = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		sub_id = id - VIOC_INTR_VIN_OFFSET - VIOC_INTR_VIN0;
		reg = VIOC_VIN_GetAddress(sub_id * 2) + VIN_INT;
		__raw_writel(
			__raw_readl(reg) & ~((mask & VIOC_VIN_INT_MASK) << 16),
			reg);
		//if ((__raw_readl(reg) & VIOC_VIN_INT_MASK) !=
		//VIOC_VIN_INT_MASK) do_irq_mask = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	}

	if (do_irq_mask) {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) \
	|| defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
		if (irq == vioc_base_irq_num[0])
			type_set_offset = IRQMASKSET0_0_OFFSET;
		else if (irq == vioc_base_irq_num[1])
			type_set_offset = IRQMASKSET1_0_OFFSET;
		else if (irq == vioc_base_irq_num[2])
			type_set_offset = IRQMASKSET2_0_OFFSET;
		else if (irq == vioc_base_irq_num[3])
			type_set_offset = IRQMASKSET3_0_OFFSET;
		else {
			pr_err("[ERR][VIOC_INTR] %s-%d :: irq(%d) is wierd.\n",
			       __func__, __LINE__, irq);
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
			__raw_writel(
				1 << (id - 32), reg + type_set_offset + 0x4);
#else
		if (id >= 64)
			__raw_writel(
				1 << (id - 64), reg + type_set_offset + 0x8);
		else if (id >= 32 && id < 64)
			__raw_writel(
				1 << (id - 32), reg + type_set_offset + 0x4);
		else
			__raw_writel(1 << id, reg + type_set_offset);
#endif
	}

	return 0;
}
EXPORT_SYMBOL(vioc_intr_disable);

unsigned int vioc_intr_get_status(int id)
{
	volatile void __iomem *reg;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return 0;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3)
			id -= VIOC_INTR_DISP_OFFSET;
		reg = VIOC_DISP_GetAddress(id) + DSTATUS;

		return (__raw_readl(reg) & VIOC_DISP_INT_MASK);
#endif
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		return (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		return (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
#endif
#endif
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		return (__raw_readl(reg) & VIOC_VIN_INT_MASK);
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		return (__raw_readl(reg) & VIOC_VIN_INT_MASK);
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	}
	return 0;
}
EXPORT_SYMBOL(vioc_intr_get_status);

bool check_vioc_irq_status(volatile void __iomem *reg, int id)
{
	unsigned int flag;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return false;

	if (id < 32) {
		flag = (__raw_readl(reg + IRQSELECT0_0_OFFSET) & (1 << id)) ?
			(__raw_readl(reg + SYNCSTATUS0_OFFSET) & (1 << id)) :
			(__raw_readl(reg + RAWSTATUS0_OFFSET) & (1 << id));
	} else if (id >= 32 && id < 64) {
		flag = (__raw_readl(reg + IRQMASKCLR0_1_OFFSET)
			& (1 << (id - 32))) ?
			(__raw_readl(reg + SYNCSTATUS1_OFFSET)
			 & (1 << (id - 32))) :
			(__raw_readl(reg + RAWSTATUS1_OFFSET)
			 & (1 << (id - 32)));
	}
#if !defined(CONFIG_ARCH_TCC897X)
	else {
		flag = (__raw_readl(reg + IRQMASKCLR0_2_OFFSET)
			& (1 << (id - 64))) ?
			(__raw_readl(reg + SYNCSTATUS2_OFFSET)
			 & (1 << (id - 64))) :
			(__raw_readl(reg + RAWSTATUS2_OFFSET)
			 & (1 << (id - 64)));
	}
#endif

	if (flag)
		return true;
	return false;
}
EXPORT_SYMBOL(check_vioc_irq_status);

bool is_vioc_intr_activatied(int id, unsigned int mask)
{
	volatile void __iomem *reg;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return false;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3)
			id -= VIOC_INTR_DISP_OFFSET;

		reg = VIOC_DISP_GetAddress(id);
		if (__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK))
			return true;
		return false;
#endif
#endif
		reg = VIOC_DISP_GetAddress(id);
		if (__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK))
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(id) + RDMASTAT;
		if (__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK))
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return true;
		return false;
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return true;
		return false;
#endif
#endif
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		if (__raw_readl(reg) & (mask & VIOC_VIN_INT_MASK))
			return true;
		return false;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		if (__raw_readl(reg) & (mask & VIOC_VIN_INT_MASK))
			return true;
		return false;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	}
	return false;
}
EXPORT_SYMBOL(is_vioc_intr_activatied);

bool is_vioc_intr_unmasked(int id, unsigned int mask)
{
	volatile void __iomem *reg;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return false;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3)
			id -= VIOC_INTR_DISP_OFFSET;

		reg = VIOC_DISP_GetAddress(id);
		if (__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK))
			return true;
		return false;
#endif
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress(id) + RDMAIRQMSK;
		if (__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK))
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQMSK_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return false;
		return true;
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQMSK_OFFSET;
		if (__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK))
			return false;
		return true;
#endif
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		if (__raw_readl(reg) & ((mask & VIOC_VIN_INT_MASK) << 16))
			return true;
		return false;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		if (__raw_readl(reg) & ((mask & VIOC_VIN_INT_MASK) << 16))
			return true;
		return false;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	}
	return false;
}
EXPORT_SYMBOL(is_vioc_intr_unmasked);

bool is_vioc_display_device_intr_masked(int id, unsigned int mask)
{
	void __iomem *reg;
	bool ret = false;
	u32 reg_val;

	if (get_vioc_type(id) != get_vioc_type(VIOC_DISP)) {
		ret = true;
		goto out;
	}

	switch (get_vioc_index(id)) {
	case get_vioc_index(VIOC_DISP0):
	case get_vioc_index(VIOC_DISP1):
	#if defined(VIOC_DISP2)
	case get_vioc_index(VIOC_DISP2):
	#endif
	#if defined(VIOC_DISP3)
	case get_vioc_index(VIOC_DISP3):
	#endif
		break;
	default:
		ret = true;
		goto out;
	}
	reg = (void __iomem *)VIOC_DISP_GetAddress(id);
	if (!reg)
		goto out;
	reg_val = readl(reg + DIM);
	if ((reg_val & mask) & VIOC_DISP_INT_MASK)
		ret = true;
out:
	return ret;
}
EXPORT_SYMBOL(is_vioc_display_device_intr_masked);

int vioc_intr_clear(int id, unsigned int mask)
{
	volatile void __iomem *reg;

	if ((id < 0) || (id > VIOC_INTR_NUM))
		return -1;

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3)
			id -= VIOC_INTR_DISP_OFFSET;

		reg = VIOC_DISP_GetAddress(id);
		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		break;
#endif
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
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
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		id -= (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		break;
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		id -= (VIOC_INTR_WD_OFFSET - VIOC_INTR_WD_OFFSET2
		       - VIOC_INTR_WD0);
		reg = VIOC_WDMA_GetAddress(id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		break;
#endif
#endif
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		__raw_writel((mask & VIOC_VIN_INT_MASK), reg);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);
		reg = VIOC_VIN_GetAddress(id * 2) + VIN_INT;
		__raw_writel((mask & VIOC_VIN_INT_MASK), reg);
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	}

	return 0;
}
EXPORT_SYMBOL(vioc_intr_clear);

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
#if defined(CONFIG_ARCH_TCC805X)
	for (i = 0;
	     i < (VIOC_INTR_DEV3 - (VIOC_INTR_DISP_OFFSET + VIOC_INTR_DEV0));
	     i++) {
#else
	for (i = 0; i < (VIOC_INTR_DEV2 - VIOC_INTR_DEV0); i++) {
#endif
		reg = VIOC_DISP_GetAddress(i);
		if (reg == NULL)
			continue;
		__raw_writel(VIOC_DISP_INT_MASK, reg + DIM);
		__raw_writel(VIOC_DISP_INT_MASK, reg + DSTATUS);
	}

	/* rdma irq mask & status clear */
	for (i = 0; i < (VIOC_INTR_RD11 - VIOC_INTR_RD0); i++) {
		reg = VIOC_RDMA_GetAddress(i);
		if (reg == NULL)
			continue;
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMAIRQMSK);
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMASTAT);
	}

		/* wdma irq mask & status clear */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_ARCH_TCC805X)
	for (i = 0;
		i < (
			VIOC_INTR_WD13 - (
				VIOC_INTR_WD_OFFSET + VIOC_INTR_WD_OFFSET2 +
				VIOC_INTR_WD0)); i++) {
#else
	for (i = 0;
	     i < (VIOC_INTR_WD12 - (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0));
	     i++) {
#endif
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
EXPORT_SYMBOL(vioc_intr_initialize);

#ifdef CONFIG_TCC_VIOC_CORE_RESET_SUPPORT
static int fb_cr_intr_clear(void)
{
	int i;
	unsigned int irq_mask;

	for (i = 0; i < VIOC_INTR_NUM; i++) {
		if (__raw_readl(pTIMER_reg + FB_CR_REG_OFFSET(i))
		    & (1 << FB_CR_BIT_OFFSET(i))) {
			irq_mask = vioc_intr_get_status(i);
			vioc_intr_clear(i, irq_mask);
			__raw_writel(
				__raw_readl(pTIMER_reg + FB_CR_REG_OFFSET(i))
					& ~(1 << FB_CR_BIT_OFFSET(i)),
				pTIMER_reg + FB_CR_REG_OFFSET(i));
			pr_info("[INF][VIOC_INTR] %s : cleared intr = %d, reg = 0x%08x\n",
				__func__, i, pTIMER_reg + FB_CR_REG_OFFSET(i));
		}
	}
}
#endif

static int __init vioc_intr_init(void)
{
	struct device_node *ViocIntr_np;

	ViocIntr_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_intr");

	if (ViocIntr_np == NULL) {
		pr_info("[INF][VIOC_INTR] disabled [this is mandatory for vioc display]\n");
	} else {
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) \
	|| defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
		int i = 0;

		for (i = 0; i < VIOC_IRQ_MAX; i++) {
			vioc_base_irq_num[i] =
				irq_of_parse_and_map(ViocIntr_np, i);
			pr_info("[INF][VIOC_INTR] vioc-intr%d: irq %d\n", i,
				vioc_base_irq_num[i]);
		}

#ifdef CONFIG_TCC_VIOC_CORE_RESET_SUPPORT
		pTIMER_reg = (volatile void __iomem *)of_iomap(
			ViocIntr_np,
			is_VIOC_REMAP ? FB_CR_VIOC_REMAP_ENABLE :
				FB_CR_VIOC_REMAP_DISABLE);
		pr_info("[INF][VIOC_INTR] vioc-intr: fb core reset mode. backup reg = 0x%08x\n",
			pTIMER_reg);
		fb_cr_intr_clear();
#endif

#else
		vioc_base_irq_num[0] = irq_of_parse_and_map(ViocIntr_np, 0);
		pr_info("[INF][VIOC_INTR] vioc-intr%d : %d\n",
			0, vioc_base_irq_num[0]);
#endif
	}

	return 0;
}
fs_initcall(vioc_intr_init);
