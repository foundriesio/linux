/*
 *	drivers/net/phy/broadcom.c
 *
 *	Broadcom BCM5411, BCM5421 and BCM5461 Gigabit Ethernet
 *	transceivers.
 *
 *	Copyright (c) 2006  Maciej W. Rozycki
 *
 *	Inspired by code written by Amy Fong.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/phy.h>

MODULE_DESCRIPTION("TCC Broadcom PHY driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");

#define BCM89810_MIICTL		(0x00)
#define BCM89810_DSPRW		(0x15)
#define BCM89810_DSPADDR	(0x17)
#define BCM89810_AUXCTRL	(0x18)

#define BCM89810_EXPREG0E	(0x0F0E)
#define BCM89810_EXPREG90	(0x0F90)
#define BCM89810_EXPREG92	(0x0F92)
#define BCM89810_EXPREG93	(0x0F93)
#define BCM89810_EXPREG99	(0x0F99)
#define BCM89810_EXPREG9A	(0x0F9A)
#define BCM89810_EXPREG9F	(0x0F9F)
#define BCM89810_EXPREGFD	(0x0FFD)
#define BCM89810_EXPREGFE	(0x0FFE)

static int bcm89810_config_init(struct phy_device *phydev)
{
	// -----------begin PHY optimization portion----------
	phy_write(phydev, BCM89810_MIICTL, 0x8000); // reset

	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG93);
	phy_write(phydev, BCM89810_DSPRW, 0x107F);
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG90);
	phy_write(phydev, BCM89810_DSPRW, 0x0001);
	phy_write(phydev, BCM89810_MIICTL, 0x3000);
	phy_write(phydev, BCM89810_MIICTL, 0x0200);

	phy_write(phydev, BCM89810_AUXCTRL, 0x0C00);

	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG90);
	phy_write(phydev, BCM89810_DSPRW, 0x0000);
	phy_write(phydev, BCM89810_MIICTL, 0x0100);

	phy_write(phydev, BCM89810_DSPADDR, 0x0001);
	phy_write(phydev, BCM89810_DSPRW, 0x0027);

	phy_write(phydev, BCM89810_DSPADDR, 0x000E);
	phy_write(phydev, BCM89810_DSPRW, 0x9B52);

	phy_write(phydev, BCM89810_DSPADDR, 0x000F);
	phy_write(phydev, BCM89810_DSPRW, 0xA04D);

	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG90);
	phy_write(phydev, BCM89810_DSPRW, 0x0001);

	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG92);
	phy_write(phydev, BCM89810_DSPRW, 0x9224);

	phy_write(phydev, BCM89810_DSPADDR, 0x000A);
	phy_write(phydev, BCM89810_DSPRW, 0x0323);

	// shut off unused clocks
	// If verification of the previous register
	// writes is required, it should be done before
	// shutting off the unused clocks (next 4 lines below)
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREGFD);
	phy_write(phydev, BCM89810_DSPRW, 0x1C3F);

	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREGFE);
	phy_write(phydev, BCM89810_DSPRW, 0x1C3F);

	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG99);
	phy_write(phydev, BCM89810_DSPRW, 0x7180);
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG9A);
	phy_write(phydev, BCM89810_DSPRW, 0x34C0);
	// ---------END PHY optimization portion-----------

	// ---------begin MACconfiguration portion-----------
#if defined(CONFIG_TCC_RGMII_MODE_BCM_89810)
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG0E);
	// turn off MII Lite
	phy_write(phydev, BCM89810_DSPRW, 0x0000);
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG9F);
	// unset MII reverse
	phy_write(phydev, BCM89810_DSPRW, 0x0000);
	phy_write(phydev, BCM89810_AUXCTRL, 0xF1E7); // turn on RGMII mode
#elif defined(CONFIG_TCC_MII_MODE_BCM_89810)
	phy_write(phydev, BCM89810_AUXCTRL, 0xF167); // turn off RGMII mode
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG0E);
	// turn on MII Lite
	phy_write(phydev, BCM89810_DSPRW, 0x0800);
	phy_write(phydev, BCM89810_DSPADDR, BCM89810_EXPREG9F);
	// unset MII reverse
	phy_write(phydev, BCM89810_DSPRW, 0x0000);
#else
//	error "Not supported Phy Mode"
#endif
	// ---------end MACconfiguration portion-----------

	// ---------begin mdi configuration portion-----------
#ifdef CONFIG_TCC_BOARDCOM_MDI_MASTER_MODE_BCM_89810
	phy_write(phydev, BCM89810_MIICTL, 0x0208);
#else
	phy_write(phydev, BCM89810_MIICTL, 0x0200);
#endif
	// ---------end mdi configuration portion-----------

	return 0;
}

int bcm89810_update_link(struct phy_device *phydev)
{
	int status;

	/* Read link and autonegotiation status */
	status = phy_read(phydev, MII_BMSR);

	if (status < 0)
		return status;

	if ((status & BMSR_LSTATUS) == 0)
		phydev->link = 0;
	else
		phydev->link = 1;

	return 0;
}
int bcm89810_read_status(struct phy_device *phydev)
{
	int err;
	static int mdi_mode;
	static int count;
	static int max;


	err = bcm89810_update_link(phydev);
	if (err)
		return err;

#if 0
	if (phydev->link == 0) {
		if (count++ > max) {
			int rand;

			if (mdi_mode) {
				phy_write(phydev, BCM89810_MIICTL, 0x0200);
				mdi_mode = 0;
			} else {
				phy_write(phydev, BCM89810_MIICTL, 0x0208);
				mdi_mode = 1;
			}
			count = 0;
			get_random_bytes_arch(&rand, 1);
			max = rand % 3;
			//printk("mdi_mode(%d), max(%d)\n", mdi_mode, max);
		}
	}
#endif

	phydev->duplex = DUPLEX_FULL;
	phydev->speed = SPEED_100;

	return 0;
}

static int bcm89810_config_aneg(struct phy_device *phydev)
{

	return 0;
}

static struct phy_driver bcm89810_driver = {
	.phy_id			= 0x03625cc0,
	.phy_id_mask	= 0xfffffff0,
	.name			= "Broadcom BCM89810",
	.features		= SUPPORTED_100baseT_Full | SUPPORTED_MII,
	.flags			= PHY_POLL,
	.config_init	= bcm89810_config_init,
	.read_status	= bcm89810_read_status,
	.config_aneg	= bcm89810_config_aneg,
	.driver		= { .owner = THIS_MODULE },
};


static int __init tcc_broadcom_init(void)
{
	int ret;

	ret = phy_driver_register(&bcm89810_driver);
	if (ret)
		phy_driver_unregister(&bcm89810_driver);


	return ret;
}

static void __exit tcc_broadcom_exit(void)
{
	phy_driver_unregister(&bcm89810_driver);
}

module_init(tcc_broadcom_init);
module_exit(tcc_broadcom_exit);

static struct mdio_device_id __maybe_unused tcc_broadcom_tbl[] = {
	{ 0x03625cc0, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, tcc_broadcom_tbl);
