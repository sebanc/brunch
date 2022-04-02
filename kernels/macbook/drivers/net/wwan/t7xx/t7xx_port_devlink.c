// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, MediaTek Inc.
 * Copyright (c) 2022, Intel Corporation.
 */

#include <linux/debugfs.h>
#include <linux/bitfield.h>
#include <linux/firmware.h>

#include "t7xx_hif_cldma.h"
#include "t7xx_pci_rescan.h"
#include "t7xx_port_devlink.h"
#include "t7xx_port_proxy.h"
#include "t7xx_state_monitor.h"
#include "t7xx_uevent.h"

int fastboot_dl_mode;
static struct t7xx_pci_dev *mtk_dev_global;
static struct dentry *debugfs_dir;

static struct t7xx_devlink_region_info
t7xx_devlink_region_list[T7XX_TOTAL_REGIONS] = {
	{"mr_dump", T7XX_MRDUMP_SIZE},
	{"lk_dump", T7XX_LKDUMP_SIZE},
};

static ssize_t t7xx_devlink_download_mode(struct file *filp,
					  const char __user *buffer,
					  size_t count,
					  loff_t *ppos)
{
	unsigned long val = 0;
	int ret;

	ret = kstrtoul_from_user(buffer, count, 10, &val);
	if (ret)
		return ret;
	if (val == 1) {
		fastboot_dl_mode = 1;
		mtk_queue_rescan_work(mtk_dev_global->pdev);
	} else if (val == 0) {
		fastboot_dl_mode = 0;
	} else {
		dev_err(&mtk_dev_global->pdev->dev,
			"unsupported download mode!");
		return -EINVAL;
	}
	return count;
}

static const struct file_operations devlink_fastboot_ops = {
	.open = simple_open,
	.write = t7xx_devlink_download_mode,
};

void t7xx_devlink_flash_debugfs_create(void)
{
	debugfs_dir = debugfs_create_dir("ccci_driver", NULL);
	debugfs_create_file("flashing", 0600, debugfs_dir, NULL,
			    &devlink_fastboot_ops);
}

void t7xx_devlink_flash_debugfs_remove(void)
{
	debugfs_remove_recursive(debugfs_dir);
}

static ssize_t t7xx_devlink_port_read(struct t7xx_port *port,
				      char *buf, size_t count)
{
	bool full_req_done = false;
	int ret = 0, read_len;
	struct sk_buff *skb;

	spin_lock_irq(&port->rx_wq.lock);
	if (skb_queue_empty(&port->rx_skb_list)) {
		ret = wait_event_interruptible_locked_irq(port->rx_wq,
					!skb_queue_empty(&port->rx_skb_list));
		if (ret == -ERESTARTSYS) {
			spin_unlock_irq(&port->rx_wq.lock);
			ret = -EINTR;
			goto read_err;
		}
	}

	skb = skb_peek(&port->rx_skb_list);

	if (count >= skb->len) {
		read_len = skb->len;
		full_req_done = true;
		__skb_unlink(skb, &port->rx_skb_list);
	} else {
		read_len = count;
	}
	spin_unlock_irq(&port->rx_wq.lock);

	memcpy(buf, skb->data, read_len);
	skb_pull(skb, read_len);
	if (full_req_done)
		dev_kfree_skb(skb);

	return ret ? ret : read_len;
read_err:
	return ret;
}

static ssize_t t7xx_devlink_port_write(struct t7xx_port *port,
				       const char *buf, size_t count)
{
	struct t7xx_port_static *port_static = port->port_static;
	size_t actual_count, txq_mtu;
	int i, ret, multi_packet = 1;
	struct cldma_ctrl *md_ctrl;
	struct sk_buff *skb;

	md_ctrl = port->t7xx_dev->md->md_ctrl[port_static->path_id];
	txq_mtu = cldma_txq_mtu(md_ctrl, port_static->txq_index);

	actual_count = count > txq_mtu ? txq_mtu : count;
	mutex_lock(&port->tx_mutex_lock);
	for (i = 0; i < multi_packet; i++) {
		if (multi_packet > 1 && multi_packet == i + 1)
			actual_count = count % txq_mtu;

		skb = __dev_alloc_skb(actual_count, GFP_KERNEL);
		if (!skb) {
			ret = -ENOMEM;
			mutex_unlock(&port->tx_mutex_lock);
			goto write_exit;
		}

		memcpy(skb_put(skb, actual_count),
		       buf + i * txq_mtu,
		       actual_count);

		/* send out */
		ret = t7xx_port_send_skb_to_md(port, skb);
		if (ret) {
			goto err_out;
		} else {
			/* Record the port seq_num after the data is sent to HIF.
			 * Only bits 0-14 are used, thus negating overflow.
			 */
			port->seq_nums[MTK_TX]++;
		}

		if (multi_packet == 1) {
			mutex_unlock(&port->tx_mutex_lock);
			ret = actual_count;
			goto write_exit;
		} else if (multi_packet == i + 1) {
			mutex_unlock(&port->tx_mutex_lock);
			ret = count;
			goto write_exit;
		}
	}

err_out:
	mutex_unlock(&port->tx_mutex_lock);
	dev_err(port->dev, "write error done on %s, size: %zu, ret: %d\n",
		port_static->name, actual_count, ret);
	dev_kfree_skb(skb);
write_exit:
	return ret;
}

static int t7xx_devlink_fb_handle_response(struct t7xx_port *port, int *data)
{
	int ret = 0, index = 0, return_data = 0, read_bytes = 0;
	char status[T7XX_FB_RESPONSE_SIZE + 1];

	while (index < T7XX_FB_RESP_COUNT) {
		index++;

		read_bytes = t7xx_devlink_port_read(port, status,
						    T7XX_FB_RESPONSE_SIZE);

		if (read_bytes < 0) {
			dev_err(port->dev, "status read failed");
			ret = -EIO;
			break;
		}
		status[read_bytes] = '\0';

		if (strncmp(status, T7XX_FB_RESP_INFO,
			    strlen(T7XX_FB_RESP_INFO)) == 0) {
			break;
		} else if (strncmp(status, T7XX_FB_RESP_OKAY,
				   strlen(T7XX_FB_RESP_OKAY)) == 0) {
			break;
		} else if (strncmp(status, T7XX_FB_RESP_FAIL,
				   strlen(T7XX_FB_RESP_FAIL)) == 0) {
			ret = -EPROTO;
			break;
		} else if (strncmp(status, T7XX_FB_RESP_DATA,
				   strlen(T7XX_FB_RESP_DATA)) == 0) {
			if (data) {
				if (!kstrtoint(status +
					       strlen(T7XX_FB_RESP_DATA),
					       16,
					       &return_data)) {
					*data = return_data;
				} else {
					dev_err(port->dev,
						"kstrtoint error!\n");
					ret = -EPROTO;
				}
			}
			break;
		} else {
			dev_err(port->dev, "UNKNOWN status in the response\n");
			ret = -EPROTO;
			break;
		}
	}
	return ret;
}

static int t7xx_devlink_fb_raw_command(char *cmd,
				       struct t7xx_port *port, int *data)
{
	int cmd_size = strnlen(cmd, T7XX_FB_CMD_SIZE);
	int ret = 0;

	if (cmd_size > T7XX_FB_COMMAND_SIZE) {
		ret = -EINVAL;
		dev_err(port->dev, "command length %d is long\n", cmd_size);
		goto err_out;
	}

	if (cmd_size != t7xx_devlink_port_write(port, cmd, cmd_size)) {
		dev_err(port->dev, "raw command = %s write failed\n", cmd);
		ret = -EIO;
		goto err_out;
	}

	dev_dbg(port->dev, "raw command = %s written to the device\n", cmd);

	ret = t7xx_devlink_fb_handle_response(port, data);
	if (ret)
		dev_err(port->dev, "raw command = %s response FAILURE:%d\n",
			cmd, ret);
err_out:
	return ret;
}

static int t7xx_devlink_fb_send_buffer(struct t7xx_port *port,
				       const u8 *buf, size_t size)
{
	size_t remaining = size, offset = 0, len;
	int write_done = 0, ret = 0;

	if (!size) {
		ret = -EINVAL;
		goto err_out;
	}

	while (remaining) {
		len = min_t(size_t, remaining, MTK_SKB_2K);
		write_done = t7xx_devlink_port_write(port, buf + offset, len);

		if (write_done < 0) {
			dev_err(port->dev, "write to device failed in %s",
				__func__);
			ret = -EIO;
			goto err_out;
		} else if (write_done != len) {
			dev_err(port->dev,
				"write Error. Only %d/%zu bytes written",
				write_done, len);
			ret = -EIO;
			goto err_out;
		}
		remaining -= len;
		offset += len;
		udelay(500);
	}

err_out:
	return ret;
}

static int t7xx_devlink_fb_download_command(struct t7xx_port *port, size_t size)
{
	char download_command[T7XX_FB_CMD_SIZE];

	snprintf(download_command, sizeof(download_command),
		 "%s:%08zx", T7XX_FB_CMD_DOWNLOAD, size);

	return t7xx_devlink_fb_raw_command(download_command, port, NULL);
}

static int t7xx_devlink_fb_download(struct t7xx_port *port,
				    const u8 *buf, size_t size)
{
	int ret = 0;

	if (size <= 0 || size > SIZE_MAX) {
		dev_err(port->dev, "file is too large to download");
		ret = -EINVAL;
		goto err_out;
	}

	ret = t7xx_devlink_fb_download_command(port, size);
	if (ret)
		goto err_out;

	ret = t7xx_devlink_fb_send_buffer(port, buf, size);
	if (ret)
		goto err_out;

	return t7xx_devlink_fb_handle_response(port, NULL);
err_out:
	return ret;
}

static int t7xx_devlink_fb_flash(const char *cmd, struct t7xx_port *port)
{
	char flash_command[T7XX_FB_CMD_SIZE];

	snprintf(flash_command, sizeof(flash_command),
		 "%s:%s", T7XX_FB_CMD_FLASH, cmd);

	return t7xx_devlink_fb_raw_command(flash_command, port, NULL);
}

static int t7xx_devlink_fb_flash_partition(const char *partition,
				const u8 *buf,
				struct t7xx_port *port,
				size_t size)
{
	int ret = 0;

	ret = t7xx_devlink_fb_download(port, buf, size);
	if (ret)
		return ret;

	return t7xx_devlink_fb_flash(partition, port);
}

static int t7xx_devlink_fb_get_core(struct t7xx_port *port)
{
	struct t7xx_devlink_region_info *mrdump_region = NULL;
	char mrdump_complete_event[T7XX_FB_EVENT_SIZE];
	u32 mrd_mb = T7XX_MRDUMP_SIZE / (1024 * 1024);
	struct t7xx_devlink *dl = port->dl;
	unsigned long long zipsize = 0;
	char mcmd[T7XX_FB_MCMD_SIZE];
	size_t clen, offset_dlen = 0;
	int dlen = 0, result = 0;
	char *mdata = NULL;

	set_bit(T7XX_MRDUMP_STATUS, &dl->status);

	mdata = kmalloc(T7XX_FB_MDATA_SIZE, GFP_KERNEL);
	if (!mdata) {
		result = -ENOMEM;
		goto get_core_exit;
	}

	mrdump_region = dl->dl_region_info[T7XX_MRDUMP_INDEX];
	mrdump_region->dump = vmalloc(mrdump_region->default_size);
	if (!mrdump_region->dump) {
		kfree(mdata);
		result = -ENOMEM;
		goto get_core_exit;
	}

	result = t7xx_devlink_fb_raw_command(T7XX_FB_CMD_OEM_MRDUMP,
					     port, NULL);
	if (result) {
		dev_err(port->dev, "%s command failed\n",
			T7XX_FB_CMD_OEM_MRDUMP);
		vfree(mrdump_region->dump);
		kfree(mdata);
		goto get_core_exit;
	}

	while (true) {
		clen = t7xx_devlink_port_read(port, mcmd, sizeof(mcmd));
		if (clen == strlen(T7XX_FB_CMD_RTS) &&
		    (!strncmp(mcmd, T7XX_FB_CMD_RTS,
			     strlen(T7XX_FB_CMD_RTS)))) {
			memset(mdata, 0, T7XX_FB_MDATA_SIZE);
			dlen = 0;

			memset(mcmd, 0, sizeof(mcmd));
			clen = snprintf(mcmd, sizeof(mcmd),
					"%s", T7XX_FB_CMD_CTS);

			if (t7xx_devlink_port_write(port, mcmd, clen) != clen) {
				dev_err(port->dev,
					"write for _CTS failed:%zd\n",
					clen);
				goto get_core_err;
			}

			dlen = t7xx_devlink_port_read(port, mdata,
						      T7XX_FB_MDATA_SIZE);
			if (dlen > 0) {
				zipsize += (unsigned long long)(dlen);
				if (offset_dlen >=
						mrdump_region->default_size) {
					dev_err(port->dev,
						"mrdump exceeds %uMB size. Discarded!",
						mrd_mb);
					t7xx_uevent_send(port->dev,
							 T7XX_UEVENT_MRD_DISCD);
					goto get_core_err;
				}
				memcpy(mrdump_region->dump +
				       offset_dlen, mdata, dlen);
				offset_dlen += dlen;
				memset(mcmd, 0, sizeof(mcmd));
				clen = snprintf(mcmd, sizeof(mcmd), "%s",
						T7XX_FB_CMD_FIN);

				if (t7xx_devlink_port_write(port, mcmd, clen) !=
				    (int)(clen)) {
					dev_err(port->dev,
						"%s: _FIN failed, (Read %05zd:%05llu)\n",
						__func__, clen, zipsize);
					goto get_core_err;
				}
			} else {
				dev_err(port->dev, "read data error(%d)\n",
					dlen);
				goto get_core_err;
			}

		} else if ((clen == strlen(T7XX_FB_RESP_MRDUMP_DONE)) &&
			  (!strncmp(mcmd, T7XX_FB_RESP_MRDUMP_DONE,
				    strlen(T7XX_FB_RESP_MRDUMP_DONE)))) {
			dev_dbg(port->dev, "%s! size:%zd\n",
				T7XX_FB_RESP_MRDUMP_DONE,
				offset_dlen);

			mrdump_region->actual_size = offset_dlen;

			snprintf(mrdump_complete_event,
				 sizeof(mrdump_complete_event),
				 "%s size=%zu",
				 T7XX_UEVENT_MRDUMP_READY,
				 offset_dlen);

			t7xx_uevent_send(dl->dev, mrdump_complete_event);

			kfree(mdata);
			result = 0;
			goto get_core_exit;
		} else {
			dev_err(port->dev,
				"getcore protocol error (read len %05zd)\n",
				clen);
			goto get_core_err;
		}
	}

get_core_err:
	kfree(mdata);
	vfree(mrdump_region->dump);
	clear_bit(T7XX_MRDUMP_STATUS, &dl->status);
	return -EPROTO;
get_core_exit:
	clear_bit(T7XX_MRDUMP_STATUS, &dl->status);
	return result;
}

static int t7xx_devlink_fb_dump_log(struct t7xx_port *port)
{
	struct t7xx_devlink_region_info *lkdump_region = NULL;
	char lkdump_complete_event[T7XX_FB_EVENT_SIZE];
	struct t7xx_devlink *dl = port->dl;
	size_t dlen, offset_dlen = 0;
	int datasize = 0, result = 0;
	u8 *data;

	set_bit(T7XX_LKDUMP_STATUS, &dl->status);
	result = t7xx_devlink_fb_raw_command(T7XX_FB_CMD_OEM_LKDUMP,
					     port,
					     &datasize);
	if (result) {
		dev_err(port->dev, "%s command returns failure\n",
			T7XX_FB_CMD_OEM_LKDUMP);
		goto dump_err;
	}

	lkdump_region = dl->dl_region_info[T7XX_LKDUMP_INDEX];
	if (datasize > lkdump_region->default_size) {
		dev_err(port->dev, "lkdump size is more than %dKB. Discarded!",
			T7XX_LKDUMP_SIZE / 1024);
		t7xx_uevent_send(dl->dev, T7XX_UEVENT_LKD_DISCD);
		result = -EPROTO;
		goto dump_err;
	}

	data = kzalloc(datasize, GFP_KERNEL);
	if (!data) {
		result = -EINVAL;
		goto dump_err;
	}

	lkdump_region->dump = vmalloc(lkdump_region->default_size);

	if (!lkdump_region->dump) {
		kfree(data);
		result = -EINVAL;
		goto dump_err;
	}

	while (datasize > 0) {
		dlen = t7xx_devlink_port_read(port, data, datasize);
		if (dlen > 0) {
			memcpy(lkdump_region->dump + offset_dlen,
			       data, dlen);
			datasize -= dlen;
			offset_dlen += dlen;
		}
	}
	dev_dbg(port->dev, "LKDUMP DONE! size:%zd\n", offset_dlen);
	lkdump_region->actual_size = offset_dlen;

	snprintf(lkdump_complete_event,
		 sizeof(lkdump_complete_event),
		 "%s size=%zu",
		 T7XX_UEVENT_LKDUMP_READY,
		 offset_dlen);

	t7xx_uevent_send(dl->dev, lkdump_complete_event);
	kfree(data);
	clear_bit(T7XX_LKDUMP_STATUS, &dl->status);
	return t7xx_devlink_fb_handle_response(port, NULL);
dump_err:
	clear_bit(T7XX_LKDUMP_STATUS, &dl->status);
	return result;
}

static int t7xx_devlink_flash_update(struct devlink *devlink,
				     struct devlink_flash_update_params *params,
				     struct netlink_ext_ack *extack)
{
	struct t7xx_devlink *dl = devlink_priv(devlink);
	const char *component = params->component;
	char flash_event[T7XX_FB_EVENT_SIZE];
	const struct firmware *fw = NULL;
	struct t7xx_port *port = NULL;
	int ret = 0;

	port = dl->port;

	if (port->dl->mode != T7XX_FB_DL_MODE) {
		dev_err(port->dev, "Modem is not in fastboot download mode!");
		ret = -EPERM;
		goto err_out;
	}

	if (dl->status != 0x0) {
		dev_err(port->dev, "Modem is busy!");
		ret = -EBUSY;
		goto err_out;
	}

	if (!component) {
		ret = -EINVAL;
		goto err_out;
	}

	set_bit(T7XX_FLASH_STATUS, &dl->status);
	devlink_flash_update_begin_notify(devlink);
	ret = request_firmware(&fw, params->file_name, devlink->dev);
	if (ret) {
		dev_dbg(port->dev, "requested firmware file not found");
		return ret;
	}

	dev_dbg(port->dev, "flash partition name:%s binary size:%zu\n",
		component, fw->size);

	ret = t7xx_devlink_fb_flash_partition(component,
					      fw->data,
					      port,
					      fw->size);

	if (ret) {
		devlink_flash_update_status_notify(devlink, "flashing failure!",
						   params->component,
						   1, fw->size);
		snprintf(flash_event,
			 sizeof(flash_event),
			 "%s for [%s]",
			 T7XX_UEVENT_FLASHING_FAILURE,
			 params->component);
	} else {
		devlink_flash_update_status_notify(devlink, "flashing success!",
						   params->component,
						   1, fw->size);
		snprintf(flash_event,
			 sizeof(flash_event),
			 "%s for [%s]",
			 T7XX_UEVENT_FLASHING_SUCCESS,
			 params->component);
	}
	devlink_flash_update_end_notify(devlink);
	t7xx_uevent_send(dl->dev, flash_event);
err_out:
	clear_bit(T7XX_FLASH_STATUS, &dl->status);
	return ret;
}

/* Call back function for devlink ops */
static const struct devlink_ops devlink_flash_ops = {
	.supported_flash_update_params = DEVLINK_SUPPORT_FLASH_UPDATE_COMPONENT,
	.flash_update = t7xx_devlink_flash_update,
};

static int t7xx_devlink_region_snapshot(struct devlink *dl,
					const struct devlink_region_ops *ops,
					struct netlink_ext_ack *extack,
					u8 **data)
{
	struct t7xx_devlink_region_info *region_info = ops->priv;
	struct t7xx_devlink *t7xx_dl = devlink_priv(dl);
	u8 *snapshot_mem = NULL;
	int ret = 0;

	if (t7xx_dl->status != 0x0) {
		dev_err(t7xx_dl->dev, "Modem is busy!");
		ret = -EBUSY;
		goto snapshot_err;
	}

	dev_dbg(t7xx_dl->dev, "accessed devlink region:%s index:%d",
		ops->name, region_info->entry);

	if (strncmp(ops->name,"mr_dump",7) == 0) {
		if (!region_info->dump) {
			dev_err(t7xx_dl->dev,
				"devlink region:%s dump memory is not valid!",
				region_info->region_name);
			ret = -ENOMEM;
			goto snapshot_err;
		}
		snapshot_mem = vmalloc(region_info->default_size);

		if (!snapshot_mem) {
			dev_err(t7xx_dl->dev,
				"devlink region:%s snapshot allocation failed!",
				region_info->region_name);
			ret = -ENOMEM;
			goto snapshot_err;
		}
		memcpy(snapshot_mem,
		       region_info->dump,
		       region_info->default_size);
		*data = snapshot_mem;
	} else if (strncmp(ops->name, "lk_dump", 7) == 0) {
		ret = t7xx_devlink_fb_dump_log(t7xx_dl->port);
		if (ret)
			goto snapshot_err;
		*data = region_info->dump;
	}

snapshot_err:
	return ret;
}

/* To create regions for dump files */
static int t7xx_devlink_create_region(struct t7xx_devlink *dl)
{
	struct devlink_region_ops *region_ops;
	int rc = 0, i;

	region_ops = dl->dl_region_ops;
	for (i = 0; i < T7XX_TOTAL_REGIONS; i++) {
		region_ops[i].name = t7xx_devlink_region_list[i].region_name;
		region_ops[i].snapshot = t7xx_devlink_region_snapshot;
		region_ops[i].destructor = vfree;
		dl->dl_region[i] =
		devlink_region_create(dl->dl_ctx,
				      &region_ops[i], T7XX_MAX_SNAPSHOTS,
				      t7xx_devlink_region_list[i].default_size);

		if (IS_ERR(dl->dl_region[i])) {
			rc = PTR_ERR(dl->dl_region[i]);
			dev_err(dl->dev, "devlink region fail,err %d", rc);

			for ( ; i >= 0; i--)
				devlink_region_destroy(dl->dl_region[i]);
			goto region_create_fail;
		}
		t7xx_devlink_region_list[i].entry = i;
		region_ops[i].priv = t7xx_devlink_region_list + i;
	}
region_create_fail:
	return rc;
}

/* To Destroy devlink regions */
static void t7xx_devlink_destroy_region(struct t7xx_devlink *dl)
{
	u8 i;

	for (i = 0; i < T7XX_TOTAL_REGIONS; i++)
		devlink_region_destroy(dl->dl_region[i]);
}

/**
 * t7xx_devlink_region_init - Initialize/register devlink to CCCI driver
 * @port:   Pointer to port structure
 * @dw: Pointer to devlink work structure
 * @wq: Pointer to devlink workqueue structure
 *
 * Returns: Pointer to t7xx_devlink on success and NULL on failure
 */
static struct t7xx_devlink
*t7xx_devlink_region_init(struct t7xx_port *port, struct t7xx_devlink_work *dw,
			  struct workqueue_struct *wq)
{
	struct t7xx_pci_dev *mtk_dev = port->t7xx_dev;
	struct t7xx_devlink *dl;
	struct devlink *dl_ctx;
	int rc, i;

	dl_ctx = devlink_alloc(&devlink_flash_ops,
			       sizeof(struct t7xx_devlink));
	if (!dl_ctx) {
		dev_err(&mtk_dev->pdev->dev, "devlink_alloc failed");
		goto devlink_alloc_fail;
	}

	mtk_dev_global = mtk_dev;
	dl = devlink_priv(dl_ctx);
	dl->dl_ctx = dl_ctx;
	dl->mtk_dev = mtk_dev;
	dl->dev = &mtk_dev->pdev->dev;
	dl->mode = T7XX_FB_NO_MODE;
	dl->status = 0x0;
	dl->dl_work = dw;
	dl->dl_wq = wq;
	for (i = 0; i < T7XX_TOTAL_REGIONS; i++) {
		dl->dl_region_info[i] = &t7xx_devlink_region_list[i];
		dl->dl_region_info[i]->dump = NULL;
	}
	dl->port = port;
	port->dl = dl;

	devlink_register(dl_ctx, &mtk_dev->pdev->dev);
	rc = t7xx_devlink_create_region(dl);
	if (rc) {
		dev_err(dl->dev, "devlink region creation failed, rc %d",
			rc);
		goto region_create_fail;
	}

	return dl;

region_create_fail:
	devlink_free(dl_ctx);
devlink_alloc_fail:
	return NULL;
}

/**
 * t7xx_devlink_region_deinit - To unintialize the devlink from T7XX driver.
 * @dl:        Devlink instance
 */
static void t7xx_devlink_region_deinit(struct t7xx_devlink *dl)
{
	struct devlink *dl_ctx = dl->dl_ctx;

	dl->mode = T7XX_FB_NO_MODE;
	devlink_unregister(dl_ctx);
	t7xx_devlink_destroy_region(dl);
	devlink_free(dl_ctx);
}

static void t7xx_devlink_work_handler(struct work_struct *data)
{
	struct t7xx_devlink_work *dl_work;

	dl_work = container_of(data, struct t7xx_devlink_work, work);

	t7xx_devlink_fb_get_core(dl_work->port);
}

static int t7xx_devlink_init(struct t7xx_port *port)
{
	struct t7xx_port_static *port_static = port->port_static;
	struct t7xx_devlink_work *dl_work;
	struct workqueue_struct *wq;
	int ret;

	dl_work = kmalloc(sizeof(*dl_work), GFP_KERNEL);

	if (!dl_work) {
		ret = -ENOMEM;
		goto err_out;
	}

	mutex_init(&port->tx_mutex_lock);

	wq = create_workqueue("t7xx_devlink");

	if (!wq) {
		kfree(dl_work);
		dev_err(port->dev, "create_workqueue failed\n");
		ret = -ENODATA;
		goto err_out;
	}

	INIT_WORK(&dl_work->work, t7xx_devlink_work_handler);

	dl_work->port = port;
	port->rx_length_th = T7XX_MAX_QUEUE_LENGTH;

	if (port_static->rx_ch == PORT_CH_UART2_RX)
		port->flags |= PORT_F_RX_CH_TRAFFIC;
	else if (port_static->rx_ch == CCCI_FS_RX)
		port->flags |= (PORT_F_RX_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);

	t7xx_devlink_region_init(port, dl_work, wq);

	return 0;
err_out:
	return ret;
}

static void t7xx_devlink_uninit(struct t7xx_port *port)
{
	struct t7xx_devlink *dl = port->dl;
	struct sk_buff *skb = NULL;
	unsigned long flags;

	if (dl->dl_region_info[T7XX_MRDUMP_INDEX]->dump)
		vfree(dl->dl_region_info[T7XX_MRDUMP_INDEX]->dump);

	t7xx_devlink_region_deinit(port->dl);

	if (dl->dl_wq)
		destroy_workqueue(dl->dl_wq);

	kfree(dl->dl_work);

	spin_lock_irqsave(&port->rx_skb_list.lock, flags);
	while ((skb = __skb_dequeue(&port->rx_skb_list)) != NULL)
		dev_kfree_skb(skb);

	spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
}

static int t7xx_devlink_enable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_ENABLE;
	spin_unlock(&port->port_update_lock);

	if (port->dl->dl_wq && (port->dl->mode == T7XX_FB_DUMP_MODE)) {
		queue_work(port->dl->dl_wq,
			   &port->dl->dl_work->work);
	}

	return 0;
}

static int t7xx_devlink_disable_chl(struct t7xx_port *port)
{
	spin_lock(&port->port_update_lock);
	port->chan_enable = CCCI_CHAN_DISABLE;
	spin_unlock(&port->port_update_lock);

	return 0;
}

struct port_ops devlink_port_ops = {
	.init = &t7xx_devlink_init,
	.recv_skb = &t7xx_port_recv_skb,
	.uninit = &t7xx_devlink_uninit,
	.enable_chl = &t7xx_devlink_enable_chl,
	.disable_chl = &t7xx_devlink_disable_chl,
};

