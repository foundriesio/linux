/*
 * Core PHY library, taken from phy.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/export.h>
#include <linux/phy.h>

static void mmd_phy_indirect(struct mii_bus *bus, int phy_addr, int devad,
			     u16 regnum)
{
	/* Write the desired MMD Devad */
	bus->write(bus, phy_addr, MII_MMD_CTRL, devad);

	/* Write the desired MMD register address */
	bus->write(bus, phy_addr, MII_MMD_DATA, regnum);

	/* Select the Function : DATA with no post increment */
	bus->write(bus, phy_addr, MII_MMD_CTRL, devad | MII_MMD_CTRL_NOINCR);
}

/**
 * phy_read_mmd - Convenience function for reading a register
 * from an MMD on a given PHY.
 * @phydev: The phy_device struct
 * @devad: The MMD to read from (0..31)
 * @regnum: The register on the MMD to read (0..65535)
 *
 * Same rules as for phy_read();
 */
int phy_read_mmd(struct phy_device *phydev, int devad, u32 regnum)
{
	int val;

	if (regnum > (u16)~0 || devad > 32)
		return -EINVAL;

	if (phydev->drv->read_mmd) {
		val = phydev->drv->read_mmd(phydev, devad, regnum);
	} else if (phydev->is_c45) {
		u32 addr = MII_ADDR_C45 | (devad << 16) | (regnum & 0xffff);

		val = mdiobus_read(phydev->mdio.bus, phydev->mdio.addr, addr);
	} else {
		struct mii_bus *bus = phydev->mdio.bus;
		int phy_addr = phydev->mdio.addr;

		mutex_lock(&bus->mdio_lock);
		mmd_phy_indirect(bus, phy_addr, devad, regnum);

		/* Read the content of the MMD's selected register */
		val = bus->read(bus, phy_addr, MII_MMD_DATA);
		mutex_unlock(&bus->mdio_lock);
	}
	return val;
}
EXPORT_SYMBOL(phy_read_mmd);

/**
 * phy_write_mmd - Convenience function for writing a register
 * on an MMD on a given PHY.
 * @phydev: The phy_device struct
 * @devad: The MMD to read from
 * @regnum: The register on the MMD to read
 * @val: value to write to @regnum
 *
 * Same rules as for phy_write();
 */
int phy_write_mmd(struct phy_device *phydev, int devad, u32 regnum, u16 val)
{
	int ret;

	if (regnum > (u16)~0 || devad > 32)
		return -EINVAL;

	if (phydev->drv->write_mmd) {
		ret = phydev->drv->write_mmd(phydev, devad, regnum, val);
	} else if (phydev->is_c45) {
		u32 addr = MII_ADDR_C45 | (devad << 16) | (regnum & 0xffff);

		ret = mdiobus_write(phydev->mdio.bus, phydev->mdio.addr,
				    addr, val);
	} else {
		struct mii_bus *bus = phydev->mdio.bus;
		int phy_addr = phydev->mdio.addr;

		mutex_lock(&bus->mdio_lock);
		mmd_phy_indirect(bus, phy_addr, devad, regnum);

		/* Write the data into MMD's selected register */
		bus->write(bus, phy_addr, MII_MMD_DATA, val);
		mutex_unlock(&bus->mdio_lock);

		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(phy_write_mmd);

/**
 * __phy_modify() - Convenience function for modifying a PHY register
 * @phydev: a pointer to a &struct phy_device
 * @regnum: register number
 * @mask: bit mask of bits to clear
 * @set: bit mask of bits to set
 *
 * Unlocked helper function which allows a PHY register to be modified as
 * new register value = (old register value & mask) | set
 */
int __phy_modify(struct phy_device *phydev, u32 regnum, u16 mask, u16 set)
{
	int ret, res;

	ret = __phy_read(phydev, regnum);
	if (ret >= 0) {
		res = __phy_write(phydev, regnum, (ret & ~mask) | set);
		if (res < 0)
			ret = res;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(__phy_modify);
