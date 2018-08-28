/****************************************************************************************
 *   FileName    : tcc_ipc_mbox.h
 *   Description : 
 ****************************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved 
 
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not limited 
to re-distribution in source or binary form is strictly prohibited.
This source code is provided ¡°AS IS¡± and nothing contained in this source code 
shall constitute any express or implied warranty of any kind, including without limitation, 
any warranty of merchantability, fitness for a particular purpose or non-infringement of any patent, 
copyright or other third party intellectual property right. 
No warranty is made, express or implied, regarding the information¡¯s accuracy, 
completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability arising from, 
out of or in connection with this source code or the use in the source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure Agreement 
between Telechips and Company.
*
****************************************************************************************/

#ifndef __TCC_IPC_MBOX_H__
#define __TCC_IPC_MBOX_H__

typedef void (*ipc_mbox_receive)(struct mbox_client *, void * );

IPC_INT32 ipc_mailbox_send(struct ipc_device *ipc_dev, struct tcc_mbox_data * ipc_msg);
struct mbox_chan *ipc_request_channel(struct platform_device *pdev, const char *name, ipc_mbox_receive handler);


#endif /* __TCC_IPC_MBOX_H__ */
