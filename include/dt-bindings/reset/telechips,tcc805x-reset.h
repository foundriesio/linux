/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Telechips Inc.
 */

#ifndef DT_BINDINGS_TELECHIPS_TCC805X_RESET_H
#define DT_BINDINGS_TELECHIPS_TCC805X_RESET_H

#define RESET_CPU0		0UL
#define RESET_CPU1		1UL
#define RESET_CPUBUS		2UL
#define RESET_CMBUS		3UL
#define RESET_MEMBUS		4UL
#define RESET_VBUS		5UL
#define RESET_HSIOBUS		6UL
#define RESET_SMUBUS		7UL
#define RESET_3D		8UL
#define RESET_DDIBUS		9UL
#define RESET_2D		10UL
#define RESET_IOBUS		11UL
#define RESET_CODA		12UL
#define RESET_CHEVCDEC		13UL
/* Reserved Field		14UL */
#define RESET_BHEVCDEC		15UL
/* Reserved Field		16UL */
#define RESET_PCIE		17UL
/* Reserved Field		18UL */
#define RESET_STORAGE		19UL
/* Reserved Field		20UL */
#define RESET_CHEVCENC		21UL
#define RESET_BHEVCENC		22UL
#define RESET_USB30		23UL

#endif /* DT_BINDINGS_TELECHIPS_TCC805X_RESET_H */
