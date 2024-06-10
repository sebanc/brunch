# This patch adjusts the "cap_bpf" in patchpanel service according to the selected kernel (cap_bpf is only available on 5.10+ kernels).

if [ -f /roota/etc/init/patchpanel.conf ]; then
	if [ "$(cat /proc/cmdline | cut -d' ' -f1 | cut -d'/' -f2)" == "chromebook-4.19" ] || [ "$(cat /proc/cmdline | cut -d' ' -f1 | cut -d'/' -f2)" == "chromebook-5.4" ]; then
		sed -i 's@caps="${caps},cap_bpf"@# caps="${caps},cap_bpf"@g' /roota/etc/init/patchpanel.conf
	else
		sed -i 's@# caps="${caps},cap_bpf"@caps="${caps},cap_bpf"@g' /roota/etc/init/patchpanel.conf
	fi
fi

