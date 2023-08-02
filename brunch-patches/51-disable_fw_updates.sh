# Disable chromebooks firmware writing mechanisms to avoid the risk (very low) that ChromeOS would flash the firmware of an unsupported device

native_chromebook_image=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "native_chromebook_image" ]; then native_chromebook_image=1; fi
done

if [ "$native_chromebook_image" -eq 1 ]; then exit 0; fi

ret=0

echo "exit 0" > /roota/usr/bin/nvramtool
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
echo "exit 0" > /roota/usr/sbin/chromeos-firmwareupdate
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo "exit 0" > /roota/usr/sbin/flashrom
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
echo "exit 0" > /roota/usr/sbin/gsctool
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
rm -r /roota/opt/google/touch/scripts/*
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi

exit $ret
