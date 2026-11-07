# BEN DROWNED technical reference

## Ownership

`BenCore.h` is the deterministic engine-independent director; `BenContent.h` supplies stable text/event labels. `BenDrowned.cpp` connects simulation, save state and the copied HUD. `BenWorld.cpp` owns one transient world event. `BenHud.cpp` owns presentation. New `.cpp` files enter the existing recursive CMake `2s2h` source list on reconfigure.

The mode is OFF by default. `GameInteractor_ExecuteOnSaveLoad` calls `Ben_OnSaveLoad`, which latches `gEnhancements.BenDrowned.Enabled`. Ordinary scene transitions do not relatch it. Slot and file-created identity checks prevent one file's journal or pending UI response from entering another file. Developer entrances have a first-update fallback.

`Actor_UpdateAll` calls `Ben_Update` after RPG and bots. Only eligible simulation ticks advance RNG, story clocks or trials. `Play_Draw` calls `Ben_RefreshHud` to publish and consume queued UI responses even when native pause skips actor updates. This runs on the game thread, not the ImGui renderer thread. Publication/request queues share a mutex; the UI receives fixed-size owned data only. Requests carry an encounter ID and scene-generation token. Old queued answers are discarded after a room/scene/file change.

`Play_Destroy` resets BEN **before** effect/actor arena teardown. Room transitions reset the current event. Actor destruction invalidates NXT's BEN enemy set using the existing destruction hook. No raw actor address is serialized or passed into ImGui.

## World and rendering

The human Elegy display list already belongs to `gameplay_keep`. BEN draws it directly with its own transform and interpolation identity. He does not instantiate `EnTorch2`, write `elegyShells`, register an extra player, create colliders, occupy switch actors, or modify respawn data.

Placement uses the NXT floor/step and line-of-sight checks and avoids nearby NPCs, doors and chests. Statue teleport attempts are bounded. Approach/orbit/gaze challenges anchor the statue; other movement obeys its gaze rule. Native smoke and warp audio supply the teleport presentation; the sound uses a stable owned position rather than a stack address.

Native atmosphere is a **same-frame draw transaction**. Begin saves the affected ambient/fog/directional-light values, applies bounded tints, and the scene/actor display lists consume them. The scene hook re-emits fog after begin because the initial fog commands occur earlier in `Play_DrawMain`. End restores values and emits the original fog before later effects/UI. Update state and native clock/quest variables are not changed. Reset/suspend handle an outstanding draw transaction.

Reflection uses a process-local atomic override. `Ben_MirrorState(base)` XORs the BEN state with the player's existing mirror choice. All native mirror readers—projection/culling, input, stereo audio, map cursors, shop controls, nameplates and enemy bars—use the same effective state. The original mirror CVar is never written, so saving settings during a reflection cannot make it permanent. The override clears on suspend, end, scene teardown and file load.

## Encounters and RPG

The NXT bridge `PlayerBots_BenSpawnEnemy` reuses the audited reinforcement spawn path and biome palette, including temporary object scope. It honors safe scenes, enabled/combat/reinforcement settings, warmup, native-plus-extra enemy caps, dungeon limits, actor capacity, collider budgets and active world-boss exclusion. Only BEN-owned extra enemies are retired. No native quest enemy is removed to make room.

Spawns are staggered and capped at three BEN actors. If attempts cannot place any enemy, the event becomes a non-combat movement trial. BEN enemies are registered in the normal reinforcement map, allowing existing bot combat adapters and RPG scaling/credit rules to apply. The ordinary director does not prune BEN-owned enemies in the middle of their trial. BEN cleanup never calls the RPG defeat/XP path itself.

Trials report success/failure once. Successful mercy/bargain/false-dawn challenges can restore half a native heart through `Health_ChangeBy`; nearby non-raid bots receive a bounded recovery. Failure only informs future director weights. The mode does not fake damage contacts on the player to hurt a bot.

## Save format

`ShipSaveInfo` appends pointer-free `BenSaveData`. The outer save schema rises **9 → 10** and BEN's independent section is schema 1. Migration 10 adds a missing `ben` object independently to owl/new-cycle snapshots, validates the parent object, and preserves all existing Zelda/RPG fields and any existing BEN section. The old executable will reject format 10, so rollback uses the original save backup.

`BenJson.h` accepts only valid integer types and bounded representations, validates fixed deck lengths, normalizes supported state, and rejects unknown versions. Its conversion commits only after the entire BEN section has decoded. BEN uses the normal save lifecycle; it adds no disk writer/autosave. No global configuration is used as a substitute for per-slot story memory.

## Configuration

All keys have prefix `gEnhancements.BenDrowned.`.

| Key | Default | Meaning |
| --- | --- | --- |
| Enabled | 0 | Applied on file load |
| Intensity | 1 | 0 restrained, 1 haunted, 2 relentless |
| ReducedEffects | 1 | Lower presentation intensity |
| Subtitles | 1 | Passive narrative overlay |
| AmbientAudio | 1 | Native apparition audio |
| Encounters | 1 | Allows added combat events, subject to NXT guards |
| Mirror | 1 | Allows transient reflected-world presentation |

Suspend/resume and dialogue choices are queued runtime requests, not persistent settings or direct renderer writes to the game world.

## Extension points

Keep new event decisions engine-independent and describe their actual gameplay behavior. A new family changes saved catalogue indexing; version it rather than reinterpreting old IDs. New world effects must own and restore their state and have a non-combat fallback when geometry or resources are unavailable. New ordinary enemy species require auditing the NXT combat/object requirements first. Do not spawn quest actors to imitate a boss without studying their native flags and lifecycle.

Use `tests/ben_drowned/run_tests.py` for production core/codec and explicit headless coordinator/world fixtures. The optional root `BUILD_BEN_TESTS` CMake switch provides core and real-JSON migration CTest targets for the Windows helper. Fixtures do not enter the release executable.
