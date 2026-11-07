#!/system/bin/sh
# This release is intentionally locked to the binaries tested on the heat device.
compatible() {
  [ "$(getprop ro.product.device)" = heat ] &&
  [ "$(getprop ro.build.version.sdk)" = 36 ] &&
  [ "$(getprop ro.build.version.incremental)" = 1790684359 ] &&
  [ "$(sha256sum /system_ext/priv-app/SystemUI/SystemUI.apk | cut -d ' ' -f1)" = 98ae4e970263ab4b0e1aa5951c928314e4a2950eff962e469d112556d75c5a0f ] &&
  [ "$(sha256sum /system/framework/telephony-common.jar | cut -d ' ' -f1)" = 3eab182c4f76a3dbbe8c22b9d107eeda070f21a9c60e81136f85984b4cce648f ] &&
  [ "$(sha256sum /vendor/lib64/android.hardware.power-service-mediatek.so | cut -d ' ' -f1)" = bbe236b4869f08d9703448fbe5e3a67539d46e4a014345651b8b99e379e7054d ]
}
restore_settings() {
  [ -f "$MODDIR/state/settings.tsv" ] || return 0
  while read -r namespace key user value; do
    case "$namespace" in global|system) ;; *) continue ;; esac
    case "$user" in ''|*[!0-9]*) continue ;; esac
    case "$key" in min_refresh_rate|peak_refresh_rate|disable_window_blurs|window_animation_scale|transition_animation_scale|animator_duration_scale) ;; *) continue ;; esac
    if [ "$value" = null ]; then
      settings --user "$user" delete "$namespace" "$key"
    else
      settings --user "$user" put "$namespace" "$key" "$value"
    fi
  done < "$MODDIR/state/settings.tsv"
}
