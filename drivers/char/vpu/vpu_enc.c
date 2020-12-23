// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || \
	defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4) || \
	defined(CONFIG_VENC_CNT_5) || defined(CONFIG_VENC_CNT_6) || \
	defined(CONFIG_VENC_CNT_7) || defined(CONFIG_VENC_CNT_8)

#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <linux/io.h>
//#include <asm/io.h>
#include <asm/div64.h>

#include "vpu_buffer.h"
#include "vpu_mgr.h"

#ifdef CONFIG_SUPPORT_TCC_JPU
#include "jpu_mgr.h"
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
#include "vpu_hevc_enc_mgr.h"
#endif

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_ENC: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_ENC: " msg)
#define err(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_VPU_ENC[Err]: "msg)

static void _venc_inter_add_list(struct _vpu_encoder_data *vdata, int cmd,
				void *args)
{
	struct VpuList *oper_data = &vdata->venc_list[vdata->list_idx];

	oper_data->type = vdata->gsEncType;
	oper_data->cmd_type = cmd;

	switch (vdata->gsCodecType) {
	//////////////////////////////
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		oper_data->handle = vdata->gsJpuEncInit_Info.gsJpuEncHandle;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		oper_data->handle = vdata->gsVpuHevcEncInit_Info.handle;
		break;
#endif
	default:
		oper_data->handle     = vdata->gsVpuEncInit_Info.gsVpuEncHandle;
		break;
	}

	oper_data->args = args;
	oper_data->comm_data = &vdata->vComm_data;
	oper_data->vpu_result = &vdata->gsCommEncResult;
	*oper_data->vpu_result = 0;

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		jmgr_list_manager(oper_data, LIST_ADD);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		vmgr_hevc_enc_list_manager(oper_data, LIST_ADD);
		break;
#endif
	default:
		vmgr_list_manager(oper_data, LIST_ADD);
		break;
	}

	vdata->list_idx = (vdata->list_idx + 1) % LIST_MAX;
}

static void _venc_init_list(struct _vpu_encoder_data *vdata)
{
	int i;

	for (i = 0; i < LIST_MAX; i++)
		vdata->venc_list[i].comm_data = NULL;
}

static int _venc_proc_init(struct _vpu_encoder_data *vdata, VENC_INIT_t *arg)
{
	void *pArgs;

	dprintk("%s (Codec:%d) :: %s!!",
		vdata->misc->name, vdata->gsCodecType, __func__);

	_venc_init_list(vdata);

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		if (copy_from_user(&vdata->gsJpuEncInit_Info,
				arg, sizeof(JENC_INIT_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsJpuEncInit_Info;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		if (copy_from_user(&vdata->gsVpuHevcEncInit_Info,
				arg, sizeof(VENC_HEVC_INIT_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuHevcEncInit_Info;
		break;
#endif
	default:
		if (copy_from_user(&vdata->gsVpuEncInit_Info,
				arg, sizeof(VENC_INIT_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuEncInit_Info;
		break;
	}

	_venc_inter_add_list(vdata, VPU_ENC_INIT, (void *)pArgs);

	return 0;
}

static int _venc_proc_exit(struct _vpu_encoder_data *vdata, void *arg)
{
	void *pArgs;

	dprintk("%s (Codec-%d) :: %s!!",
			vdata->misc->name, vdata->gsCodecType, __func__);

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		if (copy_from_user(&vdata->gsJpuEncInOut_Info,
				arg, sizeof(JPU_ENCODE_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsJpuEncInOut_Info;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		if (copy_from_user(&vdata->gsVpuHevcEncInOut_Info,
				arg, sizeof(VENC_HEVC_ENCODE_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuHevcEncInOut_Info;
		break;
#endif
	default:
		if (copy_from_user(&vdata->gsVpuEncInOut_Info,
				arg, sizeof(VENC_ENCODE_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuEncInOut_Info;
		break;
	}

	_venc_inter_add_list(vdata, VPU_ENC_CLOSE, (void *)pArgs);

	return 0;
}

static int _venc_proc_put_header(struct _vpu_encoder_data *vdata,
					   VENC_PUT_HEADER_t *arg)
{
	void *pArgs;

	dprintk("%s (Codec-%d) :: %s!!",
			vdata->misc->name, vdata->gsCodecType, __func__);

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		V_DBG(VPU_DBG_ERROR, "%s :: jpu not support this !!\n",
				vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		if (copy_from_user(&vdata->gsVpuHevcEncPutHeader_Info,
				arg, sizeof(VENC_HEVC_PUT_HEADER_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuHevcEncPutHeader_Info;
		break;
#endif
	default:
		if (copy_from_user(&vdata->gsVpuEncPutHeader_Info,
				arg, sizeof(VENC_PUT_HEADER_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuEncPutHeader_Info;
		break;
	}

	_venc_inter_add_list(vdata, VPU_ENC_PUT_HEADER, (void *)pArgs);

	return 0;
}

static int _venc_proc_reg_framebuffer(struct _vpu_encoder_data *vdata,
						VENC_SET_BUFFER_t *arg)
{
	void *pArgs;

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s :: jpu not support this!!", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		if (copy_from_user(&vdata->gsVpuHevcEncBuffer_Info,
				arg, sizeof(VENC_HEVC_SET_BUFFER_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuHevcEncBuffer_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
		"_vpu_hevc_enc_proc_reg_framebuffer addr :: phy = 0x%x, virt = 0x%x!!",
		vdata->gsVpuHevcEncBuffer_Info.encBuffer
		    .m_FrameBufferStartAddr[0],
		vdata->gsVpuHevcEncBuffer_Info.encBuffer
		    .m_FrameBufferStartAddr[1]);
		break;
#endif
	default:
		if (copy_from_user(&vdata->gsVpuEncBuffer_Info,
				arg, sizeof(VENC_SET_BUFFER_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuEncBuffer_Info;

		detailk("addr :: phy = 0x%x, virt = 0x%x!!",
		vdata->gsVpuEncBuffer_Info.gsVpuEncBuffer
		    .m_FrameBufferStartAddr[0],
		vdata->gsVpuEncBuffer_Info.gsVpuEncBuffer
		    .m_FrameBufferStartAddr[1]);
		break;
	}

	_venc_inter_add_list(vdata, VPU_ENC_REG_FRAME_BUFFER, (void *)pArgs);

	return 0;
}

static int _venc_proc_encode(struct _vpu_encoder_data *vdata,
			VENC_ENCODE_t *arg)
{
	void *pArgs;

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		if (copy_from_user(&vdata->gsJpuEncInOut_Info,
						arg, sizeof(JPU_ENCODE_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsJpuEncInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
		"_jpu_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
		vdata->gsJpuEncInit_Info.gsJpuEncHandle,
		vdata->gsJpuEncInOut_Info.gsJpuEncInput.m_iBitstreamBufferSize);
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		if (copy_from_user(&vdata->gsVpuHevcEncInOut_Info,
				arg, sizeof(VENC_HEVC_ENCODE_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuHevcEncInOut_Info;

		V_DBG(VPU_DBG_IO_FB_INFO,
		"_vpu_hevc_enc_proc_encode In !! handle = 0x%x, in_stream_size = 0x%x",
		vdata->gsVpuHevcEncInit_Info.handle,
		vdata->gsVpuHevcEncInOut_Info.encInput
		    .m_iBitstreamBufferSize);
		break;
#endif
	default:
		if (copy_from_user(&vdata->gsVpuEncInOut_Info,
				arg, sizeof(VENC_ENCODE_t)))
			return -EFAULT;
		pArgs = (void *)&vdata->gsVpuEncInOut_Info;

		detailk(
		"%s In !! handle = 0x%x, in_stream_size = 0x%x",
		__func__,
		vdata->gsVpuEncInit_Info.gsVpuEncHandle,
		vdata->gsVpuEncInOut_Info.gsVpuEncInput
		    .m_iBitstreamBufferSize);
		break;
	}

	_venc_inter_add_list(vdata, VPU_ENC_ENCODE, (void *)pArgs);

	return 0;
}

static int _venc_result_general(struct _vpu_encoder_data *vdata, int *arg)
{
	if (copy_to_user(arg, &vdata->gsCommEncResult, sizeof(int)))
		return -EFAULT;

	return 0;
}

static int _venc_result_init(struct _vpu_encoder_data *vdata, VENC_INIT_t *arg)
{
	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		vdata->gsJpuEncInit_Info.result = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsJpuEncInit_Info,
						sizeof(JENC_INIT_t)))
			return -EFAULT;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		vdata->gsVpuHevcEncInit_Info.result = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsVpuHevcEncInit_Info,
				sizeof(VENC_HEVC_INIT_t)))
			return -EFAULT;
		break;
#endif
	default:
		vdata->gsVpuEncInit_Info.result = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsVpuEncInit_Info,
							sizeof(VENC_INIT_t)))
			return -EFAULT;
		break;
	}

	return 0;
}

static int _venc_result_put_header(struct _vpu_encoder_data *vdata,
						VENC_PUT_HEADER_t *arg)
{
	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		err("%s ::jpu not support this !!\n", vdata->misc->name);
		return -0x999;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		vdata->gsVpuHevcEncPutHeader_Info.result
		  = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsVpuHevcEncPutHeader_Info,
				sizeof(VENC_HEVC_PUT_HEADER_t)))
			return -EFAULT;
		break;
#endif
	default:
		vdata->gsVpuEncPutHeader_Info.result = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsVpuEncPutHeader_Info,
				sizeof(VENC_PUT_HEADER_t)))
			return -EFAULT;
		break;
	}

	return 0;
}

static int _venc_proc_encode_result(struct _vpu_encoder_data *vdata,
						VENC_ENCODE_t *arg)
{
	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		detailk(
		"%s :: %s !! Encoded_size[%d/%d]",
		vdata->misc->name, __func__,
		vdata->gsJpuEncInOut_Info.gsJpuEncOutput.m_iBitstreamHeaderSize,
		vdata->gsJpuEncInOut_Info.gsJpuEncOutput.m_iBitstreamOutSize);

		vdata->gsJpuEncInOut_Info.result = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsJpuEncInOut_Info,
				sizeof(JPU_ENCODE_t)))
			return -EFAULT;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		detailk(
		"%s :: %s !! PicType[%d], Encoded_size[%d]",
		vdata->misc->name, __func__,
		vdata->gsVpuHevcEncInOut_Info.encOutput.m_iPicType,
		vdata->gsVpuHevcEncInOut_Info.encOutput.m_iBitstreamOutSize);

		vdata->gsVpuHevcEncInOut_Info.result = vdata->gsCommEncResult;
		if (copy_to_user(arg, &vdata->gsVpuHevcEncInOut_Info,
				sizeof(VENC_HEVC_ENCODE_t)))
			return -EFAULT;
		break;
#endif
	default:
		detailk(
		"%s :: %s !! PicType[%d], Encoded_size[%d]",
		vdata->misc->name, __func__,
		vdata->gsVpuEncInOut_Info.gsVpuEncOutput.m_iPicType,
		vdata->gsVpuEncInOut_Info.gsVpuEncOutput.m_iBitstreamOutSize);

		vdata->gsVpuEncInOut_Info.result = vdata->gsCommEncResult;
		if (copy_to_user
		    (arg, &vdata->gsVpuEncInOut_Info, sizeof(VENC_ENCODE_t)))
			return -EFAULT;
		break;
	}

	return 0;
}

static void _venc_force_close(struct _vpu_encoder_data *vdata)
{
	int ret = 0;
	struct VpuList *cmd_list = &vdata->venc_list[vdata->list_idx];

	vdata->list_idx = (vdata->list_idx + 1) % LIST_MAX;

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		if (!jmgr_get_close(vdata->gsEncType) && jmgr_get_alive()) {
			int max_count = 100;

			jmgr_process_ex(cmd_list, vdata->gsEncType,
							VPU_ENC_CLOSE, &ret);
			while (!jmgr_get_close(vdata->gsEncType)) {
				max_count--;
				msleep(20);

				if (max_count <= 0)
					break;
			}
		}
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		if (!vmgr_hevc_enc_get_close(vdata->gsEncType)
		    && vmgr_hevc_enc_get_alive()) {
			int max_count = 100;

			vmgr_hevc_enc_process_ex(cmd_list,
					vdata->gsEncType, VPU_ENC_CLOSE, &ret);
			while (!vmgr_hevc_enc_get_close(vdata->gsEncType)) {
				max_count--;
				msleep(20);

				if (max_count <= 0)
					break;
			}
		}
		break;
#endif
	default:
		if (!vmgr_get_close(vdata->gsEncType) && vmgr_get_alive()) {
			int max_count = 100;

			vmgr_process_ex(cmd_list,
					vdata->gsEncType, VPU_ENC_CLOSE, &ret);
			while (!vmgr_get_close(vdata->gsEncType)) {
				max_count--;
				msleep(20);

				if (max_count <= 0)
					break;
			}
		}
		break;
	}
}

static int _vdev_init(struct _vpu_encoder_data *vdata, void *arg)
{
	if (vdata->vComm_data.dev_opened > 1) {
#ifdef CONFIG_SUPPORT_TCC_JPU
		if (vdata->gsCodecType == STD_MJPG) {
			V_DBG(VPU_DBG_ERROR,
			"Jpu(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Enc(%d)",
			vdata->misc->name, jmgr_get_alive(),
			jmgr_get_close(vdata->gsEncType));
		} else
#endif
		{
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
			if (vdata->gsCodecType == STD_HEVC_ENC) {
				V_DBG(VPU_DBG_ERROR,
				"%s (Codec-%d) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Enc(%d)",
				vdata->misc->name, vdata->gsCodecType,
				vmgr_hevc_enc_get_alive(),
				vmgr_hevc_enc_get_close(vdata->gsEncType));
			} else
#endif
			{
				V_DBG(VPU_DBG_ERROR,
				"%s (Codec-%d) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Enc(%d)",
				vdata->misc->name, vdata->gsCodecType,
				vmgr_get_alive(),
				vmgr_get_close(vdata->gsEncType));
			}
		}

		_venc_force_close(vdata);

		switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
		case STD_MJPG:
			jmgr_set_close(vdata->gsEncType, 1, 1);
			break;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		case STD_HEVC_ENC:
			vmgr_hevc_enc_set_close(vdata->gsEncType, 1, 1);
			break;
#endif
		default:
			vmgr_set_close(vdata->gsEncType, 1, 1);
			break;
		}
		vdata->vComm_data.dev_opened--;
	}

	vdata->gsCodecType = -1;
	if (copy_from_user(&vdata->gsCodecType, arg, sizeof(int)))
		return -EFAULT;

	switch (vdata->gsCodecType) {
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		memset(&vdata->gsJpuEncInit_Info, 0x00, sizeof(JENC_INIT_t));
		memset(&vdata->gsJpuEncInOut_Info, 0x00, sizeof(JPU_ENCODE_t));
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		memset(&vdata->gsVpuHevcEncInit_Info, 0x00,
					sizeof(VENC_HEVC_INIT_t));
		memset(&vdata->gsVpuHevcEncPutHeader_Info, 0x00,
					sizeof(VENC_HEVC_PUT_HEADER_t));
		memset(&vdata->gsVpuHevcEncBuffer_Info, 0x00,
					sizeof(VENC_HEVC_SET_BUFFER_t));
		memset(&vdata->gsVpuHevcEncInOut_Info, 0x00,
					sizeof(VENC_HEVC_ENCODE_t));
		break;
#endif
	default:
		memset(&vdata->gsVpuEncInit_Info, 0x00, sizeof(VENC_INIT_t));
		memset(&vdata->gsVpuEncPutHeader_Info, 0x00,
					sizeof(VENC_PUT_HEADER_t));
		memset(&vdata->gsVpuEncBuffer_Info, 0x00,
					sizeof(VENC_SET_BUFFER_t));
		memset(&vdata->gsVpuEncInOut_Info, 0x00, sizeof(VENC_ENCODE_t));
		break;
	}

	return 0;
}

int venc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		err("%s :: mmap : this address is not allowed",
			vdata->misc->name);
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
						vma->vm_end - vma->vm_start,
						vma->vm_page_prot)) {
		err("%s :: mmap :: remap_pfn_range failed",
			vdata->misc->name);
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

unsigned int venc_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

	if (vdata == NULL)
		return POLLERR | POLLNVAL;

	if (vdata->vComm_data.count == 0)
		poll_wait(filp, &(vdata->vComm_data.wq), wait);

	if (vdata->vComm_data.count > 0) {
		vdata->vComm_data.count--;
		return POLLIN;
	}

	return 0;
}

static int _venc_cmd_open(struct _vpu_encoder_data *vdata, char *str)
{
	dprintk("%s :: _venc_%s_open(%d)!!\n", vdata->misc->name,
			str, vdata->vComm_data.dev_opened);

	if (vmem_get_free_memory(vdata->gsEncType) == 0) {
		err(
		"[VPU] %s : Couldn't open device because of no reserved memory.",
			vdata->misc->name);
		return -ENOMEM;
	}

	dprintk("======> %s :: _venc_%s_open(%d)!!", vdata->misc->name, str,
		vdata->vComm_data.dev_opened);

	if (vdata->vComm_data.dev_opened == 0)
		vdata->vComm_data.count = 0;
	vdata->vComm_data.dev_opened++;

	return 0;
}

static int _venc_cmd_release(struct _vpu_encoder_data *vdata, char *str)
{
	detailk("======> %s :: _venc_%s_release In(%d)!!", vdata->misc->name,
		str, vdata->vComm_data.dev_opened);

	if (vdata->vComm_data.dev_opened > 0)
		vdata->vComm_data.dev_opened--;

	if (vdata->vComm_data.dev_opened == 0) {
		venc_clear_instance(vdata->gsEncType - VPU_ENC);
		_venc_force_close(vdata);

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG)
		jmgr_set_close(vdata->gsEncType, 1, 1);
	else
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		if (vdata->gsCodecType == STD_HEVC_ENC)
			vmgr_hevc_enc_set_close(vdata->gsEncType, 1, 1);
		else
#endif
			vmgr_set_close(vdata->gsEncType, 1, 1);
	}

	V_DBG(VPU_DBG_CLOSE, "======> %s :: _venc_%s_release Out(%d)!!",
		vdata->misc->name, str, vdata->vComm_data.dev_opened);

	return 0;
}

long venc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

	if (vmgr_get_alive() == 0
#ifdef CONFIG_SUPPORT_TCC_JPU
	    && jmgr_get_alive() == 0
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		&& vmgr_hevc_enc_get_alive() == 0
#endif
	    ) {
#ifdef CONFIG_SUPPORT_TCC_JPU
		V_DBG(VPU_DBG_ERROR,
		"This command(0x%x) for %s can't process because jmgr is not alive(%d)!!!",
		cmd, vdata->misc->name, jmgr_get_alive());
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		V_DBG(VPU_DBG_ERROR,
		"This command(0x%x) for %s can't process because of vmgr_hevc_enc is not alive(%d)!!!",
		cmd, vdata->misc->name, vmgr_hevc_enc_get_alive());
#endif
		V_DBG(VPU_DBG_ERROR,
		"This command(0x%x) for %s can't process because of vmgr is not alive(%d)!!!",
		cmd, vdata->misc->name, vmgr_get_alive());

		return -EPERM;
	}

	switch (cmd) {
	case DEVICE_INITIALIZE:
		return _vdev_init(vdata, (void *)arg);

	case V_ENC_INIT:
		return _venc_proc_init(vdata, (VENC_INIT_t *) arg);

	case V_ENC_PUT_HEADER:
		return _venc_proc_put_header(vdata, (VENC_PUT_HEADER_t *) arg);

	case V_ENC_REG_FRAME_BUFFER:
		return _venc_proc_reg_framebuffer(vdata,
						  (VENC_SET_BUFFER_t *) arg);

	case V_ENC_ENCODE:
		return _venc_proc_encode(vdata, (VENC_ENCODE_t *) arg);

	case V_ENC_CLOSE:
		return _venc_proc_exit(vdata, (void *)arg);

	case V_ENC_ALLOC_MEMORY:
		{
			int ret;
			MEM_ALLOC_INFO_t alloc_info;

			if (copy_from_user
			    (&alloc_info, (MEM_ALLOC_INFO_t *) arg,
			     sizeof(MEM_ALLOC_INFO_t)))
				return -EFAULT;

			ret =
			    vmem_proc_alloc_memory(vdata->gsCodecType,
						   &alloc_info,
						   vdata->gsEncType);

			if (copy_to_user
			    ((MEM_ALLOC_INFO_t *) arg, &alloc_info,
			     sizeof(MEM_ALLOC_INFO_t)))
				return -EFAULT;

			return ret;
		}
		break;

	case V_ENC_FREE_MEMORY:
		{
			return vmem_proc_free_memory(vdata->gsEncType);
		}
		break;

	case VPU_GET_FREEMEM_SIZE:
		{
			int szFreeMem = 0;

			szFreeMem = vmem_get_free_memory(vdata->gsEncType);
			if (copy_to_user
			    ((unsigned int *)arg, &szFreeMem,
			     sizeof(szFreeMem)))
				return -EFAULT;
			return 0;
		}
		break;

	case V_ENC_GENERAL_RESULT:
		return _venc_result_general(vdata, (int *)arg);

	case V_ENC_INIT_RESULT:
		return _venc_result_init(vdata, (VENC_INIT_t *) arg);

	case V_ENC_PUT_HEADER_RESULT:
		return _venc_result_put_header(vdata,
					       (VENC_PUT_HEADER_t *) arg);

	case V_ENC_ENCODE_RESULT:
		return _venc_proc_encode_result(vdata, (VENC_ENCODE_t *) arg);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case V_ENC_TRY_OPEN_DEV:
		_venc_cmd_open(vdata, "cmd");
		break;

	case V_ENC_TRY_CLOSE_DEV:
		_venc_cmd_release(vdata, "cmd");
		break;
#endif
	default:
		err("[%s] Unsupported ioctl[%d]!!!\n", vdata->misc->name, cmd);
		break;
	}

	return 0;
}

int venc_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	vdata->vComm_data.dev_file_opened++;

	dprintk("%s :: open Out(%d)!!\n", vdata->misc->name,
		vdata->vComm_data.dev_file_opened);
#else
	_venc_cmd_open(vdata, "file");
#endif

	return 0;
}

int venc_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct _vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	vdata->vComm_data.dev_file_opened--;

	dprintk("%s :: release Out(%d)!!\n", vdata->misc->name,
		vdata->vComm_data.dev_file_opened);
#else
	_venc_cmd_release(vdata, "file");
#endif

	return 0;
}

static const struct file_operations vdev_enc_fops = {
	.owner = THIS_MODULE,
	.open = venc_open,
	.release = venc_release,
	.mmap = venc_mmap,
	.unlocked_ioctl = venc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = venc_ioctl,
#endif
	.poll = venc_poll,
};

int venc_probe(struct platform_device *pdev)
{
	struct _vpu_encoder_data *vdata;
	int ret = -ENODEV;

	vdata = kzalloc(sizeof(struct _vpu_encoder_data), GFP_KERNEL);
	if (!vdata)
		return -ENOMEM;

	vdata->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!vdata->misc) {
		ret = -ENOMEM;
		goto err_misc_alloc;
	}

	vdata->misc->minor = MISC_DYNAMIC_MINOR;
	vdata->misc->fops = &vdev_enc_fops;
	vdata->misc->name = pdev->name;
	vdata->misc->parent = &pdev->dev;

	vdata->gsEncType = pdev->id;
	memset(&vdata->vComm_data, 0, sizeof(struct _vpu_dec_data_t));
	spin_lock_init(&(vdata->vComm_data.lock));
	init_waitqueue_head(&(vdata->vComm_data.wq));

	if (misc_register(vdata->misc)) {
		pr_info("[VPU] %s: Couldn't register device.\n",
		       pdev->name);
		ret = -EBUSY;
		goto err_misc_register;
	}

	platform_set_drvdata(pdev, vdata);
	pr_info("[VPU] %s Driver(id:%d) Initialized.\n", pdev->name, pdev->id);

	return 0;

err_misc_register:
	kfree(vdata->misc);
err_misc_alloc:
	kfree(vdata);

	return ret;
}
EXPORT_SYMBOL(venc_probe);

int venc_remove(struct platform_device *pdev)
{
	struct _vpu_encoder_data *vdata =
	    (struct _vpu_encoder_data *) platform_get_drvdata(pdev);

	misc_deregister(vdata->misc);

	kfree(vdata->misc);
	kfree(vdata);

	return 0;
}
EXPORT_SYMBOL(venc_remove);
#endif /*CONFIG_VENC_CNT_X*/
