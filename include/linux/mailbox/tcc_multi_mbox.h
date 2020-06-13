/****************************************************************************
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

#ifndef TCC_MULTI_MBOX_H
#define TCC_MULTI_MBOX_H

#if defined(CONFIG_ARCH_TCC803X)
	#include <linux/mailbox/tcc803x_multi_mailbox/tcc803x_multi_mbox.h>
#elif defined(CONFIG_ARCH_TCC805X)
	#include <linux/mailbox/tcc805x_multi_mailbox/tcc805x_multi_mbox.h>
#endif

#endif
