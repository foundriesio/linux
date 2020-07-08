/* Copyright (C) 2018 Telechips Inc.
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
 */

#ifndef INCLUDED_TSMP_IOCTL
#define INCLUDED_TSMP_IOCTL

/**
 * @addtogroup tsmp
 * @{
 * @file tsmp_ioctl.h
 */

#define MAX_DEPACK_FRAMES 10

/**
 * This structure contains ring buffer information. To give buffer information
 * off1 and off1_size are normally enough. However, when data resides across
 * the start/end point of the ring buffer, off2 and off2_size are provided
 * as well as off1 and off1_size.
 */
struct tsmp_ringbuf_info
{
	uintptr_t off1; /**< Pointer to a ring buffer offset1 */
	uintptr_t off2; /**< Pointer to a ring buffer offset2 */
	int off1_size;
	int off2_size;
};

enum tsmp_pid_type
{
	TSMP_AUDIO_TYPE = 0,
	TSMP_VIDEO_TYPE,
	TSMP_SECTION_TYPE,
	TSMP_PES_TYPE,
	TSMP_TYPE_MAX,

	TSMP_UNKNOWN_TYPE = TSMP_TYPE_MAX,
};

struct tsmp_pid_info
{
	enum tsmp_pid_type type;
	uint16_t pid;
};

struct depack_frame_info
{
	uint32_t size; // size of a frame
	uint64_t pts; // timestamp of a frame
};

struct tsmp_depack_stream
{
	uintptr_t pBuffer; // secure output buffer address
	int32_t nLength; // total size of frames
	int32_t nFrames; // # of frames
	int64_t nTimestampMs; // deprecated
	struct depack_frame_info info[MAX_DEPACK_FRAMES]; // list of information for frames

	struct tsmp_pid_info pid_info;
};

/* clang-format off */
/** Event flag to notify */
#define TSMP_EVENT			(0x00000001U)

#define TSMP_IOCTL_MAGIC 'T'

/** Gets an event information after being notified */
#define TSMP_GET_BUF_INFO       _IOR(TSMP_IOCTL_MAGIC, 1, struct tsmp_ringbuf_info)
#define TSMP_SET_PID_INFO       _IOW(TSMP_IOCTL_MAGIC, 2, struct tsmp_pid_info)

#define TSMP_DEPACK_STREAM      _IOWR(TSMP_IOCTL_MAGIC, 3, struct tsmp_depack_stream)
#define TSMP_SET_MAX_NUMBER_FRAMES      _IOWR(TSMP_IOCTL_MAGIC, 4, uint32_t)

/* clang-format on */

#endif /* INCLUDED_TSMP_IOCTL */
