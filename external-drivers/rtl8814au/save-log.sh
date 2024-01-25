#!/bin/sh

# Purpose: Save a log file with RTW lines only.
#
# To make this file executable:
#
# $ chmod +x save-log.sh
#
# To execute this file:
#
# $ sudo ./save-log.sh
#
# or
#
# $ sudo sh save-log.sh

SCRIPT_NAME="save-log.sh"

if [ "$(id -u)" -ne 0 ]; then
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# deletes existing log
rm -f -- rtw.log

dmesg | cut -d"]" -f2- | grep "RTW" >> rtw.log
RESULT=$?

if [ "$RESULT" != "0" ]; then
	echo "An error occurred while running: ${SCRIPT_NAME}"
	echo "Did you set a log level > 0 ?"
	exit 1
else
	echo "rtw.log saved successfully."
fi
