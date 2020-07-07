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

#define LOG_TAG "SDRIPC"
int sdripc_verbose_mode=1;
#define eprintk(dev, msg, ...)	((void)dev_err(dev, "[ERROR][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define wprintk(dev, msg, ...)	((void)dev_warn(dev, "[WARN][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define iprintk(dev, msg, ...)	((void)dev_info(dev, "[INFO][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__))
#define dprintk(dev, msg, ...)	{ if(sdripc_verbose_mode == 1) { (void)dev_info(dev, "[INFO][%s][%s]: " pr_fmt(msg), (const char *)LOG_TAG,__FUNCTION__, ##__VA_ARGS__); } }

#define SHARED_MEM_SIZE (1024*512)

static DECLARE_WAIT_QUEUE_HEAD(event_waitq);

struct sdripc_receive_list {
	struct tcc_mbox_data data;
	struct list_head queue;
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

	unsigned char *vaddr;             // Holds a virtual address to DMA.
	dma_addr_t paddr;                 // Holds a physical address to DMA.
	int shared_head_pos;

	unsigned char *receive_vaddr;
	dma_addr_t receive_paddr;

	spinlock_t  rx_queue_lock;
	struct list_head rx_queue;
	//wait_queue_head_t event_waitq;
	int recv_event;
	struct mutex mboxMutex;		/* mbox mutex */
	struct file *filp;
};

static int sdripc_rxqueue_init(struct sdripc_device *sdripc_dev)
{
	//warn//struct device *dev = sdripc_dev->dev;

	INIT_LIST_HEAD(&sdripc_dev->rx_queue);
	spin_lock_init(&sdripc_dev->rx_queue_lock);
	return 0;
}

static int sdripc_rxqueue_push(struct sdripc_device *sdripc_dev, struct tcc_mbox_data *mbox_data)
{
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

	spin_lock_irqsave(&sdripc_dev->rx_queue_lock, flags);
	list_add_tail(&receive_list->queue, &sdripc_dev->rx_queue);
	spin_unlock_irqrestore(&sdripc_dev->rx_queue_lock, flags);

	return 0;
}

static int sdripc_rxqueue_get(struct sdripc_device *sdripc_dev, struct tcc_mbox_data *mbox_data)
{
	//warn//struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&sdripc_dev->rx_queue_lock, flags);
	if(list_empty(&sdripc_dev->rx_queue)==0)
	{
		receive_list = list_first_entry(&sdripc_dev->rx_queue, struct sdripc_receive_list, queue);
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
	spin_unlock_irqrestore(&sdripc_dev->rx_queue_lock, flags);

	return ret;
}

static int sdripc_rxqueue_pop(struct sdripc_device *sdripc_dev)
{
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&sdripc_dev->rx_queue_lock, flags);
	if(list_empty(&sdripc_dev->rx_queue)==0)
	{
		receive_list = list_first_entry(&sdripc_dev->rx_queue, struct sdripc_receive_list, queue);
		if(receive_list!=NULL)
		{
			list_del(&receive_list->queue);
			devm_kfree(dev, receive_list);
		}
	}
	spin_unlock_irqrestore(&sdripc_dev->rx_queue_lock, flags);
	return ret;
}

static int sdripc_rxqueue_deinit(struct sdripc_device *sdripc_dev)
{
	struct device *dev = sdripc_dev->dev;
	struct sdripc_receive_list *receive_list;
    struct sdripc_receive_list *receive_list_tmp;
	unsigned long flags;
	int ret = 0;

	/* flush rxqueue */
	spin_lock_irqsave(&sdripc_dev->rx_queue_lock, flags);
    list_for_each_entry_safe(receive_list, receive_list_tmp, &sdripc_dev->rx_queue, queue)
    {
        list_del_init(&receive_list->queue);
        devm_kfree(dev, receive_list);
    }
	spin_unlock_irqrestore(&sdripc_dev->rx_queue_lock, flags);
	return ret;
}


static int shared_buffer_init(struct sdripc_device *sdripc_dev)
{
	//warning//struct device *dev = sdripc_dev->dev;
	int ret = 0;

	sdripc_dev->shared_head_pos = 0;

	return ret;
}

#if 0
static int shared_buffer_read(struct sdripc_device *sdripc_dev, unsigned char *data, size_t size)
{
	//struct device *dev = sdripc_dev->dev;
	int ret = 0;


	return ret;
}
#endif

static int shared_buffer_copy_from_user(struct sdripc_device *sdripc_dev, const char __user *data, int size)
{
	struct device *dev = sdripc_dev->dev;
	//warning//int ret = 0;
	unsigned long result;
	unsigned char *shared_buf_vaddr;
	int start_pos;

	if( (sdripc_dev->shared_head_pos+(int)size)>SHARED_MEM_SIZE )
	{
		/* rollback shared_buffer */
		sdripc_dev->shared_head_pos = 0;
	}

	shared_buf_vaddr = sdripc_dev->vaddr + sdripc_dev->shared_head_pos;
	start_pos = sdripc_dev->shared_head_pos;

	result = copy_from_user(shared_buf_vaddr, data, size);
	if (result != 0)
	{
		eprintk(dev, "copy_from_user failed: %ld\n", result);
		return (-1);
	}

	sdripc_dev->shared_head_pos += (int)size;
	return start_pos;
}

void sdripc_mbox_receive_message(struct mbox_client *client, void *message)
{
	struct platform_device *pdev = to_platform_device(client->dev);
	struct sdripc_device *sdripc_dev = platform_get_drvdata(pdev);
	struct device *dev = client->dev;
	struct tcc_mbox_data *mbox_data = (struct tcc_mbox_data *)message;
	int ret;
	int type;
	int start_pos;
	int size;
	dma_addr_t receive_paddr;

	/* check if ipc client is opened */
	if(sdripc_dev->filp == NULL)
	{
		return;
	}

	//dprintk(dev, "cmd[%08X][%08X][%08X] [%08X][%08X]\n",
	//	mbox_data->cmd[0], mbox_data->cmd[1], mbox_data->cmd[2], mbox_data->cmd[4], mbox_data->cmd[5]);
	type = mbox_data->cmd[0];
	start_pos = mbox_data->cmd[1];
	size = mbox_data->cmd[2];

	memcpy((char*)&receive_paddr, &mbox_data->cmd[4], sizeof(dma_addr_t));

	/* remap received buffer(physical address) to virtual address */
	if(receive_paddr != sdripc_dev->receive_paddr )
	{
		if(sdripc_dev->receive_paddr!=0)
		{
			/* ignore */
			eprintk(dev, "change paddr prev[%llX] to new[%llX]\n", sdripc_dev->receive_paddr, receive_paddr);
			dprintk(dev, "cmd[%08X][%08X][%08X] [%08X][%08X]\n",
				mbox_data->cmd[0], mbox_data->cmd[1], mbox_data->cmd[2], mbox_data->cmd[4], mbox_data->cmd[5]);
			return;
		}

		if(sdripc_dev->receive_vaddr != NULL)
		{
			iounmap((void *)sdripc_dev->receive_vaddr);
			sdripc_dev->receive_vaddr = NULL;
		}

		sdripc_dev->receive_vaddr = (unsigned char*)ioremap_nocache(receive_paddr, SHARED_MEM_SIZE);
		if (sdripc_dev->receive_vaddr == NULL)
		{
			eprintk(dev, "Fail ioremap_nocache\n");
			return;
		}
		sdripc_dev->receive_paddr = receive_paddr;
		dprintk(dev, "Remap paddr paddr[%llX] to vaddr[%p]\n", sdripc_dev->receive_paddr, sdripc_dev->receive_vaddr);
	}

	ret = sdripc_rxqueue_push(sdripc_dev, mbox_data);

	sdripc_dev->recv_event = 1;
	wake_up(&event_waitq);

	#if 0
	dprintk(dev, "received : type[%d] pos[%X/%d] size[%d] paddr[%llX]\n", type, start_pos, start_pos, size, receive_paddr);
	{
		unsigned char *pData = sdripc_dev->receive_vaddr + start_pos;

		dprintk(dev, " offset[0x%08x] %02X %02X %02X %02X\n", 0, pData[0], pData[1], pData[2], pData[3]);
		dprintk(dev, " offset[0x%08x] %02X %02X %02X %02X\n", size-4,
			pData[size-4], pData[size-3], pData[size-2], pData[size-1] );
	}
	{
		unsigned char *pData = sdripc_dev->receive_vaddr + start_pos;
		dprintk(dev, "received : pos[%04X] size[%d] data[%02X][%02X%02X..%02X%02X][%02X]\n", start_pos, size,
			(start_pos==0)?0:*(pData-1),
			pData[0], pData[1], pData[size-2], pData[size-1],
			((start_pos+size)>=SHARED_MEM_SIZE)?0:pData[size]
		);
	}
	#endif
}

#if 0 //warning//
static int sdripc_mbox_send_message(struct ipctest_device *ipc_dev, struct tcc_mbox_data * ipc_msg)
{
	dev_info(ipc_dev->dev, "[%s] Entry\n", __func__);

	return 0;
}
#endif

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
	struct sdripc_device *sdripc_dev = (struct sdripc_device *)filp->private_data;
	//warn//struct device *dev = sdripc_dev->dev;
	unsigned int mask = 0;

	//poll_wait(filp, &sdripc_dev->event_waitq, wait);
	poll_wait(filp, &event_waitq, wait);
	if(sdripc_dev->recv_event)
	{
		mask |= POLLIN | POLLRDNORM;
		sdripc_dev->recv_event = 0;
	}
	return mask;
}

static ssize_t sdripc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t ret=0;
	struct sdripc_device *sdripc_dev = (struct sdripc_device *)filp->private_data;
	struct device *dev = sdripc_dev->dev;
	int mbox_result = 0;
	struct tcc_mbox_data mbox_data;
	int shared_start_pos;

	if(buf==NULL)
	{
		ret = (-EINVAL);
		return ret;
	}

	if(count==0)
	{
		return 0;
	}

	/* copy data to share_queue_buffer */
	shared_start_pos = shared_buffer_copy_from_user(sdripc_dev, buf, (int)count);

	/* data_fifo won't be used */
	mbox_data.data_len = 0;
	/* message type */
	mbox_data.cmd[0] = 0;
	/* shared buffer start position */
	mbox_data.cmd[1] = shared_start_pos;
	/* data size */
	mbox_data.cmd[2] = (int)count;
	/* physical address of shared memory */
	memcpy(&mbox_data.cmd[4], (char*)&sdripc_dev->paddr, sizeof(dma_addr_t));

	mutex_lock(&sdripc_dev->mboxMutex);
	mbox_result = mbox_send_message(sdripc_dev->mbox_channel, &mbox_data);
	if(mbox_result < 0 )
	{
		//eprintk(dev, "mbox_send_message failed: %d\n", mbox_result);
		ret = (-ETIMEDOUT);
	}
	else
	{
		ret = 0;
	}
	mutex_unlock(&sdripc_dev->mboxMutex);

	return ret;
}

static ssize_t sdripc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct sdripc_device *sdripc_dev = (struct sdripc_device *)filp->private_data;
	struct device *dev = sdripc_dev->dev;
	struct tcc_mbox_data mbox_data;

	int ret;
	int total_size=0;
	int type;
	int start_pos;
	int size;

#if 0
	{
		struct sdripc_receive_list *receive_list;
		struct sdripc_receive_list *receive_list_temp;

		list_for_each_entry_safe(receive_list, receive_list_temp, &sdripc_dev->rx_queue, queue)
		{
			dprintk(dev, "cmd[%08X]\n", receive_list->data.cmd[1]);
		};
	}
#endif
	if (sdripc_dev->receive_vaddr==NULL)
	{
		dprintk(dev, "receive_vaddr is NULL\n");
		return 0;
	}

	//do{
		ret = sdripc_rxqueue_get(sdripc_dev, &mbox_data);
		if(ret==0)
		{
			unsigned char *pSrc;
			unsigned char *pDst;
			int result;

			type = mbox_data.cmd[0];
			start_pos = mbox_data.cmd[1];
			size = mbox_data.cmd[2];

			//if(count<(total_size+size))
			//{
			//	break;
			//}

			//dprintk(dev, "cmd[%08X][%08X][%08X]/[%08X][%08X]\n",
			//	mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2], mbox_data.cmd[4], mbox_data.cmd[5]);

			#if 1
			pSrc = sdripc_dev->receive_vaddr + start_pos;
			pDst = buf + total_size;
			//dprintk(dev, "read %p to %p, size=%d\n", pSrc, pDst, size);
			result = copy_to_user(pDst, pSrc, size);
			if (result != 0)
			{
				eprintk(dev, "copy_to_user failed: %d\n", result);
			}
			#endif
			total_size += size;

			ret = sdripc_rxqueue_pop(sdripc_dev);
		}
	//}while(ret==0 && (count>total_size));

	return (ssize_t)total_size;
}

static int sdripc_open(struct inode *inode, struct file *filp)
{
	int ret=0;
	struct sdripc_device *sdripc_dev = container_of(inode->i_cdev, struct sdripc_device, cdev);
	struct device *dev = sdripc_dev->dev;

	dprintk(dev, "enter\n");
	filp->private_data = sdripc_dev;
	shared_buffer_init(sdripc_dev);
	ret = sdripc_rxqueue_init(sdripc_dev);

	//init_waitqueue_head(&sdripc_dev->event_waitq);
	sdripc_dev->recv_event = 0;
	mutex_init(&sdripc_dev->mboxMutex);

	/* register ipc client */
	sdripc_dev->filp = filp;
	dprintk(dev, "return %d\n", ret);
	return ret;
}

static int sdripc_close(struct inode *inode, struct file *filp)
{
	int ret=0;
	struct sdripc_device *sdripc_dev = container_of(inode->i_cdev, struct sdripc_device, cdev);
	struct device *dev = sdripc_dev->dev;

	dprintk(dev, "enter\n");

	/* unregister ipc client */
	sdripc_dev->filp = NULL;
	sdripc_rxqueue_deinit(sdripc_dev);

	mutex_destroy(&sdripc_dev->mboxMutex);

	if(sdripc_dev->receive_vaddr != NULL)
	{
		iounmap((void *)sdripc_dev->receive_vaddr);
		sdripc_dev->receive_vaddr = NULL;
		sdripc_dev->receive_paddr = (dma_addr_t)NULL;
	}

	dprintk(dev, "return %d\n", ret);
	return ret;
}

static long sdripc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
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
	sdripc_dev->vaddr = dma_alloc_coherent(dev, SHARED_MEM_SIZE, &sdripc_dev->paddr, GFP_KERNEL);
	if (sdripc_dev->vaddr == NULL)
	{
		eprintk(dev, "DMA alloc fail\n");
		ret = -ENOMEM;
		goto error_dma_alloc;
	}

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
	dma_free_coherent(dev, SHARED_MEM_SIZE, sdripc_dev->vaddr, sdripc_dev->paddr);
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

	/* free DMA memory */
	if( sdripc_dev->vaddr != NULL )
	{
		dma_free_coherent(dev, SHARED_MEM_SIZE, sdripc_dev->vaddr, sdripc_dev->paddr);
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
