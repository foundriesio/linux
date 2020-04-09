/*
 * Copyright (C) 2008-2019 Telechips Inc.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <video/tcc/tcc_fb.h>

// setting display block type
void tcc_db_set_output(struct lcd_panel *db)
{

	switch (db->db_type) {
		case TCC_DB_OUT_RGB:
			break;

		case TCC_DB_OUT_LVDS:
			break;
		
		#ifdef CONFIG_TCC_MIPI
		case TCC_DB_OUT_DSI:
			db->db_out_ops = &tcc_db_dsi_ops;
			break;
		#endif//
		
		case TCC_DB_OUT_HDMI:
			break;
		
		default:
			db->db_out_ops = NULL;
			break;
	}

	if (db->db_out_ops && db->db_out_ops->init)
		db->db_out_ops->init(db);

}
