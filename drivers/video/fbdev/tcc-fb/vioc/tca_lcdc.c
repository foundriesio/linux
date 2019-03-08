/*
*   tca_lcdc.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC lcd Driver
 *
 *   Copyright (C) 2008-2009 Telechips
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

#include <linux/module.h>
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
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

#include <video/tcc/tcc_fb.h>
#include <video/tcc/tca_lcdc.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/vioc_outcfg.h>
#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_disp.h>
#include <video/tcc/tccfb.h>

#if defined(CONFIG_TCC_VIOCMG)
#include <video/tcc/viocmg.h>
#endif

void tca_lcdc_interrupt_onoff(char onoff, char lcdc)
{
	volatile void __iomem *pDISPBase;
	unsigned int mask = VIOC_DISP_INT_MASK;

	pDISPBase = VIOC_DISP_GetAddress(lcdc);
	if (pDISPBase == NULL) {
		pr_err("%s: pDISPBase is null\n", __func__);
		return;
	}

	// init INT mask reg
	// __raw_writel((mask & ~VIOC_DISP_INTR_DISPLAY), pDISPBase + DIM);

#if defined(CONFIG_TCC_M2M_USE_INTERLACE_OUTPUT)
	if (__raw_readl(pDISPBase + DCTRL) & DCTRL_NI_MASK)
#endif
		mask = (VIOC_DISP_IREQ_RU_MASK);

#if defined(CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION)
	mask |= (VIOC_DISP_IREQ_FU_MASK | VIOC_DISP_IREQ_DD_MASK);
#endif

	if (onoff) // VIOC INT en
	{
		VIOC_DISP_SetIreqMask(pDISPBase, mask, 0);
	} else // VIOC INT dis
	{
		VIOC_DISP_SetIreqMask(pDISPBase, mask, mask);
	}
}
EXPORT_SYMBOL(tca_lcdc_interrupt_onoff);

void lcdc_initialize(struct lcd_panel *lcd_spec, struct tcc_dp_device *pdata)
{
	volatile void __iomem *pDISPBase;
	volatile void __iomem *pWMIXBase;
	unsigned int default_ovp = 24;
	unsigned long val;

	if (pdata->DispOrder == DD_SUB) {
#if defined(CONFIG_TCC_DISPLAY_HDMI_LVDS)
		default_ovp = 20; // T: 1-2-3-0 :B
#endif
	}
#if defined(CONFIG_TCC_VIOCMG)
	else
		default_ovp = viocmg_get_main_display_ovp();
#endif

	pDISPBase = pdata->ddc_info.virt_addr;
	pWMIXBase = pdata->wmixer_info.virt_addr;

	VIOC_WMIX_SetSize(pWMIXBase, lcd_spec->xres, lcd_spec->yres);

#if defined(CONFIG_TCC_VIOCMG)
	viocmg_set_wmix_ovp(VIOCMG_CALLERID_FB, pdata->wmixer_info.blk_num,
			    default_ovp);
#else
	VIOC_WMIX_SetOverlayPriority(pWMIXBase, default_ovp);
#endif

	VIOC_WMIX_SetBGColor(pWMIXBase, 0, 0, 0, 0);
	__raw_writel((__raw_readl(pDISPBase + DCTRL) & ~(DCTRL_Y2R_MASK)),
		     pDISPBase + DCTRL);

	if (lcd_spec->bus_width == 24)
		VIOC_DISP_SetPXDW(pDISPBase, 0xC);
	else if (lcd_spec->bus_width == 18)
		VIOC_DISP_SetPXDW(pDISPBase, 0x5);
	else
		VIOC_DISP_SetPXDW(pDISPBase, 0x3);

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	if (lcd_spec->bus_width <= 24)
		VIOC_DISP_SetAlign(pDISPBase, 0x1); // Convert to 10-to-8 bits bus
	else
		VIOC_DISP_SetAlign(pDISPBase, 0x0);
#endif

	if (lcd_spec->sync_invert & ID_INVERT)
		__raw_writel(
			((__raw_readl(pDISPBase + DCTRL) & ~(DCTRL_ID_MASK)) |
			 0x1 << DCTRL_ID_SHIFT),
			pDISPBase + DCTRL);

	if (lcd_spec->sync_invert & IV_INVERT)
		__raw_writel(
			((__raw_readl(pDISPBase + DCTRL) & ~(DCTRL_IV_MASK)) |
			 0x1 << DCTRL_IV_SHIFT),
			pDISPBase + DCTRL);

	if (lcd_spec->sync_invert & IH_INVERT)
		__raw_writel(
			((__raw_readl(pDISPBase + DCTRL) & ~(DCTRL_IH_MASK)) |
			 0x1 << DCTRL_IH_SHIFT),
			pDISPBase + DCTRL);

	if (lcd_spec->sync_invert & IP_INVERT)
		__raw_writel(
			((__raw_readl(pDISPBase + DCTRL) & ~(DCTRL_IP_MASK)) |
			 0x1 << DCTRL_IP_SHIFT),
			pDISPBase + DCTRL);

	val = (__raw_readl(pDISPBase + DCTRL) &
	       ~(DCTRL_Y2RMD_MASK | DCTRL_DP_MASK | DCTRL_NI_MASK));
	val |= ((0x1 << DCTRL_Y2RMD_SHIFT) | (0x0 << DCTRL_DP_SHIFT) | (0x1 << DCTRL_NI_SHIFT));

	__raw_writel(val, pDISPBase + DCTRL);


	__raw_writel((lcd_spec->clk_div / 2) << DCLKDIV_PXCLKDIV_SHIFT,
		     pDISPBase + DCLKDIV);

	#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X)
	__raw_writel(0x00000000, pDISPBase + DBG0);
	__raw_writel(0x00000000, pDISPBase + DBG1);
	#else
	__raw_writel(0x00000000, pDISPBase + DBC);
	#endif

	val = (((lcd_spec->xres - 1) << DHTIME1_LPC_SHIFT) |
	       (lcd_spec->lpw << DHTIME1_LPW_SHIFT));
	__raw_writel(val, pDISPBase + DHTIME1);

	val = ((lcd_spec->lswc << DHTIME2_LSWC_SHIFT) |
	       (lcd_spec->lewc << DHTIME2_LEWC_SHIFT));
	__raw_writel(val, pDISPBase + DHTIME2);

	val = (__raw_readl(pDISPBase + DVTIME1) &
	       ~(DVTIME1_FPW_MASK | DVTIME1_FLC_MASK));
	val |= ((lcd_spec->fpw1 << DVTIME1_FPW_SHIFT) |
		((lcd_spec->yres - 1) << DVTIME1_FLC_SHIFT));
	__raw_writel(val, pDISPBase + DVTIME1);

	val = ((lcd_spec->fswc1 << DVTIME2_FSWC_SHIFT) |
	       (lcd_spec->fewc1 << DVTIME2_FEWC_SHIFT));
	__raw_writel(val, pDISPBase + DVTIME2);

	val = ((lcd_spec->fpw2 << DVTIME3_FPW_SHIFT) |
	       ((lcd_spec->yres - 1) << DVTIME3_FLC_SHIFT));
	__raw_writel(val, pDISPBase + DVTIME3);

	val = ((lcd_spec->fswc2 << DVTIME4_FSWC_SHIFT) |
	       (lcd_spec->fewc2 << DVTIME4_FEWC_SHIFT));
	__raw_writel(val, pDISPBase + DVTIME4);

	val = ((lcd_spec->yres << DDS_VSIZE_SHIFT) |
	       (lcd_spec->xres << DDS_HSIZE_SHIFT));
	__raw_writel(val, pDISPBase + DDS);

// pjj
#if 0
	val = (__raw_readl(pDISPBase+DCTRL) & ~(DCTRL_LEN_MASK));
	val |= (0x1 << DCTRL_LEN_SHIFT);
	__raw_writel(val, pDISPBase+DCTRL);
#endif
}

EXPORT_SYMBOL(lcdc_initialize);

void tcc_lcdc_dithering_setting(struct tcc_dp_device *pdata)
{
	volatile void __iomem *reg = pdata->ddc_info.virt_addr;
	unsigned long val;

	/* dithering option */
	val = (__raw_readl(reg + DCTRL) & ~(DCTRL_PXDW_MASK));
	val |= (0x5 << DCTRL_PXDW_SHIFT);
	__raw_writel(val, reg + DCTRL);

	__raw_writel(0xC0000000, reg + DDITH);
	__raw_writel(0x6e4ca280, reg + DDMAT0);
	__raw_writel(0x5d7f91b3, reg + DDMAT1);
}
