/*
 *   FileName : vpu_buffer.h
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

#include "vpu_type.h"
#include <video/tcc/tcc_video_common.h>

#ifndef _VBUFFER_H_
#define _VBUFFER_H_

//////////////////////////////////////
// Memory Management!!
extern int vmem_proc_alloc_memory(int codec_type, MEM_ALLOC_INFO_t *alloc_info, vputype type);
extern int vmem_proc_free_memory(vputype type);
extern unsigned int vmem_get_free_memory(vputype type);
extern unsigned int vmem_get_freemem_size(vputype type);
extern int vmem_init(void);
extern void vmem_reinit(void);
extern void vmem_deinit(void);
extern void vmem_set_only_decode_mode(int bDec_only);

extern void vdec_get_instance(int *nIdx);
extern void vdec_clear_instance(int nIdx);
extern void vdec_check_instance_available(unsigned int *nAvailable_Inst);
extern void venc_get_instance(int *nIdx);
extern void venc_clear_instance(int nIdx);
extern void venc_check_instance_available(unsigned int *nAvailable_Inst);

#if defined(CONFIG_TCC_MEM)
extern int range_is_allowed(unsigned long pfn, unsigned long size);
#endif
extern pgprot_t vmem_get_pgprot(pgprot_t ulOldProt, unsigned long ulPageOffset);
#endif
