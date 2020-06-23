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

#ifndef VIDEOSOURCE_IF_H
#define VIDEOSOURCE_IF_H

#include "videosource_types.h"

extern int videosource_if_init_mipi_csi2_interface(videosource_t * vdev, videosource_format_t * format, unsigned int onOff);
extern int videosource_if_set_mipi_csi2_interrupt(videosource_t * vdev, videosource_format_t * format, unsigned int onOff);
extern int videosource_if_probe(videosource_t * vdev);
extern int videosource_if_remove(videosource_t * vdev);

#endif//VIDEOSOURCE_IF_H

