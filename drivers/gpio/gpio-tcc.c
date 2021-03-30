#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#if defined(CONFIG_PINCTRL_TCC_SCFW)
#include <linux/soc/telechips/tcc_sc_protocol.h>

static const struct tcc_sc_fw_handle *sc_fw_handle;
#endif

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int32_t request_gpio_to_sc(
                ulong address, ulong bit_number, ulong width, ulong value)
{
        s32 ret;
        u32 u32_mask = 0xFFFFFFFFU;
        u32 addr_32 = (u32)(address & u32_mask);
        u32 bit_num_32 = (u32)(bit_number & u32_mask);
        u32 width_32 = (u32)(width & u32_mask);
        u32 value_32 = (u32)(value & u32_mask);

        if (sc_fw_handle != NULL) {
                ret = sc_fw_handle
                        ->ops.gpio_ops->request_gpio_no_res
                        (sc_fw_handle, addr_32,
                         bit_num_32, width_32, value_32);
        } else {
                (void)pr_err(
                                "[ERROR][PINCTRL] %s : sc_fw_handle is NULL"
                                , __func__);
                ret = -EINVAL;
        }

        return ret;
}
#endif

#define EINT_MUX 0x280
#define GPIO_BIT_SET 0x8
#define GPIO_BIT_CLEAR 0xC

static const struct of_device_id telechips_gpio_dt_ids[] = {
	{.compatible = "telechips,tcc-gpio",	.data = NULL,},
};

struct tcc_gpio_port;

struct tcc_gpio_soc_data {
	int temp_data;
};

struct tcc_gpio_group {
	struct tcc_gpio_port *port;
        struct gpio_chip gc;
        struct irq_chip ic;
        u32 source_section;
        u32 *gpio_offset_num;
        u32 *source_offset_num;
        u32 *source_range;
	const char *name;
	u32 reg_offset;
};

struct tcc_gpio_port {
	int gpio_num_gr;
	void __iomem *base;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 raw_base;
#endif
	struct tcc_gpio_group *gpio_gr;
	const struct tcc_gpio_soc_data *sdata;
	int *irq;
	int irq_num;
	int **irq_port_map;
};

static void tcc_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val);

static irqreturn_t tcc_gpio_irq_handler(int irq, void *data)
{
	struct tcc_gpio_group *gpio_gr = data;
	struct tcc_gpio_port *port = gpio_gr->port;
	struct irq_desc *desc = irq_to_desc(irq);
	struct irq_data *d = irq_get_irq_data(irq);
	struct irq_chip *chip = d->chip;
	irq_hw_number_t hwirq;
	int i;
	int irq_source;
	int pin;
	int idx_sel_reg, idx_sel_plc;

        pr_debug("%s: ############name : %s %s %s %s %s\n"
            , __func__, d->chip->name, gpio_gr->name, port->gpio_gr->ic.name, port->gpio_gr->gc.label, port->gpio_gr->name);

	hwirq = irqd_to_hwirq(d);
	hwirq -= 32;

#if defined(CONFIG_ARCH_TCC897X)
	for (i = 0; i < port->irq_num; i++) {
	    pr_debug("[GPIO][DEBUG] %s: irq map 4 : %ld hwirq : %ld\n", __func__, port->irq_port_map[i][4], hwirq);
		if (port->irq_port_map[i][4] == hwirq) {
			break;
		}
	}

	if (i > 15) {
		i -= 16;
		idx_sel_reg = i / 4;
		idx_sel_plc = i % 4;
	} else {
		idx_sel_reg = i / 4;
		idx_sel_plc = i % 4;
	}

#elif defined(CONFIG_TCC803X_CA7S)
	if (hwirq > 7) {
		hwirq -= 8;
		idx_sel_reg = hwirq / 2;
		idx_sel_plc = hwirq % 2;
	} else {
		idx_sel_reg = hwirq / 2;
		idx_sel_plc = hwirq % 2;
	}
#else
	if (hwirq > 15) {
		hwirq -= 16;
		idx_sel_reg = hwirq / 4;
		idx_sel_plc = hwirq % 4;
	} else {
		idx_sel_reg = hwirq / 4;
		idx_sel_plc = hwirq % 4;
	}
#endif
	pr_debug("[GPIO][DEBUG] %s: hwirq : %ld\n", __func__, hwirq);
	pr_debug("[GPIO][DEBUG] %s: idx_sel_reg : %d idx_sel_lc : %d\n",
		 __func__, idx_sel_reg, idx_sel_plc);

#if defined(CONFIG_TCC803X_CA7S)
	irq_source =
	    0xff & (readl(port->base + (0x280 + (idx_sel_reg * 4))) >>
		    (16 * idx_sel_plc));
#else
	irq_source =
	    0xff & (readl(port->base + (0x280 + (idx_sel_reg * 4))) >>
		    (8 * idx_sel_plc));
#endif

	pr_debug("[GPIO][DEBUG] %s: irq_source num : %d\n", __func__,
		 irq_source);
	pr_debug("[GPIO][DEBUG] %s: sources map : %d %d\n", __func__,
		 port->irq_port_map[0][1], port->irq_port_map[0][2]);

	for (i = 0; i < port->irq_num / 2; i++) {
		if (port->irq_port_map[i][2] == irq_source) {
			pin = port->irq_port_map[i][1];
			break;
		}
	}

	pr_debug("[GPIO][DEBUG] %s: pin num : %d\n", __func__, pin);

	chained_irq_enter(chip, desc);

	generic_handle_irq(irq_find_mapping(gpio_gr->gc.irqdomain, pin));

	chained_irq_exit(chip, desc);

	return IRQ_HANDLED;
}

static int tcc_gpio_irq_set_type(struct irq_data *d, u32 type)
{
	struct tcc_gpio_port *port =
	    gpiochip_get_data(irq_data_get_irq_chip_data(d));
	int ret;
	int pin_valid = 0;
	int i;
	int irq_source;
	int pin = d->hwirq;
	int irq_mux_num;
	int idx_sel_reg, idx_sel_plc;
	int irq_valid = 0;
	struct tcc_gpio_group *gpio_gr;


	gpio_gr = port->gpio_gr;

	for(i = 0; i < port->gpio_num_gr; i++) {
	    if(!strcmp(d->chip->name, gpio_gr->name)) {
		break;
	    } else {
		gpio_gr++;
	    }

	}

        pr_debug("%s: ############name : %s %s %s %s %s\n"
            , __func__, d->chip->name, gpio_gr->name, port->gpio_gr->ic.name, port->gpio_gr->gc.label, port->gpio_gr->name);

	if (gpio_gr->source_section == 0xff) {
		pr_err
		    ("[GPIO][ERROR] %s : external interrupt is not supported\n",
		     __func__);
		return -EINVAL;
	}

	for (i = 0; i < gpio_gr->source_section; i++) {

		if ((pin >= gpio_gr->gpio_offset_num[i]) &&
		    (pin <
		     (gpio_gr->gpio_offset_num[i] + gpio_gr->source_range[i]))) {

			irq_source = gpio_gr->source_offset_num[i] +
			    (pin - gpio_gr->gpio_offset_num[i]);
			pin_valid = 1;	//true
			break;

		} else {

			pin_valid = 0;	//false

		}
	}

	if (!pin_valid) {
		pr_err("[GPIO][ERROR] %s: %d is out of range of pin number\n"
			, __func__, pin);
		return -EINVAL;
	}

	for (i = 0; i < port->irq_num / 2; i++) {
		pr_debug(
		    "[GPIO][DEBUG] %s: pin : %d, irq_source : %d before setting\n"
		     , __func__, port->irq_port_map[i][1],
		     port->irq_port_map[i][2]);
		if (port->irq_port_map[i][3] == 0) {
			port->irq_port_map[i][1] = pin;
			port->irq_port_map[i][2] = irq_source;
			port->irq_port_map[i][3] = 1;
			irq_mux_num = i;
			irq_valid = 1;
			pr_debug(
			    "[GPIO][DEBUG] %s: pin : %d, irq_source : %d irq mux num : %d after setting\n"
			     , __func__, port->irq_port_map[i][1],
			     port->irq_port_map[i][2], i);
			break;
		}
	}

	if (irq_valid == 0) {
		pr_err("[GPIO][ERROR] %s: no more irq\n", __func__);
		return -1;
	}
#if defined(CONFIG_TCC803X_CA7S)
	idx_sel_reg = port->irq_port_map[i][0] / 2;
	idx_sel_plc = port->irq_port_map[i][0] % 2;
#else
	idx_sel_reg = port->irq_port_map[i][0] / 4;
	idx_sel_plc = port->irq_port_map[i][0] % 4;
#endif

#if defined(CONFIG_TCC803X_CA7S)
	irq_source = (irq_source | (irq_source << 8)) << (idx_sel_plc * 16);
	writel(readl(port->base + EINT_MUX + (idx_sel_reg * 4)) |
	       irq_source,
	       port->base + EINT_MUX + (idx_sel_reg * 4));
#else
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	(void)request_gpio_to_sc(port->raw_base + EINT_MUX + (idx_sel_reg * 4),
			    (idx_sel_plc * 8), 8, irq_source);
#else
	writel(readl(port->base + EINT_MUX + (idx_sel_reg * 4)) |
	       irq_source << (idx_sel_plc * 8),
	       port->base + EINT_MUX + (idx_sel_reg * 4));
#endif
#endif
	pr_debug("[GPIO][DEBUG] %s: both irq hwirq : %ld type!! : %d\n",
		 __func__, d->hwirq, type);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		pr_debug("[GPIO][DEBUG] %s: type edge rising\n", __func__);
		ret = devm_request_irq(gpio_gr->gc.parent, port->irq[irq_mux_num],
				       tcc_gpio_irq_handler,
				       IRQ_TYPE_EDGE_RISING, KBUILD_MODNAME,
				       gpio_gr);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		pr_debug("[GPIO][DEBUG] %s: type edge falling\n", __func__);
		ret = devm_request_irq(gpio_gr->gc.parent,
				       port->irq[irq_mux_num +
						 port->irq_num / 2],
				       tcc_gpio_irq_handler,
				       IRQ_TYPE_EDGE_RISING, KBUILD_MODNAME,
				       gpio_gr);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		pr_debug("[GPIO][DEBUG] %s: type edge both\n", __func__);
		ret = devm_request_irq(gpio_gr->gc.parent, port->irq[irq_mux_num],
				       tcc_gpio_irq_handler,
				       IRQ_TYPE_EDGE_RISING, KBUILD_MODNAME,
				       gpio_gr);
		ret =
		    devm_request_irq(gpio_gr->gc.parent,
				     port->irq[irq_mux_num + port->irq_num / 2],
				     tcc_gpio_irq_handler, IRQ_TYPE_EDGE_RISING,
				     KBUILD_MODNAME,
				     gpio_gr);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		pr_debug("[GPIO][DEBUG] %s: type level low\n", __func__);
		ret = devm_request_irq(gpio_gr->gc.parent,
				       port->irq[irq_mux_num +
						 port->irq_num / 2],
				       tcc_gpio_irq_handler,
				       IRQ_TYPE_LEVEL_HIGH, KBUILD_MODNAME,
				       gpio_gr);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		pr_debug("[GPIO][DEBUG] %s: type level high\n", __func__);
		ret = devm_request_irq(gpio_gr->gc.parent, port->irq[irq_mux_num],
				       tcc_gpio_irq_handler,
				       IRQ_TYPE_LEVEL_HIGH, KBUILD_MODNAME,
				       gpio_gr);
		break;
	default:
		return -EINVAL;
	}

	if (type & IRQ_TYPE_LEVEL_MASK)
		irq_set_handler_locked(d, handle_level_irq);
	else
		irq_set_handler_locked(d, handle_edge_irq);

	return 0;
}

static int tcc_gpio_direction_input(struct gpio_chip *chip, unsigned gpio)
{
        return pinctrl_gpio_direction_input(chip->base + gpio);
}

static int tcc_gpio_direction_output(struct gpio_chip *chip, unsigned gpio,
                                       int value)
{
	tcc_gpio_set(chip, gpio, value);
        return pinctrl_gpio_direction_output(chip->base + gpio);
}

static int tcc_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
        struct tcc_gpio_port *port = gpiochip_get_data(gc);
        struct tcc_gpio_group *gpio_gr = port->gpio_gr;
        unsigned long bit = BIT(gpio);
	int i;
        void __iomem *reg;

        for(i = 0; i < port->gpio_num_gr; i++) {
            if(!strcmp(gc->label, gpio_gr->name)) {
                break;
            } else {
                gpio_gr++;
            }
        }

        reg = port->base + gpio_gr->reg_offset;

	//pr_debug("%s: base : 0x%px, offset 0x%x\n", __func__, port->base, gpio_gr->reg_offset);

	return readl_relaxed(reg) & bit;
}

static void tcc_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
        struct tcc_gpio_port *port = gpiochip_get_data(gc);
	struct tcc_gpio_group *gpio_gr = port->gpio_gr;
        unsigned long bit = BIT(gpio);
	void __iomem *reg;
	int i;

        for(i = 0; i < port->gpio_num_gr; i++) {
            if(!strcmp(gc->label, gpio_gr->name)) {
                break;
            } else {
                gpio_gr++;
            }
        }

	reg = port->base + gpio_gr->reg_offset;

	//pr_debug("%s: base : 0x%px, offset 0x%x\n", __func__, port->base, gpio_gr->reg_offset);

	if(val) {
	    writel_relaxed(bit, reg + GPIO_BIT_SET);
	} else {
	    writel_relaxed(bit, reg + GPIO_BIT_CLEAR);
	}
}

static int tcc_get_direction(struct gpio_chip *gc, unsigned int gpio)
{
        struct tcc_gpio_port *port = gpiochip_get_data(gc);
        struct tcc_gpio_group *gpio_gr = port->gpio_gr;
        unsigned long bit = BIT(gpio);
	int output_enable_val, i;
        void __iomem *reg;

        for(i = 0; i < port->gpio_num_gr; i++) {
            if(!strcmp(gc->label, gpio_gr->name)) {
                break;
            } else {
                gpio_gr++;
            }
        }

        reg = port->base + gpio_gr->reg_offset;

	//pr_debug("%s: base : 0x%px, offset 0x%x, bit : %d\n", __func__, port->base, gpio_gr->reg_offset, bit);

	output_enable_val = readl_relaxed(reg+0x4);;

	pr_debug("%s: output enable val : 0x%x\n",__func__, output_enable_val);
	output_enable_val &= bit;

	if(output_enable_val==0) {
	    pr_debug("%s: input\n", __func__);
	    return 1;
	} else {
	    pr_debug("%s: output\n", __func__);
	    return 0;
	}
}

static void tcc_irq_ack(struct irq_data *d)
{
}

static int telechips_gpio_probe(struct platform_device *pdev)
{
    const struct of_device_id *of_id =
	of_match_device(telechips_gpio_dt_ids, &pdev->dev);
    struct device *dev = &pdev->dev;
    struct device_node *node = dev->of_node;
    struct device_node *np;
    struct tcc_gpio_port *port;
    struct resource *res;
    struct gpio_chip *gc;
    struct irq_chip *ic;
    struct tcc_gpio_group *gpio_gr;
    u32 source_section;
    int gpio_num;
    int irq_num;
    int i;
    struct property *prop;
    int ret;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
    struct device_node *sc_np;
#endif

    dev_dbg(&(pdev->dev), "%s: tcc gpio driver start\n", __func__);

    port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
    if (!port)
	return -ENOMEM;

    port->sdata = of_id->data;
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    port->base = devm_ioremap_resource(dev, res);
    if (IS_ERR(port->base))
	return PTR_ERR(port->base);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
	port->raw_base = res->start;
	sc_np = of_parse_phandle(node, "sc-firmware", 0);
	if (sc_np == NULL) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : sc_np == NULL\n"
				, __func__);
		return -EINVAL;
	}

	sc_fw_handle = tcc_sc_fw_get_handle(sc_np);
	if (sc_fw_handle == NULL) {
		dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] %s : sc_fw_handle == NULL\n"
			, __func__);
		return -EINVAL;
	}

	if ((sc_fw_handle->version.major == 0U)
			&& (sc_fw_handle->version.minor == 0U)
			&& (sc_fw_handle->version.patch < 7U)) {
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : The version of SCFW is low. So, register cannot be set through SCFW.\n"
				, __func__);
		dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] %s : SCFW Version : %d.%d.%d\n",
				__func__,
				sc_fw_handle->version.major,
				sc_fw_handle->version.minor,
				sc_fw_handle->version.patch);
		return -EINVAL;
	}
#endif

    for_each_child_of_node(node, np) {
	prop = of_find_property(np, "gpio-controller", NULL);
	if(prop != NULL) {
	    ++port->gpio_num_gr;
	}
    }


    port->irq_num = of_irq_count(node);
    if (port->irq_num < 0)
	return port->irq_num;

    pr_debug("[GPIO][DEBUG] %s: irq num : %d\n", __func__, port->irq_num);
    irq_num = port->irq_num;


#if defined(CONFIG_ARCH_TCC897X)
    port->irq_port_map =
	devm_kcalloc(&pdev->dev, irq_num, sizeof(int *), GFP_KERNEL);

    for (i = 0; i < irq_num; i++) {
	port->irq_port_map[i] = devm_kcalloc(&pdev->dev, 5, sizeof(int),
		GFP_KERNEL);
    }

    for (i = 0; i < irq_num; i++) {
	port->irq_port_map[i][0] = i;
       of_property_read_u32_index(node, "interrupts", 1 + (i * 3),
	                      &port->irq_port_map[i][4]);
	pr_debug("[GPIO][DEBUG] %s: interrupt port mux num : %d\n",
		__func__, port->irq_port_map[i][4]);
    }
#else
    port->irq_port_map =
	devm_kcalloc(&pdev->dev, irq_num / 2, sizeof(int *), GFP_KERNEL);

    for (i = 0; i < irq_num / 2; i++) {
	port->irq_port_map[i] = devm_kcalloc(&pdev->dev, 4, sizeof(int),
		GFP_KERNEL);
    }


     for (i = 0; i < irq_num / 2; i++) {
       of_property_read_u32_index(node, "interrupts", 1 + (i * 3),
               &port->irq_port_map[i][0]);
       pr_debug("[GPIO][DEBUG] %s: interrupt port mux num : %d\n",
                __func__, port->irq_port_map[i][0]);
     }
#endif

    port->irq = devm_kcalloc(&pdev->dev, irq_num, sizeof(int), GFP_KERNEL);

    for (i = 0; i < irq_num; i++) {
	port->irq[i] = of_irq_get(node, i);
	if (port->irq[i] < 0)
	    return port->irq[i];
    }


    port->gpio_gr = kcalloc(port->gpio_num_gr, sizeof(struct tcc_gpio_group),
            GFP_KERNEL);

    gpio_gr = port->gpio_gr;


    for_each_child_of_node(node, np) {

	ret = of_property_read_string(np, "label", &gpio_gr->name);
	if(ret) {
	    return ret;
	}

	of_property_read_u32_index(np, "reg-offset", 0, &gpio_gr->reg_offset);
	of_property_read_u32_index(np, "gpio-ranges", 3, &gpio_num);

	gc = &gpio_gr->gc;
	gc->of_node = np;
	gc->parent = dev;
	gc->label = gpio_gr->name;
	gc->ngpio = gpio_num;
	gc->base = of_alias_get_id(np, "gpio");

	gc->request = gpiochip_generic_request;
	gc->free = gpiochip_generic_free;
	gc->direction_input = tcc_gpio_direction_input;
	gc->direction_output = tcc_gpio_direction_output;
	gc->get_direction = tcc_get_direction;
	gc->get = tcc_gpio_get;
	gc->set = tcc_gpio_set;

	ret = gpiochip_add_data(gc, port);
	if (ret < 0) {
	    return ret;
	}

	    ret = of_property_read_u32_index(np, "source-num", 0, &source_section);

	    gpio_gr->source_section = source_section;

	    if (gpio_gr->source_section != 0xffU) {
		gpio_gr->gpio_offset_num = kcalloc(source_section, sizeof(u32),
			GFP_KERNEL);
		gpio_gr->source_offset_num = kcalloc(source_section, sizeof(u32),
			GFP_KERNEL);
		gpio_gr->source_range = kcalloc(source_section, sizeof(u32),
			GFP_KERNEL);
		for (i = 0U; i < gpio_gr->source_section; i++) {
		    ret =
			of_property_read_u32_index(np, "source-num",
				((i * 3) + 1),
				&gpio_gr->gpio_offset_num[i]);
		    if (ret > 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get source offset base\n"
			       );
			return -EINVAL;
		    }
		    ret =
			of_property_read_u32_index(np, "source-num",
				((i * 3) + 2),
				&gpio_gr->source_offset_num[i]);
		    if (ret > 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get source base\n"
			       );
			return -EINVAL;
		    }
		    ret =
			of_property_read_u32_index(np, "source-num",
				((i * 3) + 3),
				&gpio_gr->source_range[i]);
		    if (ret > 0) {
			dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get source range\n"
			       );
			return -EINVAL;
		    }
		}

	    }

	    ic = &gpio_gr->ic;
	    ic->name = gpio_gr->name;
	    ic->irq_ack = tcc_irq_ack;
	    ic->irq_set_type = tcc_gpio_irq_set_type;

	    ret = gpiochip_irqchip_add(gc, ic, 0, handle_simple_irq, IRQ_TYPE_NONE);
	    if (ret) {
		dev_err(dev, "failed to add irqchip\n");
		gpiochip_remove(gc);
		return ret;
	    }

	gpio_gr->port=port;
	gpio_gr++;
    }

    platform_set_drvdata(pdev, port);

    return 0;
}

static s32 __maybe_unused tcc_gpio_suspend(struct device *dev)
{
        return 0;
}

static s32 __maybe_unused tcc_gpio_resume(struct device *dev)
{
        struct tcc_gpio_port *port = dev_get_drvdata(dev);
	int idx_sel_reg, idx_sel_plc;
	int i;

	for(i = 0; i < port->irq_num / 2; i++) {
	    if(port->irq_port_map[i][3] == 1) {
		idx_sel_reg = port->irq_port_map[i][0] / 4;
		idx_sel_plc = port->irq_port_map[i][0] % 4;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
		(void)request_gpio_to_sc(port->raw_base + EINT_MUX + (idx_sel_reg * 4),
			(idx_sel_plc * 8), 8, port->irq_port_map[i][2]);
#else
		writel(readl(port->base + EINT_MUX + (idx_sel_reg * 4)) |
			port->irq_port_map[i][2] << (idx_sel_plc * 8),
			port->base + EINT_MUX + (idx_sel_reg * 4));
#endif
	    }
	}

        return 0;
}

static SIMPLE_DEV_PM_OPS(tcc_gpio_pm_ops,
                         tcc_gpio_suspend, tcc_gpio_resume);

static struct platform_driver telechips_gpio_driver = {
	.driver = {
		   .name = "tcc-gpio",
		   .of_match_table = telechips_gpio_dt_ids,
		   .pm = &tcc_gpio_pm_ops,
		   },
	.probe = telechips_gpio_probe,
};

static int __init tcc_gpio_drv_register(void)
{
	return platform_driver_register(&telechips_gpio_driver);
}
postcore_initcall(tcc_gpio_drv_register);


static void __exit tcc_gpio_drv_unregister(void)
{
	platform_driver_unregister(&telechips_gpio_driver);
}
module_exit(tcc_gpio_drv_unregister);

MODULE_DESCRIPTION("Telechips GPIO driver");
MODULE_LICENSE("GPL");
