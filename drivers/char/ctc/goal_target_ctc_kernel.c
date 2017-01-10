/** @file
 *
 * @brief Core to core module
 *
 * This module implements the core to core functionality between the M3 and A7 core
 * of the RZ-N.
 *
 * @copyright
 * (C) Copyright 2017 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pl320-ipc.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <linux/workqueue.h>

#include <asm/uaccess.h>
#include <asm/io.h>


/****************************************************************************/
/* Local defines */
/****************************************************************************/
#define CTC_KERNEL_VERSION_MAJOR 2              /**< major version of the kernel */
#define CTC_KERNEL_VERSION_MINOR 0              /**< minor version of the kernel */

#define CTC_CHANNEL_NUM 5                       /**< Number of channels */

#define CTC_DEVICE_NAME "ctc"                   /**< Name of sysfs device */

#define CTC_DEVICE_FILE "c2c_chan%u"            /**< Device file name in /dev */

#define CTC_FIFO_SIZE 16                        /**< Number of elements in fifo. kfifo requires a power of 2 */

#define CTC_WORK_QUEUE_NAME "CTC_FIFO_WQUEUE"   /**< Work queue */

/**< 16 pages of 64 bytes = 1024 Bytes write buffer per channel */
#define CTC_PAGE_COUNT_EXPONENT 4               /**< exponent of the number of pages */
#define CTC_PAGE_SIZE_EXPONENT 6                /**< exponent of the size of a page */

#define CTC_PAGE_COUNT (1 << CTC_PAGE_COUNT_EXPONENT)
#define CTC_PAGE_SIZE (1 << CTC_PAGE_SIZE_EXPONENT)

#define CTC_MEM_NAME "ctc_mem"                  /**< name of memory */
/**< size of shm for one direction */
#define CTC_MEM_SIZE (CTC_PAGE_COUNT * CTC_PAGE_SIZE * CTC_CHANNEL_NUM)
#define CTC_MEM_OFFSET_RX (CTC_MEM_SIZE)

/**< types of a mailbox message */
#define CTC_MBOX_MSG_REQ 0
#define CTC_MBOX_MSG_RES 1

/**< construction of mailbox data */
#ifndef CTC_MBOX_TIME_MS
#define CTC_MBOX_TIME_MS 500                    /**< time till mailbox message shall be send in ms */
#endif
#define CTC_MBOX_LEN 7                          /**< mailbox data length */

#define CTC_MBOX_DATA0_SIZE_MSK 0x0000FFFF      /**< mask for data length */
#define CTC_MBOX_DATA1_DST_OFFSET 16            /**< offset for direction */
#define CTC_MBOX_DATA0_DST_MSK 0x00FF0000       /**< mask for direction */
#define CTC_MBOX_DATA1_CHN_OFFSET 24            /**< offset for channel */
#define CTC_MBOX_DATA0_CHN_MSK 0xFF000000       /**< mask for channel */

#define CTC_MBOX_DATA1_PGE_MSK 0xFFFFFFFF       /**< mask page number */

/**< io ctl defines */
#define CTC_IOCTL_NUM 0x0C2C                    /**< magic number */
#define CTC_IOCTL_SIZE_FRAGMENT _IOR(CTC_IOCTL_NUM, 0, int) /**< read the fragment size */
#define CTC_IOCTL_SET_MBOX_MS _IOW(CTC_IOCTL_NUM, 1, int) /**< set the maximal mailbox waiting time */
#define CTC_IOCTL_SETUP_CYCLIC _IO(CTC_IOCTL_NUM, 2) /**< io control cmd for setup a cyclic channel */
#define CTC_IOCTL_VERSION _IOR(CTC_IOCTL_NUM, 3, int) /**< io control cmd for reading ctc kernel major version */


/****************************************************************************/
/* Makros */
/****************************************************************************/
#define CTC_GET_TIME_MS(hdlTime) ((hdlTime.tv_sec * 1000) + (hdlTime.tv_nsec / 1000000)) /**< read time in ms */


/****************************************************************************/
/* Local structs/enums */
/****************************************************************************/
/**< FIFO data entry for received data */
struct ctcDataEntry {
    void *pBuf;                                 /**< pointer to data on shared memory */
    __u16 size;                                 /**< size of data */
};

/**< Device structure */
struct ctcDeviceData {
    struct cdev chrdev;                         /**< character device */
    int minor;                                  /**< minor number */
    struct device *sysDev;                      /**< sysfs device */
    int wrIdx;                                  /**< write index */
    int rdIdx;                                  /**< read index */
    char *pWrData;                              /**< pointer to write data buffer (virtual memory) */
    DECLARE_KFIFO(rdFifo, struct ctcDataEntry *, CTC_FIFO_SIZE); /**< read fifo */
    pid_t pid;                                  /**< PID of the user space process */
    wait_queue_head_t ctcRxWait;                /**< wait queue for read fifo */
    struct mutex ctc_mutex;                     /**< mutex for ctc device */
    bool flgCyclic;                             /**< flag for cyclic data */
};

/**< Work queue entry */
typedef struct {
    struct work_struct ctcWork;                 /**< core to core work queue */
    __u32 ctcMboxData[CTC_MBOX_LEN];            /**< data received from mailbox */
} CTC_WORK_T;


/****************************************************************************/
/* Local prototypes */
/****************************************************************************/
static int ctc_open(
    struct inode *pInode,                       /**< pointer to inode */
    struct file *pFile                          /**< pointer to file */
);

static int ctc_release(
    struct inode *pInode,                       /**< pointer to inode */
    struct file *pFile                          /**< pointer to file */
);

static ssize_t ctc_read(
    struct file *pFile,                         /**< file handler */
    char *pBuf,                                 /**< buffer */
    size_t len,                                 /**< length */
    loff_t *pOffset                             /**< offset */
);

static ssize_t ctc_write(
    struct file *pFile,                         /**< file handler */
    const char *pBuf,                           /**< buffer */
    size_t len,                                 /**< length */
    loff_t *pOffset                             /**< offset */
);

long ctc_ioctl(
    struct file *pFile,                         /**< file handler */
    unsigned int cmd,                           /**< io control command */
    unsigned long arg                           /**< pointer to user data as ulong */
);

static void ctc_cleanup(
    void
);

static int ctc_create_devices(
    void
);

static int ctc_init_mbox(
    void
);

static int ctc_init_shm(
    void
);

static int ctc_notifier(
    struct notifier_block *pNb,                 /**< notifier block */
    unsigned long page,                         /**< first element is the start page of the message */
    void *pData                                 /**< pointer to second element */
);

static int ctc_mBoxRxNewJob(
    int minor,                                  /**< minor number */
    void *pBuf,                                 /**< data buffer */
    __u16 size                                  /**< buffer length */
);

static int ctc_mBoxRxRes(
    int minor,                                  /**< minor number */
    __u16 size                                  /**< buffer length */
);

static size_t ctcCalcPages(
    size_t len,                                 /**< data length */
    int ctcIdx,                                 /**< index of mailbox fifo */
    int *pReqPages,                             /**< required pages */
    int *pOffsetIdx                             /**< offset to write index */
);

static void ctcSetMboxMsg(
    __u32 *pCtcMboxData,                        /**< data for mailbox */
    int minor,                                  /**< minor number */
    __u32 page,                                 /**< first message page */
    __u16 len,                                  /**< data length */
    char ctcMsgType                             /**< request or acknowledge */
);

static void ctcGetMboxMsg(
    __u32 *pCtcMboxData,                        /**< data for mailbox */
    int *pMinor,                                /**< minor number */
    __u16 *pLen,                                /**< data length */
    char *pCtcMsgType                           /**< request or acknowledge */
);


/****************************************************************************/
/* Local variables */
/****************************************************************************/
static dev_t ctcDev = 0;                        /**< core to core device */
static int ctcMajor = 0;                        /**< core to core major number */
static struct class *ctcCharClass = NULL;       /**< core to core char class */

/**< File operations for char devices */
static struct file_operations ctc_fops =
{
    .open = ctc_open,
    .read = ctc_read,
    .write = ctc_write,
    .release = ctc_release,
    .unlocked_ioctl = ctc_ioctl,
};

/**< Core to core notifier */
static struct notifier_block ctc_nb = {
    .notifier_call = ctc_notifier,
};

static struct ctcDeviceData ctcDevices[CTC_CHANNEL_NUM]; /**< core to core devices */
static void *pVirtKernMem;                      /**< data buffer as virtual kernel memory */
static struct workqueue_struct *fifo_workqueue = NULL; /**< fifo queue for work */
static unsigned int ctcMboxTimeoutMs = CTC_MBOX_TIME_MS; /**< mailbox waiting time */


/****************************************************************************/
/** Initializes the core to core module
 *
 * This function initialize the core to core module on Linux. A requirement
 * next to the char devices is the mailbox driver.
 *
 * @return 0 if successful, otherwise error code
 */
static int __init ctc_init(
    void
)
{
    int res;                                    /* response */

    printk(KERN_INFO "CTC: Start initialization of CTC module version %u.%u.\n", CTC_KERNEL_VERSION_MAJOR, CTC_KERNEL_VERSION_MINOR);

    memset(&(ctcDevices[0]), 0, sizeof(struct ctcDeviceData) * CTC_CHANNEL_NUM);

    /* Alloc char dev regions */
    res = alloc_chrdev_region(&ctcDev, 0, CTC_CHANNEL_NUM, CTC_DEVICE_NAME);
    if (res) {
        printk(KERN_NOTICE "CTC: %s - Failed to alloc char devs.\n", __func__);
        return -EFAULT;
    }
    ctcMajor = MAJOR(ctcDev);

    /* Create class in sysfs */
    ctcCharClass = class_create(THIS_MODULE, CTC_DEVICE_NAME);
    printk(KERN_NOTICE "CTC: Created ctc class.\n");

    /* Create char devices */
    res = ctc_create_devices();
    if (res) {
        ctc_cleanup();
        return res;
    }

    /* Init mailbox and interrupt handler */
    res = ctc_init_mbox();
    if (res) {
        ctc_cleanup();
        return res;
    }

    /* Create the workqueue for signaling */
    fifo_workqueue = create_workqueue(CTC_WORK_QUEUE_NAME);

    printk(KERN_INFO "CTC: CTC module version %u.%u initialized.\n", CTC_KERNEL_VERSION_MAJOR, CTC_KERNEL_VERSION_MINOR);

    return 0;
}


/****************************************************************************/
/** Initializes the shared memory
 *
 * This function initialize the shared memory for linux usage. The device
 * tree node c2c_sram contains the physical start address and the length of
 * the whole shared memory. It is remaped to Kernel space, thus the module is
 * able to read received messages.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_init_shm(
    void
)
{
    struct device_node *pDp;                    /* device tree node for shared memory register */
    struct resource res;                        /* recource */
    int ret;                                    /* return value */
    unsigned int ctc_sramSize;                  /* size of SRAM */

    /* find the SRAM node */
    pDp = of_find_compatible_node(NULL, NULL, "mmio-sram");
    if (!pDp) {
        /* Error */
        printk(KERN_ALERT "CTC: %s - unable to find node c2c_sram.\n", __func__);
        return -EFAULT;
    }

    /* read the memory size */
    ret = of_address_to_resource(pDp, 0, &res);
    if (0 > ret) {
        printk(KERN_ALERT "CTC: %s - could not get address for node %s.\n",
               __func__, pDp->full_name);
        of_node_put(pDp);
        return ret;
    }
    ctc_sramSize = resource_size(&res);
    if ((CTC_MEM_SIZE << 1) > ctc_sramSize) {
        printk(KERN_ALERT "CTC: %s - Shared memory is too small: 0x%X byte availible 0x%X byte needed.\n", __func__, ctc_sramSize, (CTC_MEM_SIZE << 1));
        of_node_put(pDp);
        return -EFAULT;
    }

    /* map the SRAM */
    pVirtKernMem = of_iomap(pDp, 0);
    if (NULL == pVirtKernMem) {
        printk(KERN_NOTICE "CTC: %s - unable to remap 0x%X byte\n", __func__, ctc_sramSize);
        of_node_put(pDp);
        return -EFAULT;
    }

    /* clear the memory for transmission */
    memset(pVirtKernMem, 0, CTC_MEM_SIZE);

    of_node_put(pDp);

    return 0;
}


/****************************************************************************/
/** Initializes the char devices for the different channels
 *
 * All devices are created here. The shared memory region of reading and
 * writing is initialized at the beginning to ensure available of the needed
 * RAM.
 * Right after, the individual char devices and sysfs devices are initialized.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_create_devices(
    void
)
{
    int res;                                    /* response */
    int idx;                                    /* index */
    char buffer[10];                            /* buffer for device name */

    /* initialize the shared memory */
    res = ctc_init_shm();
    if (res) {
        return res;
    }

    /* Init individual char devs and sysfs devices */
    for (idx = 0; idx < CTC_CHANNEL_NUM; idx++) {
        cdev_init(&(ctcDevices[idx].chrdev), &ctc_fops);
        ctcDevices[idx].chrdev.owner = THIS_MODULE;
        ctcDevices[idx].chrdev.ops = &ctc_fops;
        ctcDevices[idx].minor = idx;
        res = cdev_add(&(ctcDevices[idx].chrdev), MKDEV(ctcMajor, idx), 1);
        if (res) {
            printk(KERN_NOTICE "CTC: %s - Failed to alloc char devs.\n", __func__);
            return -EFAULT;
        } else {
            printk(KERN_NOTICE "CTC: Created device "CTC_DEVICE_FILE".\n", idx);
        }
        sprintf(buffer, CTC_DEVICE_FILE, idx);
        ctcDevices[idx].sysDev = device_create(ctcCharClass, NULL, MKDEV(ctcMajor, idx), NULL, buffer);

        /* Init the fifo for the mailbox messages */
        INIT_KFIFO(ctcDevices[idx].rdFifo);

        ctcDevices[idx].pWrData = (char *) (pVirtKernMem + (CTC_PAGE_COUNT * CTC_PAGE_SIZE * idx));

        ctcDevices[idx].wrIdx = 0;
        ctcDevices[idx].rdIdx = 0;
        ctcDevices[idx].pid = 0;
        ctcDevices[idx].flgCyclic = false;

        /* init the wait queue for the channel */
        init_waitqueue_head(&(ctcDevices[idx].ctcRxWait));

        /* init the mutex */
        mutex_init(&(ctcDevices[idx].ctc_mutex));
    }

    return 0;
}


/****************************************************************************/
/** Initializes the mailboxes incl. interrupts here
 *
 * The mailbox driver PL320 is used for the communication. This function
 * registers the notifier.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_init_mbox(
    void
)
{
    pl320_ipc_register_notifier(&ctc_nb);
    return 0;
}


/****************************************************************************/
/** Core to Core notifier
 *
 * This notifier is called, after a new mailbox message received.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_notifier(
    struct notifier_block *pNb,                 /**< notifier block */
    unsigned long page,                         /**< first element is the start page of the message */
    void *pData                                 /**< pointer to second element */
)
{
    int minor;                                  /* minor number */
    __u16 len;                                  /* data length */
    __u32 offsetRxData;                         /* offset to new rx data */
    char ctcMsgType;                            /* request or acknowledge */

    /* evaluate the mailbox data on the work queue */
    ctcGetMboxMsg((__u32 *) pData, &minor, &len, &ctcMsgType);

    if ((CTC_CHANNEL_NUM > minor) && (CTC_PAGE_COUNT > page)) {
        /* select the message type */
        if (CTC_MBOX_MSG_REQ != ctcMsgType) {
            ctc_mBoxRxRes(minor, len);
            /* no evaluation of the return value - in error case, the message will be lost */

        } else {
            /* data are on the rx buffer. Calculate the offset to it */
            /* Offset to the read buffer */
            offsetRxData = CTC_MEM_OFFSET_RX;
            /* Offset to the correct channel */
            offsetRxData += (minor << (CTC_PAGE_SIZE_EXPONENT + CTC_PAGE_COUNT_EXPONENT));
            /* Offset to the correct page */
            offsetRxData += (page << CTC_PAGE_SIZE_EXPONENT);

            ctc_mBoxRxNewJob(minor, (pVirtKernMem +  offsetRxData), len);
            /* no evaluation of the return value - in error case, the message will be lost */
        }
    }
    return 0;
}


/****************************************************************************/
/** Handle received mailbox response
 *
 * This function increases the read index of the writing buffer based on the
 * length of data. It is called during the mailbox receive interrupt.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_mBoxRxRes(
    int minor,                                  /**< minor number */
    __u16 size                                  /**< buffer length */
)
{
    int reqPages;                               /* required pages */
    int offsetIdx;                              /* offset to write index */

    /* calculate the required pages and the offset */
    if (size != ctcCalcPages((size_t) size, ctcDevices[minor].rdIdx, &reqPages, &offsetIdx)) {
        return -EPERM;
    }

    /* increase the read index and consider the overflow */
    ctcDevices[minor].rdIdx += (reqPages + offsetIdx);
    ctcDevices[minor].rdIdx &= (CTC_PAGE_COUNT - 1);

    return 0;
}


/****************************************************************************/
/** Handle new Job
 *
 * This function is called, if a new job was received on the mailbox. The data
 * are stored to the mailbox fifo and the read process is waked up.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_mBoxRxNewJob(
    int minor,                                  /**< minor number */
    void *pBuf,                                 /**< data buffer */
    __u16 size                                  /**< buffer length */
)
{
    struct ctcDataEntry *pEntry;                /* fifo entry */
    struct kfifo *pFifo;                        /* dido of the channel */
    unsigned int ret;                           /* return value */

    /* alloc a new fifo entry */
    pEntry = kmalloc(sizeof(struct ctcDataEntry), GFP_ATOMIC);
    if (!pEntry) {
        /* error during allocation */
        printk(KERN_NOTICE "CTC: %s - Could not allocate memory for fifo entry.\n", __func__);
        return -ENOMEM;
    }
    /* store the data on the fifo */
    pEntry->size = size;
    pEntry->pBuf = pBuf;

    /* Get the according channel fifo */
    pFifo = (struct kfifo *) &(ctcDevices[minor].rdFifo);

    if (!kfifo_is_full(pFifo)) {
        /* add entry */
        ret = kfifo_in(pFifo, &pEntry, 1);
        if (1 != ret) {
            printk(KERN_NOTICE "CTC: %s - Could not get data from FIFO.\n", __func__);
            return -EPERM;
        }

        /* wake up the read process */
        wake_up_interruptible(&(ctcDevices[minor].ctcRxWait));
        return 0;

    } else {
        /* Error handler */
        printk(KERN_NOTICE "CTC: %s - Mailbox FIFO is full.\n", __func__);
    }

    return -EPERM;
}


/****************************************************************************/
/** Calculate Pages
 *
 * This function calculates the required pages and the index offset based on
 * the data length and the index.
 * The required pages doesn't include the offset!
 *
 * @return data length if successful, otherwise 0
 */
static size_t ctcCalcPages(
    size_t len,                                 /**< data length */
    int ctcIdx,                                 /**< rndex of mailbox fifo */
    int *pReqPages,                             /**< required pages */
    int *pOffsetIdx                             /**< offset to write index */
)
{
    int reqPages;                               /* required pages */

    /* calculate the necessary pages */
    reqPages = (len + (CTC_PAGE_SIZE - 1)) >> CTC_PAGE_SIZE_EXPONENT;

    /* The data will not be splitted at the end of the cyclic buffer. Instead they'll be written to the first page */
    /* Check if the new data will fit onto the buffer */
    if ((1 < reqPages) &&
            (CTC_PAGE_COUNT < (ctcIdx + reqPages))) {
        /* calculate offset */
        *pOffsetIdx = (int) (CTC_PAGE_COUNT - ctcIdx);
    } else {
        *pOffsetIdx = 0;
    }

    /* hand back the calculated required pages */
    *pReqPages = reqPages;

    return len;
}


/****************************************************************************/
/** Set mailbox message
 *
 * This function prepares the mailbox message by filling all data into the
 * mailbox data structure.
 */
static void ctcSetMboxMsg(
    __u32 *pCtcMboxData,                        /**< data for mailbox */
    int minor,                                  /**< minor number */
    __u32 page,                                 /**< first message page */
    __u16 len,                                  /**< data length */
    char ctcMsgType                             /**< request or acknowledge */
)
{
    /* prepare the mailbox message */
    /* add the information of the first page entry */
    *(pCtcMboxData) = (CTC_MBOX_DATA1_PGE_MSK & page);

    /* add the minor number as channel ID */
    *(pCtcMboxData + 1) = (CTC_MBOX_DATA0_CHN_MSK & ((__u32) minor << CTC_MBOX_DATA1_CHN_OFFSET));

    /* add the message type */
    *(pCtcMboxData + 1) |= (CTC_MBOX_DATA0_DST_MSK & ((__u32) ctcMsgType << CTC_MBOX_DATA1_DST_OFFSET));

    /* add the data length */
    *(pCtcMboxData + 1) |= (CTC_MBOX_DATA0_SIZE_MSK & len);
}


/****************************************************************************/
/** Get mailbox message
 *
 * This function reads all informations from the mailbox buffer.
 *
 * @return data length if successful, otherwise 0
 */
static void ctcGetMboxMsg(
    __u32 *pCtcMboxData,                        /**< data for mailbox */
    int *pMinor,                                /**< minor number */
    __u16 *pLen,                                /**< data length */
    char *pCtcMsgType                           /**< request or acknowledge */
)
{
    /* read the minor number */
    *pMinor = (CTC_MBOX_DATA0_CHN_MSK & *(pCtcMboxData)) >> CTC_MBOX_DATA1_CHN_OFFSET;

    /* read the message type */
    *pCtcMsgType = (CTC_MBOX_DATA0_DST_MSK & *(pCtcMboxData)) >> CTC_MBOX_DATA1_DST_OFFSET;

    /* read the data length */
    *pLen = (__u16) (CTC_MBOX_DATA0_SIZE_MSK & *(pCtcMboxData));
}


/****************************************************************************/
/** Deinitializes the core to core module
 *
 */
static void ctc_cleanup(
    void
)
{
    int idx;                                    /* index */

	/* don't teardown resources that haven't been claimed yet. doh */
	if (!pVirtKernMem)
		return;

    /* unregister the notifier */
    pl320_ipc_unregister_notifier(&ctc_nb);

    /* Deinit individual char devs and devices */
    for (idx = 0; idx < CTC_CHANNEL_NUM; idx++) {
        cdev_del(&(ctcDevices[idx].chrdev));
        device_destroy(ctcCharClass, MKDEV(ctcMajor, idx));
        printk(KERN_NOTICE "CTC: Removed device ctc_chan%u.\n", idx);
    }

    /* Unregister region */
    if (0 != ctcDev) {
        unregister_chrdev_region(ctcDev, CTC_CHANNEL_NUM);
    }

    /* Destroy class */
    if (ctcCharClass) {
        class_destroy(ctcCharClass);
        printk(KERN_NOTICE "CTC: Removed ctc class.\n");
    }

    printk(KERN_INFO "CTC: CTC module released.\n");
}


/****************************************************************************/
/** Opens a core to core char device for reading/ writing
 *
 * Open a char device for core to core usage. This is only possible, if the
 * nonblocking option is disabled.
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_open(
    struct inode *pInode,                       /**< pointer to inode */
    struct file *pFile                          /**< pointer to file */
)
{
    int minor;                                  /* minor number */

    minor = MINOR(pFile->f_inode->i_rdev);

    /* Check for non blocking option */
    if (O_NONBLOCK == (O_NONBLOCK & (pFile->f_flags))) {
        return -EACCES;
    }

    /* check if the file is already open */
    if (ctcDevices[minor].pid) {
        return -EACCES;
    }

    /* save the pid */
    ctcDevices[minor].pid = current->pid;

    return 0;
}


/****************************************************************************/
/** Closes a core to core char device
 *
 * This function closes a core to core char device. This is only possible by
 * the task, which opened it.
 *
 * @param pInode, pointer to inode
 * @param pFile, pointer to file
 *
 * @return 0 if successful, otherwise error code
 */
static int ctc_release(
    struct inode *pInode,                       /**< pointer to inode */
    struct file *pFile                          /**< pointer to file */
)
{
    int minor;                                  /* minor number */

    minor = MINOR(pFile->f_inode->i_rdev);

    /* remove the pid */
    ctcDevices[minor].pid = 0;
    return 0;
}


/****************************************************************************/
/** Reads up to len bytes from a core to core char device
 *
 * This function provides a blocking reading of new messages on the mailbox
 * FIFO.
 *
 * @return data length if successful, otherwise error code or 0 if signal
 * occurs
 */
static ssize_t ctc_read(
    struct file *pFile,                         /**< file handler */
    char *pBuf,                                 /**< buffer */
    size_t len,                                 /**< length */
    loff_t *pOffset                             /**< offset */
)
{
    int minor;                                  /* minor number */
    int resMbox;                                /* mailbox result */
    struct kfifo *pFifo;                        /* fifo of the channel */
    struct ctcDataEntry *pEntry;                /* selected fifo entry */
    size_t fifoDataLen;                         /* length of the fido entry */
    ssize_t res = 0;                            /* result */
    __u32 ctcMboxData[CTC_MBOX_LEN];            /* data for mailbox */
    struct timespec mboxTimeout;                /* mailbox timeout */
    struct timespec mboxTime;                   /* elapsed mailbox time */

    minor = MINOR(pFile->f_inode->i_rdev);

    if (CTC_CHANNEL_NUM <= minor) {
        printk(KERN_INFO "CTC: "CTC_DEVICE_FILE" does not exist.\n", minor);
        return -EFAULT;
    }

    /* Get the according channel fifo */
    pFifo = (struct kfifo *) &(ctcDevices[minor].rdFifo);

    /* set module to sleep until there is an entry on the mailbox fifo */
    res = wait_event_interruptible(ctcDevices[minor].ctcRxWait, !kfifo_is_empty(pFifo));
    if (0 == res) {
        /* get a fifo entry */
        fifoDataLen = kfifo_out(pFifo, &pEntry, 1);

        /* check if the reading was successfull */
        if (1 == fifoDataLen) {

            /* check the length */
            if (len >= pEntry->size) {
                /* copy the data to user space */
                if (!copy_to_user(pBuf, pEntry->pBuf, pEntry->size)) {
                    /* Data has been read successful. Update the result length */
                    res = pEntry->size;
                }
            }
            /* set the mailbox message */
            ctcSetMboxMsg(ctcMboxData, minor, 0, pEntry->size, CTC_MBOX_MSG_RES);

            /* get the current time stamp */
            getnstimeofday(&mboxTimeout);
            do {
                /* send the mailbox message */
                resMbox = pl320_ipc_transmit(ctcMboxData);
                getnstimeofday(&mboxTime);
                if (0 == resMbox) {
                    /* if mailbox sending was successful break */
                    break;
                }
            } while ((CTC_GET_TIME_MS(mboxTimeout) + ctcMboxTimeoutMs) > CTC_GET_TIME_MS(mboxTime));

            /* Free the buffer whether the destination buffer is to small or not. */
            kfree(pEntry);

            /* print a kernel info in case of an error */
            if ((CTC_GET_TIME_MS(mboxTimeout) + ctcMboxTimeoutMs) <= CTC_GET_TIME_MS(mboxTime)) {
                printk(KERN_NOTICE "CTC: %s - Unable to send a response [Err: 0x%x].\n", __func__, resMbox);
            }
        }
    }

    return res;
}


/****************************************************************************/
/** Writes up to len bytes to a core to core char device
 *
 * @return data length if successful, otherwise 0
 */
static ssize_t ctc_write(
    struct file *pFile,                         /**< file handler */
    const char *pBuf,                           /**< buffer */
    size_t len,                                 /**< length */
    loff_t *pOffset                             /**< offset */
)
{
    __u32 ctcMboxData[CTC_MBOX_LEN];            /* data for mailbox */
    int minor;                                  /* minor number */
    int ctcPageWrIdx;                           /* write index for one page */
    int resMbox;                                /* mailbox result */
    unsigned int remPages;                      /* remaining pages */
    unsigned int reqPages;                      /* required pages */
    unsigned int offsetIdx;                     /* offset to first element of cyclic buffer */
    char *pDst;                                 /* destination on kernel memory */
    struct timespec mboxTimeout;                /* mailbox timeout */
    struct timespec mboxTime;                   /* elapsed mailbox time */

    minor = MINOR(pFile->f_inode->i_rdev);

    if (CTC_CHANNEL_NUM <= minor) {
        printk(KERN_INFO "CTC:"CTC_DEVICE_FILE" does not exist.\n", minor);
        return 0;
    }

    if (ctcDevices[minor].flgCyclic) {
        if (ctcDevices[minor].rdIdx != ctcDevices[minor].wrIdx) {
            /* sending of more than one message to the destination isn't allowed on cyclic channel */
            printk(KERN_INFO "CTC: An other message blocks the cyclic channel.\n");
            return 0;
        }
    }

    mutex_lock(&(ctcDevices[minor].ctc_mutex));

    /* get the actual write index - update it at successful writing */
    /* The index is 0 <= wrIdx < CTC_PAGE_COUNT */
    ctcPageWrIdx = ctcDevices[minor].wrIdx;

    /* calculate the required pages and offset */
    if (len != ctcCalcPages(len, ctcPageWrIdx, &reqPages, &offsetIdx)) {
        /* Error */
        mutex_unlock(&(ctcDevices[minor].ctc_mutex));
        printk(KERN_INFO "CTC: %s - Unable to calculate pages.\n", __func__);
        return 0;
    }

    /* calculate the remaining pages on the buffer */
    remPages = (CTC_PAGE_COUNT - 1) - (ctcPageWrIdx) + ctcDevices[minor].rdIdx;
    remPages &= (CTC_PAGE_COUNT - 1);

    /* check if there are enough free entries on the buffer */
    if (remPages < (reqPages + offsetIdx)) {
        /* ERROR */
        mutex_unlock(&(ctcDevices[minor].ctc_mutex));
        printk(KERN_INFO "CTC: %s - No space for %u byte data on buffer.\n", __func__, len);
        return 0;
    }

    /* add the offset and consider the overflow */
    ctcPageWrIdx += offsetIdx;
    ctcPageWrIdx &= (CTC_PAGE_COUNT - 1);

    /* determine the destination page for writing data */
    pDst = ctcDevices[minor].pWrData + (ctcPageWrIdx << CTC_PAGE_SIZE_EXPONENT);

    /* Copy the data from user space. */
    if (copy_from_user(pDst, pBuf, len)) {
        /* ERROR */
        mutex_unlock(&(ctcDevices[minor].ctc_mutex));
        printk(KERN_INFO "CTC: %s - Unable to copy user data.\n", __func__);
        return 0;
    }

    /* prepare the mailbox message */
    ctcSetMboxMsg(ctcMboxData, minor, ctcPageWrIdx, (uint16_t) len, CTC_MBOX_MSG_REQ);

    /* get the current time stamp */
    getnstimeofday(&mboxTimeout);
    do {
        /* send the mailbox message */
        resMbox = pl320_ipc_transmit(ctcMboxData);
        getnstimeofday(&mboxTime);
        if (0 == resMbox) {
            /* update the write index according to the required pages and consider the overflow */
            ctcDevices[minor].wrIdx = (ctcPageWrIdx + reqPages) & (CTC_PAGE_COUNT - 1);
            mutex_unlock(&(ctcDevices[minor].ctc_mutex));
            return len;
        }
    } while ((CTC_GET_TIME_MS(mboxTimeout) + ctcMboxTimeoutMs) > CTC_GET_TIME_MS(mboxTime));

    /* unable to send the message */
    mutex_unlock(&(ctcDevices[minor].ctc_mutex));
    printk(KERN_INFO "CTC: %s - Unable to send the mailbox message [Err: 0x%x].\n", __func__, resMbox);
    return 0;
}


/****************************************************************************/
/** Io control for core to core char device
 *
 * @return 0 if successful, otherwise error code
 */
long ctc_ioctl(
    struct file *pFile,                         /**< file handler */
    unsigned int cmd,                           /**< io control command */
    unsigned long arg                           /**< pointer to user data as ulong */
)
{
    int res = 0;                                /* result */
    int minor;                                  /* minor number */
    struct kfifo *pFifo;                        /* dido of the channel */

    minor = MINOR(pFile->f_inode->i_rdev);

    if (CTC_CHANNEL_NUM <= minor) {
        printk(KERN_INFO "CTC:"CTC_DEVICE_FILE" does not exist.\n", minor);
        return 0;
    }

    switch (cmd) {
        case CTC_IOCTL_SIZE_FRAGMENT:
            if (ctcDevices[minor].flgCyclic) {
                /* double buffer */
                res = put_user(((CTC_PAGE_COUNT * CTC_PAGE_SIZE) >> 1), (int __user *) arg);
            }
            else {
                res = put_user(((CTC_PAGE_COUNT - 1) << CTC_PAGE_SIZE_EXPONENT), (int __user *) arg);
            }
            break;

        case CTC_IOCTL_SET_MBOX_MS:
            ctcMboxTimeoutMs = *(unsigned int __user *) arg;
            break;

        case CTC_IOCTL_SETUP_CYCLIC:
            /* Get the according channel fifo */
            pFifo = (struct kfifo *) &(ctcDevices[minor].rdFifo);
            /* configure the channel as cyclic if it is opened and empty */
            if ((0 != ctcDevices[minor].pid)
                    && (ctcDevices[minor].wrIdx == ctcDevices[minor].rdIdx)
                    && (kfifo_is_empty(pFifo))) {
                ctcDevices[minor].flgCyclic = true;
                printk(KERN_INFO "CTC:"CTC_DEVICE_FILE" has been configured for cyclic data.\n", minor);
            }
            else {
                printk(KERN_INFO "CTC:"CTC_DEVICE_FILE" could not be configured for cyclic data.\n", minor);
                res = -EFAULT;
            }
            break;

        case CTC_IOCTL_VERSION:
            res = put_user(CTC_KERNEL_VERSION_MAJOR, (int __user *) arg);
            break;

        default:
            res = -EFAULT;
            break;
    }

    return res;
}


module_init(ctc_init);
module_exit(ctc_cleanup);

MODULE_LICENSE("GPLv2");
MODULE_AUTHOR("GOAL");
MODULE_DESCRIPTION("Core2Core driver");
