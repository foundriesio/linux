// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "vpu_comm.h"
#include "vp9_mgr_sys.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VP9_MGR_SYS: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VP9_MGR_SYS: " msg)
#define err(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_VP9_MGR_SYS[Err]: "msg)
#define info(msg...)     V_DBG(VPU_DBG_INFO, "TCC_VP9_MGR_SYS[Info]: "msg)

// for pwdn and vBus
static struct clk *fbus_vbus_clk;	// = NULL;
static struct clk *vbus_vp9_clk;	// = NULL;

#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
// for pwdn and vBus
static struct reset_control *vbus_vp9_reset;	// = NULL;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped;	// = 0;

void vp9mgr_enable_clock(int vbus_no_ctrl)
{
	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_prepare_enable(fbus_vbus_clk);
	if (vbus_vp9_clk)
		clk_prepare_enable(vbus_vp9_clk);
}

void vp9mgr_disable_clock(int vbus_no_ctrl)
{
	if (vbus_vp9_clk)
		clk_disable_unprepare(vbus_vp9_clk);
#if !defined(VBUS_CLK_ALWAYS_ON)
	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_disable_unprepare(fbus_vbus_clk);
#endif
}

void vp9mgr_get_clock(struct device_node *node)
{
	if (node == NULL)
		err("device node is null");

	fbus_vbus_clk = of_clk_get(node, 0);
	BUG_ON(IS_ERR(fbus_vbus_clk));

	vbus_vp9_clk = of_clk_get(node, 1);
	BUG_ON(IS_ERR(vbus_vp9_clk));
}

void vp9mgr_put_clock(void)
{
	if (vbus_vp9_clk) {
		clk_put(vbus_vp9_clk);
		vbus_vp9_clk = NULL;
	}
	if (fbus_vbus_clk) {
		clk_put(fbus_vbus_clk);
		fbus_vbus_clk = NULL;
	}
}

void vp9mgr_get_reset(struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
		err("device node is null");

	V_DBG(VPU_DBG_RSTCLK,
		"hmgr_get_reset");
	vbus_vp9_reset = of_reset_control_get(node, "vp9_bus");
	BUG_ON(IS_ERR(vbus_vp9_reset));
#endif
}

void vp9mgr_put_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vbus_vp9_reset) {
		reset_control_put(vbus_vp9_reset);
		vbus_vp9_reset = NULL;
	}
#endif
}

int vp9mgr_get_reset_register(void)
{
	return 0;
}

void vp9mgr_hw_assert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_vp9_reset)
		reset_control_assert(vbus_vp9_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vp9mgr_get_reset_register());
#endif
}

void vp9mgr_hw_deassert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_vp9_reset)
		reset_control_deassert(vbus_vp9_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vp9mgr_get_reset_register());
#endif
}

void vp9mgr_hw_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	udelay(1000);

	vp9mgr_hw_assert();

	udelay(1000);

	vp9mgr_hw_deassert();

	udelay(1000);
#endif
}

void vp9mgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	vp9mgr_hw_assert();
	while (opened_count) {
		vp9mgr_disable_clock(vbus_no_ctrl);
		if (opened_count > 0)
			opened_count--;
	}

	udelay(1000); //1ms

	opened_count = opened_cnt;
	while (opened_count) {
		vp9mgr_enable_clock(vbus_no_ctrl);
		if (opened_count > 0)
			opened_count--;
	}
	vp9mgr_hw_deassert();
#else
	vp9mgr_hw_reset();
#endif
}

void vp9mgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void vp9mgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void vp9mgr_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

int vp9mgr_request_irq(unsigned int irq,
		       irqreturn_t (*handler)(int irq, void *dev_id),
		       unsigned long frags, const char *device, void *dev_id)
{
	return request_irq(irq, handler, frags, device, dev_id);
}

unsigned long vp9mgr_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void vp9mgr_init_interrupt(void)
{
	//return;
}

int vp9mgr_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void vp9mgr_status_clear(unsigned int *base_addr)
{
	//return;
}

void vp9mgr_init_variable(void)
{
	cache_droped = 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vp9 devices driver");
MODULE_LICENSE("GPL");

#endif /*CONFIG_SUPPORT_TCC_G2V2_VP9*/
