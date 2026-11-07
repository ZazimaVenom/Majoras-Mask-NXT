#!/usr/bin/env python3
"""Run bot regression tests and (optionally) compile affected engine units.

Requires Python 3.9+, GCC/G++ on Linux (or compatible Clang). No ROM or GUI
libraries are required. Headless integration uses documented engine doubles;
this is NOT a full game build or an in-game test. Run from any working directory.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
INCLUDES = ["mm", "mm/2s2h", "mm/include", "mm/src", "mm/assets", "libultraship/include",
            "libultraship/include/libultraship", "mm/include/PR",
            "libultraship/src", "libultraship/extern/fast3d/include"]
SOURCES = ["mm/src/code/z_actor.c", "mm/src/code/z_collision_check.c", "mm/src/code/z_play.c",
           "mm/src/code/z_scene.c", "mm/src/code/z_actor_dlftbls.c"] + [
    f"mm/src/overlays/actors/ovl_{name}/z_{name.lower()}.c" for name in [
        "En_Dekubaba", "En_Dodongo", "En_Firefly", "En_Neo_Reeba", "En_Slime", "En_Tite", "En_Wf"]
] + ["mm/2s2h/Enhancements/PlayerBots/PlayerBots.cpp"]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-engine", action="store_true", help="Compile 13 actual engine/bot translation units to objects (not a game executable).")
    parser.add_argument("--sanitizers", action="store_true", help="ASan+UBSan for the core test; UBSan for headless integration.")
    parser.add_argument("--output", type=Path, default=ROOT / "build-player-bots-tests")
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    cxx = shutil.which(os.environ.get("CXX", "g++")); cc = shutil.which(os.environ.get("CC", "gcc"))
    if not cxx or (args.compile_engine and not cc):
        print("A C++20 compiler (g++) and, for --compile-engine, gcc are required. Set CXX/CC to override.", file=sys.stderr)
        return 2
    report: list[dict] = []
    env = os.environ.copy()
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
    env.setdefault("ASAN_OPTIONS", "detect_leaks=1:halt_on_error=1")
    def run(label: str, command: list[str]) -> bool:
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        log = out / (label + ".log"); log.write_text(result.stdout, encoding="utf-8")
        report.append({"test": label, "returncode": result.returncode, "command": command, "log": log.name})
        print(("PASS " if result.returncode == 0 else "FAIL ") + label)
        if result.stdout and ("run" in label or result.returncode): print(result.stdout[-6000:])
        (out / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        return result.returncode == 0
    common = ["-DF3DEX_GBI_2", "-DCONTROLLERBUTTONS_T=uint32_t"] + ["-I"+p for p in INCLUDES]
    core = out / "core_tests"
    core_flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitizers else []
    if not run("core_build", [cxx,"-std=c++20","-O1","-g",*core_flags,"tests/player_bots/core_tests.cpp","-o",str(core)]): return 1
    if not run("core_run", [str(core)]): return 1
    combat = out / "combat_api_tests"
    # Garbage-collect renderer/population functions unused by this test. Address
    # Sanitizer roots global ActorProfile data, so use UBSan for this executable.
    combat_flags = ["-fsanitize=undefined"] if args.sanitizers else []
    if not run("combat_build", [cxx,"-std=c++20","-O1","-g","-w","-ffunction-sections","-fdata-sections",
        *combat_flags,*common,"tests/player_bots/combat_api_tests.cpp","-Wl,--gc-sections","-o",str(combat)]): return 1
    if not run("combat_run", [str(combat)]): return 1
    if args.compile_engine:
        for source in SOURCES:
            cpp = source.endswith(".cpp")
            command = [cxx if cpp else cc,"-std=c++20" if cpp else "-std=gnu11","-c","-O0","-w",*common]
            if not cpp:
                # Same port compatibility categories suppressed by mm/CMakeLists.txt.
                command += ["-Wno-incompatible-pointer-types","-Wno-int-conversion","-Werror=implicit-function-declaration"]
            command += [source,"-o",str(out / (Path(source).stem+".o"))]
            if not run("compile_"+Path(source).stem, command): return 1
        nm = shutil.which("nm")
        if nm:
            result = subprocess.run([nm,"-u",str(out / "PlayerBots.o")],capture_output=True,text=True,check=True)
            if "_ZN12_GLOBAL__N_1" in result.stdout:
                print("FAIL: an unresolved anonymous-namespace symbol exists in PlayerBots.o", file=sys.stderr)
                return 1
            (out / "PlayerBots_undefined_symbols.txt").write_text(result.stdout, encoding="utf-8")
    print("Completed. GUI compilation, complete game linking, graphics and in-game navigation are not covered.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
