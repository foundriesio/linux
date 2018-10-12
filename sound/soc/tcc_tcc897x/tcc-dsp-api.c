/*
 * linux/sound/soc/tcc/tcc-i2s-dsp.c
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>
     
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>
     
#include <mach/tca_mailbox.h>
#include <mach/tcc-dsp-api.h>

#undef alsa_dbg
#if 0
#define alsa_dbg(f, a...)  printk("== alsa-tcdsp == " f, ##a)
#else
#define alsa_dbg(f, a...)  
#endif

#define MAX_I2S_NUM 8

struct DSPSWParams tc_dsp_dai[MAX_I2S_NUM];

void tc_dsp_setCurrAudioBuffer(unsigned long i2s_num, unsigned long CurrAudioBufferAddr,int playback );

void tc_dsp_setI2sChannelNum(unsigned long i2s_num, unsigned long ChannelNum )
{
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        pDSPInfo->I2SNumber = ChannelNum;
    }
}

void tc_dsp_setI2sNum(unsigned long i2s_num, unsigned long I2SNumber )
{
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        pDSPInfo->I2SNumber = I2SNumber;
    }
}

void tc_dsp_setI2sFormat(unsigned long i2s_num, unsigned long I2SFormat )
{
    
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        pDSPInfo->I2SFormat = I2SFormat;
    }
}
void tc_dsp_setI2sSamplerate(unsigned long i2s_num, unsigned long Samplerate )
{
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        pDSPInfo->Samplerate = Samplerate;
    }
}

void tc_dsp_setI2sMasterMode(unsigned long i2s_num, unsigned long I2sMasterMode )
{
    
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        pDSPInfo->I2sMasterMode = I2sMasterMode;
    }
}

void tc_dsp_setAudioBuffer(unsigned long i2s_num, unsigned long AudioBufferAddr,
    unsigned long AudioBufferSize,unsigned long PeriodSize,int playback)
{
    
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        
        if(playback){
            pDSPInfo->outAudioBufferAddr = AudioBufferAddr;
            pDSPInfo->outAudioBufferSize = AudioBufferSize;
            pDSPInfo->outPeriodSize = PeriodSize;
        }
        else {
            pDSPInfo->inAudioBufferAddr = AudioBufferAddr;
            pDSPInfo->inAudioBufferSize = AudioBufferSize;
            pDSPInfo->inPeriodSize = PeriodSize;
        }  

	tc_dsp_setCurrAudioBuffer(i2s_num, AudioBufferAddr, playback);
    }
}

void tc_dsp_setPCMChannelNum(unsigned long i2s_num, unsigned long ChannelNum)
{
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        pDSPInfo->ChannelNum = ChannelNum;
    }
}

void tc_dsp_getCurrAudioBuffer(unsigned long i2s_num, unsigned long* CurrAudioBufferAddr,int playback )
{
    
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
        if(playback){
            *CurrAudioBufferAddr = pDSPInfo->outCurrAudioBufferAddr;
        }
        else {
            *CurrAudioBufferAddr = pDSPInfo->inCurrAudioBufferAddr;
        }
    }
}


void tc_dsp_setCurrAudioBuffer(unsigned long i2s_num, unsigned long CurrAudioBufferAddr,int playback )
{
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
    
        if(playback){
            pDSPInfo->outCurrAudioBufferAddr = CurrAudioBufferAddr;
        }
        else {
            pDSPInfo->inCurrAudioBufferAddr = CurrAudioBufferAddr;

        }
    }
}

#include "../../../drivers/misc/tcc/tcc_dsp/tcc_dsp.h"
#include "../../../drivers/misc/tcc/tcc_dsp/tcc_dsp_ipc.h"

void tc_dsp_DoPlayback(unsigned long i2s_num,unsigned long id, unsigned int dir)
{

    struct tcc_dsp_ipc_data_t tmp = {0};

    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];

        tmp.id = id;
        tmp.type = CMD_TYPE;

	if(dir == 0)//Rx
	{
	        tmp.param[0] = ALSA_IN_BUF_PARAM;
	        tmp.param[1] = pDSPInfo->Samplerate;
	        tmp.param[2] = pDSPInfo->inAudioBufferAddr;
	        tmp.param[3] = pDSPInfo->inAudioBufferSize;
	        tmp.param[4] = pDSPInfo->inPeriodSize;
	        tmp.param[5] = pDSPInfo->ChannelNum;
	}	
	else//Tx
	{
	        tmp.param[0] = ALSA_OUT_BUF_PARAM;
	        tmp.param[1] = pDSPInfo->Samplerate;
	        tmp.param[2] = pDSPInfo->outAudioBufferAddr;
	        tmp.param[3] = pDSPInfo->outAudioBufferSize;
	        tmp.param[4] = pDSPInfo->outPeriodSize;
	        tmp.param[5] = pDSPInfo->ChannelNum;
	}
        tcc_dsp_ipc_send_msg(&tmp);

        tmp.id = id;
        tmp.type = CMD_TYPE;
        tmp.param[0] = ALSA_PLAYBACK_START;
        tmp.param[1] = 0;
        tmp.param[2] = 0;
        tmp.param[3] = 0;
        tmp.param[4] = 0;
        tmp.param[5] = 0;
        tcc_dsp_ipc_send_msg(&tmp);

        pDSPInfo->DSPStatus &= (~DSP_STOP);        
        pDSPInfo->DSPStatus |= DSP_PLAYBACK;
    }
}

void tc_dsp_DoStop(unsigned long i2s_num,unsigned long id  )
{
    
    struct tcc_dsp_ipc_data_t tmp = {0};
    
    if(i2s_num < MAX_I2S_NUM){
        struct DSPSWParams* pDSPInfo = &tc_dsp_dai[i2s_num];
    
        tmp.id = id;
        tmp.type = CMD_TYPE;
        tmp.param[0] = ALSA_PLAYBACK_STOP;
        tmp.param[1] = 0;
        tmp.param[2] = 0;
        tmp.param[3] = 0;
        tmp.param[4] = 0;
        tmp.param[5] = 0;
        tcc_dsp_ipc_send_msg(&tmp);
        
        pDSPInfo->DSPStatus &= (~DSP_PLAYBACK);        
        pDSPInfo->DSPStatus |= DSP_STOP;

    }
}

void tc_applyDSPInfo2DSP(unsigned long i2s_num)
{
    // Applay info push to DSP

}

EXPORT_SYMBOL(tc_dsp_setI2sNum);
EXPORT_SYMBOL(tc_dsp_setPCMChannelNum);
EXPORT_SYMBOL(tc_dsp_setI2sChannelNum);
EXPORT_SYMBOL(tc_dsp_setI2sSamplerate);
EXPORT_SYMBOL(tc_dsp_setI2sFormat);
EXPORT_SYMBOL(tc_dsp_setI2sMasterMode);
EXPORT_SYMBOL(tc_dsp_DoStop);
EXPORT_SYMBOL(tc_dsp_DoPlayback);
EXPORT_SYMBOL(tc_dsp_getCurrAudioBuffer);
EXPORT_SYMBOL(tc_dsp_setCurrAudioBuffer);
EXPORT_SYMBOL(tc_dsp_setAudioBuffer);
