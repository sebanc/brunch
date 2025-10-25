# Add complementary features to chromeos:
# - useful tools: dnsmasq, rdmsr, wrmsr, tinyproxy, mksquashfs, unsquashfs, cpuid
# - efibootmgr
# - nano
# - swtpm
# - the option "chromebook_audio" to support audio for certain chromebook models
# - the option "rtbth" to make use of the RT3290 module built-in brunch

ret=0

tar zxf /rootc/packages/base.tar.gz -C /roota
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

tar zxf /rootc/packages/binaries.tar.gz -C /roota
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

tar zxf /rootc/packages/nano.tar.gz -C /roota
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi

tar zxf /rootc/packages/swtpm.tar.gz -C /roota
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "chromebook_audio" ]; then tar zxf /rootc/packages/chromebook_audio/"$(cat /sys/class/dmi/id/board_name | tr A-Z a-z)".tar.gz -C /roota; fi
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi
	if [ "$i" == "rtbth" ]; then tar zxf /rootc/packages/rtbth.tar.gz -C /roota; fi
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
done

exit $ret
