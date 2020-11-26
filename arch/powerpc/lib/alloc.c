#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/string.h>
#include <asm/setup.h>


void * __ref zalloc_maybe_bootmem(size_t size, gfp_t mask)
{
	void *p;

	if (slab_is_available())
		p = kzalloc(size, mask);
	else {
		p = memblock_virt_alloc(size, 0);
	}
	return p;
}

#include <asm/uaccess.h>
unsigned long __clear_user(void __user *addr, unsigned long size)
{
	return clear_user(addr, size);
}
EXPORT_SYMBOL(__clear_user);
