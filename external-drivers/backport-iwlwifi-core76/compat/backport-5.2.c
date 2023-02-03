// SPDX-License-Identifier: GPL-2.0
/*
 * NETLINK      Netlink attributes
 *
 * 		Authors:	Thomas Graf <tgraf@suug.ch>
 * 				Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/types.h>
#include <net/netlink.h>

/* For these data types, attribute length should be exactly the given
 * size. However, to maintain compatibility with broken commands, if the
 * attribute length does not match the expected size a warning is emitted
 * to the user that the command is sending invalid data and needs to be fixed.
 */
static const u8 nla_attr_len[NLA_TYPE_MAX+1] = {
	[NLA_U8]	= sizeof(u8),
	[NLA_U16]	= sizeof(u16),
	[NLA_U32]	= sizeof(u32),
	[NLA_U64]	= sizeof(u64),
	[NLA_S8]	= sizeof(s8),
	[NLA_S16]	= sizeof(s16),
	[NLA_S32]	= sizeof(s32),
	[NLA_S64]	= sizeof(s64),
};

static const u8 nla_attr_minlen[NLA_TYPE_MAX+1] = {
	[NLA_U8]	= sizeof(u8),
	[NLA_U16]	= sizeof(u16),
	[NLA_U32]	= sizeof(u32),
	[NLA_U64]	= sizeof(u64),
	[NLA_MSECS]	= sizeof(u64),
	[NLA_NESTED]	= NLA_HDRLEN,
	[NLA_S8]	= sizeof(s8),
	[NLA_S16]	= sizeof(s16),
	[NLA_S32]	= sizeof(s32),
	[NLA_S64]	= sizeof(s64),
};

static int validate_nla_bitfield32(const struct nlattr *nla,
				   const u32 *valid_flags_mask)
{
	const struct nla_bitfield32 *bf = nla_data(nla);

	if (!valid_flags_mask)
		return -EINVAL;

	/*disallow invalid bit selector */
	if (bf->selector & ~*valid_flags_mask)
		return -EINVAL;

	/*disallow invalid bit values */
	if (bf->value & ~*valid_flags_mask)
		return -EINVAL;

	/*disallow valid bit values that are not selected*/
	if (bf->value & ~bf->selector)
		return -EINVAL;

	return 0;
}

static int nla_validate_array(const struct nlattr *head, int len, int maxtype,
			      const struct nla_policy *policy,
			      struct netlink_ext_ack *extack,
			      unsigned int validate)
{
	const struct nlattr *entry;
	int rem;

	nla_for_each_attr(entry, head, len, rem) {
		int ret;

		if (nla_len(entry) == 0)
			continue;

		if (nla_len(entry) < NLA_HDRLEN) {
			NL_SET_ERR_MSG_ATTR(extack, entry,
					    "Array element too short");
			return -ERANGE;
		}

		ret = __nla_validate(nla_data(entry), nla_len(entry),
				     maxtype, policy, validate, extack);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int nla_validate_int_range(const struct nla_policy *pt,
				  const struct nlattr *nla,
				  struct netlink_ext_ack *extack)
{
	bool validate_min, validate_max;
	s64 value;

	validate_min = pt->validation_type == NLA_VALIDATE_RANGE ||
		       pt->validation_type == NLA_VALIDATE_MIN;
	validate_max = pt->validation_type == NLA_VALIDATE_RANGE ||
		       pt->validation_type == NLA_VALIDATE_MAX;

	switch (pt->type) {
	case NLA_U8:
		value = nla_get_u8(nla);
		break;
	case NLA_U16:
		value = nla_get_u16(nla);
		break;
	case NLA_U32:
		value = nla_get_u32(nla);
		break;
	case NLA_S8:
		value = nla_get_s8(nla);
		break;
	case NLA_S16:
		value = nla_get_s16(nla);
		break;
	case NLA_S32:
		value = nla_get_s32(nla);
		break;
	case NLA_S64:
		value = nla_get_s64(nla);
		break;
	case NLA_U64:
		/* treat this one specially, since it may not fit into s64 */
		if ((validate_min && nla_get_u64(nla) < pt->min) ||
		    (validate_max && nla_get_u64(nla) > pt->max)) {
			NL_SET_ERR_MSG_ATTR(extack, nla,
					    "integer out of range");
			return -ERANGE;
		}
		return 0;
	default:
		WARN_ON(1);
		return -EINVAL;
	}

	if ((validate_min && value < pt->min) ||
	    (validate_max && value > pt->max)) {
		NL_SET_ERR_MSG_ATTR(extack, nla,
				    "integer out of range");
		return -ERANGE;
	}

	return 0;
}

static int validate_nla(const struct nlattr *nla, int maxtype,
			const struct nla_policy *policy, unsigned int validate,
			struct netlink_ext_ack *extack)
{
	u16 strict_start_type = policy[0].strict_start_type;
	const struct nla_policy *pt;
	int minlen = 0, attrlen = nla_len(nla), type = nla_type(nla);
	int err = -ERANGE;

	if (strict_start_type && type >= strict_start_type)
		validate |= NL_VALIDATE_STRICT;

	if (type <= 0 || type > maxtype)
		return 0;

	pt = &policy[type];

	BUG_ON(pt->type > NLA_TYPE_MAX);

	if ((nla_attr_len[pt->type] && attrlen != nla_attr_len[pt->type]) ||
	    (pt->type == NLA_EXACT_LEN_WARN && attrlen != pt->len)) {
		pr_warn_ratelimited("netlink: '%s': attribute type %d has an invalid length.\n",
				    current->comm, type);
		if (validate & NL_VALIDATE_STRICT_ATTRS) {
			NL_SET_ERR_MSG_ATTR(extack, nla,
					    "invalid attribute length");
			return -EINVAL;
		}
	}

	if (validate & NL_VALIDATE_NESTED) {
		if ((pt->type == NLA_NESTED || pt->type == NLA_NESTED_ARRAY) &&
		    !(nla->nla_type & NLA_F_NESTED)) {
			NL_SET_ERR_MSG_ATTR(extack, nla,
					    "NLA_F_NESTED is missing");
			return -EINVAL;
		}
		if (pt->type != NLA_NESTED && pt->type != NLA_NESTED_ARRAY &&
		    pt->type != NLA_UNSPEC && (nla->nla_type & NLA_F_NESTED)) {
			NL_SET_ERR_MSG_ATTR(extack, nla,
					    "NLA_F_NESTED not expected");
			return -EINVAL;
		}
	}

	switch (pt->type) {
	case NLA_EXACT_LEN:
		if (attrlen != pt->len)
			goto out_err;
		break;

	case NLA_REJECT:
		if (extack && pt->validation_data) {
			NL_SET_BAD_ATTR(extack, nla);
			extack->_msg = pt->validation_data;
			return -EINVAL;
		}
		err = -EINVAL;
		goto out_err;

	case NLA_FLAG:
		if (attrlen > 0)
			goto out_err;
		break;

	case NLA_BITFIELD32:
		if (attrlen != sizeof(struct nla_bitfield32))
			goto out_err;

		err = validate_nla_bitfield32(nla, pt->validation_data);
		if (err)
			goto out_err;
		break;

	case NLA_NUL_STRING:
		if (pt->len)
			minlen = min_t(int, attrlen, pt->len + 1);
		else
			minlen = attrlen;

		if (!minlen || memchr(nla_data(nla), '\0', minlen) == NULL) {
			err = -EINVAL;
			goto out_err;
		}
		/* fall through */

	case NLA_STRING:
		if (attrlen < 1)
			goto out_err;

		if (pt->len) {
			char *buf = nla_data(nla);

			if (buf[attrlen - 1] == '\0')
				attrlen--;

			if (attrlen > pt->len)
				goto out_err;
		}
		break;

	case NLA_BINARY:
		if (pt->len && attrlen > pt->len)
			goto out_err;
		break;

	case NLA_NESTED:
		/* a nested attributes is allowed to be empty; if its not,
		 * it must have a size of at least NLA_HDRLEN.
		 */
		if (attrlen == 0)
			break;
		if (attrlen < NLA_HDRLEN)
			goto out_err;
		if (pt->validation_data) {
			err = __nla_validate(nla_data(nla), nla_len(nla), pt->len,
					     pt->validation_data, validate,
					     extack);
			if (err < 0) {
				/*
				 * return directly to preserve the inner
				 * error message/attribute pointer
				 */
				return err;
			}
		}
		break;
	case NLA_NESTED_ARRAY:
		/* a nested array attribute is allowed to be empty; if its not,
		 * it must have a size of at least NLA_HDRLEN.
		 */
		if (attrlen == 0)
			break;
		if (attrlen < NLA_HDRLEN)
			goto out_err;
		if (pt->validation_data) {
			int err;

			err = nla_validate_array(nla_data(nla), nla_len(nla),
						 pt->len, pt->validation_data,
						 extack, validate);
			if (err < 0) {
				/*
				 * return directly to preserve the inner
				 * error message/attribute pointer
				 */
				return err;
			}
		}
		break;

	case NLA_UNSPEC:
		if (validate & NL_VALIDATE_UNSPEC) {
			NL_SET_ERR_MSG_ATTR(extack, nla,
					    "Unsupported attribute");
			return -EINVAL;
		}
		/* fall through */
	case NLA_MIN_LEN:
		if (attrlen < pt->len)
			goto out_err;
		break;

	default:
		if (pt->len)
			minlen = pt->len;
		else
			minlen = nla_attr_minlen[pt->type];

		if (attrlen < minlen)
			goto out_err;
	}

	/* further validation */
	switch (pt->validation_type) {
	case NLA_VALIDATE_NONE:
		/* nothing to do */
		break;
	case NLA_VALIDATE_RANGE:
	case NLA_VALIDATE_MIN:
	case NLA_VALIDATE_MAX:
		err = nla_validate_int_range(pt, nla, extack);
		if (err)
			return err;
		break;
	case NLA_VALIDATE_FUNCTION:
		if (pt->validate) {
			err = pt->validate(nla, extack);
			if (err)
				return err;
		}
		break;
	}

	return 0;
out_err:
	NL_SET_ERR_MSG_ATTR(extack, nla, "Attribute failed policy validation");
	return err;
}

static int __nla_validate_parse(const struct nlattr *head, int len, int maxtype,
				const struct nla_policy *policy,
				unsigned int validate,
				struct netlink_ext_ack *extack,
				struct nlattr **tb)
{
	const struct nlattr *nla;
	int rem;

	if (tb)
		memset(tb, 0, sizeof(struct nlattr *) * (maxtype + 1));

	nla_for_each_attr(nla, head, len, rem) {
		u16 type = nla_type(nla);

		if (type == 0 || type > maxtype) {
			if (validate & NL_VALIDATE_MAXTYPE) {
				NL_SET_ERR_MSG_ATTR(extack, nla,
						    "Unknown attribute type");
				return -EINVAL;
			}
			continue;
		}
		if (policy) {
			int err = validate_nla(nla, maxtype, policy,
					       validate, extack);

			if (err < 0)
				return err;
		}

		if (tb)
			tb[type] = (struct nlattr *)nla;
	}

	if (unlikely(rem > 0)) {
		pr_warn_ratelimited("netlink: %d bytes leftover after parsing attributes in process `%s'.\n",
				    rem, current->comm);
		NL_SET_ERR_MSG(extack, "bytes leftover after parsing attributes");
		if (validate & NL_VALIDATE_TRAILING)
			return -EINVAL;
	}

	return 0;
}

/**
 * __nla_validate - Validate a stream of attributes
 * @head: head of attribute stream
 * @len: length of attribute stream
 * @maxtype: maximum attribute type to be expected
 * @policy: validation policy
 * @validate: validation strictness
 * @extack: extended ACK report struct
 *
 * Validates all attributes in the specified attribute stream against the
 * specified policy. Validation depends on the validate flags passed, see
 * &enum netlink_validation for more details on that.
 * See documenation of struct nla_policy for more details.
 *
 * Returns 0 on success or a negative error code.
 */
int __nla_validate(const struct nlattr *head, int len, int maxtype,
		   const struct nla_policy *policy, unsigned int validate,
		   struct netlink_ext_ack *extack)
{
	return __nla_validate_parse(head, len, maxtype, policy, validate,
				    extack, NULL);
}
EXPORT_SYMBOL(__nla_validate);

/**
 * nla_policy_len - Determin the max. length of a policy
 * @policy: policy to use
 * @n: number of policies
 *
 * Determines the max. length of the policy.  It is currently used
 * to allocated Netlink buffers roughly the size of the actual
 * message.
 *
 * Returns 0 on success or a negative error code.
 */
int
nla_policy_len(const struct nla_policy *p, int n)
{
	int i, len = 0;

	for (i = 0; i < n; i++, p++) {
		if (p->len)
			len += nla_total_size(p->len);
		else if (nla_attr_len[p->type])
			len += nla_total_size(nla_attr_len[p->type]);
		else if (nla_attr_minlen[p->type])
			len += nla_total_size(nla_attr_minlen[p->type]);
	}

	return len;
}
EXPORT_SYMBOL(nla_policy_len);

/**
 * __nla_parse - Parse a stream of attributes into a tb buffer
 * @tb: destination array with maxtype+1 elements
 * @maxtype: maximum attribute type to be expected
 * @head: head of attribute stream
 * @len: length of attribute stream
 * @policy: validation policy
 * @validate: validation strictness
 * @extack: extended ACK pointer
 *
 * Parses a stream of attributes and stores a pointer to each attribute in
 * the tb array accessible via the attribute type.
 * Validation is controlled by the @validate parameter.
 *
 * Returns 0 on success or a negative error code.
 */
int __nla_parse(struct nlattr **tb, int maxtype,
		const struct nlattr *head, int len,
		const struct nla_policy *policy, unsigned int validate,
		struct netlink_ext_ack *extack)
{
	return __nla_validate_parse(head, len, maxtype, policy, validate,
				    extack, tb);
}
EXPORT_SYMBOL(__nla_parse);
