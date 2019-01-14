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

static int tcc_ptr_update_thread_for_capture(void *data);
static int tcc_appl_ptr_check_thread_for_play(void *data);

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
	new_node->prev = NULL;

	if(list->list_len > 0) {
		list->tail->next = new_node;
		new_node->prev = list->tail;
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
		if(list->list_len == 0) {
			printk("[%s][%d]There is no footprint (len=%d)\n", __func__, __LINE__, list->list_len);
			return -EFAULT;
		} else if(list->list_len == 1) {
			kfree(list->tail);
			(list->list_len)--;
			return list->list_len;
		}

		dummy = list->head;
		list->head = dummy->next;
		list->head->prev = NULL;

		kfree(dummy);
		(list->list_len)--;
	} else {	//delete all
		while(list->list_len > 0) {
			if(list->list_len == 1) {
				kfree(list->tail);
				(list->list_len)--;
			} else {
				dummy = list->head;
				list->head = dummy->next;
				list->head->prev = NULL;

				kfree(dummy);
				(list->list_len)--;
			}
		}
	}
	return list->list_len;
}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
static int tcc_asrc_m2m_pcm_set_action_to_mbox(struct mbox_audio_device *mbox_audio_dev, struct snd_pcm_substream *substream, int cmd)
{
    struct mbox_audio_data_header_t mbox_header;

    const struct snd_pcm *pcm;
    unsigned int device, action_value;

    unsigned int mbox_msg[MBOX_MSG_SIZE_FOR_ACTION] = {0};

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
        action_value = ASRC_PCM_VALUE_ACTION_START;
        break;
    case SNDRV_PCM_TRIGGER_STOP:
    case SNDRV_PCM_TRIGGER_SUSPEND:
    case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
        action_value = ASRC_PCM_VALUE_ACTION_STOP;
        break;
    default :
        pr_err("%s - invalid command\n", __func__);
        return -EINVAL;
    }

    mbox_header.usage = MBOX_AUDIO_USAGE_SET;
    mbox_header.cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
    mbox_header.msg_size = MBOX_MSG_SIZE_FOR_ACTION;
    mbox_header.tx_instance = 0;

    mbox_msg[0] = ASRC_PCM_ACTION;
    mbox_msg[1] = device;
    mbox_msg[2] = action_value;

    tcc_mbox_audio_send_message(mbox_audio_dev, &mbox_header, mbox_msg, NULL);

    asrc_m2m_pcm_dbg("[%s] mbox message : device = 0x%02x, action = 0x%01x\n", __func__, mbox_msg[1], mbox_msg[2]);

    return 0;
}

static int tcc_asrc_m2m_pcm_set_hw_params_to_mbox(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
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

    pcm = asrc_m2m_pcm->asrc_substream->pcm;
    device = pcm->card->number << 4 | (pcm->device & 0x0F);
	phy_address = asrc_m2m_pcm->middle->dma_buf->addr;
	if (!phy_address) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s] fail to get phy addr\n", __func__);
		return -EFAULT;
	}
    //printk("[%s] phy_address = 0x%08x\n", __func__, phy_address);
    mbox_header.usage = MBOX_AUDIO_USAGE_SET;
    mbox_header.cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
    mbox_header.msg_size = MBOX_MSG_SIZE_FOR_PARAMS;
    mbox_header.tx_instance = 0;

    mbox_msg[0] = ASRC_PCM_PARAMS;
    mbox_msg[1] = device;
    mbox_msg[2] = (asrc_m2m_pcm->asrc_substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? ASRC_PCM_VALUE_DIRECTON_TX : ASRC_PCM_VALUE_DIRECTON_RX;
    mbox_msg[3] = (unsigned int) phy_address;
    mbox_msg[4] = asrc_m2m_pcm->middle->buffer_bytes;

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
	int len=0;

    if (asrc_m2m_pcm == NULL) {
        printk("%s - asrc_m2m_pcm is NULL\n", __func__);
		goto playback_callback_end;
    }

	if(asrc_m2m_pcm->asrc_substream != NULL) {
		substream = asrc_m2m_pcm->asrc_substream;
	} else {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ERROR!! asrc_m2m_pcm->asrc_substream is null!!\n");
		goto playback_callback_end;
	}

	mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;
	mix_pos = asrc_m2m_pcm->middle->pre_pos;

	if(asrc_m2m_pcm->middle->cur_pos_from_ipc != asrc_m2m_pcm->middle->pre_pos_from_ipc) {
		if(asrc_m2m_pcm->middle->pre_pos_from_ipc > asrc_m2m_pcm->middle->cur_pos_from_ipc) {
			mix_byte = mid_buffer_bytes - asrc_m2m_pcm->middle->pre_pos_from_ipc;
			mix_byte += asrc_m2m_pcm->middle->cur_pos_from_ipc;
		} else {
			mix_byte = asrc_m2m_pcm->middle->cur_pos_from_ipc - asrc_m2m_pcm->middle->pre_pos_from_ipc;
		}
		//This line already be in the main function of mbox callback.
		//asrc_m2m_pcm->middle->pre_pos_from_ipc = asrc_m2m_pcm->middle->cur_pos_from_ipc;
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
		printk("[%s][%d]cur_pos=%d, pre_pos=%d\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->middle->cur_pos_from_ipc, (unsigned int)asrc_m2m_pcm->middle->pre_pos_from_ipc);
	}
*/	
	asrc_m2m_pcm->middle->pre_pos = mix_pos;
	cur_pos = asrc_m2m_pcm->middle->cur_pos;
	len = asrc_m2m_pcm->asrc_footprint->list_len;
	elapsed_en = false;
	while (len > 0) {
		head_pos = asrc_m2m_pcm->asrc_footprint->head->print_pos;
		mix_byte = asrc_m2m_pcm->asrc_footprint->head->input_byte;	
		if(cur_pos > mix_pos) {
			if((head_pos <= mix_pos)||(head_pos >= cur_pos)) {
				asrc_m2m_pcm->Bwrote += mix_byte;
				if(asrc_m2m_pcm->Bwrote >= asrc_m2m_pcm->src->buffer_bytes)
					asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote - asrc_m2m_pcm->src->buffer_bytes;
				len = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, false);
				elapsed_en = true;
			} else {
			//	printk("[%s][%d]cur_pos=%d, pre_pos=%d, head_pos=%d, len=%d, mix_byte=%d\n", __func__, __LINE__, cur_pos, mix_pos, head_pos, len, (unsigned int)mix_byte);
				break;
			}
		} else {
			if((head_pos <= mix_pos)&&(head_pos >= cur_pos)) {
				asrc_m2m_pcm->Bwrote += mix_byte;
				if(asrc_m2m_pcm->Bwrote >= asrc_m2m_pcm->src->buffer_bytes)
					asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote - asrc_m2m_pcm->src->buffer_bytes;
				len = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, false);
				elapsed_en = true;
			} else {
			//	printk("[%s][%d]cur_pos=%d, pre_pos=%d, head_pos=%d, len=%d, mix_byte=%d\n", __func__, __LINE__, cur_pos, mix_pos, head_pos, len, (unsigned int)mix_byte);
				break;
			}
		}
	}
playback_callback_end:
	if(elapsed_en == true) {
		snd_pcm_period_elapsed(substream);
	}
}

static void capture_for_mbox_callback(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	unsigned int read_pos=0;

	read_pos = asrc_m2m_pcm->middle->cur_pos_from_ipc;

#if	(CHECK_OVERRUN == 1)
	if((asrc_m2m_pcm->middle->cur_pos > asrc_m2m_pcm->middle->pre_pos)
			&&(read_pos > asrc_m2m_pcm->middle->pre_pos)&&(read_pos < asrc_m2m_pcm->middle->cur_pos)) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%u, read_pos=%u, cur_pos=%u\n", asrc_m2m_pcm->middle->pre_pos, read_pos, asrc_m2m_pcm->middle->cur_pos);

	} else if((asrc_m2m_pcm->middle->cur_pos < asrc_m2m_pcm->middle->pre_pos)
			&&((read_pos > asrc_m2m_pcm->middle->pre_pos)||(read_pos < asrc_m2m_pcm->middle->cur_pos))) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%u, read_pos=%u, cur_pos=%u\n", asrc_m2m_pcm->middle->pre_pos, read_pos, asrc_m2m_pcm->middle->cur_pos);
	}
#endif

	asrc_m2m_pcm->middle->cur_pos = read_pos;

	atomic_set(&asrc_m2m_pcm->wakeup, 1);
	wake_up_interruptible(&(asrc_m2m_pcm->kth_wq));
}

static void tcc_asrc_m2m_pcm_mbox_callback(void *data, unsigned int *msg, unsigned short msg_size, unsigned short cmd_type)
{
    struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	//unsigned int cmd = msg[0];	//for debug
	unsigned int device = msg[1];	//for debug
	unsigned int pos = msg[2];
	unsigned long flags;
	char Ctemp=0;

	if (asrc_m2m_pcm == NULL) {
		printk("[%s] Cannot get asrc_m2m_pcm driver data\n", __func__);
		return;
	}

    if (cmd_type != asrc_m2m_pcm->mbox_cmd_type) {
		printk("[%s] Invalid cmd type between %u and %u\n", __func__, cmd_type, asrc_m2m_pcm->mbox_cmd_type);
		return;
    }

    if (msg_size == AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE) {
        if (cmd_type == MBOX_AUDIO_CMD_TYPE_DATA_TX_0) {
            pos = msg[2];
        } else if (cmd_type == MBOX_AUDIO_CMD_TYPE_DATA_TX_1) {
            pos = msg[3];
        } else if (cmd_type == MBOX_AUDIO_CMD_TYPE_DATA_TX_2) {
            pos = msg[4];
        }
    }
	spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
	Ctemp = asrc_m2m_pcm->is_flag;
	spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
	if((Ctemp & IS_A7S_STARTED)
		&&(asrc_m2m_pcm->middle->cur_pos_from_ipc != pos)) {
		asrc_m2m_pcm->middle->pre_pos_from_ipc = asrc_m2m_pcm->middle->cur_pos_from_ipc;
		asrc_m2m_pcm->middle->cur_pos_from_ipc = pos;
		if((device != ASRC_PCM_VALUE_DEVICE_0_3)&&(device != ASRC_PCM_VALUE_DEVICE_1_3)) {	//For playback
			playback_for_mbox_callback(asrc_m2m_pcm);
		} else {			//For capture
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
	unsigned long flags;

	asrc_m2m_pcm_dbg("%s, id=%d\n", __func__, substream->pcm->device);

	if(asrc_m2m_pcm == NULL) {
		pr_err("%s - asrc_m2m_pcm is NULL\n", __func__);
		return -EFAULT;
	}

	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	snd_pcm_hw_constraint_step(substream->runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);

    if (substream->pcm->device == ASRC_7_1CH_DEV_NUM) {  
        snd_soc_set_runtime_hwparams(substream, &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_7_1CH]);
    } else {
        snd_soc_set_runtime_hwparams(substream, &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_STEREO]);
    }
	asrc_m2m_pcm->asrc_substream = substream;

	//Buffer info reset.
	asrc_m2m_pcm->app->pre_pos = 0;
	asrc_m2m_pcm->app->pre_ptr = 0;

	asrc_m2m_pcm->src->buffer_bytes = 0;
	asrc_m2m_pcm->src->period_bytes = 0;

	asrc_m2m_pcm->dst->buffer_bytes = 0;
	asrc_m2m_pcm->dst->period_bytes = 0;


	asrc_m2m_pcm->middle->cur_pos = 0;
	asrc_m2m_pcm->middle->pre_pos = 0;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	asrc_m2m_pcm->middle->cur_pos_from_ipc = 0;
	asrc_m2m_pcm->middle->pre_pos_from_ipc = 0;
#endif
	if(asrc_m2m_pcm->middle->dma_buf->area != NULL)
		memset(asrc_m2m_pcm->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
	spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
	asrc_m2m_pcm->is_flag = 0;
	spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);

	asrc_m2m_pcm->Bwrote = 0;

	if(asrc_m2m_pcm->first_open == false) {
		asrc_m2m_pcm->first_open = true;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
			pkth_id = (struct task_struct *) kthread_run(tcc_appl_ptr_check_thread_for_play, (void *)(asrc_m2m_pcm), "tcc_appl_ptr_check_thread_for_play");
		} else {
			pkth_id = (struct task_struct *) kthread_run(tcc_ptr_update_thread_for_capture, (void *)(asrc_m2m_pcm), "tcc_ptr_update_thread_for_capture");
		}
		if(pkth_id != NULL) asrc_m2m_pcm->kth_id = pkth_id;
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	switch(substream->pcm->device) {
	case 0:
		asrc_m2m_pcm->mbox_cmd_type = MBOX_AUDIO_CMD_TYPE_DATA_TX_0;
		break;
    case 1:
		asrc_m2m_pcm->mbox_cmd_type = MBOX_AUDIO_CMD_TYPE_DATA_TX_1;
		break;
	case 2:
		asrc_m2m_pcm->mbox_cmd_type = MBOX_AUDIO_CMD_TYPE_DATA_TX_2;
		break;
	case 3:
		asrc_m2m_pcm->mbox_cmd_type = MBOX_AUDIO_CMD_TYPE_DATA_RX;
		break;
	default:
		asrc_m2m_pcm_dbg("Not Supported devices %d\n", substream->pcm->device);
		asrc_m2m_pcm->mbox_cmd_type = -1;
	}

    if (asrc_m2m_pcm->mbox_cmd_type >= MBOX_AUDIO_CMD_TYPE_DATA_TX_0 && asrc_m2m_pcm->mbox_cmd_type <= MBOX_AUDIO_CMD_TYPE_DATA_RX) {
		asrc_m2m_pcm->mbox_audio_dev = tcc_mbox_audio_register_set_kernel_callback((void*)asrc_m2m_pcm, tcc_asrc_m2m_pcm_mbox_callback, asrc_m2m_pcm->mbox_cmd_type);
    }
#endif

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);
    return 0;
}

static int tcc_asrc_m2m_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	int ret=0;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s] start\n", __func__);
	asrc_m2m_pcm->app->pre_pos = 0;
	asrc_m2m_pcm->app->pre_ptr = 0;

	asrc_m2m_pcm->src->buffer_bytes = 0;
	asrc_m2m_pcm->src->period_bytes = 0;
	asrc_m2m_pcm->src->rate = 0;

	asrc_m2m_pcm->dst->buffer_bytes = 0;
	asrc_m2m_pcm->dst->period_bytes = 0;
	asrc_m2m_pcm->dst->rate = 0;

	asrc_m2m_pcm->middle->cur_pos = 0;
	asrc_m2m_pcm->middle->pre_pos = 0;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	asrc_m2m_pcm->middle->cur_pos_from_ipc = 0;
	asrc_m2m_pcm->middle->pre_pos_from_ipc = 0;
	if (asrc_m2m_pcm->mbox_audio_dev != NULL && asrc_m2m_pcm->mbox_cmd_type >= MBOX_AUDIO_CMD_TYPE_DATA_TX_0 && asrc_m2m_pcm->mbox_cmd_type <= MBOX_AUDIO_CMD_TYPE_DATA_RX) {
		tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->mbox_cmd_type);
    }
#endif
	atomic_set(&asrc_m2m_pcm->wakeup, 0);

	asrc_m2m_pcm->middle->buffer_bytes = 0;

	asrc_m2m_pcm->interval = 0;
	asrc_m2m_pcm->Bwrote = 0;

	if(asrc_m2m_pcm->asrc_footprint->list_len > 0) {
		ret = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, true);
	}
	tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);

	if (asrc_m2m_pcm->asrc_substream != NULL) { 
		asrc_m2m_pcm->asrc_substream = NULL;
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
	struct tcc_asrc_param_t *pasrc_param = asrc_m2m_pcm->asrc_m2m_param;

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

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "format : 0x%08x\n", format);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "channels : %d\n", channels);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "period_bytes : %u\n", period_bytes);
	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "buffer_bytes : %u\n", buffer_bytes);

	memset(substream->dma_buffer.area, 0, buffer_bytes);
	memset(pasrc_param, 0, sizeof(struct tcc_asrc_param_t));

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
		ratio_shift22 = ((uint64_t)0x400000 * ASRC_DST_RATE);
		do_div(ratio_shift22, rate);
	} else {
		ratio_shift22 = ((uint64_t)0x400000 * rate);
		do_div(ratio_shift22, ASRC_SRC_RATE);
	}

	pasrc_param->u.cfg.src_bitwidth = asrc_bitwidth;
	pasrc_param->u.cfg.dst_bitwidth = asrc_bitwidth;
	pasrc_param->u.cfg.channels = asrc_channels;
	pasrc_param->u.cfg.ratio_shift22 = (uint32_t)ratio_shift22;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 

		asrc_m2m_pcm->src->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->src->period_bytes = period_bytes;
		asrc_m2m_pcm->src->rate = rate;
		asrc_m2m_pcm->interval = TCC_APP_PTR_CHECK_INTERVAL_TX;

		asrc_m2m_pcm->dst->rate = ASRC_DST_RATE;
		asrc_m2m_pcm->dst->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->dst->period_bytes = PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->dst->period_bytes = MIN_PERIOD_BYTES;

		asrc_m2m_pcm->middle->buffer_bytes = asrc_m2m_pcm->dst->buffer_bytes * MID_BUFFER_CONST;
	} else {
		asrc_m2m_pcm->dst->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->dst->period_bytes = period_bytes;
		asrc_m2m_pcm->dst->rate = rate;
		asrc_m2m_pcm->interval = TCC_APP_PTR_CHECK_INTERVAL_RX;

		asrc_m2m_pcm->src->rate = ASRC_SRC_RATE;
		asrc_m2m_pcm->src->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->src->period_bytes = PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->src->period_bytes = MIN_PERIOD_BYTES;

		asrc_m2m_pcm->middle->buffer_bytes = asrc_m2m_pcm->src->buffer_bytes * MID_BUFFER_CONST; 
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_asrc_m2m_pcm_set_hw_params_to_mbox(asrc_m2m_pcm);
#endif
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned long flags;
	char Ctemp=0;
	int ret = -1;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);
	
	while(1) {
		spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
		Ctemp = asrc_m2m_pcm->is_flag;
		spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
		if(!(Ctemp & IS_ASRC_RUNNING)) {
			break;
		}
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s] ASRC is running.\n", __func__);
		msleep(1);
	}
	spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
	asrc_m2m_pcm->is_flag &= ~(IS_TRIG_STARTED | IS_ASRC_STARTED | IS_ASRC_RUNNING);
	spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
	atomic_set(&asrc_m2m_pcm->wakeup, 1);
	wake_up_interruptible(&(asrc_m2m_pcm->kth_wq));

	if(substream){
		snd_pcm_set_runtime_buffer(substream, NULL);
		ret=0;
	} else {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "%s-error\n", __func__);
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

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);

	while(1) {
		spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
		Ctemp = asrc_m2m_pcm->is_flag;
		spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
		if(!(Ctemp & IS_ASRC_RUNNING)) {
			break;
		}
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s] ASRC is running.\n", __func__);
		msleep(1);
	}

	asrc_m2m_pcm->app->pre_pos = 0;
	asrc_m2m_pcm->app->pre_ptr = 0;

	asrc_m2m_pcm->middle->cur_pos = 0;
	asrc_m2m_pcm->middle->pre_pos = 0;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	asrc_m2m_pcm->middle->cur_pos_from_ipc = 0;
	asrc_m2m_pcm->middle->pre_pos_from_ipc = 0;
#endif
	asrc_m2m_pcm->Bwrote = 0;

	if(asrc_m2m_pcm->asrc_footprint->list_len > 0) {
		// delete all footprint
		ret = tcc_asrc_footprint_delete(asrc_m2m_pcm->asrc_footprint, true);
	}
	tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);

	if(asrc_m2m_pcm->middle->dma_buf->area != NULL)
		memset(asrc_m2m_pcm->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
	memset(runtime->dma_area, 0x00, runtime->dma_bytes);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
//	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_asrc_param_t *pasrc_param = asrc_m2m_pcm->asrc_m2m_param;
	struct tcc_asrc_t *pasrc = asrc_m2m_pcm->asrc;
//	unsigned int temp=0;
	unsigned long flags;
	char Ctemp=0;
	int ret=-1;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);

	if(asrc_m2m_pcm == NULL) {
		return -EFAULT;
	}

	spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
	Ctemp = asrc_m2m_pcm->is_flag;
	spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				if(!(Ctemp & IS_ASRC_STARTED)) {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_START, PLAY\n");
					if(asrc_m2m_pcm->src->rate != asrc_m2m_pcm->dst->rate) {
						ret = tcc_asrc_m2m_start(asrc_m2m_pcm->asrc,
								asrc_m2m_pcm->pair_id, // Pair
								pasrc_param->u.cfg.src_bitwidth, // SRC Bit width
								pasrc_param->u.cfg.dst_bitwidth, // DST Bit width
								pasrc_param->u.cfg.channels, //Channels
								pasrc_param->u.cfg.ratio_shift22); // multiple of 2
					} else {
						ret = 0;
					}
					Ctemp |= IS_ASRC_STARTED;
				} else {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_START already, PLAY\n");
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
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
				}
#endif
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_START end ret=%d, PLAY\n", ret);
			} else {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_START, CAPTURE\n");
				if(!(Ctemp & IS_ASRC_STARTED)) {
					if(asrc_m2m_pcm->src->rate != asrc_m2m_pcm->dst->rate) {
						ret = tcc_asrc_m2m_start(pasrc,
								asrc_m2m_pcm->pair_id, // Pair
								pasrc_param->u.cfg.src_bitwidth, // SRC Bit width
								pasrc_param->u.cfg.dst_bitwidth, // DST Bit width
								pasrc_param->u.cfg.channels, //Channels
								pasrc_param->u.cfg.ratio_shift22); // multiple of 2
					} else {
						ret = 0;
					}
					Ctemp |= IS_ASRC_STARTED;

				} else {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_START already, CAPTURE\n");
				}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
				ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
				if(ret < 0) {
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
				} else {
					Ctemp |= IS_A7S_STARTED;
				}
#endif
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_START end ret=%d, CAPTURE\n", ret);
			}

			Ctemp |= IS_TRIG_STARTED;

			spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
			asrc_m2m_pcm->is_flag |= Ctemp;
			spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
			atomic_set(&asrc_m2m_pcm->wakeup, 1);
			wake_up_interruptible(&(asrc_m2m_pcm->kth_wq));
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:

			Ctemp &= ~IS_TRIG_STARTED;

			spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
			asrc_m2m_pcm->is_flag &= Ctemp;
			spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
			//atomic_set(&asrc_m2m_pcm->wakeup, 1);
			//wake_up_interruptible(&(asrc_m2m_pcm->kth_wq));

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_STOP, PLAY\n");
				if(asrc_m2m_pcm->src->rate != asrc_m2m_pcm->dst->rate) {
					while(1) {
						spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
						Ctemp = asrc_m2m_pcm->is_flag;
						spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
						if(!(Ctemp & IS_ASRC_RUNNING)) {
							break;
						}
						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s][%d] ASRC is running.\n", __func__, __LINE__);
						mdelay(1);
					}
					ret = tcc_asrc_m2m_stop(asrc_m2m_pcm->asrc, asrc_m2m_pcm->pair_id);
				} else {
					ret = 0;
				}
				Ctemp &= ~IS_ASRC_STARTED;
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_STOP ret=%d, PLAY\n", ret);
			} else {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_STOP, CAPTURE\n");
				if(asrc_m2m_pcm->src->rate != asrc_m2m_pcm->dst->rate) {
					while(1) {
						spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
						Ctemp = asrc_m2m_pcm->is_flag;
						spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
						if(!(Ctemp & IS_ASRC_RUNNING)) {
							break;
						}
						asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s][%d] ASRC is running.\n", __func__, __LINE__);
						mdelay(1);
					}
					ret = tcc_asrc_m2m_stop(asrc_m2m_pcm->asrc, asrc_m2m_pcm->pair_id);
				} else {
					ret = 0;
				}
				Ctemp &= ~IS_ASRC_STARTED;
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ASRC_TRIGGER_STOP ret=%d, CAPTURE\n", ret);
			}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
			if(ret < 0) {
				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
			} else {
				Ctemp &= ~IS_A7S_STARTED;
			}
#endif
			spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
			asrc_m2m_pcm->is_flag &= Ctemp;
			spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
			atomic_set(&asrc_m2m_pcm->wakeup, 1);
			wake_up_interruptible(&(asrc_m2m_pcm->kth_wq));
			break;
		default:
			return -EINVAL;
	}

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);

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
		ret = bytes_to_frames(runtime, asrc_m2m_pcm->Bwrote);
	} else {
		ret = asrc_m2m_pcm->app->pre_pos;
	}
	//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ret=%d\n", (int)ret);
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

static int tcc_ptr_update_function_for_capture(struct snd_pcm_substream *psubstream, ssize_t readable_byte, ssize_t max_cpy_byte)
{
	struct snd_pcm_substream *substream = psubstream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc; 
	snd_pcm_uframes_t Ftemp=0;
	ssize_t dst_buffer_bytes=0, dst_period_bytes=0;
	ssize_t read_pos=0, write_pos=0, temp_pos=0;
	ssize_t writeable_byte=0, read_byte=0, Wtemp=0, Btemp=0;
	unsigned long flags;
	char *pin_buf, *pout_buf, *ptemp_buf, Ctemp=0;

	ptemp_buf = asrc_m2m_pcm->middle->ptemp_buf;
	pin_buf = asrc_m2m_pcm->middle->dma_buf->area;
	pout_buf = substream->dma_buffer.area;

	dst_buffer_bytes = asrc_m2m_pcm->dst->buffer_bytes;
	dst_period_bytes = asrc_m2m_pcm->dst->period_bytes;
	while(1) {
		spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
		Ctemp = asrc_m2m_pcm->is_flag;
		spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
		if((readable_byte <= 0)
			||((Ctemp & IS_TRIG_STARTED) == 0)
			||((Ctemp & IS_ASRC_STARTED) == 0)) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "readable_byte: %d, is_flag=0x%02x\n", readable_byte, (unsigned int)Ctemp);
			break;//return 0;
		}

		if(readable_byte > max_cpy_byte) {
			read_byte = max_cpy_byte; 
		} else {
			read_byte = readable_byte; 
		}

		write_pos = frames_to_bytes(runtime, asrc_m2m_pcm->app->pre_pos);
		read_pos = asrc_m2m_pcm->middle->pre_pos;

		if(asrc_m2m_pcm->src->rate == asrc_m2m_pcm->dst->rate) {

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
			
			//check ptr size for update
			Ftemp = bytes_to_frames(runtime, read_byte);
			Btemp = frames_to_bytes(runtime, Ftemp);
			if(Btemp != read_byte) {
				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%d] read_byte=%d, Btemp=%d\n", __LINE__, read_byte, Btemp);
			}

			asrc_m2m_pcm->app->pre_pos += Ftemp;
			if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
				asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;

			asrc_m2m_pcm->app->pre_ptr += Ftemp;
			if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
				asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

			asrc_m2m_pcm->Bwrote += read_byte;
			if(asrc_m2m_pcm->Bwrote >= dst_period_bytes) {
				asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % dst_period_bytes;
				snd_pcm_period_elapsed(substream);
			}

		} else {
			spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
			asrc_m2m_pcm->is_flag |= IS_ASRC_RUNNING;
			spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
			writeable_byte = tcc_asrc_m2m_push_data(asrc, asrc_m2m_pcm->pair_id, pin_buf+read_pos, read_byte);
			if(write_pos + writeable_byte >= dst_buffer_bytes) {
				if(writeable_byte > TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST) {
					ptemp_buf = NULL;
					ptemp_buf = kzalloc(sizeof(char)*writeable_byte, GFP_KERNEL);
					if(ptemp_buf == NULL){
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!!\n", __func__, __LINE__);
						spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
						asrc_m2m_pcm->is_flag &= ~IS_ASRC_RUNNING;
						spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
						return -1;
					}
				} else {
					memset(ptemp_buf, 0, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);
				}
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, ptemp_buf, writeable_byte);
				spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
				asrc_m2m_pcm->is_flag &= ~IS_ASRC_RUNNING;
				spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);

				//1st part copy for out_buf
				temp_pos = dst_buffer_bytes - write_pos;
				memcpy(pout_buf+write_pos, ptemp_buf, temp_pos);

				//check ptr size for update
				Ftemp = bytes_to_frames(runtime, temp_pos);
				Btemp = frames_to_bytes(runtime, Ftemp);
				if(Btemp != temp_pos) {
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%d] temp_pos=%d, Btemp=%d\n", __LINE__, temp_pos, Btemp);
				}

				//1st part check ptr size for update
				asrc_m2m_pcm->app->pre_pos += Ftemp;
				if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
					asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
				asrc_m2m_pcm->app->pre_ptr += Ftemp;
				if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
					asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;
				
				asrc_m2m_pcm->Bwrote += temp_pos;
				if(asrc_m2m_pcm->Bwrote  >= dst_period_bytes) {
					asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % dst_period_bytes;
					snd_pcm_period_elapsed(substream);
				}

				//2nd part copy size
				Btemp = Wtemp - temp_pos;

				//2nd part copy for out_buf
				memcpy(pout_buf, ptemp_buf+temp_pos, Btemp);

				//check ptr size for update
				Ftemp = bytes_to_frames(runtime, Btemp);
				temp_pos = frames_to_bytes(runtime, Ftemp);
				if(Btemp != temp_pos) {
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%d] temp_pos=%d, Btemp=%d\n", __LINE__, temp_pos, Btemp);
				}

				//2nd part check ptr size for update
				asrc_m2m_pcm->app->pre_pos += Ftemp;
				if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
					asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
				asrc_m2m_pcm->app->pre_ptr += Ftemp;
				if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
					asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

				asrc_m2m_pcm->Bwrote += Btemp;
				if(asrc_m2m_pcm->Bwrote  >= dst_period_bytes) {
					asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % dst_period_bytes;
					snd_pcm_period_elapsed(substream);
				}

			} else {
				//copy for out_buf
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf+write_pos, writeable_byte);
				spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
				asrc_m2m_pcm->is_flag &= ~IS_ASRC_RUNNING;
				spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);

				//check ptr size for update
				Ftemp = bytes_to_frames(runtime, Wtemp);
				Btemp = frames_to_bytes(runtime, Ftemp);
				if(Btemp != Wtemp) {
					asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%d] Wtemp=%d, Btemp=%d\n", __LINE__, Wtemp, Btemp);
				}

				asrc_m2m_pcm->app->pre_pos += Ftemp;
				if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
					asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
				asrc_m2m_pcm->app->pre_ptr += Ftemp;
				if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
					asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

				asrc_m2m_pcm->Bwrote += Wtemp;
				if(asrc_m2m_pcm->Bwrote  >= dst_period_bytes) {
					asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % dst_period_bytes;
					snd_pcm_period_elapsed(substream);
				}
			}
		}
		asrc_m2m_pcm->middle->pre_pos += read_byte;
		if(asrc_m2m_pcm->middle->pre_pos >= asrc_m2m_pcm->middle->buffer_bytes) {
			asrc_m2m_pcm->middle->pre_pos = asrc_m2m_pcm->middle->pre_pos - asrc_m2m_pcm->middle->buffer_bytes;
		}
		readable_byte -= read_byte;
		if(readable_byte < 0) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ERROR!! readable_byte: %d\n", readable_byte);
			return -1;
		}
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "readable_byte: %d, read_byte: %d, pre_pos: %d\n", readable_byte, read_byte, asrc_m2m_pcm->middle->pre_pos);
	}
	return 0;
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
	unsigned long flags;
	char Ctemp=0;
	int readable_byte=0;
	int ret=0;

	while(!kthread_should_stop()) {
		spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
		Ctemp = asrc_m2m_pcm->is_flag;
		spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
		if((Ctemp & IS_TRIG_STARTED)&&(Ctemp & IS_ASRC_STARTED)) {
			if (asrc_m2m_pcm->middle->cur_pos != asrc_m2m_pcm->middle->pre_pos) {
    			substream = asrc_m2m_pcm->asrc_substream;
    			rtd = substream->private_data;
    			runtime = substream->runtime;
    
    			cur_pos = asrc_m2m_pcm->middle->cur_pos;
    			pre_pos = asrc_m2m_pcm->middle->pre_pos;
    
    			mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;
    
    			temp = frames_to_bytes(runtime, runtime->period_size);
    			if(temp > TCC_ASRC_MAX_SIZE) {
    				max_cpy_byte = TCC_ASRC_MAX_SIZE;
    			} else {
    				max_cpy_byte = temp;
    			}
    			temp=0;
    
    			if(cur_pos >= pre_pos) {
    				//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, cur_pos, pre_pos);
    				readable_byte = cur_pos - pre_pos;
    				if(readable_byte > 0) {
    					ret = tcc_ptr_update_function_for_capture(substream, readable_byte, max_cpy_byte);
    					if(ret < 0) {
    						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
    					}
    				}
    			} else {			
    				//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, cur_pos, pre_pos);
					
    				//1st part copy for in_buf
    				readable_byte = mid_buffer_bytes - pre_pos;
    
    				if(readable_byte > 0){
    					ret = tcc_ptr_update_function_for_capture(substream, readable_byte, max_cpy_byte);
    					if(ret < 0) {
    						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
    					}
    				}
    				
    				//2nd part copy
    				readable_byte = cur_pos;
    				//asrc_m2m_pcm->middle->pre_pos = 0;
    				if(readable_byte > 0) {
    					ret = tcc_ptr_update_function_for_capture(substream, readable_byte, max_cpy_byte);
    					if(ret < 0) {
    						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
    					}
    				}
    			} //(cur_pos > pre_pos)?
			} else {
				wait_event_interruptible_timeout(asrc_m2m_pcm->kth_wq, atomic_read(&asrc_m2m_pcm->wakeup), msecs_to_jiffies(asrc_m2m_pcm->interval));
			    atomic_set(&asrc_m2m_pcm->wakeup, 0);
			}
		} else { //start or closed?
		    wait_event_interruptible(asrc_m2m_pcm->kth_wq, ((asrc_m2m_pcm->is_flag & IS_TRIG_STARTED) && (asrc_m2m_pcm->is_flag & IS_ASRC_STARTED)));
			atomic_set(&asrc_m2m_pcm->wakeup, 0);
		}
	}//while
	return 0;
}

static ssize_t tcc_appl_ptr_check_function_for_play(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm, ssize_t readable_byte, ssize_t max_cpy_byte, char *pin_buf)
{
    struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc;
	unsigned int mid_buffer_bytes=0, write_pos=0, temp_pos=0, pre_pos=0;
	ssize_t read_byte=0, writeable_byte=0;
	ssize_t Btemp=0, Wtemp=0, ret=0;
	unsigned long flags;
	int len=0;
	char *pout_buf, *ptemp_buf;

	write_pos = asrc_m2m_pcm->middle->cur_pos;
	pre_pos = asrc_m2m_pcm->middle->pre_pos;

	mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;

	ptemp_buf = asrc_m2m_pcm->middle->ptemp_buf;
	pout_buf = asrc_m2m_pcm->middle->dma_buf->area;

	if(readable_byte > max_cpy_byte) {
		read_byte = max_cpy_byte;
	} else {
		read_byte = readable_byte;
	}

	if(read_byte <= 0) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! readable_byte=%d, max_cpy_byte=%d\n", __func__, __LINE__, readable_byte, max_cpy_byte);
		return -1;
	}
	
	if(asrc_m2m_pcm->src->rate == asrc_m2m_pcm->dst->rate) {
		if(write_pos + read_byte >= mid_buffer_bytes) {
			//1st part copy for out_buf
			Btemp = mid_buffer_bytes - write_pos;
			memcpy(pout_buf+write_pos, pin_buf, Btemp);
			temp_pos = Btemp;
			//2nd part copy for out_buf
			Btemp = read_byte - Btemp;
			memcpy(pout_buf, pin_buf+temp_pos, Btemp);

			asrc_m2m_pcm->middle->cur_pos = Btemp;
		} else {
			memcpy(pout_buf+write_pos, pin_buf, read_byte);
			asrc_m2m_pcm->middle->cur_pos += read_byte; 
		}

		ret = read_byte;
	} else {
		spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
		asrc_m2m_pcm->is_flag |= IS_ASRC_RUNNING;
		spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
		writeable_byte = tcc_asrc_m2m_push_data(asrc, asrc_m2m_pcm->pair_id, pin_buf, read_byte);
		if(write_pos + writeable_byte >= mid_buffer_bytes) {
			if(writeable_byte > TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST) {
				ptemp_buf = NULL;
				ptemp_buf = kzalloc(sizeof(char)*writeable_byte, GFP_KERNEL);
				if(ptemp_buf == NULL){
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!!\n", __func__, __LINE__);
					spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
					asrc_m2m_pcm->is_flag &= ~IS_ASRC_RUNNING;
					spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
					return -1;
				}
			}
			Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, ptemp_buf, writeable_byte);

			spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
			asrc_m2m_pcm->is_flag &= ~IS_ASRC_RUNNING;
			spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);

			//1st part copy for out_buf
			Btemp = mid_buffer_bytes - write_pos;
			memcpy(pout_buf+write_pos, ptemp_buf, Btemp);

			//2nd part copy for out_buf
			temp_pos = Btemp;
			Btemp = Wtemp - Btemp;
			memcpy(pout_buf, ptemp_buf+temp_pos, Btemp);

			asrc_m2m_pcm->middle->cur_pos = write_pos + Wtemp - mid_buffer_bytes;
			memset(ptemp_buf, 0, TCC_ASRC_MAX_SIZE*MID_BUFFER_CONST);

		} else {
			Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf+write_pos, writeable_byte);
			spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
			asrc_m2m_pcm->is_flag &= ~IS_ASRC_RUNNING;
			spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);

			asrc_m2m_pcm->middle->cur_pos += Wtemp;

		}
		ret = read_byte;
	}

	len = tcc_asrc_footprint_insert(asrc_m2m_pcm->asrc_footprint, asrc_m2m_pcm->middle->cur_pos, read_byte);

#if	(CHECK_OVERRUN == 1)
	if((write_pos > pre_pos)
			&&(asrc_m2m_pcm->middle->cur_pos > pre_pos)&&(write_pos > asrc_m2m_pcm->middle->cur_pos)) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%u, write_pos=%u, cur_pos=%u\n", pre_pos, write_pos, asrc_m2m_pcm->middle->cur_pos);

	} else if((write_pos < pre_pos)
			&&((asrc_m2m_pcm->middle->cur_pos > pre_pos)||(write_pos > asrc_m2m_pcm->middle->cur_pos))) {
		asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%u, write_pos=%u, cur_pos=%u\n", pre_pos, write_pos, asrc_m2m_pcm->middle->cur_pos);
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
	unsigned long flags;
#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
	snd_pcm_uframes_t read_byte;
#endif

	while(!kthread_should_stop()) {

wait_check_play:

		spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
		Ctemp = asrc_m2m_pcm->is_flag;
		spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
		if((Ctemp & IS_TRIG_STARTED)&&(Ctemp & IS_ASRC_STARTED)) {

			substream = asrc_m2m_pcm->asrc_substream;
			if(!substream) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] is_flag=0x%02x\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->is_flag);
				goto wait_check_play;
			}

			rtd = substream->private_data;
			if(!rtd) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] is_flag=0x%02x\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->is_flag);
				goto wait_check_play;
			}

			runtime = substream->runtime;
			if(!runtime) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] is_flag=0x%02x\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->is_flag);
				goto wait_check_play;
			}

			cur_appl_ptr = runtime->control->appl_ptr;
			pre_appl_ptr = asrc_m2m_pcm->app->pre_ptr;

			if((cur_appl_ptr != pre_appl_ptr)) {
				cur_appl_ofs = cur_appl_ptr % runtime->buffer_size;
				/*	//ehk23 This does not need.
				if((cur_appl_ofs == 0) && (cur_appl_ptr > 0)) {
					cur_appl_ofs = runtime->buffer_size;
				}*/
				pre_appl_ofs = asrc_m2m_pcm->app->pre_pos;

				Btemp = frames_to_bytes(runtime, runtime->period_size);
				if(Btemp > TCC_ASRC_MAX_SIZE) {
					max_cpy_byte = TCC_ASRC_MAX_SIZE;
				} else {
					max_cpy_byte = Btemp;
				}
				Btemp=0;

				pin_buf = substream->dma_buffer.area;

				if(cur_appl_ofs > pre_appl_ofs) {
					//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%lu, pre=%lu, cur_ptr=%lu, pre_ptr=%lu\n", __func__, __LINE__, cur_appl_ofs, pre_appl_ofs, cur_appl_ptr, pre_appl_ptr);
					Ftemp = cur_appl_ofs - pre_appl_ofs;
					readable_byte = frames_to_bytes(runtime, Ftemp);
					read_pos = frames_to_bytes(runtime, asrc_m2m_pcm->app->pre_pos);

					if(readable_byte > 0) {

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&start);
#endif

						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, max_cpy_byte, pin_buf+read_pos);
						if(ret < 0) {
							asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
						}

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&end);

						elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
						do_div(elapsed_usecs64, NSEC_PER_USEC);
						elapsed_usecs = elapsed_usecs64;

						printk("asrc m2m elapsed time : %03d usec, %ldbytes\n", elapsed_usecs, ret);
#endif

						Ftemp = bytes_to_frames(runtime, ret);
						Btemp = frames_to_bytes(runtime, Ftemp);
						if(Btemp != ret) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%d] ret=%d, Btemp=%d\n", __LINE__, ret, Btemp);
						}
						 

						asrc_m2m_pcm->app->pre_pos += Ftemp;
						if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size) {
							asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
						}

						asrc_m2m_pcm->app->pre_ptr += Ftemp;
						if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
							asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

					}
				} else {	//(cur_appl_ofs > pre_appl_ofs)?
					//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%lu, pre=%lu, cur_ptr=%lu, pre_ptr=%lu\n", __func__, __LINE__, cur_appl_ofs, pre_appl_ofs, cur_appl_ptr, pre_appl_ptr);

					//1st part copy for in_buf
					Ftemp = runtime->buffer_size - pre_appl_ofs;
					readable_byte = frames_to_bytes(runtime, Ftemp);
					read_pos = frames_to_bytes(runtime, asrc_m2m_pcm->app->pre_pos);

					if(readable_byte > 0) {
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, max_cpy_byte, pin_buf+read_pos);
						if(ret < 0) {
							asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
						}

						Ftemp = bytes_to_frames(runtime, ret);
						Btemp = frames_to_bytes(runtime, Ftemp);
						if(Btemp != ret) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%d] ret=%d, Btemp=%d\n", __LINE__, ret, Btemp);
						}

						asrc_m2m_pcm->app->pre_pos += Ftemp;
						if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size) {
							asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
						}

						asrc_m2m_pcm->app->pre_ptr += Ftemp;
						if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
							asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

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
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_byte, max_cpy_byte, pin_buf);
						if(ret < 0) {
							asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
						}

						Ftemp = bytes_to_frames(runtime, ret);
						Btemp = frames_to_bytes(runtime, Ftemp);
						if(Btemp != ret) {
							asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%d] ret=%d, Btemp=%d\n", __LINE__, ret, Btemp);
						}


						asrc_m2m_pcm->app->pre_pos = Ftemp;
						if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size) {
							asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
						}

						asrc_m2m_pcm->app->pre_ptr += Ftemp;
						if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
							asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

					}
				} //(cur_appl_ofs > pre_appl_ofs)? else?
max_cpy_tx:
				Ftemp=0;

				spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
				Ctemp = asrc_m2m_pcm->is_flag;
				spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
			    if((!(Ctemp & IS_A7S_STARTED))
        			&&(Ctemp & IS_ASRC_STARTED)
					&&(asrc_m2m_pcm->app->pre_ptr >= runtime->buffer_size)) {
        
        			substream = asrc_m2m_pcm->asrc_substream;
        
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
        			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, SNDRV_PCM_TRIGGER_START);
        			if(ret < 0) {
        				asrc_m2m_pcm_dbg_id_err(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
					} else {
						spin_lock_irqsave(&asrc_m2m_pcm->is_locked, flags);
						asrc_m2m_pcm->is_flag |= IS_A7S_STARTED;
						spin_unlock_irqrestore(&asrc_m2m_pcm->is_locked, flags);
        			}
#endif
        		}

			} else { //(cur_appl_ptr != pre_appl_ptr)?
				/*
				 * This kthread check the appl_ptr in every interval (default 5ms).
				 */
				wait_event_interruptible_timeout(asrc_m2m_pcm->kth_wq, atomic_read(&asrc_m2m_pcm->wakeup), msecs_to_jiffies(asrc_m2m_pcm->interval));
				atomic_set(&asrc_m2m_pcm->wakeup, 0);
			}	//(cur_appl_ptr != pre_appl_ptr)? else?
		} else { //start or closed? 
			wait_event_interruptible(asrc_m2m_pcm->kth_wq, ((asrc_m2m_pcm->is_flag & IS_TRIG_STARTED) && (asrc_m2m_pcm->is_flag & IS_ASRC_STARTED)));
			atomic_set(&asrc_m2m_pcm->wakeup, 0);
		} //start or closed? else ? 
	}//while
	return 0;
}

static int tcc_asrc_m2m_pcm_new(struct snd_soc_pcm_runtime *rtd) 
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm	= rtd->pcm;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);
	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret) {
		return ret;
	}

	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, card->dev, MAX_BUFFER_BYTES*MID_BUFFER_CONST, asrc_m2m_pcm->middle->dma_buf);
	if (ret) {
		return ret;
	}
	ret = snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
			card->dev, MAX_BUFFER_BYTES, MAX_BUFFER_BYTES);
	if (ret) {
		return ret;
	}

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);

	return 0;
}

static void tcc_asrc_m2m_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);
	snd_pcm_lib_preallocate_free_for_all(pcm);
	snd_dma_free_pages(asrc_m2m_pcm->middle->dma_buf);

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);
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

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] start\n", __func__, __LINE__);

	if(asrc_m2m_pcm->first_open == true) {
		if (asrc_m2m_pcm->kth_id != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->kth_id);
			asrc_m2m_pcm->kth_id = NULL;
		}
		asrc_m2m_pcm->first_open = false;
	}

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);
    return 0;
}

static int tcc_asrc_m2m_pcm_resume(struct platform_device *pdev)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)platform_get_drvdata(pdev);
	asrc_m2m_pcm->first_open = false;
    return 0;
}
#endif

static int tcc_asrc_m2m_pcm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm;
	struct tcc_asrc_t *asrc;
	struct tcc_asrc_param_t *asrc_m2m_param;
	struct snd_pcm_substream *asrc_substream;
	struct tcc_mid_buf *middle;

	struct tcc_param_info *src;
	struct tcc_param_info *dst;
	struct tcc_app_buffer_info *app;

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

	asrc_m2m_param = (struct tcc_asrc_param_t*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_asrc_param_t), GFP_KERNEL);
	if(!asrc_m2m_param) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_asrc_m2m_param;
	} else {
		memset(asrc_m2m_param, 0, sizeof(struct tcc_asrc_param_t));
	}

	asrc_substream = (struct snd_pcm_substream*)devm_kzalloc(&pdev->dev, sizeof(struct snd_pcm_substream), GFP_KERNEL);
	if(!asrc_substream) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_asrc_substream;
	} else {
		memset(asrc_substream, 0, sizeof(struct snd_pcm_substream));
	}

	middle = (struct tcc_mid_buf*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_mid_buf), GFP_KERNEL);
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

	src = (struct tcc_param_info*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_param_info), GFP_KERNEL);
	if(!src) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_src;
	} else {
		memset(src, 0, sizeof(struct tcc_param_info));
	}

	dst = (struct tcc_param_info*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_param_info), GFP_KERNEL);
	if(!dst) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_dst;
	} else {
		memset(dst, 0, sizeof(struct tcc_param_info));
	}

	app = (struct tcc_app_buffer_info*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_app_buffer_info), GFP_KERNEL);
	if(!app) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_app;
	} else {
		memset(app, 0, sizeof(struct tcc_app_buffer_info));
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


	asrc_m2m_pcm->first_open = false;

	asrc_m2m_pcm->asrc = asrc;

	asrc_m2m_pcm->asrc_m2m_param = asrc_m2m_param;
	asrc_m2m_pcm->asrc_substream = asrc_substream;
	asrc_m2m_pcm->middle = middle;

	asrc_m2m_pcm->src = src;
	asrc_m2m_pcm->dst = dst;
	asrc_m2m_pcm->app = app;

    asrc_m2m_pcm->pair_id = -1;
    asrc_m2m_pcm->pair_id = of_alias_get_id(pdev->dev.of_node, "asrc_m2m_pcm");
	if(asrc_m2m_pcm->pair_id < 0) {
		asrc_m2m_pcm_dbg("[%s] ERROR!! id[%d] is wrong.\n", __func__, asrc_m2m_pcm->pair_id);
		ret = -EINVAL;
		goto error_of_node;
	}

    asrc_m2m_pcm->dev = &pdev->dev;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	asrc_m2m_pcm->mbox_cmd_type = -1;
    asrc_m2m_pcm->mbox_audio_dev = get_tcc_mbox_audio_device();
    if (asrc_m2m_pcm->mbox_audio_dev == NULL) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s] ERROR!! mbox_audio_dev is NULL!\n", __func__);
		goto error_mailbox;
    }
#endif
	asrc_m2m_pcm->asrc_footprint = (List *)kzalloc(sizeof(List), GFP_KERNEL);
	if(!asrc_m2m_pcm->asrc_footprint) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_footprint;
	} else {
		tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);
	}

    atomic_set(&asrc_m2m_pcm->wakeup, 0);

	init_waitqueue_head(&asrc_m2m_pcm->kth_wq);
	spin_lock_init(&asrc_m2m_pcm->is_locked);

	platform_set_drvdata(pdev, asrc_m2m_pcm);

	ret = snd_soc_register_platform(&pdev->dev, &tcc_asrc_m2m_pcm_platform);
	if (ret < 0) {
		printk("tcc_asrc_m2m_pcm_platform_register failed\n");
		goto error_register;
	}
	//asrc_m2m_pcm_dbg("tcc_asrc_m2m_pcm_platform_register success\n");

	asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] end\n", __func__, __LINE__);
	return ret;

error_register:
	kfree(asrc_m2m_pcm->asrc_footprint);
error_footprint:
error_mailbox:
error_of_node:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->app);
error_app:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->dst);
error_dst:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->src);
error_src:
	kfree(asrc_m2m_pcm->middle->ptemp_buf);
error_middle_temp_buf:
	kfree(asrc_m2m_pcm->middle->dma_buf);
error_middle_buf:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->middle);
error_middle:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->asrc_substream);
error_asrc_substream:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->asrc_m2m_param);
error_asrc_m2m_param:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->asrc);
error_asrc:
	devm_kfree(&pdev->dev, asrc_m2m_pcm);
error_pcm:

	return ret;
}

static int tcc_asrc_m2m_pcm_remove(struct platform_device *pdev)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)platform_get_drvdata(pdev);
	asrc_m2m_pcm_dbg("%s\n", __func__);

	if(asrc_m2m_pcm->first_open == true) {
		if (asrc_m2m_pcm->kth_id != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->kth_id);
			asrc_m2m_pcm->kth_id = NULL;
		}
		asrc_m2m_pcm->first_open = false;
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->mbox_cmd_type);
#endif
	kfree(asrc_m2m_pcm->asrc_footprint);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->app);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->dst);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->src);
	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->middle);
	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->asrc_m2m_param);
	kfree(asrc_m2m_pcm->middle->ptemp_buf);
	kfree(asrc_m2m_pcm->middle->dma_buf);
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
