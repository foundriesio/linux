/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
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
