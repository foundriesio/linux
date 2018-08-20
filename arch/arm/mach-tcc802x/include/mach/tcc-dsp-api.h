/*
 * linux/sound/soc/tcc/tcc-i2s-dsp.c
 *
 */

#ifndef _tcc_PCM_DSP_H
#define _tcc_PCM_DSP_H


#define DSP_PLAYBACK    1
#define DSP_STOP        (1 << 1)


enum//Max Device is 8 per 1 card
{
	VIR_IDX_I2S0 = 0, //alsa main
	VIR_IDX_I2S1,     //alsa sub1
	VIR_IDX_I2S2,     //alsa sub2
	VIR_IDX_I2S3,     //alsa rx start index
	VIR_IDX_I2SMAX
};

struct DSPSWParams {
	unsigned long I2SNumber;
	unsigned long I2SFormat; // SNDRV_PCM_FORMAT_S24_LE SNDRV_PCM_FORMAT_S16_LE
	unsigned long Samplerate;
	unsigned long I2sMasterMode;
	unsigned long outAudioBufferAddr;
	unsigned long outAudioBufferSize;
	unsigned long outCurrAudioBufferAddr;
	unsigned long outPeriodSize;
	unsigned long inAudioBufferAddr;
	unsigned long inAudioBufferSize;
	unsigned long inCurrAudioBufferAddr;
	unsigned long inPeriodSize;
	unsigned long ChannelNum;
	unsigned long DSPStatus;     
};

extern void tc_dsp_setI2sChannelNum(unsigned long i2s_num, unsigned long ChannelNum );
extern void tc_dsp_setI2sNum(unsigned long i2s_num, unsigned long I2SNumber );
extern void tc_dsp_setI2sFormat(unsigned long i2s_num, unsigned long I2SFormat );
extern void tc_dsp_setI2sSamplerate(unsigned long i2s_num, unsigned long Samplerate );
extern void tc_dsp_setI2sMasterMode(unsigned long i2s_num, unsigned long I2sMasterMode );
extern void tc_dsp_setAudioBuffer(unsigned long i2s_num, unsigned long AudioBufferAddr,
	unsigned long AudioBufferSize,unsigned long PeriodSize,int playback );
extern void tc_dsp_setPCMChannelNum(unsigned long i2s_num, unsigned long ChannelNum);
extern void tc_dsp_getCurrAudioBuffer(unsigned long i2s_num, unsigned long* CurrAudioBufferAddr,int playback );
extern void tc_dsp_setCurrAudioBuffer(unsigned long i2s_num, unsigned long CurrAudioBufferAddr,int playback );
extern void tc_dsp_DoPlayback(unsigned long i2s_num ,unsigned long id, unsigned int dir);
extern void tc_dsp_DoStop(unsigned long i2s_num,unsigned long id );
extern void tc_applyDSPInfo2DSP(unsigned long i2s_num);



#endif
