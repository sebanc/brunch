#ifndef __BACKPORT_LINUX_PHY_H
#define __BACKPORT_LINUX_PHY_H
#include_next <linux/phy.h>
#include <linux/compiler.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,5,0)
#define phydev_name LINUX_BACKPORT(phydev_name)
static inline const char *phydev_name(const struct phy_device *phydev)
{
	return dev_name(&phydev->dev);
}

#define mdiobus_is_registered_device LINUX_BACKPORT(mdiobus_is_registered_device)
static inline bool mdiobus_is_registered_device(struct mii_bus *bus, int addr)
{
	return bus->phy_map[addr];
}

#define phy_attached_print LINUX_BACKPORT(phy_attached_print)
void phy_attached_print(struct phy_device *phydev, const char *fmt, ...)
	__printf(2, 3);
#define phy_attached_info LINUX_BACKPORT(phy_attached_info)
void phy_attached_info(struct phy_device *phydev);

static inline int backport_mdiobus_register(struct mii_bus *bus)
{
	bus->irq = kmalloc(sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (!bus->irq) {
		pr_err("mii_bus irq allocation failed\n");
		return -ENOMEM;
	}

	memset(bus->irq, PHY_POLL, sizeof(int) * PHY_MAX_ADDR);

	return __mdiobus_register(bus, THIS_MODULE);
}
#ifdef mdiobus_register
#undef mdiobus_register
#endif
#define mdiobus_register LINUX_BACKPORT(mdiobus_register)

static inline void backport_mdiobus_unregister(struct mii_bus *bus)
{
	kfree(bus->irq);
	mdiobus_unregister(bus);
}
#define mdiobus_unregister LINUX_BACKPORT(mdiobus_unregister)
#endif /* < 4.5 */

#if LINUX_VERSION_IS_LESS(4,5,0)
#define phydev_get_addr LINUX_BACKPORT(phydev_get_addr)
static inline int phydev_get_addr(struct phy_device *phydev)
{
	return phydev->addr;
}
#else
#define phydev_get_addr LINUX_BACKPORT(phydev_get_addr)
static inline int phydev_get_addr(struct phy_device *phydev)
{
	return phydev->mdio.addr;
}
#endif

#endif /* __BACKPORT_LINUX_PHY_H */
