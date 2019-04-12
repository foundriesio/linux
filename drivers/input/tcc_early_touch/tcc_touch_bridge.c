#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/delay.h>

#define TOUCH_BRIDGE_NAME       "tcc_touch_bridge"
#define TOUCH_BRIDGE_MINOR		0

static int debug = 0;

#define dprintk(msg...)                                \
{                                                      \
	if (debug)  	                                   \
        printk(KERN_DEBUG "(D)" msg);		           \
}

typedef struct _mbox_list {
	struct tcc_mbox_data 	msg;
	struct list_head    	queue;
}mbox_list;

typedef struct _mbox_queue {
	struct kthread_worker  kworker;
	struct task_struct     *kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             queue_lock;
	struct list_head       queue;
}mbox_queue;

static struct class *tcc_touch_bridge_class;
static int tcc_touch_bridge_major_num;
static const char * tcc_touch_bridge_mbox_name;
struct mbox_chan *tcc_touch_bridge_channel;
struct tcc_mbox_data sendMsg;
struct mbox_client client;
mbox_queue ttb_queue;


static void ttb_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
	unsigned long flags;
	mbox_list *ttb_list = NULL;
	dprintk("Event. Dev: %s, Type: %d, Code: %d, Value: %d\n",
				       dev_name(&handle->dev->dev), type, code, value);

	switch(code)
	{
		case 53:
			sendMsg.cmd[0] = value;
		break;
		case 54:
			sendMsg.cmd[1] = value;
		break;
		case 57:
			sendMsg.cmd[2] = value;
		break;
		default:
			sendMsg.cmd[3] = 0;
			sendMsg.cmd[4] = 0;
			sendMsg.cmd[5] = 0;
			sendMsg.cmd[6] = 0;
			sendMsg.data_len = 0;
		break;
	}
	if(type == 0 && code == 0 && value == 0)
	{
		ttb_list = kzalloc(sizeof(mbox_list), GFP_ATOMIC);
		if(ttb_list != NULL)
		{
			memcpy(ttb_list->msg.cmd, sendMsg.cmd, 3);
			spin_lock_irqsave(&ttb_queue.queue_lock, flags);
			list_add_tail(&ttb_list->queue, &ttb_queue.queue);
			spin_unlock_irqrestore(&ttb_queue.queue_lock, flags);
			kthread_queue_work(&ttb_queue.kworker, &ttb_queue.pump_messages);
		}
	}
}

static int ttb_connect(struct input_handler *handler, struct input_dev *dev,
					 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;

	handle->name = "tcc_tb";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;
	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;


	dprintk("Connected device: %s (%s at %s)\n",
		dev_name(&dev->dev), dev->name ?: "unknown", dev->phys ?: "unknown");

	return 0;

err_unregister_handle:
	printk(KERN_ERR "Connected device: Err1");
	input_unregister_handle(handle);
err_free_handle:
	printk(KERN_ERR "Connected device: Err2");
	kfree(handle);
	return error;
}

static void ttb_disconnect(struct input_handle *handle)
{
	dprintk("Disconnected device: %s\n",
       dev_name(&handle->dev->dev));

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}
static const struct input_device_id ttb_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = 	BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
		{ .driver_info = 1 },	/* Matches all devices */
		{ },					/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, ttb_ids);

static struct input_handler ttb_handler = {
		.event =		ttb_event,
		.connect =		ttb_connect,
		.disconnect =	ttb_disconnect,
		.name =			"ttb",
		.id_table =		ttb_ids,
};

static void receive_message(struct mbox_client *client, void *message)
{
}

static void ttb_pump_messages(struct kthread_work *work)
{
	mbox_list *ttb_list, *ttb_list_tmp;
	unsigned long flags;

	spin_lock_irqsave(&ttb_queue.queue_lock, flags);
	list_for_each_entry_safe(ttb_list, ttb_list_tmp, &ttb_queue.queue, queue)
	{
		spin_unlock_irqrestore(&ttb_queue.queue_lock, flags);
		//send touch data to a7s
		(void)mbox_send_message(tcc_touch_bridge_channel, &sendMsg);
		mbox_client_txdone(tcc_touch_bridge_channel,0);
		spin_lock_irqsave(&ttb_queue.queue_lock, flags);
		list_del_init(&ttb_list->queue);
		kfree(ttb_list);
	}
	spin_unlock_irqrestore(&ttb_queue.queue_lock, flags);
}

static int tcc_touch_bridge_probe(struct platform_device *pdev)
{
	INIT_LIST_HEAD(&ttb_queue.queue);
	spin_lock_init(&ttb_queue.queue_lock);

	input_register_handler(&ttb_handler);

	tcc_touch_bridge_major_num = register_chrdev(0, TOUCH_BRIDGE_NAME, &ttb_handler);
	tcc_touch_bridge_class = class_create(THIS_MODULE, TOUCH_BRIDGE_NAME);
	device_create(tcc_touch_bridge_class, NULL, MKDEV(tcc_touch_bridge_major_num, TOUCH_BRIDGE_MINOR), NULL, TOUCH_BRIDGE_NAME);

	of_property_read_string(pdev->dev.of_node,"mbox-names", &tcc_touch_bridge_mbox_name);
	client.dev = &pdev->dev;
	client.rx_callback = receive_message;
	client.tx_done = NULL;
	client.tx_block = false;
	client.knows_txdone = false;
	client.tx_tout = 10;
	tcc_touch_bridge_channel = mbox_request_channel_byname(&client, tcc_touch_bridge_mbox_name);

	dprintk("Prove TCC_TB Device\n");
	kthread_init_worker(&ttb_queue.kworker);
	ttb_queue.kworker_task = kthread_run(kthread_worker_fn,&ttb_queue.kworker,"ttb_thread");
	if (IS_ERR(ttb_queue.kworker_task)) {
		printk(KERN_ERR "%s : failed to create message pump task\n", __func__);
		return -ENOMEM;
	}
	kthread_init_work(&ttb_queue.pump_messages, ttb_pump_messages);
	return 0;
}
static int tcc_touch_bridge_remove(struct platform_device * pdev)
{
	input_unregister_handler(&ttb_handler);
	kthread_flush_worker(&ttb_queue.kworker);
	kthread_stop(ttb_queue.kworker_task);
	mbox_free_channel(tcc_touch_bridge_channel);
	device_destroy(tcc_touch_bridge_class, MKDEV(tcc_touch_bridge_major_num, TOUCH_BRIDGE_MINOR));
	class_destroy(tcc_touch_bridge_class);
	unregister_chrdev(tcc_touch_bridge_major_num, TOUCH_BRIDGE_NAME);
	dprintk("Remove TCC_TB Device\n");
	return 0;
}

static struct platform_driver tcc_touch_bridge = {
	.probe	= tcc_touch_bridge_probe,
	.remove	= tcc_touch_bridge_remove,
	.driver	= {
		.name	= TOUCH_BRIDGE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init tcc_touch_bridge_init(void)
{
	return platform_driver_register(&tcc_touch_bridge);
}

static void __exit tcc_touch_bridge_exit(void)
{
	platform_driver_unregister(&tcc_touch_bridge);
}


module_init(tcc_touch_bridge_init);
module_exit(tcc_touch_bridge_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips Input driver bridge");
MODULE_LICENSE("GPL");
