#ifndef _ASM_S390_DMA_MAPPING_H
#define _ASM_S390_DMA_MAPPING_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/dma-debug.h>
#include <linux/io.h>

#define DMA_ERROR_CODE		(~(dma_addr_t) 0x0)

extern const struct dma_map_ops s390_pci_dma_ops;

static inline const struct dma_map_ops *get_arch_dma_ops(struct bus_type *bus)
{
	return &dma_noop_ops;
}

static inline void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
				  enum dma_data_direction direction)
{
}

#endif /* _ASM_S390_DMA_MAPPING_H */
