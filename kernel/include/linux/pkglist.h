#ifndef _PKGLIST_H_
#define _PKGLIST_H_

#include <linux/dcache.h>
#include <linux/uidgid.h>

#define QSTR_LITERAL(string) QSTR_INIT(string, sizeof(string)-1)

static inline bool str_case_eq(const char *s1, const char *s2)
{
	return !strcasecmp(s1, s2);
}

static inline bool str_n_case_eq(const char *s1, const char *s2, size_t len)
{
	return !strncasecmp(s1, s2, len);
}

static inline bool qstr_case_eq(const struct qstr *q1, const struct qstr *q2)
{
	return q1->len == q2->len && str_case_eq(q1->name, q2->name);
}

#define BY_NAME		BIT(0)
#define BY_USERID	BIT(1)

struct pkg_list {
	struct list_head list;
	void (*update)(int flags, const struct qstr *name, uint32_t userid);
};

kuid_t pkglist_get_appid(const char *key);
kgid_t pkglist_get_ext_gid(const char *key);
bool pkglist_user_is_excluded(const char *key, uint32_t user);
kuid_t pkglist_get_allowed_appid(const char *key, uint32_t user);
void pkglist_register_update_listener(struct pkg_list *pkg);
void pkglist_unregister_update_listener(struct pkg_list *pkg);
#endif
