// SPDX-License-Identifier: GPL-2.0
/*
 * Loader for cpio archive based on init/initramfs.c
 *
 * Copyright (C) 2019 Telechips Inc.
 */

#ifdef __CHECKER__
#undef __CHECKER__
#warning "Sparse checking disabled for this file"
#endif

#include <linux/init.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/dirent.h>
#include <linux/syscalls.h>
#include <linux/utime.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/namei.h>
#include <linux/initramfs.h>
#include "internal.h"

static bool image_loaded = false;
static void *image_buf;
static loff_t image_size;

static void unload_image(void)
{
	image_loaded = false;
	kfree(image_buf);
}

static ssize_t load_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	struct file *file;
	char *path;
	loff_t pos;

	if (image_loaded)
		unload_image();

	path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!path)
		return -ENOMEM;
	sscanf(buf, "%s", path);

	file = filp_open(path, O_RDONLY, 0);
	kfree(path);
	if (IS_ERR(file))
		return PTR_ERR(file);

	image_size = i_size_read(file_inode(file));
	image_buf = kmalloc(image_size, GFP_KERNEL);

	pos = 0;
	image_size = kernel_read(file, image_buf, image_size, &pos);
	fput(file);

	image_loaded = true;
	return count;
}

static ssize_t unload_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	if (!image_loaded)
		return -EINVAL;

	unload_image();
	return count;
}

static ssize_t addr_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	phys_addr_t image_pa;

	if (!image_loaded)
		return sprintf(buf, "\n");

	image_pa = __virt_to_phys((unsigned long) image_buf);
	return sprintf(buf, "%pa\n", &image_pa);
}

static ssize_t size_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	if (!image_loaded)
		return sprintf(buf, "\n");

	return sprintf(buf, "%lld\n", image_size);
}

static struct kobj_attribute load_attribute =
	__ATTR_WO(load);

static struct kobj_attribute unload_attribute =
	__ATTR_WO(unload);

static struct kobj_attribute addr_attribute =
	__ATTR_RO(addr);

static struct kobj_attribute size_attribute =
	__ATTR_RO(size);

static struct attribute *loader_attrs[] = {
	&load_attribute.attr,
	&unload_attribute.attr,
	&addr_attribute.attr,
	&size_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = loader_attrs,
};

static struct kobject *loader_kobj;

static int init_cpio_loader(void)
{
	int ret;

	loader_kobj = kobject_create_and_add("loader", fs_kobj);
	if (!loader_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(loader_kobj, &attr_group);
	if (ret)
		kobject_put(loader_kobj);

	return 0;
}
fs_initcall(init_cpio_loader);
