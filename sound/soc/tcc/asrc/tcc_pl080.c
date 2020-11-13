/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "tcc_pl080.h"

void tcc_pl080_dump_regs(void __iomem *pl080_reg)
{
	pr_info("PL080 Regs(virts:0x%p)\n", pl080_reg);
	pr_info("IntStatus      : 0x%08x\n",
		readl(pl080_reg + PL080_INT_STATUS));
	pr_info("IntTCStatus    : 0x%08x\n",
		readl(pl080_reg + PL080_TC_STATUS));
	pr_info("IntErrorStatus : 0x%08x\n",
		readl(pl080_reg + PL080_ERR_STATUS));
	pr_info("RawIntTCStatus : 0x%08x\n",
		readl(pl080_reg + PL080_RAW_TC_STATUS));
	pr_info("RawIntErrStatus: 0x%08x\n",
		readl(pl080_reg + PL080_RAW_ERR_STATUS));
	pr_info("EnbldChns      : 0x%08x\n",
		readl(pl080_reg + PL080_EN_CHAN));
	pr_info("SoftBreq       : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_BREQ));
	pr_info("SoftSreq       : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_SREQ));
	pr_info("SoftLBRreq     : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_LBREQ));
	pr_info("SoftLSReq      : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_LSREQ));
	pr_info("Configuration  : 0x%08x\n",
		readl(pl080_reg + PL080_CONFIG));
	pr_info("Sync           : 0x%08x\n",
		readl(pl080_reg + PL080_SYNC));

	pr_info("C0_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(0)));
	pr_info("C0_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(0)));
	pr_info("C0_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(0)));
	pr_info("C0_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(0)));
	pr_info("C0_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(0)));

	pr_info("C1_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(1)));
	pr_info("C1_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(1)));
	pr_info("C1_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(1)));
	pr_info("C1_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(1)));
	pr_info("C1_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(1)));

	pr_info("C2_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(2)));
	pr_info("C2_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(2)));
	pr_info("C2_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(2)));
	pr_info("C2_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(2)));
	pr_info("C2_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(2)));

	pr_info("C3_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(3)));
	pr_info("C3_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(3)));
	pr_info("C3_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(3)));
	pr_info("C3_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(3)));
	pr_info("C3_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(3)));

	pr_info("C4_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(4)));
	pr_info("C4_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(4)));
	pr_info("C4_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(4)));
	pr_info("C4_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(4)));
	pr_info("C4_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(4)));

	pr_info("C5_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(5)));
	pr_info("C5_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(5)));
	pr_info("C5_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(5)));
	pr_info("C5_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(5)));
	pr_info("C5_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(5)));

	pr_info("C6_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(6)));
	pr_info("C6_DstAddr     : 0x%08x\n"
		, readl(pl080_reg + PL080_Cx_DST_ADDR(6)));
	pr_info("C6_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(6)));
	pr_info("C6_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(6)));
	pr_info("C6_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(6)));

	pr_info("C7_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(7)));
	pr_info("C7_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(7)));
	pr_info("C7_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(7)));
	pr_info("C7_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(7)));
	pr_info("C7_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(7)));
}

void tcc_pl080_enable(void __iomem *pl080_reg, int enable)
{
	if (enable)
		writel(PL080_CONFIG_ENABLE, pl080_reg+PL080_CONFIG);
	else
		writel(0, pl080_reg+PL080_CONFIG);
}

uint32_t tcc_pl080_get_int_status(void __iomem *pl080_reg)
{
	return readl(pl080_reg+PL080_INT_STATUS);
}

void tcc_pl080_clear_int(void __iomem *pl080_reg, uint32_t bitmask)
{
	writel(bitmask, pl080_reg+PL080_TC_CLEAR);
}

void tcc_pl080_clear_err(void __iomem *pl080_reg, uint32_t bitmask)
{
	writel(bitmask, pl080_reg+PL080_TC_CLEAR);
}

void tcc_pl080_set_first_lli(
	void __iomem *pl080_reg,
	int dma_ch,
	struct pl080_lli *lli)
{
	writel(lli->src_addr, pl080_reg+PL080_Cx_SRC_ADDR(dma_ch));
	writel(lli->dst_addr, pl080_reg+PL080_Cx_DST_ADDR(dma_ch));
	writel(lli->next_lli, pl080_reg+PL080_Cx_LLI(dma_ch));
	writel(lli->control0, pl080_reg+PL080_Cx_CONTROL(dma_ch));
}

void tcc_pl080_set_channel_mem2per(
	void __iomem *pl080_reg,
	int dma_ch,
	enum tcc_peri_id_t dst_peri,
	int irq_en,
	int err_en)
{
	uint32_t val = 0;

	val = (PL080_FLOW_MEM2PER << PL080_CONFIG_FLOW_CONTROL_SHIFT)
		| (dst_peri << PL080_CONFIG_DST_SEL_SHIFT);

	if (irq_en)
		val |= PL080_CONFIG_TC_IRQ_MASK;

	if (err_en)
		val |= PL080_CONFIG_ERR_IRQ_MASK;

	writel(val, pl080_reg + PL080_Cx_CONFIG(dma_ch));
}

void tcc_pl080_set_channel_per2mem(
	void __iomem *pl080_reg,
	int dma_ch,
	enum tcc_peri_id_t src_peri,
	int irq_en,
	int err_en)
{
	uint32_t val = 0;

	val = (PL080_FLOW_PER2MEM << PL080_CONFIG_FLOW_CONTROL_SHIFT)
		| (src_peri << PL080_CONFIG_SRC_SEL_SHIFT);

	if (irq_en)
		val |= PL080_CONFIG_TC_IRQ_MASK;

	if (err_en)
		val |= PL080_CONFIG_ERR_IRQ_MASK;

	writel(val, pl080_reg + PL080_Cx_CONFIG(dma_ch));
}


void tcc_pl080_channel_enable(void __iomem *pl080_reg, int dma_ch, int enable)
{
	uint32_t val;

	val = readl(pl080_reg+PL080_Cx_CONFIG(dma_ch));

	if (enable)
		val |= PL080_CONFIG_ENABLE;
	else
		val &= ~PL080_CONFIG_ENABLE;

	writel(val, pl080_reg+PL080_Cx_CONFIG(dma_ch));
}

void tcc_pl080_halt_enable(void __iomem *pl080_reg, int dma_ch, int enable)
{
	uint32_t val;

	val = readl(pl080_reg+PL080_Cx_CONFIG(dma_ch));

	if (enable)
		val |= PL080_CONFIG_HALT;
	else
		val &= ~PL080_CONFIG_HALT;

	writel(val, pl080_reg+PL080_Cx_CONFIG(dma_ch));
}

void tcc_pl080_channel_sync_mode(void __iomem *pl080_reg, int dma_ch, int sync)
{
	uint32_t val;

	val = readl(pl080_reg+PL080_SYNC);

	if (sync)
		val |= (1 << dma_ch);
	else
		val &= ~(1 << dma_ch);

	writel(val, pl080_reg+PL080_SYNC);
}

uint32_t tcc_pl080_lli_control_value(
	uint32_t transfer_size,
	enum tcc_pl080_width_t src_width,
	int src_incr,
	enum tcc_pl080_width_t dst_width,
	int dst_incr,
	enum tcc_pl080_bsize_t src_bsize,
	enum tcc_pl080_bsize_t dst_bsize,
	int irq_en)
{
	uint32_t val = 0;

	val = (src_width << PL080_CONTROL_SWIDTH_SHIFT)
		| (dst_width << PL080_CONTROL_DWIDTH_SHIFT)
		| (src_bsize << PL080_CONTROL_SB_SIZE_SHIFT)
		| (dst_bsize << PL080_CONTROL_DB_SIZE_SHIFT);
	val	|= ((transfer_size&PL080_CONTROL_TRANSFER_SIZE_MASK) <<
		PL080_CONTROL_TRANSFER_SIZE_SHIFT);

	if (src_incr)
		val |= PL080_CONTROL_SRC_INCR;

	if (dst_incr)
		val |= PL080_CONTROL_DST_INCR;

	if (irq_en)
		val |= PL080_CONTROL_TC_IRQ_EN;

	return val;
}

uint32_t tcc_pl080_get_cur_src_addr(void __iomem *pl080_reg, uint32_t dma_ch)
{
	return readl(pl080_reg + PL080_Cx_SRC_ADDR(dma_ch));
}

uint32_t tcc_pl080_get_cur_dst_addr(void __iomem *pl080_reg, uint32_t dma_ch)
{
	return readl(pl080_reg + PL080_Cx_DST_ADDR(dma_ch));
}

uint32_t tcc_pl080_get_cur_lli_addr(void __iomem *pl080_reg, uint32_t dma_ch)
{
	return readl(pl080_reg + PL080_Cx_LLI(dma_ch));
}
