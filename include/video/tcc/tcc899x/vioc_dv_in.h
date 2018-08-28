/*
 * linux/include/video/tcc/vioc_dv_in.h
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

#ifndef _VIOC_DV_IN_H_
#define	_VIOC_DV_IN_H_

#define DV_IN_ON	(1)
#define DV_IN_OFF 	(0)

#define FMT_DV_IN_RGB444_24BIT (0x9)

/*
 * Register offset
 */
#define	DV_IN_CTRL			(0x000)
#define	DV_IN_MISC			(0x004)
#define	DV_IN_SIZE			(0x010)
#define	DV_IN_OFFS			(0x014)
#define	DV_IN_OFFS_INTL		(0x018)
#define	DV_IN_INT			(0x060)
#define DV_IN_TUNNELING		(0x064)

/*
 * DV_IN Control Register
 */
#define DV_IN_CTRL_CP_SHIFT			(31)
#define DV_IN_CTRL_SKIP_SHIFT		(24)
#define DV_IN_CTRL_DO_SHIFT			(20)
#define DV_IN_CTRL_FMT_SHIFT		(16)
#define DV_IN_CTRL_SE_SHIFT			(14)
#define DV_IN_CTRL_GFEN_SHIFT		(13)
#define DV_IN_CTRL_DEAL_SHIFT		(12)
#define DV_IN_CTRL_FOL_SHIFT		(11)
#define DV_IN_CTRL_VAL_SHIFT		(10)
#define DV_IN_CTRL_HAL_SHIFT		(9)
#define DV_IN_CTRL_PXP_SHIFT		(8)
#define DV_IN_CTRL_VM_SHIFT			(6)
#define DV_IN_CTRL_FLUSH_SHIFT		(5)
#define DV_IN_CTRL_HDCE_SHIFT		(4)
#define DV_IN_CTRL_INTPLEN_SHIFT 	(3)
#define DV_IN_CTRL_INTEN_SHIFT		(2)
#define DV_IN_CTRL_CONV_SHIFT		(1)
#define DV_IN_CTRL_EN_SHIFT			(0)

#define DV_IN_CTRL_CP_MASK		(0x1 << DV_IN_CTRL_CP_SHIFT)
#define DV_IN_CTRL_SKIP_MASK	(0xF << DV_IN_CTRL_SKIP_SHIFT)
#define DV_IN_CTRL_DO_MASK		(0x3 << DV_IN_CTRL_DO_SHIFT)
#define DV_IN_CTRL_FMT_MASK		(0x3 << DV_IN_CTRL_FMT_SHIFT)
#define DV_IN_CTRL_SE_MASK		(0x1 << DV_IN_CTRL_SE_SHIFT)
#define DV_IN_CTRL_GFEN_MASK	(0x1 << DV_IN_CTRL_GFEN_SHIFT)
#define DV_IN_CTRL_DEAL_MASK	(0x1 << DV_IN_CTRL_DEAL_SHIFT)
#define DV_IN_CTRL_FOL_MASK		(0x1 << DV_IN_CTRL_FOL_SHIFT)
#define DV_IN_CTRL_VAL_MASK		(0x1 << DV_IN_CTRL_VAL_SHIFT)
#define DV_IN_CTRL_HAL_MASK		(0x1 << DV_IN_CTRL_HAL_SHIFT)
#define DV_IN_CTRL_PXP_MASK		(0x1 << DV_IN_CTRL_PXP_SHIFT)
#define DV_IN_CTRL_VM_MASK		(0x1 << DV_IN_CTRL_VM_SHIFT)
#define DV_IN_CTRL_FLUSH_MASK	(0x1 << DV_IN_CTRL_FLUSH_SHIFT)
#define DV_IN_CTRL_HDCE_MASK	(0x1 << DV_IN_CTRL_HDCE_SHIFT)
#define DV_IN_CTRL_INTPLEN_MASK	(0x1 << DV_IN_CTRL_INTPLEN_SHIFT)
#define DV_IN_CTRL_INTEN_MASK	(0x1 << DV_IN_CTRL_INTEN_SHIFT)
#define DV_IN_CTRL_CONV_MASK	(0x1 << DV_IN_CTRL_CONV_SHIFT)
#define DV_IN_CTRL_EN_MASK		(0x1 << DV_IN_CTRL_EN_SHIFT)

/*
 * DV_IN Misc. Register
 */
#define DV_IN_MISC_VS_DELAY_SHIFT	(20)
#define DV_IN_MISC_FVS_SHIFT		(16)

#define DV_IN_MISC_VS_DELAY_MASK	(0xF << DV_IN_MISC_VS_DELAY_SHIFT)
#define DV_IN_MISC_FVS_MASK			(0x1 << DV_IN_MISC_FVS_SHIFT)

/*
 * DV_IN Size Register
 */
#define DV_IN_SIZE_HEIGHT_SHIFT		(16)
#define DV_IN_SIZE_WIDTH_SHIFT		(0)

#define DV_IN_SIZE_HEIGHT_MASK		(0xFFFF << DV_IN_SIZE_HEIGHT_SHIFT)
#define DV_IN_SIZE_WIDTH_MASK		(0xFFFF << DV_IN_SIZE_WIDTH_SHIFT)

/*
 * DV_IN Offset Register
 */
#define DV_IN_OFFS_OFS_HEIGHT_SHIFT		(16)
#define DV_IN_OFFS_OFS_WIDTH_SHIFT		(0)

#define DV_IN_OFFS_OFS_HEIGHT_MASK		(0xFFFF << DV_IN_OFFS_OFS_HEIGHT_SHIFT)
#define DV_IN_OFFS_OFS_WIDTH_MASK		(0xFFFF << DV_IN_OFFS_OFS_WIDTH_SHIFT)

/*
 * DV_IN Offset in Interlaced Register
 */
#define DV_IN_OFFS_INTL_OFS_HEIGHT_SHIFT	(16)

#define DV_IN_OFFS_INTL_OFS_HEIGHT_MASK		(0xFFFF << DV_IN_OFFS_OFS_HEIGHT_SHIFT)

/*
 * DV_IN Interrupt Register
 */
#define DV_IN_INT_INTEN_SHIFT		(31)
#define DV_IN_INT_MINVS_SHIFT		(19)
#define DV_IN_INT_MVS_SHIFT			(18)
#define DV_IN_INT_MEOF_SHIFT		(17)
#define DV_IN_INT_MUPD_SHIFT		(16)
#define DV_IN_INT_FS_SHIFT			(11)
#define DV_IN_INT_INVS_SHIFT		(3)
#define DV_IN_INT_VS_SHIFT			(2)
#define DV_IN_INT_EOF_SHIFT			(1)
#define DV_IN_INT_UPD_SHIFT			(0)

#define DV_IN_INT_INTEN_MASK		(0x1 << DV_IN_INT_INTEN_SHIFT)
#define DV_IN_INT_MINVS_MASK		(0x1 << DV_IN_INT_MINVS_SHIFT)
#define DV_IN_INT_MVS_MASK			(0x1 << DV_IN_INT_MVS_SHIFT)
#define DV_IN_INT_MEOF_MASK			(0x1 << DV_IN_INT_MEOF_SHIFT)
#define DV_IN_INT_MUPD_MASK			(0x1 << DV_IN_INT_MUPD_SHIFT)
#define DV_IN_INT_FS_MASK			(0x1 << DV_IN_INT_FS_SHIFT)
#define DV_IN_INT_INVS_MASK			(0x1 << DV_IN_INT_INVS_SHIFT)
#define DV_IN_INT_VS_MASK			(0x1 << DV_IN_INT_VS_SHIFT)
#define DV_IN_INT_EOF_MASK			(0x1 << DV_IN_INT_EOF_SHIFT)
#define DV_IN_INT_UPD_MASK			(0x1 << DV_IN_INT_UPD_SHIFT)

/*
 * DV_IN Tunneling Register
 */
#define DV_IN_TUNNELING_SWAP_MODE_SHIFT	(0)
#define DV_IN_TUNNELING_TN_EN_SHIFT		(8)

#define DV_IN_TUNNELING_SWAP_MODE_MASK	(0x7 << DV_IN_TUNNELING_SWAP_MODE_SHIFT)
#define DV_IN_TUNNELING_TN_EN_MASK		(0x1 << DV_IN_TUNNELING_TN_EN_SHIFT)

/* Interface APIs. */
/* VIN polarity Setting */
extern void VIOC_DV_IN_SetSyncPolarity(
				unsigned int hs_active_low,
				unsigned int vs_active_low,
				unsigned int field_bfield_low,
				unsigned int de_active_low,
				unsigned int gen_field_en,
				unsigned int pxclk_pol);

/* VIN Configuration 1 */
extern void VIOC_DV_IN_SetCtrl(unsigned int conv_en,
			unsigned int hsde_connect_en, unsigned int vs_mask,
			unsigned int fmt, unsigned int data_order);


/* Interlace mode setting */
extern void VIOC_DV_IN_SetInterlaceMode(
				 unsigned int intl_en, unsigned int intpl_en);


/* VIN Capture mode Enable */
extern void VIOC_DV_IN_SetCaptureModeEnable(
				     unsigned int cap_en);


/* VIN Enable/Disable */
extern void VIOC_DV_IN_SetEnable(unsigned int vin_en);


extern unsigned int VIOC_DV_IN_IsEnable(volatile void __iomem *reg);
extern void VIOC_DV_IN_SetVsyncFlush(unsigned int bflush);


/* Image size setting */
extern void VIOC_DV_IN_SetImageSize(unsigned int width,
			     unsigned int height);


/* Image offset setting */
extern void VIOC_DV_IN_SetImageOffset(
			       unsigned int offs_width,
			       unsigned int offs_height,
			       unsigned int offs_height_intl);


/* VIN Enable/Disable */
extern void VIOC_DV_IN_SetTunneling(
			     unsigned int tunneling_en);


void VIOC_DV_IN_SetSwap(
			     unsigned int mode);

extern void VIOC_DV_IN_Configure(unsigned int in_width, unsigned int in_height, unsigned int in_fmt, unsigned int bTunneling);

extern volatile void __iomem *VIOC_DV_IN_GetAddress(void);


extern void VIOC_DV_IN_DUMP(void);
#endif
