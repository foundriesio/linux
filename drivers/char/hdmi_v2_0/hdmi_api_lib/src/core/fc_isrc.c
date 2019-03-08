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
#include <core/fc_isrc.h>

void fc_isrc_status(struct hdmi_tx_dev *dev, u8 code)
{
	LOG_TRACE1(code);
	hdmi_dev_write_mask(dev, FC_ISCR1_0, FC_ISCR1_0_ISRC_STATUS_MASK, code);
}

void fc_isrc_valid(struct hdmi_tx_dev *dev, u8 validity)
{
	LOG_TRACE1(validity);
	hdmi_dev_write_mask(dev, FC_ISCR1_0, FC_ISCR1_0_ISRC_VALID_MASK, (validity ? 1 : 0));
}

void fc_isrc_cont(struct hdmi_tx_dev *dev, u8 isContinued)
{
	LOG_TRACE1(isContinued);
	hdmi_dev_write_mask(dev, FC_ISCR1_0, FC_ISCR1_0_ISRC_CONT_MASK, (isContinued ? 1 : 0));
}

void fc_isrc_isrc1_codes(struct hdmi_tx_dev *dev, u8 * codes, u8 length)
{
	u8 c;
	LOG_TRACE1(codes[0]);
	if (length > (FC_ISCR1_1 - FC_ISCR1_16 + 1)) {
		length = (FC_ISCR1_1 - FC_ISCR1_16 + 1);
		LOGGER(SNPS_WARN,"ISCR1 Codes Truncated");
	}

	for (c = 0; c < length; c++)
		hdmi_dev_write(dev, FC_ISCR1_1 - c, codes[c]);
}

void fc_isrc_isrc2_codes(struct hdmi_tx_dev *dev, u8 * codes, u8 length)
{
	u8 c;
	LOG_TRACE1(codes[0]);
	if (length > (FC_ISCR2_0 - FC_ISCR2_15 + 1)) {
		length = (FC_ISCR2_0 - FC_ISCR2_15 + 1);
		LOGGER(SNPS_WARN,"ISCR2 Codes Truncated");
	}

	for (c = 0; c < length; c++)
		hdmi_dev_write(dev, FC_ISCR2_0 - c, codes[c]);
}
