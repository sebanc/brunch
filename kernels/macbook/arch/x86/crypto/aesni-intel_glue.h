/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Support for Intel AES-NI instructions. This file contains function
 * prototypes to be referenced for other AES implementations
 */

int aesni_set_key(struct crypto_aes_ctx *ctx, const u8 *in_key, unsigned int key_len);

int aesni_enc(const void *ctx, u8 *out, const u8 *in);
int aesni_dec(const void *ctx, u8 *out, const u8 *in);

int aesni_xts_encrypt(const struct crypto_aes_ctx *ctx, u8 *out, const u8 *in,
		      unsigned int len, u8 *iv);
int aesni_xts_decrypt(const struct crypto_aes_ctx *ctx, u8 *out, const u8 *in,
		      unsigned int len, u8 *iv);

