#!/bin/bash

SCRIPT_NAME="remove-driver.sh"
SCRIPT_VERSION="20210531"

DRV_NAME="rtl8821cu"
DRV_VERSION="5.8.1.7"
OPTIONS_FILE="8821cu.conf"

DRV_DIR="$(pwd)"

if [[ $EUID -ne 0 ]]
then
	echo "You must run this script with superuser (root) privileges."
	echo "Try \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

echo "Starting removal."

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
	echo "Deleting ${DRV_DIR}"
	rm -rf ${DRV_DIR}
	echo "The driver was removed successfully."
else
	echo "An error occurred. dkms remove error = ${RESULT}"
	echo "Please report this error."
	exit $RESULT
fi

read -p "Are you ready to reboot now? [y/n] " -n 1 -r
echo    # move to a new line
if [[ $REPLY =~ ^[Yy]$ ]]
then
    reboot
fi

exit 0
