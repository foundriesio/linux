

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
