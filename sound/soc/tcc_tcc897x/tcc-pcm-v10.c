/*
 * linux/sound/soc/tcc/tcc-pcm-v10.c  
 *
 * Based on:    linux/sound/arm/pxa2xx-pcm.c
 * Author:  <linux@telechips.com>
 * Created: Nov 30, 2004
 * Modified:    Feb 24, 2016
 * Description: ALSA PCM interface for the Telechips TCC897x chip
 *
 * Copyright:	MontaVista Software, Inc.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tcc-i2s.h"
#include "tcc-spdif.h"
#include "tcc-cdif.h"
#include "tcc-pcm.h"

#include "tcc/tca_tcchwcontrol.h"

#undef alsa_dbg
#if 0
#define alsa_adma_dbg(flag, id, f, a...)  printk("== alsa-debug PCM V10 %s-%d== " f,\
		(flag&TCC_I2S_FLAG)?"I2S":(flag&TCC_SPDIF_FLAG)?"SPDIF":"CDIF",id,##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug PCM V10 == " f, ##a)
#else
#define alsa_adma_dbg(flag, id, f, a...)
#define alsa_dbg(f, a...)
#endif

static int tcc_adma_i2s_tx_enable(struct snd_pcm_substream *substream, int En);
static int tcc_adma_i2s_rx_enable(struct snd_pcm_substream *substream, int En);
static int tcc_adma_spdif_tx_enable(struct snd_pcm_substream *substream, int En);
static int tcc_adma_sp_cdif_rx_enable(struct snd_pcm_substream *substream, int En);

static const struct snd_pcm_hardware tcc_pcm_hardware_play = {
	.info = (SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID
			| SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_PAUSE
			| SNDRV_PCM_INFO_RESUME),

	.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE),
	.rates        = SNDRV_PCM_RATE_8000_192000,
	.rate_min     = 8000,
	.rate_max     = 192000,
	.channels_min = 1,
	.channels_max = 8,
	.period_bytes_min = min_period_size,
	.period_bytes_max = __play_buf_size,
	.periods_min      = min_period_cnt,
#if defined(CONFIG_ANDROID) // Android Style	
	.periods_max      = __play_buf_cnt,
	.buffer_bytes_max = __play_buf_cnt * __play_buf_size,
#else  // Linux Style
	.periods_max      = (__play_buf_cnt * __play_buf_size)/min_period_size,
	.buffer_bytes_max = __play_buf_cnt * __play_buf_size,
#endif	
	.fifo_size = 16,  //ignored
};

static const struct snd_pcm_hardware tcc_pcm_hardware_capture = {
	.info = (SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID
			| SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_PAUSE
			| SNDRV_PCM_INFO_RESUME),

#if defined(CONFIG_VIDEO_HDMI_IN_SENSOR)
	.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#else
	.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#endif
	.rates        = SNDRV_PCM_RATE_8000_192000,
	.rate_min     = 8000,
	.rate_max     = 192000,
	.channels_min = 1,
	.channels_max = 8,
	.period_bytes_min = min_period_size,
	.period_bytes_max = __cap_buf_size,
	.periods_min      = min_period_cnt,
#if defined(CONFIG_ANDROID) // Android Style		
	.periods_max      = __cap_buf_cnt,
	.buffer_bytes_max = __cap_buf_cnt * __cap_buf_size,
#else  // Linux Style	
	.periods_max      = (__cap_buf_cnt * __cap_buf_size)/min_period_size,
	.buffer_bytes_max = __cap_buf_cnt * __cap_buf_size,
#endif	
	.fifo_size = 16, //ignored
};

static void set_dma_outbuffer(struct tcc_runtime_data *prtd, struct snd_pcm_hw_params *params)
{
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	snd_pcm_format_t format = params_format(params);
	unsigned int chs = params_channels(params);
	unsigned int length = params_buffer_bytes(params);
	unsigned int addr = prtd->dma_buffer;
	unsigned int period = min_period_size;
	unsigned int dma_buffer = 0, bit_count=0, reg_value=0;
	unsigned long flags;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	//    dma_buffer = 0xFFFFFF00 / ((length >> 4) << 8);
	//    dma_buffer = dma_buffer * ((length >> 4) << 8);

	bit_count = 31;
	while(bit_count > 3)
	{
		if((0x00000001 << bit_count) & length) 
			break;

		bit_count--;
	};

	if(bit_count <= 3)
	{
		alsa_dbg("Error : not valid length\n");
		return;
	}
	else
	{
		length = 0x00000001<<bit_count;
		alsa_dbg("[%s], original len[%u] period[%u]\n", __func__, length,period);
	}

	dma_buffer = 0xFFFFFF00 & (~((length<<4)-1));

	if((prtd->flag & TCC_I2S_FLAG) != 0){
		/* DAI ADMA TX Setting */
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		reg_value &= ~Hw28;	//DAI TX DMA channel disable
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		reg_value = dma_buffer | 4;
		pcm_writel(reg_value, padma_reg+ADMA_TXDAPARAM);

		// generate interrupt when half of buffer was played
		reg_value = (period >> 0x05) - 1;
		pcm_writel(reg_value, padma_reg+ADMA_TXDATCNT);

		if(chs == 1){
			alsa_dbg("[%s] addr[0x%08X] mono_dma_play,addr[0x%08X]\n", __func__,addr,prtd->mono_dma_play->addr);    
			pcm_writel(addr, padma_reg+ADMA_TXDASARL);
			pcm_writel(prtd->mono_dma_play->addr, padma_reg+ADMA_TXDASAR);

			reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
			//Hw28: TX DAM begin Current SRC/DEST Addr.
			//Hw16: DAI TX Repeat Mode Enable
			//Hw9: Busrt Size 4 cycle
			//Hw1, Hw0: Word Size 32bit 
			reg_value |= (Hw28 | Hw16 | Hw9 | Hw1 | Hw0); // MONO
			reg_value &= ~Hw8;        
			pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		}else {
			pcm_writel(addr, padma_reg+ADMA_TXDASAR);

			reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
			//Hw28: TX DAM begin Current SRC/DEST Addr.
			//Hw16: DAI TX Repeat Mode Enable
			//Hw9, Hw8: Busrt Size 8 cycle
			//Hw1, Hw0: Word Size 32bit 
			reg_value |= (Hw28 | Hw16 | Hw9 | Hw8 | Hw1 | Hw0); 
			pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		}

		spin_lock_irqsave(prtd->adma_slock, flags);
		//CHCTRL register setting start
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		if(chs == 1) {
			reg_value |= Hw16;  // MONO
		} else {
			reg_value &= ~Hw16;  
		}
		if(format == SNDRV_PCM_FORMAT_S24_LE) { // Dynamic 16/24bit support
			reg_value &= ~Hw12;  
			alsa_dbg("%s SNDRV_PCM_FORMAT_S24_LE foramt %d\n",__func__,format);
		} else {
			reg_value |= Hw12;  
			alsa_dbg("%s SNDRV_PCM_FORMAT_S16_LE foramt %d\n",__func__,format);
		}
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		//CHCTRL register setting end
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	} else if((prtd->flag & TCC_SPDIF_FLAG) != 0){
		/* SPDIF ADMA TX Setting */
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		reg_value &= ~Hw29;	//DAI TX DMA channel disable
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		reg_value = dma_buffer | 4;
		pcm_writel(reg_value, padma_reg+ADMA_TXSPPARAM);

		// generate interrupt when half of buffer was played
		reg_value = (period >> 0x05) - 1;
		pcm_writel(reg_value, padma_reg+ADMA_TXSPTCNT);

		pcm_writel(addr, padma_reg+ADMA_TXSPSAR);

		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		//Hw28: TX DAM begin Current SRC/DEST Addr.
		//Hw17: SPDIF TX Repeat Mode Enable
		//Hw11, Hw10: Busrt Size 8 cycle
		//Hw3, Hw2: Word Size 32bit 
		reg_value |= (Hw28 | Hw17 | Hw11 | Hw10 | Hw3 | Hw2); 
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);

		spin_lock_irqsave(prtd->adma_slock, flags);
		//CHCTRL register setting start
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		if(format == SNDRV_PCM_FORMAT_S24_LE){ // Dynamic 16/24bit support
			reg_value &= ~Hw13; //Width of Audio Data of SPDIF TX 24bit 
		} else{
			reg_value |= Hw13;  //Width of Audio Data of SPDIF TX 16bit 
		}
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		//CHCTRL register setting end
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	} else {
		printk("ERROR!! %s DEVICE FLAG=0x%04x.\n", __func__, prtd->flag);
	}
	alsa_dbg("[%s] device %s TX\n", __func__,
			((prtd->flag & TCC_I2S_FLAG) != 0) ? "i2s":
			((prtd->flag & TCC_SPDIF_FLAG) != 0) ? "spdif":"cdif");
	alsa_dbg("[%s] addr[0x%08X], len[%u], period[%u]\n", __func__, addr, length, period);
	alsa_dbg("[%s] HwTxDaParam [0x%X]\n", __func__, (int)(dma_buffer | 4));
	alsa_dbg("[%s] HwTxDaTCnt [%d]\n", __func__, ((period) >> 0x05) - 1);

}

static void set_dma_inbuffer(struct tcc_runtime_data *prtd, struct snd_pcm_hw_params *params)
{
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	snd_pcm_format_t format = params_format(params);
	unsigned int chs = params_channels(params);
	unsigned int length = params_buffer_bytes(params);
	unsigned int addr = prtd->dma_buffer;
	unsigned int period = min_period_size;
	unsigned int dma_buffer = 0, bit_count=0, reg_value=0;
	unsigned long flags;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	//    dma_buffer = 0xFFFFFF00 / ((length >> 4) << 8);
	//    dma_buffer = dma_buffer * ((length >> 4) << 8);

	bit_count = 31;
	while(bit_count > 3)
	{
		if((0x00000001 << bit_count) & length) 
			break;

		bit_count--;
	};

	if(bit_count <= 3)
	{
		alsa_dbg("Error : not valid length\n");
		return;
	}
	else
	{
		length = 0x00000001<<bit_count;
		alsa_dbg("[%s], original len[%u]\n", __func__, length);

	}

	dma_buffer = 0xFFFFFF00 & (~((length<<4)-1));

	if((prtd->flag & TCC_I2S_FLAG) != 0){
		/* DAI ADMA RX Setting */
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		reg_value &= ~Hw30;
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		reg_value = dma_buffer | 4;
		pcm_writel(reg_value, padma_reg+ADMA_RXDAPARAM);

		reg_value = (period >> 0x05) - 1;
		pcm_writel(reg_value, padma_reg+ADMA_RXDATCNT);

		if(chs == 1){
			alsa_dbg("[%s] addr[0x%08X] mono_dma_capture,addr[0x%08X]\n", 
					__func__, addr, prtd->mono_dma_capture->addr);   

			pcm_writel(addr, padma_reg+ADMA_RXDADARL);
			pcm_writel(prtd->mono_dma_capture->addr, padma_reg+ADMA_RXDADAR);

			reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
			//Hw29: RX DMA begin Current SRC/DEST Addr.
			//Hw18: DAI RX Repeat Mode Enable
			//Hw13: Busrt Size 4 cycle for MONO
			//Hw5, Hw4: Word Size 32bit 
			reg_value &= ~Hw12;        
			reg_value |= (Hw29 | Hw18 | Hw13 | Hw4 | Hw5);
			pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		}else {
			pcm_writel(addr, padma_reg+ADMA_RXDADAR);

			reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
			//Hw29: RX DMA begin Current SRC/DEST Addr.
			//Hw18: DAI RX Repeat Mode Enable
			//Hw13, Hw12: Busrt Size 8 cycle
			//Hw5, Hw4: Word Size 32bit 
			reg_value |= (Hw29 | Hw18 | Hw13 | Hw12 | Hw5 | Hw4); 
			pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		}

		spin_lock_irqsave(prtd->adma_slock, flags);
		//CHCTRL register setting start
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		if(chs == 1) {
			reg_value |= Hw18;  // MONO
		} else {
			reg_value &= ~Hw18; 
		}

		if(format == SNDRV_PCM_FORMAT_S24_LE){ // Dynamic 16/24bit support
			alsa_dbg("%s SNDRV_PCM_FORMAT_S24_LE foramt %d\n",__func__,format);
			reg_value &= ~Hw14; 
		}    
		else{
			alsa_dbg("%s SNDRV_PCM_FORMAT_S16_LE foramt %d\n",__func__,format);
			reg_value |= Hw14;
		}    
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		//CHCTRL register setting end
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	} else {
		/* SPDIF RX, CDIF ADMA Setting */
		reg_value = dma_buffer | 4;
		pcm_writel(reg_value, padma_reg+ADMA_RXCDPARAM);

		// generate interrupt when half of buffer was played
		reg_value = (period >> 0x05) - 1;
		pcm_writel(reg_value, padma_reg+ADMA_RXCDTCNT);

		pcm_writel(addr, padma_reg+ADMA_RXCDDAR);

		if((prtd->flag & TCC_SPDIF_FLAG) != 0){

			reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
			//Hw29: RX DMA begin Current SRC/DEST Addr.
			//Hw19: CDIF/SPDIF RX Repeat Mode Enable
			//Hw15, Hw14: Busrt Size 8 cycle
			//Hw7, Hw6: Word Size 32bit 
			reg_value |= (Hw29 | Hw19 | Hw15 | Hw14 | Hw7 | Hw6); 
			pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);

		} else {	// This is for CDIF

			reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
			//Hw29: RX DMA begin Current SRC/DEST Addr.
			//Hw19: CDIF/SPDIF RX Repeat Mode Enable
			//Hw15: Busrt Size 4 cycle for CDIF
			//Hw7, Hw6: Word Size 32bit 
			reg_value |= (Hw29 | Hw19 | Hw15 | Hw7 | Hw6); 
			reg_value &= ~(Hw14); 
			pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);

		}

		spin_lock_irqsave(prtd->adma_slock, flags);
		//CHCTRL register setting start
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		if(format == SNDRV_PCM_FORMAT_S24_LE){ // Dynamic 16/24bit support
			reg_value &= ~Hw15; //Width of Audio Data of CDIF/SPDIF RX 24bit 
		} else{
			reg_value |= Hw15;  //Width of Audio Data of CDIF/SPDIF RX 16bit 
		}
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		//CHCTRL register setting end
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	}

	alsa_dbg("[%s] device %s RX\n", __func__,
			((prtd->flag & TCC_I2S_FLAG) != 0) ? "i2s":
			((prtd->flag & TCC_SPDIF_FLAG) != 0) ? "spdif":"cdif");
	alsa_dbg("[%s] addr[0x%08X], len[%d]\n", __func__, addr, length);
	alsa_dbg("[%s] HwRxDaParam [0x%X]\n", __func__, (int)dma_buffer | 4);
	alsa_dbg("[%s] HwRxDaTCnt [%d]\n", __func__, ((period) >> 0x05) - 1);

}

static irqreturn_t tcc_dma_done_handler(int irq, void *dev_id)
{
	struct tcc_runtime_data *prtd = (struct tcc_runtime_data *) dev_id;
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int flag = prtd->ptcc_intr->flag;
	unsigned int reg_read=0, reg_write=0, dmaInterruptSource = 0;

	reg_read = pcm_readl(padma_reg+ADMA_INTSTATUS);

	if(prtd->flag & TCC_I2S_FLAG) {
		if (reg_read & (Hw4 | Hw0)) {
			dmaInterruptSource |= DMA_I2S_OUT;
			reg_write |= Hw4 | Hw0;
		}
		//reg_read = pcm_readl(padma_reg+ADMA_INTSTATUS);
		if (reg_read & (Hw6 | Hw2)) {
			dmaInterruptSource |= DMA_I2S_IN;
			reg_write |= Hw6 | Hw2;
		}
	} else {	//For SPDIF, CDIF
		//reg_read = pcm_readl(padma_reg+ADMA_INTSTATUS);
		if (reg_read & (Hw5 | Hw1)) {
			dmaInterruptSource |= DMA_SPDIF_OUT;
			reg_write |= Hw5 | Hw1;
		}

		// Planet 20150812 S/PDIF_Rx
		//reg_read = pcm_readl(padma_reg+ADMA_INTSTATUS);
		if (reg_read & (Hw7 | Hw3)) {
			dmaInterruptSource |= DMA_CDIF_SPDIF_IN;
			reg_write |= Hw7 | Hw3;
		}
	}
	if (reg_write) { 
		//reg_read = pcm_readl(padma_reg+ADMA_INTSTATUS);
		reg_read |= reg_write;
		pcm_writel(reg_read, padma_reg+ADMA_INTSTATUS);
	}

	if (((dmaInterruptSource & DMA_I2S_OUT) && (flag & TCC_RUNNING_PLAY))
		|| ((dmaInterruptSource & DMA_SPDIF_OUT) && (flag & TCC_RUNNING_SPDIF_PLAY))) {
		prtd->ptcc_intr->nOut ++;
		if(prtd->ptcc_intr->nOut >= prtd->nperiod){
			snd_pcm_period_elapsed(prtd->ptcc_intr->play_ptr);
			prtd->ptcc_intr->nOut=0;
		}
	}
	
#if defined(RX_FIFO_CLEAR_IN_DMA_BUF)
	if ((dmaInterruptSource & DMA_I2S_IN) && (flag & TCC_RUNNING_CAPTURE) && (prtd->to_be_cleared_rx_period_cnt > 0)) {		
		memset(prtd->ptcc_intr->cap_ptr->dma_buffer.area + (min_period_size * prtd->ptcc_intr->nIn), 0, min_period_size); //clear period_size	
		prtd->to_be_cleared_rx_period_cnt--;
	}
#endif
	
	if (((dmaInterruptSource & DMA_I2S_IN) && (flag & TCC_RUNNING_CAPTURE))
		|| ((dmaInterruptSource & DMA_CDIF_SPDIF_IN) && (flag & TCC_RUNNING_CDIF_SPDIF_CAPTURE))) {
		prtd->ptcc_intr->nIn ++;
		if(prtd->ptcc_intr->nIn >= prtd->nperiod){
			snd_pcm_period_elapsed(prtd->ptcc_intr->cap_ptr);
			prtd->ptcc_intr->nIn=0;
		}
	}

	return IRQ_HANDLED;
}

static int tcc_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	//	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;	// dai base addr

	size_t totsize = params_buffer_bytes(params);
	size_t period_bytes = params_period_bytes(params);
	int chs = params_channels(params);
	unsigned int reg_value=0;
	unsigned long flags;
	int ret = 0, mod=0, ttemp=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	if (!(prtd->ptcc_intr->flag & TCC_INTERRUPT_REQUESTED)) {

		ret = request_irq(prtd->adma_irq,
				tcc_dma_done_handler,
				IRQF_SHARED,
				"alsa-audio",
				prtd);
		if (ret < 0) {
			return -EBUSY;
		}
		prtd->ptcc_intr->flag |= TCC_INTERRUPT_REQUESTED;

	}


	//DMA Buffer Control
	//DMA transfer buffer pointer initialization
	//DMA size should be 2^n.
	mod = totsize/min_period_size;
	ttemp = 1;	
	while(1){
		if(mod == 1) break;
		else{
			if((ttemp*2) > mod)break;
			else ttemp = ttemp*2;
		}
	}
	if(mod != ttemp){
		printk("== alsa-debug == Warning!! buffer_size should be 2^n.\n Your buffer[%d bytes] set [%d bytes] in TCC.\n", totsize, ttemp*min_period_size);
	}
	//totsize = ttemp * min_period_size; //this can occured mis-match buffer size when mmap mode.

	mod = period_bytes%min_period_size;
	ttemp = period_bytes/min_period_size;
	if(mod > 0) ttemp = ttemp+1;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);        
	runtime->dma_bytes = totsize;

	prtd->dma_buffer_end = runtime->dma_addr + totsize;
	prtd->dma_buffer = runtime->dma_addr;
	prtd->period_size = period_bytes;
	prtd->nperiod = ttemp;

	alsa_dbg("buffer bytes=0x%X period bytes=0x%X, rate=%d, chs=%d\n",
			totsize, period_bytes, params_rate(params), params_channels(params));
	alsa_dbg("~~~!!! Substream->pcm->device = %d\n",substream->pcm->device);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		set_dma_outbuffer(prtd, params);
		//period counter init.
		if(prtd->flag & TCC_I2S_FLAG) {
			prtd->ptcc_intr->nOut=0;
		}
	} else {
		set_dma_inbuffer(prtd, params);
		//period counter init.
		if(prtd->flag & TCC_I2S_FLAG) {
			prtd->ptcc_intr->nIn=0;
		}
	}

	if(prtd->flag & TCC_I2S_FLAG){
		prtd->flag |= (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? DMA_I2S_OUT : DMA_I2S_IN;
		if (chs > 2) { // 7.1ch
			if(prtd->id < 3){
				reg_value = pcm_readl(padma_reg+ADMA_RPTCTRL);
				//Hw27~24: DAI Buffer Threshold. 0x7 should be set.
				reg_value |= (Hw26 | Hw25 | Hw24);
				pcm_writel(reg_value, padma_reg+ADMA_RPTCTRL);

				spin_lock_irqsave(prtd->adma_slock, flags);
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					//Hw24: DAI TX DMA Multi-channel mode Enable
					//Hw21, Hw20: b'11 7.1channel.
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					reg_value |= (Hw24 | Hw21 | Hw20);
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
				}else{
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					//Hw25: DAI RX DMA Multi-channel mode Enable
					//Hw23, Hw22: b'11 7.1channel.
					reg_value |= (Hw25 | Hw23 | Hw22);
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
				}
				spin_unlock_irqrestore(prtd->adma_slock, flags);
			}else{
				printk("cannot support %d channels \n", chs);
				return -1;
			}
		}else { // 2 ch
			reg_value = pcm_readl(padma_reg+ADMA_RPTCTRL);
			//Hw27~24: DAI Buffer Threshold. 0x7 should be set.
			reg_value |= (Hw26 | Hw25 | Hw24);
			pcm_writel(reg_value, padma_reg+ADMA_RPTCTRL);

			spin_lock_irqsave(prtd->adma_slock, flags);
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				//Hw24: DAI TX DMA Multi-channel mode Disable
				//Hw21, Hw20: b'11 7.1channel Disable.
				reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
				reg_value &= ~(Hw24 | Hw21 | Hw20);
				pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
			}else{
				//Hw25: DAI RX DMA Multi-channel mode Disable
				//Hw23, Hw22: b'11 7.1channel Disable.
				reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
				reg_value &= ~(Hw25 | Hw23 | Hw22);
				pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
			}
			spin_unlock_irqrestore(prtd->adma_slock, flags);
		}
	} else {	//This is for SPDIF and CDIF
		prtd->flag |= (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? DMA_SPDIF_OUT : DMA_CDIF_SPDIF_IN;
	}

	return ret;
}

static int tcc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int bStop=0, ret=0;
	unsigned long flags;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	/*This is to stop ADMA*/
	spin_lock_irqsave(prtd->adma_slock, flags);
	bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
	spin_unlock_irqrestore(prtd->adma_slock, flags);
	if(prtd->flag & TCC_I2S_FLAG){
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			bStop = bStop & Hw28;
			if(bStop){ 
				ret = tcc_adma_i2s_tx_enable(substream, 0);
				if(ret) alsa_dbg("%s() I2S playback stop has some error.\n", __func__);
				else alsa_dbg("%s() I2S playback end complete.\n", __func__);
			}
		} else {
			bStop = bStop & Hw30;
			if(bStop){    
				ret = tcc_adma_i2s_rx_enable(substream, 0);
				if(ret) alsa_dbg("%s() I2S capture stop has some error.\n", __func__);
				else alsa_dbg("%s() I2S capture end complete.\n", __func__);
			}
		}
	}else{	//SPDIF or CDIF
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			bStop = bStop & Hw29;
			if(bStop){ 
				ret = tcc_adma_spdif_tx_enable(substream, 0);
				if(ret) alsa_dbg("%s() SPDIF playback stop has some error.\n", __func__);
				else alsa_dbg("%s() SPDIF playback end complete.\n", __func__);
			}
		} else {
			bStop = bStop & Hw31;
			if(bStop){    
				ret = tcc_adma_sp_cdif_rx_enable(substream, 0);
				if(ret) alsa_dbg("%s() SP/CDIF capture stop has some error.\n", __func__);
				else alsa_dbg("%s() SP/CDIF capture end complete.\n", __func__);
			}
		}
	}
	/*This is to stop ADMA*/

	if (prtd->flag & DMA_FLAG) {
		snd_pcm_set_runtime_buffer(substream, NULL);
	}

	return 0;
}

static int tcc_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int bStop=0, ret=0;
	unsigned long flags;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	/*This is to stop ADMA*/
	spin_lock_irqsave(prtd->adma_slock, flags);
	bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
	spin_unlock_irqrestore(prtd->adma_slock, flags);
	if(prtd->flag & TCC_I2S_FLAG){
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			bStop = bStop & Hw28;
			if(bStop){ 
				alsa_dbg("%s() I2S TX ADMA doesn't stop yet. So, TX ADMA stop start.\n", __func__);
				ret = tcc_adma_i2s_tx_enable(substream, 0);
				if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
				else alsa_dbg("%s() playback end complete.\n", __func__);
			}
		} else {
			bStop = bStop & Hw30;
			if(bStop){    
				alsa_dbg("%s() I2S RX ADMA doesn't stop yet. So, RX ADMA stop start.\n", __func__);
				ret = tcc_adma_i2s_rx_enable(substream, 0);
				if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
				else alsa_dbg("%s() capture end complete.\n", __func__);
			}
		}
	}else{	//SPDIF or CDIF
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			bStop = bStop & Hw29;
			if(bStop){ 
				alsa_dbg("%s() SPDIF TX ADMA doesn't stop yet. So, TX ADMA stop start.\n", __func__);
				ret = tcc_adma_spdif_tx_enable(substream, 0);
				if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
				else alsa_dbg("%s() playback end complete.\n", __func__);
			}
		} else {
			bStop = bStop & Hw31;
			if(bStop){    
				alsa_dbg("%s() SP/CDIF RX ADMA doesn't stop yet. So, RX ADMA stop start.\n", __func__);
				ret = tcc_adma_sp_cdif_rx_enable(substream, 0);
				if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
				else alsa_dbg("%s() capture end complete.\n", __func__);
			}
		}
	}
	/*This is to stop ADMA*/

	//DMA initialize
	memset(runtime->dma_area, 0x00, runtime->dma_bytes);
	alsa_dbg(" %s %d dma_bytes -> 0 set Done\n",__func__,runtime->dma_bytes);
	return ret;
}

#if defined(RX_FIFO_CLEAR_BY_DELAY_AND_MUTE)
static void tcc_dai_mute(struct device *dev, bool enable)
{	
	struct pinctrl *pin;
	if(enable) {
		pin = devm_pinctrl_get_select(dev, "dai_mute");
	} else {
		pin = devm_pinctrl_get_select(dev, "default");
	}
	if(pin == NULL) {
		printk("%s() has some wrong.\n", __func__);
	}
}
#endif

#if 0
static int first_init_delay = 0;
static int tcc_i2s_tx_enable(struct snd_pcm_substream *substream, int En)
{
	volatile PADMADAI pDAI  = (volatile PADMADAI)tcc_p2v(BASE_ADDR_DAI0);
	volatile PADMA    pADMA = (volatile PADMA)tcc_p2v(BASE_ADDR_ADMA0);
	unsigned int ret=1;

	if(En)
	{
		BITSET(pADMA->ChCtrl, Hw0);
		BITSET(pADMA->ChCtrl, Hw28);
		pDAI->DAMR |= Hw14;

		if(first_init_delay){
			mdelay(1);
		}
		else{
			first_init_delay = 1;
			mdelay(5);
		}
		pDAI->DAVC = 0; // Mute OFF

		ret = 0;
	}
	else
	{
		pDAI->DAMR &= ~Hw14;
		mdelay(1);
		BITCLR(pADMA->ChCtrl, Hw28);
		BITCLR(pADMA->ChCtrl, Hw0);

		ret = 0;
	}

	return ret;
}
#else
static int tcc_adma_i2s_tx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;	// dai base addr
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int tTxDatCnt=0, tChCtrl=0;	//temp value
	unsigned int reg_value=0, timeout=0;
	unsigned long flags;
	int ret=0;
	//int i; //for Debug

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	if(En) {
		/*	// for debug 
			for(i=0; i<32; i++) {
			reg_value = pcm_readl(pdai_reg+I2S_DADOR0);
			printk("[%s]DADORO=0x%08x\n", __func__, reg_value);
			}
			*/
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		tChCtrl = reg_value & Hw28;
		if(tChCtrl){
			alsa_dbg("[%s] I2S TX ADMA doesn't stop yet!!\n", __func__);
			ret = 1;
		}
		//Hw0: DAI DMA TX Intr enable
		//Hw28: DAI DMA TX enable 
		reg_value |= (Hw0 | Hw28);
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
	} else {
		/* for FIFO clear */
		memset(runtime->dma_area, 0x00, runtime->dma_bytes);
		mdelay(2);

		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value &= ~(Hw16);	/* ADMA Repeate mode off */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		/* REMOVE Gabage PCM data of ADMA */
		while(1){
			/* Get remained garbage PCM */
			tTxDatCnt = (pcm_readl(padma_reg+ADMA_TXDATCNT) & 0xFFFF0000) >> 16;
			if(tTxDatCnt == 0){
				timeout=0;
				while(1){
					spin_lock_irqsave(prtd->adma_slock, flags);
					tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL) & Hw28;
					spin_unlock_irqrestore(prtd->adma_slock, flags);
					if(tChCtrl == 0){
						alsa_dbg("ADMA TX stoped!\n");
						break;
					}

					mdelay(1);
					timeout++;

					if(timeout >= 300){
						tTxDatCnt = pcm_readl(padma_reg+ADMA_TXDATCNT);
						spin_lock_irqsave(prtd->adma_slock, flags);
						tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL);
						printk("[%s] ADMA STOP TxDaTCnt[0x%08x]ChCtrl[0x%08x] timeout return\n", 
								__func__, tTxDatCnt, tChCtrl);
						tChCtrl &= ~(Hw28); /* STOP ADMA */
						pcm_writel(tChCtrl, padma_reg+ADMA_CHCTRL);
						spin_unlock_irqrestore(prtd->adma_slock, flags);
						break;
					}
				}

				spin_lock_irqsave(prtd->adma_slock, flags);
				reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
				reg_value &= ~(Hw0); /* STOP ADMA intr*/
				pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
				spin_unlock_irqrestore(prtd->adma_slock, flags);

				reg_value = pcm_readl(pdai_reg+I2S_DAMR);
				reg_value &= ~Hw14;	//DAI TX Disable
				pcm_writel(reg_value, pdai_reg+I2S_DAMR);

				break;
			}else{
				mdelay(1);
				timeout++;

				if(timeout >= 300){
					tTxDatCnt = pcm_readl(padma_reg+ADMA_TXDATCNT);
					printk("[%s] TxDaTCnt [0x%08x] timeout return\n", __func__,
							(tTxDatCnt & 0xffff0000) >> 16);

					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					reg_value &= ~(Hw0 | Hw28); /* STOP ADMA intr*/
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);

					reg_value = pcm_readl(pdai_reg+I2S_DAMR);
					reg_value &= ~Hw14;	//DAI TX Disable
					pcm_writel(reg_value, pdai_reg+I2S_DAMR);

					ret = 1;
					break;
				}
			}
		}
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value	|= Hw16;	/* ADMA Repeate mode enable */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
	}
	return ret;
}
#endif
static int tcc_adma_spdif_tx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;	// dai base addr
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int tTxSptCnt=0, tChCtrl=0;	//temp value
	unsigned int reg_value=0, timeout=0;
	unsigned long flags;
	int ret=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	if(En) {
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		tChCtrl = reg_value & Hw29;
		if(tChCtrl){
			alsa_dbg("[%s] SPDIF TX ADMA doesn't stop yet!!\n", __func__);
			ret = 1;
		}
		//Hw1: SPDIF DMA TX Intr enable
		//Hw29: SPDIF DMA TX enable 
		reg_value |= (Hw1| Hw29);
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
	} else {
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value &= ~(Hw17);	/* ADMA Repeate mode off */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		/* REMOVE Gabage PCM data of ADMA */
		while(1){
			/* Get remained garbage PCM */
			tTxSptCnt = (pcm_readl(padma_reg+ADMA_TXSPTCNT) & 0xFFFF0000) >> 16;
			if(tTxSptCnt == 0){
				timeout=0;
				while(1){
					spin_lock_irqsave(prtd->adma_slock, flags);
					tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL) & Hw29;
					spin_unlock_irqrestore(prtd->adma_slock, flags);
					if(tChCtrl == 0){
						alsa_dbg("ADMA TX stoped!\n");
						break;
					}

					mdelay(1);
					timeout++;

					if(timeout >= 300){
						tTxSptCnt = pcm_readl(padma_reg+ADMA_TXSPTCNT);
						spin_lock_irqsave(prtd->adma_slock, flags);
						tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL);
						printk("[%s] ADMA STOP TxSpTCnt[0x%08x]ChCtrl[0x%08x] timeout return\n", 
								__func__, tTxSptCnt, tChCtrl);
						tChCtrl &= ~(Hw28); /* STOP ADMA */
						pcm_writel(tChCtrl, padma_reg+ADMA_CHCTRL);
						spin_unlock_irqrestore(prtd->adma_slock, flags);
						break;
					}
				}

				spin_lock_irqsave(prtd->adma_slock, flags);
				reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
				reg_value &= ~(Hw1); /* STOP ADMA intr*/
				pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
				spin_unlock_irqrestore(prtd->adma_slock, flags);

				reg_value = pcm_readl(pdai_reg+SPDIF_TXCONFIG);
				//Hw2: Tx Intr Enable
				//Hw1: Data Valid bit
				//Hw0: Tx Enable
				reg_value &= ~(Hw2 | Hw1 | Hw0);	//SPDIF TX Disable
				pcm_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);

				break;
			}else{
				mdelay(1);
				timeout++;

				if(timeout >= 300){
					tTxSptCnt = pcm_readl(padma_reg+ADMA_TXSPTCNT);
					printk("[%s] TxSpTCnt [0x%08x] timeout return\n", __func__,
							(tTxSptCnt & 0xffff0000) >> 16);

					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					reg_value &= ~(Hw1 | Hw29); /* STOP ADMA intr*/
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);

					reg_value = pcm_readl(pdai_reg+SPDIF_TXCONFIG);
					//Hw2: Tx Intr Enable
					//Hw1: Data Valid bit
					//Hw0: Tx Enable
					reg_value &= ~(Hw2 | Hw1 | Hw0);	//SPDIF TX Disable
					pcm_writel(reg_value, pdai_reg+SPDIF_TXCONFIG);

					ret = 1;
					break;
				}
			}
		}
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value	|= Hw17;	/* ADMA Repeate mode enable */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
	}
	return ret;
}

#if 0
static int tcc_i2s_rx_enable(struct snd_pcm_substream *substream, int En)
{
	volatile PADMADAI pDAI  = (volatile PADMADAI)tcc_p2v(BASE_ADDR_DAI0);
	volatile PADMA    pADMA = (volatile PADMA)tcc_p2v(BASE_ADDR_ADMA0);
	unsigned int ret=1;

	if(En)
	{
		BITSET(pADMA->ChCtrl, Hw2);
		BITSET(pADMA->ChCtrl, Hw30);

		pDAI->DAMR |= Hw13;

		ret = 0;
	}
	else
	{
		pDAI->DAMR &= ~Hw13;
		BITCLR(pADMA->ChCtrl, Hw30);
		BITCLR(pADMA->ChCtrl, Hw2);

		ret = 0;
	}

	return ret;
}
#else
static int tcc_adma_i2s_rx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;	// dai base addr
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int tRxDatCnt=0, tChCtrl=0;	//temp value
	unsigned int reg_value=0, timeout=0;
	unsigned long flags;
	int ret=0;
	//int i; //for Debug

	if(En) {
		/* for Debug
		   for(i=0; i<32; i++) {
		   reg_value = pcm_readl(pdai_reg+I2S_DADIR0);
		   printk("[%s]DADIRO=0x%08x\n", __func__, reg_value);
		   }
		   */
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		tChCtrl = reg_value & Hw30;
		if(tChCtrl){
			alsa_dbg("[%s] I2S RX ADMA doesn't stop yet!!\n", __func__);
			ret = 1;
		}
		//Hw2: DAI DMA RX Intr enable
		//Hw30: DAI DMA RX enable
		reg_value |= (Hw2 | Hw30);
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	} else {
#if defined(RX_FIFO_CLEAR_BY_DELAY_AND_MUTE)
		/* For FIFO clear */
		//DAI mute enable
		tcc_dai_mute(dai->dev, 1);
		mdelay(2);
#endif

		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value &= ~(Hw18);	/* ADMA Repeate mode off */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		while(1){
			/* Get remained garbage PCM */
			tRxDatCnt = (pcm_readl(padma_reg+ADMA_RXDATCNT) & 0xFFFF0000) >> 16;
			if(tRxDatCnt == 0) {
				timeout=0;
				while(1){
					spin_lock_irqsave(prtd->adma_slock, flags);
					tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL) & Hw30;
					spin_unlock_irqrestore(prtd->adma_slock, flags);
					if(tChCtrl == 0){
						alsa_dbg("ADMA RX stoped!\n");
						break;
					}

					mdelay(1);
					timeout++;

					if(timeout >= 300) {

						tRxDatCnt = pcm_readl(padma_reg+ADMA_RXDATCNT);
						spin_lock_irqsave(prtd->adma_slock, flags);
						tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL);
						printk("[%s] ADMA STOP RxDaTCnt[0x%08x]ChCtrl[0x%08x] timeout return\n", 
								__func__, tRxDatCnt, tChCtrl);
						tChCtrl &= ~(Hw30); /* STOP ADMA */
						pcm_writel(tChCtrl, padma_reg+ADMA_CHCTRL);
						spin_unlock_irqrestore(prtd->adma_slock, flags);

						ret = 1;
						break;

					}
				}

				spin_lock_irqsave(prtd->adma_slock, flags);
				reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
				reg_value &= ~(Hw2); /* STOP ADMA intr*/
				pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
				spin_unlock_irqrestore(prtd->adma_slock, flags);

				reg_value  = pcm_readl(pdai_reg+I2S_DAMR);
				//reg_value &= ~Hw17;	//This is loop-back disable. Direction: TX->RX
				reg_value &= ~Hw13;	//DAI RX Disable
				pcm_writel(reg_value, pdai_reg+I2S_DAMR);

				break;

			}else{
				mdelay(1);
				timeout++;
				if(timeout >= 300){

					tRxDatCnt = pcm_readl(padma_reg+ADMA_RXDATCNT);
					printk("[%s] RxDaTCnt [0x%X] timeout return\n", __func__,
							(tRxDatCnt & 0xffff0000) >> 16);

					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					reg_value &= ~(Hw2 | Hw30); /* STOP ADMA */
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);

					reg_value = pcm_readl(pdai_reg+I2S_DAMR);
					//reg_value &= ~Hw17;	//This is loop-back disable. Direction: TX->RX
					reg_value &= ~Hw13;	//DAI RX Disable
					pcm_writel(reg_value, pdai_reg+I2S_DAMR);

					ret = 1;
					break;
				}
			}
		}

		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value |= Hw18;	/* ADMA Repeate mode enable */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

#if defined(RX_FIFO_CLEAR_BY_DELAY_AND_MUTE)
		//DAI mute disable
		tcc_dai_mute(dai->dev, 0);
#endif

	}
	
	return ret;
}
#endif 
static int tcc_adma_sp_cdif_rx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;	// dai base addr
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int tRxCdtCnt=0, tChCtrl=0;	//temp value
	unsigned int reg_value=0, timeout=0;
	unsigned long flags;
	int ret=0;

	if(En) {
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
		tChCtrl = reg_value & Hw31;
		if(tChCtrl){
			alsa_dbg("[%s] SP/CDIF RX ADMA doesn't stop yet!!\n", __func__);
			ret = 1;
		}
		//Hw31: CDIF/SPDIF RX ADMA enable
		//Hw26: CDIF/SPDIF RX Select
		//Hw3: CDIF/SPDIF RX INTR enable
		reg_value |= Hw31 | Hw26 | Hw3;
		//CDIF Select
		if(prtd->flag & TCC_CDIF_FLAG) reg_value &= ~Hw26;
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	} else {
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value &= ~(Hw19);	/* ADMA Repeate mode off */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		while(1){
			/* Get remained garbage PCM */
			tRxCdtCnt = (pcm_readl(padma_reg+ADMA_RXCDTCNT) & 0xFFFF0000) >> 16;
			if(tRxCdtCnt == 0) {
				timeout=0;
				while(1){
					spin_lock_irqsave(prtd->adma_slock, flags);
					tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL) & Hw31;
					spin_unlock_irqrestore(prtd->adma_slock, flags);
					if(tChCtrl == 0){
						alsa_dbg("ADMA RX stoped!\n");
						break;
					}

					mdelay(1);
					timeout++;

					if(timeout >= 300) {
						tRxCdtCnt = pcm_readl(padma_reg+ADMA_RXCDTCNT);
						spin_lock_irqsave(prtd->adma_slock, flags);
						tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL);
						printk("[%s] ADMA STOP RxCdTCnt[0x%08x]ChCtrl[0x%08x] timeout return\n", 
								__func__, tRxCdtCnt, tChCtrl);
						tChCtrl &= ~(Hw31); /* STOP ADMA */
						pcm_writel(tChCtrl, padma_reg+ADMA_CHCTRL);
						spin_unlock_irqrestore(prtd->adma_slock, flags);

						ret = 1;
						break;

					}
				}

				spin_lock_irqsave(prtd->adma_slock, flags);
				reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
				reg_value &= ~(Hw26 | Hw3); /* STOP ADMA intr*/
				pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
				spin_unlock_irqrestore(prtd->adma_slock, flags);

				if(prtd->flag & TCC_CDIF_FLAG){
					reg_value = pcm_readl(pdai_reg-CDIF_OFFSET+SPDIF_RXCONFIG_OFFSET);
					//Hw0: SPDIF Rx Enable
					reg_value &= ~Hw0;
					pcm_writel(reg_value, pdai_reg-CDIF_OFFSET+SPDIF_RXCONFIG_OFFSET);
					reg_value = pcm_readl(pdai_reg+CDIF_CICR);
					//Hw7: CDIF Enable
					reg_value &= ~Hw7;
					pcm_writel(reg_value, pdai_reg+CDIF_CICR);
				}else{	//SPDIF RX
					reg_value = pcm_readl(pdai_reg+SPDIF_RXCONFIG);
					//Hw4: Sample Data Store
					//Hw3: RxStatus Register Holds Channel
					//Hw2: Rx Intr Enable
					//Hw1: Store Data
					//Hw0: Rx Enable
					reg_value &= ~(Hw4 | Hw3 | Hw2 | Hw1 | Hw0);
					pcm_writel(reg_value, pdai_reg+SPDIF_RXCONFIG);
				}

				break;

			}else{
				mdelay(1);
				timeout++;
				if(timeout >= 300){
					tRxCdtCnt = pcm_readl(padma_reg+ADMA_RXCDTCNT);
					printk("[%s] RxCdTCnt [0x%X] timeout return\n", __func__,
							(tRxCdtCnt & 0xffff0000) >> 16);

					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					reg_value &= ~(Hw31 | Hw26 | Hw3); /* STOP ADMA */
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);

					if(prtd->flag & TCC_CDIF_FLAG){
						reg_value = pcm_readl(pdai_reg-CDIF_OFFSET+SPDIF_RXCONFIG_OFFSET);
						//Hw0: SPDIF Rx Enable
						reg_value &= ~Hw0;
						pcm_writel(reg_value, pdai_reg-CDIF_OFFSET+SPDIF_RXCONFIG_OFFSET);
						reg_value = pcm_readl(pdai_reg+CDIF_CICR);
						//Hw7: CDIF Enable
						reg_value &= ~Hw7;
						pcm_writel(reg_value, pdai_reg+CDIF_CICR);
					}else{	//SPDIF RX
						reg_value = pcm_readl(pdai_reg+SPDIF_RXCONFIG);
						//Hw4: Sample Data Store
						//Hw3: RxStatus Register Holds Channel
						//Hw2: Rx Intr Enable
						//Hw1: Store Data
						//Hw0: Rx Enable
						reg_value &= ~(Hw4 | Hw3 | Hw2 | Hw1 | Hw0);
						pcm_writel(reg_value, pdai_reg+SPDIF_RXCONFIG);
					}

					ret = 1;
					break;
				}
			}
		}

		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
		reg_value |= Hw19;	/* ADMA Repeate mode enable */
		pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
	}
	return ret;
}

static int tcc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	int ret = 0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	alsa_dbg("cmd[%d] frame_bits[%d]\n", cmd, substream->runtime->frame_bits);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:        
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if(prtd->flag & TCC_I2S_FLAG){
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() playback start\n", __func__);

					ret = tcc_adma_i2s_tx_enable(substream, 1);
					if(ret) alsa_dbg("%s() playback start has some error.\n", __func__);
					else alsa_dbg("%s() playback start\n", __func__);

				} else {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() recording start\n", __func__);

					ret = tcc_adma_i2s_rx_enable(substream, 1);
					if(ret) alsa_dbg("%s() capture start has some error.\n", __func__);
					else alsa_dbg("%s() capture start\n", __func__);
				}
			}else{	//For SPDIF and CDIF
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() playback start\n", __func__);

					ret = tcc_adma_spdif_tx_enable(substream, 1);
					if(ret) alsa_dbg("%s() playback start has some error.\n", __func__);
					else alsa_dbg("%s() playback start\n", __func__);

				} else {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() recording start\n", __func__);

					ret = tcc_adma_sp_cdif_rx_enable(substream, 1);
					if(ret) alsa_dbg("%s() capture start has some error.\n", __func__);
					else alsa_dbg("%s() capture start\n", __func__);
				}
			}
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				alsa_adma_dbg(prtd->flag, prtd->id, "%s() playback end\n", __func__);

				memset(runtime->dma_area, 0x00, runtime->dma_bytes);
			} else {
				alsa_adma_dbg(prtd->flag, prtd->id, "%s() capture end\n", __func__);
#if defined(RX_FIFO_CLEAR_IN_DMA_BUF)
				prtd->to_be_cleared_rx_period_cnt = ((RX_FIFO_SIZE-1)/min_period_size) + 1;
#endif
				/* should works more? */
			}
			break;

		default:
			ret = -EINVAL;
	}

	return ret;
}


//buffer point update
//current position   range 0-(buffer_size-1)
static snd_pcm_uframes_t tcc_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;
	snd_pcm_uframes_t x;
	dma_addr_t ptr = 0;
	int chs = runtime->channels;

	//alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	if(prtd->flag & TCC_I2S_FLAG){
		if(chs == 1){
			ptr = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? pcm_readl(padma_reg+ADMA_TXDACSARL) : pcm_readl(padma_reg+ADMA_RXDACDARL); // MONO
		}else{
			ptr = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? pcm_readl(padma_reg+ADMA_TXDACSAR) : pcm_readl(padma_reg+ADMA_RXDACDAR);
		}

	} else {	//For SPDIF, CDIF
		ptr = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? pcm_readl(padma_reg+ADMA_TXSPCSAR) : pcm_readl(padma_reg+ADMA_RXCDCDAR);
	}

	x = bytes_to_frames(runtime, ptr - runtime->dma_addr);

	if (x < runtime->buffer_size) {
		return x;
	}
	return 0;
}

static int tcc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	int ret=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]open pcm device, %s\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "output" : "input"));

	//Buffer size should be 2^n.
	snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES);
	//32 means 8Burst_DMA_transfer*32Bits_align
	snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	//snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES)

	//Initialize variable about interrupt status
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {  
		if(prtd->flag & TCC_I2S_FLAG) {
			prtd->ptcc_intr->flag |= TCC_RUNNING_PLAY;
		} else {
			prtd->ptcc_intr->flag |= TCC_RUNNING_SPDIF_PLAY;
		}
		prtd->ptcc_intr->play_ptr = substream;
		snd_soc_set_runtime_hwparams(substream, &tcc_pcm_hardware_play);
	} else {
		if(prtd->flag & TCC_I2S_FLAG) {
			prtd->ptcc_intr->flag |= TCC_RUNNING_CAPTURE;
		} else { //For SPDIF and CDIF
			prtd->ptcc_intr->flag |= TCC_RUNNING_CDIF_SPDIF_CAPTURE;
		}
		prtd->ptcc_intr->cap_ptr = substream;
		snd_soc_set_runtime_hwparams(substream, &tcc_pcm_hardware_capture);

#if defined(RX_FIFO_CLEAR_IN_DMA_BUF)
		prtd->to_be_cleared_rx_period_cnt = ((RX_FIFO_SIZE-1)/min_period_size) + 1;
#endif
	}

	return ret;
}

static int tcc_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *padma_reg;
	unsigned int bStop=0, ret=0;
	unsigned long flags;

	if (prtd) {
		alsa_adma_dbg(prtd->flag, prtd->id, "[%s]close pcm device, %s\n", __func__,
				substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "output" : "input");
		padma_reg = prtd->ptcc_reg->adma_reg;

		spin_lock_irqsave(prtd->adma_slock, flags);
		bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
		if(prtd->flag & TCC_I2S_FLAG){
			/* AUDIO DMA I2S TX/RX */
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_PLAY;
				bStop = bStop & Hw28;

				if(bStop){ 
					alsa_dbg("%s() DAI TX ADMA stop start.\n", __func__);
					ret = tcc_adma_i2s_tx_enable(substream, 0);

					if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
					else alsa_dbg("%s() playback end complete.\n", __func__);
				}

				alsa_dbg("[%s] close i2s Playback device\n", __func__);
			} else {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_CAPTURE;
				bStop = bStop & Hw30;

				if(bStop){    
					alsa_dbg("%s() DAI RX ADMA stop start.\n", __func__);
					ret = tcc_adma_i2s_rx_enable(substream, 0);

					if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
					else alsa_dbg("%s() capture end complete.\n", __func__);
				}

				alsa_dbg("[%s] close i2s Capture device\n", __func__);
			}

		} else {	//for SPDIF and CDIF
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_SPDIF_PLAY;
				bStop = bStop & Hw29;

				if(bStop){ 
					alsa_dbg("%s() SPDIF TX ADMA stop start.\n", __func__);
					ret = tcc_adma_spdif_tx_enable(substream, 0);

					if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
					else alsa_dbg("%s() playback end complete.\n", __func__);
				}

				alsa_dbg("[%s] close spdif Playback device\n", __func__);
			} else {	//SPDIF RX, CDIF
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_CDIF_SPDIF_CAPTURE;
				bStop = bStop & Hw31;

				if(bStop){    
					alsa_dbg("%s() SP/CDIF RX ADMA stop start.\n", __func__);
					ret = tcc_adma_sp_cdif_rx_enable(substream, 0);

					if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
					else alsa_dbg("%s() capture end complete.\n", __func__);
				}

				alsa_dbg("[%s] close %s Capture device\n", __func__, (prtd->flag & TCC_CDIF_FLAG)? "cdif":"spdif");
			}
		}

		if ((prtd->ptcc_intr)&&(!(prtd->ptcc_intr->flag & TCC_RUNNING_FLAG))
			&&(prtd->ptcc_intr->flag & TCC_INTERRUPT_REQUESTED)) {
			free_irq(prtd->adma_irq, prtd);
			prtd->ptcc_intr->flag &= ~TCC_INTERRUPT_REQUESTED;
		}
	}


	return 0;
}

static int tcc_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
			runtime->dma_area,
			runtime->dma_addr,
			runtime->dma_bytes);
}

static struct snd_pcm_ops tcc_pcm_ops = {
	.open  = tcc_pcm_open,
	.close  = tcc_pcm_close,
	.ioctl  = snd_pcm_lib_ioctl,
	.hw_params = tcc_pcm_hw_params,
	.hw_free = tcc_pcm_hw_free,
	.prepare = tcc_pcm_prepare,
	.trigger = tcc_pcm_trigger,
	.pointer = tcc_pcm_pointer,
	.mmap = tcc_pcm_mmap,
};

static int tcc_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream 	*substream = pcm->streams[stream].substream;
	struct snd_dma_buffer 		*buf = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	size_t size = 0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		size = tcc_pcm_hardware_play.buffer_bytes_max;

		if (dai->driver->playback.channels_min == 1) {
			prtd->mono_dma_play = (struct snd_dma_buffer *)kmalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
			if (!prtd->mono_dma_play) {
				alsa_dbg("%s ERROR mono_dma_play kmalloc\n",__func__);
				return -ENOMEM;
			}
			prtd->mono_dma_play->dev.type = SNDRV_DMA_TYPE_DEV;
			prtd->mono_dma_play->dev.dev = 0;// pcm->card->dev;
			prtd->mono_dma_play->private_data = NULL;


			prtd->mono_dma_play->area = dma_alloc_writecombine(prtd->mono_dma_play->dev.dev, size, &prtd->mono_dma_play->addr, GFP_KERNEL);

			if (!prtd->mono_dma_play->area || !prtd->mono_dma_play->addr) {
				alsa_dbg("%s ERROR mono_dma_play dma_alloc_writecombine [%d]\n",__func__,size);
				return -ENOMEM;
			}

			prtd->mono_dma_play->bytes = size;
			alsa_dbg("mono_dma_play size [%d]\n", size);
		}

	} else {
		size = tcc_pcm_hardware_capture.buffer_bytes_max;

		if (dai->driver->capture.channels_min == 1) {
			prtd->mono_dma_capture = (struct snd_dma_buffer *)kmalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
			if (!prtd->mono_dma_capture) {
				alsa_dbg("%s ERROR mono_dma_capture kmalloc\n",__func__);
				return -ENOMEM;
			}
			prtd->mono_dma_capture->dev.type =  SNDRV_DMA_TYPE_DEV;
			prtd->mono_dma_capture->dev.dev = 0;// pcm->card->dev;
			prtd->mono_dma_capture->private_data =  NULL;

			prtd->mono_dma_capture->area = dma_alloc_writecombine(prtd->mono_dma_capture->dev.dev, size, &prtd->mono_dma_capture->addr, GFP_KERNEL);    
			if ( !prtd->mono_dma_capture->area || !prtd->mono_dma_capture->addr) {
				alsa_dbg("%s ERROR  mono_dma_capture dma_alloc_writecombine [%d]\n",__func__,size);
				return -ENOMEM;
			}
			prtd->mono_dma_capture->bytes = size;            
			alsa_dbg("mono_dma_capture size [%d]\n", size);
		}
	}

	buf->area = dma_alloc_writecombine(pcm->card->dev, size, &buf->addr, GFP_KERNEL);
	if (!buf->area || !buf->addr ) {
		alsa_dbg("%s ERROR dma_alloc_writecombine [%d]\n",__func__,size);
		return -ENOMEM;
	}
	else
		alsa_dbg("tcc_pcm_preallocate_dma_buffer size [%d] addr 0x%x, area 0x%x\n", size, (unsigned int)buf->area, (unsigned int)buf->addr);

	buf->bytes = size;

	return 0;
}


static void tcc_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct snd_dma_buffer *buf;
	int stream;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream) { continue; }

		buf = &substream->dma_buffer;
		if (!buf->area) { continue; }

		dma_free_writecombine(pcm->card->dev, buf->bytes, buf->area, buf->addr);
		buf->area = NULL;
	}

	if(prtd->mono_dma_play->area != 0 ){
		dma_free_writecombine(prtd->mono_dma_play->dev.dev, prtd->mono_dma_play->bytes, prtd->mono_dma_play->area, prtd->mono_dma_play->addr);
		alsa_dbg("%s mono_dma_play.area Free \n", __func__);
		prtd->mono_dma_play->area = 0;
	}

	if(prtd->mono_dma_capture->area != 0 ){
		dma_free_writecombine(prtd->mono_dma_capture->dev.dev, prtd->mono_dma_capture->bytes, prtd->mono_dma_capture->area, prtd->mono_dma_capture->addr);
		alsa_dbg("%s mono_dma_capture.area Free \n", __func__);
		prtd->mono_dma_capture->area = 0;
	}
}

static u64 tcc_pcm_dmamask = DMA_BIT_MASK(32);

static int tcc_pcm_new(struct snd_soc_pcm_runtime *rtd) 
{
	struct snd_card *card   = rtd->card->snd_card;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct snd_pcm *pcm     = rtd->pcm;
	int ret = 0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

	if (!card->dev->dma_mask) {
		card->dev->dma_mask = &tcc_pcm_dmamask;
	}
	if (!card->dev->coherent_dma_mask) {
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	}
	if (dai->driver->playback.channels_min) {
		ret = tcc_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret) { goto out; }
	}
	if (dai->driver->capture.channels_min) {
		ret = tcc_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret) { goto out; }
	} 

	if(prtd->ptcc_intr) {
		prtd->ptcc_intr->flag = 0;
	} else {
		alsa_adma_dbg(prtd->flag, prtd->id, "[%s] Error!! ptcc_intr is null!!\n", __func__);
	} 

out:
	return ret;
}


#if defined(CONFIG_PM)
static int tcc_pcm_suspend(struct snd_soc_dai *dai)
{
	/*	//move to adma driver
		struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

		alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
		prtd->backup_adma = (struct tcc_adma_backup_reg *)kmalloc(sizeof(struct tcc_adma_backup_reg), GFP_KERNEL);
		prtd->backup_adma->rTransCtrl	=	pcm_readl(prtd->ptcc_reg->adma_reg+ADMA_TRANSCTRL);
		prtd->backup_adma->rRptCtrl	=	pcm_readl(prtd->ptcc_reg->adma_reg+ADMA_RPTCTRL);
		prtd->backup_adma->rChCtrl 	=	pcm_readl(prtd->ptcc_reg->adma_reg+ADMA_CHCTRL);
		if(prtd->id == 0)
		prtd->backup_adma->rGIntReq=pcm_readl(prtd->ptcc_reg->adma_reg+ADMA_GINTREQ);
		*/
	return 0;
}

static int tcc_pcm_resume(struct snd_soc_dai *dai)
{
	/*	//move to adma driver
		struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

		alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
		pcm_writel(prtd->backup_adma->rTransCtrl, prtd->ptcc_reg->adma_reg+ADMA_TRANSCTRL);
		pcm_writel(prtd->backup_adma->rRptCtrl  , prtd->ptcc_reg->adma_reg+ADMA_RPTCTRL);
		pcm_writel(prtd->backup_adma->rChCtrl   , prtd->ptcc_reg->adma_reg+ADMA_CHCTRL);
		if(prtd->id == 0)
		pcm_writel(prtd->backup_adma->rGIntReq  , prtd->ptcc_reg->adma_reg+ADMA_GINTREQ);

		kfree(prtd->backup_adma);
		*/
	return 0;
}
#endif

static struct snd_soc_platform_driver tcc_soc_platform = {
	.ops      = &tcc_pcm_ops,
	.pcm_new  = tcc_pcm_new,
	.pcm_free = tcc_pcm_free_dma_buffers,
//#if defined(CONFIG_PM)
//	.suspend  = tcc_pcm_suspend,
//	.resume   = tcc_pcm_resume,
//#endif
};


int tcc_pcm_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &tcc_soc_platform);
}
EXPORT_SYMBOL_GPL(tcc_pcm_platform_register);

void tcc_pcm_platform_unregister(struct device *dev)
{
	return snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(tcc_pcm_platform_unregister);

MODULE_DESCRIPTION("Telechips PCM ASoC driver");
MODULE_LICENSE("GPL");
