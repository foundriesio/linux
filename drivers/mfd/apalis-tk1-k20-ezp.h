/*
 * Copyright 2016 Toradex AG
 * Dominik Sliwa <dominik.sliwa@toradex.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#ifndef __DRIVERS_MFD_APALIS_TK1_K20_H
#define __DRIVERS_MFD_APALIS_TK1_K20_H

#ifdef CONFIG_APALIS_TK1_K20_EZP
#define APALIS_TK1_K20_FW_FOPT_ADDR	0x40D
#define APALIS_TK1_K20_FOPT_EZP_ENA	BIT(1)
#define APALIS_TK1_K20_FW_VER_ADDR	0x410

#define APALIS_TK1_K20_FLASH_SIZE	0x80000

/* EZ Port commands */
#define APALIS_TK1_K20_EZP_WREN		0x06
#define APALIS_TK1_K20_EZP_WRDI		0x04
#define APALIS_TK1_K20_EZP_RDSR		0x05
#define APALIS_TK1_K20_EZP_READ		0x03
#define APALIS_TK1_K20_EZP_FREAD	0x0B
#define APALIS_TK1_K20_EZP_SP		0x02
#define APALIS_TK1_K20_EZP_SE		0xD8
#define APALIS_TK1_K20_EZP_BE		0xC7
#define APALIS_TK1_K20_EZP_RESET	0xB9
#define APALIS_TK1_K20_EZP_WRFCCOB	0xBA
#define APALIS_TK1_K20_EZP_FRDFCOOB	0xBB

/* Bits of EZ Port Status register */
#define APALIS_TK1_K20_EZP_STA_WIP	BIT(0)
#define APALIS_TK1_K20_EZP_STA_WEN	BIT(1)
#define APALIS_TK1_K20_EZP_STA_BEDIS	BIT(2)
#define APALIS_TK1_K20_EZP_STA_WEF	BIT(6)
#define APALIS_TK1_K20_EZP_STA_FS	BIT(7)

#define APALIS_TK1_K20_EZP_MAX_SPEED	3180000
#define APALIS_TK1_K20_EZP_MAX_DATA	32
#define APALIS_TK1_K20_EZP_WRITE_SIZE	32

static const struct firmware *fw_entry;
#endif /* CONFIG_APALIS_TK1_K20_EZP */

#endif /* __DRIVERS_MFD_APALIS_TK1_K20_H */
