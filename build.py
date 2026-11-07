#!/usr/bin/env python3
"""Build the single arm64 Zygisk library, unchanged Java hooks, and Magisk ZIP."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent

def run(args):
    subprocess.run([str(x) for x in args], check=True)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ndk', type=Path, default=os.environ.get('ANDROID_NDK_HOME'))
    p.add_argument('--sdk', type=Path, default=os.environ.get('ANDROID_HOME'))
    p.add_argument('--java-home', type=Path, default=os.environ.get('JAVA_HOME'))
    p.add_argument('--cmake', default='cmake')
    p.add_argument('--ninja', default='ninja')
    p.add_argument('--jobs', default='4')
    a = p.parse_args()
    if not all((a.ndk, a.sdk, a.java_home)):
        p.error('Set ANDROID_NDK_HOME, ANDROID_HOME and JAVA_HOME, or supply the corresponding options.')
    exe = '.exe' if os.name == 'nt' else ''
    java = a.java_home / 'bin' / ('java' + exe)
    javac = a.java_home / 'bin' / ('javac' + exe)
    jar = a.sdk / 'platforms/android-33/android.jar'
    d8 = a.sdk / 'build-tools/36.0.0/lib/d8.jar'
    build = ROOT / 'build'
    run([a.cmake, '-S', ROOT, '-B', build, '-G', 'Ninja',
         '-DCMAKE_TOOLCHAIN_FILE=' + str(a.ndk/'build/cmake/android.toolchain.cmake'),
         '-DCMAKE_MAKE_PROGRAM=' + str(Path(a.ninja).resolve() if Path(a.ninja).exists() else a.ninja),
         '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-34',
         '-DANDROID_STL=c++_static', '-DCMAKE_BUILD_TYPE=Release'])
    run([a.cmake, '--build', build, '--target', 'performance', '-j', a.jobs])
    classes = build / 'java'
    if classes.exists():
        shutil.rmtree(classes)
    classes.mkdir(exist_ok=True)
    run([javac, '-source', '8', '-target', '8', '-classpath', jar,
         '-d', classes, *sorted((ROOT/'java').rglob('*.java'))])
    mod = ROOT / 'module'
    for kind, package in [('icon', 'iconcache'), ('radio', 'radiostartup')]:
        dex = build / (kind+'-dex')
        dex.mkdir(exist_ok=True)
        run([java, '-cp', d8, 'com.android.tools.r8.D8', '--lib', jar,
             '--min-api', '34', '--output', dex,
             *sorted((classes/'local/redmi12'/package).glob('*.class'))])
        shutil.copy2(dex/'classes.dex', mod/(kind+'.dex'))
    (mod/'zygisk').mkdir(exist_ok=True)
    shutil.copy2(build/'libperformance.so', mod/'zygisk/arm64-v8a.so')
    host = 'windows-x86_64' if os.name == 'nt' else ('darwin-x86_64' if os.uname().sysname == 'Darwin' else 'linux-x86_64')
    run([a.ndk/'toolchains/llvm/prebuilt'/host/'bin'/('llvm-strip'+exe),
         '--strip-debug', mod/'zygisk/arm64-v8a.so'])
    dist = ROOT / 'dist'
    dist.mkdir(exist_ok=True)
    # Only explicit module inputs; never publish state, dumps or a copied device module.
    files = sorted(x for x in mod.rglob('*') if x.is_file())
    for x in files:
        rel = x.relative_to(mod)
        assert 'state' not in rel.parts and x.name not in ('armed', 'disable', 'remove')
        if x.suffix in ('.sh', '.prop', '.rule'):
            assert b'\r' not in x.read_bytes(), f'CRLF in {rel}'
    manifest = {x.relative_to(mod).as_posix(): hashlib.sha256(x.read_bytes()).hexdigest() for x in files}
    archive = dist/'redmi12-performance-v1.0.0.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for x in files:
            info = zipfile.ZipInfo(x.relative_to(mod).as_posix(), (2026,11,7,0,0,0))
            info.external_attr = (0o100755 if x.suffix == '.sh' or x.name == 'update-binary' else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, x.read_bytes())
        z.writestr('payload-sha256.json', json.dumps(manifest, indent=2)+'\n')
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (dist/'SHA256SUMS').write_text(f'{digest}  {archive.name}\n', encoding='utf-8')
    (dist/'payload-sha256.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(f'\nBuilt {archive}\nSHA256 {digest}')

if __name__ == '__main__':
    main()
