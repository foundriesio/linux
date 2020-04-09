/*
 * Copyright (C) Telechips Inc.
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
#ifndef __TCA_LCDC_H__
#define __TCA_LCDC_H__

#include "tccfb_ioctrl.h"
#include "tcc_fb.h"

extern unsigned int DEV_LCDC_Wait_signal(char lcdc);
extern unsigned int DEV_LCDC_Wait_signal_Ext(void);
extern void tcc_lcdc_dithering_setting(struct tcc_dp_device *pdata);
extern void lcdc_initialize(struct lcd_panel *lcd_spec, struct tcc_dp_device *pdata);

#define ID_INVERT	0x01 	// Invered Data Enable(ACBIS pin)  anctive Low
#define IV_INVERT	0x02	// Invered Vertical sync  anctive Low
#define IH_INVERT	0x04	// Invered Horizontal sync	 anctive Low
#define IP_INVERT	0x08	// Invered Pixel Clock : anctive Low

#endif //__TCA_LCDC_H__

