// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
* Synopsys DesignWare HDMI driver
*/
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
	do { if (hdmi_log_level >= level) printk(KERN_INFO "[INFO][HDMI_V20] \e[33m[HDMI_TX]%s(%d) \e[0m" format, __func__, __LINE__, ##__VA_ARGS__); } while (0)

extern int hdmi_log_level;

#endif	/* __HDMI_LOG_H__ */
