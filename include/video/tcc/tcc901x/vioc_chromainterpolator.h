/*
 * linux/include/video/tcc/vioc_chromainterpolator.h
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
#ifndef __VIOC_CHROMAINTERPOLATOR_H__
#define	__VIOC_CHROMAINTERPOLATOR_H__
extern void VIOC_ChromaInterpol_ctrl(volatile void __iomem *reg, unsigned int mode, 
		unsigned int r2y_en, unsigned int r2y_mode, unsigned int y2r_en, unsigned int y2r_mode);

extern volatile void __iomem *VIOC_ChromaInterpol_GetAddress(unsigned int vioc_id);
#endif//__VIOC_CHROMAINTERPOLATOR_H__

