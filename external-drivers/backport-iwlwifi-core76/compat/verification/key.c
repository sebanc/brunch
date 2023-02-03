#include <linux/export.h>
#include <linux/slab.h>
#include <linux/key.h>
#include <linux/err.h>
#include <keys/asymmetric-type.h>
#include "x509_parser.h"

static void keyring_clear(struct key *keyring)
{
	struct key *key, *tmp;

	if (!keyring->keyring)
		return;

	list_for_each_entry_safe(key, tmp, &keyring->list, list) {
		WARN_ON(refcount_read(&key->refcount) > 1);
		key_put(key);
	}
}

void key_put(struct key *key)
{
	if (refcount_dec_and_test(&key->refcount)) {
		keyring_clear(key);
		list_del(&key->list);
		kfree(key->description);
		public_key_free(key->public_key);
		public_key_signature_free(key->sig);
		kfree(key->kids.id[0]);
		kfree(key->kids.id[1]);
		kfree(key);
	}
}
EXPORT_SYMBOL_GPL(key_put);

static struct key *key_alloc(void)
{
	struct key *key = kzalloc(sizeof(*key), GFP_KERNEL);

	if (!key)
		return NULL;
	refcount_set(&key->refcount, 1);
	INIT_LIST_HEAD(&key->list);

	return key;
}

struct key *bp_keyring_alloc(void)
{
	struct key *key = key_alloc();

	if (!key)
		return NULL;

	key->keyring = true;

	return key;
}
EXPORT_SYMBOL_GPL(bp_keyring_alloc);

key_ref_t bp_key_create_or_update(key_ref_t keyring,
				  const char *description,
				  const void *payload,
				  size_t plen)
{
	struct key *key = key_alloc();
	struct x509_certificate *cert;
	const char *q;
	size_t srlen, sulen;
	char *desc = NULL, *p;
	int err;

	if (!key)
		return NULL;

	cert = x509_cert_parse(payload, plen);
	if (IS_ERR(cert)) {
		err = PTR_ERR(cert);
		goto free;
	}

	if (cert->unsupported_sig) {
		public_key_signature_free(cert->sig);
		cert->sig = NULL;
	}

	sulen = strlen(cert->subject);
	if (cert->raw_skid) {
		srlen = cert->raw_skid_size;
		q = cert->raw_skid;
	} else {
		srlen = cert->raw_serial_size;
		q = cert->raw_serial;
	}

	err = -ENOMEM;
	desc = kmalloc(sulen + 2 + srlen * 2 + 1, GFP_KERNEL);
	if (!desc)
		goto free;
	p = memcpy(desc, cert->subject, sulen);
	p += sulen;
	*p++ = ':';
	*p++ = ' ';
	p = bin2hex(p, q, srlen);
	*p = 0;
	key->description = desc;

	key->kids.id[0] = cert->id;
	key->kids.id[1] = cert->skid;
	key->public_key = cert->pub;
	key->sig = cert->sig;

	cert->id = NULL;
	cert->skid = NULL;
	cert->pub = NULL;
	cert->sig = NULL;
	x509_free_certificate(cert);

	refcount_inc(&key->refcount);
	list_add_tail(&key->list, &key_ref_to_ptr(keyring)->list);

	return make_key_ref(key, 0);
free:
	kfree(key);
	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(bp_key_create_or_update);

struct key *find_asymmetric_key(struct key *keyring,
				const struct asymmetric_key_id *id_0,
				const struct asymmetric_key_id *id_1,
				const struct asymmetric_key_id *id_2,
				bool partial)
{
	struct key *key;

	if (WARN_ON(partial))
		return ERR_PTR(-ENOENT);
	if (WARN_ON(!keyring))
		return ERR_PTR(-EINVAL);

	list_for_each_entry(key, &keyring->list, list) {
		const struct asymmetric_key_ids *kids = &key->kids;

		if (id_0 && (!kids->id[0] ||
			     !asymmetric_key_id_same(id_0, kids->id[0])))
			continue;
		if (id_1 && (!kids->id[1] ||
			     !asymmetric_key_id_same(id_0, kids->id[1])))
			continue;

		refcount_inc(&key->refcount);
		return key;
	}

	return ERR_PTR(-ENOKEY);
}

struct asymmetric_key_id *
asymmetric_key_generate_id(const void *val_1, size_t len_1,
			   const void *val_2, size_t len_2)
{
	struct asymmetric_key_id *kid;

	kid = kmalloc(sizeof(struct asymmetric_key_id) + len_1 + len_2,
		      GFP_KERNEL);
	if (!kid)
		return ERR_PTR(-ENOMEM);
	kid->len = len_1 + len_2;
	memcpy(kid->data, val_1, len_1);
	memcpy(kid->data + len_1, val_2, len_2);
	return kid;
}
