// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Synopsys, Inc. and/or its affiliates.
 * Synopsys DesignWare HDMI driver
 */
#ifndef __HDMI_LOG_H__
#define __HDMI_LOG_H__

enum {
	SNPS_ERROR = 0,
	SNPS_WARN,
	SNPS_NOTICE,
	SNPS_DEBUG,
	SNPS_TRACE,
	SNPS_ASSERT
};

#define LOG_TRACE()
#define LOG_TRACE1(a)
#define LOG_TRACE2(a, b)
#define LOG_TRACE3(a, b, c)
#define LOGGER(level, format, ...)  \
	do { \
		if (hdmi_log_level >= level) \
			pr_info( \
				"[INFO][HDMI_V20] \e[33m[HDMI_TX]%s(%d) \e[0m" \
				format, __func__, __LINE__, ##__VA_ARGS__);  \
	} while (0)

extern int hdmi_log_level;

#endif	/* __HDMI_LOG_H__ */
