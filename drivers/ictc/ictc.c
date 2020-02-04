/****************************************************************************
 * Copyright (C) 2020 Telechips Inc.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>


#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/clk.h>

//#define DEBUG_ICTC
#ifdef DEBUG_ICTC
#define debug_ictc(msg...) printk(KERN_DEBUG "[DEBUG][ICTC]" msg);
#define err_ictc(msg...) printk(KERN_ERR "[ERROR][ICTC]" msg);
#else
#define debug_ictc(msg...)
#define err_ictc(msg...)
#endif


#define ictc_readl		__raw_readl
#define ictc_writel		__raw_writel

#define IRQ_CLR_MASK		0x0000ff00
#define IRQ_STAT_MASK		0xff
#define IRQ_STAT_REDGE		0x80
#define IRQ_STAT_FEDGE		0x40
#define IRQ_STAT_DFFULL		0x20
#define IRQ_STAT_FCHG		0x10
#define IRQ_STAT_DCHG		0x8
#define IRQ_STAT_EFULL		0x4
#define IRQ_STAT_TOFULL		0x2
#define IRQ_STAT_NFEDFULL	0x1


#define OP_EN_CTRL		(ictc_base+0x0)
#define OP_MODE_CTRL		(ictc_base+0x4)
#define IRQ_CTRL		(ictc_base+0x8)
#define TIME_OUT		(ictc_base+0xc)
#define R_EDGE			(ictc_base+0x10)
#define F_EDGE			(ictc_base+0x14)
#define PRD_CMP_RND		(ictc_base+0x18)
#define DUTY_CMP_RND		(ictc_base+0x1c)
#define EDGE_CNT_MAT		(ictc_base+0x20)
#define CNT_PRE_PRD		(ictc_base+0x30)
#define OLD_CNT_PRE_PRD		(ictc_base+0x34)
#define CNT_PRE_DUTY		(ictc_base+0x38)
#define OLD_CNT_PRE_DUTY	(ictc_base+0x3c)
#define CUR_EDGE_CNT		(ictc_base+0x40)
#define PRV_EDGE_CNT		(ictc_base+0x44)
#define R_EDGE_TSTMP_CNT	(ictc_base+0x48)
#define F_EDGE_TSTMP_CNT	(ictc_base+0x4c)

#define ICTC_EN			(1<<31)
#define TCLK_EN			(1<<21)
#define TSCNT_EN		(1<<20)
#define FLTCNT_EN		(1<<19)
#define TOCNT_EN		(1<<18)
#define ECNT_EN			(1<<17)
#define PDCNT_EN		(1<<16)
#define TSCNT_CLR		(1<<4)
#define FLTCNT_CLR		(1<<3)
#define TOCNT_CLR		(1<<2)
#define ECNT_CLR		(1<<1)
#define PDCNT_CLR		(1<<0)

#define REDGEINT		(1<<31)
#define FEDGEINT		(1<<30)
#define DFFULLINT		(1<<29)
#define FCHGINT			(1<<28)
#define DCHGINT			(1<<27)
#define EFULLINT		(1<<26)
#define TOFULLINT		(1<<25)
#define NFEDFULLINT		(1<<24)


#define LOCK_EN			(1<<31)
#define ABS_SEL			(1<<29)
#define EDGE_SEL		(1<<28)
#define TCLK_POL		(1<<26)
#define TCK_SEL(x)		(x<<22)
#define FLT_F_MODE(x)		(x<<20)
#define FLT_R_MODE(x)		(x<<18)
#define CMP_ERR_SEL		(1<<16)
#define CMP_ERR_BOTH		(1<<17)
#define DMA_EN			(1<<15)
#define DMA_SEL(x)		(x<<12)
#define F_IN_SEL(x)		(x<<0)

//TCC803x GPIOs
static int gpio_f_in[] = {
	0x1,     //GPIO_A
	0x21,     //GPIO_B
	0x3e,     //GPIO_C
	0,     //GPIO_D
	0x5c,     //GPIO_E
	0,     //GPIO_F
	0x70,     //GPIO_G
	0x7b,     //GPIO_H
	0,     //GPIO_K
	0x87,     //GPIO_MA
	0xa6,     //GPIO_SD0
	0xb4,     //GPIO_SD1
	0xc0,     //GPIO_SD2
};

static int gpio_num_group[] = {
	0,	//GPIO_A
	32,	//GPIO_B
	61,     //GPIO_C
	91,     //GPIO_D
	113,     //GPIO_E
	133,     //GPIO_F
	165,     //GPIO_G
	176,     //GPIO_H
	188,     //GPIO_K
	207,     //GPIO_MA
	237,     //GPIO_SD0
	252,     //GPIO_SD1
	263     //GPIO_SD2
};

#define RTC_WKUP	0xa5

static void __iomem *ictc_base;
static int f_in_gpio = (int)NULL;
static int f_in_source = (int)NULL;
static int f_in_rtc_wkup = (int)NULL;

static int irq_setting = (int)NULL;

static char ictc_prop_v_l[][50] =
{
	"r-edge",
	"f-edge",
	"edge-matching-value",
	"duty-rounding-value",
	"prd-rounding-value",
	"flt-f-mode",
	"flt-r-mode",
	"time-out",
	"abs-sel",
	"edge-sel",
	"tck-pol",
	"tck-sel",
	"lock-en",
};

struct ictc_property_value {
	int r_edge;
	int f_edge;
	int edge_matching_val;
	int duty_rounding_val;
	int prd_rounding_val;
	int flt_f_mode;
	int flt_r_mode;
	int time_out;
	int abs_sel;
	int edge_sel;
	int tck_pol;
	int tck_sel;
	int lock_en;
};
static struct ictc_property_value ictc_prop_v;

static char ictc_prop_b_l[][50] =
{
	"f-edge-int",
	"r-edge-int",
	"df-cnt-full-int",
	"f-chg-int",
	"d-chg-int",
	"e-cnt-full-int",
	"to-cnt-full-int",
	"nf-ed-cnt-full-int",
	"time-stamp-cnt",
};

struct ictc_property_bool {
	int f_edge_int;
	int r_edge_int;
	int df_cnt_full_int;
	int f_chg_int;
	int d_chg_int;
	int e_cnt_full_int;
	int to_cnt_full_int;
	int nf_ed_cnt_full_int;
	int time_stamp_cnt;
};
static struct ictc_property_bool ictc_prop_b;

static void ictc_clear_interrupt(void);
static void ictc_clear_counter(void);
static void ictc_enable_interrupt(void);

static irqreturn_t ictc_interrupt_handler(int irq, void *dev_id)
{
	unsigned int int_state = 0;

	int_state = IRQ_STAT_MASK&ictc_readl(IRQ_CTRL);

	if(int_state&IRQ_STAT_REDGE)
		debug_ictc("rising edge interrupt\n");
	if(int_state&IRQ_STAT_FEDGE)
		debug_ictc("falling edge interrupt\n");
	if(int_state&IRQ_STAT_DFFULL)
		debug_ictc("duty and frequency counter full interrupt\n");
	if(int_state&IRQ_STAT_FCHG)
		debug_ictc("frequency changing interrupt\n");
	if(int_state&IRQ_STAT_DCHG)
		debug_ictc("duty changing interrupt\n");
	if(int_state&IRQ_STAT_EFULL)
		debug_ictc("edge counter full interrupt\n");
	if(int_state&IRQ_STAT_TOFULL)
		debug_ictc("timeout counter full interrupt\n");
	if(int_state&IRQ_STAT_NFEDFULL)
		debug_ictc("noise-filter and edge counter full interrupt\n");

	/* to do */

	//////////

	ictc_clear_interrupt();
	ictc_clear_counter();
	ictc_enable_interrupt();

	return IRQ_NONE;

}

static int ictc_parse_dt(struct platform_device *pdev)
{

	int ret = 0, count = 0, node_num=0;
	struct device_node *np = pdev->dev.of_node;

	count = sizeof(ictc_prop_v_l)/(sizeof(ictc_prop_v_l[0]));

	for(node_num = 0; node_num < count; node_num++)
	{
		if(of_property_read_u32(np, ictc_prop_v_l[node_num],(u32 *)&ictc_prop_v+node_num))
		{
			err_ictc("no property found in DT : %s\n", ictc_prop_v_l[node_num]);
			ret = -EINVAL;
		}
		debug_ictc("%s -> %x\n", ictc_prop_v_l[node_num], *((u32 *)&ictc_prop_v+node_num));

		if(ret!=0)
			break;

	}

	count = sizeof(ictc_prop_b_l)/(sizeof(ictc_prop_b_l[0]));

	for(node_num = 0; node_num < count; node_num++)
	{

		*((u32 *)&ictc_prop_b+node_num) = of_property_read_bool(np, ictc_prop_b_l[node_num]);

		debug_ictc("%s -> %x\n", ictc_prop_b_l[node_num], *((u32 *)&ictc_prop_b+node_num));
	}


	return ret;

}

static void ictc_configure(void)
{
	unsigned int config_val = 0;

	if(ictc_prop_v.abs_sel)
		config_val |= ABS_SEL;
	else
		config_val &= ~ABS_SEL;

	if(ictc_prop_v.edge_sel)
		config_val |= EDGE_SEL;

	if(ictc_prop_v.tck_pol)
		config_val |= TCLK_POL;

	config_val |= TCK_SEL(ictc_prop_v.tck_sel);

	config_val |= FLT_F_MODE(ictc_prop_v.flt_f_mode);
	config_val |= FLT_R_MODE(ictc_prop_v.flt_r_mode);

	if(ictc_prop_b.d_chg_int&&ictc_prop_b.f_chg_int)
	{
		config_val |= CMP_ERR_BOTH;
	}
	else
	{
		if(ictc_prop_b.d_chg_int)
			config_val |= CMP_ERR_SEL;
		else if(ictc_prop_b.f_chg_int)
			config_val &= ~CMP_ERR_SEL;
		else
			debug_ictc("no compare error selection\n");
	}


	if(!f_in_rtc_wkup)
		config_val |= F_IN_SEL(f_in_source);
	else
		config_val |= F_IN_SEL(RTC_WKUP);

	ictc_writel(config_val, OP_MODE_CTRL);

	ictc_writel(ictc_prop_v.time_out, TIME_OUT);

	ictc_writel(ictc_prop_v.r_edge, R_EDGE);
	ictc_writel(ictc_prop_v.f_edge, F_EDGE);

	ictc_writel(ictc_prop_v.edge_matching_val, EDGE_CNT_MAT);

	ictc_writel(ictc_prop_v.duty_rounding_val, DUTY_CMP_RND);
	ictc_writel(ictc_prop_v.prd_rounding_val, PRD_CMP_RND);

}

static void ictc_clear_interrupt(void)
{
	ictc_writel(IRQ_CLR_MASK, IRQ_CTRL);
	ictc_writel(0, IRQ_CTRL);
}

static void ictc_enable_interrupt(void)
{
	if(!irq_setting){

		unsigned int enable_int = 0;
		if(ictc_prop_b.r_edge_int)
			enable_int |= REDGEINT;
		if(ictc_prop_b.f_edge_int)
			enable_int |= FEDGEINT;
		if(ictc_prop_b.df_cnt_full_int)
			enable_int |= DFFULLINT;
		if(ictc_prop_b.f_chg_int)
			enable_int |= FCHGINT;
		if(ictc_prop_b.d_chg_int)
			enable_int |= DCHGINT;
		if(ictc_prop_b.e_cnt_full_int)
			enable_int |= EFULLINT;
		if(ictc_prop_b.to_cnt_full_int)
			enable_int |= TOFULLINT;
		if(ictc_prop_b.nf_ed_cnt_full_int)
			enable_int |= NFEDFULLINT;

		irq_setting = enable_int;

		ictc_writel(enable_int, IRQ_CTRL);
	} else {
		ictc_writel(irq_setting, IRQ_CTRL);
	}


}

static void ictc_clear_counter(void)
{

	ictc_writel(ictc_readl(OP_EN_CTRL)|0x1f, OP_EN_CTRL);
	ictc_writel(ictc_readl(OP_EN_CTRL)&(~0x1f), OP_EN_CTRL);
}

static void ictc_enable_counter(void)
{

	unsigned int enable_val = 0;

	if(ictc_prop_b.time_stamp_cnt)
		enable_val |= TSCNT_EN;
	if(ictc_prop_b.to_cnt_full_int)
		enable_val |= TOCNT_EN;
	if(ictc_prop_b.d_chg_int||ictc_prop_b.f_chg_int||ictc_prop_b.df_cnt_full_int)
		enable_val |= PDCNT_EN;
	if(ictc_prop_b.r_edge_int||ictc_prop_b.f_edge_int||ictc_prop_b.e_cnt_full_int||ictc_prop_b.nf_ed_cnt_full_int)
		enable_val |= ECNT_EN;

	enable_val |= TCLK_EN|FLTCNT_EN;

	ictc_writel(enable_val, OP_EN_CTRL);

}

static void ictc_enable(void)
{

	ictc_writel(ictc_readl(OP_EN_CTRL)|ICTC_EN, OP_EN_CTRL);

}

static int gpio_to_f_in(int gpio_num)
{

	int count = 0, gpio_group = 0;

	count = sizeof(gpio_num_group)/sizeof(gpio_num_group[0]);

	debug_ictc("gpio group count : %d\n", count);

	for(gpio_group = 0 ; gpio_group < count ; gpio_group++)
	{

		if((gpio_num - gpio_num_group[gpio_group]) < 0){
			gpio_group -= 1;
			break;
		}

	}

	debug_ictc("gpio_f_in : 0x%x gpio_num_group : %d, gpio_num : %d", gpio_f_in[gpio_group], gpio_num_group[gpio_group], gpio_num);

	return gpio_f_in[gpio_group] + (gpio_num - gpio_num_group[gpio_group]);


}

static int ictc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned int irq = platform_get_irq(pdev, 0);
	struct clk *pPClk = of_clk_get(pdev->dev.of_node, 0);
	int ret = 0;

	if(!np){
		err_ictc("device does not exist!\n");
		return -EINVAL;
	}

	clk_prepare_enable(pPClk);

	ret = request_irq(irq, ictc_interrupt_handler, IRQF_SHARED, "ICTC", pdev);
	if(ret){
		err_ictc("Interrupt request fail ret : %d irq : %x\n", ret, irq);
		return ret;
	}

	ictc_base = of_iomap(np, 0);

	f_in_rtc_wkup = of_property_read_bool(np, "f-in-rtc-wkup");
	if(!f_in_rtc_wkup)
	{

		f_in_gpio = of_get_named_gpio(np, "f-in-gpio", 0);
		f_in_source = gpio_to_f_in(f_in_gpio);

	}

	ret = ictc_parse_dt(pdev);
	if(ret){
		err_ictc("device node does not exist!\n");
		return ret;
	}

	ictc_configure();

	ictc_enable_interrupt();

	ictc_enable_counter();

	ictc_enable();


	return ret;

}


static const struct of_device_id ictc_match_table[] = {
	{ .compatible = "telechips,ictc" },
	{ }
};
MODULE_DEVICE_TABLE(of, ictc_match_table);

static struct platform_driver ictc_driver = {
	.probe		= ictc_probe,
	.driver		= {
		.name	= "tcc-ictc",
		.of_match_table = ictc_match_table,
	},
};


static int __init ictc_init(void)
{

	return platform_driver_register(&ictc_driver);

}

static void __exit ictc_exit(void)
{
	platform_driver_unregister(&ictc_driver);
}

arch_initcall(ictc_init);
module_exit(ictc_exit);

MODULE_DESCRIPTION("Telechips ICTC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ictc");

