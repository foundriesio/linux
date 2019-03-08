/*
 * linux/include/video/tcc/tcc897x/vioc_lvds.h
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

#ifndef __VIOC_LVDS_H__
#define	__VIOC_LVDS_H__


extern void tcc_set_ddi_lvds_reset(unsigned int reset);
extern void tcc_set_ddi_lvds_pms(unsigned int lcdc_n, unsigned int pclk, unsigned int enable);

#define LVDS_DUMMY			(0x1F)
#define LVDS_DE				(24)
#define LVDS_HS				(25)
#define LVDS_VS				(26)
#define LVDS_R_D(x)			(x + 0x10)
#define LVDS_G_D(x)			(x + 0x8)
#define LVDS_B_D(x)			(x )

#define LVDS_MAX_LINE			4
#define LVDS_DATA_PER_LINE		7
extern void tcc_set_ddi_lvds_data_arrary(unsigned int data[LVDS_MAX_LINE][LVDS_DATA_PER_LINE]);
extern void tcc_set_ddi_lvds_config(void);

#endif
