// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_ENC_H
#define VPU_ENC_H

#include "vpu_comm.h"

#if DEFINED_CONFIG_VENC_CNT_12345678
int venc_probe(struct platform_device *pdev);
int venc_remove(struct platform_device *pdev);
#endif

#endif /*VPU_ENC_H*/
