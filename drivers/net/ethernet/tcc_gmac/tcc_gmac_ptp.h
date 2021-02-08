// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/ptp_clock_kernel.h>

#include "tcc_gmac_ctrl.h"
#include "tcc_gmac_drv.h"

struct ptp_clock *tcc_gmac_ptp_probe(struct net_device *dev);
void tcc_gmac_ptp_remove(struct ptp_clock *ptp);
