# Build

Required: Python 3.10+, CMake 3.28+ with C++ module support (tested 4.4.4), Ninja, Android NDK r29, JDK 17, Android SDK platform 33 and build-tools 36.0.0. Dependencies are vendored; no Gradle or online dependency resolution is needed.

```sh
python build.py --ndk /path/to/android-ndk-r29 --sdk /path/to/android-sdk --java-home /path/to/jdk17
```

Use `--cmake` and `--ninja` for explicit executable paths, or set `ANDROID_NDK_HOME`, `ANDROID_HOME`, `JAVA_HOME` and PATH. Windows paths with spaces must be quoted.

Outputs: `dist/redmi12-performance-v1.0.0.zip`, `dist/SHA256SUMS`, `dist/payload-sha256.json`. The ZIP contains one module ID and one arm64 native library. C++ uses the static libc++ runtime. Builds contain toolchain-dependent native binaries; bit-identical rebuilding on arbitrary hosts is not claimed. ZIP timestamps are fixed.

`third_party/dobby-prebuilt/libdobby.a` is the existing arm64 Dobby 1.2 prefab archive used for the tested hooks. Its header and license are included; upstream source is linked in `THIRD_PARTY.md`. You may replace this archive with a compatible locally built Dobby library, then rebuild/relink the complete module. LSPlant, dex_builder and xdl are built from the included source. Modified sources can be relinked using this same command; do not load an untested ART hook into a daily-use system process.

The fingerprint resource overlay APK is the previously tested APK. Its manifest and resource source are in `overlay/`. To rebuild it, use SDK `aapt2 compile` on `overlay/res`, `aapt2 link --manifest overlay/AndroidManifest.xml -I <android.jar> --auto-add-overlay`, then sign with your own key using `apksigner`. No signing private key is included; overlay changes require separate runtime validation.

For another ROM build, changing the install hashes alone is insufficient: recheck ART symbols/build ID, process UIDs, hooked methods/fields, the telephony callback ABI, sensor resources, vendor interfaces and policy permissions. The original fingerprints are intentionally restrictive.
