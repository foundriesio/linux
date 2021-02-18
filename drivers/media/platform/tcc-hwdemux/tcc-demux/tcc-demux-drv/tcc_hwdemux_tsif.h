/*
 * Copyright (C) Telechips, Inc.
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

#ifndef	_TCC_HWDEMUX_TSIF_H_
#define	_TCC_HWDEMUX_TSIF_H_

int tcc_hwdmx_tsif_probe(struct platform_device *pdev);
int tcc_hwdmx_tsif_remove(struct platform_device *pdev);
long tcc_hwdmx_tsif_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg);
ssize_t tcc_hwdmx_tsif_read(struct file *filp, char *buf, size_t len,
			    loff_t *ppos);
ssize_t tcc_hwdmx_tsif_write(struct file *filp, const char *buf, size_t len,
			     loff_t *ppos);
unsigned int tcc_hwdmx_tsif_poll(struct file *filp,
				 struct poll_table_struct *wait);
int tcc_hwdmx_tsif_open(struct inode *inode, struct file *filp);
int tcc_hwdmx_tsif_release(struct inode *inode, struct file *filp);
int tcc_hwdmx_tsif_ioctl_ex(struct file *filp, unsigned int cmd,
			    unsigned long arg);
int tcc_hwdmx_tsif_set_external_tsdemux(struct file *filp,
					int (*decoder)(char *p1, int p1_size,
							char *p2, int p2_size,
							int devid),
					int max_dec_packet, int devid);
int tcc_hwdmx_tsif_buffer_flush(struct file *filp, unsigned long x, int size);

#endif //_TCC_HWDEMUX_TSIF_H_
