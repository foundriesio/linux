// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
/*
 *   drivers/char/vpu/vpu_hevc_enc_mgr_sys.c
 *   Author:  <linux@telechips.com>
 *   Created: Apr 29, 2020
 *   Description: reset, clock and interrupt functions for TCC HEVC ENC
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC

#include "vpu_hevc_enc_mgr_sys.h"

static struct clk *fbus_vbus_clk = NULL;
static struct clk *fbus_chevcenc_clk = NULL;
static struct clk *fbus_bhevcenc_clk = NULL;
static struct clk *vbus_hevc_encoder_clk = NULL;

#if defined( VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
static struct reset_control *vbus_hevc_encoder_rst = NULL;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped = 0;

//#define USE_CLK_DYNAMIC_CTRL

//////////////////////////////////////////////////////////////////////////////
void vmgr_hevc_enc_enable_clock(int vbus_no_ctrl)
{
	V_DBG(DEBUG_RSTCLK, "@@ vmgr_hevc_enc_enable_clock");

	if (fbus_vbus_clk && !vbus_no_ctrl)
		clk_prepare_enable(fbus_vbus_clk);

	if (fbus_bhevcenc_clk)
		clk_prepare_enable(fbus_bhevcenc_clk);

	if (fbus_chevcenc_clk)
		clk_prepare_enable(fbus_chevcenc_clk);

	if (vbus_hevc_encoder_clk)
		clk_prepare_enable(vbus_hevc_encoder_clk);
}

void vmgr_hevc_enc_disable_clock(int vbus_no_ctrl)
{
	V_DBG(DEBUG_RSTCLK, "@@ vmgr_hevc_enc_disable_clock");

	if (vbus_hevc_encoder_clk)
		clk_disable_unprepare(vbus_hevc_encoder_clk);

	if(fbus_chevcenc_clk)
		clk_disable_unprepare(fbus_chevcenc_clk);

	if(fbus_bhevcenc_clk)
		clk_disable_unprepare(fbus_bhevcenc_clk);

#if !defined(VBUS_CLK_ALWAYS_ON)
	if(fbus_vbus_clk && !vbus_no_ctrl)
		clk_disable_unprepare(fbus_vbus_clk);
#endif
}

void vmgr_hevc_enc_get_clock(struct device_node *node)
{
	if(node == NULL)
	{
		V_DBG(DEBUG_VPU_ERROR, "device node is null");
	}

	fbus_vbus_clk = of_clk_get(node, 0);
	BUG_ON(IS_ERR(fbus_vbus_clk));

	fbus_chevcenc_clk = of_clk_get(node, 1);
	BUG_ON(IS_ERR(fbus_chevcenc_clk));

	fbus_bhevcenc_clk = of_clk_get(node, 2);
	BUG_ON(IS_ERR(fbus_bhevcenc_clk));

	vbus_hevc_encoder_clk = of_clk_get(node, 3);
	BUG_ON(IS_ERR(vbus_hevc_encoder_clk));
}

void vmgr_hevc_enc_put_clock(void)
{
	if (fbus_chevcenc_clk)
	{
		clk_put(fbus_chevcenc_clk);
		fbus_chevcenc_clk = NULL;
	}

	if (fbus_bhevcenc_clk)
	{
		clk_put(fbus_bhevcenc_clk);
		fbus_bhevcenc_clk = NULL;
	}

	if (vbus_hevc_encoder_clk)
	{
		clk_put(vbus_hevc_encoder_clk);
		vbus_hevc_encoder_clk = NULL;
	}

	if (fbus_vbus_clk)
	{
		clk_put(fbus_vbus_clk);
		fbus_vbus_clk = NULL;
	}
}

void vmgr_hevc_enc_change_clock(unsigned int width, unsigned int height)
{
#ifdef USE_CLK_DYNAMIC_CTRL
	static unsigned int prev_resolution = 0x0;
	unsigned long vbus_clk_value, bhevc_clk_value = 0, chevc_clk_value = 0;
	unsigned long curr_resolution = width * height;
	unsigned int bclk_changed = 0x0;
	int err;

//pll: Based on 1500/800
// => path to confiture PLL: bootable/bootloader/uboot/board/telechips/tcc8990_stb/clock.c, clock_init_early
/*
	tcc_set_pll(PLL_VIDEO_0, ENABLE, 800000000, 2);
	tcc_set_pll(PLL_VIDEO_1, ENABLE, 1500000000, 2);
*/
	if(prev_resolution == curr_resolution)
		return;

	prev_resolution = curr_resolution;

	if( curr_resolution > 1920*1088 )
	{
		vbus_clk_value = 800000000;
		bhevc_clk_value = 500000000;
		chevc_clk_value = 800000000;
	}
	else if( curr_resolution > 1280*720 )
	{
		vbus_clk_value = 500000000;
		bhevc_clk_value = 250000000;
		chevc_clk_value = 500000000;
	}
	else if( curr_resolution > 720*480 )
	{
		vbus_clk_value = 300000000;
		bhevc_clk_value = 200000000;
		chevc_clk_value = 300000000;
	}
	else// if( curr_resolution > 0 )
	{
		vbus_clk_value = 200000000;
		bhevc_clk_value = 150000000;
		chevc_clk_value = 200000000;
	}

	if(fbus_vbus_clk)
	{
		err = clk_set_rate(fbus_vbus_clk, vbus_clk_value);
		if (err)
			pr_err("cannot change fbus_vbus_clk rate to %ld: %d\n", vbus_clk_value, err);
		else
			bclk_changed |= 0x1;
	}

	if(fbus_bhevcenc_clk)
	{
		err = clk_set_rate(fbus_bhevcenc_clk, bhevc_clk_value);
		if (err)
			pr_err("cannot change fbus_bhevcenc_clk rate to %ld: %d\n", bhevc_clk_value, err);
		else
			bclk_changed |= 0x2;
	}

	if(fbus_chevcenc_clk){
		err = clk_set_rate(fbus_chevcenc_clk, chevc_clk_value);
		if (err)
			pr_err("cannot change fbus_chevcenc_clk rate to %ld: %d\n", chevc_clk_value, err);
		else
			bclk_changed |= 0x4;
	}

	printk("%s-%d :[0x%x]: %d x %d => clock : vbus(%d Mhz), hevc(%d/%d Mhz) \n", __func__, __LINE__,
			bclk_changed, width, height,
			(unsigned int)(vbus_clk_value/1000000),
			(unsigned int)(chevc_clk_value/1000000),
			(unsigned int)(bhevc_clk_value/1000000));
#endif
}

void vmgr_hevc_enc_get_reset(struct device_node *node)
{
#if defined( VIDEO_IP_DIRECT_RESET_CTRL)
	if(node == NULL) {
		printk("device node is null\n");
	}

	vbus_hevc_encoder_rst = of_reset_control_get(node, "hevc_encoder");
	BUG_ON(IS_ERR(vbus_hevc_encoder_rst));
#endif
}

void vmgr_hevc_enc_put_reset(void)
{
#if defined( VIDEO_IP_DIRECT_RESET_CTRL)
	if (vbus_hevc_encoder_rst)
	{
		reset_control_put(vbus_hevc_encoder_rst);
		vbus_hevc_encoder_rst = NULL;
	}
#endif
}

int vmgr_hevc_enc_get_reset_register(void)
{
#ifdef ENABLE_LOG_RESET_REGISTER
	return vetc_reg_read(vbus, 0x4);
#else
	return 0;
#endif
}

void vmgr_hevc_enc_hw_assert(void)
{
#if defined( VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(DEBUG_RSTCLK, "enter");

	if(vbus_hevc_encoder_rst)
	{
		V_DBG(DEBUG_RSTCLK, "Video bus hevc encoder reset: assert (rsr:0x%x)\n", vmgr_hevc_enc_get_reset_register());
		reset_control_assert(vbus_hevc_encoder_rst);
	}

	V_DBG(DEBUG_RSTCLK, "out!! (rsr:0x%x)", vmgr_hevc_enc_get_reset_register());
#endif
}

void vmgr_hevc_enc_hw_deassert(void)
{
#if defined( VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(DEBUG_RSTCLK, "enter");

	if(vbus_hevc_encoder_rst)
	{
		V_DBG(DEBUG_RSTCLK, "Video bus hevc encoder reset: deassert (rsr:0x%x)\n", vmgr_hevc_enc_get_reset_register());
		reset_control_deassert(vbus_hevc_encoder_rst);
	}

	V_DBG(DEBUG_RSTCLK, "out!! (rsr:0x%x)", vmgr_hevc_enc_get_reset_register());
#endif
}

void vmgr_hevc_enc_hw_reset(void)
{
#if defined( VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(DEBUG_RSTCLK, "enter");

	udelay(1000); //1ms

	vmgr_hevc_enc_hw_assert();

	udelay(1000); //1ms

	vmgr_hevc_enc_hw_deassert();

	udelay(1000); //1ms

	V_DBG(DEBUG_RSTCLK, "out (rsr:0x%x)", vmgr_hevc_enc_get_reset_register());
#endif
}

void vmgr_hevc_enc_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1 // unnecessary process: recommended by soc
	int opened_count = opened_cnt;

	V_DBG(DEBUG_RSTCLK, "opened_cnt: %d", opened_cnt);

	vmgr_hevc_enc_hw_assert();

	udelay(1000); //1ms

	while(opened_count)
	{
		vmgr_hevc_enc_disable_clock(vbus_no_ctrl);
		if(opened_count > 0)
			opened_count--;
	}

	udelay(1000); //1ms
	
	opened_count = opened_cnt;
	while(opened_count)
	{
		vmgr_hevc_enc_enable_clock(vbus_no_ctrl);
		if(opened_count > 0)
			opened_count--;
	}

	udelay(1000); //1ms

	vmgr_hevc_enc_hw_deassert();
#else
	vmgr_hevc_enc_hw_reset();
#endif
}

void vmgr_hevc_enc_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void vmgr_hevc_enc_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void vmgr_hevc_enc_free_irq(unsigned int irq, void * dev_id)
{
	free_irq(irq, dev_id);
}

int vmgr_hevc_enc_request_irq(unsigned int irq, irqreturn_t (*handler)(int irq, void *dev_id),
								unsigned long frags, const char * device, void * dev_id)
{
	return request_irq(irq, handler, frags, device, dev_id);
}
unsigned long vmgr_hevc_enc_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void vmgr_hevc_enc_init_interrupt(void)
{
}

int vmgr_hevc_enc_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void vmgr_hevc_enc_status_clear(unsigned int *base_addr)
{
#if defined(VPU_C5)
	vetc_reg_write(base_addr, 0x174, 0x00);
	vetc_reg_write(base_addr, 0x00C, 0x01);
#endif
}

int vmgr_hevc_enc_is_loadable(void)
{
	return 0;
}
EXPORT_SYMBOL(vmgr_hevc_enc_is_loadable);

void vmgr_hevc_enc_init_variable(void)
{
	cache_droped = 0;
}

#endif //CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu hevc enc device driver");
MODULE_LICENSE("GPL");
