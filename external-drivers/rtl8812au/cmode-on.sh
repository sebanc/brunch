#!/bin/bash
#
# 2021-12-03
#
# Purpose: Turn Concurrent Mode on.
#
# To make this file executable:
#
# $ chmod +x edit-options.sh
#
# To execute this file:
#
# $ ./cmode-on.sh

sed -i 's/CONFIG_CONCURRENT_MODE = n/CONFIG_CONCURRENT_MODE = y/g' Makefile
RESULT=$?

if [[ "$RESULT" != "0" ]]; then
	echo "An error occurred and Concurrent Mode was not turned on in Makefile."
	exit 1
else
	echo "Concurrent Mode was turned on in Makefile as planned."
fi
