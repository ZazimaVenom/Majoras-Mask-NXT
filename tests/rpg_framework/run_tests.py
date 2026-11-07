#!/usr/bin/env python3
"""Run production RPG policy, combat, raid and optional real-JSON regressions.

Linux GCC/Clang harness; not a full application/Windows/graphics test. Engine
fixtures use explicit doubles for resources and engine services. --compile-engine
also compiles the affected gameplay source to real object files. JSON checks use
nlohmann/json.hpp when installed; --require-json makes absence a hard failure.
"""
from __future__ import annotations
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("bot_test_tools", ROOT / "tests/player_bots/run_tests.py")
assert _spec is not None and _spec.loader is not None
bot_tests = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bot_tests)
SOURCES = bot_tests.SOURCES + [
    "mm/src/code/z_parameter.c", "mm/src/code/z_skelanime.c",
    "mm/src/overlays/actors/ovl_En_Fz/z_en_fz.c",
    "mm/src/overlays/actors/ovl_En_Snowman/z_en_snowman.c",
    "mm/2s2h/Enhancements/RpgFramework/RpgFramework.cpp",
    "mm/2s2h/Enhancements/RpgFramework/RpgWorldBoss.cpp",
]

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--compile-engine", action="store_true")
    ap.add_argument("--sanitizers", action="store_true")
    ap.add_argument("--require-json", action="store_true")
    ap.add_argument("--json-include", type=Path, help="Directory containing nlohmann/json.hpp")
    ap.add_argument("--output", type=Path, default=ROOT / "build-rpg-tests")
    args = ap.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    cxx = shutil.which(os.environ.get("CXX", "g++"))
    cc = shutil.which(os.environ.get("CC", "gcc"))
    if not cxx or (args.compile_engine and not cc):
        print("Set CXX/CC to installed GCC or Clang C++20/C11 compilers.", file=sys.stderr)
        return 2
    env = os.environ.copy()
    env.setdefault("ASAN_OPTIONS", "detect_leaks=1:halt_on_error=1")
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
    report: list[dict] = []
    def record(entry: dict) -> None:
        report.append(entry)
        (out / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    def run(label: str, command: list[str]) -> bool:
        r = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / (label + ".log")).write_text(r.stdout, encoding="utf-8")
        record({"test": label, "status": "PASS" if not r.returncode else "FAIL", "returncode": r.returncode,
                "command": command, "log": label + ".log"})
        print(("PASS " if not r.returncode else "FAIL ") + label, flush=True)
        if r.stdout and (label.endswith("run") or r.returncode): print(r.stdout[-10000:], flush=True)
        return r.returncode == 0
    common = ["-DF3DEX_GBI_2", "-DCONTROLLERBUTTONS_T=uint32_t"] + ["-I" + p for p in bot_tests.INCLUDES]
    for name in ("core", "runtime", "boss", "raid_roster"):
        binary = out / (name + "_tests")
        flags: list[str] = []
        if args.sanitizers:
            # ASan roots global native ActorProfiles. Renderer-independent engine
            # fixtures therefore use UBSan and --gc-sections; the core uses both.
            flags = ["-fsanitize=address,undefined" if name == "core" else "-fsanitize=undefined", "-fno-omit-frame-pointer"]
        cmd = [cxx, "-std=c++20", "-O1", "-g", *flags]
        if name != "core": cmd += ["-w", "-ffunction-sections", "-fdata-sections", *common]
        cmd += [f"tests/rpg_framework/{name}_tests.cpp", "-Wl,--gc-sections", "-o", str(binary)]
        if not run(name + "_build", cmd) or not run(name + "_run", [str(binary)]): return 1
    json_flags = ["-I" + str(args.json_include.resolve())] if args.json_include else []
    probe = subprocess.run([cxx, "-std=c++20", *json_flags, "-E", "-x", "c++", "-"],
                           input="#include <nlohmann/json.hpp>\n", capture_output=True, text=True)
    if probe.returncode:
        status = "FAIL" if args.require_json else "NOT_RUN"
        (out / "json_dependency_probe.log").write_text(probe.stderr, encoding="utf-8")
        record({"test": "save_json", "status": status, "reason": "Real nlohmann/json.hpp not installed; no substitute JSON used",
                "log": "json_dependency_probe.log"})
        print(f"{status} save_json: install nlohmann JSON or pass --json-include.", flush=True)
        if args.require_json: return 1
    else:
        binary = out / "save_tests"
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitizers else []
        cmd = [cxx,"-std=c++20","-O1","-g",*flags,"-Imm",*json_flags,
               "tests/rpg_framework/save_tests.cpp", "mm/2s2h/SaveManager/Migrations/9.cpp","-o",str(binary)]
        if not run("save_build",cmd) or not run("save_run",[str(binary)]): return 1
    if args.compile_engine:
        for source in SOURCES:
            cpp = source.endswith(".cpp")
            cmd = [cxx if cpp else cc, "-std=c++20" if cpp else "-std=gnu11", "-c", "-O0", "-w", *common]
            if not cpp: cmd += ["-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Werror=implicit-function-declaration"]
            cmd += [source, "-o", str(out / (Path(source).stem + ".o"))]
            if not run("compile_" + Path(source).stem, cmd): return 1
    if not run("source_wiring_run", [sys.executable, str(ROOT / "tests/rpg_framework/source_checks.py")]): return 1
    print("Finished. Inspect report.json for NOT_RUN checks. No complete game, renderer or Windows run was performed.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
