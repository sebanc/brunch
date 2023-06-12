#ifndef __BP_KEY_H
#define __BP_KEY_H
#ifndef CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION
#include_next <linux/key.h>
#else
#include <linux/types.h>
#include <linux/refcount.h>
#include <linux/list.h>
#include <keys/asymmetric-type.h>

typedef uint32_t key_perm_t;

struct key {
	refcount_t refcount;
	const char *description;
	s32 serial;
	struct list_head list;

	struct asymmetric_key_ids kids;
	struct public_key *public_key;
	struct public_key_signature *sig;

	bool keyring;
};

typedef struct __key_reference_with_attributes *key_ref_t;

static inline key_ref_t make_key_ref(const struct key *key,
				     bool possession)
{
	return (key_ref_t) ((unsigned long) key | possession);
}

static inline struct key *key_ref_to_ptr(const key_ref_t key_ref)
{
	return (struct key *) ((unsigned long) key_ref & ~1UL);
}

#define key_put LINUX_BACKPORT(key_put)
extern void key_put(struct key *key);

static inline void key_ref_put(key_ref_t key_ref)
{
	key_put(key_ref_to_ptr(key_ref));
}

#define key_create_or_update(keyring, type, desc, payload, plen, perm, flags) \
	bp_key_create_or_update(keyring, desc, payload, plen)

extern key_ref_t bp_key_create_or_update(key_ref_t keyring,
					 const char *description,
					 const void *payload,
					 size_t plen);

#define keyring_alloc(desc, uid, gid, cred, perm, flags, restrict, dest) \
	bp_keyring_alloc();

extern struct key *bp_keyring_alloc(void);

static inline s32 key_serial(const struct key *key)
{
	return key ? key->serial : 0;
}

#endif /* CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION */
#endif /* __BP_KEY_H */
