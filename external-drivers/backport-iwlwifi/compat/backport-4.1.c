/*
 * Copyright (c) 2015  Stefan Assmann <sassmann@kpanic.de>
 * Copyright (c) 2015  Hauke Mehrtens <hauke@hauke-m.de>
 *
 * Backport functionality introduced in Linux 4.1.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/netdevice.h>
#include <linux/tty.h>

netdev_features_t passthru_features_check(struct sk_buff *skb,
					  struct net_device *dev,
					  netdev_features_t features)
{
	return features;
}
EXPORT_SYMBOL_GPL(passthru_features_check);

#ifdef CONFIG_TTY
#if LINUX_VERSION_IS_GEQ(4,0,0)
static void unset_locked_termios(struct ktermios *termios,
				 struct ktermios *old,
				 struct ktermios *locked)
{
	int	i;

#define NOSET_MASK(x, y, z) (x = ((x) & ~(z)) | ((y) & (z)))

	if (!locked) {
		printk(KERN_WARNING "Warning?!? termios_locked is NULL.\n");
		return;
	}

	NOSET_MASK(termios->c_iflag, old->c_iflag, locked->c_iflag);
	NOSET_MASK(termios->c_oflag, old->c_oflag, locked->c_oflag);
	NOSET_MASK(termios->c_cflag, old->c_cflag, locked->c_cflag);
	NOSET_MASK(termios->c_lflag, old->c_lflag, locked->c_lflag);
	termios->c_line = locked->c_line ? old->c_line : termios->c_line;
	for (i = 0; i < NCCS; i++)
		termios->c_cc[i] = locked->c_cc[i] ?
			old->c_cc[i] : termios->c_cc[i];
	/* FIXME: What should we do for i/ospeed */
}

int tty_set_termios(struct tty_struct *tty, struct ktermios *new_termios)
{
	struct ktermios old_termios;
	struct tty_ldisc *ld;

	WARN_ON(tty->driver->type == TTY_DRIVER_TYPE_PTY &&
		tty->driver->subtype == PTY_TYPE_MASTER);
	/*
	 *	Perform the actual termios internal changes under lock.
	 */


	/* FIXME: we need to decide on some locking/ordering semantics
	   for the set_termios notification eventually */
	down_write(&tty->termios_rwsem);
	old_termios = tty->termios;
	tty->termios = *new_termios;
	unset_locked_termios(&tty->termios, &old_termios, &tty->termios_locked);

	if (tty->ops->set_termios)
		tty->ops->set_termios(tty, &old_termios);
	else
		tty_termios_copy_hw(&tty->termios, &old_termios);

	ld = tty_ldisc_ref(tty);
	if (ld != NULL) {
		if (ld->ops->set_termios)
			ld->ops->set_termios(tty, &old_termios);
		tty_ldisc_deref(ld);
	}
	up_write(&tty->termios_rwsem);
	return 0;
}
EXPORT_SYMBOL_GPL(tty_set_termios);
#endif /* LINUX_VERSION_IS_GEQ(4,0,0) */
#endif /* CONFIG_TTY */
