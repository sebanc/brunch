#!/bin/bash

# Purpose: Install Realtek out-of-kernel USB WiFi adapter drivers.
#
# Supports dkms and non-dkms installations.

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
SCRIPT_VERSION="20230109"
MODULE_NAME="8812au"
DRV_VERSION="5.13.6"

KVER="$(uname -r)"
KARCH="$(uname -m)"
KSRC="/lib/modules/${KVER}/build"
MODDESTDIR="/lib/modules/${KVER}/kernel/drivers/net/wireless/"

DRV_NAME="rtl${MODULE_NAME}"
DRV_DIR="$(pwd)"
OPTIONS_FILE="${MODULE_NAME}.conf"

# check to ensure sudo was used
if [[ $EUID -ne 0 ]]
then
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# ensure /usr/sbin is in the PATH so iw can be found
if ! echo "$PATH" | grep -qw sbin; then
        export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
fi

# check to ensure gcc is installed
if ! command -v gcc >/dev/null 2>&1
then
	echo "A required package is not installed."
	echo "Please install the following package: gcc"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure make is installed
if ! command -v make >/dev/null 2>&1
then
	echo "A required package is not installed."
	echo "Please install the following package: make"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to see if header files are installed
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
	echo "Your kernel header files aren't properly installed."
	echo "Please consult your distro documentation."
	echo "Once the header files are installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure iw is installed
if ! command -v iw >/dev/null 2>&1
then
	echo "A required package is not installed."
	echo "Please install the following package: iw"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure rfkill is installed
if ! command -v rfkill >/dev/null 2>&1
then
	echo "A required package is not installed."
	echo "Please install the following package: rfkill"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# check to ensure nano is installed
if ! command -v nano >/dev/null 2>&1
then
	echo "A required package is not installed."
	echo "Please install the following package: nano"
	echo "Once the package is installed, please run \"sudo ./${SCRIPT_NAME}\""
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

# displays script name and version
echo "Script: ${SCRIPT_NAME} v${SCRIPT_VERSION}"

# information that helps with bug reports

# display architecture
echo "Arch:${KARCH}"

# display total memory in system
grep MemTotal /proc/meminfo

# display kernel version
echo "Kernel:  ${KVER}"

# display gcc version
gcc_ver=$(gcc --version | grep -i gcc)
echo "gcc: "${gcc_ver}

# display dkms version
# run if dkms is installed
if command -v dkms >/dev/null 2>&1
then
	dkms --version
fi

# display secure mode status
# run if mokutil is installed
if command -v mokutil >/dev/null 2>&1
then
	mokutil --sb-state
fi

# display ISO 3166-1 alpha-2 Country Code
#a2_country_code=$(iw reg get | grep -i country)
#echo "Country:  "${a2_country_code}
#if [[ $a2_country_code == *"00"* ]];
#then
#    echo "The Country Code may not be properly set."
#    echo "File alpha-2_Country_Codes is located in the driver directory."
#    echo "Please read and follow the directions in the file after installation."
#fi

# check for and remove non-dkms installations
# standard naming
if [[ -f "${MODDESTDIR}${MODULE_NAME}.ko" ]]
then
	echo "Removing a non-dkms installation: ${MODDESTDIR}${MODULE_NAME}.ko"
	rm -f ${MODDESTDIR}${MODULE_NAME}.ko
	/sbin/depmod -a ${KVER}
	echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	make clean >/dev/null 2>&1
fi

# check for and remove non-dkms installations
# with rtl added to module name (PClinuxOS)
if [[ -f "${MODDESTDIR}rtl${MODULE_NAME}.ko" ]]
then
	echo "Removing a non-dkms installation: ${MODDESTDIR}rtl${MODULE_NAME}.ko"
	rm -f ${MODDESTDIR}rtl${MODULE_NAME}.ko
	/sbin/depmod -a ${KVER}
	echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	make clean >/dev/null 2>&1
fi

# check for and remove non-dkms installations
# with compressed module in a unique non-standard location (Armbian)
# Example: /usr/lib/modules/5.15.80-rockchip64/kernel/drivers/net/wireless/rtl8821cu/8821cu.ko.xz
# Dear Armbiam, this is a really bad idea.
if [[ -f "/usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz" ]]
then
	echo "Removing a non-dkms installation: /usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz"
	rm -f /usr/lib/modules/${KVER}/kernel/drivers/net/wireless/${DRV_NAME}/${MODULE_NAME}.ko.xz
	/sbin/depmod -a ${KVER}
	echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	make clean >/dev/null 2>&1
fi

# check for and remove dkms installations
if command -v dkms >/dev/null 2>&1
then
	if dkms status | grep -i  ${DRV_NAME}
	then
		echo "Removing a dkms installation: ${DRV_NAME}"
		dkms remove -m ${DRV_NAME} -v ${DRV_VERSION} --all
		echo "Removing ${OPTIONS_FILE} from /etc/modprobe.d"
		rm -f /etc/modprobe.d/${OPTIONS_FILE}
		echo "Removing source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
		rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	fi
fi

# sets module parameters (driver options) and blacklisted modules
echo "Installing ${OPTIONS_FILE} to /etc/modprobe.d"
cp -f ${OPTIONS_FILE} /etc/modprobe.d

# determine if dkms is installed and run the appropriate routines
if ! command -v dkms >/dev/null 2>&1
then
	echo "The non-dkms installation routines are in use."

	make clean >/dev/null 2>&1

	make -j$(nproc)
	RESULT=$?

	if [[ "$RESULT" != "0" ]]
	then
		echo "An error occurred:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "You will need to run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	fi

# 	As shown in Makefile
# 	install:
#		install -p -m 644 $(MODULE_NAME).ko  $(MODDESTDIR)
#		/sbin/depmod -a ${KVER}
	make install
	RESULT=$?

	if [[ "$RESULT" = "0" ]]
	then
        	make clean >/dev/null 2>&1
		echo "The driver was installed successfully."
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
	if [[ "$RESULT" != "0" ]]
	then
		if [[ "$RESULT" = "3" ]]
		then
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
	fi

	if command -v /usr/bin/time >/dev/null 2>&1
	then
		/usr/bin/time -f "Compile time: %U seconds" dkms build -m ${DRV_NAME} -v ${DRV_VERSION}
	else
		dkms build -m ${DRV_NAME} -v ${DRV_VERSION}
	fi
	RESULT=$?

	if [[ "$RESULT" != "0" ]]
	then
		echo "An error occurred. dkms build error:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "Run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	else
		echo "The driver was built by dkms successfully."
	fi

	dkms install -m ${DRV_NAME} -v ${DRV_VERSION}
	RESULT=$?

	if [[ "$RESULT" != "0" ]]
	then
		echo "An error occurred. dkms install error:  ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the problem report."
		echo "Run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	else
		echo "The driver was installed by dkms successfully."
	fi
fi

# unblock wifi
if command -v rfkill >/dev/null 2>&1
then
	rfkill unblock wlan
else
	echo "Unable to run $ rfkill unblock wlan"
fi

# if NoPrompt is not used, ask user some questions
if [ $NO_PROMPT -ne 1 ]
then
	read -p "Do you want to edit the driver options file now? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		nano /etc/modprobe.d/${OPTIONS_FILE}
	fi

	read -p "Do you want to reboot now? (recommended) [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		reboot
	fi
fi

exit 0
