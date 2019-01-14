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
#include <core/fc_vsd.h>

void fc_vsd_vendor_OUI(struct hdmi_tx_dev *dev, u32 id)
{
	LOG_TRACE1(id);
	hdmi_dev_write(dev, (FC_VSDIEEEID0), id);
	hdmi_dev_write(dev, (FC_VSDIEEEID1), id >> 8);
	hdmi_dev_write(dev, (FC_VSDIEEEID2), id >> 16);
}

u8 fc_vsd_vendor_payload(struct hdmi_tx_dev *dev, const u8 * data, unsigned short length)
{
	const unsigned short size = 24;
	unsigned i;
	LOG_TRACE();
	if (data == NULL) {
		LOGGER(SNPS_WARN,"invalid parameter");
		return 1;
	}
	if (length > size) {
		length = size;
		LOGGER(SNPS_WARN,"vendor payload truncated");
	}
	for (i = 0; i < length; i++) {
		hdmi_dev_write(dev, (FC_VSDPAYLOAD0 + (i*4)), data[i]);
	}
	// VSD sise IEEE(3-bit) + length
	hdmi_dev_write(dev, FC_VSDSIZE, length + 3);
	return 0;
}

