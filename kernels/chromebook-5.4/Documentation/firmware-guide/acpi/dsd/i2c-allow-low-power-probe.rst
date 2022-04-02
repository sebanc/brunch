.. SPDX-License-Identifier: GPL-2.0

======================================
Probing I²C devices in low power state
======================================

Introduction
============

In some cases it may be preferred to leave certain devices powered off for the
entire system bootup if powering on these devices has adverse side effects,
beyond just powering on the said device. Linux recognizes the _DSD property [1]
"i2c-allow-low-power-probe" that can be used for this purpose.

How it works
============

The boolean device property "i2c-allow-low-power-probe" may be used to tell
Linux that the I²C framework should instruct the kernel ACPI framework to leave
the device in the low power state. If the driver indicates its support for this
by setting the I2C_DRV_FL_ALLOW_LOW_POWER_PROBE flag in struct i2c_driver.flags
field and the "i2c-allow-low-power-probe" property is present, the device will
not be powered on for probe.

The downside is that as the device is not powered on, even if there's a problem
with the device, the driver likely probes just fine but the first user will
find out the device doesn't work, instead of a failure at probe time. This
feature should thus be used sparingly.

Example
=======

An ASL example describing an ACPI device using this property looks like
this. Some objects not relevant from the example point of view have been
omitted.

	Device (CAM0)
        {
		Name (_HID, "SONY319A")
		Name (_UID, Zero)
		Name (_CRS, ResourceTemplate ()
		{
			I2cSerialBus(0x0020, ControllerInitiated, 0x00061A80,
				     AddressingMode7Bit, "\\_SB.PCI0.I2C0",
				     0x00, ResourceConsumer)
		})
		Name (PRT4, Package() {
			ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
			Package () {
				Package () { "i2c-allow-low-power-probe", 1 },
			}
                })
	}

References
==========

[1] Device Properties UUID For _DSD.
    https://www.uefi.org/sites/default/files/resources/_DSD-device-properties-UUID.pdf,
    referenced 2020-09-02.
