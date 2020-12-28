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

#ifdef DEBUG_ICTC		/* For enabling debug, define DEBUG_ICTC */
#define debug_ictc(msg...) pr_warn("[DEBUG][ICTC]" msg)
#define err_ictc(msg...) pr_err("[ERROR][ICTC]" msg)
#else
#define debug_ictc(msg...)
#define err_ictc(msg...)
#endif

#define SUCCESS                 0

#define ictc_readl              __raw_readl
#define ictc_writel             __raw_writel

#define IRQ_CLR_MASK            0x0000ff00
#define IRQ_STAT_MASK           0xff
#define IRQ_STAT_REDGE          0x80
#define IRQ_STAT_FEDGE          0x40
#define IRQ_STAT_DFFULL         0x20
#define IRQ_STAT_FCHG           0x10
#define IRQ_STAT_DCHG           0x8
#define IRQ_STAT_EFULL          0x4
#define IRQ_STAT_TOFULL         0x2
#define IRQ_STAT_NFEDFULL       0x1

#define OP_EN_CTRL              (ictc_base+0x0)
#define OP_MODE_CTRL            (ictc_base+0x4)
#define IRQ_CTRL                (ictc_base+0x8)
#define TIME_OUT                (ictc_base+0xc)
#define R_EDGE                  (ictc_base+0x10)
#define F_EDGE                  (ictc_base+0x14)
#define PRD_CMP_RND             (ictc_base+0x18)
#define DUTY_CMP_RND            (ictc_base+0x1c)
#define CNT_EDGE_MAT            (ictc_base+0x20)
#define CNT_PRE_PRD             (ictc_base+0x30)
#define OLD_CNT_PRE_PRD         (ictc_base+0x34)
#define CNT_PRE_DUTY            (ictc_base+0x38)
#define OLD_CNT_PRE_DUTY        (ictc_base+0x3c)
#define CUR_EDGE_CNT            (ictc_base+0x40)
#define PRV_EDGE_CNT            (ictc_base+0x44)
#define R_EDGE_TSTMP_CNT        (ictc_base+0x48)
#define F_EDGE_TSTMP_CNT        (ictc_base+0x4c)

#define ICTC_EN                 ((uint32_t)1<<31)
#define TCLK_EN                 ((uint32_t)1<<21)
#define TSCNT_EN                ((uint32_t)1<<20)
#define FLTCNT_EN               ((uint32_t)1<<19)
#define TOCNT_EN                ((uint32_t)1<<18)
#define E_CNT_EN                ((uint32_t)1<<17)
#define PDCNT_EN                ((uint32_t)1<<16)
#define TSCNT_CLR               ((uint32_t)1<<4)
#define FLTCNT_CLR              ((uint32_t)1<<3)
#define TOCNT_CLR               ((uint32_t)1<<2)
#define E_CNT_CLR               ((uint32_t)1<<1)
#define PDCNT_CLR               ((uint32_t)1<<0)

#define REDGEINT                ((uint32_t)1<<31)
#define FEDGEINT                ((uint32_t)1<<30)
#define DFFULLINT               ((uint32_t)1<<29)
#define FCHGINT                 ((uint32_t)1<<28)
#define DCHGINT                 ((uint32_t)1<<27)
#define E_FULLINT               ((uint32_t)1<<26)
#define TOFULLINT               ((uint32_t)1<<25)
#define NFEDFULLINT             ((uint32_t)1<<24)

#define LOCK_EN                 ((uint32_t)1<<31)
#define ABS_SEL                 ((uint32_t)1<<29)
#define E_DGE_SEL               ((uint32_t)1<<28)
#define TCLK_POL                ((uint32_t)1<<26)
#define TCK_SEL(x)              ((uint32_t)(x)<<22)
#define FLT_F_MODE(x)           ((uint32_t)(x)<<20)
#define FLT_R_MODE(x)           ((uint32_t)(x)<<18)
#define CMP_ERR_SEL             ((uint32_t)1<<16)
#define CMP_ERR_BOTH            ((uint32_t)1<<17)
#define DMA_EN                  ((uint32_t)1<<15)
#define DMA_SEL(x)              ((uint32_t)(x)<<12)

static LIST_HEAD(ictc_list);

struct ictc_pin_map {
	u32 reg_base;
	u32 source_section;
	u32 *source_offset_base;
	u32 *source_base;
	u32 *source_range;
};

struct ictc_dev {
	struct device_node *np;
	uint32_t irq;
	uint32_t int_state;
	struct clk *pPClk;
	struct device *dev;
	struct list_head list;
	struct ictc_pin_map *ictc_pin_map_val;
	uint32_t num_gpio;
};

#define RTC_WKUP        0xa5

static struct tasklet_struct ictc_tasklet;
static void __iomem *ictc_base;
static uint32_t f_in_gpio_base;
static uint32_t f_in_gpio_bit;
static int32_t f_in_source;
static bool f_in_rtc_wkup;

static uint32_t irq_setting;

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

enum ictc_property_value_enum {
	R_EDGE_V = 0,
	F_EDGE_V,
	E_M_VAL_V,
	DUTY_ROUNDING_VAL_V,
	PRD_ROUNDING_VAL_V,
	FLT_F_MODE_V,
	FLT_R_MODE_V,
	TIME_OUT_V,
	ABS_SEL_V,
	E_SEL_V,
	TCK_POL_V,
	TCK_SEL_V,
	LOCK_EN_V,
	PROPERTY_VALUE_NUM_V
};

static uint32_t ictc_prop_v[PROPERTY_VALUE_NUM_V] = { 0, };

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

enum ictc_property_bool_enum {
	F_EDGE_INT = 0,
	R_EDGE_INT,
	DF_CNT_FULL_INT,
	F_CHG_INT,
	D_CHG_INT,
	E_CNT_FULL_INT,
	TO_CNT_FULL_INT,
	NF_ED_CNT_FULL_INT,
	TIME_STAMP_CNT,
	PROPERTY_BOOL_NUM
};

static uint32_t ictc_prop_b[PROPERTY_BOOL_NUM] = { 0, };

static void ictc_clear_interrupt(void);
static void ictc_clear_counter(void);
static void ictc_enable_interrupt(void);

static void ictc_tasklet_handler(unsigned long data)
{

	struct ictc_dev *idev = (struct ictc_dev *)data;
	struct ictc_dev *idev_pos, *idev_pos_safe;

	if (idev != NULL) {

		list_for_each_entry_safe(idev_pos, idev_pos_safe, &ictc_list,
					 list) {
#ifdef DEBUG_ICTC
			if ((idev_pos->int_state & (uint32_t) IRQ_STAT_REDGE) ==
			    (uint32_t) IRQ_STAT_REDGE) {
				debug_ictc("rising edge interrupt\n");
			}
			if ((idev_pos->int_state & (uint32_t) IRQ_STAT_FEDGE) ==
			    (uint32_t) IRQ_STAT_FEDGE) {
				debug_ictc("falling edge interrupt\n");
			}
			if ((idev_pos->
			     int_state & (uint32_t) IRQ_STAT_DFFULL) ==
			    (uint32_t) IRQ_STAT_DFFULL) {
				debug_ictc
				    ("duty and frequency counter full interrupt\n");
			}
			if ((idev_pos->int_state & (uint32_t) IRQ_STAT_FCHG) ==
			    (uint32_t) IRQ_STAT_FCHG) {
				debug_ictc("frequency changing interrupt\n");
			}
			if ((idev_pos->int_state & (uint32_t) IRQ_STAT_DCHG) ==
			    (uint32_t) IRQ_STAT_DCHG) {
				debug_ictc("duty changing interrupt\n");
			}
			if ((idev_pos->int_state & (uint32_t) IRQ_STAT_EFULL) ==
			    (uint32_t) IRQ_STAT_EFULL) {
				debug_ictc("edge counter full interrupt\n");
			}
			if ((idev_pos->
			     int_state & (uint32_t) IRQ_STAT_TOFULL) ==
			    (uint32_t) IRQ_STAT_TOFULL) {
				debug_ictc("timeout counter full interrupt\n");
			}
			if ((idev_pos->
			     int_state & (uint32_t) IRQ_STAT_NFEDFULL) ==
			    (uint32_t) IRQ_STAT_NFEDFULL) {
				debug_ictc
				    ("noise-filter and edge counter full interrupt\n");
			}
#endif
			list_del(&idev_pos->list);

			devm_kfree(idev->dev, idev_pos);
		}

		/* to do */

	}
}

static irqreturn_t ictc_interrupt_handler(int irq, void *dev)
{
	struct ictc_dev *idev = dev_get_drvdata(dev);
	struct ictc_dev *idev_new;

	idev_new = devm_kzalloc(idev->dev, sizeof(struct ictc_dev), GFP_KERNEL);
	idev_new->int_state = (uint32_t) IRQ_STAT_MASK & ictc_readl(IRQ_CTRL);
	list_add_tail(&idev_new->list, &ictc_list);

	tasklet_schedule(&ictc_tasklet);

	/* to do */

	ictc_clear_interrupt();
	ictc_clear_counter();
	ictc_enable_interrupt();

	return IRQ_HANDLED;

}

static int32_t ictc_parse_dt(struct device_node *np, struct device *dev)
{

	int32_t temp, ret = 0;
	uint32_t count;
	struct device_node *gpio_node;
	struct device_node *pinctrl_node;
	struct ictc_dev *idev = dev_get_drvdata(dev);
	uint32_t num_gpio = 0, i = 0, j, node_num;
	bool temp_bool;

	pinctrl_node = of_parse_phandle(np, "pinctrl-map", 0);

	for_each_child_of_node((pinctrl_node), (gpio_node)) {
		if (of_find_property(gpio_node, "gpio-controller", NULL) !=
		    NULL) {
			if ((UINT_MAX - num_gpio) < 1u) {
				dev_err(dev,
					"[ERROR][ICTC] warparound guard error\n");
				return -1;
			} else {
				num_gpio = num_gpio + 1u;
			}
		}
	}

	idev->num_gpio = num_gpio;

	if ((UINT_MAX / sizeof(struct ictc_pin_map)) < num_gpio) {
		return -1;
	} else {
		if (num_gpio != 0u) {
			idev->ictc_pin_map_val =
			    kzalloc(sizeof(struct ictc_pin_map) * num_gpio,
				    GFP_KERNEL);
		} else {
			return -1;
		}
	}

	if (idev->ictc_pin_map_val != NULL) {

		for_each_child_of_node((pinctrl_node), (gpio_node)) {
			if (of_find_property(gpio_node, "gpio-controller", NULL)
			    == NULL) {
				continue;
			}

			ret =
			    of_property_read_u32_index(gpio_node, "reg", 0,
						       &idev->
						       ictc_pin_map_val[i].
						       reg_base);
			if (ret < 0) {
				dev_err(dev,
					"[ERROR][ICTC] failed to get reg base\n");
				return ret;
			}

			ret =
			    of_property_read_u32_index(gpio_node, "source-num",
						       0,
						       &idev->
						       ictc_pin_map_val[i].
						       source_section);
			if (ret < 0) {
				dev_err(dev,
					"[ERROR][ICTC] failed to get source section number\n");
				return ret;
			}

			if (idev->ictc_pin_map_val[i].source_section != 0xffu) {
				if ((UINT_MAX / sizeof(u32)) <
				    idev->ictc_pin_map_val[i].source_section) {
					return -1;
				} else {
					idev->ictc_pin_map_val[i].
					    source_offset_base =
					    kzalloc(sizeof(u32) *
						    idev->ictc_pin_map_val[i].
						    source_section, GFP_KERNEL);
					idev->ictc_pin_map_val[i].source_base =
					    kzalloc(sizeof(u32) *
						    idev->ictc_pin_map_val[i].
						    source_section, GFP_KERNEL);
					idev->ictc_pin_map_val[i].source_range =
					    kzalloc(sizeof(u32) *
						    idev->ictc_pin_map_val[i].
						    source_section, GFP_KERNEL);
				}

				for (j = 0U;
				     j <
				     idev->ictc_pin_map_val[i].source_section;
				     j++) {
					if ((UINT_MAX - 3u) / 3u < j) {
						return -1;
					} else {
						ret =
						    of_property_read_u32_index
						    (gpio_node, "source-num",
						     ((j * 3u) + 1u),
						     &idev->ictc_pin_map_val[i].
						     source_offset_base[j]);
						if (ret < 0) {
							dev_err(dev,
								"[ERROR][PINCTRL] failed to get source offset base\n");
							return ret;
						}
						ret =
						    of_property_read_u32_index
						    (gpio_node, "source-num",
						     ((j * 3u) + 2u),
						     &idev->ictc_pin_map_val[i].
						     source_base[j]);
						if (ret < 0) {
							dev_err(dev,
								"[ERROR][PINCTRL] failed to get source base\n");
							return ret;
						}
						ret =
						    of_property_read_u32_index
						    (gpio_node, "source-num",
						     ((j * 3u) + 3u),
						     &idev->ictc_pin_map_val[i].
						     source_range[j]);
						if (ret < 0) {
							dev_err(dev,
								"[ERROR][PINCTRL] failed to get source range\n");
							return ret;
						}
					}
				}
			}

			if ((UINT_MAX - i) < 1u) {
				dev_err(dev,
					"[ERROR][ICTC] warparound guard error\n");
				return -1;
			} else {
				i++;
			}
		}

		count = ARRAY_SIZE(ictc_prop_v_l);

		for (node_num = 0; node_num < count; node_num++) {
			temp = of_property_read_u32(np, ictc_prop_v_l[node_num],
						    &ictc_prop_v[node_num]);
			if (temp != 0) {
				err_ictc("no property found in DT : %s\n",
					 ictc_prop_v_l[node_num]);
				ret = -EINVAL;
			}
			debug_ictc("%s -> %x\n", ictc_prop_v_l[node_num],
				   ictc_prop_v[node_num]);

			if (ret != 0) {
				break;
			}

		}

		count = ARRAY_SIZE(ictc_prop_b_l);

		for (node_num = 0; node_num < count; node_num++) {

			temp_bool =
			    of_property_read_bool(np, ictc_prop_b_l[node_num]);

			if (temp_bool) {
				ictc_prop_b[node_num] = 1;
			} else {
				ictc_prop_b[node_num] = 0;
			}

			debug_ictc("%s -> %x\n", ictc_prop_b_l[node_num],
				   ictc_prop_b[node_num]);
		}
	} else {
		return -1;
	}

	return ret;

}

static void ictc_configure(void)
{
	uint32_t config_val = 0;

	if (ictc_prop_v[ABS_SEL_V] != 0u) {
		config_val |= ABS_SEL;
	}

	if (ictc_prop_v[E_SEL_V] != 0u) {
		config_val |= E_DGE_SEL;
	}

	if (ictc_prop_v[TCK_POL_V] != 0u) {
		config_val |= TCLK_POL;
	}

	config_val |= TCK_SEL((ictc_prop_v[TCK_SEL_V]));

	config_val |= FLT_F_MODE((ictc_prop_v[FLT_F_MODE_V]));
	config_val |= FLT_R_MODE((ictc_prop_v[FLT_R_MODE_V]));

	if ((ictc_prop_b[D_CHG_INT] != 0u) && (ictc_prop_b[F_CHG_INT] != 0u)) {
		config_val |= CMP_ERR_BOTH;
	} else {
		if (ictc_prop_b[D_CHG_INT] != 0u) {
			config_val |= CMP_ERR_SEL;
		} else {
			debug_ictc("no compare error selection\n");
		}
	}

	if (!f_in_rtc_wkup) {
		config_val |= (uint32_t) f_in_source;
	} else {
		config_val |= (uint32_t) RTC_WKUP;
	}

	ictc_writel(config_val, OP_MODE_CTRL);

	ictc_writel(ictc_prop_v[TIME_OUT_V], TIME_OUT);

	ictc_writel(ictc_prop_v[R_EDGE_V], R_EDGE);
	ictc_writel(ictc_prop_v[F_EDGE_V], F_EDGE);

	ictc_writel(ictc_prop_v[E_M_VAL_V], CNT_EDGE_MAT);

	ictc_writel(ictc_prop_v[DUTY_ROUNDING_VAL_V], DUTY_CMP_RND);
	ictc_writel(ictc_prop_v[PRD_ROUNDING_VAL_V], PRD_CMP_RND);

}

static void ictc_clear_interrupt(void)
{
	ictc_writel(IRQ_CLR_MASK, IRQ_CTRL);
	ictc_writel(0, IRQ_CTRL);
}

static void ictc_enable_interrupt(void)
{
	if (irq_setting == 0u) {

		uint32_t enable_int = 0;

		if (ictc_prop_b[R_EDGE_INT] != 0u) {
			enable_int |= REDGEINT;
		}
		if (ictc_prop_b[F_EDGE_INT] != 0u) {
			enable_int |= FEDGEINT;
		}
		if (ictc_prop_b[DF_CNT_FULL_INT] != 0u) {
			enable_int |= DFFULLINT;
		}
		if (ictc_prop_b[F_CHG_INT] != 0u) {
			enable_int |= FCHGINT;
		}
		if (ictc_prop_b[D_CHG_INT] != 0u) {
			enable_int |= DCHGINT;
		}
		if (ictc_prop_b[E_CNT_FULL_INT] != 0u) {
			enable_int |= E_FULLINT;
		}
		if (ictc_prop_b[TO_CNT_FULL_INT] != 0u) {
			enable_int |= TOFULLINT;
		}
		if (ictc_prop_b[NF_ED_CNT_FULL_INT] != 0u) {
			enable_int |= NFEDFULLINT;
		}

		irq_setting = enable_int;

		ictc_writel(enable_int, IRQ_CTRL);
	} else {
		ictc_writel(irq_setting, IRQ_CTRL);
	}

}

static void ictc_clear_counter(void)
{
	uint32_t val;

	val = ictc_readl(OP_EN_CTRL) | (uint32_t) 0x1f;
	ictc_writel(val, OP_EN_CTRL);
	val = ictc_readl(OP_EN_CTRL) & (~(uint32_t) 0x1f);
	ictc_writel(val, OP_EN_CTRL);
}

static void ictc_enable_counter(void)
{

	uint32_t enable_val = 0;

	if (ictc_prop_b[TIME_STAMP_CNT] != 0u) {
		enable_val |= TSCNT_EN;
	}
	if (ictc_prop_b[TO_CNT_FULL_INT] != 0u) {
		enable_val |= TOCNT_EN;
	}
	if ((ictc_prop_b[D_CHG_INT] != 0u) || (ictc_prop_b[F_CHG_INT] != 0u)
	    || (ictc_prop_b[DF_CNT_FULL_INT] != 0u)) {
		enable_val |= PDCNT_EN;
	}
	if ((ictc_prop_b[R_EDGE_INT] != 0u) || (ictc_prop_b[F_EDGE_INT] != 0u)
	    || (ictc_prop_b[E_CNT_FULL_INT] != 0u)
	    || (ictc_prop_b[NF_ED_CNT_FULL_INT] != 0u)) {
		enable_val |= E_CNT_EN;
	}

	enable_val |= TCLK_EN | FLTCNT_EN;

	ictc_writel(enable_val, OP_EN_CTRL);

}

static void ictc_enable(void)
{

	ictc_writel(ictc_readl(OP_EN_CTRL) | ICTC_EN, OP_EN_CTRL);

}

static int32_t gpio_to_f_in(uint32_t gpio_base, uint32_t gpio_bit,
			    struct device *dev)
{

	uint32_t count, i, f_in_gpio_num;
	int32_t pin_valid = 0;
	struct ictc_dev *idev = dev_get_drvdata(dev);

	for (count = 0; count < idev->num_gpio; count++) {

		if (idev->ictc_pin_map_val[count].reg_base == gpio_base) {
			if (idev->ictc_pin_map_val[count].source_section ==
			    0xffu) {
				err_ictc
				    ("[ERROR][ICTC] %s: not supported for ICTC source\n",
				     __func__);
				return -EINVAL;
			}
		} else {
			for (i = 0;
			     i < idev->ictc_pin_map_val[count].source_section;
			     i++) {
				if ((UINT_MAX -
				     idev->ictc_pin_map_val[count].
				     source_offset_base[i]) <
				    idev->ictc_pin_map_val[count].
				    source_range[i]) {
					return -1;
				} else {
					if ((gpio_bit >=
					     idev->ictc_pin_map_val[count].
					     source_offset_base[i])
					    && (gpio_bit <
						(idev->ictc_pin_map_val[count].
						 source_offset_base[i] +
						 idev->ictc_pin_map_val[count].
						 source_range[i]))) {
						if ((UINT_MAX -
						     idev->
						     ictc_pin_map_val[count].
						     source_base[i]) <
						    (gpio_bit -
						     idev->
						     ictc_pin_map_val[count].
						     source_offset_base[i])) {
							return -1;
						} else {
							f_in_gpio_num =
							    idev->
							    ictc_pin_map_val
							    [count].
							    source_base[i] +
							    (gpio_bit -
							     idev->
							     ictc_pin_map_val
							     [count].
							     source_offset_base
							     [i]);
						}
						pin_valid = 1;
						break;
					}
				}
			}
		}

	}
	if (pin_valid != 0) {
		return (int32_t) f_in_gpio_num;
	} else {
		return -1;
	}

}

static int32_t ictc_probe(struct platform_device *pdev)
{
	struct ictc_dev *idev;
	struct device_node *gpio_node;
	struct device_node *np = pdev->dev.of_node;
	struct clk *pPClk = of_clk_get(pdev->dev.of_node, 0);
	int32_t ret;

	//initialize global variables
	f_in_gpio_base = 0;
	f_in_gpio_bit = 0;
	f_in_source = 0;
	irq_setting = 0;
	f_in_rtc_wkup = (bool) false;

	if (np == NULL) {
		err_ictc("device does not exist!\n");
		ret = -EINVAL;
	} else {
		ret = SUCCESS;
	}

	if (ret == SUCCESS) {
		idev =
		    devm_kzalloc(&pdev->dev, sizeof(struct ictc_dev),
				 GFP_KERNEL);

		if (idev == NULL) {
			ret = -ENXIO;
		} else {

			INIT_LIST_HEAD(&idev->list);

			idev->np = np;
			idev->irq = (uint32_t) platform_get_irq(pdev, 0);
			idev->pPClk = pPClk;
			idev->dev = &pdev->dev;
			platform_set_drvdata(pdev, idev);

			ret = clk_prepare_enable(pPClk);

			if (ret == SUCCESS) {
				ret =
				    request_irq(idev->irq,
						(irq_handler_t)
						ictc_interrupt_handler,
						IRQF_SHARED, "ICTC",
						(void *)&pdev->dev);
				tasklet_init(&ictc_tasklet,
					     ictc_tasklet_handler,
					     (unsigned long)idev);

				debug_ictc("ICTC probe ictc irq : %d\n",
					   idev->irq);

				if (ret != SUCCESS) {
					err_ictc("irq req. fail: %d irq: %x\n",
						 ret, idev->irq);
				} else {

					ret = ictc_parse_dt(np, &pdev->dev);

					if (ret != SUCCESS) {
						err_ictc("ictc:No dev node\n");
					} else {

						ictc_base = of_iomap(np, 0);

						f_in_rtc_wkup =
						    of_property_read_bool(np,
									  "f-in-rtc-wkup");

						if (!f_in_rtc_wkup) {

							gpio_node =
							    of_parse_phandle(np,
									     "f-in-gpio",
									     0);
							ret =
							    of_property_read_u32_index
							    (gpio_node, "reg",
							     0,
							     &f_in_gpio_base);
							if (ret != SUCCESS) {
								return -EINVAL;
							}
							ret =
							    of_property_read_u32_index
							    (np, "f-in-gpio", 1,
							     &f_in_gpio_bit);
							if (ret != SUCCESS) {
								return -EINVAL;
							}
							f_in_source =
							    gpio_to_f_in
							    (f_in_gpio_base,
							     f_in_gpio_bit,
							     &pdev->dev);

							if (f_in_source < 0) {
								err_ictc
								    ("ictc: invalid gpio\n");
								return -EINVAL;
							}

						}

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

static int32_t ictc_suspend(struct device *dev)
{
	struct ictc_dev *idev = dev_get_drvdata(dev);
	int32_t ret = 0;

	if (idev == NULL) {
		ret = -EINVAL;
	} else {
		tasklet_kill(&ictc_tasklet);
		free_irq(idev->irq, (void *)dev);
		clk_disable_unprepare(idev->pPClk);
	}

	return ret;

}

static int32_t ictc_resume(struct device *dev)
{
	struct ictc_dev *idev = dev_get_drvdata(dev);
	int32_t ret;

	if (idev == NULL) {
		ret = -EINVAL;
	} else {
		ret = SUCCESS;
	}

	if (ret == SUCCESS) {
		ret = clk_prepare_enable(idev->pPClk);

		if (ret == SUCCESS) {

			ret =
			    request_irq(idev->irq, ictc_interrupt_handler,
					IRQF_SHARED, "ICTC", (void *)dev);
			tasklet_init(&ictc_tasklet,
				     ictc_tasklet_handler, (unsigned long)idev);

			if (ret != SUCCESS) {
				tasklet_kill(&ictc_tasklet);
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

static int32_t __init ictc_init(void)
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
