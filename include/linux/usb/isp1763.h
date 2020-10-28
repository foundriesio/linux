/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) RuggedCom 2010
 *
 * Description:
 * Adapted from isp1760.h
 *
 * board initialization should put one of these into dev->platform_data
 * and place the isp1763 onto platform_bus named "isp1763-hcd".
 */

#ifndef __LINUX_USB_ISP1763_H
#define __LINUX_USB_ISP1763_H

struct isp1763_platform_data {
	unsigned bus_width_8:1;			/* 8/16-bit data bus width */
	unsigned port1_otg:1;			/* Port 1 supports OTG */
	unsigned dack_polarity_high:1;		/* DACK active high */
	unsigned dreq_polarity_high:1;		/* DREQ active high */
	unsigned intr_polarity_high:1;		/* INTR active high */
	unsigned intr_edge_trigger:1;		/* INTR edge trigger */
};

#endif /* __LINUX_USB_ISP1763_H */
