#ifndef F_IAP2_EXT_ACC_H		/* 016.11.14 */
#define F_IAP2_EXT_ACC_H

extern int iap2_ext_acc_setup(void);
extern void iap2_ext_acc_cleanup(void);
extern int iap2_ext_acc_bind_config(struct usb_configuration *c);
extern int iap2_ext_acc_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl);

#endif /* F_IAP2_EXT_ACC_H */
