// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "tcc_gmac_ptp.h"

static struct tcc_gmac_ptp_ops *g_ptp_ops;
static void __iomem *g_base_addr;

static int tcc_gmac_ptp_adjfreq(struct ptp_clock_info *ptp, s32 ppb)
{
	bool addsub;
	u32 ns;

	if (ppb < 0) {
		addsub = (bool) false;
		ns = (unsigned int)(-ppb);
	} else {
		addsub = (bool) true;
		ns = (unsigned int)ppb;
	}

	return g_ptp_ops->update_system_time(g_base_addr, (unsigned int)0,
					     (unsigned int)ns, (bool) addsub);
}

static int tcc_gmac_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	bool addsub;
	u64 ns;
	ktime_t time;
	struct timespec ts;

	if (delta < 0) {
		addsub = (bool) false;
		ns = (unsigned int)-delta;
	} else {
		addsub = (bool) true;
		ns = (unsigned int)delta;
	}

	time = ns_to_ktime(ns);
	ts = ktime_to_timespec(time);
	return g_ptp_ops->update_system_time(g_base_addr,
			(unsigned int)ts.tv_sec,
					     (unsigned int)ts.tv_nsec,
					     (bool) addsub);
}

static int tcc_gmac_ptp_gettime(struct ptp_clock_info *ptp,
				struct timespec64 *ts)
{
	return g_ptp_ops->get_system_time(g_base_addr, ts);
}

static int tcc_gmac_ptp_settime(struct ptp_clock_info *ptp,
				const struct timespec64 *ts)
{
	return g_ptp_ops->set_system_time(g_base_addr, (u32) ts->tv_sec,
					  (u32) ts->tv_sec);
}

static int tcc_gmac_ptp_enable(struct ptp_clock_info *ptp,
			       struct ptp_clock_request *rq, int on)
{
	pr_info("%s\n", __func__);
	return 0;
}

static struct ptp_clock_info tcc_gmac_ptp = {
	.owner = THIS_MODULE,
	.name = "gmac clock",
	//.max_adj    = 512000,
	.max_adj = 999999999,
	.n_alarm = 1,
	.n_ext_ts = 0,
	.n_per_out = 0,
	.pps = 0,
	.adjfreq = tcc_gmac_ptp_adjfreq,
	.adjtime = tcc_gmac_ptp_adjtime,
	.gettime64 = tcc_gmac_ptp_gettime,
	.settime64 = tcc_gmac_ptp_settime,
	.enable = tcc_gmac_ptp_enable,
};

struct ptp_clock *tcc_gmac_ptp_probe(struct net_device *dev)
{
	struct tcc_gmac_priv *priv = netdev_priv(dev);

	g_base_addr = (void __iomem *)dev->base_addr;
	g_ptp_ops = priv->hw->ptp;

	return ptp_clock_register(&tcc_gmac_ptp, NULL);
}

void tcc_gmac_ptp_remove(struct ptp_clock *ptp)
{
	ptp_clock_unregister(ptp);
}
