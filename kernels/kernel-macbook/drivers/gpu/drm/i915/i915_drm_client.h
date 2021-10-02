/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Intel Corporation
 */

#ifndef __I915_DRM_CLIENT_H__
#define __I915_DRM_CLIENT_H__

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/xarray.h>

#include "gt/intel_engine_types.h"

struct drm_i915_private;

struct i915_drm_clients {
	struct drm_i915_private *i915;

	struct xarray xarray;
	u32 next_id;

	struct kobject *root;
};

struct i915_drm_client;

struct i915_engine_busy_attribute {
	struct device_attribute attr;
	struct i915_drm_client *client;
	unsigned int engine_class;
};

struct i915_drm_client_name {
	struct rcu_head rcu;
	struct i915_drm_client *client;
	struct pid *pid;
	char name[];
};

struct i915_drm_client {
	struct kref kref;

	struct rcu_work rcu;

	struct mutex update_lock; /* Serializes name and pid updates. */

	unsigned int id;
	struct i915_drm_client_name __rcu *name;
	bool closed;

	spinlock_t ctx_lock; /* For add/remove from ctx_list. */
	struct list_head ctx_list; /* List of contexts belonging to client. */

	struct i915_drm_clients *clients;

	struct kobject *root;
	struct kobject *busy_root;
	struct {
		struct device_attribute pid;
		struct device_attribute name;
		struct i915_engine_busy_attribute busy[MAX_ENGINE_CLASS + 1];
	} attr;

	/**
	 * @past_runtime: Accumulation of pphwsp runtimes from closed contexts.
	 */
	atomic64_t past_runtime[MAX_ENGINE_CLASS + 1];
};

void i915_drm_clients_init(struct i915_drm_clients *clients,
			   struct drm_i915_private *i915);

static inline struct i915_drm_client *
i915_drm_client_get(struct i915_drm_client *client)
{
	kref_get(&client->kref);
	return client;
}

void __i915_drm_client_free(struct kref *kref);

static inline void i915_drm_client_put(struct i915_drm_client *client)
{
	kref_put(&client->kref, __i915_drm_client_free);
}

void i915_drm_client_close(struct i915_drm_client *client);

struct i915_drm_client *i915_drm_client_add(struct i915_drm_clients *clients,
					    struct task_struct *task);

int i915_drm_client_update(struct i915_drm_client *client,
			   struct task_struct *task);

static inline const struct i915_drm_client_name *
__i915_drm_client_name(const struct i915_drm_client *client)
{
	return rcu_dereference(client->name);
}

static inline const char *
i915_drm_client_name(const struct i915_drm_client *client)
{
	return __i915_drm_client_name(client)->name;
}

static inline struct pid *
i915_drm_client_pid(const struct i915_drm_client *client)
{
	return __i915_drm_client_name(client)->pid;
}

void i915_drm_clients_fini(struct i915_drm_clients *clients);

#endif /* !__I915_DRM_CLIENT_H__ */
