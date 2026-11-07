ui_print "Redmi 12 Performance: compatibility checks"
[ "$BOOTMODE" = true ] || abort "Install from the Magisk app while Android is running."
[ "$ARCH" = arm64 ] || abort "arm64 required."
[ "${MAGISK_VER_CODE:-0}" -ge 31100 ] || abort "Magisk 31.1 or newer required."
. "$MODPATH/common.sh"
compatible || abort "Unsupported device/ROM binaries. This ZIP targets the tested heat crDroid 12.12 build only."
ZYGISK=$(magisk --sqlite "SELECT value FROM settings WHERE key='zygisk'" 2>/dev/null)
[ "$ZYGISK" = value=1 ] || abort "Enable Zygisk in Magisk settings, reboot, then install."
CURRENT_USER=$(am get-current-user)
case "$CURRENT_USER" in ''|*[!0-9]*) abort "Cannot identify Android user." ;; esac
[ "$CURRENT_USER" = 0 ] || abort "Install from the owner profile (user 0)."
wm size | grep -q 'Physical size: 1080x2460' || abort "Unexpected physical display size."
wm density | grep -q 'Physical density: 440' || abort "Unexpected physical display density."
wm size | grep -q '^Override' && abort "Reset the resolution override before installing."
wm density | grep -q '^Override' && abort "Reset the density override before installing."
ui_print "Includes native resolution / 90 Hz, icon cache, radio retry guard and PowerHAL query fix."
ui_print "Fingerprint unlock is disabled; PIN/password remains available."
PREV=/data/adb/modules/redmi12_performance
mkdir -p "$MODPATH/state"
if [ -d "$PREV/state" ]; then cp -af "$PREV/state/." "$MODPATH/state/"; fi
if [ ! -f "$MODPATH/state/settings.tsv" ]; then
  for key in disable_window_blurs window_animation_scale transition_animation_scale animator_duration_scale; do
    printf 'global\t%s\t0\t%s\n' "$key" "$(settings get global "$key")"
  done > "$MODPATH/state/settings.tsv"
  for key in min_refresh_rate peak_refresh_rate; do
    printf 'system\t%s\t0\t%s\n' "$key" "$(settings --user 0 get system "$key")"
  done >> "$MODPATH/state/settings.tsv"
fi
# Finish all abort-capable checks before retiring old components.
for old in redmi12_responsiveness redmi12_iconcache redmi12_radiostartup redmi12_radiostartup_v2 redmi12_powerquery; do
  for base in /data/adb/modules /data/adb/modules_update; do
    [ -d "$base/$old" ] || continue
    if [ ! -f "$base/$old/disable" ]; then
      touch "$base/$old/disable"
      printf '%s\n' "$base/$old" >> "$MODPATH/state/migrated.txt"
    fi
  done
done
set_perm_recursive "$MODPATH" 0 0 0755 0644
for script in customize.sh common.sh post-fs-data.sh service.sh action.sh uninstall.sh compile-apps.sh status.sh restore-settings.sh; do
  set_perm "$MODPATH/$script" 0 0 0755
done
set_perm_recursive "$MODPATH/state" 0 0 0700 0600
ui_print "Ready. Reboot to activate. Previous components have been disabled to avoid duplicate hooks."
