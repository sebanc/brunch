/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_KEYLOCKER_H
#define _ASM_KEYLOCKER_H

#ifndef __ASSEMBLY__

#include <asm/processor.h>
#include <linux/bits.h>
#include <asm/fpu/types.h>

/**
 * struct iwkey - A temporary internal wrapping key storage.
 * @integrity_key:	A 128-bit key to check that key handles have not
 *			been tampered with.
 * @encryption_key:	A 256-bit encryption key used in
 *			wrapping/unwrapping a clear text key.
 *
 * This storage should be flushed immediately after loaded.
 */
struct iwkey {
	struct reg_128_bit integrity_key;
	struct reg_128_bit encryption_key[2];
};

#define KEYLOCKER_CPUID			0x019
#define KEYLOCKER_CPUID_EAX_SUPERVISOR	BIT(0)
#define KEYLOCKER_CPUID_EBX_AESKLE	BIT(0)
#define KEYLOCKER_CPUID_EBX_WIDE	BIT(2)
#define KEYLOCKER_CPUID_EBX_BACKUP	BIT(4)

#ifdef CONFIG_X86_KEYLOCKER
void setup_keylocker(struct cpuinfo_x86 *c);
void destroy_keylocker_data(void);
void restore_keylocker(void);
extern bool valid_keylocker(void);
#else
#define setup_keylocker(c) do { } while (0)
#define destroy_keylocker_data() do { } while (0)
#define restore_keylocker() do { } while (0)
static inline bool valid_keylocker(void) { return false; }
#endif

#endif /*__ASSEMBLY__ */
#endif /* _ASM_KEYLOCKER_H */
