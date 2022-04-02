// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Google, Inc.
 */
#include <linux/input.h>
#include <linux/slab.h>

#include <drm/drm_device.h>
#include <drm/drm_input_helper.h>

/**
 * DOC: overview
 *
 * This helper library provides a thin wrapper around input handles, so that
 * DRM drivers can easily perform domain-specific actions in response to user
 * activity. e.g., if someone is moving a mouse, we're likely to want to
 * display something soon, and we should exit panel self-refresh.
 */

static void drm_input_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	struct drm_input_handler *handler = handle->handler->private;

	handler->callback(handler);
}

static int drm_input_connect(struct input_handler *handler,
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
	handle->name = "drm-input-helper";

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

static void drm_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id drm_input_ids[] = {
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

int drm_input_handle_register(struct drm_device *dev,
			      struct drm_input_handler *handler)
{
	int ret;

	if (!handler->callback)
		return -EINVAL;

	handler->handler.event = drm_input_event;
	handler->handler.connect = drm_input_connect;
	handler->handler.disconnect = drm_input_disconnect;
	handler->handler.name = kasprintf(GFP_KERNEL, "drm-input-helper-%s",
					  dev_name(dev->dev));
	if (!handler->handler.name)
		return -ENOMEM;

	handler->handler.id_table = drm_input_ids;
	handler->handler.private = handler;

	ret = input_register_handler(&handler->handler);
	if (ret)
		goto err;

	return 0;

err:
	kfree(handler->handler.name);
	return ret;
}
EXPORT_SYMBOL(drm_input_handle_register);

void drm_input_handle_unregister(struct drm_input_handler *handler)
{
	input_unregister_handler(&handler->handler);
	kfree(handler->handler.name);
}
EXPORT_SYMBOL(drm_input_handle_unregister);
