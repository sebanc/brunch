/*
 * Linux Security Module for Chromium OS
 *
 * Copyright 2011 Google Inc. All Rights Reserved
 *
 * Authors:
 *      Stephan Uphoff  <ups@google.com>
 *      Kees Cook       <keescook@chromium.org>
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

#define pr_fmt(fmt) "Chromium OS LSM: " fmt

#include <asm/syscall.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/fs_parser.h>
#include <linux/fs_struct.h>
#include <linux/hashtable.h>
#include <linux/lsm_hooks.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/namei.h>	/* for nameidata_get_total_link_count */
#include <linux/path.h>
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
#include <linux/sched.h>	/* current and other task related stuff */
#include <linux/security.h>
#include <uapi/linux/mount.h>

#include "inode_mark.h"
#include "utils.h"

/*
 * Offset in the fc->security buffer where the ChromiumOS LSM options are
 * stored. Must be larger than selinux_mnt_opts. 512 was chosen
 * to give selinux_mnt_opts to grow in the future.
 */
#define LSM_OPTS_OFFSET 512

/*
 * We don't have an easy way to tell who allocated the fc->security buffer
 * so we use a magic value to ensure it was this module that allocated it
 */
#define LSM_OPT_MAGIC 0x0123456789ABCDEF

#define NUM_BITS 8 // 128 buckets in hash table

static DEFINE_HASHTABLE(sb_nosymfollow_hashtable, NUM_BITS);

struct sb_entry {
	struct hlist_node next;
	struct hlist_node dlist; /* for deletion cleanup */
	uintptr_t sb;
};

#if defined(CONFIG_SECURITY_CHROMIUMOS_NO_UNPRIVILEGED_UNSAFE_MOUNTS) || \
	defined(CONFIG_SECURITY_CHROMIUMOS_NO_SYMLINK_MOUNT)
static void report(const char *origin, const struct path *path, char *operation)
{
	char *alloced = NULL, *cmdline;
	char *pathname; /* Pointer to either static string or "alloced". */

	if (!path)
		pathname = "<unknown>";
	else {
		/* We will allow 11 spaces for ' (deleted)' to be appended */
		alloced = pathname = kmalloc(PATH_MAX+11, GFP_KERNEL);
		if (!pathname)
			pathname = "<no_memory>";
		else {
			pathname = d_path(path, pathname, PATH_MAX+11);
			if (IS_ERR(pathname))
				pathname = "<too_long>";
			else {
				pathname = printable(pathname, PATH_MAX+11);
				kfree(alloced);
				alloced = pathname;
			}
		}
	}

	cmdline = printable_cmdline(current);

	pr_notice("%s %s obj=%s pid=%d cmdline=%s\n", origin,
		  operation, pathname, task_pid_nr(current), cmdline);

	kfree(cmdline);
	kfree(alloced);
}
#endif

static int chromiumos_security_sb_mount(const char *dev_name,
					const struct path *path,
					const char *type, unsigned long flags,
					void *data)
{
#ifdef CONFIG_SECURITY_CHROMIUMOS_NO_SYMLINK_MOUNT
	if (nameidata_get_total_link_count()) {
		report("sb_mount", path, "Mount path with symlinks prohibited");
		pr_notice("sb_mount dev=%s type=%s flags=%#lx\n",
			  dev_name, type, flags);
		return -ELOOP;
	}
#endif

#ifdef CONFIG_SECURITY_CHROMIUMOS_NO_UNPRIVILEGED_UNSAFE_MOUNTS
	if ((!(flags & (MS_BIND | MS_MOVE | MS_SHARED | MS_PRIVATE | MS_SLAVE |
			MS_UNBINDABLE)) ||
	     ((flags & MS_REMOUNT) && (flags & MS_BIND))) &&
	    !capable(CAP_SYS_ADMIN)) {
		int required_mnt_flags = MNT_NOEXEC | MNT_NOSUID | MNT_NODEV;

		if (flags & MS_REMOUNT) {
			/*
			 * If this is a remount, we only require that the
			 * requested flags are a superset of the original mount
			 * flags.
			 */
			required_mnt_flags &= path->mnt->mnt_flags;
		}
		/*
		 * The three flags we are interested in disallowing in
		 * unprivileged user namespaces (MS_NOEXEC, MS_NOSUID, MS_NODEV)
		 * cannot be modified when doing a bind-mount. The kernel
		 * attempts to dispatch calls to do_mount() within
		 * fs/namespace.c in the following order:
		 *
		 * * If the MS_REMOUNT flag is present, it calls do_remount().
		 *   When MS_BIND is also present, it only allows to modify the
		 *   per-mount flags, which are copied into
		 *   |required_mnt_flags|.  Otherwise it bails in the absence of
		 *   the CAP_SYS_ADMIN in the init ns.
		 * * If the MS_BIND flag is present, the only other flag checked
		 *   is MS_REC.
		 * * If any of the mount propagation flags are present
		 *   (MS_SHARED, MS_PRIVATE, MS_SLAVE, MS_UNBINDABLE),
		 *   flags_to_propagation_type() filters out any additional
		 *   flags.
		 * * If MS_MOVE flag is present, all other flags are ignored.
		 */
		if ((required_mnt_flags & MNT_NOEXEC) && !(flags & MS_NOEXEC)) {
			report("sb_mount", path,
			       "Mounting a filesystem with 'exec' flag requires CAP_SYS_ADMIN in init ns");
			pr_notice("sb_mount dev=%s type=%s flags=%#lx\n",
				  dev_name, type, flags);
			return -EPERM;
		}
		if ((required_mnt_flags & MNT_NOSUID) && !(flags & MS_NOSUID)) {
			report("sb_mount", path,
			       "Mounting a filesystem with 'suid' flag requires CAP_SYS_ADMIN in init ns");
			pr_notice("sb_mount dev=%s type=%s flags=%#lx\n",
				  dev_name, type, flags);
			return -EPERM;
		}
		if ((required_mnt_flags & MNT_NODEV) && !(flags & MS_NODEV) &&
		    strcmp(type, "devpts")) {
			report("sb_mount", path,
			       "Mounting a filesystem with 'dev' flag requires CAP_SYS_ADMIN in init ns");
			pr_notice("sb_mount dev=%s type=%s flags=%#lx\n",
				  dev_name, type, flags);
			return -EPERM;
		}
	}
#endif

	return 0;
}

static DEFINE_SPINLOCK(sb_nosymfollow_hashtable_spinlock);

/* Check for entry in hash table. */
static bool chromiumos_check_sb_nosymfollow_hashtable(struct super_block *sb)
{
	struct sb_entry *entry;
	uintptr_t sb_pointer = (uintptr_t)sb;
	bool found = false;

	rcu_read_lock();
	hash_for_each_possible_rcu(sb_nosymfollow_hashtable,
				   entry, next, sb_pointer) {
		if (entry->sb == sb_pointer) {
			found = true;
			break;
		}
	}
	rcu_read_unlock();

	/*
	 * Its possible that a policy gets added in between the time we check
	 * above and when we return false here. Such a race condition should
	 * not affect this check however, since it would only be relevant if
	 * userspace tried to traverse a symlink on a filesystem before that
	 * filesystem was done being mounted (or potentially while it was being
	 * remounted with new mount flags).
	 */
	return found;
}

/* Add entry to hash table. */
static int chromiumos_add_sb_nosymfollow_hashtable(struct super_block *sb)
{
	struct sb_entry *new;
	uintptr_t sb_pointer = (uintptr_t)sb;

	/* Return if entry already exists */
	if (chromiumos_check_sb_nosymfollow_hashtable(sb))
		return 0;

	new = kzalloc(sizeof(struct sb_entry), GFP_KERNEL);
	if (!new)
		return -ENOMEM;
	new->sb = sb_pointer;
	spin_lock(&sb_nosymfollow_hashtable_spinlock);
	hash_add_rcu(sb_nosymfollow_hashtable, &new->next, sb_pointer);
	spin_unlock(&sb_nosymfollow_hashtable_spinlock);
	return 0;
}

/* Flush all entries from hash table. */
void chromiumos_flush_sb_nosymfollow_hashtable(void)
{
	struct sb_entry *entry;
	struct hlist_node *hlist_node;
	unsigned int bkt_loop_cursor;
	HLIST_HEAD(free_list);

	/*
	 * Could probably use hash_for_each_rcu here instead, but this should
	 * be fine as well.
	 */
	spin_lock(&sb_nosymfollow_hashtable_spinlock);
	hash_for_each_safe(sb_nosymfollow_hashtable, bkt_loop_cursor,
			   hlist_node, entry, next) {
		hash_del_rcu(&entry->next);
		hlist_add_head(&entry->dlist, &free_list);
	}
	spin_unlock(&sb_nosymfollow_hashtable_spinlock);
	synchronize_rcu();
	hlist_for_each_entry_safe(entry, hlist_node, &free_list, dlist)
		kfree(entry);
}

/* Remove entry from hash table. */
static void chromiumos_remove_sb_nosymfollow_hashtable(struct super_block *sb)
{
	struct sb_entry *entry;
	struct hlist_node *hlist_node;
	uintptr_t sb_pointer = (uintptr_t)sb;
	bool free_entry = false;

	/*
	 * Could probably use hash_for_each_rcu here instead, but this should
	 * be fine as well.
	 */
	spin_lock(&sb_nosymfollow_hashtable_spinlock);
	hash_for_each_possible_safe(sb_nosymfollow_hashtable, entry,
			   hlist_node, next, sb_pointer) {
		if (entry->sb == sb_pointer) {
			hash_del_rcu(&entry->next);
			free_entry = true;
			break;
		}
	}
	spin_unlock(&sb_nosymfollow_hashtable_spinlock);
	if (free_entry) {
		synchronize_rcu();
		kfree(entry);
	}
}

int chromiumos_security_sb_umount(struct vfsmount *mnt, int flags)
{
	/* If mnt->mnt_sb is in nosymfollow hashtable, remove it. */
	chromiumos_remove_sb_nosymfollow_hashtable(mnt->mnt_sb);

	return 0;
}

/*
 * NOTE: The WARN() calls will emit a warning in cases of blocked symlink
 * traversal attempts. These will show up in kernel warning reports
 * collected by the crash reporter, so we have some insight on spurious
 * failures that need addressing.
 */
static int chromiumos_security_inode_follow_link(struct dentry *dentry,
						 struct inode *inode, bool rcu)
{
	static char accessed_path[PATH_MAX];
	enum chromiumos_inode_security_policy policy;

	/* Deny if symlinks have been disabled on this superblock. */
	if (chromiumos_check_sb_nosymfollow_hashtable(dentry->d_sb)) {
		WARN(1,
		     "Blocked symlink traversal for path %x:%x:%s (symlinks were disabled on this FS through the 'nosymfollow' mount option)\n",
		     MAJOR(dentry->d_sb->s_dev),
		     MINOR(dentry->d_sb->s_dev),
		     dentry_path(dentry, accessed_path, PATH_MAX));
		return -EACCES;
	}

	policy = chromiumos_get_inode_security_policy(
		dentry, inode,
		CHROMIUMOS_SYMLINK_TRAVERSAL);

	WARN(policy == CHROMIUMOS_INODE_POLICY_BLOCK,
	     "Blocked symlink traversal for path %x:%x:%s (see https://goo.gl/8xICW6 for context and rationale)\n",
	     MAJOR(dentry->d_sb->s_dev), MINOR(dentry->d_sb->s_dev),
	     dentry_path(dentry, accessed_path, PATH_MAX));

	return policy == CHROMIUMOS_INODE_POLICY_BLOCK ? -EACCES : 0;
}

static int chromiumos_security_file_open(struct file *file)
{
	static char accessed_path[PATH_MAX];
	enum chromiumos_inode_security_policy policy;
	struct dentry *dentry = file->f_path.dentry;

	/* Returns 0 if file is not a FIFO */
	if (!S_ISFIFO(file->f_inode->i_mode))
		return 0;

	policy = chromiumos_get_inode_security_policy(
		dentry, dentry->d_inode,
		CHROMIUMOS_FIFO_ACCESS);

	/*
	 * Emit a warning in cases of blocked fifo access attempts. These will
	 * show up in kernel warning reports collected by the crash reporter,
	 * so we have some insight on spurious failures that need addressing.
	 */
	WARN(policy == CHROMIUMOS_INODE_POLICY_BLOCK,
	     "Blocked fifo access for path %x:%x:%s\n (see https://goo.gl/8xICW6 for context and rationale)\n",
	     MAJOR(dentry->d_sb->s_dev), MINOR(dentry->d_sb->s_dev),
	     dentry_path(dentry, accessed_path, PATH_MAX));

	return policy == CHROMIUMOS_INODE_POLICY_BLOCK ? -EACCES : 0;
}

struct chromiumos_mnt_opts {
	/* Must be LSM_OPT_MAGIC */
	uint64_t magic;
	int nosymfollow;
};

/*
 * We use the fact that the Chromium LSM is stacked in front of SELinux
 * to allocate a buffer big enough to store the SELinux selinux_mnt_opts
 * and the chromiumos_mnt_opts. We do this because *mnt_opts is shared
 * between all the LSMs without a way of coordinating and sharing
 * storage. This means that the ChromiumOS LSM needs to be the one that
 * allocates the memory.
 *
 * There is a catch to this trick. SELinux must be initialized before the
 * *mnt_opts can be set. Otherwise SELinux will complain. This means we can
 * only use nosymfollow after SELinux has started.
 *
 * We use the magic value to determine if the ChromiumOS LSM was the one that
 * allocated the buffer and if our options are present.
 */
int get_chromiumos_mnt_opts(void **mnt_opts, struct chromiumos_mnt_opts **opts)
{
	void *data = *mnt_opts;

	if (!data) {
		data = kzalloc(PAGE_SIZE, GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		*mnt_opts = data;
		*opts = (struct chromiumos_mnt_opts *)((uintptr_t)data +
						       LSM_OPTS_OFFSET);
		(*opts)->magic = LSM_OPT_MAGIC;
	} else {
		*opts = (struct chromiumos_mnt_opts *)((uintptr_t)data +
						       LSM_OPTS_OFFSET);

		/*
		 * Check to see if we allocated the buffer. If this fails that
		 * means something was stacked in front of this module.
		 */
		if ((*opts)->magic != LSM_OPT_MAGIC)
			panic("ChromiumOS LSM must be first in the stack!");
	}

	return 0;
}

int chromiumos_sb_eat_lsm_opts(char *orig, void **mnt_opts)
{
	int rc;
	char *orig_copy;
	char *orig_copy_cur;
	char *option;
	size_t offset = 0;
	bool found = false;
	struct chromiumos_mnt_opts *opts;

	if (!orig || *orig == 0)
		return 0;

	orig_copy = (char *)get_zeroed_page(GFP_KERNEL);
	if (!orig_copy)
		return -ENOMEM;
	strncpy(orig_copy, orig, PAGE_SIZE);

	memset(orig, 0, strlen(orig));

	orig_copy_cur = orig_copy;
	while (orig_copy_cur) {
		option = strsep(&orig_copy_cur, ",");
		if (strcmp(option, "nosymfollow") == 0) {
			if (found) /* Found multiple times. */
				return -EINVAL;
			found = true;
		} else {
			if (offset > 0) {
				orig[offset] = ',';
				offset++;
			}
			strcpy(orig + offset, option);
			offset += strlen(option);
		}
	}

	if (found) {
		rc = get_chromiumos_mnt_opts(mnt_opts, &opts);
		if (rc)
			return rc;
		opts->nosymfollow = 1;
	}

	free_page((unsigned long)orig_copy);
	return 0;
}

static void chromiumos_sb_free_mnt_opts(void *mnt_opts)
{
	/* We let SELinux free the mnt_opts */
	return;
}

enum {
	Opt_error = -1,
	Opt_nosymfollow = 0,
};

static const struct fs_parameter_spec chromiumos_param_specs[] = {
	fsparam_bool("symfollow",	Opt_nosymfollow),
	{}
};

static const struct fs_parameter_description chromiumos_fs_parameters = {
	.name		= "ChromiumOS",
	.specs		= chromiumos_param_specs,
};

static int chromiumos_fs_context_parse_param(struct fs_context *fc,
					  struct fs_parameter *param)
{
	struct fs_parse_result result;
	int opt, rc;
	struct chromiumos_mnt_opts *opts;

	opt = fs_parse(fc, &chromiumos_fs_parameters, param, &result);

	if (opt < 0)
		return opt;

	rc = get_chromiumos_mnt_opts(&fc->security, &opts);
	if (rc)
		return rc;

	if (opt == Opt_nosymfollow)
		opts->nosymfollow = !result.boolean;

	return 0;
}

int chromiumos_sb_set_mnt_opts(struct super_block *sb, void *mnt_opts,
			       unsigned long kern_flags,
			       unsigned long *set_kern_flags)
{
	int ret;
	struct chromiumos_mnt_opts *opts;

	if (!mnt_opts)
		return 0;

	ret = get_chromiumos_mnt_opts(&mnt_opts, &opts);
	if (ret)
		return ret;

	if (opts->nosymfollow)
		ret = chromiumos_add_sb_nosymfollow_hashtable(sb);
	else
		chromiumos_remove_sb_nosymfollow_hashtable(sb);

	return ret;
}

static struct security_hook_list chromiumos_security_hooks[] = {
	LSM_HOOK_INIT(sb_mount, chromiumos_security_sb_mount),
	LSM_HOOK_INIT(inode_follow_link, chromiumos_security_inode_follow_link),
	LSM_HOOK_INIT(file_open, chromiumos_security_file_open),
	LSM_HOOK_INIT(sb_eat_lsm_opts, chromiumos_sb_eat_lsm_opts),
	LSM_HOOK_INIT(fs_context_parse_param, chromiumos_fs_context_parse_param),
	LSM_HOOK_INIT(sb_free_mnt_opts, chromiumos_sb_free_mnt_opts),
	LSM_HOOK_INIT(sb_set_mnt_opts, chromiumos_sb_set_mnt_opts),
	LSM_HOOK_INIT(sb_umount, chromiumos_security_sb_umount)
};

static int __init chromiumos_security_init(void)
{
	security_add_hooks(chromiumos_security_hooks,
			   ARRAY_SIZE(chromiumos_security_hooks), "chromiumos");

	pr_info("enabled");

	return 0;
}
DEFINE_LSM(chromiumos) = {
	.name = "chromiumos",
	.init = chromiumos_security_init
};
