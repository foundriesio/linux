/*
 * Copyright (C) 2016 Telechips
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mailbox/mailbox-tcc.h>

#include "tcc_mailbox_cmd.h"
#include "tcc_mailbox.h"
#include "tcc_sap.h"

/*****************************************************************************
 * Defines
 ******************************************************************************/
#define MBOX_MAX_MSG_LEN        TCC_MBOX_MAX_MSG
#define MBOX_BYTES_PER_LINE	    16
#define MBOX_HEXDUMP_LINE_LEN	((MBOX_BYTES_PER_LINE * 4) + 2)
#define MBOX_HEXDUMP_MAX_LEN	(MBOX_HEXDUMP_LINE_LEN * (MBOX_MAX_MSG_LEN / MBOX_BYTES_PER_LINE))

#define CMD_TIMEOUT		        msecs_to_jiffies(1000)

/*****************************************************************************
 * structures
 ******************************************************************************/
struct tcc_audio_mbox {
    struct mbox_client cl;
    struct mbox_chan *mbox_ch;
    //struct completion tx_complete;
    //bool async;
    struct mutex lock;
    spinlock_t list_lock;
    struct tcc_mbox_msg	*msg;
    char *rx_buf;

    unsigned int count;
};

/*****************************************************************************
 * Variables
 ******************************************************************************/
static LIST_HEAD(mbox_inst_list);


/*****************************************************************************
 * Functions
 ******************************************************************************/
struct ca7_mbox_instance *tcc_sap_get_mbox(struct tcc_audio_mbox *audio_mbox, unsigned int id) {
    struct ca7_mbox_instance *mbox_inst, *entry;
    unsigned long flags;

    mbox_inst = NULL;
    spin_lock_irqsave(&audio_mbox->list_lock, flags);
    list_for_each_entry(entry, &mbox_inst_list, elem) {
		if (entry->id == id) {
			mbox_inst = entry;
            break;
        }
	}
    spin_unlock_irqrestore(&audio_mbox->list_lock, flags);

    return mbox_inst;
}

static void mbox_message_handler(struct tcc_audio_mbox *audio_mbox, struct tcc_mbox_msg *msg) {
    struct ca7_mbox_instance *mbox_inst;
    unsigned char command;
    unsigned int id;
    unsigned int *param = (unsigned int *)msg->message;

    command = param[0];
    id = param[1];

    if (command == CA7_MBOX_NOTI_AUDIO_DRV) {
        extern audio_data_queue_t audio_data_queue[];
        audio_data_queue_t *paudio_data_queue;
        cmdmsg *pcmdmsg;

        memcpy(audio_mbox->rx_buf, msg->message, msg->msg_len);
        pcmdmsg = (cmdmsg *)audio_mbox->rx_buf;

        paudio_data_queue = (audio_data_queue_t *)&audio_data_queue[pcmdmsg->dev];

        paudio_data_queue->ret = pcmdmsg->param.retpar.ret;
        paudio_data_queue->done = 1;
        wake_up(&paudio_data_queue->wait);
        return;
    }

    mbox_inst = tcc_sap_get_mbox(audio_mbox, id);
    if (!mbox_inst) {
        printk("no vaild id %d, cmd 0x%02X\n", id, command);
        return;
    }

    if (msg->msg_len > sizeof(struct ca7_mbox_msg)) {
        printk("invaild message size (%d) - id %d, cmd 0x%02X\n", msg->msg_len, id, command);
        return;
    }

    memcpy(&mbox_inst->reply, msg->message, msg->msg_len);

    if (mbox_inst->cmd.command != command && command != CA7_MBOX_NOTI_ERROR) {
        printk("something wrong with mbox_inst - id %d, cmd 0x%02X, waiting 0x%02X\n", id, command, mbox_inst->cmd.command);
        return;
    }

    mbox_inst->done = 1;
    wake_up(&mbox_inst->wait);
    return;
}

static void tcc_sap_mbox_message_received(struct mbox_client *client, void *message) {
    struct tcc_audio_mbox *audio_mbox = container_of(client, struct tcc_audio_mbox, cl);
    struct tcc_mbox_msg *msg = (struct tcc_mbox_msg *)message;
    //printk("-- In %s ---\n", __func__);

    if (msg->message) {
        //print_hex_dump_bytes("Client: Received [API]: ", DUMP_PREFIX_ADDRESS,
        //		     msg->message, MBOX_MAX_MSG_LEN);
        if (!audio_mbox->rx_buf) {
            dev_err(client->dev, "rx_buf alloc fail!\n");
            return;
        }

        //memcpy(audio_mbox->rx_buf, msg->message, msg->msg_len);
        mbox_message_handler(audio_mbox, msg);
    } else {
        printk("error no message\n");
    }
}

ssize_t tcc_sap_mbox_send(void *mailbox, unsigned int *msg, size_t count)
{
    int ret;
    struct tcc_audio_mbox *audio_mbox = (struct tcc_audio_mbox *)mailbox;

    if (!audio_mbox->mbox_ch) {
        dev_err(audio_mbox->cl.dev, "Channel cannot do Tx\n");
        return -EINVAL;
    }

    if (count > MBOX_MAX_MSG_LEN) {
        dev_err(audio_mbox->cl.dev,
            "Message length %zd greater than max allowed %d\n",
            count, MBOX_MAX_MSG_LEN);
        return -EINVAL;
    }

    mutex_lock(&audio_mbox->lock);

    audio_mbox->msg = kzalloc(sizeof(struct tcc_mbox_msg), GFP_KERNEL);
    if (!audio_mbox->msg) {
        dev_err(audio_mbox->cl.dev, "Failed to alloc tx-msg buffer\n");
        mutex_unlock(&audio_mbox->lock);
        return -ENOMEM;
    }

    audio_mbox->msg->cmd = MBOX_CMD(MBOX_DEV_TEST, 0);
    audio_mbox->msg->msg_len = count;
    audio_mbox->msg->trans_type = DATA_MBOX;
    memcpy(audio_mbox->msg->message, msg, audio_mbox->msg->msg_len);

    ret = mbox_send_message(audio_mbox->mbox_ch, audio_mbox->msg);
    if (ret < 0) {
        dev_err(audio_mbox->cl.dev, "Failed to send message via mailbox\n");
    }

    kfree(audio_mbox->msg);

    mutex_unlock(&audio_mbox->lock);

    return ret < 0 ? ret : count;
}

static void tcc_sap_mbox_message_sent(struct mbox_client *client, void *message, int r)
{
    //struct tcc_audio_mbox *audio_mbox = container_of(client, struct tcc_audio_mbox, cl);
    //struct tcc_mbox_msg *msg = (struct tcc_mbox_msg *)message;

    //printk("-- In %s ---\n", __func__);
    if (r) {
        dev_warn(client->dev,
                "Client: Message could not be sent: %d\n", r);
    }
    //else
    //	dev_info(client->dev,
    //		 "Client: Message sent\n");

    //if (client->tx_block == false) {
    //    complete(&audio_mbox->tx_complete);
    //}
}

int tcc_sap_send_command(struct ca7_mbox_instance *mbox_inst, unsigned int cmd, unsigned int size, int wait) {
    struct ca7_mbox_msg *msg = (struct ca7_mbox_msg *)&mbox_inst->cmd;

    msg->id = mbox_inst->id;
    msg->command = cmd;
    size += sizeof(unsigned int) * 2;

    mbox_inst->done = 0;

    if (tcc_sap_mbox_send(mbox_inst->mbox, (unsigned int *)&mbox_inst->cmd, size) != size) {
        printk("[%d] ERROR MailBox Send CMD(0x%02X) Error\n", mbox_inst->id, cmd);
        return ERROR_MAILBOX_SEND;
    }

    if (wait) {
        if (wait_event_timeout(mbox_inst->wait, mbox_inst->done == 1, CMD_TIMEOUT) == 0) {
            printk("[%d] ERROR MailBox CMD(0x%02X) TIME OUT\n", mbox_inst->id, cmd);
            return ERROR_MAILBOX_TIMEOUT;
        }
    }

    return NO_ERROR;
}

struct ca7_mbox_instance* tcc_sap_get_new_id(void *mailbox) {
    int err;
    unsigned long flags;
    struct tcc_audio_mbox *audio_mbox = (struct tcc_audio_mbox *)mailbox;
    struct ca7_mbox_instance *mbox_inst;

    mbox_inst = devm_kzalloc(audio_mbox->cl.dev, sizeof(struct ca7_mbox_instance), GFP_KERNEL);
    if (!mbox_inst) {
        return NULL;
    }

    mbox_inst->mbox = mailbox;
    init_waitqueue_head(&mbox_inst->wait);

    spin_lock_irqsave(&audio_mbox->list_lock, flags);
    audio_mbox->count++;
    audio_mbox->count = audio_mbox->count & 0x0000FFFF;
    mbox_inst->id = audio_mbox->count + MBOX_REPLY_ID_BASE; // temporarily make a id
    list_add(&mbox_inst->elem, &mbox_inst_list);
    spin_unlock_irqrestore(&audio_mbox->list_lock, flags);

    err = tcc_sap_send_command(mbox_inst, CA7_MBOX_CMD_GET_NEW_INSTANCE_ID, 0, 1);
    if (err != NO_ERROR) {
        printk("[%s] mbox error %d\n", __func__, err);
    } else if (((MBOXReply *)&mbox_inst->reply.params)->mError) {
        printk("[%s] There is no room to add new device.\n", __func__);
        err = ERROR_TOO_MANY_ID;
    }

    if (err) {
        spin_lock_irqsave(&audio_mbox->list_lock, flags);
        list_del(&mbox_inst->elem);
        spin_unlock_irqrestore(&audio_mbox->list_lock, flags);

        devm_kfree(audio_mbox->cl.dev, mbox_inst);
        return NULL;
    }

    /* Update the communication id with the new id. */
    mbox_inst->id = ((MBOXReplyGetNewId *)&mbox_inst->reply.params)->mNewID;

    return mbox_inst;
}

int tcc_sap_remove_component(struct ca7_mbox_instance *mbox_inst) {
    int err;
    unsigned long flags;
    struct tcc_audio_mbox *audio_mbox = (struct tcc_audio_mbox *)mbox_inst->mbox;

    err = tcc_sap_send_command(mbox_inst, CA7_MBOX_CMD_REMOVE_INSTANCE, 0, 1);
    if (err != NO_ERROR) {
        printk("[%s] mbox error %d, id %d\n", __func__, err, mbox_inst->id);
    }

    printk("[sap] id %d removed\n", mbox_inst->id);

    spin_lock_irqsave(&audio_mbox->list_lock, flags);
    mbox_inst->id = -1;
    list_del(&mbox_inst->elem);
    spin_unlock_irqrestore(&audio_mbox->list_lock, flags);

    devm_kfree(audio_mbox->cl.dev, mbox_inst);

    return NO_ERROR;
}

int tcc_sap_init_ca7(void) {
    return NO_ERROR;
}

int tcc_sap_deinit_ca7(void) {
    return NO_ERROR;
}

void* tcc_sap_init_mbox(struct platform_device *pdev) {

    struct tcc_audio_mbox *audio_mbox = devm_kzalloc(&pdev->dev, sizeof(*audio_mbox), GFP_KERNEL);
    if (!audio_mbox) {
        return ERR_PTR(-ENOMEM);
    }

    audio_mbox->rx_buf = devm_kzalloc(&pdev->dev, MBOX_MAX_MSG_LEN, GFP_KERNEL);
    if (!audio_mbox->rx_buf) {
        return ERR_PTR(-ENOMEM);
    }

    audio_mbox->cl.dev = &pdev->dev;
    audio_mbox->cl.tx_block = true;
    audio_mbox->cl.rx_callback = tcc_sap_mbox_message_received;
    audio_mbox->cl.tx_done = audio_mbox->cl.tx_block == true ? NULL : tcc_sap_mbox_message_sent;
    audio_mbox->cl.knows_txdone = false;
    audio_mbox->cl.tx_tout = 500;
    //init_completion(&audio_mbox->tx_complete);

    audio_mbox->mbox_ch = mbox_request_channel_byname(&audio_mbox->cl, "tcc-mbox-audio");
    if (IS_ERR(audio_mbox->mbox_ch)) {
        dev_warn(&pdev->dev, "Failed to request %s channel\n", "tcc-mbox-audio");
        return NULL;
    }

    mutex_init(&audio_mbox->lock);

    spin_lock_init(&audio_mbox->list_lock);

    return (void *)audio_mbox;
}

int tcc_sap_close_mbox(void *arg) {
    struct tcc_audio_mbox *audio_mbox = (struct tcc_audio_mbox *)arg;

    mutex_destroy(&audio_mbox->lock);
    if (audio_mbox->mbox_ch) {
        mbox_free_channel(audio_mbox->mbox_ch);
    }

    return NO_ERROR;
}
