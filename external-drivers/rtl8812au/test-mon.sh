#!/bin/bash


SCRIPT_NAME="test-mon.sh"
SCRIPT_VERSION="20211218"


# Check that sudo was used to start the script
if [[ $EUID -ne 0 ]]
then
	clear
	echo "You must run this script with superuser (root) privileges."
	echo "Try: \"sudo ./${SCRIPT_NAME}\""
	exit 1
fi


# Set color definitions (https://en.wikipedia.org/wiki/ANSI_escape_code)
# Black        0;30     Dark Gray     1;30
# Red          0;31     Light Red     1;31
# Green        0;32     Light Green   1;32
# Brown/Orange 0;33     Yellow        1;33
# Blue         0;34     Light Blue    1;34
# Purple       0;35     Light Purple  1;35
# Cyan         0;36     Light Cyan    1;36
# Light Gray   0;37     White         1;37
RED='\033[1;31m'
YELLOW='\033[0;33;1m'
GREEN='\033[1;32m'
CYAN='\033[1;36m'
NoColor='\033[0m'


# Display docs
clear
echo -e "${GREEN}"
echo ' --------------------------------'
echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
echo ' --------------------------------'
echo 
echo ' Purpose: Test monitor mode on the provided wlan interface'
echo
echo ' Usage: $ sudo ./test-mon.sh [interface:wlan0]'
echo
echo ' Please feel free to help make this script better.'
echo
echo ' Some parts of this script require the installation of:'
echo
echo ' aircrack-ng'
echo ' wireshark'
echo
echo ' Note: To exit this script and install the above: Ctrl + c'  
echo
echo ' Note: For installation on Debian based systems:'
echo
echo ' $ sudo apt install -y aircrack-ng wireshark'
echo
echo ' --------------------------------'
echo -e "${NoColor}"
# Interfering processes must be disabled prior to running this script:
#
#```
# $ sudo airmon-ng check kill
#```


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
ip link set dev $iface0 down
# Check if iface0 exists and continue if true
if [ $? -eq 0 ]
then
# 	Disabled interfering processes
#	clear
	echo
	read -p " Do you want to use airmon-ng to disable interfering processes? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		airmon-ng check kill
		read -p " Press any key to continue. " -n 1 -r
	fi

#	Display interface settings
	clear
	echo -e "${GREEN}"
	echo ' --------------------------------'
	echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
	echo ' --------------------------------'
	echo '    WiFi Interface:'
	echo '             '$iface0
	echo ' --------------------------------'
	iface_name=$(iw dev $iface0 info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
	echo '    name  - ' $iface_name
	iface_type=$(iw dev $iface0 info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
	echo '    type  - ' $iface_type
	iface_state=$(ip addr show $iface0 | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
	echo '    state - ' $iface_state
	iface_addr=$(iw dev $iface0 info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
	echo '    addr  - ' $iface_addr
	echo ' --------------------------------'
	echo -e "${NoColor}"		

#	Set addr (has to be done before renaming the interface)
	iface_addr_orig=$iface_addr
	read -p " Do you want to set a new addr? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		read -p " What addr do you want? ( 12:34:56:78:90:ab ) " iface_addr
		ip link set dev $iface0 address $iface_addr
	fi
#	iface_addr=$(iw dev $iface0 info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
#	echo '    addr  - ' $iface_addr 
#	exit 1

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
	iw dev $iface0 set monitor control
	
#	Rename interface
	ip link set dev $iface0 name $iface0mon
	
#	Bring the interface up
	ip link set dev $iface0mon up
	
#	Run airodump-ng
#	airodump-ng will display a list of detected access points and clients
#	https://www.aircrack-ng.org/doku.php?id=airodump-ng
#	https://en.wikipedia.org/wiki/Regular_expression
#	Display interface settings
	clear
	echo -e "${GREEN}"
	echo ' --------------------------------'
	echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
	echo ' --------------------------------'
	echo '    WiFi Interface:'
	echo '             '$iface0
	echo ' --------------------------------'
	iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
	echo '    name  - ' $iface_name
	iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
	echo '    type  - ' $iface_type
	iface_state=$(ip addr show $iface0mon | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
	echo '    state - ' $iface_state
	iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
	echo '    addr  - ' $iface_addr
	echo ' --------------------------------'
	echo -e "${NoColor}"
	echo -e " airodump-ng can receive and interpret key strokes while running..."
	echo
	echo -e " a - select active area"
	echo -e " i - invert sorting order"
	echo -e " s - change sort column"
	echo -e " q - quit"
	echo ' ----------------------------'
	echo
	read -p " Do you want to run airodump-ng to display a list of detected access points and clients? [y/N] " -n 1 -r
	echo
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
#
		airodump-ng -c 1-165 -a --ignore-negative-one --essid-regex '^(?=.)^(?!.*CoxWiFi)' $iface0mon
	fi

#	Set channel
	read -p " Do you want to set the channel? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		read -p " What channel do you want to set? " chan
#		ip link set dev $iface0mon down
		iw dev $iface0mon set channel $chan
#		ip link set dev $iface0mon up
	fi

#	Display interface settings
	clear
	echo -e "${GREEN}"
	echo ' --------------------------------'
	echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
	echo ' --------------------------------'
	echo '    WiFi Interface:'
	echo '             '$iface0
	echo ' --------------------------------'
	iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
	echo '    name  - ' $iface_name
	iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
	echo '    type  - ' $iface_type
	iface_state=$(ip addr show $iface0mon | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
	echo '    state - ' $iface_state
	iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
	echo '    addr  - ' $iface_addr
	iface_chan=$(iw dev $iface0mon info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
	echo '    chan  - ' $chan
	iface_txpw=$(iw dev $iface0mon info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
	echo '    txpw  - ' $iface_txpw
	echo ' --------------------------------'
	echo -e "${NoColor}"

#	Set txpw
	read -p " Do you want to set the txpower? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
	read -p " What txpw setting do you want? ( 2300 = 23 dBm ) " iface_txpw
#		ip link set dev $iface0mon down
		iw dev $iface0mon set txpower fixed $iface_txpw
#		ip link set dev $iface0mon up
	fi

#	Display interface settings
	clear
	echo -e "${GREEN}"
	echo ' --------------------------------'
	echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
	echo ' --------------------------------'
	echo '    WiFi Interface:'
	echo '             '$iface0
	echo ' --------------------------------'
	iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
	echo '    name  - ' $iface_name
	iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
	echo '    type  - ' $iface_type
	iface_state=$(ip addr show $iface0mon | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
	echo '    state - ' $iface_state
	iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
	echo '    addr  - ' $iface_addr
	iface_chan=$(iw dev $iface0mon info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
	echo '    chan  - ' $chan
	iface_txpw=$(iw dev $iface0mon info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
	echo '    txpw  - ' $iface_txpw
	echo ' --------------------------------'
	echo -e "${NoColor}"

#	Test injection capability with aireplay-ng
	read -p " Do you want to test injection capability? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
#		ip link set dev $iface0mon up
		aireplay-ng --test $iface0mon
	fi

#	Start wireshark
	read -p " Do you want to start Wireshark? [y/N] " -n 1 -r
	echo
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
#		ip link set dev $iface0mon up
		wireshark --interface wlan0mon
#		test filter: wlan.fc.type_subtype == 29
		#	Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' --------------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo ' --------------------------------'
		echo '    WiFi Interface:'
		echo '             '$iface0
		echo ' --------------------------------'
		iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name  - ' $iface_name
		iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type  - ' $iface_type
		iface_state=$(ip addr show $iface0mon | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
		echo '    state - ' $iface_state
		iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr  - ' $iface_addr
		iface_chan=$(iw dev $iface0mon info | grep 'channel' | sed 's/channel //' | sed -e 's/^[ \t]*//')
		echo '    chan  - ' $chan
#		iface_txpw=$(iw dev $iface0mon info | grep 'txpower' | sed 's/txpower //' | sed -e 's/^[ \t]*//')
#		echo '    txpw  - ' $iface_txpw
		echo ' --------------------------------'
		echo -e "${NoColor}"
	fi

#	Return the adapter to original settings
	read -p " Do you want to return the adapter to original settings? [Y/n] " -n 1 -r
	if [[ $REPLY =~ ^[Nn]$ ]]
	then
#		ip link set dev $iface0mon up
#		Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' --------------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo ' --------------------------------'
		echo '    WiFi Interface:'
		echo '             '$iface0
		echo ' --------------------------------'
		iface_name=$(iw dev $iface0mon info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name  - ' $iface_name
		iface_type=$(iw dev $iface0mon info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type  - ' $iface_type
		iface_state=$(ip addr show $iface0mon | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
		echo '    state - ' $iface_state
		iface_addr=$(iw dev $iface0mon info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr  - ' $iface_addr
		echo ' --------------------------------'
		echo -e "${NoColor}"
	else
		ip link set dev $iface0mon down
		ip link set dev $iface0mon address $iface_addr_orig
		iw $iface0mon set type managed
		ip link set dev $iface0mon name $iface0
		ip link set dev $iface0 up
#		Display interface settings
		clear
		echo -e "${GREEN}"
		echo ' --------------------------------'
		echo -e "    ${SCRIPT_NAME} ${SCRIPT_VERSION}"
		echo ' --------------------------------'
		echo '    WiFi Interface:'
		echo '             '$iface0
		echo ' --------------------------------'
		iface_name=$(iw dev $iface0 info | grep 'Interface' | sed 's/Interface //' | sed -e 's/^[ \t]*//')
		echo '    name  - ' $iface_name
		iface_type=$(iw dev $iface0 info | grep 'type' | sed 's/type //' | sed -e 's/^[ \t]*//')
		echo '    type  - ' $iface_type
		iface_state=$(ip addr show $iface0 | grep 'state' | sed 's/.*state \([^ ]*\)[ ]*.*/\1/')
		echo '    state - ' $iface_state
		iface_addr=$(iw dev $iface0 info | grep 'addr' | sed 's/addr //' | sed -e 's/^[ \t]*//')
		echo '    addr  - ' $iface_addr
		echo ' --------------------------------'
		echo -e "${NoColor}"
	fi
	exit 0
else
	echo -e "${YELLOW}ERROR: Please provide an existing interface as parameter! ${NoColor}"
	echo -e "${NoColor}Usage: $ ${CYAN}sudo ./$SCRIPT_NAME [interface:wlan0] ${NoColor}"
	echo -e "${NoColor}Tip:   $ ${CYAN}iw dev ${NoColor}(displays available interfaces)"
	exit 1
fi
