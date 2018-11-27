/****************************************************************************
One line to give the program's name and a brief idea of what it does.
Copyright (C) 2013 Telechips Inc.
 
This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.
 
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
****************************************************************************/

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int r_show(struct seq_file *m, void *v)
{
#if defined(CONFIG_ROTATION_WITH180)
    seq_puts(m, "Block180\t: false\n");
#elif  defined(CONFIG_ROTATION_WITHOUT180)
    seq_puts(m, "Block180\t: true\n");
#else
    #if defined(CONFIG_DRAM_16BIT_USED)
    	    seq_puts(m, "Block180\t: true\n");
    #else
           seq_puts(m, "Block180\t: false\n");
    #endif
#endif
	return 0;
}


static void *r_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *r_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void r_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations rotationinfo_op = {
	.start	= r_start,
	.next	= r_next,
	.stop	= r_stop,
	.show	= r_show
};


static int rotationinfo_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &rotationinfo_op);
}

static const struct file_operations proc_rotationinfo_operations = {
	.open		= rotationinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	       = seq_release,
};

static int __init proc_rotationinfo_init(void)
{
    printk("proc_rotationinfo_init start /n");
	proc_create("rotationinfo", 0, NULL, &proc_rotationinfo_operations);
    printk("proc_rotationinfo_init end /n");
	return 0;
}
module_init(proc_rotationinfo_init);

