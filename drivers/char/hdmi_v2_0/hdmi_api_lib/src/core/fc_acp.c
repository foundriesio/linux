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
#include <core/fc_acp.h>

void fc_acp_type(struct hdmi_tx_dev *dev, u8 type)
{
	LOG_TRACE1(type);
	hdmi_dev_write(dev, FC_ACP0, type);
}

void fc_acp_type_dependent_fields(struct hdmi_tx_dev *dev, u8 * fields, u8 fieldsLength)
{
	u8 c ;
	LOG_TRACE1(fields[0]);
	if (fieldsLength > (FC_ACP1 - FC_ACP16 + 1)) {
		fieldsLength = (FC_ACP1 - FC_ACP16 + 1);
		LOGGER(SNPS_WARN,"ACP Fields Truncated");
	}

	for (c = 0; c < fieldsLength; c++)
		hdmi_dev_write(dev, FC_ACP1 - c, fields[c]);
}
