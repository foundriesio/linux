// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_sdr_ipc.h>

#define LOG_TAG "SDRIPC"
int sdripc_verbose_mode=1;
#define eprintk(dev, msg, ...)	((void)dev_err(dev, "[ERROR][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	((void)dev_warn(dev, "[WARN][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	((void)dev_info(dev, "[INFO][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	{ if(sdripc_verbose_mode == 1) { (void)dev_info(dev, "[INFO][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } }

#define MAX_NUM_IPC_CLIENT (16)
#define SHARED_BUFFER_SIZE (1024*512)

struct sdripc_receive_list {
	struct tcc_mbox_data data;
	struct list_head queue;
};

struct ipclient_t {
	int client_index;            /* initial value should be (-1) */
	spinlock_t  rx_queue_lock;
	struct list_head rx_queue;
	int rx_queue_count;
	wait_queue_head_t event_waitq;
	struct sdripc_device *sdripc_dev;
};

struct sdripc_device
{
	struct platform_device *pdev;
	struct device *dev;
	struct cdev   cdev;
	struct class *class;
	dev_t devnum;
	const char *device_name;
	const char *mbox_name;
	const char *mbox_id_string;
	struct mbox_chan *mbox_channel;

	unsigned char *vaddr;      /* Holds a virtual address to send */
	dma_addr_t paddr;          /* Holds a physical address to send */
	int shared_head_offset;    /* Offset to be saved */

	unsigned char *receive_vaddr;  /* Holds a virtual address to be received */
	dma_addr_t receive_paddr;      /* Holds a physical address to be received */

	struct mutex mboxsendMutex;		/* mailbos send mutex */
	struct ipclient_t *ipclient_list[MAX_NUM_IPC_CLIENT];
	spinlock_t ipclient_lock[MAX_NUM_IPC_CLIENT];
};

/* get client index from IPC data */
static int get_ipclient_index(unsigned char *buf, int size)
{
	int ret;

	if((buf!=NULL)&&(size>4))
	{
		ret = (int)(buf[4]&0x0F);
	}
	else
	{
		ret = -1;
	}

	return ret;
}

static int get_ipclient(struct sdripc_device *sdripc_dev, int index, struct ipclient_t **p_ipclient)
{
	//warn//struct device *dev = sdripc_dev->dev;
	int ret=0;

	if(sdripc_dev==NULL)
	{
		return (-EINVAL);
	}

	if(p_ipclient==NULL)
	{
		return (-EINVAL);
	}

	if((index<0) || (index>=MAX_NUM_IPC_CLIENT))
	{
		return (-EINVAL);
	}

	*p_ipclient = sdripc_dev->ipclient_list[index];
	//dprintk(dev,"ipclient_list[%d]=%p\n", index,sdripc_dev->ipclient_list[index]);
	return ret;
}

static int register_ipclient(struct sdripc_device *sdripc_dev, int index, struct ipclient_t *ipclient)
{
	struct device *dev = sdripc_dev->dev;
	int ret=0;

	if(sdripc_dev==NULL)
	{
		return (-EINVAL);
	}

	if(index<0 || index>=MAX_NUM_IPC_CLIENT)
	{
		return (-EINVAL);
	}

	if(ipclient->client_index!=(-1))
	{
		/* given index is already in used */
		return (-EBUSY);
	}

	if(sdripc_dev->ipclient_list[index]!=NULL)
	{
		/* given index is already in used */
		return (-EBUSY);
	}

	sdripc_dev->ipclient_list[index] = ipclient;
	ipclient->client_index = index;

	dprintk(dev,"register ipclient[%p] idx[%d]\n", ipclient, index);

	#if 0 // debug
	{
		int i;
		for(i=0;i<MAX_NUM_IPC_CLIENT;i++)
		{
			if(sdripc_dev->ipclient_list[i]!=NULL)
			{
				dprintk(dev,"ipclient_list[%d]=%p\n", i,sdripc_dev->ipclient_list[i]);
			}
		}
	}
	#endif
	return ret;
}

static int unregister_ipclient(struct sdripc_device *sdripc_dev, struct ipclient_t *ipclient)
{
	struct device *dev = sdripc_dev->dev;
	int ret=0;

	if(sdripc_dev==NULL)
	{
		return (-EINVAL);
	}

	if((ipclient->client_index)<0 || (ipclient->client_index)>=MAX_NUM_IPC_CLIENT)
	{
		/* invalid_index */
		return (-EINVAL);
	}


	dprintk(dev,"unregister ipclient[%p] idx[%d]\n", ipclient,ipclient->client_index);
	sdripc_dev->ipclient_list[ipclient->client_index] = NULL;
	ipclient->client_index = -1;

	#if 0 // debug
	{
		int i;
		for(i=0;i<MAX_NUM_IPC_CLIENT;i++)
		{
			if(sdripc_dev->ipclient_list[i]!=NULL)
			{
				dprintk(dev,"ipclient_list[%d]=%p\n", i,sdripc_dev->ipclient_list[i]);
			}
		}
	}
	#endif
	return ret;
}

static int sdripc_rxqueue_init(struct ipclient_t *ipclient)//(struct sdripc_device *sdripc_dev)
{
	//warn//struct sdripc_device *sdripc_dev=ipclient->sdripc_dev;
	//warn//struct device *dev = sdripc_dev->dev;

	INIT_LIST_HEAD(&ipclient->rx_queue);
	spin_lock_init(&ipclient->rx_queue_lock);
	ipclient->rx_queue_count=0;
	return 0;
}

static int sdripc_rxqueue_push(struct ipclient_t *ipclient, struct tcc_mbox_data *mbox_data)
{
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	unsigned long flags;
	int ret=0;

	receive_list = devm_kzalloc(dev, sizeof(struct sdripc_receive_list), GFP_KERNEL);
	if(receive_list == NULL)
	{
		ret = -ENOMEM;
		return ret;
	}
	INIT_LIST_HEAD(&receive_list->queue);
	memcpy(&receive_list->data, mbox_data, sizeof(struct tcc_mbox_data));

	spin_lock_irqsave(&ipclient->rx_queue_lock, flags);
	list_add_tail(&receive_list->queue, &ipclient->rx_queue);
	ipclient->rx_queue_count++;
	spin_unlock_irqrestore(&ipclient->rx_queue_lock, flags);

	return 0;
}

static int sdripc_rxqueue_get_count(struct ipclient_t *ipclient)
{
	return ipclient->rx_queue_count;
}

static int sdripc_rxqueue_get(struct ipclient_t *ipclient, struct tcc_mbox_data *mbox_data)
{
	//warn//struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	//warn//struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ipclient->rx_queue_lock, flags);
	if(list_empty(&ipclient->rx_queue)==0)
	{
		receive_list = list_first_entry(&ipclient->rx_queue, struct sdripc_receive_list, queue);
		if(receive_list!=NULL)
		{
			memcpy(mbox_data, &receive_list->data, sizeof(struct tcc_mbox_data));
		}
		else
		{	/* no data */
			ret = -1;
		}
	}
	else
	{	/* no data */
		ret = -1;
	}
	spin_unlock_irqrestore(&ipclient->rx_queue_lock, flags);

	return ret;
}

static int sdripc_rxqueue_pop(struct ipclient_t *ipclient)
{
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ipclient->rx_queue_lock, flags);
	if(list_empty(&ipclient->rx_queue)==0)
	{
		receive_list = list_first_entry(&ipclient->rx_queue, struct sdripc_receive_list, queue);
		if(receive_list!=NULL)
		{
			list_del(&receive_list->queue);
			devm_kfree(dev, receive_list);
		}
	}
	ipclient->rx_queue_count--;
	spin_unlock_irqrestore(&ipclient->rx_queue_lock, flags);
	return ret;
}

static int sdripc_rxqueue_deinit(struct ipclient_t *ipclient)
{
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
    struct sdripc_receive_list *receive_list_tmp;
	unsigned long flags;
	int ret = 0;

	/* for debug */
	if(ipclient->rx_queue_count!=0)
	{
		eprintk(dev,"dev%d remain count %d\n", ipclient->client_index,  ipclient->rx_queue_count);
	}

	/* flush rxqueue */
	spin_lock_irqsave(&ipclient->rx_queue_lock, flags);
    list_for_each_entry_safe(receive_list, receive_list_tmp, &ipclient->rx_queue, queue)
    {
        list_del_init(&receive_list->queue);
        devm_kfree(dev, receive_list);
    }
	ipclient->rx_queue_count=0;
	spin_unlock_irqrestore(&ipclient->rx_queue_lock, flags);
	return ret;
}


static int shared_buffer_init(struct sdripc_device *sdripc_dev)
{
	//warning//struct device *dev = sdripc_dev->dev;
	int ret = 0;

	sdripc_dev->shared_head_offset = 0;

	return ret;
}

static int shared_buffer_copy_from_user(struct sdripc_device *sdripc_dev, const char __user *data, int size)
{
	struct device *dev = sdripc_dev->dev;
	//warning//int ret = 0;
	unsigned long result;
	unsigned char *shared_buf_vaddr;
	int start_offset;
	int next_start_offset;

	if( (sdripc_dev->shared_head_offset+(int)size)>SHARED_BUFFER_SIZE )
	{
		/* rollback shared_buffer */
		sdripc_dev->shared_head_offset = 0;
	}

	shared_buf_vaddr = sdripc_dev->vaddr + sdripc_dev->shared_head_offset;
	start_offset = sdripc_dev->shared_head_offset;

	result = copy_from_user(shared_buf_vaddr, data, size);
	if (result != 0)
	{
		eprintk(dev, "copy_from_user failed: %ld\n", result);
		return (-1);
	}

	/* Align address to 8 byte */
	next_start_offset = start_offset + size;
	sdripc_dev->shared_head_offset = (next_start_offset+(8-1))&~(8-1);

	return start_offset;
}

void sdripc_mbox_receive_message(struct mbox_client *client, void *message)
{
	struct platform_device *pdev = to_platform_device(client->dev);
	struct sdripc_device *sdripc_dev = platform_get_drvdata(pdev);
	struct device *dev = client->dev;
	struct tcc_mbox_data *mbox_data = (struct tcc_mbox_data *)message;
	int ret;
	int client_index;
	struct ipclient_t *ipclient;
	int start_offset;
	int dataSize;
	//dprintk(dev, "cmd[%08X][%08X][%08X] [%08X][%08X]\n",
	//	mbox_data->cmd[0], mbox_data->cmd[1], mbox_data->cmd[2], mbox_data->cmd[3], mbox_data->cmd[4], mbox_data->cmd[5]);

	if(sdripc_dev->receive_paddr==0)
	{
		dma_addr_t receive_paddr;
		memcpy((char*)&receive_paddr, &mbox_data->cmd[4], sizeof(dma_addr_t));
		sdripc_dev->receive_vaddr = (unsigned char*)ioremap_nocache(receive_paddr, SHARED_BUFFER_SIZE);
		if (sdripc_dev->receive_vaddr == NULL)
		{
			eprintk(dev, "Fail ioremap_nocache\n");
			return;
		}
		sdripc_dev->receive_paddr = receive_paddr;
		dprintk(dev, "Remap paddr paddr[%llX] to vaddr[%p]\n", sdripc_dev->receive_paddr, sdripc_dev->receive_vaddr);
	}
	#if 1
	else /* never happens */
	{
		dma_addr_t receive_paddr;
		memcpy((char*)&receive_paddr, &mbox_data->cmd[4], sizeof(dma_addr_t));
		if(receive_paddr != sdripc_dev->receive_paddr )
		{
			/* abnormal situation */
			eprintk(dev, "change paddr prev[%llX] to new[%llX]\n", sdripc_dev->receive_paddr, receive_paddr);
			dprintk(dev, "cmd[%08X][%08X][%08X] [%08X][%08X]\n",
				mbox_data->cmd[0], mbox_data->cmd[1], mbox_data->cmd[2], mbox_data->cmd[4], mbox_data->cmd[5]);
			return;
		}
	}
	#endif

	/* get ipclient */
	start_offset = (int)mbox_data->cmd[1];
	dataSize = (int)mbox_data->cmd[2];
	client_index = get_ipclient_index(sdripc_dev->receive_vaddr+start_offset, dataSize);
	spin_lock(&sdripc_dev->ipclient_lock[client_index]);
	ret = get_ipclient(sdripc_dev, client_index, &ipclient);
	if((ret<0) || (ipclient==NULL))
	{
		/* discard data, because client_index is not registered */
		spin_unlock(&sdripc_dev->ipclient_lock[client_index]);
		//dprintk(dev,"client_index[%d] is not registered\n", client_index);
		return;
	}

	ret = sdripc_rxqueue_push(ipclient, mbox_data);
	if(ret<0)
	{
		/* fail to push */
		spin_unlock(&sdripc_dev->ipclient_lock[client_index]);
		return;
	}
	wake_up(&ipclient->event_waitq);
	spin_unlock(&sdripc_dev->ipclient_lock[client_index]);
}

struct mbox_chan *sdripc_mbox_request_channel(struct platform_device *pdev, const char *channel_name)
{
	struct device *dev = &pdev->dev;
	struct mbox_client *client;
	struct mbox_chan *channel = NULL;

	if((pdev != NULL)&&(channel_name != NULL))
	{
		client = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
		if (!client)
		{
			return ERR_PTR(-ENOMEM);
		}

		client->dev = dev;
		client->rx_callback = sdripc_mbox_receive_message;
		client->tx_done = NULL;
		client->knows_txdone = (bool)false;
		client->tx_block = (bool)true;
		client->tx_tout = 500;

		channel = mbox_request_channel_byname(client, channel_name);
		if (IS_ERR(channel))
		{
			eprintk(dev, "Failed to request mbox channel[%s]\n", channel_name);
			return NULL;
		}
	}
	return channel;
}


static unsigned int sdripc_poll( struct file *filp, poll_table *wait)
{
	struct ipclient_t *ipclient=(struct ipclient_t *)filp->private_data;
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	//warn//struct device *dev = sdripc_dev->dev;
	unsigned int mask = 0;
	int rx_queue_count=0;


	poll_wait(filp, &ipclient->event_waitq, wait);
	spin_lock(&sdripc_dev->ipclient_lock[ipclient->client_index]);
	rx_queue_count = sdripc_rxqueue_get_count(ipclient);
	if(rx_queue_count>0)
	{
		mask |= POLLIN|POLLRDNORM;
	}
	spin_unlock(&sdripc_dev->ipclient_lock[ipclient->client_index]);
	return mask;
}

static ssize_t sdripc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t ret = -1;
	struct ipclient_t *ipclient = (struct ipclient_t *)filp->private_data;
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	//warn//struct device *dev = sdripc_dev->dev;
	struct tcc_mbox_data mbox_data;
	int mbox_result = 0;
	int shared_start_offset;

	if(buf==NULL)
	{
		return (-EINVAL);
	}

	if(count==0)
	{
		return 0;
	}

	mutex_lock(&sdripc_dev->mboxsendMutex);
	{
		/* copy data to share_buffer */
		shared_start_offset = shared_buffer_copy_from_user(sdripc_dev, buf, (int)count);
		if(shared_start_offset >= 0)
		{
			/* cmd[0]: message type. TDB later if needed */
			mbox_data.cmd[0] = 0;
			/* cmd[1]: shared buffer start offset */
			mbox_data.cmd[1] = shared_start_offset;
			/* cmd[2]: data size */
			mbox_data.cmd[2] = (int)count;
			/* cmd[3]: not used */
			/* cmd[4-5]: physical address of shared buffer */
			memcpy(&mbox_data.cmd[4], (char*)&sdripc_dev->paddr, sizeof(dma_addr_t));
			/* data_fifo won't be used */
			mbox_data.data_len = 0;

			mbox_result = mbox_send_message(sdripc_dev->mbox_channel, &mbox_data);
			if(mbox_result < 0)
			{
				//eprintk(dev, "mbox_send_message failed: %d\n", mbox_result);
				ret = (-ETIMEDOUT);
			}
			else
			{
				ret = count;
			}
		}
		else
		{
			//eprintk(dev, "shared_buffer_copy_from_user failed: %d\n", shared_start_offset);
			ret = (-EFAULT);
		}
	}
	mutex_unlock(&sdripc_dev->mboxsendMutex);

	return ret;
}

static ssize_t sdripc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct ipclient_t *ipclient = (struct ipclient_t *)filp->private_data;
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct tcc_mbox_data mbox_data;

	int ret;
	int type;
	int start_offset;
	int size=0;

	if(buf==NULL)
	{
		return (-EINVAL);
	}

#if 0 /* debug : display receive_list */
	{
		struct sdripc_receive_list *receive_list;
		struct sdripc_receive_list *receive_list_temp;
		list_for_each_entry_safe(receive_list, receive_list_temp, &ipclient_t->rx_queue, queue)
		{
			dprintk(dev, "cmd[%08X]\n", receive_list->data.cmd[1]);
		};
	}
#endif

	ret = sdripc_rxqueue_get(ipclient, &mbox_data);
	if(ret==0)
	{
		unsigned char *pSrc;
		unsigned char *pDst;
		int result;

		//dprintk(dev, "cmd[%08X][%08X][%08X]/[%08X][%08X]\n",
		//	mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2], mbox_data.cmd[4], mbox_data.cmd[5]);

		/* cmd[0]: message type. TDB later if needed */
		type = mbox_data.cmd[0];
		/* cmd[1]: shared buffer start offset */
		start_offset = mbox_data.cmd[1];
		/* cmd[2]: data size */
		size = mbox_data.cmd[2];

		if (count < size)
		{
			dprintk(dev, "buffer not enough : [%d]\n", (-ENOBUFS));
			return (-ENOBUFS);
		}

		pSrc = sdripc_dev->receive_vaddr + start_offset;
		pDst = buf;
		result = copy_to_user(pDst, pSrc, size);
		if (result != 0)
		{
			eprintk(dev, "copy_to_user failed : %d\n", result);
			return (-EFAULT);
		}

		ret = sdripc_rxqueue_pop(ipclient);
	}

	return (ssize_t)size;
}

static int sdripc_open(struct inode *inode, struct file *filp)
{
	int ret=0;
	struct sdripc_device *sdripc_dev = container_of(inode->i_cdev, struct sdripc_device, cdev);
	struct device *dev = sdripc_dev->dev;
	struct ipclient_t *ipclient;

	dprintk(dev, "enter\n");

	/* allocate ipclient instance */
	ipclient = devm_kzalloc(dev, sizeof(struct ipclient_t), GFP_KERNEL);
	if (ipclient==NULL)
	{
		return (-ENOMEM);
	}

	filp->private_data = ipclient;
	ipclient->sdripc_dev = sdripc_dev;

	ret = sdripc_rxqueue_init(ipclient);
	init_waitqueue_head(&ipclient->event_waitq);

	ipclient->client_index=(-1);

	dprintk(dev, "ipclient=%p\n", ipclient);
	dprintk(dev, "exit %d\n", ret);
	return ret;
}

static int sdripc_close(struct inode *inode, struct file *filp)
{
	int ret=0;
	struct sdripc_device *sdripc_dev = container_of(inode->i_cdev, struct sdripc_device, cdev);
	struct device *dev = sdripc_dev->dev;
	struct ipclient_t *ipclient;
	int client_index;
	dprintk(dev, "enter\n");

	ipclient = filp->private_data;
	client_index = ipclient->client_index;
	if(client_index!=(-1))
	{
		spin_lock(&sdripc_dev->ipclient_lock[client_index]);
	}
	unregister_ipclient(sdripc_dev, ipclient);

	/* question : event_waitq need to destory? */

	sdripc_rxqueue_deinit(ipclient);

	/* free ipclient instance */
	devm_kfree(dev, ipclient);
	if(client_index!=(-1))
	{
		spin_unlock(&sdripc_dev->ipclient_lock[client_index]);
	}
	filp->private_data = NULL;
	dprintk(dev, "exit %d\n", ret);
	return ret;
}

static long sdripc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = -1;

	if(filp != NULL)
	{
		struct ipclient_t *ipclient = (struct ipclient_t *)filp->private_data;
		struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
		struct device *dev = sdripc_dev->dev;

		if(ipclient != NULL)
		{
			switch(cmd)
			{
				case IOCTL_SDRPIC_SET_ID:
					{
						int index = (int)arg;
						/* register ipc client */
						spin_lock(&sdripc_dev->ipclient_lock[index]);
						ret = register_ipclient(sdripc_dev, index, ipclient);
						spin_unlock(&sdripc_dev->ipclient_lock[index]);
					}
					break;
				default:
					{
						dprintk(dev,"unrecognized ioctl (0x%x)\n",cmd);
						ret = (-EINVAL);
					}
					break;
			}
		}
		else
		{
			ret = (-ENODEV);
		}
	}
	else
	{
		ret = (-EINVAL);
	}
	return ret;
}

static const struct file_operations sdripc_ops = {
	.owner  = THIS_MODULE,
	.open	= sdripc_open,
	.release= sdripc_close,
	.write	= sdripc_write,
	.read	= sdripc_read,
	.unlocked_ioctl = sdripc_ioctl,
	.compat_ioctl = sdripc_ioctl,
	.poll	= sdripc_poll,
};

static int sdripc_cdev_create(struct device *dev, struct sdripc_device *sdripc_dev)
{
	int ret=0;

	ret = alloc_chrdev_region(&sdripc_dev->devnum, 0, 1, sdripc_dev->device_name);
	if (ret != 0)
	{
		eprintk(dev, "alloc_chrdev_region error %d\n", ret);
		goto err_alloc_chrdev;
	}

	/* Add character device */
	cdev_init(&sdripc_dev->cdev, &sdripc_ops);
	sdripc_dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&sdripc_dev->cdev, sdripc_dev->devnum, 1);
	if (ret != 0)
	{
		eprintk(dev, "cdev_add error %d\n", ret);
		goto error_cdev_add;
	}

	/* Create a class */
	sdripc_dev->class = class_create(THIS_MODULE, sdripc_dev->device_name);
	if (IS_ERR(sdripc_dev->class))
	{
		ret = (int)PTR_ERR(sdripc_dev->class);
		eprintk(dev, "class_create error %d\n", ret);
		goto error_class_create;
	}

	/* Create a device node */
	sdripc_dev->dev = device_create(sdripc_dev->class, dev, sdripc_dev->devnum, sdripc_dev, sdripc_dev->device_name);
	if (IS_ERR(sdripc_dev->dev))
	{
		ret = (int)PTR_ERR(sdripc_dev->dev);
		eprintk(dev, "device_create error %d\n", ret);
		goto error_device_create;
	}
	return 0;

error_device_create:
	class_destroy(sdripc_dev->class);
error_class_create:
	cdev_del(&sdripc_dev->cdev);
error_cdev_add:
	unregister_chrdev_region(sdripc_dev->devnum, 1);
err_alloc_chrdev:
	return ret;
}

static int sdripc_cdev_destory(struct device *dev, struct sdripc_device *sdripc_dev)
{
	device_destroy(sdripc_dev->class, sdripc_dev->devnum);
	class_destroy(sdripc_dev->class);
	cdev_del(&sdripc_dev->cdev);
	unregister_chrdev_region(sdripc_dev->devnum, 1);

	return 0;
}

static int sdripc_probe(struct platform_device *pdev)
{
	int ret=0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;
	int i;

	if(pdev==NULL)
	{
		ret = -ENODEV;
		goto error_return;
	}

	dev = &pdev->dev;
	sdripc_dev = devm_kzalloc(dev, sizeof(struct sdripc_device), GFP_KERNEL);
	if (sdripc_dev==NULL)
	{
		ret = -ENOMEM;
		goto error_return;
	}

	platform_set_drvdata(pdev, sdripc_dev);

	of_property_read_string(pdev->dev.of_node,"device-name", &sdripc_dev->device_name);
	of_property_read_string(pdev->dev.of_node,"mbox-names", &sdripc_dev->mbox_name);
	of_property_read_string(pdev->dev.of_node,"mbox-id", &sdripc_dev->mbox_id_string);
	dprintk(dev, "device name[%s], mbox-names[%s], mbox-id[%s]\n", sdripc_dev->device_name, sdripc_dev->mbox_name, sdripc_dev->mbox_id_string);

	/* create char device and device node */
	ret = sdripc_cdev_create(dev, sdripc_dev);
	if (ret)
	{
		goto error_create_cdev;
	}

	/* allocate DMA memory as shared memory */
	sdripc_dev->vaddr = dma_alloc_coherent(dev, SHARED_BUFFER_SIZE, &sdripc_dev->paddr, GFP_KERNEL);
	if (sdripc_dev->vaddr == NULL)
	{
		eprintk(dev, "DMA alloc fail\n");
		ret = -ENOMEM;
		goto error_dma_alloc;
	}

	shared_buffer_init(sdripc_dev);
	for(i=0;i<MAX_NUM_IPC_CLIENT;i++)
	{
		spin_lock_init(&sdripc_dev->ipclient_lock[i]);
	}
	mutex_init(&sdripc_dev->mboxsendMutex);

	/* register mailbox client */
	sdripc_dev->mbox_channel = sdripc_mbox_request_channel(pdev, sdripc_dev->mbox_name);
	if (sdripc_dev->mbox_channel == NULL)
	{
		eprintk(dev, "mbox_request_channel fail\n");
		ret = -EPROBE_DEFER;
		goto error_mbox_request_channel;
	}

	return 0;

error_mbox_request_channel:
	dma_free_coherent(dev, SHARED_BUFFER_SIZE, sdripc_dev->vaddr, sdripc_dev->paddr);
error_dma_alloc:
	sdripc_cdev_destory(dev, sdripc_dev);
error_create_cdev:
error_return:
	return ret;

}

static int sdripc_remove(struct platform_device *pdev)
{
	int ret=0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;

	sdripc_dev = (struct sdripc_device *)platform_get_drvdata(pdev);
	dev = &pdev->dev;

	/* unregister mailbox client */
	if(sdripc_dev->mbox_channel != NULL)
	{
		mbox_free_channel(sdripc_dev->mbox_channel);
		sdripc_dev->mbox_channel = NULL;
	}

	mutex_destroy(&sdripc_dev->mboxsendMutex);

	if(sdripc_dev->receive_vaddr != NULL)
	{
		iounmap((void *)sdripc_dev->receive_vaddr);
		sdripc_dev->receive_vaddr = NULL;
		sdripc_dev->receive_paddr = (dma_addr_t)NULL;
	}

	/* free DMA memory */
	if( sdripc_dev->vaddr != NULL )
	{
		dma_free_coherent(dev, SHARED_BUFFER_SIZE, sdripc_dev->vaddr, sdripc_dev->paddr);
		sdripc_dev->vaddr = NULL;
		sdripc_dev->paddr = 0x0;
	}

	/* destory char device and device node */
	ret = sdripc_cdev_destory(dev, sdripc_dev);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id sdripc_match[] = {
	{ .compatible = "telechips,sdr_ipc" },
	{},
};
MODULE_DEVICE_TABLE(of, sdripc_match);
#endif

static struct platform_driver sdripc_driver = {
	.driver = {
		.name = "sdripc",
#ifdef CONFIG_OF
		.of_match_table = sdripc_match,
#endif
	},
	.probe  = sdripc_probe,
	.remove = sdripc_remove,
};
module_platform_driver(sdripc_driver);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC SDR IPC driver");
MODULE_LICENSE("GPL");
