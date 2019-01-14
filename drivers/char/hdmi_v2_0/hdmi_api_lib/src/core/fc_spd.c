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
#include <include/hdmi_includes.h>
#include <include/hdmi_access.h>
#include <include/hdmi_log.h>
#include <hdmi_api_lib/src/core/frame_composer/frame_composer_reg.h>
#include <include/hdmi_ioctls.h>
#include <core/fc_spd.h>
#include <core/fc_packets.h>

void fc_spd_VendorName(struct hdmi_tx_dev *dev, const u8 * data, unsigned short length)
{
	unsigned short i;
	LOG_TRACE();
	for (i = 0; i < length; i++) {
		hdmi_dev_write(dev, FC_SPDVENDORNAME0 + (i*4), data[i]);
	}
}

void fc_spd_ProductName(struct hdmi_tx_dev *dev, const u8 * data, unsigned short length)
{
	unsigned short i;
	LOG_TRACE();
	for (i = 0; i < length; i++) {
		hdmi_dev_write(dev, FC_SPDPRODUCTNAME0 + (i*4), data[i]);
	}
}

void fc_spd_SourceDeviceInfo(struct hdmi_tx_dev *dev, u8 code)
{
	LOG_TRACE1(code);
	hdmi_dev_write(dev, FC_SPDDEVICEINF, code);
}

int fc_spd_config(struct hdmi_tx_dev *dev, fc_spd_info_t *spd_data)
{

        const unsigned short vSize = 8;
	const unsigned short pSize = 16;

	LOG_TRACE();

	if(spd_data == NULL){
		LOGGER(SNPS_ERROR, "Improper argument: spd_data");
		return FALSE;
	}

	fc_packets_AutoSend(dev, 0, SPD_TX);	/* prevent sending half the info. */

	if (spd_data->vName == NULL) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return FALSE;
	}
	if (spd_data->vLength > vSize) {
		spd_data->vLength = vSize;
		LOGGER(SNPS_WARN,"vendor name truncated");
	}
	if (spd_data->pName == NULL) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return FALSE;
	}
	if (spd_data->pLength > pSize) {
		spd_data->pLength = pSize;
		LOGGER(SNPS_WARN,"product name truncated");
	}

	fc_spd_VendorName(dev, spd_data->vName, spd_data->vLength);
	fc_spd_ProductName(dev, spd_data->pName, spd_data->pLength);

	fc_spd_SourceDeviceInfo(dev, spd_data->code);

	if (spd_data->autoSend) {
		fc_packets_AutoSend(dev, spd_data->autoSend, SPD_TX);
	} else {
		fc_packets_ManualSend(dev, SPD_TX);
	}

	return TRUE;
}
