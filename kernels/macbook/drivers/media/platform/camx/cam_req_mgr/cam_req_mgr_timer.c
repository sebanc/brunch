// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "cam_req_mgr_timer.h"
#include "cam_debug_util.h"

#define CRM_TIMER_CACHE_NAME "crm_timer"

static struct tm_kmem_cache *g_tm_cachep = NULL;
static DEFINE_MUTEX(tm_mutex); /* protects timer cache */

void crm_timer_reset(struct cam_req_mgr_timer *crm_timer)
{
	if (!crm_timer)
		return;
	CAM_DBG(CAM_CRM, "Starting timer %pK to fire in %d ms. (jiffies=%lu)\n",
		crm_timer, crm_timer->expires, jiffies);
	mod_timer(&crm_timer->sys_timer,
		(jiffies + msecs_to_jiffies(crm_timer->expires)));
}

static void crm_timer_callback(struct timer_list *timer_data)
{
	struct cam_req_mgr_timer *timer =
		container_of(timer_data, struct cam_req_mgr_timer, sys_timer);
	if (!timer)
		return;

	CAM_DBG(CAM_CRM, "timer %pK parent %pK", timer, timer->parent);
	crm_timer_reset(timer);
}

void crm_timer_modify(struct cam_req_mgr_timer *crm_timer,
	int32_t expires)
{
	CAM_DBG(CAM_CRM, "timer %pK new time %d ms", crm_timer, expires);
	if (crm_timer) {
		crm_timer->expires = expires;
		crm_timer_reset(crm_timer);
	}
}

int crm_timer_init(struct cam_req_mgr_timer **timer, int32_t expires,
		   void *parent, void (*timer_cb)(struct timer_list *))
{
	struct tm_kmem_cache *tm_cachep = g_tm_cachep;
	struct kmem_cache *mem_cachep = NULL;
	struct cam_req_mgr_timer *crm_timerp;

	if (!timer)
		return -EINVAL;

	if (*timer) {
		CAM_WARN(CAM_CRM, "Timer already exists!");
		return -EINVAL;
	}

	if (tm_cachep)
		mem_cachep = tm_cachep->mem_cachep;
	if (!mem_cachep)
		return -EPERM;

	CAM_DBG(CAM_CRM, "init timer %d ms %pK", expires, *timer);
	crm_timerp = kmem_cache_alloc(mem_cachep,
				      __GFP_ZERO | GFP_KERNEL);
	if (!crm_timerp)
		return -ENOMEM;

	crm_timerp->expires = expires;
	crm_timerp->parent = parent;
	if (timer_cb != NULL)
		crm_timerp->timer_cb = timer_cb;
	else
		crm_timerp->timer_cb = crm_timer_callback;
	timer_setup(&crm_timerp->sys_timer, crm_timerp->timer_cb, 0);
	crm_timer_reset(crm_timerp);
	*timer = crm_timerp;

	return 0;
}

void crm_timer_exit(struct cam_req_mgr_timer **timer)
{
	struct tm_kmem_cache *tm_cachep = g_tm_cachep;
	struct kmem_cache *mem_cachep = NULL;
	struct cam_req_mgr_timer *crm_timerp;

	if (!timer)
		return;

	crm_timerp = *timer;
	if (!crm_timerp) {
		CAM_ERR(CAM_CRM, "The CRM timer does not exist and cannot be destroyed!");
		return;
	}

	CAM_DBG(CAM_CRM, "destroy timer %pK", crm_timerp);
	del_timer_sync(&crm_timerp->sys_timer);

	if (tm_cachep)
		mem_cachep = tm_cachep->mem_cachep;

	if (!mem_cachep) {
		CAM_ERR(CAM_CRM,
			"Cannot release memory of timer %pK. Operation is not permitted!",
			crm_timerp);
		return;
	}

	kmem_cache_free(mem_cachep, crm_timerp);
	*timer = NULL;
}

int crm_timer_cache_create(struct tm_kmem_cache **tm_cache)
{
	struct tm_kmem_cache *timer_cachep = NULL;

	if (!tm_cache)
		return -EINVAL;

	if (!g_tm_cachep) {
		timer_cachep = kzalloc(sizeof(struct tm_kmem_cache), GFP_KERNEL);
		if (!timer_cachep)
			return -ENOMEM;

		timer_cachep->mem_cachep = kmem_cache_create(CRM_TIMER_CACHE_NAME,
							sizeof(struct cam_req_mgr_timer),
							64,
							SLAB_CONSISTENCY_CHECKS |
							SLAB_RED_ZONE | SLAB_POISON |
							SLAB_STORE_USER, NULL);
		if (!timer_cachep->mem_cachep) {
			kfree(timer_cachep);
			return -ENOMEM;
		}
	}

	mutex_lock(&tm_mutex);
	if (g_tm_cachep) {
		if (timer_cachep) {
			kmem_cache_destroy(timer_cachep->mem_cachep);
			kfree(timer_cachep);
		}
	} else {
		g_tm_cachep = timer_cachep;
	}
	g_tm_cachep->use_count++;
	*tm_cache = g_tm_cachep;
	mutex_unlock(&tm_mutex);

	return 0;
}

void crm_timer_cache_destroy(void)
{
	mutex_lock(&tm_mutex);

	if (!g_tm_cachep) {
		CAM_ERR(CAM_CRM, "Cannot destroy CRM timer cache. It does not exist!");
		goto end_destroy;
	}

	g_tm_cachep->use_count--;
	if (!g_tm_cachep->use_count) {
		kmem_cache_destroy(g_tm_cachep->mem_cachep);
		g_tm_cachep->mem_cachep = NULL;
		kfree(g_tm_cachep);
		g_tm_cachep = NULL;
	}

end_destroy:
	mutex_unlock(&tm_mutex);
}
