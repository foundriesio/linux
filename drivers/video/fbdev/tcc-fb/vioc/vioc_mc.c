/*
 * linux/arch/arm/mach-tcc893x/vioc_rdma.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
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
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/system_info.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>

#include <video/tcc/vioc_global.h>
#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_mc.h>
#if defined(DOLBY_VISION_CHECK_SEQUENCE)
#include <video/tcc/vioc_v_dv.h>
#endif

#define MC_MAX_N 2

static struct device_node *ViocMC_np;
static volatile void __iomem *pMC_reg[MC_MAX_N] = {0};

void VIOC_MC_Get_OnOff(volatile void __iomem *reg, uint *enable)
{
	*enable = ((__raw_readl(reg + MC_CTRL) & MC_CTRL_START_MASK) >>
		   MC_CTRL_START_SHIFT);
}

void VIOC_MC_Start_OnOff(volatile void __iomem *reg, uint OnOff)
{
	unsigned long val;

#if defined(DOLBY_VISION_CHECK_SEQUENCE)
	VIOC_MC_Get_OnOff(reg, &val);
	if(val != OnOff)
	{
		dprintk_dv_sequence("### ====> %d MC %s \n", (reg == VIOC_MC_GetAddress(0)) ? 0 : 1, OnOff ? "On" : "Off");
	}
#endif

	val = (__raw_readl(reg + MC_CTRL) & ~(MC_CTRL_START_MASK));
	val |= ((OnOff & 0x1) << MC_CTRL_START_SHIFT);
	val |= (0x1 << MC_CTRL_UPD_SHIFT);
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_UPD(volatile void __iomem *reg)
{
	unsigned long val;

	if (__raw_readl(reg + MC_CTRL) & MC_CTRL_UPD_MASK)
		return;

	val = (__raw_readl(reg + MC_CTRL) & ~(MC_CTRL_UPD_MASK));
	val |= (0x1 << MC_CTRL_UPD_SHIFT);
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_Y2R_OnOff(volatile void __iomem *reg, uint OnOff, uint mode)
{
	unsigned long val;
	val = (__raw_readl(reg + MC_CTRL) &
	       ~(MC_CTRL_Y2REN_MASK | MC_CTRL_Y2RMD_MASK));
	val |= (((OnOff & 0x1) << MC_CTRL_Y2REN_SHIFT) |
		((mode & 0x7) << MC_CTRL_Y2RMD_SHIFT));
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_Start_BitDepth(volatile void __iomem *reg, uint Chroma, uint Luma)
{
	unsigned long val;
	val = (__raw_readl(reg + MC_CTRL) &
	       ~(MC_CTRL_DEPC_MASK | MC_CTRL_DEPY_MASK));
	val |= (((Chroma & 0x3) << MC_CTRL_DEPC_SHIFT) |
		((Luma & 0x3) << MC_CTRL_DEPY_SHIFT));
	__raw_writel(val, reg + MC_CTRL);
}

void VIOC_MC_OFFSET_BASE(volatile void __iomem *reg, uint base_y, uint base_c)
{
	__raw_writel(((base_y & 0xFFFFFFFF) << MC_OFS_BASE_Y_BASE_SHIFT),
		     reg + MC_OFS_BASE_Y);
	__raw_writel(((base_c & 0xFFFFFFFF) << MC_OFS_BASE_C_BASE_SHIFT),
		     reg + MC_OFS_BASE_C);
}

void VIOC_MC_FRM_BASE(volatile void __iomem *reg, uint base_y, uint base_c)
{
	__raw_writel(((base_y & 0xFFFFFFFF) << MC_FRM_BASE_Y_BASE_SHIFT),
		     reg + MC_FRM_BASE_Y);
	__raw_writel(((base_c & 0xFFFFFFFF) << MC_FRM_BASE_C_BASE_SHIFT),
		     reg + MC_FRM_BASE_C);
}

void VIOC_MC_FRM_SIZE(volatile void __iomem *reg, uint xsize, uint ysize)
{
	unsigned long val;

#if defined(CONFIG_MC_WORKAROUND)
	if(!system_rev)
		ysize += 0x4;
#endif

	val = (((ysize & 0xFFFF) << MC_FRM_SIZE_YSIZE_SHIFT) |
	       ((xsize & 0xFFFF) << MC_FRM_SIZE_XSIZE_SHIFT));
	__raw_writel(val, reg + MC_FRM_SIZE);
}

void VIOC_MC_FRM_SIZE_MISC(volatile void __iomem *reg, uint pic_height,
			   uint stride_y, uint stride_c)
{
	unsigned long val;
	val = ((pic_height & 0x3FFF) << MC_PIC_HEIGHT_SHIFT);
	__raw_writel(val, reg + MC_PIC);

	val = (((stride_c & 0xFFFF) << MC_FRM_STRIDE_STRIDE_C_SHIFT) |
	       ((stride_y & 0xFFFF) << MC_FRM_STRIDE_STRIDE_Y_SHIFT));
	__raw_writel(val, reg + MC_FRM_STRIDE);
}

void VIOC_MC_FRM_POS(volatile void __iomem *reg, uint xpos, uint ypos)
{
	unsigned long val;
	val = (((ypos & 0xFFFF) << MC_FRM_POS_YPOS_SHIFT) |
	       ((xpos & 0xFFFF) << MC_FRM_POS_XPOS_SHIFT));
	__raw_writel(val, reg + MC_FRM_POS);
}

void VIOC_MC_ENDIAN(volatile void __iomem *reg, uint ofs_endian,
		    uint comp_endian)
{
	unsigned long val;
	val = (__raw_readl(reg + MC_FRM_MISC0) &
	       ~(MC_FRM_MISC0_COMP_ENDIAN_MASK | MC_FRM_MISC0_OFS_ENDIAN_MASK));
	val |= (((comp_endian & 0xF) << MC_FRM_MISC0_COMP_ENDIAN_SHIFT) |
		((ofs_endian & 0xF) << MC_FRM_MISC0_OFS_ENDIAN_SHIFT));
	__raw_writel(val, reg + MC_FRM_MISC0);
}

void VIOC_MC_DITH_CONT(volatile void __iomem *reg, uint en, uint sel)
{
	unsigned long val;
	val = (__raw_readl(reg + MC_DITH_CTRL) &
	       ~(MC_DITH_CTRL_EN_MASK | MC_DITH_CTRL_SEL_MASK));
	val = (((sel & 0xF) << MC_DITH_CTRL_SEL_SHIFT) |
	       ((en & 0xFF) << MC_DITH_CTRL_EN_SHIFT));
	__raw_writel(val, reg + MC_DITH_CTRL);
}

void VIOC_MC_SetDefaultAlpha(volatile void __iomem *reg, uint alpha)
{
	unsigned long val;
	val = (__raw_readl(reg + MC_TIMEOUT) & ~(MC_TIMEOUT_ALPHA_MASK));
	val |= ((alpha & 0x3FF) << MC_TIMEOUT_ALPHA_SHIFT);
	__raw_writel(val, reg + MC_TIMEOUT);
}

volatile void __iomem *VIOC_MC_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_MC_MAX)
		goto err;

	if (pMC_reg[Num] == NULL)
		pr_err("[ERR][MC] %s num:%d \n", __func__, Num);

	return pMC_reg[Num];

err:
	pr_err("[ERR][MC] %s Num:%d max:%d \n", __func__, Num, VIOC_MC_MAX);
	return NULL;
}

int tvc_mc_get_info(unsigned int component_num, mc_info_type *pMC_info)
{
	volatile void __iomem *reg;
	unsigned int value = 0;
	if ((component_num >= VIOC_MC0) && (component_num <= (VIOC_MC0 + VIOC_MC_MAX))) {
		reg = VIOC_MC_GetAddress(component_num);
		if (!reg)
			return -1;

		value = __raw_readl(reg + MC_FRM_SIZE);
		pMC_info->width = (value & MC_FRM_SIZE_XSIZE_MASK) >>
				  MC_FRM_SIZE_XSIZE_SHIFT;
		pMC_info->height = (value & MC_FRM_SIZE_YSIZE_MASK) >>
				   MC_FRM_SIZE_YSIZE_SHIFT;

		return 0;
	}
	return -1;
}

void VIOC_MC_DUMP(unsigned int vioc_id)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_MC_MAX)
		goto err;

	pReg = VIOC_MC_GetAddress(Num);

	pr_debug("[DBG][MC] 0x%p \n", pReg);
	while (cnt < 0x100) {
		pr_debug("[DBG][MC] %p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][MC] %s Num:%d max:%d \n", __func__, Num, VIOC_MC_MAX);
	return;
}


static int __init vioc_mc_init(void)
{
	int i;

	ViocMC_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_mc");
	if (ViocMC_np == NULL) {
		pr_info("[INF][MC] disabled\n");
	} else {
		for (i = 0; i < MC_MAX_N; i++) {
			pMC_reg[i] = (volatile void __iomem *)of_iomap(ViocMC_np,
							is_VIOC_REMAP ? (i + MC_MAX_N) : i);

			if (pMC_reg[i])
				pr_info("[INF][MC] vioc-mc%d: 0x%p\n", i, pMC_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_mc_init);
