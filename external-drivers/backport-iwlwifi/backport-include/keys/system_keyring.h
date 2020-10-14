#ifndef __BP_SYSTEM_KEYRING_H
#define __BP_SYSTEM_KEYRING_H
#ifndef CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION
#include_next <keys/system_keyring.h>
#else
#include <linux/key.h>

#define is_hash_blacklisted(...)	0
#endif /* CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION */
#endif /* __BP_SYSTEM_KEYRING_H */
