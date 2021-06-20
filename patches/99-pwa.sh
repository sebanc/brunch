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
	websocketd --port=8080 --origin=https://sebanc.github.io pwa-helper
end script
INSMOD
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
	cat >/roota/usr/sbin/pwa-helper <<PWA
#!/bin/bash
while read line
do
	case "\$line" in
		"brunch-version")
			echo -e "\$line":next:"\$(cat /etc/brunch_version)"
		;;
		"latest-stable")
			echo -e "\$line":next:"\$(curl -L -s "https://api.github.com/repos/sebanc/brunch/releases/latest" | grep '"name":' | head -1 | cut -d '"' -f 4)"
		;;
		"update-stable")
			echo -e '<p style="color:#33266e;">Do not turn off your PC while the update is in progress.<br><br>---Log start---</p>'
			if [ -d /home/chronos/user/Downloads/brunch-pwa ]; then rm -r /home/chronos/user/Downloads/brunch-pwa; fi
			mkdir /home/chronos/user/Downloads/brunch-pwa
			echo -e "Downloading latest brunch stable."
			curl -L -s -o /home/chronos/user/Downloads/brunch-pwa/brunch_update.tar.gz \$(curl -L -s "https://api.github.com/repos/sebanc/brunch/releases/latest" | grep browser_download_url | tr -d '"' | sed 's#browser_download_url: ##g')
			chromeos-update -f /home/chronos/user/Downloads/brunch-pwa/brunch_update.tar.gz
			rm -r /home/chronos/user/Downloads/brunch-pwa
			echo -e '<p style="color:#33266e;">---Log end---<br><br>The update process is finished:<br>- If you see error messages in the above log, do not turn off your computer and manually update brunch according to the instructions on github.<br>- Otherwise <a href=javascript:reboot();>click here</a> to reboot your computer and finish the update.</p>'
		;;
		"latest-unstable")
			echo -e "\$line":next:"\$(curl -L -s "https://api.github.com/repos/sebanc/brunch-unstable/releases/latest" | grep '"name":' | head -1 | cut -d '"' -f 4)"
		;;
		"update-unstable")
			echo -e '<p style="color:#33266e;">Do not turn off your PC while the update is in progress.<br><br>---Log start---</p>'
			if [ -d /home/chronos/user/Downloads/brunch-pwa ]; then rm -r /home/chronos/user/Downloads/brunch-pwa; fi
			mkdir /home/chronos/user/Downloads/brunch-pwa
			echo -e "Downloading latest brunch unstable."
			curl -L -s -o /home/chronos/user/Downloads/brunch-pwa/brunch_update.tar.gz \$(curl -L -s "https://api.github.com/repos/sebanc/brunch-unstable/releases/latest" | grep browser_download_url | tr -d '"' | sed 's#browser_download_url: ##g')
			chromeos-update -f /home/chronos/user/Downloads/brunch-pwa/brunch_update.tar.gz
			rm -r /home/chronos/user/Downloads/brunch-pwa
			echo -e '<p style="color:#33266e;">---Log end---<br><br>The update process is finished:<br>- If you see error messages in the above log, do not turn off your computer and manually update brunch according to the instructions on github.<br>- Otherwise <a href=javascript:reboot();>click here</a> to reboot your computer and finish the update.</p>'
		;;
		"chromeos-version")
			echo -e "\$line":next:"\$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'=' -f2 | cut -d'-' -f1) \$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'/' -f2 | cut -d'-' -f1)"
		;;
		"latest-chromeos")
			version="\$(curl -L -s https://chromium.googlesource.com/chromiumos/third_party/kernel/+refs | tr '>' '\n' | grep "^release" | grep "\$(curl -L -s https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf | grep file= | cut -d'=' -f2| grep "\$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'=' -f2 | cut -d'-' -f1)" | cut -d'_' -f2 | cut -d'.' -f1)" | cut -d'-' -f2 | tail -1)"
			if [ ! "\$version" == "" ]; then
				recovery="\$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BUILDER_PATH | cut -d'=' -f2 | cut -d'-' -f1) "
			fi
			echo -e "\$line":next:"\$recovery\$version"
		;;
		"update-chromeos")
			echo -e '<p style="color:#33266e;">Do not turn off your PC while the update is in progress.<br><br>---Log start---</p>'
			if [ -d /home/chronos/user/Downloads/brunch-pwa ]; then rm -r /home/chronos/user/Downloads/brunch-pwa; fi
			mkdir /home/chronos/user/Downloads/brunch-pwa
			echo -e "Downloading latest chromeos recovery image."
			curl -L -s -o /home/chronos/user/Downloads/brunch-pwa/recovery_image.zip \$(curl -L -s https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf | grep .bin.zip | cut -d'=' -f2 | grep \$(cat /etc/lsb-release | grep CHROMEOS_RELEASE_BOARD | cut -d'=' -f2 | cut -d'-' -f1) | tail -1)
			bsdtar -xf /home/chronos/user/Downloads/brunch-pwa/recovery_image.zip -C /home/chronos/user/Downloads/brunch-pwa
			rm /home/chronos/user/Downloads/brunch-pwa/recovery_image.zip
			recovery=\$(find /home/chronos/user/Downloads/brunch-pwa -type f -name "*.bin" | tail -1)
			chromeos-update -r "\$recovery"
			rm -r /home/chronos/user/Downloads/brunch-pwa
			echo -e '<p style="color:#33266e;">---Log end---<br><br>The update process is finished:<br>- If you see error messages in the above log, do not turn off your computer and manually update ChromeOS according to the instructions on github.<br>- Otherwise <a href=javascript:reboot();>click here</a> to reboot your computer and finish the update.</p>'
		;;
		"install-toolchain")
			echo -e '<p style="color:#33266e;">---Log start---</p>'
			if [ -d /home/chronos/user/Downloads/brunch-pwa ]; then rm -r /home/chronos/user/Downloads/brunch-pwa; fi
			echo -e "Downloading the latest brunch-toolchain."
			mkdir /home/chronos/user/Downloads/brunch-pwa
			curl -L -s -o /home/chronos/user/Downloads/brunch-pwa/brunch_toolchain.tar.gz \$(curl -L -s "https://api.github.com/repos/sebanc/brunch-toolchain/releases/latest" | grep browser_download_url | tr -d '"' | sed 's#browser_download_url: ##g')
			rm -r /usr/local/*
			chown -R 1000:1000 /usr/local
			tar zxf /home/chronos/user/Downloads/brunch-pwa/brunch_toolchain.tar.gz -C /usr/local
			echo -e "Downloading the latest brioche."
			curl -l https://raw.githubusercontent.com/sebanc/brioche/main/brioche -o /home/chronos/user/Downloads/brunch-pwa/brioche
			install -Dt /usr/local/bin -m 755 -g 1000 -o 1000 /home/chronos/user/Downloads/brunch-pwa/brioche
			rm -r /home/chronos/user/Downloads/brunch-pwa
			echo -e "Brunch-toolchain and brioche installed."
			echo -e '<p style="color:#33266e;">---Log end---</p>'
		;;
		"install-toolkit")
			echo -e '<p style="color:#33266e;">---Log start---</p>'
			if [ -d /home/chronos/user/Downloads/brunch-pwa ]; then rm -r /home/chronos/user/Downloads/brunch-pwa; fi
			echo -e "Downloading the latest brunch-toolkit."
			mkdir /home/chronos/user/Downloads/brunch-pwa
			curl -l https://raw.githubusercontent.com/WesBosch/brunch-toolkit/main/brunch-toolkit -o /home/chronos/user/Downloads/brunch-pwa/brunch-toolkit
			install -Dt /usr/local/bin -m 755 -g 1000 -o 1000 /home/chronos/user/Downloads/brunch-pwa/brunch-toolkit
			rm -r /home/chronos/user/Downloads/brunch-pwa
			echo -e "Brunch-toolkit installed."
			echo -e '<p style="color:#33266e;">---Log end---</p>'
		;;
		"reboot")
			reboot
		;;
		*)
		;;
	esac
done < "\${1:-/dev/stdin}"
PWA
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi
	chmod 0755 /roota/usr/sbin/pwa-helper
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi
fi
exit $ret
