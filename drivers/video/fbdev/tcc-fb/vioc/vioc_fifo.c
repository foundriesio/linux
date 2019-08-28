/*
 * linux/arch/arm/mach-tcc893x/vioc_fifo.c
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

#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/tcc/vioc_fifo.h>

static struct device_node *ViocFifo_np;
static volatile void __iomem *pFIFO_reg[VIOC_FIFO_MAX] = {0};

void VIOC_FIFO_ConfigEntry(volatile void __iomem *reg, unsigned int *buf)
{
	unsigned int EEMPTY	= 1;	// emergency empty
	unsigned int EFULL	= 1;	// emergency full
	unsigned int WMT	= 0;	// wdma mode - time
	unsigned int NENTRY	= 4;	// frame memory number  ->  max. frame count is 4.
	unsigned int RMT	= 0;	// rdma mode - time
	unsigned int idxBuf;
	unsigned long val;

	for (idxBuf = 0; idxBuf < NENTRY; idxBuf++)
		__raw_writel(buf[idxBuf], reg + (CH0_BASE + (idxBuf * 0x04)));

	val = (__raw_readl(reg + CH0_CTRL0) &
	       ~(CH_CTRL_EEMPTY_MASK | CH_CTRL_EFULL_MASK | CH_CTRL_WMT_MASK | CH_CTRL_NENTRY_MASK | CH_CTRL_RMT_MASK));
	val |= (((EEMPTY & 0x3) << CH_CTRL_EEMPTY_SHIFT) |
		((EFULL & 0x3) << CH_CTRL_EFULL_SHIFT) |
		((WMT & 0x3) << CH_CTRL_WMT_SHIFT) |
		((NENTRY & 0x3F) << CH_CTRL_NENTRY_SHIFT) |
		((RMT & 0x1) << CH_CTRL_RMT_SHIFT));
	__raw_writel(val, reg + CH0_CTRL0);
}

void VIOC_FIFO_ConfigDMA(volatile void __iomem *reg, unsigned int nWDMA,
	unsigned int nRDMA0, unsigned int nRDMA1, unsigned int nRDMA2)
{
	unsigned long val;
	val = (__raw_readl(reg + CH0_CTRL1) &
		~(CH_CTRL1_RDMA2SEL_MASK | CH_CTRL1_RDMA1SEL_MASK | CH_CTRL1_RDMA0SEL_MASK | CH_CTRL1_WDMASEL_MASK));
	val |= (((nRDMA2 & 0xF) << CH_CTRL1_RDMA2SEL_SHIFT) |
		((nRDMA1 & 0xF) << CH_CTRL1_RDMA1SEL_SHIFT) |
		((nRDMA0 & 0xF) << CH_CTRL1_RDMA0SEL_SHIFT) |
		((nWDMA & 0xF) << CH_CTRL1_WDMASEL_SHIFT));
	__raw_writel(val, reg + CH0_CTRL1);
}

void VIOC_FIFO_SetEnable(volatile void __iomem *reg, unsigned int nWDMA,
	unsigned int nRDMA0, unsigned int nRDMA1, unsigned int nRDMA2)
{
	unsigned long val;
	val = (__raw_readl(reg + CH0_CTRL0) &
	       ~(CH_CTRL_REN2_MASK | CH_CTRL_REN1_MASK | CH_CTRL_REN0_MASK | CH_CTRL_WEN_MASK));
	val |= (((nRDMA2 & 0x1) << CH_CTRL_REN2_SHIFT) |
		((nRDMA1 & 0x1) << CH_CTRL_REN1_SHIFT) |
		((nRDMA0 & 0x1) << CH_CTRL_REN0_SHIFT) |
		((nWDMA & 0x1) << CH_CTRL_WEN_SHIFT));
	__raw_writel(val, reg + CH0_CTRL0);
}

volatile void __iomem *VIOC_FIFO_GetAddress(unsigned int vioc_id)
{
	int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_FIFO_MAX)
		goto err;

	if (pFIFO_reg[Num] == NULL) {
		pr_err("pfifo null pointer address\n");
		goto err;
	}

	return pFIFO_reg[Num];

err:
	pr_err("%s num:%d Max fifo num:%d \n", __func__, Num, VIOC_FIFO_MAX);
	return NULL;
}

static int __init vioc_fifo_init(void)
{
	int i = 0;
	ViocFifo_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_fifo");
	if (ViocFifo_np == NULL) {
		pr_info("vioc-fifo: disabled\n");
	} else {
		for (i = 0; i < VIOC_FIFO_MAX; i++) {
			pFIFO_reg[i] = (volatile void __iomem *)of_iomap(ViocFifo_np,
							is_VIOC_REMAP ? (i + VIOC_FIFO_MAX) : i);

			if(pFIFO_reg[i])
				pr_info("vioc-fifo%d: 0x%p\n", i, pFIFO_reg[i]);
		}
	}
	return 0;
}
arch_initcall(vioc_fifo_init);

