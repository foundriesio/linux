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

#ifndef TCC_SNOR_UPDATER_CMD_H
#define TCC_SNOR_UPDATER_CMD_H

#define CMD_TYPE_MASK 	0xFFFF0000

#define SNOR_UPDATE_ACK			1
#define SNOR_UPDATE_NACK		2

typedef enum{
	NACK_FIRMWARE_LOAD_FAIL = 1,
	NACK_SNOR_INIT_FAIL,
	NACK_SNOR_ACCESS_FAIL,
	NACK_CRC_ERROR,
	NACK_SNOR_ERASE_FAIL,
	NACK_WRITE_FAIL,
	NACK_FW_COUNT_ERROR,
	NACK_MAX
}updateCmdNACKType;

void snor_updater_event_create(struct snor_updater_device *updater_dev);
void snor_updater_event_delete(struct snor_updater_device *updater_dev);
int snor_updater_wait_event_timeout(struct snor_updater_device *updater_dev,unsigned int reqeustCMD, struct tcc_mbox_data *receiveMsg, unsigned int timeOut);
void snor_updater_wake_up(struct snor_updater_device *updater_dev, struct tcc_mbox_data *receiveMsg);
int send_update_start(struct snor_updater_device *updater_dev);
int send_update_done(struct snor_updater_device *updater_dev);
int send_fw_start(struct snor_updater_device *updater_dev, unsigned int fwStartAddress, unsigned int fwPartitionSize, unsigned int fwDataSize);
int send_fw_send(struct snor_updater_device *updater_dev,
									unsigned int fwStartAddress,
									unsigned int currentCount,
									unsigned int totalCount,
									char *fwData, 
									unsigned int fwDataSize, 
									unsigned int fwdataCRC);
int send_fw_done(struct snor_updater_device *updater_dev);

#endif
