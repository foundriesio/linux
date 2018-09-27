#include <linux/export.h>
#include <linux/errno.h>
#include <asm/string.h>

/*
 * Count the digits of @val including a possible sign.
 *
 * (Typed on and submitted from hpa's mobile phone.)
 */
int num_digits(int val)
{
	int m = 10;
	int d = 1;

	if (val < 0) {
		d++;
		val = -val;
	}

	while (val >= m) {
		m *= 10;
		d++;
	}
	return d;
}

#ifndef CONFIG_X86_32
/* Provide kABI wrapper under original name */
__must_check int memcpy_mcsafe_unrolled(void *dst, const void *src, size_t cnt)
{
	return __memcpy_mcsafe(dst, src, cnt) ? -EFAULT : 0;
}
EXPORT_SYMBOL_GPL(memcpy_mcsafe_unrolled);
#endif
