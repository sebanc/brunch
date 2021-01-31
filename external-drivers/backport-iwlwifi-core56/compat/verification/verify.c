#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/verification.h>
#include <linux/export.h>
#include <linux/err.h>
#include <crypto/pkcs7.h>

int verify_pkcs7_signature(const void *data, size_t len,
			   const void *raw_pkcs7, size_t pkcs7_len,
			   struct key *trusted_keys,
			   enum key_being_used_for usage,
			   int (*view_content)(void *ctx,
					       const void *data, size_t len,
					       size_t asn1hdrlen),
			   void *ctx)
{
	struct pkcs7_message *pkcs7;
	int ret;

	pkcs7 = pkcs7_parse_message(raw_pkcs7, pkcs7_len);
	if (IS_ERR(pkcs7))
		return PTR_ERR(pkcs7);

	/* The data should be detached - so we need to supply it. */
	if (data && pkcs7_supply_detached_data(pkcs7, data, len) < 0) {
		pr_err("PKCS#7 signature with non-detached data\n");
		ret = -EBADMSG;
		goto error;
	}

	ret = pkcs7_verify(pkcs7, usage);
	if (ret < 0)
		goto error;

	if (WARN_ON(!trusted_keys)) {
		ret = -EINVAL;
		goto error;
	}

	ret = pkcs7_validate_trust(pkcs7, trusted_keys);
	if (ret < 0) {
		if (ret == -ENOKEY)
			pr_err("PKCS#7 signature not signed with a trusted key\n");
		goto error;
	}

	if (view_content) {
		size_t asn1hdrlen;

		ret = pkcs7_get_content_data(pkcs7, &data, &len, &asn1hdrlen);
		if (ret < 0) {
			if (ret == -ENODATA)
				pr_devel("PKCS#7 message does not contain data\n");
			goto error;
		}

		ret = view_content(ctx, data, len, asn1hdrlen);
	}

error:
	pkcs7_free_message(pkcs7);
	pr_devel("<==%s() = %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(verify_pkcs7_signature);
