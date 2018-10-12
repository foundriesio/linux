/*
 * tcc-dummy.h  --  TCC DUMMY Soc Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Based on tcc-dummy.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _TCCDUMMY_H
#define _TCCDUMMY_H

/* TCC_DUMMY register space */

#define TCC_DUMMY_SYSCLK	0
#define TCC_DUMMY_DAI		0

struct tcc_dummy_setup_data {
	int            spi;
	int            i2c_bus;
	unsigned short i2c_address;
};

#endif

