/*
 * linux/drivers/video/fbdev/tcc-fb/vioc/vioc_pxdemux.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: TCC VIOC h/w block
 *
 * Copyright (C) 2008-2009 Telechips
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/tcc/vioc_pxdemux.h>

static volatile void __iomem *pPXDEMUX_reg = NULL;

/* VIOC_PXDEMUX_SetConfigure
 * Set pixel demuxer configuration
 * idx : pixel demuxer idx
 * lr : pixel demuxer output - mode 0: even/odd, 1: left/right
 * bypass : pixel demuxer bypass mode
 * width : pixel demuxer width - single port: real width, dual port: half width
 */
void VIOC_PXDEMUX_SetConfigure(unsigned int idx, unsigned int lr,
			       unsigned int bypass, unsigned int width)
{
	volatile void __iomem *reg = VIOC_PXDEMUX_GetAddress();

	if ((idx >= PD_IDX_MAX) || (idx < 0))
		goto error_set_cfg;

	if (reg) {
		unsigned int offset = idx ? PD1_CFG : PD0_CFG;
		unsigned int val = (__raw_readl(reg + offset) &
				    ~(PD_CFG_WIDTH_MASK | PD_CFG_MODE_MASK |
				      PD_CFG_LR_MASK | PD_CFG_BP_MASK));
		val |= (((width & 0xFFF) << PD_CFG_WIDTH_SHIFT) |
			((bypass & 0x1) << PD_CFG_BP_SHIFT) |
			((lr & 0x1) << PD_CFG_LR_SHIFT));
		__raw_writel(val, reg + offset);
	}
	return;
error_set_cfg:
	pr_err("[ERR][P_DEMUX] %s in error, invalid parameter(idx: %d) \n", __func__, idx);
}

/* VIOC_PXDEMUX_SetDataSwap
 * Set pixel demuxter output data swap
 * idx: pixel demuxer idx
 * ch : pixel demuxer output channel(0, 1, 2, 3)
 * set : pixel demuxer data swap mode
 */
void VIOC_PXDEMUX_SetDataSwap(unsigned int idx, unsigned int ch,
			      unsigned int set)
{
	volatile void __iomem *reg = VIOC_PXDEMUX_GetAddress();

	if ((idx >= PD_IDX_MAX) || (idx < 0))
		goto error_data_swap;

	if (reg) {
		unsigned int offset, val;

		offset = idx ? PD1_CFG : PD0_CFG;
		switch (ch) {
		case 0:
			val = (__raw_readl(reg + offset) &
			       ~(PD_CFG_SWAP0_MASK));
			val |= ((set & 0x3) << PD_CFG_SWAP0_SHIFT);
			__raw_writel(val, reg + offset);
			break;
		case 1:
			val = (__raw_readl(reg + offset) &
			       ~(PD_CFG_SWAP1_MASK));
			val |= ((set & 0x3) << PD_CFG_SWAP1_SHIFT);
			__raw_writel(val, reg + offset);
			break;
		case 2:
			val = (__raw_readl(reg + offset) &
			       ~(PD_CFG_SWAP2_MASK));
			val |= ((set & 0x3) << PD_CFG_SWAP2_SHIFT);
			__raw_writel(val, reg + offset);
			break;
		case 3:
			val = (__raw_readl(reg + offset) &
			       ~(PD_CFG_SWAP3_MASK));
			val |= ((set & 0x3) << PD_CFG_SWAP3_SHIFT);
			__raw_writel(val, reg + offset);
			break;
		default:
			pr_err("[ERR][P_DEMUX] %s: invalid parameter(%d, %d)\n", __func__, ch,
			       set);
			break;
		}
	}
	return;
error_data_swap:
	pr_err("[ERR][P_DEMUX] %s: invalid parameter(idx: %d, ch: %d, set: 0x%08x) \n",
	       __func__, idx, ch, set);
}

/* VIOC_PXDEMUX_SetMuxOutput
 * Set MUX output selection
 * mux: the type of mux (PD_MUX3TO1_TYPE, PD_MUX5TO1_TYPE)
 * select : the selecti
 */
void VIOC_PXDEMUX_SetMuxOutput(PD_MUX_TYPE mux, unsigned int ch,
			       unsigned int select, unsigned int enable)
{
	volatile void __iomem *reg = VIOC_PXDEMUX_GetAddress();
	unsigned int val;

	if (reg) {
		switch (mux) {
		case PD_MUX3TO1_TYPE:
			switch (ch) {
			case 0:
				val = (__raw_readl(reg + MUX3_1_SEL0) &
				       ~(MUX3_1_SEL_SEL_MASK));
				val |= ((select & 0x3) << MUX3_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX3_1_SEL0);
				val = (__raw_readl(reg + MUX3_1_EN0) &
				       ~(MUX3_1_EN_EN_MASK));
				val |= ((enable & 0x1) << MUX3_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX3_1_EN0);
				break;
			case 1:
				val = (__raw_readl(reg + MUX3_1_SEL1) &
				       ~(MUX3_1_SEL_SEL_MASK));
				val |= ((select & 0x3) << MUX3_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX3_1_SEL1);
				val = (__raw_readl(reg + MUX3_1_EN1) &
				       ~(MUX3_1_EN_EN_MASK));
				val |= ((enable & 0x1) << MUX3_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX3_1_EN1);
				break;
			default:
				goto error_mux_output;
			}
			break;
		case PD_MUX5TO1_TYPE:
			switch (ch) {
			case 0:
				val = (__raw_readl(reg + MUX5_1_SEL0) &
				       ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL0);
				val = (__raw_readl(reg + MUX5_1_EN0) &
				       ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN0);
				break;
			case 1:
				val = (__raw_readl(reg + MUX5_1_SEL1) &
				       ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL1);
				val = (__raw_readl(reg + MUX5_1_EN1) &
				       ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN1);
				break;
			case 2:
				val = (__raw_readl(reg + MUX5_1_SEL2) &
				       ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL2);
				val = (__raw_readl(reg + MUX5_1_EN2) &
				       ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN2);
				break;
			case 3:
				val = (__raw_readl(reg + MUX5_1_SEL3) &
				       ~(MUX5_1_SEL_SEL_MASK));
				val |= ((select & 0x7) << MUX5_1_SEL_SEL_SHIFT);
				__raw_writel(val, reg + MUX5_1_SEL3);
				val = (__raw_readl(reg + MUX5_1_EN3) &
				       ~(MUX5_1_EN_EN_MASK));
				val |= ((enable & 0x1) << MUX5_1_EN_EN_SHIFT);
				__raw_writel(val, reg + MUX5_1_EN3);
				break;
			default:
				goto error_mux_output;
			}
			break;
		case PD_MUX_TYPE_MAX:
		default:
			goto error_mux_output;
		}
	}
	return;
error_mux_output:
	pr_err("[ERR][P_DEMUX] %s: invalid parameter(mux: %d, ch: %d) \n", __func__,
	       mux, ch);
}

/* VIOC_PXDEMUX_SetDataPath
 * Set Data output format of pixel demuxer
 * ch : channel number of pixel demuxer 5x1
 * path : path number of pixel demuxer 5x1
 * set : data output format of pixel demuxer 5x1
 */
void VIOC_PXDEMUX_SetDataPath(unsigned int ch, unsigned int path,
			      unsigned int set)
{
	volatile void __iomem *reg = VIOC_PXDEMUX_GetAddress();
	unsigned int offset;

	if ((path < 0) || (path >= PD_TXOUT_SEL_MAX))
		goto error_data_path;

	if (reg) {
		switch (ch) {
		case 0:
			offset = TXOUT_SEL0_0;
			break;
		case 1:
			offset = TXOUT_SEL0_1;
			break;
		case 2:
			offset = TXOUT_SEL0_2;
			break;
		case 3:
			offset = TXOUT_SEL0_3;
			break;
		default:
			goto error_data_path;
		}

		__raw_writel((set & 0xFFFFFFFF), reg + (offset + (0x4 * path)));
	}
	return;
error_data_path:
	pr_err("[ERR][P_DEMUX] %s: invalid parameter(ch: %d, path: %d) \n", __func__,
	       ch, path);
}

/* VIOC_PXDEMUX_SetDataArray
 * Set the data output format of pixel demuxer 5x1
 * ch : channel number of pixel demuxer 5x1
 * data : the array included the data output format
 */
void VIOC_PXDEMUX_SetDataArray(unsigned int ch, unsigned int data[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE])
{
	volatile void __iomem *reg = VIOC_PXDEMUX_GetAddress();
	unsigned int *lvdsdata = (unsigned int *)data;
	unsigned int idx, value, path;
	unsigned int data0, data1, data2, data3;

	if ((ch < 0) || (ch >= PD_MUX5TO1_IDX_MAX))
		goto error_data_array;

	if(reg) {
		for(idx = 0; idx < (TXOUT_MAX_LINE * TXOUT_DATA_PER_LINE);  idx +=4)
		{
			data0 = TXOUT_GET_DATA(idx);
			data1 = TXOUT_GET_DATA(idx + 1);
			data2 = TXOUT_GET_DATA(idx + 2);
			data3 = TXOUT_GET_DATA(idx + 3);

			path = idx / 4;
			value = ((lvdsdata[data3] << 24) | (lvdsdata[data2] << 16)| (lvdsdata[data1] << 8)| (lvdsdata[data0]));
			VIOC_PXDEMUX_SetDataPath(ch, path, value);
		}
	}
	return;
error_data_array:
	pr_err("%s in error, invalid parameter(ch: %d) \n",
			__func__, ch);
}

volatile void __iomem *VIOC_PXDEMUX_GetAddress(void)
{
	if (pPXDEMUX_reg == NULL)
		pr_err("[ERR][P_DEMUX] %s pPD:%p \n", __func__, pPXDEMUX_reg);

	return pPXDEMUX_reg;
}

static int __init vioc_pxdemux_init(void)
{
	struct device_node *ViocPXDEMUX_np;
	ViocPXDEMUX_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_pxdemux");

	if (ViocPXDEMUX_np == NULL) {
		pr_info("[INF][P_DEMUX] vioc-pxdemux: disabled\n");
	} else {
		pPXDEMUX_reg = (volatile void __iomem *)of_iomap(ViocPXDEMUX_np, 0);

		if (pPXDEMUX_reg)
			pr_info("[INF][P_DEMUX] vioc-pxdemux: 0x%p\n", pPXDEMUX_reg);
	}
	return 0;
}
arch_initcall(vioc_pxdemux_init);
