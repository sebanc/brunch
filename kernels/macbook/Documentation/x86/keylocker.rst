.. SPDX-License-Identifier: GPL-2.0

==============
x86 Key Locker
==============

Introduction
============

Key Locker is a CPU feature feature to reduce key exfiltration
opportunities while maintaining a programming interface similar to AES-NI.
It converts the AES key into an encoded form, called the 'key handle'. The
key handle is a wrapped version of the clear-text key where the wrapping
key has limited exposure. Once converted, all subsequent data encryption
using new AES instructions (AES-KL) uses this key handle, reducing the
exposure of private key material in memory.

Internal Wrapping Key (IWKey)
=============================

The CPU-internal wrapping key is an entity in a software-invisible CPU
state. On every system boot, a new key is loaded. So the key handle that
was encoded by the old wrapping key is no longer usable on system shutdown
or reboot.

And the key may be lost on the following exceptional situation upon wakeup:

IWKey Restore Failure
---------------------

The CPU state is volatile with the ACPI S3/4 sleep states. When the system
supports those states, the key has to be backed up so that it is restored
on wake up. The kernel saves the key in non-volatile media.

The event of an IWKey restore failure upon resume from suspend, all
established key handles become invalid. In flight dm-crypt operations
receive error results from pending operations. In the likely scenario that
dm-crypt is hosting the root filesystem the recovery is identical to if a
storage controller failed to resume from suspend, reboot. If the volume
impacted by an IWKey restore failure is a data-volume then it is possible
that I/O errors on that volume do not bring down the rest of the system.
However, a reboot is still required because the kernel will have
soft-disabled Key Locker. Upon the failure, the crypto library code will
return -ENODEV on every AES-KL function call. The Key Locker implementation
only loads a new IWKey at initial boot, not any time after like resume from
suspend.

Use Case and Non-use Cases
==========================

Bare metal disk encryption is the only intended use case.

Userspace usage is not supported because there is no ABI provided to
communicate and coordinate wrapping-key restore failure to userspace. For
now, key restore failures are only coordinated with kernel users. But the
kernel can not prevent userspace from using the feature's AES instructions
('AES-KL') when the feature has been enabled. So, the lack of userspace
support is only documented, not actively enforced.

Key Locker is not expected to be advertised to guest VMs and the kernel
implementation ignores it even if the VMM enumerates the capability. The
expectation is that a guest VM wants private IWKey state, but the
architecture does not provide that. An emulation of that capability, by
caching per VM IWKeys in memory, defeats the purpose of Key Locker. The
backup / restore facility is also not performant enough to be suitable for
guest VM context switches.

AES Instruction Set
===================

The feature accompanies a new AES instruction set. This instruction set is
analogous to AES-NI. A set of AES-NI instructions can be mapped to an
AES-KL instruction. For example, AESENC128KL is responsible for ten rounds
of transformation, which is equivalent to nine times AESENC and one
AESENCLAST in AES-NI.

But they have some notable differences:

* AES-KL provides a secure data transformation using an encrypted key.

* If an invalid key handle is provided, e.g. a corrupted one or a handle
  restriction failure, the instruction fails with setting RFLAGS.ZF. The
  crypto library implementation includes the flag check to return an error
  code. Note that the flag is also set when the internal wrapping key is
  changed because of missing backup.

* AES-KL implements support for 128-bit and 256-bit keys, but there is no
  AES-KL instruction to process an 192-bit key. But there is no AES-KL
  instruction to process a 192-bit key. The AES-KL cipher implementation
  logs a warning message with a 192-bit key and then falls back to AES-NI.
  So, this 192-bit key-size limitation is only documented, not enforced. It
  means the key will remain in clear-text in memory. This is to meet Linux
  crypto-cipher expectation that each implementation must support all the
  AES-compliant key sizes.

* Some AES-KL hardware implementation may have noticeable performance
  overhead when compared with AES-NI instructions.

