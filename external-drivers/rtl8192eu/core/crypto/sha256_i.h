/*
 * SHA-256 internal definitions
 * Copyright (c) 2003-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef SHA256_I_H
#define SHA256_I_H

#define SHA256_BLOCK_SIZE 64

struct rtl_sha256_state {
	u64 length;
	u32 state[8], curlen;
	u8 buf[SHA256_BLOCK_SIZE];
};

void rtl_sha256_init(struct rtl_sha256_state *md);
int rtl_sha256_process(struct rtl_sha256_state *md, const unsigned char *in,
		   unsigned long inlen);
int rtl_sha256_done(struct rtl_sha256_state *md, unsigned char *out);

#endif /* SHA256_I_H */
