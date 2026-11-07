#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/common.sh"
restore_settings
echo "Restored the six settings saved before this module's first installation."
