// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef F_IAP2_H		/* 016.11.14 */
#define F_IAP2_H

int iap_setup(void);
void iap_cleanup(void);
int iap_bind_config(struct usb_configuration *c);
int iap_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl);

#endif /* F_IAP2_H */
