
/****************************************************************************
 *   FileName    : tca_rtc.C
 *   Description : 
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips, Inc.
 *   ALL RIGHTS RESERVED
 *
 ****************************************************************************/


/*****************************************************************************
*
* Header Files Include
*
******************************************************************************/
#include "tca_rtc.h"
#include <asm/io.h>

/*****************************************************************************
*
* Defines
*
******************************************************************************/

/*****************************************************************************
*
* structures
*
******************************************************************************/


/*****************************************************************************
*
* Variables
*
******************************************************************************/



/*****************************************************************************
*
* Functions
*
******************************************************************************/


/*****************************************************************************
* Function Name : tca_rtc_gettime()
******************************************************************************/
#define rtc_readl   __raw_readl
#define rtc_writel  __raw_writel
void tca_rtc_gettime(void __iomem *pRTC,rtctime *pTime)
{
	unsigned int data;
	unsigned short seconds;

	//void __iomem *pRTC = (void __iomem*)devbaseaddresss;
	
   // Enable RTC control.
    //BITSET(pRTC->RTCCON, Hw1); // RTC write enable bit - enabled
	//BITSET(pRTC->INTCON, Hw0); // Interrupt Block Write Enable bit - enable
  	//BITCLR(pRTC->RTCCON, Hw4); // RTC clock count set bit - no reset
	rtc_writel( rtc_readl(pRTC+RTCCON) | Hw1, pRTC+RTCCON);
	rtc_writel( rtc_readl(pRTC+INTCON) | Hw0, pRTC+INTCON);
	rtc_writel( rtc_readl(pRTC+RTCCON) & ~Hw4, pRTC+RTCCON);

    do {
		
		//data = (pRTC->BCDSEC & 0x7f);
		data = rtc_readl(pRTC+BCDSEC) & 0x7f;
		seconds = FROM_BCD(data);

		//data = (pRTC->BCDYEAR & 0xFFFF);
		data = rtc_readl(pRTC+BCDYEAR) & 0xFFFF;
		//pTime->wYear = FROM_BCD(data); // Start from 1980
		pTime->wYear = (FROM_BCD( data>>8 )*100) + FROM_BCD( data&0x00FF );	// YearFix : hjbae

		//data = (pRTC->BCDMON & 0x1f);
		data = rtc_readl(pRTC+BCDMON) & 0x1f;
		pTime->wMonth = FROM_BCD(data);

		//data = (pRTC->BCDDATE & 0x3f);
		data = rtc_readl(pRTC+BCDDATE) & 0x3f;
		pTime->wDay = FROM_BCD(data);

		//data = (pRTC->BCDDAY & 0x07);
		data = rtc_readl(pRTC+BCDDAY) & 0x07;
		pTime->wDayOfWeek = FROM_BCD(data);

		//data = (pRTC->BCDHOUR & 0x3f);
		data = rtc_readl(pRTC+BCDHOUR) & 0x3f;
		pTime->wHour = FROM_BCD(data);

		//data = (pRTC->BCDMIN & 0x7f);
		data = rtc_readl(pRTC+BCDMIN) & 0x7f;
		pTime->wMinute = FROM_BCD(data);

		//data = (pRTC->BCDSEC & 0x7f);
		data = rtc_readl(pRTC+BCDSEC) & 0x7f;
		pTime->wSecond = FROM_BCD(data);

		pTime->wMilliseconds = 0;
    } while (pTime->wSecond != seconds); 

	//BITCLR(pRTC->INTCON, Hw0); // Interrupt Block Write Enable bit - disable
	rtc_writel( rtc_readl(pRTC+INTCON) & ~Hw0, pRTC+INTCON);
	//BITCLR(pRTC->RTCCON, Hw1); // RTC write enable bit - disable
	rtc_writel( rtc_readl(pRTC+RTCCON) & ~Hw1, pRTC+RTCCON);
}

/*****************************************************************************
* Function Name : tca_rtc_settime()
******************************************************************************/
void tca_rtc_settime(void __iomem *pRTC, rtctime *pTime)
{
	//PRTC	pRTC = (PRTC)devbaseaddresss;
	
	//BITSET(pRTC->RTCCON, Hw1);	
	rtc_writel( rtc_readl(pRTC+RTCCON) | Hw1, pRTC+RTCCON); // RTC Register write enabled
	//BITSET(pRTC->INTCON, Hw0);	// Interrupt Block Write Enable
	rtc_writel( rtc_readl(pRTC+INTCON) | Hw0, pRTC+INTCON); // Interrupt Block Write Enable

     // Enable RTC control.
    //BITSET(pRTC->RTCCON, Hw0);	//RTC Start Halt
    //BITSET(pRTC->RTCCON, Hw4);	//RTC Clock Count Reset
	//BITCLR(pRTC->RTCCON, Hw4);	//RTC Clock Count No Reset
	//BITSET(pRTC->RTCCON, Hw4);	 //RTC Clock Count Reset
	rtc_writel( rtc_readl(pRTC+RTCCON) | Hw0, pRTC+RTCCON); //RTC Start Halt
	rtc_writel( rtc_readl(pRTC+RTCCON) | Hw4, pRTC+RTCCON); 	//RTC Clock Count Reset
	rtc_writel( rtc_readl(pRTC+RTCCON) & ~Hw4, pRTC+RTCCON); //RTC Clock Count No Reset
	rtc_writel( rtc_readl(pRTC+RTCCON) | Hw4, pRTC+RTCCON); //RTC Clock Count Reset

	//BITSET(pRTC->INTCON, Hw15); //RTC Protection - enable
	//BITCLR(pRTC->INTCON, Hw15); //RTC Protection - disable
	rtc_writel( rtc_readl(pRTC+INTCON) | Hw15, pRTC+INTCON); //RTC Protection - enable
	rtc_writel( rtc_readl(pRTC+INTCON) & ~Hw15, pRTC+INTCON); //RTC Protection - disable

	rtc_writel(TO_BCD(pTime->wSecond), pRTC+BCDSEC);
	//pRTC->BCDSEC	=	TO_BCD(pTime->wSecond);
	//pRTC->BCDMIN	=	TO_BCD(pTime->wMinute);
	rtc_writel(TO_BCD(pTime->wMinute),pRTC+BCDMIN);
	//pRTC->BCDHOUR	=	TO_BCD(pTime->wHour);
	rtc_writel(TO_BCD(pTime->wHour), pRTC+BCDHOUR);
	//pRTC->BCDDAY	=	TO_BCD(pTime->wDayOfWeek);
	rtc_writel(TO_BCD(pTime->wDayOfWeek),pRTC+BCDDAY);
	//pRTC->BCDDATE	=	TO_BCD(pTime->wDay);
	rtc_writel(TO_BCD(pTime->wDay), pRTC+BCDDATE);
	//pRTC->BCDMON	=	TO_BCD(pTime->wMonth);
	rtc_writel(TO_BCD(pTime->wMonth), pRTC+BCDMON);
	//pRTC->BCDYEAR	=	TO_BCD(pTime->wYear);
	//pRTC->BCDYEAR	= (TO_BCD( pTime->wYear/100 )<<8) | TO_BCD( pTime->wYear%100 );	// YearFix : hjbae
	rtc_writel((TO_BCD( pTime->wYear/100 )<<8) | TO_BCD( pTime->wYear%100 ),pRTC+BCDYEAR);	// YearFix : hjbae

	//BITSET(pRTC->INTCON, Hw15); // RTC Protection - Enable
	//BITCLR(pRTC->RTCCON, Hw0);	// RTC Start RUN
	rtc_writel( rtc_readl(pRTC+INTCON) | Hw15, pRTC+INTCON); //RTC Protection - Enable
	rtc_writel( rtc_readl(pRTC+RTCCON) & ~Hw0, pRTC+RTCCON); 	// RTC Start RUN

	//BITCLR(pRTC->INTCON, Hw0); 	// Interrupt Block Write - disable
	//BITCLR(pRTC->RTCCON, Hw1);	// RTC Write - Disable
	rtc_writel( rtc_readl(pRTC+INTCON) & ~Hw0, pRTC+INTCON); // Interrupt Block Write - disable
	rtc_writel( rtc_readl(pRTC+RTCCON) & ~Hw1, pRTC+RTCCON); // RTC Write - Disable
}

/*****************************************************************************
* Function Name : tca_rtc_checkvalidtime()
******************************************************************************/
unsigned int	tca_rtc_checkvalidtime(void __iomem *devbaseaddresss)
{
	unsigned	IsNeedMending;
	rtctime		pTimeTest;

	tca_rtc_gettime(devbaseaddresss, &pTimeTest);

	//-------------------------------------------
	// Conversion to Dec and Check validity
	//-------------------------------------------
	IsNeedMending = 0;

	/* Second */
	if (pTimeTest.wSecond > 59)
	{
		IsNeedMending	= 1;
	}

	/* Minute */
	if ( pTimeTest.wMinute > 59 )
	{
		IsNeedMending	= 1;
	}

	/* Hour */
	if ( pTimeTest.wHour > 23 )
	{
		IsNeedMending	= 1;
	}

	/* Date */ // 1~31
	if ( ( pTimeTest.wDay < 1 ) || ( pTimeTest.wDay > 31 ) )
	{
		IsNeedMending	= 1;
	}

	/* Day */ // Sunday ~ Saturday
	if ( pTimeTest.wDayOfWeek > 6 )
	{
		IsNeedMending	= 1;
	}

	/* Month */
	if ( ( pTimeTest.wMonth < 1 ) || ( pTimeTest.wMonth > 12 ) )
	{
		IsNeedMending	= 1;
	}

	/* Year */
	if ( pTimeTest.wYear < 1980 || pTimeTest.wYear > 2037)
	{
		IsNeedMending	= 1;
	}

	return	IsNeedMending;
}

