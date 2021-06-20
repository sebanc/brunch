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

#include <linux/capability.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "inode_mark.h"
#include "process_management.h"

static struct dentry *chromiumos_dir;
static struct dentry *chromiumos_inode_policy_dir;
static struct dentry *chromiumos_process_management_policy_dir;

struct chromiumos_inode_policy_file_entry {
	const char *name;
	int (*handle_write)(struct chromiumos_inode_policy_file_entry *,
			    struct dentry *);
	enum chromiumos_inode_security_policy_type type;
	enum chromiumos_inode_security_policy policy;
	struct dentry *dentry;
};

struct chromiumos_process_management_file_entry {
	const char *name;
	enum chromiumos_process_management_file_write_type type;
	struct dentry *dentry;
};

static int chromiumos_inode_policy_file_write(
	struct chromiumos_inode_policy_file_entry *file_entry,
	struct dentry *dentry)
{
	return chromiumos_update_inode_security_policy(dentry->d_inode,
		file_entry->type, file_entry->policy);
}

/*
 * Causes all marks to be removed from inodes thus removing all inode security
 * policies.
 */
static int chromiumos_inode_policy_file_flush_write(
	struct chromiumos_inode_policy_file_entry *file_entry,
	struct dentry *dentry)
{
	return chromiumos_flush_inode_security_policies(dentry->d_sb);
}

static struct chromiumos_inode_policy_file_entry
		chromiumos_inode_policy_files[] = {
	{.name = "block_symlink",
	 .handle_write = chromiumos_inode_policy_file_write,
	 .type = CHROMIUMOS_SYMLINK_TRAVERSAL,
	 .policy = CHROMIUMOS_INODE_POLICY_BLOCK},
	{.name = "allow_symlink",
	 .handle_write = chromiumos_inode_policy_file_write,
	 .type = CHROMIUMOS_SYMLINK_TRAVERSAL,
	 .policy = CHROMIUMOS_INODE_POLICY_ALLOW},
	{.name = "reset_symlink",
	 .handle_write = chromiumos_inode_policy_file_write,
	 .type = CHROMIUMOS_SYMLINK_TRAVERSAL,
	 .policy = CHROMIUMOS_INODE_POLICY_INHERIT},
	{.name = "block_fifo",
	 .handle_write = chromiumos_inode_policy_file_write,
	 .type = CHROMIUMOS_FIFO_ACCESS,
	 .policy = CHROMIUMOS_INODE_POLICY_BLOCK},
	{.name = "allow_fifo",
	 .handle_write = chromiumos_inode_policy_file_write,
	 .type = CHROMIUMOS_FIFO_ACCESS,
	 .policy = CHROMIUMOS_INODE_POLICY_ALLOW},
	{.name = "reset_fifo",
	 .handle_write = chromiumos_inode_policy_file_write,
	 .type = CHROMIUMOS_FIFO_ACCESS,
	 .policy = CHROMIUMOS_INODE_POLICY_INHERIT},
	{.name = "flush_policies",
	 .handle_write = &chromiumos_inode_policy_file_flush_write},
};

static struct chromiumos_process_management_file_entry
		chromiumos_process_management_files[] = {
	{.name = "add_whitelist_policy",
	 .type = CHROMIUMOS_PROCESS_MANAGEMENT_ADD},
	{.name = "flush_whitelist_policies",
	 .type = CHROMIUMOS_PROCESS_MANAGEMENT_FLUSH},
};

static int chromiumos_resolve_path(const char __user *buf, size_t len,
				   struct path *path)
{
	char *filename = NULL;
	char *canonical_buf = NULL;
	char *canonical;
	int ret;

	if (len + 1 > PATH_MAX)
		return -EINVAL;

	/*
	 * Copy the path to a kernel buffer. We can't use user_path_at()
	 * since it expects a zero-terminated path, which we generally don't
	 * have here.
	 */
	filename = kzalloc(len + 1, GFP_KERNEL);
	if (!filename)
		return -ENOMEM;

	if (copy_from_user(filename, buf, len)) {
		ret = -EFAULT;
		goto out;
	}

	ret = kern_path(filename, 0, path);
	if (ret)
		goto out;

	/*
	 * Make sure the path is canonical, i.e. it didn't contain symlinks. To
	 * check this we convert |path| back to an absolute path (within the
	 * global root) and compare the resulting path name with the passed-in
	 * |filename|. This is stricter than needed (i.e. consecutive slashes
	 * don't get ignored), but that's fine for our purposes.
	 */
	canonical_buf = kzalloc(len + 1, GFP_KERNEL);
	if (!canonical_buf) {
		ret = -ENOMEM;
		goto out;
	}

	canonical = d_absolute_path(path, canonical_buf, len + 1);
	if (IS_ERR(canonical)) {
		ret = PTR_ERR(canonical);

		/* Buffer too short implies |filename| wasn't canonical. */
		if (ret == -ENAMETOOLONG)
			ret = -EMLINK;

		goto out;
	}

	ret = strcmp(filename, canonical) ? -EMLINK : 0;

out:
	kfree(canonical_buf);
	if (ret < 0)
		path_put(path);
	kfree(filename);
	return ret;
}

static ssize_t chromiumos_inode_file_write(
	struct file *file,
	const char __user *buf,
	size_t len,
	loff_t *ppos)
{
	struct chromiumos_inode_policy_file_entry *file_entry =
		file->f_inode->i_private;
	struct path path = {};
	int ret;

	if (!ns_capable(current_cred()->user_ns, CAP_SYS_ADMIN))
		return -EPERM;

	if (*ppos != 0)
		return -EINVAL;

	ret = chromiumos_resolve_path(buf, len, &path);
	if (ret)
		return ret;

	ret = file_entry->handle_write(file_entry, path.dentry);
	path_put(&path);
	return ret < 0 ? ret : len;
}

/*
 * In the case the input buffer contains one or more invalid UIDS, the kuid_t
 * variables pointed to by 'parent' and 'child' will get updated but this
 * function will return an error.
 */
static int chromiumos_parse_process_management_policy(const char __user *buf,
						      size_t len,
						      kuid_t *parent,
						      kuid_t *child)
{
	char *kern_buf;
	char *parent_buf;
	char *child_buf;
	const char separator[] = ":";
	int ret;
	size_t first_substring_length;
	long parsed_parent;
	long parsed_child;

	/* Duplicate string from user memory and NULL-terminate */
	kern_buf = memdup_user_nul(buf, len);
	if (IS_ERR(kern_buf))
		return PTR_ERR(kern_buf);

	/*
	 * Format of |buf| string should be <UID>:<UID>.
	 * Find location of ":" in kern_buf (copied from |buf|).
	 */
	first_substring_length = strcspn(kern_buf, separator);
	if (first_substring_length == 0 || first_substring_length == len) {
		ret = -EINVAL;
		goto free_kern;
	}

	parent_buf = kmemdup_nul(kern_buf, first_substring_length, GFP_KERNEL);
	if (!parent_buf) {
		ret = -ENOMEM;
		goto free_kern;
	}

	ret = kstrtol(parent_buf, 0, &parsed_parent);
	if (ret)
		goto free_both;

	child_buf = kern_buf + first_substring_length + 1;
	ret = kstrtol(child_buf, 0, &parsed_child);
	if (ret)
		goto free_both;

	*parent = make_kuid(current_user_ns(), parsed_parent);
	if (!uid_valid(*parent)) {
		ret = -EINVAL;
		goto free_both;
	}

	*child = make_kuid(current_user_ns(), parsed_child);
	if (!uid_valid(*child)) {
		ret = -EINVAL;
		goto free_both;
	}

free_both:
	kfree(parent_buf);
free_kern:
	kfree(kern_buf);
	return ret;
}

static ssize_t chromiumos_process_management_file_write(struct file *file,
							const char __user *buf,
							size_t len,
							loff_t *ppos)
{
	struct chromiumos_process_management_file_entry *file_entry =
		file->f_inode->i_private;
	kuid_t parent;
	kuid_t child;
	int ret;

	if (!ns_capable(current_user_ns(), CAP_SYS_ADMIN))
		return -EPERM;

	if (*ppos != 0)
		return -EINVAL;

	if (file_entry->type == CHROMIUMOS_PROCESS_MANAGEMENT_FLUSH) {
		chromiumos_flush_process_management_entries();
		return len;
	}

	/* file_entry->type must equal CHROMIUMOS_PROCESS_MANAGEMENT_ADD */
	ret = chromiumos_parse_process_management_policy(buf, len, &parent,
							 &child);
	if (ret)
		return ret;

	ret = chromiumos_add_process_management_entry(parent, child);
	if (ret)
		return ret;

	/* Return len on success so caller won't keep trying to write */
	return len;
}

static const struct file_operations chromiumos_inode_policy_file_fops = {
	.write = chromiumos_inode_file_write,
};

static const struct file_operations chromiumos_process_management_file_fops = {
	.write = chromiumos_process_management_file_write,
};

static void chromiumos_shutdown_securityfs(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(chromiumos_inode_policy_files); ++i) {
		struct chromiumos_inode_policy_file_entry *entry =
			&chromiumos_inode_policy_files[i];
		securityfs_remove(entry->dentry);
		entry->dentry = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(chromiumos_process_management_files); ++i) {
		struct chromiumos_process_management_file_entry *entry =
			&chromiumos_process_management_files[i];
		securityfs_remove(entry->dentry);
		entry->dentry = NULL;
	}

	securityfs_remove(chromiumos_inode_policy_dir);
	chromiumos_inode_policy_dir = NULL;

	securityfs_remove(chromiumos_process_management_policy_dir);
	chromiumos_process_management_policy_dir = NULL;

	securityfs_remove(chromiumos_dir);
	chromiumos_dir = NULL;
}

static int chromiumos_init_securityfs(void)
{
	int i;
	int ret;

	chromiumos_dir = securityfs_create_dir("chromiumos", NULL);
	if (!chromiumos_dir) {
		ret = PTR_ERR(chromiumos_dir);
		goto error;
	}

	chromiumos_inode_policy_dir =
		securityfs_create_dir(
			"inode_security_policies",
			chromiumos_dir);
	if (!chromiumos_inode_policy_dir) {
		ret = PTR_ERR(chromiumos_inode_policy_dir);
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(chromiumos_inode_policy_files); ++i) {
		struct chromiumos_inode_policy_file_entry *entry =
			&chromiumos_inode_policy_files[i];
		entry->dentry = securityfs_create_file(
			entry->name, 0200, chromiumos_inode_policy_dir,
			entry, &chromiumos_inode_policy_file_fops);
		if (IS_ERR(entry->dentry)) {
			ret = PTR_ERR(entry->dentry);
			goto error;
		}
	}

	chromiumos_process_management_policy_dir =
		securityfs_create_dir(
			"process_management_policies",
			chromiumos_dir);
	if (!chromiumos_process_management_policy_dir) {
		ret = PTR_ERR(chromiumos_process_management_policy_dir);
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(chromiumos_process_management_files); ++i) {
		struct chromiumos_process_management_file_entry *entry =
			&chromiumos_process_management_files[i];
		entry->dentry = securityfs_create_file(
			entry->name,
			0200,
			chromiumos_process_management_policy_dir,
			entry, &chromiumos_process_management_file_fops);
		if (IS_ERR(entry->dentry)) {
			ret = PTR_ERR(entry->dentry);
			goto error;
		}
	}

	return 0;

error:
	chromiumos_shutdown_securityfs();
	return ret;
}
fs_initcall(chromiumos_init_securityfs);
