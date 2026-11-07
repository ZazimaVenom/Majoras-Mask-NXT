#!/usr/bin/env bash
# Linux source build helper. See the original docs/BUILDING.md for dependencies.
# No automatic package/ROM download, save copying, or launching.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build="$root/build-rpg-framework"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release -DBUILD_RPG_TESTS=ON -DRPG_ISOLATED_OUTPUT=ON
cmake --build "$build" --target RpgCoreTests RpgSaveTests --parallel "${JOBS:-4}"
ctest --test-dir "$build" -C Release --output-on-failure -R '^Rpg\.'
cmake --build "$build" --target Generate2ShipOtr --parallel "${JOBS:-4}"
cmake --build "$build" --target 2ship --parallel "${JOBS:-4}"
printf '%s\n' 'Release build completed. Stage using the original Linux packaging instructions and your own game assets.'
