#!/bin/bash

SCRIPT_NAME="remove-driver.sh"
SCRIPT_VERSION="20211212"

DRV_NAME="rtl8812au"
DRV_VERSION="5.13.6"
OPTIONS_FILE="8812au.conf"

DRV_DIR="$(pwd)"
KRNL_VERSION="$(uname -r)"

clear
echo "Running ${SCRIPT_NAME} version ${SCRIPT_VERSION}"

# support for NoPrompt allows non-interactive use of this script
NO_PROMPT=0

# get the options
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

# check to ensure sudo was used
if [[ $EUID -ne 0 ]]
then
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

echo "Starting removal..."

dkms remove -m ${DRV_NAME} -v ${DRV_VERSION} --all
RESULT=$?

# RESULT will be 3 if there are no instances of module to remove
# however we still need to remove the files or the install script
# will complain.
if [[ ("$RESULT" = "0")||("$RESULT" = "3") ]]
then
	echo "Deleting ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "Deleting source files from /usr/src/${DRV_NAME}-${DRV_VERSION}"
	rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}
	echo "The driver was removed successfully."
	echo "You may now delete the driver directory if desired."
else
	echo "An error occurred. dkms remove error = ${RESULT}"
	echo "Please report this error."
	exit $RESULT
fi

if [ $NO_PROMPT -ne 1 ]
then
	read -p "Do you want to reboot now? (recommended) [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		reboot
	fi
fi

exit 0
