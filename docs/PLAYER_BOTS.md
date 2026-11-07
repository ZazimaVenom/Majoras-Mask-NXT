# Player Bots — implementation reference

## Scope and boundaries

This update adds source code for independent adventurer NPCs and a bounded enemy
reinforcement director. Its integration is specific to the uploaded 2Ship source
snapshot. The validation boundary is described separately; this document describes
implemented behavior, not a claim of observed gameplay in every map.

The module is `mm/2s2h/Enhancements/PlayerBots/`. `BotNavigation.h` provides the
engine-independent graph, A*, capacity arithmetic, and deterministic random number
generator. `BotScenePolicy.h` is the fail-closed combat and exclusion policy.
`PlayerBots.cpp` implements actors, poses, combat, population, and lifecycle.
`PlayerBots.h` exposes the small C bridge used by the original engine sources.

## NPC identity, state, and rendering

A new profile, `En_Player_Bot_Profile`, is appended at actor ID `0x2B2` without
renumbering existing actor IDs. Bots are NPC-category actors, not second player
actors. Each bot's POD actor allocation begins with a `Player` layout for compatible
render data and supported-enemy targeting reads. C++ runtime state is separately
owned by the director.

The real player is not replaced. Bot code does not call the normal player
initialization/update/destruction loop, drive player input, or own the save's
inventory. A fixed 20-slot roster supplies stable names and varied cosmetic tints;
actors and their current combat state are temporary.

The form cycle includes Human, Deku, Goron, Zora, and optional Fierce Deity. Human
masks are no mask, Keaton, Truth, Romani, Postman, Kafei, Bremen, and Scents. These
are curated static visuals, not every mask or every mask's gameplay ability.
Mask choices do not confer the real player's mask-based invisibility behavior.

Each bot has its own sampled animation pose. Sampling avoids submitting an entire
extra player animation job set to the shared player animation queue. Joint-angle
interpolation and short pose transitions are handled within the module. Skeletons
are registered/unregistered through the engine resource manager. Player-renderer
global form/LOD selectors are restored after drawing. Model/animation appearance,
resource replacements, and frame interpolation still require in-game testing.

Nameplates show a bot's name and health. They are rebuilt only when needed after
health/visibility changes. The existing name-tag matrix allocation is also corrected
to use matching `new`/`delete` ownership.

## Navigation and simulation

The navigation graph is derived incrementally from scene collision near the real
player's starting floor. Graph nodes are approximately 64 world units apart;
expansion considers up to eight neighbors with a maximum of 4,096 nodes. Floor,
wall, clearance, hazard/exit surface, slope, step/drop, and water checks screen both
graph construction and movement. Bot routes use A* with a two-plan-per-tick budget.
Navigation expansion defaults to eight edge attempts per tick.

The state machine includes roaming, chasing, attacking, evading, recovering,
downed, and respawn states. Movement rechecks live collision and uses soft local
separation around bots, original NPCs, and the real player. Bots deliberately do
not register ordinary overlap collision, avoiding solid crowd walls and pressure
switch activation. They are not intended to push the player out of doors.

Only the loaded scene/current room is populated and simulated. Room/scene changes
retire old generated actors and reset navigation; actor destruction invalidates
cached references before address reuse. Simulation pauses during the guarded
pause/dialogue/cutscene/transition/ocarina states. No long-lived bot records are
written into the save.

Ground navigation cannot guarantee coverage across disconnected platforms, narrow
or dynamic passages, moving platforms, ladders, jumps, or full swimming routes.
Zora form permits some reachable underwater-floor navigation, not general swimming.
The 4,096-node bound and collision restrictions can leave areas unvisited. No
performance or all-map traversal claim has been validated in the actual game.

## Supported combat

The following original enemy implementations are integrated:

| Enemy family | Native actor |
| --- | --- |
| Deku Baba | `ACTOR_EN_DEKUBABA` |
| Wolfos / supported white variant | `ACTOR_EN_WF` |
| Tektite | `ACTOR_EN_TITE` |
| Chuchu | `ACTOR_EN_SLIME` |
| Leever | `ACTOR_EN_NEO_REEBA` |
| Keese / supported elemental variants | `ACTOR_EN_FIREFLY` |
| Dodongo | `ACTOR_EN_DODONGO` |

Eligible original enemies must belong to the current approved combat area, be
alive/initialized, and have no scripted parent or cutscene ownership. Bosses and
unsupported actors do not become bot combat targets. Extra actors are explicitly
tracked separately from originals.

These seven native implementations use the target helper locally. Immediately
around an eligible enemy's update, the engine's enemy-to-player distance/yaw fields
may temporarily describe its selected bot. The actual player object and player
actor list are not swapped. Distances/yaw are restored before external post-update
hooks execute. Close real-player combat has priority (160 world units; up to 320
while locking on or attacking). Bot target selection uses distance, line of sight,
and hysteresis, with a limit of three bot pursuers per creature.

Bots use native attack/receive colliders. The collision-filter hook blocks
player-to-bot, bot-to-player, and bot-to-bot damage and rejects bot damage against
scenery, unsupported enemies, or protected scripted actors. Supported creatures
can damage bots. Damage handling uses engine-copied hit damage, never a retained
previous-frame attacker collider pointer. Bots have brief invulnerability, combat
recovery, out-of-combat regeneration, a downed state, and delayed respawn.

Current health values are 20 for Deku, 36 for Goron, and 28 for the other forms.
Incoming damage is clamped to 1–8. The respawn delay has up to five seconds of
staggering beyond the configured base. These are initial tuning values, not the
result of an in-game balance pass. Combat integration does not add player-style
item inventories, projectile arsenals, full transformation abilities, or boss AI.

## Enemy population and safe areas

`BotScenePolicy.h` contains the only approved reinforcement scenes. Unknown or
non-whitelisted scenes get no added enemies. Clock Town (all four sections), houses,
shops, and other ordinary non-combat maps can have visitors but no added combat.
The coverage report lists all defined scenes and exclusions.

The desired number of extra creatures is:

```
clamp(active_bots * enemies_per_bot - supported_original_enemies, 0, extra_cap)
```

Active bot and configuration counts are bounded before arithmetic. The default
ratio is two and extra cap 28; dungeon scenes have a separate ceiling of 12 extras.
Existing unsupported enemies are not cloned or converted into bot targets.
A dungeon also needs a supported original ordinary enemy in the current room
before a new reinforcement may be admitted. Consequently an empty dungeon puzzle
room or a room containing only unsupported scripted enemies gets no new spawns.

Original local supported species are preferred when selecting reinforcements.
Only a safe subset of variant parameters is copied: no parent, switch index, or
cutscene association. When no local supported species is available in outdoor
combat scenes, a biome-specific fallback chooses from the supported families.
No new monster models are created. Not all of the game's original combat regions
are approved; e.g. restricted stealth/scripted regions are deliberately unchanged.

Spawns require legal separated floor positions, distance from the real player and
other actors, and capacity headroom. New monsters are not spawned within 420 world
units of Link. Keese spawn clearance is checked above the floor. This can prevent
reinforcements in a confined but otherwise whitelisted room. Bots spawn in a
staggered sequence rather than all at once.

Enemy refill delay defaults to three seconds when only one enemy is missing.
Larger deficits divide that delay by up to 15, with a 0.2-second lower bound and no
more than one actual spawn per simulation tick. Failure to find a safe site delays
spawning. Existing extras are reduced if the configured desired count falls.
Original map actors are not deleted by that reduction or by disabling bots.

Extras enter the miscellaneous actor category from creation and remain outside
native enemy room-clear counting, including when Wolfos initialization requests an
enemy-category change. Their room field is detached after successful initialization
and they are explicitly retired at room changes. This prevents extra populations
from holding room-clear doors shut. **Killing an original enemy can still produce
its normal loot or room-clear/progression effects**, including when a bot kills it.
This update is not a promise that all gameplay progression remains unaffected.

A narrow spawn scope permits only the selected extra actor's object dependency to
use the persistent gameplay-keep slot when its object is absent from the room.
The actual named OTR/O2R resource remains required. Ordinary scene actor dependency
checks are unchanged. Real resource loading and every supported variant still
need validation in the complete game.

## Capacity and integration changes

The original actor-count bound is 255. New spawns stop at 210 loaded actors,
leaving 45 slots for native effects/scripts and other gameplay. Collision arrays
are expanded from 50 AT / 60 AC / 50 OC entries to 128 AT / 192 AC / 128 OC entries.
The population director records recent collision counts and stops extra admission
before the expanded arrays are full. Native registration/clear code continues to
use the array sizes; no save-file structure is extended.

The expanded runtime `CollisionCheckContext` changes `PlayState` layout. **Rebuild
the whole program in a fresh directory; do not mix old binary objects with these
headers.** Actor/collider budgets reduce a specific capacity risk; they are not a
measured guarantee of frame rate or compatibility with all other enhancements.

Integration points: `z_actor.c` (spawn, update, lifecycle and category handling),
`z_collision_check.c` (friendly-fire/eligibility filter), `z_play.c` (collision
usage snapshot and teardown), `z_scene.c` (scoped object dependency), the seven enemy
sources above, the actor table, collision header, and the Player Bots menu.
The recursive C++ source glob already includes the new module after reconfiguration.
Root CMake tolerates missing Git metadata in source archives.

## Settings

All names below have the prefix `gEnhancements.PlayerBots.`. Existing CVar
persistence handles settings; generated bot positions/health are not saved.

| Suffix | Default | Bounds / effect |
| --- | --- | --- |
| `Enabled` | 1 | Master population toggle |
| `Population` | 15 | Target 10–20, constrained by legal space/capacity |
| `Nameplates` | 1 | Names and health display |
| `FierceDeity` | 1 | Applies to newly spawned form choices |
| `RespawnSeconds` | 25 | 10–120, plus 0–5 seconds stagger |
| `Combat` | 1 | Supported bot/creature fighting in approved scenes |
| `Reinforcements` | 1 | Extra enemies in approved combat scenes |
| `EnemiesPerBot` | 2 | 1–3 supported enemies per active bot |
| `EnemyCap` | 28 | 0–32 extras; dungeon ceiling 12 |
| `EnemySpawnSeconds` | 3 | 1–30, adaptive refill pacing |
| `NavigationBudget` | 8 | 2–24 expansion attempts per tick |

## Files and provenance

This is a targeted update for the uploaded baseline, not an upgrade to another
upstream version. Existing project licensing and notices remain applicable.
No ROM, extracted proprietary game-data archive, game executable, or developer
machine build dependencies are included in the update. See the archive's change
manifest for exact file paths and hashes.
