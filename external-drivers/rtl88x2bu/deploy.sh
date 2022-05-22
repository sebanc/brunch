#!/bin/bash

set -euo pipefail

function ensure_no_cli_args() {
    if [ $# -ne 0 ]
    then
        echo "No command line arguments accepted!" >&2
        exit 1
    fi
}

function ensure_root_permissions() {
    if ! sudo -v
    then
        echo "Root permissions required to deploy the driver!" >&2
        exit 1
    fi
}

function get_version() {
    sed -En 's/PACKAGE_VERSION="(.*)"/\1/p' dkms.conf
}

function deploy_driver() {
    VER=$(get_version)
    sudo rsync --delete --exclude=.git -rvhP ./ "/usr/src/rtl88x2bu-${VER}"
    for action in add build install
    do
      sudo dkms "${action}" -m rtl88x2bu -v "${VER}"
    done
    sudo modprobe 88x2bu
}

ensure_no_cli_args "$@"
ensure_root_permissions
deploy_driver
