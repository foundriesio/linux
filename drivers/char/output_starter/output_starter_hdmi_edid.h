/****************************************************************************
Copyright (C) 2018 Telechips Inc.

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
#ifndef __HEMI_EDID_H__
#define __HEMI_EDID_H__


int edid_get_scdc_present(void);
int edid_get_lts_340mcs_scramble(void);
int edid_get_hdmi20(void);
int edid_is_sink_need_hdcp_keepout(void);
int edid_get_optimal_settings(struct hdmi_tx_dev *dev, int *hdmi_mode, int *vic, encoding_t *encoding, int optimal_phase);

#endif /* __HEMI_EDID_H__ */
