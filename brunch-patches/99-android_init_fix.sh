# Starting from ChromeOS R115, android apps break after each ChromeOS update, apply the below ugly hack until a proper fix is found.

ret=0

cat >/roota/etc/init/brunch-update-cleanup.conf <<'UPDATECLEANUP'
start on start-user-session
task
import CHROMEOS_USER

script
        if [ ! -z "$CHROMEOS_USER" ] && [ -f /.brunch_update_done ] && [ -d "$(cryptohome-path system $CHROMEOS_USER)"/android-data/data/misc/installd ]; then
		rm -rf "$(cryptohome-path system $CHROMEOS_USER)"/android-data/data/misc/installd
		rm /.brunch_update_done
	fi
end script
UPDATECLEANUP
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
