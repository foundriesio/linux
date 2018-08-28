#ifndef F_IAP2_H		/* 016.11.14 */
#define F_IAP2_H

extern int iap_setup(void);
extern void iap_cleanup(void);
extern int iap_bind_config(struct usb_configuration *c);
extern int iap_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl);

#endif /* F_IAP2_H */