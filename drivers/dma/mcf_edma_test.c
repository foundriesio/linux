/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * Author: Andrey Butok
 *
 * Simple test/example module for Coldfire eDMA.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ***************************************************************************
 * Changes:
 *   v0.001    29 February 2008                Andrey Butok
 *             Initial Release
 *
 * NOTE:       This module tests eDMA driver performing
 *             a simple memory to memory transfer with a 32 bit
 *             source and destination transfer size that generates
 *             an interrupt when the transfer is complete.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <asm/cacheflush.h>
#include <mach/mcf_edma.h>
#include <linux/slab.h>

#define MCF_EDMA_TEST_DRIVER_VERSION	"Revision: 0.001"
#define MCF_EDMA_TEST_DRIVER_AUTHOR	\
		"Freescale Semiconductor Inc, Andrey Butok"
#define MCF_EDMA_TEST_DRIVER_DESC	\
		"Simple testing module for Coldfire eDMA "
#define MCF_EDMA_TEST_DRIVER_INFO	\
		MCF_EDMA_TEST_DRIVER_VERSION " " MCF_EDMA_TEST_DRIVER_DESC
#define MCF_EDMA_TEST_DRIVER_LICENSE	"GPL"
#define MCF_EDMA_TEST_DRIVER_NAME	"mcf_edma_test"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* Global variable used to signal main process when interrupt is recognized */
static int mcf_edma_test_interrupt;
int *mcf_edma_test_interrupt_p = &mcf_edma_test_interrupt;

/********************************************************************/
static int
mcf_edma_test_handler(int channel, void *dev_id)
{
	/* Clear interrupt flag */
	mcf_edma_confirm_interrupt_handled(channel);

	/* Set interrupt status flag to TRUE */
	mcf_edma_test_interrupt = TRUE;

	return IRQ_HANDLED;
}

/********************************************************************/

int
mcf_edma_test_block_compare(u8 *block1, u8 *block2, u32 size)
{
	u32 i;

	asm("nop;\n");
	asm("nop;\n");
	for (i = 0; i < (size); i++) {
		if ((*(u8 *) (block1 + i)) != (*(u8 *) (block2 + i))) {
			printk(KERN_INFO "Invalid edma test value: ");
			printk(KERN_INFO "0x%x != 0x%x\n", *(u8 *)(block1 + i),
							   *(u8 *)(block2 + i));
			return FALSE;
		}
	}

	return TRUE;
}

/********************************************************************/

void
mcf_edma_test_run(void)
{
	u16 byte_count;
	u32 i, j;
	u8 *start_address;
	u8 *dest_address;
	u32 test_data;
	int channel;
	u32 allocated_channels = 0;

	printk(KERN_INFO "\n===============================================\n");
	printk(KERN_INFO "\nStarting eDMA transfer tests!\n");

	/* Initialize test variables */
	byte_count = 0x20;
	test_data = 0xA5A5A5A5;

	/* DMA buffer must be from GFP_DMA zone, so it will not be cached */
	start_address = kmalloc(byte_count, GFP_DMA);
	if (start_address == NULL) {
		printk(KERN_INFO MCF_EDMA_TEST_DRIVER_NAME
		       ": failed to allocate DMA[%d] buffer\n", byte_count);
		goto err_out;
	}
	dest_address = kmalloc(byte_count, GFP_DMA);
	if (dest_address == NULL) {
		printk(KERN_INFO MCF_EDMA_TEST_DRIVER_NAME
		       ": failed to allocate DMA[%d] buffer\n", byte_count);
		goto err_free_mem;
	}


	/* Test all automatically allocated DMA channels. The test data is
	 * complemented at the end of the loop, so that the testData value
	 * isn't the same twice in a row */
	for (i = 0; i < 16; i++) {
		/* request eDMA channel */
		channel = mcf_edma_request_channel(
			 /* MCF_EDMA_CHANNEL_ANY*/i,
			  mcf_edma_test_handler,
			  NULL,
			  0x6,
			  NULL,
			  NULL,
			  MCF_EDMA_TEST_DRIVER_NAME);

		if (channel < 0)
			goto test_end;

		allocated_channels |= (1 << channel);

		/* Initialize data for DMA to move */
		for (j = 0; j < byte_count; j = j + 4)
			*((u32 *) (start_address + j)) = test_data;

		flush_cache_all();
		/* Clear interrupt status indicator */
		mcf_edma_test_interrupt = FALSE;

		/* Configure DMA Channel TCD */
		mcf_edma_set_tcd_params(channel, (u32) start_address,
					(u32) dest_address,
					(0 | MCF_EDMA_TCD_ATTR_SSIZE_32BIT |
					 MCF_EDMA_TCD_ATTR_DSIZE_32BIT), 0x04,
					byte_count, 0x0, 1, 1, 0x04, 0x0, 0x1,
					0x0, 0x0);

		/* Start DMA. */
		mcf_edma_start_transfer(channel);

		printk(KERN_INFO "DMA channel %d testing started.\n", channel);

		/* Wait for DMA to complete */
		while (!*mcf_edma_test_interrupt_p)
			;

		/* Test data */
		if (mcf_edma_test_block_compare
		    (start_address, dest_address, byte_count))
			printk(KERN_INFO "Data transfered correctly.\n");
		else
			printk(KERN_INFO "ERROR: Data error!\n");

		printk(KERN_INFO "DMA channel %d testing complete.\n", channel);
		printk(KERN_INFO "-------------------------------\n");

		/* Complement test data so next channel test does not
		 * use same values */
		test_data = ~test_data;
	}

test_end:
	printk(KERN_INFO "All tests have completed\n\n");
	printk(KERN_INFO "Automatically allocated %d eDMA channels:\n", i);
	for (i = 0; i < MCF_EDMA_CHANNELS; i++) {
		if (allocated_channels & (1 << i)) {
			printk(KERN_INFO "%d,\n", i);
			mcf_edma_free_channel(i, NULL);
		}
	}
	printk(KERN_INFO "===============================================\n\n");

	kfree(dest_address);
err_free_mem:
	kfree(start_address);
err_out:
	return;
}

/********************************************************************/

static int __init
mcf_edma_test_init(void)
{
	mcf_edma_test_run();

	/* We intentionaly return -EAGAIN to prevent keeping
	 * the module. It does all its work from init()
	 * and doesn't offer any runtime functionality */
	return -EAGAIN;
}

static void __exit
mcf_edma_test_exit(void)
{
}

module_init(mcf_edma_test_init);
module_exit(mcf_edma_test_exit);

MODULE_DESCRIPTION(MCF_EDMA_TEST_DRIVER_INFO);
MODULE_AUTHOR(MCF_EDMA_TEST_DRIVER_AUTHOR);
MODULE_LICENSE(MCF_EDMA_TEST_DRIVER_LICENSE);
