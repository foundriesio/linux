/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
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

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "tcc_asrc.h"

void tcc_asrc_dump_regs(void __iomem *asrc_reg)
{
	printk("ASRC Regs (virts:0x%p, phys:0x%08x)\n", asrc_reg, (uint32_t)io_v2p(asrc_reg));
	printk("ENABLE         : 0x%08x\n", readl(asrc_reg + TCC_ASRC_ENABLE_OFFSET));
	printk("INIT           : 0x%08x\n", readl(asrc_reg + TCC_ASRC_INIT_OFFSET));
	printk("INPORT_CTRL    : 0x%08x\n", readl(asrc_reg + TCC_ASRC_INPORT_CTRL_OFFSET));
	printk("OUTPORT_CTRL   : 0x%08x\n", readl(asrc_reg + TCC_ASRC_OUTPORT_CTRL_OFFSET));
	printk("INIT_ZERO_SZ   : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_INIT_ZERO_SZ_OFFSET));
	printk("SRC_OPT_BUF_LVL: 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_OPT_BUF_LVL_OFFSET));
	printk("EXT_IO_FMT     : 0x%08x\n", readl(asrc_reg + TCC_ASRC_EXT_IO_FMT_OFFSET));

	printk("PERIOD_CNT   0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_PERIOD_SYNC_CNT0_OFFSET));
	printk("SRC_RATE     0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_RATE0_OFFSET));
	printk("SRC_CAL_RATE 0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_CAL_RATE0_OFFSET));
	printk("SRC_STATUS   0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_STATUS0_OFFSET));

	printk("PERIOD_CNT   1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_PERIOD_SYNC_CNT1_OFFSET));
	printk("SRC_RATE     1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_RATE1_OFFSET));
	printk("SRC_CAL_RATE 1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_CAL_RATE1_OFFSET));
	printk("SRC_STATUS   1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_STATUS1_OFFSET));

	printk("PERIOD_CNT   2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_PERIOD_SYNC_CNT2_OFFSET));
	printk("SRC_RATE     2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_RATE2_OFFSET));
	printk("SRC_CAL_RATE 2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_CAL_RATE2_OFFSET));
	printk("SRC_STATUS   2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_STATUS2_OFFSET));

	printk("PERIOD_CNT   3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_PERIOD_SYNC_CNT2_OFFSET));
	printk("SRC_RATE     3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_RATE2_OFFSET));
	printk("SRC_CAL_RATE 3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_CAL_RATE2_OFFSET));
	printk("SRC_STATUS   3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SRC_STATUS2_OFFSET));

	printk("MIX_IN_EN      : 0x%08x\n", readl(asrc_reg + TCC_ASRC_MIX_IN_EN_OFFSET));
	printk("SAMP_TIMING    : 0x%08x\n", readl(asrc_reg + TCC_ASRC_SAMP_TIMING_OFFSET));

	printk("VOL_CTRL_EN    : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_CTRL_EN_OFFSET));

	printk("VOL_GAIN     0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_GAIN0_OFFSET));
	printk("RAMP_GAIN    0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_GAIN0_OFFSET));
	printk("RAMP_UP      0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_UP_CFG0_OFFSET));
	printk("RAMP_DN      0 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_DN_CFG0_OFFSET));

	printk("VOL_GAIN     1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_GAIN1_OFFSET));
	printk("RAMP_GAIN    1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_GAIN1_OFFSET));
	printk("RAMP_UP      1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_UP_CFG1_OFFSET));
	printk("RAMP_DN      1 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_DN_CFG1_OFFSET));

	printk("VOL_GAIN     2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_GAIN2_OFFSET));
	printk("RAMP_GAIN    2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_GAIN2_OFFSET));
	printk("RAMP_UP      2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_UP_CFG2_OFFSET));
	printk("RAMP_DN      2 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_DN_CFG2_OFFSET));

	printk("VOL_GAIN     3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_GAIN3_OFFSET));
	printk("RAMP_GAIN    3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_GAIN3_OFFSET));
	printk("RAMP_UP      3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_UP_CFG3_OFFSET));
	printk("RAMP_DN      3 : 0x%08x\n", readl(asrc_reg + TCC_ASRC_VOL_RAMP_DN_CFG3_OFFSET));

	printk("IRQ_RAW_STATUS : 0x%08x\n", readl(asrc_reg + TCC_ASRC_IRQ_RAW_STATUS_OFFSET));
	printk("IRQ_MASK_STATUS: 0x%08x\n", readl(asrc_reg + TCC_ASRC_IRQ_MASK_STATUS_OFFSET));
	printk("IRQ_ENABLE     : 0x%08x\n", readl(asrc_reg + TCC_ASRC_IRQ_ENABLE_OFFSET));

	printk("FIFO_IN_CTRL  0: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN0_CTRL_OFFSET));
	printk("FIFO_IN_STATUS0: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN0_STATUS_OFFSET));
	printk("FIFO_IN_CTRL  1: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN1_CTRL_OFFSET));
	printk("FIFO_IN_STATUS1: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN1_STATUS_OFFSET));
	printk("FIFO_IN_CTRL  2: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN2_CTRL_OFFSET));
	printk("FIFO_IN_STATUS2: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN2_STATUS_OFFSET));
	printk("FIFO_IN_CTRL  3: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN3_CTRL_OFFSET));
	printk("FIFO_IN_STATUS3: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_IN3_STATUS_OFFSET));

	printk("FIFO_OUT_CTRL  0: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT0_CTRL_OFFSET));
	printk("FIFO_OUT_STATUS0: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT0_STATUS_OFFSET));
	printk("FIFO_OUT_CTRL  1: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT1_CTRL_OFFSET));
	printk("FIFO_OUT_STATUS1: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT1_STATUS_OFFSET));
	printk("FIFO_OUT_CTRL  2: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT2_CTRL_OFFSET));
	printk("FIFO_OUT_STATUS2: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT2_STATUS_OFFSET));
	printk("FIFO_OUT_CTRL  3: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT3_CTRL_OFFSET));
	printk("FIFO_OUT_STATUS3: 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_OUT3_STATUS_OFFSET));

	printk("FIFO_MISC_CTRL : 0x%08x\n", readl(asrc_reg + TCC_ASRC_FIFO_MISC_CTRL_OFFSET));
}


void tcc_asrc_reset(void __iomem *asrc_reg)
{
	uint32_t val;

	val = TCC_ASRC_INIT_ASRC0
		| TCC_ASRC_INIT_ASRC1 
		| TCC_ASRC_INIT_ASRC2 
		| TCC_ASRC_INIT_ASRC3 
		| TCC_ASRC_INIT_INPORT
		| TCC_ASRC_INIT_OUTPORT
		| TCC_ASRC_INIT_EXTIO
		| TCC_ASRC_INIT_MIXER;
	writel(val, asrc_reg+TCC_ASRC_INIT_OFFSET);
	writel(0x00, asrc_reg+TCC_ASRC_INIT_OFFSET);
}

void tcc_asrc_component_reset(void __iomem *asrc_reg, enum tcc_asrc_component_t comp)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_INIT_OFFSET) | (1 << comp);
	writel(val, asrc_reg+TCC_ASRC_INIT_OFFSET);

	val &= ~(1 << comp);
	writel(val, asrc_reg+TCC_ASRC_INIT_OFFSET);
}

void tcc_asrc_component_enable(void __iomem *asrc_reg, enum tcc_asrc_component_t comp, uint32_t enable)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_ENABLE_OFFSET);
	if (enable) {
		val |= (1 << comp);
	} else {
		val &= ~(1 << comp);
	}
	writel(val, asrc_reg+TCC_ASRC_ENABLE_OFFSET);
}


void tcc_asrc_set_inport_path(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_path_t path)
{
	uint32_t val;

	val = readl(asrc_reg + TCC_ASRC_INPORT_CTRL_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_IP0_PATH_MASK :
		(asrc_ch == 1) ? ~TCC_ASRC_IP1_PATH_MASK :
		(asrc_ch == 2) ? ~TCC_ASRC_IP2_PATH_MASK : ~TCC_ASRC_IP3_PATH_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_IP0_PATH(path):
		(asrc_ch == 1) ? TCC_ASRC_IP1_PATH(path):
		(asrc_ch == 2) ? TCC_ASRC_IP2_PATH(path): TCC_ASRC_IP3_PATH(path);

	writel(val, asrc_reg+TCC_ASRC_INPORT_CTRL_OFFSET);
}

void tcc_asrc_set_outport_path(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_path_t path)
{
	uint32_t val;

	val = readl(asrc_reg + TCC_ASRC_OUTPORT_CTRL_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_OP0_PATH_MASK :
		(asrc_ch == 1) ? ~TCC_ASRC_OP1_PATH_MASK :
		(asrc_ch == 2) ? ~TCC_ASRC_OP2_PATH_MASK : ~TCC_ASRC_OP3_PATH_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_OP0_PATH(path):
		(asrc_ch == 1) ? TCC_ASRC_OP1_PATH(path):
		(asrc_ch == 2) ? TCC_ASRC_OP2_PATH(path): TCC_ASRC_OP3_PATH(path);

	writel(val, asrc_reg+TCC_ASRC_OUTPORT_CTRL_OFFSET);
}

void tcc_asrc_set_zero_init_val(void __iomem *asrc_reg, int asrc_ch, uint32_t ratio_shift22)
{
	uint32_t val;
	uint32_t zero_init_sz = 32;

	if (ratio_shift22 < 0x400000) {
		if (ratio_shift22) {
			zero_init_sz = (32*0x400000) / ratio_shift22;
		} else {
			zero_init_sz = 0xff;
		}
	}

	//printk("zero_init_sz : %d(0x%x)\n", zero_init_sz, zero_init_sz);
	
	val = readl(asrc_reg+TCC_ASRC_SRC_INIT_ZERO_SZ_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_SRC_INIT_ZERO_SIZE0_MASK :
		(asrc_ch == 1) ? ~TCC_ASRC_SRC_INIT_ZERO_SIZE1_MASK :
		(asrc_ch == 2) ? ~TCC_ASRC_SRC_INIT_ZERO_SIZE2_MASK : ~TCC_ASRC_SRC_INIT_ZERO_SIZE3_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_SRC_INIT_ZERO_SIZE0(zero_init_sz) :
		(asrc_ch == 1) ? TCC_ASRC_SRC_INIT_ZERO_SIZE1(zero_init_sz) :
		(asrc_ch == 2) ? TCC_ASRC_SRC_INIT_ZERO_SIZE2(zero_init_sz) : TCC_ASRC_SRC_INIT_ZERO_SIZE3(zero_init_sz);

	writel(val, asrc_reg+TCC_ASRC_SRC_INIT_ZERO_SZ_OFFSET);
}

void tcc_asrc_set_opt_buf_lvl(void __iomem *asrc_reg, int asrc_ch, uint32_t lvl)
{
	uint32_t val;

	val = readl(asrc_reg + TCC_ASRC_SRC_OPT_BUF_LVL_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_SRC_OPT_BUF_LVL0_MASK :
		(asrc_ch == 1) ? ~TCC_ASRC_SRC_OPT_BUF_LVL1_MASK :
		(asrc_ch == 2) ? ~TCC_ASRC_SRC_OPT_BUF_LVL2_MASK : ~TCC_ASRC_SRC_OPT_BUF_LVL3_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_SRC_OPT_BUF_LVL0(lvl) :
		(asrc_ch == 1) ? TCC_ASRC_SRC_OPT_BUF_LVL1(lvl) :
		(asrc_ch == 2) ? TCC_ASRC_SRC_OPT_BUF_LVL2(lvl) : TCC_ASRC_SRC_OPT_BUF_LVL3(lvl);

	writel(val, asrc_reg+TCC_ASRC_SRC_OPT_BUF_LVL_OFFSET);
}

void tcc_asrc_set_ratio(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_mode_t sync, uint32_t ratio_shift22)
{
	uint32_t val;
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_SRC_RATE0_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_SRC_RATE1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_SRC_RATE2_OFFSET : TCC_ASRC_SRC_RATE3_OFFSET;
			
	val = readl(asrc_reg + offset);

	val &= ~(TCC_ASRC_SRC_MODE_MASK|TCC_ASRC_MANUAL_RATIO_MASK);
	val |= TCC_ASRC_SRC_MODE(sync) | TCC_ASRC_MANUAL_RATIO(ratio_shift22);

	writel(val, asrc_reg+offset);
}

void tcc_asrc_set_period_sync_cnt(void __iomem *asrc_reg, int asrc_ch, uint32_t cnt)
{
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_PERIOD_SYNC_CNT0_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_PERIOD_SYNC_CNT1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_PERIOD_SYNC_CNT2_OFFSET : TCC_ASRC_PERIOD_SYNC_CNT3_OFFSET;
			
	writel(cnt, asrc_reg+offset);
}

void tcc_asrc_set_mixer_timing(void __iomem *asrc_reg, enum tcc_asrc_mixer_timing_t timing)
{
	uint32_t val;

	val = readl(asrc_reg + TCC_ASRC_SAMP_TIMING_OFFSET);

	val &= ~TCC_ASRC_MIX_TIMING_MASK;
	val |= TCC_ASRC_MIX_TIMING(timing);

	writel(val, asrc_reg+TCC_ASRC_SAMP_TIMING_OFFSET);
}

void tcc_asrc_set_inport_timing(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_ip_op_timing_t timing)
{
	uint32_t val;

	val = readl(asrc_reg + TCC_ASRC_SAMP_TIMING_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_IP0_TIMING_MASK :
		(asrc_ch == 1) ? ~TCC_ASRC_IP1_TIMING_MASK :
		(asrc_ch == 2) ? ~TCC_ASRC_IP2_TIMING_MASK : ~TCC_ASRC_IP3_TIMING_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_IP0_TIMING(timing):
		(asrc_ch == 1) ? TCC_ASRC_IP1_TIMING(timing):
		(asrc_ch == 2) ? TCC_ASRC_IP2_TIMING(timing): TCC_ASRC_IP3_TIMING(timing);

	writel(val, asrc_reg+TCC_ASRC_SAMP_TIMING_OFFSET);
}

void tcc_asrc_set_outport_timing(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_ip_op_timing_t timing)
{
	uint32_t val;

	val = readl(asrc_reg + TCC_ASRC_SAMP_TIMING_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_OP0_TIMING_MASK :
		(asrc_ch == 1) ? ~TCC_ASRC_OP1_TIMING_MASK :
		(asrc_ch == 2) ? ~TCC_ASRC_OP2_TIMING_MASK : ~TCC_ASRC_OP3_TIMING_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_OP0_TIMING(timing):
		(asrc_ch == 1) ? TCC_ASRC_OP1_TIMING(timing):
		(asrc_ch == 2) ? TCC_ASRC_OP2_TIMING(timing): TCC_ASRC_OP3_TIMING(timing);

	writel(val, asrc_reg+TCC_ASRC_SAMP_TIMING_OFFSET);
}

void tcc_asrc_set_volume_gain(void __iomem *asrc_reg, int asrc_ch, uint32_t gain)
{
	uint32_t offset;
	uint32_t val;

	offset = (asrc_ch == 0) ? TCC_ASRC_VOL_GAIN0_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_VOL_GAIN1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_VOL_GAIN2_OFFSET : TCC_ASRC_VOL_GAIN3_OFFSET;

	val = TCC_ASRC_VOL_GAIN(gain);
			
	writel(val, asrc_reg+offset);
}

void tcc_asrc_set_volume_ramp_gain(void __iomem *asrc_reg, int asrc_ch, uint32_t gain)
{
	uint32_t offset;
	uint32_t val;

	offset = (asrc_ch == 0) ? TCC_ASRC_VOL_RAMP_GAIN0_OFFSET: 
		(asrc_ch == 1) ? TCC_ASRC_VOL_RAMP_GAIN1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_VOL_RAMP_GAIN2_OFFSET : TCC_ASRC_VOL_RAMP_GAIN3_OFFSET;

	val = TCC_ASRC_VOL_RAMP_GAIN(gain);
			
	writel(val, asrc_reg+offset);
}

void tcc_asrc_set_volume_ramp_dn_time(void __iomem *asrc_reg, int asrc_ch, uint32_t time, uint32_t wait)
{
	uint32_t offset;
	uint32_t val;

	offset = (asrc_ch == 0) ? TCC_ASRC_VOL_RAMP_DN_CFG0_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_VOL_RAMP_DN_CFG1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_VOL_RAMP_DN_CFG2_OFFSET : TCC_ASRC_VOL_RAMP_DN_CFG3_OFFSET;

	val = TCC_ASRC_VOL_RAMP_TIME(time)|TCC_ASRC_VOL_RAMP_WAIT(wait);
			
	writel(val, asrc_reg+offset);
}

void tcc_asrc_set_volume_ramp_up_time(void __iomem *asrc_reg, int asrc_ch, uint32_t time, uint32_t wait)
{
	uint32_t offset;
	uint32_t val;

	offset = (asrc_ch == 0) ? TCC_ASRC_VOL_RAMP_UP_CFG0_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_VOL_RAMP_UP_CFG1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_VOL_RAMP_UP_CFG2_OFFSET : TCC_ASRC_VOL_RAMP_UP_CFG3_OFFSET;

	val = TCC_ASRC_VOL_RAMP_TIME(time)|TCC_ASRC_VOL_RAMP_WAIT(wait);
			
	writel(val, asrc_reg+offset);
}

void tcc_asrc_volume_enable(void __iomem *asrc_reg, int asrc_ch, uint32_t enable)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_VOL_CTRL_EN_OFFSET);

	if (enable) {
		val |= (asrc_ch == 0) ? TCC_ASRC_VOL_EN0 :
			(asrc_ch == 1) ? TCC_ASRC_VOL_EN1 :
			(asrc_ch == 2) ? TCC_ASRC_VOL_EN2 : TCC_ASRC_VOL_EN3;
	} else {
		val &= (asrc_ch == 0) ? ~TCC_ASRC_VOL_EN0 :
			(asrc_ch == 1) ? ~TCC_ASRC_VOL_EN1 :
			(asrc_ch == 2) ? ~TCC_ASRC_VOL_EN2 : ~TCC_ASRC_VOL_EN3;
	}

	writel(val, asrc_reg+TCC_ASRC_VOL_CTRL_EN_OFFSET);
}

void tcc_asrc_volume_ramp_enable(void __iomem *asrc_reg, int asrc_ch, uint32_t enable)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_VOL_CTRL_EN_OFFSET);

	if (enable) {
		val |= (asrc_ch == 0) ? TCC_ASRC_RAMP_EN0 :
			(asrc_ch == 1) ? TCC_ASRC_RAMP_EN1 :
			(asrc_ch == 2) ? TCC_ASRC_RAMP_EN2 : TCC_ASRC_RAMP_EN3;
	} else {
		val &= (asrc_ch == 0) ? ~TCC_ASRC_RAMP_EN0 :
			(asrc_ch == 1) ? ~TCC_ASRC_RAMP_EN1 :
			(asrc_ch == 2) ? ~TCC_ASRC_RAMP_EN2 : ~TCC_ASRC_RAMP_EN3;
	}

	writel(val, asrc_reg+TCC_ASRC_VOL_CTRL_EN_OFFSET);
}


void tcc_asrc_dma_arbitration(void __iomem *asrc_reg, uint32_t round_robin)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_FIFO_MISC_CTRL_OFFSET);

	if (round_robin) {
		val |= TCC_ASRC_DMA_ARB_ROUND_ROBIN;
	} else {
		val &= ~TCC_ASRC_DMA_ARB_ROUND_ROBIN;
	}

	writel(val, asrc_reg+TCC_ASRC_FIFO_MISC_CTRL_OFFSET);
}

void tcc_asrc_fifo_in_config(void __iomem *asrc_reg, 
		int asrc_ch, 
		enum tcc_asrc_fifo_fmt_t fmt, 
		enum tcc_asrc_fifo_mode_t mode, 
		uint32_t threshold)
{
	uint32_t val;
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_IN0_CTRL_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_FIFO_IN1_CTRL_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_FIFO_IN2_CTRL_OFFSET : TCC_ASRC_FIFO_IN3_CTRL_OFFSET;

	val = readl(asrc_reg+offset);

	val &= ~(TCC_ASRC_FIFO_FMT_MASK|TCC_ASRC_FIFO_THRESHOLD_MASK|TCC_ASRC_FIFO_MODE_MASK);
	val |= TCC_ASRC_FIFO_MODE(mode);
	val |= TCC_ASRC_FIFO_FMT(fmt);
	val |= TCC_ASRC_FIFO_THRESHOLD(threshold);

	writel(val, asrc_reg+offset);
}

void tcc_asrc_fifo_out_config(void __iomem *asrc_reg, 
		int asrc_ch, 
		enum tcc_asrc_fifo_fmt_t fmt, 
		enum tcc_asrc_fifo_mode_t mode, 
		uint32_t threshold)
{
	uint32_t val;
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_OUT0_CTRL_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_FIFO_OUT1_CTRL_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_FIFO_OUT2_CTRL_OFFSET : TCC_ASRC_FIFO_OUT3_CTRL_OFFSET;

	val = readl(asrc_reg+offset);

	val &= ~(TCC_ASRC_FIFO_FMT_MASK|TCC_ASRC_FIFO_THRESHOLD_MASK|TCC_ASRC_FIFO_MODE_MASK);
	val |= TCC_ASRC_FIFO_MODE(mode);
	val |= TCC_ASRC_FIFO_FMT(fmt);
	val |= TCC_ASRC_FIFO_THRESHOLD(threshold);

	writel(val, asrc_reg+offset);
}

void tcc_asrc_fifo_in_dma_en(void __iomem *asrc_reg, int asrc_ch, int enable)
{
	uint32_t val;
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_IN0_CTRL_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_FIFO_IN1_CTRL_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_FIFO_IN2_CTRL_OFFSET : TCC_ASRC_FIFO_IN3_CTRL_OFFSET;

	val = readl(asrc_reg+offset);

	if (enable){
		val |= TCC_ASRC_FIFO_DMA_EN;
	} else {
		val &= ~TCC_ASRC_FIFO_DMA_EN;
	}

	writel(val, asrc_reg+offset);
}

void tcc_asrc_fifo_out_dma_en(void __iomem *asrc_reg, int asrc_ch, int enable)
{
	uint32_t val;
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_OUT0_CTRL_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_FIFO_OUT1_CTRL_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_FIFO_OUT2_CTRL_OFFSET : TCC_ASRC_FIFO_OUT3_CTRL_OFFSET;

	val = readl(asrc_reg+offset);

	if (enable){
		val |= TCC_ASRC_FIFO_DMA_EN;
	} else {
		val &= ~TCC_ASRC_FIFO_DMA_EN;
	}

	writel(val, asrc_reg+offset);
}

uint32_t get_fifo_in_phys_addr(void __iomem *asrc_reg, int asrc_ch)
{
	uint32_t base = (uint32_t)io_v2p(asrc_reg);
	uint32_t offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_IN0_DATA_OFFSET :
		(asrc_ch == 1) ? TCC_ASRC_FIFO_IN1_DATA_OFFSET :
		(asrc_ch == 2) ? TCC_ASRC_FIFO_IN2_DATA_OFFSET : TCC_ASRC_FIFO_IN3_DATA_OFFSET;

	return base + offset;
}

uint32_t get_fifo_out_phys_addr(void __iomem *asrc_reg, int asrc_ch)
{
	uint32_t base = (uint32_t)io_v2p(asrc_reg);
	uint32_t offset = (asrc_ch== 0) ? TCC_ASRC_FIFO_OUT0_DATA_OFFSET :
		(asrc_ch == 1) ? TCC_ASRC_FIFO_OUT1_DATA_OFFSET :
		(asrc_ch == 2) ? TCC_ASRC_FIFO_OUT2_DATA_OFFSET : TCC_ASRC_FIFO_OUT3_DATA_OFFSET;

	return base + offset;
}

uint32_t tcc_asrc_get_fifo_in_status(void __iomem *asrc_reg, int asrc_ch)
{
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_IN0_STATUS_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_FIFO_IN1_STATUS_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_FIFO_IN2_STATUS_OFFSET : TCC_ASRC_FIFO_IN3_STATUS_OFFSET;

	return readl(asrc_reg+offset);
}

uint32_t tcc_asrc_get_fifo_out_status(void __iomem *asrc_reg, int asrc_ch)
{
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_FIFO_OUT0_STATUS_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_FIFO_OUT1_STATUS_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_FIFO_OUT2_STATUS_OFFSET : TCC_ASRC_FIFO_OUT3_STATUS_OFFSET;

	return readl(asrc_reg+offset);
}

uint32_t tcc_asrc_get_src_status(void __iomem *asrc_reg, int asrc_ch)
{
	uint32_t offset;

	offset = (asrc_ch == 0) ? TCC_ASRC_SRC_STATUS0_OFFSET : 
		(asrc_ch == 1) ? TCC_ASRC_SRC_STATUS1_OFFSET : 
		(asrc_ch == 2) ? TCC_ASRC_SRC_STATUS2_OFFSET : TCC_ASRC_SRC_STATUS3_OFFSET;

	return readl(asrc_reg+offset);
}

void tcc_asrc_set_inport_clksel(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_clksel_t clksel)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_INPORT_CTRL_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_IP0_CLKSEL_MASK :
			(asrc_ch == 1) ? ~TCC_ASRC_IP1_CLKSEL_MASK :
			(asrc_ch == 2) ? ~TCC_ASRC_IP2_CLKSEL_MASK : ~TCC_ASRC_IP3_CLKSEL_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_IP0_CLKSEL(clksel) :
			(asrc_ch == 1) ? TCC_ASRC_IP1_CLKSEL(clksel) :
			(asrc_ch == 2) ? TCC_ASRC_IP2_CLKSEL(clksel) : TCC_ASRC_IP3_CLKSEL(clksel) ;

	writel(val, asrc_reg+TCC_ASRC_INPORT_CTRL_OFFSET);
}

void tcc_asrc_set_outport_clksel(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_clksel_t clksel)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_OUTPORT_CTRL_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_OP0_CLKSEL_MASK :
			(asrc_ch == 1) ? ~TCC_ASRC_OP1_CLKSEL_MASK :
			(asrc_ch == 2) ? ~TCC_ASRC_OP2_CLKSEL_MASK : ~TCC_ASRC_OP3_CLKSEL_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_OP0_CLKSEL(clksel) :
			(asrc_ch == 1) ? TCC_ASRC_OP1_CLKSEL(clksel) :
			(asrc_ch == 2) ? TCC_ASRC_OP2_CLKSEL(clksel) : TCC_ASRC_OP3_CLKSEL(clksel) ;

	writel(val, asrc_reg+TCC_ASRC_OUTPORT_CTRL_OFFSET);
}

void tcc_asrc_set_inport_route(void __iomem *asrc_reg, int asrc_ch, enum tcc_asrc_ip_route_t route)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_INPORT_CTRL_OFFSET);

	val &= (asrc_ch == 0) ? ~TCC_ASRC_IP0_ROUTE_MASK:
			(asrc_ch == 1) ? ~TCC_ASRC_IP1_ROUTE_MASK:
			(asrc_ch == 2) ? ~TCC_ASRC_IP2_ROUTE_MASK: ~TCC_ASRC_IP3_ROUTE_MASK;

	val |= (asrc_ch == 0) ? TCC_ASRC_IP0_ROUTE(route) :
			(asrc_ch == 1) ? TCC_ASRC_IP1_ROUTE(route) :
			(asrc_ch == 2) ? TCC_ASRC_IP2_ROUTE(route) : TCC_ASRC_IP3_ROUTE(route) ;

	writel(val, asrc_reg+TCC_ASRC_INPORT_CTRL_OFFSET);
}

void tcc_asrc_set_outport_route(void __iomem *asrc_reg, int mcaudio_ch, enum tcc_asrc_op_route_t route)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_OUTPORT_CTRL_OFFSET);

	val &= (mcaudio_ch == 0) ? ~TCC_ASRC_OP0_ROUTE_MASK:
			(mcaudio_ch == 1) ? ~TCC_ASRC_OP1_ROUTE_MASK:
			(mcaudio_ch == 2) ? ~TCC_ASRC_OP2_ROUTE_MASK: ~TCC_ASRC_OP3_ROUTE_MASK;

	val |= (mcaudio_ch == 0) ? TCC_ASRC_OP0_ROUTE(route) :
			(mcaudio_ch == 1) ? TCC_ASRC_OP1_ROUTE(route) :
			(mcaudio_ch == 2) ? TCC_ASRC_OP2_ROUTE(route) : TCC_ASRC_OP3_ROUTE(route) ;

	writel(val, asrc_reg+TCC_ASRC_OUTPORT_CTRL_OFFSET);
}

void tcc_asrc_set_inport_format(void __iomem *asrc_reg, int mcaudio_ch, enum tcc_asrc_ext_io_fmt_t fmt, int swap)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_EXT_IO_FMT_OFFSET);

	val &= (mcaudio_ch == 0) ? ~TCC_ASRC_IP0_FMT_MASK:
			(mcaudio_ch == 1) ? ~TCC_ASRC_IP1_FMT_MASK:
			(mcaudio_ch == 2) ? ~TCC_ASRC_IP2_FMT_MASK: ~TCC_ASRC_IP3_FMT_MASK;

	val |= (mcaudio_ch == 0) ? TCC_ASRC_IP0_FMT(fmt) :
			(mcaudio_ch == 1) ? TCC_ASRC_IP1_FMT(fmt) :
			(mcaudio_ch == 2) ? TCC_ASRC_IP2_FMT(fmt) : TCC_ASRC_IP3_FMT(fmt) ;
	

	if (swap) {
		val |= (mcaudio_ch == 0) ? TCC_ASRC_IP0_SWAP :
			(mcaudio_ch == 1) ? TCC_ASRC_IP1_SWAP :
			(mcaudio_ch == 2) ? TCC_ASRC_IP2_SWAP : TCC_ASRC_IP3_SWAP;
	} else {
		val &= (mcaudio_ch == 0) ? ~TCC_ASRC_IP0_SWAP :
			(mcaudio_ch == 1) ? ~TCC_ASRC_IP1_SWAP :
			(mcaudio_ch == 2) ? ~TCC_ASRC_IP2_SWAP : ~TCC_ASRC_IP3_SWAP;
	}

	writel(val, asrc_reg+TCC_ASRC_EXT_IO_FMT_OFFSET);
}

void tcc_asrc_set_outport_format(void __iomem *asrc_reg, int mcaudio_ch, enum tcc_asrc_ext_io_fmt_t fmt, int swap)
{
	uint32_t val;

	val = readl(asrc_reg+TCC_ASRC_EXT_IO_FMT_OFFSET);

	val &= (mcaudio_ch == 0) ? ~TCC_ASRC_OP0_FMT_MASK:
			(mcaudio_ch == 1) ? ~TCC_ASRC_OP1_FMT_MASK:
			(mcaudio_ch == 2) ? ~TCC_ASRC_OP2_FMT_MASK: ~TCC_ASRC_OP3_FMT_MASK;

	val |= (mcaudio_ch == 0) ? TCC_ASRC_OP0_FMT(fmt) :
			(mcaudio_ch == 1) ? TCC_ASRC_OP1_FMT(fmt) :
			(mcaudio_ch == 2) ? TCC_ASRC_OP2_FMT(fmt) : TCC_ASRC_OP3_FMT(fmt) ;
	

	if (swap) {
		val |= (mcaudio_ch == 0) ? TCC_ASRC_OP0_SWAP :
			(mcaudio_ch == 1) ? TCC_ASRC_OP1_SWAP :
			(mcaudio_ch == 2) ? TCC_ASRC_OP2_SWAP : TCC_ASRC_OP3_SWAP;
	} else {
		val &= (mcaudio_ch == 0) ? ~TCC_ASRC_OP0_SWAP :
			(mcaudio_ch == 1) ? ~TCC_ASRC_OP1_SWAP :
			(mcaudio_ch == 2) ? ~TCC_ASRC_OP2_SWAP : ~TCC_ASRC_OP3_SWAP;
	}

	writel(val, asrc_reg+TCC_ASRC_EXT_IO_FMT_OFFSET);
}

void tcc_asrc_set_mixer_enable(void __iomem *asrc_reg, int asrc_ch, int mixer_ch, int enable)
{
	uint32_t val;
	uint32_t bit_offset;

	bit_offset = (mixer_ch * 4) + asrc_ch;

	val = readl(asrc_reg+TCC_ASRC_MIX_IN_EN_OFFSET);

	if (enable) {
		val |= (1 << bit_offset);
	} else {
		val &= ~(1 << bit_offset);
	}

	writel(val, asrc_reg+TCC_ASRC_MIX_IN_EN_OFFSET);
}

