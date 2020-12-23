// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_COMM_H
#define VPU_COMM_H

/*****************************************************************************
 * VIDEO DECODER COUNT DEFINITION
 */
#define DEFINED_CONFIG_VDEC_CNT_5 defined(CONFIG_VDEC_CNT_5)

#define DEFINED_CONFIG_VDEC_CNT_45 \
	(defined(CONFIG_VDEC_CNT_4) || DEFINED_CONFIG_VDEC_CNT_5)

#define DEFINED_CONFIG_VDEC_CNT_345 \
	(defined(CONFIG_VDEC_CNT_3) || DEFINED_CONFIG_VDEC_CNT_45)

#define DEFINED_CONFIG_VDEC_CNT_2345 \
	(defined(CONFIG_VDEC_CNT_2) || DEFINED_CONFIG_VDEC_CNT_345)

#define DEFINED_CONFIG_VDEC_CNT_12345 \
	(defined(CONFIG_VDEC_CNT_1) || DEFINED_CONFIG_VDEC_CNT_2345)

/*****************************************************************************
 * VIDEO ENCODER COUNT DEFINITION
 */
#define DEFINED_CONFIG_VENC_CNT_8 defined(CONFIG_VENC_CNT_8)

#define DEFINED_CONFIG_VENC_CNT_78 \
	(defined(CONFIG_VENC_CNT_7) || DEFINED_CONFIG_VENC_CNT_8)

#define DEFINED_CONFIG_VENC_CNT_678 \
	(defined(CONFIG_VENC_CNT_6) || DEFINED_CONFIG_VENC_CNT_78)

#define DEFINED_CONFIG_VENC_CNT_5678 \
	(defined(CONFIG_VENC_CNT_5) || DEFINED_CONFIG_VENC_CNT_678)

#define DEFINED_CONFIG_VENC_CNT_45678 \
	(defined(CONFIG_VENC_CNT_4) || DEFINED_CONFIG_VENC_CNT_5678)

#define DEFINED_CONFIG_VENC_CNT_345678 \
	(defined(CONFIG_VENC_CNT_3) || DEFINED_CONFIG_VENC_CNT_45678)

#define DEFINED_CONFIG_VENC_CNT_2345678 \
	(defined(CONFIG_VENC_CNT_2) || DEFINED_CONFIG_VENC_CNT_345678)

#define DEFINED_CONFIG_VENC_CNT_12345678 \
	(defined(CONFIG_VENC_CNT_1) || DEFINED_CONFIG_VENC_CNT_2345678)

/*****************************************************************************/

#include "vpu_structure.h"
#include "vpu_etc.h"
#include "vpu_dbg.h"

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
#include "smp_driver.h"
#endif

#if defined(CONFIG_TYPE_C5)
#include <video/tcc/TCC_VPUs_CODEC.h>
#else
#include <video/tcc/TCC_VPU_CODEC.h>
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
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2	// HEVC/VP9
#include <video/tcc/TCC_VPU_4K_D2_CODEC.h>
#include <video/tcc/tcc_4k_d2_ioctl.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include <video/tcc/TCC_VP9_CODEC.h>
#include <video/tcc/tcc_vp9_ioctl.h>
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
#include <video/tcc/TCC_VPU_HEVC_ENC_CODEC.h>
#include <video/tcc/tcc_vpu_hevc_enc_ioctl.h>
#endif

//In case of kernel operation (open/close in the kernel level)
// like TMS, open/close works asynchronosly.
#if 0
#define USE_DEV_OPEN_CLOSE_IOCTL
#endif

/* COMMON */
#define IRQ_INT_TYPE    (IRQ_TYPE_EDGE_RISING|IRQF_SHARED)

#define LIST_MAX 10

#define RET4_WAIT   0X00010000
#define RET3        0x00008000
#define RET2        0x00004000
#define RET1        0x00002000
#define RET0        0x00001000

enum list_cmd_type {
	LIST_ADD = 0,
	LIST_DEL,
	LIST_IS_EMPTY,
	LIST_GET_ENTRY
};

struct _vpu_dec_data_t {
	wait_queue_head_t wq;
	spinlock_t lock;
	unsigned int count;
	unsigned char dev_opened;
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	unsigned char dev_file_opened;
#endif
};

struct VpuList {
	struct list_head list;

	unsigned int type;	//encode or decode ?
	int cmd_type;		//vpu command
	long handle;
	void *args;		//vpu argument!!
	struct _vpu_dec_data_t *comm_data;
	int *vpu_result;
};

struct MgrCommData {
	struct list_head main_list;
	struct list_head wait_list;
	struct mutex list_mutex;
	struct mutex io_mutex;
	struct mutex file_mutex;
	unsigned int thread_intr;
	wait_queue_head_t thread_wq;
};

#ifdef CONFIG_VPU_TIME_MEASUREMENT
struct TimeInfo {
	unsigned int print_out_index;
	unsigned int proc_time[30];
	unsigned int proc_base_cnt;	// 0~29
	unsigned int proc_time_30frames;
	// between 1st frame and last one.
	unsigned int accumulated_proc_time;
	unsigned int accumulated_frame_cnt;
};
#endif

struct _mgr_data_t {
//IRQ number and IP base
	unsigned int irq;
	void __iomem *base_addr;
	int check_interrupt_detection;
	int closed[VPU_MAX];
	long handle[VPU_MAX];
	int fileplay_mode[VPU_MAX];

#if defined(CONFIG_PM)
	struct VpuList vList[VPU_MAX];
#endif

	struct MgrCommData comm_data;

	atomic_t oper_intr;
	wait_queue_head_t oper_wq;

	MEM_ALLOC_INFO_t work_memInfo;
	atomic_t dev_opened;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_t dev_file_opened;
#endif
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
	struct TimeInfo iTime[VPU_MAX];
#endif

	bool bVpu_already_proc_force_closed;
};

#if DEFINED_CONFIG_VDEC_CNT_12345
struct _vpu_decoder_data {
	struct miscdevice *misc;
	struct _vpu_dec_data_t vComm_data;
	int gsDecType;
	int gsCodecType;

	int gsIsDiminishedCopy;

	VDEC_INIT_t gsVpuDecInit_Info;
	VDEC_SEQ_HEADER_t gsVpuDecSeqHeader_Info;
	VDEC_SET_BUFFER_t gsVpuDecBuffer_Info;
	VDEC_DECODE_t gsVpuDecInOut_Info;
	VDEC_RINGBUF_GETINFO_t gsVpuDecBufStatus;
	VDEC_RINGBUF_SETBUF_t gsVpuDecBufFill;
	VDEC_RINGBUF_SETBUF_PTRONLY_t gsVpuDecUpdateWP;
	VDEC_GET_VERSION_t gsVpuDecVersion;

#ifdef CONFIG_SUPPORT_TCC_JPU
	JDEC_INIT_t gsJpuDecInit_Info;
#if defined(JPU_C6)
	JDEC_SEQ_HEADER_t gsJpuDecSeqHeader_Info;
#endif
	JPU_SET_BUFFER_t gsJpuDecBuffer_Info;
	JPU_DECODE_t gsJpuDecInOut_Info;
	JPU_GET_VERSION_t gsJpuDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	VPU_4K_D2_INIT_t gsV4kd2DecInit_Info;
	VPU_4K_D2_SEQ_HEADER_t gsV4kd2DecSeqHeader_Info;
	VPU_4K_D2_SET_BUFFER_t gsV4kd2DecBuffer_Info;
	VPU_4K_D2_DECODE_t gsV4kd2DecInOut_Info;
	VPU_4K_D2_RINGBUF_GETINFO_t gsV4kd2DecBufStatus;
	VPU_4K_D2_RINGBUF_SETBUF_t gsV4kd2DecBufFill;
	VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t gsV4kd2DecUpdateWP;
	VPU_4K_D2_GET_VERSION_t gsV4kd2DecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	HEVC_INIT_t gsHevcDecInit_Info;
	HEVC_SEQ_HEADER_t gsHevcDecSeqHeader_Info;
	HEVC_SET_BUFFER_t gsHevcDecBuffer_Info;
	HEVC_DECODE_t gsHevcDecInOut_Info;
	HEVC_RINGBUF_GETINFO_t gsHevcDecBufStatus;
	HEVC_RINGBUF_SETBUF_t gsHevcDecBufFill;
	HEVC_RINGBUF_SETBUF_PTRONLY_t gsHevcDecUpdateWP;
	HEVC_GET_VERSION_t gsHevcDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	VP9_INIT_t gsVp9DecInit_Info;
	VP9_SEQ_HEADER_t gsVp9DecSeqHeader_Info;
	VP9_SET_BUFFER_t gsVp9DecBuffer_Info;
	VP9_DECODE_t gsVp9DecInOut_Info;
	VP9_GET_VERSION_t gsVp9DecVersion;
#endif

	unsigned int gsDecClearBuffer_index;
	int gsCommDecResult;
	unsigned char list_idx;

	struct VpuList vdec_list[LIST_MAX];
};
#endif

#if DEFINED_CONFIG_VENC_CNT_12345678
struct _vpu_encoder_data {
	struct miscdevice *misc;
	struct _vpu_dec_data_t vComm_data;
	int gsEncType;
	int gsCodecType;

	VENC_INIT_t gsVpuEncInit_Info;
	VENC_PUT_HEADER_t gsVpuEncPutHeader_Info;
	VENC_SET_BUFFER_t gsVpuEncBuffer_Info;
	VENC_ENCODE_t gsVpuEncInOut_Info;

#ifdef CONFIG_SUPPORT_TCC_JPU
	JENC_INIT_t gsJpuEncInit_Info;
	JPU_ENCODE_t gsJpuEncInOut_Info;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	VENC_HEVC_INIT_t gsVpuHevcEncInit_Info;
	VENC_HEVC_SET_BUFFER_t gsVpuHevcEncBuffer_Info;
	VENC_HEVC_PUT_HEADER_t gsVpuHevcEncPutHeader_Info;
	VENC_HEVC_ENCODE_t gsVpuHevcEncInOut_Info;
#endif

	int gsCommEncResult;
	unsigned char list_idx;

	struct VpuList venc_list[LIST_MAX];
};
#endif

#endif /*VPU_COMM_H*/
