// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DT_TCC_MBOX_CH_H
#define DT_TCC_MBOX_CH_H

/*  MBOX0 : A53 <-> R5 */
#define TCC_MBOX0_CH_RESERVED		(0)
#define TCC_MBOX0_CH_IPC			(1)
#define TCC_MBOX0_CH_SUSPEND        (2)
#define TCC_MBOX0_CH_RESUME			(3)
#define TCC_MBOX0_CH_SNOR_UPDATE	(4)
#define TCC_MBOX0_CH_HLC			(5)
#define TCC_MBOX0_CH_HSM			(6)
#define TCC_MBOX0_CH_SND			(7)
#define TCC_MBOX0_CH_MAX			(8)

/*  MBOX1 : A53 <-> A7S */
#define TCC_MBOX1_CH_RESERVED		(0)
#define TCC_MBOX1_CH_IPC			(1)
#define TCC_MBOX1_CH_VIOC			(2)
#define TCC_MBOX1_CH_SND			(3)
#define TCC_MBOX1_CH_SWITCH			(4)
#define TCC_MBOX1_CH_TCHSHARE		(5)
#define TCC_MBOX1_CH_SCRSHARE		(6)
#define TCC_MBOX1_CH_HSM			(7)
#define TCC_MBOX1_CH_MAX			(8)

#define TCC_MBOX_CH_LIMIT			(8)

#endif
