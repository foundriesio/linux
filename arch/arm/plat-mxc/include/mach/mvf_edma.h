/*
 * Copyright 2007-2012 Freescale Semiconductor, Inc.
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __MVF_EDMA_H__
#define __MVF_EDMA_H__

/*
 * Enhanced DMA (EDMA)
 */
/* unioned DMA request source */
/* DMA MUX0,3 unioned reqeust number */
#define DMA_MUX_UART0_RX	2
#define DMA_MUX_UART0_TX	3
#define DMA_MUX_UART1_RX	4
#define DMA_MUX_UART1_TX	5
#define DMA_MUX_UART2_RX	6
#define DMA_MUX_UART2_TX	7
#define DMA_MUX_UART3_RX	8
#define DMA_MUX_UART3_TX	9
#define DMA_MUX_DSPI0_RX	12
#define DMA_MUX_DSPI0_TX	13
#define DMA_MUX_DSPI1_RX	14
#define DMA_MUX_DSPI1_TX	15
#define DMA_MUX_SAI0_RX	16
#define DMA_MUX_SAI0_TX	17
#define DMA_MUX_SAI1_RX	18
#define DMA_MUX_SAI1_TX	19
#define DMA_MUX_SAI2_RX	20
#define DMA_MUX_SAI2_TX	21
#define DMA_MUX_PDB	22
#define DMA_MUX_FTM0_CH0	24
#define DMA_MUX_FTM0_CH1	25
#define DMA_MUX_FTM0_CH2	26
#define DMA_MUX_FTM0_CH3	27
#define DMA_MUX_FTM0_CH4	28
#define DMA_MUX_FTM0_CH5	29
#define DMA_MUX_FTM0_CH6	30
#define DMA_MUX_FTM0_CH7	31
#define DMA_MUX_FTM1_CH0	32
#define DMA_MUX_FTM1_CH1	33
#define DMA_MUX_ADC0	34
#define DMA_MUX_QUADSPI0	36
#define DMA_MUX_GPIOA	38
#define DMA_MUX_GPIOB	39
#define DMA_MUX_GPIOC	40
#define DMA_MUX_GPIOD	41
#define DMA_MUX_GPIOE	42
#define DMA_MUX_RLE_RX	45
#define DMA_MUX_RLE_TX	46
#define DMA_MUX_SPDIF_RX	47
#define DMA_MUX_SPDIF_TX	48
#define DMA_MUX_I2C0_RX	50
#define DMA_MUX_I2C0_TX	51
#define DMA_MUX_I2C1_RX	52
#define DMA_MUX_I2C1_TX	53
#define DMA_MUX_ALWAYS0	54
#define DMA_MUX_ALWAYS1	55
#define DMA_MUX_ALWAYS2	56
#define DMA_MUX_ALWAYS3	57
#define DMA_MUX_ALWAYS4	58
#define DMA_MUX_ALWAYS5	59
#define DMA_MUX_ALWAYS6	60
#define DMA_MUX_ALWAYS7	61
#define DMA_MUX_ALWAYS8	62
#define DMA_MUX_ALWAYS9	63

/* DMA MUX1,2 unioned request number */
#define DMA_MUX_UART4_RX	(2 + 64)
#define DMA_MUX_UART4_TX	(3 + 64)
#define DMA_MUX_UART5_RX	(4 + 64)
#define DMA_MUX_UART5_TX	(5 + 64)
#define DMA_MUX_SAI3_RX	(8 + 64)
#define DMA_MUX_SAI3_TX	(9 + 64)
#define DMA_MUX_DSPI2_RX	(10 + 64)
#define DMA_MUX_DSPI2_TX	(11 + 64)
#define DMA_MUX_DSPI3_RX	(12 + 64)
#define DMA_MUX_DSPI3_TX	(13 + 64)
#define DMA_MUX_FTM2_CH0	(16 + 64)
#define DMA_MUX_FTM2_CH1	(17 + 64)
#define DMA_MUX_FTM3_CH0	(18 + 64)
#define DMA_MUX_FTM3_CH1	(19 + 64)
#define DMA_MUX_FTM3_CH2	(20 + 64)
#define DMA_MUX_FTM3_CH3	(21 + 64)
#define DMA_MUX_FTM3_CH4	(22 + 64)
#define DMA_MUX_FTM3_CH5	(24 + 64)
#define DMA_MUX_FTM3_CH6	(25 + 64)
#define DMA_MUX_FTM3_CH7	(26 + 64)
#define DMA_MUX_QUADSPI1	(27 + 64)
#define DMA_MUX_DAC0	(32 + 64)
#define DMA_MUX_DAC1	(33 + 64)
#define DMA_MUX_ESAI_BIFIFO_TX	(34 + 64)
#define DMA_MUX_ESAI_BIFIFO_RX	(35 + 64)
#define DMA_MUX_I2C2_RX	(36 + 64)
#define DMA_MUX_I2C2_TX	(37 + 64)
#define DMA_MUX_I2C3_RX	(38 + 64)
#define DMA_MUX_I2C3_TX	(39 + 64)
#define DMA_MUX_ASRC0_TX	(40 + 64)
#define DMA_MUX_ASRC0_RX	(41 + 64)
#define DMA_MUX_ASRC1_TX	(42 + 64)
#define DMA_MUX_ASRC1_RX	(43 + 64)
#define DMA_MUX_TIMER0	(44 + 64)
#define DMA_MUX_TIMER1	(45 + 64)
#define DMA_MUX_TIMER2	(46 + 64)
#define DMA_MUX_TIMER3	(47 + 64)
#define DMA_MUX_TIMER4	(48 + 64)
#define DMA_MUX_TIMER5	(49 + 64)
#define DMA_MUX_TIMER6	(50 + 64)
#define DMA_MUX_TIMER7	(51 + 64)
#define DMA_MUX_ASRC2_TX	(52 + 64)
#define DMA_MUX_ASRC2_RX	(53 + 64)
#define DMA_MUX_ALWAYS10	(54 + 64)
#define DMA_MUX_ALWAYS11	(55 + 64)
#define DMA_MUX_ALWAYS12	(56 + 64)
#define DMA_MUX_ALWAYS13	(57 + 64)
#define DMA_MUX_ALWAYS14	(58 + 64)
#define DMA_MUX_ALWAYS15	(59 + 64)
#define DMA_MUX_ALWAYS16	(60 + 64)
#define DMA_MUX_ALWAYS17	(61 + 64)
#define DMA_MUX_ALWAYS18	(62 + 64)
#define DMA_MUX_ALWAYS19	(63 + 64)


/* DMA channel mode */
#define DMACH_MODE_DISABLED
#define DMACH_MODE_NORMAL
#define DMACH_MODE_PERIODIC

#define DMAMUX_CHCFG(x)	(x)
/* bits definition of DMAMUX reg */
#define DMAMUX_CHCFG_ENBL	0x80 /* DMA channel enable */
#define DMAMUX_CHCFG_TRIG	0x40 /* DMA channel periodic triger enable */
#define DMAMUX_CHCFG_SRC_MASK	0x3F /* DMA channel source slot */
#define DMAMUX_CHCFG_SRC_OFFSET	0x0
#define DMAMUX_CHCFG_SOURCE(n) ((n) & 0x3F)

/* Register offset */
#define MCF_EDMA_CR		(0x00) /* Control reg */
#define MCF_EDMA_ES		(0x04) /* Error stattus reg */
#define MCF_EDMA_ERQ		(0x0C) /* Enable request reg */
#define MCF_EDMA_EEI		(0x14) /* Enable error INT reg */
#define MCF_EDMA_SERQ		(0x1B) /* Set enable request reg */
#define MCF_EDMA_CERQ		(0x1A) /* Clear enable request reg */
#define MCF_EDMA_SEEI		(0x19) /* Set enable error INT reg */
#define MCF_EDMA_CEEI		(0x18) /* Clear enable error INT reg */
#define MCF_EDMA_CINT		(0x1F) /* Clear INT request reg  */
#define MCF_EDMA_CERR		(0x1E) /* Clear given bit in ERR */
#define MCF_EDMA_SSRT		(0x1D) /* Set START bit in TCD */
#define MCF_EDMA_CDNE		(0x1C) /* Clear DONE status bit in TCD reg */
#define MCF_EDMA_INTR		(0x24) /* INT rqeust reg */
#define MCF_EDMA_ERR		(0x2C) /* Error present for each channel */
#define MCF_EDMA_HRS		(0x34) /* Hardware request status reg */
#define MCF_EDMA_DCHPRI3	(0x100)
#define MCF_EDMA_DCHPRI2	(0x101)
#define MCF_EDMA_DCHPRI1	(0x102)
#define MCF_EDMA_DCHPRI0	(0x103)
#define MCF_EDMA_DCHPRI7	(0x104)
#define MCF_EDMA_DCHPRI6	(0x105)
#define MCF_EDMA_DCHPRI5	(0x106)
#define MCF_EDMA_DCHPRI4	(0x107)
#define MCF_EDMA_DCHPRI11	(0x108)
#define MCF_EDMA_DCHPRI10	(0x109)
#define MCF_EDMA_DCHPRI9	(0x10A)
#define MCF_EDMA_DCHPRI8	(0x10B)
#define MCF_EDMA_DCHPRI15	(0x10C)
#define MCF_EDMA_DCHPRI14	(0x10D)
#define MCF_EDMA_DCHPRI13	(0x10E)
#define MCF_EDMA_DCHPRI12	(0x10F)
#define MCF_EDMA_DCHPRI19	(0x110)
#define MCF_EDMA_DCHPRI18	(0x111)
#define MCF_EDMA_DCHPRI17	(0x112)
#define MCF_EDMA_DCHPRI16	(0x113)
#define MCF_EDMA_DCHPRI23	(0x114)
#define MCF_EDMA_DCHPRI22	(0x115)
#define MCF_EDMA_DCHPRI21	(0x116)
#define MCF_EDMA_DCHPRI20	(0x117)
#define MCF_EDMA_DCHPRI27	(0x118)
#define MCF_EDMA_DCHPRI26	(0x119)
#define MCF_EDMA_DCHPRI25	(0x11A)
#define MCF_EDMA_DCHPRI24	(0x11B)
#define MCF_EDMA_DCHPRI31	(0x11C)
#define MCF_EDMA_DCHPRI30	(0x11D)
#define MCF_EDMA_DCHPRI29	(0x11E)
#define MCF_EDMA_DCHPRI28	(0x11F)
#define MCF_EDMA_DCHPRI(x)	(0x100 + (x) + 3 - (x)%4 - (x)%4)

#define MCF_EDMA_TCD(x)		(0x1000 + 32 * (x))
#if 0
#define MCF_EDMA_TCD_SADDR	(0x000) /* 32-bit */
#define MCF_EDMA_TCD_SOFF	(0x004) /* 16-bit */
#define MCF_EDMA_TCD_ATTR	(0x006) /* 16-bit */
#define MCF_EDMA_TCD_NBYTES	(0x008) /* 32-bit */
#define MCF_EDMA_TCD_SLAST	(0x00C) /* 32-bit */
#define MCF_EDMA_TCD_DADDR	(0x010) /* 32-bit */
#define MCF_EDMA_TCD_DOFF		(0x014) /* 16-bit */
#define MCF_EDMA_TCD_CITER_ELINK	(0x016)
#define MCF_EDMA_TCD_CITER		(0x016) /* 16-bit */
#define MCF_EDMA_TCD_DLAST_SGA		(0x018) /* 32-bit */
#define MCF_EDMA_TCD_CSR		(0x01C) /* 16-bit */
#define MCF_EDMA_TCD_BITER_ELINK	(0x01E)
#define MCF_EDMA_TCD_BITER		(0x01E) /* 16-bit */
#endif

#define MCF_EDMA_TCD_SADDR(x)	(0x1000 + 32 * (x))
#define MCF_EDMA_TCD_SOFF(x)	(0x1004 + 32 * (x))
#define MCF_EDMA_TCD_ATTR(x)	(0x1006 + 32 * (x))
#define MCF_EDMA_TCD_NBYTES(x)	(0x1008 + 32 * (x))
#define MCF_EDMA_TCD_SLAST(x)	(0x100C + 32 * (x))
#define MCF_EDMA_TCD_DADDR(x)	(0x1010 + 32 * (x))
#define MCF_EDMA_TCD_DOFF(x)		(0x1014 + 32 * (x))
#define MCF_EDMA_TCD_CITER_ELINK(x)	(0x1016 + 32 * (x))
#define MCF_EDMA_TCD_CITER(x)		(0x1016 + 32 * (x))
#define MCF_EDMA_TCD_DLAST_SGA(x)	(0x1018 + 32 * (x))
#define MCF_EDMA_TCD_CSR(x)		(0x101C + 32 * (x))
#define MCF_EDMA_TCD_BITER_ELINK(x)	(0x101E + 32 * (x))
#define MCF_EDMA_TCD_BITER(x)		(0x101E + 32 * (x))


/* Bit definitions and macros for CR */
#define MCF_EDMA_CR_EDBG	(0x00000002)
#define MCF_EDMA_CR_ERCA	(0x00000004)
#define MCF_EDMA_CR_ERGA	(0x00000008)
#define MCF_EDMA_CR_HOE		(0x00000010)
#define MCF_EDMA_CR_HALT	(0x00000020)
#define MCF_EDMA_CR_CLM		(0x00000040)
#define MCF_EDMA_CR_EMLM	(0x00000080)
#define MCF_EDMA_CR_GRP0PRI(x)	(((x)&0x03)<<8)
#define MCF_EDMA_CR_GRP1PRI(x)	(((x)&0x03)<<10)
#define MCF_EDMA_CR_ECX		(0x00010000)
#define MCF_EDMA_CR_CX		(0x00020000)

/* Bit definitions and macros for ES */
#define MCF_EDMA_ES_DBE		(0x00000001)
#define MCF_EDMA_ES_SBE		(0x00000002)
#define MCF_EDMA_ES_SGE		(0x00000004)
#define MCF_EDMA_ES_NCE		(0x00000008)
#define MCF_EDMA_ES_DOE		(0x00000010)
#define MCF_EDMA_ES_DAE		(0x00000020)
#define MCF_EDMA_ES_SOE		(0x00000040)
#define MCF_EDMA_ES_SAE		(0x00000080)
#define MCF_EDMA_ES_ERRCHN(x)	(((x)&0x01F)<<8)
#define MCF_EDMA_ES_CPE		(0x00004000)
#define MCF_EDMA_ES_GPE		(0x00008000)
#define MCF_EDMA_ES_ECX		(0x00010000)
#define MCF_EDMA_ES_VLD		(0x80000000)

/* Bit definitions and macros for Enable DMA request reg */
#define MCF_EDMA_ERQ_ERQ(x)	(0x0001 << (x))
#define MCF_EDMA_ERQ_ERQ0	(0x0001)
#define MCF_EDMA_ERQ_ERQ1	(0x0002)
#define MCF_EDMA_ERQ_ERQ2	(0x0004)
#define MCF_EDMA_ERQ_ERQ3	(0x0008)
#define MCF_EDMA_ERQ_ERQ4	(0x0010)
#define MCF_EDMA_ERQ_ERQ5	(0x0020)
#define MCF_EDMA_ERQ_ERQ6	(0x0040)
#define MCF_EDMA_ERQ_ERQ7	(0x0080)
#define MCF_EDMA_ERQ_ERQ8	(0x0100)
#define MCF_EDMA_ERQ_ERQ9	(0x0200)
#define MCF_EDMA_ERQ_ERQ10	(0x0400)
#define MCF_EDMA_ERQ_ERQ11	(0x0800)
#define MCF_EDMA_ERQ_ERQ12	(0x1000)
#define MCF_EDMA_ERQ_ERQ13	(0x2000)
#define MCF_EDMA_ERQ_ERQ14	(0x4000)
#define MCF_EDMA_ERQ_ERQ15	(0x8000)

/* Bit definitions and macros for Enable Error Interrupt reg */
#define MCF_EDMA_EEI_EEI(x)	(0x0001 << (x))
#define MCF_EDMA_EEI_EEI0       (0x0001)
#define MCF_EDMA_EEI_EEI1       (0x0002)
#define MCF_EDMA_EEI_EEI2       (0x0004)
#define MCF_EDMA_EEI_EEI3       (0x0008)
#define MCF_EDMA_EEI_EEI4       (0x0010)
#define MCF_EDMA_EEI_EEI5       (0x0020)
#define MCF_EDMA_EEI_EEI6       (0x0040)
#define MCF_EDMA_EEI_EEI7       (0x0080)
#define MCF_EDMA_EEI_EEI8       (0x0100)
#define MCF_EDMA_EEI_EEI9       (0x0200)
#define MCF_EDMA_EEI_EEI10      (0x0400)
#define MCF_EDMA_EEI_EEI11      (0x0800)
#define MCF_EDMA_EEI_EEI12      (0x1000)
#define MCF_EDMA_EEI_EEI13      (0x2000)
#define MCF_EDMA_EEI_EEI14      (0x4000)
#define MCF_EDMA_EEI_EEI15      (0x8000)

/* Bit definitions and macros for Set Enable Request reg */
#define MCF_EDMA_SERQ_SERQ(x)   (((x)&0x1F))
#define MCF_EDMA_SERQ_SAER      (0x40)
#define MCF_EDMA_SERQ_NOP	(0x80)

/* Bit definitions and macros for Clear Enable Request reg */
#define MCF_EDMA_CERQ_CERQ(x)   (((x)&0x1F))
#define MCF_EDMA_CERQ_CAER      (0x40)
#define MCF_EDMA_CERQ_NOP	(0x80)

/* Bit definitions and macros for Set Enable Error Interrupt reg */
#define MCF_EDMA_SEEI_SEEI(x)   (((x)&0x1F))
#define MCF_EDMA_SEEI_SAEE      (0x40)
#define MCF_EDMA_SEEI_NOP	(0x80)

/* Bit definitions and macros for Clear Enable Error Interrupt reg */
#define MCF_EDMA_CEEI_CEEI(x)   (((x)&0x1F))
#define MCF_EDMA_CEEI_CAEE      (0x40)
#define MCF_EDMA_CEEI_NOP	(0x80)

/* Bit definitions and macros for CINT */
#define MCF_EDMA_CINT_CINT(x)   (((x)&0x1F))
#define MCF_EDMA_CINT_CAIR      (0x40)
#define MCF_EDMA_CINT_NOP	(0x80)

/* Bit definitions and macros for CERR */
#define MCF_EDMA_CERR_CERR(x)   (((x)&0x1F))
#define MCF_EDMA_CERR_CAER      (0x40)
#define MCF_EDMA_CERR_NOP	(0x80)

/* Bit definitions and macros for Set START bit in TCD */
#define MCF_EDMA_SSRT_SSRT(x)   (((x)&0x1F))
#define MCF_EDMA_SSRT_SAST      (0x40)
#define MCF_EDMA_SSRT_NOP	(0x80)

/* Bit definitions and macros for Clear DoNE bit in TCD */
#define MCF_EDMA_CDNE_CDNE(x)   (((x)&0x1F))
#define MCF_EDMA_CDNE_CADN      (0x40)
#define MCF_EDMA_CDNE_NOP	(0x80)

/* Bit definitions and macros for INTR */
#define MCF_EDMA_INTR_INT(x)	(0x0001 << (x))
/*
#define MCF_EDMA_INTR_INT0      (0x0001)
#define MCF_EDMA_INTR_INT1      (0x0002)
#define MCF_EDMA_INTR_INT2      (0x0004)
#define MCF_EDMA_INTR_INT3      (0x0008)
#define MCF_EDMA_INTR_INT4      (0x0010)
#define MCF_EDMA_INTR_INT5      (0x0020)
#define MCF_EDMA_INTR_INT6      (0x0040)
#define MCF_EDMA_INTR_INT7      (0x0080)
#define MCF_EDMA_INTR_INT8      (0x0100)
#define MCF_EDMA_INTR_INT9      (0x0200)
#define MCF_EDMA_INTR_INT10     (0x0400)
#define MCF_EDMA_INTR_INT11     (0x0800)
#define MCF_EDMA_INTR_INT12     (0x1000)
#define MCF_EDMA_INTR_INT13     (0x2000)
#define MCF_EDMA_INTR_INT14     (0x4000)
#define MCF_EDMA_INTR_INT15     (0x8000)
*/

/* Bit definitions and macros for ERR */
#define MCF_EDMA_ERR_ERR(x)	(0x0001 << (x))
/*
#define MCF_EDMA_ERR_ERR0       (0x0001)
#define MCF_EDMA_ERR_ERR1       (0x0002)
#define MCF_EDMA_ERR_ERR2       (0x0004)
#define MCF_EDMA_ERR_ERR3       (0x0008)
#define MCF_EDMA_ERR_ERR4       (0x0010)
#define MCF_EDMA_ERR_ERR5       (0x0020)
#define MCF_EDMA_ERR_ERR6       (0x0040)
#define MCF_EDMA_ERR_ERR7       (0x0080)
#define MCF_EDMA_ERR_ERR8       (0x0100)
#define MCF_EDMA_ERR_ERR9       (0x0200)
#define MCF_EDMA_ERR_ERR10      (0x0400)
#define MCF_EDMA_ERR_ERR11      (0x0800)
#define MCF_EDMA_ERR_ERR12      (0x1000)
#define MCF_EDMA_ERR_ERR13      (0x2000)
#define MCF_EDMA_ERR_ERR14      (0x4000)
#define MCF_EDMA_ERR_ERR15      (0x8000)
*/

/* Bit definitions and macros for DCHPRI group */
#define MCF_EDMA_DCHPRI_CHPRI(x)	(((x)&0x0F))
#define MCF_EDMA_DCHPRI_ECP		(0x80)
#define MCF_EDMA_DCHPRI_DPA		(0x40)

/* Bit definitions and macros for TCD_SADDR group */
#define MCF_EDMA_TCD_SADDR_SADDR(x)     (x)

/* Bit definitions and macros for TCD_ATTR group */
#define MCF_EDMA_TCD_ATTR_DSIZE(x)          (((x)&0x0007))
#define MCF_EDMA_TCD_ATTR_DMOD(x)           (((x)&0x001F)<<3)
#define MCF_EDMA_TCD_ATTR_SSIZE(x)          (((x)&0x0007)<<8)
#define MCF_EDMA_TCD_ATTR_SMOD(x)           (((x)&0x001F)<<11)
#define MCF_EDMA_TCD_ATTR_SSIZE_8BIT        (0x0000)
#define MCF_EDMA_TCD_ATTR_SSIZE_16BIT       (0x0100)
#define MCF_EDMA_TCD_ATTR_SSIZE_32BIT       (0x0200)
#define MCF_EDMA_TCD_ATTR_SSIZE_64BIT      (0x0300)
#define MCF_EDMA_TCD_ATTR_SSIZE_32BYTE      (0x0500)
#define MCF_EDMA_TCD_ATTR_DSIZE_8BIT        (0x0000)
#define MCF_EDMA_TCD_ATTR_DSIZE_16BIT       (0x0001)
#define MCF_EDMA_TCD_ATTR_DSIZE_32BIT       (0x0002)
#define MCF_EDMA_TCD_ATTR_DSIZE_64BIT      (0x0003)
#define MCF_EDMA_TCD_ATTR_DSIZE_32BYTE      (0x0005)

/* Bit definitions and macros for TCD_SOFF group */
#define MCF_EDMA_TCD_SOFF_SOFF(x)   (x)

/* Bit definitions and macros for TCD_NBYTES group */
#define MCF_EDMA_TCD_NBYTES_NBYTES(x)   (x)
#define MCF_EDMA_TCD_NBYTES_SMLOE       (0x80000000)
#define MCF_EDMA_TCD_NBYTES_DMLOE       (0x40000000)
#define MCF_EDMA_TCD_NBYTES_MLOFF(x)    (((x)&0xFFFFF)<<10)
#define MCF_EDMA_TCD_NBYTES_10BITS       ((x)&0x3FF)

/* Bit definitions and macros for TCD_SLAST group */
#define MCF_EDMA_TCD_SLAST_SLAST(x)     (x)

/* Bit definitions and macros for TCD_DADDR group */
#define MCF_EDMA_TCD_DADDR_DADDR(x)     (x)

/* Bit definitions and macros for TCD_CITER_ELINK group */
#define MCF_EDMA_TCD_CITER_ELINK_CITER(x)       (((x)&0x01FF))
#define MCF_EDMA_TCD_CITER_ELINK_LINKCH(x)      (((x)&0x001F)<<9)
#define MCF_EDMA_TCD_CITER_ELINK_E_LINK         (0x8000)

/* Bit definitions and macros for TCD_CITER group */
#define MCF_EDMA_TCD_CITER_CITER(x)     (((x)&0x7FFF))
#define MCF_EDMA_TCD_CITER_E_LINK       (0x8000)

/* Bit definitions and macros for TCD_DOFF group */
#define MCF_EDMA_TCD_DOFF_DOFF(x)   (x)

/* Bit definitions and macros for TCD_DLAST_SGA group */
#define MCF_EDMA_TCD_DLAST_SGA_DLAST_SGA(x)     (x)

/* Bit definitions and macros for TCD_BITER_ELINK group */
#define MCF_EDMA_TCD_BITER_ELINK_BITER(x)       (((x)&0x01FF))
#define MCF_EDMA_TCD_BITER_ELINK_LINKCH(x)      (((x)&0x001F)<<9)
#define MCF_EDMA_TCD_BITER_ELINK_E_LINK         (0x8000)

/* Bit definitions and macros for TCD_BITER group */
#define MCF_EDMA_TCD_BITER_BITER(x)     (((x)&0x7FFF))
#define MCF_EDMA_TCD_BITER_E_LINK       (0x8000)

/* Bit definitions and macros for TCD_CSR group */
#define MCF_EDMA_TCD_CSR_START              (0x0001)
#define MCF_EDMA_TCD_CSR_INT_MAJOR          (0x0002)
#define MCF_EDMA_TCD_CSR_INT_HALF           (0x0004)
#define MCF_EDMA_TCD_CSR_D_REQ              (0x0008)
#define MCF_EDMA_TCD_CSR_E_SG               (0x0010)
#define MCF_EDMA_TCD_CSR_E_LINK             (0x0020)
#define MCF_EDMA_TCD_CSR_ACTIVE             (0x0040)
#define MCF_EDMA_TCD_CSR_DONE               (0x0080)
#define MCF_EDMA_TCD_CSR_LINKCH(x)          (((x)&0x001F)<<8)
#define MCF_EDMA_TCD_CSR_BWC(x)             (((x)&0x0003)<<14)
#define MCF_EDMA_TCD_CSR_BWC_NO_STALL       (0x0000)
#define MCF_EDMA_TCD_CSR_BWC_4CYC_STALL     (0x8000)
#define MCF_EDMA_TCD_CSR_BWC_8CYC_STALL     (0xC000)

#endif /* __MVF_EDMA_H__ */
