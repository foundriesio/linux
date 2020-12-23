// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC

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
#include <soc/tcc/pmap.h>

#include "vpu_buffer.h"
#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_hevc_enc_mgr_sys.h"
#include "vpu_hevc_enc_mgr.h"

static unsigned int cntInt_vpu_he;	// = 0;
static void __iomem *vidsys_conf_reg;

// Control only once!!
static struct _mgr_data_t vmgr_hevc_enc_data;
static struct task_struct *kidle_task;	// = NULL;

extern int tcc_vpu_hevc_enc(int Op, codec_handle_t *pHandle,
					void *pParam1, void *pParam2);
extern int tcc_vpu_hevc_enc_esc(int Op, codec_handle_t *pHandle,
					void *pParam1, void *pParam2);
extern int tcc_vpu_hevc_enc_ext(int Op, codec_handle_t *pHandle,
					void *pParam1, void *pParam2);

struct VpuList *vmgr_hevc_enc_list_manager(struct VpuList *args,
					unsigned int cmd)
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

	mutex_lock(&vmgr_hevc_enc_data.comm_data.list_mutex);
	{
		switch (cmd) {
		case LIST_ADD:
			*oper_data->vpu_result |= RET1;
			list_add_tail(&oper_data->list,
				&vmgr_hevc_enc_data.comm_data.main_list);
			vmgr_hevc_enc_data.cmd_queued++;
			vmgr_hevc_enc_data.comm_data.thread_intr++;
			break;
		case LIST_DEL:
			list_del(&oper_data->list);
			vmgr_hevc_enc_data.cmd_queued--;
			break;
		case LIST_IS_EMPTY:
			if (list_empty(&vmgr_hevc_enc_data.comm_data.main_list))
				ret = (struct VpuList *) 0x1234;
			break;
		case LIST_GET_ENTRY:
			ret = list_first_entry(
					&vmgr_hevc_enc_data.comm_data.main_list,
					struct VpuList, list);
			break;
		}
	}
	mutex_unlock(&vmgr_hevc_enc_data.comm_data.list_mutex);

	if (cmd == LIST_ADD)
		wake_up_interruptible(&vmgr_hevc_enc_data.comm_data.thread_wq);

	return ret;
}

static void _vmgr_hevc_enc_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	//wait!! in case exceptional processing. ex). sdcard out!!
	while (vmgr_hevc_enc_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			V_DBG(VPU_DBG_ERROR,
			"cmd_processing(cmd %d) didn't finish!!",
				vmgr_hevc_enc_data.current_cmd);
			break;
		}
	}
}

static irqreturn_t _vmgr_hevc_enc_isr_handler(int irq, void *dev_id)
{
	cntInt_vpu_he++;

	atomic_inc(&vmgr_hevc_enc_data.oper_intr);

	wake_up_interruptible(&vmgr_hevc_enc_data.oper_wq);

	return IRQ_HANDLED;
}

static int _vmgr_hevc_enc_cmd_open(char *str)
{
	int ret = 0;

	V_DBG(VPU_DBG_SEQUENCE, "======> _vmgr_hevc_enc_%s_open enter!! %d'th",
		str, atomic_read(&vmgr_hevc_enc_data.dev_opened));

	vmgr_hevc_enc_enable_clock(0);

	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) == 0) {
	#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
	#endif
		vmgr_hevc_enc_data.only_decmode = 0;
		vmgr_hevc_enc_data.clk_limitation = 1;
		vmgr_hevc_enc_data.cmd_processing = 0;

		vmgr_hevc_enc_hw_reset();
		vmgr_hevc_enc_enable_irq(vmgr_hevc_enc_data.irq);

		ret = vmem_init();
		if (ret < 0) {
			V_DBG(VPU_DBG_ERROR,
			"failed to allocate memory for VPU_HEVC_ENC(WAVE420L)!! %d",
				ret);
		}

		cntInt_vpu_he = 0;
	}

	atomic_inc(&vmgr_hevc_enc_data.dev_opened);

	V_DBG(VPU_DBG_SEQUENCE, "======> _vmgr_hevc_enc_%s_open out!! %d'th",
		str, atomic_read(&vmgr_hevc_enc_data.dev_opened));

	return 0;
}

int vmgr_hevc_enc_get_alive(void)
{
	return atomic_read(&vmgr_hevc_enc_data.dev_opened);
}

int vmgr_hevc_enc_get_close(vputype type)
{
	return vmgr_hevc_enc_data.closed[type];
}

int vmgr_hevc_enc_set_close(vputype type, int value, int bfreemem)
{
	if (vmgr_hevc_enc_get_close(type) == value) {
		V_DBG(VPU_DBG_CLOSE, " %d was already set to %d.", type, value);
		return -1;
	}

	vmgr_hevc_enc_data.closed[type] = value;
	if (value == 1) {
		vmgr_hevc_enc_data.handle[type] = 0x00;

		if (bfreemem)
			vmem_proc_free_memory(type);
	}

	return 0;
}

static void _vmgr_hevc_enc_close_all(int bfreemem)
{
#if DEFINED_CONFIG_VENC_CNT_12345678
	vmgr_hevc_enc_set_close(VPU_ENC, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_2345678
	vmgr_hevc_enc_set_close(VPU_ENC_EXT, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_345678
	vmgr_hevc_enc_set_close(VPU_ENC_EXT2, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_45678
	vmgr_hevc_enc_set_close(VPU_ENC_EXT3, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_5678
	vmgr_hevc_enc_set_close(VPU_ENC_EXT4, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_678
	vmgr_hevc_enc_set_close(VPU_ENC_EXT5, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_78
	vmgr_hevc_enc_set_close(VPU_ENC_EXT6, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_8
	vmgr_hevc_enc_set_close(VPU_ENC_EXT7, 1, bfreemem);
#endif
}

int vmgr_hevc_enc_process_ex(struct VpuList *cmd_list, vputype type,
					int Op, int *result)
{
	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) == 0)
		return 0;

	pr_info("\n process_ex %d - 0x%x\n\n", type, Op);

	if (!vmgr_hevc_enc_get_close(type)) {
		cmd_list->type = type;
		cmd_list->cmd_type = Op;
		cmd_list->handle = vmgr_hevc_enc_data.handle[type];
		cmd_list->args = NULL;
		cmd_list->comm_data = NULL;
		cmd_list->vpu_result = result;

		vmgr_hevc_enc_list_manager(cmd_list, LIST_ADD);
	}

	return 1;
}

static int _vmgr_hevc_enc_proc_exit_by_external(struct VpuList *list,
					int *result, unsigned int type)
{
	if (!vmgr_hevc_enc_get_close(type)
			&& vmgr_hevc_enc_data.handle[type] != 0x00) {
		list->type = type;
		list->cmd_type = VPU_ENC_CLOSE;
		list->handle = vmgr_hevc_enc_data.handle[type];
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		pr_info("%s for %d!!\n", __func__, type);
		vmgr_hevc_enc_list_manager(list, LIST_ADD);

		return 1;
	}

	return 0;
}

static int _vmgr_hevc_enc_external_all_close(int wait_ms)
{
	int type = 0;
	int max_count = 0;
	int ret;

	for (type = VPU_ENC; type < VPU_HEVC_ENC_MAX; type++) {
		if (_vmgr_hevc_enc_proc_exit_by_external(
				&vmgr_hevc_enc_data.vList[type],
				&ret, type)) {
			max_count = wait_ms / 10;

			while (!vmgr_hevc_enc_get_close(type)) {
				max_count--;
				usleep_range(0, 1000);	//msleep(10);
			}
		}
	}

	return 0;
}

static int _vmgr_hevc_enc_cmd_release(char *str)
{
	V_DBG(VPU_DBG_CLOSE, "======> _vmgr_hevc_enc_%s_release In!! %d'th",
		str, atomic_read(&vmgr_hevc_enc_data.dev_opened));

	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) > 0)
		atomic_dec(&vmgr_hevc_enc_data.dev_opened);

	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) == 0) {
		int type = 0;
		int alive_cnt = 0;

		//To close whole vpu-hevc-enc instance
		// when being killed process opened this.
		if (!vmgr_hevc_enc_data.bVpu_already_proc_force_closed) {
			vmgr_hevc_enc_data.external_proc = 1;
			_vmgr_hevc_enc_external_all_close(200);
			vmgr_hevc_enc_data.external_proc = 0;
		}
		vmgr_hevc_enc_data.bVpu_already_proc_force_closed = false;

		for (type = VPU_ENC; type < VPU_HEVC_ENC_MAX; type++) {
			if (vmgr_hevc_enc_data.closed[type] == 0)
				alive_cnt++;
		}

		if (alive_cnt)
			pr_info("VPU-HEVC-ENC might be cleared by force.\n");

		atomic_set(&vmgr_hevc_enc_data.oper_intr, 0);
		vmgr_hevc_enc_data.cmd_processing = 0;

		_vmgr_hevc_enc_close_all(1);

		vmgr_hevc_enc_disable_irq(vmgr_hevc_enc_data.irq);
		vmgr_hevc_enc_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();

		vmgr_hevc_enc_hw_assert();
		udelay(1000); //1ms
	}

	vmgr_hevc_enc_disable_clock(0);

	vmgr_hevc_enc_data.nOpened_Count++;

	V_DBG(VPU_DBG_ERROR,
	"======> _vmgr_hevc_enc_%s_release Out!! %d'th, total = %d  - ENC(%d/%d/%d/%d/%d/%d/%d/%d)",
		str,
		atomic_read(&vmgr_hevc_enc_data.dev_opened),
		vmgr_hevc_enc_data.nOpened_Count,
		vmgr_hevc_enc_get_close(VPU_ENC),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT2),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT3),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT4),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT5),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT6),
		vmgr_hevc_enc_get_close(VPU_ENC_EXT7));

	return 0;
}

static int _vmgr_hevc_enc_open(struct inode *inode, struct file *filp)
{
	if (!vmgr_hevc_enc_data.irq_reged)
		V_DBG(VPU_DBG_ERROR, "not registered vpu-hevc-enc-mgr-irq");

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	V_DBG(VPU_DBG_SEQUENCE, "%s In!! %d'th",
		__func__,
		atomic_read(&vmgr_hevc_enc_data.dev_file_opened));
	atomic_inc(&vmgr_hevc_enc_data.dev_file_opened);
	V_DBG(VPU_DBG_SEQUENCE, "%s Out!! %d'th",
		__func__,
		atomic_read(&vmgr_hevc_enc_data.dev_file_opened));
#else
	mutex_lock(&vmgr_hevc_enc_data.comm_data.file_mutex);
	_vmgr_hevc_enc_cmd_open("file");
	mutex_unlock(&vmgr_hevc_enc_data.comm_data.file_mutex);
#endif

	filp->private_data = &vmgr_hevc_enc_data;
	return 0;
}

static int _vmgr_hevc_enc_release(struct inode *inode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	V_DBG(VPU_DBG_CLOSE,
		"enter!! %d'th",
		atomic_read(&vmgr_hevc_enc_data.dev_file_opened));

	atomic_dec(&vmgr_hevc_enc_data.dev_file_opened);
	vmgr_hevc_enc_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE,
	"Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		atomic_read(&vmgr_hevc_enc_data.dev_file_opened),
		vmgr_hevc_enc_data.nOpened_Count,
		vmgr_hevc_enc_get_close(VPU_DEC),
		vmgr_hevc_enc_get_close(VPU_DEC_EXT),
		vmgr_hevc_enc_get_close(VPU_DEC_EXT2),
		vmgr_hevc_enc_get_close(VPU_DEC_EXT3),
		vmgr_hevc_enc_get_close(VPU_DEC_EXT4));
#else
	V_DBG(VPU_DBG_CLOSE, "enter");

	mutex_lock(&vmgr_hevc_enc_data.comm_data.file_mutex);
	_vmgr_hevc_enc_cmd_release("file");
	mutex_unlock(&vmgr_hevc_enc_data.comm_data.file_mutex);

	V_DBG(VPU_DBG_CLOSE, "out");
#endif

	return 0;
}

static int _vmgr_hevc_enc_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	int timeout = 200;

	if (vmgr_hevc_enc_data.check_interrupt_detection) {
		if (atomic_read(&vmgr_hevc_enc_data.oper_intr) > 0) {
			V_DBG(VPU_DBG_INTERRUPT,
			"Success-1: vpu hevc enc operation!! (isr cnt:%d)",
				cntInt_vpu_he);
			ret_code = RETCODE_SUCCESS;
		} else {
			ret = wait_event_interruptible_timeout(
				    vmgr_hevc_enc_data.oper_wq,
				    atomic_read(
				      &vmgr_hevc_enc_data.oper_intr) > 0,
				    msecs_to_jiffies(timeout));

			if (atomic_read(&vmgr_hevc_enc_data.oper_intr) > 0) {
				V_DBG(VPU_DBG_INTERRUPT,
				"Success-2: vpu hevc enc operation!! (isr cnt:%d)",
					cntInt_vpu_he);
#if defined(FORCED_ERROR)
				if (forced_error_count-- <= 0) {
					ret_code = RETCODE_CODEC_EXIT;
					forced_error_count = FORCED_ERR_CNT;
					vetc_dump_reg_all(
					  vmgr_hevc_enc_data.base_addr,
					  "_vmgr_internal_handler force-timed_out");
				} else {
					ret_code = RETCODE_SUCCESS;
				}
#else
				ret_code = RETCODE_SUCCESS;
#endif
			} else {
				V_DBG(VPU_DBG_ERROR,
				"[CMD 0x%x][ret:%d]: vpu timed_out(ref %d msec) => oper_intr[%d], isr cnt:%d!! [%d]th frame len %d",
				  vmgr_hevc_enc_data.current_cmd,
				  ret,
				  timeout,
				  atomic_read(&vmgr_hevc_enc_data.oper_intr),
				  cntInt_vpu_he,
				  vmgr_hevc_enc_data.nDecode_Cmd,
				  vmgr_hevc_enc_data.szFrame_Len
				);

				vetc_dump_reg_all(vmgr_hevc_enc_data.base_addr,
					"_vmgr_internal_handler timed_out");
				ret_code = RETCODE_CODEC_EXIT;
			}
		}
		atomic_set(&vmgr_hevc_enc_data.oper_intr, 0);
		vmgr_hevc_enc_status_clear(vmgr_hevc_enc_data.base_addr);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt option=%d, isr cnt=%d, ev=%d)",
		vmgr_hevc_enc_data.check_interrupt_detection,
		cntInt_vpu_he,
		ret_code
		);

	return ret_code;
}

static int _vmgr_hevc_enc_process(vputype type, int cmd, long pHandle,
					 void *args)
{
	int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	struct timeval t1, t2;
	int time_gap_ms = 0;
#endif

	vmgr_hevc_enc_data.check_interrupt_detection = 0;
	vmgr_hevc_enc_data.current_cmd = cmd;

#if DEFINED_CONFIG_VENC_CNT_12345678

	if (type <= VPU_HEVC_ENC_MAX) {
		if (cmd != VPU_ENC_INIT) {
			if (vmgr_hevc_enc_get_close(type)
				 || vmgr_hevc_enc_data.handle[type] == 0x00) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if (cmd != VPU_ENC_ENCODE)
			V_DBG(VPU_DBG_SEQUENCE, "Encoder(%d), command: 0x%x",
				type, cmd);

		switch (cmd) {
		case VPU_ENC_INIT:
		{
			VENC_HEVC_INIT_t *arg = (VENC_HEVC_INIT_t *) args;

			vmgr_hevc_enc_data.check_interrupt_detection = 1;

			vmgr_hevc_enc_data.handle[type] = 0x00;
			vmgr_hevc_enc_data.szFrame_Len
			= arg->encInit.m_iPicWidth
				* arg->encInit.m_iPicHeight * 3 / 2;

			arg->encInit.m_RegBaseAddr[VA]
			= (codec_addr_t) vmgr_hevc_enc_data.base_addr;
			arg->encInit.m_Memcpy = vetc_memcpy;
			arg->encInit.m_Memset
			= (void (*) (void *, int, unsigned int, unsigned int))
				vetc_memset;
			arg->encInit.m_Interrupt
			= (int (*) (void)) _vmgr_hevc_enc_internal_handler;
			arg->encInit.m_Ioremap
			= (void * (*) (phys_addr_t, unsigned int)) vetc_ioremap;
			arg->encInit.m_Iounmap
			= (void (*) (void *)) vetc_iounmap;
			arg->encInit.m_reg_read
			= (unsigned int (*)(void *, unsigned int))
				vetc_reg_read;
			arg->encInit.m_reg_write
			= (void (*)(void *, unsigned int, unsigned int))
				vetc_reg_write;
			arg->encInit.m_Usleep
			= (void (*)(unsigned int, unsigned int))vetc_usleep;

			V_DBG(VPU_DBG_SEQUENCE,
			"Enc :: Init In =>Memcpy(0x%px),Memset(0x%px),Interrupt(0x%px),remap(0x%px),unmap(0x%px),read(0x%px),write(0x%px),sleep(0x%px)||workbuff(0x%px/0x%px),Reg(0x%px/0x%px), format(%d),W:H(%d:%d),Fps(%d),Bps(%d),Keyi(%d),Stream(0x%px/0x%px, %d),Reg(0x%px):Sec. AXI (0x%x)",
			    arg->encInit.m_Memcpy,
			    arg->encInit.m_Memset,
			    arg->encInit.m_Interrupt,
			    arg->encInit.m_Ioremap,
			    arg->encInit.m_Iounmap,
			    arg->encInit.m_reg_read,
			    arg->encInit.m_reg_write,
			    arg->encInit.m_Usleep,
			    arg->encInit.m_BitWorkAddr[PA],
			    arg->encInit.m_BitWorkAddr[VA],
			    vmgr_hevc_enc_data.base_addr,
			    arg->encInit.m_RegBaseAddr[VA],
			    arg->encInit.m_iBitstreamFormat,
			    arg->encInit.m_iPicWidth,
			    arg->encInit.m_iPicHeight,
			    arg->encInit.m_iFrameRate,
			    arg->encInit.m_iTargetKbps,
			    arg->encInit.m_iKeyInterval,
			    arg->encInit.m_BitstreamBufferAddr[PA],
			    arg->encInit.m_BitstreamBufferAddr[VA],
			    arg->encInit.m_iBitstreamBufferSize,
			    vidsys_conf_reg,
			    vetc_reg_read((void *) vidsys_conf_reg, 0x84));

			ret = tcc_vpu_hevc_enc(cmd, (void *)(&arg->handle),
				(void *)(&arg->encInit),
				(void *)(&arg->encInitialInfo));
			if (ret != RETCODE_SUCCESS) {
				V_DBG(VPU_DBG_ERROR,
				" :: Init failed with ret(0x%x)", ret);
				if (ret != RETCODE_CODEC_EXIT)
					vetc_dump_reg_all(
					  vmgr_hevc_enc_data.base_addr,
					  "Init error");
			}

			if (ret != RETCODE_CODEC_EXIT && arg->handle != 0) {
				vmgr_hevc_enc_data.handle[type] = arg->handle;
				vmgr_hevc_enc_set_close(type, 0, 0);
				V_DBG(VPU_DBG_SEQUENCE,
				"vmgr_hevc_enc_data.handle = 0x%x",
					arg->handle);
			} else {
				//To free memory!!
				vmgr_hevc_enc_set_close(type, 0, 0);
				vmgr_hevc_enc_set_close(type, 1, 1);
			}
			V_DBG(VPU_DBG_SEQUENCE,
			" :: Init Done Handle(0x%x)", arg->handle);
			vmgr_hevc_enc_data.nDecode_Cmd = 0;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			vmgr_hevc_enc_data.iTime[type].print_out_index
			= vmgr_hevc_enc_data.iTime[type].proc_base_cnt = 0;
			vmgr_hevc_enc_data.iTime[type].accumulated_proc_time
			= vmgr_hevc_enc_data.iTime[type].accumulated_frame_cnt
			= 0;
			vmgr_hevc_enc_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_ENC_REG_FRAME_BUFFER:
		{
			VENC_HEVC_SET_BUFFER_t *arg
			= (VENC_HEVC_SET_BUFFER_t *) args;

			V_DBG(VPU_DBG_SEQUENCE,
			"HEnc-%d: Register a frame buffer w PA(0x%px)/VA(0x%px)",
				type,
				arg->encBuffer.m_FrameBufferStartAddr[0],
				arg->encBuffer.m_FrameBufferStartAddr[1]);

			ret = tcc_vpu_hevc_enc(cmd, (codec_handle_t *) &pHandle,
				(void *)(&arg->encBuffer), (void *)NULL);
		}
		break;

		case VPU_ENC_PUT_HEADER:
		{
			VENC_HEVC_PUT_HEADER_t *arg
			= (VENC_HEVC_PUT_HEADER_t *) args;

			vmgr_hevc_enc_data.check_interrupt_detection = 1;

			V_DBG(VPU_DBG_SEQUENCE,
			"HEnc-%d: put an Enc header w type(%d),size(%d),PA(0x%px)/VA(0x%px)",
				type,
				arg->encHeader.m_iHeaderType,
				arg->encHeader.m_iHeaderSize,
				arg->encHeader.m_HeaderAddr[0],
				arg->encHeader.m_HeaderAddr[1]);
			ret = tcc_vpu_hevc_enc(cmd, (codec_handle_t *)&pHandle,
				(void *)(&arg->encHeader), (void *)NULL);
		}
		break;

		case VPU_ENC_ENCODE:
		{
			VENC_HEVC_ENCODE_t *arg = (VENC_HEVC_ENCODE_t *)args;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
			do_gettimeofday(&t1);
#endif

			V_DBG(VPU_DBG_SEQUENCE,
			" enter w/ Handle(0x%x) :: 0x%x-0x%x-0x%x, %d-%d-%d, %d-%d-%d, %d, 0x%x-%d",
				pHandle,
				arg->encInput.m_PicYAddr,
				arg->encInput.m_PicCbAddr,
				arg->encInput.m_PicCrAddr,
				arg->encInput.m_iForceIPicture,
				arg->encInput.m_iSkipPicture,
				arg->encInput.m_iQuantParam,
				arg->encInput.m_iChangeRcParamFlag,
				arg->encInput.m_iChangeTargetKbps,
				arg->encInput.m_iChangeFrameRate,
				arg->encInput.m_iChangeKeyInterval,
				arg->encInput.m_BitstreamBufferAddr,
				arg->encInput.m_iBitstreamBufferSize);

			vmgr_hevc_enc_data.check_interrupt_detection = 1;
			ret = tcc_vpu_hevc_enc(cmd, (codec_handle_t *)&pHandle,
				(void *)(&arg->encInput),
				(void *)(&arg->encOutput));

			vmgr_hevc_enc_data.nDecode_Cmd++;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			do_gettimeofday(&t2);
#endif

			V_DBG(VPU_DBG_SEQUENCE,
			" out w/ [%d] !! PicType[%d], Encoded_size[%d]",
				ret,
				arg->encOutput.m_iPicType,
				arg->encOutput.m_iBitstreamOutSize);
		}
		break;

		case VPU_ENC_CLOSE:
			V_DBG(VPU_DBG_SEQUENCE, "VPU_ENC_CLOSE !!");
			vmgr_hevc_enc_data.check_interrupt_detection = 1;
			ret = tcc_vpu_hevc_enc(cmd, (codec_handle_t *)&pHandle,
				(void *)NULL, (void *)NULL);
			vmgr_hevc_enc_set_close(type, 1, 1);
			break;

		default:
			V_DBG(VPU_DBG_ERROR,
			":: not supported command(0x%x)", cmd);
			return 0x999;
		}
	}

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	if (cmd == VPU_ENC_ENCODE) {
		time_gap_ms = vetc_GetTimediff_ms(t1, t2);
		vmgr_hevc_enc_data.iTime[type].accumulated_frame_cnt++;
		vmgr_hevc_enc_data.iTime[type]
		.proc_time[vmgr_hevc_enc_data.iTime[type].proc_base_cnt]
		= time_gap_ms;
		vmgr_hevc_enc_data.iTime[type].proc_time_30frames
		+= time_gap_ms;
		vmgr_hevc_enc_data.iTime[type].accumulated_proc_time
		+= time_gap_ms;
		if (vmgr_hevc_enc_data.iTime[type].proc_base_cnt != 0
			&& vmgr_hevc_enc_data.iTime[type].proc_base_cnt
				% 29 == 0) {
			V_DBG(VPU_DBG_PERF,
			"VHEVC[%u] Henc[%4u] time %2u.%2u / %2u.%2u ms: %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u",
			  type,
			  vmgr_hevc_enc_data.iTime[type].print_out_index,
			  vmgr_hevc_enc_data.iTime[type].proc_time_30frames/30,
			  ((vmgr_hevc_enc_data.iTime[type].proc_time_30frames
			  % 30) * 100) / 30,
			  vmgr_hevc_enc_data.iTime[type].accumulated_proc_time
			  /vmgr_hevc_enc_data.iTime[type].accumulated_frame_cnt,
			  ((vmgr_hevc_enc_data.iTime[type].accumulated_proc_time
			  %vmgr_hevc_enc_data.iTime[type].accumulated_frame_cnt)
			  *100)
			  /vmgr_hevc_enc_data.iTime[type].accumulated_frame_cnt,
			  vmgr_hevc_enc_data.iTime[type].proc_time[0],
			  vmgr_hevc_enc_data.iTime[type].proc_time[1],
			  vmgr_hevc_enc_data.iTime[type].proc_time[2],
			  vmgr_hevc_enc_data.iTime[type].proc_time[3],
			  vmgr_hevc_enc_data.iTime[type].proc_time[4],
			  vmgr_hevc_enc_data.iTime[type].proc_time[5],
			  vmgr_hevc_enc_data.iTime[type].proc_time[6],
			  vmgr_hevc_enc_data.iTime[type].proc_time[7],
			  vmgr_hevc_enc_data.iTime[type].proc_time[8],
			  vmgr_hevc_enc_data.iTime[type].proc_time[9],
			  vmgr_hevc_enc_data.iTime[type].proc_time[10],
			  vmgr_hevc_enc_data.iTime[type].proc_time[11],
			  vmgr_hevc_enc_data.iTime[type].proc_time[12],
			  vmgr_hevc_enc_data.iTime[type].proc_time[13],
			  vmgr_hevc_enc_data.iTime[type].proc_time[14],
			  vmgr_hevc_enc_data.iTime[type].proc_time[15],
			  vmgr_hevc_enc_data.iTime[type].proc_time[16],
			  vmgr_hevc_enc_data.iTime[type].proc_time[17],
			  vmgr_hevc_enc_data.iTime[type].proc_time[18],
			  vmgr_hevc_enc_data.iTime[type].proc_time[19],
			  vmgr_hevc_enc_data.iTime[type].proc_time[20],
			  vmgr_hevc_enc_data.iTime[type].proc_time[21],
			  vmgr_hevc_enc_data.iTime[type].proc_time[22],
			  vmgr_hevc_enc_data.iTime[type].proc_time[23],
			  vmgr_hevc_enc_data.iTime[type].proc_time[24],
			  vmgr_hevc_enc_data.iTime[type].proc_time[25],
			  vmgr_hevc_enc_data.iTime[type].proc_time[26],
			  vmgr_hevc_enc_data.iTime[type].proc_time[27],
			  vmgr_hevc_enc_data.iTime[type].proc_time[28],
			  vmgr_hevc_enc_data.iTime[type].proc_time[29]);

			vmgr_hevc_enc_data.iTime[type].proc_base_cnt = 0;
			vmgr_hevc_enc_data.iTime[type].proc_time_30frames = 0;
			vmgr_hevc_enc_data.iTime[type].print_out_index++;
		} else {
			vmgr_hevc_enc_data.iTime[type].proc_base_cnt++;
		}
	}
#endif
#endif

	return ret;
}

static int _vmgr_hevc_enc_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;

	while (!vmgr_hevc_enc_list_manager(NULL, LIST_IS_EMPTY)) {
		V_DBG(VPU_DBG_THREAD, ":: not empty cmd_queued (%d)",
		 vmgr_hevc_enc_data.cmd_queued);

		vmgr_hevc_enc_data.cmd_processing = 1;
		oper_finished = 1;

		oper_data
		 = (struct VpuList *)vmgr_hevc_enc_list_manager(NULL,
						LIST_GET_ENTRY);
		if (!oper_data) {
			V_DBG(VPU_DBG_ERROR, "data is null");
			vmgr_hevc_enc_data.cmd_processing = 0;
			return 0;
		}

		*oper_data->vpu_result |= RET2;

		V_DBG(VPU_DBG_THREAD,
		"[%d] :: cmd = 0x%x, vmgr_hevc_enc_data.cmd_queued (%d)",
			oper_data->type,
			oper_data->cmd_type,
			vmgr_hevc_enc_data.cmd_queued);

		if (oper_data->type >= VPU_ENC
			&& oper_data->type < VPU_HEVC_ENC_MAX) {
			*oper_data->vpu_result |= RET3;
			*oper_data->vpu_result
			= _vmgr_hevc_enc_process(oper_data->type,
				oper_data->cmd_type,
				oper_data->handle,
				oper_data->args);
			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result
				    != RETCODE_INSUFFICIENT_BITSTREAM) &&
				    (*oper_data->vpu_result
				    != RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {
					V_DBG(VPU_DBG_ERROR,
					"- out[0x%px] :: type = %u, vmgr_hevc_enc_data.handle = 0x%lx, cmd = %d, frame_len %u",
						(void *)*oper_data->vpu_result,
						oper_data->type,
						oper_data->handle,
						oper_data->cmd_type,
						vmgr_hevc_enc_data.szFrame_Len);
				}

				if (*oper_data->vpu_result
					== RETCODE_CODEC_EXIT) {
					vmgr_hevc_enc_restore_clock(0,
					  atomic_read(
					    &vmgr_hevc_enc_data.dev_opened));
					_vmgr_hevc_enc_close_all(1);
				}
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
			" :: missed info or unknown command > type = 0x%x, cmd = 0x%x",
				oper_data->type,
				oper_data->cmd_type);

			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished) {
			if (oper_data->comm_data != NULL
				&& atomic_read(
					&vmgr_hevc_enc_data.dev_opened) != 0) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1) {
					V_DBG(VPU_DBG_THREAD,
					"polling wakeup count = %d :: type(0x%x) cmd(0x%x)",
					    oper_data->comm_data->count,
					    oper_data->type,
					    oper_data->cmd_type);
				}
				wake_up_interruptible(
					&oper_data->comm_data->wq);
			} else {
				V_DBG(VPU_DBG_ERROR,
				"Error: abnormal exception or external command was processed!! 0x%p - %d",
				    oper_data->comm_data,
				    atomic_read(
				      &vmgr_hevc_enc_data.dev_opened));
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
			"Error: abnormal exception 2!! 0x%p - %d",
			    oper_data->comm_data,
			    atomic_read(&vmgr_hevc_enc_data.dev_opened));
		}

		vmgr_hevc_enc_list_manager(oper_data, LIST_DEL);

		vmgr_hevc_enc_data.cmd_processing = 0;
	}

	return 0;
}

static int _vmgr_hevc_enc_thread(void *kthread)
{
	V_DBG(VPU_DBG_THREAD, "enter");

	do {
		if (vmgr_hevc_enc_list_manager(NULL, LIST_IS_EMPTY)) {
			vmgr_hevc_enc_data.cmd_processing = 0;

			(void)wait_event_interruptible_timeout(
				vmgr_hevc_enc_data.comm_data.thread_wq,
				vmgr_hevc_enc_data.comm_data.thread_intr > 0,
				msecs_to_jiffies(50));

			vmgr_hevc_enc_data.comm_data.thread_intr = 0;
		} else {
			if (atomic_read(&vmgr_hevc_enc_data.dev_opened)
				|| vmgr_hevc_enc_data.external_proc) {
				_vmgr_hevc_enc_operation();
			} else {
				struct VpuList *oper_data = NULL;

				V_DBG(VPU_DBG_THREAD, "DEL for empty");

				oper_data
				= vmgr_hevc_enc_list_manager(NULL,
					LIST_GET_ENTRY);
				if (oper_data)
					vmgr_hevc_enc_list_manager(
						oper_data, LIST_DEL);
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "finish");

	return 0;
}

static unsigned int hangup_rel_count;	// = 0;
static long _vmgr_hevc_enc_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;

	mutex_lock(&vmgr_hevc_enc_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
		if (cmd == VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *) arg,
				sizeof(info));

			if (info.type >= VPU_ENC && info.isSWCodec) {
				vmgr_hevc_enc_data.clk_limitation = 0;
				V_DBG(VPU_DBG_SEQUENCE,
				"The clock limitation for VPU HEVC ENC is released.");
			}
		} else {
			if (copy_from_user(&info, (CONTENTS_INFO *)arg,
				sizeof(info))) {
				ret = -EFAULT;
			} else {
				if (info.type >= VPU_ENC && info.isSWCodec) {
					vmgr_hevc_enc_data.clk_limitation = 0;
					V_DBG(VPU_DBG_SEQUENCE,
					"The clock limitation for VPU HEVC ENC is released.");
				}
			}
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
			if (type > VPU_HEVC_ENC_MAX)
				type = VPU_DEC;
			freemem_sz = vmem_get_freemem_size(type);
			(void)memcpy((unsigned int *)arg, &freemem_sz,
			 sizeof(unsigned int));
		} else {
			if (copy_from_user(&type, (unsigned int *)arg,
				 sizeof(unsigned int))) {
				ret = -EFAULT;
			} else {
				if (type > VPU_HEVC_ENC_MAX)
					type = VPU_DEC;
				freemem_sz = vmem_get_freemem_size(type);
				if (copy_to_user((unsigned int *)arg,
					&freemem_sz,
					sizeof(unsigned int))) {
					ret = -EFAULT;
				}
			}
		}
	}
	break;

	case VPU_HW_RESET:
		vmgr_hevc_enc_hw_reset();
		break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
		if (cmd == VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info, (OPENED_sINFO *)arg,
			sizeof(OPENED_sINFO));

			if (open_info.opened_cnt != 0)
				vmem_set_only_decode_mode(open_info.type);
			ret = 0;
		} else {
			if (copy_from_user(&open_info, (OPENED_sINFO *)arg,
				 sizeof(OPENED_sINFO))) {
				ret = -EFAULT;
			} else {
				if (open_info.opened_cnt != 0)
					vmem_set_only_decode_mode(
						open_info.type);
				ret = 0;
			}
		}
		break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
		if (cmd == VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy((int *)arg,
			vmgr_hevc_enc_data.closed,
			sizeof(vmgr_hevc_enc_data.closed));
		} else {
			if (copy_to_user((int *)arg,
				vmgr_hevc_enc_data.closed,
				sizeof(vmgr_hevc_enc_data.closed))) {
				ret = -EFAULT;
			} else {
				ret = 0;
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
			if (memcpy(&type, (int *)arg,
				sizeof(unsigned int)) == NULL) {
				ret = -EFAULT;
			} else {
				if (copy_from_user(&type, (int *)arg,
						sizeof(unsigned int))) {
					ret = -EFAULT;
				}
			}
		}

		if (ret == 0) {
			vdec_check_instance_available(&nAvailable_Instance);

			if (cmd == VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
				(void)memcpy((unsigned int *)arg,
					&nAvailable_Instance,
					sizeof(unsigned int));
			} else {
				if (copy_to_user((unsigned int *)arg,
					&nAvailable_Instance,
					sizeof(unsigned int))) {
					ret = -EFAULT;
				}
			}
		}
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
					sizeof(INSTANCE_INFO))) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				venc_get_instance(&iInst.nInstance);

				if (cmd == VPU_GET_INSTANCE_IDX_KERNEL) {
					(void)memcpy((int *)arg, &iInst,
						sizeof(INSTANCE_INFO));
				} else {
					if (copy_to_user((int *)arg, &iInst,
						sizeof(INSTANCE_INFO))) {
						ret = -EFAULT;
					}
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
				if (copy_from_user(&iInst, (int *)arg,
					sizeof(INSTANCE_INFO)))
					ret = -EFAULT;
			}
			if (ret == 0)
				venc_clear_instance(iInst.nInstance);
		}
		break;

	case VPU_SET_RENDERED_FRAMEBUFFER:
	case VPU_SET_RENDERED_FRAMEBUFFER_KERNEL:
		if (cmd == VPU_SET_RENDERED_FRAMEBUFFER_KERNEL) {
			(void)memcpy(&vmgr_hevc_enc_data.gsRender_fb_info,
				(void *)arg, sizeof(VDEC_RENDERED_BUFFER_t));
		} else {
			if (copy_from_user(&vmgr_hevc_enc_data.gsRender_fb_info,
				(void *)arg, sizeof(VDEC_RENDERED_BUFFER_t))) {
				ret = -EFAULT;
			} else {
				V_DBG(VPU_DBG_SEQUENCE,
				"set rendered buffer info: 0x%x ~ 0x%x ",
				  vmgr_hevc_enc_data.gsRender_fb_info
				  .start_addr_phy,
				  vmgr_hevc_enc_data.gsRender_fb_info.size);
			}
		}
		break;

	case VPU_GET_RENDERED_FRAMEBUFFER:
	case VPU_GET_RENDERED_FRAMEBUFFER_KERNEL:
		if (cmd == VPU_GET_RENDERED_FRAMEBUFFER_KERNEL) {
			(void)memcpy((void *)arg,
			&vmgr_hevc_enc_data.gsRender_fb_info,
			 sizeof(VDEC_RENDERED_BUFFER_t));
		} else {
			if (copy_to_user((void *)arg,
				&vmgr_hevc_enc_data.gsRender_fb_info,
				sizeof(VDEC_RENDERED_BUFFER_t))) {
				ret = -EFAULT;
			} else {
				V_DBG(VPU_DBG_SEQUENCE,
				"get rendered buffer info: 0x%x ~ 0x%x",
				  vmgr_hevc_enc_data
				    .gsRender_fb_info.start_addr_phy,
				  vmgr_hevc_enc_data.gsRender_fb_info.size);
			}
		}
	break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
		if (!vmgr_hevc_enc_data.bVpu_already_proc_force_closed) {
			vmgr_hevc_enc_data.external_proc = 1;
			_vmgr_hevc_enc_external_all_close(200);
			vmgr_hevc_enc_data.external_proc = 0;
			vmgr_hevc_enc_data
			.bVpu_already_proc_force_closed = true;
		}
		break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
		vmgr_hevc_enc_restore_clock(0,
			atomic_read(&vmgr_hevc_enc_data.dev_opened));
		break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		_vmgr_hevc_enc_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		_vmgr_hevc_enc_cmd_release("cmd");
		break;
#endif

	case VPU_TRY_HANGUP_RELEASE:
		hangup_rel_count++;
		V_DBG(VPU_DBG_CMD,
			" vpu ===> VPU_TRY_HANGUP_RELEASE %d'th",
			hangup_rel_count);
		break;

	default:
		V_DBG(VPU_DBG_ERROR, "Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&vmgr_hevc_enc_data.comm_data.io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long _vmgr_hevc_enc_compat_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	return _vmgr_hevc_enc_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static int _vmgr_hevc_enc_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		V_DBG(VPU_DBG_ERROR, "_vmgr_mmap: this address is not allowed");
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		 vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		V_DBG(VPU_DBG_ERROR, "_vmgr_mmap :: remap_pfn_range failed");
		return -EAGAIN;
	}

	vma->vm_ops	= NULL;
	vma->vm_flags	|= VM_IO;
	vma->vm_flags	|= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static const struct file_operations _vmgr_hevc_enc_fops = {
	.open			= _vmgr_hevc_enc_open,
	.release		= _vmgr_hevc_enc_release,
	.mmap			= _vmgr_hevc_enc_mmap,
	.unlocked_ioctl		= _vmgr_hevc_enc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= _vmgr_hevc_enc_compat_ioctl,
#endif
};

static struct miscdevice _vmgr_hevc_enc_misc_device = {
	MISC_DYNAMIC_MINOR,
	VPU_HEVC_ENC_MGR_NAME,
	&_vmgr_hevc_enc_fops,
};

int vmgr_hevc_enc_opened(void)
{
	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) == 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(vmgr_hevc_enc_opened);

int vmgr_hevc_enc_probe(struct platform_device *pdev)
{
	int ret;
	int type = 0;
	unsigned long int_flags;
	struct resource *resource = NULL;

	if (pdev->dev.of_node == NULL)
		return -ENODEV;

	V_DBG(VPU_DBG_PROBE, "enter");

	memset(&vmgr_hevc_enc_data, 0, sizeof(struct _mgr_data_t));
	for (type = VPU_ENC; type < VPU_HEVC_ENC_MAX; type++)
		vmgr_hevc_enc_data.closed[type] = 1;

	vmgr_hevc_enc_init_variable();

	atomic_set(&vmgr_hevc_enc_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&vmgr_hevc_enc_data.dev_file_opened, 0);
#endif

	//Fetching IRQ number for device
	{
		int irq = platform_get_irq(pdev, 0);

		if (irq < 0) {
			dev_err(&pdev->dev, "could not get IRQ");
			return irq;
		}

		vmgr_hevc_enc_data.irq = (unsigned int)irq;
	}

	vmgr_hevc_enc_data.nOpened_Count = 0;
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		V_DBG(VPU_DBG_ERROR, "failed to get phy memory resourcea");
		return -1;
	}
	resource->end += 1;

	vmgr_hevc_enc_data.base_addr = devm_ioremap(&pdev->dev,
					resource->start,
					(resource->end - resource->start));
	V_DBG(VPU_DBG_PROBE,
		"VPU-HEVC-ENC base address [0x%x -> 0x%px], resource size [%d], irq num [%d]",
		resource->start,
		vmgr_hevc_enc_data.base_addr,
		(resource->end - resource->start),
		(vmgr_hevc_enc_data.irq - 32));

	vidsys_conf_reg = (void __iomem *)
			of_iomap(pdev->dev.of_node, 1);
	if (vidsys_conf_reg == NULL) {
		V_DBG(VPU_DBG_ERROR, "vidsys_conf_reg: NULL");
	} else {
		vetc_reg_write((void *)vidsys_conf_reg, 0x84, 0x9c9a3000);
		V_DBG(VPU_DBG_PROBE,
			"Video sub-system cfg reg (0x%px) : Sec. AXI (0x%x)",
			vidsys_conf_reg,
			vetc_reg_read((void *) vidsys_conf_reg, 0x84));
	}

	vmgr_hevc_enc_get_clock(pdev->dev.of_node);
	vmgr_hevc_enc_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&vmgr_hevc_enc_data.comm_data.thread_wq);
	init_waitqueue_head(&vmgr_hevc_enc_data.oper_wq);

	mutex_init(&vmgr_hevc_enc_data.comm_data.list_mutex);
	mutex_init(&vmgr_hevc_enc_data.comm_data.io_mutex);
	mutex_init(&vmgr_hevc_enc_data.comm_data.file_mutex);

	INIT_LIST_HEAD(&vmgr_hevc_enc_data.comm_data.main_list);
	INIT_LIST_HEAD(&vmgr_hevc_enc_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		V_DBG(VPU_DBG_ERROR,
		"unable to configure memory for VPU HEVC ENC!! %d",
			ret);
		return -ENOMEM;
	}

	vmgr_hevc_enc_init_interrupt();
	int_flags = vmgr_hevc_enc_get_int_flags();
	ret = vmgr_hevc_enc_request_irq(vmgr_hevc_enc_data.irq,
				_vmgr_hevc_enc_isr_handler,
				int_flags,
				VPU_HEVC_ENC_MGR_NAME,
				&vmgr_hevc_enc_data);
	if (ret)
		V_DBG(VPU_DBG_ERROR, "to aquire vpu-hevc-enc-irq");

	vmgr_hevc_enc_data.irq_reged = 1;
	vmgr_hevc_enc_disable_irq(vmgr_hevc_enc_data.irq);

	kidle_task = kthread_run(_vmgr_hevc_enc_thread, NULL,
	 "vHEVC_ENC_th");
	if (IS_ERR(kidle_task)) {
		V_DBG(VPU_DBG_ERROR, "unable to create thread!!");
		kidle_task = NULL;
		return -1;
	}
	V_DBG(VPU_DBG_PROBE, "success: thread created!!");

	_vmgr_hevc_enc_close_all(1);

	if (misc_register(&_vmgr_hevc_enc_misc_device)) {
		V_DBG(VPU_DBG_ERROR,
			"VPU HEVC ENC Manager: Couldn't register device");
		return -EBUSY;
	}

	vmgr_hevc_enc_enable_clock(0);
	vmgr_hevc_enc_disable_clock(0);

	return 0;

}
EXPORT_SYMBOL(vmgr_hevc_enc_probe);

int vmgr_hevc_enc_remove(struct platform_device *pdev)
{
	V_DBG(VPU_DBG_CLOSE, "enter");

	misc_deregister(&_vmgr_hevc_enc_misc_device);

	if (kidle_task) {
		kthread_stop(kidle_task);
		kidle_task = NULL;
	}

	devm_iounmap(&pdev->dev,
		(void __iomem *)vmgr_hevc_enc_data.base_addr);
	if (vmgr_hevc_enc_data.irq_reged) {
		vmgr_hevc_enc_free_irq(vmgr_hevc_enc_data.irq,
			&vmgr_hevc_enc_data);
		vmgr_hevc_enc_data.irq_reged = 0;
	}

	vmgr_hevc_enc_put_clock();
	vmgr_hevc_enc_put_reset();
	vmem_deinit();

	V_DBG(VPU_DBG_CLOSE, "out :: thread stopped!!");

	return 0;
}
EXPORT_SYMBOL(vmgr_hevc_enc_remove);

#if defined(CONFIG_PM)
int vmgr_hevc_enc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) != 0) {
		pr_info("\n vpu hevc enc: suspend enter for ENC(%d/%d/%d/%d/%d)\n",
			vmgr_hevc_enc_get_close(VPU_ENC),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT2),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT3),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT4),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT5),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT6),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT7));

		_vmgr_hevc_enc_external_all_close(200);

		open_count = atomic_read(&vmgr_hevc_enc_data.dev_opened);

		vmgr_hevc_enc_hw_assert();

		for (i = 0; i < open_count; i++)
			vmgr_hevc_enc_disable_clock(0);

		pr_info("vpu hevc enc: suspend out for ENC(%d/%d/%d/%d/%d)\n",
			vmgr_hevc_enc_get_close(VPU_ENC),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT2),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT3),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT4),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT5),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT6),
			vmgr_hevc_enc_get_close(VPU_ENC_EXT7));
	}

	return 0;

}
EXPORT_SYMBOL(vmgr_hevc_enc_suspend);

int vmgr_hevc_enc_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	if (atomic_read(&vmgr_hevc_enc_data.dev_opened) != 0) {
		open_count = atomic_read(&vmgr_hevc_enc_data.dev_opened);

		for (i = 0; i < open_count; i++)
			vmgr_hevc_enc_enable_clock(0);

		vmgr_hevc_enc_hw_deassert();

		pr_info("\n vpu: resume\n\n");
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_hevc_enc_resume);
#endif

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu hevc enc manager");
MODULE_LICENSE("GPL");

#endif /*CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC*/
