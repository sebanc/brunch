# This patch generates a simple script to facilitate the modification of grub configuration

ret=0
cat >/roota/usr/sbin/edit-grub-config <<EDITGRUB
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi
if [ \$(whoami) != "root" ]; then echo "Please run with this script with sudo"; exit 1; fi

if [ -z "\$EDITOR" ]; then EDITOR=nano; fi

source=\$(rootdev -d)
if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
	partsource="\$source"p
else
	partsource="\$source"
fi

if [[ "\$source" =~ .*"loop".* ]] ; then 
cat <<DUALBOOT
┌──────────────────────────────────────────────────────────────┐
│ edit-grub-config does not work with dual boot installations. │
│ In dual boot, you have to modify the grub config that was    │
│ created at the end of the image file installation process.   │
└──────────────────────────────────────────────────────────────┘
DUALBOOT
else
mkdir -p /root/tmpgrub
mount "\$partsource"12 /root/tmpgrub
\$EDITOR /root/tmpgrub/efi/boot/grub.cfg
umount /root/tmpgrub
fi
EDITGRUB
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /roota/usr/sbin/edit-grub-config
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
