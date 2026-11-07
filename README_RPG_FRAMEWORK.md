# 2Ship RPG Framework — Player Bots expansion

Prepared 11 October 2026. Update target: the working Player Bots source previously
produced from `MajoraWithBots.rar` plus `MajoraWithBots_PlayerBots_Update.zip`.

**Delivery status: implemented source update, not a prebuilt executable or an
in-game-verified release.** The RPG gameplay objects and the headless regression
suites compile/run here. The complete Windows application, graphics/UI integration,
and live save/gameplay acceptance still need verification. The Windows helper below
builds the executable and packages it on your machine, with explicit failure checks.

## Keep the working version and its saves

Make a separate copy of your **working bot source and working game installation**.
Back up the actual `saves` directory too. The existing **Open App Files Folder**
button opens the location that contains the game's saves/configuration; it may not
be the folder containing the executable in a non-portable installation.

This update adds an RPG section to each save and raises the outer save format from
**8 to 9**. Existing version-8 saves are migrated when loaded by the new executable.
The old executable rejects newer-format saves. To roll back, restore the old
executable **and the backed-up version-8 saves**, not just the executable. Disabling
the RPG feature does not convert a version-9 save back to version 8.

RPG levels, XP, kill counters, random state, and the world-boss cooldown follow the
normal save lifecycle. **Use the normal owl or Song of Time saves to keep progress.**
There is no new per-kill autosave. Closing without saving can lose XP earned since
the last applicable game save. Temporary enemies, bot positions and boss fight HP
are not written into saves.

## Apply this source update

1. Work in the copy of the successful **bot source**, not in the installed game's
   Release folder and not in an unmodified upstream checkout.
2. Merge the update ZIP's `MajoraWithBots` contents into that source copy. Replace
   the matching files. `CMakeLists.txt`, `mm`, `libultraship`,
   `Build-RpgFramework.cmd`, and `Build-RpgFramework.ps1` must be together.
3. Build in the new `build-rpg-framework` directory using the new helper below.
   Do not copy old object files into it: `SaveContext`/`ShipSaveInfo` changed layout.
4. Test the newly packaged installation with backed-up saves. Keep the previous
   bot-only installation separate and untouched.

The ZIP is a changed/new-files overlay. It does not replace unchanged assets,
submodules or dependency trees in your working source. The accompanying `.patch`
is an **alternative** against the same working bot baseline; do not apply both.
The manifest supplies before/after SHA-256 hashes of each update file.

## Build and package Windows Release

Use the same Visual Studio 2022 C++ / MSVC v143 / Windows SDK / Python / Git / CMake
installation that built your working bot version. See the original
`docs/BUILDING.md` for that source snapshot's dependencies. CMake 3.26+ is required.
Dependency retrieval can require internet access.

**Double-click `Build-RpgFramework.cmd` in the merged source folder.** It starts
PowerShell with a process-local execution-policy option, builds Release, and keeps
the window open so errors remain visible. It does not change the machine's saved
PowerShell policy or launch the game.

The equivalent PowerShell command, from the merged source directory, is:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Build-RpgFramework.ps1" -Jobs 4
```

To copy your already-working game asset archive into the new package as well:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Build-RpgFramework.ps1" -Jobs 4 -GameAssetsDirectory "C:\path\to\your\working\Release"
```

That option reads **only `mm.o2r`** from the supplied directory. It does not copy the
old executable, saves, configuration, mods or ROM into the new release.

The helper runs these stages in order and stops on failure:

- Configure an isolated Release build; compile and run the RPG core and **real
  nlohmann JSON/save-migration tests** using the Windows toolchain.
- Generate `2ship.o2r`, compile/link `2ship`, and stage the upstream `2s2h` install
  component in a new timestamped folder.
- Verify the installed executable matches this build and check the port archive,
  controller database and extraction XML. Copy same-build DLLs when present.
- Write a startup note, hashes and a ZIP under `rpg-releases`.

After success, use the printed folder/ZIP:

```text
rpg-releases/2Ship-RPG-Windows-x64-<timestamp>-<id>/
rpg-releases/2Ship-RPG-Windows-x64-<timestamp>-<id>.zip
```

Run **that folder's `2ship.exe`** with its generated **`2ship.o2r` beside it**. Use
your existing supported `mm.o2r` or the normal supported-ROM extraction flow. No
Nintendo game assets or runnable game binary are supplied in this source update.
Do not substitute the old executable: the RPG implementation is compiled code,
not a file to put in `mods`.

A complete helper run establishes a local compile/link/test/package result, not
completed live-game acceptance. The helper was reviewed but could not be executed
under PowerShell/MSVC here. Preserve its transcript when investigating a failure;
start with the first error, not only the final package-failed line.

## What the expansion implements

| System | Default behavior |
| --- | --- |
| Character progression | Level 1–99, per-save XP and kill counters; multi-level gains supported |
| Character stats | HP, Attack, Defense, Magic, Resistance, Agility and Critical chance |
| Ordinary enemies | Eligible families roll around Link's level, normally within ±2; 85–115% stat variance |
| Player bots | Regular and dedicated raid bots use Link's level with form-specific stat profiles |
| Berserk monsters | 10% of eligible new enemy lives; stronger, 1.75× ground speed, 1.65× animation rate, audited offensive waits accelerated |
| Recovery | 3% maximum HP per active second after six seconds without damage; never revives a dead player |
| HUD | XP/level/HP strip, level-up messages, supported target stats, world-boss HP/cast/raid information |
| World boss | One giant skeleton encounter in Termina Field, the **Ossuary Colossus** |
| Dedicated raid | 12 extra helpers by default, adjustable to 16; separate from the regular roaming population |
| Encounter budget | Default 10-minute HP budget, adjustable 3–20 minutes; actual duration depends on real damage/uptime |
| Boss return | Default 15-minute active-play cooldown after defeat; saved with normal progression |

Open **Enhancements → RPG Framework** for controls and the character sheet. Leave
**Player Bots → Enable Player Bots** and **Bots Fight Creatures** enabled for the
world boss. The RPG master switch, XP rate, level spread, berserk chance/effects,
regeneration, HUD, raid size, boss HP budget and cooldown have dedicated settings.

The native heart pool remains authoritative. The numeric RPG HP display maps onto
that pool; level/stat growth changes damage calculation without awarding permanent
heart containers. Fairy behavior, heart containers and ordinary healing remain
native systems. Physical and elemental damage use different RPG defenses.

Player damage tags an eligible monster for XP. A nearby recent player contribution
can still earn credit when a bot lands the finishing hit. **Watching bots kill
untouched monsters does not generate XP**. Removing an actor on a room transition
or weather despawn does not count as a player kill. Berserk kills award extra XP.

## Finding and fighting the world boss

Enter **Termina Field** and allow the regular collision-navigation graph to build.
The director searches for a sufficiently broad, dry, reachable area away from
nearby NPCs and exit floors. Its placement is selected from validated geometry,
not from one fixed landmark. Spawning is deferred when space or actor capacity is
insufficient; a boss is not forced into unsafe geometry.

Approach the giant skeleton to start its rally. The boss uses a six-times-scale
native Stalchild model and its animations, under a new encounter actor rather than
Captain Keeta's quest actor. Dedicated helpers arrive in a staggered wave, approach
and attack it, evade marked attacks, and can be defeated and replaced.

The three attack patterns are a forward **Bone Cleave**, a surrounding
**Gravequake**, and a **Death Knell** ring with a safer inner space. Floor telegraphs
and a cast bar precede impacts. At low health it enrages. Boss HP and its stat budget
are locked for each attempt so incoming helpers or a mid-fight player level-up do
not heal it. Leaving the fight or prolonged inactivity causes a return/reset.

The added raid roster departs after the kill or reset; the **regular roaming bots
are not drafted into the raid or despawned to make room**. Their normal reinforcement
budget does not count the raid helpers. Boss XP requires recent nearby player
participation, not spectating a bot-only kill. The long-fight HP budget is a tuning
estimate, not a promise that every attempt will take precisely ten minutes.

## Safe maps and coverage limits

**The existing safe-map enemy-spawn policy is unchanged. No new monsters are added
to Clock Town, houses, shops or other non-combat maps.** Dungeon room-clear behavior
and the ordinary reinforcement whitelist remain the bot baseline's behavior.

The RPG layer supports **24 ordinary enemy families**, not every enemy/boss/minigame
actor. Native story bosses, timed/stealth challenges and quest-bound actors are not
rewritten as RPG fights. The normal bots retain their seven-family combat adapters;
the dedicated raid helpers add the new custom boss encounter. Extra ordinary enemy
spawning still uses the previous supported species selection.

Faster berserk attack waits are explicitly integrated in those seven bot-combat
families. Other eligible families receive stat/movement/animation modifiers without
blindly double-ticking their scripted state machines. Actual movement/attack behavior
and elemental variants require live-game checks.

This is an offline RPG progression/combat/encounter framework, not network
multiplayer or a replacement quest, equipment, class, trade or economy system.
Unloaded maps are not simulated. Existing traversal limits, resource replacements
and vanilla enhancement interactions still require testing. Other cheats, damage
multipliers, mods and untested platforms are not claimed to be compatible.

Read `docs/RPG_FRAMEWORK_VALIDATION.md` for exactly what ran, what did not, and the
manual acceptance checks. `docs/RPG_FRAMEWORK.md` documents the implementation and
balancing controls.
