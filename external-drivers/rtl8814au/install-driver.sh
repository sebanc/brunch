#!/bin/sh

# Purpose: Install Realtek out-of-kernel USB WiFi adapter drivers.
#
# Supports dkms and non-dkms installations.
#
# To make this file executable:
#
# $ chmod +x install-driver.sh
#
# To execute this file:
#
# $ sudo ./install-driver.sh
#
# or
#
# $ sudo sh install-driver.sh
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

SCRIPT_NAME="install-driver.sh"
SCRIPT_VERSION="20230227"
MODULE_NAME="8814au"
DRV_VERSION="5.8.5.1"

KARCH="$(uname -m)"
KVER="$(uname -r)"
MODDESTDIR="/lib/modules/${KVER}/kernel/drivers/net/wireless/"

DRV_NAME="rtl${MODULE_NAME}"
DRV_DIR="$(pwd)"
OPTIONS_FILE="${MODULE_NAME}.conf"

# check to ensure sudo was used to start the script
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

# ensure /usr/sbin is in the PATH so iw can be found
if ! echo "$PATH" | grep -qw sbin; then
        export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
fi

# check to ensure gcc is installed
if ! command -v gcc >/dev/null 2>&1; then
	echo "A required package is not installed."
	echo "Please install the following package: gcc"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure bc is installed
if ! command -v bc >/dev/null 2>&1; then
	echo "A required package is not installed."
	echo "Please install the following package: bc"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure make is installed
if ! command -v make >/dev/null 2>&1; then
	echo "A required package is not installed."
	echo "Please install the following package: make"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to see if the correct header files are installed
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
	echo "Your kernel header files aren't properly installed."
	echo "Please consult your distro documentation or user support forums."
	echo "Once the header files are properly installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure iw is installed
if ! command -v iw >/dev/null 2>&1; then
	echo "A required package is not installed."
	echo "Please install the following package: iw"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure rfkill is installed
if ! command -v rfkill >/dev/null 2>&1; then
	echo "A required package is not installed."
	echo "Please install the following package: rfkill"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

DEFAULT_EDITOR="$(cat default-editor.txt)"
# try to find the user's default text editor through the EDITORS_SEARCH array
for TEXT_EDITOR in "${VISUAL}" "${EDITOR}" "${DEFAULT_EDITOR}" vi; do
	command -v "${TEXT_EDITOR}" >/dev/null 2>&1 && break
done
# fail if no editor was found
if ! command -v "${TEXT_EDITOR}" >/dev/null 2>&1; then
	echo "No text editor found (default: ${DEFAULT_EDITOR})."
	echo "Please install ${DEFAULT_EDITOR} or edit the file 'default-editor.txt' to specify your editor."
	echo "Once complete, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

echo ": ---------------------------"

# displays script name and version
echo ": ${SCRIPT_NAME} v${SCRIPT_VERSION}"

# information that helps with bug reports

# display architecture
echo ": ${KARCH} (architecture)"

SMEM=$(LANG=C free | awk '/Mem:/ { print $2 }')
sproc=$(nproc)
# avoid Out of Memory condition in low-RAM systems by limiting core usage
if [ "$sproc" -gt 1 ]; then
	if [ "$SMEM" -lt 1400000 ]
	then
		sproc=2
	fi
	if [ "$SMEM" -lt 700000 ]
	then
		sproc=1
	fi
fi

# display number of in-use processing units / total processing units
echo ": ${sproc}/$(nproc) (in-use/total processing units)"

# display total system memory
echo ": ${SMEM} (total system memory)"

# display kernel version
echo ": ${KVER} (kernel version)"

# display gcc version
gcc_ver=$(gcc --version | grep -i gcc)
echo ": ""${gcc_ver}"

# display dkms version if installed
if command -v dkms >/dev/null 2>&1; then
	dkms_ver=$(dkms --version)
	echo ": ""${dkms_ver}"
fi

# display secure mode status
if command -v mokutil >/dev/null 2>&1; then
	if mokutil --sb-state | grep -i  enabled >/dev/null 2>&1; then
		echo ": SecureBoot enabled"
	fi
	if mokutil --sb-state | grep -i  disabled >/dev/null 2>&1; then
		echo ": SecureBoot disabled"
	fi
	if mokutil --sb-state | grep -i  EFI >/dev/null 2>&1; then
		echo ": EFI variables are not supported on this system"
	fi
else
	echo ": mokutil not installed"
fi


echo ": ---------------------------"
echo

echo "Checking for previously installed drivers."

# check for and remove non-dkms installations
# standard naming
if [ -f "${MODDESTDIR}${MODULE_NAME}.ko" ]; then
	echo ": ---------------------------"
	echo
	echo "Removing a non-dkms installation: ${MODDESTDIR}${MODULE_NAME}.ko"
	rm -f "${MODDESTDIR}"${MODULE_NAME}.ko
	/sbin/depmod -a "${KVER}"
	echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	make clean >/dev/null 2>&1
	echo "Removal complete."
fi

# check for and remove non-dkms installations
# with rtl added to module name (PClinuxOS)
if [ -f "${MODDESTDIR}rtl${MODULE_NAME}.ko" ]; then
	echo ": ---------------------------"
	echo
	echo "Removing a non-dkms installation: ${MODDESTDIR}rtl${MODULE_NAME}.ko"
	rm -f "${MODDESTDIR}"rtl${MODULE_NAME}.ko
	/sbin/depmod -a "${KVER}"
	echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	make clean >/dev/null 2>&1
	echo "Removal complete."
fi

# check for and remove non-dkms installations
# with compressed module in a unique non-standard location (Armbian)
# Example: /usr/lib/modules/5.15.80-rockchip64/kernel/drivers/net/wireless/rtl8821cu/8821cu.ko.xz
# Dear Armbiam, this is a really bad idea.
if [ -f "/usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz" ]; then
	echo ": ---------------------------"
	echo
	echo "Removing a non-dkms installation: /usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz"
	rm -f /usr/lib/modules/"${KVER}"/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz
	/sbin/depmod -a "${KVER}"
	echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	make clean >/dev/null 2>&1
	echo "Removal complete."
fi

# check for and remove dkms installations
if command -v dkms >/dev/null 2>&1; then
	if dkms status | grep -i  ${DRV_NAME}; then
		echo ": ---------------------------"
		echo
# need to add code here to delete any DRV_VERSION
		echo "Removing a dkms installation."
		dkms remove -m ${DRV_NAME} -v ${DRV_VERSION} --all
		echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
		rm -f /etc/modprobe.d/${OPTIONS_FILE}
		echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
		rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
		echo "Removal complete."
	fi
fi

# sets module parameters (driver options) and blacklisted modules
echo ": ---------------------------"
echo
echo "Starting installation."
echo "Installing ${OPTIONS_FILE} to /etc/modprobe.d"
cp -f ${OPTIONS_FILE} /etc/modprobe.d

# determine if dkms is installed and run the appropriate installation routines
if ! command -v dkms >/dev/null 2>&1; then
	echo "The non-dkms installation routines are in use."

	make clean >/dev/null 2>&1

	make -j"$(nproc)"
	RESULT=$?

	if [ "$RESULT" != "0" ]; then
		echo "An error occurred:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "You will need to run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	fi

#	if secure boot is active, use sign-install
	if command -v mokutil >/dev/null 2>&1; then
		if mokutil --sb-state | grep -i  enabled >/dev/null 2>&1; then
			echo ": SecureBoot enabled - read FAQ about SecureBoot"
			make sign-install
			RESULT=$?
		else
			make install
			RESULT=$?		
		fi
	else
		make install
		RESULT=$?
	fi
	
	if [ "$RESULT" = "0" ]; then
        	make clean >/dev/null 2>&1
		echo "The driver was installed successfully."
		echo ": ---------------------------"
		echo
	else
		echo "An error occurred:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "You will need to run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	fi
else
	echo "The dkms installation routines are in use."

# 	the dkms add command requires source in /usr/src/${DRV_NAME}-${DRV_VERSION}
	echo "Copying source files to /usr/src/${DRV_NAME}-${DRV_VERSION}"
	cp -rf "${DRV_DIR}" /usr/src/${DRV_NAME}-${DRV_VERSION}

	dkms add -m ${DRV_NAME} -v ${DRV_VERSION}
	RESULT=$?

#	RESULT will be 3 if the DKMS tree already contains the same module/version
#	combo. You cannot add the same module/version combo more than once.
	if [ "$RESULT" != "0" ]; then
		if [ "$RESULT" = "3" ]; then
			echo "This driver may already be installed."
			echo "Run the following and then reattempt installation."
			echo "$ sudo ./remove-driver.sh"
			exit $RESULT
		else
			echo "An error occurred. dkms add error:  ${RESULT}"
			echo "Please report this error."
			echo "Please copy all screen output and paste it into the problem report."
			echo "Run the following before reattempting installation."
			echo "$ sudo ./remove-driver.sh"
			exit $RESULT
		fi
	else
		echo "The driver was added to dkms successfully."
		echo ": ---------------------------"
	fi

	if command -v /usr/bin/time >/dev/null 2>&1; then
		/usr/bin/time -f "Compile time: %U seconds" dkms build -m ${DRV_NAME} -v ${DRV_VERSION}
	else
		dkms build -m ${DRV_NAME} -v ${DRV_VERSION}
	fi
	RESULT=$?

	if [ "$RESULT" != "0" ]; then
		echo "An error occurred. dkms build error:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "Run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	else
		echo "The driver was built by dkms successfully."
		echo ": ---------------------------"
	fi

	dkms install -m ${DRV_NAME} -v ${DRV_VERSION}
	RESULT=$?

	if [ "$RESULT" != "0" ]; then
		echo "An error occurred. dkms install error:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "Run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	else
		echo "The driver was installed by dkms successfully."
		echo ": ---------------------------"
		echo
	fi
fi

# provide driver upgrade information
echo "Info: Upgrade this driver with the following commands as needed:"
echo "$ git pull"
echo "$ sudo sh install-driver.sh"
echo "Note: Upgrades to this driver should be performed before distro upgrades."
echo "Note: Upgrades can be performed as often as you like."
echo "Note: Work on this driver is continuous."
echo ": ---------------------------"
echo

# unblock wifi
if command -v rfkill >/dev/null 2>&1; then
	rfkill unblock wlan
else
	echo "Unable to run $ rfkill unblock wlan"
fi

# if NoPrompt is not used, ask user some questions
if [ $NO_PROMPT -ne 1 ]; then
	printf "Do you want to edit the driver options file now? (recommended) [Y/n] "
	read -r yn
	case "$yn" in
		[nN]) ;;
		*) ${TEXT_EDITOR} /etc/modprobe.d/${OPTIONS_FILE} ;;
	esac

	printf "Do you want to apply the new options by rebooting now? (recommended) [Y/n] "
	read -r yn
	case "$yn" in
		[nN]) ;;
		*) reboot ;;
	esac
fi
