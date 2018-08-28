/*
 *   FileName    : vp9_mgr_sys.c
 *   Description : TCC VP9 h/w block for Verisilicon VPU IP
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

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9

#include "vp9_mgr_sys.h"

#define dprintk(msg...)	//printk( "TCC_VP9_MGR_SYS: " msg);
#define detailk(msg...) //printk( "TCC_VP9_MGR_SYS: " msg);
#define err(msg...) printk("TCC_VP9_MGR_SYS[Err]: "msg);
#define info(msg...) printk("TCC_VP9_MGR_SYS[Info]: "msg);

static struct clk *fbus_vbus_clk = NULL;
static struct clk *vbus_vp9_clk = NULL; // for pwdn and vBus.

extern int tccxxx_sync_player(int sync);
static int cache_droped = 0;

//////////////////////////////////////////////////////////////////////////////
void vp9mgr_enable_clock(void)
{
	if (fbus_vbus_clk)
		clk_prepare_enable(fbus_vbus_clk);
	if (vbus_vp9_clk)
		clk_prepare_enable(vbus_vp9_clk);
}

void vp9mgr_disable_clock(void)
{
	if (vbus_vp9_clk)
		clk_disable_unprepare(vbus_vp9_clk);
#if !defined(VBUS_CLK_ALWAYS_ON)
	if (fbus_vbus_clk)
		clk_disable_unprepare(fbus_vbus_clk);
#endif
}

void vp9mgr_get_clock(struct device_node *node)
{
    if(node == NULL) {
        printk("device node is null\n");
    }

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

void vp9mgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void vp9mgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void vp9mgr_free_irq(unsigned int irq, void * dev_id) {
	free_irq(irq, dev_id);
}

int vp9mgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int irq, void *dev_id),
                       unsigned long frags, const char * device, void * dev_id) {
    return request_irq(irq, handler, frags, device, dev_id);
}

unsigned long vp9mgr_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

void vp9mgr_init_interrupt(void)
{
    return;
}

int vp9mgr_BusPrioritySetting(int mode, int type)
{
	return 0;
}

void vp9mgr_status_clear(unsigned int *base_addr)
{
    return;
}

int vp9mgr_is_loadable(void)
{
	return 0;
}
EXPORT_SYMBOL(vp9mgr_is_loadable);

void vp9mgr_init_variable(void)
{
	cache_droped = 0;
}


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vp9 devices driver");
MODULE_LICENSE("GPL");

#endif // CONFIG_SUPPORT_TCC_G2V2_VP9
