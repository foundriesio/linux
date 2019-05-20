/****************************************************************************
hdmi_cec

Copyright (C) 2018 Telechips Inc.

NOTE: Tab size is 8
****************************************************************************/
#ifndef __HDMI_CEC_H__
#define __HDMI_CEC_H__

#include "../include/hdmi_cec.h"

/** Broadcast logical address */
#define BCST_ADDR   (0x0FUL)
/** Control register bitfield */
#define    FRAME_TYP1   (0x1 << 2)
/** Control register bitfield */
#define    FRAME_TYP0   (0x1 << 1)

/******************************************************************************
 * define
 *****************************************************************************/
int cec_Init(struct cec_device * dev);
int cec_Disable(struct cec_device * dev, int wakeup);
int cec_CfgWakeupFlag(struct cec_device * dev, int wakeup);
int cec_CfgLogicAddr(struct cec_device * dev, unsigned addr, int enable);
int cec_CfgStandbyMode(struct cec_device * dev, int enable);
int cec_CfgSignalFreeTime(struct cec_device * dev, int time);

int cec_CfgBroadcastNAK(struct cec_device * dev, int enable);
int cec_ctrlReceiveFrame(struct cec_device * dev, char *buf, unsigned size);
int cec_ctrlSendFrame(struct cec_device * dev, const char *buf, unsigned size);
int cec_check_wake_up_interrupt(struct cec_device * dev);
int cec_set_wakeup(struct cec_device * dev, unsigned int flag);
int cec_clear_wakeup(struct cec_device * dev);
void cec_register_dump(struct cec_device * dev);

/******************************************************************************
 *
 *****************************************************************************/

#endif /* TCC_CEC_H */

