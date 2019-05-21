// SPDX-License-Identifier: GPL-2.0
/*
 * Extractor for cpio archive based on init/initramfs.c
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

static phys_addr_t image_addr;
static loff_t image_size;

static ssize_t xwrite(int fd, const char *p, size_t count)
{
	ssize_t out = 0;

	/* sys_write only can write MAX_RW_COUNT aka 2G-4K bytes at most */
	while (count) {
		ssize_t rv = sys_write(fd, p, count);

		if (rv < 0) {
			if (rv == -EINTR || rv == -EAGAIN)
				continue;
			return out ? out : rv;
		} else if (rv == 0)
			break;

		p += rv;
		out += rv;
		count -= rv;
	}

	return out;
}

static char *message;
static void error(char *x)
{
	if (!message)
		message = x;
}

/* link hash */

#define N_ALIGN(len) ((((len) + 1) & ~3) + 2)

static struct hash {
	int ino, minor, major;
	umode_t mode;
	struct hash *next;
	char name[N_ALIGN(PATH_MAX)];
} *head[32];

static inline int hash(int major, int minor, int ino)
{
	unsigned long tmp = ino + minor + (major << 3);
	tmp += tmp >> 5;
	return tmp & 31;
}

static char *find_link(int major, int minor, int ino,
			      umode_t mode, char *name)
{
	struct hash **p, *q;
	for (p = head + hash(major, minor, ino); *p; p = &(*p)->next) {
		if ((*p)->ino != ino)
			continue;
		if ((*p)->minor != minor)
			continue;
		if ((*p)->major != major)
			continue;
		if (((*p)->mode ^ mode) & S_IFMT)
			continue;
		return (*p)->name;
	}
	q = kmalloc(sizeof(struct hash), GFP_KERNEL);
	if (!q)
		panic("can't allocate link hash entry");
	q->major = major;
	q->minor = minor;
	q->ino = ino;
	q->mode = mode;
	strcpy(q->name, name);
	q->next = NULL;
	*p = q;
	return NULL;
}

static void free_hash(void)
{
	struct hash **p, *q;
	for (p = head; p < head + 32; p++) {
		while (*p) {
			q = *p;
			*p = q->next;
			kfree(q);
		}
	}
}

static long do_utime(char *filename, time_t mtime)
{
	struct timespec64 t[2];

	t[0].tv_sec = mtime;
	t[0].tv_nsec = 0;
	t[1].tv_sec = mtime;
	t[1].tv_nsec = 0;

	return do_utimes(AT_FDCWD, filename, t, AT_SYMLINK_NOFOLLOW);
}

static LIST_HEAD(dir_list);
struct dir_entry {
	struct list_head list;
	char *name;
	time_t mtime;
};

static void dir_add(const char *name, time_t mtime)
{
	struct dir_entry *de = kmalloc(sizeof(struct dir_entry), GFP_KERNEL);
	if (!de)
		panic("can't allocate dir_entry buffer");
	INIT_LIST_HEAD(&de->list);
	de->name = kstrdup(name, GFP_KERNEL);
	de->mtime = mtime;
	list_add(&de->list, &dir_list);
}

static void dir_utime(void)
{
	struct dir_entry *de, *tmp;
	list_for_each_entry_safe(de, tmp, &dir_list, list) {
		list_del(&de->list);
		do_utime(de->name, de->mtime);
		kfree(de->name);
		kfree(de);
	}
}

static time_t mtime;

/* cpio header parsing */

static unsigned long ino, major, minor, nlink;
static umode_t mode;
static unsigned long body_len, name_len;
static uid_t uid;
static gid_t gid;
static unsigned rdev;

static void parse_header(char *s)
{
	unsigned long parsed[12];
	char buf[9];
	int i;

	buf[8] = '\0';
	for (i = 0, s += 6; i < 12; i++, s += 8) {
		memcpy(buf, s, 8);
		parsed[i] = simple_strtoul(buf, NULL, 16);
	}
	ino = parsed[0];
	mode = parsed[1];
	uid = parsed[2];
	gid = parsed[3];
	nlink = parsed[4];
	mtime = parsed[5];
	body_len = parsed[6];
	major = parsed[7];
	minor = parsed[8];
	rdev = new_encode_dev(MKDEV(parsed[9], parsed[10]));
	name_len = parsed[11];
}

/* FSM */

static enum state {
	Start,
	Collect,
	GotHeader,
	SkipIt,
	GotName,
	CopyFile,
	GotSymlink,
	Reset
} state, next_state;

static char *victim;
static unsigned long byte_count;
static loff_t this_header, next_header;

static inline void eat(unsigned n)
{
	victim += n;
	this_header += n;
	byte_count -= n;
}

static char *vcollected;
static char *collected;
static long remains;
static char *collect;

static void read_into(char *buf, unsigned size, enum state next)
{
	if (byte_count >= size) {
		collected = victim;
		eat(size);
		state = next;
	} else {
		collect = collected = buf;
		remains = size;
		next_state = next;
		state = Collect;
	}
}

static char *header_buf, *symlink_buf, *name_buf;

static int do_start(void)
{
	read_into(header_buf, 110, GotHeader);
	return 0;
}

static int do_collect(void)
{
	unsigned long n = remains;
	if (byte_count < n)
		n = byte_count;
	memcpy(collect, victim, n);
	eat(n);
	collect += n;
	if ((remains -= n) != 0)
		return 1;
	state = next_state;
	return 0;
}

static int do_header(void)
{
	if (memcmp(collected, "070707", 6)==0) {
		error("incorrect cpio method used: use -H newc option");
		return 1;
	}
	if (memcmp(collected, "070701", 6)) {
		error("no cpio magic");
		return 1;
	}
	parse_header(collected);
	next_header = this_header + N_ALIGN(name_len) + body_len;
	next_header = (next_header + 3) & ~3;
	state = SkipIt;
	if (name_len <= 0 || name_len > PATH_MAX)
		return 0;
	if (S_ISLNK(mode)) {
		if (body_len > PATH_MAX)
			return 0;
		collect = collected = symlink_buf;
		remains = N_ALIGN(name_len) + body_len;
		next_state = GotSymlink;
		state = Collect;
		return 0;
	}
	if (S_ISREG(mode) || !body_len)
		read_into(name_buf, N_ALIGN(name_len), GotName);
	return 0;
}

static int do_skip(void)
{
	if (this_header + byte_count < next_header) {
		eat(byte_count);
		return 1;
	} else {
		eat(next_header - this_header);
		state = next_state;
		return 0;
	}
}

static int do_reset(void)
{
	while (byte_count && *victim == '\0')
		eat(1);
	if (byte_count && (this_header & 3))
		error("broken padding");
	return 1;
}

static int maybe_link(void)
{
	if (nlink >= 2) {
		char *old = find_link(major, minor, ino, mode, collected);
		if (old)
			return (sys_link(old, collected) < 0) ? -1 : 1;
	}
	return 0;
}

static void clean_path(char *path, umode_t fmode)
{
	struct kstat st;

	if (!vfs_lstat(path, &st) && (st.mode ^ fmode) & S_IFMT) {
		if (S_ISDIR(st.mode))
			sys_rmdir(path);
		else
			sys_unlink(path);
	}
}

static inline int build_open_flags(int flags, umode_t mode, struct open_flags *op)
{
	int lookup_flags = 0;
	int acc_mode = ACC_MODE(flags);

	/*
	 * Clear out all open flags we don't know about so that we don't report
	 * them in fcntl(F_GETFD) or similar interfaces.
	 */
	flags &= VALID_OPEN_FLAGS;

	if (flags & (O_CREAT | __O_TMPFILE))
		op->mode = (mode & S_IALLUGO) | S_IFREG;
	else
		op->mode = 0;

	/* Must never be set by userspace */
	flags &= ~FMODE_NONOTIFY & ~O_CLOEXEC;

	/*
	 * O_SYNC is implemented as __O_SYNC|O_DSYNC.  As many places only
	 * check for O_DSYNC if the need any syncing at all we enforce it's
	 * always set instead of having to deal with possibly weird behaviour
	 * for malicious applications setting only __O_SYNC.
	 */
	if (flags & __O_SYNC)
		flags |= O_DSYNC;

	if (flags & __O_TMPFILE) {
		if ((flags & O_TMPFILE_MASK) != O_TMPFILE)
			return -EINVAL;
		if (!(acc_mode & MAY_WRITE))
			return -EINVAL;
	} else if (flags & O_PATH) {
		/*
		 * If we have O_PATH in the open flag. Then we
		 * cannot have anything other than the below set of flags
		 */
		flags &= O_DIRECTORY | O_NOFOLLOW | O_PATH;
		acc_mode = 0;
	}

	op->open_flag = flags;

	/* O_TRUNC implies we need access checks for write permissions */
	if (flags & O_TRUNC)
		acc_mode |= MAY_WRITE;

	/* Allow the LSM permission hook to distinguish append
	   access from general write access. */
	if (flags & O_APPEND)
		acc_mode |= MAY_APPEND;

	op->acc_mode = acc_mode;

	op->intent = flags & O_PATH ? 0 : LOOKUP_OPEN;

	if (flags & O_CREAT) {
		op->intent |= LOOKUP_CREATE;
		if (flags & O_EXCL)
			op->intent |= LOOKUP_EXCL;
	}

	if (flags & O_DIRECTORY)
		lookup_flags |= LOOKUP_DIRECTORY;
	if (!(flags & O_NOFOLLOW))
		lookup_flags |= LOOKUP_FOLLOW;
	op->lookup_flags = lookup_flags;
	return 0;
}

static int wfd;

static long xopen(const char *filename, int flags, umode_t mode)
{
#if 1
	int fd;

	fd = get_unused_fd_flags(flags);
	if (fd >= 0) {
		struct file *f = filp_open(filename, flags, mode);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else
			fd_install(fd, f);
	}
	return fd;
#else
	struct open_flags op;
	int fd = build_open_flags(flags, mode, &op);
	struct filename *tmp;

#if 0
	if (fd)
		return fd;

	tmp = __getname();
	strncpy(tmp, filename, 
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);
#endif

	fd = get_unused_fd_flags(flags);
	if (fd >= 0) {
		struct file *f = do_filp_open(AT_FDCWD, filename, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
#endif
}

static int do_name(void)
{
	state = SkipIt;
	next_state = Reset;
	if (strcmp(collected, "TRAILER!!!") == 0) {
		free_hash();
		return 0;
	}
	clean_path(collected, mode);
	if (S_ISREG(mode)) {
		int ml = maybe_link();
		if (ml >= 0) {
			int openflags = O_WRONLY|O_CREAT;
			if (ml != 1)
				openflags |= O_TRUNC;
			wfd = xopen(collected, openflags, mode);
			if (wfd >= 0) {
				sys_fchown(wfd, uid, gid);
				sys_fchmod(wfd, mode);
				if (body_len)
					sys_ftruncate(wfd, body_len);
				vcollected = kstrdup(collected, GFP_KERNEL);
				state = CopyFile;
			}
		}
	} else if (S_ISDIR(mode)) {
		sys_mkdir(collected, mode);
		sys_chown(collected, uid, gid);
		sys_chmod(collected, mode);
		dir_add(collected, mtime);
	} else if (S_ISBLK(mode) || S_ISCHR(mode) ||
		   S_ISFIFO(mode) || S_ISSOCK(mode)) {
		if (maybe_link() == 0) {
			sys_mknod(collected, mode, rdev);
			sys_chown(collected, uid, gid);
			sys_chmod(collected, mode);
			do_utime(collected, mtime);
		}
	}
	return 0;
}

static int do_copy(void)
{
	if (byte_count >= body_len) {
		if (xwrite(wfd, victim, body_len) != body_len)
			error("write error");
		sys_close(wfd);
		do_utime(vcollected, mtime);
		kfree(vcollected);
		eat(body_len);
		state = SkipIt;
		return 0;
	} else {
		if (xwrite(wfd, victim, byte_count) != byte_count)
			error("write error");
		body_len -= byte_count;
		eat(byte_count);
		return 1;
	}
}

static int do_symlink(void)
{
	collected[N_ALIGN(name_len) + body_len] = '\0';
	clean_path(collected, 0);
	sys_symlink(collected + N_ALIGN(name_len), collected);
	sys_lchown(collected, uid, gid);
	do_utime(collected, mtime);
	state = SkipIt;
	next_state = Reset;
	return 0;
}

static int (*actions[])(void) = {
	[Start]		= do_start,
	[Collect]	= do_collect,
	[GotHeader]	= do_header,
	[SkipIt]	= do_skip,
	[GotName]	= do_name,
	[CopyFile]	= do_copy,
	[GotSymlink]	= do_symlink,
	[Reset]		= do_reset,
};

static long write_buffer(char *buf, unsigned long len)
{
	byte_count = len;
	victim = buf;

	while (!actions[state]())
		;
	return len - byte_count;
}

#if 0
static long flush_buffer(void *bufv, unsigned long len)
{
	char *buf = (char *) bufv;
	long written;
	long origLen = len;
	if (message)
		return -1;
	while ((written = write_buffer(buf, len)) < len && !message) {
		char c = buf[written];
		if (c == '0') {
			buf += written;
			len -= written;
			state = Start;
		} else if (c == 0) {
			buf += written;
			len -= written;
			state = Reset;
		} else
			error("junk in compressed archive");
	}
	return origLen;
}

static unsigned long my_inptr; /* index of next byte to be processed in inbuf */

#include <linux/decompress/generic.h>
#endif

static char * unpack_to_rootfs(char *buf, unsigned long len)
{
	long written;
#ifdef SUPPORT_DECOMPRESSOR
	decompress_fn decompress;
	const char *compress_name;
	static char msg_buf[64];
#endif

	header_buf = kmalloc(110, GFP_KERNEL);
	symlink_buf = kmalloc(PATH_MAX + N_ALIGN(PATH_MAX) + 1, GFP_KERNEL);
	name_buf = kmalloc(N_ALIGN(PATH_MAX), GFP_KERNEL);

	if (!header_buf || !symlink_buf || !name_buf)
		panic("can't allocate buffers");

	state = Start;
	this_header = 0;
	message = NULL;
	while (!message && len) {
#ifdef SUPPORT_DECOMPRESSOR
		loff_t saved_offset = this_header;
#endif
		if (*buf == '0' && !(this_header & 3)) {
			state = Start;
			written = write_buffer(buf, len);
			buf += written;
			len -= written;
			continue;
		}
		if (!*buf) {
			buf++;
			len--;
			this_header++;
			continue;
		}
#ifdef SUPPORT_COMPRESSOR
		this_header = 0;
		decompress = decompress_method(buf, len, &compress_name);
		pr_debug("Detected %s compressed data\n", compress_name);
		if (decompress) {
			int res = decompress(buf, len, NULL, flush_buffer, NULL,
				   &my_inptr, error);
			if (res)
				error("decompressor failed");
		} else if (compress_name) {
			if (!message) {
				snprintf(msg_buf, sizeof msg_buf,
					 "compression method %s not configured",
					 compress_name);
				message = msg_buf;
			}
		} else
			error("junk in compressed archive");
		if (state != Reset)
			error("junk in compressed archive");
		this_header = saved_offset + my_inptr;
		buf += my_inptr;
		len -= my_inptr;
#endif
	}
	dir_utime();
	kfree(name_buf);
	kfree(symlink_buf);
	kfree(header_buf);
	return message;
}

static ssize_t addr_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	sscanf(buf, "%lli", &image_addr);
	return count;
}

static ssize_t size_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t count)
{
	sscanf(buf, "%lli", &image_size);
	return count;
}

static ssize_t extract_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	mm_segment_t old_fs;
	struct path old_pwd;
	char *image_buf;
	char *path;
	char *err;

	if (!image_addr || !image_size)
		return -EINVAL;

	if (pfn_valid(__phys_to_pfn(image_addr)))
		image_buf = phys_to_virt(image_addr);
	else
		image_buf = ioremap(image_addr, image_size);

	if (!image_buf) {
		pr_err("Failed to map image address\n");
		return -ENODEV;
	}

	path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!path)
		return -ENOMEM;

	get_fs_pwd(current->fs, &old_pwd);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	sscanf(buf, "%s", path);
	sys_chdir(path);
	kfree(path);

	err = unpack_to_rootfs(image_buf, image_size);
	if (err) {
		set_fs(old_fs);
		pr_err("Failed to extract image: %s\n", err);
		return -ENOMEM;
	}
	set_fs(old_fs);
	set_fs_pwd(current->fs, &old_pwd);

	if (!pfn_valid(__phys_to_pfn(image_addr)))
		iounmap(image_buf);

	return count;
}

static struct kobj_attribute addr_attribute =
	__ATTR_WO(addr);

static struct kobj_attribute size_attribute =
	__ATTR_WO(size);

static struct kobj_attribute extract_attribute =
	__ATTR_WO(extract);

static struct attribute *extractor_attrs[] = {
	&addr_attribute.attr,
	&size_attribute.attr,
	&extract_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = extractor_attrs,
};

static struct kobject *extractor_kobj;

static int init_cpio_extractor(void)
{
	int ret;

	extractor_kobj = kobject_create_and_add("extractor", fs_kobj);
	if (!extractor_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(extractor_kobj, &attr_group);
	if (ret)
		kobject_put(extractor_kobj);

	return 0;
}
fs_initcall(init_cpio_extractor);
