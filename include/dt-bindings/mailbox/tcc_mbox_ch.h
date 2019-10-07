#ifndef __DT_TCC_MBOX_CH_H__
#define __DT_TCC_MBOX_CH_H__

/*  MBOX0 : A53 <-> R5 */
#define TCC_MBOX0_CH_RESERVED		0
#define TCC_MBOX0_CH_IPC			1
#define TCC_MBOX0_CH_SUSPEND        2
#define TCC_MBOX0_CH_RESUME			3
#define TCC_MBOX0_CH_SNOR_UPDATE	4
#define TCC_MBOX0_CH_MAX			5

/*  MBOX1 : A53 <-> A7S */
#define TCC_MBOX1_CH_RESERVED		0
#define TCC_MBOX1_CH_IPC			1
#define TCC_MBOX1_CH_VIOC			2
#define TCC_MBOX1_CH_SND			3
#define TCC_MBOX1_CH_SWITCH			4
#define TCC_MBOX1_CH_TOUCH			5
#define TCC_MBOX1_CH_SCRSHARE		6
#define TCC_MBOX1_CH_MAX			7

#define TCC_MBOX_CH_LIMIT			7

#endif /* __DT_TCC_MBOX_CH_H__ */
