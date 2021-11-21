#ifndef AAUDIO_DESCRIPTION_H
#define AAUDIO_DESCRIPTION_H

#include <linux/types.h>

struct aaudio_apple_description {
    u64 sample_rate_double;
    u32 format_id;
    u32 format_flags;
    u32 bytes_per_packet;
    u32 frames_per_packet;
    u32 bytes_per_frame;
    u32 channels_per_frame;
    u32 bits_per_channel;
    u32 reserved;
};

enum {
    AAUDIO_FORMAT_LPCM = 0x6c70636d  // 'lpcm'
};

enum {
    AAUDIO_FORMAT_FLAG_FLOAT = 1,
    AAUDIO_FORMAT_FLAG_BIG_ENDIAN = 2,
    AAUDIO_FORMAT_FLAG_SIGNED = 4,
    AAUDIO_FORMAT_FLAG_PACKED = 8,
    AAUDIO_FORMAT_FLAG_ALIGNED_HIGH = 16,
    AAUDIO_FORMAT_FLAG_NON_INTERLEAVED = 32,
    AAUDIO_FORMAT_FLAG_NON_MIXABLE = 64
};

static inline u64 aaudio_double_to_u64(u64 d)
{
    u8 sign = (u8) ((d >> 63) & 1);
    s32 exp = (s32) ((d >> 52) & 0x7ff) - 1023;
    u64 fr = d & ((1LL << 52) - 1);
    if (sign || exp < 0)
        return 0;
    return (u64) ((1LL << exp) + (fr >> (52 - exp)));
}

#endif //AAUDIO_DESCRIPTION_H
