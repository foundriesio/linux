/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __MAX96712_REG_H__
#define __MAX96712_REG_H__

/*
 * MAX96712 REG
 */
#define MAX96712_LINK_EN_A			(1 << 0)
#define MAX96712_LINK_EN_B			(1 << 1)
#define MAX96712_LINK_EN_C			(1 << 2)
#define MAX96712_LINK_EN_D			(1 << 3)

#define MAX96712_GMSL1_A			(0 << 4)
#define MAX96712_GMSL1_B			(0 << 5)
#define MAX96712_GMSL1_C			(0 << 6)
#define MAX96712_GMSL1_D			(0 << 7)

#define MAX96712_GMSL2_A			(1 << 4)
#define MAX96712_GMSL2_B			(1 << 5)
#define MAX96712_GMSL2_C			(1 << 6)
#define MAX96712_GMSL2_D			(1 << 7)

#define MAX96712_GMSL1_4CH					\
		(MAX96712_GMSL1_A | MAX96712_GMSL1_B |		\
		 MAX96712_GMSL1_C | MAX96712_GMSL1_D |		\
		 MAX96712_LINK_EN_A | MAX96712_LINK_EN_B |	\
		 MAX96712_LINK_EN_C | MAX96712_LINK_EN_D)

#define MAX96712_GMSL1_2CH					\
		(MAX96712_GMSL1_A | MAX96712_GMSL1_B |		\
		 MAX96712_GMSL1_C | MAX96712_GMSL1_D |		\
		 MAX96712_LINK_EN_A | MAX96712_LINK_EN_B)

#define MAX96712_GMSL1_1CH					\
		(MAX96712_GMSL1_A | MAX96712_GMSL1_B |		\
		 MAX96712_GMSL1_C | MAX96712_GMSL1_D |		\
		 MAX96712_LINK_EN_A)

#define MAX96712_REG_GMSL1_A_FWDCCEN		(0x0B04)
#define MAX96712_REG_GMSL1_B_FWDCCEN		(0x0C04)
#define MAX96712_REG_GMSL1_C_FWDCCEN		(0x0D04)
#define MAX96712_REG_GMSL1_D_FWDCCEN		(0x0E04)

#define MAX96712_GMSL1_FWDCC_ENABLE		(0x03)
#define MAX96712_GMSL1_FWDCC_DISABLE		(0x00)

#endif

