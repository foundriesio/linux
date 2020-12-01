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


#ifdef DEBUG_ICTC /* For enabling debug, define DEBUG_ICTC */
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

struct ictc_dev {
        struct device_node *np;
        uint32_t irq;
        uint32_t int_state;
        struct clk *pPClk;
        struct device *dev;
        struct list_head list;
};

//TCC803x GPIOs
static int32_t gpio_f_in[] = {
        0x1,                    //GPIO_A
        0x21,                   //GPIO_B
        0x3e,                   //GPIO_C
        0,                      //GPIO_D
        0x5c,                   //GPIO_E
        0,                      //GPIO_F
        0x70,                   //GPIO_G
        0x7b,                   //GPIO_H
        0,                      //GPIO_K
        0x87,                   //GPIO_MA
        0xa6,                   //GPIO_SD0
        0xb4,                   //GPIO_SD1
        0xc0,                   //GPIO_SD2
};

static int32_t gpio_num_group[] = {
        0,                      //GPIO_A
        32,                     //GPIO_B
        61,                     //GPIO_C
        91,                     //GPIO_D
        113,                    //GPIO_E
        133,                    //GPIO_F
        165,                    //GPIO_G
        176,                    //GPIO_H
        188,                    //GPIO_K
        207,                    //GPIO_MA
        237,                    //GPIO_SD0
        252,                    //GPIO_SD1
        263                     //GPIO_SD2
};

#define RTC_WKUP        0xa5

static struct tasklet_struct ictc_tasklet;
static void __iomem *ictc_base;
static int32_t f_in_gpio;
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

static uint32_t ictc_prop_v[PROPERTY_VALUE_NUM_V] = {0,};

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

static uint32_t ictc_prop_b[PROPERTY_BOOL_NUM] = {0, };

static void ictc_clear_interrupt(void);
static void ictc_clear_counter(void);
static void ictc_enable_interrupt(void);

static void ictc_tasklet_handler(unsigned long data)
{

        struct ictc_dev *idev = (struct ictc_dev *)data;
        struct ictc_dev *idev_pos, *idev_pos_safe;
        uint32_t int_state;

        if(idev != NULL) {

        list_for_each_entry_safe(idev_pos, idev_pos_safe, &ictc_list, list) {
                int_state = idev_pos->int_state;
                list_del(&idev_pos->list);

                if ((int_state & (uint32_t)IRQ_STAT_REDGE) ==
                    (uint32_t)IRQ_STAT_REDGE) {
                        debug_ictc("rising edge interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_FEDGE) ==
                    (uint32_t)IRQ_STAT_FEDGE) {
                        debug_ictc("falling edge interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_DFFULL) ==
                    (uint32_t)IRQ_STAT_DFFULL) {
                        debug_ictc
                            ("duty and frequency counter full interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_FCHG) ==
                    (uint32_t)IRQ_STAT_FCHG) {
                        debug_ictc("frequency changing interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_DCHG) ==
                    (uint32_t)IRQ_STAT_DCHG) {
                        debug_ictc("duty changing interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_EFULL) ==
                    (uint32_t)IRQ_STAT_EFULL) {
                        debug_ictc("edge counter full interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_TOFULL) ==
                    (uint32_t)IRQ_STAT_TOFULL) {
                        debug_ictc("timeout counter full interrupt\n");
                }
                if ((int_state & (uint32_t)IRQ_STAT_NFEDFULL) ==
                    (uint32_t)IRQ_STAT_NFEDFULL) {
                        debug_ictc
                            ("noise-filter and edge counter full interrupt\n");
                }

                devm_kfree(idev->dev, idev_pos);
        }

        /* to do */


        }
}

static irqreturn_t ictc_interrupt_handler(int irq, void *dev)
{
        struct ictc_dev *idev = dev_get_drvdata(dev);
        struct ictc_dev *idev_new;

        disable_irq((uint32_t)irq);
        idev_new = devm_kzalloc(idev->dev, sizeof(struct ictc_dev), GFP_KERNEL);
        idev_new->int_state =
            (uint32_t)IRQ_STAT_MASK & ictc_readl(IRQ_CTRL);
        list_add_tail(&idev_new->list, &ictc_list);

        tasklet_schedule(&ictc_tasklet);

        /* to do */


        ictc_clear_interrupt();
        ictc_clear_counter();
        ictc_enable_interrupt();
        enable_irq((uint32_t)irq);

        return IRQ_HANDLED;

}

static int32_t ictc_parse_dt(struct device_node *np)
{

        int32_t  temp, ret = 0;
        uint32_t node_num;
        uint64_t count;
        bool temp_bool;

        count = ARRAY_SIZE(ictc_prop_v_l);

        for (node_num = 0; node_num < (uint32_t)count; node_num++) {
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

                temp_bool = of_property_read_bool(np, ictc_prop_b_l[node_num]);

                if(temp_bool) {
                        ictc_prop_b[node_num] = 1;
                } else {
                        ictc_prop_b[node_num] = 0;
                }


                debug_ictc("%s -> %x\n", ictc_prop_b_l[node_num],
                           ictc_prop_b[node_num]);
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
                config_val |= (uint32_t)f_in_source;
        } else {
                config_val |= (uint32_t)RTC_WKUP;
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

        val = ictc_readl(OP_EN_CTRL) | (uint32_t)0x1f;
        ictc_writel(val, OP_EN_CTRL);
        val = ictc_readl(OP_EN_CTRL) & (~(uint32_t)0x1f);
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

static int32_t gpio_to_f_in(int32_t gpio_num)
{

        int32_t count;
        int32_t gpio_group, gpio_group_val = 0;

        count = ARRAY_SIZE(gpio_num_group);

        debug_ictc("gpio group count : %ld\n", count);

        for (gpio_group = 1; gpio_group < count; gpio_group++) {

                if ((gpio_num - gpio_num_group[gpio_group]) < 0) {
                        gpio_group_val = gpio_group - 1;
                        break;
                }

        }

        debug_ictc("gpio_f_in : 0x%x gpio_num_group : %d, gpio_num : %d",
                   gpio_f_in[gpio_group_val], gpio_num_group[gpio_group_val],
                   gpio_num);

        return gpio_f_in[gpio_group_val] + (gpio_num -
                                            gpio_num_group[gpio_group_val]);

}

static int32_t ictc_probe(struct platform_device *pdev)
{
        struct ictc_dev *idev;
        struct device_node *np = pdev->dev.of_node;
        uint32_t irq = (uint32_t)platform_get_irq(pdev, 0);
        struct clk *pPClk = of_clk_get(pdev->dev.of_node, 0);
        int32_t ret;

        //initialize global variables
        f_in_gpio = 0;
        f_in_source = 0;
        irq_setting = 0;
        f_in_rtc_wkup = (bool)false;

        debug_ictc("ICTC probe ictc irq : %d\n", irq);

        if (np == NULL) {
                err_ictc("device does not exist!\n");
                ret = -EINVAL;
        } else {
                ret = SUCCESS;
        }

        if(ret == SUCCESS) {
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
                                    request_irq(irq, (irq_handler_t)ictc_interrupt_handler,
                                                IRQF_SHARED, "ICTC",
                                                (void *)&pdev->dev);
                                        tasklet_init(&ictc_tasklet,
                                             ictc_tasklet_handler,
                                             (unsigned long)idev);

                                if (ret != SUCCESS) {
                                        err_ictc("irq req. fail: %d irq: %x\n",
                                                 ret, irq);
                                } else {

                                        ictc_base = of_iomap(np, 0);

                                        f_in_rtc_wkup = of_property_read_bool(np,
                                                                  "f-in-rtc-wkup");

                                        if (!f_in_rtc_wkup) {

                                                f_in_gpio =
                                                    of_get_named_gpio(np,
                                                                      "f-in-gpio",
                                                                      0);
                                                f_in_source =
                                                    gpio_to_f_in(f_in_gpio);


                                        }

                                        ret = ictc_parse_dt(np);

                                        if (ret != SUCCESS) {
                                                free_irq(irq, (void *)&pdev->dev);
                                                tasklet_kill(&ictc_tasklet);
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


        if(ret == SUCCESS) {
                ret = clk_prepare_enable(idev->pPClk);

                if (ret == SUCCESS) {

                        ret =
                            request_irq(idev->irq, ictc_interrupt_handler,
                                        IRQF_SHARED, "ICTC", (void *)dev);
                                tasklet_init(&ictc_tasklet,
                                             ictc_tasklet_handler,
                                             (unsigned long)idev);

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
