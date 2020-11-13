/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_asrc_m2m_pcm.h"

#if 0
#define tpcm_dbg(f, a...) \
	pr_info("[DEBUG][TSND_PCM]" f, ##a)
#define tpcm_info(f, a...) \
	pr_info(f, ##a)
#else
#define tpcm_dbg(f, a...)
#define tpcm_info(f, a...)
#endif
#define tpcm_err(f, a...) \
	pr_err("[ERROR][T-sound_PCM]" f, ##a)
#define tpcm_warn(f, a...) \
	pr_err("[WARN][T-sound_PCM]" f, ##a)

#define CHECK_OVERRUN \
	(0)
#define CHECK_ASRC_M2M_ELAPSED_TIME \
	(0)

static void tcc_asrc_m2m_pcm_stream_reset(
	struct asrc_m2m_pcm_stream *strm,
	bool prepare);
static int tcc_asrc_m2m_pcm_asrc_stop(struct snd_pcm_substream *substream);
static char tcc_is_flag_update(
	struct asrc_m2m_pcm_stream *strm,
	char is_flag,
	char mode);
static int tcc_ptr_update_thread_for_capture(void *data);
static int tcc_appl_ptr_check_thread_for_play(void *data);

//#define TAIL_DEBUG
#ifdef TAIL_DEBUG
ssize_t cap_tail;
ssize_t play_tail;
#endif

static const struct snd_pcm_hardware
	tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_TYPE_MAX]	= {
	[TCC_ASRC_M2M_7_1CH] = {
		.info =
			(SNDRV_PCM_INFO_MMAP
			|SNDRV_PCM_INFO_MMAP_VALID
			|SNDRV_PCM_INFO_INTERLEAVED
			|SNDRV_PCM_INFO_BLOCK_TRANSFER
			|SNDRV_PCM_INFO_PAUSE
			|SNDRV_PCM_INFO_RESUME),

		.formats =
			(SNDRV_PCM_FMTBIT_S16_LE
			|SNDRV_PCM_FMTBIT_S24_LE),
		.rates = SNDRV_PCM_RATE_8000_192000,
		.rate_min = 8000,
		.rate_max = 192000,
		.channels_min = 2,
		.channels_max = 8,
		.period_bytes_min = MIN_PERIOD_BYTES,
		.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
		.periods_min = MIN_PERIOD_CNT,
		.periods_max =
		MAX_BUFFER_BYTES / MIN_PERIOD_BYTES,
		.buffer_bytes_max = MAX_BUFFER_BYTES,
		.fifo_size = 16,
	},
	[TCC_ASRC_M2M_STEREO] = {
		.info =
			(SNDRV_PCM_INFO_MMAP
			|SNDRV_PCM_INFO_MMAP_VALID
			|SNDRV_PCM_INFO_INTERLEAVED
			|SNDRV_PCM_INFO_BLOCK_TRANSFER
			|SNDRV_PCM_INFO_PAUSE
			|SNDRV_PCM_INFO_RESUME),

		.formats =
			(SNDRV_PCM_FMTBIT_S16_LE
			|SNDRV_PCM_FMTBIT_S24_LE),
		.rates = SNDRV_PCM_RATE_8000_192000,
		.rate_min = 8000,
		.rate_max = 192000,
		.channels_min = 2,
		.channels_max = 2,
		.period_bytes_min = MIN_PERIOD_BYTES,
		.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
		.periods_min = MIN_PERIOD_CNT,
		.periods_max =
		MAX_BUFFER_BYTES / MIN_PERIOD_BYTES,
		.buffer_bytes_max = MAX_BUFFER_BYTES,
		.fifo_size = 16,
	},
};

#ifdef FOOTPRINT_LINKED_LIST
static void tcc_asrc_footprint_init(struct List *list)
{
	unsigned long flags;

	spin_lock_init(&list->foot_locked);

	spin_lock_irqsave(&list->foot_locked, flags);
	list->head = NULL;
//      list->current = NULL;
	list->tail = NULL;
	list->list_len = 0;
	spin_unlock_irqrestore(&list->foot_locked, flags);
}

//From tail insert
static int tcc_asrc_footprint_insert(struct List *list,
	unsigned int pos, ssize_t size)
{
	struct Node *new_node = NULL;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&list->foot_locked, flags);

	new_node = kzalloc(sizeof(struct Node), GFP_KERNEL);
	if (!new_node) {
		spin_unlock_irqrestore(&list->foot_locked, flags);
		tpcm_err("[%s][%d]ERROR!!\n", __func__,
		       __LINE__);
		return -EFAULT;
	}

	new_node->print_pos = pos;
	new_node->input_byte = size;
	new_node->next = NULL;

	if (list->list_len > 0)
		list->tail->next = new_node;
	else
		list->head = new_node;

	list->tail = new_node;
	(list->list_len)++;

	ret = list->list_len;

	spin_unlock_irqrestore(&list->foot_locked, flags);

	return ret;
}

//Get list_len.
static int tcc_asrc_footprint_get_length(List *list)
{
	//unsigned long flags;
	int ret = 0;
	//spin_lock_irqsave(&list->foot_locked, flags);
	ret = list->list_len;
	//spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}

//Get head position
static unsigned int tcc_asrc_footprint_get_head_position(List *list)
{
	//unsigned long flags;
	unsigned int ret = 0;
	//spin_lock_irqsave(&list->foot_locked, flags);
	ret = list->head->print_pos;
	//spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}

//Get head byte
static ssize_t tcc_asrc_footprint_get_head_inputbyte(List *list)
{
	//unsigned long flags;
	ssize_t ret = 0;
	//spin_lock_irqsave(&list->foot_locked, flags);
	ret = list->head->input_byte;
	//spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}

//From head delete.
static int tcc_asrc_footprint_delete(struct List *list, bool all)
{
	struct Node *dummy = NULL;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&list->foot_locked, flags);
	if (all == FALSE) {
		if (list->list_len <= 0) {
			spin_unlock_irqrestore(&list->foot_locked, flags);
			tpcm_err(
				"[%s][%d]There is no footprint (len=%d)\n",
			     __func__,
				 __LINE__,
				 list->list_len);
			return -EFAULT;
		} else if (list->list_len == 1) {
			kfree(list->tail);
			(list->list_len)--;
		} else {
			dummy = list->head;
			list->head = dummy->next;
			kfree(dummy);
			(list->list_len)--;
		}
	} else {		//delete all
		while (list->list_len > 0) {
			if (list->list_len == 1) {
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

	ret = list->list_len;
	spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}
#else
static void tcc_asrc_footprint_init(struct footprint *list)
{
	unsigned long flags;

	spin_lock_init(&list->foot_locked);

	spin_lock_irqsave(&list->foot_locked, flags);
	list->head = 0;
	list->tail = 0;
	list->list_len = 0;
	spin_unlock_irqrestore(&list->foot_locked, flags);
}

//From tail insert
static int tcc_asrc_footprint_insert(
	struct footprint *list,
	unsigned int pos,
	ssize_t size)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&list->foot_locked, flags);
	if ((list->list_len > 0) && (list->list_len < FOOTPRINT_LENGTH)) {
		list->tail++;
	} else if (list->list_len >= FOOTPRINT_LENGTH) {
		spin_unlock_irqrestore(&list->foot_locked, flags);
		tpcm_err(
			"[%s][%d]There is no footprint (len=%d, head=%d, tail=%d)\n",
		     __func__,
			 __LINE__,
			 list->list_len,
			 list->head,
		     list->tail);
		return -1;
	}
	if (list->tail >= FOOTPRINT_LENGTH)
		list->tail = list->tail - FOOTPRINT_LENGTH;

	list->print_pos[list->tail] = pos;
	list->input_byte[list->tail] = size;
	(list->list_len)++;

	ret = list->list_len;
	spin_unlock_irqrestore(&list->foot_locked, flags);

	return ret;
}

#if 0				//For Debug
//Dump the List.
static void tcc_asrc_footprint_dump(struct footprint *list)
{
	unsigned long flags;
	int i = 0;

	spin_lock_irqsave(&list->foot_locked, flags);
	for (i = 0; i < FOOTPRINT_LENGTH; i++) {
		tpcm_err(" pos[%d]=%d, size[%d]=%d\n",
			i,
		    list->print_pos[i],
			i,
			list->input_byte[i]);
	}
	spin_unlock_irqrestore(&list->foot_locked, flags);
}
#endif
//Get list_len.
static int tcc_asrc_footprint_get_length(struct footprint *list)
{
	//unsigned long flags;
	int ret = 0;
	//spin_lock_irqsave(&list->foot_locked, flags);
	ret = list->list_len;
	//spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}

//Get head position
static unsigned int tcc_asrc_footprint_get_head_position(struct footprint *list)
{
	//unsigned long flags;
	unsigned int ret = 0;
	//spin_lock_irqsave(&list->foot_locked, flags);
	ret = list->print_pos[list->head];
	//spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}

//Get head byte
static ssize_t tcc_asrc_footprint_get_head_inputbyte(struct footprint *list)
{
	//unsigned long flags;
	ssize_t ret = 0;
	//spin_lock_irqsave(&list->foot_locked, flags);
	ret = list->input_byte[list->head];
	//spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}

//From head delete.
static int tcc_asrc_footprint_delete(struct footprint *list, bool all)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&list->foot_locked, flags);
	if (all == FALSE) {
		if (list->list_len <= 0) {
			spin_unlock_irqrestore(&list->foot_locked, flags);
			tpcm_err(
			"[%s][%d] There is no footprint (len=%d)\n",
			     __func__,
				 __LINE__,
				 list->list_len);
			return -EFAULT;
		} else if (list->list_len == 1) {
			goto list_delete_all;
		} else {
			list->head++;
			if (list->head >= FOOTPRINT_LENGTH)
				list->head = list->head - FOOTPRINT_LENGTH;

			list->list_len--;
		}
	} else {		//delete all
list_delete_all:
		memset(list->print_pos, 0x0, sizeof(list->print_pos));
		memset(list->input_byte, 0x0, sizeof(list->input_byte));
		list->head = 0;
		list->tail = 0;
		list->list_len = 0;
	}
	ret = list->list_len;
	spin_unlock_irqrestore(&list->foot_locked, flags);
	return ret;
}
#endif
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
static int tcc_asrc_m2m_pcm_set_action_to_mbox(
	struct mbox_audio_device *mbox_audio_dev,
	struct snd_pcm_substream
	*substream,
	int cmd)
{
	struct mbox_audio_data_header_t mbox_header;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO_R5
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	(struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	//Added for R5
#endif//CONFIG_TCC_MULTI_MAILBOX_AUDIO_R5
	const struct snd_pcm *pcm;
	unsigned int device, action_value;

	unsigned int mbox_msg[MBOX_MSG_SIZE_FOR_ACTION] = { 0, };

	if (substream == NULL) {
		tpcm_err("[%s] substream is NULL\n", __func__);
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
	default:
		tpcm_err("[%s] invalid command\n", __func__);
		return -EINVAL;
	}

	mbox_header.usage = MBOX_AUDIO_USAGE_SET;
	mbox_header.cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
	mbox_header.msg_size = MBOX_MSG_SIZE_FOR_ACTION;
	mbox_header.tx_instance = 0;

	mbox_msg[0] = PCM_ACTION;
	mbox_msg[1] = device;
	mbox_msg[2] =
	    (substream->stream ==
	     SNDRV_PCM_STREAM_PLAYBACK) ? PCM_VALUE_DIRECTON_TX :
	    PCM_VALUE_DIRECTON_RX;
	mbox_msg[3] = action_value;
	mbox_msg[4] = PCM_VALUE_MUX_SOURCE_MAIN;

	tcc_mbox_audio_send_message(
		mbox_audio_dev,
		&mbox_header,
		mbox_msg,
		NULL);
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO_R5
	if (asrc_m2m_pcm->mbox_audio_dev_r5 != NULL) {
		mbox_header.usage = MBOX_AUDIO_USAGE_SET_R5;
		tcc_mbox_audio_send_message(
				asrc_m2m_pcm->mbox_audio_dev_r5,
				&mbox_header,
				mbox_msg,
				NULL);
	}
#endif//CONFIG_TCC_MULTI_MAILBOX_AUDIO_R5
	tpcm_dbg("[%s] mbox message : device = 0x%02x, action = 0x%01x\n",
		__func__,
	     mbox_msg[1],
		 mbox_msg[2]);

	return 0;
}

static int tcc_asrc_m2m_pcm_set_hw_params_to_mbox(
	struct tcc_asrc_m2m_pcm
	*asrc_m2m_pcm,
	int stream)
{
	struct mbox_audio_data_header_t mbox_header;

	const struct snd_pcm *pcm;
	unsigned int device;
	dma_addr_t phy_address;

	unsigned int mbox_msg[MBOX_MSG_SIZE_FOR_PARAMS] = { 0 };

	if (asrc_m2m_pcm == NULL) {
		tpcm_warn("[%s] asrc_m2m_pcm is NULL\n",
		       __func__);
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
		tpcm_warn("[%d][%s] fail to get phy addr\n",
		       asrc_m2m_pcm->dev_id, __func__);
		return -EFAULT;
	}
	//tpcm_dbg("[%s] phy_address = 0x%08x\n",
	//	__func__, phy_address);
	mbox_header.usage = MBOX_AUDIO_USAGE_SET;
	mbox_header.cmd_type = MBOX_AUDIO_CMD_TYPE_PCM;
	mbox_header.msg_size = MBOX_MSG_SIZE_FOR_PARAMS;
	mbox_header.tx_instance = 0;

	mbox_msg[0] = PCM_PARAMS;
	mbox_msg[1] = device;
	mbox_msg[2] =
	    (stream == SNDRV_PCM_STREAM_PLAYBACK) ? PCM_VALUE_DIRECTON_TX :
	    PCM_VALUE_DIRECTON_RX;
	mbox_msg[3] = (unsigned int)phy_address;
	mbox_msg[4] =
		(stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		asrc_m2m_pcm->playback->middle->buffer_bytes :
		asrc_m2m_pcm->capture->middle->buffer_bytes;

	tcc_mbox_audio_send_message(
		asrc_m2m_pcm->mbox_audio_dev,
		&mbox_header,
		mbox_msg,
		NULL);
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO_R5
	if (asrc_m2m_pcm->mbox_audio_dev_r5 != NULL) {
		mbox_header.usage = MBOX_AUDIO_USAGE_SET_R5;
		tcc_mbox_audio_send_message(
			asrc_m2m_pcm->mbox_audio_dev_r5,
			&mbox_header,
			mbox_msg,
			NULL);
	}
#endif//CONFIG_TCC_MULTI_MAILBOX_AUDIO_R5

	tpcm_dbg("[%s] mbox message : device = 0x%02x,",
		__func__,
		mbox_msg[1]);
	tpcm_info("tx_rx = 0x%01x, buffer address = 0x%x, buffer size = %u\n",
	    mbox_msg[2],
		mbox_msg[3],
		mbox_msg[4]);

	return 0;
}

static void playback_for_mbox_callback(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	struct snd_pcm_substream *substream = NULL;
	//	 = asrc_m2m_pcm->asrc_substream;
	struct asrc_m2m_pcm_stream *playback;
	unsigned int mid_buffer_bytes = 0, mix_pos = 0;
	unsigned int head_pos = 0, cur_pos = 0;
	bool period_elapsed_en = FALSE, footprint_elapsed_en = FALSE;
	ssize_t mix_byte = 0, mix_temp = 0;
	int len = 0;

	if (asrc_m2m_pcm == NULL) {
		tpcm_err("[%s] asrc_m2m_pcm is NULL\n", __func__);
		return;
	}

	playback = asrc_m2m_pcm->playback;

	if (playback->asrc_substream != NULL)
		substream = playback->asrc_substream;
	else {
		tpcm_warn("[%d] asrc_m2m_pcm->asrc_substream is null!!\n",
		     asrc_m2m_pcm->dev_id);
		return;
	}

	mid_buffer_bytes = playback->middle->buffer_bytes;
	mix_pos = playback->middle->pre_pos;

	if (playback->middle->cur_pos_from_ipc !=
	    playback->middle->pre_pos_from_ipc) {
		if (playback->middle->pre_pos_from_ipc >
		    playback->middle->cur_pos_from_ipc) {
			mix_byte =
				mid_buffer_bytes -
				playback->middle->pre_pos_from_ipc;
			mix_byte += playback->middle->cur_pos_from_ipc;
		} else {
			mix_byte =
			    playback->middle->cur_pos_from_ipc -
			    playback->middle->pre_pos_from_ipc;
		}
		//This line already be in the main function of mbox callback.
		//asrc_m2m_pcm->playback->middle->pre_pos_from_ipc =
		//asrc_m2m_pcm->playback->middle->cur_pos_from_ipc;
	} else {
		mix_byte = 0;
	}

	mix_pos += mix_byte;
	if (mix_pos >= mid_buffer_bytes)
		mix_pos = mix_pos - mid_buffer_bytes;


/*	if (mix_byte > 1024) {
 *		tpcm_dbg("[%d]mix_byte=%d, mix_pos=%d, mid_buf_byte=%d\n",
 *		__LINE__,(unsigned int)mix_byte, mix_pos, mid_buffer_bytes);
 *		tpcm_dbg("[%s][%d]cur_pos=%d, pre_pos=%d\n", __func__, __LINE__,
 *		(uint32_t)asrc_m2m_pcm->playback->middle->cur_pos_from_ipc,
 *		(uint32_t)asrc_m2m_pcm->playback->middle->pre_pos_from_ipc);
 *	}
 */

	playback->middle->pre_pos = mix_pos;
	cur_pos = playback->middle->cur_pos;
	period_elapsed_en = FALSE;
	mix_temp = 0;
	while (1) {
		footprint_elapsed_en = FALSE;
		len =
		    tcc_asrc_footprint_get_length(asrc_m2m_pcm->asrc_footprint);
		if (len <= 0) {
			//tpcm_dbg("[%s][%d]len=%d\n", __func__, __LINE__, len);
			break;
		}
		head_pos = tcc_asrc_footprint_get_head_position(
			asrc_m2m_pcm->asrc_footprint);
		mix_byte = tcc_asrc_footprint_get_head_inputbyte(
			asrc_m2m_pcm->asrc_footprint);
/*
 * #ifdef FOOTPRINT_LINKED_LIST
 *		head_pos = asrc_m2m_pcm->asrc_footprint->head->print_pos;
 *		mix_byte = asrc_m2m_pcm->asrc_footprint->head->input_byte;
 * #else
 *		head_pos = asrc_m2m_pcm->asrc_footprint->print_pos[
 *			asrc_m2m_pcm->asrc_footprint->head];
 *		mix_byte = asrc_m2m_pcm->asrc_footprint->input_byte[
 *			asrc_m2m_pcm->asrc_footprint->head];
 * #endif
 */
		if (cur_pos > mix_pos) {
			if ((head_pos <= mix_pos) || (head_pos >= cur_pos))
				footprint_elapsed_en = TRUE;

		} else {
			if ((head_pos <= mix_pos) && (head_pos >= cur_pos))
				footprint_elapsed_en = TRUE;
		}

		if (footprint_elapsed_en == TRUE) {
			playback->Bbuffer_pos += mix_byte;
			playback->Bperiod_pos += mix_byte;
			mix_temp += mix_byte;
			if (playback->Bbuffer_pos >=
				playback->src->buffer_bytes) {
				playback->Bbuffer_pos =
				    playback->Bbuffer_pos -
				    playback->src->buffer_bytes;
			}

			if (playback->Bperiod_pos >=
			    playback->src->period_bytes) {
				playback->Bperiod_pos =
				    playback->Bperiod_pos -
				    playback->src->period_bytes;
				//period_elapsed_en = TRUE;
			}
			len = tcc_asrc_footprint_delete(
				asrc_m2m_pcm->asrc_footprint, FALSE);
			period_elapsed_en = TRUE;
		} else {
			//tpcm_dbg("[%s][%d]cur_pos=%d, pre_pos=%d, head_pos=%d,
			//len=%d, mix_byte=%d\n", __func__, __LINE__, cur_pos,
			//mix_pos, head_pos, len, (unsigned int)mix_byte);
			break;
		}
	}

	if (period_elapsed_en == TRUE) {
		snd_pcm_period_elapsed(substream);
		atomic_set(&playback->wakeup, 1);
		wake_up_interruptible(&(playback->kth_wq));
	} else {
		//This is for snd_pcm_drain
		if (substream->runtime->status->state ==
		    SNDRV_PCM_STATE_DRAINING) {
			while (len > 0) {
				mix_byte =
					tcc_asrc_footprint_get_head_inputbyte(
						asrc_m2m_pcm->asrc_footprint);
/*
 *#ifdef FOOTPRINT_LINKED_LIST
 *				mix_byte =
 *				 asrc_m2m_pcm->asrc_footprint->head->input_byte;
 *#else
 *				mix_byte =
 *				 asrc_m2m_pcm->asrc_footprint->input_byte[
 *				 asrc_m2m_pcm->asrc_footprint->head];
 *#endif
 */
				playback->Bbuffer_pos += mix_byte;
				playback->Bperiod_pos += mix_byte;
				if (playback->Bbuffer_pos >=
				    playback->src->buffer_bytes) {
					playback->Bbuffer_pos =
						playback->Bbuffer_pos -
						playback->src->buffer_bytes;
				}

				if (playback->Bperiod_pos >=
				    playback->src->period_bytes) {
					playback->Bperiod_pos =
						playback->Bperiod_pos -
						playback->src->period_bytes;
					if (substream)
						snd_pcm_period_elapsed(
						substream);
					else {
						tpcm_err(
							"[%d]asrc_m2m_pcm->",
							asrc_m2m_pcm->dev_id);
						pr_err("asrc_substream");
						pr_err("set to null during");
						pr_err("drain!!\n");
						break;
					}
				}
				len = tcc_asrc_footprint_delete(
					asrc_m2m_pcm->asrc_footprint, FALSE);
			}
		}
	}
}

static void capture_for_mbox_callback(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	unsigned int read_pos = 0;

	read_pos = asrc_m2m_pcm->capture->middle->cur_pos_from_ipc;

#if	(CHECK_OVERRUN == 1)
	if ((asrc_m2m_pcm->capture->middle->cur_pos >
		asrc_m2m_pcm->capture->middle->pre_pos) &&
		(read_pos > asrc_m2m_pcm->capture->middle->pre_pos) &&
		(read_pos < asrc_m2m_pcm->capture->middle->cur_pos)) {
		tpcm_err("[%d] overrun? pre_pos=%u, read_pos=%u,",
			asrc_m2m_pcm->dev_id,
			asrc_m2m_pcm->capture->middle->pre_pos,
			read_pos);
		pr_err("cur_pos=%u\n",
			asrc_m2m_pcm->capture->middle->cur_pos);

	} else if ((asrc_m2m_pcm->capture->middle->cur_pos <
		asrc_m2m_pcm->capture->middle->pre_pos) &&
		((read_pos > asrc_m2m_pcm->capture->middle->pre_pos) ||
		(read_pos < asrc_m2m_pcm->capture->middle->cur_pos))) {
		tpcm_err("[%d] overrun? pre_pos=%u, read_pos=%u,",
			asrc_m2m_pcm->dev_id,
			asrc_m2m_pcm->capture->middle->pre_pos,
			read_pos);
		pr_err("cur_pos=%u\n",
		asrc_m2m_pcm->capture->middle->cur_pos);
	}
#endif

	asrc_m2m_pcm->capture->middle->cur_pos = read_pos;

	atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
	wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
}

static void tcc_asrc_m2m_pcm_mbox_callback(
	void *data,
	unsigned int *msg,
	unsigned short msg_size,
	unsigned short cmd_type)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	//unsigned int cmd = msg[0];    //for debug
	unsigned int pos = msg[2];
	char Ctemp = 0;

	if (asrc_m2m_pcm == NULL) {
		tpcm_err("[%s] Cannot get asrc_m2m_pcm driver data\n",
		     __func__);
		return;
	}

	if ((cmd_type != asrc_m2m_pcm->playback->mbox_cmd_type)
	    && (cmd_type != asrc_m2m_pcm->capture->mbox_cmd_type)) {
		tpcm_err("[%s] Invalid cmd type between msg[%u], playback[%u] and capture[%u]\n",
		     __func__,
			 cmd_type,
			 asrc_m2m_pcm->playback->mbox_cmd_type,
		     asrc_m2m_pcm->capture->mbox_cmd_type);
		return;
	}

	if (msg_size == AUDIO_MBOX_CONCURRENT_PCM_POSITION_MESSAGE_SIZE)
		pos = msg[cmd_type];

	//For playback
	if (cmd_type == asrc_m2m_pcm->playback->mbox_cmd_type) {

		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->playback, 0,
			IS_FLAG_GET);
		if (((Ctemp & IS_A7S_STARTED) != 0) &&
			(asrc_m2m_pcm->playback->middle->cur_pos_from_ipc !=
			pos)) {
			asrc_m2m_pcm->playback->middle->pre_pos_from_ipc =
			    asrc_m2m_pcm->playback->middle->cur_pos_from_ipc;
			asrc_m2m_pcm->playback->middle->cur_pos_from_ipc = pos;
			playback_for_mbox_callback(asrc_m2m_pcm);
		}
	} else {		//For capture

		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture, 0,
			IS_FLAG_GET);
		if (((Ctemp & IS_A7S_STARTED) != 0) &&
			(asrc_m2m_pcm->capture->middle->cur_pos_from_ipc !=
		    pos)) {
			asrc_m2m_pcm->capture->middle->pre_pos_from_ipc =
			    asrc_m2m_pcm->capture->middle->cur_pos_from_ipc;
			asrc_m2m_pcm->capture->middle->cur_pos_from_ipc = pos;
			capture_for_mbox_callback(asrc_m2m_pcm);
		}
	}
	//tpcm_dbg("[%s] callback get! cmd = %d, device = %d,
	//position = 0x%08x, cmd_type = %d, msg_size = %d\n",
	//__func__,cmd, device, pos, cmd_type, msg_size);
}
#endif

static int tcc_asrc_m2m_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	struct asrc_m2m_pcm_stream *playback;
	struct task_struct *pkth_id = NULL;

	tpcm_dbg("[%s] id=%d\n", __func__, substream->pcm->device);

	if (asrc_m2m_pcm == NULL) {
		tpcm_err("[%s] asrc_m2m_pcm is NULL\n", __func__);
		return -EFAULT;
	}
	playback = asrc_m2m_pcm->playback;

	snd_pcm_hw_constraint_step(
		substream->runtime,
		0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
		32);
	snd_pcm_hw_constraint_step(
		substream->runtime,
		0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
		32);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (playback->pair_id == ASRC_7_1CH_DEV_NUM) {
			snd_soc_set_runtime_hwparams(
				substream,
				&tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_7_1CH]);
		} else {
			snd_soc_set_runtime_hwparams(
				substream,
				&tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_STEREO]);
		}
		playback->asrc_substream = substream;
		tcc_asrc_m2m_pcm_stream_reset(playback, FALSE);

		tcc_is_flag_update(playback, 0, IS_FLAG_RESET);

		if (playback->first_open == FALSE) {
			playback->first_open = TRUE;
			pkth_id =
			    (struct task_struct *)
			    kthread_run(
					tcc_appl_ptr_check_thread_for_play,
					(void *)(asrc_m2m_pcm),
					"tcc_appl_ptr_check_thread_for_play");
			if (pkth_id != NULL)
				playback->kth_id = pkth_id;
		}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO

		if ((playback->mbox_cmd_type >=
		     MBOX_AUDIO_CMD_TYPE_POSITION_MIN)
		    && (playback->mbox_cmd_type <=
			MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			asrc_m2m_pcm->mbox_audio_dev =
			    tcc_mbox_audio_register_set_kernel_callback(
					(void *)asrc_m2m_pcm,
					tcc_asrc_m2m_pcm_mbox_callback,
					playback->mbox_cmd_type);
		}
#endif

	} else {
		if (asrc_m2m_pcm->capture->pair_id == ASRC_7_1CH_DEV_NUM) {
			snd_soc_set_runtime_hwparams(substream,
				 &tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_7_1CH]);
		} else {
			snd_soc_set_runtime_hwparams(substream,
				&tcc_asrc_m2m_pcm_hw[TCC_ASRC_M2M_STEREO]);
		}
		asrc_m2m_pcm->capture->asrc_substream = substream;
		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->capture, FALSE);

		tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_RESET);

		if (asrc_m2m_pcm->capture->first_open == FALSE) {
			asrc_m2m_pcm->capture->first_open = TRUE;
			pkth_id = (struct task_struct *)kthread_run(
				tcc_ptr_update_thread_for_capture,
				(void *)asrc_m2m_pcm,
				"tcc_ptr_update_thread_for_capture");
			if (pkth_id != NULL)
				asrc_m2m_pcm->capture->kth_id = pkth_id;
		}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		if ((asrc_m2m_pcm->capture->mbox_cmd_type >=
		     MBOX_AUDIO_CMD_TYPE_POSITION_MIN)
		    && (asrc_m2m_pcm->capture->mbox_cmd_type <=
			MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			asrc_m2m_pcm->mbox_audio_dev =
			    tcc_mbox_audio_register_set_kernel_callback(
				(void *)asrc_m2m_pcm,
				tcc_asrc_m2m_pcm_mbox_callback,
				asrc_m2m_pcm->capture->mbox_cmd_type);
		}
#endif
	}

	tpcm_dbg("[%d][%s][%d] end\n", asrc_m2m_pcm->dev_id, __func__,
			 __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd =
	    (struct snd_soc_pcm_runtime *)substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	int ret = 0;

	tpcm_dbg("[%d][%s] start\n", asrc_m2m_pcm->dev_id, __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		if ((asrc_m2m_pcm->mbox_audio_dev != NULL)
		    && (asrc_m2m_pcm->playback->mbox_cmd_type >=
			MBOX_AUDIO_CMD_TYPE_POSITION_MIN)
		    && (asrc_m2m_pcm->playback->mbox_cmd_type <=
			MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			tcc_mbox_audio_unregister_set_kernel_callback
			    (asrc_m2m_pcm->mbox_audio_dev,
			     asrc_m2m_pcm->playback->mbox_cmd_type);
		}
#endif
		atomic_set(&asrc_m2m_pcm->playback->wakeup, 0);

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->playback, FALSE);
		if (asrc_m2m_pcm->asrc_footprint->list_len > 0) {
			ret = tcc_asrc_footprint_delete(
				asrc_m2m_pcm->asrc_footprint, TRUE);
		}
		tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);

		if (asrc_m2m_pcm->playback->asrc_substream != NULL)
			asrc_m2m_pcm->playback->asrc_substream = NULL;

	} else {
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		if ((asrc_m2m_pcm->mbox_audio_dev != NULL)
		    && (asrc_m2m_pcm->capture->mbox_cmd_type >=
			MBOX_AUDIO_CMD_TYPE_POSITION_MIN)
		    && (asrc_m2m_pcm->capture->mbox_cmd_type <=
			MBOX_AUDIO_CMD_TYPE_POSITION_MAX)) {
			tcc_mbox_audio_unregister_set_kernel_callback
			    (asrc_m2m_pcm->mbox_audio_dev,
			     asrc_m2m_pcm->capture->mbox_cmd_type);
		}
#endif
		atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->capture, FALSE);
		if (asrc_m2m_pcm->capture->asrc_substream != NULL)
			asrc_m2m_pcm->capture->asrc_substream = NULL;
	}

/*
 *	if (asrc_m2m_pcm->kth_id != NULL) {
 *		tpcm_dbg("[%s][%d]\n", __func__, __LINE__);
 *		kthread_stop(asrc_m2m_pcm->kth_id);
 *		asrc_m2m_pcm->kth_id = NULL;
 *	}
 */
	tpcm_dbg("[%s][%d] end\n", __func__, __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_mmap(
	struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	tpcm_dbg("[%s]\n", __func__);

	return dma_mmap_writecombine(
		substream->pcm->card->dev,
		vma,
		runtime->dma_area,
		runtime->dma_addr,
		runtime->dma_bytes);
}

static int tcc_asrc_m2m_pcm_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	struct tcc_asrc_param_t *pasrc_param;
	// = asrc_m2m_pcm->asrc_m2m_param;

	int channels = params_channels(params);
	size_t rate = params_rate(params);
	size_t period_bytes = params_period_bytes(params);
	size_t buffer_bytes = params_buffer_bytes(params);
	snd_pcm_format_t format = params_format(params);
	bool multi_ch = FALSE;
//      bool mono_mode = (channels == 1) ? TRUE : FALSE;
	enum tcc_asrc_drv_bitwidth_t asrc_bitwidth;
	enum tcc_asrc_drv_ch_t asrc_channels;
	uint64_t ratio_shift22;

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	tpcm_dbg("[%d] format : 0x%08x\n",
		asrc_m2m_pcm->dev_id,
		format);
	tpcm_dbg("[%d] channels : %d\n",
		asrc_m2m_pcm->dev_id,
		channels);
	tpcm_dbg("[%d] period_bytes : %u\n",
		asrc_m2m_pcm->dev_id,
		period_bytes);
	tpcm_dbg("[%d] buffer_bytes : %u\n",
		asrc_m2m_pcm->dev_id,
		buffer_bytes);

	memset(substream->dma_buffer.area, 0, buffer_bytes);

	asrc_bitwidth =
	    (format ==
	     SNDRV_PCM_FORMAT_S24_LE) ? TCC_ASRC_24BIT : TCC_ASRC_16BIT;

	if (substream->pcm->device == ASRC_7_1CH_DEV_NUM) {
		multi_ch = (channels > 2) ? TRUE : FALSE;
		asrc_channels =
			(channels < 3) ? TCC_ASRC_NUM_OF_CH_2 :
		    (channels < 5) ? TCC_ASRC_NUM_OF_CH_4 :
		    (channels < 7) ? TCC_ASRC_NUM_OF_CH_6 :
			TCC_ASRC_NUM_OF_CH_8;
	} else {
		multi_ch = FALSE;
		asrc_channels = TCC_ASRC_NUM_OF_CH_2;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pasrc_param = asrc_m2m_pcm->playback->asrc_m2m_param;

		ratio_shift22 = ((uint64_t) 0x400000 * ASRC_DST_RATE);
		do_div(ratio_shift22, rate);
	} else {
		pasrc_param = asrc_m2m_pcm->capture->asrc_m2m_param;

		ratio_shift22 = ((uint64_t) 0x400000 * rate);
		do_div(ratio_shift22, ASRC_SRC_RATE);
	}
	memset(pasrc_param, 0, sizeof(struct tcc_asrc_param_t));

	pasrc_param->u.cfg.src_bitwidth = asrc_bitwidth;
	pasrc_param->u.cfg.dst_bitwidth = asrc_bitwidth;
	pasrc_param->u.cfg.channels = asrc_channels;
	pasrc_param->u.cfg.ratio_shift22 = (uint32_t) ratio_shift22;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		asrc_m2m_pcm->playback->src->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->playback->src->period_bytes = period_bytes;
		asrc_m2m_pcm->playback->src->rate = rate;
		asrc_m2m_pcm->playback->interval =
		    TCC_APP_PTR_CHECK_INTERVAL_TX;

		asrc_m2m_pcm->playback->dst->rate = ASRC_DST_RATE;
		asrc_m2m_pcm->playback->dst->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->playback->dst->period_bytes =
		//	PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->playback->dst->period_bytes = MIN_PERIOD_BYTES;

		asrc_m2m_pcm->playback->middle->buffer_bytes =
		    asrc_m2m_pcm->playback->dst->buffer_bytes *
		    MID_BUFFER_CONST;
	} else {
		asrc_m2m_pcm->capture->dst->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->capture->dst->period_bytes = period_bytes;
		asrc_m2m_pcm->capture->dst->rate = rate;
		asrc_m2m_pcm->capture->interval = TCC_APP_PTR_CHECK_INTERVAL_RX;

		asrc_m2m_pcm->capture->src->rate = ASRC_SRC_RATE;
		asrc_m2m_pcm->capture->src->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->capture->src->period_bytes =
		//	PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->capture->src->period_bytes = MIN_PERIOD_BYTES;

		asrc_m2m_pcm->capture->middle->buffer_bytes =
		    asrc_m2m_pcm->capture->src->buffer_bytes * MID_BUFFER_CONST;
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_asrc_m2m_pcm_set_hw_params_to_mbox(asrc_m2m_pcm, substream->stream);
#endif
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	char Ctemp = 0;
	int ret = -1;

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
	} else {
		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
	}

	//tpcm_dbg("[%s][%d] Ctemp=0x%08x\n", __func__, __LINE__, Ctemp);
	if ((Ctemp & IS_ASRC_STARTED) != 0)
		tcc_asrc_m2m_pcm_asrc_stop(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tcc_is_flag_update(
			asrc_m2m_pcm->playback,
			(IS_TRIG_STARTED
			|IS_ASRC_STARTED
			|IS_ASRC_RUNNING),
			IS_FLAG_RELEASE);

		atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
		wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));
	} else {
		tcc_is_flag_update(
			asrc_m2m_pcm->capture,
			(IS_TRIG_STARTED
			|IS_ASRC_STARTED
			|IS_ASRC_RUNNING),
			IS_FLAG_RELEASE);

		atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
		wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
	}

	if (substream) {
		snd_pcm_set_runtime_buffer(substream, NULL);
		ret = 0;
	} else
		tpcm_warn("[%d] %s-error\n",
		       asrc_m2m_pcm->dev_id, __func__);

	return ret;
}

static int tcc_asrc_m2m_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	char Ctemp = 0;
	int ret = 0;

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
	} else {
		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
	}

	if ((Ctemp & IS_ASRC_STARTED) != 0) {
		tpcm_dbg("[%d][%s] ASRC stop yet.\n",
			asrc_m2m_pcm->dev_id,
			__func__);
		tcc_asrc_m2m_pcm_asrc_stop(substream);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->playback, TRUE);

		if (asrc_m2m_pcm->asrc_footprint->list_len > 0) {
			// delete all footprint
			ret = tcc_asrc_footprint_delete(
				asrc_m2m_pcm->asrc_footprint, TRUE);
		}
		tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);

		if (asrc_m2m_pcm->playback->middle->dma_buf->area != NULL)
			memset(
				asrc_m2m_pcm->playback->middle->dma_buf->area,
				0,
				(sizeof(unsigned char)
				*MAX_BUFFER_BYTES
				*MID_BUFFER_CONST));
#ifdef TAIL_DEBUG
		play_tail = 0;
#endif
	} else {

		tcc_asrc_m2m_pcm_stream_reset(asrc_m2m_pcm->capture, TRUE);

		if (asrc_m2m_pcm->capture->middle->dma_buf->area != NULL)
			memset(
				asrc_m2m_pcm->capture->middle->dma_buf->area,
				0,
			    (sizeof(unsigned char)
				*MAX_BUFFER_BYTES
				*MID_BUFFER_CONST));
#ifdef TAIL_DEBUG
		cap_tail = 0;
#endif
	}

	memset(runtime->dma_area, 0x00, runtime->dma_bytes);

	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_trigger(
	struct snd_pcm_substream *substream,
	int cmd)
{
//      struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	struct asrc_m2m_pcm_stream *playback = NULL;
	struct tcc_asrc_param_t *pasrc_param = NULL;
		//asrc_m2m_pcm->asrc_m2m_param;
	struct tcc_asrc_t *pasrc = asrc_m2m_pcm->asrc;
//      unsigned int temp=0;
	char Ctemp = 0;
	int ret = -1;

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);

	if (asrc_m2m_pcm == NULL) {
		tpcm_err("[%s][%d] error!\n", __func__, __LINE__);
		return -EFAULT;

	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			pasrc_param = asrc_m2m_pcm->playback->asrc_m2m_param;
		else
			pasrc_param = asrc_m2m_pcm->capture->asrc_m2m_param;

		if (pasrc_param == NULL) {
			tpcm_err("[%d][%s][%d] error!\n",
				asrc_m2m_pcm->dev_id,
				__func__,
				__LINE__);
			return -EFAULT;
		}
	}
	playback = asrc_m2m_pcm->playback;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		Ctemp = tcc_is_flag_update(playback, 0, IS_FLAG_GET);
	else
		Ctemp = tcc_is_flag_update(
			asrc_m2m_pcm->capture,
			0,
			IS_FLAG_GET);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tpcm_dbg("[%d] ASRC_TRIGGER_START, PLAY\n",
					 asrc_m2m_pcm->dev_id);
			if (!(Ctemp & IS_ASRC_STARTED)) {
				if ((playback->pair_id == 99)
					&& (playback->src->rate !=
					playback->dst->rate)) {
					tpcm_err("[%d][%s][%d] error!!\n",
					     asrc_m2m_pcm->dev_id,
						 __func__,
					     __LINE__);
					ret = -EINVAL;
					goto error_trigger;

				} else if (
					(playback->pair_id != 99)
					&& (playback->src->rate !=
					    playback->dst->rate)) {
					ret = tcc_asrc_m2m_start(
						pasrc, playback->pair_id,// Pair
						pasrc_param->u.cfg.src_bitwidth,
						pasrc_param->u.cfg.dst_bitwidth,
						pasrc_param->u.cfg.channels,
						pasrc_param->u.cfg.ratio_shift22
						/* multiple of 2*/);
				} else {
					ret = 0;
				}

				Ctemp |= IS_ASRC_STARTED;
			} else {
				tpcm_dbg
				    ("[%d] ASRC_TRIGGER_START already, PLAY\n",
				     asrc_m2m_pcm->dev_id);
			}
/*
 *	//ref. code
 *				temp = runtime->period_size;
 *				temp = (temp*1000)/(asrc_m2m_pcm->src->rate);
 *
 *				asrc_m2m_pcm->interval = temp/4;
 *				if (asrc_m2m_pcm->interval <= 0)
 *					asrc_m2m_pcm->interval = 1;
 *				tpcm_dbg("interval: %d, period: %dB,
 *				buffer: %dB\n",
 *					(int)asrc_m2m_pcm->interval,
 *					(int)asrc_m2m_pcm->src->period_bytes,
 *					(int)asrc_m2m_pcm->src->buffer_bytes);
 */

/*
 * After 1st ASRC operation, AMD send cmd of start to A7S.
 */
#if 0				//def CONFIG_TCC_MULTI_MAILBOX_AUDIO
			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(
				asrc_m2m_pcm->mbox_audio_dev,
				substream,
				cmd);
			if (ret < 0) {
				tpcm_dbg("[%d][%s][%d] ERROR!! ret=%d\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     ret);
			}
#endif
			tpcm_dbg("[%d] ASRC_TRIGGER_START end ret=%d, PLAY\n",
			     asrc_m2m_pcm->dev_id,
				 ret);
		} else {
			tpcm_dbg("[%d] ASRC_TRIGGER_START, CAPTURE\n",
					 asrc_m2m_pcm->dev_id);
			if (!(Ctemp & IS_ASRC_STARTED)) {
				if ((asrc_m2m_pcm->capture->pair_id == 99)
				    && (asrc_m2m_pcm->capture->src->rate !=
					asrc_m2m_pcm->capture->dst->rate)) {
					tpcm_err("[%d][%s][%d] error!!\n",
					     asrc_m2m_pcm->dev_id,
						 __func__,
					     __LINE__);
					ret = -EINVAL;
					goto error_trigger;

				} else if (
					(asrc_m2m_pcm->capture->pair_id != 99)
					&& (asrc_m2m_pcm->capture->src->rate !=
					    asrc_m2m_pcm->capture->dst->rate)) {
					ret = tcc_asrc_m2m_start(
						pasrc,
						asrc_m2m_pcm->capture->pair_id,
						pasrc_param->u.cfg.src_bitwidth,
						pasrc_param->u.cfg.dst_bitwidth,
						pasrc_param->u.cfg.channels,
						pasrc_param->u.cfg.ratio_shift22
						/*multiple of 2*/);
				} else
					ret = 0;

				Ctemp |= IS_ASRC_STARTED;

			} else {
				tpcm_dbg("[%d] ASRC_TRIGGER_START already, CAPTURE\n",
				     asrc_m2m_pcm->dev_id);
				return -EINVAL;
			}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(
			    asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
			if (ret < 0)
				tpcm_err("[%d][%s][%d] ERROR!! ret=%d\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     ret);
			else
				Ctemp |= IS_A7S_STARTED;

#endif
			tpcm_dbg("[%d] ASRC_TRIGGER_START end ret=%d, CAPTURE\n",
			     asrc_m2m_pcm->dev_id,
				 ret);
		}

		Ctemp |= IS_TRIG_STARTED;

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tcc_is_flag_update(playback, Ctemp, IS_FLAG_SET);

			atomic_set(&playback->wakeup, 1);
			wake_up_interruptible(&(playback->kth_wq));
		} else {
			tcc_is_flag_update(asrc_m2m_pcm->capture, Ctemp,
				IS_FLAG_SET);

			atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
			wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:

		Ctemp &= ~IS_TRIG_STARTED;

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
		ret = tcc_asrc_m2m_pcm_set_action_to_mbox(
			asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
		if (ret < 0) {
			tpcm_err("[%d][%s][%d] ERROR!! ret=%d\n",
			     asrc_m2m_pcm->dev_id,
				 __func__,
				 __LINE__,
				 ret);
		} else {
			Ctemp &= ~IS_A7S_STARTED;
		}
#endif
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tpcm_dbg("[%d] ASRC_TRIGGER_STOP, PLAY\n",
				asrc_m2m_pcm->dev_id);

			tcc_is_flag_update(playback, Ctemp, IS_FLAG_APPLY);

			atomic_set(&playback->wakeup, 1);
			wake_up_interruptible(&(playback->kth_wq));
		} else {
			tpcm_dbg("[%d] ASRC_TRIGGER_STOP, CAPTURE\n",
				asrc_m2m_pcm->dev_id);

			tcc_is_flag_update(asrc_m2m_pcm->capture, Ctemp,
				IS_FLAG_APPLY);

			atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
			wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
		}
		break;
	default:
		return -EINVAL;
	}
error_trigger:
	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);

	return ret;
}

static snd_pcm_uframes_t tcc_asrc_m2m_pcm_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	snd_pcm_uframes_t ret = 0;

	if (asrc_m2m_pcm == NULL) {
		tpcm_dbg("[%s][%d]\n", __func__, __LINE__);
		return bytes_to_frames(runtime, 0);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = bytes_to_frames(
			runtime,
			asrc_m2m_pcm->playback->Bbuffer_pos);
	else
		ret = asrc_m2m_pcm->capture->app->pre_pos;

	//tpcm_dbg("[%d] ret=%d\n", asrc_m2m_pcm->dev_id, (int)ret);
	return ret;
}

static struct snd_pcm_ops tcc_asrc_m2m_pcm_ops = {
	.open = tcc_asrc_m2m_pcm_open,
	.close = tcc_asrc_m2m_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = tcc_asrc_m2m_pcm_hw_params,
	.hw_free = tcc_asrc_m2m_pcm_hw_free,
	.prepare = tcc_asrc_m2m_pcm_prepare,
	.trigger = tcc_asrc_m2m_pcm_trigger,
	.pointer = tcc_asrc_m2m_pcm_pointer,
	.mmap = tcc_asrc_m2m_pcm_mmap,
};

static void tcc_asrc_m2m_pcm_stream_reset(
	struct asrc_m2m_pcm_stream *strm,
	bool prepare)
{
/*
 *	unsigned long flags;
 *	spin_lock_irqsave(&strm->is_locked, flags);
 *	strm->is_flag = 0;
 *	spin_unlock_irqrestore(&strm->is_locked, flags);
 *
 *
 *	//atomic_set(&strm->wakeup, 0);
 */
	//Buffer info reset.
	strm->app->pre_pos = 0;
	strm->app->pre_ptr = 0;
	if (!prepare) {
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
	if ((prepare) && (strm->middle->dma_buf->area != NULL))
		memset(
			strm->middle->dma_buf->area,
			0,
			(sizeof(unsigned char)
			*MAX_BUFFER_BYTES
			*MID_BUFFER_CONST));

	strm->Bbuffer_pos = 0;
	strm->Bperiod_pos = 0;
	strm->Btail = 0;
}

static int tcc_asrc_m2m_pcm_asrc_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	char Ctemp = 0;
	bool recheck = FALSE;
	int ret = -1, timeout = 0;

	while (timeout < 300) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			Ctemp =
				tcc_is_flag_update(
					asrc_m2m_pcm->playback,
					0,
					IS_FLAG_GET);
		else
			Ctemp =
				tcc_is_flag_update(
					asrc_m2m_pcm->capture,
					0,
					IS_FLAG_GET);

		if (((Ctemp & IS_TRIG_STARTED) == 0)
		    && ((Ctemp & IS_ASRC_RUNNING) == 0)) {

			if (recheck)
				break;

			recheck = TRUE;
		} else {
			recheck = FALSE;
		}

		timeout++;
		udelay(1000);
	}
/*	//for debug
 *	if (timeout > 2) {
 *		tpcm_err("[%d][%s][%d] Ctemp=0x%08x, timeout=%d\n",
 *		asrc_m2m_pcm->dev_id, __func__, __LINE__, Ctemp, timeout);
 *	}
 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->playback, 0, IS_FLAG_GET);
		tpcm_dbg("[%d] ASRC_STOP, PLAY\n",
				 asrc_m2m_pcm->dev_id);
		if ((asrc_m2m_pcm->playback->pair_id == 99)
		    && (asrc_m2m_pcm->playback->src->rate !=
			asrc_m2m_pcm->playback->dst->rate)) {
			tpcm_err("[%d][%s][%d] error!!\n",
			       asrc_m2m_pcm->dev_id,
				   __func__,
				   __LINE__);
			ret = -EINVAL;
			goto error_pcm_asrc_stop;

		} else if ((asrc_m2m_pcm->playback->pair_id != 99)
			   && (asrc_m2m_pcm->playback->src->rate !=
			       asrc_m2m_pcm->playback->dst->rate)) {

			ret =
			    tcc_asrc_m2m_stop(
					asrc_m2m_pcm->asrc,
					asrc_m2m_pcm->playback->pair_id);
		} else {
			ret = 0;
		}

		Ctemp &= ~IS_ASRC_STARTED;

		tcc_is_flag_update(
			asrc_m2m_pcm->playback,
			Ctemp,
			IS_FLAG_APPLY);

		//atomic_set(&asrc_m2m_pcm->playback->wakeup, 1);
		//wake_up_interruptible(&(asrc_m2m_pcm->playback->kth_wq));

		tpcm_dbg("[%d] ASRC_STOP ret=%d, PLAY\n",
			asrc_m2m_pcm->dev_id,
			ret);
	} else {
		tpcm_dbg("[%d] ASRC_STOP, CAPTURE\n",
			asrc_m2m_pcm->dev_id);
		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);

		if ((asrc_m2m_pcm->capture->pair_id == 99)
		    && (asrc_m2m_pcm->capture->src->rate !=
			asrc_m2m_pcm->capture->dst->rate)) {
			tpcm_err("[%d][%s][%d] error!!\n",
				asrc_m2m_pcm->dev_id,
				__func__,
				__LINE__);
			ret = -EINVAL;
			goto error_pcm_asrc_stop;

		} else if ((asrc_m2m_pcm->capture->pair_id != 99)
			   && (asrc_m2m_pcm->capture->src->rate !=
			       asrc_m2m_pcm->capture->dst->rate)) {

			ret =
			    tcc_asrc_m2m_stop(
					asrc_m2m_pcm->asrc,
					asrc_m2m_pcm->capture->pair_id);
		} else {
			ret = 0;
		}

		Ctemp &= ~IS_ASRC_STARTED;

		tcc_is_flag_update(asrc_m2m_pcm->capture, Ctemp, IS_FLAG_APPLY);

		//atomic_set(&asrc_m2m_pcm->capture->wakeup, 1);
		//wake_up_interruptible(&(asrc_m2m_pcm->capture->kth_wq));
		tpcm_dbg("[%d] ASRC_STOP ret=%d, CAPTURE\n",
			asrc_m2m_pcm->dev_id,
			ret);
	}
error_pcm_asrc_stop:
	return ret;
}

static void tcc_pcm_app_position_update(
	unsigned int dev_id,
	struct asrc_m2m_pcm_stream *strm,
	struct snd_pcm_runtime *runtime,
	ssize_t byte_size)
{
	ssize_t Btemp = 0;
	snd_pcm_uframes_t Ftemp = 0;

	if ((!strm) || (!runtime)) {
		if (!runtime) {
			tpcm_err("[%d][%s][%d] runtime is wrong!\n",
			     dev_id,
				 __func__,
				 __LINE__);
		} else {
			tpcm_err("[%d][%s][%d] strm is wrong!\n",
			     dev_id,
				 __func__,
				 __LINE__);
		}
	} else {
		/*      //ref. code
		 *  Ftemp = bytes_to_frames(runtime, read_byte);
		 *  Btemp = frames_to_bytes(runtime, Ftemp);
		 *  if (Btemp != read_byte) {
		 *  tpcm_err("[%d][%d] read_byte=%d, Btemp=%d\n",
		 *	dev_id, __LINE__, read_byte, Btemp);
		 *  }
		 */
		//check ptr size for update
		Btemp = byte_size + strm->Btail;
		Ftemp = bytes_to_frames(runtime, Btemp);
		strm->Btail = Btemp - (frames_to_bytes(runtime, Ftemp));

		//update ptr info.
		strm->app->pre_pos += Ftemp;
		if (strm->app->pre_pos >= runtime->buffer_size) {
			strm->app->pre_pos =
			    strm->app->pre_pos - runtime->buffer_size;
		}

		strm->app->pre_ptr += Ftemp;
		if (strm->app->pre_ptr >= runtime->boundary) {
			strm->app->pre_ptr =
			    strm->app->pre_ptr - runtime->boundary;
		}
	}
}

#if 0
static void tcc_pcm_app_position_rewind(
	unsigned int dev_id,
	struct asrc_m2m_pcm_stream *strm,
	struct snd_pcm_runtime *runtime,
	snd_pcm_uframes_t rewind_frames)
{
	snd_pcm_uframes_t Ftemp = 0;

	if ((!strm) || (!runtime)) {
		if (!runtime) {
			tpcm_err("[%d][%s][%d] runtime is wrong!\n",
				dev_id,
				__func__,
				__LINE__);
		} else {
			tpcm_err("[%d][%s][%d] strm is wrong!\n",
				dev_id,
				__func__,
				__LINE__);
		}
	} else {
		//rewind ptr info.
		Ftemp = strm->app->pre_pos;
		if (Ftemp >= rewind_frames) {
			strm->app->pre_pos = Ftemp - rewind_frames;
		} else {
			strm->app->pre_pos =
				runtime->buffer_size - rewind_frames + Ftemp;
		}

		Ftemp = strm->app->pre_ptr;
		if (Ftemp >= rewind_frames) {
			strm->app->pre_ptr = Ftemp - rewind_frames;
		} else {
			strm->app->pre_ptr =
				runtime->boundary - rewind_frames + Ftemp;
		}
	}
}
#endif
static snd_pcm_uframes_t tcc_pcm_app_rewind_check(
	unsigned int dev_id,
	struct asrc_m2m_pcm_stream
	*strm,
	struct snd_pcm_runtime
	*runtime,
	snd_pcm_uframes_t cur_ptr)
{
	snd_pcm_uframes_t buffer_base = 0, buffer_boundary = 0, Ftemp = 0;
	snd_pcm_uframes_t pre_ptr = 0, pre_pos = 0;
	snd_pcm_uframes_t ret = 0;

	if ((!strm) || (!runtime)) {
		if (!runtime) {
			tpcm_err("[%d][%s][%d] runtime is wrong!\n",
				dev_id,
				__func__,
				__LINE__);
		} else {
			tpcm_err("[%d][%s][%d] strm is wrong!\n",
				dev_id,
				__func__,
				__LINE__);
		}
		//ret = -EINVAL;
	} else {
		pre_pos = strm->app->pre_pos;
		pre_ptr = strm->app->pre_ptr;
		if (runtime->buffer_size <= pre_ptr) {
			buffer_base = pre_ptr - runtime->buffer_size;
			Ftemp = pre_ptr + runtime->buffer_size;
			if (Ftemp >= runtime->boundary)
				buffer_boundary = Ftemp % runtime->boundary;
			else
				buffer_boundary = Ftemp;

			if ((buffer_base < buffer_boundary)
			    && ((cur_ptr < buffer_base)
				|| (cur_ptr > buffer_boundary))) {
				tpcm_dbg("[%d][%s][%d] cur_ptr:%lu is wrong!",
					dev_id, __func__, __LINE__, cur_ptr);
				tpcm_info("pre_ptr:%lu,pre_pos:%lu,",
					pre_ptr,
					pre_pos);
				tpcm_info("buffer_size:%lu, boundary:%lu\n",
					runtime->buffer_size,
					runtime->boundary);
				//ret = -EINVAL;
			} else if ((buffer_base > buffer_boundary)
				   && ((cur_ptr < buffer_base)
				       && (cur_ptr > buffer_boundary))) {
				tpcm_dbg("[%d][%s][%d] cur_ptr:%lu is wrong!",
					dev_id, __func__, __LINE__, cur_ptr);
				tpcm_info("pre_ptr:%lu, pre_pos:%lu,",
					pre_ptr,
					pre_pos);
				tpcm_info("buffer_size:%lu, boundary:%lu\n",
					runtime->buffer_size,
					runtime->boundary);
				//ret = -EINVAL;
			} else {
				if ((cur_ptr <= pre_ptr)
				    && (buffer_base <= cur_ptr)) {
					ret = pre_ptr - cur_ptr;
					//tpcm_err("[%d][%s][%d] cur_ptr[%lu]
					//pre_ptr[%lu], pre_pos[%lu],
					//buffer_size[%lu], boundary[%lu]\n",
					//dev_id, __func__, __LINE__, cur_ptr,
					//pre_ptr,
					//pre_pos, runtime->buffer_size,
					//runtime->boundary);
				} else {
					//There is no rewind.
					ret = 0;
					//tpcm_err("[%d][%s][%d] cur_ptr[%lu]
					//pre_ptr[%lu], pre_pos[%lu],
					//buffer_size[%lu], boundary[%lu]\n",
					//dev_id, __func__, __LINE__, cur_ptr,
					//pre_ptr,pre_pos, runtime->buffer_size,
					//runtime->boundary);
				}
			}
		} else {
			buffer_base =
			    runtime->boundary - runtime->buffer_size + pre_ptr;
			buffer_boundary = pre_ptr + runtime->buffer_size;
			if ((cur_ptr < buffer_base)
			    && (cur_ptr > buffer_boundary)) {
				tpcm_dbg("[%d][%s][%d] cur_ptr[%lu] is wrong!",
					dev_id, __func__, __LINE__, cur_ptr);
				tpcm_info("pre_ptr[%lu], pre_pos[%lu],",
					pre_ptr,
					pre_pos);
				tpcm_info("buffer_size[%lu], boundary[%lu]\n",
					runtime->buffer_size,
					runtime->boundary);
				//ret = -EINVAL;
			} else {
				if (cur_ptr <= pre_ptr) {
					ret = pre_ptr - cur_ptr;
					//tpcm_err("[%d][%s][%d] cur_ptr[%lu]
					//pre_ptr[%lu], pre_pos[%lu],
					//buffer_size[%lu], boundary[%lu]\n",
					//dev_id, __func__, __LINE__, cur_ptr,
					//pre_ptr, pre_pos,
					//runtime->buffer_size,
					//runtime->boundary);
				} else if ((cur_ptr > pre_ptr)
					   && (buffer_base <= cur_ptr)) {
					ret =
					    runtime->boundary - cur_ptr +
					    pre_ptr;
					//tpcm_err("[%d][%s][%d] cur_ptr[%lu]
					//pre_ptr[%lu], pre_pos[%lu],
					//buffer_size[%lu], boundary[%lu]\n",
					//dev_id, __func__, __LINE__, cur_ptr,
					//pre_ptr, pre_pos,
					//runtime->buffer_size,
					//runtime->boundary);
				} else {
					//There is no rewind.
					ret = 0;
					//tpcm_err("[%d][%s][%d] cur_ptr[%lu]
					//pre_ptr[%lu], pre_pos[%lu],
					//buffer_size[%lu], boundary[%lu]\n",
					//dev_id, __func__, __LINE__, cur_ptr,
					//pre_ptr, pre_pos,
					//runtime->buffer_size,
					//runtime->boundary);
				}
			}
		}
	}

	return ret;
}

static char tcc_is_flag_update(
	struct asrc_m2m_pcm_stream *strm,
	char is_flag,
	char mode)
{
	unsigned long flags;
	char ret = 0;
	/*
	 * mode
	 * #define IS_FLAG_GET     0x00
	 * #define IS_FLAG_RELEASE 0x01
	 * #define IS_FLAG_SET     0x02
	 * #define IS_FLAG_RESET   0x04
	 * #define IS_FLAG_APPLY   0x08
	 */

	spin_lock_irqsave(&strm->is_locked, flags);
	switch (mode) {
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
		tpcm_err("[%s][%d] ERROR!!\n", __func__, __LINE__);
		break;
	}
	ret = strm->is_flag;
	spin_unlock_irqrestore(&strm->is_locked, flags);

	return ret;
}

static int tcc_ptr_update_func_for_capture(
	struct snd_pcm_substream
	*psubstream,
	ssize_t readable_byte,
	ssize_t max_cpy_byte)
{
	struct snd_pcm_substream *substream = psubstream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc;
	//struct asrc_m2m_pcm_stream *strm =
	//	(struct asrc_m2m_pcm_stream *)asrc_m2m_pcm->capture;
	ssize_t dst_buffer_bytes = 0, dst_period_bytes = 0;
	ssize_t read_pos = 0, write_pos = 0, temp_pos = 0;
	ssize_t writeable_byte = 0, read_byte = 0, Wtemp = 0, Btemp = 0;
	unsigned char *pin_buf, *pout_buf, *ptemp_buf, Ctemp = 0;
	int ret = 0;

	ptemp_buf = asrc_m2m_pcm->capture->middle->ptemp_buf;
	pin_buf = asrc_m2m_pcm->capture->middle->dma_buf->area;
	pout_buf = substream->dma_buffer.area;

	dst_buffer_bytes = asrc_m2m_pcm->capture->dst->buffer_bytes;
	dst_period_bytes = asrc_m2m_pcm->capture->dst->period_bytes;
	while (1) {
		Ctemp = tcc_is_flag_update(asrc_m2m_pcm->capture,
			IS_ASRC_RUNNING, IS_FLAG_RELEASE);

		if ((readable_byte <= 0)
		    || ((Ctemp & IS_TRIG_STARTED) == 0)
		    || ((Ctemp & IS_ASRC_STARTED) == 0)) {

			tpcm_dbg
			    ("[%d] readable_byte: %zd, is_flag=0x%02x\n",
			     asrc_m2m_pcm->dev_id,
				 readable_byte,
			     (unsigned int)Ctemp);
			ret = 0;
			break;	//return 0;

		} else {
			/////ASRC Running start
			tcc_is_flag_update(
				asrc_m2m_pcm->capture,
				IS_ASRC_RUNNING,
				IS_FLAG_SET);
		}

		if (readable_byte > max_cpy_byte)
			read_byte = max_cpy_byte;
		else
			read_byte = readable_byte;

		write_pos =
		    frames_to_bytes(
				runtime,
				asrc_m2m_pcm->capture->app->pre_pos) +
		    asrc_m2m_pcm->capture->Btail;
		read_pos = asrc_m2m_pcm->capture->middle->pre_pos;

		if (asrc_m2m_pcm->capture->src->rate ==
		    asrc_m2m_pcm->capture->dst->rate) {

			if (write_pos + read_byte >= dst_buffer_bytes) {
				//1st part copy for out_buf
				Btemp = dst_buffer_bytes - write_pos;
				memcpy(
					pout_buf + write_pos,
					pin_buf + read_pos,
				    Btemp);
				read_pos = read_pos + Btemp;

				//2nd part copy for out_buf
				Btemp = read_byte - Btemp;
				memcpy(pout_buf, pin_buf + read_pos, Btemp);
			} else {
				memcpy(
					pout_buf + write_pos,
					pin_buf + read_pos,
				    read_byte);
			}

			tcc_pcm_app_position_update(
				asrc_m2m_pcm->dev_id,
				asrc_m2m_pcm->capture,
				runtime,
				read_byte);

			asrc_m2m_pcm->capture->Bperiod_pos += read_byte;
			if (asrc_m2m_pcm->capture->Bperiod_pos >=
			    dst_period_bytes) {
				asrc_m2m_pcm->capture->Bperiod_pos =
				    asrc_m2m_pcm->capture->Bperiod_pos %
				    dst_period_bytes;
				snd_pcm_period_elapsed(substream);
			}

		} else {
			if (asrc_m2m_pcm->capture->pair_id == 99) {
				tpcm_warn("[%d][%s][%d] ERROR!! asrc pair_id[%d] is wrong\n",
				     asrc_m2m_pcm->dev_id, __func__, __LINE__,
				     asrc_m2m_pcm->playback->pair_id);

				Ctemp =
				    tcc_is_flag_update(
						asrc_m2m_pcm->capture,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = -1;
				break;
			}

			writeable_byte =
				tcc_asrc_m2m_push_data(
					asrc,
					asrc_m2m_pcm->capture->pair_id,
					pin_buf + read_pos,
					read_byte);
			if (writeable_byte >
				TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST) {
				tpcm_err(
				"[%d][%s][%d]writeable_byte[0x%08zx]",
					asrc_m2m_pcm->dev_id,
					__func__,
					__LINE__,
					writeable_byte);
				pr_err("is bigger than max[0x%08x]\n",
				    TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST);
				Ctemp = tcc_is_flag_update(
					asrc_m2m_pcm->capture,
					IS_ASRC_RUNNING,
					IS_FLAG_RELEASE);
				ret = -1;
				break;
			} else if (writeable_byte < 0) {
				Ctemp =
				    tcc_is_flag_update(
					asrc_m2m_pcm->capture,
					0,
					IS_FLAG_GET);
				tpcm_err(
				"[%d][%s][%d] read_byte[%zd]",
					asrc_m2m_pcm->dev_id,
					__func__,
					__LINE__,
				    read_byte);
				pr_err("writeable_byte[%zd] Ctemp[0x%02x]\n",
					writeable_byte,
					Ctemp);

				Ctemp =
				    tcc_is_flag_update(
						asrc_m2m_pcm->capture,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = writeable_byte;
				break;
			}

			if (write_pos + writeable_byte >= dst_buffer_bytes) {
				Wtemp = tcc_asrc_m2m_pop_data(
					asrc,
					asrc_m2m_pcm->capture->pair_id,
					ptemp_buf,
					writeable_byte);
			} else {
				//copy for out_buf
				Wtemp = tcc_asrc_m2m_pop_data(
					asrc,
					asrc_m2m_pcm->capture->pair_id,
					pout_buf + write_pos,
					writeable_byte);
			}

			if (Wtemp < 0) {
				Ctemp =
					tcc_is_flag_update(
						asrc_m2m_pcm->capture,
						0,
						IS_FLAG_GET);

				tpcm_err("[%d][%s][%d] writeable_byte[%zd]",
					asrc_m2m_pcm->dev_id,
					__func__, __LINE__,
					writeable_byte);
				pr_err("Wtemp[%zd] Ctemp[0x%02x]!\n",
					Wtemp,
					Ctemp);

				Ctemp =
				    tcc_is_flag_update(
						asrc_m2m_pcm->capture,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = Wtemp;
				break;
			}
/*
 *			spin_lock_irqsave(
 *				&asrc_m2m_pcm->capture->is_locked, flags);
 *			asrc_m2m_pcm->capture->is_flag &= ~IS_ASRC_RUNNING;
 *			spin_unlock_irqrestore(
 *				&asrc_m2m_pcm->capture->is_locked, flags);
 *			/////ASRC Running end
 */
			if (write_pos + writeable_byte >= dst_buffer_bytes) {

				//1st part copy size
				temp_pos = dst_buffer_bytes - write_pos;
				//1st part copy for out_buf
				memcpy(
					pout_buf + write_pos,
					ptemp_buf,
					temp_pos);

				//2nd part copy size
				Btemp = Wtemp - temp_pos;
				//2nd part copy for out_buf
				memcpy(pout_buf, ptemp_buf + temp_pos, Btemp);

				if (writeable_byte >
				    TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST) {

					tpcm_err("[%d][%s][%d] ERROR!! writeable_byte[%zd] is bigger than max[0x%08x]\n",
					     asrc_m2m_pcm->dev_id,
						 __func__,
					     __LINE__,
						 writeable_byte,
					     (TCC_ASRC_MAX_SIZE
						 *MID_BUFFER_CONST));
				} else {
					memset(
						ptemp_buf,
						0,
					    (TCC_ASRC_MAX_SIZE
						*MID_BUFFER_CONST));
				}
			}

			tcc_pcm_app_position_update(
				asrc_m2m_pcm->dev_id,
				asrc_m2m_pcm->capture,
				runtime,
				Wtemp);
			asrc_m2m_pcm->capture->Bperiod_pos += Wtemp;
			if (asrc_m2m_pcm->capture->Bperiod_pos >=
			    dst_period_bytes) {
				asrc_m2m_pcm->capture->Bperiod_pos =
				    asrc_m2m_pcm->capture->Bperiod_pos %
				    dst_period_bytes;
				snd_pcm_period_elapsed(substream);
			}
#ifdef TAIL_DEBUG
			if (asrc_m2m_pcm->capture->Btail != cap_tail) {
				tpcm_err("[%d][%s][%d] Btail=%d, write_pos=%zd,",
					asrc_m2m_pcm->dev_id,
					__func__,
					__LINE__,
					asrc_m2m_pcm->capture->Btail,
					write_pos);
				pr_err("Wtemp=%zd,Btemp=%zd, read_byte=%zd,",
					Wtemp,
					Btemp,
					read_byte);
				pr_err("readable_byte=%zd,",
					readable_byte);
				pr_err("writeable_byte=%zd!!\n",
					writeable_byte);
			}

			cap_tail = asrc_m2m_pcm->capture->Btail;
#endif
		}
		asrc_m2m_pcm->capture->middle->pre_pos += read_byte;
		if (asrc_m2m_pcm->capture->middle->pre_pos >=
		    asrc_m2m_pcm->capture->middle->buffer_bytes) {
			asrc_m2m_pcm->capture->middle->pre_pos =
			    asrc_m2m_pcm->capture->middle->pre_pos -
			    asrc_m2m_pcm->capture->middle->buffer_bytes;
		}
		readable_byte -= read_byte;
		if (readable_byte < 0) {
			tpcm_warn("[%d] ERROR!! readable_byte: %zd\n",
			     asrc_m2m_pcm->dev_id,
				 readable_byte);
			Ctemp =
			    tcc_is_flag_update
					(asrc_m2m_pcm->capture,
					IS_ASRC_RUNNING,
					IS_FLAG_RELEASE);
			/////ASRC Running end
			ret = -1;
			break;
		}
		tpcm_dbg
		    ("[%d] readable_byte: %zd, read_byte: %zd, pre_pos: %d\n",
		     asrc_m2m_pcm->dev_id,
			 readable_byte,
			 read_byte,
		     asrc_m2m_pcm->capture->middle->pre_pos);
	}
	return ret;
}

static int tcc_ptr_update_thread_for_capture(void *data)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	struct snd_pcm_substream *substream;
	// = asrc_m2m_pcm->asrc_substream;
	struct snd_soc_pcm_runtime *rtd;
	// = substream->private_data;
	struct snd_pcm_runtime *runtime;
	// = substream->runtime;
	snd_pcm_uframes_t temp = 0;
	unsigned int mid_buffer_bytes = 0, max_cpy_byte = 0;
	unsigned int cur_pos = 0, pre_pos = 0;
	char Ctemp = 0;
	int readable_byte = 0;
	int ret = 0;

	while (!kthread_should_stop()) {
		Ctemp =
		    tcc_is_flag_update(asrc_m2m_pcm->capture, 0, IS_FLAG_GET);
		if (((Ctemp & IS_TRIG_STARTED) != 0)
		    && ((Ctemp & IS_ASRC_STARTED) != 0)) {
			if (asrc_m2m_pcm->capture->middle->cur_pos !=
			    asrc_m2m_pcm->capture->middle->pre_pos) {
				substream =
				    asrc_m2m_pcm->capture->asrc_substream;
				rtd = substream->private_data;
				runtime = substream->runtime;

				cur_pos =
				    asrc_m2m_pcm->capture->middle->cur_pos;
				pre_pos =
				    asrc_m2m_pcm->capture->middle->pre_pos;

				mid_buffer_bytes =
				    asrc_m2m_pcm->capture->middle->buffer_bytes;

				temp = frames_to_bytes(runtime,
					runtime->period_size);
				if (temp > TCC_ASRC_MAX_SIZE)
					max_cpy_byte = TCC_ASRC_MAX_SIZE;
				else
					max_cpy_byte = temp;

				temp = 0;

				if (cur_pos >= pre_pos) {
					//tpcm_dbg("[%d][%s][%d]cur=%d,
					//pre=%d\n", asrc_m2m_pcm->dev_id,
					//__func__, __LINE__, cur_pos, pre_pos);
					readable_byte = cur_pos - pre_pos;
					if (readable_byte > 0) {
						ret =
						tcc_ptr_update_func_for_capture(
							substream,
							readable_byte,
							max_cpy_byte);
					if (ret < 0) {
						tpcm_err("[%d][%s][%d]ERROR!! ret: %d\n",
						     asrc_m2m_pcm->dev_id,
						     __func__,
							 __LINE__,
						     ret);
						readable_byte = 0;
					}
					}
				} else {
					//tpcm_dbg("[%d][%s][%d]cur=%d,
					//pre=%d\n",
					//asrc_m2m_pcm->dev_id, __func__,
					//__LINE__, cur_pos, pre_pos);

					//1st part copy for in_buf
					readable_byte =
					    mid_buffer_bytes - pre_pos;

					if (readable_byte > 0) {
						ret =
						tcc_ptr_update_func_for_capture(
							substream,
							readable_byte,
						     max_cpy_byte);
					if (ret < 0) {
						tpcm_err("[%d][%s][%d]ERROR!! ret: %d\n",
						     asrc_m2m_pcm->dev_id,
						     __func__,
						     __LINE__,
							 ret);
						readable_byte = 0;
						cur_pos = 0;
					}
					}
					//2nd part copy
					readable_byte = cur_pos;
					if (readable_byte > 0) {
						ret =
					    tcc_ptr_update_func_for_capture(
							substream,
							readable_byte,
							max_cpy_byte);
					if (ret < 0) {
						tpcm_err("[%d][%s][%d]ERROR!! ret: %d\n",
						     asrc_m2m_pcm->dev_id,
						     __func__,
						     __LINE__,
							 ret);
						readable_byte = 0;
					}
					}
				}	//(cur_pos > pre_pos)?
			} else {
				/*
				 * This kthread check the appl_ptr in
				 * every interval (default 1000us).
				 */
				wait_event_interruptible_timeout(
					asrc_m2m_pcm->capture->kth_wq,
					atomic_read(
					&asrc_m2m_pcm->capture->wakeup),
					usecs_to_jiffies(
					asrc_m2m_pcm->capture->interval));
				atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);
			}
		} else {	//start or closed?
			wait_event_interruptible(
				asrc_m2m_pcm->capture->kth_wq,
				(((asrc_m2m_pcm->capture->is_flag &
				IS_TRIG_STARTED) != 0) &&
				((asrc_m2m_pcm->capture->is_flag &
				IS_ASRC_STARTED) != 0)));
			atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);
		}
	}			//while
	return 0;
}

static ssize_t tcc_appl_ptr_check_for_play(
	struct tcc_asrc_m2m_pcm
	*asrc_m2m_pcm,
	ssize_t readable_byte,
	ssize_t max_asrc_byte,
	char *pin_buf)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc;
	unsigned int mid_buffer_bytes = 0, write_pos = 0, temp_pos =
	    0, pre_pos = 0;
	struct asrc_m2m_pcm_stream *playback;
	ssize_t read_byte = 0, writeable_byte = 0;
	ssize_t Btemp = 0, Wtemp = 0, ret = 0;
	int len = 0;
	char *pout_buf, *ptemp_buf, Ctemp = 0;

	playback = asrc_m2m_pcm->playback;

	mid_buffer_bytes = playback->middle->buffer_bytes;
	ptemp_buf = playback->middle->ptemp_buf;
	pout_buf = playback->middle->dma_buf->area;


#ifdef PERFORM_ASRC_MULTIPLE_TIME
	while (1) {
#endif
		Ctemp =
			tcc_is_flag_update(
				playback,
				IS_ASRC_RUNNING,
				IS_FLAG_RELEASE);

		if ((readable_byte <= 0)
		    || ((Ctemp & IS_TRIG_STARTED) == 0)
		    || ((Ctemp & IS_ASRC_STARTED) == 0)) {

			tpcm_dbg
			    ("[%d] readable_byte: %d, is_flag=0x%02x\n",
			     asrc_m2m_pcm->dev_id,
				 readable_byte,
			     (unsigned int)Ctemp);
#ifdef PERFORM_ASRC_MULTIPLE_TIME
			break;
#else
			return ret;
#endif
		} else {
			Ctemp = tcc_is_flag_update(
				playback,
				IS_ASRC_RUNNING,
				IS_FLAG_SET);
		}
		if (readable_byte > max_asrc_byte)
			read_byte = max_asrc_byte;
		else
			read_byte = readable_byte;


		write_pos = playback->middle->cur_pos;
		pre_pos = playback->middle->pre_pos;

		if (playback->src->rate ==
		    playback->dst->rate) {
			if (write_pos + read_byte >= mid_buffer_bytes) {
				//1st part copy for out_buf
				Btemp = mid_buffer_bytes - write_pos;
				memcpy(pout_buf + write_pos, pin_buf + ret,
				       Btemp);
				temp_pos = Btemp;
				//2nd part copy for out_buf
				Btemp = read_byte - Btemp;
				memcpy(pout_buf, pin_buf + ret + temp_pos,
				       Btemp);

				//playback->middle->cur_pos =
				//Btemp;
			} else {
				memcpy(pout_buf + write_pos, pin_buf + ret,
				       read_byte);
				//playback->middle->cur_pos +=
				//read_byte;
			}

			//ret += read_byte;
			Wtemp = read_byte;
		} else {
			if (playback->pair_id == 99) {
				tpcm_warn("[%d][%s][%d] ERROR!! asrc pair_id[%d] is wrong\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     playback->pair_id);

				Ctemp =
				    tcc_is_flag_update(
						playback,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = -1;
				break;
			}

			writeable_byte =
			    tcc_asrc_m2m_push_data(
					asrc,
					playback->pair_id,
					pin_buf + ret,
					read_byte);

			if (writeable_byte >
			    TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST) {

				tpcm_err("[%d][%s][%d] ERROR!! writeable_byte[0x%08zx] is bigger than max[0x%08x]\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     writeable_byte,
				     TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST);
				Ctemp =
				    tcc_is_flag_update(
						playback,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = -1;
				break;
			} else if (writeable_byte < 0) {
				Ctemp =
				    tcc_is_flag_update(playback,
						       0, IS_FLAG_GET);

				tpcm_err("[%d][%s][%d] ERROR!! read_byte[%zd] writeable_byte[%zd] Ctemp[0x%02x]\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     read_byte,
					 writeable_byte,
					 Ctemp);

				Ctemp =
				    tcc_is_flag_update(
						playback,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = writeable_byte;
				break;
			}

			if (write_pos + writeable_byte >= mid_buffer_bytes) {
				Wtemp =
				    tcc_asrc_m2m_pop_data(
						asrc,
						playback->pair_id,
						ptemp_buf,
						writeable_byte);
			} else {
				Wtemp =
				    tcc_asrc_m2m_pop_data(
						asrc,
						playback->pair_id,
						pout_buf + write_pos,
						writeable_byte);
			}

			if (Wtemp < 0) {
				Ctemp =
				    tcc_is_flag_update(
						playback,
						0,
						IS_FLAG_GET);

				tpcm_err("[%d][%s][%d] ERROR!! writealble_byte[%zd] Wtemp[%zd] Ctemp[0x%08x]!?\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     writeable_byte,
					 Wtemp,
					 Ctemp);

				Ctemp =
				    tcc_is_flag_update(
						playback,
						IS_ASRC_RUNNING,
						IS_FLAG_RELEASE);
				ret = Wtemp;
				break;
			}

			if (write_pos + writeable_byte >= mid_buffer_bytes) {

				//1st part copy for out_buf
				Btemp = mid_buffer_bytes - write_pos;
				memcpy(pout_buf + write_pos, ptemp_buf, Btemp);

				//2nd part copy for out_buf
				temp_pos = Btemp;
				Btemp = Wtemp - Btemp;
				memcpy(pout_buf, ptemp_buf + temp_pos, Btemp);

				if (writeable_byte >
				    TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST) {

					tpcm_err("[%d][%s][%d] ERROR!! writeable_byte[0x%08zx] is bigger than max[0x%08x]\n",
					     asrc_m2m_pcm->dev_id,
						 __func__,
					     __LINE__,
						 writeable_byte,
					     (TCC_ASRC_MAX_SIZE
						 *MID_BUFFER_CONST));
				} else {
					memset(
						ptemp_buf,
						0,
					    (TCC_ASRC_MAX_SIZE
						*MID_BUFFER_CONST));
				}
			}
		}

		playback->middle->cur_pos += Wtemp;
		if (playback->middle->cur_pos >= mid_buffer_bytes) {
			playback->middle->cur_pos =
			    playback->middle->cur_pos -
			    mid_buffer_bytes;
		}

		ret += read_byte;
		readable_byte -= read_byte;
		if (readable_byte < 0) {
			tpcm_err("[%d][%s][%d] ERROR!! readable_byte=%zd, max_asrc_byte=%zd, ret=%zd\n",
			     asrc_m2m_pcm->dev_id,
				 __func__,
				 __LINE__,
			     readable_byte,
				 max_asrc_byte,
				 ret);

			Ctemp =
			    tcc_is_flag_update(
					playback,
					IS_ASRC_RUNNING,
					IS_FLAG_RELEASE);
			ret = -1;
			break;
		}

		len =
			tcc_asrc_footprint_insert(
				asrc_m2m_pcm->asrc_footprint,
				playback->middle->cur_pos,
				read_byte);
		if (len <= 0) {
			tpcm_err("[%d][%s][%d] ERROR!! len=%d\n",
			     asrc_m2m_pcm->dev_id,
				 __func__,
				 __LINE__,
				 len);
			//tcc_asrc_footprint_dump(asrc_m2m_pcm->asrc_footprint);
			ret = -1;
			break;
		}
#if	(CHECK_OVERRUN == 1)

		if ((write_pos > pre_pos)
		    && (playback->middle->cur_pos > pre_pos)
		    && (write_pos > playback->middle->cur_pos)) {
			tpcm_err("[%d] overrun?! pre_pos=%u, write_pos=%u, cur_pos=%u\n",
			     asrc_m2m_pcm->dev_id,
				 pre_pos,
				 write_pos,
			     playback->middle->cur_pos);

		} else if ((write_pos < pre_pos) &&
			   ((playback->middle->cur_pos > pre_pos) ||
			   (write_pos > playback->middle->cur_pos))) {
			tpcm_err("[%d] overrun?? pre_pos=%u, write_pos=%u, cur_pos=%u\n",
			     asrc_m2m_pcm->dev_id,
				 pre_pos,
				 write_pos,
			     playback->middle->cur_pos);
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
	struct asrc_m2m_pcm_stream *playback;
	struct snd_pcm_substream *substream;
	// = asrc_m2m_pcm->asrc_substream;
	struct snd_soc_pcm_runtime *rtd;
	// = substream->private_data;
	struct snd_pcm_runtime *runtime;
	// = substream->runtime;
	snd_pcm_uframes_t pre_appl_ptr = 0, cur_appl_ptr = 0,
		pre_appl_ofs = 0, cur_appl_ofs = 0;
	snd_pcm_uframes_t Ftemp = 0, rewind_frames = 0;
	//snd_pcm_uframes_t dbg_pre_ptr=0;      //rewind debug
	ssize_t readable_byte = 0, max_cpy_byte = 0, read_pos = 0,
		Btemp = 0, ret = 0;
	char *pin_buf, Ctemp = 0;
#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
	snd_pcm_uframes_t read_byte;
#endif

	playback = asrc_m2m_pcm->playback;
	while (!kthread_should_stop()) {

wait_check_play:

		Ctemp = tcc_is_flag_update(playback, 0, IS_FLAG_GET);
		if (((Ctemp & IS_TRIG_STARTED) != 0)
		    && ((Ctemp & IS_ASRC_STARTED) != 0)) {

			substream = playback->asrc_substream;
			rtd = substream->private_data;
			runtime = substream->runtime;

			if ((!substream) || (!rtd) || (!runtime)) {
				tpcm_dbg
				    ("[%d][%s][%d] is_flag=0x%02x\n",
				     asrc_m2m_pcm->dev_id,
					 __func__,
					 __LINE__,
				     (unsigned int)playback->is_flag);
				goto wait_check_play;
			}
			//dbg_pre_ptr = cur_appl_ptr;   //rewind debug
			cur_appl_ptr = runtime->control->appl_ptr;
			pre_appl_ptr = playback->app->pre_ptr;
			if (cur_appl_ptr != pre_appl_ptr) {
				rewind_frames = 0;
				rewind_frames = tcc_pcm_app_rewind_check(
					asrc_m2m_pcm->dev_id,
					playback,
					runtime,
					cur_appl_ptr);
				if (rewind_frames > 0) {
					//rewind debug
					//tpcm_err("[%d] rewind_frames=%lu,
					//dbg_pre_ptr=%lu, cur_ptr=%lu,
					//pre_ptr=%lu\n",
					//asrc_m2m_pcm->dev_id, rewind_frames,
					//dbg_pre_ptr, cur_appl_ptr,
					//pre_appl_ptr);
					//tcc_pcm_app_position_rewind(
					//asrc_m2m_pcm->dev_id,
					//asrc_m2m_pcm->playback,
					//runtime, rewind_frames);
					continue;
				}
				cur_appl_ofs =
				    cur_appl_ptr % runtime->buffer_size;
				   //ehk23 This does not need.
				   //if ((cur_appl_ofs == 0) &&
				   //(cur_appl_ptr > 0)) {
				   //cur_appl_ofs = runtime->buffer_size;
				   //}
				pre_appl_ofs = playback->app->pre_pos;

				Btemp =
				    frames_to_bytes(
						runtime,
						runtime->period_size);
				if (Btemp > TCC_ASRC_MAX_SIZE)
					max_cpy_byte = TCC_ASRC_MAX_SIZE;
				else
					max_cpy_byte = Btemp;

				Btemp = 0;

				pin_buf = substream->dma_buffer.area;

				if (cur_appl_ofs > pre_appl_ofs) {
					//tpcm_dbg("[%d][%s][%d]cur=%lu,
					//pre=%lu, cur_ptr=%lu,
					//pre_ptr=%lu\n", asrc_m2m_pcm->dev_id,
					//__func__, __LINE__, cur_appl_ofs,
					//pre_appl_ofs, cur_appl_ptr,
					//pre_appl_ptr);
					Ftemp = cur_appl_ofs - pre_appl_ofs;
					readable_byte =
					    frames_to_bytes(runtime, Ftemp);
					read_pos =
					    frames_to_bytes(runtime,
					    playback->app->pre_pos) +
					    playback->Btail;

					if (readable_byte > 0) {

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&start);
#endif
#ifdef PERFORM_ASRC_MULTIPLE_TIME
						ret =
						tcc_appl_ptr_check_for_play(
							asrc_m2m_pcm,
							readable_byte,
							(ssize_t)
							TCC_ASRC_UNIT_SIZE,
							pin_buf + read_pos);
#else
						ret =
						tcc_appl_ptr_check_for_play(
							asrc_m2m_pcm,
							readable_byte,
							max_cpy_byte,
							pin_buf + read_pos);
#endif

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&end);

						elapsed_usecs64 =
						    timeval_to_ns(&end) -
						    timeval_to_ns(&start);
						do_div(elapsed_usecs64,
						       NSEC_PER_USEC);
						elapsed_usecs = elapsed_usecs64;

						pr_info("asrc m2m elapsed time :");
						pr_info("%03d usec, %zdbytes\n",
						     elapsed_usecs,
							 ret);
#endif
					if (ret < 0) {
						tpcm_err("[%d][%s][%d]ret=%zd\n",
							asrc_m2m_pcm->dev_id,
							__func__,
							__LINE__,
							ret);
						goto max_cpy_tx;
					} else {
						tcc_pcm_app_position_update(
							asrc_m2m_pcm->dev_id,
							playback,
							runtime,
							ret);
					}

#ifdef TAIL_DEBUG
					if (asrc_m2m_pcm->playback->Btail !=
						play_tail) {
						tpcm_err("[%d][%s][%d] Btail=%d,",
							asrc_m2m_pcm->dev_id,
						    __func__, __LINE__,
						    playback->Btail);
						pr_err("read_pos=%zd, Ftemp=%lu,"
							read_pos,
							Ftemp);
						pr_err("Btemp=%zd, ret=%zd,",
						    Btemp,
							ret);
						pr_err("readable_byte=%zd!!\n",
						    readable_byte);
					}
						play_tail = playback->Btail;
#endif
					}
				} else {
				//(cur_appl_ofs > pre_appl_ofs)?
					//tpcm_dbg("[%d][%s][%d]cur=%lu,
					//pre=%lu, cur_ptr=%lu, pre_ptr=%lu\n",
					//asrc_m2m_pcm->dev_id, __func__,
					//__LINE__, cur_appl_ofs,
					//pre_appl_ofs, cur_appl_ptr,
					//pre_appl_ptr);

					//1st part copy for in_buf
					Ftemp = runtime->buffer_size -
						pre_appl_ofs;
					readable_byte = frames_to_bytes(
						runtime,
						Ftemp);
					read_pos = frames_to_bytes(
						runtime,
						playback->app->pre_pos) +
					    playback->Btail;

					if (readable_byte > 0) {
#ifdef PERFORM_ASRC_MULTIPLE_TIME
						ret =
						    tcc_appl_ptr_check_for_play
						    (asrc_m2m_pcm,
						     readable_byte,
						     (ssize_t)
						     TCC_ASRC_UNIT_SIZE,
						     pin_buf + read_pos);
#else
						ret =
						    tcc_appl_ptr_check_for_play
						    (asrc_m2m_pcm,
						     readable_byte,
						     max_cpy_byte,
						     pin_buf + read_pos);
#endif
					if (ret < 0) {
						tpcm_dbg("[%d][%s][%d]",
							asrc_m2m_pcm->dev_id,
						    __func__,
						    __LINE__);
						tpcm_info("ret=%zd\n", ret);
						goto max_cpy_tx;
					} else {
						tcc_pcm_app_position_update(
							asrc_m2m_pcm->dev_id,
							playback, runtime,
							ret);
					}

#ifdef TAIL_DEBUG
					if (asrc_m2m_pcm->playback->Btail !=
						play_tail) {
						tpcm_err("[%d][%s][%d] Btail=%d,",
							asrc_m2m_pcm->dev_id,
							__func__, __LINE__,
							playback->Btail)
						pr_err("read_pos=%zd, Ftemp=%lu,",
							read_pos,
							Ftemp);
						pr_err("Btemp=%zd, ret=%zd,",
							Btemp, ret);
						pr_err("readable_byte=%zd!!\n",
							readable_byte);
					}
						play_tail = playback->Btail;
#endif
					}

					if (ret >= max_cpy_byte) {
						goto max_cpy_tx;
					} else {
						readable_byte =
						    max_cpy_byte - ret;
					}

					//2nd part copy for in_buf
					Ftemp = cur_appl_ofs;
					Btemp = frames_to_bytes(runtime, Ftemp);
					if (readable_byte > Btemp)
						readable_byte = Btemp;

					if (readable_byte > 0) {
#ifdef PERFORM_ASRC_MULTIPLE_TIME
						ret =
						tcc_appl_ptr_check_for_play
						    (asrc_m2m_pcm,
						     readable_byte,
						     (ssize_t)
						     TCC_ASRC_UNIT_SIZE,
						     pin_buf);
#else
						ret =
						tcc_appl_ptr_check_for_play
						    (asrc_m2m_pcm,
						     readable_byte,
						     max_cpy_byte, pin_buf);
#endif
					if (ret < 0) {
						tpcm_err("[%d][%s][%d]",
							asrc_m2m_pcm->dev_id,
							__func__,
							__LINE__);
						pr_err("ret=%zd\n",
							ret);
						goto max_cpy_tx;
					} else {
						tcc_pcm_app_position_update
						    (asrc_m2m_pcm->dev_id,
						    playback, runtime,
							ret);
					}

#ifdef TAIL_DEBUG
					if (asrc_m2m_pcm->playback->Btail !=
						play_tail) {
						tpcm_err(
						"[%d][%s][%d]",
							asrc_m2m_pcm->dev_id,
							__func__,
							__LINE__);
						pr_err("Btail=%d, read_pos=%zd,",
							playback->Btail,
							read_pos);
						pr_err("Ftemp=%lu, Btemp=%zd,",
							Ftemp, Btemp);
						pr_err("ret=%zd,", ret);
						pr_err("readable_byte=%zd!!\n",
						     readable_byte);
					}
						play_tail = playback->Btail;
#endif
					}
				}	//(cur_appl_ofs > pre_appl_ofs)? else?
max_cpy_tx:
				Ftemp = 0;

				Ctemp = tcc_is_flag_update(
					playback, 0, IS_FLAG_GET);
				if ((!(Ctemp & IS_A7S_STARTED))
				    && ((Ctemp & IS_ASRC_STARTED) != 0)
				    && ((Ctemp & IS_TRIG_STARTED) != 0)) {

					substream =
						playback->asrc_substream;

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
					ret =
					    tcc_asrc_m2m_pcm_set_action_to_mbox
					    (asrc_m2m_pcm->mbox_audio_dev,
					     substream,
					     SNDRV_PCM_TRIGGER_START);
					if (ret < 0) {
						tpcm_err("[%d][%s][%d] ERROR!! ret=%zd\n",
						     asrc_m2m_pcm->dev_id,
						     __func__, __LINE__, ret);
					} else {
						Ctemp =
						    tcc_is_flag_update
						    (playback,
						     IS_A7S_STARTED,
						     IS_FLAG_SET);
					}
#endif
				}

			} else {	//(cur_appl_ptr != pre_appl_ptr)?
				/*
				 * This kthread check the appl_ptr in
				 * every interval (default 100us).
				 */
				wait_event_interruptible_timeout(
					playback->kth_wq,
					atomic_read(&playback->wakeup),
					usecs_to_jiffies(playback->interval));
				atomic_set(&playback->wakeup, 0);
			}	//(cur_appl_ptr != pre_appl_ptr)? else?
		} else {	//start or closed?
			wait_event_interruptible(playback->kth_wq,
				(((playback->is_flag & IS_TRIG_STARTED) != 0) &&
				((playback->is_flag & IS_ASRC_STARTED) != 0)));
			atomic_set(&playback->wakeup, 0);
		}		//start or closed? else ?
	}			//while
	return 0;
}

static int tcc_asrc_m2m_pcm_stream_init(
	struct device *dev,
	struct asrc_m2m_pcm_stream *strm)
{
	struct tcc_asrc_param_t *asrc_m2m_param;
	struct snd_pcm_substream *asrc_substrm;
	struct tcc_mid_buf *middle;
	struct tcc_param_info *src;
	struct tcc_param_info *dst;
	struct tcc_app_buffer_info *app;
	int ret = 0;

	if (strm->asrc_m2m_param == NULL) {
		asrc_m2m_param =
			(struct tcc_asrc_param_t *)devm_kzalloc(dev,
			sizeof(struct tcc_asrc_param_t), GFP_KERNEL);
		if (!asrc_m2m_param) {
			tpcm_err("[%s][%d] Error!!\n",
				__func__,
				__LINE__);
			ret = -ENOMEM;
			goto error_asrc_m2m_param;
		} else {
			memset(
				asrc_m2m_param,
				0,
				sizeof(struct tcc_asrc_param_t));
		}
	} else {
		tpcm_warn("[%s][%d] asrc_m2m_param is already allocated!!\n",
		     __func__,
			 __LINE__);
		asrc_m2m_param = NULL;
	}

	if (strm->asrc_substream == NULL) {
		asrc_substrm =
		    (struct snd_pcm_substream *)devm_kzalloc(dev,
		    sizeof(struct snd_pcm_substream), GFP_KERNEL);
		if (!asrc_substrm) {
			tpcm_err("[%s][%d] Error!!\n",
			       __func__,
				   __LINE__);
			ret = -ENOMEM;
			goto error_asrc_substrm;
		} else {
			memset(
				asrc_substrm,
				0,
				sizeof(struct snd_pcm_substream));
		}
	} else {
		tpcm_warn("[%s][%d] asrc_substreamm is already allocated!!\n",
		     __func__,
			 __LINE__);
		asrc_substrm = NULL;
	}

	if (strm->middle == NULL) {
		middle =
		    (struct tcc_mid_buf *)devm_kzalloc(dev,
		    sizeof(struct tcc_mid_buf), GFP_KERNEL);
		if (!middle) {
			tpcm_err("[%s][%d] Error!!\n",
			       __func__, __LINE__);
			ret = -ENOMEM;
			goto error_middle;
		} else {
			memset(middle, 0, sizeof(struct tcc_mid_buf));
		}

		middle->dma_buf =
		    kzalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
		if (!middle->dma_buf) {
			tpcm_err("[%s][%d] Error!!\n",
				__func__,
				__LINE__);
			ret = -ENOMEM;
			goto error_middle_buf;
		} else {
			memset(
				middle->dma_buf,
				0,
				sizeof(struct snd_dma_buffer));
		}

		middle->ptemp_buf =
			kzalloc(sizeof(unsigned char) *
			TCC_ASRC_MAX_SIZE * MID_BUFFER_CONST, GFP_KERNEL);
		if (!middle->ptemp_buf) {
			tpcm_err("[%s][%d]\n",
				__func__,
				__LINE__);
			ret = -ENOMEM;
			goto error_middle_temp_buf;
		} else {
			memset(
				middle->ptemp_buf,
				0,
				(sizeof(unsigned char)
				*TCC_ASRC_MAX_SIZE
				*MID_BUFFER_CONST));
		}
	} else {
		tpcm_warn("[%s][%d] middle is already allocated!!\n",
			__func__,
			__LINE__);
		middle = NULL;
	}

	if (strm->src == NULL) {
		src =
			devm_kzalloc(
				dev,
				sizeof(struct tcc_param_info),
				GFP_KERNEL);
		if (!src) {
			tpcm_err("[%s][%d] Error!!\n",
			       __func__,
				   __LINE__);
			ret = -ENOMEM;
			goto error_src;
		} else {
			memset(src, 0, sizeof(struct tcc_param_info));
		}
	} else {
		tpcm_warn("[%s][%d] src is already allocated!!\n",
		     __func__,
			 __LINE__);
		src = NULL;
	}

	if (strm->dst == NULL) {
		dst =
			devm_kzalloc(
				dev,
				sizeof(struct tcc_param_info),
				GFP_KERNEL);
		if (!dst) {
			tpcm_err("[%s][%d] Error!!\n",
				__func__,
				__LINE__);
			ret = -ENOMEM;
			goto error_dst;
		} else {
			memset(dst, 0, sizeof(struct tcc_param_info));
		}
	} else {
		tpcm_warn("[%s][%d] dst is already allocated!!\n",
		     __func__,
			 __LINE__);
		dst = NULL;
	}

	if (strm->app == NULL) {
		app =
			(struct tcc_app_buffer_info *)devm_kzalloc(
			dev, sizeof(struct tcc_app_buffer_info), GFP_KERNEL);
		if (!app) {
			tpcm_err("[%s][%d] Error!!\n",
				__func__,
				__LINE__);
			ret = -ENOMEM;
			goto error_app;
		} else {
			memset(app, 0, sizeof(struct tcc_app_buffer_info));
		}
	} else {
		tpcm_warn("[%s][%d] app is already allocated!!\n",
			__func__,
			__LINE__);
		app = NULL;
	}

	strm->asrc_m2m_param = asrc_m2m_param;
	strm->asrc_substream = asrc_substrm;
	strm->middle = middle;

	strm->src = src;
	strm->dst = dst;
	strm->app = app;

	strm->first_open = FALSE;
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

static void tcc_asrc_m2m_pcm_stream_deinit(
	struct device *dev,
	struct asrc_m2m_pcm_stream *strm)
{
	struct tcc_asrc_param_t *asrc_m2m_param;
	struct tcc_mid_buf *middle;
	struct tcc_param_info *src;
	struct tcc_param_info *dst;
	struct tcc_app_buffer_info *app;

	if (strm->asrc_m2m_param) {
		asrc_m2m_param = strm->asrc_m2m_param;
		devm_kfree(dev, asrc_m2m_param);
	}

	if (strm->middle) {
		middle = strm->middle;
		kfree(middle->ptemp_buf);
		kfree(middle->dma_buf);
		devm_kfree(dev, middle);
	}

	if (strm->src) {
		src = strm->src;
		devm_kfree(dev, src);
	}

	if (strm->dst) {
		dst = strm->dst;
		devm_kfree(dev, dst);
	}

	if (strm->app) {
		app = strm->app;
		devm_kfree(dev, app);
	}
}

static int tcc_asrc_m2m_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);
	struct platform_device *pdev;
	bool playback = TRUE, capture = TRUE;
	int ret;

	if (asrc_m2m_pcm) {
		pdev = asrc_m2m_pcm->pdev;
	} else {
		tpcm_err("[%s][%d] ERROR!!\n", __func__, __LINE__);
		return -1;
	}

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	if (dai_link->playback_only)
		capture = FALSE;

	if (dai_link->capture_only)
		playback = FALSE;

	if (playback) {
		ret = tcc_asrc_m2m_pcm_stream_init(
			&pdev->dev,
			asrc_m2m_pcm->playback);
		if (ret != 0) {
			tpcm_dbg("[%d][%s][%d] Error!!\n",
				asrc_m2m_pcm->dev_id,
				__func__,
				__LINE__);
			goto error_playback_stream_init;
		}
#ifdef FOOTPRINT_LINKED_LIST
		asrc_m2m_pcm->asrc_footprint =
		    kzalloc(sizeof(struct List), GFP_KERNEL);
#else
		asrc_m2m_pcm->asrc_footprint =
		    kzalloc(sizeof(struct footprint), GFP_KERNEL);
#endif
		if (!asrc_m2m_pcm->asrc_footprint) {
			tpcm_dbg("[%d][%s][%d] Error!!\n",
				asrc_m2m_pcm->dev_id,
				__func__,
				__LINE__);
			ret = -ENOMEM;
			goto error_footprint;
		} else {
			tcc_asrc_footprint_init(asrc_m2m_pcm->asrc_footprint);
		}

		ret = snd_dma_alloc_pages(
			SNDRV_DMA_TYPE_DEV,
			card->dev,
			MAX_BUFFER_BYTES * MID_BUFFER_CONST,
			asrc_m2m_pcm->playback->middle->dma_buf);
		if (ret)
			goto error_alloc_pages_play;

	}

	if (capture) {
		ret = tcc_asrc_m2m_pcm_stream_init(
			&pdev->dev,
			asrc_m2m_pcm->capture);
		if (ret != 0) {
			tpcm_dbg("[%d][%s][%d] Error!!\n",
				asrc_m2m_pcm->dev_id,
				__func__,
				__LINE__);
			goto error_capture_stream_init;
		}

		ret = snd_dma_alloc_pages(
			SNDRV_DMA_TYPE_DEV,
			card->dev,
			MAX_BUFFER_BYTES * MID_BUFFER_CONST,
			asrc_m2m_pcm->capture->middle->dma_buf);
		if (ret)
			goto error_alloc_pages_cap;

	}

	ret = snd_pcm_lib_preallocate_pages_for_all(
		pcm,
		SNDRV_DMA_TYPE_DEV,
		card->dev,
		MAX_BUFFER_BYTES,
		MAX_BUFFER_BYTES);
	if (ret)
		goto error_prealloc;

	if (playback) {
		atomic_set(&asrc_m2m_pcm->playback->wakeup, 0);

		init_waitqueue_head(&asrc_m2m_pcm->playback->kth_wq);
		spin_lock_init(&asrc_m2m_pcm->playback->is_locked);
	}

	if (capture) {
		atomic_set(&asrc_m2m_pcm->capture->wakeup, 0);

		init_waitqueue_head(&asrc_m2m_pcm->capture->kth_wq);
		spin_lock_init(&asrc_m2m_pcm->capture->is_locked);
	}
	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);

	return 0;

error_prealloc:
	if (capture) {
		snd_dma_free_pages(asrc_m2m_pcm->capture->middle->dma_buf);
error_alloc_pages_cap:
		tcc_asrc_m2m_pcm_stream_deinit(
			&pdev->dev,
			asrc_m2m_pcm->playback);
	}
error_capture_stream_init:
	if (playback) {
		snd_dma_free_pages(asrc_m2m_pcm->playback->middle->dma_buf);
error_alloc_pages_play:
		kfree(asrc_m2m_pcm->asrc_footprint);
error_footprint:
		tcc_asrc_m2m_pcm_stream_deinit(
			&pdev->dev,
			asrc_m2m_pcm->capture);
	}
error_playback_stream_init:
	return ret;
}

static void tcc_asrc_m2m_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(
	    rtd->platform);

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	snd_pcm_lib_preallocate_free_for_all(pcm);
	snd_dma_free_pages(asrc_m2m_pcm->playback->middle->dma_buf);
	snd_dma_free_pages(asrc_m2m_pcm->capture->middle->dma_buf);

	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
}

static struct snd_soc_platform_driver tcc_asrc_m2m_pcm_platform = {
	.ops = &tcc_asrc_m2m_pcm_ops,
	.pcm_new = tcc_asrc_m2m_pcm_new,
	.pcm_free = tcc_asrc_m2m_pcm_free_dma_buffers,
};

#if defined(CONFIG_PM)
static int tcc_asrc_m2m_pcm_suspend(
	struct platform_device *pdev,
	pm_message_t state)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)platform_get_drvdata(pdev);

	tpcm_dbg("[%d][%s][%d] start\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);

	if (asrc_m2m_pcm->playback->first_open == TRUE) {
		if (asrc_m2m_pcm->playback->kth_id != NULL) {
			tpcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->playback->kth_id);
			asrc_m2m_pcm->playback->kth_id = NULL;
		}
		asrc_m2m_pcm->playback->first_open = FALSE;
	}

	if (asrc_m2m_pcm->capture->first_open == TRUE) {
		if (asrc_m2m_pcm->capture->kth_id != NULL) {
			tpcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->capture->kth_id);
			asrc_m2m_pcm->capture->kth_id = NULL;
		}
		asrc_m2m_pcm->capture->first_open = FALSE;
	}

	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_resume(struct platform_device *pdev)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)platform_get_drvdata(pdev);
	asrc_m2m_pcm->playback->first_open = FALSE;
	asrc_m2m_pcm->capture->first_open = FALSE;
	return 0;
}
#endif

static int parse_asrc_m2m_pcm_dt(
	struct platform_device *pdev,
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	unsigned int itemp = 0;
	int ret = 0;

	asrc_m2m_pcm->pdev = pdev;

	asrc_m2m_pcm->dev_id = -1;
	asrc_m2m_pcm->dev_id =
	    of_alias_get_id(pdev->dev.of_node, "asrc-m2m-pcm");
	if (asrc_m2m_pcm->dev_id < 0) {
		tpcm_err("[%s] id[%d] is wrong.\n",
			__func__,
			asrc_m2m_pcm->dev_id);
		ret = -EINVAL;
		goto error_dev_id;
	}
	tpcm_dbg("[%s] dev_id[%u]\n", __func__, asrc_m2m_pcm->dev_id);

	if ((of_property_read_u32
	     (pdev->dev.of_node, "playback-pair-id", &itemp)) >= 0) {
		asrc_m2m_pcm->playback->pair_id = itemp;
	} else {
		asrc_m2m_pcm->playback->pair_id = 99;
	}
	tpcm_dbg("[%s] playback_pair_id[%u]\n",
		__func__,
		asrc_m2m_pcm->playback->pair_id);

	if ((of_property_read_u32(pdev->dev.of_node, "capture-pair-id", &itemp))
	    >= 0) {
		asrc_m2m_pcm->capture->pair_id = itemp;
	} else {
		asrc_m2m_pcm->capture->pair_id = 99;
	}
	tpcm_dbg("[%s] capture_pair_id[%u]\n",
		__func__,
		asrc_m2m_pcm->capture->pair_id);

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	if ((of_property_read_u32
	     (pdev->dev.of_node, "playback-amd-cmd", &itemp)) >= 0) {
		if ((itemp >= 0x0) && (itemp < 0x9)) {
			//0~5: playback stream to mixer, 6~8: general
			asrc_m2m_pcm->playback->mbox_cmd_type =
			    (char)(itemp & 0xFF);
		} else {
			asrc_m2m_pcm->playback->mbox_cmd_type = 99;
		}
	} else {
		asrc_m2m_pcm->playback->mbox_cmd_type = 99;
	}
	tpcm_dbg("[%s] playback-amd-cmd[%u]\n",
		__func__,
		(unsigned int)asrc_m2m_pcm->playback->mbox_cmd_type);

	if ((of_property_read_u32(pdev->dev.of_node, "capture-amd-cmd",
		&itemp)) >= 0) {
		if ((itemp >= 0x0) && (itemp < 0x9)) {
			//0~5: playback stream to mixer, 6~8: general
			asrc_m2m_pcm->capture->mbox_cmd_type =
			    (char)(itemp & 0xFF);
		} else {
			asrc_m2m_pcm->capture->mbox_cmd_type = 99;
		}
	} else {
		asrc_m2m_pcm->capture->mbox_cmd_type = 99;
	}
	tpcm_dbg("[%s] capture-amd-cmd[%u]\n",
		__func__,
		(unsigned int)asrc_m2m_pcm->capture->mbox_cmd_type);

	if ((asrc_m2m_pcm->playback->mbox_cmd_type == 99)
	    && (asrc_m2m_pcm->capture->mbox_cmd_type == 99)) {
		tpcm_err("[%s] ERROR!! cmdtype number is wrong! Please check device tree.\n",
		     __func__);
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

	tpcm_dbg("[%s][%d] start\n", __func__, __LINE__);

	asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)devm_kzalloc(
			&pdev->dev,
			sizeof(struct
			tcc_asrc_m2m_pcm),
			GFP_KERNEL);
	if (!asrc_m2m_pcm) {
		tpcm_err("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_pcm;
	} else {
		memset(asrc_m2m_pcm, 0, sizeof(struct tcc_asrc_m2m_pcm));
	}

	asrc =
	    (struct tcc_asrc_t *)devm_kzalloc(
			&pdev->dev,
			sizeof(struct tcc_asrc_t),
			GFP_KERNEL);
	if (!asrc) {
		tpcm_err("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_asrc;
	} else {
		memset(asrc, 0, sizeof(struct tcc_asrc_t));
	}

	playback_strm =
	    devm_kzalloc(&pdev->dev, sizeof(struct asrc_m2m_pcm_stream),
	    GFP_KERNEL);
	if (!playback_strm) {
		tpcm_err("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_playback_strm;
	} else {
		memset(playback_strm, 0, sizeof(struct asrc_m2m_pcm_stream));
	}

	capture_strm =
	    devm_kzalloc(
			&pdev->dev,
			sizeof(struct asrc_m2m_pcm_stream),
			GFP_KERNEL);
	if (!capture_strm) {
		tpcm_err("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_capture_strm;
	} else {
		memset(capture_strm, 0, sizeof(struct asrc_m2m_pcm_stream));
	}

	of_node_asrc = of_parse_phandle(pdev->dev.of_node, "asrc", 0);
	if (of_node_asrc == NULL) {
		tpcm_err("[%s] ERROR!! of_node_asrc is NULL!\n",
		     __func__);
		ret = -EINVAL;
		goto error_of_node;
	}

	asrc = tcc_asrc_get_handle_by_node(of_node_asrc);
	if (asrc == NULL) {
		tpcm_err("[%s] ERROR!! asrc is NULL!\n",
		       __func__);
		ret = -EINVAL;
		goto error_of_node;
	}

	asrc_m2m_pcm->asrc = asrc;
	asrc_m2m_pcm->playback = playback_strm;
	asrc_m2m_pcm->capture = capture_strm;

	ret = parse_asrc_m2m_pcm_dt(pdev, asrc_m2m_pcm);

	if (ret < 0) {
		tpcm_err("[%s]: Fail to parse asrc_m2m_pcm dt\n",
		     __func__);
		goto error_parse;
	}

	asrc_m2m_pcm->dev = &pdev->dev;

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	asrc_m2m_pcm->mbox_audio_dev = get_tcc_mbox_audio_device();
	if (asrc_m2m_pcm->mbox_audio_dev == NULL) {
		tpcm_dbg("[%d][%s] ERROR!! mbox_audio_dev is NULL!\n",
			asrc_m2m_pcm->dev_id,
			__func__);
		goto error_mailbox;
	}
#endif

	platform_set_drvdata(pdev, asrc_m2m_pcm);

	ret = snd_soc_register_platform(&pdev->dev, &tcc_asrc_m2m_pcm_platform);
	if (ret < 0) {
		tpcm_err(" tcc_asrc_m2m_pcm_platform_register failed\n");
		goto error_register;
	}
	//tpcm_dbg(" tcc_asrc_m2m_pcm_platform_register success\n");

	tpcm_dbg("[%d][%s][%d] end\n",
		asrc_m2m_pcm->dev_id,
		__func__,
		__LINE__);
	return ret;

error_register:
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
error_mailbox:
#endif
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
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm =
	    (struct tcc_asrc_m2m_pcm *)platform_get_drvdata(pdev);
	tpcm_dbg("[%s]\n", __func__);

	if (asrc_m2m_pcm->playback->first_open == TRUE) {
		if (asrc_m2m_pcm->playback->kth_id != NULL) {
			tpcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->playback->kth_id);
			asrc_m2m_pcm->playback->kth_id = NULL;
		}
		asrc_m2m_pcm->playback->first_open = FALSE;
	}

	if (asrc_m2m_pcm->capture->first_open == TRUE) {
		if (asrc_m2m_pcm->capture->kth_id != NULL) {
			tpcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->capture->kth_id);
			asrc_m2m_pcm->capture->kth_id = NULL;
		}
		asrc_m2m_pcm->capture->first_open = FALSE;
	}
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_mbox_audio_unregister_set_kernel_callback(
		asrc_m2m_pcm->mbox_audio_dev,
		asrc_m2m_pcm->playback->mbox_cmd_type);
	tcc_mbox_audio_unregister_set_kernel_callback(
		asrc_m2m_pcm->mbox_audio_dev,
		asrc_m2m_pcm->capture->mbox_cmd_type);
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

static const struct of_device_id tcc_asrc_m2m_pcm_of_match[] = {
	{.compatible = "telechips,asrc_m2m_pcm"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_asrc_m2m_pcm_of_match);

static struct platform_driver tcc_asrc_m2m_pcm_driver = {
	.probe = tcc_asrc_m2m_pcm_probe,
	.remove = tcc_asrc_m2m_pcm_remove,
#if defined(CONFIG_PM)
	.suspend = tcc_asrc_m2m_pcm_suspend,
	.resume = tcc_asrc_m2m_pcm_resume,
#endif
	.driver = {
		.name = "tcc_asrc_m2m_pcm_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_asrc_m2m_pcm_of_match),
#endif
	},
};

module_platform_driver(tcc_asrc_m2m_pcm_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips ASRC M2M PCM Driver");
MODULE_LICENSE("GPL");
