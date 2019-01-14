/****************************************************************************
Copyright (C) 2018 Telechips Inc.
Copyright (C) 2018 Synopsys Inc.

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/
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
