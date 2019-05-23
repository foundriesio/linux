 /* ak7759ctl.h
 *
 * Copyright (C) 2018 Asahi Kasei Microdevices Corporation.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *      18/02/19	     1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#ifndef __DSP_AK7759_H__
#define __DSP_AK7759_H__

/* IO CONTROL definition of AK7759 */
#define AK7759_IOCTL_MAGIC     's'

#define AK7759_MAGIC	0xD0
#define MAX_WREG		32
#define MAX_WCRAM		48

enum ak7759_ram_type {
	RAMTYPE_PRAM = 0,
	RAMTYPE_CRAM,
	RAMTYPE_OFREG,
};

enum ak7759_status {
	POWERDOWN = 0,
	SUSPEND,
	STANDBY,
	DOWNLOAD,
	DOWNLOAD_FINISH,
	RUN,
};

typedef struct _REG_CMD {
	unsigned char addr;
	unsigned char data;
} REG_CMD;

struct ak7759_wreg_handle {
	REG_CMD *regcmd;
	int len;
};

struct ak7759_wcram_handle{
	int    dsp;
	int    addr;
	unsigned char *cram;
	int    len;
};

struct ak7759_rcram_handle{ // Add151111
	int    dsp;
	int    addr;
};

#define AK7759_IOCTL_SETSTATUS	_IOW(AK7759_MAGIC, 0x10, int)
#define AK7759_IOCTL_SETMIR		_IOW(AK7759_MAGIC, 0x12, int)
#define AK7759_IOCTL_GETMIR		_IOR(AK7759_MAGIC, 0x13, unsigned long)
#define AK7759_IOCTL_WRITEREG	_IOW(AK7759_MAGIC, 0x14, struct ak7759_wreg_handle)
#define AK7759_IOCTL_WRITECRAM	_IOW(AK7759_MAGIC, 0x15, struct ak7759_wcram_handle)
#define AK7759_IOCTL_READCRAM	_IOW(AK7759_MAGIC, 0x11, struct ak7759_rcram_handle)


#endif

