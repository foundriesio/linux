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
#ifndef	_TCC_UTIL_H__
#define	_TCC_UTIL_H__

struct f_size {
	long blocks;
	long avail;
};

struct MOUNTP {
	FILE *fp;			// ���� ��Ʈ�� ������
	char devname[80];	// ��ġ �̸�
	char mountdir[80];	// ����Ʈ ���丮 �̸�
	char fstype[12];	// ���� �ý��� Ÿ��
	struct f_size size;	// ���� �ý����� ��ũ��/�����
};

struct MOUNTP *dfopen();
struct MOUNTP *dfget(struct MOUNTP *MP, int deviceType);
int dfclose(struct MOUNTP *MP);

#endif //_TCC_UTIL_H__
