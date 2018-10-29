#ifndef _TCC_ASRC_M2M_PCM_DT_H_
#define _TCC_ASRC_M2M_PCM_DT_H_

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
#include <sound/tcc/params/tcc_mbox_audio_pcm_params.h>
#include <sound/tcc/utils/tcc_mbox_audio_utils.h>
#endif

#include "tcc_adma.h"
#include "tcc_adma_pcm.h"
#include "asrc/tcc_asrc_drv.h"
//#include "./asrc/tcc_asrc_m2m.h"

//define: Playback/Capture by hw:0,0
//#define TEST_BY_ALSA

#define FILE_PCM_PLAYBACK   "/dev/snd/pcmC0D0p"
#define FILE_PCM_CAPTURE    "/dev/snd/pcmC0D0c"
#define FILE_CONTROL        "/dev/snd/controlC0"

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

typedef enum {
	TCC_ASRC_M2M_7_1CH = 0,
	TCC_ASRC_M2M_STEREO = 1,
	TCC_ASRC_M2M_TYPE_MAX,
} TCC_ASRC_DEV_TYPE;

#ifdef TEST_BY_ALSA
struct tcc_snd_dev_for_test {
	struct file *filp;
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
	char *dev_name;
	int access;
	int format;
	int channels;
	int rate;
};
#endif

struct tcc_mid_buf {
	unsigned char *buf;
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
#ifdef TEST_BY_ALSA
	struct tcc_snd_dev_for_test *test_dev;
#endif
	struct task_struct *kth_id_ptr_check;
	struct task_struct *kth_id_ptr_update;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	struct mbox_audio_device *mbox_audio_dev;
    unsigned short mbox_cmd_type;
#endif
	bool first_open;
	bool is_closed;
	bool is_asrc_started;
	bool is_a7s_started;
	bool is_asrc_running;
	unsigned int interval; //ms
	ssize_t Bwrote; //Bytes 
	wait_queue_head_t check_wq;
	wait_queue_head_t update_wq;
};

#endif //_TCC_ASRC_M2M_PCM_DT_H_
