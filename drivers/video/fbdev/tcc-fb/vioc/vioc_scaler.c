/*
 * linux/arch/arm/mach-tcc893x/vioc_scaler.c
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <asm/system_info.h>

#include <video/tcc/tcc_types.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP

static struct device_node *ViocSC_np;
static volatile void __iomem *pScale[VIOC_SCALER_MAX];

void VIOC_SC_SetBypass(volatile void __iomem *reg, unsigned int nOnOff)
{
	unsigned long val;
	val = (__raw_readl(reg + SCCTRL) & ~(SCCTRL_BP_MASK));
	val |= (nOnOff << SCCTRL_BP_SHIFT);
	__raw_writel(val, reg + SCCTRL);
}

void VIOC_SC_SetUpdate(volatile void __iomem *reg)
{
	unsigned long val;
	val = (__raw_readl(reg + SCCTRL) & ~(SCCTRL_UPD_MASK /*| SCCTRL_FFC_MASK*/));
	val |= (0x1 << SCCTRL_UPD_SHIFT);
	__raw_writel(val, reg + SCCTRL);
}

void VIOC_SC_SetSrcSize(volatile void __iomem *reg, unsigned int nWidth,
			unsigned int nHeight)
{
	unsigned long val;
	val = ((nHeight << SCSSIZE_HEIGHT_SHIFT) |
	       (nWidth << SCSSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + SCSSIZE);
}

#if defined(CONFIG_MC_WORKAROUND)
unsigned int VIOC_SC_GetPlusSize(unsigned int src_height, unsigned dst_height)
{
	unsigned int plus_temp, plus_height;

	if(system_rev)
		return 0;

	if(src_height <= dst_height) //up-scaling
	{
		plus_temp = (0x4)*((dst_height*10)/src_height);
		plus_height = (plus_temp/10);
		if((plus_temp%10) > 0)
			plus_height += 1;
	}
	else //down-scaling
	{
		plus_temp = ((src_height + 0x4)*100)/((src_height*10)/dst_height);
		plus_height = (plus_temp/10) - src_height;
		if((plus_temp%10) > 0)
			plus_height += 1;
	}

//	pr_info("[INF][SC] %s-%d :: %d -> %d scale, %d => %d\n", __func__, __LINE__, src_height, dst_height, plus_temp, plus_height);

	return 0x4;
//	return plus_height;
}
#endif

void VIOC_SC_SetDstSize(volatile void __iomem *reg, unsigned int nWidth,
			unsigned int nHeight)
{
	unsigned long val;
	val = ((nHeight << SCDSIZE_HEIGHT_SHIFT) |
	       (nWidth << SCDSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + SCDSIZE);
}

void VIOC_SC_SetOutSize(volatile void __iomem *reg, unsigned int nWidth,
			unsigned int nHeight)
{
	unsigned long val;
	val = ((nHeight << SCOSIZE_HEIGHT_SHIFT) |
	       (nWidth << SCOSIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + SCOSIZE);
}

void VIOC_SC_SetOutPosition(volatile void __iomem *reg, unsigned int nXpos,
			    unsigned int nYpos)
{
	unsigned long val;
	val = ((nYpos << SCOPOS_YPOS_SHIFT) | (nXpos << SCOPOS_XPOS_SHIFT));
	__raw_writel(val, reg + SCOPOS);
}

volatile void __iomem *VIOC_SC_GetAddress(unsigned int vioc_id)
{
	volatile void __iomem *pScaler = NULL;

	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_SCALER_MAX)
		goto err;

	if (ViocSC_np == NULL) {
		pr_err("[ERR][SC] %s VIOC_SC NULL : %p \n", __func__, ViocSC_np);
	} else {
		if (pScale[Num] == NULL)
			pr_err("[ERR][SC] scaler number over range %d : function:%s \n  ",
			       Num, __func__);
		else
			pScaler = pScale[Num];
	}

	return pScaler;

err:
	pr_err("[ERR][SC] %s Num:%d max num:%d \n", __func__, Num, VIOC_SCALER_MAX);
	return NULL;	
}

void VIOC_SCALER_DUMP(volatile void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	volatile void __iomem *pReg = reg;
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_SCALER_MAX)
		goto err;

	if (pReg == NULL)
		pReg = VIOC_SC_GetAddress(vioc_id);

	pr_debug("[DBG][SC] SCALER-%d :: 0x%p \n", Num, pReg);
	while (cnt < 0x20) {
		pr_debug("0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	return;

err:
	pr_err("[ERR][SC] %s Num:%d max num:%d \n", __func__, Num, VIOC_SCALER_MAX);
	return;
}

static int __init vioc_sc_init(void)
{
	int i;
	ViocSC_np = of_find_compatible_node(NULL, NULL, "telechips,scaler");

	if (ViocSC_np == NULL) {
		pr_err("[ERR][SC] cann't find vioc_scaler \n");
	} else {
		for (i = 0; i < VIOC_SCALER_MAX; i++)
			pScale[i] = (volatile void __iomem *)of_iomap(ViocSC_np,
						is_VIOC_REMAP ? (i + VIOC_SCALER_MAX) : i);
	}

	return 0;
}
arch_initcall(vioc_sc_init);
