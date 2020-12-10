/*
 * Copyright (C) 20010-2020 Telechips
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
 */

#ifndef HDCP_API_H
#define HDCP_API_H

#define DDICFG_AESKEYREG(x) (x)
#define DDICFG_HDMI_AESKEY_VALID DDICFG_AESKEYREG(0x0014)
#define DDICFG_HDMI_AESKEY_DATA0 DDICFG_AESKEYREG(0x0018)
#define DDICFG_HDMI_AESKEY_DATA1 DDICFG_AESKEYREG(0x001c)
#define DDICFG_HDMI_AESKEY_HW0 DDICFG_AESKEYREG(0x0020)
#define DDICFG_HDMI_AESKEY_HW1 DDICFG_AESKEYREG(0x0024)
#define DDICFG_HDMI_AESKEY_HW2 DDICFG_AESKEYREG(0x0028)

// clang-format off
typedef enum {
	AESKEY_DATA0,
	AESKEY_DATA1,
	AESKEY_HW_0,
	AESKEY_HW_1,
	AESKEY_HW_2
} _AESKEY_;
// clang-format on

extern int hdcp_api_initialize(void);
extern int hdcp_api_cmd_process(unsigned int cmd, unsigned long arg);
extern int hdcp_api_status_chk(unsigned char flag);
extern unsigned int hdcp_api_poll_chk(struct file *file, poll_table *wait);
extern int hdcp_api_open(void);
extern int hdcp_api_close(void);

#endif /*HDCP_API_H*/
