// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/uaccess.h>

#include <linux/io.h>
//#if (KERNEL_VERSION(2, 6, 39) <= LINUX_VERSION_CODE)
#include <asm/pgtable.h>
//#endif

#include <soc/tcc/pmap.h>

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_buffer.h"
#include "vpu_mem.h"

#include <video/tcc/tcc_mem_ioctl.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_vsync_ioctl.h>

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/export.h>

struct tcc_dma_buf_priv {
    struct dma_buf *buf;    // dma-buf handle structure
    dma_addr_t phys;        // physical address
    size_t size;            // allocation size
    void *virt;             // virtual address
};

struct tcc_dma_buf_data {
    int dmabuf_fd;
};

#if defined(CONFIG_TCC_MEM)
extern int set_displaying_index(unsigned long arg);
extern int get_displaying_index(int nInst);
extern int set_buff_id(unsigned long arg);
extern int get_buff_id(int nInst);
#endif

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && \
	defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
extern void tcc_vsync_set_deinterlace_mode(int mode);
extern int tcc_video_last_frame(void *pVSyncDisp,
				struct stTcc_last_frame iLastFrame,
				struct tcc_lcdc_image_update *lastUpdated,
				unsigned int type);
#endif

#define vpu_dma_printk //printk
#define VPU_PRINT_LEVEL KERN_DEBUG

static int tcc_mem_create_dma_buf(stVpuPhysInfo *pmap_info);
static int tcc_mem_release_dma_buf(int fd);

long vmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch (cmd) {
#if defined(CONFIG_TCC_MEM)
	case TCC_VIDEO_SET_DISPLAYING_IDX:
		{
			ret = set_displaying_index(arg);
		}
		break;

	case TCC_VIDEO_GET_DISPLAYING_IDX:
		{
			int nInstIdx = 0, nId = 0;

			if (copy_from_user(&nInstIdx, (int *)arg,
								sizeof(int))) {
				ret = -EFAULT;
			} else {
				nId = get_displaying_index(nInstIdx);
				if (copy_to_user((int *)arg, &nId, sizeof(int)))
					ret = -EFAULT;
			}
		}
		break;

	case TCC_VIDEO_SET_DISPLAYED_BUFFER_ID:
		{
			ret = set_buff_id(arg);
		}
		break;

	case TCC_VIDEO_GET_DISPLAYED_BUFFER_ID:
		{
			int nInstIdx = 0, nId = 0;

			if (copy_from_user(&nInstIdx, (int *)arg,
								sizeof(int))) {
				ret = -EFAULT;
			} else {
				nId = get_buff_id(nInstIdx);
				if (copy_to_user((int *)arg, &nId, sizeof(int)))
					ret = -EFAULT;
			}
		}
		break;
#endif

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && \
	defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
	case TCC_LCDC_VIDEO_KEEP_LASTFRAME:
		{
			struct stTcc_last_frame lastframe_info;

			if (copy_from_user((void *)&lastframe_info,
				(const void *)arg,
				sizeof(struct stTcc_last_frame))) {
				ret = -EFAULT;
			} else {
				struct tcc_lcdc_image_update lastUpdated;

				// Only support VSYNC_MAIN
				ret = tcc_video_last_frame(NULL, lastframe_info,
						&lastUpdated, 0/*VSYNC_MAIN*/);
				if (ret > 0)
					tcc_vsync_set_deinterlace_mode(0);
			}
		}
		break;
#endif
	case TCC_VIDEO_CREATE_DMA_BUF:
		{
			stVpuPhysInfo pmap_info;
			if (copy_from_user((void *)&pmap_info,
				(const void *)arg,
				sizeof(stVpuPhysInfo)))	{
				ret = -EFAULT;
			} else {
				ret = tcc_mem_create_dma_buf(&pmap_info);
				if (!ret) {
					if (copy_to_user((stVpuPhysInfo *)arg,
						&pmap_info,
						sizeof(stVpuPhysInfo)))	{
						tcc_mem_release_dma_buf(pmap_info.fd);
						ret = -EFAULT;
					}
				}
			}
		}
		break;
	case TCC_VIDEO_RELEASE_DMA_BUF:
		{
			int fd;
			if (copy_from_user((void *)&fd,
				(void *)arg,
				sizeof(int))) {
				ret = -EFAULT;
			} else {
				ret = tcc_mem_release_dma_buf(fd);
			}
		}
		break;

	default:
		V_DBG(VPU_DBG_ERROR,
			"Unsupported cmd(0x%x) for vmem_ioctl_fn.", cmd);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int tcc_dma_buf_attach(struct dma_buf *buf,
						struct device *dev,
						struct dma_buf_attachment *attach)
{
	return 0;
}

static void tcc_dma_buf_detach(struct dma_buf *buf,
						struct dma_buf_attachment *attach)
{
}

static struct sg_table *tcc_dma_buf_map(struct dma_buf_attachment *attach,
						enum dma_data_direction dir)
{
	struct tcc_dma_buf_priv *dma_buf_priv = attach->dmabuf->priv;
	struct sg_table *sgt = NULL;
	if (WARN_ON(dir == DMA_NONE || !dma_buf_priv)) {
		V_DBG(VPU_DBG_ERROR,
			"%s %d dir(%d) dma_buf_priv=%d dmabuf=%p\n",
			__func__,__LINE__,dir,dma_buf_priv,attach->dmabuf);
		return ERR_PTR(-EINVAL);
	}

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (sgt) {
		if (sg_alloc_table(sgt, 1, GFP_KERNEL))	{
			V_DBG(VPU_DBG_ERROR,
				"%s %d sg_alloc_table() failed \n",__func__,__LINE__);
			kfree(sgt);
			sgt = NULL;
		} else {
			sg_dma_address(sgt->sgl) = dma_buf_priv->phys;
			sg_dma_len(sgt->sgl) = dma_buf_priv->size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR,"%s %d \n",__func__,__LINE__);
	}

    return sgt;
}

static void tcc_dma_buf_unmap(struct dma_buf_attachment *attach,
						struct sg_table *sgt,
						enum dma_data_direction dir)
{
	if(sgt)	{
		sg_free_table(sgt);
		kfree(sgt);
	}
}

static void tcc_dma_buf_release(struct dma_buf *buf)
{
	struct tcc_dma_buf_priv *dma_buf_priv;
	if(buf) {
		dma_buf_priv = buf->priv;
		#if 1
		if(dma_buf_priv) {
			kfree(dma_buf_priv);
		}
		#endif
		buf->priv = NULL;
	} else {
		V_DBG(VPU_DBG_ERROR,"%s %d buf is null\n",__func__,__LINE__);
	}
}

static int tcc_dma_buf_begin_cpu_access(struct dma_buf *buf,
						enum dma_data_direction direction)
{
	return 0;
}

static int tcc_dma_buf_end_cpu_access(struct dma_buf *buf,
						enum dma_data_direction direction)
{
	return 0;
}

static void *tcc_dma_buf_kmap(struct dma_buf *buf, unsigned long page)
{
	return NULL;
}

static void tcc_dma_buf_kunmap(struct dma_buf *buf,
						unsigned long page,
						void *vaddr)
{
}

static void *tcc_dma_buf_kmap_atomic(struct dma_buf *buf,
						unsigned long page_num)
{
	return NULL;
}

static void tcc_dma_buf_vm_open(struct vm_area_struct *vma)
{
}

static void tcc_dma_buf_vm_close(struct vm_area_struct *vma)
{
}

static int tcc_dma_buf_vm_fault(struct vm_fault *vmf)
{
	return 0;
}

static const struct vm_operations_struct tcc_dma_buf_vm_ops = {
	.open = tcc_dma_buf_vm_open,
	.close = tcc_dma_buf_vm_close,
	.fault = tcc_dma_buf_vm_fault,
};

static int tcc_dma_buf_mmap(struct dma_buf *buf,
						struct vm_area_struct *vma)
{
	int ret = -EINVAL;
	if(vma && buf) 	{
		struct tcc_dma_buf_priv *dma_buf_priv = buf->priv;
		if(dma_buf_priv) {
			pgprot_t prot = vm_get_page_prot(vma->vm_flags);

			vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
			vma->vm_ops = &tcc_dma_buf_vm_ops;
			vma->vm_private_data = dma_buf_priv;
			vma->vm_page_prot = pgprot_writecombine(prot);

			ret = remap_pfn_range(vma, vma->vm_start,
								dma_buf_priv->phys >> PAGE_SHIFT,
								vma->vm_end - vma->vm_start,
								vma->vm_page_prot);
		} else {
			V_DBG(VPU_DBG_ERROR,
				"%s %d buf %p , vma %p.. dma_buf_priv is null\n",
				__func__,__LINE__,buf,vma);
		}
	} else {
		V_DBG(VPU_DBG_ERROR,
			"%s %d Invalid argument buf %p , vma %p\n"
			,__func__,__LINE__,buf,vma);
	}
	return ret;
}

static const struct dma_buf_ops vpu_dma_buf_ops = {
	.attach = tcc_dma_buf_attach,
	.detach = tcc_dma_buf_detach,
	.map_dma_buf = tcc_dma_buf_map,
	.unmap_dma_buf = tcc_dma_buf_unmap,
	.release = tcc_dma_buf_release,
	.mmap = tcc_dma_buf_mmap,
	.begin_cpu_access = tcc_dma_buf_begin_cpu_access,
	.end_cpu_access = tcc_dma_buf_end_cpu_access,
	.map = tcc_dma_buf_kmap,
	.unmap = tcc_dma_buf_kunmap,
	.map_atomic = tcc_dma_buf_kmap_atomic
};

#define TEST_VERSION "2021.0504"

static int tcc_mem_create_dma_buf(stVpuPhysInfo *pmap_info)
{
	int ret = 0;
	struct tcc_dma_buf_priv *dma_buf_priv = NULL;

	dma_buf_priv = kzalloc(sizeof(struct tcc_dma_buf_priv), GFP_KERNEL);
	if (dma_buf_priv) {

		/* Telechips specific code for get the reserved memory */
		dma_buf_priv->phys = (dma_addr_t)pmap_info->phys;
		dma_buf_priv->size = (size_t)pmap_info->size;
		/* This allocates virtual address */
		dma_buf_priv->virt = ioremap_nocache(dma_buf_priv->phys, dma_buf_priv->size);

		if (!dma_buf_priv->virt) {
			V_DBG(VPU_DBG_ERROR,
				"%s %d phys:0x%lx size:0x%lx.. ioremap_nocache() failed \n",
				__func__,__LINE__,pmap_info->phys,pmap_info->size);
			ret = -ENOMEM;
			goto free_object;
		}

		struct dma_buf_export_info export_info = {
			.exp_name = "vpu_dma_exp",
			.owner = THIS_MODULE,
			.ops = &vpu_dma_buf_ops,
			.size = dma_buf_priv->size,
			.flags = O_CLOEXEC | O_RDWR,
			.priv = dma_buf_priv,
		};
		dma_buf_priv->buf = dma_buf_export(&export_info);

		if(dma_buf_priv->buf) {
			if(IS_ERR(dma_buf_priv->buf))
			{
				V_DBG(VPU_DBG_ERROR,
					"%s %d phys:0x%lx size:0x%lx.. IS_ERR(buf)\n",
					__func__,__LINE__,pmap_info->phys,pmap_info->size);
				ret = PTR_ERR(dma_buf_priv->buf);
				goto free_object;
			} else {
				/* get the dma-buf fd for pass to target device through userspace */
				pmap_info->fd = dma_buf_fd(dma_buf_priv->buf, O_CLOEXEC);
				if (pmap_info->fd < 0) {
					V_DBG(VPU_DBG_ERROR,
						"%s %d phys:0x%lx size:0x%lx.. dma_buf_fd(buf)\n",
						__func__,__LINE__,pmap_info->phys,pmap_info->size);
					dma_buf_put(dma_buf_priv->buf);
					goto free_object;
				} else {
					ret = 0;
				}
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
				"%s %d phys:0x%lx size:0x%lx.. dma_buf_export() failed\n",
				__func__,__LINE__,pmap_info->phys,pmap_info->size);
			goto free_object;
		}
	} else {
		V_DBG(VPU_DBG_ERROR,
			"%s %d phys:0x%lx size:0x%lx.. kzalloc() failed\n",
			__func__,__LINE__,pmap_info->phys,pmap_info->size);
	}

    return ret;

free_object:
	if(dma_buf_priv) {
		dma_buf_priv->virt = NULL;
		kfree(dma_buf_priv);
		dma_buf_priv = NULL;
	}
    return ret;
}

static int tcc_mem_release_dma_buf(int fd)
{
	struct dma_buf *dmabuf = NULL;
	struct tcc_dma_buf_priv *dma_buf_priv = NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		V_DBG(VPU_DBG_ERROR,
			"%s %d fd %d, IS_ERR_OR_NULL(dmabuf) \n",
			__func__,__LINE__,fd);
		return -EINVAL;
	}

	dma_buf_priv = dmabuf->priv;
	if(dma_buf_priv) {
		/* Unmap virtual address */
		if (dma_buf_priv->virt)	{
			iounmap(dma_buf_priv->virt);
		}

		dma_buf_put(dma_buf_priv->buf);
	} else {
		V_DBG(VPU_DBG_ERROR,
			"%s %d fd %d, dma_buf_priv is null\n",
			__func__,__LINE__,fd);
	}

	return 0;
}


#ifdef CONFIG_COMPAT
static long vmem_compat_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	return vmem_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static unsigned int vmem_poll(struct file *filp, poll_table *wait)
{
	struct _vpu_dec_data_t *vpu_data
			= (struct _vpu_dec_data_t *) filp->private_data;

	if (vpu_data == NULL)
		return POLLERR | POLLNVAL;

	if (vpu_data->count == 0)
		poll_wait(filp, &(vpu_data->wq), wait);

	if (vpu_data->count > 0) {
		vpu_data->count--;
		return POLLIN;
	}

	return 0;
}

static int vmem_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int vmem_release(struct inode *inode, struct file *filp)
{
	V_DBG(VPU_DBG_CLOSE, "Out!!");
	return 0;
}

static int vmem_mmap(struct file *file, struct vm_area_struct *vma)
{
#if defined(CONFIG_TCC_MEM)

	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		V_DBG(VPU_DBG_ERROR,
			"mem_mmap: this address is not allowed");
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
						vma->vm_end - vma->vm_start,
						vma->vm_page_prot)) {
		V_DBG(VPU_DBG_ERROR,
			"mem_mmap :: remap_pfn_range failed");
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= VM_IO;
	vma->vm_flags |= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static const struct file_operations vdev_mem_fops = {
	.owner = THIS_MODULE,
	.open = vmem_open,
	.release = vmem_release,
	.mmap = vmem_mmap,
	.unlocked_ioctl = vmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vmem_compat_ioctl,
#endif
	.poll = vmem_poll,
};

static struct miscdevice vmem_misc_device = {
	MISC_DYNAMIC_MINOR,
	MEM_NAME,
	&vdev_mem_fops,
};

int vmem_probe(struct platform_device *pdev)
{
	if (misc_register(&vmem_misc_device)) {
		pr_info("VPU mem: Couldn't register device.\n");
		return -EBUSY;
	}

	return 0;
}

int vmem_remove(struct platform_device *pdev)
{
	misc_deregister(&vmem_misc_device);
	return 0;
}
