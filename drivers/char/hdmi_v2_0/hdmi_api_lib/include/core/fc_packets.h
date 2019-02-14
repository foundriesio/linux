/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2019 -present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
#ifndef HALFRAMECOMPOSERPACKETS_H_
#define HALFRAMECOMPOSERPACKETS_H_


#define ACP_TX  	0
#define ISRC1_TX 	1
#define ISRC2_TX 	2
#define SPD_TX 		4
#define VSD_TX 		3

void fc_packets_metadata_config(struct hdmi_tx_dev *dev);

void fc_packets_AutoSend(struct hdmi_tx_dev *dev, u8 enable, u8 mask);

void fc_packets_ManualSend(struct hdmi_tx_dev *dev, u8 mask);

void fc_packets_disable_all(struct hdmi_tx_dev *dev);

#endif	/* HALFRAMECOMPOSERPACKETS_H_ */
