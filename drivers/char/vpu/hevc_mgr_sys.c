/*
 *   FileName    : hevc_mgr_sys.h
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

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

#include "hevc_mgr_sys.h"

#define dprintk(msg...)	//printk( "TCC_HEVC_MGR_SYS: " msg);
#define detailk(msg...) //printk( "TCC_HEVC_MGR_SYS: " msg);
#define err(msg...) printk("TCC_HEVC_MGR_SYS[Err]: "msg);
#define info(msg...) printk("TCC_HEVC_MGR_SYS[Info]: "msg);

static struct clk *fbus_vbus_clk = NULL;
static struct clk *fbus_chevc_clk = NULL;
#ifdef CONFIG_ARCH_TCC898X
static struct clk *fbus_vhevc_clk = NULL;
#endif
static struct clk *fbus_bhevc_clk = NULL;
static struct clk *vbus_hevc_bus_clk = NULL; // for pwdn and vBus.
#if defined(VBUS_CODA_CORE_CLK_CTRL)
static struct clk *vbus_hevc_core_clk = NULL; // for pwdn and vBus.
#endif

extern int tccxxx_sync_player(int sync);
static int cache_droped = 0;

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

void hmgr_enable_clock(int vbus_no_ctrl)
{
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

    if(node == NULL) {
        printk("device node is null\n");
    }

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

void hmgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

    while(opened_count)
    {
        hmgr_disable_clock(vbus_no_ctrl);
        if(opened_count > 0)
            opened_count--;
    }

    //msleep(1);
    opened_count = opened_cnt;
    while(opened_count)
    {
        hmgr_enable_clock(vbus_no_ctrl);
        if(opened_count > 0)
            opened_count--;
    }
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

void hmgr_free_irq(unsigned int irq, void * dev_id) {
	free_irq(irq, dev_id);
}

int hmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int irq, void *dev_id),
                     unsigned long frags, const char * device, void * dev_id) {
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
    return;
}

int hmgr_is_loadable(void)
{
	return 0;
}
EXPORT_SYMBOL(hmgr_is_loadable);

void hmgr_init_variable(void)
{
	cache_droped = 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("GPL");

#endif // CONFIG_SUPPORT_TCC_WAVE410_HEVC
