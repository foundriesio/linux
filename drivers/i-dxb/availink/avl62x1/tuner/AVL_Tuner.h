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

#ifndef AVL_TUNER_H
#define AVL_TUNER_H

#include "IBSP.h"

#ifdef AVL_CPLUSPLUS
extern "C" {
#endif

struct AVL_Tuner;
typedef struct AVL_Tuner
{
    AVL_uint16 usTunerI2CAddr;
    AVL_uchar ucTunerLocked; //1 Lock;   0 unlock
    
    AVL_uint32 uiRFFrequencyHz;
    AVL_uint32 uiLPFHz;//only valid for satellite tuner

    AVL_uchar  ucBlindScanMode;
  
    void *vpMorePara;
    
    AVL_uint32 (* fpInitializeFunc)(struct AVL_Tuner *);
    AVL_uint32 (* fpLockFunc)(struct AVL_Tuner *);
    AVL_uint32 (* fpGetLockStatusFunc)(struct AVL_Tuner *);
    AVL_uint32 (* fpGetRFStrength)(struct AVL_Tuner *,AVL_int32 *);

    //Maximum tuner low pass filter bandwidth in Hz
    AVL_uint32 (* fpGetMaxLPF)(struct AVL_Tuner *, AVL_uint32 *);
    
    //Minimum tuner low pass filter bandwidth in Hz
    AVL_uint32 (* fpGetMinLPF)(struct AVL_Tuner *, AVL_uint32 *);

    //Low pass filter bandwidth step size in Hz
    AVL_uint32 (* fpGetLPFStepSize)(struct AVL_Tuner *, AVL_uint32 *);
    
    //Tuner AGC gain slope in dB per Volt (dB/V).
    //Tuners with non-inverted AGC sense have a positive slope.
    //Tuners with inverted AGC sense have a negative slope.
	//If the gain slope is not known, implement a function that
	//  returns 1 if the AGC sense is non-inverted,
	//  and returns -1 if the AGC sense is inverted.
    AVL_uint32 (* fpGetAGCSlope)(struct AVL_Tuner *, AVL_int32 *);
    
    //Voltage at which gain reaches minimum value.  Voltage in millivolts.
    //For a tuner with non-inverted sense (positive slope), this will be a small value.
    //For a tuner with inverted sense (negative slope), this will be a large value.
    AVL_uint32 (* fpGetMinGainVoltage)(struct AVL_Tuner *, AVL_uint32 *);
    
    //Voltage at which gain reaches its maximum value. Voltage in millivolts.
    //For a tuner with non-inverted sense (positive slope), this will be a large value.
    //For a tuner with inverted sense (negative slope), this will be a small value.
    AVL_uint32 (* fpGetMaxGainVoltage)(struct AVL_Tuner *, AVL_uint32 *);

    //RF Frequency step size in Hz
    AVL_uint32 (* fpGetRFFreqStepSize)(struct AVL_Tuner *, AVL_uint32 *);

}AVL_Tuner;

#ifdef AVL_CPLUSPLUS
}
#endif
    
#endif

