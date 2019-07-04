/* SPDX-License-Identifier: GPL-2.0 */
/*
 * SVA library for IOMMU drivers
 */
#ifndef _IOMMU_SVA_H
#define _IOMMU_SVA_H

#include <linux/iommu.h>
#include <linux/kref.h>
#include <linux/mmu_notifier.h>

struct io_mm_ops {
	/* Allocate a PASID context for an mm */
	void *(*alloc)(struct mm_struct *mm);

	/*
	 * Attach a PASID context to a device. Write the entry into the PASID
	 * table.
	 */
	int (*attach)(struct device *dev, int pasid, void *ctx);

	/*
	 * Detach a PASID context from a device. Clear the entry from the PASID
	 * table and invalidate if necessary. Cannot sleep.
	 */
	void (*detach)(struct device *dev, int pasid, void *ctx);

	/* Invalidate a range of addresses. Cannot sleep. */
	void (*invalidate)(struct device *dev, int pasid, void *ctx,
			   unsigned long vaddr, size_t size);

	/* Free a context */
	void (*release)(void *ctx);
};

struct iommu_sva_param {
	u32			min_pasid;
	u32			max_pasid;
	int			nr_bonds;
};

struct iommu_sva *
iommu_sva_bind_generic(struct device *dev, struct mm_struct *mm,
		       const struct io_mm_ops *ops, void *drvdata);
void iommu_sva_unbind_generic(struct iommu_sva *handle);
int iommu_sva_get_pasid_generic(struct iommu_sva *handle);

int iommu_sva_enable(struct device *dev, struct iommu_sva_param *sva_param);
int iommu_sva_disable(struct device *dev);
bool iommu_sva_enabled(struct device *dev);

#endif /* _IOMMU_SVA_H */
