#!/bin/bash

# Purpose: Remove Realtek USB WiFi adapter drivers.
#
# This version of the removal script does not use dkms.

SCRIPT_NAME="remove-driver-no-dkms.sh"
SCRIPT_VERSION="20220419"
OPTIONS_FILE="8821cu.conf"

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

make uninstall
RESULT=$?

if [[ ("$RESULT" = "0")]]
then
	echo "Deleting ${OPTIONS_FILE} from /etc/modprobe.d"
	rm -f /etc/modprobe.d/${OPTIONS_FILE}
	echo "The driver was removed successfully."
else
	echo "An error occurred. Error = ${RESULT}"
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
