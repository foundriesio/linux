/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2020 Telechips Inc.
 */

#ifndef __DT_BINDINGS_TCC805X_BOOT_MODE_H
#define __DT_BINDINGS_TCC805X_BOOT_MODE_H

#define BOOT_COLD	(0x00UL)
#define BOOT_WATCHDOG	(0x01UL)
#define BOOT_PANIC	(0x02UL)

#define BOOT_NORMAL	(0x51UL)
#define BOOT_RECOVERY	(0x52UL)
#define BOOT_FASTBOOT	(0x50UL)
#define BOOT_DDR_CHECK	(0x54UL)
#define BOOT_TCUPDATE	(0x53UL)

#endif

