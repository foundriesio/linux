/* soc/tcc/pmap.h
 *
 * Copyright (C) 2010 Telechips, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TCC_SIP_H_
#define __TCC_SIP_H_

#define SIP_DEV(x)		(((x)&0xF) << 12)
#define SIP_DEV_CHIP		(7)

#define SIP_CMD(dev, cmd)	(0x82000000|SIP_DEV(dev)|(cmd&0xFFF))

/* TCC SiP Service for chip info */
enum{
	SIP_CHIP_REV = SIP_CMD(SIP_DEV_CHIP, 0),
	SIP_CHIP_NAME,
	SIP_CHIP_ID,
};

/* Clock Control SIP Service */
enum {
	SIP_CLK_INIT = 0x82000000,
	SIP_CLK_SET_PLL,
	SIP_CLK_GET_PLL,
	SIP_CLK_SET_CLKCTRL,
	SIP_CLK_GET_CLKCTRL,
	SIP_CLK_ENABLE_CLKCTRL,
	SIP_CLK_DISABLE_CLKCTRL,
	SIP_CLK_IS_CLKCTRL,
	SIP_CLK_SET_PCLKCTRL,
	SIP_CLK_GET_PCLKCTRL,
	SIP_CLK_ENABLE_PERI,
	SIP_CLK_DISABLE_PERI,
	SIP_CLK_IS_PERI,
	SIP_CLK_ENABLE_DDIBUS,
	SIP_CLK_DISABLE_DDIBUS,
	SIP_CLK_IS_DDIBUS,
	SIP_CLK_RESET_DDIBUS,
	SIP_CLK_ENABLE_IOBUS,
	SIP_CLK_DISABLE_IOBUS,
	SIP_CLK_IS_IOBUS,
	SIP_CLK_RESET_IOBUS,
	SIP_CLK_ENABLE_HSIOBUS,
	SIP_CLK_DISABLE_HSIOBUS,
	SIP_CLK_IS_HSIOBUS,
	SIP_CLK_RESET_HSIOBUS,
	SIP_CLK_ENABLE_VPUBUS,
	SIP_CLK_DISABLE_VPUBUS,
	SIP_CLK_IS_VPUBUS,
	SIP_CLK_RESET_VPUBUS,
	SIP_CLK_ENABLE_ISODDI,
	SIP_CLK_DISABLE_ISODDI,
	SIP_CLK_IS_ISODDI,
	SIP_CLK_ENABLE_ISOTOP,
	SIP_CLK_DISABLE_ISOTOP,
	SIP_CLK_IS_ISOTOP,
	SIP_CLK_PWDN_DDIBUS,
	SIP_CLK_PWDN_IOBUS,
	SIP_CLK_PWDN_HSIOBUS,
};


#endif
