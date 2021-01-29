// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCA_RTC_H
#define TCA_RTC_H

#define FROM_BCD(n)     ((((n) >> (u32)4) * (u32)10) + ((n) & (u32)0xf))
#define TO_BCD(n)       ((((n) / (u32)10) << (u32)4) | ((n) % (u32)10))
#define RTC_YEAR_DATUM  (1980)

#define Hw31            ((u32)0x80000000)
#define Hw30            ((u32)0x40000000)
#define Hw29            ((u32)0x20000000)
#define Hw28            ((u32)0x10000000)
#define Hw27            ((u32)0x08000000)
#define Hw26            ((u32)0x04000000)
#define Hw25            ((u32)0x02000000)
#define Hw24            ((u32)0x01000000)
#define Hw23            ((u32)0x00800000)
#define Hw22            ((u32)0x00400000)
#define Hw21            ((u32)0x00200000)
#define Hw20            ((u32)0x00100000)
#define Hw19            ((u32)0x00080000)
#define Hw18            ((u32)0x00040000)
#define Hw17            ((u32)0x00020000)
#define Hw16            ((u32)0x00010000)
#define Hw15            ((u32)0x00008000)
#define Hw14            ((u32)0x00004000)
#define Hw13            ((u32)0x00002000)
#define Hw12            ((u32)0x00001000)
#define Hw11            ((u32)0x00000800)
#define Hw10            ((u32)0x00000400)
#define Hw9             ((u32)0x00000200)
#define Hw8             ((u32)0x00000100)
#define Hw7             ((u32)0x00000080)
#define Hw6             ((u32)0x00000040)
#define Hw5             ((u32)0x00000020)
#define Hw4             ((u32)0x00000010)
#define Hw3             ((u32)0x00000008)
#define Hw2             ((u32)0x00000004)
#define Hw1             ((u32)0x00000002)
#define Hw0             ((u32)0x00000001)

#define RTCCON          ((u32)0x00)
#define INTCON          ((u32)0x04)
#define RTCALM          ((u32)0x08)
#define ALMSEC          ((u32)0x0C)
#define ALMMIN          ((u32)0x10)
#define ALMHOUR         ((u32)0x14)
#define ALMDATE         ((u32)0x18)
#define ALMDAY          ((u32)0x1C)
#define ALMMON          ((u32)0x20)
#define ALMYEAR         ((u32)0x24)
#define BCDSEC          ((u32)0x28)
#define BCDMIN          ((u32)0x2C)
#define BCDHOUR         ((u32)0x30)
#define BCDDATE         ((u32)0x34)
#define BCDDAY          ((u32)0x38)
#define BCDMON          ((u32)0x3C)
#define BCDYEAR         ((u32)0x40)
#define RTCIM           ((u32)0x44)
#define RTCPEND         ((u32)0x48)
#define RTCSTR          ((u32)0x4C)

#define BITSET(X, MASK)         ((X) |= (u32)(MASK))
#define BITSCLR(X, SMASK, CMASK)	\
	((X) = ((((u32)(X)) | ((u32)(SMASK))) & ~((u32)(CMASK))))
#define BITCSET(X, CMASK, SMASK)	\
	((X) = ((((u32)(X)) & ~((u32)(CMASK))) | ((u32)(SMASK))))
#define BITCLR(X, MASK)         ((X) &= ~((u32)(MASK)))
#define BITXOR(X, MASK)         ((X) ^= (u32)(MASK))
#define ISZERO(X, MASK)         ((((u32)(X)) & ((u32)(MASK))) == (u32)0)
#define	ISSET(X, MASK)          (((u32)(X) & ((u32)(MASK))) != (u32)0)

struct rtctime {
	u32 wYear;
	u32 wMonth;
	u32 wDayOfWeek;
	u32 wDay;
	u32 wHour;
	u32 wMinute;
	u32 wSecond;
	u32 wMilliseconds;
};

void tca_rtc_gettime(void *pRTC, struct rtctime *pTime);
void tca_rtc_settime(void *pRTC, struct rtctime *pTime);
s32 tca_rtc_checkvalidtime(void __iomem *devbaseaddresss);

#ifdef __cplusplus
extern
"C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* TCA_RTC_H */
