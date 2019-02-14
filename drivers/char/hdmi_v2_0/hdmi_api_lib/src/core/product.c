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


void product_reset(struct hdmi_tx_dev *dev, productParams_t * product)
{
	product->mVendorNameLength = 0;
	product->mProductNameLength = 0;
	product->mSourceType = (u8) (-1);
	product->mOUI = (u8) (-1);
	product->mVendorPayloadLength = 0;
        memset(product->mVendorName, 0, sizeof(product->mVendorName));
        memset(product->mProductName, 0, sizeof(product->mProductName));
        memset(product->mVendorPayload, 0, sizeof(product->mVendorPayload));
}

u8 product_SetProductName(struct hdmi_tx_dev *dev, productParams_t * product, const u8 * data, u8 length)
{
	u16 i;
	if ((data == NULL) || length > sizeof(product->mProductName)) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return 1;
	}
	product->mProductNameLength = 0;
	for (i = 0; i < sizeof(product->mProductName); i++) {
		product->mProductName[i] = (i < length) ? data[i] : 0;
	}
	product->mProductNameLength = length;
	return 0;
}

u8 product_SetVendorName(struct hdmi_tx_dev *dev, productParams_t * product, const u8 * data, u8 length)
{
	u16 i;
	if (data == NULL || length > sizeof(product->mVendorName)) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return 1;
	}
	product->mVendorNameLength = 0;
	for (i = 0; i < sizeof(product->mVendorName); i++) {
		product->mVendorName[i] = (i < length) ? data[i] : 0;
	}
	product->mVendorNameLength = length;
	return 0;
}

u8 product_SetVendorPayload(struct hdmi_tx_dev *dev, productParams_t * product, const u8 * data, u8 length)
{
	u16 i;
	if (data == NULL || length > sizeof(product->mVendorPayload)) {
		LOGGER(SNPS_ERROR,"invalid parameter");
		return 1;
	}
	product->mVendorPayloadLength = 0;
	for (i = 0; i < sizeof(product->mVendorPayload); i++) {
		product->mVendorPayload[i] = (i < length) ? data[i] : 0;
	}
	product->mVendorPayloadLength = length;
	return 0;
}

u8 product_IsSourceProductValid(struct hdmi_tx_dev *dev, productParams_t * product)
{
	return (product->mSourceType != (u8)(-1)) && (product->mVendorNameLength != 0) && (product->mProductNameLength != 0);
}

u8 product_IsVendorSpecificValid(struct hdmi_tx_dev *dev, productParams_t * product)
{
	return (product->mOUI != (u32)(-1));
}
