// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

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
#include "hevc_mgr_sys.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_HEVC_MGR_SYS: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_HEVC_MGR_SYS: " msg)
#define err(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_HEVC_MGR_SYS[Err]: "msg)
#define info(msg...)     V_DBG(VPU_DBG_INFO, "TCC_HEVC_MGR_SYS[Info]: "msg)

static struct clk *fbus_vbus_clk;	// = NULL;
static struct clk *fbus_chevc_clk;	//= NULL;
#ifdef CONFIG_ARCH_TCC898X
static struct clk *fbus_vhevc_clk;	// = NULL;
#endif
static struct clk *fbus_bhevc_clk;	// = NULL;
// for pwdn and vBus.
static struct clk *vbus_hevc_bus_clk;	// = NULL;
#if defined(VBUS_CODA_CORE_CLK_CTRL)
// for pwdn and vBus.
static struct clk *vbus_hevc_core_clk;	// = NULL;
#endif

#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
static struct reset_control *vbus_hevc_bus_reset;	// = NULL;
static struct reset_control *vbus_hevc_core_reset;	// = NULL;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped;	// = 0;

#ifdef VBUS_QOS_MATRIX_CTL
inline void vbus_matrix(void)
{
	vetc_reg_write(0x15444100, 0x00, 0x0);	// PRI Core0 read - port0
	vetc_reg_write(0x15444100, 0x04, 0x0);	// PRI Core0 write - port0

	vetc_reg_write(0x15445100, 0x00, 0x3);	// PROC read - port1
	vetc_reg_write(0x15445100, 0x04, 0x3);	// PROC read - port1

	vetc_reg_write(0x15446100, 0x00, 0x2);	// SDMA read - port2
	vetc_reg_write(0x15446100, 0x04, 0x2);	// SDMA read - port2

	vetc_reg_write(0x15447100, 0x00, 0x0);	// SECON Core0 read - port3
	vetc_reg_write(0x15447100, 0x04, 0x0);	// SECON Core0 read - port3

	vetc_reg_write(0x15449100, 0x00, 0x1);	// DMA read - port4
	vetc_reg_write(0x15449100, 0x04, 0x1);	// DMA read - port4

	vetc_reg_write(0x1544A100, 0x00, 0x0);	// PRI Core1 read - port5
	vetc_reg_write(0x1544A100, 0x04, 0x0);	// PRI Core1 read - port5

	vetc_reg_write(0x1544B100, 0x00, 0x0);	// SECON Core1 read - port6
	vetc_reg_write(0x1544B100, 0x04, 0x0);	// SECON Core1 read - port6
}
#endif

void hmgr_enable_clock(int vbus_no_ctrl)
{
	//  BCLK > CCLK > ACLK
	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_prepare_enable(fbus_vbus_clk);

	if (fbus_chevc_clk)
		clk_prepare_enable(fbus_chevc_clk);

#ifdef CONFIG_ARCH_TCC898X
	if (fbus_vhevc_clk)
		clk_prepare_enable(fbus_vhevc_clk);
#endif

	if (fbus_bhevc_clk)
		clk_prepare_enable(fbus_bhevc_clk);

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vbus_hevc_core_clk)
		clk_prepare_enable(vbus_hevc_core_clk);
#endif

	if (vbus_hevc_bus_clk)
		clk_prepare_enable(vbus_hevc_bus_clk);

#ifdef VBUS_QOS_MATRIX_CTL
	vbus_matrix();
#endif
}

void hmgr_disable_clock(int vbus_no_ctrl)
{
	// ACLK > CCLK > BCLK
	if (vbus_hevc_bus_clk)
		clk_disable_unprepare(vbus_hevc_bus_clk);

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vbus_hevc_core_clk)
		clk_disable_unprepare(vbus_hevc_core_clk);
#endif

	if (fbus_bhevc_clk)
		clk_disable_unprepare(fbus_bhevc_clk);

#ifdef CONFIG_ARCH_TCC898X
	if (fbus_vhevc_clk)
		clk_disable_unprepare(fbus_vhevc_clk);
#endif

	if (fbus_chevc_clk)
		clk_disable_unprepare(fbus_chevc_clk);

#if !defined(VBUS_CLK_ALWAYS_ON)
	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_disable_unprepare(fbus_vbus_clk);
#endif
}

void hmgr_get_clock(struct device_node *node)
{
	int i = 0;

	if (node == NULL)
		err("device node is null");

	i = 0;
	fbus_vbus_clk = of_clk_get(node, i);
	BUG_ON(IS_ERR(fbus_vbus_clk));

	i += 1;
	fbus_chevc_clk = of_clk_get(node, i);
	BUG_ON(IS_ERR(fbus_chevc_clk));

#ifdef CONFIG_ARCH_TCC898X
	i += 1;
	fbus_vhevc_clk = of_clk_get(node, i);
	BUG_ON(IS_ERR(fbus_vhevc_clk));
#endif

	i += 1;
	fbus_bhevc_clk = of_clk_get(node, i);
	BUG_ON(IS_ERR(fbus_bhevc_clk));

	i += 1;
	vbus_hevc_bus_clk = of_clk_get(node, i);
	BUG_ON(IS_ERR(vbus_hevc_bus_clk));

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	i += 1;
	vbus_hevc_core_clk = of_clk_get(node, i);
	BUG_ON(IS_ERR(vbus_hevc_core_clk));
#endif
}

void hmgr_put_clock(void)
{
	if (fbus_chevc_clk) {
		clk_put(fbus_chevc_clk);
		fbus_chevc_clk = NULL;
	}

#ifdef CONFIG_ARCH_TCC898X
	if (fbus_vhevc_clk) {
		clk_put(fbus_vhevc_clk);
		fbus_vhevc_clk = NULL;
	}
#endif

	if (fbus_bhevc_clk) {
		clk_put(fbus_bhevc_clk);
		fbus_bhevc_clk = NULL;
	}

	if (vbus_hevc_bus_clk) {
		clk_put(vbus_hevc_bus_clk);
		vbus_hevc_bus_clk = NULL;
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vbus_hevc_core_clk) {
		clk_put(vbus_hevc_core_clk);
		vbus_hevc_core_clk = NULL;
	}
#endif

	if (fbus_vbus_clk) {
		clk_put(fbus_vbus_clk);
		fbus_vbus_clk = NULL;
	}
}

void hmgr_get_reset(struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
		err("device node is null");

	V_DBG(VPU_DBG_RSTCLK, "enter");
	vbus_hevc_bus_reset = of_reset_control_get(node, "hevc_bus");
	BUG_ON(IS_ERR(vbus_hevc_bus_reset));

	vbus_hevc_core_reset = of_reset_control_get(node, "hevc_core");
	BUG_ON(IS_ERR(vbus_hevc_core_reset));
#endif
}

void hmgr_put_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vbus_hevc_bus_reset) {
		reset_control_put(vbus_hevc_bus_reset);
		vbus_hevc_bus_reset = NULL;
	}

	if (vbus_hevc_core_reset) {
		reset_control_put(vbus_hevc_core_reset);
		vbus_hevc_core_reset = NULL;
	}
#endif
}

int hmgr_get_reset_register(void)
{
	return 0;
}

void hmgr_hw_assert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	// ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vbus_hevc_bus_reset)
		reset_control_assert(vbus_hevc_bus_reset);

	if (vbus_hevc_core_reset)
		reset_control_assert(vbus_hevc_core_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", hmgr_get_reset_register());
#endif
}

void hmgr_hw_deassert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	//  BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vbus_hevc_core_reset)
		reset_control_deassert(vbus_hevc_core_reset);

	if (vbus_hevc_bus_reset)
		reset_control_deassert(vbus_hevc_bus_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", hmgr_get_reset_register());
#endif
}

void hmgr_hw_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)

	udelay(1000);

	hmgr_hw_assert();

	udelay(1000);

	hmgr_hw_deassert();

	udelay(1000);

#endif
}

void hmgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	hmgr_hw_assert();
	while (opened_count) {
		hmgr_disable_clock(vbus_no_ctrl);
		if (opened_count > 0)
			opened_count--;
	}

	udelay(1000);

	opened_count = opened_cnt;
	while (opened_count) {
		hmgr_enable_clock(vbus_no_ctrl);
		if (opened_count > 0)
			opened_count--;
	}
	hmgr_hw_deassert();
#else
	hmgr_hw_reset();
#endif
}

void hmgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void hmgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void hmgr_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

int hmgr_request_irq(unsigned int irq,
		     irqreturn_t (*handler)(int irq, void *dev_id),
		     unsigned long frags, const char *device, void *dev_id)
{
	return request_irq(irq, handler, frags, device, dev_id);
}

unsigned long hmgr_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void hmgr_init_interrupt(void)
{
}

int hmgr_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void hmgr_status_clear(unsigned int *base_addr)
{
//	return;
}

void hmgr_init_variable(void)
{
	cache_droped = 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("GPL");

#endif /*CONFIG_SUPPORT_TCC_WAVE410_HEVC*/
