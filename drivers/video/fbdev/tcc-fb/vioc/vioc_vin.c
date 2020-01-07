/*
 * linux/arch/arm/mach-tcc893x/vioc_vin.c
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
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_vin.h>

#define VIOC_VIN_IREQ_UPD_MASK 0x00000001UL
#define VIOC_VIN_IREQ_EOF_MASK 0x00000002UL
#define VIOC_VIN_IREQ_VS_MASK 0x00000004UL
#define VIOC_VIN_IREQ_INVS_MASK 0x00000008UL

static volatile void __iomem *pVIN_reg[VIOC_VIN_MAX] = {0};

/* VIN polarity Setting */
void VIOC_VIN_SetSyncPolarity(volatile void __iomem *reg,
			      unsigned int hs_active_low,
			      unsigned int vs_active_low,
			      unsigned int field_bfield_low,
			      unsigned int de_active_low,
			      unsigned int gen_field_en, unsigned int pxclk_pol)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_CTRL) &
	       ~(VIN_CTRL_GFEN_MASK | VIN_CTRL_DEAL_MASK | VIN_CTRL_FOL_MASK |
		 VIN_CTRL_VAL_MASK | VIN_CTRL_HAL_MASK | VIN_CTRL_PXP_MASK));
	val |= (((gen_field_en & 0x1) << VIN_CTRL_GFEN_SHIFT) |
		((de_active_low & 0x1) << VIN_CTRL_DEAL_SHIFT) |
		((field_bfield_low & 0x1) << VIN_CTRL_FOL_SHIFT) |
		((vs_active_low & 0x1) << VIN_CTRL_VAL_SHIFT) |
		((hs_active_low & 0x1) << VIN_CTRL_HAL_SHIFT) |
		((pxclk_pol & 0x1) << VIN_CTRL_PXP_SHIFT));
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Configuration 1 */
void VIOC_VIN_SetCtrl(volatile void __iomem *reg, unsigned int conv_en,
		      unsigned int hsde_connect_en, unsigned int vs_mask,
		      unsigned int fmt, unsigned int data_order)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_CTRL) &
	       ~(VIN_CTRL_CONV_MASK | VIN_CTRL_HDCE_MASK | VIN_CTRL_VM_MASK |
		 VIN_CTRL_FMT_MASK | VIN_CTRL_DO_MASK));
	val |= (((conv_en & 0x1) << VIN_CTRL_CONV_SHIFT) |
		((hsde_connect_en & 0x1) << VIN_CTRL_HDCE_SHIFT) |
		((vs_mask & 0x1) << VIN_CTRL_VM_SHIFT) |
		((fmt & 0xF) << VIN_CTRL_FMT_SHIFT) |
		((data_order & 0x7) << VIN_CTRL_DO_SHIFT));
	__raw_writel(val, reg + VIN_CTRL);
}

/* Interlace mode setting */
void VIOC_VIN_SetInterlaceMode(volatile void __iomem *reg, unsigned int intl_en,
			       unsigned int intpl_en)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_CTRL) &
	       ~(VIN_CTRL_INTEN_MASK | VIN_CTRL_INTPLEN_MASK));
	val |= (((intl_en & 0x1) << VIN_CTRL_INTEN_SHIFT) |
		((intpl_en & 0x1) << VIN_CTRL_INTPLEN_SHIFT));
	__raw_writel(val, reg);
}

/* VIN Capture mode Enable */
void VIOC_VIN_SetCaptureModeEnable(volatile void __iomem *reg,
				   unsigned int cap_en)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_CTRL) & ~(VIN_CTRL_CP_MASK));
	val |= ((cap_en & 0x1) << VIN_CTRL_CP_SHIFT);
	__raw_writel(val, reg + VIN_CTRL);
}

/* VIN Enable/Disable */
void VIOC_VIN_SetEnable(volatile void __iomem *reg, unsigned int vin_en)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_CTRL) & ~(VIN_CTRL_EN_MASK));
	val |= ((vin_en & 0x1) << VIN_CTRL_EN_SHIFT);
	__raw_writel(val, reg + VIN_CTRL);
}

unsigned int VIOC_VIN_IsEnable(volatile void __iomem *reg)
{
	return ((__raw_readl(reg + VIN_CTRL) & (VIN_CTRL_EN_MASK)) >>
		VIN_CTRL_EN_SHIFT);
}

/* Image size setting */
void VIOC_VIN_SetImageSize(volatile void __iomem *reg, unsigned int width,
			   unsigned int height)
{
	unsigned long val;
	val = (((height & 0xFFFF) << VIN_SIZE_HEIGHT_SHIFT) |
	       ((width & 0xFFFF) << VIN_SIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + VIN_SIZE);
}

/* Image offset setting */
void VIOC_VIN_SetImageOffset(volatile void __iomem *reg,
			     unsigned int offs_width, unsigned int offs_height,
			     unsigned int offs_height_intl)
{
	unsigned long val;
	val = (((offs_height & 0xFFFF) << VIN_OFFS_OFS_HEIGHT_SHIFT) |
	       ((offs_width & 0xFFFF) << VIN_OFFS_OFS_WIDTH_SHIFT));
	__raw_writel(val, reg + VIN_OFFS);

	val = ((offs_height_intl & 0xFFFF) << VIN_OFFS_INTL_OFS_HEIGHT_SHIFT);
	__raw_writel(val, reg + VIN_OFFS_INTL);
}

void VIOC_VIN_SetImageCropSize(volatile void __iomem *reg, unsigned int width,
			       unsigned int height)
{
	unsigned long val;
	val = (((height & 0xFFFF) << VIN_CROP_SIZE_HEIGHT_SHIFT) |
	       ((width & 0xFFFF) << VIN_CROP_SIZE_WIDTH_SHIFT));
	__raw_writel(val, reg + VIN_CROP_SIZE);
}

void VIOC_VIN_SetImageCropOffset(volatile void __iomem *reg,
				 unsigned int offs_width,
				 unsigned int offs_height)
{
	unsigned long val;
	val = (((offs_height & 0xFFFF) << VIN_CROP_OFFS_OFS_HEIGHT_SHIFT) |
	       ((offs_width & 0xFFFF) << VIN_CROP_OFFS_OFS_WIDTH_SHIFT));
	__raw_writel(val, reg + VIN_CROP_OFFS);
}

/* Y2R conversion mode setting */
void VIOC_VIN_SetY2RMode(volatile void __iomem *reg, unsigned int y2r_mode)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_Y2RM_MASK));
	val |= ((y2r_mode & 0x7) << VIN_MISC_Y2RM_SHIFT);
	__raw_writel(val, reg + VIN_MISC);
}

/* Y2R conversion Enable/Disable */
void VIOC_VIN_SetY2REnable(volatile void __iomem *reg, unsigned int y2r_en)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_Y2REN_MASK));
	val |= ((y2r_en & 0x1) << VIN_MISC_Y2REN_SHIFT);
	__raw_writel(val, reg + VIN_MISC);
}

/* initialize LUT, for example, LUT values are set to inverse function. */
void VIOC_VIN_SetLUT(volatile void __iomem *reg, unsigned int *pLUT)
{
	unsigned int *pLUT0, *pLUT1, *pLUT2, uiCount;

	pLUT0 = (unsigned int *)(pLUT + 0);
	pLUT1 = (unsigned int *)(pLUT0 + 256 / 4);
	pLUT2 = (unsigned int *)(pLUT1 + 256 / 4);
	__raw_writel(
		((__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_LUTIF_MASK)) |
		 (0x1 << VIN_MISC_LUTIF_SHIFT)),
		reg + VIN_MISC); /* Access Look-Up Table Using Slave Port */

	for (uiCount = 0; uiCount < 256;
	     uiCount = uiCount + 4) { /* Initialize Look-up Table */
		*pLUT0++ = ((uiCount + 3) << 24) | ((uiCount + 2) << 16) |
			   ((uiCount + 1) << 8) | ((uiCount + 0) << 0);
		*pLUT1++ = ((uiCount + 3) << 24) | ((uiCount + 2) << 16) |
			   ((uiCount + 1) << 8) | ((uiCount + 0) << 0);
		*pLUT2++ = ((uiCount + 3) << 24) | ((uiCount + 2) << 16) |
			   ((uiCount + 1) << 8) | ((uiCount + 0) << 0);
	}

	__raw_writel(
		(__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_LUTIF_MASK)),
		reg + VIN_MISC); /* Access Look-Up Table Using Vin Module */
}

/* LUT Enable/Disable */
void VIOC_VIN_SetLUTEnable(volatile void __iomem *reg, unsigned int lut0_en,
			   unsigned int lut1_en, unsigned int lut2_en)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_LUTEN_MASK));
	val |= (((lut0_en & 0x1) << VIN_MISC_LUTEN_SHIFT) |
		((lut1_en & 0x1) << (VIN_MISC_LUTEN_SHIFT + 1)) |
		((lut2_en & 0x1) << (VIN_MISC_LUTEN_SHIFT + 2)));
	__raw_writel(val, reg + VIN_MISC);
}

void VIOC_VIN_SetDemuxPort(volatile void __iomem *reg, unsigned int p0,
			   unsigned int p1, unsigned int p2, unsigned int p3)
{
	unsigned long val;
	val = (__raw_readl(reg + VD_CTRL) &
	       ~(VD_CTRL_SEL3_MASK | VD_CTRL_SEL2_MASK | VD_CTRL_SEL1_MASK |
		 VD_CTRL_SEL0_MASK));
	val |= (((p0 & 0x7) << VD_CTRL_SEL0_SHIFT) |
		((p1 & 0x7) << VD_CTRL_SEL1_SHIFT) |
		((p2 & 0x7) << VD_CTRL_SEL2_SHIFT) |
		((p3 & 0x7) << VD_CTRL_SEL3_SHIFT));
	__raw_writel(val, reg + VD_CTRL);
}

void VIOC_VIN_SetDemuxClock(volatile void __iomem *reg, unsigned int mode)
{
	unsigned long val;
	val = (__raw_readl(reg + VD_CTRL) & ~(VD_CTRL_CM_MASK));
	val |= ((mode & 0x7) << VD_CTRL_CM_SHIFT);
	__raw_writel(val, reg + VD_CTRL);
}

void VIOC_VIN_SetDemuxEnable(volatile void __iomem *reg, unsigned int enable)
{
	unsigned long val;
	val = (__raw_readl(reg + VD_CTRL) & ~(VD_CTRL_EN_MASK));
	val |= ((enable & 0x1) << VD_CTRL_EN_SHIFT);
	__raw_writel(val, reg + VD_CTRL);
}

void VIOC_VIN_SetSEEnable(volatile void __iomem *reg, unsigned int se)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_CTRL) &
	       ~(VIN_CTRL_SE_MASK));
	val |= (((se & 0x1) << VIN_CTRL_SE_SHIFT));
	__raw_writel(val, reg + VIN_CTRL);
}

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
void VIOC_VIN_SetFlushBufferEnable(volatile void __iomem *reg, unsigned int fvs)
{
	unsigned long val;
	val = (__raw_readl(reg + VIN_MISC) & ~(VIN_MISC_FVS_MASK));
	val |= ((fvs & 0x1) << VIN_MISC_FVS_SHIFT);
	__raw_writel(val, reg + VIN_MISC);
}
#endif

volatile void __iomem *VIOC_VIN_GetAddress(unsigned int Num)
{
	Num = get_vioc_index(Num);
	if (Num >= VIOC_VIN_MAX)
		goto err;

	if (pVIN_reg[Num] == NULL)
		pr_err("num:%d ADDRESS NULL \n", Num);

	return pVIN_reg[Num];

err:
	pr_err("err %s Num:%d , max :%d \n", __func__, Num, VIOC_VIN_MAX);
	return NULL;
}

static int __init vioc_vin_init(void)
{
	int i;
	struct device_node *ViocVin_np;

	ViocVin_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_vin");
	if (ViocVin_np == NULL) {
		pr_info("vioc-vin: disabled\n");
	} else {
		for (i = 0; i < VIOC_VIN_MAX; i++) {
			pVIN_reg[i] = (volatile void __iomem *)of_iomap(ViocVin_np,
							is_VIOC_REMAP ? (i + VIOC_VIN_MAX) : i);

			if (pVIN_reg[i])
				pr_info("vioc-vin%d: 0x%p\n", i, pVIN_reg[i]);
		}
	}

	return 0;
}
arch_initcall(vioc_vin_init);
