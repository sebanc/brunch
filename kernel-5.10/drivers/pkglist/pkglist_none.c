/*
 * Copyright (C) 2017 Google Inc., Author: Daniel Rosenberg <drosen@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ctype.h>
#include <linux/dcache.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pkglist.h>

kuid_t pkglist_get_appid(const char *key)
{
	return make_kuid(&init_user_ns, 0);
}
EXPORT_SYMBOL_GPL(pkglist_get_appid);

kgid_t pkglist_get_ext_gid(const char *key)
{
	return make_kgid(&init_user_ns, 0);
}
EXPORT_SYMBOL_GPL(pkglist_get_ext_gid);

bool pkglist_user_is_excluded(const char *key, uint32_t user)
{
	return false;
}
EXPORT_SYMBOL_GPL(pkglist_user_is_excluded);

kuid_t pkglist_get_allowed_appid(const char *key, uint32_t user)
{
	return make_kuid(&init_user_ns, 0);
}
EXPORT_SYMBOL_GPL(pkglist_get_allowed_appid);

void pkglist_register_update_listener(struct pkg_list *pkg) { }
EXPORT_SYMBOL_GPL(pkglist_register_update_listener);

void pkglist_unregister_update_listener(struct pkg_list *pkg) { }
EXPORT_SYMBOL_GPL(pkglist_unregister_update_listener);

int __init pkglist_init(void)
{
	return 0;
}
module_init(pkglist_init);

void pkglist_exit(void) { }

module_exit(pkglist_exit);

MODULE_AUTHOR("Daniel Rosenberg, Google");
MODULE_DESCRIPTION("Empty Pkglist implementation");
MODULE_LICENSE("GPL v2");
