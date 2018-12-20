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

#ifndef _TCC_PL080_H_
#define _TCC_PL080_H_

#include <linux/amba/pl080.h>

#define PL080_MAX_TRANSFER_SIZE			(1<<11)
#define TRANSFER_UNIT_BYTES				(4)

enum tcc_peri_id_t { 
	PERI_ID_P0_TX = 0,
	PERI_ID_P1_TX = 1,
	PERI_ID_P2_TX = 2,
	PERI_ID_P3_TX = 3,
	PERI_ID_P0_RX = 4,
	PERI_ID_P1_RX = 5,
	PERI_ID_P2_RX = 6,
	PERI_ID_P3_RX = 7,
};

enum tcc_pl080_width_t {
	TCC_PL080_WIDTH_8BIT  = PL080_WIDTH_8BIT,
	TCC_PL080_WIDTH_16BIT = PL080_WIDTH_16BIT,
	TCC_PL080_WIDTH_32BIT = PL080_WIDTH_32BIT,
};

enum tcc_pl080_bsize_t {
	TCC_PL080_BSIZE_1   = PL080_BSIZE_1,
	TCC_PL080_BSIZE_4   = PL080_BSIZE_4,
	TCC_PL080_BSIZE_8   = PL080_BSIZE_8,
	TCC_PL080_BSIZE_16  = PL080_BSIZE_16,
	TCC_PL080_BSIZE_32  = PL080_BSIZE_32,
	TCC_PL080_BSIZE_64  = PL080_BSIZE_64,
	TCC_PL080_BSIZE_128 = PL080_BSIZE_128,
	TCC_PL080_BSIZE_256 = PL080_BSIZE_256,
};

void tcc_pl080_dump_regs(void __iomem *pl080_reg);
void tcc_pl080_enable(void __iomem* pl080_reg, int enable);
uint32_t tcc_pl080_get_int_status(void __iomem* pl080_reg);
void tcc_pl080_clear_int(void __iomem* pl080_reg, uint32_t bitmask);
void tcc_pl080_clear_err(void __iomem* pl080_reg, uint32_t bitmask);
void tcc_pl080_set_first_lli(void __iomem* pl080_reg, int dma_ch, struct pl080_lli *lli);
void tcc_pl080_set_channel_mem2per(void __iomem* pl080_reg,
		int dma_ch,
		enum tcc_peri_id_t dst_peri,
		int irq_en,
		int err_en);
void tcc_pl080_set_channel_per2mem(void __iomem* pl080_reg,
		int dma_ch,
		enum tcc_peri_id_t src_peri,
		int irq_en,
		int err_en);
void tcc_pl080_channel_enable(void __iomem* pl080_reg, int dma_ch, int enable);
void tcc_pl080_halt_enable(void __iomem* pl080_reg, int dma_ch, int enable);
void tcc_pl080_channel_sync_mode(void __iomem* pl080_reg, int dma_ch, int sync);
uint32_t tcc_pl080_lli_control_value(
		uint32_t transfer_size,
		enum tcc_pl080_width_t src_width,
		int src_incr,
		enum tcc_pl080_width_t dst_width,
		int dst_incr,
		enum tcc_pl080_bsize_t src_bsize,
		enum tcc_pl080_bsize_t dst_bsize,
		int irq_en);

uint32_t tcc_pl080_get_cur_src_addr(void __iomem* pl080_reg, uint32_t dma_ch);
uint32_t tcc_pl080_get_cur_dst_addr(void __iomem* pl080_reg, uint32_t dma_ch);
uint32_t tcc_pl080_get_cur_lli_addr(void __iomem* pl080_reg, uint32_t dma_ch);

#endif /*_TCC_PL080_H_*/
