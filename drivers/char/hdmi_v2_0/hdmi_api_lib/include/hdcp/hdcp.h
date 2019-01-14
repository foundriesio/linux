/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
#ifndef __API_HDCP_H_
#define __API_HDCP_H_

#include "../../../include/hdmi_includes.h"
#include "../../../include/hdmi_ioctls.h"
#include "../../../include/hdmi_access.h"

#define ADDR_JUMP 4

typedef enum {
	HDCP_IDLE = 0,
	HDCP_KSV_LIST_READY,
	HDCP_ERR_KSV_LIST_NOT_VALID,
	HDCP_KSV_LIST_ERR_DEPTH_EXCEEDED,
	HDCP_KSV_LIST_ERR_MEM_ACCESS,
	HDCP_ENGAGED,
	HDCP_FAILED,
} hdcp_status_t;

typedef enum {
	HDCP2_NOT_CAPABLE = 7,
	HDCP2_CAPABLE,
	HDCP2_AUTHENTICATION_LOST,
	HDCP2_AUTHENTICATED,
	HDCP2_AUTHENTICATION_FAIL,
	HDCP2_DECRYPTED_CHG,
} hdcp2_status_t;

/**
 * The method handles DONE and ERROR events.
 * A DONE event will trigger the retrieving the read byte, and sending a request to read the
 * following byte. The EDID is read until the block is done and then the reading moves to the next
 * block. When the block is successfully read, it is sent to be parsed.
 * @param dev Device structure
 * @param hpd on or off
 * @param state of the HDCP engine interrupts
 * @param param to be returned to application:
 * 		no of KSVs in KSV LIST if KSV_LIST_EVENT
 * 		1 (engaged) 0 (fail) if HDCP_EGNAGED_EVENT
 * @return the state of which the event was handled (FALSE for fail)
 */
/* @param ksvHandler Handler to call when KSV list is ready*/
u8 hdcp_event_handler(struct hdmi_tx_dev *dev, int *param);

/**
 * Enable/disable HDCP 1.4
 * @param dev Device structure
 * @param enable
 */
void hdcp_rxdetect(struct hdmi_tx_dev *dev, u8 enable);

/**
 * Enter or exit AV mute mode
 * @param dev Device structure
 * @param enable the HDCP AV mute
 */
void hdcp_av_mute(struct hdmi_tx_dev *dev, int enable);

/**
 * Bypass data encryption stage
 * @param dev Device structure
 * @param bypass the HDCP AV mute
 */
// void hdcp_BypassEncryption(struct hdmi_tx_dev *dev, int bypass);

void hdcp_sw_reset(struct hdmi_tx_dev *dev);

/**
 *@param dev Device structure
 * @param disable the HDCP encrption
 */
void hdcp_disable_encryption(struct hdmi_tx_dev *dev, int disable);

/**
 * @param baseAddr base address of HDCP module registers
 * @return HDCP interrupts state
 */
u8 hdcp_interrupt_status(struct hdmi_tx_dev *dev);

/**
 * Clear HDCP interrupts
 * @param dev Device structure
 * @param value mask of interrupts to clear
 * @return TRUE if successful
 */
int hdcp_interrupt_clear(struct hdmi_tx_dev *dev, u8 value);

u8 _HDCP22CtrlRegReset(struct hdmi_tx_dev *dev);
void hdcp_statusinit(struct hdmi_tx_dev *dev);
u8 _HDCP22RegMask(struct hdmi_tx_dev *dev, u8 value);
u8 _HDCP22RegMute(struct hdmi_tx_dev *dev, u8 value);
u8 _HDCP22RegStat(struct hdmi_tx_dev *dev, u8 value);
u8 _HDCP22RegStatusRead(struct hdmi_tx_dev *dev);
u8 _HDCP22RegMaskRead(struct hdmi_tx_dev *dev);
u8 _HDCP22RegMuteRead(struct hdmi_tx_dev *dev);
u8 _HDCP22RegReadStat(struct hdmi_tx_dev *dev);
u8 hdcp22_event_handler(struct hdmi_tx_dev *dev, int *param);

void _InterruptMask(struct hdmi_tx_dev *dev, u8 value);
u8 _InterruptMaskStatus(struct hdmi_tx_dev *dev);

#endif /* __API_HDCP_H_ */
