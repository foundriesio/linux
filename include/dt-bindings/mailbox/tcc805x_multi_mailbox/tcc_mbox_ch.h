#ifndef DT_TCC_MBOX_CH_H
#define DT_TCC_MBOX_CH_H

/* CPUBUS MBOX : A72 <-> A53 */
#define TCC_CB_MBOX_RESERVED		(0)
#define TCC_CB_MBOX_TEST			(1)
#define TCC_CB_MBOX_IPC				(2)
#define TCC_CB_MBOX_HSM				(3)
#define TCC_CB_MBOX_VIOC			(4)
#define TCC_CB_MBOX_MAX				(5)

/* MICOM MBOX0 : A72 <-> R5 */
#define TCC_MICOM_MBOX0_RESERVED	(0)
#define TCC_MICOM_MBOX0_TEST		(1)
#define TCC_MICOM_MBOX0_IPC			(2)
#define TCC_MICOM_MBOX0_HSM			(3)
#define TCC_MICOM_MBOX0_FWUG		(4)
#define TCC_MICOM_MBOX0_MAX			(5)

/* MICOM MBOX1 : A53 <-> R5 */
#define TCC_MICOM_MBOX1_RESERVED	(0)
#define TCC_MICOM_MBOX1_TEST		(1)
#define TCC_MICOM_MBOX1_IPC			(2)
#define TCC_MICOM_MBOX1_HSM			(3)
#define TCC_MICOM_MBOX1_MAX			(4)

/* HSM MBOX0 : A72 <-> HSM */
#define TCC_HSM_MBOX0_RESERVED		(0)
#define TCC_HSM_MBOX0				(1)
#define TCC_HSM_MBOX0_MAX			(2)

/* HSM MBOX1 : A53 <-> HSM */
#define TCC_HSM_MBOX1_RESERVED		(0)
#define TCC_HSM_MBOX1				(1)
#define TCC_HSM_MBOX1_MAX			(2)

/* MBOX ID length is 3 ~ 6. */
#define TCC_MBOX_TEST_ID		"TEST01"	/* Test client driver for tcc multi channel mbox test */
#define TCC_MBOX_IPC_ID			"IPC"		/* client driver for user applicaiton ipc*/
#define TCC_MBOX_HSM_ID			"HSM"		/* for HSM(Hardware Security Module) */
#define TCC_MBOX_FWUG_ID		"FWUG"		/* for snor firmware upate */
#define TCC_MBOX_VIOC_ID		"VIOC"		/* for vioc manager */
#endif
