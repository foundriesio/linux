/*
 * linux/arch/arm/mach-tcc899x/vioc_pixel_mapper.c
 * Author:  <linux@telechips.com>
 * Created: Jan 20, 2018
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
#include <asm/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_lut_3d.h>

#define LUT0_INDEX				125
#define LUT1_INDEX				225
#define LUT2_INDEX				325
#define LUT3_INDEX				405
#define LUT4_INDEX				505
#define LUT5_INDEX				585
#define LUT6_INDEX				665
#define LUT7_INDEX				729

static volatile void __iomem *pLUT3D_reg[VIOC_LUT_3D_MAX] = {0};

unsigned int lut_reg[729];

int vioc_lut_3d_set_select(unsigned int lut_n, unsigned int sel)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_lut_3d_address(lut_n);

	value = __raw_readl(reg + LUT_3D_CTRL_OFFSET) & ~(LUT_3D_CTRL_SEL_MASK);
	value |= (sel << LUT_3D_CTRL_SEL_SHIFT);

	__raw_writel(value, reg);
	return 0;
}

int vioc_lut_3d_set_table(unsigned int lut_n, unsigned int *table)
{
	unsigned int index, i, offset;
	volatile void __iomem *reg;
	reg = get_lut_3d_address(lut_n) + LUT_3D_TABLE_OFFSET;

	index = 0;
	vioc_lut_3d_set_select(lut_n, 0);
	for(i = 0; i<LUT0_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 1);
	for(i = LUT0_INDEX; i<LUT1_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 2);
	for(i = LUT1_INDEX; i<LUT2_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 3);
	for(i = LUT2_INDEX; i<LUT3_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 4);
	for(i = LUT3_INDEX; i<LUT4_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 5);
	for(i = LUT4_INDEX; i<LUT5_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 6);
	for(i = LUT5_INDEX; i<LUT6_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	index = 0;
	vioc_lut_3d_set_select(lut_n, 7);
	for(i = LUT6_INDEX; i<LUT7_INDEX; i++, index++)	{
		offset = (0xFFF & index);
		__raw_writel(table[i], reg + (offset *0x4));
	}

	vioc_lut_3d_pend(lut_n, 1);
	vioc_lut_3d_setupdate(lut_n);

	return 0;
}

int vioc_lut_3d_setupdate(unsigned int lut_n)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_lut_3d_address(lut_n);

	value = __raw_readl(reg + LUT_3D_CTRL_OFFSET) | (1 << LUT_3D_CTRL_UPD_SHIFT);
	__raw_writel(value, reg + LUT_3D_CTRL_OFFSET);

	return 0;
}

/*
 *	vioc_lut_3d_bypass
 *	@lut_n : 3d LUT NUM
 *	@onoff :
 *		0 - Bypass ON	(Disable 3D LUT)
 *		1 - Bypass OFF	(Enable 3D LUT)
 *
 */
int vioc_lut_3d_bypass(unsigned int lut_n, unsigned int onoff)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_lut_3d_address(lut_n);

	if(onoff)
		value = __raw_readl(reg + LUT_3D_CTRL_OFFSET) & ~(LUT_3D_CTRL_BYPASS_MASK);
	else
		value = __raw_readl(reg + LUT_3D_CTRL_OFFSET) | (LUT_3D_CTRL_BYPASS_MASK);

	__raw_writel(value, reg + LUT_3D_CTRL_OFFSET);
	return 0;
}

int vioc_lut_3d_pend(unsigned int lut_n, unsigned int onoff)
{
	unsigned int value;
	volatile void __iomem *reg;
	reg = get_lut_3d_address(lut_n) + LUT_3D_PEND_OFFSET;

	if(onoff)
		value = __raw_readl(reg) | (LUT_3D_PEND_PEND_MASK);
	else
		value = __raw_readl(reg) & ~(LUT_3D_PEND_PEND_MASK);

	__raw_writel(value, reg);
	return 0;
}

volatile void __iomem *get_lut_3d_address(unsigned int lut_n)
{
	if (lut_n < VIOC_LUT_3D_MAX)
		return pLUT3D_reg[lut_n];
	else
		return NULL;
}


static int __init vioc_lut_3d_init(void)
{
	int i = 0;
	struct device_node *ViocLUT3D_np;
	volatile void __iomem *reg;
	ViocLUT3D_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_lut_3d");

	if (ViocLUT3D_np == NULL) {
		pr_info("[INF][LUT_3D] disabled\n");
	} else {
		for (i = 0; i <  VIOC_LUT_3D_MAX; i++) {
			pLUT3D_reg[i] = of_iomap(ViocLUT3D_np, i);

			if (pLUT3D_reg[i])
				printk("[INF][LUT_3D] vioc-lut-3d%d: 0x%p\n", i, pLUT3D_reg[i]);

			vioc_lut_3d_bypass(i, 0);
			memset(&lut_reg, 0x0, sizeof(lut_reg));
		}
	}
	return 0;
}
arch_initcall(vioc_lut_3d_init);
