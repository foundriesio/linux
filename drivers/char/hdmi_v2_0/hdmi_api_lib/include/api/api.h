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

