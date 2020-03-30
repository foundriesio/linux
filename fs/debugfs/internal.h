/*
 *  internal.h - declarations internal to debugfs
 *
 *  Copyright (C) 2016 Nicolai Stange <nicstange@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 *
 */

#ifndef _DEBUGFS_INTERNAL_H_
#define _DEBUGFS_INTERNAL_H_

struct file_operations;

/* declared over in file.c */
extern const struct file_operations debugfs_noop_file_operations;
extern const struct file_operations debugfs_open_proxy_file_operations;
extern const struct file_operations debugfs_full_proxy_file_operations;

struct debugfs_fsdata {
	const struct file_operations *real_fops;
	refcount_t active_users;
	struct completion active_users_drained;
};

/*
 * A dentry's ->d_fsdata either points to the real fops or to a
 * dynamically allocated debugfs_fsdata instance.
 * In order to distinguish between these two cases, a real fops
 * pointer gets its lowest bit set.
 */
#define DEBUGFS_FSDATA_IS_REAL_FOPS_BIT BIT(0)

/*
 * The file removal protection series, upstream commit
 * 7c8d469877b1 ("debugfs: add support for more elaborate ->d_fsdata")
 * in particular, changed the semantics of S_IFREG files' dentry->d_fsdata.
 * Before that change, the original, a pointer to the user-provided
 * file_operations had been stored there and external modules might depend on
 * this. Especially as debugfs_real_fops() used to be defined as an inline
 * function in public include/linux/debugfs.h.
 *
 * Work around this by abusing the associated inode's ->i_link instead, which
 * is unused for regular debugfs files. The dereference + cast dance below
 * is to make the result an lvalue of type void *.
 */
#define __kabi_debugfs_d_fsdata(inode)			\
	(*(void **)&(inode)->i_link)

#define kabi_debugfs_d_fsdata(dentry)			\
	__kabi_debugfs_d_fsdata(d_inode(dentry))

#endif /* _DEBUGFS_INTERNAL_H_ */
