# This patch replaces the chromeos_startup binary with a script custom script.

ret=0

cat >/roota/sbin/chromeos_startup <<STARTUP
#!/bin/bash

mount_or_fail()
{
	echo "mount_or_fail was called with the following arguments: \$@"
	if ! mount \$@; then reboot -f; fi
}

mount_with_log()
{
	echo "mount_with_log was called with the following arguments: \$@"
	mount \$@
}

exec 1>>/root/brunch_startup_log
exec 2>>/root/brunch_startup_log
echo "Brunch startup:"

systemd-tmpfiles --create --remove --boot --prefix /dev --prefix /proc --prefix /run

mount_with_log -t debugfs -o nosuid,nodev,noexec,mode=0750,uid=0,gid=\$(cat /etc/group | grep '^debugfs-access:' | cut -d':' -f3) debugfs /sys/kernel/debug
mount_with_log -t tracefs -o nosuid,nodev,noexec,mode=0755 tracefs /sys/kernel/tracing
mount_with_log -t configfs -o nosuid,nodev,noexec configfs /sys/kernel/config
mount_with_log -t bpf -o nosuid,nodev,noexec,mode=0770,gid=\$(cat /etc/group | grep '^bpf-access:' | cut -d':' -f3) bpf /sys/fs/bpf
mount_with_log -t securityfs -o nosuid,nodev,noexec securityfs /sys/kernel/security
sysctl -q --system
mount_with_log -o bind /run/namespaces /run/namespaces
mount_with_log --make-private /run/namespaces

#sed '/^#/d' /usr/share/cros/startup/process_management_policies/*gid_allowlist.txt 2>/dev/null | sed -r '/^\s*\$/d' > /sys/kernel/security/safesetid/gid_allowlist_policy
#sed '/^#/d' /usr/share/cros/startup/process_management_policies/*uid_allowlist.txt 2>/dev/null | sed -r '/^\s*\$/d' > /sys/kernel/security/safesetid/uid_allowlist_policy
#echo '/var' > /sys/kernel/security/chromiumos/inode_security_policies/block_symlink
#echo '/var' > /sys/kernel/security/chromiumos/inode_security_policies/block_fifo
#echo '/var/lib/timezone' > /sys/kernel/security/chromiumos/inode_security_policies/allow_symlink
#echo '/var/log' > /sys/kernel/security/chromiumos/inode_security_policies/allow_symlink
#echo '/home' > /sys/kernel/security/chromiumos/inode_security_policies/allow_symlink
#cat /dev/null > /sys/kernel/security/loadpin/dm-verity

data_partition="\$(df -h --output=source / | tail -1 | sed 's/.\$//')1"
if [ ! -b \$data_partition ]; then echo "data partition \$data_partition was not found."; reboot -f; fi
tune2fs -g 20119 -O encrypt,project,quota,verity -Q usrquota,grpquota,prjquota \$data_partition
mount_or_fail -o nosuid,nodev,noexec,noatime,commit=600,discard \$data_partition /mnt/stateful_partition
if [ -f /mnt/stateful_partition/factory_install_reset ]; then echo "the factory_install_reset file triggered a powerwash."; rm -rf /mnt/stateful_partition/{*,.*}; fi
mount_with_log -o ro,nosuid,nodev,noexec /dev/loop0p8 /usr/share/oem
systemd-tmpfiles --create --remove --boot --prefix /mnt/stateful_partition
mount_or_fail -o bind /mnt/stateful_partition/home /home
mount_with_log -o remount,nosuid,nodev,noexec,nosymfollow /home
if [ -f /etc/init/tpm2-simulator.conf ]; then initctl start tpm2-simulator; fi
mkdir -p /mnt/stateful_partition/encrypted/chronos /mnt/stateful_partition/encrypted/var
chmod 0755 /mnt/stateful_partition/encrypted/chronos /mnt/stateful_partition/encrypted/var
mount_or_fail -o bind /mnt/stateful_partition/encrypted /mnt/stateful_partition/encrypted
mount_or_fail -o bind /mnt/stateful_partition/encrypted/chronos /home/chronos
mount_or_fail -o bind /mnt/stateful_partition/encrypted/var /var
rm -r /var/log
systemd-tmpfiles --create --remove --boot --prefix /home --prefix /var
mount_with_log -o bind /run /var/run
mount_with_log -o bind /run/lock /var/lock

for d in /etc/daemon-store/*/; do
	mkdir -p /run/daemon-store/\$(basename \$d)
	chmod 0755 /run/daemon-store/\$(basename \$d)
	mount_with_log -o bind /run/daemon-store/\$(basename \$d) /run/daemon-store/\$(basename \$d)
	mount_with_log --make-shared /run/daemon-store/\$(basename \$d)
	mkdir -p /run/daemon-store-cache/\$(basename \$d)
	chmod 0755 /run/daemon-store-cache/\$(basename \$d)
	mount_with_log -o bind /run/daemon-store-cache/\$(basename \$d) /run/daemon-store-cache/\$(basename \$d)
	mount_with_log --make-shared /run/daemon-store-cache/\$(basename \$d)
done

mount_with_log -t tmpfs -o nosuid,nodev,noexec media /media
mount_with_log --make-shared /media
systemd-tmpfiles --create --remove --boot --prefix /media

restorecon -r /home/chronos /home/root /home/user /sys/devices/system/cpu /var
for f in /home/.shadow/*; do if [ -f \$f ]; then restorecon \$f; fi; done
for f in /home/.shadow/.*; do if [ -f \$f ]; then restorecon \$f; fi; done
for f in /home/.shadow/*/*; do if [ -f \$f ]; then restorecon \$f; fi; done

mkdir -p /var/log/asan
chmod 1777 /var/log/asan
mkdir -p /mnt/stateful_partition/dev_image
chmod 0755 /mnt/stateful_partition/dev_image
mount_with_log -o bind /mnt/stateful_partition/dev_image /usr/local
mount_with_log -o remount,suid,dev,exec /usr/local
if [ -d /mnt/stateful_partition/var_overlay/cache/dlc-images ]; then mount_with_log -o bind /mnt/stateful_partition/var_overlay/cache/dlc-images /var/cache/dlc-images; fi
if [ -d /mnt/stateful_partition/var_overlay/db/pkg ]; then mount_with_log -o bind /mnt/stateful_partition/var_overlay/db/pkg /var/db/pkg; fi
if [ -d /mnt/stateful_partition/var_overlay/lib/portage ]; then mount_with_log -o bind /mnt/stateful_partition/var_overlay/lib/portage /var/lib/portage; fi

mount_with_log -o remount,ro /sys/kernel/security

exit 0
STARTUP
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /roota/sbin/chromeos_startup
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

exit $ret
