#ifndef AAUDIO_H
#define AAUDIO_H

#include <linux/types.h>
#include <sound/pcm.h>
#include "../apple_bce.h"
#include "protocol_bce.h"
#include "description.h"

#define AAUDIO_SIG 0x19870423

#define AAUDIO_DEVICE_MAX_UID_LEN 128
#define AAUDIO_DEIVCE_MAX_INPUT_STREAMS 1
#define AAUDIO_DEIVCE_MAX_OUTPUT_STREAMS 1
#define AAUDIO_DEIVCE_MAX_BUFFER_COUNT 1

#define AAUDIO_BUFFER_ID_NONE 0xffu

struct snd_card;
struct snd_pcm;
struct snd_pcm_hardware;
struct snd_jack;

struct __attribute__((packed)) __attribute__((aligned(4))) aaudio_buffer_struct_buffer {
    size_t address;
    size_t size;
    size_t pad[4];
};
struct aaudio_buffer_struct_stream {
    u8 num_buffers;
    struct aaudio_buffer_struct_buffer buffers[100];
    char filler[32];
};
struct aaudio_buffer_struct_device {
    char name[128];
    u8 num_input_streams;
    u8 num_output_streams;
    struct aaudio_buffer_struct_stream input_streams[5];
    struct aaudio_buffer_struct_stream output_streams[5];
    char filler[128];
};
struct aaudio_buffer_struct {
    u32 version;
    u32 signature;
    u32 flags;
    u8 num_devices;
    struct aaudio_buffer_struct_device devices[20];
};

struct aaudio_device;
struct aaudio_dma_buf {
    dma_addr_t dma_addr;
    void *ptr;
    size_t size;
};
struct aaudio_stream {
    aaudio_object_id_t id;
    size_t buffer_cnt;
    struct aaudio_dma_buf *buffers;

    struct aaudio_apple_description desc;
    struct snd_pcm_hardware *alsa_hw_desc;
    u32 latency;

    bool waiting_for_first_ts;

    ktime_t remote_timestamp;
    snd_pcm_sframes_t frame_min;
    int started;
};
struct aaudio_subdevice {
    struct aaudio_device *a;
    struct list_head list;
    aaudio_device_id_t dev_id;
    u32 in_latency, out_latency;
    u8 buf_id;
    int alsa_id;
    char uid[AAUDIO_DEVICE_MAX_UID_LEN + 1];
    size_t in_stream_cnt;
    struct aaudio_stream in_streams[AAUDIO_DEIVCE_MAX_INPUT_STREAMS];
    size_t out_stream_cnt;
    struct aaudio_stream out_streams[AAUDIO_DEIVCE_MAX_OUTPUT_STREAMS];
    bool is_pcm;
    struct snd_pcm *pcm;
    struct snd_jack *jack;
};
struct aaudio_alsa_pcm_id_mapping {
    const char *name;
    int alsa_id;
};

struct aaudio_device {
    struct pci_dev *pci;
    dev_t devt;
    struct device *dev;
    void __iomem *reg_mem_bs;
    dma_addr_t reg_mem_bs_dma;
    void __iomem *reg_mem_cfg;

    u32 __iomem *reg_mem_gpr;

    struct aaudio_buffer_struct *bs;

    struct apple_bce_device *bce;
    struct aaudio_bce bcem;

    struct snd_card *card;

    struct list_head subdevice_list;
    int next_alsa_id;

    struct completion remote_alive;
};

void aaudio_handle_notification(struct aaudio_device *a, struct aaudio_msg *msg);
void aaudio_handle_command(struct aaudio_device *a, struct aaudio_msg *msg);

int aaudio_module_init(void);
void aaudio_module_exit(void);

extern struct aaudio_alsa_pcm_id_mapping aaudio_alsa_id_mappings[];

#endif //AAUDIO_H
