# This patch creates a startup service for the software TPM

ret=0

cat >/roota/etc/init/tpm2-simulator.conf <<'SWTPM'
stop on stopping boot-services and stopped trunksd

oom score -100

respawn

pre-start script
  mkdir -p -m 755 /mnt/stateful_partition/brunch/swtpm
end script

expect fork

script
  swtpm chardev --daemon --vtpm-proxy --tpm2 --tpmstate dir=/mnt/stateful_partition/brunch/swtpm --ctrl type=tcp,port=10001 --flags not-need-init
  until [ -c /dev/tpm0 ]; do sleep 1; done
end script

post-stop script
  pgrep swtpm | xargs -r kill
  sync
end script
SWTPM
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

exit $ret
