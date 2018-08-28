/****************************************************************************
 *
 *  Copyright (C) 2016 Telechips, Inc.
 *  All Rights Reserved
 *
 ****************************************************************************/

#ifndef __TCC_HCD_H__
#define __TCC_HCD_H__

#define Hw37		(1LL << 37)
#define Hw36		(1LL << 36)
#define Hw35		(1LL << 35)
#define Hw34		(1LL << 34)
#define Hw33		(1LL << 33)
#define Hw32		(1LL << 32)
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
#define HwZERO		0x00000000

#define ENABLE		1
#define DISABLE		0

#define ON		1
#define OFF		0

#define FALSE		0
#define TRUE		1

#define BITSET(X, MASK)			((X) |= (unsigned int)(MASK))
#define BITSCLR(X, SMASK, CMASK)	((X) = ((((unsigned int)(X)) | ((unsigned int)(SMASK))) & ~((unsigned int)(CMASK))) )
#define BITCSET(X, CMASK, SMASK)	((X) = ((((unsigned int)(X)) & ~((unsigned int)(CMASK))) | ((unsigned int)(SMASK))) )
#define BITCLR(X, MASK)			((X) &= ~((unsigned int)(MASK)) )
#define BITXOR(X, MASK)			((X) ^= (unsigned int)(MASK) )
#define ISZERO(X, MASK)			(!(((unsigned int)(X))&((unsigned int)(MASK))))
#define	ISSET(X, MASK)			((unsigned long)(X)&((unsigned long)(MASK)))

/* USB OHCI	*/
typedef struct {
    unsigned VALUE          :32;
} TCC_OHCI_32BIT_IDX_TYPE;

typedef union {
    unsigned long           nREG;
    TCC_OHCI_32BIT_IDX_TYPE   bREG;
} TCC_OHCI_32BIT_TYPE;

typedef struct _TCC_USB_OHCI {
    volatile TCC_OHCI_32BIT_TYPE  HcRevision;         // 0x000  R    0x00000010   // Control and status registers
    volatile TCC_OHCI_32BIT_TYPE  HcControl;          // 0x004  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcCommandStatus;    // 0x008  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcInterruptStatus;  // 0x00C  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcInterruptEnable;  // 0x010  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcInterruptDisable; // 0x014  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcHCCA;             // 0x018  R/W  0x00000000   // Memory pointer registers
    volatile TCC_OHCI_32BIT_TYPE  HcPeriodCurrentED;  // 0x01C  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcControlHeadED;    // 0x020  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcControlCurrentED; // 0x024  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcBulkHeadED;       // 0x028  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcBulkCurrentED;    // 0x02C  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcDoneHead;         // 0x030  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcRminterval;       // 0x034  R/W  0x00002EDF   // Frame counter registers
    volatile TCC_OHCI_32BIT_TYPE  HcFmRemaining;      // 0x038  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcFmNumber;         // 0x03C  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcPeriodStart;      // 0x040  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcLSThreshold;      // 0x044  R/W  0x00000628
    volatile TCC_OHCI_32BIT_TYPE  HcRhDescriptorA;    // 0x048  R/W  0x02001202   // Root hub registers
    volatile TCC_OHCI_32BIT_TYPE  HcRhDescriptorB;    // 0x04C  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcRhStatus;         // 0x050  R/W  0x00000000
    volatile TCC_OHCI_32BIT_TYPE  HcRhPortStatus1;    // 0x054  R/W  0x00000100
    volatile TCC_OHCI_32BIT_TYPE  HcRhPortStatus2;    // 0x058  R/W  0x00000100
} TCC_OHCI, *PTCC_OHCI;

#define TCC_OHCI_UHCRHDA_PSM_PERPORT	1

#define TCC_OHCI_PPM_NPS				1
#define TCC_OHCI_PPM_GLOBAL			2
#define TCC_OHCI_PPM_PERPORT			3
#define TCC_OHCI_PPM_MIXED			4

#endif
