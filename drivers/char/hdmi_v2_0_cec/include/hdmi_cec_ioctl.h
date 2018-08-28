
/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi_cec_ioctl.h
*  \brief       HDMI CEC controller driver
*  \details   
*  \version     1.0
*  \date        2014-2015
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written  permission  of Telechips including not 
limited to re-distribution in source  or binary  form  is strictly prohibited.
This source  code is  provided "AS IS"and nothing contained in this source 
code  shall  constitute any express  or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a   particular 
purpose or non-infringement  of  any  patent,  copyright  or  other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source  code or the  use in the 
source code. 
This source code is provided subject  to the  terms of a Mutual  Non-Disclosure 
Agreement between Telechips and Company.
*******************************************************************************/


#ifndef _TCC_HDMI_V2_0_CEC_H_
#define _TCC_HDMI_V2_0_CEC_H_

#define CEC_IOC_MAGIC        'c'

/**
 * CEC device request code to set logical address.
 */
#define CEC_IOC_SETLADDR     _IOW(CEC_IOC_MAGIC, 0, unsigned int)
#define CEC_IOC_SENDDATA         _IOW(CEC_IOC_MAGIC, 1, unsigned int)
#define CEC_IOC_RECVDATA         _IOW(CEC_IOC_MAGIC, 2, unsigned int)

#define CEC_IOC_START    _IO(CEC_IOC_MAGIC, 3)
#define CEC_IOC_STOP     _IO(CEC_IOC_MAGIC, 4)

#define CEC_IOC_GETRESUMESTATUS _IOR(CEC_IOC_MAGIC, 5, unsigned int)
#define CEC_IOC_CLRRESUMESTATUS _IOW(CEC_IOC_MAGIC, 6, unsigned int)

#define CEC_IOC_STATUS 			_IOW(CEC_IOC_MAGIC, 7, unsigned int)
#define CEC_IOC_CLEARLADDR     _IOR(CEC_IOC_MAGIC, 8, unsigned int)
#define CEC_IOC_SETPADDR     _IOR(CEC_IOC_MAGIC, 9, unsigned int)

#define CEC_IOC_BLANK                      _IOW(CEC_IOC_MAGIC,50, unsigned int)

#endif /* _TCC_HDMI_V2_0_CEC_H_ */