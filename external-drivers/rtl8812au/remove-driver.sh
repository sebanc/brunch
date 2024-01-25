#!/bin/sh

# Purpose: Remove Realtek out-of-kernel USB WiFi adapter drivers.
#
# Supports dkms and non-dkms removals.
#
# To make this file executable:
#
# $ chmod +x remove-driver.sh
#
# To execute this file:
#
# $ sudo ./remove-driver.sh
#
# or
#
# $ sudo sh remove-driver.sh
#
# To check for errors and that this script does not require bash:
#
# $ shellcheck remove-driver.sh
#
# Copyright(c) 2023 Nick Morrow
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

SCRIPT_NAME="remove-driver.sh"
SCRIPT_VERSION="20240117"

MODULE_NAME="8812au"

DRV_NAME="rtl8812au"
DRV_VERSION="5.13.6"

OPTIONS_FILE="${MODULE_NAME}.conf"

#KARCH="$(uname -m)"
if [ -z "${KARCH+1}" ]; then
	KARCH="$(uname -m)"
fi

#KVER="$(uname -r)"
if [ -z "${KVER+1}" ]; then
	KVER="$(uname -r)"
fi

MODDESTDIR="/lib/modules/${KVER}/kernel/drivers/net/wireless/"

# check to ensure sudo or su - was used to start the script
if [ "$(id -u)" -ne 0 ]; then
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# support for the NoPrompt option allows non-interactive use of this script
NO_PROMPT=0
# get the script options
while [ $# -gt 0 ]
do
	case $1 in
		NoPrompt)
			NO_PROMPT=1 ;;
		*h|*help|*)
			echo "Syntax $0 <NoPrompt>"
			echo "       NoPrompt - noninteractive mode"
			echo "       -h|--help - Show help"
			exit 1
			;;
	esac
	shift
done

echo ": ---------------------------"

# displays script name and version
echo ": ${SCRIPT_NAME} v${SCRIPT_VERSION}"

# information that helps with bug reports

# display kernel architecture
echo ": ${KARCH} (kernel architecture)"

# display kernel version
echo ": ${KVER} (kernel version)"

echo ": ---------------------------"
echo

# check for and remove non-dkms installations
# standard naming
if [ -f "${MODDESTDIR}${MODULE_NAME}.ko" ]; then
	echo "Removing a non-dkms installation: ${MODDESTDIR}${MODULE_NAME}.ko"
	rm -f "${MODDESTDIR}"${MODULE_NAME}.ko
	/sbin/depmod -a "${KVER}"
fi

# check for and remove non-dkms installations
# with rtl added to module name (PClinuxOS)
# Dear PCLinuxOS devs, the driver name uses rtl, the module name does not.
if [ -f "${MODDESTDIR}rtl${MODULE_NAME}.ko" ]; then
	echo "Removing a non-dkms installation: ${MODDESTDIR}rtl${MODULE_NAME}.ko"
	rm -f "${MODDESTDIR}"rtl${MODULE_NAME}.ko
	/sbin/depmod -a "${KVER}"
fi

# check for and remove non-dkms installations
# with compressed module in a unique non-standard location (Armbian)
# Example: /usr/lib/modules/5.15.80-rockchip64/kernel/drivers/net/wireless/rtl8821cu/8821cu.ko.xz
if [ -f "/usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz" ]; then
	echo "Removing a non-dkms installation: /usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz"
	rm -f /usr/lib/modules/"${KVER}"/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz
	/sbin/depmod -a "${KVER}"
fi

# check for and remove dkms installations
#
if command -v dkms >/dev/null 2>&1; then
	dkms status | while IFS="/,: " read -r drvname drvver kerver _dummy; do
		case "$drvname" in *${MODULE_NAME})
			if [ "${kerver}" = "added" ]; then
				dkms remove -m "${drvname}" -v "${drvver}" --all
			else
				dkms remove -m "${drvname}" -v "${drvver}" -k "${kerver}" -c "/usr/src/${drvname}-${drvver}/dkms.conf"
			fi
		esac
	done
	if [ -f /etc/modprobe.d/${OPTIONS_FILE} ]; then
		echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
		rm /etc/modprobe.d/${OPTIONS_FILE}
	fi
	if [ -d /usr/src/${DRV_NAME}-${DRV_VERSION} ]; then
		echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
		rm -r /usr/src/${DRV_NAME}-${DRV_VERSION}
	fi
fi

# ensure the driver directory is clean in case driver was manually compiled
make clean >/dev/null 2>&1
echo "The driver was removed successfully."
echo "You may now delete the driver directory if desired."
echo ": ---------------------------"
echo

# if NoPrompt is not used, ask user some questions
if [ $NO_PROMPT -ne 1 ]; then
	printf "Do you want to reboot now? (recommended) [Y/n] "
	read -r yn
	case "$yn" in
		[nN]) ;;
		*) reboot ;;
	esac
fi
