// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2

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

#include "vpu_4k_d2_mgr_sys.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR_SYS: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR_SYS: " msg)
#define err(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_4K_D2_VMGR_SYS[Err]: " msg)
#define info(msg...)     V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR_SYS[Info]: " msg)

static struct clk *fbus_vbus_clk;	// = NULL;
static struct clk *fbus_chevc_clk;	// = NULL;
static struct clk *fbus_bhevc_clk;	// = NULL;
static struct clk *vbus_hevc_bus_clk;	// = NULL;
static struct clk *vbus_hevc_core_clk ;	//= NULL;	// for pwdn and vBus.

#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
static struct reset_control *vbus_hevc_bus_reset;	// = NULL;
// for pwdn and vBus.
static struct reset_control *vbus_hevc_core_reset;	// = NULL;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped;	// = 0;

#if 0
#define USE_CLK_DYNAMIC_CTRL
#endif

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

void vmgr_4k_d2_enable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	// BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_prepare_enable(fbus_vbus_clk);

	if (fbus_chevc_clk)
		clk_prepare_enable(fbus_chevc_clk);

	if (fbus_bhevc_clk)
		clk_prepare_enable(fbus_bhevc_clk);

	if (vbus_hevc_core_clk)
		clk_prepare_enable(vbus_hevc_core_clk);

	if (vbus_hevc_bus_clk)
		clk_prepare_enable(vbus_hevc_bus_clk);

#ifdef VBUS_QOS_MATRIX_CTL
	vbus_matrix();
#endif

#if (defined(CONFIG_ARCH_TCC899X) || \
	 defined(CONFIG_ARCH_TCC901X)) && \
	 defined(USE_TA_LOADING)
	if (!only_clk_ctrl) {
		int ret = vpu_optee_open();

		if (ret != 0) {
			err("vpu_optee_open: failed !! (ret: %d)",
				ret);
		} else {
			ret = vpu_optee_fw_load(TYPE_VPU_4K_D2);
			if (ret != 0)
				err("vpu_optee_fw_load: failed !! (ret: %d)",
					ret);
		}
	}
#endif
}

void vmgr_4k_d2_disable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	// ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vbus_hevc_bus_clk)
		clk_disable_unprepare(vbus_hevc_bus_clk);

	if (vbus_hevc_core_clk)
		clk_disable_unprepare(vbus_hevc_core_clk);

	if (fbus_bhevc_clk)
		clk_disable_unprepare(fbus_bhevc_clk);

	if (fbus_chevc_clk)
		clk_disable_unprepare(fbus_chevc_clk);

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

void vmgr_4k_d2_get_clock(struct device_node *node)
{
	if (node == NULL)
		err("device node is null");

	fbus_vbus_clk = of_clk_get(node, 0);
	BUG_ON(IS_ERR(fbus_vbus_clk));

	fbus_chevc_clk = of_clk_get(node, 1);
	BUG_ON(IS_ERR(fbus_chevc_clk));

	fbus_bhevc_clk = of_clk_get(node, 2);
	BUG_ON(IS_ERR(fbus_bhevc_clk));

	vbus_hevc_bus_clk = of_clk_get(node, 3);
	BUG_ON(IS_ERR(vbus_hevc_bus_clk));

	vbus_hevc_core_clk = of_clk_get(node, 4);
	BUG_ON(IS_ERR(vbus_hevc_core_clk));
}

void vmgr_4k_d2_put_clock(void)
{
	if (fbus_chevc_clk) {
		clk_put(fbus_chevc_clk);
		fbus_chevc_clk = NULL;
	}

	if (fbus_bhevc_clk) {
		clk_put(fbus_bhevc_clk);
		fbus_bhevc_clk = NULL;
	}

	if (vbus_hevc_bus_clk) {
		clk_put(vbus_hevc_bus_clk);
		vbus_hevc_bus_clk = NULL;
	}

	if (vbus_hevc_core_clk) {
		clk_put(vbus_hevc_core_clk);
		vbus_hevc_core_clk = NULL;
	}

	if (fbus_vbus_clk) {
		clk_put(fbus_vbus_clk);
		fbus_vbus_clk = NULL;
	}
}

void vmgr_4k_d2_change_clock(unsigned int width, unsigned int height)
{
#ifdef USE_CLK_DYNAMIC_CTRL
	static unsigned int prev_resolution; // = 0x0;
	unsigned long vbus_clk_value, bhevc_clk_value = 0, chevc_clk_value = 0;
	unsigned long curr_resolution = width * height;
	unsigned int bclk_changed = 0x0;
	int err;

//pll: Based on 1500/800
// => path to configure PLL:
// bootable/bootloader/uboot/board/telechips/tcc8990_stb/clock.c,
// clock_init_early

#if 0 //Remove below lines if not necessary
	tcc_set_pll(PLL_VIDEO_0, ENABLE, 800000000, 2);
	tcc_set_pll(PLL_VIDEO_1, ENABLE, 1500000000, 2);
#endif

	if (prev_resolution == curr_resolution)
		return;
	prev_resolution = curr_resolution;

	if (curr_resolution > 1920 * 1088) {
		vbus_clk_value = 800000000;
		bhevc_clk_value = 500000000;
		chevc_clk_value = 800000000;
	} else if (curr_resolution > 1280 * 720) {
		vbus_clk_value = 500000000;
		bhevc_clk_value = 250000000;
		chevc_clk_value = 500000000;
	} else if (curr_resolution > 720 * 480) {
		vbus_clk_value = 300000000;
		bhevc_clk_value = 200000000;
		chevc_clk_value = 300000000;
	} else {	//curr_resolution > 0
		vbus_clk_value = 200000000;
		bhevc_clk_value = 150000000;
		chevc_clk_value = 200000000;
	}

	if (fbus_vbus_clk) {
		err = clk_set_rate(fbus_bhevc_clk, vbus_clk_value);
		if (err) {
			pr_err("cannot change fbus_vbus_clk rate to %ld: %d\n",
			       vbus_clk_value, err);
		} else
			bclk_changed |= 0x1;
	}

	if (fbus_bhevc_clk) {
		err = clk_set_rate(fbus_bhevc_clk, bhevc_clk_value);
		if (err) {
			pr_err("cannot change fbus_bhevc_clk rate to %ld: %d\n",
			       bhevc_clk_value, err);
		} else
			bclk_changed |= 0x2;
	}

	if (fbus_chevc_clk) {
		err = clk_set_rate(fbus_chevc_clk, chevc_clk_value);
		if (err) {
			pr_err("cannot change fbus_chevc_clk rate to %ld: %d\n",
			       chevc_clk_value, err);
		} else
			bclk_changed |= 0x4;
	}

	V_DBG(VPU_DBG_RSTCLK,
		"[0x%x]: %d x %d => clock : vbus(%d Mhz), hevc(%d/%d Mhz)",
	     bclk_changed, width, height,
	     (unsigned int)(vbus_clk_value / 1000000),
	     (unsigned int)(chevc_clk_value / 1000000),
	     (unsigned int)(bhevc_clk_value / 1000000));
#endif
}

void vmgr_4k_d2_get_reset(struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
		err("device node is null");

	vbus_hevc_bus_reset = of_reset_control_get(node, "hevc_bus");
	BUG_ON(IS_ERR(vbus_hevc_bus_reset));

	vbus_hevc_core_reset = of_reset_control_get(node, "hevc_core");
	BUG_ON(IS_ERR(vbus_hevc_core_reset));
#endif
}

void vmgr_4k_d2_put_reset(void)
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

int vmgr_4k_d2_get_reset_register(void)
{
	return 0;
}

void vmgr_4k_d2_hw_assert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vbus_hevc_bus_reset)
		reset_control_assert(vbus_hevc_bus_reset);

	if (vbus_hevc_core_reset)
		reset_control_assert(vbus_hevc_core_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)",
		vmgr_4k_d2_get_reset_register());
#endif
}

void vmgr_4k_d2_hw_deassert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vbus_hevc_core_reset)
		reset_control_deassert(vbus_hevc_core_reset);

	if (vbus_hevc_bus_reset)
		reset_control_deassert(vbus_hevc_bus_reset);

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)",
		vmgr_4k_d2_get_reset_register());
#endif
}

void vmgr_4k_d2_hw_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	udelay(1000); //1ms

	vmgr_4k_d2_hw_assert();

	udelay(1000); //1ms

	vmgr_4k_d2_hw_deassert();

	udelay(1000); //1ms

	V_DBG(VPU_DBG_RSTCLK,
		"out (rsr:0x%x)", vmgr_4k_d2_get_reset_register());
#endif
}

void vmgr_4k_d2_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	vmgr_4k_d2_hw_assert();

	udelay(1000);	//1ms

	while (opened_count) {
		vmgr_4k_d2_disable_clock(vbus_no_ctrl, 0);
		if (opened_count > 0)
			opened_count--;
	}

	udelay(1000); //1ms

	opened_count = opened_cnt;
	while (opened_count) {
		vmgr_4k_d2_enable_clock(vbus_no_ctrl, 0);
		if (opened_count > 0)
			opened_count--;
	}

	vmgr_4k_d2_hw_deassert();
#else
	vmgr_4k_d2_hw_reset();
#endif
}

void vmgr_4k_d2_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void vmgr_4k_d2_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void vmgr_4k_d2_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

int vmgr_4k_d2_request_irq(unsigned int irq,
			   irqreturn_t (*handler)(int irq, void *dev_id),
			   unsigned long frags,
			   const char *device,
			    void *dev_id)
{
	return request_irq(irq, handler, frags, device, dev_id);
}

unsigned long vmgr_4k_d2_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void vmgr_4k_d2_init_interrupt(void)
{
}

int vmgr_4k_d2_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void vmgr_4k_d2_init_variable(void)
{
	cache_droped = 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("GPL");

#endif /*CONFIG_SUPPORT_TCC_WAVE512_4K_D2*/
