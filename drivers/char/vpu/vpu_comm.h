/*
 *   FileName : vpu_comm.h
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

#ifndef _VPU_COMM_H_
#define _VPU_COMM_H_

#include "vpu_structure.h"
#include "vpu_etc.h"

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
#include "smp_driver.h"
#endif

#if defined(CONFIG_TYPE_C5)
#include <video/tcc/TCC_VPUs_CODEC.h> // VPU video codec
#else
#include <video/tcc/TCC_VPU_CODEC.h> // VPU video codec
#endif
#include <video/tcc/tcc_vpu_ioctl.h>

#ifdef CONFIG_SUPPORT_TCC_JPU
#if defined(JPU_C5)
#include <video/tcc/TCC_JPU_CODEC.h>
#else
#include <video/tcc/TCC_JPU_C6.h>
#endif
#include <video/tcc/tcc_jpu_ioctl.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include <video/tcc/TCC_HEVC_CODEC.h>
#include <video/tcc/tcc_hevc_ioctl.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
#include <video/tcc/TCC_VPU_4K_D2_CODEC.h>
#include <video/tcc/tcc_4k_d2_ioctl.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include <video/tcc/TCC_VP9_CODEC.h>
#include <video/tcc/tcc_vp9_ioctl.h>
#endif

////////////////////////////////////////////////////////////////////////////////////////
/* COMMON */
#define IRQ_INT_TYPE    (IRQ_TYPE_EDGE_RISING|IRQF_SHARED)

#define LIST_MAX 10

#define RET3        0x00008000
#define RET2        0x00004000
#define RET1        0x00002000
#define RET0        0x00001000

typedef enum {
    LIST_ADD = 0,
    LIST_DEL,
    LIST_IS_EMPTY,
    LIST_GET_ENTRY
}list_cmd_type;

typedef enum {
    INTR_INIT = 0,
    INTR_GET,
    INTR_MINUS,
    INTR_WAKEUP
}intr_func_type;

typedef struct _vpu_dec_data_t{
    wait_queue_head_t wq;
    spinlock_t lock;
    unsigned int count;
    unsigned char dev_opened;
} vpu_comm_data_t;

typedef struct VpuList{
    struct list_head list;

    unsigned int type; //encode or decode ?
    int cmd_type; //vpu command
    long handle;
    void *args;    //vpu argument!!
    vpu_comm_data_t *comm_data;

    int *vpu_result;
}VpuList_t;

typedef struct MgrCommData{
    struct list_head main_list;
    struct list_head wait_list;
    struct mutex list_mutex;
    struct mutex io_mutex;

//  spinlock_t lock;
    unsigned int thread_intr;
    wait_queue_head_t thread_wq;
}Mgr_CommData_t;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
typedef struct TimeInfo{
    unsigned int print_out_index;
    unsigned int proc_time[30];
    unsigned int proc_base_cnt;         // 0~29
    unsigned int proc_time_30frames;
    unsigned int accumulated_proc_time; // between 1st frame and last one.
    unsigned int accumulated_frame_cnt;
}TimeInfo_t;
#endif

typedef struct _mgr_data_t {
//IRQ number and IP base
    unsigned int irq;
    void __iomem *base_addr;
    int check_interrupt_detection;
    int closed[VPU_MAX];
    long handle[VPU_MAX];
    int fileplay_mode[VPU_MAX];
#if defined(CONFIG_PM)
    VpuList_t vList[VPU_MAX];
#endif
    Mgr_CommData_t comm_data;

    spinlock_t oper_lock;
    unsigned int oper_intr;
    wait_queue_head_t oper_wq;

    MEM_ALLOC_INFO_t work_memInfo;
    unsigned char dev_opened;
    unsigned char irq_reged;

    unsigned char cmd_processing;
    unsigned char only_decmode;
    unsigned char clk_limitation;

    unsigned char external_proc;

    int current_cmd;
    unsigned int szFrame_Len;
    unsigned int nDecode_Cmd;
    unsigned int nOpened_Count;
    unsigned int current_resolution;

    VDEC_RENDERED_BUFFER_t gsRender_fb_info;
    unsigned int cmd_queued;

    bool bDiminishInputCopy;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
    TimeInfo_t iTime[VPU_MAX];
#endif
	bool bVpu_already_proc_force_closed;
} mgr_data_t;

#if defined(CONFIG_VDEC_CNT_1) || defined(CONFIG_VDEC_CNT_2) || defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
typedef struct _vpu_decoder_data{
    struct miscdevice *misc;
    vpu_comm_data_t vComm_data;
    int gsDecType;
    int gsCodecType;

    int gsIsDiminishedCopy;

    VDEC_INIT_t             gsVpuDecInit_Info;
    VDEC_SEQ_HEADER_t       gsVpuDecSeqHeader_Info;
    VDEC_SET_BUFFER_t       gsVpuDecBuffer_Info;
    VDEC_DECODE_t           gsVpuDecInOut_Info;
    VDEC_RINGBUF_GETINFO_t  gsVpuDecBufStatus;
    VDEC_RINGBUF_SETBUF_t   gsVpuDecBufFill;
    VDEC_RINGBUF_SETBUF_PTRONLY_t gsVpuDecUpdateWP;
    VDEC_GET_VERSION_t      gsVpuDecVersion;

#ifdef CONFIG_SUPPORT_TCC_JPU
    JDEC_INIT_t             gsJpuDecInit_Info;
#if defined(JPU_C6)
    JDEC_SEQ_HEADER_t       gsJpuDecSeqHeader_Info;
#endif
    JPU_SET_BUFFER_t        gsJpuDecBuffer_Info;
    JPU_DECODE_t            gsJpuDecInOut_Info;
    JPU_GET_VERSION_t       gsJpuDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
    VPU_4K_D2_INIT_t            gsV4kd2DecInit_Info;
    VPU_4K_D2_SEQ_HEADER_t      gsV4kd2DecSeqHeader_Info;
    VPU_4K_D2_SET_BUFFER_t      gsV4kd2DecBuffer_Info;
    VPU_4K_D2_DECODE_t          gsV4kd2DecInOut_Info;
    VPU_4K_D2_RINGBUF_GETINFO_t gsV4kd2DecBufStatus;
    VPU_4K_D2_RINGBUF_SETBUF_t  gsV4kd2DecBufFill;
    VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t gsV4kd2DecUpdateWP;
    VPU_4K_D2_GET_VERSION_t     gsV4kd2DecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
    HEVC_INIT_t             gsHevcDecInit_Info;
    HEVC_SEQ_HEADER_t       gsHevcDecSeqHeader_Info;
    HEVC_SET_BUFFER_t       gsHevcDecBuffer_Info;
    HEVC_DECODE_t           gsHevcDecInOut_Info;
    HEVC_RINGBUF_GETINFO_t  gsHevcDecBufStatus;
    HEVC_RINGBUF_SETBUF_t   gsHevcDecBufFill;
    HEVC_RINGBUF_SETBUF_PTRONLY_t gsHevcDecUpdateWP;
    HEVC_GET_VERSION_t      gsHevcDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
    VP9_INIT_t              gsVp9DecInit_Info;
    VP9_SEQ_HEADER_t        gsVp9DecSeqHeader_Info;
    VP9_SET_BUFFER_t        gsVp9DecBuffer_Info;
    VP9_DECODE_t            gsVp9DecInOut_Info;
    /*
    VP9_RINGBUF_GETINFO_t   gsVp9DecBufStatus;
    VP9_RINGBUF_SETBUF_t    gsVp9DecBufFill;
    VP9_RINGBUF_SETBUF_PTRONLY_t gsVp9DecUpdateWP;
    */
    VP9_GET_VERSION_t gsVp9DecVersion;
#endif

    unsigned int gsDecClearBuffer_index;
    int gsCommDecResult;
    unsigned char list_idx;

    VpuList_t vdec_list[LIST_MAX];
} vpu_decoder_data;
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
typedef struct _vpu_encoder_data{
    struct miscdevice *misc;
    vpu_comm_data_t vComm_data;
    int gsEncType;
    int gsCodecType;

    VENC_INIT_t gsVpuEncInit_Info;
    VENC_PUT_HEADER_t gsVpuEncPutHeader_Info;
    VENC_SET_BUFFER_t gsVpuEncBuffer_Info;
    VENC_ENCODE_t gsVpuEncInOut_Info;

#ifdef CONFIG_SUPPORT_TCC_JPU
    JENC_INIT_t gsJpuEncInit_Info;
    // JPU_SEQ_HEADER_t gsJpuEncSeqHeader_Info;
    // JPU_SET_BUFFER_t gsJpuEncBuffer_Info;
    JPU_ENCODE_t gsJpuEncInOut_Info;
    // JPU_GET_VERSION_t gsJpuEncVersion;
#endif

    int gsCommEncResult;
    unsigned char list_idx;

    VpuList_t venc_list[LIST_MAX];
} vpu_encoder_data;
#endif
#endif

