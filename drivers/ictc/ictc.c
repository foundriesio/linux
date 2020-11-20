// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
#define debug_ictc(msg...) pr_warn("[DEBUG][ICTC]" msg)
#define err_ictc(msg...) pr_err("[ERROR][ICTC]" msg)
#else
#define debug_ictc(msg...)
#define err_ictc(msg...)
#endif

#define SUCCESS			0

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

#define ICTC_EN			((unsigned int)1<<31)
#define TCLK_EN			((unsigned int)1<<21)
#define TSCNT_EN		((unsigned int)1<<20)
#define FLTCNT_EN		((unsigned int)1<<19)
#define TOCNT_EN		((unsigned int)1<<18)
#define ECNT_EN			((unsigned int)1<<17)
#define PDCNT_EN		((unsigned int)1<<16)
#define TSCNT_CLR		((unsigned int)1<<4)
#define FLTCNT_CLR		((unsigned int)1<<3)
#define TOCNT_CLR		((unsigned int)1<<2)
#define ECNT_CLR		((unsigned int)1<<1)
#define PDCNT_CLR		((unsigned int)1<<0)

#define REDGEINT		((unsigned int)1<<31)
#define FEDGEINT		((unsigned int)1<<30)
#define DFFULLINT		((unsigned int)1<<29)
#define FCHGINT			((unsigned int)1<<28)
#define DCHGINT			((unsigned int)1<<27)
#define EFULLINT		((unsigned int)1<<26)
#define TOFULLINT		((unsigned int)1<<25)
#define NFEDFULLINT		((unsigned int)1<<24)

#define LOCK_EN			((unsigned int)1<<31)
#define ABS_SEL			((unsigned int)1<<29)
#define EDGE_SEL		((unsigned int)1<<28)
#define TCLK_POL		((unsigned int)1<<26)
#define TCK_SEL(x)		((unsigned int)(x)<<22)
#define FLT_F_MODE(x)		((unsigned int)(x)<<20)
#define FLT_R_MODE(x)		((unsigned int)(x)<<18)
#define CMP_ERR_SEL		((unsigned int)1<<16)
#define CMP_ERR_BOTH		((unsigned int)1<<17)
#define DMA_EN			((unsigned int)1<<15)
#define DMA_SEL(x)		((unsigned int)(x)<<12)
#define F_IN_SEL(x)		((unsigned int)(x)<<0)

LIST_HEAD(ictc_list);

struct ictc_dev {
	struct device_node *np;
	unsigned int irq;
	unsigned int int_state;
	struct clk *pPClk;
	struct device *dev;
	struct list_head list;
};

//TCC803x GPIOs
static int gpio_f_in[] = {
	0x1,			//GPIO_A
	0x21,			//GPIO_B
	0x3e,			//GPIO_C
	0,			//GPIO_D
	0x5c,			//GPIO_E
	0,			//GPIO_F
	0x70,			//GPIO_G
	0x7b,			//GPIO_H
	0,			//GPIO_K
	0x87,			//GPIO_MA
	0xa6,			//GPIO_SD0
	0xb4,			//GPIO_SD1
	0xc0,			//GPIO_SD2
};

static int gpio_num_group[] = {
	0,			//GPIO_A
	32,			//GPIO_B
	61,			//GPIO_C
	91,			//GPIO_D
	113,			//GPIO_E
	133,			//GPIO_F
	165,			//GPIO_G
	176,			//GPIO_H
	188,			//GPIO_K
	207,			//GPIO_MA
	237,			//GPIO_SD0
	252,			//GPIO_SD1
	263			//GPIO_SD2
};

#define RTC_WKUP	0xa5

static struct tasklet_struct ictc_tasklet;
static void __iomem *ictc_base;
static int f_in_gpio;
static int f_in_source;
static unsigned int f_in_rtc_wkup;

static unsigned int irq_setting;

static char ictc_prop_v_l[][50] = {
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
	unsigned int r_edge;
	unsigned int f_edge;
	unsigned int edge_matching_val;
	unsigned int duty_rounding_val;
	unsigned int prd_rounding_val;
	unsigned int flt_f_mode;
	unsigned int flt_r_mode;
	unsigned int time_out;
	unsigned int abs_sel;
	unsigned int edge_sel;
	unsigned int tck_pol;
	unsigned int tck_sel;
	unsigned int lock_en;
};
static struct ictc_property_value ictc_prop_v;

static char ictc_prop_b_l[][50] = {
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
	unsigned int f_edge_int;
	unsigned int r_edge_int;
	unsigned int df_cnt_full_int;
	unsigned int f_chg_int;
	unsigned int d_chg_int;
	unsigned int e_cnt_full_int;
	unsigned int to_cnt_full_int;
	unsigned int nf_ed_cnt_full_int;
	unsigned int time_stamp_cnt;
};

static struct ictc_property_bool ictc_prop_b;

static void ictc_clear_interrupt(void);
static void ictc_clear_counter(void);
static void ictc_enable_interrupt(void);

static void ictc_tasklet_handler(unsigned long data)
{

	struct ictc_dev *idev = (struct ictc_dev *)data;
	struct ictc_dev *idev_pos, *idev_pos_safe;
	unsigned int int_state;

	list_for_each_entry_safe(idev_pos, idev_pos_safe, &ictc_list, list) {
		int_state = idev_pos->int_state;
		list_del(&idev_pos->list);

		if ((int_state & (unsigned int)IRQ_STAT_REDGE) ==
		    (unsigned int)IRQ_STAT_REDGE)
			debug_ictc("rising edge interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_FEDGE) ==
		    (unsigned int)IRQ_STAT_FEDGE)
			debug_ictc("falling edge interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_DFFULL) ==
		    (unsigned int)IRQ_STAT_DFFULL)
			debug_ictc
			    ("duty and frequency counter full interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_FCHG) ==
		    (unsigned int)IRQ_STAT_FCHG)
			debug_ictc("frequency changing interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_DCHG) ==
		    (unsigned int)IRQ_STAT_DCHG)
			debug_ictc("duty changing interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_EFULL) ==
		    (unsigned int)IRQ_STAT_EFULL)
			debug_ictc("edge counter full interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_TOFULL) ==
		    (unsigned int)IRQ_STAT_TOFULL)
			debug_ictc("timeout counter full interrupt\n");
		if ((int_state & (unsigned int)IRQ_STAT_NFEDFULL) ==
		    (unsigned int)IRQ_STAT_NFEDFULL)
			debug_ictc
			    ("noise-filter and edge counter full interrupt\n");

		devm_kfree(idev->dev, idev_pos);
	}

	/* to do */

	/* ///// */

}

static irqreturn_t ictc_interrupt_handler(int irq, void *dev)
{
	struct ictc_dev *idev = dev_get_drvdata(dev);
	struct ictc_dev *idev_new;

	idev_new = devm_kzalloc(idev->dev, sizeof(struct ictc_dev), GFP_KERNEL);
	idev_new->int_state =
	    (unsigned int)IRQ_STAT_MASK & ictc_readl(IRQ_CTRL);
	list_add_tail(&idev_new->list, &ictc_list);

	tasklet_schedule(&ictc_tasklet);

	/* to do */

	/* ///// */

	ictc_clear_interrupt();
	ictc_clear_counter();
	ictc_enable_interrupt();

	return IRQ_HANDLED;

}

static int ictc_parse_dt(struct device_node *np)
{

	int ret = 0;
	unsigned int node_num = 0;
	unsigned long count = 0;

	count = ARRAY_SIZE(ictc_prop_v_l);

	for (node_num = 0; node_num < (unsigned int)count; node_num++) {
		if (of_property_read_u32
		    (np, ictc_prop_v_l[node_num],
		     (u32 *)&ictc_prop_v + node_num) != 0) {
			err_ictc("no property found in DT : %s\n",
				 ictc_prop_v_l[node_num]);
			ret = -EINVAL;
		}
		debug_ictc("%s -> %x\n", ictc_prop_v_l[node_num],
			   *((u32 *)&ictc_prop_v + node_num));

		if (ret != 0)
			break;

	}

	count = ARRAY_SIZE(ictc_prop_b_l);

	for (node_num = 0; node_num < count; node_num++) {

		*((u32 *)&ictc_prop_b + node_num) =
		    (unsigned int)of_property_read_bool(np,
							ictc_prop_b_l
							[node_num]);

		debug_ictc("%s -> %x\n", ictc_prop_b_l[node_num],
			   *((u32 *)&ictc_prop_b + node_num));
	}

	return ret;

}

static void ictc_configure(void)
{
	unsigned int config_val = 0;

	if (ictc_prop_v.abs_sel != 0u)
		config_val |= ABS_SEL;
	else
		config_val &= ~ABS_SEL;

	if (ictc_prop_v.edge_sel != 0u)
		config_val |= EDGE_SEL;

	if (ictc_prop_v.tck_pol != 0u)
		config_val |= TCLK_POL;

	config_val |= TCK_SEL((ictc_prop_v.tck_sel));

	config_val |= FLT_F_MODE((ictc_prop_v.flt_f_mode));
	config_val |= FLT_R_MODE((ictc_prop_v.flt_r_mode));

	if ((ictc_prop_b.d_chg_int != 0u) && (ictc_prop_b.f_chg_int != 0u)) {
		config_val |= CMP_ERR_BOTH;
	} else {
		if (ictc_prop_b.d_chg_int != 0u)
			config_val |= CMP_ERR_SEL;
		else if (ictc_prop_b.f_chg_int != 0u)
			config_val &= ~CMP_ERR_SEL;
		else
			debug_ictc("no compare error selection\n");
	}

	if (f_in_rtc_wkup == 0u)
		config_val |= F_IN_SEL((f_in_source));
	else
		config_val |= F_IN_SEL((RTC_WKUP));

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
	if (irq_setting == 0u) {

		unsigned int enable_int = 0;

		if (ictc_prop_b.r_edge_int != 0u)
			enable_int |= REDGEINT;
		if (ictc_prop_b.f_edge_int != 0u)
			enable_int |= FEDGEINT;
		if (ictc_prop_b.df_cnt_full_int != 0u)
			enable_int |= DFFULLINT;
		if (ictc_prop_b.f_chg_int != 0u)
			enable_int |= FCHGINT;
		if (ictc_prop_b.d_chg_int != 0u)
			enable_int |= DCHGINT;
		if (ictc_prop_b.e_cnt_full_int != 0u)
			enable_int |= EFULLINT;
		if (ictc_prop_b.to_cnt_full_int != 0u)
			enable_int |= TOFULLINT;
		if (ictc_prop_b.nf_ed_cnt_full_int != 0u)
			enable_int |= NFEDFULLINT;

		irq_setting = enable_int;

		ictc_writel(enable_int, IRQ_CTRL);
	} else {
		ictc_writel(irq_setting, IRQ_CTRL);
	}

}

static void ictc_clear_counter(void)
{
	unsigned int val = 0;

	val = ictc_readl(OP_EN_CTRL) | (unsigned int)0x1f;
	ictc_writel(val, OP_EN_CTRL);
	val = ictc_readl(OP_EN_CTRL) & (~(unsigned int)0x1f);
	ictc_writel(val, OP_EN_CTRL);
}

static void ictc_enable_counter(void)
{

	unsigned int enable_val = 0;

	if (ictc_prop_b.time_stamp_cnt != 0u)
		enable_val |= TSCNT_EN;
	if (ictc_prop_b.to_cnt_full_int != 0u)
		enable_val |= TOCNT_EN;
	if ((ictc_prop_b.d_chg_int != 0u) || (ictc_prop_b.f_chg_int != 0u)
	    || (ictc_prop_b.df_cnt_full_int != 0u))
		enable_val |= PDCNT_EN;
	if ((ictc_prop_b.r_edge_int != 0u) || (ictc_prop_b.f_edge_int != 0u)
	    || (ictc_prop_b.e_cnt_full_int != 0u)
	    || (ictc_prop_b.nf_ed_cnt_full_int != 0u))
		enable_val |= ECNT_EN;

	enable_val |= TCLK_EN | FLTCNT_EN;

	ictc_writel(enable_val, OP_EN_CTRL);

}

static void ictc_enable(void)
{

	ictc_writel(ictc_readl(OP_EN_CTRL) | ICTC_EN, OP_EN_CTRL);

}

static int gpio_to_f_in(int gpio_num)
{

	unsigned long count = 0;
	unsigned int gpio_group = 0, gpio_group_val = 0;

	count = ARRAY_SIZE(gpio_num_group);

	debug_ictc("gpio group count : %ld\n", count);

	for (gpio_group = 1; gpio_group < (unsigned int)count; gpio_group++) {

		if ((gpio_num - gpio_num_group[gpio_group]) < 0) {
			gpio_group_val = gpio_group - 1u;
			break;
		}

	}

	debug_ictc("gpio_f_in : 0x%x gpio_num_group : %d, gpio_num : %d",
		   gpio_f_in[gpio_group_val], gpio_num_group[gpio_group_val],
		   gpio_num);

	return gpio_f_in[gpio_group_val] + (gpio_num -
					    gpio_num_group[gpio_group_val]);

}

static int ictc_probe(struct platform_device *pdev)
{
	struct ictc_dev *idev;
	struct device_node *np = pdev->dev.of_node;
	unsigned int irq = (unsigned int)platform_get_irq(pdev, 0);
	struct clk *pPClk = of_clk_get(pdev->dev.of_node, 0);
	int ret = 0;

	//initialize global variables
	f_in_gpio = 0;
	f_in_source = 0;
	f_in_rtc_wkup = 0;
	irq_setting = 0;

	debug_ictc("ICTC probe ictc irq : %d\n", irq);

	if (np == NULL) {
		err_ictc("device does not exist!\n");
		ret = -EINVAL;
	} else {

		idev =
		    devm_kzalloc(&pdev->dev, sizeof(struct ictc_dev),
				 GFP_KERNEL);

		if (idev == NULL) {
			ret = -ENXIO;
		} else {

			INIT_LIST_HEAD(&idev->list);

			idev->np = np;
			idev->irq = irq;
			idev->pPClk = pPClk;
			idev->dev = &pdev->dev;
			platform_set_drvdata(pdev, idev);

			ret = clk_prepare_enable(pPClk);

			if (ret == SUCCESS) {
				ret =
				    request_irq(irq, ictc_interrupt_handler,
						IRQF_SHARED, "ICTC",
						&pdev->dev);
				tasklet_init(&ictc_tasklet,
					     ictc_tasklet_handler,
					     (unsigned long)idev);

				if (ret != SUCCESS) {
					err_ictc("irq req. fail: %d irq: %x\n",
						 ret, irq);
				} else {

					ictc_base = of_iomap(np, 0);

					f_in_rtc_wkup = (unsigned int)
					    of_property_read_bool(np,
								  "f-in-rtc-wkup");

					if (f_in_rtc_wkup == 0u) {

						f_in_gpio =
						    of_get_named_gpio(np,
								      "f-in-gpio",
								      0);
						f_in_source =
						    gpio_to_f_in(f_in_gpio);

					}

					ret = ictc_parse_dt(np);

					if (ret != SUCCESS) {
						err_ictc("ictc:No dev node\n");
					} else {

						ictc_configure();

						ictc_enable_interrupt();

						ictc_enable_counter();

						ictc_enable();

					}
				}

				if (ret != SUCCESS) {
					clk_disable_unprepare(pPClk);
				}

			}

			if (ret != SUCCESS) {

				devm_kfree(&pdev->dev, idev);
			}

		}

	}

	return ret;

}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_ARCH_TCC805X)

static int ictc_suspend(struct device *dev)
{
	struct ictc_dev *idev = dev_get_drvdata(dev);
	int ret = 0;

	if (idev == NULL) {
		ret = -EINVAL;
	} else {
		free_irq(idev->irq, idev);
		clk_disable_unprepare(idev->pPClk);
	}

	return ret;

}

static int ictc_resume(struct device *dev)
{
	struct ictc_dev *idev = dev_get_drvdata(dev);
	int ret = 0;

	if (idev == NULL) {
		ret = -EINVAL;
	} else {
		ret = clk_prepare_enable(idev->pPClk);

		if (ret == SUCCESS) {

			ret =
			    request_irq(idev->irq, ictc_interrupt_handler,
					IRQF_SHARED, "ICTC", idev);

			if (ret != SUCCESS) {

				clk_disable_unprepare(idev->pPClk);
				err_ictc
				    ("Interrupt request fail: %d irq: %x\n",
				     ret, idev->irq);

			} else {

				ictc_configure();

				ictc_enable_interrupt();

				ictc_enable_counter();

				ictc_enable();

			}

		}

		if (ret != SUCCESS) {
			devm_kfree(idev->dev, idev);
		}
	}

	return ret;

}

static SIMPLE_DEV_PM_OPS(ictc_pm_ops, ictc_suspend, ictc_resume);

#endif

static const struct of_device_id ictc_match_table[] = {
	{.compatible = "telechips,ictc"},
	{}
};

MODULE_DEVICE_TABLE(of, ictc_match_table);

static struct platform_driver ictc_driver = {
	.probe = ictc_probe,
	.driver = {
		   .name = "tcc-ictc",
#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_ARCH_TCC805X)
		   .pm = &ictc_pm_ops,
#endif
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
