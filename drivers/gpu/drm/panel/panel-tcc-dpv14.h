// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#ifndef __PANEL_TCC_DPV14_H__
#define __PANEL_TCC_DPV14_H__

#if defined(CONFIG_DRM_PANEL_MAX968XX)
int panel_max968xx_reset(void);
int panel_max968xx_get_topology(unsigned char *num_of_ports);
#endif

#endif