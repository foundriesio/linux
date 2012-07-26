/*
 * Copyright 2004-2012 Freescale Semiconductor, Inc.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup UART Universal Asynchronous Receiver Transmitter (UART) Driver
 */

/*!
 * @file arch-mxc/mxc_uart.h
 *
 * @brief This file contains the UART configuration structure definition.
 *
 *
 * @ingroup UART
 */

#ifndef __ASM_ARCH_MXC_UART_H__
#define __ASM_ARCH_MXC_UART_H__

#ifdef __KERNEL__

#include <linux/serial_core.h>

/*
 * The modes of the UART ports
 */
#define MODE_DTE                0
#define MODE_DCE                1
/*
 * Is the UART configured to be a IR port
 */
#define IRDA                    0
#define NO_IRDA                 1

/*!
 * This structure is used to store the the physical and virtual
 * addresses of the UART DMA receive buffer.
 */
typedef struct {
	/*!
	 * DMA Receive buffer virtual address
	 */
	char *rx_buf;
	/*!
	 * DMA Receive buffer physical address
	 */
	dma_addr_t rx_handle;
} mxc_uart_rxdmamap;

/*!
 * This structure is a way for the low level driver to define their own
 * \b uart_port structure. This structure includes the core \b uart_port
 * structure that is provided by Linux as an element and has other
 * elements that are specifically required by this low-level driver.
 */
typedef struct {
	/*!
	 * The port structure holds all the information about the UART
	 * port like base address, and so on.
	 */
	struct uart_port port;
	/*!
	 * Flag to determine if the interrupts are muxed.
	 */
	int ints_muxed;
	/*!
	 * Array that holds the receive and master interrupt numbers
	 * when the interrupts are not muxed.
	 */
	int irqs[2];
	/*!
	 * Flag to determine the DTE/DCE mode.
	 */
	int mode;
	/*!
	 * Flag to hold the IR mode of the port.
	 */
	int ir_mode;
	/*!
	 * Flag to enable/disable the UART port.
	 */
	int enabled;
	/*!
	 * Flag to indicate if we wish to use hardware-driven hardware
	 * flow control.
	 */
	int hardware_flow;
	/*!
	 * Holds the threshold value at which the CTS line is deasserted in
	 * case we use hardware-driven hardware flow control.
	 */
	unsigned int cts_threshold;
	/*!
	 * Flag to enable/disable DMA data transfer.
	 */
	int dma_enabled;
	/*!
	 * Holds the DMA receive buffer size.
	 */
	int dma_rxbuf_size;
	/*!
	 * DMA Receive buffers information
	 */
	mxc_uart_rxdmamap *rx_dmamap;
	/*!
	 * DMA RX buffer id
	 */
	int dma_rxbuf_id;
	/*!
	 * DMA Transmit buffer virtual address
	 */
	char *tx_buf;
	/*!
	 * DMA Transmit buffer physical address
	 */
	dma_addr_t tx_handle;
	/*!
	 * Holds the RxFIFO threshold value.
	 */
	unsigned int rx_threshold;
	/*!
	 * Holds the TxFIFO threshold value.
	 */
	unsigned int tx_threshold;
	/*!
	 * Information whether this is a shared UART
	 */
	unsigned int shared;
	/*!
	 * Clock id for UART clock
	 */
	struct clk *clk;
	/*!
	 * Information whether RXDMUXSEL must be set or not for IR port
	 */
	int rxd_mux;
	int ir_tx_inv;
	int ir_rx_inv;
} uart_mxc_port;

/* Address offsets of the UART registers */
#ifdef CONFIG_ARCH_MVF
/* All uart module registers for MVF is 8-bit width */
#define MXC_UARTBDH		0x00 /* Baud rate reg: high */
#define MXC_UARTBDL		0x01 /* Baud rate reg: low */
#define MXC_UARTCR1		0x02 /* Control reg 1 */
#define MXC_UARTCR2		0x03 /* Control reg 2 */
#define MXC_UARTSR1		0x04 /* Status reg 1 */
#define MXC_UARTSR2		0x05 /* Status reg 2 */
#define MXC_UARTCR3		0x06 /* Control reg 3 */
#define MXC_UARTDR		0x07 /* Data reg */
#define MXC_UARTMAR1		0x08 /* Match address reg 1 */
#define MXC_UARTMAR2		0x09 /* Match address reg 2 */
#define MXC_UARTCR4		0x0A /* Control reg 4 */
#define MXC_UARTCR5		0x0B /* Control reg 5 */
#define MXC_UARTEDR		0x0C /* Extended data reg */
#define MXC_UARTMODEM		0x0D /* Modem reg */
#define MXC_UARTIR		0x0E /* Infrared reg */
#define MXC_UARTPFIFO		0x10 /* FIFO parameter reg */
#define MXC_UARTCFIFO		0x11 /* FIFO control reg */
#define MXC_UARTSFIFO		0x12 /* FIFO status reg */
#define MXC_UARTTWFIFO		0x13 /* FIFO transmit watermark reg */
#define MXC_UARTTCFIFO		0x14 /* FIFO transmit count reg */
#define MXC_UARTRWFIFO		0x15 /* FIFO receive watermark reg */
#define MXC_UARTRCFIFO		0x16 /* FIFO receive count reg */
#define MXC_UARTC7816		0x18 /* 7816 control reg */
#define MXC_UARTIE7816		0x19 /* 7816 interrupt enable reg */
#define MXC_UARTIS7816		0x1A /* 7816 interrupt status reg */
#define MXC_UARTWP7816T0	0x1B /* 7816 wait parameter reg */
#define MXC_UARTWP7816T1	0x1B /* 7816 wait parameter reg */
#define MXC_UARTWN7816		0x1C /* 7816 wait N reg */
#define MXC_UARTWF7816		0x1D /* 7816 wait FD reg */
#define MXC_UARTET7816		0x1E /* 7816 error threshold reg */
#define MXC_UARTTL7816		0x1F /* 7816 transmit length reg */
#define MXC_UARTCR6		0x21 /* CEA709.1-B contrl reg */
#define MXC_UARTPCTH		0x22 /* CEA709.1-B packet cycle counter high */
#define MXC_UARTPCTL		0x23 /* CEA709.1-B packet cycle counter low */
#define MXC_UARTB1T		0x24 /* CEA709.1-B beta 1 time */
#define MXC_UARTSDTH		0x25 /* CEA709.1-B secondary delay timer high */
#define MXC_UARTSDTL		0x26 /* CEA709.1-B secondary delay timer low */
#define MXC_UARTPRE		0x27 /* CEA709.1-B preamble */
#define MXC_UARTTPL		0x28 /* CEA709.1-B transmit packet length */
#define MXC_UARTIE		0x29 /* CEA709.1-B transmit interrupt enable */
#define MXC_UARTSR3		0x2B /* CEA709.1-B status reg */
#define MXC_UARTSR4		0x2C /* CEA709.1-B status reg */
#define MXC_UARTRPL		0x2D /* CEA709.1-B received packet length */
#define MXC_UARTRPREL		0x2E /* CEA709.1-B received preamble length */
#define MXC_UARTCPW		0x2F /* CEA709.1-B collision pulse width */
#define MXC_UARTRIDT		0x30 /* CEA709.1-B receive indeterminate time */
#define MXC_UARTTIDT		0x31 /* CEA709.1-B transmit indeterminate time*/

/* Bit definations of BDH */
#define MXC_UARTBDH_LBKDIE	0x80 /* LIN break detect interrupt enable */
#define MXC_UARTBDH_RXEDGIE	0x40 /* RxD input Active edge interrupt enable*/
#define MXC_UARTBDH_SBR_MASK	0x1f /* Uart baud rate high 5-bits */
/* Bit definations of CR1 */
#define MXC_UARTCR1_LOOPS	0x80 /* Loop mode select */
#define MXC_UARTCR1_RSRC	0x20 /* Receiver source select */
#define MXC_UARTCR1_M		0x10 /* 9-bit 8-bit mode select */
#define MXC_UARTCR1_WAKE	0x08 /* Receiver wakeup method */
#define MXC_UARTCR1_ILT		0x04 /* Idle line type */
#define MXC_UARTCR1_PE		0x02 /* Parity enable */
#define MXC_UARTCR1_PT		0x01 /* Parity type */
/* Bit definations of CR2 */
#define MXC_UARTCR2_TIE		0x80 /* Tx interrupt or DMA request enable */
#define MXC_UARTCR2_TCIE	0x40 /* Transmission complete int enable */
#define MXC_UARTCR2_RIE		0x20 /* Rx full int or DMA request enable */
#define MXC_UARTCR2_ILIE	0x10 /* Idle line interrupt enable */
#define MXC_UARTCR2_TE		0x08 /* Transmitter enable */
#define MXC_UARTCR2_RE		0x04 /* Receiver enable */
#define MXC_UARTCR2_RWU		0x02 /* Receiver wakeup control */
#define MXC_UARTCR2_SBK		0x01 /* Send break */
/* Bit definations of SR1 */
#define MXC_UARTSR1_TDRE	0x80 /* Tx data reg empty */
#define MXC_UARTSR1_TC		0x40 /* Transmit complete */
#define MXC_UARTSR1_RDRF	0x20 /* Rx data reg full */
#define MXC_UARTSR1_IDLE	0x10 /* Idle line flag */
#define MXC_UARTSR1_OR		0x08 /* Receiver overrun */
#define MXC_UARTSR1_NF		0x04 /* Noise flag */
#define MXC_UARTSR1_FE		0x02 /* Frame error */
#define MXC_UARTSR1_PE		0x01 /* Parity error */
/* Bit definations of SR2 */
#define MXC_UARTSR2_LBKDIF	0x80 /* LIN brk detect interrupt flag */
#define MXC_UARTSR2_RXEDGIF	0x40 /* RxD pin active edge interrupt flag */
#define MXC_UARTSR2_MSBF	0x20 /* MSB first */
#define MXC_UARTSR2_RXINV	0x10 /* Receive data inverted */
#define MXC_UARTSR2_RWUID	0x08 /* Receive wakeup idle detect */
#define MXC_UARTSR2_BRK13	0x04 /* Break transmit character length */
#define MXC_UARTSR2_LBKDE	0x02 /* LIN break detection enable */
#define MXC_UARTSR2_RAF		0x01 /* Receiver active flag */
/* Bit definations of CR3 */
#define MXC_UARTCR3_R8		0x80 /* Received bit8, for 9-bit data format */
#define MXC_UARTCR3_T8		0x40 /* transmit bit8, for 9-bit data format */
#define MXC_UARTCR3_TXDIR	0x20 /* Tx pin direction in single-wire mode */
#define MXC_UARTCR3_TXINV	0x10 /* Transmit data inversion */
#define MXC_UARTCR3_ORIE	0x08 /* Overrun error interrupt enable */
#define MXC_UARTCR3_NEIE	0x04 /* Noise error interrupt enable */
#define MXC_UARTCR3_FEIE	0x02 /* Framing error interrupt enable */
#define MXC_UARTCR3_PEIE	0x01 /* Parity errror interrupt enable */
/* Bit definations of CR4 */
#define MXC_UARTCR4_MAEN1	0x80 /* Match address mode enable 1 */
#define MXC_UARTCR4_MAEN2	0x40 /* Match address mode enable 2 */
#define MXC_UARTCR4_M10		0x20 /* 10-bit mode select */
#define MXC_UARTCR4_BRFA_MASK	0x1F /* Baud rate fine adjust */
#define MXC_UARTCR4_BRFA_OFF	0
/* Bit definations of CR5 */
#define MXC_UARTCR5_TDMAS	0x80 /* Transmitter DMA select */
#define MXC_UARTCR5_RDMAS	0x20 /* Receiver DMA select */
/* Bit definations of Modem */
#define MXC_UARTMODEM_RXRTSE	0x08 /* Enable receiver request-to-send */
#define MXC_UARTMODEM_TXRTSPOL	0x04 /* Select transmitter RTS polarity */
#define MXC_UARTMODEM_TXRTSE	0x02 /* Enable transmitter request-to-send */
#define MXC_UARTMODEM_TXCTSE	0x01 /* Enable transmitter CTS clear-to-send */
/* Bit definations of EDR */
#define MXC_UARTEDR_NOISY	0x80 /* Current dataword received with noise */
#define MXC_UARTEDR_PARITYE	0x40 /* Dataword received with parity error */
/* Bit definations of Infrared reg(IR) */
#define MXC_UARTIR_IREN		0x04 /* Infrared enable */
#define MXC_UARTIR_TNP_MASK	0x03 /* Transmitter narrow pluse */
#define MXC_UARTIR_TNP_OFF	0
/* Bit definations of FIFO parameter reg */
#define MXC_UARTPFIFO_TXFE	0x80 /* Transmit fifo enable */
#define MXC_UARTPFIFO_TXFIFOSIZE_MASK	0x7
#define MXC_UARTPFIFO_TXFIFOSIZE_OFF	4
#define MXC_UARTPFIFO_RXFE	0x08 /* Receiver fifo enable */
#define MXC_UARTPFIFO_RXFIFOSIZE_MASK	0x7
#define MXC_UARTPFIFO_RXFIFOSIZE_OFF	0
/* Bit definations of FIFO control reg */
#define MXC_UARTCFIFO_TXFLUSH	0x80 /* Transmit FIFO/buffer flush */
#define MXC_UARTCFIFO_RXFLUSH	0x40 /* Receive FIFO/buffer flush */
#define MXC_UARTCFIFO_RXOFE	0x04 /* Receive fifo overflow INT enable */
#define MXC_UARTCFIFO_TXOFE	0x02 /* Transmit fifo overflow INT enable */
#define MXC_UARTCFIFO_RXUFE	0x01 /* Receive fifo underflow INT enable */
/* Bit definations of FIFO status reg */
#define MXC_UARTSFIFO_TXEMPT	0x80 /* Transmit fifo/buffer empty */
#define MXC_UARTSFIFO_RXEMPT	0x40 /* Receive fifo/buffer empty */
#define MXC_UARTSFIFO_RXOF	0x04 /* Rx buffer overflow flag */
#define MXC_UARTSFIFO_TXOF	0x02 /* Tx buffer overflow flag */
#define MXC_UARTSFIFO_RXUF	0x01 /* Rx buffer underflow flag */

#else
#define MXC_UARTURXD            0x000	/* Receive reg */
#define MXC_UARTUTXD            0x040	/* Transmitter reg */
#define	MXC_UARTUCR1            0x080	/* Control reg 1 */
#define MXC_UARTUCR2            0x084	/* Control reg 2 */
#define MXC_UARTUCR3            0x088	/* Control reg 3 */
#define MXC_UARTUCR4            0x08C	/* Control reg 4 */
#define MXC_UARTUFCR            0x090	/* FIFO control reg */
#define MXC_UARTUSR1            0x094	/* Status reg 1 */
#define MXC_UARTUSR2            0x098	/* Status reg 2 */
#define MXC_UARTUESC            0x09C	/* Escape character reg */
#define MXC_UARTUTIM            0x0A0	/* Escape timer reg */
#define MXC_UARTUBIR            0x0A4	/* BRM incremental reg */
#define MXC_UARTUBMR            0x0A8	/* BRM modulator reg */
#define MXC_UARTUBRC            0x0AC	/* Baud rate count reg */
#define MXC_UARTONEMS           0x0B0	/* One millisecond reg */
#define MXC_UARTUTS             0x0B4	/* Test reg */
#define MXC_UARTUMCR            0x0B8	/* RS485 Mode control */
#endif

/* Bit definations of UCR1 */
#define MXC_UARTUCR1_ADEN       0x8000
#define MXC_UARTUCR1_ADBR       0x4000
#define MXC_UARTUCR1_TRDYEN     0x2000
#define MXC_UARTUCR1_IDEN       0x1000
#define MXC_UARTUCR1_RRDYEN     0x0200
#define MXC_UARTUCR1_RXDMAEN    0x0100
#define MXC_UARTUCR1_IREN       0x0080
#define MXC_UARTUCR1_TXMPTYEN   0x0040
#define MXC_UARTUCR1_RTSDEN     0x0020
#define MXC_UARTUCR1_SNDBRK     0x0010
#define MXC_UARTUCR1_TXDMAEN    0x0008
#define MXC_UARTUCR1_ATDMAEN    0x0004
#define MXC_UARTUCR1_DOZE       0x0002
#define MXC_UARTUCR1_UARTEN     0x0001

/* Bit definations of UCR2 */
#define MXC_UARTUCR2_ESCI       0x8000
#define MXC_UARTUCR2_IRTS       0x4000
#define MXC_UARTUCR2_CTSC       0x2000
#define MXC_UARTUCR2_CTS        0x1000
#define MXC_UARTUCR2_PREN       0x0100
#define MXC_UARTUCR2_PROE       0x0080
#define MXC_UARTUCR2_STPB       0x0040
#define MXC_UARTUCR2_WS         0x0020
#define MXC_UARTUCR2_RTSEN      0x0010
#define MXC_UARTUCR2_ATEN       0x0008
#define MXC_UARTUCR2_TXEN       0x0004
#define MXC_UARTUCR2_RXEN       0x0002
#define MXC_UARTUCR2_SRST       0x0001

/* Bit definations of UCR3 */
#define MXC_UARTUCR3_DTREN      0x2000
#define MXC_UARTUCR3_PARERREN   0x1000
#define MXC_UARTUCR3_FRAERREN   0x0800
#define MXC_UARTUCR3_DSR        0x0400
#define MXC_UARTUCR3_DCD        0x0200
#define MXC_UARTUCR3_RI         0x0100
#define MXC_UARTUCR3_RXDSEN     0x0040
#define MXC_UARTUCR3_AWAKEN     0x0010
#define MXC_UARTUCR3_DTRDEN     0x0008
#define MXC_UARTUCR3_RXDMUXSEL  0x0004
#define MXC_UARTUCR3_INVT       0x0002

/* Bit definations of UCR4 */
#define MXC_UARTUCR4_CTSTL_OFFSET       10
#define MXC_UARTUCR4_CTSTL_MASK         (0x3F << 10)
#define MXC_UARTUCR4_INVR               0x0200
#define MXC_UARTUCR4_ENIRI              0x0100
#define MXC_UARTUCR4_REF16              0x0040
#define MXC_UARTUCR4_IRSC               0x0020
#define MXC_UARTUCR4_TCEN               0x0008
#define MXC_UARTUCR4_OREN               0x0002
#define MXC_UARTUCR4_DREN               0x0001

/* Bit definations of UFCR */
#define MXC_UARTUFCR_RFDIV              0x0200	/* Ref freq div is set to 2 */
#define MXC_UARTUFCR_RFDIV_OFFSET       7
#define MXC_UARTUFCR_RFDIV_MASK         (0x7 << 7)
#define MXC_UARTUFCR_TXTL_OFFSET        10
#define MXC_UARTUFCR_DCEDTE             0x0040

/* Bit definations of URXD */
#define MXC_UARTURXD_ERR        0x4000
#define MXC_UARTURXD_OVRRUN     0x2000
#define MXC_UARTURXD_FRMERR     0x1000
#define MXC_UARTURXD_BRK        0x0800
#define MXC_UARTURXD_PRERR      0x0400

/* Bit definations of USR1 */
#define MXC_UARTUSR1_PARITYERR  0x8000
#define MXC_UARTUSR1_RTSS       0x4000
#define MXC_UARTUSR1_TRDY       0x2000
#define MXC_UARTUSR1_RTSD       0x1000
#define MXC_UARTUSR1_FRAMERR    0x0400
#define MXC_UARTUSR1_RRDY       0x0200
#define MXC_UARTUSR1_AGTIM      0x0100
#define MXC_UARTUSR1_DTRD       0x0080
#define MXC_UARTUSR1_AWAKE      0x0010

/* Bit definations of USR2 */
#define MXC_UARTUSR2_TXFE       0x4000
#define MXC_UARTUSR2_IDLE       0x1000
#define MXC_UARTUSR2_RIDELT     0x0400
#define MXC_UARTUSR2_RIIN       0x0200
#define MXC_UARTUSR2_DCDDELT    0x0040
#define MXC_UARTUSR2_DCDIN      0x0020
#define MXC_UARTUSR2_TXDC       0x0008
#define MXC_UARTUSR2_ORE        0x0002
#define MXC_UARTUSR2_RDR        0x0001
#define MXC_UARTUSR2_BRCD       0x0004

/* Bit definations of UTS */
#define MXC_UARTUTS_LOOP        0x1000

#endif				/* __KERNEL__ */

#endif				/* __ASM_ARCH_MXC_UART_H__ */
