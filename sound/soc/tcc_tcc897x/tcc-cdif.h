/*
 * linux/sound/soc/tcc/tcc-cdif.h
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author:  <linux@telechips.com>
 * Created:    FEB 23, 2017
 * Description: ALSA PCM interface for the Intel PXA2xx chip
 *
 * Copyright (C) 2008-2009 Telechips 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 
 * as published by the Free Software Foundation
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

#ifndef _TCC_CDIF_H
#define _TCC_CDIF_H

#define TCC_CDIF_RATES   SNDRV_PCM_RATE_8000_192000

#define CDIF_OFFSET 0x1080		// Offset from Audio Base ADDR
#define	CDIF_CDDI0	0x000		// CD Digital Audio Input Register 0
#define	CDIF_CDDI1	0x004		// CD Digital Audio Input Register 1
#define	CDIF_CDDI2	0x008		// CD Digital Audio Input Register 2
#define	CDIF_CDDI3	0x00C		// CD Digital Audio Input Register 3
#define	CDIF_CDDI4	0x010		// CD Digital Audio Input Register 4
#define	CDIF_CDDI5	0x014		// CD Digital Audio Input Register 5
#define	CDIF_CDDI6	0x018		// CD Digital Audio Input Register 6
#define	CDIF_CDDI7	0x01C		// CD Digital Audio Input Register 7
#define	CDIF_CICR	0x020		// CD Interface Control Register

#define SPDIF_RXCONFIG_OFFSET 0x2804		// Offset from Audio Base ADDR

#define TCC_FIX_FS 64 //We support 32, 48, 64 fs.
struct tcc_cdif_data {
	unsigned long	bclk_fs;
	unsigned int	backup_rCICR;
};

#endif
