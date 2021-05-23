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

/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */
static struct kmem_cache *esdfs_inode_cachep;
static LIST_HEAD(esdfs_list);
static DEFINE_SPINLOCK(esdfs_list_lock);

void esdfs_msg(struct super_block *sb, const char *level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	printk("%sESDFS-fs (%s): %pV", level, sb->s_id, &vaf);
	va_end(args);
}

void esdfs_add_super(struct esdfs_sb_info *sbi, struct super_block *sb)
{
	sbi->s_sb = sb;

	spin_lock(&esdfs_list_lock);
	list_add_tail(&sbi->s_list, &esdfs_list);
	spin_unlock(&esdfs_list_lock);
}

static void esdfs_remove_super(struct esdfs_sb_info *sbi)
{
	spin_lock(&esdfs_list_lock);
	list_del(&sbi->s_list);
	spin_unlock(&esdfs_list_lock);
}

void esdfs_truncate_share(struct super_block *sb, struct inode *lower_inode,
				loff_t newsize)
{
	struct list_head *p;
	struct esdfs_sb_info *sbi;
	struct super_block *lower_sb = lower_inode->i_sb;
	struct inode *inode;

	spin_lock(&esdfs_list_lock);
	p = esdfs_list.next;
	while (p != &esdfs_list) {
		sbi = list_entry(p, struct esdfs_sb_info, s_list);
		if (sbi->s_sb == sb || sbi->lower_sb != lower_sb) {
			p = p->next;
			continue;
		}
		spin_unlock(&esdfs_list_lock);
		inode = ilookup(sbi->s_sb, lower_inode->i_ino);
		if (inode) {
			truncate_setsize(inode, newsize);
			iput(inode);
		}
		spin_lock(&esdfs_list_lock);
		p = p->next;
	}
	spin_unlock(&esdfs_list_lock);
}

/* final actions when unmounting a file system */
static void esdfs_put_super(struct super_block *sb)
{
	struct esdfs_sb_info *spd;
	struct super_block *s;

	spd = ESDFS_SB(sb);
	if (!spd)
		return;

	/* decrement lower super references */
	s = esdfs_lower_super(sb);
	esdfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);

	esdfs_remove_super(spd);

	kfree(spd);
	sb->s_fs_info = NULL;
}

static int esdfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	int err;
	struct path lower_path;
	struct inode *inode = dentry->d_inode;

	if (test_opt(ESDFS_SB(inode->i_sb), ACCESS_DISABLE))
		return -ENOENT;

	esdfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	esdfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = ESDFS_SUPER_MAGIC;

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int esdfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	int err = 0;

	/*
	 * The VFS will take care of "ro" and "rw" flags among others.  We
	 * can safely accept a few flags (RDONLY, MANDLOCK), and honor
	 * SILENT, but anything else left over is an error.
	 */
	if ((*flags & ~(MS_RDONLY | MS_MANDLOCK | MS_SILENT)) != 0) {
		esdfs_msg(sb, KERN_ERR, "remount flags 0x%x unsupported\n",
			*flags);
		err = -EINVAL;
	}

	return err;
}

/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void esdfs_evict_inode(struct inode *inode)
{
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	/*
	 * Decrement a reference to a lower_inode, which was incremented
	 * by our read_inode when it was created initially.
	 */
	lower_inode = esdfs_lower_inode(inode);
	esdfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

static struct inode *esdfs_alloc_inode(struct super_block *sb)
{
	struct esdfs_inode_info *i;

	i = kmem_cache_alloc(esdfs_inode_cachep, GFP_KERNEL);
	if (!i)
		return NULL;

	/* memset everything up to the inode to 0 */
	memset(i, 0, offsetof(struct esdfs_inode_info, vfs_inode));

	i->vfs_inode.i_version = 1;
	return &i->vfs_inode;
}

static void i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);

	kmem_cache_free(esdfs_inode_cachep, ESDFS_I(inode));
}

static void esdfs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, i_callback);
}

/* esdfs inode cache constructor */
static void init_once(void *obj)
{
	struct esdfs_inode_info *i = obj;

	inode_init_once(&i->vfs_inode);
}

int esdfs_init_inode_cache(void)
{
	int err = 0;

	esdfs_inode_cachep =
		kmem_cache_create("esdfs_inode_cache",
				  sizeof(struct esdfs_inode_info), 0,
				  SLAB_RECLAIM_ACCOUNT, init_once);
	if (!esdfs_inode_cachep)
		err = -ENOMEM;
	return err;
}

/* esdfs inode cache destructor */
void esdfs_destroy_inode_cache(void)
{
	if (esdfs_inode_cachep)
		kmem_cache_destroy(esdfs_inode_cachep);
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void esdfs_umount_begin(struct super_block *sb)
{
	struct super_block *lower_sb;

	lower_sb = esdfs_lower_super(sb);
	if (lower_sb && lower_sb->s_op && lower_sb->s_op->umount_begin)
		lower_sb->s_op->umount_begin(lower_sb);
}

static int esdfs_show_options(struct seq_file *seq, struct dentry *root)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(root->d_sb);

	if (memcmp(&sbi->lower_perms,
		   &esdfs_perms_table[ESDFS_PERMS_LOWER_DEFAULT],
		   sizeof(struct esdfs_perms)))
		seq_printf(seq, ",lower=%u:%u:%ho:%ho",
				sbi->lower_perms.raw_uid,
				sbi->lower_perms.raw_gid,
				sbi->lower_perms.fmask,
				sbi->lower_perms.dmask);

	if (memcmp(&sbi->upper_perms,
		   &esdfs_perms_table[ESDFS_PERMS_UPPER_LEGACY],
		   sizeof(struct esdfs_perms)))
		seq_printf(seq, ",upper=%u:%u:%ho:%ho",
				sbi->upper_perms.raw_uid,
				sbi->upper_perms.raw_gid,
				sbi->upper_perms.fmask,
				sbi->upper_perms.dmask);

	if (test_opt(sbi, DERIVE_PUBLIC))
		seq_puts(seq, ",derive=public");
	else if (test_opt(sbi, DERIVE_MULTI))
		seq_puts(seq, ",derive=multi");
	else if (test_opt(sbi, DERIVE_UNIFIED))
		seq_puts(seq, ",derive=unified");
	else if (test_opt(sbi, DERIVE_LEGACY))
		seq_puts(seq, ",derive=legacy");
	else
		seq_puts(seq, ",derive=none");

	if (test_opt(sbi, DERIVE_CONFINE))
		seq_puts(seq, ",confine");
	else
		seq_puts(seq, ",noconfine");
	if (test_opt(sbi, GID_DERIVATION))
		seq_puts(seq, ",derive_gid");
	if (test_opt(sbi, DEFAULT_NORMAL))
		seq_puts(seq, ",default_normal");
	if (test_opt(sbi, SPECIAL_DOWNLOAD)) {
		seq_printf(seq, ",dl_loc=%s", sbi->dl_loc);
		seq_printf(seq, ",dl_uid=%d", sbi->lower_dl_perms.raw_uid);
		seq_printf(seq, ",dl_gid=%d", sbi->lower_dl_perms.raw_gid);
	}
	return 0;
}

const struct super_operations esdfs_sops = {
	.put_super	= esdfs_put_super,
	.statfs		= esdfs_statfs,
	.remount_fs	= esdfs_remount_fs,
	.evict_inode	= esdfs_evict_inode,
	.umount_begin	= esdfs_umount_begin,
	.show_options	= esdfs_show_options,
	.alloc_inode	= esdfs_alloc_inode,
	.destroy_inode	= esdfs_destroy_inode,
	.drop_inode	= generic_delete_inode,
};
