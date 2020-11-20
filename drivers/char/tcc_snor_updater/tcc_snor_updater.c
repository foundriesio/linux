// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpufreq.h>
#include <linux/err.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/cdev.h>
#include <linux/atomic.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include "tcc_snor_updater_typedef.h"
#include <linux/tcc_snor_updater_dev.h>
#include "tcc_snor_updater_mbox.h"
#include "tcc_snor_updater_cmd.h"
#include "tcc_snor_updater_crc8.h"
#include "tcc_snor_updater.h"

int32_t snor_update_start(struct snor_updater_device *updater_dev)
{
	int32_t ret = SNOR_UPDATER_ERR_ARGUMENT;

	if (updater_dev != NULL) {
		ret = send_update_start(updater_dev);
	}
	return ret;
}

int32_t snor_update_done(struct snor_updater_device *updater_dev)
{
	int32_t ret = SNOR_UPDATER_ERR_ARGUMENT;

	if (updater_dev != NULL) {
		ret = send_update_done(updater_dev);
	}
	return ret;
}

int32_t snor_update_fw(struct snor_updater_device *updater_dev,
		tcc_snor_update_param *fwInfo)
{
	int32_t ret = SNOR_UPDATER_ERR_ARGUMENT;

	if ((updater_dev != NULL) && (fwInfo != NULL))	{
		ret = send_fw_start(updater_dev,
			fwInfo->start_address,
			fwInfo->partition_size,
			fwInfo->image_size);

		if (ret == SNOR_UPDATER_SUCCESS) {
			uint32_t imageOffset;
			uint32_t remainSize;
			uint32_t currentCount;
			uint32_t totalCount;
			uint32_t fwDataSize;
			uint32_t fwdataCRC;

			imageOffset = 0;
			currentCount = 0;
			remainSize = fwInfo->image_size;
			totalCount = fwInfo->image_size/
				(uint32_t)MAX_FW_BUF_SIZE;

			if ((fwInfo->image_size %
				(uint32_t)MAX_FW_BUF_SIZE) != (uint32_t)0) {
				totalCount++;
			}

			for (currentCount = 0;
				currentCount < totalCount;
				currentCount++)	{

				ret = SNOR_UPDATER_SUCCESS;
				if (remainSize >=
					(uint32_t)MAX_FW_BUF_SIZE) {

					fwDataSize =
						(uint32_t)MAX_FW_BUF_SIZE;

					remainSize -=
						(uint32_t)MAX_FW_BUF_SIZE;
				} else {
					fwDataSize = remainSize;
					remainSize = 0;
				}

				fwdataCRC = tcc_snor_calc_crc8(
					&fwInfo->image[imageOffset],
					fwDataSize);

				ret = send_fw_send(
					updater_dev,
					(fwInfo->start_address + imageOffset),
					currentCount + (uint32_t)1,
					totalCount, &fwInfo->image[imageOffset],
					fwDataSize, fwdataCRC);
				if (ret != SNOR_UPDATER_SUCCESS) {
					break;
				}

				imageOffset += fwDataSize;
			}

			if (ret == SNOR_UPDATER_SUCCESS) {
				ret = send_fw_done(updater_dev);
			}
		}

	}
	return ret;
}


