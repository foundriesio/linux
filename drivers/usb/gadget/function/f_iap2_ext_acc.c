// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gadget Function Driver for
 * Apple iAP2 External Accessory Protocol Native Transfer
 *
 * Copyright (C) 2016 Telechips, Inc.
 * Author: thjeong <thjeong@telechips.com>
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <linux/types.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/usb.h>
#include <linux/usb/ch9.h>

#include <linux/configfs.h>
#include <linux/usb/composite.h>

#include "configfs.h"
#include "f_iap2_ext_acc.h"

#if 0
#define EA_DBG(stuff...)   pr_debug("[INFO][USB] EA_DBG: " stuff)
#else
#define EA_DBG(stuff...)                        do { } while (0)
#endif

#define IAP2_EXT_ACC_BULK_BUFFER_SIZE           (512U)
#define IAP2_EXT_ACC_MAX_INST_NAME_LEN          (40)

/* String IDs */
#define IAP2_EXT_ACC_INTF_ALT0_STRING_INDEX     (0)

/* iAP2 Ext. Acc. protocol name */
static char iap2_ext_acc_protocol_name0[256];

/* iAP2 Ext. Acc. Default protocol name */
#define IAP2_EXT_ACC_DEF_INTF_STRING0           ("com.company.protocol0")

#ifndef USB_MFI_SUBCLASS_VENDOR_SPEC
#define USB_MFI_SUBCLASS_VENDOR_SPEC            (0xf0)
#endif

/* number of tx and rx requests to allocate */
#define IAP2_EXT_ACC_TX_REQ_MAX                 (4)
#define IAP2_EXT_ACC_RX_REQ_MAX                 (2)

enum iap2_ext_acc_state {
	IAP2_EXT_ACC_DISCONNECTED = 0,	// Disconnected
	IAP2_EXT_ACC_CONNECTED,	// Connected with ALT 0
	IAP2_EXT_ACC_ONLINE	// Connected with ALT 1
};

struct iap2_ext_acc_event {
	/* size of the event */
	size_t length;
	/* event data to send */
	void *data;
};

struct iap2_ext_acc_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	struct miscdevice *misc_dev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	/* iap2_ext_acc Gadget Interface Number */
	u8 intf;
	/* Indicate current alterate setting number */
	u8 alt;

	/* Status of iap2_ext_acc gadget */
	enum iap2_ext_acc_state state;

	/* synchronize access to our device file */
	atomic_t open_excl;

	/* List of usb requests that is not queued */
	struct list_head tx_idle;
	/* List of usb requests that is queued */
	struct list_head tx_busy;
	wait_queue_head_t write_wq;

	wait_queue_head_t read_wq;
	struct usb_request *rx_req[IAP2_EXT_ACC_RX_REQ_MAX];
	int32_t rx_done;

	/* Notify start of EA gadget */
	struct delayed_work start_work;
	/* Notify change of alternate setting */
	struct delayed_work alt_change_work;

};

#define IAP2_EXT_ACC_INTERFACE_PROC_NUMBER	(0x01)
// Alternate Setting 1 - Active seesion
static struct usb_interface_descriptor iap2_ext_acc_interface_alt1_desc = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bNumEndpoints = 2,
	.bAlternateSetting = 1,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = USB_MFI_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol = IAP2_EXT_ACC_INTERFACE_PROC_NUMBER,
};

// Alternate Setting 0 - Zero Bandwidth
static struct usb_interface_descriptor iap2_ext_acc_interface_alt0_desc = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bNumEndpoints = 0,
	.bAlternateSetting = 0,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = USB_MFI_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol = IAP2_EXT_ACC_INTERFACE_PROC_NUMBER,
};

static struct usb_endpoint_descriptor iap2_ext_acc_highspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor iap2_ext_acc_highspeed_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor iap2_ext_acc_fullspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor iap2_ext_acc_fullspeed_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_iap2_ext_acc_descs[] = {
	(struct usb_descriptor_header *)&iap2_ext_acc_interface_alt0_desc,
	(struct usb_descriptor_header *)&iap2_ext_acc_interface_alt1_desc,
	(struct usb_descriptor_header *)&iap2_ext_acc_fullspeed_in_desc,
	(struct usb_descriptor_header *)&iap2_ext_acc_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_iap2_ext_acc_descs[] = {
	(struct usb_descriptor_header *)&iap2_ext_acc_interface_alt0_desc,
	(struct usb_descriptor_header *)&iap2_ext_acc_interface_alt1_desc,
	(struct usb_descriptor_header *)&iap2_ext_acc_highspeed_in_desc,
	(struct usb_descriptor_header *)&iap2_ext_acc_highspeed_out_desc,
	NULL,
};

static struct usb_string iap2_ext_acc_string_defs[] = {
	[IAP2_EXT_ACC_INTF_ALT0_STRING_INDEX].s = iap2_ext_acc_protocol_name0,
	{},			/* end of list */
};

static struct usb_gadget_strings iap2_ext_acc_string_table = {
	.language = 0x0409,	/* en-US */
	.strings = iap2_ext_acc_string_defs,
};

static struct usb_gadget_strings *iap2_ext_acc_strings[] = {
	&iap2_ext_acc_string_table,
	NULL,
};

struct iap2_ext_acc_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct iap2_ext_acc_dev *dev;
};

/* temporary variable used between open() and gadget_bind() */
static struct iap2_ext_acc_dev *iap2_ext_acc_dev_tmp;

/* Convert usb function to ea device*/
static inline struct iap2_ext_acc_dev
*usb_func_to_iap2_ext_acc_dev(struct usb_function *f)
{
	return container_of(f, struct iap2_ext_acc_dev, function);
}

#if 1
#define IAP2_STRING_ATTR(field, buffer) \
static ssize_t \
field ## _show(struct device *dev, struct device_attribute *attr, \
		char *buf) \
{ \
	return sprintf(buf, "%s", buffer); \
} \
static ssize_t \
field ## _store(struct device *dev, struct device_attribute *attr, \
		const char *buf, size_t size) \
{ \
	if (size >= sizeof(buffer)) \
		return -EINVAL; \
	return strlcpy(buffer, buf, sizeof(buffer)); \
} \
static DEVICE_ATTR(field, 0644, field ## _show, field ## _store)

IAP2_STRING_ATTR(protocol_name0, iap2_ext_acc_protocol_name0);

static struct device_attribute *iap2_ext_acc_attrs[] = {
	&dev_attr_protocol_name0,
	NULL
};
#endif
/* Allocate usb_request */
static struct usb_request *iap2_ext_acc_request_new(struct usb_ep *ep,
						    int32_t buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (req == NULL) {
		return NULL;
	}

	/* now allocate buffers for the requests */
	req->buf = kmalloc((size_t)buffer_size, (uint32_t)GFP_KERNEL);
	if (req->buf == NULL) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

/* Free usb_request */
static void iap2_ext_acc_request_free(struct usb_request *req,
				      struct usb_ep *ep)
{
	if (req != NULL) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/* Add a request to the tail of a idle list from the head of a busy list*/
static void iap2_ext_acc_req_put(struct iap2_ext_acc_dev *dev,
				 struct list_head *head,
				 struct usb_request *req)
{
	ulong flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* Add a request to the tail of a busy list from the head of a idle list*/
static struct usb_request *iap2_ext_acc_req_get(struct iap2_ext_acc_dev *dev,
						struct list_head *head)
{
	ulong flags;
	struct usb_request *req;

	if (head == NULL) {
		return NULL;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head) != 0) {
		req = NULL;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

/* Change EA Gadget state to DISONNECTED */
static void iap2_ext_acc_set_disconnected(struct iap2_ext_acc_dev *dev)
{
	dev->state = IAP2_EXT_ACC_DISCONNECTED;
	EA_DBG("## %s %d\n", __func__, dev->state);
}

/* Change EA Gadget state to CONNECTED */
static void iap2_ext_acc_set_connected(struct iap2_ext_acc_dev *dev)
{
	dev->state = IAP2_EXT_ACC_CONNECTED;
	dev->alt = 0;
	EA_DBG("## %s %d\n", __func__, dev->state);

	schedule_delayed_work(&dev->alt_change_work, 0);
}

/* Change EA Gadget state to ONLINE */
static void iap2_ext_acc_set_online(struct iap2_ext_acc_dev *dev)
{
	dev->state = IAP2_EXT_ACC_ONLINE;
	dev->alt = 1;
	EA_DBG("## %s %d\n", __func__, dev->state);

	schedule_delayed_work(&dev->alt_change_work, 0);
}

/* Callback for completion of IN endpoint */
static void iap2_ext_acc_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct iap2_ext_acc_dev *dev;

	if (req == NULL) {
		return;
	}

	dev = req->context;

	EA_DBG("%s : req->status[%d]\n", __func__, req->status);
	if (dev == NULL) {
		EA_DBG("%s : iap2_ext_acc_dev is NULL!!\n", __func__);
		return;
	}

	if (req->status != 0) {
		iap2_ext_acc_set_disconnected(dev);
	}

	iap2_ext_acc_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

/* Callback for completion of OUT endpoint */
static void iap2_ext_acc_complete_out(struct usb_ep *ep,
				      struct usb_request *req)
{
	struct iap2_ext_acc_dev *dev;

	if (req == NULL) {
		return;
	}

	dev = req->context;

	EA_DBG("%s : req->status[%d]\n", __func__, req->status);
	if (dev == NULL) {
		EA_DBG("%s : iap2_ext_acc_dev is NULL!!\n", __func__);
		return;
	}

	if (req->status != 0) {
		iap2_ext_acc_set_disconnected(dev);
	}

	dev->rx_done = 1;

	wake_up(&dev->read_wq);
}

/* Congfig endpoints and allocate usb_requests */
static int32_t __init create_iap2_ext_acc_bulk_endpoints(struct iap2_ext_acc_dev
		*dev,
		struct
		usb_endpoint_descriptor
		*in_desc,
		struct
		usb_endpoint_descriptor
		*out_desc
		/*struct usb_endpoint_descriptor *intr_desc */
		)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int32_t i;

	EA_DBG("create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (ep == NULL) {
		pr_info("[INFO][USB] usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}

	EA_DBG("usb_ep_autoconfig for ep_in got %s\n", ep->name);

	ep->driver_data = dev;	/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (ep == NULL) {
		pr_info("[INFO][USB] usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}

	EA_DBG("usb_ep_autoconfig for ep_out got %s\n", ep->name);

	ep->driver_data = dev;	/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	for (i = 0; i < IAP2_EXT_ACC_TX_REQ_MAX; i++) {
		req =
		    iap2_ext_acc_request_new(dev->ep_in,
					     IAP2_EXT_ACC_BULK_BUFFER_SIZE);
		if (req == NULL) {
			goto fail;
		}
		req->complete = &iap2_ext_acc_complete_in;
		iap2_ext_acc_req_put(dev, &dev->tx_idle, req);
	}
	for (i = 0; i < IAP2_EXT_ACC_RX_REQ_MAX; i++) {
		req =
		    iap2_ext_acc_request_new(dev->ep_out,
					     IAP2_EXT_ACC_BULK_BUFFER_SIZE);
		if (req == NULL) {
			goto fail;
		}
		req->complete = &iap2_ext_acc_complete_out;
		dev->rx_req[i] = req;
	}

	return 0;

fail:
	pr_err
	    ("[ERROR][USB] iap2_ext_acc_bind() could not allocate requests\n");
	while ((req = iap2_ext_acc_req_get(dev, &dev->tx_idle)) != 0) {
		iap2_ext_acc_request_free(req, dev->ep_in);
	}

	for (i = 0; i < IAP2_EXT_ACC_RX_REQ_MAX; i++) {
		iap2_ext_acc_request_free(dev->rx_req[i], dev->ep_out);
	}

	return -1;
}

/* Read the data from Host, wait the online state and enqueue a request*/
static ssize_t iap2_ext_acc_read(struct file *fp, char __user *buf,
				 size_t count, loff_t *pos)
{
	struct iap2_ext_acc_dev *dev;
	struct usb_request *req;
	int32_t r;
	int32_t xfer;
	int32_t ret = 0;

	if (fp == NULL) {
		return -ENOENT;
	}

	dev = fp->private_data;

	EA_DBG("%s(%d)\n", __func__, count);

	if (dev->state != IAP2_EXT_ACC_ONLINE) {
		EA_DBG("error! gadget connection state %d\n", dev->state);
		return -ENODEV;
	}

	if (count > IAP2_EXT_ACC_BULK_BUFFER_SIZE) {
		count = IAP2_EXT_ACC_BULK_BUFFER_SIZE;
	}

	/* we will block until we're online */
	EA_DBG("Wait until state is online\n");
	ret =
	    wait_event_interruptible(dev->read_wq,
				     (dev->state == IAP2_EXT_ACC_ONLINE));
	if (ret < 0) {
		r = ret;
		EA_DBG("Failed to wait for online state\n");
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req[0];

	if (req == NULL) {
		return -ENXIO;
	}

	req->length = (unsigned)count;
	req->context = dev;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_KERNEL);
	if (ret < 0) {
		r = -EIO;
		EA_DBG("Failed to queue a request (req: 0x%p)\n", req);
		goto done;
	} else {
		EA_DBG("queue a rx request (req: 0x%p length: %d)\n", req,
		       req->length);
	}

	/* wait for a request to complete */
	EA_DBG("Wait for a request to complete\n");
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		r = ret;
		pr_debug("Failed to wait completion (req: 0x%p r: %d rx_done: %d state: %d)\n",
		     req, r, dev->rx_done, dev->state);
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (dev->state == IAP2_EXT_ACC_ONLINE) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0U) {
			pr_debug("Got a zero length packet (%d), requeue a request\n",
			     req->actual);
			goto requeue_req;
		}

		EA_DBG("rx %p %d\n", req, req->actual);

#ifdef VERBOSE_DEBUG
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1,
			       req->buf, req->actual, 0);
#endif

		xfer = (req->actual < count) ? (int)req->actual : (int)count;
		r = xfer;
		if (copy_to_user(buf, req->buf, (ulong)xfer) != 0) {
			r = -EFAULT;
		} else {
			EA_DBG("read ok.\n");
		}
	}

done:
	EA_DBG("%s returning %d\n", __func__, r);
	return r;
}

/* Write the data, enqueue the usb_request and wait for completion of tx*/
static ssize_t iap2_ext_acc_write(struct file *fp, const char __user *buf,
				  size_t count, loff_t *pos)
{
	struct iap2_ext_acc_dev *dev;
	struct usb_request *req = NULL;
	int32_t r;
	int32_t xfer;
	int32_t ret;

	if (fp == NULL) {
		return -ENOENT;
	}

	dev = fp->private_data;

	EA_DBG("%s(%d)\n", __func__, count);

#ifdef VERBOSE_DEBUG
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1, buf, count,
		       0);
#endif

	if (dev->state != IAP2_EXT_ACC_ONLINE) {
		EA_DBG("error0! gadget connection state %d\n", dev->state);
		return -ENODEV;
	}

	while (count > 0U) {
		/* get an idle tx request to use */
		EA_DBG("Wait for getting an idle tx request\n");
		req = NULL;
		ret = wait_event_interruptible(dev->write_wq,
				((req =
				  iap2_ext_acc_req_get(dev,
					  &dev->tx_idle))
				 || (dev->state !=
					 IAP2_EXT_ACC_ONLINE)));
		if (req == NULL) {
			r = ret;
			pr_debug("Failed to wait completion (req: 0x%p r: %d state: %d)\n",
			     req, r, dev->state);
			break;
		}

		if (count > IAP2_EXT_ACC_BULK_BUFFER_SIZE) {
			xfer = IAP2_EXT_ACC_BULK_BUFFER_SIZE;
		} else {
			xfer = (int32_t)count;
		}

		if (copy_from_user(req->buf, buf, (ulong)xfer) != 0) {
			r = -EFAULT;
			break;
		}

		req->length = (unsigned)xfer;
		req->context = dev;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			EA_DBG("Failed to queue a request (req: 0x%p)\n", req);
			r = -EIO;
			break;
		}

		buf += xfer;
		count -= (size_t)xfer;

		/* zero this so we don't try to free it on error exit */
		req = NULL;
	}

	if (req != NULL) {
		iap2_ext_acc_req_put(dev, &dev->tx_idle, req);
	}

	EA_DBG("%s returning %d\n", __func__, r);
	return r;
}

static int32_t iap2_ext_acc_open(struct inode *ip, struct file *fp)
{
	if (fp == NULL) {
		return -ENOENT;
	}

	pr_info("[INFO][USB] %s\n", __func__);

	if (atomic_xchg(&iap2_ext_acc_dev_tmp->open_excl, 1)) {
		//iap_release(ip,fp);
		return -EBUSY;
	}

	fp->private_data = iap2_ext_acc_dev_tmp;
	EA_DBG("iap2_ext_acc gadget state: %d\n", iap2_ext_acc_dev_tmp->state);

	return 0;
}

static int32_t iap2_ext_acc_release(struct inode *ip, struct file *fp)
{
	struct iap2_ext_acc_dev *dev;

	if (fp == NULL) {
		return -ENOENT;
	}

	dev = fp->private_data;

	pr_info("[INFO][USB] %s\n", __func__);
	EA_DBG("iap2_ext_acc gadget state: %d\n", iap2_ext_acc_dev_tmp->state);

	WARN_ON(!atomic_xchg(&dev->open_excl, 0));
	return 0;
}

/* file operations for /dev/usb_accessory */
static const struct file_operations iap2_ext_acc_fops = {
	.owner = THIS_MODULE,
	.read = iap2_ext_acc_read,
	.write = iap2_ext_acc_write,
	.unlocked_ioctl = NULL,	//iap2_ext_acc_ioctl,
	.open = iap2_ext_acc_open,
	.release = iap2_ext_acc_release,
};

static struct miscdevice iap2_ext_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "iap2_ext_acc",
	.fops = &iap2_ext_acc_fops,
};

/* Handle control requests */
int32_t iap2_ext_acc_ctrlrequest(struct usb_composite_dev *cdev,
			     const struct usb_ctrlrequest *ctrl)
{
#if 0
	struct iap2_ext_acc_dev *dev = iap2_ext_acc_dev_tmp;
#endif
	int32_t value = -EOPNOTSUPP;
	u16 w_index;
	u16 w_value;
	u16 w_length;

	if (ctrl == NULL) {
		return -ENXIO;
	}

	w_index = le16_to_cpu(ctrl->wIndex);
	w_value = le16_to_cpu(ctrl->wValue);
	w_length = le16_to_cpu(ctrl->wLength);

	EA_DBG("%s %02x.%02x v%04x i%04x l%u\n", __func__, b_requestType,
	       b_request, w_value, w_index, w_length);
	// TODO: Need to implement
#if 0
	if (value >= 0) {
		cdev->req->zero = (unsigned)0;
		cdev->req->length = (unsigned)value;
		cdev->req->context = dev;
		value = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
	}

	if (value == -EOPNOTSUPP)
#endif
		pr_debug("unknown class-specific control req %02x.%02x v%04x i%04x l%u\n",
		     ctrl->bRequestType, ctrl->bRequest, w_value, w_index,
		     w_length);

	return value;
}

/* Bind the function, alloc interface number and config endpoints*/
static int32_t iap2_ext_acc_function_bind(struct usb_configuration *c,
				      struct usb_function *f)
{
	struct usb_composite_dev *cdev;
	struct iap2_ext_acc_dev *dev = usb_func_to_iap2_ext_acc_dev(f);
	struct usb_string *us;
	int32_t id;
	int32_t ret;

	if (c == NULL) {
		return -ENXIO;
	}

	cdev = c->cdev;

	if (dev == NULL) {
		return -ENODEV;
	}

	dev->cdev = cdev;

	EA_DBG("%s dev : %p\n", __func__, dev);

	/* allocate a string ID for our interface */
	us = usb_gstrings_attach(cdev, iap2_ext_acc_strings,
				 ARRAY_SIZE(iap2_ext_acc_string_defs));
	if (IS_ERR(us)) {
		pr_info("[INFO][USB] [%s:%d] Failed to allocate string id for ALT0\n",
		     __func__, __LINE__);
		return PTR_ERR(us);
	}
	iap2_ext_acc_interface_alt0_desc.iInterface =
	    us[IAP2_EXT_ACC_INTF_ALT0_STRING_INDEX].id;
	iap2_ext_acc_interface_alt1_desc.iInterface =
	    us[IAP2_EXT_ACC_INTF_ALT0_STRING_INDEX].id;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0) {
		return id;
	}
	iap2_ext_acc_interface_alt0_desc.bInterfaceNumber = (unsigned char)id;
	iap2_ext_acc_interface_alt1_desc.bInterfaceNumber = (unsigned char)id;
	dev->intf = (unsigned char)id;

	/* allocate endpoints */
	ret =
	    create_iap2_ext_acc_bulk_endpoints(dev,
					       &iap2_ext_acc_fullspeed_in_desc,
					       &iap2_ext_acc_fullspeed_out_desc
					       /*, &iap_intr_desc */);
	if (ret != 0) {
		return ret;
	}

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget) != 0) {
		iap2_ext_acc_highspeed_in_desc.bEndpointAddress =
		    iap2_ext_acc_fullspeed_in_desc.bEndpointAddress;
		iap2_ext_acc_highspeed_out_desc.bEndpointAddress =
		    iap2_ext_acc_fullspeed_out_desc.bEndpointAddress;
	}

	EA_DBG("iAP2 Ext. Acc. %s speed %s: IN/%s, OUT/%s, INTF ID %d\n",
	       (gadget_is_dualspeed(c->cdev->gadget) != 0) ? "dual" : "full",
	       f->name, dev->ep_in->name, dev->ep_out->name, dev->intf);
	return 0;
}

/* Unbind the function, de-alocate usb_requests*/
static void iap2_ext_acc_function_unbind(struct usb_configuration *c,
					 struct usb_function *f)
{
	struct iap2_ext_acc_dev *dev = usb_func_to_iap2_ext_acc_dev(f);
	struct usb_request *req;
	int32_t i;

	if (dev == NULL) {
		return;
	}

	EA_DBG("%s\n", __func__);

	while ((req = iap2_ext_acc_req_get(dev, &dev->tx_idle)) != 0) {
		iap2_ext_acc_request_free(req, dev->ep_in);
	}

	for (i = 0; i < IAP2_EXT_ACC_RX_REQ_MAX; i++) {
		iap2_ext_acc_request_free(dev->rx_req[i], dev->ep_out);
	}
}

/* Notify start of EA gadget */
static void iap2_ext_acc_start_work(struct work_struct *work)
{
	struct iap2_ext_acc_dev *dev = NULL;
	struct miscdevice *misc_dev;
	char *envp[2] = { "EA=START", NULL };

	EA_DBG("%s\n", __func__);
	dev =
	    (struct iap2_ext_acc_dev *)container_of(work,
						    struct iap2_ext_acc_dev,
						    start_work.work);
	if (dev == NULL) {
		pr_info("[INFO][USB] [%s:%d] Error: iap2_ext_acc_dev is NULL!!\n",
		     __func__, __LINE__);
		return;
	}
	misc_dev = dev->misc_dev;

	if (misc_dev != NULL) {
		pr_info("[INFO][USB] %s: Send uevent [%s]\n",
				__func__, envp[0]);
		kobject_uevent_env(&iap2_ext_acc_device.this_device->kobj,
				   KOBJ_CHANGE, envp);
	} else {
		pr_info("[INFO][USB] [%s:%d] Error: misc_dev is NULL!!\n",
		     __func__, __LINE__);
	}
}

/* Notify alternate setting change */
static void iap2_ext_acc_alt_change(struct work_struct *work)
{
	struct iap2_ext_acc_dev *dev = NULL;
	struct miscdevice *misc_dev;
	char *alt0[2] = { "EA=ALT0", NULL };
	char *alt1[2] = { "EA=ALT1", NULL };

	EA_DBG("%s\n", __func__);

	dev =
	    (struct iap2_ext_acc_dev *)container_of(work,
						    struct iap2_ext_acc_dev,
						    alt_change_work.work);
	if (dev == NULL) {
		pr_info("[INFO][USB] [%s:%d] Error: iap2_ext_acc_dev is NULL!!\n",
				__func__, __LINE__);
		return;
	}

	misc_dev = dev->misc_dev;

	if (misc_dev != NULL) {
		if (dev->alt == 0U) {
			pr_info("[INFO][USB] %s: Send uevent [%s]\n", __func__,
					alt0[0]);
			kobject_uevent_env(&misc_dev->this_device->kobj,
					KOBJ_CHANGE, alt0);
		} else if (dev->alt == 1U) {
			pr_info("[INFO][USB] %s: Send uevent [%s]\n", __func__,
					alt1[0]);
			kobject_uevent_env(&misc_dev->this_device->kobj,
					KOBJ_CHANGE, alt1);
		} else {
			pr_debug("%s iAP2 EA does not have more than 3 alt settings.\n",
			     __func__);
		}
	} else {
		pr_info("[INFO][USB] [%s:%d] Error: misc_dev is NULL!!\n",
		     __func__, __LINE__);
	}

}

/* Set alternate setting */
static int32_t iap2_ext_acc_function_set_alt(struct usb_function *f,
					 uint32_t intf, uint32_t alt)
{
	struct iap2_ext_acc_dev *dev;
	struct usb_composite_dev *cdev = f->config->cdev;
	int32_t ret;

	if (f == NULL) {
		return -ENXIO;
	}

	dev = usb_func_to_iap2_ext_acc_dev(f);

	ret = 0;

	pr_info("[INFO][USB] %s intf: %d alt: %d\n", __func__, intf, alt);

	if (intf != dev->intf) {
		EA_DBG("%s : interface number is %d, not %d\n",
		       __func__, dev->intf, intf);
		return -EINVAL;
	}

	if (alt > 1U) {
		EA_DBG
		    ("%s : Interface does not have more than 3 alt settings.\n",
		     __func__);
		return -EINVAL;
	}

	if (alt == 0U) {
		// Zero bandwidth
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->ep_in);

		dev->rx_done = 1;	// dead lock
		iap2_ext_acc_set_connected(dev);

		wake_up(&dev->read_wq);
		wake_up(&dev->write_wq);
	} else if (alt == 1U) {
		// BULK IN/OUT
		// ep in
		ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
		if (ret != 0) {
			return ret;
		}

		ret = usb_ep_enable(dev->ep_in);
		if (ret != 0) {
			return ret;
		}

		// ep out
		ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
		if (ret != 0) {
			return ret;
		}

		ret = usb_ep_enable(dev->ep_out);
		if (ret != 0) {
			usb_ep_disable(dev->ep_in);
			return ret;
		}
		iap2_ext_acc_set_online(dev);

		/* readers may be blocked waiting for us to go online */
		wake_up(&dev->read_wq);
	} else {
		/* Nothing to do */
	}

	return ret;
}

/* Get current alternate setting*/
static int32_t iap2_ext_acc_function_get_alt(struct usb_function *f,
		uint32_t intf)
{
	struct iap2_ext_acc_dev *dev = usb_func_to_iap2_ext_acc_dev(f);

	if (dev == NULL) {
		return -ENODEV;
	}

	if (dev->state == IAP2_EXT_ACC_DISCONNECTED) {
		EA_DBG("%s : Gadget is in disconnected state\n", __func__);
		return -ENODEV;
	}

	if (intf != dev->intf) {
		EA_DBG("%s : interface number is %d, not %d\n",
		       __func__, dev->intf, intf);
		return -EINVAL;
	}

	return (int)dev->alt;
}

/* Disalbe the function */
static void iap2_ext_acc_function_disable(struct usb_function *f)
{
	struct iap2_ext_acc_dev *dev = usb_func_to_iap2_ext_acc_dev(f);

	if (dev == NULL) {
		return;
	}

	// disable all endpoints
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	dev->rx_done = 1;	// dead lock
	iap2_ext_acc_set_disconnected(dev);	// set disconnect state

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);
}

/* Set usb configuration*/
int32_t iap2_ext_acc_bind_config(struct usb_configuration *c)
{
	struct iap2_ext_acc_dev *dev = iap2_ext_acc_dev_tmp;
	int32_t ret;

	if (c == NULL) {
		return -ENXIO;
	}

	EA_DBG("%s\n", __func__);

#if 0
	/* allocate a string ID for our interface */
	if (iap2_ext_acc_string_defs[IAP2_EXT_ACC_INTF_ALT0_STRING_INDEX].id ==
	    0) {
		ret = usb_string_id(c->cdev);
		if (ret < 0) {
			pr_info("[INFO][USB] [%s:%d] Failed to allocate string id for ALT0\n",
			     __func__, __LINE__);
			return ret;
		}
		iap2_ext_acc_string_defs[
			IAP2_EXT_ACC_INTF_ALT0_STRING_INDEX].id = ret;
		iap2_ext_acc_interface_alt0_desc.iInterface = ret;
	}

	if (iap2_ext_acc_string_defs[IAP2_EXT_ACC_INTF_ALT1_STRING_INDEX].id ==
	    0) {
		ret = usb_string_id(c->cdev);
		if (ret < 0) {
			pr_info("[INFO][USB] [%s:%d] Failed to allocate string id for ALT1\n",
			     __func__, __LINE__);
			return ret;
		}
		iap2_ext_acc_string_defs[
			IAP2_EXT_ACC_INTF_ALT1_STRING_INDEX].id =
			ret;
		iap2_ext_acc_interface_alt0_desc.iInterface = ret;
	}
#endif

	dev->cdev = c->cdev;
	dev->function.name = "ea";
	dev->function.fs_descriptors = fs_iap2_ext_acc_descs;
	dev->function.hs_descriptors = hs_iap2_ext_acc_descs;
	dev->function.bind = &iap2_ext_acc_function_bind;
	dev->function.unbind = &iap2_ext_acc_function_unbind;
	dev->function.set_alt = &iap2_ext_acc_function_set_alt;
	dev->function.get_alt = &iap2_ext_acc_function_get_alt;
	dev->function.disable = &iap2_ext_acc_function_disable;

	ret = usb_add_function(c, &dev->function);
	return ret;
}

/* Initial setup for Ext. Acc. gadget */
static int32_t setup_iap2_ext_acc(struct iap2_ext_acc_instance *fi_iap2_ext_acc)
{
	struct iap2_ext_acc_dev *dev;
	struct device_attribute **attrs = iap2_ext_acc_attrs;
	struct device_attribute *attr;
	int32_t ret;

	EA_DBG("%s\n", __func__);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		return -ENOMEM;
	}

	if (fi_iap2_ext_acc != NULL) {
		fi_iap2_ext_acc->dev = dev;
	}

	memset(dev, 0x0, sizeof(struct iap2_ext_acc_dev));

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->tx_busy);

	INIT_DELAYED_WORK(&dev->start_work, iap2_ext_acc_start_work);
	INIT_DELAYED_WORK(&dev->alt_change_work, iap2_ext_acc_alt_change);

	// Init. a gadget state
	iap2_ext_acc_set_disconnected(dev);

	iap2_ext_acc_dev_tmp = dev;

	ret = misc_register(&iap2_ext_acc_device);
	if (ret != 0) {
		goto err;
	}

	dev->misc_dev = &iap2_ext_acc_device;
	if (dev->misc_dev == NULL) {
		return -ENODEV;
	}

	/* Set default protocol names */
	strncpy(iap2_ext_acc_protocol_name0, IAP2_EXT_ACC_DEF_INTF_STRING0,
		sizeof(iap2_ext_acc_protocol_name0) - 1UL);

	/* Create sysfs for misc device */
	while ((attr = *attrs++) != 0) {
		ret = device_create_file(dev->misc_dev->this_device, attr);
		if (ret != 0) {
			misc_deregister(dev->misc_dev);
			goto err;
		}
	}
	return 0;

err:
	kfree(dev);
	pr_err("[ERROR][USB] iAP2 Ext. Acc. Native Transport gadget driver failed to initialize\n");
	return ret;
}

int32_t iap2_ext_acc_setup(void)
{
	return setup_iap2_ext_acc(NULL);
}

static int32_t iap2_ext_acc_setup_configfs(struct iap2_ext_acc_instance
				       *fi_iap2_ext_acc)
{
	return setup_iap2_ext_acc(fi_iap2_ext_acc);
}

#if 0
static void iap2_ext_acc_disconnect(void)
{
	EA_DBG("%s\n", __func__);
}
#endif

void iap2_ext_acc_cleanup(void)
{
	EA_DBG("%s\n", __func__);
	misc_deregister(&iap2_ext_acc_device);
	kfree(iap2_ext_acc_dev_tmp);
	iap2_ext_acc_dev_tmp = NULL;
}

static struct iap2_ext_acc_instance *to_iap2_ext_acc_instance(struct config_item
							      *item)
{
	return container_of(to_config_group(item), struct iap2_ext_acc_instance,
			    func_inst.group);
}

static void iap2_ext_acc_attr_release(struct config_item *item)
{
	struct iap2_ext_acc_instance *fi_iap2_ext_acc =
	    to_iap2_ext_acc_instance(item);
	usb_put_function_instance(&fi_iap2_ext_acc->func_inst);
}

static struct configfs_item_operations iap2_ext_acc_item_ops = {
	.release = iap2_ext_acc_attr_release,
};

static struct config_item_type iap2_ext_acc_func_type = {
	.ct_item_ops = &iap2_ext_acc_item_ops,
	.ct_owner = THIS_MODULE,
};

static struct iap2_ext_acc_instance *to_fi_iap2_ext_acc(struct
							usb_function_instance
							*fi)
{
	return container_of(fi, struct iap2_ext_acc_instance, func_inst);
}

static int32_t iap2_ext_acc_set_inst_name(struct usb_function_instance *fi,
				      const char *name)
{
	struct iap2_ext_acc_instance *fi_iap2_ext_acc;
	char *ptr;
	int32_t name_len;

	name_len = (int32_t)strnlen(name, (size_t)IAP2_EXT_ACC_MAX_INST_NAME_LEN) + 1UL;
	if (name_len > IAP2_EXT_ACC_MAX_INST_NAME_LEN) {
		return -ENAMETOOLONG;
	}

	ptr = kstrndup(name, (size_t)name_len, (uint32_t)GFP_KERNEL);
	if (ptr == NULL) {
		return -ENOMEM;
	}

	fi_iap2_ext_acc = to_fi_iap2_ext_acc(fi);
	if (fi_iap2_ext_acc == NULL) {
		return -ENXIO;
	}

	fi_iap2_ext_acc->name = ptr;

	return 0;
}

static void iap2_ext_acc_free_inst(struct usb_function_instance *fi)
{
	struct iap2_ext_acc_instance *fi_iap2_ext_acc;

	fi_iap2_ext_acc = to_fi_iap2_ext_acc(fi);

	if (fi_iap2_ext_acc == NULL) {
		return;
	}

	kfree(fi_iap2_ext_acc->name);
	iap2_ext_acc_cleanup();
	kfree(fi_iap2_ext_acc);
}

static struct usb_function_instance *iap2_ext_acc_alloc_inst(void)
{
	struct iap2_ext_acc_instance *fi_iap2_ext_acc;
	int32_t ret = 0;

	fi_iap2_ext_acc = kzalloc(sizeof(*fi_iap2_ext_acc), GFP_KERNEL);
	if (fi_iap2_ext_acc == NULL) {
		return ERR_PTR(-ENOMEM);
	}

	fi_iap2_ext_acc->func_inst.set_inst_name = &iap2_ext_acc_set_inst_name;
	fi_iap2_ext_acc->func_inst.free_func_inst = &iap2_ext_acc_free_inst;

	ret = iap2_ext_acc_setup_configfs(fi_iap2_ext_acc);
	if (ret != 0) {
		kfree(fi_iap2_ext_acc);
		pr_err("[ERROR][USB] Error setting iAP2 Ext Acc\n");
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_iap2_ext_acc->func_inst.group,
				    "", &iap2_ext_acc_func_type);

	return &fi_iap2_ext_acc->func_inst;
}

static int32_t iap2_ext_acc_ctrlreq_configfs(struct usb_function *f,
					 const struct usb_ctrlrequest *ctrl)
{
	if (f != NULL) {
		return iap2_ext_acc_ctrlrequest(f->config->cdev, ctrl);
	} else {
		return -ENXIO;
	}
}

static void iap2_ext_acc_free(struct usb_function *f)
{
	/*NO-OP: no function specific resource
	 * allocation in iap2_ext_acc_alloc
	 */
}

struct usb_function *function_alloc_iap2_ext_acc(struct usb_function_instance
						 *fi)
{
	struct iap2_ext_acc_instance *fi_iap2_ext_acc = to_fi_iap2_ext_acc(fi);
	struct iap2_ext_acc_dev *dev;

	if (fi_iap2_ext_acc == NULL) {
		return NULL;
	}

	if (fi_iap2_ext_acc->dev == NULL) {
		pr_err("[ERROR][USB] Error: Create iap2_ext_acc function before linking iap2_ext_acc function with a gadget configuration\n");
		pr_err("[ERROR][USB] \t1: Delete existing iap2_ext_acc function if any\n");
		pr_err("[ERROR][USB] \t2: Create iap2_ext_acc function\n");
		pr_err("[ERROR][USB] \t3: Create and symlink iap2_ext_acc function with a gadget configuration\n");
		return NULL;
	}

	dev = fi_iap2_ext_acc->dev;
	dev->function.name = "ea";
	dev->function.strings = iap2_ext_acc_strings;
	dev->function.fs_descriptors = fs_iap2_ext_acc_descs;
	dev->function.hs_descriptors = hs_iap2_ext_acc_descs;
	dev->function.bind = &iap2_ext_acc_function_bind;
	dev->function.unbind = &iap2_ext_acc_function_unbind;
	dev->function.set_alt = &iap2_ext_acc_function_set_alt;
	dev->function.get_alt = &iap2_ext_acc_function_get_alt;
	dev->function.disable = &iap2_ext_acc_function_disable;
	dev->function.setup = &iap2_ext_acc_ctrlreq_configfs;
	dev->function.free_func = &iap2_ext_acc_free;

	fi->f = &dev->function;

	return &dev->function;
}

static struct usb_function *iap2_ext_acc_alloc(struct usb_function_instance *fi)
{
	return function_alloc_iap2_ext_acc(fi);
}

DECLARE_USB_FUNCTION_INIT(iap2_ext_acc, iap2_ext_acc_alloc_inst,
			  iap2_ext_acc_alloc);
MODULE_LICENSE("GPL");
