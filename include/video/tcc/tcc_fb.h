/* linux/arch/arm/mach-tcc88xx/include/mach/tcc_fb.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __TCC_FB_H
#define __TCC_FB_H

#include "autoconf.h"

#define ATAG_TCC_PANEL	0x54434364 /* TCCd */

enum {
	PANEL_ID_AT070TN93,
	PANEL_ID_ED090NA,	
	PANEL_ID_FLD0800,
	PANEL_ID_TM123XDHP90,
	PANEL_ID_HDMI	
};

// Display Device 
typedef enum{
	TCC_OUTPUT_NONE,
	TCC_OUTPUT_LCD,
	TCC_OUTPUT_HDMI,
	TCC_OUTPUT_COMPOSITE,
	TCC_OUTPUT_COMPONENT,
	TCC_OUTPUT_MAX
} TCC_OUTPUT_TYPE;

struct lcd_panel;
struct tcc_dp_device;
struct tccfb_platform_data;

typedef enum{
	TCC_PWR_INIT,
	TCC_PWR_ON,		
	TCC_PWR_OFF
}tcc_db_power_s;

struct tcc_db_platform_data{
	/* destroy output.  db clocks are not on at this point */
	int (*set_power)(struct lcd_panel *db, tcc_db_power_s pwr);
};
extern struct tcc_db_platform_data *get_tcc_db_platform_data(void);

// tcc display output block operation 
struct tcc_db_out_ops {
	/* initialize output.  db clocks are not on at this point */
	int (*init)(struct lcd_panel *db);
	/* destroy output.  db clocks are not on at this point */
	void (*destroy)(struct lcd_panel *db);
	/* detect connected display.  can sleep.*/
	unsigned (*detect)(struct lcd_panel *db);
	/* enable output.  db clocks are on at this point */
	void (*enable)(struct lcd_panel *db);
	/* disable output.  db clocks are on at this point */
	void (*disable)(struct lcd_panel *db);
	/* suspend output.  db clocks are on at this point */
	void (*suspend)(struct lcd_panel *db);
	/* resume output.  db clocks are on at this point */
	void (*resume)(struct lcd_panel *db);
};


enum{
	TCC_DB_OUT_RGB,
	TCC_DB_OUT_LVDS,
	TCC_DB_OUT_DSI,
	TCC_DB_OUT_HDMI,
};

#ifdef CONFIG_ARCH_TCC803X
struct lvds_gpio {
	int power_on;
	int display_on;
	int reset;
	int stby;
	int power;
};

struct lvds_data {
	struct clk *clk;
	struct mutex panel_lock;
	struct lvds_gpio gpio;
	unsigned int main_port;
	unsigned int sub_port;
};
#endif

struct lcd_gpio_data {
	unsigned int power_on;
	unsigned int display_on;
	unsigned int reset;
	unsigned int gpio_port_num;
};

struct lcd_panel {

	struct device *dev;

	int						db_type;
	void						*db_out_data;
	struct tcc_db_out_ops		*db_out_ops;

	const char *name;
	const char *manufacturer;

	int id;			/* panel ID */
	int xres;		/* x resolution in pixels */
	int yres;		/* y resolution in pixels */
	int width;		/* display width in mm */
	int height;		/* display height in mm */
	int bpp;		/* bits per pixels */

	int clk_freq;
	int clk_div;
	int bus_width;
	int lpw;		/* line pulse width */
	int lpc;		/* line pulse count */
	int lswc;		/* line start wait clock */
	int lewc;		/* line end wait clock */
	int vdb;		/* back porch vsync delay */
	int vdf;		/* front porch vsync delay */
	int fpw1;		/* frame pulse width 1 */
	int flc1;		/* frame line count 1 */
	int fswc1;		/* frame start wait cycle 1 */
	int fewc1;		/* frame end wait cycle 1 */
	int fpw2;		/* frame pulse width 2 */
	int flc2;		/* frame line count 2 */
	int fswc2;		/* frame start wait cycle 2 */
	int fewc2;		/* frame end wait cycle 2 */
	int sync_invert;

	int (*init)(struct lcd_panel *panel, struct tcc_dp_device *fb_pdata);
	int (*set_power)(struct lcd_panel *panel, int on, struct tcc_dp_device *fb_pdata);

	int state; //current state 0 off , 0: on
	void *panel_data;
};

int tccfb_register_panel(struct lcd_panel *panel);
int extfb_register_panel(struct lcd_panel *panel);
struct lcd_panel *tccfb_get_panel(void);

static inline void tcc_db_set_outdata(struct lcd_panel *db, void *data)
{
	db->db_out_data = data;
}

static inline void *tcc_db_get_outdata(struct lcd_panel *db)
{
	return db->db_out_data;
}

extern void tcc_db_set_output(struct lcd_panel *db);

#ifdef CONFIG_TCC_MIPI
extern struct tcc_db_out_ops tcc_db_dsi_ops;
#endif//

#endif //__TCC_FB_H
