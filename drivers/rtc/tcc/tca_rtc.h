// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCA_RTC_H
#define TCA_RTC_H

#define FROM_BCD(n)     ((((n) >> 4) * 10) + ((n) & 0xf))
#define TO_BCD(n)       ((((n) / 10) << 4) | ((n) % 10))
#define RTC_YEAR_DATUM  1980

#define Hw31		0x80000000
#define Hw30		0x40000000
#define Hw29		0x20000000
#define Hw28		0x10000000
#define Hw27		0x08000000
#define Hw26		0x04000000
#define Hw25		0x02000000
#define Hw24		0x01000000
#define Hw23		0x00800000
#define Hw22		0x00400000
#define Hw21		0x00200000
#define Hw20		0x00100000
#define Hw19		0x00080000
#define Hw18		0x00040000
#define Hw17		0x00020000
#define Hw16		0x00010000
#define Hw15		0x00008000
#define Hw14		0x00004000
#define Hw13		0x00002000
#define Hw12		0x00001000
#define Hw11		0x00000800
#define Hw10		0x00000400
#define Hw9		0x00000200
#define Hw8		0x00000100
#define Hw7		0x00000080
#define Hw6		0x00000040
#define Hw5		0x00000020
#define Hw4		0x00000010
#define Hw3		0x00000008
#define Hw2		0x00000004
#define Hw1		0x00000002
#define Hw0		0x00000001

#define RTCCON      0x00
#define INTCON      0x04
#define RTCALM      0x08
#define ALMSEC      0x0C
#define ALMMIN      0x10
#define ALMHOUR     0x14
#define ALMDATE     0x18
#define ALMDAY      0x1C
#define ALMMON      0x20
#define ALMYEAR     0x24
#define BCDSEC      0x28
#define BCDMIN      0x2C
#define BCDHOUR     0x30
#define BCDDATE     0x34
#define BCDDAY      0x38
#define BCDMON      0x3C
#define BCDYEAR     0x40
#define RTCIM       0x44
#define RTCPEND     0x48
#define RTCSTR      0x4C

#define BITSET(X, MASK)			((X) |= (u32)(MASK))
#define BITSCLR(X, SMASK, CMASK)	\
	((X) = ((((u32)(X)) | ((u32)(SMASK))) & ~((u32)(CMASK))))
#define BITCSET(X, CMASK, SMASK)	\
	((X) = ((((u32)(X)) & ~((u32)(CMASK))) | ((u32)(SMASK))))
#define BITCLR(X, MASK)			((X) &= ~((u32)(MASK)))
#define BITXOR(X, MASK)			((X) ^= (u32)(MASK))
#define ISZERO(X, MASK)			(!(((u32)(X)) & ((u32)(MASK))))
#define	ISSET(X, MASK)			((u32)(X) & ((u32)(MASK)))

//#define rtc_readl	__raw_readl
//#define rtc_writel	__raw_writel

struct rtctime {
	unsigned int wYear;
	unsigned int wMonth;
	unsigned int wDayOfWeek;
	unsigned int wDay;
	unsigned int wHour;
	unsigned int wMinute;
	unsigned int wSecond;
	unsigned int wMilliseconds;
};

void tca_rtc_gettime(void *pRTC, struct rtctime *pTime);
void tca_rtc_settime(void *pRTC, struct rtctime *pTime);
unsigned int	tca_rtc_checkvalidtime(void *devbaseaddresss);

#ifdef __cplusplus
extern
"C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* TCA_RTC_H */

