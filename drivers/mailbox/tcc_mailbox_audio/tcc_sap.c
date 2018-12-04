/*
 * TCC Audio MailBox Device driver
 *
 * Copyright (C) 2018 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/completion.h>

#include "tcc_mailbox_cmd.h"
#include "tcc_mailbox.h"
#include "tcc_sap.h"
#include "tcc_sap_shared.h"

static struct class *msg_class;
static unsigned int msg_major;

struct tcc_sap_component_mem {
    dma_addr_t info_phys_addr;
    unsigned char *info_virt_addr;
    unsigned int info_mem_size;

    dma_addr_t out_phys_addr;
    unsigned char *out_virt_addr;
    unsigned int out_mem_size;

    dma_addr_t in_phys_addr;
    unsigned char *in_virt_addr;
    unsigned int in_mem_size;

    unsigned int ref;
    struct list_head elem;
};

struct tcc_ca7_buffer_status {
    unsigned int filled_len;
    unsigned int need_input;
    unsigned int need_output;
    unsigned int updated;
    int error;
};

struct tcc_sap_component {
    struct mbox_audio_device *device;
    unsigned int id;
    unsigned int created;
    unsigned int initialized;
    unsigned int mode;
    unsigned int secured_in;
    unsigned int secured_out;
    unsigned int updated;

    struct tcc_sap_component_mem *share_mem;

    struct ca7_mbox_instance *mbox_inst;
    struct tcc_ca7_buffer_status buffer_status;
};

struct mbox_audio_device {
    struct device *pdev;
    struct cdev cdev;
    void *mailbox;

    struct mutex lock;
};

#define TCC_SAP_DRV_VERSION "2.00"

//#define DEBUG_TEST_ENABLE	1

/*****************************************************************************
 * Log Message
 ******************************************************************************/
#if defined(DEBUG_TEST_ENABLE)
#define LOG_TAG    "[TCC_SAP for Debug]"

#define DBG_TEST_ADDR    0x2FE00000
#else
#define LOG_TAG    "[TCC_SAP]"
#endif

#define dprintk(msg...)                                \
{                                                      \
    printk(KERN_INFO LOG_TAG " (D) " msg);             \
}

#define eprintk(msg...)                                \
{                                                      \
    printk(KERN_INFO LOG_TAG " (E) " msg);             \
}

/*****************************************************************************
 * Defines
 ******************************************************************************/

#define DEV_NAME      "tcc_mbox_audio"

#define MAX_BUFFER    (500)

#define WDMA_SIZE     (188*2000)
#define RDMA_SIZE     (4*1024*1024)

#define USE_DMA_MEMORY
#define DEFAULT_INDEX	0
#define EXTRA_INDEX		1

#if 1//__SAP_AUDIO__
#define CMD_TIMEOUT	msecs_to_jiffies(10000)
#define MBOX_TIMEOUT	1000000
#endif

/*****************************************************************************
 * Variables
 ******************************************************************************/
#if 1//__SAP_AUDIO__
audio_data_queue_t audio_data_queue[AUD_IPC_MAX];
int audio_data_queue_init = 1;
#endif

static struct tcc_sap_component_mem comp_shared_mem[MAX_INSTANCE_NUM];
static LIST_HEAD(comp_mem_list);

/*****************************************************************************
 * External Functions
 ******************************************************************************/

/*****************************************************************************
 * Functions
 ******************************************************************************/

static void tcc_sap_alloc_component_buffers(struct device *dev, struct tcc_sap_component_mem *mem) {
    mem->info_mem_size = sizeof(tcc_sap_shared_buffer);
    mem->info_virt_addr = dma_alloc_writecombine(dev, mem->info_mem_size, &mem->info_phys_addr, GFP_KERNEL);

    mem->out_mem_size = 64 * 1024;
    mem->out_virt_addr = dma_alloc_writecombine(dev, mem->out_mem_size, &mem->out_phys_addr, GFP_KERNEL);

    mem->in_mem_size = 32 * 1024;
    mem->in_virt_addr = dma_alloc_writecombine(dev, mem->in_mem_size, &mem->in_phys_addr, GFP_KERNEL);
}

static void tcc_sap_free_component_buffers(struct device *dev, struct tcc_sap_component_mem *mem) {
    if (mem->info_virt_addr) {
        dma_free_writecombine(dev, mem->info_mem_size, mem->info_virt_addr, mem->info_phys_addr);
        mem->info_virt_addr = NULL;
        mem->info_phys_addr = 0;
    }

    if (mem->out_virt_addr) {
        dma_free_writecombine(dev, mem->out_mem_size, mem->out_virt_addr, mem->out_phys_addr);
        mem->out_virt_addr = NULL;
        mem->out_phys_addr = 0;
    }

    if (mem->in_virt_addr) {
        dma_free_writecombine(dev, mem->in_mem_size, mem->in_virt_addr, mem->in_phys_addr);
        mem->in_virt_addr = NULL;
        mem->in_phys_addr = 0;
    }
}

static int createInstance(struct tcc_sap_component *comp, SapInitParam *param, unsigned int codingType) {
    int err;
    MBOXCmdCreate *msg = (MBOXCmdCreate *)&comp->mbox_inst->cmd.params;
    MBOXReply *reply = (MBOXReply *)&comp->mbox_inst->reply.params;

    comp->mode = param->mOperationMode;
    if (param->mInportNum > 1 || param->mOutportNum > 1) {
        eprintk("[%s] multi-port is not unsupported\n", __func__);
        return ERROR_INVALID_MODE;
    }

    switch(comp->mode) {
        case FILTER_MODE:
            comp->secured_in = 0;
            comp->secured_out = 0;
            break;
        case FILTER_MODE_SECURE_IN:
            comp->secured_in = 1;
            comp->secured_out = 0;
            break;
        case FILTER_MODE_SECURE_IN_OUT:
            comp->secured_in = 1;
            comp->secured_out = 1;
            break;
        case FILTER_MODE_SECURE_OUT:
            comp->secured_in = 0;
            comp->secured_out = 1;
            break;

        default:
            eprintk("[%s] unsupported mode (%d)\n", __func__, comp->mode);
            return ERROR_INVALID_MODE;
    }

    //tcc_sap_alloc_component_buffers(&comp->device->dev, comp->share_mem);

    msg->mMode = comp->mode;
    msg->mInportNum = 1;
    msg->mOutportNum = 1;
    msg->mSharedBase = comp->share_mem->info_phys_addr;
    msg->mSharedSize = comp->share_mem->info_mem_size;
    msg->mCodingType = codingType;

#if defined(DEBUG_TEST_ENABLE)&&(DEBUG_TEST_ENABLE == 1)	//forTEST
    dprintk("1. [%s] shBase:0x%08X, shSize:0x%08X\n", __FUNCTION__, msg->mSharedBase, msg->mSharedSize);
    msg->mSharedBase = DBG_TEST_ADDR;
    dprintk("1. [%s] changed shBase:0x%08X\n", __FUNCTION__, msg->mSharedBase);
#endif

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_CREATE_INSTANCE, sizeof(MBOXCmdCreate), 1);
    if (err != 0) {
        eprintk("createInstance: mbox_inst error (%d), id (%d)\n", err, comp->id);
        return err;
    }

    if (reply->mError) {
        eprintk("createInstance: err (%d), id (%d)\n", reply->mError, comp->id);
        return reply->mError;
    }

    comp->created = 1;

    //printk("[%s#%d]\n", __func__, comp->id);

    return NO_ERROR;
}

static int initInstance(struct tcc_sap_component *comp, SapAudioSetInfoParam *srcParam) {
    MBOXCmdInit *msg = (MBOXCmdInit *)&comp->mbox_inst->cmd.params;
    MBOXReplyInit *reply = (MBOXReplyInit *)comp->mbox_inst->reply.params;
    tcc_sap_init_struct *info;
    void *extra;
    int err;

    // write params to shared memory
    //printk("init securein %d, secureout %d\n", comp->secured_in, comp->secured_out);
    info = (tcc_sap_init_struct *) comp->share_mem->info_virt_addr;

    memcpy(&info->mAudioCodecParams, &srcParam->mAudioCodecParams, sizeof(union_audio_codec_t));
    memcpy(&info->mStream, &srcParam->mStream, sizeof(SapAudioStreamInfoParam));
    memcpy(&info->mPcm, &srcParam->mPcm, sizeof(SapAudioPcmInfoParam));

    // malloc extra-config data memory
    if (srcParam->mExtra && srcParam->mExtraSize) {
        if (!comp->secured_in) {
            info->mExtra = comp->share_mem->in_phys_addr;
            extra = (void *) comp->share_mem->in_virt_addr;
            if (copy_from_user(extra, (const void *)srcParam->mExtra, srcParam->mExtraSize) != 0) {
                return ERROR_COPY_FROM_USER;
            }
        } else {
            // CSD data (Physical Address)
            info->mExtra = srcParam->mExtra; // check 32bit address
        }
        info->mExtraSize = srcParam->mExtraSize;
    } else {
        info->mExtra = 0;
        info->mExtraSize = 0;
    }

    msg->mConfigData = comp->share_mem->info_phys_addr;	// physical address of info
    msg->mConfigSize = sizeof(tcc_sap_init_struct);

#if defined(DEBUG_TEST_ENABLE)&&(DEBUG_TEST_ENABLE == 2)	//forTEST
    dprintk("2. [%s] shBase:0x%08X, shSize:0x%08X\n", __FUNCTION__, msg->mConfigData, msg->mConfigSize);
    msg->mConfigData = DBG_TEST_ADDR;
    dprintk("2. [%s] changed shBase:0x%08X\n", __FUNCTION__, msg->mConfigData);
#endif

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_INIT_INSTANCE, sizeof(MBOXCmdInit), 1);
    if (err) {
        printk("initInstance: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    //printk("[%s#%d], update: %d\n", __func__, comp->id, comp->updated);
    if (reply->mError) {
        printk("initInstance: err %d, id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    comp->updated = reply->mUpdate;
    comp->initialized = 1;

    comp->buffer_status.need_input = 1;
    comp->buffer_status.need_output = 1;
    comp->buffer_status.filled_len = 0;
    comp->buffer_status.updated = 0;

    return err;
}

#ifdef INCLUDE_VIDEO_CODEC
static int initVideoInstance(struct tcc_sap_component *comp, VP9_INIT_t *srcParam) {
    MBOXCmdInit *msg = (MBOXCmdInit *)&comp->mbox_inst->cmd.params;
    MBOXReplyInit *reply = (MBOXReplyInit *)comp->mbox_inst->reply.params;
    VP9_INIT_t *info;
    int err;

    // write params to shared memory
    info = (VP9_INIT_t *) comp->share_mem->info_virt_addr;
    memcpy(info, srcParam, sizeof(VP9_INIT_t));

    msg->mConfigData = comp->share_mem->info_phys_addr;	// physical address of info
    msg->mConfigSize = sizeof(VP9_INIT_t);

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_INIT_INSTANCE, sizeof(MBOXCmdInit), 1);
    if (err) {
        printk("initVideoInstance: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    //printk("[%s#%d], update: %d\n", __func__, comp->id, comp->updated);
    if (reply->mError) {
        printk("initVideoInstance: err %d id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    comp->updated = reply->mUpdate;
    comp->initialized = 1;

    srcParam->gsVp9DecHandle = info->gsVp9DecHandle;

    return err;
}
#endif

static int tcc_sap_process_buffers(struct tcc_sap_component *comp, SapProcessBuffer *param) {
    int err;
    MBOXCmdProcessBuffer *msg = (MBOXCmdProcessBuffer *)&comp->mbox_inst->cmd.params;
    MBOXReplyProcessBuffer *reply = (MBOXReplyProcessBuffer *)&comp->mbox_inst->reply.params;

    //printk("In tcc_sap_process_buffers %d\n", comp->mode);

    if (comp->buffer_status.need_input) {
        if (!comp->secured_in) {
            err = copy_from_user(comp->share_mem->in_virt_addr, (const void *)param->mInput[0].mData, param->mInput[0].mDataSize);
            if (err) {
                printk("copy input to shmem error(%d)\n", err);
                return ERROR_COPY_FROM_USER;
            }

            msg->mInBuffer = comp->share_mem->in_phys_addr;
            msg->mInSize = param->mInput[0].mDataSize;
            msg->mInBufferSize = comp->share_mem->in_mem_size;
            //printk("send in (%d) 0x%08X, %d\n", comp->id, msg->mInBuffer, msg->in_mem_size);
        } else {
            // stream data address (Physical Address)
            msg->mInBuffer = (unsigned int) param->mInput[0].mData; // check 32bit address
            msg->mInSize = param->mInput[0].mDataSize;
            if (msg->mInSize == 0 || msg->mInBuffer == 0) {
                // in-buffer can be null but don't send null buffer when "need_input" is true
                msg->mInBuffer = comp->share_mem->info_phys_addr; // Not actually used.
                msg->mInSize = 0;
            }
        }
    } else {
        msg->mInBuffer = 0;
        msg->mInSize = 0;
    }

    if (comp->buffer_status.need_output) {
        if (!comp->secured_out) {
            msg->mOutBuffer = comp->share_mem->out_phys_addr;
            msg->mOutSize = comp->share_mem->out_mem_size;
        } else {
            // pcm buffer address (Physical Address)
            msg->mOutBuffer = param->mOutput[0].mData; // check 32bit address
            msg->mOutSize = param->mOutput[0].mBufferSize;
        }
    } else {
        msg->mOutBuffer = 0;
        msg->mOutSize = 0;
    }

#if defined(DEBUG_TEST_ENABLE)&&(DEBUG_TEST_ENABLE == 3)	//forTEST
    dprintk("3. [%s] msg->pOut:0x%08x(size:%d)\n",
            __FUNCTION__, msg->mOutBuffer, msg->out_mem_size);
    msg->mOutBuffer = DBG_TEST_ADDR;
    dprintk("3. [%s] changed msg->pOut:0x%08x\n", __FUNCTION__, msg->mOutBuffer);
#endif

#if defined(DEBUG_TEST_ENABLE)&&(DEBUG_TEST_ENABLE == 4)	//forTEST
    dprintk("4. [%s] msg->pIn:0x%08x(size:%d)\n",
            __FUNCTION__, msg->mInBuffer, msg->in_mem_size);
    msg->mInBuffer = DBG_TEST_ADDR;
    dprintk("4. [%s] changed msg->pIn:0x%08x\n", __FUNCTION__, msg->mInBuffer);
#endif

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_PROCESS_BUFFER, sizeof(MBOXCmdProcessBuffer), 1);
    if (err != 0) {
        eprintk("tcc_sap_process_buffers: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    comp->buffer_status.filled_len = reply->mFilledLength;
    comp->buffer_status.need_input = reply->mNeedMoreInput;
    comp->buffer_status.need_output = reply->mFilledLength != 0;
    comp->buffer_status.updated = reply->mUpdate;
    comp->buffer_status.error = (int)reply->mError;

    if (reply->mError) {
        // decoding error;
        if (reply->mError != -13) {
            printk("tcc_sap_process_buffers: err %d id %d\n", reply->mError, comp->id);
        }
        //return reply->mError;
    }

    //dprintk("[%s#%d] %d\n", __func__, comp->id, err);

    return NO_ERROR;
}

static int tcc_sap_flush(struct tcc_sap_component *comp) {
    int err;
    MBOXReply *reply = (MBOXReply *)&comp->mbox_inst->reply.params;

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_FLUSH, 0, 1);
    if (err != 0) {
        eprintk("tcc_sap_flush: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    if (reply->mError) {
        eprintk("tcc_sap_flush: err %d, id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    comp->buffer_status.need_input = 1;
    comp->buffer_status.need_output = 1;
    comp->buffer_status.filled_len = 0;

    //dprintk("[%s#%d]\n", __func__, comp->id);

    return NO_ERROR;
}

static int tcc_sap_get_param(struct tcc_sap_component *comp, void *param, unsigned int size, unsigned int index) {
    MBOXCmdInfo *msg = (MBOXCmdInfo *)&comp->mbox_inst->cmd.params;
    MBOXReply *reply = (MBOXReply *)&comp->mbox_inst->reply.params;
    void *info;
    int err;

    //printk("In tcc_sap_get_drc %d\n", comp->mode);
    info = (void *)comp->share_mem->info_virt_addr;
    msg->mParamAddr = comp->share_mem->info_phys_addr;	// physical address of info
    msg->mParamSize = size;
    msg->mParamIndex = index;

#if defined(DEBUG_TEST_ENABLE)&&(DEBUG_TEST_ENABLE == 6)	//forTEST
    dprintk("6. [%s] msg->pAddr:0x%08x(size:%d)\n",
            __FUNCTION__, msg->mParamAddr, msg->mParamSize);
    msg->mParamAddr = DBG_TEST_ADDR;
    dprintk("6. [%s] changed msg->pAddr:0x%08x\n", __FUNCTION__, msg->mParamAddr);
#endif

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_GET_PARAM, sizeof(MBOXCmdInfo), 1);
    if (err != 0) {
        eprintk("tcc_sap_get_param: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    if (reply->mError) {
        eprintk("tcc_sap_get_param: err %d, id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    if (copy_to_user((char *) param, info, size) != 0) {
        printk( KERN_ERR "[tcc_sap_get_drc] Error copy info from kernel to user\n" );
        return ERROR_COPY_TO_USER;
    }

    //dprintk("[%s#%d]\n", __func__, comp->id);

    return NO_ERROR;
}

static int tcc_sap_set_param(struct tcc_sap_component *comp, void *param, unsigned int size, unsigned int index) {
    MBOXCmdInfo *msg = (MBOXCmdInfo *)&comp->mbox_inst->cmd.params;
    MBOXReply *reply = (MBOXReply *)&comp->mbox_inst->reply.params;
    void *to;
    int err;

    to = (void *) comp->share_mem->info_virt_addr;
    if (copy_from_user((void*)to, param, size) != 0) {
        return ERROR_COPY_FROM_USER;
    }

    msg->mParamAddr = comp->share_mem->info_phys_addr;	// physical address of info
    msg->mParamSize = size;
    msg->mParamIndex = index;

#if defined(DEBUG_TEST_ENABLE)&&(DEBUG_TEST_ENABLE == 7)	//forTEST
    dprintk("7. [%s] msg->pAddr:0x%08x(size:%d)\n",
            __FUNCTION__, msg->mParamAddr, msg->mParamSize);
    msg->mParamAddr = DBG_TEST_ADDR;
    dprintk("7. [%s] changed msg->pAddr:0x%08x\n", __FUNCTION__, msg->mParamAddr);
#endif

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_SET_PARAM, sizeof(MBOXCmdInfo), 1);
    if (err != 0) {
        eprintk("tcc_sap_set_param: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    if (reply->mError) {
        eprintk("tcc_sap_set_param: err %d, id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    //dprintk("[%s#%d]\n", __func__, comp->id);

    return NO_ERROR;
}

#ifdef INCLUDE_VIDEO_CODEC
static int tcc_sap_process_video_buffers(struct tcc_sap_component *comp, VP9_DECODE_t *param) {
    MBOXCmdInfo *msg = (MBOXCmdInfo *)&comp->mbox_inst->cmd.params;
    MBOXReply *reply = (MBOXReply *)&comp->mbox_inst->reply.params;
    VP9_DECODE_t *shared;
    int err;

    if (comp->mode != FILTER_MODE) {
        return ERROR_INVALID_MODE;
    }

    // write params to shared memory
    shared = (VP9_DECODE_t *)comp->share_mem->info_virt_addr;
    memcpy(shared, param, sizeof(VP9_DECODE_t));

    // set MailBox structure
    msg->mParamIndex = PARAM_INDEX_VPU_DEC_DECODE;
    msg->mParamAddr = comp->share_mem->info_phys_addr;	// physical address of shared
    msg->mParamSize = sizeof(VP9_DECODE_t);

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_SET_PARAM, sizeof(MBOXCmdInfo), 1);
    if (err != OK) {
        eprintk("tcc_sap_process_video_buffers: mbox_inst error %d, id %d\n", err, comp->id);
        return err;
    }

    if (reply->mError) {
        eprintk("tcc_sap_process_video_buffers: err %d, id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    // copy result to usr's output buffer
    if (memcpy((void*)&param->gsVp9DecOutput, comp->share_mem->out_virt_addr, sizeof(vp9_dec_output_t)) != NULL) {
        printk( KERN_ERR "Error copy pcm from kernel to user\n" );
        return ERROR_COPY_TO_USER;
    }

    return NO_ERROR;
}

static int send_structured_command(struct tcc_sap_component *comp, unsigned int cmd, void *srcParam, unsigned int paramSize) {
    MBOXCmdInfo *msg = (MBOXCmdInfo *)&comp->mbox_inst->cmd.params;
    MBOXReply *reply = (MBOXReply *)&comp->mbox_inst->reply.params;
    void *dest;
    int err;

    // write params to shared memory
    dest = (void *)comp->share_mem->info_virt_addr;
    memcpy(dest, srcParam, paramSize);

    msg->mParamIndex = cmd;
    msg->mParamAddr = comp->share_mem->info_phys_addr;	// physical address of dest
    msg->mParamSize = paramSize;

    err = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_SET_PARAM, sizeof(MBOXCmdInfo), 1);
    if (err != OK) {
        eprintk("send_structured_command: mbox_inst error %d, cmd %d, id %d\n", err, cmd, comp->id);
        return err;
    }

    if (reply->mError) {
        eprintk("send_structured_command: err %d, id %d\n", reply->mError, comp->id);
        return reply->mError;
    }

    return NO_ERROR;
}

static int get_structured_result(struct tcc_sap_component *comp, unsigned int cmd, void *destParam, unsigned int paramSize) {
    void *src;
    // write result to usr param
    src = (void *)comp->share_mem->info_virt_addr;

    if (copy_to_user((char *)destParam, src, paramSize) != 0) {
        printk( KERN_ERR "[get_structured_result] Error copy info from kernel to user\n" );
        return ERROR_COPY_TO_USER;
    }

    return NO_ERROR;
}

static int get_structured_video_result(struct tcc_sap_component *comp, unsigned int cmd, void *destParam, unsigned int paramSize) {
    void *src;
    // write result to usr param
    src = (void *)comp->share_mem->info_virt_addr;

    memcpy((char *)destParam, src, paramSize);

    return NO_ERROR;
}
#endif

static long tcc_sap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct tcc_sap_component *comp = filp->private_data;
    struct mbox_audio_device *sap_dev = comp->device;
    int ret = UNKNOWN_ERROR;

    //mutex_lock(&sap_dev->lock);

    switch (cmd)
    {
        case IOCTL_TCC_SAP_INIT:
        {
            SapInitParam param;

            if (copy_from_user((void*) &param, (void*) arg, sizeof(SapInitParam)) != OK) {
                ret = ERROR_COPY_FROM_USER;
                break;
            }

            if (!comp->created) {
                if (comp->share_mem == NULL) {
                    struct tcc_sap_component_mem *comp_mem = NULL;
                    mutex_lock(&sap_dev->lock);
                    if (!list_empty(&comp_mem_list)) {
                        comp_mem = list_first_entry(&comp_mem_list, struct tcc_sap_component_mem, elem);
                        list_del(&comp_mem->elem);
                        comp->share_mem = comp_mem;
                        //printk("get mem %d\n", comp_mem->ref);
                    }
                    mutex_unlock(&sap_dev->lock);
                }

                if (comp->share_mem == NULL) {
                    eprintk("[%s] Error too many instance\n", __func__);
                    ret = ERROR_TOO_MANY_ID;
                    break;
                } else {
                    ret = createInstance(comp, &param, param.mSetInfo.mStream.mCodingType);
                    if (ret != OK) {
                        ret = ERROR_CREATE_INSTANCE;
                        break;
                    }
                }
            }

            if (comp->created && !comp->initialized) {
                ret = initInstance(comp, &param.mSetInfo);
                if (ret != OK) {
                    break;
                }

                if (comp->updated) {
                    //SapAudioSetInfoParam *dest = (SapAudioSetInfoParam *)arg;
                    //if (copy_from_user((void*)&dest->mPcm, (void*)comp->mPcmInfo, sizeof(SapAudioPcmInfoParam)) != OK) {
                    //	ret = ERROR_COPY_FROM_USER;
                    //  break;
                    //}
                    comp->updated = 0;
                }
            }
            break;
        }

        case IOCTL_TCC_SAP_DEINIT:
        {
            break;
        }

        case IOCTL_TCC_SAP_GET_PCM_INFO:
        {
            if (comp->initialized != 1) {
                eprintk("[%s] Error Uninitialized instance (%d), cmd (%X)\n", __func__, comp->id, cmd);
                ret = ERROR_UNINITIALIZED_ID;
                break;
            }
            ret = tcc_sap_get_param(comp, (void *) arg, sizeof(SapAudioPcmInfoParam), PARAM_INDEX_PCM);
            break;
        }

        case IOCTL_TCC_SAP_FLUSH:
        {
            if (comp->initialized != 1) {
                eprintk("[%s] Error Uninitialized instance (%d), cmd (%X)\n", __func__, comp->id, cmd);
                ret = ERROR_UNINITIALIZED_ID;
                break;
            }
            ret = tcc_sap_flush(comp);
            break;
        }

        case IOCTL_TCC_SAP_GET_DRC_INFO:
        {
            if (comp->initialized != 1) {
                eprintk("[%s] Error Uninitialized instance (%d), cmd (%X)\n", __func__, comp->id, cmd);
                ret = ERROR_UNINITIALIZED_ID;
                break;
            }

            ret = tcc_sap_get_param(comp, (void *) arg, sizeof(SapDRCInfoParam), PARAM_INDEX_DRC);
            break;
        }

        case IOCTL_TCC_SAP_SET_DRC_INFO:
        {
            if (comp->initialized != 1) {
                eprintk("[%s] Error Uninitialized instance (%d), cmd (%X)\n", __func__, comp->id, cmd);
                ret = ERROR_UNINITIALIZED_ID;
                break;
            }

            ret = tcc_sap_set_param(comp, (void *) arg, sizeof(SapDRCInfoParam), PARAM_INDEX_DRC);
            break;
        }

        case IOCTL_TCC_MBOX_SEND:
        {
            ret = tcc_sap_send_command(comp->mbox_inst, CA7_MBOX_CMD_PREPARE, 0, 1);
            if (ret != OK) {
                eprintk("[%s #%d] Error IOCTL_TCC_MBOX_SEND(%d)\n", __func__, comp->id, ret);
                break;
            }

            break;
        }

        case IOCTL_TCC_SAP_PROCESS_BUFFER:
        {
            SapProcessBuffer param;
            struct tcc_ca7_buffer_status *buffers = &comp->buffer_status;

            if (comp->initialized != 1) {
                eprintk("[%s] Error Uninitialized instance (%d), cmd (%X)\n", __func__, comp->id, cmd);
                ret = ERROR_UNINITIALIZED_ID;
                break;
            }

            //dprintk("IOCTL_TCC_SAP_PROCESS_BUFFER start\n");

            if (copy_from_user((void*) &param, (void*) arg, sizeof(SapProcessBuffer)) != OK) {
                ret = ERROR_COPY_FROM_USER;
                break;
            }

            ret = tcc_sap_process_buffers(comp, &param);
            if (ret != OK) {
                break;
            }

            param.mOutput[0].mDataSize = buffers->filled_len;
            param.mInput[0].mBufferNeeded = buffers->need_input;

            if (buffers->filled_len) {
                param.mOutput[0].mBufferNeeded = 1;
                //dprintk("copy result pcm(0x%08X, %d) to user\n", buffers->mReturned[0], buffers->mLength[0]);
                if (!comp->secured_out) {
                    if (buffers->filled_len > comp->share_mem->out_mem_size) {
                        printk( KERN_ERR "Error! Output size exceeds buffer size.\n" );
                        ret = ERROR_OUTPUT_IS_TOO_BIG;
                        break;
                    }

                    if (copy_to_user((void *)param.mOutput[0].mData, comp->share_mem->out_virt_addr, buffers->filled_len) != OK) {
                        printk( KERN_ERR "Error copy pcm from kernel to user\n" );
                        ret = ERROR_COPY_TO_USER;
                        break;
                    }
                }
            } else {
                param.mOutput[0].mBufferNeeded = 0;
            }

            param.mUpdate = buffers->updated;
            param.mRet = buffers->error;

            //dprintk("copy result param to user\n");
            if (copy_to_user((char *) arg, &param, sizeof(SapProcessBuffer)) != OK) {
                printk( KERN_ERR "Error copy info from kernel to user\n" );
                ret = ERROR_COPY_TO_USER;
                break;
            }
            //dprintk("IOCTL_TCC_SAP_PROCESS_BUFFER end (%d)\n", ret);
            break;
        }
#ifdef INCLUDE_VIDEO_CODEC
        case IOCTL_TCC_SAP_VPU_DEC_INIT:
        {
            SapVideoInitParam param;
            void* test = 0;
            VP9_INIT_t* out;

            test = memcpy((void*)&param.mSetInfo, (void*)arg, sizeof(VP9_INIT_t));

            param.mCommon.mOperationMode = FILTER_MODE;
            param.mCommon.mDirectRead = 0;
            param.mCommon.mInportNum = 1;
            param.mCommon.mOutportNum = 1;

            param.mSetInfo.gsVp9DecInit.m_BitstreamBufAddr[1] = param.mSetInfo.gsVp9DecInit.m_BitstreamBufAddr[0];
            param.mSetInfo.gsVp9DecInit.m_EntropySaveBuffer[1] = param.mSetInfo.gsVp9DecInit.m_EntropySaveBuffer[0];

            if (!comp->created) {
                ret = createInstance(comp, &param.mCommon, COMP_TYPE_VP9_DEC);
                if (ret != OK) {
                    ret = ERROR_CREATE_INSTANCE;
                    break;
                }
            }

            if (!comp->initialized) {
                ret = initVideoInstance(comp, &param.mSetInfo);
                if (ret != OK) {
                    break;
                }
            }

            out = (VP9_INIT_t*)arg;
            out->gsVp9DecHandle = param.mSetInfo.gsVp9DecHandle;
            break;
        }

        case IOCTL_TCC_SAP_VPU_DEC_SEQ_HEADER:
        {
            VP9_SEQ_HEADER_t *param = (VP9_SEQ_HEADER_t *) arg;

            printk("[VP9 : SVP] seq size %d addr 0x%x\n", param->stream_size);

            ret = send_structured_command(comp, PARAM_INDEX_VPU_DEC_SEQ_HEADER, param, sizeof(VP9_SEQ_HEADER_t));
            if (ret != OK) {
                break;
            }

            get_structured_video_result(comp, PARAM_INDEX_VPU_DEC_SEQ_HEADER, (void *)arg, sizeof(VP9_SEQ_HEADER_t));
            break;
        }

        case IOCTL_TCC_SAP_VPU_DEC_REG_FRAME_BUFFER:
        {
            VP9_SET_BUFFER_t *param = (VP9_SET_BUFFER_t *)arg;

            ret = send_structured_command(comp, PARAM_INDEX_VPU_DEC_REG_FRAME_BUFFER, param, sizeof(VP9_SET_BUFFER_t));
            if (ret != OK) {
                break;
            }

            get_structured_video_result(comp, PARAM_INDEX_VPU_DEC_REG_FRAME_BUFFER, (void *)arg, sizeof(VP9_SET_BUFFER_t));
            break;
        }

        case IOCTL_TCC_SAP_VPU_DEC_DECODE:
        {
            VP9_DECODE_t *param = (VP9_DECODE_t *) arg;

            ret = send_structured_command(comp, PARAM_INDEX_VPU_DEC_DECODE, param, sizeof(VP9_DECODE_t));
            if (ret != OK) {
                break;
            }

            get_structured_video_result(comp, PARAM_INDEX_VPU_DEC_DECODE, (void *)arg, sizeof(VP9_DECODE_t));
            break;
        }

        case IOCTL_TCC_SAP_VPU_DEC_BUF_FLAG_CLEAR:
        {
            SapVideoBufferClearParam param;
            param.mIndex = *(int *)arg;
            printk("param.mIndex %d\n", param.mIndex);
            ret = send_structured_command(comp, PARAM_INDEX_VPU_DEC_BUF_FLAG_CLEAR, &param, sizeof(SapVideoBufferClearParam));
            if (ret != OK) {
                break;
            }
            break;
        }

        case IOCTL_TCC_SAP_VPU_DEC_FLUSH_OUTPUT:
        {
            VP9_DECODE_t *param = (VP9_DECODE_t *) arg;

            ret = send_structured_command(comp, PARAM_INDEX_VPU_DEC_FLUSH_OUTPUT, param, sizeof(VP9_DECODE_t));
            if (ret != OK) {
                break;
            }

            get_structured_video_result(comp, PARAM_INDEX_VPU_DEC_FLUSH_OUTPUT, (void *)arg, sizeof(VP9_DECODE_t));
            break;
        }

        case IOCTL_TCC_SAP_VPU_DEC_SWRESET:
        {
            ret = send_structured_command(comp, PARAM_INDEX_VPU_DEC_SWRESET, NULL, 0);
            if (ret != OK) {
                break;
            }

            break;
        }

        case IOCTL_TCC_SAP_VPU_CODEC_GET_VERSION:
        {
            VP9_GET_VERSION_t *param = (VP9_GET_VERSION_t *)arg;

            ret = send_structured_command(comp, PARAM_INDEX_VPU_CODEC_GET_VERSION, param, sizeof(VP9_GET_VERSION_t));
            if (ret != OK) {
                break;
            }

            get_structured_video_result(comp, PARAM_INDEX_VPU_CODEC_GET_VERSION, (void *)arg, sizeof(VP9_GET_VERSION_t));
            break;
        }
#endif
#if 1//__SAP_AUDIO__
        case IOCTL_TCC_SAP_AUDIO_CMD:
        {
            audio_data_queue_t *paudio_data_queue;
            cmdmsg cmdmsg;

            int ret_sub;

            //printk(">>>>>>>>> init audio data queue start \n");
            if (audio_data_queue_init) {
                /* init cmd queue */
                int aud_ipc_idx;
                for(aud_ipc_idx = 0; aud_ipc_idx < AUD_IPC_MAX; ++aud_ipc_idx)
                    init_waitqueue_head(&audio_data_queue[aud_ipc_idx].wait);
                audio_data_queue_init = 0;
            }
            //printk("<<<<<<<<<<  init audio data queue end");

            if (copy_from_user((void*) &cmdmsg, (void*) arg, sizeof(cmdmsg)) != OK) {
                ret = ERROR_COPY_FROM_USER;
            } else {
                if (cmdmsg.dev >= AUD_IPC_MAX) {
                    printk("ERROR dev is too big (%d) start \n", cmdmsg.dev);
                    ret = ERROR_OUTPUT_IS_TOO_BIG;
                    break;
                }

                paudio_data_queue = (audio_data_queue_t *)&audio_data_queue[cmdmsg.dev];
                paudio_data_queue->done = 0;

                //printk(">>>>>>>>> IOCTL_TCC_SAP_AUDIO_CMD(%d) start \n", cmdmsg.cmd);
                cmdmsg.cmdid = CA7_MBOX_CMD_AUDIO_DRV;

                if (cmdmsg.cmd == AUD_CMD_OPEN) {
                    /* check the error if alloced mem is released or not */
                    if (paudio_data_queue->txdmamem.vaddr != NULL) {
                        dma_free_writecombine(0, paudio_data_queue->txdmamem.size, paudio_data_queue->txdmamem.vaddr, paudio_data_queue->txdmamem.paddr);
                        paudio_data_queue->txdmamem.vaddr = NULL;
                        paudio_data_queue->txdmamem.paddr = 0;
                        paudio_data_queue->txdmamem.size = 0;
                    }

                    if (cmdmsg.param.openpar.type == 0) {//clear contents
                        paudio_data_queue->txdmamem.size = cmdmsg.param.openpar.buffer_bytes;
                        paudio_data_queue->txdmamem.vaddr = dma_alloc_writecombine(comp->device->pdev, paudio_data_queue->txdmamem.size, &paudio_data_queue->txdmamem.paddr, GFP_KERNEL);
                    } else {
                        paudio_data_queue->txdmamem.vaddr = NULL;
                        paudio_data_queue->txdmamem.paddr = 0;
                        paudio_data_queue->txdmamem.size = 0;
                    }
                } else if (cmdmsg.cmd == AUD_CMD_WRITE) {
                    if (paudio_data_queue->txdmamem.vaddr != NULL) { //clear contest
                        copy_from_user((void*)paudio_data_queue->txdmamem.vaddr, (void*)cmdmsg.param.writepar.pdata, cmdmsg.param.writepar.size);
                        //printk("write dst=0x%x, src=0x%x, size=%d\n", txdmamem.vaddr, cmdmsg.param.writepar.pdata, cmdmsg.param.writepar.size);
                        cmdmsg.param.writepar.pdata = paudio_data_queue->txdmamem.paddr;
                    } //else drm contents
                }

                ret_sub = tcc_sap_mbox_send(comp->mbox_inst->mbox, (unsigned int *)&cmdmsg, sizeof(cmdmsg));
                if (ret_sub != sizeof(cmdmsg)) {
                    printk("cmd[%d] ERROR Send CMD(%d) Error (%d) \n", 1, cmdmsg.cmd, ret);
                    ret = ERROR_MAILBOX_INIT;
                } else {
                    if (wait_event_timeout(paudio_data_queue->wait, paudio_data_queue->done == 1, CMD_TIMEOUT) == 0) {
                        printk("cmd[%d] ERROR Send CMD(%d) TIME OUT\n", 1, cmdmsg.cmd);
                        ret = ERROR_MAILBOX_TIMEOUT;
                    } else {
                        ret = paudio_data_queue->ret;
                        //printk("cmd[%d] Send Noti(%d) Done\n", 1, cmdmsg.cmd);
                    }
                }

                if ( (cmdmsg.cmd == AUD_CMD_CLOSE) ||
                    ((cmdmsg.cmd == AUD_CMD_OPEN) && (ret_sub < 0)) )
                {
                    if (paudio_data_queue->txdmamem.vaddr != NULL) {
                        dma_free_writecombine(comp->device->pdev, paudio_data_queue->txdmamem.size, paudio_data_queue->txdmamem.vaddr, paudio_data_queue->txdmamem.paddr);
                    }
                    paudio_data_queue->txdmamem.vaddr = NULL;
                    paudio_data_queue->txdmamem.paddr = 0;
                    paudio_data_queue->txdmamem.size = 0;
                }
                //printk("<<<<<<< IOCTL_TCC_SAP_AUDIO_CMD(%d) end \n", cmdmsg.cmd);
            }
        }
        break;
#endif
        default:
            ret = ERROR_UNDEFINED_COMMAND;
            break;
    }

    //mutex_unlock(&sap_dev->lock);

    return ret;
}

#ifdef CONFIG_COMPAT
static long tcc_sap_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    return tcc_sap_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static ssize_t tcc_sap_write(struct file *filp, const char __user *userbuf, size_t count, loff_t *ppos) {
    return 0;
}

static ssize_t tcc_sap_read(struct file *filp, char __user *userbuf, size_t count, loff_t *ppos) {
    return 0;
}

static unsigned int tcc_sap_poll(struct file *filp, struct poll_table_struct *wait) {
    return 0;
}

static int tcc_sap_open(struct inode *inode, struct file *filp) {
    struct mbox_audio_device *sap_dev = container_of(inode->i_cdev, struct mbox_audio_device, cdev);
    struct tcc_sap_component *comp;
    comp = kzalloc(sizeof(struct tcc_sap_component), GFP_KERNEL);
    if (comp == NULL) {
        return -1;
    }

    comp->mbox_inst = tcc_sap_get_new_id(sap_dev->mailbox);
    if (comp->mbox_inst) {
        unsigned int fw_ver_major, fw_ver_minor, extension;
        MBOXReplyGetNewId *newID = (MBOXReplyGetNewId *)&comp->mbox_inst->reply.params;
        comp->id = comp->mbox_inst->id;
        comp->mbox_inst = comp->mbox_inst;
        //Vmme
        fw_ver_major = newID->mFwVersion >> 12;
        fw_ver_minor = (newID->mFwVersion >> 4) & 0x00FF;
        extension = newID->mFwVersion & 0xF;
        printk("[sap] version drv: v%s, ca7: v%1d.%02d%1X\n", TCC_SAP_DRV_VERSION, fw_ver_major, fw_ver_minor, extension);
        printk("[sap] new-id:%d, tot-num: %d\n", comp->id, newID->mTotalID);
    } else {
        eprintk("[sap] Error: get new id\n");
        kfree(comp);
        return -1;
    }

    comp->device = sap_dev;
    filp->private_data = comp;

    return NO_ERROR;
}

static int tcc_sap_release(struct inode *inode, struct file *filp) {
    struct tcc_sap_component *comp = filp->private_data;
    //struct mbox_test_device *sap_dev = container_of(inode->i_cdev, struct mbox_audio_device, cdev);
    struct mbox_audio_device *sap_dev = comp->device;

    tcc_sap_remove_component(comp->mbox_inst);
    comp->mbox_inst = NULL;
    if (comp->share_mem) {
        mutex_lock(&sap_dev->lock);
        list_add_tail(&comp->share_mem->elem, &comp_mem_list);
        mutex_unlock(&sap_dev->lock);
        comp->share_mem = NULL;
    }

    kfree(comp);

    return NO_ERROR;
}

/*****************************************************************************
 * Module Init/Exit
 ******************************************************************************/
static const struct file_operations tcc_sap_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = tcc_sap_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = tcc_sap_compat_ioctl,
#endif
    .open = tcc_sap_open,
    .read = tcc_sap_read,
    .write = tcc_sap_write,
    .poll = tcc_sap_poll,
    .release = tcc_sap_release,
    .llseek	= generic_file_llseek,
};

int tcc_sap_init(struct mbox_audio_device *sap_dev) {
    int i;
    dprintk("[%s] ver %s\n", __FUNCTION__, TCC_SAP_DRV_VERSION);

    mutex_init(&sap_dev->lock);

    of_dma_configure(sap_dev->pdev, NULL);

    for (i = 0; i < MAX_INSTANCE_NUM; i++) {
        tcc_sap_alloc_component_buffers(sap_dev->pdev, &comp_shared_mem[i]);
        comp_shared_mem[i].ref = i;
        list_add_tail(&comp_shared_mem[i].elem, &comp_mem_list);
    }

    return NO_ERROR;
}

void tcc_sap_deinit(struct mbox_audio_device *sap_dev) {
    int i;
    struct tcc_sap_component_mem *mem;
    //tcc_sap_deinit_ca7();

    mutex_destroy(&sap_dev->lock);

    for (i = 0; i < MAX_INSTANCE_NUM; i++) {
        tcc_sap_free_component_buffers(sap_dev->pdev, &comp_shared_mem[i]);
    }

    list_for_each_entry(mem, &comp_mem_list, elem) {
        list_del(&mem->elem);
	}

    dprintk("[%s]\n", __FUNCTION__);
}

static int mbox_audio_create_cdev(struct device *parent, struct mbox_audio_device *sap_dev)
{
    int ret;
    dev_t devt;

    ret = alloc_chrdev_region(&devt, 0, 1, KBUILD_MODNAME);
    if (ret) {
        dev_err(parent, "%s: alloc_chrdev_region failed: %d\n", __func__, ret);
        goto err_alloc_chrdev;
    }

    msg_major = MAJOR(devt);
    msg_class = class_create(THIS_MODULE, KBUILD_MODNAME);
    if (IS_ERR(msg_class)) {
        ret = PTR_ERR(msg_class);
        dev_err(parent, "%s: class_create failed: %d\n", __func__, ret);
        goto err_class_create;
    }

    cdev_init(&sap_dev->cdev, &tcc_sap_fops);
    sap_dev->cdev.owner = THIS_MODULE;

    /* Add character device */
    ret = cdev_add(&sap_dev->cdev, devt, 1);
    if (ret) {
        dev_err(parent, "%s: cdev_add failed (%d)\n", __func__, ret);
        goto err_add_cdev;
    }

    /* Create a device node */
    sap_dev->pdev = device_create(msg_class, parent, devt, sap_dev, DEV_NAME);
    if (IS_ERR(sap_dev->pdev)) {
        ret = PTR_ERR(sap_dev->pdev);
        dev_err(parent, "%s: device_create failed: %d\n", __func__, ret);
        goto err_device_create;
    }

    tcc_sap_init(sap_dev);

    return 0;

err_device_create:
    cdev_del(&sap_dev->cdev);
err_add_cdev:
    class_destroy(msg_class);
err_class_create:
    unregister_chrdev_region(MKDEV(msg_major, 0), 1);
err_alloc_chrdev:

    return ret;
}

static int mbox_audio_probe(struct platform_device *pdev)
{
    struct mbox_audio_device *sap_dev;
    int ret;

    sap_dev = devm_kzalloc(&pdev->dev, sizeof(struct mbox_audio_device), GFP_KERNEL);
    if (!sap_dev) {
        return -ENOMEM;
    }

    if ((sap_dev->mailbox = tcc_sap_init_mbox(pdev)) == NULL) {
        return -EPROBE_DEFER;
    }

    sap_dev->pdev = &pdev->dev;

    platform_set_drvdata(pdev, sap_dev);

    ret = mbox_audio_create_cdev(&pdev->dev, sap_dev);
    if (ret) {
        goto err_create_cdev;
    }

    dev_info(&pdev->dev, "Successfully registered\n");

    return 0;

err_create_cdev:
    tcc_sap_close_mbox(sap_dev->mailbox);

    return ret;
}

static int mbox_audio_remove(struct platform_device *pdev)
{
    struct mbox_audio_device *sap_dev = platform_get_drvdata(pdev);

    tcc_sap_deinit(sap_dev);

    cdev_del(&sap_dev->cdev);
    class_destroy(msg_class);
    unregister_chrdev_region(MKDEV(msg_major, 0), 1);

    tcc_sap_close_mbox(sap_dev->mailbox);

    return 0;
}

static const struct of_device_id mbox_audio_match[] = {
    { .compatible = "telechips,mailbox-audio" },
    {},
};

static struct platform_driver mbox_audio_driver = {
    .driver = {
        .name = "mailbox-audio",
        .of_match_table = mbox_audio_match,
    },
    .probe  = mbox_audio_probe,
    .remove = mbox_audio_remove,
};

module_platform_driver(mbox_audio_driver);

MODULE_DESCRIPTION("Telechips Audio MailBox Client");
MODULE_AUTHOR("jrkim@telechips.com");
MODULE_LICENSE("GPL v2");
