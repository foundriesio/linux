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


