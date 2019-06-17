/*
 * linux/sound/soc/tcc/tcc-i2s.c
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author: Liam Girdwood<liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com>
 * Rewritten by:    <linux@telechips.com>
 * Created:     12th Aug 2005   Initial version.
 * Modified:    Nov 25, 2008
 * Description: ALSA PCM interface for the Intel PXA2xx chip
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
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

//#include <linux/clk-private.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <asm/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "tcc-pcm.h"
#include "tcc-spdif.h"
#include "tcc/tca_tcchwcontrol.h"

#undef alsa_dbg
#if 0
#define alsa_dai_dbg(devid, f, a...)  printk("== alsa-debug SPDIF-%d == " f, devid, ##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug SPDIF == " f, ##a)
#else
#define alsa_dai_dbg(devid, f, a...)  
#define alsa_dbg(f, a...)  
#endif

extern bool __clk_is_enabled(struct clk *clk);

//#define TCC_FIX_DIV 4
#define SPDIF_CLK_GAIN 128
static unsigned int tcc_spdif_get_mclk_fs(unsigned int freq, struct clk  *dai_pclk)
{
#if defined(TCC_FIX_DIV)
	unsigned int mindiv = (unsigned int)TCC_FIX_DIV;	
#else
	unsigned int div=1, mindiv=1, mdiff=freq*SPDIF_CLK_GAIN;
	unsigned int temp=0;

	while((SPDIF_CLK_GAIN*div >= SPDIF_CLK_GAIN)&&(SPDIF_CLK_GAIN*div <= 1024)){
		clk_set_rate(dai_pclk, freq*SPDIF_CLK_GAIN*div);
		temp = clk_get_rate(dai_pclk);
		temp = abs(freq*(SPDIF_CLK_GAIN*div)-temp);
		alsa_dbg("[%s]temp=%d, div=%d\n", __func__, temp, div);
		if(temp < mdiff){
			mdiff=temp;
			mindiv=div;
			if(temp == 0)break;
		}
		div++;
	}
#endif
	alsa_dbg("[%s]min. div=%d\n", __func__, mindiv);
	return mindiv;
}

static void tcc_spdif_set_clock(struct snd_soc_dai *dai, unsigned int req_rate, bool stream)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_spdif_data *tcc_spdif = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int clk_rate=0, clk_div=0;
	unsigned int tmpCfg=0, tmpStatus=0;	

	alsa_dai_dbg(prtd->id, "[%s] : clock req rate[%u] \n", __func__, req_rate);

	// Planet 20150812 S/PDIF_Rx Start
	if((tcc_spdif->spdif_clk_rate[0] != req_rate)
			||(tcc_spdif->spdif_clk_rate[1] != clk_get_rate(prtd->ptcc_clk->dai_pclk))) {

		//tmpCfg = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
		tmpStatus = spdif_readl(pdai_reg+SPDIF_TXCHSTAT);
		if(stream == SNDRV_PCM_STREAM_PLAYBACK) {
			spdif_writel(0, pdai_reg+SPDIF_TXCONFIG);	//SPDIF_disable
			clk_div = tcc_spdif_get_mclk_fs(req_rate, prtd->ptcc_clk->dai_pclk);

			if(clk_div <= 0){
				printk("[%s] ERR. tcc_spdif_get_mclk_fs return value %d\n", __func__, clk_div);
			}

			clk_rate = req_rate*SPDIF_CLK_GAIN*clk_div;
			tmpCfg = (((clk_div-1)&0xFF) << 8);
			if (req_rate == 44100) {          /* 44.1KHz */
				tmpStatus = ((tmpStatus & 0xFFFFFF3F) | (0 << 6));
			} else if (req_rate == 48000) {   /* 48KHz */
				tmpStatus = ((tmpStatus & 0xFFFFFF3F) | (1 << 6));
			} else if (req_rate == 32000) {   /* 32KHz */
				tmpStatus = ((tmpStatus & 0xFFFFFF3F) | (2 << 6));
			} else {                            /* Sampling Rate Converter */
				tmpStatus = ((tmpStatus & 0xFFFFFF3F) | (3 << 6));
			}
		}	// Planet 20150812 S/PDIF_Rx End
		if (prtd->ptcc_clk->dai_pclk) {
//			if(prtd->ptcc_clk->dai_pclk->enable_count)
			if(__clk_is_enabled(prtd->ptcc_clk->dai_pclk))
				clk_disable_unprepare(prtd->ptcc_clk->dai_pclk);
			clk_set_rate(prtd->ptcc_clk->dai_pclk, clk_rate);
			clk_prepare_enable(prtd->ptcc_clk->dai_pclk);
			tcc_spdif->spdif_clk_rate[0] = req_rate;
			tcc_spdif->spdif_clk_rate[1] = clk_get_rate(prtd->ptcc_clk->dai_pclk);
		}
		alsa_dai_dbg(prtd->id, "[%s]: req_rate[%u] set_pclk[%u] \n", __func__, req_rate, clk_rate);
		alsa_dai_dbg(prtd->id, "[%s]: spdif_clk_rate[0]:[%lu] spdif_clk_rate[1]:[%lu] \n", __func__, tcc_spdif->spdif_clk_rate[0], tcc_spdif->spdif_clk_rate[1]);

		spdif_writel(tmpCfg, pdai_reg+SPDIF_TXCONFIG);
		spdif_writel(tmpStatus, pdai_reg+SPDIF_TXCHSTAT);
	}
}

static int tcc_spdif_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int reg_value=0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	if (!dai->active) {
		//Hw7: FIFO clear
		spdif_writel(Hw7, pdai_reg+SPDIF_DMACFG);
		msleep(1);
		//Hw11: user data buffer DMA req. enable
		//Hw10: sample data buffer DMA req. enable
		//Hw3~Hw0: [3:0] FIFO Threshold for DMA req.
		reg_value = (Hw11 | Hw10 | Hw1 | Hw0);
		spdif_writel(reg_value, pdai_reg+SPDIF_DMACFG);
	}
	return 0;
}

static int tcc_spdif_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	/*	//For Debug
		struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
		alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
		*/
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			break;
		case SND_SOC_DAIFMT_CBM_CFS:
			break;
		default:
			break;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			break;
	}
	return 0;
}

static int tcc_spdif_set_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	/*	//For Debug
		struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
		alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
		*/
	/*
	   if (clk_id != TCC_SPDIF_SYSCLK)
	   return -ENODEV;  
	   */
	return 0;
}

static int tcc_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	snd_pcm_format_t format = params_format(params);
	unsigned int reg_value=0;
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	// Set SPDIF clock
	tcc_spdif_set_clock(dai, params_rate(params), substream->stream);

	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		//Set Audio or Data Format
		alsa_dbg("%s : runtime->format11 = %d\n", __func__, params_format(params));

		if (format == SNDRV_PCM_FORMAT_U16) {

			reg_value = spdif_readl(pdai_reg+SPDIF_TXCHSTAT);
			reg_value |= Hw0;	//Data Format
			spdif_writel(reg_value, pdai_reg+SPDIF_TXCHSTAT);
			alsa_dbg("%s : set SPDIF TX Channel STATUS to Data format \n",__func__);

			/* SPDIF TX 16bit mode setting */
			reg_value = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
			//Hw23: 16+8bit tx sample format
			reg_value &= ~Hw23;
			spdif_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);

			reg_value = spdif_readl(pdai_reg+SPDIF_DMACFG);
			reg_value &= ~(Hw13|Hw12);	//Clear Read Address LR Mode 
			reg_value |= Hw13;			//Read Address 16bit Mode 
			spdif_writel(reg_value, pdai_reg+SPDIF_DMACFG);

		} else {

			reg_value = spdif_readl(pdai_reg+SPDIF_TXCHSTAT);
			reg_value &= ~Hw0;	//Audio Format
			spdif_writel(reg_value, pdai_reg+SPDIF_TXCHSTAT);
			alsa_dbg("%s : set SPDIF TX Channel STATUS to PCM format \n",__func__);

			if(format == SNDRV_PCM_FORMAT_S24_LE) {

				/* SPDIF TX 24bit mode setting */
				reg_value = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
				//Hw23: 16+8bit tx sample format
				reg_value |= Hw23;
				spdif_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);

				reg_value = spdif_readl(pdai_reg+SPDIF_DMACFG);
				reg_value &= ~(Hw13|Hw12);	//Read Address 24bit Mode 
				spdif_writel(reg_value, pdai_reg+SPDIF_DMACFG);

			} else {
				/* SPDIF TX 16bit mode setting */

				reg_value = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
				//Hw23: 16+8bit tx sample format
				reg_value &= ~Hw23;
				spdif_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);

				reg_value = spdif_readl(pdai_reg+SPDIF_DMACFG);
				reg_value &= ~(Hw13|Hw12);	//Clear Read Address LR Mode 
				reg_value |= Hw13;			//Read Address 16bit Mode 
				spdif_writel(reg_value, pdai_reg+SPDIF_DMACFG);
			}
		}
		/* Clear Data Buffer */
		memset((void *)pdai_reg+SPDIF_USERDATA, 0, 24);
		memset((void *)pdai_reg+SPDIF_CHSTATUS, 0, 24);
		memset((void *)pdai_reg+SPDIF_TXBUFFER, 0, 128);

		/* Clear Interrupt Status */
		//reg_value = spdif_readl(pdai_reg+SPDIF_TXINTSTAT);
		reg_value = (Hw4 | Hw3 | Hw2 | Hw1 );	 
		spdif_writel(reg_value, pdai_reg+SPDIF_TXINTSTAT);

	} else {	//SNDRV_PCM_STREAM_CAPTURE
		spdif_writel(0x0, pdai_reg+SPDIF_RXBUFFER);

		if(format == SNDRV_PCM_FORMAT_S24_LE) {

			/* SPDIF RX 24bit mode setting */
			reg_value = spdif_readl(pdai_reg+SPDIF_RXCONFIG);
			//Hw23: 16+8bit tx sample format
			reg_value |= Hw23;
			spdif_writel(reg_value, pdai_reg+SPDIF_RXCONFIG);

		} else {

			/* SPDIF RX 16bit mode setting */
			reg_value = spdif_readl(pdai_reg+SPDIF_RXCONFIG);
			//Hw23: 16+8bit tx sample format
			reg_value &= ~Hw23;
			spdif_writel(reg_value, pdai_reg+SPDIF_RXCONFIG);

		}
		/* Clear Interrupt Status */
		//reg_value = spdif_readl(pdai_reg+SPDIF_RXINTSTAT);
		reg_value = (Hw4 | Hw3 | Hw2 | Hw1 );	 
		spdif_writel(reg_value, pdai_reg+SPDIF_RXINTSTAT);

		//reg_value = spdif_readl(pdai_reg+SPDIF_RXPHASEDET);
		reg_value = 0x8C0C;	 
		spdif_writel(reg_value, pdai_reg+SPDIF_RXPHASEDET);

	}

	alsa_dai_dbg(prtd->id, "=====================\n");
	alsa_dai_dbg(prtd->id, "rate        : %d\n", params_rate(params));
	alsa_dai_dbg(prtd->id, "channels    : %d\n", params_channels(params));
	alsa_dai_dbg(prtd->id, "period_size : %d\n", params_period_size(params));

	return 0;

}


static int tcc_spdif_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	//	struct tcc_spdif_data *tcc_spdif = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // spdif base addr
	unsigned int reg_value = 0;
	int ret=0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			// Planet 20150812 S/PDIF_Rx Start
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				alsa_dbg("%s() spdif playback start\n", __func__);
				// SPDIF_24BIT_MODE Planet_20160715
				reg_value = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
				//Hw2: Tx Intr Enable
				//Hw1: Data Valid bit
				//Hw0: Tx Enable
				reg_value |= (Hw2 | Hw1 | Hw0);
				spdif_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);
			} else {
				alsa_dbg("%s() S/PDIF recording start\n", __func__);
				// SPDIF_24BIT_MODE Planet_20160715

				reg_value = spdif_readl(pdai_reg+SPDIF_RXCONFIG);
				//Hw4: Sample Data Store
				//Hw3: RxStatus Register Holds Channel
				//Hw2: Rx Intr Enable
				//Hw1: Store Data
				//Hw0: Rx Enable
				reg_value |= (Hw4 | Hw3 | Hw2 | Hw1 | Hw0);
				spdif_writel(reg_value, pdai_reg+SPDIF_RXCONFIG);
			}   // Planet 20150812 S/PDIF_Rx End
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				alsa_dbg("%s() spdif playback stop\n", __func__);
			} else {
				alsa_dbg("%s() S/PDIF recording stop\n", __func__);
			} 
#else	//TCC898x, TCC802x
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				alsa_dbg("%s() spdif playback stop\n", __func__);
				reg_value = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
				//Hw23: 16+8bit tx sample format
				//Hw2: Tx Intr Enable
				//Hw1: Data Valid bit
				//Hw0: Tx Enable
				reg_value &= ~(Hw2 | Hw1 | Hw0);
				spdif_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);
			} else {
				alsa_dbg("%s() S/PDIF recording stop\n", __func__);
				reg_value = spdif_readl(pdai_reg+SPDIF_RXCONFIG);
				//Hw4: Sample Data Store
				//Hw3: RxStatus Register Holds Channel
				//Hw2: Rx Intr Enable
				//Hw1: Store Data
				//Hw0: Rx Enable
				reg_value &= ~(Hw4 | Hw3 | Hw2 | Hw1 | Hw0);
				spdif_writel(reg_value, pdai_reg+SPDIF_RXCONFIG);
			}   
#endif
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

static void tcc_spdif_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
#if !defined(CONFIG_ARCH_TCC897X) && !defined(CONFIG_ARCH_TCC570X)
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // spdif base addr
	unsigned int reg_value = 0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* SPDIF TX disable */
		reg_value = spdif_readl(pdai_reg + SPDIF_TXCONFIG);
		if((reg_value & Hw0) != 0) {
			reg_value &= ~Hw0;
			spdif_writel(reg_value, pdai_reg + SPDIF_TXCONFIG);
		}
	} else {
		/* SPDIF RX disable */
		reg_value = spdif_readl(pdai_reg + SPDIF_RXCONFIG);
		if((reg_value & Hw0) != 0) {
			reg_value &= ~Hw0;
			spdif_writel(reg_value, pdai_reg + SPDIF_RXCONFIG);
		}
	}
#endif
}

static int tcc_spdif_suspend(struct device *dev)
{
	/*
	   static int tcc_spdif_suspend(struct snd_soc_dai *dai)
	   {
	   struct tcc_spdif_data *tcc_spdif = dev_get_drvdata(dai->dev);
	   */
	struct tcc_runtime_data *prtd = dev_get_drvdata(dev);
	struct tcc_spdif_data *tcc_spdif = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int pTXCHSTAT=0;

	tcc_spdif->backup_spdif = (struct tcc_spdif_backup_reg *)kmalloc(sizeof(struct tcc_spdif_backup_reg), GFP_KERNEL);

	if(!tcc_spdif->backup_spdif) {
		alsa_dai_dbg(prtd->id, "[%s] no memory for backup_spdif\n", __func__);
		return -1;
	}
	memset(tcc_spdif->backup_spdif, 0x0, sizeof(struct tcc_spdif_backup_reg));

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	// Backup's all about dai control.
	tcc_spdif->backup_spdif->rTxConfig  = spdif_readl(pdai_reg+SPDIF_TXCONFIG);
	
	pTXCHSTAT = spdif_readl(pdai_reg+SPDIF_TXCHSTAT);
	if(pTXCHSTAT){
		tcc_spdif->backup_spdif->rTxChStat  = pTXCHSTAT;
	} else {
		tcc_spdif->backup_spdif->rTxChStat  = 0;
	}

	tcc_spdif->backup_spdif->rTxIntMask = spdif_readl(pdai_reg+SPDIF_TXINTMASK);

	tcc_spdif->backup_spdif->rDMACFG    = spdif_readl(pdai_reg+SPDIF_DMACFG);

	tcc_spdif->backup_spdif->rRxConfig  = spdif_readl(pdai_reg+SPDIF_RXCONFIG);
	tcc_spdif->backup_spdif->rRxStatus  = spdif_readl(pdai_reg+SPDIF_RXSTATUS);
	tcc_spdif->backup_spdif->rRxIntMask = spdif_readl(pdai_reg+SPDIF_RXINTMASK);

	// Disable all about dai clk
	//if((prtd->ptcc_clk->dai_pclk)&&(prtd->ptcc_clk->dai_pclk->enable_count))
	if(__clk_is_enabled(prtd->ptcc_clk->dai_pclk))
		clk_disable_unprepare(prtd->ptcc_clk->dai_pclk);
	//if((prtd->ptcc_clk->dai_hclk)&&(prtd->ptcc_clk->dai_hclk->enable_count))
	if(__clk_is_enabled(prtd->ptcc_clk->dai_hclk))
		clk_disable_unprepare(prtd->ptcc_clk->dai_hclk);

	return 0;
}

static int tcc_spdif_resume(struct device *dev)
{
	/*
	   static int tcc_spdif_resume(struct snd_soc_dai *dai)
	   {
	   struct tcc_spdif_data *tcc_spdif = dev_get_drvdata(dai->dev);
	   */
	struct tcc_runtime_data *prtd = dev_get_drvdata(dev);
	struct tcc_spdif_data *tcc_spdif = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
#if defined(CONFIG_ARCH_TCC802X)
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;
#endif
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	// Enable all about spdif clk
	//if((prtd->ptcc_clk->dai_hclk)&&(!prtd->ptcc_clk->dai_hclk->enable_count))
	if(prtd->ptcc_clk->dai_hclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_hclk);
	//if((prtd->ptcc_clk->dai_pclk)&&(!prtd->ptcc_clk->dai_pclk->enable_count))
	if(prtd->ptcc_clk->dai_pclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_pclk);

#if !defined(CONFIG_ARCH_TCC897X)&& !defined(CONFIG_ARCH_TCC570X)
	if(prtd->dai_port)    
#if defined(CONFIG_ARCH_TCC898X)
		tca_spdif_port_mux(prtd->id, prtd->dai_port);
#elif defined(CONFIG_ARCH_TCC802X)
	tca_spdif_port_mux(padma_reg, prtd->dai_port);
#endif
#endif	

	// Set dai reg from backup's.
	spdif_writel(tcc_spdif->backup_spdif->rTxConfig , pdai_reg+SPDIF_TXCONFIG);
	spdif_writel(tcc_spdif->backup_spdif->rTxChStat , pdai_reg+SPDIF_TXCHSTAT);
	spdif_writel(tcc_spdif->backup_spdif->rTxIntMask, pdai_reg+SPDIF_TXINTMASK);

	spdif_writel(tcc_spdif->backup_spdif->rDMACFG   , pdai_reg+SPDIF_DMACFG);

	spdif_writel(tcc_spdif->backup_spdif->rRxConfig , pdai_reg+SPDIF_RXCONFIG);
	spdif_writel(tcc_spdif->backup_spdif->rRxStatus , pdai_reg+SPDIF_RXSTATUS);
	spdif_writel(tcc_spdif->backup_spdif->rRxIntMask, pdai_reg+SPDIF_RXINTMASK);

	kfree(tcc_spdif->backup_spdif);
	return 0;
}

static int tcc_spdif_probe(struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_spdif_data *tcc_spdif = prtd->private_data;
#if defined(CONFIG_ARCH_TCC802X)
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;
#endif
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

#if !defined(CONFIG_ARCH_TCC897X)&& !defined(CONFIG_ARCH_TCC570X)
	if(prtd->dai_port)    
#if defined(CONFIG_ARCH_TCC898X)
		tca_spdif_port_mux(prtd->id, prtd->dai_port);
#elif defined(CONFIG_ARCH_TCC802X)
	tca_spdif_port_mux(padma_reg, prtd->dai_port);
#endif
#endif	
	/* clock enable */
	if (prtd->ptcc_clk->dai_hclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_hclk);

	if (prtd->ptcc_clk->dai_pclk) {
		tcc_spdif_set_clock(dai, tcc_spdif->spdif_clk_rate[0], SNDRV_PCM_STREAM_PLAYBACK);
	}
	return 0;
}

static struct snd_soc_dai_ops tcc_spdif_ops = {
	.set_sysclk = tcc_spdif_set_sysclk,
	.set_fmt    = tcc_spdif_set_fmt,
	.startup    = tcc_spdif_startup,
	.shutdown   = tcc_spdif_shutdown,
	.hw_params  = tcc_spdif_hw_params,
	.trigger    = tcc_spdif_trigger,
};

static struct snd_soc_dai_driver tcc_snd_spdif = {
	.name = "tcc-spdif",
	.probe = tcc_spdif_probe,
	/*
	   .suspend = tcc_spdif_suspend,
	   .resume  = tcc_spdif_resume,
	   */
	.playback = {
		.channels_min = 2,
		.channels_max = 8,
		.rates = TCC_SPDIF_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {	// Planet 20150812 S/PDIF_Rx
		.channels_min = 2,
		.channels_max = 2,
		.rates = TCC_SPDIF_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE, 
	},
	.ops   = &tcc_spdif_ops,
	.symmetric_rates = 1,
	//.symmetric_channels = 1,
	//.symmetric_samplebits = 1,
};

static const struct snd_soc_component_driver tcc_spdif_component = {
	.name		= "tcc-spdif",
};

static int soc_tcc_spdif_probe(struct platform_device *pdev)
{
	struct device_node *of_node_adma;
	struct platform_device *pdev_adma;
	struct tcc_adma_drv_data *pdev_adma_data;
	struct tcc_runtime_data *prtd;
	struct tcc_spdif_data *tcc_spdif;
	struct tcc_audio_reg *tcc_reg;
	struct tcc_audio_clk *tcc_clk;
	struct tcc_audio_intr *tcc_intr;
#if defined(CONFIG_ARCH_TCC802X)
	char *pspdif_port;
#endif
	u32 clk_rate;
	int ret = 0;

	/* Allocation for keeping Device Tree Info. START */
	prtd = devm_kzalloc(&pdev->dev, sizeof(struct tcc_runtime_data), GFP_KERNEL);
	if (!prtd) {
		dev_err(&pdev->dev, "no memory for state\n");
		goto err;
	}
	dev_set_drvdata(&pdev->dev, prtd);

	memset(prtd, 0, sizeof(struct tcc_runtime_data));

	tcc_spdif = devm_kzalloc(&pdev->dev, sizeof(struct tcc_spdif_data), GFP_KERNEL);
	if (!tcc_spdif) {
		dev_err(&pdev->dev, "no memory for state\n");
		goto err;
	}
	prtd->private_data = tcc_spdif;
	memset(tcc_spdif, 0, sizeof(struct tcc_spdif_data));

	tcc_reg = devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_reg), GFP_KERNEL);
	if (!tcc_reg) {
		dev_err(&pdev->dev, "no memory for tcc_audio_reg\n");
		goto err;
	}
	prtd->ptcc_reg = tcc_reg;
	memset(tcc_reg, 0, sizeof(struct tcc_audio_reg));

	tcc_clk = devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_clk), GFP_KERNEL);
	if (!tcc_clk) {
		dev_err(&pdev->dev, "no memory for tcc_audio_clk\n");
		goto err;
	}
	prtd->ptcc_clk = tcc_clk;
	memset(tcc_clk, 0, sizeof(struct tcc_audio_clk));

	tcc_intr = devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_intr), GFP_KERNEL);
	if (!tcc_intr) {
		dev_err(&pdev->dev, "no memory for tcc_audio_intr\n");
		goto err;
	}
	prtd->ptcc_intr = tcc_intr;
	memset(tcc_intr, 0, sizeof(struct tcc_audio_intr));

	/* Flag set SPDIF for getting I/F name in PCM driver */
	prtd->id = -1;
	prtd->id = of_alias_get_id(pdev->dev.of_node, "spdif");
	if(prtd->id < 0) {
		dev_err(&pdev->dev, "spdif id[%d] is wrong.\n", prtd->id);
		goto err;
	}
	prtd->flag = TCC_SPDIF_FLAG;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	/* get spdif info. */
	prtd->ptcc_reg->dai_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)prtd->ptcc_reg->dai_reg))
		prtd->ptcc_reg->dai_reg = NULL;

	/* get dai clk info. */
	prtd->ptcc_clk->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(prtd->ptcc_clk->dai_pclk))
		prtd->ptcc_clk->dai_pclk = NULL;
	prtd->ptcc_clk->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(prtd->ptcc_clk->dai_hclk))
		prtd->ptcc_clk->dai_hclk = NULL;

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &clk_rate);
	tcc_spdif->spdif_clk_rate[0] = (unsigned long)clk_rate;
	tcc_spdif->spdif_clk_rate[1] = 1; //Initialize value for set clock. 
	//'0' can't through if condition in tcc_spdif_set_clock()

	prtd->dai_irq = platform_get_irq(pdev, 0);


	of_node_adma = of_parse_phandle(pdev->dev.of_node, "adma", 0);
	/* get adma info */
	if (of_node_adma) {

		pdev_adma = of_find_device_by_node(of_node_adma);
		prtd->ptcc_reg->adma_reg = of_iomap(of_node_adma, 0);
		if (IS_ERR((void *)prtd->ptcc_reg->adma_reg))
			prtd->ptcc_reg->adma_reg = NULL;
		else
			alsa_dai_dbg(prtd->id, "adma_reg=%p\n", prtd->ptcc_reg->adma_reg);

		prtd->adma_irq = platform_get_irq(pdev_adma, 0);

		if(platform_get_drvdata(pdev_adma) == NULL) {
			alsa_dai_dbg(prtd->id, "adma dev data is NULL\n");
			ret= -1;
			goto err_reg_component;
		} else {
			alsa_dai_dbg(prtd->id, "adma dev data isn't NULL\n");
			pdev_adma_data = platform_get_drvdata(pdev_adma);
		}

		if(pdev_adma_data->slock == NULL) {
			alsa_dai_dbg(prtd->id, "pdev_adma_data->slock has some error.\n");
			ret= -1;
			goto err_reg_component;
		} else {
			prtd->adma_slock = pdev_adma_data->slock;
		}

	} else {
		alsa_dai_dbg(prtd->id, "of_node_adma is NULL\n");
		return -1;
	}

	/* get dai port info */
#if defined(CONFIG_ARCH_TCC898X)
	prtd->dai_port = (unsigned int *)kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (prtd->dai_port == NULL) {
		alsa_dai_dbg(prtd->id, "no memory for prtd->dai_port\n");
		ret= -1;
		goto err_reg_component;
	}
	of_property_read_u32(pdev->dev.of_node, "port-mux", prtd->dai_port);
#elif defined(CONFIG_ARCH_TCC802X)
	pspdif_port = (char *)kmalloc(sizeof(char)*2, GFP_KERNEL);	//TX, RX
	if (pspdif_port == NULL) {
		alsa_dai_dbg(prtd->id, "no memory for pspidf_port\n");
		ret= -1;
		goto err_reg_component;
	}
	prtd->dai_port = pspdif_port;	//TX, RX
	memset(pspdif_port, 255, sizeof(char)*2);
	of_property_read_u8_array(pdev->dev.of_node, "port-mux", pspdif_port,
			of_property_count_elems_of_size(pdev->dev.of_node, "port-mux", sizeof(char)));
	//	printk("port[0]=%d, port[1]=%d\n", pspdif_port[0], pspdif_port[1]);
#endif

	ret = snd_soc_register_component(&pdev->dev, &tcc_spdif_component,
			&tcc_snd_spdif, 1);
	if (ret)
		goto err_reg_component;

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);

	ret = tcc_pcm_platform_register(&pdev->dev);

	if (ret)
		goto err_reg_pcm;

	return 0;

err_reg_pcm:
	snd_soc_unregister_component(&pdev->dev);

err_reg_component:
	if (prtd->ptcc_clk->dai_hclk) {
		clk_put(prtd->ptcc_clk->dai_hclk);
		prtd->ptcc_clk->dai_hclk = NULL;
	}
	if (prtd->ptcc_clk->dai_pclk) {
		clk_put(prtd->ptcc_clk->dai_pclk);
		prtd->ptcc_clk->dai_pclk = NULL;
	}

	devm_kfree(&pdev->dev, tcc_intr);
	devm_kfree(&pdev->dev, tcc_clk);
	devm_kfree(&pdev->dev, tcc_reg);
	devm_kfree(&pdev->dev, tcc_spdif);
	devm_kfree(&pdev->dev, prtd);

err:
	return ret;
}

static int  soc_tcc_spdif_remove(struct platform_device *pdev)
{
	struct tcc_runtime_data *prtd = platform_get_drvdata(pdev);
	struct tcc_spdif_data *tcc_spdif = prtd->private_data;
	struct tcc_audio_reg *tcc_reg = prtd->ptcc_reg;
	struct tcc_audio_clk *tcc_clk = prtd->ptcc_clk;
	struct tcc_audio_intr *tcc_intr = prtd->ptcc_intr;
	/*
	   if (tcc_spdif->id == 0)
	   tcc_iec958_pcm_platform_unregister(&pdev->dev);
	   else
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
tcc_iec958_pcm_sub3_platform_unregister(&pdev->dev);
#else
tcc_iec958_pcm_sub1_platform_unregister(&pdev->dev);
#endif
*/
	tcc_pcm_platform_unregister(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);

	if (prtd->ptcc_clk->dai_hclk) {
		clk_put(prtd->ptcc_clk->dai_hclk);
		prtd->ptcc_clk->dai_hclk = NULL;
	}
	if (prtd->ptcc_clk->dai_pclk) {
		clk_put(prtd->ptcc_clk->dai_pclk);
		prtd->ptcc_clk->dai_pclk = NULL;
	}

	devm_kfree(&pdev->dev, tcc_intr);
	devm_kfree(&pdev->dev, tcc_clk);
	devm_kfree(&pdev->dev, tcc_reg);
	devm_kfree(&pdev->dev, tcc_spdif);
	devm_kfree(&pdev->dev, prtd);

	return 0;
}

static struct of_device_id tcc_spdif_of_match[] = {
	{ .compatible = "telechips,spdif" },
	{ }
};

static SIMPLE_DEV_PM_OPS(tcc_spdif_pm_ops, tcc_spdif_suspend,
		tcc_spdif_resume);

static struct platform_driver tcc_spdif_driver = {
	.driver = {
		.name = "tcc-spdif",
		.owner = THIS_MODULE,
		.pm = &tcc_spdif_pm_ops,
		.of_match_table	= of_match_ptr(tcc_spdif_of_match),
	},
	.probe = soc_tcc_spdif_probe,
	.remove = soc_tcc_spdif_remove,
};

static int __init snd_tcc_spdif_init(void)
{
	return platform_driver_register(&tcc_spdif_driver);
}
module_init(snd_tcc_spdif_init);

static void __exit snd_tcc_spdif_exit(void)
{
	return platform_driver_unregister(&tcc_spdif_driver);
}
module_exit(snd_tcc_spdif_exit);

/* Module information */
MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC SPDIF SoC Interface");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tcc_spdif_of_match);
