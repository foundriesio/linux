/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ISP1763_HCD_H_
#define _ISP1763_HCD_H_

#include <linux/spinlock.h>

struct isp1763_qh;
struct isp1763_qtd;
struct resource;
struct usb_hcd;

/*
 * 20kb divided in chunks; 256 for control; 4096 for data tansfer
 * -  8 blocks @ 256  bytes =  2KB (10%)
 * -  2 blocks @ 1024 bytes =  2KB (10%)
 * -  4 blocks @ 4096 bytes = 16KB (80%)
 */
#define BLOCK_1_NUM 8
#define BLOCK_2_NUM 2
#define BLOCK_3_NUM 4

#define BLOCK_1_SIZE 256
#define BLOCK_2_SIZE 1024
#define BLOCK_3_SIZE 4096
#define BLOCKS (BLOCK_1_NUM + BLOCK_2_NUM + BLOCK_3_NUM)
#define MAX_PAYLOAD_SIZE BLOCK_3_SIZE
#define PAYLOAD_AREA_SIZE 0x5000 /* 20 KB */

#define NUM_OF_PTD 16 /* isp1760 have 32 max PTDs; isp 1763 have 16 */

struct isp1763_slotinfo {
	struct isp1763_qh *qh;
	struct isp1763_qtd *qtd;
	unsigned long timestamp;
};

/* chip memory management */
struct isp1763_memory_chunk {
	unsigned int start;
	unsigned int size;
	unsigned int free;
};

enum isp1763_queue_head_types {
	QH_CONTROL,
	QH_BULK,
	QH_INTERRUPT,
	QH_END
};

struct isp1763_hcd {
	struct usb_hcd		*hcd;

	u32 hcs_params;
	spinlock_t		lock;
	struct isp1763_slotinfo	atl_slots[NUM_OF_PTD];
	u16			atl_done_map;
	struct isp1763_slotinfo	int_slots[NUM_OF_PTD];
	u16			int_done_map;
	struct isp1763_memory_chunk memory_pool[BLOCKS];
	struct list_head	qh_list[QH_END];

	/* periodic schedule support */
#define	DEFAULT_I_TDPS		1024
	unsigned		periodic_size;
	unsigned		i_thresh;
	unsigned long		reset_done;
	unsigned long		next_statechange;
};

int isp1763_hcd_register(struct isp1763_hcd *priv, void __iomem *regs,
			 struct resource *mem, int irq, unsigned long irqflags,
			 struct device *dev);
void isp1763_hcd_unregister(struct isp1763_hcd *priv);

int isp1763_init_kmem_once(void);
void isp1763_deinit_kmem_cache(void);

#endif /* _ISP1763_HCD_H_ */
