// SPDX-License-Identifier: GPL-2.0
/*
 * Thunderbolt/USB4 retimer support.
 *
 * Copyright (C) 2020, Intel Corporation
 * Authors: Kranthi Kuntala <kranthi.kuntala@intel.com>
 *	    Mika Westerberg <mika.westerberg@linux.intel.com>
 */

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/pm_runtime.h>
#include <linux/sched/signal.h>

#include "sb_regs.h"
#include "tb.h"

#define TB_MAX_RETIMER_INDEX	6

#ifdef CONFIG_ACPI

static const guid_t retimer_dsm_guid =
	GUID_INIT(0x61788900, 0xC470, 0x42BB,
		  0x80, 0xF0, 0x23, 0xA3, 0x13, 0x86, 0x45, 0x93);

#define RETIMER_DSM_REVID			1
#define RETIMER_DSM_FN_QUERY			0
#define RETIMER_DSM_FN_QUERY_POWER_STATE	1
#define RETIMER_DSM_FN_SET_POWER_STATE		2
#define RETIMER_DSM_FN_GET_RETIMER_INFO		3
#define RETIMER_DSM_FN_SET_RETIMER_INFO		4

#define RETIMER_DSM_FN_MINIMUM	(BIT(RETIMER_DSM_FN_QUERY) | \
				 BIT(RETIMER_DSM_FN_QUERY_POWER_STATE) | \
				 BIT(RETIMER_DSM_FN_SET_POWER_STATE) | \
				 BIT(RETIMER_DSM_FN_GET_RETIMER_INFO) | \
				 BIT(RETIMER_DSM_FN_SET_RETIMER_INFO))

#define RETIMER_OP_DELAY_MS	250

static int tb_retimer_acpi_dsm_query_fn(struct tb_switch *sw, u32 *data)
{
	union acpi_object *obj;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw) || !data)
		return -EINVAL;

	obj = acpi_evaluate_dsm(ACPI_HANDLE(&sw->tb->nhi->pdev->dev),
				&retimer_dsm_guid, RETIMER_DSM_REVID,
				RETIMER_DSM_FN_QUERY, NULL);
	if (!obj)
		return -EIO;

	*data = (u32)obj->buffer.pointer[0];
	ACPI_FREE(obj);

	return 0;
}

static int tb_retimer_acpi_dsm_get_power_state(struct tb_switch *sw, u8 *data)
{
	union acpi_object *obj;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw) || !data)
		return -EINVAL;

	obj = acpi_evaluate_dsm(ACPI_HANDLE(&sw->tb->nhi->pdev->dev),
				&retimer_dsm_guid, RETIMER_DSM_REVID,
				RETIMER_DSM_FN_QUERY_POWER_STATE, NULL);
	if (!obj)
		return -EIO;

	*data = (u8)obj->integer.value;
	ACPI_FREE(obj);
	return 0;
}

static int tb_retimer_acpi_dsm_set_power_state(struct tb_switch *sw, bool on)
{
	union acpi_object *obj, tmp, argv4 = ACPI_INIT_DSM_ARGV4(1, &tmp);

	tmp.type = ACPI_TYPE_INTEGER;
	tmp.integer.value = on;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw))
		return -EINVAL;

	obj = acpi_evaluate_dsm(ACPI_HANDLE(&sw->tb->nhi->pdev->dev),
				&retimer_dsm_guid, RETIMER_DSM_REVID,
				RETIMER_DSM_FN_SET_POWER_STATE, &argv4);
	if (!obj)
		return -EIO;

	ACPI_FREE(obj);
	return 0;
}

static int tb_retimer_acpi_dsm_get_retimer_info(struct tb_switch *sw, u8 *data)
{
	union acpi_object *obj;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw) || !data)
		return -EINVAL;

	obj = acpi_evaluate_dsm(ACPI_HANDLE(&sw->tb->nhi->pdev->dev),
				&retimer_dsm_guid, RETIMER_DSM_REVID,
				RETIMER_DSM_FN_GET_RETIMER_INFO, NULL);
	if (!obj)
		return -EIO;

	*data = (u8)obj->integer.value;

	ACPI_FREE(obj);
	return 0;
}

static int tb_retimer_acpi_dsm_set_retimer_info(struct tb_switch *sw, u8 data)
{
	union acpi_object *obj, tmp, argv4 = ACPI_INIT_DSM_ARGV4(1, &tmp);

	tmp.type = ACPI_TYPE_INTEGER;
	tmp.integer.value = data;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw))
		return -EINVAL;

	obj = acpi_evaluate_dsm(ACPI_HANDLE(&sw->tb->nhi->pdev->dev),
				&retimer_dsm_guid, RETIMER_DSM_REVID,
				RETIMER_DSM_FN_SET_RETIMER_INFO, &argv4);
	if (!obj)
		return -EIO;

	ACPI_FREE(obj);
	return 0;
}

static int tb_retimer_wait_for_value(struct tb_switch *sw, u32 value, u32 *result,
			      int timeout_msec, bool match)
{
	ktime_t timeout = ktime_add_ms(ktime_get(), timeout_msec);
	u8 data;
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw) || !result)
		return -EINVAL;

	*result = 0;

	do {
		data = 0;

		usleep_range(20000, 21000);

		/* Return if unsupported or supplied with invalid parameters */
		ret = tb_retimer_acpi_dsm_get_retimer_info(sw, &data);
		if (ret)
			return ret;

		if (match && data != USB_RETIMER_FW_UPDATE_INVALID_MUX) {
			/*
			 * If the expected value is 0, check bit 0 for a match.
			 * If it is non-zero, then do the normal check
			 */
			if ((!value && !(data & BIT(0))) ||
			    (value && ((data & value) == value))) {
				*result = data;
				return 0;
			}
		} else if (!match && data != value) {
			*result = data;
			return 0;
		}

	} while (ktime_before(ktime_get(), timeout));

	return -ETIMEDOUT;
}

static int tb_retimer_acpi_dsm_force_power(struct tb_switch *sw, bool on)
{
	u8 data = 0;
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw))
		return -EINVAL;

	ret = tb_retimer_acpi_dsm_set_power_state(sw, on);
	if (ret)
		return ret;

	/* This delay is required for the retimer to power on and be ready */
	if (on)
		msleep(50);

	ret = tb_retimer_acpi_dsm_get_power_state(sw, &data);
	if (ret)
		return ret;

	return on == data ? 0 : -EIO;
}

static int __maybe_unused tb_retimer_acpi_dsm_suspend_pd(struct tb_switch *sw,
							 bool suspend,
							 u8 typec_port_index)
{
	u32 result;
	u8 data;
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw))
		return -EINVAL;

	/* suspend / resume the PD */
	data = ((suspend ? USB_RETIMER_FW_UPDATE_SUSPEND_PD :
		USB_RETIMER_FW_UPDATE_RESUME_PD)
		<< USB_RETIMER_FW_UPDATE_OP_SHIFT) |
		typec_port_index;
	ret = tb_retimer_acpi_dsm_set_retimer_info(sw, data);
	if (ret)
		return ret;

	/* Check status */
	return tb_retimer_wait_for_value(sw, !suspend, &result,
					 RETIMER_OP_DELAY_MS, true);
}

static int __maybe_unused tb_retimer_acpi_dsm_get_mux(struct tb_switch *sw,
						      u32 *result,
						      u8 typec_port_index)
{
	u8 data;
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw) || !result)
		return -EINVAL;

	data = (USB_RETIMER_FW_UPDATE_GET_MUX << USB_RETIMER_FW_UPDATE_OP_SHIFT) |
	       typec_port_index;
	ret = tb_retimer_acpi_dsm_set_retimer_info(sw, data);
	if (ret)
		return ret;

	/* Check status */
	*result = USB_RETIMER_FW_UPDATE_INVALID_MUX;
	return tb_retimer_wait_for_value(sw, USB_RETIMER_FW_UPDATE_INVALID_MUX,
					 result, RETIMER_OP_DELAY_MS, false);
}

static int tb_retimer_acpi_dsm_set_mux(struct tb_switch *sw, u8 mux_mode,
				       u32 match, u8 typec_port_index)
{
	u32 result;
	u8 data;
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw))
		return -EINVAL;

	if (mux_mode > USB_RETIMER_FW_UPDATE_DISCONNECT)
		return -EINVAL;

	data = (mux_mode << USB_RETIMER_FW_UPDATE_OP_SHIFT) | typec_port_index;
	ret = tb_retimer_acpi_dsm_set_retimer_info(sw, data);
	if (ret)
		return ret;

	return tb_retimer_wait_for_value(sw, match, &result,
					 RETIMER_OP_DELAY_MS, true);
}

static int __maybe_unused tb_retimer_acpi_dsm_get_port_info(struct tb_switch *sw,
							    u32 *result)
{
	u8 data;
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw) || !result)
		return -EINVAL;

	data = USB_RETIMER_FW_UPDATE_QUERY_PORT << USB_RETIMER_FW_UPDATE_OP_SHIFT;
	ret = tb_retimer_acpi_dsm_set_retimer_info(sw, data);
	if (ret)
		return ret;

	/* Check status */
	return tb_retimer_wait_for_value(sw, 0, result,
					 RETIMER_OP_DELAY_MS, false);
}

static int __maybe_unused tb_retimer_enter_tbt_alt_mode(struct tb_switch *sw,
							u8 typec_port_index)
{
	int ret;

	/* Only onboard retimers supported now */
	if (!sw || tb_route(sw))
		return -EINVAL;

	/* CONNECT mode */
	ret = tb_retimer_acpi_dsm_set_mux(sw, USB_RETIMER_FW_UPDATE_SET_USB,
					  USB_PD_MUX_USB_ENABLED,
					  typec_port_index);
	if (ret)
		goto err_connect;

	/* SAFE mode */
	ret = tb_retimer_acpi_dsm_set_mux(sw, USB_RETIMER_FW_UPDATE_SET_SAFE,
					  USB_PD_MUX_SAFE_MODE,
					  typec_port_index);
	if (ret)
		goto err_disconnect;

	/* TBT ALT mode */
	ret = tb_retimer_acpi_dsm_set_mux(sw, USB_RETIMER_FW_UPDATE_SET_TBT,
					  USB_PD_MUX_TBT_COMPAT_ENABLED,
					  typec_port_index);
	if (ret)
		goto err_disconnect;

	return 0;

err_disconnect:
	tb_retimer_acpi_dsm_set_mux(sw, USB_RETIMER_FW_UPDATE_DISCONNECT,
				    USB_PD_MUX_NONE, typec_port_index);
err_connect:
	return ret;
}

/**
 * tb_retimer_start_io() - Prepare the USB4 retimer to start IO
 * @sw: USB4 switch
 * @mux_mode: Returns the current USB4 retimer's mux mode
 * @typec_port_index: Type-C port index associated with the USB4 port
 * @port: USB4 port associated with this retimer
 *
 * This function queries the support of this functionality and if present
 * suspends the PD and puts the retimer in TBT ALT mode as required.
 * Also powers on the retimer, if no devices are connected.
 * Returns the result of this operation
 */
static int __maybe_unused tb_retimer_start_io(struct tb_switch *sw,
					      u32 *mux_mode,
					      u8 typec_port_index,
					      struct tb_port *port)
{
	u32 data = 0;
	int ret;

	if (!sw || !mux_mode || typec_port_index >= EC_USB_PD_MAX_PORTS ||
	    !port)
		return -EINVAL;

	/* Limit this only to on-board retimers */
	if (tb_route(sw))
		return -ENOTSUPP;

	if (port->rt_io_started)
		return 0;

	/* check for minimum supported functions */
	ret = tb_retimer_acpi_dsm_query_fn(sw, &data);
	if (ret)
		return ret;

	if (data != RETIMER_DSM_FN_MINIMUM)
		return -ENOTSUPP;

	/* Read the current mux mode */
	ret = tb_retimer_acpi_dsm_get_mux(sw, mux_mode, typec_port_index);
	if (ret)
		return ret;

	/*
	 * For all device attached cases, exit.
	 * When the Type-C charger is connected in reverse,
	 * USB_PD_MUX_POLARITY_INVERTED bit will be set. Otherwise, it
	 * is just USB_PD_MUX_NONE.
	 */
	if (*mux_mode != USB_PD_MUX_NONE &&
	    *mux_mode != USB_PD_MUX_POLARITY_INVERTED)
		goto out;

	/* Suspend the PD */
	ret = tb_retimer_acpi_dsm_suspend_pd(sw, true, typec_port_index);
	if (ret)
		return ret;

	ret = tb_retimer_acpi_dsm_force_power(sw, true);
	if (ret)
		goto err_pd_resume;

	ret = tb_retimer_enter_tbt_alt_mode(sw, typec_port_index);
	if (ret)
		goto err_force_power_off;
out:
	port->rt_io_started = true;
	return 0;

err_force_power_off:
	tb_retimer_acpi_dsm_force_power(sw, false);
err_pd_resume:
	tb_retimer_acpi_dsm_suspend_pd(sw, false, typec_port_index);
	return ret;
}

/**
 * tb_retimer_stop_io() - Prepare the USB4 retimer to start IO
 * @sw: USB4 switch
 * @mux_mode: USB4 retimer's mux mode before starting IO
 * @typec_port_index: Type-C port index associated with the USB4 port
 * @port: USB4 port associated with this retimer
 *
 * This function undoes what was done inside tb_retimer_start_io().
 * Additionally unsets the sbtx and exits router offline mode, if the
 * originally no devices were connected prior to tb_retimer_start_io().
 * Returns the result of this operation
 */
static int __maybe_unused tb_retimer_stop_io(struct tb_switch *sw, u32 mux_mode,
					     u8 typec_port_index,
					     struct tb_port *port)
{
	int ret, i;

	if (!sw || !port || typec_port_index >= EC_USB_PD_MAX_PORTS)
		return -EINVAL;

	/* Limit this only to on-board retimers */
	if (tb_route(sw))
		return 0;

	if (!port->rt_io_started)
		return 0;

	if (mux_mode != USB_PD_MUX_NONE &&
	    mux_mode != USB_PD_MUX_POLARITY_INVERTED)
		goto out;

	for (i = 1; i <= TB_MAX_RETIMER_INDEX; i++)
		usb4_port_set_inbound_sbtx(port, i, false);

	ret = usb4_port_router_offline(port, true);

	ret = tb_retimer_acpi_dsm_set_mux(sw, USB_RETIMER_FW_UPDATE_DISCONNECT,
					  USB_PD_MUX_NONE, typec_port_index);
	if (ret)
		return ret;

	ret = tb_retimer_acpi_dsm_suspend_pd(sw, false, typec_port_index);
	if (ret)
		return ret;

	ret = tb_retimer_acpi_dsm_force_power(sw, false);
	if (ret)
		return ret;
out:
	port->rt_io_started = false;
	return 0;
}

#else

static int tb_retimer_acpi_dsm_get_port_info(struct tb_switch *sw, u32 *result)
{
	return -EOPNOTSUPP;
}

static int __maybe_unused tb_retimer_start_io(struct tb_switch *sw,
					      u32 *mux_mode,
					      u8 typec_port_index,
					      struct tb_port *port)
{
	return -EOPNOTSUPP;
}

static int __maybe_unused tb_retimer_stop_io(struct tb_switch *sw, u32 mux_mode,
					     u8 typec_port_index,
					     struct tb_port *port)
{
	return -EOPNOTSUPP;
}

#endif /* CONFIG_ACPI */

static void tb_retimer_get_devs(struct tb_port *port)
{
	if (!port)
		return;

	/* Bring the domain back from sleep if it was suspended */
	pm_runtime_get_sync(&port->sw->tb->dev);

	mutex_lock(&port->sw->tb->lock);
	tb_switch_get(port->sw);

	pm_runtime_get_sync(&port->sw->dev);
}

static void tb_retimer_put_devs(struct tb_port *port)
{
	if (!port)
		return;

	pm_runtime_mark_last_busy(&port->sw->dev);
	pm_runtime_put_autosuspend(&port->sw->dev);

	tb_switch_put(port->sw);
	mutex_unlock(&port->sw->tb->lock);

	pm_runtime_mark_last_busy(&port->sw->tb->dev);
	pm_runtime_put_autosuspend(&port->sw->tb->dev);
}

/**
 * tb_retimer_stop_io_delayed() - stop retimer io
 * @work: work that needs to be done
 * Executes on tb->wq.
 */
static void tb_retimer_stop_io_delayed(struct work_struct *work)
{
	struct tb_port *port = container_of(work, typeof(*port),
					    retimer_stop_io_work.work);

	/* Only onboard retimers supported now */
	if (!port || tb_route(port->sw))
		return;

	tb_retimer_get_devs(port);
	tb_retimer_stop_io(port->sw, port->mux_mode, port->port_index, port);
	tb_retimer_put_devs(port);
}

static int tb_retimer_nvm_read(void *priv, unsigned int offset, void *val,
			       size_t bytes)
{
	struct tb_nvm *nvm = priv;
	struct tb_retimer *rt = tb_to_retimer(nvm->dev);
	u8 port_index = 0;
	u32 mux_mode = 0;
	int ret;

	pm_runtime_get_sync(&rt->dev);

	if (!mutex_trylock(&rt->tb->lock)) {
		ret = restart_syscall();
		goto out;
	}

	/* Prepare the retimer for IO */
	tb_retimer_scan(rt->port, false, &mux_mode, &port_index);

	if (!delayed_work_pending(&rt->port->retimer_stop_io_work)) {
		/*
		 * It is possible for the read operation to be incomplete
		 * in some cases such as user canceling the read operation
		 * or the user space application (doing the read) itself
		 * crashes. Having a delayed work to check and stop the IO
		 * becomes useful in these cases, so the USB4 port can
		 * continue to be used.
		 */
		INIT_DELAYED_WORK(&rt->port->retimer_stop_io_work,
				  tb_retimer_stop_io_delayed);
		queue_delayed_work(rt->port->sw->tb->wq,
				   &rt->port->retimer_stop_io_work,
				   msecs_to_jiffies(TB_RETIMER_STOP_IO_DELAY));
	}

	ret = usb4_port_retimer_nvm_read(rt->port, rt->index, offset, val, bytes);
	/* Stop the IO on error or when the entire nvm is read */
	if (ret || offset + bytes == nvm->active_size) {
		if (delayed_work_pending(&rt->port->retimer_stop_io_work))
			cancel_delayed_work_sync(&rt->port->retimer_stop_io_work);

		tb_retimer_stop_io(rt->port->sw, rt->port->mux_mode,
				   rt->port->port_index, rt->port);
		goto out_unlock;
	}

	if (delayed_work_pending(&rt->port->retimer_stop_io_work))
		mod_delayed_work(rt->port->sw->tb->wq,
				 &rt->port->retimer_stop_io_work,
				 msecs_to_jiffies(TB_RETIMER_STOP_IO_DELAY));

out_unlock:
	mutex_unlock(&rt->tb->lock);
out:
	pm_runtime_mark_last_busy(&rt->dev);
	pm_runtime_put_autosuspend(&rt->dev);

	return ret;
}

static int tb_retimer_nvm_write(void *priv, unsigned int offset, void *val,
				size_t bytes)
{
	struct tb_nvm *nvm = priv;
	struct tb_retimer *rt = tb_to_retimer(nvm->dev);
	int ret = 0;

	if (!mutex_trylock(&rt->tb->lock))
		return restart_syscall();

	ret = tb_nvm_write_buf(nvm, offset, val, bytes);
	mutex_unlock(&rt->tb->lock);

	return ret;
}

static int tb_retimer_nvm_read_version(struct tb_retimer *rt)
{
	u32 val = 0;
	int ret;

	if (!rt || !rt->nvm)
		return -EINVAL;

	ret = usb4_port_retimer_nvm_read(rt->port, rt->index, NVM_VERSION, &val,
					 sizeof(val));
	if (ret)
		return ret;

	rt->nvm->major = val >> 16;
	rt->nvm->minor = val >> 8;

	return 0;
}

static int tb_retimer_nvm_add(struct tb_retimer *rt)
{
	struct tb_nvm *nvm;
	u32 val, nvm_size;
	int ret;

	nvm = tb_nvm_alloc(&rt->dev);
	if (IS_ERR(nvm))
		return PTR_ERR(nvm);

	rt->nvm = nvm;

	ret = tb_retimer_nvm_read_version(rt);
	if (ret)
		goto err_nvm;

	ret = usb4_port_retimer_nvm_read(rt->port, rt->index, NVM_FLASH_SIZE,
					 &val, sizeof(val));
	if (ret)
		goto err_nvm;

	nvm_size = (SZ_1M << (val & 7)) / 8;
	nvm_size = (nvm_size - SZ_16K) / 2;

	ret = tb_nvm_add_active(nvm, nvm_size, tb_retimer_nvm_read);
	if (ret)
		goto err_nvm;

	nvm->active_size = nvm_size;

	ret = tb_nvm_add_non_active(nvm, NVM_MAX_SIZE, tb_retimer_nvm_write);
	if (ret)
		goto err_nvm;

	return 0;

err_nvm:
	tb_nvm_free(nvm);
	rt->nvm = NULL;
	return ret;
}

static int tb_retimer_nvm_validate_and_write(struct tb_retimer *rt)
{
	unsigned int image_size, hdr_size;
	const u8 *buf = rt->nvm->buf;
	u16 ds_size, device;

	image_size = rt->nvm->buf_data_size;
	if (image_size < NVM_MIN_SIZE || image_size > NVM_MAX_SIZE)
		return -EINVAL;

	/*
	 * FARB pointer must point inside the image and must at least
	 * contain parts of the digital section we will be reading here.
	 */
	hdr_size = (*(u32 *)buf) & 0xffffff;
	if (hdr_size + NVM_DEVID + 2 >= image_size)
		return -EINVAL;

	/* Digital section start should be aligned to 4k page */
	if (!IS_ALIGNED(hdr_size, SZ_4K))
		return -EINVAL;

	/*
	 * Read digital section size and check that it also fits inside
	 * the image.
	 */
	ds_size = *(u16 *)(buf + hdr_size);
	if (ds_size >= image_size)
		return -EINVAL;

	/*
	 * Make sure the device ID in the image matches the retimer
	 * hardware.
	 */
	device = *(u16 *)(buf + hdr_size + NVM_DEVID);
	if (device != rt->device)
		return -EINVAL;

	/* Skip headers in the image */
	buf += hdr_size;
	image_size -= hdr_size;

	return usb4_port_retimer_nvm_write(rt->port, rt->index, 0, buf,
					   image_size);
}

static ssize_t device_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct tb_retimer *rt = tb_to_retimer(dev);

	return sprintf(buf, "%#x\n", rt->device);
}
static DEVICE_ATTR_RO(device);

static ssize_t nvm_authenticate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tb_retimer *rt = tb_to_retimer(dev);
	int ret;

	if (!mutex_trylock(&rt->tb->lock))
		return restart_syscall();

	if (!rt->nvm)
		ret = -EAGAIN;
	else
		ret = sprintf(buf, "%#x\n", rt->auth_status);

	mutex_unlock(&rt->tb->lock);

	return ret;
}

static ssize_t nvm_authenticate_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tb_retimer *rt = tb_to_retimer(dev);
	u8 port_index = 0;
	u32 mux_mode = 0;
	int ret, val;

	pm_runtime_get_sync(&rt->dev);

	if (!mutex_trylock(&rt->tb->lock)) {
		ret = restart_syscall();
		goto exit_rpm;
	}

	if (!rt->nvm) {
		ret = -EAGAIN;
		goto exit_unlock;
	}

	ret = kstrtoint(buf, 10, &val);
	if (ret)
		goto exit_unlock;

	/* Always clear status */
	rt->auth_status = 0;

	if (val <= 0 || val > WRITE_ONLY || !rt->nvm->buf) {
		ret = -EINVAL;
		goto exit_unlock;
	}

	if (val == WRITE_ONLY && rt->nvm->flushed)
		goto exit_unlock;

	tb_retimer_scan(rt->port, false, &mux_mode, &port_index);

	if (!rt->nvm->flushed) {
		ret = tb_retimer_nvm_validate_and_write(rt);
		if (!ret)
			rt->nvm->flushed = true;
	}

	if (ret || val == WRITE_ONLY)
		goto exit_stop_io;

	rt->nvm->authenticating = true;
	ret = usb4_port_retimer_nvm_authenticate(rt->port, rt->index);
	if (ret)
		goto exit_authenticating;

	tb_retimer_nvm_read_version(rt);

	/* Reset these flags after an authentication */
	rt->nvm->flushed = false;
exit_authenticating:
	rt->nvm->authenticating = false;
exit_stop_io:
	tb_retimer_stop_io(rt->port->sw, mux_mode, port_index, rt->port);
exit_unlock:
	mutex_unlock(&rt->tb->lock);
exit_rpm:
	pm_runtime_mark_last_busy(&rt->dev);
	pm_runtime_put_autosuspend(&rt->dev);

	if (ret)
		return ret;
	return count;
}
static DEVICE_ATTR_RW(nvm_authenticate);

static ssize_t nvm_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tb_retimer *rt = tb_to_retimer(dev);
	int ret;

	if (!mutex_trylock(&rt->tb->lock))
		return restart_syscall();

	if (!rt->nvm)
		ret = -EAGAIN;
	else
		ret = sprintf(buf, "%x.%x\n", rt->nvm->major, rt->nvm->minor);

	mutex_unlock(&rt->tb->lock);
	return ret;
}
static DEVICE_ATTR_RO(nvm_version);

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct tb_retimer *rt = tb_to_retimer(dev);

	return sprintf(buf, "%#x\n", rt->vendor);
}
static DEVICE_ATTR_RO(vendor);

static struct attribute *retimer_attrs[] = {
	&dev_attr_device.attr,
	&dev_attr_nvm_authenticate.attr,
	&dev_attr_nvm_version.attr,
	&dev_attr_vendor.attr,
	NULL
};

static const struct attribute_group retimer_group = {
	.attrs = retimer_attrs,
};

static const struct attribute_group *retimer_groups[] = {
	&retimer_group,
	NULL
};

static void tb_retimer_release(struct device *dev)
{
	struct tb_retimer *rt = tb_to_retimer(dev);

	kfree(rt);
}

struct device_type tb_retimer_type = {
	.name = "thunderbolt_retimer",
	.groups = retimer_groups,
	.release = tb_retimer_release,
};

static int tb_retimer_add(struct tb_port *port, u8 index, u32 auth_status)
{
	struct tb_retimer *rt;
	u32 vendor, device;
	int ret;

	if (!port->cap_usb4)
		return -EINVAL;

	ret = usb4_port_retimer_read(port, index, USB4_SB_VENDOR_ID, &vendor,
				     sizeof(vendor));
	if (ret) {
		if (ret != -ENODEV)
			tb_port_warn(port, "failed read retimer VendorId: %d\n", ret);
		return ret;
	}

	ret = usb4_port_retimer_read(port, index, USB4_SB_PRODUCT_ID, &device,
				     sizeof(device));
	if (ret) {
		if (ret != -ENODEV)
			tb_port_warn(port, "failed read retimer ProductId: %d\n", ret);
		return ret;
	}

	if (vendor != PCI_VENDOR_ID_INTEL && vendor != 0x8087) {
		tb_port_info(port, "retimer NVM format of vendor %#x is not supported\n",
			     vendor);
		return -EOPNOTSUPP;
	}

	/*
	 * Check that it supports NVM operations. If not then don't add
	 * the device at all.
	 */
	ret = usb4_port_retimer_nvm_sector_size(port, index);
	if (ret < 0)
		return ret;

	rt = kzalloc(sizeof(*rt), GFP_KERNEL);
	if (!rt)
		return -ENOMEM;

	rt->index = index;
	rt->vendor = vendor;
	rt->device = device;
	rt->auth_status = auth_status;
	rt->port = port;
	rt->tb = port->sw->tb;

	rt->dev.parent = &port->sw->dev;
	rt->dev.bus = &tb_bus_type;
	rt->dev.type = &tb_retimer_type;
	dev_set_name(&rt->dev, "%s:%u.%u", dev_name(&port->sw->dev),
		     port->port, index);

	ret = device_register(&rt->dev);
	if (ret) {
		dev_err(&rt->dev, "failed to register retimer: %d\n", ret);
		put_device(&rt->dev);
		return ret;
	}

	ret = tb_retimer_nvm_add(rt);
	if (ret) {
		dev_err(&rt->dev, "failed to add NVM devices: %d\n", ret);
		device_unregister(&rt->dev);
		return ret;
	}

	dev_info(&rt->dev, "new retimer found, vendor=%#x device=%#x\n",
		 rt->vendor, rt->device);

	pm_runtime_no_callbacks(&rt->dev);
	pm_runtime_set_active(&rt->dev);
	pm_runtime_enable(&rt->dev);
	pm_runtime_set_autosuspend_delay(&rt->dev, TB_AUTOSUSPEND_DELAY);
	pm_runtime_mark_last_busy(&rt->dev);
	pm_runtime_use_autosuspend(&rt->dev);

	return 0;
}

static void tb_retimer_remove(struct tb_retimer *rt)
{
	dev_info(&rt->dev, "retimer disconnected\n");
	tb_nvm_free(rt->nvm);
	device_unregister(&rt->dev);
}

struct tb_retimer_lookup {
	const struct tb_port *port;
	u8 index;
};

static int retimer_match(struct device *dev, void *data)
{
	const struct tb_retimer_lookup *lookup = data;
	struct tb_retimer *rt = tb_to_retimer(dev);

	return rt && rt->port == lookup->port && rt->index == lookup->index;
}

static struct tb_retimer *tb_port_find_retimer(struct tb_port *port, u8 index)
{
	struct tb_retimer_lookup lookup = { .port = port, .index = index };
	struct device *dev;

	dev = device_find_child(&port->sw->dev, &lookup, retimer_match);
	if (dev)
		return tb_to_retimer(dev);

	return NULL;
}

/**
 * tb_retimer_scan_delayed() - scan onboard retimers when no devices
 * are connected
 * @work: work that needs to be done
 * Executes on tb->wq.
 */
void tb_retimer_scan_delayed(struct work_struct *work)
{
	struct tb_port *port = container_of(work, typeof(*port),
					    retimer_scan_work.work);

	/* Only onboard retimers supported now */
	if (!port || tb_route(port->sw))
		return;

	tb_retimer_get_devs(port);
	tb_retimer_scan(port, true, NULL, NULL);
	tb_retimer_put_devs(port);
}

/**
 * tb_retimer_scan() - Scan for on-board retimers under port
 * @port: USB4 port to scan
 * @enumerate: Enumerate the retimer or just scan and prepare the retimer for IO
 * @mux_mode: stores the mux mode
 * @typec_port_index: stores the Type-C port index
 *
 * Tries to enumerate on-board retimers connected to @port. Found
 * retimers are registered as children of @port. Does not scan for cable
 * retimers for now.
 */
int tb_retimer_scan(struct tb_port *port, bool enumerate, u32 *mux_mode,
		    u8 *typec_port_index)
{
	u32 status[TB_MAX_RETIMER_INDEX] = {}, result = 0;
	u32 mode = USB_RETIMER_FW_UPDATE_INVALID_MUX;
	int ret, i, j = 0, last_idx = 0;

	if (!port->cap_usb4)
		return 0;

	if (enumerate && port->retimer_scan_done)
		return 0;

	if (!enumerate && port->rt_io_started)
		return 0;

	/* Start IO for onboard retimers */
	ret = tb_retimer_acpi_dsm_get_port_info(port->sw, &result);
	if (!ret && result) {
		/* Get the Type-C port index in j */
		for_each_set_bit(j, (const unsigned long *)&result,
				 BITS_PER_BYTE) {
			/*
			 * Skip non-matching USB4 ports given the Type-C
			 * port info
			 */
			if (port->port != (BIT(j + 1) - 1))
				continue;
			port->port_index = j;
			/* Match found */
			break;
		}

		if (typec_port_index)
			*typec_port_index = j;

		tb_retimer_start_io(port->sw, &mode, j, port);
		port->mux_mode = mode;

		if (mux_mode)
			*mux_mode = mode;

		if (mode == USB_PD_MUX_NONE ||
		    mode == USB_PD_MUX_POLARITY_INVERTED) {
			usb4_port_router_offline(port, false);
			/* This delay helps router handle further operations */
			msleep(100);
		}
	}

	/*
	 * Send broadcast RT to make sure retimer indices facing this
	 * port are set.
	 */
	ret = usb4_port_enumerate_retimers(port);
	if (ret)
		goto out_retimer_stop_io;

	/*
	 * Before doing anything else, read the authentication status.
	 * If the retimer has it set, store it for the new retimer
	 * device instance.
	 */
	for (i = 1; i <= TB_MAX_RETIMER_INDEX; i++) {
		if (!tb_route(port->sw) &&
		    (mode & USB_PD_MUX_TBT_COMPAT_ENABLED ||
		     mode & USB_PD_MUX_USB4_ENABLED ||
		     mode == USB_PD_MUX_NONE ||
		     mode == USB_PD_MUX_POLARITY_INVERTED))
			ret = usb4_port_set_inbound_sbtx(port, i, true);
		usb4_port_retimer_nvm_authenticate_status(port, i, &status[i]);
	}

	for (i = 1; i <= TB_MAX_RETIMER_INDEX; i++) {
		/*
		 * Last retimer is true only for the last on-board
		 * retimer (the one connected directly to the Type-C
		 * port).
		 */
		ret = usb4_port_retimer_is_last(port, i);
		if (ret > 0)
			last_idx = i;
		else if (ret < 0)
			break;
	}

	port->retimer_scan_done = true;
	if (!last_idx)
		goto out_retimer_stop_io;

	if (!enumerate)
		goto out;

	/* Add on-board retimers if they do not exist already */
	for (i = 1; i <= last_idx; i++) {
		struct tb_retimer *rt;

		rt = tb_port_find_retimer(port, i);
		if (rt) {
			put_device(&rt->dev);
		} else {
			ret = tb_retimer_add(port, i, status[i]);
			if (ret && ret != -EOPNOTSUPP)
				goto out_retimer_stop_io;
		}
	}

out_retimer_stop_io:
	tb_retimer_stop_io(port->sw, mode, j, port);
out:
	return ret;
}

static int remove_retimer(struct device *dev, void *data)
{
	struct tb_retimer *rt = tb_to_retimer(dev);
	struct tb_port *port = data;

	if (rt && rt->port == port)
		tb_retimer_remove(rt);
	return 0;
}

/**
 * tb_retimer_remove_all() - Remove all retimers under port
 * @port: USB4 port whose retimers to remove
 * @sw: USB4 switch whose retimers to remove
 *
 * This removes all previously added retimers under @port
 * under a given switch.
 */
void tb_retimer_remove_all(struct tb_port *port, struct tb_switch *sw)
{
	struct tb_retimer *rt;

	if (!port || !port->sw || !sw)
		return;

	/*
	 * Cancel any pending work(s) on the onboard retimers, to prevent
	 * the work item(s) from being executed after the retimer device is
	 * removed.
	 */
	if (!tb_route(sw) && delayed_work_pending(&port->retimer_scan_work))
		cancel_delayed_work_sync(&port->retimer_scan_work);
	if (!tb_route(sw) && delayed_work_pending(&port->retimer_stop_io_work))
		cancel_delayed_work_sync(&port->retimer_stop_io_work);

	rt = tb_to_retimer(&port->sw->dev);

	/*
	 * Remove the retimers that belong to the switch being removed.
	 * Allow for the removal of non-onboard retimers.
	 */
	if (port->cap_usb4 && ((rt && sw == rt->port->sw) || tb_route(sw)))
		device_for_each_child_reverse(&port->sw->dev, port,
					      remove_retimer);
}
