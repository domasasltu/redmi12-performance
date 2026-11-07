#!/system/bin/sh
umask 077
MODDIR=${0%/*}
[ ! -f "$MODDIR/disable" ] || exit 0
[ ! -f "$MODDIR/state/art-complete" ] || exit 0
mkdir "$MODDIR/state/art-lock" 2>/dev/null || exit 0
trap 'rmdir "$MODDIR/state/art-lock" 2>/dev/null' EXIT
all_ok=1
for package in com.android.systemui com.android.launcher3 com.android.settings com.android.calculator2 com.android.deskclock com.android.documentsui com.android.axion.sandbox com.android.contacts com.android.dialer com.android.messaging org.lineageos.aperture org.lineageos.etar org.lineageos.glimpse org.lineageos.recorder org.lineageos.twelve org.omnirom.logcat org.omnirom.omnijaws; do
  [ ! -f "$MODDIR/disable" ] || exit 0
  pm path "$package" >/dev/null 2>&1 || continue
  if pm art dump "$package" 2>/dev/null | grep -q '\[status=speed\]'; then
    printf '%s: already speed\n' "$package" >> "$MODDIR/state/art.log"
    continue
  fi
  if cmd package compile -m speed "$package" >> "$MODDIR/state/art.log" 2>&1; then
    printf '%s: speed compiled\n' "$package" >> "$MODDIR/state/art.log"
  else
    all_ok=0
    printf '%s: compilation failed\n' "$package" >> "$MODDIR/state/art.log"
  fi
done
[ "$all_ok" = 1 ] && touch "$MODDIR/state/art-complete"
