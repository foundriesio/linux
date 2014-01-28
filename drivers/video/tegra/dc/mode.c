/*
 * drivers/video/tegra/dc/mode.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Copyright (c) 2010-2012, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/types.h>
#include <linux/clk.h>

#include <mach/clk.h>
#include <mach/dc.h>

#include "dc_reg.h"
#include "dc_priv.h"

const struct fb_videomode tegra_modes[] = {
	/* EDT 5.7" ET070080DH or TouchRevolution Fusion 7" */
	{
		.name = 	"800x480",
		.refresh = 	60,
		.xres =		800,
		.yres =		480,
		.pixclock =	30807,
		.left_margin = 	128,
		.right_margin = 64,
		.upper_margin = 22,
		.lower_margin = 20,
		.hsync_len = 	64,
		.vsync_len = 	3,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.flag = FB_FLAG_RATIO_16_9,
		.vmode = FB_VMODE_NONINTERLACED
	},
	/* TouchRevolution Fusion 10" aka Chunghwa Picture Tubes
	 * CLAA100NC05 10.1 inch 1024x600 single channel LVDS panel
	 */
	{
		.name = 	"1024x600",
		.refresh = 	60,
		.xres = 	1024,
		.yres = 	600,
		.pixclock =     KHZ2PICOS(48000),
		.left_margin = 	104,
		.right_margin = 43,
		.upper_margin = 24,
		.lower_margin = 20,
		.hsync_len = 	5,
		.vsync_len = 	5,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.flag = FB_FLAG_RATIO_16_9,
		.vmode = FB_VMODE_NONINTERLACED
	},
	{
		/* 1366x768 */
		.refresh =      60,
		.xres =         1366,
		.yres =         768,
		.pixclock =     KHZ2PICOS(72072),
		.hsync_len =    58,    /* h_sync_width */
		.vsync_len =    4,     /* v_sync_width */
		.left_margin =  58,    /* h_back_porch */
		.upper_margin = 4,     /* v_back_porch */
		.right_margin = 58,    /* h_front_porch */
		.lower_margin = 4,      /* v_front_porch */
		.vmode =        FB_VMODE_NONINTERLACED,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	},
	{
		/* 1680x1050p 59.94/60hz */
		.refresh =      60,
		.xres =         1680,
		.yres =         1050,
		.pixclock =     KHZ2PICOS(147140),
		.hsync_len =    184,    /* h_sync_width */
		.vsync_len =    3,     /* v_sync_width */
		.left_margin =  288,    /* h_back_porch */
		.upper_margin = 33,     /* v_back_porch */
		.right_margin = 104,    /* h_front_porch */
		.lower_margin = 1,      /* v_front_porch */
		.vmode =        FB_VMODE_NONINTERLACED,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	},
	{
		/* 1920x1080p 59.94/60hz CVT */
		.refresh =	60,
		.xres =         1920,
		.yres =         1080,
		.pixclock =     KHZ2PICOS(148500),
		.hsync_len =    44,     /* h_sync_width */
		.vsync_len =    5,      /* v_sync_width */
		.left_margin =  148,     /* h_back_porch */
		.upper_margin = 36,     /* v_back_porch */
		.right_margin = 88,     /* h_front_porch */
		.lower_margin = 4,      /* v_front_porch */
		.vmode =        FB_VMODE_NONINTERLACED,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	},
	{
		/* 1920x1200p 60hz */
		.refresh =	60,
		.xres =         1920,
		.yres =         1200,
		.pixclock =     KHZ2PICOS(154000),
		.hsync_len =    32,     /* h_sync_width */
		.vsync_len =    6,      /* v_sync_width */
		.left_margin =  80,     /* h_back_porch */
		.upper_margin = 26,     /* v_back_porch */
		.right_margin = 48,     /* h_front_porch */
		.lower_margin = 3,      /* v_front_porch */
		.vmode =        FB_VMODE_NONINTERLACED,
		.sync = 0,
	},
	/* Portrait modes */
	{
		.name = 	"480x640",
		.refresh = 	60,
		.xres = 	480,
		.yres = 	640,
		.pixclock = 	55555,
		.left_margin = 	20,
		.right_margin = 8,
		.upper_margin = 7,
		.lower_margin = 8,
		.hsync_len = 	4,
		.vsync_len = 	1,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode = FB_VMODE_NONINTERLACED
	},
	{
		.name = 	"540x960",
		.refresh = 	60,
		.xres = 	540,
		.yres = 	960,
		.pixclock = 	100000,
		.left_margin = 	32,
		.right_margin = 32,
		.upper_margin = 1,
		.lower_margin = 2,
		.hsync_len = 	16,
		.vsync_len = 	1,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode = FB_VMODE_NONINTERLACED
	},
	{
		.name = 	"720x1280",
		.refresh = 	60,
		.xres = 	720,
		.yres = 	1280,
		.pixclock = 	16282,
		.left_margin = 	100,
		.right_margin = 4,
		.upper_margin = 14,
		.lower_margin = 4,
		.hsync_len = 	4,
		.vsync_len = 	4,
		.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode = FB_VMODE_NONINTERLACED
	},
};

/* try to find best matching mode using our modes, VESA and CEA modes from
 * modedb
 */
int tegra_fb_find_mode(struct fb_var_screeninfo *var, struct fb_info *info,
		       const char* option, unsigned int default_bpp)
{
	int out;

	out = fb_find_mode(var, info, option, tegra_modes,
			ARRAY_SIZE(tegra_modes), NULL, default_bpp);

	/* Only accept this mode if we found a reasonable match (resolution) */
	if (out == 1 || out == 2)
		return out;

	out = fb_find_mode(&info->var, info, option,
			cea_modes, CEA_MODEDB_SIZE, NULL, default_bpp);

	/* Check if we found a full match */
	if (out == 1 || out == 2)
		return out;

	return fb_find_mode(&info->var, info, option,
			vesa_modes, VESA_MODEDB_SIZE, NULL, default_bpp);
}
EXPORT_SYMBOL(tegra_fb_find_mode);


/* return non-zero if constraint is violated */
static int calc_h_ref_to_sync(const struct tegra_dc_mode *mode, int *href)
{
	long a, b;

	/* Constraint 5: H_REF_TO_SYNC >= 0 */
	a = 0;

	/* Constraint 6: H_FRONT_PORT >= (H_REF_TO_SYNC + 1) */
	b = mode->h_front_porch - 1;

	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11 */
	if (a + mode->h_sync_width + mode->h_back_porch <= 11)
		a = 1 + 11 - mode->h_sync_width - mode->h_back_porch;
	/* check Constraint 1 and 6 */
	if (a > b)
		return 1;

	/* Constraint 4: H_SYNC_WIDTH >= 1 */
	if (mode->h_sync_width < 1)
		return 4;

	/* Constraint 7: H_DISP_ACTIVE >= 16 */
	if (mode->h_active < 16)
		return 7;

	if (href) {
		if (b > a && a % 2)
			*href = a + 1; /* use smallest even value */
		else
			*href = a; /* even or only possible value */
	}

	return 0;
}

static int calc_v_ref_to_sync(const struct tegra_dc_mode *mode, int *vref)
{
	long a;
	a = 1; /* Constraint 5: V_REF_TO_SYNC >= 1 */

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1 */
	if (a + mode->v_sync_width + mode->v_back_porch <= 1)
		a = 1 + 1 - mode->v_sync_width - mode->v_back_porch;

	/* Constraint 6 */
	if (mode->v_front_porch < a + 1)
		a = mode->v_front_porch - 1;

	/* Constraint 4: V_SYNC_WIDTH >= 1 */
	if (mode->v_sync_width < 1)
		return 4;

	/* Constraint 7: V_DISP_ACTIVE >= 16 */
	if (mode->v_active < 16)
		return 7;

	if (vref)
		*vref = a;
	return 0;
}

static int calc_ref_to_sync(struct tegra_dc_mode *mode)
{
	int ret;
	ret = calc_h_ref_to_sync(mode, &mode->h_ref_to_sync);
	if (ret)
		return ret;
	ret = calc_v_ref_to_sync(mode, &mode->v_ref_to_sync);
	if (ret)
		return ret;

	return 0;
}

static bool check_ref_to_sync(struct tegra_dc_mode *mode)
{
	/* Constraint 1: H_REF_TO_SYNC + H_SYNC_WIDTH + H_BACK_PORCH > 11. */
	if (mode->h_ref_to_sync + mode->h_sync_width + mode->h_back_porch <= 11)
		return false;

	/* Constraint 2: V_REF_TO_SYNC + V_SYNC_WIDTH + V_BACK_PORCH > 1. */
	if (mode->v_ref_to_sync + mode->v_sync_width + mode->v_back_porch <= 1)
		return false;

	/* Constraint 3: V_FRONT_PORCH + V_SYNC_WIDTH + V_BACK_PORCH > 1
	 * (vertical blank). */
	if (mode->v_front_porch + mode->v_sync_width + mode->v_back_porch <= 1)
		return false;

	/* Constraint 4: V_SYNC_WIDTH >= 1; H_SYNC_WIDTH >= 1. */
	if (mode->v_sync_width < 1 || mode->h_sync_width < 1)
		return false;

	/* Constraint 5: V_REF_TO_SYNC >= 1; H_REF_TO_SYNC >= 0. */
	if (mode->v_ref_to_sync < 1 || mode->h_ref_to_sync < 0)
		return false;

	/* Constraint 6: V_FRONT_PORT >= (V_REF_TO_SYNC + 1);
	 * H_FRONT_PORT >= (H_REF_TO_SYNC + 1). */
	if (mode->v_front_porch < mode->v_ref_to_sync + 1 ||
		mode->h_front_porch < mode->h_ref_to_sync + 1)
		return false;

	/* Constraint 7: H_DISP_ACTIVE >= 16; V_DISP_ACTIVE >= 16. */
	if (mode->h_active < 16 || mode->v_active < 16)
		return false;

	return true;
}

#ifndef CONFIG_ANDROID
static s64 calc_frametime_ns(const struct tegra_dc_mode *m)
{
	long h_total, v_total;
	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;
	return (!m->pclk) ? 0 : (s64)(div_s64(((s64)h_total * v_total *
					1000000000ULL), m->pclk));
}
#endif /* !CONFIG_ANDROID */

/* return in 1000ths of a Hertz */
int tegra_dc_calc_refresh(const struct tegra_dc_mode *m)
{
	long h_total, v_total, refresh;
	h_total = m->h_active + m->h_front_porch + m->h_back_porch +
		m->h_sync_width;
	v_total = m->v_active + m->v_front_porch + m->v_back_porch +
		m->v_sync_width;
	refresh = m->pclk / h_total;
	refresh *= 1000;
	refresh /= v_total;
	return refresh;
}

#ifdef DEBUG
static void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note)
{
	if (mode) {
		int refresh = tegra_dc_calc_refresh(mode);
		dev_info(&dc->ndev->dev, "%s():MODE:%dx%d@%d.%03uHz pclk=%d\n",
			note ? note : "",
			mode->h_active, mode->v_active,
			refresh / 1000, refresh % 1000,
			mode->pclk);
	}
}
#else /* !DEBUG */
static inline void print_mode(struct tegra_dc *dc,
			const struct tegra_dc_mode *mode, const char *note) { }
#endif /* DEBUG */

int tegra_dc_program_mode(struct tegra_dc *dc, struct tegra_dc_mode *mode)
{
	unsigned long val;
	unsigned long rate;
	unsigned long div;
	unsigned long pclk;

	print_mode(dc, mode, __func__);

	/* use default EMC rate when switching modes */
	dc->new_emc_clk_rate = tegra_dc_get_default_emc_clk_rate(dc);
	tegra_dc_program_bandwidth(dc, true);

	tegra_dc_writel(dc, 0x0, DC_DISP_DISP_TIMING_OPTIONS);
	tegra_dc_writel(dc, mode->h_ref_to_sync | (mode->v_ref_to_sync << 16),
			DC_DISP_REF_TO_SYNC);
	tegra_dc_writel(dc, mode->h_sync_width | (mode->v_sync_width << 16),
			DC_DISP_SYNC_WIDTH);
	tegra_dc_writel(dc, mode->h_back_porch | (mode->v_back_porch << 16),
			DC_DISP_BACK_PORCH);
	tegra_dc_writel(dc, mode->h_active | (mode->v_active << 16),
			DC_DISP_DISP_ACTIVE);
	tegra_dc_writel(dc, mode->h_front_porch | (mode->v_front_porch << 16),
			DC_DISP_FRONT_PORCH);

	tegra_dc_writel(dc, DE_SELECT_ACTIVE | DE_CONTROL_NORMAL,
			DC_DISP_DATA_ENABLE_OPTIONS);

	/* TODO: MIPI/CRT/HDMI clock cals */

	val = DISP_DATA_FORMAT_DF1P1C;

	if (dc->out->align == TEGRA_DC_ALIGN_MSB)
		val |= DISP_DATA_ALIGNMENT_MSB;
	else
		val |= DISP_DATA_ALIGNMENT_LSB;

	if (dc->out->order == TEGRA_DC_ORDER_RED_BLUE)
		val |= DISP_DATA_ORDER_RED_BLUE;
	else
		val |= DISP_DATA_ORDER_BLUE_RED;

	tegra_dc_writel(dc, val, DC_DISP_DISP_INTERFACE_CONTROL);

	rate = tegra_dc_clk_get_rate(dc);

	pclk = tegra_dc_pclk_round_rate(dc, mode->pclk);
	trace_printk("%s:pclk=%ld\n", dc->ndev->name, pclk);
	if (pclk < (mode->pclk / 100 * 99) ||
	    pclk > (mode->pclk / 100 * 109)) {
		dev_err(&dc->ndev->dev,
			"can't divide %ld clock to %d -1/+9%% %ld %d %d\n",
			rate, mode->pclk,
			pclk, (mode->pclk / 100 * 99),
			(mode->pclk / 100 * 109));
		return -EINVAL;
	}

	div = (rate * 2 / pclk) - 2;
	trace_printk("%s:div=%ld\n", dc->ndev->name, div);

	tegra_dc_writel(dc, 0x00010001,
			DC_DISP_SHIFT_CLOCK_OPTIONS);
	tegra_dc_writel(dc, PIXEL_CLK_DIVIDER_PCD1 | SHIFT_CLK_DIVIDER(div),
			DC_DISP_DISP_CLOCK_CONTROL);

#ifdef CONFIG_SWITCH
	switch_set_state(&dc->modeset_switch,
			 (mode->h_active << 16) | mode->v_active);
#endif

	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	print_mode_info(dc, dc->mode);
	return 0;
}

static int panel_sync_rate;

int tegra_dc_get_panel_sync_rate(void)
{
	return panel_sync_rate;
}
EXPORT_SYMBOL(tegra_dc_get_panel_sync_rate);

int tegra_dc_set_mode(struct tegra_dc *dc, const struct tegra_dc_mode *mode)
{
	memcpy(&dc->mode, mode, sizeof(dc->mode));

	dev_info(&dc->ndev->dev, "using mode %dx%d pclk=%d href=%d vref=%d\n",
		mode->h_active, mode->v_active, mode->pclk,
		mode->h_ref_to_sync, mode->v_ref_to_sync
	);

	if (dc->out->type == TEGRA_DC_OUT_RGB)
		panel_sync_rate = tegra_dc_calc_refresh(mode);
	else if (dc->out->type == TEGRA_DC_OUT_DSI)
		panel_sync_rate = dc->out->dsi->rated_refresh_rate * 1000;

	print_mode(dc, mode, __func__);
#ifndef CONFIG_ANDROID
	dc->frametime_ns = calc_frametime_ns(mode);
#endif /* !CONFIG_ANDROID */

	return 0;
}
EXPORT_SYMBOL(tegra_dc_set_mode);

int tegra_dc_var_to_dc_mode(struct tegra_dc *dc, struct fb_var_screeninfo *var,
		struct tegra_dc_mode *mode)
{
	bool stereo_mode = false;
	int err;

	if (!var->pixclock)
		return -EINVAL;

	mode->pclk = PICOS2KHZ(var->pixclock) * 1000;
	mode->h_sync_width = var->hsync_len;
	mode->v_sync_width = var->vsync_len;
	mode->h_back_porch = var->left_margin;
	mode->v_back_porch = var->upper_margin;
	mode->h_active = var->xres;
	mode->v_active = var->yres;
	mode->h_front_porch = var->right_margin;
	mode->v_front_porch = var->lower_margin;
	mode->stereo_mode = stereo_mode;

	/*
	 * HACK:
	 * If v_front_porch is only 1, we would violate Constraint 5/6
	 * in this case, increase front porch by 1
	 */
	if (mode->v_front_porch <= 1)
		mode->v_front_porch = 2;


	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		/* HDMI controller requires h_ref=1, v_ref=1 */
		mode->h_ref_to_sync = 1;
		mode->v_ref_to_sync = 1;
	} else {
		/* Calculate ref_to_sync signals */
		err = calc_ref_to_sync(mode);
		if (err) {
			dev_err(&dc->ndev->dev, "display timing ref_to_sync"
				"calculation failed with code %d\n", err);
			return -EINVAL;
		}
		dev_info(&dc->ndev->dev, "Calculated sync href=%d vref=%d\n",
				mode->h_ref_to_sync, mode->v_ref_to_sync);
	}
	if (!check_ref_to_sync(mode)) {
		dev_err(&dc->ndev->dev,
				"display timing doesn't meet restrictions.\n");
		return -EINVAL;
	}

#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
	/* Double the pixel clock and update v_active only for
	 * frame packed mode */
	if (mode->stereo_mode) {
		mode->pclk *= 2;
		/* total v_active = yres*2 + activespace */
		mode->v_active = var->yres * 2 +
				var->vsync_len +
				var->upper_margin +
				var->lower_margin;
	}
#endif

	mode->flags = 0;

	if (!(var->sync & FB_SYNC_HOR_HIGH_ACT))
		mode->flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;

	if (!(var->sync & FB_SYNC_VERT_HIGH_ACT))
		mode->flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;

	return 0;
}
EXPORT_SYMBOL(tegra_dc_var_to_dc_mode);

/*
 * This method is only used by sysfs interface
 * /sys/devices/tegradc.1/nvdps
 */
int tegra_dc_set_fb_mode(struct tegra_dc *dc,
		const struct fb_videomode *fbmode, bool stereo_mode)
{
	int err;
	struct tegra_dc_mode mode;

	if (!fbmode->pixclock)
		return -EINVAL;

	mode.pclk = PICOS2KHZ(fbmode->pixclock) * 1000;
	mode.h_sync_width = fbmode->hsync_len;
	mode.v_sync_width = fbmode->vsync_len;
	mode.h_back_porch = fbmode->left_margin;
	mode.v_back_porch = fbmode->upper_margin;
	mode.h_active = fbmode->xres;
	mode.v_active = fbmode->yres;
	mode.h_front_porch = fbmode->right_margin;
	mode.v_front_porch = fbmode->lower_margin;
	mode.stereo_mode = stereo_mode;
	if (dc->out->type == TEGRA_DC_OUT_HDMI) {
		/* HDMI controller requires h_ref=1, v_ref=1 */
		mode.h_ref_to_sync = 1;
		mode.v_ref_to_sync = 1;
	} else {
		/*
		 * HACK:
		 * If v_front_porch is only 1, we would violate Constraint 5/6
		 * in this case, increase front porch by 1
		 */
		if (mode.v_front_porch <= 1)
			mode.v_front_porch = 2;

		err = calc_ref_to_sync(&mode);
		if (err) {
			dev_err(&dc->ndev->dev, "display timing ref_to_sync"
				"calculation failed with code %d\n", err);
			return -EINVAL;
		}
		dev_info(&dc->ndev->dev, "Calculated sync href=%d vref=%d\n",
				mode.h_ref_to_sync, mode.v_ref_to_sync);
	}
	if (!check_ref_to_sync(&mode)) {
		dev_err(&dc->ndev->dev,
				"display timing doesn't meet restrictions.\n");
		return -EINVAL;
	}

#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
	/* Double the pixel clock and update v_active only for
	 * frame packed mode */
	if (mode.stereo_mode) {
		mode.pclk *= 2;
		/* total v_active = yres*2 + activespace */
		mode.v_active = fbmode->yres * 2 +
				fbmode->vsync_len +
				fbmode->upper_margin +
				fbmode->lower_margin;
	}
#endif

	mode.flags = 0;

	if (!(fbmode->sync & FB_SYNC_HOR_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;

	if (!(fbmode->sync & FB_SYNC_VERT_HIGH_ACT))
		mode.flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;

	return tegra_dc_set_mode(dc, &mode);
}
EXPORT_SYMBOL(tegra_dc_set_fb_mode);
