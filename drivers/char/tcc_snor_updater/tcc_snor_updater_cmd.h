// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SNOR_UPDATER_CMD_H
#define TCC_SNOR_UPDATER_CMD_H

#define CMD_TYPE_MASK	(0xFFFF0000U)

#define SNOR_UPDATE_ACK			(1)
#define SNOR_UPDATE_NACK		(2)

enum {
	NACK_FIRMWARE_LOAD_FAIL = 1,
	NACK_SNOR_INIT_FAIL,
	NACK_SNOR_ACCESS_FAIL,
	NACK_CRC_ERROR,
	NACK_SNOR_ERASE_FAIL,
	NACK_WRITE_FAIL,
	NACK_FW_COUNT_ERROR,
	NACK_MAX
};

void snor_updater_event_create
	(struct snor_updater_device *updater_dev);
void snor_updater_event_delete
	(struct snor_updater_device *updater_dev);
int32_t snor_updater_wait_event_timeout
	(struct snor_updater_device *updater_dev,
	uint32_t reqeustCMD,
	struct tcc_mbox_data *receiveMsg,
	uint32_t timeOut);
void snor_updater_wake_up(
	struct snor_updater_device *updater_dev,
	struct tcc_mbox_data *receiveMsg);
int32_t send_update_start(struct snor_updater_device *updater_dev);
int32_t send_update_done(struct snor_updater_device *updater_dev);
int32_t send_fw_start(struct snor_updater_device *updater_dev,
	uint32_t fwStartAddress, uint32_t fwPartitionSize,
	uint32_t fwDataSize);
int32_t send_fw_send(struct snor_updater_device *updater_dev,
	uint32_t fwStartAddress, uint32_t currentCount,
	uint32_t totalCount, char_t *fwData,
	uint32_t fwDataSize, uint32_t fwdataCRC);
int32_t send_fw_done(struct snor_updater_device *updater_dev);

#endif
