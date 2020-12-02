#ifndef __BACKPORT_CRYPTO_AEAD_H
#define __BACKPORT_CRYPTO_AEAD_H
#include_next <crypto/aead.h>
#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,2,0)
#define aead_request_set_ad LINUX_BACKPORT(aead_request_set_ad)
static inline void aead_request_set_ad(struct aead_request *req,
				       unsigned int assoclen)
{
	req->assoclen = assoclen;
}

#define crypto_aead_reqsize LINUX_BACKPORT(crypto_aead_reqsize)
unsigned int crypto_aead_reqsize(struct crypto_aead *tfm);

struct aead_request *crypto_backport_convert(struct aead_request *req);

static inline int backport_crypto_aead_encrypt(struct aead_request *req)
{
	return crypto_aead_encrypt(crypto_backport_convert(req));
}
#define crypto_aead_encrypt LINUX_BACKPORT(crypto_aead_encrypt)

static inline int backport_crypto_aead_decrypt(struct aead_request *req)
{
	return crypto_aead_decrypt(crypto_backport_convert(req));
}
#define crypto_aead_decrypt LINUX_BACKPORT(crypto_aead_decrypt)

#endif /* LINUX_VERSION_IS_LESS(4,2,0) */

#endif /* __BACKPORT_CRYPTO_AEAD_H */
