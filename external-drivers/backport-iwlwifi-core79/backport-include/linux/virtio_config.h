#ifndef _COMPAT_LINUX_VIRTIO_CONFIG_H
#define _COMPAT_LINUX_VIRTIO_CONFIG_H
#include_next <linux/virtio_config.h>

#include <linux/version.h>

#if LINUX_VERSION_IS_LESS(4,11,0)
struct irq_affinity;
#endif

#if LINUX_VERSION_IS_LESS(4,12,0)
static inline
int virtio_find_vqs(struct virtio_device *vdev, unsigned nvqs,
			struct virtqueue *vqs[], vq_callback_t *callbacks[],
#if LINUX_VERSION_IS_LESS(4,10,0)
			const char * const names[],
#else
			const char *names[],
#endif
			struct irq_affinity *desc)
{
#if LINUX_VERSION_IS_LESS(4,11,0)
	return vdev->config->find_vqs(vdev, nvqs, vqs, callbacks, names);
#else
	return vdev->config->find_vqs(vdev, nvqs, vqs, callbacks, names, desc);
#endif
}
#endif /* < 4.12 */


#endif	/* _COMPAT_LINUX_VIRTIO_CONFIG_H */
