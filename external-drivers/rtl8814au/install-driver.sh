#!/bin/bash

# Purpose: Install Realtek out-of-kernel USB WiFi adapter drivers.
#
# Supports dkms and non-dkms installations.

SCRIPT_NAME="install-driver.sh"
SCRIPT_VERSION="20221018"
MODULE_NAME="8814au"
DRV_VERSION="5.8.5.1"
OPTIONS_FILE="${MODULE_NAME}.conf"

KVER="$(uname -r)"
MODDESTDIR="/lib/modules/${KVER}/kernel/drivers/net/wireless/"

DRV_NAME="rtl${MODULE_NAME}"
DRV_DIR="$(pwd)"

# check to ensure sudo was used
if [[ $EUID -ne 0 ]]
then
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

# displays script name and version
echo "Running ${SCRIPT_NAME} version ${SCRIPT_VERSION}"

# check for and remove non-dkms installation
if [[ -f "${MODDESTDIR}${MODULE_NAME}.ko" ]]
then
	echo "Removing a non-dkms installation."
	rm -f ${MODDESTDIR}${MODULE_NAME}.ko
	/sbin/depmod -a ${KVER}
fi

# information that helps with bug reports
# kernel
uname -r
# architecture - for ARM: aarch64 = 64 bit, armv7l = 32 bit
uname -m
#getconf LONG_BIT (may be handy in the future)

# sets module parameters (driver options)
echo "Installing ${OPTIONS_FILE} to: /etc/modprobe.d"
cp -f ${OPTIONS_FILE} /etc/modprobe.d

# determine if dkms is installed and run the appropriate routines
if ! command -v dkms >/dev/null 2>&1
then
	echo "The non-dkms installation routines are in use."

	make clean

	make
	RESULT=$?

	if [[ "$RESULT" != "0" ]]
	then
		echo "An error occurred. Error = ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the report."
		echo "You will need to run the following before reattempting installation."
		echo "$ sudo ./remove-driver-no-dkms.sh"
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
		echo "The driver was installed successfully."
	else
		echo "An error occurred. Error = ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the report."
		echo "You will need to run the following before reattempting installation."
		echo "$ sudo ./remove-driver-no-dkms.sh"
		exit $RESULT
	fi
else
	echo "The dkms installation routines are in use."

# 	the dkms add command requires source in /usr/src/${DRV_NAME}-${DRV_VERSION}
	echo "Copying source files to: /usr/src/${DRV_NAME}-${DRV_VERSION}"
	cp -rf "${DRV_DIR}" /usr/src/${DRV_NAME}-${DRV_VERSION}
	
	dkms add -m ${DRV_NAME} -v ${DRV_VERSION}
	RESULT=$?

#	RESULT will be 3 if the DKMS tree already contains the same module/version
#	combo. You cannot add the same module/version combo more than once.
	if [[ "$RESULT" != "0" ]]
	then
		if [[ "$RESULT" = "3" ]]
		then
			echo "Run the following and then reattempt installation."
			echo "$ sudo ./remove-driver.sh"
			exit $RESULT 
		else
			echo "An error occurred. dkms add error = ${RESULT}"
			echo "Please report this error."
			echo "Please copy all screen output and paste it into the report."
			echo "Run the following before reattempting installation."
			echo "$ sudo ./remove-driver.sh"
			exit $RESULT
		fi
	fi

	dkms build -m ${DRV_NAME} -v ${DRV_VERSION}
	RESULT=$?

	if [[ "$RESULT" != "0" ]]
	then
		echo "An error occurred. dkms build error = ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the report."
		echo "Run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	fi

	dkms install -m ${DRV_NAME} -v ${DRV_VERSION}
	RESULT=$?

	if [[ "$RESULT" = "0" ]]
	then
		echo "The driver was installed successfully."
	else
		echo "An error occurred. dkms install error = ${RESULT}"
		echo "Please report this error."
		echo "Please copy all screen output and paste it into the report."
		echo "Run the following before reattempting installation."
		echo "$ sudo ./remove-driver.sh"
		exit $RESULT
	fi
fi

# unblock wifi
rfkill unblock wlan

# if NoPrompt is not used, ask user some questions to complete installation
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
