/****************************************************************************
 * Copyright (C) 2015 Telechips Inc.
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
#ifndef __SRAM_MAP_H__
#define __SRAM_MAP_H__

/*
        0x?0000000  ======================
        (Code)     |  SRAM_BOOT Code      |
                   |  - psci(hyp mode)    |
                   |  - suspend/resume    |
                   |  - dram self-ref.    |
                    ----------------------
                   |  SDRAM Re-Init.      |
            0x3000  ----------------------
                   |      Do not use      |
                   |       this area      |
                   |       in Kernel      |
        0x?0008000  ======================
        (Data)     |  SRAM_BOOT Data      |
            0xA000  ----------------------
                   |  Snapshot Boot       |
            0xB000  ----------------------
                   |                      |
                   |         ----         |
                   |                      |
        0x?000F800  ======================
*/

#if defined(CONFIG_ARCH_TCC802X)
#define SRAM_CODE_BASE      0xF0000000
#define SRAM_DATA_BASE      0xF000A500
#define SRAM_TOTAL_SIZE     0x0000F800

#define sram_p2v(x)	((x)|0xF0000000)

/* SRAM Data Area */
#define CPU_DATA_REPOSITORY_ADDR    SRAM_DATA_BASE
#define CPU_DATA_REPOSITORY_SIZE    0x20    /* Real:0x010 */
#define GPIO_REPOSITORY_SIZE        0x900 /*0x2A0  Dolphin???? 0x900 */


#define SRAM_STACK_ADDR			(SRAM_CODE_BASE+SRAM_TOTAL_SIZE-0x4)


//+[TCCQB] QuickBoot SRAM AREA
#define TCC802X_CORE_MMU_DATA_ADDR		sram_p2v(SRAM_CODE_BASE + 0xA000)
#define TCC802X_CORE_MMU_DATA_SIZE		0x100
#define TCC802X_SNAPSHOT_BOOT_ADDR		sram_p2v(SRAM_CODE_BASE + 0xA100)
#define TCC802X_SNAPSHOT_BOOT_SIZE		0x400

//-[TCCQB]
//
#endif

#endif  /* __SRAM_MAP_H__ */
