# Add complementary features to chromeos:
# - useful tools: dnsmasq, rdmsr, wrmsr, tinyproxy, mksquashfs, unsquashfs, cpuid
# - nano and its ncurses dependency
# - qemu
# - swtpm
# - the option "asus_c302" to support the ASUS C302 chromebook
# - the option "rtbth" to make use of the RT3290 module built-in brunch

ret=0

tar zxf /firmware/packages/binaries.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

tar zxf /firmware/packages/nano.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

tar zxf /firmware/packages/ncurses.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi

tar zxf /firmware/packages/qemu.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi

tar zxf /firmware/packages/swtpm.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi

tar zxf /firmware/packages/version.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "asus_c302" ]; then tar zxf /firmware/packages/asus_c302.tar.gz -C /system; fi
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 6))); fi
done

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "rtbth" ]; then tar zxf /firmware/packages/rtbth.tar.gz -C /system; fi
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 7))); fi
done

exit $ret
