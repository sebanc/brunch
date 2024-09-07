# This patch creates a startup service for the software TPM

ret=0

cat >/roota/etc/init/tpm2-simulator.conf <<'SWTPM'
stop on stopping boot-services and stopped trunksd

# This daemon should very unlikely to be killed by the OOM killer otherwise
# the other TPM related daemons(trunksd/chapsd/cryptohomed...) may crash.
oom score -100

respawn

pre-start script
  mkdir -p -m 755 /mnt/stateful_partition/brunch/swtpm
end script

expect stop

exec /usr/bin/swtpm chardev --daemon --vtpm-proxy --tpm2 --tpmstate dir=/mnt/stateful_partition/brunch/swtpm --ctrl type=tcp,port=10001 --flags not-need-init >> allout.txt 2>&1

post-start exec bash -c "until [ -c /dev/tpm0 ]; do sleep 1; done"

# Add timeout and sync the disk to prevent blocking the shutdown.
pre-stop script
sync
end script

post-stop script
# Kill the remaining simulator processes.
pgrep swtpm | xargs -r kill
# Sync the disk to prevent losing data.
sync
end script
SWTPM
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
