#!/system/bin/sh
MODDIR=${0%/*}
. "$MODDIR/common.sh"
restore_settings
# Properties, mounts, sysfs changes and SELinux grants disappear on reboot.
# Legacy modules remain disabled, avoiding silently reintroducing duplicate hooks.
