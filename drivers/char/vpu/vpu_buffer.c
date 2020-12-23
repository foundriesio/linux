// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
#include "vpu_4k_d2_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include "hevc_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include "vp9_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
#include "vpu_hevc_enc_mgr.h"
#endif

#include <soc/tcc/pmap.h>

#include <video/tcc/tcc_vpu_wbuffer.h>

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
// case(1-2-3 : on/on - off/on - on/off)
//#define FIXED_STREAM_BUFFER
#define FIXED_PS_SLICE_BUFFER

static MEM_ALLOC_INFO_t gsPs_memInfo[VPU_INST_MAX];
static MEM_ALLOC_INFO_t gsSlice_memInfo[VPU_INST_MAX];
static MEM_ALLOC_INFO_t gsStream_memInfo[VPU_INST_MAX];
#endif

// Memory Management!!
static MEM_ALLOC_INFO_t gsVpuWork_memInfo;

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
static MEM_ALLOC_INFO_t gsVpu4KD2Work_memInfo;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
static MEM_ALLOC_INFO_t gsWave410_Work_memInfo;
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
static MEM_ALLOC_INFO_t gsG2V2_Vp9Work_memInfo;
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
static MEM_ALLOC_INFO_t gsJpuWork_memInfo;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
static MEM_ALLOC_INFO_t gsVpuHevcEncWork_memInfo;
#endif

#if DEFINED_CONFIG_VENC_CNT_12345678
static MEM_ALLOC_INFO_t gsVpuEncSeqheader_memInfo[VPU_ENC_MAX_CNT];
#endif

static MEM_ALLOC_INFO_t gsVpuUserData_memInfo[VPU_INST_MAX];

// Regard only the operation of 2 component!!
static struct pmap pmap_video, pmap_video_sw;
static phys_addr_t ptr_sw_addr_mem;
static unsigned int sz_sw_mem;
static phys_addr_t ptr_front_addr_mem, ptr_rear_addr_mem, ptr_ext_addr_mem;
static unsigned int sz_front_used_mem, sz_rear_used_mem, sz_ext_used_mem;
static unsigned int sz_remained_mem, sz_enc_mem;

#if DEFINED_CONFIG_VDEC_CNT_345
static struct pmap pmap_video_ext;
static phys_addr_t ptr_ext_front_addr_mem, ptr_ext_rear_addr_mem;
static unsigned int sz_ext_front_used_mem, sz_ext_rear_used_mem;
static unsigned int sz_ext_remained_mem;
#endif

#if defined(CONFIG_VDEC_CNT_5)
static struct pmap pmap_video_ext2;
static phys_addr_t ptr_ext2_front_addr_mem;
static unsigned int sz_ext2_front_used_mem;
static unsigned int sz_ext2_remained_mem;
#endif

#if DEFINED_CONFIG_VENC_CNT_12345678
static struct pmap pmap_enc;
#endif

#if DEFINED_CONFIG_VENC_CNT_2345678
static struct pmap pmap_enc_ext[7];
static phys_addr_t ptr_enc_ext_addr_mem[7];
static unsigned int sz_enc_ext_used_mem[7];
static unsigned int sz_enc_ext_remained_mem[7];
#endif

static int vmem_allocated_count[VPU_MAX] = { 0, };

static MEM_ALLOC_INFO_t vmem_alloc_info[VPU_MAX][20];
struct mutex mem_mutex;
atomic_t cntMem_Reference;
static int only_decmode;	// = 0;

static int vdec_used[VPU_INST_MAX] = { 0, };
static int venc_used[VPU_INST_MAX] = { 0, };

int vmem_alloc_count(int type)
{
	if (type >= VPU_MAX)
		return 0;
	return vmem_allocated_count[type];
}

// Page type configuration
#define PAGE_TYPE_MAX	16
static int g_iMmapPropCnt;	// = 0;
static int g_aiProperty[PAGE_TYPE_MAX];
static unsigned long g_aulPageOffset[PAGE_TYPE_MAX];

static char gMemConfigDone;	// = 0;

static void _vmem_set_page_type(phys_addr_t uiAddress, int iProperty)
{
	if (iProperty != 0) {
		int idx = 0;

		while (idx < PAGE_TYPE_MAX) {
			if (g_aiProperty[idx] == 0)
				break;
			idx++;
		}

		if (idx < PAGE_TYPE_MAX) {
			g_aiProperty[idx] = iProperty;
			g_aulPageOffset[idx] = uiAddress / PAGE_SIZE;
			g_iMmapPropCnt++;

			V_DBG(VPU_DBG_MEM_SEQ, "0x%08X, %d", uiAddress,
				iProperty);
		} else {
			V_DBG(VPU_DBG_MEM_SEQ, " (0x%08X, %d) - FAILED",
				uiAddress, iProperty);
		}
	}
}

static int _vmem_get_page_type(unsigned long ulPageOffset)
{
	int idx = 0;
	int prop = 0;

	while (idx < PAGE_TYPE_MAX) {
		if (g_aulPageOffset[idx] == ulPageOffset)
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

	if (g_iMmapPropCnt > 0) {
		int prop = _vmem_get_page_type(ulPageOffset);

		switch (prop) {
		case 3:
			{
				V_DBG(VPU_DBG_MEM_SEQ,
					"_vmem_get_pgprot (write-combine)");
				newProt = pgprot_writecombine(ulOldProt);
			}
			break;

		case 0:
		case 1:
		case 2:
		case 4:
		case 5:
		case 6:
		default:
			{
				V_DBG(VPU_DBG_MEM_SEQ,
				      "_vmem_get_pgprot (write-combine)");
				newProt = pgprot_writecombine(ulOldProt);
			}
			break;
		}
	} else {
		V_DBG(VPU_DBG_MEM_SEQ,
		      "_vmem_get_pgprot (write-combine)");
		newProt = pgprot_writecombine(ulOldProt);
	}

	mutex_unlock(&mem_mutex);
	return newProt;
}
EXPORT_SYMBOL(vmem_get_pgprot);

static void *_vmem_get_virtaddr(unsigned int start_phyaddr, unsigned int length)
{
	void *cma_virt_address = NULL;

	V_DBG(VPU_DBG_MEMORY, "physical region [0x%x - 0x%x]!!", start_phyaddr,
		start_phyaddr + length);

	if (_vmem_is_cma_allocated_phy_region(start_phyaddr, length))
		cma_virt_address =
			pmap_cma_remap(start_phyaddr, PAGE_ALIGN(length));
	else
		cma_virt_address =
			(void *)ioremap_nocache((phys_addr_t) start_phyaddr,
					PAGE_ALIGN(length));

	if (cma_virt_address == NULL) {
		V_DBG(VPU_DBG_ERROR, "error ioremap for 0x%x / 0x%x(%u)",
			start_phyaddr, length, length);
	}

	return cma_virt_address;
}

static void _vmem_release_virtaddr(void *target_virtaddr,
				unsigned int start_phyaddr,
				unsigned int length)
{
	V_DBG(VPU_DBG_MEMORY, "physical region [0x%x - 0x%x]!!", start_phyaddr,
		start_phyaddr + length);

	if (_vmem_is_cma_allocated_phy_region(start_phyaddr, length))
		pmap_cma_unmap(target_virtaddr, PAGE_ALIGN(length));
	else
		iounmap((void *)target_virtaddr);
}

#if DEFINED_CONFIG_VDEC_CNT_345
static phys_addr_t _vmem_request_phyaddr_dec_ext23(unsigned int request_size,
							vputype type)
{
	phys_addr_t curr_phyaddr = 0;

	if (sz_ext_remained_mem < request_size) {
		V_DBG(VPU_DBG_ERROR,
			"type[%d] : insufficient memory : remain(0x%x(%u) ), request(0x%x(%u) )",
			type, sz_ext_remained_mem, sz_ext_remained_mem,
			request_size, request_size);
		return 0;
	}

	if (type == VPU_DEC_EXT2) {
		curr_phyaddr = ptr_ext_front_addr_mem;
		ptr_ext_front_addr_mem += request_size;
		sz_ext_front_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x",
			type, curr_phyaddr, ptr_ext_front_addr_mem,
			request_size, sz_ext_front_used_mem);
	} else {		/*VPU_DEC_EXT3 */

		ptr_ext_rear_addr_mem -= request_size;
		curr_phyaddr = ptr_ext_rear_addr_mem;
		sz_ext_rear_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x",
			type, curr_phyaddr,
			ptr_ext_rear_addr_mem + request_size,
			request_size, sz_ext_rear_used_mem);
	}

	sz_ext_remained_mem -= request_size;
	V_DBG(VPU_DBG_MEM_USAGE,
		"type[%d] : mem usage = 0x%x(0x%x)/0x%x = Dec_ext2/3(0x%x + 0x%x) !!",
		type, sz_ext_front_used_mem + sz_ext_rear_used_mem,
		sz_ext_remained_mem, pmap_video_ext.size, sz_ext_front_used_mem,
		sz_ext_rear_used_mem);

	return curr_phyaddr;
}

static int _vmem_release_phyaddr_dec_ext23(phys_addr_t phyaddr,
						unsigned int size, vputype type)
{
	//calc remained_size and ptr_addr_mem
	if (type == VPU_DEC_EXT2) {
		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_ext_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !!",
			type, ptr_ext_front_addr_mem,
			(ptr_ext_front_addr_mem - size), ptr_ext_front_addr_mem,
			size);

		ptr_ext_front_addr_mem -= size;
		sz_ext_front_used_mem -= size;

		if (ptr_ext_front_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: ptr_ext_front_addr_mem release-mem order!! 0x%x != 0x%x",
				type, ptr_ext_front_addr_mem, phyaddr);
		}
	} else {	/*VPU_DEC_EXT3 */

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_ext_rear_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x)!!",
			type, ptr_ext_rear_addr_mem,
			(ptr_ext_rear_addr_mem + size), ptr_ext_rear_addr_mem,
			size);

		if (ptr_ext_rear_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: ptr_ext_rear_addr_mem release-mem order!! 0x%x != 0x%x",
				type, ptr_ext_rear_addr_mem, phyaddr);
		}

		ptr_ext_rear_addr_mem += size;
		sz_ext_rear_used_mem -= size;
	}

	sz_ext_remained_mem += size;

	return 0;
}
#endif

#if defined(CONFIG_VDEC_CNT_5)
static phys_addr_t _vmem_request_phyaddr_dec_ext4(unsigned int request_size,
							vputype type)
{
	phys_addr_t curr_phyaddr = 0;

	if (sz_ext2_remained_mem < request_size) {
		V_DBG(VPU_DBG_ERROR,
			"type[%d] : insufficient memory : remain(0x%x), request(0x%x)",
			type, sz_ext2_remained_mem, request_size);
		return 0;
	}

	if (type == VPU_DEC_EXT4) {
		curr_phyaddr = ptr_ext2_front_addr_mem;
		ptr_ext2_front_addr_mem += request_size;
		sz_ext2_front_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x!!, used 0x%x",
			type, curr_phyaddr, ptr_ext2_front_addr_mem,
			request_size, sz_ext2_front_used_mem);
	} else {		/*VPU_DEC_EXT5 */

		//ToDo!!
		V_DBG(VPU_DBG_ERROR,
			"~~~~~~~~~~~~~ Not Implemented Function.!!!");
	}

	sz_ext2_remained_mem -= request_size;
	V_DBG(VPU_DBG_MEM_USAGE,
		"type[%d] : mem usage = 0x%x(0x%x)/0x%x = Dec_ext4(0x%x)!!",
		type, sz_ext2_front_used_mem, sz_ext2_remained_mem,
		pmap_video_ext2.size, sz_ext2_front_used_mem);

	return curr_phyaddr;
}

static int _vmem_release_phyaddr_dec_ext4(phys_addr_t phyaddr,
					  unsigned int size, vputype type)
{
	//calc remained_size and ptr_addr_mem
	if (type == VPU_DEC_EXT4) {
		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_ext2_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !!",
			type, ptr_ext2_front_addr_mem,
			(ptr_ext2_front_addr_mem - size),
			ptr_ext2_front_addr_mem,
			size);

		ptr_ext2_front_addr_mem -= size;
		sz_ext2_front_used_mem -= size;

		if (ptr_ext2_front_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: ptr_ext2_front_addr_mem release-mem order!! 0x%x != 0x%x",
				type, ptr_ext2_front_addr_mem, phyaddr);
		}
	} else {		/*VPU_DEC_EXT3 */

		//ToDo!!
		V_DBG(VPU_DBG_ERROR,
			"~~~~~~~~~~~~~ Not Implemented Function.!!!");
	}

	sz_ext2_remained_mem += size;

	return 0;
}
#endif

#if DEFINED_CONFIG_VENC_CNT_2345678
static phys_addr_t _vmem_request_phyaddr_enc_ext(unsigned int request_size,
						 vputype type)
{
	phys_addr_t curr_phyaddr = 0;
	unsigned int enc_idx = type - VPU_ENC_EXT;

	if (sz_enc_ext_remained_mem[enc_idx] < request_size) {
		V_DBG(VPU_DBG_ERROR,
			"type[%d] : insufficient memory : remain(0x%x), request(0x%x)",
			type, sz_enc_ext_remained_mem[enc_idx], request_size);
		return 0;
	}

	curr_phyaddr = ptr_enc_ext_addr_mem[enc_idx];
	ptr_enc_ext_addr_mem[enc_idx] += request_size;
	sz_enc_ext_used_mem[enc_idx] += request_size;
	sz_enc_ext_remained_mem[enc_idx] -= request_size;

	V_DBG(VPU_DBG_MEM_USAGE,
		"type[%d] : alloc : [%d] = 0x%x ~ 0x%x, 0x%x!!, used 0x%x/0x%x",
		type, enc_idx, curr_phyaddr, ptr_enc_ext_addr_mem[enc_idx],
		request_size, sz_enc_ext_used_mem[enc_idx],
		sz_enc_ext_remained_mem[enc_idx]);

	return curr_phyaddr;
}

static int _vmem_release_phyaddr_enc_ext(phys_addr_t phyaddr, unsigned int size,
					 vputype type)
{
	unsigned int enc_idx = type - VPU_ENC_EXT;

	V_DBG(VPU_DBG_MEM_USAGE,
		"type[%d] : release ptr_enc_ext_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x) !!",
		type, ptr_enc_ext_addr_mem[enc_idx],
		(ptr_enc_ext_addr_mem[enc_idx] - size),
		ptr_enc_ext_addr_mem[enc_idx], size);

	ptr_enc_ext_addr_mem[enc_idx] -= size;
	sz_enc_ext_used_mem[enc_idx] -= size;

	if (ptr_enc_ext_addr_mem[enc_idx] != phyaddr) {
		V_DBG(VPU_DBG_ERROR,
			"type[%d] :: ptr_enc_ext_addr_mem release-mem order!! 0x%x != 0x%x",
			type, ptr_enc_ext_addr_mem[enc_idx], phyaddr);
	}

	sz_enc_ext_remained_mem[enc_idx] += size;

	return 0;
}
#endif

static int _vmem_check_allocation_available(char check_type,
					    unsigned int request_size,
					    vputype type)
{
	switch (check_type) {
	case 0:		// Check total memory which can allocate.
		if (sz_remained_mem < request_size) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] : insufficient memory : remain(0x%x), request(0x%x)",
				type, sz_remained_mem, request_size);
			return -1;
		}
		break;

	case 1:		// Check encoder memory.
		if (sz_enc_mem < (sz_rear_used_mem + request_size)) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] : insufficient memory : total(0x%x) for only encoder, allocated(0x%x), request(0x%x)",
				type, sz_enc_mem, sz_rear_used_mem,
				request_size);
			return -1;
		}
		break;

	case 2:		// Check remainning except encoder memory.
		if ((pmap_video.size - sz_enc_mem - VPU_SW_ACCESS_REGION_SIZE) <
			(sz_ext_used_mem + sz_front_used_mem + request_size)) {
			V_DBG(VPU_DBG_ERROR,
				"type[%u] : insufficient memory : total(0x%llx(%llu) ) except encoder, allocated(0x%x(%u) ), request(0x%x(%u) )",
				type, (pmap_video.size - sz_enc_mem),
				(pmap_video.size - sz_enc_mem),
				(sz_ext_used_mem + sz_front_used_mem),
				(sz_ext_used_mem + sz_front_used_mem),
				request_size, request_size);
			return -1;
		}
		break;

	default:
		break;
	}

	return 0;
}

static phys_addr_t _vmem_request_phyaddr(unsigned int request_size,
					 vputype type)
{
	phys_addr_t curr_phyaddr = 0;

#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
	if (_vmem_check_allocation_available(0, request_size, type))
		return 0;

	if (type == VPU_DEC || type == VPU_ENC) {
		curr_phyaddr = ptr_front_addr_mem;
		ptr_front_addr_mem += request_size;
		sz_front_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc front = 0x%x ~ 0x%x, 0x%x(%u)!!, used 0x%x(%u)",
			type, curr_phyaddr, ptr_front_addr_mem, request_size,
			request_size, sz_front_used_mem, sz_front_used_mem);
	} else if (type == VPU_DEC_EXT || type == VPU_ENC_EXT) {
		ptr_rear_addr_mem -= request_size;
		curr_phyaddr = ptr_rear_addr_mem;
		sz_rear_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc rear = 0x%x ~ 0x%x, 0x%x(%u)!!, used 0x%x(%u)",
			type, (ptr_rear_addr_mem + request_size), curr_phyaddr,
			request_size, request_size, sz_rear_used_mem,
			sz_rear_used_mem);
	}
#else
	if (only_decmode) {
		if (_vmem_check_allocation_available(
			0, request_size, type))
			return 0;
	} else {
		if (type == VPU_ENC) {
			if (_vmem_check_allocation_available(
				1, request_size, type) < 0)
				return 0;
		} else {	// for Decoder_ext or Decoder.
			if (type == VPU_DEC_EXT) {	// regarding to encoder.
				if (_vmem_check_allocation_available(
					2, request_size, type) < 0)
					return 0;
			} else {	// only for 1st Decoder.
				if (vmgr_get_close(VPU_DEC_EXT)
				    && vmgr_get_close(VPU_ENC)
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
				    && vmgr_hevc_enc_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
				    && vmgr_4k_d2_get_close(VPU_DEC_EXT)
				    && vmgr_4k_d2_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
				    && hmgr_get_close(VPU_DEC_EXT)
				    && hmgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
				    && vp9mgr_get_close(VPU_DEC_EXT)
				    && vp9mgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
				    && jmgr_get_close(VPU_DEC_EXT)
				    && jmgr_get_close(VPU_ENC)
#endif
				    ) {
					if (_vmem_check_allocation_available(
						0, request_size, type) < 0)
						return 0;
				} else {
					// in case Decoder only is opened
					// with Decoder_ext or Encoder.
					if (_vmem_check_allocation_available(
						2, request_size, type) < 0)
						return 0;
				}
			}
		}
	}

	if (type == VPU_DEC) {
		curr_phyaddr = ptr_front_addr_mem;
		ptr_front_addr_mem += request_size;
		sz_front_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x(%u)!!, used 0x%x(%u)",
			type, curr_phyaddr, ptr_front_addr_mem, request_size,
			request_size, sz_front_used_mem, sz_front_used_mem);
	} else if (type == VPU_DEC_EXT) {
		if (only_decmode) {
			ptr_rear_addr_mem -= request_size;
			curr_phyaddr = ptr_rear_addr_mem;
			sz_rear_used_mem += request_size;

			V_DBG(VPU_DBG_MEM_USAGE,
				"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x(%u)!!, used 0x%x(%u)",
				type, (ptr_rear_addr_mem + request_size),
				curr_phyaddr, request_size, request_size,
				sz_rear_used_mem, sz_rear_used_mem);
		} else {
			ptr_ext_addr_mem -= request_size;
			curr_phyaddr = ptr_ext_addr_mem;
			sz_ext_used_mem += request_size;

			V_DBG(VPU_DBG_MEM_USAGE,
				"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x(%u)!!, used 0x%x(%u)",
				type, (ptr_ext_addr_mem + request_size),
				curr_phyaddr, request_size, request_size,
				sz_ext_used_mem, sz_ext_used_mem);
		}
	} else {		/*VPU_ENC */

		if (only_decmode) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] : There is no memory for encoder because of only decoder mode.",
				type);
			return 0;
		}

		ptr_rear_addr_mem -= request_size;
		curr_phyaddr = ptr_rear_addr_mem;
		sz_rear_used_mem += request_size;

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : alloc = 0x%x ~ 0x%x, 0x%x(%u)!!, used 0x%x(%u)",
			type, (ptr_rear_addr_mem + request_size), curr_phyaddr,
			request_size, request_size, sz_rear_used_mem,
			sz_rear_used_mem);
	}
#endif
	V_DBG(VPU_DBG_MEM_USAGE,
		"type[%d] : mem usage = 0x%x/0x%llx = Dec(f:0x%x(%u) + ext:0x%x(%u) ) + Dec(rear(0x%x(%u) ) !!",
		type, sz_front_used_mem + sz_rear_used_mem + sz_ext_used_mem,
		pmap_video.size, sz_front_used_mem, sz_front_used_mem,
		sz_ext_used_mem, sz_ext_used_mem, sz_rear_used_mem,
		sz_rear_used_mem);

	sz_remained_mem -= request_size;

	return curr_phyaddr;
}

static int _vmem_release_phyaddr(phys_addr_t phyaddr, unsigned int size,
				vputype type)
{
#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
	if (type == VPU_DEC || type == VPU_ENC) {
		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x(%u) ) !!",
			type, ptr_front_addr_mem, (ptr_front_addr_mem - size),
			ptr_front_addr_mem, size, size);

		ptr_front_addr_mem -= size;
		sz_front_used_mem -= size;

		if (ptr_front_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: release-mem order!! 0x%x != 0x%x",
				type, ptr_front_addr_mem, phyaddr);
		}
	} else if (type == VPU_DEC_EXT || type == VPU_ENC_EXT) {
		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_rear_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x(%u) ) !!",
			type, ptr_rear_addr_mem, (ptr_rear_addr_mem + size),
			ptr_rear_addr_mem, size, size);

		if (ptr_rear_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: release-mem order!! 0x%x != 0x%x",
				type, ptr_rear_addr_mem, phyaddr);
		}

		ptr_rear_addr_mem += size;
		sz_rear_used_mem -= size;
	}
#else
	//calc remained_size and ptr_addr_mem
	if (type == VPU_DEC) {
		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_front_addr_mem = 0x%x -> 0x%x(0x%x - 0x%x(%u) ) !!",
			type, ptr_front_addr_mem, (ptr_front_addr_mem - size),
			ptr_front_addr_mem, size, size);

		ptr_front_addr_mem -= size;
		sz_front_used_mem -= size;

		if (ptr_front_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: release-mem order!! 0x%x != 0x%x",
				type, ptr_front_addr_mem, phyaddr);
		}
	} else if (type == VPU_DEC_EXT) {
		if (only_decmode) {
			V_DBG(VPU_DBG_MEM_USAGE,
				"type[%d] : release ptr_rear_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x(%u) )!!",
				type, ptr_rear_addr_mem,
				(ptr_rear_addr_mem + size), ptr_rear_addr_mem,
				size, size);

			if (ptr_rear_addr_mem != phyaddr) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d] :: release-mem order!! 0x%x != 0x%x",
					type, ptr_rear_addr_mem, phyaddr);
			}

			ptr_rear_addr_mem += size;
			sz_rear_used_mem -= size;
		} else {
			V_DBG(VPU_DBG_MEM_USAGE,
				"type[%d] : release ptr_ext_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x(%u) )!!",
				type, ptr_ext_addr_mem,
				(ptr_ext_addr_mem + size),
				ptr_ext_addr_mem, size, size);

			if (ptr_ext_addr_mem != phyaddr) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d] :: release-mem order!! 0x%x != 0x%x",
					type, ptr_ext_addr_mem, phyaddr);
			}

			ptr_ext_addr_mem += size;
			sz_ext_used_mem -= size;
		}
	} else {		/*VPU_ENC */

		V_DBG(VPU_DBG_MEM_USAGE,
			"type[%d] : release ptr_rear_addr_mem = 0x%x -> 0x%x(0x%x + 0x%x(%u) )!!",
			type, ptr_rear_addr_mem, (ptr_rear_addr_mem + size),
			ptr_rear_addr_mem, size, size);

		if (ptr_rear_addr_mem != phyaddr) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d] :: release-mem order!! 0x%x != 0x%x",
				type, ptr_rear_addr_mem, phyaddr);
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
	unsigned int vpu_hevc_enc_alloc_size = 0;
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

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	if (gsVpuHevcEncWork_memInfo.kernel_remap_addr == 0) {
		// VPU_HEVC_ENC (WAVE420L) WORK BUFFER
		gsVpuHevcEncWork_memInfo.request_size
		    = VPU_HEVC_ENC_WORK_BUF_SIZE;
		if (gsVpuHevcEncWork_memInfo.request_size) {
			gsVpuHevcEncWork_memInfo.phy_addr =
				ptr_sw_addr_mem +
					vpu_4k_d2_alloc_size +
					vpu_hevc_enc_alloc_size +
					hevc_alloc_size +
					jpu_alloc_size +
					vpu_alloc_size +
					vp9_alloc_size +
					seq_alloc_size +
					user_alloc_size;
			gsVpuHevcEncWork_memInfo.kernel_remap_addr =
				_vmem_get_virtaddr((phys_addr_t)
					gsVpuHevcEncWork_memInfo.phy_addr,
					PAGE_ALIGN(
					gsVpuHevcEncWork_memInfo.request_size
					/*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU_HEVC_ENC workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! ",
				gsVpuHevcEncWork_memInfo.phy_addr,
				gsVpuHevcEncWork_memInfo.kernel_remap_addr,
				gsVpuHevcEncWork_memInfo.request_size);

			if (gsVpuHevcEncWork_memInfo.kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				    "[0x%p] VPU_HEVC_ENC workbuffer remap ALLOC_MEMORY failed.",
				    gsVpuHevcEncWork_memInfo.kernel_remap_addr);
				ret = -1;
				goto Error;
			}

			vpu_hevc_enc_alloc_size =
				gsVpuHevcEncWork_memInfo.request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR,
			"[0x%p] VPU_HEVC_ENC (WAVE420L) - WorkBuff : already remapped?",
			gsVpuHevcEncWork_memInfo.kernel_remap_addr);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (gsVpu4KD2Work_memInfo.kernel_remap_addr == 0) {
		// VPU_4K_D2 WORK BUFFER
		gsVpu4KD2Work_memInfo.request_size = WAVExxx_WORK_BUF_SIZE;
		if (gsVpu4KD2Work_memInfo.request_size) {
			gsVpu4KD2Work_memInfo.phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size;
			gsVpu4KD2Work_memInfo.kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			     gsVpu4KD2Work_memInfo.phy_addr,
			     PAGE_ALIGN(
			       gsVpu4KD2Work_memInfo.request_size
			       /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU_4K_D2(WAVE512) workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !! ",
				gsVpu4KD2Work_memInfo.phy_addr,
				gsVpu4KD2Work_memInfo.kernel_remap_addr,
				gsVpu4KD2Work_memInfo.request_size);

			if (gsVpu4KD2Work_memInfo.kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				      "[0x%p] VPU_4K_D2 workbuffer remap ALLOC_MEMORY failed.",
				      gsVpu4KD2Work_memInfo.kernel_remap_addr);
				ret = -1;
				goto Error;
			}

			vpu_4k_d2_alloc_size =
			    gsVpu4KD2Work_memInfo.request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR,
		      "[0x%p] VPU_4K_D2-WorkBuff : already remapped?",
		      gsVpu4KD2Work_memInfo.kernel_remap_addr);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (gsWave410_Work_memInfo.kernel_remap_addr == 0) {
		// HEVC WORK BUFFER
		gsWave410_Work_memInfo.request_size = WAVExxx_WORK_BUF_SIZE;
		if (gsWave410_Work_memInfo.request_size) {
			gsWave410_Work_memInfo.phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size;
			gsWave410_Work_memInfo.kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			     gsWave410_Work_memInfo.phy_addr,
			    PAGE_ALIGN(
			      gsWave410_Work_memInfo.request_size
			      /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
			      "alloc HEVC D8(WAVE410) workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			      gsWave410_Work_memInfo.phy_addr,
			      gsWave410_Work_memInfo.kernel_remap_addr,
			      gsWave410_Work_memInfo.request_size);

			if (gsWave410_Work_memInfo.kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				      "[0x%p] HEVC workbuffer remap ALLOC_MEMORY failed.",
				      gsWave410_Work_memInfo.kernel_remap_addr);
				ret = -1;
				goto Error;
			}

			hevc_alloc_size = gsWave410_Work_memInfo.request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[0x%p] HEVC-WorkBuff : already remapped?",
		      gsWave410_Work_memInfo.kernel_remap_addr);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (gsG2V2_Vp9Work_memInfo.kernel_remap_addr == 0) {
		// VP9 WORK BUFFER
		gsG2V2_Vp9Work_memInfo.request_size = G2V2_VP9_WORK_BUF_SIZE;
		if (gsG2V2_Vp9Work_memInfo.request_size) {
			gsG2V2_Vp9Work_memInfo.phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size;
			gsG2V2_Vp9Work_memInfo.kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			     gsG2V2_Vp9Work_memInfo.phy_addr,
			    PAGE_ALIGN(
			      gsG2V2_Vp9Work_memInfo.request_size
			      /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VP9 workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				gsG2V2_Vp9Work_memInfo.phy_addr,
				gsG2V2_Vp9Work_memInfo.kernel_remap_addr,
				gsG2V2_Vp9Work_memInfo.request_size);

			if (gsG2V2_Vp9Work_memInfo.kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				  "[0x%p] VP9 workbuffer remap ALLOC_MEMORY failed.",
				  gsG2V2_Vp9Work_memInfo.kernel_remap_addr);
				ret = -1;
				goto Error;
			}

			hevc_alloc_size = gsG2V2_Vp9Work_memInfo.request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[0x%p] VP9-WorkBuff : already remapped?",
			gsG2V2_Vp9Work_memInfo.kernel_remap_addr);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (gsJpuWork_memInfo.kernel_remap_addr == 0) {
		// JPU WORK BUFFER
		gsJpuWork_memInfo.request_size = JPU_WORK_BUF_SIZE;
		if (gsJpuWork_memInfo.request_size) {
			gsJpuWork_memInfo.phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size;
			gsJpuWork_memInfo.kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			     gsJpuWork_memInfo.phy_addr,
			     PAGE_ALIGN(
			       gsJpuWork_memInfo.request_size
			       /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc JPU workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				gsJpuWork_memInfo.phy_addr,
				gsJpuWork_memInfo.kernel_remap_addr,
				gsJpuWork_memInfo.request_size);

			if (gsJpuWork_memInfo.kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
					"[0x%p] JPU workbuffer remap ALLOC_MEMORY failed.",
					gsJpuWork_memInfo.kernel_remap_addr);
				ret = -1;
				goto Error;
			}

			jpu_alloc_size = gsJpuWork_memInfo.request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[0x%p] JPU-WorkBuff : already remapped?",
			gsJpuWork_memInfo.kernel_remap_addr);
	}
#endif

	if (gsVpuWork_memInfo.kernel_remap_addr == 0) {
		// VPU WORK BUFFER
		gsVpuWork_memInfo.request_size = VPU_WORK_BUF_SIZE;
		if (gsVpuWork_memInfo.request_size) {
			gsVpuWork_memInfo.phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size;
			gsVpuWork_memInfo.kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			    gsVpuWork_memInfo.phy_addr,
			    PAGE_ALIGN(
			      gsVpuWork_memInfo.request_size
			      /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
			      "alloc VPU workbuffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			      gsVpuWork_memInfo.phy_addr,
			      gsVpuWork_memInfo.kernel_remap_addr,
			      gsVpuWork_memInfo.request_size);

			if (gsVpuWork_memInfo.kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				      "[0x%p] VPU workbuffer remap ALLOC_MEMORY failed.",
				      gsVpuWork_memInfo.kernel_remap_addr);
				ret = -1;
				goto Error;
			}

			vpu_alloc_size = gsVpuWork_memInfo.request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[0x%p] VPU-WorkBuff : already remapped?",
		      gsVpuWork_memInfo.kernel_remap_addr);
	}

#if DEFINED_CONFIG_VENC_CNT_12345678
	for (type = 0; type < VPU_ENC_MAX_CNT; type++) {
		if (gsVpuEncSeqheader_memInfo[type].kernel_remap_addr == 0) {
		// SEQ-HEADER BUFFER FOR ENCODER
			gsVpuEncSeqheader_memInfo[type].request_size
			  = PAGE_ALIGN(ENC_HEADER_BUF_SIZE);
			if (gsVpuEncSeqheader_memInfo[type].request_size) {
				gsVpuEncSeqheader_memInfo[type].phy_addr
				  = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
				    + vpu_hevc_enc_alloc_size
				    + hevc_alloc_size + jpu_alloc_size
				    + vpu_alloc_size + vp9_alloc_size
				    + seq_alloc_size + user_alloc_size;
				gsVpuEncSeqheader_memInfo
				   [type].kernel_remap_addr =
				    _vmem_get_virtaddr((phys_addr_t)
					gsVpuEncSeqheader_memInfo
					[type].phy_addr,
					PAGE_ALIGN
					(gsVpuEncSeqheader_memInfo
					[type].request_size
					/*-PAGE_SIZE*/));

				V_DBG(VPU_DBG_MEMORY,
				      "alloc VPU seqheader_mem[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				      type,
				      gsVpuEncSeqheader_memInfo[type].phy_addr,
				      gsVpuEncSeqheader_memInfo
				      [type].kernel_remap_addr,
				      gsVpuEncSeqheader_memInfo
				      [type].request_size);

				if (gsVpuEncSeqheader_memInfo
				[type].kernel_remap_addr == 0) {
					V_DBG(VPU_DBG_ERROR,
					      "[%d] VPU seqheader_mem[0x%p] remap ALLOC_MEMORY failed.",
					      type,
					      gsVpuEncSeqheader_memInfo
					      [type].kernel_remap_addr);
					ret = -1;
					goto Error;
				}
				seq_alloc_size +=
				  gsVpuEncSeqheader_memInfo
				  [type].request_size;
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
			      "[%d] SeqHeader[0x%p] : already remapped?", type,
			      gsVpuEncSeqheader_memInfo
			      [type].kernel_remap_addr);
		}
	}
#endif

	for (type = 0; type < VPU_INST_MAX; type++) {
		if (gsVpuUserData_memInfo[type].kernel_remap_addr == 0) {
			// USER DATA
			gsVpuUserData_memInfo[type].request_size =
			    PAGE_ALIGN(USER_DATA_BUF_SIZE);
			if (gsVpuUserData_memInfo[type].request_size) {
				gsVpuUserData_memInfo[type].phy_addr
				 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
				    + vpu_hevc_enc_alloc_size
				    + hevc_alloc_size + jpu_alloc_size
				    + vpu_alloc_size + vp9_alloc_size
				    + seq_alloc_size + user_alloc_size;
				gsVpuUserData_memInfo[type].kernel_remap_addr =
				    _vmem_get_virtaddr((phys_addr_t)
					gsVpuUserData_memInfo
					[type].phy_addr,
					PAGE_ALIGN
					(gsVpuUserData_memInfo
					[type].request_size
					/*-PAGE_SIZE*/));

				V_DBG(VPU_DBG_MEMORY,
				      "alloc VPU userdata_mem[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				      type,
				      gsVpuUserData_memInfo[type].phy_addr,
				      gsVpuUserData_memInfo
				      [type].kernel_remap_addr,
				      gsVpuUserData_memInfo[type].request_size);

				if (gsVpuUserData_memInfo[type]
				   .kernel_remap_addr == 0) {
					V_DBG(VPU_DBG_ERROR,
					      "[%d] VPU userdata_mem[0x%p] remap ALLOC_MEMORY failed.",
					      type,
					      gsVpuUserData_memInfo
					      [type].kernel_remap_addr);
					ret = -1;
					goto Error;
				}
				user_alloc_size +=
				    gsVpuUserData_memInfo[type].request_size;
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
			      "[%d] UserData[0x%p] : already remapped?", type,
			      gsVpuUserData_memInfo[type].kernel_remap_addr);
		}
	}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	if (gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
		gsPs_memInfo[VPU_DEC_EXT2].request_size =
		    PAGE_ALIGN(PS_SAVE_SIZE);
		if (gsPs_memInfo[VPU_DEC_EXT2].request_size) {
			gsPs_memInfo[VPU_DEC_EXT2].phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size
			    + alloc_ps_size + alloc_slice_size;
			gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr =
				_vmem_get_virtaddr((phys_addr_t)
				gsPs_memInfo[VPU_DEC_EXT2].phy_addr,
				PAGE_ALIGN(
				gsPs_memInfo[VPU_DEC_EXT2].request_size
				/*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
			      "alloc VPU Ps[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			      VPU_DEC_EXT2, gsPs_memInfo[VPU_DEC_EXT2].phy_addr,
			      gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr,
			      gsPs_memInfo[VPU_DEC_EXT2].request_size);

			if (gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				      "[%d] VPU Ps[0x%p] remap ALLOC_MEMORY failed.",
				      VPU_DEC_EXT2,
				      gsPs_memInfo
				      [VPU_DEC_EXT2].kernel_remap_addr);
				ret = -1;
				goto Error;
			}
			alloc_ps_size = gsPs_memInfo[VPU_DEC_EXT2].request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[%d] Ps[0x%p] : already remapped?",
		      VPU_DEC_EXT2,
		      gsPs_memInfo[VPU_DEC_EXT2].kernel_remap_addr);
	}

	if (gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
		gsSlice_memInfo[VPU_DEC_EXT2].request_size =
		    PAGE_ALIGN(SLICE_SAVE_SIZE);
		if (gsSlice_memInfo[VPU_DEC_EXT2].request_size) {
			gsSlice_memInfo[VPU_DEC_EXT2].phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size
			    + alloc_ps_size + alloc_slice_size;
			gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			     gsSlice_memInfo[VPU_DEC_EXT2].phy_addr,
			    PAGE_ALIGN(
			      gsSlice_memInfo[VPU_DEC_EXT2].request_size
			      /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU Slice[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				VPU_DEC_EXT2,
				gsSlice_memInfo[VPU_DEC_EXT2].phy_addr,
				gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr,
				gsSlice_memInfo[VPU_DEC_EXT2].request_size);

			if (gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr
				== 0) {
				V_DBG(VPU_DBG_ERROR,
				"[%d] VPU Slice[0x%p] remap ALLOC_MEMORY failed.",
				VPU_DEC_EXT2,
				gsSlice_memInfo[VPU_DEC_EXT2]
				    .kernel_remap_addr);
				ret = -1;
				goto Error;
			}
			alloc_slice_size =
				gsSlice_memInfo[VPU_DEC_EXT2].request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[%d] Slice[0x%p] : already remapped?",
			VPU_DEC_EXT2,
			gsSlice_memInfo[VPU_DEC_EXT2].kernel_remap_addr);
	}

	if (gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr == 0) {
		gsStream_memInfo[VPU_DEC_EXT2].request_size =
		    PAGE_ALIGN(LARGE_STREAM_BUF_SIZE);
		if (gsStream_memInfo[VPU_DEC_EXT2].request_size) {
			gsStream_memInfo[VPU_DEC_EXT2].phy_addr
			 = ptr_sw_addr_mem + vpu_4k_d2_alloc_size
			    + vpu_hevc_enc_alloc_size
			    + hevc_alloc_size + jpu_alloc_size
			    + vpu_alloc_size + vp9_alloc_size
			    + seq_alloc_size + user_alloc_size
			    + alloc_ps_size + alloc_slice_size;
			gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			     gsStream_memInfo[VPU_DEC_EXT2].phy_addr,
			    PAGE_ALIGN(
			     gsStream_memInfo[VPU_DEC_EXT2].request_size
			     /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU Stream[%d] Info buffer ::",
				VPU_DEC_EXT2);
			V_DBG(VPU_DBG_MEMORY,
				"phy = 0x%x, remap = 0x%p, size = 0x%x!!\n",
				gsStream_memInfo[VPU_DEC_EXT2].phy_addr,
				gsStream_memInfo[VPU_DEC_EXT2]
				    .kernel_remap_addr,
				gsStream_memInfo[VPU_DEC_EXT2].request_size);

			if (gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr
				== 0) {
				V_DBG(VPU_DBG_ERROR,
				"[%d] VPU Stream[0x%p] remap ALLOC_MEMORY failed.",
				VPU_DEC_EXT2,
				gsStream_memInfo[VPU_DEC_EXT2]
				    .kernel_remap_addr);
				ret = -1;
				goto Error;
			}
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[%d] Stream[0x%p] : already remapped?",
			VPU_DEC_EXT2,
			gsStream_memInfo[VPU_DEC_EXT2].kernel_remap_addr);
	}

	alloc_ps_size = alloc_slice_size = 0;

	if (gsPs_memInfo[VPU_DEC].kernel_remap_addr == 0) {
		gsPs_memInfo[VPU_DEC].request_size = PAGE_ALIGN(PS_SAVE_SIZE);
		if (gsPs_memInfo[VPU_DEC].request_size) {
			gsPs_memInfo[VPU_DEC].phy_addr
			 = ptr_ext_rear_addr_mem + alloc_ps_size
			    + alloc_slice_size;
			gsPs_memInfo[VPU_DEC].kernel_remap_addr
			 = _vmem_get_virtaddr((phys_addr_t)
			    gsPs_memInfo[VPU_DEC].phy_addr,
			    PAGE_ALIGN(gsPs_memInfo[VPU_DEC].request_size
			    /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU Ps[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				VPU_DEC, gsPs_memInfo[VPU_DEC].phy_addr,
				gsPs_memInfo[VPU_DEC].kernel_remap_addr,
				gsPs_memInfo[VPU_DEC].request_size);

			if (gsPs_memInfo[VPU_DEC].kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				"[0x%p] VPU Ps[%d] remap ALLOC_MEMORY failed.",
				VPU_DEC,
				gsPs_memInfo[VPU_DEC].kernel_remap_addr);
				ret = -1;
				goto Error;
			}
			alloc_ps_size = gsPs_memInfo[VPU_DEC].request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[%d] Ps[0x%p] : already remapped?",
			VPU_DEC, gsPs_memInfo[VPU_DEC].kernel_remap_addr);
	}

	if (gsSlice_memInfo[VPU_DEC].kernel_remap_addr == 0) {
		gsSlice_memInfo[VPU_DEC].request_size =
		    PAGE_ALIGN(SLICE_SAVE_SIZE);
		if (gsSlice_memInfo[VPU_DEC].request_size) {
			gsSlice_memInfo[VPU_DEC].phy_addr
			 = ptr_ext_rear_addr_mem + alloc_ps_size
			    + alloc_slice_size;
			gsSlice_memInfo[VPU_DEC].kernel_remap_addr
			 = _vmem_get_virtaddr(
			  (phys_addr_t)gsSlice_memInfo[VPU_DEC].phy_addr,
			  PAGE_ALIGN(gsSlice_memInfo[VPU_DEC].request_size
			  /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU Slice[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				VPU_DEC, gsSlice_memInfo[VPU_DEC].phy_addr,
				gsSlice_memInfo[VPU_DEC].kernel_remap_addr,
				gsSlice_memInfo[VPU_DEC].request_size);

			if (gsSlice_memInfo[VPU_DEC].kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				  "[0x%p] VPU Slice[%d] remap ALLOC_MEMORY failed.",
				  VPU_DEC,
				  gsSlice_memInfo[VPU_DEC].kernel_remap_addr);
				ret = -1;
				goto Error;
			}
			alloc_slice_size =
			    gsSlice_memInfo[VPU_DEC].request_size;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[%d] Slice[0x%p] : already remapped?",
			VPU_DEC, gsSlice_memInfo[VPU_DEC].kernel_remap_addr);
	}

	if (gsStream_memInfo[VPU_DEC].kernel_remap_addr == 0) {
		gsStream_memInfo[VPU_DEC].request_size =
		    PAGE_ALIGN(LARGE_STREAM_BUF_SIZE);
		if (gsStream_memInfo[VPU_DEC].request_size) {
			gsStream_memInfo[VPU_DEC].phy_addr
			 = ptr_ext_rear_addr_mem + alloc_ps_size
			    + alloc_slice_size;
			gsStream_memInfo[VPU_DEC].kernel_remap_addr
			 = _vmem_get_virtaddr(
			   (phys_addr_t)gsStream_memInfo[VPU_DEC].phy_addr,
			  PAGE_ALIGN(gsStream_memInfo[VPU_DEC].request_size
			  /*-PAGE_SIZE*/));

			V_DBG(VPU_DBG_MEMORY,
				"alloc VPU Stream[%d] Info buffer :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				VPU_DEC, gsStream_memInfo[VPU_DEC].phy_addr,
				gsStream_memInfo[VPU_DEC].kernel_remap_addr,
				gsStream_memInfo[VPU_DEC].request_size);

			if (gsStream_memInfo[VPU_DEC].kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
				  "[0x%p] VPU Stream[%d] remap ALLOC_MEMORY failed.",
				  VPU_DEC,
				  gsStream_memInfo[VPU_DEC].kernel_remap_addr);
				ret = -1;
				goto Error;
			}
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "[%d] Stream[0x%p] : already remapped?",
			VPU_DEC, gsStream_memInfo[VPU_DEC].kernel_remap_addr);
	}
#endif

Error:
	return ret;
}

void _vmem_free_dedicated_buffer(void)
{
	int type = 0;

	V_DBG(VPU_DBG_MEM_SEQ, "enter");

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	if (gsVpuHevcEncWork_memInfo.kernel_remap_addr != 0) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"free VPU_HEVC_ENC (WAVE420L) workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			gsVpuHevcEncWork_memInfo.phy_addr,
			gsVpuHevcEncWork_memInfo.kernel_remap_addr,
			gsVpuHevcEncWork_memInfo.request_size);
		_vmem_release_virtaddr(((void *)
			gsVpuHevcEncWork_memInfo.kernel_remap_addr),
			gsVpuHevcEncWork_memInfo.phy_addr,
			gsVpuHevcEncWork_memInfo.request_size);
		memset(&gsVpuHevcEncWork_memInfo, 0x00,
			sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
	if (gsJpuWork_memInfo.kernel_remap_addr != 0) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"free JPU workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			gsJpuWork_memInfo.phy_addr,
			gsJpuWork_memInfo.kernel_remap_addr,
			gsJpuWork_memInfo.request_size);
		_vmem_release_virtaddr(((void *)
			gsJpuWork_memInfo.kernel_remap_addr),
			gsJpuWork_memInfo.phy_addr,
			gsJpuWork_memInfo.request_size);
		memset(&gsJpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (gsVpu4KD2Work_memInfo.kernel_remap_addr != 0) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"free VPU_4K_D2(WAVE512) workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			gsVpu4KD2Work_memInfo.phy_addr,
			gsVpu4KD2Work_memInfo.kernel_remap_addr,
			gsVpu4KD2Work_memInfo.request_size);
		_vmem_release_virtaddr(
			((void *)gsVpu4KD2Work_memInfo.kernel_remap_addr),
			       gsVpu4KD2Work_memInfo.phy_addr,
			       gsVpu4KD2Work_memInfo.request_size);
		memset(&gsVpu4KD2Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (gsWave410_Work_memInfo.kernel_remap_addr != 0) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"free HEVC(WAVE410) workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			gsWave410_Work_memInfo.phy_addr,
			gsWave410_Work_memInfo.kernel_remap_addr,
			gsWave410_Work_memInfo.request_size);
		_vmem_release_virtaddr(((void *)
			gsWave410_Work_memInfo.kernel_remap_addr),
			gsWave410_Work_memInfo.phy_addr,
			gsWave410_Work_memInfo.request_size);
		memset(&gsWave410_Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (gsG2V2_Vp9Work_memInfo.kernel_remap_addr != 0) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"free VP9 workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			gsG2V2_Vp9Work_memInfo.phy_addr,
			gsG2V2_Vp9Work_memInfo.kernel_remap_addr,
		      gsG2V2_Vp9Work_memInfo.request_size);
		_vmem_release_virtaddr(
		  ((void *)gsG2V2_Vp9Work_memInfo.kernel_remap_addr),
			       gsG2V2_Vp9Work_memInfo.phy_addr,
			       gsG2V2_Vp9Work_memInfo.request_size);
		memset(&gsG2V2_Vp9Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_12345678
	for (type = 0; type < VPU_ENC_MAX_CNT; type++) {
		if (gsVpuEncSeqheader_memInfo[type].kernel_remap_addr != 0) {
			V_DBG(VPU_DBG_MEM_SEQ,
			    "free SeqHeader[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			    type,
			    gsVpuEncSeqheader_memInfo[type].phy_addr,
			    gsVpuEncSeqheader_memInfo[type].kernel_remap_addr,
			    gsVpuEncSeqheader_memInfo[type].request_size);
			_vmem_release_virtaddr(((void *)
			  gsVpuEncSeqheader_memInfo[type].kernel_remap_addr),
			  gsVpuEncSeqheader_memInfo[type].phy_addr,
			  gsVpuEncSeqheader_memInfo[type].request_size);
			memset(&gsVpuEncSeqheader_memInfo[type], 0x00,
			       sizeof(MEM_ALLOC_INFO_t));
		}
	}
#endif

	if (gsVpuWork_memInfo.kernel_remap_addr != 0) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"free workbuffer:: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			gsVpuWork_memInfo.phy_addr,
			gsVpuWork_memInfo.kernel_remap_addr,
			gsVpuWork_memInfo.request_size);
		_vmem_release_virtaddr(((void *)
			gsVpuWork_memInfo.kernel_remap_addr),
			gsVpuWork_memInfo.phy_addr,
			gsVpuWork_memInfo.request_size);
		memset(&gsVpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
	}

	for (type = 0; type < VPU_INST_MAX; type++) {
		if (gsVpuUserData_memInfo[type].kernel_remap_addr != 0) {
			V_DBG(VPU_DBG_MEM_SEQ,
				"free UserData[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				type, gsVpuUserData_memInfo[type].phy_addr,
				gsVpuUserData_memInfo[type].kernel_remap_addr,
				gsVpuUserData_memInfo[type].request_size);
			_vmem_release_virtaddr(
			  ((void *)
			  gsVpuUserData_memInfo[type].kernel_remap_addr),
			  gsVpuUserData_memInfo[type].phy_addr,
			  gsVpuUserData_memInfo[type].request_size);
			memset(&gsVpuUserData_memInfo[type], 0x00,
			       sizeof(MEM_ALLOC_INFO_t));
		}
	}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	{
		int type = 0;

		for (type = 0; type < VPU_INST_MAX; type++) {
			if (gsPs_memInfo[type].kernel_remap_addr != 0) {
				V_DBG(VPU_DBG_MEM_SEQ,
					"free PS :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
					gsPs_memInfo[type].phy_addr,
					gsPs_memInfo[type].kernel_remap_addr,
					gsPs_memInfo[type].request_size);
				_vmem_release_virtaddr(((void *)
					gsPs_memInfo[type].kernel_remap_addr),
					gsPs_memInfo[type].phy_addr,
					gsPs_memInfo[type].request_size);
				memset(&gsPs_memInfo, 0x00,
					sizeof(MEM_ALLOC_INFO_t));
			}
			if (gsSlice_memInfo[type].kernel_remap_addr != 0) {
				V_DBG(VPU_DBG_MEM_SEQ,
					"free Slice :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
					gsSlice_memInfo[type].phy_addr,
					gsSlice_memInfo[type].kernel_remap_addr,
					gsSlice_memInfo[type].request_size);
				_vmem_release_virtaddr(((void *)
				  gsSlice_memInfo[type].kernel_remap_addr),
				  gsSlice_memInfo[type].phy_addr,
				  gsSlice_memInfo[type].request_size);
				memset(&gsSlice_memInfo, 0x00,
					sizeof(MEM_ALLOC_INFO_t));
			}
			if (gsStream_memInfo[type].kernel_remap_addr != 0) {
				V_DBG(VPU_DBG_MEM_SEQ,
				  "free Stream :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
				  gsStream_memInfo[type].phy_addr,
				  gsStream_memInfo[type].kernel_remap_addr,
				  gsStream_memInfo[type].request_size);
				_vmem_release_virtaddr(((void *)
				  gsStream_memInfo[type].kernel_remap_addr),
				  gsStream_memInfo[type].phy_addr,
				  gsStream_memInfo[type].request_size);
				memset(&gsStream_memInfo, 0x00,
					sizeof(MEM_ALLOC_INFO_t));
			}
		}
	}
#endif
}

static phys_addr_t _vmem_request_workbuff_phyaddr(int codec_type,
						  void **remapped_addr)
{
	phys_addr_t phy_address = 0x00;

	switch (codec_type) {
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	case STD_HEVC_ENC:
		*remapped_addr = gsVpuHevcEncWork_memInfo.kernel_remap_addr;
		phy_address = gsVpuHevcEncWork_memInfo.phy_addr;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
	case STD_MJPG:
		*remapped_addr = gsJpuWork_memInfo.kernel_remap_addr;
		phy_address = gsJpuWork_memInfo.phy_addr;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	case STD_HEVC:
		*remapped_addr = gsWave410_Work_memInfo.kernel_remap_addr;
		phy_address = gsWave410_Work_memInfo.phy_addr;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	case STD_HEVC:
	case STD_VP9:
		*remapped_addr = gsVpu4KD2Work_memInfo.kernel_remap_addr;
		phy_address = gsVpu4KD2Work_memInfo.phy_addr;
		break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	case STD_VP9:
		*remapped_addr = gsG2V2_Vp9Work_memInfo.kernel_remap_addr;
		phy_address = gsG2V2_Vp9Work_memInfo.phy_addr;
		break;
#endif
	default:
		*remapped_addr = gsVpuWork_memInfo.kernel_remap_addr;
		phy_address = gsVpuWork_memInfo.phy_addr;
		break;
	}

	return phy_address;
}

static phys_addr_t _vmem_request_seqheader_buff_phyaddr(int type,
					     void **remapped_addr,
					     unsigned int *request_size)
{
	int enc_type = type - VPU_ENC;

	V_DBG(VPU_DBG_MEM_SEQ, "request seqheader type[%d]-enc_type[%d]", type,
	      enc_type);
	if (enc_type < 0 || enc_type >= VPU_ENC_MAX_CNT)
		return 0;
#if DEFINED_CONFIG_VENC_CNT_12345678
	*remapped_addr = gsVpuEncSeqheader_memInfo[enc_type].kernel_remap_addr;
	*request_size = gsVpuEncSeqheader_memInfo[enc_type].request_size;

	V_DBG(VPU_DBG_MEM_SEQ, "request seqheader remap = 0x%p, size = 0x%x !!",
	      gsVpuEncSeqheader_memInfo[enc_type].kernel_remap_addr,
	      gsVpuEncSeqheader_memInfo[enc_type].request_size);
	return gsVpuEncSeqheader_memInfo[enc_type].phy_addr;
#else
	return 0;
#endif
}

static phys_addr_t _vmem_request_userdata_buff_phyaddr(int type,
					       void **remapped_addr,
					     unsigned int *request_size)
{
	if (type < 0 || type >= VPU_INST_MAX)
		return 0;

	*remapped_addr = gsVpuUserData_memInfo[type].kernel_remap_addr;
	*request_size = gsVpuUserData_memInfo[type].request_size;

	return gsVpuUserData_memInfo[type].phy_addr;
}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
static phys_addr_t _vmem_request_ps_buff_phyaddr(int type,
					      void **remapped_addr,
					      unsigned int *request_size)
{
	if (type < 0 || type >= VPU_INST_MAX)
		return 0;

	*remapped_addr = gsPs_memInfo[type].kernel_remap_addr;
	*request_size = gsPs_memInfo[type].request_size;

	return gsPs_memInfo[type].phy_addr;
}

static phys_addr_t _vmem_request_slice_buff_phyaddr(int type,
					    void **remapped_addr,
					    unsigned int *request_size)
{
	if (type < 0 || type >= VPU_INST_MAX)
		return 0;

	*remapped_addr = gsSlice_memInfo[type].kernel_remap_addr;
	*request_size = gsSlice_memInfo[type].request_size;

	return gsSlice_memInfo[type].phy_addr;
}

static phys_addr_t _vmem_request_stream_buff_phyaddr(int type,
					     void **remapped_addr,
					     unsigned int *request_size)
{
	if (type < 0 || type >= VPU_INST_MAX)
		return 0;

	*remapped_addr = gsStream_memInfo[type].kernel_remap_addr;
	*request_size = gsStream_memInfo[type].request_size;

	return gsStream_memInfo[type].phy_addr;
}
#endif

int _vmem_init_memory_info(void)
{
	int ret = 0;

	if (pmap_get_info("video", &pmap_video) < 0) {
		ret = -10;
		goto Error;
	}

	if (pmap_get_info("video_sw", &pmap_video_sw) < 0) {
		ret = -11;
		goto Error;
	}

	ptr_front_addr_mem = (phys_addr_t) pmap_video.base;
	sz_remained_mem = pmap_video.size;

	V_DBG(VPU_DBG_MEMORY,
	      "pmap_video(0x%llx - 0x%llx(%llu) ) sw(0x%llx - 0x%llx(%llu) ) in total(0x%x(%u) ) !!",
	      pmap_video.base, pmap_video.size, pmap_video.size,
	      pmap_video_sw.base, pmap_video_sw.size, pmap_video_sw.size,
	      VPU_SW_ACCESS_REGION_SIZE, VPU_SW_ACCESS_REGION_SIZE);

	if (pmap_video_sw.size < VPU_SW_ACCESS_REGION_SIZE) {
		V_DBG(VPU_DBG_ERROR,
		      "video_sw is not enough => 0x%llx(%llu) < 0x%x(%u)",
		      pmap_video_sw.size, pmap_video_sw.size,
		      VPU_SW_ACCESS_REGION_SIZE, VPU_SW_ACCESS_REGION_SIZE);
		ret = -1;
		goto Error;
	}

	ptr_sw_addr_mem = pmap_video_sw.base;
	sz_sw_mem = pmap_video_sw.size;

	ptr_rear_addr_mem = ptr_front_addr_mem + sz_remained_mem;

#if DEFINED_CONFIG_VENC_CNT_12345678
	{
		if (pmap_get_info("enc_main", &pmap_enc) < 0) {
			ret = -12;
			goto Error;
		}
		sz_enc_mem = pmap_enc.size;
		V_DBG(VPU_DBG_MEMORY, "size of enc_main: 0x%x (%llu)",
		      sz_enc_mem, pmap_enc.size);
	}
	only_decmode = 0;
#else
	sz_enc_mem = 0;
	only_decmode = 1;
#endif

	if (sz_remained_mem < sz_enc_mem) {
		ret = -2;
		goto Error;
	}
	ptr_ext_addr_mem = ptr_rear_addr_mem - sz_enc_mem;

	if (ptr_front_addr_mem < pmap_video.base
	    || (ptr_front_addr_mem > (pmap_video.base + pmap_video.size))) {
		ret = -3;
		goto Error;
	}

	if (ptr_rear_addr_mem < pmap_video.base
	    || (ptr_rear_addr_mem > (pmap_video.base + pmap_video.size))) {
		ret = -4;
		goto Error;
	}

	sz_front_used_mem = sz_rear_used_mem = sz_ext_used_mem = 0;
	V_DBG(VPU_DBG_MEMORY,
	      "used memory: front - 0x%x(%u) / ext - 0x%x(%u) / rear - 0x%x(%u) !!",
	      ptr_front_addr_mem, ptr_front_addr_mem, ptr_ext_addr_mem,
	      ptr_ext_addr_mem, ptr_rear_addr_mem, ptr_rear_addr_mem);

#if DEFINED_CONFIG_VDEC_CNT_345
	// Additional decoder :: ext2,3
	if (pmap_get_info("video_ext", &pmap_video_ext) < 0) {
		ret = -13;
		goto Error;
	}

	ptr_ext_front_addr_mem = (phys_addr_t) pmap_video_ext.base;
	sz_ext_remained_mem = pmap_video_ext.size;

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	ptr_ext_rear_addr_mem =
	    ptr_ext_front_addr_mem + sz_ext_remained_mem -
	    (PAGE_ALIGN(PS_SAVE_SIZE) + PAGE_ALIGN(SLICE_SAVE_SIZE) +
	     PAGE_ALIGN(LARGE_STREAM_BUF_SIZE));
	sz_ext_remained_mem -=
	    (PAGE_ALIGN(PS_SAVE_SIZE) + PAGE_ALIGN(SLICE_SAVE_SIZE) +
	     PAGE_ALIGN(LARGE_STREAM_BUF_SIZE));
#else
	ptr_ext_rear_addr_mem = ptr_ext_front_addr_mem + sz_ext_remained_mem;
#endif

	sz_ext_front_used_mem = sz_ext_rear_used_mem = 0;

	if (ptr_ext_front_addr_mem < pmap_video_ext.base
	    || (ptr_ext_front_addr_mem >
		(pmap_video_ext.base + pmap_video_ext.size))) {
		ret = -5;
		goto Error;
	}
#endif

#if defined(CONFIG_VDEC_CNT_5)
	// Additional decoder :: ext4
	if (pmap_get_info("video_ext2", &pmap_video_ext2) < 0) {
		ret = -14;
		goto Error;
	}

	ptr_ext2_front_addr_mem = (phys_addr_t) pmap_video_ext2.base;
	sz_ext2_remained_mem = pmap_video_ext2.size;
	sz_ext2_front_used_mem = 0;

	if (ptr_ext2_front_addr_mem < pmap_video_ext2.base
	    || (ptr_ext2_front_addr_mem >
		(pmap_video_ext2.base + pmap_video_ext2.size))) {
		ret = -6;
		goto Error;
	}
#endif

// Additional encoder :: ext, ext2, ext3, ext4, ext5, ext6, ext7
#if DEFINED_CONFIG_VENC_CNT_2345678
	if (pmap_get_info("enc_ext", &pmap_enc_ext[0]) < 0) {
		ret = -15;
		goto Error;
	}
	ptr_enc_ext_addr_mem[0] = (phys_addr_t) pmap_enc_ext[0].base;
	sz_enc_ext_remained_mem[0] = pmap_enc_ext[0].size;
	sz_enc_ext_used_mem[0] = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_345678
	if (pmap_get_info("enc_ext2", &pmap_enc_ext[1]) < 0) {
		ret = -16;
		goto Error;
	}
	ptr_enc_ext_addr_mem[1] = (phys_addr_t) pmap_enc_ext[1].base;
	sz_enc_ext_remained_mem[1] = pmap_enc_ext[1].size;
	sz_enc_ext_used_mem[1] = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_45678
	if (pmap_get_info("enc_ext3", &pmap_enc_ext[2]) < 0) {
		ret = -17;
		goto Error;
	}
	ptr_enc_ext_addr_mem[2] = (phys_addr_t) pmap_enc_ext[2].base;
	sz_enc_ext_remained_mem[2] = pmap_enc_ext[2].size;
	sz_enc_ext_used_mem[2] = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_5678
	if (pmap_get_info("enc_ext4", &pmap_enc_ext[3]) < 0) {
		ret = -17;
		goto Error;
	}
	ptr_enc_ext_addr_mem[3] = (phys_addr_t)pmap_enc_ext[3].base;
	sz_enc_ext_remained_mem[3] = pmap_enc_ext[3].size;
	sz_enc_ext_used_mem[3] = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_678
	if (pmap_get_info("enc_ext5", &pmap_enc_ext[4]) < 0) {
		ret = -17;
		goto Error;
	}
	ptr_enc_ext_addr_mem[4] = (phys_addr_t)pmap_enc_ext[4].base;
	sz_enc_ext_remained_mem[4] = pmap_enc_ext[4].size;
	sz_enc_ext_used_mem[4] = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_78
	if (pmap_get_info("enc_ext6", &pmap_enc_ext[5]) < 0) {
		ret = -17;
		goto Error;
	}
	ptr_enc_ext_addr_mem[5] = (phys_addr_t)pmap_enc_ext[5].base;
	sz_enc_ext_remained_mem[5] = pmap_enc_ext[5].size;
	sz_enc_ext_used_mem[5] = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_8
	if (pmap_get_info("enc_ext7", &pmap_enc_ext[6]) < 0) {
		ret = -17;
		goto Error;
	}
	ptr_enc_ext_addr_mem[6] = (phys_addr_t)pmap_enc_ext[6].base;
	sz_enc_ext_remained_mem[6] = pmap_enc_ext[6].size;
	sz_enc_ext_used_mem[6] = 0;
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
	V_DBG(VPU_DBG_MEM_SEQ, "enter");

	if (pmap_video.base) {
		pmap_release_info("video");	// pmap_video
		pmap_video.base = 0;
	}

	if (pmap_video_sw.base) {
		pmap_release_info("video_sw");	// pmap_video_sw
		pmap_video_sw.base = 0;
	}

#if DEFINED_CONFIG_VENC_CNT_12345678
	if (pmap_enc.base) {
		pmap_release_info("enc_main");	// pmap_enc
		pmap_enc.base = 0;
	}
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	if (pmap_video_ext.base) {
		pmap_release_info("video_ext");	// pmap_video_ext
		pmap_video_ext.base = 0;
	}
#endif

#if defined(CONFIG_VDEC_CNT_5)
	if (pmap_video_ext2.base) {
		pmap_release_info("video_ext2");	// pmap_video_ext2
		pmap_video_ext2.base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_2345678
	if (pmap_enc_ext[0].base) {
		pmap_release_info("enc_ext");	// pmap_enc_ext[0]
		pmap_enc_ext[0].base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_345678
	if (pmap_enc_ext[1].base) {
		pmap_release_info("enc_ext2");	// pmap_enc_ext[1]
		pmap_enc_ext[1].base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_45678
	if (pmap_enc_ext[2].base) {
		pmap_release_info("enc_ext3");	// pmap_enc_ext[2]
		pmap_enc_ext[2].base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_5678
	if (pmap_enc_ext[3].base) {
		pmap_release_info("enc_ext4");	// pmap_enc_ext[3]
		pmap_enc_ext[3].base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_678
	if (pmap_enc_ext[4].base) {
		pmap_release_info("enc_ext5");	// pmap_enc_ext[4]
		pmap_enc_ext[4].base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_78
	if (pmap_enc_ext[5].base) {
		pmap_release_info("enc_ext6");	// pmap_enc_ext[5]
		pmap_enc_ext[5].base = 0;
	}
#endif

#if DEFINED_CONFIG_VENC_CNT_8
	if (pmap_enc_ext[6].base) {
		pmap_release_info("enc_ext7");	// pmap_enc_ext[6]
		pmap_enc_ext[6].base = 0;
	}
#endif

	return 0;
}

int _vmem_is_cma_within_a_region(void *start_addr,
					void *end_addr,
					void *cmp_addr,
					unsigned int cmp_length)
{
	if (cmp_addr != 0)
		if ((start_addr >= cmp_addr)
			&& (end_addr <= (cmp_addr + cmp_length - 1)))
			return 1;
	return 0;
}

int _vmem_is_cma_allocated_virt_region(const void *start_virtaddr,
					unsigned int length)
{
	int i, type;
	void *end_virtaddr = (void *)start_virtaddr + length - 1;

	for (type = 0; type < VPU_MAX; type++) {
		if (vmem_allocated_count[type] > 0) {
			for (i = vmem_allocated_count[type]; i > 0; i--) {
				if (_vmem_is_cma_within_a_region(
					(void *)start_virtaddr,
					end_virtaddr,
					vmem_alloc_info[type][i-1]
						.kernel_remap_addr,
					vmem_alloc_info[type][i-1]
						.request_size)) {
					if (_vmem_is_cma_allocated_phy_region(
						vmem_alloc_info[type][i-1]
							.phy_addr,
						vmem_alloc_info[type][i-1]
							.request_size))
						return 1;
					else
						return 0;
				}
			}
		}
	}

	return 0;
}

int _vmem_is_cma_allocated_phy_region(unsigned int start_phyaddr,
				      unsigned int length)
{
	unsigned int end_phyaddr = start_phyaddr + length - 1;

	// pmap_video
	if ((start_phyaddr >= pmap_video.base)
	    && (end_phyaddr <= (pmap_video.base + pmap_video.size - 1)))
		return pmap_is_cma_alloc(&pmap_video);

	// pmap_video_sw
	if ((start_phyaddr >= pmap_video_sw.base)
	    && (end_phyaddr <= (pmap_video_sw.base + pmap_video_sw.size - 1)))
		return pmap_is_cma_alloc(&pmap_video_sw);

#if DEFINED_CONFIG_VENC_CNT_12345678
	// pmap_enc
	if ((start_phyaddr >= pmap_enc.base)
	    && (end_phyaddr <= (pmap_enc.base + pmap_enc.size - 1)))
		return pmap_is_cma_alloc(&pmap_enc);
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	// pmap_video_ext
	if ((start_phyaddr >= pmap_video_ext.base)
	    && (end_phyaddr <= (pmap_video_ext.base + pmap_video_ext.size - 1)))
		return pmap_is_cma_alloc(&pmap_video_ext);
#endif

#if defined(CONFIG_VDEC_CNT_5)
	// pmap_video_ext2
	if ((start_phyaddr >= pmap_video_ext2.base)
	    && (end_phyaddr <=
		(pmap_video_ext2.base + pmap_video_ext2.size - 1)))
		return pmap_is_cma_alloc(&pmap_video_ext2);
#endif

#if DEFINED_CONFIG_VENC_CNT_2345678
	// pmap_enc_ext[0]
	if ((start_phyaddr >= pmap_enc_ext[0].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[0].base + pmap_enc_ext[0].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[0]);
#endif

#if DEFINED_CONFIG_VENC_CNT_345678
	//pmap_enc_ext[1]
	if ((start_phyaddr >= pmap_enc_ext[1].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[1].base + pmap_enc_ext[1].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[1]);
#endif

#if DEFINED_CONFIG_VENC_CNT_45678
	// pmap_enc_ext[2]
	if ((start_phyaddr >= pmap_enc_ext[2].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[2].base + pmap_enc_ext[2].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[2]);
#endif

#if DEFINED_CONFIG_VENC_CNT_5678
	// pmap_enc_ext[3]
	if ((start_phyaddr >= pmap_enc_ext[3].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[3].base + pmap_enc_ext[3].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[3]);
#endif

#if DEFINED_CONFIG_VENC_CNT_678
	// pmap_enc_ext[4]
	if ((start_phyaddr >= pmap_enc_ext[4].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[4].base + pmap_enc_ext[4].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[4]);
#endif

#if DEFINED_CONFIG_VENC_CNT_78
	// pmap_enc_ext[5]
	if ((start_phyaddr >= pmap_enc_ext[5].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[5].base + pmap_enc_ext[5].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[5]);
#endif

#if DEFINED_CONFIG_VENC_CNT_8
	// pmap_enc_ext[6]
	if ((start_phyaddr >= pmap_enc_ext[6].base)
	    && (end_phyaddr <=
		(pmap_enc_ext[6].base + pmap_enc_ext[6].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[6]);
#endif

	return 0;
}

int vmem_proc_alloc_memory(int codec_type, MEM_ALLOC_INFO_t *alloc_info,
			   vputype type)
{
	mutex_lock(&mem_mutex);
	V_DBG(VPU_DBG_MEM_SEQ, "type[%d]-buffer[%d] : vmgr_proc_alloc_memory!!",
		type, alloc_info->buffer_type);

	// START :: Dedicated memory region   ******
	switch (alloc_info->buffer_type) {
#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
#ifdef FIXED_PS_SLICE_BUFFER
	case BUFFER_PS:
		alloc_info->phy_addr
		 = _vmem_request_ps_buff_phyaddr(
		     type,
		     &(alloc_info->kernel_remap_addr),
		     &(alloc_info->request_size));
		if (alloc_info->phy_addr != 0)
			goto Success;
		break;
	case BUFFER_SLICE:
		alloc_info->phy_addr
		 = _vmem_request_slice_buff_phyaddr(
		    type,
		    &(alloc_info->kernel_remap_addr),
		    &(alloc_info->request_size));
		if (alloc_info->phy_addr != 0)
			goto Success;
		break;
#endif
#ifdef FIXED_STREAM_BUFFER
	case BUFFER_STREAM:
		alloc_info->phy_addr
		 = _vmem_request_stream_buff_phyaddr(
		    type,
		    &(alloc_info->kernel_remap_addr),
		    &(alloc_info->request_size));
		if (alloc_info->phy_addr != 0)
			goto Success;
		break;
#endif
#endif
	default:
		break;
	}

	if (alloc_info->buffer_type == BUFFER_WORK) {
		alloc_info->phy_addr
		 = _vmem_request_workbuff_phyaddr(
		    codec_type,
		    &(alloc_info->kernel_remap_addr));
		if (alloc_info->phy_addr == 0) {
			V_DBG(VPU_DBG_ERROR,
			      "type[%d]-buffer[%d] : [0x%x] BUFFER_WORK ALLOC_MEMORY failed.",
			      type, alloc_info->buffer_type,
			      alloc_info->phy_addr);
			goto Error;
		}
	} else if (alloc_info->buffer_type == BUFFER_USERDATA) {
		alloc_info->phy_addr
		 = _vmem_request_userdata_buff_phyaddr(
		    type,
		    &(alloc_info->kernel_remap_addr),
		    &(alloc_info->request_size));
		if (alloc_info->phy_addr == 0) {
			V_DBG(VPU_DBG_ERROR,
			      "type[%d]-buffer[%d] : [0x%x] BUFFER_USERDATA ALLOC_MEMORY failed.",
			      type, alloc_info->buffer_type,
			      alloc_info->phy_addr);
			goto Error;
		}
	} else if (alloc_info->buffer_type == BUFFER_SEQHEADER &&
			(type == VPU_ENC ||
				type == VPU_ENC_EXT ||
				type == VPU_ENC_EXT2 ||
				type == VPU_ENC_EXT3 ||
				type == VPU_ENC_EXT4 ||
				type == VPU_ENC_EXT5 ||
				type == VPU_ENC_EXT6 ||
				type == VPU_ENC_EXT7)) {	//Encoder
		alloc_info->phy_addr
		 = _vmem_request_seqheader_buff_phyaddr(type,
				&(alloc_info->kernel_remap_addr),
				&(alloc_info->request_size));
		if (alloc_info->phy_addr == 0) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d]-buffer[%d] : [0x%x] BUFFER_SEQHEADER ALLOC_MEMORY failed.",
				type, alloc_info->buffer_type,
				alloc_info->phy_addr);
			goto Error;
		}
	} else {	// END :: Dedicated memory region   ******
#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
		alloc_info->phy_addr
		 = _vmem_request_phyaddr(
		   alloc_info->request_size, type);
		if (alloc_info->phy_addr == 0) {
			V_DBG(VPU_DBG_ERROR,
				"type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.",
				type, alloc_info->buffer_type,
				alloc_info->phy_addr);
			goto Error;
		}
#else
		if (type == VPU_DEC_EXT4) {
#if defined(CONFIG_VDEC_CNT_5)
			alloc_info->phy_addr
			 = _vmem_request_phyaddr_dec_ext4(
			   alloc_info->request_size, type);
			if (alloc_info->phy_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.",
					type, alloc_info->buffer_type,
					alloc_info->phy_addr);
				goto Error;
			}
#endif
		} else if (type == VPU_DEC_EXT2 || type == VPU_DEC_EXT3) {
#if DEFINED_CONFIG_VDEC_CNT_345
			alloc_info->phy_addr
			 = _vmem_request_phyaddr_dec_ext23(
			 alloc_info->request_size, type);
			if (alloc_info->phy_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.",
					type, alloc_info->buffer_type,
					alloc_info->phy_addr);
				goto Error;
			}
#endif
		} else if (type == VPU_ENC_EXT || type == VPU_ENC_EXT2   ||
			type == VPU_ENC_EXT3 || type == VPU_ENC_EXT4 ||
			type == VPU_ENC_EXT5 || type == VPU_ENC_EXT6 ||
			type == VPU_ENC_EXT7) {
#if DEFINED_CONFIG_VENC_CNT_2345678
			alloc_info->phy_addr =
				_vmem_request_phyaddr_enc_ext(
					alloc_info->request_size, type);
			if (alloc_info->phy_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.",
					type, alloc_info->buffer_type,
					alloc_info->phy_addr);
				goto Error;
			}
#endif
		} else {
			alloc_info->phy_addr =
				_vmem_request_phyaddr(
					alloc_info->request_size, type);
			if (alloc_info->phy_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d]-buffer[%d] : [0x%x] ALLOC_MEMORY failed.",
					type, alloc_info->buffer_type,
					alloc_info->phy_addr);
				goto Error;
			}
		}
#endif
		alloc_info->kernel_remap_addr = 0x0;
		if (alloc_info->buffer_type != BUFFER_FRAMEBUFFER) {
			alloc_info->kernel_remap_addr =
			    _vmem_get_virtaddr(alloc_info->phy_addr,
					       alloc_info->request_size);
			if (alloc_info->kernel_remap_addr == 0) {
				V_DBG(VPU_DBG_ERROR,
					"type[%d]-buffer[%d] : phy[0x%x - 0x%x] remap ALLOC_MEMORY failed.",
					type, alloc_info->buffer_type,
					alloc_info->phy_addr,
					PAGE_ALIGN(
					alloc_info->request_size
					/*-PAGE_SIZE*/));
				memcpy(
				  &(vmem_alloc_info[type]
				     [vmem_allocated_count[type]]),
				  alloc_info, sizeof(MEM_ALLOC_INFO_t));
				vmem_allocated_count[type] += 1;
				goto Error;
			}
		}

		V_DBG(VPU_DBG_MEM_SEQ,
		"type[%d]-buffer[%d] : alloc addr[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x(%u) !!",
		type, alloc_info->buffer_type, vmem_allocated_count[type],
		alloc_info->phy_addr, alloc_info->kernel_remap_addr,
		alloc_info->request_size, alloc_info->request_size);
		memcpy(&(vmem_alloc_info[type][vmem_allocated_count[type]]),
		       alloc_info, sizeof(MEM_ALLOC_INFO_t));
		vmem_allocated_count[type] += 1;
	}

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
Success:
#endif

	if (alloc_info->buffer_type == BUFFER_STREAM)
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

		V_DBG(VPU_DBG_MEM_SEQ, "type[%d] : vmgr_proc_free_memory!!",
		      type);

		for (i = vmem_allocated_count[type]; i > 0; i--) {

			if (vmem_alloc_info[type][i-1].kernel_remap_addr != 0)
				_vmem_release_virtaddr(
				((void *)
				vmem_alloc_info[type][i-1].kernel_remap_addr),
				vmem_alloc_info[type][i-1].phy_addr,
				vmem_alloc_info[type][i-1].request_size);
#ifdef CONFIG_VPU_ALLOC_MEM_IN_SPECIFIC_SEQUENCE
			_vmem_release_phyaddr(
				vmem_alloc_info[type][i-1].phy_addr,
				vmem_alloc_info[type][i-1].request_size,
				type);
#else
			if (type == VPU_DEC_EXT4) {
#if defined(CONFIG_VDEC_CNT_5)
				_vmem_release_phyaddr_dec_ext4(
				vmem_alloc_info[type][i-1].phy_addr,
				vmem_alloc_info[type][i-1].request_size,
				type);
#endif
			} else if (type == VPU_DEC_EXT2
				|| type == VPU_DEC_EXT3) {
#if DEFINED_CONFIG_VDEC_CNT_345
				_vmem_release_phyaddr_dec_ext23(
				vmem_alloc_info[type][i-1].phy_addr,
				vmem_alloc_info[type][i-1].request_size,
				type);
#endif
			} else if (type == VPU_ENC_EXT ||
				type == VPU_ENC_EXT2 ||
				type == VPU_ENC_EXT3 ||
				type == VPU_ENC_EXT4 ||
				type == VPU_ENC_EXT5 ||
				type == VPU_ENC_EXT6 ||
				type == VPU_ENC_EXT7) {
#if DEFINED_CONFIG_VENC_CNT_2345678
				_vmem_release_phyaddr_enc_ext(
				vmem_alloc_info[type][i-1].phy_addr,
				vmem_alloc_info[type][i-1].request_size,
				type);
#endif
			} else {
				_vmem_release_phyaddr(
				vmem_alloc_info[type][i-1].phy_addr,
				vmem_alloc_info[type][i-1].request_size,
				type);
			}
#endif
			V_DBG(VPU_DBG_MEM_SEQ,
			  "type[%d]-buffer[%d] : free addr[%d] :: phy = 0x%x, remap = 0x%p, size = 0x%x !!",
			  type,
			  vmem_alloc_info[type][i-1].buffer_type,
			  i - 1,
			  vmem_alloc_info[type][i-1].phy_addr,
			  vmem_alloc_info[type][i-1].kernel_remap_addr,
			  vmem_alloc_info[type][i-1].request_size);
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

	if (type == VPU_DEC) {
		if (vmgr_get_close(VPU_DEC_EXT) && vmgr_get_close(VPU_ENC)
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		    && vmgr_hevc_enc_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		    && vmgr_4k_d2_get_close(VPU_DEC_EXT)
		    && vmgr_4k_d2_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		    && hmgr_get_close(VPU_DEC_EXT)
			&& hmgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		    && vp9mgr_get_close(VPU_DEC_EXT)
		    && vp9mgr_get_close(VPU_ENC)
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
		    && jmgr_get_close(VPU_DEC_EXT)
			&& jmgr_get_close(VPU_ENC)
#endif
		    ) {
			szFreeMem = sz_remained_mem;
		} else {
			if (only_decmode)
				szFreeMem = sz_remained_mem;
			else
				szFreeMem =
				    (pmap_video.size - sz_enc_mem -
				     VPU_SW_ACCESS_REGION_SIZE) -
				    (sz_ext_used_mem + sz_front_used_mem);
		}
	} else if (type == VPU_DEC_EXT) {
		if (only_decmode)
			szFreeMem = sz_remained_mem;
		else
			szFreeMem =
			    (pmap_video.size - sz_enc_mem -
			     VPU_SW_ACCESS_REGION_SIZE) - (sz_ext_used_mem +
							   sz_front_used_mem);
	} else if (type == VPU_ENC) {
		szFreeMem = sz_enc_mem - sz_rear_used_mem;
	} else if (type == VPU_DEC_EXT2 || type == VPU_DEC_EXT3) {
#if DEFINED_CONFIG_VDEC_CNT_345
		szFreeMem = sz_ext_remained_mem;
#else
		szFreeMem = 0;
#endif
	} else if (type == VPU_DEC_EXT4) {
#if defined(CONFIG_VDEC_CNT_5)
		szFreeMem = sz_ext2_remained_mem;
#else
		szFreeMem = 0;
#endif
	} else if (type == VPU_ENC_EXT || type == VPU_ENC_EXT2   ||
		type == VPU_ENC_EXT3 || type == VPU_ENC_EXT4 ||
		type == VPU_ENC_EXT5 || type == VPU_ENC_EXT6 ||
		type == VPU_ENC_EXT7) {
#if DEFINED_CONFIG_VENC_CNT_2345678
		szFreeMem = sz_enc_ext_remained_mem[type - VPU_ENC_EXT];
#else
		szFreeMem = 0;
#endif
	} else {
		szFreeMem = sz_remained_mem;
	}

	V_DBG(VPU_DBG_MEM_SEQ,
		"type(%d) free(0x%x) :: etc_info = enc(0x%x), front_used(0x%x), ext_used(0x%x), rear_used(0x%x)",
		type, szFreeMem, sz_enc_mem, sz_front_used_mem, sz_ext_used_mem,
		sz_rear_used_mem);
	return szFreeMem;
}
EXPORT_SYMBOL(vmem_get_free_memory);

unsigned int vmem_get_freemem_size(vputype type)
{
	if (type <= VPU_DEC_EXT4) {
		V_DBG(VPU_DBG_MEM_SEQ,
		      "type[%d] mem info for vpu :: remain(0x%x : 0x%x : 0x%x), used mem(0x%x/0x%x, 0x%x : 0x%x/0x%x : 0x%x)",
		      type, sz_remained_mem,
#if DEFINED_CONFIG_VDEC_CNT_345
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
#if DEFINED_CONFIG_VDEC_CNT_345
		      sz_ext_front_used_mem, sz_ext_rear_used_mem
#else
		      0, 0
#endif
#if defined(CONFIG_VDEC_CNT_5)
		      , sz_ext2_front_used_mem
#else
		      , 0
#endif
		    );
	} else if (type >= VPU_ENC && type < VPU_MAX) {
		V_DBG(VPU_DBG_MEM_SEQ,
			"type[%d] mem info for vpu :: remain(%u : %u : %u : %u : %u : %u : %u : %u), used mem(%u : %u : %u : %u : %u : %u : %u : %u)",
			type,
			sz_enc_mem - sz_rear_used_mem,
#if defined(CONFIG_VENC_CNT_8)
			sz_enc_ext_remained_mem[0],
			sz_enc_ext_remained_mem[1],
			sz_enc_ext_remained_mem[2],
			sz_enc_ext_remained_mem[3],
			sz_enc_ext_remained_mem[4],
			sz_enc_ext_remained_mem[5],
			sz_enc_ext_remained_mem[6],
#elif defined(CONFIG_VENC_CNT_7)
			sz_enc_ext_remained_mem[0],
			sz_enc_ext_remained_mem[1],
			sz_enc_ext_remained_mem[2],
			sz_enc_ext_remained_mem[3],
			sz_enc_ext_remained_mem[4],
			sz_enc_ext_remained_mem[5], 0,
#elif defined(CONFIG_VENC_CNT_6)
			sz_enc_ext_remained_mem[0],
			sz_enc_ext_remained_mem[1],
			sz_enc_ext_remained_mem[2],
			sz_enc_ext_remained_mem[3],
			sz_enc_ext_remained_mem[4], 0, 0,
#elif defined(CONFIG_VENC_CNT_5)
			sz_enc_ext_remained_mem[0],
			sz_enc_ext_remained_mem[1],
			sz_enc_ext_remained_mem[2],
			sz_enc_ext_remained_mem[3], 0, 0, 0,
#elif defined(CONFIG_VENC_CNT_4)
			sz_enc_ext_remained_mem[0],
			sz_enc_ext_remained_mem[1],
			sz_enc_ext_remained_mem[2], 0, 0, 0, 0,
#elif defined(CONFIG_VENC_CNT_3)
			sz_enc_ext_remained_mem[0],
			sz_enc_ext_remained_mem[1], 0, 0, 0, 0, 0,
#elif defined(CONFIG_VENC_CNT_2)
			sz_enc_ext_remained_mem[0], 0, 0, 0, 0, 0, 0,
#else
			0, 0, 0, 0, 0, 0, 0,
#endif
			sz_rear_used_mem,
#if defined(CONFIG_VENC_CNT_8)
			sz_enc_ext_used_mem[0],
			sz_enc_ext_used_mem[1],
			sz_enc_ext_used_mem[2],
			sz_enc_ext_used_mem[3],
			sz_enc_ext_used_mem[4],
			sz_enc_ext_used_mem[5],
			sz_enc_ext_used_mem[6]
#elif defined(CONFIG_VENC_CNT_7)
			sz_enc_ext_used_mem[0],
			sz_enc_ext_used_mem[1],
			sz_enc_ext_used_mem[2],
			sz_enc_ext_used_mem[3],
			sz_enc_ext_used_mem[4],
			sz_enc_ext_used_mem[5], 0
#elif defined(CONFIG_VENC_CNT_6)
			sz_enc_ext_used_mem[0],
			sz_enc_ext_used_mem[1],
			sz_enc_ext_used_mem[2],
			sz_enc_ext_used_mem[3],
			sz_enc_ext_used_mem[4], 0, 0
#elif defined(CONFIG_VENC_CNT_5)
			sz_enc_ext_used_mem[0],
			sz_enc_ext_used_mem[1],
			sz_enc_ext_used_mem[2],
			sz_enc_ext_used_mem[3], 0, 0, 0
#elif defined(CONFIG_VENC_CNT_4)
			sz_enc_ext_used_mem[0],
			sz_enc_ext_used_mem[1],
			sz_enc_ext_used_mem[2], 0, 0, 0, 0
#elif defined(CONFIG_VENC_CNT_3)
			sz_enc_ext_used_mem[0],
			sz_enc_ext_used_mem[1], 0, 0, 0, 0, 0
#elif defined(CONFIG_VENC_CNT_2)
			sz_enc_ext_used_mem[0], 0, 0, 0, 0, 0, 0
#else
			0, 0, 0, 0, 0, 0, 0
#endif
		);
	} else {
		V_DBG(VPU_DBG_ERROR, "mem_info :: unKnown type(%d)", type);
		return 0;
	}

	return vmem_get_free_memory(type);
}

void _vmem_config_zero(void)
{
	V_DBG(VPU_DBG_MEM_SEQ, "enter");

	pmap_video.base = 0;
	pmap_video_sw.base = 0;

#if DEFINED_CONFIG_VENC_CNT_12345678
	pmap_enc.base = 0;
#endif
#if DEFINED_CONFIG_VDEC_CNT_345
	pmap_video_ext.base = 0;
#endif
#if defined(CONFIG_VDEC_CNT_5)
	pmap_video_ext2.base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_2345678
	pmap_enc_ext[0].base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_345678
	pmap_enc_ext[1].base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_45678
	pmap_enc_ext[2].base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_5678
	pmap_enc_ext[3].base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_678
	pmap_enc_ext[4].base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_78
	pmap_enc_ext[5].base = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_8
	pmap_enc_ext[6].base = 0;
#endif

	sz_front_used_mem = sz_rear_used_mem = sz_ext_used_mem = 0;
	sz_remained_mem = sz_enc_mem = 0;

#if DEFINED_CONFIG_VDEC_CNT_345
	sz_ext_front_used_mem = sz_ext_rear_used_mem = 0;
	sz_ext_remained_mem = 0;
#endif
#if defined(CONFIG_VDEC_CNT_5)
	sz_ext2_front_used_mem = 0;
	sz_ext2_remained_mem = 0;
#endif
#if DEFINED_CONFIG_VENC_CNT_2345678
	sz_enc_ext_used_mem[0] = sz_enc_ext_used_mem[1] =
	sz_enc_ext_used_mem[2] = sz_enc_ext_used_mem[3] =
	sz_enc_ext_used_mem[4] = sz_enc_ext_used_mem[5] =
	sz_enc_ext_used_mem[6] = 0;
	sz_enc_ext_remained_mem[0] = sz_enc_ext_remained_mem[1] =
	sz_enc_ext_remained_mem[2] = sz_enc_ext_remained_mem[3] =
	sz_enc_ext_remained_mem[4] = sz_enc_ext_remained_mem[5] =
	sz_enc_ext_remained_mem[6] = 0;
#endif

// Memory Management!!
	memset(&gsVpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#ifdef CONFIG_SUPPORT_TCC_JPU
	memset(&gsJpuWork_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	memset(&gsVpu4KD2Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	memset(&gsWave410_Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	memset(&gsG2V2_Vp9Work_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t));
#endif

#if DEFINED_CONFIG_VENC_CNT_12345678
	memset(&gsVpuEncSeqheader_memInfo, 0x00,
		sizeof(MEM_ALLOC_INFO_t) * VPU_ENC_MAX_CNT);
#endif

	memset(&gsVpuUserData_memInfo, 0x00,
		sizeof(MEM_ALLOC_INFO_t) * VPU_INST_MAX);

#if defined(CONFIG_TEST_VPU_DRAM_INTLV)
	memset(&gsPs_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t) * VPU_INST_MAX);
	memset(&gsSlice_memInfo, 0x00, sizeof(MEM_ALLOC_INFO_t) * VPU_INST_MAX);
	memset(&gsStream_memInfo, 0x00,
		sizeof(MEM_ALLOC_INFO_t) * VPU_INST_MAX);
#endif
}

int vmem_init(void)
{
	int ret = 0;

	mutex_lock(&mem_mutex);
	{
		if ((vmgr_opened() == 0)
#ifdef CONFIG_SUPPORT_TCC_JPU
		    && (jmgr_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		    && (vmgr_4k_d2_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		    && (hmgr_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		    && (vp9mgr_opened() == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		    && (vmgr_hevc_enc_opened() == 0)
#endif
		    ) {
			if (!atomic_read(&cntMem_Reference)) {
				V_DBG(VPU_DBG_MEM_SEQ,
				      "The total accessible region for VPU SW is %u.",
				      VPU_SW_ACCESS_REGION_SIZE);
				ret = _vmem_init_memory_info();
				if (ret < 0) {
					V_DBG(VPU_DBG_ERROR,
					      "_vmem_init_memory_info() is failure with %d error",
					      ret);
					goto Error1;
				}
				ret = _vmem_alloc_dedicated_buffer();
				if (ret < 0) {
					V_DBG(VPU_DBG_ERROR,
					      "_vmem_alloc_dedicated_buffer() is failure with %d error",
					      ret);
					goto Error0;
				}
			}
			goto Success;
		}

Error0:
		if (ret < 0)
			_vmem_free_dedicated_buffer();
Error1:
		if (ret < 0)
			_vmem_deinit_memory_info();

		if (ret < 0) {
			sz_front_used_mem = sz_rear_used_mem = sz_ext_used_mem =
			    0;
			sz_remained_mem = sz_enc_mem = 0;

#if DEFINED_CONFIG_VDEC_CNT_345
			sz_ext_front_used_mem = sz_ext_rear_used_mem = 0;
			sz_ext_remained_mem = 0;
#endif

#if defined(CONFIG_VDEC_CNT_5)
			sz_ext2_front_used_mem = 0;
			sz_ext2_remained_mem = 0;
#endif

#if DEFINED_CONFIG_VENC_CNT_2345678
			sz_enc_ext_used_mem[0] = sz_enc_ext_used_mem[1] =
			sz_enc_ext_used_mem[2] = sz_enc_ext_used_mem[3] =
			sz_enc_ext_used_mem[4] = sz_enc_ext_used_mem[5] =
			sz_enc_ext_used_mem[6] = 0;
			sz_enc_ext_remained_mem[0] =
			sz_enc_ext_remained_mem[1] =
			sz_enc_ext_remained_mem[2] =
			sz_enc_ext_remained_mem[3] =
			sz_enc_ext_remained_mem[4] =
			sz_enc_ext_remained_mem[5] =
			sz_enc_ext_remained_mem[6] = 0;
#endif
		}

Success:
		if (ret >= 0)
			atomic_inc(&cntMem_Reference);

		mutex_unlock(&mem_mutex);
	}

	return ret;
}

int vmem_config(void)
{
	if (gMemConfigDone == 0) {
		atomic_set(&cntMem_Reference, 0);
		mutex_init(&mem_mutex);
		_vmem_config_zero();
		gMemConfigDone = 1;
	}

	return 0;
}

void vmem_deinit(void)
{
	mutex_lock(&mem_mutex);
	V_DBG(VPU_DBG_MEM_SEQ, "enter (ref count: %d)",
		atomic_read(&cntMem_Reference));
	if (atomic_read(&cntMem_Reference) > 0)
		atomic_dec(&cntMem_Reference);
	else
		V_DBG(VPU_DBG_ERROR,
			"strange ref-count :: %d",
			atomic_read(&cntMem_Reference));

	if (!atomic_read(&cntMem_Reference)) {
		_vmem_free_dedicated_buffer();
		_vmem_deinit_memory_info();
		_vmem_config_zero();
		V_DBG(VPU_DBG_ERROR,
			"all memory for VPU were released. ref-count :: %d",
			atomic_read(&cntMem_Reference));
	}
	mutex_unlock(&mem_mutex);
}

void vmem_set_only_decode_mode(int bDec_only)
{
	mutex_lock(&mem_mutex);
	{
		if (vmem_allocated_count[VPU_DEC_EXT] == 0
		    && vmem_allocated_count[VPU_ENC] == 0) {
#if DEFINED_CONFIG_VENC_CNT_12345678
			only_decmode = bDec_only;
#else
			only_decmode = 1;
#endif
			V_DBG(VPU_DBG_MEM_SEQ, "Changed alloc_mode(%d)",
				only_decmode);
		} else {
			V_DBG(VPU_DBG_MEM_SEQ, "can't change mode. (%d/%d)",
				vmem_allocated_count[VPU_DEC_EXT],
				vmem_allocated_count[VPU_ENC]);
		}
	}
	mutex_unlock(&mem_mutex);
}

void vdec_get_instance(int *nIdx)
{
	mutex_lock(&mem_mutex);
	{
		int i, nInstance = -1;

		// check instance that they want.
		if (*nIdx >= 0 && *nIdx < VPU_INST_MAX) {
			if (vdec_used[*nIdx] == 0
			    && (vmem_get_free_memory(*nIdx + VPU_DEC) > 0))
				nInstance = *nIdx;
		}

		if (nInstance < 0) {
			for (i = 0; i < VPU_INST_MAX; i++) {
				if (vdec_used[i] == 0) {
					nInstance = i;
					break;
				}
			}
		}

		if (nInstance >= 0)
			vdec_used[nInstance] = 1;
		else
			V_DBG(VPU_DBG_ERROR,
				"failed to get new instance for decoder(%d)",
				nInstance);

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

		for (i = 0; i < VPU_INST_MAX; i++) {
			if (vdec_used[i] == 0
			    && (vmem_get_free_memory(i + VPU_DEC) > 0)) {
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
		if (nIdx >= 0 && nIdx < VPU_INST_MAX)
			vdec_used[nIdx] = 0;
		else
			V_DBG(VPU_DBG_ERROR,
				"failed to clear instance for decoder(%d)",
				nIdx);
	}
	mutex_unlock(&mem_mutex);
}
EXPORT_SYMBOL(vdec_clear_instance);

void venc_get_instance(int *nIdx)
{
	mutex_lock(&mem_mutex);
	{
		int i, nInstance = -1;

		// check instance that they want.
		if (*nIdx >= 0 && *nIdx < VPU_INST_MAX) {
			if (venc_used[*nIdx] == 0
			    && (vmem_get_free_memory(*nIdx + VPU_ENC) > 0))
				nInstance = *nIdx;
		}

		if (nInstance < 0) {
			for (i = 0; i < VPU_INST_MAX; i++) {
				if (venc_used[i] == 0) {
					nInstance = i;
					break;
				}
			}
		}

		if (nInstance >= 0)
			venc_used[nInstance] = 1;
		else
			V_DBG(VPU_DBG_ERROR,
				"failed to get new instance for encoder(%d)",
				nInstance);

		V_DBG(VPU_DBG_INSTANCE, "Instance(#%d) is taken", nInstance);

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

		for (i = 0; i < VPU_INST_MAX; i++) {
			if (venc_used[i] == 0
			    && (vmem_get_free_memory(i + VPU_ENC) > 0)) {
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
		if (nIdx >= 0 && nIdx < VPU_INST_MAX)
			venc_used[nIdx] = 0;
		else
			V_DBG(VPU_DBG_ERROR,
			      "failed to clear instance for encoder(%d)", nIdx);

		V_DBG(VPU_DBG_INSTANCE, "Instance is cleared #%d", nIdx);
	}
	mutex_unlock(&mem_mutex);
}
EXPORT_SYMBOL(venc_clear_instance);
