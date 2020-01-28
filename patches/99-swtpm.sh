ret=0
tar zxf /firmware/swtpm.tar.gz -C /system
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
exit $ret
