/*
 * Copyright (C) 2010 The Chromium OS Authors <chromium-os-dev@chromium.org>
 *                    All Rights Reserved.
 *
 * This file is released under the GPL.
 *
 * Provide error types for use when creating a custom error handler.
 * See Documentation/device-mapper/dm-verity.txt
 */
#ifndef DM_VERITY_H
#define DM_VERITY_H

#include <linux/notifier.h>

struct dm_verity_error_state {
	int code;
	int transient;  /* Likely to not happen after a reboot */
	u64 block;
	const char *message;

	sector_t dev_start;
	sector_t dev_len;
	struct block_device *dev;

	sector_t hash_dev_start;
	sector_t hash_dev_len;
	struct block_device *hash_dev;

	/* Final behavior after all notifications are completed. */
	int behavior;
};

/* This enum must be matched to allowed_error_behaviors in dm-verity.c */
enum dm_verity_error_behavior {
	DM_VERITY_ERROR_BEHAVIOR_EIO = 0,
	DM_VERITY_ERROR_BEHAVIOR_PANIC,
	DM_VERITY_ERROR_BEHAVIOR_NONE,
	DM_VERITY_ERROR_BEHAVIOR_NOTIFY
};


int dm_verity_register_error_notifier(struct notifier_block *nb);
int dm_verity_unregister_error_notifier(struct notifier_block *nb);

#endif  /* DM_VERITY_H */
