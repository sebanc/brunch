#ifndef __BACKPORT_LINUX_THERMAL_H
#define __BACKPORT_LINUX_THERMAL_H
#include_next <linux/thermal.h>
#include <linux/version.h>

#ifdef CONFIG_THERMAL
#if LINUX_VERSION_IS_LESS(3,8,0)
#include <linux/errno.h>

struct thermal_bind_params {
	struct thermal_cooling_device *cdev;
	int weight;
	int trip_mask;
	int (*match)(struct thermal_zone_device *tz,
		     struct thermal_cooling_device *cdev);
};

struct thermal_zone_params {
	int num_tbps;
	struct thermal_bind_params *tbp;
};

static inline struct thermal_zone_device *
backport_thermal_zone_device_register(const char *type, int trips, int mask,
				      void *devdata,
				      struct thermal_zone_device_ops *ops,
				      const struct thermal_zone_params *tzp,
				      int passive_delay, int polling_delay)
{
	return ERR_PTR(-EOPNOTSUPP);
}
#define thermal_zone_device_register backport_thermal_zone_device_register

static inline void thermal_notify_framework(struct thermal_zone_device *tz,
	int trip)
{ }
#else /* < 3.8.0 */

#if LINUX_VERSION_IS_LESS(3,10,0)
#define thermal_notify_framework notify_thermal_framework
#endif /* LINUX_VERSION_IS_LESS(3,10,0) */

#if LINUX_VERSION_IS_LESS(4,3,0)

typedef struct thermal_zone_device_ops old_thermal_zone_device_ops_t;

/* also add a way to call the old register and unregister functions */
static inline struct thermal_zone_device *old_thermal_zone_device_register(
	const char *type, int trips, int mask, void *devdata,
	old_thermal_zone_device_ops_t *_ops,
	const struct thermal_zone_params *_tzp,
	int passive_delay, int polling_delay)
{
	struct thermal_zone_device_ops *ops =
		(struct thermal_zone_device_ops *) _ops;

	/* cast the const away */
	struct thermal_zone_params *tzp =
		(struct thermal_zone_params *)_tzp;

	return thermal_zone_device_register(type, trips, mask, devdata,
					    ops, tzp, passive_delay,
					    polling_delay);
}

static inline
void old_thermal_zone_device_unregister(struct thermal_zone_device *dev)
{
	thermal_zone_device_unregister(dev);
}

#ifndef CONFIG_BTNS_PMIC
struct backport_thermal_zone_device_ops {
	int (*bind) (struct thermal_zone_device *,
		     struct thermal_cooling_device *);
	int (*unbind) (struct thermal_zone_device *,
		       struct thermal_cooling_device *);
	int (*get_temp) (struct thermal_zone_device *, int *);
	int (*get_mode) (struct thermal_zone_device *,
			 enum thermal_device_mode *);
	int (*set_mode) (struct thermal_zone_device *,
		enum thermal_device_mode);
	int (*get_trip_type) (struct thermal_zone_device *, int,
		enum thermal_trip_type *);
	int (*get_trip_temp) (struct thermal_zone_device *, int, int *);
	int (*set_trip_temp) (struct thermal_zone_device *, int, int);
	int (*get_trip_hyst) (struct thermal_zone_device *, int, int *);
	int (*set_trip_hyst) (struct thermal_zone_device *, int, int);
	int (*get_crit_temp) (struct thermal_zone_device *, int *);
	int (*set_emul_temp) (struct thermal_zone_device *, int);
	int (*get_trend) (struct thermal_zone_device *, int,
			  enum thermal_trend *);
	int (*notify) (struct thermal_zone_device *, int,
		       enum thermal_trip_type);
};
#else /* CONFIG_BTNS_PMIC */
struct backport_thermal_zone_device_ops {
	int (*bind) (struct thermal_zone_device *,
		     struct thermal_cooling_device *);
	int (*unbind) (struct thermal_zone_device *,
		       struct thermal_cooling_device *);
	int (*get_temp) (struct thermal_zone_device *, int *);
	int (*get_mode) (struct thermal_zone_device *,
			 enum thermal_device_mode *);
	int (*set_mode) (struct thermal_zone_device *,
		enum thermal_device_mode);
	int (*get_trip_type) (struct thermal_zone_device *, int,
		enum thermal_trip_type *);
	int (*get_trip_temp) (struct thermal_zone_device *, int, int *);
	int (*set_trip_temp) (struct thermal_zone_device *, int, int);
	int (*get_trip_hyst) (struct thermal_zone_device *, int, int *);
	int (*set_trip_hyst) (struct thermal_zone_device *, int, int);
	int (*get_slope) (struct thermal_zone_device *, int *);
	int (*set_slope) (struct thermal_zone_device *, int);
	int (*get_intercept) (struct thermal_zone_device *, int *);
	int (*set_intercept) (struct thermal_zone_device *, int);
	int (*get_crit_temp) (struct thermal_zone_device *, int *);
	int (*set_emul_temp) (struct thermal_zone_device *, int);
	int (*get_trend) (struct thermal_zone_device *, int,
			  enum thermal_trend *);
	int (*notify) (struct thermal_zone_device *, int,
		       enum thermal_trip_type);
};
#endif /* CONFIG_BTNS_PMIC */
#define thermal_zone_device_ops LINUX_BACKPORT(thermal_zone_device_ops)

#undef thermal_zone_device_register
struct thermal_zone_device *backport_thermal_zone_device_register(
	const char *type, int trips, int mask, void *devdata,
	struct thermal_zone_device_ops *ops,
	const struct thermal_zone_params *tzp,
	int passive_delay, int polling_delay);

#define thermal_zone_device_register \
	LINUX_BACKPORT(thermal_zone_device_register)

#undef thermal_zone_device_unregister
void backport_thermal_zone_device_unregister(struct thermal_zone_device *);
#define thermal_zone_device_unregister			\
	LINUX_BACKPORT(thermal_zone_device_unregister)

#endif /* LINUX_VERSION_IS_LESS(4,3,0) */
#endif /* ! < 3.8.0 */
#endif /* CONFIG_THERMAL */

#endif /* __BACKPORT_LINUX_THERMAL_H */
