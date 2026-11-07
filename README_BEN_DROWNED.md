# BEN DROWNED — The File Left Open

An optional, persistent haunting mode for this supplied **Majoras-Mask-NXT Bots + RPG Framework** source baseline.

This delivery is implemented source, with automated director/save/integration checks and compiled engine/UI units. It is **not a prebuilt Windows executable or a completed in-game playtest**. See `docs/BEN_DROWNED_VALIDATION.md` for the exact evidence and remaining acceptance checks.

## Start here

1. Extract the full source ZIP into a **new folder**. Keep your working game and its saves separately. This is already a merged source tree; do not also apply the optional update overlay to it.
2. Back up the actual `saves` folder, found through the game's **Open App Files Folder** option. This build adds save format **10**. Your previous RPG build uses format 9 and cannot open the newer saves. Rollback requires the old executable **and its old save backup**.
3. With the Visual Studio 2022 C++ / MSVC v143, Windows SDK, Git, Python and CMake 3.26+ setup used for your working baseline, double-click **`Build-BenDrowned.cmd`** in the extracted source root. It uses a fresh `build-ben-drowned` build directory. Do not reuse old objects: `SaveContext` changed size.
4. The helper builds and runs the BEN/RPG core and real-JSON save tests, builds the actual game and its port archive, and packages a new timestamped installation under `ben-releases`. It stops on failure. It does not launch the game or overwrite your working installation.
5. Supply your existing supported `mm.o2r` through the normal game setup, or use the optional command below. Run the newly built `2ship.exe` with its matching, newly generated `2ship.o2r`.
6. **Before loading a save**, open **Enhancements → BEN DROWNED → Enable BEN DROWNED**. This switch defaults OFF and is latched when a file loads. A change while playing takes effect after returning to file select and loading again.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Build-BenDrowned.ps1" -Jobs 4 -GameAssetsDirectory "C:\path\to\working\Release"
```

That optional directory is read only for `mm.o2r`; it does not copy an old executable, configuration or saves. Dependency retrieval can require internet access. No ROM, extracted Nintendo game archive, or executable is bundled here.

## What happens while you play

BEN is an **offline adaptive director**. He reacts to movement, lingering, recent RPG-credited kills, low health, companions, new areas, returns, trial results, and your responses. He is not an online language model. All effects remain inside the game.

The story travels through six eight-encounter acts: **A File Left Open**, **The Other Footsteps**, **A Kindness with Teeth**, **The Town Underneath**, **Who Owns the Mask**, and **You Have Been Here Before**. After 48 encounters, another loop begins. The relationship and text decks remember; there is no final exorcism that permanently removes the mode.

| System | Implemented behavior |
| --- | --- |
| The statue | Native human Elegy mesh; collision-checked apparition placement, gaze-aware relocation, stock dark smoke and warp sound, gentle appearance, floor glyphs. No native puzzle statue is spawned or stolen. |
| Encounters | 16 families with 4 patterns and 4 tempers: **256 directed event variants**. Patterns change trial rules, approach direction or encounter population; tempers change duration, movement goals, hold time and teleport cadence. |
| Rules | Look at him, deny him a witness, remain still, keep moving, enter his circle, circle him, leave him behind, or fight an added encounter. Some visits are only a voice. |
| Atmosphere | Gradually changing native fog/lighting, subdued teal/purple presentation, legible subtitles, bounded vignette and teleport effects. Reduced effects defaults ON. |
| Reflection | Temporary horizontal reflection of the current world, including Clock Town, using the port's native mirror rendering/input/map behavior. This is **not a new upside-down Clock Town mesh or collision map**. |
| Battles | Up to three additional named ordinary enemies from the NXT audited biome palette. They fight through the existing real bot/RPG combat system. Capacity/geometry failures become a walking trial. |
| Companions | Nearby regular bots notice the apparition; successful mercy can help both Link and nearby bots. Dedicated world-boss raid helpers are not drafted into BEN encounters. |
| Dialogue | **32,768 distinct composed transmissions**, built from 160 authored fragments matched to encounter families. These are combinations, not 32,768 individually written scenes. |
| Memory | Saved story counters, deck cursors, affinity, discoveries, sessions and active-play cooldown. The same family's text does not repeat within its 2,048-entry deck; its 16 variants cycle without repetition. |
| Choices | Offer kindness, defy him, listen, or offer mercy, once per encounter. Repeated kindness and mercy can produce a real temporary truce. Other paths uncover different discoveries. |
| Casebook | A searchable 32-entry **current-session** journal and a separate casebook window. Saved relationship/deck memory persists; the whole journal is not serialized. |

This is 256 combinations of the implemented encounter rules, **not 256 unrelated new mechanics**. It does not add new dungeons, story bosses, network multiplayer, or an unrestricted agent that modifies your computer.

## Controls and pacing

The **BEN DROWNED** sidebar and casebook show the current presence, story phase and available responses. Respond through the buttons when a meeting is recorded. The gameplay overlay remains passive and does not capture movement controls.

- **Intensity** changes event cadence and pressure. There are quiet intervals rather than constant interruptions; the first event normally waits about 18 active seconds.
- **Reduced Effects**, **Subtitles**, **Ambient Audio**, **Mirrored World Events** and **Combat Encounters** have separate switches.
- **Suspend Haunting** is immediate, including from a paused menu. It retires BEN's temporary encounter and clears his reflection. **Resume** leaves a grace period before the next event.
- Combat encounters also respect **Player Bots**, **Bots Fight Creatures**, **Extra Enemies in Combat Maps**, enemy caps and collider budgets. Enable those existing options for BEN battles; the rest of the haunting still works without them.
- Clock Town, houses and shops receive atmosphere and non-combat trials. The normal safe-map enemy policy stays in effect. Scripted boss arenas and the baseline's excluded timed/stealth/minigame scenes are left alone.
- Dialogue, ocarina use, cutscenes, transitions, pauses and death suspend the haunting. Low health suppresses threatening selections; trials have no direct failure damage or confiscation. Native enemies still deal their normal combat damage.

Use ordinary **owl/Song of Time saves** to retain BEN's memory. There is no per-event autosave. Closing without saving restores the previous saved state and can replay an unsaved encounter. Temporary statues, enemies, fog, reflection, and unfinished trial state are not persisted.

## First in-game acceptance pass

Use backed-up saves. Verify mode-OFF behavior first, then enable before file load. Start in Clock Town and check the subtitle/casebook, quiet non-combat events, reflection and statue appearance. Move to Termina Field for encounters with bots and RPG enabled. Test pause, dialogue, ocarina, scene/room transitions, dying, saving/reloading, and suspending during an active reflection or battle. Confirm normal Elegy puzzle statues still operate independently.

Test a low-resolution window and your usual fullscreen renderer. Check that the new fog remains readable, the statue uses your game's native assets, nameplates track reflection, and both player and bot hits stay correctly routed. These checks need your runnable game and assets; automated fixtures cannot establish their visual result.

Detailed architecture: `docs/BEN_DROWNED_TECHNICAL.md`.
Content and optional friendship spoilers: `docs/BEN_DROWNED_CONTENT.md`.
Executed checks and limitations: `docs/BEN_DROWNED_VALIDATION.md`.
