/*
 * fs/sdcardfs/packagelist.c
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Seunghwan Hyun,
 *               Sunghwan Yun, Sungjong Seo
 *
 * This program has been developed as a stackable file system based on
 * the WrapFS which written by
 *
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009     Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#include "sdcardfs.h"
#include <linux/ctype.h>
#include <linux/dcache.h>
#include <linux/slab.h>

/* Kernel has already enforced everything we returned through
 * derive_permissions_locked(), so this is used to lock down access
 * even further, such as enforcing that apps hold sdcard_rw.
 */
int check_caller_access_to_name(struct inode *parent_node, const struct qstr *name)
{
	struct qstr q_autorun = QSTR_LITERAL("autorun.inf");
	struct qstr q__android_secure = QSTR_LITERAL(".android_secure");
	struct qstr q_android_secure = QSTR_LITERAL("android_secure");

	/* Always block security-sensitive files at root */
	if (parent_node && SDCARDFS_I(parent_node)->data->perm == PERM_ROOT) {
		if (qstr_case_eq(name, &q_autorun)
			|| qstr_case_eq(name, &q__android_secure)
			|| qstr_case_eq(name, &q_android_secure)) {
			return 0;
		}
	}

	/* Root always has access; access for any other UIDs should always
	 * be controlled through packages.list.
	 */
	if (from_kuid(&init_user_ns, current_fsuid()) == 0)
		return 1;

	/* No extra permissions to enforce */
	return 1;
}

void fixup_perms_by_flag(int flags, const struct qstr *key, uint32_t userid)
{
	struct sdcardfs_sb_info *sbinfo;
	struct limit_search limit = {
		.flags = flags,
		.userid = userid,
	};

	if (key) {
		limit.name.name = key->name;
		limit.name.len = key->len;
	}
	mutex_lock(&sdcardfs_super_list_lock);
	list_for_each_entry(sbinfo, &sdcardfs_super_list, list) {
		if (sbinfo_has_sdcard_magic(sbinfo))
			fixup_perms_recursive(sbinfo->sb->s_root, &limit);
	}
	mutex_unlock(&sdcardfs_super_list_lock);
}

static struct pkg_list sdcardfs_pkg_list = {
		.update = fixup_perms_by_flag,
};

int packagelist_init(void)
{
	pkglist_register_update_listener(&sdcardfs_pkg_list);
	return 0;
}

void packagelist_exit(void)
{
	pkglist_unregister_update_listener(&sdcardfs_pkg_list);
}
