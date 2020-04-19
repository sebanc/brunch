ret=0
cat >/system/usr/sbin/extract-recovery <<INSTALL
#!/bin/bash
if ( ! test -z {,} ); then echo "Must be ran with \"bash\""; exit 1; fi

usage()
{
	echo "Usage: extract-recovery -dst <image_path>"
}

blocksize() {
  local path="\$1"
  if [ -b "\${path}" ]; then
    local dev="\${path##*/}"
    local sys="/sys/block/\${dev}/queue/logical_block_size"
    if [ -e "\${sys}" ]; then
      cat "\${sys}"
    else
      local part="\${path##*/}"
      local block
      block="\$(get_block_dev_from_partition_dev "\${path}")"
      block="\${block##*/}"
      cat "/sys/block/\${block}/\${part}/queue/logical_block_size"
    fi
  else
    echo 512
  fi
}

numsectors() {
  local block_size
  local sectors
  local path="\$1"

  if [ -b "\${path}" ]; then
    local dev="\${path##*/}"
    block_size="\$(blocksize "\${path}")"

    if [ -e "/sys/block/\${dev}/size" ]; then
      sectors="\$(cat "/sys/block/\${dev}/size")"
    else
      part="\${path##*/}"
      block="\$(get_block_dev_from_partition_dev "\${path}")"
      block="\${block##*/}"
      sectors="\$(cat "/sys/block/\${block}/\${part}/size")"
    fi
  else
    local bytes
    bytes="\$(stat -c%s "\${path}")"
    local rem=\$(( bytes % 512 ))
    block_size=512
    sectors=\$(( bytes / 512 ))
    if [ "\${rem}" -ne 0 ]; then
      sectors=\$(( sectors + 1 ))
    fi
  fi

  echo \$(( sectors * 512 / block_size ))
}

write_base_table() {
  local target="\$1"
  local blocks
  block_size=\$(blocksize "\${target}")
  numsecs=\$(numsectors "\${target}")
  local curr=32768
  if [ \$(( 0 & (block_size - 1) )) -gt 0 ]; then
    echo "Primary Entry Array padding is not block aligned." >&2
    exit 1
  fi
  cgpt create -p \$(( 0 / block_size )) "\${target}"
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 11 -b \$(( curr / block_size )) -s \${blocks} -t firmware     -l "RWFW" "\${target}"
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 6 -b \$(( curr / block_size )) -s \${blocks} -t kernel     -l "KERN-C" "\${target}"
  : \$(( curr += blocks * block_size ))
  if [ \$(( curr % 4096 )) -gt 0 ]; then
    : \$(( curr += 4096 - curr % 4096 ))
  fi
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 7 -b \$(( curr / block_size )) -s \${blocks} -t rootfs     -l "ROOT-C" "\${target}"
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 9 -b \$(( curr / block_size )) -s \${blocks} -t reserved     -l "reserved" "\${target}"
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 10 -b \$(( curr / block_size )) -s \${blocks} -t reserved     -l "reserved" "\${target}"
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 2 -b \$(( curr / block_size )) -s \${blocks} -t kernel     -l "KERN-A" "\${target}"
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 33554432 / block_size ))
  if [ \$(( 33554432 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 4 -b \$(( curr / block_size )) -s \${blocks} -t kernel     -l "KERN-B" "\${target}"
  : \$(( curr += blocks * block_size ))
  if [ \$(( curr % 4096 )) -gt 0 ]; then
    : \$(( curr += 4096 - curr % 4096 ))
  fi
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 8 -b \$(( curr / block_size )) -s \${blocks} -t data     -l "OEM" "\${target}"
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  : \$(( curr += blocks * block_size ))
  blocks=\$(( 33554432 / block_size ))
  if [ \$(( 33554432 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 12 -b \$(( curr / block_size )) -s \${blocks} -t efi     -l "EFI-SYSTEM" "\${target}"
  : \$(( curr += blocks * block_size ))
  if [ \$(( curr % 4096 )) -gt 0 ]; then
    : \$(( curr += 4096 - curr % 4096 ))
  fi
  blocks=\$(( 1 / block_size ))
  if [ \$(( 1 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 5 -b \$(( curr / block_size )) -s \${blocks} -t rootfs     -l "ROOT-B" "\${target}"
  : \$(( curr += blocks * block_size ))
  if [ \$(( curr % 4096 )) -gt 0 ]; then
    : \$(( curr += 4096 - curr % 4096 ))
  fi
  blocks=\$(( 4294967296 / block_size ))
  if [ \$(( 4294967296 % block_size )) -gt 0 ]; then
     : \$(( blocks += 1 ))
  fi
  cgpt add -i 3 -b \$(( curr / block_size )) -s \${blocks} -t rootfs     -l "ROOT-A" "\${target}"
  : \$(( curr += blocks * block_size ))
  if [ \$(( curr % 4096 )) -gt 0 ]; then
    : \$(( curr += 4096 - curr % 4096 ))
  fi
  blocks=\$(( numsecs - (curr + 24576) / block_size ))
  cgpt add -i 1 -b \$(( curr / block_size )) -s \${blocks} -t data     -l "STATE" "\${target}"
  cgpt add -i 2 -S 0 -T 15 -P 15 "\${target}"
  cgpt add -i 4 -S 0 -T 15 -P 0 "\${target}"
  cgpt add -i 6 -S 0 -T 15 -P 0 "\${target}"
  cgpt boot -p -i 12 "\${target}"
  cgpt add -i 12 -B 0 "\${target}"
  cgpt show "\${target}"
}

source=\$(rootdev -d)
image_size=5
while [ \$# -gt 0 ]; do
	case "\$1" in
		-dst | --destination)
		shift
		destination="\$1"
		;;
		-h | --help)
		usage
		 ;;
		*)
		echo "\$1 argument is not valid"
		usage
		exit 1
	esac
	shift
done
if [ -z "\$destination" ]; then
	echo "At least the output parameter should be provided."
	usage
	exit 1
fi

if [ -f "\$destination" ]; then rm "\$destination"; fi
if [ \$(( (\$(df -k --output=avail "\${destination%/*}" | sed 1d) - ( \$image_size * 1024 * 1024)) )) -lt 0 ]; then echo "Not enought space to create image file"; exit 1; fi
echo "Creating image file"
dd if=/dev/zero of="\$destination" bs=1G seek=\$image_size count=0
if [ ! "\$?" -eq 0 ]; then echo "Could not write image here, try with sudo ?"; rm "\$destination"; exit 1; fi
write_base_table "\$destination"
sync
if (expr match "\$source" ".*[0-9]\$" >/dev/null); then
	partsource="\$source"p
else
	partsource="\$source"
fi
loopdevice=\$(losetup --show -fP "\$destination")
sleep 5
pv "\$partsource"4 > "\$loopdevice"p4
pv "\$partsource"5 > "\$loopdevice"p3
pv "\$partsource"12 > "\$loopdevice"p12
losetup -d "\$loopdevice"
echo "recovery created."

INSTALL
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
chmod 0755 /system/usr/sbin/extract-recovery
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
exit $ret
