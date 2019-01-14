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
#ifndef HALFRAMECOMPOSERSPD_H_
#define HALFRAMECOMPOSERSPD_H_

typedef struct fc_spd_info {
	const u8 * vName;
	u8 vLength;
	const u8 * pName;
	u8 pLength;
	u8 code;
	u8 autoSend;
}fc_spd_info_t;

int fc_spd_config(struct hdmi_tx_dev *dev, fc_spd_info_t *spd_data);

#endif	/* HALFRAMECOMPOSERSPD_H_ */
