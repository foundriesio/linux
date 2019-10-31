/*
 * linux/video/tcc/vioc_pixel_mapper.h
 * Author:  <linux@telechips.com>
 * Created: June 10, 2018
 * Description: TCC VIOC h/w block 
 *
 * Copyright (C) 2018-2019 Telechips
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

#ifndef __VIOC_PIXEL_MAPPER_H__
#define	__VIOC_PIXEL_MAPPER_H__

#define VIOC_PIXELMAP_LUT_NUM		8
// PIXEL MAPPER ctrl register
#define PM_CTRL_REG 			(0x0)

#define PM_CTRL_UPD_SHIFT 		(0x10)
#define PM_CTRL_UPD_MASK  		(0x1 << PM_CTRL_UPD_SHIFT)

#define PM_CTRL_AREA_EN_SHIFT 	(0x3)
#define PM_CTRL_AREA_EN_MASK  	(0x1 << PM_CTRL_AREA_EN_SHIFT)

#define PM_CTRL_INT_SHIFT 		(0x2)
#define PM_CTRL_INT_MASK  		(0x1 << PM_CTRL_INT_SHIFT)

#define PM_CTRL_BYP_SHIFT 		(0x1)
#define PM_CTRL_BYP_MASK  		(0x1 << PM_CTRL_BYP_SHIFT)

//PIXEL MAPPER AREA Width register
#define PM_AREA_W_REG			(0x4)
#define PM_AREA_W_END_SHIFT	(0x10)
#define PM_AREA_W_END_MASK	(0x1FFF << PM_AREA_W_END_SHIFT)
#define PM_AREA_W_START_SHIFT	(0x0)
#define PM_AREA_W_START_MASK	(0x1FFF << PM_AREA_W_START_SHIFT)


//PIXEL MAPPER AREA Height register
#define PM_AREA_H_REG			(0x8)
#define PM_AREA_H_END_SHIFT	(0x10)
#define PM_AREA_H_END_MASK	(0x1FFF << PM_AREA_H_END_SHIFT)
#define PM_AREA_H_START_SHIFT	(0x0)
#define PM_AREA_H_START_MASK	(0x1FFF << PM_AREA_H_START_SHIFT)


//PIXEL MAPPER AREA Height register
#define PM_LUT_SEL_REG			(0xC)
#define PM_LUT_SEL_SHIFT		(0x0)
#define PM_LUT_SEL_MASK		(0x3 << PM_LUT_SEL_SHIFT)

// PIXEL MAPPER TABLE register
#define PM_LUT_TABLE_REG		(0x400)

// PIXLE MAPPER
extern int vioc_pm_cal_lut_reg(void);

extern int vioc_pm_set_lut_table(unsigned int PM_N, unsigned int *table);

extern int vioc_pm_set_lut(unsigned int PM_N, unsigned int nLUTSEL);

extern int vioc_pm_setupdate(unsigned int PM_N);

extern int vioc_pm_bypass(unsigned int PM_N, unsigned int onoff);

extern int vioc_pm_area_onoff(unsigned int PM_N, unsigned int onoff);

extern int vioc_pm_area_size(unsigned int PM_N, unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey);

extern int vioc_pm_initialize_set(unsigned int PM_N);

extern void __iomem * get_pm_address(unsigned int PM_N);
#endif /*__VIOC_PIXEL_MAPPER_H__*/
