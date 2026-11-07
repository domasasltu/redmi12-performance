# Redmi 12 Performance

One Magisk module for **Redmi 12 4G (`heat` / `fire`), crDroid 12.12, Android 16**.

**Supported ROM build: 29 September 2026, 12:19:19 UTC** — incremental `1790684359`. The date was read directly from the installed ROM. The installer checks the device, build and required system binaries; other builds are rejected. Disable the module before updating your ROM.

## Install

1. Download **`redmi12-performance-v1.0.0.zip`** from [Releases](https://github.com/domasasltu/redmi12-performance/releases/latest).
2. Enable **Zygisk** in **Magisk 31.1 or newer**, then reboot if you just enabled it.
3. Open **Magisk → Modules → Install from storage**, select the ZIP and reboot.

Install from the owner profile with native resolution and density. Previous versions of the separate component modules are disabled automatically. The **Action** button shows status and checks the one-time app compilation.

## Features

- Native **1080×2460**, density **440**, minimum/peak refresh request **90 Hz**.
- Window blur disabled; animations set to **0.5×**.
- Threaded OpenGL compositor and a guarded SystemUI icon-setting cache.
- Optional DAMON reclaim disabled; existing **100 ms** kernel touch boost configured.
- Radio service startup/retry guard and scoped PowerHAL display-query permissions.
- One-time `speed` compilation for SystemUI, Launcher, Settings and selected built-in apps.
- Stock maximum clocks, governors and thermal protections retained; SELinux remains **Enforcing**.

**Fingerprint unlock is disabled**, and the crashing MiCharge extension is stopped. Use PIN/password. Basic charging was checked; fast charging is unverified. Android can override refresh requests for thermal/display safety.

Hooks are restricted to the tested processes and ART build. Each gets one attempt per boot; a process restart skips its hook. An incomplete-boot marker provides a disable fallback on the following boot.

## Remove

Remove the module in Magisk and reboot. Removal restores the six Android settings saved before its first installation. Previously disabled component modules stay disabled; normal Android compilation caches remain.

To restore saved settings before disabling the module:

```sh
su -c 'sh /data/adb/modules/redmi12_performance/restore-settings.sh'
```

If the UI is unavailable but root ADB works:

```sh
adb shell su -c 'touch /data/adb/modules/redmi12_performance/disable'
adb reboot
```

The same disable file can be created in recovery when `/data` is accessible.

## Source

See [build instructions](docs/BUILD.md) and [dependency notices](THIRD_PARTY.md). Module code is licensed under **GPL-3.0**; dependencies retain their original licenses.
