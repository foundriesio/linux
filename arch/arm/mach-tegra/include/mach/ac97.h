/*
 * arch/arm/mach-tegra/include/mach/ac97.h
 *
 * Copyright (C) 2011 Toradex, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_TEGRA_AC97_H
#define __ARCH_ARM_MACH_TEGRA_AC97_H

#include <linux/kernel.h>
#include <linux/types.h>

/* Offsets from TEGRA_AC97_BASE */
#define AC_AC_CTRL_0		0
#define AC_AC_CMD_0		4
#define AC_AC_STATUS1_0		8
/* ... */
#define AC_AC_FIFO1_SCR_0	0x1c
#define AC_AC_FIFO2_SCR_0	0x2c
/* ... */
#define AC_AC_FIFO_OUT1_0	0x40
#define AC_AC_FIFO_IN1_0	0x80
#define AC_AC_FIFO_OUT2_0	0x140
#define AC_AC_FIFO_IN2_0	0x180

/* AC_AC_CTRL_0 */
#define AC_AC_CTRL_STM2_EN		(1<<16)
#define AC_AC_CTRL_DOUBLE_SAMPLING_EN	(1<<11)
#define AC_AC_CTRL_IO_CNTRL_EN		(1<<10)
#define AC_AC_CTRL_HSET_DAC_EN		(1<<9)
#define AC_AC_CTRL_LINE2_DAC_EN		(1<<8)
#define AC_AC_CTRL_PCM_LFE_EN		(1<<7)
#define AC_AC_CTRL_PCM_SUR_EN		(1<<6)
#define AC_AC_CTRL_PCM_CEN_DAC_EN	(1<<5)
#define AC_AC_CTRL_LINE1_DAC_EN		(1<<4)
#define AC_AC_CTRL_PCM_DAC_EN		(1<<3)
#define AC_AC_CTRL_COLD_RESET		(1<<2)
#define AC_AC_CTRL_WARM_RESET		(1<<1)
#define AC_AC_CTRL_STM_EN		(1<<0)

/* AC_AC_CMD_0 */
#define AC_AC_CMD_CMD_ADDR_SHIFT	(24)
#define AC_AC_CMD_CMD_ADDR_MASK		(0xff<<AC_AC_CMD_CMD_ADDR_SHIFT)
#define AC_AC_CMD_CMD_DATA_SHIFT	(8)
#define AC_AC_CMD_CMD_DATA_MASK		(0xffff<<AC_AC_CMD_CMD_DATA_SHIFT)
#define AC_AC_CMD_CMD_ID_SHIFT		(2)
#define AC_AC_CMD_CMD_ID_MASK		(0x3<<AC_AC_CMD_CMD_ID_SHIFT)
#define AC_AC_CMD_BUSY			(1<<0)

/* AC_AC_STATUS1_0 */
#define AC_AC_STATUS1_STA_ADDR1_SHIFT	(24)
#define AC_AC_STATUS1_STA_ADDR1_MASK	(0xff<<AC_AC_STATUS1_STA_ADDR1_SHIFT)
#define AC_AC_STATUS1_STA_DATA1_SHIFT	(8)
#define AC_AC_STATUS1_STA_DATA1_MASK	(0xffff<<AC_AC_STATUS1_STA_DATA1_SHIFT)
#define AC_AC_STATUS1_STA_VALID1	(1<<2)
#define AC_AC_STATUS1_STANDBY1		(1<<1)
#define AC_AC_STATUS1_CODEC1_RDY	(1<<0)

/* AC_AC_FIFO1_SCR_0 and AC_AC_FIFO2_SCR_0 */
#define AC_AC_FIFOx_SCR_REC_FIFOx_MT_CNT_SHIFT		(27)
#define AC_AC_FIFOx_SCR_REC_FIFOx_MT_CNT_MASK		(0x1f << REC_FIFO1_MT_CNT_SHIFT)
#define AC_AC_FIFOx_SCR_PB_FIFOx_MT_CNT_SHIFT		(22)
#define AC_AC_FIFOx_SCR_PB_FIFOx_MT_CNT_MASK		(0x1f << PB_FIFO1_MT_CNT_SHIFT)
#define AC_AC_FIFOx_SCR_REC_FIFOx_OVERRUN_INT_STA	(1<<19)
#define AC_AC_FIFOx_SCR_PB_FIFOx_UNDERRUN_INT_STA	(1<<18)
#define AC_AC_FIFOx_SCR_RECx_FORCE_MT			(1<<17)
#define AC_AC_FIFOx_SCR_PBx_FORCE_MT			(1<<16)
#define AC_AC_FIFOx_SCR_REC_FIFOx_FULL_EN		(1<<15)
#define AC_AC_FIFOx_SCR_REC_FIFOx_3QRT_FULL_EN		(1<<14)
#define AC_AC_FIFOx_SCR_REC_FIFOx_QRT_FULL_EN		(1<<13)
#define AC_AC_FIFOx_SCR_REC_FIFOx_NOT_MT_EN		(1<<12)
#define AC_AC_FIFOx_SCR_PB_FIFOx_NOT_FULL_EN		(1<<11)
#define AC_AC_FIFOx_SCR_PB_FIFOx_QRT_MT_EN		(1<<10)
#define AC_AC_FIFOx_SCR_PB_FIFOx_3QRT_MT_EN		(1<<9)
#define AC_AC_FIFOx_SCR_PB_FIFOx_MT_EN			(1<<8)

#endif /* __ARCH_ARM_MACH_TEGRA_AC97_H */
