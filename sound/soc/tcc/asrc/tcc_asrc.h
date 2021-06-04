/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef _TCC_ASRC_H_
#define _TCC_ASRC_H_

/*
 * ASRC Register Offset
 */
#define TCC_ASRC_ENABLE_OFFSET\
	(0x000)	// RW
#define TCC_ASRC_INIT_OFFSET\
	(0x004)	// RW
#define TCC_ASRC_INPORT_CTRL_OFFSET\
	(0x008)	// RW
#define TCC_ASRC_OUTPORT_CTRL_OFFSET\
	(0x00C)	// RW
#define TCC_ASRC_SRC_INIT_ZERO_SZ_OFFSET\
	(0x010)	// RW
#define TCC_ASRC_SRC_OPT_BUF_LVL_OFFSET\
	(0x014)	// RW
#define TCC_ASRC_EXT_IO_FMT_OFFSET\
	(0x018)	// RW

#define TCC_ASRC_PERIOD_SYNC_CNT0_OFFSET\
	(0x020)	// RW
#define TCC_ASRC_SRC_RATE0_OFFSET\
	(0x024)	// RW
#define TCC_ASRC_SRC_CAL_RATE0_OFFSET\
	(0x028)	// R
#define TCC_ASRC_SRC_STATUS0_OFFSET\
	(0x02C)	// R

#define TCC_ASRC_PERIOD_SYNC_CNT1_OFFSET\
	(0x030)	// RW
#define TCC_ASRC_SRC_RATE1_OFFSET\
	(0x034)	// RW
#define TCC_ASRC_SRC_CAL_RATE1_OFFSET\
	(0x038)	// R
#define TCC_ASRC_SRC_STATUS1_OFFSET\
	(0x03C)	// R

#define TCC_ASRC_PERIOD_SYNC_CNT2_OFFSET\
	(0x040)	// RW
#define TCC_ASRC_SRC_RATE2_OFFSET\
	(0x044)	// RW
#define TCC_ASRC_SRC_CAL_RATE2_OFFSET\
	(0x048)	// R
#define TCC_ASRC_SRC_STATUS2_OFFSET\
	(0x04C)	// R

#define TCC_ASRC_PERIOD_SYNC_CNT3_OFFSET\
	(0x050)	// RW
#define TCC_ASRC_SRC_RATE3_OFFSET\
	(0x054)	// RW
#define TCC_ASRC_SRC_CAL_RATE3_OFFSET\
	(0x058)	// R
#define TCC_ASRC_SRC_STATUS3_OFFSET\
	(0x05C)	// R

#define TCC_ASRC_MIX_IN_EN_OFFSET\
	(0x060)	// RW
#define TCC_ASRC_SAMP_TIMING_OFFSET\
	(0x064)	// RW

#define TCC_ASRC_VOL_CTRL_EN_OFFSET\
	(0x070)	// RW

#define TCC_ASRC_VOL_GAIN0_OFFSET\
	(0x080)	// RW
#define TCC_ASRC_VOL_RAMP_DN_CFG0_OFFSET\
	(0x084)	// RW
#define TCC_ASRC_VOL_RAMP_GAIN0_OFFSET\
	(0x088)	// RW
#define TCC_ASRC_VOL_RAMP_UP_CFG0_OFFSET\
	(0x08C)	// RW

#define TCC_ASRC_VOL_GAIN1_OFFSET\
	(0x090)	// RW
#define TCC_ASRC_VOL_RAMP_DN_CFG1_OFFSET\
	(0x094)	// RW
#define TCC_ASRC_VOL_RAMP_GAIN1_OFFSET\
	(0x098)	// RW
#define TCC_ASRC_VOL_RAMP_UP_CFG1_OFFSET\
	(0x09C)	// RW

#define TCC_ASRC_VOL_GAIN2_OFFSET\
	(0x0A0)	// RW
#define TCC_ASRC_VOL_RAMP_DN_CFG2_OFFSET\
	(0x0A4)	// RW
#define TCC_ASRC_VOL_RAMP_GAIN2_OFFSET\
	(0x0A8)	// RW
#define TCC_ASRC_VOL_RAMP_UP_CFG2_OFFSET\
	(0x0AC)	// RW

#define TCC_ASRC_VOL_GAIN3_OFFSET\
	(0x0B0)	// RW
#define TCC_ASRC_VOL_RAMP_DN_CFG3_OFFSET\
	(0x0B4)	// RW
#define TCC_ASRC_VOL_RAMP_GAIN3_OFFSET\
	(0x0B8)	// RW
#define TCC_ASRC_VOL_RAMP_UP_CFG3_OFFSET\
	(0x0BC)	// RW

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define TCC_ASRC_IRQ_RAW_STATUS0_OFFSET\
	(0x0C0) // R
#define TCC_ASRC_IRQ_MASK_STATUS0_OFFSET\
	(0x0C4) // R
#define TCC_ASRC_IRQ_CLEAR0_OFFSET\
	(0x0C4) // W
#define TCC_ASRC_IRQ_ENABLE0_OFFSET\
	(0x0C8) // RW
#define TCC_ASRC_IRQ_RAW_STATUS1_OFFSET\
	(0x0E0) // R
#define TCC_ASRC_IRQ_MASK_STATUS1_OFFSET\
	(0x0E4) // R
#define TCC_ASRC_IRQ_CLEAR1_OFFSET\
	(0x0E4) // W
#define TCC_ASRC_IRQ_ENABLE1_OFFSET\
	(0x0E8) // RW
#else
#define TCC_ASRC_IRQ_RAW_STATUS_OFFSET\
	(0x0C0)	// R
#define TCC_ASRC_IRQ_MASK_STATUS_OFFSET\
	(0x0C4)	// R
#define TCC_ASRC_IRQ_CLEAR_OFFSET\
	(0x0C4)	// W
#define TCC_ASRC_IRQ_ENABLE_OFFSET\
	(0x0C8)	// RW
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)

#define TCC_ASRC_FIFO_IN0_CTRL_OFFSET\
	(0x100)	// RW
#define TCC_ASRC_FIFO_IN0_STATUS_OFFSET\
	(0x104)	// R

#define TCC_ASRC_FIFO_IN1_CTRL_OFFSET\
	(0x108)	// RW
#define TCC_ASRC_FIFO_IN1_STATUS_OFFSET\
	(0x10C)	// R

#define TCC_ASRC_FIFO_IN2_CTRL_OFFSET\
	(0x110)	// RW
#define TCC_ASRC_FIFO_IN2_STATUS_OFFSET\
	(0x114)	// R

#define TCC_ASRC_FIFO_IN3_CTRL_OFFSET\
	(0x118)	// RW
#define TCC_ASRC_FIFO_IN3_STATUS_OFFSET\
	(0x11C)	// R

#define TCC_ASRC_FIFO_OUT0_CTRL_OFFSET\
	(0x120)	// RW
#define TCC_ASRC_FIFO_OUT0_STATUS_OFFSET\
	(0x124)	// R

#define TCC_ASRC_FIFO_OUT1_CTRL_OFFSET\
	(0x128)	// RW
#define TCC_ASRC_FIFO_OUT1_STATUS_OFFSET\
	(0x12C)	// R

#define TCC_ASRC_FIFO_OUT2_CTRL_OFFSET\
	(0x130)	// RW
#define TCC_ASRC_FIFO_OUT2_STATUS_OFFSET\
	(0x134)	// R

#define TCC_ASRC_FIFO_OUT3_CTRL_OFFSET\
	(0x138)	// RW
#define TCC_ASRC_FIFO_OUT3_STATUS_OFFSET\
	(0x13C)	// R

#define TCC_ASRC_FIFO_MISC_CTRL_OFFSET\
	(0x140)	// RW

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define TCC_ASRC_FIFO_IN0_CTRL1_OFFSET\
	(0x150) // RW
#define TCC_ASRC_FIFO_IN1_CTRL1_OFFSET\
	(0x154) // RW
#define TCC_ASRC_FIFO_IN2_CTRL1_OFFSET\
	(0x158) // RW
#define TCC_ASRC_FIFO_IN3_CTRL1_OFFSET\
	(0x15C) // RW

#define TCC_ASRC_FIFO_IN0_STATUS1_OFFSET\
	(0x160) // R
#define TCC_ASRC_FIFO_IN1_STATUS1_OFFSET\
	(0x164) // R
#define TCC_ASRC_FIFO_IN2_STATUS1_OFFSET\
	(0x168) // R
#define TCC_ASRC_FIFO_IN3_STATUS1_OFFSET\
	(0x16C) // R

#define TCC_ASRC_FIFO_OUT0_CTRL1_OFFSET\
	(0x170) // RW
#define TCC_ASRC_FIFO_OUT1_CTRL1_OFFSET\
	(0x174) // RW
#define TCC_ASRC_FIFO_OUT2_CTRL1_OFFSET\
	(0x178) // RW
#define TCC_ASRC_FIFO_OUT3_CTRL1_OFFSET\
	(0x17C) // RW

#define TCC_ASRC_FIFO_OUT0_STATUS1_OFFSET\
	(0x180) // R
#define TCC_ASRC_FIFO_OUT1_STATUS1_OFFSET\
	(0x184) // R
#define TCC_ASRC_FIFO_OUT2_STATUS1_OFFSET\
	(0x188) // R
#define TCC_ASRC_FIFO_OUT3_STATUS1_OFFSET\
	(0x18C) // R
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)

#define TCC_ASRC_FIFO_IN0_DATA_OFFSET\
	(0x200)	// RW
#define TCC_ASRC_FIFO_OUT0_DATA_OFFSET\
	(0x300)	// RW

#define TCC_ASRC_FIFO_IN1_DATA_OFFSET\
	(0x240)	// RW
#define TCC_ASRC_FIFO_OUT1_DATA_OFFSET\
	(0x340)	// RW

#define TCC_ASRC_FIFO_IN2_DATA_OFFSET\
	(0x280)	// RW
#define TCC_ASRC_FIFO_OUT2_DATA_OFFSET\
	(0x380)	// RW

#define TCC_ASRC_FIFO_IN3_DATA_OFFSET\
	(0x2C0)	// RW
#define TCC_ASRC_FIFO_OUT3_DATA_OFFSET\
	(0x3C0)	// RW

/*
 * ASRC Regsiter Bit Mask
 */

enum tcc_asrc_component_t {
	TCC_ASRC0 = 0,
	TCC_ASRC1 = 1,
	TCC_ASRC2 = 2,
	TCC_ASRC3 = 3,
	TCC_INPORT = 4,
	TCC_OUTPORT = 5,
	TCC_EXTIO = 6,
};

//ENABLE Reg
#define TCC_ASRC_ENABLE_ASRC0\
	(0x01<<0)	// ASRC pair0
#define TCC_ASRC_ENABLE_ASRC1\
	(0x01<<1)	// ASRC pair0
#define TCC_ASRC_ENABLE_ASRC2\
	(0x01<<2)	// ASRC pair0
#define TCC_ASRC_ENABLE_ASRC3\
	(0x01<<3)	// ASRC pair0
#define TCC_ASRC_ENABLE_INPORT\
	(0x01<<4)	// Inport
#define TCC_ASRC_ENABLE_OUTPORT\
	(0x01<<5)	// Outport
#define TCC_ASRC_ENABLE_EXTIO\
	(0x01<<6)	// External I/O

//INIT Reg
#define TCC_ASRC_INIT_ASRC0\
	(0x01<<0)	// ASRC pair0
#define TCC_ASRC_INIT_ASRC1\
	(0x01<<1)	// ASRC pair1
#define TCC_ASRC_INIT_ASRC2\
	(0x01<<2)	// ASRC pair2
#define TCC_ASRC_INIT_ASRC3\
	(0x01<<3)	// ASRC pair3
#define TCC_ASRC_INIT_INPORT\
	(0x01<<4)	// Inport
#define TCC_ASRC_INIT_OUTPORT\
	(0x01<<5)	// Outport
#define TCC_ASRC_INIT_EXTIO\
	(0x01<<6)	// External I/O

//INPORT_CTRL Reg
#define TCC_ASRC_IP3_ROUTE_MASK\
	(0x0f<<28)	// IP3_Route Mask
#define TCC_ASRC_IP2_ROUTE_MASK\
	(0x0f<<24)	// IP2_Route Mask
#define TCC_ASRC_IP1_ROUTE_MASK\
	(0x0f<<20)	// IP1_Route Mask
#define TCC_ASRC_IP0_ROUTE_MASK\
	(0x0f<<16)	// IP0_Route Mask
#define TCC_ASRC_IP3_ROUTE(x)\
	(((x)<<28) & TCC_ASRC_IP3_ROUTE_MASK)	// IP3_Route
#define TCC_ASRC_IP2_ROUTE(x)\
	(((x)<<24) & TCC_ASRC_IP2_ROUTE_MASK)	// IP2_Route
#define TCC_ASRC_IP1_ROUTE(x)\
	(((x)<<20) & TCC_ASRC_IP1_ROUTE_MASK)	// IP1_Route
#define TCC_ASRC_IP0_ROUTE(x)\
	(((x)<<16) & TCC_ASRC_IP0_ROUTE_MASK)	// IP0_Route

#define TCC_ASRC_IP3_PATH_MASK\
	(0x01<<15)	// IP3_Path
#define TCC_ASRC_IP2_PATH_MASK\
	(0x01<<11)	// IP2_Path
#define TCC_ASRC_IP1_PATH_MASK\
	(0x01<<7)	// IP1_Path
#define TCC_ASRC_IP0_PATH_MASK\
	(0x01<<3)	// IP0_Path
#define TCC_ASRC_IP3_PATH(x)\
	(((x)<<15) & TCC_ASRC_IP3_PATH_MASK)	// IP3_Path
#define TCC_ASRC_IP2_PATH(x)\
	(((x)<<11) & TCC_ASRC_IP2_PATH_MASK)	// IP2_Path
#define TCC_ASRC_IP1_PATH(x)\
	(((x)<<7) & TCC_ASRC_IP1_PATH_MASK)		// IP1_Path
#define TCC_ASRC_IP0_PATH(x)\
	(((x)<<3) & TCC_ASRC_IP0_PATH_MASK)		// IP0_Path

#define TCC_ASRC_IP3_CLKSEL_MASK\
	(0x07<<12)	// IP3_Route Mask
#define TCC_ASRC_IP2_CLKSEL_MASK\
	(0x07<<8)	// IP2_Route Mask
#define TCC_ASRC_IP1_CLKSEL_MASK\
	(0x07<<4)	// IP1_Route Mask
#define TCC_ASRC_IP0_CLKSEL_MASK\
	(0x07<<0)	// IP0_Route Mask
#define TCC_ASRC_IP3_CLKSEL(x)\
	(((x)<<12) & TCC_ASRC_IP3_CLKSEL_MASK)	// IP3_Route
#define TCC_ASRC_IP2_CLKSEL(x)\
	(((x)<<8) & TCC_ASRC_IP2_CLKSEL_MASK)	// IP2_Route
#define TCC_ASRC_IP1_CLKSEL(x)\
	(((x)<<4) & TCC_ASRC_IP1_CLKSEL_MASK)	// IP1_Route
#define TCC_ASRC_IP0_CLKSEL(x)\
	(((x)<<0) & TCC_ASRC_IP0_CLKSEL_MASK)	// IP0_Route

//OUTPORT_CTRL Reg
#define TCC_ASRC_OP3_ROUTE_MASK\
	(0x0f<<28)	// OP3_Route Mask
#define TCC_ASRC_OP2_ROUTE_MASK\
	(0x0f<<24)	// OP2_Route Mask
#define TCC_ASRC_OP1_ROUTE_MASK\
	(0x0f<<20)	// OP1_Route Mask
#define TCC_ASRC_OP0_ROUTE_MASK\
	(0x0f<<16)	// OP0_Route Mask
#define TCC_ASRC_OP3_ROUTE(x)\
	(((x)<<28) & TCC_ASRC_OP3_ROUTE_MASK)	// OP3_Route
#define TCC_ASRC_OP2_ROUTE(x)\
	(((x)<<24) & TCC_ASRC_OP2_ROUTE_MASK)	// OP2_Route
#define TCC_ASRC_OP1_ROUTE(x)\
	(((x)<<20) & TCC_ASRC_OP1_ROUTE_MASK)	// OP1_Route
#define TCC_ASRC_OP0_ROUTE(x)\
	(((x)<<16) & TCC_ASRC_OP0_ROUTE_MASK)	// OP0_Route

#define TCC_ASRC_OP3_PATH_MASK\
	(0x01<<15)	// OP3_Path
#define TCC_ASRC_OP2_PATH_MASK\
	(0x01<<11)	// OP2_Path
#define TCC_ASRC_OP1_PATH_MASK\
	(0x01<<7)	// OP1_Path
#define TCC_ASRC_OP0_PATH_MASK\
	(0x01<<3)	// OP0_Path
#define TCC_ASRC_OP3_PATH(x)\
	(((x)<<15) & TCC_ASRC_OP3_PATH_MASK)	// OP3_Path
#define TCC_ASRC_OP2_PATH(x)\
	(((x)<<11) & TCC_ASRC_OP2_PATH_MASK)	// OP2_Path
#define TCC_ASRC_OP1_PATH(x)\
	(((x)<<7) & TCC_ASRC_OP1_PATH_MASK)		// OP1_Path
#define TCC_ASRC_OP0_PATH(x)\
	(((x)<<3) & TCC_ASRC_OP0_PATH_MASK)		// OP0_Path

#define TCC_ASRC_OP3_CLKSEL_MASK\
	(0x07<<12)	// OP3_Route Mask
#define TCC_ASRC_OP2_CLKSEL_MASK\
	(0x07<<8)	// OP2_Route Mask
#define TCC_ASRC_OP1_CLKSEL_MASK\
	(0x07<<4)	// OP1_Route Mask
#define TCC_ASRC_OP0_CLKSEL_MASK\
	(0x07<<0)	// OP0_Route Mask
#define TCC_ASRC_OP3_CLKSEL(x)\
	(((x)<<12) & TCC_ASRC_OP3_CLKSEL_MASK)	// OP3_Route
#define TCC_ASRC_OP2_CLKSEL(x)\
	(((x)<<8) & TCC_ASRC_OP2_CLKSEL_MASK)	// OP2_Route
#define TCC_ASRC_OP1_CLKSEL(x)\
	(((x)<<4) & TCC_ASRC_OP1_CLKSEL_MASK)	// OP1_Route
#define TCC_ASRC_OP0_CLKSEL(x)\
	(((x)<<0) & TCC_ASRC_OP0_CLKSEL_MASK)	// OP0_Route

enum tcc_asrc_ip_route_t {
	TCC_ASRC_IP_ROUTE_MCAUDIO0_10 = 0,
	TCC_ASRC_IP_ROUTE_MCAUDIO0_32 = 1,
	TCC_ASRC_IP_ROUTE_MCAUDIO0_54 = 2,
	TCC_ASRC_IP_ROUTE_MCAUDIO0_76 = 3,
	TCC_ASRC_IP_ROUTE_MCAUDIO1 = 4,
	TCC_ASRC_IP_ROUTE_MCAUDIO2 = 5,
	TCC_ASRC_IP_ROUTE_MCAUDIO3 = 6,
};

enum tcc_asrc_path_t {
	TCC_ASRC_PATH_EXTIO = 0,
	TCC_ASRC_PATH_DMA = 1,
};

enum tcc_asrc_clksel_t {
	TCC_ASRC_CLKSEL_MCAUDIO0_LRCK = 0,
	TCC_ASRC_CLKSEL_MCAUDIO1_LRCK = 1,
	TCC_ASRC_CLKSEL_MCAUDIO2_LRCK = 2,
	TCC_ASRC_CLKSEL_MCAUDIO3_LRCK = 3,
	TCC_ASRC_CLKSEL_AUXCLK0 = 4,
	TCC_ASRC_CLKSEL_AUXCLK1 = 5,
	TCC_ASRC_CLKSEL_AUXCLK2 = 6,
	TCC_ASRC_CLKSEL_AUXCLK3 = 7,
};

enum tcc_asrc_op_route_t {
	TCC_ASRC_OP_ROUTE_ASRC_PAIR0_10 = 0,
	TCC_ASRC_OP_ROUTE_ASRC_PAIR0_32 = 1,
	TCC_ASRC_OP_ROUTE_ASRC_PAIR0_54 = 2,
	TCC_ASRC_OP_ROUTE_ASRC_PAIR0_76 = 3,
	TCC_ASRC_OP_ROUTE_ASRC_PAIR1 = 4,
	TCC_ASRC_OP_ROUTE_ASRC_PAIR2 = 5,
	TCC_ASRC_OP_ROUTE_ASRC_PAIR3 = 6,
};

//Initial Zero Size Reg
#define TCC_ASRC_SRC_INIT_ZERO_SIZE3_MASK\
	(0x0ff<<24)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE2_MASK\
	(0x0ff<<16)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE1_MASK\
	(0x0ff<<8)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE0_MASK\
	(0x0ff<<0)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE3(x)\
	(((x)<<24) & TCC_ASRC_SRC_INIT_ZERO_SIZE3_MASK)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE2(x)\
	(((x)<<16) & TCC_ASRC_SRC_INIT_ZERO_SIZE2_MASK)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE1(x)\
	(((x)<<8) & TCC_ASRC_SRC_INIT_ZERO_SIZE1_MASK)
#define TCC_ASRC_SRC_INIT_ZERO_SIZE0(x)\
	(((x)<<0) & TCC_ASRC_SRC_INIT_ZERO_SIZE0_MASK)

//Optimum Buffer Level Reg
#define TCC_ASRC_SRC_OPT_BUF_LVL3_MASK\
	(0x0ff<<24)
#define TCC_ASRC_SRC_OPT_BUF_LVL2_MASK\
	(0x0ff<<16)
#define TCC_ASRC_SRC_OPT_BUF_LVL1_MASK\
	(0x0ff<<8)
#define TCC_ASRC_SRC_OPT_BUF_LVL0_MASK\
	(0x0ff<<0)
#define TCC_ASRC_SRC_OPT_BUF_LVL3(x)\
	(((x)<<24) & TCC_ASRC_SRC_OPT_BUF_LVL3_MASK)
#define TCC_ASRC_SRC_OPT_BUF_LVL2(x)\
	(((x)<<16) & TCC_ASRC_SRC_OPT_BUF_LVL2_MASK)
#define TCC_ASRC_SRC_OPT_BUF_LVL1(x)\
	(((x)<<8) & TCC_ASRC_SRC_OPT_BUF_LVL1_MASK)
#define TCC_ASRC_SRC_OPT_BUF_LVL0(x)\
	(((x)<<0) & TCC_ASRC_SRC_OPT_BUF_LVL0_MASK)

//External I/O Format Reg
#define TCC_ASRC_OP3_SWAP\
	(0x01<<30)
#define TCC_ASRC_OP2_SWAP\
	(0x01<<26)
#define TCC_ASRC_OP1_SWAP\
	(0x01<<22)
#define TCC_ASRC_OP0_SWAP\
	(0x01<<18)

#define TCC_ASRC_OP3_FMT_MASK\
	(0x03<<28)
#define TCC_ASRC_OP2_FMT_MASK\
	(0x03<<24)
#define TCC_ASRC_OP1_FMT_MASK\
	(0x03<<20)
#define TCC_ASRC_OP0_FMT_MASK\
	(0x03<<16)
#define TCC_ASRC_OP3_FMT(x)\
	(((x)<<28) & TCC_ASRC_OP3_FMT_MASK)
#define TCC_ASRC_OP2_FMT(x)\
	(((x)<<24) & TCC_ASRC_OP2_FMT_MASK)
#define TCC_ASRC_OP1_FMT(x)\
	(((x)<<20) & TCC_ASRC_OP1_FMT_MASK)
#define TCC_ASRC_OP0_FMT(x)\
	(((x)<<16) & TCC_ASRC_OP0_FMT_MASK)

#define TCC_ASRC_IP3_SWAP\
	(0x01<<14)
#define TCC_ASRC_IP2_SWAP\
	(0x01<<10)
#define TCC_ASRC_IP1_SWAP\
	(0x01<<6)
#define TCC_ASRC_IP0_SWAP\
	(0x01<<2)

#define TCC_ASRC_IP3_FMT_MASK\
	(0x03<<12)
#define TCC_ASRC_IP2_FMT_MASK\
	(0x03<<8)
#define TCC_ASRC_IP1_FMT_MASK\
	(0x03<<4)
#define TCC_ASRC_IP0_FMT_MASK\
	(0x03<<0)
#define TCC_ASRC_IP3_FMT(x)\
	(((x)<<12) & TCC_ASRC_IP3_FMT_MASK)
#define TCC_ASRC_IP2_FMT(x)\
	(((x)<<8) & TCC_ASRC_IP2_FMT_MASK)
#define TCC_ASRC_IP1_FMT(x)\
	(((x)<<4) & TCC_ASRC_IP1_FMT_MASK)
#define TCC_ASRC_IP0_FMT(x)\
	(((x)<<0) & TCC_ASRC_IP0_FMT_MASK)

enum tcc_asrc_ext_io_fmt_t {
	TCC_ASRC_FMT_NO_CHANGE = 0,
	TCC_ASRC_16BIT_LEFT_8BIT = 1,
	TCC_ASRC_16BIT_RIGHT_8BIT = 2,
};

//Sync Period 0/1/2/3 Reg
#define TCC_ASRC_SYNC_PERIOD_CNT_MASK\
	(0x3fff<<0)
#define TCC_ASRC_SYNC_PERIOD_CNT(x)\
	(((x)<<0) & TCC_ASRC_SYNC_PERIOD_CNT_MASK)

//Rate Config 0/1/2/3 Reg
#define TCC_ASRC_MANUAL_RATIO_MASK\
	(0x3ffffff<<0)
#define TCC_ASRC_MANUAL_RATIO(x)\
	(((x)<<0) & TCC_ASRC_MANUAL_RATIO_MASK)
#define TCC_ASRC_SRC_MODE_MASK\
	(0x01<<31)
#define TCC_ASRC_SRC_MODE(x)\
	(((x)<<31) & TCC_ASRC_SRC_MODE_MASK)

enum tcc_asrc_mode_t {
	TCC_ASRC_MODE_ASYNC = 0,
	TCC_ASRC_MODE_SYNC = 1,
};

//Rate Monitor Reg
#define TCC_ASRC_CAL_RATIO_MASK\
	(0x3ffffff<<0)

//SRC Status Reg
#define TCC_ASRC_LOCKED\
	(0x01<<31)
#define TCC_ASRC_OVERFLOW\
	(0x01<<30)
#define TCC_ASRC_UNDERFLOW\
	(0x01<<29)
#define TCC_ASRC_FIFO_LVL_MASK\
	(0x0ffff<<0)

//Mixer Input Enable Reg
#define TCC_ASRC_MIX7_IN_EN3\
	(0x01<<31)
#define TCC_ASRC_MIX7_IN_EN2\
	(0x01<<30)
#define TCC_ASRC_MIX7_IN_EN1\
	(0x01<<29)
#define TCC_ASRC_MIX7_IN_EN0\
	(0x01<<28)
#define TCC_ASRC_MIX6_IN_EN3\
	(0x01<<27)
#define TCC_ASRC_MIX6_IN_EN2\
	(0x01<<26)
#define TCC_ASRC_MIX6_IN_EN1\
	(0x01<<25)
#define TCC_ASRC_MIX6_IN_EN0\
	(0x01<<24)
#define TCC_ASRC_MIX5_IN_EN3\
	(0x01<<23)
#define TCC_ASRC_MIX5_IN_EN2\
	(0x01<<22)
#define TCC_ASRC_MIX5_IN_EN1\
	(0x01<<21)
#define TCC_ASRC_MIX5_IN_EN0\
	(0x01<<20)
#define TCC_ASRC_MIX4_IN_EN3\
	(0x01<<19)
#define TCC_ASRC_MIX4_IN_EN2\
	(0x01<<18)
#define TCC_ASRC_MIX4_IN_EN1\
	(0x01<<17)
#define TCC_ASRC_MIX4_IN_EN0\
	(0x01<<16)
#define TCC_ASRC_MIX3_IN_EN3\
	(0x01<<15)
#define TCC_ASRC_MIX3_IN_EN2\
	(0x01<<14)
#define TCC_ASRC_MIX3_IN_EN1\
	(0x01<<13)
#define TCC_ASRC_MIX3_IN_EN0\
	(0x01<<12)
#define TCC_ASRC_MIX2_IN_EN3\
	(0x01<<11)
#define TCC_ASRC_MIX2_IN_EN2\
	(0x01<<10)
#define TCC_ASRC_MIX2_IN_EN1\
	(0x01<<9)
#define TCC_ASRC_MIX2_IN_EN0\
	(0x01<<8)
#define TCC_ASRC_MIX1_IN_EN3\
	(0x01<<7)
#define TCC_ASRC_MIX1_IN_EN2\
	(0x01<<6)
#define TCC_ASRC_MIX1_IN_EN1\
	(0x01<<5)
#define TCC_ASRC_MIX1_IN_EN0\
	(0x01<<4)
#define TCC_ASRC_MIX0_IN_EN3\
	(0x01<<3)
#define TCC_ASRC_MIX0_IN_EN2\
	(0x01<<2)
#define TCC_ASRC_MIX0_IN_EN1\
	(0x01<<1)
#define TCC_ASRC_MIX0_IN_EN0\
	(0x01<<0)

// Sample Timing Register
#define TCC_ASRC_OP3_TIMING_MASK\
	(0x01<<23)
#define TCC_ASRC_OP2_TIMING_MASK\
	(0x01<<22)
#define TCC_ASRC_OP1_TIMING_MASK\
	(0x01<<21)
#define TCC_ASRC_OP0_TIMING_MASK\
	(0x01<<20)
#define TCC_ASRC_OP3_TIMING(x)\
	(((x)<<23) & TCC_ASRC_OP3_TIMING_MASK)
#define TCC_ASRC_OP2_TIMING(x)\
	(((x)<<22) & TCC_ASRC_OP2_TIMING_MASK)
#define TCC_ASRC_OP1_TIMING(x)\
	(((x)<<21) & TCC_ASRC_OP1_TIMING_MASK)
#define TCC_ASRC_OP0_TIMING(x)\
	(((x)<<20) & TCC_ASRC_OP0_TIMING_MASK)

#define TCC_ASRC_IP3_TIMING_MASK\
	(0x01<<19)
#define TCC_ASRC_IP2_TIMING_MASK\
	(0x01<<18)
#define TCC_ASRC_IP1_TIMING_MASK\
	(0x01<<17)
#define TCC_ASRC_IP0_TIMING_MASK\
	(0x01<<16)
#define TCC_ASRC_IP3_TIMING(x)\
	(((x)<<19) & TCC_ASRC_IP3_TIMING_MASK)
#define TCC_ASRC_IP2_TIMING(x)\
	(((x)<<18) & TCC_ASRC_IP2_TIMING_MASK)
#define TCC_ASRC_IP1_TIMING(x)\
	(((x)<<17) & TCC_ASRC_IP1_TIMING_MASK)
#define TCC_ASRC_IP0_TIMING(x)\
	(((x)<<16) & TCC_ASRC_IP0_TIMING_MASK)

enum tcc_asrc_ip_op_timing_t {
	IP_OP_TIMING_EXTERNEL_CLK = 0,
	IP_OP_TIMING_ASRC_REQUEST = 1,
};

#define TCC_ASRC_MIX_TIMING_MASK\
	(0x03<<0)
#define TCC_ASRC_MIX_TIMING(x)\
	(((x)<<0) & TCC_ASRC_MIX_TIMING_MASK)

// Volume Control Register
#define TCC_ASRC_RAMP_EN3\
	(0x01<<7)
#define TCC_ASRC_RAMP_EN2\
	(0x01<<6)
#define TCC_ASRC_RAMP_EN1\
	(0x01<<5)
#define TCC_ASRC_RAMP_EN0\
	(0x01<<4)
#define TCC_ASRC_VOL_EN3\
	(0x01<<3)
#define TCC_ASRC_VOL_EN2\
	(0x01<<2)
#define TCC_ASRC_VOL_EN1\
	(0x01<<1)
#define TCC_ASRC_VOL_EN0\
	(0x01<<0)

// Volume Gain Register
#define TCC_ASRC_VOL_GAIN_MASK\
	(0x0ffffff<<0)
#define TCC_ASRC_VOL_GAIN(x)\
	(((x)<<0) & TCC_ASRC_VOL_GAIN_MASK)
#define TCC_ASRC_VOL_GAIN_MIN\
	(0x1)	// -120dB

// Volume Ramp Gain Register
#define TCC_ASRC_VOL_RAMP_GAIN_MASK\
	(0x03ff<<0)
#define TCC_ASRC_VOL_RAMP_GAIN(x)\
	(((x)<<0) & TCC_ASRC_VOL_RAMP_GAIN_MASK)
#define TCC_ASRC_VOL_RAMP_GAIN_MAX\
	(0x2D2)	//-90.25dB

// Volume Ramp-UP/DN Config Register
#define TCC_ASRC_VOL_RAMP_TIME_MASK\
	(0x0ff<<24)
#define TCC_ASRC_VOL_RAMP_TIME(x)\
	(((x)<<24) & TCC_ASRC_VOL_RAMP_TIME_MASK)

enum tcc_asrc_ramp_time_t {
	TCC_ASRC_RAMP_64DB_PER_1SAMPLE = 1,
	TCC_ASRC_RAMP_32DB_PER_1SAMPLE = 2,
	TCC_ASRC_RAMP_16DB_PER_1SAMPLE = 3,
	TCC_ASRC_RAMP_8DB_PER_1SAMPLE = 4,
	TCC_ASRC_RAMP_4DB_PER_1SAMPLE = 5,
	TCC_ASRC_RAMP_2DB_PER_1SAMPLE = 6,
	TCC_ASRC_RAMP_1DB_PER_1SAMPLE = 7,
	TCC_ASRC_RAMP_0_5DB_PER_1SAMPLE = 8,
	TCC_ASRC_RAMP_0_25DB_PER_1SAMPLE = 9,
	TCC_ASRC_RAMP_0_125DB_PER_1SAMPLE = 10,
	TCC_ASRC_RAMP_0_125DB_PER_2SAMPLE = 11,
	TCC_ASRC_RAMP_0_125DB_PER_4SAMPLE = 12,
	TCC_ASRC_RAMP_0_125DB_PER_8SAMPLE = 13,
	TCC_ASRC_RAMP_0_125DB_PER_16SAMPLE = 14,
	TCC_ASRC_RAMP_0_125DB_PER_32SAMPLE = 15,
	TCC_ASRC_RAMP_0_125DB_PER_64SAMPLE = 16,
};

#define TCC_ASRC_VOL_RAMP_WAIT_MASK\
	(0x0ffffffff<<0)
#define TCC_ASRC_VOL_RAMP_WAIT(x)\
	(((x)<<0) & TCC_ASRC_VOL_RAMP_WAIT_MASK)

// IRQ Raw Status/Mask/Clear/Enable Register
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define TCC_ASRC_IRQ_FIFO_OUT3_COMPLETE\
	(0x01<<31)
#define TCC_ASRC_IRQ_FIFO_OUT2_COMPLETE\
	(0x01<<30)
#define TCC_ASRC_IRQ_FIFO_OUT1_COMPLETE\
	(0x01<<29)
#define TCC_ASRC_IRQ_FIFO_OUT0_COMPLETE\
	(0x01<<28)
#define TCC_ASRC_IRQ_FIFO_IN3_COMPLETE\
	(0x01<<27)
#define TCC_ASRC_IRQ_FIFO_IN2_COMPLETE\
	(0x01<<26)
#define TCC_ASRC_IRQ_FIFO_IN1_COMPLETE\
	(0x01<<25)
#define TCC_ASRC_IRQ_FIFO_IN0_COMPLETE\
	(0x01<<24)
#endif
#define TCC_ASRC_IRQ_PAIR3_OVERFLOW\
	(0x01<<23)
#define TCC_ASRC_IRQ_PAIR2_OVERFLOW\
	(0x01<<22)
#define TCC_ASRC_IRQ_PAIR1_OVERFLOW\
	(0x01<<21)
#define TCC_ASRC_IRQ_PAIR0_OVERFLOW\
	(0x01<<20)
#define TCC_ASRC_IRQ_PAIR3_UNDERFLOW\
	(0x01<<19)
#define TCC_ASRC_IRQ_PAIR2_UNDERFLOW\
	(0x01<<18)
#define TCC_ASRC_IRQ_PAIR1_UNDERFLOW\
	(0x01<<17)
#define TCC_ASRC_IRQ_PAIR0_UNDERFLOW\
	(0x01<<16)
#define TCC_ASRC_IRQ_FIFO_OUT3_FULL\
	(0x01<<15)
#define TCC_ASRC_IRQ_FIFO_OUT2_FULL\
	(0x01<<14)
#define TCC_ASRC_IRQ_FIFO_OUT1_FULL\
	(0x01<<13)
#define TCC_ASRC_IRQ_FIFO_OUT0_FULL\
	(0x01<<12)
#define TCC_ASRC_IRQ_FIFO_OUT3_EMPTY\
	(0x01<<11)
#define TCC_ASRC_IRQ_FIFO_OUT2_EMPTY\
	(0x01<<10)
#define TCC_ASRC_IRQ_FIFO_OUT1_EMPTY\
	(0x01<<9)
#define TCC_ASRC_IRQ_FIFO_OUT0_EMPTY\
	(0x01<<8)
#define TCC_ASRC_IRQ_FIFO_IN3_FULL\
	(0x01<<7)
#define TCC_ASRC_IRQ_FIFO_IN2_FULL\
	(0x01<<6)
#define TCC_ASRC_IRQ_FIFO_IN1_FULL\
	(0x01<<5)
#define TCC_ASRC_IRQ_FIFO_IN0_FULL\
	(0x01<<4)
#define TCC_ASRC_IRQ_FIFO_IN3_EMPTY\
	(0x01<<3)
#define TCC_ASRC_IRQ_FIFO_IN2_EMPTY\
	(0x01<<2)
#define TCC_ASRC_IRQ_FIFO_IN1_EMPTY\
	(0x01<<1)
#define TCC_ASRC_IRQ_FIFO_IN0_EMPTY\
	(0x01<<0)

//FIFO IN/OUT 0/1/2/3 Control Register
#define TCC_ASRC_FIFO_DMA_EN\
	(0x01<<31)
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define TCC_ASRC_IRQ_PAIR3_EMPTY\
	(0x01<<3)
#define TCC_ASRC_IRQ_PAIR2_EMPTY\
	(0x01<<2)
#define TCC_ASRC_IRQ_PAIR1_EMPTY\
	(0x01<<1)
#define TCC_ASRC_IRQ_PAIR0_EMPTY\
	(0x01<<0)

#define TCC_ASRC_FIFO_SIZE_MASK\
	(0x07<<20)
#define TCC_ASRC_FIFO_SIZE(x)\
	(((x)<<20) & TCC_ASRC_FIFO_SIZE_MASK)
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
#define TCC_ASRC_FIFO_FMT_MASK\
	(0x01<<16)
#define TCC_ASRC_FIFO_FMT(x)\
	(((x)<<16) & TCC_ASRC_FIFO_FMT_MASK)
#define TCC_ASRC_FIFO_THRESHOLD_MASK\
	(0x03<<8)
#define TCC_ASRC_FIFO_THRESHOLD(x)\
	(((x)<<8) & TCC_ASRC_FIFO_THRESHOLD_MASK)
#define TCC_ASRC_FIFO_MODE_MASK\
	(0x0f<<0)
#define TCC_ASRC_FIFO_MODE(x)\
	(((x)<<0) & TCC_ASRC_FIFO_MODE_MASK)

enum tcc_asrc_fifo_fmt_t {
	TCC_ASRC_FIFO_FMT_16BIT = 0,
	TCC_ASRC_FIFO_FMT_24BIT = 1,
};

enum tcc_asrc_fifo_mode_t {
	TCC_ASRC_FIFO_MODE_2CH = 0,
	TCC_ASRC_FIFO_MODE_4CH = 1,
	TCC_ASRC_FIFO_MODE_6CH = 2,
	TCC_ASRC_FIFO_MODE_8CH = 3,
};

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
enum tcc_asrc_fifo_in_size_t {
	TCC_ASRC_FIFO_IN_SIZE_256WORD = 0,
	TCC_ASRC_FIFO_IN_SIZE_128WORD = 1,
	TCC_ASRC_FIFO_IN_SIZE_64WORD = 2,
	TCC_ASRC_FIFO_IN_SIZE_32WORD = 3,
	TCC_ASRC_FIFO_IN_SIZE_16WORD = 4,
	TCC_ASRC_FIFO_IN_SIZE_8WORD = 5,
	TCC_ASRC_FIFO_IN_SIZE_4WORD = 6,
	TCC_ASRC_FIFO_IN_SIZE_2WORD = 7,
};
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)

//FIFO IN/OUT 0/1/2/3 Status Register
#define TCC_ASRC_FIFO_FULL\
	(0x01<<31)
#define TCC_ASRC_FIFO_EMPTY\
	(0x01<<30)
#define TCC_ASRC_FIFO_LEVEL\
	(0x0ffff<<0)

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
//FIFO IN/OUT 0/1/2/3 Control1 Register
#define TCC_ASRC_FIFO_RD_CNT_CLR\
	(0x01<<31)
#define TCC_ASRC_FIFO_RD_CNT_THR_MASK\
	(0x7FFFFFFF)
#define TCC_ASRC_FIFO_RD_CNT_THR(x)\
	(((x)<<0) & TCC_ASRC_FIFO_RD_CNT_THR_MASK)
//FIFO IN/OUT 0/1/2/3 Status1 Register
#define TCC_ASRC_FIFO_RD_CNT_CNT_MASK\
	(0x7FFFFFFF)
#define TCC_ASRC_FIFO_RD_CNT_CNT(x)\
	(((x)<<0) & TCC_ASRC_FIFO_RD_CNT_CNT_MASK)
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)

//FIFO MISC Control  Register
#define TCC_ASRC_DMA_ARB_ROUND_ROBIN\
	(0x01<<0)

struct asrc_reg_t {
	uint32_t enable;
	uint32_t inport_ctrl;
	uint32_t outport_ctrl;
	uint32_t src_init_zero_sz;
	uint32_t src_opt_buf_lvl;
	uint32_t ext_io_fmt;

	uint32_t period_sync_cnt0;
	uint32_t period_sync_cnt1;
	uint32_t period_sync_cnt2;
	uint32_t period_sync_cnt3;

	uint32_t rate_config0;
	uint32_t rate_config1;
	uint32_t rate_config2;
	uint32_t rate_config3;

	uint32_t mix_in_en;
	uint32_t samp_timing;

	uint32_t vol_gain0;
	uint32_t vol_gain1;
	uint32_t vol_gain2;
	uint32_t vol_gain3;

	uint32_t vol_ramp_gain0;
	uint32_t vol_ramp_gain1;
	uint32_t vol_ramp_gain2;
	uint32_t vol_ramp_gain3;

	uint32_t vol_ramp_dn_cfg0;
	uint32_t vol_ramp_dn_cfg1;
	uint32_t vol_ramp_dn_cfg2;
	uint32_t vol_ramp_dn_cfg3;

	uint32_t vol_ramp_up_cfg0;
	uint32_t vol_ramp_up_cfg1;
	uint32_t vol_ramp_up_cfg2;
	uint32_t vol_ramp_up_cfg3;

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	uint32_t irq_enable0;
	uint32_t irq_enable1;
#else
	uint32_t irq_enable;
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	uint32_t fifo_in_ctrl0;
	uint32_t fifo_in_ctrl1;
	uint32_t fifo_in_ctrl2;
	uint32_t fifo_in_ctrl3;

	uint32_t fifo_out_ctrl0;
	uint32_t fifo_out_ctrl1;
	uint32_t fifo_out_ctrl2;
	uint32_t fifo_out_ctrl3;

	uint32_t fifo_misc_ctrl;
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	uint32_t fifo_in_ctrl1_0;
	uint32_t fifo_in_ctrl1_1;
	uint32_t fifo_in_ctrl1_2;
	uint32_t fifo_in_ctrl1_3;

	uint32_t fifo_out_ctrl1_0;
	uint32_t fifo_out_ctrl1_1;
	uint32_t fifo_out_ctrl1_2;
	uint32_t fifo_out_ctrl1_3;

#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
};

void tcc_asrc_dump_regs(void __iomem *asrc_reg);
void tcc_asrc_reset(void __iomem *asrc_reg);
void tcc_asrc_component_reset(void __iomem *asrc_reg,
	enum tcc_asrc_component_t comp);
void tcc_asrc_component_enable(void __iomem *asrc_reg,
	enum tcc_asrc_component_t comp,
	uint32_t enable);
void tcc_asrc_set_inport_path(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_path_t path);
void tcc_asrc_set_outport_path(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_path_t path);
void tcc_asrc_set_zero_init_val(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t ratio_shift22);
void tcc_asrc_set_ratio(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_mode_t sync,
	uint32_t ratio_shift22);
void tcc_asrc_set_opt_buf_lvl(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t lvl);
void tcc_asrc_set_period_sync_cnt(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t cnt);

void tcc_asrc_set_inport_timing(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_ip_op_timing_t timing);
void tcc_asrc_set_outport_timing(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_ip_op_timing_t timing);

void tcc_asrc_volume_enable(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t enable);
void tcc_asrc_volume_ramp_enable(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t enable);
void tcc_asrc_set_volume_gain(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t gain);
void tcc_asrc_set_volume_ramp_gain(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t gain);
void tcc_asrc_set_volume_ramp_dn_time(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t time,
	uint32_t wait);
void tcc_asrc_set_volume_ramp_up_time(void __iomem *asrc_reg,
	int asrc_ch,
	uint32_t time,
	uint32_t wait);

void tcc_asrc_dma_arbitration(void __iomem *asrc_reg,
	uint32_t round_robin);
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
void tcc_asrc_fifo_in_config(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_fifo_fmt_t fmt,
	enum tcc_asrc_fifo_mode_t mode,
	enum tcc_asrc_fifo_in_size_t size,
	uint32_t threshold);
#else
void tcc_asrc_fifo_in_config(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_fifo_fmt_t fmt,
	enum tcc_asrc_fifo_mode_t mode,
	uint32_t threshold);
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
void tcc_asrc_fifo_out_config(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_fifo_fmt_t fmt,
	enum tcc_asrc_fifo_mode_t mode,
	uint32_t threshold);
void tcc_asrc_fifo_in_dma_en(void __iomem *asrc_reg,
	int asrc_ch,
	int enable);
void tcc_asrc_fifo_out_dma_en(void __iomem *asrc_reg,
	int asrc_ch,
	int enable);

uint32_t tcc_asrc_get_fifo_in_status(void __iomem *asrc_reg,
	int asrc_ch);
uint32_t tcc_asrc_get_fifo_out_status(void __iomem *asrc_reg,
	int asrc_ch);
uint32_t tcc_asrc_get_src_status(void __iomem *asrc_reg,
	int asrc_ch);

uint32_t get_fifo_in_phys_addr(uint32_t asrc_reg_phys,
	int asrc_ch);
uint32_t get_fifo_out_phys_addr(uint32_t asrc_reg_phys,
	int asrc_ch);

void tcc_asrc_set_inport_clksel(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_clksel_t clksel);
void tcc_asrc_set_outport_clksel(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_clksel_t clksel);

void tcc_asrc_set_inport_route(void __iomem *asrc_reg,
	int asrc_ch,
	enum tcc_asrc_ip_route_t route);
void tcc_asrc_set_outport_route(void __iomem *asrc_reg,
	int mcaudio_ch,
	enum tcc_asrc_op_route_t route);

void tcc_asrc_set_inport_format(void __iomem *asrc_reg,
	int mcaudio_ch,
	enum tcc_asrc_ext_io_fmt_t fmt,
	int swap);
void tcc_asrc_set_outport_format(void __iomem *asrc_reg,
	int mcaudio_ch,
	enum tcc_asrc_ext_io_fmt_t fmt,
	int swap);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
void tcc_asrc_irq0_fifo_in_read_complete_enable(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_irq0_fifo_in_read_complete_disable(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_irq0_fifo_out_read_complete_enable(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_irq0_fifo_out_read_complete_disable(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_irq1_ringbuf_empty_enable(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_irq1_ringbuf_empty_disable(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_set_fifo_in_read_cnt_threshold(void __iomem *asrc_reg, int asrc_ch,
	uint32_t buffer_size);
void tcc_asrc_clear_fifo_in_read_cnt(void __iomem *asrc_reg,
	int asrc_ch);
void tcc_asrc_set_fifo_out_read_cnt_threshold(void __iomem *asrc_reg,
	int asrc_ch, uint32_t buffer_size);
void tcc_asrc_clear_fifo_out_read_cnt(void __iomem *asrc_reg,
	int asrc_ch);
#endif
void tcc_asrc_reg_backup(void __iomem *asrc_reg,
	struct asrc_reg_t *regs);
void tcc_asrc_reg_restore(void __iomem *asrc_reg,
	struct asrc_reg_t *regs);
#endif /*_TCC_ASRC_H_*/
