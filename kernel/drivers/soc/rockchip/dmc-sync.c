/*
 * Copyright (C) 2014 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <soc/rockchip/dmc-sync.h>

static RAW_NOTIFIER_HEAD(sync_chain);
static RAW_NOTIFIER_HEAD(en_chain);
static DEFINE_MUTEX(sync_lock);
static DEFINE_MUTEX(en_lock);
static int num_wait;
static int num_disable;
static bool timeout_disabled;

/**
 * rockchip_dmc_disable_timeout - Quit observing timeouts given by the notifiers
 * in the sync chain.
 */
void rockchip_dmc_disable_timeout(void)
{
	mutex_lock(&sync_lock);
	WARN_ON(timeout_disabled);
	timeout_disabled = true;
	mutex_unlock(&sync_lock);
}

/**
 * rockchip_dmc_enable_timeout - Start observing timeouts given by the notifiers
 * in the sync chain. Should be used after a call to
 * rockchip_dmc_disable_timeout.
 */
void rockchip_dmc_enable_timeout(void)
{
	mutex_lock(&sync_lock);
	WARN_ON(!timeout_disabled);
	timeout_disabled = false;
	mutex_unlock(&sync_lock);
}

/**
 * rockchip_dmc_lock - Lock the sync notifiers.
 */
void rockchip_dmc_lock(void)
{
	mutex_lock(&sync_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dmc_lock);

/**
 * rockchip_dmc_wait - Call sync notifiers.
 */
int rockchip_dmc_wait(ktime_t *timeout)
{
	int ret;

	WARN_ON(!mutex_is_locked(&sync_lock));
	/* Set a default timeout. */
	*timeout = ktime_add_ns(ktime_get(), DMC_DEFAULT_TIMEOUT_NS);
	if (!timeout_disabled) {
		ret = raw_notifier_call_chain(&sync_chain, 0, timeout);
		if (ret == NOTIFY_BAD)
			return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rockchip_dmc_wait);

/**
 * rockchip_dmc_unlock - Unlock the sync notifiers.
 */
void rockchip_dmc_unlock(void)
{
	mutex_unlock(&sync_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dmc_unlock);

/**
 * rockchip_dmc_en_lock - Lock for calling and adding enable notifiers. Must be
 * held when calling rockchip_dmc_[un]register_enabled_notifier.
 */
void rockchip_dmc_en_lock(void)
{
	mutex_lock(&en_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dmc_en_lock);

/**
 * rockchip_dmc_en_unlock - Unlock after calling and adding enable notifiers.
 */
void rockchip_dmc_en_unlock(void)
{
	mutex_unlock(&en_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dmc_en_unlock);

/**
 * rockchip_dmc_enabled - Returns true if dmc freq is enabled, false otherwise.
 */
bool rockchip_dmc_enabled(void)
{
	return num_disable <= 0 && num_wait <= 1;
}
EXPORT_SYMBOL_GPL(rockchip_dmc_enabled);

/**
 * rockchip_dmc_enable - Enable dmc frequency scaling. Will only enable
 * frequency scaling if there are 1 or fewer notifiers. Call to undo
 * rockchip_dmc_disable.
 */
void rockchip_dmc_enable(void)
{
	mutex_lock(&en_lock);
	num_disable--;
	WARN_ON(num_disable < 0);
	if (rockchip_dmc_enabled())
		raw_notifier_call_chain(&en_chain, DMC_ENABLE, NULL);
	mutex_unlock(&en_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dmc_enable);

/**
 * rockchip_dmc_disable - Disable dmc frequency scaling. Call when something
 * cannot coincide with dmc frequency scaling.
 */
void rockchip_dmc_disable(void)
{
	mutex_lock(&en_lock);
	if (rockchip_dmc_enabled())
		raw_notifier_call_chain(&en_chain, DMC_DISABLE, NULL);
	num_disable++;
	mutex_unlock(&en_lock);
}
EXPORT_SYMBOL_GPL(rockchip_dmc_disable);

/**
 * rockchip_dmc_get - Register the notifier block for the sync chain.
 *
 * We can't sync with more than one notifier. By the time we sync with the
 * second notifier, the window of time for the first notifier for which we can
 * change the dmc freq for the first may have passed. So if the number of things
 * waiting during rockchip_dmc_wait is greater than one, call the enable call
 * chain with the disable message (this will likely disable ddr freq).
 * @nb The sync notifier block to register
 */
int rockchip_dmc_get(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;

	mutex_lock(&en_lock);
	/* This may call rockchip_dmc_lock/wait/unlock. */
	if (num_wait == 1 && num_disable <= 0)
		raw_notifier_call_chain(&en_chain, DMC_DISABLE, NULL);

	mutex_lock(&sync_lock);
	ret = raw_notifier_chain_register(&sync_chain, nb);
	mutex_unlock(&sync_lock);
	/*
	 * We need to call the enable chain before adding the notifier, which is
	 * why errors are handled this way.
	 */
	if (!ret)
		num_wait++;
	else if (num_wait == 1 && num_disable <= 0)
		raw_notifier_call_chain(&en_chain, DMC_ENABLE, NULL);

	mutex_unlock(&en_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmc_get);

/**
 * rockchip_dmc_put - Remove the notifier block from the sync chain.
 *
 * Call when registered notifier will no longer block. Increments the number of
 * things waiting during rockchip_dmc_wait. If that number is now 1, call the
 * enable call chain (likely enabling ddr freq).
 * @nb The sync notifier block to unregister
 */
int rockchip_dmc_put(struct notifier_block *nb)
{
	int ret;

	if (!nb)
		return -EINVAL;

	mutex_lock(&en_lock);
	mutex_lock(&sync_lock);
	ret = raw_notifier_chain_unregister(&sync_chain, nb);
	if (!ret)
		num_wait--;
	mutex_unlock(&sync_lock);
	/* This may call rockchip_dmc_lock/wait/unlock. */
	if (num_wait == 1 && num_disable <= 0 && !ret)
		raw_notifier_call_chain(&en_chain, DMC_ENABLE, NULL);
	mutex_unlock(&en_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmc_put);

/**
 * rockchip_dmc_register_enable_notifier - Add notifier to enable notifiers.
 *
 * Enable notifiers are called when we enable/disable dmc. This can be done
 * through rockchip_dmc_enable/disable or when there is more than one sync
 * notifier. Must call rockchip_dmc_en_lock before calling this.
 * @nb The notifier to add
 */
int rockchip_dmc_register_enable_notifier(struct notifier_block *nb)
{
	int ret = 0;

	if (!nb)
		return -EINVAL;

	ret = raw_notifier_chain_register(&en_chain, nb);
	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmc_register_enable_notifier);

/**
 * rockchip_dmc_unregister_enable_notifier - Remove notifier from enable
 * notifiers.
 *
 * Must call rockchip_dmc_en_lock before calling this.
 * @nb The notifier to remove.
 */
int rockchip_dmc_unregister_enable_notifier(struct notifier_block *nb)
{
	int ret = 0;

	if (!nb)
		return -EINVAL;

	ret = raw_notifier_chain_unregister(&en_chain, nb);
	return ret;
}
EXPORT_SYMBOL_GPL(rockchip_dmc_unregister_enable_notifier);
