#!/bin/bash

if [ $(id -u) != 0 ]; then
    echo " Script must be run as root"
    exit
fi


if [[ "$(mokutil --sb-state)" == *enabled ]]; then
    SECUREBOOT="ON"
else
    SECUREBOOT="OFF"
fi

RUNASUSER="sudo -u $SUDO_USER"

# Run this block as user
$RUNASUSER bash << EOF
    echo " Building the module"
    make -j$(nproc)

    if [ ! -d ".ssl" ] && [ $SECUREBOOT == "ON" ]; then
        mkdir .ssl
    fi
EOF

echo -e "\n Installing the module..."
make install

# Sign module if SecureBoot is enabled
if [ $SECUREBOOT == "ON" ]; then
    echo -e "\n Creating X.509 key pair"
    cd .ssl
    openssl req -new -x509 -newkey rsa:2048 -keyout MOK.priv -outform DER -out MOK.der -nodes -days 36500 -subj "/CN=local_rtl8812au/"

    SIGN=/usr/src/linux-headers-$(uname -r)/scripts/sign-file
    MODULE=$(modinfo -n 88XXau)

    echo -e "\n Signing the following module"
    echo " $MODULE"

    $SIGN sha256 ./MOK.priv ./MOK.der $MODULE

    # Add key to trusted list
    echo -e "\n\t ATTENTION"
    echo -e " MOK manager ask you to enter input password."
    echo " This password will be needed once after first reboot."

    mokutil --import ./MOK.der

    echo ""
    echo " System requires reboot."
    echo " UEFI key manager will appear during the boot."
    echo " Select 'Enroll MOK' and 'Continue. Then enter input password."
else
    modprobe 88XXau
fi
