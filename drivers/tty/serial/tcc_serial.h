/*
 * tcc_serial.h
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_SERIAL_H_
#define _TCC_SERIAL_H_

#include <linux/dmaengine.h>

#define NR_PORTS				(8)
#define FIFOSIZE				(16) /* depends on FCR */

#define CONSOLE_PORT			0
#define CONSOLE_BAUDRATE		115200

#define SERIAL_TX_DMA_CH_NUM	0
#define SERIAL_TX_DMA_MODE		0
#define SERIAL_TX_DMA_BUF_SIZE	FIFOSIZE

#define SERIAL_RX_DMA_CH_NUM	2
#define SERIAL_RX_DMA_MODE		1
#define SERIAL_RX_DMA_BUF_SIZE	0x8000

struct tcc_uart_clksrc {
	const char *name;
	unsigned int divisor;
	unsigned int min_baud;
	unsigned int max_baud;
};

struct tcc_uart_info {
	char *name;
	unsigned int type;
	unsigned int fifosize;
};

struct tcc_reg_info {
	unsigned long bDLL;
	unsigned long bIER;
	unsigned long bDLM;
	unsigned long bLCR;
	unsigned long bMCR;
	unsigned long bSCR;
	unsigned long bFCR;
	unsigned long bMSR;
	unsigned long bAFT;
	unsigned long bUCR;
};

typedef struct _tcc_dma_buf_t {
	char *addr;
	dma_addr_t dma_addr;
	unsigned int buf_size; // total size of DMA
	unsigned long dma_core;
	unsigned int dma_ch;
	unsigned int dma_port;
	unsigned int dma_intr;
	unsigned int dma_mode;
} tcc_dma_buf_t;

struct tcc_uart_port {
	struct uart_port port;
	unsigned char rx_claimed;
	unsigned char tx_claimed;

	resource_size_t base_addr;

	unsigned int baud;
	struct clk *fclk;
	struct clk *hclk;
	struct tcc_uart_info *info;
	struct tcc_uart_clksrc *clksrc;
	char name[10];
	wait_queue_head_t wait_q;
	int fifosize;

	void (*wake_peer)(struct uart_port *);

	spinlock_t rx_lock;
	tcc_dma_buf_t tx_dma_buffer;
	tcc_dma_buf_t rx_dma_buffer;
	wait_queue_head_t wait_dma_q;
	struct dma_chan *chan_tx;
	struct dma_chan *chan_rx;
	struct scatterlist rx_sg;
	struct scatterlist tx_sg;
	unsigned int rx_residue;
	unsigned int tx_residue;
	int dma_use_tx;
	int dma_use_rx;
	int tx_dma_probed;
	int rx_dma_probed;
	bool tx_dma_working;
	bool tx_dma_using;

	unsigned long dma_start_value;
	unsigned long rx_dma_tail; 
	int timer_state;
	struct timer_list *dma_timer;
	dma_cookie_t rx_cookie;
	dma_cookie_t tx_cookie;

	int tx_done;
	struct tcc_reg_info reg; // for suspend/resume

	int opened;
};

#endif
