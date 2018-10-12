/*
 * linux/sound/soc/tcc/tcc-pcm-dsp.c  
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
#include "../../../drivers/misc/tcc/tcc_dsp/tcc_dsp.h"
#include "../../../drivers/misc/tcc/tcc_dsp/tcc_dsp_ipc.h"

/* DSP */
#include <mach/tcc-dsp-api.h>

#undef alsa_dbg
#if 0
#define alsa_adma_dbg(flag, id, f, a...)  printk("== alsa-debug PCM DSP %s-%d== " f, \
		(flag&TCC_I2S_FLAG)?"I2S":(flag&TCC_SPDIF_FLAG)?"SPDIF":"CDIF",id, ##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug PCM DSP == " f, ##a)
#else
#define alsa_adma_dbg(flag, id, f, a...)
#define alsa_dbg(f, a...)
#endif
#define alsa_err(f, a...)  printk("== alsa-error PCM DSP == " f, ##a)

static const struct snd_pcm_hardware tcc_pcm_hardware_play = {
	.info = (SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID
			| SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_PAUSE
			| SNDRV_PCM_INFO_RESUME),

	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE,
	.rates        = SNDRV_PCM_RATE_8000_48000,
	.rate_min     = 8000,
	.rate_max     = 48000,
	.channels_min = 1,
	.channels_max = 2,
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

	.formats      = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.rates        = SNDRV_PCM_RATE_8000_48000,
	.rate_min     = 8000,
	.rate_max     = 48000,
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

static int tcc_get_vir_i2s_idx(int i2s_dev_id, struct snd_pcm_substream *substream)
{
	int vir_i2s_idx = 0;
	if(i2s_dev_id == 10)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			vir_i2s_idx =	VIR_IDX_I2S0;
		else
			vir_i2s_idx = -1;
	}
	else if (i2s_dev_id == 11)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			vir_i2s_idx =	VIR_IDX_I2S1;
		else
			vir_i2s_idx =	-1;
	}			
	else if (i2s_dev_id == 12)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			vir_i2s_idx =	VIR_IDX_I2S2;
		else
			vir_i2s_idx =	-1;
	}
	else if(i2s_dev_id == 13)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			vir_i2s_idx = -1;
		else
			vir_i2s_idx =	VIR_IDX_I2S3;
	}
	else
		vir_i2s_idx =	-1;

	if(vir_i2s_idx < 0)
	{
		alsa_err("not supported  virtual i2s device id=%d!!!!", i2s_dev_id);
		return -1;
	}
	return vir_i2s_idx;	
}

static int tcc_dsp_get_ipc_idx(int i2s_dev_id, struct snd_pcm_substream *substream)
{
	unsigned int ipc_idx;
	
	if(i2s_dev_id == 10)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ipc_idx = TCC_DSP_IPC_ID_ALSA_MAIN;
		else
			ipc_idx = -1;
	}
	else if (i2s_dev_id == 11)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ipc_idx = TCC_DSP_IPC_ID_ALSA_SUB01;
		else
			ipc_idx = -1;	
	}
	else if (i2s_dev_id == 12)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ipc_idx = TCC_DSP_IPC_ID_ALSA_SUB02;
		else
			ipc_idx = -1;
	}
	else if(i2s_dev_id == 13)
	{
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			ipc_idx = -1;
		else
			ipc_idx = TCC_DSP_IPC_ID_ALSA_IN_MAIN;
	}
	else
		ipc_idx = -1;		
	
	if(ipc_idx < 0)
	{
		alsa_err("not supported virtual i2s device id=%d!!!!", i2s_dev_id);
		return -1;
	}
	
	return ipc_idx;
}

static struct tcc_runtime_data *arr_prtd[VIR_IDX_I2SMAX] = {NULL, NULL, NULL, NULL};
void tcc_vir_dma_done_handler(int num, int dir)
{
	struct tcc_runtime_data *prtd;
	unsigned int flag;
	unsigned int dmaInterruptSource = 0;

	if( (num >= 0) && (num < VIR_IDX_I2SMAX) ){
		prtd = arr_prtd[num];
		dmaInterruptSource = (dir == 1) ? DMA_I2S_OUT : DMA_I2S_IN;
	} else {
		return;
	}

	if(prtd == NULL)
	{
		alsa_dbg("%s %d error! this device is not opened num=%d. \n", __func__, __LINE__, num);
		return;
	}

	flag = prtd->ptcc_intr->flag;

	if (((dmaInterruptSource & DMA_I2S_OUT)	&& (flag & TCC_RUNNING_PLAY))||
			((dmaInterruptSource & DMA_SPDIF_OUT) && (flag & TCC_RUNNING_SPDIF_PLAY))) {
		snd_pcm_period_elapsed(prtd->ptcc_intr->play_ptr);
	}
	
	if (((dmaInterruptSource & DMA_I2S_IN) && (flag & TCC_RUNNING_CAPTURE))||
			((dmaInterruptSource & DMA_CDIF_SPDIF_IN) && (flag & TCC_RUNNING_CDIF_SPDIF_CAPTURE))) {
		snd_pcm_period_elapsed(prtd->ptcc_intr->cap_ptr);
	}
}
EXPORT_SYMBOL(tcc_vir_dma_done_handler);

static int tcc_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	unsigned int vir_i2s_idx =	VIR_IDX_I2S0;
	size_t totsize = params_buffer_bytes(params);
	size_t period_bytes = params_period_bytes(params);
	snd_pcm_format_t format = params_format(params);
	int chs = params_channels(params);
	int ret = 0;

	alsa_dbg("%s %d\n", __func__, __LINE__);
	
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);        
	runtime->dma_bytes = totsize;

	prtd->dma_buffer_end = runtime->dma_addr + totsize;
	prtd->dma_buffer = runtime->dma_addr;
	prtd->period_size = period_bytes;
	prtd->nperiod = period_bytes;

    alsa_dbg("totsize=0x%X period=0x%X  rate=%d, chs=%d\n",
             totsize, period_bytes, params_rate(params), params_channels(params));

    vir_i2s_idx = tcc_get_vir_i2s_idx(prtd->id, substream);
	
	if(vir_i2s_idx >= 0)
	{
	    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tc_dsp_setAudioBuffer(vir_i2s_idx,(unsigned long)prtd->dma_buffer,totsize,period_bytes,1);
			tc_dsp_setI2sMasterMode(vir_i2s_idx,0);
			tc_dsp_setI2sFormat(vir_i2s_idx, format /*SNDRV_PCM_FORMAT_S24_LE*/);
			tc_dsp_setI2sSamplerate(vir_i2s_idx, params_rate(params));
			tc_dsp_setI2sChannelNum(vir_i2s_idx,chs );
			tc_dsp_setPCMChannelNum(vir_i2s_idx, chs);
	    } else {
			tc_dsp_setAudioBuffer(vir_i2s_idx,(unsigned long)prtd->dma_buffer,totsize,period_bytes,0);
			tc_dsp_setI2sMasterMode(vir_i2s_idx,0);
			tc_dsp_setI2sFormat(vir_i2s_idx, format /*SNDRV_PCM_FORMAT_S24_LE*/);
			tc_dsp_setI2sSamplerate(vir_i2s_idx, params_rate(params));
			tc_dsp_setI2sChannelNum(vir_i2s_idx,chs );
			tc_dsp_setPCMChannelNum(vir_i2s_idx, chs);
	    }
	}
	else
		ret = -1;

    return ret;
}

static int tcc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

	if (prtd->flag & DMA_FLAG) {
		snd_pcm_set_runtime_buffer(substream, NULL);
	}

	return 0;
}

static int tcc_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int ret=0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);
	
	//DMA initialize
	memset(runtime->dma_area, 0x00, runtime->dma_bytes);
	alsa_dbg(" %s %d dma_bytes -> 0 set Done\n",__func__,runtime->dma_bytes);
	return ret;
}

static int tcc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	unsigned int vir_i2s_idx =	VIR_IDX_I2S0;	
	unsigned int ipc_idx;
	int ret = 0;

	alsa_dbg("%s %d\n", __func__, __LINE__);
	
    vir_i2s_idx = tcc_get_vir_i2s_idx(prtd->id, substream);
	ipc_idx = tcc_dsp_get_ipc_idx(prtd->id, substream);
	
	if(vir_i2s_idx < 0 || ipc_idx < 0)
	{
		alsa_err("not supported device id=%d!!!!", prtd->id);
		return -1;
	}

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:        
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				tc_dsp_DoPlayback(vir_i2s_idx,ipc_idx, 1);
			
			} else {
				tc_dsp_DoPlayback(vir_i2s_idx,ipc_idx, 0);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				tc_dsp_DoStop(vir_i2s_idx,ipc_idx);
			} else {
				tc_dsp_DoStop(vir_i2s_idx,ipc_idx);
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
	unsigned int vir_i2s_idx = VIR_IDX_I2S0;	
	snd_pcm_uframes_t x;
	dma_addr_t ptr = 0;
	
    vir_i2s_idx = tcc_get_vir_i2s_idx(prtd->id, substream);

	if(vir_i2s_idx >= 0)
	{
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		{
			tc_dsp_getCurrAudioBuffer(vir_i2s_idx, (unsigned long*)&ptr,1);
		}
		else
		{
			tc_dsp_getCurrAudioBuffer(vir_i2s_idx, (unsigned long*)&ptr,0);
		}

		x = bytes_to_frames(runtime, ptr - runtime->dma_addr);

		if (x < runtime->buffer_size) {
			return x;
		}
	}
	else
		return 0;
}

static int tcc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_audio_intr *pintr;
	int ret=0;
	unsigned int vir_i2s_idx = VIR_IDX_I2S0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]open pcm device, %s\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "output" : "input"));
    
	vir_i2s_idx = tcc_get_vir_i2s_idx(prtd->id, substream);

	if(vir_i2s_idx < 0)
	{
		ret = -1;
		goto out;
	}

	arr_prtd[vir_i2s_idx] = prtd;

	if(!(prtd->flag & TCC_RUNNING_DAI)){

	pintr = kzalloc(sizeof(struct tcc_audio_intr), GFP_KERNEL);

	if (pintr == NULL) {
		ret = -ENOMEM;
			goto free_n_out;
	}

	memset(pintr, 0, sizeof(struct tcc_audio_intr));
		prtd->ptcc_intr = pintr;
	}

	//Buffer size should be 2^n.
	snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES);

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

	tc_dsp_setI2sNum(vir_i2s_idx,vir_i2s_idx);
		
	goto out;

free_n_out:
	if(!(prtd->flag & TCC_RUNNING_FLAG)){
		kfree(prtd->ptcc_intr);
	}
out:
	return ret;
}

static int tcc_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	unsigned int ret=0;

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]close pcm device, %s\n", __func__,
			substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? "output" : "input");

	if (prtd) {
		if(prtd->flag & TCC_I2S_FLAG){
			/* AUDIO DMA I2S TX/RX */
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_PLAY;

				alsa_dbg("[%s] close i2s Playback device\n", __func__);
			} else {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_CAPTURE;

				alsa_dbg("[%s] close i2s Capture device\n", __func__);
			}

		}else{ //for SPDIF and CDIF
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_SPDIF_PLAY;
			}else{
				prtd->ptcc_intr->flag &= ~TCC_RUNNING_CDIF_SPDIF_CAPTURE;
			}
		} 
		
		if ((prtd->ptcc_intr)&&(!(prtd->flag & TCC_RUNNING_FLAG))){
			prtd->ptcc_intr->flag &= ~TCC_INTERRUPT_REQUESTED;
			kfree(prtd->ptcc_intr);
		}
	}

	return ret;
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

		if(tcc_pcm_hardware_play.channels_min == 1){
			prtd->mono_dma_play = (struct snd_dma_buffer *)kmalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
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

		if(tcc_pcm_hardware_capture.channels_min == 1){
			prtd->mono_dma_capture = (struct snd_dma_buffer *)kmalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
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
	struct snd_pcm *pcm     = rtd->pcm;
	int ret = 0;

	//alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

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

out:
	return ret;
}


#if defined(CONFIG_PM)
static int tcc_pcm_suspend(struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

	return 0;
}

static int tcc_pcm_resume(struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	alsa_adma_dbg(prtd->flag, prtd->id, "[%s]\n", __func__);

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

int tcc_pcm_dsp_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &tcc_soc_platform);
}
EXPORT_SYMBOL_GPL(tcc_pcm_dsp_platform_register);

void tcc_pcm_dsp_platform_unregister(struct device *dev)
{
	return snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(tcc_pcm_dsp_platform_unregister);

MODULE_DESCRIPTION("Telechips PCM DSP driver");
MODULE_LICENSE("GPL");

