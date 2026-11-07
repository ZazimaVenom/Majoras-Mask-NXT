# Player Bots + Combat Population — source update

Built against the source in `MajoraWithBots.rar`, Git baseline
`a89c41a5f432d54bdd9d94ec88f87f8a9a44c847`. Prepared 11 October 2026 (Melbourne).

**Status: implemented source build candidate. Gameplay source compilation and
headless tests pass; a complete game build, Windows build, and in-game validation
have NOT been completed. No runnable game executable is included.**

## Apply this update

1. Extract your original `MajoraWithBots.rar` into a separate working folder.
2. Open `MajoraWithBots_PlayerBots_Update.zip`. Merge the contents of its
   `MajoraWithBots` directory into your existing `MajoraWithBots` source directory,
   replacing the matching files. The resulting directory must contain
   `CMakeLists.txt`, `mm`, `libultraship`, and `Build-PlayerBots.ps1` together.
3. Keep the original archive and your existing game installation unchanged. Use a
   separate test installation and a copy of your save for initial gameplay checks.
4. Build a **new executable** using the instructions below. This is not an OTR/O2R
   resource mod and cannot be installed by copying it into a game's `mods` folder.

The update ZIP contains changed/new source, build helpers, tests, and documentation,
not a second copy of the unchanged source assets and dependency trees. Those remain
in your original archive. The matching `.patch` is an alternative for Git users;
do not apply the ZIP and the patch twice. Apply a patch only to the exact baseline:

```sh
git apply --check /path/to/MajoraWithBots_PlayerBots.patch
git apply /path/to/MajoraWithBots_PlayerBots.patch
```

## What is implemented

The new actor and population director create independent, temporary adventurers in
the loaded navigable area. The default target is **15 bots**, adjustable from
**10 to 20**. Their renderer uses Human Link, Deku Link, Goron Link, Zora Link, and
optional Fierce Deity Link. Human variants wear a curated set of static masks.
Each bot has its own name, animation pose, AI, health, and combat colliders.

Bots explore collision-checked routes, roam, approach supported enemies, attack,
evade, recover, and respawn when defeated. Supported ordinary creatures can target
and damage the bots, or fight the real player. The player retains priority nearby
and while engaged; player/bot and bot/bot friendly fire are blocked.

The reinforcement director uses existing native enemy actors, not new monster
models. It scales the supported enemy population to active bots, taking existing
supported enemies into account. Defaults: two supported enemies per active bot,
at most 28 added enemies, with a separate 12-extra ceiling in dungeon scenes.
Spawns are staggered and require suitable floor space and engine capacity.

**No enemies are added to Clock Town, houses, shops, or any scene outside the
explicit combat whitelist.** Ordinary safe maps receive peaceful visitors only.
Approved dungeons receive new enemies only while a supported original ordinary
enemy remains in the current room. Added enemies do not count toward room-clear
requirements. Original enemies retain their ordinary drops and progression effects.

## Build on Windows

Use the toolchain described in the original `docs/BUILDING.md`: Visual Studio 2022
with Desktop development with C++, MSVC v143, a Windows SDK, Python 3, Git, and
CMake 3.26 or newer. Dependency downloads may require internet access. The original
archive's bundled dependency directories must be present.

From PowerShell in the merged source directory:

```powershell
.\Build-PlayerBots.ps1 -Configuration Release -Jobs 4 -Package
```

The helper stops when a command fails, builds in `build-player-bots`, optionally
packages the result, and prints executable paths only after a successful build.
It does not launch the game or overwrite an installed copy. The Windows helper
has not been executed under PowerShell/MSVC in this environment.

The same build steps can be run directly, including when local PowerShell policy
does not allow running a script:

```powershell
cmake -S . -B build-player-bots -G "Visual Studio 17 2022" -T v143 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-player-bots --config Release --target Generate2ShipOtr --parallel 4
cmake --build build-player-bots --config Release --parallel 4
```

Only continue to the next command after a successful exit. Follow the original
build documentation for packaging/runtime dependencies and use your own supported
game assets with the newly built executable.

**Use a fresh build directory.** The runtime collision-check context is larger in
this update. Every dependent translation unit must be rebuilt; mixing old object
files or binaries with the new headers is unsafe. The change does not extend the
save-file schema.

## Build on Linux

Install the dependencies for your distribution listed in `docs/BUILDING.md`, then:

```sh
bash build-player-bots.sh
```

The shell helper has passed `bash -n`; the complete game build has not succeeded
in this environment. See the validation report for the exact configure failure.

## Controls

In the modified game build, open **Enhancements > Player Bots**. Bots, supported
combat, and combat-map reinforcements default to enabled. The menu exposes
population, nameplates/health, Fierce Deity variants, respawn delay, combat,
reinforcements, enemies per bot, the extra-enemy cap, refill pace, and navigation
work per tick. Disabling removes the added population, not the original map actors.

## Important coverage limits

The target is per **loaded navigable area**, not a promise of 10–20 actors in every
room. Small interiors, disconnected geometry, unsafe floors, and capacity limits
can reduce the population. Deliberately excluded boss arenas, cutscene scenes,
stealth/timed challenges, and selected scripted maps remain unchanged. The complete
scene policy is in `docs/PLAYER_BOTS_SCENE_COVERAGE.md`.

Navigation is collision-derived ground movement, not an authored traversal system.
There is no general jumping, climbing, Deku-flower traversal, or full Zora swimming.
Animations and forms are implemented but still require visual verification against
your game resources and any resource replacements. Bot combat is integrated with
seven ordinary enemy families, not every enemy or boss in the game.

This is a local populated-world feature, **not network multiplayer**. Unloaded
maps are not simulated simultaneously, and bot health/positions are not persisted
in game saves. No new quests, trading, loot inventory, or dialogue system is added.

## Verification and further technical detail

Read `docs/PLAYER_BOTS_VALIDATION.md` for actual results, missing validation, and the
manual gameplay checklist. `docs/PLAYER_BOTS.md` contains the architecture,
configuration table, integration points, and balancing constraints.

Reproduce the headless checks on a suitable Linux GCC/Clang environment:

```sh
python3 tests/player_bots/run_tests.py --compile-engine --sanitizers
```

Passing these checks does not establish that the complete game links or that every
map/model/encounter works in a running game.
