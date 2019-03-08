/*
 *           Copyright 2007-2017 Availink, Inc.
 *
 *  This software contains Availink proprietary information and
 *  its use and disclosure are restricted solely to the terms in
 *  the corresponding written license agreement. It shall not be 
 *  disclosed to anyone other than valid licensees without
 *  written permission of Availink, Inc.
 *
 */


///$Date: 2017-8-3 15:59 $
///
#include "AV201X.h"
#include "AV201X_MoreAPI.h"

//extern s_AV201X_Params gs_AV201X_Params;

AVL_uint32 AV201X_GetMaxLPF(struct AVL_Tuner *pTuner, AVL_uint32 *puiMaxLPF_Hz)
{
	AVL_uint32 r = 0;
	*puiMaxLPF_Hz = (AVL_uint32)(AV201X_FILTER_BANDWIDTH_MAX*1000*1000);
	return (r);
}

AVL_uint32 AV201X_GetMinLPF(struct AVL_Tuner *pTuner, AVL_uint32 *puiMinLPF_Hz)
{
	AVL_uint32 r = 0;
	*puiMinLPF_Hz = (AVL_uint32)(AV201X_FIX_LPF_MIN*1000);
	return (r);
}

AVL_uint32 AV201X_GetLPFStepSize(struct AVL_Tuner *pTuner, AVL_uint32 *puiLPFStepSize_Hz)
{
	AVL_uint32 r = 0;
	*puiLPFStepSize_Hz = 166142; //ceil(211kHz/1.27)
	return (r);
}

AVL_uint32 AV201X_GetAGCSlope(struct AVL_Tuner *pTuner, AVL_int32 *piAGCSlope_dBperV)
{
	AVL_uint32 r = 0;
	*piAGCSlope_dBperV = -48;
	return (r);
}

AVL_uint32 AV201X_GetMinGainVoltage(struct AVL_Tuner *pTuner, AVL_uint32 *puiMinGainVoltage_mV)
{
	AVL_uint32 r = 0;
	*puiMinGainVoltage_mV = 2450;
	return (r);
}

AVL_uint32 AV201X_GetMaxGainVoltage(struct AVL_Tuner *pTuner, AVL_uint32 *puiMaxGainVoltage_mV)
{
	AVL_uint32 r = 0;
	*puiMaxGainVoltage_mV = 100;
	return (r);
}

AVL_uint32 AV201X_GetRFFreqStepSize(struct AVL_Tuner *pTuner, AVL_uint32 *puiRFFreqStepSizeHz)
{
	AVL_uint32 r = 0;
	AVL_uint32 xtal_freq_kHz = (AVL_uint32)(gs_AV201X_Params.eXTAL_Freq);
	*puiRFFreqStepSizeHz = (AVL_uint32)((xtal_freq_kHz*1000 + (1<<16))/(1<<17));
	return (r);
}
