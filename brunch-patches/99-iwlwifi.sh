# 11ax support is unstable on iwlwifi driver with AX101 and AX210 cards, add an option to disable it.

iwlwifi_disable11ax=0
for i in $(echo "$1" | sed 's#,# #g')
do
	if [ "$i" == "iwlwifi_disable11ax" ]; then
		iwlwifi_disable11ax=1
	fi
done

ret=0

if [ "$iwlwifi_disable11ax" -eq 1 ]; then
	cat >/roota/etc/modprobe.d/iwlwifi_disable11ax.conf <<MODPROBE_IWLWIFI
options iwlwifi disable_11ax=1
MODPROBE_IWLWIFI
	if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi
fi

exit $ret
