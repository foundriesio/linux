// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
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
