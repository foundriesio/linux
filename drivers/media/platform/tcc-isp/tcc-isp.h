// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCC_ISP_H__
#define __TCC_ISP_H__

#ifndef ON
#define ON		1
#endif

#ifndef OFF
#define OFF		0
#endif

#define TCC_ISP_DRIVER_NAME	"tcc-isp"
#define TCC_ISP_SUBDEV_NAME	TCC_ISP_DRIVER_NAME
#define TCC_ISP_FIRMWARE_NAME	"isp_firmware"

struct hdr_state {
	/* hdr input and output */
	int mode;
	int decompanding;
	int decompanding_curve_maxval;
	int decompanding_input_bit;
	int decompanding_output_bit;

	unsigned long dcpd_crv[8];
	unsigned long dcpdx[8];
};

struct input_state {
	int width;
	int height;
	int rgb_order;
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

struct tcc_isp_state {
	struct platform_device *pdev;

	/* register base addr */
	volatile void __iomem * isp_base;
	volatile void __iomem * mem_base;
	volatile void __iomem * cfg_base;

	/* uart pinctrl */
	struct pinctrl * uart_pinctrl;

	/* oak setting */
	struct hdr_state * hdr;

	/* zelcova(isp) setting */
	struct isp_state * isp;

	int irq;

	struct clk *clock;
	u32 clk_frequency;
};

struct reg_setting {
	short reg;
	unsigned long val;
};

#endif
