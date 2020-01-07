/****************************************************************************
 *
 * Copyright (C) 2013 Telechips Inc.
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

#ifndef __TCCVIN_SWITCHMANAGER_H__
#define __TCCVIN_SWITCHMANAGER_H__ 

#include "tccvin_dev.h"

extern int tccvin_switchmanager_show_info(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_start_preview(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_stop_preview(tccvin_dev_t * vdev);
extern int tcc_cam_swtichmanager_start_monitor(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_stop_monitor(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_handover_handler(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_probe(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_check_preview_v4l2(tccvin_dev_t * vdev);
extern int tccvin_switchmanager_restore_preview_v4l2(tccvin_dev_t * vdev);

#endif//__TCCVIN_SWITCHMANAGER_H__

