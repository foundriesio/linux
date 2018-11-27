/****************************************************************************
 *   FileName    : serial.h
 *   Description : 
 ****************************************************************************
 *
 *   TCC Version 1.0
 *   Copyright (c) Telechips, Inc.
 *   ALL RIGHTS RESERVED
 *
 ****************************************************************************/
#ifndef _TCC_SERIAL_H_
#define _TCC_SERIAL_H_

#include <linux/clk.h>
#include <linux/wait.h>
#include <linux/serial_core.h>
#include <linux/types.h>
#include <linux/dmaengine.h>

/*****************************************************************************
 *
 * Structures
 *
 ******************************************************************************/
struct tcc_uart_clksrc {
	const char	    *name;
	unsigned int	divisor;
	unsigned int	min_baud;
	unsigned int	max_baud;
};

struct tcc_uart_info {
	char			*name;
	unsigned int	type;
	unsigned int	fifosize;
};

struct tcc_reg_info {
	unsigned long	bDLL;
	unsigned long	bIER;
	unsigned long	bDLM;
	unsigned long	bLCR;
	unsigned long	bMCR;
	unsigned long	bSCR;
	unsigned long	bFCR;
	unsigned long	bMSR;
	unsigned long	bAFT;
	unsigned long	bUCR;
};

typedef struct _tcc_dma_buf_t {
    char            *addr;
    dma_addr_t      dma_addr;
    unsigned int    buf_size; // total size of DMA
    unsigned long   dma_core;
    unsigned int    dma_ch;
    unsigned int    dma_port;
    unsigned int    dma_intr;
    unsigned int    dma_mode;
} tcc_dma_buf_t;

struct tcc_uart_platform_data {
    int        bt_use;

    int        tx_dma_use;
    int        tx_dma_buf_size;
    dma_addr_t tx_dma_base;
    dma_addr_t tx_dma_ch;
    int        tx_dma_intr;
    int        tx_dma_mode;

    int        rx_dma_use;
    int        rx_dma_buf_size;
    dma_addr_t rx_dma_base;
    dma_addr_t rx_dma_ch;
    int        rx_dma_intr;
    int        rx_dma_mode;
};

struct tcc_uart_port {
    struct uart_port        port;
    unsigned char           rx_claimed;
    unsigned char           tx_claimed;

    resource_size_t         base_addr;      // Register base address

    unsigned int            baud;
    struct clk              *fclk;
    struct clk              *hclk;
    struct tcc_uart_info    *info;
    struct tcc_uart_clksrc  *clksrc;
    char                    name[10];
    wait_queue_head_t       wait_q;
    int                     fifosize;

    int                     bt_use;
#ifdef CONFIG_TCC_BT_DEV	
    int			    bt_suspend;
#endif
   void (*wake_peer)(struct uart_port *);

// for UART DMA
    spinlock_t		   rx_lock;
    int                     tx_dma_use;
    int                     rx_dma_use;
    tcc_dma_buf_t           tx_dma_buffer;
    tcc_dma_buf_t           rx_dma_buffer;
    wait_queue_head_t       wait_dma_q;
	struct dma_chan 	*chan_tx;
	struct dma_chan 	*chan_rx;
	struct scatterlist 	rx_sg;
	struct scatterlist 	tx_sg;
	unsigned int 		rx_residue;
	unsigned int 		tx_residue;
	int 				tx_dma_probed;
	int 				rx_dma_probed;
	bool 				tx_dma_working;
	bool 				tx_dma_using;

    unsigned long       dma_start_value;
    unsigned long            rx_dma_tail; 
    int                      timer_state;
    struct timer_list    *dma_timer;
	dma_cookie_t 	rx_cookie;
	dma_cookie_t 	tx_cookie;
//

    int                     tx_done;
    struct tcc_reg_info     reg;	// for suspend/resume

	int                     opened;
};

#endif

