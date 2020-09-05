/*
 * Adapted from the kernel for simplicity in backports.
 *
 * Copyright (C) 2012 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#define pr_fmt(fmt) "PKEY: "fmt
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/asn1_decoder.h>
#include <crypto/public_key.h>
#include "rsapubkey.asn1.h"
#include "mbedtls/rsa.h"
#include "mbedtls/md.h"

void public_key_free(struct public_key *key)
{
	if (key) {
		kfree(key->key);
		kfree(key);
	}
}

int rsa_get_n(void *context, size_t hdrlen, unsigned char tag,
	      const void *value, size_t vlen)
{
	mbedtls_rsa_context *rsa = context;

	/* invalid key provided */
	if (!value || !vlen)
		return -EINVAL;

	return mbedtls_mpi_read_binary(&rsa->N, value, vlen) ? -EINVAL : 0;
}

int rsa_get_e(void *context, size_t hdrlen, unsigned char tag,
	      const void *value, size_t vlen)
{
	mbedtls_rsa_context *rsa = context;

	/* invalid key provided */
	if (!value || !vlen)
		return -EINVAL;

	return mbedtls_mpi_read_binary(&rsa->E, value, vlen) ? -EINVAL : 0;
}

int public_key_verify_signature(const struct public_key *pkey,
				const struct public_key_signature *sig)
{
	mbedtls_rsa_context rsa;
	mbedtls_md_type_t md_alg;
	const u8 *sigdata = sig->s;
	int s_size = sig->s_size;
	int ret;

	if (WARN_ON(!pkey))
		return -EINVAL;

	if (strcmp(sig->pkey_algo, "rsa"))
		return -ENOTSUPP;

	if (strcmp(sig->hash_algo, "sha1") == 0)
		md_alg = MBEDTLS_MD_SHA1;
	else if (strcmp(sig->hash_algo, "sha256") == 0)
		md_alg = MBEDTLS_MD_SHA256;
	else
		return -ENOTSUPP;

	mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);

	ret = asn1_ber_decoder(&rsapubkey_decoder, &rsa,
			       pkey->key, pkey->keylen);
	if (ret)
		goto free;

	rsa.len = (mbedtls_mpi_bitlen(&rsa.N) + 7) >> 3;

	/*
	 * In some cases (from X.509 certificates) we get here with a
	 * BIT_STRING ASN.1 object, in which the first byte indicates
	 * the number of unused bits in the bit string (in case the
	 * string isn't a multiple of 8 long).
	 * Assume here that it's always a multiple of 8, and just skip
	 * the additional byte.
	 */
	if (s_size == rsa.len + 1 && sigdata[0] == 0) {
		sigdata = sig->s + 1;
		s_size -= 1;
	}

	if (rsa.len != s_size) {
		ret = -EINVAL;
		goto free;
	}

	ret = mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC,
				       md_alg, 0, sig->digest, sigdata);

	if (ret)
		ret = -EKEYREJECTED;
	else
		ret = 0;

 free:
	mbedtls_rsa_free(&rsa);

	return ret;
}

void public_key_signature_free(struct public_key_signature *sig)
{
	int i;

	if (sig) {
		for (i = 0; i < ARRAY_SIZE(sig->auth_ids); i++)
			kfree(sig->auth_ids[i]);
		kfree(sig->s);
		kfree(sig->digest);
		kfree(sig);
	}
}
