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
#ifndef AV201X_MOREAPI_H
#define AV201X_MOREAPI_H

#ifdef AVL_CPLUSPLUS
extern "C" {
#endif

	AVL_uint32 AV201X_GetMaxLPF(struct AVL_Tuner *pTuner, AVL_uint32 *puiMaxLPF_Hz);
	AVL_uint32 AV201X_GetMinLPF(struct AVL_Tuner *pTuner, AVL_uint32 *puiMinLPF_Hz);
	AVL_uint32 AV201X_GetLPFStepSize(struct AVL_Tuner *pTuner, AVL_uint32 *puiLPFStepSize_Hz);
	AVL_uint32 AV201X_GetAGCSlope(struct AVL_Tuner *pTuner, AVL_int32 *piAGCSlope_dBperV);
	AVL_uint32 AV201X_GetMinGainVoltage(struct AVL_Tuner *pTuner, AVL_uint32 *puiMinGainVoltage_mV);
	AVL_uint32 AV201X_GetMaxGainVoltage(struct AVL_Tuner *pTuner, AVL_uint32 *puiMaxGainVoltage_mV);
	AVL_uint32 AV201X_GetRFFreqStepSize(struct AVL_Tuner *pTuner, AVL_uint32 *puiRFFreqStepSizeHz);

#ifdef AVL_CPLUSPLUS
}
#endif

#endif
