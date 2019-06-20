/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * This header provides constants for binding Telechips SoCs.
 *
 * Copyright (C) 2019 Telechips Inc.
 */

#ifndef __DT_BINDINGS_TELECHIPS_CLK_COMMON_H
#define __DT_BINDINGS_TELECHIPS_CLK_COMMON_H

#ifndef BIT
#define BIT(nr)			(1 << (nr))
#endif

#define CLK_SET_RATE_GATE	BIT(0) /* must be gated across rate change */
#define CLK_SET_PARENT_GATE	BIT(1) /* must be gated across re-parent */
#define CLK_SET_RATE_PARENT	BIT(2) /* propagate rate change up one level */
#define CLK_IGNORE_UNUSED	BIT(3) /* do not gate even if unused */
				/* unused */
#define CLK_IS_BASIC		BIT(5) /* Basic clk, can't do a to_clk_foo() */
#define CLK_GET_RATE_NOCACHE	BIT(6) /* do not use the cached clk rate */
#define CLK_SET_RATE_NO_REPARENT BIT(7) /* don't re-parent on rate change */
#define CLK_GET_ACCURACY_NOCACHE BIT(8) /* do not use the cached clk accuracy */
#define CLK_RECALC_NEW_RATES	BIT(9) /* recalc rates after notifications */
#define CLK_SET_RATE_UNGATE	BIT(10) /* clock needs to run to set rate */
#define CLK_IS_CRITICAL		BIT(11) /* do not gate, ever */
/* parents need enable during gate/ungate, set rate and re-parent */
#define CLK_OPS_PARENT_ENABLE	BIT(12)

/* Vendor-specific flags */
#define CLK_F_SKIP_SSCG		(1 << 28)
#define CLK_F_SRC_CLK_MASK	0x3f
#define CLK_F_SRC_CLK_SHIFT	16
#define CLK_F_SRC_CLK(x)	((((x) & CLK_F_SRC_CLK_MASK) << CLK_F_SRC_CLK_SHIFT) | CLK_F_FIXED)
#define CLK_F_FIXED		(1 << 29)	/* Use fixed source clock */
#define CLK_F_DCO_MODE		(1 << 30)	/* Set DCO mode */
#define CLK_F_DIV_MODE		(1 << 31)	/* Set divide mode */

#endif /* __DT_BINDINGS_TELECHIPS_CLK_COMMON_H */
