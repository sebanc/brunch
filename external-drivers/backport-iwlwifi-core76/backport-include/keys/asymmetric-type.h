#ifndef __BP_ASYMMETRIC_TYPE_H
#define __BP_ASYMMETRIC_TYPE_H
#ifdef CPTCFG_BPAUTO_BUILD_SYSTEM_DATA_VERIFICATION

#include <linux/string.h>

struct asymmetric_key_id {
	unsigned short	len;
	unsigned char	data[];
};

struct asymmetric_key_ids {
	struct asymmetric_key_id *id[2];
};

static inline bool asymmetric_key_id_same(const struct asymmetric_key_id *kid1,
					  const struct asymmetric_key_id *kid2)
{
	if (!kid1 || !kid2)
		return false;
	if (kid1->len != kid2->len)
		return false;
	return memcmp(kid1->data, kid2->data, kid1->len) == 0;
}

extern struct asymmetric_key_id *
asymmetric_key_generate_id(const void *val_1, size_t len_1,
			   const void *val_2, size_t len_2);

extern struct key *find_asymmetric_key(struct key *keyring,
				       const struct asymmetric_key_id *id_0,
				       const struct asymmetric_key_id *id_1,
				       const struct asymmetric_key_id *id_2,
				       bool partial);
#endif
#endif /* __BP_ASYMMETRIC_TYPE_H */
