/*
 * linux/arch/arm/mach-tcc893x/vioc_outcfg.h
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

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_outcfg.h>

/* Debugging stuff */
static int debug = 0;
#define dprintk(msg...)	if (debug) { printk("\e[33m[DBG][OUTCFG]\e[0m " msg); }

static volatile void __iomem *pOUTCFG_reg = NULL;

/* -------------------------------
2¡¯b00 : Display Device 0 Component
2¡¯b01 : Display Device 1 Component
2¡¯b10 : Display Device 2 Component
2¡¯b11 : NOT USED
---------------------------------*/
void VIOC_OUTCFG_SetOutConfig(unsigned nType, unsigned nDisp)
{
	volatile void __iomem *reg = VIOC_OUTCONFIG_GetAddress();
	unsigned long val;

	if (reg == NULL)
		return;

	nDisp = get_vioc_index(nDisp);
	pr_info("[INF][OUTCFG] %s : addr:%p nType:%d nDisp:%d \n", __func__, reg, nType,
		nDisp);

	switch (nType) {
	case VIOC_OUTCFG_HDMI:
		val = (__raw_readl(reg + MISC) & ~(MISC_HDMISEL_MASK));
		val |= ((nDisp & 0x3) << MISC_HDMISEL_SHIFT);
		break;
	case VIOC_OUTCFG_SDVENC:
		val = (__raw_readl(reg + MISC) & ~(MISC_SDVESEL_MASK));
		val |= ((nDisp & 0x3) << MISC_SDVESEL_SHIFT);
		break;
	case VIOC_OUTCFG_HDVENC:
		val = (__raw_readl(reg + MISC) & ~(MISC_HDVESEL_MASK));
		val |= ((nDisp & 0x3) << MISC_HDVESEL_SHIFT);
		break;
	case VIOC_OUTCFG_M80:
		val = (__raw_readl(reg + MISC) & ~(MISC_M80SEL_MASK));
		val |= ((nDisp & 0x3) << MISC_M80SEL_SHIFT);
		break;
	case VIOC_OUTCFG_MRGB:
		val = (__raw_readl(reg + MISC) & ~(MISC_MRGBSEL_MASK));
		val |= ((nDisp & 0x3) << MISC_MRGBSEL_SHIFT);
		break;
	default:
		pr_err("[ERR][OUTCFG] %s, wrong type(0x%08x)\n", __func__, nType);
		WARN_ON(1);
		return;
		break;
	}

	/* MISC[7:6] is read-only bits. So we need below code. */
	#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
	#if defined(CONFIG_TCC_DISPLAY_MODE_DUAL_HDMI_CVBS) || defined(CONFIG_TCC_DISPLAY_MODE_DUAL_AUTO)
	val |= (1 << MISC_BVO_SDVESEL_SHIFT);
	#endif
	#endif

	__raw_writel(val, reg + MISC);

	dprintk("VIOC_OUTCFG_SetOutConfig(OUTCFG.MISC=0x%08lx)\n", val);
}

#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
void VIOC_OUTCFG_BVO_SetOutConfig(unsigned int nDisp)
{
	volatile void __iomem *reg = VIOC_OUTCONFIG_GetAddress();
	unsigned long val;

	val = (__raw_readl(reg + MISC) & ~(MISC_BVO_SDVESEL_MASK));
	val |= ((nDisp & 0x3) << MISC_BVO_SDVESEL_SHIFT);
	__raw_writel(val, reg + MISC);

	dprintk("VIOC_OUTCFG_BVO_SetOutConfig(OUTCFG.MISC=0x%08lx)\n", val);
}
#endif

volatile void __iomem *VIOC_OUTCONFIG_GetAddress(void)
{
	if (pOUTCFG_reg == NULL)
		pr_err("[ERR][OUTCFG] %s pOUTCFG_reg is NULL \n", __func__);

	return pOUTCFG_reg;
}

void VIOC_OUTCONFIG_DUMP(void)
{
	unsigned int cnt = 0;
	volatile void __iomem *pReg = VIOC_OUTCONFIG_GetAddress();

	pr_debug("[DBG][OUTCFG] %p \n", pReg);
	while (cnt < 0x10) {
		pr_debug("[DBG][OUTCFG] 0x%p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4),
		       __raw_readl(pReg + cnt + 0x8),
		       __raw_readl(pReg + cnt + 0xC));
		cnt += 0x10;
	}
}

static int __init vioc_outputconfig_init(void)
{
	struct device_node *ViocOutputConfig_np;

	ViocOutputConfig_np =
		of_find_compatible_node(NULL, NULL, "telechips,output_config");
	if (ViocOutputConfig_np == NULL) {
		pr_info("[INF][OUTCFG] vioc-outcfg: disabled [this is mandatory for vioc display]\n");
	} else {
		pOUTCFG_reg = of_iomap(ViocOutputConfig_np, 0);

		if (pOUTCFG_reg)
				pr_info("[INF][OUTCFG] vioc-outcfg: 0x%p\n", pOUTCFG_reg);
	}
	return 0;
}
arch_initcall(vioc_outputconfig_init);

// vim:ts=4:et:sw=4:sts=4
