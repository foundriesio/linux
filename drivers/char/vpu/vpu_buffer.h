/*
 *   FileName : vpu_buffer.h
 *   Description :TCC VPU h/w block
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
