// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include "tca_alarm.h"

#define rtc_readl       (__raw_readl)
#define rtc_writel      (__raw_writel)

void tca_alarm_gettime(void __iomem *pRTC, struct rtctime *pTime)
{
	u32 uCON;

	if (pTime == NULL) {
		return;
	}

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
				+ FROM_BCD(pTime->wYear & (u32)0xFF);
	}

	/* weekdays */
	if (ISZERO(uCON, Hw4)) {
		pTime->wDayOfWeek = 1;
	} else {
		pTime->wDayOfWeek = FROM_BCD(pTime->wDayOfWeek);
	}

	rtc_writel(uCON, pRTC + RTCALM);
}

void tca_alarm_settime(void __iomem *pRTC, struct rtctime *pTime)
{
	u32 uCON;

	if (pTime == NULL) {
		return;
	}

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
		rtc_writel(TO_BCD(pTime->wMinute), pRTC + ALMMIN);
	}

	/* Hour */
	if (pTime->wHour > (u32)23) {
		BITCLR(uCON, Hw2);	//HwRTCALM_HOUREN_EN
	} else {
		rtc_writel(TO_BCD(pTime->wHour), pRTC + ALMHOUR);
	}

	/* Date */
	if ((pTime->wDay > (u32)31) || (pTime->wDay < (u32)1)) {
		BITCLR(uCON, Hw3);	//HwRTCALM_DATEEN_EN
	} else {
		rtc_writel(TO_BCD(pTime->wDay), pRTC + ALMDATE);
	}

	/* month */
	if ((pTime->wMonth > (u32)12) || (pTime->wMonth < (u32)1)) {
		BITCLR(uCON, Hw5);	//HwRTCALM_MONEN_EN
	} else {
		rtc_writel(TO_BCD(pTime->wMonth), pRTC + ALMMON);
	}

	/* year */
	if ((pTime->wYear > (u32)2099) || (pTime->wYear < (u32)1900)) {
		BITCLR(uCON, Hw6);	//HwRTCALM_YEAREN_EN
	} else {
		rtc_writel((TO_BCD(pTime->wYear / (u32)100) << (u32)8) |
			   TO_BCD(pTime->wYear % (u32)100), pRTC + ALMYEAR);
	}

	/* Day */
	if (pTime->wDayOfWeek > (u32)6) {
		BITCLR(uCON, Hw4);	//HwRTCALM_DAYEN_EN
	} else {
		rtc_writel(pTime->wDayOfWeek + (u32)1, pRTC + ALMDAY);
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
	if ((rtc_readl(pRTC + INTCON) & Hw15) == (u32)0)	{
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
}

