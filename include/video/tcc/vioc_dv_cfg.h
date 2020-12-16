/*
 * Copyright (C) Telechips, Inc.
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
#ifdef CONFIG_ARCH_TCC898X
#include "tcc898x/vioc_dv_cfg.h"
#endif

#ifdef CONFIG_ARCH_TCC805X
#include "tcc805x/vioc_dv_cfg.h"
#endif

#ifdef CONFIG_ARCH_TCC899X
#include "tcc899x/vioc_dv_cfg.h"
#endif

#ifdef CONFIG_ARCH_TCC901X
#include "tcc901x/vioc_dv_cfg.h"
#endif

#define DV_FB_INT        (1<<INT_EN_R_TX_VS_SHIFT)
#define DV_VIDEO_INT     (1<<INT_EN_R_TX_VE_SHIFT) // INT_EN_F_TX_VS_SHIFT
#define DV_VIDEO_INT_SUB (1<<INT_EN_R_TX_HE_SHIFT) // INT_EN_F_TX_VS_SHIFT
