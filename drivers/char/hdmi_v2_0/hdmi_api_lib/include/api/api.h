/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef __API_H__
#define __API_H__

/** @addtogroup init_api_grp Initialization API Routines
 *
 * These routines handle initialization of the CIL and PCD driver components
 * and the DWC_usb3 controller.
 */
/** @{ */

/**
 * Configure API.
 * Configure the modules of the API according to the parameters given by
 * the user. If EDID at sink is read, it does parameter checking using the
 * Check methods against the sink's E-EDID. Violations are outputted to the
 * buffer.
 * Shall only be called after an Init call or configure.
 * @return TRUE when successful
 * @note during this function, all controller's interrupts are disabled
 * @note this function needs to have the HW initialized before the first call
 */
int hdmi_api_Configure(struct hdmi_tx_dev *dev);

/**
 * Disable API.
 */
int hdmi_api_Disable(struct hdmi_tx_dev *dev);

/**
 * AV Mute in the General Control Packet
 * @param enable TRUE set the AVMute in the general control packet
 */
void hdmi_api_avmute(struct hdmi_tx_dev *dev, int enable);

/** @} */

#endif	/* __API_H__*/

