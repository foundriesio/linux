/*!
* TCC Version 1.0
* Copyright (c) Telechips Inc.
* All rights reserved 
*  \file        hdmi_log.h
*  \brief       HDMI TX controller driver
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

#ifndef __HDMI_LOG_H__
#define __HDMI_LOG_H__

typedef enum {
	SNPS_ERROR = 0,
	SNPS_WARN,
	SNPS_NOTICE,
	SNPS_DEBUG,
	SNPS_TRACE,
	SNPS_ASSERT
} log_t;


#if 0
#define LOG_TRACE() \
	do { if (snps_functions && snps_functions->logger) snps_functions->logger(SNPS_TRACE, "%s", __func__); } while (0)
#define LOG_TRACE1(a) \
	do { if (snps_functions && snps_functions->logger) snps_functions->logger(SNPS_TRACE, "* %s:%d", __func__, a); } while (0)
#define LOG_TRACE2(a,b) \
	do { if (snps_functions && snps_functions->logger) snps_functions->logger(SNPS_TRACE, "** %s: %d %d", __func__, a, b); } while (0)
#define LOG_TRACE3(a,b,c) \
	do { if (snps_functions && snps_functions->logger) snps_functions->logger(SNPS_TRACE, "*** %s: %d %d %d", __func__, a, b, c); } while (0)
#else
#define LOG_TRACE()
#define LOG_TRACE1(a)
#define LOG_TRACE2(a,b)
#define LOG_TRACE3(a,b,c)
#endif

#define LOGGER(level, format, ...)  \
	do { if (hdmi_log_level >= level) printk("\e[33m[HDMI_TX]%s(%d) \e[0m" format, __func__, __LINE__, ##__VA_ARGS__); } while (0)
         
extern int hdmi_log_level;

#endif	/* __HDMI_LOG_H__ */
