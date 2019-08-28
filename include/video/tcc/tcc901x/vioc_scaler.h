/*
 * linux/arch/arm/mach-tcc893x/include/mach/vioc_scaler.h
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

#ifndef __VIOC_SCALER_H__
#define	__VIOC_SCALER_H__

#define VIOC_SC_INT_MASK_UPDDONE 		0x00000001UL /*Status of Register Updated*/
#define VIOC_SC_INT_MASK_EOFRISE 		0x00000002UL /*Rising EOF Interrupt Masked*/
#define VIOC_SC_INT_MASK_EOFFALL 		0x00000004UL /*Falling EOF Interrupt Masked*/
#define VIOC_SC_INT_MASK_ERROR  		0x00000008UL /*Scaler Error Interrupt Masked*/
#define VIOC_SC_INT_MASK_ALL 			0x0000000FUL /*ALL*/

#define VIOC_SC_IREQ_UPDDONE_MASK 		0x00000001UL
#define VIOC_SC_IREQ_EOFRISE_MASK 		0x00000002UL
#define VIOC_SC_IREQ_EOFFALL_MASK  		0x00000004UL
#define VIOC_SC_IREQ_ERROR_MASK  		0x00000008UL

/*
 * register offset
 */
#define SCCTRL		(0x00)
#define SCSSIZE		(0x08)
#define SCDSIZE		(0x0C)
#define SCOPOS		(0x10)
#define SCOSIZE		(0x14)
#define SCIRQSTS	(0x18)
#define SCIRQMSK	(0x1C)

/*
 * Scaler Control Register
 */
#define SCCTRL_3DM_SHIFT		(30)
#define SCCTRL_FFC_SHIFT		(17)
#define SCCTRL_UPD_SHIFT		(16)
#define SCCTRL_BP_SHIFT			(0)

#define SCCTRL_3DM_MASK		(0x3 << SCCTRL_3DM_SHIFT)
#define SCCTRL_FFC_MASK		(0x1 << SCCTRL_FFC_SHIFT)
#define SCCTRL_UPD_MASK		(0x1 << SCCTRL_UPD_SHIFT)
#define SCCTRL_BP_MASK		(0x1 << SCCTRL_BP_SHIFT)

/*
 * Scaler Source Size Register
 */
#define SCSSIZE_HEIGHT_SHIFT		(16)
#define SCSSIZE_WIDTH_SHIFT			(0)

#define SCSSIZE_HEIGHT_MASK			(0x1FFF << SCSSIZE_HEIGHT_SHIFT)
#define SCSSIZE_WIDTH_MASK			(0x1FFF << SCSSIZE_WIDTH_SHIFT)

/*
 * Scaler Destination Size Register
 */
#define SCDSIZE_HEIGHT_SHIFT		(16)
#define SCDSIZE_WIDTH_SHIFT			(0)

#define SCDSIZE_HEIGHT_MASK			(0x1FFF << SCDSIZE_HEIGHT_SHIFT)
#define SCDSIZE_WIDTH_MASK			(0x1FFF << SCDSIZE_WIDTH_SHIFT)

/*
 * Scaler Output Position Register
 */
#define SCOPOS_YPOS_SHIFT			(16)
#define SCOPOS_XPOS_SHIFT			(0)

#define SCOPOS_YPOS_MASK			(0x1FFF << SCOPOS_YPOS_SHIFT)
#define SCOPOS_XPOS_MASK			(0x1FFF << SCOPOS_XPOS_SHIFT)

/*
 * Scaler Output Size Register
 */
#define SCOSIZE_HEIGHT_SHIFT		(16)
#define SCOSIZE_WIDTH_SHIFT			(0)

#define SCOSIZE_HEIGHT_MASK			(0x1FFF << SCOSIZE_HEIGHT_SHIFT)
#define SCOSIZE_WIDTH_MASK			(0x1FFF << SCOSIZE_WIDTH_SHIFT)

/*
 * Scaler Interrupt Status Register
 */
#define SCIRQSTS_ERR_SHIFT			(3)
#define SCIRQSTS_EOFF_SHIFT			(2)
#define SCIRQSTS_EOFR_SHIFT			(1)
#define SCIRQSTS_UPD_SHIFT			(0)

#define SCIRQSTS_ERR_MASK			(0x1 << SCIRQSTS_ERR_SHIFT)
#define SCIRQSTS_EOFF_MASK			(0x1 << SCIRQSTS_EOFF_SHIFT)
#define SCIRQSTS_EOFR_MASK			(0x1 << SCIRQSTS_EOFR_SHIFT)
#define SCIRQSTS_UPD_MASK			(0x1 << SCIRQSTS_UPD_SHIFT)

/*
 * Scaler Interrupt Mask  Register
 */
#define SCIRQMSK_MERR_SHIFT			(3)
#define SCIRQMSK_MEOFF_SHIFT		(2)
#define SCIRQMSK_MEOFR_SHIFT		(1)
#define SCIRQMSK_MUPD_SHIFT			(0)

#define SCIRQMSK_MERR_MASK			(0x1 << SCIRQMSK_MERR_SHIFT)
#define SCIRQMSK_MEOFF_MASK			(0x1 << SCIRQMSK_MEOFF_SHIFT)
#define SCIRQMSK_MEOFR_MASK			(0x1 << SCIRQMSK_MEOFR_SHIFT)
#define SCIRQMSK_MUPD_MASK			(0x1 << SCIRQMSK_MUPD_SHIFT)

/* Interface APIs */
extern void VIOC_SC_SetBypass(volatile void __iomem *reg, unsigned int nOnOff);
extern void VIOC_SC_SetUpdate(volatile void __iomem *reg);
extern void VIOC_SC_SetSrcSize(volatile void __iomem *reg, unsigned int nWidth, unsigned int nHeight);
#if defined(CONFIG_MC_WORKAROUND)
extern unsigned int VIOC_SC_GetPlusSize(unsigned int src_height, unsigned dst_height);
#endif
extern void VIOC_SC_SetDstSize(volatile void __iomem *reg, unsigned int nWidth, unsigned int nHeight);
extern void VIOC_SC_SetOutSize(volatile void __iomem *reg, unsigned int nWidth, unsigned int nHeight);
extern void VIOC_SC_SetOutPosition(volatile void __iomem *reg, unsigned int nXpos, unsigned int nYpos);
extern volatile void __iomem * VIOC_SC_GetAddress(unsigned int vioc_id);
extern void VIOC_SCALER_DUMP(volatile void __iomem *reg, unsigned int vioc_id);
#endif /*__VIOC_SCALER_H__*/
