#include "pcm.h"
#include "audio.h"

static u64 aaudio_get_alsa_fmtbit(struct aaudio_apple_description *desc)
{
    if (desc->format_flags & AAUDIO_FORMAT_FLAG_FLOAT) {
        if (desc->bits_per_channel == 32) {
            if (desc->format_flags & AAUDIO_FORMAT_FLAG_BIG_ENDIAN)
                return SNDRV_PCM_FMTBIT_FLOAT_BE;
            else
                return SNDRV_PCM_FMTBIT_FLOAT_LE;
        } else if (desc->bits_per_channel == 64) {
            if (desc->format_flags & AAUDIO_FORMAT_FLAG_BIG_ENDIAN)
                return SNDRV_PCM_FMTBIT_FLOAT64_BE;
            else
                return SNDRV_PCM_FMTBIT_FLOAT64_LE;
        } else {
            pr_err("aaudio: unsupported bits per channel for float format: %u\n", desc->bits_per_channel);
            return 0;
        }
    }
#define DEFINE_BPC_OPTION(val, b) \
    case val: \
        if (desc->format_flags & AAUDIO_FORMAT_FLAG_BIG_ENDIAN) { \
            if (desc->format_flags & AAUDIO_FORMAT_FLAG_SIGNED) \
                return SNDRV_PCM_FMTBIT_S ## b ## BE; \
            else \
                return SNDRV_PCM_FMTBIT_U ## b ## BE; \
        } else { \
            if (desc->format_flags & AAUDIO_FORMAT_FLAG_SIGNED) \
                return SNDRV_PCM_FMTBIT_S ## b ## LE; \
            else \
                return SNDRV_PCM_FMTBIT_U ## b ## LE; \
        }
    if (desc->format_flags & AAUDIO_FORMAT_FLAG_PACKED) {
        switch (desc->bits_per_channel) {
            case 8:
            case 16:
            case 32:
                break;
            DEFINE_BPC_OPTION(24, 24_3)
            default:
                pr_err("aaudio: unsupported bits per channel for packed format: %u\n", desc->bits_per_channel);
                return 0;
        }
    }
    if (desc->format_flags & AAUDIO_FORMAT_FLAG_ALIGNED_HIGH) {
        switch (desc->bits_per_channel) {
            DEFINE_BPC_OPTION(24, 32_)
            default:
                pr_err("aaudio: unsupported bits per channel for high-aligned format: %u\n", desc->bits_per_channel);
                return 0;
        }
    }
    switch (desc->bits_per_channel) {
        case 8:
            if (desc->format_flags & AAUDIO_FORMAT_FLAG_SIGNED)
                return SNDRV_PCM_FMTBIT_S8;
            else
                return SNDRV_PCM_FMTBIT_U8;
        DEFINE_BPC_OPTION(16, 16_)
        DEFINE_BPC_OPTION(24, 24_)
        DEFINE_BPC_OPTION(32, 32_)
        default:
            pr_err("aaudio: unsupported bits per channel: %u\n", desc->bits_per_channel);
            return 0;
    }
}
int aaudio_create_hw_info(struct aaudio_apple_description *desc, struct snd_pcm_hardware *alsa_hw,
        size_t buf_size)
{
    uint rate;
    alsa_hw->info = (SNDRV_PCM_INFO_MMAP |
                     SNDRV_PCM_INFO_BLOCK_TRANSFER |
                     SNDRV_PCM_INFO_MMAP_VALID |
                     SNDRV_PCM_INFO_DOUBLE);
    if (desc->format_flags & AAUDIO_FORMAT_FLAG_NON_MIXABLE)
        pr_warn("aaudio: unsupported hw flag: NON_MIXABLE\n");
    if (!(desc->format_flags & AAUDIO_FORMAT_FLAG_NON_INTERLEAVED))
        alsa_hw->info |= SNDRV_PCM_INFO_INTERLEAVED;
    alsa_hw->formats = aaudio_get_alsa_fmtbit(desc);
    if (!alsa_hw->formats)
        return -EINVAL;
    rate = (uint) aaudio_double_to_u64(desc->sample_rate_double);
    alsa_hw->rates = snd_pcm_rate_to_rate_bit(rate);
    alsa_hw->rate_min = rate;
    alsa_hw->rate_max = rate;
    alsa_hw->channels_min = desc->channels_per_frame;
    alsa_hw->channels_max = desc->channels_per_frame;
    alsa_hw->buffer_bytes_max = buf_size;
    alsa_hw->period_bytes_min = desc->bytes_per_packet;
    alsa_hw->period_bytes_max = desc->bytes_per_packet;
    alsa_hw->periods_min = (uint) (buf_size / desc->bytes_per_packet);
    alsa_hw->periods_max = (uint) (buf_size / desc->bytes_per_packet);
    pr_debug("aaudio_create_hw_info: format = %llu, rate = %u/%u. channels = %u, periods = %u, period size = %lu\n",
            alsa_hw->formats, alsa_hw->rate_min, alsa_hw->rates, alsa_hw->channels_min, alsa_hw->periods_min,
            alsa_hw->period_bytes_min);
    return 0;
}

static struct aaudio_stream *aaudio_pcm_stream(struct snd_pcm_substream *substream)
{
    struct aaudio_subdevice *sdev = snd_pcm_substream_chip(substream);
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        return &sdev->out_streams[substream->number];
    else
        return &sdev->in_streams[substream->number];
}

static int aaudio_pcm_open(struct snd_pcm_substream *substream)
{
    pr_info("aaudio_pcm_open\n");
    substream->runtime->hw = *aaudio_pcm_stream(substream)->alsa_hw_desc;

    return 0;
}

static int aaudio_pcm_close(struct snd_pcm_substream *substream)
{
    pr_info("aaudio_pcm_close\n");
    return 0;
}

static int aaudio_pcm_prepare(struct snd_pcm_substream *substream)
{
    return 0;
}

static int aaudio_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
    struct aaudio_stream *astream = aaudio_pcm_stream(substream);
    pr_info("aaudio_pcm_hw_params\n");

    if (!astream->buffer_cnt || !astream->buffers)
        return -EINVAL;

    substream->runtime->dma_area = astream->buffers[0].ptr;
    substream->runtime->dma_addr = astream->buffers[0].dma_addr;
    substream->runtime->dma_bytes = astream->buffers[0].size;
    return 0;
}

static int aaudio_pcm_hw_free(struct snd_pcm_substream *substream)
{
    pr_info("aaudio_pcm_hw_free\n");
    return 0;
}

static void aaudio_pcm_start(struct snd_pcm_substream *substream)
{
    struct aaudio_subdevice *sdev = snd_pcm_substream_chip(substream);
    struct aaudio_stream *stream = aaudio_pcm_stream(substream);
    void *buf;
    size_t s;
    ktime_t time_start, time_end;
    bool back_buffer;
    time_start = ktime_get();

    back_buffer = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK);

    if (back_buffer) {
        s = frames_to_bytes(substream->runtime, substream->runtime->control->appl_ptr);
        buf = kmalloc(s, GFP_KERNEL);
        memcpy_fromio(buf, substream->runtime->dma_area, s);
        time_end = ktime_get();
        pr_info("aaudio: Backed up the buffer in %lluns [%li]\n", ktime_to_ns(time_end - time_start),
                substream->runtime->control->appl_ptr);
    }

    stream->waiting_for_first_ts = true;
    stream->frame_min = stream->latency;

    aaudio_cmd_start_io(sdev->a, sdev->dev_id);
    if (back_buffer)
        memcpy_toio(substream->runtime->dma_area, buf, s);

    time_end = ktime_get();
    pr_info("aaudio: Started the audio device in %lluns\n", ktime_to_ns(time_end - time_start));
}

static int aaudio_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    struct aaudio_subdevice *sdev = snd_pcm_substream_chip(substream);
    struct aaudio_stream *stream = aaudio_pcm_stream(substream);
    pr_info("aaudio_pcm_trigger %x\n", cmd);

    /* We only supports triggers on the #0 buffer */
    if (substream->number != 0)
        return 0;
    switch (cmd) {
        case SNDRV_PCM_TRIGGER_START:
            aaudio_pcm_start(substream);
            stream->started = 1;
            break;
        case SNDRV_PCM_TRIGGER_STOP:
            aaudio_cmd_stop_io(sdev->a, sdev->dev_id);
            stream->started = 0;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static snd_pcm_uframes_t aaudio_pcm_pointer(struct snd_pcm_substream *substream)
{
    struct aaudio_stream *stream = aaudio_pcm_stream(substream);
    ktime_t time_from_start;
    snd_pcm_sframes_t frames;
    snd_pcm_sframes_t buffer_time_length;

    if (!stream->started || stream->waiting_for_first_ts) {
        pr_warn("aaudio_pcm_pointer while not started\n");
        return 0;
    }

    /* Approximate the pointer based on the last received timestamp */
    time_from_start = ktime_get_boottime() - stream->remote_timestamp;
    buffer_time_length = NSEC_PER_SEC * substream->runtime->buffer_size / substream->runtime->rate;
    frames = (ktime_to_ns(time_from_start) % buffer_time_length) * substream->runtime->buffer_size / buffer_time_length;
    if (ktime_to_ns(time_from_start) < buffer_time_length) {
        if (frames < stream->frame_min)
            frames = stream->frame_min;
        else
            stream->frame_min = 0;
    } else {
        if (ktime_to_ns(time_from_start) < 2 * buffer_time_length)
            stream->frame_min = frames;
        else
            stream->frame_min = 0; /* Heavy desync */
    }
    frames -= stream->latency;
    if (frames < 0)
        frames += ((-frames - 1) / substream->runtime->buffer_size + 1) * substream->runtime->buffer_size;
    return (snd_pcm_uframes_t) frames;
}

static struct snd_pcm_ops aaudio_pcm_ops = {
        .open =        aaudio_pcm_open,
        .close =       aaudio_pcm_close,
        .ioctl =       snd_pcm_lib_ioctl,
        .hw_params =   aaudio_pcm_hw_params,
        .hw_free =     aaudio_pcm_hw_free,
        .prepare =     aaudio_pcm_prepare,
        .trigger =     aaudio_pcm_trigger,
        .pointer =     aaudio_pcm_pointer,
        .mmap    =     snd_pcm_lib_mmap_iomem
};

int aaudio_create_pcm(struct aaudio_subdevice *sdev)
{
    struct snd_pcm *pcm;
    struct aaudio_alsa_pcm_id_mapping *id_mapping;
    int err;

    if (!sdev->is_pcm || (sdev->in_stream_cnt == 0 && sdev->out_stream_cnt == 0)) {
        return -EINVAL;
    }

    for (id_mapping = aaudio_alsa_id_mappings; id_mapping->name; id_mapping++) {
        if (!strcmp(sdev->uid, id_mapping->name)) {
            sdev->alsa_id = id_mapping->alsa_id;
            break;
        }
    }
    if (!id_mapping->name)
        sdev->alsa_id = sdev->a->next_alsa_id++;
    err = snd_pcm_new(sdev->a->card, sdev->uid, sdev->alsa_id,
            (int) sdev->out_stream_cnt, (int) sdev->in_stream_cnt, &pcm);
    if (err < 0)
        return err;
    pcm->private_data = sdev;
    pcm->nonatomic = 1;
    sdev->pcm = pcm;
    strcpy(pcm->name, sdev->uid);
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &aaudio_pcm_ops);
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &aaudio_pcm_ops);
    return 0;
}

static void aaudio_handle_stream_timestamp(struct snd_pcm_substream *substream, ktime_t timestamp)
{
    unsigned long flags;
    struct aaudio_stream *stream;

    stream = aaudio_pcm_stream(substream);
    snd_pcm_stream_lock_irqsave(substream, flags);
    stream->remote_timestamp = timestamp;
    if (stream->waiting_for_first_ts) {
        stream->waiting_for_first_ts = false;
        snd_pcm_stream_unlock_irqrestore(substream, flags);
        return;
    }
    snd_pcm_stream_unlock_irqrestore(substream, flags);
    snd_pcm_period_elapsed(substream);
}

void aaudio_handle_timestamp(struct aaudio_subdevice *sdev, ktime_t os_timestamp, u64 dev_timestamp)
{
    struct snd_pcm_substream *substream;

    substream = sdev->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
    if (substream)
        aaudio_handle_stream_timestamp(substream, dev_timestamp);
    substream = sdev->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
    if (substream)
        aaudio_handle_stream_timestamp(substream, os_timestamp);
}
