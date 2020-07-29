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
#ifndef GPU_ALIGN_H_
#define GPU_ALIGN_H_

//Pixel unit

// luminance
#define L_STRIDE_ALIGN 64 // Arm: 16, Pvr: 64
#define L_REGION_ALIGN 64 // Arm: 64, Pvr: 64

// Chrominance
#define C_STRIDE_ALIGN 32 // Arm: 16, Pvr: 32?
#define C_REGION_ALIGN 64 // Arm: 64, Pvr: 64


#endif
