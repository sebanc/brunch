#ifndef __BACKPORT_DEBUGFS_H_
#define __BACKPORT_DEBUGFS_H_
#include_next <linux/debugfs.h>
#include <linux/version.h>
#include <linux/device.h>
#include <generated/utsrelease.h>

#if LINUX_VERSION_IS_LESS(3,19,0)
#define debugfs_create_devm_seqfile LINUX_BACKPORT(debugfs_create_devm_seqfile)
#if defined(CONFIG_DEBUG_FS)
struct dentry *debugfs_create_devm_seqfile(struct device *dev, const char *name,
					   struct dentry *parent,
					   int (*read_fn)(struct seq_file *s,
							  void *data));
#else
static inline struct dentry *debugfs_create_devm_seqfile(struct device *dev,
							 const char *name,
							 struct dentry *parent,
					   int (*read_fn)(struct seq_file *s,
							  void *data))
{
	return ERR_PTR(-ENODEV);
}
#endif /* CONFIG_DEBUG_FS */
#endif /* LINUX_VERSION_IS_LESS(3,19,0) */

#if LINUX_VERSION_IS_LESS(4,4,0)
#define debugfs_create_bool LINUX_BACKPORT(debugfs_create_bool)
#ifdef CONFIG_DEBUG_FS
struct dentry *debugfs_create_bool(const char *name, umode_t mode,
				   struct dentry *parent, bool *value);
#else
static inline struct dentry *
debugfs_create_bool(const char *name, umode_t mode,
		    struct dentry *parent, bool *value)
{
	return ERR_PTR(-ENODEV);
}
#endif
#endif /* LINUX_VERSION_IS_LESS(4,4,0) */

#if LINUX_VERSION_IS_LESS(4,9,0) && \
    !LINUX_VERSION_IN_RANGE(4,8,4, 4,9,0) && \
    !LINUX_VERSION_IN_RANGE(4,7,10, 4,8,0)
static inline const struct file_operations *
debugfs_real_fops(const struct file *filp)
{
	/*
	 * Neither the pointer to the struct file_operations, nor its
	 * contents ever change -- srcu_dereference() is not needed here.
	 */
	return filp->f_path.dentry->d_fsdata;
}
#endif /* <4.9.0 but not >= 4.8.4, 4.7.10 */

#ifndef DEFINE_DEBUGFS_ATTRIBUTE
#define DEFINE_DEBUGFS_ATTRIBUTE(__fops, __get, __set, __fmt) \
	DEFINE_SIMPLE_ATTRIBUTE(__fops, __get, __set, __fmt)
#define debugfs_create_file_unsafe(name, mode, parent, data, fops) \
	debugfs_create_file(name, mode, parent, data, fops)
#endif

#if LINUX_VERSION_IS_LESS(4,4,0)
static inline struct dentry *
debugfs_create_ulong(const char *name, umode_t mode,
		     struct dentry *parent, unsigned long *value)
{
	if (sizeof(unsigned long) == sizeof(u64))
		return debugfs_create_u64(name, mode, parent, (u64 *)value);
	if (sizeof(unsigned long) == sizeof(u32))
		return debugfs_create_u32(name, mode, parent, (u32 *)value);
	WARN_ON(1);
	return ERR_PTR(-EINVAL);
}
#endif

#if LINUX_VERSION_IS_LESS(5,5,0)
static inline void debugfs_create_xul(const char *name, umode_t mode,
				      struct dentry *parent,
				      unsigned long *value)
{
	if (sizeof(*value) == sizeof(u32))
		debugfs_create_x32(name, mode, parent, (u32 *)value);
	else
		debugfs_create_x64(name, mode, parent, (u64 *)value);
}
#endif

#endif /* __BACKPORT_DEBUGFS_H_ */
