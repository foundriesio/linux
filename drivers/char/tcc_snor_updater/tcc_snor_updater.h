// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef TCC_SNOR_UPDATER_H
#define TCC_SNOR_UPDATER_H

int32_t snor_update_start(struct snor_updater_device *updater_dev);
int32_t snor_update_done(struct snor_updater_device *updater_dev);
int32_t snor_update_fw(struct snor_updater_device *updater_dev,
	tcc_snor_update_param	*fwInfo);

#endif
