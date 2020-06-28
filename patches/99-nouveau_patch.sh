nouveau=0
for i in $(echo "$1" | sed 's#,# #g')
do
        if [ "$i" == "nouveau" ]; then nouveau=1; fi
done
 
ret=0
if [ "$nouveau" -eq 1 ]; then
        echo "brunch: $0 nouveau enabled" > /dev/kmsg
        cat >/system/etc/init/nouveau.conf <<MODPROBE
start on startup
 
script
        modprobe nouveau
end script
MODPROBE
        if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi
exit $ret
