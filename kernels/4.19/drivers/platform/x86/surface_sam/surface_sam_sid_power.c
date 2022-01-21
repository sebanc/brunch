/*
 * Surface SID Battery/AC Driver.
 * Provides support for the battery and AC on 7th generation Surface devices.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>

#include "surface_sam_ssh.h"

#define SPWR_WARN	KERN_WARNING KBUILD_MODNAME ": "
#define SPWR_DEBUG	KERN_DEBUG KBUILD_MODNAME ": "


// TODO: check BIX/BST for unknown/unsupported 0xffffffff entries
// TODO: DPTF (/SAN notifications)?
// TODO: other properties?


static unsigned int cache_time = 1000;
module_param(cache_time, uint, 0644);
MODULE_PARM_DESC(cache_time, "battery state chaching time in milliseconds [default: 1000]");

#define SPWR_AC_BAT_UPDATE_DELAY	msecs_to_jiffies(5000)


/*
 * SAM Interface.
 */

#define SAM_PWR_TC			0x02
#define SAM_PWR_RQID			0x0002

#define SAM_RQST_PWR_CID_STA		0x01
#define SAM_RQST_PWR_CID_BIX		0x02
#define SAM_RQST_PWR_CID_BST		0x03
#define SAM_RQST_PWR_CID_BTP		0x04

#define SAM_RQST_PWR_CID_PMAX		0x0b
#define SAM_RQST_PWR_CID_PSOC		0x0c
#define SAM_RQST_PWR_CID_PSRC		0x0d
#define SAM_RQST_PWR_CID_CHGI		0x0e
#define SAM_RQST_PWR_CID_ARTG		0x0f

#define SAM_EVENT_PWR_CID_BIX		0x15
#define SAM_EVENT_PWR_CID_BST		0x16
#define SAM_EVENT_PWR_CID_ADAPTER	0x17
#define SAM_EVENT_PWR_CID_DPTF		0x4f

#define SAM_BATTERY_STA_OK		0x0f
#define SAM_BATTERY_STA_PRESENT		0x10

#define SAM_BATTERY_STATE_DISCHARGING	0x01
#define SAM_BATTERY_STATE_CHARGING	0x02
#define SAM_BATTERY_STATE_CRITICAL	0x04

#define SAM_BATTERY_POWER_UNIT_MA	1


/* Equivalent to data returned in ACPI _BIX method */
struct spwr_bix {
	u8  revision;
	u32 power_unit;
	u32 design_cap;
	u32 last_full_charge_cap;
	u32 technology;
	u32 design_voltage;
	u32 design_cap_warn;
	u32 design_cap_low;
	u32 cycle_count;
	u32 measurement_accuracy;
	u32 max_sampling_time;
	u32 min_sampling_time;
	u32 max_avg_interval;
	u32 min_avg_interval;
	u32 bat_cap_granularity_1;
	u32 bat_cap_granularity_2;
	u8  model[21];
	u8  serial[11];
	u8  type[5];
	u8  oem_info[21];
} __packed;

/* Equivalent to data returned in ACPI _BST method */
struct spwr_bst {
	u32 state;
	u32 present_rate;
	u32 remaining_cap;
	u32 present_voltage;
} __packed;

/* DPTF event payload */
struct spwr_event_dptf {
	u32 pmax;
	u32 _1;		/* currently unknown */
	u32 _2;		/* currently unknown */
} __packed;


/* Get battery status (_STA) */
static int sam_psy_get_sta(u8 iid, u32 *sta)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_STA;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(u32);
	result.len = 0;
	result.data = (u8 *)sta;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Get battery static information (_BIX) */
static int sam_psy_get_bix(u8 iid, struct spwr_bix *bix)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_BIX;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(struct spwr_bix);
	result.len = 0;
	result.data = (u8 *)bix;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Get battery dynamic information (_BST) */
static int sam_psy_get_bst(u8 iid, struct spwr_bst *bst)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_BST;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(struct spwr_bst);
	result.len = 0;
	result.data = (u8 *)bst;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Set battery trip point (_BTP) */
static int sam_psy_set_btp(u8 iid, u32 btp)
{
	struct surface_sam_ssh_rqst rqst;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_BTP;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x00;
	rqst.cdl = sizeof(u32);
	rqst.pld = (u8 *)&btp;

	return surface_sam_ssh_rqst(&rqst, NULL);
}

/* Get platform power soruce for battery (DPTF PSRC) */
static int sam_psy_get_psrc(u8 iid, u32 *psrc)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_PSRC;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(u32);
	result.len = 0;
	result.data = (u8 *)psrc;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Get maximum platform power for battery (DPTF PMAX) */
__always_unused
static int sam_psy_get_pmax(u8 iid, u32 *pmax)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_PMAX;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(u32);
	result.len = 0;
	result.data = (u8 *)pmax;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Get adapter rating (DPTF ARTG) */
__always_unused
static int sam_psy_get_artg(u8 iid, u32 *artg)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_ARTG;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(u32);
	result.len = 0;
	result.data = (u8 *)artg;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Unknown (DPTF PSOC) */
__always_unused
static int sam_psy_get_psoc(u8 iid, u32 *psoc)
{
	struct surface_sam_ssh_rqst rqst;
	struct surface_sam_ssh_buf result;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_PSOC;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x01;
	rqst.cdl = 0x00;
	rqst.pld = NULL;

	result.cap = sizeof(u32);
	result.len = 0;
	result.data = (u8 *)psoc;

	return surface_sam_ssh_rqst(&rqst, &result);
}

/* Unknown (DPTF CHGI/ INT3403 SPPC) */
__always_unused
static int sam_psy_set_chgi(u8 iid, u32 chgi)
{
	struct surface_sam_ssh_rqst rqst;

	rqst.tc  = SAM_PWR_TC;
	rqst.cid = SAM_RQST_PWR_CID_CHGI;
	rqst.iid = iid;
	rqst.pri = SURFACE_SAM_PRIORITY_NORMAL;
	rqst.snc = 0x00;
	rqst.cdl = sizeof(u32);
	rqst.pld = (u8 *)&chgi;

	return surface_sam_ssh_rqst(&rqst, NULL);
}


/*
 * Common Power-Subsystem Interface.
 */

enum spwr_battery_id {
	SPWR_BAT1,
	SPWR_BAT2,
	__SPWR_NUM_BAT,
};
#define SPWR_BAT_SINGLE		PLATFORM_DEVID_NONE

struct spwr_battery_device {
	struct platform_device *pdev;
	enum spwr_battery_id id;

	char name[32];
	struct power_supply *psy;
	struct power_supply_desc psy_desc;

	struct delayed_work update_work;

	struct mutex lock;
	unsigned long timestamp;

	u32 sta;
	struct spwr_bix bix;
	struct spwr_bst bst;
	u32 alarm;
};

struct spwr_ac_device {
	struct platform_device *pdev;

	char name[32];
	struct power_supply *psy;
	struct power_supply_desc psy_desc;

	struct mutex lock;

	u32 state;
};

struct spwr_subsystem {
	struct mutex lock;

	unsigned refcount;
	struct spwr_ac_device *ac;
	struct spwr_battery_device *battery[__SPWR_NUM_BAT];
};

static struct spwr_subsystem spwr_subsystem = {
	.lock = __MUTEX_INITIALIZER(spwr_subsystem.lock),
};

static enum power_supply_property spwr_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property spwr_battery_props_chg[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

static enum power_supply_property spwr_battery_props_eng[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
};


static int spwr_battery_register(struct spwr_battery_device *bat, struct platform_device *pdev,
				 enum spwr_battery_id id);

static int spwr_battery_unregister(struct spwr_battery_device *bat);


inline static bool spwr_battery_present(struct spwr_battery_device *bat)
{
	return bat->sta & SAM_BATTERY_STA_PRESENT;
}


inline static int spwr_battery_load_sta(struct spwr_battery_device *bat)
{
	return sam_psy_get_sta(bat->id + 1, &bat->sta);
}

inline static int spwr_battery_load_bix(struct spwr_battery_device *bat)
{
	if (!spwr_battery_present(bat))
		return 0;

	return sam_psy_get_bix(bat->id + 1, &bat->bix);
}

inline static int spwr_battery_load_bst(struct spwr_battery_device *bat)
{
	if (!spwr_battery_present(bat))
		return 0;

	return sam_psy_get_bst(bat->id + 1, &bat->bst);
}


inline static int spwr_battery_set_alarm_unlocked(struct spwr_battery_device *bat, u32 value)
{
	bat->alarm = value;
	return sam_psy_set_btp(bat->id + 1, bat->alarm);
}

inline static int spwr_battery_set_alarm(struct spwr_battery_device *bat, u32 value)
{
	int status;

	mutex_lock(&bat->lock);
	status = spwr_battery_set_alarm_unlocked(bat, value);
	mutex_unlock(&bat->lock);

	return status;
}

inline static int spwr_battery_update_bst_unlocked(struct spwr_battery_device *bat, bool cached)
{
	unsigned long cache_deadline = bat->timestamp + msecs_to_jiffies(cache_time);
	int status;

	if (cached && bat->timestamp && time_is_after_jiffies(cache_deadline))
		return 0;

	status = spwr_battery_load_sta(bat);
	if (status)
		return status;

	status = spwr_battery_load_bst(bat);
	if (status)
		return status;

	bat->timestamp = jiffies;
	return 0;
}

static int spwr_battery_update_bst(struct spwr_battery_device *bat, bool cached)
{
	int status;

	mutex_lock(&bat->lock);
	status = spwr_battery_update_bst_unlocked(bat, cached);
	mutex_unlock(&bat->lock);

	return status;
}

inline static int spwr_battery_update_bix_unlocked(struct spwr_battery_device *bat)
{
	int status;

	status = spwr_battery_load_sta(bat);
	if (status)
		return status;

	status = spwr_battery_load_bix(bat);
	if (status)
		return status;

	status = spwr_battery_load_bst(bat);
	if (status)
		return status;

	bat->timestamp = jiffies;
	return 0;
}

static int spwr_battery_update_bix(struct spwr_battery_device *bat)
{
	int status;

	mutex_lock(&bat->lock);
	status = spwr_battery_update_bix_unlocked(bat);
	mutex_unlock(&bat->lock);

	return status;
}

inline static int spwr_ac_update_unlocked(struct spwr_ac_device *ac)
{
	return sam_psy_get_psrc(0x00, &ac->state);
}

static int spwr_ac_update(struct spwr_ac_device *ac)
{
	int status;

	mutex_lock(&ac->lock);
	status = spwr_ac_update_unlocked(ac);
	mutex_unlock(&ac->lock);

	return status;
}


static int spwr_battery_recheck(struct spwr_battery_device *bat)
{
	bool present = spwr_battery_present(bat);
	u32 unit = bat->bix.power_unit;
	int status;

	status = spwr_battery_update_bix(bat);
	if (status)
		return status;

	// if battery has been attached, (re-)initialize alarm
	if (!present && spwr_battery_present(bat)) {
		status = spwr_battery_set_alarm(bat, bat->bix.design_cap_warn);
		if (status)
			return status;
	}

	// if the unit has changed, re-add the battery
	if (unit != bat->bix.power_unit) {
		mutex_unlock(&spwr_subsystem.lock);

		status = spwr_battery_unregister(bat);
		if (status)
			return status;

		status = spwr_battery_register(bat, bat->pdev, bat->id);
	}

	return status;
}


static int spwr_handle_event_bix(struct surface_sam_ssh_event *event)
{
	struct spwr_battery_device *bat;
	enum spwr_battery_id bat_id = event->iid - 1;
	int status = 0;

	if (bat_id < 0 || bat_id >= __SPWR_NUM_BAT) {
		printk(SPWR_WARN "invalid BIX event iid 0x%02x\n", event->iid);
		bat_id = SPWR_BAT1;
	}

	mutex_lock(&spwr_subsystem.lock);
	bat = spwr_subsystem.battery[bat_id];
	if (bat) {
		status = spwr_battery_recheck(bat);
		if (!status)
			power_supply_changed(bat->psy);
	}

	mutex_unlock(&spwr_subsystem.lock);
	return status;
}

static int spwr_handle_event_bst(struct surface_sam_ssh_event *event)
{
	struct spwr_battery_device *bat;
	enum spwr_battery_id bat_id = event->iid - 1;
	int status = 0;

	if (bat_id < 0 || bat_id >= __SPWR_NUM_BAT) {
		printk(SPWR_WARN "invalid BST event iid 0x%02x\n", event->iid);
		bat_id = SPWR_BAT1;
	}

	mutex_lock(&spwr_subsystem.lock);

	bat = spwr_subsystem.battery[bat_id];
	if (bat) {
		status = spwr_battery_update_bst(bat, false);
		if (!status)
			power_supply_changed(bat->psy);
	}

	mutex_unlock(&spwr_subsystem.lock);
	return status;
}

static int spwr_handle_event_adapter(struct surface_sam_ssh_event *event)
{
	struct spwr_battery_device *bat1 = NULL;
	struct spwr_battery_device *bat2 = NULL;
	struct spwr_ac_device *ac;
	int status = 0;

	mutex_lock(&spwr_subsystem.lock);

	ac = spwr_subsystem.ac;
	if (ac) {
		status = spwr_ac_update(ac);
		if (status)
			goto out;

		power_supply_changed(ac->psy);
	}

	/*
	 * Handle battery update quirk:
	 * When the battery is fully charged and the adapter is plugged in or
	 * removed, the EC does not send a separate event for the state
	 * (charging/discharging) change. Furthermore it may take some time until
	 * the state is updated on the battery. Schedule an update to solve this.
	 */

	bat1 = spwr_subsystem.battery[SPWR_BAT1];
	if (bat1 && bat1->bst.remaining_cap >= bat1->bix.last_full_charge_cap)
		schedule_delayed_work(&bat1->update_work, SPWR_AC_BAT_UPDATE_DELAY);

	bat2 = spwr_subsystem.battery[SPWR_BAT2];
	if (bat2 && bat2->bst.remaining_cap >= bat2->bix.last_full_charge_cap)
		schedule_delayed_work(&bat2->update_work, SPWR_AC_BAT_UPDATE_DELAY);

out:
	mutex_unlock(&spwr_subsystem.lock);
	return status;
}

static int spwr_handle_event_dptf(struct surface_sam_ssh_event *event)
{
	return 0;	// TODO: spwr_handle_event_dptf
}

static int spwr_handle_event(struct surface_sam_ssh_event *event, void *data)
{
	printk(SPWR_DEBUG "power event (cid = 0x%02x)\n", event->cid);

	switch (event->cid) {
	case SAM_EVENT_PWR_CID_BIX:
		return spwr_handle_event_bix(event);

	case SAM_EVENT_PWR_CID_BST:
		return spwr_handle_event_bst(event);

	case SAM_EVENT_PWR_CID_ADAPTER:
		return spwr_handle_event_adapter(event);

	case SAM_EVENT_PWR_CID_DPTF:
		return spwr_handle_event_dptf(event);

	default:
		printk(SPWR_WARN "unhandled power event (cid = 0x%02x)\n", event->cid);
		return 0;
	}
}

static void spwr_battery_update_bst_workfn(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct spwr_battery_device *bat = container_of(dwork, struct spwr_battery_device, update_work);
	int status;

	status = spwr_battery_update_bst(bat, false);
	if (!status)
		power_supply_changed(bat->psy);

	if (status)
		dev_err(&bat->pdev->dev, "failed to update battery state: %d\n", status);
}


inline static int spwr_battery_prop_status(struct spwr_battery_device *bat)
{
	if (bat->bst.state & SAM_BATTERY_STATE_DISCHARGING)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (bat->bst.state & SAM_BATTERY_STATE_CHARGING)
		return POWER_SUPPLY_STATUS_CHARGING;

	if (bat->bix.last_full_charge_cap == bat->bst.remaining_cap)
		return POWER_SUPPLY_STATUS_FULL;

	if (bat->bst.present_rate == 0)
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	return POWER_SUPPLY_STATUS_UNKNOWN;
}

inline static int spwr_battery_prop_technology(struct spwr_battery_device *bat)
{
	if (!strcasecmp("NiCd", bat->bix.type))
		return POWER_SUPPLY_TECHNOLOGY_NiCd;

	if (!strcasecmp("NiMH", bat->bix.type))
		return POWER_SUPPLY_TECHNOLOGY_NiMH;

	if (!strcasecmp("LION", bat->bix.type))
		return POWER_SUPPLY_TECHNOLOGY_LION;

	if (!strncasecmp("LI-ION", bat->bix.type, 6))
		return POWER_SUPPLY_TECHNOLOGY_LION;

	if (!strcasecmp("LiP", bat->bix.type))
		return POWER_SUPPLY_TECHNOLOGY_LIPO;

	return POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
}

inline static int spwr_battery_prop_capacity(struct spwr_battery_device *bat)
{
	if (bat->bst.remaining_cap && bat->bix.last_full_charge_cap)
		return bat->bst.remaining_cap * 100 / bat->bix.last_full_charge_cap;
	else
		return 0;
}

inline static int spwr_battery_prop_capacity_level(struct spwr_battery_device *bat)
{
	if (bat->bst.state & SAM_BATTERY_STATE_CRITICAL)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;

	if (bat->bst.remaining_cap >= bat->bix.last_full_charge_cap)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;

	if (bat->bst.remaining_cap <= bat->alarm)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;

	return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
}

static int spwr_ac_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct spwr_ac_device *ac = power_supply_get_drvdata(psy);
	int status;

	mutex_lock(&ac->lock);

	status = spwr_ac_update_unlocked(ac);
	if (status)
		goto out;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ac->state == 1;
		break;

	default:
		status = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&ac->lock);
	return status;
}

static int spwr_battery_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	struct spwr_battery_device *bat = power_supply_get_drvdata(psy);
	int status;

	mutex_lock(&bat->lock);

	status = spwr_battery_update_bst_unlocked(bat, true);
	if (status)
		goto out;

	// abort if battery is not present
	if (!spwr_battery_present(bat) && psp != POWER_SUPPLY_PROP_PRESENT) {
		status = -ENODEV;
		goto out;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = spwr_battery_prop_status(bat);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = spwr_battery_present(bat);
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = spwr_battery_prop_technology(bat);
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = bat->bix.cycle_count;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = bat->bix.design_voltage * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bat->bst.present_voltage * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_POWER_NOW:
		val->intval = bat->bst.present_rate * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = bat->bix.design_cap * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		val->intval = bat->bix.last_full_charge_cap * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		val->intval = bat->bst.remaining_cap * 1000;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = spwr_battery_prop_capacity(bat);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = spwr_battery_prop_capacity_level(bat);
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = bat->bix.model;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = bat->bix.oem_info;
		break;

	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = bat->bix.serial;
		break;

	default:
		status = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&bat->lock);
	return status;
}


static ssize_t spwr_battery_alarm_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct spwr_battery_device *bat = power_supply_get_drvdata(psy);

	return sprintf(buf, "%d\n", bat->alarm * 1000);
}

static ssize_t spwr_battery_alarm_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct spwr_battery_device *bat = power_supply_get_drvdata(psy);
	unsigned long value;
	int status;

	status = kstrtoul(buf, 0, &value);
	if (status)
		return status;

	if (!spwr_battery_present(bat))
		return -ENODEV;

	status = spwr_battery_set_alarm(bat, value / 1000);
	if (status)
		return status;

	return count;
}

static const struct device_attribute alarm_attr = {
	.attr = {.name = "alarm", .mode = 0644},
	.show = spwr_battery_alarm_show,
	.store = spwr_battery_alarm_store,
};


static int spwr_subsys_init_unlocked(void)
{
	int status;

	status = surface_sam_ssh_set_event_handler(SAM_PWR_RQID, spwr_handle_event, NULL);
	if (status) {
		goto err_handler;
	}

	status = surface_sam_ssh_enable_event_source(SAM_PWR_TC, 0x01, SAM_PWR_RQID);
	if (status) {
		goto err_source;
	}

	return 0;

err_source:
	surface_sam_ssh_remove_event_handler(SAM_PWR_RQID);
err_handler:
	return status;
}

static int spwr_subsys_deinit_unlocked(void)
{
	surface_sam_ssh_disable_event_source(SAM_PWR_TC, 0x01, SAM_PWR_RQID);
	surface_sam_ssh_remove_event_handler(SAM_PWR_RQID);
	return 0;
}

static inline int spwr_subsys_ref_unlocked(void)
{
	int status = 0;

	if (!spwr_subsystem.refcount)
		status = spwr_subsys_init_unlocked();

	spwr_subsystem.refcount += 1;
	return status;
}

static inline int spwr_subsys_unref_unlocked(void)
{
	int status = 0;

	if (spwr_subsystem.refcount)
		spwr_subsystem.refcount -= 1;

	if (!spwr_subsystem.refcount)
		status = spwr_subsys_deinit_unlocked();

	return status;
}


static int spwr_ac_register(struct spwr_ac_device *ac, struct platform_device *pdev)
{
	struct power_supply_config psy_cfg = {};
	u32 sta;
	int status;

	// make sure the device is there and functioning properly
	status = sam_psy_get_sta(0x00, &sta);
	if (status)
		return status;

	if ((sta & SAM_BATTERY_STA_OK) != SAM_BATTERY_STA_OK)
		return -ENODEV;

	psy_cfg.drv_data = ac;

	ac->pdev = pdev;
	mutex_init(&ac->lock);

	snprintf(ac->name, ARRAY_SIZE(ac->name), "ADP0");

	ac->psy_desc.name = ac->name;
	ac->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
	ac->psy_desc.properties = spwr_ac_props;
	ac->psy_desc.num_properties = ARRAY_SIZE(spwr_ac_props);
	ac->psy_desc.get_property = spwr_ac_get_property;

	mutex_lock(&spwr_subsystem.lock);
	if (spwr_subsystem.ac) {
		status = -EEXIST;
		goto err;
	}

	status = spwr_subsys_ref_unlocked();
	if (status)
		goto err;

	ac->psy = power_supply_register(&ac->pdev->dev, &ac->psy_desc, &psy_cfg);
	if (IS_ERR(ac->psy)) {
		status = PTR_ERR(ac->psy);
		goto err_unref;
	}

	spwr_subsystem.ac = ac;
	mutex_unlock(&spwr_subsystem.lock);
	return 0;

err_unref:
	spwr_subsys_unref_unlocked();
err:
	mutex_unlock(&spwr_subsystem.lock);
	mutex_destroy(&ac->lock);
	return status;
}

static int spwr_ac_unregister(struct spwr_ac_device *ac)
{
	int status;

	mutex_lock(&spwr_subsystem.lock);
	if (spwr_subsystem.ac != ac) {
		mutex_unlock(&spwr_subsystem.lock);
		return -EINVAL;
	}

	spwr_subsystem.ac = NULL;
	power_supply_unregister(ac->psy);

	status = spwr_subsys_unref_unlocked();
	mutex_unlock(&spwr_subsystem.lock);

	mutex_destroy(&ac->lock);
	return status;
}

static int spwr_battery_register(struct spwr_battery_device *bat, struct platform_device *pdev,
				 enum spwr_battery_id id)
{
	struct power_supply_config psy_cfg = {};
	u32 sta;
	int status;

	if ((id < 0 || id >= __SPWR_NUM_BAT) && id != SPWR_BAT_SINGLE)
		return -EINVAL;

	bat->pdev = pdev;
	bat->id = id != SPWR_BAT_SINGLE ? id : SPWR_BAT1;

	// make sure the device is there and functioning properly
	status = sam_psy_get_sta(bat->id + 1, &sta);
	if (status)
		return status;

	if ((sta & SAM_BATTERY_STA_OK) != SAM_BATTERY_STA_OK)
		return -ENODEV;

	status = spwr_battery_update_bix_unlocked(bat);
	if (status)
		return status;

	if (spwr_battery_present(bat)) {
		status = spwr_battery_set_alarm_unlocked(bat, bat->bix.design_cap_warn);
		if (status)
			return status;
	}

	snprintf(bat->name, ARRAY_SIZE(bat->name), "BAT%d", bat->id);
	bat->psy_desc.name = bat->name;
	bat->psy_desc.type = POWER_SUPPLY_TYPE_BATTERY;

	if (bat->bix.power_unit == SAM_BATTERY_POWER_UNIT_MA) {
		bat->psy_desc.properties = spwr_battery_props_chg;
		bat->psy_desc.num_properties = ARRAY_SIZE(spwr_battery_props_chg);
	} else {
		bat->psy_desc.properties = spwr_battery_props_eng;
		bat->psy_desc.num_properties = ARRAY_SIZE(spwr_battery_props_eng);
	}

	bat->psy_desc.get_property = spwr_battery_get_property;

	mutex_init(&bat->lock);
	psy_cfg.drv_data = bat;

	INIT_DELAYED_WORK(&bat->update_work, spwr_battery_update_bst_workfn);

	mutex_lock(&spwr_subsystem.lock);
	if (spwr_subsystem.battery[bat->id]) {
		status = -EEXIST;
		goto err;
	}

	status = spwr_subsys_ref_unlocked();
	if (status)
		goto err;

	bat->psy = power_supply_register(&bat->pdev->dev, &bat->psy_desc, &psy_cfg);
	if (IS_ERR(bat->psy)) {
		status = PTR_ERR(bat->psy);
		goto err_unref;
	}

	status = device_create_file(&bat->psy->dev, &alarm_attr);
	if (status)
		goto err_dereg;

	spwr_subsystem.battery[bat->id] = bat;
	mutex_unlock(&spwr_subsystem.lock);
	return 0;

err_dereg:
	power_supply_unregister(bat->psy);
err_unref:
	spwr_subsys_unref_unlocked();
err:
	mutex_unlock(&spwr_subsystem.lock);
	return status;
}

static int spwr_battery_unregister(struct spwr_battery_device *bat)
{
	int status;

	if (bat->id < 0 || bat->id >= __SPWR_NUM_BAT)
		return -EINVAL ;

	mutex_lock(&spwr_subsystem.lock);
	if (spwr_subsystem.battery[bat->id] != bat) {
		mutex_unlock(&spwr_subsystem.lock);
		return -EINVAL;
	}

	spwr_subsystem.battery[bat->id] = NULL;

	status = spwr_subsys_unref_unlocked();
	mutex_unlock(&spwr_subsystem.lock);

	cancel_delayed_work_sync(&bat->update_work);
	device_remove_file(&bat->psy->dev, &alarm_attr);
	power_supply_unregister(bat->psy);

	mutex_destroy(&bat->lock);
	return status;
}


/*
 * Battery Driver.
 */

#ifdef CONFIG_PM_SLEEP
static int surface_sam_sid_battery_resume(struct device *dev)
{
	struct spwr_battery_device *bat = dev_get_drvdata(dev);
	return spwr_battery_recheck(bat);
}
#else
#define surface_sam_sid_battery_resume NULL
#endif

SIMPLE_DEV_PM_OPS(surface_sam_sid_battery_pm, NULL, surface_sam_sid_battery_resume);

static int surface_sam_sid_battery_probe(struct platform_device *pdev)
{
	int status;
	struct spwr_battery_device *bat;

	// link to ec
	status = surface_sam_ssh_consumer_register(&pdev->dev);
	if (status)
		return status == -ENXIO ? -EPROBE_DEFER : status;

	bat = devm_kzalloc(&pdev->dev, sizeof(struct spwr_battery_device), GFP_KERNEL);
	if (!bat)
		return -ENOMEM;

	platform_set_drvdata(pdev, bat);
	return spwr_battery_register(bat, pdev, pdev->id);
}

static int surface_sam_sid_battery_remove(struct platform_device *pdev)
{
	struct spwr_battery_device *bat = platform_get_drvdata(pdev);
	return spwr_battery_unregister(bat);
}

static struct platform_driver surface_sam_sid_battery = {
	.probe = surface_sam_sid_battery_probe,
	.remove = surface_sam_sid_battery_remove,
	.driver = {
		.name = "surface_sam_sid_battery",
		.pm = &surface_sam_sid_battery_pm,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


/*
 * AC Driver.
 */

static int surface_sam_sid_ac_probe(struct platform_device *pdev)
{
	int status;
	struct spwr_ac_device *ac;

	// link to ec
	status = surface_sam_ssh_consumer_register(&pdev->dev);
	if (status)
		return status == -ENXIO ? -EPROBE_DEFER : status;

	ac = devm_kzalloc(&pdev->dev, sizeof(struct spwr_ac_device), GFP_KERNEL);
	if (!ac)
		return -ENOMEM;

	status = spwr_ac_register(ac, pdev);
	if (status)
		return status;

	platform_set_drvdata(pdev, ac);
	return 0;
}

static int surface_sam_sid_ac_remove(struct platform_device *pdev)
{
	struct spwr_ac_device *ac = platform_get_drvdata(pdev);
	return spwr_ac_unregister(ac);
}

static struct platform_driver surface_sam_sid_ac = {
	.probe = surface_sam_sid_ac_probe,
	.remove = surface_sam_sid_ac_remove,
	.driver = {
		.name = "surface_sam_sid_ac",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};


static int __init surface_sam_sid_power_init(void)
{
	int status;

	status = platform_driver_register(&surface_sam_sid_battery);
	if (status)
		return status;

	status = platform_driver_register(&surface_sam_sid_ac);
	if (status) {
		platform_driver_unregister(&surface_sam_sid_battery);
		return status;
	}

	return 0;
}

static void __exit surface_sam_sid_power_exit(void)
{
	platform_driver_unregister(&surface_sam_sid_battery);
	platform_driver_unregister(&surface_sam_sid_ac);
}

module_init(surface_sam_sid_power_init);
module_exit(surface_sam_sid_power_exit);

MODULE_AUTHOR("Maximilian Luz <luzmaximilian@gmail.com>");
MODULE_DESCRIPTION("Surface Battery/AC Driver for 7th Generation Surface Devices");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:surface_sam_sid_ac");
MODULE_ALIAS("platform:surface_sam_sid_battery");
