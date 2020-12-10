/*
 * Copyright (C) Telechips Inc.
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
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/irq.h>

#define fb_width(fb) ((fb)->var.xres)
#define fb_height(fb) ((fb)->var.yres)
#define fb_size(fb) ((fb)->var.xres * (fb)->var.yres * 2)

static void memset16(void *_ptr, unsigned short val, unsigned int count)
{
	unsigned short *ptr = _ptr;

	count >>= 1;
	while (count--)
		*ptr++ = val;
}
#if 0

unsigned short *data; // file read data memory pointer
unsigned int count;

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename)
{
	struct fb_info *info;
	int fd, err = 0;
	unsigned int count, max;
	unsigned short *data, *bits, *ptr;

	pr_info("~~  %s:  %s\n", __func__, filename);
	info = registered_fb[0];
	if (!info) {
		pr_info("%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		pr_info("%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
	count = (unsigned int)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		pr_info("%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if ((unsigned int)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	bits = (unsigned short *)(info->screen_base);
	while (count > 3) {
		unsigned int n = ptr[0];

		if (n > max)
			break;
		memset16(bits, ptr[1], n << 1);
		bits += n;
		max -= n;
		ptr += 2;
		count -= 4;
	}

err_logo_free_data:
	kfree(data);

err_logo_close_file:
	sys_close(fd);

	pr_info("~~  %s:  %s  err:%d end\n",
		__func__, filename, err);

	return err;
}
EXPORT_SYMBOL(load_565rle_image);
#endif //

#if 1
unsigned short *data; // file read data memory pointer
unsigned int count;

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename)
{
	int fd, err = 0;

	pr_info("[INF][LOGO] %s:  %s\n", __func__, filename);

	data = NULL;
	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		pr_err("[ERR][LOGO] %s: Can not open %s\n", __func__, filename);
		return -ENOENT;
	}
	count = (unsigned int)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}

	sys_lseek(fd, (off_t)0, 0);
	data = (unsigned short *)kmalloc(count, GFP_KERNEL);
	if (!data) {
		pr_err("[ERR][LOGO] %s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}

	if ((unsigned int)sys_read(fd, (char *)data, count) != count)
		err = -EIO;

err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_565rle_image);
#endif

int load_image_display(void)
{
	struct fb_info *info;
	unsigned int max;
	unsigned short *bits, *ptr;

	pr_info("%s:\n", __func__);

	info = registered_fb[0];
	if (!info) {
		pr_err("[ERR][LOGO] %s: Can not access framebuffer\n",
		       __func__);
		return -ENODEV;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	// bits = (unsigned short *)(info->screen_base) + (info->var.xres
	// *info->var.yoffset * (info->var.bits_per_pixel/8));
	bits = (unsigned short *)(info->screen_base);
	while (count > 3) {
		unsigned int n = ptr[0];

		if (n > max)
			break;

		memset16(bits, ptr[1], n << 1);
		bits += n;
		max -= n;
		ptr += 2;
		count -= 4;
	}
	return 0;
}
EXPORT_SYMBOL(load_image_display);

int load_image_free(void)
{
	if (data != NULL) {
		kfree(data);
		return 0;
	}
	return (-ENOMEM);
}
EXPORT_SYMBOL(load_image_free);
