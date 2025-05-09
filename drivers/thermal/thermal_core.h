/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  thermal_core.h
 *
 *  Copyright (C) 2012  Intel Corp
 *  Author: Durgadoss R <durgadoss.r@intel.com>
 */

#ifndef __THERMAL_CORE_H__
#define __THERMAL_CORE_H__

#include <linux/cleanup.h>
#include <linux/device.h>
#include <linux/thermal.h>

#include "thermal_netlink.h"
#include "thermal_thresholds.h"
#include "thermal_debugfs.h"

struct thermal_attr {
	struct device_attribute attr;
	char name[THERMAL_NAME_LENGTH];
};

struct thermal_trip_attrs {
	struct thermal_attr type;
	struct thermal_attr temp;
	struct thermal_attr hyst;
};

struct thermal_trip_desc {
	struct thermal_trip trip;
	struct thermal_trip_attrs trip_attrs;
	struct list_head list_node;
	struct list_head thermal_instances;
	int threshold;
};

/**
 * struct thermal_governor - structure that holds thermal governor information
 * @name:	name of the governor
 * @bind_to_tz: callback called when binding to a thermal zone.  If it
 *		returns 0, the governor is bound to the thermal zone,
 *		otherwise it fails.
 * @unbind_from_tz:	callback called when a governor is unbound from a
 *			thermal zone.
 * @trip_crossed:	called for trip points that have just been crossed
 * @manage:	called on thermal zone temperature updates
 * @update_tz:	callback called when thermal zone internals have changed, e.g.
 *		thermal cooling instance was added/removed
 * @governor_list:	node in thermal_governor_list (in thermal_core.c)
 */
struct thermal_governor {
	const char *name;
	int (*bind_to_tz)(struct thermal_zone_device *tz);
	void (*unbind_from_tz)(struct thermal_zone_device *tz);
	void (*trip_crossed)(struct thermal_zone_device *tz,
			     const struct thermal_trip *trip,
			     bool upward);
	void (*manage)(struct thermal_zone_device *tz);
	void (*update_tz)(struct thermal_zone_device *tz,
			  enum thermal_notify_event reason);
	struct list_head	governor_list;
};

#define	TZ_STATE_FLAG_SUSPENDED	BIT(0)
#define	TZ_STATE_FLAG_RESUMING	BIT(1)
#define	TZ_STATE_FLAG_INIT	BIT(2)
#define	TZ_STATE_FLAG_EXIT	BIT(3)

#define TZ_STATE_READY		0

/**
 * struct thermal_zone_device - structure for a thermal zone
 * @id:		unique id number for each thermal zone
 * @type:	the thermal zone device type
 * @device:	&struct device for this thermal zone
 * @removal:	removal completion
 * @resume:	resume completion
 * @trips_high:	trips above the current zone temperature
 * @trips_reached:	trips below or at the current zone temperature
 * @trips_invalid:	trips with invalid temperature
 * @mode:		current mode of this thermal zone
 * @devdata:	private pointer for device private data
 * @num_trips:	number of trip points the thermal zone supports
 * @passive_delay_jiffies: number of jiffies to wait between polls when
 *			performing passive cooling.
 * @polling_delay_jiffies: number of jiffies to wait between polls when
 *			checking whether trip points have been crossed (0 for
 *			interrupt driven systems)
 * @recheck_delay_jiffies: delay after a failed attempt to determine the zone
 * 			temperature before trying again
 * @temperature:	current temperature.  This is only for core code,
 *			drivers should use thermal_zone_get_temp() to get the
 *			current temperature
 * @last_temperature:	previous temperature read
 * @emul_temperature:	emulated temperature when using CONFIG_THERMAL_EMULATION
 * @passive:		1 if you've crossed a passive trip point, 0 otherwise.
 * @prev_low_trip:	the low current temperature if you've crossed a passive
			trip point.
 * @prev_high_trip:	the above current temperature if you've crossed a
			passive trip point.
 * @ops:	operations this &thermal_zone_device supports
 * @tzp:	thermal zone parameters
 * @governor:	pointer to the governor for this thermal zone
 * @governor_data:	private pointer for governor data
 * @ida:	&struct ida to generate unique id for this zone's cooling
 *		devices
 * @lock:	lock to protect thermal_instances list
 * @node:	node in thermal_tz_list (in thermal_core.c)
 * @poll_queue:	delayed work for polling
 * @notify_event: Last notification event
 * @state: 	current state of the thermal zone
 * @trips:	array of struct thermal_trip objects
 */
struct thermal_zone_device {
	int id;
	char type[THERMAL_NAME_LENGTH];
	struct device device;
	struct completion removal;
	struct completion resume;
	struct attribute_group trips_attribute_group;
	struct list_head trips_high;
	struct list_head trips_reached;
	struct list_head trips_invalid;
	enum thermal_device_mode mode;
	void *devdata;
	int num_trips;
	unsigned long passive_delay_jiffies;
	unsigned long polling_delay_jiffies;
	unsigned long recheck_delay_jiffies;
	int temperature;
	int last_temperature;
	int emul_temperature;
	int passive;
	int prev_low_trip;
	int prev_high_trip;
	struct thermal_zone_device_ops ops;
	struct thermal_zone_params *tzp;
	struct thermal_governor *governor;
	void *governor_data;
	struct ida ida;
	struct mutex lock;
	struct list_head node;
	struct delayed_work poll_queue;
	enum thermal_notify_event notify_event;
	u8 state;
#ifdef CONFIG_THERMAL_DEBUGFS
	struct thermal_debugfs *debugfs;
#endif
	struct list_head user_thresholds;
	struct thermal_trip_desc trips[] __counted_by(num_trips);
};

DEFINE_GUARD(thermal_zone, struct thermal_zone_device *, mutex_lock(&_T->lock),
	     mutex_unlock(&_T->lock))

DEFINE_GUARD(thermal_zone_reverse, struct thermal_zone_device *,
	     mutex_unlock(&_T->lock), mutex_lock(&_T->lock))

/* Initial thermal zone temperature. */
#define THERMAL_TEMP_INIT	INT_MIN

/*
 * Default and maximum delay after a failed thermal zone temperature check
 * before attempting to check it again (in jiffies).
 */
#define THERMAL_RECHECK_DELAY		msecs_to_jiffies(250)
#define THERMAL_MAX_RECHECK_DELAY	(120 * HZ)

/* Default Thermal Governor */
#if defined(CONFIG_THERMAL_DEFAULT_GOV_STEP_WISE)
#define DEFAULT_THERMAL_GOVERNOR       "step_wise"
#elif defined(CONFIG_THERMAL_DEFAULT_GOV_FAIR_SHARE)
#define DEFAULT_THERMAL_GOVERNOR       "fair_share"
#elif defined(CONFIG_THERMAL_DEFAULT_GOV_USER_SPACE)
#define DEFAULT_THERMAL_GOVERNOR       "user_space"
#elif defined(CONFIG_THERMAL_DEFAULT_GOV_POWER_ALLOCATOR)
#define DEFAULT_THERMAL_GOVERNOR       "power_allocator"
#elif defined(CONFIG_THERMAL_DEFAULT_GOV_BANG_BANG)
#define DEFAULT_THERMAL_GOVERNOR       "bang_bang"
#endif

/* Initial state of a cooling device during binding */
#define THERMAL_NO_TARGET -1UL

/* Init section thermal table */
extern struct thermal_governor *__governor_thermal_table[];
extern struct thermal_governor *__governor_thermal_table_end[];

#define THERMAL_TABLE_ENTRY(table, name)			\
	static typeof(name) *__thermal_table_entry_##name	\
	__used __section("__" #table "_thermal_table") = &name

#define THERMAL_GOVERNOR_DECLARE(name)	THERMAL_TABLE_ENTRY(governor, name)

#define for_each_governor_table(__governor)		\
	for (__governor = __governor_thermal_table;	\
	     __governor < __governor_thermal_table_end;	\
	     __governor++)

int for_each_thermal_zone(int (*cb)(struct thermal_zone_device *, void *),
			  void *);

int for_each_thermal_cooling_device(int (*cb)(struct thermal_cooling_device *,
					      void *), void *);

int for_each_thermal_governor(int (*cb)(struct thermal_governor *, void *),
			      void *thermal_governor);

struct thermal_zone_device *thermal_zone_get_by_id(int id);

DEFINE_CLASS(thermal_zone_get_by_id, struct thermal_zone_device *,
	     if (_T) put_device(&_T->device), thermal_zone_get_by_id(id), int id)

static inline bool cdev_is_power_actor(struct thermal_cooling_device *cdev)
{
	return cdev->ops->get_requested_power && cdev->ops->state2power &&
		cdev->ops->power2state;
}

void thermal_cdev_update(struct thermal_cooling_device *);
void thermal_cdev_update_nocheck(struct thermal_cooling_device *cdev);
void __thermal_cdev_update(struct thermal_cooling_device *cdev);

int get_tz_trend(struct thermal_zone_device *tz, const struct thermal_trip *trip);

/*
 * This structure is used to describe the behavior of
 * a certain cooling device on a certain trip point
 * in a certain thermal zone
 */
struct thermal_instance {
	int id;
	char name[THERMAL_NAME_LENGTH];
	struct thermal_cooling_device *cdev;
	const struct thermal_trip *trip;
	bool initialized;
	unsigned long upper;	/* Highest cooling state for this trip point */
	unsigned long lower;	/* Lowest cooling state for this trip point */
	unsigned long target;	/* expected cooling state */
	char attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute attr;
	char weight_attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute weight_attr;
	struct list_head trip_node; /* node in trip->thermal_instances */
	struct list_head cdev_node; /* node in cdev->thermal_instances */
	unsigned int weight; /* The weight of the cooling device */
	bool upper_no_limit;
};

#define to_thermal_zone(_dev) \
	container_of(_dev, struct thermal_zone_device, device)

#define to_cooling_device(_dev)	\
	container_of(_dev, struct thermal_cooling_device, device)

int thermal_register_governor(struct thermal_governor *);
void thermal_unregister_governor(struct thermal_governor *);
int thermal_zone_device_set_policy(struct thermal_zone_device *, char *);
int thermal_build_list_of_policies(char *buf);
void __thermal_zone_device_update(struct thermal_zone_device *tz,
				  enum thermal_notify_event event);
void thermal_zone_device_critical_reboot(struct thermal_zone_device *tz);
void thermal_zone_device_critical_shutdown(struct thermal_zone_device *tz);
void thermal_governor_update_tz(struct thermal_zone_device *tz,
				enum thermal_notify_event reason);

/* Helpers */
#define for_each_trip_desc(__tz, __td)	\
	for (__td = __tz->trips; __td - __tz->trips < __tz->num_trips; __td++)

#define trip_to_trip_desc(__trip)	\
	container_of(__trip, struct thermal_trip_desc, trip)

const char *thermal_trip_type_name(enum thermal_trip_type trip_type);

void thermal_zone_set_trips(struct thermal_zone_device *tz, int low, int high);
int thermal_zone_trip_id(const struct thermal_zone_device *tz,
			 const struct thermal_trip *trip);
int __thermal_zone_get_temp(struct thermal_zone_device *tz, int *temp);
void thermal_zone_set_trip_hyst(struct thermal_zone_device *tz,
				struct thermal_trip *trip, int hyst);

/* sysfs I/F */
int thermal_zone_create_device_groups(struct thermal_zone_device *tz);
void thermal_zone_destroy_device_groups(struct thermal_zone_device *);
void thermal_cooling_device_setup_sysfs(struct thermal_cooling_device *);
void thermal_cooling_device_destroy_sysfs(struct thermal_cooling_device *cdev);
void thermal_cooling_device_stats_reinit(struct thermal_cooling_device *cdev);
/* used only at binding time */
ssize_t trip_point_show(struct device *, struct device_attribute *, char *);
ssize_t weight_show(struct device *, struct device_attribute *, char *);
ssize_t weight_store(struct device *, struct device_attribute *, const char *,
		     size_t);

#ifdef CONFIG_THERMAL_STATISTICS
void thermal_cooling_device_stats_update(struct thermal_cooling_device *cdev,
					 unsigned long new_state);
#else
static inline void
thermal_cooling_device_stats_update(struct thermal_cooling_device *cdev,
				    unsigned long new_state) {}
#endif /* CONFIG_THERMAL_STATISTICS */

#endif /* __THERMAL_CORE_H__ */
