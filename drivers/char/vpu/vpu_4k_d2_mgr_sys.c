/*
 *   FileName : vpu_4k_d2_mgr_sys.c
 *   Description : TCC VPU h/w block
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips Inc.
 *   All rights reserved
 *
 * This source code contains confidential information of Telechips.
 * Any unauthorized use without a written permission of Telechips including
 * not limited to re-distribution in source or binary form is strictly prohibited.
 * This source code is provided "AS IS" and nothing contained in this source code
 * shall constitute any express or implied warranty of any kind,
 * including without limitation, any warranty of merchantability,
 * fitness for a particular purpose or non-infringement of any patent,
 * copyright or other third party intellectual property right.
 * No warranty is made, express or implied, regarding the informations accuracy,
 * completeness, or performance.
 * In no event shall Telechips be liable for any claim, damages or other liability
 * arising from, out of or in connection with this source code or the use
 * in the source code.
 * This source code is provided subject to the terms of a Mutual Non-Disclosure
 * Agreement between Telechips and Company.
 *
 */

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2

#include "vpu_4k_d2_mgr_sys.h"

#define dprintk(msg...) //printk( "TCC_4K_D2_VMGR_SYS: " msg);
#define detailk(msg...) //printk( "TCC_4K_D2_VMGR_SYS: " msg);
#define err(msg...) printk("TCC_4K_D2_VMGR_SYS[Err]: "msg);
#define info(msg...) printk("TCC_4K_D2_VMGR_SYS[Info]: "msg);

static struct clk *fbus_vbus_clk = NULL;
static struct clk *fbus_chevc_clk = NULL;
static struct clk *fbus_bhevc_clk = NULL;
static struct clk *vbus_hevc_bus_clk = NULL;
static struct clk *vbus_hevc_core_clk = NULL; // for pwdn and vBus.

extern int tccxxx_sync_player(int sync);
static int cache_droped = 0;

//#define USE_CLK_DYNAMIC_CTRL

//////////////////////////////////////////////////////////////////////////////
#ifdef VBUS_QOS_MATRIX_CTL
inline void vbus_matrix(void)
{
    vetc_reg_write(0x15444100, 0x00, 0x0); // PRI Core0 read - port0
    vetc_reg_write(0x15444100, 0x04, 0x0); // PRI Core0 write - port0

    vetc_reg_write(0x15445100, 0x00, 0x3); // PROC read - port1
    vetc_reg_write(0x15445100, 0x04, 0x3); // PROC read - port1

    vetc_reg_write(0x15446100, 0x00, 0x2); // SDMA read - port2
    vetc_reg_write(0x15446100, 0x04, 0x2); // SDMA read - port2

    vetc_reg_write(0x15447100, 0x00, 0x0); // SECON Core0 read - port3
    vetc_reg_write(0x15447100, 0x04, 0x0); // SECON Core0 read - port3

    vetc_reg_write(0x15449100, 0x00, 0x1); // DMA read - port4
    vetc_reg_write(0x15449100, 0x04, 0x1); // DMA read - port4

    vetc_reg_write(0x1544A100, 0x00, 0x0); // PRI Core1 read - port5
    vetc_reg_write(0x1544A100, 0x04, 0x0); // PRI Core1 read - port5

    vetc_reg_write(0x1544B100, 0x00, 0x0); // SECON Core1 read - port6
    vetc_reg_write(0x1544B100, 0x04, 0x0); // SECON Core1 read - port6
}
#endif

void vmgr_4k_d2_enable_clock(void)
{
    dprintk("@@ vmgr_4k_d2_enable_clock \n");
    if (fbus_vbus_clk)
        clk_prepare_enable(fbus_vbus_clk);
    if (fbus_chevc_clk)
        clk_prepare_enable(fbus_chevc_clk);
    if (fbus_bhevc_clk)
        clk_prepare_enable(fbus_bhevc_clk);
    if (vbus_hevc_bus_clk)
        clk_prepare_enable(vbus_hevc_bus_clk);
    if (vbus_hevc_core_clk)
        clk_prepare_enable(vbus_hevc_core_clk);

#ifdef VBUS_QOS_MATRIX_CTL
    vbus_matrix();
#endif

#if defined(CONFIG_ARCH_TCC899X) && defined(USE_TA_LOADING)
    {
        int ret = vpu_optee_open();
        if (ret != 0) {
            printk("[%s] vpu_optee_open: failed !! (ret: %d)\n", __func__, ret);
        } else {
            ret = vpu_optee_fw_load(TYPE_VPU_4K_D2);
            if (ret != 0) {
                printk("[%s] vpu_optee_fw_load: failed !! (ret: %d)\n", __func__, ret);
            }
        }
    }
#endif
}

void vmgr_4k_d2_disable_clock(void)
{
    dprintk("@@ vmgr_4k_d2_disable_clock \n");

    if (vbus_hevc_bus_clk)
        clk_disable_unprepare(vbus_hevc_bus_clk);
    if (vbus_hevc_core_clk)
        clk_disable_unprepare(vbus_hevc_core_clk);
    if (fbus_bhevc_clk)
        clk_disable_unprepare(fbus_bhevc_clk);
    if (fbus_chevc_clk)
        clk_disable_unprepare(fbus_chevc_clk);
#if !defined(VBUS_CLK_ALWAYS_ON)
    if (fbus_vbus_clk)
        clk_disable_unprepare(fbus_vbus_clk);
#endif

#if defined(CONFIG_ARCH_TCC899X) && defined(USE_TA_LOADING)
    vpu_optee_close();
#endif
}

void vmgr_4k_d2_get_clock(struct device_node *node)
{
    if(node == NULL) {
        printk("device node is null\n");
    }

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
		vbus_clk_value 	= 800000000;
		bhevc_clk_value = 500000000;
		chevc_clk_value = 800000000;
	}
	else if( curr_resolution > 1280*720 )
	{
		vbus_clk_value 	= 500000000;
		bhevc_clk_value = 250000000;
		chevc_clk_value = 500000000;
	}
	else if( curr_resolution > 720*480 )
	{
		vbus_clk_value 	= 300000000;
		bhevc_clk_value = 200000000;
		chevc_clk_value = 300000000;
	}
	else// if( curr_resolution > 0 )
	{
		vbus_clk_value 	= 200000000;
		bhevc_clk_value = 150000000;
		chevc_clk_value = 200000000;
	}

	if(fbus_vbus_clk){
		err = clk_set_rate(fbus_bhevc_clk, vbus_clk_value);
		if (err) {
	        pr_err("cannot change fbus_vbus_clk rate to %ld: %d\n",
	               vbus_clk_value, err);
		}
		else
			bclk_changed |= 0x1;
	}

	if(fbus_bhevc_clk){
		err = clk_set_rate(fbus_bhevc_clk, bhevc_clk_value);
		if (err) {
	        pr_err("cannot change fbus_bhevc_clk rate to %ld: %d\n",
	               bhevc_clk_value, err);
		}
		else
			bclk_changed |= 0x2;
	}

	if(fbus_chevc_clk){
 		err = clk_set_rate(fbus_chevc_clk, chevc_clk_value);
		if (err) {
	        pr_err("cannot change fbus_chevc_clk rate to %ld: %d\n",
	               chevc_clk_value, err);
		}
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

void vmgr_4k_d2_enable_irq(unsigned int irq)
{
    enable_irq(irq);
}

void vmgr_4k_d2_disable_irq(unsigned int irq)
{
    disable_irq(irq);
}

void vmgr_4k_d2_free_irq(unsigned int irq, void * dev_id) {
    free_irq(irq, dev_id);
}

int vmgr_4k_d2_request_irq(unsigned int irq,
                           irqreturn_t (*handler)(int irq, void *dev_id),
                           unsigned long frags,
                           const char * device,
                           void * dev_id)
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

int vmgr_4k_d2_is_loadable(void)
{
    return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_is_loadable);

void vmgr_4k_d2_init_variable(void)
{
    cache_droped = 0;
}


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("GPL");

#endif // CONFIG_SUPPORT_TCC_WAVE512_4K_D2
