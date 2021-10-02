#!/bin/bash

if [[ $EUID -ne 0 ]]; then
  echo "You must run this with superuser priviliges.  Try \"sudo ./dkms-remove.sh\"" 2>&1
  exit 1
else
  echo "About to run dkms removal steps..."
fi

DRV_NAME=rtl8821ce
DRV_VERSION=v5.5.2_34066.20200325

dkms remove ${DRV_NAME}/${DRV_VERSION} --all
rm -rf /usr/src/${DRV_NAME}-${DRV_VERSION}

RESULT=$?
if [[ "$RESULT" != "0" ]]; then
  echo "Error occurred while running dkms remove." 2>&1
else
  echo "Finished running dkms removal steps."
fi

exit $RESULT
