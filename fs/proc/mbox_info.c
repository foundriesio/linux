// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mailbox/tcc_multi_mbox.h>

extern int show_mbox_channel_info(struct seq_file *p, void *v);

static int mbox_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_mbox_channel_info, NULL);
}

static const struct file_operations mbox_proc_fops = {
	.open		= mbox_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_mbox_init(void)
{
	proc_create("mbox_info", 0, NULL, &mbox_proc_fops);
	return 0;
}
fs_initcall(proc_mbox_init);
