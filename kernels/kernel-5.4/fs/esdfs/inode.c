/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 * Copyright (C) 2013-2014 Motorola Mobility, LLC
 * Copyright (C) 2017      Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esdfs.h"
#include <linux/fsnotify.h>

static int esdfs_create(struct inode *dir, struct dentry *dentry,
			 umode_t mode, bool want_excl)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;
	struct inode *lower_inode;
	int mask;
	const struct cred *creds;

	/*
	 * Need to recheck derived permissions unified mode to prevent certain
	 * applications from creating files at the root.
	 */
	if (test_opt(ESDFS_SB(dir->i_sb), DERIVE_UNIFIED) &&
	    esdfs_check_derived_permission(dir, ESDFS_MAY_CREATE) != 0)
		return -EACCES;

	if (test_opt(ESDFS_SB(dir->i_sb), ACCESS_DISABLE))
		return -ENOENT;

	creds = esdfs_override_creds(ESDFS_SB(dir->i_sb), ESDFS_I(dir), &mask);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	esdfs_set_lower_mode(ESDFS_SB(dir->i_sb), ESDFS_I(dir), &mode);

	lower_inode = esdfs_lower_inode(dir);
	err = vfs_create(lower_inode, lower_dentry, mode, want_excl);
	if (err)
		goto out;

	err = esdfs_interpose(dentry, dir->i_sb, &lower_path,
				ESDFS_I(dir)->userid);
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, esdfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
	esdfs_derive_lower_ownership(dentry, dentry->d_name.name);

out:
	unlock_dir(lower_parent_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}

static int esdfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode;
	struct dentry *lower_dir_dentry;
	struct path lower_path;
	const struct cred *creds;

	creds = esdfs_override_creds(ESDFS_SB(dir->i_sb), ESDFS_I(dir), NULL);
	if (!creds)
		return -ENOMEM;

	if (test_opt(ESDFS_SB(dir->i_sb), ACCESS_DISABLE)) {
		esdfs_revert_creds(creds, NULL);
		return -ENOENT;
	}

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);

	lower_dir_dentry = lock_parent(lower_dentry);

	/* d_parent might be changed in vfs_rename */
	if (lower_dir_dentry != lower_dentry->d_parent) {
		err = -ENOENT;
		goto out;
	}

	/* lower_dir_inode might be changed as well
	 * get the new inode with new lower dir dentry
	 */
	lower_dir_inode = lower_dir_dentry->d_inode;

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-renamed files.
	 * Trying to delete such files results in EBUSY from NFS
	 * below.  Silly-renamed files will get deleted by NFS later on, so
	 * we just need to detect them here and treat such EBUSY errors as
	 * if the upper file was successfully deleted.
	 */
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(dentry->d_inode,
		  esdfs_lower_inode(dentry->d_inode)->i_nlink);
	dentry->d_inode->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, NULL);
	return err;
}

static int esdfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct path lower_path;
	int mask;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(dir->i_sb),
					ESDFS_I(dir), &mask);
	if (!creds)
		return -ENOMEM;

	if (test_opt(ESDFS_SB(dir->i_sb), ACCESS_DISABLE)) {
		esdfs_revert_creds(creds, NULL);
		return -ENOENT;
	}

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	mode |= S_IFDIR;
	esdfs_set_lower_mode(ESDFS_SB(dir->i_sb), ESDFS_I(dir), &mode);
	err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry, mode);
	if (err)
		goto out;

	err = esdfs_interpose(dentry, dir->i_sb, &lower_path,
				ESDFS_I(dir)->userid);
	if (err)
		goto out;

	fsstack_copy_attr_times(dir, esdfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
	/* update number of links on parent directory */
	set_nlink(dir, esdfs_lower_inode(dir)->i_nlink);
	esdfs_derive_lower_ownership(dentry, dentry->d_name.name);

	if (ESDFS_DERIVE_PERMS(ESDFS_SB(dir->i_sb)))
		err = esdfs_derive_mkdir_contents(dentry);

out:
	unlock_dir(lower_parent_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}

static int esdfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int err;
	struct path lower_path;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(dir->i_sb),
					ESDFS_I(dir), NULL);
	if (!creds)
		return -ENOMEM;

	/* Never remove a pseudo link target.  Only the source. */
	if (ESDFS_DENTRY_HAS_STUB(dentry))
		esdfs_get_lower_stub_path(dentry, &lower_path);
	else
		esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;

	lower_dir_dentry = lock_parent(lower_dentry);

	/* d_parent might be changed in vfs_rename */
	if (lower_dir_dentry != lower_dentry->d_parent) {
		err = -ENOENT;
		goto out;
	}

	err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	if (err)
		goto out;

	d_drop(dentry);	/* drop our dentry on success (why not VFS's job?) */
	if (dentry->d_inode)
		clear_nlink(dentry->d_inode);
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);

out:
	unlock_dir(lower_dir_dentry);
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, NULL);
	return err;
}

/*
 * The locking rules in esdfs_rename are complex.  We could use a simpler
 * superblock-level name-space lock for renames and copy-ups.
 */
static int esdfs_rename(struct inode *old_dir, struct dentry *old_dentry,
			struct inode *new_dir, struct dentry *new_dentry, unsigned int flags)
{
	int err = 0;
	struct esdfs_sb_info *sbi = ESDFS_SB(old_dir->i_sb);
	struct dentry *lower_old_dentry = NULL;
	struct dentry *lower_new_dentry = NULL;
	struct dentry *lower_old_dir_dentry = NULL;
	struct dentry *lower_new_dir_dentry = NULL;
	struct dentry *trap = NULL;
	struct path lower_old_path, lower_new_path;
	int mask;
	const struct cred *creds;

	if (test_opt(sbi, SPECIAL_DOWNLOAD)) {
		if ((ESDFS_I(old_dir)->tree == ESDFS_TREE_DOWNLOAD
			|| ESDFS_I(new_dir)->tree == ESDFS_TREE_DOWNLOAD)
			&& ESDFS_I(old_dir)->tree != ESDFS_I(new_dir)->tree)
			return -EXDEV;
	}

	if (test_opt(sbi, GID_DERIVATION)) {
		if (ESDFS_I(old_dir)->userid != ESDFS_I(new_dir)->userid
			|| ((ESDFS_I(old_dir)->under_obb
			|| ESDFS_I(new_dir)->under_obb)
			&& ESDFS_I(old_dir)->under_obb
				!= ESDFS_I(new_dir)->under_obb))
			return -EXDEV;
	}
	creds = esdfs_override_creds(sbi, ESDFS_I(new_dir), &mask);
	if (!creds)
		return -ENOMEM;

	if (test_opt(ESDFS_SB(old_dir->i_sb), ACCESS_DISABLE)) {
		esdfs_revert_creds(creds, NULL);
		return -ENOENT;
	}

	/* Never rename to or from a pseudo hard link target. */
	if (ESDFS_DENTRY_HAS_STUB(old_dentry))
		esdfs_get_lower_stub_path(old_dentry, &lower_old_path);
	else
		esdfs_get_lower_path(old_dentry, &lower_old_path);
	if (ESDFS_DENTRY_HAS_STUB(new_dentry))
		esdfs_get_lower_stub_path(new_dentry, &lower_new_path);
	else
		esdfs_get_lower_path(new_dentry, &lower_new_path);
	lower_old_dentry = lower_old_path.dentry;
	lower_new_dentry = lower_new_path.dentry;
	esdfs_get_lower_parent(old_dentry, lower_old_dentry,
			       &lower_old_dir_dentry);
	esdfs_get_lower_parent(new_dentry, lower_new_dentry,
			       &lower_new_dir_dentry);

	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	/* source should not be ancestor of target */
	if (trap == lower_old_dentry) {
		err = -EINVAL;
		goto out;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		err = -ENOTEMPTY;
		goto out;
	}

	err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
			 lower_new_dir_dentry->d_inode, lower_new_dentry,
			 NULL, flags);
	if (err)
		goto out;

	esdfs_copy_attr(new_dir, lower_new_dir_dentry->d_inode);
	fsstack_copy_inode_size(new_dir, lower_new_dir_dentry->d_inode);
	if (new_dir != old_dir) {
		esdfs_copy_attr(old_dir,
				      lower_old_dir_dentry->d_inode);
		fsstack_copy_inode_size(old_dir,
					lower_old_dir_dentry->d_inode);
	}

	/* Drop any old links */
	if (ESDFS_DENTRY_HAS_STUB(old_dentry))
		d_drop(old_dentry);
	if (ESDFS_DENTRY_HAS_STUB(new_dentry))
		d_drop(new_dentry);
	esdfs_derive_lower_ownership(old_dentry, new_dentry->d_name.name);
out:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	esdfs_put_lower_parent(old_dentry, &lower_old_dir_dentry);
	esdfs_put_lower_parent(new_dentry, &lower_new_dir_dentry);
	esdfs_put_lower_path(old_dentry, &lower_old_path);
	esdfs_put_lower_path(new_dentry, &lower_new_path);
	esdfs_revert_creds(creds, &mask);
	return err;
}

static int esdfs_permission(struct inode *inode, int mask)
{
	struct inode *lower_inode;
	int err;

	/* First, check the upper permissions */
	err = generic_permission(inode, mask);

	/* Basic checking of the lower inode (can't override creds here) */
	lower_inode = esdfs_lower_inode(inode);
	if (S_ISSOCK(lower_inode->i_mode) ||
	    S_ISLNK(lower_inode->i_mode) ||
	    S_ISBLK(lower_inode->i_mode) ||
	    S_ISCHR(lower_inode->i_mode) ||
	    S_ISFIFO(lower_inode->i_mode))
		err = -EACCES;

	/* Finally, check the derived permissions */
	if (!err && ESDFS_DERIVE_PERMS(ESDFS_SB(inode->i_sb)))
		err = esdfs_check_derived_permission(inode, mask);

	return err;
}

static int esdfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err;
	loff_t oldsize;
	loff_t newsize;
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct path lower_path;
	struct iattr lower_ia;
	const struct cred *creds;

	/* We don't allow chmod or chown, so skip those */
	ia->ia_valid &= ~(ATTR_UID | ATTR_GID | ATTR_MODE);
	if (!ia->ia_valid)
		return 0;
	/* Allow touch updating timestamps. A previous permission check ensures
	 * we have write access. Changes to mode, owner, and group are ignored
	 */
	ia->ia_valid |= ATTR_FORCE;

	inode = dentry->d_inode;

	if (test_opt(ESDFS_SB(inode->i_sb), ACCESS_DISABLE))
		return -ENOENT;

	/*
	 * Check if user has permission to change inode.  We don't check if
	 * this user can change the lower inode: that should happen when
	 * calling notify_change on the lower inode.
	 */
	err = setattr_prepare(dentry, ia);
	if (err)
		return err;

	creds = esdfs_override_creds(ESDFS_SB(dentry->d_inode->i_sb),
				ESDFS_I(inode), NULL);
	if (!creds)
		return -ENOMEM;

	esdfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = esdfs_lower_inode(inode);

	/* prepare our own lower struct iattr (with the lower file) */
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE)
		lower_ia.ia_file = esdfs_lower_file(ia->ia_file);

	/*
	 * If shrinking, first truncate upper level to cancel writing dirty
	 * pages beyond the new eof; and also if its' maxbytes is more
	 * limiting (fail with -EFBIG before making any change to the lower
	 * level).  There is no need to vmtruncate the upper level
	 * afterwards in the other cases: we fsstack_copy_inode_size from
	 * the lower level.
	 */
	if (ia->ia_valid & ATTR_SIZE) {
		err = inode_newsize_ok(inode, ia->ia_size);
		if (err)
			goto out;
		/*
		 * i_size_write needs locking around it
		 * otherwise i_size_read() may spin forever
		 * (see include/linux/fs.h).
		 * similar to function fsstack_copy_inode_size
		 */
		oldsize = i_size_read(inode);
		newsize = ia->ia_size;

#if BITS_PER_LONG == 32 && defined(CONFIG_SMP)
		spin_lock(&inode->i_lock);
#endif
		i_size_write(inode, newsize);
#if BITS_PER_LONG == 32 && defined(CONFIG_SMP)
		spin_unlock(&inode->i_lock);
#endif
		if (newsize > oldsize)
			pagecache_isize_extended(inode, oldsize, newsize);
		truncate_pagecache(inode, newsize);
		esdfs_truncate_share(inode->i_sb, lower_dentry->d_inode,
					ia->ia_size);
	}

	/*
	 * mode change is for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		lower_ia.ia_valid &= ~ATTR_MODE;

	/* notify the (possibly copied-up) lower inode */
	/*
	 * Note: we use lower_dentry->d_inode, because lower_inode may be
	 * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
	 * tries to open(), unlink(), then ftruncate() a file.
	 */
	inode_lock(lower_dentry->d_inode);
	err = notify_change(lower_dentry, &lower_ia, /* note: lower_ia */
			    NULL);
	inode_unlock(lower_dentry->d_inode);
	if (err)
		goto out;

	/* get attributes from the lower inode */
	esdfs_copy_attr(inode, lower_inode);
	/*
	 * Not running fsstack_copy_inode_size(inode, lower_inode), because
	 * VFS should update our inode size, and notify_change on
	 * lower_inode should update its size.
	 */

out:
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, NULL);
	return err;
}

static int esdfs_getattr(const struct path *path, struct kstat *stat,
			u32 request_mask, unsigned int flags)
{
	int err;
	struct dentry *dentry = path->dentry;
	struct path lower_path;
	struct kstat lower_stat;
	struct inode *lower_inode;
	struct inode *inode = dentry->d_inode;
	const struct cred *creds =
			esdfs_override_creds(ESDFS_SB(inode->i_sb),
						ESDFS_I(inode), NULL);
	if (!creds)
		return -ENOMEM;

	if (test_opt(ESDFS_SB(inode->i_sb), ACCESS_DISABLE)) {
		esdfs_revert_creds(creds, NULL);
		return -ENOENT;
	}

	esdfs_get_lower_path(dentry, &lower_path);

	/* We need the lower getattr to calculate stat->blocks for us. */
	err = vfs_getattr(&lower_path, &lower_stat, request_mask, flags);
	if (err)
		goto out;

	lower_inode = esdfs_lower_inode(inode);
	esdfs_copy_attr(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);
	generic_fillattr(inode, stat);

	stat->blocks = lower_stat.blocks;

out:
	esdfs_put_lower_path(dentry, &lower_path);
	esdfs_revert_creds(creds, NULL);
	return err;
}

const struct inode_operations esdfs_symlink_iops = {
	.permission     = esdfs_permission,
	.setattr	= esdfs_setattr,
	.getattr	= esdfs_getattr,
};

const struct inode_operations esdfs_dir_iops = {
	.create		= esdfs_create,
	.lookup		= esdfs_lookup,
	.unlink		= esdfs_unlink,
	.mkdir		= esdfs_mkdir,
	.rmdir		= esdfs_rmdir,
	.rename		= esdfs_rename,
	.permission     = esdfs_permission,
	.setattr	= esdfs_setattr,
	.getattr	= esdfs_getattr,
};

const struct inode_operations esdfs_main_iops = {
	.permission     = esdfs_permission,
	.setattr	= esdfs_setattr,
	.getattr	= esdfs_getattr,
};
