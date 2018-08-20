#include <linux/platform_device.h>
#include <asm/smp.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <mach/iomap.h>
#include <mach/structures_cm4.h>
#include <mach/tca_mailbox.h>

#define MBOX_BASE       0xF000F800
#define MBOX_TX_BASE    0xF000F900

static volatile tca_mbox_handler_t pHandler = NULL;
static volatile void *pBaseRX;
static volatile void *pBaseTX;
static struct clk *pCPUClk = NULL;
static int iGpioNum = 0;
static int* pGpio;

void mbox_interrupt(void)
{
	volatile PMAILBOX pMailBox = (volatile PMAILBOX) pBaseRX;
	unsigned int nReg[8];

	if (pHandler == NULL)
		return;

	if (pMailBox->uMBOX_CTL_017.nREG & 0x1)
		return;

	nReg[0] = pMailBox->uMBOX_RX0.nREG;
	nReg[1] = pMailBox->uMBOX_RX1.nREG;
	nReg[2] = pMailBox->uMBOX_RX2.nREG;
	nReg[3] = pMailBox->uMBOX_RX3.nREG;
	nReg[4] = pMailBox->uMBOX_RX4.nREG;
	nReg[5] = pMailBox->uMBOX_RX5.nREG;
	nReg[6] = pMailBox->uMBOX_RX6.nREG;
	nReg[7] = pMailBox->uMBOX_RX7.nREG;

	pMailBox->uMBOX_CTL_017.nREG |= 0x1; // Set MEMP

	pHandler(nReg);
}

#if 0
static int tca_cpu_power_up(void)
{
	volatile void __iomem *pPMUBase = (void __iomem *)io_p2v(TCC_PA_PMU);
	volatile void __iomem *pMEMCfgBase = (void __iomem *)io_p2v(TCC_PA_MEMBUSCFG);
	u32 value = 0;
	int i;

	// CA7SP Clock Enable
	pCPUClk = clk_get(NULL, "cpu1");
	if (IS_ERR(pCPUClk))
	{
		printk("ca7sp clock error\n");
		pCPUClk = NULL;
	}
	else
	{
		for (i = 0; i < iGpioNum; i++)
		{
			gpio_set_value(pGpio[i], 1);
		}

		clk_set_rate(pCPUClk, 800*1000*1000);
		clk_enable(pCPUClk);
	}

	value = readl_relaxed(pPMUBase + 0x48);

	// CPU1_PWRCTRL.SC_CL = 0
	value &= ~(1U << 1);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.SCALL_CL = 0
	value &= ~(1U << 2);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.ISO_CL = 0
	value &= ~(1U << 0);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.CLKOFF = 0
	value &= ~(1U << 16);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.CLKOFF = 1
	value |= (1U << 16);
	writel_relaxed(value, pPMUBase + 0x48);

	// PMU_SYSRST.CA7SP_CL = 1
	writel_relaxed(readl_relaxed(pPMUBase + 0x10) | (1U << 22), pPMUBase + 0x10);

	// CPU1_PWRCTRL.DRHOLD = 0
	value &= ~(1U << 14);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.CLKOFF = 0
	value &= ~(1U << 16);
	writel_relaxed(value, pPMUBase + 0x48);

	// MEMBUS Async.Bridge Reset De-assert
	writel_relaxed(readl_relaxed(pMEMCfgBase + 0x4) | (1U << 14), pMEMCfgBase + 0x4);

	return 0;
}

static int tca_cpu_power_down(void)
{
	volatile void __iomem *pCPUBase = (void __iomem *)io_p2v(TCC_PA_CPU_BUS);
	volatile void __iomem *pPMUBase = (void __iomem *)io_p2v(TCC_PA_PMU);
	volatile void __iomem *pMEMCfgBase = (void __iomem *)io_p2v(TCC_PA_MEMBUSCFG);
	u32 value = 0;
	int i;
	unsigned long start;

	// CPU STOP(7)
	writel_relaxed(0x00100007, (void *)io_p2v(0x77101f00/*TCC_PA_GIC_DIST + GIC_DIST_SOFTINT*/));

	// STANDBYWFE
	start = jiffies;
	while (1)
	{
		value = readl_relaxed(pCPUBase + 0x3C);

		if (value & 0x2)	return 0;

		if (value & 0x20)	break;

		if (msecs_to_jiffies(jiffies - start) > 200)
		{
			printk("Can not stop CA7\n");
			break;
		}
	}

	// MEMBUS Async.Bridge Reset Assert
	writel_relaxed(readl_relaxed(pMEMCfgBase + 0x4) & ~(1U << 14), pMEMCfgBase + 0x4);

	value = readl_relaxed(pPMUBase + 0x48);

	// CPU1_PWRCTRL.CLKOFF = 1
	value |= (1U << 16);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.DRHOLD = 1
	value |= (1U << 14);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.ISO_CL = 1
	value |= (1U << 0);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.SCALL_CL = 1
	value |= (1U << 2);
	writel_relaxed(value, pPMUBase + 0x48);

	// CPU1_PWRCTRL.SC_CL = 1
	value |= (1U << 1);
	writel_relaxed(value, pPMUBase + 0x48);

	// PMU_SYSRST.CA7SP_CL = 0
	writel_relaxed(readl_relaxed(pPMUBase + 0x10) & ~(1U << 22), pPMUBase + 0x10);

	// CA7SP Clock Disable
	if (pCPUClk)
	{
		clk_disable(pCPUClk);
		clk_put(pCPUClk);
		pCPUClk = NULL;

		for (i = 0; i < iGpioNum; i++)
		{
			gpio_set_value(pGpio[i], 0);
		}
	}

	return 0;
}
#endif

int tca_mailbox_init(tca_mbox_handler_t handler)
{
	volatile PMAILBOX pMailBox;

	pBaseRX  = ioremap_nocache(MBOX_BASE, 0x48);
	pBaseTX  = ioremap_nocache(MBOX_TX_BASE, 0x48);

	pMailBox = (volatile PMAILBOX) pBaseRX;

	// FLUSH: 6bit, OEN: 5bit, IEN: 4bit, LEVEL: 1~0bit
	pMailBox->uMBOX_CTL_016.nREG = (1 << 6) | (1 << 4) | (0x3);
	pMailBox->uMBOX_CTL_017.nREG = 0x10001; // Set MEMP

	pHandler = handler;

	//tca_cpu_power_up();

	return 0;
}
EXPORT_SYMBOL(tca_mailbox_init);

void tca_mailbox_deinit(void)
{
	//tca_cpu_power_down();

	pHandler = NULL;

	iounmap(pBaseRX);
	iounmap(pBaseTX);
}
EXPORT_SYMBOL(tca_mailbox_deinit);

int tca_mailbox_send(unsigned int *pMsg, int iWaitTime)
{
	volatile PMAILBOX pMailBox = (volatile PMAILBOX) pBaseTX;

	if ((pMailBox->uMBOX_CTL_017.nREG & 0x1) == 0)
		pMailBox->uMBOX_CTL_016.nREG |= (1 << 6);

	pMailBox->uMBOX_RX0.nREG = pMsg[0];
	pMailBox->uMBOX_RX1.nREG = pMsg[1];
	pMailBox->uMBOX_RX2.nREG = pMsg[2];
	pMailBox->uMBOX_RX3.nREG = pMsg[3];
	pMailBox->uMBOX_RX4.nREG = pMsg[4];
	pMailBox->uMBOX_RX5.nREG = pMsg[5];
	pMailBox->uMBOX_RX6.nREG = pMsg[6];
	pMailBox->uMBOX_RX7.nREG = pMsg[7];
	pMailBox->uMBOX_CTL_017.nREG &= ~(1 << 0);

	arch_send_mbox_transfer(4);

	while ((pMailBox->uMBOX_CTL_017.nREG & 0x1) == 0 && iWaitTime > 0)
		iWaitTime--;

	return iWaitTime;
}
EXPORT_SYMBOL(tca_mailbox_send);

static int tca_mailbox_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int i;

	iGpioNum = of_gpio_count(np);
	pGpio = devm_kzalloc(&pdev->dev, sizeof(int) * iGpioNum, GFP_KERNEL);
	for (i = 0; i < iGpioNum; i++)
	{
		pGpio[i] = of_get_named_gpio(np, "gpios", i);
	}

	printk("%s : GPIO NUM(%d)\n", __func__, iGpioNum);

	return 0;
}

static int tca_mailbox_remove(struct platform_device *pdev)
{
	iGpioNum = 0;
	kfree(pGpio);
	return 0;
}

static const struct of_device_id tca_mailbox_of_match[] = {
	{ .compatible = "cpu1,mailbox", },
	{ },
};
MODULE_DEVICE_TABLE(of, tca_mailbox_of_match);

static struct platform_driver tca_mailbox_drv = {
	.probe = tca_mailbox_probe,
	.remove = tca_mailbox_remove,
	.driver = {
		.name = "cpu1,mailbox",
		.owner = THIS_MODULE,
		.of_match_table = tca_mailbox_of_match,
	},
};

static int __init tca_mailbox_module_init(void)
{
	return platform_driver_register(&tca_mailbox_drv);
}

static void __exit tca_mailbox_module_exit(void)
{
	platform_driver_unregister(&tca_mailbox_drv);
}

module_init(tca_mailbox_module_init);
module_exit(tca_mailbox_module_exit);

