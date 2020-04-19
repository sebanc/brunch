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

for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "asus_c302" ]; then tar zxf /firmware/packages/asus_c302.tar.gz -C /system; fi
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 5))); fi
done

exit $ret
