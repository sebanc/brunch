/*
 * Interface for Surface ACPI/Notify (SAN).
 *
 * The SAN is the main interface between the Surface Serial Hub (SSH) and the
 * Surface/System Aggregator Module (SAM). It allows requests to be translated
 * from ACPI to SSH/SAM. It also interfaces with the discrete GPU hot-plug
 * driver.
 */

#ifndef _SURFACE_SAM_SAN_H
#define _SURFACE_SAM_SAN_H

#include <linux/types.h>


struct surface_sam_san_rqsg {
	u8 tc;				// target category
	u8 cid;				// command ID
	u8 iid;				// instance ID
	u8 cdl;				// command data length (lenght of payload)
	u8 *pld;			// pointer to payload of length cdl
};

typedef int (*surface_sam_san_rqsg_handler_fn)(struct surface_sam_san_rqsg *rqsg, void *data);

int surface_sam_san_consumer_register(struct device *consumer, u32 flags);
int surface_sam_san_set_rqsg_handler(surface_sam_san_rqsg_handler_fn fn, void *data);

#endif /* _SURFACE_SAM_SAN_H */
