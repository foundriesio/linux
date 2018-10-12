/*
 * linux/sound/soc/tcc/tcc-pcm-v20.c  
 *
 * Based on:    linux/sound/arm/pxa2xx-pcm.c
 * Author:  <linux@telechips.com>
 * Created: Nov 30, 2004
 * Modified:    Feb 24, 2016
 * Description: ALSA PCM interface for the Telechips TCC898x, TCC802x chip 
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

#if defined(CONFIG_ARCH_TCC898X) 
#include <asm/system_info.h>
#endif

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tcc-i2s.h"
#include "tcc-pcm.h"

#include "tcc/tca_tcchwcontrol.h"

#undef alsa_dbg
#if 0
#define alsa_adma_dbg(flag, id, f, a...)  printk("== alsa-debug PCM V20 %s-%d== " f,\
		(flag&TCC_I2S_FLAG)?"I2S":(flag&TCC_SPDIF_FLAG)?"SPDIF":"CDIF",id,##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug PCM V20 == " f, ##a)
#else
#define alsa_adma_dbg(flag, id, f, a...)
#define alsa_dbg(f, a...)
#endif


static int tcc_adma_tx_enable(struct snd_pcm_substream *substream, int En);
static int tcc_adma_rx_enable(struct snd_pcm_substream *substream, int En);

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
	unsigned int period = params_period_bytes(params);
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
	unsigned int period = params_period_bytes(params);
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

	if (((dmaInterruptSource & DMA_I2S_OUT)	&& (flag & TCC_RUNNING_PLAY))||
			((dmaInterruptSource & DMA_SPDIF_OUT) && (flag & TCC_RUNNING_SPDIF_PLAY))) {
		snd_pcm_period_elapsed(prtd->ptcc_intr->play_ptr);
	}
	// Planet 20150812 S/PDIF_Rx
	if (((dmaInterruptSource & DMA_I2S_IN) && (flag & TCC_RUNNING_CAPTURE))||
			((dmaInterruptSource & DMA_CDIF_SPDIF_IN) && (flag & TCC_RUNNING_CDIF_SPDIF_CAPTURE))) {
		snd_pcm_period_elapsed(prtd->ptcc_intr->cap_ptr);
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
	unsigned int tRptCtrl=0, tChCtrl=0;
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
	//	totsize = ttemp * min_period_size; //this can occured mis-match buffer size when mmap mode.

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);        
	runtime->dma_bytes = totsize;

	prtd->dma_buffer_end = runtime->dma_addr + totsize;
	prtd->dma_buffer = runtime->dma_addr;
	prtd->period_size = period_bytes;
	prtd->nperiod = period_bytes;

	alsa_dbg("buffer bytes=0x%X period bytes=0x%X, rate=%d, chs=%d\n",
			totsize, period_bytes, params_rate(params), params_channels(params));
	alsa_dbg("~~~!!! Substream->pcm->device = %d\n",substream->pcm->device);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		set_dma_outbuffer(prtd, params);
	} else {
		set_dma_inbuffer(prtd, params);
	}

	if(prtd->flag & TCC_I2S_FLAG){
		prtd->flag |= (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? DMA_I2S_OUT : DMA_I2S_IN;

		tRptCtrl=0;
		tChCtrl=0;
		switch(chs){
			case 8:
#if defined(CONFIG_ARCH_TCC898X)
				if(prtd->id == 0) {

					tRptCtrl |= (0x7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw24: DAI TX DMA Multi-channel Enable
						//Hw21, Hw20: DAI TX DMA Multi-channel b'11: 7.1ch
						tChCtrl |= (Hw24 | Hw21 | Hw20);
					}else{
						//Hw25: DAI RX DMA Multi-channel Enable
						//Hw23, Hw22: DAI RX DMA Multi-channel b'11: 7.1ch
						tChCtrl |= (Hw25 | Hw23 | Hw22);
					}

				} else { // TCC898x Audio1-I2S
#if defined(BURST_SIZE_4)
					tRptCtrl |= (0xF << 24);
#else
					tRptCtrl |= (0x7 << 24);
#endif  //if defined(BURST_SIZE_4)
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw21, Hw20: DAI TX DMA Multi-channel b'10: 7.1ch
						tChCtrl |= Hw21;
					}else{
						//Hw23, Hw22: DAI RX DMA Multi-channel b'10: 7.1ch
						tChCtrl |= Hw23;
					}

				}
#else //This is for TCC802x
				{                                 
					tRptCtrl |= (0x7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw24: DAI TX DMA Multi-channel Enable
						//Hw21, Hw20: DAI TX DMA Multi-channel b'11: 7.1ch
						tChCtrl |= (Hw24 | Hw21 | Hw20);
					}else{
						//Hw25: DAI RX DMA Multi-channel Enable
						//Hw23, Hw22: DAI RX DMA Multi-channel b'11: 7.1ch
						tChCtrl |= (Hw25 | Hw23 | Hw22);
					}
				}
#endif	//if defined(CONFIG_ARCH_TCC898X)
				break;
#if 0
				//TCC doesn't support 6channel for ALSA.
			case 6:
#if defined(CONFIG_ARCH_TCC898X)
				if(prtd->id == 0) {                                 

					tRptCtrl |= (0x7 << 24);

					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						tChCtrl |= (Hw24 | Hw21 | Hw20);
					}else{
						tChCtrl |= (Hw25 | Hw23 | Hw22);
					}

				} else {

					tRptCtrl |= (0x5 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						tChCtrl |= Hw20;
					}else{
						tChCtrl |= Hw22;
					}
				}
#else
				{                                 
					tRptCtrl |= (7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						tChCtrl |= (Hw24 | Hw20);
					}else{
						tChCtrl |= (Hw25 | Hw22);
					}
				}
#endif	//if defined(CONFIG_ARCH_TCC898X)
				break;
#endif	//if 0
			case 4:
#if defined(CONFIG_ARCH_TCC898X)
				if(prtd->id == 0) {                                 

					tRptCtrl |= (0x7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw24: DAI TX DMA Multi-channel Enable
						//Hw21, Hw20: DAI TX DMA Multi-channel b'00: 3.1ch
						tChCtrl |= Hw24;
					}else{
						//Hw25: DAI TX DMA Multi-channel Enable
						//Hw23, Hw22: DAI RX DMA Multi-channel b'00: 3.1ch
						tChCtrl |= Hw25;
					}
				} else { // TCC898x Audio1-I2S
#if defined(BURST_SIZE_4)
					tRptCtrl |= (0x7 << 24);
#else
					tRptCtrl |= (0x3 << 24);
#endif  //if defined(BURST_SIZE_4)
					//Hw21, Hw20: DAI TX DMA Multi-channel b'00: 3.1ch
					//Hw23, Hw22: DAI RX DMA Multi-channel b'00: 3.1ch
				} 
#else //This is for TCC802x
				{                                 
					tRptCtrl |= (0x7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw24: DAI TX DMA Multi-channel Enable
						//Hw21, Hw20: DAI TX DMA Multi-channel b'00: 3.1ch
						tChCtrl |= Hw24;
					}else{
						//Hw25: DAI TX DMA Multi-channel Enable
						//Hw23, Hw22: DAI RX DMA Multi-channel b'00: 3.1ch
						tChCtrl |= Hw25;
					}
				}
#endif	//if defined(CONFIG_ARCH_TCC898X)
				break;
			case 2:
			case 1:
#if defined(CONFIG_ARCH_TCC898X)
				if(prtd->id == 0) {                                 

					tRptCtrl |= (0x7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw21, Hw20: DAI TX DMA Multi-channel b'11: 7.1ch
						//When Stereo, This is for DMA All area used.
						tChCtrl |= (Hw21 | Hw20);
					}else{
						//Hw23, Hw22: DAI RX DMA Multi-channel b'11: 7.1ch
						//When Stereo, This is for DMA All area used.
						tChCtrl |= (Hw23 | Hw22);
					}

				} else { // TCC898x Audio1-I2S

					tRptCtrl  |= (0x1 << 24);
				} 
#else //This is for TCC802x
				{                                 
					tRptCtrl |= (0x7 << 24);
					if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
						//Hw21, Hw20: DAI TX DMA Multi-channel b'11: 7.1ch
						//When Stereo, This is for DMA All area used.
						tChCtrl |= (Hw21 | Hw20);
					}else{
						//Hw23, Hw22: DAI RX DMA Multi-channel b'11: 7.1ch
						//When Stereo, This is for DMA All area used.
						tChCtrl |= (Hw23 | Hw22);
					}
				}
#endif	//if defined(CONFIG_ARCH_TCC898X)
				break;
			default:
				printk("ERROR!! %d channel is not supported!\n", chs);
		}

		tRptCtrl = pcm_readl(padma_reg+ADMA_RPTCTRL)| (tRptCtrl & (Hw28-Hw24));
		pcm_writel(tRptCtrl, padma_reg+ADMA_RPTCTRL);

		spin_lock_irqsave(prtd->adma_slock, flags);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL) | (tChCtrl & (Hw24 | Hw21 | Hw20));
		}else{
			tChCtrl = pcm_readl(padma_reg+ADMA_CHCTRL) | (tChCtrl & (Hw25 | Hw23 | Hw22));
		}	
		pcm_writel(tChCtrl, padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

	} else {    //This is for SPDIF and CDIF

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
	unsigned long flags;
	unsigned int bStop=0, ret=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

	if(prtd->flag & TCC_I2S_FLAG){
		spin_lock_irqsave(prtd->adma_slock, flags);
		bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
		/* AUDIO DMA I2S TX/RX */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			bStop = bStop & Hw28;

			if(bStop){ 
				alsa_dbg("%s() DAI TX ADMA stop start.\n", __func__);
				ret = tcc_adma_tx_enable(substream, 0);

				if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
				else alsa_dbg("%s() playback end complete.\n", __func__);
			}

			alsa_dbg("[%s] close i2s Playback device\n", __func__);
		} else {
			bStop = bStop & Hw30;

			if(bStop){    
				alsa_dbg("%s() DAI RX ADMA stop start.\n", __func__);
				ret = tcc_adma_rx_enable(substream, 0);

				if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
				else alsa_dbg("%s() capture end complete.\n", __func__);
			}

			alsa_dbg("[%s] close i2s Capture device\n", __func__);
		}
	}

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
	unsigned long flags;
	unsigned int bStop=0, ret=0;
	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

	if(prtd->flag & TCC_I2S_FLAG){
		spin_lock_irqsave(prtd->adma_slock, flags);
		bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
		spin_unlock_irqrestore(prtd->adma_slock, flags);
		/* AUDIO DMA I2S TX/RX */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			bStop = bStop & Hw28;

			if(bStop){ 
				alsa_dbg("%s() DAI TX ADMA stop start.\n", __func__);
				ret = tcc_adma_tx_enable(substream, 0);

				if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
				else alsa_dbg("%s() playback end complete.\n", __func__);
			}

			alsa_dbg("[%s] close i2s Playback device\n", __func__);
		} else {
			bStop = bStop & Hw30;

			if(bStop){    
				alsa_dbg("%s() DAI RX ADMA stop start.\n", __func__);
				ret = tcc_adma_rx_enable(substream, 0);

				if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
				else alsa_dbg("%s() capture end complete.\n", __func__);
			}

			alsa_dbg("[%s] close i2s Capture device\n", __func__);
		}
	}
	//DMA initialize
	memset(runtime->dma_area, 0x00, runtime->dma_bytes);
	alsa_dbg(" %s %d dma_bytes -> 0 set Done\n",__func__,runtime->dma_bytes);
	return ret;
}

static int tcc_adma_hcnt_clear(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;	// adma base addr
	unsigned int reg_value=0, tTCnt=0;
	unsigned long flags;

	if(prtd->flag & TCC_I2S_FLAG) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tTCnt = pcm_readl(padma_reg+ADMA_TXDATCNT);
		} else {
			tTCnt = pcm_readl(padma_reg+ADMA_RXDATCNT);
		}
	} else {	//SPIDF TX or CDIF/SPDIF RX
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tTCnt = pcm_readl(padma_reg+ADMA_TXSPTCNT);
		} else {
			tTCnt = pcm_readl(padma_reg+ADMA_RXCDTCNT);
		}
	}

	if((tTCnt & 0xFFFF0000) == 0x0) {
		goto out;
	} else { 
		alsa_adma_dbg(prtd->flag, prtd->id, "[%s] tTCnt = 0x%08x\n", __func__, tTCnt);
	} 

	/* Remain DMA Hop Count Clear Start */
	spin_lock_irqsave(prtd->adma_slock, flags);
	reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
	//Hw30: HCNT clear
	reg_value |= Hw30;
	pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
	spin_unlock_irqrestore(prtd->adma_slock, flags);

	if(prtd->flag & TCC_I2S_FLAG) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			reg_value = pcm_readl(padma_reg+ADMA_TXDASAR);
			pcm_writel(reg_value, padma_reg+ADMA_TXDASAR);
			/* 
			   pcm_writel(0x0, padma_reg+ADMA_TXDASAR1);
			   pcm_writel(0x0, padma_reg+ADMA_TXDASAR2);
			   pcm_writel(0x0, padma_reg+ADMA_TXDASAR3);
			   pcm_writel(0x0, padma_reg+ADMA_TXDASARL);
			   pcm_writel(0x0, padma_reg+ADMA_TXDASARL1);
			   pcm_writel(0x0, padma_reg+ADMA_TXDASARL2);
			   pcm_writel(0x0, padma_reg+ADMA_TXDASARL3);
			   */
		} else {
			reg_value = pcm_readl(padma_reg+ADMA_RXDADAR);
			pcm_writel(reg_value, padma_reg+ADMA_RXDADAR);
			/* 
			   pcm_writel(0x0, padma_reg+ADMA_RXDADAR1);
			   pcm_writel(0x0, padma_reg+ADMA_RXDADAR2);
			   pcm_writel(0x0, padma_reg+ADMA_RXDADAR3);
			   pcm_writel(0x0, padma_reg+ADMA_RXDADARL);
			   pcm_writel(0x0, padma_reg+ADMA_RXDADARL1);
			   pcm_writel(0x0, padma_reg+ADMA_RXDADARL2);
			   pcm_writel(0x0, padma_reg+ADMA_RXDADARL3);
			   */
		}
	} else {	//SPIDF TX or CDIF/SPDIF RX
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			reg_value = pcm_readl(padma_reg+ADMA_TXSPSAR);
			pcm_writel(reg_value, padma_reg+ADMA_TXSPSAR);
		} else {
			reg_value = pcm_readl(padma_reg+ADMA_RXCDDAR);
			pcm_writel(reg_value, padma_reg+ADMA_RXCDDAR);
		}
	}
	mdelay(1);

	spin_lock_irqsave(prtd->adma_slock, flags);
	reg_value = pcm_readl(padma_reg+ADMA_TRANSCTRL);
	//Hw30: HCNT clear
	reg_value &= ~Hw30;
	pcm_writel(reg_value, padma_reg+ADMA_TRANSCTRL);
	spin_unlock_irqrestore(prtd->adma_slock, flags);
	/* Remain DMA Hop Count Clear End */

	if((tTCnt & 0xFFFF0000) != 0x0) {
		alsa_adma_dbg(prtd->flag, prtd->id, "[%s] has some error!! tTCnt = 0x%08x\n", __func__, tTCnt);
		return -1;
	} 
out:
	return 0;
}

static int tcc_adma_tx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	//    void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg; // adma base addr
	unsigned int tChCtrl=0, reg_value=0;
	unsigned long flags;
	int ret=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	if(En) {
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
		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
#if defined(CONFIG_ARCH_TCC898X) 
		if(system_rev < 0x0002){	//!(TCC898X_EVT2)
			//Hw29: SPDIF DMA TX enable
			tChCtrl = reg_value & Hw29;
			if(!tChCtrl)
				reg_value &= ~Hw28;
		}else{
			reg_value &= ~Hw28;
		}
#else
		reg_value &= ~Hw28;
#endif
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		// BITCLR(pADMA->ChCtrl, Hw0); /* We must have 1 mdelay for DTIEN */
		//mdelay(1);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		/* Remain DAI TX DMA Hop Count Clear Start */
		ret = tcc_adma_hcnt_clear(substream);
		if(ret) {
			alsa_adma_dbg(prtd->flag, prtd->id, "Error when HCNT clear.\n");
			ret = 1;
		}
		/* Remain DAI TX DMA Hop Count Clear End */

		//tca_adma_dump(padma_reg);
		//tca_dai_dump(pdai_reg);
	}
	return ret;
}

static int tcc_adma_rx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	//    void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg; // adma base addr
	unsigned int tChCtrl=0, reg_value=0;
	unsigned long flags;
	int ret=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	if(En) {

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

		spin_lock_irqsave(prtd->adma_slock, flags);
		reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
#if defined(CONFIG_ARCH_TCC898X) 
		if(system_rev < 0x0002){	//!(TCC898X_EVT2)
			//Hw31: SPDIF DMA RX enable
			tChCtrl = reg_value & Hw31;
			if(!tChCtrl)
				reg_value &= ~Hw30;
		}else{
			reg_value &= ~Hw30;
		}
#else
		reg_value &= ~Hw30;
#endif
		pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
		//BITCLR(pADMA->ChCtrl, Hw2); /* We must have 1 mdelay for RTIEN */
		//mdelay(1);
		spin_unlock_irqrestore(prtd->adma_slock, flags);

		/* Remain DAI RX DMA Hop Count Clear Start */
		ret = tcc_adma_hcnt_clear(substream);
		if(ret) {
			alsa_adma_dbg(prtd->flag, prtd->id, "Error when HCNT clear.\n");
			ret = 1;
		}
		/* Remain DAI TX DMA Hop Count Clear End */

		//tca_adma_dump(padma_reg);
		//tca_dai_dump(pdai_reg);

	}
	return ret;
}


static int tcc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	//void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	void __iomem *padma_reg = prtd->ptcc_reg->adma_reg;
	unsigned int reg_value=0;
	unsigned long flags=0;
#if defined(CONFIG_ARCH_TCC898X) 
	unsigned int tChCtrl=0;
#endif
	int ret = 0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	alsa_dbg("cmd[%d] frame_bits[%d]\n", cmd, substream->runtime->frame_bits);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:        
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if(prtd->flag & TCC_I2S_FLAG) {
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() playback start\n", __func__);

					ret = tcc_adma_tx_enable(substream, 1);
					if(ret) alsa_dbg("%s() playback start has some error.\n", __func__);
					else alsa_dbg("%s() playback start\n", __func__);

				} else {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() recording start\n", __func__);

					ret = tcc_adma_rx_enable(substream, 1);
					if(ret) alsa_dbg("%s() capture start has some error.\n", __func__);
					else alsa_dbg("%s() capture start\n", __func__);
				}
			} else {	//For SPDIF and CDIF
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					alsa_dbg("%s() playback start\n", __func__);
					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
#if defined(CONFIG_ARCH_TCC898X) 
					if(system_rev < 0x0002){	//!(TCC898X_EVT2)
						//Hw28: DAI TX ADMA enable
						tChCtrl = reg_value & Hw28;
						if(!tChCtrl)
							reg_value |= Hw28;
					}
#endif
					//Hw29: SPDIF TX ADMA enable
					//Hw1: SPDIF TX INTR enable
					reg_value |= Hw29 | Hw1;
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);
				} else {	//For CDIF, SPDIF RX 
					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					//Hw31: CDIF/SPDIF RX ADMA enable
					//Hw26: CDIF/SPDIF RX Select
					//Hw3: CDIF/SPDIF RX INTR enable
					reg_value |= Hw31 | Hw26 | Hw3;	
					//CDIF Select
					if(prtd->flag & TCC_CDIF_FLAG) reg_value &= ~Hw26;
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);
				}
			}
			break;

		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if(prtd->flag & TCC_I2S_FLAG) {
				/* AUDIO DMA I2S TX/RX */
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() playback end\n", __func__);
#if 0
					// Planet 20160113 Add CDIF Bypass Start
					if((tcc_i2s->id == 0)&&(tcc_i2s->cdif_bypass_en == 1)) {
						reg_value = pcm_readl(tcc_i2s->dai_reg+CDIF_BYPASS_CICR);
						reg_value &= ~Hw7;			// CDIF Disable
						pcm_writel(reg_value, tcc_i2s->dai_reg+CDIF_BYPASS_CICR);

						reg_value = pcm_readl(tcc_i2s->dai_reg+I2S_DAMR);
						reg_value &= ~(Hw2 | Hw8);			// CDIF Monitor Mode & CDIF Clock Select
						pcm_writel(reg_value, tcc_i2s->dai_reg+I2S_DAMR);
						tcc_i2s->cdif_bypass_en = 0;
					}

					pcm_writel(0x10, tcc_i2s->dai_reg+I2S_DAVC); /* Mute ON */
#endif
					memset(runtime->dma_area, 0x00, runtime->dma_bytes);
				} else {
					alsa_adma_dbg(prtd->flag, prtd->id, "%s() capture end\n", __func__);
				}
			} else {	//For SPDIF and CDIF
				if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
#if defined(CONFIG_ARCH_TCC898X) 
					if(system_rev < 0x0002) {	//!(TCC898X_EVT2)
						//Hw28: DAI TX ADMA enable
						//Hw0: DAI TX ADMA Intr enable
						tChCtrl = reg_value & Hw0;
						if(!tChCtrl)
							reg_value &= ~Hw28;
					}
#endif
					//Hw29: SPDIF TX ADMA enable
					//Hw1: SPDIF TX INTR enable
					reg_value &= ~(Hw29 | Hw1);
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);
					/* Remain SPDIF TX DMA Hop Count Clear */
					ret = tcc_adma_hcnt_clear(substream);
					if(ret) { 
						printk("== alsa-debug PCM V20 %s_TX-%d== Error when HCNT clear.\n", 
							(prtd->flag&TCC_SPDIF_FLAG)?"SPDIF":"CDIF",prtd->id);
					}

				} else {	//For SPDIF RX, CDIF
					spin_lock_irqsave(prtd->adma_slock, flags);
					reg_value = pcm_readl(padma_reg+ADMA_CHCTRL);
					//Hw31: CDIF/SPDIF RX ADMA enable
					//Hw26: CDIF/SPDIF RX Select
					//Hw3: CDIF/SPDIF RX INTR enable
					reg_value &= ~(Hw31 | Hw26 | Hw3);
					pcm_writel(reg_value, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);
					/* Remain CDIF/SPDIF RX DMA Hop Count Clear */
					ret = tcc_adma_hcnt_clear(substream);
					if(ret) { 
						printk("== alsa-debug PCM V20 %s_RX-%d== Error when HCNT clear.\n", 
							(prtd->flag&TCC_SPDIF_FLAG)?"SPDIF":"CDIF",prtd->id);
					}
				}
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
	if(prtd->flag & TCC_I2S_FLAG) {
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

		if(prtd->flag & TCC_I2S_FLAG){
			spin_lock_irqsave(prtd->adma_slock, flags);
			bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
			spin_unlock_irqrestore(prtd->adma_slock, flags);
			/* AUDIO DMA I2S TX/RX */
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_PLAY;
				bStop = bStop & Hw28;

				if(bStop){ 
					alsa_dbg("%s() DAI TX ADMA stop start.\n", __func__);
					ret = tcc_adma_tx_enable(substream, 0);

					if(ret) alsa_dbg("%s() playback stop has some error.\n", __func__);
					else alsa_dbg("%s() playback end complete.\n", __func__);
				}

				alsa_dbg("[%s] close i2s Playback device\n", __func__);
			} else {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_CAPTURE;
				bStop = bStop & Hw30;

				if(bStop){    
					alsa_dbg("%s() DAI RX ADMA stop start.\n", __func__);
					ret = tcc_adma_rx_enable(substream, 0);

					if(ret) alsa_dbg("%s() capture stop has some error.\n", __func__);
					else alsa_dbg("%s() capture end complete.\n", __func__);
				}

				alsa_dbg("[%s] close i2s Capture device\n", __func__);
			}

		} else {	//for SPDIF and CDIF

			spin_lock_irqsave(prtd->adma_slock, flags);
			bStop = pcm_readl(padma_reg+ADMA_CHCTRL);
			spin_unlock_irqrestore(prtd->adma_slock, flags);

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_SPDIF_PLAY;

				if(bStop & Hw29){ 
					bStop &= ~(Hw29); /* SPDIF TX ADMA Disable */

					spin_lock_irqsave(prtd->adma_slock, flags);
					pcm_writel(bStop, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);
				}

				alsa_dbg("[%s] close spdif Playback device\n", __func__);
			} else {	//SPDIF RX, CDIF
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_CDIF_SPDIF_CAPTURE;

				if(bStop & Hw31){ 
					bStop &= ~(Hw31); /* SPDIF RX, CDIF ADMA Disable */

					spin_lock_irqsave(prtd->adma_slock, flags);
					pcm_writel(bStop, padma_reg+ADMA_CHCTRL);
					spin_unlock_irqrestore(prtd->adma_slock, flags);
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
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
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
#if defined(CONFIG_PM)
	.suspend  = tcc_pcm_suspend,
	.resume   = tcc_pcm_resume,
#endif
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
