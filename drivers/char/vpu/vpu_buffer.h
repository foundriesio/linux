// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_BUFFER_H
#define VPU_BUFFER_H

#include "vpu_type.h"
#include <video/tcc/tcc_video_common.h>

int vmem_alloc_count(int type);
int vmem_proc_alloc_memory(int codec_type, MEM_ALLOC_INFO_t* alloc_info, vputype type);
int vmem_proc_free_memory(vputype type);
unsigned int vmem_get_free_memory(vputype type);
unsigned int vmem_get_freemem_size(vputype type);
int vmem_config(void);
int vmem_init(void);
void vmem_deinit(void);
void vmem_set_only_decode_mode(int bDec_only);

void vdec_get_instance(int* nIdx);
void vdec_clear_instance(int nIdx);
void vdec_check_instance_available(unsigned int* nAvailable_Inst);
void venc_get_instance(int* nIdx);
void venc_clear_instance(int nIdx);
void venc_check_instance_available(unsigned int* nAvailable_Inst);

#if defined(CONFIG_TCC_MEM)
int range_is_allowed(unsigned long pfn, unsigned long size);
#endif

pgprot_t vmem_get_pgprot(pgprot_t ulOldProt, unsigned long ulPageOffset);

#endif /*VPU_BUFFER_H*/
