/****************************************************************************
 * include/linux/tcc_printk.h - to use the print message
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef __TCC_PRINTK_H__
#define __TCC_PRINTK_H__

#include <linux/printk.h>

#if !defined(__TCC_NAME__)
#define __TCC_NAME__	KBUILD_MODNAME
#endif

#define tcc_printk(level, fmt, ...)	\
	do {	\
		printk(level "[%s:%d] " fmt "\n",	\
			__TCC_NAME__, __LINE__, ##__VA_ARGS__);	\
	} while(0)

#define tcc_pr_emerg(fmt, ...)		\
	tcc_printk(KERN_EMERG, fmt, ##__VA_ARGS__)

#define tcc_pr_alert(fmt, ...)		\
	tcc_printk(KERN_ALERT, fmt, ##__VA_ARGS__)

#define tcc_pr_crit(fmt, ...)			\
	tcc_printk(KERN_CRIT, fmt, ##__VA_ARGS__)

#define tcc_pr_err(fmt, ...)			\
	tcc_printk(KERN_ERR, fmt, ##__VA_ARGS__)

#define tcc_pr_warning(fmt, ...)	\
	tcc_printk(KERN_WARNING, fmt, ##__VA_ARGS__)
#define tcc_pr_warn tcc_pr_warning

#define tcc_pr_notice(fmt, ...)		\
	tcc_printk(KERN_NOTICE, fmt, ##__VA_ARGS__)
#define tcc_pr_noti tcc_pr_notice

#define tcc_pr_info(fmt, ...) \
	tcc_printk(KERN_INFO, fmt, ##__VA_ARGS__)

#if defined(CONFIG_DYNAMIC_DEBUG)
#define tcc_pr_debug(fmt, ...) \
	dynamic_pr_debug("[%s:%d] " fmt "\n",	\
		__TCC_NAME__, __LINE__, ##__VA_ARGS__)
#elif defined(DEBUG)
#define tcc_pr_debug(fmt, ...) 		\
	tcc_printk(KERN_DEBUG, fmt, ##__VA_ARGS__)
#else
#define tcc_pr_debug(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#endif /* __TCC_PRINTK_H__ */
