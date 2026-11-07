#!/system/bin/sh
MODDIR=${0%/*}
echo "Redmi 12 Performance 1.0.0"
echo "Device: $(getprop ro.product.device); build: $(getprop ro.build.version.incremental)"
echo "SELinux: $(getenforce); compositor: $(getprop debug.renderengine.backend)"
wm size
wm density
echo "Refresh request: $(settings --user 0 get system min_refresh_rate) / $(settings --user 0 get system peak_refresh_rate) Hz"
echo "Blur disabled: $(settings get global disable_window_blurs)"
echo "DAMON reclaim: $(cat /sys/module/damon_reclaim/parameters/enabled 2>/dev/null)"
echo "SystemUI: $(pidof com.android.systemui); Phone: $(pidof com.android.phone)"
echo "Hook attempts (present token means target has not consumed it yet):"
for kind in icon radio; do
  if [ -f "$MODDIR/$kind.armed" ]; then echo "$kind: not consumed"; else echo "$kind: consumed or not armed"; fi
done
echo "Recent hook diagnostics:"
logcat -d -s R12IconCache:I R12RadioStartup:I R12Cache:I '*:S' | tail -n 25
