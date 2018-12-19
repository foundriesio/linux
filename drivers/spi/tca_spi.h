/* 
 * Author:  <linux@telechips.com>
 * Created: 10th Jun, 2013 
 * Description: LINUX SPI DRIVER FUNCTIONS
 *
 * Copyright (c) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TCA_SPI_HWSET_H__
#define __TCA_SPI_HWSET_H__

#include <linux/scatterlist.h>
#ifdef CONFIG_TCC_DMA
#define TCC_DMA_ENGINE
#endif

#ifdef CONFIG_ARCH_TCC802X
#define TCC_USE_GFB_PORT
#endif

#ifdef CONFIG_TCC_GPSB_PARTIAL_TRANSFERS
#define TCC_SPI_PARTIAL_TRANSFERS
#endif

#define tca_spi_writel	__raw_writel
#define tca_spi_readl	__raw_readl

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

#define TCC_GPSB_BITSET(X, MASK)	  (tca_spi_writel(((tca_spi_readl(X)) | ((u32)(MASK))), X))
#define TCC_GPSB_BITSCLR(X, SMASK, CMASK) (tca_spi_writel((((tca_spi_readl(X)) | ((u32)(SMASK))) & ~((u32)(CMASK))) , X))
#define TCC_GPSB_BITCSET(X, CMASK, SMASK) (tca_spi_writel((((tca_spi_readl(X)) & ~((u32)(CMASK))) | ((u32)(SMASK))) , X))
#define TCC_GPSB_BITCLR(X, MASK)	  (tca_spi_writel(((tca_spi_readl(X)) & ~((u32)(MASK))), X))
#define TCC_GPSB_BITXOR(X, MASK)	  (tca_spi_writel(((tca_spi_readl(X)) ^ ((u32)(MASK))), X)
#define TCC_GPSB_ISZERO(X, MASK)	  (!(tca_spi_readl(X)) & ((u32)(MASK)))
#define	TCC_GPSB_ISSET(X, MASK)		  ((tca_spi_readl(X)) & ((u32)(MASK)))

#define TCC_GPSB_TSIF_DEF_INTR_INTERVAL	4	// DMA Default Interrupt Interval for GPSB TSIF

/*
 * GPSB Registers
 */
#define TCC_GPSB_PORT	0x00	// Data port
#define TCC_GPSB_STAT	0x04	// Status register
#define TCC_GPSB_INTEN	0x08	// Interrupt enable
#define TCC_GPSB_MODE	0x0C	// Mode register
#define TCC_GPSB_CTRL	0x10	// Control register
#define TCC_GPSB_EVTCTRL	0x14	// Counter and Ext. Event Control
#define TCC_GPSB_CCV	0x18	// Counter current value
#define TCC_GPSB_TXBASE	0x20	// TX base address register
#define TCC_GPSB_RXBASE	0x24	// RX base address register
#define TCC_GPSB_PACKET	0x28	// Packet register
#define TCC_GPSB_DMACTR	0x2C	// DMA control register
#define TCC_GPSB_DMASTR	0x30	// DMA status register
#define TCC_GPSB_DMAICR	0x34	// DMA interrupt control register

/*
 * GPSB PORT Registers
 */
#ifdef TCC_USE_GFB_PORT
#define TCC_GPSB_PCFG0	0x00	// Port configuration register 0
#define TCC_GPSB_PCFG1	0x04	// Port configuration register 1
#define TCC_GPSB_PCFG2	0x08	// Port configuration register 2
#define TCC_GPSB_PCFG3	0x0C	// Port configuration register 3
#define TCC_GPSB_PCFG4	0x10	// Port configuration register 4
#define TCC_GPSB_PCFG5	0x14	// Port configuration register 5
#define TCC_GPSB_CIRQST	0x1C	// Channel IRQ status register

#define TCC_GPSB_PCFG_OFFSET 0x4
#define TCC_GPSB_PCFG_SHIFT  8

#define TCC_GPSB_MAX_CH	6
#else
#define TCC_GPSB_PCFG0	0x00	// Port configuration register 0
#define TCC_GPSB_PCFG1	0x04	// Port configuration register 1
#define TCC_GPSB_CIRQST	0x0C	// Channel IRQ status register

#define TCC_GPSB_MAX_CH	6
#endif

/*
 * GPSB Access Control Register
 */
#define TCC_GPSB_AC0_START 0x00
#define TCC_GPSB_AC0_LIMIT 0x04
#define TCC_GPSB_AC1_START 0x08
#define TCC_GPSB_AC1_LIMIT 0x0C
#define TCC_GPSB_AC2_START 0x10
#define TCC_GPSB_AC2_LIMIT 0x14
#define TCC_GPSB_AC3_START 0x18
#define TCC_GPSB_AC3_LIMIT 0x1C

typedef struct {
	unsigned CH0            :8;
	unsigned CH1            :8;
	unsigned CH2            :8;
	unsigned CH3            :8;
} GPSB_PCFG0_IDX_TYPE;

typedef union {
	unsigned long           nREG;
	GPSB_PCFG0_IDX_TYPE     bREG;
} GPSB_PCFG0_TYPE;

typedef struct {
	unsigned CH4            :8;
	unsigned CH5            :8;
	unsigned                :16;
} GPSB_PCFG1_IDX_TYPE;

typedef union {
	unsigned long           nREG;
	GPSB_PCFG1_IDX_TYPE     bREG;
} GPSB_PCFG1_TYPE;

typedef struct {
	unsigned ISTC0          :1;
	unsigned ISTD0          :1;
	unsigned ISTC1          :1;
	unsigned ISTD1          :1;
	unsigned ISTC2          :1;
	unsigned ISTD2          :1;
	unsigned ISTC3          :1;
	unsigned                :1;
	unsigned ISTC4          :1;
	unsigned                :1;
	unsigned ISTC5          :1;
	unsigned                :21;
} GPSB_CIRQST_IDX_TYPE;

typedef union {
	unsigned long           nREG;
	GPSB_CIRQST_IDX_TYPE    bREG;
} GPSB_CIRQST_TYPE;

typedef struct _GPSBPORTCFG{
	volatile GPSB_PCFG0_TYPE    PCFG0;          // 0x800  R/W  0x03020100   Port Configuration Register 0
	volatile GPSB_PCFG1_TYPE    PCFG1;          // 0x800  R/W  0x00000504   Port Configuration Register 1
	unsigned                    :32;
	volatile GPSB_CIRQST_TYPE   CIRQST;         // 0x80C  R    0x00000000   Channel IRQ Status Register
} GPSBPORTCFG, *PGPSBPORTCFG;

#define SPI_GDMA_PACKET_SIZE 256
#define MST_GDMA_BSIZE 1	//read cycle per 1 burst transfer
#define SLV_GDMA_BSIZE 1	//read cycle per 1 burst transfer

#define MST_GDMA_WSIZE 1	//in Bytes
#define SLV_GDMA_WSIZE 1	//in Bytes
/*
 * This is for TSIF BLOCK.
 * '0' means there is no TSIF BLOCK device--ehk23
 */
#define SPI_MINOR_NUM_OFFSET 2

/* SPI SLAVE normal mode dev name. */
#define SPI_SLAVE_DEV_NAMES "tcc-spislv%d"

#define WAIT_TIME_FOR_DMA_DONE (1000 * 8)

#if defined(CONFIG_MACH_TCC8800) && defined(CONFIG_DRAM_DDR3)
//#define     SUPORT_USE_SRAM
#define     SRAM_PHY_ADDR      0x10001000
#define     SRAM_VIR_ADDR      0xEFF01000
#define     SRAM_TOT_PACKET    410
#define     SRAM_INT_PACKET    1
#endif

#pragma pack(push, 4)
struct tca_spi_regs {
	volatile unsigned long PORT, STAT, INTEN, MODE, CTRL, EVTCTRL, CCV, 
		DUMMY,
		TXBASE, RXBASE, PACKET, DMACTR, DMASTR, DMAICR;
};
#pragma pack(pop)

struct tea_dma_buf {
	void *v_addr;
	dma_addr_t dma_addr;
	int buf_size; // total size of DMA
};

struct tca_spi_port_config {
	int          gpsb_id;
#ifdef TCC_USE_GFB_PORT
	unsigned int gpsb_port[4];
#else
	unsigned gpsb_port;
#endif
	const char *name;
};

typedef struct tca_spi_handle tca_spi_handle_t;
typedef int (*dma_alloc_f)(struct tea_dma_buf *tdma, unsigned int size, struct device *dev);
typedef void (*dma_free_f)(struct tea_dma_buf *tdma, struct device *dev);

struct tcc_dma_slave
{
	struct device	*dma_dev;
};
struct tcc_spi_dma {
	struct dma_chan *chan_rx;
	struct dma_chan *chan_tx;
	struct scatterlist sgrx;
	struct scatterlist sgtx;
	struct dma_async_tx_descriptor	*data_desc_rx;
	struct dma_async_tx_descriptor	*data_desc_tx;

	struct tcc_dma_slave dma_slave;
};

#define TCC_GPSB_MASTER			0x0
#define TCC_GPSB_SLAVE_TSIF		0x1
#define TCC_GPSB_SLAVE_NORMAL	0x2

struct tca_spi_handle {
	struct device *dev;
	//volatile struct tca_spi_regs *regs;	// GPSB Register
	void __iomem *regs;
	/*u32 */phys_addr_t phy_reg_base; // GPSB Phy Register
	void __iomem *port_regs;	// Port Configuration Register
	void __iomem *pid_regs; // PID Table Register
	struct tea_dma_buf tx_dma, rx_dma;
	struct tea_dma_buf tx_dma_1;
	struct tca_spi_port_config port_config;
	int flag;
	int irq;
	void *private_data;
	int id;
	int is_slave;
#ifdef TCC_USE_GFB_PORT
	unsigned int gpsb_port[4];
#else
	int gpsb_port;
#endif
	int gpsb_channel;

	int (*is_enable_dma)(tca_spi_handle_t *h);
	int (*dma_stop)(tca_spi_handle_t *h);
	int (*dma_start)(tca_spi_handle_t *h);
	void (*clear_fifo_packet)(tca_spi_handle_t *h);
	void (*set_packet_cnt)(tca_spi_handle_t *h, int cnt);
	void (*set_bit_width)(tca_spi_handle_t *h, int width);
	void (*set_dma_addr)(tca_spi_handle_t *h);
	void (*hw_init)(tca_spi_handle_t *h);
	void (*set_mpegts_pidmode)(tca_spi_handle_t *h, int is_set);

	dma_alloc_f tea_dma_alloc;	// tea function.
	dma_free_f tea_dma_free;	// tea function.

	int clk;	// Mhz
	int ctf;	// continuous transfer mode
	int tx_pkt_remain;

	/* add for slave */
	unsigned int dma_total_packet_cnt, dma_intr_packet_cnt;
	int q_pos, cur_q_pos;
	int dma_total_size;
	int dma_mode;

	/* backup gpsb regs */
	unsigned int bak_gpio_port;
	unsigned int bak_gpsb_port;

	/* DMA-engine specific */
	int gdma_use;
	struct tcc_spi_dma	dma;
};

#define tca_spi_setCPOL(R, S) \
	do {\
		if (S) tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) | Hw16,R + TCC_GPSB_MODE);\
		else tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) & ~Hw16,R + TCC_GPSB_MODE);\
	} while (0)

#define tca_spi_setCPHA(R, S) \
	do {\
		if (S) tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) | (Hw18 | Hw17),R + TCC_GPSB_MODE);\
		else tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) & ~(Hw18 | Hw17),R + TCC_GPSB_MODE);\
	} while (0)

#define tca_spi_setCS_HIGH(R, S) \
	do {\
		if (S) tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) | (Hw20 | Hw19),R + TCC_GPSB_MODE);\
		else tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) & ~(Hw20 | Hw19),R + TCC_GPSB_MODE);\
	} while (0)
#define tca_spi_setLSB_FIRST(R, S) \
	do {\
		if (S) tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) | Hw7,R + TCC_GPSB_MODE);\
		else tca_spi_writel(tca_spi_readl(R + TCC_GPSB_MODE) & ~Hw7,R + TCC_GPSB_MODE);\
	} while (0) 

#ifdef __cplusplus
extern "C" {
#endif
	extern int tca_spi_init(tca_spi_handle_t *h,
			void __iomem *regs,
			unsigned int phy_reg_base,
			void __iomem *port_regs,
			void __iomem *pid_regs,
			int irq,
			dma_alloc_f tea_dma_alloc,
			dma_free_f tea_dma_free,
			int dma_size,
			int id,
			int is_slave,
			struct tca_spi_port_config *port,
			const char *gpsb_name,
			struct device *dev);

	extern void tca_spi_clean(tca_spi_handle_t *h);
	extern int tca_spi_register_pids(tca_spi_handle_t *h, unsigned int *pids, unsigned int count);
	extern int tca_tsif_can_support(void);
	extern int tca_spi_is_use_gdma(tca_spi_handle_t *h);
	extern int tca_spi_is_normal_slave(tca_spi_handle_t *h);
	extern int tca_spi_gdma_intr_ch(struct tca_spi_handle *tspi);
#ifdef __cplusplus
}
#endif

#endif /*__TCA_SPI_H__*/
