# This patch generates an helper script for the brunch PWA

pwa=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "pwa" ]; then pwa=1; fi
done

ret=0
if [ "$pwa" -eq 1 ]; then
	echo "brunch: $0 pwa enabled" > /dev/kmsg
	cat >/roota/etc/init/pwa-helper.conf <<INSMOD
start on stopped udev-trigger

script
	websocketd --port=8080 --origin=https://sebanc.github.io,https://itesaurabh.github.io pwa-helper
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/usr/sbin/pwa-helper <<'PWA'
#!/bin/bash
while read line
do
	case "$line" in
		"brunch-version")
			echo -e "$line":next:"$(cat /etc/brunch_version)"
		;;
		"latest-stable")
			echo -e "$line":next:"$(curl -L -s "https://api.github.com/repos/sebanc/brunch/releases/latest" | grep '"name":' | head -1 | cut -d '"' -f 4)"
		;;
		"update-stable")
            if [ $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted | sed 1d) / 1024 / 1024) - 4 )) -lt 0 ]; then echo "Not enough space to update Brunch, 4Gb of free disk space is required but only $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted | sed 1d) / 1024 / 1024) ))Gb available."; continue; fi
			echo -e 'Do not turn off your PC while the update is in progress.'
			if [ -d /mnt/stateful_partition/unencrypted/brunch_pwa ]; then rm -r /mnt/stateful_partition/unencrypted/brunch_pwa; fi
			mkdir /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e "Downloading latest brunch stable."
			curl -L -s -o /mnt/stateful_partition/unencrypted/brunch_pwa/brunch_update.tar.gz $(curl -L -s "https://api.github.com/repos/sebanc/brunch/releases/latest" | grep browser_download_url | tr -d '"' | sed 's#browser_download_url: ##g')
			chromeos-update -f /mnt/stateful_partition/unencrypted/brunch_pwa/brunch_update.tar.gz
			rm -r /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e '<a href=javascript:reboot(); style=color:#ffffff;>Click here</a> to reboot your computer and finish the update.'
		;;
		"latest-unstable")
			echo -e "$line":next:"$(curl -L -s "https://api.github.com/repos/sebanc/brunch-unstable/releases/latest" | grep '"name":' | head -1 | cut -d '"' -f 4)"
		;;
		"update-unstable")
            if [ $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted | sed 1d) / 1024 / 1024) - 4 )) -lt 0 ]; then echo "Not enough space to update Brunch, 4Gb of free disk space is required but only $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted | sed 1d) / 1024 / 1024) ))Gb available."; continue; fi
			echo -e 'Do not turn off your PC while the update is in progress.'
			if [ -d /mnt/stateful_partition/unencrypted/brunch_pwa ]; then rm -r /mnt/stateful_partition/unencrypted/brunch_pwa; fi
			mkdir /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e "Downloading latest brunch unstable."
			curl -L -s -o /mnt/stateful_partition/unencrypted/brunch_pwa/brunch_update.tar.gz $(curl -L -s "https://api.github.com/repos/sebanc/brunch-unstable/releases/latest" | grep browser_download_url | tr -d '"' | sed 's#browser_download_url: ##g')
			chromeos-update -f /mnt/stateful_partition/unencrypted/brunch_pwa/brunch_update.tar.gz
			rm -r /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e '<a href=javascript:reboot(); style=color:#ffffff;>Click here</a> to reboot your computer and finish the update.'
		;;
		"chromeos-version")
			echo -e "$line":next:"$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'=' -f2 | cut -d'-' -f1) $(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'/' -f2 | cut -d'-' -f1)"
		;;
		"latest-chromeos")
			recovery="$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BOARD | cut -d'=' -f2 | cut -d'-' -f1)"
			if [ "$recovery" == "reven" ]; then
				recovery_file_url="https://dl.google.com/dl/edgedl/chromeos/recovery/cloudready_recovery.conf"
			else
				recovery_file_url="https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf"
			fi
			version="R$(curl -L -s https://chromium.googlesource.com/chromiumos/third_party/kernel/+refs | tr '>' '\n' | grep "^release" | grep "$(curl -L -s $recovery_file_url | grep file= | cut -d'=' -f2| grep "$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'=' -f2 | cut -d'-' -f1)" | cut -d'_' -f2 | cut -d'.' -f1)" | cut -d'-' -f2 | cut -c2- | sort -n | tail -1)"
			echo -e "$line":next:"$recovery $version"
		;;
		"update-chromeos")
			if [ $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted | sed 1d) / 1024 / 1024) - 8 )) -lt 0 ]; then echo "Not enough space to update ChromeOS, 8Gb of free disk space is required but only $(( ($(df -k --output=avail /mnt/stateful_partition/unencrypted | sed 1d) / 1024 / 1024) ))Gb available."; continue; fi
			echo -e 'Do not turn off your PC while the update is in progress.'
			if [ -d /mnt/stateful_partition/unencrypted/brunch_pwa ]; then rm -r /mnt/stateful_partition/unencrypted/brunch_pwa; fi
			mkdir /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e "Downloading latest chromeos recovery image."
			recovery="$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BOARD | cut -d'=' -f2 | cut -d'-' -f1)"
			if [ "$recovery" == "reven" ]; then
				recovery_file_url="https://dl.google.com/dl/edgedl/chromeos/recovery/cloudready_recovery.conf"
			else
				recovery_file_url="https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf"
			fi
			curl -L -s -o /mnt/stateful_partition/unencrypted/brunch_pwa/recovery_image.zip $(curl -L -s $recovery_file_url | grep .bin.zip | cut -d'=' -f2 | grep $(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BOARD | cut -d'=' -f2 | cut -d'-' -f1) | sort -n | tail -1)
			bsdtar -xf /mnt/stateful_partition/unencrypted/brunch_pwa/recovery_image.zip -C /mnt/stateful_partition/unencrypted/brunch_pwa
			rm /mnt/stateful_partition/unencrypted/brunch_pwa/recovery_image.zip
			recovery=$(find /mnt/stateful_partition/unencrypted/brunch_pwa -type f -name "*.bin" | tail -1)
			chromeos-update -r "$recovery"
			rm -r /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e '<a href=javascript:reboot(); style=color:#ffffff;>Click here</a> to reboot your computer and finish the update.'
		;;
		"install-toolchain")
			if [ -d /mnt/stateful_partition/unencrypted/brunch_pwa ]; then rm -r /mnt/stateful_partition/unencrypted/brunch_pwa; fi
			echo -e "Downloading the latest brunch-toolchain."
			mkdir /mnt/stateful_partition/unencrypted/brunch_pwa
			curl -L -s -o /mnt/stateful_partition/unencrypted/brunch_pwa/brunch_toolchain.tar.gz $(curl -L -s "https://api.github.com/repos/sebanc/brunch-toolchain/releases/latest" | grep browser_download_url | tr -d '"' | sed 's#browser_download_url: ##g')
			rm -r /usr/local/*
			chown -R 1000:1000 /usr/local
			tar zxf /mnt/stateful_partition/unencrypted/brunch_pwa/brunch_toolchain.tar.gz -C /usr/local
			echo -e "Downloading the latest brioche."
			curl -l https://raw.githubusercontent.com/sebanc/brioche/main/brioche -o /mnt/stateful_partition/unencrypted/brunch_pwa/brioche
			install -Dt /usr/local/bin -m 755 -g 1000 -o 1000 /mnt/stateful_partition/unencrypted/brunch_pwa/brioche
			rm -r /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e "Brunch-toolchain and brioche installed."
		;;
		"install-toolkit")
			if [ -d /mnt/stateful_partition/unencrypted/brunch_pwa ]; then rm -r /mnt/stateful_partition/unencrypted/brunch_pwa; fi
			echo -e "Downloading the latest brunch-toolkit."
			mkdir /mnt/stateful_partition/unencrypted/brunch_pwa
			curl -l https://raw.githubusercontent.com/WesBosch/brunch-toolkit/main/brunch-toolkit -o /mnt/stateful_partition/unencrypted/brunch_pwa/brunch-toolkit
			install -Dt /usr/local/bin -m 755 -g 1000 -o 1000 /mnt/stateful_partition/unencrypted/brunch_pwa/brunch-toolkit
			rm -r /mnt/stateful_partition/unencrypted/brunch_pwa
			echo -e "Brunch-toolkit installed."
		;;
		"reboot")
			reboot
		;;
		*)
		;;
	esac
done < "${1:-/dev/stdin}"
PWA
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	chmod 0755 /roota/usr/sbin/pwa-helper
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi
exit $ret
