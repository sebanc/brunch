#ifndef _COMPAT_LINUX_FS_H
#define _COMPAT_LINUX_FS_H
#include_next <linux/fs.h>
#include <linux/version.h>


#if LINUX_VERSION_IS_LESS(4,5,0)
#define no_seek_end_llseek LINUX_BACKPORT(no_seek_end_llseek)
extern loff_t no_seek_end_llseek(struct file *, loff_t, int);
#endif /* < 4.5 */

#if LINUX_VERSION_IS_LESS(5,5,0)
#ifdef CONFIG_COMPAT
#define compat_ptr_ioctl LINUX_BACKPORT(compat_ptr_ioctl)
extern long compat_ptr_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg);
#else
#define compat_ptr_ioctl NULL
#endif
#endif /* < 5.5 */

#if LINUX_VERSION_IS_LESS(5,6,0)
#define proc_ops file_operations
#define proc_open open
#define proc_read read
#define proc_lseek llseek
#define proc_release release
#define proc_write write
#endif /* < 5.6 */

#endif	/* _COMPAT_LINUX_FS_H */
