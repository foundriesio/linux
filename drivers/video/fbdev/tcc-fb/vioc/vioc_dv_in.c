/*
 * linux/include/video/tcc/vioc_dv_in.c
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

#include <video/tcc/vioc_v_dv.h>

struct device_node *pDVIN_np;
volatile void __iomem *pDVIN_reg = NULL;

#define __dvin_reg_r __raw_readl
#define __dvin_reg_w __raw_writel

/* VIN polarity Setting */
void VIOC_DV_IN_SetSyncPolarity(
				unsigned int hs_active_low,
				unsigned int vs_active_low,
				unsigned int field_bfield_low,
				unsigned int de_active_low,
				unsigned int gen_field_en,
				unsigned int pxclk_pol)
{
	unsigned long val;
	val = (__dvin_reg_r(pDVIN_reg + DV_IN_CTRL) &
	       ~(DV_IN_CTRL_GFEN_MASK | DV_IN_CTRL_DEAL_MASK |
		 DV_IN_CTRL_FOL_MASK | DV_IN_CTRL_VAL_MASK |
		 DV_IN_CTRL_HAL_MASK | DV_IN_CTRL_PXP_MASK));
	val |= (((gen_field_en & 0x1) << DV_IN_CTRL_GFEN_SHIFT) |
		((de_active_low & 0x1) << DV_IN_CTRL_DEAL_SHIFT) |
		((field_bfield_low & 0x1) << DV_IN_CTRL_FOL_SHIFT) |
		((vs_active_low & 0x1) << DV_IN_CTRL_VAL_SHIFT) |
		((hs_active_low & 0x1) << DV_IN_CTRL_HAL_SHIFT) |
		((pxclk_pol & 0x1) << DV_IN_CTRL_PXP_SHIFT));
	__dvin_reg_w(val, pDVIN_reg + DV_IN_CTRL);
}

/* VIN Configuration 1 */
void VIOC_DV_IN_SetCtrl( unsigned int conv_en,
			unsigned int hsde_connect_en, unsigned int vs_mask,
			unsigned int fmt, unsigned int data_order)
{
	unsigned long val;
	val = (__dvin_reg_r(pDVIN_reg + DV_IN_CTRL) &
	       ~(DV_IN_CTRL_CONV_MASK | DV_IN_CTRL_HDCE_MASK |
		 DV_IN_CTRL_VM_MASK | DV_IN_CTRL_FMT_MASK |
		 DV_IN_CTRL_DO_MASK));
	val |= (((conv_en & 0x1) << DV_IN_CTRL_CONV_SHIFT) |
		((hsde_connect_en & 0x1) << DV_IN_CTRL_HDCE_SHIFT) |
		((vs_mask & 0x1) << DV_IN_CTRL_VM_SHIFT) |
		((fmt & 0xF) << DV_IN_CTRL_FMT_SHIFT) |
		((data_order & 0x7) << DV_IN_CTRL_DO_SHIFT));
	__dvin_reg_w(val, pDVIN_reg + DV_IN_CTRL);
}

/* Interlace mode setting */
void VIOC_DV_IN_SetInterlaceMode(
				 unsigned int intl_en, unsigned int intpl_en)
{
	unsigned long val;
	val = (__dvin_reg_r(pDVIN_reg + DV_IN_CTRL) &
	       ~(DV_IN_CTRL_INTEN_MASK | DV_IN_CTRL_INTPLEN_MASK));
	val |= (((intl_en & 0x1) << DV_IN_CTRL_INTEN_SHIFT) |
		((intpl_en & 0x1) << DV_IN_CTRL_INTPLEN_SHIFT));
	__dvin_reg_w(val, pDVIN_reg);
}

/* VIN Capture mode Enable */
void VIOC_DV_IN_SetCaptureModeEnable(
				     unsigned int cap_en)
{
	unsigned long val;
	val = (__dvin_reg_r(pDVIN_reg + DV_IN_CTRL) & ~(DV_IN_CTRL_CP_MASK));
	val |= ((cap_en & 0x1) << DV_IN_CTRL_CP_SHIFT);
	__dvin_reg_w(val, pDVIN_reg + DV_IN_CTRL);
}

/* VIN Enable/Disable */
void VIOC_DV_IN_SetEnable( unsigned int vin_en)
{
	unsigned long val;

	dprintk_dv_sequence("### DV_IN %s \n", vin_en ? "On" : "Off");

	val = (__dvin_reg_r(pDVIN_reg + DV_IN_CTRL) & ~(DV_IN_CTRL_EN_MASK));
	val |= ((vin_en & 0x1) << DV_IN_CTRL_EN_SHIFT);
	__dvin_reg_w(val, pDVIN_reg + DV_IN_CTRL);
}

unsigned int VIOC_DV_IN_IsEnable(volatile void __iomem *reg)
{
	return ((__dvin_reg_r(pDVIN_reg + DV_IN_CTRL) & (DV_IN_CTRL_EN_MASK)) >>
		DV_IN_CTRL_EN_SHIFT);
}

void VIOC_DV_IN_SetVsyncFlush(unsigned int bflush)
{
	unsigned long val;
	val = (__raw_readl(pDVIN_reg + DV_IN_MISC) & ~(DV_IN_MISC_FVS_MASK));
	val |= ((bflush & 0x1) << DV_IN_MISC_FVS_SHIFT);
	__raw_writel(val, pDVIN_reg + DV_IN_MISC);
}


/* Image size setting */
void VIOC_DV_IN_SetImageSize( unsigned int width,
			     unsigned int height)
{
	unsigned long val;
	val = (((height & 0xFFFF) << DV_IN_SIZE_HEIGHT_SHIFT) |
	       ((width & 0xFFFF) << DV_IN_SIZE_WIDTH_SHIFT));
	__dvin_reg_w(val, pDVIN_reg + DV_IN_SIZE);
}

/* Image offset setting */
void VIOC_DV_IN_SetImageOffset(
			       unsigned int offs_width,
			       unsigned int offs_height,
			       unsigned int offs_height_intl)
{
	unsigned long val;
	val = (((offs_height & 0xFFFF) << DV_IN_OFFS_OFS_HEIGHT_SHIFT) |
	       ((offs_width & 0xFFFF) << DV_IN_OFFS_OFS_WIDTH_SHIFT));
	__dvin_reg_w(val, pDVIN_reg + DV_IN_OFFS);

	val = ((offs_height_intl & 0xFFFF) << DV_IN_OFFS_INTL_OFS_HEIGHT_SHIFT);
	__dvin_reg_w(val, pDVIN_reg + DV_IN_OFFS_INTL);
}

/* VIN Enable/Disable */
void VIOC_DV_IN_SetTunneling(
			     unsigned int tunneling_en)
{
	unsigned long val;
	val = (__dvin_reg_r(pDVIN_reg + DV_IN_TUNNELING) &
	       ~(DV_IN_TUNNELING_TN_EN_MASK));
	val |= ((tunneling_en & 0x1) << DV_IN_TUNNELING_TN_EN_SHIFT);
	__dvin_reg_w(val, pDVIN_reg + DV_IN_TUNNELING);
}

/* VIN Enable/Disable */
void VIOC_DV_IN_SetSwap(
			     unsigned int mode)
{
	unsigned long val;
	val = (__dvin_reg_r(pDVIN_reg + DV_IN_TUNNELING) &
	       ~(DV_IN_TUNNELING_SWAP_MODE_MASK));
	val |= ((mode & 0x7) << DV_IN_TUNNELING_SWAP_MODE_SHIFT);
	__dvin_reg_w(val, pDVIN_reg + DV_IN_TUNNELING);
}

volatile void __iomem *VIOC_DV_IN_GetAddress(void)
{
	if (pDVIN_reg == NULL){
		pr_err("[ERR][DV_IN] %s \n", __func__);
		goto err;
	}

	return pDVIN_reg;

err:
	pr_err("[ERR][DV_IN] %s no remapped register \n", __func__);
	return NULL;
}

void VIOC_DV_IN_Configure(unsigned int in_width, unsigned int in_height, unsigned int in_fmt, unsigned int bTunneling)
{
	VIOC_DV_IN_SetSyncPolarity(1, 1, 0, 0, 0, 0);
	VIOC_DV_IN_SetCtrl(0, 0, 0, in_fmt, 0);
	VIOC_DV_IN_SetInterlaceMode(DV_IN_OFF, DV_IN_OFF);
	VIOC_DV_IN_SetCaptureModeEnable(DV_IN_OFF);

	VIOC_DV_IN_SetImageSize(in_width, in_height);
	VIOC_DV_IN_SetImageOffset(0, 0, 0);
//	VIOC_DV_IN_SetVsyncFlush(DV_IN_ON);
	VIOC_DV_IN_SetTunneling(bTunneling);
	VIOC_DV_IN_SetSwap(0);
}

void VIOC_DV_IN_DUMP(void)
{
	volatile void __iomem *pReg;
	unsigned int cnt = 0;

	if (pDVIN_np == NULL)
		return;

	pReg = VIOC_DV_IN_GetAddress();

	pr_debug("[DBG][DV_IN] %p \n", pReg);
	while (cnt < 0x20) {
		pr_debug("[DBG][DV_IN] %p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + cnt,
		       __dvin_reg_r(pReg + cnt), __dvin_reg_r(pReg + cnt + 0x4),
		       __dvin_reg_r(pReg + cnt + 0x8),
		       __dvin_reg_r(pReg + cnt + 0xC));
		cnt += 0x10;
	}
	pr_debug("[DBG][DV_IN] %p: 0x%08x 0x%08x 0x%08x 0x%08x \n", pReg + 0x60,
	       __dvin_reg_r(pReg + 0x60), __dvin_reg_r(pReg + 0x60 + 0x4),
	       __dvin_reg_r(pReg + 0x60 + 0x8),
	       __dvin_reg_r(pReg + 0x60 + 0xC));
}

static int __init vioc_dvin_init(void)
{
	pDVIN_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_dv_in");
	if (pDVIN_np == NULL) {
		pr_info("[INF][DV_IN] disabled\n");
	} else {
		pDVIN_reg = of_iomap(pDVIN_np, 0);
		if (pDVIN_reg)
			pr_info("[INF][DV_IN] 0x%p\n", pDVIN_reg);

	}

	return 0;
}
arch_initcall(vioc_dvin_init);
