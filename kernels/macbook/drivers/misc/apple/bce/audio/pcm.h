#ifndef AAUDIO_PCM_H
#define AAUDIO_PCM_H

#include <linux/types.h>
#include <linux/ktime.h>

struct aaudio_subdevice;
struct aaudio_apple_description;
struct snd_pcm_hardware;

int aaudio_create_hw_info(struct aaudio_apple_description *desc, struct snd_pcm_hardware *alsa_hw, size_t buf_size);
int aaudio_create_pcm(struct aaudio_subdevice *sdev);

void aaudio_handle_timestamp(struct aaudio_subdevice *sdev, ktime_t os_timestamp, u64 dev_timestamp);

#endif //AAUDIO_PCM_H
