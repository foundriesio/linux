/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
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

#ifndef _TCC_PORT_MUX_H_
#define _TCC_PORT_MUX_H_

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "tcc_audio_hw.h"

#if 0 //DEBUG
#define chmux_writel(v,c)			({printk("<ASoC> IOCFG_REG(%p) = 0x%08x\n", c, (unsigned int)v); writel(v,c); })
#else
#define chmux_writel(v,c)			writel(v,c)
#endif

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)

#define IOBUSCFG_DAI_PORT_MAX		(3)
#define IOBUSCFG_CDIF_PORT_MAX		(3)
#define IOBUSCFG_SPDIF_PORT_MAX		(3)

static inline void iobuscfg_dai_chmux(void __iomem *iobuscfg_reg, int id, int port)
{
	ptrdiff_t offset = (id == 0) ? IOBUS_CFG_DAI0_CHMUX : IOBUS_CFG_DAI1_CHMUX;

	if (port <= IOBUSCFG_DAI_PORT_MAX) {
		chmux_writel(_VAL2FLD(IOBUS_CFG_CHMUX_SEL, (1<<port)), iobuscfg_reg+offset);
	}
}

static inline void iobuscfg_cdif_chmux(void __iomem *iobuscfg_reg, int id, int port)
{
	ptrdiff_t offset = (id == 0) ? IOBUS_CFG_CDIF0_CHMUX : IOBUS_CFG_CDIF1_CHMUX;

	if (port <= IOBUSCFG_CDIF_PORT_MAX) {
		chmux_writel(_VAL2FLD(IOBUS_CFG_CHMUX_SEL, (1<<port)), iobuscfg_reg+offset);
	}
}

static inline void iobuscfg_spdif_chmux(void __iomem *iobuscfg_reg, int id, int port)
{
	ptrdiff_t offset = (id == 0) ? IOBUS_CFG_SPDIF0_CHMUX : IOBUS_CFG_SPDIF1_CHMUX;

	if (port <= IOBUSCFG_SPDIF_PORT_MAX) {
		chmux_writel(_VAL2FLD(IOBUS_CFG_CHMUX_SEL, (1<<port)), iobuscfg_reg+offset);
	}
}

#elif defined(CONFIG_ARCH_TCC803X)

#define IOBUSCFG_DAI_71CH_PORT_MAX		(3)
#define IOBUSCFG_DAI_SRCH_PORT_MAX		(5)
#define IOBUSCFG_SPDIF_71CH_PORT_MAX	(6)
#define IOBUSCFG_SPDIF_SRCH_PORT_MAX	(6)

static inline void iobuscfg_dai_chmux(void __iomem *iobuscfg_reg, int id, int port)
{
	ptrdiff_t offset = (id == 0) ? IOBUS_CFG_DAI_71CH_CHMUX :
					  (id == 1) ? IOBUS_CFG_DAI_71CH_CHMUX :
					  (id == 2) ? IOBUS_CFG_DAI_71CH_CHMUX : IOBUS_CFG_DAI_SRCH_CHMUX;

	uint32_t pos = (id == 0) ? IOBUS_CFG_DAI_CHMUX_71CH_0_SEL_Pos :
				   (id == 1) ? IOBUS_CFG_DAI_CHMUX_71CH_1_SEL_Pos :
				   (id == 2) ? IOBUS_CFG_DAI_CHMUX_71CH_2_SEL_Pos :
				   (id == 3) ? IOBUS_CFG_DAI_CHMUX_SRCH_0_SEL_Pos :
				   (id == 4) ? IOBUS_CFG_DAI_CHMUX_SRCH_1_SEL_Pos :
				   (id == 5) ? IOBUS_CFG_DAI_CHMUX_SRCH_2_SEL_Pos : IOBUS_CFG_DAI_CHMUX_SRCH_3_SEL_Pos;

	uint32_t mask = (id == 0) ? IOBUS_CFG_DAI_CHMUX_71CH_0_SEL_Msk :
					(id == 1) ? IOBUS_CFG_DAI_CHMUX_71CH_1_SEL_Msk :
					(id == 2) ? IOBUS_CFG_DAI_CHMUX_71CH_2_SEL_Msk :
					(id == 3) ? IOBUS_CFG_DAI_CHMUX_SRCH_0_SEL_Msk :
					(id == 4) ? IOBUS_CFG_DAI_CHMUX_SRCH_1_SEL_Msk :
					(id == 5) ? IOBUS_CFG_DAI_CHMUX_SRCH_2_SEL_Msk : IOBUS_CFG_DAI_CHMUX_SRCH_3_SEL_Msk;

	uint32_t value;

	if (id > 6)
		return;

	value = readl(iobuscfg_reg + offset);
	value &= ~mask;

	if (((id < 3) && (port <= IOBUSCFG_DAI_71CH_PORT_MAX)) ||
	 	((id >= 3) && (port <= IOBUSCFG_DAI_SRCH_PORT_MAX)))
		value |= ((1 << port) << pos);

	chmux_writel(value, iobuscfg_reg + offset);
}

static inline void iobuscfg_cdif_chmux(void __iomem *iobuscfg_reg, int id, int port)
{

}

static inline void iobuscfg_spdif_chmux(void __iomem *iobuscfg_reg, int id, int port)
{
	ptrdiff_t offset = (id == 0) ? IOBUS_CFG_SPDIF_71CH0_CHMUX :
					  (id == 1) ? IOBUS_CFG_SPDIF_71CH0_CHMUX :
					  (id == 2) ? IOBUS_CFG_SPDIF_71CH1_CHMUX :
					  (id == 3) ? IOBUS_CFG_SPDIF_SRCH0_CHMUX :
					  (id == 4) ? IOBUS_CFG_SPDIF_SRCH0_CHMUX : IOBUS_CFG_SPDIF_SRCH1_CHMUX;

	uint32_t pos = (id == 0) ? IOBUS_CFG_SPDIF_CHMUX_71CH0_0_SEL_Pos :
				   (id == 1) ? IOBUS_CFG_SPDIF_CHMUX_71CH0_1_SEL_Pos :
				   (id == 2) ? IOBUS_CFG_SPDIF_CHMUX_71CH1_2_SEL_Pos :
				   (id == 3) ? IOBUS_CFG_SPDIF_CHMUX_SRCH0_0_SEL_Pos :
				   (id == 4) ? IOBUS_CFG_SPDIF_CHMUX_SRCH0_1_SEL_Pos :
				   (id == 5) ? IOBUS_CFG_SPDIF_CHMUX_SRCH1_2_SEL_Pos : IOBUS_CFG_SPDIF_CHMUX_SRCH1_3_SEL_Pos;

	uint32_t mask = (id == 0) ? IOBUS_CFG_SPDIF_CHMUX_71CH0_0_SEL_Msk :
					(id == 1) ? IOBUS_CFG_SPDIF_CHMUX_71CH0_1_SEL_Msk :
					(id == 2) ? IOBUS_CFG_SPDIF_CHMUX_71CH1_2_SEL_Msk :
					(id == 3) ? IOBUS_CFG_SPDIF_CHMUX_SRCH0_0_SEL_Msk :
					(id == 4) ? IOBUS_CFG_SPDIF_CHMUX_SRCH0_1_SEL_Msk :
					(id == 5) ? IOBUS_CFG_SPDIF_CHMUX_SRCH1_2_SEL_Msk : IOBUS_CFG_SPDIF_CHMUX_SRCH1_3_SEL_Msk;

	uint32_t value;

	if (id > 6)
		return;

	value = readl(iobuscfg_reg + offset);
	value &= ~mask;

	if (((id < 3) && (port <= IOBUSCFG_SPDIF_71CH_PORT_MAX)) ||
		((id >= 3) && (port <= IOBUSCFG_SPDIF_SRCH_PORT_MAX)))
		value |= ((1 << port) << pos);

	chmux_writel(value, iobuscfg_reg + offset);
}

#elif defined(CONFIG_ARCH_TCC802X)
struct tcc_gfb_i2s_port {
	char clk[3]; //mclk, bclk, lrck
	char daout[4]; //dai0~3
	char dain[4]; //dao0~3
};

struct tcc_gfb_spdif_port {
	char port[2]; //tx, rx
};

struct tcc_gfb_cdif_port {
	char port[3]; //bclk, lrck, dai
};

static inline void tcc_gfb_dump_portcfg(void __iomem *pcfg_reg)
{
	printk("PCFG0 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG0_OFFSET));
	printk("PCFG1 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG1_OFFSET));
	printk("PCFG2 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG2_OFFSET));
	printk("PCFG3 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG3_OFFSET));
}

static inline void tcc_gfb_i2s_portcfg(void __iomem *pcfg_reg, struct tcc_gfb_i2s_port *port)
{
	uint32_t pcfg0 = readl(pcfg_reg + TCC_AUDIO_PCFG0_OFFSET);
	uint32_t pcfg1 = readl(pcfg_reg + TCC_AUDIO_PCFG1_OFFSET);
	uint32_t pcfg2 = readl(pcfg_reg + TCC_AUDIO_PCFG2_OFFSET);

	pcfg0 &= ~(PCFG0_DAI_DI0_Msk|PCFG0_DAI_DI1_Msk|PCFG0_DAI_DI2_Msk|PCFG0_DAI_DI3_Msk);
	pcfg1 &= ~(PCFG1_DAI_LRCK_Msk|PCFG1_DAI_BCLK_Msk|PCFG1_DAI_DO0_Msk|PCFG1_DAI_DO1_Msk);
	pcfg2 &= ~(PCFG2_DAI_DO2_Msk|PCFG2_DAI_DO3_Msk|PCFG2_DAI_MCLK_Msk);

	pcfg0 |= _VAL2FLD(PCFG0_DAI_DI0, port->dain[0]) |
			 _VAL2FLD(PCFG0_DAI_DI1, port->dain[1]) |
			 _VAL2FLD(PCFG0_DAI_DI2, port->dain[2]) |
			 _VAL2FLD(PCFG0_DAI_DI3, port->dain[3]);

	pcfg1 |= _VAL2FLD(PCFG1_DAI_DO0, port->daout[0]) |
			 _VAL2FLD(PCFG1_DAI_DO1, port->daout[1]) |
			 _VAL2FLD(PCFG1_DAI_BCLK, port->clk[1]) |
			 _VAL2FLD(PCFG1_DAI_LRCK, port->clk[2]);

	pcfg2 |= _VAL2FLD(PCFG2_DAI_DO2, port->daout[2]) |
			 _VAL2FLD(PCFG2_DAI_DO3, port->daout[3]) |
			 _VAL2FLD(PCFG2_DAI_MCLK, port->clk[0]);

	chmux_writel(pcfg0, pcfg_reg + TCC_AUDIO_PCFG0_OFFSET);
	chmux_writel(pcfg1, pcfg_reg + TCC_AUDIO_PCFG1_OFFSET);
	chmux_writel(pcfg2, pcfg_reg + TCC_AUDIO_PCFG2_OFFSET);
}

static inline void tcc_gfb_spdif_portcfg(void __iomem *pcfg_reg, struct tcc_gfb_spdif_port *port)
{
	uint32_t pcfg3 = readl(pcfg_reg + TCC_AUDIO_PCFG3_OFFSET);

	pcfg3 &= ~(PCFG3_SPDIF_TX_Msk|PCFG3_SPDIF_RX_Msk);

	pcfg3 |= _VAL2FLD(PCFG3_SPDIF_TX, port->port[0]) |
			 _VAL2FLD(PCFG3_SPDIF_RX, port->port[1]);

	chmux_writel(pcfg3, pcfg_reg + TCC_AUDIO_PCFG3_OFFSET);
}

static inline void tcc_gfb_cdif_portcfg(void __iomem *pcfg_reg, struct tcc_gfb_cdif_port *port)
{
	uint32_t pcfg2 = readl(pcfg_reg + TCC_AUDIO_PCFG2_OFFSET);
	uint32_t pcfg3 = readl(pcfg_reg + TCC_AUDIO_PCFG3_OFFSET);

	pcfg2 &= ~(PCFG2_CD_DI_Msk);
	pcfg3 &= ~(PCFG3_CD_BCLK_Msk|PCFG3_CD_LRCK_Msk);

	pcfg2 |= _VAL2FLD(PCFG2_CD_DI, port->port[2]);
	pcfg3 |= _VAL2FLD(PCFG3_CD_BCLK, port->port[0]) |
			 _VAL2FLD(PCFG3_CD_LRCK, port->port[1]);

	chmux_writel(pcfg2, pcfg_reg + TCC_AUDIO_PCFG2_OFFSET);
	chmux_writel(pcfg3, pcfg_reg + TCC_AUDIO_PCFG3_OFFSET);
}
#endif /* CONFIG_ARCH_TCC802X */

#endif /*_TCC_PORT_MUX_H_*/
