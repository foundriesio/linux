/*
 * drivers/char/tcc_mem.c
 *
 * TCC MEM driver
 *
 * Copyright (C) 2009 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/clk.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
//#include <asm/pgtable.h>
#endif
//#include <asm/mach-types.h>

#include <soc/tcc/pmap.h>
#if defined(CONFIG_ARCH_TCC570X) || defined(CONFIG_ARCH_TCC802X)
#include <mach/bsp.h>
#include <mach/tcc_mem_ioctl.h>
#include <mach/tccfb_ioctrl.h>
#include <mach/tcc_vsync_ioctl.h>
#else
#include <video/tcc/tcc_mem_ioctl.h>
#include <video/tcc/tccfb_ioctrl.h>
#include <video/tcc/tcc_vsync_ioctl.h>
#include <video/tcc/tcc_vpu_wbuffer.h>
#endif

#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
#include <video/tcc/vioc_v_dv.h>
#include <video/tcc/vioc_config.h>
#endif

#define DEV_MAJOR   213
#define DEV_MINOR   1
#define DEV_NAME    "tmem"

#define dprintk(msg...) if (0) { printk( "tMEM: " msg); }

#if defined(CONFIG_ARCH_TCC893X)
#define VPU_BASE        0x75000000
#define JPEG_BASE       0x75300000
#define CLK_BASE        0x74000000
#define DXB_BASE1       0x30000000 // have to modify
#define DXB_BASE2       0xF08C0000 // have to modify
#define AV_BASE         0x76067000
#define AV_BASE2        0x74200000
#define AD_BASE         0x76066000

#elif defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
#define VPU_BASE        0x75000000
#define JPEG_BASE       0x75180000
#define CLK_BASE        0x74000000
#define DXB_BASE1       0x30000000 // have to modify
#define DXB_BASE2       0xF08C0000 // have to modify
#define AV_BASE         0x76067000
#define AV_BASE2        0x74400000
#define AD_BASE         0x76066000

#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X)
#define VPU_BASE        0x15000000
#define JPEG_BASE       0x15180000
#define CLK_BASE        0x14000000
#define DXB_BASE1       0x30000000 // have to modify
#define DXB_BASE2       0xF08C0000 // have to modify
#define AV_BASE         0x16067000
#define AV_BASE2        0x14400000
#define AD_BASE         0x16051000

#else
#define VPU_BASE        0xF0700000
#define CLK_BASE        0xF0400000
#define DXB_BASE1       0xE0000000
#define DXB_BASE2       0x00000000
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X)
#define AV_BASE         0xF4000000
#define AV_BASE2        0xE0003000
#define AD_BASE         0x16051000
#else
#define AV_BASE         0xE0000000
#define AV_BASE2        0xF0102000
#define AD_BASE         0x00000000
#endif
#endif

extern unsigned int HDMI_video_width;
extern unsigned int HDMI_video_height;

#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
static pmap_t pmap_total;
#endif

struct allow_region {
    unsigned long   start;
    unsigned long   len;
};

struct allow_region AllowRegion[] = {
    { VPU_BASE, 0x21000},   //VPU Register..
    { CLK_BASE, 0x1000},    //to check vpu-running.
    { DXB_BASE1,0x1000},    //for broadcasting.
    { DXB_BASE2,0x1000},    //for broadcasting.
#if defined(CONFIG_ARCH_TCC893X) || defined(CONFIG_ARCH_TCC896X)
    { JPEG_BASE,0x2000},    //JPEG(in VPU) Register..
#endif

    { AV_BASE,  0x1000},    //for AV_algorithm.
    { AV_BASE2, 0x1000},    //for AV_algorithm.
    { AD_BASE, 0x2000}      //for Apple device.
};

#define SIZE_TABLE_MAX (sizeof(AllowRegion)/sizeof(struct allow_region))

//extern  void drop_pagecache(void);
int tccxxx_sync_player(int sync)
{
    if(sync)
    {
        sys_sync();
//      drop_pagecache();
    }

    return 0;
}
EXPORT_SYMBOL(tccxxx_sync_player);


static int curr_displaying_idx[VPU_INST_MAX] = {-1, };
static int curr_displayed_buffer_id[VPU_INST_MAX] = {-1,};
struct mutex io_mutex;

int set_displaying_index(unsigned long arg)
{
    int ret = 0;

    mutex_lock(&io_mutex);
    {
        vbuffer_manager vBuffSt;
        ret = 0;

        if(copy_from_user(&vBuffSt,(int*)arg, sizeof(vbuffer_manager)))
            ret = -EFAULT;
        else{
            if( vBuffSt.istance_index < 0 || vBuffSt.istance_index >= VPU_INST_MAX ){
                ret = -1;
            }
            else{
                //printk("set_displaying_index :: [%d]=%d\n", vBuffSt.istance_index, vBuffSt.index);
                curr_displaying_idx[vBuffSt.istance_index] = vBuffSt.index;
            }
        }
    }
    mutex_unlock(&io_mutex);

    return ret;
}
EXPORT_SYMBOL(set_displaying_index);

int get_displaying_index(int nInst)
{
    int ret = 0;

    mutex_lock(&io_mutex);
    {
        if( nInst < 0 || nInst >= VPU_INST_MAX ){
            ret = -1;
        }
        else{
            ret = curr_displaying_idx[nInst];
        }
    }
    mutex_unlock(&io_mutex);

    return ret;
}
EXPORT_SYMBOL(get_displaying_index);

int set_buff_id(unsigned long arg)
{
    int ret = 0;

    mutex_lock(&io_mutex);
    {
        vbuffer_manager vBuffSt;
        ret = 0;

        if(copy_from_user(&vBuffSt,(int*)arg, sizeof(vbuffer_manager)))
            ret = -EFAULT;
        else{
            if( vBuffSt.istance_index < 0 || vBuffSt.istance_index >= VPU_INST_MAX ){
                ret = -1;
            }
            else{
                //printk("set_buff_id :: [%d]=%d\n", vBuffSt.istance_index, vBuffSt.index);
                curr_displayed_buffer_id[vBuffSt.istance_index] = vBuffSt.index;
            }
        }
    }
    mutex_unlock(&io_mutex);

    return ret;
}
EXPORT_SYMBOL(set_buff_id);

int get_buff_id(int nInst)
{
    int ret = 0;

    mutex_lock(&io_mutex);
    {
        if( nInst < 0 || nInst >= VPU_INST_MAX ){
            ret = -1;
        }
        else{
            ret = curr_displayed_buffer_id[nInst];
        }
    }
    mutex_unlock(&io_mutex);

    return ret;
}
EXPORT_SYMBOL(get_buff_id);

#ifdef USE_UMP_RESERVED_SW_PMAP
typedef struct {
    unsigned int phy_addr;
    unsigned int ref_count;
}stUmp_sw_buffer;

static unsigned int bInited_ump_reserved_sw = 0;
static pmap_t pmap_ump_reserved_sw;
static void* remap_ump_reserved_sw = NULL;
static stUmp_sw_buffer ump_sw_buf[UMP_SW_BLOCK_MAX_CNT]; //physical address

#define ump_printk_info(msg...) if (0) { printk( "UMP_RESERVED_SW: " msg); }
#define ump_printk_check(msg...) if (0) { printk( "UMP_RESERVED_SW: " msg); }
#define ump_printk_err(msg...) if (1) { printk( "UMP_RESERVED_SW: " msg); }

static void ump_sw_mgmt_init(void)
{
	pmap_get_info("ump_reserved_sw", &pmap_ump_reserved_sw);

	if( (pmap_ump_reserved_sw.size == 0) ||
		(pmap_ump_reserved_sw.size < (UMP_SW_BLOCK_SIZE*UMP_SW_BLOCK_MAX_CNT))
	)
	{
		ump_printk_err("%s :: The size(0x%x vs. 0x%x) of ump_reserved_sw is wierd. please check !! \n", __func__, pmap_ump_reserved_sw.size, (UMP_SW_BLOCK_SIZE*UMP_SW_BLOCK_MAX_CNT));
	}
	else
	{
		int i = 0;

		for(i = 0; i < UMP_SW_BLOCK_MAX_CNT; i++)
		{
			ump_sw_buf[i].phy_addr = 0x00;
            ump_sw_buf[i].ref_count = 0x00;
		}

    	remap_ump_reserved_sw = (void *)ioremap_nocache(pmap_ump_reserved_sw.base, PAGE_ALIGN(pmap_ump_reserved_sw.size/*-PAGE_SIZE*/));
    	if(remap_ump_reserved_sw == NULL) {
			ump_printk_err("phy[0x%x - 0x%x] mmap failed.\n", pmap_ump_reserved_sw.base, pmap_ump_reserved_sw.size );
		}
		else {
			memset_io(remap_ump_reserved_sw, 0x00, pmap_ump_reserved_sw.size);
		}
		bInited_ump_reserved_sw = 1;
	}

	ump_printk_info("%s \n", __func__);
}

static void ump_sw_mgmt_deinit(void)
{
	if(remap_ump_reserved_sw)
		iounmap((void*)remap_ump_reserved_sw);
    remap_ump_reserved_sw = NULL;

	ump_printk_info("%s \n", __func__);
}

static void ump_sw_mgmt_check_status(void)
{
	int i = 0;

	for(i = 0; i < UMP_SW_BLOCK_MAX_CNT; i++)
	{
		if( ump_sw_buf[i].ref_count != 0 ){
			ump_printk_check("%s :: [%d] = 0x%x/%d \n", __func__, i, ump_sw_buf[i].phy_addr, ump_sw_buf[i].ref_count);
		}
	}
}

static void ump_sw_mgmt_write_recognition_addr(unsigned int index, unsigned int addr)
{
	TCC_PLATFORM_PRIVATE_PMEM_INFO *plat_priv = (TCC_PLATFORM_PRIVATE_PMEM_INFO *)((unsigned char*)remap_ump_reserved_sw + (UMP_SW_BLOCK_SIZE * index));

	if(addr)
	{
    	ump_printk_info("%s :: %p = %p + 0x%x \n", __func__, plat_priv, remap_ump_reserved_sw, (UMP_SW_BLOCK_SIZE * index));
		memset_io(plat_priv, 0x00, sizeof(TCC_PLATFORM_PRIVATE_PMEM_INFO));	// zero init	
	}

	plat_priv->gralloc_phy_address = addr;
}

static void ump_sw_mgmt_register(unsigned int phy_addr)
{
	int i = 0;
    int bFound = 0;
	ump_printk_info("%s with 0x%x \n", __func__, phy_addr);

	if(!bInited_ump_reserved_sw)
		return;

	for(i = 0; i < UMP_SW_BLOCK_MAX_CNT; i++)
	{
		if(ump_sw_buf[i].phy_addr == phy_addr){
            bFound = 1;
			break;
		}
	}

    if(!bFound){
    	for(i = 0; i < UMP_SW_BLOCK_MAX_CNT; i++)
    	{
    		if(ump_sw_buf[i].phy_addr == 0x00)
    			break;
    	}
    }

	if(i >= UMP_SW_BLOCK_MAX_CNT)
	{
		ump_printk_err("%s :: There is no empty room for 0x%x ! \n", __func__, phy_addr);
		return;
	}

	ump_printk_info("%s :: [%d] = 0x%x/%d\n", __func__, i, phy_addr, ump_sw_buf[i].ref_count);

    if(ump_sw_buf[i].ref_count == 0){
    	ump_sw_mgmt_write_recognition_addr(i, phy_addr);
		ump_printk_check("%s :: [%d] = 0x%x len: 0x%x\n", __func__, i, phy_addr, sizeof(TCC_PLATFORM_PRIVATE_PMEM_INFO));
    }
	ump_sw_buf[i].phy_addr = phy_addr;
    ump_sw_buf[i].ref_count += 1;

	ump_printk_info("%s :: [%d] = 0x%x/%d \n", __func__, i, phy_addr, ump_sw_buf[i].ref_count);

}


static void ump_sw_mgmt_deregister(unsigned int phy_addr)
{
	int i = 0;
	ump_printk_info("%s with 0x%x \n", __func__, phy_addr);

	if(!bInited_ump_reserved_sw)
		return;

    if(!phy_addr)
        return;

	for(i = 0; i < UMP_SW_BLOCK_MAX_CNT; i++)
	{
		if(ump_sw_buf[i].phy_addr == phy_addr)
			break;
	}

	if(i >= UMP_SW_BLOCK_MAX_CNT)
	{
		ump_printk_err("%s :: There is no same room for 0x%x ! \n", __func__, phy_addr);
		return;
	}

    ump_sw_buf[i].ref_count -= 1;
    if(ump_sw_buf[i].ref_count == 0){
	    ump_sw_buf[i].phy_addr = 0x00;
	    ump_sw_mgmt_write_recognition_addr(i, phy_addr);
		ump_printk_check("%s :: [%d] = 0x%x \n", __func__, i, phy_addr);
    }

	ump_printk_info("%s :: [%d] = 0x%x/%d \n", __func__, i, phy_addr, ump_sw_buf[i].ref_count);

}
#endif

long tmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

    switch (cmd)
    {
        case TCC_LIMIT_PRESENTATION_WINDOW:
        {
        #if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_MID) && defined(CONFIG_DRAM_16BIT_USED)
            int res_region;

            if(copy_from_user(&res_region,(int*)arg, sizeof(res_region)))
                ret = -EFAULT;

            else if( res_region > PRESENTATION_LIMIT_RESOLUTION)
                ret = 1;
            else
                ret = 0;
        #else
            ret = 0;
        #endif
        }
        break;

        case TCC_DRAM_16BIT_USED:
        {
        #if defined(CONFIG_DRAM_16BIT_USED)
            ret = 1;
        #else
            ret = 0;
        #endif
        }
        break;

        case TCC_8925S_IS_XX_CHIP:
        {
            ret = 0;
        }
        break;

        case TCC_VIDEO_SET_DISPLAYING_IDX:
        {
            ret = set_displaying_index(arg);
        }
        break;

        case TCC_VIDEO_GET_DISPLAYING_IDX:
        {
            int nInstIdx = 0, nId = 0;
            if(copy_from_user(&nInstIdx,(int*)arg, sizeof(int))){
                ret = -EFAULT;
            }
            else{
                nId = get_displaying_index(nInstIdx);
                //printk("TCC_VIDEO_GET_DISPLAYING_IDX :: [%d]=%d\n", nInstIdx, nId);
                if (copy_to_user((int *)arg, &nId, sizeof(int))){
                    ret = -EFAULT;
                }
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
            if(copy_from_user(&nInstIdx,(int*)arg, sizeof(int))){
                ret = -EFAULT;
            }
            else{
                nId = get_buff_id(nInstIdx);
                //printk("TCC_VIDEO_GET_DISPLAYED_BUFFER_ID :: [%d]=%d\n", nInstIdx, nId);
                if (copy_to_user((int *)arg, &nId, sizeof(int))){
                    ret = -EFAULT;
                }
            }
        }
        break;

        case TCC_GET_HDMI_INFO:
        {
            vHdmi_info mInfo_Hdmi;

            memset(&mInfo_Hdmi, 0x00, sizeof(vHdmi_info));

            if(copy_from_user(&mInfo_Hdmi, (const void*)arg, sizeof(vHdmi_info))){
                ret = -EFAULT;
            }
            else{
            #if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
                if (VIOC_CONFIG_DV_GET_EDR_PATH()) {
                    mInfo_Hdmi.dv_path = 1;
                    mInfo_Hdmi.out_type = vioc_get_out_type();
                    mInfo_Hdmi.width = Hactive;
                    mInfo_Hdmi.height = Vactive;
                    mInfo_Hdmi.dv_vsvdb_size = vioc_v_dv_get_vsvdb((unsigned char*)mInfo_Hdmi.dv_vsvdb);
                    //printk("@@@@@@@@@@@@@@@@@@@@@@ hdmi info DV_path(%d)/out(%d), %d x %d \n", mInfo_Hdmi.dv_path, mInfo_Hdmi.out_type, mInfo_Hdmi.width, mInfo_Hdmi.height);
                }
                else
            #endif
                {
                    mInfo_Hdmi.dv_path = 0;
                    mInfo_Hdmi.out_type = 2;
                #ifdef CONFIG_FB_VIOC
                    mInfo_Hdmi.width = HDMI_video_width;
                    mInfo_Hdmi.height = HDMI_video_height;
                #else
                    mInfo_Hdmi.width = 1920;
                    mInfo_Hdmi.height = 1080;
                    printk("Error :: Can't get the resolution information related to HDMI \n");
                #endif
                }

                if (copy_to_user((void *)arg, &mInfo_Hdmi, sizeof(vHdmi_info))) {
                    ret = -EFAULT;
                }
            }

        }
        break;

#ifdef USE_UMP_RESERVED_SW_PMAP
		case TCC_REGISTER_UMP_SW_INFO:
		{
			unsigned int phy_addr = 0x00;

			if(copy_from_user(&phy_addr, (const void*)arg, sizeof(unsigned int))){
                ret = -EFAULT;
            }
            else{
				ump_sw_mgmt_register(phy_addr);
            }
		}
		break;

		case TCC_DEREGISTER_UMP_SW_INFO:
		{
			unsigned int phy_addr = 0x00;

			if(copy_from_user(&phy_addr, (const void*)arg, sizeof(unsigned int))){
                ret = -EFAULT;
            }
            else{
				ump_sw_mgmt_deregister(phy_addr);
            }
		}
		break;
#endif

        default:
            printk("Unsupported cmd(0x%x) for tmem_ioctl. \n", cmd);
            ret = -EFAULT;
            break;
    }

    return ret;
}

#ifdef CONFIG_COMPAT
static long tmem_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return tmem_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int sync_once = 1;
static int tmem_open(struct inode *inode, struct file *filp)
{
    dprintk("tmem_open \n");
    if (sync_once) {
        tccxxx_sync_player(0);
        sync_once = 0;
    }

    return 0;
}

static int tmem_release(struct inode *inode, struct file *filp)
{
    dprintk("tmem_release  \n");

#ifdef USE_UMP_RESERVED_SW_PMAP
	ump_sw_mgmt_check_status();
#endif

    return 0;
}

static void mmap_mem_open(struct vm_area_struct *vma)
{
    /* do nothing */
}

static void mmap_mem_close(struct vm_area_struct *vma)
{
    /* do nothing */
}

static struct vm_operations_struct tmem_mmap_ops = {
    .open = mmap_mem_open,
    .close = mmap_mem_close,
};

static inline int uncached_access(struct file *file, unsigned long addr)
{
    if (file->f_flags & O_SYNC)
        return 1;
    return addr >= __pa(high_memory);
}

#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39))
static pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
                     unsigned long size, pgprot_t vma_prot)
{
    unsigned long offset = pfn << PAGE_SHIFT;

    if (uncached_access(file, offset))
        return pgprot_noncached(vma_prot);
    return vma_prot;
}
#endif

int range_is_allowed(unsigned long pfn, unsigned long size)
{
    int i;
    unsigned long request_start, request_end;

    request_start = pfn << PAGE_SHIFT;
    request_end = request_start + size;
    dprintk("Req: 0x%lx - 0x%lx, 0x%lx \n", request_start, request_end, size);

    /* Check reserved physical memory. */
#if defined(CONFIG_ARCH_TCC897X)
    if ((pmap_total.base <= request_start) && ((pmap_total.base+pmap_total.size) >= request_end)) {
        dprintk("Allowed Reserved Physical mem : 0x%x <= 0x%lx && 0x%x >= 0x%lx \n", pmap_total.base, request_start, (pmap_total.base+pmap_total.size), request_end);
        return 1;
    }
#else
    if (pmap_check_region(request_start, size)) {
        dprintk("Allowed Reserved Physical mem: 0x%lx -- 0x%lx\n", request_start, request_end);
        return 1;
    }
#endif

    for (i=0; i<SIZE_TABLE_MAX; i++)
    {
        if ((AllowRegion[i].start <= request_start) && ((AllowRegion[i].start+AllowRegion[i].len) >= request_end))
        {
            dprintk("Allowed : 0x%lx <= 0x%lx && 0x%lx >= 0x%lx \n", AllowRegion[i].start, request_start, (AllowRegion[i].start+AllowRegion[i].len), request_end);
            return 1;
        }
    }

    printk("Can't allow to mmap : size %ld, 0x%lx ~ 0x%lx \n", size, request_start, request_end);

    return -1;
}
EXPORT_SYMBOL(range_is_allowed);

static int tmem_mmap(struct file *file, struct vm_area_struct *vma)
{
    size_t size = vma->vm_end - vma->vm_start;

    if (range_is_allowed(vma->vm_pgoff, size) < 0) {
        printk(KERN_ERR  "tmem: this address is not allowed \n");
        return -EPERM;
    }

    vma->vm_page_prot = phys_mem_access_prot(file, vma->vm_pgoff, size, vma->vm_page_prot);
    vma->vm_ops = &tmem_mmap_ops;

    if (remap_pfn_range(vma,
                vma->vm_start,
                vma->vm_pgoff,
                size,
                vma->vm_page_prot)) {
        printk(KERN_ERR  "tmem: remap_pfn_range failed \n");
        return -EAGAIN;
    }
    return 0;
}

static struct file_operations tmem_fops = {
    .unlocked_ioctl = tmem_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = tmem_compat_ioctl, // for 32bit OMX
#endif
    .open           = tmem_open,
    .release        = tmem_release,
    .mmap           = tmem_mmap,
};

static void __exit tmem_cleanup(void)
{
    unregister_chrdev(DEV_MAJOR,DEV_NAME);
}

static struct class *tmem_class;

static int __init tmem_init(void)
{
#if defined(CONFIG_ARCH_TCC897X) || defined(CONFIG_ARCH_TCC570X)
    pmap_get_info("total", &pmap_total);
#endif

    if (register_chrdev(DEV_MAJOR, DEV_NAME, &tmem_fops))
        printk(KERN_ERR "unable to get major %d for tMEM device\n", DEV_MAJOR);
    tmem_class = class_create(THIS_MODULE, DEV_NAME);
    device_create(tmem_class, NULL, MKDEV(DEV_MAJOR, DEV_MINOR), NULL, DEV_NAME);

    mutex_init(&io_mutex);

#ifdef USE_UMP_RESERVED_SW_PMAP
	ump_sw_mgmt_init();
#endif

    return 0;
}

module_init(tmem_init)
module_exit(tmem_cleanup)
