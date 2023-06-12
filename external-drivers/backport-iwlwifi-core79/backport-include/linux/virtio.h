#ifndef __BACKPORT_LINUX_VIRTIO_H
#define __BACKPORT_LINUX_VIRTIO_H
#include_next <linux/virtio.h>

#if LINUX_VERSION_IS_LESS(5,17,0)
#include <linux/virtio_config.h>
static inline void virtio_reset_device(struct virtio_device *dev)
{
	dev->config->reset(dev);
}
#endif

#endif /* __BACKPORT_LINUX_VIRTIO_H */
