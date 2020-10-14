// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) 2018, Linux Foundation.

#include <linux/configfs.h>
#include <linux/err.h>
#include <linux/string.h>

#include "ufs.h"
#include "ufshcd.h"

static inline struct ufs_hba *config_item_to_hba(struct config_item *item)
{
	struct config_group *group = to_config_group(item);
	struct configfs_subsystem *subsys = to_configfs_subsystem(group);
	struct ufs_hba *hba = container_of(subsys, struct ufs_hba, subsys);

	return hba;
}

static ssize_t ufs_config_desc_show(struct config_item *item, char *buf,
				    u8 index)
{
	struct ufs_hba *hba = config_item_to_hba(item);
	u8 *desc_buf = NULL;
	int desc_buf_len = hba->desc_size.conf_desc;
	int i, ret, curr_len = 0;

	desc_buf = kzalloc(desc_buf_len, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_CONFIGURATION, index,
					    0, desc_buf, &desc_buf_len);
	if (ret)
		goto out;

	for (i = 0; i < desc_buf_len; i++)
		curr_len += snprintf((buf + curr_len), (PAGE_SIZE - curr_len),
				"0x%x ", desc_buf[i]);

out:
	kfree(desc_buf);
	return ret ? ret : curr_len;
}

ssize_t ufshcd_desc_configfs_store(struct config_item *item, const char *buf,
				   size_t count, u8 index)
{
	char *strbuf;
	char *strbuf_copy;
	struct ufs_hba *hba = config_item_to_hba(item);
	u8 *desc_buf = NULL;
	int desc_buf_len = hba->desc_size.conf_desc;
	char *token;
	int i, ret;
	u8 value;
	u32 config_desc_lock = 0;

	desc_buf = kzalloc(desc_buf_len, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;

	strbuf = kstrdup(buf, GFP_KERNEL);
	if (!strbuf) {
		ret = -ENOMEM;
		goto out;
	}

	strbuf_copy = strbuf;

	/* Just return if bConfigDescrLock is already set */
	ret = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				QUERY_ATTR_IDN_CONF_DESC_LOCK, 0, 0,
				&config_desc_lock);
	if (ret)
		goto out;

	if (config_desc_lock) {
		dev_err(hba->dev, "%s: bConfigDescrLock already set to %u, cannot re-provision device!\n",
			__func__, config_desc_lock);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * First read the current configuration descriptor
	 * and then update with user provided parameters
	 */
	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_CONFIGURATION, index,
					    0, desc_buf, &desc_buf_len);
	if (ret)
		goto out;

	for (i = 0; i < desc_buf_len; i++) {
		token = strsep(&strbuf_copy, " ");
		if (!token)
			break;

		ret = kstrtou8(token, 0, &value);
		if (ret) {
			dev_err(hba->dev, "%s: Invalid value %s writing UFS configuration descriptor %u\n",
				__func__, token, index);
			ret = -EINVAL;
			goto out;
		}
		desc_buf[i] = value;
	}

	/* Write configuration descriptor to provision ufs */
	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_WRITE_DESC,
					    QUERY_DESC_IDN_CONFIGURATION, index,
					    0, desc_buf, &desc_buf_len);
	if (!ret)
		dev_info(hba->dev, "%s: UFS Configuration Descriptor %u written\n",
			 __func__, index);

out:
	kfree(strbuf);
	kfree(desc_buf);
	return ret ? ret : count;
}

static ssize_t ufs_config_desc_0_show(struct config_item *item, char *buf)
{
	return ufs_config_desc_show(item, buf, 0);
}

static ssize_t ufs_config_desc_0_store(struct config_item *item,
				       const char *buf, size_t count)
{
	return ufshcd_desc_configfs_store(item, buf, count, 0);
}

static ssize_t ufs_config_desc_1_show(struct config_item *item, char *buf)
{
	return ufs_config_desc_show(item, buf, 1);
}

static ssize_t ufs_config_desc_1_store(struct config_item *item,
				       const char *buf, size_t count)
{
	return ufshcd_desc_configfs_store(item, buf, count, 1);
}

static ssize_t ufs_config_desc_2_show(struct config_item *item, char *buf)
{
	return ufs_config_desc_show(item, buf, 2);
}

static ssize_t ufs_config_desc_2_store(struct config_item *item,
				       const char *buf, size_t count)
{
	return ufshcd_desc_configfs_store(item, buf, count, 2);
}

static ssize_t ufs_config_desc_3_show(struct config_item *item, char *buf)
{
	return ufs_config_desc_show(item, buf, 3);
}

static ssize_t ufs_config_desc_3_store(struct config_item *item,
				       const char *buf, size_t count)
{
	return ufshcd_desc_configfs_store(item, buf, count, 3);
}

static struct configfs_attribute ufshcd_attr_provision_0 = {
	.ca_name	= "ufs_config_desc_0",
	.ca_mode	= 0644,
	.ca_owner	= THIS_MODULE,
	.show		= ufs_config_desc_0_show,
	.store		= ufs_config_desc_0_store,
};

static struct configfs_attribute ufshcd_attr_provision_1 = {
	.ca_name	= "ufs_config_desc_1",
	.ca_mode	= 0644,
	.ca_owner	= THIS_MODULE,
	.show		= ufs_config_desc_1_show,
	.store		= ufs_config_desc_1_store,
};

static struct configfs_attribute ufshcd_attr_provision_2 = {
	.ca_name	= "ufs_config_desc_2",
	.ca_mode	= 0644,
	.ca_owner	= THIS_MODULE,
	.show		= ufs_config_desc_2_show,
	.store		= ufs_config_desc_2_store,
};

static struct configfs_attribute ufshcd_attr_provision_3 = {
	.ca_name	= "ufs_config_desc_3",
	.ca_mode	= 0644,
	.ca_owner	= THIS_MODULE,
	.show		= ufs_config_desc_3_show,
	.store		= ufs_config_desc_3_store,
};

static struct configfs_attribute *ufshcd_attrs[] = {
	&ufshcd_attr_provision_0,
	&ufshcd_attr_provision_1,
	&ufshcd_attr_provision_2,
	&ufshcd_attr_provision_3,
	NULL,
};

static struct config_item_type ufscfg_type = {
	.ct_attrs	= ufshcd_attrs,
	.ct_owner	= THIS_MODULE,
};

void ufshcd_configfs_init(struct ufs_hba *hba, const char *name)
{
	int ret;
	struct config_item *cg_item;
	struct configfs_subsystem *subsys;

	cg_item = &hba->subsys.su_group.cg_item;
	snprintf(cg_item->ci_namebuf, CONFIGFS_ITEM_NAME_LEN, "%s", name);
	cg_item->ci_type = &ufscfg_type;

	subsys = &hba->subsys;
	config_group_init(&subsys->su_group);
	mutex_init(&subsys->su_mutex);
	ret = configfs_register_subsystem(subsys);
	if (ret)
		pr_err("Error %d while registering subsystem %s\n",
		       ret,
		       subsys->su_group.cg_item.ci_namebuf);
}

void ufshcd_configfs_exit(struct ufs_hba *hba)
{
	configfs_unregister_subsystem(&hba->subsys);
}
