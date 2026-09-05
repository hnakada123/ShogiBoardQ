#!/usr/bin/env python3
"""LinuxのRelease/Ninjaビルドを再利用し、実MainWindowのGUIテストをリンクする。"""
from pathlib import Path
import shutil
import subprocess

source = Path(__file__).resolve().parent
repo = source.parent.parent
build = repo / "build"
cache = dict(line.split("=", 1) for line in (build / "CMakeCache.txt").read_text().splitlines()
             if "=" in line and not line.startswith(("#", "//")))
if cache.get("CMAKE_GENERATOR:INTERNAL") != "Ninja" or cache.get("CMAKE_BUILD_TYPE:STRING") != "Release":
    raise SystemExit("GUIテストはLinuxのRelease/Ninjaビルドが対象です。tests/gui/README.mdを参照してください。")
audit = build / "gui-audit"
audit.mkdir(parents=True, exist_ok=True)
(audit / "screenshots").mkdir(exist_ok=True)
shutil.copy2(source / "mock_usi.py", audit / "mock_usi.py")
subprocess.run(['cmake', '--build', str(build), '-j', '4'], check=True)
rule = next(line for line in (build / 'build.ninja').read_text().splitlines()
            if line.startswith('build ShogiBoardQ:'))
objects = [str((build / item).resolve()) for item in rule.split()
           if item.endswith('.o') and not item.endswith('/src/app/main.cpp.o')]
assert objects, 'Application object list was empty'
(audit / 'objects.cmake').write_text('set(APP_OBJECTS\n' + ''.join(
    f'  "{item}"\n' for item in objects) + ')\n')
subprocess.run(['cmake', '-S', str(source), '-B', str(audit / 'test-build'),
                f'-DREPO={repo}', f'-DAPP_BUILD={build}', f'-DAUDIT_DIR={audit}', '-DCMAKE_BUILD_TYPE=Release'], check=True)
subprocess.run(['cmake', '--build', str(audit / 'test-build'), '-j', '4'], check=True)
