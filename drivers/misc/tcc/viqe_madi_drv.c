/****************************************************************************
 * Copyright (C) 2018 Telechips Inc.
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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <soc/tcc/pmap.h>

#include <video/tcc/vioc_intr.h>
#include <video/tcc/tcc_types.h>

#include <video/tcc/viqe_madi_ioctl.h>

#include <video/tcc/vioc_config.h>
#include <video/tcc/vioc_ddicfg.h>
#include <video/tcc/viqe_madi.h>

#if defined(EN_MADI_VERIFICATION)
static int debug = 1;
#else
static int debug = 0;
#endif
#define dprintk(fmt, args...) if(debug) printk("\e[38m"fmt"\e[0m", ## args);

#define __madi_dreg_r	__raw_readl
#define __madi_dreg_w	__raw_writel

#define MAX_FB_WIDTH (1920)
#define MAX_FB_HEIGHT (1088)

unsigned int g_width;	//alank
unsigned int g_height;

struct viqe_madi_data {
	// wait for poll
	wait_queue_head_t	poll_wq;
	spinlock_t			poll_lock;
	unsigned int		poll_count;

	// wait for ioctl command
	wait_queue_head_t	cmd_wq;
	spinlock_t			cmd_lock;
	unsigned int		cmd_count;

	struct mutex		io_mutex;
	unsigned char		block_operating;
	unsigned char		block_waiting;
	unsigned char		irq_reged;
	unsigned int		dev_opened;
};

struct viqe_madi_info_type {
	stVIQE_MADI_INIT_TYPE init;
	unsigned int first_frame;
	unsigned int skip_count;
	unsigned int max_buffer_cnt;
	unsigned int Yoffset_bottom;
	unsigned int Coffset_bottom;
	unsigned int curr_src_index;
	unsigned int curr_out_index;
	unsigned int source_Yaddr[MADI_ADDR_MAX];
	unsigned int source_Caddr[MADI_ADDR_MAX];
};

struct viqe_madi_type {
	struct vioc_intr_type	*vioc_intr;

	unsigned int		id;
	unsigned int		irq;
	unsigned int 		irq_status;
	struct miscdevice	*misc;

	struct clk					*ddi_madi_clk;
	struct clk					*peri_madi_clk;

	struct viqe_madi_data		*data;
	struct viqe_madi_info_type	*info;
};

extern void tccxxx_GetAddress(unsigned char format, unsigned int base_Yaddr, unsigned int src_imgx, unsigned int  src_imgy,
								unsigned int start_x, unsigned int start_y, unsigned int* Y, unsigned int* U,unsigned int* V);
extern int range_is_allowed(unsigned long pfn, unsigned long size);

static int viqe_madi_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
		printk(KERN_ERR	 "%s():  This address is not allowed. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma,vma->vm_start, vma->vm_pgoff , vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		printk(KERN_ERR	 "%s():  Virtual address page port error. \n", __func__);
		return -EAGAIN;
	}

	vma->vm_ops		= NULL;
	vma->vm_flags 	|= VM_IO;
	vma->vm_flags 	|= VM_DONTEXPAND | VM_PFNMAP;

	return 0;
}

static unsigned int viqe_madi_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct viqe_madi_type	*viqe_madi = dev_get_drvdata(misc->parent);
	int ret = 0;

	if(viqe_madi->data == NULL)
		return 0;

	poll_wait(filp, &(viqe_madi->data->poll_wq), wait);
	spin_lock_irq(&(viqe_madi->data->poll_lock));
	if (viqe_madi->data->block_operating == 0)
		ret = (POLLIN|POLLRDNORM);
	spin_unlock_irq(&(viqe_madi->data->poll_lock));

	return ret;
}

static irqreturn_t viqe_madi_handler(int irq, void *client_data)
{
	struct viqe_madi_type *viqe_madi = (struct viqe_madi_type *)client_data;
	volatile void __iomem *reg = NULL;
    unsigned int raw;
    unsigned int enable;

	reg = VIQE_MADI_GetAddress(VMADI_TIMMING);

    raw = __madi_dreg_r(reg + MADITIMMING_GEN_CFG_STS_IREQ_RAW_OFFSET);
    enable = __madi_dreg_r(reg + MADITIMMING_GEN_CFG_IREQ_EN_OFFSET);

    if ( enable & raw & MADI_INT_START ) {
		dprintk("DDEI_TG Start! \n");
    }

    if ( enable & raw & MADI_INT_ACTIVATED ) {
		unsigned int sts_active = __madi_dreg_r(reg + MADITIMMING_GEN_CFG_STS_ACTIVE_OFFSET);
        sts_active += 1;
		__madi_dreg_w(sts_active, reg + MADITIMMING_GEN_CFG_STS_ACTIVE_OFFSET);
        dprintk("DDEI_TG Activated! \n");
    }

	if ( enable & raw & MADI_INT_VDEINT ) {
		dprintk("DDEI_TG V_DEINT! \n");
    }

	if ( enable & raw & MADI_INT_VNR ) {
		dprintk("DDEI_TG V_NR! \n");
    }

    if ( enable & raw & MADI_INT_DEACTIVATED ) {
		dprintk("DDEI_TG Deactivated! \n");
		dprintk("%s():  block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
				viqe_madi->data->block_operating, viqe_madi->data->block_waiting, viqe_madi->data->cmd_count, viqe_madi->data->poll_count);

		if(viqe_madi->data->block_operating >= 1)
			viqe_madi->data->block_operating = 0;

		wake_up_interruptible(&(viqe_madi->data->poll_wq));

		if(viqe_madi->data->block_waiting)
			wake_up_interruptible(&viqe_madi->data->cmd_wq);
    }

	viqe_madi->irq_status = raw;
    __madi_dreg_w(raw, reg + MADITIMMING_GEN_CFG_IREQ_CLR_OFFSET);

	return IRQ_HANDLED;
}

static long viqe_madi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct viqe_madi_type	*viqe_madi = dev_get_drvdata(misc->parent);
	int ret = 0;

	mutex_lock(&viqe_madi->data->io_mutex);

	if( viqe_madi->data->block_operating || viqe_madi->data->block_waiting )
		dprintk("%s():  cmd(%d), block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d). \n", __func__, 	\
				cmd, viqe_madi->data->block_operating, viqe_madi->data->block_waiting, viqe_madi->data->cmd_count, viqe_madi->data->poll_count);

	switch(cmd) {
		case TCC_VIQE_MADI_INIT:
			{
				if(copy_from_user(&viqe_madi->info->init,(void *)arg, sizeof(stVIQE_MADI_INIT_TYPE))) {
					printk(KERN_ALERT "%s():  Error copy_from_user(%d)\n", __func__, cmd);
					ret = -EFAULT;
				}
				else {
					viqe_madi->info->first_frame = 1;
					viqe_madi->info->skip_count = 1 * 2;

					if(viqe_madi->info->init.src_one_field_only_frame)
						viqe_madi->info->max_buffer_cnt = MADI_ADDR_MAX;
					else
						viqe_madi->info->max_buffer_cnt = MADI_ADDR_MAX/2;

					viqe_madi->info->Yoffset_bottom = viqe_madi->info->init.src_ImgWidth;
					if(viqe_madi->info->init.src_fmt == VIQE_MADI_YUV420_inter || viqe_madi->info->init.src_fmt == VIQE_MADI_YUV422_inter)
						viqe_madi->info->Coffset_bottom = viqe_madi->info->init.src_ImgWidth;
					else
						viqe_madi->info->Coffset_bottom = viqe_madi->info->init.src_ImgWidth/2;

					VIQE_MADI_Ctrl_Enable(MADI_READY);

					VIQE_MADI_Gen_Timming(viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
											viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop);

					//alank
					g_width = viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft;
					g_height = viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop;

					VIQE_MADI_SetBasicConfiguration(viqe_madi->info->init.odd_first);

					VIQE_MADI_Set_SrcImgSize(viqe_madi->info->init.src_ImgWidth, viqe_madi->info->init.src_ImgHeight,
												viqe_madi->info->init.src_fmt <= VIQE_MADI_YUV420_inter ? 1 : 0,
												viqe_madi->info->init.src_bit_depth,
												viqe_madi->info->init.src_winLeft, viqe_madi->info->init.src_winTop,
												viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
												viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop);
					VIQE_MADI_Set_TargetImgSize(viqe_madi->info->init.src_ImgWidth, viqe_madi->info->init.src_ImgHeight,
												viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
												viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop,
												viqe_madi->info->init.dest_bit_depth);

					VIQE_MADI_Ctrl_Enable(MADI_ON);

#ifdef USE_REG_EXTRACTOR // for testing!!
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_0, 0x00100000 << COMMON_ADDR_SHIFT, 0x00300000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_1, (0x00100000 << COMMON_ADDR_SHIFT)+viqe_madi->info->Yoffset_bottom, (0x00300000 << COMMON_ADDR_SHIFT)+viqe_madi->info->Coffset_bottom);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_2, 0x00140000 << COMMON_ADDR_SHIFT, 0x00340000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_3, (0x00140000 << COMMON_ADDR_SHIFT)+viqe_madi->info->Yoffset_bottom, (0x00340000 << COMMON_ADDR_SHIFT)+viqe_madi->info->Coffset_bottom);
					msleep(100);
					_reg_print_ext();
#endif
					dprintk("TCC_VIQE_MADI_INIT(0x%x) :: %dx%d, %d,%d ~ %dx%d, offset(%d/%d)\n",
							viqe_madi->irq_status, viqe_madi->info->init.src_ImgWidth, viqe_madi->info->init.src_ImgHeight,
							viqe_madi->info->init.src_winLeft, viqe_madi->info->init.src_winTop,
							viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
							viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop,
							viqe_madi->info->Yoffset_bottom, viqe_madi->info->Coffset_bottom);

				}				
			}
			break;

		case TCC_VIQE_MADI_DEINIT:
			{
	#ifndef USE_REG_EXTRACTOR // for testing!!
				dprintk("TCC_VIQE_MADI_DEINIT \n");
				VIQE_MADI_Ctrl_Disable();
				viqe_madi->info->curr_src_index = viqe_madi->info->curr_out_index = 0;
	#endif
			}
			break;

		case TCC_VIQE_MADI_PROC:
			{
	#ifndef USE_REG_EXTRACTOR // for testing!!
				stVIQE_MADI_PROC_TYPE proc_info;

				if(copy_from_user(&proc_info,(void *)arg, sizeof(stVIQE_MADI_PROC_TYPE))) {
					printk(KERN_ALERT "%s():  Error copy_from_user(%d)\n", __func__, cmd);
					ret = -EFAULT;
				}
				else
				{
					unsigned int i = 0;
					unsigned int cur_src_index = viqe_madi->info->curr_src_index;

					if(viqe_madi->info->init.src_one_field_only_frame)
					{
						dprintk("TCC_VIQE_MADI_PROC :: one field only \n");
						VIQE_MADI_Set_SrcImgBase(cur_src_index, proc_info.src_Yaddr, proc_info.src_Uaddr);
						viqe_madi->info->curr_src_index = (cur_src_index + 1)%viqe_madi->info->max_buffer_cnt;
					}
					else
					{
						if(!proc_info.second_field_proc)
						{
							dprintk("TCC_VIQE_MADI_PROC :: 1st field process (%d) \n", cur_src_index);
							if(cur_src_index == 0)
							{
								VIQE_MADI_Set_SrcImgBase(viqe_madi->info->init.odd_first ? MADI_ADDR_1 : MADI_ADDR_0, proc_info.src_Yaddr, proc_info.src_Uaddr);
								VIQE_MADI_Set_SrcImgBase(viqe_madi->info->init.odd_first ? MADI_ADDR_0 : MADI_ADDR_1, proc_info.src_Yaddr+viqe_madi->info->Yoffset_bottom, proc_info.src_Uaddr+viqe_madi->info->Coffset_bottom);
							}
							else
							{
								VIQE_MADI_Set_SrcImgBase(viqe_madi->info->init.odd_first ? MADI_ADDR_3 : MADI_ADDR_2, proc_info.src_Yaddr, proc_info.src_Uaddr);
								VIQE_MADI_Set_SrcImgBase(viqe_madi->info->init.odd_first ? MADI_ADDR_2 : MADI_ADDR_3, proc_info.src_Yaddr+viqe_madi->info->Yoffset_bottom, proc_info.src_Uaddr+viqe_madi->info->Coffset_bottom);
							}
							viqe_madi->info->curr_src_index = (cur_src_index + 1)%viqe_madi->info->max_buffer_cnt;
						}
						else
						{
							dprintk("TCC_VIQE_MADI_PROC :: 2nd field process \n");
						}
					}

					for(i=MADI_ADDR_0;i<MADI_ADDR_MAX;i++) {
						VIQE_MADI_Set_TargetImgBase(i, proc_info.dest_Aaddr, proc_info.dest_Yaddr, proc_info.dest_Caddr);
					}

					viqe_madi->data->block_operating = 1;
					VIQE_MADI_Go_Request();

					if (proc_info.responsetype == VIQE_MADI_POLLING) {
						ret = wait_event_interruptible_timeout(viqe_madi->data->poll_wq,  viqe_madi->data->block_operating == 0, msecs_to_jiffies(200));
						if (ret <= 0) {
							viqe_madi->data->block_operating = 0;
							printk("%s():  time out(%d), line(%d). \n", __func__, ret, __LINE__);
						}
					} else if (proc_info.responsetype  == VIQE_MADI_NOWAIT) {
						// TODO:
					}
					dprintk("TCC_VIQE_MADI_PROC(0x%x) \n", viqe_madi->irq_status);
				}
	#endif
			}
			break;

		case TCC_VIQE_MADI_GET_RESULT:
			{
	#ifndef USE_REG_EXTRACTOR // for testing!!
				unsigned int cfg_code, cur_alpha_index, cur_yc_index;
				stVIQE_MADI_RESULT_TYPE result;
				volatile void __iomem *reg = NULL;

				if(viqe_madi->info->first_frame || viqe_madi->info->skip_count > 0)
				{
					result.dest_Yaddr = result.dest_Uaddr = 0x00;
					viqe_madi->info->first_frame = 0;
					if(viqe_madi->info->skip_count > 0)
						viqe_madi->info->skip_count -= 1;
				}
				else
				{
					reg = VIQE_MADI_GetAddress(VMADI_TIMMING);
				    cfg_code = __madi_dreg_r(reg + MADITIMMING_GEN_CFG_CODE_OFFSET);
					cur_alpha_index = (cfg_code+3)&0x3;
					cur_yc_index = (cur_alpha_index+3)&0x3;

					VIQE_MADI_Get_TargetImgBase((MadiADDR_Type)cur_yc_index, &result.dest_Yaddr, &result.dest_Uaddr);
					//result.dest_Yaddr = viqe_madi->info->out_Yaddr[cur_yc_index];
					//result.dest_Uaddr = viqe_madi->info->out_Caddr[cur_yc_index];
				}

				VIQE_MADI_Change_Cfg();
				//FieldInsertionCtrl();	//alank

				dprintk("TCC_VIQE_MADI_GET_RESULT(0x%x) :: [%d] = 0x%x/0x%x \n", viqe_madi->irq_status, viqe_madi->info->first_frame, result.dest_Yaddr, result.dest_Uaddr);

				if (copy_to_user((unsigned int*)arg, &result, sizeof(stVIQE_MADI_RESULT_TYPE))) {
					ret = -EFAULT;
				}
	#endif
			}
			break;

	#ifdef USE_REG_EXTRACTOR // for testing!!
		case 0x10001:
			{
				// ioctl /dev/viqe_madi 0x10001 0 8 1920 1080 0 0 256 256 0 8
				// ioctl /dev/viqe_madi 0x10001 0 8 1920 1080 0 0 1920 1080 0 8
				if(copy_from_user(&viqe_madi->info->init,(void *)arg, sizeof(stVIQE_MADI_INIT_TYPE))) {
					printk(KERN_ALERT "%s():  Error copy_from_user(%d)\n", __func__, cmd);
					ret = -EFAULT;
				}
				else {
					VIQE_MADI_Gen_Timming(viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
											viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop);
					VIQE_MADI_Ctrl_Enable(MADI_READY);
					VIQE_MADI_SetBasicConfiguration(0);

					VIQE_MADI_Set_SrcImgSize(viqe_madi->info->init.src_ImgWidth, viqe_madi->info->init.src_ImgHeight,
												viqe_madi->info->init.src_fmt <= VIQE_MADI_YUV420_inter ? 1 : 0,
												viqe_madi->info->init.src_bit_depth,
												viqe_madi->info->init.src_winLeft, viqe_madi->info->init.src_winTop,
												viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
												viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop);
					VIQE_MADI_Set_TargetImgSize(viqe_madi->info->init.src_ImgWidth, viqe_madi->info->init.src_ImgHeight,
												viqe_madi->info->init.src_winRight - viqe_madi->info->init.src_winLeft,
												viqe_madi->info->init.src_winBottom - viqe_madi->info->init.src_winTop,
												viqe_madi->info->init.dest_bit_depth);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_0, 0x00100000 << COMMON_ADDR_SHIFT, 0x00300000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_1, 0x00140000 << COMMON_ADDR_SHIFT, 0x00340000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_2, 0x00180000 << COMMON_ADDR_SHIFT, 0x00380000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_SrcImgBase(MADI_ADDR_3, 0x001C0000 << COMMON_ADDR_SHIFT, 0x003C0000 << COMMON_ADDR_SHIFT);			
					VIQE_MADI_Set_TargetImgBase(MADI_ADDR_0, 0x00500000 << COMMON_ADDR_SHIFT, 0x00800000 << COMMON_ADDR_SHIFT, 0x00c00000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_TargetImgBase(MADI_ADDR_1, 0x00500800 << COMMON_ADDR_SHIFT, 0x00840000 << COMMON_ADDR_SHIFT, 0x00c40000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_TargetImgBase(MADI_ADDR_2, 0x00501000 << COMMON_ADDR_SHIFT, 0x00880000 << COMMON_ADDR_SHIFT, 0x00c80000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Set_TargetImgBase(MADI_ADDR_3, 0x00501800 << COMMON_ADDR_SHIFT, 0x008c0000 << COMMON_ADDR_SHIFT, 0x00cc0000 << COMMON_ADDR_SHIFT);
					VIQE_MADI_Ctrl_Enable(MADI_ON);

					VIQE_MADI_Change_Cfg();

					msleep(100);
					_reg_print_ext();
				}
			}
			break;
	#endif

		default:
			printk(KERN_ALERT "%s():  Not Supported viqe_madi_IOCTL(%d). \n", __func__, cmd);
			break;			
	}

	mutex_unlock(&viqe_madi->data->io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long viqe_madi_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return viqe_madi_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int viqe_madi_release(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct viqe_madi_type	*viqe_madi = dev_get_drvdata(misc->parent);

	dprintk("%s():  In -release(%d), block(%d), wait(%d), cmd(%d), irq(%d) \n", __func__, viqe_madi->data->dev_opened, viqe_madi->data->block_operating, \
			viqe_madi->data->block_waiting, viqe_madi->data->cmd_count, viqe_madi->data->irq_reged);

	if (viqe_madi->data->dev_opened > 0) {
		viqe_madi->data->dev_opened--;
	}

	if (viqe_madi->data->dev_opened == 0) {
#ifndef USE_REG_EXTRACTOR
		if (viqe_madi->data->block_operating) {
			int ret = 0;
			ret = wait_event_interruptible_timeout(viqe_madi->data->cmd_wq, viqe_madi->data->block_operating == 0, msecs_to_jiffies(200));
			if (ret <= 0) {
	 			printk("%s(%d):  timed_out block_operation:%d, cmd_count:%d. \n", __func__, ret, viqe_madi->data->block_waiting, viqe_madi->data->cmd_count);
			}
		}
#endif

		if (viqe_madi->data->irq_reged) {
			free_irq(viqe_madi->irq, viqe_madi);
			viqe_madi->data->irq_reged = 0;
		}

		viqe_madi->data->block_operating = viqe_madi->data->block_waiting = 0;
		viqe_madi->data->poll_count = viqe_madi->data->cmd_count = 0;
	}

	if (viqe_madi->peri_madi_clk)
		clk_disable_unprepare(viqe_madi->peri_madi_clk);
	if (viqe_madi->ddi_madi_clk)
		clk_disable_unprepare(viqe_madi->ddi_madi_clk);

	dprintk("%s():  Out - release(%d). \n", __func__, viqe_madi->data->dev_opened);

	return 0;
}

static int viqe_madi_open(struct inode *inode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct viqe_madi_type	*viqe_madi = dev_get_drvdata(misc->parent);
	
	int ret = 0;
	printk("%s():  In -open(%d), block(%d), wait(%d), cmd(%d), irq(%d) \n", __func__, viqe_madi->data->dev_opened, viqe_madi->data->block_operating, \
			viqe_madi->data->block_waiting, viqe_madi->data->cmd_count, viqe_madi->data->irq_reged);

	if (viqe_madi->ddi_madi_clk)
		clk_prepare_enable(viqe_madi->ddi_madi_clk);
	if (viqe_madi->peri_madi_clk)
		clk_prepare_enable(viqe_madi->peri_madi_clk);

#ifdef TEST_REG_RW
	VIQE_MADI_Reg_RW_Test();
	if (viqe_madi->peri_madi_clk)
		clk_disable_unprepare(viqe_madi->peri_madi_clk);
//	if (viqe_madi->ddi_madi_clk)
//		clk_disable_unprepare(viqe_madi->ddi_madi_clk);
//	if (viqe_madi->ddi_madi_clk)
//		clk_prepare_enable(viqe_madi->ddi_madi_clk);
	if (viqe_madi->peri_madi_clk)
		clk_prepare_enable(viqe_madi->peri_madi_clk);
#endif

	if (!viqe_madi->data->irq_reged) {
		ret = request_irq(viqe_madi->irq, viqe_madi_handler, IRQF_SHARED, viqe_madi->misc->name, viqe_madi);
		viqe_madi->data->irq_reged = 1;
	}

	viqe_madi->data->dev_opened++;

	printk("%s():  Out - open(%d). \n", __func__, viqe_madi->data->dev_opened);
	return ret;
}

static struct file_operations viqe_madi_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl	= viqe_madi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= viqe_madi_compat_ioctl,
#endif
	.mmap			= viqe_madi_mmap,
	.open			= viqe_madi_open,
	.release		= viqe_madi_release,
	.poll			= viqe_madi_poll,
};

static int viqe_madi_probe(struct platform_device *pdev)
{
	struct viqe_madi_type *viqe_madi;
	int ret = -ENODEV;

	dprintk("%s \n", __func__);
	viqe_madi = kzalloc(sizeof(struct viqe_madi_type), GFP_KERNEL);
	if (!viqe_madi)
		return -ENOMEM;

	viqe_madi->ddi_madi_clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(viqe_madi->ddi_madi_clk)){
		printk("%s-%d Error:: get clock 0 \n", __func__, __LINE__);
		viqe_madi->ddi_madi_clk = NULL;
		ret = -EIO;
		goto err_misc_alloc;
	}

	viqe_madi->peri_madi_clk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(viqe_madi->peri_madi_clk)){
		printk("%s-%d Error:: get clock 1 \n", __func__, __LINE__);
		viqe_madi->peri_madi_clk = NULL;
		ret = -EIO;
		goto err_misc_alloc;
	}

	ret = clk_set_rate(viqe_madi->peri_madi_clk, 400000000);
	if (ret) {
		printk("%s-%d Clock rate change failed %d\n", __func__, __LINE__, ret);
		ret = -EIO;
		goto err_misc_alloc;
	}

	viqe_madi->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (viqe_madi->misc == 0)
		goto err_misc_alloc;

	viqe_madi->info = kzalloc(sizeof(struct viqe_madi_info_type), GFP_KERNEL);
	if (viqe_madi->info == 0)
		goto err_info_alloc;

	viqe_madi->data = kzalloc(sizeof(struct viqe_madi_data), GFP_KERNEL);
	if (viqe_madi->data == 0)
		goto err_data_alloc;

	viqe_madi->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (viqe_madi->vioc_intr == 0)
		goto err_vioc_intr_alloc;

 	/* register viqe_madi discdevice */
	viqe_madi->misc->minor = MISC_DYNAMIC_MINOR;
	viqe_madi->misc->fops = &viqe_madi_fops;
	viqe_madi->misc->name = "viqe_madi";
	viqe_madi->misc->parent = &pdev->dev;
	ret = misc_register(viqe_madi->misc);
	if (ret)
		goto err_misc_register;

	viqe_madi->id = of_alias_get_id(pdev->dev.of_node, "viqe_madi");
	viqe_madi->irq = platform_get_irq(pdev, 0);

	dprintk(" viqe_madi driver name : %s \n", viqe_madi->misc->name);

	spin_lock_init(&(viqe_madi->data->poll_lock));
	spin_lock_init(&(viqe_madi->data->cmd_lock));

	mutex_init(&(viqe_madi->data->io_mutex));
	
	init_waitqueue_head(&(viqe_madi->data->poll_wq));
	init_waitqueue_head(&(viqe_madi->data->cmd_wq));

	platform_set_drvdata(pdev, viqe_madi);

	pr_info("%s: viqe_madi Driver Initialized\n", pdev->name);

	return 0;

	misc_deregister(viqe_madi->misc);

err_misc_register:
	kfree(viqe_madi->vioc_intr);

err_vioc_intr_alloc:
	kfree(viqe_madi->data);

err_data_alloc:
	kfree(viqe_madi->info);

err_info_alloc:
	kfree(viqe_madi->misc);

err_misc_alloc:
	kfree(viqe_madi);

	printk("%s: %s: err ret:%d \n", __func__, pdev->name, ret);
	return ret;
}

static int viqe_madi_remove(struct platform_device *pdev)
{
	struct viqe_madi_type *viqe_madi = (struct viqe_madi_type *)platform_get_drvdata(pdev);

	misc_deregister(viqe_madi->misc);
	kfree(viqe_madi->vioc_intr);
	kfree(viqe_madi->data);
	kfree(viqe_madi->info);
	kfree(viqe_madi->misc);
	kfree(viqe_madi);
	return 0;
}

static int viqe_madi_suspend(struct platform_device *pdev, pm_message_t state)
{
	// TODO:
	return 0;
}

static int viqe_madi_resume(struct platform_device *pdev)
{
	// TODO:
	return 0;
}

static struct of_device_id viqe_madi_of_match[] = {
	{ .compatible = "telechips,viqe_madi" },
	{}
};
MODULE_DEVICE_TABLE(of, viqe_madi_of_match);

static struct platform_driver viqe_madi_driver = {
	.probe		= viqe_madi_probe,
	.remove		= viqe_madi_remove,
	.suspend	= viqe_madi_suspend,
	.resume		= viqe_madi_resume,
	.driver 	= {
		.name	= "viqe_madi_pdev",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(viqe_madi_of_match),
#endif
	},
};

static int __init viqe_madi_init(void)
{
	dprintk("%s \n", __func__);
	return platform_driver_register(&viqe_madi_driver);
}

static void __exit viqe_madi_exit(void)
{
	platform_driver_unregister(&viqe_madi_driver);
}

module_init(viqe_madi_init);
module_exit(viqe_madi_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips VIQE_MADI Driver");
MODULE_LICENSE("GPL");

