/**
 * @file cec.h
 * Generic configuration, OS abstraction and logging
 *
 * @author David Lopo
 *
 * Synopsys Inc.
 * SG DWC PT02
 *
 *****************************************************************************/
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
int cec_GetSend(struct cec_device * dev);
int cec_SetSend(struct cec_device * dev);
int cec_IntClear(struct cec_device * dev, unsigned char mask);
int cec_IntDisable(struct cec_device * dev, unsigned char mask);
int cec_IntEnable(struct cec_device * dev, unsigned char mask);
int cec_IntStatus(struct cec_device * dev, unsigned char mask);
int cec_CfgTxBuf(struct cec_device * dev, const char *buf, unsigned size);
int cec_CfgRxBuf(struct cec_device * dev, char *buf, unsigned size);
int cec_GetLocked(struct cec_device * dev);
int cec_SetLocked(struct cec_device * dev);
int cec_CfgBroadcastNAK(struct cec_device * dev, int enable);
int cec_ctrlReceiveFrame(struct cec_device * dev, char *buf, unsigned size);
int cec_ctrlSendFrame(struct cec_device * dev, const char *buf, unsigned size);
int cec_check_wake_up_interrupt(struct cec_device * dev);
int cec_CfgWakeupFlag(struct cec_device * dev, int wakeup);

/******************************************************************************
 *
 *****************************************************************************/

#endif /* TCC_CEC_H */

