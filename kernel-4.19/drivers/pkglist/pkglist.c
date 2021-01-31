/*
 * Copyright (C) 2017 Google Inc., Author: Daniel Rosenberg <drosen@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/configfs.h>
#include <linux/dcache.h>
#include <linux/ctype.h>
#include <linux/cred.h>

#include <linux/pkglist.h>

/*
 * This presents a configfs interface for Android's emulated sdcard layer.
 * It relates the names of packages to their package ids, so that they can be
 * given access to their app specific folders.
 *
 * To add a package, create a directory at the base level with the name of that
 * package. Within these folders, write to appid to set its id.
 * If an Android user should not know of an app's installation, write their
 * Android user id to excluded_userids. Write to clear_userid to remove users
 * from that list.
 *
 * remove_userid offers a way to remove all instances of a user from all exclude
 * lists.
 *
 * Additionally, pkglist allows configuring the gid assigned to the lower file
 * outside of package specific directories for the purpose of tracking storage
 * with quotas.
 *
 * To track files with a particular extension, create a folder inside extensions
 * for each class of thing you wish to track. Inside that directory, write the
 * gid you want to associate to the group to ext_gid, and make a directory for
 * extension you want to include. All are assumed to be case insensitive.
 *
 * ex: mkdir /config/[config_location]/extension/audio/
 *     echo 1055 > /config/[config_location]/extension/audio/ext_gid
 *     mkdir /config/[config_location]/extension/audio/
 *
 */

static char *pkglist_config_location = "sdcardfs";
module_param(pkglist_config_location, charp, 0);
MODULE_PARM_DESC(pkglist_config_location, "Location of pkglist in configfs");

static struct kmem_cache *hashtable_entry_cachep;

static DEFINE_HASHTABLE(package_to_appid, 8);
static DEFINE_HASHTABLE(package_to_userid, 8);
static DEFINE_HASHTABLE(ext_to_groupid, 8);
static DEFINE_MUTEX(pkg_list_lock);
static LIST_HEAD(pkglist_listeners);

struct extensions_value {
	struct config_group group;
	kgid_t gid;
};

struct extension_details {
	struct config_item item;
	struct hlist_node hlist;
	struct qstr name;
	struct extensions_value *value;
};

struct hashtable_entry {
	struct hlist_node hlist;
	struct hlist_node dlist; /* for deletion cleanup */
	struct qstr key;
	atomic_t value;
};

static unsigned int full_name_case_hash(const unsigned char *name,
					unsigned int len)
{
	unsigned long hash = init_name_hash(0);

	while (len--)
		hash = partial_name_hash(tolower(*name++), hash);
	return end_name_hash(hash);
}

static inline void qstr_init(struct qstr *q, const char *name)
{
	q->name = name;
	q->len = strlen(q->name);
	q->hash = full_name_case_hash(q->name, q->len);
}

static inline int qstr_copy(const struct qstr *src, struct qstr *dest)
{
	dest->name = kstrdup(src->name, GFP_KERNEL);
	dest->hash_len = src->hash_len;
	return !!dest->name;
}

static kuid_t __get_appid(const struct qstr *key)
{
	struct hashtable_entry *hash_cur;
	unsigned int hash = key->hash;
	uid_t ret_id;

	rcu_read_lock();
	hash_for_each_possible_rcu(package_to_appid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->key)) {
			ret_id = atomic_read(&hash_cur->value);
			rcu_read_unlock();
			return make_kuid(&init_user_ns, ret_id);
		}
	}
	rcu_read_unlock();
	return INVALID_UID;
}

kuid_t pkglist_get_appid(const char *key)
{
	struct qstr q;

	qstr_init(&q, key);
	return __get_appid(&q);
}
EXPORT_SYMBOL_GPL(pkglist_get_appid);

static kgid_t __get_ext_gid(const struct qstr *key)
{
	struct extension_details *hash_cur;
	unsigned int hash = key->hash;
	kgid_t ret_id;

	rcu_read_lock();
	hash_for_each_possible_rcu(ext_to_groupid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->name)) {
			ret_id = hash_cur->value->gid;
			rcu_read_unlock();
			return ret_id;
		}
	}
	rcu_read_unlock();
	return INVALID_GID;
}

kgid_t pkglist_get_ext_gid(const char *key)
{
	struct qstr q;

	qstr_init(&q, key);
	return __get_ext_gid(&q);
}
EXPORT_SYMBOL_GPL(pkglist_get_ext_gid);

static bool __is_excluded(const struct qstr *app_name, uint32_t user)
{
	struct hashtable_entry *hash_cur;
	unsigned int hash = app_name->hash;

	rcu_read_lock();
	hash_for_each_possible_rcu(package_to_userid, hash_cur, hlist, hash) {
		if (atomic_read(&hash_cur->value) == user &&
				qstr_case_eq(app_name, &hash_cur->key)) {
			rcu_read_unlock();
			return true;
		}
	}
	rcu_read_unlock();
	return false;
}

bool pkglist_user_is_excluded(const char *key, uint32_t user)
{
	struct qstr q;

	qstr_init(&q, key);
	return __is_excluded(&q, user);
}
EXPORT_SYMBOL_GPL(pkglist_user_is_excluded);

kuid_t pkglist_get_allowed_appid(const char *key, uint32_t user)
{
	struct qstr q;

	qstr_init(&q, key);
	if (!__is_excluded(&q, user))
		return __get_appid(&q);
	else
		return INVALID_UID;
}
EXPORT_SYMBOL_GPL(pkglist_get_allowed_appid);

static struct hashtable_entry *alloc_hashtable_entry(const struct qstr *key,
		uid_t value)
{
	struct hashtable_entry *ret = kmem_cache_alloc(hashtable_entry_cachep,
			GFP_KERNEL);
	if (!ret)
		return NULL;
	INIT_HLIST_NODE(&ret->dlist);
	INIT_HLIST_NODE(&ret->hlist);

	if (!qstr_copy(key, &ret->key)) {
		kmem_cache_free(hashtable_entry_cachep, ret);
		return NULL;
	}

	atomic_set(&ret->value, value);
	return ret;
}

static int insert_packagelist_appid_entry_locked(const struct qstr *key,
						kuid_t value)
{
	struct hashtable_entry *hash_cur;
	struct hashtable_entry *new_entry;
	unsigned int hash = key->hash;

	hash_for_each_possible_rcu(package_to_appid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->key)) {
			atomic_set(&hash_cur->value, value.val);
			return 0;
		}
	}
	new_entry = alloc_hashtable_entry(key, value.val);
	if (!new_entry)
		return -ENOMEM;
	hash_add_rcu(package_to_appid, &new_entry->hlist, hash);
	return 0;
}

static int insert_ext_gid_entry_locked(struct extension_details *ed)
{
	struct extension_details *hash_cur;
	unsigned int hash = ed->name.hash;

	/* An extension can only belong to one gid */
	hash_for_each_possible_rcu(ext_to_groupid, hash_cur, hlist, hash) {
		if (qstr_case_eq(&ed->name, &hash_cur->name))
			return -EINVAL;
	}

	hash_add_rcu(ext_to_groupid, &ed->hlist, hash);
	return 0;
}

static int insert_userid_exclude_entry_locked(const struct qstr *key,
						unsigned int value)
{
	struct hashtable_entry *hash_cur;
	struct hashtable_entry *new_entry;
	unsigned int hash = key->hash;

	/* Only insert if not already present */
	hash_for_each_possible_rcu(package_to_userid, hash_cur, hlist, hash) {
		if (atomic_read(&hash_cur->value) == value &&
				qstr_case_eq(key, &hash_cur->key))
			return 0;
	}
	new_entry = alloc_hashtable_entry(key, value);
	if (!new_entry)
		return -ENOMEM;
	hash_add_rcu(package_to_userid, &new_entry->hlist, hash);
	return 0;
}

static int insert_packagelist_entry(const struct qstr *key, kuid_t value)
{
	struct pkg_list *pkg;
	int err;

	mutex_lock(&pkg_list_lock);
	err = insert_packagelist_appid_entry_locked(key, value);
	if (!err) {
		list_for_each_entry(pkg, &pkglist_listeners, list) {
			pkg->update(BY_NAME, key, 0);
		}
	}
	mutex_unlock(&pkg_list_lock);

	return err;
}

static int insert_ext_gid_entry(struct extension_details *ed)
{
	int err;

	mutex_lock(&pkg_list_lock);
	err = insert_ext_gid_entry_locked(ed);
	mutex_unlock(&pkg_list_lock);

	return err;
}

static int insert_userid_exclude_entry(const struct qstr *key, uint32_t value)
{
	int err;
	struct pkg_list *pkg;

	mutex_lock(&pkg_list_lock);
	err = insert_userid_exclude_entry_locked(key, value);
	if (!err) {
		list_for_each_entry(pkg, &pkglist_listeners, list) {
			pkg->update(BY_NAME|BY_USERID, key, value);
		}
	}
	mutex_unlock(&pkg_list_lock);

	return err;
}

static void free_hashtable_entry(struct hashtable_entry *entry)
{
	kfree(entry->key.name);
	kmem_cache_free(hashtable_entry_cachep, entry);
}

static void remove_packagelist_entry_locked(const struct qstr *key)
{
	struct hashtable_entry *hash_cur;
	unsigned int hash = key->hash;
	struct hlist_node *h_t;
	HLIST_HEAD(free_list);

	hash_for_each_possible_rcu(package_to_userid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->key)) {
			hash_del_rcu(&hash_cur->hlist);
			hlist_add_head(&hash_cur->dlist, &free_list);
		}
	}
	hash_for_each_possible_rcu(package_to_appid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->key)) {
			hash_del_rcu(&hash_cur->hlist);
			hlist_add_head(&hash_cur->dlist, &free_list);
			break;
		}
	}
	synchronize_rcu();
	hlist_for_each_entry_safe(hash_cur, h_t, &free_list, dlist)
		free_hashtable_entry(hash_cur);
}

static void remove_packagelist_entry(const struct qstr *key)
{
	struct pkg_list *pkg;

	mutex_lock(&pkg_list_lock);
	remove_packagelist_entry_locked(key);
	list_for_each_entry(pkg, &pkglist_listeners, list) {
		pkg->update(BY_NAME, key, 0);
	}
	mutex_unlock(&pkg_list_lock);
}

static void remove_ext_gid_entry_locked(struct extension_details *ed)
{
	struct extension_details *hash_cur;
	struct qstr *key = &ed->name;
	unsigned int hash = key->hash;

	hash_for_each_possible_rcu(ext_to_groupid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->name)
				&& hash_cur->value == ed->value) {
			hash_del_rcu(&hash_cur->hlist);
			synchronize_rcu();
			break;
		}
	}
}

static void remove_ext_gid_entry(struct extension_details *ed)
{
	mutex_lock(&pkg_list_lock);
	remove_ext_gid_entry_locked(ed);
	mutex_unlock(&pkg_list_lock);
}

static void remove_userid_all_entry_locked(uint32_t userid)
{
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_t;
	HLIST_HEAD(free_list);
	int i;

	hash_for_each_rcu(package_to_userid, i, hash_cur, hlist) {
		if (atomic_read(&hash_cur->value) == userid) {
			hash_del_rcu(&hash_cur->hlist);
			hlist_add_head(&hash_cur->dlist, &free_list);
		}
	}
	synchronize_rcu();
	hlist_for_each_entry_safe(hash_cur, h_t, &free_list, dlist) {
		free_hashtable_entry(hash_cur);
	}
}

static void remove_userid_all_entry(uint32_t userid)
{
	struct pkg_list *pkg;

	mutex_lock(&pkg_list_lock);
	remove_userid_all_entry_locked(userid);

	list_for_each_entry(pkg, &pkglist_listeners, list) {
		pkg->update(BY_USERID, NULL, userid);
	}
	mutex_unlock(&pkg_list_lock);
}

static void remove_userid_exclude_entry_locked(const struct qstr *key,
						uint32_t userid)
{
	struct hashtable_entry *hash_cur;
	unsigned int hash = key->hash;

	hash_for_each_possible_rcu(package_to_userid, hash_cur, hlist, hash) {
		if (qstr_case_eq(key, &hash_cur->key) &&
				atomic_read(&hash_cur->value) == userid) {
			hash_del_rcu(&hash_cur->hlist);
			synchronize_rcu();
			free_hashtable_entry(hash_cur);
			break;
		}
	}
}

static void remove_userid_exclude_entry(const struct qstr *key, uint32_t userid)
{
	struct pkg_list *pkg;

	mutex_lock(&pkg_list_lock);
	remove_userid_exclude_entry_locked(key, userid);
	list_for_each_entry(pkg, &pkglist_listeners, list) {
		pkg->update(BY_NAME|BY_USERID, key, userid);
	}
	mutex_unlock(&pkg_list_lock);
}

static void packagelist_destroy(void)
{
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_t;
	HLIST_HEAD(free_list);
	int i;

	mutex_lock(&pkg_list_lock);
	hash_for_each_rcu(package_to_appid, i, hash_cur, hlist) {
		hash_del_rcu(&hash_cur->hlist);
		hlist_add_head(&hash_cur->dlist, &free_list);
	}
	hash_for_each_rcu(package_to_userid, i, hash_cur, hlist) {
		hash_del_rcu(&hash_cur->hlist);
		hlist_add_head(&hash_cur->dlist, &free_list);
	}
	synchronize_rcu();
	hlist_for_each_entry_safe(hash_cur, h_t, &free_list, dlist)
		free_hashtable_entry(hash_cur);
	mutex_unlock(&pkg_list_lock);
	pr_info("pkglist: destroyed pkglist\n");
}

#define PACKAGE_DETAILS_ATTR(_pfx, _name)			\
static struct configfs_attribute _pfx##attr_##_name = {	\
	.ca_name	= __stringify(_name),		\
	.ca_mode	= S_IRUGO | S_IWUGO,		\
	.ca_owner	= THIS_MODULE,			\
	.show		= _pfx##_name##_show,		\
	.store		= _pfx##_name##_store,		\
}

#define PACKAGE_DETAILS_ATTR_RO(_pfx, _name)			\
static struct configfs_attribute _pfx##attr_##_name = {	\
	.ca_name	= __stringify(_name),		\
	.ca_mode	= S_IRUGO,			\
	.ca_owner	= THIS_MODULE,			\
	.show		= _pfx##_name##_show,		\
}

#define PACKAGE_DETAILS_ATTR_WO(_pfx, _name)			\
static struct configfs_attribute _pfx##attr_##_name = {	\
	.ca_name	= __stringify(_name),		\
	.ca_mode	= S_IWUGO,			\
	.ca_owner	= THIS_MODULE,			\
	.store		= _pfx##_name##_store,		\
}


struct package_details {
	struct config_item item;
	struct qstr name;
};

static inline struct package_details *to_package_details(
						struct config_item *item)
{
	return item ? container_of(item, struct package_details, item) : NULL;
}

#define PACKAGE_DETAILS_ATTRIBUTE(name) (&package_details_attr_##name)

static ssize_t package_details_appid_show(struct config_item *item, char *page)
{
	return scnprintf(page, PAGE_SIZE, "%u\n", from_kuid(current_user_ns(),
				__get_appid(&to_package_details(item)->name)));
}

static ssize_t package_details_appid_store(struct config_item *item,
					   const char *page, size_t count)
{
	unsigned int tmp;
	int ret;
	kuid_t uid;

	ret = kstrtouint(page, 10, &tmp);
	if (ret)
		return ret;

	uid = make_kuid(current_user_ns(), tmp);

	ret = insert_packagelist_entry(&to_package_details(item)->name, uid);

	if (ret)
		return ret;

	return count;
}

static ssize_t package_details_excluded_userids_show(struct config_item *item,
						     char *page)
{
	struct package_details *package_details = to_package_details(item);
	struct hashtable_entry *hash_cur;
	unsigned int hash = package_details->name.hash;
	int count = 0;

	rcu_read_lock();
	hash_for_each_possible_rcu(package_to_userid, hash_cur, hlist, hash) {
		if (qstr_case_eq(&package_details->name, &hash_cur->key))
			count += scnprintf(page + count, PAGE_SIZE - count,
					   "%d ", atomic_read(&hash_cur->value));
	}
	rcu_read_unlock();
	if (count)
		count--;
	count += scnprintf(page + count, PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t package_details_excluded_userids_store(struct config_item *item,
						      const char *page, size_t count)
{
	unsigned int tmp;
	int ret;

	ret = kstrtouint(page, 10, &tmp);
	if (ret)
		return ret;

	ret = insert_userid_exclude_entry(&to_package_details(item)->name, tmp);

	if (ret)
		return ret;

	return count;
}

static ssize_t package_details_clear_userid_store(struct config_item *item,
						  const char *page, size_t count)
{
	unsigned int tmp;
	int ret;

	ret = kstrtouint(page, 10, &tmp);
	if (ret)
		return ret;
	remove_userid_exclude_entry(&to_package_details(item)->name, tmp);
	return count;
}

static void package_details_release(struct config_item *item)
{
	struct package_details *package_details = to_package_details(item);

	pr_info("pkglist: removing %s\n", package_details->name.name);
	remove_packagelist_entry(&package_details->name);
	kfree(package_details->name.name);
	kfree(package_details);
}

PACKAGE_DETAILS_ATTR(package_details_, appid);
PACKAGE_DETAILS_ATTR(package_details_, excluded_userids);
PACKAGE_DETAILS_ATTR_WO(package_details_, clear_userid);

static struct configfs_attribute *package_details_attrs[] = {
	PACKAGE_DETAILS_ATTRIBUTE(appid),
	PACKAGE_DETAILS_ATTRIBUTE(excluded_userids),
	PACKAGE_DETAILS_ATTRIBUTE(clear_userid),
	NULL,
};

static struct configfs_item_operations package_details_item_ops = {
	.release = package_details_release,
};

static struct config_item_type package_appid_type = {
	.ct_item_ops	= &package_details_item_ops,
	.ct_attrs	= package_details_attrs,
	.ct_owner	= THIS_MODULE,
};

static inline struct extensions_value *to_extensions_value(
					struct config_item *item)
{
	return item ? container_of(to_config_group(item),
				struct extensions_value, group)
			: NULL;
}

static inline struct extension_details *to_extension_details(
					struct config_item *item)
{
	return item ? container_of(item, struct extension_details, item)
			: NULL;
}

#define EXTENSIONS_VALUE_ATTRIBUTE(name) (&extensions_value_attr_##name)

static void extension_details_release(struct config_item *item)
{
	struct extension_details *ed = to_extension_details(item);

	pr_debug("pkglist: No longer mapping %s files to gid %d\n",
				ed->name.name,
				from_kgid(current_user_ns(), ed->value->gid));
	remove_ext_gid_entry(ed);
	kfree(ed->name.name);
	kfree(ed);
}

static struct configfs_item_operations extension_details_item_ops = {
	.release = extension_details_release,
};

static ssize_t extensions_value_ext_gid_show(
			struct config_item *item, char *page)
{
	return scnprintf(page, PAGE_SIZE, "%u\n",
				from_kgid(current_user_ns(), to_extensions_value(item)->gid));
}

static ssize_t extensions_value_ext_gid_store(
				struct config_item *item,
				const char *page, size_t count)
{
	unsigned int tmp;
	int ret;

	ret = kstrtouint(page, 10, &tmp);
	if (ret)
		return ret;

	to_extensions_value(item)->gid = make_kgid(current_user_ns(), tmp);

	return count;
}

PACKAGE_DETAILS_ATTR(extensions_value_, ext_gid);

static struct configfs_attribute *extensions_value_attrs[] = {
	EXTENSIONS_VALUE_ATTRIBUTE(ext_gid),
	NULL,
};

static struct config_item_type extension_details_type = {
	.ct_item_ops = &extension_details_item_ops,
	.ct_owner = THIS_MODULE,
};

static struct config_item *extension_details_make_item(
				struct config_group *group, const char *name)
{
	struct extensions_value *extensions_value =
			to_extensions_value(&group->cg_item);
	struct extension_details *extension_details =
			kzalloc(sizeof(struct extension_details), GFP_KERNEL);
	const char *tmp;
	int ret;

	if (!extension_details)
		return ERR_PTR(-ENOMEM);

	tmp = kstrdup(name, GFP_KERNEL);
	if (!tmp) {
		kfree(extension_details);
		return ERR_PTR(-ENOMEM);
	}
	qstr_init(&extension_details->name, tmp);
	extension_details->value = extensions_value;
	ret = insert_ext_gid_entry(extension_details);

	if (ret) {
		kfree(extension_details->name.name);
		kfree(extension_details);
		return ERR_PTR(ret);
	}
	config_item_init_type_name(&extension_details->item, name,
					&extension_details_type);

	return &extension_details->item;
}

static struct configfs_group_operations extensions_value_group_ops = {
	.make_item = extension_details_make_item,
};

static struct config_item_type extensions_name_type = {
	.ct_attrs	= extensions_value_attrs,
	.ct_group_ops	= &extensions_value_group_ops,
	.ct_owner	= THIS_MODULE,
};

static struct config_group *extensions_make_group(struct config_group *group,
							const char *name)
{
	struct extensions_value *extensions_value;
	unsigned int tmp;
	int ret;

	extensions_value = kzalloc(sizeof(struct extensions_value), GFP_KERNEL);
	if (!extensions_value)
		return ERR_PTR(-ENOMEM);
	/* For legacy reasons, if the name is a number, assume it's the gid*/
	ret = kstrtouint(name, 10, &tmp);
	if (!ret)
		extensions_value->gid = make_kgid(current_user_ns(), tmp);

	config_group_init_type_name(&extensions_value->group, name,
						&extensions_name_type);
	return &extensions_value->group;
}

static void extensions_drop_group(struct config_group *group,
					struct config_item *item)
{
	struct extensions_value *value = to_extensions_value(item);

	pr_debug("pkglist: No longer mapping any files to gid %d\n",
			from_kgid(current_user_ns(), value->gid));
	kfree(value);
}

static struct configfs_group_operations extensions_group_ops = {
	.make_group	= extensions_make_group,
	.drop_item	= extensions_drop_group,
};

static struct config_item_type extensions_type = {
	.ct_group_ops	= &extensions_group_ops,
	.ct_owner	= THIS_MODULE,
};

static struct config_group extension_group = {
	.cg_item = {
		.ci_namebuf = "extensions",
		.ci_type = &extensions_type,
	},
};

struct packages {
	struct configfs_subsystem subsystem;
};

static inline struct packages *to_packages(struct config_item *item)
{
	return item ? container_of(
			to_configfs_subsystem(to_config_group(item)),
					struct packages, subsystem) : NULL;
}

static struct config_item *packages_make_item(struct config_group *group,
							const char *name)
{
	struct package_details *package_details;
	const char *tmp;

	package_details = kzalloc(sizeof(struct package_details), GFP_KERNEL);
	if (!package_details)
		return ERR_PTR(-ENOMEM);
	tmp = kstrdup(name, GFP_KERNEL);
	if (!tmp) {
		kfree(package_details);
		return ERR_PTR(-ENOMEM);
	}
	qstr_init(&package_details->name, tmp);
	config_item_init_type_name(&package_details->item, name,
						&package_appid_type);

	return &package_details->item;
}

static ssize_t packages_list_show(struct config_item *item, char *page)
{
	struct hashtable_entry *hash_cur_app;
	struct hashtable_entry *hash_cur_user;
	int i;
	int count = 0, written = 0;
	const char errormsg[] = "<truncated>\n";
	unsigned int hash;

	rcu_read_lock();
	hash_for_each_rcu(package_to_appid, i, hash_cur_app, hlist) {
		written = scnprintf(page + count,
				    PAGE_SIZE - sizeof(errormsg) - count,
				    "%s %d\n",
				    hash_cur_app->key.name,
				    atomic_read(&hash_cur_app->value));
		hash = hash_cur_app->key.hash;
		hash_for_each_possible_rcu(package_to_userid, hash_cur_user, hlist, hash) {
			if (qstr_case_eq(&hash_cur_app->key, &hash_cur_user->key)) {
				written += scnprintf(page + count + written - 1,
					PAGE_SIZE - sizeof(errormsg) - count - written + 1,
					" %d\n", atomic_read(&hash_cur_user->value)) - 1;
			}
		}
		if (count + written == PAGE_SIZE - sizeof(errormsg) - 1) {
			count += scnprintf(page + count, PAGE_SIZE - count, errormsg);
			break;
		}
		count += written;
	}
	rcu_read_unlock();

	return count;
}

static ssize_t packages_remove_userid_store(struct config_item *item,
					    const char *page, size_t count)
{
	unsigned int tmp;
	int ret;

	ret = kstrtouint(page, 10, &tmp);
	if (ret)
		return ret;
	remove_userid_all_entry(tmp);
	return count;
}

static struct configfs_attribute packages_attr_packages_gid_list = {
    .ca_name	= "packages_gid.list",
    .ca_mode	= S_IRUGO,
    .ca_owner	= THIS_MODULE,
    .show	= packages_list_show,
};
PACKAGE_DETAILS_ATTR_WO(packages_, remove_userid);

static struct configfs_attribute *packages_attrs[] = {
	&packages_attr_packages_gid_list,
	&packages_attr_remove_userid,
	NULL,
};

/*
 * Note that, since no extra work is required on ->drop_item(),
 * no ->drop_item() is provided.
 */
static struct configfs_group_operations packages_group_ops = {
	.make_item	= packages_make_item,
};

static struct config_item_type packages_type = {
	.ct_group_ops	= &packages_group_ops,
	.ct_attrs	= packages_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct config_group *sd_default_groups[] = {
	&extension_group,
	NULL,
};

static struct packages pkglist_packages = {
	.subsystem = {
		.su_group = {
			.cg_item = {
				.ci_type = &packages_type,
			},
		},
	},
};

static int configfs_pkglist_init(void)
{
	int ret, i;
	struct configfs_subsystem *subsys = &pkglist_packages.subsystem;
	config_item_set_name(&pkglist_packages.subsystem.su_group.cg_item,
						pkglist_config_location);
	config_group_init(&subsys->su_group);

	for (i = 0; sd_default_groups[i]; i++) {
		config_group_init(sd_default_groups[i]);
		configfs_add_default_group(sd_default_groups[i], &subsys->su_group);
	}
	mutex_init(&subsys->su_mutex);
	ret = configfs_register_subsystem(subsys);
	if (ret) {
		pr_err("Error %d while registering subsystem %s\n", ret,
				subsys->su_group.cg_item.ci_namebuf);
	}
	return ret;
}

static void configfs_pkglist_exit(void)
{
	configfs_unregister_subsystem(&pkglist_packages.subsystem);
}

void pkglist_register_update_listener(struct pkg_list *pkg)
{
	if (!pkg->update)
		return;
	mutex_lock(&pkg_list_lock);
	list_add(&pkg->list, &pkglist_listeners);
	mutex_unlock(&pkg_list_lock);
}
EXPORT_SYMBOL_GPL(pkglist_register_update_listener);

void pkglist_unregister_update_listener(struct pkg_list *pkg)
{
	mutex_lock(&pkg_list_lock);
	list_del(&pkg->list);
	mutex_unlock(&pkg_list_lock);
}
EXPORT_SYMBOL_GPL(pkglist_unregister_update_listener);

static int __init pkglist_init(void)
{
	hashtable_entry_cachep =
		kmem_cache_create("packagelist_hashtable_entry",
				sizeof(struct hashtable_entry), 0, 0, NULL);
	if (!hashtable_entry_cachep) {
		pr_err("pkglist: failed creating pkgl_hashtable entry slab cache\n");
		return -ENOMEM;
	}

	return configfs_pkglist_init();
}
module_init(pkglist_init);

static void __exit pkglist_exit(void)
{
	configfs_pkglist_exit();
	packagelist_destroy();
	kmem_cache_destroy(hashtable_entry_cachep);
}

module_exit(pkglist_exit);

MODULE_AUTHOR("Daniel Rosenberg, Google");
MODULE_DESCRIPTION("Configfs Pkglist implementation");
MODULE_LICENSE("GPL v2");
