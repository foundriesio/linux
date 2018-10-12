/****************************************************************************
 *   FileName    : tca_tcchwcontrol.h
 *   Description : 
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips, Inc.
 *   ALL RIGHTS RESERVED
 *
 ****************************************************************************/
#ifndef __TCA_TCCHWCONTROL_H__
#define __TCA_TCCHWCONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
 *
 * Defines
 *
 ******************************************************************************/

#if defined(CONFIG_ARCH_TCC898X)
#define BASE_ADDR_CHSEL (0x1605105C)	//Audio Interface Channel Select

#define CHSEL_DAI0		0x00	//0x5C AUDIO0 DAI CHMUX Configuration Register
#define CHSEL_DAI1  	0x04	//0x60 AUDIO1 DAI CHMUX Configuration Register
#define CHSEL_CDIF0 	0x08	//0x64 AUDIO0 CDIF CHMUX Configuration Register
#define CHSEL_CDIF1 	0x0C	//0x68 AUDIO1 CDIF CHMUX Configuration Register
#define CHSEL_SPDIF0	0x10	//0x6C AUDIO0 SPDIF CHMUX Configuration Register
#define CHSEL_SPDIF1	0x14	//0x70 AUDIO1 SPDIF CHMUX Configuration Register
#define CHSEL_END		0x18	//0x74

#elif defined(CONFIG_ARCH_TCC802X)

#define PCFG_OFFSET 0x3000 //Port Configuration Register Offset
#define PCFG0 		0x00 //Port Configuration Register0
#define PCFG1 		0x04 //Port Configuration Register1
#define PCFG2 		0x08 //Port Configuration Register2
#define PCFG3 		0x0C //Port Configuration Register3

#define BASE_ADDR_GPIOA                     (0x14200000)

#define OFFSET_GPIOB	0x040	//GPIO_B BASE ADDR OFFSET
#define OFFSET_GPIOC	0x080	//GPIO_C BASE ADDR OFFSET
#define OFFSET_GPIOD	0x0C0	//GPIO_D BASE ADDR OFFSET
#define OFFSET_GPIOE	0x100	//GPIO_E BASE ADDR OFFSET
#define OFFSET_GPIOF	0x140	//GPIO_F BASE ADDR OFFSET
#define OFFSET_GPIOG	0x180	//GPIO_G BASE ADDR OFFSET
#define OFFSET_GPIOSD0	0x240	//GPIO_SD0 BASE ADDR OFFSET
#define OFFSET_GPIOH	0x400	//GPIO_H BASE ADDR OFFSET
#define OFFSET_GPIOK	0x440	//GPIO_K BASE ADDR OFFSET

#define GPIO_SIZE		0x040	//Size of GPIO Setting Register Group

/* Don't need it.
#define GDATA		0x000	// Data
#define GOUT_EN		0x004	// Output Enable
#define GOUT_OR		0x008	// OR Fnction on Output Data 
#define GOUT_BIC	0x00C	// BIC Function on Output Data 
#define GOUT_XOR	0x010	// XOR Function on Output Data 
#define GSTRENGTH0	0x014	// Driver Strength Control 0 
#define GSTRENGTH1	0x018	// Driver Strength Control 1 
#define GPULL_EN	0x01C	// Pull-Up/Down Enable 
#define GPULL_SEL	0x020	// Pull-Up/Down Select 
#define GIN_EN		0x024	// Input Enable 
*/
#define GIN_TYPE	0x028	// Input Type (Shmitt / CMOS) 

/* Don't need it.
#define GSLEW_RATE	0x02C	// Slew Rate 
*/
#define GF_SEL0		0x030	// Port Configuration 0
#define GF_SEL1		0x034	// Port Configuration 1
#define GF_SEL2		0x038	// Port Configuration 2
#define GF_SEL3		0x03C	// Port Configuration 3

#endif              

#define tca_writel	__raw_writel
#define tca_readl	__raw_readl

/*****************************************************************************
 *
 * Extern Functions
 *
 ******************************************************************************/

#if defined(CONFIG_ARCH_TCC898X)
	extern void tca_i2s_port_mux(int id, int *pport);
	extern void tca_spdif_port_mux(int id, int *pport);
	extern void tca_cdif_port_mux(int id, int *pport);
#elif defined(CONFIG_ARCH_TCC802X)
	struct audio_i2s_port {
		char clk[3];
		char daout[4];
		char dain[4];
	};

	extern void tca_i2s_port_mux(void *pDAIBaseAddr, struct audio_i2s_port *port);
	extern void tca_spdif_port_mux(void *pDAIBaseAddr, char *port);
	extern void tca_cdif_port_mux(void *pDAIBaseAddr, struct audio_i2s_port *port);
	extern void tca_i2s_schmitt_set(struct audio_i2s_port *port, bool en);

#endif
	extern void tca_audio_reset(int index);
	extern void tca_iobus_dump(void);
	extern void tca_adma_dump(void *pADMABaseAddr);
	extern void tca_dai_dump(void *pDAIBaseAddr);
	extern void tca_spdif_dump(void *pSPDIFBaseAddr);

/*****************************************************************************
 *
 * ETC
 *
 ******************************************************************************/

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
#define Hw9			0x00000200
#define Hw8			0x00000100
#define Hw7			0x00000080
#define Hw6			0x00000040
#define Hw5			0x00000020
#define Hw4			0x00000010
#define Hw3			0x00000008
#define Hw2			0x00000004
#define Hw1			0x00000002
#define Hw0			0x00000001

#ifdef __cplusplus
}
#endif
#endif

