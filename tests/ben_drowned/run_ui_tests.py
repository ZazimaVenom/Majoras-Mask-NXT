#!/usr/bin/env python3
"""Run production BEN HUD against real pinned Dear ImGui and explicit Ship host doubles.

No native game or GPU backend is present. Requires the actual upstream ImGui
v1.91.9b-docking checkout (including .cpp sources), plus nlohmann JSON headers.
Produces neutral-background PPM QA images in --output, not game screenshots.
Also checks the production HUD with the Windows RPC small macro defined.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--imgui-source', type=Path, required=True)
ap.add_argument('--json-include', type=Path)
ap.add_argument('--output', type=Path, default=ROOT / 'build-ben-ui-tests')
ap.add_argument('--sanitizers', action='store_true')
args = ap.parse_args()
out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
imgui = args.imgui_source.resolve()
cxx = shutil.which(os.environ.get('CXX', 'g++'))
if not cxx:
    raise SystemExit('C++20 compiler missing; set CXX.')
includes = [ROOT / p for p in ('mm', 'mm/2s2h', 'mm/include', 'libultraship/include', 'libultraship/include/libultraship')]
includes += [imgui]
if args.json_include:
    includes += [args.json_include.resolve()]
source = ROOT / 'mm/2s2h/Enhancements/BenDrowned/BenHud.cpp'
flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if args.sanitizers else []
binary = out / 'ui_tests'
common = [cxx, '-std=c++20', '-O1', '-g', '-Wall', '-Wextra', *flags]
common += ['-I' + str(p) for p in includes]
# Windows RPC headers define small as char. Keep this check isolated to the
# production HUD so it models header pollution without changing ImGui's build.
macro_check = [*common, '-Dsmall=char', '-fsyntax-only', str(source)]
command = [*common]
command += [str(ROOT / 'tests/ben_drowned/ui_tests.cpp'), str(source)]
command += [str(imgui / p) for p in ('imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp')]
command += ['-o', str(binary)]
env = os.environ.copy()
env.setdefault('ASAN_OPTIONS', 'detect_leaks=1:halt_on_error=1')
env.setdefault('UBSAN_OPTIONS', 'halt_on_error=1:print_stacktrace=1')
report = []
for name, cmd in [('ui_windows_small_macro', macro_check), ('ui_build', command), ('ui_run', [str(binary)])]:
    result = subprocess.run(cmd, cwd=out, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / (name + '.log')).write_text(result.stdout)
    report.append({'test': name, 'status': 'PASS' if not result.returncode else 'FAIL',
                   'returncode': result.returncode, 'command': cmd, 'log': name + '.log'})
    (out / 'report.json').write_text(json.dumps(report, indent=2))
    print(name, 'PASS' if not result.returncode else 'FAIL', flush=True)
    if result.stdout: print(result.stdout[-6000:], flush=True)
    if result.returncode: raise SystemExit(1)
