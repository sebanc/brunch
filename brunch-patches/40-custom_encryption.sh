# This patch bypasses early mount-encrypted call to avoid swtpm issue (swtpm does not work with cryptohome since r103)

ret=0

cat >/roota/usr/share/cros/startup_utils.sh <<'MOUNTS'
mount_var_and_home_chronos() {
  mkdir -p /mnt/stateful_partition/brunch/data/var
  mount -n --bind /mnt/stateful_partition/brunch/data/var /var || return 1
  mkdir -p /mnt/stateful_partition/brunch/data/home/chronos
  mount -n --bind /mnt/stateful_partition/brunch/data/home/chronos /home/chronos || return 1
}

umount_var_and_home_chronos() {
umount /home/chronos
umount /var
}
MOUNTS
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
