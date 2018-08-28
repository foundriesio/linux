/*
 *   FileName : vpu_mgr_sys.c
 *   Description :
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

#include "vpu_mgr_sys.h"

#define dprintk(msg...) //printk( "TCC_VPU_MGR_SYS: " msg);
#define detailk(msg...) //printk( "TCC_VPU_MGR_SYS: " msg);
#define err(msg...) printk("TCC_VPU_MGR_SYS[Err]: "msg);
#define info(msg...) printk("TCC_VPU_MGR_SYS[Info]: "msg);

static struct clk *fbus_vbus_clk = NULL;
static struct clk *fbus_xoda_clk = NULL;
static struct clk *vbus_xoda_clk = NULL; // for pwdn and vBus.
#if defined(VBUS_CODA_CORE_CLK_CTRL)
static struct clk *vbus_core_clk = NULL;
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped = 0;

//////////////////////////////////////////////////////////////////////////////
void vmgr_enable_clock(void)
{
    if (fbus_vbus_clk)
        clk_prepare_enable(fbus_vbus_clk);
    if (fbus_xoda_clk)
        clk_prepare_enable(fbus_xoda_clk);
#if defined(VBUS_CODA_CORE_CLK_CTRL)
    if (vbus_core_clk)
        clk_prepare_enable(vbus_core_clk);
#endif
    if (vbus_xoda_clk)
        clk_prepare_enable(vbus_xoda_clk);

#if defined(CONFIG_ARCH_TCC899X) && defined(USE_TA_LOADING)
    {
        int ret = vpu_optee_open();
        if (ret != 0) {
            printk("vpu_optee_open: failed !! - ret [%d]\n", ret);
        } else {
            ret = vpu_optee_fw_load(TYPE_VPU_D6);
            if (ret != 0) {
                printk("[%s] vpu_optee_fw_load: failed !! (ret: %d)\n", __func__, ret);
            }
        }
    }
#endif
}

void vmgr_disable_clock(void)
{
#if defined(VBUS_CODA_CORE_CLK_CTRL)
    if (vbus_core_clk)
        clk_disable_unprepare(vbus_core_clk);
#endif
    if (vbus_xoda_clk)
        clk_disable_unprepare(vbus_xoda_clk);
    if (fbus_xoda_clk)
        clk_disable_unprepare(fbus_xoda_clk);
#if !defined(VBUS_CLK_ALWAYS_ON)
    if (fbus_vbus_clk)
        clk_disable_unprepare(fbus_vbus_clk);
#endif
#if defined(CONFIG_ARCH_TCC899X) && defined(USE_TA_LOADING)
    vpu_optee_close();
#endif
}

void vmgr_get_clock(struct device_node *node)
{

    if(node == NULL) {
        printk("device node is null\n");
    }
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

void vmgr_enable_irq(unsigned int irq)
{
    enable_irq(irq);
}

void vmgr_disable_irq(unsigned int irq)
{
    disable_irq(irq);
}

void vmgr_free_irq(unsigned int irq, void * dev_id) {
    free_irq(irq, dev_id);
}

int vmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int irq, void *dev_id), unsigned long frags, const char * device, void * dev_id) {
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

int vmgr_is_loadable(void)
{
    return 0;
}
EXPORT_SYMBOL(vmgr_is_loadable);

void vmgr_init_variable(void)
{
    cache_droped = 0;
}


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu devices driver");
MODULE_LICENSE("GPL");
