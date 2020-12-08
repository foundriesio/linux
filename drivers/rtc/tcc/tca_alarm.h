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

int tca_alarm_gettime(void __iomem *rtcbaseaddress, struct rtctime *pTime);
int tca_alarm_settime(void __iomem *rtcbaseaddress, struct rtctime *pTime);
int tca_alarm_setint(void __iomem *rtcbaseaddress);
int tca_alarm_disable(void __iomem *rtcbaseaddress);

#ifdef __cplusplus
}
#endif

#endif //__TCA_ALARAM_H__

