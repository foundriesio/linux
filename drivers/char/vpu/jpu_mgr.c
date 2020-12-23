/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_JPU

#include <asm/system_info.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <soc/tcc/pmap.h>

#include "vpu_buffer.h"
#include "vpu_comm.h"
#include "vpu_devices.h"
#include "jpu_mgr_sys.h"
#include "jpu_mgr.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR: " msg)
#define cmdk(msg...)     V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR [Cmd]: " msg)
#define err(msg...)      V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR [Err]: " msg)

#define JPU_REGISTER_DUMP

#if 0 //For test purpose!!
#define FORCED_ERROR
#endif
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

// Control only once!!
static struct _mgr_data_t jmgr_data;
static struct task_struct *kidle_task;	// = NULL;

extern int tcc_jpu_dec(int Op, codec_handle_t *pHandle, void *pParam1,
		       void *pParam2);
#if DEFINED_CONFIG_VENC_CNT_12345678
extern int tcc_jpu_enc(int Op, codec_handle_t *pHandle, void *pParam1,
			void *pParam2);
#endif

static int (*gs_fpTccJpuDec)(int Op, codec_handle_t *pHandle, void *pParam1,
			      void *pParam2);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)

#if 0
#define DEBUG_TRACE_TA
#endif
#ifdef DEBUG_TRACE_TA
#define tracetee(msg...) V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR: " msg)
#else
#define tracetee(msg...)
#endif

// kernel client application area
#include <linux/slab.h>		// kmalloc
struct JpuInstKernel {
	uint32_t pRegBaseAddr;	// physical addr
	uint32_t u32RegBaseAddrSize;	// physical addr size
	codec_handle_t hJpuDecHandle;
	jpu_dec_init_t stJpuDecInit;
	jpu_dec_initial_info_t stJpuDecInitialInfo;
	jpu_dec_buffer_t stJpuDecBuffer;
	jpu_dec_input_t stJpuDecInput;
	jpu_dec_output_t stJpuDecOutput;
	int iSeqHeaderSize;
	char szVersion[64];
	char szBuildData[32];
};

#define JPU_REG_BASE_ADDR (0x15180000)

int tcc_jpu_dec_internal(int Op, codec_handle_t *pHandle, void *pParam1,
			void *pParam2)
{

	int ret = 0;
	struct JpuInstKernel *pstInst = NULL;

	tracetee(
	"[KERNEL(%s)] OpCode = %d, %x, pHandle = %x, %x\n",
	    __func__, Op, Op, pHandle, *pHandle);

	if (pHandle != NULL)
		pstInst = (struct JpuInstKernel *)((codec_handle_t)*pHandle);

	switch (Op) {
	case JPU_DEC_INIT:
	{
		tracetee("[KERNEL(JPU_DEC_INIT) %s] pHandle = %p, %x\n",
		 __func__, pHandle, (uint32_t)(*pHandle));

		pstInst = kmalloc(sizeof(struct JpuInstKernel),
		  GFP_KERNEL);
		if (pstInst == NULL)
			tracetee("[KERNEL(JPU_DEC_INIT)] pstInst is NULL\n");

		memset(pstInst, 0, sizeof(struct JpuInstKernel));
		memcpy(&pstInst->stJpuDecInit,
		       (jpu_dec_init_t *) pParam1,
		       sizeof(jpu_dec_init_t));

		pstInst->pRegBaseAddr = JPU_REG_BASE_ADDR;
		pstInst->u32RegBaseAddrSize = 16 * 1024;

		// you need re-mapping into TA
		pstInst->stJpuDecInit.m_Memset = NULL;
		pstInst->stJpuDecInit.m_Memcpy = NULL;
		pstInst->stJpuDecInit.m_reg_read = NULL;
		pstInst->stJpuDecInit.m_reg_write = NULL;

		pstInst->stJpuDecInit.m_Interrupt = NULL;
		pstInst->stJpuDecInit.m_Ioremap = NULL;
		pstInst->stJpuDecInit.m_Iounmap = NULL;

		if (pParam2 != NULL) {
			tracetee("[KERNEL(JPU_DEC_INIT)] trace\n");
			memcpy(&pstInst->stJpuDecInitialInfo,
			       (jpu_dec_initial_info_t *) pParam2,
			       sizeof(jpu_dec_initial_info_t));
		}

#ifdef DEBUG_TRACE_TA
		{
			jpu_dec_init_t *pinit =
			    (jpu_dec_init_t *)pParam1;
			unsigned char *ptr =
			    pinit->m_BitstreamBufAddr[VA];

			tracetee(
			"[KERNEL(JPU_DEC_INIT):%s] BITSTRAM_DATA %x %x =========================",
			 __func__,
			    pstInst->stJpuDecInit.m_BitstreamBufAddr[PA],
			    pstInst->stJpuDecInit.m_BitstreamBufAddr[VA]);
			tracetee("%02x %02x %02x %02x %02x %02x %02x %02x\n",
			    ptr[0], ptr[1], ptr[2], ptr[3],
			    ptr[4], ptr[5], ptr[6], ptr[7]);
		}
#endif

		// New instance for TA
		ret = jpu_optee_command(Op, (void *)pstInst,
		 sizeof(struct JpuInstKernel));

		tracetee(
		    "[KERNEL(JPU_DEC_INIT):%s] Instance Handle address = %p %p, pstInst = %p\n",
		     __func__, pHandle, *pHandle, pstInst);
		*pHandle = (codec_handle_t)pstInst;
	}
	break;

#if defined(JPU_C6)
	case JPU_DEC_SEQ_HEADER:
	{
		bool bDiminishInputCopy = (pstInst->stJpuDecInit.m_uiDecOptFlags
			 & (1<<26)) ? true : false;

		tracetee(
		    "[KERNEL(JPU_DEC_SEQ_HEADER) %s] 002 pHandle = %p, %x\n",
		    __func__, pHandle, (uint32_t) (*pHandle));

		if (bDiminishInputCopy) {
			memcpy(&pstInst->stJpuDecInput,
			       (jpu_dec_input_t *) pParam1,
			       sizeof(jpu_dec_input_t));
			pstInst->iSeqHeaderSize =
			  (int)pstInst->stJpuDecInput.m_iBitstreamDataSize;
			tracetee(
			    "[KERNEL(JPU_DEC_SEQ_HEADER) %s] DiminishInputCopy[O] - seq.header size = %d, %x\n",
			     __func__, pstInst->iSeqHeaderSize,
			     pstInst->iSeqHeaderSize);
		} else {
			pstInst->iSeqHeaderSize = (int)pParam1;
			tracetee(
			    "[KERNEL(JPU_DEC_SEQ_HEADER) %s] DiminishInputCopy[X] - seq.header size = %d, %x\n",
			     __func__, pstInst->iSeqHeaderSize,
			     pstInst->iSeqHeaderSize);
		}

		ret = jpu_optee_command(Op, (void *)pstInst,
			 sizeof(struct JpuInstKernel));

		memcpy((jpu_dec_initial_info_t *)pParam2,
		     &pstInst->stJpuDecInitialInfo,
		     sizeof(jpu_dec_initial_info_t));
		tracetee(
		    "[KERNEL(JPU_DEC_SEQ_HEADER) %s] [INITIAL_INFO] W x H (%d x %d)",
		    __func__, pstInst->stJpuDecInitialInfo.m_iPicWidth,
		    pstInst->stJpuDecInitialInfo.m_iPicHeight);
		}
		break;

	case JPU_DEC_GET_ROI_INFO:
		break;
	#endif

	case JPU_DEC_REG_FRAME_BUFFER:
	{
		tracetee(
		"[KERNEL(JPU_DEC_REG_FRAME_BUFFER) %s] pHandle = %p, %x\n",
		    __func__, pHandle, (uint32_t)(*pHandle));

		memcpy(&pstInst->stJpuDecBuffer,
		     (jpu_dec_buffer_t *) pParam1,
		     sizeof(jpu_dec_buffer_t));

		ret =
		    jpu_optee_command(Op, (void *)pstInst,
				    sizeof(struct JpuInstKernel));
	}
	break;

	case JPU_DEC_DECODE:
	{
		tracetee(
		"[KERNEL(JPU_DEC_DECODE) xxxxxxxxx %s] pHandle = %p, %x\n",
		    __func__, pHandle, (uint32_t)(*pHandle));

		memcpy(&pstInst->stJpuDecInput,
		 (jpu_dec_input_t *)pParam1, sizeof(jpu_dec_input_t));
		memcpy(&pstInst->stJpuDecOutput,
		 (jpu_dec_output_t *)pParam2, sizeof(jpu_dec_output_t));

		#ifdef DEBUG_TRACE_TA
		tracetee(
		"[KERNEL(JPU_DEC_DECODE) BITSTRAM_DATA %x, Size = %d=========================",
		    pstInst->stJpuDecInput.m_BitstreamDataAddr[VA],
		    pstInst->stJpuDecInput.m_iBitstreamDataSize);

		{
			unsigned char *ptr =
			  pstInst->stJpuDecInput.m_BitstreamDataAddr[VA];

			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
		}
#endif
		ret =
		    jpu_optee_command(Op, (void *)pstInst,
			    sizeof(struct JpuInstKernel));

		memcpy((jpu_dec_output_t *) pParam2,
		       &pstInst->stJpuDecOutput,
		       sizeof(jpu_dec_output_t));
		tracetee(
		  "[[KERNEL(JPU_DEC_DECODE)OUT][W:%d][H:%d][DecStatus:%d][ConsumedBytes:%d][ErrMBs:%d][DispOutIdx:%d]",
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iWidth,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iHeight,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iDecodingStatus,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iConsumedBytes,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iNumOfErrMBs,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iDispOutIdx);

	}
	break;

	case JPU_DEC_CLOSE:
		{
			tracetee(
			    "[KERNEL(JPU_DEC_CLOSE) %s] pHandle = %p, %x\n",
			     __func__, pHandle, (uint32_t) (*pHandle));

			ret =
			    jpu_optee_command(Op, (void *)pstInst,
					      sizeof(struct JpuInstKernel));

			kfree(pstInst);
			pstInst = NULL;
			pHandle = NULL;
			tracetee("free jpu instance !!");
		}
		break;

	case JPU_CODEC_GET_VERSION:
		{
			tracetee(
			    "[KERNEL(JPU_CODEC_GET_VERSION) %s] pHandle = %p, %x\n",
			     __func__, pHandle, (uint32_t) (*pHandle));

			ret =
			    jpu_optee_command(Op, (void *)pstInst,
					      sizeof(struct JpuInstKernel));

			if (pParam1 == NULL && pParam2 == NULL) {
				pParam1 = pstInst->szVersion;
				pParam2 = pstInst->szBuildData;
			} else {
				memcpy(pParam1, pstInst->szVersion,
				       sizeof(pstInst->szVersion));
				memcpy(pParam2, pstInst->szBuildData,
				       sizeof(pstInst->szBuildData));
			}

			tracetee(
			    "[KERNEL(JPU_CODEC_GET_VERSION) %s] Version = %s, %s\n",
			     __func__, pParam1, pstInst->szVersion);
			tracetee(
			    "[KERNEL(JPU_CODEC_GET_VERSION) %s] BuildData = %s, %s\n",
			     __func__, pParam2, pstInst->szBuildData);
		}
		break;

	default:
			err("Invalid Operation = %d(0x%x)", __func__, Op, Op);
		break;
	}

	return ret;
}
#endif /*CONFIG_ARCH_TCC899X || CONFIG_ARCH_TCC901X*/

int jmgr_opened(void)
{
	if (atomic_read(&jmgr_data.dev_opened) == 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(jmgr_opened);

int jmgr_get_close(vputype type)
{
	return jmgr_data.closed[type];
}

int jmgr_get_alive(void)
{
	return atomic_read(&jmgr_data.dev_opened);
}

int jmgr_set_close(vputype type, int value, int bfreemem)
{
	if (jmgr_get_close(type) == value) {
		dprintk(" %d was already set into %d.", type, value);
		return -1;
	}

	jmgr_data.closed[type] = value;
	if (value == 1) {
		jmgr_data.handle[type] = 0x00;

		if (bfreemem)
			vmem_proc_free_memory(type);
	}

	return 0;
}

static void _jmgr_close_all(int bfreemem)
{
	jmgr_set_close(VPU_DEC, 1, bfreemem);
	jmgr_set_close(VPU_DEC_EXT, 1, bfreemem);
	jmgr_set_close(VPU_DEC_EXT2, 1, bfreemem);
	jmgr_set_close(VPU_DEC_EXT3, 1, bfreemem);
	jmgr_set_close(VPU_DEC_EXT4, 1, bfreemem);
}

int jmgr_process_ex(struct VpuList *cmd_list, vputype type, int Op, int *result)
{
	if (atomic_read(&jmgr_data.dev_opened) == 0)
		return 0;

	err("\n process_ex %d - 0x%x\n", type, Op);

	if (!jmgr_get_close(type)) {
		cmd_list->type = type;
		cmd_list->cmd_type = Op;
		cmd_list->handle = jmgr_data.handle[type];
		cmd_list->args = NULL;
		cmd_list->comm_data = NULL;
		cmd_list->vpu_result = result;
		jmgr_list_manager(cmd_list, LIST_ADD);

		return 1;
	}

	return 1;
}

static int _jmgr_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	int timeout = 200;

	if (jmgr_data.current_resolution > 1920 * 1080)
		timeout = 5000;

	if (jmgr_data.check_interrupt_detection) {
		if (atomic_read(&jmgr_data.oper_intr) > 0) {
			detailk("Success 1: jpu operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			ret =
			    wait_event_interruptible_timeout(
				    jmgr_data.oper_wq,
				    atomic_read(&jmgr_data.oper_intr) > 0,
				    msecs_to_jiffies(timeout));

			if (atomic_read(&jmgr_data.oper_intr) > 0) {
				detailk("Success 2: jpu operation!!");

#if defined(FORCED_ERROR)
				if (forced_error_count-- <= 0) {
					ret_code = RETCODE_CODEC_EXIT;
					forced_error_count = FORCED_ERR_CNT;
					vetc_dump_reg_all(jmgr_data.base_addr,
					  "jmgr_internal_handler force-timed_out");
				} else {
					ret_code = RETCODE_SUCCESS;
				}
#else
				ret_code = RETCODE_SUCCESS;
#endif
			} else {
				err(
				"[CMD 0x%x][%d]: jpu timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d\n",
				jmgr_data.current_cmd, ret, timeout,
				atomic_read(&jmgr_data.oper_intr),
				jmgr_data.nDecode_Cmd,
				jmgr_data.szFrame_Len);
				vetc_dump_reg_all(jmgr_data.base_addr,
					  "jmgr_internal_handler timed_out");
				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&jmgr_data.oper_intr, 0);
		jmgr_status_clear(jmgr_data.base_addr);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt option=%d, ev=%d)",
		jmgr_data.check_interrupt_detection,
		ret_code);

	return ret_code;
}

static int _jmgr_convert_returnType(int err)
{
	int ret;

	if (err >= JPG_RET_INVALID_HANDLE
	    && err <= JPG_RET_NOT_INITIALIZED) {
		ret = (err - 2);
	} else {
		switch (err) {
		case JPG_RET_BIT_EMPTY:
			ret = 50;
			break;
		case JPG_RET_EOS:
			ret = 51;
			break;
		case JPG_RET_INSUFFICIENT_BITSTREAM_BUF:
			ret = RETCODE_INSUFFICIENT_BITSTREAM_BUF;
			break;
		case JPG_RET_CODEC_FINISH:
			ret = RETCODE_CODEC_FINISH;
			break;
		default:
			ret = err;
			break;
		}
	}

	return ret;
}

static int _jmgr_process(vputype type, int cmd, long pHandle, void *args)
{
	int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	struct timeval t1, t2;
	int time_gap_ms = 0;
#endif

	jmgr_data.check_interrupt_detection = 0;
	jmgr_data.current_cmd = cmd;

	if (type < VPU_ENC) {
		if (cmd != VPU_DEC_INIT && cmd != VPU_DEC_INIT_KERNEL) {
			if (jmgr_get_close(type)
			    || jmgr_data.handle[type] == 0x00)
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
		}

		if (cmd != VPU_DEC_BUF_FLAG_CLEAR && cmd != VPU_DEC_DECODE
		    && cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL
		    && cmd != VPU_DEC_DECODE_KERNEL) {
			cmdk("Decoder(%d), command: 0x%x\n", type, cmd);
		}

		switch (cmd) {
		case VPU_DEC_INIT:
		case VPU_DEC_INIT_KERNEL:
		{
			JDEC_INIT_t *arg = (JDEC_INIT_t *)args;
			int retDec;

			jmgr_data.handle[type] = 0x00;

			arg->gsJpuDecInit.m_RegBaseVirtualAddr
			 = (codec_addr_t)jmgr_data.base_addr;
			arg->gsJpuDecInit.m_Memcpy = vetc_memcpy;
			arg->gsJpuDecInit.m_Memset
			 = (void  (*) (void *, int, unsigned int, unsigned int))
			    vetc_memset;
			arg->gsJpuDecInit.m_Interrupt
			 = (int  (*) (void))_jmgr_internal_handler;
			arg->gsJpuDecInit.m_reg_read
			 = (unsigned int (*)(void *, unsigned int))
			    vetc_reg_read;
			arg->gsJpuDecInit.m_reg_write
			 = (void (*)(void *, unsigned int, unsigned int))
			    vetc_reg_write;

			jmgr_data.check_interrupt_detection = 1;
			jmgr_data.bDiminishInputCopy
			 = (arg->gsJpuDecInit.m_uiDecOptFlags
			    & (1<<26)) ? true : false;

			gs_fpTccJpuDec = tcc_jpu_dec;
			dprintk("Dec :: loading JPU ...");

#if defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
			if (arg->gsJpuDecInit.m_uiDecOptFlags
			    & (1<<30)) {
				dprintk("Dec :: USE OPTEE_JPU\n");
				gs_fpTccJpuDec = tcc_jpu_dec_internal;
			}
#endif

			if (vmem_alloc_count(type) <= 0) {
				dprintk(
				"Dec-%d ######################## No Buffer allocation\n",
				    type);
				return RETCODE_FAILURE;
			}

#if defined(JPU_C5)
			dprintk(
			"Dec :: Init In => Reg(0x%x/0x%x), Stream(0x%x/0x%x, 0x%x)",
			    jmgr_data.base_addr,
			    arg->gsJpuDecInit.m_RegBaseVirtualAddr,
			    arg->gsJpuDecInit.m_BitstreamBufAddr[PA],
			    arg->gsJpuDecInit.m_BitstreamBufAddr[VA],
			    arg->gsJpuDecInit.m_iBitstreamBufSize);

			dprintk(
			"Dec :: Init In => rotation(%d/%d), mirror(%d/%d), Interleave: %d\n",
			    arg->gsJpuDecInit.m_iRot_angle,
			    arg->gsJpuDecInit.m_iRot_enalbe,
			    arg->gsJpuDecInit.m_iMirrordir,
			    arg->gsJpuDecInit.m_iMirror_enable,
			    arg->gsJpuDecInit.m_bCbCrInterleaveMode);

			retDec = gs_fpTccJpuDec(JPU_DEC_INIT,
				    (void *)(&arg->gsJpuDecHandle),
				    (void *)(&arg->gsJpuDecInit),
				    (void *)(&arg->gsJpuDecInitialInfo));
			jmgr_data.current_resolution
			 = arg->gsJpuDecInitialInfo.m_iPicWidth
			   * arg->gsJpuDecInitialInfo.m_iPicHeight;
#else
			dprintk(
			"Dec :: Init In => Handle(0x%x) Reg(0x%x/0x%x), Stream(0x%x/0x%x, 0x%x) Interleave: %d\n",
			    arg->gsJpuDecHandle,
			    jmgr_data.base_addr,
			    arg->gsJpuDecInit.m_RegBaseVirtualAddr,
			    arg->gsJpuDecInit.m_BitstreamBufAddr[PA],
			    arg->gsJpuDecInit.m_BitstreamBufAddr[VA],
			    arg->gsJpuDecInit.m_iBitstreamBufSize,
			    arg->gsJpuDecInit.m_iCbCrInterleaveMode);

			retDec = gs_fpTccJpuDec(JPU_DEC_INIT,
				    (void *)(&arg->gsJpuDecHandle),
				    (void *)(&arg->gsJpuDecInit),
				    (void *)NULL);
#endif

			if (retDec != RETCODE_SUCCESS) {
				dprintk("Dec :: Init Done with ret(0x%x)",
				     retDec);
				if (retDec != JPG_RET_CODEC_EXIT) {
					vetc_dump_reg_all(
					  jmgr_data.base_addr, "init failure");
				}
			}

			ret = _jmgr_convert_returnType(retDec);
			if (ret != RETCODE_CODEC_EXIT
			    && arg->gsJpuDecHandle != 0) {
				jmgr_data.handle[type]
				    = arg->gsJpuDecHandle;
				jmgr_set_close(type, 0, 0);
				cmdk("Dec :: jmgr_data.handle = 0x%x\n",
				     arg->gsJpuDecHandle);
			} else {
				jmgr_set_close(type, 0, 0);
				jmgr_set_close(type, 1, 1);
			}
			dprintk("Dec :: Init Done Handle(0x%x)",
				arg->gsJpuDecHandle);

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			jmgr_data.iTime[type].print_out_index
			 = jmgr_data.iTime[type].proc_base_cnt = 0;
			jmgr_data.iTime[type].accumulated_proc_time
			 = jmgr_data.iTime[type].accumulated_frame_cnt = 0;
			jmgr_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_DEC_SEQ_HEADER:
		case VPU_DEC_SEQ_HEADER_KERNEL:
#if defined(JPU_C6)
		{
			int iSize;
			void *arg = args;
			int retDec;
			jpu_dec_initial_info_t *gsJpuDecInitialInfo;

			gsJpuDecInitialInfo
			 = jmgr_data.bDiminishInputCopy
			 ? &((JPU_DECODE_t *)arg)->gsJpuDecInitialInfo
			 : &((JDEC_SEQ_HEADER_t *)arg)->gsJpuDecInitialInfo;
			jmgr_data.szFrame_Len = iSize
			 = jmgr_data.bDiminishInputCopy
			 ? (int)((JPU_DECODE_t *)arg)->gsJpuDecInput
			    .m_iBitstreamDataSize
			 : (int)((JDEC_SEQ_HEADER_t *)arg)->stream_size;

			jmgr_data.check_interrupt_detection = 1;
			jmgr_data.nDecode_Cmd = 0;
			dprintk(
			"Dec :: JPU_DEC_SEQ_HEADER in :: Handle(0x%x) size(%d)",
			    pHandle, iSize);
			retDec = gs_fpTccJpuDec(JPU_DEC_SEQ_HEADER,
					(codec_handle_t *)&pHandle,
					(jmgr_data.bDiminishInputCopy ?
						(void *)(&((JPU_DECODE_t *)arg)
							->gsJpuDecInput)
							: (void *)(long)iSize),
					(void *)gsJpuDecInitialInfo);

			ret = _jmgr_convert_returnType(retDec);
			dprintk(
			"Dec :: JPU_DEC_SEQ_HEADER out 0x%x :: res info(%d x %d), src_format(%d), Error_reason(%d), minFB(%d)",
			  ret, gsJpuDecInitialInfo->m_iPicWidth,
			  gsJpuDecInitialInfo->m_iPicHeight,
			  gsJpuDecInitialInfo->m_iSourceFormat,
			  gsJpuDecInitialInfo->m_iErrorReason,
			  gsJpuDecInitialInfo->m_iMinFrameBufferCount);
			jmgr_data.current_resolution
			 = gsJpuDecInitialInfo->m_iPicWidth
			    * gsJpuDecInitialInfo->m_iPicHeight;
		}
#else
		{
			err("Dec :: not supported command(0x%x)", cmd);
			return 0x999;
		}
#endif
		break;

		case VPU_DEC_REG_FRAME_BUFFER:
		case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
		{
			JPU_SET_BUFFER_t *arg = (JPU_SET_BUFFER_t *)args;
			int retDec;

#if defined(JPU_C5)
			dprintk(
			"Dec :: JPU_DEC_REG_FRAME_BUFFER in :: scale[%d], addr[0x%x/0x%x]",
			  arg->gsJpuDecBuffer.m_iJPGScaleRatio,
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[PA],
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[VA]);
#else
			dprintk(
			"Dec :: JPU_DEC_REG_FRAME_BUFFER in :: count[%d], scale[%d], addr[0x%x/0x%x], Reserved[8] = 0x%x\n",
			  arg->gsJpuDecBuffer.m_iFrameBufferCount,
			  arg->gsJpuDecBuffer.m_iJPGScaleRatio,
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[PA],
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[VA],
			  arg->gsJpuDecBuffer.m_Reserved[8]);
#endif

			retDec
			 = gs_fpTccJpuDec(
			    JPU_DEC_REG_FRAME_BUFFER,
			    (codec_handle_t *)&pHandle,
			    (void *)(&arg->gsJpuDecBuffer),
			    (void *)NULL);
			ret = _jmgr_convert_returnType(retDec);
			dprintk(
			"Dec :: JPU_DEC_REG_FRAME_BUFFER out\n");
		}
		break;

		case VPU_DEC_DECODE:
		case VPU_DEC_DECODE_KERNEL:
		{
			JPU_DECODE_t *arg = (JPU_DECODE_t *)args;
			int retDec;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			do_gettimeofday(&t1);
#endif

			jmgr_data.szFrame_Len
			 = arg->gsJpuDecInput.m_iBitstreamDataSize;
#if defined(JPU_C6)
			dprintk(
			"Dec :: Dec In => 0x%x - 0x%x, 0x%x\n",
			  arg->gsJpuDecInput.m_BitstreamDataAddr[PA],
			  arg->gsJpuDecInput.m_BitstreamDataAddr[VA],
			  arg->gsJpuDecInput.m_iBitstreamDataSize);
#else
			dprintk(
			"Dec :: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, toggle: %d\n",
			  arg->gsJpuDecInput.m_BitstreamDataAddr[PA],
			  arg->gsJpuDecInput.m_BitstreamDataAddr[VA],
			  arg->gsJpuDecInput.m_iBitstreamDataSize,
			  arg->gsJpuDecInput.m_FrameBufferStartAddr[PA],
			  arg->gsJpuDecInput.m_FrameBufferStartAddr[VA],
					arg->gsJpuDecInput.m_iLooptogle);
#endif
			jmgr_data.check_interrupt_detection = 1;
			retDec
			 = gs_fpTccJpuDec(JPU_DEC_DECODE,
			    (codec_handle_t *)&pHandle,
			    (void *)(&arg->gsJpuDecInput),
			    (void *)(&arg->gsJpuDecOutput));
			ret = _jmgr_convert_returnType(retDec);

			dprintk(
			"Dec :: Dec Out => %d x %d, status(%d), Consumed(%d), Err(%d)",
			  arg->gsJpuDecOutput.m_DecOutInfo.m_iWidth,
			  arg->gsJpuDecOutput.m_DecOutInfo.m_iHeight,
			  arg->gsJpuDecOutput.m_DecOutInfo.m_iDecodingStatus,
			  arg->gsJpuDecOutput.m_DecOutInfo.m_iConsumedBytes,
			  arg->gsJpuDecOutput.m_DecOutInfo.m_iNumOfErrMBs);

			dprintk(
			"Dec :: Dec Out => 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x\n",
			  (unsigned long)arg->gsJpuDecOutput.m_pCurrOut[PA][0],
			  (unsigned long)arg->gsJpuDecOutput.m_pCurrOut[PA][1],
			  (unsigned long)arg->gsJpuDecOutput.m_pCurrOut[PA][2],
			  (unsigned long)arg->gsJpuDecOutput.m_pCurrOut[VA][0],
			  (unsigned long)arg->gsJpuDecOutput.m_pCurrOut[VA][1],
			  (unsigned long)arg->gsJpuDecOutput.m_pCurrOut[VA][2]);

			if (arg->gsJpuDecOutput.m_DecOutInfo.m_iDecodingStatus
			  == VPU_DEC_BUF_FULL) {
				err("Buffer full\n");
			}
			jmgr_data.nDecode_Cmd++;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			do_gettimeofday(&t2);
#endif
		}
		break;

		case VPU_DEC_CLOSE:
		case VPU_DEC_CLOSE_KERNEL:
		{
			int retDec;

			jmgr_data.check_interrupt_detection = 1;
			retDec =
			    gs_fpTccJpuDec(JPU_DEC_CLOSE,
					   (codec_handle_t *) &pHandle,
					   (void *)NULL,
					   (void *)NULL);
			ret = _jmgr_convert_returnType(retDec);
			dprintk("Dec :: JPU_DEC_CLOSED !!");

			jmgr_set_close(type, 1, 1);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		case VPU_CODEC_GET_VERSION_KERNEL:
		{
			JPU_GET_VERSION_t *arg = (JPU_GET_VERSION_t *)args;
			int retDec;

			jmgr_data.check_interrupt_detection = 1;

			retDec = gs_fpTccJpuDec(JPU_CODEC_GET_VERSION,
			 (codec_handle_t *) &pHandle,
			 arg->pszVersion,
			 arg->pszBuildData);
			ret = _jmgr_convert_returnType(retDec);
			dprintk("Dec :: version : %s, build : %s\n",
				arg->pszVersion, arg->pszBuildData);
		}
		break;

		default:
			err("Dec :: not supported command(0x%x)", cmd);
			return 0x999;
		}
	}
#if DEFINED_CONFIG_VENC_CNT_12345678
	else {
		switch (cmd) {
		case VPU_ENC_INIT:
		{
			JENC_INIT_t *arg = (JENC_INIT_t *)args;
			int retEnc;

			jmgr_data.handle[type] = 0x00;

			arg->gsJpuEncInit.m_RegBaseVirtualAddr
			 = (codec_addr_t)jmgr_data.base_addr;
			arg->gsJpuEncInit.m_Memcpy
			 = vetc_memcpy;
			arg->gsJpuEncInit.m_Memset
			 = (void (*) (void *, int, unsigned int, unsigned int))
			     vetc_memset;
			arg->gsJpuEncInit.m_Interrupt
			 = (int (*) (void))_jmgr_internal_handler;
			arg->gsJpuEncInit.m_reg_read
			 = (unsigned int (*)(void *, unsigned int))
			     vetc_reg_read;
			arg->gsJpuEncInit.m_reg_write
			 = (void (*)(void*, unsigned int, unsigned int))
			    vetc_reg_write;

			jmgr_data.check_interrupt_detection = 1;
#if defined(JPU_C6)
			dprintk(
			  "Enc :: Init In => Reg(0x%x/0x%x), Src(%d x %d, %d), Q(%d), Stream(0x%x/0x%x, 0x%x), Inter(%d), Option(0x%x)",
			  jmgr_data.base_addr,
			  arg->gsJpuEncInit.m_RegBaseVirtualAddr,
			  arg->gsJpuEncInit.m_iPicWidth,
			  arg->gsJpuEncInit.m_iPicHeight,
			  arg->gsJpuEncInit.m_iSourceFormat,
			  arg->gsJpuEncInit.m_iEncQuality,
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[PA],
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[VA],
			  arg->gsJpuEncInit.m_iBitstreamBufferSize,
			  arg->gsJpuEncInit.m_iCbCrInterleaveMode,
			  arg->gsJpuEncInit.m_uiEncOptFlags);
#else
			dprintk(
			  "Enc :: Init In => Reg(0x%x/0x%x), Src(%d x %d, %d), Stream(0x%x/0x%x, 0x%x), rotation(%d), Inter(%d), Option(0x%x)",
			  jmgr_data.base_addr,
			  arg->gsJpuEncInit.m_RegBaseVirtualAddr,
			  arg->gsJpuEncInit.m_iPicWidth,
			  arg->gsJpuEncInit.m_iPicHeight,
			  arg->gsJpuEncInit.m_iMjpg_sourceFormat,
			  arg->gsJpuEncInit.m_iEncQuality,
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[PA],
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[VA],
			  arg->gsJpuEncInit.m_iBitstreamBufferSize,
			  arg->gsJpuEncInit.m_iRotMode,
			  arg->gsJpuEncInit.m_bCbCrInterleaveMode,
			  arg->gsJpuEncInit.m_uiEncOptFlags);
#endif
			jmgr_data.szFrame_Len
			 = arg->gsJpuEncInit.m_iPicWidth
			  * arg->gsJpuEncInit.m_iPicHeight * 3 / 2;
			jmgr_data.current_resolution
			 = arg->gsJpuEncInit.m_iPicWidth
			  * arg->gsJpuEncInit.m_iPicHeight;

			retEnc
			 = tcc_jpu_enc(JPU_ENC_INIT,
			    (void *)(&arg->gsJpuEncHandle),
			    (void *)(&arg->gsJpuEncInit), (void *)NULL);
			if (retEnc != RETCODE_SUCCESS) {
				dprintk(
				    "## Enc :: Init Done with ret(0x%x)",
				    retEnc);
				if (retEnc != RETCODE_CODEC_EXIT) {
					vetc_dump_reg_all(
					  jmgr_data.base_addr, "init failure");
				}
			}

			ret = _jmgr_convert_returnType(retEnc);
			if (ret != RETCODE_CODEC_EXIT
			    && arg->gsJpuEncHandle != 0) {
				jmgr_data.handle[type]
				    = arg->gsJpuEncHandle;
				jmgr_set_close(type, 0, 0);
				cmdk("Enc :: jmgr_data.handle = 0x%x\n",
				     arg->gsJpuEncHandle);
			}
			dprintk("Enc :: Init Done Handle(0x%x)",
				  arg->gsJpuEncHandle);
			jmgr_data.nDecode_Cmd = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
			jmgr_data.iTime[type].print_out_index
			 = jmgr_data.iTime[type].proc_base_cnt = 0;
			jmgr_data.iTime[type].accumulated_proc_time
			 = jmgr_data.iTime[type].accumulated_frame_cnt
			 = 0;
			jmgr_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_ENC_ENCODE:
			{
				JPU_ENCODE_t *arg = (JPU_ENCODE_t *)args;
				int retEnc;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
				do_gettimeofday(&t1);
#endif

				dprintk(
				  "Enc :: Enc In => Handle(0x%x), YUV(0x%x - 0x%x - 0x%x) -> BitStream(0x%x - 0x%x / 0x%x)",
				  pHandle,
				  arg->gsJpuEncInput.m_PicYAddr,
				  arg->gsJpuEncInput.m_PicCbAddr,
				  arg->gsJpuEncInput.m_PicCrAddr,
				  arg->gsJpuEncInput.m_BitstreamBufferAddr[PA],
				  arg->gsJpuEncInput.m_BitstreamBufferAddr[VA],
				  arg->gsJpuEncInput.m_iBitstreamBufferSize);

				jmgr_data.check_interrupt_detection = 1;
				retEnc =
				 tcc_jpu_enc(JPU_ENC_ENCODE,
				  (codec_handle_t *) &pHandle,
				   (void *)(&arg->gsJpuEncInput),
				    (void *)(&arg->gsJpuEncOutput));
				ret = _jmgr_convert_returnType(retEnc);

#if defined(JPU_C5)
				dprintk(
				"Enc :: Enc Out => (0x%x/0x%x), Size(%d/%d)",
				     arg->gsJpuEncOutput.m_BitstreamOut[0],
				     arg->gsJpuEncOutput.m_BitstreamOut[1],
				     arg->gsJpuEncOutput.m_iHeaderOutSize,
				     arg->gsJpuEncOutput.m_iBitstreamOutSize);
#else
				dprintk(
				"Enc :: Enc Out => ret(%d) (0x%x/0x%x), Size(%d/%d)",
				     ret, arg->gsJpuEncOutput.m_BitstreamOut[0],
				     arg->gsJpuEncOutput.m_BitstreamOut[1],
				     arg->gsJpuEncOutput.m_iBitstreamHeaderSize,
				     arg->gsJpuEncOutput.m_iBitstreamOutSize);
#endif
				jmgr_data.nDecode_Cmd++;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
				do_gettimeofday(&t2);
#endif
			}
			break;

		case VPU_ENC_CLOSE:
		{
			int retEnc;

			jmgr_data.check_interrupt_detection = 1;
			retEnc
			 = tcc_jpu_enc(JPU_ENC_CLOSE,
			    (codec_handle_t *) &pHandle,
			    (void *)NULL, (void *)NULL);
			ret = _jmgr_convert_returnType(retEnc);
			dprintk("Enc :: JPU_ENC_CLOSED!!");

			jmgr_set_close(type, 1, 1);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		{
			JPU_GET_VERSION_t *arg = (JPU_GET_VERSION_t *)args;
			int retEnc;

			jmgr_data.check_interrupt_detection = 1;
			retEnc = tcc_jpu_enc(JPU_CODEC_GET_VERSION,
					(codec_handle_t *) &pHandle,
					arg->pszVersion,
					arg->pszBuildData);
			ret = _jmgr_convert_returnType(retEnc);
			dprintk("Enc :: version : %s, build : %s\n",
				arg->pszVersion, arg->pszBuildData);
		}
		break;

		default:
			{
				err("Enc :: not supported command(0x%x)", cmd);
				return 0x999;
			}
			break;
		}
	}
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	time_gap_ms = vetc_GetTimediff_ms(t1, t2);

	if (cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE) {
		jmgr_data.iTime[type].accumulated_frame_cnt++;
		jmgr_data.iTime[type].proc_time[
		  jmgr_data.iTime[type].proc_base_cnt]
		 = time_gap_ms;
		jmgr_data.iTime[type].proc_time_30frames
		 += time_gap_ms;
		jmgr_data.iTime[type].accumulated_proc_time
		 += time_gap_ms;
		if (jmgr_data.iTime[type].proc_base_cnt != 0
		    && jmgr_data.iTime[type].proc_base_cnt % 29 == 0) {
			V_DBG(VPU_DBG_PERF,
			"JPU[%d] :: Dec[%4d] time %2d.%2d / %2d.%2d ms: %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d\n",
			    type,
			    jmgr_data.iTime[type].print_out_index,
			    jmgr_data.iTime[type].proc_time_30frames / 30,
			    ((jmgr_data.iTime[type]
			     .proc_time_30frames % 30) * 100) / 30,
			    jmgr_data.iTime[type].accumulated_proc_time
			     /jmgr_data.iTime[type].accumulated_frame_cnt,
			    ((jmgr_data.iTime[type].accumulated_proc_time
			     % jmgr_data.iTime[type].accumulated_frame_cnt)
			     * 100)
			     /jmgr_data.iTime[type].accumulated_frame_cnt,
			    jmgr_data.iTime[type].proc_time[0],
			    jmgr_data.iTime[type].proc_time[1],
			    jmgr_data.iTime[type].proc_time[2],
			    jmgr_data.iTime[type].proc_time[3],
			    jmgr_data.iTime[type].proc_time[4],
			    jmgr_data.iTime[type].proc_time[5],
			    jmgr_data.iTime[type].proc_time[6],
			    jmgr_data.iTime[type].proc_time[7],
			    jmgr_data.iTime[type].proc_time[8],
			    jmgr_data.iTime[type].proc_time[9],
			    jmgr_data.iTime[type].proc_time[10],
			    jmgr_data.iTime[type].proc_time[11],
			    jmgr_data.iTime[type].proc_time[12],
			    jmgr_data.iTime[type].proc_time[13],
			    jmgr_data.iTime[type].proc_time[14],
			    jmgr_data.iTime[type].proc_time[15],
			    jmgr_data.iTime[type].proc_time[16],
			    jmgr_data.iTime[type].proc_time[17],
			    jmgr_data.iTime[type].proc_time[18],
			    jmgr_data.iTime[type].proc_time[19],
			    jmgr_data.iTime[type].proc_time[20],
			    jmgr_data.iTime[type].proc_time[21],
			    jmgr_data.iTime[type].proc_time[22],
			    jmgr_data.iTime[type].proc_time[23],
			    jmgr_data.iTime[type].proc_time[24],
			    jmgr_data.iTime[type].proc_time[25],
			    jmgr_data.iTime[type].proc_time[26],
			    jmgr_data.iTime[type].proc_time[27],
			    jmgr_data.iTime[type].proc_time[28],
			    jmgr_data.iTime[type].proc_time[29]);
			jmgr_data.iTime[type].proc_base_cnt = 0;
			jmgr_data.iTime[type].proc_time_30frames = 0;
			jmgr_data.iTime[type].print_out_index++;
		} else {
			jmgr_data.iTime[type].proc_base_cnt++;
		}
	}
#endif

	return ret;
}

static int _jmgr_proc_exit_by_external(struct VpuList *list, int *result,
				       unsigned int type)
{
	if (!jmgr_get_close(type) && jmgr_data.handle[type] != 0x00) {
		list->type = type;

		if (type >= VPU_ENC)
			list->cmd_type = VPU_ENC_CLOSE;
		else
			list->cmd_type = VPU_DEC_CLOSE;

		list->handle = jmgr_data.handle[type];
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		dprintk("%s for %d!!", __func__, type);
		jmgr_list_manager(list, LIST_ADD);

		return 1;
	}

	return 0;
}

#if 0 // Keep the code for future use
static void _jmgr_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	// In case of exceptional processing. ex). sdcard out!!
	while (jmgr_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			err("cmd_processing(cmd %d) didn't finish!!",
			    jmgr_data.current_cmd);
			break;
		}
	}
}
#endif

static int _jmgr_external_all_close(int wait_ms)
{
	int type;
	int max_count;
	int ret;

	for (type = 0; type < JPU_MAX; type++) {
		if (_jmgr_proc_exit_by_external(
		    &jmgr_data.vList[type], &ret, type)) {
			max_count = wait_ms / 10;

			while (!jmgr_get_close(type)) {
				max_count--;
				usleep_range(0, 1000);	//msleep(10);
			}
		}
	}

	return 0;
}

static int _jmgr_cmd_open(char *str)
{
	int ret = 0;

	dprintk("_jmgr_%s_open In!! %d'th\n", str,
		atomic_read(&jmgr_data.dev_opened));

	jmgr_enable_clock(0, 0);

	if (atomic_read(&jmgr_data.dev_opened) == 0) {
#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
#endif
#if DEFINED_CONFIG_VENC_CNT_12345678
		jmgr_data.only_decmode = 0;
#else
		jmgr_data.only_decmode = 1;
#endif
		jmgr_data.clk_limitation = 1;
		jmgr_data.cmd_processing = 0;

		jmgr_hw_reset();
		jmgr_enable_irq(jmgr_data.irq);
		vetc_reg_init(jmgr_data.base_addr);
		ret = vmem_init();
		if (ret < 0)
			err("failed to allocate memory for JPU!! %d\n", ret);

	}
	atomic_inc(&jmgr_data.dev_opened);

	dprintk("_jmgr_%s_open Out!! %d'th\n", str,
		atomic_read(&jmgr_data.dev_opened));

	return 0;
}

static int _jmgr_cmd_release(char *str)
{
	dprintk("_jmgr_%s_release In!! %d'th\n", str,
		atomic_read(&jmgr_data.dev_opened));

	if (atomic_read(&jmgr_data.dev_opened) > 0)
		atomic_dec(&jmgr_data.dev_opened);

	if (atomic_read(&jmgr_data.dev_opened) == 0) {
		int type = 0;
		int alive_cnt = 0;

// To close whole jpu instance when being killed process opened this.
#if 1
		if (!jmgr_data.bVpu_already_proc_force_closed) {
			jmgr_data.external_proc = 1;
			_jmgr_external_all_close(200);
			jmgr_data.external_proc = 0;
		}
		jmgr_data.bVpu_already_proc_force_closed = false;
#endif

		for (type = 0; type < JPU_MAX; type++) {
			if (jmgr_data.closed[type] == 0)
				alive_cnt++;
		}

		if (alive_cnt)
			dprintk("JPU might be cleared by force.");

		atomic_set(&jmgr_data.oper_intr, 0);
		jmgr_data.cmd_processing = 0;

		_jmgr_close_all(1);

		jmgr_disable_irq(jmgr_data.irq);
		jmgr_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();

		jmgr_hw_assert();
		udelay(1000);
	}

	jmgr_disable_clock(0, 0);

	jmgr_data.nOpened_Count++;

	dprintk(
	"_jmgr_%s_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d)",
	  str,
	  atomic_read(&jmgr_data.dev_opened),
	  jmgr_data.nOpened_Count,
	  jmgr_get_close(VPU_DEC),
	  jmgr_get_close(VPU_DEC_EXT),
	  jmgr_get_close(VPU_DEC_EXT2),
	  jmgr_get_close(VPU_DEC_EXT3),
	  jmgr_get_close(VPU_DEC_EXT4));

	return 0;
}

static long _jmgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;

	mutex_lock(&jmgr_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
		if (cmd == VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *)arg,
			    sizeof(info));
		} else {
			if (copy_from_user(&info, (CONTENTS_INFO *)arg,
			    sizeof(info)))
				ret = -EFAULT;
		}
		break;

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
		{
			unsigned int type;
			unsigned int freemem_sz;

			if (cmd == VPU_GET_FREEMEM_SIZE_KERNEL) {
				(void)memcpy(&type, (unsigned int *)arg,
				    sizeof(unsigned int));
			} else {
				if (copy_from_user(&type, (unsigned int *)arg,
				    sizeof(unsigned int)))
					ret = -EFAULT;
			}

			if (ret == 0) {
				if (type > VPU_MAX)
					type = VPU_DEC;
				freemem_sz = vmem_get_freemem_size(type);

				if (cmd == VPU_GET_FREEMEM_SIZE_KERNEL) {
					(void)memcpy((unsigned int *)arg,
					    &freemem_sz, sizeof(unsigned int));
				} else {
					if (copy_to_user((unsigned int *)arg,
					    &freemem_sz, sizeof(unsigned int)))
						ret = -EFAULT;
				}
			}
		}
		break;

	case VPU_HW_RESET:
		jmgr_hw_reset();
		break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
		if (cmd == VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info,
			    (OPENED_sINFO *)arg,
			    sizeof(OPENED_sINFO));
		} else {
			if (copy_from_user(&open_info,
			    (OPENED_sINFO *)arg,
			    sizeof(OPENED_sINFO)))
				ret = -EFAULT;
		}

		if (ret == 0) {
			if (open_info.opened_cnt != 0)
				vmem_set_only_decode_mode(
				    open_info.type);
			ret = 0;
		}
		break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
		if (cmd == VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy((int *)arg, jmgr_data.closed,
			    sizeof(jmgr_data.closed));
		} else {
			if (copy_to_user((int *)arg, jmgr_data.closed,
			    sizeof(jmgr_data.closed)))
				ret = -EFAULT;
		}
		break;

	case VPU_GET_INSTANCE_IDX:
	case VPU_GET_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;

			if (cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)arg,
				    sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(&iInst, (int *)arg,
				    sizeof(INSTANCE_INFO)))
					ret = -EFAULT;
			}

			if (ret == 0) {
				if (iInst.type == VPU_ENC)
					venc_get_instance(&iInst.nInstance);
				else
					vdec_get_instance(&iInst.nInstance);

				if (cmd == VPU_GET_INSTANCE_IDX_KERNEL) {

					(void)memcpy((int *)arg, &iInst,
					    sizeof(INSTANCE_INFO));
				} else {
					if (copy_to_user((int *)arg, &iInst,
					    sizeof(INSTANCE_INFO)))
						ret = -EFAULT;
				}
			}
		}
		break;

	case VPU_CLEAR_INSTANCE_IDX:
	case VPU_CLEAR_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;

			if (cmd == VPU_CLEAR_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)arg,
				     sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(
				    &iInst, (int *)arg, sizeof(INSTANCE_INFO)))
					ret = -EFAULT;
			}

			if (ret == 0) {
				if (iInst.type == VPU_ENC)
					venc_clear_instance(iInst.nInstance);
				else
					vdec_clear_instance(iInst.nInstance);
			}
		}
		break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
		if (!jmgr_data.bVpu_already_proc_force_closed) {
			jmgr_data.external_proc = 1;
			_jmgr_external_all_close(200);
			jmgr_data.external_proc = 0;
			jmgr_data.bVpu_already_proc_force_closed = true;
		}
		break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
		jmgr_restore_clock(0,
				   atomic_read(&jmgr_data.dev_opened));
		break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		_jmgr_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		_jmgr_cmd_release("cmd");
		break;
#endif

	default:
		err("Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&jmgr_data.comm_data.io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long _jmgr_compat_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	return _jmgr_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static irqreturn_t _jmgr_isr_handler(int irq, void *dev_id)
{

	detailk("%s\n", __func__);
	atomic_inc(&jmgr_data.oper_intr);
	wake_up_interruptible(&jmgr_data.oper_wq);

	return IRQ_HANDLED;
}

static int _jmgr_open(struct inode *inode, struct file *filp)
{
	if (!jmgr_data.irq_reged)
		err("not registered jpu-mgr-irq\n");

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk("%s In!! %d'th\n", __func__,
		atomic_read(&jmgr_data.dev_file_opened));
	atomic_inc(&jmgr_data.dev_file_opened);
	dprintk("%s Out!! %d'th\n", __func__,
		atomic_read(&jmgr_data.dev_file_opened));
#else
	mutex_lock(&jmgr_data.comm_data.file_mutex);
	_jmgr_cmd_open("file");
	mutex_unlock(&jmgr_data.comm_data.file_mutex);
#endif

	filp->private_data = &jmgr_data;

	return 0;
}

static int _jmgr_release(struct inode *inode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk("%s In!! %d'th\n", __func__,
		atomic_read(&jmgr_data.dev_file_opened));
	atomic_dec(&jmgr_data.dev_file_opened);
	jmgr_data.nOpened_Count++;

	pr_info(
	"_vmgr_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
	    atomic_read(&jmgr_data.dev_file_opened),
	    jmgr_data.nOpened_Count,
	    jmgr_get_close(VPU_DEC),
	    jmgr_get_close(VPU_DEC_EXT),
	    jmgr_get_close(VPU_DEC_EXT2),
	    jmgr_get_close(VPU_DEC_EXT3),
	    jmgr_get_close(VPU_DEC_EXT4));
#else
	mutex_lock(&jmgr_data.comm_data.file_mutex);
	_jmgr_cmd_release("file");
	mutex_unlock(&jmgr_data.comm_data.file_mutex);
#endif

	return 0;
}

struct VpuList *jmgr_list_manager(struct VpuList *args, unsigned int cmd)
{
	struct VpuList *ret = NULL;
	struct VpuList *oper_data = (struct VpuList *) args;

	if (!oper_data) {
		if (cmd == LIST_ADD || cmd == LIST_DEL) {
			V_DBG(VPU_DBG_ERROR, "Data is null, cmd=%d", cmd);
			return NULL;
		}
	}

	if (cmd == LIST_ADD)
		*oper_data->vpu_result = RET0;

	mutex_lock(&jmgr_data.comm_data.list_mutex);
	{
		switch (cmd) {
		case LIST_ADD:
			*oper_data->vpu_result |= RET1;
			list_add_tail(&oper_data->list,
			    &jmgr_data.comm_data.main_list);
			jmgr_data.cmd_queued++;
			jmgr_data.comm_data.thread_intr++;
			break;

		case LIST_DEL:
			list_del(&oper_data->list);
			jmgr_data.cmd_queued--;
			break;

		case LIST_IS_EMPTY:
			if (list_empty(&jmgr_data.comm_data.main_list))
				ret = (struct VpuList *) 0x1234;
			break;

		case LIST_GET_ENTRY:
			ret =
			    list_first_entry(&jmgr_data.comm_data.main_list,
					     struct VpuList, list);
			break;
		}
	}
	mutex_unlock(&jmgr_data.comm_data.list_mutex);

	if (cmd == LIST_ADD)
		wake_up_interruptible(&jmgr_data.comm_data.thread_wq);

	return ret;
}
static int _jmgr_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;

	while (!jmgr_list_manager(NULL, LIST_IS_EMPTY)) {
		jmgr_data.cmd_processing = 1;
		oper_finished = 1;
		dprintk("%s :: not empty jmgr_data.cmd_queued(%d)",
		    __func__, jmgr_data.cmd_queued);

		oper_data = jmgr_list_manager(NULL, LIST_GET_ENTRY);
		if (!oper_data) {
			err("data is null\n");
			jmgr_data.cmd_processing = 0;
			return 0;
		}
		*oper_data->vpu_result |= RET2;

		dprintk("%s [%d] :: cmd = 0x%x, cmd_queued(%d)",
		    __func__,  oper_data->type,
		    oper_data->cmd_type, jmgr_data.cmd_queued);

		if (oper_data->type < JPU_MAX) {
			*oper_data->vpu_result |= RET3;

			*oper_data->vpu_result
			 = _jmgr_process(oper_data->type,
				    oper_data->cmd_type,
				    oper_data->handle,
				    oper_data->args);
			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result !=
				      RETCODE_INSUFFICIENT_BITSTREAM) &&
				    (*oper_data->vpu_result !=
				      RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {
					err(
					"jmgr_out[0x%x] :: type = %d, handle = 0x%x, cmd = 0x%x, frame_len %d\n",
					  *oper_data->vpu_result,
					  oper_data->type,
					  oper_data->handle,
					  oper_data->cmd_type,
					  jmgr_data.szFrame_Len);
				}

				if (*oper_data->vpu_result
				     == RETCODE_CODEC_EXIT) {
					jmgr_restore_clock(0,
					    atomic_read(&jmgr_data.dev_opened));
					_jmgr_close_all(1);
				}
			}
		} else {
			dprintk(
			"%s :: missed info or unknown command => type = 0x%x, cmd = 0x%x,",
			  __func__, oper_data->type, oper_data->cmd_type);
			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished) {
			if (oper_data->comm_data != NULL
			    && atomic_read(&jmgr_data.dev_opened) != 0) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1) {
					dprintk(
					  "poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
					  oper_data->comm_data->count,
					  oper_data->type, oper_data->cmd_type);
				}

				wake_up_interruptible(
				    &oper_data->comm_data->wq);
			} else {
				err(
				"Error: abnormal exception or external command was processed!! 0x%p - %d\n",
				    oper_data->comm_data,
				    atomic_read(&jmgr_data.dev_opened));
			}
		} else {
			err(
			"Error: abnormal exception 2!! 0x%p - %d\n",
			    oper_data->comm_data,
			    atomic_read(&jmgr_data.dev_opened));
		}

		jmgr_list_manager((void *)oper_data, LIST_DEL);

		jmgr_data.cmd_processing = 0;
	}

	return 0;
}

static int _jmgr_thread(void *kthread)
{
	dprintk("enter %s\n", __func__);

	do {
		if (jmgr_list_manager(NULL, LIST_IS_EMPTY)) {
			jmgr_data.cmd_processing = 0;

			(void)wait_event_interruptible_timeout(
				jmgr_data.comm_data.thread_wq,
				jmgr_data.comm_data.thread_intr > 0,
				msecs_to_jiffies(50));

			jmgr_data.comm_data.thread_intr = 0;
		} else {
			if (atomic_read(&jmgr_data.dev_opened)
			    || jmgr_data.external_proc) {
				_jmgr_operation();
			} else {
				struct VpuList *oper_data = NULL;

				err("DEL for empty\n");

				oper_data =
				    jmgr_list_manager(NULL, LIST_GET_ENTRY);
				if (oper_data)
					jmgr_list_manager(oper_data, LIST_DEL);
			}
		}
	} while (!kthread_should_stop());

	dprintk("finish %s\n", __func__);

	return 0;
}

static int _jmgr_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		err(KERN_ERR "%s: this address is not allowed\n", __func__);
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
	    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		err("%s :: remap_pfn_range failed\n", __func__);
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static const struct file_operations _jmgr_fops = {
	.open = _jmgr_open,
	.release = _jmgr_release,
	.mmap = _jmgr_mmap,
	.unlocked_ioctl = _jmgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = _jmgr_compat_ioctl,
#endif
};

static struct miscdevice _jmgr_misc_device = {
	MISC_DYNAMIC_MINOR,
	JMGR_NAME,
	&_jmgr_fops,
};

int jmgr_probe(struct platform_device *pdev)
{
	int ret;
	int type;
	unsigned long int_flags;
	struct resource *resource = NULL;

	if (pdev->dev.of_node == NULL)
		return -ENODEV;

	dprintk("jmgr initializing!!");
	memset(&jmgr_data, 0, sizeof(struct _mgr_data_t));
	for (type = 0; type < JPU_MAX; type++)
		jmgr_data.closed[type] = 1;

	jmgr_init_variable();
	atomic_set(&jmgr_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&jmgr_data.dev_file_opened, 0);
#endif

	jmgr_data.irq = platform_get_irq(pdev, 0);
	jmgr_data.nOpened_Count = 0;
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(&pdev->dev, "missing phy memory resource\n");
		return -1;
	}
	resource->end += 1;

	jmgr_data.base_addr =
	    devm_ioremap(&pdev->dev, resource->start,
	      (resource->end - resource->start));
	dprintk(
	    "============> JPU base address [0x%x -> 0x%p], irq num [%d]",
	    resource->start, jmgr_data.base_addr, (jmgr_data.irq - 32));

	jmgr_get_clock(pdev->dev.of_node);
	jmgr_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&jmgr_data.comm_data.thread_wq);
	init_waitqueue_head(&jmgr_data.oper_wq);

	mutex_init(&jmgr_data.comm_data.list_mutex);
	mutex_init(&(jmgr_data.comm_data.io_mutex));
	mutex_init(&(jmgr_data.comm_data.file_mutex));

	INIT_LIST_HEAD(&jmgr_data.comm_data.main_list);
	INIT_LIST_HEAD(&jmgr_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		err("unable to configure memory for VPU!! %d\n", ret);
		return -ENOMEM;
	}

	jmgr_init_interrupt();
	int_flags = jmgr_get_int_flags();
	ret =
	    jmgr_request_irq(jmgr_data.irq, _jmgr_isr_handler, int_flags,
			     JMGR_NAME, &jmgr_data);
	if (ret)
		err("to aquire jpu-dec-irq\n");

	jmgr_data.irq_reged = 1;
	jmgr_disable_irq(jmgr_data.irq);

	kidle_task = kthread_run(_jmgr_thread, NULL, "vJPU_th");
	if (IS_ERR(kidle_task)) {
		err("unable to create thread!!");
		kidle_task = NULL;
		return -1;
	}
	dprintk("success :: thread created!!");

	_jmgr_close_all(1);

	if (misc_register(&_jmgr_misc_device)) {
		err("JPU Manager: Couldn't register device.");
		return -EBUSY;
	}

	jmgr_enable_clock(0, 1);
	jmgr_disable_clock(0, 1);

	return 0;
}
EXPORT_SYMBOL(jmgr_probe);

int jmgr_remove(struct platform_device *pdev)
{
	misc_deregister(&_jmgr_misc_device);

	if (kidle_task) {
		kthread_stop(kidle_task);
		kidle_task = NULL;
	}

	devm_iounmap(&pdev->dev, jmgr_data.base_addr);
	if (jmgr_data.irq_reged) {
		jmgr_free_irq(jmgr_data.irq, &jmgr_data);
		jmgr_data.irq_reged = 0;
	}

	jmgr_put_clock();
	jmgr_put_reset();
	vmem_deinit();

	pr_info("success :: jmgr thread stopped!!\n");

	return 0;
}
EXPORT_SYMBOL(jmgr_remove);

#if defined(CONFIG_PM)
int jmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	if (atomic_read(&jmgr_data.dev_opened) != 0) {
		pr_info(
		    "\n jpu: suspend In DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d/%d/%d/%d/%d)\n",
		     jmgr_get_close(VPU_DEC), jmgr_get_close(VPU_DEC_EXT),
		     jmgr_get_close(VPU_DEC_EXT2), jmgr_get_close(VPU_DEC_EXT3),
		     jmgr_get_close(VPU_DEC_EXT4),
		     jmgr_get_close(VPU_ENC), jmgr_get_close(VPU_ENC_EXT),
		     jmgr_get_close(VPU_ENC_EXT2), jmgr_get_close(VPU_ENC_EXT3),
		     jmgr_get_close(VPU_ENC_EXT4), jmgr_get_close(VPU_ENC_EXT5),
		     jmgr_get_close(VPU_ENC_EXT6), jmgr_get_close(VPU_ENC_EXT7));

		_jmgr_external_all_close(200);

		open_count = atomic_read(&jmgr_data.dev_opened);

		for (i = 0; i < open_count; i++)
			jmgr_disable_clock(0, 0);
		pr_info(
		    "jpu: suspend Out DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d/%d/%d/%d/%d)\n\n",
		     jmgr_get_close(VPU_DEC), jmgr_get_close(VPU_DEC_EXT),
		     jmgr_get_close(VPU_DEC_EXT2), jmgr_get_close(VPU_DEC_EXT3),
		     jmgr_get_close(VPU_DEC_EXT4),
		     jmgr_get_close(VPU_ENC), jmgr_get_close(VPU_ENC_EXT),
		     jmgr_get_close(VPU_ENC_EXT2), jmgr_get_close(VPU_ENC_EXT3),
		     jmgr_get_close(VPU_ENC_EXT4), jmgr_get_close(VPU_ENC_EXT5),
		     jmgr_get_close(VPU_ENC_EXT6), jmgr_get_close(VPU_ENC_EXT7));
	}

	return 0;
}
EXPORT_SYMBOL(jmgr_suspend);

int jmgr_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	if (atomic_read(&jmgr_data.dev_opened) != 0) {

		open_count = atomic_read(&jmgr_data.dev_opened);

		for (i = 0; i < open_count; i++)
			jmgr_enable_clock(0, 0);

		pr_info("\n jpu: resume\n\n");
	}

	return 0;
}
EXPORT_SYMBOL(jmgr_resume);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu manager");
MODULE_LICENSE("GPL");

#endif
