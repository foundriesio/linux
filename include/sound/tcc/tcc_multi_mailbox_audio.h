/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/
#ifndef _TCC_MULTI_MAILBOX_AUDIO_H_
#define _TCC_MULTI_MAILBOX_AUDIO_H_

#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/slab.h>


#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include <sound/tcc/params/tcc_mbox_ak4601_codec_params.h>
#include <sound/tcc/params/tcc_mbox_audio_params.h>


//We should consider the number of queue (and worker thread)
//because there are many transmission about the position inform during pcm writing or reading at each device.
//The number of devices are totally 4 (3 tx and 1 rx).
//So, we estimates 5 queues (4 queues are for i/o device and 1 queue is for command.)
#define RX_QUEUE_DEFAULT_NAME	"mbox_audio_rx_"

#define RX_QUEUE_FOR_AUDIO_DEVICE_TX_0	    0  //hw0:0
#define RX_QUEUE_FOR_AUDIO_DEVICE_TX_1	    1  //hw0:1
#define RX_QUEUE_FOR_AUDIO_DEVICE_TX_2	    2  //hw0:2
#define RX_QUEUE_FOR_AUDIO_DEVICE_RX		3  //hw0:3
#define RX_QUEUE_FOR_COMMAND				4  //for command from a53 to a7s
#define RX_QUEUE_COUNT						(RX_QUEUE_FOR_COMMAND + 1)

#define TX_MAX_REPLY_COUNT					5
#define MAX_USER_QUEUE_SIZE					20

//TODO : use driver variable? or use extern variable?
struct mbox_audio_backup_data_t {
    unsigned int ak4601_data[AK4601_VIRTUAL_INDEX_COUNT];
	//struct mbox_audio_effect_backup_data_t am3d_power_base_data[EFFECT_TYPE_COUNT];
};

/* data header for cmd[0] for other drivers */
struct mbox_audio_data_header_t {
    unsigned short usage;
	unsigned short cmd_type;
	unsigned short tx_instance;
	unsigned short msg_size; //TODO : we should choose the size of msg_size (1. unsigned int, 2. unsigned short)
};

/* replied data for other drivers */
struct mbox_audio_tx_reply_data_t {
	unsigned short cmd_type;
	unsigned short msg_size;
	unsigned int msg[AUDIO_MBOX_MESSAGE_SIZE];
	
	int updated;
};

typedef void (*mbox_audio_work_handler)(void *device_info, struct tcc_mbox_data *data, int handle);

// client must set "set_callback" to receive data to set params
struct mbox_audio_client_t {
	void (*set_callback)(void *client_data, unsigned int *msg, unsigned short msg_size, unsigned short cmd_type);
	
    void *client_data;
	int is_used;
	int is_client_user; // 0: kernel (use callback), 1: user (use polling)
};

// tx for reply processing
struct mbox_audio_tx_t {
	spinlock_t lock;
	struct list_head list;
	atomic_t seq;
	atomic_t wakeup;
	wait_queue_head_t wait;

	int handle;
	int reserved; // to avoid abnormal operation for multi-thread
	struct mbox_audio_tx_reply_data_t reply;
};

// rx worker thread with queue
struct mbox_audio_rx_t {
	struct kthread_worker kworker;
	struct task_struct *task;
	struct kthread_work work;
	spinlock_t lock;
	struct list_head list;
	atomic_t seq;
	
	const char *name;
    int handle;

	mbox_audio_work_handler handler;
};

struct mbox_audio_user_queue_t {
	struct list_head list;
	wait_queue_head_t uq_wait;
	struct mutex uq_lock;

	atomic_t uq_size;
};

// for wrapping to queue item
struct mbox_audio_cmd_t {
	struct list_head list;
	struct tcc_mbox_data mbox_data;
	struct mbox_audio_device *adev;
};

// root device structure
struct mbox_audio_device {
    struct platform_device * pdev;
    struct device *dev;
    struct cdev c_dev;
    struct class *class;
    struct mbox_chan *mbox_ch;
    
    dev_t dev_num;
    const char *dev_name; //for debug inform
    const char *mbox_name; //for debug inform

	unsigned short chip_id;
    
    struct mutex lock;
    unsigned int users; //user count open driver
    
    int mbox_audio_ready; //after initialize, this value is changed as DRV_STATUS_READY
    
    struct mbox_audio_tx_t tx[TX_MAX_REPLY_COUNT];
    struct mbox_audio_rx_t rx[RX_QUEUE_COUNT];

    struct mbox_audio_user_queue_t user_queue; //polling queue
    
    //for set callback to other devices
    struct mbox_audio_client_t client[MBOX_AUDIO_CMD_TYPE_MAX];

	struct mbox_audio_backup_data_t backup_data;
 };


//all datas to set/get for restore
//TODO : make it extern?
//struct mbox_audio_codec_data {
    
    
//};

struct mbox_audio_device *get_global_audio_dev(void);
int tcc_mbox_audio_send_command(struct mbox_audio_device *audio_dev, struct mbox_audio_data_header_t *header, unsigned int *msg, struct mbox_audio_tx_reply_data_t *reply);
#endif//_TCC_MULTI_MAILBOX_AUDIO_H_

