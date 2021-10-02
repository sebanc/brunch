#!/bin/bash

if [[ $EUID -ne 0 ]]; then
  echo "You must run this with superuser priviliges.  Try \"sudo ./dkms-remove.sh\"" 2>&1
  exit 1
else
  echo "About to run dkms removal steps..."
fi

DRV_DIR=rtl8821CU
DRV_NAME=rtl8821CU
DRV_VERSION=5.4.1

dkms remove ${DRV_NAME}/${DRV_VERSION} --all
rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}

RESULT=$?
if [[ "$RESULT" != "0" ]]; then
  echo "Error occurred while running dkms remove." 2>&1
else
  echo "Finished running dkms removal steps."
fi

exit $RESULT
