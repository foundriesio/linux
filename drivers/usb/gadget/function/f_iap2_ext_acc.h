// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef F_IAP2_EXT_ACC_H		/* 016.11.14 */
#define F_IAP2_EXT_ACC_H

int iap2_ext_acc_setup(void);
void iap2_ext_acc_cleanup(void);
int iap2_ext_acc_bind_config(struct usb_configuration *c);
int iap2_ext_acc_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl);

#endif /* F_IAP2_EXT_ACC_H */
