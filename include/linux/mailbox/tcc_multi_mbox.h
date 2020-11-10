// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_MULTI_MBOX_H
#define TCC_MULTI_MBOX_H

#if defined(CONFIG_ARCH_TCC803X)
	#include <linux/mailbox/tcc803x_multi_mailbox/tcc803x_multi_mbox.h>
#elif defined(CONFIG_ARCH_TCC805X)
	#include <linux/mailbox/tcc805x_multi_mailbox/tcc805x_multi_mbox.h>
#endif

#endif
