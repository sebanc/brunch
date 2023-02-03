#ifndef __BACKPORT_LINUX_TTY_H
#define __BACKPORT_LINUX_TTY_H
#include_next <linux/tty.h>

#ifndef N_NCI
#define N_NCI		25	/* NFC NCI UART */
#endif

#endif /* __BACKPORT_LINUX_TTY_H */
