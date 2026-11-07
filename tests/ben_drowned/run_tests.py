#!/usr/bin/env python3
"""Run BEN director, save, and headless runtime regressions.

Uses the production source and real engine headers. The optional engine runtime
fixtures substitute explicit renderer/resource/collision services and do not
exercise the actual game, graphics driver, ROM assets, or Windows ABI. JSON tests
require real nlohmann/json.hpp; a missing dependency is reported, never simulated.
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
assert _spec and _spec.loader
bot_tests = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bot_tests)
ENGINE_SOURCES = [
    "mm/2s2h/Enhancements/BenDrowned/BenDrowned.cpp",
    "mm/2s2h/Enhancements/BenDrowned/BenWorld.cpp",
    "mm/src/code/z_actor.c", "mm/src/code/z_play.c", "mm/src/code/z_sram_NES.c",
    "mm/2s2h/Enhancements/PlayerBots/PlayerBots.cpp",
    "mm/src/audio/lib/synthesis.c", "mm/src/code/z_kankyo.c", "mm/src/code/z_map_disp.c",
    "mm/src/code/z_parameter.c", "mm/src/code/z_view.c",
    "mm/src/overlays/actors/ovl_En_Fsn/z_en_fsn.c",
    "mm/src/overlays/actors/ovl_En_Gakufu/z_en_gakufu.c",
    "mm/src/overlays/actors/ovl_En_Ossan/z_en_ossan.c",
    "mm/src/overlays/actors/ovl_En_Sob1/z_en_sob1.c",
    "mm/src/overlays/actors/ovl_En_Trt/z_en_trt.c",
    "mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_map.c",
    "mm/src/overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope_NES.c",
]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--compile-engine", action="store_true", help="Compile the actual BEN and changed engine C/C++ units to objects.")
    ap.add_argument("--sanitizers", action="store_true", help="ASan + UBSan for pure policy/JSON; UBSan for engine fixtures.")
    ap.add_argument("--require-json", action="store_true", help="Fail when real nlohmann JSON headers are missing.")
    ap.add_argument("--json-include", type=Path, help="Directory containing nlohmann/json.hpp")
    ap.add_argument("--output", type=Path, default=ROOT / "build-ben-tests")
    args = ap.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cxx = shutil.which(os.environ.get("CXX", "g++"))
    cc = shutil.which(os.environ.get("CC", "gcc"))
    if not cxx or not cc:
        print("Install GCC/Clang with C++20 and C11, or set CXX/CC.", file=sys.stderr)
        return 2
    env = os.environ.copy()
    env.setdefault("ASAN_OPTIONS", "detect_leaks=1:halt_on_error=1")
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
    report: list[dict] = []
    def record(entry: dict) -> None:
        report.append(entry)
        (out / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    def run(label: str, cmd: list[str]) -> bool:
        result = subprocess.run(cmd, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / (label + ".log")).write_text(result.stdout, encoding="utf-8")
        record({"test": label, "status": "PASS" if result.returncode == 0 else "FAIL",
                "returncode": result.returncode, "command": cmd, "log": label + ".log"})
        print(("PASS " if result.returncode == 0 else "FAIL ") + label, flush=True)
        if result.stdout and (label.endswith("run") or result.returncode):
            print(result.stdout[-12000:], flush=True)
        return result.returncode == 0
    record({"test": "environment", "status": "INFO", "compiler": cxx,
            "ASAN_OPTIONS": env["ASAN_OPTIONS"], "UBSAN_OPTIONS": env["UBSAN_OPTIONS"],
            "scope": "Headless Linux policy/integration; explicit engine doubles; not a full game or Windows build"})
    common = ["-DF3DEX_GBI_2", "-DCONTROLLERBUTTONS_T=uint32_t"] + ["-I" + p for p in bot_tests.INCLUDES]
    for name in ("core", "runtime", "world"):
        source = ROOT / f"tests/ben_drowned/{name}_tests.cpp"
        binary = out / f"{name}_tests"
        flags: list[str] = []
        if args.sanitizers:
            flags = ["-fsanitize=address,undefined" if name == "core" else "-fsanitize=undefined", "-fno-omit-frame-pointer"]
        cmd = [cxx, "-std=c++20", "-O1", "-g", *flags]
        if name != "core":
            cmd += ["-w", "-ffunction-sections", "-fdata-sections", *common]
        link_objects = []
        if name == "world":
            # Compile the interpolation doubles as C, in a separate object.
            # A C++-mangled renderer reference must fail at the real link step.
            interpolation_object = out / "interpolation_link_stubs.o"
            c_flags = ["-fsanitize=undefined", "-fno-omit-frame-pointer"] if args.sanitizers else []
            c_cmd = [cc, "-std=gnu11", "-O1", "-g", "-w", *c_flags, *common, "-c",
                     str(ROOT / "tests/ben_drowned/interpolation_link_stubs.c"), "-o", str(interpolation_object)]
            if not run("world_c_linkage_build", c_cmd):
                return 1
            link_objects.append(str(interpolation_object))
        cmd += [str(source), *link_objects, "-Wl,--gc-sections", "-o", str(binary)]
        if not run(name + "_build", cmd) or not run(name + "_run", [str(binary)]):
            return 1
    json_flags = ["-I" + str(args.json_include.resolve())] if args.json_include else []
    probe = subprocess.run([cxx, "-std=c++20", *json_flags, "-E", "-x", "c++", "-"],
                           input="#include <nlohmann/json.hpp>\n", text=True, capture_output=True)
    if probe.returncode:
        status = "FAIL" if args.require_json else "NOT_RUN"
        (out / "json_dependency_probe.log").write_text(probe.stderr, encoding="utf-8")
        record({"test": "save_json", "status": status, "reason": "Real nlohmann/json.hpp absent; no substitute JSON used",
                "log": "json_dependency_probe.log"})
        print(status + " save_json: install nlohmann JSON or pass --json-include.", flush=True)
        if args.require_json:
            return 1
    else:
        binary = out / "save_tests"
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitizers else []
        cmd = [cxx, "-std=c++20", "-O1", "-g", *flags, "-Imm", *json_flags,
               "tests/ben_drowned/save_tests.cpp", "mm/2s2h/SaveManager/Migrations/10.cpp", "-o", str(binary)]
        if not run("save_build", cmd) or not run("save_run", [str(binary)]):
            return 1
    if args.compile_engine:
        for source in ENGINE_SOURCES:
            cpp = source.endswith(".cpp")
            cmd = [cxx if cpp else cc, "-std=c++20" if cpp else "-std=gnu11", "-c", "-O0", "-w", *common]
            if not cpp:
                cmd += ["-Wno-incompatible-pointer-types", "-Wno-int-conversion", "-Werror=implicit-function-declaration"]
            cmd += [source, "-o", str(out / (Path(source).stem + ".o"))]
            if not run("compile_" + Path(source).stem, cmd):
                return 1
    print("Completed. Read report.json for NOT_RUN checks. No full game link, Windows build, GPU rendering, asset loading or playtest is claimed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
