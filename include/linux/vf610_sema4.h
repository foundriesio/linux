/*
 * Copyright (C) 2016 Toradex AG.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_VF610_SEMA4_H__
#define __LINUX_VF610_SEMA4_H__

#define SEMA4_NUM_DEVICES	1
#define SEMA4_NUM_GATES		16

#define SEMA4_UNLOCK	0x00
#define SEMA4_A5_LOCK	0x01
#define SEMA4_GATE_MASK	0x03

#define CORE_MUTEX_VALID	(('c'<<24)|('m'<<24)|('t'<<24)|'x')

/*
 * The enumerates
 */
enum {
	/* sema4 registers offset */
	SEMA4_CP0INE	= 0x42,
	SEMA4_CP1INE	= 0x4A,
	SEMA4_CP0NTF	= 0x82,
	SEMA4_CP1NTF	= 0x8A,
};

static const unsigned int idx_sema4[SEMA4_NUM_GATES] = {
	1 << 15, 1 << 14, 1 << 13, 1 << 12,
	1 << 11, 1 << 10, 1 << 9, 1 << 8,
	1 << 7, 1 << 6, 1 << 5, 1 << 4,
	1 << 3, 1 << 2, 1 << 1, 1 << 0,
};

struct vf610_sema4_mutex {
	u32			valid;
	u32			gate_num;
	unsigned char		gate_val;
	wait_queue_head_t       wait_q;
};

struct vf610_sema4_mutex_device {
	struct device		*dev;
	u16			cpntf_val;
	u16			cpine_val;
	void __iomem		*ioaddr;	/* Mapped address */
	spinlock_t		lock;		/* Mutex */
	int			irq;

	u16			alloced;
	struct vf610_sema4_mutex	*mutex_ptr[SEMA4_NUM_GATES];
};

struct vf610_sema4_mutex *
	vf610_sema4_mutex_create(u32 dev_num, u32 mutex_num);
#ifdef CONFIG_VF610_SEMA4
int vf610_sema4_mutex_destroy(struct vf610_sema4_mutex *mutex_ptr);
int vf610_sema4_mutex_trylock(struct vf610_sema4_mutex *mutex_ptr);
int vf610_sema4_mutex_lock(struct vf610_sema4_mutex *mutex_ptr);
int vf610_sema4_mutex_unlock(struct vf610_sema4_mutex *mutex_ptr);
#else
static inline int vf610_sema4_mutex_destroy(struct vf610_sema4_mutex *mutex_ptr)
{
	return 0;
}
static inline int vf610_sema4_mutex_trylock(struct vf610_sema4_mutex *mutex_ptr)
{
	return 0;
}
static inline int vf610_sema4_mutex_lock(struct vf610_sema4_mutex *mutex_ptr)
{
	return 0;
}
static inline int vf610_sema4_mutex_unlock(struct vf610_sema4_mutex *mutex_ptr)
{
	return 0;
}
#endif
#endif /* __LINUX_VF610_SEMA4_H__ */
