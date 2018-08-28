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

extern int vioc_pm_set_lut(unsigned int PM_N, unsigned int nLUTSEL);

extern int vioc_pm_setupdate(unsigned int PM_N);

extern int vioc_pm_bypass(unsigned int PM_N, unsigned int onoff);

extern int vioc_pm_area_onoff(unsigned int PM_N, unsigned int onoff);

extern int vioc_pm_area_size(unsigned int PM_N, unsigned int sx, unsigned int sy, unsigned int ex, unsigned int ey);

extern int vioc_pm_initialize_set(unsigned int PM_N);

extern void __iomem * get_pm_address(unsigned int PM_N);
#endif /*__VIOC_PIXEL_MAPPER_H__*/
