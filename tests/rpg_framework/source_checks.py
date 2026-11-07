#!/usr/bin/env python3
"""Source invariants and actual CMake copy-helper execution on synthetic bytes.

This does not generate game archives or test their contents. It catches omissions
in source wiring, safe-map policy and the Windows beside-executable copy path.
"""
from pathlib import Path
import hashlib
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
checks = 0

def check(condition: bool, text: str) -> None:
    global checks
    checks += 1
    if not condition: raise AssertionError(text)

def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

check(hashlib.sha256((ROOT / "mm/2s2h/Enhancements/PlayerBots/BotScenePolicy.h").read_bytes().replace(b"\r\n", b"\n")).hexdigest() ==
      "4af22d40786e02b83ce4ab679ad64064eb8bcb041b0567a4c0d47896d0842a9e", "Original safe-map spawn policy changed")
actor = text("mm/src/code/z_actor.c")
for symbol in ["Rpg_Update(play)", "RpgWorldBoss_UpdateDirector(play)", "Rpg_BeginActorUpdate(play, actor)",
               "Rpg_EndActorUpdate(play, actor)", "Rpg_OnActorDestroyed(actor)", "Rpg_GetDrawColorFilter(actor, &drawFilterParams, &drawFilterTimer)",
               "Rpg_MovementMultiplier(actor)"]:
    check(symbol in actor, "Missing actor integration: " + symbol)
check("Rpg_RefreshHud(this)" in text("mm/src/code/z_play.c"), "Pause-safe HUD refresh missing")
check("Rpg_OnDamageContact(play, atCol, col, finalDamage" in text("mm/src/code/z_collision_check.c"), "Damage attribution missing")
check("RpgWorldBoss_RecordContact(play, atCol" in text("mm/src/code/z_collision_check.c"), "Multi-helper boss contact hook missing")
check("const uint32_t CURRENT_SAVE_VERSION = 10;" in text("mm/2s2h/SaveManager/SaveManager.cpp"), "Save migration version not raised")
check("{ 8, SaveManager_Migration_9 }" in text("mm/2s2h/SaveManager/SaveManager.cpp"), "Migration not registered")
check('"rpg", shipSaveInfo.rpg' in text("mm/2s2h/BenJsonConversions.hpp"), "RPG progression not serialized")
check('Rpg::DecodeProgress(j.at("rpg"))' in text("mm/2s2h/BenJsonConversions.hpp"), "RPG progression not loaded")
check("RpgSaveData rpg" in text("mm/include/z64save.h"), "Progression not part of per-save data")
check("ACTOR_EN_RPG_WORLD_BOSS" in text("mm/include/tables/actor_table.h"), "Custom actor missing")
check('sceneId != SCENE_00KEIKOKU' in text("mm/2s2h/Enhancements/RpgFramework/RpgWorldBoss.cpp"), "World boss is not Termina-only")
check('make_shared<RpgHud>' in text("mm/2s2h/BenGui/BenGui.cpp"), "HUD not registered")
check('"RPG Framework"' in text("mm/2s2h/BenGui/BenMenu.cpp"), "RPG settings missing")
check("Actor_SetColorFilter(" not in text("mm/2s2h/Enhancements/RpgFramework/RpgFramework.cpp"), "Cosmetic rage may change native stun state")
# Actual CMake script invocation; paths contain spaces to exercise quoting.
with tempfile.TemporaryDirectory(prefix="rpg copy fixture ") as temporary:
    base = Path(temporary); source = base / "source"; build = base / "build"; target = build / "Release"
    (source / "mm").mkdir(parents=True)
    port = b"synthetic port archive for copy-helper test, NOT an actual O2R"
    game = b"synthetic owned-game archive for copy-helper test, NOT game content"
    (source / "mm/2ship.o2r").write_bytes(port)
    cmd = ["cmake", "-DSYSTEM_NAME=Windows", "-DTARGET_DIR=" + str(target), "-DSOURCE_DIR=" + str(source),
           "-DBINARY_DIR=" + str(build)]
    script = str(ROOT / "CMake/copy-existing-otrs.cmake")
    result = subprocess.run([*cmd,"-DONLY2SHIPOTR=ON","-P",script], cwd=base, capture_output=True, text=True)
    check(result.returncode == 0, "Port archive copy failed: " + result.stdout + result.stderr)
    for path in [source/"2ship.o2r",build/"mm/2ship.o2r",target/"2ship.o2r"]:
        check(path.read_bytes() == port, "Copied port archive mismatch")
    check(not (target/"mm.o2r").exists(), "Port-only build unexpectedly needs a ROM")
    (source/"mm/mm.o2r").write_bytes(game)
    result = subprocess.run([*cmd,"-DONLY2SHIPOTR=OFF","-P",script], cwd=base, capture_output=True, text=True)
    check(result.returncode == 0, "Both archives could not be staged")
    check((target/"mm.o2r").read_bytes() == game, "Game archive copy mismatch")
    (source/"mm/2ship.o2r").unlink()
    result = subprocess.run([*cmd,"-DONLY2SHIPOTR=ON","-P",script], cwd=base, capture_output=True, text=True)
    check(result.returncode != 0, "Missing port archive must cause a hard error")
print(f"PASS {checks} source-wiring and CMake copy-helper assertions (synthetic archive bytes)")
