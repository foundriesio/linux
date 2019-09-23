#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_gpio.h>
//#include <plat/globals.h>
//#include <linux/err.h>
//#include <linux/of_device.h>
#define ON      1
#define OFF     0

#ifndef BITSET
#define BITSET(X, MASK)            	((X) |= (unsigned int)(MASK))
#define BITSCLR(X, SMASK, CMASK)   	((X) = ((((unsigned int)(X)) | ((unsigned int)(SMASK))) & ~((unsigned int)(CMASK))) )
#define BITCSET(X, CMASK, SMASK)   	((X) = ((((unsigned int)(X)) & ~((unsigned int)(CMASK))) | ((unsigned int)(SMASK))) )
#define BITCLR(X, MASK)            	((X) &= ~((unsigned int)(MASK)) )
#define BITXOR(X, MASK)            	((X) ^= (unsigned int)(MASK) )
#define ISZERO(X, MASK)            	(!(((unsigned int)(X))&((unsigned int)(MASK))))
#define ISSET(X, MASK)          	((unsigned long)(X)&((unsigned long)(MASK)))
#endif

struct tcc_ehci_device {
	struct device	*dev;
	void __iomem	*base;		//Phy base address
	struct usb_phy 	phy;
	struct clk 		*hclk;
	struct clk 		*isol;
	int 			mux_port;

	int vbus_gpio;
	int vbus_status;
#if defined (CONFIG_TCC_BC_12)
	int irq;
	//struct task_struct *chg_det_thread;
#endif
};

struct ehci_phy_reg
{
	volatile uint32_t bcfg;
	volatile uint32_t pcfg0;
	volatile uint32_t pcfg1;
	volatile uint32_t pcfg2;
	volatile uint32_t pcfg3;
	volatile uint32_t pcfg4;
	volatile uint32_t sts;
	volatile uint32_t lcfg0;
	volatile uint32_t lcfg1;
};

/*
 * TOP Isolateion Control fuction
 */
static int tcc_ehci_phy_isolation(struct usb_phy *phy, int on_off)
{
	struct tcc_ehci_device *phy_dev = container_of(phy, struct tcc_ehci_device, phy);

	if (!phy_dev->isol)
		return -1;

	if(on_off == ON) {
		if(clk_prepare_enable(phy_dev->isol) != 0) {
			dev_err(phy_dev->dev,
			"can't do usb 2.0 phy enable\n");
		}
	} else if(on_off == OFF) {
		clk_disable_unprepare(phy_dev->isol);
	} else
		printk("bad argument\n");

	return 0;
}

/*
 * Set vbus fuction
 */
static int tcc_ehci_vbus_set(struct usb_phy *phy, int on_off)
{
	struct tcc_ehci_device *phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	int retval = 0;

	if (!phy_dev->vbus_gpio) {
		printk("ehci vbus ctrl disabled.\n");
		return -1;
	}

	retval = gpio_request(phy_dev->vbus_gpio, "vbus_gpio_phy");
	if(retval) {
		dev_err(phy->dev, "can't requeest vbus gpio\n");
		return retval;
	}

	retval = gpio_direction_output(phy_dev->vbus_gpio, on_off);
	if(retval) {
		dev_err(phy_dev->dev, "can't enable vbus (gpio ctrl err)\n");
		return retval;
	}

	gpio_free(phy_dev->vbus_gpio);

	phy_dev->vbus_status = on_off;
	
	return retval;
	
}

#if defined (CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
#define PCFG1_TXVRT_MASK   0xF
#define PCFG1_TXVRT_SHIFT  0x0

#define get_txvrt(x)	((x & PCFG1_TXVRT_MASK) >> PCFG1_TXVRT_SHIFT)

static int tcc_ehci_get_dc_level(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg	*ehci_pcfg = (struct ehci_phy_reg*)ehci_phy_dev->base;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&ehci_pcfg->pcfg1);

	//printk("cur dc level = %d\n", get_txvrt(pcfg1_val));

	return get_txvrt(pcfg1_val);
}

static int tcc_ehci_set_dc_level(struct usb_phy *phy, unsigned int level)
{
	struct tcc_ehci_device *ehci_phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg	*ehci_pcfg = (struct ehci_phy_reg*)ehci_phy_dev->base;
	uint32_t pcfg1_val;

	pcfg1_val = readl(&ehci_pcfg->pcfg1);
	BITCSET(pcfg1_val, PCFG1_TXVRT_MASK, level << PCFG1_TXVRT_SHIFT);
	writel(pcfg1_val, &ehci_pcfg->pcfg1);

	printk("cur dc level = %d\n", get_txvrt(pcfg1_val));

	return 0;
}
#endif
#if defined (CONFIG_TCC_BC_12)
static void tcc_ehci_set_chg_det(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg	*ehci_pcfg = (struct ehci_phy_reg*)ehci_phy_dev->base;

	writel(readl(&ehci_pcfg->pcfg4) | (1<<31), &ehci_pcfg->pcfg4);//clear irq
	udelay(1);
	writel(readl(&ehci_pcfg->pcfg4) & ~(1<<31), &ehci_pcfg->pcfg4);//clear irq

	writel(readl(&ehci_pcfg->pcfg2) |(1<<8) , &ehci_pcfg->pcfg2); //enable chg det
}

static irqreturn_t chg_irq(int irq, void *data)
{
    struct tcc_ehci_device *phy_dev = (struct tcc_ehci_device *)data;
    struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg*)phy_dev->base;
    uint32_t pcfg2=0;

	printk("Charging Detection!\n");
    //printk("pcfg2 = 0x%08x\n", readl(&ehci_pcfg->pcfg2));
    pcfg2 = readl(&ehci_pcfg->pcfg2);
    writel((pcfg2|(1<<9)), &ehci_pcfg->pcfg2);
    //printk("Chager Detecttion!!\n");
    mdelay(50);
    writel(readl(&ehci_pcfg->pcfg2) & ~((1<<9)|(1<<8)) , &ehci_pcfg->pcfg2);
    //printk("pcfg2 = 0x%08x\n", readl(&ehci_pcfg->pcfg2));
    //writel(readl(&ehci_pcfg->pcfg2) |(1<<8) , &ehci_pcfg->pcfg2);

    writel(readl(&ehci_pcfg->pcfg4) | (1<<31), &ehci_pcfg->pcfg4);//clear irq
    udelay(1);
    writel(readl(&ehci_pcfg->pcfg4) & ~(1<<31), &ehci_pcfg->pcfg4);//clear irq
    //writel(readl(&ehci_pcfg->pcfg2) |(1<<8) , &ehci_pcfg->pcfg2);
    return IRQ_HANDLED;
}
#endif
#define TCC_MUX_H_SWRST				(1<<4)		/* Host Controller in OTG MUX S/W Reset */
#define TCC_MUX_H_CLKMSK			(1<<3)		/* Host Controller in OTG MUX Clock Enable */
#define TCC_MUX_O_SWRST				(1<<2)		/* OTG Controller in OTG MUX S/W Reset */
#define TCC_MUX_O_CLKMSK			(1<<1)		/* OTG Controller in OTG MUX Clock Enable */
#define TCC_MUX_OPSEL				(1<<0)		/* OTG MUX Controller Select */
#define TCC_MUX_O_SELECT			(TCC_MUX_O_SWRST|TCC_MUX_O_CLKMSK)
#define TCC_MUX_H_SELECT			(TCC_MUX_H_SWRST|TCC_MUX_H_CLKMSK)

#define MUX_MODE_HOST		0
#define MUX_MODE_DEVICE		1
	
int tcc_ehci_phy_init(struct usb_phy *phy)
{
	struct tcc_ehci_device *ehci_phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg	*ehci_pcfg = (struct ehci_phy_reg*)ehci_phy_dev->base;
	uint32_t mux_cfg_val;
	int i;

	//printk("%s:ehci_pcfg=%p\n ", __func__, ehci_pcfg);

	if (ehci_phy_dev->mux_port) {
		mux_cfg_val = readl(phy->otg->mux_cfg_addr); /* get otg control cfg register */
		BITCSET(mux_cfg_val, TCC_MUX_OPSEL, TCC_MUX_H_SELECT);
		writel(mux_cfg_val, phy->otg->mux_cfg_addr);
	}

	if(!__clk_is_enabled(ehci_phy_dev->hclk))
		clk_enable(ehci_phy_dev->hclk);

	clk_reset(ehci_phy_dev->hclk, 1);

	// Reset PHY Registers
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
	writel(0x83000025, &ehci_pcfg->pcfg0);
	writel(0xE31C243A, &ehci_pcfg->pcfg1);
	#else
	writel(0x03000115, &ehci_pcfg->pcfg0);
	writel(0x0334D175, &ehci_pcfg->pcfg1);
	#endif
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
	writel(0x00000000, &ehci_pcfg->pcfg2);
	#else
	writel(0x00000004, &ehci_pcfg->pcfg2);
	#endif
	writel(0x00000000, &ehci_pcfg->pcfg3);
	writel(0x00000000, &ehci_pcfg->pcfg4);
	writel(0x30048020, &ehci_pcfg->lcfg0);

	// Set the POR
	writel(readl(&ehci_pcfg->pcfg0) | (1<<31), &ehci_pcfg->pcfg0);
	// Set the Core Reset
	writel(readl(&ehci_pcfg->lcfg0) & 0xCFFFFFFF, &ehci_pcfg->lcfg0);

	#if defined(CONFIG_ARCH_TCC803X)	
	// Clear SIDDQ
	writel(readl(&ehci_pcfg->pcfg0) & ~(1<<24), &ehci_pcfg->pcfg0);
	#endif

	#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	// Wait 30 usec
	udelay(30);
	#else
	// Wait 10 usec
	udelay(10);
	#endif

	// Release POR
	writel(readl(&ehci_pcfg->pcfg0) & ~(1<<31), &ehci_pcfg->pcfg0);
	#ifndef CONFIG_ARCH_TCC803X
	// Clear SIDDQ
	writel(readl(&ehci_pcfg->pcfg0) & ~(1<<24), &ehci_pcfg->pcfg0); //moved it before Release POR for stability
	#endif
	// Set Phyvalid en
	writel(readl(&ehci_pcfg->pcfg4) | (1<<30), &ehci_pcfg->pcfg4);
	// Set DP/DM (pull down)
	writel(readl(&ehci_pcfg->pcfg4) | 0x1400, &ehci_pcfg->pcfg4);

	clk_reset(ehci_phy_dev->hclk, 0);

	// Wait Phy Valid Interrupt
	i = 0;
	while (i < 10000) {
	#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		if ((readl(&ehci_pcfg->pcfg4) & (1<<27))) break;
	#else
		if ((readl(&ehci_pcfg->pcfg0) & (1<<21))) break;
	#endif
		i++;
		udelay(5);
	}
	printk("EHCI PHY valid check %s\x1b[0m\n",i>=9999?"fail!":"pass.");

	// Release Core Reset
	writel(readl(&ehci_pcfg->lcfg0) | 0x30000000, &ehci_pcfg->lcfg0);

#ifdef CONFIG_USB_HS_DC_VOLTAGE_LEVEL		/* 017.03.03 */
	phy->set_dc_voltage_level(phy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif /* CONFIG_USB_HS_DC_VOLTAGE_LEVEL */

#if defined (CONFIG_TCC_BC_12)	
	writel(readl(&ehci_pcfg->pcfg4) | (1<<31), &ehci_pcfg->pcfg4);//clear irq
	printk("%s : Not Mux host\n", __func__);
	writel(readl(&ehci_pcfg->pcfg4) & ~(1<<30), &ehci_pcfg->pcfg4);//Disable VBUS Detect
	
	writel(readl(&ehci_pcfg->pcfg4) & ~(1<<31), &ehci_pcfg->pcfg4);//clear irq
	udelay(1);
	writel(readl(&ehci_pcfg->pcfg2) | ((1<<8)|(1<<10)), &ehci_pcfg->pcfg2);
	udelay(1);
	writel(readl(&ehci_pcfg->pcfg4 ) | (1<<28), &ehci_pcfg->pcfg4);//enable CHG_DET interrupt

	enable_irq(ehci_phy_dev->irq);
#endif

	return 0;
}

#define USB20_PCFG0_PHY_POR		(1<<31)
#define USB20_PCFG0_PHY_SIDDQ	(1<<24)
static void tcc_ehci_phy_shutdown(struct usb_phy *phy)
{
	phy->set_phy_state(phy, 0);
}
static int tcc_ehci_phy_state_set(struct usb_phy *phy, int on_off)
{
	struct tcc_ehci_device *ehci_phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg*)ehci_phy_dev->base;

	if (on_off == ON) {
		BITCLR(ehci_pcfg->pcfg0, (USB20_PCFG0_PHY_POR | USB20_PCFG0_PHY_SIDDQ));
		printk("EHCI PHY start\n");
	} else if (on_off == OFF) {
		BITSET(ehci_pcfg->pcfg0, (USB20_PCFG0_PHY_POR | USB20_PCFG0_PHY_SIDDQ));
		printk("EHCI PHY stop\n");
	}
	printk("EHCI PHY pcfg0 : %08X\n", ehci_pcfg->pcfg0);
	return 0;
}

static void tcc_ehci_phy_mux_sel(struct usb_phy *phy, int is_mux)
{
	struct tcc_ehci_device *ehci_phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct ehci_phy_reg *ehci_pcfg = (struct ehci_phy_reg*)ehci_phy_dev->base;
	uint32_t mux_cfg_val;

	if (ehci_phy_dev->mux_port) {
		mux_cfg_val = readl(phy->otg->mux_cfg_addr);
		if (is_mux) {
			BITCSET(mux_cfg_val, TCC_MUX_OPSEL, TCC_MUX_H_SELECT);
			writel(mux_cfg_val, phy->otg->mux_cfg_addr);
		} else {
			BITSET(mux_cfg_val, TCC_MUX_OPSEL|TCC_MUX_O_SELECT|TCC_MUX_H_SELECT);
			writel(mux_cfg_val, phy->otg->mux_cfg_addr);
		}
	}
}

#ifdef CONFIG_ARCH_TCC803X
static int tcc_ehci_phy_set_vbus_resource(struct usb_phy *phy)
{
	struct tcc_ehci_device *phy_dev = container_of(phy, struct tcc_ehci_device, phy);
	struct device *dev = phy_dev->dev;
	//===============================================
	// Check vbus enable pin	
	//===============================================
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0)) {
		phy_dev->vbus_gpio = of_get_named_gpio(dev->of_node, "vbus-gpio", 0);
		if(!gpio_is_valid(phy_dev->vbus_gpio)) {
			dev_err(dev, "can't find dev of node: vbus gpio\n");
			return -ENODEV;
		}
	} else {
		phy_dev->vbus_gpio = 0;	// can not control vbus
	}

	return 0;
}
#endif
static int tcc_ehci_create_phy(struct device *dev, struct tcc_ehci_device *phy_dev)
{
	phy_dev->phy.otg = devm_kzalloc(dev, sizeof(*phy_dev->phy.otg),	GFP_KERNEL);
	if (!phy_dev->phy.otg)
		return -ENOMEM;

	phy_dev->mux_port = of_find_property(dev->of_node, "mux_port", 0)?1:0;
#ifndef CONFIG_ARCH_TCC803X
	//===============================================
	// Check vbus enable pin	
	//===============================================
	if (of_find_property(dev->of_node, "vbus-ctrl-able", 0)) {
		phy_dev->vbus_gpio = of_get_named_gpio(dev->of_node, "vbus-gpio", 0);
		if(!gpio_is_valid(phy_dev->vbus_gpio)) {
			dev_err(dev, "can't find dev of node: vbus gpio\n");
			return -ENODEV;
		}
	} else {
		phy_dev->vbus_gpio = 0;	// can not control vbus
	}
#else
	phy_dev->vbus_gpio = 1;
#endif

	// HCLK
	phy_dev->hclk = of_clk_get(dev->of_node, 0);
	if (IS_ERR(phy_dev->hclk))
		phy_dev->hclk = NULL;

	// isol
	phy_dev->isol = of_clk_get(dev->of_node, 1);
	if (IS_ERR(phy_dev->isol))
		phy_dev->isol = NULL;
	
	phy_dev->dev				= dev;

	phy_dev->phy.dev			= phy_dev->dev;
	phy_dev->phy.label			= "tcc_ehci_phy";
	phy_dev->phy.state			= OTG_STATE_UNDEFINED;
	phy_dev->phy.type			= USB_PHY_TYPE_USB2;
	phy_dev->phy.init 			= tcc_ehci_phy_init;
	phy_dev->phy.set_phy_isol	= tcc_ehci_phy_isolation;
	phy_dev->phy.set_phy_state 	= tcc_ehci_phy_state_set;
	phy_dev->phy.set_phy_mux_sel = tcc_ehci_phy_mux_sel;
#ifdef CONFIG_ARCH_TCC803X
	phy_dev->phy.set_vbus_resource	= tcc_ehci_phy_set_vbus_resource;
#endif
	if (phy_dev->vbus_gpio)
		phy_dev->phy.set_vbus		= tcc_ehci_vbus_set;

#ifdef CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT		/* 017.02.24 */
	phy_dev->phy.get_dc_voltage_level = tcc_ehci_get_dc_level;
	phy_dev->phy.set_dc_voltage_level = tcc_ehci_set_dc_level;
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */
#if defined (CONFIG_TCC_BC_12)
	phy_dev->phy.set_chg_det = tcc_ehci_set_chg_det;	
#endif

	phy_dev->phy.otg->usb_phy		= &phy_dev->phy;

	return 0;
}

static int tcc_ehci_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_ehci_device *phy_dev;
	int retval;

#if defined (CONFIG_TCC_BC_12)
	int irq, ret=0;
#endif

	printk("%s:%s\n",pdev->dev.kobj.name, __func__);
	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	retval = tcc_ehci_create_phy(dev, phy_dev);
	if (retval)
		return retval;

#if defined (CONFIG_TCC_BC_12)
	if (1) {//phy_dev->mux_port) {
		//printk("%s : MUX Host\n", __func__);
		irq = platform_get_irq(pdev, 0);
		if (irq <= 0) {
			dev_err(&pdev->dev,
					"Found HC with no IRQ. Check %s setup!\n",
					dev_name(&pdev->dev));
			retval = -ENODEV;
		}
		else
		{
			printk("%s: irq=%d\n", __func__, irq);
			phy_dev->irq = irq;
		}
	}
#endif

	if (!request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end - pdev->resource[0].start + 1,
				"ehci_phy")) {
		dev_dbg(&pdev->dev, "error reserving mapped memory\n");
		retval = -EFAULT;
	}
	phy_dev->base = (void __iomem*)ioremap_nocache((resource_size_t)pdev->resource[0].start,
				 pdev->resource[0].end - pdev->resource[0].start+1);

	phy_dev->phy.base = phy_dev->base;
#if defined (CONFIG_TCC_BC_12)
	if (1) {//phy_dev->mux_port) {
		ret = devm_request_irq(&pdev->dev, phy_dev->irq, chg_irq, IRQF_SHARED, pdev->dev.kobj.name, phy_dev);
		if (ret)
			dev_err(&pdev->dev, "request irq failed\n");

		disable_irq(phy_dev->irq);
		/*
		phy_dev->chg_det_thread = kthread_run(chg_det_monitor_thread, (void*)phy_dev, "chg_det_monitor");
		if (IS_ERR(phy_dev->chg_det_thread)) {
			printk("\x1b[1;33m[%s:%d]\x1b[0m thread error\n", __func__, __LINE__);
		}
		*/
	}
#endif
	platform_set_drvdata(pdev, phy_dev);

	retval = usb_add_phy_dev(&phy_dev->phy);
	if (retval) {
		dev_err(&pdev->dev, "usb_add_phy failed\n");
		return retval;
	}

	return retval;	
}

static int tcc_ehci_phy_remove(struct platform_device *pdev)
{
	struct tcc_ehci_device *phy_dev = platform_get_drvdata(pdev);;

	usb_remove_phy(&phy_dev->phy);

	return 0;
}

static const struct of_device_id tcc_ehci_phy_match[] = {
	{ .compatible = "telechips,tcc_ehci_phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_ehci_phy_match);

static struct platform_driver tcc_ehci_phy_driver = {
	.probe = tcc_ehci_phy_probe,
	.remove = tcc_ehci_phy_remove,
	.driver = {
		.name = "ehci_phy",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_ehci_phy_match),
#endif
	},
};

static int __init tcc_ehci_phy_drv_init(void)
{
	int retval = 0;

	retval = platform_driver_register(&tcc_ehci_phy_driver);
	if (retval < 0)
		printk(KERN_ERR "%s retval=%d\n", __func__, retval);

	return retval;
}
core_initcall(tcc_ehci_phy_drv_init);

static void __exit tcc_ehci_phy_cleanup(void)
{
	platform_driver_unregister(&tcc_ehci_phy_driver);
}
module_exit(tcc_ehci_phy_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TCC USB transceiver driver");
