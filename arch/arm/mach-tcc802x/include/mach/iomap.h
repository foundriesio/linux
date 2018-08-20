/****************************************************************************
 * iomap.h
 * Copyright (C) 2014 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __ASM_ARCH_TCC_IOMAP_H
#define __ASM_ARCH_TCC_IOMAP_H

/****************
 * SMU & PMU
 ****************/

#define TCC_PA_CKC		0x14000000
#define TCC_PA_PIC		0x14100000
#define TCC_PA_VIC		0x14100200
#define TCC_PA_GPIO		0x14200000
#define TCC_PA_TIMER		0x14300000
#define TCC_PA_PMU		0x14400000
#define TCC_PA_GIC_POL		0x14600000



/****************
 * Display BUS
 ****************/

#define TCC_PA_VIOC		0x12000000
#define TCC_PA_VIOC_CFGINT	0x1200A000

#endif
