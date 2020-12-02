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

#ifndef _ESDFS_H_
#define _ESDFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/iversion.h>
#include <linux/aio.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <uapi/linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs_struct.h>
#include <linux/uidgid.h>
#include <linux/user_namespace.h>
#include <linux/pkglist.h>

#include "../internal.h"

/* the file system name */
#define ESDFS_NAME "esdfs"

/* ioctl command */
#define ESDFS_IOCTL_MAGIC	'e'
#define ESDFS_IOC_DIS_ACCESS	_IO(ESDFS_IOCTL_MAGIC, 1)

/* esdfs root inode number */
#define ESDFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* mount options */
#define ESDFS_MOUNT_DERIVE_LEGACY	0x00000001
#define ESDFS_MOUNT_DERIVE_UNIFIED	0x00000002
#define ESDFS_MOUNT_DERIVE_MULTI	0x00000004
#define ESDFS_MOUNT_DERIVE_PUBLIC	0x00000008
#define ESDFS_MOUNT_DERIVE_CONFINE	0x00000010
#define ESDFS_MOUNT_ACCESS_DISABLE	0x00000020
#define ESDFS_MOUNT_GID_DERIVATION	0x00000040
#define ESDFS_MOUNT_DEFAULT_NORMAL	0x00000080
#define ESDFS_MOUNT_SPECIAL_DOWNLOAD	0x00000100

#define clear_opt(sbi, option)	(sbi->options &= ~ESDFS_MOUNT_##option)
#define set_opt(sbi, option)	(sbi->options |= ESDFS_MOUNT_##option)
#define test_opt(sbi, option)	(sbi->options & ESDFS_MOUNT_##option)

#define ESDFS_DERIVE_PERMS(sbi)	(test_opt(sbi, DERIVE_UNIFIED) || \
				 test_opt(sbi, DERIVE_LEGACY))
#define ESDFS_RESTRICT_PERMS(sbi) (ESDFS_DERIVE_PERMS(sbi) && \
				   !test_opt(sbi, DERIVE_PUBLIC) && \
				   !test_opt(sbi, DERIVE_MULTI))

/* from android_filesystem_config.h */
#define AID_ROOT             0
#define AID_SDCARD_RW     1015
#define AID_MEDIA_RW      1023
#define AID_SDCARD_R      1028
#define AID_SDCARD_PICS   1033
#define AID_SDCARD_AV     1034
#define AID_SDCARD_ALL    1035
#define AID_MEDIA_OBB     1059

/* used in extra persmission check during file creation */
#define ESDFS_MAY_CREATE	0x00001000

/* derived permissions model based on tree location */
enum {
	ESDFS_TREE_NONE = 0,		/* permissions not derived */
	ESDFS_TREE_ROOT_LEGACY,		/* root for legacy emulated storage */
	ESDFS_TREE_ROOT,		/* root for a user */
	ESDFS_TREE_MEDIA,		/* per-user basic permissions */
	ESDFS_TREE_DOWNLOAD,		/* .../Download */
	ESDFS_TREE_ANDROID,		/* .../Android */
	ESDFS_TREE_ANDROID_DATA,	/* .../Android/data */
	ESDFS_TREE_ANDROID_OBB,		/* .../Android/obb */
	ESDFS_TREE_ANDROID_MEDIA,	/* .../Android/media */
	ESDFS_TREE_ANDROID_APP,		/* .../Android/data|obb|media/... */
	ESDFS_TREE_ANDROID_APP_CACHE,	/* .../Android/data|obb|media/.../cache */
	ESDFS_TREE_ANDROID_USER,	/* .../Android/user */
};

/* for permissions table lookups */
enum {
	ESDFS_PERMS_LOWER_DEFAULT = 0,
	ESDFS_PERMS_UPPER_LEGACY,
	ESDFS_PERMS_UPPER_DERIVED,
	ESDFS_PERMS_LOWER_DOWNLOAD,
	ESDFS_PERMS_TABLE_SIZE

};

#define PKG_NAME_MAX		128
#define PKG_APPID_PER_USER	100000
#define AID_APP_START		10000 /* first app user */
#define AID_APP_END		19999 /* last app user */
#define AID_CACHE_GID_START	20000 /* start of gids for apps to mark cached data */
#define AID_EXT_GID_START	30000 /* start of gids for apps to mark external data */
#define AID_EXT_CACHE_GID_START	40000 /* start of gids for apps to mark external cached data */
#define AID_EXT_CACHE_GID_END	49999 /* end of gids for apps to mark external cached data */
#define AID_SHARED_GID_START	50000 /* start of gids for apps in each user to share */
#define PKG_APPID_MIN		1000
#define PKG_APPID_MAX		(PKG_APPID_PER_USER - 1)

/* operations vectors defined in specific files */
extern const struct file_operations esdfs_main_fops;
extern const struct file_operations esdfs_dir_fops;
extern const struct inode_operations esdfs_main_iops;
extern const struct inode_operations esdfs_dir_iops;
extern const struct inode_operations esdfs_symlink_iops;
extern const struct super_operations esdfs_sops;
extern const struct dentry_operations esdfs_dops;
extern const struct address_space_operations esdfs_aops, esdfs_dummy_aops;
extern const struct vm_operations_struct esdfs_vm_ops;

extern void esdfs_msg(struct super_block *, const char *, const char *, ...);
extern int esdfs_init_inode_cache(void);
extern void esdfs_destroy_inode_cache(void);
extern int esdfs_init_dentry_cache(void);
extern void esdfs_destroy_dentry_cache(void);
extern int esdfs_new_dentry_private_data(struct dentry *dentry);
extern void esdfs_free_dentry_private_data(struct dentry *dentry);
extern struct dentry *esdfs_lookup(struct inode *dir, struct dentry *dentry,
				   unsigned int flags);
extern struct inode *esdfs_iget(struct super_block *sb,
				struct inode *lower_inode,
				uint32_t id);
extern int esdfs_interpose(struct dentry *dentry, struct super_block *sb,
			   struct path *lower_path, uint32_t id);
extern int esdfs_init_package_list(void);
extern void esdfs_destroy_package_list(void);
extern void esdfs_derive_perms(struct dentry *dentry);
extern void esdfs_set_derived_perms(struct inode *inode);
extern int esdfs_is_dl_lookup(struct dentry *dentry, struct dentry *parent);
extern int esdfs_derived_lookup(struct dentry *dentry, struct dentry **parent);
extern int esdfs_derived_revalidate(struct dentry *dentry,
				    struct dentry *parent);
extern int esdfs_check_derived_permission(struct inode *inode, int mask);
extern int esdfs_derive_mkdir_contents(struct dentry *dentry);
extern int esdfs_lookup_nocase(struct path *lower_parent_path,
		const struct qstr *name, struct path *lower_path);

/* file private data */
struct esdfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

struct esdfs_perms {
	uid_t raw_uid;
	uid_t raw_gid;
	uid_t uid;
	gid_t gid;
	unsigned short fmask;
	unsigned short dmask;
};

/* esdfs inode data in memory */
struct esdfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
	unsigned version;	/* package list version this was derived from */
	int tree;		/* storage tree location */
	uint32_t userid;	/* Android User ID (not Linux UID) */
	uid_t appid;		/* Linux UID for this app/user combo */
	bool under_obb;
};

/* esdfs dentry data in memory */
struct esdfs_dentry_info {
	spinlock_t lock;	/* protects lower_path and lower_stub_path */
	struct path lower_path;
	struct path lower_stub_path;
	struct dentry *real_parent;
};

/* esdfs super-block data in memory */
struct esdfs_sb_info {
	struct super_block *lower_sb;
	struct super_block *s_sb;
	struct user_namespace *base_ns;
	struct list_head s_list;
	struct esdfs_perms lower_perms;
	struct esdfs_perms upper_perms;	   /* root in derived mode */
	struct dentry *obb_parent;	   /* pinned dentry for obb link parent */
	struct path dl_path;		   /* path of lower downloads folder */
	struct qstr dl_name;		   /* name of lower downloads folder */
	const char *dl_loc;		   /* location of dl folder */
	struct esdfs_perms lower_dl_perms; /* permissions for lower downloads folder */
	struct user_namespace *dl_ns;	   /* lower downloads namespace */
	int ns_fd;
	unsigned int options;
};

extern struct esdfs_perms esdfs_perms_table[ESDFS_PERMS_TABLE_SIZE];
extern unsigned esdfs_package_list_version;

void esdfs_add_super(struct esdfs_sb_info *, struct super_block *);
void esdfs_truncate_share(struct super_block *, struct inode *, loff_t newsize);

void esdfs_derive_lower_ownership(struct dentry *dentry, const char *name);

static inline bool is_obb(struct qstr *name)
{
	struct qstr q_obb = QSTR_LITERAL("obb");
	return qstr_case_eq(name, &q_obb);
}

static inline bool is_dl(struct qstr *name)
{
	struct qstr q_dl = QSTR_LITERAL("Download");

	return qstr_case_eq(name, &q_dl);
}

#define ESDFS_INODE_IS_STALE(i) ((i)->version != esdfs_package_list_version)
#define ESDFS_INODE_CAN_LINK(i) (test_opt(ESDFS_SB((i)->i_sb), \
					  DERIVE_LEGACY) || \
				 (test_opt(ESDFS_SB((i)->i_sb), \
					   DERIVE_UNIFIED) && \
				  ESDFS_I(i)->userid > 0))
#define ESDFS_DENTRY_NEEDS_LINK(d) (is_obb(&(d)->d_name))
#define ESDFS_DENTRY_NEEDS_DL_LINK(d) (is_dl(&(d)->d_name))
#define ESDFS_DENTRY_IS_LINKED(d) (ESDFS_D(d)->real_parent)
#define ESDFS_DENTRY_HAS_STUB(d) (ESDFS_D(d)->lower_stub_path.dentry)

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * esdfs_inode_info structure, ESDFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct esdfs_inode_info *ESDFS_I(const struct inode *inode)
{
	return container_of(inode, struct esdfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define ESDFS_D(dent) ((struct esdfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define ESDFS_SB(super) ((struct esdfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define ESDFS_F(file) ((struct esdfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *esdfs_lower_file(const struct file *f)
{
	return ESDFS_F(f)->lower_file;
}

static inline void esdfs_set_lower_file(struct file *f, struct file *val)
{
	ESDFS_F(f)->lower_file = val;
}

/* inode to lower inode. */
static inline struct inode *esdfs_lower_inode(const struct inode *i)
{
	return ESDFS_I(i)->lower_inode;
}

static inline void esdfs_set_lower_inode(struct inode *i, struct inode *val)
{
	ESDFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *esdfs_lower_super(
	const struct super_block *sb)
{
	return ESDFS_SB(sb)->lower_sb;
}

static inline void esdfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{
	ESDFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void esdfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(lower_path, &ESDFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&ESDFS_D(dent)->lock);
}
static inline void esdfs_get_lower_stub_path(const struct dentry *dent,
					     struct path *lower_stub_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(lower_stub_path, &ESDFS_D(dent)->lower_stub_path);
	path_get(lower_stub_path);
	spin_unlock(&ESDFS_D(dent)->lock);
}
static inline void esdfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
}
static inline void esdfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&ESDFS_D(dent)->lower_path, lower_path);
	spin_unlock(&ESDFS_D(dent)->lock);
}
static inline void esdfs_set_lower_stub_path(const struct dentry *dent,
					     struct path *lower_stub_path)
{
	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&ESDFS_D(dent)->lower_stub_path, lower_stub_path);
	spin_unlock(&ESDFS_D(dent)->lock);
}
static inline void esdfs_put_reset_lower_paths(const struct dentry *dent)
{
	struct path lower_path;
	struct path lower_stub_path = {
		.mnt = NULL,
		.dentry = NULL,
	};

	spin_lock(&ESDFS_D(dent)->lock);
	pathcpy(&lower_path, &ESDFS_D(dent)->lower_path);
	ESDFS_D(dent)->lower_path.dentry = NULL;
	ESDFS_D(dent)->lower_path.mnt = NULL;
	if (ESDFS_DENTRY_HAS_STUB(dent)) {
		pathcpy(&lower_stub_path, &ESDFS_D(dent)->lower_stub_path);
		ESDFS_D(dent)->lower_stub_path.dentry = NULL;
		ESDFS_D(dent)->lower_stub_path.mnt = NULL;
	}
	spin_unlock(&ESDFS_D(dent)->lock);

	path_put(&lower_path);
	if (lower_stub_path.dentry)
		path_put(&lower_stub_path);
}
static inline void esdfs_get_lower_parent(const struct dentry *dent,
					  struct dentry *lower_dentry,
					  struct dentry **lower_parent)
{
	*lower_parent = NULL;
	spin_lock(&ESDFS_D(dent)->lock);
	if (ESDFS_DENTRY_IS_LINKED(dent)) {
		*lower_parent = ESDFS_D(dent)->real_parent;
		dget(*lower_parent);
	}
	spin_unlock(&ESDFS_D(dent)->lock);
	if (!*lower_parent)
		*lower_parent = dget_parent(lower_dentry);
}
static inline void esdfs_put_lower_parent(const struct dentry *dent,
					  struct dentry **lower_parent)
{
	dput(*lower_parent);
}
static inline void esdfs_set_lower_parent(const struct dentry *dent,
					  struct dentry *parent)
{
	struct dentry *old_parent = NULL;

	spin_lock(&ESDFS_D(dent)->lock);
	if (ESDFS_DENTRY_IS_LINKED(dent))
		old_parent = ESDFS_D(dent)->real_parent;
	ESDFS_D(dent)->real_parent = parent;
	dget(parent);	/* pin the lower parent */
	spin_unlock(&ESDFS_D(dent)->lock);
	if (old_parent)
		dput(old_parent);
}
static inline void esdfs_release_lower_parent(const struct dentry *dent)
{
	struct dentry *real_parent = NULL;

	spin_lock(&ESDFS_D(dent)->lock);
	if (ESDFS_DENTRY_IS_LINKED(dent)) {
		real_parent = ESDFS_D(dent)->real_parent;
		ESDFS_D(dent)->real_parent = NULL;
	}
	spin_unlock(&ESDFS_D(dent)->lock);
	if (real_parent)
		dput(real_parent);
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);

	inode_lock_nested(dir->d_inode, I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	inode_unlock(dir->d_inode);
	dput(dir);
}

static inline void esdfs_set_lower_mode(struct esdfs_sb_info *sbi,
		struct esdfs_inode_info *inode_i, umode_t *mode)
{
	struct esdfs_perms *perms = &sbi->lower_perms;

	if (test_opt(sbi, SPECIAL_DOWNLOAD) &&
			inode_i->tree == ESDFS_TREE_DOWNLOAD)
		perms = &sbi->lower_dl_perms;

	if (S_ISDIR(*mode))
		*mode = (*mode & S_IFMT) | perms->dmask;
	else
		*mode = (*mode & S_IFMT) | perms->fmask;
}

static inline void esdfs_set_perms(struct inode *inode)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(inode->i_sb);

	if (ESDFS_DERIVE_PERMS(sbi)) {
		esdfs_set_derived_perms(inode);
		return;
	}
	i_uid_write(inode, sbi->upper_perms.uid);
	i_gid_write(inode, sbi->upper_perms.gid);
	if (S_ISDIR(inode->i_mode))
		inode->i_mode = (inode->i_mode & S_IFMT) |
				sbi->upper_perms.dmask;
	else
		inode->i_mode = (inode->i_mode & S_IFMT) |
				sbi->upper_perms.fmask;
}

static inline void esdfs_revalidate_perms(struct dentry *dentry)
{
	if (ESDFS_DERIVE_PERMS(ESDFS_SB(dentry->d_sb)) &&
	    dentry->d_inode &&
	    ESDFS_INODE_IS_STALE(ESDFS_I(dentry->d_inode))) {
		esdfs_derive_perms(dentry);
		esdfs_set_perms(dentry->d_inode);
	}
}

static inline uid_t derive_uid(struct esdfs_inode_info *inode_i, uid_t uid)
{
	return inode_i->userid * PKG_APPID_PER_USER +
	       (uid % PKG_APPID_PER_USER);
}

static inline bool uid_is_app(uid_t uid)
{
	uid_t appid = uid % PKG_APPID_PER_USER;

	return appid >= AID_APP_START && appid <= AID_APP_END;
}

static inline gid_t multiuser_get_ext_cache_gid(uid_t uid)
{
	return uid - AID_APP_START + AID_EXT_CACHE_GID_START;
}

static inline gid_t multiuser_get_ext_gid(uid_t uid)
{
	return uid - AID_APP_START + AID_EXT_GID_START;
}

/* file attribute helpers */
static inline void esdfs_copy_lower_attr(struct inode *dest,
					 const struct inode *src)
{
	dest->i_mode = src->i_mode & S_IFMT;
	dest->i_rdev = src->i_rdev;
	dest->i_atime = src->i_atime;
	dest->i_mtime = src->i_mtime;
	dest->i_ctime = src->i_ctime;
	dest->i_blkbits = src->i_blkbits;
	dest->i_flags = src->i_flags;
	set_nlink(dest, src->i_nlink);
}

static inline void esdfs_copy_attr(struct inode *dest, const struct inode *src)
{
	esdfs_copy_lower_attr(dest, src);
	esdfs_set_perms(dest);
}

static inline uid_t esdfs_from_local_uid(struct esdfs_sb_info *sbi, uid_t uid)
{
	return from_kuid(sbi->base_ns, make_kuid(current_user_ns(), uid));
}

static inline gid_t esdfs_from_local_gid(struct esdfs_sb_info *sbi, gid_t gid)
{
	return from_kgid(sbi->base_ns, make_kgid(current_user_ns(), gid));
}

static inline uid_t esdfs_from_kuid(struct esdfs_sb_info *sbi, kuid_t uid)
{
	return from_kuid(sbi->base_ns, uid);
}

static inline gid_t esdfs_from_kgid(struct esdfs_sb_info *sbi, kgid_t gid)
{
	return from_kgid(sbi->base_ns, gid);
}

static inline kuid_t esdfs_make_kuid(struct esdfs_sb_info *sbi, uid_t uid)
{
	return make_kuid(sbi->base_ns, uid);
}

static inline kgid_t esdfs_make_kgid(struct esdfs_sb_info *sbi, gid_t gid)
{
	return make_kgid(sbi->base_ns, gid);
}

/* Helper functions to read and write to inode uid/gids without
 * having to worry about translating into/out of esdfs's preferred
 * base user namespace.
 */
static inline uid_t esdfs_i_uid_read(const struct inode *inode)
{
	return esdfs_from_kuid(ESDFS_SB(inode->i_sb), inode->i_uid);
}

static inline gid_t esdfs_i_gid_read(const struct inode *inode)
{
	return esdfs_from_kgid(ESDFS_SB(inode->i_sb), inode->i_gid);
}

static inline void esdfs_i_uid_write(struct inode *inode, uid_t uid)
{
	inode->i_uid = esdfs_make_kuid(ESDFS_SB(inode->i_sb), uid);
}

static inline void esdfs_i_gid_write(struct inode *inode, gid_t gid)
{
	inode->i_gid = esdfs_make_kgid(ESDFS_SB(inode->i_sb), gid);
}

/*
 * Based on nfs4_save_creds() and nfs4_reset_creds() in nfsd/nfs4recover.c.
 * Returns NULL if prepare_creds() could not allocate heap, otherwise
 */
static inline const struct cred *esdfs_override_creds(
		struct esdfs_sb_info *sbi,
		struct esdfs_inode_info *info, int *mask)
{
	struct cred *creds = prepare_creds();
	uid_t uid;
	gid_t gid = sbi->lower_perms.gid;

	if (!creds)
		return NULL;

	/* clear the umask so that the lower mode works for create cases */
	if (mask) {
		*mask = 0;
		*mask = xchg(&current->fs->umask, *mask & S_IRWXUGO);
	}

	if (test_opt(sbi, SPECIAL_DOWNLOAD) &&
			info->tree == ESDFS_TREE_DOWNLOAD) {
		creds->fsuid = make_kuid(sbi->dl_ns,
					 sbi->lower_dl_perms.raw_uid);
		creds->fsgid = make_kgid(sbi->dl_ns,
					 sbi->lower_dl_perms.raw_gid);
	} else {
		if (test_opt(sbi, GID_DERIVATION)) {
			if (info->under_obb)
				uid = AID_MEDIA_OBB;
			else
				uid = derive_uid(info, sbi->lower_perms.uid);
		} else {
			uid = sbi->lower_perms.uid;
		}
		creds->fsuid = esdfs_make_kuid(sbi, uid);
		creds->fsgid = esdfs_make_kgid(sbi, gid);
	}

	/* this installs the new creds into current, which we must destroy */
	return override_creds(creds);
}

static inline void esdfs_revert_creds(const struct cred *creds, int *mask)
{
	const struct cred *current_creds = current->cred;

	/* restore the old umask */
	if (mask)
		*mask = xchg(&current->fs->umask, *mask & S_IRWXUGO);

	/* restore the old creds into current */
	revert_creds(creds);
	put_cred(current_creds);	/* destroy the old temporary creds */
}

#endif	/* not _ESDFS_H_ */
