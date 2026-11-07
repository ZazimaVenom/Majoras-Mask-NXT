# RPG Framework — technical and balancing reference

## Baseline and scope

This is incremental source against the prior Player Bots overlay, itself based on
uploaded `MajoraWithBots.rar` at upstream commit
`a89c41a5f432d54bdd9d94ec88f87f8a9a44c847`. It is not a patch against unmodified
upstream. No stock executable is compatible with these source-only changes.

The feature is an offline character-progression, ordinary-combat scaling, bot
scaling and cooperative world-boss layer. Native Zelda quests, dungeons, inventory,
heart containers, save slots and resource loading remain in place. Multiplayer,
trading, equipment tiers, class selection and a new quest engine are not implemented.

## Ownership and lifecycle

`RpgSaveData.h` is a 48-byte, versioned POD appended to `ShipSaveInfo`. It stores
level, within-level/lifetime XP, credited/berserk/world-boss kill counters, a
persistent RNG state and remaining active-play boss cooldown. No actor pointers,
colliders, heap objects, current fight HP or UI state are serialized.

`RpgCore.h` holds deterministic policy/formulas usable in production and standalone
tests. `RpgFramework.cpp` owns an `Actor*`-keyed temporary combat ledger. Actor
creation/first valid update supplies a record, actor destruction removes it, and
native revival establishes a fresh life. Scene/file identity transitions clear
runtime records without resetting that save's progression. Damage contact metadata
copies attacker stats/flags rather than retaining collider or attacker pointers.

`RpgWorldBoss.cpp` keeps engine-allocated actor memory POD. Its heap-owned encounter
contains STL collections, timers and presentation data; reset/destruction explicitly
invalidates the encounter and its actor references. `PlayerBots.cpp` owns ordinary
and raid bot slots with distinct role flags and lifetime rules.

`Rpg_Update` advances only on the actor-simulation path. `Rpg_RefreshHud` additionally
publishes on `Play_Draw`, because paused frames can skip `Actor_UpdateAll`. This
snapshot-only call never advances regen, cooldowns, encounter time or XP. Rendering
reads a mutex-protected POD snapshot, not actors or writable save state.

## Save schema

The outer 2Ship save version becomes **9**, while the optional RPG section's own
schema is **1**. `SaveManager/Migrations/9.cpp` adds a default RPG object only when
absent in each migrated owl/new-cycle snapshot. Existing Zelda values and an
already-existing RPG object are not replaced by that migration.

`RpgJson.h` defines real nlohmann JSON ADL conversions for the global C POD.
Decoding validates unsigned fields, rejects negatives, fractions, booleans and
out-of-range values, and commits only after successful parsing. Normalization
bounds level/XP/cooldown, repairs a zero RNG state and clears reserved data. Unknown
future RPG versions are not silently rewritten. This does not replace the upstream
save-file writer or claim that its disk writes are atomic.

Normal owl/new-cycle save semantics determine which snapshot is restored. XP is
not autosaved per monster. Existing save loading recomputes native checksums after
migration. All dependent translation units must be rebuilt because native struct
layout/size changed. Never open your only copy of a save for acceptance testing.
Older format-8 executables require the pre-update save backups for rollback.

## Player stats and damage

Let `L` be level clamped to 1–99 and `n = L - 1`. `H` is owned hearts beyond the
initial three, clamped to 0–17. Default base stats are:

| Stat | Formula |
| --- | --- |
| Maximum HP | `120 + 20n + 0.75n² + H(20 + 1.5L)` |
| Attack | `18 + 4n + 0.10n²` |
| Defense | `9 + 2n + 0.055n²` |
| Magic | `16 + 4.2n + 0.11n²` |
| Resistance | `9 + 2n + 0.05n²` |
| Agility | `10 + 0.5n` |
| Critical chance | `5 + 0.08n` percent |

Numeric HP represents the native heart pool's fraction of RPG max HP. Native heart
ownership is not increased on level-up. Converted damage uses ceiling/positive
bounds so a valid RPG hit cannot become an accidental zero-damage native event.
Original environmental damage, native fairy behavior and health pickups remain
native. Unsupported native bosses retain their native damage paths rather than
being silently assigned arbitrary RPG health.

Native weapon damage supplies a power factor `0.4 + 0.6 * nativeDamage`, clamped
to 1–7. Physical attacks use Attack/Defense; elemental arrows and Zora barrier use
Magic/Resistance. Mitigation is `0.45 + 0.55 * offense / (offense + 0.85 * defense)`.
Hits vary by ±8%; critical hits deal 1.5× damage. A defending Agility advantage can
produce a 0.65× glancing hit, capped at 14% chance. This softens actual hits rather
than falsifying collision misses. Native damage-table weaknesses and special
execution paths are retained where adapted.

Bot profiles use Link's current level. Goron profiles favor HP/defense, Deku
profiles have lower HP, and form-dependent physical/magic factors differentiate
other forms. Raid helpers have an additional 1.45× HP and 1.1× defense allowance.
A live bot's health fraction is preserved when recalculating after a player level-up.

## Enemy lives and coverage

At registration, a supported enemy rolls a triangular level deviation around Link,
default ±2 (configurable 0–4), then an 85–115% stat multiplier. Native maximum HP
influences the family's body-size/HP factor. Per-life stats are locked: Link gaining
a level does not refill an existing monster's HP. A floating HP ledger avoids
putting large RPG health values into the engine's native byte counter; a bounded
native mirror supports existing state machines and native death handling.

The following 24 families are eligible subject to scene, category, room,
initialization, parent and cutscene guards:

| Actor family | Display name | Ordinary bot combat adapter |
| --- | --- | --- |
| `EN_DEKUBABA` | Deku Baba | Yes |
| `EN_WF` | Wolfos | Yes |
| `EN_TITE` | Tektite | Yes |
| `EN_SLIME` | Chuchu | Yes |
| `EN_NEO_REEBA` | Leever | Yes |
| `EN_FIREFLY` | Keese | Yes |
| `EN_DODONGO` | Dodongo | Yes |
| `EN_SKB` | Stalchild | No |
| `EN_DEKUNUTS` | Mad Scrub | No |
| `EN_OKUTA` | Octorok | No |
| `EN_PEEHAT` | Peahat | No |
| `EN_PP` | Hiploop | No |
| `EN_RD` | ReDead | No |
| `EN_FLOORMAS` | Floormaster | No |
| `EN_WALLMAS` | Wallmaster | No |
| `EN_SB` | Shell Blade | No |
| `EN_RR` | Like Like | No |
| `EN_FZ` | Freezard | No |
| `EN_KAME` | Snapper | No |
| `EN_GRASSHOPPER` | Dragonfly | No |
| `EN_SNOWMAN` | Eeno | No |
| `EN_DINOFOS` | Dinolfos | No |
| `EN_AM` | Armos | No |
| `EN_BB` | Bubble | No |

Membership is not a claim of full live validation of every subtype, attack or
scripted placement. Parent-attached or quest/cutscene-bound actors are excluded;
selected timed/stealth/boss scenes retain the existing bot exclusion policy.
The native RPG boss layer does not convert story bosses. Eligibility for RPG stats
is distinct from eligibility for additional enemy spawning. The latter retains
`BotScenePolicy.h` byte-for-byte and the previous species/director safeguards.

Deku Baba and Keese native one-hit assumptions have explicit health adapters.
Dodongo swallowed bombs can attribute player damage. Freezard/Eeno elemental death
paths and native zero-health executions have dedicated integration where needed.
The engine contact/ApplyDamage paths handle the common cases. Native environmental
removal is not inferred to be a legitimate XP-bearing kill.

## XP and regeneration

XP needed to advance from level `L` is `100 + 32n + 8n²`; level 99 has no next-level
requirement. Grants support several levels at once and saturate lifetime counters
rather than wrapping. Base ordinary-kill XP is `18 + 6L + floor(L²/12)`, adjusted
for level gap, berserk status and the configurable XP-rate multiplier.

Credit requires a damaging player contribution. An ordinary monster can be finished
by a bot within a 45-second assist window while the living player is within 2,200
horizontal game units. A boss uses a 120-second contribution window. Passive
spectators, transition despawns, zero-damage stuns and double death notifications do
not grant extra XP. Reflected player-type projectiles can be attributed to Link.

A boss completion grants twice the **current level's next-XP requirement**, adjusted
by the XP-rate setting. This is a reward budget, not a guarantee of exactly two
level-ups because successive levels cost more. Boss-only kills and regular-kill
counters are recorded separately.

Default regeneration is 3% of native/RPG maximum HP per active second after six
seconds without damage. Fractional healing accumulates until a native health unit
is available. Regen pauses for dead/full health, pause, dialogue, cutscenes,
transitions and ocarina interactions; it never triggers fairy revival. Native damage
also resets the delay. It uses normal simulation time, not interpolation frames or
wall-clock time while menus are open.

## Berserk implementation

Each eligible new enemy life rolls the configured chance, default 10%. It is not a
per-frame lottery. Berserk stats multiply HP by 1.45, Attack/Magic by 1.4, Defense by
0.9 and Agility by 1.85 relative to the ordinary rolled profile; crit becomes 10%.
Berserk kills grant twice ordinary XP before the other reward modifiers.

Ground movement uses a 1.75× multiplier and scoped skeletal animation uses 1.65×.
Audited offensive waits in the seven bot-supported families count down faster.
Whole actor updates are **never** duplicated to accelerate AI: timers governing
freezing, death, quest behavior and collision processing are not blindly sped up.
Other families therefore do not promise the same measured attack-cycle multiplier.

Presentation is a short red pulse and nearby electrical particles, plus a BERSERK
label/target color. Cosmetic pulsing is local to the draw path; it **never changes
`Actor.colorFilterTimer` or `colorFilterParams`**, which native enemies also use for
stun/invulnerability. Native active filters take priority. Effects can be disabled
without changing the enemy's stats or status. Particle drawing is distance-limited.

## Ossuary Colossus encounter

A new `EN_RPG_WORLD_BOSS` profile (`0x2B3`) uses the native Stalchild skeleton and
animations at scale 0.060 (native standard scale 0.010). It is not the quest-bound
Captain Keeta actor and does not invoke that quest's cutscene progression.

The director runs only in Termina Field (`SCENE_00KEIKOKU`) with RPG, world boss,
Player Bots and bot combat enabled. It waits for a useful navigation graph and
searches for a broad floor footprint with dry/non-exit geometry, a suitable player
distance and NPC separation. Capacity limits defer the spawn rather than evicting
normal actors. Actual placement can vary; there is no fixed coordinate promise.

An approach starts an eight-second rally. The encounter locks base player level,
raid size, boss stats and HP budget for that attempt. Boss level is Link+3 capped at
99. HP is budgeted from expected raid/player DPS, a 55% uptime factor and the selected
3–20-minute setting; minimum HP is 2,200. Real time-to-kill depends on contact rate,
gear, navigation, deaths and participation. The default is a tuning target, not a
stopwatch-enforced ten-minute fight.

| Attack | Footprint | Windup / recovery before enrage |
| --- | --- | --- |
| Bone Cleave | Forward cone, radius 275, roughly 70° half-angle | 1.5 / 2.6 seconds |
| Gravequake | Surrounding disk, radius 340 | 2.7 / 3.5 seconds |
| Death Knell | Ring from radius 145 to 465 | 3.1 / 4.0 seconds |

Telegraph geometry is prepared on phase changes and impacts occur exactly once on
the transition. Range, height, line of sight and native player invulnerability are
checked. The boss slows/stops to telegraph rather than silently striking every
render frame. Lower-health rotation adds area pressure; below 30% health the boss
enrages and shortens windup/recovery timing. The HUD exposes HP, phase/cast progress,
enrage and active helper count.

Sixteen dedicated helper slots exist **in addition** to the twenty normal slots.
Default target is twelve, spawned in staggered intervals. Helpers use their own
names, scaled forms, target routing and combat lifecycle; regular bots continue
roaming/fighting their native targets. Helper deaths can be replaced after a delay.
Evasion has a reaction/participation factor so not every helper perfectly dodges.
Raid helpers are excluded from the normal extra-monster budget.

Multiple helpers' actual collider contacts are collected before the native
single-last-hit AC slot can discard concurrent attackers. Per-attacker short
cooldowns prevent duplicate contacts from dealing repeated same-swing damage.
A player-type projectile uses Link's stats. Ordinary roaming bots are not forced to
attack the boss through their normal target search.

The boss returns when the player remains too far away, leash distance is exceeded,
or there are no hits for an extended interval. The added helpers retire, the boss
walks home and the attempt resets. A persistently blocked return retires the boss
and lets the site director try again, rather than teleporting through geometry.
Defeat enters a short death presentation, retires helpers and starts the saved
active-play cooldown. Boss death/teardown paths cannot award the same victory twice.

## Controls

All keys below are prefixed with `gEnhancements.Rpg.` and are available under
**Enhancements → RPG Framework** unless noted otherwise.

| Key | Default | Range / meaning |
| --- | --- | --- |
| `Enabled` | 1 | Master RPG enable; does not downgrade saves when disabled |
| `XpRate` | 100 | 25–500 percent |
| `LevelSpread` | 2 | 0–4 levels in each direction |
| `BerserkChance` | 10 | 0–100 percent; affects new lives |
| `BerserkEffects` | 1 | Cosmetic effects only |
| `Regeneration` | 1 | Native-pool recovery |
| `RegenDelay` | 6 | 2–20 active seconds |
| `RegenPercent` | 3 | 1–8 percent max HP per second |
| `Hud` | 1 | XP/HP/encounter overlay |
| `TargetHud` | 1 | Ordinary target panel |
| `HudScale` | 100 | 75–150 percent |
| `BossHudY` | 18 | 8–40 percent of game viewport height |
| `WorldBoss` | 1 | Termina Field encounter director |
| `RaidHelpers` | 12 | 0–16 extra helpers; locked per attempt |
| `BossMinutes` | 10 | 3–20 minute HP budget; locked per attempt |
| `BossCooldownMinutes` | 15 | 1–60 active-play minutes after defeat |

The master switch preserves progression. Turning the encounter or bot support off
retires the encounter rather than leaving a live boss with no functioning helper
system. Existing normal bot settings retain their previous meanings. Boss budget
and roster setting changes apply to the next attempt, not a mid-fight HP reset.

## Build and validation boundaries

The source overlay includes a fresh Windows Release/package helper, isolated target
output selection, explicit CTest core/save gates and a fixed archive-copy script.
It cannot prove that a local Windows toolchain/dependency set will configure or that
in-game models, collisions and all enhancement combinations behave correctly.
See `RPG_FRAMEWORK_VALIDATION.md` for executed commands, counts and outstanding
acceptance. No runtime binary, Nintendo assets, test object files or fonts are
included in the source overlay.
