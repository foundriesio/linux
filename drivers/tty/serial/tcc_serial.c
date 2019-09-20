/*
 * linux/drivers/tty/serial/tcc_serial.c
 *
 * Based on: drivers/serial/s3c2410.c and driver/serial/bfin_5xx.c
 * Author:  <linux@telechips.com>
 * Created: June 10, 2008
 * Description: Driver for onboard UARTs on the Telechips TCC Series
 *
 * Copyright (C) 2008-2018 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA V
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/serial_reg.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <mach/iomap.h>

#include <linux/dma-mapping.h>
#include <linux/cpufreq.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include "tcc_serial.h"

#define DRV_NAME "tcc-uart"

#define Hw7 0x00000080
#define Hw6 0x00000040
#define Hw5 0x00000020
#define Hw4 0x00000010
#define Hw3 0x00000008
#define Hw2 0x00000004
#define Hw1 0x00000002
#define Hw0 0x00000001

#define TIME_STEP                   1//(1*HZ)

#define TCC_BT_UART 1

/* UARTx IIR Masks */
#define IIR_IDD         0x0E    /* Interrupt ID, Source, Reset */
#define IIR_IPF         Hw0     /* Interrupt Flag   */

// Interrupt Ident. Register IID value
#define IIR_THRE	0x01	/* Transmitter holding register empty */
#define IIR_RDA		0x02	/* Received data available */
#define IIR_RLS		0x03	/* Receiver line status */
#define IIR_CTI		0x06	/* Character timeout indication */

/* UARTx_FCR Masks                                  */
#define FCR_FE          Hw0     /* FIFO Enable      */
#define FCR_RXFR        Hw1     /* RX FIFO Reset    */
#define FCR_TXFR        Hw2     /* TX FIFO Reset    */
#define FCR_DRTE        Hw3     /* DMA Mode Select  */

/* UARTx_LCR Masks                                                  */
#define LCR_WLS(x)      (((x)-5) & 0x03)    /* Word Length Select   */
#define LCR_STB         Hw2                 /* Stop Bits            */
#define LCR_PEN         Hw3                 /* Parity Enable        */
#define LCR_EPS         Hw4                 /* Even Parity Select   */
#define LCR_SP          Hw5                 /* Stick Parity         */
#define LCR_SB          Hw6                 /* Set Break            */
#define LCR_DLAB        Hw7                 /* Divisor Latch Access */

/* UARTx_MCR Mask                                                   */
#define MCR_RTS         Hw1     /* Request To Sent                  */
#define MCR_LOOP        Hw4     /* Loopback Mode Enable             */
#define MCR_AFE         Hw5     /* Auto Flow Control Enable         */
#define MCR_RS          Hw6     /* RTS Deassert Condition Control   */

/* UARTx_LSR Masks                                          */
#define LSR_DR          Hw0     /* Data Ready               */
#define LSR_OE          Hw1     /* Overrun Error            */
#define LSR_PE          Hw2     /* Parity Error             */
#define LSR_FE          Hw3     /* Framing Error            */
#define LSR_BI          Hw4     /* Break Interrupt          */
#define LSR_THRE        Hw5     /* THR Empty                */
#define LSR_TEMT        Hw6     /* TSR and UART_THR Empty   */

/* UARTx_IER Masks */
#define IER_ERXI        Hw0
#define IER_ETXI        Hw1
#define IER_ELSI        Hw2

/* UARTx UCR Masks */
#define UCR_TxDE        Hw0
#define UCR_RxDE        Hw1
#define UCR_TWA         Hw2
#define UCR_RWA         Hw3

#define OFFSET_THR    0x00	/* Transmit Holding register            */
#define OFFSET_RBR    0x00	/* Receive Buffer register              */
#define OFFSET_DLL    0x00	/* Divisor Latch (Low-Byte)             */
#define OFFSET_IER    0x04	/* Interrupt Enable Register            */
#define OFFSET_DLM    0x04	/* Divisor Latch (High-Byte)            */
#define OFFSET_IIR    0x08	/* Interrupt Identification Register    */
#define OFFSET_FCR    0x08	/* FIFO Control Register                */
#define OFFSET_LCR    0x0C	/* Line Control Register                */
#define OFFSET_MCR    0x10	/* Modem Control Register               */
#define OFFSET_LSR    0x14	/* Line Status Register                 */
#define OFFSET_MSR    0x18	/* Modem Status Register                */
#define OFFSET_SCR    0x1C	/* SCR Scratch Register                 */
#define OFFSET_AFT    0x20	/* AFC Trigger Level Register           */
#define OFFSET_UCR    0x24	/* UART Control Register                */

#define portaddr(port, reg) ((port)->membase + (reg))

#define rd_regb(port, reg) (__raw_readb(portaddr(port, reg)))
#define rd_regl(port, reg) (__raw_readl(portaddr(port, reg)))

#define wr_regl(port, reg, val) \
	do { __raw_writel(val, portaddr(port, reg)); } while(0)
#define wr_regb(port, reg, val) \
	do { __raw_writeb(val, portaddr(port, reg)); } while(0)


/* configuration defines */
#if 0
#define dbg(fmt,arg...) printk("==== tcc uart: "fmt, ##arg);
#define dbg_on 1
#else /* no debug */
#define dbg(x...) do {} while(0)
#define dbg_on 0
#endif
/* uart pm debug */
#if 0
#define pm_dbg(fmt,arg...) printk("==== tcc uart pm: "fmt, ##arg);
#else /* no debug */
#define pm_dbg(x...) do {} while(0)
#endif

/* UART name and device definitions */

#define TCC_SERIAL_NAME	    "ttyS"
#define TCC_SERIAL_MAJOR	204
#define TCC_SERIAL_MINOR	64

/* conversion functions */
#define tcc_dev_to_port(__dev) (struct uart_port *)dev_get_drvdata(__dev)
#define tx_enabled(port)	((port)->unused[0])
#define rx_enabled(port)	((port)->unused[1])
#define port_used(port)		((port)->quirks)

struct tcc_uart_port tcc_serial_ports[NR_PORTS] = {
	[0] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[0].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 0,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART0),
		.name = "uart0"
	},
	[1] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[1].port.lock),
			.iotype		= UPIO_MEM,
			.uartclk	= 0,
			.irq		= INT_UART,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 1,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART1),
		.name = "uart1"
	},
	[2] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[2].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 2,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART2),
		.name = "uart2"
	},
	[3] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[3].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 3,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART3),
		.name = "uart3"
	},
	[4] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[4].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 4,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART4),
		.name = "uart4"
	},
	[5] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[5].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 5,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART5),
		.name = "uart5"
	},
	[6] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[6].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 6,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART6),
		.name = "uart6"
	},
	[7] = {
		.port = {
			.lock		= __SPIN_LOCK_UNLOCKED(tcc_uart_port[7].port.lock),
			.iotype		= UPIO_MEM,
			.irq		= INT_UART,
			.uartclk	= 0,
			.fifosize	= FIFOSIZE,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 7,
		},
		.base_addr      = tcc_p2v(TCC_PA_UART7),
		.name = "uart7"
	},	
};

static struct device tcc_uart_dev[8];
static unsigned long uartPortCFG0, uartPortCFG1;

static void __iomem *portcfg_base = NULL;

static void *tcc_free_dma_buf(tcc_dma_buf_t *dma_buf)
{
	dbg("%s\n", __func__);
	if (dma_buf) {
		if (dma_buf->dma_addr != 0) {
			dma_free_writecombine(0, dma_buf->buf_size,
					dma_buf->addr, dma_buf->dma_addr);
		}
		memset(dma_buf, 0, sizeof(tcc_dma_buf_t));
	}
	return NULL;
}

static void *tcc_malloc_dma_buf(tcc_dma_buf_t *dma_buf, int buf_size)
{
	int i;
	dbg("%s\n", __func__);
	if (dma_buf) {
		tcc_free_dma_buf(dma_buf);
		dma_buf->buf_size = buf_size;
		dma_buf->addr = dma_alloc_writecombine(0, dma_buf->buf_size,
				&dma_buf->dma_addr, GFP_KERNEL);
		for(i=0; i<buf_size;i++){
			dma_buf->addr[i]=0;
		}
		dbg("Malloc DMA buffer @0x%X(Phy=0x%X), size:%d\n",
				(unsigned int)dma_buf->addr,
				(unsigned int)dma_buf->dma_addr,
				dma_buf->buf_size);
		return dma_buf->addr;
	}
	return NULL;
}

void *kerneltimer_timeover(void *arg );

void kerneltimer_registertimer(struct timer_list* ptimer,
		unsigned long timeover,struct uart_port *port)
{
	init_timer( ptimer );
	ptimer->expires  = get_jiffies_64() + timeover;
	ptimer->data     = (unsigned long)port;
	ptimer->function = (void *)kerneltimer_timeover;
	add_timer( ptimer);
}

static int tcc_run_rx_dma(struct uart_port *port);

static void tcc_dma_rx_timeout(void *arg)
{
	struct uart_port *port = (struct uart_port *) arg;
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct dma_chan *chan = tp->chan_rx;
	struct dma_tx_state state;
	unsigned int ch, i;
	unsigned int last;
	unsigned uerstat = 0, flag = 0;
	char *buf;

	spin_lock(&tp->rx_lock);
	chan->device->device_tx_status(chan, tp->rx_cookie, &state);
	buf = (char *)tp->rx_dma_buffer.addr;

	if(tp->rx_residue < state.residue) {
		last = state.residue;
		if(last > SERIAL_RX_DMA_BUF_SIZE) {
			last = SERIAL_RX_DMA_BUF_SIZE;
		}

		/*if(state.residue < tp->rx_residue)
		  last = SERIAL_RX_DMA_BUF_SIZE;*/

		for(i = tp->rx_residue; i < last; i++) {
			ch = buf[i];

			/*if (uart_handle_sysrq_char(port, ch)) {
			  spin_unlock(&tp->rx_lock);
			  return;
			  }*/
			//[> put the received char into UART buffer <]
			uart_insert_char(port, uerstat, UART_LSR_OE, ch, flag);
			tty_flip_buffer_push(&port->state->port);
			port->icount.rx++;
		}

		tp->rx_residue = last;
		//spin_unlock(&tp->rx_lock);
	}
	else if(tp->rx_residue > state.residue)
	{
		last = SERIAL_RX_DMA_BUF_SIZE;

		if(tp->rx_residue != SERIAL_RX_DMA_BUF_SIZE) {
			for(i = tp->rx_residue; i < last; i++) {
				ch = buf[i];
				uart_insert_char(port, uerstat, UART_LSR_OE, ch, flag);
				tty_flip_buffer_push(&port->state->port);
				port->icount.rx++;
			}
		}
		last = state.residue;
		for(i = 0; i < last; i++) {
			ch = buf[i];
			uart_insert_char(port, uerstat, UART_LSR_OE, ch, flag);
			tty_flip_buffer_push(&port->state->port);
			port->icount.rx++;
		}
		tp->rx_residue = state.residue;
	}
	spin_unlock(&tp->rx_lock);
}

void *kerneltimer_timeover(void *arg)
{
	struct uart_port *port = arg;
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	tcc_dma_rx_timeout(arg);

	if (tp->timer_state == 1) {
		kerneltimer_registertimer( tp->dma_timer, TIME_STEP,arg );
	}

	return 0;
}

int kerneltimer_init(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	tp->dma_timer= kmalloc( sizeof( struct timer_list ), GFP_KERNEL );

	if(tp->dma_timer == NULL ) {
		return -ENOMEM;
	}

	memset( tp->dma_timer, 0, sizeof( struct timer_list) );
	kerneltimer_registertimer( tp->dma_timer,10,port );

	return 0;
}

/* translate a port to the device name */
static inline const char *tcc_serial_portname(struct uart_port *port)
{
	return to_platform_device(port->dev)->name;
}

void tcc_serial_uart_putchar(struct uart_port *port, int ch)
{
	while (!(rd_regl(port, OFFSET_LSR) & LSR_THRE))
		cpu_relax();

	wr_regb(port, OFFSET_THR, ch);
}

static void tcc_serial_rx(struct uart_port *port, unsigned int lsr)
{
	unsigned int flag;
	unsigned int ch = 0;

	ch = rd_regb(port, OFFSET_RBR);

	flag = TTY_NORMAL;
	port->icount.rx++;
	if (lsr & UART_LSR_BI) {
		port->icount.brk++;
		return;
	}

	if (lsr & UART_LSR_PE)
		port->icount.parity++;

	if (lsr & UART_LSR_FE)
		port->icount.frame++;

	if (lsr & UART_LSR_OE)
		port->icount.overrun++;

	lsr &= port->read_status_mask;

	if (lsr & UART_LSR_BI)
		flag = TTY_BREAK;
	else if (lsr & UART_LSR_PE)
		flag = TTY_PARITY;
	else if (lsr & ( UART_LSR_FE | UART_LSR_OE))
		flag = TTY_FRAME;

	if (uart_handle_sysrq_char(port, ch))
		return ;
	/* put the received char into UART buffer */
	uart_insert_char(port, lsr, UART_LSR_OE, ch, flag);

}

static void tcc_serial_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	if (tp->tx_dma_probed)
		tp->tx_done = 0;

	tp->fifosize = uart_circ_chars_pending(xmit);
	if (tp->fifosize > FIFOSIZE) {
		tp->fifosize = FIFOSIZE;
		if ((xmit->tail + tp->fifosize) > UART_XMIT_SIZE )
			tp->fifosize = UART_XMIT_SIZE - xmit->tail;
	}

	if (port->x_char) {
		if(tp->fifosize > 1)
			wr_regl(port, OFFSET_IER, (rd_regl(port, OFFSET_IER) & ~IER_ETXI));

		tcc_serial_uart_putchar(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		tp->fifosize--;
	}

	if (uart_circ_empty(xmit)|| uart_tx_stopped(port)) {
		wr_regl(port, OFFSET_IER, (rd_regl(port, OFFSET_IER) & ~IER_ETXI));
		return ;
	}

	if(!(rd_regl(port, OFFSET_LSR) & LSR_THRE))
		goto tx_exit;

	while (tp->fifosize > 0) {

		wr_regb(port, OFFSET_THR, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		tp->fifosize--;

		if(uart_circ_empty(xmit))
			break;
	} //// while
tx_exit:
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);
}

static void tcc_serial_stop_tx(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct circ_buf *xmit = &port->state->xmit;
	struct dma_tx_state state;
	int count;

	dbg("%s\n", __func__);

	tp->tx_done = 1;

	while(!((rd_regl(port, OFFSET_LSR)) & LSR_TEMT))
		continue;

	if(tp->chan_tx != NULL) {
		dmaengine_terminate_all(tp->chan_tx);
		dmaengine_tx_status(tp->chan_tx, tp->tx_cookie, &state);
		count = tp->tx_residue - state.residue;
		xmit->tail = (xmit->tail + count) & (UART_XMIT_SIZE - 1);
		tp->tx_dma_working = false;
		tp->chan_tx = NULL;
	}

	if(tx_enabled(port)) {
		wr_regl(port, OFFSET_IER, (rd_regl(port, OFFSET_IER) & ~IER_ETXI));
		tx_enabled(port) = 0;
	}
}

static void tcc_serial_stop_rx(struct uart_port *port)
{
	u32 ier;

	dbg("%s  line[%d] irq[%d]\n", __func__, port->line, port->irq);

	if (rx_enabled(port)) {
		ier = rd_regl(port, OFFSET_IER);
		wr_regl(port, OFFSET_IER, (ier & ~IER_ERXI));

		mdelay(10);
		rx_enabled(port) = 0;
	}
}

static void tcc_serial_enable_ms(struct uart_port *port)
{
	dbg("%s\n", __func__);
}

static irqreturn_t tcc_serial_interrupt(int irq, void *id)
{
	void __iomem *ists_reg = portcfg_base + 0x0C;
	struct uart_port *port = id;
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	unsigned int lsr_data = 0;
	unsigned int iir_data = 0;
	unsigned long flags;

	if ((__raw_readl(ists_reg) & (1<<(port->line))) == 0)
		return IRQ_NONE;

	spin_lock_irqsave(&port->lock,flags);

	iir_data = rd_regl(port, OFFSET_IIR);
	iir_data = (iir_data & 0x0E) >> 1;

	lsr_data = rd_regl(port, OFFSET_LSR);

	if (iir_data == IIR_RDA || iir_data == IIR_CTI) {
		if(!tp->rx_dma_probed) {
			tcc_serial_rx(port, lsr_data);
			tty_flip_buffer_push(&port->state->port);
		}
	}
	else if (iir_data == IIR_THRE) {
		tcc_serial_tx(port);
	}

	spin_unlock_irqrestore(&port->lock,flags);

	return IRQ_HANDLED;
}

static unsigned int tcc_serial_tx_empty(struct uart_port *port)
{
	unsigned short lsr;

	lsr = rd_regl(port, OFFSET_LSR);
	if (lsr & LSR_TEMT)
		return TIOCSER_TEMT;
	else
		return 0;

}

static unsigned int tcc_serial_get_mctrl(struct uart_port *port)
{
	dbg("%s\n", __func__);
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void tcc_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	unsigned int mcr = 0;
	dbg("%s\n", __func__);
	/* todo - possibly remove AFC and do manual CTS */

	mcr = rd_regl(port, OFFSET_MCR);

	if (mctrl & TIOCM_LOOP) {
		mcr |= UART_MCR_LOOP;
	} else {
		mcr &= ~UART_MCR_LOOP;
	}

	wr_regl(port, OFFSET_MCR, mcr);
}

static void tcc_serial_break_ctl(struct uart_port *port, int break_state)
{
	unsigned long flags;
	unsigned int lcr;

	dbg("%s\n", __func__);
	spin_lock_irqsave(&port->lock, flags);

	lcr = rd_regl(port, OFFSET_LCR);
	if (break_state == -1) {
		lcr |= LCR_SB;
	}
	else {
		lcr &= ~LCR_SB;
	}
	wr_regl(port, OFFSET_LCR, lcr);
	spin_unlock_irqrestore(&port->lock,flags);
}

static void tcc_serial_dma_shutdown(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	if(tp->rx_dma_probed) {
		dmaengine_terminate_all(tp->chan_rx);
		tp->rx_dma_probed = false;
		dma_release_channel(tp->chan_rx);
		tcc_free_dma_buf(&(tp->rx_dma_buffer));
		wr_regl(port, OFFSET_UCR, rd_regl(port, OFFSET_UCR) & ~Hw1);
	}

	if(tp->tx_dma_probed) {
		dmaengine_terminate_all(tp->chan_tx);
		tp->tx_dma_probed = false;
		tp->tx_dma_working = false;
		tp->tx_dma_using = false;
		dma_release_channel(tp->chan_tx);
		tcc_free_dma_buf(&(tp->tx_dma_buffer));
	}
}

static void tcc_serial_shutdown(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	dbg("%s\n", __func__);
	if(tp->rx_dma_probed) {
		if(tp->dma_timer != NULL) {
			tp->timer_state=0;
			mdelay(1);
			del_timer(tp->dma_timer);
			kfree(tp->dma_timer);
			tp->dma_timer = NULL;
		}
	}

	tcc_serial_dma_shutdown(port);

	wr_regl(port, OFFSET_IER, 0x0);
	free_irq(port->irq, port);

	port_used(port) = 0;	// for suspend/resume

	dbg("%s   line[%d] out...\n", __func__, port->line);
}

static int tcc_serial_dma_tx(struct uart_port *port);

static void tcc_dma_tx_callback(void *data)
{
	struct uart_port *port = data;
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct circ_buf *xmit = &port->state->xmit;
	struct dma_tx_state state;
	unsigned long flags;
	int count;

	dmaengine_tx_status(tp->chan_tx, tp->tx_cookie, &state);
	count = tp->tx_residue - state.residue;

	spin_lock_irqsave(&port->lock, flags);
	xmit->tail = (xmit->tail + count) & (UART_XMIT_SIZE - 1);
	port->icount.tx += count;

	if(uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	tp->tx_dma_working = false;
	if(tp->tx_dma_using == true)
		tcc_serial_dma_tx(port);
	spin_unlock_irqrestore(&port->lock, flags);
}

static int tcc_serial_dma_tx(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct dma_async_tx_descriptor *desc;
	struct circ_buf *xmit = &port->state->xmit;
	unsigned long count;
	unsigned int count_fst;
	unsigned int count_snd;

	if(tp->tx_dma_working == true) {
		return 0;
	}

	count = uart_circ_chars_pending(xmit);
	if(!count) {
		return 0;
	}

	if(xmit->tail < xmit->head) {
		memcpy(tp->tx_dma_buffer.addr, &xmit->buf[xmit->tail], count);
	}
	else {
		count_fst = UART_XMIT_SIZE - xmit->tail;

		if(count_fst > count) {
			count_fst = count;
		}

		count_snd = count - count_fst;

		memcpy(tp->tx_dma_buffer.addr, &xmit->buf[xmit->tail], count_fst);
		if(count_snd) {
			memcpy(&tp->tx_dma_buffer.addr[count_fst], &xmit->buf[0], count_snd);
		}
	}

	tp->tx_residue = count;
	desc = dmaengine_prep_slave_single(tp->chan_tx, tp->tx_dma_buffer.dma_addr, 
			count, DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT);

	desc->callback = tcc_dma_tx_callback;
	desc->callback_param = port;

	tp->tx_cookie = dmaengine_submit(desc);
	dma_async_issue_pending(tp->chan_tx);

	tp->tx_dma_working = true;

	return 0;
}

static int tcc_serial_tx_dma_probe(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct dma_slave_config tx_conf;
	int ret;

	if(tp->tx_dma_probed == true)
		return 0;

	wr_regl(port, OFFSET_UCR, rd_regl(port, OFFSET_UCR) & ~UCR_TxDE);

	tp->chan_tx = dma_request_slave_channel(port->dev, "tx");
	if(tp->chan_tx == NULL) {
		dev_dbg(port->dev, "Failed to request slave tx\n");
		return -1;
	}

	tx_conf.direction = DMA_MEM_TO_DEV;
	tx_conf.dst_addr = port->mapbase;
	tx_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;

	ret = dmaengine_slave_config(tp->chan_tx, &tx_conf);
	if(ret) {
		dev_err(port->dev, "Config DMA Tx failed\n");
	}

	if(!tcc_malloc_dma_buf(&(tp->tx_dma_buffer), UART_XMIT_SIZE)) {
		dbg("Unable to attach UART TX DMA channel\n");
		return -ENOMEM;
	}

	wr_regl(port, OFFSET_UCR, rd_regl(port, OFFSET_UCR) | UCR_TxDE);
	tp->tx_dma_probed = true;
	tp->tx_dma_working = false;

	return 0;
}

static void tcc_dma_rx_callback(void *data)
{
	struct uart_port *port = data;

	// RTS High
	wr_regl(port, OFFSET_MCR, rd_regl(port, OFFSET_MCR) & ~Hw1);

	tcc_run_rx_dma(port);

	// RTS Low
	wr_regl(port, OFFSET_MCR, rd_regl(port, OFFSET_MCR) | Hw1);
}

static int tcc_run_rx_dma(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct dma_async_tx_descriptor *desc;
	struct scatterlist *sg = &tp->rx_sg;

	desc = dmaengine_prep_slave_sg(tp->chan_rx, sg, 1,
			DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);

	desc->callback = tcc_dma_rx_callback;
	desc->callback_param = port;
	tp->rx_cookie = dmaengine_submit(desc);
	dma_async_issue_pending(tp->chan_rx);

	wr_regl(port, OFFSET_UCR, rd_regl(port, OFFSET_UCR) | UCR_RxDE);

	return 0;
}

static int tcc_serial_rx_dma_probe(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	struct dma_slave_config rx_conf;
	struct scatterlist *sg = &tp->rx_sg;
	int ret;

	if(tp->rx_dma_probed == true) {
		return 0;
	}

	wr_regl(port, OFFSET_UCR, rd_regl(port, OFFSET_UCR) & ~UCR_RxDE);

	tp->chan_rx = dma_request_slave_channel(port->dev, "rx");
	if(tp->chan_rx == NULL) {
		dev_dbg(port->dev, "Failed to request slave rx\n");
		return -1;
	}

	rx_conf.direction = DMA_DEV_TO_MEM;
	rx_conf.src_addr = port->mapbase;
	rx_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;

	ret = dmaengine_slave_config(tp->chan_rx, &rx_conf);
	if(ret) {
		dev_err(port->dev, "Config DMA Rx failed\n");
	}

	if(!tcc_malloc_dma_buf(&(tp->rx_dma_buffer), SERIAL_RX_DMA_BUF_SIZE)) {
		dbg("Unable to attach UART RX DMA 1 channel\n");
		return -ENOMEM;
	}

	sg_init_table(sg, 1);
	sg_set_page(sg, phys_to_page(tp->rx_dma_buffer.dma_addr),
			SERIAL_RX_DMA_BUF_SIZE,
			offset_in_page(tp->rx_dma_buffer.dma_addr));
	sg_dma_address(sg) = tp->rx_dma_buffer.dma_addr;
	sg_dma_len(sg) = SERIAL_RX_DMA_BUF_SIZE;

	tp->timer_state = 1;

	tp->rx_dma_probed = true;

	kerneltimer_init(port);

	return 0;
}

static int tcc_serial_dma_probe(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	wr_regl(port, OFFSET_MCR, rd_regl(port, OFFSET_MCR) | MCR_RTS);
	wr_regl(port, OFFSET_AFT, 0x00000021);

	if(tcc_serial_rx_dma_probe(port))
		tp->rx_dma_probed = false;

	if(tcc_serial_tx_dma_probe(port))
		tp->tx_dma_probed = false;

	return 0;
}

static void tcc_serial_start_tx(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	if(!tx_enabled(port))
		tx_enabled(port) = 1;

	if(tp->tx_dma_probed == true) {
		tp->tx_dma_using = true;
		tcc_serial_dma_tx(port);
	}
	else {
		wr_regl(port, OFFSET_IER, rd_regl(port, OFFSET_IER) | IER_ETXI);
	}
}

/*
 * while application opening the console device, this function will invoked
 * This function will initialize the interrupt handling
 */
static int tcc_serial_startup(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	int retval=0;
	unsigned int lcr;
	unsigned long flags;

	dbg("%s() line[%d] in...\n", __func__, port->line);

	tx_enabled(port) = 1;
	rx_enabled(port) = 1;
	port_used(port) = 1;	// for suspend/resume

	/* clear interrupt */
	wr_regl(port, OFFSET_IER, 0x0);

	lcr = rd_regl(port, OFFSET_LCR);
	wr_regl(port, OFFSET_LCR, (lcr | LCR_DLAB));

	wr_regl(port, OFFSET_FCR, (FCR_TXFR|FCR_RXFR|FCR_FE));    /* FIFO Enable, Rx/Tx FIFO reset */
	tp->fifosize = FIFOSIZE;
	tp->reg.bFCR = 0x07;          /* for resume restore */

	if(tp->tx_dma_probed)
		tp->tx_done  = 1;

	wr_regl(port, OFFSET_LCR, (lcr & (~LCR_DLAB)));

	rd_regl(port, OFFSET_IIR);

	//set cpu for processing of irq
	//irq_set_affinity(port->irq, cpumask_of(1));
	retval = request_irq(port->irq, tcc_serial_interrupt,
			IRQF_SHARED, tp->name , port);
	printk("request serial irq:%d,retval:%d\n", port->irq, retval);

	tp->rx_residue = 0;
	tcc_serial_dma_probe(port);

	if(tp->rx_dma_probed == true) {
		tcc_run_rx_dma(port);
	}
	else {
		spin_lock_irqsave(&port->lock, flags);
		wr_regl(port, OFFSET_IER, IER_ERXI);
		spin_unlock_irqrestore(&port->lock, flags);
	}

	tp->opened = 1;

	dbg(" %s() out...\n", __func__);
	return retval;
}

/* power power management control */
static void tcc_serial_pm(struct uart_port *port, unsigned int level, unsigned int old)
{
}

static void tcc_serial_set_baud(struct tcc_uart_port *tp, unsigned int baud)
{
	/* Set UARTx peripheral clock */
	switch(baud) {
		case 921600:
			if (tp->fclk)
				clk_set_rate(tp->fclk, 103219200);	// 103.219MHz
			break;
		case 2500000:
			if (tp->fclk)
				clk_set_rate(tp->fclk, 120*1000*1000);	// 120MHz
			break;
		default:
			if (tp->fclk)
				clk_set_rate(tp->fclk, 96*1000*1000);	// 96MHz
			break;
	}
}

#if defined(CONFIG_HIBERNATION) && defined(CONFIG_SERIAL_TCC_CONSOLE)
extern unsigned int do_hibernate_boot;
#endif

static void tcc_serial_set_termios(struct uart_port *port, struct ktermios *termios,
		struct ktermios *old)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);
	unsigned long flags;
	unsigned int baud, quot;
	unsigned int ulcon;
	unsigned int umcon;
	int uart_clk = 0;

	/*
	 * We don't support modem control lines.
	 */
	termios->c_cflag &= ~(HUPCL | CMSPAR);
	termios->c_cflag |= CLOCAL;

	/* Ask the core to calculate the baud rate. */
	baud = uart_get_baud_rate(port, termios, old, 0, 3500000);

#if defined(CONFIG_HIBERNATION) && defined(CONFIG_SERIAL_TCC_CONSOLE)
	if(unlikely(do_hibernate_boot && tp->port.line == CONSOLE_PORT)) {
		tcc_serial_set_baud(tp, baud);
		port->uartclk = baud;
	}
#endif

	/*
	 * set byte size
	 */
	switch (termios->c_cflag & CSIZE) {
		case CS5:
			dbg("config: 5bits/char  cflag[0x%x]\n", termios->c_cflag );
			ulcon = 0;
			break;
		case CS6:
			dbg("config: 6bits/char  cflag[0x%x]\n", termios->c_cflag );
			ulcon = 1;
			break;
		case CS7:
			dbg("config: 7bits/char  cflag[0x%x]\n", termios->c_cflag );
			ulcon = 2;
			break;
		case CS8:
		default:
			dbg("config: 8bits/char  cflag[0x%x]\n", termios->c_cflag );
			ulcon = 3;
			break;
	}

	spin_lock_irqsave(&port->lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	tcc_serial_set_baud(tp, baud);
	port->uartclk = baud;

	/*
	 * Ask the core to calculate the divisor for us.
	 */

	if (tp->fclk)
		uart_clk = clk_get_rate(tp->fclk);
	quot = (uart_clk + ((16*baud)>>1))/(16*baud);


	/* preserve original lcon IR settings */
	if (termios->c_cflag & CSTOPB)
		ulcon |= LCR_STB;

	umcon = (termios->c_cflag & CRTSCTS) ? (MCR_AFE|MCR_RTS) : MCR_RTS;

	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			ulcon |= (LCR_PEN);
		else
			ulcon |= (LCR_EPS|LCR_PEN);
	} else {
		ulcon &= ~(LCR_EPS|LCR_PEN);
	}

	//	tcc_serial_input_buffer_set(tp->port.line, 1);

	wr_regl(port, OFFSET_MCR, umcon);
	wr_regl(port, OFFSET_LCR, (ulcon | LCR_DLAB));
	if(quot > 0xFF) {
		wr_regl(port, OFFSET_DLL, quot & 0x00FF);
		wr_regl(port, OFFSET_DLM, quot >> 8);
	} else if (quot > 0) {
		wr_regl(port, OFFSET_DLL, quot);
		wr_regl(port, OFFSET_DLM, 0x0);
	}
	wr_regl(port, OFFSET_LCR, (ulcon & (~LCR_DLAB)));


	/*
	 * Which character status flags are we interested in?
	 */
	port->read_status_mask = 0;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= 0;

	/*
	 * Which character status flags should we ignore?
	 */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= LSR_PE | LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= LSR_BI;

		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= LSR_OE;
	}

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= 0;

	spin_unlock_irqrestore(&port->lock, flags);

	dbg("[UART%02d] setting ulcon: %08x, umcon: %08x, brddiv to %d, baud %d, uart_clk %d\n", 
			port->line, ulcon, umcon, quot, baud, uart_clk);
}

static const char *tcc_serial_type(struct uart_port *port)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	return tp->name;
}

#define MAP_SIZE (0x100)

static void tcc_serial_release_port(struct uart_port *port)
{
	/* TODO */
	//release_mem_region(port->mapbase, MAP_SIZE);
}

static int tcc_serial_request_port(struct uart_port *port)
{
	return 0;
	/*
	   return request_mem_region(port->mapbase, MAP_SIZE, "tcc7901") ? 0 : -EBUSY;
	   */
}

static void tcc_serial_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE && tcc_serial_request_port(port) == 0)
		port->type = PORT_TCC;
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int tcc_serial_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	if(tp->info == NULL)
		return -EINVAL;

	if (ser->type != PORT_UNKNOWN && ser->type != tp->info->type)
		return -EINVAL;
	return 0;
}

int tcc_uart_enable(int port_num )
{
	int ret;

	ret = pm_runtime_get_sync(&tcc_uart_dev[port_num]);

	pm_dbg("%s ret : %d\n", __func__, ret);

	return 0;
}

int tcc_uart_disable(int port_num)
{
	int ret;

	ret = pm_runtime_put_sync_suspend(&tcc_uart_dev[port_num]);

	pm_dbg("%s ret : %d\n", __func__, ret);

	return 0;
}
EXPORT_SYMBOL(tcc_uart_disable);
EXPORT_SYMBOL(tcc_uart_enable);

#ifdef CONFIG_SERIAL_TCC_CONSOLE
static struct console tcc_serial_console;
#define TCC_SERIAL_CONSOLE      &tcc_serial_console
#else
#define TCC_SERIAL_CONSOLE      NULL
#endif

static struct uart_ops tcc_serial_ops = {
	.pm		= tcc_serial_pm,
	.tx_empty	= tcc_serial_tx_empty,
	.get_mctrl	= tcc_serial_get_mctrl,
	.set_mctrl	= tcc_serial_set_mctrl,
	.stop_tx	= tcc_serial_stop_tx,
	.start_tx	= tcc_serial_start_tx,
	.stop_rx	= tcc_serial_stop_rx,
	.enable_ms	= tcc_serial_enable_ms,
	.break_ctl	= tcc_serial_break_ctl,
	.startup	= tcc_serial_startup,
	.shutdown	= tcc_serial_shutdown,
	.set_termios	= tcc_serial_set_termios,
	.type		= tcc_serial_type,
	.release_port	= tcc_serial_release_port,
	.request_port	= tcc_serial_request_port,
	.config_port	= tcc_serial_config_port,
	.verify_port	= tcc_serial_verify_port,
};

static struct uart_driver tcc_uart_drv = {
	.owner          = THIS_MODULE,
	.dev_name       = TCC_SERIAL_NAME,
	.nr             = NR_PORTS,
	.cons           = TCC_SERIAL_CONSOLE,
	.driver_name    = DRV_NAME,
	.major          = TCC_SERIAL_MAJOR,
	.minor          = TCC_SERIAL_MINOR,
};

/*  initialise  serial port information */
/* cpu specific variations on the serial port support */
static struct tcc_uart_info tcc_uart_inf = {
	.name		= "Telechips UART",
	.type		= PORT_TCC,
	.fifosize	= FIFOSIZE,
};

static int tcc_serial_remove(struct platform_device *dev)
{
	struct uart_port *port = tcc_dev_to_port(&dev->dev);
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	if (port)
		uart_remove_one_port(&tcc_uart_drv, port);

	if (tp->tx_dma_probed){
		tcc_free_dma_buf(&(tp->tx_dma_buffer));
		dma_release_channel(tp->chan_tx);
	}

	if (tp->rx_dma_probed) {
		tcc_free_dma_buf(&(tp->rx_dma_buffer));
		dma_release_channel(tp->chan_rx);
		wr_regl(port, OFFSET_UCR, rd_regl(port, OFFSET_UCR) & ~Hw1);

		// DMA channel is disabled.
		//tca_dma_clren(tp->rx_dma_buffer.dma_ch,
		//      (unsigned long *)tp->rx_dma_buffer.dma_core);
	}

	if (tp->fclk) {
		clk_disable_unprepare(tp->fclk);
		clk_put(tp->fclk);
		tp->fclk = NULL;
	}

	if(tp->hclk) {
		clk_disable_unprepare(tp->hclk);
		clk_put(tp->hclk);
		tp->hclk = NULL;
	}

	return 0;
}

/* UART power management code */

#ifdef CONFIG_PM
/*-------------------------------------------------
 * TODO: handling DMA_PORT suspend/resume (TCC79X)
 *       DMA stop and start ...
 *-------------------------------------------------*/
static int tcc_serial_suspend(struct device *dev)
{
	struct uart_port *port = tcc_dev_to_port(dev);
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

	uartPortCFG0 = *(volatile unsigned long *) (portcfg_base);
	uartPortCFG1 = *(volatile unsigned long *) (portcfg_base + 0x4);

	dbg("%s in...\n", __func__);

	if (port) {
		//port->suspended = 1;

		uart_suspend_port(&tcc_uart_drv, port);

#if defined(CONFIG_PM_CONSOLE_NOT_SUSPEND)
		if(!port->cons || (port->cons->index != port->line)){
#endif
			if (tp->fclk)
				clk_disable_unprepare(tp->fclk);
			if (tp->hclk)
				clk_disable_unprepare(tp->hclk);

#if defined(CONFIG_PM_CONSOLE_NOT_SUSPEND)
		}
#endif

	}
	dbg("%s out...\n", __func__);
	return 0;
}

static int tcc_serial_resume(struct device *dev)
{
	struct uart_port *port = tcc_dev_to_port(dev);
	struct tcc_uart_port *tp =
		container_of(port, struct tcc_uart_port, port);

#if defined(CONFIG_PM_CONSOLE_NOT_SUSPEND)
	if(!port->cons || (port->cons->index != port->line)){
#endif
		if (tp->hclk)
			clk_prepare_enable(tp->hclk);
		if (tp->fclk)
			clk_prepare_enable(tp->fclk);

#if defined(CONFIG_PM_CONSOLE_NOT_SUSPEND)
	}
#endif

	*(volatile unsigned long *) (portcfg_base) = uartPortCFG0;
	*(volatile unsigned long *) (portcfg_base + 0x4) = uartPortCFG1;

	if (port) {
		if (port->suspended) {

			uart_resume_port(&tcc_uart_drv, port);

			port->suspended = 0;
		}
	}

	dbg("%s out...\n", __func__);
	return 0;
}

#else
#define tcc_serial_suspend NULL
#define tcc_serial_resume  NULL
#endif

static const struct dev_pm_ops tcc_serial_pm_ops = {
	.suspend = tcc_serial_suspend,
	.resume = tcc_serial_resume,
	.freeze = tcc_serial_suspend,
	.thaw = tcc_serial_resume,
	.restore = tcc_serial_resume,
};

extern void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

static int tcc_serial_portcfg(struct device_node *np, struct uart_port *port)
{
	unsigned int portcfg;
	int port_mux;

	if(of_property_read_u32(np, "port-mux", &port_mux) == 0) {
		dbg("%s, port_mux:%d\n", __func__, port_mux);

		portcfg_base = ioremap(TCC_PA_UARTPORTCFG, 0x8);

		if(port->line < 4) {
			portcfg = readl(portcfg_base);
			portcfg = (portcfg & ~(0xFF << (port->line * 8)))
				| (port_mux << (port->line * 8));
			writel(portcfg, portcfg_base);
		}
		else {
			portcfg = readl(portcfg_base + 0x4);
			portcfg = (portcfg & ~(0xFF << ((port->line - 4) * 8)))
				| (port_mux << ((port->line -4) * 8));
			writel(portcfg, portcfg_base + 0x4);
		}

		if(!portcfg_base)
			iounmap(portcfg_base);
	}
	else {
		dev_err(port->dev, "UART%d failed port cofiguration\n", port->line);
		return -EINVAL;
	}

	return 0;
}

static int tcc_serial_probe(struct platform_device *dev)
{
	struct resource *mem;
	struct tcc_uart_port *tp;
	struct uart_port *port;
	struct device_node *np = dev->dev.of_node;
	int irq;
	int id;
	int ret;

	if (np)
		id = of_alias_get_id(np, "serial");
	else
		id = dev->id;
	dbg("%s: id = %d\n", __func__, id);

	tp = &tcc_serial_ports[id];
	port = &tp->port;

	mem = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&dev->dev, "[UART%d] no memory resource?\n", id);
		return -EINVAL;
	}
	port->membase = devm_ioremap_resource(&dev->dev, mem);
	if (!port->membase) {
		dev_err(port->dev, "failed to ioremap\n");
		return -ENOMEM;
	}

	port->mapbase  = mem->start;
	irq = platform_get_irq(dev, 0);
	if (irq < 0) {
		dev_err(port->dev, "[UART%d] no irq resource?\n", id);
		return -ENODEV;
	}

	if(tcc_serial_portcfg(np, port) != 0)
		return -EINVAL;

	/* Bus Clock Enable of UARTx */
	tp->hclk = of_clk_get(np, 0);
	if (IS_ERR(tp->hclk))
		tp->hclk = NULL;
	else
		clk_prepare_enable(tp->hclk);

	tp->fclk = of_clk_get(np, 1);
	if (IS_ERR(tp->fclk))
		tp->fclk = NULL;
	else
		clk_prepare_enable(tp->fclk);

	dbg("initialising uart ports...\n");

	tp->tx_dma_probed = false;
	tp->rx_dma_probed = false;

	port->iotype	= UPIO_MEM;
	port->irq	= irq;
	port->uartclk	= 0;
	port->flags	= UPF_BOOT_AUTOCONF;
	port->ops	= &tcc_serial_ops;
	port->fifosize	= FIFOSIZE;
	port->line	= id;
	port->dev	= &dev->dev;
	tcc_uart_dev[id]	= dev->dev;
	tp->port.type     = PORT_TCC;
	tp->port.irq      = irq;
	tp->baud          = 0;
	tp->info          = &tcc_uart_inf;
#if defined(CONFIG_TCC_BCM4330_LPM)
	if(tp->port.line == TCC_BT_UART)
		tp->wake_peer = bcm_bt_lpm_exit_lpm_locked;
#endif

	init_waitqueue_head(&(tp->wait_q));

	ret = uart_add_one_port(&tcc_uart_drv, &tp->port);
	if (ret) {
		dev_err(port->dev, "uart_add_one_port failure\n");
		goto probe_err;
	}

	spin_lock_init(&tp->rx_lock);

	platform_set_drvdata(dev, &tp->port);

	return ret;

probe_err:

	dbg("probe_err\n");

	if (tp == NULL)
		return ret;

	if (tp->fclk) {
		clk_disable_unprepare(tp->fclk);
		clk_put(tp->fclk);
		tp->fclk = NULL;
	}
	if (tp->hclk) {
		clk_disable_unprepare(tp->hclk);
		clk_put(tp->hclk);
		tp->hclk = NULL;
	}

	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id tcc_uart_of_match[] = {
	{ .compatible = "telechips,tcc893x-uart" },
	{ .compatible = "telechips,tcc896x-uart" },
	{ .compatible = "telechips,tcc897x-uart" },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_uart_of_match);
#endif

static struct platform_driver tcc_serial_drv = {
	.probe		= tcc_serial_probe,
	.remove		= tcc_serial_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &tcc_serial_pm_ops,
		.of_match_table = of_match_ptr(tcc_uart_of_match),
	},
};

/* module initialisation code */
static int __init tcc_serial_modinit(void)
{
	int ret;

	ret = uart_register_driver(&tcc_uart_drv);
	if (ret < 0) {
		dbg(KERN_ERR "failed to register UART driver\n");
		return ret;
	}

	ret = platform_driver_register(&tcc_serial_drv);
	if(ret != 0) {
		uart_unregister_driver(&tcc_uart_drv);
		return ret;
	}

	return 0;
}

static void __exit tcc_serial_modexit(void)
{
	platform_driver_unregister(&tcc_serial_drv);
	uart_unregister_driver(&tcc_uart_drv);
}

module_init(tcc_serial_modinit);
module_exit(tcc_serial_modexit);

/***************************************************************
 * The following is Console driver
 *
 ***************************************************************/
#ifdef CONFIG_SERIAL_TCC_CONSOLE

static struct uart_port *cons_uart;

#ifdef CONFIG_DEFERRED_CONSOLE_OUTPUT

#define CONSOLE_BUF_LEN		(1 << CONFIG_LOG_BUF_SHIFT)

static char console_buffer[CONSOLE_BUF_LEN];
static int buffer_head;
static int buffer_tail;
static struct tasklet_struct console_tasklet;

static void tcc_console_tasklet(unsigned long data)
{
	struct uart_port *port = (struct uart_port *) data;

	while (buffer_head != buffer_tail) {
		if (!(rd_regl(port, OFFSET_LSR) & LSR_THRE)) {
			tasklet_schedule(&console_tasklet);
			break;
		}
		wr_regb(port, OFFSET_THR, console_buffer[buffer_tail++]);
		if (buffer_tail == CONSOLE_BUF_LEN)
			buffer_tail = 0;
	}
}
#endif /* CONFIG_DEFERRED_CONSOLE_OUTPUT */

static void tcc_serial_console_putchar(struct uart_port *port, int ch)
{
#ifdef CONFIG_DEFERRED_CONSOLE_OUTPUT
	if (oops_in_progress) {
		while (!(rd_regl(port, OFFSET_LSR) & LSR_THRE))
			cpu_relax();

		wr_regb(port, OFFSET_THR, ch);
	} else {
		console_buffer[buffer_head++] = ch;
		if (buffer_head == CONSOLE_BUF_LEN)
			buffer_head = 0;
		tasklet_schedule(&console_tasklet);
	}
#else
	while (!(rd_regl(port, OFFSET_LSR) & LSR_THRE))
		cpu_relax();

	wr_regb(port, OFFSET_THR, ch);
#endif

}

static void tcc_console_write(struct console *co, const char *s,
		unsigned int count)
{
	struct uart_port *port;
	unsigned int t_ier, b_ier;
	unsigned long flags;
	int locked = 1;

	port = &tcc_serial_ports[co->index].port;

	local_irq_save(flags);
	if(port->sysrq)
		locked = 0;
	else if(oops_in_progress && !dbg_on)
		locked = spin_trylock(&port->lock);
	else{
		if(!dbg_on){
			spin_lock(&port->lock);
		}
	}

	if(!dbg_on){
		t_ier = rd_regl(port, OFFSET_IER);
		b_ier = t_ier;

		wr_regl(port, OFFSET_IER, t_ier & ~IER_ETXI);
	}

	uart_console_write(cons_uart, s, count, tcc_serial_console_putchar);

	if(!dbg_on)
		wr_regl(port, OFFSET_IER, b_ier);


	if(locked && !dbg_on)
		spin_unlock(&port->lock);
	local_irq_restore(flags);

}

static void __init tcc_serial_get_options(struct uart_port *port, int *baud,
		int *parity, int *bits)
{
}

static int __init tcc_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = CONSOLE_BAUDRATE;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	dbg("tcc_serial_console_setup: co=%p (%d), %s\n", co, co->index, options);

	/* is this a valid port */
	if (co->index == -1 || co->index >= NR_PORTS)
		co->index = CONSOLE_PORT;

	port = &tcc_serial_ports[co->index].port;
	port->ops = &tcc_serial_ops;

#ifdef CONFIG_DEFERRED_CONSOLE_OUTPUT
	buffer_head = buffer_tail = 0;
	tasklet_init(&console_tasklet, tcc_console_tasklet,
			(unsigned long) port);
#endif

	/* is the port configured? */
	if (port->mapbase == 0x0) {
		dbg("port->mapbase is 0\n");
		port->mapbase = tcc_serial_ports[co->index].base_addr;
		port->membase = (unsigned char __iomem *)port->mapbase;
		port = &tcc_serial_ports[co->index].port;
		port->ops = &tcc_serial_ops;
	}

	cons_uart = port;

	if (options) {
		dbg("uart_parse_options\n");
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	} else {
		dbg("tcc_serial_get_options\n");
		tcc_serial_get_options(port, &baud, &parity, &bits);
	}

	dbg("tcc_serial_console_setup: port=%p (%d)\n", port, co->index);
	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console tcc_serial_console = {
	.name		= TCC_SERIAL_NAME,
	.device		= uart_console_device,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.write		= tcc_console_write,
	.setup		= tcc_console_setup,
	.data		= &tcc_uart_drv,
};

/*
 * Initialise the console from one of the uart drivers
 */
static int tcc_console_init(void)
{
	//	dbg("%s\n", __func__);

	register_console(&tcc_serial_console);

	return 0;
}
console_initcall(tcc_console_init);
#endif /* CONFIG_SERIAL_TCC_CONSOLE */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC Serial port driver");
