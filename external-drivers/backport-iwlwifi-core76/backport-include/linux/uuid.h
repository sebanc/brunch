#ifndef __BACKPORT_LINUX_UUID_H_
#define __BACKPORT_LINUX_UUID_H_
#include <linux/version.h>
#include_next <linux/uuid.h>

#ifndef UUID_STRING_LEN
/*
 * The length of a UUID string ("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee")
 * not including trailing NUL.
 */
#define	UUID_STRING_LEN		36
#endif

#if LINUX_VERSION_IS_LESS(4,13,0)
#define guid_t uuid_le
#define uuid_t uuid_be
#define GUID_INIT UUID_LE

static inline void guid_gen(guid_t *u)
{
	return uuid_le_gen(u);
}
static inline void uuid_gen(uuid_t *u)
{
	return uuid_be_gen(u);
}

static inline void guid_copy(guid_t *dst, const guid_t *src)
{
	memcpy(dst, src, sizeof(guid_t));
}
#endif

#endif /* __BACKPORT_LINUX_UUID_H_ */
