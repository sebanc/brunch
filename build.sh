#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"sudo bash\""; exit 1; fi
if [ $(whoami) != "root" ]; then echo "Please run with sudo"; exit 1; fi
if [ -z $(which cgpt) ]; then echo "The cgpt package/binary has to be installed first"; exit 1; fi
if [ ! -f "./kernel/out/arch/x86/boot/bzImage" ]; then echo "The kernel has to be built first"; exit 1; fi
if [ ! -f "$1" ]; then echo "ChromeOS recovery image $1 not found"; exit 1; fi
if [ ! $(dd if="$1" bs=1 count=4 status=none | od -A n -t x1 | sed 's/ //g') == '33c0fa8e' ] || [ $(cgpt show -i 12 -b "$1") -eq 0 ] || [ $(cgpt show -i 13 -b "$1") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$1") == 'ROOT-A' ]; then echo "$1 is not a valid ChromeOS recovery image"; fi

if mountpoint -q ./chroot/dev; then umount ./chroot/dev; fi
if mountpoint -q ./chroot/sys; then umount ./chroot/sys; fi
if mountpoint -q ./chroot/proc; then umount ./chroot/proc; fi
if mountpoint -q ./chroot/out; then umount ./chroot/out; fi
if [ -d ./chroot ]; then rm -r ./chroot; fi
if [ -d ./out ]; then rm -r ./out; fi

mkdir -p ./chroot/out ./out
chmod 0777 ./out

recovery_image=$(losetup --show -fP "$1")
mount -o ro "$recovery_image"p3 ./out
cp -a ./out/* ./chroot/
umount ./out
mkdir ./chroot/home/chronos/image
dd if="$recovery_image"p12 of=./chroot/home/chronos/image/efi.img bs=1M
cp -r ./efi-mods ./chroot/home/chronos/image/
chown -R 1000:1000 ./chroot/home/chronos/image
losetup -d "$recovery_image"

chmod 0777 ./chroot/home/chronos
rm ./chroot/etc/resolv.conf
echo 'nameserver 8.8.4.4' > ./chroot/etc/resolv.conf
echo 'chronos ALL=(ALL) NOPASSWD: ALL' > ./chroot/etc/sudoers.d/95_cros_base

mkdir ./chroot/home/chronos/initramfs
cp ./scripts/brunch-init ./chroot/home/chronos/initramfs/init
chmod 0755 ./chroot/home/chronos/initramfs/init
chown -R 1000:1000 ./chroot/home/chronos/initramfs

mkdir -p ./chroot/home/chronos/rootc/lib
cd ./kernel
cp ./out/arch/x86/boot/bzImage ../chroot/home/chronos/rootc/kernel
cp -r ./out/headers ../chroot/home/chronos/rootc/lib/headers
make O=out INSTALL_MOD_PATH=../../chroot/home/chronos/rootc modules_install
cd ..

cp -r ./chroot/home/chronos/rootc/lib/modules ./chroot/home/chronos/rootc/lib/orig
cp -r ./external-drivers/backport-iwlwifi ./chroot/tmp/
cd ./chroot/tmp/backport-iwlwifi
make defconfig-iwlwifi-public
make
make KLIB=../../chroot/home/chronos/rootc install
cd ../../..
rm -r ./chroot/tmp/backport-iwlwifi
mv ./chroot/home/chronos/rootc/lib/modules ./chroot/home/chronos/rootc/lib/iwlwifi_backport
mv ./chroot/home/chronos/rootc/lib/orig ./chroot/home/chronos/rootc/lib/modules

cp -r ./external-drivers/rtl8188eu ./chroot/tmp/
cd ./chroot/tmp/rtl8188eu
make modules
cp ./8188eu.ko ../../../chroot/home/chronos/rootc/lib/modules/rtl8188eu.ko
cd ../../..
rm -r ./chroot/tmp/rtl8188eu

cp -r ./external-drivers/rtl8723de ./chroot/tmp/
cd ./chroot/tmp/rtl8723de
make modules
cp ./8723de.ko ../../../chroot/home/chronos/rootc/lib/modules/rtl8723de.ko
cd ../../..
rm -r ./chroot/tmp/rtl8723de

cp -r ./external-drivers/rtl8821ce ./chroot/tmp/
cd ./chroot/tmp/rtl8821ce
make modules
cp ./8821ce.ko ../../../chroot/home/chronos/rootc/lib/modules/rtl8821ce.ko
cd ../../..
rm -r ./chroot/tmp/rtl8821ce

cp -r ./external-drivers/broadcom-wl ./chroot/tmp/
cd ./chroot/tmp/broadcom-wl
make
cp ./wl.ko ../../../chroot/home/chronos/rootc/lib/modules/broadcom_wl.ko
cd ../../..
rm -r ./chroot/tmp/broadcom-wl

cp -r ./packages ./chroot/home/chronos/rootc/
cp -r ./patches ./chroot/home/chronos/rootc/
chmod -R 0755 ./chroot/home/chronos/rootc/patches
chown -R 1000:1000 ./chroot/home/chronos/rootc

mkdir -p ./chroot/home/chronos/rootc/usr/share/alsa
cp -r ./alsa-ucm-mods ./chroot/home/chronos/rootc/usr/share/alsa/ucm
chown -R 1000:1000 ./chroot/home/chronos/rootc/usr

cp -r ./firmware-mods ./chroot/home/chronos/
chown -R 1000:1000 ./chroot/home/chronos/firmware-mods

mkdir ./chroot/home/chronos/brunch
cp ./scripts/chromeos-install.sh ./chroot/home/chronos/brunch/
chmod 0755 ./chroot/home/chronos/brunch/chromeos-install.sh
chown -R 1000:1000 ./chroot/home/chronos/brunch

mount --bind ./out ./chroot/out
mount -t proc none ./chroot/proc
mount -t sysfs none ./chroot/sys
mount -t devtmpfs none ./chroot/dev

cp ./scripts/build-init ./chroot/init
chroot --userspec=1000:1000 ./chroot /init

umount ./chroot/dev
umount ./chroot/sys
umount ./chroot/proc
umount ./chroot/out
rm -r ./chroot
