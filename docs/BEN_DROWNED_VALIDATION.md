# BEN DROWNED validation

This report describes checks run on the supplied Majora's Mask NXT source with the BEN implementation, 11 October 2026. It does **not** claim a completed Windows game build or in-game playtest. The ZIP is source code; a local native build and the acceptance checks below are still required.

## MSVC linker-fix follow-up (build fix 2)

The next supplied log reaches the linker and reports two unresolved interpolation functions from `BenWorld.obj`. The interpolation header already declares C linkage. The defect is the block-scope redeclarations inside `OPEN_DISPS`/`CLOSE_DISPS`: expansion in a C++ namespace can request different symbols. `gfx.h` now declares the C API at header scope and uses explicit global calls in C++; redundant BEN, bot and boss wrappers were removed. Interpolation remains active and call order is preserved.

The previous world fixture never called `WorldDraw`, allowing linker garbage collection to discard the failing path. It now executes actual statue/circle draw code with guarded graphics buffers and explicit native-service doubles. Its interpolation functions come from a separate source compiled as C, with no namespace aliases. **The previous production source fails to link under this fixture with the same two unresolved functions; corrected source passes 408 assertions.** It also links/runs at `-O0` without section garbage collection.

Additional executed checks:

- Actual BEN, PlayerBots and RPG boss objects compile; symbol inspection confirms only the two unmangled C interpolation references.
- Standalone CMake target and CTest `Ben.WorldDrawLinkage` pass. The PowerShell build helper now includes this gate before the game build. Local CMake used a package wrapper pointing to the real installed JSON headers.
- The director/content suite, 32 coordinator assertions, expanded 408 world assertions and 675 JSON/migration assertions pass via `run_tests.py` with the documented sanitizers.
- Actual player and Wipe5 C units compile. Player conditional-pointer diagnostics fall from 17 to zero; Wipe5 passes the integer/pointer warning gate that fails on old source.
- The pause unit compiles with unknown pragmas treated as errors; GCC/IDO pragmas are now guarded for their compilers.
- Actual randomizer headers and 3,072 clock cases pass with signed-comparison warnings treated as errors. Source-extracted pool metrics/formats pass 80,803 cases under strict conversion/format flags, including counts above `INT_MAX`. Pool counts use `std::size_t` and `%zu`; the ineffective enum `typedef` is removed.

Evidence is in `tests/ben_drowned/validation/msvc-link-fix/`. All checks ran on Linux. Full Menu and mixer unit checks stop at missing SDL2 and Opus headers respectively; focused checks do not replace complete builds. Other unrelated legacy GCC diagnostics remain. No complete Windows/MSVC game link, GPU/asset validation or live game/save test was performed. Save format stays 10. See `README_MSVC_BUILD_FIX_2.txt` for applying the cumulative update; back up the working installation and saves before testing the built game.

## MSVC compile-fix follow-up

The reported Windows build exposed a macro collision missed by the initial Linux checks: Windows RPC headers define `small` as `char`. Renaming the HUD variable to `labelFontSize` fixes the declaration and all downstream HUD errors in the supplied log. The notification reverse offset now uses an explicit positive floating-point conversion and unsigned traversal, notification expiry uses safe iterator erasure, the tooltip uses a literal `100%`, and `previousDay` matches the game's signed `s32` day field. Save format remains 10.

Follow-up verification:

- The previously delivered HUD fails with `-Dsmall=char` at the reported locations. The corrected production HUD passes with real ImGui headers. `run_ui_tests.py` now runs this isolated compile regression before the normal UI build.
- The real ImGui UI suite passes again with ASan/UBSan at 240x180, 640x480, 1280x800 and 3840x2160, including overlay visibility, geometry, journal and response-button checks.
- The production coordinator passes all 32 session/runtime assertions under UBSan and separately compiles with `-Werror=sign-compare`. Existing controller macro redefinition warnings remain in that isolated Linux compile; this is not a claim of a warning-free application build.
- Extracted production notification positioning and the complete expiry method pass 75 checks under UBSan with strict sign/conversion warnings treated as errors, with no compiler diagnostics. Empty, single and multiple notifications, all corners, hidden mode, and adjacent/all-expired entries are covered. This fixture uses explicit host doubles and is not a complete compile of `Notification.cpp`.

Follow-up commands, logs and the notification fixture are in `tests/ben_drowned/validation/msvc-compile-fix/`. All checks run on Linux; the `small` macro regression emulates that specific Windows header behavior. A complete Windows/MSVC build and native game playtest remain unverified here. No real installation or save was opened. Back up both before testing the newly built game. See `README_MSVC_COMPILE_FIX.txt` for applying the small update ZIP.

## Executed checks

| Area | Executed result | Scope |
| --- | --- | --- |
| Director and content | PASS | Production `BenCore.h` and `BenContent.h`: all 32,768 generated transmissions unique; maximum 175 bytes; all 256 family/variant labels; 2,048 no-repeat transmissions per family; deterministic continuation across 4,096 events and 85 loops; context weights, pacing, safe-area/low-health exclusions, responses, friendship and four-meeting truce, bounded counters, and future-schema preservation. |
| Session and UI coordinator | PASS: 32 assertions | Actual `BenDrowned.cpp` with explicit world/CVar/actor-classification doubles: pre-load option latch, pause-side publication, suspend/resume requests, one answer per event, stale room requests, save-slot changes, counter resets, death/cycle observation, and mirror restoration. |
| World/trials | PASS: 52 assertions | Actual `BenWorld.cpp` with explicit ground/audio/particle/bot/health doubles: bounded placement attempts, gradual encounter spawning, blocked-spawn fallback, one-shot success/failure, single mercy reward, travel discontinuity filtering, pause/room cleanup, and exact native lighting/fog transaction restoration. |
| Save codec and migration | PASS: 675 assertions | Real nlohmann JSON 3.12.0 and actual version-10 migration. Signed/unsigned bounds, all fields and deck arrays, atomic malformed rejection, 64-bit counters, future-version rejection, normal Zelda/RPG field preservation, independent owl/cycle snapshots, and idempotence. No real user save is opened. |
| Changed engine units | PASS: 18 object compilations | Actual BEN coordinator/world, PlayerBots, actor/play/save-buffer, audio synthesis, environment/map/HUD/view, shopkeeper/instrument actors, and pause-map C sources, using real supplied engine headers. These are native Linux objects, not a complete application link. |
| Real ImGui presentation | PASS | Production `BenHud.cpp` with upstream Dear ImGui 1.91.9b-docking, ASan/UBSan, explicit Ship/snapshot host doubles: 240x180, 640x480, 1280x800, 3840x2160; finite geometry and clip rectangles; inactive/suspended/hidden overlays; bounded journal; mouse clicks enqueue exactly Suspend/Resume/Kindness; no snapshot mutation or repeated answer submission. This is a headless draw-data test, not a GPU/game screenshot. |
| Existing Player Bots | PASS | 42,334 policy/pathfinding assertions and 88 headless combat/lifetime assertions. |
| Existing RPG Framework | PASS | 679,312 core assertions; 78 runtime, 7,349 boss, 92 raid-roster, 235 real-JSON migration assertions; 19 engine object compilations; 29 source-wiring/CMake archive-copy assertions. |

The core and JSON executables ran with AddressSanitizer and UndefinedBehaviorSanitizer. Engine fixtures use UndefinedBehaviorSanitizer and linker section garbage collection because unused native actor renderer registrations retain external game symbols under AddressSanitizer. LeakSanitizer could not inspect this runtime's restricted `/proc` task information; `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` was used. That disables leak reporting, not memory-access or undefined-behavior checks.

Logs and machine-readable commands are included in `tests/ben_drowned/validation/`. Earlier reports retained elsewhere in the baseline are historical and do not supersede this report.

## Build checks and portability repair

A real full-application CMake configure was attempted with CMake 4.4.4 and GCC 13.3. The archive's existing dependency patches had CRLF line-ending problems. The two dependency `.patch` files were normalized to LF without changing text content. The existing patch helper now uses `git apply --ignore-space-change`, including the reverse check, because the pinned StormLib source contains mixed line endings. Substantive patch context remains required. Both patched dependency downloads then configured successfully.

Four further modified C++ units (`NameTag.cpp`, `EnemyHealthBars.cpp`, `BetterOwlWarpMenu.cpp`, and `PauseOwlWarp.cpp`) compile against the real JSON/ImGui headers. The JSON header was explicitly preincluded for their baseline shared-header dependency. `BenHud.cpp` also compiles against real ImGui headers.

The full configure stops at the absent SDL2 development package (`libultraship/cmake/dependencies/linux.cmake`); BZip2 development libraries are also unavailable. No complete executable, Windows/MSVC link, game archive generation, GPU rendering, or actual save-load play session was performed in this environment. The `full-configure.log` records the exact endpoint. `GameInteractor.cpp`, `BenMenu.cpp` and `BenGui.cpp` were also attempted independently: after supplying official spdlog v1.15.3 headers (commit `6fa36017cfd5731d617e1a934f0e5ea9c4445b13`; the baseline does not pin a spdlog version), all three stop at missing `SDL2/SDL.h` through `Ship::Context`. No complete compile of these three units is claimed; their final native integration remains a Windows build gate.

`Build-BenDrowned.cmd` invokes the supplied checked PowerShell helper. On the user's Visual Studio 2022 C++/CMake environment it first builds and runs BEN and RPG core/real-JSON tests and the BEN world draw linkage fixture, then generates the port archive, builds the full `2ship` executable, and stages an isolated release using the upstream install rules. It refuses packaging if any gate or required asset is missing. The helper itself has not been executed on Windows here. It never replaces an existing installation or touches existing saves.

## Reproduce the headless checks

With Python 3, GCC/G++ (or compatible Clang), and nlohmann JSON installed:

```sh
python3 tests/ben_drowned/run_tests.py --sanitizers --compile-engine --require-json
python3 tests/player_bots/run_tests.py --sanitizers
python3 tests/rpg_framework/run_tests.py --sanitizers --compile-engine --require-json
```

If nlohmann JSON is not in a default include directory, pass `--json-include /path/containing/nlohmann/json.hpp` using the **parent include directory**, for example `/opt/nlohmann/include`. The scripts never substitute a fake JSON implementation. `--require-json` makes a missing JSON dependency a failure.

Use `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` only in an environment where LeakSanitizer itself is unsupported. CMake is needed by the RPG source/copy-helper check. BEN/RPG `CMakeLists.txt` also expose `Ben.Core`, `Ben.SaveMigration`, `Rpg.Core`, and `Rpg.SaveMigration` through CTest on a configured native build.

To reproduce the real-ImGui presentation fixture, obtain the pinned upstream `v1.91.9b-docking` source (headers and implementation) and run:

```sh
python3 tests/ben_drowned/run_ui_tests.py --sanitizers --imgui-source /path/to/imgui --json-include /path/to/json/include
```

This creates neutral-background PPM QA renders in `build-ben-ui-tests`; they are not in-game screenshots. Its Ship host and snapshot queue are explicit fixture doubles; the ImGui implementation and BEN presentation code are real.

## Required native acceptance pass

Use a copy of the working installation and backed-up save files; version 10 is intentionally rejected by older executables.

1. At title/file selection, enable BEN DROWNED. Load a three-heart fresh file and an established RPG file. Confirm initial guidance, subtitle visibility, casebook journal, and that the setting changed during play is shown as pending for the next load.
2. Enter Clock Town, an unsafe overworld map, and a dungeon. Confirm towns receive noncombat hauntings; the apparition avoids walls/ledges/doors; encounters respect existing population, collider, and scene limits. Visit multiple rooms and return to title repeatedly.
3. Trigger watcher/gaze, avoid-looking, stillness, travel, approach, retreat and orbit trials. Confirm their UI objectives match behavior, the statue moves only under its allowed rules, and the existing native Elegy puzzle remains unchanged.
4. Observe marked BEN enemies fighting Link and regular NXT bots. Confirm bot hits do not damage Link, ordinary actors and the world-boss raid are not retired by BEN cleanup, and unavailable spawns turn into a movement trial.
5. Test Mirror World with the user's regular Mirror World option both off and on. Verify maps, UI/nameplates, audio panning and view stay consistent; pause, suspend BEN, change room, die, reload and leave gameplay. The temporary mirror and native fog/light must always restore.
6. Use each response, then build trust with kindness and mercy. Verify only one response per encounter and an earned truce ends current threats. Suspend/resume from the settings while the game is paused.
7. Save normally via owl and Song of Time, quit and reload. Verify BEN's deck/relationship/progress and the existing RPG progression survive independently per file. Confirm no autosave or inventory/quest/clock corruption occurs.
8. Check 4:3, widescreen and small-window UI, controller navigation, lower-power hardware with bots/encounters active, reduced effects, and audio toggles. Headless arithmetic/geometry checks cannot establish the final GPU appearance, asset loading, timing or player experience.
