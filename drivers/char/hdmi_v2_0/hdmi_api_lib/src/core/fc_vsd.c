// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
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

