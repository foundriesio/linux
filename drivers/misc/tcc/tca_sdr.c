/****************************************************************************
 *   FileName    : tca_sdr.c
 *   Description : This is for TCC802x set port mux.
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips, Inc.
 *   ALL RIGHTS RESERVED
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/mach-types.h>

#include "tcc_sdr.h"

#if defined(CONFIG_ARCH_TCC802X)
/////* All about GPIO */////
#define OFFSET_GPIOB    0x040   //GPIO_B BASE ADDR OFFSET
#define OFFSET_GPIOC    0x080   //GPIO_C BASE ADDR OFFSET
#define OFFSET_GPIOD    0x0C0   //GPIO_D BASE ADDR OFFSET
#define OFFSET_GPIOE    0x100   //GPIO_E BASE ADDR OFFSET
#define OFFSET_GPIOF    0x140   //GPIO_F BASE ADDR OFFSET
#define OFFSET_GPIOG    0x180   //GPIO_G BASE ADDR OFFSET
#define OFFSET_GPIOSD0  0x240   //GPIO_SD0 BASE ADDR OFFSET
#define OFFSET_GPIOH    0x400   //GPIO_H BASE ADDR OFFSET
#define OFFSET_GPIOK    0x440   //GPIO_K BASE ADDR OFFSET

#define GPIO_SIZE       0x040   //Size of GPIO Setting Register Group

/* Don't need it.
#define GDATA       0x000   // Data
#define GOUT_EN     0x004   // Output Enable
#define GOUT_OR     0x008   // OR Fnction on Output Data
#define GOUT_BIC    0x00C   // BIC Function on Output Data
#define GOUT_XOR    0x010   // XOR Function on Output Data
#define GSTRENGTH0  0x014   // Driver Strength Control 0
#define GSTRENGTH1  0x018   // Driver Strength Control 1
#define GPULL_EN    0x01C   // Pull-Up/Down Enable
#define GPULL_SEL   0x020   // Pull-Up/Down Select
#define GIN_EN      0x024   // Input Enable
*/
#define GIN_TYPE    0x028   // Input Type (Shmitt / CMOS)

/* Don't need it.
#define GSLEW_RATE  0x02C   // Slew Rate
*/
#define GF_SEL0     0x030   // Port Configuration 0
#define GF_SEL1     0x034   // Port Configuration 1
#define GF_SEL2     0x038   // Port Configuration 2
#define GF_SEL3     0x03C   // Port Configuration 3

/////* Specific of TCC802x */////
#define DAI_OFFSET	0x1000 //Port Configuration Register Offset
#define PCFG_OFFSET 0x3000 //Port Configuration Register Offset
#define PCFG0       0x00 //Port Configuration Register0
#define PCFG1       0x04 //Port Configuration Register1
#define PCFG2       0x08 //Port Configuration Register2
#define PCFG3       0x0C //Port Configuration Register3

#define BASE_ADDR_GPIOA                     (0x14200000)

void tca_sdr_gpio_schmitt(unsigned int base_addr, int pin_num, bool en)
{
	void __iomem *gpio_reg = ioremap(base_addr, GPIO_SIZE);
	unsigned int reg_value=0, breg=0;

	if((pin_num >= 0)&&(pin_num < 32)){

		reg_value = readl(gpio_reg + GIN_TYPE);
		if(en) {
			breg = 0x1 << (pin_num);
			reg_value |= breg;
		} else {
			breg = ~(0x1) << (pin_num);
			reg_value &= breg;
		}
		writel(reg_value, gpio_reg + GIN_TYPE);

	}else{
		printk("[%s] ERROR! Invalid input [pin_num:%d]\n", __func__, pin_num);
	}

	iounmap(gpio_reg);
}

void tca_sdr_gpio_func(unsigned int base_addr, int pin_num, int func)
{
	void __iomem *gpio_reg = ioremap(base_addr, GPIO_SIZE);
	unsigned int reg_value=0, breg=0xF;

	if((pin_num >= 0)&&(pin_num < 8)){

		reg_value = readl(gpio_reg + GF_SEL0);
		breg = 0xF << (pin_num*4);
		reg_value &= ~breg;
		breg = func << (pin_num*4);
		reg_value |= breg;
		writel(reg_value, gpio_reg + GF_SEL0);

	}else if((pin_num >= 8)&&(pin_num < 16)){

		reg_value = readl(gpio_reg + GF_SEL1);
		breg = 0xF << ((pin_num-8)*4);
		reg_value &= ~breg;
		breg = func << ((pin_num-8)*4);
		reg_value |= breg;
		writel(reg_value, gpio_reg + GF_SEL1);

	}else if((pin_num >= 16)&&(pin_num < 24)){

		reg_value = readl(gpio_reg + GF_SEL2);
		breg = 0xF << ((pin_num-16)*4);
		reg_value &= ~breg;
		breg = func << ((pin_num-16)*4);
		reg_value |= breg;
		writel(reg_value, gpio_reg + GF_SEL2);

	}else if((pin_num >= 24)&&(pin_num < 32)){

		reg_value = readl(gpio_reg + GF_SEL3);
		breg = 0xF << ((pin_num-24)*4);
		reg_value &= ~breg;
		breg = func << ((pin_num-24)*4);
		reg_value |= breg;
		writel(reg_value, gpio_reg + GF_SEL3);

	}else{
		printk("[%s] ERROR! Invalid input [pin_num:%d]\n", __func__, pin_num);
	}

	iounmap(gpio_reg);
}
void tca_sdr_gpio_set(char port_num, int mode, bool en)
{
	//GPIO_A, B, C, D, E, F, G, SD0, H, K, END
	unsigned int gpio_alphabet[]={0, 17, 47, 77, 99, 131, 163, 183, 195, 206, 238};
	unsigned int gpio_offset[]={ 0, 0x40, 0x80, 0xC0, 0x100, 0x140, 0x180, 0x240, 0x400, 0x440, 0x480};
	unsigned int base_addr = BASE_ADDR_GPIOA;
	int i=0;
	if(port_num == 255) {
		goto out;
	}

	if(port_num > 237) {
		printk("[%s] ERROR! Invalid input [pin_num:%d]\n", __func__, port_num);

	} else {

		for(i=0; i < (sizeof(gpio_alphabet)-1); i++){

			base_addr=BASE_ADDR_GPIOA + gpio_offset[i];

			if((port_num >= gpio_alphabet[i])&&(port_num < gpio_alphabet[i+1])){
				if(mode == 0) {
					tca_sdr_gpio_func(base_addr, port_num-gpio_alphabet[i], (en)? 0xf : 0);
				} else if(mode == 1) {
					tca_sdr_gpio_schmitt(base_addr, port_num-gpio_alphabet[i], en);
				} else {
					printk("[%s] ERROR! Invalid input [mode:%d]\n", __func__, mode);
				}
#if 0   //for debug
				if((base_addr&0xFFF) <= 0x180)
					printk("GPIO_%c_%d[pin_num:%d]\n", ('A'+i), port_num-gpio_alphabet[i], port_num);
				else if((base_addr&0xFFF) == 0x240)
					printk("GPIO_SD0_%d[pin_num:%d]\n", port_num-gpio_alphabet[i], port_num);
				else if((base_addr&0xFFF) == 0x400)
					printk("GPIO_H_%d[pin_num:%d]\n", port_num-gpio_alphabet[i], port_num);
				else if((base_addr&0xFFF) == 0x440)
					printk("GPIO_K_%d[pin_num:%d]\n", port_num-gpio_alphabet[i], port_num);
#endif
				break;
			}
		}
	}
out:
	i=0;    //This is only to avoid compile error

}

void tca_sdr_port_mux(void *pDAIBaseAddr, HS_I2S_PORT *port)
{
	void __iomem *pcfg_reg = pDAIBaseAddr - DAI_OFFSET + PCFG_OFFSET;
	HS_I2S_PORT *sdr_port = port;
	unsigned int reg_value = 0;
	int i = 0;

	for(i=0; i<3; i++){
		//GPIO FNC set
		//printk("%s clk[%d]=%d ", __func__,i, sdr_port->clk[i]);
		tca_sdr_gpio_set(sdr_port->clk[i], 0, 1);
	}
	for(i=0; i<4; i++){
		//GPIO FNC set
		//printk("%s dain[%d]=%d ", __func__,i, sdr_port->dain[i]);
		tca_sdr_gpio_set(sdr_port->dain[i], 0, 1);
	}

	reg_value = (sdr_port->dain[0]|(sdr_port->dain[1]<<8)|(sdr_port->dain[2]<<16)|(sdr_port->dain[3]<<24));
	/* DAI_DI0 | DAI_DI1 | DAI_DI2 | DAI_DI3 */
	writel(reg_value, pcfg_reg + PCFG0);

	reg_value = (sdr_port->clk[1]|(sdr_port->clk[2]<<8)|(0xFF<<16)|(0xFF<<24));
	/* DAI_BCLK | DAI_LRCK | DAI_DO0 | DAI_DO1 */
	writel(reg_value, pcfg_reg + PCFG1);

	/* For Debug
		reg_value = readl(pcfg_reg + PCFG0);
		printk("PCFG0(%p)=0x%08x\n", io_v2p(pcfg_reg+PCFG0), reg_value);
		reg_value = readl(pcfg_reg + PCFG1);
		printk("PCFG1(%p)=0x%08x\n", io_v2p(pcfg_reg+PCFG1), reg_value);
		reg_value = readl(pcfg_reg + PCFG2);
		printk("PCFG2(%p)=0x%08x\n", io_v2p(pcfg_reg+PCFG2), reg_value);
	*/

}
#endif
