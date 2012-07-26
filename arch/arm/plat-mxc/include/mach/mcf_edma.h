/*
 * mcf_edma.h - Coldfire eDMA driver header file.
 *
 * Copyright 2008-2012 Freescale Semiconductor, Inc.
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _MCF_EDMA_H
#define _MCF_EDMA_H

#include <linux/interrupt.h>

#if defined(CONFIG_M5445X)
#include <asm/mcf5445x_edma.h>
#elif defined(CONFIG_M5441X)
#include <asm/mcf5441x_edma.h>
#elif defined(CONFIG_ARCH_MVF)
#include <mach/mvf.h>
#include <mach/mvf_edma.h>
#endif

#include <linux/scatterlist.h>

#define MCF_EDMA_INT0_CHANNEL_BASE	(8)
#define MCF_EDMA_INT0_CONTROLLER_BASE	(64)
#define MCF_EDMA_INT0_BASE		(MCF_EDMA_INT0_CHANNEL_BASE + \
					 MCF_EDMA_INT0_CONTROLLER_BASE)
#define MCF_EDMA_INT0_NUM		(16)
#define MCF_EDMA_INT0_END		(MCF_EDMA_INT0_NUM)

#if defined(CONFIG_M5441X)
#define MCF_EDMA_INT1_CHANNEL_BASE	(8)
#define MCF_EDMA_INT1_CONTROLLER_BASE	(128)
#define MCF_EDMA_INT1_BASE		(MCF_EDMA_INT1_CHANNEL_BASE + \
					 MCF_EDMA_INT1_CONTROLLER_BASE)
#define MCF_EDMA_INT1_NUM		(40)
#define MCF_EDMA_INT1_END		(MCF_EDMA_INT0_END + MCF_EDMA_INT1_NUM)

#define MCF_EDMA_INT2_CHANNEL_BASE	(0)
#define MCF_EDMA_INT2_CONTROLLER_BASE	(192)
#define MCF_EDMA_INT2_BASE		(MCF_EDMA_INT2_CHANNEL_BASE + \
					 MCF_EDMA_INT2_CONTROLLER_BASE)
#define MCF_EDMA_INT2_NUM		(8)
#define MCF_EDMA_INT2_END		(MCF_EDMA_INT1_END + MCF_EDMA_INT2_NUM)

#endif

#if defined(CONFIG_M5445X)
#define MCF_EDMA_CHANNELS		(16)	/* 0-15 */
#elif defined(CONFIG_M5441X)
#define MCF_EDMA_CHANNELS		(64)	/* 0-63 */
#elif defined(CONFIG_ARCH_MVF)
/* two DMA engine with each 32 channels */
#define MCF_EDMA_CHANNELS		(64)
/* two DMA engine with each 64 sources */
#define MCF_EDMA_SOURCES		(128)
#endif


#define MCF_EDMA_CHANNEL_ANY		(0xFF)
#define MCF_EDMA_INT_ERR		(16)	/* edma error interrupt */

#define MCF_EDMA_TCD_PER_CHAN		256

#if defined(CONFIG_M54455) || defined(CONFIG_ARCH_MVF)
/* eDMA engine TCD memory description */

struct TCD {
	u32	saddr;
	u16	attr;
	u16	soff;
	u32	nbytes;
	u32	slast;
	u32	daddr;
	u16	citer;
	u16	doff;
	u32	dlast_sga;
	u16	biter;
	u16	csr;
} __packed;

struct fsl_edma_requestbuf {
	dma_addr_t	saddr;
	dma_addr_t	daddr;
	u32	soff;
	u32	doff;
	u32	attr;
	u32	 minor_loop;
	u32	len;
};

/*
 * config the eDMA to use the TCD sg feature
 *
 * @channel: which channel. in fact this function is designed to satisfy
 * the ATA driver TCD SG need, i.e. by now it is a special
 * func, because it need prev alloc channel TCD physical memory
 * first, we add the ATA's in the eDMA init only
 * @buf: buffer array to fill the TCDs
 * @nents: the size of the buf
 */
void mcf_edma_sg_config(int channel, struct fsl_edma_requestbuf *buf,
			int nents);

/*
 * The zero-copy version of mcf_edma_sg_config()
 */
void mcf_edma_sglist_config(int channel, struct scatterlist *sgl, int n_elem,
			int dma_dir, u32 addr, u32 attr,
			u32 soff, u32 doff, u32 nbytes);
#endif

/* Setup transfer control descriptor (TCD)
 *   channel - descriptor number
 *   source  - source address
 *   dest    - destination address
 *   attr    - attributes
 *   soff    - source offset
 *   nbytes  - number of bytes to be transfered in minor loop
 *   slast   - last source address adjustment
 *   citer   - major loop count
 *   biter   - begining minor loop count
 *   doff    - destination offset
 *   dlast_sga - last destination address adjustment
 *   major_int - generate interrupt after each major loop
 *   disable_req - disable DMA request after major loop
 *   enable_sg - enable scatter/gather processing
 */
void mcf_edma_set_tcd_params(int channel, u32 source, u32 dest,
			     u32 attr, u32 soff, u32 nbytes, u32 slast,
			     u32 citer, u32 biter, u32 doff, u32 dlast_sga,
			     int major_int, int disable_req, int enable_sg);

/* Setup transfer control descriptor (TCD) and enable halfway irq
 *   channel - descriptor number
 *   source  - source address
 *   dest    - destination address
 *   attr    - attributes
 *   soff    - source offset
 *   nbytes  - number of bytes to be transfered in minor loop
 *   slast   - last source address adjustment
 *   biter   - major loop count
 *   doff    - destination offset
 *   dlast_sga - last destination address adjustment
 *   disable_req - disable DMA request after major loop
 */
void mcf_edma_set_tcd_params_halfirq(int channel, u32 source, u32 dest,
				     u32 attr, u32 soff, u32 nbytes, u32 slast,
				     u32 biter, u32 doff, u32 dlast_sga,
				     int disable_req, int enable_sg);

/* check if dma is done
 *   channel - descriptor number
 *   return 1 if done
 */
int mcf_edma_check_done(int channel);

#if defined(CONFIG_COLDFIRE)
/* Starts eDMA transfer on specified channel
 *   channel - eDMA TCD number
 */
static inline void
mcf_edma_start_transfer(int channel)
{
	MCF_EDMA_SERQ = channel;
	MCF_EDMA_SSRT = channel;
}

/* Restart eDMA transfer from halfirq
 *   channel - eDMA TCD number
 */
static inline void
mcf_edma_confirm_halfirq(int channel)
{
	/*MCF_EDMA_TCD_CSR(channel) = 7;*/
	MCF_EDMA_SSRT = channel;
}

/* Starts eDMA transfer on specified channel based on peripheral request
 *   channel - eDMA TCD number
 */
static inline void  mcf_edma_enable_transfer(int channel)
{
	MCF_EDMA_SERQ = channel;
}

/* Stops eDMA transfer
 *   channel - eDMA TCD number
 */
static inline void
mcf_edma_stop_transfer(int channel)
{
	MCF_EDMA_CINT = channel;
	MCF_EDMA_CERQ = channel;
}

/* Confirm that interrupt has been handled
 *   channel - eDMA TCD number
 */
static inline void
mcf_edma_confirm_interrupt_handled(int channel)
{
	MCF_EDMA_CINT = channel;
}
#elif defined(CONFIG_ARCH_MVF)
void mcf_edma_start_transfer(int channel);
void mcf_edma_confirm_halfirq(int channel);
void mcf_edma_enable_transfer(int channel);
void mcf_edma_stop_transfer(int channel);
void mcf_edma_confirm_interrupt_handled(int channel);
#endif
/**
 * mcf_edma_request_channel - Request an eDMA channel
 * @channel: channel number. In case it is equal to EDMA_CHANNEL_ANY
 *		it will be allocated a first free eDMA channel.
 * @handler: dma handler
 * @error_handler: dma error handler
 * @irq_level: irq level for the dma handler
 * @arg: argument to pass back
 * @lock: optional spinlock to hold over interrupt
 * @device_id: device id
 *
 * Returns allocatedd channel number if success or
 * a negative value if failure.
 */
int mcf_edma_request_channel(int channel,
			     irqreturn_t(*handler) (int, void *),
			     void (*error_handler) (int, void *),
			     u8 irq_level,
			     void *arg,
			     spinlock_t *lock, const char *device_id);

/**
 * Update the channel callback/arg
 * @channel: channel number
 * @handler: dma handler
 * @error_handler: dma error handler
 * @arg: argument to pass back
 *
 * Returns 0 if success or a negative value if failure
 */
int mcf_edma_set_callback(int channel,
			  irqreturn_t(*handler) (int, void *),
			  void (*error_handler) (int, void *), void *arg);

/**
 * Free the edma channel
 * @channel: channel number
 * @arg: argument created with
 *
 * Returns 0 if success or a negative value if failure
 */
int mcf_edma_free_channel(int channel, void *arg);

void mcf_edma_dump_channel(int channel);

#endif				/* _MCF_EDMA_H */
