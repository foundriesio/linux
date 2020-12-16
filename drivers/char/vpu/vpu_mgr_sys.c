// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
#include "vpu_mgr_sys.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR_SYS: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR_SYS: " msg)
#define err(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_VPU_MGR_SYS[Err]: "msg)
#define info(msg...)     V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR_SYS[Info]: "msg)

static struct clk *fbus_vbus_clk;
static struct clk *fbus_xoda_clk;
static struct clk *vbus_xoda_clk;	// for pwdn and vBus.

#if defined(VBUS_CODA_CORE_CLK_CTRL)
static struct clk *vbus_core_clk;
#endif

#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
static struct reset_control *vbus_xoda_reset;	// for pwdn and vBus.
static struct reset_control *vbus_core_reset;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped;

void vmgr_enable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	// BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_prepare_enable(fbus_vbus_clk);
	if (fbus_xoda_clk)
		clk_prepare_enable(fbus_xoda_clk);
#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vbus_core_clk)
		clk_prepare_enable(vbus_core_clk);
#endif
	if (vbus_xoda_clk)
		clk_prepare_enable(vbus_xoda_clk);

#if (defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)) && \
	defined(USE_TA_LOADING)
	if (!only_clk_ctrl) {
		int ret = vpu_optee_open();

		if (ret != 0)
			err("vpu_optee_open: failed !! - ret [%d]", ret);
		else {
			ret = vpu_optee_fw_load(TYPE_VPU_D6);
			if (ret != 0)
				err("vpu_optee_fw_load: failed !! (ret: %d)",
					ret);
		}
	}
#endif
}

void vmgr_disable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	// ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vbus_xoda_clk)
		clk_disable_unprepare(vbus_xoda_clk);
#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vbus_core_clk)
		clk_disable_unprepare(vbus_core_clk);
#endif
	if (fbus_xoda_clk)
		clk_disable_unprepare(fbus_xoda_clk);
#if !defined(VBUS_CLK_ALWAYS_ON)
	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_disable_unprepare(fbus_vbus_clk);
#endif
#if (defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)) && \
	defined(USE_TA_LOADING)
	if (!only_clk_ctrl)
		vpu_optee_close();
#endif
}

void vmgr_get_clock(struct device_node *node)
{
	if (node == NULL)
		err("device node is null");

	fbus_vbus_clk = of_clk_get(node, 0);
	BUG_ON(IS_ERR(fbus_vbus_clk));

	fbus_xoda_clk = of_clk_get(node, 1);
	BUG_ON(IS_ERR(fbus_xoda_clk));

	vbus_xoda_clk = of_clk_get(node, 2);
	BUG_ON(IS_ERR(vbus_xoda_clk));

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	vbus_core_clk = of_clk_get(node, 3);
	BUG_ON(IS_ERR(vbus_core_clk));
#endif
}

void vmgr_put_clock(void)
{
	if (fbus_vbus_clk) {
		clk_put(fbus_vbus_clk);
		fbus_vbus_clk = NULL;
	}

	if (fbus_xoda_clk) {
		clk_put(fbus_xoda_clk);
		fbus_xoda_clk = NULL;
	}

	if (vbus_xoda_clk) {
		clk_put(vbus_xoda_clk);
		vbus_xoda_clk = NULL;
	}
#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vbus_core_clk) {
		clk_put(vbus_core_clk);
		vbus_core_clk = NULL;
	}
#endif
}

void vmgr_get_reset(struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
		err("device node is null");

	V_DBG(VPU_DBG_RSTCLK, "enter");
	vbus_xoda_reset = of_reset_control_get(node, "xoda_bus");
	BUG_ON(IS_ERR(vbus_xoda_reset));

	vbus_core_reset = of_reset_control_get(node, "xoda_core");
	BUG_ON(IS_ERR(vbus_core_reset));
#endif
}

void vmgr_put_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vbus_xoda_reset) {
		reset_control_put(vbus_xoda_reset);
		vbus_xoda_reset = NULL;
	}
	if (vbus_core_reset) {
		reset_control_put(vbus_core_reset);
		vbus_core_reset = NULL;
	}
#endif
}

int vmgr_get_reset_register(void)
{
	return 0;
}

void vmgr_hw_assert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	//  ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_xoda_reset)
		reset_control_assert(vbus_xoda_reset);

	if (vbus_core_reset)
		reset_control_assert(vbus_core_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_get_reset_register());
#endif
}

void vmgr_hw_deassert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	// BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_core_reset)
		reset_control_deassert(vbus_core_reset);

	if (vbus_xoda_reset)
		reset_control_deassert(vbus_xoda_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_get_reset_register());
#endif
}

void vmgr_hw_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	udelay(1000);		//1ms

	vmgr_hw_assert();

	udelay(1000);		//1ms

	vmgr_hw_deassert();

	udelay(1000);		//1ms
#endif
}

void vmgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	vmgr_hw_assert();

	while (opened_count) {
		vmgr_disable_clock(vbus_no_ctrl, 0);
		if (opened_count > 0)
			opened_count--;
	}

	udelay(1000);		//1ms

	opened_count = opened_cnt;
	while (opened_count) {
		vmgr_enable_clock(vbus_no_ctrl, 0);
		if (opened_count > 0)
			opened_count--;
	}

	vmgr_hw_deassert();
#else
	vmgr_hw_reset();
#endif
}

void vmgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void vmgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void vmgr_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

int vmgr_request_irq(unsigned int irq,
			irqreturn_t (*handler)(int irq, void *dev_id),
			unsigned long frags, const char *device, void *dev_id)
{
	return request_irq(irq, handler, frags, device, dev_id);
}

unsigned long vmgr_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void vmgr_init_interrupt(void)
{
}

int vmgr_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void vmgr_status_clear(unsigned int *base_addr)
{
#if defined(VPU_C5)
	vetc_reg_write(base_addr, 0x174, 0x00);
	vetc_reg_write(base_addr, 0x00C, 0x01);
#endif
}

void vmgr_init_variable(void)
{
	cache_droped = 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu devices driver");
MODULE_LICENSE("GPL");
