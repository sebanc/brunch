#!/bin/bash

if [ ! -d /home/runner/work ]; then NTHREADS=$(nproc); else NTHREADS=$(($(nproc)*4)); fi

kernels=$(ls -d ./kernels/* | sed 's#./kernels/##g')
for kernel in $kernels; do
	if [ ! -f "./kernels/$kernel/out/arch/x86/boot/bzImage" ]; then echo "The kernel $kernel has to be built first"; exit 1; fi
done
if ( ! test -z {,} ); then echo "Must be ran with \"sudo bash\""; exit 1; fi
if [ $(whoami) != "root" ]; then echo "Please run with sudo"; exit 1; fi

if mountpoint -q ./chroot/dev/shm; then umount ./chroot/dev/shm; fi
if mountpoint -q ./chroot/dev; then umount ./chroot/dev; fi
if mountpoint -q ./chroot/sys; then umount ./chroot/sys; fi
if mountpoint -q ./chroot/proc; then umount ./chroot/proc; fi
if mountpoint -q ./chroot/out; then umount ./chroot/out; fi
if [ -d ./chroot ]; then rm -r ./chroot; fi
if [ -d ./out ]; then rm -r ./out; fi

mkdir -p ./chroot/out ./out || { echo "Failed to create output directory"; exit 1; }
chmod 0777 ./out || { echo "Failed to fix output directory permissions"; exit 1; }

if [ ! -z $1 ] && [ "$1" != "skip" ] ; then
	if [ ! -f "$1" ]; then echo "ChromeOS recovery image $1 not found"; exit 1; fi
	if [ ! $(dd if="$1" bs=1 count=4 status=none | od -A n -t x1 | sed 's/ //g') == '33c0fa8e' ] || [ $(cgpt show -i 12 -b "$1") -eq 0 ] || [ $(cgpt show -i 13 -b "$1") -gt 0 ] || [ ! $(cgpt show -i 3 -l "$1") == 'ROOT-A' ]; then echo "$1 is not a valid ChromeOS recovery image"; fi
	recovery_image=$(losetup --show -fP "$1")
	[ -b "$recovery_image"p3 ] || { echo "Failed to setup loop device"; exit 1; }
	mount -o ro "$recovery_image"p3 ./out || { echo "Failed to mount ChromeOS rootfs"; exit 1; }
	cp -a ./out/* ./chroot/ || { echo "Failed to copy ChromeOS rootfs content"; exit 1; }
	umount ./out || { echo "Failed to unmount ChromeOS rootfs"; exit 1; }
	losetup -d "$recovery_image" || { echo "Failed to detach loop device"; exit 1; }
else
	git clone -b master https://github.com/sebanc/chromeos-ota-extract.git rootfs || { echo "Failed to clone chromeos-ota-extract"; exit 1; }
	cd rootfs
	curl -L https://dl.google.com/chromeos/rammus/16063.45.0/stable-channel/chromeos_16063.45.0_rammus_stable-channel_full_mp-v5.bin-gy3tinjzgu4glfbberjrt4viet54fz4j.signed -o ./update.signed || { echo "Failed to Download the OTA update"; exit 1; }
	PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python python3 extract_android_ota_payload.py ./update.signed || { echo "Failed to extract the OTA update"; exit 1; }
	cd ..
	[ -f ./rootfs/root.img ] || { echo "ChromeOS rootfs has not been extracted"; exit 1; }
	mount -o ro ./rootfs/root.img ./out || { echo "Failed to mount ChromeOS rootfs image"; exit 1; }
	cp -a ./out/* ./chroot/ || { echo "Failed to copy ChromeOS rootfs content"; exit 1; }
	umount ./out || { echo "Failed to unmount ChromeOS rootfs image"; exit 1; }
	rm -r ./rootfs
fi

mkdir -p ./chroot/home/chronos/image/tmp || { echo "Failed to create image directory"; exit 1; }
cp -r ./efi-partition ./chroot/home/chronos/image/ || { echo "Failed to copy the efi partition directory"; exit 1; }
chown -R 1000:1000 ./chroot/home/chronos/image || { echo "Failed to fix image directory ownership"; exit 1; }

chmod 0777 ./chroot/home/chronos || { echo "Failed to fix chronos directory permissions"; exit 1; }
rm -f ./chroot/etc/resolv.conf
echo 'nameserver 8.8.4.4' > ./chroot/etc/resolv.conf || { echo "Failed to replace chroot resolv.conf file"; exit 1; }
echo 'chronos ALL=(ALL) NOPASSWD: ALL' > ./chroot/etc/sudoers.d/95_cros_base || { echo "Failed to add custom chroot sudoers file"; exit 1; }

mkdir ./chroot/home/chronos/brunch || { echo "Failed to create brunch directory"; exit 1; }
cp ./scripts/chromeos-install.sh ./chroot/home/chronos/brunch/ || { echo "Failed to copy the chromeos-install.sh script"; exit 1; }
chmod 0755 ./chroot/home/chronos/brunch/chromeos-install.sh || { echo "Failed to change chromeos-install.sh permissions"; exit 1; }
chown -R 1000:1000 ./chroot/home/chronos/brunch || { echo "Failed to fix brunch directory ownership"; exit 1; }

mkdir -p ./chroot/home/chronos/initramfs/sbin || { echo "Failed to create initramfs directory"; exit 1; }
cp ./scripts/brunch-init ./chroot/home/chronos/initramfs/init || { echo "Failed to copy brunch init script"; exit 1; }
cp ./scripts/brunch-setup ./chroot/home/chronos/initramfs/sbin/ || { echo "Failed to copy brunch setup script"; exit 1; }
cp -r ./bootsplashes ./chroot/home/chronos/initramfs/ || { echo "Failed to copy bootsplashes"; exit 1; }
chmod 0755 ./chroot/home/chronos/initramfs/init || { echo "Failed to change init script permissions"; exit 1; }
chown -R 1000:1000 ./chroot/home/chronos/initramfs || { echo "Failed to fix initramfs directory ownership"; exit 1; }

mkdir ./chroot/home/chronos/rootc || { echo "Failed to create rootc directory"; exit 1; }
ln -s kernel-6.6 ./chroot/home/chronos/rootc/kernel || { echo "Failed to make the default kernel symlink"; exit 1; }
ln -s kernel ./chroot/home/chronos/rootc/kernel-4.19 || { echo "Failed to make the legacy kernel symlink"; exit 1; }
ln -s kernel ./chroot/home/chronos/rootc/kernel-5.4 || { echo "Failed to make the legacy kernel symlink"; exit 1; }
ln -s kernel ./chroot/home/chronos/rootc/kernel-5.10 || { echo "Failed to make the legacy kernel symlink"; exit 1; }
ln -s kernel ./chroot/home/chronos/rootc/kernel-5.15 || { echo "Failed to make the legacy kernel symlink"; exit 1; }
ln -s kernel-chromebook-6.6 ./chroot/home/chronos/rootc/kernel-macbook || { echo "Failed to make the macbook kernel symlink"; exit 1; }
ln -s kernel-chromebook-6.6 ./chroot/home/chronos/rootc/kernel-macbook-t2 || { echo "Failed to make the macbook kernel symlink"; exit 1; }
cp -r ./packages ./chroot/home/chronos/rootc/ || { echo "Failed to copy brunch packages"; exit 1; }
cp -r ./brunch-patches ./chroot/home/chronos/rootc/patches || { echo "Failed to copy brunch patches"; exit 1; }
chmod -R 0755 ./chroot/home/chronos/rootc/patches || { echo "Failed to change patches directory permissions"; exit 1; }
chown -R 1000:1000 ./chroot/home/chronos/rootc || { echo "Failed to fix rootc directory ownership"; exit 1; }

for kernel in $kernels; do

mkdir -p ./chroot/home/chronos/kernel || { echo "Failed to create directory for kernel $kernel"; exit 1; }
cp -r ./kernels/"$kernel" ./chroot/tmp/kernel || { echo "Failed to copy source for kernel $kernel"; exit 1; }
cd ./chroot/tmp/kernel || { echo "Failed to enter source directory for kernel $kernel"; exit 1; }
kernel_version="$(file ./out/arch/x86/boot/bzImage | cut -d' ' -f9)"
[ ! "$kernel_version" == "" ] || { echo "Failed to read version for kernel $kernel"; exit 1; }
cp ./out/arch/x86/boot/bzImage ../../home/chronos/rootc/kernel-"$kernel" || { echo "Failed to copy the kernel $kernel"; exit 1; }
make -j"$NTHREADS" O=out INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=../../../home/chronos/kernel modules_install || { echo "Failed to install modules for kernel $kernel"; exit 1; }
rm -f ../../home/chronos/kernel/lib/modules/"$kernel_version"/build || { echo "Failed to remove the build directory for kernel $kernel"; exit 1; }
rm -f ../../home/chronos/kernel/lib/modules/"$kernel_version"/source || { echo "Failed to remove the source directory for kernel $kernel"; exit 1; }
cp -r ./headers ../../home/chronos/kernel/lib/modules/"$kernel_version"/build || { echo "Failed to replace the build directory for kernel $kernel"; exit 1; }
mkdir -p ../../home/chronos/kernel/usr/src || { echo "Failed to create the linux-headers directory for kernel $kernel"; exit 1; }
ln -s /lib/modules/"$kernel_version"/build ../../home/chronos/kernel/usr/src/linux-headers-"$kernel_version" || { echo "Failed to symlink the linux-headers directory for kernel $kernel"; exit 1; }
cd ../../..

if [ "$1" != "skip" ] && [ "$2" != "skip" ]; then

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8188eu ./chroot/tmp/ || { echo "Failed to build external rtl8188eu module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8188eu || { echo "Failed to build external rtl8188eu module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8188eu module for kernel $kernel"; exit 1; }
cp ./8188eu.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8188eu.ko || { echo "Failed to build external rtl8188eu module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8188eu module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8188eu || { echo "Failed to build external rtl8188eu module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8192eu ./chroot/tmp/ || { echo "Failed to build external rtl8192eu module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8192eu || { echo "Failed to build external rtl8192eu module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8192eu module for kernel $kernel"; exit 1; }
cp ./8192eu.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8192eu.ko || { echo "Failed to build external rtl8192eu module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8192eu module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8192eu || { echo "Failed to build external rtl8192eu module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8723bs ./chroot/tmp/ || { echo "Failed to build external rtl8723bs module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8723bs || { echo "Failed to build external rtl8723bs module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8723bs module for kernel $kernel"; exit 1; }
cp ./8723bs.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8723bs.ko || { echo "Failed to build external rtl8723bs module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8723bs module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8723bs || { echo "Failed to build external rtl8723bs module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8723bu ./chroot/tmp/ || { echo "Failed to build external rtl8723bu module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8723bu || { echo "Failed to build external rtl8723bu module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8723bu module for kernel $kernel"; exit 1; }
cp ./8723bu.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8723bu.ko || { echo "Failed to build external rtl8723bu module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8723bu module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8723bu || { echo "Failed to build external rtl8723bu module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8723du ./chroot/tmp/ || { echo "Failed to build external rtl8723du module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8723du || { echo "Failed to build external rtl8723du module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" || { echo "Failed to build external rtl8723du module for kernel $kernel"; exit 1; }
cp ./8723du.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8723du.ko || { echo "Failed to build external rtl8723du module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8723du module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8723du || { echo "Failed to build external rtl8723du module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8812au ./chroot/tmp/ || { echo "Failed to build external rtl8812au module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8812au || { echo "Failed to build external rtl8812au module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8812au module for kernel $kernel"; exit 1; }
cp ./8812au.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8812au.ko || { echo "Failed to build external rtl8812au module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8812au module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8812au || { echo "Failed to build external rtl8812au module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8814au ./chroot/tmp/ || { echo "Failed to build external rtl8814au module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8814au || { echo "Failed to build external rtl8814au module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8814au module for kernel $kernel"; exit 1; }
cp ./8814au.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8814au.ko || { echo "Failed to build external rtl8814au module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8814au module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8814au || { echo "Failed to build external rtl8814au module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8821ce ./chroot/tmp/ || { echo "Failed to build external rtl8821ce module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8821ce || { echo "Failed to build external rtl8821ce module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8821ce module for kernel $kernel"; exit 1; }
cp ./8821ce.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8821ce.ko || { echo "Failed to build external rtl8821ce module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8821ce module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8821ce || { echo "Failed to build external rtl8821ce module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl8821cu ./chroot/tmp/ || { echo "Failed to build external rtl8821cu module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl8821cu || { echo "Failed to build external rtl8821cu module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl8821cu module for kernel $kernel"; exit 1; }
cp ./8821cu.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl8821cu.ko || { echo "Failed to build external rtl8821cu module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8821cu module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl8821cu || { echo "Failed to build external rtl8821cu module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl88x2bu ./chroot/tmp/ || { echo "Failed to build external rtl88x2bu module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl88x2bu || { echo "Failed to build external rtl88x2bu module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" modules || { echo "Failed to build external rtl88x2bu module for kernel $kernel"; exit 1; }
cp ./88x2bu.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl88x2bu.ko || { echo "Failed to build external rtl88x2bu module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl88x2bu module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl88x2bu || { echo "Failed to build external rtl88x2bu module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/rtl885xxx ./chroot/tmp/ || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/rtl885xxx || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
mkdir -p ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw89core.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw89core.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw89pci.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw89pci.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw_8852a.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw_8852a.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw_8852ae.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw_8852ae.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw_8852b.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw_8852b.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw_8852be.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw_8852be.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw_8852c.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw_8852c.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cp ./rtw_8852ce.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/rtl885xxx/rtw_8852ce.ko || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/rtl885xxx || { echo "Failed to build external rtl8852ae module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/broadcom-wl ./chroot/tmp/ || { echo "Failed to build external broadcom-wl module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/broadcom-wl || { echo "Failed to build external broadcom-wl module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" || { echo "Failed to build external broadcom-wl module for kernel $kernel"; exit 1; }
cp ./wl.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/broadcom_wl.ko || { echo "Failed to build external broadcom-wl module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external broadcom-wl module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/broadcom-wl || { echo "Failed to build external broadcom-wl module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/acpi_call ./chroot/tmp/ || { echo "Failed to build external acpi_call module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/acpi_call || { echo "Failed to build external acpi_call module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" || { echo "Failed to build external acpi_call module for kernel $kernel"; exit 1; }
cp ./acpi_call.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/acpi_call.ko || { echo "Failed to build external acpi_call module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external acpi_call module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/acpi_call || { echo "Failed to build external acpi_call module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/ipts ./chroot/tmp/ || { echo "Failed to build external ipts module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/ipts || { echo "Failed to build external ipts module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" || { echo "Failed to build external ipts module for kernel $kernel"; exit 1; }
cp ./src/ipts.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/ipts.ko || { echo "Failed to build external ipts module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external ipts module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/ipts || { echo "Failed to build external ipts module for kernel $kernel"; exit 1; }

fi

if [ "$kernel" == "6.1" ] || [ "$kernel" == "6.6" ]; then

cp -r ./external-drivers/ithc ./chroot/tmp/ || { echo "Failed to build external ithc module for kernel $kernel"; exit 1; }
cd ./chroot/tmp/ithc || { echo "Failed to build external ithc module for kernel $kernel"; exit 1; }
make -j"$NTHREADS" || { echo "Failed to build external ithc module for kernel $kernel"; exit 1; }
cp ./build/ithc.ko ../../../chroot/home/chronos/kernel/lib/modules/"$kernel_version"/ithc.ko || { echo "Failed to build external ithc module for kernel $kernel"; exit 1; }
cd ../../.. || { echo "Failed to build external ithc module for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/ithc || { echo "Failed to build external ithc module for kernel $kernel"; exit 1; }

fi

fi

cd ./chroot/home/chronos/kernel || { echo "Failed to enter directory for kernel $kernel"; exit 1; }
tar zcf ../rootc/packages/kernel-"$kernel_version".tar.gz * --owner=0 --group=0 || { echo "Failed to create archive for kernel $kernel"; exit 1; }
cd ../../../.. || { echo "Failed to cleanup for kernel $kernel"; exit 1; }
rm -r ./chroot/home/chronos/kernel || { echo "Failed to cleanup for kernel $kernel"; exit 1; }
rm -r ./chroot/tmp/kernel || { echo "Failed to cleanup for kernel $kernel"; exit 1; }

done

cd ./chroot/home/chronos || { echo "Failed to switch to chronos directory"; exit 1; }
cp -r ../../../alsa-ucm-conf ./ || { echo "Failed to copy ucm configuration"; exit 1; }
cd ./alsa-ucm-conf || { echo "Failed to switch to ucm configuration directory"; exit 1; }
tar zcf ../rootc/packages/alsa-ucm-conf.tar.gz * --owner=0 --group=0 || { echo "Failed to create ucm configuration archive"; exit 1; }
cd .. || { echo "Failed to cleanup ucm configuration directory"; exit 1; }
rm -r ./alsa-ucm-conf || { echo "Failed to cleanup ucm configuration directory"; exit 1; }

git clone --depth=1 -b main https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git || { echo "Failed to clone the linux firmware git"; exit 1; }
cd ./linux-firmware || { echo "Failed to enter the linux firmware directory"; exit 1; }
make -j"$NTHREADS" DESTDIR=./tmp FIRMWAREDIR=/lib/firmware install || { echo "Failed to install firmwares in temporary directory"; exit 1; }
mv ./tmp/lib/firmware ./out || { echo "Failed to move the firmwares temporary directory"; exit 1; }
rm -rf ./out/bnx2x
rm -rf ./out/dpaa2
rm -rf ./out/liquidio
rm -rf ./out/mellanox
rm -rf ./out/mrvl/prestera
rm -rf ./out/netronome
rm -rf ./out/qcom
rm -rf ./out/qed
rm -rf ./out/ti-connectivity
curl -L https://git.kernel.org/pub/scm/linux/kernel/git/sforshee/wireless-regdb.git/plain/regulatory.db -o ./out/regulatory.db || { echo "Failed to download the regulatory db"; exit 1; }
curl -L https://git.kernel.org/pub/scm/linux/kernel/git/sforshee/wireless-regdb.git/plain/regulatory.db.p7s -o ./out/regulatory.db.p7s || { echo "Failed to download the regulatory db"; exit 1; }
cp -r ../../../../extra-firmwares/* ./out/ || { echo "Failed to copy brunch extra firmware files"; exit 1; }
mkdir -p ../rootc/lib/firmware || { echo "Failed to make firmware directory"; exit 1; }
curl -L https://archlinux.org/packages/core/any/amd-ucode/download/ -o /tmp/amd-ucode.tar.zst || { echo "Failed to download amd ucode"; exit 1; }
tar -C ../rootc/lib/firmware/ -xf /tmp/amd-ucode.tar.zst boot/amd-ucode.img --strip 1 || { echo "Failed to extract amd ucode"; exit 1; }
rm /tmp/amd-ucode.tar.zst || { echo "Failed to cleanup amd ucode"; exit 1; }
curl -L https://archlinux.org/packages/extra/any/intel-ucode/download/ -o /tmp/intel-ucode.tar.zst || { echo "Failed to download intel ucode"; exit 1; }
tar -C ../rootc/lib/firmware/ -xf /tmp/intel-ucode.tar.zst boot/intel-ucode.img --strip 1 || { echo "Failed to extract intel ucode"; exit 1; }
rm /tmp/intel-ucode.tar.zst || { echo "Failed to cleanup intel ucode"; exit 1; }
cd ./out || { echo "Failed to enter the final firmware directory"; exit 1; }
tar zcf ../../rootc/packages/firmwares.tar.gz * --owner=0 --group=0 || { echo "Failed to create the firmwares archive"; exit 1; }
cd ../.. || { echo "Failed to cleanup firmwares directory"; exit 1; }
rm -r ./linux-firmware || { echo "Failed to cleanup firmwares directory"; exit 1; }
cd ../../.. || { echo "Failed to cleanup firmwares directory"; exit 1; }

mount --bind ./out ./chroot/out || { echo "Failed to bind mount output directory in chroot"; exit 1; }
mount -t proc none ./chroot/proc || { echo "Failed to mount proc directory in chroot"; exit 1; }
mount -t sysfs none ./chroot/sys || { echo "Failed to mount sys directory in chroot"; exit 1; }
mount -t devtmpfs none ./chroot/dev || { echo "Failed to mount dev directory in chroot"; exit 1; }
mount -t tmpfs -o mode=1777,nosuid,nodev,strictatime tmpfs ./chroot/dev/shm || { echo "Failed to mount shm directory in chroot"; exit 1; }

cp ./scripts/build-init ./chroot/init || { echo "Failed to copy the chroot init script"; exit 1; }
NTHREADS="$NTHREADS" chroot --userspec=1000:1000 ./chroot /init || { echo "The chroot script failed"; exit 1; }

umount ./chroot/dev/shm || { echo "Failed to umount shm directory from chroot"; exit 1; }
umount ./chroot/dev || { echo "Failed to umount dev directory from chroot"; exit 1; }
umount ./chroot/sys || { echo "Failed to umount sys directory from chroot"; exit 1; }
umount ./chroot/proc || { echo "Failed to umount proc directory from chroot"; exit 1; }
umount ./chroot/out || { echo "Failed to umount output directory from chroot"; exit 1; }
rm -r ./chroot || { echo "Failed the final cleanup"; exit 1; }
