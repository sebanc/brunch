# Import brunch-toolchain env variables

for i in /roota/usr/lib64/libm.so /roota/usr/lib64/libpthread.so /roota/usr/lib64/libc.so
do
	if grep -q 'OUTPUT_FORMAT' $i; then sed -i "s@/usr/lib64/@/usr/local/lib64/@g" $i; fi
done

sed -i 's@MODE="0660"@MODE="0666"@g' /roota/lib/udev/rules.d/99-vm.rules

if [ -d /roota/usr/include ]; then rm -r /roota/usr/include; fi
ln -s /usr/local/include /roota/usr/include
if [ -d /roota/usr/lib64/pkgconfig ]; then rm -r /roota/usr/lib64/pkgconfig; fi
if [ -d /roota/usr/share/pkgconfig ]; then rm -r /roota/usr/share/pkgconfig; fi

ln -s /usr/local/etc/profile.d/toolchain.sh /roota/etc/profile.d/toolchain.sh
cat >/roota/etc/init/toolchain.conf <<'INIT'
start on start-user-session
script
        if [ -x /usr/local/bin/start-toolchain ]; then
                sudo -H -u chronos bash -c '/usr/local/bin/start-toolchain'
        fi
end script
INIT
