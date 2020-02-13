/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2020 Telechips Inc.

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

/**
 * tee_trace_config
 *
 * @base: [in] Shared memory base address for trace log.
 * @size: [in] Shared memory size for trace log.
 */
void tee_trace_config(char *base, uint32_t size);

/**
 * tee_trace_get_log
 *
 * @addr: [out]user memory address for get trace log
 * @size: [in]user memory size for get trace log
 *
 * @result:
 *    result > 0: copied buffer size
 *    result < 0: error
 */
int tee_trace_get_log(uint64_t __user addr, uint64_t size);
