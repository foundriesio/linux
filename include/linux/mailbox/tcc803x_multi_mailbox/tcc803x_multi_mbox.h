/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC803X_MULTI_MBOX_H
#define TCC803X_MULTI_MBOX_H


#define MBOX_CMD_FIFO_SIZE		(7)
#define MBOX_DATA_FIFO_SIZE		(128)

/*
MBOX has 8 cmd fifo but user can only use 7 cmd fifo.
Cmd fifo[0] sis used inside mbox drvier for channel ID.

	MBOX FIFO--User cmd--remark
	cmd fifo[0] :	-----	channel ID  (use mbox driver only)
	cmd fifo[1] :	cmd[0] 	user cmd 1
	cmd fifo[2] :	cmd[1] 	user cmd 2
	cmd fifo[3] :	cmd[2] 	user cmd 3
	cmd fifo[4] :	cmd[3] 	user cmd 4
	cmd fifo[5] :	cmd[4] 	user cmd 5
	cmd fifo[6] :	cmd[5] 	user cmd 6
	cmd fifo[7] :	cmd[6] 	user cmd 7
*/
struct tcc_mbox_data
{
	unsigned int 	cmd[MBOX_CMD_FIFO_SIZE];
	unsigned int 	data[MBOX_DATA_FIFO_SIZE];
	unsigned int 	data_len;
};

#endif
