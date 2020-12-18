/*
 * vioc_afbcDec.h
 * Author:  <linux@telechips.com>
 * Created: , 2018
 * Description: TCC VIOC h/w block 
 *
 * Copyright (C) 2018 Telechips
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
#ifndef __VIOC_PVRIC_FBDC_H__
#define	__VIOC_PVRIC_FBDC_H__

typedef enum
{
	VIOC_PVRICCTRL_SWIZZ_ARGB = 0,
	VIOC_PVRICCTRL_SWIZZ_ARBG, 
	VIOC_PVRICCTRL_SWIZZ_AGRB, 
	VIOC_PVRICCTRL_SWIZZ_AGBR, 
	VIOC_PVRICCTRL_SWIZZ_ABGR, 
	VIOC_PVRICCTRL_SWIZZ_ABRG, 
	VIOC_PVRICCTRL_SWIZZ_RGBA = 8, 
	VIOC_PVRICCTRL_SWIZZ_RBGA, 
	VIOC_PVRICCTRL_SWIZZ_GRBA, 
	VIOC_PVRICCTRL_SWIZZ_GBRA, 
	VIOC_PVRICCTRL_SWIZZ_BGRA, 
	VIOC_PVRICCTRL_SWIZZ_BRGA, 
}VIOC_PVRICCTRL_SWIZZ_MODE;

typedef enum
{
	PVRICCTRL_FMT_ARGB8888 = 0x0C,
	PVRICCTRL_FMT_RGB888 = 0x3A,
}VIOC_PVRICCTRL_FMT_MODE;

typedef enum
{
	VIOC_PVRICCTRL_TILE_TYPE_8X8 = 1,
	VIOC_PVRICCTRL_TILE_TYPE_16X4,
	VIOC_PVRICCTRL_TILE_TYPE_32X2,
}VIOC_PVRICCTRL_TILE_TYPE;

typedef enum
{
	VIOC_PVRICSIZE_MAX_WIDTH_8X8 = 2048,
	VIOC_PVRICSIZE_MAX_WIDTH_16X4 = 4096, 
	VIOC_PVRICSIZE_MAX_WIDTH_32X2 = 7860,
}VIOC_PVRICSIZE_MAX_WIDTH;

/*
 * Register offset
 */
#define PVRICCTRL						(0x00)
#define PVRICSIZE 						(0x0C)
#define PVRICRADDR 						(0x10)
#define PVRICURTILE 					(0x14)
#define PVRICOFFS 						(0x18)
#define PVRICOADDR 						(0x1C)
#define PVRICIDLE 						(0x74)
#define PVRICSTS 						(0x78)

/*
  * PVRIC FBDC Control Register Information
  */
#define PVRICCTRL_SWIZZ_SHIFT			(24)
#define PVRICCTRL_UPD_SHIFT				(16)
#define PVRICCTRL_FMT_SHIFT				(8)
#define PVRICCTRL_MEML_SHIFT			(7)
#define PVRICCTRL_TILE_TYPE_SHIFT		(5)
#define PVRICCTRL_LOSSY_SHIFT			(4)
#define PVRICCTRL_CONTEXT_SHIFT			(1)
#define PVRICCTRL_START_SHIFT			(0)

#define PVRICCTRL_SWIZZ_MASK			(0xF << PVRICCTRL_SWIZZ_SHIFT)
#define PVRICCTRL_UPD_MASK				(0x1 << PVRICCTRL_UPD_SHIFT)
#define PVRICCTRL_FMT_MASK				(0x7F << PVRICCTRL_FMT_SHIFT)
#define PVRICCTRL_TILE_TYPE_MASK		(0x3 << PVRICCTRL_TILE_TYPE_SHIFT)
#define PVRICCTRL_LOSSY_MASK			(0x1 << PVRICCTRL_LOSSY_SHIFT)
#define PVRICCTRL_START_MASK			(0x1 << PVRICCTRL_START_SHIFT)

/*
  * PVRIC FBDC Frame Size Register Information
  */
#define PVRICSIZE_HEIGHT_SHIFT			(16)
#define PVRICSIZE_WIDTH_SHIFT			(0)

#define PVRICSIZE_HEIGHT_MASK			(0x1FFF << PVRICSIZE_HEIGHT_SHIFT)
#define PVRICSIZE_WIDTH_MASK			(0x1FFF << PVRICSIZE_WIDTH_SHIFT)

/*
  * PVRIC FBDC Request Base address Register Information
  */
#define PVRICRADDR_SHIFT				(0)

#define PVRICRADDR_MASK					(0xFFFFFFFF << PVRICRADDR_SHIFT)


/*
  * PVRIC FBDC Current Tile NumberRegister Information
  */
#define PVRICCURTILE_SHIFT				(0)

#define PVRICCURTILE_MASK				(0x7FFFFF << PVRICCURTILE_SHIFT)


/*
  * PVRIC FBDC Output buffer Offset Register Information
  */
#define PVRICOFFS_SHIFT					(0)

#define PVRICOFFS_MASK					(0xFFFFFFFF << PVRICOFFS_SHIFT)


/*
  * PVRIC FBDC Output buffer Base address Register Information
  */
#define PVRICOADDR_SHIFT				(0)

#define PVRICOADDR_MASK					(0xFFFFFFFF << PVRICOFFS_SHIFT)

/*
  * PVRIC FBDC Idle status Register Information
  */
#define PVRICIDLER_SHIFT				(0)

#define PVRICOADDR_MASK					(0x1 << PVRICOFFS_SHIFT)

/*
  * PVRIC FBDC IREQ Status Register Information
  */
#define PVRICSTS_IRQ_MASK_SHIFT			(16)

#define PVRICSTS_EOF_ERR_SHIFT			(4)
#define PVRICSTS_ADDR_ERR_SHIFT			(3)
#define PVRICSTS_TILE_ERR_SHIFT			(2)
#define PVRICSTS_UPD_SHIFT				(1)
#define PVRICSTS_IDLE_SHIFT				(0)

#define PVRICSTS_EOF_ERR_MASK			(0x1 << PVRICSTS_EOF_ERR_SHIFT)
#define PVRICSTS_ADDR_ERR_MASK			(0x1 << PVRICSTS_ADDR_ERR_SHIFT)
#define PVRICSTS_TILE_ERR_MASK			(0x1 << PVRICSTS_TILE_ERR_SHIFT)
#define PVRICSTS_UPD_MASK				(0x1 << PVRICSTS_UPD_SHIFT)
#define PVRICSTS_IDLE_MASK				(0x1 << PVRICSTS_IDLE_SHIFT)

#define PVRICSYS_IRQ_ALL			    ( PVRICSTS_EOF_ERR_MASK | PVRICSTS_ADDR_ERR_MASK | PVRICSTS_TILE_ERR_MASK | PVRICSTS_UPD_MASK | PVRICSTS_IDLE_MASK )	

/* Interface APIs */
extern void VIOC_PVRIC_FBDC_SetARGBSwizzMode(volatile void __iomem *reg, VIOC_PVRICCTRL_SWIZZ_MODE mode);
extern void VIOC_PVRIC_FBDC_SetUpdateInfo(volatile void __iomem *reg, unsigned int enable);
extern void VIOC_PVRIC_FBDC_SetFormat(volatile void __iomem *reg, VIOC_PVRICCTRL_FMT_MODE fmt);
extern void VIOC_PVRIC_FBDC_SetTileType(volatile void __iomem *reg, VIOC_PVRICCTRL_TILE_TYPE type);
extern void VIOC_PVRIC_FBDC_SetLossyDecomp(volatile void __iomem *reg, unsigned int enable);

extern void VIOC_PVRIC_FBDC_SetFrameSize(volatile void __iomem *reg, unsigned int width, unsigned int height);
extern void VIOC_PVRIC_FBDC_GetFrameSize(volatile void __iomem *reg, unsigned int *width, unsigned int *height);
extern void VIOC_PVRIC_FBDC_SetRequestBase(volatile void __iomem *reg, unsigned int base);
extern void VIOC_PVRIC_FBDC_GetCurTileNum(volatile void __iomem *reg, unsigned int *tile_num);
extern void VIOC_PVRIC_FBDC_SetOutBufOffset(volatile void __iomem *reg, unsigned int imgFmt, unsigned int imgWidth);
extern void VIOC_PVRIC_FBDC_SetOutBufBase(volatile void __iomem *reg, unsigned int base);
extern unsigned int VIOC_PVRIC_FBDC_GetIdle(volatile void __iomem *reg);
extern unsigned int VIOC_PVRIC_FBDC_GetStatus(volatile void __iomem *reg);
extern void VIOC_PVRIC_FBDC_SetIrqMask(volatile void __iomem *reg, unsigned int enable, unsigned int mask);
extern void VIOC_PVRIC_FBDC_ClearIrq(volatile void __iomem *reg, unsigned int mask);
extern void VIOC_PVRIC_FBDC_TurnOFF(volatile void __iomem *reg);
extern void VIOC_PVRIC_FBDC_TurnOn(volatile void __iomem *reg);
extern int VIOC_PVRIC_FBDC_SetBasicConfiguration(volatile void __iomem *reg, unsigned int base,
			 unsigned int imgFmt, unsigned int imgWidth, unsigned int imgHeight, unsigned int decomp_mode);
extern void VIOC_PVRIC_FBDC_DUMP(volatile void __iomem *reg, unsigned int vioc_id);
extern volatile void __iomem* VIOC_PVRIC_FBDC_GetAddress(unsigned int vioc_id);


#endif //__VIOC_PVRICFBDC_H__


