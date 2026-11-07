# Bundled components

The application/module sources are provided under GPL-3.0. Dependencies retain their notices and licenses. Required source is included for rebuilding the static hook engine; see `docs/BUILD.md`.

| Component | Source / revision | License |
| --- | --- | --- |
| LSPlant | [LSPosed/LSPlant](https://github.com/LSPosed/LSPlant), `d8b5d1dbb664abc606644036822e4bb64547edf6` | LGPL-3.0, `third_party/lsplant/LICENSE` |
| dex_builder / slicer / parallel-hashmap | Nested source shipped with the above LSPlant checkout, including its original notices | See their license files under `third_party/lsplant/lsplant/src/main/jni/external/` |
| xdl | [hexhacking/xDL](https://github.com/hexhacking/xDL), `1e0b6254165a2ddcbd32f77a371700c69155acf8` | MIT, `third_party/xdl/LICENSE` |
| Dobby prebuilt | Existing Dobby 1.2 arm64 prefab archive, SHA256 `609ed766e94c21313a4373ed62a3b296bd12c8b4bfabfab4df19a611b0a38746` | Apache-2.0, `third_party/dobby/LICENSE`; upstream: [jmpews/Dobby](https://github.com/jmpews/Dobby) |
| Zygisk API header | [topjohnwu/zygisk-module-sample](https://github.com/topjohnwu/zygisk-module-sample), `8ce26128f81baaed0b969aaf7f52f886b61af4ab` | ISC-style permission notice in `third_party/zygisk/zygisk.hpp` |
| Magisk module installer | [topjohnwu/Magisk v31.1](https://github.com/topjohnwu/Magisk/blob/v31.1/scripts/module_installer.sh) | GPL license supplied as `module/licenses/Magisk-COPYING` |

The release ZIP includes dependency license notices. The NoFingerprint resource overlay source is in `overlay/`.
