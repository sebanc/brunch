#!/bin/sh

# Purpose: Make it easier to edit the correct driver options file.
#
# Flexible editor support.
#
# To make this file executable:
#
# $ chmod +x edit-options.sh
#
# To execute this file:
#
# $ sudo ./edit-options.sh
#
# Copyright(c) 2023 Nick Morrow
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

SCRIPT_NAME="edit-options.sh"
# SCRIPT_VERSION="20230126"
OPTIONS_FILE="8814au.conf"

# check to ensure sudo was used to start the script
if [ "$(id -u)" -ne 0 ]; then
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

DEFAULT_EDITOR="$(cat default-editor.txt)"
# try to find the user's default text editor through the EDITORS_SEARCH array
for TEXT_EDITOR in "${VISUAL}" "${EDITOR}" "${DEFAULT_EDITOR}" vi; do
        command -v "${TEXT_EDITOR}" >/dev/null 2>&1 && break
done
# failure message if no editor was found
if ! command -v "${TEXT_EDITOR}" >/dev/null 2>&1; then
        echo "No text editor was found (default: ${DEFAULT_EDITOR})."
        echo "Please install ${DEFAULT_EDITOR} or edit the file 'default-editor.txt' to specify your editor."
        echo "Once complete, please run \"sudo ./${SCRIPT_NAME}\""
        exit 1
fi

${TEXT_EDITOR} /etc/modprobe.d/${OPTIONS_FILE}

printf "Do you want to apply the new options by rebooting now? (recommended) [y/N] "
read -r REPLY
case "$REPLY" in
	[yY]*) reboot ;;
esac
