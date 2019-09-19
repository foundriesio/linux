/*
 *   FileName : vpu_mgr.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "vpu_mgr_sys.h"
#include <linux/of.h>
#include <linux/io.h>
#include "vpu_buffer.h"
#include "vpu_devices.h"
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <soc/tcc/pmap.h>

#define dprintk(msg...) //printk( "TCC_VPU_MGR: " msg);
#define detailk(msg...) //printk( "TCC_VPU_MGR: " msg);
#define cmdk(msg...) //printk( "TCC_VPU_MGR [Cmd]: " msg);
#define err(msg...) printk("TCC_VPU_MGR [Err]: "msg);
#define wprintk(msg...) //printk("TCC_VPU_MGR [Wait]: "msg);

#define VPU_REGISTER_DUMP
//#define VPU_REGISTER_INIT
#define USE_WAIT_LIST // for interlaced format with fileplay-mode or all format with ring-mode

//#define FORCED_ERROR //For test purpose!!
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

/////////////////////////////////////////////////////////////////////////////
// Control only once!!
static mgr_data_t vmgr_data;
static struct task_struct *kidle_task = NULL;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
extern void do_gettimeofday(struct timeval *tv);
#endif
extern int tcc_vpu_dec( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
//extern codec_result_t tcc_vpu_dec_esc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
//extern codec_result_t tcc_vpu_dec_ext( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
extern int tcc_vpu_enc( int Op, codec_handle_t* pHandle, void* pParam1, void* pParam2 );
#endif

#ifdef USE_WAIT_LIST
typedef struct wait_list_entry{
    int wait_dec_status;
    unsigned int type;  //encode or decode ?
    int cmd_type;       //vpu command
    long vpu_handle;
}wait_list_entry_t;

static wait_list_entry_t wait_entry_info;
static unsigned int use_wait_list = 0;
#define IsUseWaitList() (unsigned int)use_wait_list
#endif

VpuList_t* vmgr_list_manager(VpuList_t* args, unsigned int cmd);
/////////////////////////////////////////////////////////////////////////////

int vmgr_opened(void)
{
    if(vmgr_data.dev_opened == 0)
        return 0;
    return 1;
}
EXPORT_SYMBOL(vmgr_opened);

int vmgr_get_close(vputype type)
{
    return vmgr_data.closed[type];
}

int vmgr_get_alive(void)
{
    return vmgr_data.dev_opened;
}

int vmgr_set_close(vputype type, int value, int bfreemem)
{
    if( vmgr_get_close(type) == value ){
        dprintk(" %d was already set into %d. \n", type, value);
        return -1;
    }

    vmgr_data.closed[type] = value;
    if(value == 1){
        vmgr_data.handle[type] = 0x00;

        if(bfreemem)
            vmem_proc_free_memory(type);
    }

    return 0;
}

#ifdef USE_WAIT_LIST
void vmgr_set_fileplay_mode(vputype type, int filemode)
{
    vmgr_data.fileplay_mode[type] = filemode;
}

int vmgr_get_fileplay_mode(vputype type)
{
    return vmgr_data.fileplay_mode[type];
}
#endif

static void _vmgr_close_all(int bfreemem)
{
    vmgr_set_close(VPU_DEC, 1, bfreemem);
    vmgr_set_close(VPU_DEC_EXT, 1, bfreemem);
    vmgr_set_close(VPU_DEC_EXT2, 1, bfreemem);
    vmgr_set_close(VPU_DEC_EXT3, 1, bfreemem);
    vmgr_set_close(VPU_DEC_EXT4, 1, bfreemem);

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    vmgr_set_close(VPU_ENC, 1, bfreemem);
#endif
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    vmgr_set_close(VPU_ENC_EXT, 1, bfreemem);
#endif
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    vmgr_set_close(VPU_ENC_EXT2, 1, bfreemem);
#endif
#if defined(CONFIG_VENC_CNT_4)
    vmgr_set_close(VPU_ENC_EXT3, 1, bfreemem);
#endif
}

int vmgr_process_ex(VpuList_t *cmd_list, vputype type, int Op, int *result)
{
    if (vmgr_data.dev_opened == 0) {
        return 0;
    }

    printk(" \n process_ex %d - 0x%x \n\n", type, Op);

    if (!vmgr_get_close(type)) {
        cmd_list->type          = type;
        cmd_list->cmd_type      = Op;
        cmd_list->handle        = vmgr_data.handle[type];
        cmd_list->args          = NULL;
        cmd_list->comm_data     = NULL;
        cmd_list->vpu_result    = result;
        vmgr_list_manager(cmd_list, LIST_ADD);

        return 1;
    }

    return 1;
}

static int _vmgr_internal_handler(void)
{
    int ret, ret_code = RETCODE_SUCCESS;
    int timeout = 200;

    if (vmgr_data.check_interrupt_detection) {
        if (vmgr_data.oper_intr > 0) {
            detailk("Success 1: vpu operation!! \n");
            ret_code = RETCODE_SUCCESS;
        } else {
            ret = wait_event_interruptible_timeout(vmgr_data.oper_wq, vmgr_data.oper_intr > 0, msecs_to_jiffies(timeout));

            if (vmgr_is_loadable() > 0) {
                ret_code = RETCODE_CODEC_EXIT;
            }
            else if (vmgr_data.oper_intr > 0)
            {
                detailk("Success 2: vpu operation!! \n");
            #if defined(FORCED_ERROR)
                if (forced_error_count-- <= 0)
                {
                    ret_code = RETCODE_CODEC_EXIT;
                    forced_error_count = FORCED_ERR_CNT;
                    vetc_dump_reg_all(vmgr_data.base_addr, "_vmgr_internal_handler force-timed_out");
                }
                else
            #endif
                ret_code = RETCODE_SUCCESS;
            }
            else
            {
                err("[CMD 0x%x][%d]: vpu timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d\n",
                        vmgr_data.current_cmd, ret, timeout, vmgr_data.oper_intr, vmgr_data.nDecode_Cmd, vmgr_data.szFrame_Len);

                vetc_dump_reg_all(vmgr_data.base_addr, "_vmgr_internal_handler timed_out");
                ret_code = RETCODE_CODEC_EXIT;
            }
        }

        vmgr_data.oper_intr = 0;
        vmgr_status_clear(vmgr_data.base_addr);
    }

    return ret_code;
}

static int _vmgr_process(vputype type, int cmd, long pHandle, void* args)
{
    int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
    struct timeval t1, t2;
    int time_gap_ms = 0;
#endif

    vmgr_data.check_interrupt_detection = 0;
    vmgr_data.current_cmd = cmd;

    if (type < VPU_ENC) {
        if (cmd != VPU_DEC_INIT && cmd != VPU_DEC_INIT_KERNEL) {
            if (vmgr_get_close(type) || vmgr_data.handle[type] == 0x00) {
                return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
            }
        }

        if (cmd != VPU_DEC_BUF_FLAG_CLEAR && cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL &&
            cmd != VPU_DEC_DECODE && cmd != VPU_DEC_DECODE_KERNEL)
        {
            cmdk("%s: Decoder(%d), command: 0x%x \n", __func__, type, cmd);
        }

        switch (cmd)
        {
            case VPU_DEC_INIT: case VPU_DEC_INIT_KERNEL:
            {
                VDEC_INIT_t* arg;

                arg = (VDEC_INIT_t *)args;
                vmgr_data.handle[type] = 0x00;

                arg->gsVpuDecInit.m_RegBaseVirtualAddr = (codec_addr_t)vmgr_data.base_addr;
                arg->gsVpuDecInit.m_Memcpy    = (void* (*) ( void*, const void*, unsigned int, unsigned int ))vetc_memcpy;
                arg->gsVpuDecInit.m_Memset    = (void  (*) ( void*, int, unsigned int, unsigned int ))vetc_memset;
                arg->gsVpuDecInit.m_Interrupt = (int  (*) ( void ))_vmgr_internal_handler;
                arg->gsVpuDecInit.m_Ioremap   = (void* (*) ( phys_addr_t, unsigned int ))vetc_ioremap;
                arg->gsVpuDecInit.m_Iounmap   = (void  (*) ( void* ))vetc_iounmap;
                arg->gsVpuDecInit.m_reg_read  = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
                arg->gsVpuDecInit.m_reg_write = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;

            #ifdef USE_WAIT_LIST
                vmgr_set_fileplay_mode(type, arg->gsVpuDecInit.m_iFilePlayEnable);
            #endif

                vmgr_data.bDiminishInputCopy = (arg->gsVpuDecInit.m_uiDecOptFlags & (1 << 26)) ? true : false;

                //vpu_optee_fw_read(TYPE_VPU_D6);
                dprintk("@@ Dec-%d: Init In => workbuff 0x%x/0x%x, Reg: 0x%x, format : %d,"
                        " Stream(0x%x/0x%x, %d), Res: %d x %d \n", type,
                            arg->gsVpuDecInit.m_BitWorkAddr[PA], arg->gsVpuDecInit.m_BitWorkAddr[VA],
                            arg->gsVpuDecInit.m_RegBaseVirtualAddr,
                            arg->gsVpuDecInit.m_iBitstreamFormat,
                            arg->gsVpuDecInit.m_BitstreamBufAddr[PA], arg->gsVpuDecInit.m_BitstreamBufAddr[VA],
                            arg->gsVpuDecInit.m_iBitstreamBufSize,
                            arg->gsVpuDecInit.m_iPicWidth, arg->gsVpuDecInit.m_iPicHeight);

                dprintk("@@ Dec-%d: Init In => optFlag 0x%x, avcBuff: 0x%x- %d, Userdata(%d), "
                       "Inter: %d, PlayEn: %d, MaxRes: %d, disminished Copy(%d) \n", type,
                            arg->gsVpuDecInit.m_uiDecOptFlags,
                            arg->gsVpuDecInit.m_pSpsPpsSaveBuffer, arg->gsVpuDecInit.m_iSpsPpsSaveBufferSize,
                            arg->gsVpuDecInit.m_bEnableUserData, arg->gsVpuDecInit.m_bCbCrInterleaveMode,
                            arg->gsVpuDecInit.m_iFilePlayEnable, arg->gsVpuDecInit.m_iMaxResolution,
                            vmgr_data.bDiminishInputCopy);

                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (void *)&arg->gsVpuDecHandle, (void *)&arg->gsVpuDecInit, (void *)NULL);
                if (ret != RETCODE_SUCCESS) {
                    printk("@@ Dec-%d :: Init Done with ret(0x%x)\n", type, ret);
                    if (ret != RETCODE_CODEC_EXIT) {
                        vetc_dump_reg_all(vmgr_data.base_addr, "Init error");
                    }
                }
                if (ret != RETCODE_CODEC_EXIT && arg->gsVpuDecHandle != 0) {
                    vmgr_data.handle[type] = arg->gsVpuDecHandle;
                    vmgr_set_close(type, 0, 0);
                    cmdk("@@ Dec-%d :: handle = 0x%x \n", type, arg->gsVpuDecHandle);
                } else {
                    //To free memory!!
                    vmgr_set_close(type, 0, 0);
                    vmgr_set_close(type, 1, 1);
                }
                dprintk("@@ Dec-%d :: Init Done Handle(0x%x) \n", type, arg->gsVpuDecHandle);

            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                vmgr_data.iTime[type].print_out_index = vmgr_data.iTime[type].proc_base_cnt = 0;
                vmgr_data.iTime[type].accumulated_proc_time = vmgr_data.iTime[type].accumulated_frame_cnt = 0;
                vmgr_data.iTime[type].proc_time_30frames = 0;
            #endif
            }
            break;

            case VPU_DEC_SEQ_HEADER:
            case VPU_DEC_SEQ_HEADER_KERNEL:
            {
                void* arg = args;
                int iSize;

                dec_initial_info_t *gsVpuDecInitialInfo = vmgr_data.bDiminishInputCopy
                    ? &((VDEC_DECODE_t  *)arg)->gsVpuDecInitialInfo
                    : &((VDEC_SEQ_HEADER_t *)arg)->gsVpuDecInitialInfo;


                vmgr_data.szFrame_Len = iSize = vmgr_data.bDiminishInputCopy
                    ? (int)((VDEC_DECODE_t  *)arg)->gsVpuDecInput.m_iBitstreamDataSize
                    : (int)((VDEC_SEQ_HEADER_t *)arg)->stream_size;

                vmgr_data.check_interrupt_detection = 1;
                vmgr_data.nDecode_Cmd = 0;

                dprintk("@@ Dec-%d: VPU_DEC_SEQ_HEADER in :: size(%d) \n", type, iSize);

                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL,
                        (codec_handle_t*)&pHandle,
                        (vmgr_data.bDiminishInputCopy
                             ? (void*)(&((VDEC_DECODE_t  *)arg)->gsVpuDecInput) : (void*)iSize),
                        (void*)gsVpuDecInitialInfo);

                dprintk("@@ Dec-%d: VPU_DEC_SEQ_HEADER out 0x%x res info. %d - %d - %d, %d - %d - %d \n", type, ret,
                        gsVpuDecInitialInfo->m_iPicWidth,
                        gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropLeft,
                        gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropRight,
                        gsVpuDecInitialInfo->m_iPicHeight,
                        gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropTop,
                        gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropBottom);
            }
            break;

            case VPU_DEC_REG_FRAME_BUFFER:
            case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
            {
                VDEC_SET_BUFFER_t *arg;

                arg = (VDEC_SET_BUFFER_t *)args;
                dprintk("@@ Dec-%d: VPU_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x \n", type,
                        arg->gsVpuDecBuffer.m_FrameBufferStartAddr[0], arg->gsVpuDecBuffer.m_FrameBufferStartAddr[1]);

                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsVpuDecBuffer), (void*)NULL);
                dprintk("@@ Dec-%d: VPU_DEC_REG_FRAME_BUFFER out \n", type);
            }
            break;

            case VPU_DEC_DECODE:
            case VPU_DEC_DECODE_KERNEL:
            {
                VDEC_DECODE_t *arg;
            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                do_gettimeofday( &t1 );
            #endif
                arg = (VDEC_DECODE_t *)args;

                vmgr_data.szFrame_Len = arg->gsVpuDecInput.m_iBitstreamDataSize;
                dprintk("@@ Dec-%d :: In => 0x%x - 0x%x, %d, 0x%x - 0x%x, %d, flag: %d / %d / %d  \n", type,
                        arg->gsVpuDecInput.m_BitstreamDataAddr[PA],
                        arg->gsVpuDecInput.m_BitstreamDataAddr[VA],
                        arg->gsVpuDecInput.m_iBitstreamDataSize,
                        arg->gsVpuDecInput.m_UserDataAddr[PA],
                        arg->gsVpuDecInput.m_UserDataAddr[VA],
                        arg->gsVpuDecInput.m_iUserDataBufferSize,
                        arg->gsVpuDecInput.m_iFrameSearchEnable,
                        arg->gsVpuDecInput.m_iSkipFrameMode,
                        arg->gsVpuDecInput.m_iSkipFrameNum);

                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsVpuDecInput), (void*)(&arg->gsVpuDecOutput));

                dprintk("@@ Dec-%d :: Out => %d - %d - %d, %d - %d - %d \n", type,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iWidth,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropLeft,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropRight,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iHeight,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropTop,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropBottom);

                dprintk("@@ Dec-%d :: Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d] \n", type, ret,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iPicType,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iDispOutIdx,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodedIdx,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iOutputStatus,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus);

                dprintk("@@ Dec-%d :: Out => %d :: 0x%x 0x%x 0x%x \n", type,
                        arg->gsVpuDecOutput.m_DecOutInfo.m_iDispOutIdx,
                        arg->gsVpuDecOutput.m_pDispOut[PA][0],
                        arg->gsVpuDecOutput.m_pDispOut[PA][1],
                        arg->gsVpuDecOutput.m_pDispOut[PA][2]);

                if(arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus == VPU_DEC_BUF_FULL){
                    err("[%d] Buffer full\n", type);
                    //vetc_dump_reg_all(vmgr_data.base_addr, "Buffer full");
                }
                vmgr_data.nDecode_Cmd++;

            #ifdef USE_WAIT_LIST
                if (IsUseWaitList()) {
                    int wait_condition = 0;

                    wprintk("[%d] check waiting :: play-mode[%d], ret[0x%x/%d], already-wait[%d] \n", type,
                            vmgr_get_fileplay_mode(type), ret, ret, wait_entry_info.wait_dec_status);

                    if (vmgr_get_fileplay_mode(type)) {
                        wait_condition = (arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus == VPU_DEC_SUCCESS_FIELD_PICTURE);
                    } else {
                        wait_condition = (ret == RETCODE_INSUFFICIENT_BITSTREAM);
                    }

                    if (wait_condition) {
                        wprintk("[%d] start waiting...\n", type);
                        wait_entry_info.wait_dec_status = 1;
                        wait_entry_info.type = type;
                        wait_entry_info.cmd_type = cmd;
                        wait_entry_info.vpu_handle = pHandle;
                    }
                }

                if (IsUseWaitList() && (arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus == VPU_DEC_SUCCESS) &&
                                       (wait_entry_info.wait_dec_status == 1))
                {
                    wait_entry_info.wait_dec_status = 0;
                    memset(&wait_entry_info, 0, sizeof(wait_list_entry_t));
                    wprintk("[%d] end waiting...\n", type);
                }
            #endif

            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                do_gettimeofday(&t2);
            #endif
            }
            break;

            case VPU_DEC_BUF_FLAG_CLEAR:
            case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
            {
                int *arg = (int *)args;
                dprintk("@@ Dec-%d :: DispIdx Clear %d \n", type, *arg);
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void *)(arg), (void *)NULL);
            }
            break;

            case VPU_DEC_FLUSH_OUTPUT:
            case VPU_DEC_FLUSH_OUTPUT_KERNEL:
            {
                VDEC_DECODE_t *arg;

                arg = (VDEC_DECODE_t *)args;
                printk("@@ Dec-%d :: VPU_DEC_FLUSH_OUTPUT !! \n", type);
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void *)(&arg->gsVpuDecInput), (void *)(&arg->gsVpuDecOutput));
            }
            break;

            case VPU_DEC_CLOSE:
            case VPU_DEC_CLOSE_KERNEL:
            {
                //VDEC_DECODE_t *arg;

                //arg = (VDEC_DECODE_t *)args;
                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void *)NULL, (void *)NULL/*(&arg->gsVpuDecOutput)*/);
                dprintk("@@ Dec-%d :: VPU_DEC_CLOSED !! \n", type);

            #ifdef USE_WAIT_LIST
                if (IsUseWaitList() && wait_entry_info.wait_dec_status == 1){
                    wait_entry_info.wait_dec_status = 0;
                    wprintk("[%d] end waiting with closing VPU\n", type);
                }
            #endif

                vmgr_set_close(type, 1, 1);
            }
            break;

            case GET_RING_BUFFER_STATUS:
            case GET_RING_BUFFER_STATUS_KERNEL:
            {
                VDEC_RINGBUF_GETINFO_t *arg;

                arg = (VDEC_RINGBUF_GETINFO_t *)args;
                vmgr_data.check_interrupt_detection = 1;

                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void *)NULL, (void *)(&arg->gsVpuDecRingStatus));
            }
            break;

            case FILL_RING_BUFFER_AUTO:
            case FILL_RING_BUFFER_AUTO_KERNEL:
            {
                VDEC_RINGBUF_SETBUF_t *arg;

                arg = (VDEC_RINGBUF_SETBUF_t *)args;
                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void *)(&arg->gsVpuDecInit), (void *)(&arg->gsVpuDecRingFeed));
                dprintk("@@ Dec-%d :: ReadPTR : 0x%08x, WritePTR : 0x%08x\n", type,
                        vetc_reg_read(vmgr_data.base_addr, 0x120), vetc_reg_read(vmgr_data.base_addr, 0x124));
            }
            break;

            case VPU_UPDATE_WRITE_BUFFER_PTR:
            case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
            {
                VDEC_RINGBUF_SETBUF_PTRONLY_t *arg;

                arg = (VDEC_RINGBUF_SETBUF_PTRONLY_t *)args;
                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(arg->iCopiedSize), (void*)(arg->iFlushBuf));
            }
            break;

            case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
            case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL:
            {
                VDEC_SEQ_HEADER_t *arg;

                arg = (VDEC_SEQ_HEADER_t *)args;
                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, (void*)(&arg->gsVpuDecInitialInfo), NULL);
            }
            break;

            case VPU_CODEC_GET_VERSION:
            case VPU_CODEC_GET_VERSION_KERNEL:
            {
                VDEC_GET_VERSION_t *arg;

                arg = (VDEC_GET_VERSION_t *)args;
                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, arg->pszVersion, arg->pszBuildData);
                dprintk("@@ Dec-%d :: version : %s, build : %s\n", type, arg->pszVersion, arg->pszBuildData);
            }
            break;

            case VPU_DEC_SWRESET:
            case VPU_DEC_SWRESET_KERNEL:
            {
                ret = tcc_vpu_dec(cmd & ~VPU_BASE_OP_KERNEL, (codec_handle_t*)&pHandle, NULL, NULL);
            }
            break;

            default:
            {
                err("@@ Dec-%d :: not supported command(0x%x) \n", type, cmd);
                return 0x999;
            }
            break;
        }
    }
#if !defined(VPU_D6)
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
    else
    {
        if (cmd != VPU_ENC_INIT) {
            if (vmgr_get_close(type) || vmgr_data.handle[type] == 0x00) {
                return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
            }
        }

        if (cmd != VPU_ENC_ENCODE) {
            cmdk("@@@@@@@@@@ Encoder(%d), command: 0x%x \n", type, cmd);
        }

        switch (cmd)
        {
            case VPU_ENC_INIT:
            {
                VENC_INIT_t* arg;

                arg = (VENC_INIT_t *)args;
                vmgr_data.handle[type] = 0x00;
                vmgr_data.szFrame_Len = arg->gsVpuEncInit.m_iPicWidth * arg->gsVpuEncInit.m_iPicHeight *3 / 2;

                arg->gsVpuEncInit.m_RegBaseVirtualAddr = (codec_addr_t)vmgr_data.base_addr;
                arg->gsVpuEncInit.m_Memcpy              = (void* (*) ( void*, const void*, unsigned int, unsigned int ))vetc_memcpy;
                arg->gsVpuEncInit.m_Memset              = (void  (*) ( void*, int, unsigned int, unsigned int ))vetc_memset;
                arg->gsVpuEncInit.m_Interrupt           = (int  (*) ( void ))_vmgr_internal_handler;
                arg->gsVpuEncInit.m_Ioremap             = (void* (*) ( phys_addr_t, unsigned int ))vetc_ioremap;
                arg->gsVpuEncInit.m_Iounmap             = (void  (*) ( void* ))vetc_iounmap;
                arg->gsVpuEncInit.m_reg_read            = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
                arg->gsVpuEncInit.m_reg_write           = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;


                ret = tcc_vpu_enc(cmd, (void*)(&arg->gsVpuEncHandle), (void*)(&arg->gsVpuEncInit), (void*)(&arg->gsVpuEncInitialInfo));

                if( ret != RETCODE_SUCCESS ){
                    printk("## Enc :: Init Done with ret(0x%x)\n", ret);
                    if( ret != RETCODE_CODEC_EXIT ){
                        vetc_dump_reg_all(vmgr_data.base_addr, "Init error");
                    }
                }

                if( ret != RETCODE_CODEC_EXIT && arg->gsVpuEncHandle != 0) {
                    vmgr_data.handle[type] = arg->gsVpuEncHandle;
                    vmgr_set_close(type, 0, 0);
                    cmdk("## Enc vmgr_data.handle = 0x%x \n", arg->gsVpuEncHandle);
                }
                else{
                    //To free memory!!
                    vmgr_set_close(type, 0, 0);
                    vmgr_set_close(type, 1, 1);
                }
                dprintk("## Enc :: Init Done Handle(0x%x) \n", arg->gsVpuEncHandle);
                vmgr_data.nDecode_Cmd = 0;
            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                vmgr_data.iTime[type].print_out_index = vmgr_data.iTime[type].proc_base_cnt = 0;
                vmgr_data.iTime[type].accumulated_proc_time = vmgr_data.iTime[type].accumulated_frame_cnt = 0;
                vmgr_data.iTime[type].proc_time_30frames = 0;
            #endif
            }
            break;

            case VPU_ENC_REG_FRAME_BUFFER:
            {
                VENC_SET_BUFFER_t *arg;

                arg = (VENC_SET_BUFFER_t *)args;
                ret = tcc_vpu_enc(cmd, (codec_handle_t*)&pHandle, (void*)(&arg->gsVpuEncBuffer), (void*)NULL);
                //vetc_dump_reg_all("enc reg framebuffer");
            }
            break;

            case VPU_ENC_PUT_HEADER:
            {
                VENC_PUT_HEADER_t *arg;
            #if !defined(VPU_C5)
                vmgr_data.check_interrupt_detection = 1;
            #endif
                arg = (VENC_PUT_HEADER_t *)args;
                ret = tcc_vpu_enc(cmd, (codec_handle_t*)&pHandle, (void*)(&arg->gsVpuEncHeader), (void*)NULL);
                //printk("put header len %d \n", arg->gsVpuEncHeader.m_iHeaderSize);
                //vetc_dump_reg_all("enc put header");
            }
            break;

            case VPU_ENC_ENCODE:
            {
                VENC_ENCODE_t *arg;
            #ifdef CONFIG_VPU_TIME_MEASUREMENT
                do_gettimeofday( &t1 );
            #endif
                arg = (VENC_ENCODE_t *)args;
                dprintk("## Enc In !! Handle = 0x%x \n", pHandle);

                dprintk("## Enc :: 0x%x-0x%x-0x%x, %d-%d-%d, %d-%d-%d, %d-%d-%d, 0x%x-%d \n",
                        arg->gsVpuEncInput.m_PicYAddr, arg->gsVpuEncInput.m_PicCbAddr, arg->gsVpuEncInput.m_PicCrAddr,
                        arg->gsVpuEncInput.m_iForceIPicture,
                        arg->gsVpuEncInput.m_iSkipPicture,
                        arg->gsVpuEncInput.m_iQuantParam,
                        arg->gsVpuEncInput.m_iChangeRcParamFlag,
                        arg->gsVpuEncInput.m_iChangeTargetKbps,
                        arg->gsVpuEncInput.m_iChangeFrameRate,
                        arg->gsVpuEncInput.m_iChangeKeyInterval,
                        arg->gsVpuEncInput.m_iReportSliceInfoEnable,
                        arg->gsVpuEncInput.m_iReportSliceInfoEnable,
                        arg->gsVpuEncInput.m_BitstreamBufferAddr,
                        arg->gsVpuEncInput.m_iBitstreamBufferSize);

                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_enc(cmd, (codec_handle_t*)&pHandle, (void*)(&arg->gsVpuEncInput), (void*)(&arg->gsVpuEncOutput));

            #if 0//def VPU_REGISTER_DUMP
                if(arg->gsVpuEncInput.m_iForceIPicture != 0)
                    printk("ForceIPicture = %d, 0x%x - 0x%x \n", arg->gsVpuEncInput.m_iForceIPicture, _vmgr_reg_read(0x194), _vmgr_reg_read(0x1C4));
        //#else
                if(arg->gsVpuEncInput.m_iChangeRcParamFlag != 0 && (arg->gsVpuEncInput.m_iChangeTargetKbps != 0 || arg->gsVpuEncInput.m_iChangeFrameRate != 0))
                {
                    printk("Flag(%d) :: %d Kbps, %d fps => %d kbps, %d bit, %d Qp \n", arg->gsVpuEncInput.m_iChangeRcParamFlag,
                            arg->gsVpuEncInput.m_iChangeTargetKbps, arg->gsVpuEncInput.m_iChangeFrameRate, _vmgr_reg_read(0x128), _vmgr_reg_read(0x12C), _vmgr_reg_read(0x1D4));
                    //vetc_dump_reg_all("after encode");
                }
                else
                {
                    if(arg->gsVpuEncInput.m_iChangeTargetKbps != _vmgr_reg_read(0x128)){
                        printk("%d Kbps => %d kbps, %d bit, %d Qp \n", arg->gsVpuEncInput.m_iChangeTargetKbps, _vmgr_reg_read(0x128), _vmgr_reg_read(0x12C), _vmgr_reg_read(0x1D4));
                    }
                }
            #endif
                vmgr_data.nDecode_Cmd++;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
                do_gettimeofday( &t2 );
#endif
                dprintk("## Enc Out[%d] !! PicType[%d], Encoded_size[%d] \n", ret, arg->gsVpuEncOutput.m_iPicType, arg->gsVpuEncOutput.m_iBitstreamOutSize);
            }
            break;

            case VPU_ENC_CLOSE:
            {
                //VENC_ENCODE_t *arg;

                //arg = (VENC_ENCODE_t *)args;
                vmgr_data.check_interrupt_detection = 1;
                ret = tcc_vpu_enc(cmd, (codec_handle_t*)&pHandle, (void*)NULL, (void*)NULL);
                dprintk("## Enc VPU_ENC_CLOSED !! \n");
                vmgr_set_close(type, 1, 1);
            }
            break;

            default:
            {
                err("## Enc :: not supported command(0x%x) \n", cmd);
                return 0x999;
            }
            break;
        }
    }
#endif
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
    time_gap_ms = vetc_GetTimediff_ms(t1, t2);

    if (cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE) {
        vmgr_data.iTime[type].accumulated_frame_cnt++;
        vmgr_data.iTime[type].proc_time[vmgr_data.iTime[type].proc_base_cnt] = time_gap_ms;
        vmgr_data.iTime[type].proc_time_30frames += time_gap_ms;
        vmgr_data.iTime[type].accumulated_proc_time += time_gap_ms;
        if (vmgr_data.iTime[type].proc_base_cnt != 0 && vmgr_data.iTime[type].proc_base_cnt % 29 == 0)
        {
            printk("VPU[%d] Dec[%4d] time %2d.%2d / %2d.%2d ms: "
                   "%2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, "
                   "%2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d \n",
                    type,
                    vmgr_data.iTime[type].print_out_index,
                    vmgr_data.iTime[type].proc_time_30frames / 30,
                    ((vmgr_data.iTime[type].proc_time_30frames % 30) * 100) / 30,
                    vmgr_data.iTime[type].accumulated_proc_time/vmgr_data.iTime[type].accumulated_frame_cnt,
                    ((vmgr_data.iTime[type].accumulated_proc_time%vmgr_data.iTime[type].accumulated_frame_cnt)*100)/vmgr_data.iTime[type].accumulated_frame_cnt,
                    vmgr_data.iTime[type].proc_time[0], vmgr_data.iTime[type].proc_time[1], vmgr_data.iTime[type].proc_time[2],
                    vmgr_data.iTime[type].proc_time[3], vmgr_data.iTime[type].proc_time[4], vmgr_data.iTime[type].proc_time[5],
                    vmgr_data.iTime[type].proc_time[6], vmgr_data.iTime[type].proc_time[7], vmgr_data.iTime[type].proc_time[8],
                    vmgr_data.iTime[type].proc_time[9], vmgr_data.iTime[type].proc_time[10], vmgr_data.iTime[type].proc_time[11],
                    vmgr_data.iTime[type].proc_time[12], vmgr_data.iTime[type].proc_time[13], vmgr_data.iTime[type].proc_time[14],
                    vmgr_data.iTime[type].proc_time[15], vmgr_data.iTime[type].proc_time[16], vmgr_data.iTime[type].proc_time[17],
                    vmgr_data.iTime[type].proc_time[18], vmgr_data.iTime[type].proc_time[19], vmgr_data.iTime[type].proc_time[20],
                    vmgr_data.iTime[type].proc_time[21], vmgr_data.iTime[type].proc_time[22], vmgr_data.iTime[type].proc_time[23],
                    vmgr_data.iTime[type].proc_time[24], vmgr_data.iTime[type].proc_time[25], vmgr_data.iTime[type].proc_time[26],
                    vmgr_data.iTime[type].proc_time[27], vmgr_data.iTime[type].proc_time[28], vmgr_data.iTime[type].proc_time[29]);

            vmgr_data.iTime[type].proc_base_cnt = 0;
            vmgr_data.iTime[type].proc_time_30frames = 0;
            vmgr_data.iTime[type].print_out_index++;
        } else {
            vmgr_data.iTime[type].proc_base_cnt++;
        }
    }
#endif

    return ret;
}

static int _vmgr_proc_exit_by_external(struct VpuList *list, int *result, unsigned int type)
{
    if(!vmgr_get_close(type) && vmgr_data.handle[type] != 0x00)
    {
        list->type = type;
        if( type >= VPU_ENC )
            list->cmd_type = VPU_ENC_CLOSE;
        else
            list->cmd_type = VPU_DEC_CLOSE;
        list->handle    = vmgr_data.handle[type];
        list->args      = NULL;
        list->comm_data = NULL;
        list->vpu_result = result;

        printk("_vmgr_proc_exit_by_external for %d!! \n", type);
        vmgr_list_manager(list, LIST_ADD);

        return 1;
    }

    return 0;
}

static void _vmgr_wait_process(int wait_ms)
{
    int max_count = wait_ms/20;

    //wait!! in case exceptional processing. ex). sdcard out!!
    while(vmgr_data.cmd_processing)
    {
        max_count--;
        msleep(20);

        if(max_count <= 0)
        {
            err("cmd_processing(cmd %d) didn't finish!! \n", vmgr_data.current_cmd);
            break;
        }
    }
}

static int _vmgr_external_all_close(int wait_ms)
{
    int type = 0;
    int max_count = 0;
    int ret;

    for (type = 0; type < VPU_MAX; type++) {
        if (_vmgr_proc_exit_by_external(&vmgr_data.vList[type], &ret, type)) {
            max_count = wait_ms/10;

            while (!vmgr_get_close(type)) {
                max_count--;
                msleep(10);
            }
        }
    }

    return 0;
}

static long _vmgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    CONTENTS_INFO info;
    OPENED_sINFO open_info;

    mutex_lock(&vmgr_data.comm_data.io_mutex);

    switch (cmd)
    {
        case VPU_SET_CLK:
        case VPU_SET_CLK_KERNEL:
        {
            if (cmd == VPU_SET_CLK_KERNEL) {
                if (NULL==memcpy(&info, (CONTENTS_INFO *)arg, sizeof(info)))
                    ret = -EFAULT;
                else {
                    if (info.type >= VPU_ENC && info.isSWCodec) {
                        vmgr_data.clk_limitation = 0;
                        printk("The clock limitation for VPU is released. \n");
                    }
                }
            } else {
                if (copy_from_user(&info, (CONTENTS_INFO *)arg, sizeof(info))) {
                    ret = -EFAULT;
                } else {
                    if (info.type >= VPU_ENC && info.isSWCodec) {
                        vmgr_data.clk_limitation = 0;
                        printk("The clock limitation for VPU is released. \n");
                    }
                }
            }
        }
        break;

        case VPU_GET_FREEMEM_SIZE:
        case VPU_GET_FREEMEM_SIZE_KERNEL:
        {
            unsigned int type;
            unsigned int freemem_sz;
            if(cmd == VPU_GET_FREEMEM_SIZE_KERNEL)
            {
                if (NULL == memcpy(&type, (unsigned int*)arg, sizeof(unsigned int)))
                    ret = -EFAULT;
                else {
                    if(type > VPU_MAX)
                        type = VPU_DEC;

                    freemem_sz = vmem_get_freemem_size(type);

                    if (NULL==memcpy((unsigned int*)arg, &freemem_sz, sizeof(unsigned int)))
                        ret = -EFAULT;
                }
            }
            else
            {
                if(copy_from_user(&type, (unsigned int*)arg, sizeof(unsigned int)))
                    ret = -EFAULT;
                else {
                    if(type > VPU_MAX)
                        type = VPU_DEC;

                    freemem_sz = vmem_get_freemem_size(type);

                    if (copy_to_user((unsigned int*)arg, &freemem_sz, sizeof(unsigned int)))
                        ret = -EFAULT;
                }
            }
        }
        break;

        case VPU_HW_RESET:
        break;

        case VPU_SET_MEM_ALLOC_MODE:
        case VPU_SET_MEM_ALLOC_MODE_KERNEL:
        {
            if (cmd == VPU_SET_MEM_ALLOC_MODE_KERNEL) {
                if (NULL == memcpy(&open_info,(OPENED_sINFO*)arg, sizeof(OPENED_sINFO))) {
                    ret = -EFAULT;
                } else {
                    if (open_info.opened_cnt != 0) {
                        vmem_set_only_decode_mode(open_info.type);
                    }
                    ret = 0;
                }
            } else {
                if (copy_from_user(&open_info, (OPENED_sINFO *)arg, sizeof(OPENED_sINFO))) {
                    ret = -EFAULT;
                } else {
                    if (open_info.opened_cnt != 0) {
                        vmem_set_only_decode_mode(open_info.type);
                    }
                    ret = 0;
                }
            }
        }
        break;

        case VPU_CHECK_CODEC_STATUS:
        case VPU_CHECK_CODEC_STATUS_KERNEL:
        {
            if (cmd == VPU_CHECK_CODEC_STATUS_KERNEL) {
                if (NULL == memcpy((int *)arg, vmgr_data.closed, sizeof(vmgr_data.closed))) {
                    ret = -EFAULT;
                }
            } else {
                if (copy_to_user((int*)arg, vmgr_data.closed, sizeof(vmgr_data.closed))) {
                    ret = -EFAULT;
                } else {
                    ret = 0;
                }
            }
        }
        break;

        case VPU_CHECK_INSTANCE_AVAILABLE:
        case VPU_CHECK_INSTANCE_AVAILABLE_KERNEL:
        {
            unsigned int nAvailable_Instance = 0;
            unsigned int type = VPU_DEC;

            ret = 0;

            if (cmd == VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
                if (NULL == memcpy(&type, (int*)arg, sizeof(unsigned int))) {
                    ret = -EFAULT;
                }
                else {
                    if (copy_from_user(&type, (int*)arg, sizeof(unsigned int)))
                        ret = -EFAULT;
                }
            }

            if (ret == 0) {
                if (type < VPU_ENC) {
                    vdec_check_instance_available(&nAvailable_Instance);
                } else {
                    venc_check_instance_available(&nAvailable_Instance);
                }

                if (cmd == VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
                    if ( NULL == memcpy((unsigned int*)arg, &nAvailable_Instance, sizeof(unsigned int)))
                        ret = -EFAULT;
                } else {
                    if (copy_to_user((unsigned int*)arg, &nAvailable_Instance, sizeof(unsigned int)))
                        ret = -EFAULT;
                }
            }
        }
        break;

        case VPU_GET_INSTANCE_IDX:
        case VPU_GET_INSTANCE_IDX_KERNEL:
        {
            INSTANCE_INFO iInst;

            if(cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
                if (NULL == memcpy(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                    ret = -EFAULT;
            } else {
                if (copy_from_user(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                    ret = -EFAULT;
            }

            if(ret == 0) {
                if( iInst.type == VPU_ENC )
                    venc_get_instance(&iInst.nInstance);
                else
                    vdec_get_instance(&iInst.nInstance);

                if(cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
                    if(NULL == memcpy((int*)arg, &iInst, sizeof(INSTANCE_INFO)))
                        ret = -EFAULT;
                } else {
                    if (copy_to_user((int*)arg, &iInst, sizeof(INSTANCE_INFO)))
                        ret = -EFAULT;
                }
            }
        }
        break;

        case VPU_CLEAR_INSTANCE_IDX:
        case VPU_CLEAR_INSTANCE_IDX_KERNEL:
        {
            INSTANCE_INFO iInst;
            if(cmd == VPU_CLEAR_INSTANCE_IDX_KERNEL) {
                if (NULL == memcpy(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                    ret = -EFAULT;
            } else {
                if (copy_from_user(&iInst, (int*)arg, sizeof(INSTANCE_INFO)))
                    ret = -EFAULT;
            }

            if(ret == 0) {
                if( iInst.type == VPU_ENC )
                    venc_clear_instance(iInst.nInstance);
                else
                    vdec_clear_instance(iInst.nInstance);
            }
        }
        break;

    #ifdef USE_WAIT_LIST
        case VPU_USE_WAIT_LIST:
        {
            int value;
            if (copy_from_user(&value, (int *)arg, sizeof(int))) {
                ret = -EFAULT;
            } else {
                if (value) {
                    use_wait_list = 1;
                } else {
                    use_wait_list = 0;
                }
            }
        }
        break;
    #endif

        case VPU_SET_RENDERED_FRAMEBUFFER:
        case VPU_SET_RENDERED_FRAMEBUFFER_KERNEL:
        {
            if (cmd == VPU_SET_RENDERED_FRAMEBUFFER_KERNEL) {
                if (NULL == memcpy(&vmgr_data.gsRender_fb_info,(void*)arg,sizeof(VDEC_RENDERED_BUFFER_t)))
                    ret = -EFAULT;
            } else {
                if (copy_from_user(&vmgr_data.gsRender_fb_info, (void *)arg, sizeof(VDEC_RENDERED_BUFFER_t))) {
                    ret = -EFAULT;
                } else {
                    dprintk("set rendered buffer info: 0x%x ~ 0x%x \n",
                            vmgr_data.gsRender_fb_info.start_addr_phy, vmgr_data.gsRender_fb_info.size);
                }
            }
        }
        break;

        case VPU_GET_RENDERED_FRAMEBUFFER:
        case VPU_GET_RENDERED_FRAMEBUFFER_KERNEL:
        {
            if (cmd == VPU_GET_RENDERED_FRAMEBUFFER_KERNEL) {
                if (NULL == memcpy((void*)arg, &vmgr_data.gsRender_fb_info, sizeof(VDEC_RENDERED_BUFFER_t)))
                    ret = -EFAULT;
            } else {
                if (copy_to_user((void*)arg, &vmgr_data.gsRender_fb_info, sizeof(VDEC_RENDERED_BUFFER_t))) {
                    ret = -EFAULT;
                } else {
                    dprintk("get rendered buffer info: 0x%x ~ 0x%x \n", vmgr_data.gsRender_fb_info.start_addr_phy, vmgr_data.gsRender_fb_info.size);
                }
            }
        }
        break;

    #ifndef CONFIG_SUPPORT_TCC_JPU
        case VPU_ENABLE_JPU_CLOCK:
        {
            vmgr_enable_jpu_clock();
        }
        break;

        case VPU_DISABLE_JPU_CLOCK:
        {
            vmgr_disable_jpu_clock();
        }
        break;
    #endif

		case VPU_TRY_FORCE_CLOSE:
		case VPU_TRY_FORCE_CLOSE_KERNEL:
		{
            //tcc_vpu_dec_esc(1, 0, 0, 0);

            if(!vmgr_data.bVpu_already_proc_force_closed)
			{
				_vmgr_wait_process(200);
				vmgr_data.external_proc = 1;
				_vmgr_external_all_close(200);
				vmgr_data.external_proc = 0;
				vmgr_data.bVpu_already_proc_force_closed = true;
			}
        }
		break;

		case VPU_TRY_CLK_RESTORE:
		case VPU_TRY_CLK_RESTORE_KERNEL:
		{
			vmgr_restore_clock(0, vmgr_data.dev_opened);
		}
		break;

        default:
            err("Unsupported ioctl[%d]!!!\n", cmd);
            ret = -EINVAL;
        break;
    }

    mutex_unlock(&vmgr_data.comm_data.io_mutex);

    return ret;
}

#ifdef CONFIG_COMPAT
static long _vmgr_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return _vmgr_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static unsigned int cntInt_vpu = 0;
static irqreturn_t _vmgr_isr_handler(int irq, void *dev_id)
{
    unsigned long flags;

	cntInt_vpu++;

	if(cntInt_vpu%30 == 0){
		detailk("_vmgr_isr_handler %d \n", cntInt_vpu);
	}

    spin_lock_irqsave(&(vmgr_data.oper_lock), flags);
    vmgr_data.oper_intr++;
    spin_unlock_irqrestore(&(vmgr_data.oper_lock), flags);

    wake_up_interruptible(&(vmgr_data.oper_wq));

    return IRQ_HANDLED;
}

static int _vmgr_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

    if (!vmgr_data.irq_reged) {
        err("not registered vpu-mgr-irq \n");
    }

    dprintk("_vmgr_open In!! %d'th \n", vmgr_data.dev_opened);

    vmgr_enable_clock(0, 0);

    if(vmgr_data.dev_opened == 0)
    {
#ifdef FORCED_ERROR
        forced_error_count = FORCED_ERR_CNT;
#endif
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
        vmgr_data.only_decmode = 0;
#else
        vmgr_data.only_decmode = 1;
#endif
        vmgr_data.clk_limitation = 1;
        //vmgr_hw_reset();
        vmgr_data.cmd_processing = 0;

        vmgr_enable_irq(vmgr_data.irq);
        vetc_reg_init(vmgr_data.base_addr);
        vmem_init();
		cntInt_vpu = 0;
    }
    vmgr_data.dev_opened++;

    filp->private_data = &vmgr_data;

	dprintk("_vmgr_open Out!! %d'th \n", vmgr_data.dev_opened);

    return 0;
}

static int _vmgr_release(struct inode *inode, struct file *filp)
{
    dprintk("_vmgr_release In!! %d'th \n", vmgr_data.dev_opened);

	if(!vmgr_data.bVpu_already_proc_force_closed)
	{
		_vmgr_wait_process(200);
	}
    if (vmgr_data.dev_opened > 0) {
        vmgr_data.dev_opened--;
    }

    if (vmgr_data.dev_opened == 0)
    {
//////////////////////////////////////
        int type = 0, alive_cnt = 0;

    #if 1 // To close whole vpu instance when being killed process opened this.
		if(!vmgr_data.bVpu_already_proc_force_closed)
		{
			vmgr_data.external_proc = 1;
			_vmgr_external_all_close(200);
			_vmgr_wait_process(200);
			vmgr_data.external_proc = 0;
		}
		vmgr_data.bVpu_already_proc_force_closed = false;
    #endif

        for(type=0; type<VPU_MAX; type++) {
            if( vmgr_data.closed[type] == 0 ){
                alive_cnt++;
            }
        }

        if( alive_cnt )
        {
            // clear instances of vpu by force.
            //TCC_VPU_DEC( 0x40, (void*)NULL, (void*)NULL, (void*)NULL);
            printk("VPU might be cleared by force. \n");
        }

//////////////////////////////////////
        vmgr_data.oper_intr = 0;
        vmgr_data.cmd_processing = 0;

        _vmgr_close_all(1);
//////////////////////////////////////

        vmgr_disable_irq(vmgr_data.irq);
        vmgr_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();
    }

    vmgr_disable_clock(0, 0);

    vmgr_data.nOpened_Count++;

    printk("_vmgr_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d) \n", vmgr_data.dev_opened, vmgr_data.nOpened_Count,
                    vmgr_get_close(VPU_DEC), vmgr_get_close(VPU_DEC_EXT), vmgr_get_close(VPU_DEC_EXT2), vmgr_get_close(VPU_DEC_EXT3), vmgr_get_close(VPU_DEC_EXT4));

    return 0;
}

VpuList_t* vmgr_list_manager(VpuList_t* args, unsigned int cmd)
{
    VpuList_t *ret;

    mutex_lock(&vmgr_data.comm_data.list_mutex);

    {
        VpuList_t* data = NULL;
        ret = NULL;

    #if 0
        data = (VpuList_t *)args;

        if (cmd == LIST_ADD)
            printk("cmd = %d - 0x%x \n", cmd, data->cmd_type);
        else
            printk("cmd = %d \n", cmd);
    #endif

        switch (cmd) {
            case LIST_ADD:
                if (!args) {
                    err("ADD :: data is null \n");
                    goto Error;
                }

                data = (VpuList_t*)args;
                *(data->vpu_result) |= RET1;
                list_add_tail(&data->list, &vmgr_data.comm_data.main_list);
                vmgr_data.cmd_queued++;
                vmgr_data.comm_data.thread_intr++;
                wake_up_interruptible(&(vmgr_data.comm_data.thread_wq));
                break;

            case LIST_DEL:
                if(!args)
                {
                    err("DEL :: data is null \n");
                    goto Error;
                }
                data = (VpuList_t*)args;
                list_del(&data->list);
                vmgr_data.cmd_queued--;
                break;

            case LIST_IS_EMPTY:
                if (list_empty(&vmgr_data.comm_data.main_list)) {
                    ret =(VpuList_t *)0x1234;
                }
                break;

            case LIST_GET_ENTRY:
                ret = list_first_entry(&vmgr_data.comm_data.main_list, VpuList_t, list);
                break;
        }
    }

Error:
    mutex_unlock(&vmgr_data.comm_data.list_mutex);

    return ret;
}

#ifdef USE_WAIT_LIST
static unsigned int cmd_queued_fot_wait_list = 0;
VpuList_t* vmgr_waitlist_manager(VpuList_t* args, unsigned int cmd)
{
    VpuList_t *ret;

    {
        VpuList_t *data = NULL;
        ret = NULL;
    #if 0
        data = (VpuList_t *)args;

        if(cmd == LIST_ADD)
            printk("cmd = %d - 0x%x \n", cmd, data->cmd_type);
        else
            printk("cmd = %d \n", cmd);
    #endif

        switch (cmd) {
            case LIST_ADD:
                if (!args) {
                    err("ADD :: data is null \n");
                    goto Error;
                }

                data = (VpuList_t*)args;
                *(data->vpu_result) |= RET1;
                list_add_tail(&data->list, &vmgr_data.comm_data.wait_list);cmd_queued_fot_wait_list++;
                break;

            case LIST_DEL:
                if (!args) {
                    err("DEL :: data is null \n");
                    goto Error;
                }
                data = (VpuList_t*)args;
                list_del(&data->list);cmd_queued_fot_wait_list--;
                break;

            case LIST_IS_EMPTY:
                if (list_empty(&vmgr_data.comm_data.wait_list)) {
                    ret = (VpuList_t *)0x1234;
                }
                break;

            case LIST_GET_ENTRY:
                ret = list_first_entry(&vmgr_data.comm_data.wait_list, VpuList_t, list);
                break;
        }
    }

Error:
    return ret;
}
#endif

//////////////////////////////////////////////////////////////////////////
// VPU Thread!!
static int _vmgr_operation(void)
{
    int oper_finished;
    VpuList_t *oper_data = NULL;
#ifdef USE_WAIT_LIST
    int is_from_wait_list = 0;
#endif

    while (!vmgr_list_manager(NULL, LIST_IS_EMPTY)) {
        vmgr_data.cmd_processing = 1;
        oper_finished = 1;

        dprintk("_vmgr_operation :: not empty cmd_queued(%d) \n", vmgr_data.cmd_queued);
    #ifdef USE_WAIT_LIST
        dprintk("_vmgr_operation :: not empty cmd_queued in wait list(%d) \n", cmd_queued_fot_wait_list);

        if (IsUseWaitList()) {
            if (wait_entry_info.wait_dec_status == 1) {
                for (;;) {
                    if (vmgr_list_manager(NULL, LIST_IS_EMPTY)) {
                        vmgr_data.cmd_processing = 0;
                        return 0;
                    }

                    oper_data = vmgr_list_manager(NULL, LIST_GET_ENTRY);

                    if (wait_entry_info.type != oper_data->type) {
                        vmgr_list_manager(oper_data, LIST_DEL);
                        vmgr_waitlist_manager(oper_data, LIST_ADD);  //move to field-picture wait list
                        wprintk("[%d] move command into waiting queue!! \n", oper_data->type);
                    } else {
                        is_from_wait_list = 0;
                        if (!oper_data) {
                            vmgr_data.cmd_processing = 0;
                            return 0;
                        }
                        wprintk("[%d] process command for waiting VPU!! \n", oper_data->type);
                        *(oper_data->vpu_result) |= RET2;

                        break;
                    }
                }
            } else {
                if (!vmgr_waitlist_manager(NULL, LIST_IS_EMPTY)) {
                    oper_data = vmgr_waitlist_manager(NULL, LIST_GET_ENTRY);
                    wprintk("[%d] process command from waiting queue!! \n", oper_data->type);
                    is_from_wait_list = 1;
                } else {
                    oper_data = vmgr_list_manager(NULL, LIST_GET_ENTRY);
                    is_from_wait_list = 0;
                }

                if (!oper_data) {
                    vmgr_data.cmd_processing = 0;
                    return 0;
                }

                *(oper_data->vpu_result) |= RET2;
            }
        } else
    #endif
        {
            oper_data = vmgr_list_manager(NULL, LIST_GET_ENTRY);
            if(!oper_data) {
                err("data is null \n");
                vmgr_data.cmd_processing = 0;
                return 0;
            }
            *(oper_data->vpu_result) |= RET2;
        }


        dprintk("_vmgr_operation [%d] :: cmd = 0x%x, cmd_queued(%d) \n", oper_data->type, oper_data->cmd_type, vmgr_data.cmd_queued);

        if (oper_data->type < VPU_MAX && oper_data != NULL /*&& oper_data->comm_data != NULL*/)
        {
            *(oper_data->vpu_result) |= RET3;

            *(oper_data->vpu_result) = _vmgr_process(oper_data->type, oper_data->cmd_type, oper_data->handle, oper_data->args);
            oper_finished = 1;
            if (*(oper_data->vpu_result) != RETCODE_SUCCESS)
            {
                if (*(oper_data->vpu_result) != RETCODE_INSUFFICIENT_BITSTREAM &&
                    *(oper_data->vpu_result) != RETCODE_INSUFFICIENT_BITSTREAM_BUF) {
                    err("vmgr_out[0x%x] :: type = %d, handle = 0x%x, cmd = 0x%x, frame_len %d \n",
                            *(oper_data->vpu_result), oper_data->type, oper_data->handle, oper_data->cmd_type, vmgr_data.szFrame_Len);
                }

                if (*(oper_data->vpu_result) == RETCODE_CODEC_EXIT) {
                	vmgr_restore_clock(0, vmgr_data.dev_opened);
                    _vmgr_close_all(1);
                }
            }
        } else {
            printk("_vmgr_operation: missed info or unknown command => type = 0x%x, cmd = 0x%x \n", oper_data->type, oper_data->cmd_type);

            *(oper_data->vpu_result) = RETCODE_FAILURE;
            oper_finished = 0;
        }

        if (oper_finished) {
            if (oper_data->comm_data != NULL && vmgr_data.dev_opened != 0) {
                //unsigned long flags;
                //spin_lock_irqsave(&(oper_data->comm_data->lock), flags);
                oper_data->comm_data->count += 1;
                if (oper_data->comm_data->count != 1) {
                    dprintk("poll wakeup count = %d :: type(0x%x) cmd(0x%x) \n",
                            oper_data->comm_data->count, oper_data->type, oper_data->cmd_type);
                }

                //spin_unlock_irqrestore(&(oper_data->comm_data->lock), flags);
                wake_up_interruptible(&(oper_data->comm_data->wq));
            } else {
                err("Error: abnormal exception or external command was processed!! 0x%p - %d\n", oper_data->comm_data, vmgr_data.dev_opened);
            }
        }
        else{
            err("Error: abnormal exception 2!! 0x%p - %d\n",oper_data->comm_data, vmgr_data.dev_opened);
        }

    #ifdef USE_WAIT_LIST
        if (IsUseWaitList()) {
            if (is_from_wait_list == 1) {
                vmgr_waitlist_manager(oper_data, LIST_DEL);
            }

            if (is_from_wait_list == 0) {
                vmgr_list_manager(oper_data, LIST_DEL);
            }
        } else
    #endif
        {
            vmgr_list_manager(oper_data, LIST_DEL);
        }

        vmgr_data.cmd_processing = 0;
    }

    return 0;
}

static int _vmgr_thread(void *kthread)
{
    dprintk("enter %s\n", __func__);

    do {
        //detailk("_vmgr_thread wait_sleep \n");

        if (vmgr_list_manager(NULL, LIST_IS_EMPTY)) {
            vmgr_data.cmd_processing = 0;

            //wait_event_interruptible(vmgr_data.comm_data.thread_wq, vmgr_data.comm_data.thread_intr > 0);
            wait_event_interruptible_timeout(vmgr_data.comm_data.thread_wq,
                                             vmgr_data.comm_data.thread_intr > 0,
                                             msecs_to_jiffies(50));
            vmgr_data.comm_data.thread_intr = 0;
        } else {
            if (vmgr_data.dev_opened || vmgr_data.external_proc) {
                _vmgr_operation();
            } else {
                VpuList_t *oper_data = NULL;

                printk("DEL for empty \n");

                oper_data = vmgr_list_manager(NULL, LIST_GET_ENTRY);
                if (oper_data) {
                    vmgr_list_manager(oper_data, LIST_DEL);
                }
            }
        }
    } while (!kthread_should_stop());

    dprintk("finish %s\n", __func__);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
static int _vmgr_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)
    if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0){
        printk(KERN_ERR  "_vmgr_mmap: this address is not allowed \n");
        return -EAGAIN;
    }
#endif

    vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
    if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
    {
        printk("_vmgr_mmap :: remap_pfn_range failed\n");
        return -EAGAIN;
    }

    vma->vm_ops    = NULL;
    vma->vm_flags |= VM_IO;
    vma->vm_flags |= VM_DONTEXPAND | VM_PFNMAP;

    return 0;
}

static struct file_operations _vmgr_fops = {
    .open               = _vmgr_open,
    .release            = _vmgr_release,
    .mmap               = _vmgr_mmap,
    .unlocked_ioctl     = _vmgr_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl       = _vmgr_compat_ioctl,
#endif
};

static struct miscdevice _vmgr_misc_device =
{
    MISC_DYNAMIC_MINOR,
    MGR_NAME,
    &_vmgr_fops,
};

int vmgr_probe(struct platform_device *pdev)
{
    int ret;
    int type = 0;
    unsigned long int_flags;
    struct resource *res = NULL;

    if (pdev->dev.of_node == NULL) {
        return -ENODEV;
    }

    dprintk("vmgr initializing!! \n");
    memset(&vmgr_data, 0, sizeof(mgr_data_t));
    for(type=0; type<VPU_MAX; type++) {
        vmgr_data.closed[type] = 1;
    }

    vmgr_init_variable();

    vmgr_data.irq = platform_get_irq(pdev, 0);
    vmgr_data.nOpened_Count = 0;
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "missing phy memory resource\n");
        return -1;
    }
    res->end += 1;
    //vmgr_data.base_addr = regs->start;
    vmgr_data.base_addr = devm_ioremap(&pdev->dev, res->start, res->end-res->start);
    dprintk("============> VPU base address [0x%x -> 0x%p], irq num [%d] \n", res->start, vmgr_data.base_addr, vmgr_data.irq - 32);

    vmgr_get_clock(pdev->dev.of_node);

    spin_lock_init(&(vmgr_data.oper_lock));
    //  spin_lock_init(&(vmgr_data.comm_data.lock));

    init_waitqueue_head(&vmgr_data.comm_data.thread_wq);
    init_waitqueue_head(&vmgr_data.oper_wq);

    mutex_init(&vmgr_data.comm_data.list_mutex);
    mutex_init(&(vmgr_data.comm_data.io_mutex));

    INIT_LIST_HEAD(&vmgr_data.comm_data.main_list);
    INIT_LIST_HEAD(&vmgr_data.comm_data.wait_list);

    if( 0 > (ret = vmem_config()))
    {
        err("unable to configure memory for VPU!! %d \n", ret);
        return -ENOMEM;
    }

    vmgr_init_interrupt();
    int_flags = vmgr_get_int_flags();
    ret = vmgr_request_irq(vmgr_data.irq, _vmgr_isr_handler, int_flags, MGR_NAME, &vmgr_data);
    if (ret) {
        err("to aquire vpu-dec-irq \n");
    }
    vmgr_data.irq_reged = 1;
    vmgr_disable_irq(vmgr_data.irq);

    kidle_task = kthread_run(_vmgr_thread, NULL, "vVPU_th");
    if (IS_ERR(kidle_task)) {
        err("unable to create thread!! \n");
        kidle_task = NULL;
        return -1;
    }
    dprintk("success: thread created!! \n");

    _vmgr_close_all(1);

    if (misc_register(&_vmgr_misc_device)) {
        printk(KERN_WARNING "VPU Manager: Couldn't register device.\n");
        return -EBUSY;
    }

    vmgr_enable_clock(0, 1);
    vmgr_disable_clock(0, 1);

    return 0;
}
EXPORT_SYMBOL(vmgr_probe);

int vmgr_remove(struct platform_device *pdev)
{
    misc_deregister(&_vmgr_misc_device);

    if (kidle_task) {
        kthread_stop(kidle_task);
        kidle_task = NULL;
    }

    devm_iounmap(&pdev->dev, (void __iomem *)vmgr_data.base_addr);
    if (vmgr_data.irq_reged) {
        vmgr_free_irq(vmgr_data.irq, &vmgr_data);
        vmgr_data.irq_reged = 0;
    }

    vmgr_put_clock();
    vmem_deinit();

    printk("success :: thread stopped!! \n");

    return 0;
}
EXPORT_SYMBOL(vmgr_remove);

#if defined(CONFIG_PM)
int vmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
    int i, open_count = 0;

    if (vmgr_data.dev_opened != 0) {
        printk("\n vpu: suspend In DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d) \n",
                vmgr_get_close(VPU_DEC), vmgr_get_close(VPU_DEC_EXT), vmgr_get_close(VPU_DEC_EXT2), vmgr_get_close(VPU_DEC_EXT3), vmgr_get_close(VPU_DEC_EXT4),
                vmgr_get_close(VPU_ENC), vmgr_get_close(VPU_ENC_EXT), vmgr_get_close(VPU_ENC_EXT2), vmgr_get_close(VPU_ENC_EXT3));

        _vmgr_external_all_close(200);

        open_count = vmgr_data.dev_opened;

        for (i = 0; i < open_count; i++) {
            vmgr_disable_clock(0, 0);
        }
        printk("vpu: suspend Out DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d) \n\n",
                vmgr_get_close(VPU_DEC), vmgr_get_close(VPU_DEC_EXT), vmgr_get_close(VPU_DEC_EXT2), vmgr_get_close(VPU_DEC_EXT3), vmgr_get_close(VPU_DEC_EXT4),
                vmgr_get_close(VPU_ENC), vmgr_get_close(VPU_ENC_EXT), vmgr_get_close(VPU_ENC_EXT2), vmgr_get_close(VPU_ENC_EXT3));
    }

    return 0;
}
EXPORT_SYMBOL(vmgr_suspend);

int vmgr_resume(struct platform_device *pdev)
{
    int i, open_count = 0;

    if (vmgr_data.dev_opened != 0) {

        open_count = vmgr_data.dev_opened;

        for (i=0; i<open_count; i++) {
            vmgr_enable_clock(0, 0);
        }
        printk("\n vpu: resume \n\n");
    }

    return 0;
}
EXPORT_SYMBOL(vmgr_resume);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu manager");
MODULE_LICENSE("GPL");

