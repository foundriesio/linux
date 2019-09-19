/*
 *   FileName    : vp9_mgr_sys.c
 *   Author:  <linux@telechips.com>
 *   Created: June 10, 2008
 *   Description: TCC VPU h/w block
 *
 *   Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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

void vp9mgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

    while(opened_count)
    {
        vp9mgr_disable_clock(vbus_no_ctrl, 0);
        if(opened_count > 0)
            opened_count--;
    }

    //msleep(1);
    opened_count = opened_cnt;
    while(opened_count)
    {
        vp9mgr_enable_clock(vbus_no_ctrl, 0);
        if(opened_count > 0)
            opened_count--;
    }
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
