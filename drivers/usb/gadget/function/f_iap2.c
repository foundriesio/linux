/*
 * Gadget Function Driver for Apple IAP2 
 *
 * Copyright (C) 2013 Telechips, Inc.
 * Author: jmlee <jmlee@telechips.com>
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

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
#include "f_iap2.h"

#if defined(CONFIG_CHIP_TCC8935S) || defined(CONFIG_ARCH_TCC892X)
#if defined(CONFIG_TCC_DWC_HS_ELECT_TST)
#undef DMA_MODE
#else
//#define DMA_MODE
#endif
#endif

//#define IAP_USE_INTR

#define IAP_BULK_BUFFER_SIZE    512
#define IAP_INTR_BUFFER_SIZE    512
#define IAP_MAX_INST_NAME_LEN   40

#define PROTOCOL_VERSION    2

/* String IDs */
#define INTERFACE_STRING_INDEX	0
#define INTERFACE_STRING_SERIAL_IDX		1
#define USB_MFI_SUBCLASS_VENDOR_SPEC 0xf0

/* number of tx and rx requests to allocate */
#define TX_REQ_MAX 4
#define RX_REQ_MAX 2
#define INTR_REQ_MAX 5

#define IAP2_SEND_EVT 0
#define IAP2_SEND_ZLP 1
#define IAP2_SEND_RELEASE 2

struct iap2_event {
   /* size of the event */
   size_t      length;
   /* event data to send */
   void        *data;
};

struct iap_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
#ifdef IAP_USE_INTR
	struct usb_ep *ep_intr;
#endif

	int state;
	/* set to 1 when we connect */
	int online:1;
	/* Set to 1 when we disconnect.
	 * Not cleared until our file is closed.
	 */
	int disconnected:1;

	/* for acc_complete_set_string */
	int string_index;

	/* set to 1 if we have a pending start request */
	int start_requested;

	int audio_mode;

	/* synchronize access to our device file */
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
#ifdef IAP_USE_INTR
	struct list_head intr_idle;
	wait_queue_head_t intr_wq;
#endif
	struct usb_request *rx_req[RX_REQ_MAX];
	int rx_done;

	/* delayed work for handling iap2 start */
	struct delayed_work start_work;

};

static struct usb_interface_descriptor iap_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
#ifdef IAP_USE_INTR
	.bNumEndpoints          = 3,
#else
	.bNumEndpoints          = 2,
#endif
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = USB_MFI_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol     = 0,
};

static struct usb_endpoint_descriptor iap_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor iap_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor iap_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor iap_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

#ifdef IAP_USE_INTR
static struct usb_endpoint_descriptor iap_intr_desc = {
   .bLength                = USB_DT_ENDPOINT_SIZE,
   .bDescriptorType        = USB_DT_ENDPOINT,
   .bEndpointAddress       = USB_DIR_IN,
   .bmAttributes           = USB_ENDPOINT_XFER_INT,
   .wMaxPacketSize         = __constant_cpu_to_le16(IAP_INTR_BUFFER_SIZE),
   .bInterval              = 1,
};
#endif

static struct usb_descriptor_header *fs_iap_descs[] = {
	(struct usb_descriptor_header *) &iap_interface_desc,
	(struct usb_descriptor_header *) &iap_fullspeed_in_desc,
	(struct usb_descriptor_header *) &iap_fullspeed_out_desc,
#ifdef IAP_USE_INTR
	(struct usb_descriptor_header *) &iap_intr_desc,
#endif
	NULL,
};

static struct usb_descriptor_header *hs_iap_descs[] = {
	(struct usb_descriptor_header *) &iap_interface_desc,
	(struct usb_descriptor_header *) &iap_highspeed_in_desc,
	(struct usb_descriptor_header *) &iap_highspeed_out_desc,
#ifdef IAP_USE_INTR
	(struct usb_descriptor_header *) &iap_intr_desc,
#endif
	NULL,
};

static struct usb_string iap_string_defs[] = {
	[INTERFACE_STRING_INDEX].s	= "iAP Interface",
	{  },	/* end of list */
};

static struct usb_gadget_strings iap_string_table = {
	.language		= 0x0409,	/* en-US */
	.strings		= iap_string_defs,
};

static struct usb_gadget_strings *iap_strings[] = {
	&iap_string_table,
	NULL,
};

struct iap_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct iap_dev *dev;
};

/* temporary variable used between iap_open() and iap_gadget_bind() */
static struct iap_dev *_iap_dev;
static int iap_release(struct inode *ip, struct file *fp);

static inline struct iap_dev *func_to_iap(struct usb_function *f)
{
	return container_of(f, struct iap_dev, function);
}

static struct usb_request *iap_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
#ifdef DMA_MODE
	req->buf = dma_alloc_coherent(NULL, buffer_size, &req->dma, GFP_KERNEL | GFP_DMA);
#else
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
#endif		
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void iap_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
#ifdef DMA_MODE
		dma_free_coherent (NULL, IAP_BULK_BUFFER_SIZE, req->buf, req->dma);
#else
		kfree(req->buf);
#endif
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
static void iap_req_put(struct iap_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request
*iap_req_get(struct iap_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void iap_set_disconnected(struct iap_dev *dev)
{
	DBG(dev->cdev, "iap_set_disconnected\n");
	dev->online = 0;
	dev->disconnected = 1;
}

static void iap_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct iap_dev *dev = _iap_dev;

	if (req->status != 0) 
		iap_set_disconnected(dev);

	iap_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void iap_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct iap_dev *dev = _iap_dev;

	dev->rx_done = 1;
	if (req->status != 0)
		iap_set_disconnected(dev);

	wake_up(&dev->read_wq);
}

#ifdef IAP_USE_INTR
static void iap_complete_intr(struct usb_ep *ep, struct usb_request *req)
{
	struct iap_dev *dev = _iap_dev;

	if (req->status != 0)
		iap_set_disconnected(dev);

	iap_req_put(dev, &dev->intr_idle, req);

	wake_up(&dev->intr_wq);
}
#endif

static int __init create_iap_bulk_endpoints(struct iap_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc,
				struct usb_endpoint_descriptor *intr_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for iap ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for iap ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for iap ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for iap ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

#ifdef IAP_USE_INTR
	ep = usb_ep_autoconfig(cdev->gadget, intr_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for iap ep_intr failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for iap ep_intr got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_intr = ep;
#endif

	/* now allocate requests for our endpoints */
	for (i = 0; i < TX_REQ_MAX; i++) {
		req = iap_request_new(dev->ep_in, IAP_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = iap_complete_in;
		iap_req_put(dev, &dev->tx_idle, req);
	}
	for (i = 0; i < RX_REQ_MAX; i++) {
		req = iap_request_new(dev->ep_out, IAP_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = iap_complete_out;
		dev->rx_req[i] = req;
	}
#ifdef IAP_USE_INTR
	for (i = 0; i < INTR_REQ_MAX; i++) {
		req = iap_request_new(dev->ep_intr, IAP_INTR_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = iap_complete_intr;
		iap_req_put(dev, &dev->intr_idle, req);
	}
#endif

	return 0;

fail:
	printk(KERN_ERR "iap_bind() could not allocate requests\n");
	return -1;
}

static ssize_t iap_read(struct file *fp, char __user *buf,
	size_t count, loff_t *pos)
{
	struct iap_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int ret = 0;

	DBG(dev->cdev, "iap_read(%d)\n", count);

	if (dev->disconnected)
		return -ENODEV;

	if (count > IAP_BULK_BUFFER_SIZE)
		count = IAP_BULK_BUFFER_SIZE;

	/* we will block until we're online */
	DBG(dev->cdev, "iap_read: waiting for online state\n");
	ret = wait_event_interruptible(dev->read_wq, dev->online);
	if (ret < 0) {
		r = ret;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req[0];
	req->length = count;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_KERNEL);
	if (ret < 0) {
		r = -EIO;
		goto done;
	} else {
		DBG(dev->cdev, "rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}

	if (dev->online) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;
		//print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1, req->buf, req->actual, 0); 
		DBG(dev->cdev, "rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else
		r = -EIO;

done:
	DBG(dev->cdev, "iap_read returning %d\n", r);
	return r;
}

static ssize_t iap_write(struct file *fp, const char __user *buf,
	size_t count, loff_t *pos)
{
	struct iap_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	DBG(dev->cdev, "iap_write(%d)\n", count);

	//print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1, buf, count, 0); 

	if (!dev->online || dev->disconnected)
		return -ENODEV;


	while (count > 0) {

		if (!dev->online) {
			DBG(dev->cdev, "iap_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = iap_req_get(dev, &dev->tx_idle)) || !dev->online));
		if (!req) {
			r = ret;
			break;
		}

		if (count > IAP_BULK_BUFFER_SIZE)
			xfer = IAP_BULK_BUFFER_SIZE;
		else
			xfer = count;
		if (xfer && copy_from_user(req->buf, buf, xfer)) {
			r = -EFAULT;
			break;
		}

		//print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET, 16, 1, req->buf, xfer, 0); 

		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			DBG(dev->cdev, "iap_write: xfer error %d\n", ret);
			r = -EIO;
			break;
		}

		buf += xfer;
		count -= xfer;

		/* zero this so we don't try to free it on error exit */
		req = 0;
	}

	if (req)
		iap_req_put(dev, &dev->tx_idle, req);

	DBG(dev->cdev, "iap_write returning %d\n", r);
	return r;
}

#ifdef IAP_USE_INTR
static int iap_send_event(struct iap_dev *dev, struct iap2_event *event)
{
	struct usb_request *req= NULL;
	int ret;
	int length = event->length;

	DBG(dev->cdev, "iap_send_event(%d)\n", event->length);

	if (length < 0 || length > IAP_INTR_BUFFER_SIZE)
		return -EINVAL;
	if (!dev->online)
		return -ENODEV;

	ret = wait_event_interruptible_timeout(dev->intr_wq,
			(req = iap_req_get(dev, &dev->intr_idle)),
			msecs_to_jiffies(1000));

	if (!req)
	    return -ETIME;

	if (copy_from_user(req->buf, (void __user *)event->data, length)) {
		iap_req_put(dev, &dev->intr_idle, req);
		return -EFAULT;
	}

	req->length = length;
	ret = usb_ep_queue(dev->ep_intr, req, GFP_KERNEL);
	if (ret)
		iap_req_put(dev, &dev->intr_idle, req);

	return ret;
}


static int iap_send_zlp(struct iap_dev *dev)
{
	struct usb_request *req= NULL;
	int ret;
	int length = 0;

	DBG(dev->cdev, "iap_send_zlp\n");

	if (!dev->online)
		return -ENODEV;

	ret = wait_event_interruptible_timeout(dev->intr_wq,
			(req = iap_req_get(dev, &dev->intr_idle)),
			msecs_to_jiffies(1000));

	if (!req)
	    return -ETIME;

	req->length = length;
	ret = usb_ep_queue(dev->ep_intr, req, GFP_KERNEL);
	if (ret)
		iap_req_put(dev, &dev->intr_idle, req);

	return ret;
}
#endif


static long iap_ioctl(struct file *fp, unsigned code, unsigned long value)
{
#ifdef IAP_USE_INTR
	struct iap_dev *dev = fp->private_data;
	int ret = -EINVAL;

	DBG(dev->cdev, "iap_ioctl\n");

	switch (code) {
		case IAP2_SEND_EVT:
		{
			struct iap2_event  event;
			ret = iap_send_event(dev, &event);
			break;
		}
		case IAP2_SEND_ZLP:
		{
			ret = iap_send_zlp(dev);
			break;
		}
	}

#else
	int ret = 0;
#endif
	return ret;
}

static int iap_open(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "iap_open\n");

	if (atomic_xchg(&_iap_dev->open_excl, 1)) {
		//iap_release(ip,fp);
		return -EBUSY;
	}

	if(_iap_dev->online == 0) {
		atomic_xchg(&_iap_dev->open_excl, 0);
		return -EIO;
	}

	_iap_dev->disconnected = 0;
	fp->private_data = _iap_dev;

	return 0;
}

static int iap_release(struct inode *ip, struct file *fp)
{
	printk(KERN_INFO "iap_release\n");

	WARN_ON(!atomic_xchg(&_iap_dev->open_excl, 0));
	_iap_dev->disconnected = 0;

	return 0;
}

/* file operations for /dev/usb_accessory */
static const struct file_operations iap_fops = {
	.owner = THIS_MODULE,
	.read = iap_read,
	.write = iap_write,
	.unlocked_ioctl = iap_ioctl,
	.open = iap_open,
	.release = iap_release,
};

static struct miscdevice iap_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "iap2",
	.fops = &iap_fops,
};

int iap_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
#if 0
	struct iap_dev	*dev = _iap_dev;
	int offset;
	unsigned long flags;
#endif

	VDBG(cdev, "iap_ctrlrequest "
			"%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	if (ctrl->bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {

	} else if (ctrl->bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {

	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		cdev->req->zero = 0;
		cdev->req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (value < 0)
			printk("%s: response queue error\n", __func__);
	}

	if (value == -EOPNOTSUPP)
	{
		DBG(cdev,
			"unknown class-specific control req "
			"%02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			ctrl->wIndex, ctrl->wValue, ctrl->wLength
			);
	}
	return value;
}
EXPORT_SYMBOL_GPL(iap_ctrlrequest);

static int
iap_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct iap_dev	*dev = func_to_iap(f);
	struct usb_string	*us;
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "iap_function_bind dev: %p\n", dev);

	dev->start_requested = 0;

	/* allocate a string ID for our interface */
	us = usb_gstrings_attach(cdev, iap_strings,
				 ARRAY_SIZE(iap_string_defs));
	if (IS_ERR(us))
		return PTR_ERR(us);
	iap_interface_desc.iInterface = us[INTERFACE_STRING_INDEX].id;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	iap_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
#ifdef IAP_USE_INTR
	ret = create_iap_bulk_endpoints(dev, &iap_fullspeed_in_desc,
			&iap_fullspeed_out_desc, &iap_intr_desc);
#else
	ret = create_iap_bulk_endpoints(dev, &iap_fullspeed_in_desc,
			&iap_fullspeed_out_desc, NULL);
#endif
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		iap_highspeed_in_desc.bEndpointAddress =
			iap_fullspeed_in_desc.bEndpointAddress;
		iap_highspeed_out_desc.bEndpointAddress =
			iap_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "IAP2 %s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
iap_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct iap_dev	*dev = func_to_iap(f);
	struct usb_request *req;
	int i;

	DBG(dev->cdev, "iap_function_unbind\n");

	while ((req = iap_req_get(dev, &dev->tx_idle)))
		iap_request_free(req, dev->ep_in);
	for (i = 0; i < RX_REQ_MAX; i++)
		iap_request_free(dev->rx_req[i], dev->ep_out);
#ifdef IAP_USE_INTR
	while ((req = iap_req_get(dev, &dev->intr_idle)))
		iap_request_free(req, dev->ep_intr);
#endif
}

static void iap_start_work(struct work_struct *data)
{
	char *envp[2] = { "IAP2=START", NULL };

	printk(KERN_INFO "iap_start_work\n");

	kobject_uevent_env(&iap_device.this_device->kobj, KOBJ_CHANGE, envp);
}


static int iap_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct iap_dev	*dev = func_to_iap(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "iap_function_set_alt intf: %d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

#ifdef IAP_USE_INTR
	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_intr);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_intr);
	if (ret) {
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->ep_in);
		return ret;
	}
#endif
	dev->online = 1;

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void iap_function_disable(struct usb_function *f)
{
	struct iap_dev	*dev = func_to_iap(f);

	DBG(dev->cdev, "iap_function_disable\n");

	iap_set_disconnected(dev);
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);
#ifdef IAP_USE_INTR
	usb_ep_disable(dev->ep_intr);
#endif
	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(dev->cdev, "%s disabled\n", dev->function.name);
}

int iap_bind_config(struct usb_configuration *c)
{
	struct iap_dev *dev = _iap_dev;

	printk(KERN_INFO "iap_bind_config\n");

	dev->cdev = c->cdev;
	dev->function.name = "iap2";
	dev->function.fs_descriptors = fs_iap_descs;
	dev->function.hs_descriptors = hs_iap_descs;
	dev->function.bind = iap_function_bind;
	dev->function.unbind = iap_function_unbind;
	dev->function.set_alt = iap_function_set_alt;
	dev->function.disable = iap_function_disable;

	return usb_add_function(c, &dev->function);
}
EXPORT_SYMBOL_GPL(iap_bind_config);

static int __iap_setup(struct iap_instance *fi_iap)
{
	struct iap_dev *dev;
	int ret;

	printk(KERN_INFO "iap_setup\n");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	if(fi_iap != NULL)
		fi_iap->dev = dev;

	spin_lock_init(&dev->lock);
	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	atomic_set(&dev->open_excl, 0);
	INIT_LIST_HEAD(&dev->tx_idle);
#ifdef IAP_USE_INTR
	init_waitqueue_head(&dev->intr_wq);
	INIT_LIST_HEAD(&dev->intr_idle);
#endif
	INIT_DELAYED_WORK(&dev->start_work, iap_start_work);

	_iap_dev = dev;

	ret = misc_register(&iap_device);
	if (ret)
		goto err;

	return 0;

err:
	_iap_dev = NULL;
	kfree(dev);
	printk(KERN_ERR "iap gadget driver failed to initialize\n");
	return ret;
}

int iap_setup(void)
{
	return __iap_setup(NULL);
}
EXPORT_SYMBOL_GPL(iap_setup);

static int iap_setup_configfs(struct iap_instance *fi_iap)
{
	return __iap_setup(fi_iap);
}

void iap_cleanup(void)
{
	struct iap_dev *dev = _iap_dev;

	if (!dev)
		return;

	DBG(dev->cdev, "iap_cleanup\n");

	misc_deregister(&iap_device);
	_iap_dev = NULL;
	kfree(dev);
}
EXPORT_SYMBOL_GPL(iap_cleanup);

static struct iap_instance *to_iap_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct iap_instance,
		func_inst.group);
}

static void iap_attr_release(struct config_item *item)
{
	struct iap_instance *fi_iap = to_iap_instance(item);
	usb_put_function_instance(&fi_iap->func_inst);
}

static struct configfs_item_operations iap_item_ops = {
	.release        = iap_attr_release,
};

static struct config_item_type iap_func_type = {
	.ct_item_ops    = &iap_item_ops,
	.ct_owner       = THIS_MODULE,
};


static struct iap_instance *to_fi_iap(struct usb_function_instance *fi)
{
	return container_of(fi, struct iap_instance, func_inst);
}

static int iap_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct iap_instance *fi_iap;
	char *ptr;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > IAP_MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_iap = to_fi_iap(fi);
	fi_iap->name = ptr;

	return 0;
}

static void iap_free_inst(struct usb_function_instance *fi)
{
	struct iap_instance *fi_iap;

	fi_iap = to_fi_iap(fi);
	kfree(fi_iap->name);
	iap_cleanup();
	kfree(fi_iap);
}

static struct usb_function_instance *iap_alloc_inst(void)
{
	struct iap_instance *fi_iap;
	int ret = 0;

	fi_iap = kzalloc(sizeof(*fi_iap), GFP_KERNEL);
	if (!fi_iap)
		return ERR_PTR(-ENOMEM);
	fi_iap->func_inst.set_inst_name = iap_set_inst_name;
	fi_iap->func_inst.free_func_inst = iap_free_inst;

	ret = iap_setup_configfs(fi_iap);
	if (ret) {
		kfree(fi_iap);
		pr_err("Error setting iAP2\n");
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_iap->func_inst.group,
					"", &iap_func_type);

	return  &fi_iap->func_inst;
}

static int iap_ctrlreq_configfs(struct usb_function *f,
				const struct usb_ctrlrequest *ctrl)
{
	return iap_ctrlrequest(f->config->cdev, ctrl);
}

static void iap_free(struct usb_function *f)
{
	/*NO-OP: no function specific resource allocation in iap_alloc*/
}

struct usb_function *function_alloc_iap(struct usb_function_instance *fi)
{
	struct iap_instance *fi_iap = to_fi_iap(fi);
	struct iap_dev *dev;

	if (fi_iap->dev == NULL) {
		pr_err("Error: Create iap function before linking"
				" iap function with a gadget configuration\n");
		pr_err("\t1: Delete existing iap function if any\n");
		pr_err("\t2: Create iap function\n");
		pr_err("\t3: Create and symlink iap function"
				" with a gadget configuration\n");
		return NULL;
	}

	dev = fi_iap->dev;
	dev->function.name = "iap2";
	dev->function.strings = iap_strings;
	dev->function.fs_descriptors = fs_iap_descs;
	dev->function.hs_descriptors = hs_iap_descs;
	dev->function.bind = iap_function_bind;
	dev->function.unbind = iap_function_unbind;
	dev->function.set_alt = iap_function_set_alt;
	dev->function.disable = iap_function_disable;
	dev->function.setup = iap_ctrlreq_configfs;
	dev->function.free_func = iap_free;

	fi->f = &dev->function;

	return &dev->function;
}
EXPORT_SYMBOL_GPL(function_alloc_iap);

static struct usb_function *iap_alloc(struct usb_function_instance *fi)
{
	return function_alloc_iap(fi);
}

DECLARE_USB_FUNCTION_INIT(iap2, iap_alloc_inst, iap_alloc);
MODULE_LICENSE("GPL");


