// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCA_ALARAM_H
#define TCA_ALARAM_H

#include "tca_rtc.h"

#ifdef __cplusplus
extern
"C" {
#endif

void tca_alarm_gettime(void __iomem *pRTC, struct rtctime *pTime);
void tca_alarm_settime(void __iomem *pRTC, struct rtctime *pTime);

#ifdef __cplusplus
}
#endif

#endif //__TCA_ALARAM_H__

