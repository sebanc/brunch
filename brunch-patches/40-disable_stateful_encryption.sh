# This patch disables stateful partition encryption (which does not work without official TPM since r103)

ret=0

cat >/roota/usr/sbin/mount-encrypted <<'MOUNTS'
#!/bin/bash
#touch /test
#echo "mount-encrypted called with args \"$@\"" >> /test
if [ $# -eq 0 ]; then
	if ! mountpoint -q /var && ! mountpoint -q /home/chronos; then
		mkdir -p /mnt/stateful_partition/encrypted/var
		mount -n --bind /mnt/stateful_partition/encrypted/var /var || return 1
		mkdir -p /mnt/stateful_partition/encrypted/chronos
		mount -n --bind /mnt/stateful_partition/encrypted/chronos /home/chronos || return 1
	fi
elif [ "$1" == "umount" ]; then
	umount /home/chronos
	umount /var
fi
MOUNTS
chmod 0755 /roota/usr/sbin/mount-encrypted
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
