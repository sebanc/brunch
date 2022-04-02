// SPDX-License-Identifier: GPL-2.0-only

/*
 * Setup Key Locker feature and support internal wrapping key
 * management.
 */

#include <linux/random.h>
#include <linux/poison.h>

#include <asm/fpu/api.h>
#include <asm/keylocker.h>
#include <asm/tlbflush.h>
#include <asm/msr.h>

static __initdata struct keylocker_setup_data {
	bool initialized;
	struct iwkey key;
} kl_setup;

/*
 * This flag is set with IWKey load. When the key restore fails, it is
 * reset. This restore state is exported to the crypto library, then AES-KL
 * will not be used there. So, the feature is soft-disabled with this flag.
 */
static bool valid_kl;

bool valid_keylocker(void)
{
	return valid_kl;
}
EXPORT_SYMBOL_GPL(valid_keylocker);

static void __init generate_keylocker_data(void)
{
	get_random_bytes(&kl_setup.key.integrity_key,  sizeof(kl_setup.key.integrity_key));
	get_random_bytes(&kl_setup.key.encryption_key, sizeof(kl_setup.key.encryption_key));
}

void __init destroy_keylocker_data(void)
{
	if (!cpu_feature_enabled(X86_FEATURE_KEYLOCKER) ||
	    cpu_feature_enabled(X86_FEATURE_HYPERVISOR))
	        return;

	memset(&kl_setup.key, KEY_DESTROY, sizeof(kl_setup.key));
	kl_setup.initialized = true;
	valid_kl = true;
}

static void __init load_keylocker(void)
{
	kernel_fpu_begin();
	load_xmm_iwkey(&kl_setup.key);
	kernel_fpu_end();
}

/**
 * copy_keylocker - Copy the internal wrapping key from the backup.
 *
 * Request hardware to copy the key in non-volatile storage to the CPU
 * state.
 *
 * Returns:	-EBUSY if the copy fails, 0 if successful.
 */
static int copy_keylocker(void)
{
	u64 status;

	wrmsrl(MSR_IA32_COPY_IWKEY_TO_LOCAL, 1);

	rdmsrl(MSR_IA32_IWKEY_COPY_STATUS, status);
	if (status & BIT(0))
		return 0;
	else
		return -EBUSY;
}

/**
 * setup_keylocker - Enable the feature.
 * @c:		A pointer to struct cpuinfo_x86
 */
void __ref setup_keylocker(struct cpuinfo_x86 *c)
{
	/* This feature is not compatible with a hypervisor. */
	if (!cpu_feature_enabled(X86_FEATURE_KEYLOCKER) ||
	    cpu_feature_enabled(X86_FEATURE_HYPERVISOR))
		goto out;

	cr4_set_bits(X86_CR4_KEYLOCKER);

	if (c == &boot_cpu_data) {
		u32 eax, ebx, ecx, edx;
		bool backup_available;

		cpuid_count(KEYLOCKER_CPUID, 0, &eax, &ebx, &ecx, &edx);
		/*
		 * Check the feature readiness via CPUID. Note that the
		 * CPUID AESKLE bit is conditionally set only when CR4.KL
		 * is set.
		 */
		if (!(ebx & KEYLOCKER_CPUID_EBX_AESKLE) ||
		    !(eax & KEYLOCKER_CPUID_EAX_SUPERVISOR)) {
			pr_debug("x86/keylocker: Not fully supported.\n");
			goto disable;
		}

		backup_available = !!(ebx & KEYLOCKER_CPUID_EBX_BACKUP);
		/*
		 * The internal wrapping key in CPU state is volatile in
		 * S3/4 states. So ensure the backup capability along with
		 * S-states.
		 */
		if (!backup_available && IS_ENABLED(CONFIG_SUSPEND)) {
			pr_debug("x86/keylocker: No key backup support with possible S3/4.\n");
			goto disable;
		}

		generate_keylocker_data();
		load_keylocker();

		/* Backup an internal wrapping key in non-volatile media. */
		if (backup_available)
			wrmsrl(MSR_IA32_BACKUP_IWKEY_TO_PLATFORM, 1);
	} else {
		int rc;

		/*
		 * Load the internal wrapping key directly when available
		 * in memory, which is only possible at boot-time.
		 *
		 * NB: When system wakes up, this path also recovers the
		 * internal wrapping key.
		 */
		if (!kl_setup.initialized) {
			load_keylocker();
		} else if (valid_kl) {
			rc = copy_keylocker();
			/*
			 * The boot CPU was successful but the key copy
			 * fails here. Then, the subsequent feature use
			 * will have inconsistent keys and failures. So,
			 * invalidate the feature via the flag.
			 */
			if (rc) {
				valid_kl = false;
				pr_err_once("x86/keylocker: Invalid copy status (rc: %d).\n", rc);
			}
		}
	}

	pr_info_once("x86/keylocker: Enabled.\n");
	return;

disable:
	setup_clear_cpu_cap(X86_FEATURE_KEYLOCKER);
	pr_info_once("x86/keylocker: Disabled.\n");
out:
	/* Make sure the feature disabled for kexec-reboot. */
	cr4_clear_bits(X86_CR4_KEYLOCKER);
}

/**
 * restore_keylocker - Restore the internal wrapping key.
 *
 * The boot CPU executes this while other CPUs restore it through the setup
 * function.
 */
void restore_keylocker(void)
{
	u64 backup_status;
	int rc;

	if (!cpu_feature_enabled(X86_FEATURE_KEYLOCKER) || !valid_kl)
		return;

	/*
	 * The IA32_IWKEYBACKUP_STATUS MSR contains a bitmap that indicates
	 * an invalid backup if bit 0 is set and a read (or write) error if
	 * bit 2 is set.
	 */
	rdmsrl(MSR_IA32_IWKEY_BACKUP_STATUS, backup_status);
	if (backup_status & BIT(0)) {
		rc = copy_keylocker();
		if (rc)
			pr_err("x86/keylocker: Invalid copy state (rc: %d).\n", rc);
		else
			return;
	} else {
		pr_err("x86/keylocker: The key backup access failed with %s.\n",
		       (backup_status & BIT(2)) ? "read error" : "invalid status");
	}

	/*
	 * Now the backup key is not available. Invalidate the feature via
	 * the flag to avoid any subsequent use. But keep the feature with
	 * zero IWKeys instead of disabling it. The current users will see
	 * key handle integrity failure but that's because of the internal
	 * key change.
	 */
	pr_err("x86/keylocker: Failed to restore internal wrapping key.\n");
	valid_kl = false;
}
