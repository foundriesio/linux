/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef INCLUDE_IDENTIFICATION_IDENTIFICATION_H_
#define INCLUDE_IDENTIFICATION_IDENTIFICATION_H_

#include "../../../include/hdmi_includes.h"

#define PRODUCT_HDMI_TX  0xA0

/**
 * Read product design information
 * @param baseAddr base address of controller
 * @return the design number stored in the hardware
 */
u8 id_design(struct hdmi_tx_dev *dev);

/**
 * Read product revision information
 * @param baseAddr base address of controller
 * @return the revision number stored in the hardware
 */
u8 id_revision(struct hdmi_tx_dev *dev);

/**
 * Read product line information
 * @param baseAddr base address of controller
 * @return the product line stored in the hardware
 */
u8 id_product_line(struct hdmi_tx_dev *dev);

/**
 * Read product type information
 * @param baseAddr base address of controller
 * @return the product type stored in the hardware
 */
u8 id_product_type(struct hdmi_tx_dev *dev);

/**
 * Check if HDCP is instantiated in hardware
 * @param baseAddr base address of controller
 * @return TRUE if hardware supports HDCP encryption
 */
int id_hdcp_support(struct hdmi_tx_dev *dev);

int id_hdcp14_support(struct hdmi_tx_dev *dev);

int id_hdcp22_support(struct hdmi_tx_dev *dev);

int id_phy(struct hdmi_tx_dev *dev);

#endif /* INCLUDE_IDENTIFICATION_IDENTIFICATION_H_ */


