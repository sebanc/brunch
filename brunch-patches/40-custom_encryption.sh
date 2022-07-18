# This patch implements a different cryptohome approach (swtpm does not work with cryptohome since r103)

disable_userdata_encryption=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "disable_userdata_encryption" ]; then disable_userdata_encryption=1; fi
done

ret=0

if [ "$disable_userdata_encryption" -eq 1 ]; then
cat >/roota/usr/share/cros/startup_utils.sh <<'NOENCRYPTION'
mount_var_and_home_chronos() {
  mkdir -p /mnt/stateful_partition/brunch_data/var
  mount -n --bind /mnt/stateful_partition/brunch_data/var /var || return 1
  mkdir -p /mnt/stateful_partition/brunch_data/unencrypted_home
  mount -n --bind /mnt/stateful_partition/brunch_data/unencrypted_home /home/chronos || return 1
  if [ -d /mnt/stateful_partition/brunch_data/encrypted_home ]; then
    rm -rf /mnt/stateful_partition/brunch_data/tmp
    mkdir -p /mnt/stateful_partition/brunch_data/tmp
    echo "passwd=$(cat /sys/class/dmi/id/board_serial /sys/class/dmi/id/board_name /sys/class/dmi/id/product_uuid /sys/class/dmi/id/product_name | tr '\n' 'n' | /usr/bin/openssl passwd -5 -stdin -salt xyz | xxd -pu -l 16)" > /tmp/passphrase
    mount -t ecryptfs -o key=passphrase:passphrase_passwd_file=/tmp/passphrase,ecryptfs_cipher=aes,ecryptfs_key_bytes=32,ecryptfs_passthrough=no,ecryptfs_enable_filename_crypto=no,no_sig_cache /mnt/stateful_partition/brunch_data/encrypted_home /mnt/stateful_partition/brunch_data/tmp
    rm -f /tmp/passphrase
    cp -rT /mnt/stateful_partition/brunch_data/tmp /home/chronos
    umount /mnt/stateful_partition/brunch_data/tmp
    rm -rf /mnt/stateful_partition/brunch_data/encrypted_home /mnt/stateful_partition/brunch_data/tmp
  fi
}

umount_var_and_home_chronos() {
umount /home/chronos
umount /var
}
NOENCRYPTION
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
else
cat >/roota/usr/share/cros/startup_utils.sh <<'ENCRYPTION'
mount_var_and_home_chronos() {
  mkdir -p /mnt/stateful_partition/brunch_data/var
  mount -n --bind /mnt/stateful_partition/brunch_data/var /var || return 1
  mkdir -p /mnt/stateful_partition/brunch_data/encrypted_home
  echo "passwd=$(cat /sys/class/dmi/id/board_serial /sys/class/dmi/id/board_name /sys/class/dmi/id/product_uuid /sys/class/dmi/id/product_name | tr '\n' 'n' | /usr/bin/openssl passwd -5 -stdin -salt xyz | xxd -pu -l 16)" > /tmp/passphrase
  mount -t ecryptfs -o key=passphrase:passphrase_passwd_file=/tmp/passphrase,ecryptfs_cipher=aes,ecryptfs_key_bytes=32,ecryptfs_passthrough=no,ecryptfs_enable_filename_crypto=no,no_sig_cache /mnt/stateful_partition/brunch_data/encrypted_home /home/chronos || return 1
  rm -f /tmp/passphrase
  if [ -d /mnt/stateful_partition/brunch_data/unencrypted_home ]; then
    cp -rT /mnt/stateful_partition/brunch_data/unencrypted_home /home/chronos
    rm -rf /mnt/stateful_partition/brunch_data/unencrypted_home
  fi
}

umount_var_and_home_chronos() {
umount /home/chronos
umount /var
}
ENCRYPTION
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi

exit $ret
