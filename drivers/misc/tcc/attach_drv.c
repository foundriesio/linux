/****************************************************************************
 * Copyright (C) 2015 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

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

#include <video/tcc/vioc_rdma.h>
#include <video/tcc/vioc_wdma.h>
#include <video/tcc/vioc_wmix.h>
#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_scaler.h>
#include <video/tcc/vioc_global.h>
#include <video/tcc/vioc_intr.h>

#include <video/tcc/tcc_attach_ioctrl.h>

struct attach_drv_vioc {
    volatile void __iomem *reg;
    unsigned int id;
    unsigned int path;
};

struct attach_drv_type {
    unsigned int        id;
    unsigned int        irq;
    struct miscdevice   *misc;

    struct vioc_intr_type   *vioc_intr;
    struct attach_drv_vioc  rdma;
    struct attach_drv_vioc  wmix;
    struct attach_drv_vioc  sc;
    struct attach_drv_vioc  wdma;

    struct clk      *clk;
    ATTACH_INFO_TYPE    *info;

	atomic_t buf_idx;
	atomic_t irq_reged;
	atomic_t dev_opened;
    atomic_t block_operating;
    atomic_t block_waiting;

    wait_queue_head_t   poll_wq;
    wait_queue_head_t   cmd_wq;
    spinlock_t      poll_lock;
};

extern int range_is_allowed(unsigned long pfn, unsigned long size);
static int attach_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct attach_drv_type *attach = dev_get_drvdata(misc->parent);

	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		pr_err("[ERR][ATTACH] %s: %s: Address range is not allowed.\n", __func__, attach->misc->name);
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops     =  NULL;
	vma->vm_flags   |= VM_IO;
	vma->vm_flags   |= VM_DONTEXPAND | VM_PFNMAP;
	return 0;
}

static unsigned int attach_drv_poll(struct file *filp, poll_table *wait)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    struct attach_drv_type *attach = dev_get_drvdata(misc->parent);
    int ret = 0;

	if (attach) {
		poll_wait(filp, &(attach->poll_wq), wait);
		spin_lock_irq(&(attach->poll_lock));

		if (atomic_read(&attach->block_operating) == 0)
			ret = (POLLIN|POLLRDNORM);

		spin_unlock_irq(&(attach->poll_lock));
	} else {
		ret = -EFAULT;
	}

    return ret;
}

static void attach_drv_update(struct attach_drv_type *attach)
{
	ATTACH_INFO_TYPE    *attach_info = attach->info;
	unsigned int channel =
		(attach->rdma.id > VIOC_RDMA03) ? get_vioc_index((attach->rdma.id - VIOC_RDMA04)) : get_vioc_index(attach->rdma.id);
	unsigned int idx = atomic_read(&attach->buf_idx);

//	pr_info("%s: size(%dx%d) pos(%d,%d)\n", __func__, attach_info->img_width, attach_info->img_height,
//			attach_info->offset_x, attach_info->offset_y);

	VIOC_RDMA_SetImageSize(attach->rdma.reg, attach_info->img_width, attach_info->img_height);
	VIOC_RDMA_SetImageFormat(attach->rdma.reg, attach_info->fmt);
	VIOC_RDMA_SetImageOffset(attach->rdma.reg, attach_info->fmt, attach_info->img_width);
	VIOC_RDMA_SetImageY2RMode(attach->rdma.reg, 0x2);
	if(attach_info->fmt > VIOC_IMG_FMT_COMP)
		VIOC_RDMA_SetImageY2REnable(attach->rdma.reg, 1);
	else
		VIOC_RDMA_SetImageY2REnable(attach->rdma.reg, 0);
	VIOC_RDMA_SetImageBase(attach->rdma.reg,
			attach_info->addr_y[idx], attach_info->addr_u[idx], attach_info->addr_v[idx]);

	VIOC_WMIX_SetPosition(attach->wmix.reg, channel, attach_info->offset_x, attach_info->offset_y);
	VIOC_WMIX_SetUpdate(attach->wmix.reg);

	VIOC_RDMA_SetImageEnable(attach->rdma.reg);
}

static void attach_drv_ctrl(struct attach_drv_type *attach)
{
	ATTACH_INFO_TYPE    *attach_info = attach->info;
	unsigned int idx;

	atomic_set(&attach->buf_idx, 0);
	idx = atomic_read(&attach->buf_idx);

//	pr_info("Buf : addr:0x%x 0x%x 0x%x  fmt:%d pos(%d,%d) size(%dx%d)\n",
//			attach_info->addr_y[idx], attach_info->addr_u[idx], attach_info->addr_v[idx], attach_info->fmt,
//			attach_info->offset_x, attach_info->offset_y, attach_info->img_width, attach_info->img_height);

	VIOC_SC_SetBypass(attach->sc.reg, 0);
	VIOC_SC_SetOutPosition(attach->sc.reg, 0, 0);
	VIOC_SC_SetDstSize(attach->sc.reg, attach_info->img_width, attach_info->img_height);
	VIOC_SC_SetOutSize(attach->sc.reg, attach_info->img_width, attach_info->img_height);
	VIOC_CONFIG_PlugIn(attach->sc.id, attach->wdma.id);
	VIOC_SC_SetUpdate(attach->sc.reg);

	VIOC_WDMA_SetImageOffset(attach->wdma.reg, attach_info->fmt, attach_info->img_width);
	VIOC_WDMA_SetImageFormat(attach->wdma.reg, attach_info->fmt);
	VIOC_WDMA_SetImageSize(attach->wdma.reg, attach_info->img_width, attach_info->img_height);
	VIOC_WDMA_SetImageBase(attach->wdma.reg,
			attach_info->addr_y[idx], attach_info->addr_u[idx], attach_info->addr_v[idx]);
	VIOC_WDMA_SetImageR2YMode(attach->wdma.reg, 0x2);
	if (attach_info->fmt > VIOC_IMG_FMT_COMP )
		VIOC_WDMA_SetImageR2YEnable(attach->wdma.reg, 1);
	else
		VIOC_WDMA_SetImageR2YEnable(attach->wdma.reg, 0);
	VIOC_WDMA_SetImageEnable(attach->wdma.reg, 0/*OFF*/);

	vioc_intr_clear(attach->vioc_intr->id, attach->vioc_intr->bits);
	if(attach_info->mode == ATTACH_CAPTURE_MODE) {
		if(wait_event_interruptible_timeout(attach->poll_wq, (atomic_read(&attach->block_operating) == 0), msecs_to_jiffies(500)) <= 0) {
			atomic_set(&attach->block_operating, 0);
			pr_err("[ERR][ATTACH] %s: %s time-out \n", __func__, attach->misc->name);
		}
	}
}

static irqreturn_t attach_drv_handler(int irq, void *client_data)
{
	struct attach_drv_type *attach = (struct attach_drv_type *)client_data;
	unsigned int idx;

	if (is_vioc_intr_activatied(attach->vioc_intr->id, attach->vioc_intr->bits) == false)
		return IRQ_NONE;
	vioc_intr_clear(attach->vioc_intr->id, attach->vioc_intr->bits);

	if (atomic_read(&attach->block_operating)) {
		atomic_set(&attach->block_operating, 0);
		wake_up_interruptible(&attach->poll_wq);
	}

    if (atomic_read(&attach->block_waiting)) {
		atomic_set(&attach->block_waiting, 0);
        wake_up_interruptible(&attach->cmd_wq);
		goto end_isr;
	}

	if(attach->info->mode == ATTACH_PREVIEW_MODE) {
		attach_drv_update(attach);

		atomic_inc(&attach->buf_idx);
		if(atomic_read(&attach->buf_idx) >= ATTACH_BUF_NUM)
			atomic_set(&attach->buf_idx, 0);

		idx = atomic_read(&attach->buf_idx);
		if(attach->info->addr_y[idx] == 0) {
			atomic_set(&attach->buf_idx, 0);
			idx = 0;
		}

		VIOC_WDMA_SetImageBase(attach->wdma.reg,
				attach->info->addr_y[idx], attach->info->addr_u[idx], attach->info->addr_v[idx]);
		VIOC_WDMA_SetImageEnable(attach->wdma.reg, 0/*OFF*/);

		atomic_set(&attach->block_operating, 1);
	}
end_isr:
	return IRQ_HANDLED;
}

static long attach_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct miscdevice *misc = (struct miscdevice *)filp->private_data;
    struct attach_drv_type *attach = dev_get_drvdata(misc->parent);
    int ret = 0;

//	pr_debug("[DBG][ATTACH] %s(): %s: cmd(%d) \n", __func__, attach->misc->name, cmd);

	if(atomic_read(&attach->block_operating)) {
		atomic_set(&attach->block_waiting, 1);
		ret = wait_event_interruptible_timeout(attach->cmd_wq, (atomic_read(&attach->block_waiting) == 0), msecs_to_jiffies(200));
		if(ret <= 0) {
			atomic_set(&attach->block_waiting, 0);
			pr_err("[ERR][ATTACH] %s: %s timed_out block_waiting:%d!! \n",
					__func__, attach->misc->name, atomic_read(&attach->block_waiting));
		}
		ret = 0;
	}

    switch(cmd) {
        case TCC_ATTACH_IOCTRL:
        case TCC_ATTACH_IOCTRL_KERNEL:
            if(cmd == TCC_ATTACH_IOCTRL_KERNEL){
                memcpy(attach->info,(ATTACH_INFO_TYPE*)arg, sizeof(ATTACH_INFO_TYPE));
            }else{
                if(copy_from_user(attach->info, (ATTACH_INFO_TYPE *)arg, sizeof(ATTACH_INFO_TYPE))) {
                    pr_err("[ERR][ATTACH] %s: Not Supported copy_from_user(%d). \n", __func__, cmd);
                    ret = -EFAULT;
                }
            }

			if(attach->info->mode == ATTACH_CAPTURE_MODE)
				atomic_set(&attach->block_operating, 1);

			attach_drv_ctrl(attach);
			break;
		case TCC_ATTACH_GET_BUF_IOCTRL:
		case TCC_ATTACH_GET_BUF_IOCTRL_KERNEL:
			if(attach->info->mode) {
				int idx = atomic_read(&attach->buf_idx);
				if(cmd == TCC_ATTACH_GET_BUF_IOCTRL_KERNEL) {
					arg = (unsigned long)attach->info->addr_y[idx];
				} else {
					unsigned int addr = attach->info->addr_y[idx];
					if(copy_to_user((unsigned long *)arg, &addr, sizeof(unsigned int))) {
						pr_err("[ERR][ATTACH] %s: Not Supported copy_to_user(%d). \n", __func__, cmd);
						ret = -EFAULT;
					}
				}
			} else {
				pr_err("[ERR][ATTACH] %s: %s driver is not running(%d). \n",
						__func__, attach->misc->name, attach->info->mode);
				ret = -EBUSY;
			}
			break;
        default:
            pr_err("[ERR][ATTACH] %s: not supported %s IOCTL(0x%x). \n", __func__, attach->misc->name, cmd);
			ret = -EINVAL;
            break;
    }

    return ret;
}

#ifdef CONFIG_COMPAT
static long attach_drv_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return attach_drv_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int attach_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct attach_drv_type *attach = dev_get_drvdata(misc->parent);
	unsigned int channel =
		(attach->rdma.id > VIOC_RDMA03) ? get_vioc_index((attach->rdma.id - VIOC_RDMA04)) : get_vioc_index(attach->rdma.id);

	if (atomic_read(&attach->dev_opened))
		atomic_dec(&attach->dev_opened);

	if (atomic_read(&attach->dev_opened) == 0) {
		if(atomic_read(&attach->block_operating)) {
		atomic_set(&attach->block_waiting, 1);
		if(wait_event_interruptible_timeout(attach->cmd_wq, (atomic_read(&attach->block_waiting) == 0), msecs_to_jiffies(200)) <= 0) {
			atomic_set(&attach->block_waiting, 0);
				pr_err("[ERR][ATTACH] %s: %s timed_out block_waiting:%d!! \n", __func__,
						attach->misc->name, atomic_read(&attach->block_waiting));
			}
		}

		if(atomic_read(&attach->irq_reged)) {
			vioc_intr_clear(attach->vioc_intr->id, attach->vioc_intr->bits);
			vioc_intr_disable(attach->irq, attach->vioc_intr->id, attach->vioc_intr->bits);
			free_irq(attach->irq, attach);
			atomic_dec(&attach->irq_reged);
		}

		VIOC_WDMA_SetImageDisable(attach->wdma.reg);
		VIOC_CONFIG_SWReset(attach->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(attach->wdma.id, VIOC_CONFIG_CLEAR);

		VIOC_CONFIG_PlugOut(attach->sc.id);
		VIOC_CONFIG_SWReset(attach->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(attach->sc.id, VIOC_CONFIG_CLEAR);

		VIOC_RDMA_SetImageDisable(attach->rdma.reg);
		VIOC_CONFIG_SWReset(attach->rdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(attach->rdma.id, VIOC_CONFIG_CLEAR);

		VIOC_WMIX_SetPosition(attach->wmix.reg, channel, 0, 0);
		VIOC_WMIX_SetUpdate(attach->wmix.reg);

		if (attach->clk)
			clk_disable_unprepare(attach->clk);
	}

	pr_info("[INF][ATTACH] %s_release OUT:  %d'th. \n", attach->misc->name, atomic_read(&attach->dev_opened));

	return 0;
}

static int attach_drv_open(struct inode *inode, struct file *filp)
{
    struct miscdevice   *misc = (struct miscdevice *)filp->private_data;
    struct attach_drv_type  *attach = dev_get_drvdata(misc->parent);

    int ret = 0;

	if(atomic_read(&attach->dev_opened))
		return -EFAULT;

	if(atomic_read(&attach->dev_opened) == 0) {
		if (attach->clk)
			clk_prepare_enable(attach->clk);

		VIOC_CONFIG_PlugOut(attach->sc.id);
		VIOC_CONFIG_SWReset(attach->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(attach->sc.id, VIOC_CONFIG_CLEAR);

		VIOC_CONFIG_SWReset(attach->wdma.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(attach->wdma.id, VIOC_CONFIG_CLEAR);

		if (atomic_read(&attach->irq_reged) == 0) {
			synchronize_irq(attach->irq);
			vioc_intr_clear(attach->vioc_intr->id, attach->vioc_intr->bits);
			ret = request_irq(attach->irq, attach_drv_handler, IRQF_SHARED, attach->misc->name, attach);
			if (ret) {
				if (attach->clk)
					clk_disable_unprepare(attach->clk);
				pr_err("[ERR][ATTACH] %s: failed to aquire %s request_irq. \n", __func__, attach->misc->name);
				return -EFAULT;
			}
			vioc_intr_enable(attach->irq, attach->vioc_intr->id, attach->vioc_intr->bits);
			atomic_inc(&attach->irq_reged);
		}

		atomic_inc(&attach->dev_opened);
	}

    pr_info("[INF][ATTACH] %s_open OUT:  %d'th. \n", attach->misc->name, atomic_read(&attach->dev_opened));

    return ret;
}

static struct file_operations attach_drv_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = attach_drv_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = attach_drv_compat_ioctl, // for 32 bit OMX ioctl
#endif
    .mmap           = attach_drv_mmap,
    .open           = attach_drv_open,
    .release        = attach_drv_release,
    .poll           = attach_drv_poll,
};

static void attach_drv_parse_dt(struct attach_drv_type *attach, struct device_node *node)
{
	struct device_node *dev_np = NULL;
	unsigned int index = 0;

    dev_np = of_parse_phandle(node, "rdma", 0);
    if(dev_np) {
        if(!of_property_read_u32_index(node, "rdma", 1, &index)) {
            attach->rdma.reg = VIOC_RDMA_GetAddress(index);
            attach->rdma.id = index;
        } else {
            attach->rdma.reg = NULL;
        }
    } else {
        pr_err("[ERR][ATTACH] %s: could not find rdma node of %s driver. \n", __func__, attach->misc->name);
        attach->rdma.reg = NULL;
    }

    dev_np = of_parse_phandle(node, "wmix", 0);
    if(dev_np) {
        of_property_read_u32_index(node, "wmix", 1, &index);
        attach->wmix.reg = VIOC_WMIX_GetAddress(index);
        attach->wmix.id = index;
    } else {
        pr_err("[ERR][ATTACH] %s: could not find wmix node of %s driver. \n", __func__, attach->misc->name);
        attach->wmix.reg = NULL;
    }

    dev_np = of_parse_phandle(node, "scaler", 0);
    if(dev_np) {
        of_property_read_u32_index(node, "scaler", 1, &index);
        attach->sc.reg = VIOC_SC_GetAddress(index);
        attach->sc.id = index;
    } else {
        pr_err("[ERR][ATTACH] %s: could not find attach node of %s driver. \n", __func__, attach->misc->name);
        attach->sc.reg = NULL;
    }

    dev_np = of_parse_phandle(node, "wdma", 0);
    if(dev_np) {
        of_property_read_u32_index(node, "wdma", 1, &index);
        attach->wdma.reg = VIOC_WDMA_GetAddress(index);
        attach->wdma.id = index;
        attach->irq = irq_of_parse_and_map(dev_np, get_vioc_index(index));
        attach->vioc_intr->id   = VIOC_INTR_WD0 + get_vioc_index(attach->wdma.id);
        attach->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
    } else {
        pr_err("[ERR][ATTACH] %s: could not find wdma node of %s driver. \n", __func__, attach->misc->name);
        attach->wdma.reg = NULL;
    }
}

static int attach_drv_probe(struct platform_device *pdev)
{
    struct attach_drv_type *attach;
    int ret = -ENODEV;

    attach = kzalloc(sizeof(struct attach_drv_type), GFP_KERNEL);
    if (!attach)
        return -ENOMEM;

    attach->clk = of_clk_get(pdev->dev.of_node, 0);
    if (IS_ERR(attach->clk))
        attach->clk = NULL;

    attach->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
    if (attach->misc == 0)
        goto err_misc_alloc;

    attach->info = kzalloc(sizeof(ATTACH_INFO_TYPE), GFP_KERNEL);
    if (attach->info == 0)
        goto err_info_alloc;

    attach->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
    if (attach->vioc_intr == 0)
        goto err_vioc_intr_alloc;

    attach->id = of_alias_get_id(pdev->dev.of_node, "attach-drv");

    /* register attach discdevice */
    attach->misc->minor = MISC_DYNAMIC_MINOR;
    attach->misc->fops = &attach_drv_fops;
    attach->misc->name = kasprintf(GFP_KERNEL,"attach%d", attach->id);
    attach->misc->parent = &pdev->dev;
	ret = misc_register(attach->misc);
	if(ret)
		goto err_misc_register;

	attach_drv_parse_dt(attach, pdev->dev.of_node);

    platform_set_drvdata(pdev, attach);

    spin_lock_init(&attach->poll_lock);
    init_waitqueue_head(&attach->poll_wq);
    init_waitqueue_head(&attach->cmd_wq);

	atomic_set(&attach->dev_opened, 0);
	atomic_set(&attach->irq_reged, 0);
	atomic_set(&attach->buf_idx, 0);
	atomic_set(&attach->block_operating, 0);
	atomic_set(&attach->block_waiting, 0);

    pr_info("[INF][ATTACH] %s: id:%d, Attach Driver Initialized\n", pdev->name, attach->id);
    return 0;

    misc_deregister(attach->misc);
err_misc_register:
    kfree(attach->vioc_intr);
err_vioc_intr_alloc:
    kfree(attach->info);
err_info_alloc:
    kfree(attach->misc);
err_misc_alloc:
    kfree(attach);

    pr_info("[INF][ATTACH] %s: %s: err ret:%d \n", __func__, pdev->name, ret);
    return ret;
}

static int attach_drv_remove(struct platform_device *pdev)
{
    struct attach_drv_type *attach = (struct attach_drv_type *)platform_get_drvdata(pdev);

    misc_deregister(attach->misc);
    kfree(attach->vioc_intr);
    kfree(attach->info);
    kfree(attach->misc);
    kfree(attach);
    return 0;
}

static int attach_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
    // TODO:
    return 0;
}

static int attach_drv_resume(struct platform_device *pdev)
{
    // TODO:
    return 0;
}

static struct of_device_id attach_of_match[] = {
    { .compatible = "telechips,attach_drv" },
    {}
};
MODULE_DEVICE_TABLE(of, attach_of_match);

static struct platform_driver attach_driver = {
    .probe      = attach_drv_probe,
    .remove     = attach_drv_remove,
    .suspend    = attach_drv_suspend,
    .resume     = attach_drv_resume,
    .driver     = {
        .name   = "attach_pdev",
        .owner  = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(attach_of_match),
#endif
    },
};

static int __init attach_drv_init(void)
{
    return platform_driver_register(&attach_driver);
}

static void __exit attach_drv_exit(void)
{
    platform_driver_unregister(&attach_driver);
}

module_init(attach_drv_init);
module_exit(attach_drv_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips Attach Driver");
MODULE_LICENSE("GPL");

