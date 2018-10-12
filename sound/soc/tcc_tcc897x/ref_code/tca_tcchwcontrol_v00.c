
/****************************************************************************
 *   FileName    : tca_tcchwcontrol.c
 *   Description : This is only for reference. This codes is old version.
 ****************************************************************************
 *
 *   TCC Version 0.0
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

#include <asm/mach-types.h>

#include "tca_audio_hw.h"
#include "tca_tcchwcontrol.h"


/* System revision Info */
extern unsigned int system_rev; 

/*****************************************************************************
 * Function Name : tca_dma_clrstatus()
 ******************************************************************************
 * Desription    :
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
unsigned int tca_dma_clrstatus(void *pADMABaseAddr, unsigned int nDmanum)
{
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;
    if (nDmanum) {
	//If you use this register to write, set interrupt status is cleared.
	pStrADMABaseReg->IntStatus |= (Hw6 | Hw2);
    } 
    else 
    {
	//If you use this register to write, set interrupt status is cleared.
	pStrADMABaseReg->IntStatus |= (Hw4 | Hw0);
    }

    return 0/*SUCCESS*/;
}

/*****************************************************************************
 * Function Name : tca_irq_getnumber()
 ******************************************************************************
 * Desription    :
 * Parameter     : 
 * Return        : DMA number 
 ******************************************************************************/
unsigned int tca_irq_getnumber(void)
{
#if 1 // defined(CONFIG_ARCH_TCC892X) || defined(CONFIG_ARCH_TCC893X)
    return INT_ADMA0;
#else
    return IRQ_ADMA;
#endif
}

/*****************************************************************************
 * Function Name : tca_dma_getstatus()
 ******************************************************************************
 * Desription    : dma status check
 * Parameter     : 
 * Return        :  
 ******************************************************************************/
unsigned int tca_dma_getstatus(void *pADMABaseAddr, unsigned int nDmanum)
{
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;
    return nDmanum ? (pStrADMABaseReg->IntStatus & (Hw6 | Hw2)) : (pStrADMABaseReg->IntStatus & (Hw4 | Hw0));
}

/*****************************************************************************
 * Function Name : tca_dma_setsrcaddr()
 ******************************************************************************
 * Desription    : dma set source address
 * Parameter     : dma number
 * Return        : address
 ******************************************************************************/
unsigned int tca_dma_setsrcaddr(void *pADMABaseAddr, unsigned int DADONum, unsigned int nDmanum, unsigned int nAddr)	
{
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;

    switch(DADONum)
    {
	case 0:
	    pStrADMABaseReg->TxDaSar = nAddr;
	    break;
	case 1:
	    pStrADMABaseReg->TxDaSar1 = nAddr;
	    break;
	case 2:
	    pStrADMABaseReg->TxDaSar2 = nAddr;
	    break;
	case 3:
	    pStrADMABaseReg->TxDaSar3 = nAddr;
	    break;
	default:
	    break;
    }

    return 0/*SUCCESS*/;
}

/*****************************************************************************
 * Function Name : tca_dma_setdestaddr()
 ******************************************************************************
 * Desription	: set dest address
 * Parameter 	: dma number, dest address
 * Return		: SUCCESS 
 ******************************************************************************/
unsigned int tca_dma_setdestaddr(void *pADMABaseAddr, unsigned int DADINum, unsigned int nDmanum, unsigned int nAddr)	
{
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;	

    switch(DADINum)
    {
	case 0:
	    pStrADMABaseReg->RxDaDar = nAddr;
	    break;
	case 1:
	    pStrADMABaseReg->RxDaDar1 = nAddr;
	    break;
	case 2:
	    pStrADMABaseReg->RxDaDar2 = nAddr;
	    break;
	case 3:
	    pStrADMABaseReg->RxDaDar3 = nAddr;
	    break;
	default:
	    break;
    }
    return 0/*SUCCESS*/;
}

/*****************************************************************************
 * Function Name : tca_dma_control()
 ******************************************************************************
 * Desription    : dma input control
 * Parameter     : mode 1: on, 0:off
 * Return        : success(SUCCESS) 
 ******************************************************************************/
unsigned int tca_dma_control(void *pADMABaseAddr, void *pADMADAIBaseAddr, unsigned int nMode, unsigned int nDmanum, unsigned int nInMode)				
{  
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;

    if(nMode) 
    { //run
	tca_i2s_start(pADMADAIBaseAddr, nInMode);
#if defined(_WM8581_INCLUDE_)		// CONFIG_SND_SOC_TCC_MULTICHANNEL
	pStrADMABaseReg->RptCtrl = Hw26|Hw25|Hw24;//DAI Buffer Threshod is set Multi-Port
#else
	pStrADMABaseReg->RptCtrl = 0;//Initialize repeat control register
#endif
	if(nInMode)//input
	{
	    pStrADMABaseReg->ChCtrl |= 0
		| Hw30 //DMA Channel enable of DAI Rx
		| Hw14 //Width of Audio Data of DAI Rx 16bites
		| Hw2; //Interrupt enable of DAI Rx
	}
	else//output
	{
	    pStrADMABaseReg->ChCtrl |= 0
		| Hw28 //DMA Channel enable of DAI Tx
#if defined(_WM8581_INCLUDE_) // CONFIG_SND_SOC_TCC_MULTICHANNEL
		| Hw21 | Hw20//DMA Multi channel select : DAI0, DAI1, DAI2, DAI3
#endif
		| Hw12	//Width of Audio Data of DAI Tx 16bites
		| Hw0; //Interrupt enable of DAI Tx
	}
    }
    else 
    {
	tca_i2s_stop(pADMADAIBaseAddr, nInMode);
	if(nInMode)//input
	{
	    pStrADMABaseReg->ChCtrl &= ~Hw30; //DMA Channel disable of DAI Rx
	}
	else//output
	{
	    pStrADMABaseReg->ChCtrl &= ~Hw28; //DMA Channel disable of DAI Tx
	}
    }
    return 0/*SUCCESS*/;
}

/*****************************************************************************
 * Function Name : tca_i2s_setregister()
 ******************************************************************************
 * Desription    : set dai register
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
unsigned int tca_i2s_setregister(void *pADMADAIBaseAddr, unsigned int nRegval)
{
    ADMADAI*pStrADMADAIBaseReg = (ADMADAI *)pADMADAIBaseAddr;
    pStrADMADAIBaseReg->DAMR = nRegval;
    return 0;
}

/*****************************************************************************
 * Function Name : tca_i2s_getregister()
 ******************************************************************************
 * Desription    : get dai register
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
unsigned int tca_i2s_getregister(void *pADMADAIBaseAddr)
{
    ADMADAI *pStrADMADAIBaseReg = (ADMADAI *)pADMADAIBaseAddr;
    return (pStrADMADAIBaseReg->DAMR);
}

/*****************************************************************************
 * Function Name : tca_i2s_dai_init()
 ******************************************************************************
 * Desription    : i2s init
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
unsigned int tca_i2s_dai_init(void *pADMADAIBaseAddr)
{
    ADMADAI *pStrADMADAIBaseReg = (ADMADAI *)pADMADAIBaseAddr;

    //Not used volume control
    pStrADMADAIBaseReg->DAVC = 0;

    pStrADMADAIBaseReg->DAMR = 0
	| Hw31 	//BCLK direct at master mode
	| Hw30 	//LRCLK direct at master mode	
#if defined(_WM8581_INCLUDE_) /* CONFIG_SND_SOC_TCC_MULTICHANNEL */
	| Hw29	//DAI Buffer Threshold Enable
	| Hw28	//Multi-Port Enable
#endif

#ifdef I2S_24BIT_MODE
		| Hw21	//DAI Rx shift Bit-pack LSB and 24bit mode
		| Hw19	//DAI Tx shift Bit-pack LSB and 24bit mode
#else
		| Hw21| Hw20 //DAI Rx shift Bit-pack LSB and 16bit mode
		| Hw19| Hw18 //DAI Tx shift Bit-pack LSB and 16bit mode
#endif

		| Hw15	//DAI Enable
		| Hw11 	//DAI System Clock master select
		| Hw10 	//DAI Bit Clock master select
		| Hw9	//DAI Frame clock(LRCLK) master selcet 
		| Hw5	//DAI LRCLK(64fs->fs)
		;

    return(0);
}

/*****************************************************************************
 * Function Name : tca_i2s_initoutput()
 ******************************************************************************
 * Desription    : i2s init output
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
void tca_i2s_initoutput(void *pADMABaseAddr, unsigned int nOutputDma, unsigned int AUDIO_DMA_PAGE_SIZE, unsigned int SAMPLERATE )
{
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;
    unsigned int bitSet=0x0;
    unsigned uSize;
    unsigned int DMABuffer = 0;

    //DMA Channel enable of DAI Tx
    pStrADMABaseReg->ChCtrl &= ~Hw28;

    DMABuffer = 0xFFFFFF00 / (((AUDIO_DMA_PAGE_SIZE*2) >> 4)<<8);
    DMABuffer = DMABuffer * (((AUDIO_DMA_PAGE_SIZE*2) >> 4)<<8);

    uSize = AUDIO_DMA_PAGE_SIZE*2;

    // DAI Tx (Right) Source Address
    pStrADMABaseReg->TxDaParam = DMABuffer | 4; //(4|0xFFFFFE00);	

    // ADMA Count Reginster setting
    pStrADMABaseReg->TxDaTCnt = (((AUDIO_DMA_PAGE_SIZE)>>0x2)>>0x2) - 1;

    // Tx Transfer Control Refister Setting	
    bitSet = ( HwZERO
	    //Tx Trigger Type is edge-trigger
	    |Hw28 //DMA transfer begins from current source/destination address
	    //|Hw24 //Issue Locked Transfer of DAI Tx DMA
	    |Hw16 //Enable repeat mode control of DAI Tx DMA
	    |Hw9/*|Hw8*/ //Burst size of DAI Tx DMA is 4 cycle
	    |Hw1|Hw0 //Word size of DAI Tx DMA is 32bit
	    );

    pStrADMABaseReg->TransCtrl |= bitSet;
#if defined(_WM8581_INCLUDE_)		// CONFIG_SND_SOC_TCC_MULTICHANNEL
    pStrADMABaseReg->RptCtrl = Hw26|Hw25|Hw24;//DAI Buffer Threshod is set Multi-Port
#else
    pStrADMABaseReg->RptCtrl = 0;
#endif

    /* ADMA Repeat register setting */
    //	BITCLR(HwRptCtrl,HwRptCtrl_DRI_EN);	//DMA Interrupt is occurred when the end of each Repeated DMAoperation.

    // Tx channel Control Register Setting	
    //Start DMA
    bitSet = ( HwZERO
	    |Hw0 //Interrupt Enable of DAI Tx	
	    );

    pStrADMABaseReg->ChCtrl |= bitSet;
}

/*****************************************************************************
 * Function Name : tca_i2s_deinit()
 ******************************************************************************
 * Desription    : i2s deinit
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/

unsigned int tca_i2s_deinit(void *pADMADAIBaseAddr)
{
    //----- 1. Stop the I2S clocks -----
    tca_i2s_stop(pADMADAIBaseAddr, 0);
    tca_i2s_stop(pADMADAIBaseAddr, 1);

    return 0/*SUCCESS*/;
}


/*****************************************************************************
 * Function Name : tca_i2s_start()
 ******************************************************************************
 * Desription    :
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
void tca_i2s_start(void *pADMADAIBaseAddr, unsigned int nMode)
{
    ADMADAI *pStrADMADAIBaseReg = (ADMADAI *)pADMADAIBaseAddr;

    if(nMode) //input
    {
	//ADMADAI Receiver enable
	pStrADMADAIBaseReg->DAMR |= Hw13;
    }
    else
    {
	//ADMADAI Transmitter enable
	pStrADMADAIBaseReg->DAMR |= Hw14;
    }


}

/*****************************************************************************
 * Function Name : tca_i2s_stop()
 ******************************************************************************
 * Desription    :
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
void tca_i2s_stop(void *pADMADAIBaseAddr, unsigned int nMode)
{
    ADMADAI *pStrADMADAIBaseReg = (ADMADAI *)pADMADAIBaseAddr;

    if(nMode)
    {
	//DAI Receiver disable
	pStrADMADAIBaseReg->DAMR &= ~Hw13;
    }
    else
    {
	//DAI Transmitter disable
	pStrADMADAIBaseReg->DAMR &= ~Hw14;
    }

}

/*****************************************************************************
 * Function Name : tca_i2s_initinput()
 ******************************************************************************
 * Desription    :
 * Parameter     : 
 * Return        : success(SUCCESS) 
 ******************************************************************************/
void tca_i2s_initinput(void *pADMABaseAddr, unsigned int nInputDma, unsigned int AUDIO_DMA_IN_PAGE_SIZE, unsigned int SAMPLERATE)
{
    ADMA *pStrADMABaseReg = (ADMA *)pADMABaseAddr;	
    unsigned int bitSet=0x0;
    unsigned uSize;
    unsigned int DMABuffer = 0;

    //DMA Channel enable of DAI Rx
    pStrADMABaseReg->ChCtrl &= ~Hw30;//

    DMABuffer = 0xFFFFFF00 / (((AUDIO_DMA_IN_PAGE_SIZE*2) >> 4)<<8);
    DMABuffer = DMABuffer * (((AUDIO_DMA_IN_PAGE_SIZE*2) >> 4)<<8);

    uSize = AUDIO_DMA_IN_PAGE_SIZE*2;

    // DAI Rx (Right) Source Address
    pStrADMABaseReg->RxDaParam = DMABuffer | 4; //(4|0xFFFFFE00);	

    // ADMA Count Reginster setting
    pStrADMABaseReg->RxDaTCnt = (((AUDIO_DMA_IN_PAGE_SIZE)>>0x02)>>0x02) - 1;

    //Rx Transfer Control Refister Setting 
    bitSet = ( HwZERO
	    //Rx Trigger Type is edge-trigger
	    |Hw29 //DMA transfer begins from current source/destination address
	    //|Hw26 //Issue Locked Transfer of DAI Rx DMA
	    |Hw18 //Enable repeat mode control of DAI Rx DMA
	    |Hw13 //Burst size of DAI Rx DMA is 4 cycle
	    |(Hw5|Hw4) //Word size of DAI Rx DMA is 32bit
	    );

    pStrADMABaseReg->TransCtrl |= bitSet;
#if defined(_WM8581_INCLUDE_)		// CONFIG_SND_SOC_TCC_MULTICHANNEL
    pStrADMABaseReg->RptCtrl = Hw26|Hw25|Hw24;//DAI Buffer Threshod is set Multi-Port
#else
    pStrADMABaseReg->RptCtrl = 0;
#endif

    /* ADMA Repeat register setting */
    //BITCLR(HwRptCtrl,HwRptCtrl_DRI_EN); //DMA Interrupt is occurred when the end of each Repeated DMAoperation.

    // Rx channel Control Register Setting	
    //Start DMA
    bitSet = ( HwZERO
	    |Hw2//Interrupt Enable of DAI Rx	
	    );

    pStrADMABaseReg->ChCtrl |= bitSet; 
}

