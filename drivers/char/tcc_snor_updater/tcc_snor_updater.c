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
#include <asm/uaccess.h>
#include <asm/io.h>
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
#include <asm/atomic.h>

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

#include "tcc_snor_updater_typedef.h"
#include <linux/tcc_snor_updater_dev.h>
#include "tcc_snor_updater_mbox.h"
#include "tcc_snor_updater_cmd.h"
#include "tcc_snor_updater_crc8.h"
#include "tcc_snor_updater.h"

extern int updaterDebugLevel;

#define dprintk(dev, msg...)                                \
{                                                      \
	if (updaterDebugLevel > 1)                                     \
		dev_info(dev, msg);           \
}

#define eprintk(dev, msg...)                                \
{                                                      \
	if (updaterDebugLevel > 0)                                     \
		dev_err(dev, msg);             \
}


int snor_update_start(struct snor_updater_device *updater_dev)
{
	int ret = SNOR_UPDATER_ERR_ARGUMENT;
	if(updater_dev != NULL)
	{
		ret = send_update_start(updater_dev);
	}
	return ret;
}

int snor_update_done(struct snor_updater_device *updater_dev)
{
	int ret = SNOR_UPDATER_ERR_ARGUMENT;
	if(updater_dev != NULL)
	{
		ret = send_update_done(updater_dev);
	}
	return ret;
}

int snor_update_fw(struct snor_updater_device *updater_dev, tcc_snor_update_param	*fwInfo)
{
	int ret = SNOR_UPDATER_ERR_ARGUMENT;
	if(updater_dev != NULL)
	{
		ret = send_fw_start(updater_dev, fwInfo->start_address, fwInfo->partition_size, fwInfo->image_size);
		if(ret == SNOR_UPDATER_SUCCESS)
		{
			unsigned int imageOffset, remainSize;
			unsigned int currentCount, totalCount, fwDataSize;
			unsigned int fwdataCRC;

			imageOffset =0;
			currentCount =0;
			remainSize = fwInfo->image_size;
			totalCount = fwInfo->image_size/MAX_FW_BUF_SIZE;
			if(fwInfo->image_size % MAX_FW_BUF_SIZE != 0)
			{
				totalCount++;
			}

			for(currentCount=0; currentCount < totalCount; currentCount++)
			{
				ret = SNOR_UPDATER_SUCCESS;
				if(remainSize >= MAX_FW_BUF_SIZE)
				{
					fwDataSize = MAX_FW_BUF_SIZE;
					remainSize -= MAX_FW_BUF_SIZE;
				}
				else
				{
					fwDataSize = remainSize;
					remainSize = 0;
				}

				fwdataCRC = tcc_snor_calc_crc8(&fwInfo->image[imageOffset], fwDataSize);
				ret = send_fw_send(updater_dev, (fwInfo->start_address + imageOffset), currentCount+1, totalCount, &fwInfo->image[imageOffset], fwDataSize, fwdataCRC);
				if(ret != SNOR_UPDATER_SUCCESS)
				{
					break;
				}

				imageOffset += fwDataSize;
			}

			if(ret == SNOR_UPDATER_SUCCESS)
			{
				ret = send_fw_done(updater_dev);
			}
		}

	}
	return ret;
}


