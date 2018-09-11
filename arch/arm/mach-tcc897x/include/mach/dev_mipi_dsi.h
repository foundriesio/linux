
#ifndef	_DEV_MIPI_DSI_H_
#define	_DEV_MIPI_DSI_H_

//#include "tcc_type.h"
#ifndef uint
typedef unsigned int		uint; 
#endif//

typedef struct {
    uint    ErrContentLP1       : 1;    //  0
    uint    ErrContentLP0       : 1;    //  1
    uint    ErrControl0         : 1;    //  2
    uint    ErrControl1         : 1;    //  3
    uint    ErrControl2         : 1;    //  4
    uint    ErrControl3         : 1;    //  5
    uint    ErrSync0            : 1;    //  6
    uint    ErrSync1            : 1;    //  7
    uint    ErrSync2            : 1;    //  8
    uint    ErrSync3            : 1;    //  9
    uint    ErrEsc0             : 1;    // 10
    uint    ErrEsc1             : 1;    // 11
    uint    ErrEsc2             : 1;    // 12
    uint    ErrEsc3             : 1;    // 13
    uint    ErrRxECErrRxCRC     : 1;    // 14
    uint    ErrRxECC            : 1;    // 15
    uint    RxAck               : 1;    // 16
    uint    TxTE                : 1;    // 17
    uint    RxDatDonTxTE        : 1;    // 18
    uint    TaTouRxDatDone      : 1;    // 19
    uint    TaTout              : 1;    // 20
    uint    LpdrTout            : 1;    // 21
    uint                        : 2;    // 23:22
    uint    FrameDone           : 1;    // 24
    uint    BusTurnOver         : 1;    // 25
    uint                        : 3;    // 28:26
    uint    SFRFifoEmpty        : 1;    // 29
    uint    SwRstRelease        : 1;    // 30
    uint    PllStable           : 1;    // 31
} MIPI_DSI_INT;

typedef union {
    uint            nReg;
    MIPI_DSI_INT    bReg;
} MIPI_DSI_INT_U;

typedef	struct {
	uint	HCLKMASK;	// 0x000 - HCLKMASK       <=  PWDATA[03:00];
	uint	SWRESET;	// 0x004 - SWRESET        <=  PWDATA[03:00];
	uint	CFG_DSI_SEL;	// 0x008 - CFG_DSI_SEL    <=  PWDATA[01:00];
				//	 -   [1:0] =  "00" PMU_SEL
				//	 -   [1:0] =  "01" IO
				//	 -   [1:0] =  "10" PORT
				//	 -   [1:0] =  "11" IO
	uint	CFG_INT_SEL;	// 0x00C - CFG_INT_SEL    <=  PWDATA[00];
	uint	CFG_MEM_EMA;	// 0x010 - CFG_MEM_EMA    <=  PWDATA[02:00];
}	MIPI_DSI_CFG;

//MIPI_DSI Base Address (0xf025b000)
typedef	struct {
	uint	        STATUS;		// 0x000 -  R (0x0010_010F) : Status register
	uint	        SWRST;		// 0x004 - R/W(0x0000_0000) : Software reset register
	uint	        CLKCTRL;	// 0x008 - R/W(0x0000_FFFF) : Clock control register
	uint	        TIMEOUT;	// 0x00C - R/W(0x00FF_FFFF) : Time-out register
	uint	        CONFIG;		// 0x010 - R/W(0x0200_0000) : Configuration register
	uint	        ESCMODE;	// 0x014 - R/W(0x0000_0000) : Escape mode register
	uint	        MDRESOL;	// 0x018 - R/W(0x0300_0400) : Main display image resolution register
	uint	        MVPORCH;	// 0x01C - R/W(0xF000_0000) : Main display vertical porch register
	uint	        MHPORCH;	// 0x020 - R/W(0x0000_0000) : Main display horizontal porch register
	uint	        MSYNC;		// 0x024 - R/W(0x0000_0000) : Main display sync area register
	uint	        SDRESOL;	// 0x028 - R/W(0x0300_0400) : Sub display image resolution registet
	MIPI_DSI_INT_U  uINTSRC;		// 0x02C - R/W(0x0000_0000) : Interrupt source register
	MIPI_DSI_INT_U  uINTMSK;		// 0x030 - R/W(0xB337_FFFF) : Interrupt mask register
	uint	        PKTHDR;		// 0x034 -  W (0x0000_0000) : Packet header FIFO register
	uint	        PAYLOAD;	// 0x038 -  W (0x0000_0000) : Payload FIFO register
	uint	        RXFIFO;		// 0x03C -  R (0xXXXX_XXXX) : Read FIFO register
	uint	        RESERVED;	// 0x040 - R/W(0x0000_01FF) : Reserved register
	uint	        FIFOCTRL;	// 0x044 - R/W(0x0155_551F) : FIFO status & control register
	uint	        MEMACCHR;	// 0x048 - R/W(0x0000_4040) : FIFO memory AC characteristic
	uint	        PLLCTRL;	// 0x04C - R/W(0x0000_0000) : PLL control register
	uint	        PLLTMR;		// 0x050 - R/W(0xFFFF_FFFF) : PLL timer register            
	uint	        PHYACCHR;	// 0x054 - R/W(0x0000_0000) : D-PHY AC characteristic register
	uint	        PHYACCHR1;	// 0x058 - R/W(0x0000_0000) : D-PHY AC characteristic
	uint	        MIPICR;		// 0x05C - R/W(0x0000_0000) : MIPI Cuntrol Register
			        	//	 -   [0] =  '0' LCD_0 Sel or '1' LCD_1 Sel
			        	//	 -   [1] =  '0' LCDSI0 Sel or '1' LCDSI1 Sel
			        	//	 -   [4] =  '1' DSI_TEST Mode Enable
			        	//	 - [6:5] = "xx" MIPI_PHY_TEST Mode Control
			        	//	 -   [7] =  '1' MIPI_PHY_LOOPBACK_Enable
			        	//	 -   [8] =  '1' CSI_TEST Mode Enable
			        	//	 -  [16] =  '1' DSI Interupt Enable

}	MIPI_DSI;


#endif
