/* Automatically created during backport process */
#ifndef CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION
#include_next <linux/oid_registry.h>
#else
#undef look_up_OID
#define look_up_OID LINUX_BACKPORT(look_up_OID)
#undef parse_OID
#define parse_OID LINUX_BACKPORT(parse_OID)
#undef sprint_oid
#define sprint_oid LINUX_BACKPORT(sprint_oid)
#undef sprint_OID
#define sprint_OID LINUX_BACKPORT(sprint_OID)
#include <linux/backport-oid_registry.h>
#endif /* CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION */
