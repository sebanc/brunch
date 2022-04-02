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

/* FS feature availability policy for inode. */
enum chromiumos_inode_security_policy {
	CHROMIUMOS_INODE_POLICY_INHERIT, /* Inherit policy from parent dir */
	CHROMIUMOS_INODE_POLICY_ALLOW,
	CHROMIUMOS_INODE_POLICY_BLOCK,
};

/*
 * Inode security policy types available for use. To add an additional
 * security policy, simply add a new member here, add the corresponding policy
 * files in securityfs.c, and associate the files being added with the new enum
 * member.
 */
enum chromiumos_inode_security_policy_type {
	CHROMIUMOS_SYMLINK_TRAVERSAL = 0,
	CHROMIUMOS_FIFO_ACCESS,
	CHROMIUMOS_NUMBER_OF_POLICIES, /* Do not add entries after this line. */
};

extern int chromiumos_update_inode_security_policy(
	struct inode *inode,
	enum chromiumos_inode_security_policy_type type,
	enum chromiumos_inode_security_policy policy);
int chromiumos_flush_inode_security_policies(struct super_block *sb);

extern enum chromiumos_inode_security_policy
chromiumos_get_inode_security_policy(
	struct dentry *dentry, struct inode *inode,
	enum chromiumos_inode_security_policy_type type);
