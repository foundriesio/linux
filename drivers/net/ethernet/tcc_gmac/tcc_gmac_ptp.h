
/*
 * linux/driver/net/tcc_gmac/tcc_gmac_ptp.c
 * 	
 * Author : Telechips <linux@telechips.com>
 * Created : Jan 28, 2013
 * Description : This is the driver for the Telechips MAC 10/100/1000 on-chip Ethernet controllers.  
 *               Telechips Ethernet IPs are built around a Synopsys IP Core.
 *
 * Copyright (C) 2013 Telechips
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */ 

#include <linux/module.h>
#include <linux/ptp_clock_kernel.h>

#include "tcc_gmac_ctrl.h"
#include "tcc_gmac_drv.h"

struct ptp_clock *tcc_gmac_ptp_probe(struct net_device *dev);
void tcc_gmac_ptp_remove(struct ptp_clock *ptp);
