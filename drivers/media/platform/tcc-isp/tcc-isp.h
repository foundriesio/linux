// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_ISP_H__
#define __TCC_ISP_H__

#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include "tcc-isp-reg.h"

#ifndef ON
#define ON		1
#endif

#ifndef OFF
#define OFF		0
#endif

#define TCC_ISP_DRIVER_NAME	"tcc-isp"
#define TCC_ISP_SUBDEV_NAME	TCC_ISP_DRIVER_NAME
#define TCC_ISP_FIRMWARE_NAME	"tcc-isp-fw"

/* Brightness*/
#define TCC_ISP_BRI_MIN		-128
#define TCC_ISP_BRI_DEF		0
#define TCC_ISP_BRI_MAX		127

struct hdr_state {
	/* hdr input and output */
	int mode;
	int decompanding;
	int decompanding_curve_maxval;
	int decompanding_input_bit;
	int decompanding_output_bit;

	unsigned long dcpd_cur_gain[8];
	unsigned long dcpd_cur_x_axis[8];
};

struct input_state {
	int width;
	int height;
	int pixel_order;
};

struct output_state {
	int x;
	int y;
	int width;
	int height;
	int format;
	int data_order;
};

struct isp_state {
	/* input and output image size and format */
	struct input_state i_state;
	struct output_state o_state;
};

struct isp_tune {
	const struct reg_setting *isp;
	unsigned int isp_setting_size;

	const struct reg_setting *adaptive;
	unsigned int adaptive_setting_size;
};

struct tcc_isp_state {
	struct platform_device *pdev;
	struct v4l2_subdev	sd;
	struct v4l2_ctrl_handler ctrl_hdl;

	char isp_fw_name[20];
	int fw_load;
	/* register base addr */
	void __iomem *isp_base;
	void __iomem *mem_base;
	void __iomem *cfg_base;

	/* uart pinctrl */
	struct pinctrl *uart_pinctrl;

	struct v4l2_mbus_framefmt fmt;

	/* oak(hdr) setting */
	const struct hdr_state *hdr;

	/* zelcova(isp) setting */
	struct isp_state isp;

	/* tune setting */
	const struct isp_tune *tune;

	int mem_share;

	int irq;

	struct clk *clock;
	u32 clk_frequency;
};

struct reg_setting {
	short reg;
	unsigned long val;
};

#endif
