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

#undef asrc_m2m_pcm_dbg
#if 0
#define asrc_m2m_pcm_dbg(f, a...)	printk("<ASRC M2M PCM>" f, ##a)
#define asrc_m2m_pcm_dbg_id(id, f, a...)	printk("<ASRC M2M PCM-%d>" f, id, ##a)
#else
#define asrc_m2m_pcm_dbg(f, a...)
#define asrc_m2m_pcm_dbg_id(id, f, a...)	//printk("<ASRC M2M PCM-%d>" f, id, ##a)
#endif

#define CHECK_ASRC_M2M_ELAPSED_TIME	(0)

static int tcc_hw_ptr_check_thread_for_capture(void *data);
static int tcc_ptr_update_thread_for_capture(void *data);
static int tcc_appl_ptr_check_thread_for_play(void *data);
static int tcc_ptr_update_thread_for_play(void *data);

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

#ifdef TEST_BY_ALSA
//From kernel/sound/core/oss/pcm_oss.c
static int snd_interval_refine_set(struct snd_interval *i, unsigned int val)
{
	struct snd_interval t;
	t.empty = 0;
	t.min = t.max = val;
	t.openmin = t.openmax = 0;
	t.integer = 1;
	return snd_interval_refine(i, &t);
}

//From kernel/sound/core/oss/pcm_oss.c
static int _snd_pcm_hw_param_set(struct snd_pcm_hw_params *params,
		snd_pcm_hw_param_t var, unsigned int val,
		int dir)
{
	int changed;
	if (hw_is_mask(var)) {
		struct snd_mask *m = hw_param_mask(params, var);
		if (val == 0 && dir < 0) {
			changed = -EINVAL;
			snd_mask_none(m);
		} else {
			if (dir > 0)
				val++;
			else if (dir < 0)
				val--;
			changed = snd_mask_refine_set(
					hw_param_mask(params, var), val);
		}
	} else if (hw_is_interval(var)) {
		struct snd_interval *i = hw_param_interval(params, var);
		if (val == 0 && dir < 0) {
			changed = -EINVAL;
			snd_interval_none(i);
		} else if (dir == 0)
			changed = snd_interval_refine_set(i, val);
		else {
			struct snd_interval t;
			t.openmin = 1;
			t.openmax = 1;
			t.empty = 0;
			t.integer = 0;
			if (dir < 0) {
				t.min = val - 1;
				t.max = val;
			} else {
				t.min = val;
				t.max = val+1;
			}
			changed = snd_interval_refine(i, &t);
		}
	} else
		return -EINVAL;
	if (changed) {
		params->cmask |= 1 << var;
		params->rmask |= 1 << var;
	}
	return changed;
}

static int tcc_test_dev_default_hw_params(struct tcc_snd_dev_for_test *test_dev)
{
	struct snd_pcm_substream *substream = test_dev->substream;
	struct snd_pcm_hw_params *params;
	snd_pcm_sframes_t result;
/*	//ref. code
	test_dev->access = SNDRV_PCM_ACCESS_RW_INTERLEAVED;
	test_dev->format = SNDRV_PCM_FORMAT_S16_LE;
	test_dev->channels = 2;
	test_dev->rate = 48000;
*/
	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if(!params) {
		return -ENOMEM;
	}

	_snd_pcm_hw_params_any(params);
	_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_ACCESS,
			test_dev->access, 0);
	_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_FORMAT,
			test_dev->format, 0);
	_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_CHANNELS,
			test_dev->channels, 0);
	_snd_pcm_hw_param_set(params, SNDRV_PCM_HW_PARAM_RATE,
			test_dev->rate, 0);

	snd_pcm_kernel_ioctl(substream, SNDRV_PCM_IOCTL_DROP, NULL);
	snd_pcm_kernel_ioctl(substream, SNDRV_PCM_IOCTL_HW_PARAMS, params);

	result = snd_pcm_kernel_ioctl(substream, SNDRV_PCM_IOCTL_PREPARE, NULL);
	if (result < 0) {
		asrc_m2m_pcm_dbg("Preparing sound card failed: %d\n", (int)result);
		kfree(params);
		return result;
	}

	/* Store the hardware parameters */
	test_dev->access = params_access(params);
	test_dev->format = params_format(params);
	test_dev->channels = params_channels(params);
	test_dev->rate = params_rate(params);

	kfree(params);

	asrc_m2m_pcm_dbg("Hardware params: access %x, format %x, channels %d, rate %d\n",
			test_dev->access, test_dev->format, test_dev->channels, test_dev->rate);

	return 0;
}
#endif

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
		printk("[%s] fail to get phy addr\n", __func__);
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

static void function_for_mbox_callback(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm)
{
	struct snd_pcm_substream *substream;// = asrc_m2m_pcm->asrc_substream;
	unsigned int mid_buffer_bytes=0, src_period_bytes=0, write_pos=0;
	ssize_t write_sz=0;

	substream = asrc_m2m_pcm->asrc_substream;
	src_period_bytes = (asrc_m2m_pcm->dst->rate * 10000) / asrc_m2m_pcm->src->rate;
	src_period_bytes = (asrc_m2m_pcm->src->period_bytes * src_period_bytes) / 10000;

	mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;
	write_pos = asrc_m2m_pcm->middle->pre_pos;
	//printk("[%s][%d]cur_pos=%d\n", __func__, __LINE__, (unsigned int)asrc_m2m_pcm->middle->cur_pos_from_ipc);
	if(asrc_m2m_pcm->middle->cur_pos_from_ipc != write_pos) {
		if(write_pos > asrc_m2m_pcm->middle->cur_pos_from_ipc) {
			write_sz = mid_buffer_bytes - write_pos;
			write_sz += asrc_m2m_pcm->middle->cur_pos_from_ipc;
		} else {
			write_sz = asrc_m2m_pcm->middle->cur_pos_from_ipc - write_pos;
		}
	} else {
		write_sz = 0;
	}

	write_pos += write_sz;
	if(write_pos >= mid_buffer_bytes) {
		write_pos = write_pos - mid_buffer_bytes;
	}
	asrc_m2m_pcm->middle->pre_pos = write_pos;
	//printk("[%s][%d]write_sz=%d\n", __func__, __LINE__, (unsigned int)write_sz);

	asrc_m2m_pcm->Bwrote += write_sz;
	if(asrc_m2m_pcm->Bwrote >= src_period_bytes) {
		//printk("[%s][%d]Bwrote=%d, src_period_bytes=%d\n", __func__, __LINE__, asrc_m2m_pcm->Bwrote, src_period_bytes);
		asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % src_period_bytes;
		snd_pcm_period_elapsed(substream);
	}
}

static void tcc_asrc_m2m_pcm_mbox_callback(void *data, unsigned int *msg, unsigned short msg_size, unsigned short cmd_type)
{
    struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	//unsigned int cmd = msg[0];	//for debug
	//unsigned int device = msg[1];	//for debug
	unsigned int pos = msg[2];

	if (asrc_m2m_pcm == NULL) {
		printk("[%s] Cannot get asrc_m2m_pcm driver data\n", __func__);
		return;
	}

    if (cmd_type != asrc_m2m_pcm->mbox_cmd_type) {
		printk("[%s] Invalid cmd type between %u and %u\n", __func__, cmd_type, asrc_m2m_pcm->mbox_cmd_type);
		return;
    }

	if(asrc_m2m_pcm->middle->cur_pos_from_ipc != pos) {
		asrc_m2m_pcm->middle->pre_pos_from_ipc = asrc_m2m_pcm->middle->cur_pos_from_ipc;
		asrc_m2m_pcm->middle->cur_pos_from_ipc = pos;
		function_for_mbox_callback(asrc_m2m_pcm);
	}
    //printk("[%s] callback get! cmd = %d, device = %d, position = 0x%08x, cmd_type = %d, msg_size = %d\n", __func__,cmd, device, pos, cmd_type, msg_size);
	return;
}
#endif

static int tcc_asrc_m2m_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	struct task_struct *pkth_id_ptr_check;
	struct task_struct *pkth_id_ptr_update;
#ifdef TEST_BY_ALSA
	struct snd_pcm_file *pcm_file;
#endif

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
	if(asrc_m2m_pcm->middle->buf != NULL)
		memset(asrc_m2m_pcm->middle->buf, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);

	if(asrc_m2m_pcm->middle->dma_buf->area != NULL)
		memset(asrc_m2m_pcm->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
	asrc_m2m_pcm->is_closed = false;
	asrc_m2m_pcm->is_asrc_started = false;
	asrc_m2m_pcm->is_asrc_running = false;
	asrc_m2m_pcm->is_a7s_started = false;

	asrc_m2m_pcm->Bwrote = 0;

	if(asrc_m2m_pcm->first_open == false) {
		asrc_m2m_pcm->first_open = true;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
			pkth_id_ptr_check = (struct task_struct *) kthread_run(tcc_appl_ptr_check_thread_for_play, (void *)(asrc_m2m_pcm), "tcc_appl_ptr_check_thread_for_play");
			pkth_id_ptr_update = (struct task_struct *) kthread_run(tcc_ptr_update_thread_for_play, (void *)(asrc_m2m_pcm), "tcc_ptr_update_thread_for_play");
		} else {
			pkth_id_ptr_check = (struct task_struct *) kthread_run(tcc_hw_ptr_check_thread_for_capture, (void *)(asrc_m2m_pcm), "tcc_hw_ptr_check_thread_for_capture");
			pkth_id_ptr_update = (struct task_struct *) kthread_run(tcc_ptr_update_thread_for_capture, (void *)(asrc_m2m_pcm), "tcc_ptr_update_thread_for_capture");
		}
		asrc_m2m_pcm->kth_id_ptr_check = pkth_id_ptr_check;
		asrc_m2m_pcm->kth_id_ptr_update = pkth_id_ptr_update;
	}

	//ALSA DEVICE OPEN
#ifdef TEST_BY_ALSA
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
		asrc_m2m_pcm->test_dev->dev_name = FILE_PCM_PLAYBACK;
		asrc_m2m_pcm->test_dev->filp = filp_open(asrc_m2m_pcm->test_dev->dev_name, O_WRONLY, 0);
		if (IS_ERR(asrc_m2m_pcm->test_dev->filp)) {
			asrc_m2m_pcm_dbg("No such PCM device: %s\n", asrc_m2m_pcm->test_dev->dev_name);
			asrc_m2m_pcm->test_dev->substream = NULL;
			return -EFAULT;
		}
	} else {
		asrc_m2m_pcm->test_dev->dev_name = FILE_PCM_CAPTURE;
		asrc_m2m_pcm->test_dev->filp = filp_open(asrc_m2m_pcm->test_dev->dev_name, O_RDONLY, 0);
		if (IS_ERR(asrc_m2m_pcm->test_dev->filp)) {
			asrc_m2m_pcm_dbg("No such PCM device: %s\n", asrc_m2m_pcm->test_dev->dev_name);
			asrc_m2m_pcm->test_dev->substream = NULL;
			return -EFAULT;
		}
	}
	pcm_file = asrc_m2m_pcm->test_dev->filp->private_data;
	asrc_m2m_pcm->test_dev->substream = pcm_file->substream;
	asrc_m2m_pcm->test_dev->runtime = pcm_file->substream->runtime;

	tcc_test_dev_default_hw_params(asrc_m2m_pcm->test_dev);
#endif

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

	asrc_m2m_pcm_dbg("[%s][%d]end\n", __func__, __LINE__);
    return 0;
}

static int tcc_asrc_m2m_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);

	asrc_m2m_pcm_dbg("%s\n", __func__);
#ifdef TEST_BY_ALSA
	//Buffer info reset.
	if(asrc_m2m_pcm->test_dev->filp) {
		filp_close(asrc_m2m_pcm->test_dev->filp, NULL);
	}
#endif
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
	asrc_m2m_pcm->middle->buffer_bytes = 0;

	asrc_m2m_pcm->interval = 0;
	asrc_m2m_pcm->Bwrote = 0;

	if (asrc_m2m_pcm->asrc_substream != NULL) { 
		asrc_m2m_pcm->asrc_substream = NULL;
	}
/*
	if (asrc_m2m_pcm->kth_id_ptr_check != NULL) { 
		asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
		kthread_stop(asrc_m2m_pcm->kth_id_ptr_check);
		asrc_m2m_pcm->kth_id_ptr_check = NULL;
	}

	if (asrc_m2m_pcm->kth_id_ptr_update != NULL) { 
		asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
		kthread_stop(asrc_m2m_pcm->kth_id_ptr_update);
		asrc_m2m_pcm->kth_id_ptr_update = NULL;
	}
*/
	asrc_m2m_pcm_dbg("[%s][%d]end\n", __func__, __LINE__);
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

	asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
	asrc_m2m_pcm_dbg("format : 0x%08x\n", format);
	asrc_m2m_pcm_dbg("channels : %d\n", channels);
	asrc_m2m_pcm_dbg("period_bytes : %lu\n", period_bytes);
	asrc_m2m_pcm_dbg("buffer_bytes : %lu\n", buffer_bytes);

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

#ifdef TEST_BY_ALSA
	asrc_m2m_pcm->test_dev->access = SNDRV_PCM_ACCESS_RW_INTERLEAVED;
	asrc_m2m_pcm->test_dev->format = format;
	asrc_m2m_pcm->test_dev->channels = channels;
	asrc_m2m_pcm->test_dev->rate = ASRC_DST_RATE;

	tcc_test_dev_default_hw_params(asrc_m2m_pcm->test_dev);
#endif
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 

		asrc_m2m_pcm->src->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->src->period_bytes = period_bytes;
		asrc_m2m_pcm->src->rate = rate;
		asrc_m2m_pcm->interval = TCC_APP_PTR_CHECK_INTERVAL_TX;

		asrc_m2m_pcm->dst->rate = ASRC_DST_RATE;
#ifdef TEST_BY_ALSA
		asrc_m2m_pcm->dst->buffer_bytes = frames_to_bytes(asrc_m2m_pcm->test_dev->runtime, asrc_m2m_pcm->test_dev->runtime->buffer_size);
		asrc_m2m_pcm->dst->period_bytes = frames_to_bytes(asrc_m2m_pcm->test_dev->runtime, asrc_m2m_pcm->test_dev->runtime->period_size);
#else
		asrc_m2m_pcm->dst->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->dst->period_bytes = PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->dst->period_bytes = MIN_PERIOD_BYTES;
#endif

		asrc_m2m_pcm->middle->buffer_bytes = asrc_m2m_pcm->dst->buffer_bytes * MID_BUFFER_CONST;
	} else {
		asrc_m2m_pcm->dst->buffer_bytes = buffer_bytes;
		asrc_m2m_pcm->dst->period_bytes = period_bytes;
		asrc_m2m_pcm->dst->rate = rate;
		asrc_m2m_pcm->interval = TCC_APP_PTR_CHECK_INTERVAL_RX;

		asrc_m2m_pcm->src->rate = ASRC_SRC_RATE;
#ifdef TEST_BY_ALSA
		asrc_m2m_pcm->src->buffer_bytes = frames_to_bytes(asrc_m2m_pcm->test_dev->runtime, asrc_m2m_pcm->test_dev->runtime->buffer_size);
		asrc_m2m_pcm->src->period_bytes = frames_to_bytes(asrc_m2m_pcm->test_dev->runtime, asrc_m2m_pcm->test_dev->runtime->period_size);
#else
		asrc_m2m_pcm->src->buffer_bytes = MAX_BUFFER_BYTES;
		//asrc_m2m_pcm->src->period_bytes = PLAYBACK_MAX_PERIOD_BYTES;
		asrc_m2m_pcm->src->period_bytes = MIN_PERIOD_BYTES;
#endif

		asrc_m2m_pcm->middle->buffer_bytes = asrc_m2m_pcm->dst->buffer_bytes * MID_BUFFER_CONST; 
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_asrc_m2m_pcm_set_hw_params_to_mbox(asrc_m2m_pcm);
#endif
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	asrc_m2m_pcm_dbg("[%s][%d]end\n", __func__, __LINE__);
	return 0;
}

static int tcc_asrc_m2m_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	int ret = -1;

	asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
	
	while(1) {
		if((asrc_m2m_pcm->is_asrc_running == false)
			&&(asrc_m2m_pcm->is_asrc_started == false)) {
			break;
		}
		printk("[%s][%d]id=%d\n", __func__, __LINE__, asrc_m2m_pcm->pair_id);
		msleep(1);
	}
	asrc_m2m_pcm->is_closed = true;
	asrc_m2m_pcm->is_asrc_started = false;
	asrc_m2m_pcm->is_asrc_running = false;
	wake_up_interruptible(&(asrc_m2m_pcm->check_wq));
	//wake_up_interruptible(&(asrc_m2m_pcm->update_wq));

	if(substream){
		snd_pcm_set_runtime_buffer(substream, NULL);
		ret=0;
	} else {
		asrc_m2m_pcm_dbg("%s-error\n", __func__);
	}

	return ret;
}

static int tcc_asrc_m2m_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
#ifdef TEST_BY_ALSA
     snd_pcm_sframes_t result;
#endif
	asrc_m2m_pcm_dbg("%s\n", __func__);

	while(1) {
		if((asrc_m2m_pcm->is_asrc_running == false)
			&&(asrc_m2m_pcm->is_asrc_started == false)) {
			break;
		}
		printk("[%s][%d]id=%d\n", __func__, __LINE__, asrc_m2m_pcm->pair_id);
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

	if(asrc_m2m_pcm->middle->buf != NULL)
		memset(asrc_m2m_pcm->middle->buf, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
	if(asrc_m2m_pcm->middle->dma_buf->area != NULL)
		memset(asrc_m2m_pcm->middle->dma_buf->area, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
	memset(runtime->dma_area, 0x00, runtime->dma_bytes);

#ifdef TEST_BY_ALSA
	if (asrc_m2m_pcm->test_dev->runtime->status->state != SNDRV_PCM_STATE_PREPARED) {
		result = snd_pcm_kernel_ioctl(asrc_m2m_pcm->test_dev->substream, SNDRV_PCM_IOCTL_PREPARE, NULL);
		if (result < 0) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Preparing sound card failed: %d\n", (int)result);
		}
	}
#endif
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
	int ret=-1;

	asrc_m2m_pcm_dbg("%s\n", __func__);

	if(asrc_m2m_pcm == NULL) {
		return -EFAULT;
	}

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				if(asrc_m2m_pcm->is_asrc_started == false) {
					asrc_m2m_pcm_dbg("ASRC_TRIGGER_START, PLAY\n");
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
					asrc_m2m_pcm->is_asrc_running = false;
					asrc_m2m_pcm->is_asrc_started = true;
					wake_up_interruptible(&(asrc_m2m_pcm->check_wq));
					//wake_up_interruptible(&(asrc_m2m_pcm->update_wq));
				} else {
					asrc_m2m_pcm_dbg("ASRC_TRIGGER_START already, PLAY\n");
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
				asrc_m2m_pcm_dbg("ASRC_TRIGGER_START end ret=%d, PLAY\n", ret);
			} else {
				asrc_m2m_pcm_dbg("ASRC_TRIGGER_START, CAPTURE\n");
				if(asrc_m2m_pcm->is_asrc_started == false) {
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
					asrc_m2m_pcm->is_asrc_running = false;
					asrc_m2m_pcm->is_asrc_started = true;
					wake_up_interruptible(&(asrc_m2m_pcm->check_wq));
				//	wake_up_interruptible(&(asrc_m2m_pcm->update_wq));
				} else {
					asrc_m2m_pcm_dbg("ASRC_TRIGGER_START already, CAPTURE\n");
				}

				asrc_m2m_pcm_dbg("ASRC_TRIGGER_START end ret=%d, CAPTURE\n", ret);
			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) { 
				asrc_m2m_pcm_dbg("ASRC_TRIGGER_STOP, PLAY\n");
#ifdef TEST_BY_ALSA
				snd_pcm_kernel_ioctl(asrc_m2m_pcm->test_dev->substream, SNDRV_PCM_IOCTL_PAUSE, NULL);
#endif
				if(asrc_m2m_pcm->src->rate != asrc_m2m_pcm->dst->rate) {
					while(asrc_m2m_pcm->is_asrc_running == true) {
						mdelay(1);
					}
					ret = tcc_asrc_m2m_stop(asrc_m2m_pcm->asrc, asrc_m2m_pcm->pair_id);
				} else {
					ret = 0;
				}
				asrc_m2m_pcm->is_asrc_started = false;
				wake_up_interruptible(&(asrc_m2m_pcm->check_wq));
				//wake_up_interruptible(&(asrc_m2m_pcm->update_wq));

				asrc_m2m_pcm_dbg("ASRC_TRIGGER_STOP ret=%d, PLAY\n", ret);
			} else {
				asrc_m2m_pcm_dbg("ASRC_TRIGGER_STOP, CAPTURE\n");
#ifdef TEST_BY_ALSA
				snd_pcm_kernel_ioctl(asrc_m2m_pcm->test_dev->substream, SNDRV_PCM_IOCTL_PAUSE, NULL);
#endif
				if(asrc_m2m_pcm->src->rate != asrc_m2m_pcm->dst->rate) {
					while(asrc_m2m_pcm->is_asrc_running == true) {
						mdelay(1);
					}
					ret = tcc_asrc_m2m_stop(asrc_m2m_pcm->asrc, asrc_m2m_pcm->pair_id);
				} else {
					ret = 0;
				}
				asrc_m2m_pcm->is_asrc_started = false;
				wake_up_interruptible(&(asrc_m2m_pcm->check_wq));
				//wake_up_interruptible(&(asrc_m2m_pcm->update_wq));
				asrc_m2m_pcm_dbg("ASRC_TRIGGER_STOP ret=%d, CAPTURE\n", ret);
			}

			asrc_m2m_pcm->is_a7s_started = false;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, cmd);
#endif
			break;
		default:
			return -EINVAL;
	}


	return ret;
}


static snd_pcm_uframes_t tcc_asrc_m2m_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	snd_pcm_uframes_t ret=0;
	unsigned int src_scale=0, temp=0;

	if(asrc_m2m_pcm == NULL) {
		asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
		return bytes_to_frames(runtime, 0);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if(asrc_m2m_pcm->src->rate == asrc_m2m_pcm->dst->rate) {
			temp = asrc_m2m_pcm->middle->pre_pos;
			ret = bytes_to_frames(runtime, temp);
			ret = ret % runtime->buffer_size;
		} else {
			src_scale = (asrc_m2m_pcm->src->rate * 1000) / asrc_m2m_pcm->dst->rate;
			temp = (asrc_m2m_pcm->middle->pre_pos * src_scale) / 1000;
			ret = bytes_to_frames(runtime, temp);
			ret = ret % runtime->buffer_size;
		}
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

static int tcc_hw_ptr_check_thread_for_capture(void *data)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	struct snd_pcm_substream *substream;// = asrc_m2m_pcm->asrc_substream;
	struct snd_soc_pcm_runtime *rtd;// = substream->private_data;
	unsigned int mid_buffer_bytes=0, max_read_sz=0;
	unsigned int read_pos=0;
	char *pout_buf;
#ifdef TEST_BY_ALSA
	unsigned int readable_sz=0;
	snd_pcm_uframes_t frames=0, ret=0;
	int err=0;
#endif

	while(!kthread_should_stop()) {

		wait_event_interruptible_timeout(asrc_m2m_pcm->check_wq, (asrc_m2m_pcm->is_asrc_started == true), msecs_to_jiffies(asrc_m2m_pcm->interval));

		if(asrc_m2m_pcm->is_asrc_started == true) {
			substream = asrc_m2m_pcm->asrc_substream;
			rtd = substream->private_data;

			read_pos = asrc_m2m_pcm->middle->cur_pos;
			//pout_buf = asrc_m2m_pcm->middle->buf;
			pout_buf = asrc_m2m_pcm->middle->dma_buf->area;
			mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;

			max_read_sz = MIN_PERIOD_BYTES * 4;
#ifdef TEST_BY_ALSA
rx_try_again:
			if (asrc_m2m_pcm->test_dev->runtime->status->state == SNDRV_PCM_STATE_XRUN ||
					asrc_m2m_pcm->test_dev->runtime->status->state == SNDRV_PCM_STATE_SUSPENDED) {
				err = snd_pcm_kernel_ioctl(asrc_m2m_pcm->test_dev->substream, SNDRV_PCM_IOCTL_PREPARE, NULL);
				if (err < 0) {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Preparing sound card failed: %d\n", (int)ret);
					//goto error_rx;
				}
			}
			if(read_pos + max_read_sz > mid_buffer_bytes) {
				//1st part read
				readable_sz = mid_buffer_bytes - read_pos;
				frames = bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, readable_sz);
				if(frames > 0) {
					ret = snd_pcm_lib_read(asrc_m2m_pcm->test_dev->substream, pout_buf+read_pos, frames);
					if ((int)ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Capture error: %d\n", (int)ret);
						goto rx_try_again;
					} else if(ret != frames) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Capture need some time : ret=%d, frames=%d\n", (int)ret, (int)frames);
						read_pos += bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, ret);
						goto next_interval;
					}
				}
				//2nd part read
				readable_sz = max_read_sz - readable_sz;
				frames = bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, readable_sz);
				if(frames > 0) {
					ret = snd_pcm_lib_read(asrc_m2m_pcm->test_dev->substream, pout_buf, frames);
					if ((int)ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Capture error: %d\n", (int)ret);
						goto rx_try_again;
					} else if(ret != frames) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Capture need some time : ret=%d, frames=%d\n", (int)ret, (int)frames);
						read_pos = bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, ret);
						goto next_interval;
					}
				}
				read_pos = readable_sz;
			} else {
				frames = bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, max_read_sz);
				if(frames > 0) {
					ret = snd_pcm_lib_read(asrc_m2m_pcm->test_dev->substream, pout_buf+read_pos, frames);
					if ((int)ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Capture error: %d\n", (int)ret);
						goto rx_try_again;
					} else if(ret != frames) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Capture need some time : ret=%d, frames=%d\n", (int)ret, (int)frames);
						read_pos += bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, ret);
						goto next_interval;
					}
					read_pos += max_read_sz;
				}
			}
next_interval:
#endif
			if(read_pos >= mid_buffer_bytes) {
				read_pos = read_pos - mid_buffer_bytes;
			}
			//overrun check!!
			if((asrc_m2m_pcm->middle->cur_pos > asrc_m2m_pcm->middle->pre_pos)
					&&(read_pos > asrc_m2m_pcm->middle->pre_pos)&&(read_pos < asrc_m2m_pcm->middle->cur_pos)) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%d, read_pos=%d, cur_pos=%d\n", (int)asrc_m2m_pcm->middle->pre_pos, (int)read_pos, (int)asrc_m2m_pcm->middle->cur_pos);

			} else if((asrc_m2m_pcm->middle->cur_pos < asrc_m2m_pcm->middle->pre_pos)
					&&((read_pos > asrc_m2m_pcm->middle->pre_pos)||(read_pos < asrc_m2m_pcm->middle->cur_pos))) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%d, read_pos=%d, cur_pos=%d\n", (int)asrc_m2m_pcm->middle->pre_pos, (int)read_pos, (int)asrc_m2m_pcm->middle->cur_pos);
			}

			//asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "pre_pos=%d, read_pos=%d, cur_pos=%d\n", (int)asrc_m2m_pcm->middle->pre_pos, (int)read_pos, (int)asrc_m2m_pcm->middle->cur_pos);
			asrc_m2m_pcm->middle->cur_pos = read_pos;
			//wake_up(&(asrc_m2m_pcm->update_wq));
			wake_up_interruptible(&(asrc_m2m_pcm->update_wq));
		} //start or closed?
	}//while
	return 0;
}
static int tcc_ptr_update_function_for_capture(struct snd_pcm_substream *psubstream, ssize_t readable_sz, ssize_t max_cpy_size)
{
	struct snd_pcm_substream *substream = psubstream;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc; 
	snd_pcm_uframes_t Ftemp=0;
	ssize_t dst_buffer_bytes=0, dst_period_bytes=0;
	ssize_t read_pos=0, write_pos=0;
	ssize_t writeable_sz=0, read_sz=0, Wtemp=0, Btemp=0;
	char *pin_buf;
	char *pout_buf;

	//pin_buf = asrc_m2m_pcm->middle->buf;
	pin_buf = asrc_m2m_pcm->middle->dma_buf->area;
	pout_buf = substream->dma_buffer.area;

	dst_buffer_bytes = asrc_m2m_pcm->dst->buffer_bytes;
	dst_period_bytes = asrc_m2m_pcm->dst->period_bytes;

	while(1) {
		if((readable_sz <= 0)||(asrc_m2m_pcm->is_closed == true)||(asrc_m2m_pcm->is_asrc_started == false)) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "readable_sz: %ld, closed=%d, started=%d\n", readable_sz, asrc_m2m_pcm->is_closed, asrc_m2m_pcm->is_asrc_started);
			break;//return 0;
		}

		if(readable_sz > max_cpy_size) {
			read_sz = max_cpy_size; 
		} else {
			read_sz = readable_sz; 
		}

		write_pos = frames_to_bytes(runtime, asrc_m2m_pcm->app->pre_pos);
		read_pos = asrc_m2m_pcm->middle->pre_pos;

		if(asrc_m2m_pcm->src->rate == asrc_m2m_pcm->dst->rate) {

			if(write_pos + read_sz >= dst_buffer_bytes) {
				//1st part copy for out_buf
				Btemp = dst_buffer_bytes - write_pos;
				memcpy(pout_buf+write_pos, pin_buf+read_pos, Btemp);
				read_pos = read_pos + Btemp;

				//2nd part copy for out_buf
				Btemp = read_sz - Btemp;
				memcpy(pout_buf, pin_buf+read_pos, Btemp);
			} else {
				memcpy(pout_buf+write_pos, pin_buf+read_pos, read_sz);
			}

			Ftemp = bytes_to_frames(runtime, read_sz);

			asrc_m2m_pcm->app->pre_pos += Ftemp;
			if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
				asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;

			asrc_m2m_pcm->app->pre_ptr += Ftemp;
			if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
				asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

			asrc_m2m_pcm->Bwrote += read_sz;
			if(asrc_m2m_pcm->Bwrote  >= dst_period_bytes) {
				asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % dst_period_bytes;
				snd_pcm_period_elapsed(substream);
			}

		} else {
			asrc_m2m_pcm->is_asrc_running = true;
			writeable_sz = tcc_asrc_m2m_push_data(asrc, asrc_m2m_pcm->pair_id, pin_buf+read_pos, read_sz);
			if(write_pos + writeable_sz >= dst_buffer_bytes) {
				//1st part copy for out_buf
				Btemp = dst_buffer_bytes - write_pos;
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf+write_pos, Btemp);
				Ftemp = bytes_to_frames(runtime, Wtemp);
				//1st part check ptr size for update
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

				//2nd part copy size
				Btemp = writeable_sz - Btemp;

				//2nd part copy for out_buf
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf, Btemp);

				Ftemp = bytes_to_frames(runtime, Wtemp);
				//2nd part check ptr size for update
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

			} else {
				//copy for out_buf
				Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf+write_pos, writeable_sz);

				//check ptr size for update
				Ftemp = bytes_to_frames(runtime, Wtemp);

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
			asrc_m2m_pcm->is_asrc_running = false;
		}
		asrc_m2m_pcm->middle->pre_pos += read_sz;
		if(asrc_m2m_pcm->middle->pre_pos >= asrc_m2m_pcm->middle->buffer_bytes) {
			asrc_m2m_pcm->middle->pre_pos = asrc_m2m_pcm->middle->pre_pos - asrc_m2m_pcm->middle->buffer_bytes;
		}
		readable_sz -= read_sz;
		if(readable_sz < 0) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "ERROR!! readable_sz: %ld\n", readable_sz);
			return -1;
		}
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "readable_sz: %ld, read_sz: %ld, pre_pos: %d\n", readable_sz, read_sz, asrc_m2m_pcm->middle->pre_pos);
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
	unsigned int mid_buffer_bytes=0, max_cpy_size=0;
	unsigned int cur_pos=0, pre_pos=0;
	int readable_sz=0;
	int ret=0;

	while(!kthread_should_stop()) {

		//wait_event(asrc_m2m_pcm->update_wq, (asrc_m2m_pcm->is_asrc_started == true));
		wait_event_interruptible(asrc_m2m_pcm->update_wq, (asrc_m2m_pcm->is_asrc_started == true));
	//	wait_event_interruptible_timeout(asrc_m2m_pcm->update_wq, (asrc_m2m_pcm->is_asrc_started == true), msecs_to_jiffies(asrc_m2m_pcm->interval));

		if((asrc_m2m_pcm->is_asrc_started == true)&&(asrc_m2m_pcm->middle->cur_pos != asrc_m2m_pcm->middle->pre_pos)) {
			substream = asrc_m2m_pcm->asrc_substream;
			rtd = substream->private_data;
			runtime = substream->runtime;

			cur_pos = asrc_m2m_pcm->middle->cur_pos;
			pre_pos = asrc_m2m_pcm->middle->pre_pos;
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, (int)cur_pos, (int)pre_pos);

			mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;

			temp = frames_to_bytes(runtime, runtime->period_size);
			if(temp > TCC_ASRC_MAX_SIZE) {
				max_cpy_size = TCC_ASRC_MAX_SIZE;
			} else {
				max_cpy_size = temp;
			}
			temp=0;

			if(cur_pos >= pre_pos) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, (int)cur_pos, (int)pre_pos);
				readable_sz = cur_pos - pre_pos;
				if(readable_sz > 0){
					ret = tcc_ptr_update_function_for_capture(substream, readable_sz, max_cpy_size);
					if(ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
					}
				}
			} else {			
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d\n", __func__, __LINE__, (int)cur_pos, (int)pre_pos);
				//1st part copy for in_buf
				readable_sz = mid_buffer_bytes - pre_pos;

				if(readable_sz > 0){
					ret = tcc_ptr_update_function_for_capture(substream, readable_sz, max_cpy_size);
					if(ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
					}
				}
				
				//2nd part copy
				readable_sz = cur_pos;
				//asrc_m2m_pcm->middle->pre_pos = 0;
				if(readable_sz > 0){
					ret = tcc_ptr_update_function_for_capture(substream, readable_sz, max_cpy_size);
					if(ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]ERROR!! ret: %d\n", __func__, __LINE__, ret);
					}
				}
			} //(cur_pos > pre_pos)?

		} //start or closed?
	}//while
	return 0;
}

static ssize_t tcc_appl_ptr_check_function_for_play(struct tcc_asrc_m2m_pcm *asrc_m2m_pcm, ssize_t readable_sz, ssize_t max_cpy_size, char *pin_buf)
{
    struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)asrc_m2m_pcm->asrc;
	unsigned int mid_buffer_bytes=0, write_pos=0, temp_pos=0;
	ssize_t read_sz=0, writeable_sz=0;
	ssize_t Btemp=0, Wtemp=0, ret=0;
	char *pout_buf;

	write_pos = asrc_m2m_pcm->middle->cur_pos;

	mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;

	//pout_buf = asrc_m2m_pcm->middle->buf;
	pout_buf = asrc_m2m_pcm->middle->dma_buf->area;

	if(readable_sz > max_cpy_size) {
		read_sz = max_cpy_size;
	} else {
		read_sz = readable_sz;
	}

	if(read_sz <= 0) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! readable_sz=%ld, max_cpy_size=%ld\n", __func__, __LINE__, readable_sz, max_cpy_size);
		return -1;
	}
	
	if(asrc_m2m_pcm->src->rate == asrc_m2m_pcm->dst->rate) {
		if(write_pos + read_sz >= mid_buffer_bytes) {
			//1st part copy for out_buf
			Btemp = mid_buffer_bytes - write_pos;
			memcpy(pout_buf+write_pos, pin_buf, Btemp);
			temp_pos = Btemp;
			//2nd part copy for out_buf
			Btemp = read_sz - Btemp;
			memcpy(pout_buf, pin_buf+temp_pos, Btemp);

			asrc_m2m_pcm->middle->cur_pos = Btemp;
		} else {
			memcpy(pout_buf+write_pos, pin_buf, read_sz);
			asrc_m2m_pcm->middle->cur_pos += read_sz; 
		}

		ret = read_sz;
	} else {
		asrc_m2m_pcm->is_asrc_running = true;
		writeable_sz = tcc_asrc_m2m_push_data(asrc, asrc_m2m_pcm->pair_id, pin_buf, read_sz);
		if(write_pos + writeable_sz >= mid_buffer_bytes) {
			//1st part copy for out_buf
			Btemp = mid_buffer_bytes - write_pos;
			Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf+write_pos, Btemp);
			if(Btemp != Wtemp) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! Btemp=%ld, Wtemp=%ld\n", __func__, __LINE__, Btemp, Wtemp);
				asrc_m2m_pcm->is_asrc_running = false;
				return -1;
			}

			//2nd part copy for out_buf
			Btemp = writeable_sz - Btemp;
			Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf, Btemp);

			if(Btemp != Wtemp) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! Btemp=%ld, Wtemp=%ld\n", __func__, __LINE__, Btemp, Wtemp);
				asrc_m2m_pcm->is_asrc_running = false;
				return -1;
			}

			asrc_m2m_pcm->middle->cur_pos = Wtemp;

		} else {
			Wtemp = tcc_asrc_m2m_pop_data(asrc, asrc_m2m_pcm->pair_id, pout_buf+write_pos, writeable_sz);
			asrc_m2m_pcm->middle->cur_pos += Wtemp;

		}
		ret = read_sz;
		asrc_m2m_pcm->is_asrc_running = false;
	}

	//overrun check!!
	if((write_pos > asrc_m2m_pcm->middle->pre_pos)
			&&(asrc_m2m_pcm->middle->cur_pos > asrc_m2m_pcm->middle->pre_pos)&&(write_pos > asrc_m2m_pcm->middle->cur_pos)) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%d, write_pos=%d, cur_pos=%d\n", (int)asrc_m2m_pcm->middle->pre_pos, (int)write_pos, (int)asrc_m2m_pcm->middle->cur_pos);

	} else if((write_pos < asrc_m2m_pcm->middle->pre_pos)
			&&((asrc_m2m_pcm->middle->cur_pos > asrc_m2m_pcm->middle->pre_pos)||(write_pos > asrc_m2m_pcm->middle->cur_pos))) {
		asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "overrun?! pre_pos=%d, write_pos=%d, cur_pos=%d\n", (int)asrc_m2m_pcm->middle->pre_pos, (int)write_pos, (int)asrc_m2m_pcm->middle->cur_pos);
	}
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
	ssize_t readable_sz=0, max_cpy_size=0, read_pos=0, Btemp=0, ret=0;
	char *pin_buf;
#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
	snd_pcm_uframes_t read_sz;
#endif

	while(!kthread_should_stop()) {

wait_check_play:
		wait_event_interruptible_timeout(asrc_m2m_pcm->check_wq, (asrc_m2m_pcm->is_asrc_started == true), msecs_to_jiffies(asrc_m2m_pcm->interval));

		if(asrc_m2m_pcm->is_asrc_started == true) {
			substream = asrc_m2m_pcm->asrc_substream;
			if(!substream) {
				printk("[%s][%d]id: %d, closed=%d, started=%d\n", __func__, __LINE__, asrc_m2m_pcm->pair_id, asrc_m2m_pcm->is_closed, asrc_m2m_pcm->is_asrc_started);
				goto wait_check_play;
			}
			rtd = substream->private_data;
			if(!rtd) {
				printk("[%s][%d]id: %d, closed=%d, started=%d\n", __func__, __LINE__, asrc_m2m_pcm->pair_id, asrc_m2m_pcm->is_closed, asrc_m2m_pcm->is_asrc_started);
				goto wait_check_play;
			}
			runtime = substream->runtime;
			if(!runtime) {
				printk("[%s][%d]id: %d, closed=%d, started=%d\n", __func__, __LINE__, asrc_m2m_pcm->pair_id, asrc_m2m_pcm->is_closed, asrc_m2m_pcm->is_asrc_started);
				goto wait_check_play;
			}

			cur_appl_ptr = runtime->control->appl_ptr;
			pre_appl_ptr = asrc_m2m_pcm->app->pre_ptr;

			if((cur_appl_ptr != pre_appl_ptr)){
				cur_appl_ofs = cur_appl_ptr % runtime->buffer_size;
				/*
				if((cur_appl_ofs == 0) && (cur_appl_ptr > 0)) {
					cur_appl_ofs = runtime->buffer_size;
				}*/
				pre_appl_ofs = asrc_m2m_pcm->app->pre_pos;

				Btemp = frames_to_bytes(runtime, runtime->period_size);
				if(Btemp > TCC_ASRC_MAX_SIZE) {
					max_cpy_size = TCC_ASRC_MAX_SIZE;
				} else {
					max_cpy_size = Btemp;
				}
				Btemp=0;

				pin_buf = substream->dma_buffer.area;

				if(cur_appl_ofs > pre_appl_ofs) {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d, cur_ptr=%d, pre_ptr=%d\n", __func__, __LINE__, (int)cur_appl_ofs, (int)pre_appl_ofs, (int)cur_appl_ptr, (int)pre_appl_ptr);
					Ftemp = cur_appl_ofs - pre_appl_ofs;
					readable_sz = frames_to_bytes(runtime, Ftemp);
					read_pos = frames_to_bytes(runtime, asrc_m2m_pcm->app->pre_pos);

					if(readable_sz > 0) {

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&start);
#endif

						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_sz, max_cpy_size, pin_buf+read_pos);
						if(ret < 0) {
							printk("[ASRC_M2M_PCM-%d][%s][%d] ERROR!! ret=%ld\n", asrc_m2m_pcm->pair_id, __func__, __LINE__, ret);
						}

#if	(CHECK_ASRC_M2M_ELAPSED_TIME == 1)
						do_gettimeofday(&end);

						elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
						do_div(elapsed_usecs64, NSEC_PER_USEC);
						elapsed_usecs = elapsed_usecs64;

						printk("asrc m2m elapsed time : %03d usec, %ldbytes\n", elapsed_usecs, ret);
#endif

						Ftemp = bytes_to_frames(runtime, ret);

						asrc_m2m_pcm->app->pre_pos += Ftemp;
						if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
							asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
						asrc_m2m_pcm->app->pre_ptr += Ftemp;
						if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
							asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

					}
				} else {
					asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d]cur=%d, pre=%d, cur_ptr=%d, pre_ptr=%d\n", __func__, __LINE__, (int)cur_appl_ofs, (int)pre_appl_ofs, (int)cur_appl_ptr, (int)pre_appl_ptr);

					//1st part copy for in_buf
					Ftemp = runtime->buffer_size - pre_appl_ofs;
					readable_sz = frames_to_bytes(runtime, Ftemp);
					read_pos = frames_to_bytes(runtime, asrc_m2m_pcm->app->pre_pos);

					if(readable_sz > 0) {
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_sz, max_cpy_size, pin_buf+read_pos);
						if(ret < 0) {
							printk("[ASRC_M2M_PCM-%d][%s][%d] ERROR!! ret=%ld\n", asrc_m2m_pcm->pair_id, __func__, __LINE__, ret);
						}

						Ftemp = bytes_to_frames(runtime, ret);

						asrc_m2m_pcm->app->pre_pos += Ftemp;
						if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
							asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
						asrc_m2m_pcm->app->pre_ptr += Ftemp;
						if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
							asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

					}

					if(readable_sz >= max_cpy_size) {
						goto max_cpy_tx;
					} else {
						readable_sz = max_cpy_size - readable_sz;
					}

					//2nd part copy for in_buf
					Ftemp = cur_appl_ofs;
					Btemp = frames_to_bytes(runtime, Ftemp); 
					if(readable_sz > Btemp) {
						readable_sz = Btemp;
					}

					if(readable_sz > 0) {
						ret = tcc_appl_ptr_check_function_for_play(asrc_m2m_pcm, readable_sz, max_cpy_size, pin_buf);
						if(ret < 0) {
							printk("[ASRC_M2M_PCM-%d][%s][%d] ERROR!! ret=%ld\n", asrc_m2m_pcm->pair_id, __func__, __LINE__, ret);
						}

						Ftemp = bytes_to_frames(runtime, ret);

						asrc_m2m_pcm->app->pre_pos = Ftemp;
						if(asrc_m2m_pcm->app->pre_pos >= runtime->buffer_size)
							asrc_m2m_pcm->app->pre_pos = asrc_m2m_pcm->app->pre_pos - runtime->buffer_size;
						asrc_m2m_pcm->app->pre_ptr += Ftemp;
						if(asrc_m2m_pcm->app->pre_ptr >= runtime->boundary)
							asrc_m2m_pcm->app->pre_ptr =asrc_m2m_pcm->app->pre_ptr - runtime->boundary;

					}
				} //(cur_appl_ofs > pre_appl_ofs)?
max_cpy_tx:
				//wake_up(&(asrc_m2m_pcm->update_wq));
				wake_up_interruptible(&(asrc_m2m_pcm->update_wq));

			} //(cur_appl_ptr != pre_appl_ptr)?
		} //start or closed?
	}//while
	return 0;
}

#ifdef TEST_BY_ALSA
static int tcc_ptr_update_function_for_play(struct snd_pcm_substream *substream, ssize_t writeable_sz, ssize_t max_write_sz) 
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int mid_buffer_bytes=0, src_period_bytes=0, write_pos=0;
	ssize_t write_sz=0;
	char *pout_buf;
	snd_pcm_sframes_t frames=0, ret=0;
	
	//pout_buf = asrc_m2m_pcm->middle->buf;
	pout_buf = asrc_m2m_pcm->middle->dma_buf->area;

	src_period_bytes = (asrc_m2m_pcm->dst->rate * 10000) / asrc_m2m_pcm->src->rate;
	src_period_bytes = (asrc_m2m_pcm->src->period_bytes * src_period_bytes) / 10000;

	mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;

	while(1) {
		if((writeable_sz <= 0)||(asrc_m2m_pcm->is_closed == true)||(asrc_m2m_pcm->is_asrc_started == false)) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "writeable_sz: %ld, closed=%d, started=%d\n", writeable_sz, asrc_m2m_pcm->is_closed, asrc_m2m_pcm->is_asrc_started);
			//return 0;
			break;
		}

		write_pos = asrc_m2m_pcm->middle->pre_pos;
		if(writeable_sz > max_write_sz) {
			write_sz = max_write_sz;
		} else {
			write_sz = writeable_sz;
		}
try_again:
		if (asrc_m2m_pcm->test_dev->runtime->status->state == SNDRV_PCM_STATE_XRUN ||
				asrc_m2m_pcm->test_dev->runtime->status->state == SNDRV_PCM_STATE_SUSPENDED) {
			ret = snd_pcm_kernel_ioctl(asrc_m2m_pcm->test_dev->substream, SNDRV_PCM_IOCTL_PREPARE, NULL);
			if ((int)ret < 0) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Preparing sound card failed: %d\n", (int)ret);
			}
		}

		frames = bytes_to_frames(asrc_m2m_pcm->test_dev->runtime, write_sz);
		write_sz = frames_to_bytes(asrc_m2m_pcm->test_dev->runtime, frames);

		ret = snd_pcm_lib_write(asrc_m2m_pcm->test_dev->substream, pout_buf+write_pos, frames);

		if (ret != frames) {
			asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "Playback error: %d\n", (int)ret);
			goto try_again;
		} else {
			write_pos += write_sz;
			if(write_pos >= mid_buffer_bytes) {
				write_pos = write_pos - mid_buffer_bytes;
			}
			asrc_m2m_pcm->middle->pre_pos = write_pos;
			
			asrc_m2m_pcm->Bwrote += write_sz;
			if(asrc_m2m_pcm->Bwrote >= src_period_bytes) {
				asrc_m2m_pcm->Bwrote = asrc_m2m_pcm->Bwrote % src_period_bytes;
				snd_pcm_period_elapsed(substream);
			}

			writeable_sz -= write_sz;
			if(writeable_sz < 0) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!!\n", __func__, __LINE__);
				return -1;
			}
		} //(ret != frames)?
	} //while
	return 0;
}
#endif

static int tcc_ptr_update_thread_for_play(void *data)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm *)data;
	struct snd_pcm_substream *substream;// = asrc_m2m_pcm->asrc_substream;
#ifdef TEST_BY_ALSA
	struct snd_soc_pcm_runtime *rtd;// = substream->private_data;
	unsigned int mid_buffer_bytes=0, cur_pos=0, pre_pos=0;
	ssize_t writeable_sz=0, max_write_sz=0;
#endif
	int ret=0;
	while(!kthread_should_stop()) {

	//	wait_event_interruptible_timeout(asrc_m2m_pcm->update_wq, (asrc_m2m_pcm->is_asrc_started == true), msecs_to_jiffies(asrc_m2m_pcm->interval));
		//wait_event(asrc_m2m_pcm->update_wq, (asrc_m2m_pcm->is_asrc_started == true));
		wait_event_interruptible(asrc_m2m_pcm->update_wq, (asrc_m2m_pcm->is_asrc_started == true));
		
#ifdef TEST_BY_ALSA
		if((asrc_m2m_pcm->is_asrc_started == true)
			&&(asrc_m2m_pcm->middle->cur_pos != asrc_m2m_pcm->middle->pre_pos)) {
			rtd = substream->private_data;

			cur_pos = asrc_m2m_pcm->middle->cur_pos;
			pre_pos = asrc_m2m_pcm->middle->pre_pos;

			mid_buffer_bytes = asrc_m2m_pcm->middle->buffer_bytes;

			max_write_sz = MIN_PERIOD_BYTES*2;

			if(cur_pos >= pre_pos) {
				writeable_sz = cur_pos - pre_pos;
				if (writeable_sz > 0) {
					ret = tcc_ptr_update_function_for_play(substream, writeable_sz, max_write_sz);
					if(ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
					}
				}

			} else {
				//1st_part
				writeable_sz = mid_buffer_bytes - pre_pos;
				if (writeable_sz > 0) {
					ret = tcc_ptr_update_function_for_play(substream, writeable_sz, max_write_sz);
					if(ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
					}
				}

				//2nd_part
				writeable_sz = cur_pos;
				if (writeable_sz > 0) {
					ret = tcc_ptr_update_function_for_play(substream, writeable_sz, max_write_sz);
					if(ret < 0) {
						asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
					}
				}
			}	//(cur_pos > pre_pos)?

#else	//TEST_BY_ALSA
		if((asrc_m2m_pcm->is_a7s_started == false)
			&&(asrc_m2m_pcm->is_asrc_started == true)
			&&(asrc_m2m_pcm->middle->cur_pos != asrc_m2m_pcm->middle->pre_pos)) {

			substream = asrc_m2m_pcm->asrc_substream;

			asrc_m2m_pcm->is_a7s_started = true;
#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
			ret = tcc_asrc_m2m_pcm_set_action_to_mbox(asrc_m2m_pcm->mbox_audio_dev, substream, SNDRV_PCM_TRIGGER_START);
			if(ret < 0) {
				asrc_m2m_pcm_dbg_id(asrc_m2m_pcm->pair_id, "[%s][%d] ERROR!! ret=%d\n", __func__, __LINE__, ret);
			}
#endif

#endif
		}	//start or closed?
	}	//(!kthread_should_stop())?
	return 0;
}


static int tcc_asrc_m2m_pcm_new(struct snd_soc_pcm_runtime *rtd) 
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm	= rtd->pcm;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
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
	asrc_m2m_pcm_dbg("[%s][%d]end\n", __func__, __LINE__);

	return 0;
}

static void tcc_asrc_m2m_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)snd_soc_platform_get_drvdata(rtd->platform);

	asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
	snd_pcm_lib_preallocate_free_for_all(pcm);
	snd_dma_free_pages(asrc_m2m_pcm->middle->dma_buf);

	asrc_m2m_pcm_dbg("[%s][%d]end\n", __func__, __LINE__);
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

	if(asrc_m2m_pcm->first_open == true) {
		if (asrc_m2m_pcm->kth_id_ptr_check != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->kth_id_ptr_check);
			asrc_m2m_pcm->kth_id_ptr_check = NULL;
		}

		if (asrc_m2m_pcm->kth_id_ptr_update != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->kth_id_ptr_update);
			asrc_m2m_pcm->kth_id_ptr_update = NULL;
		}
		asrc_m2m_pcm->first_open = false;
	}
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

#ifdef TEST_BY_ALSA
	struct tcc_snd_dev_for_test *test_dev;
#endif
	struct device_node *of_node_asrc;

	asrc_m2m_pcm_dbg("%s\n", __func__);

	asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_asrc_m2m_pcm), GFP_KERNEL);
	if(!asrc_m2m_pcm) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		return -ENOMEM;
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

	middle->buf = (unsigned char *)kzalloc(sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST, GFP_KERNEL);
	if(!middle->buf) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_middle_buf;
	} else {
		memset(middle->buf, 0, sizeof(unsigned char)*MAX_BUFFER_BYTES*MID_BUFFER_CONST);
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

#ifdef TEST_BY_ALSA
	test_dev = (struct tcc_snd_dev_for_test*)devm_kzalloc(&pdev->dev, sizeof(struct tcc_snd_dev_for_test), GFP_KERNEL);
	if(!test_dev) {
		asrc_m2m_pcm_dbg("[%s][%d] Error!!\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto error_test_dev;
	} else {
		memset(test_dev, 0, sizeof(struct tcc_snd_dev_for_test));
	}
#endif
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

#ifdef TEST_BY_ALSA
	asrc_m2m_pcm->test_dev = test_dev;
#endif

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
		asrc_m2m_pcm_dbg("[%s] ERROR!! mbox_audio_dev is NULL!\n", __func__);
		goto error_mailbox;
    }
#endif
	init_waitqueue_head(&asrc_m2m_pcm->check_wq);
	init_waitqueue_head(&asrc_m2m_pcm->update_wq);

	platform_set_drvdata(pdev, asrc_m2m_pcm);

	ret = snd_soc_register_platform(&pdev->dev, &tcc_asrc_m2m_pcm_platform);
	if (ret < 0) {
		printk("tcc_asrc_m2m_pcm_platform_register failed\n");
		goto error_of_node;
	}
	asrc_m2m_pcm_dbg("tcc_asrc_m2m_pcm_platform_register success\n");

	return ret;

error_mailbox:
error_of_node:
#ifdef TEST_BY_ALSA
	devm_kfree(&pdev->dev, asrc_m2m_pcm->test_dev);
error_test_dev:
#endif
	devm_kfree(&pdev->dev, asrc_m2m_pcm->app);
error_app:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->dst);
error_dst:
	devm_kfree(&pdev->dev, asrc_m2m_pcm->src);
error_src:
	kfree(asrc_m2m_pcm->middle->buf);
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

	return ret;
}

static int tcc_asrc_m2m_pcm_remove(struct platform_device *pdev)
{
	struct tcc_asrc_m2m_pcm *asrc_m2m_pcm = (struct tcc_asrc_m2m_pcm*)platform_get_drvdata(pdev);
	asrc_m2m_pcm_dbg("%s\n", __func__);

	if(asrc_m2m_pcm->first_open == true) {
		if (asrc_m2m_pcm->kth_id_ptr_check != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->kth_id_ptr_check);
			asrc_m2m_pcm->kth_id_ptr_check = NULL;
		}

		if (asrc_m2m_pcm->kth_id_ptr_update != NULL) { 
			asrc_m2m_pcm_dbg("[%s][%d]\n", __func__, __LINE__);
			kthread_stop(asrc_m2m_pcm->kth_id_ptr_update);
			asrc_m2m_pcm->kth_id_ptr_update = NULL;
		}
		asrc_m2m_pcm->first_open = false;
	}

#ifdef CONFIG_TCC_MULTI_MAILBOX_AUDIO
	tcc_mbox_audio_unregister_set_kernel_callback(asrc_m2m_pcm->mbox_audio_dev, asrc_m2m_pcm->mbox_cmd_type);
#endif
#ifdef TEST_BY_ALSA
	devm_kfree(&pdev->dev, asrc_m2m_pcm->test_dev);
#endif	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->app);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->dst);
	devm_kfree(&pdev->dev, asrc_m2m_pcm->src);
	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->middle);
	
	devm_kfree(&pdev->dev, asrc_m2m_pcm->asrc_m2m_param);
	kfree(asrc_m2m_pcm->middle->buf);
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
