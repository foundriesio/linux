/*
 * linux/sound/soc/tcc/tcc-i2s.h
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author:  <linux@telechips.com>
 * Created:    Nov 25, 2008
 * Description: ALSA PCM interface for the Intel PXA2xx chip
 *
 * Copyright (C) 2008-2009 Telechips 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 
 * as published by the Free Software Foundation
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

#ifndef _TCC_I2S_H
#define _TCC_I2S_H

/* I2S clock */
#define TCC_I2S_SYSCLK		0

#define TCC_I2S_RATES   SNDRV_PCM_RATE_8000_192000

#define	I2S_DADIR0	0x000		// Digital Audio Input Register 0
#define	I2S_DADIR1	0x004		// Digital Audio Input Register 1
#define	I2S_DADIR2	0x008		// Digital Audio Input Register 2
#define	I2S_DADIR3	0x00C		// Digital Audio Input Register 3
#define	I2S_DADIR4	0x010		// Digital Audio Input Register 4
#define	I2S_DADIR5	0x014		// Digital Audio Input Register 5
#define	I2S_DADIR6	0x018		// Digital Audio Input Register 6
#define	I2S_DADIR7	0x01C		// Digital Audio Input Register 7
#define	I2S_DADOR0	0x020		// Digital Audio Output Register 0
#define	I2S_DADOR1	0x024		// Digital Audio Output Register 1
#define	I2S_DADOR2	0x028		// Digital Audio Output Register 2
#define	I2S_DADOR3	0x02C		// Digital Audio Output Register 3
#define	I2S_DADOR4	0x030		// Digital Audio Output Register 4
#define	I2S_DADOR5	0x034		// Digital Audio Output Register 5
#define	I2S_DADOR6	0x038		// Digital Audio Output Register 6
#define	I2S_DADOR7	0x03C		// Digital Audio Output Register 7
#define	I2S_DAMR	0x040		// Digital Audio Mode Register
#define	I2S_DAVC	0x044		// Digital Audio Volume Control Register
#define	I2S_MCCR0	0x048		// Multi Channel Control Register 0
#define	I2S_MCCR1	0x04C		// Multi Channel Control Register 1
#define	I2S_DRMR	0x050		// Digital Radio Mode Register

#define	CDIF_BYPASS_CICR	0x0A0		// For CDIF Bypass

#define i2s_writel	__raw_writel
#define i2s_readl	__raw_readl

/* Please don't modify all about Audio Filter Clock Define. */
#define M_TCC_MCLK 6		//Min. of multiple DAI MCLK for set Audio Filter Clock.
#define MAX_TCC_AF_PCLK 300*1000*1000 //Max. of Audio Filter Clock
/* Please don't modify all about Audio Filter Clock Define. */

#if defined(CONFIG_ARCH_TCC897X) && defined(CONFIG_ARCH_TCC570X)
#define MAX_TCC_MCLK  625*100*1000 //Max. of Audio DAI MCLK
#else
#define MAX_TCC_MCLK 100*1000*1000 //Max. of Audio DAI MCLK
#endif

#define MIN_TCC_MCLK_FS 256		//Min. of DAI MCLK FS
#define MAX_TCC_MCLK_FS 1024	//Max. of DAI MCLK FS
#if defined(CONFIG_SND_SOC_TCC8971_BOARD_LCN1)
#define FIX_TCC_MCLK_FS 256 //We support 256, 512, 1024 fs.
#endif

#if defined(CONFIG_SND_SOC_TCC8970_BOARD_STB)
#define MAX_TCC_MCLK_FS 256	//Max. of DAI MCLK FS
#define FIX_TCC_MCLK_FS 256 //We support 256, 512, 1024 fs.
#endif

struct tcc_i2s_backup_reg {
	unsigned int reg_damr;
	unsigned int reg_davc;
	unsigned int reg_mccr0;
	unsigned int reg_mccr1;
};
struct tcc_i2s_data {
	unsigned long	dai_clk_rate[2];	//[0]: LRCK, [1]: MCLK

	bool dai_en_ctrl;
	bool cdif_bypass_en;

	struct tcc_i2s_backup_reg *backup_dai;
};

#endif
