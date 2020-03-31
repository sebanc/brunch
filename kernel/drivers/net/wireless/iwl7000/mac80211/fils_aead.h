/*
 * FILS AEAD for (Re)Association Request/Response frames
 * Copyright 2016, Qualcomm Atheros, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FILS_AEAD_H
#define FILS_AEAD_H

#if LINUX_VERSION_IS_GEQ(4,3,0)
int fils_encrypt_assoc_req(struct sk_buff *skb,
			   struct ieee80211_mgd_assoc_data *assoc_data);
int fils_decrypt_assoc_resp(struct ieee80211_sub_if_data *sdata,
			    u8 *frame, size_t *frame_len,
			    struct ieee80211_mgd_assoc_data *assoc_data);
#else
static inline
int fils_encrypt_assoc_req(struct sk_buff *skb,
			   struct ieee80211_mgd_assoc_data *assoc_data)
{
	return -EOPNOTSUPP;
}

static inline
int fils_decrypt_assoc_resp(struct ieee80211_sub_if_data *sdata,
			    u8 *frame, size_t *frame_len,
			    struct ieee80211_mgd_assoc_data *assoc_data)
{
	return -EOPNOTSUPP;
}
#endif

#endif /* FILS_AEAD_H */
