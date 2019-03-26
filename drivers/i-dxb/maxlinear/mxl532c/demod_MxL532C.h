////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2008 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (¡§MStar Confidential Information¡¨) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _MXL532C_H_
#define _MXL532C_H_

#include "MaxLinearDataTypes.h"

#define __DVB_CORE__
//#include "frontend.h"
//#include <linux/dvb/frontend.h>
#include "dvb_frontend.h"
//#include "../aml_dvb.h"
#define printf printk

#define MxLI2CSlvAddr 0xC0//no matter what it is by kevin.deng

struct mxl532C_fe_config {
	int                   i2c_id;
	int                 reset_pin;
	int                 demod_addr;
	int                 tuner_addr;
	void 			  *i2c_adapter;
};


struct mxl532C_state {
	struct mxl532C_fe_config config;
	struct i2c_adapter *i2c;
	u32                 freq;
//        fe_modulation_t     mode;
        u32                 symbol_rate;
        struct dvb_frontend fe;
};


extern void MxL532C_Init(void);
extern void MxL532C_Tune(UINT32 u32TunerFreq, UINT8 u8BandWidth);
extern UINT32 MxL532C_GetSNR(void);
extern MXL_BOOL_E MxL532C_GetLock(void);
extern UINT32 MxL532C_GetBER(void);
extern SINT32 MxL532C_GetSigStrength(void);
extern UINT16 MxL532C_GetTPSCellID(void);
extern void MxL532C_PowerOnOff(UINT8 u8PowerOn);
extern void Mxl532C_Debug(void);
#endif

