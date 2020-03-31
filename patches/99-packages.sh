ret=0

tar zxf /firmware/binaries.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

tar zxf /firmware/nano.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

tar zxf /firmware/ncurses.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi

tar zxf /firmware/qemu.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi

tar zxf /firmware/swtpm.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi

exit $ret
