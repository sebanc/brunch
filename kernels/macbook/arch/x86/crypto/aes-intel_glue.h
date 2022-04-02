/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Shared glue code between AES implementations, refactored from the AES-NI's.
 */

#ifndef _AES_INTEL_GLUE_H
#define _AES_INTEL_GLUE_H

#include <crypto/aes.h>

#define AES_ALIGN			16
#define AES_ALIGN_ATTR			__attribute__((__aligned__(AES_ALIGN)))
#define AES_BLOCK_MASK			(~(AES_BLOCK_SIZE - 1))
#define AES_ALIGN_EXTRA			((AES_ALIGN - 1) & ~(CRYPTO_MINALIGN - 1))
#define CRYPTO_AES_CTX_SIZE		(sizeof(struct crypto_aes_ctx) + AES_ALIGN_EXTRA)
#define XTS_AES_CTX_SIZE		(sizeof(struct aes_xts_ctx) + AES_ALIGN_EXTRA)

struct aes_xts_ctx {
	u8 raw_tweak_ctx[sizeof(struct crypto_aes_ctx)] AES_ALIGN_ATTR;
	u8 raw_crypt_ctx[sizeof(struct crypto_aes_ctx)] AES_ALIGN_ATTR;
};

static inline struct crypto_aes_ctx *aes_ctx(void *raw_ctx)
{
	unsigned long addr = (unsigned long)raw_ctx;
	unsigned long align = AES_ALIGN;

	if (align <= crypto_tfm_ctx_alignment())
		align = 1;

	return (struct crypto_aes_ctx *)ALIGN(addr, align);
}

int cbc_crypt_common(struct skcipher_request *req,
		     int (*fn)(struct crypto_aes_ctx *ctx, u8 *out, const u8 *in,
			       unsigned int len, u8 *iv));

int xts_setkey_common(struct crypto_skcipher *tfm, const u8 *key, unsigned int keylen,
		      int (*fn)(struct crypto_tfm *tfm, void *raw_ctx,
				const u8 *in_key, unsigned int key_len));

int xts_crypt_common(struct skcipher_request *req,
		     int (*crypt_fn)(const struct crypto_aes_ctx *ctx, u8 *out, const u8 *in,
				     unsigned int len, u8 *iv),
		     int (*crypt1_fn)(const void *ctx, u8 *out, const u8 *in));

#endif /* _AES_INTEL_GLUE_H */
