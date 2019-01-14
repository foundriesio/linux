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
#ifndef PRODUCTPARAMS_H_
#define PRODUCTPARAMS_H_

/** For detailed handling of this structure, refer to documentation of the functions */


void product_reset(struct hdmi_tx_dev *dev, productParams_t * product);

u8 product_SetProductName(struct hdmi_tx_dev *dev, productParams_t *product, const u8 *data, u8 length);

u8 product_SetVendorName(struct hdmi_tx_dev *dev, productParams_t *product, const u8 *data, u8 length);

u8 product_SetVendorPayload(struct hdmi_tx_dev *dev, productParams_t *product, const u8 *data, u8 length);

u8 product_IsSourceProductValid(struct hdmi_tx_dev *dev, productParams_t *product);

u8 product_IsVendorSpecificValid(struct hdmi_tx_dev *dev, productParams_t *product);

#endif	/* PRODUCTPARAMS_H_ */
