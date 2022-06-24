#!/bin/bash
#
# Purpose: Change settings in the Makefile to support compiling 64 bit
# operating systems for Raspberry Pi Hardware.
#
# To make this file executable:
#
# $ chmod +x ARM64_RPI.sh
#
# To execute this file:
#
# $ ./ARM64_RPI.sh

sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile

sed -i 's/CONFIG_PLATFORM_ARM_RPI = y/CONFIG_PLATFORM_ARM_RPI = n/g' Makefile

sed -i 's/CONFIG_PLATFORM_ARM64_RPI = n/CONFIG_PLATFORM_ARM64_RPI = y/g' Makefile
RESULT=$?

if [[ "$RESULT" != "0" ]]; then
	echo "An error occurred and Raspberry Pi OS (64 bit) support was not turned on in Makefile."
	exit 1
else
	echo "Raspberry Pi OS (64 bit) support was turned on in Makefile as planned."
	exit 0
fi
