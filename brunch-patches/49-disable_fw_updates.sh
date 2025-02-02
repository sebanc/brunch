# Disable chromebooks firmware writing mechanisms to avoid the risk (very low) that ChromeOS would flash the firmware of an unsupported device

ret=0

echo -e '#!/bin/bash\nexit 0' > /roota/usr/sbin/chromeos-firmwareupdate
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
echo -e '#!/bin/bash\nexit 0' > /roota/usr/sbin/chromeos-setgoodfirmware
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
echo -e '#!/bin/bash\nexit 0' > /roota/usr/sbin/chromeos-setgoodkernel
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
echo -e '#!/bin/bash\nexit 0' > /roota/usr/sbin/flashrom
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
echo -e '#!/bin/bash\nexit 0' > /roota/usr/sbin/gsctool
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 4))); fi

rm -rf /roota/opt/google/touch /roota/opt/google/modemfwd*

exit $ret
