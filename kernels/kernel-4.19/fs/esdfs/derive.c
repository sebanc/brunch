/*
 * Copyright (c) 2013-2014 Motorola Mobility LLC
 * Copyright (C) 2017      Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/proc_fs.h>
#include <linux/hashtable.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/security.h>
#include <linux/uaccess.h>
#include "esdfs.h"

static struct qstr names_secure[] = {
	QSTR_LITERAL("autorun.inf"),
	QSTR_LITERAL(".android_secure"),
	QSTR_LITERAL("android_secure"),
	QSTR_LITERAL("")
};

/* special path name searches */
static inline bool match_name(struct qstr *name, struct qstr names[])
{
	int i = 0;

	BUG_ON(!name);
	for (i = 0; *names[i].name; i++)
		if (qstr_case_eq(name, &names[i]))
			return true;

	return false;
}

unsigned esdfs_package_list_version;

static void fixup_perms_by_flag(int flags, const struct qstr *key,
					uint32_t userid)
{
	esdfs_package_list_version++;
}

static struct pkg_list esdfs_pkg_list = {
		.update = fixup_perms_by_flag,
};

int esdfs_init_package_list(void)
{
	pkglist_register_update_listener(&esdfs_pkg_list);
	return 0;
}

void esdfs_destroy_package_list(void)
{
	pkglist_unregister_update_listener(&esdfs_pkg_list);
}

/*
 * Derive an entry's premissions tree position based on its parent.
 */
void esdfs_derive_perms(struct dentry *dentry)
{
	struct esdfs_inode_info *inode_i = ESDFS_I(dentry->d_inode);
	bool is_root;
	int ret;
	kuid_t appid;
	struct qstr q_Download = QSTR_LITERAL("Download");
	struct qstr q_Android = QSTR_LITERAL("Android");
	struct qstr q_data = QSTR_LITERAL("data");
	struct qstr q_obb = QSTR_LITERAL("obb");
	struct qstr q_media = QSTR_LITERAL("media");
	struct qstr q_cache = QSTR_LITERAL("cache");
	struct qstr q_user = QSTR_LITERAL("user");
	struct esdfs_inode_info *parent_i = ESDFS_I(dentry->d_parent->d_inode);

	spin_lock(&dentry->d_lock);
	is_root = IS_ROOT(dentry);
	spin_unlock(&dentry->d_lock);
	if (is_root)
		return;

	/* Inherit from the parent to start */
	inode_i->tree = parent_i->tree;
	inode_i->userid = parent_i->userid;
	inode_i->appid = parent_i->appid;
	inode_i->under_obb = parent_i->under_obb;

	/*
	 * ESDFS_TREE_MEDIA* are intentionally dead ends.
	 */
	switch (inode_i->tree) {
	case ESDFS_TREE_ROOT_LEGACY:
		inode_i->tree = ESDFS_TREE_ROOT;
		ret = kstrtou32(dentry->d_name.name, 0, &inode_i->userid);
		if (qstr_case_eq(&dentry->d_name, &q_obb))
			inode_i->tree = ESDFS_TREE_ANDROID_OBB;
		break;

	case ESDFS_TREE_ROOT:
		inode_i->tree = ESDFS_TREE_MEDIA;
		if (qstr_case_eq(&dentry->d_name, &q_Download))
			inode_i->tree = ESDFS_TREE_DOWNLOAD;
		else if (qstr_case_eq(&dentry->d_name, &q_Android))
			inode_i->tree = ESDFS_TREE_ANDROID;
		break;

	case ESDFS_TREE_ANDROID:
		if (qstr_case_eq(&dentry->d_name, &q_data)) {
			inode_i->tree = ESDFS_TREE_ANDROID_DATA;
		} else if (qstr_case_eq(&dentry->d_name, &q_obb)) {
			inode_i->tree = ESDFS_TREE_ANDROID_OBB;
			inode_i->under_obb = true;
		} else if (qstr_case_eq(&dentry->d_name, &q_media)) {
			inode_i->tree = ESDFS_TREE_ANDROID_MEDIA;
		} else if (ESDFS_RESTRICT_PERMS(ESDFS_SB(dentry->d_sb)) &&
			 qstr_case_eq(&dentry->d_name, &q_user)) {
			inode_i->tree = ESDFS_TREE_ANDROID_USER;
		}
		break;

	case ESDFS_TREE_ANDROID_DATA:
	case ESDFS_TREE_ANDROID_OBB:
	case ESDFS_TREE_ANDROID_MEDIA:
		appid = pkglist_get_allowed_appid(dentry->d_name.name,
						inode_i->userid);
		if (uid_valid(appid))
			inode_i->appid = esdfs_from_kuid(
					ESDFS_SB(dentry->d_sb), appid);
		else
			inode_i->appid = 0;
		inode_i->tree = ESDFS_TREE_ANDROID_APP;
		break;
	case ESDFS_TREE_ANDROID_APP:
		if (qstr_case_eq(&dentry->d_name, &q_cache))
			inode_i->tree = ESDFS_TREE_ANDROID_APP_CACHE;
		break;
	case ESDFS_TREE_ANDROID_USER:
		/* Another user, so start over */
		inode_i->tree = ESDFS_TREE_ROOT;
		ret = kstrtou32(dentry->d_name.name, 0, &inode_i->userid);
		break;
	}
}

/* Apply tree position-specific permissions */
void esdfs_set_derived_perms(struct inode *inode)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(inode->i_sb);
	struct esdfs_inode_info *inode_i = ESDFS_I(inode);
	gid_t gid = sbi->upper_perms.gid;

	esdfs_i_uid_write(inode, sbi->upper_perms.uid);
	inode->i_mode &= S_IFMT;
	if (ESDFS_RESTRICT_PERMS(sbi))
		esdfs_i_gid_write(inode, gid);
	else {
		if (gid == AID_SDCARD_RW && !test_opt(sbi, DEFAULT_NORMAL))
			esdfs_i_gid_write(inode, AID_SDCARD_RW);
		else
			esdfs_i_gid_write(inode, derive_uid(inode_i, gid));
		inode->i_mode |= sbi->upper_perms.dmask;
	}

	switch (inode_i->tree) {
	case ESDFS_TREE_ROOT_LEGACY:
		if (ESDFS_RESTRICT_PERMS(sbi))
			inode->i_mode |= sbi->upper_perms.dmask;
		else if (test_opt(sbi, DERIVE_MULTI)) {
			inode->i_mode &= S_IFMT;
			inode->i_mode |= 0711;
		}
		break;

	case ESDFS_TREE_NONE:
	case ESDFS_TREE_ROOT:
		if (ESDFS_RESTRICT_PERMS(sbi)) {
			esdfs_i_gid_write(inode, AID_SDCARD_R);
			inode->i_mode |= sbi->upper_perms.dmask;
		} else if (test_opt(sbi, DERIVE_PUBLIC) &&
			   test_opt(ESDFS_SB(inode->i_sb), DERIVE_CONFINE)) {
			inode->i_mode &= S_IFMT;
			inode->i_mode |= 0771;
		}
		break;

	case ESDFS_TREE_MEDIA:
		if (ESDFS_RESTRICT_PERMS(sbi)) {
			esdfs_i_gid_write(inode, AID_SDCARD_R);
			inode->i_mode |= 0770;
		}
		break;

	case ESDFS_TREE_DOWNLOAD:
	case ESDFS_TREE_ANDROID:
	case ESDFS_TREE_ANDROID_DATA:
	case ESDFS_TREE_ANDROID_OBB:
	case ESDFS_TREE_ANDROID_MEDIA:
		if (ESDFS_RESTRICT_PERMS(sbi))
			inode->i_mode |= 0771;
		break;

	case ESDFS_TREE_ANDROID_APP:
	case ESDFS_TREE_ANDROID_APP_CACHE:
		if (inode_i->appid)
			esdfs_i_uid_write(inode, derive_uid(inode_i,
							inode_i->appid));
		if (ESDFS_RESTRICT_PERMS(sbi))
			inode->i_mode |= 0770;
		break;

	case ESDFS_TREE_ANDROID_USER:
		if (ESDFS_RESTRICT_PERMS(sbi)) {
			esdfs_i_gid_write(inode, AID_SDCARD_ALL);
			inode->i_mode |= 0770;
		}
		inode->i_mode |= 0770;
		break;
	}

	/* strip execute bits from any non-directories */
	if (!S_ISDIR(inode->i_mode))
		inode->i_mode &= ~S_IXUGO;
}

/*
 * Before rerouting a lookup to follow a pseudo hard link, make sure that
 * a stub exists at the source.  Without it, readdir won't see an entry there
 * resulting in a strange user experience.
 */
static int lookup_link_source(struct dentry *dentry, struct dentry *parent)
{
	struct path lower_parent_path, lower_path;
	int err;

	esdfs_get_lower_path(parent, &lower_parent_path);

	/* Check if the stub user profile folder is there. */
	err = esdfs_lookup_nocase(&lower_parent_path, &dentry->d_name,
					&lower_path);
	/* Remember it to handle renames and removal. */
	if (!err)
		esdfs_set_lower_stub_path(dentry, &lower_path);

	esdfs_put_lower_path(parent, &lower_parent_path);

	return err;
}

int esdfs_is_dl_lookup(struct dentry *dentry, struct dentry *parent)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(parent->d_sb);
	struct esdfs_inode_info *parent_i = ESDFS_I(parent->d_inode);
	/*
	 * Return 1 if this is the Download directory:
	 * The test for download checks:
	 * 1. The parent is the mount root.
	 * 2. The directory is named 'Download'.
	 * 3. The stub for the directory exists.
	 */
	if (test_opt(sbi, SPECIAL_DOWNLOAD) &&
			parent_i->tree == ESDFS_TREE_ROOT &&
			ESDFS_DENTRY_NEEDS_DL_LINK(dentry) &&
			lookup_link_source(dentry, parent) == 0) {
		return 1;
	}

	return 0;
}

int esdfs_derived_lookup(struct dentry *dentry, struct dentry **parent)
{
	struct esdfs_sb_info *sbi = ESDFS_SB((*parent)->d_sb);
	struct esdfs_inode_info *parent_i = ESDFS_I((*parent)->d_inode);
	struct qstr q_Android = QSTR_LITERAL("Android");

	/* Deny access to security-sensitive entries. */
	if (ESDFS_I((*parent)->d_inode)->tree == ESDFS_TREE_ROOT &&
	    match_name(&dentry->d_name, names_secure)) {
		pr_debug("esdfs: denying access to: %s", dentry->d_name.name);
		return -EACCES;
	}

	/* Pin the unified mode obb link parent as it flies by. */
	if (!sbi->obb_parent &&
	    test_opt(sbi, DERIVE_UNIFIED) &&
	    parent_i->tree == ESDFS_TREE_ROOT &&
	    parent_i->userid == 0 &&
	    qstr_case_eq(&dentry->d_name, &q_Android))
		sbi->obb_parent = dget(dentry);		/* keep it pinned */

	/*
	 * Handle obb directory "grafting" as a pseudo hard link by overriding
	 * its parent to point to the target obb directory's parent.  The rest
	 * of the lookup process will take care of setting up the bottom half
	 * to point to the real obb directory.
	 */
	if (parent_i->tree == ESDFS_TREE_ANDROID &&
	    ESDFS_DENTRY_NEEDS_LINK(dentry) &&
	    lookup_link_source(dentry, *parent) == 0) {
		BUG_ON(!sbi->obb_parent);
		if (ESDFS_INODE_CAN_LINK((*parent)->d_inode))
			*parent = dget(sbi->obb_parent);
	}

	return 0;
}

int esdfs_derived_revalidate(struct dentry *dentry, struct dentry *parent)
{
	/*
	 * If obb is not linked yet, it means the dentry is pointing to the
	 * stub.  Invalidate the dentry to force another lookup.
	 */
	if (ESDFS_I(parent->d_inode)->tree == ESDFS_TREE_ANDROID &&
	    ESDFS_INODE_CAN_LINK(dentry->d_inode) &&
	    ESDFS_DENTRY_NEEDS_LINK(dentry) &&
	    !ESDFS_DENTRY_IS_LINKED(dentry))
		return -ESTALE;
	if (ESDFS_I(parent->d_inode)->tree == ESDFS_TREE_ROOT &&
	    ESDFS_DENTRY_NEEDS_DL_LINK(dentry) &&
	    !ESDFS_DENTRY_IS_LINKED(dentry))
		return -ESTALE;
	return 0;
}

/*
 * Implement the extra checking that is done based on the caller's package
 * list-based access rights.
 */
int esdfs_check_derived_permission(struct inode *inode, int mask)
{
	const struct cred *cred;
	uid_t uid, appid;

	/*
	 * If we don't need to restrict access based on app GIDs and confine
	 * writes to outside of the Android/... tree, we can skip all of this.
	 */
	if (!ESDFS_RESTRICT_PERMS(ESDFS_SB(inode->i_sb)) &&
	    !test_opt(ESDFS_SB(inode->i_sb), DERIVE_CONFINE))
			return 0;

	cred = current_cred();
	uid = from_kuid(&init_user_ns, cred->uid);
	appid = uid % PKG_APPID_PER_USER;

	/* Reads, owners, and root are always granted access */
	if (!(mask & (MAY_WRITE | ESDFS_MAY_CREATE)) ||
	    uid == 0 || uid_eq(cred->uid, inode->i_uid))
		return 0;

	/*
	 * Grant access to sdcard_rw holders, unless we are in unified mode
	 * and we are trying to write to the protected /Android tree or to
	 * create files in the root (aka, "confined" access).
	 */
	if ((!test_opt(ESDFS_SB(inode->i_sb), DERIVE_UNIFIED) ||
	     (ESDFS_I(inode)->tree != ESDFS_TREE_ANDROID &&
	      ESDFS_I(inode)->tree != ESDFS_TREE_DOWNLOAD &&
	      ESDFS_I(inode)->tree != ESDFS_TREE_ANDROID_DATA &&
	      ESDFS_I(inode)->tree != ESDFS_TREE_ANDROID_OBB &&
	      ESDFS_I(inode)->tree != ESDFS_TREE_ANDROID_MEDIA &&
	      ESDFS_I(inode)->tree != ESDFS_TREE_ANDROID_APP &&
	      ESDFS_I(inode)->tree != ESDFS_TREE_ANDROID_APP_CACHE &&
	      (ESDFS_I(inode)->tree != ESDFS_TREE_ROOT ||
	       !(mask & ESDFS_MAY_CREATE)))))
		return 0;

	pr_debug("esdfs: %s: denying access to appid: %u\n", __func__, appid);
	return -EACCES;
}

static gid_t get_type(struct esdfs_sb_info *sbi, const char *name)
{
	const char *ext = strrchr(name, '.');
	kgid_t id;

	if (ext && ext[0]) {
		ext = &ext[1];
		id = pkglist_get_ext_gid(ext);
		return gid_valid(id)?esdfs_from_kgid(sbi, id):AID_MEDIA_RW;
	}
	return AID_MEDIA_RW;
}

static kuid_t esdfs_get_derived_lower_uid(struct esdfs_sb_info *sbi,
				struct esdfs_inode_info *info)
{
	uid_t uid = sbi->lower_perms.uid;
	int perm;

	perm = info->tree;
	if (info->under_obb)
		perm = ESDFS_TREE_ANDROID_OBB;

	switch (perm) {
	case ESDFS_TREE_DOWNLOAD:
		if (test_opt(sbi, SPECIAL_DOWNLOAD))
			return make_kuid(sbi->dl_ns,
					 sbi->lower_dl_perms.raw_uid);
	case ESDFS_TREE_ROOT:
	case ESDFS_TREE_MEDIA:
	case ESDFS_TREE_ANDROID:
	case ESDFS_TREE_ANDROID_DATA:
	case ESDFS_TREE_ANDROID_MEDIA:
	case ESDFS_TREE_ANDROID_APP:
	case ESDFS_TREE_ANDROID_APP_CACHE:
		uid = derive_uid(info, uid);
		break;
	case ESDFS_TREE_ANDROID_OBB:
		uid = AID_MEDIA_OBB;
		break;
	case ESDFS_TREE_ROOT_LEGACY:
	default:
		break;
	}
	return esdfs_make_kuid(sbi, uid);
}

static kgid_t esdfs_get_derived_lower_gid(struct esdfs_sb_info *sbi,
				struct esdfs_inode_info *info, const char *name)
{
	gid_t gid = sbi->lower_perms.gid;
	uid_t upper_uid;
	int perm;

	upper_uid = esdfs_i_uid_read(&info->vfs_inode);
	perm = info->tree;
	if (info->under_obb)
		perm = ESDFS_TREE_ANDROID_OBB;

	switch (perm) {
	case ESDFS_TREE_DOWNLOAD:
		if (test_opt(sbi, SPECIAL_DOWNLOAD))
			return make_kgid(sbi->dl_ns,
					 sbi->lower_dl_perms.raw_gid);
	case ESDFS_TREE_ROOT:
	case ESDFS_TREE_MEDIA:
	case ESDFS_TREE_ANDROID:
	case ESDFS_TREE_ANDROID_DATA:
	case ESDFS_TREE_ANDROID_MEDIA:
		if (S_ISDIR(info->vfs_inode.i_mode))
			gid = derive_uid(info, AID_MEDIA_RW);
		else
			gid = derive_uid(info, get_type(sbi, name));
		break;
	case ESDFS_TREE_ANDROID_OBB:
		gid = AID_MEDIA_OBB;
		break;
	case ESDFS_TREE_ANDROID_APP:
		if (uid_is_app(upper_uid))
			gid = multiuser_get_ext_gid(upper_uid);
		else
			gid = derive_uid(info, AID_MEDIA_RW);
		break;
	case ESDFS_TREE_ANDROID_APP_CACHE:
		if (uid_is_app(upper_uid))
			gid = multiuser_get_ext_cache_gid(upper_uid);
		else
			gid = derive_uid(info, AID_MEDIA_RW);
		break;
	case ESDFS_TREE_ROOT_LEGACY:
	default:
		break;
	}
	return esdfs_make_kgid(sbi, gid);
}

void esdfs_derive_lower_ownership(struct dentry *dentry, const char *name)
{
	struct path path;
	struct inode *inode;
	struct inode *delegated_inode = NULL;
	int error;
	struct esdfs_sb_info *sbi = ESDFS_SB(dentry->d_sb);
	struct esdfs_inode_info *info = ESDFS_I(dentry->d_inode);
	kuid_t kuid;
	kgid_t kgid;
	struct iattr newattrs;

	if (!test_opt(sbi, GID_DERIVATION))
		return;

	esdfs_get_lower_path(dentry, &path);
	inode = path.dentry->d_inode;
	kuid = esdfs_get_derived_lower_uid(sbi, info);
	kgid = esdfs_get_derived_lower_gid(sbi, info, name);
	if (!gid_eq(path.dentry->d_inode->i_gid, kgid)
		|| !uid_eq(path.dentry->d_inode->i_uid, kuid)) {
retry_deleg:
		newattrs.ia_valid = ATTR_GID | ATTR_UID | ATTR_FORCE;
		newattrs.ia_uid = kuid;
		newattrs.ia_gid = kgid;
		if (!S_ISDIR(inode->i_mode))
			newattrs.ia_valid |= ATTR_KILL_SUID | ATTR_KILL_SGID
						| ATTR_KILL_PRIV;
		inode_lock(inode);
		error = security_path_chown(&path, newattrs.ia_uid,
						newattrs.ia_gid);
		if (!error)
			error = notify_change(path.dentry, &newattrs,
						&delegated_inode);
		inode_unlock(inode);
		if (delegated_inode) {
			error = break_deleg_wait(&delegated_inode);
			if (!error)
				goto retry_deleg;
		}
		if (error)
			pr_debug("esdfs: Failed to touch up lower fs gid/uid for %s\n", name);
	}
	esdfs_put_lower_path(dentry, &path);
}

/*
 * The sdcard service has a hack that creates .nomedia files along certain
 * paths to stop MediaScanner.  Create those here.
 */
int esdfs_derive_mkdir_contents(struct dentry *dir_dentry)
{
	struct esdfs_inode_info *inode_i;
	struct qstr nomedia;
	struct dentry *lower_dentry;
	struct path lower_dir_path, lower_path;
	struct dentry *lower_parent_dentry = NULL;
	umode_t mode;
	int err = 0;
	const struct cred *creds;
	int mask = 0;

	if (!dir_dentry->d_inode)
		return 0;

	inode_i = ESDFS_I(dir_dentry->d_inode);

	/*
	 * Only create .nomedia in Android/data and Android/obb, but never in
	 * pseudo link stubs.
	 */
	if ((inode_i->tree != ESDFS_TREE_ANDROID_DATA &&
	     inode_i->tree != ESDFS_TREE_ANDROID_OBB) ||
	    (ESDFS_INODE_CAN_LINK(dir_dentry->d_inode) &&
	     ESDFS_DENTRY_NEEDS_LINK(dir_dentry) &&
	     !ESDFS_DENTRY_IS_LINKED(dir_dentry)))
		return 0;

	esdfs_get_lower_path(dir_dentry, &lower_dir_path);

	nomedia.name = ".nomedia";
	nomedia.len = strlen(nomedia.name);
	nomedia.hash = full_name_hash(lower_dir_path.dentry, nomedia.name,
				      nomedia.len);

	/* check if lower has its own hash */
	if (lower_dir_path.dentry->d_flags & DCACHE_OP_HASH)
		lower_dir_path.dentry->d_op->d_hash(lower_dir_path.dentry,
							&nomedia);

	creds = esdfs_override_creds(ESDFS_SB(dir_dentry->d_sb),
					inode_i, &mask);
	/* See if the lower file is there already. */
	err = vfs_path_lookup(lower_dir_path.dentry, lower_dir_path.mnt,
			      nomedia.name, 0, &lower_path);
	if (!err)
		path_put(&lower_path);
	/* If it's there or there was an error, we're done */
	if (!err || err != -ENOENT)
		goto out;

	/* The lower file is not there.  See if the dentry is in the cache. */
	lower_dentry = d_lookup(lower_dir_path.dentry, &nomedia);
	if (!lower_dentry) {
		/* It's not there, so create a negative lower dentry. */
		lower_dentry = d_alloc(lower_dir_path.dentry, &nomedia);
		if (!lower_dentry) {
			err = -ENOMEM;
			goto out;
		}
		d_add(lower_dentry, NULL);
	}

	/* Now create the lower file. */
	mode = S_IFREG;
	lower_parent_dentry = lock_parent(lower_dentry);
	esdfs_set_lower_mode(ESDFS_SB(dir_dentry->d_sb), inode_i, &mode);
	err = vfs_create(lower_dir_path.dentry->d_inode, lower_dentry, mode,
			 true);
	unlock_dir(lower_parent_dentry);
	dput(lower_dentry);

out:
	esdfs_put_lower_path(dir_dentry, &lower_dir_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}
