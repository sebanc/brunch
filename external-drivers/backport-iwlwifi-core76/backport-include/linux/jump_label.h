#ifndef __BACKPORT_LINUX_JUMP_LABEL_H
#define __BACKPORT_LINUX_JUMP_LABEL_H
#include_next <linux/jump_label.h>

#ifndef DECLARE_STATIC_KEY_FALSE
#define DECLARE_STATIC_KEY_FALSE(name)	\
	extern struct static_key_false name
#endif

#endif /* __BACKPORT_LINUX_JUMP_LABEL_H */
