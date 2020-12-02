/*
 * Linux Security Module for Chromium OS
 *
 * Copyright 2016 Google Inc. All Rights Reserved
 *
 * Authors:
 *      Mattias Nissler <mnissler@chromium.org>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/fsnotify_backend.h>
#include <linux/hash.h>
#include <linux/mutex.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "inode_mark.h"

/*
 * This file implements facilities to pin inodes in core and attach some
 * meta data to them. We use fsnotify inode marks as a vehicle to attach the
 * meta data.
 */
struct chromiumos_inode_mark {
	struct fsnotify_mark mark;
	struct inode *inode;
	enum chromiumos_inode_security_policy
		policies[CHROMIUMOS_NUMBER_OF_POLICIES];
};

static inline struct chromiumos_inode_mark *
chromiumos_to_inode_mark(struct fsnotify_mark *mark)
{
	return container_of(mark, struct chromiumos_inode_mark, mark);
}

/*
 * Hashtable entry that contains tracking information specific to the file
 * system identified by the corresponding super_block. This contains the
 * fsnotify group that holds all the marks for inodes belonging to the
 * super_block.
 */
struct chromiumos_super_block_mark {
	atomic_t refcnt;
	struct hlist_node node;
	struct super_block *sb;
	struct fsnotify_group *fsn_group;
};

#define CHROMIUMOS_SUPER_BLOCK_HASH_BITS 8
#define CHROMIUMOS_SUPER_BLOCK_HASH_SIZE (1 << CHROMIUMOS_SUPER_BLOCK_HASH_BITS)

static struct hlist_head chromiumos_super_block_hash_table
	[CHROMIUMOS_SUPER_BLOCK_HASH_SIZE] __read_mostly;
static DEFINE_MUTEX(chromiumos_super_block_hash_lock);

static struct hlist_head *chromiumos_super_block_hlist(struct super_block *sb)
{
	return &chromiumos_super_block_hash_table[hash_ptr(
		sb, CHROMIUMOS_SUPER_BLOCK_HASH_BITS)];
}

static void chromiumos_super_block_put(struct chromiumos_super_block_mark *sbm)
{
	if (atomic_dec_and_test(&sbm->refcnt)) {
		mutex_lock(&chromiumos_super_block_hash_lock);
		hlist_del_rcu(&sbm->node);
		mutex_unlock(&chromiumos_super_block_hash_lock);

		synchronize_rcu();

		fsnotify_destroy_group(sbm->fsn_group);
		kfree(sbm);
	}
}

static struct chromiumos_super_block_mark *
chromiumos_super_block_lookup(struct super_block *sb)
{
	struct hlist_head *hlist = chromiumos_super_block_hlist(sb);
	struct chromiumos_super_block_mark *sbm;
	struct chromiumos_super_block_mark *matching_sbm = NULL;

	rcu_read_lock();
	hlist_for_each_entry_rcu(sbm, hlist, node) {
		if (sbm->sb == sb && atomic_inc_not_zero(&sbm->refcnt)) {
			matching_sbm = sbm;
			break;
		}
	}
	rcu_read_unlock();

	return matching_sbm;
}

static int chromiumos_handle_fsnotify_event(struct fsnotify_group *group,
					    struct inode *inode,
					    u32 mask, const void *data,
					    int data_type,
					    const struct qstr *file_name,
					    u32 cookie,
					    struct fsnotify_iter_info *iter_info)
{
	/*
	 * This should never get called because a zero mask is set on the inode
	 * marks. All cases of marks going away (inode deletion, unmount,
	 * explicit removal) are handled in chromiumos_freeing_mark.
	 */
	WARN_ON_ONCE(1);
	return 0;
}

static void chromiumos_freeing_mark(struct fsnotify_mark *mark,
				    struct fsnotify_group *group)
{
	struct chromiumos_inode_mark *inode_mark =
		chromiumos_to_inode_mark(mark);

	iput(inode_mark->inode);
	inode_mark->inode = NULL;
	chromiumos_super_block_put(group->private);
}

static void chromiumos_free_mark(struct fsnotify_mark *mark)
{
	iput(chromiumos_to_inode_mark(mark)->inode);
	kfree(mark);
}

static const struct fsnotify_ops chromiumos_fsn_ops = {
	.handle_event = chromiumos_handle_fsnotify_event,
	.freeing_mark = chromiumos_freeing_mark,
	.free_mark = chromiumos_free_mark,
};

static struct chromiumos_super_block_mark *
chromiumos_super_block_create(struct super_block *sb)
{
	struct hlist_head *hlist = chromiumos_super_block_hlist(sb);
	struct chromiumos_super_block_mark *sbm = NULL;

	WARN_ON(!mutex_is_locked(&chromiumos_super_block_hash_lock));

	/* No match found, create a new entry. */
	sbm = kzalloc(sizeof(*sbm), GFP_KERNEL);
	if (!sbm)
		return ERR_PTR(-ENOMEM);

	atomic_set(&sbm->refcnt, 1);
	sbm->sb = sb;
	sbm->fsn_group = fsnotify_alloc_group(&chromiumos_fsn_ops);
	if (IS_ERR(sbm->fsn_group)) {
		int ret = PTR_ERR(sbm->fsn_group);

		kfree(sbm);
		return ERR_PTR(ret);
	}
	sbm->fsn_group->private = sbm;
	hlist_add_head_rcu(&sbm->node, hlist);

	return sbm;
}

static struct chromiumos_super_block_mark *
chromiumos_super_block_get(struct super_block *sb)
{
	struct chromiumos_super_block_mark *sbm;

	mutex_lock(&chromiumos_super_block_hash_lock);
	sbm = chromiumos_super_block_lookup(sb);
	if (!sbm)
		sbm = chromiumos_super_block_create(sb);

	mutex_unlock(&chromiumos_super_block_hash_lock);
	return sbm;
}

/*
 * This will only ever get called if the metadata does not already exist for
 * an inode, so no need to worry about freeing an existing mark.
 */
static int
chromiumos_inode_mark_create(
	struct chromiumos_super_block_mark *sbm,
	struct inode *inode,
	enum chromiumos_inode_security_policy_type type,
	enum chromiumos_inode_security_policy policy)
{
	struct chromiumos_inode_mark *inode_mark;
	int ret;
	size_t i;

	WARN_ON(!mutex_is_locked(&sbm->fsn_group->mark_mutex));

	inode_mark = kzalloc(sizeof(*inode_mark), GFP_KERNEL);
	if (!inode_mark)
		return -ENOMEM;

	fsnotify_init_mark(&inode_mark->mark, sbm->fsn_group);
	inode_mark->inode = igrab(inode);
	if (!inode_mark->inode) {
		ret = -ENOENT;
		goto out;
	}

	/* Initialize all policies to inherit. */
	for (i = 0; i < CHROMIUMOS_NUMBER_OF_POLICIES; i++)
		inode_mark->policies[i] = CHROMIUMOS_INODE_POLICY_INHERIT;

	inode_mark->policies[type] = policy;
	ret = fsnotify_add_mark_locked(&inode_mark->mark, &inode->i_fsnotify_marks,
				       type, false, NULL);
	if (ret)
		goto out;

	/* Take an sbm reference so the created mark is accounted for. */
	atomic_inc(&sbm->refcnt);

out:
	fsnotify_put_mark(&inode_mark->mark);
	return ret;
}

int chromiumos_update_inode_security_policy(
	struct inode *inode,
	enum chromiumos_inode_security_policy_type type,
	enum chromiumos_inode_security_policy policy)
{
	struct chromiumos_super_block_mark *sbm;
	struct fsnotify_mark *mark;
	bool free_mark = false;
	int ret;
	size_t i;

	sbm = chromiumos_super_block_get(inode->i_sb);
	if (IS_ERR(sbm))
		return PTR_ERR(sbm);

	mutex_lock(&sbm->fsn_group->mark_mutex);

	mark = fsnotify_find_mark(&inode->i_fsnotify_marks, sbm->fsn_group);
	if (mark) {
		WRITE_ONCE(chromiumos_to_inode_mark(mark)->policies[type],
				   policy);
		/*
		 * Frees mark if all policies are
		 * CHROMIUM_INODE_POLICY_INHERIT.
		 */
		free_mark = true;
		for (i = 0; i < CHROMIUMOS_NUMBER_OF_POLICIES; i++) {
			if (chromiumos_to_inode_mark(mark)->policies[i]
				!= CHROMIUMOS_INODE_POLICY_INHERIT) {
				free_mark = false;
				break;
			}
		}
		if (free_mark)
			fsnotify_detach_mark(mark);
		ret = 0;
	} else {
		ret = chromiumos_inode_mark_create(sbm, inode, type, policy);
	}

	mutex_unlock(&sbm->fsn_group->mark_mutex);
	chromiumos_super_block_put(sbm);

	/* This must happen after dropping the mark mutex. */
	if (free_mark)
		fsnotify_free_mark(mark);
	if (mark)
		fsnotify_put_mark(mark);

	return ret;
}

/* Flushes all inode security policies. */
int chromiumos_flush_inode_security_policies(struct super_block *sb)
{
	struct chromiumos_super_block_mark *sbm;

	sbm = chromiumos_super_block_lookup(sb);
	if (sbm) {
		fsnotify_clear_marks_by_group(sbm->fsn_group,
					      FSNOTIFY_OBJ_ALL_TYPES_MASK);
		chromiumos_super_block_put(sbm);
	}

	return 0;
}

enum chromiumos_inode_security_policy chromiumos_get_inode_security_policy(
	struct dentry *dentry, struct inode *inode,
	enum chromiumos_inode_security_policy_type type)
{
	struct chromiumos_super_block_mark *sbm;
	/*
	 * Initializes policy to CHROMIUM_INODE_POLICY_INHERIT, which is
	 * the value that will be returned if neither |dentry| nor any
	 * directory in its path has been asigned an inode security policy
	 * value for the given type.
	 */
	enum chromiumos_inode_security_policy policy =
		CHROMIUMOS_INODE_POLICY_INHERIT;

	if (!dentry || !inode || type >= CHROMIUMOS_NUMBER_OF_POLICIES)
		return policy;

	sbm = chromiumos_super_block_lookup(inode->i_sb);
	if (!sbm)
		return policy;

	/* Walk the dentry path and look for a traversal policy. */
	rcu_read_lock();
	while (1) {
		struct fsnotify_mark *mark = fsnotify_find_mark(
			&inode->i_fsnotify_marks, sbm->fsn_group);
		if (mark) {
			struct chromiumos_inode_mark *inode_mark =
				chromiumos_to_inode_mark(mark);
			policy = READ_ONCE(inode_mark->policies[type]);
			fsnotify_put_mark(mark);

			if (policy != CHROMIUMOS_INODE_POLICY_INHERIT)
				break;
		}

		if (IS_ROOT(dentry))
			break;
		dentry = READ_ONCE(dentry->d_parent);
		if (!dentry)
			break;
		inode = d_inode_rcu(dentry);
		if (!inode)
			break;
	}
	rcu_read_unlock();

	chromiumos_super_block_put(sbm);

	return policy;
}
