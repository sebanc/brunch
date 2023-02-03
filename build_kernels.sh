#!/bin/bash

if [ ! -d /home/runner/work ]; then NTHREADS=$(nproc); else NTHREADS=$(($(nproc)*4)); fi

if [ -z "$1" ]; then
kernels=$(ls -d ./kernels/* | sed 's#./kernels/##g')
else
kernels="$1"
fi

for kernel in $kernels; do
	echo "Building kernel $kernel"
	KCONFIG_NOTIMESTAMP=1 KBUILD_BUILD_TIMESTAMP='' KBUILD_BUILD_USER=chronos KBUILD_BUILD_HOST=localhost make -C "./kernels/$kernel" -j"$NTHREADS" O=out || { echo "Kernel build failed"; exit 1; }
	rm -f "./kernels/$kernel/out/source"
	if [ -f /persist/keys/brunch.priv ] && [ -f /persist/keys/brunch.pem ]; then
		echo "Signing kernel $kernel"
		mv "./kernels/$kernel/out/arch/x86/boot/bzImage" "./kernels/$kernel/out/arch/x86/boot/bzImage.unsigned" || { echo "Kernel signing failed"; exit 1; }
		sbsign --key /persist/keys/brunch.priv --cert /persist/keys/brunch.pem "./kernels/$kernel/out/arch/x86/boot/bzImage.unsigned" --output "./kernels/$kernel/out/arch/x86/boot/bzImage" || { echo "Kernel signing failed"; exit 1; }
	fi
	echo "Including kernel $kernel headers"
	srctree="./kernels/$kernel"
	objtree="./kernels/$kernel/out"
	SRCARCH="x86"
	KCONFIG_CONFIG="$objtree/.config"
	destdir="$srctree/headers"
	(cd $srctree; find . -name Makefile\* -o -name Kconfig\* -o -name \*.pl) > "$objtree/hdrsrcfiles"
	(cd $srctree; find arch/*/include include scripts -type f -o -type l) >> "$objtree/hdrsrcfiles"
	(cd $srctree; find arch/$SRCARCH -name module.lds -o -name Kbuild.platforms -o -name Platform) >> "$objtree/hdrsrcfiles"
	(cd $srctree; find $(find arch/$SRCARCH -name include -o -name scripts -type d) -type f) >> "$objtree/hdrsrcfiles"
	if grep -q '^CONFIG_STACK_VALIDATION=y' $KCONFIG_CONFIG ; then
		(cd $objtree; find tools/objtool -type f -executable) >> "$objtree/hdrobjfiles"
	fi
	(cd $objtree; find arch/$SRCARCH/include Module.symvers include scripts -type f) >> "$objtree/hdrobjfiles"
	if grep -q '^CONFIG_GCC_PLUGINS=y' $KCONFIG_CONFIG ; then
		(cd $objtree; find scripts/gcc-plugins -name \*.so -o -name gcc-common.h) >> "$objtree/hdrobjfiles"
	fi
	mkdir -p "$destdir"
	(cd $srctree; tar -c -f - -T -) < "$objtree/hdrsrcfiles" | (cd $destdir; tar -xf -)
	(cd $objtree; tar -c -f - -T -) < "$objtree/hdrobjfiles" | (cd $destdir; tar -xf -)
	cp $objtree/.config $destdir/.config
done

