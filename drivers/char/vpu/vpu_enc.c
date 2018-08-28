/*
 *   FileName : vpu_enc.c
 *   Description : TCC VPU h/w block
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including
 * not limited to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind,
 * including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent,
 * copyright or other third party intellectual property right.
 * No warranty is made, express or implied, regarding the informations accuracy,
 * completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability
 * arising from, out of or in connection with this source code or the use
 * in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 *
 */

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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include "vpu_buffer.h"
#include "vpu_mgr.h"

#ifdef CONFIG_SUPPORT_TCC_JPU
#include "jpu_mgr.h"
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)

#define dprintk(msg...) //printk( "TCC_VPU_ENC: " msg);
#define detailk(msg...) //printk( "TCC_VPU_ENC: " msg);
#define err(msg...) printk("TCC_VPU_ENC[Err]: "msg);

static void _venc_inter_add_list(vpu_encoder_data *vdata, int cmd, void* args)
{
    vdata->venc_list[vdata->list_idx].type          = vdata->gsEncType;
    vdata->venc_list[vdata->list_idx].cmd_type      = cmd;
#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
        vdata->venc_list[vdata->list_idx].handle    = vdata->gsJpuEncInit_Info.gsJpuEncHandle;
    else
#endif
        vdata->venc_list[vdata->list_idx].handle    = vdata->gsVpuEncInit_Info.gsVpuEncHandle;
    vdata->venc_list[vdata->list_idx].args          = args;
    vdata->venc_list[vdata->list_idx].comm_data     = &vdata->vComm_data;
    vdata->gsCommEncResult = RET0;
    vdata->venc_list[vdata->list_idx].vpu_result    = &vdata->gsCommEncResult;

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
        jmgr_list_manager(&vdata->venc_list[vdata->list_idx], LIST_ADD);
    else
#endif
        vmgr_list_manager(&vdata->venc_list[vdata->list_idx], LIST_ADD);

    vdata->list_idx = (vdata->list_idx+1)%LIST_MAX;
}

static void _venc_init_list(vpu_encoder_data *vdata)
{
    int i = 0;

    for(i=0; i<LIST_MAX; i++)
    {
        vdata->venc_list[i].comm_data = NULL;
    }
}

static int _venc_proc_init(vpu_encoder_data *vdata, VENC_INIT_t *arg)
{
    void *pArgs;
    dprintk("%s :: _venc_proc_init!! \n", vdata->misc->name);

    _venc_init_list(vdata);

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        if(copy_from_user(&vdata->gsJpuEncInit_Info, arg, sizeof(JENC_INIT_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsJpuEncInit_Info;
    }
    else
#endif
    {
        if(copy_from_user(&vdata->gsVpuEncInit_Info, arg, sizeof(VENC_INIT_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsVpuEncInit_Info;
    }

    _venc_inter_add_list(vdata, VPU_ENC_INIT, ( void *)pArgs);

    return 0;
}

static int _venc_proc_exit(vpu_encoder_data *vdata, void *arg)
{
    void *pArgs;
    dprintk("%s :: _venc_proc_exit!! \n", vdata->misc->name);

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        if(copy_from_user(&vdata->gsJpuEncInOut_Info, arg, sizeof(JPU_ENCODE_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsJpuEncInOut_Info;
    }
    else
#endif
    {
        if(copy_from_user(&vdata->gsVpuEncInOut_Info, arg, sizeof(VENC_ENCODE_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsVpuEncInOut_Info;
    }


    _venc_inter_add_list(vdata, VPU_ENC_CLOSE, ( void *)pArgs);

    return 0;
}

static int _venc_proc_put_header(vpu_encoder_data *vdata, VENC_PUT_HEADER_t *arg)
{
    void *pArgs;
    dprintk("%s :: _venc_proc_put_header!! \n", vdata->misc->name);

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        err("%s :: jpu not support this !! \n", vdata->misc->name);return -0x999;
    }
    else
#endif
    {
        if(copy_from_user(&vdata->gsVpuEncPutHeader_Info, arg, sizeof(VENC_PUT_HEADER_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsVpuEncPutHeader_Info;
    }

    _venc_inter_add_list(vdata, VPU_ENC_PUT_HEADER, ( void *)pArgs);

    return 0;
}

static int _venc_proc_reg_framebuffer(vpu_encoder_data *vdata, VENC_SET_BUFFER_t *arg)
{
    void *pArgs;

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        err("%s :: jpu not support this !! \n", vdata->misc->name);return -0x999;
    }
    else
#endif
    {
        if(copy_from_user(&vdata->gsVpuEncBuffer_Info, arg, sizeof(VENC_SET_BUFFER_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsVpuEncBuffer_Info;

        detailk("_venc_proc_reg_framebuffer addr :: phy = 0x%x, virt = 0x%x!! \n", vdata->gsVpuEncBuffer_Info.gsVpuEncBuffer.m_FrameBufferStartAddr[0],
                        vdata->gsVpuEncBuffer_Info.gsVpuEncBuffer.m_FrameBufferStartAddr[1]);
    }

    _venc_inter_add_list(vdata, VPU_ENC_REG_FRAME_BUFFER, ( void *)pArgs);

    return 0;
}

static int _venc_proc_encode(vpu_encoder_data *vdata, VENC_ENCODE_t *arg)
{
    void *pArgs;

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        if(copy_from_user(&vdata->gsJpuEncInOut_Info, arg, sizeof(JPU_ENCODE_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsJpuEncInOut_Info;

        detailk("_jpu_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x \n", vdata->gsJpuEncInit_Info.gsJpuEncHandle, vdata->gsJpuEncInOut_Info.gsJpuEncInput.m_iBitstreamBufferSize);
    }
    else
#endif
    {
        if(copy_from_user(&vdata->gsVpuEncInOut_Info, arg, sizeof(VENC_ENCODE_t)))
            return -EFAULT;
        pArgs = ( void *)&vdata->gsVpuEncInOut_Info;

        detailk("_venc_proc_encode In !! handle = 0x%x, in_stream_size = 0x%x \n", vdata->gsVpuEncInit_Info.gsVpuEncHandle, vdata->gsVpuEncInOut_Info.gsVpuEncInput.m_iBitstreamBufferSize);
    }

    _venc_inter_add_list(vdata, VPU_ENC_ENCODE, ( void *)pArgs);

    return 0;
}

static int _venc_result_general(vpu_encoder_data *vdata, int *arg)
{
    if (copy_to_user(arg, &vdata->gsCommEncResult, sizeof(int)))
        return -EFAULT;

    return 0;
}

static int _venc_result_init(vpu_encoder_data *vdata, VENC_INIT_t *arg)
{
#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        vdata->gsJpuEncInit_Info.result = vdata->gsCommEncResult;
        if (copy_to_user(arg, &vdata->gsJpuEncInit_Info, sizeof(JENC_INIT_t)))
            return -EFAULT;
    }
    else
#endif
    {
        vdata->gsVpuEncInit_Info.result = vdata->gsCommEncResult;
        if (copy_to_user(arg, &vdata->gsVpuEncInit_Info, sizeof(VENC_INIT_t)))
            return -EFAULT;
    }

    return 0;
}

static int _venc_result_put_header(vpu_encoder_data *vdata, VENC_PUT_HEADER_t *arg)
{
#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        err("%s ::jpu not support this !! \n", vdata->misc->name);return -0x999;
    }
    else
#endif
    {
        vdata->gsVpuEncPutHeader_Info.result = vdata->gsCommEncResult;
        if (copy_to_user(arg, &vdata->gsVpuEncPutHeader_Info, sizeof(VENC_PUT_HEADER_t)))
            return -EFAULT;
    }

    return 0;
}

static int _venc_proc_encode_result(vpu_encoder_data *vdata, VENC_ENCODE_t *arg)
{
#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        detailk("%s :: _venc_proc_encode_result !! Encoded_size[%d/%d] \n", vdata->misc->name, vdata->gsJpuEncInOut_Info.gsJpuEncOutput.m_iBitstreamHeaderSize, vdata->gsJpuEncInOut_Info.gsJpuEncOutput.m_iBitstreamOutSize);

        vdata->gsJpuEncInOut_Info.result = vdata->gsCommEncResult;
        if (copy_to_user(arg, &vdata->gsJpuEncInOut_Info, sizeof(JPU_ENCODE_t)))
            return -EFAULT;
    }
    else
#endif
    {
        detailk("%s :: _venc_proc_encode_result !! PicType[%d], Encoded_size[%d] \n", vdata->misc->name, vdata->gsVpuEncInOut_Info.gsVpuEncOutput.m_iPicType, vdata->gsVpuEncInOut_Info.gsVpuEncOutput.m_iBitstreamOutSize);

        vdata->gsVpuEncInOut_Info.result = vdata->gsCommEncResult;
        if (copy_to_user(arg, &vdata->gsVpuEncInOut_Info, sizeof(VENC_ENCODE_t)))
            return -EFAULT;
    }

    return 0;
}

static void _venc_force_close(vpu_encoder_data *vdata)
{
    int ret = 0;
    VpuList_t *cmd_list = &vdata->venc_list[vdata->list_idx];
    vdata->list_idx = (vdata->list_idx+1)%LIST_MAX;

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        if(!jmgr_get_close(vdata->gsEncType) && jmgr_get_alive()){
            int max_count = 100;

            jmgr_process_ex(cmd_list, vdata->gsEncType, VPU_ENC_CLOSE, &ret);
            while(!jmgr_get_close(vdata->gsEncType))
            {
                max_count--;
                msleep(20);

                if(max_count <= 0)
                    break;
            }
        }
    }
    else
#endif
    {
        if(!vmgr_get_close(vdata->gsEncType) && vmgr_get_alive()){
            int max_count = 100;

            vmgr_process_ex(cmd_list, vdata->gsEncType, VPU_ENC_CLOSE, &ret);
            while(!vmgr_get_close(vdata->gsEncType))
            {
                max_count--;
                msleep(20);

                if(max_count <= 0)
                    break;
            }
        }
    }
}

static int _vdev_init(vpu_encoder_data *vdata, void *arg)
{
    if(vdata->vComm_data.dev_opened > 1){

#ifdef CONFIG_SUPPORT_TCC_JPU
        if(vdata->gsCodecType == STD_MJPG)
        {
            err("Jpu(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Enc(%d) \n", vdata->misc->name, jmgr_get_alive(), jmgr_get_close(vdata->gsEncType));
        }
        else
#endif
        {
            err("Vpu(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Enc(%d) \n", vdata->misc->name, vmgr_get_alive(), vmgr_get_close(vdata->gsEncType));
        }

        _venc_force_close(vdata);

#ifdef CONFIG_SUPPORT_TCC_JPU
        if(vdata->gsCodecType == STD_MJPG)
            jmgr_set_close(vdata->gsEncType, 1, 1);
        else
#endif
            vmgr_set_close(vdata->gsEncType, 1, 1);

        vdata->vComm_data.dev_opened--;
    }

    vdata->gsCodecType = -1;
    if(copy_from_user(&vdata->gsCodecType, arg, sizeof(int)))
        return -EFAULT;

#ifdef CONFIG_SUPPORT_TCC_JPU
    if(vdata->gsCodecType == STD_MJPG)
    {
        memset(&vdata->gsJpuEncInit_Info, 0x00, sizeof(JENC_INIT_t));
        //memset(&vdata->gsJpuEncSeqHeader_Info, 0x00, sizeof(JPU_SEQ_HEADER_t));
        //memset(&vdata->gsJpuEncBuffer_Info, 0x00, sizeof(JPU_SET_BUFFER_t));
        memset(&vdata->gsJpuEncInOut_Info, 0x00, sizeof(JPU_ENCODE_t));
    }
    else
#endif
    {
        memset(&vdata->gsVpuEncInit_Info, 0x00, sizeof(VENC_INIT_t));
        memset(&vdata->gsVpuEncPutHeader_Info, 0x00, sizeof(VENC_PUT_HEADER_t));
        memset(&vdata->gsVpuEncBuffer_Info, 0x00, sizeof(VENC_SET_BUFFER_t));
        memset(&vdata->gsVpuEncInOut_Info, 0x00, sizeof(VENC_ENCODE_t));
    }

    return 0;
}

int venc_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

#if defined(CONFIG_TCC_MEM)
    if(range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0){
        printk(KERN_ERR  "%s :: mmap: this address is not allowed \n", vdata->misc->name);
        return -EAGAIN;
    }
#endif

    vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
    if(remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
    {
        printk("%s :: mmap :: remap_pfn_range failed\n", vdata->misc->name);
        return -EAGAIN;
    }

    vma->vm_ops     = NULL;
    vma->vm_flags   |= VM_IO;
    vma->vm_flags   |= VM_DONTEXPAND | VM_PFNMAP;

    return 0;
}


unsigned int venc_poll(struct file *filp, poll_table *wait)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

    if (vdata == NULL){
        return -EFAULT;
    }

    if (vdata->vComm_data.count > 0)
    {
        vdata->vComm_data.count--;
        return POLLIN;
    }

    poll_wait(filp, &(vdata->vComm_data.wq), wait);

    if (vdata->vComm_data.count > 0)
    {
        vdata->vComm_data.count--;
        return POLLIN;
    }

    return 0;
}

long venc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

    if(vmgr_get_alive() == 0
#ifdef CONFIG_SUPPORT_TCC_JPU
        && jmgr_get_alive() == 0
#endif
    )
    {
#if defined(CONFIG_SUPPORT_TCC_JPU)
        err("This command(0x%x) for %s can't process because Mgr is not alive(v/j = %d/%d) !!!\n", cmd, vdata->misc->name, vmgr_get_alive(), jmgr_get_alive());
#else
        err("This command(0x%x) for %s can't process because of Mgr is not alive(%d) !!!\n", cmd, vdata->misc->name, vmgr_get_alive());
#endif
        return -EPERM;;
    }

    switch(cmd)
    {
        case DEVICE_INITIALIZE:
            return _vdev_init(vdata, (void*)arg);

        case V_ENC_INIT:
            return _venc_proc_init(vdata, (VENC_INIT_t *)arg);

        case V_ENC_PUT_HEADER:
            return _venc_proc_put_header(vdata, (VENC_PUT_HEADER_t *)arg);

        case V_ENC_REG_FRAME_BUFFER:
            return _venc_proc_reg_framebuffer(vdata, (VENC_SET_BUFFER_t *)arg);

        case V_ENC_ENCODE:
            return _venc_proc_encode(vdata, (VENC_ENCODE_t *)arg);

        case V_ENC_CLOSE:
            return _venc_proc_exit(vdata, (void *)arg);

        case V_ENC_ALLOC_MEMORY:
        {
            int ret;
            MEM_ALLOC_INFO_t alloc_info;

            if(copy_from_user(&alloc_info, (MEM_ALLOC_INFO_t*)arg, sizeof(MEM_ALLOC_INFO_t)))
                return -EFAULT;

            ret = vmem_proc_alloc_memory(vdata->gsCodecType, &alloc_info, vdata->gsEncType);

            if (copy_to_user((MEM_ALLOC_INFO_t*)arg, &alloc_info, sizeof(MEM_ALLOC_INFO_t)))
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
            if (copy_to_user((unsigned int*)arg, &szFreeMem, sizeof(szFreeMem)))
                return -EFAULT;
            return 0;
        }
        break;

        case V_ENC_GENERAL_RESULT:
            return _venc_result_general(vdata, (int *)arg);

        case V_ENC_INIT_RESULT:
            return _venc_result_init(vdata, (VENC_INIT_t *)arg);

        case V_ENC_PUT_HEADER_RESULT:
            return _venc_result_put_header(vdata, (VENC_PUT_HEADER_t *)arg);

        case V_ENC_ENCODE_RESULT:
            return _venc_proc_encode_result(vdata, (VENC_ENCODE_t *)arg);

        default:
            err("[%s] Unsupported ioctl[%d]!!!\n", vdata->misc->name, cmd);
            break;
    }

    return 0;
}

int venc_open(struct inode *inode, struct file *filp)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

    dprintk("%s :: open(%d)!! \n", vdata->misc->name, vdata->vComm_data.dev_opened);

    if( vdata->vComm_data.dev_opened == 0 )
        vdata->vComm_data.count = 0;
    vdata->vComm_data.dev_opened++;

    return 0;
}

int venc_release(struct inode *inode, struct file *filp)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    vpu_encoder_data *vdata = dev_get_drvdata(misc->parent);

    detailk("%s :: release In(%d)!! \n", vdata->misc->name, vdata->vComm_data.dev_opened);


    if(vdata->vComm_data.dev_opened > 0)
        vdata->vComm_data.dev_opened--;

    if(vdata->vComm_data.dev_opened == 0)
    {
        venc_clear_instance(vdata->gsEncType-VPU_ENC);
        _venc_force_close(vdata);

#ifdef CONFIG_SUPPORT_TCC_JPU
        if(vdata->gsCodecType == STD_MJPG)
            jmgr_set_close(vdata->gsEncType, 1, 1);
        else
#endif
            vmgr_set_close(vdata->gsEncType, 1, 1);
    }

    dprintk("%s :: release Out(%d)!! \n", vdata->misc->name, vdata->vComm_data.dev_opened);

    return 0;
}

static struct file_operations vdev_enc_fops = {
    .owner              = THIS_MODULE,
    .open               = venc_open,
    .release            = venc_release,
    .mmap               = venc_mmap,
    .unlocked_ioctl     = venc_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl       = venc_ioctl,
#endif
    .poll               = venc_poll,
};

int venc_probe(struct platform_device *pdev)
{
    vpu_encoder_data *vdata;
    int ret = -ENODEV;

    if( vmem_get_free_memory(pdev->id) == 0 )
    {
        printk(KERN_WARNING "VPU %s: Couldn't register device because of no-reserved memory.\n", pdev->name);
        return -ENOMEM;
    }

    vdata = kzalloc(sizeof(vpu_encoder_data), GFP_KERNEL);
    if (!vdata)
        return -ENOMEM;

    vdata->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
    if (!vdata->misc){
        ret = -ENOMEM;
        goto err_misc_alloc;
    }

    vdata->misc->minor = MISC_DYNAMIC_MINOR;
    vdata->misc->fops = &vdev_enc_fops;
    vdata->misc->name = pdev->name;
    vdata->misc->parent = &pdev->dev;

    vdata->gsEncType = pdev->id;
    memset(&vdata->vComm_data, 0, sizeof(vpu_comm_data_t));
    spin_lock_init(&(vdata->vComm_data.lock));
    init_waitqueue_head(&(vdata->vComm_data.wq));

    if (misc_register(vdata->misc))
    {
        printk(KERN_WARNING "VPU %s: Couldn't register device.\n", pdev->name);
        ret = -EBUSY;
        goto err_misc_register;
    }

    platform_set_drvdata(pdev, vdata);
    pr_info("VPU %s Driver(id:%d) Initialized.\n", pdev->name, pdev->id);

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
    vpu_encoder_data *vdata = (vpu_encoder_data *)platform_get_drvdata(pdev);

    misc_deregister(vdata->misc);

    kfree(vdata->misc);
    kfree(vdata);

    return 0;
}
EXPORT_SYMBOL(venc_remove);
#endif

