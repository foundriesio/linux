/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi_1_4_hpd.h
*  \brief       HDMI HPD driver
*  \details   
*               Important!
*               The default tab size of this source code is setted with 8.
*  \version     1.0
*  \date        2014-2018
*  \copyright
This source code contains confidential information of Telechips.
Any unauthorized use without a written permission of Telechips including not 
limited to re-distribution in source or binary form is strictly prohibited.
This source code is provided "AS IS"and nothing contained in this source 
code shall constitute any express or implied warranty of any kind, including
without limitation, any warranty of merchantability, fitness for a particular 
purpose or non-infringement of any patent, copyright or other third party 
intellectual property right. No warranty is made, express or implied, regarding 
the information's accuracy, completeness, or performance. 
In no event shall Telechips be liable for any claim, damages or other liability 
arising from, out of or in connection with this source code or the use in the 
source code. 
This source code is provided subject to the terms of a Mutual Non-Disclosure 
Agreement between Telechips and Company. 
*/
#ifndef _LINUX_HPD_H_
#define _LINUX_HPD_H_

#define HPD_IOC_MAGIC        'p'

/**
 * HPD device request code to set logical address.
 */
#define HPD_IOC_START	 _IO(HPD_IOC_MAGIC, 0)
#define HPD_IOC_STOP	 _IO(HPD_IOC_MAGIC, 1)

#define HPD_IOC_BLANK	 _IOW(HPD_IOC_MAGIC,50, unsigned int)

#endif /* _LINUX_HPD_H_ */
