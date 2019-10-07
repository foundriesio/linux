#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include "../dwc3/core.h"
#include "../dwc3/io.h"
#include <asm/system_info.h>
//#include <plat/globals.h>
//#include <linux/err.h>
//#include <linux/of_device.h>
#define ON      1
#define OFF     0
#define PHY_RESUME 2

#ifndef BITSET
#define BITSET(X, MASK)            	((X) |= (unsigned int)(MASK))
#define BITSCLR(X, SMASK, CMASK)   	((X) = ((((unsigned int)(X)) | ((unsigned int)(SMASK))) & ~((unsigned int)(CMASK))) )
#define BITCSET(X, CMASK, SMASK)   	((X) = ((((unsigned int)(X)) & ~((unsigned int)(CMASK))) | ((unsigned int)(SMASK))) )
#define BITCLR(X, MASK)            	((X) &= ~((unsigned int)(MASK)) )
#define BITXOR(X, MASK)            	((X) ^= (unsigned int)(MASK) )
#define ISZERO(X, MASK)            	(!(((unsigned int)(X))&((unsigned int)(MASK))))
#define ISSET(X, MASK)          	((unsigned long)(X)&((unsigned long)(MASK)))
#endif

char *maximum_speed = "super";
typedef enum {
	USBPHY_MODE_RESET = 0,
	USBPHY_MODE_OTG,
	USBPHY_MODE_ON,
	USBPHY_MODE_OFF,
	USBPHY_MODE_START,
	USBPHY_MODE_STOP,
	USBPHY_MODE_DEVICE_DETACH
} USBPHY_MODE_T;

struct tcc_dwc3_device {
	struct device	*dev;
	void __iomem	*base;
	void __iomem	*h_base;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X)
	void __iomem	*ref_base;
#endif
	struct usb_phy 	phy;
	struct clk 		*phy_clk;

	int vbus_gpio;
	int vbus_status;
#if defined (CONFIG_TCC_BC_12)
	struct work_struct  dwc3_work;
	int irq;
#endif
};

typedef struct _USBPHYCFG
{
    volatile unsigned int       U30_CLKMASK;            // 0x0
    volatile unsigned int       U30_SWRESETN;           // 0x4
    volatile unsigned int       U30_PWRCTRL;            // 0x8
    volatile unsigned int       U30_OVERCRNT;           // 0xC
    volatile unsigned int       U30_PCFG0;              // 0x10  R/W    USB PHY Configuration Register0
    volatile unsigned int       U30_PCFG1;              // 0x14  R/W    USB PHY Configuration Register1
    volatile unsigned int       U30_PCFG2;              // 0x18  R/W    USB PHY Configuration Register2
    volatile unsigned int       U30_PCFG3;              // 0x1c  R/W    USB PHY Configuration Register3
    volatile unsigned int       U30_PCFG4;              // 0x20  R/W    USB PHY Configuration Register4
    volatile unsigned int       U30_PFLT;               // 0x24  R/W     USB PHY Filter Configuration Register
    volatile unsigned int       U30_PINT;               // 0x28  R/W     USB PHY Interrupt Register
    volatile unsigned int       U30_LCFG;               // 0x2C  R/W    USB 3.0 LINK Controller Configuration Register Set 0
    volatile unsigned int       U30_PCR0;               //   0x30  USB 3.0 PHY Parameter Control Register Set 0
    volatile unsigned int       U30_PCR1;               //   0x34  USB 3.0 PHY Parameter Control Register Set 1
    volatile unsigned int       U30_PCR2;               //   0x38  USB 3.0 PHY Parameter Control Register Set 2
    volatile unsigned int       U30_SWUTMI;             //   0x3C  USB 3.0 UTMI Software Control Register
} USBPHYCFG, *PUSBPHYCFG;

#if defined(CONFIG_ARCH_TCC803X)
typedef struct _USBSSPHYCFG
{
    volatile unsigned int       U30_CLKMASK;            // 0x0   USB 3.0 Clock Mask Register
    volatile unsigned int       U30_SWRESETN;           // 0x4   USB 3.0 S/W Reset Register
    volatile unsigned int       U30_PWRCTRL;            // 0x8   USB 3.0 Power Control Register
    volatile unsigned int       U30_OVERCRNT;           // 0xC   USB 3.0 Overcurrent Selection Register
    volatile unsigned int       U30_PCFG0;              // 0x10  USB 3.0 PHY Configuration Register0
    volatile unsigned int       U30_PCFG1;              // 0x14  USB 3.0 PHY Configuration Register1
    volatile unsigned int       U30_PCFG2;              // 0x18  USB 3.0 PHY Configuration Register2
    volatile unsigned int       U30_PCFG3;              // 0x1c  USB 3.0 PHY Configuration Register3
    volatile unsigned int       U30_PCFG4;              // 0x20  USB 3.0 PHY Configuration Register4
    volatile unsigned int       U30_PCFG5;              // 0x24  USB 3.0 PHY Configuration Register5
    volatile unsigned int       U30_PCFG6;              // 0x28  USB 3.0 PHY Configuration Register6
    volatile unsigned int       U30_PCFG7;              // 0x2C  USB 3.0 PHY Configuration Register7
    volatile unsigned int       U30_PCFG8;              // 0x30  USB 3.0 PHY Configuration Register8
    volatile unsigned int       U30_PCFG9;              // 0x34  USB 3.0 PHY Configuration Register9
    volatile unsigned int       U30_PCFG10;             // 0x38  USB 3.0 PHY Configuration Register10
    volatile unsigned int       U30_PCFG11;             // 0x3C  USB 3.0 PHY Configuration Register11
    volatile unsigned int       U30_PCFG12;             // 0x40  USB 3.0 PHY Configuration Register12
    volatile unsigned int       U30_PCFG13;             // 0x44  USB 3.0 PHY Configuration Register13
    volatile unsigned int       U30_PCFG14;             // 0x48  USB 3.0 PHY Configuration Register14
    volatile unsigned int       U30_PCFG15;             // 0x4C  USB 3.0 PHY Configuration Register15
    volatile unsigned int       reserved0[10];          // 0x50~74
    volatile unsigned int       U30_PFLT;               // 0x78  USB 3.0 PHY Filter Configuration Register
    volatile unsigned int       U30_PINT;               // 0x7C  USB 3.0 PHY Interrupt Register
    volatile unsigned int       U30_LCFG;               // 0x80  USB 3.0 LINK Controller Configuration Register Set 0
    volatile unsigned int       U30_PCR0;               // 0x84  USB 3.0 PHY Parameter Control Register Set 0
    volatile unsigned int       U30_PCR1;               // 0x88  USB 3.0 PHY Parameter Control Register Set 1
    volatile unsigned int       U30_PCR2;               // 0x8C  USB 3.0 PHY Parameter Control Register Set 2
    volatile unsigned int       U30_SWUTMI;             // 0x90  USB 3.0 PHY Software Control Register
    volatile unsigned int       U30_DBG0;               // 0x94  USB 3.0 LINK Controller debug Signal 0
    volatile unsigned int       U30_DBG1;               // 0x98  USB 3.0 LINK Controller debug Signal 0
    volatile unsigned int       reserved[1];            // 0x9C
    volatile unsigned int       FPHY_PCFG0;             // 0xA0  USB 3.0 High-speed PHY Configuration Register0
    volatile unsigned int       FPHY_PCFG1;             // 0xA4  USB 3.0 High-speed PHY Configuration Register1
    volatile unsigned int       FPHY_PCFG2;             // 0xA8  USB 3.0 High-speed PHY Configuration Register2
    volatile unsigned int       FPHY_PCFG3;             // 0xAc  USB 3.0 High-speed PHY Configuration Register3
    volatile unsigned int       FPHY_PCFG4;             // 0xB0  USB 3.0 High-speed PHY Configuration Register4
    volatile unsigned int       FPHY_LCFG0;             // 0xB4  USB 3.0 High-speed LINK Controller Configuration Register0
    volatile unsigned int       FPHY_LCFG1;             // 0xB8  USB 3.0 High-speed LINK Controller Configuration Register1
} USBSSPHYCFG, *PUSBSSPHYCFG;
#endif
void __iomem* dwc3_get_base(struct usb_phy *phy)
{
	struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	
	return dwc3_phy_dev->base;
}

void dwc3_bit_set_phy(void __iomem *base, u32 offset, u32 value)
{
	unsigned int uTmp;

	uTmp = readl(base + offset - DWC3_GLOBALS_REGS_START);
	writel((uTmp|value), base + offset - DWC3_GLOBALS_REGS_START);
}

void dwc3_bit_clear_phy(void __iomem *base, u32 offset, u32 value)
{
	unsigned int uTmp;

	uTmp = readl(base + offset - DWC3_GLOBALS_REGS_START);
	writel((uTmp&(~value)), base + offset - DWC3_GLOBALS_REGS_START);
}

static int tcc_dwc3_vbus_set(struct usb_phy *phy, int on_off)
{
	struct tcc_dwc3_device *phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	int retval = 0;

	if (!phy_dev->vbus_gpio) {
		printk("dwc3 vbus ctrl disable.\n");
		return -1;
	}

	retval = gpio_request(phy_dev->vbus_gpio, "vbus_gpio_phy");
	if(retval) {
		dev_err(phy->dev, "can't request vbus gpio\n");
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

#if defined (CONFIG_TCC_BC_12)
#if defined (CONFIG_ARCH_TCC803x)
static void tcc_dwc3_set_chg_det(struct usb_phy *phy)
{
	struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	PUSBSSPHYCFG USBPHYCFG = (PUSBSSPHYCFG)dwc3_phy_dev->base;
	printk("Charging Detection!!\n");
	//printk("%s : pcfg2 = 0x%x\n", __func__, readl(&USBPHYCFG->FPHY_PCFG2));
	//printk("%s : pcfg4 = 0x%x\n", __func__, readl(&USBPHYCFG->FPHY_PCFG4));
	
	//writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
	//udelay(1);
	//writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
	
	writel(readl(&USBPHYCFG->FPHY_PCFG2) |(1<<8) , &USBPHYCFG->FPHY_PCFG2); //enable chg det
}

static void tcc_dwc3_set_cdp(struct work_struct *data)
{
    struct tcc_dwc3_device *dwc3_phy_dev = container_of(data, struct tcc_dwc3_device, dwc3_work);
    PUSBSSPHYCFG USBPHYCFG = (PUSBSSPHYCFG)dwc3_phy_dev->base;
    uint32_t pcfg2=0;
    int32_t count=3;

    while(count > 0)
    {
        if((readl(&USBPHYCFG->FPHY_PCFG2) & (1<<22)) != 0)
        {
           // printk("Chager Detecttion!!\n");
            //printk("pcfg2 = 0x%08x\n", readl(&USBPHYCFG->FPHY_PCFG2));
            break;
        }
        mdelay(1);
        count--;
    }

    if(count == 0)
    {
        printk("%s : failed to detect charging!!\n", __func__);
    }
    else
    {
    	pcfg2 = readl(&USBPHYCFG->FPHY_PCFG2);
        writel((pcfg2|(1<<9)), &USBPHYCFG->FPHY_PCFG2);
        mdelay(100);
        writel(readl(&USBPHYCFG->FPHY_PCFG2) & ~((1<<9)|(1<<8)) , &USBPHYCFG->FPHY_PCFG2);
        //printk("pcfg2 = 0x%08x\n", readl(&USBPHYCFG->FPHY_PCFG2));
    }

    writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
    udelay(10);
    writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
	printk("%s:Enable chg det!!!\n", __func__);
	//writel(readl(&USBPHYCFG->FPHY_PCFG2) |(1<<8) , &USBPHYCFG->FPHY_PCFG2); //enable chg det
}

static irqreturn_t tcc_dwc3_chg_irq(int irq, void *data)
{
    struct tcc_dwc3_device *dwc3_phy_dev = (struct tcc_dwc3_device *)data;
    PUSBSSPHYCFG USBPHYCFG = (PUSBSSPHYCFG)dwc3_phy_dev->base;

	printk("%s : CHGDET\n", __func__);
	writel(readl(&USBPHYCFG->U30_PINT) | (1<<22), &USBPHYCFG->U30_PINT);//clear irq
	udelay(1);
	writel(readl(&USBPHYCFG->U30_PINT) & ~(1<<22), &USBPHYCFG->U30_PINT);//clear irq
	//printk("PINT = 0x%08x\n", readl(&USBPHYCFG->U30_PINT));
	schedule_work(&dwc3_phy_dev->dwc3_work);

    return IRQ_HANDLED;
}
#else
static void tcc_dwc3_set_chg_det(struct usb_phy *phy)
{
    struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
    PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;
    printk("Charging Detection!!\n");
    //printk("%s : pcfg2 = 0x%x\n", __func__, readl(&USBPHYCFG->FPHY_PCFG2));
    //printk("%s : pcfg4 = 0x%x\n", __func__, readl(&USBPHYCFG->FPHY_PCFG4));

    //writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
    //udelay(1);
    //writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq

    writel(readl(&USBPHYCFG->U30_PCFG1) |(1<<17) , &USBPHYCFG->U30_PCFG1); //enable chg det
}

static void tcc_dwc3_set_cdp(struct work_struct *data)
{
    struct tcc_dwc3_device *dwc3_phy_dev = container_of(data, struct tcc_dwc3_device, dwc3_work);
    PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;
    uint32_t pcfg=0;
    int32_t count=3;

    while(count > 0)
    {
        if((readl(&USBPHYCFG->U30_PCFG1) & (1<<20)) != 0)
        {
            printk("Chager Detecttion!!\n");
            //printk("pcfg2 = 0x%08x\n", readl(&USBPHYCFG->FPHY_PCFG2));
            break;
        }
        mdelay(1);
        count--;
    }

    if(count == 0)
    {
        printk("%s : failed to detect charging!!\n", __func__);
    }
    else
    {
        pcfg = readl(&USBPHYCFG->U30_PCFG1);
        writel((pcfg|(1<<18)), &USBPHYCFG->U30_PCFG1);
        mdelay(100);
        writel(readl(&USBPHYCFG->U30_PCFG1) & ~((1<<18)|(1<<17)) , &USBPHYCFG->U30_PCFG1);
        //printk("pcfg2 = 0x%08x\n", readl(&USBPHYCFG->FPHY_PCFG2));
    }

    //writel(readl(&USBPHYCFG->U30_PCFG4) | (1<<31), &USBPHYCFG->U30_PCFG4);//clear irq
    //udelay(10);
    //writel(readl(&USBPHYCFG->U30_PCFG4) & ~(1<<31), &USBPHYCFG->U30_PCFG4);//clear irq
    printk("%s:Enable chg det!!!\n", __func__);
    //writel(readl(&USBPHYCFG->FPHY_PCFG2) |(1<<8) , &USBPHYCFG->FPHY_PCFG2); //enable chg det
}

static irqreturn_t tcc_dwc3_chg_irq(int irq, void *data)
{
    struct tcc_dwc3_device *dwc3_phy_dev = (struct tcc_dwc3_device *)data;
    PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;

    printk("%s : CHGDET\n", __func__);
    writel(readl(&USBPHYCFG->U30_PINT) | (1<<22), &USBPHYCFG->U30_PINT);//clear irq
    udelay(1);
    writel(readl(&USBPHYCFG->U30_PINT) & ~(1<<22), &USBPHYCFG->U30_PINT);//clear irq
    //printk("PINT = 0x%08x\n", readl(&USBPHYCFG->U30_PINT));
    schedule_work(&dwc3_phy_dev->dwc3_work);

    return IRQ_HANDLED;
}
#endif
#endif

#ifdef DWC3_SQ_TEST_MODE		/* 016.08.26 */
unsigned int dwc3_tcc_read_u30phy_reg(struct usb_phy *phy, unsigned int address)
{
    struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;
	unsigned int tmp;
    unsigned int read_data;

    // address capture phase
    USBPHYCFG->U30_PCR0 = address;// write addr
    USBPHYCFG->U30_PCR2 |= 0x1; // capture addr

    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==1) break; // check ack == 1
    }

    USBPHYCFG->U30_PCR2 &= ~0x1; // clear capture addr

    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==0) break; // check ack == 0
    }

    // read phase
    USBPHYCFG->U30_PCR2 |= 0x1<<8; // set read
    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==1) break; // check ack == 1
    }

    read_data = USBPHYCFG->U30_PCR1;    // read data

    USBPHYCFG->U30_PCR2 &= ~(0x1<<8); // clear read
    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==0) break; // check ack == 0
    }

    return(read_data);
}

void dwc3_tcc_read_u30phy_reg_all (struct usb_phy *phy) {
	unsigned int read_data;
	unsigned int i;

	for (i=0;i<0x37;i++) {
		read_data = dwc3_tcc_read_u30phy_reg(phy,i);
		printk("addr:0x%08X value:0x%08X\n",i,read_data);
	}
	
	for (i=0x1000;i<0x1030;i++) {
		read_data = dwc3_tcc_read_u30phy_reg(phy,i);
		printk("addr:0x%08X value:0x%08X\n",i,read_data);
	}
}

unsigned int dwc3_tcc_write_u30phy_reg(struct usb_phy *phy, unsigned int address, 
	unsigned int write_data)
{
	struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;
	unsigned int tmp;
	unsigned int read_data;

	// address capture phase
	USBPHYCFG->U30_PCR0 = address; // write addr
	USBPHYCFG->U30_PCR2 |= 0x1; // capture addr
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1); // clear capture addr
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}

	// write phase
	USBPHYCFG->U30_PCR0 = write_data; // write data
	USBPHYCFG->U30_PCR2 |= 0x1<<4; // set capture data
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1<<4); // clear capture data
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}
	USBPHYCFG->U30_PCR2 |= 0x1<<12; // set write
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1<<12); // clear write
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}

	// read phase
	USBPHYCFG->U30_PCR2 |= 0x1<<8; // set read
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	read_data = USBPHYCFG->U30_PCR1; // read data
	USBPHYCFG->U30_PCR2 &= ~(0x1<<8); // clear read
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}
	if (read_data==write_data) return(1); // success
	else return(0); // fail
}

//After Rev_1
#if defined(CONFIG_ARCH_TCC803X)
unsigned int dwc3_tcc_read_ss_u30phy_reg(struct usb_phy *phy, unsigned int address)
{
    struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;
	unsigned int tmp;
    unsigned int read_data;

    // address capture phase
    USBPHYCFG->U30_PCR0 = address;// write addr
    USBPHYCFG->U30_PCR2 |= 0x1; // capture addr

    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==1) break; // check ack == 1
    }

    USBPHYCFG->U30_PCR2 &= ~0x1; // clear capture addr

    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==0) break; // check ack == 0
    }

    // read phase
    USBPHYCFG->U30_PCR2 |= 0x1<<8; // set read
    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==1) break; // check ack == 1
    }

    read_data = USBPHYCFG->U30_PCR1;    // read data

    USBPHYCFG->U30_PCR2 &= ~(0x1<<8); // clear read
    while (1) {
        tmp = USBPHYCFG->U30_PCR2;
        tmp = (tmp>>16)&0x1;
        if (tmp==0) break; // check ack == 0
    }

    return(read_data);
}

void dwc3_tcc_read_ss_u30phy_reg_all (struct usb_phy *phy) {
	unsigned int read_data;
	unsigned int i;

	for (i=0;i<0x37;i++) {
		read_data = dwc3_tcc_read_ss_u30phy_reg(phy,i);
		printk("addr:0x%08X value:0x%08X\n",i,read_data);
	}
	
	for (i=0x1000;i<0x1030;i++) {
		read_data = dwc3_tcc_read_ss_u30phy_reg(phy,i);
		printk("addr:0x%08X value:0x%08X\n",i,read_data);
	}
}

unsigned int dwc3_tcc_write_ss_u30phy_reg(struct usb_phy *phy, unsigned int address, 
	unsigned int write_data)
{
	struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;
	unsigned int tmp;
	unsigned int read_data;

	// address capture phase
	USBPHYCFG->U30_PCR0 = address; // write addr
	USBPHYCFG->U30_PCR2 |= 0x1; // capture addr
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1); // clear capture addr
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}

	// write phase
	USBPHYCFG->U30_PCR0 = write_data; // write data
	USBPHYCFG->U30_PCR2 |= 0x1<<4; // set capture data
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1<<4); // clear capture data
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}
	USBPHYCFG->U30_PCR2 |= 0x1<<12; // set write
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	USBPHYCFG->U30_PCR2 &= ~(0x1<<12); // clear write
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}

	// read phase
	USBPHYCFG->U30_PCR2 |= 0x1<<8; // set read
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==1) break; // check ack == 1
	}
	read_data = USBPHYCFG->U30_PCR1; // read data
	USBPHYCFG->U30_PCR2 &= ~(0x1<<8); // clear read
	while (1) {
	  tmp = USBPHYCFG->U30_PCR2;
	  tmp = (tmp>>16)&0x1;
	  if (tmp==0) break; // check ack == 0
	}
	if (read_data==write_data) return(1); // success
	else return(0); // fail
}
#endif
#endif

static int is_suspend = 1;

void dwc3_tcc898x_swreset(PUSBPHYCFG USBPHYCFG, int on_off)
{
	if(on_off == ON)
		BITCLR(USBPHYCFG->U30_SWRESETN, Hw1);
	else if(on_off == OFF)
		BITSET(USBPHYCFG->U30_SWRESETN, Hw1);
	else
		printk("\x1b[1;31m[%s:%d]Wrong request!!\x1b[0m\n", __func__, __LINE__);
}
int dwc3_tcc_phy_ctrl_native(struct usb_phy *phy, int on_off)
{
	struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
    PUSBPHYCFG USBPHYCFG = (PUSBPHYCFG)dwc3_phy_dev->base;

	unsigned int uTmp = 0;
	int tmp_cnt;
	
	printk("%s %s\n", __func__, (on_off)?"ON":"OFF");
	if(on_off== ON && is_suspend) {
		//clk_reset(dwc3_phy_dev->hclk, 1);
		//======================================================
	    // 1.Power-on Reset
		//======================================================
	//	*(volatile unsigned long *)tcc_p2v(HwCLK_RESET0) &= ~Hw7;
	//	*(volatile unsigned long *)tcc_p2v(HwCLK_RESET0) |= Hw7;
#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(USBPHYCFG, ON);
		mdelay(1);
		dwc3_tcc898x_swreset(USBPHYCFG, OFF);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		//dwc3_tcc899x_swreset(tcc, ON);
		//mdelay(1);
		//dwc3_tcc899x_swreset(tcc, OFF);
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31), &USBPHYCFG->U30_LCFG);
		writel((readl(&USBPHYCFG->U30_PCFG0) & ~(Hw30)), &USBPHYCFG->U30_PCFG0);

		uTmp = readl(&USBPHYCFG->U30_PCFG0);
		uTmp &= ~(Hw25); // turn off SS circuits
		uTmp &= ~(Hw24); // turn on HS circuits
		writel(uTmp, &USBPHYCFG->U30_PCFG0);
		msleep(1);

		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)), &USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)), &USBPHYCFG->U30_LCFG);
		//clk_reset(dwc3_phy_dev->hclk, 0);		
		//printk("PHY = 0x%x \n", USBPHYCFG->U30_PCFG0);
		tmp_cnt = 0;
		while( tmp_cnt < 10000)
		{
			if(readl(&USBPHYCFG->U30_PCFG0) & 0x80000000)
			{
				break;
			}

			tmp_cnt++;
			udelay(5);
		}
		printk("XHCI PHY valid check %s\x1b[0m\n",tmp_cnt>=9999?"fail!":"pass.");
#endif
		//======================================================
		// Initialize all registers
		//======================================================
#if defined(CONFIG_ARCH_TCC898X)		/* 016.09.30 */
		USBPHYCFG->U30_PCFG0 	= (system_rev >= 2)?0x40306228:0x20306228;
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		writel(0x03204208, &USBPHYCFG->U30_PCFG0);
#else
		USBPHYCFG->U30_PCFG0 	= 0x20306228;
#endif /* CONFIG_ARCH_TCC898X */
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		writel(0x00000000, &USBPHYCFG->U30_PCFG1);
		writel(0x919E1A04, &USBPHYCFG->U30_PCFG2);
		#if defined(CONFIG_ARCH_TCC899X)
		writel(0x4befef05, &USBPHYCFG->U30_PCFG3); //vboost:4, de-emp:0x1f, swing:0x6f, ios_bias:05
		#else
		writel(0x4B8E7F05, &USBPHYCFG->U30_PCFG3);
		#endif
		writel(0x00200000, &USBPHYCFG->U30_PCFG4);
		writel(0x00000351, &USBPHYCFG->U30_PFLT);
		writel(0x80000000, &USBPHYCFG->U30_PINT);
		//writel(0x007E0080, &USBPHYCFG->U30_LCFG);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw27 | Hw26 | Hw19 | Hw18 | Hw17 | Hw7)), &USBPHYCFG->U30_LCFG);
	//	writel(0x00000000, &USBPHYCFG->U30_PCR0);
	//	writel(0x00000000, &USBPHYCFG->U30_PCR1);
	//	writel(0x00000000, &USBPHYCFG->U30_PCR2);
	//	writel(0x00000000, &USBPHYCFG->U30_SWUTMI);
#else
		USBPHYCFG->U30_PCFG1 	= 0x00000300;
		USBPHYCFG->U30_PCFG2 	= 0x919f94C4;
		USBPHYCFG->U30_PCFG3 	= 0x4ab07f05;
#endif

#if defined(DWC3_SQ_TEST_MODE)		/* 016.02.22 */
		//BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm << TX_DEEMPH_SHIFT);
		//BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm_device<< TX_DEEMPH_SHIFT);
		BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_MASK, dm_host<< TX_DEEMPH_SHIFT);

		BITCSET(USBPHYCFG->U30_PCFG3, TX_DEEMPH_6DB_MASK, dm_6db<< TX_DEEMPH_6DB_SHIFT);
		BITCSET(USBPHYCFG->U30_PCFG3, TX_SWING_MASK, sw << TX_SWING_SHIFT);
		printk("dwc3 tcc: PHY cfg - TX_DEEMPH 3.5dB: 0x%02x, TX_DEEMPH 6dB: 0x%02x, TX_SWING: 0x%02x\n", dm, dm_6db, sw);

		USBPHYCFG->U30_PCFG4 	= 0x00200000;
		USBPHYCFG->U30_PFLT  	= 0x00000351;
		USBPHYCFG->U30_PINT  	= 0x00000000;
		USBPHYCFG->U30_LCFG  	= 0x00C20018;
		USBPHYCFG->U30_PCR0  	= 0x00000000;
		USBPHYCFG->U30_PCR1  	= 0x00000000;
		USBPHYCFG->U30_PCR2  	= 0x00000000;
		USBPHYCFG->U30_SWUTMI	= 0x00000000;
#endif
		// todo ----------------------------------------------------------------------------------------------
		#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(USBPHYCFG, ON);

		// Link global Reset
		USBPHYCFG->U30_LCFG &= ~(Hw31); // CoreRSTN (Cold Reset), active low
		#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		//dwc3_tcc899x_swreset(tcc, ON);
		// Link global Reset
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31), &USBPHYCFG->U30_LCFG); // CoreRSTN (Cold Reset), active low
		#endif

#if defined(DWC3_SQ_TEST_MODE)		/* 016.02.22 */
		//*(volatile unsigned long *)tcc_p2v(HwU30GCTL) |= (Hw11); // Link soft reset, active high
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB3PIPECTL) |= (Hw31); // Phy SS soft reset, active high
	    //*(volatile unsigned long *)tcc_p2v(HwU30GUSB2PHYCFG) |= (Hw31); //Phy HS soft resetactive high
		dwc3_bit_set_phy(dwc3_phy_dev->h_base, DWC3_GCTL, DWC3_GCTL_CORESOFTRESET);
		dwc3_bit_set_phy(dwc3_phy_dev->h_base, DWC3_GUSB3PIPECTL(0), DWC3_GUSB3PIPECTL_PHYSOFTRST);
		dwc3_bit_set_phy(dwc3_phy_dev->h_base, DWC3_GUSB2PHYCFG(0), DWC3_GUSB2PHYCFG_PHYSOFTRST);
#endif

#if defined(CONFIG_ARCH_TCC898X)
		if (!strncmp("high", maximum_speed, 4)) {
			// USB20 Only Mode
			USBPHYCFG->U30_LCFG |= Hw28; // enable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			USBPHYCFG->U30_PCFG0 = uTmp;
		} else if (!strncmp("super", maximum_speed, 5)) {
			// USB 3.0
			USBPHYCFG->U30_LCFG &= ~(Hw28); // disable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			USBPHYCFG->U30_PCFG0 = uTmp;	
		}

		// Turn Off OTG
	//    USBPHYCFG->U30_PCFG0 |= (Hw21);

		// Release Reset Link global
		USBPHYCFG->U30_LCFG |= (Hw31);
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		if (!strncmp("high", maximum_speed, 4)) {
			// USB20 Only Mode
			writel((readl(&USBPHYCFG->U30_LCFG) | Hw28), &USBPHYCFG->U30_LCFG); // enable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else if (!strncmp("super", maximum_speed, 5)) {
			// USB 3.0
			writel((readl(&USBPHYCFG->U30_LCFG) & ~(Hw28)), &USBPHYCFG->U30_LCFG); // disable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			uTmp &= ~(Hw30); // release PHY reset
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		}
		mdelay(1);
		// Release Reset Link global
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)), &USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)), &USBPHYCFG->U30_LCFG);
#endif

#if defined(DWC3_SQ_TEST_MODE)
		//*(volatile unsigned long *)tcc_p2v(HwU30GCTL) &= ~(Hw11); //Release CoreSoftReset
		//*(volatile unsigned long *)tcc_p2v(HwU30GUSB3PIPECTL) &= ~(Hw31); //Release Phy SS soft reset
		//*(volatile unsigned long *)tcc_p2v(HwU30GUSB2PHYCFG) &= ~(Hw31); // Release Phy HS soft reset
		//*(volatile unsigned long *)tcc_p2v(HwPMUU30PHY) |= (Hw9); //Release Phy reset
		//*(volatile unsigned long *)tcc_p2v(HwU30GUSB2PHYCFG) &= ~(Hw6); // Disable Suspend
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GCTL, DWC3_GCTL_CORESOFTRESET);
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GUSB3PIPECTL(0), DWC3_GUSB3PIPECTL_PHYSOFTRST);
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GUSB2PHYCFG(0), DWC3_GUSB2PHYCFG_PHYSOFTRST);
		dwc3_tcc898x_swreset(USBPHYCFG, OFF);
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GUSB2PHYCFG(0), DWC3_GUSB2PHYCFG_SUSPHY);

		//*(volatile unsigned long *)tcc_p2v(HwU30GCTL) &= ~(Hw11); // Release CoreSoftReset
		//*(volatile unsigned long *)tcc_p2v(HwU30GSBUSCFG0) &= ~0xff;
		//*(volatile unsigned long *)tcc_p2v(HwU30GSBUSCFG0) |= Hw1; // INCR4 Burst Enable
		//*(volatile unsigned long *)tcc_p2v(HwU30GSBUSCFG1) |= 0xf<<8; // 16 burst request limit
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GCTL, DWC3_GCTL_CORESOFTRESET);
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GSBUSCFG0, 0xff);
		dwc3_bit_set_phy(dwc3_phy_dev->h_base, DWC3_GSBUSCFG0, Hw1); // INCR4 Burst Enable
		dwc3_bit_set_phy(dwc3_phy_dev->h_base, DWC3_GSBUSCFG1, (0xf<<8)); // 6 burst request limit

		// GCTL
		uTmp = dwc3_readl(dwc3_phy_dev->h_base, DWC3_GCTL);
		uTmp &= ~(Hw7|Hw6|Hw5|Hw4|Hw3);
		uTmp &= ~0xfff80000; // clear 31:19
		uTmp |= 3<<19; // PwrDnScale = 46.875 / 16 = 3
		//    uTmp |= Hw10; // SOFITPSYNC enable
		uTmp &= ~Hw10; // SOFITPSYNC disable
		dwc3_writel(dwc3_phy_dev->h_base, DWC3_GCTL, uTmp);

		// Set REFCLKPER
		uTmp = dwc3_readl(dwc3_phy_dev->h_base, DWC3_GUCTL);
		uTmp &= ~0xffc00000; // clear REFCLKPER
		uTmp |= 41<<22; // REFCLKPER
		uTmp |= Hw15|Hw14;
		dwc3_writel(dwc3_phy_dev->h_base, DWC3_GUCTL, uTmp);

		// GFLADJ
		uTmp = dwc3_readl(dwc3_phy_dev->h_base, 0xc630/*GFLADJ*/);
		uTmp &= ~0xff7fff80;
		uTmp |= 10<<24;
		uTmp |= Hw23; // GFLADJ_REFCLK_LPM_SEL
		uTmp |= 2032<<8; // GFLADJ_REFCLK_FLADJ
		uTmp |= Hw7; // GFLADJ_30MHZ_REG_SEL
		dwc3_writel(dwc3_phy_dev->h_base, 0xc630/*GFLADJ*/, uTmp);

		// GUSB2PHYCFG
		uTmp = dwc3_readl(dwc3_phy_dev->h_base, DWC3_GUSB2PHYCFG(0));
		uTmp |= Hw30; // U2_FREECLK_EXISTS
		uTmp &= ~(Hw4); // UTMI+
		uTmp &= ~(Hw3); // UTMI+ 8bit
		uTmp |= 9<<10; // USBTrdTim = 9
		uTmp |= Hw8; // EnblSlpM
		dwc3_writel(dwc3_phy_dev->h_base, DWC3_GUSB2PHYCFG(0), uTmp);

#if 0
		// Set Host mode
		uTmp = *(volatile unsigned long *)tcc_p2v(HwU30GCTL);
		uTmp &= ~(Hw13|Hw12);
		uTmp |= 0x1<<12; // PrtCapDir
		*(volatile unsigned long *)tcc_p2v(HwU30GCTL) = uTmp;
#endif

		//USBPHYCFG->U30_PCFG0 |= Hw30;

		// Set REFCLKPER
		uTmp = dwc3_readl(dwc3_phy_dev->h_base, DWC3_GUCTL);
		uTmp &= ~0xffc00000; // clear REFCLKPER
		uTmp |= 41<<22; // REFCLKPER
		uTmp &= ~(Hw15);
		dwc3_writel(dwc3_phy_dev->h_base, DWC3_GUCTL, uTmp);

		//=====================================================================
		// Tx Deemphasis setting
		// Global USB3 PIPE Control Register (GUSB3PIPECTL0): 0x1100C2C0
		//=====================================================================
		dwc3_bit_clear_phy(dwc3_phy_dev->h_base, DWC3_GUSB3PIPECTL(0), TX_DEEMPH_SET_MASK);
		dwc3_bit_set_phy(dwc3_phy_dev->h_base, DWC3_GUSB3PIPECTL(0), sel_dm << TX_DEEMPH_SET_SHIFT);

		if(sel_dm == 0)
			printk("dwc3 tcc: PHY cfg - 6dB de-emphasis\n");
		else if (sel_dm == 1)
			printk("dwc3 tcc: PHY cfg - 3.5dB de-emphasis (default)\n");
		else if (sel_dm == 2)
			printk("dwc3 tcc: PHY cfg - de-emphasis\n");

		//=====================================================================
		// Rx EQ setting
		//=====================================================================
		if ( rx_eq != 0xFF)
		{
			//printk("dwc3 tcc: phy cfg - EQ Setting\n");
			//========================================
			//	Read RX_OVER_IN_HI
			//========================================
			uTmp = dwc3_tcc_read_u30phy_reg(phy, 0x1006);
			//printk("Reset value - RX-OVRD: 0x%08X\n", uTmp);
			//printk("Reset value - RX_EQ: 0x%X\n", ((uTmp & 0x00000700 ) >> 8));
			//printk("\x1b[1;33m[%s:%d]rx_eq: %d\x1b[0m\n", __func__, __LINE__, rx_eq);

			BITCSET(uTmp, RX_EQ_MASK, rx_eq << RX_EQ_SHIFT);    
			uTmp |= Hw11;
			//printk("Set value - RX-OVRD: 0x%08X\n", uTmp);
			//printk("Set value - RX_EQ: 0x%X\n", ((uTmp & 0x00000700 ) >> 8));

			dwc3_tcc_write_u30phy_reg(phy, 0x1006, uTmp);

			uTmp = dwc3_tcc_read_u30phy_reg(phy, 0x1006);
			//printk("    Reload - RX-OVRD: 0x%08X\n", uTmp);
			//printk("    Reload - RX_EQ: 0x%X\n", ((uTmp & 0x00000700 ) >> 8));
			printk("dwc3 tcc: PHY cfg - RX EQ: 0x%x\n", ((uTmp & 0x00000700 ) >> 8));
		}

		//=====================================================================
		// Rx Boost setting
		//=====================================================================
		if ( rx_boost != 0xFF)
		{
			//printk("dwc3 tcc: phy cfg - Rx Boost Setting\n");
			//========================================
			//	Read RX_OVER_IN_HI
			//========================================
			uTmp = dwc3_tcc_read_u30phy_reg(phy, 0x1024);
			//printk("Reset value - RX-ENPWR1: 0x%08X\n", uTmp);
			//printk("Reset value - RX_BOOST: 0x%X\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
			//printk("\x1b[1;33m[%s:%d]rx_boost: %d\x1b[0m\n", __func__, __LINE__, rx_boost);

			BITCSET(uTmp, RX_BOOST_MASK, rx_boost << RX_BOOST_SHIFT);    
			uTmp |= Hw5;
			//printk("Reset value - RX-ENPWR1: 0x%08X\n", uTmp);
			//printk("Reset value - RX_BOOST: 0x%X\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));

			dwc3_tcc_write_u30phy_reg(phy, 0x1024, uTmp);

			uTmp = dwc3_tcc_read_u30phy_reg(phy, 0x1024);
			//printk("    Reload value - RX-ENPWR1: 0x%08X\n", uTmp);
			//printk("    Reload value - RX_BOOST: 0x%X\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
			printk("dwc3 tcc: PHY cfg - RX BOOST: 0x%x\n", ((uTmp & RX_BOOST_MASK) >> RX_BOOST_SHIFT));
		}

		if ( ssc != 0xFF)
		{
			//printk("dwc3 tcc: phy cfg - TSSC Setting\n");
			//========================================
			//	Read SSC_OVER_IN
			//========================================
			uTmp = dwc3_tcc_read_u30phy_reg(phy, 0x13);

			BITCSET(uTmp, SSC_REF_CLK_SEL_MASK, ssc);

			dwc3_tcc_write_u30phy_reg(phy, 0x13, uTmp);

			uTmp = dwc3_tcc_read_u30phy_reg(phy, 0x13);
			printk("dwc3 tcc: PHY cfg - SSC_REF_CLK_SEL: 0x%x\n", uTmp & SSC_REF_CLK_SEL_MASK);
		}
#endif
		#if defined(CONFIG_ARCH_TCC898X)
		dwc3_tcc898x_swreset(USBPHYCFG, OFF);
		#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
		//dwc3_tcc899x_swreset(tcc, OFF);
		//msleep(10);
		#endif
		
		mdelay(10);

	    //while(1) {
		   // uTmp = USBPHYCFG->U30_PCFG0;
		   // uTmp = (uTmp>>31)&0x1;
	    //
		   // if (uTmp){
				//printk("dwc3 tcc: PHY init ok - %s speed support\n", maximum_speed);
				//break;
		   // }
	    //}
		if(clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
           dev_err(dwc3_phy_dev->dev,
                 "can't do xhci phy clk enable\n");
        }
		is_suspend = 0;
	} else if (on_off == OFF && !is_suspend) {
		clk_disable_unprepare(dwc3_phy_dev->phy_clk);
		// USB 3.0 PHY Power down
		printk("dwc3 tcc: PHY power down\n");
		USBPHYCFG->U30_PCFG0 |= (Hw25|Hw24);
		mdelay(10);
		uTmp = USBPHYCFG->U30_PCFG0;
		is_suspend = 1;
	} else if (on_off == PHY_RESUME && is_suspend) {
		is_suspend = 0;
		printk("dwc3 tcc: PHY resume\n");
		USBPHYCFG->U30_PCFG0 &= ~(Hw25|Hw24);
		mdelay(10);
		if (clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
			dev_err(dwc3_phy_dev->dev,
				"can't do xhci phy clk enable\n");
		}
	}
	return 0;
}

#if defined(CONFIG_ARCH_TCC803X)
int dwc3_tcc_ss_phy_ctrl_native(struct usb_phy *phy, int on_off)
{
	struct tcc_dwc3_device *dwc3_phy_dev = container_of(phy, struct tcc_dwc3_device, phy);
	PUSBSSPHYCFG USBPHYCFG = (PUSBSSPHYCFG)dwc3_phy_dev->base;
	unsigned int uTmp = 0;
	unsigned int tmp_data = 0;
	unsigned int cal_value = 0;
	int tmp_cnt;

	printk("%s %s\n", __func__, (on_off)?"ON":"OFF");
	if(on_off== ON && is_suspend) {
		//======================================================
	    // 1.Power-on Reset
		//======================================================
		writel((readl(&USBPHYCFG->U30_PCFG4) | (Hw0)), &USBPHYCFG->U30_PCFG4); // PHY external Clock Configuration mode selection
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31), &USBPHYCFG->U30_LCFG); //vcc_reset_n
		writel((readl(&USBPHYCFG->U30_PCFG0) & ~(Hw30)), &USBPHYCFG->U30_PCFG0); // phy_reset
		writel((readl(&USBPHYCFG->FPHY_PCFG0) | (Hw31)), &USBPHYCFG->FPHY_PCFG0); // USB 2.0 PHY POR Set

		uTmp = readl(&USBPHYCFG->U30_PCFG0);
		uTmp &= ~(Hw25); // turn off SS circuits
		uTmp &= ~(Hw24); // turn on HS circuits
		writel(uTmp, &USBPHYCFG->U30_PCFG0);
		msleep(1);

		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)), &USBPHYCFG->U30_PCFG0); // phy_reset
		
		//External SRAM Init Done Wait		
		tmp_data = readl(&USBPHYCFG->U30_PCFG0);
		tmp_data &= (Hw5);
		while(tmp_data == 0)
		{
			tmp_data = readl(&USBPHYCFG->U30_PCFG0); 
			tmp_data &= (Hw5);
		}
		
		// External SRAM LD Done Set
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw3)), &USBPHYCFG->U30_PCFG0);

		writel(0xE31C243C, &USBPHYCFG->FPHY_PCFG1); //Set TXVRT to 0xA
		writel(0x31C71457, &USBPHYCFG->U30_PCFG13); //Set Tx vboost level to 0x7
		writel(0xA4C4302A, &USBPHYCFG->U30_PCFG15); //Set Tx iboost level to 0xA

		// USB 2.0 PHY POR Release
		writel((readl(&USBPHYCFG->FPHY_PCFG0) & ~(Hw24)), &USBPHYCFG->FPHY_PCFG0);
		writel((readl(&USBPHYCFG->FPHY_PCFG0) & ~(Hw31)), &USBPHYCFG->FPHY_PCFG0);

		// LINK Reset Release
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)), &USBPHYCFG->U30_LCFG); // vcc_reset_n
		
		//printk("PHY = 0x%x \n", USBPHYCFG->U30_PCFG0);
		tmp_cnt = 0;
		while( tmp_cnt < 10000)
		{
			if(readl(&USBPHYCFG->U30_PCFG0) & 0x00000004)
			{
				break;
			}

			tmp_cnt++;
			udelay(5);
		}
		printk("XHCI PHY valid check %s\x1b[0m\n",tmp_cnt>=9999?"fail!":"pass.");

		//======================================================
		// Initialize all registers
		//======================================================
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw27 | Hw26 | Hw19 | Hw18 | Hw17 | Hw7)), &USBPHYCFG->U30_LCFG);
		/* Set 2.0phy REXT */
		tmp_cnt = 0;

		do
		{
			//Read calculated value
			writel(Hw26|Hw25, dwc3_phy_dev->ref_base);
			uTmp = readl(dwc3_phy_dev->ref_base);
			printk("2.0H status bus = 0x%08x\n", uTmp);
			cal_value = 0x0000F000&uTmp;
			//printk("Cal_value = 0x%08x\n", cal_value);

			cal_value = cal_value<<4; //set TESTDATAIN

			//Read Status Bus
			writel(Hw26|Hw25, &USBPHYCFG->FPHY_PCFG3); 
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);
			//printk("2.0 status bus = 0x%08x\n", uTmp);

			//Read Override Bus
			writel(Hw29|Hw26|Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);
			//printk("2.0 override bus = 0x%08x\n", uTmp);

			//Write Override Bus
			writel(Hw29|Hw26|Hw25|Hw23|Hw22|Hw21|Hw20|cal_value, &USBPHYCFG->FPHY_PCFG3);
			udelay(1);
			writel(Hw29|Hw28|Hw26|Hw25|Hw23|Hw22|Hw21|Hw20|cal_value, &USBPHYCFG->FPHY_PCFG3);
			udelay(1);
			writel(Hw29|Hw26|Hw25|Hw23|Hw22|Hw21|Hw20|cal_value, &USBPHYCFG->FPHY_PCFG3);
			udelay(1);

			//Read Status Bus
			writel(Hw26|Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);
			//printk("2.0 status bus = 0x%08x\n", uTmp);

			//Read Override Bus
			writel(Hw29|Hw26|Hw25, &USBPHYCFG->FPHY_PCFG3);
			uTmp = readl(&USBPHYCFG->FPHY_PCFG3);
			printk("2.0 REXT = 0x%08x\n", (0x0000F000&uTmp));

			tmp_cnt ++;
		} while(((uTmp&0x0000F000) == 0) && (tmp_cnt < 5));

#if defined (CONFIG_TCC_BC_12)
		writel(readl(&USBPHYCFG->FPHY_PCFG4) | (1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
		writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1<<30), &USBPHYCFG->FPHY_PCFG4);//Disable VBUS Detect

		writel(readl(&USBPHYCFG->FPHY_PCFG4) & ~(1<<31), &USBPHYCFG->FPHY_PCFG4);//clear irq
		udelay(1);
		writel(readl(&USBPHYCFG->FPHY_PCFG2) | ((1<<8)|(1<<10)), &USBPHYCFG->FPHY_PCFG2);
		udelay(1);
		writel(readl(&USBPHYCFG->FPHY_PCFG4 ) | (1<<28), &USBPHYCFG->FPHY_PCFG4);//enable CHG_DET interrupt

		writel((readl(&USBPHYCFG->U30_PINT ) & ~(1<<6)) | ((1<<31)|(1<<7)|(1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|(1<<0)), &USBPHYCFG->U30_PINT);

		enable_irq(dwc3_phy_dev->irq);
#endif
		// Link global Reset
		writel((readl(&USBPHYCFG->U30_LCFG) & ~Hw31), &USBPHYCFG->U30_LCFG); // CoreRSTN (Cold Reset), active low

		if (!strncmp("high", maximum_speed, 4)) {
			// USB20 Only Mode
			writel((readl(&USBPHYCFG->U30_LCFG) | Hw28), &USBPHYCFG->U30_LCFG); // enable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp |= Hw25; // turn off SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		} else if (!strncmp("super", maximum_speed, 5)) {
			// USB 3.0
			writel((readl(&USBPHYCFG->U30_LCFG) & ~(Hw28)), &USBPHYCFG->U30_LCFG); // disable usb20mode -> removed in DWC_usb3 2.60a, but use as interrupt
			uTmp = USBPHYCFG->U30_PCFG0;
			uTmp &= ~(Hw25); // turn on SS circuits
			uTmp &= ~(Hw24); // turn on HS circuits
			uTmp &= ~(Hw30); // release PHY reset
			writel(uTmp, &USBPHYCFG->U30_PCFG0);
		}
		mdelay(1);
		// Release Reset Link global
		writel((readl(&USBPHYCFG->U30_PCFG0) | (Hw30)), &USBPHYCFG->U30_PCFG0);
		writel((readl(&USBPHYCFG->U30_LCFG) | (Hw31)), &USBPHYCFG->U30_LCFG);

		mdelay(10);

	    //while(1) {
		   // uTmp = USBPHYCFG->U30_PCFG0;
		   // uTmp = (uTmp>>31)&0x1;
	    //
		   // if (uTmp){
				//printk("dwc3 tcc: PHY init ok - %s speed support\n", maximum_speed);
				//break;
		   // }
	    //}
		if(clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
           dev_err(dwc3_phy_dev->dev,
                 "can't do xhci phy clk enable\n");
        }
		is_suspend = 0;
	} else if (on_off == OFF && !is_suspend) {
		clk_disable_unprepare(dwc3_phy_dev->phy_clk);
		// USB 3.0 PHY Power down
		dev_info(dwc3_phy_dev->dev,
				"dwc3 tcc: PHY power down\n");
		USBPHYCFG->U30_PCFG0 |= (Hw25|Hw24);
		mdelay(10);
		uTmp = USBPHYCFG->U30_PCFG0;
		is_suspend = 1;
	} else if (on_off == PHY_RESUME && is_suspend) {
		USBPHYCFG->U30_PCFG0 &= ~(Hw25|Hw24);
		dev_info(dwc3_phy_dev->dev,
				"dwc3 tcc: PHY power up\n");
		if (clk_prepare_enable(dwc3_phy_dev->phy_clk) != 0) {
			dev_err(dwc3_phy_dev->dev,
				"can't do xhci phy clk enable\n");
		}
		is_suspend = 0;
	}

	return 0;
}
#endif
static int tcc_dwc3_init_phy(struct usb_phy *phy)
{
#ifdef CONFIG_ARCH_TCC803X
	if(system_rev == 0)
		return dwc3_tcc_phy_ctrl_native(phy, ON);
	else
		return dwc3_tcc_ss_phy_ctrl_native(phy, ON);
#else
	return dwc3_tcc_phy_ctrl_native(phy, ON);
#endif
}

static int tcc_dwc3_suspend_phy(struct usb_phy *phy, int suspend)
{
	if (!suspend) {
		return phy->set_phy_state(phy, PHY_RESUME);
	}
	else {
		return phy->set_phy_state(phy, OFF);
	}
}

static void tcc_dwc3_shutdown_phy(struct usb_phy *phy)
{
	phy->set_phy_state(phy, OFF);
}
/* create phy struct */
static int tcc_dwc3_create_phy(struct device *dev, struct tcc_dwc3_device *phy_dev)
{
	phy_dev->phy.otg = devm_kzalloc(dev, sizeof(*phy_dev->phy.otg),	GFP_KERNEL);
	if (!phy_dev->phy.otg)
		return -ENOMEM;

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

	// HCLK
	//phy_dev->hclk = of_clk_get(dev->of_node, 0);
	//if (IS_ERR(phy_dev->hclk))
	//	phy_dev->hclk = NULL;

	// PHY CLK
	phy_dev->phy_clk = of_clk_get(dev->of_node, 0);
	if (IS_ERR(phy_dev->phy_clk)) {
		printk("xhci phy clk_get failed\n");
		phy_dev->phy_clk = NULL;
	}
			
	phy_dev->dev				= dev;

	phy_dev->phy.dev			= phy_dev->dev;
	phy_dev->phy.label			= "tcc_dwc3_phy";
	phy_dev->phy.state			= OTG_STATE_UNDEFINED;
	phy_dev->phy.type			= USB_PHY_TYPE_USB3;
	phy_dev->phy.init 			= tcc_dwc3_init_phy;
	phy_dev->phy.set_suspend	= tcc_dwc3_suspend_phy;
	phy_dev->phy.shutdown		= tcc_dwc3_shutdown_phy;
#if defined(CONFIG_ARCH_TCC803X)
	phy_dev->phy.set_phy_state	= dwc3_tcc_ss_phy_ctrl_native;
#else
	phy_dev->phy.set_phy_state	= dwc3_tcc_phy_ctrl_native;
#endif
#if defined (CONFIG_TCC_BC_12)
	phy_dev->phy.set_chg_det = tcc_dwc3_set_chg_det;
#endif
	if (phy_dev->vbus_gpio)
		phy_dev->phy.set_vbus		= tcc_dwc3_vbus_set;
	phy_dev->phy.get_base			= dwc3_get_base;
//#ifdef CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT		/* 017.02.24 */
//	phy_dev->phy.get_dc_voltage_level = tcc_dwc_otg_get_dc_level;
//	phy_dev->phy.set_dc_voltage_level = tcc_dwc_otg_set_dc_level;
//#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

	phy_dev->phy.otg->usb_phy			= &phy_dev->phy;

	return 0;
}

static int tcc_dwc3_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_dwc3_device *phy_dev;
	int retval;
#if defined (CONFIG_TCC_BC_12)
	int irq, ret=0;
#endif

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);

	retval = tcc_dwc3_create_phy(dev, phy_dev);
	if (retval) {
		dev_err(&pdev->dev, "error create phy\n");
		return retval;
	}
#if defined (CONFIG_TCC_BC_12)
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
#endif

	//if (!request_mem_region(pdev->resource[0].start,
	//			pdev->resource[0].end - pdev->resource[0].start + 1,
	//			"dwc3_base")) {
	//	dev_err(&pdev->dev, "error reserving mapped memory\n");
	//	retval = -EFAULT;
	//}
	//phy_dev->h_base = (void __iomem*)ioremap_nocache((resource_size_t)pdev->resource[0].start,
	//			 pdev->resource[0].end - pdev->resource[0].start+1);


	if (!request_mem_region(pdev->resource[0].start,
				pdev->resource[0].end - pdev->resource[0].start + 1,
				"dwc3_phy")) {
		dev_err(&pdev->dev, "error reserving mapped memory\n");
		retval = -EFAULT;
	}
	phy_dev->base = (void __iomem*)ioremap_nocache((resource_size_t)pdev->resource[0].start,
				 pdev->resource[0].end - pdev->resource[0].start+1);

	phy_dev->phy.base = phy_dev->base;
#if defined (CONFIG_TCC_BC_12)
	ret = devm_request_irq(&pdev->dev, phy_dev->irq, tcc_dwc3_chg_irq, IRQF_SHARED, pdev->dev.kobj.name, phy_dev);
	if (ret)
		dev_err(&pdev->dev, "request irq failed\n");

	disable_irq(phy_dev->irq);
	INIT_WORK(&phy_dev->dwc3_work, tcc_dwc3_set_cdp);
#endif

#if defined(CONFIG_ARCH_TCC803X)
	phy_dev->ref_base = (void __iomem*)ioremap_nocache(pdev->resource[1].start, pdev->resource[1].end - pdev->resource[1].start+1);
	phy_dev->phy.ref_base = phy_dev->ref_base;
#endif

	platform_set_drvdata(pdev, phy_dev);

	retval = usb_add_phy_dev(&phy_dev->phy);
	if (retval) {
		dev_err(&pdev->dev, "usb_add_phy failed\n");
		return retval;
	}
	printk("%s:%s\n",pdev->dev.kobj.name, __func__);

	return retval;	
}

static int tcc_dwc3_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc3_device *phy_dev = platform_get_drvdata(pdev);;

	usb_remove_phy(&phy_dev->phy);
	release_mem_region(pdev->resource[0].start,     //dwc3 base
                        pdev->resource[0].end - pdev->resource[0].start + 1);
    release_mem_region(pdev->resource[1].start,     //dwc3 phy base
                        pdev->resource[1].end - pdev->resource[1].start + 1);
	return 0;
}

static const struct of_device_id tcc_dwc3_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc3_phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_dwc3_phy_match);

static struct platform_driver tcc_dwc3_phy_driver = {
	.probe = tcc_dwc3_phy_probe,
	.remove = tcc_dwc3_phy_remove,
	.driver = {
		.name = "dwc3_phy",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_dwc3_phy_match),
#endif
	},
};

static int __init tcc_dwc3_phy_drv_init(void)
{
	int retval = 0;

	retval = platform_driver_register(&tcc_dwc3_phy_driver);
	if (retval < 0)
		printk(KERN_ERR "%s retval=%d\n", __func__, retval);

	return retval;
}
core_initcall(tcc_dwc3_phy_drv_init);

static void __exit tcc_dwc3_phy_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc3_phy_driver);
}
module_exit(tcc_dwc3_phy_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TCC USB DWC3 transceiver driver");
