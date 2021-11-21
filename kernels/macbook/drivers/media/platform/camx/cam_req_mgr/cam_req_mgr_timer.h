/* SPDX-License-Identifier: GPL-2.0-only */
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

#ifndef _CAM_REQ_MGR_TIMER_H_
#define _CAM_REQ_MGR_TIMER_H_

#include <linux/slab.h>
#include <linux/timer.h>

#include "cam_req_mgr_core_defs.h"


/** struct tm_kmem_cache
 * @use_count		: number of clients using crm timers
 * @kmem_cachep		: pointer to the slab cache
 */
struct tm_kmem_cache {
	int32_t use_count;
	struct kmem_cache *mem_cachep;
};

/** struct cam_req_mgr_timer
 * @expires    : timeout value for timer
 * @sys_timer  : system timer variable
 * @parent     : priv data - link pointer
 * @timer_cb   : callback func which will be called when timeout expires
 */
struct cam_req_mgr_timer {
	int32_t             expires;
	struct timer_list   sys_timer;
	void               *parent;
	void              (*timer_cb)(struct timer_list *tl);
};

/**
 * crm_timer_modify()
 * @brief : allows ser to modify expiry time.
 * @timer : timer which will be reset to expires values
 */
void crm_timer_modify(struct cam_req_mgr_timer *crm_timer, int32_t expires);

/**
 * crm_timer_reset()
 * @brief : destroys the timer allocated.
 * @timer : timer which will be reset to expires values
 */
void crm_timer_reset(struct cam_req_mgr_timer *crm_timer);

/**
 * crm_timer_init()
 * @brief    : create a new general purpose timer.
 *             timer utility takes care of allocating memory and deleting
 * @timer    : double pointer to new timer allocated
 * @expires  : Timeout value to fire callback
 * @parent   : void pointer which caller can use for book keeping
 * @timer_cb : caller can chose to use its own callback function when
 *             timer fires the timeout. If no value is set timer util
 *             will use default.
 */
int crm_timer_init(struct cam_req_mgr_timer **timer, int32_t expires,
		   void *parent, void (*timer_cb)(struct timer_list *));

/**
 * crm_timer_exit()
 * @brief : destroys the timer allocated.
 * @timer : timer pointer which will be freed
 */
void crm_timer_exit(struct cam_req_mgr_timer **timer);

/**
 * crm_timer_cache_create()
 * @brief : creates kmem timer cache or returns the existing one
 * @tm_cache : pointer to the created cache, can be used for debug purposes
 *
 * NOTE: the number of invocations of crm_timer_cache_create shall be equal to
 * the number of invocations of crm_timer_cache_destroy
 */
int crm_timer_cache_create(struct tm_kmem_cache **tm_cache);

/**
 * crm_timer_cache_destroy()
 * @brief : destroys the timer cache.
 *
 * NOTE: this will decrease the number of users of the crm timers. The cache will be distroyed
 * if there are no cache consumers  otherwise the cache memory won't be freed.
 */
void crm_timer_cache_destroy(void);


#endif
