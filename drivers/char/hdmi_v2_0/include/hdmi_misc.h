/****************************************************************************
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#ifndef __HDMI_MISC_H__
#define __HDMI_MISC_H__

//MISC Funcations
int dwc_hdmi_misc_register(struct hdmi_tx_dev *dev);
int dwc_hdmi_misc_deregister(struct hdmi_tx_dev *dev);
void dwc_hdmi_power_on_core(struct hdmi_tx_dev *dev, int need_link_reset);

// API Functions
void dwc_hdmi_api_register(struct hdmi_tx_dev *dev);
void dwc_hdmi_power_on(struct hdmi_tx_dev *dev);
void dwc_hdmi_power_off(struct hdmi_tx_dev *dev);

int hdmi_api_dump_regs(void);
void hdmi_activate_callback(void);
int hdmi_api_vsif_update(productParams_t *productParams);
int hdmi_api_vsif_update_by_index(int index);

#endif	/* __HDMI_MISC_H__ */
