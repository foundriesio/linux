// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEC_H
#define VPU_DEC_H

#include "vpu_comm.h"

#if DEFINED_CONFIG_VDEC_CNT_12345
int vdec_probe(struct platform_device *pdev);
int vdec_remove(struct platform_device *pdev);
#endif

#endif /*VPU_DEC_H*/
