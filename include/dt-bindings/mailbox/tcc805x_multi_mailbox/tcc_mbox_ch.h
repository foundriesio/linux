// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DT_TCC_MBOX_CH_H
#define DT_TCC_MBOX_CH_H

/* CPUBUS MBOX : A72 <-> A53 */
#define TCC_CB_MBOX_RESERVED		(0)
#define TCC_CB_MBOX_TEST		(1)
#define TCC_CB_MBOX_IPC			(2)
#define TCC_CB_MBOX_HSM			(3)
#define TCC_CB_MBOX_VIOC		(4)
#define TCC_CB_MBOX_SDR			(5)
#define TCC_CB_MBOX_SCRSHARE    (6)
#define TCC_CB_MBOX_TCHSHARE    (7)
#define TCC_CB_MBOX_MAX			(8)

/* MICOM MBOX0 : A72 <-> R5 */
#define TCC_MICOM_MBOX0_RESERVED	(0)
#define TCC_MICOM_MBOX0_TEST		(1)
#define TCC_MICOM_MBOX0_IPC		(2)
#define TCC_MICOM_MBOX0_HSM		(3)
#define TCC_MICOM_MBOX0_FWUG		(4)
#define TCC_MICOM_MBOX0_POWER		(5)
#define TCC_MICOM_MBOX0_MAX		(6)

/* MICOM MBOX1 : A53 <-> R5 */
#define TCC_MICOM_MBOX1_RESERVED	(0)
#define TCC_MICOM_MBOX1_TEST		(1)
#define TCC_MICOM_MBOX1_IPC		(2)
#define TCC_MICOM_MBOX1_HSM		(3)
#define TCC_MICOM_MBOX1_POWER		(4)
#define TCC_MICOM_MBOX1_CAMIPC		(5)
#define TCC_MICOM_MBOX1_MAX		(6)

/* HSM MBOX0 : A72 <-> HSM */
#define TCC_HSM_MBOX0_RESERVED		(0)
#define TCC_HSM_MBOX0			(1)
#define TCC_HSM_MBOX0_MAX		(2)

/* HSM MBOX1 : A53 <-> HSM */
#define TCC_HSM_MBOX1_RESERVED		(0)
#define TCC_HSM_MBOX1			(1)
#define TCC_HSM_MBOX1_MAX		(2)


/* CM4 MBOX0 : A72 <-> CM4 */
#define TCC_CM4_MBOX0_RESERVED		(0)
#define TCC_CM4_MBOX0			(1)
#define TCC_CM4_MBOX0_MAX		(2)

/* CM4 MBOX1 : A53 <-> CM4 */
#define TCC_CM4_MBOX1_RESERVED		(0)
#define TCC_CM4_MBOX1			(1)
#define TCC_CM4_MBOX1_MAX		(2)

/* MBOX ID length is 3 ~ 6. */

/* Test client driver for tcc multi channel mbox test*/
#define TCC_MBOX_TEST_ID		"TEST01"
/* client driver for user applicaiton ipc*/
#define TCC_MBOX_IPC_ID			"IPC"
/* for HSM(Hardware Security Module) */
#define TCC_MBOX_HSM_ID			"HSM"
/* for snor firmware update */
#define TCC_MBOX_FWUG_ID		"FWUG"
/* for vioc manager */
#define TCC_MBOX_VIOC_ID		"VIOC"
/* for SDR middleware ipc */
#define TCC_MBOX_SDR_ID			"SDR"
/* for power event handling */
#define TCC_MBOX_POWER_ID		"POWER0"
/* for RVC status check for T-RVC */
#define TCC_MBOX_CAMIPC_ID		"CAMIPC"
/* for screen share */
#define TCC_MBOX_SCRSHARE_ID    "SCRSHR"
/* for touch share */
#define TCC_MBOX_TCHSHARE_ID    "TCHSHR"
/* for CM4 */
#define TCC_MBOX_CM4_ID			"CM4"
#endif
