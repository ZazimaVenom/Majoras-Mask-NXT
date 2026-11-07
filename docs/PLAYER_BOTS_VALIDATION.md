# Player Bots — validation record

Date: 11 October 2026 (Melbourne).
Baseline: uploaded `MajoraWithBots.rar`, Git commit
`a89c41a5f432d54bdd9d94ec88f87f8a9a44c847`.

**This is a source build candidate, not an in-game-verified release.** The tests
below were actually executed. A complete executable was not built or run.

## Executed results

| Check | Result | What it establishes |
| --- | --- | --- |
| Core production-header tests | PASS — 42,334 assertions | A*, graph guards, capacity arithmetic, scene policy, RNG and refill timing |
| Random graph comparison | PASS — 100 graphs / 1,200 searches | A* results compared against independent Dijkstra paths |
| Core sanitizers | PASS — AddressSanitizer + UndefinedBehaviorSanitizer | No reported memory/UB faults in the exercised core tests |
| Headless production combat API | PASS — 88 assertions | Target selection, priority, metric restoration, collision filters, lifetime handling and scoped object policy under explicit test doubles |
| Combat API sanitizer | PASS — UndefinedBehaviorSanitizer | No reported UB in those exercised headless code paths |
| Gameplay translation units | PASS — 13 units compiled to object files | Real uploaded engine headers and changed gameplay source accepted by GCC/G++ |
| Bot object undefined-symbol audit | PASS | No unresolved anonymous-namespace symbols; not a full link check |
| Shell helper syntax | PASS — `bash -n` | Shell syntax only, not a completed game build |

### Exactly what was compiled

GCC/G++ 14.2.0; C++20 and the source snapshot's C compatibility definitions/include
paths. The runner preserves the original port's handling of N64 resource pointer
conversions. These are object compilations, not an executable build:

```
mm/src/code/z_actor.c
mm/src/code/z_collision_check.c
mm/src/code/z_play.c
mm/src/code/z_scene.c
mm/src/code/z_actor_dlftbls.c
mm/src/overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.c
mm/src/overlays/actors/ovl_En_Dodongo/z_en_dodongo.c
mm/src/overlays/actors/ovl_En_Firefly/z_en_firefly.c
mm/src/overlays/actors/ovl_En_Neo_Reeba/z_en_neo_reeba.c
mm/src/overlays/actors/ovl_En_Slime/z_en_slime.c
mm/src/overlays/actors/ovl_En_Tite/z_en_tite.c
mm/src/overlays/actors/ovl_En_Wf/z_en_wf.c
mm/2s2h/Enhancements/PlayerBots/PlayerBots.cpp
```

`z_actor_dlftbls.c` is unchanged but was compiled to verify the appended profile's
table integration. The two edited GUI-related C++ units (`BenMenu.cpp` and
`NameTag.cpp`) were reviewed against the existing APIs but **not compiled** in this
environment because their full application dependencies were unavailable.

### Headless test boundary

`combat_api_tests.cpp` includes the production implementation and actual engine
headers, with explicit doubles for configuration, player-mask/cutscene checks,
line-of-sight, actor distance/yaw, and color filtering. Unused renderer/population
functions are discarded by linker section garbage collection. This exercises
real helper/state/filter code, but does **not** execute the complete engine,
real asset loading, full native enemy AI ticks, actual collision solving, rendering,
or live navigation through a game map.

The core suite runs ASan and UBSan. The integration fixture runs UBSan only: ASan's
global registration retains the actor profile and its otherwise-unused engine
callbacks, defeating this test's renderer-independent linkage. No claim of a
complete-game sanitizer run is made.

## Full build attempt and blockers

The attempted command was:

```sh
cmake -S . -B /mnt/data/playerbots_validation/full_build -DCMAKE_BUILD_TYPE=Release
```

CMake 3.31.6 detected the C and C++ compilers, then stopped in the existing Linux
configuration helper because the environment lacked `lsb_release`:

```
Could not detect lsb_release executable, can not gather required information
```

The environment also lacked the complete GUI/audio/resource dependency set.
Dependency installation could not proceed because the Debian repository hostname
could not be resolved. The supplied archive did not contain a runnable executable
or the extracted game-data archives needed for interactive testing.

Consequently there is no evidence here that the complete modified application
links, builds with MSVC, renders every form correctly, or behaves correctly in all
maps. Those are remaining integration requirements, not implied by the passing
object compilations. No prebuilt game binary is supplied.

## Reproduction

From the merged source tree on Linux with Python 3.9+ and GCC/G++ (or compatible
Clang) and sanitizer support:

```sh
python3 tests/player_bots/run_tests.py --compile-engine --sanitizers
```

`CC` and `CXX` can override compilers. `--output` chooses a results directory.
Default output is `build-player-bots-tests`. The tests do not require a ROM.
The runner stops at the first failed compilation or test and writes the actual
commands, return codes, and named logs to `report.json`.

The text outputs from the completed validation run are copied into
`tests/player_bots/validation/`. Absolute paths in those logs describe the executed
validation environment, not required installation paths. Linux partial object
files and headless test executables are deliberately not included as game binaries.

## Required manual acceptance checks — NOT yet performed

| Area | Required observation |
| --- | --- |
| Fresh full build | Generate assets, compile and link every unit, package, launch modified executable |
| Clock Town and interiors | Check all four town sections, houses, shops and inns: visitors only, no extra monsters |
| Form appearance | Verify all five forms, each human mask, pose transitions, held equipment, tint and nameplates |
| Open combat areas | Check legal spread/population buildup, enemy species/variants, no clipping or spawn on Link |
| Both sides of combat | Observe supported enemies attack bots and Link; both receive actual hits, bots can defeat enemies and be defeated |
| Player priority | Approach or lock on while bots fight; confirm enemies do not ignore nearby active Link |
| Friendly fire | Link and bots cannot hurt each other; bots cannot damage story NPCs/scenery/unsupported actors |
| Dungeon progression | Check empty puzzle rooms stay empty, native room-clear triggers still work, extras do not hold doors shut |
| Lifecycle | Repeated room/scene transitions, pause, dialogue, cutscenes, ocarina, game over, save/load and Song of Time |
| Disabled settings | Disable/re-enable master/combat/reinforcements; only added actors retire as appropriate |
| Path constraints | Narrow passages, ledges, hazardous floors, water, changing collision, moving geometry and disconnected platforms |
| Stress and compatibility | 20 bots and high extra caps, effects-heavy combat, long sessions, other enhancements/resource mods/frame interpolation |

Bot kills of original enemies can legitimately trigger their normal drops and
progression flags; inspect this during dungeon and quest tests. Mask/form visuals
are not an implementation of every transformation's traversal/combat mechanics.
Do not describe this build as production-ready until the full-build and gameplay
acceptance work is complete.
