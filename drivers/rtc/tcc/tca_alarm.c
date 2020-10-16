// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include "tca_alarm.h"
//#include <asm/mach-types.h>

#define rtc_readl   __raw_readl
#define rtc_writel  __raw_writel

int tca_alarm_gettime(void __iomem *pRTC, struct rtctime *pTime)
{
	unsigned int uCON;

	//pTime->wSecond                        = pRTC->ALMSEC;
	//pTime->wMinute                        = pRTC->ALMMIN;
	//pTime->wHour                  = pRTC->ALMHOUR;
	//pTime->wDayOfWeek             = pRTC->ALMDAY;
	//pTime->wDay                           = pRTC->ALMDATE;
	//pTime->wMonth                 = pRTC->ALMMON;
	//pTime->wYear                  = pRTC->ALMYEAR;
	//uCON = pRTC->RTCALM;
	pTime->wSecond = rtc_readl(pRTC + ALMSEC);
	pTime->wMinute = rtc_readl(pRTC + ALMMIN);
	pTime->wHour = rtc_readl(pRTC + ALMHOUR);
	pTime->wDayOfWeek = rtc_readl(pRTC + ALMDAY);
	pTime->wDay = rtc_readl(pRTC + ALMDATE);
	pTime->wMonth = rtc_readl(pRTC + ALMMON);
	pTime->wYear = rtc_readl(pRTC + ALMYEAR);
	uCON = rtc_readl(pRTC + RTCALM);

	/* Interrupt Block Write Disable */
	rtc_writel(rtc_readl(pRTC + INTCON) & ~Hw0, pRTC + INTCON);
	/* RTC Register write Disable */
	rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw1, pRTC + RTCCON);

	/* Second */
	if (ISZERO(uCON, Hw0)) {
		pTime->wSecond = (u32)0;
	} else {
		pTime->wSecond = FROM_BCD(pTime->wSecond);
	}

	/* Minute */
	if (ISZERO(uCON, Hw1)) {
		pTime->wMinute = (u32)0;
	} else {
		pTime->wMinute = FROM_BCD(pTime->wMinute);
	}

	/* Hour */
	if (ISZERO(uCON, Hw2)) {
		pTime->wHour = (u32)0;
	} else {
		pTime->wHour = FROM_BCD(pTime->wHour);
	}

	/* date */
	if (ISZERO(uCON, Hw3)) {
		pTime->wDay = (u32)0;
	} else {
		pTime->wDay = FROM_BCD(pTime->wDay);
	}

	/* month */
	if (ISZERO(uCON, Hw5)) {
		pTime->wMonth = (u32)0;
	} else {
		pTime->wMonth = FROM_BCD(pTime->wMonth);
	}

	/* year */
	if (ISZERO(uCON, Hw6)) {
		pTime->wYear = (u32)0;
	} else {
		/* YearFix : hjbae */
		pTime->wYear = (FROM_BCD(pTime->wYear >> (u32)8) * (u32)100)
				+ FROM_BCD(pTime->wYear & 0x00FF);
	}

	/* weekdays */
	if (ISZERO(uCON, Hw4)) {
		pTime->wDayOfWeek = 1;
	} else {
		pTime->wDayOfWeek = FROM_BCD(pTime->wDayOfWeek);
	}

	//pRTC->RTCALM  = uCON;
	rtc_writel(uCON, pRTC + RTCALM);

	return 0;
}

int tca_alarm_settime(void __iomem *pRTC, struct rtctime *pTime)
{
	unsigned int uCON;

	/* RTC Register write enabled */
	rtc_writel(rtc_readl(pRTC + RTCCON) | Hw1, pRTC + RTCCON);
	// Interrupt Block & Write Enable
	rtc_writel(rtc_readl(pRTC + INTCON) | Hw0, pRTC + INTCON);

	uCON = 0xEF;	/* Not wDayOfWeek */

	/* Second */
	if (pTime->wSecond > (u32)59) {
		BITCLR(uCON, Hw0);	//HwRTCALM_SECEN_EN
	} else {
		rtc_writel(TO_BCD(pTime->wSecond), pRTC + ALMSEC);
	}

	/* Minute */
	if (pTime->wMinute > (u32)59) {
		BITCLR(uCON, Hw1);	//HwRTCALM_MINEN_EN
	} else {
		//pRTC->ALMMIN  = TO_BCD( pTime->wMinute );
		rtc_writel(TO_BCD(pTime->wMinute), pRTC + ALMMIN);
	}

	/* Hour */
	if (pTime->wHour > 23) {
		BITCLR(uCON, Hw2);	//HwRTCALM_HOUREN_EN
	} else {
		//pRTC->ALMHOUR = TO_BCD( pTime->wHour );
		rtc_writel(TO_BCD(pTime->wHour), pRTC + ALMHOUR);
	}

	/* Date */
	if (pTime->wDay > 31 || pTime->wDay < 1) {
		BITCLR(uCON, Hw3);	//HwRTCALM_DATEEN_EN
	} else {
		//pRTC->ALMDATE = TO_BCD( pTime->wDay );
		rtc_writel(TO_BCD(pTime->wDay), pRTC + ALMDATE);
	}

	/* month */
	if (pTime->wMonth > 12 || pTime->wMonth < 1) {
		BITCLR(uCON, Hw5);	//HwRTCALM_MONEN_EN
	} else {
		//pRTC->ALMMON  = TO_BCD( pTime->wMonth );
		rtc_writel(TO_BCD(pTime->wMonth), pRTC + ALMMON);
	}

	/* year */
	if (pTime->wYear > 2099 || pTime->wYear < 1900) {
		BITCLR(uCON, Hw6);	//HwRTCALM_YEAREN_EN
	} else {
		rtc_writel((TO_BCD(pTime->wYear / 100) << 8) |
			   TO_BCD(pTime->wYear % 100), pRTC + ALMYEAR);
	}

	/* Day */
	if (pTime->wDayOfWeek > 6) {
		BITCLR(uCON, Hw4);	//HwRTCALM_DAYEN_EN
	} else {
		//pRTC->ALMDAY  = pTime->wDayOfWeek+1;
		rtc_writel(pTime->wDayOfWeek + 1, pRTC + ALMDAY);
	}

	/* Enable ALARM */
	rtc_writel(uCON, pRTC + RTCALM);

	/* Active High / Level Trigger / Enable Alarm Interrupt */
	rtc_writel(rtc_readl(pRTC + RTCIM) | Hw2 | Hw1 | Hw0, pRTC + RTCIM);
	/* Clear Alarm Wake-Up Pin Output */
	rtc_writel(0x0, pRTC + RTCPEND);
	/* Clear Alarm Wake-Up Count value */
	rtc_writel(rtc_readl(pRTC + RTCSTR) & ~(Hw3 | Hw2 | Hw1 | Hw0),
		   pRTC + RTCSTR);
	/* Clear Alarm, wake-up interrupt pending */
	rtc_writel(rtc_readl(pRTC + RTCSTR) | (Hw7 | Hw6), pRTC + RTCSTR);
	/* Alarm Interrupt Output Enable (Hw6) Wake-up Output Enable (Hw7) */
	rtc_writel(rtc_readl(pRTC + RTCCON) | (Hw7 | Hw6), pRTC + RTCCON);

	/* If protection enable bit is Disabled */
	if (!(rtc_readl(pRTC + INTCON) & Hw15))	{
		/* RTC Start bit - Halt */
		rtc_writel(rtc_readl(pRTC + RTCCON) | Hw0, pRTC + RTCCON);
		/* protect bit - enable */
		rtc_writel(rtc_readl(pRTC + INTCON) | Hw15, pRTC + INTCON);
		/* RTC Start bit - Run */
		rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw0, pRTC + RTCCON);
	}

	/* Interrupt Block Write Enable bit - disable */
	rtc_writel(rtc_readl(pRTC + INTCON) & ~Hw0, pRTC + INTCON);
	/* RTC write enable bit - disable */
	rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw1, pRTC + RTCCON);

	return 0;
}

int tca_alarm_setint(void __iomem *rtcbaseaddress)
{
	struct rtctime lpTime;

	//Set Alarm
	tca_rtc_gettime(rtcbaseaddress, (struct rtctime *)&lpTime);

	if (lpTime.wSecond < 55)
		lpTime.wSecond += 5;

	tca_alarm_settime(rtcbaseaddress, (struct rtctime *)&lpTime);

	return 0;
}

/* Disable the RTC Alarm during the power off state */
int tca_alarm_disable(void __iomem *pRTC)
{
	if (pRTC == NULL) {
		pr_err("[ERROR][tcc-rtc]failed RTC ioremap()\n");
	} else {
		/*
		 * Disable Wake Up Interrupt Output(Hw7) and
		 * Alarm Interrupt Output(Hw6)
		 */
		rtc_writel(rtc_readl(pRTC + RTCCON) & ~(Hw7 | Hw6),
			   pRTC + RTCCON);

		/* Enable - RTC Write */
		rtc_writel(rtc_readl(pRTC + RTCCON) | Hw1, pRTC + RTCCON);
		/* Enable - Interrupt Block Write */
		rtc_writel(rtc_readl(pRTC + INTCON) | Hw0, pRTC + INTCON);

		/* Disable - Alarm Control */
		rtc_writel(rtc_readl(pRTC + RTCALM) & ~(0x000000ff),
			   pRTC + RTCALM);
		/* Power down mode, Active HIGH, Disable alarm interrupt */
		rtc_writel(rtc_readl(pRTC + RTCIM) & (~(0x0000000f) | Hw2),
			   pRTC + RTCIM);

		/* Disable - Interrupt Block Write */
		rtc_writel(rtc_readl(pRTC + INTCON) & ~Hw0, pRTC + INTCON);
		/* Disable - RTC Write */
		rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw1, pRTC + RTCCON);
	}

	return 0;
}
