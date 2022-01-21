#!/bin/bash

SCRIPT_NAME="start-mon.sh"
SCRIPT_VERSION="20211109"

# Purpose: Start and test monitor mode on the provided wlan interface
#
# Usage: $ sudo ./start-mon.sh [interface:wlan0]
#
# Please feel free to help make this script better.
#
# Some parts of this script require the installation of the following:
#	aircrack-ng
#	wireshark
#
# $ sudo apt install -y aircrack-ng wireshark
#
# Interfering processes must be disabled prior to running this script:
#
# Option 1
#```
# $ sudo airmon-ng check kill
#```
#
# Option 2, another way that works for me on Linux Mint:
#
# Note: I use multiple wifi adapters in my systems and I need to stay
# connected to the internet while testing. This option works well for
# me and allows me to stay connected by allowing Network Manager to
# continue managing interfaces that are not marked as unmanaged.
#
# Ensure Network Manager doesn't cause problems
#```
# $ sudo nano /etc/NetworkManager/NetworkManager.conf
#```
# add
#```
# [keyfile]
# unmanaged-devices=interface-name:<wlan0>;interface-name:wlan0mon
#```
#
# Note: Tells Network Manager to leave the specified interfaces alone.

# Set color definitions (https://en.wikipedia.org/wiki/ANSI_escape_code)
RED='\033[1;31m'
YELLOW='\033[0;33;1m'
GREEN='\033[1;32m'
CYAN='\033[1;36m'
NoColor='\033[0m'


# Check that sudo was used to start the script
if [[ $EUID -ne 0 ]]
then
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi

# Assign default monitor mode interface name
iface0mon='wlan0mon'

# Assign default channel
chan=6

# Activate option to set automatic or manual interface mode
#
# Option 1: if you only have one wlan interface (automatic detection)
#iface0=`iw dev | grep 'Interface' | sed 's/Interface //'`
#
# Option 2: if you have more than one wlan interface (default wlan0)
iface0=${1:-wlan0}

# Set iface0 down
ip link set $iface0 down
# Check if iface0 exists and continue if true
if [ $? -eq 0 ]
then
#	Display settings
#	clear
#	echo "Running ${SCRIPT_NAME} version ${SCRIPT_VERSION}"
#	echo
#	echo ' ----------------------------'
#	echo '    WiFi Interface Status'
#	echo '    '$iface0
#	echo ' ----------------------------'
#	Display interface settings
#	iface_name=$(iw dev $iface0 info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
#	echo '    name - ' $iface_name
#	iface_type=$(iw dev $iface0 info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
#	echo '    type - ' $iface_type
#	iface_addr=$(iw dev $iface0 info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
#	echo '    addr - ' $iface_addr
#	iface_chan=$(iw dev $iface0 info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
#	echo '    chan - ' $chan
#	iface_txpw=$(iw dev $iface0 info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
#	echo '    txpw - ' $iface_txpw
#	echo ' ----------------------------'

# 	Rename the interface
#	echo
#	read -p " Do you want to rename $iface0 to wlan0mon? [y/N] " -n 1 -r
#	echo
#	if [[ $REPLY =~ ^[Yy]$ ]]
#	then
	ip link set $iface0 name $iface0mon
#	else
#		iface0mon=$iface0
#	fi
	
#	Set monitor mode
#	iw dev <devname> set monitor <flag>
#		Valid monitor flags are:
#		none:     no special flags
#		fcsfail:  show frames with FCS errors
#		control:  show control frames
#		otherbss: show frames from other BSSes
#		cook:     use cooked mode
#		active:   use active mode (ACK incoming unicast packets)
#		mumimo-groupid <GROUP_ID>: use MUMIMO according to a group id
#		mumimo-follow-mac <MAC_ADDRESS>: use MUMIMO according to a MAC address
	iw dev $iface0mon set monitor none
# 	Set iface0 up
	ip link set $iface0mon up
#	Display interface settings
	clear
	echo -e "${GREEN}"
	echo ' -----------------------------'
	echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
	echo '    WiFi Interface:'
	echo '            '$iface0
	echo ' -----------------------------'
	iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
	echo '    name - ' $iface_name
	iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
	echo '    type - ' $iface_type
	iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
	echo '    addr - ' $iface_addr
	echo ' -----------------------------'
	echo -e "${NoColor}"
	
#	Set addr
	read -p " Do you want to set a new addr? [y/N] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		iface_addr_orig=$iface_addr
		echo
		read -p " What addr do you want? ( 12:34:56:78:90:ab ) " iface_addr
		ip link set dev $iface0mon down
		ip link set dev $iface0mon address $iface_addr
		ip link set dev $iface0mon up
#		Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' -----------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo '    WiFi Interface:'
		echo '            '$iface0
		echo ' -----------------------------'
		iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name - ' $iface_name
		iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type - ' $iface_type
		iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr - ' $iface_addr
#		iface_chan=$(iw dev $iface0mon info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
#		echo '    chan - ' $chan
#		iface_txpw=$(iw dev $iface0mon info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
#		echo '    txpw - ' $iface_txpw
		echo ' -----------------------------'
		echo -e "${NoColor}"
	fi

#	Run airodump-ng
#	airodump-ng will display a list of detected access points and clients
#	https://www.aircrack-ng.org/doku.php?id=airodump-ng
#	https://en.wikipedia.org/wiki/Regular_expression
	echo -e " airodump-ng can receive and interpret key strokes while running..."
	echo
	echo -e " a - select active area"
	echo -e " i - invert sorting order"
	echo -e " s - change sort column"
	echo -e " q - quit"
	echo ' ----------------------------'
	echo
	read -p " Do you want run airodump-ng to display a list of detected access points and clients? [y/N] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
#		 usage: airodump-ng <options> <interface>[,<interface>,...]
#
#		  -c <channels>        : Capture on specific channels
#		  -a                   : Filter unassociated clients
#		 --ignore-negative-one : Removes the message that says fixed channel <interface>: -1
#		 --essid-regex <regex> : Filter APs by ESSID using a regular expression
#
#		Select option
#
#		1) shows hidden ESSIDs
#		airodump-ng -c 1-165 -a --ignore-negative-one $iface0mon
#
#		2) does not show hidden ESSIDs
		airodump-ng -c 1-165 -a --ignore-negative-one --essid-regex '^(?=.)^(?!.*CoxWiFi)' $iface0mon
	fi

#	Set channel
	read -p " Do you want to set the channel? [y/N] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		echo
		read -p " What channel do you want to set? " chan
	fi
#	ip link set dev $iface0mon down
	iw dev $iface0mon set channel $chan
#	ip link set dev $iface0mon up
#	Display interface settings
	clear
	echo -e "${GREEN}"
	echo ' -----------------------------'
	echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
	echo '    WiFi Interface:'
	echo '            '$iface0
	echo ' -----------------------------'
	iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
	echo '    name - ' $iface_name
	iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
	echo '    type - ' $iface_type
	iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
	echo '    addr - ' $iface_addr
	iface_chan=$(iw dev $iface0mon info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
	echo '    chan - ' $chan
	iface_txpw=$(iw dev $iface0mon info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
	echo '    txpw - ' $iface_txpw
	echo ' -----------------------------'
	echo -e "${NoColor}"

#	Set txpw
	read -p " Do you want to set the txpower? [y/N] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		echo
		read -p " What txpw setting do you want? ( 2300 = 23 dBm ) " iface_txpw
#		ip link set dev $iface0mon down
		iw dev $iface0mon set txpower fixed $iface_txpw
#		ip link set dev $iface0mon up
#		Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' -----------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo '    WiFi Interface:'
		echo '            '$iface0
		echo ' -----------------------------'
		iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name - ' $iface_name
		iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type - ' $iface_type
		iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr - ' $iface_addr
		iface_chan=$(iw dev $iface0mon info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
		echo '    chan - ' $chan
		iface_txpw=$(iw dev $iface0mon info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
		echo '    txpw - ' $iface_txpw
		echo ' -----------------------------'
		echo -e "${NoColor}"
	fi

#	Test injection capability with aireplay-ng
	read -p " Do you want to test injection capability? [y/N] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		echo
		aireplay-ng --test $iface0mon
	fi

#	Start wireshark
	read -p " Do you want to start Wireshark? [y/N] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		wireshark --interface wlan0mon
#		filter: wlan.fc.type_subtype == 29		
	fi

#	Return the adapter to original settings
	read -p " Do you want to return the adapter to original settings? [Y/n] " -n 1 -r
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
#		Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' -----------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo '    WiFi Interface:'
		echo '            '$iface0
		echo ' -----------------------------'
		iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name - ' $iface_name
		iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type - ' $iface_type
		iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr - ' $iface_addr
		echo ' -----------------------------'
		echo -e "${NoColor}"
	else
		ip link set $iface0mon down
		ip link set $iface0mon name $iface0
		iw $iface0 set type managed
		ip link set dev $iface0 address $iface_addr_orig
		ip link set $iface0 up
#		Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' -----------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo '    WiFi Interface:'
		echo '            '$iface0
		echo ' -----------------------------'
		iface_name=$(iw dev $iface0 info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name - ' $iface_name
		iface_type=$(iw dev $iface0 info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type - ' $iface_type
		iface_addr=$(iw dev $iface0 info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr - ' $iface_addr
		echo ' -----------------------------'
		echo -e "${NoColor}"
	fi
	exit 0
else
	echo -e "${YELLOW}ERROR: Please provide an existing interface as parameter! ${NoColor}"
	echo -e "${NoColor}Usage: $ ${CYAN}sudo ./start-mon.sh [interface:wlan0] ${NoColor}"
	echo -e "${NoColor}Tip:   $ ${CYAN}iw dev ${NoColor}(displays available interfaces)"
	exit 1
fi
