/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <asm/io.h>

#include <linux/timekeeping.h>

#include "tcc_asrc_drv.h"
#include "tcc_asrc_m2m.h"
#include "tcc_asrc.h"
#include "tcc_pl080.h"

int tcc_asrc_volume_gain(struct tcc_asrc_t *asrc, 
		uint32_t asrc_ch,
		uint32_t gain)
{
	tcc_asrc_set_volume_gain(asrc->asrc_reg, asrc_ch, gain); // 0 dB
	tcc_asrc_volume_enable(asrc->asrc_reg, asrc_ch, 1);

	return 0;
}

int tcc_asrc_volume_ramp(struct tcc_asrc_t *asrc, 
		uint32_t asrc_ch,
		uint32_t gain, 
		uint32_t dn_time, uint32_t dn_wait,
		uint32_t up_time, uint32_t up_wait)
{
	void __iomem *asrc_reg = asrc->asrc_reg;

	tcc_asrc_set_volume_ramp_dn_time(asrc_reg, asrc_ch, dn_time, dn_wait);
	tcc_asrc_set_volume_ramp_up_time(asrc_reg, asrc_ch, up_time, up_wait);
	tcc_asrc_set_volume_ramp_gain(asrc_reg, asrc_ch, gain); // 0 dB
	tcc_asrc_volume_ramp_enable(asrc_reg, asrc_ch, 1);

	return 0;
}

int tcc_asrc_tx_dma_start(struct tcc_asrc_t *asrc, int asrc_ch)
{
	uint32_t dma_tx_ch = asrc_ch;

	if (asrc_ch > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_set_first_lli(asrc->pl080_reg, dma_tx_ch, &asrc->pair[asrc_ch].txbuf.lli_virt[0]);
	tcc_pl080_set_channel_mem2per(asrc->pl080_reg, dma_tx_ch, dma_tx_ch, 1, 1);
	tcc_pl080_channel_enable(asrc->pl080_reg, dma_tx_ch, 1);
	tcc_pl080_channel_sync_mode(asrc->pl080_reg, dma_tx_ch, 1);

	return 0;
}

int tcc_asrc_tx_dma_stop(struct tcc_asrc_t *asrc, int asrc_ch)
{
	uint32_t dma_tx_ch = asrc_ch;

	if (asrc_ch > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_channel_enable(asrc->pl080_reg, dma_tx_ch, 0);

	return 0;
}

int tcc_asrc_tx_dma_halt(struct tcc_asrc_t *asrc, int asrc_ch)
{
	uint32_t dma_tx_ch = asrc_ch;

	if (asrc_ch > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_halt_enable(asrc->pl080_reg, dma_tx_ch, 1);

	return 0;
}

int tcc_asrc_rx_dma_start(struct tcc_asrc_t *asrc, int asrc_ch)
{
	uint32_t dma_rx_ch = asrc_ch + NUM_OF_ASRC_PAIR;

	if (asrc_ch > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_set_first_lli(asrc->pl080_reg, dma_rx_ch, &asrc->pair[asrc_ch].rxbuf.lli_virt[0]);
	tcc_pl080_set_channel_per2mem(asrc->pl080_reg, dma_rx_ch, dma_rx_ch, 1, 1);
	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, 1);
	tcc_pl080_channel_sync_mode(asrc->pl080_reg, dma_rx_ch, 1);

	return 0;
}

int tcc_asrc_rx_dma_stop(struct tcc_asrc_t *asrc, int asrc_ch)
{
	uint32_t dma_rx_ch = asrc_ch + NUM_OF_ASRC_PAIR;

	if (asrc_ch > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, 0);

	return 0;
}

int tcc_asrc_rx_dma_halt(struct tcc_asrc_t *asrc, int asrc_ch)
{
	uint32_t dma_rx_ch = asrc_ch + NUM_OF_ASRC_PAIR;

	if (asrc_ch > NUM_OF_ASRC_PAIR)
		return -1;

	tcc_pl080_halt_enable(asrc->pl080_reg, dma_rx_ch, 1);

	return 0;
}

// M2M SYNC
static void __tcc_asrc_m2m_sync_setup(struct tcc_asrc_t *asrc, 
		int asrc_ch, 
		enum tcc_asrc_fifo_fmt_t tx_fmt, 
		enum tcc_asrc_fifo_mode_t tx_mode,
		enum tcc_asrc_fifo_fmt_t rx_fmt, 
		enum tcc_asrc_fifo_mode_t rx_mode,
		uint32_t ratio_shift22)
{
	void __iomem *asrc_reg = asrc->asrc_reg;
	enum tcc_asrc_component_t asrc_comp;

	asrc_comp = (asrc_ch == 0) ? TCC_ASRC0 :
		(asrc_ch == 1) ? TCC_ASRC1 :
		(asrc_ch == 2) ? TCC_ASRC2 : TCC_ASRC3;

	tcc_asrc_set_inport_path(asrc_reg, asrc_ch, TCC_ASRC_PATH_DMA);
	tcc_asrc_set_outport_path(asrc_reg, asrc_ch, TCC_ASRC_PATH_DMA);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_ch, ratio_shift22); 
	tcc_asrc_set_ratio(asrc_reg, asrc_ch, TCC_ASRC_MODE_SYNC, ratio_shift22);
	tcc_asrc_component_reset(asrc->asrc_reg, asrc_comp);

	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_ch, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_ch, 0x1f);

	tcc_asrc_set_inport_timing(asrc_reg, asrc_ch, IP_OP_TIMING_ASRC_REQUEST);
	tcc_asrc_set_outport_timing(asrc_reg, asrc_ch, IP_OP_TIMING_ASRC_REQUEST);

	tcc_asrc_set_volume_gain(asrc_reg, asrc_ch, 0x100000); // 0 dB
	tcc_asrc_set_volume_ramp_gain(asrc_reg, asrc_ch, 0x0); // 0 dB

	tcc_asrc_set_volume_ramp_dn_time(asrc_reg, asrc_ch, 12, 0x100);
	tcc_asrc_set_volume_ramp_up_time(asrc_reg, asrc_ch, 10, 0x80);

	tcc_asrc_volume_ramp_enable(asrc_reg, asrc_ch, 1);
	tcc_asrc_volume_enable(asrc_reg, asrc_ch, 1);

	tcc_asrc_fifo_in_config(asrc_reg, asrc_ch, tx_fmt, tx_mode, 0);
	tcc_asrc_fifo_out_config(asrc_reg, asrc_ch, rx_fmt, rx_mode, 0);

	tcc_asrc_component_enable(asrc->asrc_reg, asrc_comp, 1);
	tcc_asrc_component_enable(asrc->asrc_reg, TCC_INPORT, 1);
	tcc_asrc_component_enable(asrc->asrc_reg, TCC_OUTPORT, 1);

	tcc_asrc_fifo_in_dma_en(asrc->asrc_reg, asrc_ch, 1);
	tcc_asrc_fifo_out_dma_en(asrc->asrc_reg, asrc_ch, 1);
}

int tcc_asrc_m2m_sync_setup(struct tcc_asrc_t *asrc, 
		int asrc_ch, 
		enum tcc_asrc_fifo_fmt_t tx_fmt, 
		enum tcc_asrc_fifo_mode_t tx_mode,
		enum tcc_asrc_fifo_fmt_t rx_fmt, 
		enum tcc_asrc_fifo_mode_t rx_mode,
		uint32_t ratio_shift22)
{
	int ret = 0;
	uint32_t dma_rx_ch;

	dma_rx_ch = asrc_ch + NUM_OF_ASRC_PAIR;
	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, 0); //disable rx dma channel

	__tcc_asrc_m2m_sync_setup(asrc, asrc_ch, tx_fmt, tx_mode, rx_fmt, rx_mode, ratio_shift22);

	return ret;
}

static irqreturn_t tcc_pl080_isr(int irq, void *dev)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)dev;
	uint32_t int_status = readl(asrc->pl080_reg+PL080_INT_STATUS);
	int i;

	dprintk("%s(0x%08x)\n", __func__, int_status);
	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		if (int_status & (1<<i)) {
			writel(1<<i, asrc->pl080_reg+PL080_TC_CLEAR);
			writel(1<<i, asrc->pl080_reg+PL080_ERR_CLEAR);

			if (asrc->pair[i].hw.path == TCC_ASRC_M2M_PATH) {
				tcc_pl080_asrc_m2m_txisr_ch(asrc, i);
			}
		}
	}

	for (i=0; i<NUM_OF_ASRC_PAIR; i++) {
		if (int_status & (1<<(i+NUM_OF_ASRC_PAIR))) {
			//tcc_pl080_asrc_rx_isr_ch(asrc, i);

			writel(1<<(i+NUM_OF_ASRC_PAIR), asrc->pl080_reg+PL080_TC_CLEAR);
			writel(1<<(i+NUM_OF_ASRC_PAIR), asrc->pl080_reg+PL080_ERR_CLEAR);
		}
	}


	return IRQ_HANDLED;
}

static int parse_asrc_dt(struct platform_device *pdev, struct tcc_asrc_t *asrc)
{
	struct device_node *of_node_asrc = pdev->dev.of_node;
	struct device_node *of_node_dma;
	uint32_t max_channel[NUM_OF_ASRC_PAIR];
	uint32_t path_type[NUM_OF_ASRC_PAIR];
	uint32_t sync_mode[NUM_OF_ASRC_PAIR];
	int i;

	printk("%s\n", __func__);

	if ((of_node_dma = of_parse_phandle(pdev->dev.of_node, "dma", 0)) == NULL) {
		printk("asrc)of_node_dma is NULL\n");
		return -EINVAL;
	}

	if ((asrc->asrc_reg = of_iomap(of_node_asrc, 0)) == NULL) {
		printk("asrc)asrc_reg is NULL\n");
		return -EINVAL;
	}
	if ((asrc->pl080_reg = of_iomap(of_node_dma, 0)) == NULL) {
		printk("asrc)pl080_reg is NULL\n");
		return -EINVAL;
	}

	for (i=0; i<NUM_OF_AUX_PERI_CLKS; i++) {
		if (IS_ERR(asrc->aux_pclk[i]=of_clk_get(of_node_asrc, i))) {
			printk("asrc)aux%d_pclk is NULL\n", i);
			return PTR_ERR(asrc->aux_pclk[i]);
		}
	}

    if (IS_ERR(asrc->asrc_hclk=of_clk_get_by_name(of_node_asrc, "asrc_hclk"))) {
		printk("asrc)asrc_hclk is invalid\n");
		return PTR_ERR(asrc->asrc_hclk);
	}

    if (IS_ERR(asrc->pl080_hclk = of_clk_get_by_name(of_node_dma, "pl080_hclk"))) {
		printk("asrc)asrc_hclk is invalid\n");
		return PTR_ERR(asrc->pl080_hclk);
	}

	if ((asrc->asrc_irq = of_irq_get(of_node_asrc, 0)) < 0) {
		printk("asrc)asrc_irq is invalid\n");
		return asrc->asrc_irq;
	}
	if ((asrc->pl080_irq = of_irq_get(of_node_dma, 0)) < 0) {
		printk("asrc)asrc_irq is invalid\n");
		return asrc->asrc_irq;
	}

	of_property_read_u32_array(of_node_asrc, "max-ch-per-pair", max_channel, NUM_OF_ASRC_PAIR);
	of_property_read_u32_array(of_node_asrc, "path-type", path_type, NUM_OF_ASRC_PAIR);

	for(i=0; i<NUM_OF_ASRC_PAIR; i++) {
		asrc->pair[i].hw.path = (path_type[i] == 3) ? TCC_ASRC_P2P_PATH :
							(path_type[i]  == 2) ? TCC_ASRC_P2M_PATH :
							(path_type[i]  == 1) ? TCC_ASRC_M2P_PATH : TCC_ASRC_M2M_PATH;
		asrc->pair[i].hw.max_channel = max_channel[i];
	}


	printk("asrc_reg : %p\n", asrc->asrc_reg);
	printk("asrc_irq: %d\n", asrc->asrc_irq);
	printk("pl080_reg : %p\n", asrc->pl080_reg);
	printk("pl080_irq : %d\n", asrc->pl080_irq);

	for(i=0; i<NUM_OF_ASRC_PAIR; i++) {
		printk("max-ch-per-pair(%d) : %d\n", i, max_channel[i]);
		printk("path_type(%d) : %d\n", i, path_type[i]);
	}

	return 0;
}

static int tcc_asrc_init(struct tcc_asrc_t *asrc)
{
	clk_prepare_enable(asrc->pl080_hclk);
	clk_prepare_enable(asrc->asrc_hclk);

	tcc_asrc_reset(asrc->asrc_reg);
	tcc_asrc_dma_arbitration(asrc->asrc_reg, 0);

	tcc_pl080_enable(asrc->pl080_reg, 1);

	tcc_pl080_clear_int(asrc->pl080_reg, 0xff);
	tcc_pl080_clear_err(asrc->pl080_reg, 0xff);

	return 0;
}

static int tcc_asrc_deinit(struct tcc_asrc_t *asrc)
{
	clk_disable_unprepare(asrc->pl080_hclk);
	clk_disable_unprepare(asrc->asrc_hclk);

	return 0;
}

#ifdef CONFIG_ARCH_TCC802X
void check_tcc802x_rev_xx(struct tcc_asrc_t *asrc)
{
#define CHIP_ID_ADDR0 0xE0003C10
#define CHIP_ID_ADDR1 0xF400001C

#define CHIP_ID_REV_XX_ADDR	(0x16042200)

	volatile uint32_t *p0 = ioremap_nocache(CHIP_ID_ADDR0, 4);
	volatile uint32_t *p1 = ioremap_nocache(CHIP_ID_ADDR1, 4);
	uint32_t val;

	val = (*p0 >> 8) & 0x0f;

	if ((val == 0)  && (*p1 == CHIP_ID_REV_XX_ADDR)) {
		asrc->chip_rev_xx = true;
	} else {
		asrc->chip_rev_xx = false;
	}

	iounmap(p0);
	iounmap(p1);
}
#endif

static int tcc_asrc_probe(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = kzalloc(sizeof(struct tcc_asrc_t), GFP_KERNEL);
	int ret = 0;

	if (asrc == NULL) {
		printk(KERN_WARNING "%s - kzalloc failed.\n", __func__);
		return -ENOMEM;
	}

	if ((ret = parse_asrc_dt(pdev, asrc)) < 0) {
		printk("%s : Fail to parse asrc dt\n", __func__);
		return ret;
	}

	platform_set_drvdata(pdev, asrc);
	asrc->pdev = pdev;

#ifdef CONFIG_ARCH_TCC802X
	check_tcc802x_rev_xx(asrc);
#endif

	tcc_asrc_init(asrc);

	tcc_asrc_m2m_drvinit(pdev);

	ret = request_irq(asrc->pl080_irq, tcc_pl080_isr, IRQF_DISABLED, "tcc-asrc-pl080", (void*)asrc);
	if (ret < 0) {
		printk("pl080 request_irq(%d) failed\n", asrc->pl080_irq);
		return ret;
	}


	return ret;
}

static int tcc_asrc_remove(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	free_irq(asrc->pl080_irq, (void*)asrc);
		
	tcc_asrc_deinit(asrc);

	return 0;
}

static int tcc_asrc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int tcc_asrc_resume(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id tcc_asrc_of_match[] = {
	{ .compatible = "telechips,asrc" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_asrc_of_match);

static struct platform_driver tcc_asrc_driver = {
	.probe		= tcc_asrc_probe,
	.remove		= tcc_asrc_remove,
	.suspend	= tcc_asrc_suspend,
	.resume		= tcc_asrc_resume,
	.driver 	= {
		.name	= "tcc_asrc_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_asrc_of_match),
#endif
	},
};

module_platform_driver(tcc_asrc_driver);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips ASRC Driver");
MODULE_LICENSE("GPL");

