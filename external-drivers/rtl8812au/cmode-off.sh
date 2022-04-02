#!/bin/bash
#
# 2021-12-18
#
# Purpose: Turn Concurrent Mode off.
#
# To make this file executable:
#
# $ chmod +x edit-options.sh
#
# To execute this file:
#
# $ ./cmode-off.sh

sed -i 's/CONFIG_CONCURRENT_MODE = y/CONFIG_CONCURRENT_MODE = n/g' Makefile
RESULT=$?

if [[ "$RESULT" != "0" ]]; then
	echo "An error occurred and Concurrent Mode was not turned off in Makefile."
	exit 1
else
	echo "Concurrent Mode was turned off in Makefile as planned."
fi
