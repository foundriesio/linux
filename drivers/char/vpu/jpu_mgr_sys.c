// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_JPU

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
#include "jpu_mgr_sys.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS:" msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS:" msg)
#define err(msg...)      V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS[Err]:" msg)
#define info(msg...)     V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS[Info]:" msg)

static struct clk *vbus_jpeg_clk;	// = NULL;

#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
static struct reset_control *vbus_jpeg_reset;	// = NULL;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped;	// = 0;

void jmgr_enable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	if (vbus_jpeg_clk)
		clk_prepare_enable(vbus_jpeg_clk);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	if (!only_clk_ctrl) {
		int ret = jpu_optee_open();

		if (ret != 0)
			err("jpu_optee_open: failed !! - ret [%d]", ret);
		else
			dprintk("jpu_optee_open: success !!");
	}
#endif
}

void jmgr_disable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	if (vbus_jpeg_clk)
		clk_disable_unprepare(vbus_jpeg_clk);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	if (!only_clk_ctrl) {
		int ret = jpu_optee_close();

		if (ret != 0)
			err("jpu_optee_close: failed !! - ret [%d]", ret);
		else
			dprintk("jpu_optee_close: success !!");
	}
#endif
}

void jmgr_get_clock(struct device_node *node)
{
	if (node == NULL)
		err("device node is null");

	vbus_jpeg_clk = of_clk_get(node, 1);
	BUG_ON(IS_ERR(vbus_jpeg_clk));
}

void jmgr_put_clock(void)
{
	if (vbus_jpeg_clk) {
		clk_put(vbus_jpeg_clk);
		vbus_jpeg_clk = NULL;
	}
}

void jmgr_get_reset(struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
		pr_info("device node is null\n");

	vbus_jpeg_reset = of_reset_control_get(node, "jpeg");
	BUG_ON(IS_ERR(vbus_jpeg_reset));
#endif
}

void jmgr_put_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vbus_jpeg_reset) {
		reset_control_put(vbus_jpeg_reset);
		vbus_jpeg_reset = NULL;
	}
#endif
}

int jmgr_get_reset_register(void)
{
	return 0;
}

void jmgr_hw_assert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_jpeg_reset)
		reset_control_assert(vbus_jpeg_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", jmgr_get_reset_register());
#endif
}

void jmgr_hw_deassert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_jpeg_reset)
		reset_control_deassert(vbus_jpeg_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", jmgr_get_reset_register());
#endif
}

void jmgr_hw_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	udelay(1000);

	jmgr_hw_assert();

	udelay(1000);

	jmgr_hw_deassert();

	udelay(1000);
#endif
}

void jmgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	jmgr_hw_assert();

	while (opened_count) {
		jmgr_disable_clock(vbus_no_ctrl, 0);
		if (opened_count > 0)
			opened_count--;
	}

	udelay(1000);

	opened_count = opened_cnt;
	while (opened_count) {
		jmgr_enable_clock(vbus_no_ctrl, 0);
		if (opened_count > 0)
			opened_count--;
	}

	jmgr_hw_deassert();
#else
	jmgr_hw_reset();
#endif
}

void jmgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void jmgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void jmgr_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

int jmgr_request_irq(unsigned int irq,
		     irqreturn_t (*handler)(int irq, void *dev_id),
		     unsigned long frags, const char *device, void *dev_id)
{
	return request_irq(irq, handler, frags, device, dev_id);
}

unsigned long jmgr_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void jmgr_init_interrupt(void)
{
//	return;
}

int jmgr_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void jmgr_status_clear(unsigned int *base_addr)
{
//	return;
}

void jmgr_init_variable(void)
{
	cache_droped = 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu devices driver");
MODULE_LICENSE("GPL");

#endif /*CONFIG_SUPPORT_TCC_JPU*/
