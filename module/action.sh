#!/system/bin/sh
MODDIR=${0%/*}
sh "$MODDIR/status.sh"
echo "Checking one-time ART compilation. Existing speed artifacts are retained."
sh "$MODDIR/compile-apps.sh"
