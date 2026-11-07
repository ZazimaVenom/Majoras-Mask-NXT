# RPG Framework — validation record

Prepared 11 October 2026 (Australia/Melbourne).

**This is an implemented source update, not an in-game-verified or prebuilt game
release.** The user verified the earlier bot-only baseline in-game; that does not
validate the new RPG code. Results below refer only to checks actually executed
against this update in the Linux build environment.

## Executed results

| Check | Result | Boundary |
| --- | --- | --- |
| Production RPG core policies | PASS — 679,312 assertions | AddressSanitizer + UndefinedBehaviorSanitizer |
| Production RPG runtime fixture | PASS — 78 assertions | UBSan; explicit engine/service doubles |
| Production world-boss fixture | PASS — 7,349 assertions | UBSan; explicit assets/engine/helper doubles |
| Production raid-roster fixture | PASS — 92 assertions | UBSan; actual PlayerBots implementation with engine doubles |
| Gameplay translation units | PASS — 19 object compilations | Actual project headers and production C/C++ sources; not full linking |
| Source wiring/archive copy | PASS — 29 assertions | Includes real CMake script execution using synthetic archive bytes |
| Original bot core regressions | PASS — 42,334 assertions | ASan + UBSan; graph/pathfinding and original policy tests |
| Original bot combat fixture | PASS — 88 assertions | UBSan; explicit engine doubles, RPG disabled |
| CMake isolated-output fragment | PASS | Synthetic target configured/compiled with Ninja Multi-Config; not MSVC/game |
| RPG/bot undefined-symbol audit | PASS | No unresolved anonymous-namespace references; not complete symbol resolution |
| Linux shell helper | PASS — `bash -n` | Shell syntax only |
| Real JSON/schema migration fixture | **NOT RUN** | Real `nlohmann/json.hpp` was unavailable; no substitute JSON was used |
| Full application configure | **BLOCKED / exit 1** | Existing Linux helper could not find `lsb_release` |
| Windows/MSVC, GUI and actual gameplay | **NOT RUN** | No complete Windows toolchain, app dependency/resource set or live game here |

The core's deterministic sample produced **9,966 berserk rolls out of 100,000**
at a 10% setting. It also exercised glancing-hit distributions, all player levels,
bounds, overflow saturation, health mirroring and encounter policy geometry.
These are test results for the implemented RNG/policy, not a measured live-game
spawn distribution.

The production encounter fixture completed its long-form raid simulation in
**364.5 seconds** with a fixed synthetic contact cadence. That verifies progression
through a prolonged encounter rather than an immediate death; it does **not**
establish actual time-to-kill with real animation, navigation, collisions or gear.
The default ten-minute HP-budget setting remains a tuning estimate.

The runtime tests exercise damage/assist ownership, duplicate-death prevention,
actor reuse/native revival, reflected projectiles, high-level floating HP versus
byte health, berserk attack scopes, render-only color filters, incoming-player
metadata expiry, regen/death/pause guards, independent save-slot state and
snapshot-only HUD refresh when actor simulation is paused.

The roster fixture exercises extra-slot allocation without changing normal slots,
independent boss target routing, friendly-fire rules, living/dead/retiring status,
raid damage/invulnerability and cleanup. These checks do not make the engine
doubles a substitute for actual in-game navigation and collision execution.

## Executed command and toolchain

GCC/G++ 14.2.0 and CMake 3.31.6 on Debian Linux. C++20/C11 compatibility definitions
and include paths come from the uploaded project. Port-specific N64 resource
pointer conversions retain the original compatibility handling.

The final consolidated RPG run was:

```sh
python3 tests/rpg_framework/run_tests.py --compile-engine --sanitizers \
  --output /mnt/data/rpg_validation/final_complete
```

The original bot regression run against the updated source was:

```sh
python3 tests/player_bots/run_tests.py --sanitizers \
  --output /mnt/data/rpg_validation/bot_regression
```

Text logs, command records and the explicit JSON `NOT_RUN` status are copied into
`tests/rpg_framework/validation/`. Absolute paths identify this validation
workspace; they are not required installation paths. Test executables and object
files are intentionally not distributed as game binaries.

## Exactly which production gameplay units compiled

```text
mm/src/code/z_actor.c
mm/src/code/z_collision_check.c
mm/src/code/z_play.c
mm/src/code/z_scene.c
mm/src/code/z_actor_dlftbls.c
mm/src/code/z_parameter.c
mm/src/code/z_skelanime.c
mm/src/overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.c
mm/src/overlays/actors/ovl_En_Dodongo/z_en_dodongo.c
mm/src/overlays/actors/ovl_En_Firefly/z_en_firefly.c
mm/src/overlays/actors/ovl_En_Neo_Reeba/z_en_neo_reeba.c
mm/src/overlays/actors/ovl_En_Slime/z_en_slime.c
mm/src/overlays/actors/ovl_En_Tite/z_en_tite.c
mm/src/overlays/actors/ovl_En_Wf/z_en_wf.c
mm/src/overlays/actors/ovl_En_Fz/z_en_fz.c
mm/src/overlays/actors/ovl_En_Snowman/z_en_snowman.c
mm/2s2h/Enhancements/PlayerBots/PlayerBots.cpp
mm/2s2h/Enhancements/RpgFramework/RpgFramework.cpp
mm/2s2h/Enhancements/RpgFramework/RpgWorldBoss.cpp
```

`z_actor_dlftbls.c` is unchanged but included to validate the appended profile's
actor-table compilation. The full application GUI/save units (`BenGui.cpp`,
`BenMenu.cpp`, `RpgHud.cpp`, `SaveManager.cpp` and JSON consumers/migration) were
**not compiled here** because their real dependency set was missing. Their API
integration was inspected, but inspection is not a successful compile.

Engine fixtures include the real implementation and engine headers, then replace
specified engine services/resources with explicit doubles. Unused renderer
functions are removed with linker section garbage collection. Those fixtures run
UBSan; ASan global roots retain otherwise-unused native ActorProfiles/renderers and
are not used for them. The core runs both sanitizers. There was no whole-game
sanitizer run or full-application link test.

## Full-build blocker

The complete configuration attempt used:

```sh
cmake -S . -B /mnt/data/rpg_validation/full_build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_RPG_TESTS=ON -DRPG_ISOLATED_OUTPUT=ON
```

It stopped in `CMake/Utils.cmake:238`:

```text
Could not detect lsb_release executable, can not gather required information
```

The environment also lacked the full GUI/audio/resource dependency set and real
nlohmann JSON headers. Dependency acquisition attempts did not provide them.
No ROM/extracted game resources were available for live testing. This is why a
finished game binary is not included, not evidence that every later configure,
compile, link or runtime step will succeed.

## Save and Windows release test gates

`save_tests.cpp` uses **actual** nlohmann JSON and production `RpgJson.h` plus
`SaveManager/Migrations/9.cpp`. It is written to check lossless XP round trips,
invalid/future schemas, non-mutating parse failures, slot/snapshot isolation and
idempotent version-8 migration. It is present but was not executed locally.

`Build-RpgFramework.ps1` enables `BUILD_RPG_TESTS`, builds `RpgCoreTests` and
`RpgSaveTests`, and runs CTest before building/staging the game. Missing dependencies,
a failed assertion or a failed compile stops that helper. This is an enforced
future build gate, **not a claim that the test already passed on Windows**.
PowerShell/MSVC execution and the complete ZIP package flow remain untested here.
The archive-copy script itself was executed with known synthetic data, and the
isolated-output CMake fragment was checked using a small synthetic target.

## Manual acceptance checklist — not performed here

| Area | Required in-game observation |
| --- | --- |
| Clean Release/package | Complete all Windows build gates; start the new executable with generated `2ship.o2r` and supported game data |
| Old save migration | Back up a version-8 save; load, check native progression and level 1, save/reload without losing either Zelda or RPG state |
| Slots and snapshots | Two independent save slots; owl save/reload; Song of Time; game over; application exit; verify the expected snapshot's XP and cooldown |
| HUD | XP/HP/target/raid panels within the game viewport; no menu/cutscene persistence; multiple aspect ratios, UI scaling and existing HUD settings |
| Player progression | Ordinary kills, multi-level grants, native heart pickups/containers, fairy revival, physical/elemental damage and game over |
| XP ownership | Player kill, recent bot-assisted finish, expired/far-away assist, spectator-only kill and actor despawn; no duplicate credit |
| Native enemies | Each listed family/subtype; native weakness/finisher, revival, freeze/stun, projectiles, day/night and room-clear behavior |
| Berserk effects | Visible pulse/particles without persistent invulnerability, accelerated motion/attack timing, effect toggle, ordinary-enemy contrast |
| Regeneration | Damage resets delay; regen after default six seconds; no healing progression in pause/dialogue; no spontaneous revival |
| Safe areas | Clock Town, houses, shops, inns and excluded scenes never receive new monsters or the world boss |
| Boss placement | One boss only; open dry reachable field site; no exit/NPC blocking, wrong-room spawn or forced teleport through geometry |
| Separate helpers | Normal roaming count/activity retained; extra roster gathers, damages boss, evades, takes damage, dies and is replaced |
| Encounter | All three telegraphs line up with real impacts; native invulnerability respected; simultaneous helpers do damage; low-HP enrage works |
| Defeat/reset | Player-contributed XP once; no spectator reward; helpers depart; boss cooldown survives save/load; leave/wipe/reset and interrupted attempts |
| Disabled features | Toggle RPG, world boss, bots and combat; no orphan boss, persistent raid helpers, duplicate spawns or corrupted progression |
| Stress/compatibility | Maximum regular/raid/reinforcement populations, effects-heavy field, long sessions, room changes, interpolation and resource replacements |

Do not present this source candidate as a finished, gameplay-verified release until
these integration checks have actually been completed.
