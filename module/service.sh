#!/system/bin/sh
umask 077
MODDIR=${0%/*}
. "$MODDIR/common.sh"
[ ! -f "$MODDIR/disable" ] || exit 0
compatible || exit 0
tries=0
while [ "$(getprop sys.boot_completed)" != 1 ]; do
  tries=$((tries + 1))
  [ "$tries" -lt 120 ] || exit 0
  sleep 2
done
settings --user 0 put system min_refresh_rate 90.0
settings --user 0 put system peak_refresh_rate 90.0
settings put global disable_window_blurs 1
settings put global window_animation_scale 0.5
settings put global transition_animation_scale 0.5
settings put global animator_duration_scale 0.5
# Rendering remains at physical size/density; never apply a smaller framebuffer.
# Installing is restricted to the tested native-size device; do not override wm.
stop vendor.fps_hal
stop micharge-hal
[ -w /sys/module/damon_reclaim/parameters/enabled ] && echo N > /sys/module/damon_reclaim/parameters/enabled
if [ "$(cat /proc/touch_boost/enable 2>/dev/null)" = 1 ] &&
   [ "$(cat /proc/touch_boost/boost_duration 2>/dev/null)" = 100 ] &&
   [ -w /proc/touch_boost/boost_opp_cluster_0 ] &&
   [ -w /proc/touch_boost/boost_opp_cluster_1 ]; then
  echo 0 > /proc/touch_boost/boost_opp_cluster_0
  echo 0 > /proc/touch_boost/boost_opp_cluster_1
fi
rm -f "$MODDIR/state/boot-pending"
cat /proc/sys/kernel/random/boot_id > "$MODDIR/state/last-boot-completed"
# Finite, once per installation; no resident polling/boost daemon.
sleep 30
[ -f "$MODDIR/disable" ] || sh "$MODDIR/compile-apps.sh"
