/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author: Yakir Yang <ykk@rock-chips.com>
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

#include <linux/input.h>
#include <linux/reboot.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "rockchip_drm_drv.h"
#include "rockchip_drm_psr.h"

#define PSR_FLUSH_TIMEOUT_MS		100

struct psr_drv {
	struct list_head	list;
	struct drm_encoder	*encoder;

	struct mutex		lock;
	int			inhibit_count;
	bool			enabled;

	struct delayed_work	flush_work;
	struct work_struct	disable_work;

	struct notifier_block	reboot_nb;
	struct input_handler    input_handler;

	int (*set)(struct drm_encoder *encoder, bool enable);
};

static struct psr_drv *find_psr_by_encoder(struct drm_encoder *encoder)
{
	struct rockchip_drm_private *drm_drv = encoder->dev->dev_private;
	struct psr_drv *psr;

	mutex_lock(&drm_drv->psr_list_lock);
	list_for_each_entry(psr, &drm_drv->psr_list, list) {
		if (psr->encoder == encoder)
			goto out;
	}
	psr = ERR_PTR(-ENODEV);

out:
	mutex_unlock(&drm_drv->psr_list_lock);
	return psr;
}

static int psr_set_state_locked(struct psr_drv *psr, bool enable)
{
	int ret;

	if (psr->inhibit_count > 0)
		return -EINVAL;

	if (enable == psr->enabled)
		return 0;

	ret = psr->set(psr->encoder, enable);
	if (ret)
		return ret;

	psr->enabled = enable;
	return 0;
}

static void psr_flush_handler(struct work_struct *work)
{
	struct psr_drv *psr = container_of(to_delayed_work(work),
					   struct psr_drv, flush_work);

	mutex_lock(&psr->lock);
	psr_set_state_locked(psr, true);
	mutex_unlock(&psr->lock);
}

/**
 * rockchip_drm_psr_inhibit_put - release PSR inhibit on given encoder
 * @encoder: encoder to obtain the PSR encoder
 *
 * Decrements PSR inhibit count on given encoder. Should be called only
 * for a PSR inhibit count increment done before. If PSR inhibit counter
 * reaches zero, PSR flush work is scheduled to make the hardware enter
 * PSR mode in PSR_FLUSH_TIMEOUT_MS.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int rockchip_drm_psr_inhibit_put(struct drm_encoder *encoder)
{
	struct psr_drv *psr = find_psr_by_encoder(encoder);

	if (IS_ERR(psr))
		return PTR_ERR(psr);

	mutex_lock(&psr->lock);
	--psr->inhibit_count;
	if (!psr->inhibit_count)
		mod_delayed_work(system_wq, &psr->flush_work,
				 PSR_FLUSH_TIMEOUT_MS);
	mutex_unlock(&psr->lock);

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_psr_inhibit_put);

/**
 * rockchip_drm_psr_inhibit_get - acquire PSR inhibit on given encoder
 * @encoder: encoder to obtain the PSR encoder
 *
 * Increments PSR inhibit count on given encoder. This function guarantees
 * that after it returns PSR is turned off on given encoder and no PSR-related
 * hardware state change occurs at least until a matching call to
 * rockchip_drm_psr_inhibit_put() is done.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int rockchip_drm_psr_inhibit_get(struct drm_encoder *encoder)
{
	struct psr_drv *psr = find_psr_by_encoder(encoder);

	if (IS_ERR(psr))
		return PTR_ERR(psr);

	mutex_lock(&psr->lock);
	psr_set_state_locked(psr, false);
	++psr->inhibit_count;
	mutex_unlock(&psr->lock);
	cancel_delayed_work_sync(&psr->flush_work);
	cancel_work_sync(&psr->disable_work);

	return 0;
}
EXPORT_SYMBOL(rockchip_drm_psr_inhibit_get);

static void rockchip_drm_do_flush(struct psr_drv *psr)
{
	cancel_delayed_work_sync(&psr->flush_work);

	mutex_lock(&psr->lock);
	if (!psr_set_state_locked(psr, false))
		mod_delayed_work(system_wq, &psr->flush_work,
				 PSR_FLUSH_TIMEOUT_MS);
	mutex_unlock(&psr->lock);
}

/**
 * rockchip_drm_psr_flush_all - force to flush all registered PSR encoders
 * @dev: drm device
 *
 * Disable the PSR function for all registered encoders, and then enable the
 * PSR function back after PSR_FLUSH_TIMEOUT. If encoder PSR state have been
 * changed during flush time, then keep the state no change after flush
 * timeout.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
void rockchip_drm_psr_flush_all(struct drm_device *dev)
{
	struct rockchip_drm_private *drm_drv = dev->dev_private;
	struct psr_drv *psr;

	mutex_lock(&drm_drv->psr_list_lock);
	list_for_each_entry(psr, &drm_drv->psr_list, list)
		rockchip_drm_do_flush(psr);
	mutex_unlock(&drm_drv->psr_list_lock);
}
EXPORT_SYMBOL(rockchip_drm_psr_flush_all);

static void psr_disable_handler(struct work_struct *work)
{
	struct psr_drv *psr = container_of(work, struct psr_drv, disable_work);

	rockchip_drm_do_flush(psr);
}

static void psr_input_event(struct input_handle *handle,
			    unsigned int type, unsigned int code,
			    int value)
{
	struct psr_drv *psr = handle->handler->private;

	schedule_work(&psr->disable_work);
}

static int psr_input_connect(struct input_handler *handler,
			     struct input_dev *dev,
			     const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "rockchip-psr";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;

err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void psr_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

/* Same device ids as cpu-boost */
static const struct input_device_id psr_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			 INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			    BIT_MASK(ABS_MT_POSITION_X) |
			    BIT_MASK(ABS_MT_POSITION_Y) },
	}, /* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_X)] = BIT_MASK(ABS_X) }

	}, /* stylus or joystick device */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) },
	}, /* pointer (e.g. trackpad, mouse) */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(KEY_ESC)] = BIT_MASK(KEY_ESC) },
	}, /* keyboard */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = {[BIT_WORD(BTN_JOYSTICK)] = BIT_MASK(BTN_JOYSTICK) },
	}, /* joysticks not caught by ABS_X above */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_KEY) },
		.keybit = { [BIT_WORD(BTN_GAMEPAD)] = BIT_MASK(BTN_GAMEPAD) },
	}, /* gamepad */
	{ },
};

static int rockchip_drm_psr_reboot_notifier(struct notifier_block *nb,
					    unsigned long action, void *data)
{
	struct psr_drv *psr = container_of(nb, struct psr_drv, reboot_nb);

	/*
	 * It looks like the driver subsystem detaches devices from power
	 * domains at shutdown without consent of the drivers. This means
	 * that we might have our power domain turned off behind our back
	 * and the only way to avoid problems is to stop doing any hardware
	 * programming after this point, which is achieved by the unbalanced
	 * call below.
	 */
	rockchip_drm_psr_inhibit_get(psr->encoder);

	return 0;
}

/**
 * rockchip_drm_psr_register - register encoder to psr driver
 * @encoder: encoder that obtain the PSR function
 * @psr_set: call back to set PSR state
 *
 * The function returns with PSR inhibit counter initialized with one
 * and the caller (typically encoder driver) needs to call
 * rockchip_drm_psr_inhibit_put() when it becomes ready to accept PSR
 * enable request.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int rockchip_drm_psr_register(struct drm_encoder *encoder,
			int (*psr_set)(struct drm_encoder *, bool enable))
{
	struct rockchip_drm_private *drm_drv = encoder->dev->dev_private;
	struct psr_drv *psr;
	int error;

	if (!encoder || !psr_set)
		return -EINVAL;

	psr = kzalloc(sizeof(struct psr_drv), GFP_KERNEL);
	if (!psr)
		return -ENOMEM;

	INIT_DELAYED_WORK(&psr->flush_work, psr_flush_handler);
	INIT_WORK(&psr->disable_work, psr_disable_handler);
	mutex_init(&psr->lock);

	psr->inhibit_count = 1;
	psr->enabled = false;
	psr->encoder = encoder;
	psr->set = psr_set;

	psr->input_handler.event = psr_input_event;
	psr->input_handler.connect = psr_input_connect;
	psr->input_handler.disconnect = psr_input_disconnect;
	psr->input_handler.name =
		kasprintf(GFP_KERNEL, "rockchip-psr-%s", encoder->name);
	if (!psr->input_handler.name) {
		error = -ENOMEM;
		goto err2;
	}
	psr->input_handler.id_table = psr_ids;
	psr->input_handler.private = psr;

	error = input_register_handler(&psr->input_handler);
	if (error)
		goto err1;

	psr->reboot_nb.notifier_call = rockchip_drm_psr_reboot_notifier;
	register_reboot_notifier(&psr->reboot_nb);

	mutex_lock(&drm_drv->psr_list_lock);
	list_add_tail(&psr->list, &drm_drv->psr_list);
	mutex_unlock(&drm_drv->psr_list_lock);

	return 0;

 err1:
	kfree(psr->input_handler.name);
 err2:
	kfree(psr);
	return error;
}
EXPORT_SYMBOL(rockchip_drm_psr_register);

/**
 * rockchip_drm_psr_unregister - unregister encoder to psr driver
 * @encoder: encoder that obtain the PSR function
 * @psr_set: call back to set PSR state
 *
 * It is expected that the PSR inhibit counter is 1 when this function is
 * called, which corresponds to a state when related encoder has been
 * disconnected from any CRTCs and its driver called
 * rockchip_drm_psr_inhibit_get() to stop the PSR logic.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
void rockchip_drm_psr_unregister(struct drm_encoder *encoder)
{
	struct rockchip_drm_private *drm_drv = encoder->dev->dev_private;
	struct psr_drv *psr, *n;

	mutex_lock(&drm_drv->psr_list_lock);
	list_for_each_entry_safe(psr, n, &drm_drv->psr_list, list) {
		if (psr->encoder == encoder) {
			/*
			 * Any other value would mean that the encoder
			 * is still in use.
			 */
			WARN_ON(psr->inhibit_count != 1);

			list_del(&psr->list);
			unregister_reboot_notifier(&psr->reboot_nb);
			input_unregister_handler(&psr->input_handler);
			kfree(psr->input_handler.name);
			kfree(psr);
		}
	}
	mutex_unlock(&drm_drv->psr_list_lock);
}
EXPORT_SYMBOL(rockchip_drm_psr_unregister);
