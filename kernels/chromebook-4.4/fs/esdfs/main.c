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
#include <linux/module.h>
#include <linux/parser.h>
#include <linux/security.h>
#include <linux/proc_ns.h>

/*
 * Derived from first generation "ANDROID_EMU" glue in modifed F2FS driver.
 */
enum {
	Opt_lower_perms,
	Opt_upper_perms,
	Opt_derive_none,
	Opt_derive_legacy,
	Opt_derive_unified,
	Opt_derive_multi,
	Opt_derive_public,
	Opt_confine,
	Opt_noconfine,
	Opt_gid_derivation,
	Opt_default_normal,
	Opt_dl_loc,
	Opt_dl_uid,
	Opt_dl_gid,
	Opt_ns_fd,

	/* From sdcardfs */
	Opt_fsuid,
	Opt_fsgid,
	Opt_gid,
	Opt_debug,
	Opt_mask,
	Opt_multiuser,
	Opt_userid,

	Opt_err,
};

static match_table_t esdfs_tokens = {
	{Opt_lower_perms, "lower=%s"},
	{Opt_upper_perms, "upper=%s"},
	{Opt_derive_none, "derive=none"},
	{Opt_derive_legacy, "derive=legacy"},
	{Opt_derive_unified, "derive=unified"},
	{Opt_derive_multi, "derive=multi"},
	{Opt_derive_public, "derive=public"},
	{Opt_confine, "confine"},
	{Opt_noconfine, "noconfine"},
	{Opt_gid_derivation, "derive_gid"},
	{Opt_default_normal, "default_normal"},
	{Opt_dl_loc, "dl_loc=%s"},
	{Opt_dl_uid, "dl_uid=%u"},
	{Opt_dl_gid, "dl_gid=%u"},
	{Opt_ns_fd, "ns_fd=%d"},
	/* compatibility with sdcardfs options */
	{Opt_fsuid, "fsuid=%u"},
	{Opt_fsgid, "fsgid=%u"},
	{Opt_gid, "gid=%u"},
	{Opt_mask, "mask=%u"},
	{Opt_userid, "userid=%d"},
	{Opt_multiuser, "multiuser"},
	{Opt_gid_derivation, "derive_gid"},
	{Opt_err, NULL},
};

struct esdfs_perms esdfs_perms_table[ESDFS_PERMS_TABLE_SIZE] = {
	/* ESDFS_PERMS_LOWER_DEFAULT */
	{ .raw_uid = -1,
	  .raw_gid = -1,
	  .uid   = AID_MEDIA_RW,
	  .gid   = AID_MEDIA_RW,
	  .fmask = 0664,
	  .dmask = 0775 },
	/* ESDFS_PERMS_UPPER_LEGACY */
	{ .raw_uid = -1,
	  .raw_gid = -1,
	  .uid   = AID_ROOT,
	  .gid   = AID_SDCARD_RW,
	  .fmask = 0664,
	  .dmask = 0775 },
	/* ESDFS_PERMS_UPPER_DERIVED */
	{ .raw_uid = -1,
	  .raw_gid = -1,
	  .uid   = AID_ROOT,
	  .gid   = AID_SDCARD_R,
	  .fmask = 0660,
	  .dmask = 0771 },
	/* ESDFS_PERMS_LOWER_DOWNLOAD */
	{ .raw_uid = -1,
	  .raw_gid = -1,
	  .uid   = -1,
	  .gid   = -1,
	  .fmask = 0644,
	  .dmask = 0711 },
};

static int parse_perms(struct esdfs_perms *perms, char *args)
{
	char *sep = args;
	char *sepres;
	int ret;

	if (!sep)
		return -EINVAL;

	sepres = strsep(&sep, ":");
	if (!sep)
		return -EINVAL;
	ret = kstrtou32(sepres, 0, &perms->uid);
	if (ret)
		return ret;

	sepres = strsep(&sep, ":");
	if (!sep)
		return -EINVAL;
	ret = kstrtou32(sepres, 0, &perms->gid);
	if (ret)
		return ret;

	sepres = strsep(&sep, ":");
	if (!sep)
		return -EINVAL;
	ret = kstrtou16(sepres, 8, &perms->fmask);
	if (ret)
		return ret;

	sepres = strsep(&sep, ":");
	ret = kstrtou16(sepres, 8, &perms->dmask);
	if (ret)
		return ret;

	return 0;
}

static inline struct user_namespace *to_user_ns(struct ns_common *ns)
{
	return container_of(ns, struct user_namespace, ns);
}

static struct user_namespace *get_ns_from_fd(int fd)
{
	struct file *file;
	struct ns_common *ns;
	struct user_namespace *user_ns = ERR_PTR(-EINVAL);

	file = proc_ns_fget(fd);
	if (IS_ERR(file))
		return ERR_CAST(file);

	ns = get_proc_ns(file_inode(file));
#ifdef CONFIG_USER_NS
	if (ns->ops == &userns_operations)
		user_ns = to_user_ns(ns);
#endif
	fput(file);
	return user_ns;
}

static int parse_options(struct super_block *sb, char *options)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(sb);
	substring_t args[MAX_OPT_ARGS];
	char *p;
	int option;

	if (!options)
		return 0;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;

		if (!*p)
			continue;
		/*
		 * Initialize args struct so we know whether arg was
		 * found; some options take optional arguments.
		 */
		args[0].to = args[0].from = NULL;
		token = match_token(p, esdfs_tokens, args);

		switch (token) {
		case Opt_lower_perms:
			if (args->from) {
				int ret;
				char *perms = match_strdup(args);

				ret = parse_perms(&sbi->lower_perms, perms);
				kfree(perms);

				if (ret)
					return -EINVAL;
			} else
				return -EINVAL;
			break;
		case Opt_upper_perms:
			if (args->from) {
				int ret;
				char *perms = match_strdup(args);

				ret = parse_perms(&sbi->upper_perms, perms);
				kfree(perms);

				if (ret)
					return -EINVAL;
			} else
				return -EINVAL;
			break;
		case Opt_derive_none:
			clear_opt(sbi, DERIVE_LEGACY);
			clear_opt(sbi, DERIVE_UNIFIED);
			clear_opt(sbi, DERIVE_MULTI);
			clear_opt(sbi, DERIVE_PUBLIC);
			break;
		case Opt_derive_legacy:
			set_opt(sbi, DERIVE_LEGACY);
			clear_opt(sbi, DERIVE_UNIFIED);
			clear_opt(sbi, DERIVE_MULTI);
			clear_opt(sbi, DERIVE_PUBLIC);
			break;
		case Opt_derive_unified:
			clear_opt(sbi, DERIVE_LEGACY);
			set_opt(sbi, DERIVE_UNIFIED);
			clear_opt(sbi, DERIVE_MULTI);
			clear_opt(sbi, DERIVE_PUBLIC);
			set_opt(sbi, DERIVE_CONFINE);	/* confine by default */
			break;
		case Opt_derive_multi:
		case Opt_multiuser:
			set_opt(sbi, DERIVE_LEGACY);
			clear_opt(sbi, DERIVE_UNIFIED);
			set_opt(sbi, DERIVE_MULTI);
			clear_opt(sbi, DERIVE_PUBLIC);
			break;
		case Opt_derive_public:
			clear_opt(sbi, DERIVE_LEGACY);
			set_opt(sbi, DERIVE_UNIFIED);
			clear_opt(sbi, DERIVE_MULTI);
			set_opt(sbi, DERIVE_PUBLIC);
			break;
		case Opt_confine:
			set_opt(sbi, DERIVE_CONFINE);
			break;
		case Opt_noconfine:
			clear_opt(sbi, DERIVE_CONFINE);
			break;
		/* for compatibility with sdcardfs options */
		case Opt_gid:
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->upper_perms.raw_gid = option;
			break;
		case Opt_userid:
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->upper_perms.raw_uid = option;
			break;
		case Opt_mask:
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->upper_perms.dmask = 0775 & ~option;
			sbi->upper_perms.fmask = 0775 & ~option;
			break;
		case Opt_fsuid:
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->lower_perms.raw_uid = option;
			break;
		case Opt_fsgid:
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->lower_perms.raw_gid = option;
			break;
		case Opt_gid_derivation:
			set_opt(sbi, GID_DERIVATION);
			break;
		case Opt_default_normal:
			set_opt(sbi, DEFAULT_NORMAL);
			break;
		case Opt_dl_loc:
			set_opt(sbi, SPECIAL_DOWNLOAD);
			sbi->dl_loc = match_strdup(args);
			break;
		case Opt_dl_uid:
			set_opt(sbi, SPECIAL_DOWNLOAD);
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->lower_dl_perms.raw_uid = option;
			break;
		case Opt_dl_gid:
			set_opt(sbi, SPECIAL_DOWNLOAD);
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->lower_dl_perms.raw_gid = option;
			break;
		case Opt_ns_fd:
			if (match_int(&args[0], &option))
				return -EINVAL;
			sbi->ns_fd = option;
			break;
		default:
			esdfs_msg(sb, KERN_ERR,
			  "unrecognized mount option \"%s\" or missing value\n",
			  p);
			return -EINVAL;
		}
	}
	return 0;
}

static int interpret_perms(struct esdfs_sb_info *sbi, struct esdfs_perms *perms)
{
	if (perms->raw_uid == -1) {
		perms->raw_uid = perms->uid;
	} else {
		perms->uid = esdfs_from_local_uid(sbi, perms->raw_uid);
		if (perms->uid == -1)
			return -EINVAL;
	}

	if (perms->raw_gid == -1) {
		perms->raw_gid = perms->gid;
	} else {
		perms->gid = esdfs_from_local_gid(sbi, perms->raw_gid);
		if (perms->gid == -1)
			return -EINVAL;
	}
	return 0;
}

/*
 * There is no need to lock the esdfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int esdfs_read_super(struct super_block *sb, const char *dev_name,
		void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	struct esdfs_sb_info *sbi;
	struct inode *inode;
	struct dentry *lower_dl_dentry;
	struct user_namespace *user_ns;
	kuid_t dl_kuid = INVALID_UID;
	kgid_t dl_kgid = INVALID_GID;

	if (!dev_name) {
		esdfs_msg(sb, KERN_ERR, "missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		esdfs_msg(sb, KERN_ERR,
			"error accessing lower directory '%s'\n", dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct esdfs_sb_info), GFP_KERNEL);
	sbi = ESDFS_SB(sb);
	if (!sbi) {
		esdfs_msg(sb, KERN_CRIT, "read_super: out of memory\n");
		err = -ENOMEM;
		goto out_pput;
	}
	INIT_LIST_HEAD(&sbi->s_list);

	/* set defaults and then parse the mount options */

	sbi->ns_fd = -1;

	/* make public default */
	clear_opt(sbi, DERIVE_LEGACY);
	set_opt(sbi, DERIVE_UNIFIED);
	clear_opt(sbi, DERIVE_MULTI);
	set_opt(sbi, DERIVE_PUBLIC);

	memcpy(&sbi->lower_perms,
	       &esdfs_perms_table[ESDFS_PERMS_LOWER_DEFAULT],
	       sizeof(struct esdfs_perms));
	if (ESDFS_DERIVE_PERMS(sbi))
		memcpy(&sbi->upper_perms,
		       &esdfs_perms_table[ESDFS_PERMS_UPPER_DERIVED],
		       sizeof(struct esdfs_perms));
	else
		memcpy(&sbi->upper_perms,
		       &esdfs_perms_table[ESDFS_PERMS_UPPER_LEGACY],
		       sizeof(struct esdfs_perms));

	memcpy(&sbi->lower_dl_perms,
	       &esdfs_perms_table[ESDFS_PERMS_LOWER_DOWNLOAD],
	       sizeof(struct esdfs_perms));

	err = parse_options(sb, (char *)raw_data);
	if (err)
		goto out_free;

	/* Initialize special namespace for lower Downloads directory */
	sbi->dl_ns = get_user_ns(current_user_ns());

	if (sbi->ns_fd == -1) {
		sbi->base_ns = get_user_ns(current_user_ns());
	} else {
		user_ns = get_ns_from_fd(sbi->ns_fd);
		if (IS_ERR(user_ns)) {
			err = PTR_ERR(user_ns);
			goto out_free;
		}
		sbi->base_ns = get_user_ns(user_ns);
	}
	/* interpret all parameters in given namespace */
	err = interpret_perms(sbi, &sbi->lower_perms);
	if (err) {
		pr_err("esdfs: Invalid permissions for lower layer\n");
		goto out_free;
	}
	err = interpret_perms(sbi, &sbi->upper_perms);
	if (err) {
		pr_err("esdfs: Invalid permissions for upper layer\n");
		goto out_free;
	}

	/* Check if the downloads uid maps into a valid kuid from
	 * the namespace of the mounting process
	 */
	if (sbi->lower_dl_perms.raw_uid != -1) {
		dl_kuid = make_kuid(sbi->dl_ns,
				    sbi->lower_dl_perms.raw_uid);
		if (!uid_valid(dl_kuid)) {
			pr_err("esdfs: Invalid permissions for dl_uid");
			err = -EINVAL;
			goto out_free;
		}
	}
	if (sbi->lower_dl_perms.raw_gid != -1) {
		dl_kgid = make_kgid(sbi->dl_ns,
				    sbi->lower_dl_perms.raw_gid);
		if (!gid_valid(dl_kgid)) {
			pr_err("esdfs: Invalid permissions for dl_gid");
			err = -EINVAL;
			goto out_free;
		}
	}

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	esdfs_set_lower_super(sb, lower_sb);

	sb->s_stack_depth = lower_sb->s_stack_depth + 1;
	if (sb->s_stack_depth > FILESYSTEM_MAX_STACK_DEPTH) {
		pr_err("esdfs: maximum fs stacking depth exceeded\n");
		err = -EINVAL;
		goto out_sput;
	}

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &esdfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = esdfs_iget(sb, lower_path.dentry->d_inode, 0);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_sput;
	}
	d_set_d_op(sb->s_root, &esdfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = esdfs_new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	if (test_opt(sbi, SPECIAL_DOWNLOAD)) {
		/* parse lower path */
		err = kern_path(sbi->dl_loc, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
				&sbi->dl_path);
		if (err) {
			esdfs_msg(sb, KERN_ERR,
				"error accessing download directory '%s'\n",
				sbi->dl_loc);
			goto out_freeroot;
		}

		lower_dl_dentry = sbi->dl_path.dentry;

		if (!S_ISDIR(lower_dl_dentry->d_inode->i_mode)) {
			err = -EINVAL;
			esdfs_msg(sb, KERN_ERR,
				"dl_loc must be a directory '%s'\n",
				sbi->dl_loc);
			goto out_dlput;
		}

		if (lower_dl_dentry->d_sb != lower_sb) {
			esdfs_msg(sb, KERN_ERR,
				"dl_loc must be in the same filesystem '%s'\n",
				sbi->dl_loc);
			goto out_dlput;
		}

		if (!uid_valid(dl_kuid)) {
			dl_kuid = esdfs_make_kuid(sbi, sbi->lower_perms.uid);
			sbi->lower_dl_perms.raw_uid = from_kuid(sbi->dl_ns,
								dl_kuid);
		}
		if (!gid_valid(dl_kgid)) {
			dl_kgid = esdfs_make_kgid(sbi, sbi->lower_perms.gid);
			sbi->lower_dl_perms.raw_gid = from_kgid(sbi->dl_ns,
								dl_kgid);
		}
		spin_lock(&lower_dl_dentry->d_lock);
		sbi->dl_name.name = kstrndup(lower_dl_dentry->d_name.name,
				lower_dl_dentry->d_name.len, GFP_ATOMIC);
		sbi->dl_name.len = lower_dl_dentry->d_name.len;
		spin_unlock(&lower_dl_dentry->d_lock);
	}
	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	esdfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		esdfs_msg(sb, KERN_INFO, "mounted on top of %s type %s\n",
			dev_name, lower_sb->s_type->name);

	if (!ESDFS_DERIVE_PERMS(sbi))
		goto out;

	/* let user know that we ignore this option in older derived modes */
	if (ESDFS_RESTRICT_PERMS(sbi) &&
	    memcmp(&sbi->upper_perms,
		   &esdfs_perms_table[ESDFS_PERMS_UPPER_DERIVED],
		   sizeof(struct esdfs_perms)))
		esdfs_msg(sb, KERN_WARNING,
			"'upper' mount option ignored in this derived mode\n");

	/*
	 * In Android 3.0 all user conent in the emulated storage tree was
	 * stored in /data/media.  Android 4.2 introduced multi-user support,
	 * which required that the primary user's content be migrated from
	 * /data/media to /data/media/0.  The framework then uses bind mounts
	 * to create per-process namespaces to isolate each user's tree at
	 * /data/media/N.  This approach of having each user in a common root
	 * is now considered "legacy" by the sdcard service.
	 */
	if (test_opt(sbi, DERIVE_LEGACY)) {
		ESDFS_I(inode)->tree = ESDFS_TREE_ROOT_LEGACY;
		sbi->obb_parent = dget(sb->s_root);
	/*
	 * Android 4.4 reorganized this sturcture yet again, so that the
	 * primary user's content was again at the root.  Secondary users'
	 * content is found in Android/user/N.  Emulated internal storage still
	 * seems to use the legacy tree, but secondary external storage uses
	 * this method.
	 */
	} else if (test_opt(sbi, DERIVE_UNIFIED))
		ESDFS_I(inode)->tree = ESDFS_TREE_ROOT;
	/*
	 * Later versions of Android organize user content using quantum
	 * entanglement, which has a low probability of being supported by
	 * this driver.
	 */
	else
		esdfs_msg(sb, KERN_WARNING,
				"unsupported derived permissions mode\n");

	/* initialize root inode */
	esdfs_derive_perms(sb->s_root);
	esdfs_set_perms(inode);

	esdfs_add_super(sbi, sb);

	goto out;

out_dlput:
	path_put(&sbi->dl_path);
	sbi->dl_path.dentry = NULL;
	sbi->dl_path.mnt = NULL;
out_freeroot:
	dput(sb->s_root);
	sb->s_root = NULL;
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
out_free:
	if (sbi->dl_ns)
		put_user_ns(sbi->dl_ns);
	if (sbi->base_ns)
		put_user_ns(sbi->base_ns);
	kfree(sbi->dl_loc);
	kfree(ESDFS_SB(sb));
	sb->s_fs_info = NULL;
out_pput:
	path_put(&lower_path);

out:
	return err;
}

struct esdfs_mount_private {
	const char *dev_name;
	void *raw_data;
};

static int __esdfs_fill_super(struct super_block *sb, void *_priv, int silent)
{
	struct esdfs_mount_private *priv = _priv;

	return esdfs_read_super(sb, priv->dev_name, priv->raw_data, silent);
}

static struct dentry *esdfs_mount(struct file_system_type *fs_type, int flags,
				const char *dev_name, void *raw_data)
{
	struct esdfs_mount_private priv = {
		.dev_name = dev_name,
		.raw_data = raw_data,
	};

	return mount_nodev(fs_type, flags, &priv, __esdfs_fill_super);
}

static void esdfs_kill_sb(struct super_block *sb)
{
	if (sb->s_fs_info && ESDFS_SB(sb)->obb_parent)
		dput(ESDFS_SB(sb)->obb_parent);
	if (sb->s_fs_info && ESDFS_SB(sb)->dl_ns)
		put_user_ns(ESDFS_SB(sb)->dl_ns);
	if (sb->s_fs_info && ESDFS_SB(sb)->base_ns)
		put_user_ns(ESDFS_SB(sb)->base_ns);
	if (sb->s_fs_info) {
		kfree(ESDFS_SB(sb)->dl_loc);
		kfree(ESDFS_SB(sb)->dl_name.name);
		path_put(&ESDFS_SB(sb)->dl_path);
	}

	kill_anon_super(sb);
}

static struct file_system_type esdfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= ESDFS_NAME,
	.mount		= esdfs_mount,
	.kill_sb	= esdfs_kill_sb,
	.fs_flags	= 0,
};
MODULE_ALIAS_FS(ESDFS_NAME);

static int __init init_esdfs_fs(void)
{
	int err;

	pr_info("Registering esdfs " ESDFS_VERSION "\n");

	esdfs_init_package_list();

	err = esdfs_init_inode_cache();
	if (err)
		goto out;
	err = esdfs_init_dentry_cache();
	if (err)
		goto out;
	err = register_filesystem(&esdfs_fs_type);
out:
	if (err) {
		esdfs_destroy_inode_cache();
		esdfs_destroy_dentry_cache();
		esdfs_destroy_package_list();
	}
	return err;
}

static void __exit exit_esdfs_fs(void)
{
	esdfs_destroy_inode_cache();
	esdfs_destroy_dentry_cache();
	esdfs_destroy_package_list();
	unregister_filesystem(&esdfs_fs_type);
	pr_info("Completed esdfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu/)");
MODULE_DESCRIPTION("esdfs " ESDFS_VERSION);
MODULE_LICENSE("GPL");

module_init(init_esdfs_fs);
module_exit(exit_esdfs_fs);
