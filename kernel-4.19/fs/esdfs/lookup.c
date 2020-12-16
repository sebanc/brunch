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

struct esdfs_name_data {
	struct dir_context ctx;
	const struct qstr *to_find;
	char match_name[NAME_MAX+1];
	bool found;
};

static int esdfs_name_match(struct dir_context *ctx, const char *name, int namelen,
		loff_t offset, u64 ino, unsigned int d_type)
{
	struct esdfs_name_data *buf = container_of(ctx, struct esdfs_name_data, ctx);
	struct qstr candidate = QSTR_INIT(name, namelen);

	if (qstr_case_eq(buf->to_find, &candidate)) {
		memcpy(buf->match_name, name, namelen);
		buf->match_name[namelen] = 0;
		buf->found = true;
		return 1;
	}
	return 0;
}

int esdfs_lookup_nocase(struct path *parent,
		const struct qstr *name,
		struct path *path) {
	int err = 0;
	/* Use vfs_path_lookup to check if the dentry exists or not */
	err = vfs_path_lookup(parent->dentry, parent->mnt, name->name, 0, path);
	/* check for other cases */
	if (err == -ENOENT) {
		struct file *file;
		const struct cred *cred = current_cred();

		struct esdfs_name_data buffer = {
			.ctx.actor = esdfs_name_match,
			.to_find = name,
			.found = false,
		};

		file = dentry_open(parent, O_RDONLY | O_DIRECTORY, cred);
		if (IS_ERR(file))
			return PTR_ERR(file);
		err = iterate_dir(file, &buffer.ctx);
		fput(file);
		if (err)
			return err;

		if (buffer.found)
			err = vfs_path_lookup(parent->dentry, parent->mnt,
						buffer.match_name, 0, path);
		else
			err = -ENOENT;
	}
	return err;
}

struct esdfs_ci_getdents_callback {
	struct dir_context ctx;
	const char *name;
	char match_name[NAME_MAX+1];
	int found; /*-1: not found, 0: found*/
	int count;
};

/* The dentry cache is just so we have properly sized dentries */
static struct kmem_cache *esdfs_dentry_cachep;

int esdfs_init_dentry_cache(void)
{
	esdfs_dentry_cachep =
		kmem_cache_create("esdfs_dentry",
				  sizeof(struct esdfs_dentry_info),
				  0, SLAB_RECLAIM_ACCOUNT, NULL);

	return esdfs_dentry_cachep ? 0 : -ENOMEM;
}

void esdfs_destroy_dentry_cache(void)
{
	if (esdfs_dentry_cachep)
		kmem_cache_destroy(esdfs_dentry_cachep);
}

void esdfs_free_dentry_private_data(struct dentry *dentry)
{
	kmem_cache_free(esdfs_dentry_cachep, dentry->d_fsdata);
	dentry->d_fsdata = NULL;
}

/* allocate new dentry private data */
int esdfs_new_dentry_private_data(struct dentry *dentry)
{
	struct esdfs_dentry_info *info = ESDFS_D(dentry);

	/* use zalloc to init dentry_info.lower_path */
	info = kmem_cache_zalloc(esdfs_dentry_cachep, GFP_ATOMIC);
	if (!info)
		return -ENOMEM;

	spin_lock_init(&info->lock);
	dentry->d_fsdata = info;

	return 0;
}

struct inode_data {
	struct inode *lower_inode;
	uint32_t id;
};

/* Multiple obb files can point to the same lower file */
static int esdfs_inode_test(struct inode *inode, void *candidate_data)
{
	struct inode *current_lower_inode = esdfs_lower_inode(inode);
	uint32_t current_userid = ESDFS_I(inode)->userid;
	struct inode_data *data = (struct inode_data *)candidate_data;

	if (current_lower_inode == data->lower_inode
			&& current_userid == data->id)
		return 1; /* found a match */
	else
		return 0; /* no match */
}

static int esdfs_inode_set(struct inode *inode, void *lower_inode)
{
	/* we do actual inode initialization in esdfs_iget */
	return 0;
}

struct inode *esdfs_iget(struct super_block *sb, struct inode *lower_inode,
						uint32_t id)
{
	struct esdfs_inode_info *info;
	struct inode_data data;
	struct inode *inode; /* the new inode to return */

	if (!igrab(lower_inode))
		return ERR_PTR(-ESTALE);
	data.id = id;
	data.lower_inode = lower_inode;
	inode = iget5_locked(sb, /* our superblock */
			     /*
			      * hashval: we use inode number, but we can
			      * also use "(unsigned long)lower_inode"
			      * instead.
			      */
			     lower_inode->i_ino, /* hashval */
			     esdfs_inode_test,	/* inode comparison function */
			     esdfs_inode_set, /* inode init function */
			     &data); /* data passed to test+set fxns */
	if (!inode) {
		iput(lower_inode);
		return ERR_PTR(-ENOMEM);
	}
	/* if found a cached inode, then just return it (after iput) */
	if (!(inode->i_state & I_NEW)) {
		iput(lower_inode);
		return inode;
	}

	/* initialize new inode */
	info = ESDFS_I(inode);
	info->tree = ESDFS_TREE_NONE;
	info->userid = 0;
	info->appid = 0;
	info->under_obb = false;

	inode->i_ino = lower_inode->i_ino;
	esdfs_set_lower_inode(inode, lower_inode);

	inode_inc_iversion(inode);

	/* use different set of inode ops for symlinks & directories */
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_op = &esdfs_dir_iops;
	else if (S_ISLNK(lower_inode->i_mode))
		inode->i_op = &esdfs_symlink_iops;
	else
		inode->i_op = &esdfs_main_iops;

	/* use different set of file ops for directories */
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_fop = &esdfs_dir_fops;
	else
		inode->i_fop = &esdfs_main_fops;

	inode->i_mapping->a_ops = &esdfs_aops;

	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;

	/* properly initialize special inodes */
	if (S_ISBLK(lower_inode->i_mode) || S_ISCHR(lower_inode->i_mode) ||
	    S_ISFIFO(lower_inode->i_mode) || S_ISSOCK(lower_inode->i_mode))
		init_special_inode(inode, lower_inode->i_mode,
				   lower_inode->i_rdev);

	/* all well, copy inode attributes */
	esdfs_copy_lower_attr(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);

	unlock_new_inode(inode);
	return inode;
}

/*
 * Helper interpose routine, called directly by ->lookup to handle
 * spliced dentries
 */
static struct dentry *__esdfs_interpose(struct dentry *dentry,
					struct super_block *sb,
					struct path *lower_path,
					uint32_t id)
{
	struct inode *inode;
	struct inode *lower_inode;
	struct super_block *lower_sb;
	struct dentry *ret_dentry;

	lower_inode = lower_path->dentry->d_inode;
	lower_sb = esdfs_lower_super(sb);

	/* check that the lower file system didn't cross a mount point */
	if (lower_inode->i_sb != lower_sb) {
		ret_dentry = ERR_PTR(-EXDEV);
		goto out;
	}

	/*
	 * We allocate our new inode below by calling esdfs_iget,
	 * which will initialize some of the new inode's fields
	 */

	/* inherit lower inode number for esdfs's inode */
	inode = esdfs_iget(sb, lower_inode, id);
	if (IS_ERR(inode)) {
		ret_dentry = ERR_CAST(inode);
		goto out;
	}

	ret_dentry = d_splice_alias(inode, dentry);
	dentry = ret_dentry ?: dentry;
	if (IS_ERR(dentry))
		goto out;

	if (ESDFS_DERIVE_PERMS(ESDFS_SB(sb)))
		esdfs_derive_perms(dentry);
	esdfs_set_perms(inode);
out:
	return ret_dentry;
}

/*
 * Connect an esdfs inode dentry/inode with several lower ones.  This is
 * the classic stackable file system "vnode interposition" action.
 *
 * @dentry: esdfs's dentry which interposes on lower one
 * @sb: esdfs's super_block
 * @lower_path: the lower path (caller does path_get/put)
 */
int esdfs_interpose(struct dentry *dentry, struct super_block *sb,
		     struct path *lower_path, uint32_t id)
{
	struct dentry *ret_dentry;

	ret_dentry = __esdfs_interpose(dentry, sb, lower_path, id);
	return PTR_ERR(ret_dentry);
}

/*
 * Main driver function for esdfs's lookup.
 *
 * Returns: NULL (ok), ERR_PTR if an error occurred.
 * Fills in lower_parent_path with <dentry,mnt> on success.
 */
static struct dentry *__esdfs_lookup(struct dentry *dentry,
				     unsigned int flags,
				     struct path *lower_parent_path,
				     uint32_t id, bool use_dl)
{
	int err = 0;
	struct vfsmount *lower_dir_mnt;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *lower_dentry;
	const char *name;
	struct path lower_path;
	struct qstr dname;
	struct dentry *ret_dentry = NULL;

	/* must initialize dentry operations */
	d_set_d_op(dentry, &esdfs_dops);

	if (IS_ROOT(dentry))
		goto out;

	if (use_dl)
		name = ESDFS_SB(dentry->d_sb)->dl_name.name;
	else
		name = dentry->d_name.name;

	dname.name = name;
	dname.len = strlen(name);

	/* now start the actual lookup procedure */
	lower_dir_dentry = lower_parent_path->dentry;
	lower_dir_mnt = lower_parent_path->mnt;

	/* if the access is to the Download directory, redirect
	 * to lower path.
	 */
	if (use_dl) {
		pathcpy(&lower_path, &ESDFS_SB(dentry->d_sb)->dl_path);
		path_get(&ESDFS_SB(dentry->d_sb)->dl_path);
	} else {
		err = esdfs_lookup_nocase(lower_parent_path, &dname,
					  &lower_path);
	}

	/* no error: handle positive dentries */
	if (!err) {
		esdfs_set_lower_path(dentry, &lower_path);
		ret_dentry =
			__esdfs_interpose(dentry, dentry->d_sb,
						&lower_path, id);
		if (IS_ERR(ret_dentry)) {
			err = PTR_ERR(ret_dentry);
			/* path_put underlying underlying path on error */
			esdfs_put_reset_lower_paths(dentry);
		}
		goto out;
	}

	/*
	 * We don't consider ENOENT an error, and we want to return a
	 * negative dentry.
	 */
	if (err && err != -ENOENT)
		goto out;

	/* instatiate a new negative dentry */
	/* See if the low-level filesystem might want
	 * to use its own hash */
	lower_dentry = d_hash_and_lookup(lower_dir_dentry, &dname);
	if (IS_ERR(lower_dentry))
		return lower_dentry;

	if (!lower_dentry) {
		/* We called vfs_path_lookup earlier, and did not get a negative
		 * dentry then. Don't confuse the lower filesystem by forcing
		 * one on it now...
		 */
		err = -ENOENT;
		goto out;
	}

	lower_path.dentry = lower_dentry;
	lower_path.mnt = mntget(lower_dir_mnt);
	esdfs_set_lower_path(dentry, &lower_path);

	/*
	 * If the intent is to create a file, then don't return an error, so
	 * the VFS will continue the process of making this negative dentry
	 * into a positive one.
	 */
	if (flags & (LOOKUP_CREATE|LOOKUP_RENAME_TARGET))
		err = 0;

out:
	if (err)
		return ERR_PTR(err);
	return ret_dentry;
}

struct dentry *esdfs_lookup(struct inode *dir, struct dentry *dentry,
			    unsigned int flags)
{
	int err;
	struct dentry *ret, *real_parent, *parent;
	struct path lower_parent_path, old_lower_parent_path;
	const struct cred *creds;
	struct esdfs_sb_info *sbi = ESDFS_SB(dir->i_sb);
	int use_dl;

	parent = real_parent = dget_parent(dentry);

	/* allocate dentry private data.  We free it in ->d_release */
	err = esdfs_new_dentry_private_data(dentry);
	if (err) {
		ret = ERR_PTR(err);
		goto out;
	}

	if (ESDFS_DERIVE_PERMS(sbi)) {
		err = esdfs_derived_lookup(dentry, &parent);
		if (err) {
			ret = ERR_PTR(err);
			goto out;
		}
	}

	esdfs_get_lower_path(parent, &lower_parent_path);

	creds =	esdfs_override_creds(ESDFS_SB(dir->i_sb),
			ESDFS_I(d_inode(parent)), NULL);
	if (!creds) {
		ret = ERR_PTR(-EINVAL);
		goto out_put;
	}

	/* Check if the lookup corresponds to the Download directory */
	use_dl = esdfs_is_dl_lookup(dentry, parent);

	ret = __esdfs_lookup(dentry, flags, &lower_parent_path,
					ESDFS_I(dir)->userid,
					use_dl);
	if (IS_ERR(ret))
		goto out_cred;
	if (ret)
		dentry = ret;
	if (dentry->d_inode) {
		fsstack_copy_attr_times(dentry->d_inode,
					esdfs_lower_inode(dentry->d_inode));
		/*
		 * Do not modify the ownership of the lower directory if it
		 * is the Download directory
		 */
		if (!use_dl)
			esdfs_derive_lower_ownership(dentry,
						     dentry->d_name.name);
	}
	/* update parent directory's atime */
	fsstack_copy_attr_atime(parent->d_inode,
				esdfs_lower_inode(parent->d_inode));

	/*
	 * If this is a pseudo hard link, store the real parent and ensure
	 * that the link target directory contains any derived contents.
	 */
	if (parent != real_parent) {
		esdfs_get_lower_path(real_parent, &old_lower_parent_path);
		esdfs_set_lower_parent(dentry, old_lower_parent_path.dentry);
		esdfs_put_lower_path(real_parent, &old_lower_parent_path);
		esdfs_derive_mkdir_contents(dentry);
	}
out_cred:
	esdfs_revert_creds(creds, NULL);
out_put:
	esdfs_put_lower_path(parent, &lower_parent_path);
out:
	dput(parent);
	if (parent != real_parent)
		dput(real_parent);
	return ret;
}
