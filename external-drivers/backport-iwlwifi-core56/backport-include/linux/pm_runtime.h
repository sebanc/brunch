#ifndef __BACKPORT_PM_RUNTIME_H
#define __BACKPORT_PM_RUNTIME_H
#include_next <linux/pm_runtime.h>

#if LINUX_VERSION_IS_LESS(3,9,0)
#define pm_runtime_active LINUX_BACKPORT(pm_runtime_active)
#ifdef CONFIG_PM
static inline bool pm_runtime_active(struct device *dev)
{
	return dev->power.runtime_status == RPM_ACTIVE
		|| dev->power.disable_depth;
}
#else
static inline bool pm_runtime_active(struct device *dev) { return true; }
#endif /* CONFIG_PM */

#endif /* LINUX_VERSION_IS_LESS(3,9,0) */

#if LINUX_VERSION_IS_LESS(3,15,0) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7,6)
static inline int pm_runtime_force_suspend(struct device *dev)
{
#ifdef CONFIG_PM
	/* cannot backport properly, I think */
	WARN_ON_ONCE(1);
	return -EINVAL;
#endif
	return 0;
}
static inline int pm_runtime_force_resume(struct device *dev)
{
#ifdef CONFIG_PM
	/* cannot backport properly, I think */
	WARN_ON_ONCE(1);
	return -EINVAL;
#endif
	return 0;
}
#endif

#endif /* __BACKPORT_PM_RUNTIME_H */
