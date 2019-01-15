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

#ifndef _TCC_ASRC_M2M_PCM_DT_H_
#define _TCC_ASRC_M2M_PCM_DT_H_

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
#include <sound/tcc/params/tcc_mbox_audio_pcm_def.h>
#include <sound/tcc/utils/tcc_mbox_audio_utils.h>
#endif

#include "tcc_adma.h"
#include "tcc_adma_pcm.h"
#include "asrc/tcc_asrc_drv.h"
//#include "./asrc/tcc_asrc_m2m.h"

//define: Playback/Capture by hw:0,0
//#define TEST_BY_ALSA

//#define FILE_PCM_PLAYBACK   "/dev/snd/pcmC0D0p"
//#define FILE_PCM_CAPTURE    "/dev/snd/pcmC0D0c"
//#define FILE_CONTROL        "/dev/snd/controlC0"

//TCC803x ASRC block has 4 pairs.
//Only 1 pair of TCC803x ASRC block supports multichannel.
#define ASRC_7_1CH_DEV_NUM 0 

#define ASRC_DST_RATE 48000	//For TX
#define ASRC_SRC_RATE 48000	//For RX

#define TCC_ASRC_MAX_SIZE (32768)	//From m2m driver

#define MAX_BUFFER_BYTES		(65536)

#define MIN_PERIOD_BYTES		(256)
#define MIN_PERIOD_CNT			(2)

#define PLAYBACK_MAX_PERIOD_BYTES		(MAX_BUFFER_BYTES/2)
#define CAPTURE_MAX_PERIOD_BYTES		(MAX_BUFFER_BYTES/4)

#define MID_BUFFER_CONST 4
#define TCC_APP_PTR_CHECK_INTERVAL_TX 5 //msec
#define TCC_APP_PTR_CHECK_INTERVAL_RX 5 //msec

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
#define MBOX_MSG_SIZE_FOR_ACTION    AUDIO_MBOX_PCM_ACTION_MESSAGE_SIZE
#define MBOX_MSG_SIZE_FOR_PARAMS    AUDIO_MBOX_PCM_PARAM_MESSAGE_SIZE
#define MBOX_MSG_SIZE_FOR_POSITION  AUDIO_MBOX_PCM_POSITION_MESSAGE_SIZE
#endif

//This is for is_flag in structure of tcc_asrc_m2m_pcm
#define IS_TRIG_STARTED 0x01
#define IS_A7S_STARTED 0x02
#define IS_ASRC_STARTED 0x04
#define IS_ASRC_RUNNING 0x08

typedef enum {
	TCC_ASRC_M2M_7_1CH = 0,
	TCC_ASRC_M2M_STEREO = 1,
	TCC_ASRC_M2M_TYPE_MAX,
} TCC_ASRC_DEV_TYPE;

struct tcc_mid_buf {
	unsigned char *ptemp_buf;
	unsigned int cur_pos;
	unsigned int pre_pos;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
    unsigned int cur_pos_from_ipc;
	unsigned int pre_pos_from_ipc;
#endif
	unsigned int buffer_bytes;
	struct snd_dma_buffer *dma_buf;
};

struct tcc_param_info {
	unsigned int period_bytes;
	unsigned int buffer_bytes;
	unsigned int rate;
};

struct tcc_app_buffer_info {
	snd_pcm_uframes_t pre_pos;
	snd_pcm_uframes_t pre_ptr;
//	snd_pcm_uframes_t cur_pos;	//for TX
//	snd_pcm_uframes_t periods;	//for TX
};

typedef struct _Node {
	unsigned int print_pos;
	ssize_t input_byte;	//bytes
	struct _Node *next;
	struct _Node *prev;
} Node;

typedef struct _List {
	Node *head;
	Node *tail;
	int list_len;
} List;

struct tcc_asrc_m2m_pcm {
	struct device *dev;
	unsigned int pair_id;
	struct tcc_asrc_t *asrc;
	struct tcc_asrc_param_t *asrc_m2m_param;
	struct snd_pcm_substream *asrc_substream;
	struct tcc_mid_buf *middle;
	struct tcc_param_info *src;
	struct tcc_param_info *dst;
	struct tcc_app_buffer_info *app;
	struct task_struct *kth_id;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	struct mbox_audio_device *mbox_audio_dev;
    unsigned short mbox_cmd_type;
#endif
	bool first_open;
	char is_flag;
	/*
	#define IS_TRIG_STARTED 0x01
	#define IS_A7S_STARTED 0x02
	#define IS_ASRC_STARTED 0x04
	#define IS_ASRC_RUNNING 0x08
	*/
	unsigned int interval; //ms
	ssize_t Bwrote; //Bytes 
	List *asrc_footprint;	//for TX
	wait_queue_head_t kth_wq;
	atomic_t wakeup;
	spinlock_t is_locked;
};

#endif //_TCC_ASRC_M2M_PCM_DT_H_
