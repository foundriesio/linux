/*
 *   FileName    : vpu_mgr.h
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _VPU_MGR_H_
#define _VPU_MGR_H_

#include "vpu_comm.h"

extern int vmgr_get_close(vputype type);
extern int vmgr_set_close(vputype type, int value, int bfreemem);
extern int vmgr_get_alive(void);

////////////////////////////////////////////////////////////////////////////////////
extern int vmgr_process_ex(VpuList_t *cmd_list, vputype type, int Op, int *result);
extern VpuList_t* vmgr_list_manager(VpuList_t* args, unsigned int cmd);

#endif // _VPU_MGR_H_
