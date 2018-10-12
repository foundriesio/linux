
/****************************************************************************
 *   FileName    : tca_tcchwcontrol.c
 *   Description : This is for TCC898x, TCC802x set port mux.
 And TCC897x, TCC898x, TCC802x Audio register dump.
 ****************************************************************************
 *
 *   TCC Version 2.0
 *   Copyright (c) Telechips, Inc.
 *   ALL RIGHTS RESERVED
 *
 ****************************************************************************/
/*****************************************************************************
 *
 * includes
 *
 ******************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/mach-types.h>

#include "../tcc-i2s.h"
#include "../tcc-spdif.h"
#include "../tcc-cdif.h"
#include "../tcc-pcm.h"
#include "tca_tcchwcontrol.h"


#if defined(CONFIG_ARCH_TCC898X)
/*****************************************************************************
 * Function Name : tca_dai_port_mux()
 ******************************************************************************
 * Desription    : DAI GPIO port mux for TCC898x
 * Parameter     : 3b'xx1-CH0 / 3b'x10-CH1 / 3b'100-CH2 / 3b'000-CH3 /
 * Return        : success(SUCCESS) 
 ******************************************************************************/
void tca_i2s_port_mux(int id, int *pport)
{
	void __iomem *pChSel_reg = ioremap(BASE_ADDR_CHSEL, CHSEL_END-CHSEL_DAI0);
	int port = *pport;
	unsigned int ret = 0;
	ret = 0x1 << port;
	ret = ret & (0x7);
	//	printk("i2s id: %d, port: %d, ret:0x%04x\n", id, port, ret);	
	if(id == 0){
		tca_writel(ret, pChSel_reg + CHSEL_DAI0);
	}else{
		tca_writel(ret, pChSel_reg + CHSEL_DAI1);
	}
	iounmap(pChSel_reg);
}
EXPORT_SYMBOL_GPL(tca_i2s_port_mux);

void tca_spdif_port_mux(int id, int *pport)
{
	void __iomem *pChSel_reg = ioremap(BASE_ADDR_CHSEL, CHSEL_END-CHSEL_DAI0);
	int port = *pport;
	unsigned int ret = 0;
	ret = 0x1 << port;
	ret = ret & (0x7);
	//	printk("spdif id: %d, port: %d, ret:0x%04x\n", id, port, ret);	
	if(id == 0){
		tca_writel(ret, pChSel_reg + CHSEL_SPDIF0);
	}else{
		tca_writel(ret, pChSel_reg + CHSEL_SPDIF1);
	}
	iounmap(pChSel_reg);
}
EXPORT_SYMBOL_GPL(tca_spdif_port_mux);

void tca_cdif_port_mux(int id, int *pport)
{
	void __iomem *pChSel_reg = ioremap(BASE_ADDR_CHSEL, CHSEL_END-CHSEL_DAI0);
	int port = *pport;
	unsigned int ret = 0;
	ret = 0x1 << port;
	ret = ret & (0x7);
	//	printk("spdif id: %d, port: %d, ret:0x%04x\n", id, port, ret);	
	if(id == 0){
		tca_writel(ret, pChSel_reg + CHSEL_CDIF0);
	}else{
		tca_writel(ret, pChSel_reg + CHSEL_CDIF1);
	}
	iounmap(pChSel_reg);
}
EXPORT_SYMBOL_GPL(tca_cdif_port_mux);

#elif defined(CONFIG_ARCH_TCC802X)

enum{
	DAI_I2S_TYPE=0,
	DAI_SPDIF_TYPE,
	DAI_CDIF_TYPE
};

static void tca_gpio_schmitt(unsigned int base_addr, int pin_num, bool en)
{
	void __iomem *gpio_reg = ioremap(base_addr, GPIO_SIZE);
	unsigned int reg_value=0, breg=0;

	if((pin_num >= 0)&&(pin_num < 32)){

		reg_value = tca_readl(gpio_reg + GIN_TYPE);
		if(en) { 
			breg = 0x1 << (pin_num);
			reg_value |= breg;
		} else { 
			breg = ~(0x1) << (pin_num);
			reg_value &= breg;
		}
		tca_writel(reg_value, gpio_reg + GIN_TYPE);

	}else{
		printk("[%s] ERROR! Invalid input [pin_num:%d]\n", __func__, pin_num);
	}

	iounmap(gpio_reg);
}
static void tca_gpio_func(unsigned int base_addr, int pin_num, int func)
{
	void __iomem *gpio_reg = ioremap(base_addr, GPIO_SIZE);
	unsigned int reg_value=0, breg=0xF;

	if((pin_num >= 0)&&(pin_num < 8)){

		reg_value = tca_readl(gpio_reg + GF_SEL0);
		breg = 0xF << (pin_num*4);
		reg_value &= ~breg;
		breg = func << (pin_num*4);
		reg_value |= breg;
		tca_writel(reg_value, gpio_reg + GF_SEL0);

	}else if((pin_num >= 8)&&(pin_num < 16)){

		reg_value = tca_readl(gpio_reg + GF_SEL1);
		breg = 0xF << ((pin_num-8)*4);
		reg_value &= ~breg;
		breg = func << ((pin_num-8)*4);
		reg_value |= breg;
		tca_writel(reg_value, gpio_reg + GF_SEL1);

	}else if((pin_num >= 16)&&(pin_num < 24)){

		reg_value = tca_readl(gpio_reg + GF_SEL2);
		breg = 0xF << ((pin_num-16)*4);
		reg_value &= ~breg;
		breg = func << ((pin_num-16)*4);
		reg_value |= breg;
		tca_writel(reg_value, gpio_reg + GF_SEL2);

	}else if((pin_num >= 24)&&(pin_num < 32)){

		reg_value = tca_readl(gpio_reg + GF_SEL3);
		breg = 0xF << ((pin_num-24)*4);
		reg_value &= ~breg;
		breg = func << ((pin_num-24)*4);
		reg_value |= breg;
		tca_writel(reg_value, gpio_reg + GF_SEL3);

	}else{
		printk("[%s] ERROR! Invalid input [pin_num:%d]\n", __func__, pin_num);
	}

	iounmap(gpio_reg);
}

/*****************************************************************************
 * Function Name : tca_gpio_set()
 ******************************************************************************
 * Desription    : This is for TCC802x gpio setting. GFB number find gpio port.
 * Parameter     : 
 - port_num (0x0 ~ 0xFF): GFB number.
 - mode: [0] gpio function selection, [1] schmitt input type enable.
 - en: [0] disable, [1] enable.
 - example
 -- mode[0]en[0] means gpio function selection '0x0'
 -- mode[0]en[1] means gpio function selection '0xf'
 -- mode[1]en[0] means gpio CMOS input type
 -- mode[1]en[1] means gpio schmitt input type
 * Return        : N/A
 ******************************************************************************/

static void tca_gpio_set(char port_num, int mode, bool en)
{
	//GPIO_A, B, C, D, E, F, G, H, K, SD0 END
	unsigned int gpio_alphabet[]={0, 17, 47, 77, 99, 131, 163, 183, 195, 227, 238};
	unsigned int gpio_offset[]={0, 0x40, 0x80, 0xC0, 0x100, 0x140, 0x180, 0x400, 0x440, 0x240, 0x480};
	unsigned int base_addr = BASE_ADDR_GPIOA;
	int i;
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
					tca_gpio_func(base_addr, port_num-gpio_alphabet[i], (en)? 0xf : 0);
				} else if(mode == 1) {
					tca_gpio_schmitt(base_addr, port_num-gpio_alphabet[i], en);
				} else {
					printk("[%s] ERROR! Invalid input [mode:%d]\n", __func__, mode);
				}
#if 0	//for debug
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
	i=0;	//This is only to avoid compile error

}

/*****************************************************************************
 * Function Name : tca_audio_gpio_port_set()
 ******************************************************************************
 * Desription    : This is for TCC802x Audio gpio setting.
 * Parameter     : 
 - type: Each I2S, SPDIF, CDIF differ the number of port.
 - port: I2S, SPDIF, CDIF port group.
 * Return        : N/A
 ******************************************************************************/

static void tca_audio_gpio_port_set(int type, void *port)	
{
	struct audio_i2s_port *i2s_port = NULL;
	char *no_i2s_port = NULL; //for SPDIF, CDIF
	int i, port_num = 0;
	if(type == DAI_I2S_TYPE){
		i2s_port = /*(struct audio_i2s_port *)*/port;
	}else if(type == DAI_SPDIF_TYPE){
		no_i2s_port = (char *)port;
		port_num = 2;
	}else if(type == DAI_CDIF_TYPE){
		i2s_port = /*(struct audio_i2s_port *)*/port;
	}

	if(type == DAI_I2S_TYPE){
		for(i=0; i<3; i++){
			//GPIO FNC set
			//printk("%s clk[%d]=%d ", __func__,i, i2s_port->clk[i]);
			tca_gpio_set(i2s_port->clk[i], 0, 1);
		}
		for(i=0; i<4; i++){
			//GPIO FNC set
			//printk("%s daout[%d]=%d ", __func__,i, i2s_port->daout[i]);
			tca_gpio_set(i2s_port->daout[i], 0, 1);
			//GPIO FNC set
			//printk("%s dain[%d]=%d ", __func__,i, i2s_port->dain[i]);
			tca_gpio_set(i2s_port->dain[i], 0, 1);
		}
	}else if(type == DAI_CDIF_TYPE){
		for(i=1; i<3; i++){
			//GPIO FNC set
			//printk("%s clk[%d]=%d ", __func__,i, i2s_port->clk[i]);
			tca_gpio_set(i2s_port->clk[i], 0, 1);
		}
	
		//printk("%s dain[0]=%d ", __func__, i2s_port->dain[0]);
		tca_gpio_set(i2s_port->dain[0], 0, 1);
	}else{
		for(i=0; i<port_num; i++){
			//GPIO FNC set
			//printk("%s no_i2s_port[%d]=%d ", __func__,i, no_i2s_port[i]);
			tca_gpio_set(no_i2s_port[i], 0, 1);
		}
	}
}
/*****************************************************************************
 * Function Name : tca_dai_port_mux()
 ******************************************************************************
 * Desription    : DAI GPIO port mux for TCC802x
 * Parameter     : Audio ID, GPIO port Number
 * Return        :  
 ******************************************************************************/
void tca_i2s_port_mux(void *pDAIBaseAddr, struct audio_i2s_port *port)
{
	void __iomem *pdai_reg = pDAIBaseAddr;
	struct audio_i2s_port *i2s_port = port;
	unsigned int reg_value = 0;

	tca_audio_gpio_port_set(DAI_I2S_TYPE, i2s_port);	

	reg_value = (i2s_port->dain[0]|(i2s_port->dain[1]<<8)|(i2s_port->dain[2]<<16)|(i2s_port->dain[3]<<24));
	/* DAI_DI0 | DAI_DI1 | DAI_DI2 | DAI_DI3 */
	tca_writel(reg_value, pdai_reg + PCFG_OFFSET + PCFG0);

	reg_value = (i2s_port->clk[1]|(i2s_port->clk[2]<<8)|(i2s_port->daout[0]<<16)|(i2s_port->daout[1]<<24));
	/* DAI_BCLK | DAI_LRCK | DAI_DO0 | DAI_DO1 */
	tca_writel(reg_value, pdai_reg + PCFG_OFFSET + PCFG1);

	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG2) & 0xFF000000; //FOR CDDI
	reg_value |= i2s_port->daout[2]|(i2s_port->daout[3]<<8)|(i2s_port->clk[0]<<16);
	/* DAI_DO2 | DAI_DO3 | DAI_MCLK */
	tca_writel(reg_value, pdai_reg + PCFG_OFFSET + PCFG2);

	/* For Debug
	   reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG0);
	   printk("PCFG0=0x%08x\n", reg_value);
	   reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG1);
	   printk("PCFG1=0x%08x\n", reg_value);
	   reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG2);
	   printk("PCFG2=0x%08x\n", reg_value);
	   */
}
EXPORT_SYMBOL_GPL(tca_i2s_port_mux);

/*****************************************************************************
 * Function Name : tca_dai_schmitt_set()
 ******************************************************************************
 * Desription    : DAI GPIO schmitt set for TCC802x.
 In I2S slave mode, for impove clock and data detection,
 schmitt input type is better.
 * Parameter     : GPIO port Number, schmitt enable
 * Return        :  
 ******************************************************************************/
void tca_i2s_schmitt_set(struct audio_i2s_port *port, bool en)
{
	struct audio_i2s_port *i2s_port = port;
	int i;

	if(en) {
		for(i=0; i<3; i++)
			tca_gpio_set(i2s_port->clk[i], 1, 1);
		for(i=0; i<4; i++)
			tca_gpio_set(i2s_port->dain[i], 1, 1);
	} else {
		for(i=0; i<3; i++)
			tca_gpio_set(i2s_port->clk[i], 1, 0);
		for(i=0; i<4; i++)
			tca_gpio_set(i2s_port->dain[i], 1, 0);
	}
}
EXPORT_SYMBOL_GPL(tca_i2s_schmitt_set);

void tca_spdif_port_mux(void *pDAIBaseAddr, char *port)
{
	void __iomem *pdai_reg = pDAIBaseAddr;
	char *spdif_port = port;
	unsigned int reg_value = 0;

	tca_audio_gpio_port_set(DAI_SPDIF_TYPE, spdif_port);	

	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG3) & 0xFFFF; //FOR CDDI
	reg_value |= (spdif_port[0] << 24) | (spdif_port[1] << 16);
	/* SPDIF_TX | SPDIF_RX | CD_LRCK | CD_BCLK */
	tca_writel(reg_value, pdai_reg + PCFG_OFFSET + PCFG3);
	/* For Debug
	   reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG3);
	   printk("PCFG3=0x%08x\n", reg_value);
	   */
}
EXPORT_SYMBOL_GPL(tca_spdif_port_mux);

 /*****************************************************************************
 * Function Name : tca_cdif_port_mux()
 ******************************************************************************
 * Desription    : DAI GPIO port mux for TCC802x
 * Parameter     : Audio ID, GPIO port Number
 * Return        :  
 ******************************************************************************/
void tca_cdif_port_mux(void *pDAIBaseAddr, struct audio_i2s_port *port)
{
	void __iomem *pdai_reg = pDAIBaseAddr;
	struct audio_i2s_port *cdif_port = port;
	unsigned int reg_value = 0;

	tca_audio_gpio_port_set(DAI_CDIF_TYPE, cdif_port);	

	/* CD_DI | DAI_MCLK | DAI_DO3 | DAI_DO2 */
	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG2) & 0x00FFFFFF; //FOR CDDI
	reg_value |= (cdif_port->dain[0] << 24);
	tca_writel(reg_value, pdai_reg + PCFG_OFFSET + PCFG2);

	/* SPDIF_TX | SPDIF_RX | CD_LRCK | CD_BCLK */
	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG3) & 0xFFFF0000; //FOR CD LRCK, CD BCLK
	reg_value |= (cdif_port->clk[1] | (cdif_port->clk[2]<<8));
	tca_writel(reg_value, pdai_reg + PCFG_OFFSET + PCFG3);

	#if 0	
	//For Debug
	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG0);
	printk("PCFG0=0x%08x\n", reg_value);
	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG1);
	printk("PCFG1=0x%08x\n", reg_value);
	reg_value = tca_readl(pdai_reg + PCFG_OFFSET + PCFG2);
	printk("PCFG2=0x%08x\n", reg_value);
    #endif
}
EXPORT_SYMBOL_GPL(tca_cdif_port_mux);
#endif

#if 0 //for debug
#define BASE_ADDR_IOCFG		(0x16051000)
#define IOCFG_HCLKEN0 0x00
#define IOCFG_HCLKEN1 0x04
#define IOCFG_HCLKEN2 0x08
#define IOCFG_HRSTEN0 0x0C
#define IOCFG_HRSTEN1 0x10
#define IOCFG_HRSTEN2 0x14

void tca_audio_reset(int index)
{
	void __iomem *piocfg_reg = ioremap(BASE_ADDR_IOCFG, 0x10);
	unsigned int reg_value=0;
	if(index == 0) {

		reg_value = tca_readl(piocfg_reg + IOCFG_HRSTEN0);
		reg_value &= ~(Hw22|Hw21); 
		tca_writel(reg_value, piocfg_reg + IOCFG_HRSTEN0);

		mdelay(10);	

		reg_value |= (Hw22|Hw21); 
		tca_writel(reg_value, piocfg_reg + IOCFG_HRSTEN0);

	}else if(index == 1) {

		reg_value = tca_readl(piocfg_reg + IOCFG_HRSTEN0);
		reg_value &= ~(Hw26|Hw25); 
		tca_writel(reg_value, piocfg_reg + IOCFG_HRSTEN0);

		mdelay(10);	

		reg_value |= (Hw26|Hw25); 
		tca_writel(reg_value, piocfg_reg + IOCFG_HRSTEN0);

	}else printk("invalid input[%d]\n", index);
	iounmap(piocfg_reg);
}
EXPORT_SYMBOL_GPL(tca_audio_reset);

void tca_iobus_dump(void)
{
	void __iomem *piocfg_reg = ioremap(BASE_ADDR_IOCFG, 0x20);
	unsigned int reg_value=0;
	reg_value = pcm_readl(piocfg_reg + IOCFG_HCLKEN0);
	printk("HCLKEN0[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(piocfg_reg + IOCFG_HCLKEN1);
	printk("HCLKEN1[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(piocfg_reg + IOCFG_HCLKEN2);
	printk("HCLKEN2[0x%08x]\n",		reg_value );

	reg_value = pcm_readl(piocfg_reg + IOCFG_HRSTEN0);
	printk("HRSTEN0[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(piocfg_reg + IOCFG_HRSTEN1);
	printk("HRSTEN1[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(piocfg_reg + IOCFG_HRSTEN2);
	printk("HRSTEN2[0x%08x]\n",		reg_value );
	iounmap(piocfg_reg);
}
EXPORT_SYMBOL_GPL(tca_iobus_dump);

void tca_adma_dump(void *pADMABaseAddr)
{
	void __iomem *pdai_reg = pADMABaseAddr;
	unsigned int reg_value=0;
#if 0
	reg_value = pcm_readl(pdai_reg + ADMA_RXDADAR);
	printk("RxDaDar[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_RXDAPARAM);
	printk("RxDaParam[0x%08x]\n",	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_RXDATCNT);
	printk("RxDaTCnt[0x%08x]\n",	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_RXDACDAR);
	printk("RxDaCdar[0x%08x]\n",	reg_value );
#endif
	printk("------I2S TX register------\n");
	reg_value = pcm_readl(pdai_reg + ADMA_TXDASAR);
	printk("TxDaSar[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TXDAPARAM);
	printk("TxDaParam[0x%08x]\n", 	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TXDATCNT);
	printk("TxDaTCnt[0x%08x]\n", 	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TXDACSAR);
	printk("TxDaCsar[0x%08x]\n", 	reg_value );
	printk("------SPDIF TX register------\n");
	reg_value = pcm_readl(pdai_reg + ADMA_TXSPSAR);
	printk("TxSpSar[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TXSPPARAM);
	printk("TxSpParam[0x%08x]\n",	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TXSPTCNT);
	printk("TxSpTCnt[0x%08x]\n",	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TXSPCSAR);
	printk("TxSpCsar[0x%08x]\n",	reg_value );
	printk("------ADMA CTL register------\n");
	reg_value = pcm_readl(pdai_reg + ADMA_RPTCTRL);
	printk("RptCtrl[0x%08x]\n", 	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_TRANSCTRL);
	printk("TransCtrl[0x%08x]\n", 	reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_CHCTRL);
	printk("ChCtrl[0x%08x]\n",		reg_value );
	reg_value = pcm_readl(pdai_reg + ADMA_INTSTATUS);
	printk("IntStatus[0x%08x]\n",	reg_value );
}
EXPORT_SYMBOL_GPL(tca_adma_dump);

void tca_dai_dump(void *pDAIBaseAddr)
{
	void __iomem *pdai_reg = pDAIBaseAddr;
	unsigned int reg_value=0;

	reg_value = i2s_readl(pdai_reg + I2S_DAMR);
	printk("DAMR[0x%08x]\n", reg_value );
	reg_value = i2s_readl(pdai_reg + I2S_DAVC);
	printk("DAVC[0x%08x]\n", reg_value );
	reg_value = i2s_readl(pdai_reg + I2S_MCCR0);
	printk("MCCR0[0x%08x]\n", reg_value );
	reg_value = i2s_readl(pdai_reg + I2S_MCCR1);
	printk("MCCR1[0x%08x]\n", reg_value );
}
EXPORT_SYMBOL_GPL(tca_dai_dump);

void tca_spdif_dump(void *pSPDIFBaseAddr)
{
	void __iomem *pdai_reg = pSPDIFBaseAddr;
	unsigned int reg_value=0;
	reg_value = spdif_readl(pdai_reg + SPDIF_TXVERSION);
	printk("TxVersion[0x%08x]\n", reg_value );
	reg_value = spdif_readl(pdai_reg + SPDIF_TXCONFIG);
	printk("TxConfig[0x%08x]\n", reg_value );
	reg_value = spdif_readl(pdai_reg + SPDIF_TXCHSTAT);
	printk("TxChStat[0x%08x]\n", reg_value );
	reg_value = spdif_readl(pdai_reg + SPDIF_TXINTMASK);
	printk("TxIntMask[0x%08x]\n", reg_value );
	reg_value = spdif_readl(pdai_reg + SPDIF_TXINTSTAT);
	printk("TxIntStat[0x%08x]\n", reg_value );
	reg_value = spdif_readl(pdai_reg + SPDIF_DMACFG);
	printk("DMACFG[0x%08x]\n", reg_value );
}
EXPORT_SYMBOL_GPL(tca_spdif_dump);
#endif
