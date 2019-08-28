/*
 *   FileName : vpu_buffer.c
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

#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#include "vpu_comm.h"
#include "vpu_buffer.h"
#include "vpu_mgr.h"
#ifdef CONFIG_SUPPORT_TCC_JPU
#include "jpu_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
#include "vpu_4k_d2_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include "hevc_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include "vp9_mgr.h"
#endif
#include <soc/tcc/pmap.h>

#include <video/tcc/tcc_vpu_wbuffer.h>

#define dprintk(msg...)	//printk( "TCC_VPU_MEM : " msg);
#define detailk(msg...) //printk( "TCC_VPU_MEM : " msg);
#define err(msg...) printk("TCC_VPU_MEM [Err]: "msg);

#define dprintk_mem(msg...)	//printk("TCC_VPU_MEM : " msg);
#define detailk_mem_dec(msg...)	//printk( "TCC_VPU_MEM_dec : " msg);
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
#define dtailk_mem_dec_ext23(msg...)	//printk( "TCC_VPU_MEM_dec_ext23 : " msg);
#endif
#if defined(CONFIG_VDEC_CNT_5)
#define dtailk_mem_dec_ext4(msg...)	//printk( "TCC_VPU_MEM_dec_ext4 : " msg);
#endif
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
#define dtailk_mem_enc_ext234(msg...)	//printk( "TCC_VPU_MEM_enc_ext234 : " msg);
#endif

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
// case(1-2-3 : on/on - off/on - on/off)
//#define FIXED_STREAM_BUFFER
#define FIXED_PS_SLICE_BUFFER

static MEM_ALLOC_INFO_t gsPs_memInfo[VPU_INST_MAX];
static MEM_ALLOC_INFO_t gsSlice_memInfo[VPU_INST_MAX];
static MEM_ALLOC_INFO_t gsStream_memInfo[VPU_INST_MAX];
#endif

//////////////////////////////////////////////////////////////////////////////
// Memory Management!!
static MEM_ALLOC_INFO_t gsVpuWork_memInfo;
extern int vmgr_opened(void);

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
static MEM_ALLOC_INFO_t gsVpu4KD2Work_memInfo;
extern int vmgr_4k_d2_opened(void);
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC // HEVC
static MEM_ALLOC_INFO_t gsWave410_Work_memInfo;
extern int hmgr_opened(void);
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
static MEM_ALLOC_INFO_t gsG2V2_Vp9Work_memInfo;
extern int vp9mgr_opened(void);
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
static MEM_ALLOC_INFO_t gsJpuWork_memInfo;
extern int jmgr_opened(void);
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
static MEM_ALLOC_INFO_t gsVpuEncSeqheader_memInfo[VPU_ENC_MAX_CNT];
#endif
static MEM_ALLOC_INFO_t gsVpuUserData_memInfo[VPU_INST_MAX];

// Regard only the operation of 2 component!!
static pmap_t pmap_video, pmap_video_sw;
static phys_addr_t ptr_sw_addr_mem;
static unsigned int sz_sw_mem;
static phys_addr_t ptr_front_addr_mem, ptr_rear_addr_mem, ptr_ext_addr_mem;
static unsigned int sz_front_used_mem, sz_rear_used_mem, sz_ext_used_mem;
static unsigned int sz_remained_mem, sz_enc_mem;

#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
static pmap_t pmap_video_ext;
static phys_addr_t ptr_ext_front_addr_mem, ptr_ext_rear_addr_mem;
static unsigned int sz_ext_front_used_mem, sz_ext_rear_used_mem;
static unsigned int sz_ext_remained_mem;
#endif

#if defined(CONFIG_VDEC_CNT_5)
static pmap_t pmap_video_ext2;
static phys_addr_t ptr_ext2_front_addr_mem;
static unsigned int sz_ext2_front_used_mem;
static unsigned int sz_ext2_remained_mem;
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
static pmap_t pmap_enc;
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
static pmap_t pmap_enc_ext[3];
static phys_addr_t ptr_enc_ext_addr_mem[3];
static unsigned int sz_enc_ext_used_mem[3];
static unsigned int sz_enc_ext_remained_mem[3];
#endif

static int vmem_allocated_count[VPU_MAX] = {0,};
static MEM_ALLOC_INFO_t vmem_alloc_info[VPU_MAX][20];
struct mutex mem_mutex;
static int cntMem_Reference = 0;
static int only_decmode = 0;

static int vdec_used[VPU_INST_MAX] = {0,};
static int venc_used[VPU_INST_MAX] = {0,};

/////////////////////////////////////////
// page type configuration
#define PAGE_TYPE_MAX	16
static int           g_iMmapPropCnt = 0;
static int           g_aiProperty[PAGE_TYPE_MAX];
static unsigned long g_aulPageOffset[PAGE_TYPE_MAX];

static void _vmem_set_page_type(phys_addr_t uiAddress, int iProperty)
{
	if( iProperty != 0 )
	{
		int idx = 0;

		while( idx < PAGE_TYPE_MAX ) {
			if( g_aiProperty[idx] == 0 )
				break;
			idx++;
		}

		if( idx < PAGE_TYPE_MAX ) {
			g_aiProperty[idx] = iProperty;
			g_aulPageOffset[idx] = uiAddress / PAGE_SIZE;
			g_iMmapPropCnt++;

			dprintk("_vmem_set_page_type(0x%08X, %d) \r\n"
				   , uiAddress
				   , iProperty
				   );
		}
		else {
			dprintk("_vmem_set_page_type(0x%08X, %d) - FAILED \r\n"
				   , uiAddress
				   , iProperty
				   );
		}
	}
}

static int _vmem_get_page_type(unsigned long ulPageOffset)
{
	int idx = 0;
	int prop = 0;

	while( idx < PAGE_TYPE_MAX ) {
		if( g_aulPageOffset[idx] == ulPageOffset )
			break;
		idx++;
	}

	prop = g_aiProperty[idx];

	g_aiProperty[idx] = 0;
	g_aulPageOffset[idx] = 0;

	g_iMmapPropCnt--;

	return prop;
}

pgprot_t vmem_get_pgprot(pgprot_t ulOldProt, unsigned long ulPageOffset)
{
	pgprot_t newProt;

	mutex_lock(&mem_mutex);

	if( g_iMmapPropCnt > 0 )
	{
		int prop = _vmem_get_page_type(ulPageOffset);

		switch( prop )
		{
			case 3:
				dprintk("_vmem_get_pgprot (write-combine) \r\n");
				newProt = pgprot_writecombine(ulOldProt);
				break;

			case 0:
			case 1:
			case 2:
			case 4:
			case 5:
			case 6:
			default:
				dprintk("_vmem_get_pgprot (default: non-cached) \r\n");
				newProt = pgprot_noncached(ulOldProt);
				break;
		}
	}
	else{
		dprintk("_vmem_get_pgprot (default: non-cached) \r\n");
		newProt = pgprot_noncached(ulOldProt);
	}

	mutex_unlock(&mem_mutex);
	return newProt;
}
EXPORT_SYMBOL(vmem_get_pgprot);
/////////////////////////////////////////

extern int _vmem_is_cma_allocated_phy_region(unsigned int start_phyaddr, unsigned int length);
static void* _vmem_get_virtaddr(unsigned int start_phyaddr, unsigned int length)
{
	void *cma_virt_address = NULL;

	dprintk_mem("_vmem_get_virtaddr :: phy_region[0x%x - 0x%x], !! \n", start_phyaddr, start_phyaddr + length);

	if (_vmem_is_cma_allocated_phy_region(start_phyaddr, length))
		cma_virt_address = pmap_cma_remap(start_phyaddr, PAGE_ALIGN(length));
	else
		cma_virt_address = (void*)ioremap_nocache((phys_addr_t)start_phyaddr, PAGE_ALIGN(length));

	if (cma_virt_address == NULL) {
		pr_err("_vmem_get_virtaddr: error ioremap for 0x%x / 0x%x \n", start_phyaddr, length);
		return cma_virt_address;
	}

	return cma_virt_address;
}

static void _vmem_release_virtaddr(void *target_virtaddr, unsigned int start_phyaddr, unsigned int length)
{
	dprintk_mem("_vmem_release_virtaddr :: phy_region[0x%x - 0x%x]!! \n", start_phyaddr, start_phyaddr + length);

	if (_vmem_is_cma_allocated_phy_region(start_phyaddr, length))
		pmap_cma_unmap(target_virtaddr, PAGE_ALIGN(length));
	else
		iounmap((void*)target_virtaddr);
}

#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
static phys_addr_t _vmem_request_phyaddr_dec_ext23(unsigned int request_size, vputype type)
{
	phys_addr_t curr_phyaddr = 0;

	if(sz_ext_remained_mem < request_size)
	{
		err("type[%d] : insufficient memory : remain(0x%x), request(0x%x) \n", type, sz_ext_remained_mem, request_size);
		return 0;
	}

	if(type == VPU_DEC_EXT2)
	{
		curr_phyaddr = ptr_ext_front_addr_mem;
		ptr_ext_front_addr_mem += request_size;
		sz_ext_front_used_mem += request_size;

		dtailk_mem_dec_ext23("type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, curr_phyaddr, ptr_ext_front_addr_mem, request_size, sz_ext_front_used_mem);
	}
	else//(type == VPU_DEC_EXT3)
	{
		ptr_ext_rear_addr_mem -= request_size;
		curr_phyaddr = ptr_ext_rear_addr_mem;
		sz_ext_rear_used_mem += request_size;

		dtailk_mem_dec_ext23("type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, curr_phyaddr, ptr_ext_rear_addr_mem + request_size, request_size, sz_ext_rear_used_mem);
	}

	sz_ext_remained_mem -= request_size;
	dtailk_mem_dec_ext23("type[%d] : mem usage = 0x%x(0x%x)/0x%x = Dec_ext2/3(0x%x + 0x%x) !! \n", type, sz_ext_front_used_mem+sz_ext_rear_used_mem, sz_ext_remained_mem, pmap_video_ext.size, sz_ext_front_used_mem, sz_ext_rear_used_mem);

	return curr_phyaddr;
}

static int _vmem_release_phyaddr_dec_ext23(phys_addr_t phyaddr, unsigned int size, vputype type)
{
	//calc remained_size and ptr_addr_mem
	if(type == VPU_DEC_EXT2)
	{
		dtailk_mem_dec_ext23("type[%d] : release ptr_ext_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !! \n", type, ptr_ext_front_addr_mem, (ptr_ext_front_addr_mem - size), ptr_ext_front_addr_mem, size);

		ptr_ext_front_addr_mem -= size;
		sz_ext_front_used_mem -= size;

		if(ptr_ext_front_addr_mem != phyaddr)
		{
			err("type[%d] :: ptr_ext_front_addr_mem release-mem order!! 0x%x != 0x%x \n", type, ptr_ext_front_addr_mem, phyaddr);
		}
	}
	else//(type == VPU_DEC_EXT3)
	{
		dtailk_mem_dec_ext23("type[%d] : release ptr_ext_rear_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x)!! \n", type, ptr_ext_rear_addr_mem, (ptr_ext_rear_addr_mem + size), ptr_ext_rear_addr_mem, size);

		if(ptr_ext_rear_addr_mem != phyaddr)
		{
			err("type[%d] :: ptr_ext_rear_addr_mem release-mem order!! 0x%x != 0x%x \n", type, ptr_ext_rear_addr_mem, phyaddr);
		}

		ptr_ext_rear_addr_mem += size;
		sz_ext_rear_used_mem -= size;
	}

	sz_ext_remained_mem += size;

	return 0;
}
#endif

#if defined(CONFIG_VDEC_CNT_5)
static phys_addr_t _vmem_request_phyaddr_dec_ext4(unsigned int request_size, vputype type)
{
	phys_addr_t curr_phyaddr = 0;

	if(sz_ext2_remained_mem < request_size)
	{
		err("type[%d] : insufficient memory : remain(0x%x), request(0x%x) \n", type, sz_ext2_remained_mem, request_size);
		return 0;
	}

	if(type == VPU_DEC_EXT4)
	{
		curr_phyaddr = ptr_ext2_front_addr_mem;
		ptr_ext2_front_addr_mem += request_size;
		sz_ext2_front_used_mem += request_size;

		dtailk_mem_dec_ext4("type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, curr_phyaddr, ptr_ext2_front_addr_mem, request_size, sz_ext2_front_used_mem);
	}
	else//(type == VPU_DEC_EXT5)
	{
		//ToDo!!
		printk("~~~~~~~~~~~~~ Not Implemented Function.!!!\n");
	}

	sz_ext2_remained_mem -= request_size;
	dtailk_mem_dec_ext4("type[%d] : mem usage = 0x%x(0x%x)/0x%x = Dec_ext4(0x%x) !! \n", type, sz_ext2_front_used_mem, sz_ext2_remained_mem, pmap_video_ext2.size, sz_ext2_front_used_mem);

	return curr_phyaddr;
}

static int _vmem_release_phyaddr_dec_ext4(phys_addr_t phyaddr, unsigned int size, vputype type)
{
	//calc remained_size and ptr_addr_mem
	if(type == VPU_DEC_EXT4)
	{
		dtailk_mem_dec_ext4("type[%d] : release ptr_ext2_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !! \n", type, ptr_ext2_front_addr_mem, (ptr_ext2_front_addr_mem - size), ptr_ext2_front_addr_mem, size);

		ptr_ext2_front_addr_mem -= size;
		sz_ext2_front_used_mem -= size;

		if(ptr_ext2_front_addr_mem != phyaddr)
		{
			err("type[%d] :: ptr_ext2_front_addr_mem release-mem order!! 0x%x != 0x%x \n", type, ptr_ext2_front_addr_mem, phyaddr);
		}
	}
	else//(type == VPU_DEC_EXT5)
	{
		//ToDo!!
		printk("~~~~~~~~~~~~~ Not Implemented Function.!!!\n");
	}

	sz_ext2_remained_mem += size;

	return 0;
}
#endif


#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
static phys_addr_t _vmem_request_phyaddr_enc_ext(unsigned int request_size, vputype type)
{
	phys_addr_t curr_phyaddr = 0;
	unsigned int enc_idx = type - VPU_ENC_EXT;

	if(sz_enc_ext_remained_mem[enc_idx] < request_size)
	{
		err("type[%d] : insufficient memory : remain(0x%x), request(0x%x) \n", type, sz_enc_ext_remained_mem[enc_idx], request_size);
		return 0;
	}

	curr_phyaddr = ptr_enc_ext_addr_mem[enc_idx];
	ptr_enc_ext_addr_mem[enc_idx] += request_size;
	sz_enc_ext_used_mem[enc_idx] += request_size;
	sz_enc_ext_remained_mem[enc_idx] -= request_size;

	dtailk_mem_enc_ext234("type[%d] : alloc : [%d] = 0x%x ~ 0x%x, 0x%x!!, used 0x%x/0x%x \n", type, enc_idx, curr_phyaddr, ptr_enc_ext_addr_mem[enc_idx], request_size, sz_enc_ext_used_mem[enc_idx], sz_enc_ext_remained_mem[enc_idx]);

	return curr_phyaddr;
}

static int _vmem_release_phyaddr_enc_ext(phys_addr_t phyaddr, unsigned int size, vputype type)
{
	unsigned int enc_idx = type - VPU_ENC_EXT;

	dtailk_mem_enc_ext234("type[%d] : release ptr_enc_ext_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !! \n", type, ptr_enc_ext_addr_mem[enc_idx], (ptr_enc_ext_addr_mem[enc_idx] - size), ptr_enc_ext_addr_mem[enc_idx], size);

	ptr_enc_ext_addr_mem[enc_idx] -= size;
	sz_enc_ext_used_mem[enc_idx] -= size;

	if(ptr_enc_ext_addr_mem[enc_idx] != phyaddr)
	{
		err("type[%d] :: ptr_enc_ext_addr_mem release-mem order!! 0x%x != 0x%x \n", type, ptr_enc_ext_addr_mem[enc_idx], phyaddr);
	}

	sz_enc_ext_remained_mem[enc_idx] += size;

	return 0;
}
#endif

static int _vmem_check_allocation_available(char check_type, unsigned int request_size, vputype type)
{
	switch(check_type)
	{
		case 0: // Check total memory which can allocate.
			if(sz_remained_mem < request_size)
			{
				err("type[%d] : insufficient memory : remain(0x%x), request(0x%x) \n", type, sz_remained_mem, request_size);
				return -1;
			}
			break;

		case 1: // Check encoder memory.
			if( sz_enc_mem < (sz_rear_used_mem + request_size) )
			{
				err("type[%d] : insufficient memory : total(0x%x) for only encoder, allocated(0x%x), request(0x%x) \n", type, sz_enc_mem, sz_rear_used_mem, request_size);
				return -1;
			}
			break;

		case 2: // Check remainning except encoder memory.
			if( (pmap_video.size - sz_enc_mem - VPU_SW_ACCESS_REGION_SIZE) < (sz_ext_used_mem + sz_front_used_mem + request_size) )
			{
				err("type[%d] : insufficient memory : total(0x%x) except encoder, allocated(0x%x), request(0x%x) \n", type, (pmap_video.size - sz_enc_mem), (sz_ext_used_mem + sz_front_used_mem), request_size);
				return -1;
			}
			break;

		default:
			break;
	}

	return 0;
}

static phys_addr_t _vmem_request_phyaddr(unsigned int request_size, vputype type)
{
	phys_addr_t curr_phyaddr = 0;

#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
	if(_vmem_check_allocation_available(0, request_size, type))
		return 0;

	if(type == VPU_DEC || type == VPU_ENC)
	{
		curr_phyaddr = ptr_front_addr_mem;
		ptr_front_addr_mem += request_size;
		sz_front_used_mem += request_size;

		detailk_mem_dec("type[%d] : alloc front = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, curr_phyaddr, ptr_front_addr_mem, request_size, sz_front_used_mem);
	}
	else if(type == VPU_DEC_EXT || type == VPU_ENC_EXT)
	{
		ptr_rear_addr_mem -= request_size;
		curr_phyaddr = ptr_rear_addr_mem;
		sz_rear_used_mem += request_size;

		detailk_mem_dec("type[%d] : alloc rear = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, (ptr_rear_addr_mem + request_size), curr_phyaddr, request_size, sz_rear_used_mem);
	}
#else
	if(only_decmode)
	{
		if(_vmem_check_allocation_available(0, request_size, type))
			return 0;
	}
	else
	{
		if(type == VPU_ENC)
		{
			if(_vmem_check_allocation_available(1, request_size, type) < 0)
				return 0;
		}
		else // for Decoder_ext or Decoder.
		{
			if(type == VPU_DEC_EXT) // regarding to encoder.
			{
				if(_vmem_check_allocation_available(2, request_size, type) < 0)
					return 0;
			}
			else // only for 1st Decoder.
			{
				if( vmgr_get_close(VPU_DEC_EXT) && vmgr_get_close(VPU_ENC)
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
					&& vmgr_4k_d2_get_close(VPU_DEC_EXT) && vmgr_4k_d2_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
					&& hmgr_get_close(VPU_DEC_EXT) && hmgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
					&& vp9mgr_get_close(VPU_DEC_EXT) && vp9mgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
					&& jmgr_get_close(VPU_DEC_EXT) && jmgr_get_close(VPU_ENC)
#endif
				)
				{
					if(_vmem_check_allocation_available(0, request_size, type) < 0)
						return 0;
				}
				else // in case Decoder only is opened with Decoder_ext or Encoder.
				{
					if(_vmem_check_allocation_available(2, request_size, type) < 0)
						return 0;
				}
			}
		}
	}

	if(type == VPU_DEC)
	{
		curr_phyaddr = ptr_front_addr_mem;
		ptr_front_addr_mem += request_size;
		sz_front_used_mem += request_size;

		detailk_mem_dec("type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, curr_phyaddr, ptr_front_addr_mem, request_size, sz_front_used_mem);
	}
	else if(type == VPU_DEC_EXT)
	{
		if(only_decmode)
		{
			ptr_rear_addr_mem -= request_size;
			curr_phyaddr = ptr_rear_addr_mem;
			sz_rear_used_mem += request_size;

			detailk_mem_dec("type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, (ptr_rear_addr_mem + request_size), curr_phyaddr, request_size, sz_rear_used_mem);
		}
		else
		{
			ptr_ext_addr_mem -= request_size;
			curr_phyaddr = ptr_ext_addr_mem;
			sz_ext_used_mem += request_size;

			detailk_mem_dec("type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x \n", type, (ptr_ext_addr_mem + request_size), curr_phyaddr, request_size, sz_ext_used_mem);
		}
	}
	else//VPU_ENC
	{
		if(only_decmode){
			err("type[%d] : There is no memory for encoder because of only decoder mode.\n", type );
			return 0;
		}

		ptr_rear_addr_mem -= request_size;
		curr_phyaddr = ptr_rear_addr_mem;
		sz_rear_used_mem += request_size;

		detailk_mem_dec("type[%d] : alloc = 0x%x ~ 0x%x, 0x%d!!, used 0x%d \n", type, (ptr_rear_addr_mem + request_size), curr_phyaddr, request_size, sz_rear_used_mem);
	}
#endif
	detailk_mem_dec("type[%d] : mem usage = 0x%x/0x%x = Dec(f:0x%x + ext:0x%x) + Rear(0x%x (Enc?)) !! \n", type, sz_front_used_mem+sz_rear_used_mem+sz_ext_used_mem, pmap_video.size, sz_front_used_mem, sz_ext_used_mem, sz_rear_used_mem);

	sz_remained_mem -= request_size;

	return curr_phyaddr;
}

static int _vmem_release_phyaddr(phys_addr_t phyaddr, unsigned int size, vputype type)
{
#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
	if(type == VPU_DEC || type == VPU_ENC)
	{
		detailk_mem_dec("type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !! \n", type, ptr_front_addr_mem, (ptr_front_addr_mem - size), ptr_front_addr_mem, size);

		ptr_front_addr_mem -= size;
		sz_front_used_mem -= size;

		if(ptr_front_addr_mem != phyaddr)
		{
			err("type[%d] :: release-mem order!! 0x%x != 0x%x \n", type, ptr_front_addr_mem, phyaddr);
		}
	}
	else if(type == VPU_DEC_EXT || type == VPU_ENC_EXT)
	{
		detailk_mem_dec("type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x)!! \n", type, ptr_rear_addr_mem, (ptr_rear_addr_mem + size), ptr_rear_addr_mem, size);

		if(ptr_rear_addr_mem != phyaddr)
		{
			err("type[%d] :: release-mem order!! 0x%x != 0x%x \n", type, ptr_rear_addr_mem, phyaddr);
		}

		ptr_rear_addr_mem += size;
		sz_rear_used_mem -= size;
	}
#else
	//calc remained_size and ptr_addr_mem
	if(type == VPU_DEC)
	{
		detailk_mem_dec("type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !! \n", type, ptr_front_addr_mem, (ptr_front_addr_mem - size), ptr_front_addr_mem, size);

		ptr_front_addr_mem -= size;
		sz_front_used_mem -= size;

		if(ptr_front_addr_mem != phyaddr)
		{
			err("type[%d] :: release-mem order!! 0x%x != 0x%x \n", type, ptr_front_addr_mem, phyaddr);
		}
	}
	else if(type == VPU_DEC_EXT)
	{
		if(only_decmode)
		{
			detailk_mem_dec("type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x)!! \n", type, ptr_rear_addr_mem, (ptr_rear_addr_mem + size), ptr_rear_addr_mem, size);

			if(ptr_rear_addr_mem != phyaddr)
			{
				err("type[%d] :: release-mem order!! 0x%x != 0x%x \n", type, ptr_rear_addr_mem, phyaddr);
			}

			ptr_rear_addr_mem += size;
			sz_rear_used_mem -= size;
		}
		else
		{
			detailk_mem_dec("type[%d] : release ptr_ext_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x)!! \n", type, ptr_ext_addr_mem, (ptr_ext_addr_mem + size), ptr_ext_addr_mem, size);

			if(ptr_ext_addr_mem != phyaddr)
			{
				err("type[%d] :: release-mem order!! 0x%x != 0x%x \n", type, ptr_ext_addr_mem, phyaddr);
			}

			ptr_ext_addr_mem += size;
			sz_ext_used_mem -= size;
		}
	}
	else//VPU_ENC
	{
		detailk_mem_dec("type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x)!! \n", type, ptr_rear_addr_mem, (ptr_rear_addr_mem + size), ptr_rear_addr_mem, size);

		if(ptr_rear_addr_mem != phyaddr)
		{
			err("type[%d] :: release-mem order!! 0x%x != 0x%x \n", type, ptr_rear_addr_mem, phyaddr);
		}

		ptr_rear_addr_mem += size;
		sz_rear_used_mem -= size;
	}
#endif

	sz_remained_mem += size;

	return 0;
}

int _vmem_alloc_dedicated_buffer(void)
{
	int ret = 0;
	int type = 0;
	unsigned int vpu_4k_d2_alloc_size = 0;
	unsigned int hevc_alloc_size = 0;
	unsigned int jpu_alloc_size = 0;
	unsigned int vpu_alloc_size = 0;
	unsigned int vp9_alloc_size = 0;
	unsigned int seq_alloc_size = 0;
	unsigned int user_alloc_size = 0;
#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	unsigned int alloc_ps_size = 0;
	unsigned int alloc_slice_size = 0;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
    if (gsVpu4KD2Work_memInfo.kernel_remap_addr == 0) {
        // VPU_4K_D2 WORK BUFFER
        gsVpu4KD2Work_memInfo.request_size = WAVExxx_WORK_BUF_SIZE;
        if (gsVpu4KD2Work_memInfo.request_size)
        {
            gsVpu4KD2Work_memInfo.phy_addr = ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
			gsVpu4KD2Work_memInfo.kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsVpu4KD2Work_memInfo.phy_addr, PAGE_ALIGN(gsVpu4KD2Work_memInfo.request_size/*-PAGE_SIZE*/));

            dprintk_mem("alloc VPU_4K_D2 workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsVpu4KD2Work_memInfo.phy_addr, gsVpu4KD2Work_memInfo.kernel_remap_addr, gsVpu4KD2Work_memInfo.request_size);

            if (gsVpu4KD2Work_memInfo.kernel_remap_addr == 0) {
                err("[0x%p] VPU_4K_D2 workbuffer remap ALLOC_MEMORY failed.\n", gsVpu4KD2Work_memInfo.kernel_remap_addr );
                ret = -1;
                goto Error;
            }
            //memset_io(gsVpu4KD2Work_memInfo.kernel_remap_addr, 0x00, WAVExxx_WORK_BUF_SIZE);
            vpu_4k_d2_alloc_size = gsVpu4KD2Work_memInfo.request_size;
        }
    }
    else{
        err("[0x%p] VPU_4K_D2-WorkBuff : already remapped? \n", gsVpu4KD2Work_memInfo.kernel_remap_addr );
    }
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC // HEVC
	if( gsWave410_Work_memInfo.kernel_remap_addr == 0 )
	{
	// HEVC WORK BUFFER
		gsWave410_Work_memInfo.request_size = WAVExxx_WORK_BUF_SIZE;
		if( gsWave410_Work_memInfo.request_size )
		{
			gsWave410_Work_memInfo.phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
			gsWave410_Work_memInfo.kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsWave410_Work_memInfo.phy_addr, PAGE_ALIGN(gsWave410_Work_memInfo.request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc HEVC workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsWave410_Work_memInfo.phy_addr, gsWave410_Work_memInfo.kernel_remap_addr, gsWave410_Work_memInfo.request_size);

			if (gsWave410_Work_memInfo.kernel_remap_addr == 0) {
				err("[0x%p] HEVC workbuffer remap ALLOC_MEMORY failed.\n", gsWave410_Work_memInfo.kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			//memset_io((void*)gsWave410_Work_memInfo.kernel_remap_addr, 0x00, WAVExxx_WORK_BUF_SIZE);
			hevc_alloc_size = gsWave410_Work_memInfo.request_size;
		}
	}
	else{
		err("[0x%p] HEVC-WorkBuff : already remapped? \n", gsWave410_Work_memInfo.kernel_remap_addr );
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
	if( gsG2V2_Vp9Work_memInfo.kernel_remap_addr == 0 )
	{
	// VP9 WORK BUFFER
		gsG2V2_Vp9Work_memInfo.request_size = G2V2_VP9_WORK_BUF_SIZE;
		if( gsG2V2_Vp9Work_memInfo.request_size )
		{
			gsG2V2_Vp9Work_memInfo.phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
			gsG2V2_Vp9Work_memInfo.kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsG2V2_Vp9Work_memInfo.phy_addr, PAGE_ALIGN(gsG2V2_Vp9Work_memInfo.request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VP9 workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsG2V2_Vp9Work_memInfo.phy_addr, gsG2V2_Vp9Work_memInfo.kernel_remap_addr, gsG2V2_Vp9Work_memInfo.request_size);

			if (gsG2V2_Vp9Work_memInfo.kernel_remap_addr == 0) {
				err("[0x%p] VP9 workbuffer remap ALLOC_MEMORY failed.\n", gsG2V2_Vp9Work_memInfo.kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			//memset_io((void*)gsG2V2_Vp9Work_memInfo.kernel_remap_addr, 0x00, G2V2_VP9_WORK_BUF_SIZE);
			hevc_alloc_size = gsG2V2_Vp9Work_memInfo.request_size;
		}
	}
	else{
		err("[0x%p] VP9-WorkBuff : already remapped? \n", gsG2V2_Vp9Work_memInfo.kernel_remap_addr );
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
	if( gsJpuWork_memInfo.kernel_remap_addr == 0 )
	{
	// JPU WORK BUFFER
		gsJpuWork_memInfo.request_size = JPU_WORK_BUF_SIZE;
		if( gsJpuWork_memInfo.request_size )
		{
			gsJpuWork_memInfo.phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
			gsJpuWork_memInfo.kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsJpuWork_memInfo.phy_addr, PAGE_ALIGN(gsJpuWork_memInfo.request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc JPU workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsJpuWork_memInfo.phy_addr, gsJpuWork_memInfo.kernel_remap_addr, gsJpuWork_memInfo.request_size);

			if (gsJpuWork_memInfo.kernel_remap_addr == 0) {
				err("[0x%p] JPU workbuffer remap ALLOC_MEMORY failed.\n", gsJpuWork_memInfo.kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			//memset_io((void*)gsJpuWork_memInfo.kernel_remap_addr, 0x00, JPU_WORK_BUF_SIZE);
			jpu_alloc_size = gsJpuWork_memInfo.request_size;
		}
	}
	else{
		err("[0x%p] JPU-WorkBuff : already remapped? \n", gsJpuWork_memInfo.kernel_remap_addr );
	}
#endif

	if( gsVpuWork_memInfo.kernel_remap_addr == 0 )
	{
	// VPU WORK BUFFER
		gsVpuWork_memInfo.request_size = VPU_WORK_BUF_SIZE;
		if( gsVpuWork_memInfo.request_size )
		{
			gsVpuWork_memInfo.phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
			gsVpuWork_memInfo.kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsVpuWork_memInfo.phy_addr, PAGE_ALIGN(gsVpuWork_memInfo.request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsVpuWork_memInfo.phy_addr, gsVpuWork_memInfo.kernel_remap_addr, gsVpuWork_memInfo.request_size);

			if (gsVpuWork_memInfo.kernel_remap_addr == 0) {
				err("[0x%p] VPU workbuffer remap ALLOC_MEMORY failed.\n", gsVpuWork_memInfo.kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			//memset_io((void*)gsVpuWork_memInfo.kernel_remap_addr, 0x00, WORK_CODE_PARA_BUF_SIZE);
			vpu_alloc_size = gsVpuWork_memInfo.request_size;
		}
	}
	else{
		err("[0x%p] VPU-WorkBuff : already remapped? \n", gsVpuWork_memInfo.kernel_remap_addr );
	}

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	for(type = 0; type < VPU_ENC_MAX_CNT; type++)
	{
		if( gsVpuEncSeqheader_memInfo[type].kernel_remap_addr == 0 )
		{
		// SEQ-HEADER BUFFER FOR ENCODER
			gsVpuEncSeqheader_memInfo[type].request_size 		= PAGE_ALIGN(ENC_HEADER_BUF_SIZE);
			if( gsVpuEncSeqheader_memInfo[type].request_size )
			{
				gsVpuEncSeqheader_memInfo[type].phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
				gsVpuEncSeqheader_memInfo[type].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsVpuEncSeqheader_memInfo[type].phy_addr, PAGE_ALIGN(gsVpuEncSeqheader_memInfo[type].request_size/*-PAGE_SIZE*/));

				dprintk_mem("alloc VPU seqheader_mem[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", type, gsVpuEncSeqheader_memInfo[type].phy_addr, gsVpuEncSeqheader_memInfo[type].kernel_remap_addr, gsVpuEncSeqheader_memInfo[type].request_size);

				if (gsVpuEncSeqheader_memInfo[type].kernel_remap_addr == 0) {
					err("[%d] VPU seqheader_mem[0x%p] remap ALLOC_MEMORY failed.\n", type, gsVpuEncSeqheader_memInfo[type].kernel_remap_addr );
					ret = -1;
					goto Error;
				}
				seq_alloc_size += gsVpuEncSeqheader_memInfo[type].request_size;
			}
		}
		else{
			err("[%d] SeqHeader[0x%p] : already remapped? \n", type, gsVpuEncSeqheader_memInfo[type].kernel_remap_addr );
		}
	}
#endif

	for(type = 0; type < VPU_INST_MAX; type++)
	{
		if( gsVpuUserData_memInfo[type].kernel_remap_addr == 0 )
		{
		// USER DATA
			gsVpuUserData_memInfo[type].request_size 		= PAGE_ALIGN(USER_DATA_BUF_SIZE);
			if( gsVpuUserData_memInfo[type].request_size )
			{
				gsVpuUserData_memInfo[type].phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size;
				gsVpuUserData_memInfo[type].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsVpuUserData_memInfo[type].phy_addr, PAGE_ALIGN(gsVpuUserData_memInfo[type].request_size/*-PAGE_SIZE*/));

				dprintk_mem("alloc VPU userdata_mem[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", type, gsVpuUserData_memInfo[type].phy_addr, gsVpuUserData_memInfo[type].kernel_remap_addr, gsVpuUserData_memInfo[type].request_size);

				if (gsVpuUserData_memInfo[type].kernel_remap_addr == 0) {
					err("[%d] VPU userdata_mem[0x%p] remap ALLOC_MEMORY failed.\n", type, gsVpuUserData_memInfo[type].kernel_remap_addr );
					ret = -1;
					goto Error;
				}
				user_alloc_size += gsVpuUserData_memInfo[type].request_size;
			}
		}
		else{
			err("[%d] UserData[0x%p] : already remapped? \n", type, gsVpuUserData_memInfo[type].kernel_remap_addr );
		}
	}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	if( gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0 )
	{
		gsPs_memInfo[VPU_DEC_EXT2].request_size 			= PAGE_ALIGN(PS_SAVE_SIZE);
		if( gsPs_memInfo[VPU_DEC_EXT2].request_size )
		{
			gsPs_memInfo[VPU_DEC_EXT2].phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size + alloc_ps_size + alloc_slice_size;
			gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsPs_memInfo[VPU_DEC_EXT2].phy_addr, PAGE_ALIGN(gsPs_memInfo[VPU_DEC_EXT2].request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU Ps[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", VPU_DEC_EXT2, gsPs_memInfo[VPU_DEC_EXT2].phy_addr, gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr, gsPs_memInfo[VPU_DEC_EXT2].request_size);

			if (gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
				err("[%d] VPU Ps[0x%p] remap ALLOC_MEMORY failed.\n", VPU_DEC_EXT2, gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			alloc_ps_size = gsPs_memInfo[VPU_DEC_EXT2].request_size;
		}
	}
	else{
		err("[%d] Ps[0x%p] : already remapped? \n", VPU_DEC_EXT2, gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr );
	}

	if( gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0 )
	{
		gsSlice_memInfo[VPU_DEC_EXT2].request_size 			= PAGE_ALIGN(SLICE_SAVE_SIZE);
		if( gsSlice_memInfo[VPU_DEC_EXT2].request_size )
		{
			gsSlice_memInfo[VPU_DEC_EXT2].phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size + alloc_ps_size + alloc_slice_size;
			gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsSlice_memInfo[VPU_DEC_EXT2].phy_addr, PAGE_ALIGN(gsSlice_memInfo[VPU_DEC_EXT2].request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU Slice[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", VPU_DEC_EXT2, gsSlice_memInfo[VPU_DEC_EXT2].phy_addr, gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr, gsSlice_memInfo[VPU_DEC_EXT2].request_size);

			if (gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
				err("[%d] VPU Slice[0x%p] remap ALLOC_MEMORY failed.\n", VPU_DEC_EXT2, gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			alloc_slice_size = gsSlice_memInfo[VPU_DEC_EXT2].request_size;
		}
	}
	else{
		err("[%d] Slice[0x%p] : already remapped? \n", VPU_DEC_EXT2, gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr );
	}

	if( gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0 )
	{
		gsStream_memInfo[VPU_DEC_EXT2].request_size 			= PAGE_ALIGN(LARGE_STREAM_BUF_SIZE);
		if( gsStream_memInfo[VPU_DEC_EXT2].request_size )
		{
			gsStream_memInfo[VPU_DEC_EXT2].phy_addr 			= ptr_sw_addr_mem + vpu_4k_d2_alloc_size + hevc_alloc_size + jpu_alloc_size + vpu_alloc_size + vp9_alloc_size + seq_alloc_size + user_alloc_size + alloc_ps_size + alloc_slice_size;
			gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsStream_memInfo[VPU_DEC_EXT2].phy_addr, PAGE_ALIGN(gsStream_memInfo[VPU_DEC_EXT2].request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU Stream[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", VPU_DEC_EXT2, gsStream_memInfo[VPU_DEC_EXT2].phy_addr, gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr, gsStream_memInfo[VPU_DEC_EXT2].request_size);

			if (gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
				err("[%d] VPU Stream[0x%p] remap ALLOC_MEMORY failed.\n", VPU_DEC_EXT2, gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr );
				ret = -1;
				goto Error;
			}
		}
	}
	else{
		err("[%d] Stream[0x%p] : already remapped? \n", VPU_DEC_EXT2, gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr );
	}

	alloc_ps_size = alloc_slice_size = 0;

	if( gsPs_memInfo[VPU_DEC].kernel_remap_addr == 0 )
	{
		gsPs_memInfo[VPU_DEC].request_size 			= PAGE_ALIGN(PS_SAVE_SIZE);
		if( gsPs_memInfo[VPU_DEC].request_size )
		{
			gsPs_memInfo[VPU_DEC].phy_addr 			= ptr_ext_rear_addr_mem + alloc_ps_size + alloc_slice_size;
			gsPs_memInfo[VPU_DEC].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsPs_memInfo[VPU_DEC].phy_addr, PAGE_ALIGN(gsPs_memInfo[VPU_DEC].request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU Ps[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", VPU_DEC, gsPs_memInfo[VPU_DEC].phy_addr, gsPs_memInfo[VPU_DEC].kernel_remap_addr, gsPs_memInfo[VPU_DEC].request_size);

			if (gsPs_memInfo[VPU_DEC].kernel_remap_addr == 0) {
				err("[0x%p] VPU Ps[%d] remap ALLOC_MEMORY failed.\n", VPU_DEC, gsPs_memInfo[VPU_DEC].kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			alloc_ps_size = gsPs_memInfo[VPU_DEC].request_size;
		}
	}
	else{
		err("[%d] Ps[0x%p] : already remapped? \n", VPU_DEC, gsPs_memInfo[VPU_DEC].kernel_remap_addr );
	}

	if( gsSlice_memInfo[VPU_DEC].kernel_remap_addr == 0 )
	{
		gsSlice_memInfo[VPU_DEC].request_size 			= PAGE_ALIGN(SLICE_SAVE_SIZE);
		if( gsSlice_memInfo[VPU_DEC].request_size )
		{
			gsSlice_memInfo[VPU_DEC].phy_addr 			= ptr_ext_rear_addr_mem + alloc_ps_size + alloc_slice_size;
			gsSlice_memInfo[VPU_DEC].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsSlice_memInfo[VPU_DEC].phy_addr, PAGE_ALIGN(gsSlice_memInfo[VPU_DEC].request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU Slice[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", VPU_DEC, gsSlice_memInfo[VPU_DEC].phy_addr, gsSlice_memInfo[VPU_DEC].kernel_remap_addr, gsSlice_memInfo[VPU_DEC].request_size);

			if (gsSlice_memInfo[VPU_DEC].kernel_remap_addr == 0) {
				err("[0x%p] VPU Slice[%d] remap ALLOC_MEMORY failed.\n", VPU_DEC, gsSlice_memInfo[VPU_DEC].kernel_remap_addr );
				ret = -1;
				goto Error;
			}
			alloc_slice_size = gsSlice_memInfo[VPU_DEC].request_size;
		}
	}
	else{
		err("[%d] Slice[0x%p] : already remapped? \n", VPU_DEC, gsSlice_memInfo[VPU_DEC].kernel_remap_addr );
	}

	if( gsStream_memInfo[VPU_DEC].kernel_remap_addr == 0 )
	{
		gsStream_memInfo[VPU_DEC].request_size 			= PAGE_ALIGN(LARGE_STREAM_BUF_SIZE);
		if( gsStream_memInfo[VPU_DEC].request_size )
		{
			gsStream_memInfo[VPU_DEC].phy_addr 			= ptr_ext_rear_addr_mem + alloc_ps_size + alloc_slice_size;
			gsStream_memInfo[VPU_DEC].kernel_remap_addr = _vmem_get_virtaddr((phys_addr_t)gsStream_memInfo[VPU_DEC].phy_addr, PAGE_ALIGN(gsStream_memInfo[VPU_DEC].request_size/*-PAGE_SIZE*/));

			dprintk_mem("alloc VPU Stream[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", VPU_DEC, gsStream_memInfo[VPU_DEC].phy_addr, gsStream_memInfo[VPU_DEC].kernel_remap_addr, gsStream_memInfo[VPU_DEC].request_size);

			if (gsStream_memInfo[VPU_DEC].kernel_remap_addr == 0) {
				err("[0x%p] VPU Stream[%d] remap ALLOC_MEMORY failed.\n", VPU_DEC, gsStream_memInfo[VPU_DEC].kernel_remap_addr );
				ret = -1;
				goto Error;
			}
		}
	}
	else{
		err("[%d] Stream[0x%p] : already remapped? \n", VPU_DEC, gsStream_memInfo[VPU_DEC].kernel_remap_addr );
	}
#endif

Error:
	return ret;
}

void _vmem_free_dedicated_buffer(void)
{
	int type = 0;

	dprintk_mem("%s \n", __func__);
	// Memory Management!!
#ifdef CONFIG_SUPPORT_TCC_JPU
	if( gsJpuWork_memInfo.kernel_remap_addr != 0 )
	{
		dprintk("free JPU workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsJpuWork_memInfo.phy_addr, gsJpuWork_memInfo.kernel_remap_addr, gsJpuWork_memInfo.request_size);
		_vmem_release_virtaddr(((void*)gsJpuWork_memInfo.kernel_remap_addr), gsJpuWork_memInfo.phy_addr, gsJpuWork_memInfo.request_size);
		memset(&gsJpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
	if( gsVpu4KD2Work_memInfo.kernel_remap_addr != 0 )
	{
		dprintk("free VPU_4K_D2 workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsVpu4KD2Work_memInfo.phy_addr, gsVpu4KD2Work_memInfo.kernel_remap_addr, gsVpu4KD2Work_memInfo.request_size);
		_vmem_release_virtaddr(((void*)gsVpu4KD2Work_memInfo.kernel_remap_addr), gsVpu4KD2Work_memInfo.phy_addr, gsVpu4KD2Work_memInfo.request_size);
		memset(&gsVpu4KD2Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC // HEVC
	if( gsWave410_Work_memInfo.kernel_remap_addr != 0 )
	{
		dprintk("free HEVC workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsWave410_Work_memInfo.phy_addr, gsWave410_Work_memInfo.kernel_remap_addr, gsWave410_Work_memInfo.request_size);
		_vmem_release_virtaddr(((void*)gsWave410_Work_memInfo.kernel_remap_addr), gsWave410_Work_memInfo.phy_addr, gsWave410_Work_memInfo.request_size);
		memset(&gsWave410_Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
	if( gsG2V2_Vp9Work_memInfo.kernel_remap_addr != 0 )
	{
		dprintk("free VP9 workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsG2V2_Vp9Work_memInfo.phy_addr, gsG2V2_Vp9Work_memInfo.kernel_remap_addr, gsG2V2_Vp9Work_memInfo.request_size);
		_vmem_release_virtaddr(((void*)gsG2V2_Vp9Work_memInfo.kernel_remap_addr), gsG2V2_Vp9Work_memInfo.phy_addr, gsG2V2_Vp9Work_memInfo.request_size);
		memset(&gsG2V2_Vp9Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	for(type = 0; type < VPU_ENC_MAX_CNT; type++) {
		if( gsVpuEncSeqheader_memInfo[type].kernel_remap_addr != 0 )
		{
			dprintk("free SeqHeader[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", type, gsVpuEncSeqheader_memInfo[type].phy_addr, gsVpuEncSeqheader_memInfo[type].kernel_remap_addr, gsVpuEncSeqheader_memInfo[type].request_size);
			_vmem_release_virtaddr(((void*)gsVpuEncSeqheader_memInfo[type].kernel_remap_addr), gsVpuEncSeqheader_memInfo[type].phy_addr, gsVpuEncSeqheader_memInfo[type].request_size);
			memset(&gsVpuEncSeqheader_memInfo[type], 0x00, sizeof(MEM_ALLOC_INFO_t));
		}
	}
#endif

	if( gsVpuWork_memInfo.kernel_remap_addr != 0 )
	{
		dprintk("free workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsVpuWork_memInfo.phy_addr, gsVpuWork_memInfo.kernel_remap_addr, gsVpuWork_memInfo.request_size);
		_vmem_release_virtaddr(((void*)gsVpuWork_memInfo.kernel_remap_addr), gsVpuWork_memInfo.phy_addr, gsVpuWork_memInfo.request_size);
		memset(&gsVpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}

	for(type = 0; type < VPU_INST_MAX; type++){
		if( gsVpuUserData_memInfo[type].kernel_remap_addr != 0 )
		{
			dprintk("free UserData[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", type, gsVpuUserData_memInfo[type].phy_addr, gsVpuUserData_memInfo[type].kernel_remap_addr, gsVpuUserData_memInfo[type].request_size);
			_vmem_release_virtaddr(((void*)gsVpuUserData_memInfo[type].kernel_remap_addr), gsVpuUserData_memInfo[type].phy_addr, gsVpuUserData_memInfo[type].request_size);
			memset(&gsVpuUserData_memInfo[type], 0x00, sizeof(MEM_ALLOC_INFO_t));
		}
	}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	{
		int type = 0;

		for(type = 0; type < VPU_INST_MAX; type++)
		{
			if( gsPs_memInfo[type].kernel_remap_addr != 0 )
			{
				dprintk("free PS :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsPs_memInfo[type].phy_addr, gsPs_memInfo[type].kernel_remap_addr, gsPs_memInfo[type].request_size);
				_vmem_release_virtaddr(((void*)gsPs_memInfo[type].kernel_remap_addr), gsPs_memInfo[type].phy_addr, gsPs_memInfo[type].request_size);
				memset(&gsPs_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
			}
			if( gsSlice_memInfo[type].kernel_remap_addr != 0 )
			{
				dprintk("free Slice :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsSlice_memInfo[type].phy_addr, gsSlice_memInfo[type].kernel_remap_addr, gsSlice_memInfo[type].request_size);
				_vmem_release_virtaddr(((void*)gsSlice_memInfo[type].kernel_remap_addr), gsSlice_memInfo[type].phy_addr, gsSlice_memInfo[type].request_size);
				memset(&gsSlice_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
			}
			if( gsStream_memInfo[type].kernel_remap_addr != 0 )
			{
				dprintk("free Stream :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", gsStream_memInfo[type].phy_addr, gsStream_memInfo[type].kernel_remap_addr, gsStream_memInfo[type].request_size);
				_vmem_release_virtaddr(((void*)gsStream_memInfo[type].kernel_remap_addr), gsStream_memInfo[type].phy_addr, gsStream_memInfo[type].request_size);
				memset(&gsStream_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
			}
		}
	}
#endif
}


static phys_addr_t _vmem_request_workbuff_phyaddr(int codec_type, void **remapped_addr)
{
	phys_addr_t phy_address = 0x00;
#ifdef CONFIG_SUPPORT_TCC_JPU // M-Jpeg
	if(codec_type == STD_MJPG)
	{
		*remapped_addr = gsJpuWork_memInfo.kernel_remap_addr;
		phy_address = gsJpuWork_memInfo.phy_addr;
	}
	else
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
	if(codec_type == STD_HEVC || codec_type == STD_VP9)
	{
		*remapped_addr = gsVpu4KD2Work_memInfo.kernel_remap_addr;
		phy_address = gsVpu4KD2Work_memInfo.phy_addr;
	}
	else
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC // HEVC
	if(codec_type == STD_HEVC)
	{
		*remapped_addr = gsWave410_Work_memInfo.kernel_remap_addr;
		phy_address = gsWave410_Work_memInfo.phy_addr;
	}
	else
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
	if(codec_type == STD_VP9)
	{
		*remapped_addr = gsG2V2_Vp9Work_memInfo.kernel_remap_addr;
		phy_address = gsG2V2_Vp9Work_memInfo.phy_addr;
	}
	else
#endif
	{
		*remapped_addr = gsVpuWork_memInfo.kernel_remap_addr;
		phy_address = gsVpuWork_memInfo.phy_addr;
	}

	return phy_address;
}

static phys_addr_t _vmem_request_seqheader_buff_phyaddr(int type, void **remapped_addr, unsigned int *request_size)
{
	int enc_type = type - VPU_ENC;

	dprintk("request seqheader type[%d]-enc_type[%d] \n", type, enc_type);
	if(enc_type < 0 || enc_type >= VPU_ENC_MAX_CNT)
		return 0;
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	*remapped_addr = gsVpuEncSeqheader_memInfo[enc_type].kernel_remap_addr;
	*request_size = gsVpuEncSeqheader_memInfo[enc_type].request_size;

	dprintk("request seqheader remap = 0x%p, size = 0x%x !! \n", gsVpuEncSeqheader_memInfo[enc_type].kernel_remap_addr, gsVpuEncSeqheader_memInfo[enc_type].request_size);
	return gsVpuEncSeqheader_memInfo[enc_type].phy_addr;
#else
	return 0;
#endif
}

static phys_addr_t _vmem_request_userdata_buff_phyaddr(int type, void **remapped_addr, unsigned int *request_size)
{
    if(type < 0 || type >= VPU_INST_MAX)
        return 0;

	*remapped_addr = gsVpuUserData_memInfo[type].kernel_remap_addr;
	*request_size = gsVpuUserData_memInfo[type].request_size;

	return gsVpuUserData_memInfo[type].phy_addr;
}


#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
static phys_addr_t _vmem_request_ps_buff_phyaddr(int type, void **remapped_addr, unsigned int *request_size)
{
    if(type < 0 || type >= VPU_INST_MAX)
        return 0;

	*remapped_addr = gsPs_memInfo[type].kernel_remap_addr;
	*request_size = gsPs_memInfo[type].request_size;

	return gsPs_memInfo[type].phy_addr;
}

static phys_addr_t _vmem_request_slice_buff_phyaddr(int type, void **remapped_addr, unsigned int *request_size)
{
    if(type < 0 || type >= VPU_INST_MAX)
        return 0;

	*remapped_addr = gsSlice_memInfo[type].kernel_remap_addr;
	*request_size = gsSlice_memInfo[type].request_size;

	return gsSlice_memInfo[type].phy_addr;
}

static phys_addr_t _vmem_request_stream_buff_phyaddr(int type, void **remapped_addr, unsigned int *request_size)
{
    if(type < 0 || type >= VPU_INST_MAX)
        return 0;

	*remapped_addr = gsStream_memInfo[type].kernel_remap_addr;
	*request_size = gsStream_memInfo[type].request_size;

	return gsStream_memInfo[type].phy_addr;
}
#endif

int _vmem_init_memory_info(void)
{
	int ret = 0;

	if(0 > pmap_get_info("video", &pmap_video)){
		ret = -10;
		goto Error;
	}

	if(0 > pmap_get_info("video_sw", &pmap_video_sw)){
		ret = -11;
		goto Error;
	}

	ptr_front_addr_mem = (phys_addr_t)pmap_video.base;
	sz_remained_mem = pmap_video.size;
	dprintk_mem("%s :: pmap_video(0x%x - 0x%x) sw(0x%x - 0x%x/0x%x) !! \n", __func__, pmap_video.base, pmap_video.size,  pmap_video_sw.base, pmap_video_sw.size, VPU_SW_ACCESS_REGION_SIZE);

	if( pmap_video_sw.size < VPU_SW_ACCESS_REGION_SIZE){
		printk("%s-%d :: video_sw is not enough => 0x%x < 0x%x \n", __func__, __LINE__, pmap_video_sw.size, VPU_SW_ACCESS_REGION_SIZE);
		ret = -1;
		goto Error;
	}
	ptr_sw_addr_mem = pmap_video_sw.base;
	sz_sw_mem = pmap_video_sw.size;

	ptr_rear_addr_mem = ptr_front_addr_mem + sz_remained_mem;

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	{
		if(0 > pmap_get_info("enc_main", &pmap_enc)){
			ret = -12;
			goto Error;
		}
		sz_enc_mem = pmap_enc.size;
		printk("%s :: enc_main 0x%x \n", __func__, sz_enc_mem);
	}
	only_decmode = 0;
#else
	sz_enc_mem = 0;
	only_decmode = 1;
#endif

	if( sz_remained_mem < sz_enc_mem){
		ret = -2;
		goto Error;
	}
	ptr_ext_addr_mem = ptr_rear_addr_mem - sz_enc_mem;

	if( ptr_front_addr_mem < pmap_video.base || (ptr_front_addr_mem > (pmap_video.base + pmap_video.size))){
		ret = -3;
		goto Error;
	}

	if( ptr_rear_addr_mem < pmap_video.base || (ptr_rear_addr_mem > (pmap_video.base + pmap_video.size))){
		ret = -4;
		goto Error;
	}

	sz_front_used_mem = sz_rear_used_mem = sz_ext_used_mem = 0;
	dprintk_mem("%s :: 0x%x / 0x%x / 0x%x !! \n", __func__, ptr_front_addr_mem, ptr_ext_addr_mem, ptr_rear_addr_mem);

#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
// Additional decoder :: ext2,3
	if(0 > pmap_get_info("video_ext", &pmap_video_ext)){
		ret = -13;
		goto Error;
	}

	ptr_ext_front_addr_mem = (phys_addr_t)pmap_video_ext.base;
	sz_ext_remained_mem = pmap_video_ext.size;
#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	ptr_ext_rear_addr_mem = ptr_ext_front_addr_mem + sz_ext_remained_mem - (PAGE_ALIGN(PS_SAVE_SIZE) + PAGE_ALIGN(SLICE_SAVE_SIZE) + PAGE_ALIGN(LARGE_STREAM_BUF_SIZE));
	sz_ext_remained_mem -= (PAGE_ALIGN(PS_SAVE_SIZE) + PAGE_ALIGN(SLICE_SAVE_SIZE) + PAGE_ALIGN(LARGE_STREAM_BUF_SIZE));
#else
	ptr_ext_rear_addr_mem = ptr_ext_front_addr_mem + sz_ext_remained_mem;
#endif
	sz_ext_front_used_mem = sz_ext_rear_used_mem = 0;

	if( ptr_ext_front_addr_mem < pmap_video_ext.base || (ptr_ext_front_addr_mem > (pmap_video_ext.base + pmap_video_ext.size))){
		ret = -5;
		goto Error;
	}
#endif

#if defined(CONFIG_VDEC_CNT_5)
// Additional decoder :: ext4
	if(0 > pmap_get_info("video_ext2", &pmap_video_ext2)){
		ret = -14;
		goto Error;
	}

	ptr_ext2_front_addr_mem = (phys_addr_t)pmap_video_ext2.base;
	sz_ext2_remained_mem = pmap_video_ext2.size;
	sz_ext2_front_used_mem = 0;

	if( ptr_ext2_front_addr_mem < pmap_video_ext2.base || (ptr_ext2_front_addr_mem > (pmap_video_ext2.base + pmap_video_ext2.size))){
		ret = -6;
		goto Error;
	}
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
// Additional encoder :: ext, ext2, ext3
	if(0 > pmap_get_info("enc_ext", &pmap_enc_ext[0])){
		ret = -15;
		goto Error;
	}
	ptr_enc_ext_addr_mem[0] = (phys_addr_t)pmap_enc_ext[0].base;
	sz_enc_ext_remained_mem[0] = pmap_enc_ext[0].size;
	sz_enc_ext_used_mem[0] = 0;
#endif
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if(0 > pmap_get_info("enc_ext2", &pmap_enc_ext[1])){
		ret = -16;
		goto Error;
	}
	ptr_enc_ext_addr_mem[1] = (phys_addr_t)pmap_enc_ext[1].base;
	sz_enc_ext_remained_mem[1] = pmap_enc_ext[1].size;
	sz_enc_ext_used_mem[1] = 0;
#endif
#if defined(CONFIG_VENC_CNT_4)
	if(0 > pmap_get_info("enc_ext3", &pmap_enc_ext[2])){
		ret = -17;
		goto Error;
	}
	ptr_enc_ext_addr_mem[2] = (phys_addr_t)pmap_enc_ext[2].base;
	sz_enc_ext_remained_mem[2] = pmap_enc_ext[2].size;
	sz_enc_ext_used_mem[2] = 0;
#endif

	memset(vmem_allocated_count, 0x00, sizeof(vmem_allocated_count));
	memset(vmem_alloc_info, 0x00, sizeof(vmem_alloc_info));
	memset(vdec_used, 0x00, sizeof(vdec_used));
	memset(venc_used, 0x00, sizeof(venc_used));

	return 0;

Error:

	return ret;
}

int _vmem_deinit_memory_info(void)
{
	dprintk_mem("%s \n", __func__);

	if(pmap_video.base){
		pmap_release_info("video");		// pmap_video
		pmap_video.base = 0;
	}
	if(pmap_video_sw.base){
		pmap_release_info("video_sw");	// pmap_video_sw
		pmap_video_sw.base = 0;
	}

#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if(pmap_enc.base){
		pmap_release_info("enc_main");	// pmap_enc
		pmap_enc.base = 0;
	}
#endif

#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
	if(pmap_video_ext.base){
		pmap_release_info("video_ext");	// pmap_video_ext
		pmap_video_ext.base = 0;
	}
#endif

#if defined(CONFIG_VDEC_CNT_5)
	if(pmap_video_ext2.base){
		pmap_release_info("video_ext2");// pmap_video_ext2
		pmap_video_ext2.base = 0;
	}
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if(pmap_enc_ext[0].base){
		pmap_release_info("enc_ext");	// pmap_enc_ext[0]
		pmap_enc_ext[0].base = 0;
	}
#endif
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if(pmap_enc_ext[1].base){
		pmap_release_info("enc_ext2");	// pmap_enc_ext[1]
		pmap_enc_ext[1].base = 0;
	}
#endif
#if defined(CONFIG_VENC_CNT_4)
	if(pmap_enc_ext[2].base){
		pmap_release_info("enc_ext3");	// pmap_enc_ext[2]
		pmap_enc_ext[2].base = 0;
	}
#endif
	return 0;
}

int _vmem_is_cma_allocated_virt_region(unsigned int start_virtaddr, unsigned int length)
{
	int type = 0, i = 0;
	void *cma_virt_address = NULL;
	unsigned int end_virtaddr = start_virtaddr + length -1;

	for(type=0; type < VPU_MAX ; type++)
	{
		if(vmem_allocated_count[type] > 0)
		{
			for(i=vmem_allocated_count[type]; i>0; i--)
			{
				if(vmem_alloc_info[type][i-1].kernel_remap_addr != 0)
				{
					if( (start_virtaddr >= vmem_alloc_info[type][i-1].kernel_remap_addr) 
						&& (end_virtaddr <= (vmem_alloc_info[type][i-1].kernel_remap_addr+vmem_alloc_info[type][i-1].request_size-1)))
					{
						if(_vmem_is_cma_allocated_phy_region(vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size))
							return 1;
						else
							return 0;
					}
				}
			}
		}
	}

	return 0;
}

int _vmem_is_cma_allocated_phy_region(unsigned int start_phyaddr, unsigned int length)
{
	unsigned int end_phyaddr = start_phyaddr + length -1;

// pmap_video
	if( (start_phyaddr >= pmap_video.base) && (end_phyaddr <= (pmap_video.base+pmap_video.size-1)))
		return pmap_is_cma_alloc(&pmap_video);

// pmap_video_sw
	if( (start_phyaddr >= pmap_video_sw.base) && (end_phyaddr <= (pmap_video_sw.base+pmap_video_sw.size-1)))
		return pmap_is_cma_alloc(&pmap_video_sw);

// pmap_enc
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if( (start_phyaddr >= pmap_enc.base) && (end_phyaddr <= (pmap_enc.base+pmap_enc.size-1)))
		return pmap_is_cma_alloc(&pmap_enc);
#endif

// pmap_video_ext
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
	if( (start_phyaddr >= pmap_video_ext.base) && (end_phyaddr <= (pmap_video_ext.base+pmap_video_ext.size-1)))
		return pmap_is_cma_alloc(&pmap_video_ext);
#endif

// pmap_video_ext2
#if defined(CONFIG_VDEC_CNT_5)
	if( (start_phyaddr >= pmap_video_ext2.base) && (end_phyaddr <= (pmap_video_ext2.base+pmap_video_ext2.size-1)))
		return pmap_is_cma_alloc(&pmap_video_ext2);
#endif

// pmap_enc_ext[0]
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if( (start_phyaddr >= pmap_enc_ext[0].base) && (end_phyaddr <= (pmap_enc_ext[0].base+pmap_enc_ext[0].size-1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[0]);
#endif

//pmap_enc_ext[1]
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	if( (start_phyaddr >= pmap_enc_ext[1].base) && (end_phyaddr <= (pmap_enc_ext[1].base+pmap_enc_ext[1].size-1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[1]);
#endif

// pmap_enc_ext[2]
#if defined(CONFIG_VENC_CNT_4)
	if( (start_phyaddr >= pmap_enc_ext[2].base) && (end_phyaddr <= (pmap_enc_ext[2].base+pmap_enc_ext[2].size-1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[2]);
#endif

	return NULL;
}

int vmem_proc_alloc_memory(int codec_type, MEM_ALLOC_INFO_t *alloc_info, vputype type)
{
	mutex_lock(&mem_mutex);
	dprintk("type[%d]-buffer[%d] : vmgr_proc_alloc_memory!! \n", type, alloc_info->buffer_type);

/******************************** START :: Dedicated memory region   **************************************/
#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
  #ifdef FIXED_PS_SLICE_BUFFER
	if( alloc_info->buffer_type == BUFFER_PS )
	{
		if( (alloc_info->phy_addr = _vmem_request_ps_buff_phyaddr( type, &(alloc_info->kernel_remap_addr), &(alloc_info->request_size)) ) != 0 )
		{
			goto Success;
		}
	}
	else if( alloc_info->buffer_type == BUFFER_SLICE )
	{
		if( (alloc_info->phy_addr = _vmem_request_slice_buff_phyaddr( type, &(alloc_info->kernel_remap_addr), &(alloc_info->request_size)) ) != 0 )
		{
			goto Success;
		}
	}
	else
  #endif
  #ifdef FIXED_STREAM_BUFFER
	if( alloc_info->buffer_type == BUFFER_STREAM )
	{
		if( (alloc_info->phy_addr = _vmem_request_stream_buff_phyaddr( type, &(alloc_info->kernel_remap_addr), &(alloc_info->request_size)) ) != 0 )
		{
			goto Success;
		}
	}
  #endif
#endif
	if( alloc_info->buffer_type == BUFFER_WORK )
	{
		if( (alloc_info->phy_addr = _vmem_request_workbuff_phyaddr( codec_type, &(alloc_info->kernel_remap_addr)) ) == 0 )
		{
			err("type[%d]-buffer[%d] : [0x%x] BUFFER_WORK ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
			goto Error;
		}
	}
	else if( alloc_info->buffer_type == BUFFER_USERDATA )
	{
		if( (alloc_info->phy_addr = _vmem_request_userdata_buff_phyaddr( type, &(alloc_info->kernel_remap_addr), &(alloc_info->request_size) )) == 0 )
		{
			err("type[%d]-buffer[%d] : [0x%x] BUFFER_USERDATA ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
			goto Error;
		}
	}
	else if( alloc_info->buffer_type == BUFFER_SEQHEADER &&
		(type == VPU_ENC || type == VPU_ENC_EXT || type == VPU_ENC_EXT2 || type == VPU_ENC_EXT3) ) //Encoder
	{
		if( (alloc_info->phy_addr = _vmem_request_seqheader_buff_phyaddr( type, &(alloc_info->kernel_remap_addr), &(alloc_info->request_size) )) == 0 )
		{
			err("type[%d]-buffer[%d] : [0x%x] BUFFER_SEQHEADER ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
			goto Error;
		}
	}
/******************************** END :: Dedicated memory region   **************************************/
	else
	{
#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
		if( (alloc_info->phy_addr = _vmem_request_phyaddr( alloc_info->request_size, type)) == 0 )
		{
			err("type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
			goto Error;
		}
#else
		if( type == VPU_DEC_EXT4 )
		{
#if defined(CONFIG_VDEC_CNT_5)
			if( (alloc_info->phy_addr = _vmem_request_phyaddr_dec_ext4( alloc_info->request_size, type)) == 0 )
			{
				err("type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
				goto Error;
			}
#endif
		}
		else if( type == VPU_DEC_EXT2 || type == VPU_DEC_EXT3 )
		{
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
			if( (alloc_info->phy_addr = _vmem_request_phyaddr_dec_ext23( alloc_info->request_size, type)) == 0 )
			{
				err("type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
				goto Error;
			}
#endif
		}
		else if( type == VPU_ENC_EXT || type == VPU_ENC_EXT2 || type == VPU_ENC_EXT3 )
		{
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
			if( (alloc_info->phy_addr = _vmem_request_phyaddr_enc_ext( alloc_info->request_size, type)) == 0 )
			{
				err("type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
				goto Error;
			}
#endif
		}
		else
		{
			if( (alloc_info->phy_addr = _vmem_request_phyaddr( alloc_info->request_size, type)) == 0 )
			{
				err("type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr );
				goto Error;
			}
		}
#endif
		alloc_info->kernel_remap_addr = 0x0;
		if( alloc_info->buffer_type != BUFFER_FRAMEBUFFER )
		{
			alloc_info->kernel_remap_addr = _vmem_get_virtaddr(alloc_info->phy_addr, alloc_info->request_size);
			if (alloc_info->kernel_remap_addr == 0) {
				err("type[%d]-buffer[%d] : phy[0x%x - 0x%x] remap ALLOC_MEMORY failed.\n", type, alloc_info->buffer_type, alloc_info->phy_addr, PAGE_ALIGN(alloc_info->request_size/*-PAGE_SIZE*/) );
				memcpy(&(vmem_alloc_info[type][vmem_allocated_count[type]]), alloc_info, sizeof(MEM_ALLOC_INFO_t));
				vmem_allocated_count[type] += 1;
				goto Error;
			}
		}

		dprintk_mem("type[%d]-buffer[%d] : alloc addr[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", type, alloc_info->buffer_type, vmem_allocated_count[type], alloc_info->phy_addr, alloc_info->kernel_remap_addr, alloc_info->request_size);
		memcpy(&(vmem_alloc_info[type][vmem_allocated_count[type]]), alloc_info, sizeof(MEM_ALLOC_INFO_t));
		vmem_allocated_count[type] += 1;
	}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
Success:
#endif
	if( alloc_info->buffer_type == BUFFER_STREAM )
		_vmem_set_page_type(alloc_info->phy_addr, 3);

	mutex_unlock(&mem_mutex);
	return 0;

Error:
	mutex_unlock(&mem_mutex);
	return -EFAULT;
}

int vmem_proc_free_memory(vputype type)
{
	mutex_lock(&mem_mutex);
	{
		int i;

		dprintk("type[%d] : vmgr_proc_free_memory!! \n", type);

		for(i=vmem_allocated_count[type]; i>0; i--)
		{
			//if(vmem_alloc_info[type][i-1].kernel_remap_addr != 0)
			{
				if( vmem_alloc_info[type][i-1].kernel_remap_addr != 0 )
					_vmem_release_virtaddr(((void*)vmem_alloc_info[type][i-1].kernel_remap_addr), vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size);
#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
				_vmem_release_phyaddr(vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size, type);
#else
				if(type == VPU_DEC_EXT4){
	#if defined(CONFIG_VDEC_CNT_5)
					_vmem_release_phyaddr_dec_ext4(vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size, type);
	#endif
				}
				else if(type == VPU_DEC_EXT2 || type == VPU_DEC_EXT3){
	#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
					_vmem_release_phyaddr_dec_ext23(vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size, type);
	#endif
				}
				else if( type == VPU_ENC_EXT || type == VPU_ENC_EXT2 || type == VPU_ENC_EXT3 ){
	#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
					_vmem_release_phyaddr_enc_ext(vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size, type);
	#endif
				}
				else{
					_vmem_release_phyaddr(vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].request_size, type);
				}
#endif
				dprintk_mem("type[%d]-buffer[%d] : free addr[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !! \n", type, vmem_alloc_info[type][i-1].buffer_type, i-1, vmem_alloc_info[type][i-1].phy_addr, vmem_alloc_info[type][i-1].kernel_remap_addr, vmem_alloc_info[type][i-1].request_size);
			}
		}

		vmem_allocated_count[type] = 0;
		memset(vmem_alloc_info[type], 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
	mutex_unlock(&mem_mutex);

	return 0;
}

unsigned int vmem_get_free_memory(vputype type)
{
	unsigned int szFreeMem = 0;

	if( type == VPU_DEC )
	{
		if( vmgr_get_close(VPU_DEC_EXT) && vmgr_get_close(VPU_ENC)
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
			&& vmgr_4k_d2_get_close(VPU_DEC_EXT) && vmgr_4k_d2_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
			&& hmgr_get_close(VPU_DEC_EXT) && hmgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
			&& vp9mgr_get_close(VPU_DEC_EXT) && vp9mgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
			&& jmgr_get_close(VPU_DEC_EXT) && jmgr_get_close(VPU_ENC)
#endif
		)
		{
			szFreeMem = sz_remained_mem;
		}
		else
		{
			if(only_decmode)
				szFreeMem = sz_remained_mem;
			else
				szFreeMem = (pmap_video.size - sz_enc_mem - VPU_SW_ACCESS_REGION_SIZE) - (sz_ext_used_mem + sz_front_used_mem);
		}
	}
	else if( type == VPU_DEC_EXT )
	{
		if(only_decmode)
			szFreeMem = sz_remained_mem;
		else
			szFreeMem = (pmap_video.size - sz_enc_mem - VPU_SW_ACCESS_REGION_SIZE) - (sz_ext_used_mem + sz_front_used_mem);
	}
	else if( type == VPU_ENC )
	{
		szFreeMem = sz_enc_mem-sz_rear_used_mem;
	}
	else if( type == VPU_DEC_EXT2 || type == VPU_DEC_EXT3 )
	{
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
		szFreeMem = sz_ext_remained_mem;
#else
		szFreeMem = 0;
#endif
	}
	else if( type == VPU_DEC_EXT4 )
	{
#if defined(CONFIG_VDEC_CNT_5)
		szFreeMem = sz_ext2_remained_mem;
#else
		szFreeMem = 0;
#endif
	}
	else if( type == VPU_ENC_EXT || type == VPU_ENC_EXT2 || type == VPU_ENC_EXT3 )
	{
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
		szFreeMem = sz_enc_ext_remained_mem[type-VPU_ENC_EXT];
#else
		szFreeMem = 0;
#endif
	}
	else
	{
		szFreeMem = sz_remained_mem;
	}

	dprintk_mem("%s :: type(%d) free(0x%x) :: etc_info = enc(0x%x), front_used(0x%x), ext_used(0x%x), rear_used(0x%x)\n", __func__, type, szFreeMem, sz_enc_mem, sz_front_used_mem, sz_ext_used_mem, sz_rear_used_mem); 
	return szFreeMem;
}
EXPORT_SYMBOL(vmem_get_free_memory);

unsigned int vmem_get_freemem_size(vputype type)
{
	if( type <= VPU_DEC_EXT4 )
	{
		dprintk("type[%d] mem info for vpu :: remain(0x%x : 0x%x : 0x%x), used mem(0x%x/0x%x, 0x%x : 0x%x/0x%x : 0x%x) \n",
					type,
					sz_remained_mem,
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
					sz_ext_remained_mem,
#else
					0,
#endif
#if defined(CONFIG_VDEC_CNT_5)
					sz_ext2_remained_mem,
#else
					0,
#endif
					sz_front_used_mem, sz_ext_used_mem, sz_rear_used_mem,
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
					sz_ext_front_used_mem, sz_ext_rear_used_mem
#else
					0, 0
#endif
#if defined(CONFIG_VDEC_CNT_5)
					,sz_ext2_front_used_mem
#else
					,0
#endif
		);
	}
	else if( VPU_ENC <= type && type < VPU_MAX )
	{
		printk("type[%d] mem info for vpu :: remain(0x%x : 0x%x : 0x%x : 0x%x), used mem(0x%x : 0x%x : 0x%x : 0x%x) \n",
					type,
					sz_enc_mem - sz_rear_used_mem,
#if defined(CONFIG_VENC_CNT_4)
					sz_enc_ext_remained_mem[0],	sz_enc_ext_remained_mem[1], sz_enc_ext_remained_mem[2],
#elif defined(CONFIG_VENC_CNT_3)
					sz_enc_ext_remained_mem[0],	sz_enc_ext_remained_mem[1],	0,
#elif defined(CONFIG_VENC_CNT_2)
					sz_enc_ext_remained_mem[0],	0, 0,
#else
					0, 0, 0,
#endif
					sz_rear_used_mem,
#if defined(CONFIG_VENC_CNT_4)
					sz_enc_ext_used_mem[0],	sz_enc_ext_used_mem[1],	sz_enc_ext_used_mem[2]
#elif defined(CONFIG_VENC_CNT_3)
					sz_enc_ext_used_mem[0],	sz_enc_ext_used_mem[1],	0
#elif defined(CONFIG_VENC_CNT_2)
					sz_enc_ext_used_mem[0],	0, 0
#else
					0, 0, 0
#endif
		);
	}
	else{
		err("mem_info :: unKnown type(%d)\n", type);
		return 0;
	}

	return vmem_get_free_memory(type);
}

void _vmem_config_zero(void)
{
	dprintk_mem("%s \n", __func__);

	pmap_video.base = 0;
	pmap_video_sw.base = 0;
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	pmap_enc.base = 0;
#endif
#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
	pmap_video_ext.base = 0;
#endif
#if defined(CONFIG_VDEC_CNT_5)
	pmap_video_ext2.base = 0;
#endif
#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	pmap_enc_ext[0].base = 0;
#endif
#if defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	pmap_enc_ext[1].base = 0;
#endif
#if defined(CONFIG_VENC_CNT_4)
	pmap_enc_ext[2].base = 0;
#endif

	sz_front_used_mem = sz_rear_used_mem = sz_ext_used_mem = 0;
	sz_remained_mem = sz_enc_mem = 0;

#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
	sz_ext_front_used_mem = sz_ext_rear_used_mem = 0;
	sz_ext_remained_mem = 0;
#endif
#if defined(CONFIG_VDEC_CNT_5)
	sz_ext2_front_used_mem = 0;
	sz_ext2_remained_mem = 0;
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	sz_enc_ext_used_mem[0] = sz_enc_ext_used_mem[1] = sz_enc_ext_used_mem[2] = 0;
	sz_enc_ext_remained_mem[0] = sz_enc_ext_remained_mem[1] = sz_enc_ext_remained_mem[2] = 0;
#endif

//////////////////////////////////////
// Memory Management!!
	memset(&gsVpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#ifdef CONFIG_SUPPORT_TCC_JPU
	memset(&gsJpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
	memset(&gsVpu4KD2Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC // HEVC
	memset(&gsWave410_Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
	memset(&gsG2V2_Vp9Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
	memset(&gsVpuEncSeqheader_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t)*VPU_ENC_MAX_CNT);
#endif
	memset(&gsVpuUserData_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t)*VPU_INST_MAX);
#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	memset(&gsPs_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t)*VPU_INST_MAX);
	memset(&gsSlice_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t)*VPU_INST_MAX);
	memset(&gsStream_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t)*VPU_INST_MAX);
#endif
}

int vmem_init(void)
{
	mutex_lock(&mem_mutex);
	{
		int ret = 0;

		if((vmgr_opened() == 0)
#ifdef CONFIG_SUPPORT_TCC_JPU
			&& (jmgr_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2 // HEVC/VP9
			&& (vmgr_4k_d2_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC // HEVC
			&& (hmgr_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9 // VP9
			&& (vp9mgr_opened() == 0)
#endif
		){
			if(!cntMem_Reference)
			{
				dprintk("_vmem_init_memory_info (work_total : 0x%x) \n", VPU_SW_ACCESS_REGION_SIZE);
				if(0 > (ret = _vmem_init_memory_info())){
					err("%s :: _vmem_init_memory_info() is failure with %d error \n", __func__, ret);
					goto Error1;
				}
				if(0 > (ret = _vmem_alloc_dedicated_buffer())){
					err("%s :: _vmem_alloc_dedicated_buffer() is failure with %d error \n", __func__, ret);
					goto Error0;
				}
			}
			goto Success;
		}

Error0:
		if(ret < 0)
			_vmem_free_dedicated_buffer();
Error1:
		if(ret < 0)
			_vmem_deinit_memory_info();

		if(ret < 0)
		{
			sz_front_used_mem = sz_rear_used_mem = sz_ext_used_mem = 0;
			sz_remained_mem = sz_enc_mem = 0;

#if defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || defined(CONFIG_VDEC_CNT_5)
			sz_ext_front_used_mem = sz_ext_rear_used_mem = 0;
			sz_ext_remained_mem = 0;
#endif
#if defined(CONFIG_VDEC_CNT_5)
			sz_ext2_front_used_mem = 0;
			sz_ext2_remained_mem = 0;
#endif

#if defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
			sz_enc_ext_used_mem[0] = sz_enc_ext_used_mem[1] = sz_enc_ext_used_mem[2] = 0;
			sz_enc_ext_remained_mem[0] = sz_enc_ext_remained_mem[1] = sz_enc_ext_remained_mem[2] = 0;
#endif
		}

Success:
		if(ret >= 0)
		{
			cntMem_Reference++;
		}

		mutex_unlock(&mem_mutex);
		return ret;
	}

	return 0;
}

int vmem_config(void)
{
	int ret = 0;
	mutex_init(&mem_mutex);
	_vmem_config_zero();

	return ret;
//////////////////////////////////////
}

void vmem_deinit(void)
{
	mutex_lock(&mem_mutex);
	dprintk_mem("%s :: ref count: %d \n", __func__, cntMem_Reference);
	if(cntMem_Reference > 0)
		cntMem_Reference--;
	else
		printk("%s :: strange ref-count :: %d\n", __func__, cntMem_Reference);

	if(!cntMem_Reference){
		_vmem_free_dedicated_buffer();
		_vmem_deinit_memory_info();
		_vmem_config_zero();
		printk("%s :: all memory for VPU were released. ref-count :: %d\n", __func__, cntMem_Reference);
	}
	mutex_unlock(&mem_mutex);
}

void vmem_set_only_decode_mode(int bDec_only)
{
	mutex_lock(&mem_mutex);
	{
		if( vmem_allocated_count[VPU_DEC_EXT] == 0 && vmem_allocated_count[VPU_ENC] == 0){
#if defined(CONFIG_VENC_CNT_1) || defined(CONFIG_VENC_CNT_2) || defined(CONFIG_VENC_CNT_3) || defined(CONFIG_VENC_CNT_4)
			only_decmode = bDec_only;
#else
			only_decmode = 1;
#endif
			dprintk("Changed alloc_mode(%d) \n", only_decmode);
		}
		else{
			dprintk("can't change mode. (%d/%d) \n", vmem_allocated_count[VPU_DEC_EXT], vmem_allocated_count[VPU_ENC]);
		}
	}
	mutex_unlock(&mem_mutex);
}

void vdec_get_instance(int *nIdx)
{
	mutex_lock(&mem_mutex);
	{
		int i, nInstance = -1;

		if( *nIdx >= 0 && *nIdx < VPU_INST_MAX ) // check instance that they want.
		{
			if( vdec_used[*nIdx] == 0 && (vmem_get_free_memory(*nIdx+VPU_DEC) > 0) )
				nInstance = *nIdx;
		}

		if( nInstance < 0 )
		{
			for(i=0; i<VPU_INST_MAX; i++) {
				if(vdec_used[i] == 0){
					nInstance = i;
					break;
				}
			}
		}

		if(nInstance >= 0)
			vdec_used[nInstance] = 1;
		else
			err("failed to get new instance for decoder(%d) \n", nInstance);

		*nIdx = nInstance;
	}
	mutex_unlock(&mem_mutex);
}

void vdec_check_instance_available(unsigned int *nAvailable_Inst)
{
	mutex_lock(&mem_mutex);
	{
		int nAvailable_Instance = 0;
		int i = 0;

		for(i=0; i<VPU_INST_MAX; i++) {
			if(vdec_used[i] == 0 && (vmem_get_free_memory(i+VPU_DEC) > 0)){
				nAvailable_Instance++;
			}
		}
		*nAvailable_Inst = nAvailable_Instance;
	}
	mutex_unlock(&mem_mutex);
}

void vdec_clear_instance(int nIdx)
{
	mutex_lock(&mem_mutex);
	{
		if(nIdx >= 0)
			vdec_used[nIdx] = 0;
		else
			err("failed to clear instance for decoder(%d) \n", nIdx);

	}
	mutex_unlock(&mem_mutex);
}
EXPORT_SYMBOL(vdec_clear_instance);

void venc_get_instance(int *nIdx)
{
	mutex_lock(&mem_mutex);
	{
		int i, nInstance = -1;

		if( *nIdx >= 0 && *nIdx < VPU_INST_MAX ) // check instance that they want.
		{
			if( venc_used[*nIdx] == 0 && (vmem_get_free_memory(*nIdx+VPU_ENC) > 0 ) )
				nInstance = *nIdx;
		}

		if( nInstance < 0 )
		{
			for(i=0; i<VPU_INST_MAX; i++) {
				if(venc_used[i] == 0){
					nInstance = i;
					break;
				}
			}
		}

		if(nInstance >= 0)
			venc_used[nInstance] = 1;
		else
			err("failed to get new instance for encoder(%d) \n", nInstance);

		*nIdx = nInstance;
	}
	mutex_unlock(&mem_mutex);
}

void venc_check_instance_available(unsigned int *nAvailable_Inst)
{
	mutex_lock(&mem_mutex);
	{
		int nAvailable_Instance = 0;
		int i = 0;

		for(i=0; i<VPU_INST_MAX; i++) {
			if(venc_used[i] == 0 && (vmem_get_free_memory(i+VPU_ENC) > 0)){
				nAvailable_Instance++;
			}
		}
		*nAvailable_Inst = nAvailable_Instance;
	}
	mutex_unlock(&mem_mutex);
}


void venc_clear_instance(int nIdx)
{
	mutex_lock(&mem_mutex);
	{
		if(nIdx >= 0)
			venc_used[nIdx] = 0;
		else
			err("failed to clear instance for encoder(%d) \n", nIdx);

	}
	mutex_unlock(&mem_mutex);
}
EXPORT_SYMBOL(venc_clear_instance);

