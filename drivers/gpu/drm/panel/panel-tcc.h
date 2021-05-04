// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#ifndef __PANEL_TCC_H__
#define __PANEL_TCC_H__

static inline int panel_tcc_pin_select_state(struct pinctrl *p,
						struct pinctrl_state *s)
{
	if(!p || !s)
		goto err_out;

	return pinctrl_select_state(p, s);
err_out:
	return -EINVAL;
}

#endif // __PANEL_TCC_H__