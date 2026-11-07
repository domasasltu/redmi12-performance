#!/system/bin/sh
umask 077
MODDIR=${0%/*}
rm -f "$MODDIR/icon.armed" "$MODDIR/radio.armed"
. "$MODDIR/common.sh"
compatible || { touch "$MODDIR/disable"; exit 0; }
# A boot that never reached service completion leaves this marker. Skip new hooks
# and disable the package on the next boot instead of repeating the failed trial.
if [ -f "$MODDIR/state/boot-pending" ]; then
  touch "$MODDIR/disable"
  printf 'Previous boot was incomplete; module disabled. Reboot to clear mounts/policy.\n' > "$MODDIR/state/fallback.txt"
  exit 0
fi
mkdir -p "$MODDIR/state"
rmdir "$MODDIR/state/art-lock" 2>/dev/null
cat /proc/sys/kernel/random/boot_id > "$MODDIR/state/boot-pending"
# The root companion atomically consumes each token. Process restarts pass through.
printf 'one SystemUI attempt\n' > "$MODDIR/icon.armed"
printf 'one Phone attempt\n' > "$MODDIR/radio.armed"
