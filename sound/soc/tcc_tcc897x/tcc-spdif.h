/*
 * linux/sound/soc/tcc/tcc-spdif.h
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author:  <linux@telechips.com>
 * Created:    Oct 25, 2016
 * Description: ALSA SPDIF for TCC chip
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

#ifndef _TCC_SPDIF_H
#define _TCC_SPDIF_H

#define TCC_SPDIF_SYSCLK		0
#define TCC_SPDIF_RATES   SNDRV_PCM_RATE_8000_192000

#define SPDIF_TXVERSION	0x000	//Verion Register
#define SPDIF_TXCONFIG	0x004	//Configuration Register
#define SPDIF_TXCHSTAT	0x008	//Channel Status Control Register
#define SPDIF_TXINTMASK	0x00C	//Interrupt Mask Register
#define SPDIF_TXINTSTAT	0x010	//Interrupt Status Register
#define SPDIF_USERDATA	0x080	//0x80~0xDC User Data Buffer
#define SPDIF_CHSTATUS	0x100	//0x100~0x15C Channel Status Buffer
#define SPDIF_TXBUFFER	0x200	//0x200~0x23C Transmit Data Buffer

#define SPDIF_DMACFG		0x400	//Additional Configuration for DMA
#define SPDIF_CSBUDB		0x680	//0x680~0x6DC Merged Window fr CSB/UDB

#define SPDIF_RXVERSION		0x800	//Verion Register
#define SPDIF_RXCONFIG		0x804	//Configuration Register
#define SPDIF_RXSTATUS		0x808	//Signal Status Buffer Register
#define SPDIF_RXINTMASK		0x80C	//Interrupt Mask Register
#define SPDIF_RXINTSTAT		0x810	//Interrupt Status Register
#define SPDIF_RXPHASEDET	0x814
#define SPDIF_RXCAPCTL		0x840	//0x40~0x7C (even) Channel Status Capture Control Register
#define SPDIF_RXCAP			0x840	//0x40~0x7C (odd) Captured Channel Status / User Bit
#define SPDIF_RXBUFFER		0xA00	//0xA00~0xA1C Receive Data Buffer

#define SPDIFRX_OFFSET		SPDIF_RXVERSION	//SPDIF RX ADDR offset

#define spdif_writel	__raw_writel
#define spdif_readl		__raw_readl

struct tcc_spdif_backup_reg {
	unsigned int rTxConfig;
	unsigned int rTxChStat;
	unsigned int rTxIntMask;
	unsigned int rDMACFG;
	unsigned int rRxConfig;
	unsigned int rRxStatus;
	unsigned int rRxIntMask;
};
struct tcc_spdif_data {
	unsigned long	spdif_clk_rate[2];	//[0]SampleRate, [1]SPDIF CLK
	struct tcc_spdif_backup_reg *backup_spdif;
};

#endif
