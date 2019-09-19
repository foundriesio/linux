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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_asrc_m2m_pcm.h"

#define CHECK_OVERRUN	(1)
#define CHECK_ASRC_M2M_ELAPSED_TIME	(0)

#undef asrc_m2m_pcm_dbg
#if 0
#define asrc_m2m_pcm_dbg(f, a...)	printk("<ASRC M2M PCM>" f, ##a)
#define asrc_m2m_pcm_dbg_id(id, f, a...)	printk("<ASRC M2M PCM-%d>" f, id, ##a)
#else
#define asrc_m2m_pcm_dbg(f, a...)
#define asrc_m2m_pcm_dbg_id(id, f, a...)	//printk("<ASRC M2M PCM-%d>" f, id, ##a)
#endif
#define asrc_m2m_pcm_dbg_err(f, a...)	printk("<ASRC M2M PCM>" f, ##a)
#define asrc_m2m_pcm_dbg_id_err(id, f, a...)	printk("<ASRC M2M PCM-%d>" f, id, ##a)

static void tcc_asrc_m2m_pcm_stream_reset(struct asrc_m2m_pcm_stream *strm, bool prepare);
static int tcc_asrc_m2m_pcm_asrc_stop(struct snd_pcm_substream *substream);
static char tcc_is_flag_update(struct asrc_m2m_pcm_stream *strm, char is_flag, char mode);
static int tcc_ptr_update_thread_for_capture(void *data);
static int tcc_appl_ptr_check_thread_for_play(void *data);

//#define TAIL_DEBUG
#ifdef TAIL_DEBUG
ssize_t cap_tail=0;
ssize_t play_tail=0;
#endif

static const struct snd_pcm_hardware tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_TYPE_MAX] = {
	[TCC_ASRC_M2M_7_1CH] = {
		.info = (SNDRV_PCM_INFO_MMAP
				| SNDRV_PCM_INFO_MMAP_VALID
				| SNDRV_PCM_INFO_INTERLEAVED
				| SNDRV_PCM_INFO_BLOCK_TRANSFER
				| SNDRV_PCM_INFO_PAUSE
				| SNDRV_PCM_INFO_RESUME),

		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.rates        = SNDRV_PCM_RATE_8000_192000,
		.rate_min     = 8000,
		.rate_max     = 192000,
		.channels_min = 2,
		.channels_max = 8,
		.period_bytes_min = MIN_PERIOD_BYTES,
		.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
		.periods_min      = MIN_PERIOD_CNT,
		.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
		.buffer_bytes_max = MAX_BUFFER_BYTES,
		.fifo_size = 16,
	},
	[TCC_ASRC_M2M_STEREO] = {
		.info = (SNDRV_PCM_INFO_MMAP
				| SNDRV_PCM_INFO_MMAP_VALID
				| SNDRV_PCM_INFO_INTERLEAVED
				| SNDRV_PCM_INFO_BLOCK_TRANSFER
				| SNDRV_PCM_INFO_PAUSE
				| SNDRV_PCM_INFO_RESUME),

		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.rates        = SNDRV_PCM_RATE_8000_192000,
		.rate_min     = 8000,
		.rate_max     = 192000,
		.channels_min = 2,
		.channels_max = 2,
		.period_bytes_min = MIN_PERIOD_BYTES,
		.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
		.periods_min      = MIN_PERIOD_CNT,
		.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
		.buffer_bytes_max = MAX_BUFFER_BYTES,
		.fifo_size = 16,
	},
};

#ifdef FOOTPRINT_LINKED_LIST
static void tcc_asrc_footprint_init(List *list)
{
	list->head = NULL;
//	list->current = NULL;
	list->tail = NULL;
	list->list_len = 0;
}

//From tail insert
static int tcc_asrc_footprint_insert(List *list, unsigned int pos, ssize_t size)
{
	Node *new_node=NULL;
	new_node = (Node *)kzalloc(sizeof(Node), GFP_KERNEL);
	if(!new_node) {
		printk("[%s][%d]ERROR!!\n", __func__, __LINE__);
        return -EFAULT;
	}

	new_node->print_pos = pos;
	new_node->input_byte = size;
	new_node->next = NULL;

	if(list->list_len > 0) {
		list->tail->next = new_node;
	} else {
		list->head = new_node;
	}

	list->tail = new_node;
	(list->list_len)++;

	return list->list_len;
}

//From head delete.
static int tcc_asrc_footprint_delete(List *list, bool all)
{
	Node *dummy=NULL;
	if(all == false) {
		if(list->list_len <= 0) {
			printk("[%s][%d]There is no footprint (len=%d)\n", __func__, __LINE__, list->list_len);
			return -EFAULT;
		} else if(list->list_len == 1) {
			kfree(list->tail);
			(list->list_len)--;
		} else {
			dummy = list->head;
			list->head = dummy->next;
			kfree(dummy);
			(list->list_len)--;
		}
	} else {	//delete all
		while(list->list_len > 0) {
			if(list->list_len == 1) {
				kfree(list->tail);
				(list->list_len)--;
			} else {
				dummy = list->head;
				list->head = dummy->next;
				kfree(dummy);
				(list->list_len)--;
			}
		}
	}
	return list->list_len;
}
#else
static void tcc_asrc_footprint_init(struct footprint *list)
{
	list->head = 0;
	list->tail = 0;
	list->list_len = 0;
}

//From tail insert
static int tcc_asrc_footprint_insert(struct footprint *list, unsigned int pos, ssize_t size)
{
	if((list->list_len > 0) && (list->list_len < FOOTPRINT_LENGTH)) {
		list->tail ++;
	} else if(list->list_len >= FOOTPRINT_LENGTH) {
		printk("[%s][%d]There is no footprint (len=%d)\n", __func__, __LINE__, list->list_len);
	}
	if(list->tail >= FOOTPRINT_LENGTH) {
		list->tail = list->tail - FOOTPRINT_LENGTH;
	}
	list->print_pos[list->tail] = pos;
	list->input_byte[list->tail] = size;
	list->list_len ++;

	return list->list_len;
}

//From head delete.
static int tcc_asrc_footprint_delete(struct footprint *list, bool all)
{
	if(all == false) {
		if(list->list_len <= 0) {
			printk("[%s][%d]There is no footprint (len=%d)\n", __func__, __LINE__, list->list_len);
			return -EFAULT;
		} else if(list->list_len == 1) {
			goto list_delete_all;	
		} else {
			list->head ++;
			if(list->head >= FOOTPRINT_LENGTH) {
				list->head = list->head - FOOTPRINT_LENGTH;
			}
			list->list_len--;
		}
	} else {	//delete all
list_delete_all:
		memset(list->print_pos, 0x0, FOOTPRINT_LENGTH);
		memset(list->input_byte, 0x0, FOOTPRINT_LENGTH);
		tcc_asrc_footprint_init(list);
	}
	return list->list_len;
}
#endif
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
static int tcc_asrc_m2m_pcm_set_action_to_mbox(struct mbox_audio_device *mbox_audio_dev, struct snd_pcm_substream *substream, int cmd)
{
    struct mbox_audio_data_header_t mbox_header;

    const struct snd_pcm *pcm;
    unsigned int device, action_value;

    unsigned int mbox_msg[MBOX_MSG_SIZE_FOR_ACTION] = {0,};

    if (substream == NULL) {
        pr_err("%s - substream is NULL\n", __func__);
        return -EFAULT;
    }

    pcm = substream->pcm;
    device = pcm->card->number << 4 | (pcm->device & 0x0F);

    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
    case SNDRV_PCM_TRIGGER_RESUME:
    case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
        action_value = PCM_VALUE_ACTION_START;
        break;
    case SNDRV_PCM_TRIGGER_STOP:
    case SNDRV_PCM_TRIGGER_SUSPEND:
    case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
        action_value = PCM_VALUE_ACTION_STOP;
        break;
    default :
        pr_err("%s - invalid command\n", __func__);
        return -EINVAL;
    }

    mbox_header.usage = MBOX_AUDIO_USAGE_SET;
    mbox_header.cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
    mbox_header.msg_size = MBOX_MSG_SIZE_FOR_ACTION;
    mbox_header.tx_instance = 0;

    mbox_msg[0] = PCM_ACTION;
    mbox_msg[1] = device;
    mbox_msg[2] = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? PCM_VALUE_DIRECTON_TX : PCM_VALUE_DIRECTON_RX;
    mbox_msg[3] = action_value;
    mbox_msg[4] = PCM_VALUE_MUX_SOURCE_MAIN;

    tcc_mbox_audio_send_message(mbox_audio_dev, &mbox_header, mbox_msg, NULL);

    asrc_m2m_pcm_dbg("[%s] mbox message : device = 0x%02x, action = 0x%01x\n", __func__, mbox_msg[1], mbox_msg[2]);

    return 0;
}

static int tcc_asrc_m2m_pcm_set_hw_params_to_mbox(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm, int stream)
{
    struct mbox_audio_data_header_t mbox_header;

    const struct snd_pcm *pcm;
    unsigned int device;
	dma_addr_t phy_address;

    unsigned int mbox_msg[MBOX_MSG_SIZE_FOR_PARAMS] = {0};

    if (asrc_m2m_pcm == NULL) {
        printk("%s - asrc_m2m_pcm is NULL\n", __func__);
        return -EFAULT;
    }

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		pcm = asrc_m2m_pcm->playback->asrc_substream->pcm;
		device = pcm->card->number << 4 | (pcm->device & 0x0F);
		phy_address = asrc_m2m_pcm->playback->middle->dma_buf->addr;
	} else {
		pcm = asrc_m2m_pcm->capture->asrc_substream->pcm;
		device = pcm->card->number << 4 | (pcm->device & 0x0F);
		phy_address = asrc_m2m_pcm->capture->middle->dma_buf->addr;
	}
	if (!phy_address) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s] fail to get phy addr\n", __func__);
		return -EFAULT;
	}
    //printk("[%s] phy_address = 0x%08x\n", __func__, phy_address);
    mbox_header.usage = MBOX_AUDIO_USAGE_SET;
    mbox_header.cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
    mbox_header.msg_size = MBOX_MSG_SIZE_FOR_PARAMS;
    mbox_header.tx_instance = 0;

    mbox_msg[0] = PCM_PARAMS;
    mbox_msg[1] = device;
    mbox_msg[2] = (stream == SNDRV_PCM_STREAM_PLAYBACK) ? PCM_VALUE_DIRECTON_TX : PCM_VALUE_DIRECTON_RX;
    mbox_msg[3] = (unsigned int) phy_address;
    mbox_msg[4] = (stream == SNDRV_PCM_STREAM_PLAYBACK) ? asrc_m2m_pcm->playback->middle->buffer_bytes : asrc_m2m_pcm->capture->middle->buffer_bytes;

    tcc_mbox_audio_send_message(asrc_m2m_pcm->mbox_audio_dev, &mbox_header, mbox_msg, NULL);

    asrc_m2m_pcm_dbg("[%s] mbox message : device = 0x%02x, tx_rx = 0x%01x, buffer address = 0x%x, buffer size = %u\n", __func__, mbox_msg[1], mbox_msg[2], mbox_msg[3], mbox_msg[4]);

    return 0;
}

static void playback_for_mbox_callback(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	struct snd_pcm_substream *substream = NULL;// = asrc_m2m_pcm->asrc_substream;
	unsigned int mid_buffer_bytes=0, mix_pos=0, head_pos=0, cur_pos=0;
	bool elapsed_en=false;
	ssize_t mix_byte=0;
	unsigned long flags;
	int len=0;

    if (asrc_m2m_pcm == NULL) {
        printk("%s - asrc_m2m_pcm is NULL\n", __func__);
		goto playback_callback_end;
    }

	if(asrc_m2m_pcm->playback->asrc_substream != NULL) {
		substream = asrc_m2m_pcm->playback->asrc_substream;
	} else {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ERROR!! asrc_m2m_pcm->asrc_substream is null!!\n");
		goto playback_callback_end;
	}

	mid_buffer_bytes = asrc_m2m_pcm->playback->middle->buffer_bytes;
	mix_pos = asrc_m2m_pcm->playback->middle->pre_pos;

	if(asrc_m2m_pcm->playback->middle->cur_pos_from_ipc != asrc_m2m_pcm->playback->middle->pre_pos_from_ipc) {
		if(asrc_m2m_pcm->playback->middle->pre_pos_from_ipc > asrc_m2m_pcm->playback->middle->cur_pos_from_ipc) {
			mix_byte = mid_buffer_bytes - asrc_m2m_pcm->playback->middle->pre_pos_from_ipc;
			mix_byte += asrc_m2m_pcm->playback->middle->cur_pos_from_ipc;
		} else {
			mix_byte = asrc_m2m_pcm->playback->middle->cur_pos_from_ipc - asrc_m2m_pcm->playback->middle->pre_pos_from_ipc;
		}
		//This line already be in the main function of mbox callback.
		//asrc_m2m_pcm->playback->middle->pre_pos_from_ipc = asrc_m2m_pcm->playback->middle->cur_pos_from_ipc;
	} else {
		mix_byte = 0;
	}

	mix_pos += mix_byte;
	if(mix_pos >= mid_buffer_bytes) {
		mix_pos = mix_pos - mid_buffer_bytes;
	}
/*	
	if(mix_byte > 1024) {
		printk("[%d]mix_byte=%d, mix_pos=%d, mid_buf_byte=%d\n", __LINE__, (unsigned int)mix_byte, mix_pos, mid_buffer_bytes);
		printk("[%s][%d]cur_pos=%d, pre_pos=%d\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->playback->middle->cur_pos_from_ipc, (unsigned int)asrc_m2m_pcm->playback->middle->pre_pos_from_ipc);
	}
*/	
	asrc_m2m_pcm->playback->middle->pre_pos = mix_pos;
	cur_pos = asrc_m2m_pcm->playback->middle->cur_pos;
	elapsed_en = false;
	while (1) {
		spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
		len = asrc_m2m_pcm->asrc_footprint->list_len;
		spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);
		if(len <= 0){
			//printk("[%s][%d]len=%d\n", __func__, __LINE__, len);
			break;
		}
#ifdef FOOTPRINT_LINKED_LIST
		head_pos = asrc_m2m_pcm->asrc_footprint->head->print_pos;
		mix_byte = asrc_m2m_pcm->asrc_footprint->head->input_byte;
#else
		head_pos = asrc_m2m_pcm->asrc_footprint->print_pos[asrc_m2m_pcm->asrc_footprint->head];
		mix_byte = asrc_m2m_pcm->asrc_footprint->input_byte[asrc_m2m_pcm->asrc_footprint->head];
#endif
		if(cur_pos > mix_pos) {
			if((head_pos <= mix_pos)||(head_pos >= cur_pos)) {
				asrc_m2m_pcm->playback->Bwrote += mix_byte;
				if(asrc_m2m_pcm->playback->Bwrote >= asrc_m2m_pcm->playback->src->buffer_bytes)
					asrc_m2m_pcm->playback->Bwrote = asrc_m2m_pcm->playback->Bwrote - asrc_m2m_pcm->playback->src->buffer_bytes;
				spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
				len = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, false);
				spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);
				elapsed_en = true;
			} else {
				break;
			}
		} else {
			if((head_pos <= mix_pos)&&(head_pos >= cur_pos)) {
				asrc_m2m_pcm->playback->Bwrote += mix_byte;
				if(asrc_m2m_pcm->playback->Bwrote >= asrc_m2m_pcm->playback->src->buffer_bytes)
					asrc_m2m_pcm->playback->Bwrote = asrc_m2m_pcm->playback->Bwrote - asrc_m2m_pcm->playback->src->buffer_bytes;
				spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
				len = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, false);
				spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);
				elapsed_en = true;
			} else {
				//printk("[%s][%d]cur_pos=%d, pre_pos=%d, head_pos=%d, len=%d, mix_byte=%d\n", __func__, __LINE__, cur_pos, mix_pos, head_pos, len, (unsigned int)mix_byte);
				break;
			}
		}
	}
playback_callback_end:
	if(elapsed_en == true) {
		snd_pcm_period_elapsed(substream);
		atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
		wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));
	} else {
		//This is for snd_pcm_drain
		if(substream->runtime->status->state == SNDRV_PCM_STATE_DRAINING) {
			while(len > 0) {
#ifdef FOOTPRINT_LINKED_LIST
				mix_byte = asrc_m2m_pcm->asrc_footprint->head->input_byte;
#else
				mix_byte = asrc_m2m_pcm->asrc_footprint->input_byte[asrc_m2m_pcm->asrc_footprint->head];
#endif

				asrc_m2m_pcm->playback->Bwrote += mix_byte;
				if(asrc_m2m_pcm->playback->Bwrote >= asrc_m2m_pcm->playback->src->buffer_bytes)
					asrc_m2m_pcm->playback->Bwrote = asrc_m2m_pcm->playback->Bwrote - asrc_m2m_pcm->playback->src->buffer_bytes;
				spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
				len = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, false);
				spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);
				if(substream) {
					snd_pcm_period_elapsed(substream);
				} else {
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "ERROR!! asrc_m2m_pcm->asrc_substream set to null during drain!!\n");
					break;
				}
			}
		}
	}
}

static void capture_for_mbox_callback(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	unsigned int read_pos=0;

	read_pos = asrc_m2m_pcm->capture->middle->cur_pos_from_ipc;

#if	(CHECK_OVERRUN == 1)
	if((asrc_m2m_pcm->capture->middle->cur_pos > asrc_m2m_pcm->capture->middle->pre_pos)
			&&(read_pos > asrc_m2m_pcm->capture->middle->pre_pos)&&(read_pos < asrc_m2m_pcm->capture->middle->cur_pos)) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "overrun?! pre_pos=%u, read_pos=%u, cur_pos=%u\n", asrc_m2m_pcm->capture->middle->pre_pos, read_pos, asrc_m2m_pcm->capture->middle->cur_pos);

	} else if((asrc_m2m_pcm->capture->middle->cur_pos < asrc_m2m_pcm->capture->middle->pre_pos)
			&&((read_pos > asrc_m2m_pcm->capture->middle->pre_pos)||(read_pos < asrc_m2m_pcm->capture->middle->cur_pos))) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "overrun?! pre_pos=%u, read_pos=%u, cur_pos=%u\n", asrc_m2m_pcm->capture->middle->pre_pos, read_pos, asrc_m2m_pcm->capture->middle->cur_pos);
	}
#endif

	asrc_m2m_pcm->capture->middle->cur_pos = read_pos;

	atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
	wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
}

static void tcc_asrc_m2m_pcm_mbox_callback(void *data, unsigned int *msg, unsigned short msg_size, unsigned short cmd_type)
{
    struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	//unsigned int cmd = msg[0];	//for debug
	unsigned int pos = msg[2];
	char Ctemp=0;

	if (asrc_m2m_pcm == NULL) {
		printk("[%s] Cannot get asrc_m2m_pcm driver data\n", __func__);
		return;
	}

    if ((cmd_type != asrc_m2m_pcm->playback->mbox_cmd_type) && (cmd_type != asrc_m2m_pcm->capture->mbox_cmd_type)) {
		printk("[%s] Invalid cmd type between msg[%u], playback[%u] and capture[%u]\n", __func__, cmd_type, asrc_m2m_pcm->playback->mbox_cmd_type, asrc_m2m_pcm->capture->mbox_cmd_type);
		return;
    }

    if (msg_size == AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE) {
        pos = msg[cmd_type];
    }
	if(cmd_type == asrc_m2m_pcm->playback->mbox_cmd_type) {	//For playback
	
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
		if(((Ctemp & IS_A7S_STARTED)!= 0)
				&&(asrc_m2m_pcm->playback->middle->cur_pos_from_ipc != pos)) {
			asrc_m2m_pcm->playback->middle->pre_pos_from_ipc = asrc_m2m_pcm->playback->middle->cur_pos_from_ipc;
			asrc_m2m_pcm->playback->middle->cur_pos_from_ipc = pos;
			playback_for_mbox_callback(asrc_m2m_pcm);
		}
	} else {	//For capture

		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
		if(((Ctemp & IS_A7S_STARTED)!= 0)
				&&(asrc_m2m_pcm->capture->middle->cur_pos_from_ipc != pos)) {
			asrc_m2m_pcm->capture->middle->pre_pos_from_ipc = asrc_m2m_pcm->capture->middle->cur_pos_from_ipc;
			asrc_m2m_pcm->capture->middle->cur_pos_from_ipc = pos;
			capture_for_mbox_callback(asrc_m2m_pcm);
		}
	}
    //printk("[%s] callback get! cmd = %d, device = %d, position = 0x%08x, cmd_type = %d, msg_size = %d\n", __func__,cmd, device, pos, cmd_type, msg_size);
	return;
}
#endif

static int tcc_asrc_m2m_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	struct task_struct *pkth_id=NULL;

	asrc_m2m_pcm_dbg("%s, id=%d\n", __func__, substream->pcm->device);

	if(asrc_m2m_pcm == NULL) {
		pr_err("%s - asrc_m2m_pcm is NULL\n", __func__);
		return -EFAULT;
	}

	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		if (asrc_m2m_pcm->playback->pair_id == ASRC_7_1CH_DEV_NUM) {  
			snd_soc_set_runtime_hwparams(substream, &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_7_1CH]);
		} else {
			snd_soc_set_runtime_hwparams(substream, &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_STEREO]);
		}
		asrc_m2m_pcm->playback->asrc_substream = substream;
		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->playback, false);

		tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_RESET);

		if(asrc_m2m_pcm->playback->first_open == false) {
			asrc_m2m_pcm->playback->first_open = true;
			pkth_id = (struct task_struct *) kthread_run(tcc_appl_ptr_check_thread_for_play, (void *)(asrc_m2m_pcm), "tcc_appl_ptr_check_thread_for_play");
			if(pkth_id != NULL) asrc_m2m_pcm->playback->kth_id = pkth_id;
		}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO

		if ((asrc_m2m_pcm->playback->mbox_cmd_type >= MBOX_AUDIO_CMD_TYPE_POSITION_MIN) 
			&& (asrc_m2m_pcm->playback->mbox_cmd_type <= MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			asrc_m2m_pcm->mbox_audio_dev = tcc_mbox_audio_register_set_kernel_callback((void*)asrc_m2m_pcm, tcc_asrc_m2m_pcm_mbox_callback, asrc_m2m_pcm->playback->mbox_cmd_type);
		}
#endif

	} else {
		if (asrc_m2m_pcm->capture->pair_id == ASRC_7_1CH_DEV_NUM) {  
			snd_soc_set_runtime_hwparams(substream, &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_7_1CH]);
		} else {
			snd_soc_set_runtime_hwparams(substream, &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_STEREO]);
		}
		asrc_m2m_pcm->capture->asrc_substream = substream;
		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->capture, false);

		tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_RESET);

		if(asrc_m2m_pcm->capture->first_open == false) {
			asrc_m2m_pcm->capture->first_open = true;
			pkth_id = (struct task_struct *) kthread_run(tcc_ptr_update_thread_for_capture, (void *)(asrc_m2m_pcm), "tcc_ptr_update_thread_for_capture");
			if(pkth_id != NULL) asrc_m2m_pcm->capture->kth_id = pkth_id;
		}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		if ((asrc_m2m_pcm->capture->mbox_cmd_type >= MBOX_AUDIO_CMD_TYPE_POSITION_MIN) 
			&& (asrc_m2m_pcm->capture->mbox_cmd_type <= MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) { 
			asrc_m2m_pcm->mbox_audio_dev = tcc_mbox_audio_register_set_kernel_callback((void*)asrc_m2m_pcm, tcc_asrc_m2m_pcm_mbox_callback, asrc_m2m_pcm->capture->mbox_cmd_type);
		}
#endif
	}

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);
    return 0;
}

static int tcc_asrc_m2m_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned long flags;
	int ret=0;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s] start\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		if ((asrc_m2m_pcm->mbox_audio_dev != NULL) 
			&& (asrc_m2m_pcm->playback->mbox_cmd_type >= MBOX_AUDIO_CMD_TYPE_POSITION_MIN) 
			&& (asrc_m2m_pcm->playback->mbox_cmd_type <= MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->playback->mbox_cmd_type);
		}
#endif
		atomic_set(&asrc_m2m_pcm->playback->wakeup, 0);

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->playback, false);
		spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
		if(asrc_m2m_pcm->asrc_footprint->list_len > 0) {
			ret = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, true);
		}
		tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);
		spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);

		if (asrc_m2m_pcm->playback->asrc_substream != NULL) { 
			asrc_m2m_pcm->playback->asrc_substream = NULL;
		}
	} else {
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		if ((asrc_m2m_pcm->mbox_audio_dev != NULL) 
			&& (asrc_m2m_pcm->capture->mbox_cmd_type >= MBOX_AUDIO_CMD_TYPE_POSITION_MIN) 
			&& (asrc_m2m_pcm->capture->mbox_cmd_type <= MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->capture->mbox_cmd_type);
		}
#endif
		atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->capture, false);
		if (asrc_m2m_pcm->capture->asrc_substream != NULL) { 
			asrc_m2m_pcm->capture->asrc_substream = NULL;
		}
	}

/*
	if (asrc_m2m_pcm->kth_id != NULL) { 
		asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
		kthread_stop(asrc_m2m_pcm->kth_id);
		asrc_m2m_pcm->kth_id = NULL;
	}
*/
	asrc_m2m_pcm_dbg("[%s][%d] end\n", __func__, __LINE__);
    return 0;
}

static int tcc_asrc_m2m_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
    struct snd_pcm_runtime *runtime = substream->runtime;

	asrc_m2m_pcm_dbg("%s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma, runtime->dma_area, runtime->dma_addr, runtime->dma_bytes);
}

static int tcc_asrc_m2m_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_asrc_param_t *pasrc_param;// = asrc_m2m_pcm->asrc_m2m_param;

	int channels = params_channels(params);
	size_t rate = params_rate(params);
	size_t period_bytes = params_period_bytes(params);
	size_t buffer_bytes = params_buffer_bytes(params);
	snd_pcm_format_t format = params_format(params);
	bool multi_ch = false;
//	bool mono_mode = (channels == 1) ? true : false;
	enum tcc_asrc_drv_bitwidth_t asrc_bitwidth;
	enum tcc_asrc_drv_ch_t asrc_channels;
	uint64_t ratio_shift22;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "format : 0x%08x\n", format);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "channels : %d\n", channels);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "period_bytes : %u\n", period_bytes);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "buffer_bytes : %u\n", buffer_bytes);

	memset(substream->dma_buffer.area, 0, buffer_bytes);

	asrc_bitwidth = (format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ASRC_24BIT : TCC_ASRC_16BIT;

    if (substream->pcm->device == ASRC_7_1CH_DEV_NUM) {  
		multi_ch = 	(channels > 2) ? true : false;
		asrc_channels =	(channels < 3) ? TCC_ASRC_NUM_OF_CH_2 :
						(channels < 5) ? TCC_ASRC_NUM_OF_CH_4 :
						(channels < 7) ? TCC_ASRC_NUM_OF_CH_6 : TCC_ASRC_NUM_OF_CH_8; 
	} else {
		multi_ch = false;
		asrc_channels = TCC_ASRC_NUM_OF_CH_2;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		pasrc_param = asrc_m2m_pcm->playback->asrc_m2m_param;

		ratio_shift22 = ((uint64_t)0x400000 * ASRC_DST_RATE);
		do_div(ratio_shift22, rate);
	} else {
		pasrc_param = asrc_m2m_pcm->capture->asrc_m2m_param;

		ratio_shift22 = ((uint64_t)0x400000 * rate);
		do_div(ratio_shift22, ASRC_SRC_RATE);
	}
	memset(pasrc_param, 0, sizeof(struct tcc_asrc_param_t));

	pasrc_param->u.cfg.src_bitwidth = asrc_bitwidth;
	pasrc_param->u.cfg.dst_bitwidth = asrc_bitwidth;
	pasrc_param->u.cfg.channels = asrc_channels;
	pasrc_param->u.cfg.ratio_shift22 = (uint32_t)ratio_shift22;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 

		asrc_m2m_pcm->playback->src->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->playback->src->period_bytes = period_bytes;
		asrc_m2m_pcm->playback->src->rate = rate;
		asrc_m2m_pcm->playback->interval = TCC_APP_PTR_CHECK_INTERVAL_TX;

		asrc_m2m_pcm->playback->dst->rate = ASRC_DST_RATE;
		asrc_m2m_pcm->playback->dst->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->playback->dst->period_bytes = PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->playback->dst->period_bytes = MIN_PERIOD_BYTES;

		asrc_m2m_pcm->playback->middle->buffer_bytes = asrc_m2m_pcm->playback->dst->buffer_bytes * MID_BUFFER_CONST;
	} else {
		asrc_m2m_pcm->capture->dst->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->capture->dst->period_bytes = period_bytes;
		asrc_m2m_pcm->capture->dst->rate = rate;
		asrc_m2m_pcm->capture->interval = TCC_APP_PTR_CHECK_INTERVAL_RX;

		asrc_m2m_pcm->capture->src->rate = ASRC_SRC_RATE;
		asrc_m2m_pcm->capture->src->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->capture->src->period_bytes = PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->capture->src->period_bytes = MIN_PERIOD_BYTES;

		asrc_m2m_pcm->capture->middle->buffer_bytes = asrc_m2m_pcm->capture->src->buffer_bytes * MID_BUFFER_CONST; 
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_asrc_m2m_pcm_set_hw_params_to_mbox(asrc_m2m_pcm, substream->stream);
#endif
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	char Ctemp=0;
	int ret = -1;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
	} else {
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
	}

	//printk("[%s][%d] Ctemp=0x%08x \n", __func__, __LINE__, Ctemp);
	if((Ctemp & IS_ASRC_STARTED) != 0) {
		tcc_asrc_m2m_pcm_asrc_stop(substream);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		tcc_is_flag_update(asrc_m2m_pcm->playback, (IS_TRIG_STARTED | IS_ASRC_STARTED | IS_ASRC_RUNNING), IS_FLAG_RELEASE);

		atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
		wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));
	} else {
		tcc_is_flag_update(asrc_m2m_pcm->capture, (IS_TRIG_STARTED | IS_ASRC_STARTED | IS_ASRC_RUNNING), IS_FLAG_RELEASE);

		atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
		wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
	}

	if(substream){
		snd_pcm_set_runtime_buffer(substream, NULL);
		ret=0;
	} else {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "%s-error\n", __func__);
	}

	return ret;
}

static int tcc_asrc_m2m_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned long flags;
	char Ctemp=0;
	int ret=0;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
	} else {
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
	}

	if((Ctemp & IS_ASRC_STARTED) != 0) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s] ASRC stop yet.\n", __func__);
		tcc_asrc_m2m_pcm_asrc_stop(substream);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->playback, true);

		spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
		if(asrc_m2m_pcm->asrc_footprint->list_len > 0) {
			// delete all footprint
			ret = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, true);
		}
		tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);
		spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);

		if(asrc_m2m_pcm->playback->middle->dma_buf->area != NULL)
			memset(asrc_m2m_pcm->playback->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
#ifdef TAIL_DEBUG
		play_tail=0;
#endif
	} else {

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->capture, true);

		if(asrc_m2m_pcm->capture->middle->dma_buf->area != NULL)
			memset(asrc_m2m_pcm->capture->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
#ifdef TAIL_DEBUG
		cap_tail=0;
#endif
	}

	memset(runtime->dma_area, 0x00, runtime->dma_bytes);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
//	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_asrc_param_t *pasrc_param = NULL; //asrc_m2m_pcm->asrc_m2m_param;
	struct tcc_asrc_t *pasrc = asrc_m2m_pcm->asrc;
//	unsigned int temp=0;
	char Ctemp=0;
	int ret=-1;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);

	if(asrc_m2m_pcm == NULL) {
		asrc_m2m_pcm_dbg_err("[%s][%d] error!\n", __func__, __LINE__);
		return -EFAULT;
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
			pasrc_param = asrc_m2m_pcm->playback->asrc_m2m_param;
		} else {
			pasrc_param = asrc_m2m_pcm->capture->asrc_m2m_param;
		}
		if(pasrc_param == NULL) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] error!\n", __func__, __LINE__);
			return -EFAULT;
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
	} else {
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
	}

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_START, PLAY\n");
				if(!(Ctemp & IS_ASRC_STARTED)) {
					if((asrc_m2m_pcm->playback->pair_id == 99)
						&&(asrc_m2m_pcm->playback->src->rate != asrc_m2m_pcm->playback->dst->rate)) {
						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] error!!\n", __func__, __LINE__);
						ret = -EINVAL;
						goto error_trigger;

					} else if((asrc_m2m_pcm->playback->pair_id != 99)
						&&(asrc_m2m_pcm->playback->src->rate != asrc_m2m_pcm->playback->dst->rate)) {
						ret = tcc_asrc_m2m_start(pasrc,
								asrc_m2m_pcm->playback->pair_id, // Pair
								pasrc_param->u.cfg.src_bitwidth, // SRC Bit width
								pasrc_param->u.cfg.dst_bitwidth, // DST Bit width
								pasrc_param->u.cfg.channels, //Channels
								pasrc_param->u.cfg.ratio_shift22); // multiple of 2
					} else {
						ret = 0;
					}

					Ctemp |= IS_ASRC_STARTED;
				} else {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_START already, PLAY\n");
				}
/*
	//ref. code
				temp = runtime->period_size;
				temp = (temp*1000)/(asrc_m2m_pcm->src->rate);
				
				asrc_m2m_pcm->interval = temp/4;
				if(asrc_m2m_pcm->interval <= 0) 
					asrc_m2m_pcm->interval = 1;

				printk("interval: %d, period: %dB, buffer: %dB\n", (int)asrc_m2m_pcm->interval, (int)asrc_m2m_pcm->src->period_bytes, (int)asrc_m2m_pcm->src->buffer_bytes);
*/

/*
 * After 1st ASRC operation, AMD send cmd of start to A7S.
 */
#if 0//def CONFIG_TCC_MULTI_MAILBOX_AUDIO
				ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
				if(ret < 0) {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
				}
#endif
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_START end ret=%d, PLAY\n", ret);
			} else {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_START, CAPTURE\n");
				if(!(Ctemp & IS_ASRC_STARTED)) {
					if((asrc_m2m_pcm->capture->pair_id == 99)
						&&(asrc_m2m_pcm->capture->src->rate != asrc_m2m_pcm->capture->dst->rate)) {
						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] error!!\n", __func__, __LINE__);
						ret = -EINVAL;
						goto error_trigger;

					} else if((asrc_m2m_pcm->capture->pair_id != 99)
							&&(asrc_m2m_pcm->capture->src->rate != asrc_m2m_pcm->capture->dst->rate)) {
						ret = tcc_asrc_m2m_start(pasrc,
								asrc_m2m_pcm->capture->pair_id, // Pair
								pasrc_param->u.cfg.src_bitwidth, // SRC Bit width
								pasrc_param->u.cfg.dst_bitwidth, // DST Bit width
								pasrc_param->u.cfg.channels, //Channels
								pasrc_param->u.cfg.ratio_shift22); // multiple of 2
					} else {
						ret = 0;
					}

					Ctemp |= IS_ASRC_STARTED;

				} else {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_START already, CAPTURE\n");
					return -EINVAL;
				}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
				ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
				if(ret < 0) {
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
				} else {
					Ctemp |= IS_A7S_STARTED;
				}
#endif
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_START end ret=%d, CAPTURE\n", ret);
			}

			Ctemp |= IS_TRIG_STARTED;

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				tcc_is_flag_update(asrc_m2m_pcm->playback, Ctemp, IS_FLAG_SET);

				atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
				wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));
			} else {
				tcc_is_flag_update(asrc_m2m_pcm->capture, Ctemp, IS_FLAG_SET);

				atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
				wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:

			Ctemp &= ~IS_TRIG_STARTED;

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
			if(ret < 0) {
				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
			} else {
				Ctemp &= ~IS_A7S_STARTED;
			}
#endif
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_STOP, PLAY\n");

				tcc_is_flag_update(asrc_m2m_pcm->playback, Ctemp, IS_FLAG_APPLY);

				atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
				wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));
			} else {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_TRIGGER_STOP, CAPTURE\n");

				tcc_is_flag_update(asrc_m2m_pcm->capture, Ctemp, IS_FLAG_APPLY);

				atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
				wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
			}
			break;
		default:
			return -EINVAL;
	}
error_trigger:
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);

	return ret;
}


static snd_pcm_uframes_t tcc_asrc_m2m_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	snd_pcm_uframes_t ret=0;

	if(asrc_m2m_pcm == NULL) {
		asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
		return bytes_to_frames(runtime, 0);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = bytes_to_frames(runtime, asrc_m2m_pcm->playback->Bwrote);
	} else {
		ret = asrc_m2m_pcm->capture->app->pre_pos;
	}
	//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ret=%d\n", (int)ret);
	return ret;
}


static struct snd_pcm_ops tcc_asrc_m2m_pcm_ops = {
    .open  = tcc_asrc_m2m_pcm_open,
    .close  = tcc_asrc_m2m_pcm_close,
    .ioctl  = snd_pcm_lib_ioctl,
    .hw_params = tcc_asrc_m2m_pcm_hw_params,
    .hw_free = tcc_asrc_m2m_pcm_hw_free,
    .prepare = tcc_asrc_m2m_pcm_prepare,
    .trigger = tcc_asrc_m2m_pcm_trigger,
    .pointer = tcc_asrc_m2m_pcm_pointer,
    .mmap = tcc_asrc_m2m_pcm_mmap,
};

static void tcc_asrc_m2m_pcm_stream_reset(struct asrc_m2m_pcm_stream *strm, bool prepare)
{
/*
	unsigned long flags;
	spin_lock_irqsave(&strm->is_locked, flags);
	strm->is_flag = 0;
	spin_unlock_irqrestore(&strm->is_locked, flags);

	//atomic_set(&strm->wakeup, 0);
*/
	//Buffer info reset.
	strm->app->pre_pos = 0;
	strm->app->pre_ptr = 0;
	if(!prepare) {
		strm->src->buffer_bytes = 0;
		strm->src->period_bytes = 0;

		strm->dst->buffer_bytes = 0;
		strm->dst->period_bytes = 0;

		strm->middle->buffer_bytes = 0;

		strm->interval = 0;
	}
	strm->middle->cur_pos = 0;
	strm->middle->pre_pos = 0;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	strm->middle->cur_pos_from_ipc = 0;
	strm->middle->pre_pos_from_ipc = 0;
#endif
	if((prepare) && (strm->middle->dma_buf->area != NULL))
		memset(strm->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);

	strm->Bwrote = 0;
	strm->Btail = 0;
}

static int tcc_asrc_m2m_pcm_asrc_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	char Ctemp=0;
	bool recheck=false;
	int ret=-1, timeout=0;

	while(timeout < 300) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
			Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
		} else {
			Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
		}

		if(((Ctemp & IS_TRIG_STARTED) == 0)
			&&((Ctemp & IS_ASRC_RUNNING) == 0)) {

			if (recheck) {
				break;
			}
			recheck = true;
		} else {
			recheck = false;
		}

		timeout ++;
		msleep(1);
	}
/*	//for debug
	if(timeout > 2) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] Ctemp=0x%08x, timeout=%d \n", __func__, __LINE__, Ctemp, timeout);
	}
*/
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 

		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_STOP, PLAY\n");
		if((asrc_m2m_pcm->playback->pair_id == 99)
				&&(asrc_m2m_pcm->playback->src->rate != asrc_m2m_pcm->playback->dst->rate)) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] error!!\n", __func__, __LINE__);
			ret = -EINVAL;
			goto error_pcm_asrc_stop;

		} else if((asrc_m2m_pcm->playback->pair_id != 99)
				&&(asrc_m2m_pcm->playback->src->rate != asrc_m2m_pcm->playback->dst->rate)) {

			ret = tcc_asrc_m2m_stop(asrc_m2m_pcm->asrc, asrc_m2m_pcm->playback->pair_id);
		} else {
			ret = 0;
		}

		Ctemp &= ~IS_ASRC_STARTED;

		tcc_is_flag_update(asrc_m2m_pcm->playback, Ctemp, IS_FLAG_APPLY);

		//atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
		//wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));

		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_STOP ret=%d, PLAY\n", ret);
	} else {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_STOP, CAPTURE\n");
			Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);


		if((asrc_m2m_pcm->capture->pair_id == 99)
				&&(asrc_m2m_pcm->capture->src->rate != asrc_m2m_pcm->capture->dst->rate)) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] error!!\n", __func__, __LINE__);
			ret = -EINVAL;
			goto error_pcm_asrc_stop;

		} else if((asrc_m2m_pcm->capture->pair_id != 99)
				&&(asrc_m2m_pcm->capture->src->rate != asrc_m2m_pcm->capture->dst->rate)) {

			ret = tcc_asrc_m2m_stop(asrc_m2m_pcm->asrc, asrc_m2m_pcm->capture->pair_id);
		} else {
			ret = 0;
		}

		Ctemp &= ~IS_ASRC_STARTED;

		tcc_is_flag_update(asrc_m2m_pcm->capture, Ctemp, IS_FLAG_APPLY);
		
		//atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
		//wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ASRC_STOP ret=%d, CAPTURE\n", ret);
	}
error_pcm_asrc_stop:
	return ret;
}

static void tcc_pcm_app_position_update(unsigned int dev_id, struct asrc_m2m_pcm_stream *strm, struct snd_pcm_runtime *runtime, ssize_t byte_size)
{
	ssize_t Btemp=0;
	snd_pcm_uframes_t Ftemp=0;

	if((!strm) || (!runtime)) {
		asrc_m2m_pcm_dbg_id_err(dev_id, "[%s][%d] strm is wrong!\n", __func__, __LINE__);
	} else {
	/*	//ref. code
		Ftemp = bytes_to_frames(runtime, read_byte);
		Btemp = frames_to_bytes(runtime, Ftemp);
		if(Btemp != read_byte) {
			asrc_m2m_pcm_dbg_id_err(dev_id, "[%d] read_byte=%d, Btemp=%d\n", __LINE__, read_byte, Btemp);
		}
   */
		//check ptr size for update
		Btemp = byte_size + strm->Btail;
		Ftemp = bytes_to_frames(runtime, Btemp);
		strm->Btail = Btemp - (frames_to_bytes(runtime, Ftemp));

		//update ptr info.
		strm->app->pre_pos += Ftemp;
		if(strm->app->pre_pos >= runtime->buffer_size) {
			strm->app->pre_pos = strm->app->pre_pos - runtime->buffer_size;
		}

		strm->app->pre_ptr += Ftemp;
		if(strm->app->pre_ptr >= runtime->boundary) {
			strm->app->pre_ptr = strm->app->pre_ptr - runtime->boundary;
		}
	}
}

static char tcc_is_flag_update(struct asrc_m2m_pcm_stream *strm, char is_flag, char mode)
{
	unsigned long flags;
	char ret=0;
	/*
	 * mode
	 * #define IS_FLAG_GET     0x00
	 * #define IS_FLAG_RELEASE 0x01
	 * #define IS_FLAG_SET     0x02
	 * #define IS_FLAG_RESET   0x04
	 * #define IS_FLAG_APPLY   0x08 
	 */

	spin_lock_irqsave(&strm->is_locked, flags);
	switch(mode) {
		case IS_FLAG_GET:	//This is for get is_flag
			break;
		case IS_FLAG_RELEASE:
			strm->is_flag &= ~(is_flag);
			break;
		case IS_FLAG_SET:
			strm->is_flag |= is_flag;
			break;
		case IS_FLAG_RESET:
			strm->is_flag = 0;
			break;
		case IS_FLAG_APPLY:
			strm->is_flag &= is_flag;
			break;
		default:
			printk("[%s][%d] ERROR!!\n", __func__, __LINE__);
			break;
	}
	ret = strm->is_flag;
	spin_unlock_irqrestore(&strm->is_locked, flags);

	return ret;
}

static int tcc_ptr_update_function_for_capture(struct snd_pcm_substream *psubstream, ssize_t readable_byte, ssize_t max_cpy_byte)
{
	struct snd_pcm_substream *substream = psubstream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc;
	//struct asrc_m2m_pcm_stream *strm = (struct asrc_m2m_pcm_stream *)asrc_m2m_pcm->capture;
	ssize_t dst_buffer_bytes=0, dst_period_bytes=0;
	ssize_t read_pos=0, write_pos=0, temp_pos=0;
	ssize_t writeable_byte=0, read_byte=0, Wtemp=0, Btemp=0;
	unsigned char *pin_buf, *pout_buf, *ptemp_buf, Ctemp=0;
	int ret=0;

	ptemp_buf = asrc_m2m_pcm->capture->middle->ptemp_buf;
	pin_buf = asrc_m2m_pcm->capture->middle->dma_buf->area;
	pout_buf = substream->dma_buffer.area;

	dst_buffer_bytes = asrc_m2m_pcm->capture->dst->buffer_bytes;
	dst_period_bytes = asrc_m2m_pcm->capture->dst->period_bytes;
	while(1) {
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
		
		if((readable_byte <= 0)
			||((Ctemp & IS_TRIG_STARTED) == 0)
			||((Ctemp & IS_ASRC_STARTED) == 0)) {

			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "readable_byte: %d, is_flag=0x%02x\n", readable_byte, (unsigned int)Ctemp);
			ret = 0;
			break;//return 0;
		} else {
			/////ASRC Running start
			tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_SET);
		}

		if(readable_byte > max_cpy_byte) {
			read_byte = max_cpy_byte; 
		} else {
			read_byte = readable_byte; 
		}

		write_pos = frames_to_bytes(runtime, asrc_m2m_pcm->capture->app->pre_pos) + asrc_m2m_pcm->capture->Btail;
		read_pos = asrc_m2m_pcm->capture->middle->pre_pos;

		if(asrc_m2m_pcm->capture->src->rate == asrc_m2m_pcm->capture->dst->rate) {

			if(write_pos + read_byte >= dst_buffer_bytes) {
				//1st part copy for out_buf
				Btemp = dst_buffer_bytes - write_pos;
				memcpy(pout_buf+write_pos, pin_buf+read_pos, Btemp);
				read_pos = read_pos + Btemp;

				//2nd part copy for out_buf
				Btemp = read_byte - Btemp;
				memcpy(pout_buf, pin_buf+read_pos, Btemp);
			} else {
				memcpy(pout_buf+write_pos, pin_buf+read_pos, read_byte);
			}

			tcc_pcm_app_position_update(asrc_m2m_pcm->dev_id, asrc_m2m_pcm->capture, runtime, read_byte);

			asrc_m2m_pcm->capture->Bwrote += read_byte;
			if(asrc_m2m_pcm->capture->Bwrote >= dst_period_bytes) {
				asrc_m2m_pcm->capture->Bwrote = asrc_m2m_pcm->capture->Bwrote % dst_period_bytes;
				snd_pcm_period_elapsed(substream);
			}

		} else {
			if(asrc_m2m_pcm->capture->pair_id == 99) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! asrc pair_id[%d] is wrong\n", __func__, __LINE__, asrc_m2m_pcm->playback->pair_id);
				
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = -1;
				break;
			}

			writeable_byte = tcc_asrc_m2m_push_data(asrc, asrc_m2m_pcm->capture->pair_id, pin_buf+read_pos, read_byte);
			if(writeable_byte > TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST) {

				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! writeable_byte[0x%08x] is bigger than max[0x%08x]\n", __func__, __LINE__, writeable_byte, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = -1;
				break;
			} else if(writeable_byte < 0) {
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! read_byte[%d] writeable_byte[%d] Ctemp[0x%02x]\n", __func__, __LINE__, read_byte, writeable_byte, Ctemp);

				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = writeable_byte;
				break;
			}

			if(write_pos + writeable_byte >= dst_buffer_bytes) {
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->capture->pair_id, ptemp_buf, writeable_byte);
			} else {
				//copy for out_buf
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->capture->pair_id, pout_buf+write_pos, writeable_byte);
			}

			if(Wtemp < 0) {
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);

				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! writeable_byte[%d] Wtemp[%d] Ctemp[0x%02x]!\n", __func__, __LINE__, writeable_byte, Wtemp, Ctemp);

				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = Wtemp;
				break;
			}
/*
			spin_lock_irqsave(&asrc_m2m_pcm->capture->is_locked, flags);
			asrc_m2m_pcm->capture->is_flag &= ~IS_ASRC_RUNNING;
			spin_unlock_irqrestore(&asrc_m2m_pcm->capture->is_locked, flags);
			/////ASRC Running end
*/
			if(write_pos + writeable_byte >= dst_buffer_bytes) {

				//1st part copy size
				temp_pos = dst_buffer_bytes - write_pos;
				//1st part copy for out_buf
				memcpy(pout_buf+write_pos, ptemp_buf, temp_pos);

				//2nd part copy size
				Btemp = Wtemp - temp_pos;
				//2nd part copy for out_buf
				memcpy(pout_buf, ptemp_buf+temp_pos, Btemp);

				if(writeable_byte > TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST) {
					
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! writeable_byte[0x%08x] is bigger than max[0x%08x]\n", __func__, __LINE__, writeable_byte, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				} else {
					memset(ptemp_buf, 0, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				}
			}

			tcc_pcm_app_position_update(asrc_m2m_pcm->dev_id, asrc_m2m_pcm->capture, runtime, Wtemp);
			asrc_m2m_pcm->capture->Bwrote += Wtemp;
			if(asrc_m2m_pcm->capture->Bwrote  >= dst_period_bytes) {
				asrc_m2m_pcm->capture->Bwrote = asrc_m2m_pcm->capture->Bwrote % dst_period_bytes;
				snd_pcm_period_elapsed(substream);
			}

#ifdef TAIL_DEBUG
			if(asrc_m2m_pcm->capture->Btail != cap_tail) {
				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, 
				"[%s][%d] Btail=%d, write_pos=%d, Wtemp=%d, Btemp=%d, read_byte=%d, readable_byte=%d, writeable_byte=%d!! \n"
				, __func__, __LINE__, asrc_m2m_pcm->capture->Btail, write_pos, Wtemp, Btemp, read_byte, readable_byte, writeable_byte);
			}

			cap_tail = asrc_m2m_pcm->capture->Btail;
#endif
		}
		asrc_m2m_pcm->capture->middle->pre_pos += read_byte;
		if(asrc_m2m_pcm->capture->middle->pre_pos >= asrc_m2m_pcm->capture->middle->buffer_bytes) {
			asrc_m2m_pcm->capture->middle->pre_pos = asrc_m2m_pcm->capture->middle->pre_pos - asrc_m2m_pcm->capture->middle->buffer_bytes;
		}
		readable_byte -= read_byte;
		if(readable_byte < 0) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "ERROR!! readable_byte: %d\n", readable_byte);
			Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
			/////ASRC Running end
			ret = -1;
			break;
		}
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "readable_byte: %d, read_byte: %d, pre_pos: %d\n", readable_byte, read_byte, asrc_m2m_pcm->capture->middle->pre_pos);
	}
	return ret;
}

static int tcc_ptr_update_thread_for_capture(void *data)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	struct snd_pcm_substream *substream;// = asrc_m2m_pcm->asrc_substream;
	struct snd_soc_pcm_runtime *rtd;// = substream->private_data;
	struct snd_pcm_runtime *runtime;// = substream->runtime;
	snd_pcm_uframes_t temp=0;
	unsigned int mid_buffer_bytes=0, max_cpy_byte=0;
	unsigned int cur_pos=0, pre_pos=0;
	char Ctemp=0;
	int readable_byte=0;
	int ret=0;

	while(!kthread_should_stop()) {
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
		if(((Ctemp & IS_TRIG_STARTED) != 0)&&((Ctemp & IS_ASRC_STARTED) != 0)) {
			if (asrc_m2m_pcm->capture->middle->cur_pos != asrc_m2m_pcm->capture->middle->pre_pos) {
    			substream = asrc_m2m_pcm->capture->asrc_substream;
    			rtd = substream->private_data;
    			runtime = substream->runtime;
    
    			cur_pos = asrc_m2m_pcm->capture->middle->cur_pos;
    			pre_pos = asrc_m2m_pcm->capture->middle->pre_pos;
    
    			mid_buffer_bytes = asrc_m2m_pcm->capture->middle->buffer_bytes;
    
    			temp = frames_to_bytes(runtime, runtime->period_size);
    			if(temp > TCC_ASRC_MAX_SIZE) {
    				max_cpy_byte = TCC_ASRC_MAX_SIZE;
    			} else {
    				max_cpy_byte = temp;
    			}
    			temp=0;
    
    			if(cur_pos >= pre_pos) {
    				//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, cur_pos, pre_pos);
    				readable_byte = cur_pos - pre_pos;
    				if(readable_byte > 0) {
    					ret = tcc_ptr_update_function_for_capture(substream, readable_byte, max_cpy_byte);
    					if(ret < 0) {
    						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
							readable_byte = 0;
    					}
    				}
    			} else {			
    				//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, cur_pos, pre_pos);
					
    				//1st part copy for in_buf
    				readable_byte = mid_buffer_bytes - pre_pos;
    
    				if(readable_byte > 0){
    					ret = tcc_ptr_update_function_for_capture(substream, readable_byte, max_cpy_byte);
    					if(ret < 0) {
    						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
							readable_byte = 0;
							cur_pos = 0;
    					}
    				}
    				
    				//2nd part copy
    				readable_byte = cur_pos;
    				//asrc_m2m_pcm->capture->middle->pre_pos = 0;
    				if(readable_byte > 0) {
    					ret = tcc_ptr_update_function_for_capture(substream, readable_byte, max_cpy_byte);
    					if(ret < 0) {
    						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
							readable_byte = 0;
    					}
    				}
    			} //(cur_pos > pre_pos)?
			} else {
				/*
				 * This kthread check the appl_ptr in every interval (default 1000us).
				 */
				wait_event_interruptible_timeout(asrc_m2m_pcm->capture->kth_wq, atomic_read(&asrc_m2m_pcm->capture->wakeup), usecs_to_jiffies(asrc_m2m_pcm->capture->interval));
			    atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);
			}
		} else { //start or closed?
		    wait_event_interruptible(asrc_m2m_pcm->capture->kth_wq, (((asrc_m2m_pcm->capture->is_flag & IS_TRIG_STARTED)!= 0) && ((asrc_m2m_pcm->capture->is_flag & IS_ASRC_STARTED)!= 0)));
			atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);
		}
	}//while
	return 0;
}

static ssize_t tcc_appl_ptr_check_function_for_play(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm, ssize_t readable_byte, ssize_t max_asrc_byte, char *pin_buf)
{
    struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc;
	unsigned int mid_buffer_bytes=0, write_pos=0, temp_pos=0, pre_pos=0;
	ssize_t read_byte=0, writeable_byte=0;
	ssize_t Btemp=0, Wtemp=0, ret=0;
	unsigned long flags;
	int len=0;
	char *pout_buf, *ptemp_buf, Ctemp=0;
	
	mid_buffer_bytes = asrc_m2m_pcm->playback->middle->buffer_bytes;

	ptemp_buf = asrc_m2m_pcm->playback->middle->ptemp_buf;
	pout_buf = asrc_m2m_pcm->playback->middle->dma_buf->area;

#ifdef PERFORM_ASRC_MULTIPLE_TIME
	while(1) {
#endif
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_RELEASE);

		if((readable_byte <= 0)
				||((Ctemp & IS_TRIG_STARTED) == 0)
				||((Ctemp & IS_ASRC_STARTED) == 0)) {

			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "readable_byte: %d, is_flag=0x%02x\n", readable_byte, (unsigned int)Ctemp);
#ifdef PERFORM_ASRC_MULTIPLE_TIME
			break;
#else
			return ret;
#endif
		} else {
			Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_SET);
		}
		if(readable_byte > max_asrc_byte) {
			read_byte = max_asrc_byte;
		} else {
			read_byte = readable_byte;
		}

		write_pos = asrc_m2m_pcm->playback->middle->cur_pos;
		pre_pos = asrc_m2m_pcm->playback->middle->pre_pos;
		
		if(asrc_m2m_pcm->playback->src->rate == asrc_m2m_pcm->playback->dst->rate) {
			if(write_pos + read_byte >= mid_buffer_bytes) {
				//1st part copy for out_buf
				Btemp = mid_buffer_bytes - write_pos;
				memcpy(pout_buf+write_pos, pin_buf+ret, Btemp);
				temp_pos = Btemp;
				//2nd part copy for out_buf
				Btemp = read_byte - Btemp;
				memcpy(pout_buf, pin_buf+ret+temp_pos, Btemp);

				//asrc_m2m_pcm->playback->middle->cur_pos = Btemp;
			} else {
				memcpy(pout_buf+write_pos, pin_buf+ret, read_byte);
				//asrc_m2m_pcm->playback->middle->cur_pos += read_byte; 
			}

			//ret += read_byte;
			Wtemp = read_byte;
		} else {
			if(asrc_m2m_pcm->playback->pair_id == 99) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! asrc pair_id[%d] is wrong\n", __func__, __LINE__, asrc_m2m_pcm->playback->pair_id);

				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = -1;
				break;
			}

			writeable_byte = tcc_asrc_m2m_push_data(asrc, asrc_m2m_pcm->playback->pair_id, pin_buf+ret, read_byte);
			
			if(writeable_byte > TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST) {
			
				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! writeable_byte[0x%08x] is bigger than max[0x%08x]\n", __func__, __LINE__, writeable_byte, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = -1;
				break;
			} else if(writeable_byte < 0) {
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);

				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! read_byte[%d] writeable_byte[%d] Ctemp[0x%02x]\n", __func__, __LINE__, read_byte, writeable_byte, Ctemp);

				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = writeable_byte;
				break;
			}

			if(write_pos + writeable_byte >= mid_buffer_bytes) {
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->playback->pair_id, ptemp_buf, writeable_byte);
			} else {
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->playback->pair_id, pout_buf+write_pos, writeable_byte);
			}

			if(Wtemp < 0) {
				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);

				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! writealble_byte[%d] Wtemp[%d] Ctemp[0x%08x]!?\n", __func__, __LINE__, writeable_byte, Wtemp, Ctemp);

				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
				ret = Wtemp;
				break;
			}

			if(write_pos + writeable_byte >= mid_buffer_bytes) {

				//1st part copy for out_buf
				Btemp = mid_buffer_bytes - write_pos;
				memcpy(pout_buf+write_pos, ptemp_buf, Btemp);

				//2nd part copy for out_buf
				temp_pos = Btemp;
				Btemp = Wtemp - Btemp;
				memcpy(pout_buf, ptemp_buf+temp_pos, Btemp);

				if(writeable_byte > TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST) {

					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! writeable_byte[0x%08x] is bigger than max[0x%08x]\n", __func__, __LINE__, writeable_byte, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				} else {
					memset(ptemp_buf, 0, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				}
			}
		}

		asrc_m2m_pcm->playback->middle->cur_pos += Wtemp;
		if(asrc_m2m_pcm->playback->middle->cur_pos >= mid_buffer_bytes) {
			asrc_m2m_pcm->playback->middle->cur_pos = asrc_m2m_pcm->playback->middle->cur_pos - mid_buffer_bytes;
		}

		ret += read_byte;
		readable_byte -= read_byte;
		if(readable_byte < 0) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! readable_byte=%d, max_asrc_byte=%d, ret=%d\n", __func__, __LINE__, readable_byte, max_asrc_byte, ret);

			Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_ASRC_RUNNING, IS_FLAG_RELEASE);
			ret = -1;
			break;
		}

		spin_lock_irqsave(&asrc_m2m_pcm->foot_locked, flags);
		len = tcc_asrc_footprint_insert(asrc_m2m_pcm->asrc_footprint, asrc_m2m_pcm->playback->middle->cur_pos, read_byte);
		spin_unlock_irqrestore(&asrc_m2m_pcm->foot_locked, flags);
		if(len <= 0) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! len=%d\n", __func__, __LINE__, len);
		}

#if	(CHECK_OVERRUN == 1)

		if((write_pos > pre_pos)
				&&(asrc_m2m_pcm->playback->middle->cur_pos > pre_pos)&&(write_pos > asrc_m2m_pcm->playback->middle->cur_pos)) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "overrun?! pre_pos=%u, write_pos=%u, cur_pos=%u\n", pre_pos, write_pos, asrc_m2m_pcm->playback->middle->cur_pos);

		} else if((write_pos < pre_pos)
				&&((asrc_m2m_pcm->playback->middle->cur_pos > pre_pos)||(write_pos > asrc_m2m_pcm->playback->middle->cur_pos))) {
			asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "overrun?? pre_pos=%u, write_pos=%u, cur_pos=%u\n", pre_pos, write_pos, asrc_m2m_pcm->playback->middle->cur_pos);
		}
#endif
#ifdef PERFORM_ASRC_MULTIPLE_TIME
	}
#endif
	return ret;
}

static int tcc_appl_ptr_check_thread_for_play(void *data)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	struct snd_pcm_substream *substream;// = asrc_m2m_pcm->asrc_substream;
	struct snd_soc_pcm_runtime *rtd;// = substream->private_data;
	struct snd_pcm_runtime *runtime;// = substream->runtime;
	snd_pcm_uframes_t pre_appl_ptr=0, cur_appl_ptr=0, pre_appl_ofs=0, cur_appl_ofs=0;
	snd_pcm_uframes_t Ftemp=0;
	ssize_t readable_byte=0, max_cpy_byte=0, read_pos=0, Btemp=0, ret=0;
	char *pin_buf, Ctemp=0;
#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
	snd_pcm_uframes_t read_byte;
#endif

	while(!kthread_should_stop()) {

wait_check_play:

		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
		if(((Ctemp & IS_TRIG_STARTED) != 0)&&((Ctemp & IS_ASRC_STARTED) != 0)) {

			substream = asrc_m2m_pcm->playback->asrc_substream;
			rtd = substream->private_data;
			runtime = substream->runtime;

			if((!substream) || (!rtd) || (!runtime)) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] is_flag=0x%02x\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->playback->is_flag);
				goto wait_check_play;
			}

			cur_appl_ptr = runtime->control->appl_ptr;
			pre_appl_ptr = asrc_m2m_pcm->playback->app->pre_ptr;

			if((cur_appl_ptr != pre_appl_ptr)) {
				cur_appl_ofs = cur_appl_ptr % runtime->buffer_size;
				/*	//ehk23 This does not need.
				if((cur_appl_ofs == 0) && (cur_appl_ptr > 0)) {
					cur_appl_ofs = runtime->buffer_size;
				}*/
				pre_appl_ofs = asrc_m2m_pcm->playback->app->pre_pos;

				Btemp = frames_to_bytes(runtime, runtime->period_size);
				if(Btemp > TCC_ASRC_MAX_SIZE) {
					max_cpy_byte = TCC_ASRC_MAX_SIZE;
				} else {
					max_cpy_byte = Btemp;
				}
				Btemp=0;

				pin_buf = substream->dma_buffer.area;

				if(cur_appl_ofs > pre_appl_ofs) {
					//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d]cur=%lu, pre=%lu, cur_ptr=%lu, pre_ptr=%lu\n", __func__, __LINE__, cur_appl_ofs, pre_appl_ofs, cur_appl_ptr, pre_appl_ptr);
					Ftemp = cur_appl_ofs - pre_appl_ofs;
					readable_byte = frames_to_bytes(runtime, Ftemp);
					read_pos = frames_to_bytes(runtime, asrc_m2m_pcm->playback->app->pre_pos) + asrc_m2m_pcm->playback->Btail;

					if(readable_byte > 0) {

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&start);
#endif
#ifdef PERFORM_ASRC_MULTIPLE_TIME
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, (ssize_t)TCC_ASRC_UNIT_SIZE, pin_buf+read_pos);
#else
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, max_cpy_byte, pin_buf+read_pos);
#endif

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&end);

						elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
						do_div(elapsed_usecs64, NSEC_PER_USEC);
						elapsed_usecs = elapsed_usecs64;

						printk("asrc m2m elapsed time : %03d usec, %ldbytes\n", elapsed_usecs, ret);
#endif
						if(ret < 0) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
						} else {
							tcc_pcm_app_position_update(asrc_m2m_pcm->dev_id, asrc_m2m_pcm->playback, runtime, ret);
						}
						
#ifdef TAIL_DEBUG
						if(asrc_m2m_pcm->playback->Btail != play_tail) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, 
							"[%s][%d] Btail=%d, read_pos=%d, Ftemp=%lu, Btemp=%d, ret=%d, readable_byte=%d!! \n"
							, __func__, __LINE__, asrc_m2m_pcm->playback->Btail, read_pos, Ftemp, Btemp, ret, readable_byte);
						}
						play_tail = asrc_m2m_pcm->playback->Btail;
#endif
					}
				} else {	//(cur_appl_ofs > pre_appl_ofs)?
					//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d]cur=%lu, pre=%lu, cur_ptr=%lu, pre_ptr=%lu\n", __func__, __LINE__, cur_appl_ofs, pre_appl_ofs, cur_appl_ptr, pre_appl_ptr);

					//1st part copy for in_buf
					Ftemp = runtime->buffer_size - pre_appl_ofs;
					readable_byte = frames_to_bytes(runtime, Ftemp);
					read_pos = frames_to_bytes(runtime, asrc_m2m_pcm->playback->app->pre_pos) + asrc_m2m_pcm->playback->Btail;

					if(readable_byte > 0) {
#ifdef PERFORM_ASRC_MULTIPLE_TIME
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, (ssize_t)TCC_ASRC_UNIT_SIZE, pin_buf+read_pos);
#else
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, max_cpy_byte, pin_buf+read_pos);
#endif
						if(ret < 0) {
							asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
						} else {
							tcc_pcm_app_position_update(asrc_m2m_pcm->dev_id, asrc_m2m_pcm->playback, runtime, ret);
						}

#ifdef TAIL_DEBUG
						if(asrc_m2m_pcm->playback->Btail != play_tail) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, 
							"[%s][%d] Btail=%d, read_pos=%d, Ftemp=%lu, Btemp=%d, ret=%d, readable_byte=%d!! \n"
							, __func__, __LINE__, asrc_m2m_pcm->playback->Btail, read_pos, Ftemp, Btemp, ret, readable_byte);
						}
						play_tail = asrc_m2m_pcm->playback->Btail;
#endif
					}

					if(ret >= max_cpy_byte) {
						goto max_cpy_tx;
					} else {
						readable_byte = max_cpy_byte - ret;
					}

					//2nd part copy for in_buf
					Ftemp = cur_appl_ofs;
					Btemp = frames_to_bytes(runtime, Ftemp); 
					if(readable_byte > Btemp) {
						readable_byte = Btemp;
					}

					if(readable_byte > 0) {
#ifdef PERFORM_ASRC_MULTIPLE_TIME
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, (ssize_t)TCC_ASRC_UNIT_SIZE, pin_buf);
#else
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, max_cpy_byte, pin_buf);
#endif
						if(ret < 0) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
						} else {
							tcc_pcm_app_position_update(asrc_m2m_pcm->dev_id, asrc_m2m_pcm->playback, runtime, ret);
						}

#ifdef TAIL_DEBUG
						if(asrc_m2m_pcm->playback->Btail != play_tail) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, 
							"[%s][%d] Btail=%d, read_pos=%d, Ftemp=%lu, Btemp=%d, ret=%d, readable_byte=%d!! \n"
							, __func__, __LINE__, asrc_m2m_pcm->playback->Btail, read_pos, Ftemp, Btemp, ret, readable_byte);
						}
						play_tail = asrc_m2m_pcm->playback->Btail;
#endif
					}
				} //(cur_appl_ofs > pre_appl_ofs)? else?
max_cpy_tx:
				Ftemp=0;

				Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
			    if((!(Ctemp & IS_A7S_STARTED))
        			&&((Ctemp & IS_ASRC_STARTED) != 0)
        			&&((Ctemp & IS_TRIG_STARTED) != 0)) {
        
        			substream = asrc_m2m_pcm->playback->asrc_substream;
        
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
        			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, SNDRV_PCM_TRIGGER_START);
        			if(ret < 0) {
        				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->dev_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
					} else {
						Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, IS_A7S_STARTED, IS_FLAG_SET);
        			}
#endif
        		}

			} else { //(cur_appl_ptr != pre_appl_ptr)?
				/*
				 * This kthread check the appl_ptr in every interval (default 100us).
				 */
				wait_event_interruptible_timeout(asrc_m2m_pcm->playback->kth_wq, atomic_read(&asrc_m2m_pcm->playback->wakeup), usecs_to_jiffies(asrc_m2m_pcm->playback->interval));
				atomic_set(&asrc_m2m_pcm->playback->wakeup, 0);
			}	//(cur_appl_ptr != pre_appl_ptr)? else?
		} else { //start or closed? 
			wait_event_interruptible(asrc_m2m_pcm->playback->kth_wq, (((asrc_m2m_pcm->playback->is_flag & IS_TRIG_STARTED)!= 0) && ((asrc_m2m_pcm->playback->is_flag & IS_ASRC_STARTED)!= 0)));
			atomic_set(&asrc_m2m_pcm->playback->wakeup, 0);
		} //start or closed? else ? 
	}//while
	return 0;
}

static int tcc_asrc_m2m_pcm_stream_init(struct device *dev, struct asrc_m2m_pcm_stream *strm)
{
	struct tcc_asrc_param_t *asrc_m2m_param;
	struct snd_pcm_substream *asrc_substrm;
	struct tcc_mid_buf *middle;
	struct tcc_param_info *src;
	struct tcc_param_info *dst;
	struct tcc_app_buffer_info *app;
	int ret = 0;

	if(strm->asrc_m2m_param == NULL) {
		asrc_m2m_param = (struct tcc_asrc_param_t*)devm_kzalloc(dev, sizeof(struct tcc_asrc_param_t), GFP_KERNEL);
		if(!asrc_m2m_param) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_asrc_m2m_param;
		} else {
			memset(asrc_m2m_param, 0, sizeof(struct tcc_asrc_param_t));
		}
	} else {
		asrc_m2m_pcm_dbg("[%s][%d] asrc_m2m_param is already allocated!!\n", __func__, __LINE__);
		asrc_m2m_param = NULL; 
	}

	if(strm->asrc_substream == NULL) {
		asrc_substrm = (struct snd_pcm_substream*)devm_kzalloc(dev, sizeof(struct snd_pcm_substream), GFP_KERNEL);
		if(!asrc_substrm) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_asrc_substrm;
		} else {
			memset(asrc_substrm, 0, sizeof(struct snd_pcm_substream));
		}
	} else {
		asrc_m2m_pcm_dbg("[%s][%d] asrc_substreamm is already allocated!!\n", __func__, __LINE__);
		asrc_substrm = NULL; 
	}

	if(strm->middle == NULL) {
		middle = (struct tcc_mid_buf*)devm_kzalloc(dev, sizeof(struct tcc_mid_buf), GFP_KERNEL);
		if(!middle) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_middle;
		} else {
			memset(middle, 0, sizeof(struct tcc_mid_buf));
		}

		middle->dma_buf = (struct snd_dma_buffer *)kzalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
		if(!middle->dma_buf) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_middle_buf;
		} else {
			memset(middle->dma_buf, 0, sizeof(struct snd_dma_buffer));
		}

		middle->ptemp_buf = (unsigned char *)kzalloc(sizeof(unsigned char)*TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST, GFP_KERNEL);
		if(!middle->ptemp_buf) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_middle_temp_buf;
		} else {
			memset(middle->ptemp_buf, 0, sizeof(unsigned char)*TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
		}
	} else {
		asrc_m2m_pcm_dbg("[%s][%d] middle is already allocated!!\n", __func__, __LINE__);
		middle = NULL; 
	}

	if(strm->src == NULL) {
		src = (struct tcc_param_info*)devm_kzalloc(dev, sizeof(struct tcc_param_info), GFP_KERNEL);
		if(!src) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_src;
		} else {
			memset(src, 0, sizeof(struct tcc_param_info));
		}
	} else {
		asrc_m2m_pcm_dbg("[%s][%d] src is already allocated!!\n", __func__, __LINE__);
		src = NULL; 
	}

	if(strm->dst == NULL) {
		dst = (struct tcc_param_info*)devm_kzalloc(dev, sizeof(struct tcc_param_info), GFP_KERNEL);
		if(!dst) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_dst;
		} else {
			memset(dst, 0, sizeof(struct tcc_param_info));
		}
	} else {
		asrc_m2m_pcm_dbg("[%s][%d] dst is already allocated!!\n", __func__, __LINE__);
		dst = NULL; 
	}

	if(strm->app == NULL) {
		app = (struct tcc_app_buffer_info*)devm_kzalloc(dev, sizeof(struct tcc_app_buffer_info), GFP_KERNEL);
		if(!app) {
			asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_app;
		} else {
			memset(app, 0, sizeof(struct tcc_app_buffer_info));
		}
	} else {
		asrc_m2m_pcm_dbg("[%s][%d] app is already allocated!!\n", __func__, __LINE__);
		app = NULL; 
	}

	strm->asrc_m2m_param = asrc_m2m_param;
	strm->asrc_substream = asrc_substrm;
	strm->middle = middle;

	strm->src = src;
	strm->dst = dst;
	strm->app = app;

	strm->first_open = false;
	return 0;
	
	devm_kfree(dev, app);
error_app:
	devm_kfree(dev, dst);
error_dst:
	devm_kfree(dev, src);
error_src:
	kfree(middle->ptemp_buf);
error_middle_temp_buf:
	kfree(middle->dma_buf);
error_middle_buf:
	devm_kfree(dev, middle);
error_middle:
	devm_kfree(dev, asrc_substrm);
error_asrc_substrm:
	devm_kfree(dev, asrc_m2m_param);
error_asrc_m2m_param:
	return ret;
}

static void tcc_asrc_m2m_pcm_stream_deinit(struct device *dev, struct asrc_m2m_pcm_stream *strm)
{
	struct tcc_asrc_param_t *asrc_m2m_param;
	struct tcc_mid_buf *middle;
	struct tcc_param_info *src;
	struct tcc_param_info *dst;
	struct tcc_app_buffer_info *app;

	if(strm->asrc_m2m_param) {
		asrc_m2m_param = strm->asrc_m2m_param;
		devm_kfree(dev, asrc_m2m_param);
	}

	if(strm->middle) {
		middle = strm->middle;
		if(middle->ptemp_buf)
			kfree(middle->ptemp_buf);
		if(middle->dma_buf)
			kfree(middle->dma_buf);
		devm_kfree(dev, middle);
	}

	if(strm->src) {
		src = strm->src;
		devm_kfree(dev, src);
	}

	if(strm->dst) {
		dst = strm->dst;
		devm_kfree(dev, dst);
	}

	if(strm->app) {
		app = strm->app;
		devm_kfree(dev, app);
	}
}

static int tcc_asrc_m2m_pcm_new(struct snd_soc_pcm_runtime *rtd) 
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm	= rtd->pcm;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	struct platform_device *pdev;
	bool playback=true, capture=true;
	int ret;

	if(asrc_m2m_pcm) {
		pdev = asrc_m2m_pcm->pdev;
	} else {
		asrc_m2m_pcm_dbg_err("[%s][%d] ERROR!!\n", __func__, __LINE__);
		return -1;
	}

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret) {
		return ret;
	}

	if(dai_link->playback_only) {
		capture = false;
	}

	if(dai_link->capture_only) {
		playback = false;
	}

	if(playback) {
		ret = tcc_asrc_m2m_pcm_stream_init(&pdev->dev, asrc_m2m_pcm->playback);
		if(ret != 0) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] Error!!\n", __func__, __LINE__);
			goto error_playback_stream_init;
		}

#ifdef FOOTPRINT_LINKED_LIST
		asrc_m2m_pcm->asrc_footprint = (List *)kzalloc(sizeof(List), GFP_KERNEL);
#else
		asrc_m2m_pcm->asrc_footprint = (struct footprint *)kzalloc(sizeof(struct footprint), GFP_KERNEL);
#endif
		if(!asrc_m2m_pcm->asrc_footprint) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] Error!!\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto error_footprint;
		} else {
			tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);
		}

		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev, MAX_BUFFER_BYTES*MID_BUFFER_CONST, asrc_m2m_pcm->playback->middle->dma_buf);
		if (ret) {
			goto error_alloc_pages_play;
		}
	}

	if(capture) {
		ret = tcc_asrc_m2m_pcm_stream_init(&pdev->dev, asrc_m2m_pcm->capture);
		if(ret != 0) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] Error!!\n", __func__, __LINE__);
			goto error_capture_stream_init;
		}

		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev, MAX_BUFFER_BYTES*MID_BUFFER_CONST, asrc_m2m_pcm->capture->middle->dma_buf);
		if (ret) {
			goto error_alloc_pages_cap;
		}
	}

	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
			card->dev, MAX_BUFFER_BYTES, MAX_BUFFER_BYTES);
	if (ret) {
		goto error_prealloc;
	}

	if(playback) {
		atomic_set(&asrc_m2m_pcm->playback->wakeup, 0);

		init_waitqueue_head(&asrc_m2m_pcm->playback->kth_wq);
		spin_lock_init(&asrc_m2m_pcm->playback->is_locked);

		spin_lock_init(&asrc_m2m_pcm->foot_locked);	//This is only for playback
	}

	if(capture) {
		atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);

		init_waitqueue_head(&asrc_m2m_pcm->capture->kth_wq);
		spin_lock_init(&asrc_m2m_pcm->capture->is_locked);
	}
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);

	return 0;
	
error_prealloc:
	if(capture) {
		snd_dma_free_pages(asrc_m2m_pcm->capture->middle->dma_buf);
error_alloc_pages_cap:
		tcc_asrc_m2m_pcm_stream_deinit(&pdev->dev, asrc_m2m_pcm->playback);
	}
error_capture_stream_init:
	if(playback) {
		snd_dma_free_pages(asrc_m2m_pcm->playback->middle->dma_buf);
error_alloc_pages_play:
		kfree(asrc_m2m_pcm->asrc_footprint);
error_footprint:
		tcc_asrc_m2m_pcm_stream_deinit(&pdev->dev, asrc_m2m_pcm->capture);
	}
error_playback_stream_init:
	return ret;
}

static void tcc_asrc_m2m_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);
	snd_pcm_lib_preallocate_free_for_all(pcm);
	snd_dma_free_pages(asrc_m2m_pcm->playback->middle->dma_buf);
	snd_dma_free_pages(asrc_m2m_pcm->capture->middle->dma_buf);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);
}


static struct snd_soc_platform_driver tcc_asrc_m2m_pcm_platform = {
    .ops      = &tcc_asrc_m2m_pcm_ops,
    .pcm_new  = tcc_asrc_m2m_pcm_new,
    .pcm_free = tcc_asrc_m2m_pcm_free_dma_buffers,
};

#if defined(CONFIG_PM)
static int tcc_asrc_m2m_pcm_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)platform_get_drvdata(pdev);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] start\n", __func__, __LINE__);

	if(asrc_m2m_pcm->playback->first_open == true) {
		if (asrc_m2m_pcm->playback->kth_id != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->playback->kth_id);
			asrc_m2m_pcm->playback->kth_id = NULL;
		}
		asrc_m2m_pcm->playback->first_open = false;
	}

	if(asrc_m2m_pcm->capture->first_open == true) {
		if (asrc_m2m_pcm->capture->kth_id != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->capture->kth_id);
			asrc_m2m_pcm->capture->kth_id = NULL;
		}
		asrc_m2m_pcm->capture->first_open = false;
	}


	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);
    return 0;
}

static int tcc_asrc_m2m_pcm_resume(struct platform_device *pdev)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)platform_get_drvdata(pdev);
	asrc_m2m_pcm->playback->first_open = false;
	asrc_m2m_pcm->capture->first_open = false;
    return 0;
}
#endif

static int parse_asrc_m2m_pcm_dt(struct platform_device *pdev, struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	unsigned int itemp=0;
	int ret=0;

	asrc_m2m_pcm->pdev = pdev;

    asrc_m2m_pcm->dev_id = -1;
    asrc_m2m_pcm->dev_id = of_alias_get_id(pdev->dev.of_node, "asrc_m2m_pcm");
	if(asrc_m2m_pcm->dev_id < 0) {
		asrc_m2m_pcm_dbg("[%s] ERROR!! id[%d] is wrong.\n", __func__, asrc_m2m_pcm->dev_id);
		ret = -EINVAL;
		goto error_dev_id;
	}
	asrc_m2m_pcm_dbg("[%s] dev_id[%u]\n", __func__, asrc_m2m_pcm->dev_id);
	
	if((of_property_read_u32(pdev->dev.of_node, "playback-pair-id", &itemp)) >= 0) {
		asrc_m2m_pcm->playback->pair_id = itemp;
	} else {
		asrc_m2m_pcm->playback->pair_id = 99;
	}
	asrc_m2m_pcm_dbg("[%s] playback_pair_id[%u]\n", __func__, asrc_m2m_pcm->playback->pair_id);

	if((of_property_read_u32(pdev->dev.of_node, "capture-pair-id", &itemp)) >= 0) {
		asrc_m2m_pcm->capture->pair_id = itemp;
	} else {
		asrc_m2m_pcm->capture->pair_id = 99;
	}
	asrc_m2m_pcm_dbg("[%s] capture_pair_id[%u]\n", __func__, asrc_m2m_pcm->capture->pair_id);

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	if((of_property_read_u32(pdev->dev.of_node, "playback-amd-cmd", &itemp)) >= 0) {
		if((itemp >= 0x0)&&(itemp < 0x9)) {	//0~5: playback stream to mixer, 6~8: general
			asrc_m2m_pcm->playback->mbox_cmd_type = (char)(itemp&0xFF);
		} else {
			asrc_m2m_pcm->playback->mbox_cmd_type = 99;
		}
	} else {
		asrc_m2m_pcm->playback->mbox_cmd_type = 99;
	}
	asrc_m2m_pcm_dbg("[%s] playback-amd-cmd[%u]\n", __func__, (unsigned int)asrc_m2m_pcm->playback->mbox_cmd_type);

	if((of_property_read_u32(pdev->dev.of_node, "capture-amd-cmd", &itemp)) >= 0) {
		if((itemp >= 0x0)&&(itemp < 0x9)) {	//0~5: playback stream to mixer, 6~8: general
			asrc_m2m_pcm->capture->mbox_cmd_type = (char)(itemp&0xFF);
		} else {
			asrc_m2m_pcm->capture->mbox_cmd_type = 99;
		}
	} else {
		asrc_m2m_pcm->capture->mbox_cmd_type = 99;
	}
	asrc_m2m_pcm_dbg("[%s] capture-amd-cmd[%u]\n", __func__, (unsigned int)asrc_m2m_pcm->capture->mbox_cmd_type);

	if((asrc_m2m_pcm->playback->mbox_cmd_type == 99)&&(asrc_m2m_pcm->capture->mbox_cmd_type == 99)) {
		asrc_m2m_pcm_dbg_err("[%s] ERROR!! cmdtype number is wrong! Please check device tree.\n", __func__);
	}
#endif
error_dev_id:
	return ret;
}


static int tcc_asrc_m2m_pcm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm;
	struct tcc_asrc_t *asrc;
	struct asrc_m2m_pcm_stream *playback_strm;
	struct asrc_m2m_pcm_stream *capture_strm;
	//struct snd_pcm_substream *asrc_substream;
	struct device_node *of_node_asrc;

	asrc_m2m_pcm_dbg("[%s][%d] start\n", __func__, __LINE__);

	asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_asrc_m2m_pcm), GFP_KERNEL);
	if(!asrc_m2m_pcm) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_pcm;
	} else {
		memset(asrc_m2m_pcm, 0, sizeof(struct tcc_asrc_m2m_pcm));
	}

	asrc = (struct tcc_asrc_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_asrc_t), GFP_KERNEL);
	if(!asrc) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_asrc;
	} else {
		memset(asrc, 0, sizeof(struct tcc_asrc_t));
	}

	playback_strm = (struct asrc_m2m_pcm_stream*)devm_kzalloc(&pdev->dev, sizeof(struct asrc_m2m_pcm_stream), GFP_KERNEL);
	if(!playback_strm) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_playback_strm;
	} else {
		memset(playback_strm, 0, sizeof(struct asrc_m2m_pcm_stream));
	}

	capture_strm = (struct asrc_m2m_pcm_stream*)devm_kzalloc(&pdev->dev, sizeof(struct asrc_m2m_pcm_stream), GFP_KERNEL);
	if(!capture_strm) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_capture_strm;
	} else {
		memset(capture_strm, 0, sizeof(struct asrc_m2m_pcm_stream));
	}

	of_node_asrc = of_parse_phandle(pdev->dev.of_node, "asrc", 0); 
	if(of_node_asrc == NULL) {
		asrc_m2m_pcm_dbg("[%s] ERROR!! of_node_asrc is NULL!\n", __func__);
		ret = -EINVAL;
		goto error_of_node;
	}

	asrc = tcc_asrc_get_handle_by_node(of_node_asrc);
	if(asrc == NULL) {
		asrc_m2m_pcm_dbg("[%s] ERROR!! asrc is NULL!\n", __func__);
		ret = -EINVAL;
		goto error_of_node;
	}

	asrc_m2m_pcm->asrc = asrc;
	asrc_m2m_pcm->playback = playback_strm;
	asrc_m2m_pcm->capture = capture_strm;

	if((ret = parse_asrc_m2m_pcm_dt(pdev, asrc_m2m_pcm)) < 0) {
		pr_err("%s : Fail to parse asrc_m2m_pcm dt\n", __func__);
		goto error_parse;
	}

    asrc_m2m_pcm->dev = &pdev->dev;

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
    asrc_m2m_pcm->mbox_audio_dev = get_tcc_mbox_audio_device();
    if (asrc_m2m_pcm->mbox_audio_dev == NULL) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s] ERROR!! mbox_audio_dev is NULL!\n", __func__);
		goto error_mailbox;
    }
#endif

	platform_set_drvdata(pdev, asrc_m2m_pcm);

	ret = snd_soc_register_platform(&pdev->dev, &tcc_asrc_m2m_pcm_platform);
	if (ret < 0) {
		printk("tcc_asrc_m2m_pcm_platform_register failed\n");
		goto error_register;
	}
	//asrc_m2m_pcm_dbg("tcc_asrc_m2m_pcm_platform_register success\n");

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->dev_id, "[%s][%d] end\n", __func__, __LINE__);
	return ret;

error_register:
error_mailbox:
error_parse:
error_of_node:
	devm_kfree(&pdev->dev, capture_strm);
error_capture_strm:
	devm_kfree(&pdev->dev, playback_strm);
error_playback_strm:
	devm_kfree(&pdev->dev, asrc);
error_asrc:
	devm_kfree(&pdev->dev, asrc_m2m_pcm);
error_pcm:
	return ret;
}

static int tcc_asrc_m2m_pcm_remove(struct platform_device *pdev)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)platform_get_drvdata(pdev);
	asrc_m2m_pcm_dbg("%s\n", __func__);

	if(asrc_m2m_pcm->playback->first_open == true) {
		if (asrc_m2m_pcm->playback->kth_id != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->playback->kth_id);
			asrc_m2m_pcm->playback->kth_id = NULL;
		}
		asrc_m2m_pcm->playback->first_open = false;
	}

	if(asrc_m2m_pcm->capture->first_open == true) {
		if (asrc_m2m_pcm->capture->kth_id != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->capture->kth_id);
			asrc_m2m_pcm->capture->kth_id = NULL;
		}
		asrc_m2m_pcm->capture->first_open = false;
	}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->playback->mbox_cmd_type);
	tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->capture->mbox_cmd_type);
#endif
	kfree(asrc_m2m_pcm->asrc_footprint);

	devm_kfree(&pdev->dev, asrc_m2m_pcm->playback->app);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->playback->dst);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->playback->src);
	
	kfree(asrc_m2m_pcm->playback->middle->ptemp_buf);
	kfree(asrc_m2m_pcm->playback->middle->dma_buf);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->playback->middle);
	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->playback->asrc_m2m_param);

	devm_kfree(&pdev->dev, asrc_m2m_pcm->capture->app);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->capture->dst);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->capture->src);
	
	kfree(asrc_m2m_pcm->capture->middle->ptemp_buf);
	kfree(asrc_m2m_pcm->capture->middle->dma_buf);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->capture->middle);
	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->capture->asrc_m2m_param);

	devm_kfree(&pdev->dev, asrc_m2m_pcm->asrc);
	devm_kfree(&pdev->dev, asrc_m2m_pcm);

	return 0;
}

static struct of_device_id tcc_asrc_m2m_pcm_of_match[] = {
	{ .compatible = "telechips,asrc_m2m_pcm" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_asrc_m2m_pcm_of_match);

static struct platform_driver tcc_asrc_m2m_pcm_driver = {
	.probe		= tcc_asrc_m2m_pcm_probe,
	.remove		= tcc_asrc_m2m_pcm_remove,
#if defined(CONFIG_PM)
    .suspend  = tcc_asrc_m2m_pcm_suspend,
    .resume   = tcc_asrc_m2m_pcm_resume,
#endif
	.driver 	= {
		.name	= "tcc_asrc_m2m_pcm_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_asrc_m2m_pcm_of_match),
#endif
	},
};

module_platform_driver(tcc_asrc_m2m_pcm_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips ASRC M2M PCM Driver");
MODULE_LICENSE("GPL");
