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

#ifndef char_t
typedef char char_t;
#endif

#ifndef long_t
typedef long long_t;
#endif

#define LOG_TAG ("SDRIPC")
static int32_t sdripc_verbose_mode=1;
#define eprintk(dev, msg, ...)	((void)dev_err(dev, "[ERROR][%s][%s]: " pr_fmt(msg), (const char_t *)LOG_TAG, __FUNCTION__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	((void)dev_warn(dev, "[WARN][%s][%s]: " pr_fmt(msg), (const char_t *)LOG_TAG, __FUNCTION__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	((void)dev_info(dev, "[INFO][%s][%s]: " pr_fmt(msg), (const char_t *)LOG_TAG, __FUNCTION__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	{ if(sdripc_verbose_mode == 1) { (void)dev_info(dev, "[INFO][%s][%s]: " pr_fmt(msg), (const char_t *)LOG_TAG, __FUNCTION__, ##__VA_ARGS__); } }

#define SUBCORE_MBOX_NAME ("sdr-ipc-a53")
#define MAX_NUM_IPC_CLIENT (16)
#define SHARED_BUFFER_SIZE (1024*512)

/* PCM buffer size : (48k*2ch*16bit)*2sec */
#define SHARED_PCM_BUFFER_SIZE (384000)

struct sdripc_receive_list {
	struct tcc_mbox_data data;
	struct list_head queue;
};

struct ipclient_t {
	int32_t client_index;            /* initial value should be (-1) */
	spinlock_t  rx_queue_lock;
	struct list_head rx_queue;
	int32_t rx_queue_count;
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
	const char_t *device_name;
	const char_t *mbox_name;
	const char_t *mbox_id_string;
	struct mbox_chan *mbox_channel;

	char_t *vaddr;              /* Holds a virtual address to send */
	dma_addr_t paddr;           /* Holds a physical address to send */
	int32_t shared_head_offset; /* Offset to be saved */

	char_t *receive_vaddr;      /* Holds a virtual address to be received */
	dma_addr_t receive_paddr;   /* Holds a physical address to be received */

	struct mutex mboxsendMutex;	/* mailbos send mutex */
	struct ipclient_t *ipclient_list[MAX_NUM_IPC_CLIENT];
	spinlock_t ipclient_lock[MAX_NUM_IPC_CLIENT];

	char_t *vaddr_pcm;
	dma_addr_t paddr_pcm;        /* Holds a physical address of pcm buffer */
};

/* get client index from IPC data */
static int32_t get_ipclient_index(char_t *buf, int32_t size)
{
	int32_t index;

	if((buf!=NULL)&&(size>4))
	{
		index = (int32_t)((uint8_t)buf[4]&(uint8_t)0x0F);
	}
	else
	{
		index = -1;
	}

	return index;
}

static int32_t get_ipclient(struct sdripc_device *sdripc_dev, int32_t index, struct ipclient_t **p_ipclient)
{
	//warn//struct device *dev = sdripc_dev->dev;
	int32_t ret=0;

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

static int32_t register_ipclient(struct sdripc_device *sdripc_dev, int32_t index, struct ipclient_t *ipclient)
{
	struct device *dev;
	int32_t ret=0;

	if(sdripc_dev==NULL)
	{
		return (-EINVAL);
	}

	dev = sdripc_dev->dev;

	if( (index<0) || (index>=MAX_NUM_IPC_CLIENT) )
	{
		return (-EINVAL);
	}

	if( ipclient->client_index != (-1) )
	{
		/* given index is already in used */
		return (-EBUSY);
	}

	if( sdripc_dev->ipclient_list[index] != NULL )
	{
		/* given index is already in used */
		return (-EBUSY);
	}

	sdripc_dev->ipclient_list[index] = ipclient;
	ipclient->client_index = index;

	dprintk(dev,"register ipclient[%p] idx[%d]\n", ipclient, index);

	#if 0 // debug
	{
		int32_t i;
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

static int32_t unregister_ipclient(struct sdripc_device *sdripc_dev, struct ipclient_t *ipclient)
{
	struct device *dev;
	int32_t ret=0;

	if( sdripc_dev == NULL )
	{
		return (-EINVAL);
	}

	dev = sdripc_dev->dev;

	if( (ipclient->client_index<0) || (ipclient->client_index>=MAX_NUM_IPC_CLIENT) )
	{
		/* invalid_index */
		return (-EINVAL);
	}

	dprintk(dev,"unregister ipclient[%p] idx[%d]\n", ipclient,ipclient->client_index);
	sdripc_dev->ipclient_list[ipclient->client_index] = NULL;
	ipclient->client_index = -1;

	#if 0 // debug
	{
		int32_t i;
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

static int32_t sdripc_rxqueue_init(struct ipclient_t *ipclient)//(struct sdripc_device *sdripc_dev)
{
	int32_t ret=0;
	//warn//struct sdripc_device *sdripc_dev=ipclient->sdripc_dev;
	//warn//struct device *dev = sdripc_dev->dev;

	INIT_LIST_HEAD(&ipclient->rx_queue);
	spin_lock_init(&ipclient->rx_queue_lock);
	ipclient->rx_queue_count=0;

	return ret;
}

static int32_t sdripc_rxqueue_push(struct ipclient_t *ipclient, struct tcc_mbox_data *mbox_data)
{
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	ulong flags;
	int32_t ret=0;

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

static int32_t sdripc_rxqueue_get_count(struct ipclient_t *ipclient)
{
	return ipclient->rx_queue_count;
}

static int32_t sdripc_rxqueue_get(struct ipclient_t *ipclient, struct tcc_mbox_data *mbox_data)
{
	//warn//struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	//warn//struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	ulong flags;
	int32_t ret = 0;

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

static int32_t sdripc_rxqueue_pop(struct ipclient_t *ipclient)
{
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	ulong flags;
	int32_t ret = 0;

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

static int32_t sdripc_rxqueue_deinit(struct ipclient_t *ipclient)
{
	struct sdripc_device *sdripc_dev = ipclient->sdripc_dev;
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list = NULL;
    struct sdripc_receive_list *receive_list_tmp = NULL;
	ulong flags;
	int32_t ret = 0;

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


static int32_t shared_buffer_init(struct sdripc_device *sdripc_dev)
{
	//warning//struct device *dev = sdripc_dev->dev;
	int32_t ret = 0;

	sdripc_dev->shared_head_offset = 0;

	return ret;
}

static int32_t shared_buffer_copy_from_user(struct sdripc_device *sdripc_dev, const char_t __user *data, size_t size)
{
	struct device *dev = sdripc_dev->dev;
	//warning//int32_t ret = 0;
	ulong result;
	char_t *shared_buf_vaddr;
	int32_t start_offset;
	int32_t next_start_offset;

	if( (sdripc_dev->shared_head_offset+(int32_t)size)>SHARED_BUFFER_SIZE )
	{
		/* rollback shared_buffer */
		sdripc_dev->shared_head_offset = 0;
	}

	shared_buf_vaddr = sdripc_dev->vaddr + sdripc_dev->shared_head_offset;
	start_offset = sdripc_dev->shared_head_offset;

	result = copy_from_user(shared_buf_vaddr, data, size);
	if (result != (ulong)0)
	{
		eprintk(dev, "copy_from_user failed: %ld\n", result);
		return (-1);
	}

	/* Align address to 8 byte */
	next_start_offset = start_offset + (int32_t)size;
	sdripc_dev->shared_head_offset = (int32_t)(((uint32_t)next_start_offset+(8-1))&(~(uint32_t)(8-1)));

	return start_offset;
}

static void sdripc_mbox_receive_message(struct mbox_client *client, void *message)
{
	struct platform_device *pdev;
	struct sdripc_device *sdripc_dev;
	struct device *dev;
	struct tcc_mbox_data *mbox_data;
	int32_t ret;
	int32_t client_index;
	struct ipclient_t *ipclient;
	int32_t start_offset;
	int32_t dataSize;

	if(client==NULL)
	{
		return;
	}

	if(message==NULL)
	{
		return;
	}

	pdev = to_platform_device(client->dev);
	sdripc_dev = platform_get_drvdata(pdev);
	dev = client->dev;
	mbox_data = (struct tcc_mbox_data *)message;

	#if 0
	dprintk(dev, "cmd[%08X][%08X][%08X] [%08X][%08X]\n",
		mbox_data->cmd[0], mbox_data->cmd[1], mbox_data->cmd[2], mbox_data->cmd[3], mbox_data->cmd[4], mbox_data->cmd[5]);
	#endif

	if(sdripc_dev->receive_paddr==(dma_addr_t)0)
	{
		dma_addr_t receive_paddr;
		memcpy((char_t*)&receive_paddr, &mbox_data->cmd[4], sizeof(dma_addr_t));
		sdripc_dev->receive_vaddr = (char_t *)ioremap_nocache(receive_paddr, SHARED_BUFFER_SIZE);
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
		memcpy((char_t*)&receive_paddr, &mbox_data->cmd[4], sizeof(dma_addr_t));
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
	start_offset = (int32_t)mbox_data->cmd[1];
	dataSize = (int32_t)mbox_data->cmd[2];
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

static struct mbox_chan *sdripc_mbox_request_channel(struct platform_device *pdev, const char_t *channel_name)
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
		client->rx_callback = &sdripc_mbox_receive_message;
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


static uint32_t sdripc_poll( struct file *filp, poll_table *wait)
{
	struct ipclient_t *ipclient;
	struct sdripc_device *sdripc_dev;
	//warn//struct device *dev;
	uint32_t mask;
	int32_t rx_queue_count;

	if(filp==NULL)
	{
		return 0;
	}

	ipclient=(struct ipclient_t *)filp->private_data;
	if(ipclient==NULL)
	{
		return 0;
	}
	sdripc_dev = ipclient->sdripc_dev;
	if(sdripc_dev==NULL)
	{
		return 0;
	}
	//dev = sdripc_dev->dev;

	poll_wait(filp, &ipclient->event_waitq, wait);
	spin_lock(&sdripc_dev->ipclient_lock[ipclient->client_index]);
	mask = 0;
	rx_queue_count = sdripc_rxqueue_get_count(ipclient);
	if(rx_queue_count>0)
	{
		mask |= (uint32_t)POLLIN|(uint32_t)POLLRDNORM;
	}
	spin_unlock(&sdripc_dev->ipclient_lock[ipclient->client_index]);

	return mask;
}

static ssize_t sdripc_write(struct file *filp, const char_t __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t ret = -1;
	struct ipclient_t *ipclient;
	struct sdripc_device *sdripc_dev;
	//warn//struct device *dev;
	struct tcc_mbox_data mbox_data;
	int32_t mbox_result = 0;
	int32_t shared_start_offset;

	if(filp==NULL)
	{
		return (-EINVAL);
	}

	if(buf==NULL)
	{
		return (-EINVAL);
	}

	if(count==(size_t)0)
	{
		return 0;
	}

	ipclient = (struct ipclient_t *)filp->private_data;
	if(ipclient==NULL)
	{
		return (-ENODEV);
	}
	sdripc_dev = ipclient->sdripc_dev;
	if(sdripc_dev==NULL)
	{
		return (-ENODEV);
	}
	//dev = sdripc_dev->dev;

	mutex_lock(&sdripc_dev->mboxsendMutex);
	{
		/* copy data to share_buffer */
		shared_start_offset = shared_buffer_copy_from_user(sdripc_dev, buf, count);
		if(shared_start_offset >= 0)
		{
			/* cmd[0]: message type. TDB later if needed */
			mbox_data.cmd[0] = 0;
			/* cmd[1]: shared buffer start offset */
			mbox_data.cmd[1] = (uint32_t)shared_start_offset;
			/* cmd[2]: data size */
			mbox_data.cmd[2] = (uint32_t)count;
			/* cmd[3]: not used */
			/* cmd[4-5]: physical address of shared buffer */
			memcpy(&mbox_data.cmd[4], (char_t*)&sdripc_dev->paddr, sizeof(dma_addr_t));
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
				ret = (ssize_t)count;
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

static ssize_t sdripc_read(struct file *filp, char_t __user *buf, size_t count, loff_t *f_pos)
{
	struct ipclient_t *ipclient;
	struct sdripc_device *sdripc_dev;
	struct device *dev;
	struct tcc_mbox_data mbox_data;

	int32_t ret;
	//int32_t type;
	int32_t start_offset;
	size_t  size=0;

	if(filp==NULL)
	{
		return (-EINVAL);
	}

	if(buf==NULL)
	{
		return (-EINVAL);
	}

	if(count==(size_t)0)
	{
		return 0;
	}

	ipclient = (struct ipclient_t *)filp->private_data;
	if(ipclient==NULL)
	{
		return (-ENODEV);
	}
	sdripc_dev = ipclient->sdripc_dev;
	if(sdripc_dev==NULL)
	{
		return (-ENODEV);
	}
	dev = sdripc_dev->dev;

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
		char_t *pSrc;
		char_t *pDst;
		ulong result;

		#if 0
		dprintk(dev, "cmd[%08X][%08X][%08X]/[%08X][%08X]\n",
			mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2], mbox_data.cmd[4], mbox_data.cmd[5]);
		#endif

		/* cmd[0]: message type. TDB later if needed */
		/* cmd[1]: shared buffer start offset */
		start_offset = (int32_t)mbox_data.cmd[1];
		/* cmd[2]: data size */
		size = (size_t)mbox_data.cmd[2];

		if (count < size)
		{
			dprintk(dev, "buffer not enough : [%d]\n", (-ENOBUFS));
			return (-ENOBUFS);
		}

		pSrc = sdripc_dev->receive_vaddr + start_offset;
		pDst = buf;
		result = copy_to_user(pDst, pSrc, size);
		if (result != (ulong)0)
		{
			eprintk(dev, "copy_to_user failed : %d\n", (int32_t)result);
			return (-EFAULT);
		}

		sdripc_rxqueue_pop(ipclient);
	}

	return (ssize_t)size;
}

static int32_t sdripc_open(struct inode *inode, struct file *filp)
{
	int32_t ret=0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;
	struct ipclient_t *ipclient;

	if(inode==NULL)
	{
		return (-EINVAL);
	}

	if(filp==NULL)
	{
		return (-EINVAL);
	}

	sdripc_dev = container_of(inode->i_cdev, struct sdripc_device, cdev);
	dev = sdripc_dev->dev;

	dprintk(dev, "enter\n");

	/* allocate ipclient instance */
	ipclient = devm_kzalloc(dev, sizeof(struct ipclient_t), GFP_KERNEL);
	if (ipclient==NULL)
	{
		return (-ENOMEM);
	}

	filp->private_data = ipclient;
	ipclient->sdripc_dev = sdripc_dev;

	sdripc_rxqueue_init(ipclient);
	init_waitqueue_head(&ipclient->event_waitq);

	ipclient->client_index=(-1);

	dprintk(dev, "ipclient=%p\n", ipclient);
	dprintk(dev, "exit %d\n", ret);
	return ret;
}

static int32_t sdripc_close(struct inode *inode, struct file *filp)
{
	int32_t ret=0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;
	struct ipclient_t *ipclient;
	int32_t client_index;

	if(inode==NULL)
	{
		return (-EINVAL);
	}

	if(filp==NULL)
	{
		return (-EINVAL);
	}

	sdripc_dev = container_of(inode->i_cdev, struct sdripc_device, cdev);
	dev = sdripc_dev->dev;
	dprintk(dev, "enter\n");

	ipclient = filp->private_data;
	if(ipclient!=NULL)
	{
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
	}

	dprintk(dev, "exit %d\n", ret);
	return ret;
}

static long_t sdripc_ioctl(struct file *filp, uint32_t cmd, ulong arg)
{
	long_t ret = -1;
	struct ipclient_t *ipclient;
	struct sdripc_device *sdripc_dev;
	struct device *dev;

	if(filp != NULL)
	{
		ipclient = (struct ipclient_t *)filp->private_data;
		if( ipclient != NULL)
		{
			sdripc_dev = ipclient->sdripc_dev;
			if( sdripc_dev != NULL )
			{
				dev = sdripc_dev->dev;

				switch(cmd)
				{
					case IOCTL_SDRPIC_SET_ID:
						{
							int32_t index = (int32_t)arg;
							/* register ipc client */
							spin_lock(&sdripc_dev->ipclient_lock[index]);
							ret = register_ipclient(sdripc_dev, index, ipclient);
							spin_unlock(&sdripc_dev->ipclient_lock[index]);
						}
						break;
					case IOCTL_SDRPIC_GET_SHM_BUFFER:
						{
							sdripc_get_shm_param shm_param;

							if(arg == (ulong)0)
							{
								ret = -EINVAL;
								return ret;
							}

							shm_param.phyaddr =  (uint64_t)sdripc_dev->paddr_pcm;
							shm_param.size    =  (int32_t)SHARED_PCM_BUFFER_SIZE;

							if( copy_to_user((void*)arg, &shm_param, sizeof(shm_param)) == (ulong)0 )
							{
								ret = 0;
							}
							else
							{
								ret =-EINVAL;
							}
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

static int32_t sdripc_cdev_create(struct device *dev, struct sdripc_device *sdripc_dev)
{
	int32_t ret=0;

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
		ret = (int32_t)PTR_ERR(sdripc_dev->class);
		eprintk(dev, "class_create error %d\n", ret);
		goto error_class_create;
	}

	/* Create a device node */
	sdripc_dev->dev = device_create(sdripc_dev->class, dev, sdripc_dev->devnum, sdripc_dev, sdripc_dev->device_name);
	if (IS_ERR(sdripc_dev->dev))
	{
		ret = (int32_t)PTR_ERR(sdripc_dev->dev);
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

static int32_t sdripc_cdev_destory(struct device *dev, struct sdripc_device *sdripc_dev)
{
	device_destroy(sdripc_dev->class, sdripc_dev->devnum);
	class_destroy(sdripc_dev->class);
	cdev_del(&sdripc_dev->cdev);
	unregister_chrdev_region(sdripc_dev->devnum, 1);

	return 0;
}

static int32_t sdripc_probe(struct platform_device *pdev)
{
	int32_t ret=0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;
	int32_t i;

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
	if (ret!=0)
	{
		goto error_create_cdev;
	}

	/* allocate IPC Data buffer from DMA memory */
	sdripc_dev->vaddr = dma_alloc_coherent(dev, SHARED_BUFFER_SIZE, &sdripc_dev->paddr, GFP_KERNEL);
	if (sdripc_dev->vaddr == NULL)
	{
		eprintk(dev, "DMA alloc fail\n");
		ret = -ENOMEM;
		goto error_dma_alloc;
	}
	dprintk(dev, "Alloc IPC data paddr[%llX], vaddr[%p]\n", sdripc_dev->paddr, sdripc_dev->vaddr);

	shared_buffer_init(sdripc_dev);

	/* allocate PCM buffer from DMA memory */
	if( memcmp(sdripc_dev->mbox_name, SUBCORE_MBOX_NAME, (sizeof(SUBCORE_MBOX_NAME)) ) ==  0 )
	{
		/* only at A53 core */
		sdripc_dev->vaddr_pcm = dma_alloc_coherent(dev, SHARED_PCM_BUFFER_SIZE, &sdripc_dev->paddr_pcm, GFP_KERNEL);
		if (sdripc_dev->vaddr_pcm == NULL)
		{
			eprintk(dev, "DMA alloc fail\n");
			ret = -ENOMEM;
			return ret;
		}
		dprintk(dev, "Alloc PCM_BUFFER paddr[%llX], vaddr[%p]\n", sdripc_dev->paddr_pcm, sdripc_dev->vaddr_pcm);
	}

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

static int32_t sdripc_remove(struct platform_device *pdev)
{
	int32_t ret=0;
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
		sdripc_dev->receive_paddr = 0;
	}

	/* free PCM Buffer memory */
	if( sdripc_dev->vaddr_pcm != NULL )
	{
		dma_free_coherent(dev, SHARED_PCM_BUFFER_SIZE, sdripc_dev->vaddr_pcm, sdripc_dev->paddr_pcm);
		sdripc_dev->vaddr_pcm = NULL;
		sdripc_dev->paddr_pcm = 0x0;
	}

	/* free IPC Data buffer memory */
	if( sdripc_dev->vaddr != NULL )
	{
		dma_free_coherent(dev, SHARED_BUFFER_SIZE, sdripc_dev->vaddr, sdripc_dev->paddr);
		sdripc_dev->vaddr = NULL;
		sdripc_dev->paddr = 0x0;
	}

	/* destory char device and device node */
	sdripc_cdev_destory(dev, sdripc_dev);

	return ret;
}

#if defined(CONFIG_PM)
static int32_t sdripc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int32_t ret = 0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;

	sdripc_dev = (struct sdripc_device *)platform_get_drvdata(pdev);
	if( sdripc_dev != NULL )
	{
		dev = &pdev->dev;

		dprintk(dev,"[%s]\n",__func__);

		/* unregister mailbox client */
		if(sdripc_dev->mbox_channel != NULL)
		{
			mbox_free_channel(sdripc_dev->mbox_channel);
			sdripc_dev->mbox_channel = NULL;
		}
	}

	return ret;
}

static int32_t sdripc_resume(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct sdripc_device *sdripc_dev;
	struct device *dev;

	sdripc_dev = (struct sdripc_device *)platform_get_drvdata(pdev);
	if( sdripc_dev != NULL )
	{
		dev = &pdev->dev;
		dprintk(dev,"[%s]\n",__func__);

		/* register mailbox client */
		sdripc_dev->mbox_channel = sdripc_mbox_request_channel(pdev, sdripc_dev->mbox_name);
		if (sdripc_dev->mbox_channel == NULL)
		{
			eprintk(dev, "mbox_request_channel fail\n");
			ret = -EPROBE_DEFER;
		}
	}

	return ret;
}
#endif

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
#if defined(CONFIG_PM)
	.suspend = sdripc_suspend,
	.resume = sdripc_resume,
#endif
};
module_platform_driver(sdripc_driver);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC SDR IPC driver");
MODULE_LICENSE("GPL");
