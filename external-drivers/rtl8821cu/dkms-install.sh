#!/bin/bash

if [[ $EUID -ne 0 ]]; then
  echo "You must run this with superuser priviliges.  Try \"sudo ./dkms-install.sh\"" 2>&1
  exit 1
else
  echo "About to run dkms install steps..."
fi


DRV_DIR=`pwd`
DRV_NAME=rtl8821CU
DRV_VERSION=5.4.1

cp -r ${DRV_DIR} /usr/src/${DRV_NAME}-${DRV_VERSION}

dkms add -m ${DRV_NAME} -v ${DRV_VERSION}
dkms build -m ${DRV_NAME} -v ${DRV_VERSION}
dkms install -m ${DRV_NAME} -v ${DRV_VERSION}
RESULT=$?

echo "Finished running dkms install steps."

if grep -q -e "^CONFIG_DISABLE_IPV6 = y$" "$DRV_DIR/Makefile" ; then
	if echo "net.ipv6.conf.all.disable_ipv6 = 1
  net.ipv6.conf.default.disable_ipv6 = 1
  net.ipv6.conf.lo.disable_ipv6 = 1" >> /etc/sysctl.conf; then
		echo "Disabled IPv6 Successfuly "
		sysctl -p
	else
		echo "Could not disable IPv6"
	fi
fi

exit $RESULT
